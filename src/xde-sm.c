/*****************************************************************************

 Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>
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

/** @section Headers
  * @{ */

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
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <langinfo.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>
#include <math.h>
#include <dlfcn.h>
#include <setjmp.h>

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
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>
#include <gtk/gtk.h>
#include <cairo.h>



#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

/** @} */

/** @section Preamble
  * @{ */

static const char *
_timestamp(void)
{
	static struct timeval tv = { 0, 0 };
	static char buf[BUFSIZ];
	double stamp;

	gettimeofday(&tv, NULL);
	stamp = (double)tv.tv_sec + (double)((double)tv.tv_usec/1000000.0);
	snprintf(buf, BUFSIZ-1, "%f", stamp);
	return buf;
}

#define XPRINTF(_args...) do { } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output > _num) { \
	fprintf(stdout, NAME ": I: "); \
	fprintf(stdout, _args); fflush(stdout); } } while (0)

#define CPRINTF(_num, _c, _args...) do { if (options.output > 1) { \
	fprintf(stdout, NAME ": I: Client Id = %s, ", _c->id); \
	fprintf(stdout, _args); fflush(stdout); } } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
	fprintf(stderr, NAME ": D: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
	fprintf(stderr, NAME ": E: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr);   } while (0)

#define WPRINTF(_args...) do { \
	fprintf(stderr, NAME ": W: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr);   } while (0)

#define PTRACE(_num) do { if (options.debug >= _num) { \
	fprintf(stderr, NAME ": T: [%s] %12s +%4d %s()\n", _timestamp(), __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

void
dumpstack(const char *file, const int line, const char *func)
{
	void *buffer[32];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 32)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, NAME ": E: %12s +%4d : %s() : \t%s\n", file, line, func, strings[i]);
}

#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#undef EXIT_SYNTAXERR

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define EXIT_SYNTAXERR	2


/** @} */
typedef enum {
	CommandDefault,
	CommandStartup,
	CommandCheckpoint,
	CommandShutdown,
	CommandEditor,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int output;
	int debug;
	Command command;
	char *display;
	int screen;
	char *session;
	Bool dryrun;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.command = CommandDefault,
	.display = NULL,
	.screen = 0,
	.session = NULL,
	.dryrun = False,
};

Display *dpy = NULL;
int screen = 0;
Window root = None;
Bool shutting_down;

/** @section manager structure
  * @{
  *
  * There are two state machines: one for the overall session manager, another
  * for the state of a given SM client as viewed by the session manager.
  *
  * Session Manager State Diagram
  *
  * start:
  *	receive ProtocolSetup		--> protocol-setup
  *
  * protocol-setup:
  *	send ProtocolSetupReply		--> register
  *
  * register:
  *	receive RegisterClient		--> acknowledge-register
  *
  * acknowledge-register:
  *	send RegisterClientReply	--> idle
  *
  * idle:
  *	receive SetProperties		--> idle
  *	receive DeleteProperties	--> idle
  *	receive ConnectionClosed	--> start
  *	receive GetProperties		--> get-properties
  *	receive SaveYourselfRequest	--> save-yourself
  *	send SaveYourself		--> saving-yourself
  *
  * save-yourself:
  *	send SaveYourself		--> saving-yourself
  *
  * get-properties:
  *	send GetPropertiesReply		--> saving-yourself
  *
  * saving-yourself:
  *	receive InteractRequest		--> saving-yourself
  *	send Interact			--> saving-yourself
  *	send ShutdownCancelled		--> idle
  *	receive InteractDone		--> saving-yourself
  *	receive SetProperties		--> saving-yourself
  *	receive DeleteProperties	--> saving-yourself
  *	receive GetProperties		--> saving-get-properties
  *	receive SaveYourselfPhase2Request --> start-phase2
  *	receive SaveYourselfDone	--> save-yourself-done
  *
  * start-phase2:
  *   If all clients have sent either SaveYourselfPhase2Request or SaveYourselfDone
  *	send SaveYourselfPhase2		--> phase2
  *   else				--> saving-yourself
  *				    
  * phase2:
  *	receive InteractRequest		--> saving-yourself
  *	send Interact			--> saving-yourself
  *	send ShutdownCancelled		--> idle
  *	receive InteractDone		--> saving-yourself
  *	receive SetProperties		--> saving-yourself
  *	receive DeleteProperties	--> saving-yourself
  *	receive GetProperties		--> saving-get-properties
  *	receive SaveYourselfPhase2Request --> start-phase2
  *	receive SaveYourselfDone	--> save-yourself-done
  *
  * save-yourself-done:
  *	If all clients are saved:
  *	    If shutting down:
  *	        send Die		--> die
  *	    otherwise
  *		send SaveComplete	--> idle
  *
  *	If some clients are not saved:
  *					--> saving-yourself
  * 
  * die:
  *	SM stops accepting connections.
  *
  */
typedef enum {
	SMS_Start,		/* the manager is starting */
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

typedef struct {
	struct {
		char *display;
		char *session;
		char *audio;
	} local, remote;
	char *session_name;
	Bool remote_allowed;
	Bool saveInProgress;
	Bool shutdownInProgress;
	Bool checkpoint_from_signal;
	Bool shutdownCancelled;
	Bool phase2InProgress;
	Bool wantShutdown;
	struct {
		int saveType;		/* save type of sent SaveYourself */
		Bool shutdown;		/* shutdown setting of sent SaveYourself */
		int interactStyle;	/* interaction style of sent SaveYourself */
		Bool fast;		/* fast setting from sent SaveYourself */
	} sy;
	SMS_State state;
} SmManager;

SmManager manager = {
	.state = SMS_Start,
};

const char *non_local_display_env = NULL;
const char *non_local_session_env = NULL;

/** @} */

/** @section client structure
  *
  * State machine:
  *
  * start:
  *	ICE protocol setup complete	--> register
  *
  * register:
  *	send RegisterClient		--> collect-id
  *
  * collect-id:
  *	receive RegisterClientReply	--> idle
  *
  * shutdown-cancelled:
  *	send SaveYourselfDone		--> idle
  *
  * idle:
  *	receive Die			--> die
  *	receive SaveYourself		--> freeze-interaction
  *	send GetProperties		--> idle
  *	receive GetPropertiesReply	--> idle
  *	send SetProperties		--> idle
  *	send DeleteProperties		--> idle
  *	send ConnectionClosed		--> connection-closed
  *	send SaveYourselfRequest	--> idle
  *
  * die:
  *	send COnnectionClosed		--> connection-closed
  *
  * freeze-interaction:
  *	freeze interaction with user	--> save-yourself
  *
  * save-yourself:
  *	receive ShutdownCancelled	--> shutdown-cancelled
  *	send SetProperties		--> save-yourself
  *	send DeleteProperties		--> save-yourself
  *	send GetProperties		--> save-yourself
  *	receive GetPropertiesReply	--> save-yourself
  *	send InteractRequest		--> interact-request
  *	send SaveYourselfPhase2Request	--> waiting-for-phase2
  *	if shutdown mode
  *	    send SaveYourselfDone	--> save-yourself-done
  *	else
  *	    send SaveYourselfDone	--> idle
  *
  * waiting-for-phase2:
  *	receive ShutdownCancelled	--> shutdown-cancelled
  *	receive SaveYourselfPhase2	--> phase2
  *
  * phase2:
  *	receive ShutdownCancelled	--> shutdown-cancelled
  *	send SetProperties		--> save-yourself
  *	send DeleteProperties		--> save-yourself
  *	send GetProperties		--> save-yourself
  *	receive GetPropertiesReply	--> save-yourself
  *	send InteractRequest		--> interact-request (errors only)
  *	if shutdown mode
  *	    send SaveYourselfDone	--> save-yourself-done
  *	else
  *	    send SaveYourselfDone	--> idle
  *
  * interact-request:
  *	receive Interact		--> interact
  *	receive ShutdownCancelled	--> shutdown-cancelled
  *
  * interact:
  *	send InteractDone		--> save-yourself
  *	receive ShutdownCancelled	--> shutdown-cancelled
  *
  * save-yourself-done: (changing state is forbidden)
  *	receive SaveComplete		--> idle
  *	receive Die			--> die
  *	receive ShutdownCancelled	--> idle
  *
  * connection-closed:
  *	client stops participating in session
  *	
  * @{ */

typedef enum {
	SMC_Start,			/* the client connection is forming */
	SMC_Register,
	SMC_CollectId,
	SMC_ShutdownCancelled,
	SMC_Idle,			/* the client is idle */
	SMC_Die,
	SMC_FreezeInteraction,
	SMC_SaveYourself,		/* save-yourself issued */
	SMC_WaitingForPhase2,
	SMC_Phase2,
	SMC_InteractRequest,
	SMC_Interact,
	SMC_SaveYourselfDone,
	SMC_ConnectionClosed,
} SMC_State;

typedef struct {
	char *id;
	char *hostname;
	SmsConn sms;
	IceConn ice;
	int num_props;
	SmProp **props;
	Bool restarted;
	Bool userIssuedCheckpoint;
	Bool freeAfterBadSavePopup;
	Bool receivedDiscardCommand;
	char *discardCommand;
	char *saveDiscardCommand;
	int restartHint;
	struct {
		int saveType;		/* save type of last sent SaveYourself */
		Bool shutdown;		/* shutdown setting of sent SaveYourself */
		int interactStyle;	/* interact style of sent SaveYourself */
		Bool fast;		/* fast setting from sent SaveYourself */
	} sy;
	SMC_State state;
} SmClient;

/** @} */

/** @section primary state queues
  * @{ */

/** @brief initializing clients
  *
  * Clients are placed on this queue when they first register and are sent the
  * initial SaveYourself message.  When they send a SaveYourselfDone, they are
  * removed from this queue.  They are not allowed to interact on the initial
  * SaveYourself.
  */
static GQueue *initialClients = NULL;

/** @brief pending clients
  *
  * When a session is restored, executed clients are added to this queue a
  * pending.  When the client registers with the restoration client id, they are
  * removed from this queue and added to the runningClients and initialClients
  * queues.
  *
  * Note that the classical xsm(1) executes all pending clients on restoration
  * in parallel.  This is not necessarily efficient nor ideal.  For example,
  * dock applications may race for order of appearance in the dock.  A better
  * approach is to execute them one after the other and wait for them to
  * establish a session or fail before proceeding with the next client.  A guard
  * timer should also be used to determine whether restart was successful or
  * not.
  */
static GQueue *pendingClients = NULL;	/* pending clients */

/** @brief restart anyways clients
  *
  * When a client shuts down and removes its connection from the session manager
  * and has a restart hint of restart-anyways, the client is placed on this
  * list.  These clients will be added to the save file when the session is
  * restored even thought client is not connected to the session manager at the
  * imte of the checkpoint or shutdown.
  */
static GQueue *anywaysClients = NULL;

/** @brief restart immediately clients
  *
  * When a client disconnects from the session manager with a restart hint of
  * restart-immediately and the session manager is not shutting down, it is
  * placed on this queue and the clone command executed to restart the client.
  */
static GQueue *restartClients = NULL;

/** @brief failed save clients
  *
  * During a checkpoint or shutdown, when a client sends a SaveYourselfDone
  * indicating a failure, they are placed on this list.  The classical xsm(1)
  * indicates failure back to the user and gives the user the opportunity to
  * cancel saving state to the session file for the checkpoint or shutdown.
  * Otherwise, if no user interaction with the session manager exists (e.g. the
  * checkpoint or shutdown was as a result of a signal being sent to the session
  * manager), the save is completed even though clients have failed.
  */
static GQueue *failureClients = NULL;

/** @brief save-yourself clients
  *
  * Clients are placed on this queue when they have been sent a SaveYourself
  * message as part of a checkpoint or shutdown.  They stay on this queue until
  * they send a SaveYourselfDone message to the session manager.
  */

static GQueue *savselfClients = NULL;
/** @brief interaction requested clients
  *
  * Clients that request interaction are placed on this queue when they send an
  * InteractRequest message.  Only the client at the head of the queue is sent
  * an Interact message and removed from the queue when the client sends an
  * InteractDone message.  When the number of clients on the savselfClients
  * queue is equal to the sum of the number of clients on this queue and the
  * wphase2Clients queue, it is ok to enter the interaction phase.
  */
static GQueue *interacClients = NULL;

/** @brief wait for phase 2 clients
  *
  * Clients that send a SaveYourselfPhase2Request message are placed on this
  * queue.  During Phase 2, the clients on this queue are sent a
  * SaveYourselfPhase2 message.  Clients are removed from this queue when they
  * send a SaveYourselfDone message.  When the number of clients on the
  * savselfClients client queue and this queue are the same, the session manager
  * is ok to enter Phase 2.
  */
static GQueue *wphase2Clients = NULL;

/** @brief running clients
  *
  * Clients that are connected to the session manager and assigned a client id
  * are added to this queue when they connect.  When the client disconnects from
  * the session manager, they are removed from this queue.
  */
static GQueue *runningClients = NULL;

/** @brief checkpoint clients
  *
  * Clients that are being checkpointed are added to this list at the start of
  * the checkpoint or shutdown cycle (along with savselfClients).  They are
  * removed from the list when the SaveComplete or Die message is sent.
  */
static GQueue *checkptClients = NULL;

/** @brief session clients
  *
  * Clients that are involved in the session.  All clients are added to this
  * list once identified and are only removed from the list once they have
  * disconnected with a restart hint of never.
  */
static GQueue *sessionClients = NULL;

/** @} */

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

	(void) clientData;
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
	(void) clientData;
	(void) saveType;
	(void) shutdown;
	(void) interactStyle;
	(void) fast;
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
	(void) clientData;
	SmcCloseConnection(smcConn, 0, NULL);
	exit(EXIT_SUCCESS);
}

void
wmpSaveCompleteCB(SmcConn smcConn, SmPointer clientData)
{
	(void) smcConn;
	(void) clientData;
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
	(void) clientData;
	if (sent_save_done)
		return;
	SmcSaveYourselfDone(smcConn, False);
	sent_save_done = 1;
}

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

void
setInitialProperties(SmClient *c, int num, SmProp *props[])
{
	int i;

	CPRINTF(1, c, "setting initial properties\n");
	c->props = props;
	c->num_props = num;

	for (i = 0; i < c->num_props; i++) {
		if (!strcmp(props[i]->name, SmDiscardCommand)) {
			if (props[i]->num_vals && props[i]->vals && props[i]->vals[0].value) {
				free(c->discardCommand);
				c->discardCommand = strdup(props[i]->vals[0].value);
			}
		} else if (!strcmp(props[i]->name, SmRestartStyleHint)) {
			if (props[i]->num_vals && props[i]->vals && props[i]->vals[0].value) {
				c->restartHint = *((char *) props[i]->vals[0].value);
			}
		}
	}
}

void
freeClient(SmClient *c)
{
	int i;

	if (g_queue_find(initialClients, c)) {
		EPRINTF("Client Id = %s, still on iniitalClients queue at deletion\n", c->id);
		g_queue_remove(initialClients, c);
	}
	if (g_queue_find(pendingClients, c)) {
		EPRINTF("Client Id = %s, still on pendingClients queue at deletion\n", c->id);
		g_queue_remove(pendingClients, c);
	}
	if (g_queue_find(anywaysClients, c)) {
		EPRINTF("Client Id = %s, still on anywaysClients queue at deletion\n", c->id);
		g_queue_remove(anywaysClients, c);
	}
	if (g_queue_find(restartClients, c)) {
		EPRINTF("Client Id = %s, still on restartClients queue at deletion\n", c->id);
		g_queue_remove(restartClients, c);
	}
	if (g_queue_find(failureClients, c)) {
		EPRINTF("Client Id = %s, still on failureClients queue at deletion\n", c->id);
		g_queue_remove(failureClients, c);
	}
	if (g_queue_find(savselfClients, c)) {
		EPRINTF("Client Id = %s, still on savselfClients queue at deletion\n", c->id);
		g_queue_remove(savselfClients, c);
	}
	if (g_queue_find(wphase2Clients, c)) {
		EPRINTF("Client Id = %s, still on wphase2Clients queue at deletion\n", c->id);
		g_queue_remove(wphase2Clients, c);
	}
	if (g_queue_find(interacClients, c)) {
		EPRINTF("Client Id = %s, still on inetracClients queue at deletion\n", c->id);
		g_queue_remove(interacClients, c);
	}
	if (g_queue_find(runningClients, c)) {
		EPRINTF("Client Id = %s, still on runningClients queue at deletion\n", c->id);
		g_queue_remove(runningClients, c);
	}
	if (g_queue_find(checkptClients, c)) {
		EPRINTF("Client Id = %s, still on checkptClients queue at deletion\n", c->id);
		g_queue_remove(checkptClients, c);
	}
	g_queue_remove(sessionClients, c);

	free(c->id);
	c->id = NULL;
	free(c->hostname);
	c->hostname = NULL;
	c->sms = NULL;
	c->ice = NULL;
	for (i = 0; i < c->num_props; i++) {
		SmFreeProperty(c->props[i]);
		c->props[i] = NULL;
	}
	free(c->props);
	c->restarted = False;
	c->userIssuedCheckpoint = False;
	c->freeAfterBadSavePopup = False;
	c->receivedDiscardCommand = False;
	free(c->discardCommand);
	free(c->saveDiscardCommand);
	c->restartHint = 0;
	c->sy.saveType = 0;
	c->sy.shutdown = False;
	c->sy.interactStyle = 0;
	c->sy.fast = False;
	c->state = SMC_Start;
	free(c);
}

void
remote_start(const char *restart_protocol, const char *restart_machine, const char *program,
	     char **args, const char *cwd, char **env, const char *non_local_display_env,
	     const char *non_local_session_env)
{
	(void) restart_protocol;
	(void) restart_machine;
	(void) program;
	(void) args;
	(void) cwd;
	(void) env;
	(void) non_local_display_env;
	(void) non_local_session_env;
	/* FIXME: port this function */
}

#if 0
#define DEFAULT_RSTART_PROTOCOL	"rstart-rsh"
#else
#define DEFAULT_RSTART_PROTOCOL	"rstart-ssh"
#endif

/** @brief determine method to restart a client
  *
  * The #restart_service_prop argument is a string valued property set by the
  * client, or NULL when the client has not set the property.  The property is
  * the non-standard property, _XC_RestartService.  The format of the property
  * is "remote_start_protocol/remote_start_data".  An example is
  * "rstart-rsh/hostname".  This is a non-standard property, which is the whoe
  * reason we need this function to dtermine the restart method.  The proxy uses
  * this property to override the 'client_host_name' from the ICE connection
  * (the proxy is connected to the SM via a local connection, but the proxy may
  * be acting as a proxy for a remote client).
  *
  * 'client_host_name' is the connection info obtained from the ICE connection.
  * It's format is "transport/host_info".  An example is "tcp/machine-port".
  *
  * If #resetart_service_prop is NULL, we use 'client_host_name' to determine
  * the restart method.  If the transport is "local", we do a local restart.
  * Otherwise, we use the default "rstart-rsh" method.
  *
  * If #restart_service_prop is non-NULL, we check the #remote_start_protocol
  * field.  "local" means a local restart.  Currently, the only remote protocol
  * we recognize is "rstart-rsh".  If the remote protocol is "rstart-rsh" but
  * the hostname in the #restart_service_prop matches client_host_name, we do a
  * local restart.
  *
  * On return, set the run_local flag, restart_protocol and restart_machine.
  */
void
getRestartInfo(const char *restart_service_prop, const char *client_host_name,
	       Bool *run_local, char **restart_protocol, char **restart_machine)
{
	*run_local = False;
	*restart_protocol = NULL;
	*restart_machine = NULL;

	if (restart_service_prop) {
		char hostname[256];
		const char *p;

		if ((p = strchr(restart_service_prop, '/'))) {
			*restart_machine = strdup(p + 1);
			*restart_protocol = strndup(restart_service_prop, p - restart_service_prop);
		} else {
			*restart_protocol = strdup(DEFAULT_RSTART_PROTOCOL);
			*restart_machine = strdup(restart_service_prop);
		}
		gethostname(hostname, sizeof(hostname));
		if (!strcmp(*restart_machine, hostname) || !strcmp(*restart_protocol, "local"))
			*run_local = True;
	} else {
		const char *p;

		if (strncmp(client_host_name, "tcp/", 4) && strncmp(client_host_name, "decnet/", 7))
			*run_local = True;
		else {
			*restart_protocol = strdup(DEFAULT_RSTART_PROTOCOL);
			if ((p = strchr(client_host_name, '/')))
				*restart_machine = strdup(p + 1);
			else
				*restart_machine = strdup(client_host_name);
		}
	}
}

Bool
checkIsManager(const char *program)
{
	static const char *names[] = { "twm", "ctwm", "etwm", NULL };
	const char **pp = names;

	while (*pp)
		if (!strcmp(program, *pp++))
			return True;
	return False;
}

void
cloneClient(SmClient *c, Bool useSavedState)
{
	const char *cwd = NULL, *program = NULL;
	char **args = NULL, **env = NULL, **pp, *p;
	const char *restart_service_prop = NULL;
	char *restart_protocol = NULL, *restart_machine = NULL;
	Bool run_local = True;
	int i, j;

	for (i = 0; i < c->num_props; i++) {
		SmProp *prop = c->props[i];

		if (!strcmp(prop->name, SmProgram)) {
			program = (char *) prop->vals[0].value;
		} else if (!strcmp(prop->name, SmCurrentDirectory)) {
			cwd = (char *) prop->vals[0].value;
		} else if (!strcmp(prop->name, "_XC_RestartService")) {
			restart_service_prop = (char *) prop->vals[0].value;
		} else if ((!useSavedState && !strcmp(prop->name, SmCloneCommand)) ||
			   !strcmp(prop->name, SmRestartCommand)) {
			args = calloc(prop->num_vals + 1, sizeof(*args));
			for (j = 0, pp = args; j < prop->num_vals; j++)
				*pp++ = (char *) prop->vals[j].value;
		} else if (!strcmp(prop->name, SmEnvironment)) {
			env = calloc(prop->num_vals + 3 + 1, sizeof(*env));
			for (j = 0, pp = env; j < prop->num_vals; j++) {
				p = prop->vals[j].value;
				if ((manager.local.display && strstr(p, "DISPLAY=") == p) ||
				    (manager.local.session && strstr(p, "SESSION_MANAGER=") == p) ||
				    (manager.local.audio && strstr(p, "AUDIOSERVER=") == p))
					continue;
				*pp++ = p;
			}
			if (manager.local.display)
				*pp++ = manager.local.display;
			if (manager.local.session)
				*pp++ = manager.local.session;
			if (manager.local.audio)
				*pp++ = manager.local.audio;
			*pp = NULL;
		}
	}
	if (program && args) {
		if (options.output > 1) {
			OPRINTF(1, "\t%s\n", program);
			fprintf(stdout, "I:\t");
			for (pp = args; *pp; pp++)
				fprintf(stdout, "%s ", *pp);
			fprintf(stdout, "\n");
			fflush(stdout);
		}
		getRestartInfo(restart_service_prop, c->hostname, &run_local, &restart_protocol,
			       &restart_machine);
		if (run_local) {
			/* clone on the local host */
			switch (fork()) {
			case -1:	/* error */
				EPRINTF("cannot fork(): %s\n", strerror(errno));
				break;
			case 0:	/* the child */
				if (chdir(cwd)) { }
				execvpe(program, args, env);
				EPRINTF("cannot execvpe %s\n", program);
				_exit(255);
			case 1:	/* the parent */
				break;
			}
		} else if (!manager.remote_allowed) {
			EPRINTF("Cannot remote clone client id %s\n", c->id);
		} else {
			/* clone on remote machine */
			remote_start(restart_protocol, restart_machine,
				     program, args, cwd, env,
				     non_local_display_env, non_local_session_env);
		}
		free(restart_protocol);
		free(restart_machine);
	} else {
		EPRINTF("Cannot restart client id = %s: no program or args\n", c->id);
	}
	free(args);
	free(env);
}

gint
idCompareFunc(gconstpointer a, gconstpointer b)
{
	const SmClient *ca = a;
	const SmClient *cb = b;

	return strcmp(ca->id, cb->id);
}

void
popupBadSave()
{
	/* FIXME */
}

void
writeSave()
{
	/* FIXME */
}

void endSession(int status);

void
finishUpSave()
{
	GList *link;

	OPRINTF(1, "All clients isseud SAVE YOURSELF DONE\n");

	manager.saveInProgress = False;
	manager.phase2InProgress = False;

	/* Execute discard commands.  Not too sure at the moment why this is being done
	   here. */
	for (link = g_queue_peek_head_link(checkptClients); link; link = link->next) {
		SmClient *c = link->data;

		if (!c->receivedDiscardCommand)
			continue;

		if (c->discardCommand) {
			/* execute discard command */
			free(c->discardCommand);
			c->discardCommand = NULL;
		}
		if (c->saveDiscardCommand) {
			c->discardCommand = c->saveDiscardCommand;
			c->saveDiscardCommand = NULL;
		}
	}
	/* write save file */
	writeSave();

	if (manager.wantShutdown && manager.shutdownCancelled) {
		/* shutdown was cancelled, do no more */
		manager.shutdownCancelled = False;
	} else if (manager.wantShutdown) {
		if (g_queue_is_empty(runningClients)) {
			/* when there are no running clients we can simply exit now */
			endSession(0);
		}
		/* When shutting down with running clients, send each client a Die
		   message and wait for them all to disconnect from the session manager
		   before exiting.  The shutdownInProgress flag accomplishes this;
		   however, it would be better handled as a manager state. */
		manager.shutdownInProgress = True;
		for (link = g_queue_peek_head_link(checkptClients); link; link = link->next) {
			SmClient *c = link->data;

			SmsDie(c->sms);
			CPRINTF(1, c, "sent DIE\n");
		}
	} else {
		SmClient *c;

		/* When checkpointing (without shutdown), send all clients a SaveComplete 
		   message so that they can continue to alter their internal state. */
		while ((c = g_queue_pop_head(checkptClients))) {
			SmsSaveComplete(c->sms);
			CPRINTF(1, c, "sent SAVE COMPLETE\n");
		}
	}
	/* If these were handled using a state variable, the logic would be more
	   apparent. */
	if (!manager.shutdownInProgress) {
		/* remove popups */
		if (manager.checkpoint_from_signal)
			manager.checkpoint_from_signal = False;
	}
}

/** @brief let a client interact
  *
  * Let the client at the head of the interact request queue interact with the
  * user.  The client will be removed from the queue when it issues an
  * InteractDone message.  The management state should be set to indicate
  * interaction.
  */
void
letClientInteract()
{
	SmClient *c;

	if ((c = g_queue_peek_head(interacClients)))
		SmsInteract(c->sms);
}

/** @brief start save-yourself phase2
  *
  * During save-yourself phase2, a SaveYourselfPhase2 message is sent to all
  * clients that requested phase 2.  We have no more need for the wphase2Clients
  * list at this time, but must mark the manager state as in phase2.
  *
  * XXX: It is not clear what happens when there are no clients requesting
  * phase 2.  This function is simply not called when the wphase2Clients queue
  * is empty.
  *
  * XXX: This function issues a SaveYourselfPhase2 message to all clients
  * requesting a phase 2.  I'm not sure whether they should be permitted to all
  * run at the same time: that is, consider smproxy(1)-like sub-managers.
  */
void
startPhase2()
{
	SmClient *c;

	while ((c = g_queue_pop_head(wphase2Clients))) {
		DPRINTF(1, "Client %s sending SAVE YOURSELF PHASE 2\n", c->id);
		SmsSaveYourselfPhase2(c->sms);
	}
	manager.state = SMS_Phase2;
}

/** @brief ok to enter interaction phase
  *
  * Tests whether it is ok to enter the interaction phase.  There are basically
  * three phases to a checkpoint or shutdown:
  *
  * 1. SM asks each client to save itself and waits for one of three responses
  *    from each client:
  *
  *    1. SaveYourselfDone:		the client is done saving itself.
  *    2. InteractRequest:		the client requests interaction with the user
  *    3. SaveYourselfPhase2Request:	the client requests a save-yourself phase2
  *
  * 2. Once a response has been received from each client, the interaction phase
  *    can be entered.  In the interaction phase, each client that requested
  *    interaction will be allowed to interact before the phase is complete.
  *
  * 3. Once the interaction phase is complete, save-yourself phase 2 can begin
  *    if it was requested.
  *    
  */
Bool
okToEnterInteractPhase()
{
	return (g_queue_get_length(interacClients) + g_queue_get_length(wphase2Clients)
		== g_queue_get_length(savselfClients));
}

/** @brief ok to enter save-yourself phase 2
  *
  * Tests whether it is ok to enter save-yourself phase 2.  There are basically
  * three phases to a checkpoint or shutdown:
  *
  * 1. SM asks each client to save itself and waits for one of three responses
  *    from each client:
  *
  *    1. SaveYourselfDone:		the client is done saving itself.
  *    2. InteractRequest:		the client requests interaction with the user
  *    3. SaveYourselfPhase2Request:	the client requests a save-yourself phase2
  *
  * 2. Once a response has been received from each client, the interaction phase
  *    can be entered.  In the interaction phase, each client that requested
  *    interaction will be allowed to interact (one at a time) before the phase
  *    is complete.
  *
  * 3. Once the interaction phase is complete, save-yourself phase 2 can begin
  *    if it was requested.
  */
Bool
okToEnterPhase2()
{
	return (g_queue_get_length(wphase2Clients) == g_queue_get_length(savselfClients));
}

void
doSave(int saveType, int interactStyle, Bool fast)
{
	GList *link;

	manager.saveInProgress = True;
	manager.shutdownCancelled = False;
	manager.phase2InProgress = False;

	if (g_queue_is_empty(runningClients))
		finishUpSave();
	manager.sy.saveType = saveType;
	manager.sy.shutdown = manager.wantShutdown;
	manager.sy.interactStyle = interactStyle;
	manager.sy.fast = fast;
	for (link = g_queue_peek_head_link(runningClients); link; link = link->next) {
		SmClient *c = link->data;

		c->state = SMC_SaveYourself;
		c->sy.saveType = saveType;
		c->sy.shutdown = manager.wantShutdown;
		c->sy.interactStyle = interactStyle;
		c->sy.fast = fast;
		SmsSaveYourself(c->sms, saveType, manager.wantShutdown, interactStyle, fast);
		g_queue_remove(savselfClients, c);
		g_queue_push_tail(savselfClients, c);
		g_queue_remove(checkptClients, c);
		g_queue_push_tail(checkptClients, c);

		c->userIssuedCheckpoint = True;
		c->receivedDiscardCommand = False;

		CPRINTF(1, c, "send SAVE YOURSELF\n");
		CPRINTF(1, c, "    Save Type = %s\n",
			saveType == SmSaveLocal ? "local" :
			saveType == SmSaveGlobal ? "global" : "both");
		CPRINTF(1, c, "    Shutdown = %s\n", manager.wantShutdown ? "true" : "false");
		CPRINTF(1, c, "    Interact Style = %s\n",
			interactStyle == SmInteractStyleNone ? "none" :
			interactStyle == SmInteractStyleErrors ? "errors" : "any");
		CPRINTF(1, c, "    Fast = %s\n", fast ? "true" : "false");
	}
	OPRINTF(1, "Sent SAVE YOURSELF to all clients.  Wiating for\n");
	OPRINTF(1, "SAVE YOURSELF DONE, INTERACT REQUEST or\n");
	OPRINTF(1, "SAVE YOURSELF PHASE 2 REQUEST from each client\n");
}

/** @section client callbacks
  *
  * @{ */


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
  *
  * When #previousId is NULL and the registration is successful, SmsSaveYourself
  * should be called with a saveType of SmSaveLocal, shudown False,
  * interactStyle SmInteractStyleNone, and fast False to generate the initial
  * state information for the client under the protocol.
  */
Status
registerClientCB(SmsConn smsConn, SmPointer data, char *previousId)
{
	SmClient *c = (typeof(c)) data;

	c->state = SMC_CollectId;

	if (!previousId) {
		free(c->id);
		c->id = SmsGenerateClientID(smsConn);
	} else
		do {
			SmClient *p;
			GList *link;

			for (link = g_queue_peek_head_link(pendingClients); link; link = link->next) {
				p = link->data;
				if (!strcmp(p->id, previousId)) {
					g_queue_delete_link(pendingClients, link);
					setInitialProperties(c, p->num_props, p->props);
					p->num_props = 0;
					p->props = NULL;
					freeClient(p);
					break;
				}
			}
			if (link)
				break;
			for (link = g_queue_peek_head_link(anywaysClients); link; link = link->next) {
				p = link->data;
				if (!strcmp(p->id, previousId)) {
					g_queue_delete_link(anywaysClients, link);
					setInitialProperties(c, p->num_props, p->props);
					p->num_props = 0;
					p->props = NULL;
					freeClient(p);
					break;
				}
			}
			if (link)
				break;
			for (link = g_queue_peek_head_link(restartClients); link; link = link->next) {
				p = link->data;
				if (!strcmp(p->id, previousId)) {
					g_queue_delete_link(restartClients, link);
					setInitialProperties(c, p->num_props, p->props);
					p->num_props = 0;
					p->props = NULL;
					freeClient(p);
					break;
				}
			}
			if (link)
				break;
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
		c->sy.saveType = SmSaveLocal;
		c->sy.shutdown = False;
		c->sy.interactStyle = SmInteractStyleNone;
		c->sy.fast = False;
		SmsSaveYourself(smsConn, SmSaveLocal, False, SmInteractStyleNone, False);

		g_queue_remove(initialClients, c);
		g_queue_push_tail(initialClients, c);
	} else {
		/* TODO: update client GtkListStore */
	}
	return (1);
}

/** @brief interact request callback
  *
  * When a client receives a SaveYourself message with an interaction style of
  * SmInteractStyleErrors or SmInteractStyleAny, the client may choose to
  * interact with the user.  Because only one client can interact with the user
  * at a time, the client must request to interact with the user.  The session
  * manager should keep a queue of all clients wishing to interact.  It should
  * send a Interact message to one client at a time and wait for an InteractDone
  * message before contining with the next client.
  *
  * The #dialogType argument specifies either SmDialogError indicating that the
  * client wants to start an error dialog, or SmDialogNormal meaning that the
  * client wishes to start a nonerror dialog.
  *
  * If a shutdown is in progress, the user may have the option of cancelling the
  * shutdown.  If the shutdown is cancelled (specified in the InteractDone
  * message), the session manager should send a ShutdownCancelled message to
  * each client that requested to interact.
  *
  * It is necessary to wait to enter the interaction phase of the checkpoint or
  * shutdown until after each client has sent SaveYourselfDone,
  * SaveYourselfPhase2Request, or IntractRequest.  So, like xsm(1), we basically
  * keep three queues: one for each.  The clients waiting for interaction are in
  * the interacClients queue.  The clients waiting for phase2 are in
  * wphase2Clients queue.  All clients awaiting SaveYourselfDone are in
  * savselfClients queue.
  */
void
interactRequestCB(SmsConn smsConn, SmPointer data, int dialogType)
{
	SmClient *c = (typeof(c)) data;

	(void) smsConn;
	CPRINTF(1, c, "received INTERACT REQUEST [Dialog Type %s]\n", dialogType == SmDialogNormal ? "normal" : "errors");

	if (c->state != SMC_SaveYourself) {
		EPRINTF("Client Id = %s, received INTERACT REQUEST out of state\n", c->id);
		/* XXX: must do more than just return??? */
		return;
	}
	if (c->sy.interactStyle == SmInteractStyleNone) {
		EPRINTF("Client Id = %s, received INTERACT REQUEST when style was none\n", c->id);
		/* XXX: must do more than just return??? */
		return;
	} else if (c->sy.interactStyle == SmInteractStyleErrors && dialogType == SmDialogNormal) {
		EPRINTF("Client Id = %s, received INTERACT REQUEST normal when style was errors\n", c->id);
		/* XXX: must do more than just return??? */
		return;
	}
	g_queue_remove(interacClients, c);
	g_queue_push_tail(interacClients, c);
	if (okToEnterInteractPhase())
		letClientInteract();
}

/** @brief interact done callback
  *
  * When the client is done interacting with the user, the interact done
  * callback will be invoked.  Note that the shutdown can be cancelled only if
  * the corresponding SaveYourself specified True for shutdown and
  * SmInteractStyleErrors or SmInteractStyleAny for the interact style.
  *
  * Note that the original xsm(1) does not check that the interact syte and
  * shutdown sent with the SaveYourself message is consistent with the
  * cancellation of shutdown flag received with the InteractDone message.  The
  * spec says that if it is inconsistent it should be ignored.
  */
void
interactDoneCB(SmsConn smsConn, SmPointer data, Bool cancelShutdown)
{
	SmClient *c = (typeof(c)) data;

	(void) smsConn;
	CPRINTF(1, c, "received INTERACT DONE [Cancel Shutdown = %s]\n", cancelShutdown ? "true" : "false");

	if (cancelShutdown) {
		g_queue_clear(interacClients);
		g_queue_clear(wphase2Clients);
		if (manager.shutdownCancelled)
			return;
		manager.shutdownCancelled = True;
		while ((c = g_queue_pop_head(checkptClients))) {
			SmsShutdownCancelled(c->sms);
			CPRINTF(1, c, "sent SHUTDOWN CANCELLED\n");
		}
	} else {
		g_queue_pop_head(interacClients);
		if (!g_queue_is_empty(interacClients))
			letClientInteract();
		else {
			OPRINTF(1, "Done interacting with all clients.\n");
			if (!g_queue_is_empty(wphase2Clients))
				startPhase2();
		}
	}
}

/** @brief save yourself request callback
  *
  * The save yourself request prompts the session manager to initiate a checkpoint
  * or shutdown.  If #global is set to False then the SaveYourself should only be
  * sent to the client that requested it.  Otherwise, it should be sent to all
  * running clients.
  *
  * Note that in the classical xsm(1), this procedure is not supported.  Which
  * is strange.  It is in fact quite easy to support.  Also, this is the only
  * way for a SmRestartImmediately restart hint client to shut down.  If it
  * exits on its own it will be restarted by the session manager.  It must issue
  * a non-global SaveYourselfRequest with shutdown to get the session manager to
  * send it a Die message and shut it down.
  */
void
saveYourselfReqCB(SmsConn smsConn, SmPointer data, int saveType, Bool shutdown,
		  int interactStyle, Bool fast, Bool global)
{
	SmClient *c = (typeof(c)) data;

	manager.sy.saveType = saveType;
	manager.sy.shutdown = shutdown;
	manager.sy.interactStyle = interactStyle;
	manager.sy.fast = fast;

	if (global) {
		GList *link;

		for (link = g_queue_peek_head_link(runningClients); link; link = link->next) {
			c = link->data;
			c->state = SMC_SaveYourself;
			c->sy.saveType = saveType;
			c->sy.shutdown = shutdown;
			c->sy.interactStyle = interactStyle;
			c->sy.fast = fast;
			SmsSaveYourself(c->sms, saveType, shutdown, interactStyle, fast);
			g_queue_remove(checkptClients, c);
			g_queue_push_tail(checkptClients, c);
			g_queue_remove(savselfClients, c);
			g_queue_push_tail(savselfClients, c);
		}
	} else {
		c->state = SMC_SaveYourself;
		c->sy.saveType = saveType;
		c->sy.shutdown = shutdown;
		c->sy.interactStyle = interactStyle;
		c->sy.fast = fast;
		SmsSaveYourself(smsConn, saveType, shutdown, interactStyle, fast);
		g_queue_remove(checkptClients, c);
		g_queue_push_tail(checkptClients, c);
		g_queue_remove(savselfClients, c);
		g_queue_push_tail(savselfClients, c);
	}
	manager.state = SMS_SavingYourself;
}

/** @brief save yourself phase 2 request callback
  *
  * This request is sent by clients that manage other clients (such as the window
  * manager, workspace managers, and so on).  Such managers must make sure that all
  * of the clients that are being managed are in an idle state so that their state
  * can be saved.
  *
  * When received during an initial client save-yourself cycle (that is, this is
  * a response to the initial SaveYourself message sent immediately after client
  * registration), we simply send the SaveYourselfPhase2 message now.  If, on
  * the other hand, we are performing a checkpoint or shutdown, we must wait for
  * the conditions of entering save-yourself phase2 state before sending.
  */
void
saveYourselfP2ReqCB(SmsConn smsConn, SmPointer data)
{
	SmClient *c = (typeof(c)) data;

	if (!manager.saveInProgress) {
		SmsSaveYourselfPhase2(smsConn);
	} else {
		g_queue_remove(wphase2Clients, c);
		g_queue_push_tail(wphase2Clients, c);
		if (!g_queue_is_empty(interacClients) && okToEnterInteractPhase()) {
			letClientInteract();
		} else if (okToEnterPhase2()) {
			startPhase2();
		}
	}
}

/** @brief save yourself done callback
  *
  * When the client is done saving its state in response to a SaveYourself message,
  * this callback will be invoked.  Before the SaveYourselfDone was sent, the
  * client must have set each required property at least once since it registered
  * with the session manager.
  *
  * There is a little bit of a race condition here: if a client newly registers
  * during a checkpoint or a shutdown there is the issue that they have not been
  * sent the SaveYourself message associated with the checkpoint or shutdown at
  * this point.  Also, we may have already entered the interact phase or
  * save-yourself phase 2.  The original xsm(1) does not address this condition
  * either.
  */
void
saveYourselfDoneCB(SmsConn smsConn, SmPointer data, Bool success)
{
	SmClient *c = (typeof(c)) data;
	GList *link;

	(void) smsConn;
	CPRINTF(1, c, "received SAVE YOURSELF DONE [Success = %s]\n", success ? "true" : "false");

	if ((link = g_queue_find(savselfClients, c))) {
		g_queue_delete_link(savselfClients, link);
		if ((link = g_queue_find(initialClients, c))) {
			g_queue_delete_link(initialClients, link);
			SmsSaveComplete(c->sms);
		}
		return;
	}
	if (!success) {
		g_queue_remove(failureClients, c);
		g_queue_push_tail(failureClients, c);
	}

	if (g_queue_is_empty(savselfClients)) {
		if (!g_queue_is_empty(failureClients) && !manager.checkpoint_from_signal)
			popupBadSave();
		else
			finishUpSave();
	} else if (!g_queue_is_empty(interacClients) && okToEnterInteractPhase())
		letClientInteract();
	else if (!g_queue_is_empty(wphase2Clients) && okToEnterPhase2())
		startPhase2();
}

/** @brief close connection callback
  *
  * If the client properly terminates (that is, it calls SmcCloseConnection) this
  * callback is invoked.  The #reasons argument will most likely be NULL and the
  * #count argument zero (0) if resignation is expected by the user.  Otherwise, it
  * contains a list of null-terminated compound text strings representing the
  * reason for termination.  The session manager should display these reason
  * messages to the user.  Call SmsFreeReasons to free the #reasons messages.
  *
  * We use dbus libnotify notifications (typically resulting in pop-ups) to 
  */
void
closeConnectionCB(SmsConn smsConn, SmPointer data, int count, char **reasons)
{
	SmClient *c = (typeof(c)) data;
	GList *link;
	int i;

	(void) smsConn;
	CPRINTF(1, c, "received CONNECTION CLOSED\n");
	for (i = 0; i < count; i++)
		OPRINTF(1, "   Reason string %d: %s\n", i, reasons[i]);

	/* XXX: should save reason messages against closed client for display */
	SmFreeReasons(count, reasons);

	OPRINTF(1, "ICE Connection closed, fd = %d\n", IceConnectionNumber(c->ice));

	SmsCleanUp(c->sms);
	IceSetShutdownNegotiation(c->ice, False);
	IceCloseConnection(c->ice);

	c->ice = NULL;
	c->sms = NULL;

	g_queue_remove(runningClients, c);
	g_queue_remove(checkptClients, c);

	if (manager.saveInProgress) {
		if ((link = g_queue_find(savselfClients, c))) {
			g_queue_remove(failureClients, c);
			g_queue_push_tail(failureClients, c);
			c->freeAfterBadSavePopup = True;
		}

		g_queue_remove(interacClients, c);
		g_queue_remove(wphase2Clients, c);

		if (link && g_queue_is_empty(savselfClients)) {
			if (!g_queue_is_empty(failureClients) && !manager.checkpoint_from_signal)
				popupBadSave();
			else
				finishUpSave();
		} else if (!g_queue_is_empty(interacClients) && okToEnterInteractPhase())
			letClientInteract();
		else if (!g_queue_is_empty(wphase2Clients) && okToEnterPhase2())
			startPhase2();
	}
	if (c->restartHint == SmRestartImmediately && !manager.shutdownInProgress) {
		cloneClient(c, True);
		g_queue_remove(restartClients, c);
		g_queue_push_tail(restartClients, c);
	} else if (c->restartHint == SmRestartAnyway) {
		g_queue_remove(anywaysClients, c);
		g_queue_push_tail(anywaysClients, c);
	} else if (!c->freeAfterBadSavePopup) {
		freeClient(c);
	}
	if (manager.shutdownInProgress) {
		if (g_queue_is_empty(runningClients))
			endSession(0);
		if (g_queue_is_empty(checkptClients))
			manager.shutdownInProgress = False;
	}
	/* FIXME: update GtkListStore */
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
setPropertiesCB(SmsConn smsConn, SmPointer data, int num, SmProp *props[])
{
	SmClient *c = (typeof(c)) data;
	int i;

	(void) smsConn;
	CPRINTF(1, c, "receive SET PROPERTIES [Numb props = %d]\n", num);

	for (i = 0; i < num; i++) {
		SmProp *prop = props[i];
		int j;

		for (j = 0; j < c->num_props; j++)
			if (!strcmp(prop->name, c->props[j]->name)
			    && !strcmp(prop->type, c->props[j]->type))
				break;
		if (j < c->num_props)
			SmFreeProperty(c->props[j]);
		else {
			c->num_props++;
			c->props = realloc(c->props, c->num_props * sizeof(*c->props));
		}
		c->props[j] = prop;

		if (!strcmp(prop->name, SmDiscardCommand)) {
			if (prop->num_vals && prop->vals && prop->vals[0].value) {
				if (manager.saveInProgress) {
					/* We are in the middle of save yourself.  We
					   save the discard command whe get now, and make 
					   it the current discard command when the save
					   is over. */
					free(c->saveDiscardCommand);
					c->saveDiscardCommand = strdup(prop->vals[0].value);
					c->receivedDiscardCommand = True;
				} else {
					free(c->discardCommand);
					c->discardCommand = strdup(prop->vals[0].value);
				}
			}
		} else if (!strcmp(prop->name, SmRestartStyleHint)) {
			if (prop->num_vals && prop->vals && prop->vals[0].value)
				c->restartHint = *((char *) prop->vals[0].value);
		}
	}
	free(props);
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
	int i;

	(void) smsConn;
	CPRINTF(1, c, "received DELETE PROPERTIES [Num props = %d]\n", num);

	for (i = 0; i < num; i++) {
		char *name = names[i];
		int j, k;

		OPRINTF(1, "    Property name: %s\n", name);

		for (j = 0; j < c->num_props; j++)
			if (!strcmp(name, c->props[j]->name))
				break;
		if (j < c->num_props) {
			SmFreeProperty(c->props[j]);
			for (k = j; k + 1 < c->num_props; k++)
				c->props[k] = c->props[k + 1];
			c->num_props--;
			if (!strcmp(name, SmDiscardCommand)) {
				free(c->discardCommand);
				c->discardCommand = NULL;
				free(c->saveDiscardCommand);
				c->saveDiscardCommand = NULL;
			}
		}
		free(name);
	}
	free(names);
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

	CPRINTF(1, c, "received GET PROPERTIES\n");
	SmsReturnProperties(smsConn, c->num_props, c->props);
}

/** @brief new client callback
  */
static Status
newClientCB(SmsConn smsConn, SmPointer data, unsigned long *mask, SmsCallbacks *cb, char **reason)
{
	SmClient *c;

	(void) data;
	(void) reason;
	if (!(c = calloc(1, sizeof(*c)))) {
		EPRINTF("%s\n", strerror(errno));
		return (0);
	}
	*mask = 0;

	c->sms = smsConn;
	c->ice = SmsGetIceConnection(smsConn);
	c->id = NULL;
	c->hostname = NULL;
	c->restarted = False;
	c->userIssuedCheckpoint = False;
	c->receivedDiscardCommand = False;
	c->freeAfterBadSavePopup = False;
	c->props = NULL;
	c->discardCommand = NULL;
	c->saveDiscardCommand = NULL;
	c->restartHint = SmRestartIfRunning;
	c->state = SMC_Start;

	g_queue_push_tail(sessionClients, c);
	g_queue_push_tail(runningClients, c);

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

/** @} */

gboolean
on_lfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	IceConn ice;
	IceAcceptStatus status;
	IceConnectStatus cstatus;
	IceListenObj obj = (typeof(obj)) data;

	(void) chan;
	/* IceIOErrorHandler should handle this ... */
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;
	}
	if (shutting_down) /* XXX: ???? */
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

			DPRINTF(1, "ICE connection opened by client, fd = %d, accepted at networkId %s\n", ifd, connstr);
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
	(void) hostname;
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

/** @brief host based authentication
  *
  * Host-based authentication is rather a bad idea.  Even the classical xsm(1)
  * does not permit host-hased authentication.
  */
Bool
HostBasedAuthProc(char *hostname)
{
	(void) hostname;
	return False;
}

static char *addAuthFile;
static char *remAuthFile;

#define MAGIC_COOKIE_LEN 16

/** @brief set authentication
  *
  * NOTE: this is a strange way to do this: I don't know why we do not simply
  * use the ICE library (see /usr/include/X11/ICE/ICEutil.h) and write the file
  * directly with correct locking.
  */
Status
SetAuthentication(int count, IceListenObj *listenObjs, IceAuthDataEntry **authDataEntries)
{
	FILE *addfp = NULL;
	FILE *removefp = NULL;
	const char *path;
	mode_t original_umask;
	char command[256] = { 0, };
	int i, j;
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
	if (system(command)) { }
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

void
FreeAuthenticationData(int count, IceAuthDataEntry *authDataEntries)
{
	char command[BUFSIZ];
	int i;

	for (i = 0; i < count * 2; i++) {
		free(authDataEntries[i].network_id);
		free(authDataEntries[i].auth_data);
	}
	free(authDataEntries);
	snprintf(command, sizeof(command), "iceauth source %s", remAuthFile);
	if (system(command)) { }
	remove(remAuthFile);
	free(addAuthFile);
	free(remAuthFile);
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
smpInitSessionManager(void)
{
	char err[256] = { 0, };
	int i;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	initialClients = g_queue_new();
	pendingClients = g_queue_new();
	anywaysClients = g_queue_new();
	restartClients = g_queue_new();
	failureClients = g_queue_new();
	savselfClients = g_queue_new();
	wphase2Clients = g_queue_new();
	interacClients = g_queue_new();
	runningClients = g_queue_new();
	checkptClients = g_queue_new();
	sessionClients = g_queue_new();

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

	manager.state = SMS_Start;
}

void
unlockSession(char *session_name)
{
	(void) session_name;
}

void
endSession(int status)
{
	OPRINTF(1, "SESSION MANAGER EXITING [status = %d]\n", status);

	FreeAuthenticationData(numTransports, authDataEntries);

	if (manager.session_name) {
		unlockSession(manager.session_name);
		free(manager.session_name);
		manager.session_name = NULL;
	}
	free(manager.local.display);
	manager.local.display = NULL;
	free(manager.local.session);
	manager.local.session = NULL;
	free(manager.local.audio);
	manager.local.audio = NULL;
	free(manager.remote.display);
	manager.remote.display = NULL;
	free(manager.remote.session);
	manager.remote.session = NULL;
	free(manager.remote.audio);
	manager.remote.audio = NULL;

}

void
handle_events(void)
{
	/* FIXME */
}

gboolean
on_xfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	(void) chan;
	(void) data;
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
on_ifd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	(void) chan;
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
	(void) signum;
	/* FIXME */
}

void
on_hup_signal(int signum)
{
	(void) signum;
	/* FIXME */
}

void
on_term_signal(int signum)
{
	(void) signum;
	/* FIXME */
}

void
on_quit_signal(int signum)
{
	(void) signum;
	/* FIXME */
}

void
on_usr1_signal(int signum)
{
	(void) signum;
	/* FIXME */
}

void
on_usr2_signal(int signum)
{
	(void) signum;
	/* FIXME */
}

void
on_alrm_signal(int signum)
{
	(void) signum;
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
do_startup(int argc, char *argv[])
{
	static const char *suffix = "/.gtkrc-2.0.xde";
	int xfd;
	GIOChannel *chan;
	gint srce;
	const char *home;
	char *file;
	int len;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

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

	smpInitSessionManager();
}

static void
do_checkpoint(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
do_shutdown(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
do_editor(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
copying(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>\n\
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
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2020, 2021, 2022  Monavacon Limited.\n\
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
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static const char *
show_bool(Bool flag)
{
	if (flag)
		return ("true");
	return ("false");
}


static void
help(int argc, char *argv[])
{
	(void) argc;
	if (!options.output && !options.debug)
		return;
	/* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options]\n\
    %1$s {-h|--help} [options]\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command options:\n\
   [-b, --startup]\n\
        start a new or specified session\n\
    -c, --checkpoint\n\
        check-point a running session\n\
    -q, --shutdown\n\
        shutdown current session\n\
    -e, --edit\n\
        edit the tasks in the session\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
Options:\n\
    -d, --display DISPLAY\n\
        specify the X display, DISPLAY, to use [default: %4$s]\n\
    -s, --screen SCREEN\n\
        specify the screen number, SCREEN, to use [default: %5$d]\n\
    -S, --session SESSION\n\
        specify the session to start [default: %6$s]\n\
    -n, --dry-run\n\
        just print what would be performed [default: %7$s]\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: %2$d]\n\
        this option may be repeated\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: %3$d]\n\
        this option may be repeated\n\
", argv[0]
	, options.debug
	, options.output
	, options.display
	, options.screen
	, options.session
	, show_bool(options.dryrun)
);
	/* *INDENT-ON* */
}

static void
set_defaults(void)
{
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;
	Bool exec_mode = False;

	setlocale(LC_ALL, "");

	set_defaults();

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"display",		required_argument,	NULL,	'd'},
			{"screen",		required_argument,	NULL,	's'},
			{"session",		required_argument,	NULL,	'S'},

			{"startup",		no_argument,		NULL,	'b'},
			{"checkpoint",		no_argument,		NULL,	'c'},
			{"shutdown",		no_argument,		NULL,	'q'},
			{"edit",		no_argument,		NULL,	'e'},

			{"debug",		optional_argument,	NULL,	'D'},
			{"verbose",		optional_argument,	NULL,	'v'},
			{"help",		no_argument,		NULL,	'h'},
			{"version",		no_argument,		NULL,	'V'},
			{"copying",		no_argument,		NULL,	'C'},
			{"?",			no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "d:s:S:bcqeD::v::hVCH?",
				     long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "d:s:S:bcqeDvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1 || exec_mode) {
			DPRINTF(1, "done options processing\n");
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'd':	/* -d, --display DISPLAY */
			setenv("DISPLAY", optarg, True);
			free(options.display);
			options.display = strdup(optarg);
			break;
		case 's':	/* -s, --screen SCREEN */
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			options.screen = val;
			break;
		case 'S':	/* -S, --session SESSION */
			free(options.session);
			options.session = strdup(optarg);
			break;

		case 'b':	/* -b, --startup */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandStartup;
			options.command = CommandStartup;
			break;
		case 'c':	/* -c, --checkpoint */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandCheckpoint;
			options.command = CommandCheckpoint;
			break;
		case 'q':	/* -q, --shutdown */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandShutdown;
			options.command = CommandShutdown;
			break;
		case 'e':	/* -e, --edit */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandEditor;
			options.command = CommandEditor;
			break;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (optarg == NULL) {
				DPRINTF(1, "%s: increasing debug verbosity\n", argv[0]);
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			DPRINTF(1, "%s: setting debug verbosity to %d\n", argv[0], val);
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			if (optarg == NULL) {
				DPRINTF(1, "%s: increasing output verbosity\n", argv[0]);
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			DPRINTF(1, "%s: setting output verbosity to %d\n", argv[0], val);
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			DPRINTF(1, "Setting command to CommandVersion\n");
			command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			DPRINTF(1, "Setting command to CommandCopying\n");
			command = CommandCopying;
			break;
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					EPRINTF("%s: syntax error near '", argv[0]);
					while (optind < argc) {
						fprintf(stderr, "%s", argv[optind++]);
						fprintf(stderr, "%s", (optind < argc) ? " " : "");
					}
					fprintf(stderr, "'\n");
				} else {
					EPRINTF("%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(EXIT_SYNTAXERR);
		}
	}
	DPRINTF(1, "%s: option index = %d\n", argv[0], optind);
	DPRINTF(1, "%s: option count = %d\n", argv[0], argc);
	switch (command) {
	default:
	case CommandDefault:
	case CommandStartup:
		do_startup(argc, argv);
		break;
	case CommandCheckpoint:
		do_checkpoint(argc, argv);
		break;
	case CommandShutdown:
		do_shutdown(argc, argv);
		break;
	case CommandEditor:
		do_editor(argc, argv);
		break;
	case CommandHelp:
		DPRINTF(1, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF(1, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF(1, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=88 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
