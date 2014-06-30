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
};

char **
get_data_dirs(int *np)
{
	char *home, *xhome, *xdata, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_DATA_HOME");
	xdata = getenv("XDG_DATA_DIRS") ? : "/usr/loca/share:/usr/share";

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

void
run_autostart(int argc, char *argv[])
{
	/* FIXME: write me */
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
        ,options.display
        ,options.desktop
        ,options.session ?: "false"
        ,options.file
        ,show_bool(options.autostart)
        ,show_bool(options.wait)
        ,options.pause
        ,options.splash ? options.banner : "false"
        ,options.guard ,options.delay
        ,options.charset
        ,options.language
        ,options.banner
        ,show_side(options.side)
        ,options.icon_theme ?: "auto"
        ,options.gtk2_theme ?: "auto"
        ,show_bool(options.usexde)
        ,show_bool(options.foreground)
        ,options.client_id ?: ""
        ,show_bool(options.dryrun)
        ,options.debug
        ,options.output
);
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
        options.display = strdup(getenv("DISPLAY") ?: "");
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

		c = getopt_long_only(argc, argv, "d:e:s:x:f:awp::l::W::c:L:b:S:i:t:XFnD::v::hVCH?", long_options,
				     &option_index);
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
