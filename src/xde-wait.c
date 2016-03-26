/*****************************************************************************

 Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>
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
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
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


#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#undef EXIT_SYNTAXERR

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define EXIT_SYNTAXERR	2

typedef enum {
	CommandDefault,
	CommandWait,
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
	Bool nowait;
	Bool manager;
	Bool systray;
	Bool pager;
	Bool composite;
	Bool audio;
	Time delay;
	char **eargv;
	int eargc;
	Bool info;
	Bool all;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.command = CommandDefault,
	.display = NULL,
	.screen = 0,
	.nowait = False,
	.manager = True,
	.systray = False,
	.pager = False,
	.composite = False,
	.audio = False,
	.delay = 2000,
	.eargv = NULL,
	.eargc = 0,
	.info = False,
	.all = False,
};

Display *dpy = NULL;
int screen = 0;
Window root = None;
volatile Bool running = True;

typedef struct {
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;
	Window stray_owner;		/* _NET_SYSTEM_TRAY_S%d owner */
	Window pager_owner;		/* _NET_DESKTOP_LAYOUT_S%d owner */
	Window compm_owner;		/* _NET_WM_CM_S%d owner */
	Window shelp_owner;		/* _XDG_ASSIST_S%d owner */
	Window audio_owner;		/* PULSE_SERVER owner or root */
} WindowManager;

WindowManager wm;

Atom _XA_KDE_WM_CHANGE_STATE;
Atom _XA_MANAGER;
Atom _XA_MOTIF_WM_INFO;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_DESKTOP_LAYOUT;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_SYSTEM_TRAY_ORIENTATION;
Atom _XA_NET_SYSTEM_TRAY_VISUAL;
Atom _XA_NET_VIRTUAL_ROOTS;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_XDG_ASSIST_CMD;
Atom _XA_XDG_ASSIST_CMD_QUIT;
Atom _XA_XDG_ASSIST_CMD_REPLACE;
Atom _XA_PULSE_COOKIE;
Atom _XA_PULSE_SERVER;
Atom _XA_PULSE_ID;

static Bool handle_MANAGER(XEvent *);
static Bool handle_MOTIF_WM_INFO(XEvent *);
static Bool handle_NET_ACTIVE_WINDOW(XEvent *);
static Bool handle_NET_CLIENT_LIST(XEvent *);
static Bool handle_NET_DESKTOP_LAYOUT(XEvent *);
static Bool handle_NET_SUPPORTED(XEvent *);
static Bool handle_NET_SUPPORTING_WM_CHECK(XEvent *);
static Bool handle_NET_SYSTEM_TRAY_ORIENTATION(XEvent *);
static Bool handle_NET_SYSTEM_TRAY_VISUAL(XEvent *);
static Bool handle_WIN_CLIENT_LIST(XEvent *);
static Bool handle_WINDOWMAKER_NOTICEBOARD(XEvent *);
static Bool handle_WIN_PROTOCOLS(XEvent *);
static Bool handle_WIN_SUPPORTING_WM_CHECK(XEvent *);
static Bool handle_WIN_WORKSPACE(XEvent *);
static Bool handle_PULSE_COOKIE(XEvent *);
static Bool handle_PULSE_SERVER(XEvent *);
static Bool handle_PULSE_ID(XEvent *);

struct atoms {
	char *name;
	Atom *atom;
	Bool (*handler) (XEvent *);
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global					handler					atom value		*/
	/* ----					------					-------					----------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,		NULL,					None			},
	{ "MANAGER",				&_XA_MANAGER,				&handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,			&handle_MOTIF_WM_INFO,			None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,			&handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,			&handle_NET_CLIENT_LIST,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,		NULL,					None			},
	{ "_NET_DESKTOP_LAYOUT",		&_XA_NET_DESKTOP_LAYOUT,		&handle_NET_DESKTOP_LAYOUT,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,			&handle_NET_SUPPORTED,			None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,		&handle_NET_SUPPORTING_WM_CHECK,	None			},
	{ "_NET_SYSTEM_TRAY_ORIENTATION",	&_XA_NET_SYSTEM_TRAY_ORIENTATION,	&handle_NET_SYSTEM_TRAY_ORIENTATION,	None			},
	{ "_NET_SYSTEM_TRAY_VISUAL",		&_XA_NET_SYSTEM_TRAY_VISUAL,		&handle_NET_SYSTEM_TRAY_VISUAL,		None			},
	{ "_NET_VIRTUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,			NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,		NULL,					None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,			NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,			NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,			&handle_WIN_CLIENT_LIST,		None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,		&handle_WINDOWMAKER_NOTICEBOARD,	None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,			&handle_WIN_PROTOCOLS,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,		&handle_WIN_SUPPORTING_WM_CHECK,	None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,			&handle_WIN_WORKSPACE,			None			},
	{ "_XDG_ASSIST_CMD_QUIT",		&_XA_XDG_ASSIST_CMD_QUIT,		NULL,					None			},
	{ "_XDG_ASSIST_CMD_REPLACE",		&_XA_XDG_ASSIST_CMD_REPLACE,		NULL,					None			},
	{ "_XDG_ASSIST_CMD",			&_XA_XDG_ASSIST_CMD,			NULL,					None			},
	{ "PULSE_COOKIE",			&_XA_PULSE_COOKIE,			&handle_PULSE_COOKIE,			None			},
	{ "PULSE_SERVER",			&_XA_PULSE_SERVER,			&handle_PULSE_SERVER,			None			},
	{ "PULSE_ID",				&_XA_PULSE_ID,				&handle_PULSE_ID,			None			},
	{ NULL,					NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

void
intern_atoms()
{
	int i, j, n;
	char **atom_names;
	Atom *atom_values;
	char *name;

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

	for (i = 0; atoms[i].name; i++) {
		if (atoms[i].value == None)
			EPRINTF("Entry (%d)%s not assigned an atom!\n", i, atoms[i].name);
		else if ((name = XGetAtomName(dpy, atoms[i].value))) {
			if (strcmp(name, atoms[i].name))
				EPRINTF("Entry (%d)%s atom %lu named %s!\n", i, atoms[i].name, atoms[i].value, name);
			XFree(name);
		} else
			EPRINTF("Entry (%d)%s atom %lu has no name!\n", i, atoms[i].name, atoms[i].value);
	}
}

int
handler(Display *display, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, "xdg-launch", num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%08lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

int
iohandler(Display *display)
{
	void *buffer[1024];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 1023)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, "backtrace> %s\n", strings[i]);
	exit(EXIT_FAILURE);
}

XContext ClientContext;			/* window to client context */
XContext MessageContext;		/* window to message context */

int (*oldhandler) (Display *, XErrorEvent *) = NULL;
int (*oldiohandler) (Display *) = NULL;

Bool
get_display()
{
	PTRACE();
	if (!dpy) {
		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		oldhandler = XSetErrorHandler(handler);
		oldiohandler = XSetIOErrorHandler(iohandler);
		for (screen = 0; screen < ScreenCount(dpy); screen++) {
			root = RootWindow(dpy, screen);
			XSelectInput(dpy, root,
				     VisibilityChangeMask | StructureNotifyMask |
				     SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
		screen = DefaultScreen(dpy);
		root = RootWindow(dpy, screen);
		intern_atoms();

		ClientContext = XUniqueContext();
		MessageContext = XUniqueContext();
	}
	return (dpy ? True : False);
}

void
put_display()
{
	PTRACE();
	if (dpy) {
		XSetErrorHandler(oldhandler);
		XSetIOErrorHandler(oldiohandler);
		XCloseDisplay(dpy);
		dpy = NULL;
	}
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
	if (XGetWindowProperty(dpy, win, prop, 0L, num, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success
	    && format != 0) {
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

#if 0
static Window *
get_windows(Window win, Atom prop, Atom type, long *n)
{
	return (Window *) get_cardinals(win, prop, type, n);
}
#endif

#if 0
static Bool
get_window(Window win, Atom prop, Atom type, Window *win_ret)
{
	return get_cardinal(win, prop, type, (long *) win_ret);
}
#endif

#if 0
static Time *
get_times(Window win, Atom prop, Atom type, long *n)
{
	return (Time *) get_cardinals(win, prop, type, n);
}
#endif

#if 0
static Bool
get_time(Window win, Atom prop, Atom type, Time *time_ret)
{
	return get_cardinal(win, prop, type, (long *) time_ret);
}
#endif

#if 0
static Atom *
get_atoms(Window win, Atom prop, Atom type, long *n)
{
	return (Atom *) get_cardinals(win, prop, type, n);
}
#endif

Bool
get_atom(Window win, Atom prop, Atom type, Atom *atom_ret)
{
	return get_cardinal(win, prop, type, (long *) atom_ret);
}

Window
check_recursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check;

	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check, StructureNotifyMask | PropertyChangeMask);
			XFree(data);
			data = NULL;
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty(dpy, check, atom, 0L, 1L, False, type, &real,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success && format != 0) {
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

Window
check_nonrecursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check = None;

	OPRINTF("non-recursive check for atom 0x%08lx\n", atom);

	if (XGetWindowProperty(dpy, root, atom, 0L, 1L, False, type, &real,
			       &format, &nitems, &after,
			       (unsigned char **) &data) == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]))
				XSelectInput(dpy, check, StructureNotifyMask | PropertyChangeMask);
		}
		if (data)
			XFree(data);
	}
	return check;
}

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
	if (XGetWindowProperty(dpy, root, protocols, 0L, num, False,
			       XA_ATOM, &real, &format, &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			DPRINTF("trying harder with num = %lu\n", num);
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
		if (data)
			XFree(data);
	}
	return result;
}

static Window
check_netwm_supported()
{
	if (check_supported(_XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK))
		return root;
	return check_nonrecursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

static Window
check_netwm()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	win = win ? : check_netwm_supported();
	if (win && win != wm.netwm_check)
		DPRINTF("NetWM/EWMH changed from 0x%08lx to 0x%08lx\n", wm.netwm_check, win);
	if (!win && wm.netwm_check)
		DPRINTF("NetWM/EWMH removed from 0x%08lx\n", wm.netwm_check);
	return (wm.netwm_check = win);
}

static Window
check_winwm_supported()
{
	if (check_supported(_XA_WIN_PROTOCOLS, _XA_WIN_SUPPORTING_WM_CHECK))
		return root;
	return check_nonrecursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

static Window
check_winwm()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	win = win ? : check_winwm_supported();
	if (win && win != wm.winwm_check)
		DPRINTF("WinWM/WMH changed from 0x%08lx to 0x%08lx\n", wm.winwm_check, win);
	if (!win && wm.winwm_check)
		DPRINTF("WinWM/WMH removed from 0x%08lx\n", wm.winwm_check);
	return (wm.winwm_check = win);
}

static Window
check_maker()
{
	int i = 0;
	Window win;

	do {
		if ((win = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW)))
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	} while (i++ < 2 && !win);
	if (win && win != wm.maker_check)
		DPRINTF("WindowMaker changed from 0x%08lx to 0x%08lx\n", wm.maker_check, win);
	if (!win && wm.maker_check)
		DPRINTF("WindowMaker removed from 0x%08lx\n", wm.maker_check);
	return (wm.maker_check = win);
}

static Window
check_motif()
{
	int i = 0;
	long *data, n = 0;
	Window win = None;

	do {
		if ((data = get_cardinals(root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n))) {
			if (n >= 2 && (win = data[1]))
				XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			XFree(data);
		}
	} while (i++ < 2 && !data);
	if (win && win != wm.motif_check)
		DPRINTF("OSF/MOTIF changed from 0x%08lx to 0x%08lx\n", wm.motif_check, win);
	if (!win && wm.motif_check)
		DPRINTF("OSF/MOTIF removed from 0x%08lx\n", wm.motif_check);
	return (wm.motif_check = win);
}

static Window
check_icccm()
{
	char buf[32];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "WM_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != wm.icccm_check)
		DPRINTF("ICCCM 2.0 WM changed from 0x%08lx to 0x%08lx\n", wm.icccm_check, win);
	if (!win && wm.icccm_check)
		DPRINTF("ICCCM 2.0 WM removed from 0x%08lx\n", wm.icccm_check);
	return (wm.icccm_check = win);
}

static Window
check_redir()
{
	XWindowAttributes wa;
	Window win = None;

	OPRINTF("checking redirection for screen %d\n", screen);
	if (XGetWindowAttributes(dpy, root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			win = root;
	if (win && win != wm.redir_check)
		DPRINTF("WM redirection changed from 0x%08lx to 0x%08lx\n", wm.redir_check, win);
	if (!win && wm.redir_check)
		DPRINTF("WM redirection removed from 0x%08lx\n", wm.redir_check);
	return (wm.redir_check = win);
}

static Window
check_anywm()
{
	if (wm.netwm_check)
		return True;
	if (wm.winwm_check)
		return True;
	if (wm.maker_check)
		return True;
	if (wm.motif_check)
		return True;
	if (wm.icccm_check)
		return True;
	if (wm.redir_check)
		return True;
	return False;
}

static Bool
check_window_manager()
{
	OPRINTF("checking wm compliance for screen %d\n", screen);
	OPRINTF("checking redirection\n");
	if (check_redir())
		OPRINTF("redirection on window 0x%08lx\n", wm.redir_check);
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm())
		OPRINTF("ICCCM 2.0 window 0x%08lx\n", wm.icccm_check);
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif())
		OPRINTF("OSF/Motif window 0x%08lx\n", wm.motif_check);
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker())
		OPRINTF("WindowMaker window 0x%08lx\n", wm.maker_check);
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm())
		OPRINTF("GNOME/WMH window 0x%08lx\n", wm.winwm_check);
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm())
		OPRINTF("NetWM/EWMH window 0x%08lx\n", wm.netwm_check);
	return check_anywm();
}

static Bool
handle_wmchange(XEvent *e)
{
	if (!e || e->type == PropertyNotify)
		if (!check_window_manager())
			check_window_manager();
	return True;
}

static Bool
handle_WINDOWMAKER_NOTICEBOARD(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_MOTIF_WM_INFO(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_NET_SUPPORTING_WM_CHECK(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_NET_SUPPORTED(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_WIN_SUPPORTING_WM_CHECK(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_WIN_PROTOCOLS(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_NET_ACTIVE_WINDOW(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_NET_CLIENT_LIST(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_WIN_CLIENT_LIST(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

static Bool
handle_WIN_WORKSPACE(XEvent *e)
{
	PTRACE();
	return handle_wmchange(e);
}

Bool
handle_DestroyNotify(XEvent *e)
{
	Bool handled = False;

	if (!e || e->type != DestroyNotify) {
		EPRINTF("bad event!\n");
		return False;
	}
	if (wm.netwm_check && e->xdestroywindow.window == wm.netwm_check) {
		check_netwm();
		handled = True;
	}
	if (wm.winwm_check && e->xdestroywindow.window == wm.winwm_check) {
		check_winwm();
		handled = True;
	}
	if (wm.maker_check && e->xdestroywindow.window == wm.maker_check) {
		check_maker();
		handled = True;
	}
	if (wm.motif_check && e->xdestroywindow.window == wm.motif_check) {
		check_motif();
		handled = True;
	}
	if (wm.icccm_check && e->xdestroywindow.window == wm.icccm_check) {
		check_icccm();
		handled = True;
	}
	if (wm.redir_check && e->xdestroywindow.window == wm.redir_check) {
		check_redir();
		handled = True;
	}
	return handled;
}

Bool
handle_atom(XEvent *e, Atom atom)
{
	struct atoms *a;
	char *name;

	PTRACE();
	for (a = atoms; a->name; a++)
		if (a->value == atom) {
			if (a->handler)
				return a->handler(e);
			break;
		}
	if ((name = XGetAtomName(dpy, atom))) {
		DPRINTF("0x%08lx %s atom unhandled!\n", e->xany.window, name);
		XFree(name);
	}
	return False;
}

void
handle_event(XEvent *e)
{
	switch (e->type) {
	case PropertyNotify:
		handle_atom(e, e->xproperty.atom);
		break;
	case DestroyNotify:
		handle_DestroyNotify(e);
		break;
	case ClientMessage:
		handle_atom(e, e->xclient.message_type);
		break;
	default:
		break;
	}
}

volatile int signum = 0;

void
sighandler(int sig)
{
	signum = sig;
}

Bool
wait_for_condition(Window (*until) (void))
{
	int xfd;
	XEvent ev;
	struct itimerval it;

	PTRACE();
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGALRM, sighandler);

	/* main event loop */
	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	it.it_value.tv_sec = options.delay / 1000;
	it.it_value.tv_usec = (options.delay % 1000) * 1000;
	setitimer(ITIMER_REAL, &it, NULL);
	signum = 0;
	while (running) {
		struct pollfd pfd = { xfd, POLLIN | POLLHUP | POLLERR, 0 };

		if (signum) {
			if (signum == SIGALRM) {
				OPRINTF("waiting for resource timed out!\n");
				return True;
			}
			exit(EXIT_SUCCESS);
		}
		if (poll(&pfd, 1, -1) == -1) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
			case ERESTART:
				continue;
			}
			EPRINTF("poll: %s\n", strerror(errno));
			fflush(stderr);
			exit(EXIT_FAILURE);
		}
		if (pfd.revents & (POLLNVAL | POLLHUP | POLLERR)) {
			EPRINTF("poll: error\n");
			fflush(stderr);
			exit(EXIT_FAILURE);
		}
		if (pfd.revents & (POLLIN)) {
			while (XPending(dpy) && running && !signum) {
				XNextEvent(dpy, &ev);
				handle_event(&ev);
				if (until())
					return True;
			}
		}
	}
	return False;
}

static Bool
check_for_window_manager()
{
	PTRACE();
	OPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm())
		OPRINTF("NetWM/EWMH window 0x%08lx\n", wm.netwm_check);
	OPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm())
		OPRINTF("GNOME/WMH window 0x%08lx\n", wm.winwm_check);
	OPRINTF("checking WindowMaker compliance\n");
	if (check_maker())
		OPRINTF("WindowMaker window 0x%08lx\n", wm.maker_check);
	OPRINTF("checking OSF/Motif compliance\n");
	if (check_motif())
		OPRINTF("OSF/Motif window 0x%08lx\n", wm.motif_check);
	OPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm())
		OPRINTF("ICCCM 2.0 window 0x%08lx\n", wm.icccm_check);
	OPRINTF("checking redirection\n");
	if (check_redir())
		OPRINTF("redirection on window 0x%08lx\n", wm.redir_check);
	return check_anywm();
}

static void
wait_for_window_manager()
{
	PTRACE();
	if (check_for_window_manager()) {
		if (options.info) {
			fputs("Have a window manager:\n\n", stdout);
			if (wm.netwm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window NetWM/EWMH",
					wm.netwm_check);
			if (wm.winwm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window WinWM/GNOME",
					wm.winwm_check);
			if (wm.maker_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window WindowMaker",
					wm.maker_check);
			if (wm.motif_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window OSF/MOTIF",
					wm.motif_check);
			if (wm.icccm_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window ICCCM 2.0",
					wm.icccm_check);
			if (wm.redir_check)
				fprintf(stdout, "%-24s = 0x%08lx\n", "Window redirection",
					wm.redir_check);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for window manager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_anywm);
}

static Window
check_stray()
{
	char buf[64];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel))) {
		if (win != wm.stray_owner) {
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("system tray changed from 0x%08lx to 0x%08lx\n", wm.stray_owner,
				win);
			wm.stray_owner = win;
		}
	} else if (wm.stray_owner) {
		DPRINTF("system tray removed from 0x%08lx\n", wm.stray_owner);
		wm.stray_owner = None;
	}
	return wm.stray_owner;
}

static Bool
handle_NET_SYSTEM_TRAY_ORIENTATION(XEvent *e)
{
	PTRACE();
	check_stray();
	return True;
}

static Bool
handle_NET_SYSTEM_TRAY_VISUAL(XEvent *e)
{
	PTRACE();
	check_stray();
	return True;
}

static void
wait_for_system_tray()
{
	PTRACE();
	if (check_stray()) {
		if (options.info) {
			fputs("Have a system tray:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "System Tray window", wm.stray_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for system tray\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_stray);
}

static Window
check_pager()
{
	char buf[64];
	Atom sel;
	Window win;
	long *cards, n = 0;

	snprintf(buf, sizeof(buf), "_NET_DESKTOP_LAYOUT_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	/* selection only held while setting _NET_DESKTOP_LAYOUT */
	if (!win && (cards = get_cardinals(root, _XA_NET_DESKTOP_LAYOUT, XA_CARDINAL, &n))
	    && n >= 4) {
		XFree(cards);
		win = root;
	}
	if (win && win != wm.pager_owner)
		DPRINTF("desktop pager changed from 0x%08lx to 0x%08lx\n", wm.pager_owner, win);
	if (!win && wm.pager_owner)
		DPRINTF("desktop pager removed from 0x%08lx\n", wm.pager_owner);
	return (wm.pager_owner = win);
}

static Bool
handle_NET_DESKTOP_LAYOUT(XEvent *e)
{
	PTRACE();
	check_pager();
	return True;
}

static void
wait_for_desktop_pager()
{
	PTRACE();
	if (check_pager()) {
		if (options.info) {
			fputs("Have a desktop pager:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "Desktop pager window",
				wm.pager_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for desktop pager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_pager);
}

static Window
check_compm()
{
	char buf[64];
	Atom sel;
	Window win;

	snprintf(buf, sizeof(buf), "_NET_WM_CM_S%d", screen);
	sel = XInternAtom(dpy, buf, True);
	if ((win = XGetSelectionOwner(dpy, sel)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != wm.compm_owner)
		DPRINTF("composite manager changed from 0x%08lx to 0x%08lx\n", wm.compm_owner, win);
	if (!win && wm.compm_owner)
		DPRINTF("composite manager removed from 0x%08lx\n", wm.compm_owner);
	return (wm.compm_owner = win);
}

static void
wait_for_composite_manager()
{
	PTRACE();
	if (check_compm()) {
		if (options.info) {
			fputs("Have a composite manager:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "Composite manager window",
				wm.compm_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for composite manager\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_compm);
}

static Window
check_audio()
{
	char *s;

	if (!(s = get_text(root, _XA_PULSE_COOKIE)))
		return (wm.audio_owner = None);
	XFree(s);
	if (!(s = get_text(root, _XA_PULSE_SERVER)))
		return (wm.audio_owner = None);
	XFree(s);
	if (!(s = get_text(root, _XA_PULSE_ID)))
		return (wm.audio_owner = None);
	XFree(s);
	return (wm.audio_owner = root);
}

static void
wait_for_audio_server()
{
	PTRACE();
	if (check_audio()) {
		if (options.info) {
			fputs("Have an audio server:\n\n", stdout);
			fprintf(stdout, "%-24s = 0x%08lx\n", "Audio server window", wm.audio_owner);
			fputs("\n", stdout);
		}
		return;
	} else {
		if (options.info) {
			fputs("Would wait for audio server\n", stdout);
			return;
		}
	}
	wait_for_condition(&check_audio);
}

static Bool
handle_PULSE_COOKIE(XEvent *e)
{
	check_audio();
	return True;
}

static Bool
handle_PULSE_SERVER(XEvent *e)
{
	check_audio();
	return True;
}

static Bool
handle_PULSE_ID(XEvent *e)
{
	check_audio();
	return True;
}

static Bool
handle_MANAGER(XEvent *e)
{
	PTRACE();
	check_compm();
	check_pager();
	check_stray();
	check_icccm();
	return True;
}

static void
wait_for_resource()
{
	if (options.manager || options.systray || options.pager || options.composite || options.audio) {
		if (options.manager)
			wait_for_window_manager();
		if (options.systray)
			wait_for_system_tray();
		if (options.pager)
			wait_for_desktop_pager();
		if (options.composite)
			wait_for_composite_manager();
		if (options.audio)
			wait_for_audio_server();
	} else
		DPRINTF("No resource wait requested.\n");
}


static void
do_wait(int argc, char *argv[])
{
	if (!get_display()) {
		EPRINTF("cannot obtain DISPLAY\n");
		exit(EXIT_FAILURE);
	}
	wait_for_resource();
	if (options.eargc && options.eargv) {
		if (options.info) {
			char **p;

			fputs("Command would be:\n\n", stdout);
			for (p = &options.eargv[0]; p && *p; p++)
				fprintf(stdout, "'%s' ", *p);
			fputs("\n\n", stdout);
			return;
		}
		execvp(options.eargv[0], options.eargv);
		EPRINTF("Should never get here!\n");
		exit(127);
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
Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016  Monavacon Limited.\n\
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
    %1$s [-w|--wait] [options] [[-c|--command|--] COMMAND [ARGUMENT [...]]]\n\
    %1$s {-i|--info} [options] [[-c|--command|--] COMMAND [ARGUMENT [...]]]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static const char *
show_command(int argc, char *argv[])
{
	static char buf[1024] = { 0, };
	int i;

	if (!argv)
		return ("");
	buf[0] = '\0';
	for (i = 0; i < argc; i++) {
		if (i)
			strncat(buf, " ", 1023);
		strncat(buf, "'", 1023);
		strncat(buf, argv[i], 1023);
		strncat(buf, "'", 1023);
	}
	return (buf);
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
	if (!options.output && !options.debug)
		return;
	/* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [-w|--wait] [options] [[-c|--command|--] COMMAND [ARGUMENT [...]]]\n\
    %1$s {-h|--help} [options]\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    COMMAND [ARGUMENT [...]]\n\
        command and arguments to execute\n\
        [default: %6$s]\n\
Command options:\n\
   [-w, --wait]\n\
        wait for desktop resource\n\
    -i, --info\n\
        do not wait, just print what would be done [default: %13$s]\n\
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
    -N, --nowait\n\
        do not wait for any desktop resource [default: %7$s]\n\
    -W, --manager, --window-manager\n\
        wait for window manager before proceeding [default: %8$s]\n\
    -S, --tray, --system-tray\n\
        wait for system tray before proceeding [default: %9$s]\n\
    -P, --pager, --desktop-pager\n\
        wait for desktop pager before proceeding [default: %10$s]\n\
    -O, --composite, --composite-manager\n\
        wait for composite manager before proceeding [default: %11$s]\n\
    -U, --audio, --audio-server\n\
        wait for audio server before proceeding [default: %15$s]\n\
    -A, --all\n\
        wait for all desktop resource [default: %14$s]\n\
    -t, --delay MILLISECONDS\n\
        only wait MILLISECONDS for desktop resource [default: %12$lu]\n\
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
	, show_command(options.eargc, options.eargv)
	, show_bool(options.nowait)
	, show_bool(options.manager)
	, show_bool(options.systray)
	, show_bool(options.pager)
	, show_bool(options.composite)
	, options.delay
	, show_bool(options.info)
	, show_bool(options.all)
	, show_bool(options.audio)
);
	/* *INDENT-ON* */
}

void
set_defaults(void)
{
	const char *env;

	if ((env = getenv("DISPLAY")))
		options.display = strdup(env);
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

			{"wait",		no_argument,		NULL,	'w'},
			{"command",		no_argument,		NULL,	'c'},
			{"info",		no_argument,		NULL,	'i'},
			{"nowait",		no_argument,		NULL,	'N'},
			{"window-manager",	no_argument,		NULL,	'W'},
			{"manager",		no_argument,		NULL,	'W'},
			{"system-tray",		no_argument,		NULL,	'S'},
			{"tray",		no_argument,		NULL,	'S'},
			{"desktop-pager",	no_argument,		NULL,	'P'},
			{"pager",		no_argument,		NULL,	'P'},
			{"composite-manager",	no_argument,		NULL,	'O'},
			{"composite",		no_argument,		NULL,	'O'},
			{"audio-server",	no_argument,		NULL,	'U'},
			{"audio",		no_argument,		NULL,	'U'},
			{"all",			no_argument,		NULL,	'A'},
			{"delay",		required_argument,	NULL,	't'},

			{"debug",		optional_argument,	NULL,	'D'},
			{"verbose",		optional_argument,	NULL,	'v'},
			{"help",		no_argument,		NULL,	'h'},
			{"version",		no_argument,		NULL,	'V'},
			{"copying",		no_argument,		NULL,	'C'},
			{"?",			no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "d:s:wciNWSOUAt:D::v::hVCH?",
				     long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "d:s:wciNWSOUAt:DvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1 || exec_mode) {
			DPRINTF("done options processing\n");
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
			options.screen = strtoul(optarg, &endptr, 0);
			if (endptr && *endptr)
				goto bad_option;
			break;

		case 'w':	/* -w, --wait */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandWait;
			options.command = CommandWait;
			break;
		case 'c':	/* -c, --command PROGRAM ARGUMENT ... */
			exec_mode = True;
			break;
		case 'i':	/* -i, --info */
			options.info = True;
			break;
		case 'N':	/* -N, --nowait */
			options.nowait = True;
			options.manager = False;
			options.systray = False;
			options.pager = False;
			options.composite = False;
			options.all = False;
			break;
		case 'W':	/* -W, --window-manager, --manager */
			options.manager = True;
			options.nowait = False;
			break;
		case 'S':	/* -S, --system-tray, --tray */
			options.systray = True;
			options.nowait = False;
			break;
		case 'P':	/* -P, --desktop-pager, --pager */
			options.pager = True;
			options.nowait = False;
			break;
		case 'O':	/* -O, --composite-manager, --composite */
			options.composite = True;
			options.nowait = False;
			break;
		case 'U':	/* -U, --audio-server, --audio */
			options.audio = True;
			options.nowait = False;
			break;
		case 'A':	/* -A, --all */
			options.all = True;
			options.manager = True;
			options.systray = True;
			options.pager = True;
			options.composite = True;
			options.audio = True;
			break;
		case 't':	/* -t, --delay MILLISECONDS */
			if ((val = strtoul(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			options.delay = val;
			break;

		case 'D':	/* -D, --debug [LEVEL] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [LEVEL] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandVersion;
			options.command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandCopying;
			options.command = CommandCopying;
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
			exit(EXIT_SYNTAXERR);
		}
	}
	if (exec_mode || optind < argc) {
		int i;

		if (optind >= argc)
			goto bad_nonopt;
		exec_mode = True;
		options.eargv = calloc(argc - optind + 1, sizeof(*options.eargv));
		options.eargc = argc - optind;
		for (i = 0; optind < argc; optind++, i++)
			options.eargv[i] = strdup(argv[optind]);
	}
	switch (command) {
	default:
	case CommandDefault:
	case CommandWait:
		do_wait(argc, argv);
		break;
	case CommandHelp:
		help(argc, argv);
		break;
	case CommandVersion:
		version(argc, argv);
		break;
	case CommandCopying:
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
