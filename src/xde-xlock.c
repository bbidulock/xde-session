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
#include <X11/Xdmcp.h>
#include <X11/SM/SMlib.h>
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

typedef enum {
	CommandDefault,
	CommandLocker,			/* run as a background locker */
	CommandReplace,			/* replace any running instance */
	CommandLock,			/* ask running instance to lock */
	CommandQuit,			/* ask running instance to quit */
	CommandHelp,			/* command argument help */
	CommandVersion,			/* command version information */
	CommandCopying,			/* command copying information */
} Command;

int xssEventBase;
int xssErrorBase;
int xssMajorVersion;
int xssMinorVersion;

XScreenSaverInfo xssInfo;

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

void
setup_screensaver(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Status status;
	Bool present;
	const char *val;

	present = XScreenSaverQueryExtension(dpy, &xssEventBase, &xssErrorBase);
	if (!present) {
		DPRINT("no MIT-SCREEN-SAVER extension present\n");
		return;
	}
	status = XScreenSaverQueryVersion(dpy, &xssMajorVersion, &xssMinorVersion);
	if (!status) {
		EPRINTF("cannot query MIT-SCREEN-SAVER version\n");
		return;
	}
	DPRINTF("MIT-SCREEN-SAVER Extension %d:%d\n", xssMajor, xssMinor);

	status = XScreenSaverQueryInfo(dpy, FIXME, &xssInfo);
	if (!status) {
		EPRINTF("cannot query MIT-SCREEN-SAVER info\n");
		return;
	}

}

static GdkFilterReturn
handle_XScreenSaverNotify(Display *dpy, XEvent *xev, XdeScreen * xscr)
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
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	// logout_result = LOGOUT_ACTION_CANCEL;
	gtk_main_quit();
	return TRUE;		/* propagate */
}

gboolean
on_expose_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GdkPixbuf *p = GDK_PIXBUF(data);
	GdkWindow *w = gtk_widget_get_window(GTK_WIDGET(widget));
	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(w));

	gdk_cairo_set_source_pixbuf(cr, p, 0, 0);
	cairo_paint(cr);
	GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };
	gdk_cairo_set_source_color(cr, &color);
	cairo_paint_with_alpha(cr, 0.7);
	return FALSE;
}

/** @brief transform a window into a grabbed window
  *
  * Transform the a window into a window that has a grab on the pointer on a
  * GtkWindow and restricts pointer movement to the window boundary.
  *
  * Note that the window and pointer grabs should not fail: the reason is that
  * we have mapped this window above all others and a fully obscured window
  * cannot hold the pointer or keyboard focus in X.
  */
void
grabbed_window(GtkWindow *window)
{
	GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(window));
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
ungrabbed_window(GtkWindow *window)
{
	GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(window));

	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_window_hide(win);
}

typedef struct {
	gint index;
	GdkRectangle geom;
} XdeMonitor;

typedef struct {
	GdkDisplay *disp;
	GdkScreen *srcn;		/* screen */
	GdkWindow *root;		/* root window of screen */
	gint width;			/* width of root window */
	gint height;			/* height of root window */
	gint index;			/* screen index */
	GtkWidget *wind;		/* lock window for screen */
	GtkWidget *ebox;		/* lock event box for monitor */
	gint primary;			/* primary monitor index */
	gint nmon;			/* number of monitors */
	XdeMonitor *mons;		/* monitors for this screen */
	Imlib_Context *context;		/* imlib context for image operations */
	Pixmap pmap;			/* background pixmap */
} XdeScreen;

XdeMonitor *screens;

void
FillBackground(XdeScreen *scr, XdeMonitor *mon)
{
}

Pixmap
NewPixmap(XdeScreen * scr)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY(scr->disp);
	Window root = GDK_WINDOW_XID(scr->root);
	Visual vis;
	Colormap cm;
	int scrn = scr->index;
	int depth;

	vis = DefaultVisual(dpy, scrn);
	cm = DefaultColormap(dpy, scrn);
	depth = DefaultDepth(dpy, scrn);

	scr->pmap = XCreatePixmap(dpy, root, scr->width, scr->height, depth);

	scr->context = imlib_context_new();
	imlib_context_push(scr->context);
	imlib_context_set_display(dpy);
	imlib_context_set_visual(DefaultVisual(dpy, scrn));
	imlib_context_set_colormap(DefaultColormap(dpy, scrn));
	imlib_context_set_drawable(scr->pmap);

	imlib_context_set_color_range(imlib_create_color_range());
	imlib_context_set_image(imlib_create_image(scr->width, scr->height));
	imlib_context_set_color(0, 0, 0, 255);
	imilb_image_fill_rectangle(0, 0, width, height);
	imlib_context_set_dither(1);
	imlib_context_set_blend(1);

	return None;
}

Pixmap
GetPixmap(XdeScreen * scr)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY(scr->disp);
	Window root = GDK_WINDOW_XID(scr->root);
	Atom prop = XInternAtom(dpy, "_XROOTPMAP_ID", True);
	Atom actual;
	int format = 0;
	unsigned long nitems = 0, after;
	unsigned long *data = NULL;
	Pixmap pmap = None;

	if (!prop)
		return NewPixmap(scr);

	if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_PIXMAP, &actual, &format,
			       &nitems, &after, (unsigned char **) &data) == Success &&
	    format == 32 && nitems >= 1 && data) {
		pmap = data[0];
	}
	if (data)
		XFree(data);
	if (!pmap)
		return NewPixmap(scr);
	return (pmap);
}

void
GetScreens(void)
{
	gint s, m;
	XdeScreen *scr;
	XdeMonitor *mon;

	GdkDisplay *disp = gdk_display_get_default();
	gint nscrn = gdk_display_get_n_screens(disp);

	screens = calloc(nscrn, sizeof(*screens));

	for (scr = screens, n = 0; n < nscrn; n++, scr++) {
		Pixmap pmap;
		scr->disp = disp;
		scr->scrn = gdk_display_get_screen(disp, n);
		scr->root = gdk_screen_get_root_window(scrn);
		scr->width = gdk_screen_get_width(scrn);
		scr->height = gdk_screen_get_height(scrn);
		scr->index = n;
		scr->primary = gdk_screen_get_primary_monitor(scrn);
		scr->nmon = gdk_screen_get_n_monitors(scrn);
		scr->mons = calloc(scr->nmon, sizeof(*scr->mons));
		pmap = GetPixmap(scr);
		for (mon = scr->mons, m = 0; m < scr->nmon; m++, mon++) {
			mon->index = m;
			gdk_screen_get_monitor_geometry(scr->scrn, m, &mon->geom);
			if (!pmap)
				FillBackground(scr, mon);
		}
	}
}

/** @brief get a pixbuf for the screen
  *
  * This pixbuf can either be the background image, if one is defined in
  * _XROOTPMAP_ID or ESETROOT_PMAP_ID, or an copy of the root window otherwise.
  */
GdkPixbuf *
get_pixbuf(GdkScreen * scrn)
{
	GdkDisplay *disp = gdk_screen_get_display(scrn);
	GdkWindow *root = gdk_screen_get_root_window(scrn);
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	gint width = gdk_screen_get_width(scrn);
	gint height = gdk_screen_get_height(scrn);
	GdkDrawable *draw = GDK_DRAWABLE(root);
	GdkColormap *cmap = gdk_drawable_get_colormap(draw);
	Atom prop = None;

	if (!(prop = XInternAtom(dpy, "_XROOTPMAP_ID", True)))
		prop = XInternAtom(dpy, "ESETROOT_PMAP_ID", True);

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
				GdkPixmap *pmap;
				
				pmap = gdk_pixmap_foreign_new_for_display(disp, p);
				gdk_pixmap_get_size(pmap, &width, &height);
				draw = GDK_DRAWABLE(pmap);
			}
		}
		if (data)
			XFree(data);
	}
	GdkPixbuf *pbuf = gdk_pixbuf_get_from_drawable(NULL, draw, cmap, 0, 0,
			0, 0, width, height);
	return (pbuf);
}

GtkWidget *wind; /* screen sized window */
GtkWidget *ebox; /* event box window within the screen */

void
GetWindow(void)
{
	char hostname[64] = { 0, };

	wind = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(wind), "delete-event", G_CALLBACK(on_delete_event), NULL);
	gtk_window_set_wmclass(GTK_WINDOW(wind), "xde-xlock", "XDE-XLock");
	gtk_window_set_title(GTK_WINDOW(wind), "XLock");
	gtk_window_set_modal(GTK_WINDOW(wind), TRUE);
	gtk_window_set_gravity(GTK_WINDOW(wind), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(wind), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_window_set_icon_name(GTK_WINDOW(wind), "xdm");
	gtk_container_set_border_width(GTK_CONTAINER(wind), 15);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(wind), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(wind), TRUE);
	gtk_window_set_position(GTK_WINDOW(wind), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_fullscreen(GTK_WINDOW(wind));
	gtk_window_set_decorated(GTK_WINDOW(wind), FALSE);

	GdkScreen *scrn = gdk_screen_get_default();
	gint width = gdk_screen_get_width(scrn);
	gint height = gdk_screen_get_height(scrn);

	char *geom = g_strdup_printf("%dx%d+0+0", width, height);

	gtk_window_parse_geometry(GTK_WINDOW(wind), geom);
	g_free(geom);

	gtk_window_set_default_size(GTK_WINDOW(wind), width, height);
	gtk_widget_set_app_paintable(wind, TRUE);

	GdkPixbuf *pixbuf = get_pixbuf(scrn);

	g_signal_connect(G_OBJECT(wind), "destroy", G_CALLBACK(on_destroy), NULL);

	/* Ultimately this has to be a GtkFixed instead of a GtkAlignment,
	   because we need to place the window in the center of the active
	   monitor. */
	GtkWidget *a = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

	gtk_container_add(GTK_CONTAINER(wind), GTK_WIDGET(a));

	gethostname(hostname, sizeof(hostname));

	ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(a), ebox);
	gtk_widget_set_size_request(ebox, -1, -1);
	g_signal_connect(G_OBJECT(wind), "expose-event", G_CALLBACK(on_expose_event), pixbuf);

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

	GtkWidget *h = gtk_hbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(v), h);

	if (options.banner) {
		GtkWidget *f = gtk_frame_new(NULL);

		gtk_frame_set_shadow_type(GTK_FRAME(f), GTK_SHADOW_ETCHED_IN);
		gtk_box_pack_start(GTK_BOX(h), GTK_WIDGET(f), FALSE, FALSE, 0);

		GtkWidget *v = gtk_vbox_new(FALSE, 5);

		gtk_container_set_border_width(GTK_CONTAINER(v), 10);
		gtk_contianer_add(GTK_CONTAINER(f), v);

		GtkWidget *s = gtk_image_new_from_file(options.banner);

		if (s)
			gtk_container_add(GTK_CONTAINER(v), s);
		else
			gtk_widget_destroy(f);
	}

	GtkWidget *f = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(f), GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_start(GTK_BOX(h), f, TRUE, TRUE, 0);

	v = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(v), 10);
	gtk_container_add(GTK_CONTAINER(f), v);

	a = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(v), a);

	GtkWidget *t = gtk_table_new(3, 1, FALSE);

	gtk_table_set_col_spacings(GTK_TABLE(t), 5);
	gtk_container_add(GTK_CONTAINER(a), t);

	GtkWidget *l = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(l),
			     "<span font=\"Liberation Sans 9\"><b>Password:</b></span>");
	gtk_misc_set_alignment(GTK_MISC(l), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(l), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(t), l, 0, 1, 0, 1);

	GtkWidget *i = gtk_entry_new();

	gtk_table_attach_defaults(GTK_TABLE(t), i, 0, 1, 1, 2);

	GtkWidget *s = gtk_label_new(NULL);

	gtk_misc_set_alignment(GTK_MISC(s), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(s), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(t), i, 0, 1, 2, 3);

	// gtk_window_set_default_size(GTK_WINDOW(ebox), -1, 300)

	/* most of this is just in case override-reidrect fails */
	gtk_window_set_focus_on_map(GTK_WINDOW(wind), TRUE);
	gtk_window_set_accept_focus(GTK_WINDOW(wind), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(wind), TRUE);
	gtk_window_set_modal(GTK_WINDOW(wind), TRUE);
	gtk_window_stick(GTK_WINDOW(wind));
	gtk_window_deiconify(GTK_WINDOW(wind));
	gtk_widget_show_all(wind);


	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);

	gdk_window_set_override_redirect(win, TRUE);
	grabbed_window(GTK_WINDOW(wind));

	// gtk_widget_grab_default(user);
	// gtk_widget_grab_focus(user);

}

/** @brief run as a background locker
  */
static void
run_locker(int argc, char *argv[])
{
	gtk_init(NULL, NULL);
	GetWindow();
	gtk_main();
	exit(EXIT_SUCCESS);
}

/** @brief replace the running background locker
  */
static void
run_replace(int argc, char *argv[])
{
}

/** @brief quit the running background locker
  */
static void
run_quit(int argc, char *argv[])
{
}

/** @brief ask running background locker to lock (or just lock the display)
  */
static void
run_lock(int argc, char *argv[])
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
    %1$s [options]\n\
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
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] ADDRESS [...]\n\
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
General options:\n\
    -b, --banner PNGFILE\n\
        banner graphic to display\n\
	(%2$s)\n\
    -p, --promp TEXT\n\
        text to prompt for password\n\
	(%3$s)\n\
    -n, --dry-run\n\
        do not act: only print intentions (%4$s)\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL (%5$d)\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL (%6$d)\n\
        this option may be repeated.\n\
", argv[0], options.banner, options.prompt, (options.dryrun ? "true" : "false"), options.debug, options.output);
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
		static stuct option long_options[] = {
			{"locker",	no_argument,		NULL, 'L'},
			{"replace",	no_argument,		NULL, 'r'},
			{"lock",	no_arugment,		NULL, 'l'},
			{"quit",	no_argument,		NULL, 'q'},

			{"banner",	required_argument,	NULL, 'b'},
			{"prompt",	required_argument,	NULL, 'p'},

			{"locker",	no_argument,		NULL, 'L'},
			{"replace",	no_argument,		NULL, 'r'},
			{"lock",	no_arugment,		NULL, 'l'},
			{"quit",	no_argument,		NULL, 'q'},

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

		c = getopt_long_only(argc, argv, "b:p:nD::v::hVCH?", long_options, &option_index);
#else
		c = getopt(argc, argv, "b:p:nDvhVCH?");
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
			break;
		case 'r':	/* -r, --replace */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandReplace;
			options.command = CommandReplace;
			break;
		case 'q':	/* -q, --quit */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandQuit;
			options.command = CommandQuit;
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
			free(options.prompt);
			options.prompt = strdup(optarg);
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

	switch (command) {
	case CommandDefault:
	case CommandLocker:
		DPRINTF("%s: running locker\n", argv[0]);
		run_locker(argc, argv);
		break;
	case CommandReplace:
		DPRINTF("%s: running replace\n", argv[0]);
		run_replace(argc, argv);
		break;
	case CommandQuit:
		DPRINTF("%s: running quit\n", argv[0]);
		run_quit(argc, argv);
		break;
	case CommandLock:
		DPRINTF("%s: running lock\n", argv[0]);
		run_lock(argc, argv);
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
