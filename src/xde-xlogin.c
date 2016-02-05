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
#include <X11/extensions/scrnsaver.h>
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/Xdmcp.h>
#include <X11/Xauth.h>
#include <X11/SM/SMlib.h>
#include <gio/gio.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo.h>

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

const char *
timestamp(void)
{
	static struct timeval tv = { 0, 0 };
	static char buf[BUFSIZ];
	double stamp;

	gettimeofday(&tv, NULL);
	stamp = (double)tv.tv_sec + (double)((double)tv.tv_usec/1000000.0);
	snprintf(buf, BUFSIZ-1, "%f", stamp);
	return buf;
}

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stderr, "I: "); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: [%s] %s +%d %s(): ", timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: [%s] %s +%d %s(): ", timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: [%s] %s +%d %s()\n", timestamp(), __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>

static int saveArgc;
static char **saveArgv;

#undef DO_XCHOOSER
#undef DO_XLOCKING
#undef DO_ONIDLE
#undef DO_CHOOSER
#undef DO_LOGOUT

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
	CommandXlogin,
	CommandXchoose,
	CommandLocker,			/* run as a background locker */
	CommandReplace,			/* replace any running instance */
	CommandLock,			/* ask running instance to lock */
	CommandQuit,			/* ask running instance to quit */
	CommandUnlock,			/* ask running instance to unlock */
} CommandType;

enum {
	BackgroundSourceSplash = (1 << 0),
	BackgroundSourcePixmap = (1 << 1),
	BackgroundSourceRoot = (1 << 2),
};

typedef enum {
	SocketScopeLoopback,
	SocketScopeLinklocal,
	SocketScopeSitelocal,
	SocketScopePrivate,
	SocketScopeGlobal,
} SocketScope;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	SocketScope clientScope;
	uint32_t clientIface;
	Bool isLocal;
	char *banner;
	char *welcome;
	CommandType command;
	char *charset;
	char *language;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	LogoSide side;
	char *current;
	Bool managed;
	char *session;
	char *choice;
	char *username;
	char *password;
	Bool usexde;
	Bool replace;
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
	unsigned guard;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet6,
	.clientScope = SocketScopeLoopback,
	.clientIface = 0,
	.isLocal = False,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.replace = False,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
	.source = BackgroundSourceSplash,
	.xsession = False,
	.setbg = True,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
	.guard = 5,
};

Options defaults = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet6,
	.clientScope = SocketScopeLoopback,
	.clientIface = 0,
	.isLocal = False,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.replace = False,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
	.source = BackgroundSourceSplash,
	.xsession = False,
	.setbg = True,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
	.guard = 5,
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
	LoginStateInit,
	LoginStateUsername,
	LoginStatePassword,
	LoginStateReady,
} LoginState;

LoginState state = LoginStateInit;

#ifdef DO_XLOCKING
typedef enum {
	LockStateLocked,
	LockStateUnlocked,
	LockStateAborted,
} LockState;

LockState lock_state = LockStateLocked;
struct timeval lock_time = { 0, 0 };

typedef enum {
	LockCommandLock,
	LockCommandUnlock,
	LockCommandQuit,
} LockCommand;

#endif				/* DO_XLOCKING */

typedef enum {
	LoginResultLogout,
	LoginResultLaunch,
} LoginResult;

LoginResult login_result;

Atom _XA_XDE_THEME_NAME;
Atom _XA_GTK_READ_RCFILES;
Atom _XA_XDE_XLOCK_COMMAND;
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
typedef struct {
	int xfd;
	Display *dpy;
} XdeDisplay;

XdeDisplay display;

const char *
show_state(int state)
{
	switch (state) {
	case ScreenSaverOff:
		return ("Off");
	case ScreenSaverOn:
		return ("On");
	case ScreenSaverDisabled:
		return ("Disabled");
	default:
		return ("(unknown)");
	}
}

const char *
show_kind(int kind)
{
	switch (kind) {
	case ScreenSaverBlanked:
		return ("Blanked");
	case ScreenSaverInternal:
		return ("Internal");
	case ScreenSaverExternal:
		return ("External");
	default:
		return ("(unknown)");
	}
}

void
setup_screensaver(void)
{
	Display *dpy = display.dpy;
	int s, nscr = ScreenCount(dpy);
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
	} else {
		DPRINTF("xssEventBase = %d\n", xssEventBase);
		DPRINTF("xssErrorBase = %d\n", xssErrorBase);
	}
	if (!(status = XScreenSaverQueryVersion(dpy, &xssMajorVersion, &xssMinorVersion))) {
		DPRINTF("cannot query MIT-SCREEN-SAVER version\n");
		return;
	} else {
		DPRINTF("xssMajorVersion = %d\n", xssMajorVersion);
		DPRINTF("xssMinorVersion = %d\n", xssMinorVersion);
	}
	for (s = 0, xscr = screens; s < nscr; s++, xscr++) {
		XScreenSaverQueryInfo(dpy, RootWindow(dpy, s), &xscr->info);
		if (options.debug > 1) {
			fprintf(stderr, "Before:\n");
			fprintf(stderr, "\twindow:\t\t0x%08lx\n", xscr->info.window);
			fprintf(stderr, "\tstate:\t\t%s\n", show_state(xscr->info.state));
			fprintf(stderr, "\tkind:\t\t%s\n", show_kind(xscr->info.kind));
			fprintf(stderr, "\ttil_or_since:\t%lu\n", xscr->info.til_or_since);
			fprintf(stderr, "\tidle:\t\t%lu\n", xscr->info.idle);
			fprintf(stderr, "\teventMask:\t0x%08lx\n", xscr->info.eventMask);
		}
		XScreenSaverSelectInput(dpy, RootWindow(dpy, s),
					ScreenSaverNotifyMask | ScreenSaverCycleMask);
		XScreenSaverQueryInfo(dpy, RootWindow(dpy, s), &xscr->info);
		if (options.debug > 1) {
			fprintf(stderr, "After:\n");
			fprintf(stderr, "\twindow:\t\t0x%08lx\n", xscr->info.window);
			fprintf(stderr, "\tstate:\t\t%s\n", show_state(xscr->info.state));
			fprintf(stderr, "\tkind:\t\t%s\n", show_kind(xscr->info.kind));
			fprintf(stderr, "\ttil_or_since:\t%lu\n", xscr->info.til_or_since);
			fprintf(stderr, "\tidle:\t\t%lu\n", xscr->info.idle);
			fprintf(stderr, "\teventMask:\t0x%08lx\n", xscr->info.eventMask);
		}
	}
}

#endif /* DO_XLOCKING */

GDBusProxy *sd_manager = NULL;
GDBusProxy *sd_session = NULL;

#ifdef DO_XLOCKING
static void LockScreen(void);
static void UnlockScreen(void);
static void AbortLockScreen(void);
#endif

void
on_sd_prox_manager_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
			  GVariant *parameters, gpointer user_data)
{
	DPRINTF("received manager proxy signal %s( %s )\n", signal_name,
		g_variant_get_type_string(parameters));

	if (!strcmp(signal_name, "PrepareForSleep")) {
	} else if (!strcmp(signal_name, "PrepareForShutdown")) {
	}
}

void
on_sd_prox_session_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
			  GVariant *parameters, gpointer user_data)
{
	DPRINTF("received session proxy signal %s( %s )\n", signal_name,
		g_variant_get_type_string(parameters));
#ifdef DO_XLOCKING
	if (!strcmp(signal_name, "Lock")) {
		LockScreen();
	} else if (!strcmp(signal_name, "Unlock")) {
		UnlockScreen();
	}
#endif
}

void
on_sd_prox_session_props_changed(GDBusProxy *proxy, GVariant *changed_properties,
				 GStrv invalidated_properties, gpointer user_data)
{
	GVariantIter iter;
	GVariant *prop;

	DPRINTF("received session proxy properties changed signal ( %s )\n",
		g_variant_get_type_string(changed_properties));

	g_variant_iter_init(&iter, changed_properties);
	while ((prop = g_variant_iter_next_value(&iter))) {
		if (g_variant_is_container(prop)) {
			GVariantIter iter2;
			GVariant *key;
			GVariant *val;
			GVariant *boxed;
			const gchar *name;

			g_variant_iter_init(&iter2, prop);
			if (!(key = g_variant_iter_next_value(&iter2))) {
				EPRINTF("no key!\n");
				continue;
			}
			if (!(name = g_variant_get_string(key, NULL))) {
				EPRINTF("no name!\n");
				g_variant_unref(key);
				continue;
			}
			if (strcmp(name, "Active")) {
				DPRINTF("not looking for %s\n", name);
				g_variant_unref(key);
				continue;
			}
			if (!(val = g_variant_iter_next_value(&iter2))) {
				EPRINTF("no val!\n");
				g_variant_unref(key);
				continue;
			}
			if (!(boxed = g_variant_get_variant(val))) {
				EPRINTF("no value!\n");
				g_variant_unref(key);
				g_variant_unref(val);
				continue;
			}
			if (!g_variant_get_boolean(boxed)) {
#ifdef DO_XLOCKING
				DPRINTF("went inactive, locking screen\n");
				LockScreen();
#endif
			}
			g_variant_unref(key);
			g_variant_unref(val);
			g_variant_unref(boxed);
			break;
		}
		g_variant_unref(prop);
	}
}

void
setup_systemd(void)
{
	GError *err = NULL;
	gchar *s;

	DPRINT();
	if (!(sd_manager =
	      g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL, "org.freedesktop.login1",
					    "/org/freedesktop/login1",
					    "org.freedesktop.login1.Manager", NULL, &err)) || err) {
		EPRINTF("could not create DBUS proxy sd_manager: %s\n",
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_signal_connect(G_OBJECT(sd_manager), "g-signal",
			 G_CALLBACK(on_sd_prox_manager_signal), NULL);
	s = g_strdup_printf("/org/freedesktop/login1/session/%s", getenv("XDG_SESSION_ID"));
	if (!(sd_session =
	      g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL, "org.freedesktop.login1", s,
					    "org.freedesktop.login1.Session", NULL, &err)) || err) {
		EPRINTF("could not create DBUS proxy sd_session: %s\n",
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_signal_connect(G_OBJECT(sd_session), "g-signal",
			 G_CALLBACK(on_sd_prox_session_signal), NULL);
	g_signal_connect(G_OBJECT(sd_session), "g-properties-changed",
			 G_CALLBACK(on_sd_prox_session_props_changed), NULL);
	g_free(s);
}

#ifdef DO_XLOCKING

void
setidlehint(gboolean flag)
{
	GError *err = NULL;
	GVariant *result;

	if (!sd_session) {
		EPRINTF("No session proxy!\n");
		return;
	}
	if (!(result =
	      g_dbus_proxy_call_sync(sd_session, "SetIdleHint", g_variant_new("(b)", flag),
				     G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err)) || err) {
		EPRINTF("SetIdleHint: %s: call failed: %s\n", getenv("XDG_SESSION_ID"),
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_variant_unref(result);
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
	switch (ev->state) {
	case ScreenSaverDisabled:
	default:
		break;
	case ScreenSaverOff:
		/* Note that if we were not locked for more than a few seconds we should
		   simply unlock the screen.  This way if the user presses a key or
		   nudges the mouse fast enought when the screen blanks, they will not
		   have to reenter a password.  This could be coupled with screen fading
		   during the grace period.  */
		setidlehint(FALSE);
		AbortLockScreen();
		break;
	case ScreenSaverOn:
		setidlehint(TRUE);
		LockScreen();
		break;
	case ScreenSaverCycle:
		LockScreen();
		break;
	}
	return G_SOURCE_CONTINUE;
}
#endif				/* DO_XLOCKING */

#ifdef DO_XCHOOSER
#define PING_TRIES	3
#define PING_INTERVAL	2	/* 2 seconds */

XdmcpBuffer directBuffer;
XdmcpBuffer broadcastBuffer;

static gpointer
sockaddr_copy_func(gpointer boxed)
{
	struct sockaddr_storage *sa = boxed;
	struct sockaddr_storage *na = NULL;

	if (sa && (na = calloc(1, sizeof(*na))))
		memmove(na, sa, sizeof(*na));
	return (na);
}

static void
sockaddr_free_func(gpointer boxed)
{
	free(boxed);
}

static GType
g_sockaddr_get_type(void)
{
	static int initialized = 0;
	static GType mytype;

	if (!initialized) {
		mytype = g_boxed_type_register_static("sockaddr",
						      sockaddr_copy_func, sockaddr_free_func);
		initialized = 1;
	}
	return mytype;
}

#undef G_TYPE_SOCKADDR
#define G_TYPE_SOCKADDR (g_sockaddr_get_type())

enum {
	XDM_COL_HOSTNAME,		/* the manager hostname */
	XDM_COL_REMOTENAME,		/* the manager remote name */
	XDM_COL_WILLING,		/* the willing status */
	XDM_COL_STATUS,			/* the status */
	XDM_COL_IPADDR,			/* the ip address */
	XDM_COL_CTYPE,			/* the connection type */
	XDM_COL_SERVICE,		/* the service */
	XDM_COL_PORT,			/* the port number */
	XDM_COL_MARKUP,			/* the combined markup description */
	XDM_COL_TOOLTIP,		/* the tooltip information */
	XDM_COL_SOCKADDR,		/* the socket address */
	XDM_COL_SCOPE,			/* the socket address scope */
	XDM_COL_IFINDEX,		/* the socket interface index */
};
#endif				/* DO_XCHOOSER */

enum {
	XSESS_COL_PIXBUF,		/* the icon name for the pixbuf */
	XSESS_COL_NAME,			/* the Name= of the XSession */
	XSESS_COL_COMMENT,		/* the Comment= of the XSession */
	XSESS_COL_MARKUP,		/* Combined Name/Comment markup */
	XSESS_COL_LABEL,		/* the label (appid) of the XSession */
	XSESS_COL_MANAGED,		/* XDE-Managed? editable setting */
	XSESS_COL_ORIGINAL,		/* XDE-Managed? original setting */
	XSESS_COL_FILENAME,		/* the full file name */
	XSESS_COL_TOOLTIP,		/* the tooltip information */
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
#ifdef DO_XLOCKING
	if (xev->xclient.message_type == _XA_XDE_XLOCK_COMMAND) {
		switch (xev->xclient.data.l[0]) {
		case LockCommandLock:
			LockScreen();
			return GDK_FILTER_REMOVE;
		case LockCommandUnlock:
			UnlockScreen();
			return GDK_FILTER_REMOVE;
		case LockCommandQuit:
			exit(EXIT_SUCCESS);
		default:
			break;
		}
	}
#endif
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
	default:
#ifdef DO_XLOCKING
		if (xssEventBase && xev->type == xssEventBase + ScreenSaverNotify)
			return handle_XScreenSaverNotify(dpy, xev);
		DPRINTF("unknown event type %d\n", xev->type);
#endif				/* DO_XLOCKING */
		break;
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
	case ClientMessage:
		break;
	default:
		EPRINTF("wrong message type for handler %d\n", xev->type);
		break;
	}
	return GDK_FILTER_CONTINUE;
}
#endif

static GdkFilterReturn
client_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	Display *dpy = xev->xany.display;

	DPRINT();
	switch (xev->type) {
	case ClientMessage:
		return event_handler_ClientMessage(dpy, xev);
	}
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}

GtkListStore *store;			/* list store for XSessions */
GtkWidget *sess;

#define XDE_ICON_GEOM 16
#define XDE_ICON_SIZE GTK_ICON_SIZE_MENU

void
on_render_pixbuf(GtkCellLayout * sess, GtkCellRenderer *cell,
		 GtkTreeModel *store, GtkTreeIter *iter, gpointer data)
{
	GValue iname_v = G_VALUE_INIT;
	const gchar *iname;
	gchar *name = NULL, *p;
	gboolean has;
	GValue pixbuf_v = G_VALUE_INIT;

	gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, XSESS_COL_PIXBUF, &iname_v);
	if ((iname = g_value_get_string(&iname_v))) {
		name = g_strdup(iname);
		/* should we really do this? */
		if ((p = strstr(name, ".xpm")) && !p[4])
			*p = '\0';
		else if ((p = strstr(name, ".svg")) && !p[4])
			*p = '\0';
		else if ((p = strstr(name, ".png")) && !p[4])
			*p = '\0';
	} else
		name = g_strdup("preferences-system-windows");
	g_value_unset(&iname_v);
	XPRINTF("will try to render icon name = \"%s\"\n", name);

	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GdkPixbuf *pixbuf = NULL;

	XPRINTF("checking icon \"%s\"\n", name);
	has = gtk_icon_theme_has_icon(theme, name);
	if (has) {
		XPRINTF("trying to load icon \"%s\"\n", name);
		pixbuf = gtk_icon_theme_load_icon(theme, name, XDE_ICON_GEOM,
						  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
						  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	}
	if (!has || !pixbuf) {
		g_free(name);
		name = g_strdup("preferences-system-windows");
		XPRINTF("checking icon \"%s\"\n", name);
		has = gtk_icon_theme_has_icon(theme, name);
		if (has) {
			XPRINTF("tyring to load icon \"%s\"\n", name);
			pixbuf = gtk_icon_theme_load_icon(theme, name, XDE_ICON_GEOM,
							  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
							  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
		}
		if (!has || !pixbuf) {
			GtkWidget *image;

			XPRINTF("tyring to load image \"%s\"\n", "gtk-missing-image");
			if ((image = gtk_image_new_from_stock("gtk-missing-image",
							      XDE_ICON_SIZE))) {
				XPRINTF("tyring to load icon \"%s\"\n", "gtk-missing-image");
				pixbuf = gtk_widget_render_icon(GTK_WIDGET(image),
								"gtk-missing-image",
								XDE_ICON_SIZE, NULL);
				g_object_unref(G_OBJECT(image));
			}
		}
	}
	if (pixbuf) {
		XPRINTF("setting pixbuf for cell renderrer\n");
		g_value_init(&pixbuf_v, G_TYPE_OBJECT);
		g_value_take_object(&pixbuf_v, pixbuf);
		g_object_set_property(G_OBJECT(cell), "pixbuf", &pixbuf_v);
		g_value_unset(&pixbuf_v);
	}
}

char **
get_xsession_dirs(int *np)
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
	for (n = 0, pos = dirs; pos < end; n++,
	     *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n + 1, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1) {
		len = strlen(pos) + strlen("/xsessions") + 1;
		xdg_dirs[n] = calloc(len, sizeof(*xdg_dirs[n]));
		strcpy(xdg_dirs[n], pos);
		strcat(xdg_dirs[n], "/xsessions");
	}
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

GKeyFile *
get_xsession_entry(const char *key, const char *file)
{
	GKeyFile *entry;

	if (!(entry = g_key_file_new())) {
		EPRINTF("%s: could not allocate key file\n", file);
		return (NULL);
	}
	if (!g_key_file_load_from_file(entry, file, G_KEY_FILE_NONE, NULL)) {
		EPRINTF("%s: could not load keyfile\n", file);
		g_key_file_unref(entry);
		return (NULL);
	}
	if (!g_key_file_has_group(entry, G_KEY_FILE_DESKTOP_GROUP)) {
		EPRINTF("%s: has no [%s] section\n", file, G_KEY_FILE_DESKTOP_GROUP);
		g_key_file_free(entry);
		return (NULL);
	}
	if (!g_key_file_has_key(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
		EPRINTF("%s: has no %s= entry\n", file, G_KEY_FILE_DESKTOP_KEY_TYPE);
		g_key_file_free(entry);
		return (NULL);
	}
	DPRINTF("got xsession file: %s (%s)\n", key, file);
	return (entry);
}

/** @brief wean out entries that should not be used
  */
gboolean
bad_xsession(const char *appid, GKeyFile *entry)
{
	gchar *name, *exec, *tryexec, *binary;

	if (!(name = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					   G_KEY_FILE_DESKTOP_KEY_NAME, NULL))) {
		DPRINTF("%s: no Name\n", appid);
		return TRUE;
	}
	g_free(name);
	if (!(exec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					   G_KEY_FILE_DESKTOP_KEY_EXEC, NULL))) {
		DPRINTF("%s: no Exec\n", appid);
		return TRUE;
	}
	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
				   G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL)) {
		DPRINTF("%s: is Hidden\n", appid);
		return TRUE;
	}
#if 0
	/* NoDisplay is often used to hide XSession desktop entries from the
	   application menu and does not indicate that it should not be
	   displayed as an XSession entry. */

	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
				   G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL)) {
		DPRINTF("%s: is NoDisplay\n", appid);
		return TRUE;
	}
#endif
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
			// to much noise
			// DPRINTF("%s: %s: %s\n", appid, file,
			// strerror(errno));
		}
		free(path);
		if (!execok) {
			DPRINTF("%s: %s: not executable\n", appid, binary);
			g_free(binary);
			return TRUE;
		}
	}
	return FALSE;
}

static void
xsession_key_free(gpointer data)
{
	free(data);
}

static void
xsession_value_free(gpointer filename)
{
	free(filename);
}

/** @brief just gets the filenames of the xsession files
  *
  * This just gets the filenames of the xsession files to avoid performing a lot
  * of time consuming startup during the login.  We process the actual xsession
  * files and add them to the list out of an idle loop.
  */
GHashTable *
get_xsessions(void)
{
	char **xdg_dirs, **dirs;
	int i, n = 0;
	static const char *suffix = ".desktop";
	static const int suflen = 8;
	static GHashTable *xsessions = NULL;

	if (xsessions)
		return (xsessions);

	if (!(xdg_dirs = get_xsession_dirs(&n)) || !n)
		return (xsessions);

	xsessions = g_hash_table_new_full(g_str_hash, g_str_equal,
					  xsession_key_free, xsession_value_free);

	/* go through them backward */
	for (i = n - 1, dirs = &xdg_dirs[i]; i >= 0; i--, dirs--) {
		char *file, *p;
		DIR *dir;
		struct dirent *d;
		int len;
		char *key;

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
			key = strdup(d->d_name);
			*strstr(key, suffix) = '\0';
			g_hash_table_replace(xsessions, key, file);
		}
		closedir(dir);
	}
	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
	return (xsessions);
}

gboolean
on_idle(gpointer data)
{
	static GHashTable *xsessions = NULL;
	static GHashTableIter xiter;
	GtkListStore *store = data;
	const char *key;
	const char *file;
	GKeyFile *entry;
	GtkTreeIter iter;

	if (!xsessions) {
		if (!(xsessions = get_xsessions())) {
			EPRINTF("cannot build XSessions\n");
			return G_SOURCE_REMOVE;
		}
		if (!g_hash_table_size(xsessions)) {
			EPRINTF("cannot find any XSessions\n");
			return G_SOURCE_REMOVE;
		}
		g_hash_table_iter_init(&xiter, xsessions);
	}
	if (!g_hash_table_iter_next(&xiter, (gpointer *) & key, (gpointer *) & file))
		return G_SOURCE_REMOVE;

	if (!(entry = get_xsession_entry(key, file)))
		return G_SOURCE_CONTINUE;

	if (bad_xsession(key, entry)) {
		g_key_file_free(entry);
		return G_SOURCE_CONTINUE;
	}

	gchar *i, *n, *c, *k, *e, *l, *f, *t;
	gboolean m;

	f = g_strdup(file);
	l = g_strdup(key);
	i = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
				  G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
	n = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					 G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL) ? : g_strdup("");
	c = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					 G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL,
					 NULL) ? : g_strdup("");
	k = g_markup_printf_escaped("<b>%s</b>\n%s", n, c);
	e = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
				  G_KEY_FILE_DESKTOP_KEY_EXEC, NULL) ? : g_strdup("");
	m = g_key_file_get_boolean(entry, "Window Manager", "X-XDE-Managed", NULL);
	if (options.debug) {
		t = g_markup_printf_escaped("<b>Name:</b> %s" "\n"
					    "<b>Comment:</b> %s" "\n"
					    "<b>Exec:</b> %s" "\n"
					    "<b>Icon:</b> %s" "\n" "<b>file:</b> %s", n, c, e, i,
					    f);
	} else {
		t = g_markup_printf_escaped("<b>%s</b>: %s", n, c);
	}

	gtk_list_store_append(store, &iter);
	/* *INDENT-OFF* */
	gtk_list_store_set(store, &iter,
			XSESS_COL_PIXBUF,	i,
			XSESS_COL_NAME,		n,
			XSESS_COL_COMMENT,	c,
			XSESS_COL_MARKUP,	k,
			XSESS_COL_LABEL,	l,
			XSESS_COL_MANAGED,	m,
			XSESS_COL_ORIGINAL,	m,
			XSESS_COL_FILENAME,	f,
			XSESS_COL_TOOLTIP,	t,
			-1);
	/* *INDENT-ON* */
	g_free(f);
	g_free(l);
	g_free(i);
	g_free(n);
	g_free(k);
	g_free(e);
	g_free(t);
	g_key_file_free(entry);
#if 1
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					     GTK_SORT_ASCENDING);
#endif

#if 0
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));

	if (gtk_tree_selection_get_selected(selection, NULL, NULL))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.session, key))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.choice, "choose")
	    && strcmp(options.choice, "default"))
		return G_SOURCE_CONTINUE;
	gtk_tree_selection_select_iter(selection, &iter);
#ifdef DO_ONIDLE
	gchar *string;

	if ((string = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(store), &iter))) {
		GtkTreeView *tree = GTK_TREE_VIEW(sess);
		GtkTreePath *path = gtk_tree_path_new_from_string(string);
		GtkTreeViewColumn *cursor = gtk_tree_view_get_column(tree, 1);

		g_free(string);
		gtk_tree_view_set_cursor_on_cell(tree, path, cursor, NULL, FALSE);
		gtk_tree_view_scroll_to_cell(tree, path, cursor, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
#endif
#else
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(sess)) != -1)
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.session, key))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.choice, "choose")
	    && strcmp(options.choice, "default"))
		return G_SOURCE_CONTINUE;
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(sess), &iter);
#endif
	return G_SOURCE_CONTINUE;
}

#ifdef DO_XCHOOSER
GtkListStore *model;
GtkWidget *view;
#endif				/* DO_XCHOOSER */

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
#ifdef DO_XLOCKING
	xhome = getenv("XDG_DATA_HOME");
#else
	xhome = "/usr/lib/X11/xdm";
#endif
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

static GtkWidget *buttons[5];
static GtkWidget *l_uname;
static GtkWidget *l_pword;
static GtkWidget *l_lstat;
static GtkWidget *l_greet;
static GtkWidget *user, *pass;

gint
xsession_compare_function(GtkTreeModel *store, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	GValue a_v = G_VALUE_INIT;
	GValue b_v = G_VALUE_INIT;
	const gchar *astr;
	const gchar *bstr;
	gint ret;

	gtk_tree_model_get_value(GTK_TREE_MODEL(store), a, XSESS_COL_NAME, &a_v);
	gtk_tree_model_get_value(GTK_TREE_MODEL(store), b, XSESS_COL_NAME, &b_v);
	astr = g_value_get_string(&a_v);
	bstr = g_value_get_string(&b_v);
	ret = g_ascii_strcasecmp(astr, bstr);
	g_value_unset(&a_v);
	g_value_unset(&b_v);
	return (ret);
}

#ifdef DO_XCHOOSER
Bool
CanConnect(struct sockaddr *sa)
{
	int sock;
	socklen_t salen;

	switch (sa->sa_family) {
	case AF_INET:
		salen = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		salen = sizeof(struct sockaddr_in6);
		break;
	case AF_UNIX:
		salen = sizeof(struct sockaddr_un);
		break;
	default:
		EPRINTF("wrong socket family %d\n", (int) sa->sa_family);
		return False;
	}
	if ((sock = socket(sa->sa_family, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: %s\n", strerror(errno));
		return False;
	}
	if (options.debug) {
		char *p, *e, *rawbuf;
		unsigned char *b;
		int i, len;

		len = 2 * salen + 1;
		rawbuf = calloc(len, sizeof(*rawbuf));
		for (i = 0, p = rawbuf, e = rawbuf + len, b = (typeof(b)) sa;
		     i < salen; i++, p += 2, b++)
			snprintf(p, e - p, "%02x", *b);
		DPRINTF("raw socket address for connect: %s\n", rawbuf);
		free(rawbuf);
	}
	if (connect(sock, sa, salen) == -1) {
		DPRINTF("connect: %s\n", strerror(errno));
		close(sock);
		return False;
	}
	if (options.debug) {
		struct sockaddr conn;
		char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };

		if (getsockname(sock, &conn, &salen) == -1) {
			EPRINTF("getsockname: %s\n", strerror(errno));
			close(sock);
			return False;
		}
		switch (conn.sa_family) {
		case AF_INET:
		{
			struct sockaddr_in *sin = (typeof(sin)) & conn;
			int port = ntohs(sin->sin_port);

			inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
			DPRINTF("address is %s port %d\n", ipaddr, port);
			break;
		}
		case AF_INET6:
		{
			struct sockaddr_in6 *sin6 = (typeof(sin6)) & conn;
			int port = ntohs(sin6->sin6_port);

			inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
			DPRINTF("address is %s port %d\n", ipaddr, port);
			break;
		}
		case AF_UNIX:
		{
			struct sockaddr_un *sun = (typeof(sun)) & conn;

			DPRINTF("family is AF_UNIX\n");
			break;
		}
		default:
			EPRINTF("bad connected family %d\n", (int) conn.sa_family);
			close(sock);
			return False;
		}
	}
	close(sock);
	return True;
}

#define IN_LINKLOCAL(a) ((((in_addr_t)(a)) & 0xffff0000) == 0xa9fe0000)
#define IN_LOOPBACK(a)	((((in_addr_t)(a)) & 0xffffff00) == 0x7f000000)
#define IN_ORGLOCAL(a) ( \
	((((in_addr_t)(a)) & 0xff000000) == 0x0a000000) || \
	((((in_addr_t)(a)) & 0xfff00000) == 0xac100000) || \
	((((in_addr_t)(a)) & 0xffff0000) == 0xc0a80000))

static SocketScope
getaddrscope(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin = (typeof(sin)) sa;
		in_addr_t addr = ntohl(sin->sin_addr.s_addr);

		if (IN_LOOPBACK(addr))
			return SocketScopeLoopback;
		if (IN_LINKLOCAL(addr))
			return SocketScopeLinklocal;
		if (IN_ORGLOCAL(addr))
			return SocketScopePrivate;
		return SocketScopeGlobal;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6 = (typeof(sin6)) sa;
		struct in6_addr *addr = &sin6->sin6_addr;

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
	case AF_UNSPEC:
	case AF_UNIX:
		break;
	}
	return SocketScopeLoopback;
}

Bool
AddHost(struct sockaddr *sa, socklen_t salen, int ifindex, xdmOpCode opc,
	ARRAY8 *authname_a, ARRAY8 *hostname_a, ARRAY8 *status_a)
{
	int ctype;
	sa_family_t family;
	short port;
	char remotename[NI_MAXHOST + 1] = { 0, };
	char service[NI_MAXSERV + 1] = { 0, };
	char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };
	char hostname[NI_MAXHOST + 1] = { 0, };
	char markup[BUFSIZ + 1] = { 0, };
	char tooltip[BUFSIZ + 1] = { 0, };
	char status[256] = { 0, };
	socklen_t len;
	SocketScope scope;

	DPRINT();

	scope = getaddrscope(sa);
	if (scope < options.clientScope) {
		DPRINTF("cannot use local scoped address for remote clients\n");
		return False;
	}
	switch (scope) {
	case SocketScopeLinklocal:
	case SocketScopeSitelocal:
		if (!ifindex) {
			DPRINTF("cannot use site/link local address without ifindex\n");
			return False;
		}
		if (scope == options.clientScope && ifindex != options.clientIface) {
			DPRINTF("cannot use site/link local address with other ifindex\n");
			return False;
		}
		break;
	default:
		break;
	}

	len = hostname_a->length;
	if (len > NI_MAXHOST)
		len = NI_MAXHOST;
	strncpy(hostname, (char *) hostname_a->data, len);
	DPRINTF("hostname is %s\n", hostname);

	len = status_a->length;
	if (len > sizeof(status) - 1)
		len = sizeof(status) - 1;
	strncpy(status, (char *) status_a->data, len);
	DPRINTF("status is %s\n", status);

	switch ((family = sa->sa_family)) {
	case AF_INET:
	{
		struct sockaddr_in *sin = (typeof(sin)) sa;

		salen = sizeof(*sin);

		DPRINTF("family is AF_INET\n");
		ctype = FamilyInternet;
		port = ntohs(sin->sin_port);
		inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
		DPRINTF("address is %s port %hd\n", ipaddr, port);
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6 = (typeof(sin6)) sa;

		salen = sizeof(*sin6);

		DPRINTF("family is AF_INET6\n");
		ctype = FamilyInternet6;
		port = ntohs(sin6->sin6_port);
		inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
		DPRINTF("address is %s port %hd\n", ipaddr, port);
		break;
	}
	case AF_UNIX:
	{
		struct sockaddr_un *sun = (typeof(sun)) sa;

		salen = sizeof(*sun);

		DPRINTF("family is AF_UNIX\n");
		ctype = FamilyLocal;
		port = 0;
		/* FIXME: display address in debug mode */
		break;
	}
	default:
		return False;
	}
	if (options.isLocal && !CanConnect(sa)) {
		DPRINTF("cannot connect\n");
		return False;
	}
	if (!options.isLocal && options.connectionType != FamilyInternet6
	    && options.connectionType != ctype) {
		DPRINTF("wrong connection type\n");
		return False;
	}
	/* We really do not want to do this for IPv4LL addresses, beause they
	 * can take 5 seconds to fail on reverse DNS lookups. */
	if (scope == SocketScopeLinklocal) {
		struct servent *serv;

		strncpy(remotename, ipaddr, NI_MAXHOST);
		if ((serv = getservbyport(port, "udp")))
			strncpy(service, serv->s_name, NI_MAXSERV);
	} else {
		DPRINTF("beg calling getnameinfo ...\n");
		if (getnameinfo(sa, salen, remotename, NI_MAXHOST, service, NI_MAXSERV, NI_DGRAM) == -1) {
			DPRINTF("getnameinfo: %s\n", strerror(errno));
			return False;
		}
		DPRINTF("... calling getnameinfo end\n");
	}

	GtkTreeIter iter;
	gboolean valid;

	for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, 0); valid;
	     valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
		GValue sock_v = G_VALUE_INIT;
		const struct sockaddr *sock;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, XDM_COL_SOCKADDR, &sock_v);
		sock = g_value_get_boxed(&sock_v);
		if (!memcmp(sock, sa, salen)) {
			g_value_unset(&sock_v);
			break;
		}
		g_value_unset(&sock_v);
	}
	if (!valid)
		gtk_list_store_append(model, &iter);
	/* *INDENT-OFF* */
	gtk_list_store_set(model, &iter,
			   XDM_COL_HOSTNAME, hostname,
			   XDM_COL_REMOTENAME, remotename,
			   XDM_COL_WILLING, opc,
			   XDM_COL_STATUS, status,
			   XDM_COL_IPADDR, ipaddr,
			   XDM_COL_CTYPE, ctype,
			   XDM_COL_SERVICE, service,
			   XDM_COL_PORT, port,
			   XDM_COL_SOCKADDR, sa,
			   XDM_COL_SCOPE, scope,
			   XDM_COL_IFINDEX, ifindex,
			   -1);
	/* *INDENT-ON* */

	const char *conntype;

	strncpy(markup, "", sizeof(markup));
	strncpy(tooltip, "", sizeof(tooltip));

	switch (ctype) {
	case FamilyLocal:
		conntype = "UNIX Domain";
		break;
	case FamilyInternet:
		conntype = "TCP (IP Version 4)";
		break;
	case FamilyInternet6:
		conntype = "TCP (IP Version 6)";
		break;
	default:
		conntype = "";
		break;
	}

	strncat(tooltip, "<small><b>Hostname:</b>\t", BUFSIZ);
	strncat(tooltip, hostname, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Alias:</b>\t\t", BUFSIZ);
	strncat(tooltip, remotename, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	if (opc == WILLING) {
		strncat(markup, "<span foreground=\"black\"><b>", BUFSIZ);
		strncat(markup, hostname, BUFSIZ);
		strncat(markup, "</b></span>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"black\">(", BUFSIZ);
		strncat(markup, remotename, BUFSIZ);
		strncat(markup, ")</span></small>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"black\"><i>", BUFSIZ);
		strncat(markup, status, BUFSIZ);
		strncat(markup, "</i></span></small>", BUFSIZ);

		strncat(tooltip, "<small><b>Willing:</b>\t\t", BUFSIZ);
		len = strlen(tooltip);
		snprintf(tooltip + len, BUFSIZ - len, "Willing(%d)", (int) opc);
		strncat(tooltip, "</small>\n", BUFSIZ);

		strncat(tooltip, "<small><b>Status:</b>\t\t", BUFSIZ);
		strncat(tooltip, status, BUFSIZ);
		strncat(tooltip, "</small>\n", BUFSIZ);
	} else {
		strncat(markup, "<span foreground=\"grey\"><b>", BUFSIZ);
		strncat(markup, hostname, BUFSIZ);
		strncat(markup, "</b></span>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"grey\">(", BUFSIZ);
		strncat(markup, remotename, BUFSIZ);
		strncat(markup, "</span></small>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"grey\"><i>", BUFSIZ);
		strncat(markup, "Unwilling(6)", BUFSIZ);
		strncat(markup, "</i></span></small>", BUFSIZ);

		strncat(tooltip, "<small><b>Willing:</b>\t\t", BUFSIZ);
		len = strlen(tooltip);
		snprintf(tooltip + len, BUFSIZ - len, "Unwilling(%d)", (int) opc);
		strncat(tooltip, "</small>\n", BUFSIZ);
	}

	strncat(tooltip, "<small><b>IP Address:</b>\t", BUFSIZ);
	strncat(tooltip, ipaddr, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Scope:</b>\t\t", BUFSIZ);
	len = strlen(tooltip);
	switch (scope) {
	case SocketScopeLoopback:
		snprintf(tooltip + len, BUFSIZ - len, "Loopback [%d]", ifindex);
		break;
	case SocketScopeLinklocal:
		snprintf(tooltip + len, BUFSIZ - len, "Link Local [%d]", ifindex);
		break;
	case SocketScopeSitelocal:
		snprintf(tooltip + len, BUFSIZ - len, "Site Local [%d]", ifindex);
		break;
	case SocketScopePrivate:
		snprintf(tooltip + len, BUFSIZ - len, "Private Network");
		break;
	case SocketScopeGlobal:
		snprintf(tooltip + len, BUFSIZ - len, "Global Network");
		break;
	}
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>ConnType:</b>\t", BUFSIZ);
	strncat(tooltip, conntype, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Service:</b>\t\t", BUFSIZ);
	strncat(tooltip, service, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Port:</b>\t\t", BUFSIZ);
	len = strlen(tooltip);
	snprintf(tooltip + len, BUFSIZ - len, "%d", (int) port);
	strncat(tooltip, "</small>", BUFSIZ);

	DPRINTF("markup is:\n%s\n", markup);
	DPRINTF("tooltip is:\n%s\n", tooltip);

	gtk_list_store_set(model, &iter, XDM_COL_MARKUP, markup, XDM_COL_TOOLTIP, tooltip, -1);

	relax();
	return True;

}

/*
 * Does what XdmcpFill does but also retrieves the interface index of the
 * received interface on IP version 4 sockets or scope_id on IP version 6
 * sockets.
 */
int
XdmcpRecv(int fd, XdmcpBufferPtr buffer, XdmcpNetaddr from, int *fromlen, int *fromif)
{
	static char cbuf[BUFSIZ];
	BYTE *newBuf;
	struct iovec iov;
	struct msghdr msg;

	if (buffer->size < XDM_MAX_MSGLEN) {
		newBuf = calloc(XDM_MAX_MSGLEN, sizeof(*newBuf));
		if (newBuf) {
			free(buffer->data);
			buffer->data = newBuf;
			buffer->size = XDM_MAX_MSGLEN;
		}
	}

	iov.iov_base = (void *) buffer->data;
	iov.iov_len = buffer->size;

	msg.msg_name = (void *) from;
	msg.msg_namelen = *fromlen;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = (void *) cbuf;
	msg.msg_controllen = sizeof(cbuf);
	msg.msg_flags = 0;

	buffer->pointer = 0;
	buffer->count = recvmsg(fd, &msg, 0);

	if (buffer->count == -1) {
		buffer->count = 0;
		return FALSE;
	}
	if (buffer->count < 6) {
		buffer->count = 0;
		errno = EMSGSIZE;
		return FALSE;
	}

	*fromlen = msg.msg_namelen;
	*fromif = 0;
	switch (((struct sockaddr *) from)->sa_family) {
	case AF_INET:
	{
#if defined(IP_PKTINFO)
		struct cmsghdr *cmsg;
		struct in_pktinfo *ipi;

		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if (cmsg->cmsg_level != IPPROTO_IP)
				continue;
			if (cmsg->cmsg_type != IP_PKTINFO)
				continue;
			ipi = (typeof(ipi)) CMSG_DATA(cmsg);
			*fromif = ipi->ipi_ifindex;
			break;
		}
#elif defined(IP_RECVIF)
		/* FIXME: there is a way to do this for BSD too... */
#endif
		break;
	}
	case AF_INET6:
		*fromif = ((struct sockaddr_in6 *) from)->sin6_scope_id;
		break;
	default:
		break;
	}
	return TRUE;
}

gboolean
ReceivePacket(GIOChannel *source, GIOCondition condition, gpointer data)
{
	XdmcpBuffer *buffer = (XdmcpBuffer *) data;
	XdmcpHeader header;
	ARRAY8 authenticationName = { 0, NULL };
	ARRAY8 hostname = { 0, NULL };
	ARRAY8 status = { 0, NULL };
	struct sockaddr_storage addr;
	int addrlen, sfd, ifindex;

	DPRINT();
	sfd = g_io_channel_unix_get_fd(source);
	addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	if (!XdmcpRecv(sfd, buffer, (XdmcpNetaddr) &addr, &addrlen, &ifindex)) {
		EPRINTF("could not fill buffer: %s\n", strerror(errno));
		return G_SOURCE_CONTINUE;
	}
	if (!XdmcpReadHeader(buffer, &header)) {
		EPRINTF("could not read header!\n");
		return G_SOURCE_CONTINUE;
	}
	if (header.version != XDM_PROTOCOL_VERSION) {
		EPRINTF("wrong header version!\n");
		return G_SOURCE_CONTINUE;
	}
	switch (header.opcode) {
	case WILLING:
		DPRINTF("host is WILLING\n");
		if (XdmcpReadARRAY8(buffer, &authenticationName)
		    && XdmcpReadARRAY8(buffer, &hostname)
		    && XdmcpReadARRAY8(buffer, &status)) {
			if (header.length == 6 + authenticationName.length +
			    hostname.length + status.length)
				AddHost((struct sockaddr *) &addr, addrlen, ifindex,
					header.opcode, &authenticationName, &hostname, &status);
			else
				EPRINTF("message is the wrong length\n");
		} else
			EPRINTF("could not parse message\n");
		break;
	case UNWILLING:
		DPRINTF("host is UNWILLING\n");
		if (XdmcpReadARRAY8(buffer, &hostname) && XdmcpReadARRAY8(buffer, &status)) {
			if (header.length == 4 + hostname.length + status.length)
				AddHost((struct sockaddr *) &addr, addrlen, ifindex,
					header.opcode, &authenticationName, &hostname, &status);
			else
				EPRINTF("message is the wrong length\n");
		} else
			EPRINTF("could not parse message\n");
		break;
	default:
		break;
	}
	XdmcpDisposeARRAY8(&authenticationName);
	XdmcpDisposeARRAY8(&hostname);
	XdmcpDisposeARRAY8(&status);
	return G_SOURCE_CONTINUE;
}

typedef struct _hostAddr {
	struct _hostAddr *next;
	struct sockaddr_storage addr;
	int addrlen;
	int sfd;
	xdmOpCode type;
} HostAddr;

HostAddr *hostAddrdb;
int pingTry = 0;
gint pingid = 0;

gboolean
PingHosts(gpointer data)
{
	HostAddr *ha;

	DPRINT();
	for (ha = hostAddrdb; ha; ha = ha->next) {
		int sfd;
		struct sockaddr *addr;
		sa_family_t family;
		char buf[INET6_ADDRSTRLEN];

		(void) buf;
		if (!(sfd = ha->sfd))
			continue;
		addr = (typeof(addr)) & ha->addr;
		family = addr->sa_family;
		switch (family) {
		case AF_INET:
			DPRINTF("ping address is AF_INET\n");
			break;
		case AF_INET6:
			DPRINTF("ping address is AF_INET6\n");
			break;
		}
		if (options.debug) {
			char *p;
			int i;

			DPRINTF("message is:");
			for (i = 0, p = (char *) &ha->addr; i < ha->addrlen; i++, p++)
				fprintf(stderr, " %02X", (unsigned int) *p);

		}
		if (ha->type == QUERY) {
			DPRINTF("ping type is QUERY\n");
			XdmcpFlush(ha->sfd, &directBuffer, (XdmcpNetaddr) &ha->addr, ha->addrlen);
		} else {
			DPRINTF("ping type is BROADCAST_QUERY\n");
			XdmcpFlush(ha->sfd, &broadcastBuffer,
				   (XdmcpNetaddr) &ha->addr, ha->addrlen);
		}
	}
	if (++pingTry < PING_TRIES) {
		DPRINTF("adding timer\n");
		pingid = g_timeout_add_seconds(PING_INTERVAL, PingHosts, (gpointer) NULL);
	}
	return G_SOURCE_REMOVE;
}

gint srce4, srce6;

static ARRAYofARRAY8 AuthenticationNames;

Bool
InitXDMCP(char *argv[], int argc)
{
	int sock4, sock6, value;
	GIOChannel *chan4, *chan6;
	XdmcpBuffer *buffer4, *buffer6;
	XdmcpHeader header;
	int i;
	char **arg;

	DPRINT();

	header.version = XDM_PROTOCOL_VERSION;
	header.opcode = (CARD16) BROADCAST_QUERY;
	header.length = 1;
	for (i = 0; i < (int) AuthenticationNames.length; i++)
		header.length += 2 + AuthenticationNames.data[i].length;
	XdmcpWriteHeader(&broadcastBuffer, &header);
	XdmcpWriteARRAYofARRAY8(&broadcastBuffer, &AuthenticationNames);

	header.version = XDM_PROTOCOL_VERSION;
	header.opcode = (CARD16) QUERY;
	header.length = 1;
	for (i = 0; i < (int) AuthenticationNames.length; i++)
		header.length += 2 + AuthenticationNames.data[i].length;
	XdmcpWriteHeader(&directBuffer, &header);
	XdmcpWriteARRAYofARRAY8(&directBuffer, &AuthenticationNames);

	if ((sock4 = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: Could not create IPv4 socket: %s\n", strerror(errno));
		return False;
	}
	value = 1;
	if (setsockopt(sock4, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		EPRINTF("setsockopt: Could not set IPv4 broadcast: %s\n", strerror(errno));
	}
#ifdef IP_PKTINFO
	if (setsockopt(sock4, IPPROTO_IP, IP_PKTINFO, &value, sizeof(value)) == -1) {
		EPRINTF("setsockopt: could not set IP_PKTINFO: %s\n", strerror(errno));
	}
#endif
	if ((sock6 = socket(PF_INET6, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: Could not create IPv6 socket: %s\n", strerror(errno));
	}
	if (setsockopt(sock6, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		EPRINTF("setsockopt: Could not set IPv6 broadcast: %s\n", strerror(errno));
	}
	chan4 = g_io_channel_unix_new(sock4);
	chan6 = g_io_channel_unix_new(sock6);

	buffer4 = calloc(1, sizeof(*buffer4));
	buffer6 = calloc(1, sizeof(*buffer6));

	srce4 = g_io_add_watch(chan4, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI,
			       ReceivePacket, (gpointer) buffer4);
	srce6 = g_io_add_watch(chan6, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI,
			       ReceivePacket, (gpointer) buffer6);

	for (i = 0, arg = argv; i < argc; i++, arg++) {
		if (!strcmp(*arg, "BROADCAST")) {
			struct ifaddrs *ifa, *ifas = NULL;
			HostAddr *ha;

			if (getifaddrs(&ifas) == 0) {
				for (ifa = ifas; ifa; ifa = ifa->ifa_next) {
					sa_family_t family;
					socklen_t addrlen;
					struct sockaddr *ifa_addr;
					struct sockaddr_in *sin;

					(void) index;
					if (ifa->ifa_flags & IFF_LOOPBACK) {
						DPRINTF("interface %s is a loopback interface\n",
							ifa->ifa_name);
						continue;
					}
					if (!(ifa_addr = ifa->ifa_addr)) {
						EPRINTF("interface %s has no address\n",
							ifa->ifa_name);
						continue;
					} else {
						if (ifa_addr->sa_family == AF_INET)
							DPRINTF("interface %s is AF_INET\n",
								ifa->ifa_name);
						else if (ifa_addr->sa_family == AF_INET6)
							DPRINTF("interface %s is AF_INET6\n",
								ifa->ifa_name);
						else if (ifa_addr->sa_family == AF_PACKET)
							DPRINTF("interface %s is AF_PACKET\n",
								ifa->ifa_name);
					}
					if (!(ifa->ifa_flags & IFF_BROADCAST)) {
						DPRINTF("interface %s has no broadcast\n",
							ifa->ifa_name);
						continue;
					}
					family = ifa_addr->sa_family;
					if (family == AF_INET)
						addrlen = sizeof(struct sockaddr_in);
					else {
						DPRINTF("interface %s has wrong family %d\n",
							ifa->ifa_name, (int) family);
						continue;
					}
					if (!(ifa_addr = ifa->ifa_broadaddr)) {
						EPRINTF("interface %s has missing broadcast\n",
							ifa->ifa_name);
						continue;
					}
					DPRINTF("interace %s is ok\n", ifa->ifa_name);
					ha = calloc(1, sizeof(*ha));
					memcpy(&ha->addr, ifa_addr, addrlen);
					sin = (typeof(sin)) & ha->addr;
					sin->sin_port = htons(XDM_UDP_PORT);
					ha->addrlen = addrlen;
					ha->sfd = sock4;
					ha->type = BROADCAST_QUERY;
					ha->next = hostAddrdb;
					hostAddrdb = ha;
				}
				freeifaddrs(ifas);
			}
		} else if (strspn(*arg, "0123456789abcdefABCDEF") == strlen(*arg)
			   && strlen(*arg) == 8) {
			char addr[4];
			char *p, *o, c, b;
			Bool ok = True;

			for (p = *arg, o = addr; *p; p += 2, o++) {
				c = tolower(p[0]);
				if (!isxdigit(c)) {
					ok = False;
					break;
				}
				b = ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
				b <<= 4;
				c = tolower(p[1]);
				if (!isxdigit(c)) {
					ok = False;
					break;
				}
				b += ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
				*o = b;
			}
			if (ok) {
				HostAddr *ha;
				struct sockaddr_in *sin;

				ha = calloc(1, sizeof(*ha));
				sin = (typeof(sin)) & ha->addr;
				sin->sin_family = AF_INET;
				sin->sin_port = XDM_UDP_PORT;
				memcpy(&sin->sin_addr, addr, 4);
				ha->addrlen = sizeof(*sin);
				ha->sfd = sock4;
				ha->type = QUERY;
				ha->next = hostAddrdb;
				hostAddrdb = ha;
			}
		} else {
			struct addrinfo hints, *result, *ai;

			hints.ai_flags = AI_ADDRCONFIG;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			hints.ai_addrlen = 0;
			hints.ai_addr = NULL;
			hints.ai_canonname = NULL;
			hints.ai_next = NULL;

			if (getaddrinfo(*arg, "xdmcp", &hints, &result) == 0) {
				HostAddr *ha;

				for (ai = result; ai; ai = ai->ai_next) {
					if (ai->ai_family == AF_INET) {
						struct sockaddr_in *sin = (typeof(sin)) ai->ai_addr;

						ha = calloc(1, sizeof(*ha));
						memcpy(&ha->addr, ai->ai_addr, ai->ai_addrlen);
						ha->addrlen = ai->ai_addrlen;
						ha->sfd = sock4;
						ha->type =
						    IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) ?
						    BROADCAST_QUERY : QUERY;
						ha->next = hostAddrdb;
						hostAddrdb = ha;
					} else if (ai->ai_family == AF_INET6) {
						struct sockaddr_in6 *sin6 =
						    (typeof(sin6)) ai->ai_addr;

						ha = calloc(1, sizeof(*ha));
						memcpy(&ha->addr, ai->ai_addr, ai->ai_addrlen);
						ha->addrlen = ai->ai_addrlen;
						ha->sfd = sock6;
						ha->type = IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) ?
						    BROADCAST_QUERY : QUERY;
						ha->next = hostAddrdb;
						hostAddrdb = ha;
					}
				}
				freeaddrinfo(result);
			}
		}
	}
	pingTry = 0;
	PingHosts((gpointer) NULL);
	return True;
}

static gint timer_id;

gboolean
on_msg_timeout(gpointer data)
{
	gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_NONE);
	timer_id = 0;
	return G_SOURCE_REMOVE;
}

void
Choose(short connectionType, char *name, struct sockaddr *sa, int scope, int ifindex)
{
	CARD8 rawaddr[20] = { 0, };
	ARRAY8 hostAddress = { 0, rawaddr };

	switch (sa->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin = (typeof(sin)) sa;

		memmove(rawaddr, &sin->sin_addr, 4);

		switch (scope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			memmove(rawaddr + 4, &ifindex, 4);
			hostAddress.length = 8;
			break;
		default:
			hostAddress.length = 4;
			break;
		}
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6 = (typeof(sin6)) sa;
		uint32_t scope_id = sin6->sin6_scope_id;

		memmove(rawaddr, &sin6->sin6_addr, 16);

		switch (scope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			memmove(rawaddr + 16, &scope_id, 4);
			hostAddress.length = 20;
			break;
		default:
			hostAddress.length = 16;
			break;
		}
		break;
	}
	default:
	case AF_UNIX:
		break;
	}

	if (options.xdmAddress.data) {
		char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };
		struct sockaddr_storage xdmAddr;
		struct sockaddr *xa;
		socklen_t xalen;
		char buf[1024];
		XdmcpBuffer buffer;
		char *xdm;
		int fd;

		/* 
		 * Connect to XDM and output result
		 */
		memset(&xdmAddr, 0, sizeof(xdmAddr));
		xa = (typeof(xa)) & xdmAddr;
		xdm = (char *) options.xdmAddress.data;
		xa->sa_family = ((int) xdm[0] << 8) + xdm[1];
		switch (xa->sa_family) {
		case AF_INET:
		{
			struct sockaddr_in *sin;

			if (options.xdmAddress.length != 2 + 2 + 4) {
				EPRINTF("Bad xdm address length %d\n",
					(int) options.xdmAddress.length);
				return;
			}
			sin = (typeof(sin)) xa;
			memmove(&sin->sin_port, xdm + 2, 2);
			memmove(&sin->sin_addr, xdm + 4, 4);
			inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
			DPRINTF("AF_INET: %s port %hd\n", ipaddr, ntohs(sin->sin_port));
			xalen = sizeof(*sin);
			break;
		}
		case AF_INET6:
		{
			struct sockaddr_in6 *sin6;

			if (options.xdmAddress.length != 2 + 2 + 16 &&
			    options.xdmAddress.length != 2 + 2 + 20) {
				EPRINTF("Bad xdm address length %d\n",
					(int) options.xdmAddress.length);
				return;
			}
			sin6 = (typeof(sin6)) xa;
			memmove(&sin6->sin6_port, xdm + 2, 2);
			memmove(&sin6->sin6_addr, xdm + 4, 16);
			if (options.xdmAddress.length == 2 + 2 + 20)
				sin6->sin6_scope_id = ntohl(*(uint32_t *) (xdm + 20));
			inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
			DPRINTF("AF_INET6: %s port %hd\n", ipaddr, ntohs(sin6->sin6_port));
			xalen = sizeof(*sin6);
			break;
		}
		case AF_UNIX:
		default:
			return;
		}
		if ((fd = socket(xa->sa_family, SOCK_STREAM, 0)) == -1) {
			EPRINTF("Cannot create response socket: %s\n", strerror(errno));
			exit(REMANAGE_DISPLAY);
		}
		if (connect(fd, xa, xalen) == -1) {
			EPRINTF("Cannot connect to xdm: %s\n", strerror(errno));
			exit(REMANAGE_DISPLAY);
		}
		buffer.data = (BYTE *) buf;
		buffer.size = sizeof(buf);
		buffer.pointer = 0;
		buffer.count = 0;
		XdmcpWriteARRAY8(&buffer, &options.clientAddress);
		XdmcpWriteCARD16(&buffer, connectionType);
		XdmcpWriteARRAY8(&buffer, &hostAddress);
		if (write(fd, (char *) buffer.data, buffer.pointer)) ;
		close(fd);
	}
	if (!options.xdmAddress.data || options.debug) {
		int i, len;
		CARD8Ptr b, buf;
		FILE *where = options.xdmAddress.data ? stderr : stdout;

		if (options.xdmAddress.data)
			DPRINTF("choice: ");
		len = options.clientAddress.length;
		buf = options.clientAddress.data;
		for (i = 0, b = buf; i < len; i++, b++)
			fprintf(where, "%02x", (unsigned) *b);
		fprintf(where, " %d ", (int) connectionType);
		len = hostAddress.length;
		buf = hostAddress.data;
		for (i = 0, b = buf; i < len; i++, b++)
			fprintf(where, "%02x", (unsigned) *b);
		fprintf(where, "\n");
	}
	exit(OBEYSESS_DISPLAY);
}

void grabbed_window(GtkWidget *window, gpointer user_data);

void
DoAccept(GtkButton *button, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	GValue ctype = G_VALUE_INIT;
	GValue ipaddr = G_VALUE_INIT;
	GValue willing = G_VALUE_INIT;
	GValue saddr = G_VALUE_INIT;
	GValue scope = G_VALUE_INIT;
	GValue index = G_VALUE_INIT;
	gint willingness, connectionType, sockScope, ifindex;
	gchar *ipAddress;
	struct sockaddr *sockAddress;

	DPRINT();
	if (!view)
		return;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n",
								    "No selection!",
								    "Click on a list entry to make a selection.");

		timer_id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_widget_realize(msg);
		grabbed_window(msg, NULL);
		gtk_dialog_run(GTK_DIALOG(msg));
		g_object_unref(G_OBJECT(msg));
		grabbed_window(top, NULL);
		if (timer_id)
			g_source_remove(timer_id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, XDM_COL_WILLING, &willing);
	willingness = g_value_get_int(&willing);
	g_value_unset(&willing);

	if (willingness != WILLING) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%d != %d\n%s\n",
								    "Host is not willing!",
								    willingness,
								    (int) WILLING,
								    "Please select another host.");

		timer_id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_widget_realize(msg);
		grabbed_window(msg, NULL);
		gtk_dialog_run(GTK_DIALOG(msg));
		g_object_unref(G_OBJECT(msg));
		grabbed_window(top, NULL);
		if (timer_id)
			g_source_remove(timer_id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, XDM_COL_CTYPE, &ctype);
	connectionType = g_value_get_int(&ctype);
	g_value_unset(&ctype);
	if (!options.isLocal && options.connectionType != FamilyInternet6
	    && connectionType != options.connectionType) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n",
								    "Host has wrong connection type!",
								    "Please select another host.");

		timer_id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_widget_realize(msg);
		grabbed_window(msg, NULL);
		gtk_dialog_run(GTK_DIALOG(msg));
		g_object_unref(G_OBJECT(msg));
		grabbed_window(top, NULL);
		if (timer_id)
			g_source_remove(timer_id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, XDM_COL_IPADDR, &ipaddr);
	ipAddress = g_value_dup_string(&ipaddr);
	g_value_unset(&ipaddr);

	gtk_tree_model_get_value(model, &iter, XDM_COL_SOCKADDR, &saddr);
	sockAddress = g_value_dup_boxed(&saddr);
	g_value_unset(&saddr);

	gtk_tree_model_get_value(model, &iter, XDM_COL_SCOPE, &scope);
	sockScope = g_value_get_int(&scope);
	g_value_unset(&scope);

	gtk_tree_model_get_value(model, &iter, XDM_COL_IFINDEX, &index);
	ifindex = g_value_get_int(&index);
	g_value_unset(&index);

	Choose(connectionType, ipAddress, sockAddress, sockScope, ifindex);
}

typedef struct {
	int willing;
} PingHost;

Bool
DoCheckWilling(PingHost *host)
{
	return (host->willing == WILLING);
}

void
DoPing(GtkButton *button, gpointer data)
{
	if (pingTry == PING_TRIES) {
		pingTry = 0;
		PingHosts(data);
	}
}

static void
on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer
		 user_data)
{
}
#endif				/* DO_XCHOOSER */

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

static int
xde_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *r, *rarray;
	const struct pam_message **m;
	int i;

	if (num_msg <= 0)
		return PAM_SUCCESS;
	if (!(rarray = calloc(num_msg, sizeof(*rarray))))
		return PAM_BUF_ERR;

	for (i = 0, m = msg, r = rarray; i < num_msg; i++, msg++, r++) {
		if (!*m) {
			r->resp_retcode = 0;
			continue;
		}
		switch ((*m)->msg_style) {
		case PAM_PROMPT_ECHO_ON:	/* obtain a string whilst
						   echoing */
		{
			DPRINTF("PAM_PROMPT_ECHO_ON: %s\n", (*m)->msg);
			gtk_label_set_text(GTK_LABEL(l_uname), (*m)->msg);
			gtk_entry_set_text(GTK_ENTRY(user), "");
			gtk_entry_set_text(GTK_ENTRY(pass), "");
			gtk_label_set_text(GTK_LABEL(l_lstat), "");
			gtk_widget_set_sensitive(user, TRUE);
			gtk_widget_set_sensitive(pass, FALSE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
			DPRINTF("running main loop...\n");
			gtk_main();
			DPRINTF("...running main loop\n");
			if (login_result == LoginResultLogout)
				return (PAM_CONV_ERR);
			r->resp = strdup(gtk_entry_get_text(GTK_ENTRY(user)));
			DPRINTF("Response is: %s\n", r->resp);
			break;
		}
		case PAM_PROMPT_ECHO_OFF:	/* obtain a string without
						   echoing */
		{
			DPRINTF("PAM_PROMPT_ECHO_OFF: %s\n", (*m)->msg);
			gtk_label_set_text(GTK_LABEL(l_pword), (*m)->msg);
			gtk_entry_set_text(GTK_ENTRY(pass), "");
			gtk_label_set_text(GTK_LABEL(l_lstat), "");
			gtk_widget_set_sensitive(user, FALSE);
			gtk_widget_set_sensitive(pass, TRUE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
			DPRINTF("running main loop...\n");
			gtk_main();
			DPRINTF("...running main loop\n");
			if (login_result == LoginResultLogout)
				return (PAM_CONV_ERR);
			r->resp = strdup(gtk_entry_get_text(GTK_ENTRY(pass)));
			// DPRINTF("Response is: %s\n", r->resp);
			break;
		}
		case PAM_ERROR_MSG:	/* display an error message */
		{
			DPRINTF("PAM_ERROR_MSG: %s\n", (*m)->msg);
			gtk_label_set_text(GTK_LABEL(l_lstat), (*m)->msg);
			relax();
			break;
		}
		case PAM_TEXT_INFO:	/* display some text */
		{
			DPRINTF("PAM_TEXT_INFO: %s\n", (*m)->msg);
			gtk_label_set_text(GTK_LABEL(l_lstat), (*m)->msg);
			relax();
			break;
		}
		}
	}
	*resp = rarray;
	return PAM_SUCCESS;
}

struct pam_conv xde_pam_conv = {
	.conv = xde_conv,
	.appdata_ptr = NULL,
};

static void
on_poweroff(GtkMenuItem *item, gpointer data)
{
	gchar *status = data;
	gboolean challenge;

	if (!status)
		return;
	if (!strcmp(status, "yes")) {
		challenge = FALSE;
	} else if (!strcmp(status, "challenge")) {
		challenge = TRUE;
	} else
		return;
	if (challenge) {
	}
}

static void
on_reboot(GtkMenuItem *item, gpointer data)
{
	gchar *status = data;
	gboolean challenge;

	if (!status)
		return;
	if (!strcmp(status, "yes")) {
		challenge = FALSE;
	} else if (!strcmp(status, "challenge")) {
		challenge = TRUE;
	} else
		return;
	if (challenge) {
	}
}

static void
on_suspend(GtkMenuItem *item, gpointer data)
{
	gchar *status = data;
	gboolean challenge;

	if (!status)
		return;
	if (!strcmp(status, "yes")) {
		challenge = FALSE;
	} else if (!strcmp(status, "challenge")) {
		challenge = TRUE;
	} else
		return;
	if (challenge) {
	}
}

static void
on_hibernate(GtkMenuItem *item, gpointer data)
{
	gchar *status = data;
	gboolean challenge;

	if (!status)
		return;
	if (!strcmp(status, "yes")) {
		challenge = FALSE;
	} else if (!strcmp(status, "challenge")) {
		challenge = TRUE;
	} else
		return;
	if (challenge) {
	}
}

static void
on_hybridsleep(GtkMenuItem *item, gpointer data)
{
	gchar *status = data;
	gboolean challenge;

	if (!status)
		return;
	if (!strcmp(status, "yes")) {
		challenge = FALSE;
	} else if (!strcmp(status, "challenge")) {
		challenge = TRUE;
	} else
		return;
	if (challenge) {
	}
}

static void
free_value(gpointer data, GClosure *unused)
{
	if (data)
		g_free(data);
}

/** @brief test is the user is remote or local
  */
Bool
test_remote_user(void)
{
	return True;
}

gboolean
append_power_action(GtkWidget *submenu, Bool islocal, const char *name, const char *label,
		    const char *icon, GCallback callback)
{
	GError *err = NULL;
	gchar *value = NULL;
	GtkWidget *imag, *item;
	gboolean gotone = FALSE;
	GVariant *result;
	GVariantIter iter;
	GVariant *var;

	result = g_dbus_proxy_call_sync(sd_manager, name,
					NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
	if (result && !err) {
		g_variant_iter_init(&iter, result);
		var = g_variant_iter_next_value(&iter);
		value = g_variant_dup_string(var, NULL);
		DPRINTF("%s status is %s\n", name, value);
		item = gtk_image_menu_item_new_with_label(label);
		imag = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				      callback, (gpointer) value, free_value, G_CONNECT_AFTER);
		if (islocal && (!strcmp(value, "yes") || !strcmp(value, "challenge"))) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
		g_variant_unref(var);
		g_variant_unref(result);
	} else {
		EPRINTF("%s call failed: %s\n", name, err ? err->message : NULL);
		g_clear_error(&err);
	}
	return gotone;

}

/** @brief add a power actions submenu to the actions menu
  *
  * We can provide power management actions to the user on the following
  * provisos:
  *
  * 1. the user is local and not remote
  * 2. the session is associated with a virtual terminal
  * 3. the DISPLAY starts with ':'.
  * 4. the X Display has no VNC-EXTENSION extension (otherwise it could in fact
  *    be remote)
  *
  * When these conditions are satisfied, the user is sitting at a physical seat
  * of the computer and could have access to the power buttons anyway.
  */
static void
append_power_actions(GtkMenu *menu)
{
	GtkWidget *submenu, *power;
	gboolean gotone = FALSE;
	Bool islocal;

	if (!menu)
		return;
	if (!sd_manager) {
		EPRINTF("no manager DBUS proxy!\n");
		return;
	}

	islocal = isLocal();

	power = gtk_image_menu_item_new_with_label(GTK_STOCK_EXECUTE);
	gtk_image_menu_item_set_use_stock(GTK_IMAGE_MENU_ITEM(power), TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), power);

	submenu = gtk_menu_new();

	if (append_power_action(submenu, islocal, "CanPowerOff", "Power Off",
				"system-shutdown", G_CALLBACK(on_poweroff)))
		gotone = TRUE;
	if (append_power_action(submenu, islocal, "CanReboot", "Reboot",
				"system-reboot", G_CALLBACK(on_reboot)))
		gotone = TRUE;
	if (append_power_action(submenu, islocal, "CanSuspend", "Suspend",
				"system-suspend", G_CALLBACK(on_suspend)))
		gotone = TRUE;
	if (append_power_action(submenu, islocal, "CanHibernate", "Hibernate",
				"system-suspend-hibernate", G_CALLBACK(on_hibernate)))
		gotone = TRUE;
	if (append_power_action(submenu, islocal, "CanHybridSleep", "Hybrid Sleep",
				"system-sleep", G_CALLBACK(on_hybridsleep)))
		gotone = TRUE;

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(power), submenu);
	if (gotone)
		gtk_widget_show_all(power);
}

/** @brief add a session tasks submenu to the actions menu
  *
  * We do not really want to provide these to the casual locked-out user, it
  * should only be provided when the user is logged in and the screen is
  * unlocked.
  */
static void
append_session_tasks(GtkMenu *menu)
{
	const char *env;

	if (!(env = getenv("SESSION_MANAGER")))
		return;
}

static void
on_switch_session(GtkMenuItem *item, gpointer data)
{
	gchar *session = data;
	GError *err = NULL;

	GVariant *result;

	if (!sd_manager) {
		EPRINTF("no session DBUS proxy\n");
		return;
	}
	if (!
	    (result =
	     g_dbus_proxy_call_sync(sd_manager, "ActivateSession",
				    g_variant_new("(s)", session), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
				    &err)) || err) {
		EPRINTF("ActivateSession: %s: call failed: %s\n", session,
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_variant_unref(result);
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

/** @brief add a switch users submenu to the actions menu
  */
static void
append_switch_users(GtkMenu *menu)
{
	const char *seat, *sess;
	char **sessions = NULL, **s;
	GtkWidget *submenu, *jumpto;
	gboolean gotone = FALSE;
	Bool islocal;
	int count;

	if (!menu)
		return;

	islocal = isLocal();

	jumpto = gtk_image_menu_item_new_with_label(GTK_STOCK_JUMP_TO);
	gtk_image_menu_item_set_use_stock(GTK_IMAGE_MENU_ITEM(jumpto), TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), jumpto);

	submenu = gtk_menu_new();

	seat = getenv("XDG_SEAT") ? : "seat0";
	sess = getenv("XDG_SESSION_ID");
	if (sd_seat_can_multi_session(NULL) <= 0) {
		DPRINTF("%s: cannot multi-session\n", seat);
		return;
	}
	if (sd_seat_get_sessions(seat, &sessions, NULL, NULL) < 0) {
		EPRINTF("%s: cannot get sessions\n", seat);
		return;
	}
	/* sort by VT number */
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
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
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
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(jumpto), submenu);
	if (gotone)
		gtk_widget_show_all(jumpto);
}

static GtkMenu *
create_action_menu(void)
{
	GtkWidget *menu;

	menu = gtk_menu_new();
	if (menu) {
		append_power_actions(GTK_MENU(menu));
		append_session_tasks(GTK_MENU(menu));
		append_switch_users(GTK_MENU(menu));
		return GTK_MENU(menu);
	}
	return NULL;
}

static void
at_pointer(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user)
{
	GdkDisplay *disp = gdk_display_get_default();

	gdk_display_get_pointer(disp, NULL, x, y, NULL);
	*push_in = TRUE;
}

void grabbed_window(GtkWidget *window, gpointer user_data);

static void
on_menu_popdown(GtkMenuShell *shell, gpointer data)
{
	GtkWidget *window = data;

	relax();
	grabbed_window(window, NULL);
}

gulong grab_broken_handler;

static void
on_action_clicked(GtkButton *button, gpointer user_data)
{
	GtkMenu *menu;
	GtkWidget *window;

	if (!(menu = create_action_menu())) {
		DPRINTF("No actions to perform\n");
		return;
	}
	window = gtk_widget_get_toplevel(GTK_WIDGET(button));
	if (grab_broken_handler) {
		g_signal_handler_disconnect(G_OBJECT(window), grab_broken_handler);
		grab_broken_handler = 0;
	}
	g_signal_connect(G_OBJECT(menu), "deactivate", G_CALLBACK(on_menu_popdown), window);
	gtk_menu_popup(menu, NULL, NULL, at_pointer, NULL, 1, GDK_CURRENT_TIME);
	return;
}

static void
on_login_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;

	switch (state) {
	case LoginStateInit:
		gtk_widget_grab_default(user);
		gtk_widget_grab_focus(user);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		break;
	case LoginStateUsername:
		gtk_widget_grab_default(pass);
		gtk_widget_grab_focus(pass);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		break;
	case LoginStatePassword:
	case LoginStateReady:
		login_result = LoginResultLaunch;
		gtk_widget_set_sensitive(buttons[3], FALSE);
		gtk_main_quit();
		break;
	}
}

static void
on_logout_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;

	(void) buttons;

	login_result = LoginResultLogout;
	gtk_main_quit();
#ifndef DO_XLOCKING
	exit(OBEYSESS_DISPLAY);
#endif
}

static void
on_user_activate(GtkEntry *user, gpointer data)
{
	const gchar *username;

	free(options.username);
	options.username = NULL;
	free(options.password);
	options.password = NULL;
	gtk_entry_set_text(GTK_ENTRY(pass), "");

	if ((username = gtk_entry_get_text(user)) && username[0]) {
		DPRINTF("Username is: %s\n", username);
		options.username = strdup(username);
		state = LoginStateUsername;
		login_result = LoginResultLaunch;
		gtk_main_quit();
		// gtk_widget_set_sensitive(buttons[3], FALSE);
		// gtk_widget_set_sensitive(GTK_WIDGET(pass), TRUE);
		// gtk_widget_grab_default(GTK_WIDGET(pass));
		// gtk_widget_grab_focus(GTK_WIDGET(pass));
		// gtk_widget_set_sensitive(GTK_WIDGET(user), FALSE);
	} else
		EPRINTF("No user name!\n");
}

static void
on_pass_activate(GtkEntry *pass, gpointer data)
{
	const gchar *password;

	free(options.password);
	options.password = NULL;

	if ((password = gtk_entry_get_text(pass))) {
		DPRINTF("Got password %d chars\n", (int) strlen(password));
		options.password = strdup(password);
		state = LoginStatePassword;
		login_result = LoginResultLaunch;
		gtk_main_quit();
		/* FIXME: do the authorization */
		// gtk_widget_set_sensitive(buttons[3], TRUE);
		// gtk_widget_grab_default(buttons[3]);
		// gtk_widget_grab_focus(buttons[3]);
		// gtk_widget_set_sensitive(GTK_WIDGET(user), FALSE);
		// gtk_widget_set_sensitive(GTK_WIDGET(pass), FALSE);
	} else
		EPRINTF("No pass word!\n");
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	login_result = LoginResultLogout;
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
	DPRINTF("Grab broken!\n");
	DPRINTF("Grab broken on %s\n", ev->keyboard ? "keyboard" : "pointer");
	DPRINTF("Grab broken %s\n", ev->implicit ? "implicit" : "explicit");
	DPRINTF("Grab broken by %s\n", ev->grab_window ? "this application" : "other");
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
	if (!grab_broken_handler)
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

#ifdef DO_XLOCKING
static Window
get_selection(Window selwin, char *selection, int s)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom atom;
	Window owner;

	snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
	atom = XInternAtom(dpy, selection, False);
	if (!(owner = XGetSelectionOwner(dpy, atom)))
		DPRINTF("No owner for %s\n", selection);
	if ((owner && options.replace) || (!owner && selwin)) {
		DPRINTF("Setting owner of %s to 0x%lx from 0x%lx\n", selection, selwin, owner);
		XSetSelectionOwner(dpy, atom, selwin, CurrentTime);
		XSync(dpy, False);
	}
	if (options.replace && selwin) {
		XEvent ev;
		Atom manager = XInternAtom(dpy, "MANAGER", False);
		GdkScreen *scrn = gdk_display_get_screen(disp, s);
		GdkWindow *root = gdk_screen_get_root_window(scrn);
		Window r = GDK_WINDOW_XID(root);
		Atom atom = XInternAtom(dpy, selection, False);

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = r;
		ev.xclient.message_type = manager;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = CurrentTime;
		ev.xclient.data.l[1] = atom;
		ev.xclient.data.l[2] = selwin;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, r, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	return (owner);
}
#endif				/* DO_XLOCKING */

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

static void
on_combo_popdown(GtkComboBox *combo, gpointer data)
{
	GtkWidget *window;

	window = gtk_widget_get_toplevel(GTK_WIDGET(combo));

	relax();
	grabbed_window(window, NULL);
}

GtkWidget *
GetPanel(void)
{
	GtkRcStyle *style;
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

	GtkWidget *align = gtk_alignment_new(0.5, 0.5, 1.0, 0.0);

	gtk_container_add(GTK_CONTAINER(inp), align);

	int rows = 3;

	if (options.xsession)
		rows = 4;

	GtkWidget *login = gtk_table_new(rows, 3, TRUE);

	gtk_container_set_border_width(GTK_CONTAINER(login), 5);
	gtk_table_set_col_spacings(GTK_TABLE(login), 5);
	gtk_table_set_row_spacings(GTK_TABLE(login), 5);
	gtk_table_set_col_spacing(GTK_TABLE(login), 0, 0);
	gtk_container_add(GTK_CONTAINER(align), login);

	l_uname = gtk_label_new(NULL);
	gtk_label_set_text(GTK_LABEL(l_uname), resources.namePrompt ? : "Username: ");
	gtk_misc_set_alignment(GTK_MISC(l_uname), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_uname), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), l_uname, 0, 1, 0, 1);
	if ((style = gtk_widget_get_modifier_style(l_uname))) {
		style->font_desc = pango_font_description_copy(resources.promptFace);
		if (options.transparent) {
			if (resources.promptColor) {
				int i;

				for (i = 0; i < 5; i++) {
					style->text[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_TEXT;
					style->fg[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_FG;
					// style->base[i] = *resources.promptColor;
					// style->color_flags[i] |= GTK_RC_BASE;
				}
			} else
				DPRINTF("No resources.promptColor!\n");
		}
		gtk_widget_modify_style(l_uname, style);
	}

	user = gtk_entry_new();
	// gtk_entry_set_width_chars(GTK_ENTRY(user), 12);
	gtk_entry_set_visibility(GTK_ENTRY(user), TRUE);
	gtk_widget_set_can_default(user, TRUE);
	gtk_widget_set_can_focus(user, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), user, 1, 2, 0, 1);
	if ((style = gtk_widget_get_modifier_style(user))) {
		style->font_desc = pango_font_description_copy(resources.inputFace);
		gtk_widget_modify_style(user, style);
	}

	l_pword = gtk_label_new(NULL);
	gtk_label_set_text(GTK_LABEL(l_pword), resources.passwdPrompt ? : "Password: ");
	gtk_misc_set_alignment(GTK_MISC(l_pword), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_pword), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), l_pword, 0, 1, 1, 2);
	if ((style = gtk_widget_get_modifier_style(l_pword))) {
		style->font_desc = pango_font_description_copy(resources.promptFace);
		if (options.transparent) {
			if (resources.promptColor) {
				int i;

				for (i = 0; i < 5; i++) {
					style->text[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_TEXT;
					style->fg[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_FG;
					// style->base[i] = *resources.promptColor;
					// style->color_flags[i] |= GTK_RC_BASE;
				}
			} else
				DPRINTF("No resources.promptColor!\n");
		}
		gtk_widget_modify_style(l_pword, style);
	}

	pass = gtk_entry_new();
	// gtk_entry_set_width_chars(GTK_ENTRY(pass), 12);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_set_can_default(pass, TRUE);
	gtk_widget_set_can_focus(pass, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), pass, 1, 2, 1, 2);
	if ((style = gtk_widget_get_modifier_style(pass))) {
		style->font_desc = pango_font_description_copy(resources.inputFace);
		gtk_widget_modify_style(pass, style);
	}

	l_lstat = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(l_lstat), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_lstat), 5, 2);
	if (options.xsession)
		gtk_table_attach_defaults(GTK_TABLE(login), l_lstat, 0, 3, 3, 4);
	else
		gtk_table_attach_defaults(GTK_TABLE(login), l_lstat, 0, 3, 2, 3);
	if ((style = gtk_widget_get_modifier_style(l_lstat))) {
		style->font_desc = pango_font_description_copy(resources.failFace);
		if (resources.failColor) {
			int i;

			for (i = 0; i < 5; i++) {
				style->text[i] = *resources.failColor;
				style->color_flags[i] |= GTK_RC_TEXT;
				style->fg[i] = *resources.failColor;
				style->color_flags[i] |= GTK_RC_FG;
				// style->base[i] = *resources.failColor;
				// style->color_flags[i] |= GTK_RC_BASE;
			}
		} else
			DPRINTF("No resources.failColor!\n");
		gtk_widget_modify_style(l_lstat, style);
	}

	g_signal_connect(G_OBJECT(user), "activate", G_CALLBACK(on_user_activate), pass);
	g_signal_connect(G_OBJECT(pass), "activate", G_CALLBACK(on_pass_activate), user);

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end(GTK_BOX(pan), bb, FALSE, FALSE, 0);

	GtkWidget *i;
	GtkWidget *b;

#ifdef DO_XCHOOSER
	buttons[1] = b = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(DoPing), NULL);
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
#endif

	if ((getenv("DISPLAY") ? : "")[0] == ':') {
		b = gtk_button_new_from_stock(GTK_STOCK_QUIT);
	} else {
		b = gtk_button_new_from_stock(GTK_STOCK_DISCONNECT);
	}
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_logout_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);
	buttons[0] = b;

	buttons[4] = b = gtk_button_new();
	gtk_widget_set_can_default(b, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-execute", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Actions");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_action_clicked), buttons);
	if ((getenv("DISPLAY") ? : "")[0] == ':')
		gtk_widget_set_sensitive(b, TRUE);
	else
		gtk_widget_set_sensitive(b, FALSE);

#ifdef DO_XCHOOSER
	buttons[2] = b = gtk_button_new_from_stock(GTK_STOCK_CONNECT);
	gtk_widget_set_can_default(b, TRUE);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(DoAccept), NULL);
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
#endif

	buttons[3] = b = gtk_button_new();
	gtk_widget_set_can_default(b, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-ok", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Login");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_login_clicked), buttons);
	gtk_widget_set_sensitive(b, FALSE);

	if (options.xsession) {
	GtkWidget *xsess = gtk_label_new(NULL);

	gtk_label_set_text(GTK_LABEL(xsess), "XSession:  ");
	gtk_misc_set_alignment(GTK_MISC(xsess), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(xsess), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), xsess, 0, 1, 2, 3);
	if ((style = gtk_widget_get_modifier_style(xsess))) {
		style->font_desc = pango_font_description_copy(resources.promptFace);
		if (options.transparent) {
			if (resources.promptColor) {
				int i;

				for (i = 0; i < 5; i++) {
					style->text[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_TEXT;
					style->fg[i] = *resources.promptColor;
					style->color_flags[i] |= GTK_RC_FG;
					// style->base[i] = *resources.promptColor;
					// style->color_flags[i] |= GTK_RC_BASE;
				}
			} else
				DPRINTF("No resources.promptColor!\n");
		}
		gtk_widget_modify_style(xsess, style);
	}

	/* *INDENT-OFF* */
	store = gtk_list_store_new(9
			,G_TYPE_STRING	/* icon */
			,G_TYPE_STRING	/* Name */
			,G_TYPE_STRING	/* Comment */
			,G_TYPE_STRING	/* Name and Comment Markup */
			,G_TYPE_STRING  /* Label */
			,G_TYPE_BOOLEAN	/* SessionManaged?  XDE-Managed?  */
			,G_TYPE_BOOLEAN /* X-XDE-managed original setting */
			,G_TYPE_STRING	/* the file name */
			,G_TYPE_STRING	/* tooltip */
		);
	/* *INDENT-ON* */

	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(store),
						xsession_compare_function, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					     GTK_SORT_ASCENDING);
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);
	/* *INDENT-OFF* */
	gtk_list_store_set(store, &iter,
			   XSESS_COL_PIXBUF, "",
			   XSESS_COL_NAME, "(Default)",
			   XSESS_COL_COMMENT, "",
			   XSESS_COL_MARKUP, "",
			   XSESS_COL_LABEL, "default",
			   XSESS_COL_MANAGED, FALSE,
			   XSESS_COL_ORIGINAL, FALSE,
			   XSESS_COL_FILENAME, "",
			   XSESS_COL_TOOLTIP, "",
			   -1);
	/* *INDENT-ON* */

	sess = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

	GtkCellRenderer *rend;

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(rend), 0, 0);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(sess), GTK_CELL_RENDERER(rend), FALSE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(sess),
					   GTK_CELL_RENDERER(rend), on_render_pixbuf, NULL, NULL);

	rend = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(rend), 5, 0);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(sess), GTK_CELL_RENDERER(rend), FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(sess), GTK_CELL_RENDERER(rend), "text",
				      XSESS_COL_NAME);

	gtk_table_attach_defaults(GTK_TABLE(login), sess, 1, 2, 2, 3);

	g_signal_connect(G_OBJECT(sess), "popdown", G_CALLBACK(on_combo_popdown), NULL);
	}

#ifdef DO_XCHOOSER
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), shadow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(pan), sw, TRUE, TRUE, 4);

	/* *INDENT-OFF* */
	model = gtk_list_store_new(13
			,G_TYPE_STRING		/* hostname */
			,G_TYPE_STRING		/* remotename */
			,G_TYPE_INT		/* willing */
			,G_TYPE_STRING		/* status */
			,G_TYPE_STRING		/* IP Address */
			,G_TYPE_INT		/* connection type */
			,G_TYPE_STRING		/* service */
			,G_TYPE_INT		/* port */
			,G_TYPE_STRING		/* markup */
			,G_TYPE_STRING		/* tooltip */
			,G_TYPE_SOCKADDR	/* socket address */
			,G_TYPE_INT		/* scope */
			,G_TYPE_INT		/* interface index */
	    );
	/* *INDENT-ON* */

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
					     XDM_COL_MARKUP, GTK_SORT_ASCENDING);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), XDM_COL_HOSTNAME);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), XDM_COL_TOOLTIP);

	gtk_container_add(GTK_CONTAINER(sw), view);

	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	int len = strlen("XDCMP Host Menu from ") + strlen(hostname) + 1;
	char *title = calloc(len, sizeof(*title));

	strncpy(title, "XDCMP Host Menu from ", len);
	strncat(title, hostname, len);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(title, renderer,
									     "markup",
									     XDM_COL_MARKUP, NULL);

	free(title);
	gtk_tree_view_column_set_sort_column_id(column, XDM_COL_HOSTNAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(column));
	g_signal_connect(G_OBJECT(view), "row_activated",
			 G_CALLBACK(on_row_activated), (gpointer) NULL);

#endif				/* DO_XCHOOSER */

	if (options.xsession) {
#ifdef DO_ONIDLE
		g_idle_add(on_idle, store);
#else
		while (on_idle(store) != G_SOURCE_REMOVE) ;
#endif
	}

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

	l_greet = gtk_label_new(NULL);
	gtk_label_set_text(GTK_LABEL(l_greet), options.welcome);
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
		box = gtk_vbox_new(FALSE, 5);
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

#if 1
	if (options.username) {
		gtk_entry_set_text(GTK_ENTRY(user), options.username);
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_widget_set_sensitive(user, FALSE);
		gtk_widget_set_sensitive(pass, TRUE);
		gtk_widget_set_sensitive(buttons[0], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		if (!noshow) {
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
		}
	} else {
		gtk_entry_set_text(GTK_ENTRY(user), "");
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_widget_set_sensitive(user, TRUE);
		gtk_widget_set_sensitive(pass, FALSE);
		gtk_widget_set_sensitive(buttons[0], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		if (!noshow) {
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
		}
	}

#else
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *string;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) &&
	    (string = gtk_tree_model_get_string_from_iter(model, &iter))) {
		GtkTreeView *tree = GTK_TREE_VIEW(sess);
		GtkTreePath *path = gtk_tree_path_new_from_string(string);
		GtkTreeViewColumn *cursor = gtk_tree_view_get_column(tree, 1);

		g_free(string);
		gtk_tree_view_set_cursor_on_cell(tree, path, cursor, NULL, FALSE);
		gtk_tree_view_scroll_to_cell(tree, path, cursor, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
	gtk_widget_grab_default(sess);
	gtk_widget_grab_focus(sess);
#endif
	if (!noshow)
		grabbed_window(xscr->wind, NULL);
	return xscr->wind;
}

#ifdef DO_XLOCKING
Bool shutting_down;

void
handle_event(Display *dpy, XEvent *xev)
{
	DPRINTF("got event %d\n", xev->type);
	switch (xev->type) {
	case KeyPress:
	case KeyRelease:
	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
	case EnterNotify:
	case LeaveNotify:
	case FocusIn:
	case FocusOut:
	case KeymapNotify:
	case Expose:
	case GraphicsExpose:
	case NoExpose:
	case VisibilityNotify:
	case CreateNotify:
	case DestroyNotify:
	case UnmapNotify:
	case MapNotify:
	case MapRequest:
	case ReparentNotify:
	case ConfigureNotify:
	case ConfigureRequest:
	case GravityNotify:
	case ResizeRequest:
	case CirculateNotify:
	case CirculateRequest:
	case PropertyNotify:
	case SelectionClear:
	case SelectionRequest:
	case SelectionNotify:
	case ColormapNotify:
	case ClientMessage:
	case MappingNotify:
	case GenericEvent:
		break;
	default:
#ifdef DO_XLOCKING
		if (xssEventBase && xev->type == xssEventBase + ScreenSaverNotify) 
			handle_XScreenSaverNotify(dpy, xev);
		else
			EPRINTF("unknown event type %d\n", xev->type);
#endif
		break;
	}
}

void
handle_events(void)
{
	XEvent ev;

	XSync(display.dpy, False);
	while (XPending(display.dpy) && !shutting_down) {
		XNextEvent(display.dpy, &ev);
		handle_event(display.dpy, &ev);
	}
}

gboolean
on_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL|G_IO_HUP|G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
				(cond & G_IO_NVAL) ? "NVAL" : "",
				(cond & G_IO_HUP) ? "HUP" : "",
				(cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		handle_events();
	}
	return TRUE; /* keep event source */
}
#endif				/* DO_XLOCKING */

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
	atom = gdk_atom_intern_static_string("_XDE_XLOCK_COMMAND");
	_XA_XDE_XLOCK_COMMAND = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);
	atom = gdk_atom_intern_static_string("_XROOTPMAP_ID");
	_XA_XROOTPMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);
	atom = gdk_atom_intern_static_string("ESETROOT_PMAP_ID");
	_XA_ESETROOT_PMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);

#ifdef DO_XLOCKING
	if (!(display.dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display\n");
		exit(EXIT_FAILURE);
	}
	display.xfd = ConnectionNumber(display.dpy);
	GIOChannel *chan = g_io_channel_unix_new(display.xfd);
	guint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	g_io_add_watch(chan, mask, on_watch, NULL);
#endif				/* DO_XLOCKING */
}

void
ShowScreen(XdeScreen *xscr)
{
	if (xscr->wind) {
		gtk_widget_show_now(GTK_WIDGET(xscr->wind));
		if (options.username) {
			gtk_widget_set_sensitive(user, FALSE);
			gtk_widget_set_sensitive(pass, TRUE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			DPRINTF("grabbing password entry widget\n");
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
		} else {
			gtk_widget_set_sensitive(user, TRUE);
			gtk_widget_set_sensitive(pass, FALSE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			DPRINTF("grabbing username entry widget\n");
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
		}
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
	DPRINT();

	HideScreens();
}

static int
authenticate(void)
{
	pam_handle_t *pamh = NULL;
	const char *uname = NULL;
	int status = 0;

	DPRINTF("starting PAM\n");
	pam_start("system-login", NULL, &xde_pam_conv, &pamh);
	if (options.username) {
		pam_set_item(pamh, PAM_USER, options.username);
		state = LoginStateUsername;
		if (getuid() != 0)
			uname = strdup(options.username);
	}
	for (;;) {
		status = pam_authenticate(pamh, 0);
		if (login_result == LoginResultLogout)
			break;
		switch (status) {
		case PAM_ABORT:
			EPRINTF("PAM_ABORT\n");
			goto done;
		case PAM_AUTH_ERR:
			EPRINTF("PAM_AUTH_ERR\n");
			pam_set_item(pamh, PAM_USER, uname);
			continue;
		case PAM_CRED_INSUFFICIENT:
			EPRINTF("PAM_CRED_INSUFFICIENT\n");
			goto done;
		case PAM_AUTHINFO_UNAVAIL:
			EPRINTF("PAM_AUTHINFO_UNAVAIL\n");
			pam_set_item(pamh, PAM_USER, uname);
			continue;
		case PAM_MAXTRIES:
			EPRINTF("PAM_MAXTRIES\n");
			goto done;
		case PAM_SUCCESS:
			DPRINTF("PAM_SUCCESS\n");
			goto done;
		case PAM_USER_UNKNOWN:
			EPRINTF("PAM_USER_UNKNOWN\n");
			pam_set_item(pamh, PAM_USER, uname);
			continue;
		default:
			EPRINTF("Unexpected pam error\n");
			goto done;
		}
		DPRINTF("running main loop...\n");
		gtk_main();
		DPRINTF("...running main loop\n");
		if (login_result == LoginResultLogout)
			break;
#ifdef DO_XLOCKING
		if (lock_state == LockStateAborted)
			break;
#endif
	}
      done:
	DPRINTF("closing PAM\n");
	pam_end(pamh, status);
	return (status);
}

#ifdef DO_XLOCKING
static void
LockScreen(void)
{
	DPRINT();

	if (lock_state == LockStateLocked) {
		DPRINTF("already locked!\n");
		return;
	}
	ShowWindow();
	lock_state = LockStateLocked;
	gettimeofday(&lock_time, NULL);
	gtk_main_quit();
}

static void
RelockScreen(void)
{
	DPRINT();

	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	XForceScreenSaver(dpy, ScreenSaverActive);
	lock_state = LockStateLocked;
	gettimeofday(&lock_time, NULL);
	DPRINTF("running main loop...\n");
	gtk_main();
	DPRINTF("...running main loop\n");
}

static void
UnlockScreen(void)
{
	DPRINT();

	if (lock_state == LockStateUnlocked) {
		EPRINTF("already unlocked!\n");
		return;
	}
	HideWindow();
	lock_state = LockStateUnlocked;
	DPRINTF("running main loop...\n");
	gtk_main();
	DPRINTF("...running main loop\n");
}

static void
AbortLockScreen(void)
{
	DPRINT();

	if (lock_state != LockStateLocked) {
		EPRINTF("not locked!\n");
		return;
	} else {
		struct timeval tv = { 0, 0 };

		gettimeofday(&tv, NULL);
		if (tv.tv_sec < lock_time.tv_sec + options.guard) {
			DPRINTF("Screen saver interrupted: unlocking screen\n");
			lock_state = LockStateAborted;
			gtk_main_quit();
		}
	}
}
#endif				/* DO_XLOCKING */

static void
do_run(int argc, char *argv[])
{
	int status;

	startup(argc, argv);
	setup_systemd();
	top = GetWindow(False);
#ifdef DO_XLOCKING
	setup_screensaver();
#endif
#ifdef DO_XCHOOSER
	InitXDMCP(argv, argc);
#endif
#ifdef DO_XLOCKING
	if (options.command != CommandLock)
		UnlockScreen();
#endif
	for (;;) {
#ifdef DO_XLOCKING
		DPRINT();
		ShowWindow();
#endif
		DPRINT();
		status = authenticate();
#ifdef DO_XLOCKING
		DPRINT();
		if (lock_state == LockStateAborted) {
			UnlockScreen();
			continue;
		}
#endif
		DPRINT();
		if (login_result == LoginResultLogout) {
#ifdef DO_XLOCKING
			RelockScreen();
#else
			exit(EXIT_FAILURE);
#endif
			continue;
		}
		DPRINT();
		switch (status) {
		case PAM_ABORT:
			break;
		case PAM_CRED_INSUFFICIENT:
		case PAM_MAXTRIES:
		default:
			DPRINT();
#ifdef DO_XLOCKING
			RelockScreen();
#else
			exit(EXIT_FAILURE);
#endif
			continue;
		case PAM_SUCCESS:
			DPRINT();
#ifdef DO_XLOCKING
			UnlockScreen();
#else
			exit(EXIT_SUCCESS);
#endif
			continue;
		}
		break;
	}
	DPRINT();
}

#ifdef DO_XLOCKING
/** @brief quit the running background locker
  */
static void
do_quit(int argc, char *argv[])
{
	GdkDisplay *disp;
	int s, nscr;
	char selection[32];

	startup(argc, argv);
	disp = gdk_display_get_default();
	nscr = gdk_display_get_n_screens(disp);
	for (s = 0; s < nscr; s++)
		get_selection(None, selection, s);
}

/** @brief ask running background locker to lock (or just lock the display)
  */
static void
do_lock(int argc, char *argv[])
{
	Display *dpy;
	int s, nscr;
	char selection[32] = { 0, };
	Bool found = False;

#if 0
	/* unfortunately, these are privileged */
	setup_systemd();
	if (sd_session) {
		GError *err = NULL;
		GVariant *result;

		result = g_dbus_proxy_call_sync(sd_session, "Lock",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
		if (!result || err) {
			EPRINTF("Lock: %s: call failed: %s\n", getenv("XDG_SESSION_ID"),
				err ? err->message : NULL);
			g_clear_error(&err);
		} else
			g_variant_unref(result);
	}
#endif
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display %s\n", getenv("DISPLAY") ? : "");
		exit(EXIT_FAILURE);
	}
	nscr = ScreenCount(dpy);
	for (s = 0; s < nscr; s++) {
		Window owner;
		XEvent ev;
		Atom atom;

		snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
		atom = XInternAtom(dpy, selection, True);
		if (!atom) {
			DPRINTF("No '%s' atom on display\n", selection);
			continue;
		}
		owner = XGetSelectionOwner(dpy, atom);
		if (!owner) {
			DPRINTF("No '%s' owner on display\n", selection);
			continue;
		}
		atom = XInternAtom(dpy, "_XDE_XLOCK_COMMAND", False);
		if (!atom) {
			EPRINTF("Could not assign atom _XDE_XLOCK_COMMAND\n");
			continue;
		}
		found = True;

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = owner;
		ev.xclient.message_type = atom;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = LockCommandLock;
		ev.xclient.data.l[1] = 0;
		ev.xclient.data.l[2] = 0;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, owner, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	XSync(dpy, True);
	XCloseDisplay(dpy);
	if (!found) {
		DPRINTF("no background instance found\n");
		options.replace = True;
		if (fork() == 0)
			do_run(argc, argv);
	}
}

/** @brief ask running background locker to unlock (or just quit)
  */
static void
do_unlock(int argc, char *argv[])
{
	Display *dpy;
	int s, nscr;
	char selection[32] = { 0, };
	Bool found = False;

#if 0
	/* unfortunately, these are privileged */
	setup_systemd();
	if (sd_session) {
		GError *err = NULL;
		GVariant *result;

		result = g_dbus_proxy_call_sync(sd_session, "Unlock",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
		if (!result || err) {
			EPRINTF("Lock: %s: call failed: %s\n", getenv("XDG_SESSION_ID"),
				err ? err->message : NULL);
			g_clear_error(&err);
		} else
			g_variant_unref(result);
	}
#endif
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display %s\n", getenv("DISPLAY") ? : "");
		exit(EXIT_FAILURE);
	}
	nscr = ScreenCount(dpy);
	for (s = 0; s < nscr; s++) {
		Window owner;
		XEvent ev;
		Atom atom;

		snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
		atom = XInternAtom(dpy, selection, True);
		if (!atom) {
			DPRINTF("No '%s' atom on display\n", selection);
			continue;
		}
		owner = XGetSelectionOwner(dpy, atom);
		if (!owner) {
			DPRINTF("No '%s' owner on display\n", selection);
			continue;
		}
		atom = XInternAtom(dpy, "_XDE_XLOCK_COMMAND", False);
		if (!atom) {
			EPRINTF("Could not assign atom _XDE_XLOCK_COMMAND\n");
			continue;
		}
		found = True;

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = owner;
		ev.xclient.message_type = atom;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = LockCommandUnlock;
		ev.xclient.data.l[1] = 0;
		ev.xclient.data.l[2] = 0;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, owner, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	XSync(dpy, True);
	XCloseDisplay(dpy);
	if (!found) {
		EPRINTF("no background instance found\n");
		exit(EXIT_FAILURE);
	}
}
#endif				/* DO_XLOCKING */

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
    %1$s [options] ADDRESS [...]\n\
    %1$s [options] {-r|--replace}\n\
    %1$s [options] {-l|--lock}\n\
    %1$s [options] {-q|--quit}\n\
    %1$s [options] {-h|--help}\n\
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
        /* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] ADDRESS [...]\n\
    %1$s [options] {-r|--replace}\n\
    %1$s [options] {-l|--lock}\n\
    %1$s [options] {-q|--quit}\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    ADDRESS [...]\n\
        host names of display managers or \"BROADCAST\"\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
XDM options:\n\
    -x, --xdmaddress ADDRESS\n\
        address of xdm socket\n\
    -c, --clientaddress IPADDR\n\
        client address that initiated the request\n\
    -t, --connectionType TYPE\n\
        connection type supported by the client\n\
General options:\n\
    -w, --welcome TEXT\n\
        text to be display in title area of host menu\n\
        (%2$s)\n\
    -b, --banner PNGFILE\n\
        banner graphic to display\n\
        (%3$s)\n\
    -S, --splash JPGFILE\n\
        background impage to display\n\
        (%4$s)\n\
    -s, --side {top|left|bottom|right}\n\
        side on which to display branding (%5$s)\n\
    --charset CHARSET\n\
        character set to use (%6$s)\n\
    --language LANG\n\
        language to use (%7$s)\n\
    -i, --icons THEME\n\
        icon theme name to use for icons (%8$s)\n\
    -T, --theme THEME\n\
        GTK2 theme name to use for widgets (%9$s)\n\
    -u, --xde-theme\n\
        use the XDE theme (%10$s)\n\
    -v, --vendor VENDOR\n\
        vendor identifier for branding (%11$s)\n\
    -n, --dry-run\n\
        do not act: only print intentions (%12$s)\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL (%13$d)\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL (%14$d)\n\
        this option may be repeated.\n\
"	,argv[0]
	,options.welcome
	,options.banner
	,options.splash
	,show_side(options.side)
	,options.charset
	,options.language
	,options.usexde ? "xde" : (options.icon_theme ? : "auto")
	,options.usexde ? "xde" : (options.gtk2_theme ? : "auto")
	,show_bool(options.usexde)
	,options.vendor
	,show_bool(options.dryrun)
	,options.debug
	,options.output
	);
        /* *INDENT-ON* */
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
	XrmCombineFileDatabase(APPDFLT, &rdb, False);
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
	// xlogin.greeting:		Welcome to CLIENTHOST
	if ((val = get_xlogin_resource(rdb, "greeting", NULL))) {
		getXrmString(val, &resources.greeting);
		getXrmString(val, &options.welcome);
	}
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
	if ((val = get_xlogin_resource(rdb, "namePrompt", "Username:  "))) {
		getXrmString(val, &resources.namePrompt);
	}
	// xlogin.passwdPrompt:		Password:
	if ((val = get_xlogin_resource(rdb, "passwdPrompt", "Password:  "))) {
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
	// xlogin.inputFace:		Sans-12:bold
	// xlogin.inputFont:
	if ((val = get_any_resource(rdb, "inputFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.inputFace);
	}
	// xlogin.inputColor:		grey20
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
	if ((val = get_resource(rdb, "welcome", NULL))) {
		getXrmString(val, &options.welcome);
	}
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
	char hostname[64] = { 0, };
	char *buf;
	int len;
	static char *format = "Welcome to %s!";

	free(defaults.welcome);
	gethostname(hostname, sizeof(hostname));
	len = strlen(format) + strnlen(hostname, sizeof(hostname)) + 1;
	buf = defaults.welcome = calloc(len, sizeof(*buf));
	snprintf(buf, len, format, hostname);
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
#ifdef DO_XCHOOSER
			{"xdmaddress",	    required_argument,	NULL, 'x'},
			{"clientaddress",   required_argument,	NULL, 'c'},
			{"connectionType",  required_argument,	NULL, 't'},
			{"welcome",	    required_argument,	NULL, 'w'},
#else					/* DO_XCHOOSER */
			{"prompt",	    required_argument,	NULL, 'p'},
#endif					/* DO_XCHOOSER */
#ifdef DO_XLOCKING
			{"replace",	    no_argument,	NULL, 'r'},
			{"lock",	    no_argument,	NULL, 'l'},
			{"unlock",	    no_argument,	NULL, 'U'},
			{"quit",	    no_argument,	NULL, 'q'},
#endif					/* DO_XLOCKING */

			{"banner",	    required_argument,	NULL, 'b'},
			{"splash",	    required_argument,	NULL, 'S'},
			{"side",	    required_argument,	NULL, 's'},
			{"charset",	    required_argument,	NULL, '1'},
			{"language",	    required_argument,	NULL, '2'},
			{"icons",	    required_argument,	NULL, 'i'},
			{"theme",	    required_argument,	NULL, 'T'},
			{"xde-theme",	    no_argument,	NULL, 'u'},
			{"vendor",	    required_argument,	NULL, '5'},
			{"xsessions",	    no_argument,	NULL, 'X'},
			{"default",	    required_argument,	NULL, '6'},
#ifndef DO_XLOCKING
			{"username",	    required_argument,	NULL, '7'},
			{"guard",	    required_argument,	NULL, 'g'},
#endif
			{"nosetbg",	    no_argument,	NULL, '8'},
			{"transparent",	    no_argument,	NULL, '9'},

			{"dry-run",	    no_argument,	NULL, 'n'},
			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "rlUqx:c:t:w:b:S:s:p:i:T:unD::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "rlUqx:c:t:w:b:S:s:p:i:T:unDvhVCH?");
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
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 'T':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 'u':	/* -u, --xde-theme */
			options.usexde = True;
			break;
		case '5':	/* --vendor VENDOR */
			free(options.vendor);
			options.vendor = strdup(optarg);
			break;
		case 'X':	/* --xsessions */
			options.xsession = True;
			break;
		case '6':	/* --default DEFAULT */
			free(options.choice);
			options.choice = strdup(optarg);
			break;
#ifndef DO_XLOCKING
		case '7':	/* --username USERNAME */
			free(options.username);
			options.username = strdup(optarg);
			break;
		case 'g':	/* --guard SECONDS */
			if ((val = strtol(optarg, NULL, 0)) < 0)
				goto bad_option;
			options.guard = val;
			break;
#endif
		case '8':	/* --nosetbg */
			options.setbg = False;
			break;
		case '9':	/* --transparent */
			options.transparent = True;
			break;

		case 'n':	/* -n, --dry-run */
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
	get_defaults(argc, argv);
	switch (command) {
	default:
	case CommandDefault:
		if (optind >= argc) {
#ifdef DO_XCHOOSER
			fprintf(stderr, "%s: missing non-option argument\n", argv[0]);
			goto bad_nonopt;
#else
		} else {
			fprintf(stderr, "%s: excess non-option arguments\n", argv[0]);
			goto bad_nonopt;
#endif
		}
		DPRINTF("%s: running default\n", argv[0]);
		do_run(argc - optind, &argv[optind]);
		exit(EXIT_FAILURE);
		break;
#ifdef DO_XLOCKING
	case CommandReplace:
		DPRINTF("%s: running replace\n", argv[0]);
		do_run(argc, argv);
		break;
	case CommandQuit:
		DPRINTF("%s: running quit\n", argv[0]);
		do_quit(argc, argv);
		break;
	case CommandLock:
		DPRINTF("%s: running lock\n", argv[0]);
		do_lock(argc, argv);
		break;
	case CommandUnlock:
		DPRINTF("%s: running unlock\n", argv[0]);
		do_unlock(argc, argv);
		break;
#endif				/* DO_XLOCKING */
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
