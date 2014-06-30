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

#include "xde-autostart.h"

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

typedef enum _LogoSide {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_BOTTOM,
} LogoSide;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *display;
	char *desktop;
	char *session;
	char **execute;
	int commands;
	char *file;
	Bool autostart;
	Bool wait;
	unsigned int pause;
	char *banner;
	Bool splash;
	unsigned int guard;
	unsigned int delay;
	char *charset;
	char *language;
	LogoSide side;
	char *icon_theme;
	char *gtk2_theme;
	Bool usexde;
	Bool foreground;
	char *client_id;
        char **desktops;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.display = NULL,
	.desktop = NULL,
	.session = NULL,
	.execute = NULL,
	.commands = 0,
	.file = NULL,
	.autostart = True,
	.wait = True,
	.pause = 2,
	.banner = NULL,
	.splash = True,
	.guard = 200,
	.delay = 0,
	.charset = NULL,
	.language = NULL,
	.side = LOGO_SIDE_TOP,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.usexde = False,
	.foreground = False,
	.client_id = NULL,
        .desktops = NULL,
};

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

GHashTable *autostarts;

char **
get_autostart_dirs(int *np)
{
	char *home, *xhome, *xconf, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_CONFIG_HOME");
	xconf = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";

	len = (xconf ? strlen(xconf) : strlen(home) + strlen("/.config")) + strlen(xconf) + 2;
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
	for (n = 0, pos = dirs; pos < end; n++,
	     *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
        xdg_dirs = calloc(n, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1) {
		len = strlen(pos) + strlen("/autostart") + 1;
		xdg_dirs[n] = calloc(len, sizeof(*xdg_dirs[n]));
		strcpy(xdg_dirs[n], pos);
		strcat(xdg_dirs[n], "/autostart");
	}
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

static void
autostart_key_free(gpointer data)
{
        free(data);
}

static void
autostart_value_free(gpointer entry)
{
        g_key_file_free((GKeyFile *) entry);
}

static void
get_autostart_entry(GHashTable *autostarts, const char *key, const char *file)
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
        if (!g_key_file_has_key(entry, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
                EPRINTF("%s: has no Type= section\n", file);
                g_key_file_free(entry);
                return;
        }
        DPRINTF("got autostart file: %s (%s)\n", key, file);
        g_hash_table_replace(autostarts, strdup(key), entry);
        return;
}

/** @brief wean out entries that should not be used
  * @param key - pointer to the application id of the entry
  * @param value - pointer to the key file entry
  * @return gboolean - TRUE if removal required, FALSE otherwise
  */
static gboolean
autostarts_filter(gpointer key, gpointer value, gpointer user_data)
{
        char *appid = (char *) key;
        GKeyFile *entry = (GKeyFile *) value;
        gchar *name, *exec, *tryexec, *binary;
	gchar **item, **list, **desktop;

        if (!(name = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_NAME, NULL))) {
                DPRINTF("%s: no Name\n", appid);
                return TRUE;
        }
        g_free(name);
        if (!(exec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_EXEC, NULL))) {
                /* TODO: handle DBus activation */
                DPRINTF("%s: no Exec\n", appid);
                return TRUE;
        }
        if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL)) {
                DPRINTF("%s: is Hidden\n", appid);
                return TRUE;
        }
#if 0
        /* NoDisplay can be used to hide desktop entries from the application
         * menu and does not indicate that it should not be executed as an
         * autostart entry.  Use Hidden for that. */
        if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL)) {
                DPRINTF("%s: is NoDisplay\n", appid);
                return TRUE;
        }
#endif
	if ((list = g_key_file_get_string_list(entry, G_KEY_FILE_DESKTOP_GROUP,
					       G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL, NULL))) {
                desktop = NULL;
		for (item = list; *item; item++) {
			for (desktop = options.desktops; *desktop; desktop++)
				if (!strcmp(*item, *desktop))
					break;
			if (*desktop)
				break;
		}
		g_strfreev(list);
		if (!*desktop) {
			DPRINTF("%s: %s not in OnlyShowIn\n", appid, options.desktop);
			return TRUE;
		}
	}
	if ((list = g_key_file_get_string_list(entry, G_KEY_FILE_DESKTOP_GROUP,
					       G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL, NULL))) {
                desktop = NULL;
		for (item = list; *item; item++) {
			for (desktop = options.desktops; *desktop; desktop++)
				if (!strcmp(*item, *desktop))
					break;
			if (*desktop)
				break;
		}
		g_strfreev(list);
		if (*desktop) {
			DPRINTF("%s: %s in NotShowIn\n", appid, *desktop);
			return TRUE;
		}
	}
        if ((tryexec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL))) {
                        binary = g_strdup(tryexec);
                        g_free(tryexec);
        } else {
                char *p;

                /* parse the first word of the exec statement and see whether it
                 * is executable or can be found in PATH */
                binary = g_strdup(exec);
                if ((p = strpbrk(binary, " \t")))
                        *p = '\0';
        }
        g_free(exec);
        if (binary[0] == '/') {
                if (access(binary, X_OK)) {
                        DPRINTF("%s: %s: %s\n", appid, binary, strerror(errno));
                        DPRINTF("%s: not executable\n", appid);
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
                        // too much noise
                        // DPRINTF("%s: %s: %s\n", appid, file, strerror(errno);
                }
                free(path);
                if (!execok) {
                        DPRINTF("%s: %s: not in executable in path\n", appid, binary);
                        g_free(binary);
                        return TRUE;
                }
        }
        return FALSE;
}

GHashTable *
get_autostarts(void)
{
        char **xdg_dirs, **dirs;
        int i, n = 0;
        static const char *suffix = ".desktop";
        static const int suflen = 8;

        if (!(xdg_dirs = get_autostart_dirs(&n)) || !n)
                return (autostarts);

        autostarts = g_hash_table_new_full(g_str_hash, g_str_equal,
                        autostart_key_free, autostart_value_free);

        /* got through them backward */
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
                        get_autostart_entry(autostarts, key, file);
                        free(key);
                        free(file);
                }
                closedir(dir);
        }
        for (i = 0; i < n; i++)
                free(xdg_dirs[i]);
        free(xdg_dirs);
#if 0
        /* don't filter stuff out because we want to show non-autostarted appids
         * as insensitive buttons. */
        g_hash_table_foreach_remove(autostarts, autostarts_filter, NULL);
#endif
        return (autostarts);
}

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

GtkWidget *splash;
GtkWidget *table;

typedef struct {
        int cols;       /* columns in the table */
        int rows;       /* rows in the table */
        int col;        /* column index of the next free cell */
        int row;        /* row index of the next free cell */
} TableContext;

void
create_splashscreen(TableContext *c)
{
	splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(splash), "xde-autostart", "XDE-Autostart");
#if 0
	unique_app_watch_window(uapp, GTK_WINDOW(splash));
	g_signal_connect(G_OBJECT(uapp), "message-received", G_CALLBACK(message_received), (gpointer) NULL);
#endif
	gtk_window_set_gravity(GTK_WINDOW(splash), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(splash), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_container_set_border_width(GTK_CONTAINER(splash), 20);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(splash), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(splash), TRUE);
	gtk_window_set_keep_below(GTK_WINDOW(splash), TRUE);
	gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER_ALWAYS);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(splash), GTK_WIDGET(hbox));

	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);

	GtkWidget *img = gtk_image_new_from_file(options.banner);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(img), FALSE, FALSE, 0);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(GTK_WIDGET(sw), 800, -1);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(sw), TRUE, TRUE, 0);

	table = gtk_table_new(c->rows, c->cols, TRUE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 1);
	gtk_table_set_row_spacings(GTK_TABLE(table), 1);
	gtk_table_set_homogeneous(GTK_TABLE(table), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(table), 750, -1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(table));

	gtk_window_set_default_size(GTK_WINDOW(splash), -1, 600);
	gtk_widget_show_all(GTK_WIDGET(splash));
	gtk_widget_show_now(GTK_WIDGET(splash));
	relax();
}

static GtkWidget *
get_icon(GtkIconSize size, const char *name, const char *stock)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GtkWidget *image = NULL;

	if (!image && name && gtk_icon_theme_has_icon(theme, name))
		image = gtk_image_new_from_icon_name(name, size);
	if (!image && stock)
		image = gtk_image_new_from_stock(stock, size);
	return (image);
}

typedef struct {
        GtkWidget *button;      /* button corresponding to this task */
        guint timer;            /* timer source for blinking button */
        const char *appid;      /* application id */
        GKeyFile *entry;        /* key file entry */
        gboolean runnable;      /* whether the entry is runnable */
} TaskState;

static gboolean
blink_button(gpointer user_data)
{
	TaskState *task = (typeof(task)) user_data;

	if (!task->button) {
		task->timer = 0;
		return G_SOURCE_REMOVE;
	}
	if (gtk_widget_is_sensitive(task->button))
		gtk_widget_set_sensitive(task->button, FALSE);
	else
		gtk_widget_set_sensitive(task->button, TRUE);
	return G_SOURCE_CONTINUE;
}

void
add_task_to_splash(TableContext *c, TaskState *task)
{
	char *name;
	GtkWidget *icon, *but;

	name = g_key_file_get_string(task->entry, G_KEY_FILE_DESKTOP_GROUP,
				     G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
	icon = get_icon(GTK_ICON_SIZE_DIALOG, name, "gtk-missing-image");

	name = g_key_file_get_locale_string(task->entry, G_KEY_FILE_DESKTOP_GROUP,
					    G_KEY_FILE_DESKTOP_KEY_NAME, options.language, NULL);
	if (!name)
		name = g_strdup(task->appid);

	but = task->button = gtk_button_new();
	gtk_button_set_image_position(GTK_BUTTON(but), GTK_POS_TOP);
	gtk_button_set_image(GTK_BUTTON(but), GTK_WIDGET(icon));
#if 1
	gtk_button_set_label(GTK_BUTTON(but), name);
#endif
	gtk_widget_set_tooltip_text(GTK_WIDGET(but), name);

	g_free(name);

	gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(but),
				  c->col, c->col + 1, c->row, c->row + 1);
	gtk_widget_set_sensitive(GTK_WIDGET(but), task->runnable);
	gtk_widget_show_all(GTK_WIDGET(but));
	gtk_widget_show_now(GTK_WIDGET(but));
	relax();

	if (++c->col >= c->cols) {
		c->row++;
		c->col = 0;
	}

        if (task->runnable)
                task->timer = g_timeout_add_seconds(1, blink_button, task);
}

void
do_autostarts()
{
        GHashTable *autostarts;
        TaskState *task;
        GHashTableIter hiter;
        gpointer key, value;
        int count;
        TableContext *c = NULL;


        if (!(autostarts = get_autostarts())) {
                EPRINTF("cannot build AutoStarts\n");
                return;
        }
        if (!(count = g_hash_table_size(autostarts))) {
                EPRINTF("cannot fine any AutoStarts\n");
                return;
        }
        if (options.splash) {
                c = calloc(1, sizeof(*c));

                c->cols = 7; /* seems like a good number */
                c->rows = (count + c->cols - 1) /c->cols;
                c->col = 0;
                c->row = 0;

                create_splashscreen(c);
        }

        g_hash_table_iter_init(&hiter, autostarts);
        while (g_hash_table_iter_next(&hiter, &key, &value)) {

                task = calloc(1, sizeof(*task));
                task->appid = key;
                task->entry = value;
                task->runnable = !autostarts_filter(key, value, NULL);

                if (options.splash)
                        add_task_to_splash(c, task);

                if (!task->runnable) {
                        free(task);
                        continue;
                }

                /* FIXME: actually launch the task */

        }
}

void
do_executes()
{
}

void
run_autostart(int argc, char *argv[])
{
        gtk_init(NULL, NULL);
        // gtk_init(argc, argv);

	/* FIXME: write me */
        do_executes();
        do_autostarts();

        gtk_main();
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
    %1$s [OPTIONS]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

static const char *
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

static const char *
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
    %1$s [OPTIONS]\n\
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
    -d, --display DISPLAY               (%2$s)\n\
        specify the X display to use\n\
    -e, --desktop DESKTOP               (%3$s)\n\
        specify the desktop environment\n\
    -s, --session SESSION               (%4$s)\n\
        specify the lxsesion(1) compatible session\n\
    -x, --exec COMMAND\n\
        specify commands to execute before autostart\n\
        this option may be repeated\n\
    -f, --file FILE                     (%5$s)\n\
        specify file from which to read commands\n\
    -a, --noautostart | --autostart     (%6$s)\n\
        (do not) perform XDG autostart of tasks\n\
    -w, --wait | --nowait               (%7$s)\n\
        wait for window manager to start\n\
    -p, --pause [PAUSE]                 (%8$u sec)\n\
        pause for PAUSE seconds before running\n\
    -l, --splash [IMAGE] | --nosplash   (%9$s)\n\
        display (or not) startup splash page\n\
    -W, --toolwait [GUARD][:DELAY]      (%10$u:%11$u)\n\
        perform xtoolwait on autostart tasks\n\
        GUARD is the longest time to wait for tool start\n\
        DELAY is the time to wait for unknown startup\n\
    -c, --charset CHARSET               (%12$s)\n\
        specify the character set\n\
    -L, --language LANG                 (%13$s)\n\
        specify the language for XDG translations\n\
    -b, --banner BANNER                 (%14$s)\n\
        specify custom autostart branding\n\
    -S, --side {left|right|top|bottom}  (%15$s)\n\
        specify side of splash for logo placement\n\
    -i, --icons THEME                   (%16$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME                   (%17$s)\n\
        set the GTK+ theme to use\n\
    -X, --xde-theme                     (%18$s)\n\
        use the XDE desktop theme for the selection window\n\
    -F, --foreground                    (%19$s)\n\
        execute in the foreground for debugging\n\
    --client-id CLIENTID                (%20$s)\n\
        client ID for X Session Management\n\
    -n, --dry-run                       (%21$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]                 (%22$d)\n\
        increment or set debug LEVEL\n\
        this option may be repeated.\n\
    -v, --verbose [LEVEL]               (%23$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
", argv[0]
		       , options.display, options.desktop, options.session ? : "",
		       options.file ? : "", show_bool(options.autostart)
		       , show_bool(options.wait)
		       , options.pause, options.splash ? options.banner : "false", options.guard,
		       options.delay, options.charset, options.language, options.banner,
		       show_side(options.side)
		       , options.icon_theme ? : "auto", options.gtk2_theme ? : "auto",
		       show_bool(options.usexde)
		       , show_bool(options.foreground)
		       , options.client_id ? : "", show_bool(options.dryrun)
		       , options.debug, options.output);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n)
		return;

	pfx = getenv("XDG_MENU_PREFIX") ? : "";

	free(options.banner);
	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

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

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

/** @brief set default file list
  *
  * This can only be run after options processing...
  */
void
set_default_file(void)
{
	char **xdg_dirs, **dirs, *file, *files;
	int i, size, n = 0, next;

	if (options.file)
		return;
	if (!options.session)
		return;

	if (!(xdg_dirs = get_config_dirs(&n)) || !n)
		return;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	files = NULL;
	size = 0;
	next = 0;

	/* process in reverse order */
	for (i = n - 1, dirs = &xdg_dirs[i]; i >= 0; i--, dirs--) {
		strncpy(file, *dirs, PATH_MAX);
		strncat(file, "/lxsession/", PATH_MAX);
		strncat(file, options.session, PATH_MAX);
		strncat(file, "/autostart", PATH_MAX);
		if (access(file, R_OK)) {
			DPRINTF("%s: %s\n", file, strerror(errno));
			continue;
		}
		size += strlen(file) + 1;
		files = realloc(files, size * sizeof(*files));
		if (next)
			strncat(files, ":", size);
		else {
			*files = '\0';
			next = 1;
		}
		strncat(files, file, size);
	}
	options.file = files;

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
split_desktops(void)
{
	char **desktops, *copy, *pos, *end;;
	int n;

	copy = strdup(options.desktop);

	for (n = 0, pos = copy, end = pos + strlen(pos); pos < end;
	     n++, *strchrnul(pos, ';') = '\0', pos += strlen(pos) + 1) ;

        desktops = calloc(n + 1, sizeof(*desktops));

        for (n = 0, pos = copy; pos < end; n++, pos += strlen(pos) + 1)
                desktops[n] = strdup(pos);

        free(copy);

        free(options.desktops);
        options.desktops = desktops;
}

void
set_defaults(void)
{
	char *p, *a;
	const char *env;

	set_default_banner();
	if ((options.language = setlocale(LC_ALL, ""))) {
		options.language = strdup(options.language);
		a = strchrnul(options.language, '@');
		if ((p = strchr(options.language, '.')))
			strcpy(p, a);
	}
	options.charset = strdup(nl_langinfo(CODESET));
	options.display = strdup(getenv("DISPLAY") ? : "");
	if ((env = getenv("XDG_CURRENT_DESKTOP")))
		options.desktop = strdup(env);
	else
		options.desktop = strdup("XDE");
}

typedef enum {
	COMMAND_AUTOSTART,
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_COPYING,
} CommandType;

int
main(int argc, char *argv[])
{
	CommandType command = COMMAND_AUTOSTART;

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"display",	required_argument,	NULL, 'd'},
			{"desktop",	required_argument,	NULL, 'e'},
			{"session",	required_argument,	NULL, 's'},
			{"exec",	required_argument,	NULL, 'x'},
			{"file",	required_argument,	NULL, 'f'},
			{"noautostart",	no_argument,		NULL, 'a'},
			{"autostart",	no_argument,		NULL, '1'},
			{"wait",	no_argument,		NULL, 'w'},
			{"nowait",	no_argument,		NULL, '2'},
			{"pause",	optional_argument,	NULL, 'p'},
			{"splash",	optional_argument,	NULL, 'l'},
			{"nosplash",	no_argument,		NULL, '4'},

			{"toolwait",	optional_argument,	NULL, 'W'},

			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'L'},
			{"banner",	required_argument,	NULL, 'b'},
			{"side",	required_argument,	NULL, 'S'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"xde-theme",	no_argument,		NULL, 'X'},

			{"foreground",	no_argument,		NULL, 'F'},
			{"client-id",	required_argument,	NULL, '3'},

			{"dryrun",	no_argument,		NULL, 'n'},
			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "d:e:s:x:f:awp::l::W::c:L:b:S:i:t:XFnD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "d:e:s:x:f:a1w2p:l:4W:c:L:b:S:i:t:XF3:nDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'd':	/* -d, --display DISPLAY */
			free(options.display);
			options.display = strdup(optarg);
			setenv("DISPLAY", optarg, 1);
			break;
		case 'e':	/* -e, --desktop DESKTOP */
			free(options.desktop);
			options.desktop = strdup(optarg);
			break;
		case 's':	/* -s, --session SESSION */
			free(options.session);
			options.session = strdup(optarg);
			break;
		case 'x':	/* -x, --exec COMMAND */
			options.execute = realloc(options.execute, (options.commands + 1) *
						  sizeof(*options.execute));
			options.execute[options.commands++] = strdup(optarg);
			break;
		case 'f':	/* -f, --file FILE */
			free(options.file);
			options.file = strdup(optarg);
			break;
		case 'a':	/* -a, --noautostart */
			options.autostart = False;
			break;
		case '2':	/* --autostart */
			options.autostart = True;
			break;
		case 'p':	/* -p, --pause [PAUSE] */
			if (optarg)
				options.pause = strtoul(optarg, NULL, 0);
			break;
		case 'l':	/* -l, --splash [IMAGE] */
			if (optarg) {
				free(options.banner);
				options.banner = strdup(optarg);
			}
			options.splash = True;
			break;

		case '4':	/* --nosplash */
			options.splash = False;
			break;
		case 'W':	/* -W, --toolwait [GUARD][:DELAY] */
			if (optarg) {
				char *g, *d;

				g = strdup(optarg);
				d = strchrnul(g, ':');
				if (*d) {
					*d++ = '\0';
					options.delay = strtoul(d, NULL, 0);
				}
				if (*g) {
					options.guard = strtoul(g, NULL, 0);
				}
			}
			break;
		case 'c':	/* -c --charset CHARSET */
			free(options.charset);
			options.charset = strdup(optarg);
			break;
		case 'L':	/* -L, --language LANG */
			free(options.language);
			options.language = strdup(optarg);
			break;
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			break;
		case 'S':	/* -S, --side {top|bottom|left|right} */
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
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 'X':	/* -X, --xde-theme */
			options.usexde = True;
			break;
		case 'F':	/* -F, --foreground */
			options.foreground = True;
			break;
		case '3':	/* --client-id CLIENTID */
			free(options.client_id);
			options.client_id = strdup(optarg);
			break;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
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
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
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
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_HELP;
			break;
		case 'V':	/* -V, --version */
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_VERSION;
			break;
		case 'C':	/* -C, --copying */
			if (command != COMMAND_AUTOSTART)
				goto bad_option;
			command = COMMAND_COPYING;
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
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
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
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", argv[0]);
		goto bad_nonopt;
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	set_default_file();
        split_desktops();
	switch (command) {
	case COMMAND_AUTOSTART:
		if (options.debug)
			fprintf(stderr, "%s: running autostart\n", argv[0]);
		run_autostart(argc, argv);
		break;
	case COMMAND_HELP:
		if (options.debug)
			fprintf(stderr, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case COMMAND_VERSION:
		if (options.debug)
			fprintf(stderr, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case COMMAND_COPYING:
		if (options.debug)
			fprintf(stderr, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(0);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
