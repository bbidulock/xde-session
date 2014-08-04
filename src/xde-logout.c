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

typedef enum _LogoSide {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_BOTTOM,
} LogoSide;

typedef enum {
	CommandDefault,
	CommandHelp,			/* command argument help */
	CommandVersion,			/* command version information */
	CommandCopying,			/* command copying information */
} CommandType;

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
	LogoSide side;
	Bool usexde;
	Bool noask;
	unsigned int timeout;
	char *clientId;
	char *saveFile;
	char *vendor;
	char *prefix;
	char *splash;
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
	.side = LOGO_SIDE_LEFT,
	.usexde = False,
	.noask = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
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
	.side = LOGO_SIDE_LEFT,
	.usexde = False,
	.noask = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
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

Atom _XA_XDE_THEME_NAME;
Atom _XA_GTK_READ_RCFILES;
Atom _XA_XROOTPMAP_ID;
Atom _XA_ESETROOT_PMAP_ID;

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
} XdeScreen;

XdeScreen *screens;

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

static void reparse(Display *dpy, Window root);
void get_source(XdeScreen *xscr);

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
		if (!xscr->pixbuf)
			get_source(xscr);
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
	GError *error = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;

	if (!getenv("XDG_SESSION_ID")) {
		DPRINTF("no XDG_SESSION_ID supplied\n");
		return;
	}
	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)) || error) {
		EPRINTF("cannot access system bus\n");
		return;
	} else
		error = NULL;
	proxy = dbus_g_proxy_new_for_name(bus,
			"org.freedesktop.login1",
			"/org/freedesktop/login1",
			"org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	} else
		error = NULL;
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
	static const struct prog_cmd progs[6] = {
		/* *INDENT-OFF* */
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

void
test_login_functions()
{
	const char *seat;
	int ret;
	
	seat = getenv("XDG_SEAT") ?: "seat0";
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
}

static void
on_switch_session(GtkMenuItem *item, gpointer data)
{
	gchar *session = data;
	GError *error = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)) || error) {
		EPRINTF("cannot access system buss\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
			"org.freedesktop.login1",
			"/org/freedesktop/login1",
			"org.freedesktop.login1.Manager");
	ok = dbus_g_proxy_call(proxy, "ActivateSession", &error, G_TYPE_STRING,
			session, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok || error)
		DPRINTF("ActivateSession: %s: call failed\n", session);
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

GtkWidget *
get_user_menu(void)
{
	const char *seat, *sess;
	char **sessions = NULL, **s;
	GtkWidget *menu = NULL;
	gboolean gotone = FALSE;

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
	for (s = sessions; s && *s; free(*s), s++) {
		char *type = NULL, *klass = NULL, *user = NULL, *host = NULL,
			*tty = NULL, *disp = NULL;
		unsigned int vtnr = 0;
		uid_t uid = -1;

		DPRINTF("%s(%s): considering session\n", seat, *s);
		if (sess && !strcmp(*s, sess)) {
			DPRINTF("%s(%s): cannot switch to own session\n", seat, *s);
			continue;
		}
		if (sd_session_is_active(*s)) {
			DPRINTF("%s(%s): cannot switch to active session\n", seat, *s);
			continue;
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
		gotone = TRUE;
	}
	free(sessions);
	if (gotone)
		return (menu);
	g_object_unref(G_OBJECT(menu));
	return (NULL);
}

void
test_manager_functions()
{
	GError *error = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
//	gboolean ok;
	const char *env;
	char *path;

	path = calloc(PATH_MAX + 1, sizeof(*path));

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)) || error) {
		EPRINTF("cannot access system bus\n");
		free(path);
		return;
	} else
		error = NULL;

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
	GError *error = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gchar *value = NULL;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)) || error) {
		EPRINTF("cannot access system bus\n");
		return;
	} else
		error = NULL;
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	value = NULL;
	ok = dbus_g_proxy_call(proxy, "CanPowerOff",
			       &error, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanPowerOff status is %s\n", value);
		action_can[LOGOUT_ACTION_POWEROFF] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else
		error = NULL;
	ok = dbus_g_proxy_call(proxy, "CanReboot",
			       &error, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanReboot status is %s\n", value);
		action_can[LOGOUT_ACTION_REBOOT] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else
		error = NULL;
	ok = dbus_g_proxy_call(proxy, "CanSuspend",
			       &error, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanSuspend status is %s\n", value);
		action_can[LOGOUT_ACTION_SUSPEND] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else
		error = NULL;
	ok = dbus_g_proxy_call(proxy, "CanHibernate",
			       &error, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanHibernate status is %s\n", value);
		action_can[LOGOUT_ACTION_HIBERNATE] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else
		error = NULL;
	ok = dbus_g_proxy_call(proxy, "CanHybridSleep",
			       &error, G_TYPE_INVALID, G_TYPE_STRING, &value, G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanHybridSleep status is %s\n", value);
		action_can[LOGOUT_ACTION_HYBRIDSLEEP] = status_of_string(value);
		g_free(value);
		value = NULL;
	} else
		error = NULL;
	g_object_unref(G_OBJECT(proxy));
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

static SmcConn smcConn;

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
	GdkWindow *w = gtk_widget_get_window(xscr->wind);
	GdkWindow *r = gdk_screen_get_root_window(xscr->scrn);
	cairo_t *cr;
	GdkEventExpose *ev = (typeof(ev)) event;

	cr = gdk_cairo_create(GDK_DRAWABLE(w));
	// gdk_cairo_reset_clip(cr, GDK_DRAWABLE(w));
	if (ev->region)
		gdk_cairo_region(cr, ev->region);
	else
		gdk_cairo_rectangle(cr, &ev->area);
	if (xscr->pixmap) {
		gdk_cairo_set_source_pixmap(cr, xscr->pixmap, 0, 0);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	} else
		gdk_cairo_set_source_window(cr, r, 0, 0);
	cairo_paint(cr);
	GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };
	gdk_cairo_set_source_color(cr, &color);
	cairo_paint_with_alpha(cr, 0.7);
	cairo_destroy(cr);
	return FALSE;
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
	if (gdk_pointer_grab(win, TRUE, mask, win, NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		EPRINTF("Could not grab pointer!\n");
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
				(int) wp, (int) hp,
				xmon->geom.width, xmon->geom.height);
		scaled = gdk_pixbuf_scale_simple(pixbuf,
						 xmon->geom.width,
						 xmon->geom.height, GDK_INTERP_BILINEAR);
		gdk_cairo_set_source_pixbuf(cr, scaled, xmon->geom.x, xmon->geom.y);
	} else if (wp <= 0.5 * wm && hp <= 0.5 * hm) {
		/* good size for tiling */
		DPRINTF("tiling pixbuf at %dx%d into %dx%d\n",
				(int) wp, (int) hp,
				xmon->geom.width, xmon->geom.height);
		gdk_cairo_set_source_pixbuf(cr, pixbuf, xmon->geom.x, xmon->geom.y);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	} else {
		/* somewhere in between: scale down for integer tile */
		DPRINTF("scaling and tiling pixbuf at %dx%d into %dx%d\n",
				(int) wp, (int) hp,
				xmon->geom.width, xmon->geom.height);
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
get_source(XdeScreen *xscr)
{
	GdkDisplay *disp = gdk_screen_get_display(xscr->scrn);
	GdkWindow *root = gdk_screen_get_root_window(xscr->scrn);
	GdkColormap *cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom prop;

	if (!xscr->pixbuf && options.splash) {
		if (!(xscr->pixbuf = gdk_pixbuf_new_from_file(options.splash, NULL))) {
			/* cannot use it again */
			free(options.splash);
			options.splash = NULL;
		}
	}
	if (xscr->pixbuf) {
		if (!xscr->pixmap) {
			xscr->pixmap = gdk_pixmap_new(GDK_DRAWABLE(root), xscr->width, xscr->height, -1);
			gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
			render_pixbuf_for_scr(xscr->pixbuf, xscr->pixmap, xscr);
		}
		return;
	}
	if (xscr->pixmap) {
		g_object_unref(G_OBJECT(xscr->pixmap));
		xscr->pixmap = NULL;
	}

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
			}
		}
		if (data)
			XFree(data);
	}
}

/** @brief get a covering window for a screen
  */
void
GetScreen(XdeScreen *xscr, int s, GdkScreen *scrn)
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
	w = GTK_WINDOW(wind);
	gtk_window_set_screen(w, scrn);
	gtk_window_set_wmclass(w, "xde-xlogin", "XDE-XLogin");
	gtk_window_set_title(w, "XDMCP Greeter");
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

	gtk_widget_set_app_paintable(wind, TRUE);

	g_signal_connect(G_OBJECT(w), "destroy", G_CALLBACK(on_destroy), NULL);
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(on_expose_event), xscr);

	gtk_window_set_focus_on_map(w, TRUE);
	gtk_window_set_accept_focus(w, TRUE);
	gtk_window_set_keep_above(w, TRUE);
	gtk_window_set_modal(w, TRUE);
	gtk_window_stick(w);
	gtk_window_deiconify(w);

	xscr->nmon = gdk_screen_get_n_monitors(scrn);
	xscr->mons = calloc(xscr->nmon, sizeof(*xscr->mons));
	for (m = 0, mon = xscr->mons; m < xscr->nmon; m++, mon++) {
		float xrel, yrel;

		mon->index = m;
		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);

		xrel = (float) (mon->geom.x + mon->geom.width / 2) / (float) xscr->width;
		yrel = (float) (mon->geom.y + mon->geom.height / 2) / (float) xscr->height;

		mon->align = gtk_alignment_new(xrel, yrel, 0, 0);
		gtk_container_add(GTK_CONTAINER(w), mon->align);
	}
	get_source(xscr);
	gtk_widget_show_all(wind);

	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);

	gdk_window_set_override_redirect(win, TRUE);

	GdkWindow *root = gdk_screen_get_root_window(scrn);
	GdkEventMask mask = gdk_window_get_events(root);

	gdk_window_add_filter(root, root_handler, xscr);
	mask |= GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
	gdk_window_set_events(root, mask);
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
GetScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	screens = calloc(nscr, sizeof(*screens));

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		GetScreen(xscr, s, gdk_display_get_screen(disp, s));

	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn);

	reparse(dpy, GDK_WINDOW_XID(root));
}

GtkWidget *cont;			/* container of event box */
GtkWidget *ebox;			/* event box window within the screen */

GtkWidget *
GetBanner(void)
{
	GtkWidget *ban = NULL, *bin, *pan, *img;

	if (options.banner && (img = gtk_image_new_from_file(options.banner))) {
		ban = gtk_vbox_new(FALSE, 0);
		bin = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(bin), GTK_SHADOW_ETCHED_IN);
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

	GtkWidget *inp = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(inp), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

	gtk_box_pack_start(GTK_BOX(pan), inp, TRUE, TRUE, 4);

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

	return (pan);
}

GtkWidget *
GetPane(GtkWidget *cont)
{
	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(cont), ebox);
	gtk_widget_set_size_request(ebox, -1, -1);

	GtkWidget *v = gtk_vbox_new(FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(v), 10);

	gtk_container_add(GTK_CONTAINER(ebox), v);

	GtkWidget *lab = gtk_label_new(NULL);
	gchar *markup;

	markup = g_strdup_printf
	    ("<span font=\"Liberation Sans 10\">%s</span>", options.welcome);
	gtk_label_set_markup(GTK_LABEL(lab), markup);
	gtk_misc_set_alignment(GTK_MISC(lab), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(lab), 3, 3);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(v), lab, FALSE, TRUE, 0);

	GtkWidget *h = gtk_hbox_new(FALSE, 5);

	gtk_box_pack_end(GTK_BOX(v), h, TRUE, TRUE, 0);

	if ((v = GetBanner()))
		gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0);

	v = GetPanel();
	gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0);

	return (ebox);
}

GtkWidget *
GetWindow(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	GdkScreen *scrn = NULL;
	XdeScreen *xscr;
	XdeMonitor *mon;
	int s, m;
	gint x = 0, y = 0;

	GetScreens();

	gdk_display_get_pointer(disp, &scrn, &x, &y, NULL);
	if (!scrn)
		scrn = gdk_display_get_default_screen(disp);
	s = gdk_screen_get_number(scrn);
	xscr = screens + s;

	m = gdk_screen_get_monitor_at_point(scrn, x, y);
	mon = xscr->mons + m;

	cont = mon->align;
	ebox = GetPane(cont);

	gtk_widget_show_all(cont);
	gtk_widget_show_now(cont);
	gtk_widget_grab_default(buttons[LOGOUT_ACTION_LOGOUT]);
	gtk_widget_grab_focus(buttons[LOGOUT_ACTION_LOGOUT]);
	grabbed_window(xscr->wind, NULL);
	return xscr->wind;
}

static void
startup(int argc, char *argv[])
{
	if (options.usexde) {
		static const char *suffix = "/.gtkrc-2.0.xde";
		const char *home = getenv("HOME") ? : ".";
		int len = strlen(home) + strlen(suffix) + 1;
		char *file = calloc(len, sizeof(*file));

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

static void
do_run(int argc, char *argv[])
{
	startup(argc, argv);
	top = GetWindow();
	gtk_main();
}


static void
logoutSetProperties(SmcConn smcConn, SmPointer data)
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
logoutSaveYourselfPhase2CB(SmcConn smcConn, SmPointer data)
{
	logoutSetProperties(smcConn, data);
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
logoutSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown,
		     int interactStyle, Bool fast)
{
	if (!(shutting_down = shutdown)) {
		if (!SmcRequestSaveYourselfPhase2(smcConn,
				logoutSaveYourselfPhase2CB, data))
			SmcSaveYourselfDone(smcConn, False);
		return;
	}
	logoutSetProperties(smcConn, data);
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
logoutDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	shutting_down = False;
	gtk_main_quit();
}

static void
logoutSaveCompleteCB(SmcConn smcConn, SmPointer data)
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
logoutShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	shutting_down = False;
	gtk_main_quit();
}

static unsigned long logoutCBMask =
    SmcSaveYourselfProcMask | SmcDieProcMask |
    SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

static SmcCallbacks logoutCBs = {
	.save_yourself = {
			  .callback = &logoutSaveYourselfCB,
			  .client_data = NULL,
			  },
	.die = {
		.callback = &logoutDieCB,
		.client_data = NULL,
		},
	.save_complete = {
			  .callback = &logoutSaveCompleteCB,
			  .client_data = NULL,
			  },
	.shutdown_cancelled = {
			       .callback = &logoutShutdownCancelledCB,
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
				    logoutCBMask, &logoutCBs, options.clientId,
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

void
run_logout(int argc, char *argv[])
{
	int i;

	/* initialize session managerment functions */
	init_smclient();

	/* determine which functions are available */
	test_login_functions();
	test_power_functions();
	test_lock_screen_program();
	test_user_functions();
	test_session_functions();

	/* adjust the tooltips for the functions */
	if (options.debug) {
		for (i = 0; i < LOGOUT_ACTION_COUNT; i++) {
			switch (action_can[i]) {
			case AvailStatusUndef:
				button_tips[i] = g_strdup_printf("\nFunction undefined."
								 "\nCan value was %s",
								 "(undefined)");
				break;
			case AvailStatusUnknown:
				button_tips[i] = g_strdup_printf("\nFunction unknown."
								 "\nCan value was %s", "(unknown)");
				break;
			case AvailStatusNa:
				button_tips[i] = g_strdup_printf("\nFunction not available."
								 "\nCan value was %s", "na");
				break;
			case AvailStatusNo:
				button_tips[i] = g_strdup_printf("\n%s"
								 "\nCan value was %s",
								 button_tips[i], "no");
				break;
			case AvailStatusChallenge:
				button_tips[i] = g_strdup_printf("\n%s"
								 "\nCan value was %s",
								 button_tips[i], "challenge");
				break;
			case AvailStatusYes:
				button_tips[i] = g_strdup_printf("\n%s"
								 "\nCan value was %s",
								 button_tips[i], "yes");
				break;
			}
		}
	}
	do_run(argc, argv);
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
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL))) {
		EPRINTF("cannot access the system bus\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktp/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "PowerOff", NULL,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to PowerOff failed\n");
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Reboot(void)
{
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL))) {
		EPRINTF("cannot access the system bus\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktp/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Reboot", NULL,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Reboot failed\n");
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Suspend(void)
{
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL))) {
		EPRINTF("cannot access the system bus\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktp/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Suspend", NULL,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Suspend failed\n");
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_Hibernate(void)
{
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL))) {
		EPRINTF("cannot access the system bus\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktp/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "Hibernate", NULL,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to Hibernate failed\n");
		g_object_unref(G_OBJECT(proxy));
		return;
	}
	g_object_unref(G_OBJECT(proxy));
}

static void
action_HybridSleep(void)
{
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gboolean ok;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL))) {
		EPRINTF("cannot access the system bus\n");
		return;
	}
	proxy = dbus_g_proxy_new_for_name(bus,
					  "org.freedesktop.login1",
					  "/org/freedesktp/login1",
					  "org.freedesktop.login1.Manager");
	if (!proxy) {
		EPRINTF("cannot create DBUS proxy\n");
		return;
	}
	ok = dbus_g_proxy_call(proxy, "HybridSleep", NULL,
			       G_TYPE_BOOLEAN, TRUE, G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ok) {
		EPRINTF("call to HybridSleep failed\n");
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
	if ((p = strstr(here, "/src")) && !*(p+4))
		*p = '\0';
	/* executed in place */
	if (strcmp(here, "/usr/bin")) {
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

void
get_defaults(int argc, char *argv[])
{
	get_default_vendor();
	get_default_banner();
	get_default_splash();
	get_default_welcome();
	get_default_language();
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	set_defaults(argc, argv);

	saveArgc = argc;
	saveArgv = argv;

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"prompt",	required_argument,	NULL, 'p'},
			{"banner",	required_argument,	NULL, 'b'},
			{"splash",	required_argument,	NULL, 'S'},
			{"side",	required_argument,	NULL, 's'},
			{"noask",	no_argument,		NULL, 'n'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"xde-theme",	no_argument,		NULL, 'x'},
			{"timeout",	required_argument,	NULL, 'T'},

			{"clientId",	required_argument,	NULL, '8'},
			{"restore",	required_argument,	NULL, '9'},

			{"dry-run",	no_argument,		NULL, 'N'},
			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "p:b:S:s:nD::v::hVCH?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "p:b:S:s:nDvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'p':	/* -p, --prompt PROPMT */
			free(options.welcome);
			options.welcome = strdup(optarg);
			break;
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
		case 'x':
			options.usexde = True;
			break;
		case 'T':
			options.timeout = strtoul(optarg, NULL, 0);
			break;

		case '8': /* -clientId CLIENTID */
			free(options.clientId);
			options.clientId = strdup(optarg);
			break;
		case '9': /* -restore SAVEFILE */
			free(options.saveFile);
			options.saveFile = strdup(optarg);
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
			exit(2);
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
	case CommandDefault:
		DPRINTF("%s: running logout\n", argv[0]);
		run_logout(argc, argv);
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
