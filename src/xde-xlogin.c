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

#include "xde-xlogin.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <X11/Xdmcp.h>
#include <X11/Xauth.h>


#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

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

GtkListStore *model;
GtkWidget *view;
GtkTreeViewColumn *cursor;

GtkWidget *user, *pass;
GtkWidget *buttons[5];

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
	free(options.choice);
	options.choice = strdup("logout");
	free(options.session);
	options.session = NULL;
	options.managed = FALSE;
	choose_result = CHOOSE_RESULT_LOGOUT;
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

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v))) {
			DPRINTF("Label selected %s\n", label);
			gtk_widget_set_sensitive(buttons[1], FALSE);
			gtk_widget_set_sensitive(buttons[2], TRUE);
			free(options.session);
			options.session = strdup(label);
		}
		g_value_unset(&label_v);
	}
	switch (state) {
	case LoginStateInit:
		gtk_widget_grab_default(user);
		gtk_widget_grab_focus(user);
		break;
	case LoginStateUsername:
		gtk_widget_grab_default(pass);
		gtk_widget_grab_focus(pass);
		break;
	default:
		break;
	}
}

static void
on_select_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;

	free(options.choice);
	options.choice = strdup("default");
	free(options.session);
	options.session = NULL;
	options.managed = TRUE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_unselect_all(selection);
	gtk_widget_set_sensitive(buttons[1], FALSE);
	gtk_widget_set_sensitive(buttons[2], FALSE);
	switch (state) {
	case LoginStateInit:
		gtk_widget_grab_default(user);
		gtk_widget_grab_focus(user);
		break;
	case LoginStateUsername:
		gtk_widget_grab_default(pass);
		gtk_widget_grab_focus(pass);
		break;
	default:
		break;
	}
}

/** @brief launch the session
  *
  * unlike the xsession chooser, this is a login button
  */
static void
on_launch_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* FIXME: not quit right here: this should start the login
	   authentication. */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &manage_v);
		if ((label = g_value_get_string(&label_v))) {
			DPRINTF("Label selected %s\n", label);
			if (!options.choice || strcmp(label, options.choice)) {
				free(options.choice);
				options.choice = strdup(label);
				free(options.session);
				options.session = NULL;
				gtk_widget_set_sensitive(buttons[1], TRUE);
				gtk_widget_set_sensitive(buttons[2], TRUE);
			}
			options.managed = g_value_get_boolean(&manage_v);
		}
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
	} else {
		free(options.choice);
		options.choice = strdup("default");
		free(options.session);
		options.session = NULL;
		options.managed = TRUE;
		gtk_widget_set_sensitive(buttons[1], FALSE);
		gtk_widget_set_sensitive(buttons[2], FALSE);
	}
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
		gtk_widget_set_sensitive(buttons[3], TRUE);
		state = LoginStatePassword;
		/* FIXME: do the authorization */
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

static void
on_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &manage_v);
		if ((label = g_value_get_string(&label_v))) {
			DPRINTF("Label selected %s\n", label);
			if (!options.choice || strcmp(label, options.choice)) {
				free(options.choice);
				options.choice = strdup(label);
				free(options.session);
				options.session = NULL;
				gtk_widget_set_sensitive(buttons[1], TRUE);
				gtk_widget_set_sensitive(buttons[2], TRUE);
			}
			options.managed = g_value_get_boolean(&manage_v);
		}
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
	} else {
		free(options.choice);
		options.choice = strdup("default");
		free(options.session);
		options.session = NULL;
		options.managed = TRUE;
		gtk_widget_set_sensitive(buttons[1], FALSE);
		gtk_widget_set_sensitive(buttons[2], FALSE);
	}
}

static void
on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_LABEL, &label_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_MANAGED, &manage_v);
		if ((label = g_value_get_string(&label_v))) {
			DPRINTF("Label selected %s\n", label);
			if (!options.choice || strcmp(label, options.choice)) {
				free(options.choice);
				options.choice = strdup(label);
				free(options.session);
				options.session = NULL;
				gtk_widget_set_sensitive(buttons[1], TRUE);
				gtk_widget_set_sensitive(buttons[2], TRUE);
			}
			options.managed = g_value_get_boolean(&manage_v);
		}
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
	}
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
			g_value_unset(&label_v);
		}
	}
	return FALSE;		/* propagate event */
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

gboolean
on_idle(gpointer data)
{
	static GHashTable *xsessions = NULL;
	static GHashTableIter hiter;
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
		g_hash_table_iter_init(&hiter, xsessions);
	}
	if (!g_hash_table_iter_next(&hiter, (gpointer *)&key, (gpointer *)&file))
		return G_SOURCE_REMOVE;

	if (!(entry = get_xsession_entry(key, file)))
		return G_SOURCE_CONTINUE;

	if (bad_xsession(key, entry)) {
		g_key_file_free(entry);
		return G_SOURCE_CONTINUE;
	}

	gchar *i, *n, *c, *k, *l, *f;
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
	k = g_strdup_printf("<b>%s</b>\n%s", n, c);
	m = g_key_file_get_boolean(entry, "Window Manager", "X-XDE-Managed", NULL);
	gtk_list_store_append(model, &iter);
	/* *INDENT-OFF* */
	gtk_list_store_set(model, &iter,
			COLUMN_PIXBUF,	 i,
			COLUMN_NAME,	 n,
			COLUMN_COMMENT,	 c,
			COLUMN_MARKUP,	 k,
			COLUMN_LABEL,	 l,
			COLUMN_MANAGED,	 m,
			COLUMN_ORIGINAL, m,
			COLUMN_FILENAME, f,
			COLUMN_KEYFILE,	 NULL,
			-1);
	/* *INDENT-ON* */
	g_free(f);
	g_free(l);
	g_free(i);
	g_free(n);
	g_free(k);
	g_key_file_free(entry);

#if 0
	if (!strcmp(options.choice, key) ||
	    ((!strcmp(options.choice, "choose") || !strcmp(options.choice, "default")) &&
	     !strcmp(options.session, key)
	    )) {
		gchar *string;

		/* FIXME: don't do this if the user has made a selection in the
		 * mean time. */
		if ((string = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter))) {
			GtkTreePath *path = gtk_tree_path_new_from_string(string);

			g_free(string);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(view),
					path, cursor, NULL, FALSE);
			gtk_tree_path_free(path);
		}
	}
#endif
	return G_SOURCE_CONTINUE;
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

/** @brief transform window into pointer-grabbed window
  * @param window - window to transform
  *
  * Trasform a window into a window that has a grab on the pointer on the window
  * and restricts pointer movement to the window boundary.
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

void
ungrabbed_window(GtkWindow *window)
{
	GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(window));

	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_window_hide(win);
}

GtkWindow *
GetWindow()
{
	GtkWidget *win, *vbox;
	GValue val = G_VALUE_INIT;
	char hostname[64] = { 0, };

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(win), "xde-xlogin", "XDE-XLogin");
	gtk_window_set_title(GTK_WINDOW(win), "XDMCP Greeter");
	gtk_window_set_gravity(GTK_WINDOW(win), GDK_GRAVITY_CENTER);
	gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ALWAYS);
	// gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_value_init(&val, G_TYPE_BOOLEAN);
	g_value_set_boolean(&val, FALSE);
	g_object_set_property(G_OBJECT(win), "allow-grow", &val);
	g_object_set_property(G_OBJECT(win), "allow-shrink", &val);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(win), 5);

	gethostname(hostname, sizeof(hostname));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_container_add(GTK_CONTAINER(win), vbox);

	GtkWidget *lab = gtk_label_new(NULL);
	gchar *markup;

	markup = g_markup_printf_escaped
	    ("<span font=\"Liberation Sans 12\"><b><i>%s</i></b></span>", options.welcome);
	gtk_label_set_markup(GTK_LABEL(lab), markup);
	gtk_misc_set_alignment(GTK_MISC(lab), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(lab), 3, 3);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(vbox), lab, FALSE, TRUE, 0);

	GtkWidget *tab = gtk_table_new(1, 2, FALSE);

	gtk_table_set_col_spacings(GTK_TABLE(tab), 5);
	gtk_box_pack_end(GTK_BOX(vbox), tab, TRUE, TRUE, 0);

	GtkWidget *vbox2 = gtk_vbox_new(FALSE, 0);

	gtk_table_attach_defaults(GTK_TABLE(tab), vbox2, 0, 1, 0, 1);

	GtkWidget *bin = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(bin), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
	gtk_box_pack_start(GTK_BOX(vbox2), bin, TRUE, TRUE, 4);

	GtkWidget *pan = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(pan), 15);
	gtk_container_add(GTK_CONTAINER(bin), pan);

	GtkWidget *img;

	if (options.banner && (img = gtk_image_new_from_file(options.banner)))
		gtk_container_add(GTK_CONTAINER(pan), img);

	vbox2 = gtk_vbox_new(FALSE, 0);

	gtk_table_attach_defaults(GTK_TABLE(tab), vbox2, 1, 2, 0, 1);

	GtkWidget *inp = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(inp), GTK_SHADOW_ETCHED_IN);
	gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

	gtk_box_pack_start(GTK_BOX(vbox2), inp, FALSE, FALSE, 4);

	GtkWidget *login = gtk_table_new(2, 3, TRUE);

	gtk_container_set_border_width(GTK_CONTAINER(login), 5);
	gtk_table_set_col_spacings(GTK_TABLE(login), 5);
	gtk_table_set_row_spacings(GTK_TABLE(login), 5);
	gtk_table_set_col_spacing(GTK_TABLE(login), 0, 0);
	gtk_container_add(GTK_CONTAINER(inp), login);

	GtkWidget *uname = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(uname), "<span font=\"Liberation Sans 9\"><b>Username:</b></span>");
	gtk_misc_set_alignment(GTK_MISC(uname), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(uname), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), uname, 0, 1, 0, 1);

	user = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(user), 10);
	gtk_entry_set_visibility(GTK_ENTRY(user), TRUE);
	gtk_widget_set_can_default(user, TRUE);
	gtk_widget_set_can_focus(user, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), user, 1, 2, 0, 1);

	GtkWidget *pword = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(pword), "<span font=\"Liberation Sans 9\"><b>Password:</b></span>");
	gtk_misc_set_alignment(GTK_MISC(pword), 1.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(pword), 5, 2);
	gtk_table_attach_defaults(GTK_TABLE(login), pword, 0, 1, 1, 2);

	pass = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(pass), 10);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_set_can_default(pass, TRUE);
	gtk_widget_set_can_focus(pass, TRUE);
	gtk_table_attach_defaults(GTK_TABLE(login), pass, 1, 2, 1, 2);

	g_signal_connect(G_OBJECT(user), "activate", G_CALLBACK(on_user_activate), pass);
	g_signal_connect(G_OBJECT(pass), "activate", G_CALLBACK(on_pass_activate), user);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 3);
	gtk_box_pack_start(GTK_BOX(vbox2), sw, TRUE, TRUE, 0);

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox2), bb, FALSE, TRUE, 0);

	/* *INDENT-OFF* */
	model = gtk_list_store_new(9
			,G_TYPE_STRING    /* pixbuf */
			,G_TYPE_STRING    /* Name */
			,G_TYPE_STRING    /* Comment */
			,G_TYPE_STRING    /* Name and Comment Markup */
			,G_TYPE_STRING    /* Label */
			,G_TYPE_BOOLEAN	  /* SessionManaged? XDE-Managed?  */
			,G_TYPE_BOOLEAN	  /* X-XDE-managed original setting */
			,G_TYPE_STRING    /* the file name */
			,G_TYPE_POINTER    /* the GKeyFile object */
		);
	/* *INDENT-ON* */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
			COLUMN_MARKUP, GTK_SORT_ASCENDING);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), COLUMN_NAME);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COLUMN_NAME);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(view), GTK_TREE_VIEW_GRID_LINES_BOTH);
	gtk_container_add(GTK_CONTAINER(sw), view);

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
	gtk_widget_set_sensitive(b, FALSE);

	buttons[2] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Use Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_select_clicked), buttons);
	gtk_widget_set_sensitive(b, FALSE);

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
	gtk_widget_set_sensitive(b, FALSE);

	free(options.choice);
	options.choice = strdup("default");
	free(options.session);
	options.session = NULL;
	options.managed = TRUE;

	/* FIXME */
	// gtk_widget_grab_default(b);

	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), buttons);

	g_signal_connect(G_OBJECT(view), "row_activated", G_CALLBACK(on_row_activated), buttons);
	g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(on_button_press), buttons);

	g_idle_add(on_idle, model);

	gtk_window_set_default_size(GTK_WINDOW(win), -1, 300);

	/* most of this is just in case override-redirect fails */
	gtk_window_set_focus_on_map(GTK_WINDOW(win), TRUE);
	gtk_window_set_accept_focus(GTK_WINDOW(win), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_window_stick(GTK_WINDOW(win));
	gtk_window_deiconify(GTK_WINDOW(win));
	gtk_widget_show_all(win);
	gtk_widget_show_now(win);
	relax();
	gtk_widget_grab_default(user);
	gtk_widget_grab_focus(user);

//	gtk_widget_realize(win);
//	GdkWindow *w = gtk_widget_get_window(win);
//	gdk_window_set_override_redirect(w, TRUE);
//	grabbed_window(GTK_WINDOW(win), NULL);

#if 0
	gtk_main();
	gtk_widget_destroy(win);

	GKeyFile *entry = NULL;

	if (strcmp(options.current, "logout") && strcmp(options.current, "login")) {
		if (!(entry = (typeof(entry)) g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s?\n", options.current);
			exit(REMANAGE_DISPLAY);
		}
	}
#endif
	return (GTK_WINDOW(win));
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
