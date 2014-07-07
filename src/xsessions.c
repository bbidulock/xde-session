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

#include "xde-xsession.h"
#include "xsession.h"

GHashTable *xsessions;

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

static void
xsession_key_free(gpointer data)
{
	free(data);
}

static void
xsession_value_free(gpointer data)
{
	XSession *value = (typeof(value)) data;

	if (value->appid)
		free(value->appid);
	if (value->file)
		free(value->file);
	if (value->entry)
		g_key_file_free(value->entry);
	free(value);
}

GHashTable *
get_xsessions(void)
{
	char *dirs, *pos, *end, *path, *suffix;

	xsessions = g_hash_table_new_full(g_str_hash, g_str_equal,
					  xsession_key_free, xsession_value_free);

	dirs = strdup(xdg_dirs.data.both);
	end = dirs + strlen(dirs);

	path = calloc(PATH_MAX + 1, sizeof(*path));

	for (pos = dirs; pos < end; *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	for (pos = dirs; pos < end; pos += strlen(pos) + 1) {
		DIR *dir;
		struct dirent *d;
		char *key, *p;
		struct stat st;
		XSession *value;

		if (!*pos)
			continue;
		strncpy(path, pos, PATH_MAX);
		strncat(path, "/xsessions", PATH_MAX);
		suffix = path + strlen(path);
		if (!(dir = opendir(path))) {
			DPRINTF("%s: %s\n", path, strerror(errno));
			continue;
		}
		while ((d = readdir(dir))) {
			if (d->d_name[0] == '.')
				continue;
			if (!(p = strstr(d->d_name, ".desktop")) || p[8]) {
				DPRINTF("%s: no .desktop suffix\n", d->d_name);
				continue;
			}
			strcpy(suffix, "/");
			strcat(suffix, d->d_name);
			if (stat(path, &st)) {
				DPRINTF("%s: %s\n", path, strerror(errno));
				continue;
			}
			if (!S_ISREG(st.st_mode)) {
				DPRINTF("%s: %s\n", path, "not a regular file");
				continue;
			}
			if (access(path, R_OK)) {
				DPRINTF("%s: %s\n", path, strerror(errno));
				continue;
			}
			key = strdup(d->d_name);
			*strrchr(key, '.') = '\0';
			value = calloc(1, sizeof(*value));
			value->appid = strdup(key);
			value->file = strdup(path);
			value->entry = NULL;
			g_hash_table_replace(xsessions, key, value);
		}
		closedir(dir);
	}
	free(path);
	free(dirs);
	return (xsessions);
}

gboolean
read_xsession(XSession *xs)
{
	GKeyFile *entry;
	gchar *exec, *tryexec, *binary;
	GError *err = NULL;
	gboolean truth;
	const gchar *g = G_KEY_FILE_DESKTOP_GROUP;

	if ((entry = xs->entry))
		return TRUE;
	if (!(entry = g_key_file_new())) {
		EPRINTF("%s: could not allocate key file\n", xs->file);
		return FALSE;
	}
	if (!g_key_file_load_from_file(entry, xs->file, G_KEY_FILE_NONE, NULL)) {
		EPRINTF("%s: could not load keyfile\n", xs->file);
		g_key_file_unref(entry);
		return FALSE;
	}
	if (!g_key_file_has_group(entry, g)) {
		EPRINTF("%s: has no [Desktop Entry] section\n", xs->file);
		goto free_fail;
	}
	if (!g_key_file_has_key(entry, g, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
		EPRINTF("%s: has no %s= key\n", xs->file, G_KEY_FILE_DESKTOP_KEY_TYPE);
		goto free_fail;
	}
	if (!g_key_file_has_key(entry, g, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		EPRINTF("%s: has no %s= key\n", xs->file, G_KEY_FILE_DESKTOP_KEY_NAME);
		goto free_fail;
	}
	truth = g_key_file_get_boolean(entry, g, G_KEY_FILE_DESKTOP_KEY_HIDDEN, &err);
	if (err)
		err = NULL;
	else if (truth) {
		DPRINTF("%s: is Hidden\n", xs->file);
		goto free_fail;
	}
#if 0
	/* NoDisplay is often used to hide XSession desktop entries form the
	   application menu and does not indicate that it should not be
	   displayed as an XSession entry. */
	truth = g_key_file_get_boolean(entry, g, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, &err);
	if (err)
		err = NULL;
	else if (truth) {
		DPRINTF("%s: is NoDisplay\n", xs->file);
		goto free_fail;
	}
#endif
	exec = g_key_file_get_string(entry, g, G_KEY_FILE_DESKTOP_KEY_EXEC, &err);
	if (err || !exec) {
		DPRINTF("%s: has no %s= key\n", xs->file, G_KEY_FILE_DESKTOP_KEY_EXEC);
		goto free_fail;
	}
	tryexec = g_key_file_get_string(entry, g, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, &err);
	if (err || !tryexec) {
		char *p;

		/* parse the first word of the exec statement and see whether
		   it is executable or can be found in PATH */
		binary = g_strdup(exec);
		if ((p = strpbrk(binary, " \t")))
			*p = '\0';
		err = NULL;
	} else {
		binary = g_strdup(tryexec);
		g_free(tryexec);
		tryexec = NULL;
	}
	g_free(exec);
	exec = NULL;
	if (binary[0] == '/') {
		if (access(binary, X_OK)) {
			DPRINTF("%s: %s: %s\n", xs->file, binary, strerror(errno));
			goto binary_free_fail;
		}
	} else {
		char *file, *dir, *end, *path;
		gboolean execok = FALSE;

		path = strdup(getenv("PATH") ? : "");
		file = calloc(PATH_MAX + 1, sizeof(*file));

		for (dir = path, end = dir + strlen(dir); dir < end;
		     *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;
		for (dir = path; dir < end; dir += strlen(dir) + 1) {
			if (!*dir)
				continue;
			strcpy(file, dir);
			strcat(file, "/");
			strcat(file, binary);
			if (!access(file, X_OK)) {
				execok = TRUE;
				break;
			}
		}
		free(file);
		free(path);
		if (!execok) {
			DPRINTF("%s: %s: not executable\n", xs->file, binary);
			goto binary_free_fail;
		}
	}
	DPRINTF("got xsession file %s (%s)\n", xs->appid, xs->file);
	xs->entry = entry;
	return TRUE;
      binary_free_fail:
	g_free(binary);
      free_fail:
	g_key_file_free(entry);
	return FALSE;
}

static gboolean
on_expose_event(GtkWidget *widget, GdkEvent *event, gpointer data)
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

static gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	free(options.current);
	options.current = strdup("logout");
	options.managed = False;
	gtk_main_quit();
	return TRUE;		/* propagate */
}

static void
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

/** @brief transform window into pointer-grabbed window
  * @param window - window to transform
  *
  * Trasform a window into a window that has a grab on the pointer on the window
  * and restricts pointer movement to the window boundary.
  */
static void
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

GKeyFile *
make_login_choice(int argc, char *argv[])
{
        GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        gtk_window_set_wmclass(GTK_WINDOW(w), "xde-session", "XDE-Session");
        gtk_window_set_role(GTK_WINDOW(w), "choose");
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
	// gtk_window_set_default_size(GTK_WINDOW(w), -1, 400);

	/* most of this is just in case override-redirect fails */
	gtk_window_set_focus_on_map(GTK_WINDOW(w), TRUE);
	gtk_window_set_accept_focus(GTK_WINDOW(w), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(w), TRUE);
	gtk_window_set_modal(GTK_WINDOW(w), TRUE);
	gtk_window_stick(GTK_WINDOW(w));
	gtk_window_deiconify(GTK_WINDOW(w));
	gtk_widget_show_all(GTK_WIDGET(w));
	gtk_widget_grab_focus(GTK_WIDGET(buttons[3]));

	gtk_widget_realize(GTK_WIDGET(w));
	GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(w));
	gdk_window_set_override_redirect(win, TRUE);
	grabbed_window(GTK_WINDOW(w), NULL);

	/* TODO: we should really set a timeout and if no user interaction has
	   occured before the timeout, we should continue if we have a viable
	   default or choice. */
	gtk_main();
	gtk_widget_destroy(GTK_WIDGET(w));

	GKeyFile *entry = NULL;

	if (strcmp(options.current, "logout")) {
		if (!(entry = (typeof(entry)) g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s?\n", options.current);
			exit(EXIT_FAILURE);
		}
	}
	return (entry);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
