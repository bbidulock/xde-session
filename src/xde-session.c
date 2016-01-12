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
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *vendor;
	Bool mkdirs;
	char *wmname;
	char *desktop;
	char *session;
	char *splash;
	char **setup;
	char *startwm;
	int pause;
	Bool wait;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.vendor = NULL,
	.mkdirs = False,
	.wmname = NULL,
	.desktop = NULL,
	.session = NULL,
	.splash = NULL,
	.setup = NULL,
	.startwm = NULL,
	.pause = 0,
	.wait = False,
};

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
		int slen, wlen, dlen, hlen, blen;

		fprintf(stderr, "%s\n", "executing default setup");
		dlen = strlen(envir.XDG_CONFIG_DIRS);
		hlen = strlen(envir.XDG_CONFIG_HOME);
		blen = dlen + hlen + 2;
		save = dirs = calloc(blen, sizeof(*dirs));
		strncpy(dirs, envir.XDG_CONFIG_HOME, blen);
		strncat(dirs, ":", blen);
		strncat(dirs, envir.XDG_CONFIG_DIRS, blen);
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
		char *dirs, *save, *dir;
		int slen, wlen, dlen, hlen, blen;

		fprintf(stderr, "%s\n", "executing default start");
		dlen = strlen(envir.XDG_CONFIG_DIRS);
		hlen = strlen(envir.XDG_CONFIG_HOME);
		blen = dlen + hlen + 2;
		save = dirs = calloc(blen, sizeof(*dirs));
		strncpy(dirs, envir.XDG_CONFIG_HOME, blen);
		strncat(dirs, ":", blen);
		strncat(dirs, envir.XDG_CONFIG_DIRS, blen);
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
			options.startwm = path;
			break;
		}
		free(save);
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

Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_SUPPORTED;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_MOTIF_WM_INFO;
Atom *_XA_WM_Sd;

Display *dpy;
int nscr;
Window *roots;

/** @brief check for recursive window properties
  * @param root - the root window
  * @param atom - property name
  * @param type - property type
  * @param offset - offset in property
  * @return Window - the recursive window property or None
  */
static Window
check_recursive(Window root, Atom atom, Atom type, int offset)
{
	Atom real;
	int format = 0;
	unsigned long n, after;
	unsigned long *data = NULL;
	Window check = None;

	if (XGetWindowProperty(dpy, root, atom, 0, 1 + offset, False, type, &real, &format, &n, &after,
			       (unsigned char **) &data) == Success && n >= offset + 1 && data[offset]) {
		check = data[offset];
		XFree(data);
		data = NULL;
		if (XGetWindowProperty(dpy, check, atom, 0, 1 + offset, False, type, &real, &format, &n,
				       &after, (unsigned char **) &data) == Success && n >= offset + 1) {
			if (check == (Window) data[offset]) {
				XFree(data);
				data = NULL;
				return (check);
			}
		}

	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	return (None);
}

/** @brief check for non-recursive window properties (that should be recursive).
  * @param root - root window
  * @param atom - property name
  * @param type - property type
  * @return @indow - the recursive window property or None
  */
static Window
check_nonrecursive(Window root, Atom atom, Atom type, int offset)
{
	Atom real;
	int format = 0;
	unsigned long n, after;
	unsigned long *data = NULL;
	Window check = None;

	if (XGetWindowProperty(dpy, root, atom, 0, 1 + offset, False, type, &real, &format, &n, &after,
			       (unsigned char **) &data) == Success && format && n >= offset + 1 && data[offset]) {
		check = data[offset];
		XFree(data);
		data = NULL;
		return (check);
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	return (None);
}

/** @brief check if an atom is in a supported atom list
  * @param root - root window
  * @param protocols - list name
  * @param supported - element name
  * @return Bool - true if atom is in list 
  */
static Bool
check_supported(Window root, Atom protocols, Atom supported)
{
	Atom real;
	int format = 0;
	unsigned long n, after, num = 1;
	unsigned long *data = NULL;
	Bool result = False;

      try_harder:
	if (XGetWindowProperty(dpy, root, protocols, 0, num, False, XA_ATOM, &real, &format,
			       &n, &after, (unsigned char **) &data) == Success && format) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			data = NULL;
			goto try_harder;
		}
		if (n > 0) {
			unsigned long i;
			Atom *atoms;

			atoms = (Atom *) data;
			for (i = 0; i < n; i++) {
				if (atoms[i] == supported) {
					result = True;
					break;
				}
			}
		}
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	return (result);

}

/** @brief Check for a non-compliant EWMH/NetWM window manager.
  *
  * There are quite a few window managers that implement part of the EWMH/NetWM
  * specification but do not fill out _NET_SUPPORTING_WM_CHECK.  This is a big
  * annoyance.  One way to test this is whether there is a _NET_SUPPORTED on the
  * root window that does not include _NET_SUPPORTING_WM_CHECK in its atom list.
  *
  * The only window manager I know of that placed _NET_SUPPORTING_WM_CHECK in
  * the list and did not set the property on the root window was 2bwm, but is
  * has now been fixed.
  *
  * There are others that provide _NET_SUPPORTING_WM_CHECK on the root window
  * but fail to set it recursively.  When _NET_SUPPORTING_WM_CHECK is reported
  * as supported, relax the check to a non-recursive check.  (Case in point is
  * echinus(1)).
  */
static Window
check_netwm_supported(Window root)
{
	if (check_supported(root, _XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK))
		return root;
	return check_nonrecursive(root, _XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW, 0);
}

/** @brief Check for an EWMH/NetWM compliant (sorta) window manager.
  */
static Window
check_netwm(Window root)
{
	int i = 0;
	Window check = None;

	do {
		check = check_recursive(root, _XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW, 0);
	}
	while (i++ < 2 && !check);

	if (!check || check == root)
		check = check_netwm_supported(root);

	return (check);
}

/** @brief Check for a non-compliant GNOME/WinWM window manager.
  *
  * There are quite a few window managers that implement part of the GNOME/WinWM
  * specification but do not fill in the _WIN_SUPPORTING_WM_CHECK.  This is
  * another big annoyance.  One way to test this is whether there is a
  * _WIN_PROTOCOLS on the root window that does not include
  * _WIN_SUPPORTING_WM_CHECK in its list of atoms.
  */
static Window
check_winwm_supported(Window root)
{
	if (check_supported(root, _XA_WIN_PROTOCOLS, _XA_WIN_SUPPORTING_WM_CHECK))
		return root;
	return check_nonrecursive(root, _XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL, 0);
}

/** @brief Check for a GNOME1/WMH/WinWM compliant window manager.
  */
static Window
check_winwm(Window root)
{
	int i = 0;
	Window check = None;

	do {
		check = check_recursive(root, _XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL, 0);
	}
	while (i++ < 2 && !check);

	if (!check || check == root)
		check = check_winwm_supported(root);

	return (check);
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker(Window root)
{
	int i = 0;
	Window check = None;

	do {
		check = check_recursive(root, _XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW, 0);
	}
	while (i++ < 2 && !check);

	return (check);
}

/** @brief Check for an OSF/Motif compliant window manager.
  * @param root - the root window of the screen
  * @return Window - the check window or None
  *
  * This is really supposed to be recursive, however, Lesstif sets the window to
  * the root window which makes it automatically recursive.
  */
static Window
check_motif(Window root)
{
	int i = 0;
	Window check = None;

	do {
		check = check_recursive(root, _XA_MOTIF_WM_INFO, AnyPropertyType, 1);
	}
	while (i++ < 2 && !check);

	return (check);
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  * @param root - the root window of the screen
  * @param selection - the WM_S%d selection atom
  * @return Window - the selection owner window or None
  */
static Window
check_icccm(Window root, Atom selection)
{
	return XGetSelectionOwner(dpy, selection);
}

/** @brief Check whether an ICCCM window manager is present.
  * @param root - the root window of the screen
  * @return Window - the root window or None
  *
  * This pretty much assumes that any ICCCM window manager will select for
  * SubstructureRedirectMask on the root window.
  */
static Window
check_redir(Window root)
{
	XWindowAttributes wa;
	Window check = None;

	if (XGetWindowAttributes(dpy, root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			check = root;
	return (check);
}

/** @brief check whether a window manager is present
  * @return Bool - true if a window manager is present on any screen
  */
static Bool
check_wm(void)
{
	int s;

	for (s = 0; s < nscr; s++) {
		Window root = roots[s];

		if (check_redir(root))
			return True;
		if (check_icccm(root, _XA_WM_Sd[s]))
			return True;
		if (check_motif(root))
			return True;
		if (check_maker(root))
			return True;
		if (check_winwm(root))
			return True;
		if (check_netwm(root))
			return True;
	}
	return False;
}


int check_iterations;

static gboolean
recheck_wm(gpointer user_data)
{
	if (check_wm() || !check_iterations--) {
		gtk_main_quit();
		return FALSE; /* remove source */
	}
	return TRUE; /* continue checking */
}

void
waitwm()
{
	char buf[16] = { 0, };
	int s;

	if (!options.wait)
		return;

	dpy = gdk_x11_get_default_xdisplay();

	_XA_NET_SUPPORTING_WM_CHECK = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	_XA_NET_SUPPORTED = XInternAtom(dpy, "_NET_SUPPORTED", False);
	_XA_WIN_SUPPORTING_WM_CHECK = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
	_XA_WIN_PROTOCOLS = XInternAtom(dpy, "_WIN_PROTOCOLS", False);
	_XA_WINDOWMAKER_NOTICEBOARD = XInternAtom(dpy, "_WINDOWMAKER_NOTICEBAORD", False);
	_XA_MOTIF_WM_INFO = XInternAtom(dpy, "_MOTIF_WM_INFO", False);

	nscr = ScreenCount(dpy);
	_XA_WM_Sd = calloc(nscr, sizeof(*_XA_WM_Sd));
	roots = calloc(nscr, sizeof(*roots));

	for (s = 0; s < nscr; s++) {
		snprintf(buf, sizeof(buf), "WM_S%d", s);
		_XA_WM_Sd[s] = XInternAtom(dpy, buf, False);
		roots[s] = RootWindow(dpy, s);
	}
	if (!check_wm()) {
		check_iterations = 3;
		g_timeout_add_seconds(1, recheck_wm, NULL);
		gtk_main();
	}
	free(_XA_WM_Sd);
	_XA_WM_Sd = NULL;
	free(roots);
	roots = NULL;
}

static gboolean
pause_done(gpointer user_data)
{
	gtk_main_quit();
	return FALSE;		/* remove source */
}


void
pause_after_wm()
{
	if (!options.pause)
		return;

	g_timeout_add(options.pause, pause_done, NULL);
	gtk_main();
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
    %1$s [options]\n\
    %1$s {-h|--help}\n\
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
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: 0]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: 1]\n\
        this option may be repeated.\n\
", argv[0]);
}

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	while(1) {
		int c, val;
#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
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

		c = getopt_long_only(argc, argv, "nD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "nDvhVC?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n",
					argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n",
					argv[0]);
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
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(0);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n",
					argv[0]);
			version(argc, argv);
			exit(0);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n",
					argv[0]);
			copying(argc, argv);
			exit(0);
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '",
						argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument",
						argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind < argc) {
		goto bad_nonopt;
	}
	exit(0);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
