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

enum {
	OBEYSESS_DISPLAY, /* obey multipleSessions resource */
	REMANAGE_DISPLAY, /* force remanage */
	UNMANAGE_DISPLAY, /* force deletion */
	RESERVER_DISPLAY, /* force server termination */
	OPENFAILED_DISPLAY, /* XOpenDisplay failed, retry */
};

typedef enum {
	CommandDefault,
	CommandXlogin,
	CommandHelp,
	CommandVersion,
	CommandCopying,
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
} Options;

typedef enum {
	LoginStateInit,
	LoginStateUsername,
	LoginStatePassword,
	LoginStateReady,
} LoginState;

LoginState state = LoginStateInit;

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
};

typedef enum {
	CHOOSE_RESULT_LOGOUT,
	CHOOSE_RESULT_LAUNCH,
} ChooseResult;

ChooseResult choose_result;

enum {
	COLUMN_PIXBUF,
	COLUMN_NAME,
	COLUMN_COMMENT,
	COLUMN_MARKUP,
	COLUMN_LABEL,
	COLUMN_MANAGED,
	COLUMN_ORIGINAL,
	COLUMN_FILENAME,
	COLUMN_KEYFILE
};

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
xsession_value_free(gpointer filename)
{
	free(filename);
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
		case PAM_PROMPT_ECHO_ON:	/* obtain a string whilst echoing */
		{
			gchar *prompt = g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>", (*m)->msg);
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
		case PAM_PROMPT_ECHO_OFF:	/* obtain a string without echoing */
		{
			gchar *prompt = g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>", (*m)->msg);
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
		case PAM_ERROR_MSG:		/* display an error message */
		{
			gchar *prompt = g_strdup_printf("<span font=\"Liberation Sans 9\" color=\"red\"><b>%s</b></span>", (*m)->msg);
			gtk_label_set_markup(GTK_LABEL(l_lstat), prompt);
			relax();
			break;
		}
		case PAM_TEXT_INFO:		/* display some text */
		{
			gchar *prompt = g_strdup_printf("<span font=\"Liberation Sans 9\"><b>%s</b></span>", (*m)->msg);
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
free_string(gpointer data, GClosure *unused)
{
	free(data);
}

static void
append_switch_users(GtkMenu * menu)
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
		choose_result = CHOOSE_RESULT_LAUNCH;
		gtk_widget_set_sensitive(buttons[3], FALSE);
		gtk_main_quit();
		break;
	}
}

static void
on_logout_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;

	(void)buttons;

	choose_result = CHOOSE_RESULT_LOGOUT;
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

/** @brief just gets the filenames of the xsession files
  *
  * This just gest the filenames of the xsession files to avoid performing a lot
  * of time consuming startup during the login.  We process the actual xession
  * files and add them to the list out of an idle loop.
  */
GHashTable *
get_xsessions(void)
{
	char **xdg_dirs, **dirs;
	int i, n = 0;
	static const char *suffix = ".desktop";
	static const int suflen = 8;
	GHashTable *xsessions = NULL;

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

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	choose_result = CHOOSE_RESULT_LOGOUT;
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

/** @brief transform window into pointer-grabbed window
  * @param window - window to transform
  *
  * Trasform a window into a window that has a grab on the pointer on the window
  * and restricts pointer movement to the window boundary.
  *
  * Note that the window and pointer grabs should not fail: the reason is that
  * we have mapped this window above all others and a fully obscured window
  * cannot hold the pointer or keyboard focus in X.
  */
void
grabbed_window(GtkWindow *window, gpointer user_data)
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

	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_window_hide(win);
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

typedef struct {
	int index;	    /* index */
	GdkScreen *scrn;    /* screen */
	GtkWidget *wind;    /* covering window for screen */
	gint width;
	gint height;
} XdeScreen;

XdeScreen *screens;

GtkWidget *ebox; /* event box window within the screen */

/** @brief get a covering window for a screen
  */
void
GetScreen(XdeScreen *scr, int s, GdkScreen *scrn)
{
	GtkWidget *wind;
	GtkWindow *w;

	scr->index = s;
	scr->scrn = scrn;
	scr->wind = wind = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	scr->width = gdk_screen_get_width(scrn);
	scr->height = gdk_screen_get_height(scrn);
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
	gtk_window_set_default_size(w, scr->width, scr->height);

	char *geom = g_strdup_printf("%dx%d+0+0", scr->width, scr->height);
	gtk_window_parse_geometry(w, geom);
	g_free(geom);

	gtk_widget_set_app_paintable(wind, TRUE);
	GdkPixbuf *pixbuf = get_pixbuf(scrn);

	g_signal_connect(G_OBJECT(w), "destroy", G_CALLBACK(on_destroy), NULL);
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(on_expose_event), pixbuf);

	gtk_window_set_focus_on_map(w, TRUE);
	gtk_window_set_accept_focus(w, TRUE);
	gtk_window_set_keep_above(w, TRUE);
	gtk_window_set_modal(w, TRUE);
	gtk_window_stick(w);
	gtk_window_deiconify(w);
	gtk_widget_show_all(wind);

	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);
	gdk_window_set_override_redirect(win, TRUE);
}

/** @brief get a covering window for each screen
  */
void
GetScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *scr;

	screens = calloc(nscr, sizeof(*screens));

	for (s = 0, scr = screens; s < nscr; s++, scr++)
		GetScreen(scr, s, gdk_display_get_screen(disp, s));
}

void
GetWindow(void)
{
	GtkWidget *wind;
	char hostname[64] = { 0, };
	GdkDisplay *disp = gdk_display_get_default();
	GdkScreen *scrn = NULL;
	GdkRectangle rect;
	XdeScreen *scr;
	int s, mon;
	gint x = 0, y = 0;
	float xrel, yrel;

	GetScreens();

	gdk_display_get_pointer(disp, &scrn, &x, &y, NULL);
	if (!scrn)
		scrn = gdk_display_get_default_screen(disp);
	s = gdk_screen_get_number(scrn);
	scr = screens + s;
	wind = scr->wind;

	mon = gdk_screen_get_monitor_at_point(scrn, x, y);
	gdk_screen_get_monitor_geometry(scrn, mon, &rect);
	x = rect.x + rect.width/2;
	y = rect.y + rect.height/2;

	xrel = (float)x/(float)scr->width;
	yrel = (float)y/(float)scr->height;
	GtkWidget *a = gtk_alignment_new(xrel, yrel, 0.0, 0.0);

	gtk_container_add(GTK_CONTAINER(wind), a);

	gethostname(hostname, sizeof(hostname));

	ebox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(a), ebox);
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

	GtkWidget *tab = gtk_table_new(1, 2, TRUE);

	gtk_table_set_col_spacings(GTK_TABLE(tab), 5);
	gtk_box_pack_end(GTK_BOX(v), tab, TRUE, TRUE, 0);
	v = gtk_vbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(tab), v, 0, 1, 0, 1);

	GtkWidget *bin = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(bin), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
	gtk_box_pack_start(GTK_BOX(v), bin, TRUE, TRUE, 4);

	GtkWidget *pan = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(pan), 15);
	gtk_container_add(GTK_CONTAINER(bin), pan);

	GtkWidget *img;

	if (options.banner && (img = gtk_image_new_from_file(options.banner)))
		gtk_container_add(GTK_CONTAINER(pan), img);

	v = gtk_vbox_new(FALSE, 0);

	gtk_table_attach_defaults(GTK_TABLE(tab), v, 1, 2, 0, 1);

	GtkWidget *inp = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(inp), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

	gtk_box_pack_start(GTK_BOX(v), inp, TRUE, TRUE, 4);

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
	gtk_entry_set_width_chars(GTK_ENTRY(user), 10);
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
	gtk_entry_set_width_chars(GTK_ENTRY(pass), 10);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_set_can_default(pass, TRUE);
	gtk_widget_set_can_focus(pass, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), pass, 1, 2, 1, 2);

	g_signal_connect(G_OBJECT(user), "activate", G_CALLBACK(on_user_activate), pass);
	g_signal_connect(G_OBJECT(pass), "activate", G_CALLBACK(on_pass_activate), user);

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end(GTK_BOX(v), bb, FALSE, TRUE, 0);

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

	gtk_widget_show_all(wind);
	gtk_widget_show_now(wind);
	gtk_widget_grab_default(user);
	gtk_widget_grab_focus(user);
	grabbed_window(GTK_WINDOW(wind), NULL);
}

static void
run_xlogin(int argc, char *argv[])
{
	gtk_init(NULL, NULL);
	GetWindow();
	gtk_main();
	exit(REMANAGE_DISPLAY);
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
    -w, --welcome TEXT\n\
        text to be display in title area of host menu\n\
	(%3$s)\n\
    -x, --xdmAddress ADDRESS\n\
        address of xdm socket\n\
    -c, --clientAddress IPADDR\n\
        client address that initiated the request\n\
    -t, --connectionType TYPE\n\
        connection type supported by the client\n\
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
set_default_welcome(void)
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
	set_default_welcome();
}

void
get_defaults(void)
{
	if (options.command == CommandDefault)
		options.command = CommandXlogin;
}

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
	array->length = len;
	array->data = calloc(len, sizeof(CARD8));
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
			{"xdmAddress",	    required_argument,	NULL, 'x'},
			{"clientAddress",   required_argument,	NULL, 'c'},
			{"connectionType",  required_argument,	NULL, 't'},
			{"banner",	    required_argument,	NULL, 'b'},
			{"welcome",	    required_argument,	NULL, 'w'},

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

		c = getopt_long_only(argc, argv, "x:c:t:b:w:nD::v::hVCH?", long_options,
				     &option_index);
#else
		c = getopt(argc, argv, "x:c:t:b:w:nDvhVCH?");
#endif
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'x':	/* -xdmAddress HEXBYTES */
			if (options.xdmAddress.length)
				goto bad_option;
			if (!HexToARRAY8(&options.xdmAddress, optarg))
				goto bad_option;
			break;
		case 'c':	/* -clientAddress HEXBYTES */
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
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strndup(optarg, 256);
			break;
		case 'w':	/* -w, --welcome WELCOME */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
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
	case CommandXlogin:
		DPRINTF("%s: running xlogin\n", argv[0]);
		run_xlogin(argc - optind, &argv[optind]);
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
	exit(OBEYSESS_DISPLAY);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
