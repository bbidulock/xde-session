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

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
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
#ifdef VNC_SUPPORTED
#include <X11/extensions/Xvnc.h>
#endif
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/SM/SMlib.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <dbus/dbus-glib.h>
#include <pwd.h>
#include <systemd/sd-login.h>
#include <security/pam_appl.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stderr, "I: "); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: %s +%d %s(): ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: %s +%d %s(): ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: %s +%d %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

static int saveArgc;
static char **saveArgv;

#undef DO_XCHOOSER
#undef DO_XLOCKING
#undef DO_ONIDLE
#undef DO_CHOOSER
#define DO_LOGOUT 1

#if defined(DO_XCHOOSER)
#   define RESNAME "xde-xchooser"
#   define RESCLAS "XDE-XChooser"
#   define RESTITL "XDMCP Chooser"
#elif defined(DO_XLOCKING)
#   define RESNAME "xde-xlock"
#   define RESCLAS "XDE-XLock"
#   define RESTITL "X11 Locker"
#elif defined(DO_CHOOSER)
#   define RESNAME "xde-chooser"
#   define RESCLAS "XDE-Chooser"
#   define RESTITL "XDE X11 Session Chooser"
#elif defined(DO_LOGOUT)
#   define RESNAME "xde-logout"
#   define RESCLAS "XDE-Logout"
#   define RESTITL "XDE X11 Session Logout"
#else
#   define RESNAME "xde-xlogin"
#   define RESCLAS "XDE-XLogin"
#   define RESTITL "XDMCP Greeter"
#endif

#define APPDFLT "/usr/share/X11/app-defaults/" RESCLAS

typedef enum _LogoSide {
	LogoSideLeft,
	LogoSideTop,
	LogoSideRight,
	LogoSideBottom,
} LogoSide;

enum {
	OBEYSESS_DISPLAY,		/* obey multipleSessions resource */
	REMANAGE_DISPLAY,		/* force remanage */
	UNMANAGE_DISPLAY,		/* force deletion */
	RESERVER_DISPLAY,		/* force server termination */
	OPENFAILED_DISPLAY,		/* XOpenDisplay failed, retry */
};

#undef EXIT_SUCCESS
#define EXIT_SUCCESS	OBEYSESS_DISPLAY
#undef EXIT_FAILURE
#define EXIT_FAILURE	REMANAGE_DISPLAY
#undef EXIT_SYNTAXERR
#define EXIT_SYNTAXERR	UNMANAGE_DISPLAY

typedef enum {
	CommandDefault,
	CommandHelp,			/* command argument help */
	CommandVersion,			/* command version information */
	CommandCopying,			/* command copying information */
} CommandType;

enum {
	BackgroundSourceSplash = (1 << 0),
	BackgroundSourcePixmap = (1 << 1),
	BackgroundSourceRoot = (1 << 2),
};

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *lockscreen;
	char *banner;
	char *welcome;
	CommandType command;
	char *charset;
	char *language;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	LogoSide side;
	Bool noask;
	Bool execute;
	char *current;
	Bool managed;
	char *session;
	char *choice;
	char *username;
	char *password;
	Bool usexde;
	unsigned int timeout;
	char *clientId;
	char *saveFile;
	GKeyFile *dmrc;
	char *vendor;
	char *prefix;
	char *splash;
	unsigned source;
	Bool xsession;
	Bool setbg;
	Bool transparent;
	int width;
	int height;
	double xposition;
	double yposition;
	Bool setstyle;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.lockscreen = NULL,
	.banner = NULL,
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.noask = False,
	.execute = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
	.source = BackgroundSourceRoot,
	.xsession = False,
	.setbg = False,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
};

Options defaults = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.lockscreen = NULL,
	.banner = NULL,
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.noask = False,
	.execute = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
	.source = BackgroundSourceRoot,
	.xsession = False,
	.setbg = False,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
};

typedef struct {
	GdkColor *foreground;
	GdkColor *background;
	PangoFontDescription *face;
	char *greeting;
	char *unsecureGreeting;
	PangoFontDescription *greetFace;
	GdkColor *greetColor;
	char *namePrompt;
	char *passwdPrompt;
	PangoFontDescription *promptFace;
	GdkColor *promptColor;
	PangoFontDescription *inputFace;
	GdkColor *inputColor;
	char *changePasswdMessage;
	char *fail;
	PangoFontDescription *failFace;
	GdkColor *failColor;
	unsigned int failTimeout;
	char *logoFileName;
	unsigned int logoPadding;
	Bool useShape;
	GdkColor *hiColor;
	GdkColor *shdColor;
	unsigned int frameWidth;
	unsigned int innerFrameWidth;
	unsigned int sepWidth;
	Bool allowRootLogin;
	Bool allowNullPasswd;
	Bool echoPasswd;
	char *echoPasswdChar;
	unsigned int borderWidth;
} Resources;

Resources resources  = {
	.foreground = NULL,
	.background = NULL,
	.face = NULL,
	.greeting = NULL,
	.unsecureGreeting = NULL,
	.greetFace = NULL,
	.greetColor = NULL,
	.namePrompt = NULL,
	.passwdPrompt = NULL,
	.promptFace = NULL,
	.promptColor = NULL,
	.inputFace = NULL,
	.inputColor = NULL,
	.changePasswdMessage = NULL,
	.fail = NULL,
	.failFace = NULL,
	.failColor = NULL,
	.failTimeout = 0,
	.logoFileName = NULL,
	.logoPadding = 0,
	.useShape = False,
	.hiColor = NULL,
	.shdColor = NULL,
	.frameWidth = 0,
	.innerFrameWidth = 0,
	.sepWidth = 0,
	.allowRootLogin = False,
	.allowNullPasswd = False,
	.echoPasswd = False,
	.echoPasswdChar = NULL,
	.borderWidth = 0,
};

typedef enum {
	LOGOUT_ACTION_POWEROFF,		/* power off the computer */
	LOGOUT_ACTION_REBOOT,		/* reboot the computer */
	LOGOUT_ACTION_SUSPEND,		/* suspend the computer */
	LOGOUT_ACTION_HIBERNATE,	/* hibernate the computer */
	LOGOUT_ACTION_HYBRIDSLEEP,	/* hybrid sleep the computer */
	LOGOUT_ACTION_SWITCHUSER,	/* switch users */
	LOGOUT_ACTION_SWITCHDESK,	/* switch desktops */
	LOGOUT_ACTION_LOCKSCREEN,	/* lock screen */
	LOGOUT_ACTION_CHECKPOINT,	/* checkpoint the current session */
	LOGOUT_ACTION_SHUTDOWN,		/* checkpoint and shutdown session */
	LOGOUT_ACTION_LOGOUT,		/* logout of current session */
	LOGOUT_ACTION_RESTART,		/* restart current session */
	LOGOUT_ACTION_CANCEL,		/* cancel logout */
	LOGOUT_ACTION_COUNT,
} LogoutActionResult;

LogoutActionResult action_result;
LogoutActionResult logout_result = LOGOUT_ACTION_CANCEL;

static SmcConn smcConn;

Atom _XA_XDE_THEME_NAME;
Atom _XA_GTK_READ_RCFILES;
Atom _XA_XROOTPMAP_ID;
Atom _XA_ESETROOT_PMAP_ID;

#ifdef DO_XLOCKING
int xssEventBase;
int xssErrorBase;
int xssMajorVersion;
int xssMinorVersion;

const char *
xssState(int state)
{
	switch (state) {
	case ScreenSaverOff:
		return ("Off");
	case ScreenSaverOn:
		return ("On");
	case ScreenSaverCycle:
		return ("Cycle");
	case ScreenSaverDisabled:
		return ("Disabled");
	}
	return ("(unknown)");
}

const char *
xssKind(int kind)
{
	switch (kind) {
	case ScreenSaverBlanked:
		return ("Blanked");
	case ScreenSaverInternal:
		return ("Internal");
	case ScreenSaverExternal:
		return ("External");
	}
	return ("(unknown)");
}
#endif

const char *
showBool(Bool boolean)
{
	if (boolean)
		return ("True");
	return ("False");
}

typedef struct {
	int index;
	GtkWidget *align;		/* alignment widget at center of
					   monitor */
	GdkRectangle geom;		/* monitor geometry */
} XdeMonitor;

typedef struct {
	int index;			/* index */
	GdkScreen *scrn;		/* screen */
	GtkWidget *wind;		/* covering window for screen */
	gint width;			/* width of screen */
	gint height;			/* height of screen */
	gint nmon;			/* number of monitors */
	XdeMonitor *mons;		/* monitors for this screen */
	GdkPixmap *pixmap;		/* pixmap for background image */
	GdkPixbuf *pixbuf;		/* pixbuf for background image */
#ifdef DO_XLOCKING
	XScreenSaverInfo info;		/* screen saver info for this screen */
	char selection[32];
	Window selwin;
#endif
} XdeScreen;

XdeScreen *screens;

#ifdef DO_XLOCKING
void
setup_screensaver(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	int s, nscr = gdk_display_get_n_screens(disp);
	char **list, **ext;
	int n, next = 0;
	Bool gotext = False;
	Bool present;
	Status status;
	XdeScreen *xscr;

	if ((list = XListExtensions(dpy, &next)) && next)
		for (n = 0, ext = list; n < next; n++, ext++)
			if (!strcmp(*ext, "MIT-SCREEN-SAVER"))
				gotext = True;
	if (!gotext) {
		DPRINTF("no MIT-SCREEN-SAVER extension\n");
		return;
	}
	if (!(present = XScreenSaverQueryExtension(dpy, &xssEventBase, &xssErrorBase))) {
		DPRINTF("MIT-SCREEN-SAVER extension not present\n");
		return;
	}
	if (!(status = XScreenSaverQueryVersion(dpy, &xssMajorVersion, &xssMinorVersion))) {
		DPRINTF("cannot query MIT-SCREEN-SAVER version\n");
		return;
	}
	for (s = 0, xscr = screens; s < nscr; s++, xscr++) {
		GdkScreen *scrn = gdk_display_get_screen(disp, s);
		XSetWindowAttributes xwa;
		unsigned long mask = 0;

		mask |= CWBackPixmap;
		xwa.background_pixmap = None;
		mask |= CWBackPixel;
		xwa.background_pixel = BlackPixel(dpy, s);
		mask |= CWBorderPixmap;
		xwa.border_pixmap = None;
		mask |= CWBorderPixel;
		xwa.border_pixel = BlackPixel(dpy, s);
		mask |= CWBitGravity;
		xwa.bit_gravity = 0;
		mask |= CWWinGravity;
		xwa.win_gravity = NorthWestGravity;
		mask |= CWBackingStore;
		xwa.backing_store = NotUseful;
		mask |= CWBackingPlanes;
		xwa.backing_pixel = 0;
		mask |= CWSaveUnder;
		xwa.save_under = True;
		mask |= CWEventMask;
		xwa.event_mask = NoEventMask;
		mask |= CWDontPropagate;
		xwa.do_not_propagate_mask = NoEventMask;
		mask |= CWOverrideRedirect;
		xwa.override_redirect = True;
		mask |= CWColormap;
		xwa.colormap = DefaultColormap(dpy, s);
		mask |= CWCursor;
		xwa.cursor = None;

		XScreenSaverQueryInfo(dpy, RootWindow(dpy, s), &xscr->info);
		XScreenSaverSelectInput(dpy, RootWindow(dpy, s),
					ScreenSaverNotifyMask | ScreenSaverCycleMask);
		XScreenSaverSetAttributes(dpy, RootWindow(dpy, s), 0, 0,
					  gdk_screen_get_width(scrn),
					  gdk_screen_get_height(scrn),
					  0, DefaultDepth(dpy, s), InputOutput,
					  DefaultVisual(dpy, s), mask, &xwa);
		GdkWindow *win = gtk_widget_get_window(xscr->wind);
		Window w = GDK_WINDOW_XID(win);

		XScreenSaverRegister(dpy, s, w, XA_WINDOW);

	}
}

GdkFilterReturn
handle_XScreenSaverNotify(Display *dpy, XEvent *xev)
{
	XScreenSaverNotifyEvent *ev = (typeof(ev)) xev;

	DPRINT();

	if (options.debug > 1) {
		fprintf(stderr, "==> XScreenSaverNotify:\n");
		fprintf(stderr, "    --> send_event = %s\n", showBool(ev->send_event));
		fprintf(stderr, "    --> window = 0x%lx\n", ev->window);
		fprintf(stderr, "    --> root = 0x%lx\n", ev->root);
		fprintf(stderr, "    --> state = %s\n", xssState(ev->state));
		fprintf(stderr, "    --> kind = %s\n", xssKind(ev->kind));
		fprintf(stderr, "    --> forced = %s\n", showBool(ev->forced));
		fprintf(stderr, "    --> time = %lu\n", ev->time);
		fprintf(stderr, "<== XScreenSaverNotify:\n");
	}
	return G_SOURCE_CONTINUE;
}
#endif				/* DO_XLOCKING */

typedef enum {
	AvailStatusUndef,		/* undefined */
	AvailStatusUnknown,		/* not known */
	AvailStatusNa,			/* not available */
	AvailStatusNo,			/* available not permitted */
	AvailStatusChallenge,		/* available with password */
	AvailStatusYes,			/* available and permitted */
} AvailStatus;

AvailStatus
status_of_string(const char *string)
{
	if (!string)
		return AvailStatusUndef;
	if (!string || !strcmp(string, "na"))
		return AvailStatusNa;
	if (!strcmp(string, "no"))
		return AvailStatusNo;
	if (!strcmp(string, "yes"))
		return AvailStatusYes;
	if (!strcmp(string, "challenge"))
		return AvailStatusChallenge;
	EPRINTF("unknown availability status %s\n", string);
	return AvailStatusUnknown;
}

static AvailStatus action_can[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= AvailStatusUndef,
	[LOGOUT_ACTION_REBOOT]		= AvailStatusUndef,
	[LOGOUT_ACTION_SUSPEND]		= AvailStatusUndef,
	[LOGOUT_ACTION_HIBERNATE]	= AvailStatusUndef,
	[LOGOUT_ACTION_HYBRIDSLEEP]	= AvailStatusUndef,
	[LOGOUT_ACTION_SWITCHUSER]	= AvailStatusUndef,
	[LOGOUT_ACTION_SWITCHDESK]	= AvailStatusUndef,
	[LOGOUT_ACTION_LOCKSCREEN]	= AvailStatusUndef,
	[LOGOUT_ACTION_CHECKPOINT]	= AvailStatusUndef,
	[LOGOUT_ACTION_SHUTDOWN]	= AvailStatusUndef,
	[LOGOUT_ACTION_LOGOUT]		= AvailStatusUndef,
	[LOGOUT_ACTION_RESTART]		= AvailStatusUndef,
	[LOGOUT_ACTION_CANCEL]		= AvailStatusUndef,
	/* *INDENT-ON* */
};

static void reparse(Display *dpy, Window root);
static void redo_source(XdeScreen *xscr);

static GdkFilterReturn
event_handler_PropertyNotify(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	DPRINT();
	if (options.debug > 2) {
		fprintf(stderr, "==> PropertyNotify:\n");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xproperty.window);
		fprintf(stderr, "    --> atom = %s\n", XGetAtomName(dpy, xev->xproperty.atom));
		fprintf(stderr, "    --> time = %ld\n", xev->xproperty.time);
		fprintf(stderr, "    --> state = %s\n",
			(xev->xproperty.state == PropertyNewValue) ? "NewValue" : "Delete");
		fprintf(stderr, "<== PropertyNotify:\n");
	}
	if (xev->xproperty.atom == _XA_XDE_THEME_NAME && xev->xproperty.state == PropertyNewValue) {
		DPRINT();
		reparse(dpy, xev->xproperty.window);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	if (xev->xproperty.atom == _XA_XROOTPMAP_ID && xev->xproperty.state == PropertyNewValue) {
		DPRINT();
		if (options.source & BackgroundSourcePixmap)
			redo_source(xscr);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
event_handler_ClientMessage(Display *dpy, XEvent *xev)
{
	DPRINT();
	if (options.debug > 1) {
		fprintf(stderr, "==> ClientMessage:\n");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xclient.window);
		fprintf(stderr, "    --> message_type = %s\n",
			XGetAtomName(dpy, xev->xclient.message_type));
		fprintf(stderr, "    --> format = %d\n", xev->xclient.format);
		switch (xev->xclient.format) {
			int i;

		case 8:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 20; i++)
				fprintf(stderr, " %02x", xev->xclient.data.b[i]);
			fprintf(stderr, "\n");
			break;
		case 16:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 10; i++)
				fprintf(stderr, " %04x", xev->xclient.data.s[i]);
			fprintf(stderr, "\n");
			break;
		case 32:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 5; i++)
				fprintf(stderr, " %08lx", xev->xclient.data.l[i]);
			fprintf(stderr, "\n");
			break;
		}
		fprintf(stderr, "<== ClientMessage:\n");
	}
	if (xev->xclient.message_type == _XA_GTK_READ_RCFILES) {
		reparse(dpy, xev->xclient.window);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
root_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = data;
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	switch (xev->type) {
	case PropertyNotify:
		return event_handler_PropertyNotify(dpy, xev, xscr);
	}
	return GDK_FILTER_CONTINUE;
}

#ifdef DO_XLOCKING
static GdkFilterReturn
event_handler_SelectionClear(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	DPRINT();
	if (options.debug > 1) {
		fprintf(stderr, "==> SelectionClear: %p\n", xscr);
		fprintf(stderr, "    --> send_event = %s\n",
			xev->xselectionclear.send_event ? "true" : "false");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xselectionclear.window);
		fprintf(stderr, "    --> selection = %s\n",
			XGetAtomName(dpy, xev->xselectionclear.selection));
		fprintf(stderr, "    --> time = %lu\n", xev->xselectionclear.time);
		fprintf(stderr, "<== SelectionClear: %p\n", xscr);
	}
	if (xscr && xev->xselectionclear.window == xscr->selwin) {
		XDestroyWindow(dpy, xscr->selwin);
		EPRINTF("selection cleared, exiting\n");
		exit(EXIT_SUCCESS);
	}
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
selwin_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = (typeof(xscr)) data;
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	DPRINT();
	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	switch (xev->type) {
	case SelectionClear:
		return event_handler_SelectionClear(dpy, xev, xscr);
	}
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}
#endif

static GdkFilterReturn
client_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	Display *dpy = (typeof(dpy)) data;

	DPRINT();
	switch (xev->type) {
	case ClientMessage:
		return event_handler_ClientMessage(dpy, xev);
	}
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}

GtkWidget *top;

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

/** @brief get system data directories
  *
  * Note that, unlike some other tools, there is no home directory at this point
  * so just search the system XDG data directories for things, but treat the XDM
  * home as /usr/lib/X11/xdm.
  */
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

/*
 * Determine whether we have been invoked under a session running lxsession(1).
 * When that is the case, we simply execute lxsession-logout(1) with the
 * appropriate parameters for branding.  In that case, this method does not
 * return (executes lxsession-logout directly).  Otherwise the method returns.
 * This method is currently unused and is deprecated.
 */
void
lxsession_check()
{
}

/** @brief test screen locking ability using systemd
  *
  * First off, if we have an XDG_SESSION_ID then we are running under a systemd
  * session.  Because anything of ours that properly registers a graphical
  * session with systemd can likely lock the screen as long as we can talk to
  * login1 on the DBUS, consider that sufficient.
  */
void
test_session_lock()
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;

	if (!getenv("XDG_SESSION_ID")) {
		DPRINTF("no XDG_SESSION_ID supplied\n");
		return;
	}
	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err)) || err) {
		EPRINTF("cannot access system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	g_object_unref(G_OBJECT(proxy));

	action_can[LOGOUT_ACTION_LOCKSCREEN] = AvailStatusYes;
}

struct prog_cmd {
	char *name;
	char *cmd;
};

/** @brief test availability of a screen locker program
  *
  * Test to see whether the caller specified a lock screen program. If not,
  * search through a short list of known screen lockers, searching PATH for an
  * executable of the corresponding name, and when one is found, set the screen
  * locking program to that function.  We could probably easily write our own
  * little screen locker here, but I don't have the time just now...
  *
  * These are hardcoded.  Sorry.  Later we can try to design a reliable search
  * for screen locking programs in the XDG applications directory or create a
  * "sensible-" or "preferred-" screen locker shell program.  That would be
  * useful for menus too.
  *
  * Note that if a screen saver is registered with the X Server then we can
  * likely simply ask the screen saver to lock the screen.
  *
  * Note also that we can use DBUS interface to systemd logind service to
  * request that a session manager lock the screen.
  */
void
test_lock_screen_program()
{
	static const struct prog_cmd progs[7] = {
		/* *INDENT-OFF* */
		{"xde-xlock",	    "xde-xlock -lock &"	    },
		{"xlock",	    "xlock -mode blank &"   },
		{"slock",	    "slock &"		    },
		{"slimlock",	    "slimlock &"	    },
		{"i3lock",	    "i3lock -c 000000 &"    },
		{"xscreensaver",    "xscreensaver -lock &"  },
		{ NULL,		     NULL		    }
		/* *INDENT-ON* */
	};
	const struct prog_cmd *prog;

	if (!options.lockscreen) {
		for (prog = progs; prog->name; prog++) {
			char *paths = strdup(getenv("PATH") ? : "");
			char *p = paths - 1;
			char *e = paths + strlen(paths);
			char *b, *path;
			struct stat st;
			int status;
			int len;

			while ((b = p + 1) < e) {
				*(p = strchrnul(b, ':')) = '\0';
				len = strlen(b) + 1 + strlen(prog->name) + 1;
				path = calloc(len, sizeof(*path));
				strncpy(path, b, len);
				strncat(path, "/", len);
				strncat(path, prog->name, len);
				status = stat(path, &st);
				free(path);
				if (status == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXOTH)) {
					options.lockscreen = strdup(prog->cmd);
					goto done;
				}
			}
		}
	}
      done:
	if (options.lockscreen)
		action_can[LOGOUT_ACTION_LOCKSCREEN] = AvailStatusYes;
	return;
}

static Bool isLocal(void);

void
test_login_functions()
{
	const char *seat;
	int ret;

	seat = getenv("XDG_SEAT") ? : "seat0";
	ret = sd_seat_can_multi_session(NULL);
	if (ret > 0) {
		action_can[LOGOUT_ACTION_SWITCHUSER] = AvailStatusYes;
		DPRINTF("%s: mutisession: true\n", seat);
	} else if (ret == 0) {
		action_can[LOGOUT_ACTION_SWITCHUSER] = AvailStatusNa;
		DPRINTF("%s: mutisession: false\n", seat);
	} else if (ret < 0) {
		action_can[LOGOUT_ACTION_SWITCHUSER] = AvailStatusUnknown;
		DPRINTF("%s: mutisession: unknown\n", seat);
	}
	if (action_can[LOGOUT_ACTION_SWITCHUSER] != AvailStatusNa && !isLocal()) {
		action_can[LOGOUT_ACTION_SWITCHUSER] = AvailStatusNa;
		DPRINTF("session not local\n");
	}
}

static void
on_switch_session(GtkMenuItem *item, gpointer data)
{
	gchar *session = data;
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err)) || err) {
		EPRINTF("cannot access system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "ActivateSession", &err, G_TYPE_STRING,
			       session, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok || err) {
		DPRINTF("ActivateSession: %s: call failed: %s\n", session,
			err ? err->message : NULL);
		g_clear_error(&err);
	}
	g_object_unref(G_OBJECT(proxy));

	if (ok) {
		action_result = LOGOUT_ACTION_SWITCHUSER;
		gtk_main_quit();
	}
}

static void
free_string(gpointer data, GClosure *unused)
{
	free(data);
}

static int
comparevts(const void *a, const void *b)
{
	const char * const *sa = a;
	const char * const *sb = b;
	unsigned int vta = 0;
	unsigned int vtb = 0;

	sd_session_get_vt(*sa, &vta);
	sd_session_get_vt(*sb, &vtb);
	DPRINTF("comparing session %s(vt%u) and %s(vt%u)", *sa, vta, *sb, vtb);
	if (vta < vtb)
		return -1;
	if (vta > vtb)
		return 1;
	return 0;
}

GtkWidget *
get_user_menu(void)
{
	const char *seat, *sess;
	char **sessions = NULL, **s;
	GtkWidget *menu = NULL;
	gboolean gotone = FALSE;
	Bool islocal;
	int count;

	islocal = isLocal();

	seat = getenv("XDG_SEAT") ? : "seat0";
	sess = getenv("XDG_SESSION_ID");
	if (sd_seat_can_multi_session(NULL) <= 0) {
		DPRINTF("%s: cannot multi-session\n", seat);
		return (NULL);
	}
	if (sd_seat_get_sessions(seat, &sessions, NULL, NULL) < 0) {
		EPRINTF("%s: cannot get sessions\n", seat);
		return (NULL);
	}
	menu = gtk_menu_new();
	for (s = sessions, count = 0; s && *s; s++, count++) ;
	if (count) {
		DPRINTF("sorting vts\n");
		qsort(sessions, count, sizeof(char *), comparevts);
	}
	for (s = sessions; s && *s; free(*s), s++) {
		char *type = NULL, *klass = NULL, *user = NULL, *host = NULL,
		    *tty = NULL, *disp = NULL;
		unsigned int vtnr = 0;
		uid_t uid = -1;
		Bool isactive = False;

		DPRINTF("%s(%s): considering session\n", seat, *s);
		if (sess && !strcmp(*s, sess)) {
			DPRINTF("%s(%s): cannot switch to own session\n", seat, *s);
			isactive = True;
		}
		if (sd_session_is_active(*s)) {
			DPRINTF("%s(%s): cannot switch to active session\n", seat, *s);
			isactive = True;
		}
		sd_session_get_vt(*s, &vtnr);
		if (vtnr == 0) {
			DPRINTF("%s(%s): cannot switch to non-vt session\n", seat, *s);
			continue;
		}
		sd_session_get_type(*s, &type);
		sd_session_get_class(*s, &klass);
		sd_session_get_uid(*s, &uid);
		if (sd_session_is_remote(*s) > 0) {
			sd_session_get_remote_user(*s, &user);
			sd_session_get_remote_host(*s, &host);
		}
		sd_session_get_tty(*s, &tty);
		sd_session_get_display(*s, &disp);

		GtkWidget *imag;
		GtkWidget *item;
		char *iname = GTK_STOCK_JUMP_TO;
		gchar *label;

		if (type) {
			if (!strcmp(type, "tty"))
				iname = "utilities-terminal";
			else if (!strcmp(type, "x11"))
				iname = "preferences-system-windows";
		}
		if (klass) {
			if (!strcmp(klass, "user")) {
				struct passwd *pw;

				if (user)
					label = g_strdup_printf("%u: %s", vtnr, user);
				else if (uid != -1 && (pw = getpwuid(uid)))
					label = g_strdup_printf("%u: %s", vtnr, pw->pw_name);
				else
					label = g_strdup_printf("%u: %s", vtnr, "(unknown)");
			} else if (!strcmp(klass, "greeter"))
				label = g_strdup_printf("%u: login", vtnr);
			else
				label = g_strdup_printf("%u: session %s", vtnr, *s);
		} else
			label = g_strdup_printf("%u: session %s", vtnr, *s);

		item = gtk_image_menu_item_new_with_label(label);
		imag = gtk_image_new_from_icon_name(iname, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		DPRINTF("%s(%s): adding item to menu: %s\n", seat, *s, label);
		g_free(label);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				      G_CALLBACK(on_switch_session),
				      strdup(*s), free_string, G_CONNECT_AFTER);
		gtk_widget_show(item);
		if (islocal && !isactive) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
	}
	free(sessions);
	if (gotone)
		return (menu);
	g_object_unref(menu);
	return (NULL);
}

/** @brief determines whether the user is local or remote using a bunch or
  * techniques and heauristics.  We check for the following conditions:
  *
  * 1. the user is local and not remote
  * 2. the session is associated with a virtual terminal
  * 3. the DISPLAY starts with ':'
  * 4. The X Display has no VNC-EXTENSION extension (otherwise it could in fact
  *    be remote).
  *
  * When these coniditions are satistifed, the use is like sitting at a physical
  * seat of the computer and could have access to the pwoer button anyway.
  */
static Bool
isLocal(void)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	Window root = DefaultRootWindow(dpy);
	Atom prop;

	if ((getenv("DISPLAY") ? : "")[0] != ':')
		return False;

	if (getenv("SSH_CLIENT") || getenv("SSH_CONNECTION") || getenv("SSH_TTY"))
		return False;

	/* ssh connection does not set these */
	if (!getenv("XDG_SEAT") || !getenv("XDG_VTNR"))
		return False;

	if ((prop = XInternAtom(dpy, "Xorg_Seat", True))) {
		XTextProperty xtp = { NULL, };
		char **list = NULL;
		int strings = 0;
		int i = 0;

		if (XGetTextProperty(dpy, root, &xtp, prop)) {
			if (Xutf8TextPropertyToTextList(dpy, &xtp, &list, &strings) == Success) {
				for (i = 0; i < strings &&
				     strcmp(list[i], getenv("XDG_SEAT")); i++) ;
				if (list)
					XFreeStringList(list);
			}
		}
		if (xtp.value)
			XFree(xtp.value);
		if (i >= strings)
			return False;
	}
	if ((prop = XInternAtom(dpy, "XFree86_has_VT", True))) {
		Atom actual = None;
		int format = 0;
		unsigned long nitems = 0, after = 0;
		long *data = NULL;
		int value = 1;

		if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_INTEGER,
				       &actual, &format, &nitems, &after,
				       (unsigned char **) &data) == Success &&
		    format == 32 && actual && nitems >= 1 && data)
			value = data[0];

		if (data)
			XFree(data);
		if (!value)
			return False;
	}
	if ((prop = XInternAtom(dpy, "XFree86_VT", True))) {
		Atom actual = None;
		int format = 0;
		unsigned long nitems = 0, after = 0;
		long *data = NULL;
		int value = 0;

		if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_INTEGER,
				       &actual, &format, &nitems, &after,
				       (unsigned char **) &data) == Success &&
		    format == 32 && actual && nitems >= 1 && data) {
			value = data[0];
			if (value != atoi(getenv("XDG_VTNR")))
				return False;
		}
	}
#ifdef VNC_SUPPORTED
	do {
		int xvncEventBase = 0;
		int xvncErrorBase = 0;
		int n, next = 0, len = 0, value = 0;
		char **list, **ext, *parm = NULL;
		Bool present = False;

		if ((list = XListExtensions(dpy, &next)) && next)
			for (n = 0, ext = list; n < next; n++, ext++)
				if (!strncmp(*ext, "VNC-EXTENSION", strlen(*ext)))
					present = True;
		if (!present) {
			DPRINTF("no VNC-EXTENSION extension\n");
			break;
		}
		present = XVncExtQueryExtension(dpy, &xvncEventBase, &xvncErrorBase);
		if (!present) {
			DPRINTF("VNC-EXTENSION not present\n");
			break;
		}
		if (XVncExtGetParam(dpy, "DisconnectClients", &parm, &len) && parm) {
			value = atoi(parm);
			XFree(parm);
			parm = NULL;
		}
		if (!value)
			return False;
		value = 0;
		if (XVncExtGetParam(dpy, "NeverShared", &parm, &len) && parm) {
			value = atoi(parm);
			XFree(parm);
			parm = NULL;
		}
		if (!value)
			return False;
		value = 0;
		if (XVncExtGetParam(dpy, "rfbport", &parm, &len) && parm) {
			value = !atoi(parm);
			XFree(parm);
			parm = NULL;
		}
		if (!value)
			return False;
		value = 0;
		if (XVncExtGetParam(dpy, "SecurityTypes", &parm, &len) && parm) {
			value = !strcmp(parm, "None");
			XFree(parm);
			parm = NULL;
		}
		if (!value)
			return False;
	} while (0);
#endif
	return True;
}

void
test_manager_functions()
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;

//      gboolean ok;
	const char *env;
	char *path;

	path = calloc(PATH_MAX + 1, sizeof(*path));

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err)) || err) {
		EPRINTF("cannot access system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		free(path);
		return;
	}
	if ((env = getenv("XDG_SEAT_PATH"))) {
		strncpy(path, env, PATH_MAX);
	} else if ((env = getenv("XDG_SEAT"))) {
		strncpy(path, "/org/freedesktop/DisplayManager/", PATH_MAX);
		strncat(path, env, PATH_MAX);
	} else
		strncpy(path, "/org/freedesktop/DisplayManager/Seat0", PATH_MAX);

	proxy = dbus_g_proxy_new_for_name(bus, "org.freedesktop.DisplayManager", path,
					  "org.freedesktop.DisplayManager.Seat");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy for %s\n", path);
		free(path);
		return;
	}

}

/** @brief test availability of power functions
  *
  * Uses DBUsGProxy and the login1 service to test for available power
  * functions.  The results of the test are stored in the corresponding booleans
  * in the available functions structure.
  */
void
test_power_functions()
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gchar *value = NULL;
	gboolean ok;
	Bool islocal;

	islocal = isLocal();

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err)) || err) {
		EPRINTF("cannot access system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	if (!(proxy = dbus_g_proxy_new_for_name(bus,
						"org.freedesktop.login1",
						"/org/freedesktop/login1",
						"org.freedesktop.login1.Manager"))) {
		EPRINTF("could not create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "CanPowerOff",
			       &err, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !err) {
		DPRINTF("CanPowerOff status is %s\n", value);
		if (islocal)
			action_can[LOGOUT_ACTION_POWEROFF] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else {
		EPRINTF("CanPowerOff call failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
	}

	ok = dbus_g_proxy_call(proxy, "CanReboot",
			       &err, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !err) {
		DPRINTF("CanReboot status is %s\n", value);
		if (islocal)
			action_can[LOGOUT_ACTION_REBOOT] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else {
		EPRINTF("CanReboot call failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
	}

	ok = dbus_g_proxy_call(proxy, "CanSuspend",
			       &err, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !err) {
		DPRINTF("CanSuspend status is %s\n", value);
		if (islocal)
			action_can[LOGOUT_ACTION_SUSPEND] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else {
		EPRINTF("CanSuspend call failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
	}

	ok = dbus_g_proxy_call(proxy, "CanHibernate",
			       &err, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !err) {
		DPRINTF("CanHibernate status is %s\n", value);
		if (islocal)
			action_can[LOGOUT_ACTION_HIBERNATE] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else {
		EPRINTF("CanHibernate call failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
	}

	ok = dbus_g_proxy_call(proxy, "CanHybridSleep",
			       &err, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !err) {
		DPRINTF("CanHybridSleep status is %s\n", value);
		if (islocal)
			action_can[LOGOUT_ACTION_HYBRIDSLEEP] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else {
		EPRINTF("CanHybridSleep call failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
	}

	g_object_unref(proxy);
}

/** @brief test availability of user functions
  *
  * For now, do not let the user switch window managers.
  */
void
test_user_functions()
{
	action_can[LOGOUT_ACTION_SWITCHDESK] = AvailStatusNa;
}

/** @brief test availability of session functions
  *
  * For now, always let the user logout or cancel, but not restart the current
  * session.
  */
void
test_session_functions()
{
	action_can[LOGOUT_ACTION_LOGOUT] = AvailStatusYes;
	action_can[LOGOUT_ACTION_RESTART] = AvailStatusNa;
	action_can[LOGOUT_ACTION_CANCEL] = AvailStatusYes;

	if (smcConn) {
		action_can[LOGOUT_ACTION_CHECKPOINT] = AvailStatusYes;
		action_can[LOGOUT_ACTION_SHUTDOWN] = AvailStatusYes;
	} else {
		action_can[LOGOUT_ACTION_CHECKPOINT] = AvailStatusNa;
		action_can[LOGOUT_ACTION_SHUTDOWN] = AvailStatusNa;
	}
}

void grabbed_window(GtkWidget *window, gpointer user_data);
void ungrabbed_window(GtkWidget *window);

/** @brief ask the user if they are sure
  * @param window - grabbed window
  * @param message - dialog message
  *
  * Simple dialog prompting the user with a yes or no question; however, the
  * window is one that was previously grabbed using grabbed_window().  This
  * method hands the focus grab to the dialog and back to the window on exit.
  * Returns the response to the dialog.
  */
gboolean
areyousure(GtkWindow *window, char *message)
{
	GtkWidget *d;
	gint result;

	ungrabbed_window(GTK_WIDGET(window));
	d = gtk_message_dialog_new(window, GTK_DIALOG_MODAL,
				   GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", message);
	gtk_window_set_title(GTK_WINDOW(d), "Are you sure?");
	gtk_window_set_modal(GTK_WINDOW(d), TRUE);
	gtk_window_set_gravity(GTK_WINDOW(d), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(d), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(d), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(d), TRUE);
	gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_widget_show_all(GTK_WIDGET(d));
	gtk_widget_show_now(GTK_WIDGET(d));
	grabbed_window(d, NULL);
	result = gtk_dialog_run(GTK_DIALOG(d));
	ungrabbed_window(d);
	gtk_widget_destroy(GTK_WIDGET(d));
	if (result == GTK_RESPONSE_YES)
		grabbed_window(GTK_WIDGET(window), NULL);
	return ((result == GTK_RESPONSE_YES) ? TRUE : FALSE);
}

GtkWidget *buttons[LOGOUT_ACTION_COUNT] = { NULL, };

static const char *button_tips[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= "Shutdown the computer.",
	[LOGOUT_ACTION_REBOOT]		= "Reboot the computer.",
	[LOGOUT_ACTION_SUSPEND]		= "Suspend the computer.",
	[LOGOUT_ACTION_HIBERNATE]	= "Hibernate the computer.",
	[LOGOUT_ACTION_HYBRIDSLEEP]	= "Hybrid sleep the computer.",
	[LOGOUT_ACTION_SWITCHUSER]	= "Switch users.",
	[LOGOUT_ACTION_SWITCHDESK]	= "Switch window managers.",
	[LOGOUT_ACTION_LOCKSCREEN]	= "Lock the screen.",
	[LOGOUT_ACTION_CHECKPOINT]	= "Checkpoint the current session.",
	[LOGOUT_ACTION_SHUTDOWN]	= "Checkpoint and shutdown current session.",
	[LOGOUT_ACTION_LOGOUT]		= "Log out of current session.",
	[LOGOUT_ACTION_RESTART]		= "Restart current session.",
	[LOGOUT_ACTION_CANCEL]		= "Cancel",
	/* *INDENT-ON* */
};

static const char *button_labels[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= "Power Off",
	[LOGOUT_ACTION_REBOOT]		= "Reboot",
	[LOGOUT_ACTION_SUSPEND]		= "Suspend",
	[LOGOUT_ACTION_HIBERNATE]	= "Hibernate",
	[LOGOUT_ACTION_HYBRIDSLEEP]	= "Hybrid Sleep",
	[LOGOUT_ACTION_SWITCHUSER]	= "Switch User",
	[LOGOUT_ACTION_SWITCHDESK]	= "Switch Desktop",
	[LOGOUT_ACTION_LOCKSCREEN]	= "Lock Screen",
	[LOGOUT_ACTION_CHECKPOINT]	= "Checkpoint",
	[LOGOUT_ACTION_SHUTDOWN]	= "Shutdown",
	[LOGOUT_ACTION_LOGOUT]		= "Logout",
	[LOGOUT_ACTION_RESTART]		= "Restart",
	[LOGOUT_ACTION_CANCEL]		= "Cancel",
	/* *INDENT-ON* */
};

/* TODO: we should actually build an icon factory with stock icons */

static const char *button_icons[LOGOUT_ACTION_COUNT][3] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= { GTK_STOCK_STOP,			"system-shutdown",		"gnome-session-halt"		},
	[LOGOUT_ACTION_REBOOT]		= { GTK_STOCK_REFRESH,			"system-reboot",		"gnome-session-reboot"		},
	[LOGOUT_ACTION_SUSPEND]		= { GTK_STOCK_SAVE,			"system-suspend",		"gnome-session-suspend"		},
	[LOGOUT_ACTION_HIBERNATE]	= { GTK_STOCK_SAVE_AS,			"system-suspend-hibernate",	"gnome-session-hibernate"	},
	[LOGOUT_ACTION_HYBRIDSLEEP]	= { GTK_STOCK_REVERT_TO_SAVED,		"system-sleep",			"gnome-session-sleep"		},
	[LOGOUT_ACTION_SWITCHUSER]	= { GTK_STOCK_JUMP_TO,			"system-users",			"system-switch-user"		},
	[LOGOUT_ACTION_SWITCHDESK]	= { GTK_STOCK_JUMP_TO,			"system-switch-user",		"gnome-session-switch"		},
	[LOGOUT_ACTION_LOCKSCREEN]	= { GTK_STOCK_DIALOG_AUTHENTICATION,	"system-lock-screen",		"gnome-lock-screen"		},
	[LOGOUT_ACTION_CHECKPOINT]	= { GTK_STOCK_SAVE,			"gtk-save",			"gtk-save"			},
	[LOGOUT_ACTION_SHUTDOWN]	= { GTK_STOCK_DELETE,			"gtk-delete",			"gtk-delete"			},
	[LOGOUT_ACTION_LOGOUT]		= { GTK_STOCK_QUIT,			"system-log-out",		"gnome-session-logout"		},
	[LOGOUT_ACTION_RESTART]		= { GTK_STOCK_REDO,			"system-run",			"gtk-refresh"			},
	[LOGOUT_ACTION_CANCEL]		= { GTK_STOCK_CANCEL,			"gtk-cancel",			"gtk-cancel"			},
	/* *INDENT-ON* */
};

/** @brief find an icon
  * @param size - icon size
  * @param icons - choices (fallback stock icon name first)
  * @param count - count of choices
  *
  * Retrieve a GtkImage for a specified icon with size.  Possible names, in
  * order of preference (with fallback stock name first) are provides along with
  * the count of choices.
  */
static GtkWidget *
get_icon(GtkIconSize size, const char *icons[], int count)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GtkWidget *image = NULL;
	int i;

	for (i = 1; i < count; i++)
		if (icons[i] && gtk_icon_theme_has_icon(theme, icons[i])
		    && (image = gtk_image_new_from_icon_name(icons[i], size)))
			break;
	if (!image && icons[0])
		image = gtk_image_new_from_stock(icons[0], size);
	return (image);
}

static void action_PowerOff(void);
static void action_Reboot(void);
static void action_Suspend(void);
static void action_Hibernate(void);
static void action_HybridSleep(void);
static void action_SwitchUser(void);
static void action_SwitchDesk(void);
static void action_LockScreen(void);
static void action_Logout(void);
static void action_Restart(void);
static void action_Cancel(void);
static void action_Checkpoint(void);
static void action_Shutdown(void);

typedef void (*ActionFunctionPointer) (void);

static const ActionFunctionPointer logout_actions[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= &action_PowerOff,
	[LOGOUT_ACTION_REBOOT]		= &action_Reboot,
	[LOGOUT_ACTION_SUSPEND]		= &action_Suspend,
	[LOGOUT_ACTION_HIBERNATE]	= &action_Hibernate,
	[LOGOUT_ACTION_HYBRIDSLEEP]	= &action_HybridSleep,
	[LOGOUT_ACTION_SWITCHUSER]	= &action_SwitchUser,
	[LOGOUT_ACTION_SWITCHDESK]	= &action_SwitchDesk,
	[LOGOUT_ACTION_LOCKSCREEN]	= &action_LockScreen,
	[LOGOUT_ACTION_CHECKPOINT]	= &action_Checkpoint,
	[LOGOUT_ACTION_SHUTDOWN]	= &action_Shutdown,
	[LOGOUT_ACTION_LOGOUT]		= &action_Logout,
	[LOGOUT_ACTION_RESTART]		= &action_Restart,
	[LOGOUT_ACTION_CANCEL]		= &action_Cancel,
	/* *INDENT-ON* */
};

static const char *check_message[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= "power off",
	[LOGOUT_ACTION_REBOOT]		= "reboot",
	[LOGOUT_ACTION_SUSPEND]		= "suspend",
	[LOGOUT_ACTION_HIBERNATE]	= "hibernate",
	[LOGOUT_ACTION_HYBRIDSLEEP]	= "hybrid sleep",
	[LOGOUT_ACTION_SWITCHUSER]	= NULL,
	[LOGOUT_ACTION_SWITCHDESK]	= NULL,
	[LOGOUT_ACTION_LOCKSCREEN]	= NULL,
	[LOGOUT_ACTION_CHECKPOINT]	= NULL,
	[LOGOUT_ACTION_SHUTDOWN]	= NULL,
	[LOGOUT_ACTION_LOGOUT]		= NULL,
	[LOGOUT_ACTION_RESTART]		= NULL,
	[LOGOUT_ACTION_CANCEL]		= NULL,
	/* *INDENT-ON* */
};

static void
on_clicked(GtkButton *button, gpointer data)
{
	LogoutActionResult action = (typeof(action)) (long) data;

	if (action_can[action] < AvailStatusChallenge) {
		EPRINTF("Button %s is disabled!\n", button_labels[action]);
		return;
	}
	if (check_message[action] && action_can[action] != AvailStatusChallenge) {
		gchar *message;
		GtkWindow *top;
		gboolean result;

		message = g_strdup_printf("Are you sure you want to %s the computer?",
					  check_message[action]);
		top = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
		result = areyousure(top, message);
		g_free(message);

		if (!result)
			return;
	}
	action_result = action;
	gtk_main_quit();
}

static void
at_pointer(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user)
{
	GdkDisplay *disp = gdk_display_get_default();

	gdk_display_get_pointer(disp, NULL, x, y, NULL);
	*push_in = TRUE;
}

void
on_switchuser(GtkButton *button, gpointer data)
{
	LogoutActionResult action = (typeof(action)) (long) data;
	GtkWidget *menu;

	if (action_can[action] < AvailStatusChallenge) {
		EPRINTF("Button %s is disabled!\n", button_labels[action]);
		return;
	}
	if (!(menu = get_user_menu())) {
		DPRINTF("No users to switch to!\n");
		return;
	}
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, at_pointer, NULL, 1, GDK_CURRENT_TIME);
	return;
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	logout_result = LOGOUT_ACTION_CANCEL;
	gtk_main_quit();
	return TRUE;		/* propagate */
}

gboolean
on_expose_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	XdeScreen *xscr = data;
	GdkWindow *w;
	GdkWindow *r;
	cairo_t *cr;
	GdkEventExpose *ev;

	w = gtk_widget_get_window(xscr->wind);
	r = gdk_screen_get_root_window(xscr->scrn);
	ev = (typeof(ev)) event;

	cr = gdk_cairo_create(GDK_DRAWABLE(w));
	// gdk_cairo_reset_clip(cr, GDK_DRAWABLE(w));
	if (ev->region)
		gdk_cairo_region(cr, ev->region);
	else
		gdk_cairo_rectangle(cr, &ev->area);
	if (xscr->pixmap) {
		gdk_cairo_set_source_pixmap(cr, xscr->pixmap, 0, 0);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
	} else {
		gdk_cairo_set_source_window(cr, r, 0, 0);
		cairo_paint(cr);
		/* only fade out root window contents */
		GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };
		gdk_cairo_set_source_color(cr, &color);
		cairo_paint_with_alpha(cr, 0.7);
	}
	cairo_destroy(cr);
	return FALSE;
}

gboolean
on_grab_broken(GtkWidget *window, GdkEvent *event, gpointer data)
{
	GdkEventGrabBroken *ev = (typeof(ev)) event;
	EPRINTF("Grab broken!\n");
	EPRINTF("Grab broken on %s\n", ev->keyboard ? "keyboard" : "pointer");
	EPRINTF("Grab broken %s\n", ev->implicit ? "implicit" : "explicit");
	EPRINTF("Grab broken by %s\n", ev->grab_window ? "this application" : "other");
	return TRUE; /* propagate */
}

/** @brief transform window into pointer-grabbed window
  * @param window - window to transform
  *
  * Transform a window into a window that has a grab on the pointer on the
  * window and restricts pointer movement to the window boundary.
  *
  * Note that the window and pointer grabs should not fail: the reason is that
  * we have mapped this window above all others and a fully obscured window
  * cannot hold the pointer or keyboard focus in X.
  */
void
grabbed_window(GtkWidget *window, gpointer user_data)
{
	GdkWindow *win = gtk_widget_get_window(window);
	GdkEventMask mask = GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

	gdk_window_set_override_redirect(win, TRUE);
	gdk_window_set_focus_on_map(win, TRUE);
	gdk_window_set_accept_focus(win, TRUE);
	gdk_window_set_keep_above(win, TRUE);
	gdk_window_set_modal_hint(win, TRUE);
	gdk_window_stick(win);
	gdk_window_deiconify(win);
	gdk_window_show(win);
	gdk_window_focus(win, GDK_CURRENT_TIME);
	if (gdk_keyboard_grab(win, TRUE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		EPRINTF("Could not grab keyboard!\n");
	else
		DPRINTF("Grabbed keyboard\n");
	if (gdk_pointer_grab(win, TRUE, mask, win, NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		EPRINTF("Could not grab pointer!\n");
	else
		DPRINTF("Grabbed pointer\n");
#if !defined(DO_CHOOSER) && !defined(DO_LOGOUT)
	grab_broken_handler = g_signal_connect(G_OBJECT(window), "grab-broken-event", G_CALLBACK(on_grab_broken), NULL);
#endif
}

/** @brief transform a window away from a grabbed window
  *
  * Transform the window back into a regular window, releasing the pointer and
  * keyboard grab and motion restriction.  window is the GtkWindow that
  * previously had the grabbed_window() method called on it.
  */
void
ungrabbed_window(GtkWidget *window)
{
	GdkWindow *win = gtk_widget_get_window(window);

#if !defined(DO_CHOOSER) && !defined(DO_LOGOUT)
	if (grab_broken_handler) {
		g_signal_handler_disconnect(G_OBJECT(window), grab_broken_handler);
		grab_broken_handler = 0;
	}
	g_signal_connect(G_OBJECT(window), "grab-broken-event", NULL, NULL);
#endif
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_window_hide(win);
}

/** @brief render a pixbuf into a pixmap for a monitor
  */
void
render_pixbuf_for_mon(cairo_t * cr, GdkPixbuf *pixbuf, double wp, double hp, XdeMonitor *xmon)
{
	double wm = xmon->geom.width;
	double hm = xmon->geom.height;
	GdkPixbuf *scaled = NULL;

	DPRINT();
	gdk_cairo_rectangle(cr, &xmon->geom);
	if (wp >= 0.8 * wm && hp >= 0.8 * hm) {
		/* good size for filling or scaling */
		/* TODO: check aspect ratio before scaling */
		DPRINTF("scaling pixbuf from %dx%d to %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		scaled = gdk_pixbuf_scale_simple(pixbuf,
						 xmon->geom.width,
						 xmon->geom.height, GDK_INTERP_BILINEAR);
		gdk_cairo_set_source_pixbuf(cr, scaled, xmon->geom.x, xmon->geom.y);
	} else if (wp <= 0.5 * wm && hp <= 0.5 * hm) {
		/* good size for tiling */
		DPRINTF("tiling pixbuf at %dx%d into %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		gdk_cairo_set_source_pixbuf(cr, pixbuf, xmon->geom.x, xmon->geom.y);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	} else {
		/* somewhere in between: scale down for integer tile */
		DPRINTF("scaling and tiling pixbuf at %dx%d into %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		scaled = gdk_pixbuf_scale_simple(pixbuf,
						 xmon->geom.width / 2,
						 xmon->geom.height / 2, GDK_INTERP_BILINEAR);
		gdk_cairo_set_source_pixbuf(cr, scaled, xmon->geom.x, xmon->geom.y);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	}
	cairo_paint(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	if (scaled)
		g_object_unref(G_OBJECT(scaled));
}

void
render_pixbuf_for_scr(GdkPixbuf *pixbuf, GdkPixmap *pixmap, XdeScreen *xscr)
{
	double w = gdk_pixbuf_get_width(pixbuf);
	double h = gdk_pixbuf_get_height(pixbuf);
	cairo_t *cr;
	int m;

	DPRINT();
	cr = gdk_cairo_create(GDK_DRAWABLE(pixmap));
	for (m = 0; m < xscr->nmon; m++) {
		XdeMonitor *xmon = xscr->mons + m;

		render_pixbuf_for_mon(cr, pixbuf, w, h, xmon);
	}
	cairo_destroy(cr);
}

void
clr_source(XdeScreen *xscr)
{
	GdkWindow *w;

	if (xscr->wind && (w = gtk_widget_get_window(xscr->wind))) {
		gint x = 0, y = 0, width = 0, height = 0, depth = 0;

		gdk_window_get_geometry(w, &x, &y, &width, &height, &depth);
		gdk_window_clear_area_e(w, x, y, width, height);
	}
}

GtkStyle *style;

void
set_style(XdeScreen *xscr)
{
	GtkWidget *window = xscr->wind;

	if (!style) {
		style = gtk_widget_get_default_style();
		style = gtk_style_copy(style);
	}
	style->bg_pixmap[GTK_STATE_NORMAL] = xscr->pixmap;
	style->bg_pixmap[GTK_STATE_PRELIGHT] = xscr->pixmap;
	gtk_widget_set_style(window, style);
}

void
update_source(XdeScreen *xscr)
{
	if (options.setstyle)
		clr_source(xscr);
	else
		set_style(xscr);
	if (xscr->pixmap && options.setbg) {
		GdkDisplay *disp;
		GdkWindow *root;
		GdkColormap *cmap;
		GdkPixmap *pixmap;
		Display *dpy;
		Pixmap p;
		int s;
		cairo_t *cr;
		long data;

		s = xscr->index;
		if (!(dpy = XOpenDisplay(NULL))) {
			EPRINTF("cannot open display %s\n", getenv("DISPLAY"));
			return;
		}
		XSetCloseDownMode(dpy, RetainTemporary);
		p = XCreatePixmap(dpy, RootWindow(dpy, s),
				  DisplayWidth(dpy, s),
				  DisplayHeight(dpy, s), DefaultDepth(dpy, s));

		XCloseDisplay(dpy);

		disp = gdk_screen_get_display(xscr->scrn);
		root = gdk_screen_get_root_window(xscr->scrn);
		cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
		pixmap = gdk_pixmap_foreign_new_for_display(disp, p);
		gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), cmap);

		cr = gdk_cairo_create(GDK_DRAWABLE(pixmap));
		gdk_cairo_set_source_pixmap(cr, xscr->pixmap, 0, 0);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
		cairo_destroy(cr);

		g_object_unref(G_OBJECT(pixmap));

		dpy = GDK_DISPLAY_XDISPLAY(disp);
		XSetWindowBackgroundPixmap(dpy, RootWindow(dpy, s), p);

		data = p;
		XChangeProperty(dpy, RootWindow(dpy, s),
				_XA_XROOTPMAP_ID, XA_PIXMAP,
				32, PropModeReplace, (unsigned char *) &data, 1);
		XKillClient(dpy, AllTemporary);
	}
}

static void
get_source(XdeScreen *xscr)
{
	GdkDisplay *disp = gdk_screen_get_display(xscr->scrn);
	GdkWindow *root = gdk_screen_get_root_window(xscr->scrn);
	GdkColormap *cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom prop;

	if (options.source & BackgroundSourcePixmap) {
		if (!(prop = _XA_XROOTPMAP_ID))
			prop = _XA_ESETROOT_PMAP_ID;

		if (prop) {
			Window w = GDK_WINDOW_XID(root);
			Atom actual = None;
			int format = 0;
			unsigned long nitems = 0, after = 0;
			long *data = NULL;

			if (XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_PIXMAP,
					       &actual, &format, &nitems, &after,
					       (unsigned char **) &data) == Success &&
			    format == 32 && actual && nitems >= 1 && data) {
				Pixmap p;

				if ((p = data[0])) {
					xscr->pixmap = gdk_pixmap_foreign_new_for_display(disp, p);
					gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
					update_source(xscr);
				}
			}
			if (data)
				XFree(data);
		}
		if (xscr->pixmap)
			return;
	}
	if (options.source & BackgroundSourceSplash) {
		if (!xscr->pixbuf && options.splash) {
			if (!(xscr->pixbuf = gdk_pixbuf_new_from_file(options.splash, NULL))) {
				/* cannot use it again */
				free(options.splash);
				options.splash = NULL;
			}
		}
		if (xscr->pixbuf) {
			if (!xscr->pixmap) {
				xscr->pixmap =
				    gdk_pixmap_new(GDK_DRAWABLE(root), xscr->width, xscr->height,
						   -1);
				gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
				render_pixbuf_for_scr(xscr->pixbuf, xscr->pixmap, xscr);
				update_source(xscr);
			}
			return;
		}
	}
	if (options.source & BackgroundSourceRoot) {
		if (!xscr->pixmap) {
			cairo_t *cr;
			GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };

			xscr->pixmap =
				gdk_pixmap_new(GDK_DRAWABLE(root), xscr->width,
						xscr->height, -1);
			gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
			cr = gdk_cairo_create(GDK_DRAWABLE(xscr->pixmap));
			gdk_cairo_set_source_window(cr, root, 0, 0);
			cairo_paint(cr);
			gdk_cairo_set_source_color(cr, &color);
			cairo_paint_with_alpha(cr, 0.7);
			update_source(xscr);
		}
	}
}

static void
redo_source(XdeScreen *xscr)
{
	if (xscr->pixmap) {
		g_object_unref(G_OBJECT(xscr->pixmap));
		xscr->pixmap = NULL;
	}
	if (xscr->pixbuf) {
		g_object_unref(G_OBJECT(xscr->pixbuf));
		xscr->pixbuf = NULL;
	}
	get_source(xscr);
}

#ifdef DO_CHOOSER
/** @brief create the selected session
  * @param label - the application id of the XSession
  * @param filename - the desktop entry file name for the XSession
  *
  * Launch the session specified by the label argument with the xsession desktop
  * file pass in the session argument.  This function writes the selection and
  * default to the user's current and default files in
  * $XDG_CONFIG_HOME/xde/current and $XDG_CONFIG_HOME/xde/default, sets the
  * option variables options.current and options.session.  A NULL session
  * pointer means that a logout should be performed instead.
  */
void
create_session(const char *label, const char *filename)
{
	char *home = getenv("HOME") ? : ".";
	char *xhome = getenv("XDG_CONFIG_HOME");
	char *cdir, *file;
	int len, dlen, flen;
	FILE *f;

	len = xhome ? strlen(xhome) : strlen(home) + strlen("/.config");
	dlen = len + strlen("/xde");
	flen = dlen + strlen("/default");
	cdir = calloc(dlen, sizeof(*cdir));
	file = calloc(flen, sizeof(*file));
	if (xhome)
		strcpy(cdir, xhome);
	else {
		strcpy(cdir, home);
		strcat(cdir, "/.config");
	}
	strcat(cdir, "/xde");

	strcpy(file, cdir);
	strcat(file, "/current");
	if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
		if ((f = fopen(file, "w"))) {
			fprintf(f, "%s\n", options.current ? : "");
			fclose(f);
		}
	}

	if (options.setdflt) {
		strcpy(file, cdir);
		strcat(file, "/default");
		if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
			if ((f = fopen(file, "w"))) {
				fprintf(f, "%s\n", options.session ? : "");
				fclose(f);
			}
		}
		if (options.session && options.dmrc) {
			char *dmrc;

			len = strlen(home) + strlen("/.dmrc");
			dmrc = calloc(len + 1, sizeof(*dmrc));
			strncpy(dmrc, home, len);
			strncat(dmrc, "/.dmrc", len);

			g_key_file_set_string(options.dmrc, "Desktop", "Session", options.session);
			g_key_file_save_to_file(options.dmrc, dmrc, NULL);

			free(dmrc);
		}
	}

	free(file);
	free(cdir);
}
#endif

GtkWidget *cont;			/* container of event box */
GtkWidget *ebox;			/* event box window within the screen */

static void
RefreshScreen(XdeScreen *xscr, GdkScreen *scrn)
{
	XdeMonitor *mon;
	GtkWindow *w = GTK_WINDOW(xscr->wind);
	GdkWindow *win = gtk_widget_get_window(xscr->wind);
	char *geom;
	int m, nmon, width, height, index;

	index = gdk_screen_get_number(scrn);
	if (xscr->index != index) {
		EPRINTF("Arrrghhhh! screen index changed from %d to %d\n", xscr->index, index);
		xscr->index = index;
	}

	if (xscr->scrn != scrn) {
		DPRINTF("Arrrghhh! screen pointer changed from %p to %p\n", xscr->scrn, scrn);
		xscr->scrn = scrn;
	}
	width = gdk_screen_get_width(scrn);
	height = gdk_screen_get_height(scrn);
	if (xscr->width != width || xscr->height != height) {
		DPRINTF("Screen %d dimensions changed %dx%d -> %dx%d\n", index,
			xscr->width, xscr->height, width, height);
		gtk_window_set_default_size(w, width, height);
		geom = g_strdup_printf("%dx%d+0+0", width, height);
		gtk_window_parse_geometry(w, geom);
		g_free(geom);
		gtk_widget_set_size_request(GTK_WIDGET(w), width, height);
		gtk_window_resize(w, width, height);
		gtk_window_move(w, 0, 0);
		gdk_window_move_resize(win, 0, 0, width, height);
		xscr->width = width;
		xscr->height = height;
	}
	nmon = gdk_screen_get_n_monitors(scrn);
	if (nmon > xscr->nmon) {
		DPRINTF("Screen %d number of monitors increased from %d to %d\n",
			index, xscr->nmon, nmon);
		xscr->mons = realloc(xscr->mons, nmon * sizeof(*xscr->mons));
		for (m = xscr->nmon; m <= nmon; m++) {
			mon = xscr->mons + m;
			memset(mon, 0, sizeof(*mon));
		}
	} else if (nmon < xscr->nmon) {
		DPRINTF("Screen %d number of monitors decreased from %d to %d\n",
			index, xscr->nmon, nmon);
		for (m = xscr->nmon; m > nmon; m--) {
			mon = xscr->mons + m - 1;
			if (ebox && cont && mon->align == cont) {
				gtk_container_remove(GTK_CONTAINER(cont), ebox);
				cont = NULL;
			}
			if (mon->align) {
				gtk_widget_destroy(mon->align);
				mon->align = NULL;
			}
		}
		xscr->mons = realloc(xscr->mons, nmon * sizeof(*xscr->mons));
	}
	if (nmon != xscr->nmon)
		xscr->nmon = nmon;

	/* always realign center alignment widgets */
	for (m = 0, mon = xscr->mons; m < nmon; m++, mon++) {
		double xrel, yrel;

		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (double) (mon->geom.x + mon->geom.width * options.xposition) / (double) xscr->width;
		yrel = (double) (mon->geom.x + mon->geom.height * options.yposition) / (double) xscr->height;
		if (!mon->align) {
			mon->align = gtk_alignment_new(xrel, yrel, 0, 0);
			gtk_container_add(GTK_CONTAINER(w), mon->align);
		} else
			gtk_alignment_set(GTK_ALIGNMENT(mon->align), xrel, yrel, 0, 0);
	}
	/* always reassign the event box if its containing monitor was removed */
	if (ebox && !cont) {
		GdkDisplay *disp = gdk_screen_get_display(scrn);
		GdkScreen *screen = NULL;
		gint x = 0, y = 0;
		XdeScreen *_xscr;
		XdeMonitor *_xmon;

		DPRINTF("Reassigning event box to new container\n");
		gdk_display_get_pointer(disp, &screen, &x, &y, NULL);
		if (!screen)
			screen = scrn;
		_xscr = screens + gdk_screen_get_number(screen);
		_xmon = _xscr->mons + gdk_screen_get_monitor_at_point(screen, x, y);
		cont = _xmon->align;
		gtk_container_add(GTK_CONTAINER(cont), ebox);
#if 0
		/* FIXME: only if it should be currently displayed */
		gtk_widget_show_all(cont);
		gtk_widget_show_now(cont);
		gtk_widget_grab_default(user);
		gtk_widget_grab_focus(user);
#endif
	}
	/* redo background images */
	redo_source(xscr);
}

/** @brief monitors changed
  *
  * The number and/or size of monitors belonging to a screen have changed.  This
  * may be as a result of RANDR or XINERAMA changes.  Walk through the monitors
  * and adjust the necessary parameters.
  */
static void
on_monitors_changed(GdkScreen *scrn, gpointer data)
{
	XdeScreen *xscr = data;

	RefreshScreen(xscr, scrn);
}

/** @brief screen size changed
  *
  * The size of the screen changed.  This may be as a result of RANDR or
  * XINERAMA changes.  Walk through the screen and the monitors on the screen
  * and adjust the necessary parameters.
  */
static void
on_size_changed(GdkScreen *scrn, gpointer data)
{
	XdeScreen *xscr = data;

	RefreshScreen(xscr, scrn);
}

/** @brief get a covering window for a screen
  */
void
GetScreen(XdeScreen *xscr, int s, GdkScreen *scrn, Bool noshow)
{
	GtkWidget *wind;
	GtkWindow *w;
	int m;
	XdeMonitor *mon;

	xscr->index = s;
	xscr->scrn = scrn;
	xscr->wind = wind = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	xscr->width = gdk_screen_get_width(scrn);
	xscr->height = gdk_screen_get_height(scrn);

	g_signal_connect(G_OBJECT(scrn), "monitors-changed", G_CALLBACK(on_monitors_changed), xscr);
	g_signal_connect(G_OBJECT(scrn), "size-changed", G_CALLBACK(on_size_changed), xscr);

	w = GTK_WINDOW(wind);
	gtk_window_set_screen(w, scrn);
	gtk_window_set_wmclass(w, RESNAME, RESCLAS);
	gtk_window_set_title(w, RESTITL);
	gtk_window_set_modal(w, TRUE);
	gtk_window_set_gravity(w, GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_window_set_icon_name(w, "xdm");
	gtk_window_set_skip_pager_hint(w, TRUE);
	gtk_window_set_skip_taskbar_hint(w, TRUE);
	gtk_window_set_position(w, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_fullscreen(w);
	gtk_window_set_decorated(w, FALSE);
	gtk_window_set_default_size(w, xscr->width, xscr->height);

	char *geom = g_strdup_printf("%dx%d+0+0", xscr->width, xscr->height);

	gtk_window_parse_geometry(w, geom);
	g_free(geom);

	gtk_widget_set_size_request(GTK_WIDGET(w), xscr->width, xscr->height);
	gtk_window_resize(w, xscr->width, xscr->height);
	gtk_window_move(w, 0, 0);

	g_signal_connect(G_OBJECT(w), "destroy", G_CALLBACK(on_destroy), NULL);
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(on_delete_event), NULL);

	if (options.setstyle) {
		gtk_widget_set_app_paintable(wind, TRUE);
		g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(on_expose_event), xscr);
	}

	gtk_window_set_focus_on_map(w, TRUE);
	gtk_window_set_accept_focus(w, TRUE);
	gtk_window_set_keep_above(w, TRUE);
	gtk_window_set_modal(w, TRUE);
	gtk_window_stick(w);
	gtk_window_deiconify(w);

	xscr->nmon = gdk_screen_get_n_monitors(scrn);
	xscr->mons = calloc(xscr->nmon, sizeof(*xscr->mons));
	for (m = 0, mon = xscr->mons; m < xscr->nmon; m++, mon++) {
		double xrel, yrel;

		mon->index = m;
		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (double) (mon->geom.x + mon->geom.width * options.xposition) / (double) xscr->width;
		yrel = (double) (mon->geom.x + mon->geom.height * options.yposition) / (double) xscr->height;
		mon->align = gtk_alignment_new(xrel, yrel, 0, 0);
		gtk_container_add(GTK_CONTAINER(w), mon->align);
	}
	redo_source(xscr);
	if (!noshow)
		gtk_widget_show_all(wind);
	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);

	gdk_window_set_override_redirect(win, TRUE);
	gdk_window_move_resize(win, 0, 0, xscr->width, xscr->height);

	GdkDisplay *disp = gdk_screen_get_display(scrn);
	GdkCursor *curs = gdk_cursor_new_for_display(disp, GDK_LEFT_PTR);

	gdk_window_set_cursor(win, curs);
	gdk_cursor_unref(curs);

	GdkWindow *root = gdk_screen_get_root_window(scrn);
	GdkEventMask mask = gdk_window_get_events(root);

	gdk_window_add_filter(root, root_handler, xscr);
	mask |= GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
	gdk_window_set_events(root, mask);

#ifdef DO_XLOCKING
	Window owner = None;
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	xscr->selwin = XCreateSimpleWindow(dpy, GDK_WINDOW_XID(root), 0, 0, 1, 1, 0, 0, 0);
	if ((owner = get_selection(xscr->selwin, xscr->selection, s))) {
		if (!options.replace) {
			XDestroyWindow(dpy, xscr->selwin);
			EPRINTF("%s: instance already running\n", NAME);
			exit(EXIT_FAILURE);
		}
	}
	GdkWindow *sel = gdk_x11_window_foreign_new_for_display(disp, xscr->selwin);

	gdk_window_add_filter(sel, selwin_handler, xscr);
	mask = gdk_window_get_events(sel);
	mask |= GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK;
	gdk_window_set_events(sel, mask);
#endif				/* DO_XLOCKING */
}

static void
reparse(Display *dpy, Window root)
{
	XTextProperty xtp = { NULL, };
	char **list = NULL;
	int strings = 0;

	DPRINT();
	gtk_rc_reparse_all();
	if (!options.usexde)
		return;
	if (XGetTextProperty(dpy, root, &xtp, _XA_XDE_THEME_NAME)) {
		if (Xutf8TextPropertyToTextList(dpy, &xtp, &list, &strings) == Success) {
			if (strings >= 1) {
				static const char *prefix = "gtk-theme-name=\"";
				static const char *suffix = "\"";
				char *rc_string;
				int len;

				len = strlen(prefix) + strlen(list[0]) + strlen(suffix) + 1;
				rc_string = calloc(len, sizeof(*rc_string));
				strncpy(rc_string, prefix, len);
				strncat(rc_string, list[0], len);
				strncat(rc_string, suffix, len);
				gtk_rc_parse_string(rc_string);
				free(rc_string);
			}
			if (list)
				XFreeStringList(list);
		} else
			DPRINTF("could not get text list for property\n");
		if (xtp.value)
			XFree(xtp.value);
	} else
		DPRINTF("could not get _XDE_THEME_NAME for root 0x%lx\n", root);
}

/** @brief get a covering window for each screen
  */
void
GetScreens(Bool noshow)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	screens = calloc(nscr, sizeof(*screens));

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		GetScreen(xscr, s, gdk_display_get_screen(disp, s), noshow);

	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn);

	reparse(dpy, GDK_WINDOW_XID(root));
}

GtkWidget *
GetBanner(void)
{
	GtkWidget *ban = NULL, *bin, *pan, *img;

	if (options.banner && (img = gtk_image_new_from_file(options.banner))) {
		GtkShadowType shadow = (options.transparent) ? GTK_SHADOW_NONE : GTK_SHADOW_ETCHED_IN;
		ban = gtk_vbox_new(FALSE, 0);
		bin = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(bin), shadow);
		gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
		gtk_box_pack_start(GTK_BOX(ban), bin, TRUE, TRUE, 4);
		pan = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
		gtk_container_set_border_width(GTK_CONTAINER(pan), 15);
		gtk_container_add(GTK_CONTAINER(bin), pan);
		gtk_container_add(GTK_CONTAINER(pan), img);
	}
	return (ban);
}

#define BB_INT_PADDING  0
#define BB_BOX_SPACING  5
#define BB_BORDER_WIDTH 5
#define BU_BORDER_WIDTH 0
#define BB_PACK_PADDING 0

GtkWidget *
GetPanel(void)
{
	GtkWidget *pan = gtk_vbox_new(FALSE, 0);
	GtkShadowType shadow = (options.transparent) ? GTK_SHADOW_NONE : GTK_SHADOW_ETCHED_IN;
	GtkWidget *inp = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(inp), shadow);
	gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

#ifdef DO_XCHOOSER
	gtk_box_pack_start(GTK_BOX(pan), inp, FALSE, FALSE, 4);
#else
	gtk_box_pack_start(GTK_BOX(pan), inp, TRUE, TRUE, 4);
#endif
	GtkWidget *bb = gtk_vbutton_box_new();

	gtk_container_set_border_width(GTK_CONTAINER(bb), BB_BORDER_WIDTH);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_EDGE);
	gtk_button_box_set_child_ipadding(GTK_BUTTON_BOX(bb), BB_INT_PADDING, BB_INT_PADDING);
	gtk_box_set_spacing(GTK_BOX(bb), BB_BOX_SPACING);
	gtk_container_add(GTK_CONTAINER(inp), bb);

	GtkWidget *b, *im;
	int i;

	for (i = 0; i < LOGOUT_ACTION_COUNT; i++) {
		if (action_can[i] < AvailStatusChallenge)
			continue;
		if (i == LOGOUT_ACTION_SWITCHUSER) {
			b = buttons[i] = gtk_button_new();
			gtk_container_set_border_width(GTK_CONTAINER(b), BU_BORDER_WIDTH);
			gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
			gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
			im = get_icon(GTK_ICON_SIZE_BUTTON, button_icons[i], 3);
			gtk_button_set_image(GTK_BUTTON(b), im);
			gtk_button_set_label(GTK_BUTTON(b), button_labels[i]);
			gtk_widget_set_can_default(b, TRUE);
			g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_switchuser),
					 (gpointer) (long) i);
		} else {
			b = buttons[i] = gtk_button_new();
			gtk_container_set_border_width(GTK_CONTAINER(b), BU_BORDER_WIDTH);
			gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
			gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
			im = get_icon(GTK_ICON_SIZE_BUTTON, button_icons[i], 3);
			gtk_button_set_image(GTK_BUTTON(b), im);
			gtk_button_set_label(GTK_BUTTON(b), button_labels[i]);
			gtk_widget_set_can_default(b, TRUE);
			g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_clicked),
					 (gpointer) (long) i);
		}
		gtk_widget_set_sensitive(b, (action_can[i] >= AvailStatusChallenge));
		gtk_widget_set_tooltip_text(b, button_tips[i]);
		gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, BB_PACK_PADDING);
	}
#if 0
	if (options.xsession) {
#ifdef DO_ONIDLE
		g_idle_add(on_idle, store);
#else
		while (on_idle(store) != G_SOURCE_REMOVE) ;
#endif
	}
#endif

	/* TODO: we should really set a timeout and if no user interaction has
	   occured before the timeout, we should continue if we have a viable
	   default or choice. */

	return (pan);
}

GtkWidget *
GetPane(GtkWidget *cont)
{
	GtkRcStyle *style;
	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	if (options.transparent)
		ebox = gtk_hbox_new(FALSE, 0);
	else
		ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(cont), ebox);
	gtk_widget_set_size_request(ebox, -1, -1);

	GtkWidget *v = gtk_vbox_new(FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(v), 10);

	gtk_container_add(GTK_CONTAINER(ebox), v);

	GtkWidget *l_greet = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l_greet), options.welcome);
	gtk_misc_set_alignment(GTK_MISC(l_greet), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_greet), 3, 3);
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideBottom:
		gtk_box_pack_start(GTK_BOX(v), l_greet, FALSE, TRUE, 0);
		break;
	case LogoSideRight:
	case LogoSideTop:
		gtk_box_pack_end(GTK_BOX(v), l_greet, FALSE, TRUE, 0);
		break;
	}
	if ((style = gtk_widget_get_modifier_style(l_greet))) {
		style->font_desc = pango_font_description_copy(resources.greetFace);
		if (options.transparent) {
			if (resources.greetColor) {
				int i;

				for (i = 0; i < 5; i++) {
					style->text[i] = *resources.greetColor;
					style->color_flags[i] |= GTK_RC_TEXT;
					style->fg[i] = *resources.greetColor;
					style->color_flags[i] |= GTK_RC_FG;
					// style->base[i] = *resources.greetColor;
					// style->color_flags[i] |= GTK_RC_BASE;
				}
			} else
				DPRINTF("No resources.greetColor!\n");
		}
		gtk_widget_modify_style(l_greet, style);
	}

	GtkWidget *box;
	
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideRight:
		box = gtk_hbox_new(FALSE, 5);
		break;
	case LogoSideTop:
	case LogoSideBottom:
		box = gtk_vbox_new(TRUE, 5);
		break;
	}

	gtk_box_pack_end(GTK_BOX(v), box, TRUE, TRUE, 0);

	if ((v = GetBanner())) {
		switch (options.side) {
		default:
		case LogoSideLeft:
		case LogoSideTop:
			gtk_box_pack_start(GTK_BOX(box), v, TRUE, TRUE, 0);
			break;
		case LogoSideRight:
		case LogoSideBottom:
			gtk_box_pack_end(GTK_BOX(box), v, TRUE, TRUE, 0);
			break;
		}
	}

	v = GetPanel();
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideTop:
		gtk_box_pack_end(GTK_BOX(box), v, TRUE, TRUE, 0);
		break;
	case LogoSideRight:
	case LogoSideBottom:
		gtk_box_pack_start(GTK_BOX(box), v, TRUE, TRUE, 0);
		break;
	}

	return (ebox);
}

GtkWidget *
GetWindow(Bool noshow)
{
	GdkDisplay *disp = gdk_display_get_default();
	GdkScreen *scrn = NULL;
	XdeScreen *xscr;
	XdeMonitor *xmon;
	gint x = 0, y = 0;

	GetScreens(noshow);

	gdk_display_get_pointer(disp, &scrn, &x, &y, NULL);
	if (!scrn)
		scrn = gdk_display_get_default_screen(disp);
	xscr = screens + gdk_screen_get_number(scrn);
	xmon = xscr->mons + gdk_screen_get_monitor_at_point(scrn, x, y);

	cont = xmon->align;
	ebox = GetPane(cont);

	gtk_widget_show_all(cont);
	gtk_widget_show_now(cont);

	if (!noshow) {
		gtk_widget_grab_default(buttons[LOGOUT_ACTION_LOGOUT]);
		gtk_widget_grab_focus(buttons[LOGOUT_ACTION_LOGOUT]);
		grabbed_window(xscr->wind, NULL);
	}
	return xscr->wind;
}

static void
startup(int argc, char *argv[])
{
	if (options.usexde) {
		static const char *suffix = "/.gtkrc-2.0.xde";
		const char *home;
		int len;
		char *file;

		home = getenv("HOME") ? : ".";
		len = strlen(home) + strlen(suffix) + 1;
		file = calloc(len, sizeof(*file));

		strncpy(file, home, len);
		strncat(file, suffix, len);
		gtk_rc_add_default_file(file);
		free(file);
	}

	gtk_init(&argc, &argv);

	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	GdkAtom atom;

	atom = gdk_atom_intern_static_string("_XDE_THEME_NAME");
	_XA_XDE_THEME_NAME = gdk_x11_atom_to_xatom_for_display(disp, atom);
	atom = gdk_atom_intern_static_string("_GTK_READ_RCFILES");
	_XA_GTK_READ_RCFILES = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);
	atom = gdk_atom_intern_static_string("_XROOTPMAP_ID");
	_XA_XROOTPMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);
	atom = gdk_atom_intern_static_string("ESETROOT_PMAP_ID");
	_XA_ESETROOT_PMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);
}

void
ShowScreen(XdeScreen *xscr)
{
	if (xscr->wind) {
		gtk_widget_show_now(GTK_WIDGET(xscr->wind));
#if 0
		if (options.username) {
			gtk_widget_set_sensitive(user, FALSE);
			gtk_widget_set_sensitive(pass, TRUE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
		} else {
			gtk_widget_set_sensitive(user, TRUE);
			gtk_widget_set_sensitive(pass, FALSE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
		}
#else
		gtk_widget_grab_default(buttons[LOGOUT_ACTION_LOGOUT]);
		gtk_widget_grab_focus(buttons[LOGOUT_ACTION_LOGOUT]);
#endif
		grabbed_window(GTK_WIDGET(xscr->wind), NULL);
	}
}

void
ShowScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		ShowScreen(xscr);
}

void
ShowWindow(void)
{
	ShowScreens();
}

void
HideScreen(XdeScreen *xscr)
{
	if (xscr->wind) {
		ungrabbed_window(GTK_WIDGET(xscr->wind));
		gtk_widget_hide(GTK_WIDGET(xscr->wind));
	}
}

void
HideScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		HideScreen(xscr);
}

void
HideWindow(void)
{
	HideScreens();
}

static void
xdeSetProperties(SmcConn smcConn, SmPointer data)
{
	char userID[20];
	int i, j, argc = saveArgc;
	char **argv = saveArgv;
	char *cwd = NULL;
	char hint;
	struct passwd *pw;
	SmPropValue *penv = NULL, *prst = NULL, *pcln = NULL;
	SmPropValue propval[11];
	SmProp prop[11];

	SmProp *props[11] = {
		&prop[0], &prop[1], &prop[2], &prop[3], &prop[4],
		&prop[5], &prop[6], &prop[7], &prop[8], &prop[9],
		&prop[10]
	};

	j = 0;

	/* CloneCommand: This is like the RestartCommand except it restarts a
	   copy of the application.  The only difference is that the
	   application doesn't supply its client id at register time.  On POSIX 
	   systems the type should be a LISTofARRAY8. */
	prop[j].name = SmCloneCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = pcln = calloc(argc, sizeof(*pcln));
	prop[j].num_vals = 0;
	props[j] = &prop[j];
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[j].vals[prop[j].num_vals].value = (SmPointer) argv[i];
			prop[j].vals[prop[j].num_vals++].length = strlen(argv[i]);
		}
	}
	j++;

#if 0
	/* CurrentDirectory: On POSIX-based systems, specifies the value of the 
	   current directory that needs to be set up prior to starting the
	   program and should be of type ARRAY8. */
	prop[j].name = SmCurrentDirectory;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = NULL;
	propval[j].length = 0;
	cwd = calloc(PATH_MAX + 1, sizeof(propval[j].value[0]));
	if (getcwd(cwd, PATH_MAX)) {
		propval[j].value = cwd;
		propval[j].length = strlen(propval[j].value);
		j++;
	} else {
		free(cwd);
		cwd = NULL;
	}
#endif

#if 0
	/* DiscardCommand: The discard command contains a command that when
	   delivered to the host that the client is running on (determined from 
	   the connection), will cause it to discard any information about the
	   current state.  If this command is not specified, the SM will assume 
	   that all of the client's state is encoded in the RestartCommand [and 
	   properties].  On POSIX systems the type should be LISTofARRAY8. */
	prop[j].name = SmDiscardCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

#if 0
	char **env;

	/* Environment: On POSIX based systems, this will be of type
	   LISTofARRAY8 where the ARRAY8s alternate between environment
	   variable name and environment variable value. */
	/* XXX: we might want to filter a few out */
	for (i = 0, env = environ; *env; i += 2, env++) ;
	prop[j].name = SmEnvironment;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = penv = calloc(i, sizeof(*penv));
	prop[j].num_vals = i;
	props[j] = &prop[j];
	for (i = 0, env = environ; *env; i += 2, env++) {
		char *equal;
		int len;

		equal = strchrnul(*env, '=');
		len = (int) (*env - equal);
		if (*equal)
			equal++;
		prop[j].vals[i].value = *env;
		prop[j].vals[i].length = len;
		prop[j].vals[i + 1].value = equal;
		prop[j].vals[i + 1].length = strlen(equal);
	}
	j++;
#endif

#if 0
	char procID[20];

	/* ProcessID: This specifies an OS-specific identifier for the process. 
	   On POSIX systems this should be of type ARRAY8 and contain the
	   return of getpid() turned into a Latin-1 (decimal) string. */
	prop[j].name = SmProcessID;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	snprintf(procID, sizeof(procID), "%ld", (long) getpid());
	propval[j].value = procID;
	propval[j].length = strlen(procID);
	j++;
#endif

	/* Program: The name of the program that is running.  On POSIX systems, 
	   this should eb the first parameter passed to execve(3) and should be 
	   of type ARRAY8. */
	prop[j].name = SmProgram;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = argv[0];
	propval[j].length = strlen(argv[0]);
	j++;

	/* RestartCommand: The restart command contains a command that when
	   delivered to the host that the client is running on (determined from
	   the connection), will cause the client to restart in its current
	   state.  On POSIX-based systems this if of type LISTofARRAY8 and each
	   of the elements in the array represents an element in the argv[]
	   array.  This restart command should ensure that the client restarts
	   with the specified client-ID.  */
	prop[j].name = SmRestartCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = prst = calloc(argc + 4, sizeof(*prst));
	prop[j].num_vals = 0;
	props[j] = &prop[j];
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[j].vals[prop[j].num_vals].value = (SmPointer) argv[i];
			prop[j].vals[prop[j].num_vals++].length = strlen(argv[i]);
		}
	}
	prop[j].vals[prop[j].num_vals].value = (SmPointer) "-clientId";
	prop[j].vals[prop[j].num_vals++].length = 9;
	prop[j].vals[prop[j].num_vals].value = (SmPointer) options.clientId;
	prop[j].vals[prop[j].num_vals++].length = strlen(options.clientId);

	prop[j].vals[prop[j].num_vals].value = (SmPointer) "-restore";
	prop[j].vals[prop[j].num_vals++].length = 9;
	prop[j].vals[prop[j].num_vals].value = (SmPointer) options.saveFile;
	prop[j].vals[prop[j].num_vals++].length = strlen(options.saveFile);
	j++;

#if 0
	/* ResignCommand: A client that sets the RestartStyleHint to
	   RestartAnyway uses this property to specify a command that undoes
	   the effect of the client and removes any saved state. */
	prop[j].name = SmResignCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

	/* RestartStyleHint: If the RestartStyleHint property is present, it
	   will contain the style of restarting the client prefers.  If this
	   flag is not specified, RestartIfRunning is assumed.  The possible
	   values are as follows: RestartIfRunning(0), RestartAnyway(1),
	   RestartImmediately(2), RestartNever(3).  The RestartIfRunning(0)
	   style is used in the usual case.  The client should be restarted in
	   the next session if it is connected to the session manager at the
	   end of the current session. The RestartAnyway(1) style is used to
	   tell the SM that the application should be restarted in the next
	   session even if it exits before the current session is terminated.
	   It should be noted that this is only a hint and the SM will follow
	   the policies specified by its users in determining what applications 
	   to restart.  A client that uses RestartAnyway(1) should also set the
	   ResignCommand and ShutdownCommand properties to the commands that
	   undo the state of the client after it exits.  The
	   RestartImmediately(2) style is like RestartAnyway(1) but in addition,
	   the client is meant to run continuously.  If the client exits, the SM
	   should try to restart it in the current session.  The RestartNever(3)
	   style specifies that the client does not wish to be restarted in the
	   next session. */
	prop[j].name = SmRestartStyleHint;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[0];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	hint = SmRestartNever;
	propval[j].value = &hint;
	propval[j].length = 1;
	j++;

#if 0
	/* ShutdownCommand: This command is executed at shutdown time to clean
	   up after a client that is no longer running but retained its state
	   by setting RestartStyleHint to RestartAnyway(1).  The command must
	   not remove any saved state as the client is still part of the
	   session. */
	prop[j].name = SmShutdownCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

	/* UserID: Specifies the user's ID.  On POSIX-based systems this will
	   contain the user's name (the pw_name field of struct passwd).  */
	errno = 0;
	prop[j].name = SmUserID;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	if ((pw = getpwuid(getuid())))
		strncpy(userID, pw->pw_name, sizeof(userID) - 1);
	else {
		EPRINTF("%s: %s\n", "getpwuid()", strerror(errno));
		snprintf(userID, sizeof(userID), "%ld", (long) getuid());
	}
	propval[j].value = userID;
	propval[j].length = strlen(userID);
	j++;

	SmcSetProperties(smcConn, j, props);

	free(cwd);
	free(pcln);
	free(prst);
	free(penv);
}

static Bool saving_yourself;
static Bool shutting_down;

static void
xdeSaveYourselfPhase2CB(SmcConn smcConn, SmPointer data)
{
	xdeSetProperties(smcConn, data);
	SmcSaveYourselfDone(smcConn, True);
}

/** @brief save yourself
  *
  * The session manager sends a "Save Yourself" message to a client either to
  * check-point it or just before termination so that it can save its state.
  * The client responds with zero or more calls to SmcSetProperties to update
  * the properties indicating how to restart the client.  When all the
  * properties have been set, the client calls SmcSaveYourselfDone.
  *
  * If interact_type is SmcInteractStyleNone, the client must not interact with
  * the user while saving state.  If interact_style is SmInteractStyleErrors,
  * the client may interact with the user only if an error condition arises.  If
  * interact_style is  SmInteractStyleAny then the client may interact with the
  * user for any purpose.  Because only one client can interact with the user at
  * a time, the client must call SmcInteractRequest and wait for an "Interact"
  * message from the session maanger.  When the client is done interacting with
  * the user, it calls SmcInteractDone.  The client may only call
  * SmcInteractRequest() after it receives a "Save Yourself" message and before
  * it calls SmcSaveYourSelfDone().
  */
static void
xdeSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown,
		     int interactStyle, Bool fast)
{
	if (!(shutting_down = shutdown)) {
		if (!SmcRequestSaveYourselfPhase2(smcConn, xdeSaveYourselfPhase2CB, data))
			SmcSaveYourselfDone(smcConn, False);
		return;
	}
	xdeSetProperties(smcConn, data);
	SmcSaveYourselfDone(smcConn, True);
}

/** @brief die
  *
  * The session manager sends a "Die" message to a client when it wants it to
  * die.  The client should respond by calling SmcCloseConnection.  A session
  * manager that behaves properly will send a "Save Yourself" message before the
  * "Die" message.
  */
static void
xdeDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	shutting_down = False;
	gtk_main_quit();
}

static void
xdeSaveCompleteCB(SmcConn smcConn, SmPointer data)
{
	if (saving_yourself) {
		saving_yourself = False;
		gtk_main_quit();
	}
}

/** @brief shutdown cancelled
  *
  * The session manager sends a "Shutdown Cancelled" message when the user
  * cancelled the shutdown during an interaction (see Section 5.5, "Interacting
  * With the User").  The client can now continue as if the shutdown had never
  * happended.  If the client has not called SmcSaveYourselfDone() yet, it can
  * either abort the save and then send SmcSaveYourselfDone() with the success
  * argument set to False or it can continue with the save and then call
  * SmcSaveYourselfDone() with the success argument set to reflect the outcome
  * of the save.
  */
static void
xdeShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	shutting_down = False;
	gtk_main_quit();
}

static unsigned long xdeCBMask =
    SmcSaveYourselfProcMask | SmcDieProcMask |
    SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

static SmcCallbacks xdeCBs = {
	.save_yourself = {
			  .callback = &xdeSaveYourselfCB,
			  .client_data = NULL,
			  },
	.die = {
		.callback = &xdeDieCB,
		.client_data = NULL,
		},
	.save_complete = {
			  .callback = &xdeSaveCompleteCB,
			  .client_data = NULL,
			  },
	.shutdown_cancelled = {
			       .callback = &xdeShutdownCancelledCB,
			       .client_data = NULL,
			       },
};

static gboolean
on_ifd_watch(GIOChannel *chan, GIOCondition cond, pointer data)
{
	SmcConn smcConn = (typeof(smcConn)) data;
	IceConn iceConn = SmcGetIceConnection(smcConn);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceProcessMessages(iceConn, NULL, NULL);
	}
	return G_SOURCE_CONTINUE;	/* keep event source */
}

static void
init_smclient(void)
{
	char err[256] = { 0, };
	GIOChannel *chan;
	int ifd, mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	char *env;
	IceConn iceConn;

	if (!(env = getenv("SESSSION_MANAGER"))) {
		if (options.clientId)
			EPRINTF("clientId provided but no SESSION_MANAGER\n");
		return;
	}

	smcConn = SmcOpenConnection(env, NULL, SmProtoMajor, SmProtoMinor,
				    xdeCBMask, &xdeCBs, options.clientId,
				    &options.clientId, sizeof(err), err);
	if (!smcConn) {
		EPRINTF("SmcOpenConnection: %s\n", err);
		return;
	}

	iceConn = SmcGetIceConnection(smcConn);

	ifd = IceConnectionNumber(iceConn);
	chan = g_io_channel_unix_new(ifd);
	g_io_add_watch(chan, mask, on_ifd_watch, smcConn);
}

static void
do_run(int argc, char *argv[])
{
	int i;

	/* initialize session managerment functions */
	init_smclient();

	startup(argc, argv);

	/* determine which functions are available */
	test_login_functions();
	test_lock_screen_program();
	test_power_functions();
	test_user_functions();
	test_session_functions();

	/* adjust the tooltips for the functions */
	if (options.debug) {
		for (i = 0; i < LOGOUT_ACTION_COUNT; i++) {
			switch (action_can[i]) {
			case AvailStatusUndef:
				button_tips[i] = g_strdup_printf("Function undefined.\n"
								 "Can value was %s",
								 "(undefined)");
				break;
			case AvailStatusUnknown:
				button_tips[i] = g_strdup_printf("Function unknown.\n"
								 "Can value was %s", "(unknown)");
				break;
			case AvailStatusNa:
				button_tips[i] = g_strdup_printf("Function not available.\n"
								 "Can value was %s", "na");
				break;
			case AvailStatusNo:
				button_tips[i] = g_strdup_printf("%s\n"
								 "Can value was %s",
								 button_tips[i], "no");
				break;
			case AvailStatusChallenge:
				button_tips[i] = g_strdup_printf("%s\n"
								 "Can value was %s",
								 button_tips[i], "challenge");
				break;
			case AvailStatusYes:
				button_tips[i] = g_strdup_printf("%s\n"
								 "Can value was %s",
								 button_tips[i], "yes");
				break;
			}
		}
	}
	top = GetWindow(False);
	gtk_main();
	if (logout_actions[action_result])
		(*logout_actions[action_result]) ();
	else {
		EPRINTF("No action for choice %d\n", action_result);
		exit(EXIT_FAILURE);
	}
}

static void
action_PowerOff(void)
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err))) {
		EPRINTF("cannot access the system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "PowerOff", &err,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to PowerOff failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Reboot(void)
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err))) {
		EPRINTF("cannot access the system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Reboot", &err,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Reboot failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Suspend(void)
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err))) {
		EPRINTF("cannot access the system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Suspend", &err,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Suspend failed %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Hibernate(void)
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err))) {
		EPRINTF("cannot access the system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Hibernate", &err,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Hibernate failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_HybridSleep(void)
{
	GError *err = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err))) {
		EPRINTF("cannot access the system bus: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "HybridSleep", &err,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to HybridSleep failed: %s\n", err ? err->message : NULL);
		g_clear_error(&err);
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_SwitchUser(void)
{
	/* do nothing: the switch has already been performed once we get here */
	return;
}

static void
action_SwitchDesk(void)
{
	/* not implemented yet */
	return;
}

static void
action_LockScreen(void)
{
}

static gboolean
session_timeout(gpointer user)
{
	gtk_main_quit();
	return G_SOURCE_REMOVE;
}

static void
action_Checkpoint(void)
{
	saving_yourself = True;
	shutting_down = False;
	SmcRequestSaveYourself(smcConn, SmSaveBoth, False, SmInteractStyleAny, False, True);
	g_timeout_add_seconds(5, session_timeout, NULL);
	gtk_main();
}

static void
action_Shutdown(void)
{
	saving_yourself = False;
	shutting_down = True;
	SmcRequestSaveYourself(smcConn, SmSaveLocal, True, SmInteractStyleErrors, True, True);
	g_timeout_add_seconds(5, session_timeout, NULL);
	gtk_main();
}

/** @brief perform the logout action
  *
  * Performs a complicated sequence of checks to log out of the current session.
  * This method supports more than just XDE sessions (L<lxsession(1)> and other
  * sessions are supported).
  *
  */
static void
action_Logout(void)
{
	const char *env;
	pid_t pid;

	/* Check for _LXSESSION_PID, _FBSESSION_PID, XDG_SESSION_PID.  When one 
	   of these exists, logging out of the session consists of sending a
	   TERM signal to the pid concerned.  When none of these exists, then
	   we can check to see if there is information on the root window. */
	if ((env = getenv("XDG_SESSION_PID")) && (pid = atoi(env))) {
		/* NOTE: we might actually be killing ourselves here...  */
		if (kill(pid, SIGTERM) == 0)
			return;
		fprintf(stderr, "kill: could not kill XDG_SESSION_PID %d: %s\n", (int) pid,
			strerror(errno));
	}
	if ((env = getenv("_FBSESSION_PID")) && (pid = atoi(env))) {
		/* NOTE: we might actually be killing ourselves here...  */
		if (kill(pid, SIGTERM) == 0)
			return;
		fprintf(stderr, "kill: could not kill _FBSESSION_PID %d: %s\n", (int) pid,
			strerror(errno));
	}
	if ((env = getenv("_LXSESSION_PID")) && (pid = atoi(env))) {
		/* NOTE: we might actually be killing ourselves here...  */
		if (kill(pid, SIGTERM) == 0)
			return;
		fprintf(stderr, "kill: could not kill _LXSESSION_PID %d: %s\n", (int) pid,
			strerror(errno));
	}

	GdkDisplayManager *mgr = gdk_display_manager_get();
	GdkDisplay *disp = gdk_display_manager_get_default_display(mgr);
	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn);

	/* When the _BLACKBOX_PID atom is set on the desktop, that is the PID
	   of the FLUXBOX window manager.  Actually it is me that is setting
	   _BLACKBOX_PID using the fluxbox init file rootCommand resource. */
	GdkAtom atom, actual;
	gint format, length;
	guchar *data;

	if ((atom = gdk_atom_intern("_BLACKBOX_PID", TRUE)) != GDK_NONE) {
		if (gdk_property_get
		    (root, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length, &data)
		    && format == 32 && length >= 1) {
			if ((pid = *(unsigned long *) data)) {
				/* NOTE: we might actually be killing ourselves 
				   here...  */
				if (kill(pid, SIGTERM) == 0)
					return;
				fprintf(stderr, "kill: could not kill _BLACKBOX_PID %d: %s\n",
					(int) pid, strerror(errno));
			}
		}

	}
	/* Openbox sets _OPENBOX_PID atom on the desktop.  It also sets
	   _OB_THEME to the theme name, _OB_CONFIG_FILE to the configuration
	   file in use, and _OB_VERSION to the version of openbox.
	   __NET_SUPPORTING_WM_CHECK is set to the WM window, which has
	   _NET_WM_NAME set to "Openbox". */
	if ((atom = gdk_atom_intern("_OPENBOX_PID", TRUE)) != GDK_NONE) {
		if (gdk_property_get
		    (root, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length, &data)
		    && format == 32 && length >= 1) {
			if ((pid = *(unsigned long *) data)) {
				/* NOTE: we might actually be killing ourselves 
				   here...  */
				if (kill(pid, SIGTERM) == 0)
					return;
				fprintf(stderr, "kill: could not kill _OPENBOX_PID %d: %s\n",
					(int) pid, strerror(errno));
			}
		}
	}
	/* IceWM-Session does not set environment variables nor elements on the 
	   root. _NET_SUPPORTING_WM_CHECK is set to the WM window, which has a
	   _NET_WM_NAME set to "IceWM 1.3.7 (Linux 3.4.0-ARCH/x86_64)" but also 
	   has _NET_WM_PID set to the pid of "icewm". Note that this is not the 
	   pid of icewm-session when that is running. */
	if ((atom = gdk_atom_intern("_NET_SUPPORTING_WM_CHECK", TRUE)) != GDK_NONE) {
		if (gdk_property_get
		    (root, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length, &data)
		    && format == 32 && length >= 1) {
			Window xid = *(unsigned long *) data;
			GdkWindow *win = gdk_window_foreign_new(xid);

			if ((atom = gdk_atom_intern("_NET_WM_PID", TRUE)) != GDK_NONE) {
				if (gdk_property_get
				    (win, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length,
				     &data) && format == 32 && length >= 1) {
					if ((pid = *(unsigned long *) data)) {
						/* NOTE: we might actually be
						   killing ourselves here...  */
						if (kill(pid, SIGTERM) == 0) {
							g_object_unref(G_OBJECT(win));
							return;
						}
						fprintf(stderr,
							"kill: could not kill _NET_WM_PID %d: %s\n",
							(int) pid, strerror(errno));
					}
				}
			}
			g_object_unref(G_OBJECT(win));
		}
	}
	/* Some window managers set _NET_WM_PID on the root window... */
	if ((atom = gdk_atom_intern("_NET_WM_PID", TRUE)) != GDK_NONE) {
		if (gdk_property_get
		    (root, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length, &data)
		    && format == 32 && length >= 1) {
			if ((pid = *(unsigned long *) data)) {
				/* NOTE: we might actually be killing ourselves 
				   here... */
				if (kill(pid, SIGTERM) == 0)
					return;
				fprintf(stderr, "kill: could not kill _NET_WM_PID %d: %s\n",
					(int) pid, strerror(errno));
			}
		}
	}
	/* We set _XDE_WM_PID on the root window when we were invoked properly
	   to the window manager PID.  Try that next. */
	if ((atom = gdk_atom_intern("_XDE_WM_PID", TRUE)) != GDK_NONE) {
		if (gdk_property_get
		    (root, atom, GDK_NONE, 0, 1, FALSE, &actual, &format, &length, &data)
		    && format == 32 && length >= 1) {
			if ((pid = *(unsigned long *) data)) {
				/* NOTE: we might actually be killing ourselves 
				   here... */
				if (kill(pid, SIGTERM) == 0)
					return;
				fprintf(stderr, "kill: could not kill _XDE_WM_PID %d: %s\n",
					(int) pid, strerror(errno));
			}
		}
	}
	/* FIXME: we can do much better than this.  See libxde.  In fact, maybe 
	   we should use libxde. */
	fprintf(stderr, "ERROR: cannot find session or window manager PID!\n");
	return;
}

static void
action_Restart(void)
{
	/* not implemented yet */
	return;
}

static void
action_Cancel(void)
{
	/* do nothing for now */
	return;
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
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

const char *
show_side(LogoSide side)
{
	switch (side) {
	case LogoSideLeft:
		return ("left");
	case LogoSideTop:
		return ("top");
	case LogoSideRight:
		return ("right");
	case LogoSideBottom:
		return ("bottom");
	}
	return ("unknown");
}

const char *
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
    %1$s [options]\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -p, --prompt PROMPT        (%2$s)\n\
        specifies a custom prompt message\n\
    -b, --banner BANNER        (%3$s)\n\
        specify custom login branding\n\
    -s, --side {l|t|r|b}       (%4$s)\n\
        specify side  of dialog for logo placement\n\
    -n, --noask                (%5$s)\n\
        do not ask what to do, just logout\n\
    -i, --icons THEME          (%6$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME          (%7$s)\n\
        set the gtk+ theme to use\n\
    -x, --xde-theme            (%8$s)\n\
        use the XDE desktop theme for the selection window\n\
    -T, --timeout SECONDS      (%9$u sec)\n\
        set dialog timeout\n\
    -N, --dry-run              (%10$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]        (%11$d)\n\
        increment or set debug LEVEL\n\
        this option may be repeated.\n\
    -v, --verbose [LEVEL]      (%12$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
", argv[0], options.welcome, options.banner, show_side(options.side), show_bool(options.noask), options.usexde ? "xde" : (options.icon_theme ? : "auto"), options.usexde ? "xde" : (options.gtk2_theme ? : "auto"), show_bool(options.usexde), options.timeout, show_bool(options.dryrun), options.debug, options.output);
}

const char *
get_nc_resource(XrmDatabase xrdb, const char *res_name, const char *res_class,
		const char *resource)
{
	char *type;
	static char name[64];
	static char clas[64];
	XrmValue value = { 0, NULL };

	snprintf(name, sizeof(name), "%s.%s", res_name, resource);
	snprintf(clas, sizeof(clas), "%s.%s", res_class, resource);
	if (XrmGetResource(xrdb, name, clas, &type, &value)) {
		if (value.addr && *(char *) value.addr) {
			DPRINTF("%s:\t\t%s\n", clas, value.addr);
			return (const char *) value.addr;
		} else
			DPRINTF("%s:\t\t%s\n", clas, value.addr);
	} else
		DPRINTF("%s:\t\t%s\n", clas, "ERROR!");
	return (NULL);
}

const char *
get_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, RESNAME, RESCLAS, resource)))
		value = dflt;
	return (value);
}

const char *
get_xlogin_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, "xlogin.Login", "Xlogin.Login", resource)))
		value = dflt;
	return (value);
}

const char *
get_any_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_resource(xrdb, resource, NULL)))
		if (!(value = get_xlogin_resource(xrdb, resource, NULL)))
			value = dflt;
	return (value);
}

const char *
get_chooser_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, "chooser", "Chooser", resource)))
		value = dflt;
	return (value);
}

gboolean
getXrmColor(const char *val, GdkColor **color)
{
	GdkColor c, *p;

	if (gdk_color_parse(val, &c) && (p = calloc(1, sizeof(*p)))) {
		*p = c;
		free(*color);
		*color = p;
		return TRUE;
	}
	EPRINTF("could not parse color '%s'\n", val);
	return FALSE;
}

gboolean
getXrmFont(const char *val, PangoFontDescription **face)
{
	FcPattern *pattern;
	PangoFontDescription *font;

	if ((pattern = FcNameParse((FcChar8 *)val))) {
		if ((font = pango_fc_font_description_from_pattern(pattern, TRUE))) {
			pango_font_description_free(*face);
			*face = font;
			DPRINTF("Font description is: %s\n",
					pango_font_description_to_string(font));
			return TRUE;
		}
		FcPatternDestroy(pattern);
	}
	EPRINTF("could not parse font descriptikon '%s'\n", val);
	return FALSE;
}

gboolean
getXrmInt(const char *val, int *integer)
{
	*integer = strtol(val, NULL, 0);
	return TRUE;
}

gboolean
getXrmUint(const char *val, unsigned int *integer)
{
	*integer = strtoul(val, NULL, 0);
	return TRUE;
}

gboolean
getXrmDouble(const char *val, double *floating)
{
	const struct lconv *lc = localeconv();
	char radix, *copy = strdup(val);

	if ((radix = lc->decimal_point[0]) != '.' && strchr(copy, '.'))
		*strchr(copy, '.') = radix;

	*floating = strtod(copy, NULL);
	DPRINTF("Got decimal value %s, translates to %f\n", val, *floating);
	free(copy);
	return TRUE;
}

gboolean
getXrmBool(const char *val, Bool *boolean)
{
	if (!strncasecmp(val, "true", strlen(val))) {
		*boolean = True;
		return TRUE;
	}
	if (!strncasecmp(val, "false", strlen(val))) {
		*boolean = False;
		return TRUE;
	}
	EPRINTF("could not parse boolean'%s'\n", val);
	return FALSE;
}

gboolean
getXrmString(const char *val, char **string)
{
	char *tmp;

	if ((tmp = strdup(val))) {
		free(*string);
		*string = tmp;
		return TRUE;
	}
	return FALSE;
}

void
get_resources(int argc, char *argv[])
{
	Display *dpy;
	XrmDatabase rdb;
	const char *val;
	XTextProperty xtp;
	Window root;
	Atom atom;

	DPRINT();
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("could not open display %s\n", getenv("DISPLAY"));
		exit(EXIT_FAILURE);
	}
	root = DefaultRootWindow(dpy);
	if (!(atom = XInternAtom(dpy, "RESOURCE_MANAGER", True))) {
		XCloseDisplay(dpy);
		DPRINTF("no resource manager database allocated\n");
		return;
	}
	if (!XGetTextProperty(dpy, root, &xtp, atom) || !xtp.value) {
		XCloseDisplay(dpy);
		EPRINTF("could not retrieve RESOURCE_MANAGER property\n");
		return;
	}
	XrmInitialize();
	// DPRINTF("RESOURCE_MANAGER = %s\n", xtp.value);
	rdb = XrmGetStringDatabase((char *) xtp.value);
	XFree(xtp.value);
	if (!rdb) {
		DPRINTF("no resource manager database allocated\n");
		XCloseDisplay(dpy);
		return;
	}
	if ((val = get_resource(rdb, "debug", "0"))) {
		getXrmInt(val, &options.debug);
	}
	if ((val = get_xlogin_resource(rdb, "width", NULL))) {
		if (strchr(val, '%')) {
			char *endptr = NULL;
			double width = strtod(val, &endptr);

			if (endptr != val && *endptr == '%' && width > 0) {
				options.width =
				    (int) ((width / 100.0) * DisplayWidth(dpy, 0));
				if (options.width < 0.20 * DisplayWidth(dpy, 0))
					options.width = -1;
			}
		} else {
			options.width = strtoul(val, NULL, 0);
			if (options.width <= 0)
				options.width = -1;
		}
	}
	if ((val = get_xlogin_resource(rdb, "height", NULL))) {
		if (strchr(val, '%')) {
			char *endptr = NULL;
			double height = strtod(val, &endptr);

			if (endptr != val && *endptr == '%' && height > 0) {
				options.height =
				    (int) ((height / 100.0) * DisplayHeight(dpy, 0));
				if (options.height < 0.20 * DisplayHeight(dpy, 0))
					options.height = -1;
			}
		} else {
			options.height = strtoul(val, NULL, 0);
			if (options.height <= 0)
				options.height = -1;
		}
	}
	if ((val = get_xlogin_resource(rdb, "x", NULL))) {
		options.xposition =
		    (double) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
		if (options.xposition < 0)
			options.xposition = 0;
		if (options.xposition > DisplayWidth(dpy, 0))
			options.xposition = 1.0;
	}
	if ((val = get_xlogin_resource(rdb, "y", NULL))) {
		options.yposition =
		    (double) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
		if (options.yposition < 0)
			options.yposition = 0;
		if (options.yposition > DisplayWidth(dpy, 0))
			options.yposition = 1.0;
	}
	// xlogin.foreground:		grey20
	if ((val = get_xlogin_resource(rdb, "foreground", NULL))) {
		getXrmColor(val, &resources.foreground);
	}
	// xlogin.background:		LightSteelBlue3
	if ((val = get_xlogin_resource(rdb, "background", NULL))) {
		getXrmColor(val, &resources.background);
	}
	// xlogin.face:			Sans-12:bold
	// xlogin.font:
	if ((val = get_any_resource(rdb, "face", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.face);
	}
#if 0
	// xlogin.greeting:		Welcome to CLIENTHOST
	if ((val = get_xlogin_resource(rdb, "greeting", NULL))) {
		getXrmString(val, &resources.greeting);
		getXrmString(val, &options.welcome);
	}
#endif
	// xlogin.unsecureGreeting:	This is an unsecure session
	if ((val = get_xlogin_resource(rdb, "unsecureGreeting", NULL))) {
		getXrmString(val, &resources.unsecureGreeting);
	}
	// xlogin.greetFace:		Sans-12:bold
	// xlogin.greetFont:
	if ((val = get_any_resource(rdb, "greetFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.greetFace);
	}
	// xlogin.greetColor:		grey20
	if ((val = get_any_resource(rdb, "greetColor", "grey20"))) {
		getXrmColor(val, &resources.greetColor);
	}
	// xlogin.namePrompt:		Username:
	if ((val = get_xlogin_resource(rdb, "namePrompt", "Username: "))) {
		getXrmString(val, &resources.namePrompt);
	}
	// xlogin.passwdPrompt:		Password:
	if ((val = get_xlogin_resource(rdb, "passwdPrompt", "Password: "))) {
		getXrmString(val, &resources.passwdPrompt);
	}
	// xlogin.promptFace:		Sans-12:bold
	// xlogin.promptFont:
	if ((val = get_any_resource(rdb, "promptFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.promptFace);
	}
	// xlogin.promptColor:		grey20
	if ((val = get_any_resource(rdb, "promptColor", "grey20"))) {
		getXrmColor(val, &resources.promptColor);
	}
	if ((val = get_any_resource(rdb, "inputFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.inputFace);
	}
	if ((val = get_any_resource(rdb, "inputColor", "grey20"))) {
		getXrmColor(val, &resources.inputColor);
	}
	// xlogin.changePasswdMessage:	Password Change Required
	if ((val = get_xlogin_resource(rdb, "changePasswdMessage", "Password Change Required"))) {
		getXrmString(val, &resources.changePasswdMessage);
	}
	// xlogin.fail:			Login incorrect!
	if ((val = get_xlogin_resource(rdb, "fail", "Login incorrect!"))) {
		getXrmString(val, &resources.fail);
	}
	// xlogin.failFace:		Sans-12:bold
	// xlogin.failFont:
	if ((val = get_any_resource(rdb, "failFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.failFace);
	}
	// xlogin.failColor:		red
	if ((val = get_any_resource(rdb, "failColor", "red"))) {
		getXrmColor(val, &resources.failColor);
	}
	// xlogin.failTimeout:		10
	if ((val = get_xlogin_resource(rdb, "failTimeout", "10"))) {
		getXrmUint(val, &resources.failTimeout);
	}
	// xlogin.logoFileName:		/etc/X11/xdm/xde/banner.png
	if ((val = get_xlogin_resource(rdb, "logoFileName", "/etc/X11/xdm/xde/banner.png"))) {
		getXrmString(val, &resources.logoFileName);
	}
	// xlogin.logoPadding:		8
	if ((val = get_xlogin_resource(rdb, "logoPadding", "8"))) {
		getXrmUint(val, &resources.logoPadding);
	}
	// xlogin.useShape:		true
	if ((val = get_xlogin_resource(rdb, "useShape", "true"))) {
		getXrmBool(val, &resources.useShape);
	}
	// xlogin.hiColor:		grey80
	if ((val = get_xlogin_resource(rdb, "hiColor", "grey80"))) {
		getXrmColor(val, &resources.hiColor);
	}
	// xlogin.shdColor:		grey20
	if ((val = get_xlogin_resource(rdb, "shdColor", "grey20"))) {
		getXrmColor(val, &resources.shdColor);
	}
	// xlogin.frameWidth:		2
	if ((val = get_xlogin_resource(rdb, "frameWidth", "2"))) {
		getXrmUint(val, &resources.frameWidth);
	}
	// xlogin.innerFrameWidth:	2
	if ((val = get_xlogin_resource(rdb, "innerFrameWidth", "2"))) {
		getXrmUint(val, &resources.innerFrameWidth);
	}
	// xlogin.sepWidth:		2
	if ((val = get_xlogin_resource(rdb, "sepWidth", "2"))) {
		getXrmUint(val, &resources.sepWidth);
	}
	// xlogin.allowRootLogin:	true
	if ((val = get_xlogin_resource(rdb, "allowRootLogin", "true"))) {
		getXrmBool(val, &resources.allowRootLogin);
	}
	// xlogin.allowNullPasswd:	false
	if ((val = get_xlogin_resource(rdb, "allowNullPasswd", "false"))) {
		getXrmBool(val, &resources.allowNullPasswd);
	}
	// xlogin.echoPasswd:		true
	if ((val = get_xlogin_resource(rdb, "echoPasswd", "true"))) {
		getXrmBool(val, &resources.echoPasswd);
	}
	// xlogin.echoPasswdChar:	*
	if ((val = get_xlogin_resource(rdb, "echoPasswdChar", "*"))) {
		getXrmString(val, &resources.echoPasswdChar);
	}
	// xlogin.borderWidth:		3
	if ((val = get_xlogin_resource(rdb, "borderWidth", "3"))) {
		getXrmUint(val, &resources.borderWidth);
	}

	// xlogin.login.translations

	// Chooser.geometry:		700x500
	// Chooser.allowShellResize:	false
	// Chooser.viewport.forceBars:	true
	// Chooser.label.font:		*-new-century-schoolbook-bold-i-normal-*-240-*
	// Chooser.label.label:		XDMCP Host Menu from CLIENTHOST
	// Chooser.list.font:		*-*-medium-r-normal-*-*-230-*-*-c-*-iso8859-1
	// Chooser.command.font:	*-new-century-schoolbook-bold-r-normal-*-180-*

	if ((val = get_resource(rdb, "Chooser.x", NULL))) {
		getXrmDouble(val, &options.xposition);
	}
	if ((val = get_resource(rdb, "Chooser.y", NULL))) {
		getXrmDouble(val, &options.yposition);
	}
	if ((val = get_resource(rdb, "banner", NULL))) {
		getXrmString(val, &options.banner);
	}
	if ((val = get_resource(rdb, "splash", NULL))) {
		getXrmString(val, &options.splash);
	}
#if 0
	if ((val = get_resource(rdb, "welcome", NULL))) {
		getXrmString(val, &options.welcome);
	}
#endif
	if ((val = get_resource(rdb, "charset", NULL))) {
		getXrmString(val, &options.charset);
	}
	if ((val = get_resource(rdb, "language", NULL))) {
		getXrmString(val, &options.language);
	}
	if ((val = get_resource(rdb, "theme.icon", NULL))) {
		getXrmString(val, &options.icon_theme);
	}
	if ((val = get_resource(rdb, "theme.name", NULL))) {
		getXrmString(val, &options.gtk2_theme);
	}
	if ((val = get_resource(rdb, "theme.cursor", NULL))) {
		getXrmString(val, &options.curs_theme);
	}
	if ((val = get_resource(rdb, "theme.xde", NULL))) {
		getXrmBool(val, &options.usexde);
	}
	if ((val = get_resource(rdb, "side", NULL))) {
		if (!strncasecmp(val, "left", strlen(val)))
			options.side = LogoSideLeft;
		else if (!strncasecmp(val, "top", strlen(val)))
			options.side = LogoSideTop;
		else if (!strncasecmp(val, "right", strlen(val)))
			options.side = LogoSideRight;
		else if (!strncasecmp(val, "bottom", strlen(val)))
			options.side = LogoSideBottom;
		else
			EPRINTF("invalid value for XDE-XChooser*side: %s\n", val);
	}
	if (getuid() == 0) {
		if ((val = get_resource(rdb, "user.default", NULL))) {
			getXrmString(val, &options.username);
		}
		if ((val = get_resource(rdb, "autologin", NULL))) {
			// getXrmBool(val, &options.autologin);
		}
	}
	if ((val = get_resource(rdb, "vendor", NULL))) {
		getXrmString(val, &options.vendor);
	}
	if ((val = get_resource(rdb, "prefix", NULL))) {
		getXrmString(val, &options.prefix);
	}
	if ((val = get_resource(rdb, "login.permit", NULL))) {
		// getXrmBool(val, &options.permitlogin);
	}
	if ((val = get_resource(rdb, "login.remote", NULL))) {
		// getXrmBool(val, &options.remotelogin);
	}
	if ((val = get_resource(rdb, "xsession.chooser", NULL))) {
		getXrmBool(val, &options.xsession);
	}
	if ((val = get_resource(rdb, "xsession.execute", NULL))) {
		// getXrmBool(val, &options.execute);
	}
	if ((val = get_resource(rdb, "xsession.default", NULL))) {
		getXrmString(val, &options.choice);
	}
	if ((val = get_resource(rdb, "setbg", NULL))) {
		getXrmBool(val, &options.setbg);
	}
	if ((val = get_resource(rdb, "transparent", NULL))) {
		getXrmBool(val, &options.transparent);
	}
	XrmDestroyDatabase(rdb);
	XCloseDisplay(dpy);
}

void
set_default_vendor(void)
{
	char *p, *vendor, *prefix;
	int len;

	if ((vendor = getenv("XDG_VENDOR_ID"))) {
		free(defaults.vendor);
		defaults.vendor = strdup(vendor);
	}
	if ((prefix = getenv("XDG_MENU_PREFIX"))) {
		free(defaults.prefix);
		defaults.prefix = strdup(prefix);
		if (!vendor) {
			free(defaults.vendor);
			vendor = defaults.vendor = strdup(prefix);
			if ((p = strrchr(vendor, '-')) && !*(p + 1))
				*p = '\0';
		}
	} else if (vendor && *vendor) {
		free(defaults.prefix);
		len = strlen(vendor) + 1;
		prefix = defaults.prefix = calloc(len + 1, sizeof(*prefix));
		strncpy(prefix, vendor, len);
		strncat(prefix, "-", len);
	}
	if (!defaults.vendor)
		defaults.vendor = strdup("");
	if (!defaults.prefix)
		defaults.prefix = strdup("");
}

void
set_default_xdgdirs(int argc, char *argv[])
{
	static const char *confdir = "/etc/xdg/xde:/etc/xdg";
	static const char *datadir = "/usr/share/xde:/usr/local/share:/usr/share";
	char *here, *p, *q;
	char *conf, *data;
	int len;

	here = strdup(argv[0]);
	if (here[0] != '/') {
		char *cwd = calloc(PATH_MAX + 1, sizeof(*cwd));

		if (!getcwd(cwd, PATH_MAX)) {
			EPRINTF("%s: %s\n", "getcwd", strerror(errno));
			exit(EXIT_FAILURE);
		}
		strncat(cwd, "/", PATH_MAX);
		strncat(cwd, here, PATH_MAX);
		free(here);
		here = strdup(cwd);
		free(cwd);
	}
	while ((p = strstr(here, "/./")))
		memmove(p, p + 2, strlen(p + 2) + 1);
	while ((p = strstr(here, "/../"))) {
		for (q = p - 1; q > here && *q != '/'; q--) ;
		if (q > here || *q != '/')
			break;
		memmove(q, p + 3, strlen(p + 3) + 1);
	}
	if ((p = strrchr(here, '/')))
		*p = '\0';
	if ((p = strstr(here, "/src")) && !*(p + 4))
		*p = '\0';
	/* executed in place */
	if (strchr(here, '/') && strcmp(here, "/usr/bin")) {
		len = strlen(here) + strlen("/data/xdg/xde:")
		    + strlen(here) + strlen("/data/xdg:") + strlen(confdir);
		conf = calloc(len + 1, sizeof(*conf));
		strncpy(conf, here, len);
		strncat(conf, "/data/xdg/xde:", len);
		strncat(conf, here, len);
		strncat(conf, "/data/xdg:", len);
		strncat(conf, confdir, len);

		len = strlen(here) + strlen("/data/share/xde:")
		    + strlen(here) + strlen("/data/share:") + strlen(datadir);
		data = calloc(len + 1, sizeof(*data));
		strncpy(data, here, len);
		strncat(data, "/data/share/xde:", len);
		strncat(data, here, len);
		strncat(data, "/data/share:", len);
		strncat(data, datadir, len);
	} else {
		conf = strdup(confdir);
		data = strdup(datadir);
	}
	setenv("XDG_CONFIG_DIRS", conf, 1);
	setenv("XDG_DATA_DIRS", data, 1);
	DPRINTF("setting XDG_CONFIG_DIRS to '%s'\n", conf);
	DPRINTF("setting XDG_DATA_DIRS   to '%s'\n", data);
	free(conf);
	free(data);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	free(defaults.banner);
	defaults.banner = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		defaults.banner = NULL;
		return;
	}

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = defaults.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "banner", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					defaults.banner = strdup(file);
					break;
				}
			}
			if (defaults.banner)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
set_default_splash(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	free(defaults.splash);
	defaults.splash = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		defaults.splash = NULL;
		return;
	}

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = defaults.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "splash", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					defaults.splash = strdup(file);
					break;
				}
			}
			if (defaults.splash)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
set_default_welcome(void)
{
	char *session = NULL, *welcome, *p;
	const char *s;
	int i, len;

	welcome = calloc(PATH_MAX, sizeof(*welcome));

	if ((s = getenv("XDG_CURRENT_DESKTOP")) && *s) {
		session = strdup(s);
		while ((p = strchr(session, ';')))
			*p = ':';
	} else if ((s = defaults.vendor) && *s) {
		session = strdup(s);
	} else if ((s = defaults.prefix) && *s) {
		session = strdup(s);
		p = session + strlen(session) - 1;
		if (*p == '-')
			*p = '\0';
	} else {
		session = strdup("XDE");
	}
	len = strlen(session);
	for (i = 0, p = session; i < len; i++, p++)
		*p = toupper(*p);
	snprintf(welcome, PATH_MAX - 1, "Logout of <b>%s</b> session?", session);
	defaults.welcome = strdup(welcome);
	free(session);
	free(welcome);
}

void
set_default_language(void)
{
	char *p, *a;

	if ((defaults.language = setlocale(LC_ALL, ""))) {
		defaults.language = strdup(defaults.language);
		a = strchrnul(defaults.language, '@');
		if ((p = strchr(defaults.language, '.')))
			strcpy(p, a);
	}
	defaults.charset = strdup(nl_langinfo(CODESET));
}

#ifdef DO_XCHOOSER
void
set_default_address(void)
{
	XdmcpReallocARRAY8(&defaults.clientAddress, sizeof(struct in6_addr));
	*(struct in6_addr *) defaults.clientAddress.data = (struct in6_addr) IN6ADDR_LOOPBACK_INIT;
	defaults.connectionType = FamilyInternet6;
	defaults.clientScope = SocketScopeLoopback;
	defaults.isLocal = True;
}
#endif				/* DO_XCHOOSER */

#if 0
void
set_default_session(void)
{
	char **xdg_dirs, **dirs, *file, *line, *p;
	int i, n = 0;
	static const char *session = "/xde/default";
	static const char *current = "/xde/current";
	static const char *dmrc = "/.dmrc";
	const char *home = getenv("HOME") ? : ".";

	free(defaults.session);
	defaults.session = NULL;
	free(defaults.current);
	defaults.current = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	strncpy(file, home, PATH_MAX);
	strncat(file, dmrc, PATH_MAX);

	if (!defaults.dmrc)
		defaults.dmrc = g_key_file_new();
	if (defaults.dmrc) {
		if (g_key_file_load_from_file(defaults.dmrc, file,
					      G_KEY_FILE_KEEP_COMMENTS |
					      G_KEY_FILE_KEEP_TRANSLATIONS, NULL)) {
			gchar *sess;

			if ((sess = g_key_file_get_string(defaults.dmrc,
							  "Desktop", "Session", NULL))) {
				free(defaults.session);
				defaults.session = strdup(sess);
				free(defaults.current);
				defaults.current = strdup(sess);
				g_free(sess);
				free(file);
				return;
			}
		}
	}

	if (!(xdg_dirs = get_config_dirs(&n)) || !n) {
		free(file);
		return;
	}

	line = calloc(BUFSIZ + 1, sizeof(*line));

	/* go through them forward */
	for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
		FILE *f;

		if (!defaults.session) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, session, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						defaults.session = strdup(line);
					}
					fclose(f);
				}
			}

		}
		if (!defaults.current) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, current, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						defaults.current = strdup(line);
					}
					fclose(f);
				}
			}
		}
	}
	free(line);
	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}
#endif

void
set_default_choice(void)
{
	free(defaults.choice);
	defaults.choice = strdup("default");
}

void
set_defaults(int argc, char *argv[])
{
	char *p;

	if ((p = getenv("XDE_DEBUG")))
		options.debug = atoi(p);

	set_default_vendor();
	set_default_xdgdirs(argc, argv);
	set_default_banner();
	set_default_splash();
	set_default_welcome();
	set_default_language();
#ifdef DO_XCHOOSER
	set_default_address();
#endif				/* DO_XCHOOSER */
#if 0
	set_default_session();
#endif
	set_default_choice();
}

void
get_default_vendor(void)
{
	if (!options.vendor) {
		options.vendor = defaults.vendor;
		options.prefix = defaults.prefix;
	} else if (*options.vendor) {
		int len = strlen(options.vendor) + 1;

		free(options.prefix);
		options.prefix = calloc(len + 1, sizeof(*options.prefix));
		strncpy(options.prefix, options.vendor, len);
		strncat(options.prefix, "-", len);
	} else {
		free(options.prefix);
		options.prefix = strdup("");
	}
	if (options.vendor && *options.vendor)
		setenv("XDG_VENDOR_ID", options.vendor, 1);
	else
		unsetenv("XDG_VENDOR_ID");
	if (options.prefix && *options.prefix)
		setenv("XDG_MENU_PREFIX", options.prefix, 1);
	else
		unsetenv("XDG_MENU_PREFIX");
}

void
get_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (options.banner)
		return;

	free(options.banner);
	options.banner = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		options.banner = defaults.banner;
		return;
	}

	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = options.prefix)) {
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
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);

	if (!options.banner)
		options.banner = defaults.banner;
}

void
get_default_splash(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (options.splash)
		return;

	free(options.splash);
	options.splash = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		options.splash = defaults.splash;
		return;
	}

	options.splash = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = options.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "splash", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					options.splash = strdup(file);
					break;
				}
			}
			if (options.splash)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);

	if (!options.splash)
		options.splash = defaults.splash;
}

void
get_default_welcome(void)
{
	if (!options.welcome) {
		free(options.welcome);
		options.welcome = defaults.welcome;
	}
}

void
get_default_language(void)
{
	if (!options.charset) {
		free(options.charset);
		options.charset = defaults.charset;
	}
	if (!options.language) {
		free(options.language);
		options.language = defaults.language;
	}
	if (strcmp(options.charset, defaults.charset) ||
	    strcmp(options.language, defaults.language)) {
		/* FIXME: actually set the language and charset */
	}
}

#ifdef DO_XCHOOSER
SocketScope
GetScope(ARRAY8Ptr clientAddress, CARD16 connectionType)
{
	switch (connectionType) {
	case FamilyLocal:
		break;
	case FamilyInternet:
	{
		in_addr_t addr = ntohl(*(in_addr_t *) clientAddress->data);

		if (IN_LOOPBACK(addr))
			return SocketScopeLoopback;
		if (IN_LINKLOCAL(addr))
			return SocketScopeLinklocal;
		if (IN_ORGLOCAL(addr))
			return SocketScopePrivate;
		return SocketScopeGlobal;
	}
	case FamilyInternet6:
	{
		struct in6_addr *addr = (typeof(addr)) clientAddress->data;

		if (IN6_IS_ADDR_LOOPBACK(addr))
			return SocketScopeLoopback;
		if (IN6_IS_ADDR_LINKLOCAL(addr))
			return SocketScopeLinklocal;
		if (IN6_IS_ADDR_SITELOCAL(addr))
			return SocketScopeSitelocal;
		if (IN6_IS_ADDR_V4MAPPED(addr) || IN6_IS_ADDR_V4COMPAT(addr)) {
			in_addr_t ipv4 = ntohl(((uint32_t *) addr)[3]);

			if (IN_LOOPBACK(ipv4))
				return SocketScopeLoopback;
			if (IN_LINKLOCAL(ipv4))
				return SocketScopeLinklocal;
			if (IN_ORGLOCAL(ipv4))
				return SocketScopePrivate;
			return SocketScopeGlobal;
		}
		return SocketScopeGlobal;
	}
	default:
		break;
	}
	return SocketScopeLoopback;
}

Bool
TestLocal(ARRAY8Ptr clientAddress, CARD16 connectionType)
{
	sa_family_t family;
	struct ifaddrs *ifa, *ifas = NULL;

	switch (connectionType) {
	case FamilyLocal:
		family = AF_UNIX;
		return True;
	case FamilyInternet:
		if (ntohl((*(in_addr_t *) clientAddress->data)) == INADDR_LOOPBACK)
			return True;
		family = AF_INET;
		break;
	case FamilyInternet6:
		if (IN6_IS_ADDR_LOOPBACK(clientAddress->data))
			return True;
		family = AF_INET6;
		break;
	default:
		family = AF_UNSPEC;
		return False;
	}
	if (getifaddrs(&ifas) == 0) {
		for (ifa = ifas; ifa; ifa = ifa->ifa_next) {
			struct sockaddr *ifa_addr;

			if (!(ifa_addr = ifa->ifa_addr)) {
				EPRINTF("interface %s has no address\n", ifa->ifa_name);
				continue;
			}
			if (ifa_addr->sa_family != family) {
				DPRINTF("interface %s has wrong family\n", ifa->ifa_name);
				continue;
			}
			switch (family) {
			case AF_INET:
			{
				struct sockaddr_in *sin = (typeof(sin)) ifa_addr;

				if (!memcmp(&sin->sin_addr, clientAddress->data, 4)) {
					DPRINTF("interface %s matches\n", ifa->ifa_name);
					freeifaddrs(ifas);
					return True;
				}

				break;
			}
			case AF_INET6:
			{
				struct sockaddr_in6 *sin6 = (typeof(sin6)) ifa_addr;

				if (!memcmp(&sin6->sin6_addr, clientAddress->data, 16)) {
					DPRINTF("interface %s matches\n", ifa->ifa_name);
					freeifaddrs(ifas);
					return True;
				}
				break;
			}
			}
		}
		freeifaddrs(ifas);
	}
	return False;
}

void
get_default_address(void)
{
	switch (options.clientAddress.length) {
	case 0:
		options.clientAddress = defaults.clientAddress;
		options.connectionType = defaults.connectionType;
		options.clientScope = defaults.clientScope;
		options.clientIface = defaults.clientIface;
		options.isLocal = defaults.isLocal;
		break;
	case 4:
	case 8:
		if (options.connectionType != FamilyInternet) {
			EPRINTF("Mismatch in connectionType %d != %d\n",
				FamilyInternet, options.connectionType);
			exit(EXIT_SYNTAXERR);
		}
		options.clientScope = GetScope(&options.clientAddress, options.connectionType);
		options.isLocal = TestLocal(&options.clientAddress, options.connectionType);
		switch (options.clientAddress.length) {
		case 4:
			options.clientIface = defaults.clientIface;
			break;
		case 8:
			memmove(&options.clientIface, options.clientAddress.data + 4, 4);
			break;
		}
		switch (options.clientScope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			if (!options.clientIface) {
				EPRINTF("link or site local address with no interface\n");
				exit(EXIT_SYNTAXERR);
			}
			break;
		default:
			break;
		}
		break;
	case 16:
	case 20:
		if (options.connectionType != FamilyInternet6) {
			EPRINTF("Mismatch in connectionType %d != %d\n",
				FamilyInternet, options.connectionType);
			exit(EXIT_SYNTAXERR);
		}
		options.clientScope = GetScope(&options.clientAddress, options.connectionType);
		options.isLocal = TestLocal(&options.clientAddress, options.connectionType);
		switch (options.clientAddress.length) {
		case 16:
			options.clientIface = defaults.clientIface;
			break;
		case 20:
			memmove(&options.clientIface, options.clientAddress.data + 16, 4);
			break;
		}
		switch (options.clientScope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			if (!options.clientIface) {
				EPRINTF("link or site local address with no interface\n");
				exit(EXIT_SYNTAXERR);
			}
			break;
		default:
			break;
		}
		break;
	default:
		EPRINTF("Invalid client address length %d\n", options.clientAddress.length);
		exit(EXIT_SYNTAXERR);
	}
}
#endif				/* DO_XCHOOSER */

void
get_default_session(void)
{
	if (options.dmrc)
		g_key_file_unref(options.dmrc);
	options.dmrc = defaults.dmrc;
	if (!options.session) {
		free(options.session);
		if (!(options.session = defaults.session))
			options.session = strdup("");
	}
	if (!options.current) {
		free(options.current);
		if (!(options.current = defaults.current))
			options.current = strdup("");
	}
}

void
get_default_choice(void)
{
	if (!options.choice) {
		free(options.choice);
		if (!(options.choice = defaults.choice))
			options.choice = strdup("default");
	}
}

void
get_default_username(void)
{
	struct passwd *pw;

	if (options.username)
		return;
	if (getuid() == 0)
		return;

	if (!(pw = getpwuid(getuid()))) {
		EPRINTF("cannot get users password entry\n");
		exit(EXIT_FAILURE);
	}
	free(options.username);
	options.username = strdup(pw->pw_name);
}

void
get_defaults(int argc, char *argv[])
{
	get_default_vendor();
	get_default_banner();
	get_default_splash();
	get_default_welcome();
	get_default_language();
#ifdef DO_XCHOOSER
	get_default_address();
#endif				/* DO_XCHOOSER */
	get_default_session();
	get_default_choice();
	get_default_username();
}

#ifdef DO_XCHOOSER
Bool
HexToARRAY8(ARRAY8 *array, char *hex)
{
	short len;
	CARD8 *o, b;
	char *p, c;

	len = strlen(hex);
	if (len & 0x01)
		return False;
	len >>= 1;
	XdmcpReallocARRAY8(array, len);
	for (p = hex, o = array->data; *p; p += 2, o++) {
		c = tolower(p[0]);
		if (!isxdigit(c))
			return False;
		b = ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		b <<= 4;
		c = tolower(p[1]);
		if (!isxdigit(c))
			return False;
		b += ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		*o = b;
	}
	return True;
}
#endif

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	saveArgc = argc;
	saveArgv = argv;

	setlocale(LC_ALL, "");

	set_defaults(argc, argv);

	get_resources(argc, argv);

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"prompt",	    required_argument,	NULL, 'p'},
			{"banner",	    required_argument,	NULL, 'b'},
			{"splash",	    required_argument,	NULL, 'S'},
			{"side",	    required_argument,	NULL, 's'},
			{"noask",	    no_argument,	NULL, 'n'},
			{"charset",	    required_argument,	NULL, '1'},
			{"language",	    required_argument,	NULL, '2'},
			{"icons",	    required_argument,	NULL, 'i'},
			{"theme",	    required_argument,	NULL, 't'},
			{"xde-theme",	    no_argument,	NULL, 'x'},
			{"timeout",	    required_argument,	NULL, 'T'},
			{"vendor",	    required_argument,	NULL, '5'},
			{"default",	    required_argument,	NULL, '6'},
			{"setbg",	    no_argument,	NULL, '8'},
			{"transparent",	    no_argument,	NULL, '9'},

			{"clientId",	    required_argument,	NULL, '3'},
			{"restore",	    required_argument,	NULL, '4'},

			{"dry-run",	    no_argument,	NULL, 'N'},
			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "p:b:S:s:ni:t:xT:ND::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "p:b:S:s:ni:t:xT:NDvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

#ifdef DO_XCHOOSER
		case 'x':	/* -xdmaddress HEXBYTES */
			if (options.xdmAddress.length)
				goto bad_option;
			if (!HexToARRAY8(&options.xdmAddress, optarg))
				goto bad_option;
			break;
		case 'c':	/* -clientaddress HEXBYTES */
			if (options.clientAddress.length)
				goto bad_option;
			if (!HexToARRAY8(&options.clientAddress, optarg))
				goto bad_option;
			break;
		case 't':	/* -connectionType TYPE */
			if (!strcmp(optarg, "FamilyInternet") || atoi(optarg) == FamilyInternet)
				options.connectionType = FamilyInternet;
			else if (!strcmp(optarg, "FamilyInternet6")
				 || atoi(optarg) == FamilyInternet6)
				options.connectionType = FamilyInternet6;
			else
				goto bad_option;
			break;
		case 'w':	/* -w, --welcome WELCOME */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
			break;
#else				/* DO_XCHOOSER */
		case 'p':	/* -p, --prompt PROMPT */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
			break;
#endif				/* DO_XCHOOSER */

#ifdef DO_XLOCKING
		case 'r':	/* -r, --replace */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandReplace;
			options.command = CommandReplace;
			options.replace = True;
			break;
		case 'q':	/* -q, --quit */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandQuit;
			options.command = CommandQuit;
			options.replace = True;
			break;
		case 'l':	/* -l, --lock */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandLock;
			options.command = CommandLock;
			break;
#endif				/* DO_XLOCKING */

		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			break;
		case 'S':	/* -S, --splash SPLASH */
			free(options.splash);
			options.splash = strdup(optarg);
			break;
		case 's':	/* -s, --side {top|bottom|left|right} */
			if (!strncasecmp(optarg, "left", strlen(optarg))) {
				options.side = LogoSideLeft;
				break;
			}
			if (!strncasecmp(optarg, "top", strlen(optarg))) {
				options.side = LogoSideTop;
				break;
			}
			if (!strncasecmp(optarg, "right", strlen(optarg))) {
				options.side = LogoSideRight;
				break;
			}
			if (!strncasecmp(optarg, "bottom", strlen(optarg))) {
				options.side = LogoSideBottom;
				break;
			}
			goto bad_option;
		case '1':	/* -c --charset CHARSET */
			free(options.charset);
			options.charset = strdup(optarg);
			break;
		case '2':	/* -l, --language LANG */
			free(options.language);
			options.language = strdup(optarg);
			break;
		case 'n':	/* -n, --noask */
			options.noask = True;
			break;
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 'x':	/* -x, --xde-theme */
			options.usexde = True;
			break;
		case 'T':	/* -T, --timeout TIMEOUT */
			options.timeout = strtoul(optarg, NULL, 0);
			break;
		case '5':	/* --vendor VENDOR */
			free(options.vendor);
			options.vendor = strdup(optarg);
			break;
		case '6':	/* --default DEFAULT */
			free(options.choice);
			options.choice = strdup(optarg);
			break;
		case '3':	/* -clientId CLIENTID */
			free(options.clientId);
			options.clientId = strdup(optarg);
			break;
		case '4':	/* -restore SAVEFILE */
			free(options.saveFile);
			options.saveFile = strdup(optarg);
			break;
		case '8':	/* --setbg */
			options.setbg = True;
			break;
		case '9':	/* --transparent */
			options.transparent = True;
			break;

		case 'N':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			DPRINTF("%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			DPRINTF("%s: increasing output verbosity\n", argv[0]);
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
	DPRINTF("%s: option index = %d\n", argv[0], optind);
	DPRINTF("%s: option count = %d\n", argv[0], argc);
	if (optind < argc) {
		fprintf(stderr, "%s: excess non-option arguments\n", argv[0]);
		goto bad_nonopt;
	}
	get_defaults(argc, argv);
	switch (command) {
	default:
	case CommandDefault:
		DPRINTF("%s: running logout\n", argv[0]);
		do_run(argc, argv);
		break;
	case CommandHelp:
		DPRINTF("%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF("%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF("%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
