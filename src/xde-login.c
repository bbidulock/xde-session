/*****************************************************************************

 Copyright (c) 2010-2020  Monavacon Limited <http://www.monavacon.com/>
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

/** @section Headers
  * @{ */

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
#include <sys/select.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <langinfo.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>
#include <math.h>
#include <dlfcn.h>
#include <setjmp.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#endif
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/Xauth.h>
#include <X11/SM/SMlib.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <pwd.h>
#include <systemd/sd-login.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

/** @} */

/** @section Preamble
  * @{ */

static const char *
_timestamp(void)
{
	static struct timeval tv = { 0, 0 };
	static char buf[BUFSIZ];
	double stamp;

	gettimeofday(&tv, NULL);
	stamp = (double)tv.tv_sec + (double)((double)tv.tv_usec/1000000.0);
	snprintf(buf, BUFSIZ-1, "%f", stamp);
	return buf;
}

#define XPRINTF(_args...) do { } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output > _num) { \
	fprintf(stdout, NAME ": I: "); \
	fprintf(stdout, _args); fflush(stdout); } } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
	fprintf(stderr, NAME ": D: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
	fprintf(stderr, NAME ": E: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr);   } while (0)

#define WPRINTF(_args...) do { \
	fprintf(stderr, NAME ": W: [%s] %12s +%4d %s(): ", _timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, _args); fflush(stderr);   } while (0)

#define PTRACE(_num) do { if (options.debug >= _num) { \
	fprintf(stderr, NAME ": T: [%s] %12s +%4d %s()\n", _timestamp(), __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

void
dumpstack(const char *file, const int line, const char *func)
{
	void *buffer[32];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 32)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, NAME ": E: %12s +%4d : %s() : \t%s\n", file, line, func, strings[i]);
}

#undef EXIT_SUCCESS
#undef EXIT_FAILURE
#undef EXIT_SYNTAXERR

#define EXIT_SUCCESS		0
#define EXIT_FAILURE		1
#define EXIT_SYNTAXERR		2

/** @} */

typedef enum {
	CommandDefault,
	CommandLogin,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	char *host;
	char *user;
	char *display;
	char *seat;
	char *service;
	char *vtnr;
	char *tty;
	char *class;
	char *type;
	char *desktop;
	char *authfile;
	char *shell;
	char *maildir;
	char *homedir;
	int cmd_argc;
	char **cmd_argv;
	Command command;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.host = NULL,
	.user = NULL,
	.seat = NULL,
	.service = NULL,
	.vtnr = NULL,
	.tty = NULL,
	.class = NULL,
	.type = NULL,
	.desktop = NULL,
	.authfile = NULL,
	.shell = NULL,
	.maildir = NULL,
	.homedir = NULL,
	.cmd_argc = 0,
	.cmd_argv = NULL,
	.command = CommandDefault,
};

#ifdef PAM_XAUTHDATA
void
add_xauth_data(pam_handle_t *pamh)
{
	char *file, *nam, *num;
	Xauth *xau;
	int namlen, numlen;
	char *typ = "MIT-MAGIC-COOKIE-1";
	int typlen = strlen(typ);
	struct pam_xauth_data data;

	DPRINTF(1, "strapped out for now\n");
	return;
	DPRINTF(1, "determining X auth data\n");
	nam = options.display;
	namlen = strchr(nam, ':') - nam;
	num = nam + namlen + 1;
	numlen = strlen(num);

	if (!(file = options.authfile))
		file = XauFileName();
	if (!file) {
		EPRINTF("cannot determine Xauthority file name\n");
		return;
	}
	xau = XauGetBestAuthByAddr(FamilyNetname, namlen, nam, numlen, num, 1, &typ, &typlen);
	if (xau) {
		EPRINTF("cannot obtain useable Xauth entry\n");
		return;
	}

	data.namelen = xau->name_length;
	data.name = xau->name;
	data.datalen = xau->data_length;
	data.data = xau->data;

	DPRINTF(1, "setting X auth data\n");
	pam_set_item(pamh, PAM_XAUTHDATA, &data);
	XauDisposeAuth(xau);
	return;
}
#endif

static int caught_signal;

void
signal_child(int signum)
{
	caught_signal = signum;
}

static int
pam_conv_cb(int len, const struct pam_message **msg, struct pam_response **resp, void *data)
{
	(void) len;
	(void) msg;
	(void) resp;
	(void) data;
	return PAM_SUCCESS;	/* we don't do auth */
}

void
run_login(int argc, char *const *argv)
{
	pam_handle_t *pamh = NULL;
	const char *env;
	pid_t pid;
	uid_t uid;
	int result, status;
	struct pam_conv conv = { pam_conv_cb, NULL };
	FILE *dummy;
	const char **var, *vars[] =
	    { "PATH", "LANG", "USER", "LOGNAME", "HOME", "SHELL", "XDG_SEAT", "XDG_VNTR", "MAIL", "TERM", "HOME", "PWD", NULL };

	(void) argc;
	(void) argv;
	uid = getuid();
#if 0
	if (setresuid(0, 0, 0)) {
		EPRINTF("setresuid: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
	switch ((pid = fork())) {
	case 0: /* the child */
		dummy = freopen("/dev/null", "r", stdin);
		dummy = freopen("/dev/null", "w", stdout);
		(void) dummy;
		if (setsid() < 0) {
			EPRINTF("setsid: %s\n", strerror(errno));
			exit(errno);
		}
		break;
	case -1:
		EPRINTF("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	default: /* the parent */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && !WEXITSTATUS(status))
			exit(EXIT_SUCCESS);
		exit(EXIT_FAILURE);
	}
	DPRINTF(1, "starting PAM\n");
	result = pam_start(options.service, options.user, &conv, &pamh);
	if (result != PAM_SUCCESS) {
		EPRINTF("pam_start: %s\n", pam_strerror(pamh, result));
		exit(EXIT_FAILURE);
	}
	DPRINTF(1, "adjusting environment variables\n");
	unsetenv("XDG_SESSION_ID");
	pam_putenv(pamh, "XDG_SESSION_ID");

	if (options.display) {
		DPRINTF(1, "setting DISPLAY=%s\n", options.display);
		setenv("DISPLAY", options.display, 1);
		pam_misc_setenv(pamh, "DISPLAY", options.display, 1);
	}
	if (options.authfile) {
		DPRINTF(1, "setting XAUTHORITY=%s\n", options.authfile);
		setenv("XAUTHORITY", options.authfile, 1);
		pam_misc_setenv(pamh, "XAUTHORITY", options.authfile, 1);
	}
	if (options.class) {
		DPRINTF(1, "setting XDG_SESSION_CLASS=%s\n", options.class);
		setenv("XDG_SESSION_CLASS", options.class, 1);
		pam_misc_setenv(pamh, "XDG_SESSION_CLASS", options.class, 1);
	} else {
		DPRINTF(1, "unsetting XDG_SESSION_CLASS\n");
		unsetenv("XDG_SESSION_CLASS");
		pam_putenv(pamh, "XDG_SESSION_CLASS");
	}
	if (options.type) {
		DPRINTF(1, "setting XDG_SESSION_TYPE=%s\n", options.type);
		setenv("XDG_SESSION_TYPE", options.type, 1);
		pam_misc_setenv(pamh, "XDG_SESSION_TYPE", options.type, 1);
	} else {
		DPRINTF(1, "unsetting XDG_SESSION_TYPE\n");
		unsetenv("XDG_SESSION_TYPE");
		pam_putenv(pamh, "XDG_SESSION_TYPE");
	}
	if (options.seat) {
		DPRINTF(1, "setting XDG_SEAT=%s\n", options.seat);
		setenv("XDG_SEAT", options.seat, 1);
		pam_misc_setenv(pamh, "XDG_SEAT", options.seat, 1);
	} else {
		DPRINTF(1, "unsetting XDG_SEAT\n");
		unsetenv("XDG_SEAT");
		pam_putenv(pamh, "XDG_SEAT");
	}
	if (options.vtnr) {
		DPRINTF(1, "setting XDG_VTNR=%s\n", options.vtnr);
		setenv("XDG_VTNR", options.vtnr, 1);
		pam_misc_setenv(pamh, "XDG_VTNR", options.vtnr, 1);
	} else {
		DPRINTF(1, "unsetting XDG_VTNR\n");
		unsetenv("XDG_VTNR");
		pam_putenv(pamh, "XDG_VTNR");
	}
	if (options.desktop) {
		DPRINTF(1, "setting XDG_SESSION_DESKTOP=%s\n", options.desktop);
		setenv("XDG_SESSION_DESKTOP", options.desktop, 1);
		pam_misc_setenv(pamh, "XDG_SESSION_DESKTOP", options.desktop, 1);
	} else {
		unsetenv("XDG_SESSION_DESKTOP");
		pam_putenv(pamh, "XDG_SESSION_DESKTOP");
	}

	DPRINTF(1, "setting PAM items\n");
	if (options.user)
		pam_set_item(pamh, PAM_RUSER, options.user);
	if (options.host)
		pam_set_item(pamh, PAM_RHOST, options.host);
	if (options.tty)
		pam_set_item(pamh, PAM_TTY, options.tty);
	else if (options.display)
		pam_set_item(pamh, PAM_TTY, options.display);
#ifdef PAM_XDISPLAY
	if (options.display)
		pam_set_item(pamh, PAM_XDISPLAY, options.display);
#endif
#ifdef PAM_XAUTHDATA
	add_xauth_data(pamh);
#endif
	DPRINTF(1, "setting PAM environment variables\n");
	if (options.user) {
		pam_misc_setenv(pamh, "USER", options.user, 1);
		pam_misc_setenv(pamh, "LOGNAME", options.user, 1);
	}
	if (options.display)
		pam_misc_setenv(pamh, "DISPLAY", options.display, 1);
	if (options.authfile)
		pam_misc_setenv(pamh, "XAUTHORITY", options.authfile, 1);
	if (options.maildir)
		pam_misc_setenv(pamh, "MAIL", options.maildir, 1);
	if (options.homedir) {
		pam_misc_setenv(pamh, "HOME", options.homedir, 1);
		pam_misc_setenv(pamh, "PWD", options.homedir, 1);
	}
	if (options.vtnr)
		pam_misc_setenv(pamh, "WINDOWPATH", options.vtnr, 1);

	for (var = vars; *var; var++)
		if ((env = getenv(*var)))
			pam_misc_setenv(pamh, *var, env, 1);

	DPRINTF(1, "establishing credentials\n");
	result = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if (result != PAM_SUCCESS) {
		EPRINTF("pam_setcred: %s\n", pam_strerror(pamh, result));
		pam_end(pamh, result);
		exit(EXIT_FAILURE);
	}
	DPRINTF(1, "trying to close previous session\n");
	result = pam_close_session(pamh, 0);
	if (result != PAM_SUCCESS) {
		EPRINTF("pam_close_session: %s\n", pam_strerror(pamh, result));
		pam_end(pamh, result);
		exit(EXIT_FAILURE);
	}
	DPRINTF(1, "opening session\n");
	result = pam_open_session(pamh, 0);
	if (result != PAM_SUCCESS) {
		EPRINTF("pam_open_session: %s\n", pam_strerror(pamh, result));
		pam_end(pamh, result);
		exit(EXIT_FAILURE);
	}

	/* catch signals and pass to child in case child becomes a session or
	   process group leader */
	signal(SIGTERM, signal_child);
	signal(SIGQUIT, signal_child);
	signal(SIGHUP, signal_child);

	DPRINTF(1, "forking child\n");
	switch ((pid = fork())) {
	case 0:		/* the child */
		/* drop privileges */
		if (setresuid(uid, uid, uid)) {
			EPRINTF("setuid: %s\n", strerror(errno));
			_exit(errno);
		}
		if (setsid() < 0) {
			EPRINTF("setsid: %s\n", strerror(errno));
			_exit(errno);
		}
		if (options.debug) {
			char **arg;

			DPRINTF(1, "command is:");
			for (arg = options.cmd_argv; arg && *arg; arg++)
				fprintf(stderr, " '%s'", *arg);
			fprintf(stderr, "\n");
		}
		execvpe(options.cmd_argv[0], options.cmd_argv, pam_getenvlist(pamh));
		EPRINTF("execve: %s\n", strerror(errno));
		_exit(EXIT_FAILURE);
		break;
	case -1:
		EPRINTF("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	default:		/* the parent */
		break;
	}

	caught_signal = 0;

	for (;;) {
		DPRINTF(1, "waiting for child\n");
		if (waitpid(pid, &status, WUNTRACED | WCONTINUED) == -1) {
			if (errno != EINTR) {
				EPRINTF("waitpid: %s\n", strerror(errno));
				kill(pid, SIGTERM);
				kill(pid, SIGCONT);
				exit(EXIT_FAILURE);
			}
			if (caught_signal) {
				caught_signal = 0;
				kill(pid, SIGTERM);
				kill(pid, SIGCONT);
			}
			continue;
		}
		if (WIFEXITED(status)) {
			int rc = WEXITSTATUS(status);

			DPRINTF(1, "child returned with exit status %d\n", rc);
			break;
		} else if (WIFSIGNALED(status)) {
			int signum, dump;

			signum = WTERMSIG(status);
			dump = WCOREDUMP(status);
			DPRINTF(1, "child exited on signal %d%s\n", signum,
				dump ? " (dumped core)" : "");
			break;
		} else if (WIFSTOPPED(status)) {
			int signum;

			signum = WSTOPSIG(status);
			DPRINTF(1, "child stoppped on signal %d\n", signum);
			continue;
		} else if (WIFCONTINUED(status)) {
			DPRINTF(1, "child continued\n");
			continue;
		} else {
			EPRINTF("invalid status %d\n", status);
			kill(pid, SIGTERM);
			kill(pid, SIGCONT);
			exit(EXIT_FAILURE);
		}
	}
	DPRINTF(1, "closing PAM session\n");
	result = pam_close_session(pamh, 0);
	if (result != PAM_SUCCESS) {
		EPRINTF("pam_close_session: %s\n", pam_strerror(pamh, result));
		pam_end(pamh, result);
		exit(EXIT_FAILURE);
	}
	DPRINTF(1, "ending PAM\n");
	pam_end(pamh, result);
	DPRINTF(1, "done\n");
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
Copyright (c) 2010-2020  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2020  Monavacon Limited.\n\
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
    %1$s [options] {-e|--} [COMMAND ARG ...]\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

const char *
show_cmd(int argc, char **argv)
{
	static char cmd[PATH_MAX+1] = { 0, };
	char **arg;
	int len;

	(void) argc;
	cmd[0] = '\0';
	for (arg = argv; arg && *arg; arg++) {
		strcat(cmd, *arg);
		strcat(cmd, " ");
	}
	if ((len = strlen(cmd)))
		cmd[len-1] = '\0';
	return cmd;
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
    %1$s [options] {-e|--} [COMMAND ARG ...]\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    COMMAND ARG ...            (%18$s)\n\
        command and args to exec within session\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -d, --display DISPLAY      (%11$s)\n\
        display to use instead of $DISPLAY\n\
    -u, --user USER            (%12$s)\n\
        user of session to create\n\
    -s, --seat SEAT            (%2$s)\n\
        the login1 seat, SEAT\n\
    -S, --service SERVICE      (%3$s)\n\
        the PAM service, SERVICE\n\
    -T, --vt VTNR              (%4$s)\n\
        the virtual terminal number, VTNR\n\
    -y, --tty TTY              (%5$s)\n\
        the tty device, TTY\n\
    -c, --class CLASS          (%6$s)\n\
        the XDG_SESSION_CLASS, CLASS\n\
    -t, --type TYPE            (%7$s)\n\
        the XDG_SESSION_TYPE, TYPE\n\
    -A, --authfile FILE        (%13$s)\n\
        the X authority file\n\
    -l,  --shell SHELL         (%15$s)\n\
        the user shell\n\
    -m, --maildir MAILDIR      (%16$s)\n\
        the user mail directory\n\
    -M, --homedir HOMEDIR      (%17$s)\n\
        the user home directory\n\
    -E, --desktop DESKTOP      (%14$s)\n\
        target XDG desktop environment\n\
    -n, --dry-run              (%8$s)\n\
        do not do anything: just print it\n\
    -D, --debug [LEVEL]        (%9$d)\n\
        increment or set debug LEVEL\n\
        this option may be repeated.\n\
    -v, --verbose [LEVEL]      (%10$d)\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated.\n\
"
		, argv[0]
		, options.seat
		, options.service
		, options.vtnr
		, options.tty
		, options.class
		, options.type
		, options.dryrun ? "true" : "false"
		, options.debug
		, options.output
		, options.display
		, options.user
		, options.authfile
		, options.desktop
		, options.shell
		, options.maildir
		, options.homedir
		, show_cmd(options.cmd_argc, options.cmd_argv)
	);
	/* *INDENT-ON* */
}

void
set_defaults(void)
{
	const char *env;
	uid_t me;
	struct passwd *pw;
	char buf[HOST_NAME_MAX + 1] = { 0, };

	options.display = (env = getenv("DISPLAY")) ? strdup(env) : NULL;
	options.authfile = (env = XauFileName())? strdup(env) : NULL;
	options.desktop = (env = getenv("XDG_CURRENT_DESKTOP")) ? strdup(env) : NULL;
	options.seat = strdup("auto");
	options.service = strdup("login");
	options.vtnr = strdup("auto");
	options.tty = strdup("auto");
	options.class = strdup("user");
	options.type = strdup("x11");
	options.dryrun = False;
	options.debug = 0;
	options.output = 1;
	me = getuid();
	if ((pw = getpwuid(me))) {
		int len;

		options.user = strdup(pw->pw_name);
		options.shell = pw->pw_shell ? strdup(pw->pw_shell) : NULL;
		if (options.shell[0] == '\0') {
			free(options.shell);
			setusershell();
			options.shell = strdup(getusershell());
			endusershell();
		}
		len = strlen("/var/mail/") + strlen(pw->pw_name);
		options.maildir = calloc(len + 1, sizeof(*options.maildir));
		strcpy(options.maildir, "/var/mail/");
		strcat(options.maildir, pw->pw_name);
		options.homedir = strdup(pw->pw_dir);
	}
	if (gethostname(buf, HOST_NAME_MAX) == -1)
		EPRINTF("gethostname: %s\n", strerror(errno));
	else
		options.host = strdup(buf);
}

void
get_x11_defaults(void)
{
	Display *dpy;
	Window root;
	Atom property, actual;
	int format;
	unsigned long nitems, after;
	long *data;

	if (!(dpy = XOpenDisplay(0))) {
		EPRINTF("cannot open display %s\n", options.display);
		exit(EXIT_FAILURE);
	}
	root = RootWindow(dpy, 0);
	if (!strcmp(options.vtnr, "auto")) {
		free(options.vtnr);
		options.vtnr = NULL;
		format = 0;
		nitems = 0;
		data = NULL;
		if ((property = XInternAtom(dpy, "XFree86_VT", True)) &&
		    XGetWindowProperty(dpy, root, property, 0, 1, False, XA_INTEGER, &actual,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success &&
		    format == 32 && nitems && data) {
			options.vtnr = calloc(16, sizeof(*options.vtnr));
			snprintf(options.vtnr, 16, "%lu", *(unsigned long *) data);
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}
	if (!strcmp(options.seat, "auto")) {
		free(options.seat);
		options.seat = NULL;
		format = 0;
		nitems = 0;
		data = NULL;
		if ((property = XInternAtom(dpy, "Xorg_Seat", True)) &&
		    XGetWindowProperty(dpy, root, property, 0, 16, False, XA_STRING, &actual,
				       &format, &nitems, &after,
				       (unsigned char **) &data) == Success &&
		    format == 8 && nitems && data) {
			options.seat = calloc(nitems + 1, sizeof(*options.seat));
			strncpy(options.seat, (char *) data, nitems);
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
		if (!options.seat && options.vtnr)
			options.seat = strdup("seat0");
	}
	if (!strcmp(options.tty, "auto")) {
		free(options.tty);
		options.tty = NULL;
		if (options.vtnr) {
			options.tty = calloc(16, sizeof(*options.tty));
			snprintf(options.tty, 16, "tty%s", options.vtnr);
		}
	}
	XCloseDisplay(dpy);
}

void
get_defaults(void)
{
	if (options.display)
		setenv("DISPLAY", options.display, 1);
	get_x11_defaults();
	if (options.command == CommandDefault)
		options.command = CommandLogin;
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;
	struct passwd *pw;
	char *args[5] = { NULL, };

	setlocale(LC_ALL, "");

	set_defaults();

	while (1) {
		int c, val;
		char *endptr = NULL;
		long num;
		char *p;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"authfile",	required_argument,  NULL,   'A'},
			{"shell",	required_argument,  NULL,   'l'},
			{"maildir",	required_argument,  NULL,   'm'},
			{"homedir",	required_argument,  NULL,   'M'},
			{"desktop",	required_argument,  NULL,   'E'},
//			{"user",	required_argument,  NULL,   'u'},
			{"seat",	required_argument,  NULL,   's'},
			{"service",	required_argument,  NULL,   'S'},
			{"vt",		required_argument,  NULL,   'T'},
			{"tty",		required_argument,  NULL,   'y'},
			{"class",	required_argument,  NULL,   'c'},
			{"type",	required_argument,  NULL,   't'},
			{"exec",	no_argument,	    NULL,   'e'},

			{"dry-run",	no_argument,	    NULL,   'n'},
			{"debug",	optional_argument,  NULL,   'D'},
			{"verbose",	optional_argument,  NULL,   'v'},
			{"help",	optional_argument,  NULL,   'h'},
			{"version",	no_argument,	    NULL,   'V'},
			{"copying",	no_argument,	    NULL,   'C'},
			{"?",		no_argument,	    NULL,   'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "A:l:m:M:E:u:s:S:T:y:c:t:enD::v::hVCH?", long_options,
				     &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "A:l:m:M:E:u:s:S:T:y:c:t:enDvhVC?");
#endif				/* _GNU_SOURCE */
		if (c == -1 || c == 'e') {	/* -e, --exec COMMAND ARG ... */
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'A':	/* -A, --authfile FILE */
			free(options.authfile);
			options.authfile = strndup(optarg, PATH_MAX);
			break;
		case 'l':	/* -l, --shell SHELL */
			free(options.shell);
			options.shell = strdup(optarg);
			break;
		case 'm':	/* -m, --maildir MAILDIR */
			free(options.maildir);
			options.maildir = strdup(optarg);
			break;
		case 'M':	/* -M, --homedir HOMEDIR */
			free(options.homedir);
			options.homedir = strdup(optarg);
			break;
		case 'E':	/* -E, --desktop DESKTOP */
			free(options.desktop);
			options.desktop = strdup(optarg);
			break;
		case 'u':	/* -u, --user USER */
			if (!(pw = getpwnam(optarg)))
				goto bad_option;
			free(options.user);
			options.user = strdup(optarg);
			break;
		case 's':	/* -s, --seat SEAT */
			free(options.seat);
			options.seat = strdup(optarg);
			break;
		case 'S':	/* -S, --service SERVICE */
			free(options.service);
			options.service = strdup(optarg);
			break;
		case 'T':	/* -T, --vt VTNR */
			num = strtol(optarg, &endptr, 0);
			if (*endptr || num < 1 || num > 12)
				goto bad_option;
			free(options.vtnr);
			options.vtnr = strdup(optarg);
			break;
		case 'y':	/* -y, --tty TTY */
			p = (p = strrchr(optarg, '/')) ? p + 1 : optarg;
			if (!*p)
				goto bad_option;
			free(options.tty);
			options.tty = strdup(p);
			break;
		case 'c':	/* -c, --class CLASS */
			free(options.class);
			options.class = strdup(optarg);
			break;
		case 't':	/* -t, --type TYPE */
			free(options.type);
			options.type = strdup(optarg);
			break;

		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (optarg == NULL) {
				DPRINTF(1, "%s: increasing debug verbosity\n", argv[0]);
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			DPRINTF(1, "%s: setting debug verbosity to %d\n", argv[0], val);
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			if (optarg == NULL) {
				DPRINTF(1, "%s: increasing output verbosity\n", argv[0]);
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			DPRINTF(1, "%s: setting output verbosity to %d\n", argv[0], val);
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			DPRINTF(1, "Setting command to CommandVersion\n");
			command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			DPRINTF(1, "Setting command to CommandCopying\n");
			command = CommandCopying;
			break;
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					EPRINTF("%s: syntax error near '", argv[0]);
					while (optind < argc) {
						fprintf(stderr, "%s", argv[optind++]);
						fprintf(stderr, "%s", (optind < argc) ? " " : "");
					}
					fprintf(stderr, "'\n");
				} else {
					EPRINTF("%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(EXIT_SYNTAXERR);
		}
	}
	DPRINTF(1, "%s: option index = %d\n", argv[0], optind);
	DPRINTF(1, "%s: option count = %d\n", argv[0], argc);
	if (optind >= argc) {
		char *init = calloc(PATH_MAX + 1, sizeof(*init));

		strcpy(init, options.homedir);
		strcat(init, "/.xinitrc");
		if (access(init, R_OK) == 0) {
			args[0] = "/bin/sh";
			args[1] = "-ls";
			args[2] = "-C";
			args[3] = init;
			args[4] = NULL;
			options.cmd_argc = 4;
			options.cmd_argv = args;
		} else {
			strcpy(init, "/etc/X11/xinit/xinitrc");
			if (access(init, R_OK) == 0) {
				args[0] = "/bin/sh";
				args[1] = "-l";
				args[2] = init;
				args[3] = NULL;
				options.cmd_argc = 3;
				options.cmd_argv = args;
			} else {
				fprintf(stderr, "%s: missing non-option argument\n", argv[0]);
				goto bad_nonopt;
			}
		}
	} else {
		options.cmd_argc = argc - optind;
		options.cmd_argv = argv + optind;
	}
	get_defaults();

	switch (command) {
	default:
	case CommandDefault:
	case CommandLogin:
		DPRINTF(1, "%s: running login\n", argv[0]);
		run_login(options.cmd_argc, options.cmd_argv);
		break;
	case CommandHelp:
		DPRINTF(1, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF(1, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF(1, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=88 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
