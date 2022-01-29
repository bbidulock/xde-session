/*****************************************************************************

 Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2002-2009  OpenSS7 Corporation <http://www.openss7.com/>
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
#include "autostart.h"
#include "display.h"

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

Options options = {
	.output = 1,
	.debug = 0,
	.command = COMMAND_DEFAULT,
	.dryrun = False,
	.display = NULL,
	.rcfile = NULL,
	.desktop = NULL,
	.profile = NULL,
	.startwm = NULL,
	.file = NULL,
	.setup = NULL,
	.exec = NULL,
	.exec_num = 0,
	.autostart = True,
	.wait = True,
	.pause = 2000,
	.toolwait = True,
	.guard = 200,
	.delay = 0,
	.charset = NULL,
	.language = NULL,
	.vendor = NULL,
	.splash = True,
	.image = NULL,
	.banner = NULL,
	.side = {
		 .splash = LOGO_SIDE_TOP,
		 .chooser = LOGO_SIDE_LEFT,
		 .logout = LOGO_SIDE_LEFT,
		 }
	,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.usexde = False,
	.xinit = False,
	.noask = False,
	.managed = False,
	.choice = NULL,
        .current = NULL,
	.manage = {
		   .session = True,
		   .proxy = True,
		   .dockapps = True,
		   .systray = True,
		   }
	,
	.slock = {
		  .lock = True,
		  .program = NULL,
		  }
	,
	.assist = True,
	.xsettings = True,
	.xinput = True,
	.style = {
		  .theme = True,
		  .dockapps = True,
		  .systray = True,
		  }
	,
};

Optargs optargs = { NULL, };

Optargs defaults = {
	.output = "1",
	.debug = "0",
	.dryrun = "false",
	.display = "$DISPLAY",
	.rcfile = "",
	.desktop = "XDE",
	.profile = "default",
	.startwm = "",
	.file = "",
	.setup = "",
	.exec = "",
	.autostart = "true",
	.wait = "true",
	.pause = "2000",
	.toolwait = "false",
	.guard = "200",
	.delay = "0",
	.charset = "LC_CHARSET",
	.language = "LC_ALL",
	.vendor = "xde",
	.splash = "true",
	.image = "",
	.banner = "",
	.side = {
		 .splash = "top",
		 .chooser = "left",
		 .logout = "left",
		 }
	,
	.icon_theme = "",
	.gtk2_theme = "",
	.curs_theme = "",
	.usexde = "false",
	.xinit = "false",
	.noask = "false",
	.managed = "flase",
	.choice = "choose",
        .current = "",
	.prompt = {
		   .chooser = {
			       .text = "",
			       .markup = "",
			       }
		   ,
		   .logout = {
			      .text = "Log out of %p session?",
			      .markup = "Log out of <b>%p</b> session?",
			      }
		   ,
		   }
	,
	.manage = {
		   .session = "true",
		   .proxy = "true",
		   .dockapps = "true",
		   .systray = "true",
		   }
	,
	.slock = {
		  .lock = "true",
		  .program = "",
		  }
	,
	.assist = "true",
	.xsettings = "true",
	.xinput = "true",
	.style = {
		  .theme = "true",
		  .dockapps = "true",
		  .systray = "true",
		  }
	,
};

XdgDirectories xdg_dirs;

XdgScreen *screens;

Display *dpy = NULL;
int nscr;
XdgScreen *scr;

void
get_directories(void)
{
	char *home;
	char *xhome, *xdirs, *xboth;
	int len;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_CONFIG_HOME");
	if (xhome)
		xhome = strdup(xhome);
	else {
		len = strlen(home) + strlen("/.config") + 1;
		xhome = calloc(len, sizeof(*xhome));
		strcpy(xhome, home);
		strcat(xhome, "/.config");
	}
	xdirs = strdup(getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg");
	len = strlen(xhome) + 1 + strlen(xdirs) + 1;
	xboth = calloc(len, sizeof(*xboth));
	strcpy(xboth, xhome);
	strcat(xboth, ":");
	strcat(xboth, xdirs);

	free(xdg_dirs.conf.home);
	xdg_dirs.conf.home = xhome;
	free(xdg_dirs.conf.dirs);
	xdg_dirs.conf.dirs = xdirs;
	free(xdg_dirs.conf.both);
	xdg_dirs.conf.both = xboth;

	xhome = getenv("XDG_DATA_HOME");
	if (xhome)
		xhome = strdup(xhome);
	else {
		len = strlen(home) + strlen("/.local/share") + 1;
		xhome = calloc(len, sizeof(*xhome));
		strcpy(xhome, home);
		strcat(xhome, "/.local/share");
	}
	xdirs = strdup(getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share");
	len = strlen(xhome) + 1 + strlen(xdirs) + 1;
	xboth = calloc(len, sizeof(*xboth));
	strcpy(xboth, xhome);
	strcat(xboth, ":");
	strcat(xboth, xdirs);

	free(xdg_dirs.data.home);
	xdg_dirs.data.home = xhome;
	free(xdg_dirs.data.dirs);
	xdg_dirs.data.dirs = xdirs;
	free(xdg_dirs.data.both);
	xdg_dirs.data.both = xboth;
}

/** @brief set configuration file defaults
  * @param config - key file in which to set defaults
  */
void
set_config_defaults(GKeyFile *config)
{
	const char *key;
	const char *group = "XDE Session";

	key = "DefaultXSession";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, defaults.choice);
	key = "Desktop";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, defaults.desktop);
	key = "StartupFile";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "Wait";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, FALSE);
	key = "Pause";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_uint64(config, group, key, 2);
	key = "ToolWait";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, FALSE);
	key = "ToolWaitGuard";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_uint64(config, group, key, 200);
	key = "ToolWaitDelay";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_uint64(config, group, key, 0);
	key = "Language";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "Vendor";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "Splash";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "SplashImage";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "ChooserPromptText";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "");
	key = "ChooserPromptMarkup";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "");
	key = "LogoutPromptText";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "Log out of %p session?");
	key = "LogoutPromptMarkup";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "Log out of <b>%p</b> session?");
	key = "ChooserSide";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "left");
	key = "SplashSide";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "top");
	key = "LogoutSide";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_string(config, group, key, "left");
	key = "IconTheme";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "Theme";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "CursorTheme";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "UseXDETheme";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, FALSE);
	key = "SessionManage";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "SessionManageProxy";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "SessionManageDockApps";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "SessionManageSysTray";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "ScreenLock";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "ScreenLockProgram";
	if (!g_key_file_has_key(config, group, key, NULL)) { }
	key = "AutoStart";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "StartupNotificationAssist";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "XSettings";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "XInput";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "ThemeManage";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "ThemeManageDockApps";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
	key = "ThemeManageSysTray";
	if (!g_key_file_has_key(config, group, key, NULL))
		g_key_file_set_boolean(config, group, key, TRUE);
}

/** @brief find and read the configuration file with defaults set
  */
GKeyFile *
find_read_config(void)
{
	GKeyFile *config;
	char *rcfile = NULL;
	guint flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	if (!(config = g_key_file_new())) {
		EPRINTF("could not alllocate key file\n");
		exit(EXIT_FAILURE);
		return (config);
	}
	if (optargs.rcfile) {
		if (access(options.rcfile, R_OK))
			EPRINTF("%s: %s\n", options.rcfile, strerror(errno));
		else {
			if (g_key_file_load_from_file(config, options.rcfile, flags, NULL))
				rcfile = strdup(options.rcfile);
			else
				EPRINTF("%s: could not load key file\n", options.rcfile);
		}
	}
	if (!rcfile) {
		char *dirs, *file, *dir, *end;
		int len;

		dirs = strdup(xdg_dirs.conf.both);
		/* go through them forward */
		for (dir = dirs, end = dirs + strlen(dirs); dir < end;
		     *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) {
			if (!*dir)
				continue;
			len = strlen(dir) + strlen("xde/session.ini") + 1;
			file = calloc(len, sizeof(*file));
			strcpy(file, dir);
			strcat(file, "xde/session.ini");
			if (!access(file, R_OK)) {
				if (g_key_file_load_from_file(config, file, flags, NULL)) {
					rcfile = file;
					break;
				}
				DPRINTF("%s: could not load key file\n", file);
			}
			free(file);
		}
		free(dirs);
	}
	if (rcfile)
		g_key_file_set_string(config, "XDE Session", "FileName", rcfile);
	free(rcfile);
	// set_config_defaults(config);
	return (config);
}

void
set_default_session(void)
{
	char *dirs, *dir, *end, *file, *line, *p;
	static const char *session = "/xde/default";
	static const char *current = "/xde/current";

	free(options.session);
	options.session = NULL;
	free(options.current);
	options.current = NULL;

	dirs = strdup(xdg_dirs.conf.both);
	end = dirs + strlen(dirs);

	file = calloc(PATH_MAX + 1, sizeof(*file));
	line = calloc(BUFSIZ + 1, sizeof(*line));

	/* go through them forward */
	for (dir = dirs; dir < end; *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;
	for (dir = dirs; dir < end; dir += strlen(dir) + 1) {
		FILE *f;

		if (!*dir)
			continue;
		if (!options.session) {
			strncpy(file, dir, PATH_MAX);
			strncat(file, session, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						options.session = strdup(line);
					}
					fclose(f);
				}
			}

		}
		if (!options.current) {
			strncpy(file, dir, PATH_MAX);
			strncat(file, current, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						options.current = strdup(line);
					}
					fclose(f);
				}
			}
		}
	}
	free(line);
	free(file);
	free(dirs);
}

void
set_default_splash(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char *dirs, *dir, *end, *pfx, *suffix, *file;
	size_t i;

	dirs = strdup(xdg_dirs.data.both);
	end = dirs + strlen(dirs);
	for (dir = dirs; dir < end; *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;

	free(options.image);
	options.image = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	for (pfx = getenv("XDG_MENU_PREFIX") ? : ""; pfx; pfx = *pfx ? "" : NULL) {
		for (dir = dirs; dir < end; dir += strlen(dir) + 1) {
			if (!*dir)
				continue;
			strncpy(file, dir, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "splash", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
				strcpy(suffix, exts[i]);
				if (!access(file, R_OK)) {
					options.image = strdup(file);
					break;
				}
				DPRINTF("%s: %s\n", file, strerror(errno));
			}
			if (options.image)
				break;
		}
		if (options.image)
			break;
	}
	free(file);
	free(dirs);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char *dirs, *dir, *end, *pfx, *suffix, *file;
	size_t i;

	dirs = strdup(xdg_dirs.data.both);
	end = dirs + strlen(dirs);
	for (dir = dirs; dir < end; *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;

	free(options.banner);
	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	for (pfx = getenv("XDG_MENU_PREFIX") ? : ""; pfx; pfx = *pfx ? "" : NULL) {
		for (dir = dirs; dir < end; dir += strlen(dir) + 1) {
			if (!*dir)
				continue;
			strncpy(file, dir, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "banner", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
				strcpy(suffix, exts[i]);
				if (!access(file, R_OK)) {
					options.banner = strdup(file);
					break;
				}
				DPRINTF("%s: %s\n", file, strerror(errno));
			}
			if (options.banner)
				break;
		}
		if (options.banner)
			break;
	}
	free(file);
	free(dirs);
}

void
set_default_rcfile(void)
{
	char *dirs, *dir, *end, *file;

	dirs = strdup(xdg_dirs.conf.both);
	end = dirs + strlen(dirs);
	for (dir = dirs; dir < end; *strchrnul(dir, ':') = '\0', dir += strlen(dir) + 1) ;

	free(options.rcfile);
	options.rcfile = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	for (dir = dirs; dir < end; dir += strlen(dir) + 1) {
		if (!*dir)
			continue;
		strncpy(file, dir, PATH_MAX);
		strncat(file, "/xde/session.ini", PATH_MAX);
		if (!access(file, R_OK)) {
			options.rcfile = strdup(file);
			break;
		}
		DPRINTF("%s: %s\n", file, strerror(errno));
	}
	free(file);
	free(dirs);
}

void
set_defaults(void)
{
	const char *env;
	int len;

	get_directories();

	defaults.display = getenv("DISPLAY") ? : "";
	options.display = strdup(defaults.display);
	options.desktop = strdup("XDE");

	if ((env = getenv("XDG_VENDOR_ID")) && *env) {
		options.vendor = strdup(env);
	} else if ((env = getenv("XDG_MENU_PREFIX")) && *env) {
		char *vendor = strdup(env);

		if (vendor[strlen(vendor) - 1] == '-')
			vendor[strlen(vendor) - 1] = '\0';
		options.vendor = vendor;
	}
	if (options.vendor) {
		char *prefix;

		len = strlen(options.vendor) + 2;
		prefix = calloc(len, sizeof(*prefix));
		strcpy(prefix, options.vendor);
		strcat(prefix, "-");
		setenv("XDG_MENU_PREFIX", prefix, 0);
		free(prefix);
		setenv("XDG_VENDOR_ID", options.vendor, 0);
	}
	set_default_rcfile();
	set_default_banner();
	set_default_splash();
        set_default_session();
	if (options.banner && !options.image)
		options.image = strdup(options.banner);

	if ((env = setlocale(LC_ALL, "")))
		options.language = strdup(env);
	options.charset = strdup(nl_langinfo(CODESET));

}

void
get_defaults(void)
{
	GKeyFile *c = find_read_config();
	const char *g = "XDE Session";
	gchar *s;
	gboolean b;
	guint64 u;
	GError *e = NULL;

	if (!optargs.desktop) {
		s = g_key_file_get_string(c, g, "Desktop", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.desktop = optargs.desktop = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.profile) {
		s = g_key_file_get_string(c, g, "Profile", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.profile = optargs.profile = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.file) {
		s = g_key_file_get_string(c, g, "StartupFile", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.file = optargs.file = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.wait) {
		b = g_key_file_get_boolean(c, g, "Wait", &e);
		if (e)
			e = NULL;
		else {
			options.wait = b ? True : False;
			optargs.wait = b ? "true" : "false";
		}
	}
	if (!optargs.pause) {
		u = g_key_file_get_uint64(c, g, "Pause", &e);
		if (e)
			e = NULL;
		else {
			static char static_string[16];

			options.pause = u;
			sprintf(static_string, "%d", (int) u);
			optargs.pause = static_string;
		}
	}
	if (!optargs.toolwait) {
		b = g_key_file_get_boolean(c, g, "ToolWait", &e);
		if (e)
			e = NULL;
		else {
			options.toolwait = b ? True : False;
			optargs.toolwait = b ? "true" : "false";
		}
	}
	if (!optargs.guard) {
		u = g_key_file_get_uint64(c, g, "ToolWaitGuard", &e);
		if (e)
			e = NULL;
		else {
			static char static_string[16];

			options.guard = u;
			sprintf(static_string, "%d", (int) u);
			optargs.guard = static_string;
		}
	}
	if (!optargs.delay) {
		u = g_key_file_get_uint64(c, g, "ToolWaitDelay", &e);
		if (e)
			e = NULL;
		else {
			static char static_string[16];

			options.delay = u;
			sprintf(static_string, "%d", (int) u);
			optargs.delay = static_string;
		}
	}
	if (!optargs.language) {
		s = g_key_file_get_string(c, g, "Language", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.language = optargs.language = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.vendor) {
		s = g_key_file_get_string(c, g, "Vendor", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.vendor = optargs.vendor = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.splash) {
		b = g_key_file_get_boolean(c, g, "Splash", &e);
		if (e)
			e = NULL;
		else {
			options.splash = b ? True : False;
			optargs.splash = b ? "true" : "false";
		}
	}
	if (!optargs.image) {
		s = g_key_file_get_string(c, g, "SplashImage", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.image = optargs.image = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.banner) {
		s = g_key_file_get_string(c, g, "Banner", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.banner = optargs.banner = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.prompt.chooser.text) {
		s = g_key_file_get_locale_string(c, g, "ChooserPromptText", options.language, &e);
		if (e)
			e = NULL;
		else if (s) {
			options.prompt.chooser.text = optargs.prompt.chooser.text = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.prompt.chooser.markup) {
		s = g_key_file_get_locale_string(c, g, "ChooserPromptMarkup", options.language, &e);
		if (e)
			e = NULL;
		else if (s) {
			options.prompt.chooser.markup = optargs.prompt.chooser.markup = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.prompt.logout.text) {
		s = g_key_file_get_locale_string(c, g, "LogoutPromptText", options.language, &e);
		if (e)
			e = NULL;
		else if (s) {
			options.prompt.logout.text = optargs.prompt.logout.text = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.prompt.logout.markup) {
		s = g_key_file_get_locale_string(c, g, "LogoutPromptMarkup", options.language, &e);
		if (e)
			e = NULL;
		else if (s) {
			options.prompt.logout.markup = optargs.prompt.logout.markup = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.side.splash) {
		s = g_key_file_get_string(c, g, "SplashSide", &e);
		if (e)
			e = NULL;
		else if (s) {
			if (strcmp(s, "left")) {
				optargs.side.splash = "left";
				options.side.splash = LOGO_SIDE_LEFT;
			} else if (strcmp(s, "right")) {
				optargs.side.splash = "right";
				options.side.splash = LOGO_SIDE_RIGHT;
			} else if (strcmp(s, "top")) {
				optargs.side.splash = "top";
				options.side.splash = LOGO_SIDE_TOP;
			} else if (strcmp(s, "bottom")) {
				optargs.side.splash = "bottom";
				options.side.splash = LOGO_SIDE_BOTTOM;
			}
			g_free(s);
		}
	}
	if (!optargs.side.chooser) {
		s = g_key_file_get_string(c, g, "ChooserSide", &e);
		if (e)
			e = NULL;
		else if (s) {
			if (strcmp(s, "left")) {
				optargs.side.chooser = "left";
				options.side.chooser = LOGO_SIDE_LEFT;
			} else if (strcmp(s, "right")) {
				optargs.side.chooser = "right";
				options.side.chooser = LOGO_SIDE_RIGHT;
			} else if (strcmp(s, "top")) {
				optargs.side.chooser = "top";
				options.side.chooser = LOGO_SIDE_TOP;
			} else if (strcmp(s, "bottom")) {
				optargs.side.chooser = "bottom";
				options.side.chooser = LOGO_SIDE_BOTTOM;
			}
			g_free(s);
		}
	}
	if (!optargs.side.logout) {
		s = g_key_file_get_string(c, g, "LogoutSide", &e);
		if (e)
			e = NULL;
		else if (s) {
			if (strcmp(s, "left")) {
				optargs.side.logout = "left";
				options.side.logout = LOGO_SIDE_LEFT;
			} else if (strcmp(s, "right")) {
				optargs.side.logout = "right";
				options.side.logout = LOGO_SIDE_RIGHT;
			} else if (strcmp(s, "top")) {
				optargs.side.logout = "top";
				options.side.logout = LOGO_SIDE_TOP;
			} else if (strcmp(s, "bottom")) {
				optargs.side.logout = "bottom";
				options.side.logout = LOGO_SIDE_BOTTOM;
			}
			g_free(s);
		}
	}
	if (!optargs.icon_theme) {
		s = g_key_file_get_string(c, g, "IconTheme", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.icon_theme = optargs.icon_theme = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.gtk2_theme) {
		s = g_key_file_get_string(c, g, "Theme", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.gtk2_theme = optargs.gtk2_theme = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.curs_theme) {
		s = g_key_file_get_string(c, g, "CursorTheme", &e);
		if (e)
			e = NULL;
		else if (s) {
			options.curs_theme = optargs.curs_theme = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.usexde) {
		b = g_key_file_get_boolean(c, g, "UseXDETheme", &e);
		if (e)
			e = NULL;
		else {
			options.usexde = b ? True : False;
			optargs.usexde = b ? "true" : "false";
		}
	}
	if (!optargs.manage.session) {
		b = g_key_file_get_boolean(c, g, "SessionManage", &e);
		if (e)
			e = NULL;
		else {
			options.manage.session = b ? True : False;
			optargs.manage.session = b ? "true" : "false";
		}
	}
	if (!optargs.manage.proxy) {
		b = g_key_file_get_boolean(c, g, "SessionManageProxy", &e);
		if (e)
			e = NULL;
		else {
			options.manage.proxy = b ? True : False;
			optargs.manage.proxy = b ? "true" : "false";
		}
	}
	if (!optargs.manage.dockapps) {
		b = g_key_file_get_boolean(c, g, "SessionManageDockApps", &e);
		if (e)
			e = NULL;
		else {
			options.manage.dockapps = b ? True : False;
			optargs.manage.dockapps = b ? "true" : "false";
		}
	}
	if (!optargs.manage.systray) {
		b = g_key_file_get_boolean(c, g, "SessionManageSysTray", &e);
		if (e)
			e = NULL;
		else {
			options.manage.systray = b ? True : False;
			optargs.manage.systray = b ? "true" : "false";
		}
	}
	if (!optargs.slock.lock) {
		b = g_key_file_get_boolean(c, g, "ScreenLock", &e);
		if (e)
			e = NULL;
		else {
			options.slock.lock = b ? True : False;
			optargs.slock.lock = b ? "true" : "false";
		}
	}
	if (!optargs.slock.program) {
		s = g_key_file_get_string(c, g, "ScreenLockProgram", &e);
		if (e)
			e = NULL;
		else {
			options.slock.program = optargs.slock.program = strdup(s);
			g_free(s);
		}
	}
	if (!optargs.autostart) {
		b = g_key_file_get_boolean(c, g, "AutoStart", &e);
		if (e)
			e = NULL;
		else {
			options.autostart = b ? True : False;
			optargs.autostart = b ? "true" : "false";
		}
	}
	if (!optargs.assist) {
		b = g_key_file_get_boolean(c, g, "StartupNotificationAssist", &e);
		if (e)
			e = NULL;
		else {
			options.assist = b ? True : False;
			optargs.assist = b ? "true" : "false";
		}
	}
	if (!optargs.xsettings) {
		b = g_key_file_get_boolean(c, g, "XSettings", &e);
		if (e)
			e = NULL;
		else {
			options.xsettings = b ? True : False;
			optargs.xsettings = b ? "true" : "false";
		}
	}
	if (!optargs.xinput) {
		b = g_key_file_get_boolean(c, g, "XInput", &e);
		if (e)
			e = NULL;
		else {
			options.xinput = b ? True : False;
			optargs.xinput = b ? "true" : "false";
		}
	}
	if (!optargs.style.theme) {
		b = g_key_file_get_boolean(c, g, "ThemeManage", &e);
		if (e)
			e = NULL;
		else {
			options.style.theme = b ? True : False;
			optargs.style.theme = b ? "true" : "false";
		}
	}
	if (!optargs.style.dockapps) {
		b = g_key_file_get_boolean(c, g, "ThemeManageDockApps", &e);
		if (e)
			e = NULL;
		else {
			options.style.dockapps = b ? True : False;
			optargs.style.dockapps = b ? "true" : "false";
		}
	}
	if (!optargs.style.systray) {
		b = g_key_file_get_boolean(c, g, "ThemeManageSysTray", &e);
		if (e)
			e = NULL;
		else {
			options.style.systray = b ? True : False;
			optargs.style.systray = b ? "true" : "false";
		}
	}
}

void
run_launch(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

void
run_choose(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

void
run_logout(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

void
run_manage(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
copying(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2010-2022  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2002-2009  OpenSS7 Corporation <http://www.openss7.com/>\n\
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
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2020, 2021, 2022  Monavacon Limited.\n\
Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009  OpenSS7 Corporation.\n\
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
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [OPTIONS] [XSESSION|SPECIAL]\n\
    %1$s [OPTIONS] {-h|--help}\n\
    %1$s [OPTIONS] {-V|--version}\n\
    %1$s [OPTIONS] {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	(void) argc;
	if (!options.output && !options.debug)
		return;
        /* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [OPTIONS] [XSESSION|SPECIAL]\n\
    %1$s [OPTIONS] {-h|--help}\n\
    %1$s [OPTIONS] {-V|--version}\n\
    %1$s [OPTIONS] {-C|--copying}\n\
Arguments:\n\
    XSESSION | SPECIAL                  (%2$s)\n\
        The name of the XDG session to execute or one of:\n\
        \"default\"                       (%29$s)\n\
            run default session without prompting\n\
        \"current\"                       (%30$s)\n\
            run current session without prompting\n\
        \"choose\"\n\
            post a desktop chooser dialog\n\
        \"logout\"\n\
            post a desktop logout dialog\n\
        \"manage\"\n\
            post a session manager dialog\n\
Command Options:\n\
   [--launch]\n\
        launch the session specified by XSESSION\n\
    --choose\n\
        launch a desktop chooser dialog\n\
    --logout\n\
        launch a desktop logout dialog\n\
    --manage\n\
        launch a desktop manager dialog\n\
    -h, --help\n\
        print this usage information and exit\n\
    -V, --version\n\
        print program version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General Options:\n\
    -0, --xinit                         (%3$s)\n\
        perform additional .xinitrc initializations\n\
    -N, --noask                         (%31$s)\n\
        do not ask for a choice but proceed with default\n\
    -d, --display DISPLAY               (%4$s)\n\
        specify the X display to use\n\
    -r, --rcfile CONFIG                 (%28$s)\n\
        specify a custom configuration file\n\
    -e, --desktop DESKTOP               (%5$s)\n\
        specify the desktop environment\n\
    -s, --session PROFILE               (%6$s)\n\
        specify the lxsesion(1) compatible profile\n\
    -m, --startwm COMMAND               (%7$s)\n\
        specify command to start the window manager\n\
    -f, --file FILE                     (%8$s)\n\
        specify file from which to read commands\n\
    -I, --setup COMMAND                 (%9$s)\n\
        specify setup command to run before window manager\n\
    -x, --exec COMMAND                  (%10$s)\n\
        specify commands to execute before autostart\n\
        this option may be repeated\n\
    -a, --noautostart | --autostart     (%11$s)\n\
        (do not) perform XDG autostart of tasks\n\
    -w, --wait | --nowait               (%12$s)\n\
        (do not) wait for window manager to start\n\
    -p, --pause [PAUSE]                 (%13$s)\n\
        pause for PAUSE seconds before running\n\
    -W, --toolwait [GUARD][:DELAY]      (%14$s)\n\
        perform xtoolwait on autostart tasks\n\
          GUARD is the longest time to wait for tool start\n\
        DELAY is the time to wait for unknown startup\n\
    -c, --charset CHARSET               (%15$s)\n\
        specify the character set\n\
    -L, --language LANGUAGE             (%16$s)\n\
        specify the language for XDG translations\n\
    -O, --vendor VENDOR                 (%17$s)\n\
        specify the vendor id for branding\n\
    -l, --splash [IMAGE] | --nosplash   (%18$s)\n\
        display (or not) startup splash page\n\
    -b, --banner BANNER                 (%19$s)\n\
        specify custom session branding\n\
    -S, --side {top|left|right|bottom}  (%20$s)\n\
        specify side of splash or dialog for logo placement\n\
    -i, --icons THEME                   (%21$s)\n\
        set the icon theme to use\n\
    -t, --theme THEME                   (%22$s)\n\
        set the GTK+ theme to use\n\
    -z, --cursors THEME                 (%23$s)\n\
        set the cursor theme to use\n\
    -X, --xde-theme                     (%24$s)\n\
        use the XDE desktop theme for the selection window\n\
    -n, --dry-run                       (%25$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]                 (%26$d)\n\
        increment or set debug LEVEL\n\
        this option may be repeated.\n\
    -v, --verbose [LEVEL]               (%27$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
"               , argv[0]
                , options.choice ? : (optargs.choice ? : defaults.choice)
                , optargs.xinit ? : defaults.xinit
                , optargs.display ? : defaults.display
                , optargs.desktop ? : defaults.desktop
                , optargs.profile ? : defaults.profile
                , optargs.startwm ? : defaults.startwm
                , optargs.file ? : defaults.file
                , optargs.setup ? : defaults.setup
                , optargs.exec ? : defaults.exec
                , optargs.autostart ? : defaults.autostart
                , optargs.wait ? : defaults.wait
                , optargs.pause ? : defaults.pause
                , optargs.toolwait ? : defaults.toolwait
                , options.charset ? : (optargs.charset ? : defaults.charset)
                , options.language ? : (optargs.language ? : defaults.language)
                , options.vendor ? : (optargs.vendor ? : defaults.vendor)
                , options.image ? : (optargs.image ? : defaults.image)
                , options.banner ? : (optargs.banner ? : defaults.banner)
                , optargs.side.splash ? : defaults.side.splash
                , optargs.icon_theme ? : defaults.icon_theme
                , optargs.gtk2_theme ? : defaults.gtk2_theme
                , optargs.curs_theme ? : defaults.curs_theme
                , optargs.usexde ? : defaults.usexde
                , optargs.dryrun ? : defaults.dryrun
                , options.debug
                , options.output
                , options.rcfile ? : (optargs.rcfile ? : defaults.rcfile)
                , options.session
                , options.current
                , optargs.noask ? : defaults.noask
        );
        /* *INDENT-ON* */
}

int
main(int argc, char *argv[])
{
	CommandType command = COMMAND_DEFAULT;

	setlocale(LC_ALL, "");

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"launch",	no_argument,		NULL, '1'},
			{"choose",	no_argument,		NULL, '2'},
			{"logout",	no_argument,		NULL, '3'},
			{"manage",	no_argument,		NULL, '4'},

			{"xinit",	no_argument,		NULL, '0'},
			{"noask",	no_argument,		NULL, 'N'},
			{"display",	required_argument,	NULL, 'd'},
                        {"rcfile",      required_argument,      NULL, 'r'},
			{"desktop",	required_argument,	NULL, 'e'},
			{"session",	required_argument,	NULL, 's'},
			{"startwm",	required_argument,	NULL, 'm'},
			{"file",	required_argument,	NULL, 'f'},
			{"setup",	required_argument,	NULL, 'I'},
			{"exec",	required_argument,	NULL, 'x'},
			{"autostart",	no_argument,		NULL, '6'},
			{"noautostart",	no_argument,		NULL, 'a'},
			{"autowait",	no_argument,		NULL, 'A'},
			{"noautowait",	no_argument,		NULL, '9'},
			{"wait",	no_argument,		NULL, 'w'},
			{"nowait",	no_argument,		NULL, '7'},
			{"pause",	optional_argument,	NULL, 'p'},
			{"toolwait",	optional_argument,	NULL, 'W'},
			{"notoolwait",	optional_argument,	NULL, '8'},
			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'L'},
			{"vendor",	required_argument,	NULL, 'O'},
			{"splash",	optional_argument,	NULL, 'l'},
			{"nosplash",	no_argument,		NULL, '!'},
			{"banner",	required_argument,	NULL, 'b'},
			{"side",	required_argument,	NULL, 'S'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"cursors",	required_argument,	NULL, 'z'},
			{"xde-theme",	no_argument,		NULL, 'X'},

			{"foreground",	no_argument,		NULL, 'F'},
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

		c = getopt_long_only(argc, argv,
				     "0d:r:e:s:m:f:I:x:awp::W::c:L:O:l::b:S:i:t:z:XnD::v::hVCH?",
				     long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "0d:r:e:s:m:f:I:x:awp:W:c:L:O:l:b:S:i:t:z:XnDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case '1':	/* --launch */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_LAUNCH;
			options.command = COMMAND_LAUNCH;
			defaults.choice = "choose";
			break;
		case '2':	/* --chooser */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_CHOOSE;
			options.command = COMMAND_CHOOSE;
			defaults.choice = "choose";
			break;
		case '3':	/* --logout */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_LOGOUT;
			options.command = COMMAND_LOGOUT;
			defaults.choice = "logout";
			break;
		case '4':	/* --manager */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_MANAGE;
			options.command = COMMAND_MANAGE;
			defaults.choice = "manage";
			break;

		case '0':	/* -0, --xinit */
			options.xinit = True;
			break;
		case 'N':	/* -N, --noask */
			options.noask = True;
			break;
		case 'd':	/* -d, --display DISPLAY */
			free(options.display);
			options.display = strdup(optarg);
			optargs.display = optarg;
			break;
		case 'r':	/* -r, --rcfile FILENAME */
			free(options.rcfile);
			options.rcfile = strdup(optarg);
			optargs.rcfile = optarg;
			break;
		case 'e':	/* -e, --desktop DESKTOP */
			free(options.desktop);
			options.desktop = strdup(optarg);
			optargs.desktop = optarg;
			break;
		case 's':	/* -s, --session SESSION */
			free(options.profile);
			options.profile = strdup(optarg);
			optargs.profile = optarg;
			break;
		case 'm':	/* -m, --startwm COMMAND */
			free(options.startwm);
			options.startwm = strdup(optarg);
			optargs.startwm = optarg;
			break;
		case 'f':	/* -f, --file FILE */
			free(options.file);
			options.file = strdup(optarg);
			optargs.file = optarg;
			break;
		case 'I':	/* -I, --setup COMMAND */
			free(options.setup);
			options.setup = strdup(optarg);
			optargs.setup = optarg;
			break;
		case 'x':	/* -x, --exec COMMAND */
			options.exec = realloc(options.exec, (options.exec_num + 1) *
					       sizeof(*options.exec));
			options.exec[options.exec_num + 1] = NULL;
			options.exec[options.exec_num++] = strdup(optarg);
			break;
		case '6':	/* --autostart */
			optargs.autostart = "true";
			options.autostart = True;
			break;
		case 'a':	/* -a, --noautostart */
			optargs.autostart = "false";
			options.autostart = False;
			break;
		case 'w':	/* -w, --wait */
			optargs.wait = "false";
			options.wait = True;
			break;
		case '7':	/* --nowait */
			optargs.wait = "false";
			options.wait = False;
			break;
		case 'p':	/* -p, --pause [PAUSE] */
			optargs.pause = optarg;
			if (optarg)
				options.pause = strtoul(optarg, NULL, 0);
			else
				options.pause = 2000;
			break;
		case 'W':	/* -W, --toolwait [GUARD][:DELAY] */
		case '8':	/* --notoolwait [GUARD][:DELAY] */
			options.toolwait = (c == 'W') ? True : False;
			optargs.toolwait = (c == 'W') ? "true" : "false";
			if (optarg) {
				char *g = NULL, *d;

				if ((d = strchr(optarg, ':'))) {
					if (d != optarg)
						g = optarg;
					*d = '\0';
					d++;
				}
				optargs.guard = g;
				optargs.delay = d;
				options.guard = g ? strtoul(g, NULL, 0) : 200;
				options.delay = d ? strtoul(d, NULL, 0) : 0;
			} else {
				optargs.guard = NULL;
				optargs.delay = NULL;
				options.guard = 200;
				options.delay = 0;
			}
			break;
		case 'c':	/* -c, --charset CHARSET */
			free(options.charset);
			options.charset = strdup(optarg);
			optargs.charset = optarg;
			break;
		case 'L':	/* -L, --language LANGUAGE */
			free(options.language);
			options.language = strdup(optarg);
			optargs.language = optarg;
			break;
		case 'O':	/* -O, --vendor VENDOR */
			free(options.vendor);
			options.vendor = strdup(optarg);
			optargs.vendor = optarg;
			break;
		case 'l':	/* -l, --splash [IMAGE] */
			options.splash = True;
			optargs.splash = "true";
			optargs.image = optarg;
			if (optarg) {
				free(options.image);
				options.image = strdup(optarg);
			}
			break;
		case '!':	/* --nosplash */
			options.splash = False;
			optargs.splash = "false";
			break;
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			optargs.banner = optarg;
			break;
		case 'S':	/* -S, --side {top|left|right|bottom} */
			optargs.side.splash = optarg;
			if (!strncasecmp(optarg, "left", strlen(optarg)))
				options.side.splash = LOGO_SIDE_LEFT;
			else if (!strncasecmp(optarg, "top", strlen(optarg)))
				options.side.splash = LOGO_SIDE_TOP;
			else if (!strncasecmp(optarg, "right", strlen(optarg)))
				options.side.splash = LOGO_SIDE_RIGHT;
			else if (!strncasecmp(optarg, "bottom", strlen(optarg)))
				options.side.splash = LOGO_SIDE_BOTTOM;
			break;
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			optargs.icon_theme = optarg;
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			optargs.gtk2_theme = optarg;
			break;
		case 'z':	/* -z, --cursors THEME */
			free(options.curs_theme);
			options.curs_theme = strdup(optarg);
			optargs.curs_theme = optarg;
			break;
		case 'X':	/* -X, --xde-theme */
			options.usexde = True;
			optargs.usexde = "true";
			break;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			optargs.dryrun = "true";
			break;
		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				optargs.debug = optarg;
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
			} else {
				optargs.output = optarg;
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.output = val;
			}
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = COMMAND_HELP;
			break;
		case 'V':	/* -V, --version */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_VERSION;
			options.command = COMMAND_VERSION;
			break;
		case 'C':	/* -C, --copying */
			if (options.command != COMMAND_DEFAULT)
				goto bad_option;
			if (command == COMMAND_DEFAULT)
				command = COMMAND_COPYING;
			options.command = COMMAND_COPYING;
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
		free(options.choice);
		options.choice = strdup(argv[optind]);
		if (strcasecmp(options.choice, "default")) {
			if (options.command != COMMAND_DEFAULT && options.command != COMMAND_LAUNCH)
				goto bad_nonopt;
			options.command = COMMAND_LAUNCH;
		} else if (strcasecmp(options.choice, "current")) {
			if (options.command != COMMAND_DEFAULT && options.command != COMMAND_LAUNCH)
				goto bad_nonopt;
			options.command = COMMAND_LAUNCH;
		} else if (strcasecmp(options.choice, "choose")) {
			if (options.command != COMMAND_DEFAULT && options.command != COMMAND_CHOOSE)
				goto bad_nonopt;
			options.command = COMMAND_CHOOSE;
		} else if (strcasecmp(options.choice, "logout")) {
			if (options.command != COMMAND_DEFAULT && options.command != COMMAND_LOGOUT)
				goto bad_nonopt;
			options.command = COMMAND_LOGOUT;
		} else if (strcasecmp(options.choice, "manage")) {
			if (options.command != COMMAND_DEFAULT && options.command != COMMAND_MANAGE)
				goto bad_nonopt;
			options.command = COMMAND_MANAGE;
		}
		if (++optind < argc) {
			fprintf(stderr, "%s: too many arguments\n", argv[0]);
			goto bad_nonopt;
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	get_defaults();
	switch (command) {
	case COMMAND_DEFAULT:
	case COMMAND_LAUNCH:
		if (options.debug)
			fprintf(stderr, "%s: running session launch\n", argv[0]);
		run_launch(argc, argv);
		break;
	case COMMAND_CHOOSE:
		if (options.debug)
			fprintf(stderr, "%s: running chooser\n", argv[0]);
		run_choose(argc, argv);
		break;
	case COMMAND_LOGOUT:
		if (options.debug)
			fprintf(stderr, "%s: running logout\n", argv[0]);
		run_logout(argc, argv);
		break;
	case COMMAND_MANAGE:
		if (options.debug)
			fprintf(stderr, "%s: running manager\n", argv[0]);
		run_manage(argc, argv);
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
