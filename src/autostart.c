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
#include "autostart.h"

GHashTable *autostarts;

static void
autostart_key_free(gpointer data)
{
	free(data);
}

static void
autostart_value_free(gpointer data)
{
	AutoStart *value = (typeof(value)) data;

	if (value->appid)
		free(value->appid);
	if (value->file)
		free(value->file);
	if (value->entry)
		g_key_file_free(value->entry);
	free(value);
}

GHashTable *
get_autostarts(void)
{
	char *dirs, *pos, *end, *path, *suffix;

	autostarts = g_hash_table_new_full(g_str_hash, g_str_equal,
					   autostart_key_free, autostart_value_free);

	dirs = strdup(xdg_dirs.conf.both);
	end = dirs + strlen(dirs);

	path = calloc(PATH_MAX + 1, sizeof(*path));

	for (pos = dirs; pos < end; *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	for (pos = dirs; pos < end; pos += strlen(pos) + 1) {
		DIR *dir;
		struct dirent *d;
		char *key, *p;
		struct stat st;
		AutoStart *value;

		if (!*pos)
			continue;
		strncpy(path, pos, PATH_MAX);
		strncat(path, "/autostart", PATH_MAX);
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
			g_hash_table_replace(autostarts, key, value);
		}
		closedir(dir);
	}
	free(path);
	free(dirs);
	return (autostarts);
}

gboolean
read_autostart(AutoStart *as)
{
	GKeyFile *entry;
	gchar *exec, *tryexec, *binary;
	GError *err = NULL;
	gboolean truth;
	const gchar *g = G_KEY_FILE_DESKTOP_GROUP;

	if ((entry = as->entry))
		return TRUE;
	if (!(entry = g_key_file_new())) {
		EPRINTF("%s: could not allocate key file\n", as->file);
		return FALSE;
	}
	if (!g_key_file_load_from_file(entry, as->file, G_KEY_FILE_NONE, NULL)) {
		EPRINTF("%s: could not load keyfile\n", as->file);
		g_key_file_unref(entry);
		return FALSE;
	}
	if (!g_key_file_has_group(entry, g)) {
		EPRINTF("%s: has no [Desktop Entry] section\n", as->file);
		goto free_fail;
	}
	if (!g_key_file_has_key(entry, g, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
		EPRINTF("%s: has no %s= key\n", as->file, G_KEY_FILE_DESKTOP_KEY_TYPE);
		goto free_fail;
	}
	if (!g_key_file_has_key(entry, g, G_KEY_FILE_DESKTOP_KEY_NAME, NULL)) {
		EPRINTF("%s: has no %s= key\n", as->file, G_KEY_FILE_DESKTOP_KEY_NAME);
		goto free_fail;
	}
	truth = g_key_file_get_boolean(entry, g, G_KEY_FILE_DESKTOP_KEY_HIDDEN, &err);
	if (err)
		err = NULL;
	else if (truth) {
		DPRINTF("%s: is Hidden\n", as->file);
		goto free_fail;
	}
#if 0
	/* NoDisplay is often used to hide AutoStart desktop entries form the
	   application menu and does not indicate that it should not be
	   displayed as an AutoStart entry. */
	truth = g_key_file_get_boolean(entry, g, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, &err);
	if (err)
		err = NULL;
	else if (truth) {
		DPRINTF("%s: is NoDisplay\n", as->file);
		goto free_fail;
	}
#endif
	exec = g_key_file_get_string(entry, g, G_KEY_FILE_DESKTOP_KEY_EXEC, &err);
	if (err || !exec) {
		DPRINTF("%s: has no %s= key\n", as->file, G_KEY_FILE_DESKTOP_KEY_EXEC);
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
			DPRINTF("%s: %s: %s\n", as->file, binary, strerror(errno));
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
			DPRINTF("%s: %s: not executable\n", as->file, binary);
			goto binary_free_fail;
		}
	}
	DPRINTF("got an autostart file %s (%s)\n", as->appid, as->file);
	as->entry = entry;
	return TRUE;
      binary_free_fail:
	g_free(binary);
      free_fail:
	g_key_file_free(entry);
	return FALSE;
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
