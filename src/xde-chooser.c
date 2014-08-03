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
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <pwd.h>
#include <systemd/sd-login.h>
#include <security/pam_appl.h>

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
	char *banner;
	char *welcome;
	CommandType command;
	char *charset;
	char *language;
	char *icon_theme;
	char *gtk2_theme;
	LogoSide side;
	Bool prompt;
	Bool noask;
	Bool setdflt;
	Bool execute;
	char *current;
	Bool managed;
	char *session;
	char *choice;
	Bool usexde;
	unsigned int timeout;
	GKeyFile *dmrc;
	char *vendor;
	char *prefix;
	char *splash;
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
	.side = LOGO_SIDE_LEFT,
	.prompt = False,
	.noask = False,
	.setdflt = False,
	.execute = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.usexde = False,
	.timeout = 15,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
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
	.side = LOGO_SIDE_LEFT,
	.prompt = False,
	.noask = False,
	.setdflt = False,
	.execute = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.usexde = False,
	.timeout = 15,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.splash = NULL,
};

typedef enum {
	ChooseResultLogout,
	ChooseResultLaunch,
} ChooseResult;

ChooseResult choose_result;

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

GHashTable *xsessions;

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

static void
xsession_key_free(gpointer data)
{
	free(data);
}

static void
xsession_value_free(gpointer entry)
{
	g_key_file_free((GKeyFile *) entry);
}

static void
get_xsession_entry(GHashTable *xsessions, const char *key, const char *file)
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
	DPRINTF("got xsession file: %s (%s)\n", key, file);
	g_hash_table_replace(xsessions, strdup(key), entry);
	return;
}

/** @brief wean out entries that should not be used
  * @param key - pointer to the application id of the entry
  * @param value - pointer to the key file entry
  * @return gboolean - TRUE if removal required, FALSE otherwise
  *
  */
static gboolean
xsessions_filter(gpointer key, gpointer value, gpointer user_data)
{
	char *appid = (char *) key;
	GKeyFile *entry = (GKeyFile *) value;
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

GHashTable *
get_xsessions(void)
{
	char **xdg_dirs, **dirs;
	int i, n = 0;
	static const char *suffix = ".desktop";
	static const int suflen = 8;

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
			get_xsession_entry(xsessions, key, file);
			free(key);
			free(file);
		}
		closedir(dir);
	}
	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
	g_hash_table_foreach_remove(xsessions, xsessions_filter, NULL);
	return (xsessions);
}

enum {
	COLUMN_PIXBUF,
	COLUMN_NAME,
	COLUMN_COMMENT,
	COLUMN_MARKUP,
	COLUMN_LABEL,
	COLUMN_MANAGED,
	COLUMN_ORIGINAL
};

GtkListStore *model;
GtkWidget *view;
GtkTreeViewColumn *cursor;

void
on_managed_toggle(GtkCellRendererToggle *rend, gchar *path, gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path)) {
		GValue user_v = G_VALUE_INIT;
		GValue orig_v = G_VALUE_INIT;
		gboolean user;
		gboolean orig;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &user_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_ORIGINAL, &orig_v);
		user = g_value_get_boolean(&user_v);
		orig = g_value_get_boolean(&orig_v);
		if (orig) {
			user = user ? FALSE : TRUE;
			g_value_set_boolean(&user_v, user);
			gtk_list_store_set_value(GTK_LIST_STORE(model), &iter, COLUMN_MANAGED,
						 &user_v);
		}
		g_value_unset(&user_v);
		g_value_unset(&orig_v);
	}
}

void
on_render_pixbuf(GtkTreeViewColumn *col, GtkCellRenderer *cell,
		 GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	GValue iname_v = G_VALUE_INIT;
	const gchar *iname;
	gchar *name = NULL, *p;
	gboolean has;
	GValue pixbuf_v = G_VALUE_INIT;

	gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, COLUMN_PIXBUF, &iname_v);
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
	XPRINTF("will try to render icon name =\"%s\"\n", name);

	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GdkPixbuf *pixbuf = NULL;

	XPRINTF("checking icon \"%s\"\n", name);
	has = gtk_icon_theme_has_icon(theme, name);
	if (has) {
		XPRINTF("tyring to load icon \"%s\"\n", name);
		pixbuf = gtk_icon_theme_load_icon(theme, name, 32,
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
			pixbuf = gtk_icon_theme_load_icon(theme, name, 32,
							  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
							  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
		}
		if (!has || !pixbuf) {
			GtkWidget *image;

			XPRINTF("tyring to load image \"%s\"\n", "gtk-missing-image");
			if ((image = gtk_image_new_from_stock("gtk-missing-image",
							      GTK_ICON_SIZE_LARGE_TOOLBAR))) {
				XPRINTF("tyring to load icon \"%s\"\n", "gtk-missing-image");
				pixbuf = gtk_widget_render_icon(GTK_WIDGET(image),
								"gtk-missing-image",
								GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
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

static void
on_logout_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
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

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);

		if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
			if ((f = fopen(file, "w"))) {
				fprintf(f, "%s\n", label ? : "");
				gtk_widget_set_sensitive(buttons[1], FALSE);
				gtk_widget_set_sensitive(buttons[2], FALSE);
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

static void
on_select_clicked(GtkButton *button, gpointer user_data)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (options.session) {
		GtkTreeIter iter;
		gboolean valid;

		for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, 0);
		     valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter,
						 COLUMN_LABEL, &label_v);
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
			     gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter))) {
				GtkTreePath *path = gtk_tree_path_new_from_string(string);

				g_free(string);
				gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(view), path, cursor,
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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &manage_v);
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

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		if (label && !strcmp(label, options.session)) {
			gtk_widget_set_sensitive(buttons[1], FALSE);
			gtk_widget_set_sensitive(buttons[2], FALSE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		} else {
			gtk_widget_set_sensitive(buttons[1], TRUE);
			gtk_widget_set_sensitive(buttons[2], TRUE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		}
		g_value_unset(&label_v);
	} else {
		gtk_widget_set_sensitive(buttons[1], FALSE);
		gtk_widget_set_sensitive(buttons[2], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
	}
}

static void
on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &manage_v);
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
on_button_press(GtkWidget *view, GdkEvent *event, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
					  event->button.x,
					  event->button.y, &path, &col, NULL, NULL)) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		gtk_tree_selection_select_path(selection, path);
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL,
						 &label_v);
			if ((label = g_value_get_string(&label_v)))
				DPRINTF("Label clicked was: %s\n", label);
#if 0
			GKeyFile *entry;

			if ((entry = (typeof(entry)) g_hash_table_lookup(xsessions, label))) {
			}
#endif
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

/** @brief create the selected session
  * @param label - the application id of the XSession
  * @param session - the desktop entry file for the XSession (or NULL)
  *
  * Launch the session specified by the label arguemtnwith the xsession desktop
  * file pass in the session argument.  This function writes the selection and
  * default to the user's current and default files in
  * $XDG_CONFIG_HOME/xde/current and $XDG_CONFIG_HOME/xde/default, sets the
  * option variables options.current and options.session.  A NULL session
  * pointer means that a logout should be performed instead.
  */
void
create_session(const char *label, GKeyFile *session)
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
	gtk_window_set_wmclass(w, "xde-chooser", "XDE-Chooser");
	gtk_window_set_title(w, "XDE XSession Chooser");
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

static void
FillView(void)
{
	{
		int status;

		status = system("xsetroot -cursor_name left_ptr");
		(void) status;
	}

	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), buttons);

	g_signal_connect(G_OBJECT(view), "row_activated", G_CALLBACK(on_row_activated), NULL);
	g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(on_button_press), NULL);

	GHashTableIter hiter;
	gpointer key, value;
	GtkTreeIter iter;

	g_hash_table_iter_init(&hiter, xsessions);
	while (g_hash_table_iter_next(&hiter, &key, &value)) {
		const gchar *label = (typeof(label)) key;
		GKeyFile *entry = (typeof(entry)) value;
		gchar *i, *n, *c, *k;
		gboolean m;

		i = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					  G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
		n = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
						 G_KEY_FILE_DESKTOP_KEY_NAME, NULL,
						 NULL) ? : g_strdup("");
		c = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
						 G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL,
						 NULL) ? : g_strdup("");
		k = g_strdup_printf("<b>%s</b>\n%s", n, c);
		m = g_key_file_get_boolean(entry, "Window Manager", "X-XDE-Managed", NULL);

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter,
				   COLUMN_PIXBUF, i,
				   COLUMN_NAME, n,
				   COLUMN_COMMENT, c,
				   COLUMN_MARKUP, k,
				   COLUMN_LABEL, label,
				   COLUMN_MANAGED, m,
				   COLUMN_ORIGINAL, m,
				   -1);
		g_free(i);
		g_free(n);
		g_free(c);
		g_free(k);

		if (!strcmp(options.choice, label) ||
		    ((!strcmp(options.choice, "choose") || !strcmp(options.choice, "default"))
		     && !strcmp(options.session, label)
		    )) {
			gchar *string;

			gtk_tree_selection_select_iter(selection, &iter);
			if ((string =
			     gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter))) {
				GtkTreePath *path = gtk_tree_path_new_from_string(string);

				g_free(string);
				gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(view), path, cursor,
								 NULL, FALSE);
				gtk_tree_path_free(path);
			}
		}
	}
	/* TODO: we should really set a timeout and if no user interaction has
	   occured before the timeout, we should continue if we have a viable
	   default or choice. */
}

GtkWidget *
GetPanel(void)
{
	GtkWidget *pan;

	pan = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(pan), 0);


	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 3);
	gtk_box_pack_start(GTK_BOX(pan), sw, TRUE, TRUE, 0);

	/* *INDENT-OFF* */
	model = gtk_list_store_new(7
			,GTK_TYPE_STRING	/* pixbuf */
			,GTK_TYPE_STRING	/* Name */
			,GTK_TYPE_STRING	/* Comment */
			,GTK_TYPE_STRING	/* Name and Comment Markup */
			,GTK_TYPE_STRING	/* Label */
			,GTK_TYPE_BOOL		/* SessionManaged?  XDE-Managed?  */
			,GTK_TYPE_BOOL		/* X-XDE-Managed original setting */
	    );
	/* *INDENT-ON* */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
					     COLUMN_MARKUP, GTK_SORT_ASCENDING);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COLUMN_NAME);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(view), GTK_TREE_VIEW_GRID_LINES_BOTH);
	gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(view));

	GtkCellRenderer *rend = gtk_cell_renderer_toggle_new();

	gtk_cell_renderer_toggle_set_activatable(GTK_CELL_RENDERER_TOGGLE(rend), TRUE);
	g_signal_connect(G_OBJECT(rend), "toggled", G_CALLBACK(on_managed_toggle), NULL);
	GtkTreeViewColumn *col;

	col = gtk_tree_view_column_new_with_attributes("Managed", rend, "active", COLUMN_MANAGED,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(col));

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
						   -1, "Icon", rend, on_render_pixbuf, NULL, NULL);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Window Manager", rend, "markup",
						       COLUMN_MARKUP, NULL);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COLUMN_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(col));
	cursor = col;

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_END);
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

	buttons[2] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Select Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_select_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

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

	FillView();

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

	markup = g_markup_printf_escaped
	    ("<span font=\"Liberation Sans 12\"><b><i>%s</i></b></span>", options.welcome);
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
	gtk_widget_grab_default(buttons[3]);
	gtk_widget_grab_focus(buttons[3]);
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

GKeyFile *
choose(int argc, char *argv[])
{
	GHashTable *xsessions;
	GKeyFile *entry = NULL;
	char *p;

	if (!(xsessions = get_xsessions())) {
		EPRINTF("cannot build XSessions\n");
		return (entry);
	}
	if (!g_hash_table_size(xsessions)) {
		EPRINTF("cannot find any XSessions\n");
		return (entry);
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
		if (!(entry = (typeof(entry)) g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s?\n", options.current);
			exit(EXIT_FAILURE);
		}
	}
	return (entry);
}

static void
do_chooser(int argc, char *argv[])
{
	GKeyFile *entry;

	if (!(entry = choose(argc, argv))) {
		DPRINTF("Logging out...\n");
		fprintf(stdout, "logout");
		return;
	}
	DPRINTF("Launching session %s...\n", options.current);
	fprintf(stdout, options.current);
	create_session(options.current, entry);
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
    %1$s [OPTIONS] [--] [SESSION]\n\
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
    -b, --banner BANNER    (%2$s)\n\
        specify custom login branding\n\
    -p, --prompt           (%3$s)\n\
        prompt for session regardless of SESSION argument\n\
    -s, --side {l|t|r|b}   (%4$s)\n\
        specify side  of dialog for logo placement\n\
    -n, --noask            (%5$s)\n\
        do not ask to set session as default\n\
    -c, --charset CHARSET  (%6$s)\n\
        specify the character set\n\
    -l, --language LANG    (%7$s)\n\
        specify the language\n\
    -d, --default          (%8$s)\n\
        set the future default to choice\n\
    -i, --icons THEME      (%15$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME      (%16$s)\n\
        set the gtk+ theme to use\n\
    -e, --exec             (%9$s)\n\
        execute the Exec= statement instead of returning as string\n\
        indicating the selected XSession\n\
    -x, --xde-theme        (%10$s)\n\
        use the XDE desktop theme for the selection window\n\
    -T, --timeout SECONDS  (%11$u sec)\n\
        set dialog timeout\n\
    -N, --dry-run          (%12$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]    (%13$d)\n\
        increment or set debug LEVEL\n\
    -v, --verbose [LEVEL]  (%14$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
", argv[0], options.banner, show_bool(options.prompt), show_side(options.side), show_bool(options.noask), options.charset, options.language, show_bool(options.setdflt), show_bool(options.execute), show_bool(options.usexde), options.timeout, show_bool(options.dryrun), options.debug, options.output, options.usexde ? "xde" : (options.gtk2_theme ? : "auto"), options.usexde ? "xde" : (options.icon_theme ? : "auto"), options.choice, options.session ? : "", options.current ? : "");
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
		len = strlen(here) + strlen("/xdg/xde:")
		    + strlen(here) + strlen("/xdg:") + strlen(confdir);
		conf = calloc(len + 1, sizeof(*conf));
		strncpy(conf, here, len);
		strncat(conf, "/xdg/xde:", len);
		strncat(conf, here, len);
		strncat(conf, "/xdg:", len);
		strncat(conf, confdir, len);

		len = strlen(here) + strlen("/share/xde:")
		    + strlen(here) + strlen("/share:") + strlen(datadir);
		data = calloc(len + 1, sizeof(*data));
		strncpy(data, here, len);
		strncat(data, "/share/xde:", len);
		strncat(data, here, len);
		strncat(data, "/share:", len);
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
	set_default_session();
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
		options.session = defaults.session;
	}
	if (!options.current) {
		free(options.current);
		options.current = defaults.current;
	}
}

void
get_default_choice(void)
{
	if (!options.choice) {
		free(options.choice);
		options.choice = defaults.choice;
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
	get_default_session();
	get_default_choice();
}

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	set_defaults(argc, argv);

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"prompt",	no_argument,		NULL, 'p'},
			{"banner",	required_argument,	NULL, 'b'},
			{"splash",	required_argument,	NULL, 'S'},
			{"side",	required_argument,	NULL, 's'},
			{"noask",	no_argument,		NULL, 'n'},
			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'l'},
			{"default",	no_argument,		NULL, 'd'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"exec",	no_argument,		NULL, 'e'},
			{"xde-theme",	no_argument,		NULL, 'x'},
			{"timeout",	required_argument,	NULL, 'T'},
			{"vendor",	required_argument,	NULL, '5'},

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

		c = getopt_long_only(argc, argv, "pb:s:nc:l:di:t:exT:ND::v::hVCH?", long_options,
				     &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "pb:s:nc:l:di:t:exT:NDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'p':	/* -p, --prompt */
			options.prompt = True;
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
			exit(2);
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
