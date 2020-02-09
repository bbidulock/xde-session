/*****************************************************************************

 Copyright (c) 2010-2018  Monavacon Limited <http://www.monavacon.com/>
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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#ifdef _GNU_SOURCE
#include <getopt.h>
#endif
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stdout, "I: "); \
	fprintf(stdout, args); \
	fflush(stdout); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)
#define PTRACE() do { if (options.debug > 0 || options.output > 2) { \
	fprintf(stderr, "T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#undef EXIT_SYNTAXERR

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define EXIT_SYNTAXERR	2

typedef enum {
	CommandDefault,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int output;
	int debug;
	Command command;
	char *display;
	int screen;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.command = CommandDefault,
	.display = NULL,
	.screen = 0,
};

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
Copyright (c) 2010-2018  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018  Monavacon Limited.\n\
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
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
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
    %1$s [options]\n\
    %1$s {-h|--help} [options]\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
Options:\n\
    -d, --display DISPLAY\n\
        specify the X display, DISPLAY, to use [default: %4$s]\n\
    -s, --screen SCREEN\n\
        specify the screen number, SCREEN, to use [default: %5$d]\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: %2$d]\n\
        this option may be repeated\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: %3$d]\n\
        this option may be repeated\n\
", argv[0]
	, options.debug
	, options.output
	, options.display
	, options.screen
);
	/* *INDENT-ON* */
}

static void
set_defaults(void)
{
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;
	Bool exec_mode = False;

	setlocale(LC_ALL, "");

	set_defaults();

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"display",		required_argument,	NULL,	'd'},
			{"screen",		required_argument,	NULL,	's'},

			{"manage",		no_argument,		NULL,	'm'},
			{"replace",		no_argument,		NULL,	'r'},
			{"quit",		no_argument,		NULL,	'q'},
			{"edit",		no_argument,		NULL,	'e'},

			{"nolaunch",		no_argument,		NULL,	'N'},
			{"dry-run",		no_argument,		NULL,	'n'},

			{"client-id",		required_argument,	NULL,	'c'},
			{"save-file",		required_argument,	NULL,	's'},


			{"debug",		optional_argument,	NULL,	'D'},
			{"verbose",		optional_argument,	NULL,	'v'},
			{"help",		no_argument,		NULL,	'h'},
			{"version",		no_argument,		NULL,	'V'},
			{"copying",		no_argument,		NULL,	'C'},
			{"?",			no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "d:s:D::v::hVCH?",
				     long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "d:s:DvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1 || exec_mode) {
			DPRINTF("done options processing\n");
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'd':	/* -d, --display DISPLAY */
			setenv("DISPLAY", optarg, True);
			free(options.display);
			options.display = strdup(optarg);
			break;
		case 's':	/* -s, --screen SCREEN */
			options.screen = strtoul(optarg, &endptr, 0);
			if (endptr && *endptr)
				goto bad_option;
			break;

		case 'D':	/* -D, --debug [LEVEL] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [LEVEL] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
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
			exit(EXIT_SYNTAXERR);
		}
	}
	switch (command) {
	default:
	case CommandDefault:
		break;
	case CommandHelp:
		help(argc, argv);
		break;
	case CommandVersion:
		version(argc, argv);
		break;
	case CommandCopying:
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
