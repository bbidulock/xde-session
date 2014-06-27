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

#include "xde-chooser.h"

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

enum {
	CHOOSE_RESULT_LOGOUT,
};

typedef enum _ChooserSide {
	CHOOSER_SIDE_LEFT,
	CHOOSER_SIDE_TOP,
	CHOOSER_SIDE_RIGHT,
	CHOOSER_SIDE_BOTTOM,
} ChooserSide;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	Bool prompt;
	char *banner;
	ChooserSide side;
	Bool noask;
	char *charset;
	char *language;
	Bool dflt;
	char *icon_theme;
	char *gtk2_theme;
	Bool execute;
	char *current;
	Bool managed;
	char *choice;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.banner = NULL,
};

int choose_result;

typedef struct {
} XSessionEntry;

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
		strcat(dirs, "/.local/shoare");
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
			DPRINTF("%s: %s: %s\n", appid, file, strerror(errno));
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
	size_t entries = 0;

	if (!(xdg_dirs = get_xsession_dirs(&n)) || !n)
		return (xsessions);

	xsessions = g_hash_table_new_full(g_str_hash, g_str_equal,
					  xsession_key_free, xsession_value_free);

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
			get_xsession_entry(xsessions, key, file);
			free(key);
			free(file);
		}
		closedir(dir);
	}
	for (i = 0; i < entries; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
	g_hash_table_foreach_remove(xsessions, xsessions_filter, NULL);
	return (xsessions);
}

gboolean
choose_xsession(void)
{
	GHashTable *xsessions;
	gchar **keys;
	guint length = 0;

	if (!(xsessions = get_xsessions()))
		return FALSE;
	if (!(keys = (typeof(keys)) g_hash_table_get_keys_as_array(xsessions, &length))) {
		return FALSE;
	}
	return TRUE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent * event, gpointer data)
{
	choose_result = CHOOSE_RESULT_LOGOUT;
	gtk_main_quit();
	return TRUE;		/* propagate */
}

gboolean
on_expose_event(GtkWidget *widget, GdkEvent * event, gpointer data)
{
	GdkPixbuf *pixbuf = GDK_PIXBUF(data);
	GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(widget));
	cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(window));

	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);
	GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0 };
	gdk_cairo_set_source_color(cr, &color);
	cairo_paint_with_alpha(cr, 0.7);
	return TRUE;		/* propagate */
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
on_managed_toggle(GtkCellRendererToggle *rend, gchar * path, gpointer user_data)
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

	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GdkPixbuf *pixbuf = NULL;

	has = gtk_icon_theme_has_icon(theme, name);
	if (has)
		pixbuf = gtk_icon_theme_load_icon(theme, name, 32,
						  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
						  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	if (!has || !pixbuf) {
		g_free(name);
		name = g_strdup("preferences-system-windows");
		has = gtk_icon_theme_has_icon(theme, name);
		if (has)
			pixbuf = gtk_icon_theme_load_icon(theme, name, 32,
							  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
							  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
		if (!has || !pixbuf) {
			GtkWidget *image;

			image = gtk_image_new_from_stock("gtk-missing-image",
							 GTK_ICON_SIZE_LARGE_TOOLBAR);
			pixbuf = gtk_widget_render_icon(GTK_WIDGET(image),
							"gtk-missing-image",
							GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
			g_object_unref(G_OBJECT(image));
		}
	}
	g_object_unref(G_OBJECT(theme));
	g_value_init(&pixbuf_v, G_TYPE_OBJECT);
	g_value_take_object(&pixbuf_v, pixbuf);
	g_object_set_property(G_OBJECT(cell), "pixbuf", &pixbuf_v);
	g_value_unset(&pixbuf_v);
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
				fprintf(f, "%s\n", label);
				gtk_widget_set_sensitive(buttons[1], FALSE);
				gtk_widget_set_sensitive(buttons[2], FALSE);
				gtk_widget_set_sensitive(buttons[3], TRUE);
				fclose(f);
			}
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
	if (options.dflt) {
		GtkTreeIter iter;
		gboolean valid;

		for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, 0);
		     valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter,
						 COLUMN_LABEL, &label_v);
			label = g_value_get_string(&label_v);
			if (!strcmp(label, options.dflt)) {
				g_value_unset(&label_v);
				break;
			}
			g_value_unset(&label_v);
		}
		if (valid) {
			gchar *string;
			GtkTreePath *path;

			gtk_tree_selection_select_iter(GTK_TREE_SELECTION(selection), &iter);
			string = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);
			path = gtk_tree_path_new_from_string(string);
			g_free(string);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(view), path, cursor, NULL,
							 FALSE);
			gtk_tree_path_free(path);
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
		free(options.choice);
		options.choice = strdup(label);
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
		if (label && !strcmp(label, options.dflt)) {
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
on_row_activated(GtkTreeView *view, GtkTreePath * path, GtkTreeViewColumn *col, gpointer user_data)
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
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
	}
}

static gboolean
on_button_press(GtkWidget *view, GdkEvent * event, gpointer user_data)
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

void
make_login_choice(int argc, char *argv[])
{
	{
		int status;

		status = system("xsetroot -cursor_name left_ptr");
		(void) status;
	}
	gtk_init(&argc, &argv);

	GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_wmclass(GTK_WINDOW(w), "xde-chooser", "XDE-Chooser");
	gtk_window_set_title(GTK_WINDOW(w), "Window Manager Selection");
	gtk_window_set_modal(GTK_WINDOW(w), TRUE);
	gtk_window_set_gravity(GTK_WINDOW(w), GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_icon_name(GTK_WINDOW(w), "xdm");
#if 0
	gtk_container_set_border_width(GTK_CONTAINER(w), 15);
#endif
	gtk_window_set_skip_pager_hint(GTK_WINDOW(w), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(w), TRUE);
	gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_fullscreen(GTK_WINDOW(w));
	gtk_window_set_decorated(GTK_WINDOW(w), FALSE);

	GdkScreen *scrn = gdk_screen_get_default();
	GdkWindow *root = gdk_screen_get_root_window(scrn);
	gint width = gdk_screen_get_width(scrn);
	gint height = gdk_screen_get_height(scrn);

	gtk_window_set_default_size(GTK_WINDOW(w), width, height);
	gtk_widget_set_app_paintable(GTK_WIDGET(w), TRUE);

	GdkPixbuf *pixbuf = gdk_pixbuf_get_from_drawable(NULL, root, NULL,
							 0, 0, 0, 0, width, height);

	GtkWidget *a = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

	gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(a));

	GtkWidget *e = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(a), GTK_WIDGET(e));
	gtk_widget_set_size_request(GTK_WIDGET(e), -1, 400);
	g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(on_expose_event),
			 (gpointer) pixbuf);

	GtkWidget *v = gtk_vbox_new(FALSE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(v), 15);
	gtk_container_add(GTK_CONTAINER(e), GTK_WIDGET(v));
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(on_delete_event), NULL);

	GtkWidget *h = gtk_hbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(v), GTK_WIDGET(h));

	if (options.banner) {
		GtkWidget *f = gtk_frame_new(NULL);

		gtk_frame_set_shadow_type(GTK_FRAME(f), GTK_SHADOW_ETCHED_IN);
		gtk_box_pack_start(GTK_BOX(h), GTK_WIDGET(f), FALSE, FALSE, 0);

		GtkWidget *v = gtk_vbox_new(FALSE, 5);

		gtk_container_set_border_width(GTK_CONTAINER(v), 10);
		gtk_container_add(GTK_CONTAINER(f), GTK_WIDGET(v));

		GtkWidget *s = gtk_image_new_from_file(options.banner);

		if (s)
			gtk_container_add(GTK_CONTAINER(v), GTK_WIDGET(s));
		else
			gtk_widget_destroy(GTK_WIDGET(f));
	}
	GtkWidget *f = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(f), GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_start(GTK_BOX(h), GTK_WIDGET(f), TRUE, TRUE, 0);
	v = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(v), 0);
	gtk_container_add(GTK_CONTAINER(f), GTK_WIDGET(v));

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 3);
	gtk_box_pack_start(GTK_BOX(v), GTK_WIDGET(sw), TRUE, TRUE, 0);

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(v), GTK_WIDGET(bb), FALSE, TRUE, 0);

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

	GtkWidget *i;
	GtkWidget *b;
	GtkWidget *buttons[5];

	buttons[0] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((getenv("DISPLAY") ? : "")[0] == ':') {
		if ((i = gtk_image_new_from_stock("gtk-quit", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), GTK_WIDGET(i));
		gtk_button_set_label(GTK_BUTTON(b), "Logout");
	} else {
		if ((i = gtk_image_new_from_stock("gtk-disconnect", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), GTK_WIDGET(i));
		gtk_button_set_label(GTK_BUTTON(b), "Disconnect");
	}
	gtk_box_pack_start(GTK_BOX(bb), GTK_WIDGET(b), TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_logout_clicked), buttons);

	buttons[1] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-save", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), GTK_WIDGET(i));
	gtk_button_set_label(GTK_BUTTON(b), "Make Default");
	gtk_box_pack_start(GTK_BOX(bb), GTK_WIDGET(b), TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_default_clicked), buttons);

	buttons[2] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), GTK_WIDGET(i));
	gtk_button_set_label(GTK_BUTTON(b), "Select Default");
	gtk_box_pack_start(GTK_BOX(bb), GTK_WIDGET(b), TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_select_clicked), buttons);

	buttons[3] = b = gtk_button_new();
	gtk_widget_set_can_default(GTK_WIDGET(b), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-ok", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), GTK_WIDGET(i));
	gtk_button_set_label(GTK_BUTTON(b), "Launch Session");
	gtk_box_pack_start(GTK_BOX(bb), GTK_WIDGET(b), TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_launch_clicked), buttons);

	gtk_widget_grab_default(GTK_WIDGET(b));

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
		gchar *iname = g_key_file_get_string(entry,
						     G_KEY_FILE_DESKTOP_GROUP,
						     G_KEY_FILE_DESKTOP_KEY_ICON,
						     NULL);
		gchar *name = g_key_file_get_locale_string(entry,
							   G_KEY_FILE_DESKTOP_GROUP,
							   G_KEY_FILE_DESKTOP_KEY_NAME,
							   NULL, NULL) ? : "";
		gchar *comment = g_key_file_get_locale_string(entry,
							      G_KEY_FILE_DESKTOP_GROUP,
							      G_KEY_FILE_DESKTOP_KEY_COMMENT,
							      NULL, NULL) ? : "";
		gchar *markup = g_strdup_printf("<b>%s</b>\n%s",
						name, comment);
		gboolean managed = g_key_file_get_boolean(entry,
							  "Window Manager",
							  "X-XDE-Managed",
							  NULL);

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter,
				   COLUMN_PIXBUF, iname,
				   COLUMN_NAME, name,
				   COLUMN_COMMENT, comment,
				   COLUMN_MARKUP, markup,
				   COLUMN_LABEL, label,
				   COLUMN_MANAGED, managed, COLUMN_ORIGINAL, managed, -1);
		g_free(iname);
		g_free(name);
		g_free(comment);
		g_free(markup);
		if (!strcmp(options.choice, label) ||
		    ((!strcmp(options.choice, "choose") ||
		      !strcmp(options.choice, "default")) && !strcmp(options.dflt, label))) {
			gchar *string;
			GtkTreePath *path;

			gtk_tree_selection_select_iter(selection, &iter);
			string = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);
			path = gtk_tree_path_new_from_string(string);
			g_free(string);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(view), path, cursor, NULL,
							 FALSE);
			gtk_tree_path_free(path);
		}
	}
	// gtk_window_set_default_size(GTK_WINDOW(w), -1, 400);
	gtk_widget_show_all(GTK_WIDGET(w));
	gtk_widget_grab_focus(GTK_WIDGET(buttons[3]));

	/* TODO: we should really set a timeout and if no user interaction has
	   occured before the timeout, we should continue if we have a viable
	   default or choice. */
	gtk_main();
	gtk_widget_destroy(GTK_WIDGET(w));

	GKeyFile *entry = NULL;

	if (strcmp(options.current, "logout")) {
		if (!(entry = (typeof(entry)) g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s\n", options.current);
			exit(EXIT_FAILURE);
		}
	}
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
    %1$s [OPTIONS] [SESSION]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    SESSION\n\
        The name of the XDG session to execute, or \"default\"\n\
	or \"choose\".  When unspecified defaults to \"default\"\n\
	\"default\" means execute default without prompting\n\
	\"choose\" means post a dialog to choose the session\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -p, --prompt\n\
        prompt for session regardless of SESSION argument\n\
    -b, --banner BANNER\n\
	specify custom login branding\n\
    -s, --side {left|top|right|bottom}\n\
        specify side  of dialog for logo placement\n\
    -n, --noask\n\
        do not ask to set session as default\n\
    -c, --charset CHARSET\n\
	specify the character set, defaults to current locale\n\
    -l, --language LANGUAGE\n\
        specify the language, defaults to current locale\n\
    -d, --default\n\
        set the future default to the SESSION argument or choice\n\
    -i, --icons THEME\n\
        set the icon theme to use, default to current setting\n\
    -t, --theme THEME\n\
        set the gtk+ theme to use, default to current setting\n\
    -e, --exec\n\
        execute the Exec= statement instead of returning as string\n\
	indicating the selected XSession\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: 0]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: 1]\n\
        this option may be repeated.\n\
", argv[0]);
}

void
set_default_banner(void)
{
	char *home, *xhome, *xdata, *dirs, *pos, *end, *pfx, *file, *banner = NULL;
	int len, n;
	struct stat st;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	pfx = getenv("XDG_MENU_PREFIX") ? : "";
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
	for (n = 0, pos = dirs; pos < end; n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) {
		*strchrnul(pos, ':') = '\0';
		if (!*pos)
			continue;
		strncpy(file, pos, PATH_MAX);
		strncat(file, "/images/", PATH_MAX);
		strncat(file, pfx, PATH_MAX);
		strncat(file, "banner.png", PATH_MAX);
		if (stat(file, &st)) {
			DPRINTF("%s: %s\n", file, strerror(errno));
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			DPRINTF("%s: not a file\n", file);
			continue;
		}
		banner = strdup(file);
		break;
	}
	if (!banner && *pfx) {
		pfx = "";
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
		     n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) {
			*strchrnul(pos, ':') = '\0';
			if (!*pos)
				continue;
			strncpy(file, pos, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "banner.png", PATH_MAX);
			if (stat(file, &st)) {
				DPRINTF("%s: %s\n", file, strerror(errno));
				continue;
			}
			if (!S_ISREG(st.st_mode)) {
				DPRINTF("%s: not a file\n", file);
				continue;
			}
			banner = strdup(file);
			break;
		}
	}
	free(dirs);
	free(file);
	options.banner = banner;
}

void
set_defaults(void)
{
	set_default_banner();
}

int
main(int argc, char *argv[])
{
	set_defaults();
	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"prompt",	no_argument,		NULL, 'p'},
			{"banner",	required_argument,	NULL, 'b'},
			{"side",	required_argument,	NULL, 's'},
			{"noask",	no_argument,		NULL, 'n'},
			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'l'},
			{"default",	no_argument,		NULL, 'd'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"exec",	no_argument,		NULL, 'e'},

			{"dryrun",	no_argument,		NULL, 'N'},
			{"debug",	optional_argument,	NULL, 'D'},
			{"verbose",	optional_argument,	NULL, 'v'},
			{"help",	no_argument,		NULL, 'h'},
			{"version",	no_argument,		NULL, 'V'},
			{"copying",	no_argument,		NULL, 'C'},
			{"?",		no_argument,		NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "ND::v::hVCH?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "NDvhVCH?");
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
		case 's':	/* -s, --side {top|bottom|left|right} */
			if (!strncasecmp(optarg, "left", strlen(optarg))) {
				options.side = CHOOSER_SIDE_LEFT;
				break;
			}
			if (!strncasecmp(optarg, "top", strlen(optarg))) {
				options.side = CHOOSER_SIDE_TOP;
				break;
			}
			if (!strncasecmp(optarg, "right", strlen(optarg))) {
				options.side = CHOOSER_SIDE_RIGHT;
				break;
			}
			if (!strncasecmp(optarg, "bottom", strlen(optarg))) {
				options.side = CHOOSER_SIDE_BOTTOM;
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
		case 'l':	/* -l, --language LANGUAGE */
			free(options.language);
			options.language = strdup(optarg);
			break;
		case 'd':	/* -d, --default */
			options.dflt = True;
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

		case 'N':	/* -N, --dry-run */
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
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(0);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n", argv[0]);
			version(argc, argv);
			exit(0);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n", argv[0]);
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
