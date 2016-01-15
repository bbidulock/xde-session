/*****************************************************************************

 Copyright (c) 2008-2014  Monavacon Limited <http://www.monavacon.com/>
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
#include "smclient.h"

#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

static Bool sent_save_done;

gboolean
on_ifd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceConn ice = (typeof(ice)) data;

		IceProcessMessages(ice, NULL, NULL);
	}
	return G_SOURCE_CONTINUE;
}

static void
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

/** @brief save yourself callback
  *
  * The session manager sends a SaveYourself message to the client either to
  * checkpoint it or so that it can save its state.  The client responds with
  * zero or more calls to SmcSetProperties to update the properties indicating
  * how to restart the client.  When all the properties have been set, the
  * client calls SmcSaveYourselfDone.
  *
  * If #interactStyle is SmInteractStyleNone, the client must not interact with
  * the user while saving state.  If #interactStyle is SmInteractStyleErrors,
  * the client may interact with the user only if an error condition arises.  If
  * interactStyle is SmInteractStyleAny, then the client may interact with the
  * user at a time, the client must call SmcInteractRequest and wait for an
  * Interact message from the session manager.  When the client is done
  * interacting with the user, it calls SmcInteractDone.  The client may only
  * call SmcInteractRequest after it receives a SaveYourself message and before
  * it call SmcSaveYourselfDone.
  *
  * If saveType is SmSaveLocal, the client must update the properties to reflect
  * its current state.  Specifically , it should save enough information to
  * restore the state as seen by the user of this client.  It should not affect
  * the state as seen by other users.  If saveType is SmSaveGlobal, the user
  * wants the client to commit all of its data to permanent, globally accessible
  * storage.  If saveType is SmSaveBoth, the client should do both of these (it
  * should first commit the data to permanent storage before updating its
  * properties).
  *
  * The #shutdown argument specifies whether the system is being shut down.  The
  * interaction is different depending on whether or not shutdown is set.  If
  * not shutting down, the client should save its state and wait for a
  * SaveComplete message.  If shutting down, the client must save state and then
  * prevent interaction until it receives either a Die or a ShutdownCancelled.
  *
  * The #fast argument specifies that the client should save its state as
  * quickly as possible.  For example, if the session manager knows that popwer
  * is about to fail, it would set #fast to True.
  */
void
wmpSaveYourselfCB(SmcConn smcConn, SmPointer clientData, int saveType, Bool shutdown,
		  int interactStyle, Bool fast)
{
	if (!SmcRequestSaveYourselfPhase2(smcConn, wmpSaveYourselfPhase2CB, NULL)) {
		SmcSaveYourselfDone(smcConn, False);
		sent_save_done = True;
	} else
		sent_save_done = False;
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
setInitialProperties(Client *c, SmProp *props[])
{
	/* FIXME !!!! */
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
