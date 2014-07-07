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

#include "xde-xsession.h"

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

typedef enum {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_BOTTOM,
} LogoSide;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *display;
        char *rcfile;
	char *desktop;
	char *profile;
	char *startwm;
	char *file;
	char *setup;
	char **exec;
	int exec_num;
	Bool autostart;
	Bool wait;
	unsigned long pause;
	Bool toolwait;
	unsigned long guard;
	unsigned long delay;
	Bool splash;
	char *image;
	char *banner;
	LogoSide side;
	char *icons;
	char *theme;
	char *cursors;
	Bool usexde;
	Bool xinit;
	char *argument;
} Options;

typedef struct {
	char *output;
	char *debug;
	char *dryrun;
	char *display;
        char *rcfile;
	char *desktop;
	char *profile;
	char *startwm;
	char *file;
	char *setup;
	char *exec;
	char *autostart;
	char *wait;
	char *pause;
	char *toolwait;
	char *guard;
        char *delay;
	char *splash;
	char *image;
	char *banner;
	char *side;
	char *icons;
	char *theme;
	char *cursors;
	char *usexde;
        char *xinit;
	char *argument;
} Optargs;

Optargs optargs = { NULL, };

Optargs defaults = {
	.output = "1",
	.debug = "0",
	.dryrun = "false",
	.display = "$DISPLAY",
        .rcfile = "$XDG_CONFIG_HOME/xde/default/session.ini",
	.desktop = "",
	.profile = "default",
	.startwm = "",
	.file = "xde/$PROFILE/autostart",
	.setup = "",
	.exec = "",
	.autostart = "true",
	.wait = "true",
	.pause = "2000",
	.toolwait = "false",
	.guard = "200",
	.delay = "0",
	.splash = "images/${XDG_MENU_PREFIX}splash.png",
	.image = "images/${XDG_MENU_PREFIX}splash.png",
	.banner = "images/${XDG_MENU_PREFIX}banner.png",
	.side = "top",
	.icons = "",
	.theme = "",
	.cursors = "",
	.usexde = "false",
        .xinit = "false",
	.argument = "choose",
};

Options options = {
	.output = 1,
	.debug = 0,
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
	.splash = NULL,
	.banner = NULL,
	.side = LOGO_SIDE_LEFT,
	.icons = NULL,
	.theme = NULL,
	.cursors = NULL,
	.usexde = False,
	.xinit = False,
	.argument = NULL,
};

static const char *XDESessionGroup = "XDE Session";

/** @brief set configuration file defaults
  * @param config - key file in which to set defaults
  */
void
set_config_defaults(GKeyFile *config)
{
        const char *key;

        key = "DefaultXSession";
	if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
		g_key_file_set_string(config, XDESessionGroup, key, "choose");
        key = "Desktop";
	if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "$PROFILE");
        key = "StartupFile";
	if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "xde/$PROFILE/autostart");
        key = "Wait";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, FALSE);
        key = "Pause";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_uint64(config, XDESessionGroup, key, 2);
        key = "ToolWait";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, FALSE);
        key = "ToolWaitGuard";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_uint64(config, XDESessionGroup, key, 200);
        key = "ToolWaitDelay";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_uint64(config, XDESessionGroup, key, 0);
        key = "Language";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "Vendor";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "Splash";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "SplashImage";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "ChooserPromptText";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "");
        key = "ChooserPromptMarkup";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "");
        key = "LogoutPromptText";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "Log out of %p session?");
        key = "LogoutPromptMarkup";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "Log out of <b>%p</b> session?");
        key = "ChooserSide";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "left");
        key = "SplashSide";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "top");
        key = "LogoutSide";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_string(config, XDESessionGroup, key, "left");
        key = "IconTheme";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "Theme";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "CursorTheme";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "UseXDETheme";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, FALSE);
        key = "SessionManagement";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "SessionManagementProxy";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "SessionManageDockApps";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "SessionManageSysTray";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "ScreenLocking";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "ScreenLockerProgram";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                ;
        key = "AutoStart";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "StartupNotificationAssist";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "XSettings";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "XInput";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "ThemeManagement";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "ThemeManageDockApps";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
        key = "ThemeManageSysTray";
        if (!g_key_file_has_key(config, XDESessionGroup, key, NULL))
                g_key_file_set_boolean(config, XDESessionGroup, key, TRUE);
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
	if (options.rcfile) {
		if (access(options.rcfile, R_OK))
			EPRINTF("%s: %s\n", options.rcfile, strerror(errno));
		else {
			if (g_key_file_load_file(config, options.rcfile, flags, NULL))
				rcfile = strdup(options.rcfile);
			else
				EPRINTF("%s: could not load key file\n", rcfile);
		}
	}
	if (!rcfile) {
		char *home = getenv("HOME") ? : ".";
		char *xhome = getenv("XDG_CONFIG_HOME");
		char *xdirs = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";
		char *dirs, *file, *dir, *end;
		int len;

		if (xhome)
			xhome = strdup(xhome);
		else {
			len = strlen(home) + strlen("/.config") + 1;
			xhome = calloc(len, sizeof(*xhome));
			strncpy(xhome, home, len);
			strncat(xhome, "/.config", len);
		}
		len = strlen(xhome) + 1 + strlen(xdirs) + 1;
		dirs = calloc(len, sizeof(*dirs));
		strncpy(dirs, xhome, len);
		strncat(dirs, ":", len);
		strncat(dirs, xdirs, len);
		/* go through then forward */
		for (dir = dirs, end = dirs + strlen(dirs); dir < end;
		     *strchrnul(dir, ":") = '\0', dir += strlen(dir) + 1) {
			if (!*dir)
				continue;
			len = strlen(dir) + strlen("xde/") +
			    strlen(options.profile) + strlen("/session.ini")
			    + 1;
			file = calloc(len, sizeof(*file));
			strncpy(file, dir, len);
			strncat(file, "xde/", len);
			strncat(file, options.profile, len);
			strncat(file, "/session.ini", len);
			if (!access(file, R_OK)) {
				if (g_key_file_load(config, file, flags, NULL)) {
					rcfile = file;
					break;
				}
				DPRINTF("%s: could not load key file\n", file);
			}
			free(file);
		}
		free(dirs);
		free(xhome);
	}
	if (rcfile)
		g_key_file_set_string(config, XDESessionGroup, "FileName", rcfile);
	free(rcfile);
	set_config_defaults(config);
	return (config);
}

void
set_defaults(void)
{
	options.profile = strdup("default");
}

void
get_defaults(void)
{
	GKeyFile *config = find_read_config();
	const char *key;
	gchar *string;
	gboolean boolean;
	guint64 time;
	GError *err = NULL;

	if (!optargs.desktop) {
		string = g_key_file_get_string(config, XDESessionGroup, "Desktop", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.desktop = optargs.desktop = strdup(string);
			gfree(string);
		}
	}
	if (!optargs.profile) {
		string = g_key_file_get_string(config, XDESessionGroup, "Profile", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.profile = optargs.profile = strdup(string);
			gfree(string);
		}
	}
	if (!optargs.file) {
		string = g_key_file_get_string(config, XDESessionGroup, "StartupFile", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.file = optargs.file = strdup(string);
			gfree(string);
		}
	}
	if (!optargs.wait) {
		boolean = g_key_file_get_boolean(config, XDESessionGroup, "Wait", &err);
		if (err)
			err = NULL;
		else {
			options.wait = boolean ? True : False;
			optargs.wait = boolean ? "true" : "false";
		}
	}
	if (!optargs.pause) {
		time = g_key_file_get_uint64(config, XDESessionGroup, "Pause", &err);
		if (err)
			err = NULL;
		else {
			static char static_string[16];

			options.pause = time;
			sprintf(static_string, "%d", (int) time);
			optargs.pause = static_string;
		}
	}
	if (!optargs.toolwait) {
		boolean = g_key_file_get_boolean(config, XDESessionGroup, "ToolWait", &err);
		if (err)
			err = NULL;
		else {
			options.toolwait = boolean ? True : False;
			optargs.toolwait = boolean ? "true" : "false";
		}
	}
	if (!optargs.guard) {
		time = g_key_file_get_uint64(config, XDESessionGroup, "ToolWaitGuard", &err);
		if (err)
			err = NULL;
		else {
			static char static_string[16];

			options.guard = time;
			sprintf(static_string, "%d", (int) time);
			optargs.guard = static_string;
		}
	}
	if (!optargs.delay) {
		time = g_key_file_get_uint64(config, XDESessionGroup, "ToolWaitDelay", &err);
		if (err)
			err = NULL;
		else {
			static char static_string[16];

			options.delay = time;
			sprintf(static_string, "%d", (int) time);
			optargs.delay = static_string;
		}
	}
	if (!optargs.language) {
		string = g_key_file_get_string(config, XDESessionGroup, "Language", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.language = optargs.language = strdup(string);
			gfree(string);
		}
	}
	if (!optargs.vendor) {
		string = g_key_file_get_string(config, XDESessionGroup, "Vendor", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.vendor = optargs.vendor = strdup(string);
			gfree(string);
		}
	}
	if (!optargs.splash) {
		boolean = g_key_file_get_boolean(config, XDESessionGroup, "Splash", &err);
		if (err)
			err = NULL;
		else {
			options.splash = boolean ? True : False;
			optargs.splash = boolean ? "true" : "false";
		}
	}
	if (!optargs.image) {
		string = g_key_file_get_string(config, XDESessionGroup, "SplashImage", &err);
		if (err)
			err = NULL;
		else if (string) {
			options.image = optargs.image = strdup(string);
			gfree(string);
		}
	}
}

void
run_launch(int argc, char *argv[])
{
}

void
run_choose(int argc, char *argv[])
{
}

void
run_logout(int argc, char *argv[])
{
}

void
run_manage(int argc, char *argv[])
{
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
    %1$s [OPTIONS] [XSESSION]\n\
    %1$s [OPTIONS] {-h|--help}\n\
    %1$s [OPTIONS] {-V|--version}\n\
    %1$s [OPTIONS] {-C|--copying}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [OPTIONS] [XSESSION]\n\
    %1$s [OPTIONS] {-h|--help}\n\
    %1$s [OPTIONS] {-V|--version}\n\
    %1$s [OPTIONS] {-C|--copying}\n\
Arguments:\n\
    XSESSION                            (%2$s)\n\
        The name of the XDG session to execute, or \"default\"\n\
        or \"choose\" or \"logout\" or \"manage\".\n\
Command Options:\n\
General Options:\n\
  --xinit                               (%3$s)\n\
  --display, -d DISPLAY                 (%4$s)\n\
  --desktop, -e DESKTOP                 (%5$s)\n\
  --session, -s PROFILE                 (%6$s)\n\
  --startwm, -m COMMAND                 (%7$s)\n\
  --file, -f FILE                       (%8$s)\n\
  --setup COMMAND                       (%9$s)\n\
  --exec COMMAND                        (%10$s)\n\
  --(no)autostart, -a                   (%11$s)\n\
  --(no)wait, -w                        (%12$s)\n\
  --pause, -p [PAUSE]                   (%13$s)\n\
  --(no)toolwait, -W [GUARD][:DELAY]    (%14$s)\n\
  --charset, -c CHARSET                 (%15$s)\n\
  --language, -L LANGUAGE               (%16$s)\n\
  --vendor VENDOR                       (%17$s)\n\
  --(no)splash, -l [IMAGE]              (%18$s)\n\
  --banner, -b BANNER                   (%19$s)\n\
  --side {top|left|right|bottom}        (%20$s)\n\
  --icons, -i THEME                     (%21$s)\n\
  --theme, -t THEME                     (%22$s)\n\
  --cursors, -z THEME                   (%23$s)\n\
  --xde-theme, -X                       (%24$s)\n\
  --dryrun, -n                          (%25$s)\n\
  --debug, -D [LEVEL]                   (%26$d)\n\
  --verbose, -v [LEVEL]                 (%27$d)\n\
", argv[0]
       , optargs.argument ? : defaults.argument
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
       , optargs.charset ? : defaults.charset
       , optargs.language ? : defaults.language
       , optargs.vendor ? : defaults.vendor
       , optargs.splash ? : defaults.splash
       , optargs.banner ? : defaults.banner
       , optargs.side ? : defaults.side
       , optargs.icons ? : defaults.icons
       , optargs.theme ? : defaults.theme
       , optargs.cursors ? : defaults.cursors
       , optargs.usexde ? : defaults.usexde
       , optargs.dryrun ? : defaults.dryrun
       , options.debug
       , options.output
);
}

typedef enum {
	COMMAND_DEFAULT,
	COMMAND_LAUNCH,
	COMMAND_CHOOSE,
	COMMAND_LOGOUT,
	COMMAND_MANAGE,
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_COPYING,
} CommandType;

int
main(int argc, char *argv[])
{
	CommandType command = COMMAND_DEFAULT;

	set_defaults();

	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"launch",	no_argument,		NULL, '0'},
			{"chooser",	no_argument,		NULL, '2'},
			{"logout",	no_argument,		NULL, '3'},
			{"manager",	no_argument,		NULL, '4'},

			{"xinit",	no_argument,		NULL, '1'},
			{"display",	required_argument,	NULL, 'd'},
                        {"rcfile",      required_argument,      NULL, 'r'},
			{"desktop",	required_argument,	NULL, 'e'},
			{"session",	required_argument,	NULL, 's'},
			{"startwm",	required_argument,	NULL, 'm'},
			{"file",	required_argument,	NULL, 'f'},
			{"setup",	required_argument,	NULL, '5'},
			{"exec",	required_argument,	NULL, 'x'},
			{"autostart",	no_argument,		NULL, '6'},
			{"noautostart",	no_argument,		NULL, 'a'},
			{"wait",	no_argument,		NULL, 'w'},
			{"nowait",	no_argument,		NULL, '7'},
			{"pause",	optional_argument,	NULL, 'p'},
			{"toolwait",	optional_argument,	NULL, 'W'},
			{"notoolwait",	optional_argument,	NULL, '8'},
			{"charset",	required_argument,	NULL, 'c'},
			{"language",	required_argument,	NULL, 'L'},
			{"vendor",	required_argument,	NULL, '9'},
			{"splash",	optional_argument,	NULL, 'l'},
			{"nosplash",	no_argument,		NULL, '!'},
			{"banner",	required_argument,	NULL, 'b'},
			{"side",	required_argument,	NULL, 'S'},
			{"icons",	required_argument,	NULL, 'i'},
			{"theme",	required_argument,	NULL, 't'},
			{"cursors",	required_argument,	NULL, 'z'},
			{"xde-theme",	no_argument,		NULL, 'X'},

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

		c = getopt_long_only(argc, argv, "nD::v::hVCH?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "nDvhVCH?");
#endif				/* defined _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case '0':	/* --launch */
			if (command != COMMAND_DEFAULT)
				goto bad_option;
			command = COMMAND_LAUNCH;
			defaults.argument = "choose";
			break;
		case '2':	/* --chooser */
			if (command != COMMAND_DEFAULT)
				goto bad_option;
			command = COMMAND_CHOOSE;
			defaults.argument = "choose";
			break;
		case '3':	/* --logout */
			if (command != COMMAND_DEFAULT)
				goto bad_option;
			command = COMMAND_LOGOUT;
			defaults.argument = "logout";
			break;
		case '4':	/* --manager */
			if (command != COMMAND_DEFAULT)
				goto bad_option;
			command = COMMAND_MANAGE;
			defaults.argument = "manage";
			break;

		case '1':	/* --xinit */
			options.xinit = True;
			break;
		case 'd':	/* -d, --display DISPLAY */
			free(options.display);
			options.display = strdup(optarg);
			optargs.display = optarg;
			break;
                case 'r':       /* -r, --rcfile FILENAME */
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
		case 'S':	/* -S, --setup COMMAND */
			free(options.setup);
			options.setup = strdup(optarg);
			optargs.setup = optarg;
			break;
		case 'x':	/* -x, --exec COMMAND */
			options.exec = realloc(options.exec, (options.exec_num + 1)
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
		case '9':	/* --vendor VENDOR */
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
			optargs.side = optarg;
			if (!strncasecmp(optarg, "left", strlen(optarg)))
				options.side = LOGO_SIDE_LEFT;
			else if (!strncasecmp(optarg, "top", strlen(optarg)))
				options.side = LOGO_SIDE_TOP;
			else if (!strncasecmp(optarg, "right", strlen(optarg)))
				options.side = LOGO_SIDE_RIGHT;
			else if (!strncasecmp(optarg, "bottom", strlen(optarg)))
				options.side = LOGO_SIDE_BOTTOM;
			break;
		case 'i':	/* -i, --icons THEME */
			free(options.icons);
			options.icons = strdup(optarg);
			optargs.icons = optarg;
			break;
		case 't':	/* -t, --theme THEME */
			free(options.theme);
			options.theme = strdup(optarg);
			optargs.theme = optarg;
			break;
		case 'z':	/* -z, --cursors THEME */
			free(options.cursors);
			options.cursors = strdup(optarg);
			optargs.cursors = optarg;
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
		free(options.argument);
		options.argument = strdup(argv[optind]);
		if (strcasecmp(options.argument, "default")) {
			if (command != COMMAND_DEFAULT && command != COMMAND_LAUNCH)
				goto bad_nonopt;
			command = COMMAND_LAUNCH;
		} else if (strcasecmp(options.argument, "current")) {
			if (command != COMMAND_DEFAULT && command != COMMAND_LAUNCH)
				goto bad_nonopt;
			command = COMMAND_LAUNCH;
		} else if (strcasecmp(options.argument, "choose")) {
			if (command != COMMAND_DEFAULT && command != COMMAND_CHOOSE)
				goto bad_nonopt;
			command = COMMAND_CHOOSE;
		} else if (strcasecmp(options.argument, "logout")) {
			if (command != COMMAND_DEFAULT && command != COMMAND_LOGOUT)
				goto bad_nonopt;
			command = COMMAND_LOGOUT;
		} else if (strcasecmp(options.argument, "manage")) {
			if (command != COMMAND_DEFAULT && command != COMMAND_MANAGE)
				goto bad_nonopt;
			command = COMMAND_MANAGE;
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
