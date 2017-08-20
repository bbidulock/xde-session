/*****************************************************************************

 Copyright (c) 2010-2017  Monavacon Limited <http://www.monavacon.com/>
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

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
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
#include <stdarg.h>
#include <strings.h>
#include <regex.h>

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
#ifdef VNC_SUPPORTED
#include <X11/extensions/Xvnc.h>
#endif
#include <X11/extensions/scrnsaver.h>
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/Xdmcp.h>
#include <X11/Xauth.h>
#include <X11/SM/SMlib.h>
#include <unique/unique.h>
#include <glib-unix.h>
#include <glib/gfileutils.h>
#include <glib/gkeyfile.h>
#include <glib/gdataset.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo.h>

#define GTK_EVENT_STOP		TRUE
#define GTK_EVENT_PROPAGATE	FALSE

#include <pwd.h>
#include <systemd/sd-login.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>

#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#include <langinfo.h>
#include <locale.h>

const char *
timestamp(void)
{
	static struct timeval tv = { 0, 0 };
	static char buf[BUFSIZ];
	double stamp;

	gettimeofday(&tv, NULL);
	stamp = (double)tv.tv_sec + (double)((double)tv.tv_usec/1000000.0);
	snprintf(buf, BUFSIZ-1, "%f", stamp);
	return buf;
}

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stderr, "I: "); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: [%s] %s +%d %s(): ", timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: [%s] %s +%d %s(): ", timestamp(), __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: [%s] %s +%d %s()\n", timestamp(), __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)

#define EXIT_SUCCESS		0
#define EXIT_FAILURE		1
#define EXIT_SYNTAXERR		2

static int saveArgc;
static char **saveArgv;

#undef DO_XCHOOSER
#undef DO_XLOGIN
#undef DO_GREETER
#undef DO_XLOCKING
#undef DO_ONIDLE
#undef DO_CHOOSER
#undef DO_LOGOUT
#undef DO_AUTOSTART
#define DO_SESSION 1
#undef DO_STARTWM

#if defined(DO_XCHOOSER)
#   define RESNAME "xde-xchooser"
#   define RESCLAS "XDE-XChooser"
#   define RESTITL "XDMCP Chooser"
#elif defined(DO_XLOCKING)
#   define LOGO_NAME "gnome-lockscreen"
#   define RESNAME "xde-xlock"
#   define RESCLAS "XDE-XLock"
#   define RESTITL "X11 Locker"
#elif defined(DO_CHOOSER)
#   define RESNAME "xde-chooser"
#   define RESCLAS "XDE-Chooser"
#   define RESTITL "XDE X11 Session Chooser"
#elif defined(DO_LOGOUT)
#   define RESNAME "xde-logout"
#   define RESCLAS "XDE-Logout"
#   define RESTITL "XDE X11 Session Logout"
#elif defined(DO_XLOGIN)
#   define RESNAME "xde-xlogin"
#   define RESCLAS "XDE-XLogin"
#   define RESTITL "XDMCP Greeter"
#elif defined(DO_GREETER)
#   define RESNAME "xde-greeter"
#   define RESCLAS "XDE-Greeter"
#   define RESTITL "XDMCP Greeter"
#elif defined(DO_AUTOSTART)
#   define RESNAME "xde-autostart"
#   define RESCLAS "XDE-AutoStart"
#   define RESTITL "XDE XDG Auto Start"
#elif defined(DO_SESSION)
#   define RESNAME "xde-session"
#   define RESCLAS "XDE-Session"
#   define RESTITL "XDE XDG Session"
#elif defined(DO_STARTWM)
#   define RESNAME "xde-startwm"
#   define RESCLAS "XDE-StartWM"
#   define RESTITL "XDE Sindow Manager Startup"
#else
#   error Undefined program type.
#endif

#define APPDFLT "/usr/share/X11/app-defaults/" RESCLAS

typedef enum _LogoSide {
	LogoSideLeft,
	LogoSideTop,
	LogoSideRight,
	LogoSideBottom,
} LogoSide;

enum {
	OBEYSESS_DISPLAY,		/* obey multipleSessions resource */
	REMANAGE_DISPLAY,		/* force remanage */
	UNMANAGE_DISPLAY,		/* force deletion */
	RESERVER_DISPLAY,		/* force server termination */
	OPENFAILED_DISPLAY,		/* XOpenDisplay failed, retry */
};

#undef EXIT_SUCCESS
#define EXIT_SUCCESS	OBEYSESS_DISPLAY
#undef EXIT_FAILURE
#define EXIT_FAILURE	REMANAGE_DISPLAY
#undef EXIT_SYNTAXERR
#define EXIT_SYNTAXERR	UNMANAGE_DISPLAY

typedef enum {
	CommandDefault,
	CommandHelp,			/* command argument help */
	CommandVersion,			/* command version information */
	CommandCopying,			/* command copying information */
	CommandXlogin,
	CommandXchoose,
	CommandLocker,			/* run as a background locker */
	CommandReplace,			/* replace any running instance */
	CommandLock,			/* ask running instance to lock */
	CommandQuit,			/* ask running instance to quit */
	CommandUnlock,			/* ask running instance to unlock */
	CommandAutostart,
	CommandSession,
} CommandType;

enum {
	BackgroundSourceSplash = (1 << 0),
	BackgroundSourcePixmap = (1 << 1),
	BackgroundSourceRoot = (1 << 2),
};

#ifdef DO_XCHOOSER
typedef enum {
	SocketScopeLoopback,
	SocketScopeLinklocal,
	SocketScopeSitelocal,
	SocketScopePrivate,
	SocketScopeGlobal,
} SocketScope;
#endif

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	CommandType command;
	char *display;
	char *seat;
	char *service;
	char *vtnr;
	char *tty;
#ifdef DO_XCHOOSER
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	SocketScope clientScope;
	uint32_t clientIface;
	Bool isLocal;
#endif
	char *lockscreen;
	char *banner;
	char *welcome;
	char *charset;
	char *language;
	char *desktop;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	LogoSide side;
	Bool prompt;
	Bool noask;
	Bool setdflt;
	Bool launch;
	char *current;
	Bool managed;
	char *session;
	char *choice;
	char *username;
	char *password;
	Bool usexde;
	Bool replace;
	unsigned int timeout;
	char *clientId;
	char *saveFile;
	GKeyFile *dmrc;
	char *vendor;
	char *prefix;
	char *backdrop;
	char **desktops;
	char *file;
	unsigned source;
	Bool xsession;
	Bool setbg;
	Bool transparent;
	int width;
	int height;
	double xposition;
	double yposition;
	Bool setstyle;
	Bool filename;
	unsigned protect;
	Bool tray;
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	char *authfile;
	Bool autologin;
	Bool permitlogin;
	Bool remotelogin;
#endif
	Bool mkdirs;
	char *wmname;
	Bool splash;
	char **setup;
	char *startwm;
	int pause;
	Bool wait;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.command = CommandDefault,
	.display = NULL,
	.seat = NULL,
	.service = NULL,
	.vtnr = NULL,
	.tty = NULL,
#ifdef DO_XCHOOSER
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet6,
	.clientScope = SocketScopeLoopback,
	.clientIface = 0,
	.isLocal = False,
#endif
	.lockscreen = NULL,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.charset = NULL,
	.language = NULL,
	.desktop = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideTop,
	.prompt = False,
	.noask = False,
	.setdflt = False,
	.launch = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.replace = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.backdrop = NULL,
	.desktops = NULL,
	.file = NULL,
	.source = BackgroundSourceSplash,
	.xsession = True,
	.setbg = False,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
	.filename = False,
	.protect = 5,
	.tray = False,
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	.authfile = NULL,
	.autologin = False,
	.permitlogin = True,
	.remotelogin = True,
#endif
	.mkdirs = False,
	.wmname = NULL,
	.setup = NULL,
	.startwm = NULL,
	.pause = 0,
	.wait = False,
};

Options defaults = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.display = NULL,
	.seat = NULL,
	.service = NULL,
	.vtnr = NULL,
	.tty = NULL,
#ifdef DO_XCHOOSER
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet6,
	.clientScope = SocketScopeLoopback,
	.clientIface = 0,
	.isLocal = False,
#endif
	.lockscreen = NULL,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.command = CommandDefault,
	.charset = NULL,
	.language = NULL,
	.icon_theme = NULL,
	.gtk2_theme = NULL,
	.curs_theme = NULL,
	.side = LogoSideLeft,
	.prompt = False,
	.noask = False,
	.setdflt = False,
	.launch = False,
	.current = NULL,
	.managed = True,
	.session = NULL,
	.choice = NULL,
	.username = NULL,
	.password = NULL,
	.usexde = False,
	.replace = False,
	.timeout = 15,
	.clientId = NULL,
	.saveFile = NULL,
	.dmrc = NULL,
	.vendor = NULL,
	.prefix = NULL,
	.backdrop = NULL,
	.desktops = NULL,
	.file = NULL,
	.source = BackgroundSourceSplash,
	.xsession = True,
	.setbg = False,
	.transparent = False,
	.width = -1,
	.height = -1,
	.xposition = 0.5,
	.yposition = 0.5,
	.setstyle = True,
	.filename = False,
	.protect = 5,
	.tray = False,
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	.authfile = NULL,
	.autologin = False,
	.permitlogin = True,
	.remotelogin = True,
#endif
	.mkdirs = False,
	.wmname = NULL,
	.setup = NULL,
	.startwm = NULL,
	.pause = 0,
	.wait = False,
};

typedef struct {
	GdkColor *foreground;
	GdkColor *background;
	PangoFontDescription *face;
	char *greeting;
	char *unsecureGreeting;
	PangoFontDescription *greetFace;
	GdkColor *greetColor;
	char *namePrompt;
	char *passwdPrompt;
	PangoFontDescription *promptFace;
	GdkColor *promptColor;
	PangoFontDescription *inputFace;
	GdkColor *inputColor;
	char *changePasswdMessage;
	char *fail;
	PangoFontDescription *failFace;
	GdkColor *failColor;
	unsigned int failTimeout;
	char *logoFileName;
	unsigned int logoPadding;
	Bool useShape;
	GdkColor *hiColor;
	GdkColor *shdColor;
	unsigned int frameWidth;
	unsigned int innerFrameWidth;
	unsigned int sepWidth;
	Bool allowRootLogin;
	Bool allowNullPasswd;
	Bool echoPasswd;
	char *echoPasswdChar;
	unsigned int borderWidth;
	Bool autoLock;
	Bool systemLock;
	char *authDir;
	char **exportList;
	Bool grabServer;
	int grabTimeout;
	Bool authorize;
	Bool authComplain;
	char **authName;
	char *authFile;
	char *setup;
	char *startup;
	char *reset;
	char *session;
	char *userPath;
	char *systemPath;
	char *systemShell;
	char *failsafeClient;
	char *userAuthDir;
	char *chooser;
	char *greeter;
} Resources;

Resources resources  = {
	.foreground = NULL,
	.background = NULL,
	.face = NULL,
	.greeting = NULL,
	.unsecureGreeting = NULL,
	.greetFace = NULL,
	.greetColor = NULL,
	.namePrompt = NULL,
	.passwdPrompt = NULL,
	.promptFace = NULL,
	.promptColor = NULL,
	.inputFace = NULL,
	.inputColor = NULL,
	.changePasswdMessage = NULL,
	.fail = NULL,
	.failFace = NULL,
	.failColor = NULL,
	.failTimeout = 0,
	.logoFileName = NULL,
	.logoPadding = 0,
	.useShape = False,
	.hiColor = NULL,
	.shdColor = NULL,
	.frameWidth = 0,
	.innerFrameWidth = 0,
	.sepWidth = 0,
	.allowRootLogin = False,
	.allowNullPasswd = False,
	.echoPasswd = False,
	.echoPasswdChar = NULL,
	.borderWidth = 0,
	.autoLock = True,
	.systemLock = True,
	.authDir = NULL,
	.exportList = NULL,
	.authFile = NULL,
	.grabServer = False,
	.grabTimeout = 5,
	.authorize = True,
	.authComplain = True,
	.authName = NULL,
	.setup = NULL,
	.startup = NULL,
	.reset = NULL,
	.session = NULL,
	.userPath = NULL,
	.systemPath = NULL,
	.systemShell = NULL,
	.failsafeClient = NULL,
	.userAuthDir = NULL,
	.chooser = NULL,
	.greeter = NULL,
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
		strncpy(buf, "/etc/xdg/xde", PATH_MAX);
		if (options.vendor) {
			strncat(buf, ":/etc/xdg/", PATH_MAX);
			strncat(buf, options.vendor, PATH_MAX);
		}
		if (envir.XDG_CONFIG_DIRS) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, envir.XDG_CONFIG_DIRS, PATH_MAX);
		}
		free(envir.XDG_CONFIG_DIRS);
		envir.XDG_CONFIG_DIRS = strdup(buf);
		strncpy(buf, "/usr/share/xde", PATH_MAX);
		if (options.vendor) {
			strncat(buf, ":/usr/share/", PATH_MAX);
			strncat(buf, options.vendor, PATH_MAX);
		}
		if (envir.XDG_CONFIG_DIRS) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, envir.XDG_CONFIG_DIRS, PATH_MAX);
		}
		free(envir.XDG_DATA_DIRS);
		envir.XDG_DATA_DIRS = strdup(buf);
	} else {
		strncpy(buf, "/etc/xdg/xde", PATH_MAX);
		if (options.vendor) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, options.vendor, PATH_MAX);
		}
		if (envir.XDG_CONFIG_DIRS) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, envir.XDG_CONFIG_DIRS, PATH_MAX);
		}
		free(envir.XDG_CONFIG_DIRS);
		envir.XDG_CONFIG_DIRS = strdup(buf);
		strncpy(buf, "/usr/share/xde", PATH_MAX);
		if (options.vendor) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, options.vendor, PATH_MAX);
		}
		if (envir.XDG_CONFIG_DIRS) {
			strncat(buf, ":", PATH_MAX);
			strncat(buf, envir.XDG_CONFIG_DIRS, PATH_MAX);
		}
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

	GtkWidget *img = gtk_image_new_from_file(options.backdrop);
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

/** @brief get system data directories
  *
  * Note that, unlike some other tools, there is no home directory at this point
  * so just search the system XDG data directories for things, but treat the XDM
  * home as /usr/lib/X11/xdm.
  */
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

void
do_session(int argc, char *argv[])
{
}

void
run_program(int argc, char *argv[])
{
#if   defined DO_SESSION
	do_session(argc, argv);
#elif defined DO_CHOOSER
	do_chooser(argc, argv);
#elif defined DO_AUTOSTART
	do_autostart(argc, argv);
#elif defined DO_STARTWM
	do_startwm(argc, argv);
#else
#   error Undefined program type.
#endif
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
Copyright (c) 2010-2017  Monavacon Limited <http://www.monavacon.com/>\n\
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
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017  Monavacon Limited.\n\
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
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [options] [--] [SESSION]\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
", argv[0]);
}

const char *
show_side(LogoSide side)
{
	switch (side) {
	case LogoSideLeft:
		return ("left");
	case LogoSideTop:
		return ("top");
	case LogoSideRight:
		return ("right");
	case LogoSideBottom:
		return ("bottom");
	}
	return ("unknown");
}

const char *
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
/* *INDENT-OFF* */
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] [--] [SESSION]\n\
    %1$s [options] {-h|--help}\n\
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
/* *INDENT-ON* */
}

const char *
get_nc_resource(XrmDatabase xrdb, const char *res_name, const char *res_class,
		const char *resource)
{
	char *type;
	static char name[64];
	static char clas[64];
	XrmValue value = { 0, NULL };

	snprintf(name, sizeof(name), "%s.%s", res_name, resource);
	snprintf(clas, sizeof(clas), "%s.%s", res_class, resource);
	if (XrmGetResource(xrdb, name, clas, &type, &value)) {
		if (value.addr && *(char *) value.addr) {
			DPRINTF("%s:\t\t%s\n", clas, value.addr);
			return (const char *) value.addr;
		} else
			DPRINTF("%s:\t\t%s\n", clas, value.addr);
	} else
		DPRINTF("%s:\t\t%s\n", clas, "ERROR!");
	return (NULL);
}

const char *
get_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, RESNAME, RESCLAS, resource)))
		value = dflt;
	return (value);
}

const char *
get_dm_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, "DisplayManager", "DisplayManager", resource)))
		value = dflt;
	return (value);
}

const char *
get_dm_dpy_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;
	static char nc[64], *p;

	snprintf(nc, sizeof(nc), "DisplayManager.%s", options.display);
	for (p = nc + 15; *p; p++)
		if (*p == ':' || *p == '.')
			*p = '_';
	if (!(value = get_nc_resource(xrdb, nc, nc, resource)))
		value = dflt;
	return (value);
}

const char *
get_xlogin_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, "xlogin.Login", "Xlogin.Login", resource)))
		value = dflt;
	return (value);
}

const char *
get_any_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_resource(xrdb, resource, NULL)))
		if (!(value = get_xlogin_resource(xrdb, resource, NULL)))
			value = dflt;
	return (value);
}

const char *
get_chooser_resource(XrmDatabase xrdb, const char *resource, const char *dflt)
{
	const char *value;

	if (!(value = get_nc_resource(xrdb, "chooser", "Chooser", resource)))
		value = dflt;
	return (value);
}

gboolean
getXrmColor(const char *val, GdkColor **color)
{
	GdkColor c, *p;

	if (gdk_color_parse(val, &c) && (p = calloc(1, sizeof(*p)))) {
		*p = c;
		free(*color);
		*color = p;
		return TRUE;
	}
	EPRINTF("could not parse color '%s'\n", val);
	return FALSE;
}

gboolean
getXrmFont(const char *val, PangoFontDescription **face)
{
	FcPattern *pattern;
	PangoFontDescription *font;

	if ((pattern = FcNameParse((FcChar8 *)val))) {
		if ((font = pango_fc_font_description_from_pattern(pattern, TRUE))) {
			pango_font_description_free(*face);
			*face = font;
			DPRINTF("Font description is: %s\n",
					pango_font_description_to_string(font));
			return TRUE;
		}
		FcPatternDestroy(pattern);
	}
	EPRINTF("could not parse font descriptikon '%s'\n", val);
	return FALSE;
}

gboolean
getXrmInt(const char *val, int *integer)
{
	*integer = strtol(val, NULL, 0);
	return TRUE;
}

gboolean
getXrmUint(const char *val, unsigned int *integer)
{
	*integer = strtoul(val, NULL, 0);
	return TRUE;
}

gboolean
getXrmDouble(const char *val, double *floating)
{
	const struct lconv *lc = localeconv();
	char radix, *copy = strdup(val);

	if ((radix = lc->decimal_point[0]) != '.' && strchr(copy, '.'))
		*strchr(copy, '.') = radix;

	*floating = strtod(copy, NULL);
	DPRINTF("Got decimal value %s, translates to %f\n", val, *floating);
	free(copy);
	return TRUE;
}

gboolean
getXrmBool(const char *val, Bool *boolean)
{
	if (!strncasecmp(val, "true", strlen(val))) {
		*boolean = True;
		return TRUE;
	}
	if (!strncasecmp(val, "false", strlen(val))) {
		*boolean = False;
		return TRUE;
	}
	EPRINTF("could not parse boolean'%s'\n", val);
	return FALSE;
}

gboolean
getXrmString(const char *val, char **string)
{
	char *tmp;

	if ((tmp = strdup(val))) {
		free(*string);
		*string = tmp;
		return TRUE;
	}
	return FALSE;
}

gboolean
getXrmStringList(const char *val, char ***list)
{
	gchar **tmp;

	if ((tmp = g_strsplit(val, " ", -1))) {
		g_strfreev(*list);
		*list = tmp;
		return TRUE;
	}
	return FALSE;
}

void
get_resources(int argc, char *argv[])
{
	Display *dpy;
	XrmDatabase rdb;
	const char *val;
	XTextProperty xtp;
	Window root;
	Atom atom;

	DPRINT();
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("could not open display %s\n", getenv("DISPLAY"));
		exit(EXIT_FAILURE);
	}
	root = DefaultRootWindow(dpy);
	if (!(atom = XInternAtom(dpy, "RESOURCE_MANAGER", True))) {
		XCloseDisplay(dpy);
		DPRINTF("no resource manager database allocated\n");
		return;
	}
	if (!XGetTextProperty(dpy, root, &xtp, atom) || !xtp.value) {
		XCloseDisplay(dpy);
		EPRINTF("could not retrieve RESOURCE_MANAGER property\n");
		return;
	}
	XrmInitialize();
	// DPRINTF("RESOURCE_MANAGER = %s\n", xtp.value);
	rdb = XrmGetStringDatabase((char *) xtp.value);
	XFree(xtp.value);
	if (!rdb) {
		DPRINTF("no resource manager database allocated\n");
		XCloseDisplay(dpy);
		return;
	}
	DPRINTF("combining database from %s\n", APPDFLT);
	XrmCombineFileDatabase(APPDFLT, &rdb, False);
	if ((val = get_resource(rdb, "debug", "0"))) {
		getXrmInt(val, &options.debug);
	}
	if ((val = get_xlogin_resource(rdb, "width", NULL))) {
		if (strchr(val, '%')) {
			char *endptr = NULL;
			double width = strtod(val, &endptr);

			DPRINTF("Got decimal value %s, translates to %f\n", val, width);
			if (endptr != val && *endptr == '%' && width > 0) {
				options.width =
				    (int) ((width / 100.0) * DisplayWidth(dpy, 0));
				if (options.width < 0.20 * DisplayWidth(dpy, 0))
					options.width = -1;
			}
		} else {
			options.width = strtoul(val, NULL, 0);
			if (options.width <= 0)
				options.width = -1;
		}
	}
	if ((val = get_xlogin_resource(rdb, "height", NULL))) {
		if (strchr(val, '%')) {
			char *endptr = NULL;
			double height = strtod(val, &endptr);

			DPRINTF("Got decimal value %s, translates to %f\n", val, height);
			if (endptr != val && *endptr == '%' && height > 0) {
				options.height =
				    (int) ((height / 100.0) * DisplayHeight(dpy, 0));
				if (options.height < 0.20 * DisplayHeight(dpy, 0))
					options.height = -1;
			}
		} else {
			options.height = strtoul(val, NULL, 0);
			if (options.height <= 0)
				options.height = -1;
		}
	}
	if ((val = get_xlogin_resource(rdb, "x", NULL))) {
		options.xposition =
		    (double) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
		if (options.xposition < 0)
			options.xposition = 0;
		if (options.xposition > DisplayWidth(dpy, 0))
			options.xposition = 1.0;
	}
	if ((val = get_xlogin_resource(rdb, "y", NULL))) {
		options.yposition =
		    (double) strtoul(val, NULL, 0) / DisplayWidth(dpy, 0);
		if (options.yposition < 0)
			options.yposition = 0;
		if (options.yposition > DisplayWidth(dpy, 0))
			options.yposition = 1.0;
	}
	// xlogin.foreground:		grey20
	if ((val = get_xlogin_resource(rdb, "foreground", NULL))) {
		getXrmColor(val, &resources.foreground);
	}
	// xlogin.background:		LightSteelBlue3
	if ((val = get_xlogin_resource(rdb, "background", NULL))) {
		getXrmColor(val, &resources.background);
	}
	// xlogin.face:			Sans-12:bold
	// xlogin.font:
	if ((val = get_any_resource(rdb, "face", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.face);
	}
#ifndef DO_LOGOUT
	// xlogin.greeting:		Welcome to CLIENTHOST
	if ((val = get_xlogin_resource(rdb, "greeting", NULL))) {
		getXrmString(val, &resources.greeting);
		getXrmString(val, &options.welcome);
	}
#endif
	// xlogin.unsecureGreeting:	This is an unsecure session
	if ((val = get_xlogin_resource(rdb, "unsecureGreeting", NULL))) {
		getXrmString(val, &resources.unsecureGreeting);
	}
	// xlogin.greetFace:		Sans-12:bold
	// xlogin.greetFont:
	if ((val = get_any_resource(rdb, "greetFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.greetFace);
	}
	// xlogin.greetColor:		grey20
	if ((val = get_any_resource(rdb, "greetColor", "grey20"))) {
		getXrmColor(val, &resources.greetColor);
	}
	// xlogin.namePrompt:		Username:
	if ((val = get_xlogin_resource(rdb, "namePrompt", "Username:  "))) {
		getXrmString(val, &resources.namePrompt);
	}
	// xlogin.passwdPrompt:		Password:
	if ((val = get_xlogin_resource(rdb, "passwdPrompt", "Password:  "))) {
		getXrmString(val, &resources.passwdPrompt);
	}
	// xlogin.promptFace:		Sans-12:bold
	// xlogin.promptFont:
	if ((val = get_any_resource(rdb, "promptFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.promptFace);
	}
	// xlogin.promptColor:		grey20
	if ((val = get_any_resource(rdb, "promptColor", "grey20"))) {
		getXrmColor(val, &resources.promptColor);
	}
	// xlogin.inputFace:		Sans-12:bold
	// xlogin.inputFont:
	if ((val = get_any_resource(rdb, "inputFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.inputFace);
	}
	// xlogin.inputColor:		grey20
	if ((val = get_any_resource(rdb, "inputColor", "grey20"))) {
		getXrmColor(val, &resources.inputColor);
	}
	// xlogin.changePasswdMessage:	Password Change Required
	if ((val = get_xlogin_resource(rdb, "changePasswdMessage", "Password Change Required"))) {
		getXrmString(val, &resources.changePasswdMessage);
	}
	// xlogin.fail:			Login incorrect!
	if ((val = get_xlogin_resource(rdb, "fail", "Login incorrect!"))) {
		getXrmString(val, &resources.fail);
	}
	// xlogin.failFace:		Sans-12:bold
	// xlogin.failFont:
	if ((val = get_any_resource(rdb, "failFace", "Sans:size=12:bold"))) {
		getXrmFont(val, &resources.failFace);
	}
	// xlogin.failColor:		red
	if ((val = get_any_resource(rdb, "failColor", "red"))) {
		getXrmColor(val, &resources.failColor);
	}
	// xlogin.failTimeout:		10
	if ((val = get_xlogin_resource(rdb, "failTimeout", "10"))) {
		getXrmUint(val, &resources.failTimeout);
	}
	// xlogin.logoFileName:		/etc/X11/xdm/xde/banner.png
	if ((val = get_xlogin_resource(rdb, "logoFileName", "/etc/X11/xdm/xde/banner.png"))) {
		getXrmString(val, &resources.logoFileName);
	}
	// xlogin.logoPadding:		8
	if ((val = get_xlogin_resource(rdb, "logoPadding", "8"))) {
		getXrmUint(val, &resources.logoPadding);
	}
	// xlogin.useShape:		true
	if ((val = get_xlogin_resource(rdb, "useShape", "true"))) {
		getXrmBool(val, &resources.useShape);
	}
	// xlogin.hiColor:		grey80
	if ((val = get_xlogin_resource(rdb, "hiColor", "grey80"))) {
		getXrmColor(val, &resources.hiColor);
	}
	// xlogin.shdColor:		grey20
	if ((val = get_xlogin_resource(rdb, "shdColor", "grey20"))) {
		getXrmColor(val, &resources.shdColor);
	}
	// xlogin.frameWidth:		2
	if ((val = get_xlogin_resource(rdb, "frameWidth", "2"))) {
		getXrmUint(val, &resources.frameWidth);
	}
	// xlogin.innerFrameWidth:	2
	if ((val = get_xlogin_resource(rdb, "innerFrameWidth", "2"))) {
		getXrmUint(val, &resources.innerFrameWidth);
	}
	// xlogin.sepWidth:		2
	if ((val = get_xlogin_resource(rdb, "sepWidth", "2"))) {
		getXrmUint(val, &resources.sepWidth);
	}
	// xlogin.allowRootLogin:	true
	if ((val = get_xlogin_resource(rdb, "allowRootLogin", "true"))) {
		getXrmBool(val, &resources.allowRootLogin);
	}
	// xlogin.allowNullPasswd:	false
	if ((val = get_xlogin_resource(rdb, "allowNullPasswd", "false"))) {
		getXrmBool(val, &resources.allowNullPasswd);
	}
	// xlogin.echoPasswd:		true
	if ((val = get_xlogin_resource(rdb, "echoPasswd", "true"))) {
		getXrmBool(val, &resources.echoPasswd);
	}
	// xlogin.echoPasswdChar:	*
	if ((val = get_xlogin_resource(rdb, "echoPasswdChar", "*"))) {
		getXrmString(val, &resources.echoPasswdChar);
	}
	// xlogin.borderWidth:		3
	if ((val = get_xlogin_resource(rdb, "borderWidth", "3"))) {
		getXrmUint(val, &resources.borderWidth);
	}
	if ((val = get_xlogin_resource(rdb, "autoLock", "true"))) {
		getXrmBool(val, &resources.autoLock);
	}
	if ((val = get_xlogin_resource(rdb, "systemLock", "true"))) {
		getXrmBool(val, &resources.systemLock);
	}

	// xlogin.login.translations

	// Chooser.geometry:		700x500
	// Chooser.allowShellResize:	false
	// Chooser.viewport.forceBars:	true
	// Chooser.label.font:		*-new-century-schoolbook-bold-i-normal-*-240-*
	// Chooser.label.label:		XDMCP Host Menu from CLIENTHOST
	// Chooser.list.font:		*-*-medium-r-normal-*-*-230-*-*-c-*-iso8859-1
	// Chooser.command.font:	*-new-century-schoolbook-bold-r-normal-*-180-*

	if ((val = get_resource(rdb, "Chooser.x", NULL))) {
		getXrmDouble(val, &options.xposition);
	}
	if ((val = get_resource(rdb, "Chooser.y", NULL))) {
		getXrmDouble(val, &options.yposition);
	}
	if ((val = get_resource(rdb, "banner", NULL))) {
		getXrmString(val, &options.banner);
	}
	if ((val = get_resource(rdb, "splash", NULL))) {
		getXrmString(val, &options.backdrop);
	}
#if defined DO_XCHOOSER || defined DO_XLOGIN || defined(DO_GREETER)
	if ((val = get_resource(rdb, "welcome", NULL))) {
		getXrmString(val, &options.welcome);
	}
#endif
	if ((val = get_resource(rdb, "charset", NULL))) {
		getXrmString(val, &options.charset);
	}
	if ((val = get_resource(rdb, "language", NULL))) {
		getXrmString(val, &options.language);
	}
	if ((val = get_resource(rdb, "theme.icon", NULL))) {
		getXrmString(val, &options.icon_theme);
	}
	if ((val = get_resource(rdb, "theme.name", NULL))) {
		getXrmString(val, &options.gtk2_theme);
	}
	if ((val = get_resource(rdb, "theme.cursor", NULL))) {
		getXrmString(val, &options.curs_theme);
	}
	if ((val = get_resource(rdb, "theme.xde", NULL))) {
		getXrmBool(val, &options.usexde);
	}
	if ((val = get_resource(rdb, "side", NULL))) {
		if (!strncasecmp(val, "left", strlen(val)))
			options.side = LogoSideLeft;
		else if (!strncasecmp(val, "top", strlen(val)))
			options.side = LogoSideTop;
		else if (!strncasecmp(val, "right", strlen(val)))
			options.side = LogoSideRight;
		else if (!strncasecmp(val, "bottom", strlen(val)))
			options.side = LogoSideBottom;
		else
			EPRINTF("invalid value for XDE-XChooser*side: %s\n", val);
	}
	if (getuid() == 0) {
		if ((val = get_resource(rdb, "user.default", NULL))) {
			getXrmString(val, &options.username);
		}
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
		if ((val = get_resource(rdb, "autologin", NULL))) {
			getXrmBool(val, &options.autologin);
		}
#endif
	}
	if ((val = get_resource(rdb, "vendor", NULL))) {
		getXrmString(val, &options.vendor);
	}
	if ((val = get_resource(rdb, "prefix", NULL))) {
		getXrmString(val, &options.prefix);
	}
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	if ((val = get_resource(rdb, "login.permit", NULL))) {
		getXrmBool(val, &options.permitlogin);
	}
	if ((val = get_resource(rdb, "login.remote", NULL))) {
		getXrmBool(val, &options.remotelogin);
	}
#endif
	if ((val = get_resource(rdb, "xsession.chooser", NULL))) {
		getXrmBool(val, &options.xsession);
	}
	if ((val = get_resource(rdb, "xsession.execute", NULL))) {
		getXrmBool(val, &options.launch);
	}
	if ((val = get_resource(rdb, "xsession.default", NULL))) {
		getXrmString(val, &options.choice);
	}
	if ((val = get_resource(rdb, "setbg", NULL))) {
		getXrmBool(val, &options.setbg);
	}
	if ((val = get_resource(rdb, "transparent", NULL))) {
		getXrmBool(val, &options.transparent);
	}
	// DisplayManager.servers:		:0 local /usr/bin/X11/X :0
	// DisplayManager.requestPort:		177
	// DisplayManager.debugLevel:		0
	// DisplayManager.errorLogFile:		
	// DisplayManager.daemonMode:		true
	// DisplayManager.pidFile:		
	// DisplayManager.lockPidFile:		true
	// DisplayManager.authDir:		/usr/lib/X11/xdm
	if ((val = get_dm_resource(rdb, "authDir", "/usr/lib/X11/xdm"))) {
		getXrmString(val, &resources.authDir);
	}
	// DisplayManager.autoRescan:		true
	// DisplayManager.removeDomainname:	true
	// DisplayManager.keyFile:		
	// DisplayManager.accessFile:		
	// DisplayManager.exportList:		
	if ((val = get_dm_resource(rdb, "exportList", ""))) {
		getXrmStringList(val, &resources.exportList);
	}
	// DisplayManager.randomFile:		/dev/mem
	// DisplayManager.prngSocket:		/tmp/entropy
	// DisplayManager.prngPort:		0
	// DisplayManager.randomDevice:		DEV_RANDOM
	// DisplayManager.greeterLib:		/usr/lib/X11/xdm/libXdmGreet.so
	// DisplayManager.choiceTimeout:	15
	// DisplayManager.sourceAddress:	false
	// DisplayManager.willing:		

	// DisplayManager.*.serverAttempts:	1
	// DisplayManager.*.openDelay:		15
	// DisplayManager.*.openRepeat:		5
	// DisplayManager.*.openTimeout:	120
	// DisplayManager.*.startAttempts:	4
	// DisplayManager.*.reservAttempts:	2
	// DisplayManager.*.pingInterval:	5
	// DisplayManager.*.pingTimeout:	5
	// DisplayManager.*.terminateServer:	false
	// DisplayManager.*.grabServer:		false
	if ((val = get_dm_dpy_resource(rdb, "grabServer", "false"))) {
		getXrmBool(val, &resources.grabServer);
	}
	// DisplayManager.*.grabTimeout:	3
	if ((val = get_dm_dpy_resource(rdb, "grabTimeout", "3"))) {
		getXrmInt(val, &resources.grabTimeout);
	}
	// DisplayManager.*.resetSignal:	1
	// DisplayManager.*.termSignal:		15
	// DisplayManager.*.resetForAuth:	false
	// DisplayManager.*.authorize:		true
	if ((val = get_dm_dpy_resource(rdb, "authorize", "true"))) {
		getXrmBool(val, &resources.authorize);
	}
	// DisplayManager.*.authComplain:	true
	if ((val = get_dm_dpy_resource(rdb, "authComplain", "true"))) {
		getXrmBool(val, &resources.authComplain);
	}
	// DisplayManager.*.authName:		XDM-AUTHORIZATION-1 MIT-MAGIC-COOKIE-1
	if ((val = get_dm_dpy_resource(rdb, "authName", "XDM-AUTHORIZATION-1 MIT-MAGIC-COOKIE-1"))) {
		getXrmStringList(val, &resources.authName);
	}
	// DisplayManager.*.authFile:		
	if ((val = get_dm_dpy_resource(rdb, "authFile", NULL))) {
		getXrmString(val, &resources.authFile);
	}
	// DisplayManager.*.resources:		
	// DisplayManager.*.xrdb:		/usr/bin/X11/xrdb
	// DisplayManager.*.setup:		
	if ((val = get_dm_dpy_resource(rdb, "setup", NULL))) {
		getXrmString(val, &resources.setup);
	}
	// DisplayManager.*.startup:		
	if ((val = get_dm_dpy_resource(rdb, "startup", NULL))) {
		getXrmString(val, &resources.startup);
	}
	// DisplayManager.*.reset:		
	if ((val = get_dm_dpy_resource(rdb, "reset", NULL))) {
		getXrmString(val, &resources.reset);
	}
	// DisplayManager.*.session:		/usr/bin/X11/xterm -ls
	if ((val = get_dm_dpy_resource(rdb, "session", NULL))) {
		getXrmString(val, &resources.session);
	}
	// DisplayManager.*.userPath:		:/bin:/usr/bin:/usr/bin/X11:/usr/ucb
	if ((val = get_dm_dpy_resource(rdb, "userPath", NULL))) {
		getXrmString(val, &resources.userPath);
	}
	// DisplayManager.*.systemPath:		/etc:/bin:/usr/bin:/usr/bin/X11:/usr/ucb
	if ((val = get_dm_dpy_resource(rdb, "systemPath", NULL))) {
		getXrmString(val, &resources.systemPath);
	}
	// DisplayManager.*.systemShell:	/bin/sh
	if ((val = get_dm_dpy_resource(rdb, "systemShell", NULL))) {
		getXrmString(val, &resources.systemShell);
	}
	// DisplayManager.*.failsafeClient:	/usr/bin/X11/xterm
	if ((val = get_dm_dpy_resource(rdb, "failsafeClient", NULL))) {
		getXrmString(val, &resources.failsafeClient);
	}
	// DisplayManager.*.userAuthDir:	/tmp
	if ((val = get_dm_dpy_resource(rdb, "userAuthDir", NULL))) {
		getXrmString(val, &resources.userAuthDir);
	}
	// DisplayManager.*.chooser:		/usr/lib/X11/xdm/chooser
	if ((val = get_dm_dpy_resource(rdb, "chooser", NULL))) {
		getXrmString(val, &resources.chooser);
	}
	// DisplayManager.*.greeter:		
	if ((val = get_dm_dpy_resource(rdb, "greeter", NULL))) {
		getXrmString(val, &resources.greeter);
	}

	XrmDestroyDatabase(rdb);
	XCloseDisplay(dpy);
}

void
set_default_debug(void)
{
	const char *env = getenv("XDE_DEBUG");

	if (env)
		options.debug = atoi(env);
}

void
set_default_display(void)
{
	const char *env = getenv("DISPLAY");

	if (env) {
		free(options.display);
		options.display = strdup(env);
	}
}

void
set_default_x11(void)
{
	const char *env;

	if ((env = getenv("XDG_VTNR"))) {
		free(options.vtnr);
		options.vtnr = strdup(env);
	}
	if ((env = getenv("XDG_SEAT"))) {
		free(options.seat);
		options.seat = strdup(env);
	}
	if (options.vtnr) {
		free(options.tty);
		if ((options.tty = calloc(16, sizeof(*options.tty))))
			snprintf(options.tty, 16, "tty%s", options.vtnr);
	}
}

#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
void
set_default_authfile(void)
{
	const char *env = XauFileName();

	if (env) {
		free(options.authfile);
		options.authfile = strdup(env);
	}
}
#endif

void
set_default_desktop(void)
{
	const char *env = getenv("XDG_CURRENT_DESKTOP");

	free(options.desktop);
	options.desktop = env ? strdup(env) : strdup("XDE");
}

void
set_default_vendor(void)
{
	char *p, *vendor, *prefix;
	int len;

	if ((vendor = getenv("XDG_VENDOR_ID"))) {
		free(defaults.vendor);
		defaults.vendor = strdup(vendor);
	}
	if ((prefix = getenv("XDG_MENU_PREFIX"))) {
		free(defaults.prefix);
		defaults.prefix = strdup(prefix);
		if (!vendor) {
			free(defaults.vendor);
			vendor = defaults.vendor = strdup(prefix);
			if ((p = strrchr(vendor, '-')) && !*(p + 1))
				*p = '\0';
		}
	} else if (vendor && *vendor) {
		free(defaults.prefix);
		len = strlen(vendor) + 1;
		prefix = defaults.prefix = calloc(len + 1, sizeof(*prefix));
		strncpy(prefix, vendor, len);
		strncat(prefix, "-", len);
	}
	if (!defaults.vendor)
		defaults.vendor = strdup("");
	if (!defaults.prefix)
		defaults.prefix = strdup("");
}

void
set_default_xdgdirs(int argc, char *argv[])
{
	static const char *confdir = "/etc/xdg/xde:/etc/xdg";
	static const char *datadir = "/usr/share/xde:/usr/local/share:/usr/share";
	char *here, *p, *q;
	char *conf, *data;
	int len;

	here = strdup(argv[0]);
	if (here[0] != '/') {
		char *cwd = calloc(PATH_MAX + 1, sizeof(*cwd));

		if (!getcwd(cwd, PATH_MAX)) {
			EPRINTF("%s: %s\n", "getcwd", strerror(errno));
			exit(EXIT_FAILURE);
		}
		strncat(cwd, "/", PATH_MAX);
		strncat(cwd, here, PATH_MAX);
		free(here);
		here = strdup(cwd);
		free(cwd);
	}
	while ((p = strstr(here, "/./")))
		memmove(p, p + 2, strlen(p + 2) + 1);
	while ((p = strstr(here, "/../"))) {
		for (q = p - 1; q > here && *q != '/'; q--) ;
		if (q > here || *q != '/')
			break;
		memmove(q, p + 3, strlen(p + 3) + 1);
	}
	if ((p = strrchr(here, '/')))
		*p = '\0';
	if ((p = strstr(here, "/src")) && !*(p + 4))
		*p = '\0';
	/* executed in place */
	if (strchr(here, '/') && strcmp(here, "/usr/bin")) {
		len = strlen(here) + strlen("/data/xdg/xde:")
		    + strlen(here) + strlen("/data/xdg:") + strlen(confdir);
		conf = calloc(len + 1, sizeof(*conf));
		strncpy(conf, here, len);
		strncat(conf, "/data/xdg/xde:", len);
		strncat(conf, here, len);
		strncat(conf, "/data/xdg:", len);
		strncat(conf, confdir, len);

		len = strlen(here) + strlen("/data/share/xde:")
		    + strlen(here) + strlen("/data/share:") + strlen(datadir);
		data = calloc(len + 1, sizeof(*data));
		strncpy(data, here, len);
		strncat(data, "/data/share/xde:", len);
		strncat(data, here, len);
		strncat(data, "/data/share:", len);
		strncat(data, datadir, len);
	} else {
		conf = strdup(confdir);
		data = strdup(datadir);
	}
	setenv("XDG_CONFIG_DIRS", conf, 1);
	setenv("XDG_DATA_DIRS", data, 1);
	DPRINTF("setting XDG_CONFIG_DIRS to '%s'\n", conf);
	DPRINTF("setting XDG_DATA_DIRS   to '%s'\n", data);
	free(conf);
	free(data);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	free(defaults.banner);
	defaults.banner = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		defaults.banner = NULL;
		return;
	}

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = defaults.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "banner", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					defaults.banner = strdup(file);
					break;
				}
			}
			if (defaults.banner)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
set_default_splash(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	free(defaults.backdrop);
	defaults.backdrop = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		defaults.backdrop = NULL;
		return;
	}

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = defaults.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "splash", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					defaults.backdrop = strdup(file);
					break;
				}
			}
			if (defaults.backdrop)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

#ifdef DO_LOGOUT
void
set_default_welcome(void)
{
	char *session = NULL, *welcome, *p;
	const char *s;
	int i, len;

	welcome = calloc(PATH_MAX, sizeof(*welcome));

	if ((s = getenv("XDG_CURRENT_DESKTOP")) && *s) {
		session = strdup(s);
		while ((p = strchr(session, ';')))
			*p = ':';
	} else if ((s = defaults.vendor) && *s) {
		session = strdup(s);
	} else if ((s = defaults.prefix) && *s) {
		session = strdup(s);
		p = session + strlen(session) - 1;
		if (*p == '-')
			*p = '\0';
	} else {
		session = strdup("XDE");
	}
	len = strlen(session);
	for (i = 0, p = session; i < len; i++, p++)
		*p = toupper(*p);
	snprintf(welcome, PATH_MAX - 1, "Logout of <b>%s</b> session?", session);
	defaults.welcome = strdup(welcome);
	free(session);
	free(welcome);
}
#else				/* DO_LOGOUT */
void
set_default_welcome(void)
{
	char hostname[64] = { 0, };
	char *buf;
	int len;
	static char *format = "Welcome to %s!";

	free(defaults.welcome);
	gethostname(hostname, sizeof(hostname));
	len = strlen(format) + strnlen(hostname, sizeof(hostname)) + 1;
	buf = defaults.welcome = calloc(len, sizeof(*buf));
	snprintf(buf, len, format, hostname);
}
#endif				/* DO_LOGOUT */

void
set_default_language(void)
{
	char *p, *a;

	if ((defaults.language = setlocale(LC_ALL, ""))) {
		defaults.language = strdup(defaults.language);
		a = strchrnul(defaults.language, '@');
		if ((p = strchr(defaults.language, '.')))
			strcpy(p, a);
	}
	defaults.charset = strdup(nl_langinfo(CODESET));
}

#ifdef DO_XCHOOSER
void
set_default_address(void)
{
	XdmcpReallocARRAY8(&defaults.clientAddress, sizeof(struct in6_addr));
	*(struct in6_addr *) defaults.clientAddress.data = (struct in6_addr) IN6ADDR_LOOPBACK_INIT;
	defaults.connectionType = FamilyInternet6;
	defaults.clientScope = SocketScopeLoopback;
	defaults.isLocal = True;
}
#endif				/* DO_XCHOOSER */

#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
void
set_default_session(void)
{
	char **xdg_dirs, **dirs, *file, *line, *p;
	int i, n = 0;
	static const char *session = "/xde/default";
	static const char *current = "/xde/current";
	static const char *dmrc = "/.dmrc";
	const char *home = getenv("HOME") ? : ".";

	free(defaults.session);
	defaults.session = NULL;
	free(defaults.current);
	defaults.current = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	strncpy(file, home, PATH_MAX);
	strncat(file, dmrc, PATH_MAX);

	if (!defaults.dmrc)
		defaults.dmrc = g_key_file_new();
	if (defaults.dmrc) {
		if (g_key_file_load_from_file(defaults.dmrc, file,
					      G_KEY_FILE_KEEP_COMMENTS |
					      G_KEY_FILE_KEEP_TRANSLATIONS, NULL)) {
			gchar *sess;

			if ((sess = g_key_file_get_string(defaults.dmrc,
							  "Desktop", "Session", NULL))) {
				free(defaults.session);
				defaults.session = strdup(sess);
				free(defaults.current);
				defaults.current = strdup(sess);
				g_free(sess);
				free(file);
				return;
			}
		}
	}

	if (!(xdg_dirs = get_config_dirs(&n)) || !n) {
		free(file);
		return;
	}

	line = calloc(BUFSIZ + 1, sizeof(*line));

	/* go through them forward */
	for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
		FILE *f;

		if (!defaults.session) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, session, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						defaults.session = strdup(line);
					}
					fclose(f);
				}
			}

		}
		if (!defaults.current) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, current, PATH_MAX);

			if (!access(file, R_OK)) {
				if ((f = fopen(file, "r"))) {
					if (fgets(line, BUFSIZ, f)) {
						if ((p = strchr(line, '\n')))
							*p = '\0';
						defaults.current = strdup(line);
					}
					fclose(f);
				}
			}
		}
	}
	free(line);
	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}
#endif

void
set_default_choice(void)
{
	free(defaults.choice);
	defaults.choice = strdup("default");
}

void
set_defaults(int argc, char *argv[])
{
	set_default_debug();
	set_default_display();
	set_default_x11();
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	set_default_authfile();
#endif
	set_default_desktop();
	set_default_vendor();
	set_default_xdgdirs(argc, argv);
	set_default_banner();
	set_default_splash();
	set_default_welcome();
	set_default_language();
#ifdef DO_XCHOOSER
	set_default_address();
#endif				/* DO_XCHOOSER */
#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
	set_default_session();
#endif
	set_default_choice();
}

void
get_default_display(void)
{
	if (options.display)
		setenv("DISPLAY", options.display, 1);
}

void
get_default_x11(void)
{
	Display *dpy;
	Window root;
	Atom property, actual;
	int format;
	unsigned long nitems, after;
	long *data = NULL;

	if (!(dpy = XOpenDisplay(0))) {
		EPRINTF("cannot open display %s\n", options.display);
		exit(EXIT_FAILURE);
	}
	root = RootWindow(dpy, 0);
	format = 0;
	nitems = after = 0;
	if ((property = XInternAtom(dpy, "XFree86_VT", True)) &&
	    XGetWindowProperty(dpy, root, property, 0, 1, False, XA_INTEGER, &actual,
			       &format, &nitems, &after, (unsigned char **) &data)
	    == Success && format == 32 && nitems && data) {
		free(options.vtnr);
		if ((options.vtnr = calloc(16, sizeof(*options.vtnr))))
			snprintf(options.vtnr, 16, "%lu", *(unsigned long *) data);
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	format = 0;
	nitems = after = 0;
	if ((property = XInternAtom(dpy, "Xorg_Seat", True)) &&
	    XGetWindowProperty(dpy, root, property, 0, 16, False, XA_STRING, &actual,
			       &format, &nitems, &after, (unsigned char **) &data)
	    == Success && format == 8 && nitems && data) {
		free(options.seat);
		if ((options.seat = calloc(nitems + 1, sizeof(*options.seat))))
			strncpy(options.seat, (char *) data, nitems);
	}
	if (data) {
		XFree(data);
		data = NULL;
	}
	if (options.vtnr) {
		if (!options.seat)
			options.seat = strdup("seat0");
		if ((options.tty = calloc(16, sizeof(options.tty))))
			snprintf(options.tty, 16, "tty%s", options.vtnr);
	}
	if (!options.tty)
		options.tty = strdup(options.display);
	XCloseDisplay(dpy);
}

#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
void
get_default_authfile(void)
{
	if (options.authfile)
		setenv("XAUTHORITY", options.authfile, 1);
}
#endif

void
get_default_vendor(void)
{
	if (!options.vendor) {
		options.vendor = defaults.vendor;
		options.prefix = defaults.prefix;
	} else if (*options.vendor) {
		int len = strlen(options.vendor) + 1;

		free(options.prefix);
		options.prefix = calloc(len + 1, sizeof(*options.prefix));
		strncpy(options.prefix, options.vendor, len);
		strncat(options.prefix, "-", len);
	} else {
		free(options.prefix);
		options.prefix = strdup("");
	}
	if (options.vendor && *options.vendor)
		setenv("XDG_VENDOR_ID", options.vendor, 1);
	else
		unsetenv("XDG_VENDOR_ID");
	if (options.prefix && *options.prefix)
		setenv("XDG_MENU_PREFIX", options.prefix, 1);
	else
		unsetenv("XDG_MENU_PREFIX");
}

void
get_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (options.banner)
		return;

	free(options.banner);
	options.banner = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		options.banner = defaults.banner;
		return;
	}

	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = options.prefix)) {
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
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);

	if (!options.banner)
		options.banner = defaults.banner;
}

void
get_default_splash(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (options.backdrop)
		return;

	free(options.backdrop);
	options.backdrop = NULL;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n) {
		options.backdrop = defaults.backdrop;
		return;
	}

	options.backdrop = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	if ((pfx = options.prefix)) {
		for (i = 0, dirs = &xdg_dirs[i]; i < n; i++, dirs++) {
			strncpy(file, *dirs, PATH_MAX);
			strncat(file, "/images/", PATH_MAX);
			strncat(file, pfx, PATH_MAX);
			strncat(file, "splash", PATH_MAX);
			suffix = file + strnlen(file, PATH_MAX);

			for (j = 0; j < sizeof(exts) / sizeof(exts[0]); j++) {
				strcpy(suffix, exts[j]);
				if (!access(file, R_OK)) {
					options.backdrop = strdup(file);
					break;
				}
			}
			if (options.backdrop)
				break;
		}
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);

	if (!options.backdrop)
		options.backdrop = defaults.backdrop;
}

void
get_default_welcome(void)
{
	if (!options.welcome) {
		free(options.welcome);
		options.welcome = defaults.welcome;
	}
}

void
get_default_language(void)
{
	if (!options.charset) {
		free(options.charset);
		options.charset = defaults.charset;
	}
	if (!options.language) {
		free(options.language);
		options.language = defaults.language;
	}
	if (strcmp(options.charset, defaults.charset) ||
	    strcmp(options.language, defaults.language)) {
		/* FIXME: actually set the language and charset */
	}
}

#ifdef DO_XCHOOSER
SocketScope
GetScope(ARRAY8Ptr clientAddress, CARD16 connectionType)
{
	switch (connectionType) {
	case FamilyLocal:
		break;
	case FamilyInternet:
	{
		in_addr_t addr = ntohl(*(in_addr_t *) clientAddress->data);

		if (IN_LOOPBACK(addr))
			return SocketScopeLoopback;
		if (IN_LINKLOCAL(addr))
			return SocketScopeLinklocal;
		if (IN_ORGLOCAL(addr))
			return SocketScopePrivate;
		return SocketScopeGlobal;
	}
	case FamilyInternet6:
	{
		struct in6_addr *addr = (typeof(addr)) clientAddress->data;

		if (IN6_IS_ADDR_LOOPBACK(addr))
			return SocketScopeLoopback;
		if (IN6_IS_ADDR_LINKLOCAL(addr))
			return SocketScopeLinklocal;
		if (IN6_IS_ADDR_SITELOCAL(addr))
			return SocketScopeSitelocal;
		if (IN6_IS_ADDR_V4MAPPED(addr) || IN6_IS_ADDR_V4COMPAT(addr)) {
			in_addr_t ipv4 = ntohl(((uint32_t *) addr)[3]);

			if (IN_LOOPBACK(ipv4))
				return SocketScopeLoopback;
			if (IN_LINKLOCAL(ipv4))
				return SocketScopeLinklocal;
			if (IN_ORGLOCAL(ipv4))
				return SocketScopePrivate;
			return SocketScopeGlobal;
		}
		return SocketScopeGlobal;
	}
	default:
		break;
	}
	return SocketScopeLoopback;
}

Bool
TestLocal(ARRAY8Ptr clientAddress, CARD16 connectionType)
{
	sa_family_t family;
	struct ifaddrs *ifa, *ifas = NULL;

	switch (connectionType) {
	case FamilyLocal:
		family = AF_UNIX;
		return True;
	case FamilyInternet:
		if (ntohl((*(in_addr_t *) clientAddress->data)) == INADDR_LOOPBACK)
			return True;
		family = AF_INET;
		break;
	case FamilyInternet6:
		if (IN6_IS_ADDR_LOOPBACK(clientAddress->data))
			return True;
		family = AF_INET6;
		break;
	default:
		family = AF_UNSPEC;
		return False;
	}
	if (getifaddrs(&ifas) == 0) {
		for (ifa = ifas; ifa; ifa = ifa->ifa_next) {
			struct sockaddr *ifa_addr;

			if (!(ifa_addr = ifa->ifa_addr)) {
				EPRINTF("interface %s has no address\n", ifa->ifa_name);
				continue;
			}
			if (ifa_addr->sa_family != family) {
				DPRINTF("interface %s has wrong family\n", ifa->ifa_name);
				continue;
			}
			switch (family) {
			case AF_INET:
			{
				struct sockaddr_in *sin = (typeof(sin)) ifa_addr;

				if (!memcmp(&sin->sin_addr, clientAddress->data, 4)) {
					DPRINTF("interface %s matches\n", ifa->ifa_name);
					freeifaddrs(ifas);
					return True;
				}

				break;
			}
			case AF_INET6:
			{
				struct sockaddr_in6 *sin6 = (typeof(sin6)) ifa_addr;

				if (!memcmp(&sin6->sin6_addr, clientAddress->data, 16)) {
					DPRINTF("interface %s matches\n", ifa->ifa_name);
					freeifaddrs(ifas);
					return True;
				}
				break;
			}
			}
		}
		freeifaddrs(ifas);
	}
	return False;
}

void
get_default_address(void)
{
	switch (options.clientAddress.length) {
	case 0:
		options.clientAddress = defaults.clientAddress;
		options.connectionType = defaults.connectionType;
		options.clientScope = defaults.clientScope;
		options.clientIface = defaults.clientIface;
		options.isLocal = defaults.isLocal;
		break;
	case 4:
	case 8:
		if (options.connectionType != FamilyInternet) {
			EPRINTF("Mismatch in connectionType %d != %d\n",
				FamilyInternet, options.connectionType);
			exit(EXIT_SYNTAXERR);
		}
		options.clientScope = GetScope(&options.clientAddress, options.connectionType);
		options.isLocal = TestLocal(&options.clientAddress, options.connectionType);
		switch (options.clientAddress.length) {
		case 4:
			options.clientIface = defaults.clientIface;
			break;
		case 8:
			memmove(&options.clientIface, options.clientAddress.data + 4, 4);
			break;
		}
		switch (options.clientScope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			if (!options.clientIface) {
				EPRINTF("link or site local address with no interface\n");
				exit(EXIT_SYNTAXERR);
			}
			break;
		default:
			break;
		}
		break;
	case 16:
	case 20:
		if (options.connectionType != FamilyInternet6) {
			EPRINTF("Mismatch in connectionType %d != %d\n",
				FamilyInternet, options.connectionType);
			exit(EXIT_SYNTAXERR);
		}
		options.clientScope = GetScope(&options.clientAddress, options.connectionType);
		options.isLocal = TestLocal(&options.clientAddress, options.connectionType);
		switch (options.clientAddress.length) {
		case 16:
			options.clientIface = defaults.clientIface;
			break;
		case 20:
			memmove(&options.clientIface, options.clientAddress.data + 16, 4);
			break;
		}
		switch (options.clientScope) {
		case SocketScopeLinklocal:
		case SocketScopeSitelocal:
			if (!options.clientIface) {
				EPRINTF("link or site local address with no interface\n");
				exit(EXIT_SYNTAXERR);
			}
			break;
		default:
			break;
		}
		break;
	default:
		EPRINTF("Invalid client address length %d\n", options.clientAddress.length);
		exit(EXIT_SYNTAXERR);
	}
}
#endif				/* DO_XCHOOSER */

void
get_default_session(void)
{
	if (options.dmrc)
		g_key_file_unref(options.dmrc);
	options.dmrc = defaults.dmrc;
	if (!options.session) {
		free(options.session);
		if (!(options.session = defaults.session))
			options.session = strdup("");
	}
	if (!options.current) {
		free(options.current);
		if (!(options.current = defaults.current))
			options.current = strdup("");
	}
}

void
get_default_choice(void)
{
	if (!options.choice) {
		free(options.choice);
		if (!(options.choice = defaults.choice))
			options.choice = strdup("default");
	}
}

void
get_default_username(void)
{
	struct passwd *pw;

	if (options.username)
		return;
	if (getuid() == 0)
		return;

	if (!(pw = getpwuid(getuid()))) {
		EPRINTF("cannot get users password entry\n");
		exit(EXIT_FAILURE);
	}
	free(options.username);
	options.username = strdup(pw->pw_name);
	endpwent();
}

void
get_default_file(void)
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
get_default_desktops(void)
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
get_defaults(int argc, char *argv[])
{
	get_default_display();
	get_default_x11();
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
	get_default_authfile();
#endif
	get_default_vendor();
	get_default_banner();
	get_default_splash();
	get_default_welcome();
	get_default_language();
#ifdef DO_XCHOOSER
	get_default_address();
#endif				/* DO_XCHOOSER */
	get_default_session();
	get_default_choice();
	get_default_username();
	get_default_file();
	get_default_desktops();
}

#ifdef DO_XCHOOSER
Bool
HexToARRAY8(ARRAY8 *array, char *hex)
{
	short len;
	CARD8 *o, b;
	char *p, c;

	len = strlen(hex);
	if (len & 0x01)
		return False;
	len >>= 1;
	XdmcpReallocARRAY8(array, len);
	for (p = hex, o = array->data; *p; p += 2, o++) {
		c = tolower(p[0]);
		if (!isxdigit(c))
			return False;
		b = ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		b <<= 4;
		c = tolower(p[1]);
		if (!isxdigit(c))
			return False;
		b += ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		*o = b;
	}
	return True;
}
#endif

int
main(int argc, char *argv[])
{
	CommandType command = CommandDefault;

	saveArgc = argc;
	saveArgv = argv;

	setlocale(LC_ALL, "");

	set_defaults(argc, argv);

	get_resources(argc, argv);

	while (1) {
		int c, val;
		char *endptr = NULL;

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
			DPRINTF("%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'd':	/* -d, --display DISPLAY */
			free(options.display);
			options.display = strndup(optarg, 256);
			break;
		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (optarg == NULL) {
				DPRINTF("%s: increasing debug verbosity\n", argv[0]);
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			DPRINTF("%s: setting debug verbosity to %d\n", argv[0], val);
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			if (optarg == NULL) {
				DPRINTF("%s: increasing output verbosity\n", argv[0]);
				options.output++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0)
				goto bad_option;
			if (endptr && *endptr)
				goto bad_option;
			DPRINTF("%s: setting output verbosity to %d\n", argv[0], val);
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
#ifndef DO_XCHOOSER
#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
	if (optind < argc) {
		free(options.choice);
		options.choice = strdup(argv[optind++]);
#endif
		if (optind < argc) {
			EPRINTF("%s: excess non-option arguments\n", argv[0]);
			goto bad_nonopt;
		}
#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
	}
#endif
#endif
	DPRINTF("%s: option index = %d\n", argv[0], optind);
	DPRINTF("%s: option count = %d\n", argv[0], argc);
	get_defaults(argc, argv);
	switch (command) {
	default:
	case CommandDefault:
#ifdef DO_XCHOOSER
		if (optind >= argc) {
			EPRINTF("%s: missing non-option argument\n", argv[0]);
			goto bad_nonopt;
		}
#endif
#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
		DPRINTF("%s: running program\n", argv[0]);
		run_program(argc, argv);
#else
		DPRINTF("%s: running default\n", argv[0]);
		do_run(argc - optind, &argv[optind]);
#endif
		break;
#ifdef DO_XLOCKING
	case CommandReplace:
		DPRINTF("%s: running replace\n", argv[0]);
		do_run(argc, argv);
		break;
	case CommandQuit:
		DPRINTF("%s: running quit\n", argv[0]);
		do_quit(argc, argv);
		break;
	case CommandLock:
		DPRINTF("%s: running lock\n", argv[0]);
		do_lock(argc, argv);
		break;
	case CommandUnlock:
		DPRINTF("%s: running unlock\n", argv[0]);
		do_unlock(argc, argv);
		break;
#endif				/* DO_XLOCKING */
	case CommandHelp:
		DPRINTF("%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF("%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF("%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
