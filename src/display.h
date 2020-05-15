/*****************************************************************************

 Copyright (c) 2008-2020  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; version 3 of the License.

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

#ifndef __LOCAL_DISPLAY_H__
#define __LOCAL_DISPLAY_H__

typedef enum {
	FIELD_OFFSET_LAUNCHER,
	FIELD_OFFSET_LAUNCHEE,
	FIELD_OFFSET_SEQUENCE,
	FIELD_OFFSET_ID,
	FIELD_OFFSET_NAME,
	FIELD_OFFSET_ICON,
	FIELD_OFFSET_BIN,
	FIELD_OFFSET_DESCRIPTION,
	FIELD_OFFSET_WMCLASS,
	FIELD_OFFSET_SILENT,
	FIELD_OFFSET_APPLICATION_ID,
	FIELD_OFFSET_DESKTOP,
	FIELD_OFFSET_SCREEN,
	FIELD_OFFSET_MONITOR,
	FIELD_OFFSET_TIMESTAMP,
	FIELD_OFFSET_PID,
	FIELD_OFFSET_HOSTNAME,
	FIELD_OFFSET_COMMAND,
	FIELD_OFFSET_ACTION,
	FIELD_OFFSET_FILE,
	FIELD_OFFSET_URL,
} FieldOffset;

struct fields {
	char *launcher;
	char *launchee;
	char *sequence;
	char *id;
	char *name;
	char *icon;
	char *bin;
	char *description;
	char *wmclass;
	char *silent;
	char *application_id;
	char *desktop;
	char *screen;
	char *monitor;
	char *timestamp;
	char *pid;
	char *hostname;
	char *command;
	char *action;
	char *file;
	char *url;
};

typedef enum {
	StartupNotifyIdle,
	StartupNotifyNew,
	StartupNotifyChanged,
	StartupNotifyComplete,
} StartupNotifyState;

typedef struct _Client Client;

typedef struct _Sequence {
	struct _Sequence *next;
	int refs;
	StartupNotifyState state;
	Client *client;
	Bool changed;
	union {
		char *fields[sizeof(struct fields) / sizeof(char *)];
		struct fields f;
	};
	struct {
		unsigned screen;
		unsigned monitor;
		unsigned desktop;
		unsigned timestamp;
		unsigned silent;
		unsigned pid;
		unsigned sequence;
	} n;
	gint timer;
} Sequence;

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

extern Sequence *sequences;		/* sequences for this display */

typedef struct _Client Client;

typedef struct {
	int screen;			/* screen number */
	Window root;			/* root window of screen */
	Window owner;			/* _XDE_AUTOSTART_S%d selection owner
					   (theirs) */
	Window selwin;			/* _XDE_AUTOSTART_S%d selection window
					   (ours) */
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;		/* set to root when
					   SubstructureRedirect */
	Window stray;			/* _NET_SYSTEM_TRAY_S%d owner */
	Atom icccm_atom;		/* WM_S%d atom for this screen */
	Atom stray_atom;		/* _NET_SYSTEM_TRAY_S%d atom this
					   screen */
	Atom slctn_atom;		/* _XDE_AUTOSTART_S%d atom this screen */
	Client *clients;		/* clients for this screen */
} XdgScreen;

extern XdgScreen *screens;

extern Display *dpy;
extern int nscr;
extern XdgScreen *scr;

typedef struct {
	unsigned long state;
	Window icon_window;
} XWMState;

enum {
	XEMBED_MAPPED = (1 << 0),
};

typedef struct {
	unsigned long version;
	unsigned long flags;
} XEmbedInfo;

struct _Client {
	Client *next;			/* next client in list */
	Sequence *seq;			/* associated startup sequence */
	int screen;			/* screen number */
	Window win;			/* client window */
	Window time_win;		/* client time window */
	Window transient_for;		/* transient for */
	Window leader;			/* client leader window */
	Window group;			/* client group window */
	Bool dockapp;			/* client is a dock app */
	Bool statusicon;		/* client is a status icon */
	Bool breadcrumb;		/* for traversing list */
	Bool managed;			/* managed by window manager */
	Bool listed;			/* listed by window manager */
	Bool new;			/* brand new */
	Time active_time;		/* last time active */
	Time focus_time;		/* last time focused */
	Time user_time;			/* last time used */
	Time map_time;			/* last time window was mapped */
	Time last_time;			/* last time something happened to this window */
	pid_t pid;			/* process id */
	char *startup_id;		/* startup id (property) */
	char **command;			/* command */
	char *name;			/* window name */
	char *hostname;			/* client machine */
	char *client_id;		/* session management id */
	char *role;			/* session management role */
	XClassHint ch;			/* class hint */
	XWMHints *wmh;			/* window manager hints */
	XWMState *wms;			/* window manager state */
	XEmbedInfo *xei;		/* _XEMBED_INFO property */
};

extern Bool init_display(void);
extern void setup_main_loop(int, char *[]);

extern XContext PropertyContext;	/* atom to property context */
extern XContext ClientMessageContext;	/* atom to client message context */
extern XContext ScreenContext;		/* window to screen context */
extern XContext ClientContext;		/* window to client context */
extern XContext MessageContext;		/* window to message context */

extern Atom _XA_KDE_WM_CHANGE_STATE;
extern Atom _XA_MANAGER;
extern Atom _XA_MOTIF_WM_INFO;
extern Atom _XA_NET_ACTIVE_WINDOW;
extern Atom _XA_NET_CLIENT_LIST;
extern Atom _XA_NET_CLIENT_LIST_STACKING;
extern Atom _XA_NET_CLOSE_WINDOW;
extern Atom _XA_NET_CURRENT_DESKTOP;
extern Atom _XA_NET_MOVERESIZE_WINDOW;
extern Atom _XA_NET_REQUEST_FRAME_EXTENTS;
extern Atom _XA_NET_RESTACK_WINDOW;
extern Atom _XA_NET_STARTUP_ID;
extern Atom _XA_NET_STARTUP_INFO;
extern Atom _XA_NET_STARTUP_INFO_BEGIN;
extern Atom _XA_NET_SUPPORTED;
extern Atom _XA_NET_SUPPORTING_WM_CHECK;
extern Atom _XA_NET_VIRTUAL_ROOTS;
extern Atom _XA_NET_VISIBLE_DESKTOPS;
extern Atom _XA_NET_WM_ALLOWED_ACTIONS;
extern Atom _XA_NET_WM_FULLSCREEN_MONITORS;
extern Atom _XA_NET_WM_ICON_GEOMETRY;
extern Atom _XA_NET_WM_ICON_NAME;
extern Atom _XA_NET_WM_MOVERESIZE;
extern Atom _XA_NET_WM_NAME;
extern Atom _XA_NET_WM_PID;
extern Atom _XA_NET_WM_STATE;
extern Atom _XA_NET_WM_STATE_FOCUSED;
extern Atom _XA_NET_WM_STATE_HIDDEN;
extern Atom _XA_NET_WM_USER_TIME;
extern Atom _XA_NET_WM_USER_TIME_WINDOW;
extern Atom _XA_NET_WM_VISIBLE_ICON_NAME;
extern Atom _XA_NET_WM_VISIBLE_NAME;
extern Atom _XA_SM_CLIENT_ID;
extern Atom _XA__SWM_VROOT;
extern Atom _XA_TIMESTAMP_PROP;
extern Atom _XA_WIN_APP_STATE;
extern Atom _XA_WIN_CLIENT_LIST;
extern Atom _XA_WIN_CLIENT_MOVING;
extern Atom _XA_WINDOWMAKER_NOTICEBOARD;
extern Atom _XA_WIN_FOCUS;
extern Atom _XA_WIN_HINTS;
extern Atom _XA_WIN_LAYER;
extern Atom _XA_WIN_PROTOCOLS;
extern Atom _XA_WIN_STATE;
extern Atom _XA_WIN_SUPPORTING_WM_CHECK;
extern Atom _XA_WIN_WORKSPACE;
extern Atom _XA_WM_CHANGE_STATE;
extern Atom _XA_WM_CLIENT_LEADER;
extern Atom _XA_WM_DELETE_WINDOW;
extern Atom _XA_WM_PROTOCOLS;
extern Atom _XA_WM_SAVE_YOURSELF;
extern Atom _XA_WM_STATE;
extern Atom _XA_WM_WINDOW_ROLE;
extern Atom _XA_XEMBED;
extern Atom _XA_XEMBED_INFO;
extern Atom _XA_UTF8_STRING;

typedef void (*pc_handler_t) (XPropertyEvent *, Client *);
typedef void (*cm_handler_t) (XClientMessageEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	pc_handler_t pc_handler;
	cm_handler_t cm_handler;
	Atom value;
};

extern struct atoms atoms[];

enum {
	XEMBED_EMBEDDED_NOTIFY,
	XEMBED_WINDOW_ACTIVATE,
	XEMBED_WINDOW_DEACTIVATE,
	XEMBED_REQUEST_FOCUS,
	XEMBED_FOCUS_IN,
	XEMBED_FOCUS_OUT,
	XEMBED_FOCUS_NEXT,
	XEMBED_FOCUS_PREV,
	XEMBED_GRAB_KEY,
	XEMBED_UNGRAB_KEY,
	XEMBED_MODALITY_ON,
	XEMBED_MODALITY_OFF,
	XEMBED_REGISTER_ACCELERATOR,
	XEMBED_UNREGISTER_ACCELERATOR,
	XEMBED_ACTIVATE_ACCELERATOR,
};

/* detail for XEMBED_FOCUS_IN: */
enum {
	XEMBED_FOCUS_CURRENT,
	XEMBED_FOCUS_FIRST,
	XEMBED_FOCUS_LAST,
};

enum {
	XEMBED_MODIFIER_SHIFT = (1 << 0),
	XEMBED_MODIFIER_CONTROL = (1 << 1),
	XEMBED_MODIFIER_ALT = (1 << 2),
	XEMBED_MODIFIER_SUPER = (1 << 3),
	XEMBED_MODIFIER_HYPER = (1 << 4),
};

enum {
	XEMBED_ACCELERATOR_OVERLOADED = (1 << 0),
};

#endif				/* __LOCAL_DISPLAY_H__ */
