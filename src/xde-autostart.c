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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "xde-autostart.h"

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

typedef enum _LogoSide {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_BOTTOM,
} LogoSide;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *display;
	char *desktop;
	char *session;
	char **execute;
	int commands;
	char *file;
	Bool autostart;
	Bool wait;
	unsigned int pause;
	char *banner;
	Bool splash;
	unsigned int guard;
	unsigned int delay;
	char *charset;
	char *language;
	LogoSide side;
	char *icon_theme;
	char *gtk2_theme;
	Bool usexde;
	Bool foreground;
	char *client_id;
	char **desktops;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.display = NULL,
	.desktop = NULL,
	.session = NULL,
	.execute = NULL,
	.commands = 0,
	.file = NULL,
	.autostart = True,
	.wait = True,
	.pause = 2,
	.banner = NULL,
	.splash = True,
	.guard = 200,
	.delay = 0,
	.charset = NULL,
	.language = NULL,
	.side = LOGO_SIDE_TOP,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.usexde = False,
	.foreground = False,
	.client_id = NULL,
	.desktops = NULL,
};

/*
 * The run state of the program.  There are several states as follows:
 *
 * RunStateStartup - the program is starting up and initializing.  On completion
 *	check for a window manager and if not present start guard timer and move
 *	to RunStatePhase1.
 *
 * RunStatePhase1 - waiting for a window manager to appear.  Upon expiry of the
 *	guard timer or appearance of a window manager move to RunStatePhase2.
 *	Set the desktop session to the window manager name when one appears.
 *	When none appears, fall back to command-line options of default for
 *	desktop session.  Set DESKTOP_SESSION and XDG_CURRENT_DESKTOP
 *	environment variables.  Start spawned thread to begin reading autostart
 *	database.
 *
 * RunStatePhase2 - post window-manager pause state.  We detect window manager
 *	startup quite quickly, so there is an optional pause after detection
 *	before proceeding.  This state waits for the pause timer to expire
 *	before proceeding to RunStatePhase3.
 *
 * RunStatePhase3 - performing execs.  Read the startup file and perform any
 *	commands, also from command-line arguments (--exec flag).  Still apply
 *	whatever startup detection is possible (appearance of windows with
 *	associated PIDs).  We might be able to get xde-launch to do the job for
 *	us anyway because it can perform startup notification even for basic
 *	commands.  Note that these commands represent independent tasks, not
 *	just a list of simple commands.  It should not really matter in what
 *	order they are executed.  After the last guard/delay timers have
 *	expired, move to RunStatePhase4.
 *
 * RunStatePhase4 - performing autostart.  Read the autostart desktop entry
 *      database.  (Note we can overlap reading of the desktop entries with the
 *      previous phases by spawning a thread and joining it here instead of
 *      serializing.)  Execute eacn autostart task in turn, following command
 *      line options for guard, delay, possibly serialization or parallel start;
 *      once done, move to RunStatePhase5.  Each task has its own state.
 *
 * RunStatePhase5 - awaiting startup completion.  Await the startup completion
 *	or failure of each launched task.  We should provide desktop
 *	notification for tasks that fail or fail to signal competion.  Once all
 *	tasks have finalized, move to RunStateMonitor.
 *
 * RunStateMonitor - monitoring.  Monitor child processes checking whether we
 *	should restart tasks upon failure.  Continue to provide notification of
 *	failures and provide restart options to user.  We can checkpoint with a
 *	session manager when entering this phase.  If the window manager changes
 *	during this phase, we may wish to tear down any children that had an
 *	OnlyShowIn  or NotShowIn that no longer applies, or start a task for one
 *	that newly applies.  We might also perform similar changes on
 *	notification of changes to the autostart directories.  For window
 *	manager changes, we may choose to tear down all autostart tasks and
 *	start again from scratch: this is because the whole autostart process
 *	largely expects tasks to execute _after_ the window manager appears.  So
 *	a window manager change is more like logging out and logging back in
 *	again, which is ok.  So, for a window manager change, tear down all
 *	tasks and go back to Phase 2.
 *
 *
 */
typedef enum {
	RunStateStartup,		/* starting up */
	RunStatePhase1,			/* waiting for window manager */
	RunStatePhase2,			/* post-window-manager pause */
	RunStatePhase3,			/* running execs */
	RunStatePhase4,			/* running autostarts */
	RunStatePhase5,			/* waiting for startup completion */
	RunStateMonitor,		/* monitoring window manager */
} XdeRunState;

XdeRunState runstate = RunStateStartup;

typedef struct {
	XdeRunState state;
	guint timer;

} XdeContext;

XdeContext ctx;

typedef enum {
	TaskStateIdle,			/* task is not running yet */
	TaskStateStartup,		/* running but in startup notification */
	TaskStateRunning,		/* completed startup notification */
	TaskStateSignaled,		/* exited on a signal */
	TaskStateFailed,		/* exited with non-zero exit status */
	TaskStateDone,			/* exited with zero exit status */
} TaskState;

typedef struct {
	TaskState state;		/* state of the task */
	GtkWidget *button;		/* button corresponding to this task */
	guint blink;			/* timer source for blinking button */
	const char *appid;		/* application id */
	GKeyFile *entry;		/* key file entry */
	gboolean runnable;		/* whether the entry is runnable */
	const char *cmd;		/* command to execute */
} TaskContext;

GHashTable *autostarts;

GtkWidget *splash;
GtkWidget *table;

typedef struct {
	int cols;			/* columns in the table */
	int rows;			/* rows in the table */
	int col;			/* column index of the next free cell */
	int row;			/* row index of the next free cell */
} TableContext;

static const char *StartupNotifyFields[] = {
	"LAUNCHER",
	"LAUNCHEE",
	"SEQUENCE",
	"ID",
	"NAME",
	"ICON",
	"BIN",
	"DESCRIPTION",
	"WMCLASS",
	"SILENT",
	"APPLICATION_ID",
	"DESKTOP",
	"SCREEN",
	"MONITOR",
	"TIMESTAMP",
	"PID",
	"HOSTNAME",
	"COMMAND",
	"ACTION",
	"FILE",
	"URL",
	NULL
};

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

Sequence *sequences;			/* sequences for this display */

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

XdgScreen *screens;

Display *dpy = NULL;
int nscr;
XdgScreen *scr;

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
	Time last_time;			/* last time something happened to this 
					   window */
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

Time current_time = CurrentTime;
Time last_user_time = CurrentTime;

Atom _XA_KDE_WM_CHANGE_STATE;
Atom _XA_MANAGER;
Atom _XA_MOTIF_WM_INFO;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_CLIENT_LIST_STACKING;
Atom _XA_NET_CLOSE_WINDOW;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_MOVERESIZE_WINDOW;
Atom _XA_NET_REQUEST_FRAME_EXTENTS;
Atom _XA_NET_RESTACK_WINDOW;
Atom _XA_NET_STARTUP_ID;
Atom _XA_NET_STARTUP_INFO;
Atom _XA_NET_STARTUP_INFO_BEGIN;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_VIRTUAL_ROOTS;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_NET_WM_ALLOWED_ACTIONS;
Atom _XA_NET_WM_FULLSCREEN_MONITORS;
Atom _XA_NET_WM_ICON_GEOMETRY;
Atom _XA_NET_WM_ICON_NAME;
Atom _XA_NET_WM_MOVERESIZE;
Atom _XA_NET_WM_NAME;
Atom _XA_NET_WM_PID;
Atom _XA_NET_WM_STATE;
Atom _XA_NET_WM_STATE_FOCUSED;
Atom _XA_NET_WM_STATE_HIDDEN;
Atom _XA_NET_WM_USER_TIME;
Atom _XA_NET_WM_USER_TIME_WINDOW;
Atom _XA_NET_WM_VISIBLE_ICON_NAME;
Atom _XA_NET_WM_VISIBLE_NAME;
Atom _XA_SM_CLIENT_ID;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_APP_STATE;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WIN_CLIENT_MOVING;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_FOCUS;
Atom _XA_WIN_HINTS;
Atom _XA_WIN_LAYER;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_STATE;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_CHANGE_STATE;
Atom _XA_WM_CLIENT_LEADER;
Atom _XA_WM_DELETE_WINDOW;
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_SAVE_YOURSELF;
Atom _XA_WM_STATE;
Atom _XA_WM_WINDOW_ROLE;
Atom _XA_XEMBED;
Atom _XA_XEMBED_INFO;
Atom _XA_UTF8_STRING;

static void pc_handle_MOTIF_WM_INFO(XPropertyEvent *, Client *);
static void pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *, Client *);
static void pc_handle_NET_STARTUP_ID(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTED(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_PID(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *, Client *);
static void pc_handle_SM_CLIENT_ID(XPropertyEvent *, Client *);
static void pc_handle_TIMESTAMP_PROP(XPropertyEvent *, Client *);
static void pc_handle_WIN_APP_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *, Client *);
static void pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *, Client *);
static void pc_handle_WIN_FOCUS(XPropertyEvent *, Client *);
static void pc_handle_WIN_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WIN_LAYER(XPropertyEvent *, Client *);
static void pc_handle_WIN_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WIN_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_WIN_WORKSPACE(XPropertyEvent *, Client *);
static void pc_handle_WM_CLASS(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_LEADER(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *, Client *);
static void pc_handle_WM_COMMAND(XPropertyEvent *, Client *);
static void pc_handle_WM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_SIZE(XPropertyEvent *, Client *);
static void pc_handle_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_NORMAL_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WM_SIZE_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *, Client *);
static void pc_handle_WM_WINDOW_ROLE(XPropertyEvent *, Client *);
static void pc_handle_WM_ZOOM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_XEMBED_INFO(XPropertyEvent *, Client *);

static void cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_MANAGER(XClientMessageEvent *, Client *);
static void cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *, Client *);
static void cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_LAYER(XClientMessageEvent *, Client *);
static void cm_handle_WIN_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_WORKSPACE(XClientMessageEvent *, Client *);
static void cm_handle_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WM_PROTOCOLS(XClientMessageEvent *, Client *);
static void cm_handle_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_XEMBED(XClientMessageEvent *, Client *);

typedef void (*pc_handler_t) (XPropertyEvent *, Client *);
typedef void (*cm_handler_t) (XClientMessageEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	pc_handler_t pc_handler;
	cm_handler_t cm_handler;
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,	NULL,					&cm_handle_KDE_WM_CHANGE_STATE,		None			},
	{ "MANAGER",				&_XA_MANAGER,			NULL,					&cm_handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,		&pc_handle_MOTIF_WM_INFO,		NULL,					None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,		&pc_handle_NET_ACTIVE_WINDOW,		&cm_handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,		&pc_handle_NET_CLIENT_LIST,		NULL,					None			},
	{ "_NET_CLIENT_LIST_STACKING",		&_XA_NET_CLIENT_LIST_STACKING,	&pc_handle_NET_CLIENT_LIST_STACKING,	NULL,					None			},
	{ "_NET_CLOSE_WINDOW",			&_XA_NET_CLOSE_WINDOW,		NULL,					&cm_handle_NET_CLOSE_WINDOW,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,	NULL,					NULL,					None			},
	{ "_NET_MOVERESIZE_WINDOW",		&_XA_NET_MOVERESIZE_WINDOW,	NULL,					&cm_handle_NET_MOVERESIZE_WINDOW,	None			},
	{ "_NET_REQUEST_FRAME_EXTENTS",		&_XA_NET_REQUEST_FRAME_EXTENTS,	NULL,					&cm_handle_NET_REQUEST_FRAME_EXTENTS,	None			},
	{ "_NET_RESTACK_WINDOW",		&_XA_NET_RESTACK_WINDOW,	NULL,					&cm_handle_NET_RESTACK_WINDOW,		None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,	NULL,					&cm_handle_NET_STARTUP_INFO_BEGIN,	None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,		NULL,					&cm_handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,		&pc_handle_NET_SUPPORTED,		NULL,					None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,	&pc_handle_NET_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,		NULL,					NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,	NULL,					NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_WM_FULLSCREEN_MONITORS",	&_XA_NET_WM_FULLSCREEN_MONITORS,&pc_handle_NET_WM_FULLSCREEN_MONITORS,	&cm_handle_NET_WM_FULLSCREEN_MONITORS,	None			},
	{ "_NET_WM_ICON_GEOMETRY",		&_XA_NET_WM_ICON_GEOMETRY,	&pc_handle_NET_WM_ICON_GEOMETRY,	NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ "_NET_WM_MOVERESIZE",			&_XA_NET_WM_MOVERESIZE,		NULL,					&cm_handle_NET_WM_MOVERESIZE,		None			},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE_FOCUSED",		&_XA_NET_WM_STATE_FOCUSED,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE_HIDDEN",		&_XA_NET_WM_STATE_HIDDEN,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "_NET_WM_VISIBLE_ICON_NAME",		&_XA_NET_WM_VISIBLE_ICON_NAME,	&pc_handle_NET_WM_VISIBLE_ICON_NAME,	NULL,					None			},
	{ "_NET_WM_VISIBLE_NAME",		&_XA_NET_WM_VISIBLE_NAME,	&pc_handle_NET_WM_VISIBLE_NAME,		NULL,					None			},
	{ "SM_CLIENT_ID",			&_XA_SM_CLIENT_ID,		&pc_handle_SM_CLIENT_ID,		NULL,					None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,		NULL,					NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,		&pc_handle_TIMESTAMP_PROP,		NULL,					None			},
	{ "_WIN_APP_STATE",			&_XA_WIN_APP_STATE,		&pc_handle_WIN_APP_STATE,		NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,		&pc_handle_WIN_CLIENT_LIST,		NULL,					None			},
	{ "_WIN_CLIENT_MOVING",			&_XA_WIN_CLIENT_MOVING,		&pc_handle_WIN_CLIENT_MOVING,		NULL,					None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,	&pc_handle_WINDOWMAKER_NOTICEBOARD,	NULL,					None			},
	{ "_WIN_FOCUS",				&_XA_WIN_FOCUS,			&pc_handle_WIN_FOCUS,			NULL,					None			},
	{ "_WIN_HINTS",				&_XA_WIN_HINTS,			&pc_handle_WIN_HINTS,			NULL,					None			},
	{ "_WIN_LAYER",				&_XA_WIN_LAYER,			&pc_handle_WIN_LAYER,			&cm_handle_WIN_LAYER,			None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,		&pc_handle_WIN_PROTOCOLS,		NULL,					None			},
	{ "_WIN_STATE",				&_XA_WIN_STATE,			&pc_handle_WIN_STATE,			&cm_handle_WIN_STATE,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,	&pc_handle_WIN_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,		&pc_handle_WIN_WORKSPACE,		&cm_handle_WIN_WORKSPACE,		None			},
	{ "WM_CHANGE_STATE",			&_XA_WM_CHANGE_STATE,		NULL,					&cm_handle_WM_CHANGE_STATE,		None			},
	{ "WM_CLASS",				NULL,				&pc_handle_WM_CLASS,			NULL,					XA_WM_CLASS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_DELETE_WINDOW",			&_XA_WM_DELETE_WINDOW,		NULL,					NULL,					None			},
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_ICON_NAME",			NULL,				&pc_handle_WM_ICON_NAME,		NULL,					XA_WM_ICON_NAME		},
	{ "WM_ICON_SIZE",			NULL,				&pc_handle_WM_ICON_SIZE,		NULL,					XA_WM_ICON_SIZE		},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "WM_NORMAL_HINTS",			NULL,				&pc_handle_WM_NORMAL_HINTS,		NULL,					XA_WM_NORMAL_HINTS	},
	{ "WM_PROTOCOLS",			&_XA_WM_PROTOCOLS,		&pc_handle_WM_PROTOCOLS,		&cm_handle_WM_PROTOCOLS,		None			},
	{ "WM_SAVE_YOURSELF",			&_XA_WM_SAVE_YOURSELF,		NULL,					NULL,					None			},
	{ "WM_SIZE_HINTS",			NULL,				&pc_handle_WM_SIZE_HINTS,		NULL,					XA_WM_SIZE_HINTS	},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "WM_TRANSIENT_FOR",			NULL,				&pc_handle_WM_TRANSIENT_FOR,		NULL,					XA_WM_TRANSIENT_FOR	},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "WM_ZOOM_HINTS",			NULL,				&pc_handle_WM_ZOOM_HINTS,		NULL,					XA_WM_ZOOM_HINTS	},
	{ "_XEMBED_INFO",			&_XA_XEMBED_INFO,		&pc_handle_XEMBED_INFO,			NULL,					None			},
	{ "_XEMBED",				&_XA_XEMBED,			NULL,					&cm_handle_XEMBED,			None			},
	{ "UTF8_STRING",			&_XA_UTF8_STRING,		NULL,					NULL,					None			},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

/* These are listed in the order that we want them evaluated. */

struct atoms wmprops[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

XContext PropertyContext;		/* atom to property handler context */
XContext ClientMessageContext;		/* atom to client message handler
					   context */
XContext ScreenContext;			/* window to screen context */
XContext ClientContext;			/* window to client context */
XContext MessageContext;		/* window to message context */

static void
intern_atoms()
{
	int i, j, n;
	char **atom_names;
	Atom *atom_values;
	struct atoms *atom;

	for (i = 0, n = 0; atoms[i].name; i++)
		if (atoms[i].atom)
			n++;
	atom_names = calloc(n + 1, sizeof(*atom_names));
	atom_values = calloc(n + 1, sizeof(*atom_values));
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			atom_names[j++] = atoms[i].name;
	XInternAtoms(dpy, atom_names, n, False, atom_values);
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			*atoms[i].atom = atoms[i].value = atom_values[j++];
	free(atom_names);
	free(atom_values);
	for (atom = wmprops; atom->name; atom++)
		if (atom->atom)
			atom->value = *(atom->atom);
	for (atom = atoms; atom->name; atom++) {
		if (atom->pc_handler)
			XSaveContext(dpy, atom->value, PropertyContext,
				     (XPointer) atom->pc_handler);
		if (atom->cm_handler)
			XSaveContext(dpy, atom->value, ClientMessageContext,
				     (XPointer) atom->cm_handler);
	}
}

static int
handler(Display *display, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, NAME, num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

Bool
init_display(void)
{
	if (!dpy) {
		int s;
		char sel[64] = { 0, };

		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		XSetErrorHandler(handler);

		PropertyContext = XUniqueContext();
		ClientMessageContext = XUniqueContext();
		ScreenContext = XUniqueContext();
		ClientContext = XUniqueContext();
		MessageContext = XUniqueContext();

		intern_atoms();

		nscr = ScreenCount(dpy);
		if (!(screens = calloc(nscr, sizeof(*screens)))) {
			EPRINTF("no memory\n");
			exit(EXIT_FAILURE);
		}
		for (s = 0, scr = screens; s < nscr; s++, scr++) {
			scr->screen = s;
			scr->root = RootWindow(dpy, s);
			XSaveContext(dpy, scr->root, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, scr->root,
				     VisibilityChangeMask | StructureNotifyMask |
				     SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask);
			snprintf(sel, sizeof(sel), "WM_S%d", s);
			scr->icccm_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_SYSTEM_TRAY_S%d", s);
			scr->stray_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_XDE_AUTOSTART_S%d", s);
			scr->slctn_atom = XInternAtom(dpy, sel, False);
		}
		s = DefaultScreen(dpy);
		scr = screens + s;
	}
	return (dpy ? True : False);
}

static char *
get_text(Window win, Atom prop)
{
	XTextProperty tp = { NULL, };

	XGetTextProperty(dpy, win, &tp, prop);
	if (tp.value) {
		tp.value[tp.nitems + 1] = '\0';
		return (char *) tp.value;
	}
	return NULL;
}

static long *
get_cardinals(Window win, Atom prop, Atom type, long *n)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	long *data = NULL;

      try_harder:
	if (XGetWindowProperty
	    (dpy, win, prop, 0L, num, False, type, &real, &format, &nitems, &after,
	     (unsigned char **) &data) == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			goto try_harder;
		}
		if ((*n = nitems) > 0)
			return data;
		if (data)
			XFree(data);
	} else
		*n = -1;
	return NULL;
}

static Bool
get_cardinal(Window win, Atom prop, Atom type, long *card_ret)
{
	Bool result = False;
	long *data, n;

	if ((data = get_cardinals(win, prop, type, &n)) && n > 0) {
		*card_ret = data[0];
		result = True;
	}
	if (data)
		XFree(data);
	return result;
}

static Window *
get_windows(Window win, Atom prop, Atom type, long *n)
{
	return (Window *) get_cardinals(win, prop, type, n);
}

static Bool
get_window(Window win, Atom prop, Atom type, Window *win_ret)
{
	return get_cardinal(win, prop, type, (long *) win_ret);
}

Time *
get_times(Window win, Atom prop, Atom type, long *n)
{
	return (Time *) get_cardinals(win, prop, type, n);
}

static Bool
get_time(Window win, Atom prop, Atom type, Time *time_ret)
{
	return get_cardinal(win, prop, type, (long *) time_ret);
}

static Atom *
get_atoms(Window win, Atom prop, Atom type, long *n)
{
	return (Atom *) get_cardinals(win, prop, type, n);
}

Bool
get_atom(Window win, Atom prop, Atom type, Atom *atom_ret)
{
	return get_cardinal(win, prop, type, (long *) atom_ret);
}

XWMState *
XGetWMState(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XWMState *wms = NULL;

	if (XGetWindowProperty
	    (d, win, _XA_WM_STATE, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &wms) == Success && format != 0) {
		if (format != 32 || nitems < 2) {
			if (wms) {
				XFree(wms);
				return NULL;
			}
		}
		return wms;
	}
	return NULL;

}

XEmbedInfo *
XGetEmbedInfo(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XEmbedInfo *xei = NULL;

	if (XGetWindowProperty
	    (d, win, _XA_XEMBED_INFO, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &xei) == Success && format != 0) {
		if (format != 32 || nitems < 2) {
			if (xei) {
				XFree(xei);
				return NULL;
			}
		}
		return xei;
	}
	return NULL;
}

Bool
latertime(Time last, Time time)
{
	if (time == CurrentTime)
		return False;
	if (last == CurrentTime || (int) time - (int) last > 0)
		return True;
	return False;
}

void
pushtime(Time *last, Time time)
{
	if (latertime(*last, time))
		*last = time;
}

void
pulltime(Time *last, Time time)
{
	if (!latertime(*last, time))
		*last = time;
}

/** @brief Check for recursive window properties
  * @param atom - property name
  * @param type - property type
  * @return Window - the recursive window property or None
  */
Window
check_recursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check;

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
			XFree(data);
			data = NULL;
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty
		    (dpy, check, atom, 0L, 1L, False, type, &real, &format, &nitems,
		     &after, (unsigned char **) &data) == Success && format != 0) {
			if (nitems > 0) {
				if (check != (Window) data[0]) {
					XFree(data);
					return None;
				}
			} else {
				if (data)
					XFree(data);
				return None;
			}
			XFree(data);
		} else
			return None;
	} else
		return None;
	return check;
}

/** @brief Check for non-recusive window properties (that should be recursive).
  * @param atom - property name
  * @param atom - property type
  * @return Window - the recursive window property or None
  */
Window
check_nonrecursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check = None;

	OPRINTF("non-recursive check for atom 0x%lx\n", atom);

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
		}
		if (data)
			XFree(data);
	}
	return check;
}

/** @brief Check if an atom is in a supported atom list.
  * @param atom - list name
  * @param atom - element name
  * @return Bool - true if atom is in list
  */
static Bool
check_supported(Atom protocols, Atom supported)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	unsigned long *data = NULL;
	Bool result = False;

	OPRINTF("check for non-compliant NetWM\n");

      try_harder:
	if (XGetWindowProperty(dpy, scr->root, protocols, 0L, num, False, XA_ATOM, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success && format) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			data = NULL;
			goto try_harder;
		}
		if (nitems > 0) {
			unsigned long i;
			Atom *atoms;

			atoms = (Atom *) data;
			for (i = 0; i < nitems; i++) {
				if (atoms[i] == supported) {
					result = True;
					break;
				}
			}
		}
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	return result;
}

/** @brief Check for a non-compliant EWMH/NetWM window manager.
  *
  * There are quite a few window managers that implement part of the EWMH/NetWM
  * specification but do not fill out _NET_SUPPORTING_WM_CHECK.  This is a big
  * annoyance.  One way to test this is whether there is a _NET_SUPPORTED on the
  * root window that does not include _NET_SUPPORTING_WM_CHECK in its atom list.
  *
  * The only window manager I know of that placed _NET_SUPPORTING_WM_CHECK in
  * the list and did not set the property on the root window was 2bwm, but is
  * has now been fixed.
  *
  * There are others that provide _NET_SUPPORTING_WM_CHECK on the root window
  * but fail to set it recursively.  When _NET_SUPPORTING_WM_CHECK is reported
  * as supported, relax the check to a non-recursive check.  (Case in point is
  * echinus(1)).
  */
static Window
check_netwm_supported()
{
	if (check_supported(_XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

/** @brief Check for an EWMH/NetWM compliant (sorta) window manager.
  */
static Window
check_netwm()
{
	int i = 0;

	do {
		scr->netwm_check = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
	} while (i++ < 2 && !scr->netwm_check);

	if (scr->netwm_check && scr->netwm_check != scr->root) {
		XSaveContext(dpy, scr->netwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->netwm_check, PropertyChangeMask | StructureNotifyMask);
	} else
		scr->netwm_check = check_netwm_supported();

	return scr->netwm_check;
}

/** @brief Check for a non-compliant GNOME/WinWM window manager.
  *
  * There are quite a few window managers that implement part of the GNOME/WinWM
  * specification but do not fill in the _WIN_SUPPORTING_WM_CHECK.  This is
  * another big annoyance.  One way to test this is whether there is a
  * _WIN_PROTOCOLS on the root window that does not include
  * _WIN_SUPPORTING_WM_CHECK in its list of atoms.
  */
static Window
check_winwm_supported()
{
	if (check_supported(_XA_WIN_PROTOCOLS, _XA_WIN_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

/** @brief Check for a GNOME1/WMH/WinWM compliant window manager.
  */
static Window
check_winwm()
{
	int i = 0;

	do {
		scr->winwm_check = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
	} while (i++ < 2 && !scr->winwm_check);

	if (scr->winwm_check && scr->winwm_check != scr->root) {
		XSaveContext(dpy, scr->winwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->winwm_check, PropertyChangeMask | StructureNotifyMask);
	} else
		scr->winwm_check = check_winwm_supported();

	return scr->winwm_check;
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker()
{
	int i = 0;

	do {
		scr->maker_check = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW);
	} while (i++ < 2 && !scr->maker_check);

	if (scr->maker_check && scr->maker_check != scr->root) {
		XSaveContext(dpy, scr->maker_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->maker_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->maker_check;
}

/** @brief Check for an OSF/Motif compliant window manager.
  */
static Window
check_motif()
{
	int i = 0;
	long *data, n = 0;

	do {
		data = get_cardinals(scr->root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n);
	} while (i++ < 2 && !data);

	if (data && n >= 2)
		scr->motif_check = data[1];
	if (scr->motif_check && scr->motif_check != scr->root) {
		XSaveContext(dpy, scr->motif_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->motif_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->motif_check;
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	scr->icccm_check = XGetSelectionOwner(dpy, scr->icccm_atom);

	if (scr->icccm_check && scr->icccm_check != scr->root) {
		XSaveContext(dpy, scr->icccm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->icccm_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->icccm_check;
}

/** @brief Check whether an ICCCM window manager is present.
  *
  * This pretty much assumes that any ICCCM window manager will select for
  * SubstructureRedirectMask on the root window.
  */
static Window
check_redir()
{
	XWindowAttributes wa;

	OPRINTF("checking direction for screen %d\n", scr->screen);

	scr->redir_check = None;
	if (XGetWindowAttributes(dpy, scr->root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			scr->redir_check = scr->root;
	return scr->redir_check;
}

/** @brief Find window manager and compliance for the current screen.
  */
static Bool
check_window_manager()
{
	Bool have_wm = False;

	OPRINTF("checking wm compliance for screen %d\n", scr->screen);

	OPRINTF("checking redirection\n");
	if (check_redir()) {
		have_wm = True;
		OPRINTF("redirection on window 0x%lx\n", scr->redir_check);
	}
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm()) {
		have_wm = True;
		OPRINTF("ICCCM 2.0 window 0x%lx\n", scr->icccm_check);
	}
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif()) {
		have_wm = True;
		OPRINTF("OSF/Motif window 0x%lx\n", scr->motif_check);
	}
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker()) {
		have_wm = True;
		OPRINTF("WindowMaker window 0x%lx\n", scr->maker_check);
	}
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm()) {
		have_wm = True;
		OPRINTF("GNOME/WMH window 0x%lx\n", scr->winwm_check);
	}
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm()) {
		have_wm = True;
		OPRINTF("NetWM/EWMH window 0x%lx\n", scr->netwm_check);
	}

	return have_wm;
}

static void
handle_wmchange()
{
	if (!check_window_manager())
		check_window_manager();
}

static void
pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_MOTIF_WM_INFO(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

Bool
set_screen_of_root(Window sroot)
{
	int s;

	for (s = 0; s < nscr; s++) {
		if (screens[s].root == sroot) {
			scr = screens + s;
			return True;
		}
	}
	EPRINTF("Could not find screen for root 0x%lx!\n", sroot);
	return False;
}

Bool
find_window_screen(Window w)
{
	Window wroot, dw, *dwp;
	unsigned int du;

	if (!XQueryTree(dpy, w, &wroot, &dw, &dwp, &du))
		return False;
	if (dwp)
		XFree(dwp);

	return set_screen_of_root(wroot);
}

Bool
find_screen(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ScreenContext, (XPointer *) &scr))
		return True;
	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = screens + c->screen;
		return True;
	}
	return find_window_screen(w);
}

Client *
find_client(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = screens + c->screen;
		return (c);
	}
	find_screen(w);
	return (c);
}

void
apply_quotes(char **str, char *q)
{
	char *p;

	for (p = *str; *p; p++) ;
	while (*q) {
		if (*q == ' ' || *q == '"' || *q == '\\')
			*p++ = '\\';
		*p++ = *q++;
	}
	*p = '\0';
	*str = p;
}

Bool running;

void
send_msg(char *msg)
{
	XEvent xev;
	int l;
	char *p;

	DPRINTF("Message to 0x%lx is: '%s'\n", scr->root, msg);

	xev.type = ClientMessage;
	xev.xclient.message_type = _XA_NET_STARTUP_INFO_BEGIN;
	xev.xclient.display = dpy;
	xev.xclient.window = scr->selwin;
	xev.xclient.format = 8;

	l = strlen((p = msg)) + 1;

	while (l > 0) {
		strncpy(xev.xclient.data.b, p, 20);
		p += 20;
		l -= 20;
		/* just PropertyChange mask in the spec doesn't work :( */
		if (!XSendEvent(dpy, scr->root, False, StructureNotifyMask |
				SubstructureNotifyMask | SubstructureRedirectMask |
				PropertyChangeMask, &xev))
			EPRINTF("XSendEvent: failed!\n");
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}
}

struct {
	char *label;
	FieldOffset offset;
} labels[] = {
	/* *INDENT-OFF* */
	{ " NAME=",		FIELD_OFFSET_NAME		},
	{ " ICON=",		FIELD_OFFSET_ICON		},
	{ " BIN=",		FIELD_OFFSET_BIN		},
	{ " DESCRIPTION=",	FIELD_OFFSET_DESCRIPTION	},
	{ " WMCLASS=",		FIELD_OFFSET_WMCLASS		},
	{ " SILENT=",		FIELD_OFFSET_SILENT		},
	{ " APPLICATION_ID=",	FIELD_OFFSET_APPLICATION_ID	},
	{ " DESKTOP=",		FIELD_OFFSET_DESKTOP		},
	{ " SCREEN=",		FIELD_OFFSET_SCREEN		},
	{ " MONITOR=",		FIELD_OFFSET_MONITOR		},
	{ " TIMESTAMP=",	FIELD_OFFSET_TIMESTAMP		},
	{ " PID=",		FIELD_OFFSET_PID		},
	{ " HOSTNAME=",		FIELD_OFFSET_HOSTNAME		},
	{ " COMMAND=",		FIELD_OFFSET_COMMAND		},
        { " ACTION=",           FIELD_OFFSET_ACTION		},
	{ NULL,			FIELD_OFFSET_ID			}
	/* *INDENT-ON* */
};

void
add_field(Sequence * seq, char **p, char *label, FieldOffset offset)
{
	char *value;

	if ((value = seq->fields[offset])) {
		strcat(*p, label);
		apply_quotes(p, value);
	}
}

void
add_fields(Sequence * seq, char *msg)
{
	int i;

	for (i = 0; labels[i].label; i++)
		add_field(seq, &msg, labels[i].label, labels[i].offset);
}

/** @brief send a 'new' startup notification message
  * @param seq - sequence context for which to send new message
  *
  * We do not really use this in this program...
  */
void
send_new(Sequence * seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "new:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyNew;
}

/** @brief send a 'change' startup notification message
  * @param seq - sequence context for which to send change message
  *
  * We do not really use this in this program...  However, we could use it to
  * update information determined about the client (if a client is associated)
  * before closing or waiting for the closure of the sequence.
  */
void
send_change(Sequence * seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "change:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyChanged;
}

/** @brief send a 'remove' startup notification message
  * @param seq - sequence context for which to send remove message
  *
  * We only need the ID= field to send a remove message.  We do this to close a
  * sequence that is awaiting the mapping of a window.
  */
static void
send_remove(Sequence * seq)
{
	char *msg, *p;

	p = msg = calloc(4096, sizeof(*msg));
	strcat(p, "remove:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyComplete;
}

/** @brief - get a proc file and read it into a buffer
  * @param pid - process id for which to get the proc file
  * @param name - name of the proc file
  * @param size - return the size of the buffer
  * @return char* - the buffer or NULL if it does not exist or error
  *
  * Used to get a proc file for a pid by name and read the entire file into a
  * buffer.  Returns the buffer and the size of the buffer read.
  */
char *
get_proc_file(pid_t pid, char *name, size_t *size)
{
	struct stat st;
	char *file, *buf;
	FILE *f;
	size_t read, total;

	file = calloc(64, sizeof(*file));
	snprintf(file, 64, "/proc/%d/%s", pid, name);

	if (stat(file, &st)) {
		free(file);
		*size = 0;
		return NULL;
	}
	buf = calloc(st.st_size, sizeof(*buf));
	if (!(f = fopen(file, "rb"))) {
		free(file);
		free(buf);
		*size = 0;
		return NULL;
	}
	free(file);
	/* read entire file into buffer */
	for (total = 0; total < st.st_size; total += read)
		if ((read = fread(buf + total, 1, st.st_size - total, f)))
			if (total < st.st_size) {
				free(buf);
				fclose(f);
				*size = 0;
				return NULL;
			}
	fclose(f);
	*size = st.st_size;
	return buf;
}

/** @brief get a process' startup id
  * @param pid - pid of the process
  * @return char* - startup id or NULL
  *
  * Gets the DESKTOP_STARTUP_ID environment variable for a given process from
  * the environment file for the corresponding pid.  This only works when the
  * pid is for a process on the same host as we are.
  *
  * /proc/%d/environ contains the evironment for the process.  The entries are
  * separated by null bytes ('\0'), and there may be a null byte at the end.
  */
char *
get_proc_startup_id(pid_t pid)
{
	char *buf, *pos, *end;
	size_t size;

	if (!(buf = get_proc_file(pid, "environ", &size)))
		return NULL;

	for (pos = buf, end = buf + size; pos < end; pos += strlen(pos) + 1) {
		if (strstr(pos, "DESKTOP_STARTUP_ID=") == pos) {
			pos += strlen("DESKTOP_STARTUP_ID=");
			pos = strdup(pos);
			free(buf);
			return pos;
		}
	}
	free(buf);
	return NULL;
}

/** @brief get the command of a process
  * @param pid - pid of the process
  * @return char* - the command or NULL
  *
  * Obtains the command line of a process.
  */
char *
get_proc_comm(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "comm", &size);
}

/** @brief get the executable of a process
  * @param pid - pid of the process
  * @return char* - the executable path or NULL
  *
  * Obtains the executable path (binary file) executed by a process, or NULL if
  * there is no executable for the specified pid.  This uses the /proc/%d/exe
  * symbolic link to the executable file.  The returned pointer belongs to the
  * caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/exec is a symblolic link containing the actual pathname of the
  * executed command.  This symbolic link can be dereference normally;
  * attempting to open it will open the executable.  You can even execute the
  * file to run another copy of the same executable as is being run by the
  * process.  In a multithreaded process, the contents of this symblic link are
  * not available if the main thread has already terminated.
  */
char *
get_proc_exec(pid_t pid)
{
	struct stat st;
	char *file, *buf;
	ssize_t size;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	snprintf(file, PATH_MAX, "/proc/%d/exe", pid);
	if (stat(file, &st)) {
		free(file);
		return NULL;
	}
	buf = calloc(256, sizeof(*buf));
	if ((size = readlink(file, buf, 256)) < 0 || size > 256 - 1) {
		free(file);
		free(buf);
		return NULL;
	}
	buf[size + 1] = '\0';
	return buf;
}

/** @brief get the first argument of the command line for a process
  * @param pid - pid of the process
  * @return char* - the command line
  *
  * Obtains the arguments of the command line (argv) from the /proc/%d/cmdline
  * file.  This file contains a list of null-terminated strings.  The pointer
  * returned points to the first argument in the list.  The returned pointer
  * belongs to the caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/cmdline holds the complete command line for the process, unless the
  * process is a zombie.  In the later case there is nothing in this file: that
  * is, a readon on this file will return 0 characters.  The command-line
  * arguments appear in this file as a set of null ('\0') terminated strings.
  */
char *
get_proc_argv0(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "cmdline", &size);
}

/** @brief test a client against a startup notification sequence
  * @param c - client to test
  * @param seq - sequence against which to test
  * @return Bool - True if the client matches the sequence, False otherwise
  */
Bool
test_client(Client *c, Sequence * seq)
{
	pid_t pid;
	char *str;

	if (c->startup_id) {
		if (!strcmp(c->startup_id, seq->f.id))
			return True;
		else
			return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcmp(seq->f.hostname, c->hostname)))
		pid = 0;
	if (pid && (str = get_proc_startup_id(pid))) {
		if (strcmp(seq->f.id, str))
			return False;
		else
			return True;
		free(str);
	}
	/* correct wmclass */
	if (seq->f.wmclass) {
		if (c->ch.res_name && !strcmp(seq->f.wmclass, c->ch.res_name))
			return True;
		if (c->ch.res_class && !strcmp(seq->f.wmclass, c->ch.res_class))
			return True;
	}
	/* same process id */
	if (pid && atoi(seq->f.pid) == pid)
		return True;
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == strtoul(seq->f.timestamp, NULL, 0))
		return True;
	/* correct executable */
	if (pid && seq->f.bin) {
		if ((str = get_proc_comm(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
	}
	/* NOTE: we use the PID to look in: /proc/[pid]/cmdline for argv[]
	   /proc/[pid]/environ for env[] /proc/[pid]/comm basename of
	   executable /proc/[pid]/exe symbolic link to executable */
	return False;
}

/** @brief assist some clients by adding information missing from window
  *
  * Some applications do not use a full toolkit and do not properly set all of
  * the EWMH properties.  Once we have identified the startup sequence
  * associated with a client, we can set infomration on the client from the
  * startup notification sequence.
  *
  * Also do things like use /proc/[pid]/cmdline to set up WM_COMMAND if not
  * present.
  */
void
setup_client(Client *c)
{
	Sequence *seq;
	long data;

	if (!(seq = c->seq))
		return;

	/* set up _NET_STARTUP_ID */
	if (!c->startup_id && seq->f.id) {
		XChangeProperty(dpy, c->win, _XA_NET_STARTUP_ID, _XA_UTF8_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.id, strlen(seq->f.id));
	}

	/* set up _NET_WM_PID */
	if (!c->pid && seq->n.pid) {
		data = seq->n.pid;
		XChangeProperty(dpy, c->win, _XA_NET_WM_PID, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
		c->pid = seq->n.pid;
	}

	/* set up WM_CLIENT_MACHINE */
	if (!c->hostname && seq->f.hostname) {
		XChangeProperty(dpy, c->win, XA_WM_CLIENT_MACHINE, XA_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.hostname,
				strlen(seq->f.hostname));
	}

	/* set up WM_COMMAND */
	if (c->pid && !c->command) {
		char *string;
		char buf[65] = { 0, };
		size_t size;

		gethostname(buf, sizeof(buf) - 1);
		if ((c->hostname && !strcmp(buf, c->hostname)) ||
		    (seq->f.hostname && !strcmp(buf, seq->f.hostname))) {
			if ((string = get_proc_file(c->pid, "cmdline", &size))) {
				XChangeProperty(dpy, c->win, XA_WM_COMMAND,
						XA_STRING, 8, PropModeReplace,
						(unsigned char *) string, size);
				free(string);
			}
		}
	}
}

static Sequence *ref_sequence(Sequence * seq);

/** @brief find the startup sequence for a client
  * @param c - client to lookup startup sequence for
  * @return Sequence* - the found sequence or NULL if not found
  */
static Sequence *
find_startup_seq(Client *c)
{
	Sequence *seq;

	if ((seq = c->seq))
		return seq;

	/* search by startup id */
	if (c->startup_id) {
		for (seq = sequences; seq; seq = seq->next) {
			if (!seq->f.id) {
				EPRINTF("sequence with null id!\n");
				continue;
			}
			if (!strcmp(c->startup_id, seq->f.id))
				break;
		}
		if (!seq) {
			DPRINTF("cannot find startup id '%s'!\n", c->startup_id);
			return (seq);
		}
		DPRINTF("Found sequence by _NET_STARTUP_ID\n");
		goto found_it;
	}

	/* search by wmclass */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name && !strcmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class && !strcmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF("Found sequence by res_name or res_class\n");
			goto found_it;
		}
	}

	/* search by binary */
	if (c->command) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *binary;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((binary = seq->f.bin)) {
				if (c->command[0] && !strcmp(binary, c->command[0]))
					break;
			}

		}
		if (seq) {
			DPRINTF("Found sequence by command\n");
			goto found_it;
		}
	}

	/* search by wmclass (this time case insensitive) */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name && !strcasecmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class && !strcasecmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF("Found sequence by res_name or res_class\n");
			goto found_it;
		}
	}

	/* search by pid and hostname */
	if (c->pid && c->hostname) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *hostname;
			pid_t pid;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if (!(pid = seq->n.pid) || !(hostname = seq->f.hostname))
				continue;
			if (c->pid == pid && !strcmp(c->hostname, hostname))
				break;
		}
		if (seq) {
			DPRINTF("Found sequence by pid and hostname\n");
			goto found_it;
		}
	}
	DPRINTF("could not find startup ID for client\n");
	return NULL;
      found_it:
	seq->client = c;

	c->seq = ref_sequence(seq);
	setup_client(c);
	return (seq);
}

/** @brief detect whether a client is a dockapp
  * @param c - client the check
  * @return Bool - true if the client is a dockapp; false, otherwise.
  */
static Bool
is_dockapp(Client *c)
{
	/* In an attempt to get around GTK+ >= 2.4.0 limitations, some GTK+
	   dock apps simply set the res_class to "DockApp". */
	if (c->ch.res_class && !strcmp(c->ch.res_class, "DockApp"))
		return True;
	if (!c->wmh)
		return False;
	/* Many libxpm based dockapps use xlib to set the state hint correctly. 
	 */
	if ((c->wmh->flags & StateHint) && c->wmh->initial_state == WithdrawnState)
		return True;
	/* Many dockapps that were based on GTK+ < 2.4.0 are having their
	   initial_state changed to NormalState by GTK+ >= 2.4.0, so when the
	   other flags are set, accept anyway (note that IconPositionHint is
	   optional). */
	if ((c->wmh->flags & ~IconPositionHint) == (WindowGroupHint | StateHint | IconWindowHint))
		return True;
	return False;
}

/** @brief detect whether a client is a status icon
  */
Bool
is_statusicon(Client *c)
{
	if (c->xei)
		return True;
	return False;
}

/** @brief update client from X server
  * @param c - the client to update
  *
  * Updates the client from information and properties maintained by the X
  * server.  Care should be taken that information that shoule be obtained from
  * a group window is obtained from that window first and then overwritten by
  * any information contained in the specific window.
  */
static void
update_client(Client *c)
{
#if 1
#if 1
	struct atoms *atom;
#endif

	XSaveContext(dpy, c->win, ScreenContext, (XPointer) scr);
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);

#if 1
	for (atom = wmprops; atom->name; atom++)
		if (atom->pc_handler)
			atom->pc_handler(NULL, c);
#endif
	c->new = False;
#else
	long card;
	int count = 0;

	if (c->wmh)
		XFree(c->wmh);
	c->group = None;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
	}

	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);

	XFetchName(dpy, c->win, &c->name);
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);
	}

	if (c->hostname)
		XFree(c->hostname);
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);

	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);

	free(c->startup_id);
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);

	card = 0;
	if (!get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card) && c->group)
		get_cardinal(c->group, _XA_NET_WM_PID, XA_CARDINAL, &card);
	c->pid = card;

	find_startup_seq(c);
#if 0
	if (test_client(c))
		setup_client(c);
#endif
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);
	if (c->group) {
		XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->group,
			     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
			     FocusChangeMask | PropertyChangeMask);
	}
	c->new = False;
#endif
}

static Client *
add_client(Window win)
{
	Client *c;

	c = calloc(1, sizeof(*c));
	c->win = win;
	c->next = scr->clients;
	scr->clients = c;
	update_client(c);
	c->new = True;
	return (c);
}

static Sequence *unref_sequence(Sequence * seq);

static void
remove_client(Client *c)
{
	Window *winp;
	int i;

	DPRINT();
	if (c->startup_id) {
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (c->name) {
		XFree(c->name);
		c->name = NULL;
	}
	if (c->hostname) {
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (c->role) {
		XFree(c->role);
		c->role = NULL;
	}
	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (c->wmh) {
		XFree(c->wmh);
		c->wmh = NULL;
	}
	if (c->wms) {
		XFree(c->wms);
		c->wms = NULL;
	}
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	for (i = 0, winp = &c->win; i < 5; i++, winp++) {
		if (*winp) {
			XDeleteContext(dpy, *winp, ScreenContext);
			XDeleteContext(dpy, *winp, ClientContext);
			XSelectInput(dpy, *winp, NoEventMask);
			*winp = None;
		}
	}
	if (c->seq && c->seq->client == c) {
		c->seq->client = NULL;

		unref_sequence(c->seq);
		c->seq = NULL;
	}
	free(c);
}

void
del_client(Client *r)
{
	Client *c, **cp;

	DPRINT();
	for (cp = &scr->clients, c = *cp; c && c != r; cp = &c->next, c = *cp) ;
	if (c == r)
		*cp = c->next;
	remove_client(r);
}

/** @brief update client from startup notification sequence
  * @param seq - the sequence that has changed state
  *
  * Update the client associated with a startup notification sequence.
  */
static void
update_startup_client(Sequence * seq)
{
	Client *c;

	if (!(c = seq->client))
		/* Note that we do not want to go searching for clients because 
		   any client that we would find at this point could get a
		   false positive against an client that existed before the
		   startup notification sequence.  We could use creation
		   timestamps to filter out the false positives, but that is
		   for later. */
		return;
	/* TODO: things to update are: _NET_WM_PID, WM_CLIENT_MACHINE, ... Note 
	   that _NET_WM_STARTUP_ID should already be set. */
}

static void
convert_sequence_fields(Sequence * seq)
{
	if (seq->f.screen)
		seq->n.screen = atoi(seq->f.screen);
	if (seq->f.monitor)
		seq->n.monitor = atoi(seq->f.monitor);
	if (seq->f.desktop)
		seq->n.desktop = atoi(seq->f.desktop);
	if (seq->f.timestamp)
		seq->n.timestamp = atoi(seq->f.timestamp);
	if (seq->f.silent)
		seq->n.silent = atoi(seq->f.silent);
	if (seq->f.pid)
		seq->n.pid = atoi(seq->f.pid);
	if (seq->f.sequence)
		seq->n.sequence = atoi(seq->f.sequence);
}

static void
free_sequence_fields(Sequence * seq)
{
	int i;

	for (i = 0; i < sizeof(seq->f) / sizeof(char *); i++) {
		free(seq->fields[i]);
		seq->fields[i] = NULL;
	}
}

static void
show_sequence(Sequence * seq)
{
	char **label, **field;

	if (options.debug <= 0)
		return;
	for (label = (char **) StartupNotifyFields, field = seq->fields; *label; label++, field++) {
		if (*field)
			fprintf(stderr, "%s=%s\n", *label, *field);
		else
			fprintf(stderr, "%s=\n", *label);
	}
}

static void
copy_sequence_fields(Sequence * old, Sequence * new)
{
	int i;

	for (i = 0; i < sizeof(old->f) / sizeof(char *); i++) {
		if (new->fields[i]) {
			free(old->fields[i]);
			old->fields[i] = new->fields[i];
		}
	}
	convert_sequence_fields(old);
	DPRINTF("Updated sequence fields:\n");
	show_sequence(old);
}

static Sequence *
find_seq_by_id(char *id)
{
	Sequence *seq;

	for (seq = sequences; seq; seq = seq->next)
		if (!strcmp(seq->f.id, id))
			break;
	return (seq);
}

static void
close_sequence(Sequence * seq)
{
#ifdef HAVE_GLIB_EVENT_LOOP
	if (seq->timer) {
		DPRINTF("removing timer\n");
		g_source_remove(seq->timer);
		seq->timer = 0;
	}
#endif				/* HAVE_GLIB_EVENT_LOOP */
#ifdef SYSTEM_TRAY_STATUS_ICON
	if (seq->status) {
		DPRINTF("removing statusicon\n");
		g_object_unref(G_OBJECT(seq->status));
		seq->status = NULL;
	}
#endif				/* SYSTEM_TRAY_STATUS_ICON */
#ifdef DESKTOP_NOTIFICATIONS
	if (seq->notification) {
		DPRINTF("removing notificiation\n");
		g_object_unref(G_OBJECT(seq->notification));
		seq->notification = NULL;
	}
#endif				/* DESKTOP_NOTIFICATIONS */
}

static Sequence *
unref_sequence(Sequence * seq)
{
	if (seq) {
		if (--seq->refs <= 0) {
			Sequence *s, **prev;

			DPRINTF("deleting sequence\n");
			for (prev = &sequences, s = *prev; s && s != seq;
			     prev = &s->next, s = *prev) ;
			if (s) {
				*prev = s->next;
				s->next = NULL;
			}
			close_sequence(seq);
			free_sequence_fields(seq);
			free(seq);
			return (NULL);
		} else
			DPRINTF("sequence still has %d references\n", seq->refs);
	} else
		EPRINTF("called with null pointer\n");
	return (seq);
}

static Sequence *
ref_sequence(Sequence * seq)
{
	if (seq)
		++seq->refs;
	return (seq);
}

static Sequence *
remove_sequence(Sequence * del)
{
	Sequence *seq, **prev;

	DPRINTF("Removing sequence:\n");
	show_sequence(del);
	for (prev = &sequences, seq = *prev; seq && seq != del; prev = &seq->next, seq = *prev) ;
	if (seq) {
		*prev = seq->next;
		seq->next = NULL;
		close_sequence(seq);
		unref_sequence(seq);
	} else
		EPRINTF("could not find sequence\n");
	return (seq);
}

static gboolean
sequence_timeout_callback(gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	if (seq->state == StartupNotifyComplete) {
		remove_sequence(seq);
		seq->timer = 0;
		return FALSE;	/* stop timeout interval */
	}
	/* for now, just generate a remove message after the guard-time */
	if (1) {
		send_remove(seq);
		seq->timer = 0;
		return G_SOURCE_REMOVE;	/* remove timeout source */
	}
	return G_SOURCE_CONTINUE;	/* continue timeout interval */
}

/** @brief add a new startup notification sequence to list for screen
  *
  * When a startup notification sequence is added, it is because we have
  * received a 'new:' or 'remove:' message.  What we want to do is to generate a
  * status icon at this point that will be updated and removed as the startup
  * notification sequence is changed or completed.  We add StartupNotifyComplete
  * sequences that start with a 'remove:' message in case the 'new:' message is
  * late.  We always add a guard timer so that the StartupNotifyComplete
  * sequence will always be removed at some point.
  */
static void
add_sequence(Sequence * seq)
{
	seq->refs = 1;
	seq->client = NULL;

	seq->next = sequences;
	sequences = seq;
	if (seq->state == StartupNotifyNew) {
	}
	seq->timer = g_timeout_add(options.guard, sequence_timeout_callback, (gpointer) seq);
	DPRINTF("Added sequence:\n");
	show_sequence(seq);
}

static void
process_startup_msg(Message * m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q, *copy, *b;
	const char **f;
	int i;
	int escaped, quoted;

	DPRINTF("Got message: %s\n", m->data);
	if (!strncmp(p, "new:", 4)) {
		cmd.state = StartupNotifyNew;
	} else if (!strncmp(p, "change:", 7)) {
		cmd.state = StartupNotifyChanged;
	} else if (!strncmp(p, "remove:", 7)) {
		cmd.state = StartupNotifyComplete;
	} else {
		free(m->data);
		free(m);
		return;
	}
	p = strchr(p, ':') + 1;
	while (*p != '\0') {
		while (*p == ' ')
			p++;
		k = p;
		if (!(v = strchr(k, '='))) {
			free_sequence_fields(&cmd);
			DPRINTF("mangled message\n");
			return;
		} else {
			*v++ = '\0';
			p = q = v;
			escaped = quoted = 0;
			for (;;) {
				if (!escaped) {
					if (*p == '"') {
						p++;
						quoted ^= 1;
						continue;
					} else if (*p == '\\') {
						p++;
						escaped = 1;
						continue;
					} else if (*p == '\0' || (*p == ' ' && !quoted)) {
						if (quoted) {
							free_sequence_fields(&cmd);
							DPRINTF("mangled message\n");
							return;
						}
						if (*p == ' ')
							p++;
						*q = '\0';
						break;
					}
				}
				*q++ = *p++;
				escaped = 0;
			}
			for (i = 0, f = StartupNotifyFields; f[i] != NULL; i++)
				if (strcmp(f[i], k) == 0)
					break;
			if (f[i] != NULL)
				cmd.fields[i] = strdup(v);
		}
	}
	free(m->data);
	free(m);
	if (!cmd.f.id) {
		free_sequence_fields(&cmd);
		DPRINTF("message with no ID= field\n");
		return;
	}
	/* get information from ID */
	do {
		p = q = copy = strdup(cmd.f.id);
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launcher)
			cmd.f.launcher = strdup(q);
		q = p;
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launchee)
			cmd.f.launchee = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.pid)
			cmd.f.pid = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.sequence)
			cmd.f.sequence = strdup(q);
		q = p;
		if (!(p = strstr(q, "_TIME")))
			break;
		*p++ = '\0';
		if (!cmd.f.hostname)
			cmd.f.hostname = strdup(q);
		q = p + 4;
		if (!cmd.f.timestamp)
			cmd.f.timestamp = strdup(q);
	} while (0);
	free(copy);
	/* get timestamp from ID if necessary */
	if (!cmd.f.timestamp && (p = strstr(cmd.f.id, "_TIME")) != NULL)
		cmd.f.timestamp = strdup(p + 5);
	convert_sequence_fields(&cmd);
	if (!(seq = find_seq_by_id(cmd.f.id))) {
		switch (cmd.state) {
		default:
			free_sequence_fields(&cmd);
			DPRINTF("message out of sequence\n");
			return;
		case StartupNotifyNew:
		case StartupNotifyComplete:
			break;
		}
		if (!(seq = calloc(1, sizeof(*seq)))) {
			free_sequence_fields(&cmd);
			DPRINTF("no memory\n");
			return;
		}
		*seq = cmd;
		add_sequence(seq);
		return;
	}
	switch (seq->state) {
	case StartupNotifyIdle:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
			seq->state = StartupNotifyNew;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyNew:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyChanged:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyComplete:
		/* remove sequence */
		break;
	}
	/* remove sequence */
	remove_sequence(seq);
}

static void
cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent * e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	DPRINT();
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m) {
		m = calloc(1, sizeof(*m));
		XSaveContext(dpy, from, MessageContext, (XPointer) m);
	}
	free(m->data);
	m->origin = from;
	m->data = calloc(21, sizeof(*m->data));
	m->len = 0;
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

static void
cm_handle_NET_STARTUP_INFO(XClientMessageEvent * e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	DPRINT();
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m)
		return;
	m->data = realloc(m->data, m->len + 21);
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

void
clean_msgs(Window w)
{
	Message *m = NULL;

	if (XFindContext(dpy, w, MessageContext, (XPointer *) &m))
		return;

	XDeleteContext(dpy, w, MessageContext);
	free(m->data);
	free(m);
}

/** @brief perform actions necessary for a newly managed client
  * @param c - the client
  * @param t - the time that the client was managed or CurrentTime when unknown
  *
  * The client has just become managed, so try to associated it with a startup
  * notfication sequence if it has not already been associated.  If the client
  * can be associated with a startup notification sequence, then terminate the
  * sequence when it has WMCLASS set, or when SILENT is set (indicating no
  * startup notification is pending).  Otherwise, wait for the sequence to
  * complete on its own.
  *
  * If we cannot associated a sequence at this point, the launcher may be
  * defective and only sent the startup notification after the client has
  * already launched, so leave the client around for later checks.  We can
  * compare the map_time of the client to the timestamp of a later startup
  * notification to determine whether the startup notification should have been
  * sent before the client mapped.
  */
static void
managed_client(Client *c, Time t)
{
	Sequence *seq = NULL;

	c->managed = True;
	pulltime(&c->map_time, t ? : c->last_time);
	if (!(seq = c->seq) && !(seq = find_startup_seq(c)))
		return;
	switch (seq->state) {
	case StartupNotifyIdle:
	case StartupNotifyComplete:
		break;
	case StartupNotifyNew:
	case StartupNotifyChanged:
		if (!seq->f.wmclass && !seq->n.silent)
			/* We are expecting that the client will generate
			   startup notification completion on its own, so let
			   the timers run and wait for completion. */
			return;
		/* We are not expecting that the client will generate startup
		   notification completion on its own.  Either we generate the
		   completion or wait for a supporting window manager to do so. 
		 */
		send_remove(seq);
		break;
	}
	/* FIXME: we can remove the startup notification and the client, but
	   perhaps we will just wait for the client to be unmanaged or
	   destroyed. */
}

static void
unmanaged_client(Client *c)
{
	c->managed = False;
	/* FIXME: we can remove the client and any associated startup
	   notification. */
}

/** @brief handle a client list of windows
  * @param e - PropertyNotify event that trigger the change
  * @param atom - the client list atom
  * @param type - the type of the list (XA_WINDOW or XA_CARDINAL)
  *
  * Process a changed client list.  Basically any client window that is on this
  * list is managed if it was not previously managed.  Also, any client that was
  * previously on the list is no longer managed when it does not appear on the
  * list any more (unless it is a dockapp or a statusicon).
  */
static void
pc_handle_CLIENT_LIST(XPropertyEvent *e, Atom atom, Atom type)
{
	Window *list;
	long i, n;
	Client *c, *cn;

	DPRINT();
	if ((list = get_windows(scr->root, atom, type, &n))) {
		for (c = scr->clients; c; c->breadcrumb = False, c->new = False, c = c->next) ;
		for (i = 0; i < n; i++) {
			if (XFindContext(dpy, list[i], ClientContext, (XPointer *) &c) == Success) {
				c->breadcrumb = True;
				c->listed = True;
				managed_client(c, e->time);
			}
		}
		XFree(list);
		for (cn = scr->clients; (c = cn);) {
			cn = c->next;
			if (!c->breadcrumb && c->listed) {
				c->listed = False;
				unmanaged_client(c);
			}
		}
	}
}

/** @brief track the active window
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the _NET_ACTIVE_WINDOW property on the root window set
  * by EWMH/NetWM compliant window managers to indicate the active window.  We
  * keep track of the last time that each client window was active.
  */
static void
pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *e, Client *c)
{
	Window active = None;

	DPRINT();
	if (c || !e || e->window != scr->root || e->state == PropertyDelete)
		return;
	if (get_window(scr->root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW, &active) && active) {
		if (XFindContext(dpy, active, ClientContext, (XPointer *) &c) == Success) {
			pushtime(&c->active_time, e->time);
			managed_client(c, e->time);
		}
	}
}

static void
cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	pushtime(&current_time, e->data.l[1]);
	pushtime(&c->active_time, e->data.l[1]);
	if (c->managed)
		return;
	EPRINTF("_NET_ACTIVE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, e->data.l[1]);
}

static void
pc_handle_NET_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static void
pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST_STACKING, XA_WINDOW);
}

static void
cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent * e, Client *c)
{
	Time time;

	DPRINT();
	if (!c)
		return;
	time = e ? e->data.l[0] : CurrentTime;
	pushtime(&current_time, time);
	pushtime(&c->active_time, time);

	if (c->managed)
		return;
	EPRINTF("_NET_CLOSE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, time);
}

static void
cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_MOVERESIZE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	/* This message, unlike others, is sent before a window is initially
	   mapped (managed). */
	if (!c || !c->managed)
		return;
	EPRINTF("_NET_REQUEST_FRAME_EXTENTS for managed window 0x%lx\n", e->window);
}

static void
cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_RESTACK_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_STARTUP_ID(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->startup_id) {
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);
}

static void
pc_handle_NET_SUPPORTED(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	EPRINTF("_NET_WM_MOVERSIZE for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_PID(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

/** @brief handle _NET_WM_STATE property change
  *
  * Unlike WM_STATE, it is ok for the client to set this property before mapping
  * a window.  After the window is mapped it must be changd with the
  * _NET_WM_STATE client message.  There are, however, two atoms in the state:
  * _NET_WM_STATE_HIDDEN and _NET_WM_STATE_FOCUSED, that should be treated as
  * read-only by clients and will, therefore, only be set by the window manager.
  * When either of these atoms are set (or removed [TODO]), we should treat the
  * window as managed if it has not already been managed.
  *
  * The window manager is the only one responsible for deleting the property,
  * and WM-SPEC-1.5 says that it should be deleted whenever a window is
  * withdrawn but left in place when the window manager shuts down, is replaced,
  * or restarts.
  */
static void
pc_handle_NET_WM_STATE(XPropertyEvent *e, Client *c)
{
	Atom *atoms = NULL;
	long i, n;

	DPRINT();
	if (!c)
		return;
	if (e && e->state == PropertyDelete) {
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	if ((atoms = get_atoms(c->win, _XA_NET_WM_STATE, AnyPropertyType, &n))) {
		for (i = 0; i < n; i++) {
			if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
				pushtime(&c->focus_time, e->time);
				if (!c->managed)
					managed_client(c, e->time);
			} else if (atoms[i] == _XA_NET_WM_STATE_HIDDEN) {
				if (!c->managed)
					managed_client(c, e->time);
			}
		}
		XFree(atoms);
		if (!c->managed)
			managed_client(c, e->time);
	}
}

/** @brief handle _NET_WM_STATE client message
  *
  * If the client sends _NET_WM_STATE client messages, the client thinks that
  * the window is managed by the window manager because otherwise it would
  * simply set the property itself.  Therefore, this client message indicates
  * that the window should be managed if it has not already.
  *
  * Basically a client message is a client request to alter state while the
  * window is mapped, so mark window managed if it is not already.
  */
static void
cm_handle_NET_WM_STATE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_WM_STATE sent for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	c->time_win = None;
	if (e && e->state == PropertyDelete)
		return;
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		Time time;

		XSaveContext(dpy, c->time_win, ScreenContext, (XPointer) scr);
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);

		if (get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
			pushtime(&c->user_time, time);
			pushtime(&last_user_time, time);
			pushtime(&current_time, time);
		}
	}
}

static void
pc_handle_NET_WM_USER_TIME(XPropertyEvent *e, Client *c)
{
	Time time;

	DPRINT();
	if (!c || (e && e->state == PropertyDelete))
		return;
	if (get_time(c->time_win ? : c->win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
		pushtime(&c->user_time, time);
		pushtime(&last_user_time, time);
		pushtime(&current_time, time);
	}
}

/** @brief handle _NET_WM_VISIBLE_ICON_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	DPRINT();
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_ICON_NAME)))
		return;
	EPRINTF("_NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

/** @brief handle _NET_WM_VISIBLE_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	DPRINT();
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_NAME)))
		return;
	EPRINTF("_NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

static void
pc_handle_WIN_APP_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_WIN_CLIENT_LIST, XA_CARDINAL);
}

static void
pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_FOCUS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_LAYER(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_WIN_LAYER(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_WIN_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete)
		return;
	/* Not like WM_STATE and _NET_WM_STATE */
}

static void
cm_handle_WIN_STATE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_WIN_WORKSPACE(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_WIN_WORKSPACE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_SM_CLIENT_ID(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->client_id = get_text(c->win, _XA_SM_CLIENT_ID)) && c->leader)
		c->client_id = get_text(c->leader, _XA_SM_CLIENT_ID);
	if (!(c->client_id) && c->group)
		c->client_id = get_text(c->group, _XA_SM_CLIENT_ID);
}

static void
pc_handle_WM_CLASS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);
	c->dockapp = is_dockapp(c);
}

static void
cm_handle_WM_CHANGE_STATE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_CLIENT_LEADER(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->leader = None;
	if (e && e->state == PropertyDelete)
		return;
	get_window(c->win, _XA_WM_CLIENT_LEADER, AnyPropertyType, &c->leader);
}

static void
pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->hostname) {
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->leader)
		c->hostname = get_text(c->leader, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->transient_for)
		c->hostname = get_text(c->transient_for, XA_WM_CLIENT_MACHINE);
}

static void
pc_handle_WM_COMMAND(XPropertyEvent *e, Client *c)
{
	DPRINT();
	int count = 0;

	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);
	if (!c->command && c->leader)
		XGetCommand(dpy, c->leader, &c->command, &count);
	if (!c->command && c->transient_for)
		XGetCommand(dpy, c->transient_for, &c->command, &count);
}

static void
pc_handle_WM_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wmh) {
		XFree(c->wmh);
		c->wmh = NULL;
		c->group = None;
	}
	if (e && e->state == PropertyDelete)
		return;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
		if (c->group && c->group != scr->root) {
			XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->group,
				     ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
		c->dockapp = is_dockapp(c);
	}
}

static void
pc_handle_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_ICON_SIZE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_NORMAL_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_WM_PROTOCOLS(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_SIZE_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wms) {
		XFree(c->wms);
		c->wms = NULL;
	}
	if (e && e->state == PropertyDelete) {
		unmanaged_client(c);
		return;
	}
	if (!(c->wms = XGetWMState(dpy, c->win))) {
		/* XXX: about the only time this should happed would be if the
		   window was destroyed out from under us: check that.  We
		   should get a DestroyNotify soon anyway. */
		return;
	}
	if (c->managed) {
		switch (c->wms->state) {
		case WithdrawnState:
			unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* This is merely a state transition between active
			   states. */
			break;
		}
	} else {
		switch (c->wms->state) {
		case WithdrawnState:
			/* The window manager placed a WM_STATE of
			   WithdrawnState on the window which probably means
			   that it is a dock app that was just managed, but
			   only for WindowMaker work-alikes. Otherwise, per
			   ICCCM, placing withdrawn state on the window means
			   that it is unmanaged. */
			if ((c->dockapp = is_dockapp(c)) && scr->maker_check)
				managed_client(c, e ? e->time : CurrentTime);
			else
				unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* The window manager place a WM_STATE of other than
			   WithdrawnState on the window which means that it was 
			   just managed per ICCCM. */
			managed_client(c, e ? e->time : CurrentTime);
			break;
		}
	}
}

static void
cm_handle_WM_STATE(XClientMessageEvent * e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->transient_for = None;
	if (e && e->state == PropertyDelete)
		return;
	get_window(c->win, XA_WM_TRANSIENT_FOR, AnyPropertyType, &c->transient_for);
}

static void
pc_handle_WM_WINDOW_ROLE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->role) {
		XFree(c->role);
		c->role = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	c->role = get_text(c->win, _XA_WM_WINDOW_ROLE);
}

static void
pc_handle_WM_ZOOM_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	/* we don't actually process zoom hints */
}

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

static void
pc_handle_XEMBED_INFO(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	if (e && e->state == PropertyDelete) {
		c->statusicon = False;
		return;
	}
	if ((c->xei = XGetEmbedInfo(dpy, c->win))) {
		c->dockapp = False;
		c->statusicon = True;
		return;
	}
}

/* Note:
 *	xclient.data.l[0] = timestamp
 *	xclient.data.l[1] = major opcode
 *	xclient.data.l[2] = detail code
 *	xclient.data.l[3] = data1
 *	xclient.data.l[4] = data2
 *
 * Sent to target window with no event mask and propagation false.
 */

static void
cm_handle_XEMBED(XClientMessageEvent * e, Client *c)
{
	DPRINT();
	if (!c || !e || e->type != ClientMessage)
		return;
	pushtime(&current_time, e->data.l[0]);
	if (c->managed)
		return;
	switch (e->data.l[1]) {
	case XEMBED_EMBEDDED_NOTIFY:
		/* Sent from the embedder to the client on embedding, after
		   reparenting and mapping the client's X window.  A client
		   that receives this message knows that its window was
		   embedded by an XEmbed site and not simply reparented by a
		   window manager. The embedder's window handle is in data1.
		   data2 is the protocol version in use. */
		managed_client(c, e->data.l[0]);
		return;
	case XEMBED_WINDOW_ACTIVATE:
	case XEMBED_WINDOW_DEACTIVATE:
		/* Sent from embedder to client when the window becomes active
		   or inactive (keyboard focus) */
		break;
	case XEMBED_REQUEST_FOCUS:
		/* Sent from client to embedder to request keyboard focus. */
		break;
	case XEMBED_FOCUS_IN:
	case XEMBED_FOCUS_OUT:
		/* Sent from embedder to client when it gets or loses focus. */
		break;
	case XEMBED_FOCUS_NEXT:
	case XEMBED_FOCUS_PREV:
		/* Sent from the client to embedder at the end or start of tab
		   chain. */
		break;
	case XEMBED_GRAB_KEY:
	case XEMBED_UNGRAB_KEY:
		/* Obsolete */
		break;
	case XEMBED_MODALITY_ON:
	case XEMBED_MODALITY_OFF:
		/* Sent from embedder to client when shadowed or released by a
		   modal dialog. */
		break;
	case XEMBED_REGISTER_ACCELERATOR:
	case XEMBED_UNREGISTER_ACCELERATOR:
	case XEMBED_ACTIVATE_ACCELERATOR:
		/* Sent from client to embedder to register an accelerator.
		   'detail' is the accelerator_id.  For register, data1 is the
		   X key symbol, and data2 is a bitfield of modifiers. */
		break;
	}
	/* XXX: there is a question whether we should manage the client here or 
	   not. These other messages indicate that we have probably missed a
	   management event somehow, so mark it managed anyway. */
	EPRINTF("_XEMBED message for unmanaged client 0x%lx\n", e->window);
	managed_client(c, e->data.l[0]);
}

/** @brief handle MANAGER client message
  *
  * We are expecting MANAGER messages for two selections: WM_S%d and
  * _NET_SYSTEM_TRAY_S%d which correspond to window manager and system tray,
  * respectively.
  */
static void
cm_handle_MANAGER(XClientMessageEvent * e, Client *c)
{
	Window owner;
	Atom selection;
	Time time;

	DPRINT();
	if (!e || e->format != 32)
		return;
	time = e->data.l[0];
	selection = e->data.l[1];
	owner = e->data.l[2];

	(void) time;		/* FIXME: use this somehow */

	if (selection == scr->icccm_atom) {
		if (owner && owner != scr->icccm_check) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("window manager changed from 0x%lx to 0x%lx\n",
				scr->icccm_check, owner);
			scr->icccm_check = owner;
			handle_wmchange();
		}
	} else if (selection == scr->stray_atom) {
		if (owner && owner != scr->stray) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("system tray changed from 0x%lx to 0x%lx\n", scr->stray, owner);
			scr->stray = owner;
			handle_wmchange();
		}
	}
}

static void
pc_handle_TIMESTAMP_PROP(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

void
pc_handle_atom(XPropertyEvent *e, Client *c)
{
	pc_handler_t handle = NULL;

	if (XFindContext(dpy, e->atom, PropertyContext, (XPointer *) &handle) == Success)
		(*handle) (e, c);
	else
		DPRINTF("no PropertyNotify handler for %s\n", XGetAtomName(dpy, e->atom));
}

void
cm_handle_atom(XClientMessageEvent * e, Client *c)
{
	cm_handler_t handle = NULL;

	if (XFindContext(dpy, e->message_type, ClientMessageContext, (XPointer *) &handle)
	    == Success)
		(*handle) (e, c);
	else
		DPRINTF("no ClientMessage handler for %s\n", XGetAtomName(dpy, e->message_type));
}

/** @brief handle monitoring events
  * @param e - X event to handle
  *
  * If the window mwnager has a client list, we can check for newly mapped
  * window by additions to the client list.  We can calculate the user time by
  * tracking _NET_WM_USER_TIME and _NET_WM_TIME_WINDOW on all clients.  If the
  * window manager supports _NET_WM_STATE_FOCUSED or at least
  * _NET_ACTIVE_WINDOW, coupled with FocusIn and FocusOut events, we should be
  * able to track the last focused window at the time that the app started (not
  * all apps support _NET_WM_USER_TIME).
  */
void
handle_event(XEvent *e)
{
	Client *c;

	switch (e->type) {
	case PropertyNotify:
		XPRINTF("got PropertyNotify event\n");
		pushtime(&current_time, e->xproperty.time);
		if (!find_screen(e->xproperty.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xproperty.window);
			break;
		}
		if ((c = find_client(e->xproperty.window)))
			pushtime(&c->last_time, e->xproperty.time);
		pc_handle_atom(&e->xproperty, c);
		break;
	case FocusIn:
	case FocusOut:
		XPRINTF("got FocusIn/FocusOut event\n");
		if (!find_screen(e->xfocus.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xfocus.window);
			break;
		}
		if ((c = find_client(e->xfocus.window)) && !c->managed)
			/* We missed a management event, so treat the window as 
			   managed now... */
			managed_client(c, CurrentTime);
		break;
	case Expose:
		XPRINTF("got Expose event\n");
		if (!find_screen(e->xexpose.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xexpose.window);
			break;
		}
		if ((c = find_client(e->xexpose.window)) && !c->managed)
			/* We missed a management event, so treat the window as 
			   managed now... */
			managed_client(c, CurrentTime);
		break;
	case VisibilityNotify:
		XPRINTF("got VisibilityNotify event\n");
		if (!find_screen(e->xvisibility.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xvisibility.window);
			break;
		}
		if ((c = find_client(e->xvisibility.window)) && !c->managed)
			/* We missed a management event, so treat the window as 
			   managed now. */
			managed_client(c, CurrentTime);
		break;
	case CreateNotify:
		XPRINTF("got CreateNotify event\n");
		/* only interested in top-level windows that are not override
		   redirect */
		if (e->xcreatewindow.override_redirect)
			break;
		if (!find_screen(e->xcreatewindow.window)) {
			EPRINTF("could not find screen for window 0x%lx\n",
				e->xcreatewindow.window);
			break;
		}
		if (!(c = find_client(e->xcreatewindow.window)))
			if (e->xcreatewindow.parent == scr->root)
				c = add_client(e->xcreatewindow.window);
		break;
	case DestroyNotify:
		XPRINTF("got DestroyNotify event\n");
		if (!find_screen(e->xdestroywindow.window)) {
			EPRINTF("could not find screen for window 0x%lx\n",
				e->xdestroywindow.window);
			break;
		}
		if ((c = find_client(e->xdestroywindow.window)))
			del_client(c);
		clean_msgs(e->xdestroywindow.window);
		break;
	case MapNotify:
		XPRINTF("got MapNotify event\n");
		if (e->xmap.override_redirect)
			break;
		if (!find_screen(e->xmap.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xmap.window);
			break;
		}
		if ((c = find_client(e->xmap.window)) && !c->managed)
			/* Not sure we should do this for anything but dockapps 
			   here... Window managers may map normal windows
			   before setting the WM_STATE property, and system
			   trays invariably map window before sending the
			   _XEMBED message. */
			if ((c->dockapp = is_dockapp(c)))
				/* We missed a management event, so treat the
				   window as managed now */
				managed_client(c, CurrentTime);
		break;
	case UnmapNotify:
		XPRINTF("got UnmapNotify event\n");
		/* we can't tell much from a simple unmap event */
		break;
	case ReparentNotify:
		XPRINTF("got ReparentNotify event\n");
		/* any top-level window that is reparented by the window
		   manager should have WM_STATE set eventually (either before
		   or after the reparenting), or receive an _XEMBED client
		   message it they are a status icon.  The exception is dock
		   apps: some window managers reparent the dock app and never
		   set WM_STATE on it (even though WindowMaker does). */
		if (e->xreparent.override_redirect)
			break;
		if (!find_screen(e->xreparent.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xreparent.window);
			break;
		}
		if ((c = find_client(e->xreparent.window)) && !c->managed) {
			if (e->xreparent.parent == scr->root)
				/* reparented the wrong way */
				break;
			if ((c->statusicon = is_statusicon(c))) {
				/* This is a status icon. */
				if (scr->stray)
					/* Status icons will receive an _XEMBED 
					   message from the system tray when
					   managed. */
					break;
				/* It is questionable whether status icons will 
				   be reparented at all when there is no system 
				   tray... */
				EPRINTF("status icon 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else if (!(c->dockapp = is_dockapp(c))) {
				/* This is a normal window. */
				if (scr->redir_check)
					/* We will receive WM_STATE property
					   change when managed (if there is a
					   window manager present). */
					break;
				/* It is questionable whether regular windows
				   will be reparented at all when there is no
				   window manager... */
				EPRINTF("normal window 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else {
				/* This is a dock app. */
				if (scr->maker_check)
					/* We will receive a WM_STATE property
					   change when managed when a
					   WindowMaker work-alike is present. */
					break;
				/* Non-WindowMaker window managers reparent
				   dock apps without any further indication
				   that the dock app has been managed. */
				managed_client(c, CurrentTime);
			}
		}
		break;
	case ConfigureNotify:
		XPRINTF("got ConfigureNotify event\n");
		break;
	case ClientMessage:
		XPRINTF("got ClientMessage event\n");
		if (!find_screen(e->xclient.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xclient.window);
		}
		c = find_client(e->xclient.window);
		cm_handle_atom(&e->xclient, c);
		break;
	case MappingNotify:
		XPRINTF("got MappingNotify event\n");
	default:
		EPRINTF("unexpected xevent %d\n", (int) e->type);
		break;
	case SelectionClear:
		XPRINTF("got SelectionClear event\n");
#if 0
		int s;
#endif

		if (!find_screen(e->xselectionclear.window)) {
			EPRINTF("could not find screen for window 0x%lx\n",
				e->xselectionclear.window);
			break;
		}
		if (e->xselectionclear.selection != scr->slctn_atom)
			break;
		if (e->xselectionclear.window != scr->selwin)
			break;
		XDestroyWindow(dpy, scr->selwin);
		scr->selwin = None;
#if 0
		for (s = 0; s < nscr; s++)
			if (screens[s].selwin)
				break;
		if (s < nscr)
			break;
#endif
		DPRINTF("lost _XDE_AUTOSTART_S%d selection: exiting\n", scr->screen);
		exit(EXIT_SUCCESS);
	}
}

int signum;

void
sighandler(int sig)
{
	signum = sig;
}

static void
autoSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown,
		int interactStyle, Bool fast)
{
}

static void
autoDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	exit(EXIT_SUCCESS);
}

static void
autoSaveCompleteCB(SmcConn smcConn, SmPointer data)
{
}

static void
autoShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	/* nothing to do really */
}

static IceConn iceConn;
static SmcConn smcConn;

static unsigned long autoCBMask =
    SmcSaveYourselfProcMask | SmcDieProcMask |
    SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

static SmcCallbacks autoCBs = {
	.save_yourself = {
		.callback = &autoSaveYourselfCB,
		.client_data = NULL,
	},
	.die = {
		.callback = &autoDieCB,
		.client_data = NULL,
	},
	.save_complete = {
		.callback = &autoSaveCompleteCB,
		.client_data = NULL,
	},
	.shutdown_cancelled = {
		.callback = &autoShutdownCancelledCB,
		.client_data = NULL,
	},
};

static gboolean
on_ifd_watch(GIOChannel *chan, GIOCondition cond, pointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
	      return G_SOURCE_REMOVE;
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceProcessMessages(iceConn, NULL, NULL);
	}
	return G_SOURCE_CONTINUE;	/* keep event source */
}

void
init_proxy(void)
{
	char err[256] = { 0, };
	GIOChannel *chan;
	int ifd, mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	char *env;

	if (smcConn) {
		DPRINTF("already connected\n");
		return;
	}
	if (!(env = getenv("SESSION_MANAGER"))) {
		if (options.client_id)
			EPRINTF("clientId provided by no SESSION_MANAGER\n");
		return;
	}

	smcConn = SmcOpenConnection(env, NULL, SmProtoMajor, SmProtoMinor,
				    autoCBMask, &autoCBs, options.client_id,
				    &options.client_id, sizeof(err), err);

	if (!smcConn) {
		EPRINTF("SmcOpenConnection: %s\n", err);
		exit(EXIT_FAILURE);
	}

	iceConn = SmcGetIceConnection(smcConn);

	ifd = IceConnectionNumber(iceConn);
	chan = g_io_channel_unix_new(ifd);
	g_io_add_watch(chan, mask, on_ifd_watch, NULL);
}

gboolean
on_xfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		XEvent ev;

		while (XPending(dpy) && running) {
			XNextEvent(dpy, &ev);
			XPRINTF("Handling Event\n");
			handle_event(&ev);
		}
	}
	return TRUE;		/* keep event source */

}

static void
setup_main_loop(int argc, char *argv[])
{
	int xfd;
	GIOChannel *chan;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	guint srce;

	gtk_init(NULL, NULL);
	// gtk_init(argc, argv);

	// gdk_error_trap_push();

	/* do this after gtk_init() otherwise we get gdk_xerror() Xlib error
	   handler */
	init_display();

	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	chan = g_io_channel_unix_new(xfd);
	srce = g_io_add_watch(chan, mask, on_xfd_watch, NULL);
	(void) srce;
}

static void
announce_selection(void)
{
	XEvent ev;

	ev.xclient.type = ClientMessage;
	ev.xclient.serial = 0;
	ev.xclient.send_event = False;
	ev.xclient.display = dpy;
	ev.xclient.window = scr->root;
	ev.xclient.message_type = _XA_MANAGER;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;	/* FIXME */
	ev.xclient.data.l[1] = scr->slctn_atom;
	ev.xclient.data.l[2] = scr->selwin;
	ev.xclient.data.l[3] = 2;
	ev.xclient.data.l[4] = 0;

	XSendEvent(dpy, scr->root, False, StructureNotifyMask, (XEvent *) &ev);
	XSync(dpy, False);
}

static Bool
selectionreleased(Display *d, XEvent *e, XPointer arg)
{
	int *n = (typeof(n)) arg;

	if (e->type != DestroyNotify)
		return False;
	if (XFindContext(d, e->xdestroywindow.window, ScreenContext, (XPointer *) &scr))
		return False;
	if (!scr->owner || scr->owner != e->xdestroywindow.window)
		return False;
	scr->owner = None;
	*n = *n - 1;
	return True;
}

/** @brief run without replacing a running instance
  *
  * This is performed by detecting owners of the _XDE_AUTOSTART_S%d selection for
  * any screen and aborting when one exists.
  */
void
do_run(int argc, char *argv[])
{
	int s;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy,
										   s),
					BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if (!(scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
			XSync(dpy, False);
		}
		XUngrabServer(dpy);

		if (scr->owner) {
			EPRINTF("another instance is running on screen %d\n", s);
			exit(EXIT_FAILURE);
		}
		announce_selection();
	}
}

/** @brief replace running instance with this one
  *
  * This is performed by detecting owners of the _XDE_AUTOSTART_S%d selection for
  * each screen and setting the selection to our own window.  When the runing
  * instance detects the selection clear event for a managed screen, it will
  * destroy the selection window and exit when no more screens are managed.
  */
void
do_replace(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy,
										   s),
					BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			selcount++;
		}
		XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
		XSync(dpy, False);
		XUngrabServer(dpy);

		if (!scr->owner)
			announce_selection();
	}
	while (selcount) {
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
		announce_selection();
	}
}

/** @brief ask running instance to quit
  *
  * This is performed by detecting owners of the _XDE_AUTOSTART_S%d selection for
  * each screen and clearing the selection to None.  When the running instance
  * detects the selection clear event for a managed screen, it will destroy
  * the selection window and exit when no more screens are managed.
  */
void
do_quit(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = screens + s;
		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			XSetSelectionOwner(dpy, scr->slctn_atom, None, CurrentTime);
			XSync(dpy, False);
			selcount++;
		}
		XUngrabServer(dpy);
	}
	/* sure, wait for them all to destroy */
	while (selcount)
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
}

char **
get_data_dirs(int *np)
{
	char *home, *xhome, *xdata, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_DATA_HOME");
	xdata = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";

	len = (xhome ? strlen(xhome) : strlen(home) + strlen("/.local/share")) + strlen(xdata) + 2;
	dirs = calloc(len, sizeof(*dirs));
	if (xhome)
		strcpy(dirs, xhome);
	else {
		strcpy(dirs, home);
		strcat(dirs, "/.local/share");
	}
	strcat(dirs, ":");
	strcat(dirs, xdata);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end;
	     n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n + 1, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1)
		xdg_dirs[n] = strdup(pos);
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

char **
get_config_dirs(int *np)
{
	char *home, *xhome, *xconf, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_CONFIG_HOME");
	xconf = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";

	len = (xhome ? strlen(xhome) : strlen(home) + strlen("/.config")) + strlen(xconf) + 2;
	dirs = calloc(len, sizeof(*dirs));
	if (xhome)
		strcpy(dirs, xhome);
	else {
		strcpy(dirs, home);
		strcat(dirs, "/.config");
	}
	strcat(dirs, ":");
	strcat(dirs, xconf);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end;
	     n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n + 1, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1)
		xdg_dirs[n] = strdup(pos);
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

char **
get_autostart_dirs(int *np)
{
	char *home, *xhome, *xconf, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_CONFIG_HOME");
	xconf = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";

	len = (xconf ? strlen(xconf) : strlen(home) + strlen("/.config")) + strlen(xconf) + 2;
	dirs = calloc(len, sizeof(*dirs));
	if (xhome)
		strcpy(dirs, xhome);
	else {
		strcpy(dirs, home);
		strcat(dirs, "/.config");
	}
	strcat(dirs, ":");
	strcat(dirs, xconf);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end; n++,
	     *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1) {
		len = strlen(pos) + strlen("/autostart") + 1;
		xdg_dirs[n] = calloc(len, sizeof(*xdg_dirs[n]));
		strcpy(xdg_dirs[n], pos);
		strcat(xdg_dirs[n], "/autostart");
	}
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

static void
autostart_key_free(gpointer data)
{
	free(data);
}

static void
autostart_value_free(gpointer entry)
{
	g_key_file_free((GKeyFile *) entry);
}

static void
get_autostart_entry(GHashTable *autostarts, const char *key, const char *file)
{
	GKeyFile *entry;

	if (!(entry = g_key_file_new())) {
		EPRINTF("%s: could not allocate key file\n", file);
		return;
	}
	if (!g_key_file_load_from_file(entry, file, G_KEY_FILE_NONE, NULL)) {
		EPRINTF("%s: could not load keyfile\n", file);
		g_key_file_unref(entry);
		return;
	}
	if (!g_key_file_has_group(entry, G_KEY_FILE_DESKTOP_GROUP)) {
		EPRINTF("%s: has no [%s] section\n", file, G_KEY_FILE_DESKTOP_GROUP);
		g_key_file_free(entry);
		return;
	}
	if (!g_key_file_has_key(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
		EPRINTF("%s: has no Type= section\n", file);
		g_key_file_free(entry);
		return;
	}
	DPRINTF("got autostart file: %s (%s)\n", key, file);
	g_hash_table_replace(autostarts, strdup(key), entry);
	return;
}

/** @brief wean out entries that should not be used
  * @param key - pointer to the application id of the entry
  * @param value - pointer to the key file entry
  * @return gboolean - TRUE if removal required, FALSE otherwise
  */
static gboolean
autostarts_filter(gpointer key, gpointer value, gpointer user_data)
{
	char *appid = (char *) key;
	GKeyFile *entry = (GKeyFile *) value;
	gchar *name, *exec, *tryexec, *binary;
	gchar **item, **list, **desktop;

	if (!(name = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					   G_KEY_FILE_DESKTOP_KEY_NAME, NULL))) {
		DPRINTF("%s: no Name\n", appid);
		return TRUE;
	}
	g_free(name);
	if (!(exec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					   G_KEY_FILE_DESKTOP_KEY_EXEC, NULL))) {
		/* TODO: handle DBus activation */
		DPRINTF("%s: no Exec\n", appid);
		return TRUE;
	}
	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
				   G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL)) {
		DPRINTF("%s: is Hidden\n", appid);
		return TRUE;
	}
#if 0
	/* NoDisplay can be used to hide desktop entries from the application
	   menu and does not indicate that it should not be executed as an
	   autostart entry.  Use Hidden for that. */
	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
				   G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL)) {
		DPRINTF("%s: is NoDisplay\n", appid);
		return TRUE;
	}
#endif
	if ((list = g_key_file_get_string_list(entry, G_KEY_FILE_DESKTOP_GROUP,
					       G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL, NULL))) {
		desktop = NULL;
		for (item = list; *item; item++) {
			for (desktop = options.desktops; *desktop; desktop++)
				if (!strcmp(*item, *desktop))
					break;
			if (*desktop)
				break;
		}
		g_strfreev(list);
		if (!*desktop) {
			DPRINTF("%s: %s not in OnlyShowIn\n", appid, options.desktop);
			return TRUE;
		}
	}
	if ((list = g_key_file_get_string_list(entry, G_KEY_FILE_DESKTOP_GROUP,
					       G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL, NULL))) {
		desktop = NULL;
		for (item = list; *item; item++) {
			for (desktop = options.desktops; *desktop; desktop++)
				if (!strcmp(*item, *desktop))
					break;
			if (*desktop)
				break;
		}
		g_strfreev(list);
		if (*desktop) {
			DPRINTF("%s: %s in NotShowIn\n", appid, *desktop);
			return TRUE;
		}
	}
	if ((tryexec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					     G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL))) {
		binary = g_strdup(tryexec);
		g_free(tryexec);
	} else {
		char *p;

		/* parse the first word of the exec statement and see whether
		   it is executable or can be found in PATH */
		binary = g_strdup(exec);
		if ((p = strpbrk(binary, " \t")))
			*p = '\0';
	}
	g_free(exec);
	if (binary[0] == '/') {
		if (access(binary, X_OK)) {
			DPRINTF("%s: %s: %s\n", appid, binary, strerror(errno));
			DPRINTF("%s: not executable\n", appid);
			g_free(binary);
			return TRUE;
		}
	} else {
		char *dir, *end;
		char *path = strdup(getenv("PATH") ? : "");
		int blen = strlen(binary) + 2;
		gboolean execok = FALSE;

		for (dir = path, end = dir + strlen(dir); dir < end;
		     *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;
		for (dir = path; dir < end; dir += strlen(dir) + 1) {
			int len = strlen(dir) + blen;
			char *file = calloc(len, sizeof(*file));

			strcpy(file, dir);
			strcat(file, "/");
			strcat(file, binary);
			if (!access(file, X_OK)) {
				execok = TRUE;
				free(file);
				break;
			}
			// too much noise
			// DPRINTF("%s: %s: %s\n", appid, file,
			// strerror(errno);
		}
		free(path);
		if (!execok) {
			DPRINTF("%s: %s: not in executable in path\n", appid, binary);
			g_free(binary);
			return TRUE;
		}
	}
	return FALSE;
}

GHashTable *
get_autostarts(void)
{
	char **xdg_dirs, **dirs;
	int i, n = 0;
	static const char *suffix = ".desktop";
	static const int suflen = 8;

	if (!(xdg_dirs = get_autostart_dirs(&n)) || !n)
		return (autostarts);

	autostarts = g_hash_table_new_full(g_str_hash, g_str_equal,
					   autostart_key_free, autostart_value_free);

	/* got through them backward */
	for (i = n - 1, dirs = &xdg_dirs[i]; i >= 0; i--, dirs--) {
		char *file, *p;
		DIR *dir;
		struct dirent *d;
		int len;
		char *key;
		struct stat st;

		if (!(dir = opendir(*dirs))) {
			DPRINTF("%s: %s\n", *dirs, strerror(errno));
			continue;
		}
		while ((d = readdir(dir))) {
			if (d->d_name[0] == '.')
				continue;
			if (!(p = strstr(d->d_name, suffix)) || p[suflen]) {
				DPRINTF("%s: no %s suffix\n", d->d_name, suffix);
				continue;
			}
			len = strlen(*dirs) + strlen(d->d_name) + 2;
			file = calloc(len, sizeof(*file));
			strcpy(file, *dirs);
			strcat(file, "/");
			strcat(file, d->d_name);
			if (stat(file, &st)) {
				EPRINTF("%s: %s\n", file, strerror(errno));
				free(file);
				continue;
			}
			if (!S_ISREG(st.st_mode)) {
				EPRINTF("%s: %s\n", file, "not a regular file");
				free(file);
				continue;
			}
			key = strdup(d->d_name);
			*strstr(key, suffix) = '\0';
			get_autostart_entry(autostarts, key, file);
			free(key);
			free(file);
		}
		closedir(dir);
	}
	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
#if 0
	/* don't filter stuff out because we want to show non-autostarted
	   appids as insensitive buttons. */
	g_hash_table_foreach_remove(autostarts, autostarts_filter, NULL);
#endif
	return (autostarts);
}

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

void
create_splashscreen(TableContext * c)
{
	splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(splash), "xde-autostart", "XDE-Autostart");
#if 0
	unique_app_watch_window(uapp, GTK_WINDOW(splash));
	g_signal_connect(G_OBJECT(uapp), "message-received", G_CALLBACK(message_received),
			 (gpointer) NULL);
#endif
	gtk_window_set_gravity(GTK_WINDOW(splash), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(splash), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_container_set_border_width(GTK_CONTAINER(splash), 20);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(splash), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(splash), TRUE);
	gtk_window_set_keep_below(GTK_WINDOW(splash), TRUE);
	gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER_ALWAYS);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(splash), GTK_WIDGET(hbox));

	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);

	GtkWidget *img = gtk_image_new_from_file(options.banner);

	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(img), FALSE, FALSE, 0);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(GTK_WIDGET(sw), 800, -1);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(sw), TRUE, TRUE, 0);

	table = gtk_table_new(c->rows, c->cols, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 1);
	gtk_table_set_row_spacings(GTK_TABLE(table), 1);
	gtk_table_set_homogeneous(GTK_TABLE(table), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(table), 750, -1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(table));

	gtk_window_set_default_size(GTK_WINDOW(splash), -1, 600);
	gtk_widget_show_all(GTK_WIDGET(splash));
	gtk_widget_show_now(GTK_WIDGET(splash));
	relax();
}

static GtkWidget *
get_icon(GtkIconSize size, const char *name, const char *stock)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GtkWidget *image = NULL;

	if (!image && name && gtk_icon_theme_has_icon(theme, name))
		image = gtk_image_new_from_icon_name(name, size);
	if (!image && stock)
		image = gtk_image_new_from_stock(stock, size);
	return (image);
}

static gboolean
blink_button(gpointer user_data)
{
	TaskContext *task = (typeof(task)) user_data;

	if (!task->button) {
		task->blink = 0;
		return G_SOURCE_REMOVE;
	}
	if (gtk_widget_is_sensitive(task->button))
		gtk_widget_set_sensitive(task->button, FALSE);
	else
		gtk_widget_set_sensitive(task->button, TRUE);
	return G_SOURCE_CONTINUE;
}

void
add_task_to_splash(TableContext * c, TaskContext * task)
{
	char *name;
	GtkWidget *icon, *but;

	name = g_key_file_get_string(task->entry, G_KEY_FILE_DESKTOP_GROUP,
				     G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
	icon = get_icon(GTK_ICON_SIZE_DIALOG, name, "gtk-missing-image");

	name = g_key_file_get_locale_string(task->entry, G_KEY_FILE_DESKTOP_GROUP,
					    G_KEY_FILE_DESKTOP_KEY_NAME, options.language, NULL);
	if (!name)
		name = g_strdup(task->appid);

	but = task->button = gtk_button_new();
	gtk_button_set_image_position(GTK_BUTTON(but), GTK_POS_TOP);
	gtk_button_set_image(GTK_BUTTON(but), GTK_WIDGET(icon));
#if 1
	gtk_button_set_label(GTK_BUTTON(but), name);
#endif
	gtk_widget_set_tooltip_text(GTK_WIDGET(but), name);

	g_free(name);

	gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(but),
				  c->col, c->col + 1, c->row, c->row + 1);
	gtk_widget_set_sensitive(GTK_WIDGET(but), task->runnable);
	gtk_widget_show_all(GTK_WIDGET(but));
	gtk_widget_show_now(GTK_WIDGET(but));
	relax();

	if (++c->col >= c->cols) {
		c->row++;
		c->col = 0;
	}

	if (task->runnable)
		task->blink = g_timeout_add_seconds(1, blink_button, task);
}

void
do_autostarts()
{
	GHashTable *autostarts;
	TaskContext *task;
	GHashTableIter hiter;
	gpointer key, value;
	int count;
	TableContext *c = NULL;

	if (!(autostarts = get_autostarts())) {
		EPRINTF("cannot build AutoStarts\n");
		return;
	}
	if (!(count = g_hash_table_size(autostarts))) {
		EPRINTF("cannot fine any AutoStarts\n");
		return;
	}
	if (options.splash) {
		c = calloc(1, sizeof(*c));

		c->cols = 7;	/* seems like a good number */
		c->rows = (count + c->cols - 1) / c->cols;
		c->col = 0;
		c->row = 0;

		create_splashscreen(c);
	}

	g_hash_table_iter_init(&hiter, autostarts);
	while (g_hash_table_iter_next(&hiter, &key, &value)) {

		task = calloc(1, sizeof(*task));
		task->appid = key;
		task->entry = value;
		task->runnable = !autostarts_filter(key, value, NULL);

		if (options.splash)
			add_task_to_splash(c, task);

		if (!task->runnable) {
			free(task);
			continue;
		}

		/* FIXME: actually launch the task */

	}
}

/** @brief perform pre-autostart executions.
  */
void
do_executes()
{
}

/** @brief do run state startup
  *
  * RunStateStartup: the program is starting up and initializing.  Upon
  * completion move to RunStatePhase2.
  */
void
run_state_Startup(void)
{
	relax();
	runstate = RunStatePhase1;
}

static gboolean
phase1_timeout(gpointer user_data)
{
	gtk_main_quit();
	return G_SOURCE_REMOVE;
}

/** @brief do run state phase 1
  *
  * RunStatePhase1: waiting for a window manager to appear.  Upon expiry of the
  * guard timer or appearance of a window manager move to RunStatePhase2.  Set
  * the desktop session to the window manager name when one appears.  When none
  * appears, fall back to command-line options of default for desktop session.
  * Set DESKTOP_SESSION and XDG_CURRENT_DESKTOP environment variables.  Start
  * spawned thread to begin reading autostart database.
  */
void
run_state_Phase1(void)
{
	if (!check_window_manager() && options.wait) {
		ctx.timer = g_timeout_add_seconds(options.pause, phase1_timeout, NULL);
		gtk_main();
	}
}

/** @brief do run state phase 2
  */
void
run_state_Phase2(void)
{
}

/** @brief do run state phase 3
  */
void
run_state_Phase3(void)
{
}

/** @brief do run state phase 4
  */
void
run_state_Phase4(void)
{
}

/** @brief do run state phase 5
  */
void
run_state_Phase5(void)
{
}

/** @brief do run state monitoring
  */
void
run_state_Monitor(void)
{
}

void
run_state_machine(void)
{
	runstate = RunStateStartup;
	while (running) {
		switch (runstate) {
		case RunStateStartup:
			run_state_Startup();
			break;
		case RunStatePhase1:
			run_state_Phase1();
			break;
		case RunStatePhase2:
			run_state_Phase2();
			break;
		case RunStatePhase3:
			run_state_Phase3();
			break;
		case RunStatePhase4:
			run_state_Phase4();
			break;
		case RunStatePhase5:
			run_state_Phase5();
			break;
		case RunStateMonitor:
			run_state_Monitor();
			break;
		}
	}
}

void
run_autostart(int argc, char *argv[])
{
	/* FIXME: write me */

	setup_main_loop(argc, argv);
	do_executes();
	do_autostarts();

	gtk_main();
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
Copyright (c) 2008-2014  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>\n\
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
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014  Monavacon Limited.\n\
Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008  OpenSS7 Corporation.\n\
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
    %1$s [OPTIONS]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static const char *
show_side(LogoSide side)
{
	switch (side) {
	case LOGO_SIDE_LEFT:
		return ("left");
	case LOGO_SIDE_TOP:
		return ("top");
	case LOGO_SIDE_RIGHT:
		return ("right");
	case LOGO_SIDE_BOTTOM:
		return ("bottom");
	}
	return ("unknown");
}

static const char *
show_bool(Bool val)
{
	if (val)
		return ("true");
	return ("false");
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [OPTIONS]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    SESSION                (%17$s)\n\
        The name of the XDG session to execute, or \"default\"\n\
        or \"choose\".  When unspecified defaults to \"default\"\n\
        \"default\" means execute default without prompting\n\
        \"choose\" means post a dialog to choose the session\n\
	[default: %18$s] [current: %19$s]\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -d, --display DISPLAY               (%2$s)\n\
        specify the X display to use\n\
    -e, --desktop DESKTOP               (%3$s)\n\
        specify the desktop environment\n\
    -s, --session SESSION               (%4$s)\n\
        specify the lxsesion(1) compatible session\n\
    -x, --exec COMMAND\n\
        specify commands to execute before autostart\n\
        this option may be repeated\n\
    -f, --file FILE                     (%5$s)\n\
        specify file from which to read commands\n\
    -a, --noautostart | --autostart     (%6$s)\n\
        (do not) perform XDG autostart of tasks\n\
    -w, --wait | --nowait               (%7$s)\n\
        wait for window manager to start\n\
    -p, --pause [PAUSE]                 (%8$u sec)\n\
        pause for PAUSE seconds before running\n\
    -l, --splash [IMAGE] | --nosplash   (%9$s)\n\
        display (or not) startup splash page\n\
    -W, --toolwait [GUARD][:DELAY]      (%10$u:%11$u)\n\
        perform xtoolwait on autostart tasks\n\
        GUARD is the longest time to wait for tool start\n\
        DELAY is the time to wait for unknown startup\n\
    -c, --charset CHARSET               (%12$s)\n\
        specify the character set\n\
    -L, --language LANG                 (%13$s)\n\
        specify the language for XDG translations\n\
    -b, --banner BANNER                 (%14$s)\n\
        specify custom autostart branding\n\
    -S, --side {left|right|top|bottom}  (%15$s)\n\
        specify side of splash for logo placement\n\
    -i, --icons THEME                   (%16$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME                   (%17$s)\n\
        set the GTK+ theme to use\n\
    -X, --xde-theme                     (%18$s)\n\
        use the XDE desktop theme for the selection window\n\
    -F, --foreground                    (%19$s)\n\
        execute in the foreground for debugging\n\
    --client-id CLIENTID                (%20$s)\n\
        client ID for X Session Management\n\
    -n, --dry-run                       (%21$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]                 (%22$d)\n\
        increment or set debug LEVEL\n\
        this option may be repeated.\n\
    -v, --verbose [LEVEL]               (%23$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
", argv[0]
		       , options.display, options.desktop, options.session ? : "",
		       options.file ? : "", show_bool(options.autostart)
		       , show_bool(options.wait)
		       , options.pause, options.splash ? options.banner : "false", options.guard,
		       options.delay, options.charset, options.language, options.banner,
		       show_side(options.side)
		       , options.icon_theme ? : "auto", options.gtk2_theme ? : "auto",
		       show_bool(options.usexde)
		       , show_bool(options.foreground)
		       , options.client_id ? : "", show_bool(options.dryrun)
		       , options.debug, options.output);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n)
		return;

	free(options.banner);
	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	for (pfx = getenv("XDG_MENU_PREFIX") ? : ""; pfx; pfx = *pfx ? "" : NULL) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "banner", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					options.banner = strdup(file);
					break;
				}
			}
			if (options.banner)
				break;
		}
		if (options.banner)
			break;
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

/** @brief set default file list
  *
  * This can only be run after options processing...
  */
void
set_default_file(void)
{
	char **xdg_dirs, **dirs, *file, *files;
	int i, size, n = 0, next;

	if (options.file)
		return;
	if (!options.session)
		return;

	if (!(xdg_dirs = get_config_dirs(&n)) || !n)
		return;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	files = NULL;
	size = 0;
	next = 0;

	/* process in reverse order */
	for (i = n - 1, dirs = &xdg_dirs[i]; i >= 0; i--, dirs--) {
		strncpy(file, *dirs, PATH_MAX);
		strncat(file, "/lxsession/", PATH_MAX);
		strncat(file, options.session, PATH_MAX);
		strncat(file, "/autostart", PATH_MAX);
		if (access(file, R_OK)) {
			DPRINTF("%s: %s\n", file, strerror(errno));
			continue;
		}
		size += strlen(file) + 1;
		files = realloc(files, size * sizeof(*files));
		if (next)
			strncat(files, ":", size);
		else {
			*files = '\0';
			next = 1;
		}
		strncat(files, file, size);
	}
	options.file = files;

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
split_desktops(void)
{
	char **desktops, *copy, *pos, *end;;
	int n;

	copy = strdup(options.desktop);

	for (n = 0, pos = copy, end = pos + strlen(pos); pos < end;
	     n++, *strchrnul(pos, ';') = '\0', pos += strlen(pos) + 1) ;

	desktops = calloc(n + 1, sizeof(*desktops));

	for (n = 0, pos = copy; pos < end; n++, pos += strlen(pos) + 1)
		desktops[n] = strdup(pos);

	free(copy);

	free(options.desktops);
	options.desktops = desktops;
}

void
set_defaults(void)
{
	char *p, *a;
	const char *env;

	set_default_banner();
	if ((options.language = setlocale(LC_ALL, ""))) {
		options.language = strdup(options.language);
		a = strchrnul(options.language, '@');
		if ((p = strchr(options.language, '.')))
			strcpy(p, a);
	}
	options.charset = strdup(nl_langinfo(CODESET));
	options.display = strdup(getenv("DISPLAY") ? : "");
	if ((env = getenv("XDG_CURRENT_DESKTOP")))
		options.desktop = strdup(env);
	else
		options.desktop = strdup("XDE");
}

typedef enum {
	COMMAND_AUTOSTART,
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_COPYING,
} CommandType;

int
main(int argc, char *argv[])
{
	CommandType command = COMMAND_AUTOSTART;

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"display",	required_argument,	NULL, 'd'},
			{"desktop",	required_argument,	NULL, 'e'},
			{"session",	required_argument,	NULL, 's'},
			{"exec",	required_argument,	NULL, 'x'},
			{"file",	required_argument,	NULL, 'f'},
			{"noautostart",	no_argument,		NULL, 'a'},
			{"autostart",	no_argument,		NULL, '1'},
			{"wait",	no_argument,		NULL, 'w'},
			{"nowait",	no_argument,		NULL, '2'},
			{"pause",	optional_argument,	NULL, 'p'},
			{"splash",	optional_argument,	NULL, 'l'},
			{"nosplash",	no_argument,		NULL, '4'},

			{"toolwait",	optional_argument,	NULL, 'W'},

			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'L'},
			{"banner",	required_argument,	NULL, 'b'},
			{"side",	required_argument,	NULL, 'S'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"xde-theme",	no_argument,		NULL, 'X'},

			{"foreground",	no_argument,		NULL, 'F'},
			{"client-id",	required_argument,	NULL, '3'},

			{"dryrun",	no_argument,		NULL, 'n'},
			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "d:e:s:x:f:awp::l::W::c:L:b:S:i:t:XFnD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "d:e:s:x:f:a1w2p:l:4W:c:L:b:S:i:t:XF3:nDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'd':	/* -d, --display DISPLAY */
			free(options.display);
			options.display = strdup(optarg);
			setenv("DISPLAY", optarg, 1);
			break;
		case 'e':	/* -e, --desktop DESKTOP */
			free(options.desktop);
			options.desktop = strdup(optarg);
			break;
		case 's':	/* -s, --session SESSION */
			free(options.session);
			options.session = strdup(optarg);
			break;
		case 'x':	/* -x, --exec COMMAND */
			options.execute = realloc(options.execute, (options.commands + 1) *
						  sizeof(*options.execute));
			options.execute[options.commands++] = strdup(optarg);
			break;
		case 'f':	/* -f, --file FILE */
			free(options.file);
			options.file = strdup(optarg);
			break;
		case 'a':	/* -a, --noautostart */
			options.autostart = False;
			break;
		case '2':	/* --autostart */
			options.autostart = True;
			break;
		case 'p':	/* -p, --pause [PAUSE] */
			if (optarg)
				options.pause = strtoul(optarg, NULL, 0);
			break;
		case 'l':	/* -l, --splash [IMAGE] */
			if (optarg) {
				free(options.banner);
				options.banner = strdup(optarg);
			}
			options.splash = True;
			break;

		case '4':	/* --nosplash */
			options.splash = False;
			break;
		case 'W':	/* -W, --toolwait [GUARD][:DELAY] */
			if (optarg) {
				char *g, *d;

				g = strdup(optarg);
				d = strchrnul(g, ':');
				if (*d) {
					*d++ = '\0';
					options.delay = strtoul(d, NULL, 0);
				}
				if (*g) {
					options.guard = strtoul(g, NULL, 0);
				}
			}
			break;
		case 'c':	/* -c --charset CHARSET */
			free(options.charset);
			options.charset = strdup(optarg);
			break;
		case 'L':	/* -L, --language LANG */
			free(options.language);
			options.language = strdup(optarg);
			break;
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			break;
		case 'S':	/* -S, --side {top|bottom|left|right} */
			if (!strncasecmp(optarg, "left", strlen(optarg))) {
				options.side = LOGO_SIDE_LEFT;
				break;
			}
			if (!strncasecmp(optarg, "top", strlen(optarg))) {
				options.side = LOGO_SIDE_TOP;
				break;
			}
			if (!strncasecmp(optarg, "right", strlen(optarg))) {
				options.side = LOGO_SIDE_RIGHT;
				break;
			}
			if (!strncasecmp(optarg, "bottom", strlen(optarg))) {
				options.side = LOGO_SIDE_BOTTOM;
				break;
			}
			goto bad_option;
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 'X':	/* -X, --xde-theme */
			options.usexde = True;
			break;
		case 'F':	/* -F, --foreground */
			options.foreground = True;
			break;
		case '3':	/* --client-id CLIENTID */
			free(options.client_id);
			options.client_id = strdup(optarg);
			break;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
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
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_HELP;
			break;
		case 'V':	/* -V, --version */
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_VERSION;
			break;
		case 'C':	/* -C, --copying */
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_COPYING;
			break;
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '", argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", argv[0]);
		goto bad_nonopt;
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	set_default_file();
	split_desktops();
	switch (command) {
	case COMMAND_AUTOSTART:
		if (options.debug)
			fprintf(stderr, "%s: running autostart\n", argv[0]);
		run_autostart(argc, argv);
		break;
	case COMMAND_HELP:
		if (options.debug)
			fprintf(stderr, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case COMMAND_VERSION:
		if (options.debug)
			fprintf(stderr, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case COMMAND_COPYING:
		if (options.debug)
			fprintf(stderr, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(0);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
