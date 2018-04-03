/*****************************************************************************

 Copyright (c) 2010-2018  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2002-2009  OpenSS7 Corporation <http://www.openss7.com/>
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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <fcntl.h>
#ifdef _GNU_SOURCE
#include <getopt.h>
#endif
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#endif
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <gtk/gtk.h>
#include <cairo.h>

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stdout, "I: "); \
	fprintf(stdout, args); \
	fflush(stdout); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)
#define PTRACE() do { if (options.debug > 0 || options.output > 2) { \
	fprintf(stderr, "T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#undef EXIT_SYNTAXERR

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define EXIT_SYNTAXERR	2

enum {
	OBEYSESS_DISPLAY, /* obey multipleSessions resource */
	REMANAGE_DISPLAY, /* force remanage */
	UNMANAGE_DISPLAY, /* force deletion */
	RESERVER_DISPLAY, /* force server termination */
	OPENFAILED_DISPLAY, /* XOpenDisplay failed, retry */
};

Display *dpy;
int screen;
Window root;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *clientId;
	char *saveFile;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.clientId = NULL,
	.saveFile = NULL,
};

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
wmpSaveYourselfPhase2CB(SmcConn smcConn, SmPointer clientData)
{
	/* for each client in stacking order (use _NET_CLIENT_LIST_STACKING),
	   or use XQueryTree with _NET_CLIENT_LIST/_WIN_CLIENT_LIST, or just
	   use XQueryTree with windows that have a WM_STATE property set. */
	/* find the WM_CLIENT_LEADER window, otherwise skip that client */
	/* get the SM_CLIENT_ID property from that window */
	/* get the WM_WINDOW_ROLE for the original window */
	/* write "r %s %s ", SM_CLIENT_ID, WM_WINDOW_ROLE */
	/* otherwise use resource class and instance */
	/* write "c %s %s %s ", cid, WM_CLASS.res_class, WM_CLASS.res_instance */
	/* write "%d:%d:%d:%d %ld %lu %ld\n", x, y, width, height, workspace,
	   state, active layer */
	/* at the end write "w %lu\n", active workspace */

	/* additional information we should print about windows */
	/* _WIN_EXPANDED_SIZE */
	/* _WIN_HINTS */
	/* _WIN_LAYER */
	/* _WIN_MAXIMIZED_GEOMETRY */
	/* _WIN_STATE */
	/* _WIN_WORSPACE */
	/* _NET_FRAME_EXTENTS */
	/* _NET_WM_DESKTOP */
	/* _NET_WM_FULLSCREEN_MONITORS */
	/* _NET_WM_VISIBLE_NAME */
	/* _NET_WM_VISIBLE_ICON_NAME */
	/* _NET_WM_STATE */
	/* _NET_WM_STRUT */
	/* _NET_WM_STRUT_PARTIAL */

	/* additional information we should print about the window manager */
	/* _WIN_AREA_COUNT */
	/* _WIN_AREA */
	/* _WIN_FOCUS - the window that had the focus (if session managed) */
	/* _WIN_WORKAREA */
	/* _WIN_WORKSPACE_COUNT */
	/* _WIN_WORKSPACE_NAMES */
	/* _WIN_WORSPACE */
	/* _WIN_WORKSPACES */
	/* _NET_ACTIVE_WINDOW */
	/* _NET_CLIENT_LIST */
	/* _NET_CLIENT_LIST_STACKING */
	/* _NET_CURRENT_DESKTOP */
	/* _NET_DESKTOP_GEOMETRY */
	/* _NET_DESKTOP_LAYOUT */
	/* _NET_DESKTOP_NAMES */
	/* _NET_DESKTOP_VIEWPORT */
	/* _NET_NUMBER_OF_DESKTOPS */
	/* _NET_SHOWING_DESKTOP */
	/* _NET_WORKAREA */

	/* Additionally, optionally we might save: */

	/* Old session management apps (like smproxy) */
	/* WindowMaker Dock Apps */
	/* System tray icons */

	SmcSaveYourselfDone(smcConn, True);
}

int sent_save_done;

/** @brief save yourself callback
  *
  * The session manager sends a SaveYourself message to the client either to
  * checkpoint it or so that ic an save its state.  The client response with zero
  * or more calls to SmcSetProperties to update the properties indicating how to
  * restart the client.  When all the properties have been set, the client call
  * SmcSaveYourselfDone.
  *
  * If #interactStyle is SmInteractStyleNone, the client must not interact with the
  * user while saving state.  If #interactStyle is SmInteractStyleErrors, the
  * client may interact with the user only if an error condition arises.  If
  * interactStyle is SmInteractStyleAny, then the client may interact with the user
  * at a time, the client must acll SmcInteractRequest and wait for an Interact
  * message from the session manager.  When the client is done interacting with the
  * user, it calls SmcInteractDone.  The client may only call SmcInteractRequest
  * after it receives a SaveYourself message and before it call
  * SmcSaveYourselfDone.
  *
  * If saveType is SmSaveLocal, the client must update the properties to reflect
  * its current state.  Specifically , it should save enough information to
  * restore the state as seen by the user of this client.  It should not affect the
  * state as seen by other users.  If saveType is SmSaveGlobal, the user wants the
  * client to commit all of its data to permanent, globally accessible storage.  If
  * saveType is SmSaveBoth, the client should do both of these (it should first
  * commit the data to permanent storage before updating its properties).
  *
  * The #shutdown argument specifies whether the system is being shut down.  The
  * interaction is different depending on whether or not shutdown is set.  If not
  * shutting down, the client should save its state and wait for a SaveComplete
  * message.  If shutting down, the client must save state and then prevent
  * interaction until it receives either a Die or a ShutdownCancelled.
  *
  * The #fast argument specifies that the client should save its state as quickly
  * as possible.  For example, if the session manager knows that popwer is about to
  * fail, it would set #fast to True.
  */
void
wmpSaveYourselfCB(SmcConn smcConn, SmPointer clientData, int saveType, Bool shutdown,
		  int interactStyle, Bool fast)
{
	if (!SmcRequestSaveYourselfPhase2(smcConn, wmpSaveYourselfPhase2CB, NULL)) {
		SmcSaveYourselfDone(smcConn, False);
		sent_save_done = 1;
	} else
		sent_save_done = 0;
}

/** @brief die callback
  *
  * The session manager sends a Die message to a client when it wants it to die.
  * The client should respond by calling SmcCloseConnection.  A session manager
  * that behaves properly will send a SaveYourself message before a Die message.
  */
void
wmpDieCB(SmcConn smcConn, SmPointer clientData)
{
	SmcCloseConnection(smcConn, 0, NULL);
	exit(EXIT_SUCCESS);
}

void
wmpSaveCompleteCB(SmcConn smcConn, SmPointer clientData)
{
	/* doesn't do anything */
}

/** @brief shutdown cancelled callback
  *
  * The session manager sends a ShutdownCancelled message when the user cancelled
  * the shutdown during an interaction.  The client can now continue as if the
  * shutdown had never happended.  If the client has not call SmcSaveYourselfDone
  * yet, it can either abort the save and then call SmcSaveYourselfDone with the
  * success argument set to False, or it can continue with the save and then call
  * SmcSaveYourselfDone with the success argument set to reflect the outcome of the
  * save.
  */
void
wmpShutdownCancelledCB(SmcConn smcConn, SmPointer clientData)
{
	if (sent_save_done)
		return;
	SmcSaveYourselfDone(smcConn, False);
	sent_save_done = 1;
}

SmcConn
wmpConnectToSessionManager()
{
	char err[256] = { 0, };
	unsigned long mask;
	SmcCallbacks cb;
	SmcConn smcConn = NULL;

	if (getenv("SESSION_MANAGER")) {
		mask =
		    SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask |
		    SmcShutdownCancelledProcMask;

		cb.save_yourself.callback = wmpSaveYourselfCB;
		cb.save_yourself.client_data = (SmPointer) NULL;

		cb.die.callback = wmpDieCB;
		cb.die.client_data = (SmPointer) NULL;

		cb.save_complete.callback = wmpSaveCompleteCB;
		cb.save_complete.client_data = (SmPointer) NULL;

		cb.shutdown_cancelled.callback = wmpShutdownCancelledCB;
		cb.shutdown_cancelled.callback = (SmPointer) NULL;

		if ((smcConn = SmcOpenConnection(NULL,	/* use SESSION_MANAGER
							   env */
						 NULL,	/* context */
						 SmProtoMajor, SmProtoMinor, mask, &cb,
						 options.clientId, &options.clientId, sizeof(err),
						 err)) == NULL) {
			fprintf(stderr, "SmcOpenConnection: %s\n", err);
			exit(EXIT_FAILURE);
		}
	} else if (options.clientId)
		fprintf(stderr, "clientId specified by no SESSION_MANAGER\n");
	return (smcConn);
}

void
setSMProperties(int argc, char *argv[])
{
	char procID[20], userID[20];
	CARD8 hint;
	int i;

	SmPropValue propval[11];

	SmProp prop[11] = {
		[0] = {SmCloneCommand, SmLISTofARRAY8, 1, &propval[0]},
		[1] = {SmCurrentDirectory, SmARRAY8, 1, &propval[1]},
		[2] = {SmDiscardCommand, SmLISTofARRAY8, 1, &propval[2]},
		[3] = {SmEnvironment, SmLISTofARRAY8, 1, &propval[3]},
		[4] = {SmProcessID, SmARRAY8, 1, &propval[4]},
		[5] = {SmProgram, SmARRAY8, 1, &propval[5]},
		[6] = {SmRestartCommand, SmLISTofARRAY8, 1, &propval[6]},
		[7] = {SmResignCommand, SmLISTofARRAY8, 1, &propval[7]},
		[8] = {SmRestartStyleHint, SmARRAY8, 1, &propval[8]},
		[9] = {SmShutdownCommand, SmLISTofARRAY8, 1, &propval[9]},
		[10] = {SmUserID, SmARRAY8, 1, &propval[10]},
	};
	SmProp *props[11] = {
		[0] = &prop[0],
		[1] = &prop[1],
		[2] = &prop[2],
		[3] = &prop[3],
		[4] = &prop[4],
		[5] = &prop[5],
		[6] = &prop[6],
		[7] = &prop[7],
		[8] = &prop[9],
		[9] = &prop[9],
		[10] = &prop[10],
	};
	(void) props;

	prop[0].vals = calloc(argc, sizeof(SmPropValue));
	prop[0].num_vals = 0;
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[0].vals[prop[0].num_vals].value = (SmPointer) argv[i];
			prop[0].vals[prop[0].num_vals++].length = strlen(argv[i]);
		}
	}

	snprintf(procID, sizeof(procID), "%ld", (long) getpid());
	propval[4].value = procID;
	propval[4].length = strlen(procID);

	propval[5].value = argv[0];
	propval[5].length = strlen(argv[0]);

	prop[6].vals = calloc(argc + 4, sizeof(SmPropValue));
	prop[6].num_vals = 0;
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[6].vals[prop[6].num_vals].value = (SmPointer) argv[i];
			prop[6].vals[prop[6].num_vals++].length = strlen(argv[i]);
		}
	}
	prop[6].vals[prop[6].num_vals].value = (SmPointer) "-clientId";
	prop[6].vals[prop[6].num_vals++].length = 9;
	prop[6].vals[prop[6].num_vals].value = (SmPointer) options.clientId;
	prop[6].vals[prop[6].num_vals++].length = strlen(options.clientId);

	prop[6].vals[prop[6].num_vals].value = (SmPointer) "-restore";
	prop[6].vals[prop[6].num_vals++].length = 9;
	prop[6].vals[prop[6].num_vals].value = (SmPointer) options.saveFile;
	prop[6].vals[prop[6].num_vals++].length = strlen(options.saveFile);

	hint = SmRestartIfRunning;
	propval[8].value = &hint;
	propval[8].length = 1;

	snprintf(userID, sizeof(userID), "%ld", (long) getuid());
	propval[10].value = userID;
	propval[10].length = strlen(userID);

}

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

void
setInitialProperties(SmClient *c, SmProp *props[])
{
	/* FIXME !!!! */
}

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

	(void) send_save; /* FIXME */
	(void) found; /* FIXME */

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
	int success = 0; /* FIXME */
	int checkpoint_from_signal = 0; /* FIXME */

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
	return G_SOURCE_CONTINUE;		/* keep event source */
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
	default_handler = IceSetIOErrorHandler(ManagerIOErrorHandler);
	if (prev_handler == default_handler)
		prev_handler = NULL;
}

IceAuthDataEntry *authDataEntries;
char *networkIds;

void
SmpInitSessionManager(void)
{
	char err[256] = { 0, };
	int i;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	InstallIOErrorHandler();

	if (!SmsInitialize(NAME, VERSION, newClientCB, NULL, hostBasedAuthCB, sizeof(err), err)) {
		EPRINTF("SmsInitialize: %s\n", err);
		exit(EXIT_FAILURE);
	}
	if (!IceListenForConnections(&numTransports, &listenObjs, sizeof(err), err)) {
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

void
handle_events(void)
{
	/* FIXME */
}

gboolean
on_xfd_watch(GIOChannel * chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		fprintf(stderr, "ERROR: poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		handle_events();
	}
	return TRUE;		/* keep event source */
}

gboolean
on_ifd_watch(GIOChannel * chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		fprintf(stderr, "ERROR: poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceConn ice = (typeof(ice)) data;

		IceProcessMessages(ice, NULL, NULL);
	}
	return TRUE;		/* keep event source */
}

void
on_int_signal(int signum)
{
	/* FIXME */
}

void
on_hup_signal(int signum)
{
	/* FIXME */
}

void
on_term_signal(int signum)
{
	/* FIXME */
}

void
on_quit_signal(int signum)
{
	/* FIXME */
}

void
on_usr1_signal(int signum)
{
	/* FIXME */
}

void
on_usr2_signal(int signum)
{
	/* FIXME */
}

void
on_alrm_signal(int signum)
{
	/* FIXME */
}

int
handler(Display *dpy, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, "xdg-launch", num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%lx): %s\n", req, xev->resourceid, msg);
	}
	return(0);
}

void
startup(int argc, char *argv[])
{
	static const char *suffix = "/.gtkrc-2.0.xde";
	int xfd;
	GIOChannel *chan;
	gint srce;
	const char *home;
	char *file;
	int len;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	initialClients = g_hash_table_new(g_str_hash, g_str_equal);
	pendingClients = g_hash_table_new(g_str_hash, g_str_equal);
	anywaysClients = g_hash_table_new(g_str_hash, g_str_equal);
	restartClients = g_hash_table_new(g_str_hash, g_str_equal);

	home = getenv("HOME") ? : ".";
	len = strlen(home) + strlen(suffix) + 1;
	file = calloc(len, sizeof(*file));
	strncpy(file, home, len);
	strncat(file, suffix, len);
	gtk_rc_add_default_file(file);
	free(file);

	gtk_init(&argc, &argv);

	signal(SIGINT, on_int_signal);
	signal(SIGHUP, on_hup_signal);
	signal(SIGTERM, on_term_signal);
	signal(SIGQUIT, on_quit_signal);
	signal(SIGUSR1, on_usr1_signal);
	signal(SIGUSR2, on_usr2_signal);
	signal(SIGALRM, on_alrm_signal);

	if (!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "%s: %s %s\n", argv[0], "cannot open display",
			getenv("DISPLAY") ? : "");
		exit(EXIT_FAILURE);
	}
	xfd = ConnectionNumber(dpy);
	chan = g_io_channel_unix_new(xfd);
	srce = g_io_add_watch(chan, mask, on_xfd_watch, NULL);
	(void) srce;

	XSetErrorHandler(handler);
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	SmpInitSessionManager();

	if ((smc = wmpConnectToSessionManager())) {
		IceConn ice;
		int ifd;

		ice = SmcGetIceConnection(smc);
		ifd = IceConnectionNumber(ice);
		chan = g_io_channel_unix_new(ifd);
		srce = g_io_add_watch(chan, mask, on_ifd_watch, (gpointer) ice);
	}

}

static void
copying(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2010-2018  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2002-2009  OpenSS7 Corporation <http://www.openss7.com/>\n\
Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>\n\
\n\
All Rights Reserved.\n\
--------------------------------------------------------------------------------\n\
This program is free software: you can  redistribute it  and/or modify  it under\n\
the terms of the  GNU Affero  General  Public  License  as published by the Free\n\
Software Foundation, version 3 of the license.\n\
\n\
This program is distributed in the hope that it will  be useful, but WITHOUT ANY\n\
WARRANTY; without even  the implied warranty of MERCHANTABILITY or FITNESS FOR A\n\
PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.\n\
\n\
You should have received a copy of the  GNU Affero General Public License  along\n\
with this program.   If not, see <http://www.gnu.org/licenses/>, or write to the\n\
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\
--------------------------------------------------------------------------------\n\
U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on behalf\n\
of the U.S. Government (\"Government\"), the following provisions apply to you. If\n\
the Software is supplied by the Department of Defense (\"DoD\"), it is classified\n\
as \"Commercial  Computer  Software\"  under  paragraph  252.227-7014  of the  DoD\n\
Supplement  to the  Federal Acquisition Regulations  (\"DFARS\") (or any successor\n\
regulations) and the  Government  is acquiring  only the  license rights granted\n\
herein (the license rights customarily provided to non-Government users). If the\n\
Software is supplied to any unit or agency of the Government  other than DoD, it\n\
is  classified as  \"Restricted Computer Software\" and the Government's rights in\n\
the Software  are defined  in  paragraph 52.227-19  of the  Federal  Acquisition\n\
Regulations (\"FAR\")  (or any successor regulations) or, in the cases of NASA, in\n\
paragraph  18.52.227-86 of  the  NASA  Supplement  to the FAR (or any  successor\n\
regulations).\n\
--------------------------------------------------------------------------------\n\
", NAME " " VERSION);
}

static void
version(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018  Monavacon Limited.\n\
Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009  OpenSS7 Corporation.\n\
Copyright (c) 1997, 1998, 1999, 2000, 2001  Brian F. G. Bidulock.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Distributed by OpenSS7 under GNU Affero General Public License Version 3,\n\
with conditions, incorporated herein by reference.\n\
\n\
See `%1$s --copying' for copying permissions.\n\
", NAME, PACKAGE, VERSION);
}

static void
usage(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [options] COMMAND ARG ...\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	/* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] COMMAND ARG ...\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    COMMAND ARG ...\n\
        command and arguments to launch window manager\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
Options:\n\
    -c, --clientId CLIENTID\n\
        session management client id [default: %4$s]\n\
    -r, --restore SAVEFILE\n\
        session management state file [default: %5$s]\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: %2$d]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: %3$d]\n\
        this option may be repeated.\n\
", argv[0]
	, options.debug
	, options.output
	, options.clientId
	, options.saveFile
);
	/* *INDENT-ON* */
}

void
set_defaults()
{
}

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"clientId",	required_argument,  NULL,   'c'},
			{"restore",	required_argument,  NULL,   'r'},

			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "c:r:D::v::hVCH?", long_options, &option_index);
#else
		c = getopt(argc, argv, "c:r:DvhVCH?");
#endif
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'c':	/* -clientId, -c CLIENTID */
			if (options.clientId)
				goto bad_option;
			options.clientId = strdup(optarg);
			break;
		case 'r':	/* -restore, -r SAVEFILE */
			if (options.saveFile)
				goto bad_option;
			options.saveFile = strdup(optarg);
			break;
		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.output++;
				break;
			}
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(OBEYSESS_DISPLAY);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n", argv[0]);
			version(argc, argv);
			exit(OBEYSESS_DISPLAY);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n", argv[0]);
			copying(argc, argv);
			exit(OBEYSESS_DISPLAY);
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '", argv[0]);
					while (optind < argc) {
						fprintf(stderr, "%s", argv[optind++]);
						fprintf(stderr, "%s", (optind < argc) ? " " : "");
					}
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(REMANAGE_DISPLAY);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind >= argc) {
		fprintf(stderr, "%s: missing non-option argument\n", argv[0]);
		usage(argc, argv);
		exit(REMANAGE_DISPLAY);
	}
	/* FIXME */
	exit(0);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
