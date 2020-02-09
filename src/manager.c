/*****************************************************************************

 Copyright (c) 2008-2020  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, version 3 of the license.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 this program.  If not, see <http://www.gnu.org/licenses/>, or write to the
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 -----------------------------------------------------------------------------

 U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on
 behalf of the U.S. Government ("Government"), the following provisions apply
 to you.  If the Software is supplied by the Department of Defense ("DoD"), it
 is classified as "Commercial Computer Software" under paragraph 252.227-7014
 of the DoD Supplement to the Federal Acquisition Regulations ("DFARS") (or any
 successor regulations) and the Government is acquiring only the license rights
 granted herein (the license rights customarily provided to non-Government
 users).  If the Software is supplied to any unit or agency of the Government
 other than DoD, it is classified as "Restricted Computer Software" and the
 Government's rights in the Software are defined in paragraph 52.227-19 of the
 Federal Acquisition Regulations ("FAR") (or any successor regulations) or, in
 the cases of NASA, in paragraph 18.52.227-86 of the NASA Supplement to the FAR
 (or any successor regulations).

 -----------------------------------------------------------------------------

 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See http://www.openss7.com/

 *****************************************************************************/

#include "xde-xsession.h"
#include "manager.h"

#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

/*
 * There are two state machines: one for the overall session manager, another
 * for the state of a given SM client as viewed by the session manager.
 */

typedef enum {
	SMS_Start,
	SMS_ProtocolSetup,
	SMS_Register,
	SMS_AckRegister,
	SMS_Idle,
	SMS_SaveYourself,
	SMS_GetProperties,
	SMS_SavingGetProperties,
	SMS_SavingYourself,
	SMS_StartPhase2,
	SMS_Phase2,
	SMS_SaveYourselfDone,
	SMS_Die,
} SMS_State;

SMS_State managerState;

typedef enum {
	SMC_Start,
	SMC_Register,
	SMC_CollectId,
	SMC_ShutdownCancelled,
	SMC_Idle,
	SMC_Die,
	SMC_FreezeInteraction,
	SMC_SaveYourself,
	SMC_WaitingForPhase2,
	SMC_Phase2,
	SMC_InteractRequest,
	SMC_Interact,
	SMC_SaveYourselfDone,
	SMC_ConnectionClosed,
} SMC_State;

static Bool shutting_down;

typedef struct {
	char *id;
	char *hostname;
	SmsConn sms;
	IceConn ice;
	SmProp **props;
	Bool restarted;
	Bool checkpoint;
	Bool discard;
	Bool freeafter;
	char *discardCommand;
	char *saveDiscardCommand;
	int restartHint;
	SMC_State state;
} SmClient;

static SmcConn smc;

static GHashTable *initialClients; /* initializing clients */
static GHashTable *pendingClients; /* pending clients */
static GHashTable *anywaysClients;
static GHashTable *restartClients;
static GHashTable *failureClients;
static GHashTable *savselfClients;
static GHashTable *wphase2Clients;

static GSList *interacClients;
static GSList *runningClients;

void
freeClient(SmClient *c)
{
	/* FIXME !!!! */
}

/** @brief register client callback
  *
  * Before any further interaction takes place with the client, the client must be
  * registered with the session manager.
  *
  * If the client is being restarted from a previous state, #previousId will
  * contain a null-terminated string representing the client ID from the previous
  * session.  Call free() on this pointer when it is no longer needed.  If the
  * client is first joining the session, #previousId will be NULL.
  *
  * If #previousId is invalid, the session manager should not register the client
  * at this time.  The callback should return a status of zero, which will cause
  * an error message to be sent to the client.  The client should re-register with
  * #previousId set to NULL.
  *
  * Otherwise, the session manager should register the client with a unique client
  * ID by calling the SmsRegisterClientReply function, and the callback should
  * return a status of one.
  */
Status
registerClientCB(SmsConn smsConn, SmPointer data, char *previousId)
{
	SmClient *c = (typeof(c)) data;
	gpointer found;
	int send_save;

	(void) send_save;	/* FIXME */
	(void) found;		/* FIXME */

	if (!previousId) {
		free(c->id);
		c->id = SmsGenerateClientID(smsConn);
	} else
		do {
			SmClient *p;

			if ((p = (typeof(p)) g_hash_table_lookup(pendingClients, previousId))) {
				g_hash_table_remove(pendingClients, previousId);
				setInitialProperties(c, p->props);
				p->props = NULL;
				freeClient(p);
				break;
			}
			if ((p = (typeof(p)) g_hash_table_lookup(anywaysClients, previousId))) {
				g_hash_table_remove(anywaysClients, previousId);
				setInitialProperties(c, p->props);
				p->props = NULL;
				freeClient(p);
				break;
			}
			if ((p = (typeof(p)) g_hash_table_lookup(restartClients, previousId))) {
				g_hash_table_remove(restartClients, previousId);
				setInitialProperties(c, p->props);
				p->props = NULL;
				freeClient(p);
				break;
			}
			free(previousId);
			return (0);
		} while (0);
	c->state = SMC_Idle;
	SmsRegisterClientReply(smsConn, c->id);
	free(c->hostname);
	c->hostname = SmsClientHostName(smsConn);
	c->restarted = (previousId != NULL);
	if (!previousId) {
		c->state = SMC_SaveYourself;
		SmsSaveYourself(smsConn, SmSaveLocal, False, SmInteractStyleNone, False);
		g_hash_table_insert(initialClients, c->id, (gpointer) c);
	} else {
		/* TODO: update client GtkListStore */
	}
	return (1);
}

/** @brief interact request callback
  *
  * When a client receives a SaveYourself message with an interation style of
  * SmInteractStyleErrors or SmInteractStyleAny, the client may choose to interact
  * with the user.  Because only one client can interact with the user at a time,
  * the client must request to interact with the user.  The session manager should
  * keep a queue of all clients wishing to interact.  It should send a Interact
  * message to one client at a time and wait for an InteractDone message before
  * contining with the next client.
  *
  * The dialogType argument specifies either SmDialogError indicating that the
  * client wants to start an error dialog, or SmDialogNormal meaning that the
  * client wishes to start a nonerror dialog.
  *
  * If a shutdown is in progress, the use may have the option of cancelling the
  * shutdown.  If the shutdown is cancelled (specified in the InteractDone
  * message), the session manager should send a ShutdownCancelled message to each
  * client that requested to interact.
  */
void
interactRequestCB(SmsConn smsConn, SmPointer data, int dialogType)
{
	SmClient *c = (typeof(c)) data;
}

void
popupBadSave()
{
	/* FIXME */
}

void
finishUpSave()
{
	/* FIXME */
}

void
letClientInteract()
{
	/* FIXME */
}

void
startPhase2()
{
	/* FIXME */
}

Bool
okToEnterInteractPhase()
{
	/* FIXME */
	return False;
}

Bool
okToEnterPhase2()
{
	/* FIXME */
	return False;
}

/** @brief interact done callback
  *
  * When the client is done interacting with the user, the interact done callback
  * will be invoked.  Note that the shutdown can be cancelled only if the
  * corresponding SaveYourself specified True for shutdown and
  * SmInteractStyleErrors or SmInteractStyleAny for the interact style.
  */
void
interactDoneCB(SmsConn smsConn, SmPointer data, Bool cancelShutdown)
{
	int success = 0;		/* FIXME */
	int checkpoint_from_signal = 0;	/* FIXME */

	SmClient *c = (typeof(c)) data, *p;

	if ((p = g_hash_table_lookup(savselfClients, c->id))) {
		g_hash_table_remove(savselfClients, c->id);
		SmsSaveComplete(smsConn);
		return;
	}
	if (!success)
		g_hash_table_insert(failureClients, c->id, c);
	if (g_hash_table_size(savselfClients) == 0) {
		if (g_hash_table_size(failureClients) > 0 && !checkpoint_from_signal)
			popupBadSave();
		else
			finishUpSave();
	} else if (g_slist_length(interacClients) > 0 && okToEnterInteractPhase())
		letClientInteract();
	else if (g_hash_table_size(wphase2Clients) > 0 && okToEnterPhase2())
		startPhase2();
}

/** @brief save yourself request callback
  *
  * The save yourself request prompts the session manager to initiate a checkpoint
  * or shutdown.  If #global is set to False then the SaveYourself should only be
  * sent to the client that requested it.
  */
void
saveYourselfReqCB(SmsConn smsConn, SmPointer data, int type, Bool shutdown,
		  int style, Bool fast, Bool global)
{
	SmClient *c = (typeof(c)) data;
}

/** @brief save yourself phase 2 request callback
  *
  * This request is sent by clients that manage other clients (such as the window
  * manager, workspace managers, and so on).  Such managers must make sure that all
  * of the clients that are being managed are in an idle state so that their state
  * can be saved.
  */
void
saveYourselfP2ReqCB(SmsConn smsConn, SmPointer data)
{
	SmClient *c = (typeof(c)) data;
}

/** @brief save yourself done callback
  *
  * When the client is done saving its state in response to a SaveYourself message,
  * this callback will be invoked.  Before the SaveYourselfDone was sent, the
  * client must have set each required property at least once since it registered
  * with the session manager.
  */
void
saveYourselfDoneCB(SmsConn smsConn, SmPointer data, Bool success)
{
	SmClient *c = (typeof(c)) data;
}

/** @brief close connection callback
  *
  * If the client properly terminates (that is, it calls SmcCloseConnection) this
  * callback is invoked.  The #reason argument will most likely be NULL and the
  * #count argument zero (0) if resignation is expected by the user.  Otherwise, it
  * contains a list of null-terminated compound text strings representing the
  * reason for termination.  The session manager should display these reason
  * messages to the user.  Call SmsFreeReasons to free the #reason messages.
  */
void
closeConnectionCB(SmsConn smsConn, SmPointer data, int count, char **reason)
{
	SmClient *c = (typeof(c)) data;
}

/** @brief set properties callback
  *
  * When the client sets session management properties, this callback will be
  * invoked.  The properties are specified as an array of property pointers.
  * Previously set property values may be overwritten.  Some properties have
  * predefined semantics.  The session manager is required to store nonpredefined
  * properties.  To free each property, use SmFreeProperty.  You should free the
  * actual array of pointers with free().
  */
void
setPropertiesCB(SmsConn smsConn, SmPointer data, int num, SmProp * props[])
{
	SmClient *c = (typeof(c)) data;
}

/** @brief delete properties callback
  *
  * When the client deletes session management properties, this callback is
  * invoked.  The properties are specified as an array of strings.
  */
void
deletePropertiesCB(SmsConn smsConn, SmPointer data, int num, char *names[])
{
	SmClient *c = (typeof(c)) data;
}

/** @brief get properties callback
  *
  * This callback is invoked when the client wants to retrieve properties it set.
  * The session manager should respond by calling SmsReturnProperties.  All of the
  * properties set for this client should be returned.
  */
void
getPropertiesCB(SmsConn smsConn, SmPointer data)
{
	SmClient *c = (typeof(c)) data;
}

static Status
newClientCB(SmsConn smsConn, SmPointer data, unsigned long *mask, SmsCallbacks *cb, char **reason)
{
	SmClient *c;

	if (!(c = calloc(1, sizeof(*c)))) {
		fprintf(stderr, "Memory allocation failed\n");
		return (0);
	}
	*mask = 0;

	c->sms = smsConn;
	c->ice = SmsGetIceConnection(smsConn);
	c->id = NULL;
	c->hostname = NULL;
	c->restarted = False;
	c->checkpoint = False;
	c->discard = False;
	c->freeafter = False;
	c->props = NULL;
	c->discardCommand = NULL;
	c->saveDiscardCommand = NULL;
	c->restartHint = SmRestartIfRunning;
	c->state = SMC_Start;

	runningClients = g_slist_append(runningClients, c);

	*mask |= SmsRegisterClientProcMask;
	cb->register_client.callback = registerClientCB;
	cb->register_client.manager_data = (SmPointer) c;

	*mask |= SmsInteractRequestProcMask;
	cb->interact_request.callback = interactRequestCB;
	cb->interact_request.manager_data = (SmPointer) c;

	*mask |= SmsInteractDoneProcMask;
	cb->interact_done.callback = interactDoneCB;
	cb->interact_done.manager_data = (SmPointer) c;

	*mask |= SmsSaveYourselfRequestProcMask;
	cb->save_yourself_request.callback = saveYourselfReqCB;
	cb->save_yourself_request.manager_data = (SmPointer) c;

	*mask |= SmsSaveYourselfP2RequestProcMask;
	cb->save_yourself_phase2_request.callback = saveYourselfP2ReqCB;
	cb->save_yourself_phase2_request.manager_data = (SmPointer) c;

	*mask |= SmsSaveYourselfDoneProcMask;
	cb->save_yourself_done.callback = saveYourselfDoneCB;
	cb->save_yourself_done.manager_data = (SmPointer) c;

	*mask |= SmsCloseConnectionProcMask;
	cb->close_connection.callback = closeConnectionCB;
	cb->close_connection.manager_data = (SmPointer) c;

	*mask |= SmsSetPropertiesProcMask;
	cb->set_properties.callback = setPropertiesCB;
	cb->set_properties.manager_data = (SmPointer) c;

	*mask |= SmsDeletePropertiesProcMask;
	cb->delete_properties.callback = deletePropertiesCB;
	cb->delete_properties.manager_data = (SmPointer) c;

	*mask |= SmsGetPropertiesProcMask;
	cb->get_properties.callback = getPropertiesCB;
	cb->get_properties.manager_data = (SmPointer) c;

	return (1);
}

gboolean
on_lfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	IceConn ice;
	IceAcceptStatus status;
	IceConnectStatus cstatus;
	IceListenObj obj = (typeof(obj)) data;

	/* IceIOErrorHandler should handle this ... */
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;
	}
	if (shutting_down)
		return G_SOURCE_REMOVE;

	if (!(ice = IceAcceptConnection(obj, &status))) {
		if (options.debug)
			EPRINTF("IceAcceptConnection: failed\n");
		return G_SOURCE_REMOVE;
	}
	while ((cstatus = IceConnectionStatus(ice)) == IceConnectPending)
		relax();
	if (cstatus == IceConnectAccepted) {
		if (options.debug) {
			int ifd = IceConnectionNumber(ice);
			char *connstr = IceConnectionString(ice);

			DPRINTF
			    ("ICE connection opened by client, fd = %d, accepted at networkId %s\n",
			     ifd, connstr);
			free(connstr);
		}
	} else {
		if (cstatus == IceConnectIOError)
			EPRINTF("IO error during ICE connection\n");
		else
			EPRINTF("ICE connection rejected\n");
		IceCloseConnection(ice);
	}
	return G_SOURCE_CONTINUE;	/* keep event source */
}

Bool
hostBasedAuthCB(char *hostname)
{
	return (False);		/* refuse host based authentication */
}

int numTransports;
IceListenObj *listenObjs;

void
CloseListeners(void)
{
	IceFreeListenObjs(numTransports, listenObjs);
}

static void
write_iceauth(FILE *addfp, FILE *removefp, IceAuthDataEntry *entry)
{
	int i;

	fprintf(addfp, "add %s \"\" %s %s ",
		entry->protocol_name, entry->network_id, entry->auth_name);
	for (i = 0; i < entry->auth_data_length; i++)
		fprintf(addfp, "%02x", (char) entry->auth_data[i]);
	fprintf(addfp, "\n");
	fprintf(removefp, "remove protoname=%s protodata=\"\" netid=%s authname=%s\n",
		entry->protocol_name, entry->network_id, entry->auth_name);
}

static char *
unique_filename(const char *path, const char *prefix, int *pFd)
{
	char tempFile[PATH_MAX];
	char *ptr;

	snprintf(tempFile, sizeof(tempFile), "%s/%sXXXXXX", path, prefix);
	if ((ptr = strdup(tempFile)))
		*pFd = mkstemp(ptr);
	return (ptr);
}

Bool
HostBasedAuthProc(char *hostname)
{
	return False;
}

static char *addAuthFile;
static char *remAuthFile;

#define MAGIC_COOKIE_LEN 16

/** @brief set authentication
  *
  * NOTE: this is a strange way to do this: I don't know why we do not simply
  * use the iceauth library and write the file directly with correct locking.
  */
Status
SetAuthentication(int count, IceListenObj * listenObjs, IceAuthDataEntry **authDataEntries)
{
	FILE *addfp = NULL;
	FILE *removefp = NULL;
	const char *path;
	mode_t original_umask;
	char command[256] = { 0, };
	int i, j, status;
	int fd;

	original_umask = umask(0077);
	path = getenv("SM_SAVE_DIR") ? : (getenv("HOME") ? : ".");
	if ((addAuthFile = unique_filename(path, ".xsm", &fd)) == NULL)
		goto bad;
	if (!(addfp = fdopen(fd, "wb")))
		goto bad;
	if ((remAuthFile = unique_filename(path, ".xsm", &fd)) == NULL)
		goto bad;
	if (!(removefp = fdopen(fd, "wb")))
		goto bad;
	if ((*authDataEntries = calloc(count * 2, sizeof(IceAuthDataEntry))) == NULL)
		goto bad;
	for (i = 0, j = 0; j < count; i += 2, j++) {
		(*authDataEntries)[i].network_id = IceGetListenConnectionString(listenObjs[j]);
		(*authDataEntries)[i].protocol_name = "ICE";
		(*authDataEntries)[i].auth_name = "MIT-MAGIC-COOKIE-1";
		(*authDataEntries)[i].auth_data = IceGenerateMagicCookie(MAGIC_COOKIE_LEN);
		(*authDataEntries)[i].auth_data_length = MAGIC_COOKIE_LEN;

		(*authDataEntries)[i].network_id = IceGetListenConnectionString(listenObjs[j]);
		(*authDataEntries)[i].protocol_name = "XSMP";
		(*authDataEntries)[i].auth_name = "MIT-MAGIC-COOKIE-1";
		(*authDataEntries)[i].auth_data = IceGenerateMagicCookie(MAGIC_COOKIE_LEN);
		(*authDataEntries)[i].auth_data_length = MAGIC_COOKIE_LEN;

		write_iceauth(addfp, removefp, &(*authDataEntries)[i]);
		write_iceauth(addfp, removefp, &(*authDataEntries)[i + 1]);

		IceSetPaAuthData(2, &(*authDataEntries)[i]);
		IceSetHostBasedAuthProc(listenObjs[j], HostBasedAuthProc);
	}
	fclose(addfp);
	fclose(removefp);
	umask(original_umask);
	snprintf(command, sizeof(command), "iceauth source %s", addAuthFile);
	status = system(command);
	(void) status;
	remove(addAuthFile);
	return (1);
      bad:
	if (addfp)
		fclose(addfp);
	if (removefp)
		fclose(removefp);
	if (addAuthFile) {
		remove(addAuthFile);
		free(addAuthFile);
	}
	if (remAuthFile) {
		remove(remAuthFile);
		free(remAuthFile);
	}
	return (0);
}

static IceIOErrorHandler prev_handler;

static void
ManagerIOErrorHandler(IceConn ice)
{
	if (prev_handler)
		(*prev_handler) (ice);
}

static void
InstallIOErrorHandler(void)
{
	IceIOErrorHandler default_handler;

	prev_handler = IceSetIOErrorHandler(NULL);
	default_handler = IceSetErrorHander(ManagerIOErrorHandler);
	if (prev_handler == default_handler)
		prev_handler = NULL;
}

IceAuthDataEntry *authDataEntries;
char *networkIds;

void
smpInitSessionManager(void)
{
	char err[256] = { 0, };
	int i;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	InstallErrorHandler();

	if (!SmsInitialize(NAME, VERSION, newClientCB, NULL, hostBasedAuthCB, sizeof(err), err)) {
		EPRINTF("SmsInitialize: %s\n", err);
		exit(EXIT_FAILURE);
	}
	if (!IceListenForConnections(&numTransports, *listenObjs, sizeof(err), err)) {
		EPRINTF("IceListenForConnections: %s\n", err);
		exit(EXIT_FAILURE);
	}
	atexit(CloseListeners);
	if (!SetAuthentication(numTransports, listenObjs, &authDataEntries)) {
		EPRINTF("SetAuthentication: could not set authorization\n");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < numTransports; i++) {
		int lfd = IceGetListenConnectionNumber(listenObjs[i]);
		GIOChannel *chan = g_io_channel_unix_new(lfd);
		gint srce = g_io_add_watch(chan, mask, on_lfd_watch, (gpointer) listenObjs[i]);

		(void) srce;
	}
	networkIds = IceComposeNetworkIdList(numTransports, listenObjs);
	setenv("SESSION_MANAGER", networkIds, TRUE);

	managerState = SMS_Start;
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
