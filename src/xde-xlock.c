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
#include <X11/extensions/scrnsaver.h>
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/Xdmcp.h>
#include <X11/SM/SMlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <dbus/dbus-glib.h>
#include <pwd.h>
#include <systemd/sd-login.h>
#include <security/pam_appl.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

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

typedef enum _LogoSide {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_BOTTOM,
} LogoSide;

enum {
	OBEYSESS_DISPLAY,		/* obey multipleSessions resource */
	REMANAGE_DISPLAY,		/* force remanage */
	UNMANAGE_DISPLAY,		/* force deletion */
	RESERVER_DISPLAY,		/* force server termination */
	OPENFAILED_DISPLAY,		/* XOpenDisplay failed, retry */
};

typedef enum {
	CommandDefault,
	CommandLocker,			/* run as a background locker */
	CommandReplace,			/* replace any running instance */
	CommandLock,			/* ask running instance to lock */
	CommandQuit,			/* ask running instance to quit */
	CommandHelp,			/* command argument help */
	CommandVersion,			/* command version information */
	CommandCopying,			/* command copying information */
} CommandType;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	char *banner;
	char *welcome;
	CommandType command;
	char *current;
	Bool managed;
	char *session;
	char *choice;
	char *username;
	char *password;
	Bool usexde;
	Bool replace;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.command = CommandDefault,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.replace = False,
};

typedef enum {
	LoginStateInit,
	LoginStateUsername,
	LoginStatePassword,
	LoginStateReady,
} LoginState;

LoginState state = LoginStateInit;

typedef enum {
	LoginResultLogout,
	LoginResultLaunch,
} LoginResult;

LoginResult login_result;

Atom _XA_XDE_THEME_NAME;
Atom _XA_GTK_READ_RCFILES;
Atom _XA_XROOTPMAP_ID;
Atom _XA_ESETROOT_PMAP_ID;

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
	XScreenSaverInfo info;		/* screen saver info for this screen */
	char selection[32];
	Window selwin;
} XdeScreen;

XdeScreen *screens;

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

static GtkWidget *buttons[5];
static GtkWidget *l_uname, *l_pword, *l_lstat;
static GtkWidget *user, *pass;

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
			gchar *prompt =
			    g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>",
					    (*m)->msg);

			gtk_label_set_markup(GTK_LABEL(l_uname), prompt);
			gtk_label_set_text(GTK_LABEL(user), "");
			gtk_label_set_text(GTK_LABEL(pass), "");
			gtk_label_set_text(GTK_LABEL(l_lstat), "");
			gtk_widget_set_sensitive(user, TRUE);
			gtk_widget_set_sensitive(pass, FALSE);
			gtk_widget_set_sensitive(buttons[0], FALSE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_main();
			r->resp = strdup(gtk_entry_get_text(GTK_ENTRY(user)));
			break;
		}
		case PAM_PROMPT_ECHO_OFF:	/* obtain a string without
						   echoing */
		{
			gchar *prompt =
			    g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>",
					    (*m)->msg);

			gtk_label_set_markup(GTK_LABEL(l_pword), prompt);
			gtk_label_set_text(GTK_LABEL(pass), "");
			gtk_label_set_text(GTK_LABEL(l_lstat), "");
			gtk_widget_set_sensitive(user, FALSE);
			gtk_widget_set_sensitive(pass, TRUE);
			gtk_widget_set_sensitive(buttons[0], FALSE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			gtk_main();
			r->resp = strdup(gtk_entry_get_text(GTK_ENTRY(pass)));
			break;
		}
		case PAM_ERROR_MSG:	/* display an error message */
		{
			gchar *prompt =
			    g_strdup_printf
			    ("<span font=\"Liberation Sans 9\" color=\"red\"><b>%s</b></span>",
			     (*m)->msg);

			gtk_label_set_markup(GTK_LABEL(l_lstat), prompt);
			relax();
			break;
		}
		case PAM_TEXT_INFO:	/* display some text */
		{
			gchar *prompt =
			    g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>",
					    (*m)->msg);

			gtk_label_set_markup(GTK_LABEL(l_lstat), prompt);
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
}

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

static void
free_string(gpointer data, GClosure *unused)
{
	free(data);
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
	GError *error = NULL;
	DBusGConnection *bus;
	DBusGProxy *proxy;
	gchar *value = NULL;
	gboolean ok;
	GtkWidget *submenu, *power, *imag, *item;
	gboolean gotone = FALSE;

	if (!menu)
		return;

	if (!(bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)) || error) {
		EPRINTF("cannot access system bus\n");
		return;
	}
	if (!(proxy = dbus_g_proxy_new_for_name(bus,
						"org.freedesktop.login1",
						"/org/freedesktop/login1",
						"org.freedesktop.login1.Manager"))) {
		EPRINTF("could create DBUS proxy\n");
		return;
	}

	power = gtk_image_menu_item_new_with_label(GTK_STOCK_EXECUTE);
	gtk_image_menu_item_set_use_stock(GTK_IMAGE_MENU_ITEM(power), TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), power);

	submenu = gtk_menu_new();

	ok = dbus_g_proxy_call(proxy, "CanPowerOff",
			&error, G_TYPE_INVALID, G_TYPE_STRING, &value,
			G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanPowerOff status is %s\n", value);
		item = gtk_image_menu_item_new_with_label("Power Off");
		imag = gtk_image_new_from_icon_name("system-shutdown", GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				G_CALLBACK(on_poweroff),
				value, free_value, G_CONNECT_AFTER);
		if (!strcmp(value, "yes") || !strcmp(value, "challenge")) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
	} else
		g_clear_error(&error);

	ok = dbus_g_proxy_call(proxy, "CanReboot",
			&error, G_TYPE_INVALID, G_TYPE_STRING, &value,
			G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanSuspend status is %s\n", value);
		item = gtk_image_menu_item_new_with_label("Reboot");
		imag = gtk_image_new_from_icon_name("system-reboot", GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				G_CALLBACK(on_reboot),
				value, free_value, G_CONNECT_AFTER);
		if (!strcmp(value, "yes") || !strcmp(value, "challenge")) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
	} else
		g_clear_error(&error);

	ok = dbus_g_proxy_call(proxy, "CanSuspend",
			&error, G_TYPE_INVALID, G_TYPE_STRING, &value,
			G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanSuspend status is %s\n", value);
		item = gtk_image_menu_item_new_with_label("Suspend");
		imag = gtk_image_new_from_icon_name("system-suspend", GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				G_CALLBACK(on_suspend),
				value, free_value, G_CONNECT_AFTER);
		if (!strcmp(value, "yes") || !strcmp(value, "challenge")) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
	} else
		g_clear_error(&error);

	ok = dbus_g_proxy_call(proxy, "CanHibernate",
			&error, G_TYPE_INVALID, G_TYPE_STRING, &value,
			G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanSuspend status is %s\n", value);
		item = gtk_image_menu_item_new_with_label("Hibernate");
		imag = gtk_image_new_from_icon_name("system-suspend-hibernate", GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				G_CALLBACK(on_hibernate),
				value, free_value, G_CONNECT_AFTER);
		if (!strcmp(value, "yes") || !strcmp(value, "challenge")) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
	} else
		g_clear_error(&error);

	ok = dbus_g_proxy_call(proxy, "CanHybridSleep",
			&error, G_TYPE_INVALID, G_TYPE_STRING, &value,
			G_TYPE_INVALID);
	if (ok && !error) {
		DPRINTF("CanSuspend status is %s\n", value);
		item = gtk_image_menu_item_new_with_label("Hybrid Sleep");
		imag = gtk_image_new_from_icon_name("system-sleep", GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), imag);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				G_CALLBACK(on_hybridsleep),
				value, free_value, G_CONNECT_AFTER);
		if (!strcmp(value, "yes") || !strcmp(value, "challenge")) {
			gtk_widget_set_sensitive(item, TRUE);
			gotone = TRUE;
		} else
			gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		value = NULL;
	} else
		g_clear_error(&error);

	g_object_unref(proxy);
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

/** @brief add a switch users submenu to the actions menu
  */
static void
append_switch_users(GtkMenu *menu)
{
	const char *seat, *sess;
	char **sessions = NULL, **s;
	GtkWidget *submenu, *jumpto;
	gboolean gotone = FALSE;

	if (!menu)
		return;

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
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		g_signal_connect_data(G_OBJECT(item), "activate",
				      G_CALLBACK(on_switch_session),
				      strdup(*s), free_string, G_CONNECT_AFTER);
		gtk_widget_show(item);
		gotone = TRUE;
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

static void
on_action_clicked(GtkButton *button, gpointer user_data)
{
	GtkMenu *menu;

	if (!(menu = create_action_menu())) {
		DPRINTF("No actions to perform\n");
		return;
	}
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
}

static void
on_user_activate(GtkEntry *user, gpointer data)
{
	GtkEntry *pass = data;
	const gchar *username;

	free(options.username);
	options.username = NULL;
	free(options.password);
	options.password = NULL;
	gtk_entry_set_text(pass, "");

	if ((username = gtk_entry_get_text(user)) && username[0]) {
		options.username = strdup(username);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		state = LoginStateUsername;
		gtk_widget_grab_default(GTK_WIDGET(pass));
		gtk_widget_grab_focus(GTK_WIDGET(pass));
	}
}

static void
on_pass_activate(GtkEntry *pass, gpointer data)
{
	GtkEntry *user = data;
	const gchar *password;

	(void) user;
	free(options.password);
	options.password = NULL;

	if ((password = gtk_entry_get_text(pass))) {
		options.password = strdup(password);
		state = LoginStatePassword;
		/* FIXME: do the authorization */
		gtk_widget_set_sensitive(buttons[3], TRUE);
		gtk_widget_grab_default(buttons[3]);
		gtk_widget_grab_focus(buttons[3]);
	}
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

void
get_source(XdeScreen *xscr)
{
	GdkDisplay *disp = gdk_screen_get_display(xscr->scrn);
	GdkWindow *root = gdk_screen_get_root_window(xscr->scrn);
	GdkColormap *cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom prop;

	xscr->pixmap = NULL;

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
	get_source(xscr);

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
	gtk_widget_show_all(wind);

	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);

	gdk_window_set_override_redirect(win, TRUE);

	GdkWindow *root = gdk_screen_get_root_window(scrn);
	GdkEventMask mask = gdk_window_get_events(root);

	gdk_window_add_filter(root, root_handler, xscr);
	mask |= GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
	gdk_window_set_events(root, mask);

	Window owner = None;
	GdkDisplay *disp = gdk_display_get_default();
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

GtkWidget *
GetPanel(void)
{
	GtkWidget *pan;

	pan = gtk_vbox_new(FALSE, 0);

	GtkWidget *inp = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(inp), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

	gtk_box_pack_start(GTK_BOX(pan), inp, TRUE, TRUE, 4);

	GtkWidget *align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

	gtk_container_add(GTK_CONTAINER(inp), align);

	GtkWidget *login = gtk_table_new(2, 3, TRUE);

	gtk_container_set_border_width(GTK_CONTAINER(login), 5);
	gtk_table_set_col_spacings(GTK_TABLE(login), 5);
	gtk_table_set_row_spacings(GTK_TABLE(login), 5);
	gtk_table_set_col_spacing(GTK_TABLE(login), 0, 0);
	gtk_container_add(GTK_CONTAINER(align), login);

	l_uname = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l_uname),
			     "<span font=\"Liberation Sans 9\"><b>Username:</b></span>");
	gtk_misc_set_alignment(GTK_MISC(l_uname), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_uname), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), l_uname, 0, 1, 0, 1);

	user = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(user), 12);
	gtk_entry_set_visibility(GTK_ENTRY(user), TRUE);
	gtk_widget_set_can_default(user, TRUE);
	gtk_widget_set_can_focus(user, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), user, 1, 2, 0, 1);

	l_pword = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l_pword),
			     "<span font=\"Liberation Sans 9\"><b>Password:</b></span>");
	gtk_misc_set_alignment(GTK_MISC(l_pword), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_pword), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), l_pword, 0, 1, 1, 2);

	pass = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(pass), 12);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_set_can_default(pass, TRUE);
	gtk_widget_set_can_focus(pass, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), pass, 1, 2, 1, 2);

	g_signal_connect(G_OBJECT(user), "activate", G_CALLBACK(on_user_activate), pass);
	g_signal_connect(G_OBJECT(pass), "activate", G_CALLBACK(on_pass_activate), user);

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end(GTK_BOX(pan), bb, FALSE, TRUE, 0);

	GtkWidget *i;
	GtkWidget *b;

	buttons[0] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((getenv("DISPLAY") ? : "")[0] == ':') {
		if ((i = gtk_image_new_from_stock("gtk-quit", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), i);
		gtk_button_set_label(GTK_BUTTON(b), "Logout");
	} else {
		if ((i = gtk_image_new_from_stock("gtk-disconnect", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), i);
		gtk_button_set_label(GTK_BUTTON(b), "Disconnect");
	}
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_logout_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

	buttons[4] = b = gtk_button_new();
	gtk_widget_set_can_default(b, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
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

	return (pan);
}

GtkWidget *
GetPane(void)
{
	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(cont), ebox);
	gtk_widget_set_size_request(ebox, -1, -1);

	GtkWidget *v = gtk_vbox_new(FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(v), 15);

	gtk_container_add(GTK_CONTAINER(ebox), v);

	GtkWidget *lab = gtk_label_new(NULL);
	gchar *markup;

	markup = g_markup_printf_escaped
	    ("<span font=\"Liberation Sans 12\"><b><i>%s</i></b></span>", options.welcome);
	gtk_label_set_markup(GTK_LABEL(lab), markup);
	gtk_misc_set_alignment(GTK_MISC(lab), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(lab), 3, 3);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(v), lab, FALSE, TRUE, 0);

	GtkWidget *tab = gtk_table_new(1, 2, FALSE);

	gtk_table_set_col_spacings(GTK_TABLE(tab), 5);
	gtk_box_pack_end(GTK_BOX(v), tab, TRUE, TRUE, 0);

	if ((v = GetBanner()))
		gtk_table_attach_defaults(GTK_TABLE(tab), v, 0, 1, 0, 1);

	v = GetPanel();
	gtk_table_attach_defaults(GTK_TABLE(tab), v, 1, 2, 0, 1);

	return (ebox);
}

void
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
	ebox = GetPane();

	gtk_widget_show_all(cont);
	gtk_widget_show_now(cont);
	gtk_widget_grab_default(user);
	gtk_widget_grab_focus(user);
	grabbed_window(xscr->wind, NULL);
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
do_run(int argc, char *argv[], Bool replace)
{
	GetWindow();
	gtk_main();
	exit(EXIT_SUCCESS);
}

/** @brief quit the running background locker
  */
static void
do_quit(int argc, char *argv[])
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	char selection[32];

	for (s = 0; s < nscr; s++)
		get_selection(None, selection, s);
}

/** @brief ask running background locker to lock (or just lock the display)
  */
static void
do_lock(int argc, char *argv[])
{
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
    %1$s [options] [{-L|--locker}]\n\
    %1$s [options] {-r|--replace}\n\
    %1$s [options] {-l|--lock}\n\
    %1$s [options] {-q|--quit}\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] [{-L|--locker}]\n\
    %1$s [options] {-r|--replace}\n\
    %1$s [options] {-l|--lock}\n\
    %1$s [options] {-q|--quit}\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command options:\n\
   [-L, --locker]\n\
        run continuously as a screen locker instance\n\
    -r, --replace\n\
        replace a running instance with the current one\n\
    -l, --lock\n\
        ask a running instance to lock the screen now\n\
    -q, --quit\n\
        ask a running instance to quit\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -b, --banner PNGFILE\n\
        banner graphic to display\n\
        (%2$s)\n\
    -p, --prompt TEXT\n\
        text to prompt for password\n\
        (%3$s)\n\
    -n, --dry-run\n\
        do not act: only print intentions (%4$s)\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL (%5$d)\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL (%6$d)\n\
        this option may be repeated.\n\
", argv[0], options.banner, options.welcome, (options.dryrun ? "true" : "false"), options.debug, options.output);
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

void
set_default_prompt(void)
{
	char hostname[64] = { 0, };
	char *buf;
	int len;
	static char *format = "Welcome to %s!";

	free(options.welcome);
	gethostname(hostname, sizeof(hostname));
	len = strlen(format) + strnlen(hostname, sizeof(hostname)) + 1;
	buf = options.welcome = calloc(len, sizeof(*buf));
	snprintf(buf, len, format, hostname);
}

void
set_defaults(void)
{
	set_default_banner();
	set_default_prompt();
}

void
get_defaults(void)
{
	if (options.command == CommandDefault)
		options.command = CommandLocker;
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"locker",	no_argument,		NULL, 'L'},
			{"replace",	no_argument,		NULL, 'r'},
			{"lock",	no_argument,		NULL, 'l'},
			{"quit",	no_argument,		NULL, 'q'},

			{"banner",	required_argument,	NULL, 'b'},
			{"prompt",	required_argument,	NULL, 'p'},
			{"xde-theme",	no_argument,		NULL, 'u'},

			{"dry-run",	no_argument,		NULL, 'n'},
			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "b:p:unD::v::hVCH?", long_options, &option_index);
#else
		c = getopt(argc, argv, "b:p:unDvhVCH?");
#endif
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'L':	/* -L, --locker */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandLocker;
			options.command = CommandLocker;
			options.replace = False;
			break;
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

		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			break;
		case 'p':	/* -p, --prompt PROMPT */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
			break;
		case 'u':	/* -u, --xde-theme */
			options.usexde = True;
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
			exit(REMANAGE_DISPLAY);
		}
	}
	DPRINTF("%s: option index = %d\n", argv[0], optind);
	DPRINTF("%s: option count = %d\n", argv[0], argc);
	if (optind < argc) {
		fprintf(stderr, "%s: excess non-option arguments\n", argv[0]);
		goto bad_nonopt;
	}

	startup(argc, argv);
	switch (command) {
	case CommandDefault:
	case CommandLocker:
		DPRINTF("%s: running locker\n", argv[0]);
		do_run(argc, argv, False);
		break;
	case CommandReplace:
		DPRINTF("%s: running replace\n", argv[0]);
		do_run(argc, argv, True);
		break;
	case CommandQuit:
		DPRINTF("%s: running quit\n", argv[0]);
		do_quit(argc, argv);
		break;
	case CommandLock:
		DPRINTF("%s: running lock\n", argv[0]);
		do_lock(argc, argv);
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
