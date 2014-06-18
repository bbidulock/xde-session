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

#include "xde-session.h"

#include <sys/types.h>
#include <pwd.h>
#include <unique/unique.h>
#include <glib-unix.h>
#include <glib/gfileutils.h>
#include <glib/gkeyfile.h>
#include <glib/gdataset.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

typedef struct {
	char *vendor;
	Bool mkdirs;
	char *wmname;
	char *desktop;
	char *session;
	char *splash;
	char **setup;
	char *startwm;
} Options;

Options options;

typedef struct {
	char *HOSTNAME;
	char *USER;
	char *HOME;
	char *XDG_CONFIG_HOME;
	char *XDG_CONFIG_DIRS;
	char *XDG_DATA_HOME;
	char *XDG_DATA_DIRS;
	char *XDG_DESKTOP_DIR;
	char *XDG_MENU_PREFIX;
} Environment;

Environment envir;

typedef struct {
	char *Desktop;
	char *Download;
	char *Templates;
	char *Public;
	char *Documents;
	char *Music;
	char *Pictures;
	char *Videos;
} UserDirs;

UserDirs userdirs;

typedef struct {
	char *Encoding;
	char *Type;
	char *Name;
	char *GenericName;
	char *Comment;
	char *OnlyShowIn;
	char *NotShowIn;
	char *Categories;
	char *Actions;
	char *Hidden;
	char *NoDisplay;
	char *Terminal;
	char *StartupNotify;
	char *StartupWMClass;
	char *Icon;
	char *TryExec;
	char *Exec;
	GQuark id;
	char *file;
} DesktopEntry;

GData *entries;
GData *execute;

void
setup_environment()
{
	char *env, buf[PATH_MAX] = { 0, };
	struct stat st;
	FILE *f;
	int status;

	/* set defaults or determine values */
	if ((env = getenv("HOSTNAME")))
		envir.HOSTNAME = strdup(env);
	else {
		gethostname(buf, PATH_MAX);
		envir.HOSTNAME = strdup(buf);
	}
	if ((env = getenv("USER")))
		envir.USER = strdup(env);
	else {
		struct passwd pwd = { NULL, }, *pw = NULL;
		if (getpwuid_r(getuid(), &pwd, buf, PATH_MAX, &pw) != -1 && pw)
			envir.USER = strdup(pw->pw_name);
	}
	if ((env = getenv("HOME")))
		envir.HOME = strdup(env);
	else {
		struct passwd pwd = { NULL, }, *pw = NULL;
		if (getpwuid_r(getuid(), &pwd, buf, PATH_MAX, &pw) != -1 && pw)
			envir.HOME = strdup(pw->pw_dir);
	}
	if ((env = getenv("XDG_CONFIG_HOME")))
		envir.XDG_CONFIG_HOME = strdup(env);
	else {
		snprintf(buf, PATH_MAX, "%s/.config", envir.HOME);
		envir.XDG_CONFIG_HOME = strdup(buf);
	}
	if ((env = getenv("XDG_CONFIG_DIRS")))
		envir.XDG_CONFIG_DIRS = strdup(env);
	else
		envir.XDG_CONFIG_DIRS = strdup("/usr/local/share:/usr/share");
	if ((env = getenv("XDG_DATA_HOME")))
		envir.XDG_DATA_HOME = strdup(env);
	else {
		snprintf(buf, PATH_MAX, "%s/.local/share", envir.HOME);
		envir.XDG_DATA_HOME = strdup(buf);
	}
	/* add overrides for XDE and for vendor */
	if (options.vendor) {
		snprintf(buf, PATH_MAX, "/etc/xdg/xde:/etc/xdg/%s:%s", options.vendor, envir.XDG_CONFIG_DIRS);
		free(envir.XDG_CONFIG_DIRS);
		envir.XDG_CONFIG_DIRS = strdup(buf);
		snprintf(buf, PATH_MAX, "/usr/share/xde:/usr/share/%s:%s", options.vendor, envir.XDG_CONFIG_DIRS);
		free(envir.XDG_DATA_DIRS);
		envir.XDG_DATA_DIRS = strdup(buf);
	} else {
		snprintf(buf, PATH_MAX, "/etc/xdg/xde:%s:%s", options.vendor, envir.XDG_CONFIG_DIRS);
		free(envir.XDG_CONFIG_DIRS);
		envir.XDG_CONFIG_DIRS = strdup(buf);
		snprintf(buf, PATH_MAX, "/usr/share/xde:%s:%s", options.vendor, envir.XDG_CONFIG_DIRS);
		free(envir.XDG_DATA_DIRS);
		envir.XDG_DATA_DIRS = strdup(buf);
	}
	/* set primary XDG environment variables */
	if (envir.HOSTNAME)
		setenv("HOSTNAME", envir.HOSTNAME, 1);
	if (envir.USER)
		setenv("USER", envir.USER, 1);
	if (envir.HOME)
		setenv("HOME", envir.HOME, 1);
	if (envir.XDG_CONFIG_HOME) {
		setenv("XDG_CONFIG_HOME", envir.XDG_CONFIG_HOME, 1);
		if (stat(envir.XDG_CONFIG_HOME, &st) == -1 || !S_ISDIR(st.st_mode))
			mkdir(envir.XDG_CONFIG_HOME, 0755);
	}
	if (envir.XDG_CONFIG_DIRS)
		setenv("XDG_CONFIG_DIRS", envir.XDG_CONFIG_DIRS, 1);
	if (envir.XDG_DATA_HOME) {
		setenv("XDG_DATA_HOME", envir.XDG_DATA_HOME, 1);
		if (stat(envir.XDG_DATA_HOME, &st) == -1 || !S_ISDIR(st.st_mode))
			mkdir(envir.XDG_DATA_HOME, 0755);
	}
	if (envir.XDG_DATA_DIRS)
		setenv("XDG_DATA_DIRS", envir.XDG_DATA_DIRS, 1);

	/* check for XDG user directory settings */
	snprintf(buf, PATH_MAX, "%s/user-dirs.dirs", envir.XDG_CONFIG_HOME);
	if (stat("/usr/bin/xdg-user-dirs-update", &st) != -1 && (S_IXOTH & st.st_mode))
		status = system("/usr/bin/xdg-user-dirs-update");
	else if (stat(buf, &st) == -1 || !S_ISREG(st.st_mode)) {
		if ((f = fopen(buf, ">"))) {
			fputs("XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n", f);
			fputs("XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n", f);
			fputs("XDG_TEMPLATES_DIR=\"$HOME/Templates\"\n", f);
			fputs("XDG_PUBLICSHARE_DIR=\"$HOME/Public\"\n", f);
			fputs("XDG_DOCUMENTS_DIR=\"$HOME/Documents\"\n", f);
			fputs("XDG_MUSIC_DIR=\"$HOME/Documents/Music\"\n", f);
			fputs("XDG_PICTURES_DIR=\"$HOME/Documents/Pictures\"\n", f);
			fputs("XDG_VIDEOS_DIR=\"$HOME/Documents/Videos\"\n", f);
			fclose(f);
		}
	}
	(void) status;
	if ((f = fopen(buf, "<"))) {
		char *key, *val, *p, **where;
		int hlen = strlen(envir.HOME);

		while ((fgets(buf, PATH_MAX, f))) {
			if (!(key = strstr(buf, "XDG_")) || key != buf)
				continue;
			key += 4;
			if (!(val = strstr(key, "_DIR=\"")))
				continue;
			*val = '\0';
			val += 6;
			if (!(p = strchr(val, '"')))
				continue;
			*p = '\0';
			if (!strcmp(key, "DESKTOP"))
				where = &userdirs.Desktop;
			else if (!strcmp(key, "DOWNLOAD"))
				where = &userdirs.Download;
			else if (!strcmp(key, "TEMPLATES"))
				where = &userdirs.Templates;
			else if (!strcmp(key, "PUBLICSHARE"))
				where = &userdirs.Public;
			else if (!strcmp(key, "DOCUMENTS"))
				where = &userdirs.Documents;
			else if (!strcmp(key, "MUSIC"))
				where = &userdirs.Music;
			else if (!strcmp(key, "PICTURES"))
				where = &userdirs.Pictures;
			else if (!strcmp(key, "VIDEOS"))
				where = &userdirs.Videos;
			else
				continue;
			p = val;
			while ((p = strstr(p, "$HOME"))) {
				memmove(p + hlen, p + 4, strlen(p + 5) + 1);
				memcpy(p, envir.HOME, hlen);
				p += hlen;
			}
			free(*where);
			*where = strdup(val);
		}
		fclose(f);
	}
	/* apply defaults to remaining values */
	if (!userdirs.Desktop) {
		snprintf(buf, PATH_MAX, "%s/Desktop", envir.HOME);
		userdirs.Desktop = strdup(buf);
	}
	if (!userdirs.Download) {
		snprintf(buf, PATH_MAX, "%s/Downloads", envir.HOME);
		userdirs.Download = strdup(buf);
	}
	if (!userdirs.Templates) {
		snprintf(buf, PATH_MAX, "%s/Templates", envir.HOME);
		userdirs.Templates = strdup(buf);
	}
	if (!userdirs.Public) {
		snprintf(buf, PATH_MAX, "%s/Public", envir.HOME);
		userdirs.Public = strdup(buf);
	}
	if (!userdirs.Documents) {
		snprintf(buf, PATH_MAX, "%s/Documents", envir.HOME);
		userdirs.Documents = strdup(buf);
	}
	if (!userdirs.Music) {
		snprintf(buf, PATH_MAX, "%s/Documents/Music", envir.HOME);
		userdirs.Music = strdup(buf);
	}
	if (!userdirs.Pictures) {
		snprintf(buf, PATH_MAX, "%s/Documents/Pictures", envir.HOME);
		userdirs.Pictures = strdup(buf);
	}
	if (!userdirs.Videos) {
		snprintf(buf, PATH_MAX, "%s/Documents/Videos", envir.HOME);
		userdirs.Videos = strdup(buf);
	}
	if (options.mkdirs) {
		/* make missing user directories */
		if ((stat(userdirs.Desktop, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Desktop, 0755);
		if ((stat(userdirs.Download, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Download, 0755);
		if ((stat(userdirs.Templates, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Templates, 0755);
		if ((stat(userdirs.Public, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Public, 0755);
		if ((stat(userdirs.Documents, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Documents, 0755);
		if ((stat(userdirs.Music, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Music, 0755);
		if ((stat(userdirs.Pictures, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Pictures, 0755);
		if ((stat(userdirs.Videos, &st) == -1) || !S_ISDIR(st.st_mode))
			mkdir(userdirs.Videos, 0755);
	}
	if ((env = getenv("XDG_DESKTOP_DIR")))
		envir.XDG_DESKTOP_DIR = strdup(env);
	else {
		if (userdirs.Desktop)
			envir.XDG_DESKTOP_DIR = strdup(userdirs.Desktop);
		else {
			snprintf(buf, PATH_MAX, "%s/Desktop", envir.HOME);
			envir.XDG_DESKTOP_DIR = strdup(buf);
		}
	}
	setenv("XDG_DESKTOP_DIR", envir.XDG_DESKTOP_DIR, 1);
	if ((stat(envir.XDG_DESKTOP_DIR, &st) == -1) || !S_ISDIR(st.st_mode))
		mkdir(envir.XDG_DESKTOP_DIR, 0755);

	if (options.wmname && *options.wmname && !options.desktop) {
		int i, len;

		options.desktop = strdup(options.wmname);
		for (i = 0, len = strlen(options.desktop); i < len; i++)
			options.desktop[i] = toupper(options.desktop[i]);

	}
	if (!options.desktop && (env = getenv("XDG_CURRENT_DESKTOP")) && *env)
		options.desktop = strdup(env);
	if (!options.desktop && (env = getenv("FBXDG_DE")) && *env)
		options.desktop = strdup(env);
	if (!options.desktop)
		options.desktop = strdup("ADWM");
	setenv("XDG_CURRENT_DESKTOP", options.desktop, 1);
	setenv("FBXDG_DE", options.desktop, 1);

	if (!options.session && (env = getenv("DESKTOP_SESSION")) && *env)
		options.session = strdup(env);
	if (!options.session && options.wmname && *options.wmname) {
		int i, len;

		options.session = strdup(options.wmname);
		for (i = 0, len = strlen(options.session); i < len; i++)
			options.desktop[i] = toupper(options.desktop[i]);
	}
	if (!options.session)
		options.session = strdup("ADWM");
	setenv("DESKTOP_SESSION", options.session, 1);

	if (!options.wmname && options.desktop) {
		int i, len;

		options.wmname = strdup(options.desktop);
		for (i = 0, len = strlen(options.wmname); i < len; i++)
			options.wmname[i] = tolower(options.wmname[i]);
	}
	setenv("XDE_WM_NAME", options.wmname, 1);

	if ((env = getenv("XDG_MENU_PREFIX")) && *env)
		envir.XDG_MENU_PREFIX = strdup(env);
	if (!envir.XDG_MENU_PREFIX && options.vendor) {
		envir.XDG_MENU_PREFIX = calloc(strlen(options.vendor) + 2,
				sizeof(*envir.XDG_MENU_PREFIX));
		strcpy(envir.XDG_MENU_PREFIX, options.vendor);
		strcat(envir.XDG_MENU_PREFIX, "-");
	}
	if (!envir.XDG_MENU_PREFIX && options.desktop && !strcmp(options.desktop, "LXDE"))
		envir.XDG_MENU_PREFIX = strdup("lxde-");
	if (!envir.XDG_MENU_PREFIX)
		envir.XDG_MENU_PREFIX = strdup("xde-");
	setenv("XDG_MENU_PREFIX", envir.XDG_MENU_PREFIX, 1);


}

enum {
	COMMAND_SESSION = 1,
	COMMAND_EDITOR = 2,
	COMMAND_MENUBLD = 3,
	COMMAND_CONTROL = 4,
	COMMAND_LOGOUT = 5,
	COMMAND_EXECUTE = 6,
};

UniqueResponse
message_received(UniqueApp * app, gint cmd_id, UniqueMessageData * umd, guint time_)
{
	switch (cmd_id) {
	case COMMAND_SESSION:
		break;
	case COMMAND_EDITOR:
		/* launch a session editor window and gtk_window_set_screen(GTK_WINDOW(win),
		   unique_message_get_screen(umd)); */
		break;
	case COMMAND_MENUBLD:
		/* launch a session editor window and gtk_window_set_screen(GTK_WINDOW(win),
		   unique_message_get_screen(umd)); */
		break;
	case COMMAND_CONTROL:
		/* perform the control actions and gtk_window_set_screen(GTK_WINDOW(win),
		   unique_message_get_screen(umd)); */
		break;
	case COMMAND_LOGOUT:
		/* perform the control actions and gtk_window_set_screen(GTK_WINDOW(win),
		   unique_message_get_screen(umd)); */
		break;
	case COMMAND_EXECUTE:
		/* perform the control actions and gtk_window_set_screen(GTK_WINDOW(win),
		   unique_message_get_screen(umd)); */
		break;
	}
	const gchar *startup_id = unique_message_data_get_startup_id(umd);
	if (!startup_id && getenv("DESKTOP_STARTUP_ID"))
		startup_id = g_strdup(getenv("DESKTOP_STARTUP_ID"));
	if (startup_id)
		gdk_notify_startup_complete_with_id(startup_id);
	else
		gdk_notify_startup_complete();
	/* we are actually not doing anything, maybe later... */
	return UNIQUE_RESPONSE_OK;
}

UniqueApp *uapp;

void
session_startup(int argc, char *argv[])
{
	GdkDisplayManager *mgr = gdk_display_manager_get();
	GdkDisplay *dpy = gdk_display_manager_get_default_display(mgr);
	GdkScreen *screen = gdk_display_get_default_screen(dpy);
	GdkWindow *root = gdk_screen_get_root_window(screen);
	(void) root;

	uapp = unique_app_new_with_commands("com.unexicon.xde-session",
						      NULL,
						      "xde-session", COMMAND_SESSION,
						      "xde-session-edit", COMMAND_EDITOR,
						      "xde-session-menu", COMMAND_MENUBLD,
						      "xde-session-ctrl", COMMAND_CONTROL,
						      "xde-session-logout", COMMAND_LOGOUT,
						      "xde-session-run", COMMAND_EXECUTE,
						      NULL);

	if (unique_app_is_running(uapp)) {
		const char *cmd = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
		int cmd_id = -1;

		if (!strcmp(cmd, "xde-session"))
			cmd_id = COMMAND_SESSION;
		else if (!strcmp(cmd, "xde-session-edit"))
			cmd_id = COMMAND_EDITOR;
		else if (!strcmp(cmd, "xde-session-menu"))
			cmd_id = COMMAND_MENUBLD;
		else if (!strcmp(cmd, "xde-session-ctrl"))
			cmd_id = COMMAND_CONTROL;
		else if (!strcmp(cmd, "xde-session-logout"))
			cmd_id = COMMAND_LOGOUT;
		else if (!strcmp(cmd, "xde-session-run"))
			cmd_id = COMMAND_EXECUTE;
		fprintf(stderr, "Another instance of %s is already running.\n", cmd);
		if (cmd_id != -1) {
			int i, size;
			char *data, *p;

			for (i = 0, size = 0; i < argc; i++)
				size += strlen(argv[i]) + 1;

			data = calloc(size, sizeof(*data));

			for (i = 0, p = data; i < argc; i++, p += size) {
				int len = strlen(argv[i]) + 1;

				memcpy(p, argv[i], len);
			}
			UniqueMessageData *umd = unique_message_data_new();

			unique_message_data_set(umd, (guchar *) data, size);
			UniqueResponse resp = unique_app_send_message(uapp, cmd_id, umd);

			exit(resp);
		}
	}
}

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

GtkWidget *table;
int cols = 7;

void
splashscreen()
{
	GtkWidget *splscr = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(splscr), "xde-session", "XDE-Session");
	unique_app_watch_window(uapp, GTK_WINDOW(splscr));
	g_signal_connect(G_OBJECT(uapp), "message-received", G_CALLBACK(message_received), (gpointer) NULL);
	gtk_window_set_gravity(GTK_WINDOW(splscr), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(splscr), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_container_set_border_width(GTK_CONTAINER(splscr), 20);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(splscr), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(splscr), TRUE);
	gtk_window_set_keep_below(GTK_WINDOW(splscr), TRUE);
	gtk_window_set_position(GTK_WINDOW(splscr), GTK_WIN_POS_CENTER_ALWAYS);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(splscr), GTK_WIDGET(hbox));

	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);

	GtkWidget *img = gtk_image_new_from_file(options.splash);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(img), FALSE, FALSE, 0);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(GTK_WIDGET(sw), 800, -1);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(sw), TRUE, TRUE, 0);

	table = gtk_table_new(1, cols, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 1);
	gtk_table_set_row_spacings(GTK_TABLE(table), 1);
	gtk_table_set_homogeneous(GTK_TABLE(table), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(table), 750, -1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(table));

	gtk_window_set_default_size(GTK_WINDOW(splscr), -1, 600);
	gtk_widget_show_all(GTK_WIDGET(splscr));
	gtk_widget_show_now(GTK_WIDGET(splscr));
	relax();
}

void
key_file_unref(gpointer data)
{
	g_key_file_unref((GKeyFile *)data);
}

int autostarts = 0;

void
get_autostart()
{
	int dlen = strlen(envir.XDG_CONFIG_DIRS);
	int hlen = strlen(envir.XDG_CONFIG_HOME);
	int size = dlen + 1 + hlen + 1;
	char *dir;
	char *dirs = calloc(size, sizeof(*dirs)), *save = dirs;
	char *path = calloc(PATH_MAX, sizeof(*path));

	strcpy(dirs, envir.XDG_CONFIG_HOME);
	strcat(dirs, ":");
	strcat(dirs, envir.XDG_CONFIG_DIRS);

	g_datalist_init(&entries);

	while ((dir = strsep(&save, ":"))) {
		DIR *D;
		struct dirent *d;

		snprintf(path, PATH_MAX, "%s/autostart", dir);
		if (!(D = opendir(path)))
			continue;
		relax();
		while ((d = readdir(D))) {
			char *p;
			GKeyFile *kf;
			GError *err = NULL;

			if (*d->d_name == '.')
				continue;
			if (!(p = strstr(d->d_name, ".desktop")) || p[8])
				continue;
			snprintf(path, PATH_MAX, "%s/autostart/%s", dir, d->d_name);
			if ((kf = (typeof(kf)) g_datalist_get_data(&entries, d->d_name)))
				continue;
			relax();
			kf = g_key_file_new();
			if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err)) {
				g_datalist_set_data_full(&entries, d->d_name, (gpointer) kf, key_file_unref);
				autostarts++;
			}
		}
		closedir(D);
	}
	free(path);
	free(dirs);
}

typedef struct {
	int cols;
	int rows;
	int col;
	int row;
} TableContext;

void
foreach_autostart(GQuark key_id, gpointer data, gpointer user_data)
{
	static const char section[] = "Desktop Entry";
	GKeyFile *kf = (GKeyFile *) data;
	TableContext *c = (typeof(c)) user_data;
	char *name, *p, *value, **list, **item;
	GtkWidget *icon, *but;
	gboolean found;

	if ((name = g_key_file_get_string(kf, section, "Icon", NULL))) {
		if (((p = strstr(name, ".xpm")) && !p[4]) || ((p = strstr(name, ".png")) && !p[4]))
			*p = '\0';
	} else
		name = g_strdup("gtk-missing-image");
	icon = gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_DIALOG);
	g_free(name);

	if (!(name = g_key_file_get_locale_string(kf, section, "Name", NULL, NULL)))
		name = g_strdup("Unknown");
	but = gtk_button_new();
	gtk_button_set_image_position(GTK_BUTTON(but), GTK_POS_TOP);
	gtk_button_set_image(GTK_BUTTON(but), GTK_WIDGET(icon));
#if 0
	gtk_button_set_label(GTK_BUTTON(but), name);
#endif
	gtk_widget_set_tooltip_text(GTK_WIDGET(but), name);
	g_free(name);

	gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(but), c->col, c->col + 1, c->row, c->row + 1);
	gtk_widget_set_sensitive(GTK_WIDGET(but), FALSE);
	gtk_widget_show_all(GTK_WIDGET(but));
	gtk_widget_show_now(GTK_WIDGET(but));
	relax();

	if (++c->col >= c->cols) {
		c->row++;
		c->col = 0;
	}

	found = FALSE;
	if ((value = g_key_file_get_string(kf, section, "Type", NULL))) {
		if (!strcmp(value, "Application"))
			found = TRUE;
		g_free(value);
	}
	if (!found)
		return;

	if (g_key_file_get_boolean(kf, section, "Hidden", NULL))
		return;

	found = TRUE;
	if ((list = g_key_file_get_string_list(kf, section, "OnlyShowIn", NULL, NULL))) {
		for (found = FALSE, item = list; *item; item++) {
			if (!strcmp(*item, options.desktop)) {
				found = TRUE;
				break;
			}
		}
		g_strfreev(list);
	}
	if (!found)
		return;

	found = FALSE;
	if ((list = g_key_file_get_string_list(kf, section, "NotShowIn", NULL, NULL))) {
		for (found = FALSE, item = list; *item; item++) {
			if (!strcmp(*item, options.desktop)) {
				found = TRUE;
				break;
			}
		}
		g_strfreev(list);
	}
	if (found)
		return;

	found = TRUE;
	if ((value = g_key_file_get_string(kf, section, "TryExec", NULL))) {
		if (strchr(value, '/')) {
			struct stat st;

			if (stat(value, &st) == -1 || !((S_IXUSR | S_IXGRP | S_IXOTH) & st.st_mode))
				found = FALSE;
		} else {
			gchar **path;

			found = FALSE;
			if ((path = g_strsplit(getenv("PATH") ? : "", ":", -1))) {
				for (item = path; *item && !found; item++) {
					char *file;
					struct stat st;

					file = g_strjoin("/", *item, value, NULL);
					if (stat(file, &st) != -1 && ((S_IXUSR | S_IXGRP | S_IXOTH) & st.st_mode))
						found = TRUE;
					g_free(file);
				}
				g_strfreev(path);
			}
		}
		g_free(value);
	}
	if (!found)
		return;
	/* TODO: handle DBus activation */
	if (!g_key_file_has_key(kf, section, "Exec", NULL))
		return;

	gtk_widget_set_sensitive(GTK_WIDGET(but), TRUE);
	gtk_widget_show_now(GTK_WIDGET(but));
	relax();

	/* Do we really need to make two lists? Why not launch them right now? */
	g_key_file_ref(kf);
	g_datalist_id_set_data_full(&execute, key_id, (gpointer) kf, key_file_unref);
}

void
show_splashscreen()
{
	TableContext c = { cols, 0, 0, 0 };
	
	g_datalist_init(&execute);
	c.rows = (autostarts + c.cols + 1) / c.cols;
	gtk_table_resize(GTK_TABLE(table), c.rows, c.cols);
	g_datalist_foreach(&entries, foreach_autostart, (gpointer) &c);
	relax();
}

void
daemonize()
{
	pid_t pid, sid;
	char buf[64] = { 0, };
	FILE *status;

	pid = getpid();

	status = freopen("/dev/null", "r", stdin);
	status = freopen("/dev/null", "w", stdout);
	(void) status;

	if ((sid = setsid()) == -1)
		sid = pid;

	snprintf(buf, sizeof(buf), "%d", (int)sid);
	setenv("XDG_SESSION_PID", buf, 1);
	setenv("_LXSESSION_PID", buf, 1);
}

GHashTable *children;
GHashTable *restarts;
GHashTable *watchers;
GHashTable *commands;

pid_t wmpid;

pid_t startup(char *cmd, gpointer data);

void
childexit(pid_t pid, int wstat, gpointer data)
{
	gpointer key = (gpointer) (long) pid;
	int status;
	gboolean res;

	g_hash_table_remove(watchers, key);
	g_hash_table_remove(children, key);
	char *cmd = (char *) g_hash_table_lookup(commands, key);
	g_hash_table_steal(commands, key);
	res = g_hash_table_remove(restarts, key);

	if (WIFEXITED(wstat)) {
		if ((status = WEXITSTATUS(wstat)))
			fprintf(stderr, "child %d exited with status %d (%s)\n", pid, status, cmd);
		else {
			if (data) {
				wstat = system("xde-logout");
				if (!WIFEXITED(wstat) || WEXITSTATUS(wstat)) {
					res = FALSE;
					gtk_main_quit();
				}
			} else {
				res = FALSE;
			}
		}
	} else if (WIFSIGNALED(wstat)) {
		status = WTERMSIG(wstat);
		fprintf(stderr, "child %d exited on signal %d (%s)\n", pid, status, cmd);
	} else if (WIFSTOPPED(wstat)) {
		fprintf(stderr, "child %d stopped (%s)\n", pid, cmd);
		kill(pid, SIGTERM);
	}
	if (res) {
		fprintf(stderr, "restarting %d with %s\n", pid, cmd);
		pid = startup(cmd, data);
		if (data)
			wmpid = pid;
	}
	else if (data || !g_hash_table_size(children)) {
		fprintf(stderr, "there goes our last female... (%s)\n", cmd);
		gtk_main_quit();
	}
}

pid_t
startup(char *cmd, gpointer data)
{
	int restart = 0;
	pid_t child;

	if (*cmd == '@') {
		restart = 1;
		memmove(cmd, cmd + 1, strlen(cmd));
	}
	if ((child = fork()) == -1) {
		fprintf(stderr, "fork: %s\n", strerror(errno));
		return (0);
	}
	if (child) {
		/* we are the parent */
		gpointer val, key = (gpointer) (long) child;

		fprintf(stderr, "Child %d started...(%s)\n", child, cmd);
		g_hash_table_insert(commands, key, cmd);
		if (restart)
			g_hash_table_add(restarts, key);
		g_hash_table_add(children, key);
		val = (gpointer) (long) g_child_watch_add(child, childexit, data);
		g_hash_table_insert(watchers, key, val);
	} else {
		/* we are the child */
		execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);
		fprintf(stderr, "execl: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return (child);
}

void
execorfail(char *cmd)
{
	int status;

	fprintf(stderr, "Executing %s...\n", cmd);
	if ((status = system(cmd)) == -1) {
		fprintf(stderr, "system: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	else if (WIFEXITED(status)) {
		if ((status = WEXITSTATUS(status)) == 0)
			return;
		fprintf(stderr, "system: command \"%s\" exited with status %d\n", cmd, status);
		exit(EXIT_FAILURE);
	}
	else if (WIFSIGNALED(status)) {
		status = WTERMSIG(status);
		fprintf(stderr, "system: command \"%s\" exited on signal %d\n", cmd, status);
		exit(EXIT_FAILURE);
	}
	else if (WIFSTOPPED(status)) {
		fprintf(stderr, "system: command \"%s\" stopped\n", cmd);
		exit(EXIT_FAILURE);
	}
}

void
setup()
{
	static const char *script = "/setup.sh";
	char **prog;

	if (!(prog = options.setup)) {
		char *dirs, *save, *dir;
		int slen, wlen;

		fprintf(stderr, "%s\n", "executing default setup");
		save = dirs = strdup(envir.XDG_DATA_DIRS);
		slen = strlen(script);
		wlen = strlen(options.wmname);

		while ((dir = strsep(&dirs, ":"))) {
			int status, len = strlen(dir) + 1 + wlen + slen + 1;
			char *path = calloc(len, sizeof(*path));
			struct stat st;

			strncpy(path, dir, len);
			strncat(path, "/", len);
			strncat(path, options.wmname, len);
			strncat(path, script, len);

			status = stat(path, &st);
			if (status == -1) {
				fprintf(stderr, "stat: %s: %s\n", path, strerror(errno));
				free(path);
				continue;
			}
			if (!((S_IXUSR | S_IXGRP | S_IXOTH) & st.st_mode)) {
				fprintf(stderr, "stat: %s: %s\n", path, "not executable");
				free(path);
				continue;
			}
			execorfail(path);
			free(path);
			break;
		}
		free(save);
	} else
		while (*prog)
			execorfail(*prog++);
}

void
startwm()
{
	static const char *script = "/start.sh";
	char *cmd;

	if (!options.startwm) {
		char *dirs, *dir;
		int slen, wlen;

		fprintf(stderr, "%s\n", "executing default start");
		dirs = strdup(envir.XDG_DATA_DIRS);
		slen = strlen(script);
		wlen = strlen(options.wmname);

		while ((dir = strsep(&dirs, ":"))) {
			int status, len = strlen(dir) + 1 + wlen + slen + 1;
			char *path = calloc(len, sizeof(*path));
			struct stat st;

			strncpy(path, dir, len);
			strncat(path, "/", len);
			strncat(path, options.wmname, len);
			strncat(path, script, len);

			status = stat(path, &st);
			if (status != -1 && ((S_IXUSR | S_IXGRP | S_IXOTH) & st.st_mode)) {
				options.startwm = path;
				break;
			}
			free(path);
		}
		if (!options.startwm) {
			fprintf(stderr, "will simply execute '%s'\n", options.wmname);
			options.startwm = strdup(options.wmname);
		}
	}
	cmd = calloc(strlen(options.startwm) + 2, sizeof(*cmd));
	strcpy(cmd, "@");
	strcat(cmd, options.startwm);
	wmpid = startup(cmd, (gpointer) 1);
	/* startup assumes ownership of cmd */
}

void
waitwm()
{
}


void
pause_after_wm()
{
}

void
run_exec_options()
{
}

void
run_autostart()
{
}

static gboolean
hidesplash(gpointer user_data)
{
	return FALSE; /* remove source */
}

void
do_loop()
{
	g_timeout_add_seconds(150, hidesplash, NULL);
	gtk_main();
	exit(EXIT_SUCCESS);
}

void
set_defaults(void)
{
}

int
main(int argc, char *argv[])
{
	set_defaults();

	while(1) {
		int c, val;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"wmname",	    required_argument,	NULL, 'n'},
			{"startwm",	    required_argument,	NULL, 'm'},
			{"pause",	    optional_argument,	NULL, 'p'},
			{"desktop",	    optional_argument,	NULL, 'e'},
			{"wait",	    no_argument,	NULL, 'w'},
			{"nowait",	    no_argument,	NULL, 'W'},
			{"setup",	    required_argument,	NULL, 'S'},
			{"exec",	    required_argument,	NULL, 'x'},
			{"autostart",	    no_argument,	NULL, 'A'},
			{"noautostart",	    no_argument,	NULL, 'a'},
			{"session",	    required_argument,	NULL, 's'},
			{"file",	    required_argument,	NULL, 'f'},
			{"splash",	    required_argument,	NULL, 'l'},
			{"message",	    required_argument,	NULL, 'g'},
			{"side",	    required_argument,	NULL, 'i'},
			{"vendor",	    required_argument,	NULL, 'v'},
			{"mkdirs",	    no_argument,	NULL, 'k'},
			{"nomkdirs",	    no_argument,	NULL, 'K'},
			{"debug",	    optional_argument,	NULL, 'D'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */
		(void) c;
		(void) val;
		(void) long_options;
		break;
	}
	return (0);
}
