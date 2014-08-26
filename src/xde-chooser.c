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
#include <X11/Xdmcp.h>
#include <X11/SM/SMlib.h>
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
#define DO_CHOOSER 1
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
	char *banner;
	char *welcome;
	CommandType command;
	char *charset;
	char *language;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	LogoSide side;
	Bool prompt;
	Bool noask;
	Bool setdflt;
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
	float xposition;
	float yposition;
	Bool setstyle;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.banner = NULL,
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.prompt = False,
	.noask = False,
	.setdflt = False,
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
	.source = BackgroundSourceSplash,
	.xsession = True,
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
	.banner = NULL,
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.prompt = False,
	.noask = False,
	.setdflt = False,
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
	.source = BackgroundSourceSplash,
	.xsession = True,
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
	ChooseResultLogout,
	ChooseResultLaunch,
} ChooseResult;

ChooseResult choose_result;

static SmcConn smcConn;

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
		break;
	}
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

static GtkWidget *buttons[5];

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

GtkListStore *store;			/* list store for XSessions */
GtkWidget *sess;

#define XDE_ICON_GEOM 32
#define XDE_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR

void
on_render_pixbuf(GtkTreeViewColumn *col, GtkCellRenderer *cell,
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
#if 0
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					     GTK_SORT_ASCENDING);
#endif

#if 1
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

void
on_managed_toggle(GtkCellRendererToggle *rend, gchar *path, gpointer data)
{
	GtkListStore *store = GTK_LIST_STORE(data);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path)) {
		GValue user_v = G_VALUE_INIT;
		GValue orig_v = G_VALUE_INIT;
		gboolean user;
		gboolean orig;

		gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, XSESS_COL_MANAGED, &user_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, XSESS_COL_ORIGINAL, &orig_v);
		user = g_value_get_boolean(&user_v);
		orig = g_value_get_boolean(&orig_v);
		if (orig) {
			user = user ? FALSE : TRUE;
			g_value_set_boolean(&user_v, user);
			gtk_list_store_set_value(GTK_LIST_STORE(store), &iter,
						 XSESS_COL_MANAGED, &user_v);
		}
		g_value_unset(&user_v);
		g_value_unset(&orig_v);
	}
}

static void
on_logout_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		g_value_unset(&label_v);
	}
	free(options.current);
	options.current = strdup("logout");
	options.managed = False;
	choose_result = ChooseResultLogout;
	gtk_main_quit();
}

static void
on_default_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		const gchar *label;
		char *home = getenv("HOME") ? : ".";
		char *xhome = getenv("XDG_CONFIG_HOME");
		char *cdir, *file;
		int len, dlen, flen;
		FILE *f;

		if (xhome) {
			len = strlen(xhome);
		} else {
			len = strlen(home);
			len += strlen("/.config");
		}
		dlen = len + strlen("/xde");
		flen = dlen + strlen("/default");
		cdir = calloc(dlen, sizeof(*cdir));
		file = calloc(flen, sizeof(*file));
		if (xhome) {
			strcpy(cdir, xhome);
		} else {
			strcpy(cdir, home);
			strcat(cdir, "/.config");
		}
		strcat(cdir, "/xde");
		strcpy(file, cdir);
		strcat(file, "/default");

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);

		if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
			if ((f = fopen(file, "w"))) {
				fprintf(f, "%s\n", label ? : "");
				gtk_widget_set_sensitive(buttons[1], FALSE);
				// gtk_widget_set_sensitive(buttons[2], FALSE);
				gtk_widget_set_sensitive(buttons[3], TRUE);
				fclose(f);
			}
		}
		if (label && label[0] && options.dmrc) {
			char *dmrc;

			len = strlen(home) + strlen("/.dmrc");
			dmrc = calloc(len + 1, sizeof(*dmrc));
			strncpy(dmrc, home, len);
			strncat(dmrc, "/.dmrc", len);

			g_key_file_set_string(options.dmrc, "Desktop", "Session", label);
			g_key_file_save_to_file(options.dmrc, dmrc, NULL);

			free(dmrc);
		}

		g_value_unset(&label_v);
		free(file);
		free(cdir);
	}
}

void
on_select_clicked(GtkButton *button, gpointer data)
{
	GtkTreeSelection *selection;
	GtkListStore *store = GTK_LIST_STORE(data);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (options.session) {
		GtkTreeIter iter;
		gboolean valid;

		for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0);
		     valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter,
						 XSESS_COL_LABEL, &label_v);
			label = g_value_get_string(&label_v);
			if (!strcmp(label, options.session)) {
				g_value_unset(&label_v);
				break;
			}
			g_value_unset(&label_v);
		}
		if (valid) {
			gchar *string;

			gtk_tree_selection_select_iter(GTK_TREE_SELECTION(selection), &iter);
			if ((string =
			     gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(store), &iter))) {
				GtkTreePath *path = gtk_tree_path_new_from_string(string);
				GtkTreeViewColumn *cursor =
				    gtk_tree_view_get_column(GTK_TREE_VIEW(sess), 1);

				g_free(string);
				gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(sess), path, cursor,
								 NULL, FALSE);
				gtk_tree_path_free(path);
			}
		}
	} else {
		gtk_tree_selection_unselect_all(selection);
	}
}

static void
on_launch_clicked(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		gtk_tree_model_get_value(model, &iter, XSESS_COL_MANAGED, &manage_v);
		label = g_value_get_string(&label_v);
		manage = g_value_get_boolean(&manage_v);
		free(options.current);
		options.current = strdup(label);
		options.managed = manage;
		choose_result = ChooseResultLaunch;
		gtk_main_quit();
	}
}

static void
on_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		if (label && !strcmp(label, options.session)) {
			gtk_widget_set_sensitive(buttons[1], FALSE);
			// gtk_widget_set_sensitive(buttons[2], FALSE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		} else {
			gtk_widget_set_sensitive(buttons[1], TRUE);
			// gtk_widget_set_sensitive(buttons[2], TRUE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		}
		g_value_unset(&label_v);
	} else {
		gtk_widget_set_sensitive(buttons[1], FALSE);
		// gtk_widget_set_sensitive(buttons[2], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
	}
}

static void
on_row_activated(GtkTreeView *sess, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		gtk_tree_model_get_value(model, &iter, XSESS_COL_MANAGED, &manage_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		manage = g_value_get_boolean(&manage_v);
		free(options.current);
		options.current = strdup(label);
		options.managed = manage;
		choose_result = ChooseResultLaunch;
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
		gtk_main_quit();
	}
}

static gboolean
on_button_press(GtkWidget *sess, GdkEvent *event, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(sess),
					  event->button.x,
					  event->button.y, &path, &col, NULL, NULL)) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
		gtk_tree_selection_select_path(selection, path);
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
			if ((label = g_value_get_string(&label_v)))
				DPRINTF("Label clicked was: %s\n", label);
			g_value_unset(&label_v);
		}
	}
	return FALSE;		/* propagate event */
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	free(options.current);
	options.current = strdup("logout");
	options.managed = False;
	choose_result = ChooseResultLogout;
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
		float xrel, yrel;

		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (float) (mon->geom.x + mon->geom.width * options.xposition) / (float) xscr->width;
		yrel = (float) (mon->geom.x + mon->geom.height * options.yposition) / (float) xscr->height;
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
		float xrel, yrel;

		mon->index = m;
		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (float) (mon->geom.x + mon->geom.width * options.xposition) / (float) xscr->width;
		yrel = (float) (mon->geom.x + mon->geom.height * options.yposition) / (float) xscr->height;
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

GtkWidget *
GetPanel(void)
{
	GtkWidget *pan = gtk_vbox_new(FALSE, 0);
	GtkShadowType shadow = (options.transparent) ? GTK_SHADOW_NONE : GTK_SHADOW_ETCHED_IN;

	gtk_container_set_border_width(GTK_CONTAINER(pan), 0);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), shadow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 3);
	gtk_box_pack_start(GTK_BOX(pan), sw, TRUE, TRUE, 0);

	/* *INDENT-OFF* */
	store = gtk_list_store_new(9
			,G_TYPE_STRING	/* icon */
			,G_TYPE_STRING	/* Name */
			,G_TYPE_STRING	/* Comment */
			,G_TYPE_STRING	/* Name and Comment Markup */
			,G_TYPE_STRING	/* Label */
			,G_TYPE_BOOLEAN	/* SessionManaged?  XDE-Managed?  */
			,G_TYPE_BOOLEAN	/* X-XDE-managed original setting */
			,G_TYPE_STRING	/* the file name */
			,G_TYPE_STRING	/* tooltip */
	    );
	/* *INDENT-ON* */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     XSESS_COL_MARKUP, GTK_SORT_ASCENDING);
	sess = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_widget_set_can_default(sess, TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(sess), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(sess), XSESS_COL_NAME);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(sess), XSESS_COL_TOOLTIP);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sess), FALSE);
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_GRID_LINES_BOTH);
	gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(sess));

	g_signal_connect(G_OBJECT(sess), "row_activated", G_CALLBACK(on_row_activated), NULL);
	g_signal_connect(G_OBJECT(sess), "button_press_event", G_CALLBACK(on_button_press), NULL);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), buttons);

	GtkCellRenderer *rend = gtk_cell_renderer_toggle_new();

	gtk_cell_renderer_toggle_set_activatable(GTK_CELL_RENDERER_TOGGLE(rend), TRUE);
	g_signal_connect(G_OBJECT(rend), "toggled", G_CALLBACK(on_managed_toggle), store);
	GtkTreeViewColumn *col;

	col = gtk_tree_view_column_new_with_attributes("Managed", rend, "active", XSESS_COL_MANAGED,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_COLUMN(col));

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(sess),
						   -1, "Icon", rend, on_render_pixbuf, NULL, NULL);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Window Manager", rend, "markup",
						       XSESS_COL_MARKUP, NULL);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), XSESS_COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_COLUMN(col));

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

	buttons[1] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-save", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Make Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_default_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

#if 0
	buttons[2] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Select Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_select_clicked), store);
	gtk_widget_set_sensitive(b, TRUE);
#endif

	buttons[3] = b = gtk_button_new();
	gtk_widget_set_can_default(b, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-ok", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Launch Session");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_launch_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

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

	GtkWidget *l_greet = gtk_label_new(NULL);
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

#if 0
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
do_run(int argc, char *argv[])
{
	startup(argc, argv);
	top = GetWindow(False);
	gtk_main();
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

const char *
choose(int argc, char *argv[])
{
	GHashTable *xsessions;
	char *file = NULL;
	char *p;

	/* initialize session managerment functions */
	init_smclient();

	if (!(xsessions = get_xsessions())) {
		EPRINTF("cannot build XSessions\n");
		return (file);
	}
	if (!g_hash_table_size(xsessions)) {
		EPRINTF("cannot find any XSessions\n");
		return (file);
	}
	if (!options.choice)
		options.choice = strdup("default");
	for (p = options.choice; p && *p; p++)
		*p = tolower(*p);
	if (!strcasecmp(options.choice, "default") && options.session) {
		free(options.choice);
		options.choice = strdup(options.session);
	} else if (!strcasecmp(options.choice, "current") && options.current) {
		free(options.choice);
		options.choice = strdup(options.current);
	}

	if (options.session && !g_hash_table_contains(xsessions, options.session)) {
		DPRINTF("Default %s is not available!\n", options.session);
		if (!strcasecmp(options.choice, options.session)) {
			free(options.choice);
			options.choice = strdup("choose");
		}
	}
	if (!strcasecmp(options.choice, "default") && !options.session) {
		DPRINTF("Default is chosen but there is no default\n");
		free(options.choice);
		options.choice = strdup("choose");
	}
	if (strcasecmp(options.choice, "choose") &&
	    !g_hash_table_contains(xsessions, options.choice)) {
		DPRINTF("Choice %s is not available.\n", options.choice);
		free(options.choice);
		options.choice = strdup("choose");
	}
	if (!strcasecmp(options.choice, "choose"))
		options.prompt = True;

	DPRINTF("The default was: %s\n", options.session);
	DPRINTF("Choosing %s...\n", options.choice);
	if (options.prompt)
		do_run(argc, argv);
	if (strcmp(options.current, "logout")) {
		if (!(file = g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s?\n", options.current);
			exit(EXIT_FAILURE);
		}
	}
	return (file);
}

static void
do_chooser(int argc, char *argv[])
{
	const char *file;
	char *out;
	int i;

	if (!(file = choose(argc, argv))) {
		DPRINTF("Logging out...\n");
		fprintf(stdout, "logout");
		return;
	}
	DPRINTF("Launching session %s...\n", options.current);
	out = strdup(options.current);
	for (i = 0; i < strlen(out); i++)
		out[i] = tolower(out[i]);
	fprintf(stdout, out);
	free(out);
	create_session(options.current, file);
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
    %1$s [options] [--] [SESSION]\n\
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
    %1$s [OPTIONS] [--] [SESSION]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    SESSION                (%2$s)\n\
        The name of the XDG session to execute, or \"default\"\n\
        or \"choose\".  When unspecified defaults to \"default\"\n\
        \"default\" means execute default without prompting\n\
        \"choose\" means post a dialog to choose the session\n\
	[default: %3$s] [current: %4$s]\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -b, --banner BANNER    (%5$s)\n\
        specify custom login branding\n\
    -S, --splash SPLASH    (%6$s)\n\
        background image to display\n\
    -p, --prompt           (%7$s)\n\
        prompt for session regardless of SESSION argument\n\
    -s, --side {l|t|r|b}   (%8$s)\n\
        specify side  of dialog for logo placement\n\
    -n, --noask            (%9$s)\n\
        do not ask to set session as default\n\
    -c, --charset CHARSET  (%10$s)\n\
        specify the character set\n\
    -l, --language LANG    (%11$s)\n\
        specify the language\n\
    -d, --default          (%12$s)\n\
        set the future default to choice\n\
    -e, --exec             (%13$s)\n\
        execute the Exec= statement instead of returning as string\n\
        indicating the selected XSession\n\
    -i, --icons THEME      (%14$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME      (%15$s)\n\
        set the gtk+ theme to use\n\
    -x, --xde-theme        (%16$s)\n\
        use the XDE desktop theme for the selection window\n\
    -T, --timeout SECONDS  (%17$u sec)\n\
        set dialog timeout\n\
    --vendor VENDOR        (%18$s)\n\
        vendor identifier for branding\n\
    -N, --dry-run          (%19$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]    (%20$d)\n\
        increment or set debug LEVEL\n\
    -v, --verbose [LEVEL]  (%21$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
"	,argv[0]
	,options.choice
	,options.session ? : ""
	,options.current ? : ""
	,options.banner
	,options.splash
	,show_bool(options.prompt)
	,show_side(options.side)
	,show_bool(options.noask)
	,options.charset
	,options.language
	,show_bool(options.setdflt)
	,show_bool(options.execute)
	,options.usexde ? "xde" : (options.gtk2_theme ? : "auto")
	,options.usexde ? "xde" : (options.icon_theme ? : "auto")
	,show_bool(options.usexde)
	,options.timeout
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
getXrmFloat(const char *val, float *floating)
{
	*floating = strtod(val, NULL);
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
	XrmCombineFileDatabase(APPDFLT, &rdb, False);
	if ((val = get_resource(rdb, "debug", "0"))) {
		getXrmInt(val, &options.debug);
	}
	if ((val = get_xlogin_resource(rdb, "width", NULL))) {
		if (strchr(val, '%')) {
			char *endptr = NULL;
			float width = strtod(val, &endptr);

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
			float height = strtod(val, &endptr);

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
		    (float) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
		if (options.xposition < 0)
			options.xposition = 0;
		if (options.xposition > DisplayWidth(dpy, 0))
			options.xposition = 1.0;
	}
	if ((val = get_xlogin_resource(rdb, "y", NULL))) {
		options.yposition =
		    (float) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
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
		getXrmFloat(val, &options.xposition);
	}
	if ((val = get_resource(rdb, "Chooser.y", NULL))) {
		getXrmFloat(val, &options.yposition);
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

#if 1
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
#if 1
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
	get_default_session();
	get_default_choice();
	get_default_username();
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	saveArgc = argc;
	saveArgv = argv;

	set_defaults(argc, argv);

	get_resources(argc, argv);

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"prompt",	    no_argument,	NULL, 'p'},
			{"welcome",	    required_argument,	NULL, 'w'},
			{"banner",	    required_argument,	NULL, 'b'},
			{"splash",	    required_argument,	NULL, 'S'},
			{"side",	    required_argument,	NULL, 's'},
			{"noask",	    no_argument,	NULL, 'n'},
			{"charset",	    required_argument,	NULL, 'c'},
			{"language",	    required_argument,	NULL, 'l'},
			{"default",	    no_argument,	NULL, 'd'},
			{"icons",	    required_argument,	NULL, 'i'},
			{"theme",	    required_argument,	NULL, 't'},
			{"exec",	    no_argument,	NULL, 'e'},
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

		c = getopt_long_only(argc, argv, "pb:S:s:nc:l:di:t:exT:ND::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "pb:s:nc:l:di:t:exT:NDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'p':	/* -p, --prompt */
			options.prompt = True;
			break;
		case 'w':	/* -w, --welcome WELCOME */
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
		case 'n':	/* -n, --noask */
			options.noask = True;
			break;
		case 'c':	/* -c --charset CHARSET */
			free(options.charset);
			options.charset = strdup(optarg);
			break;
		case 'l':	/* -l, --language LANG */
			free(options.language);
			options.language = strdup(optarg);
			break;
		case 'd':	/* -d, --default */
			options.setdflt = True;
			break;
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 'e':	/* -e, --exec */
			options.execute = True;
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

		case 'N':	/* -N, --dry-run */
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
		free(options.choice);
		options.choice = strdup(argv[optind++]);
		if (optind < argc) {
			fprintf(stderr, "%s: too many arguments\n", argv[0]);
			goto bad_nonopt;
		}
	}
	get_defaults(argc, argv);
	switch (command) {
	default:
	case CommandDefault:
		DPRINTF("%s: running chooser\n", argv[0]);
		do_chooser(argc, argv);
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
