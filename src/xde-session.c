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
#   define SELECTION_ATOM "_XDE_XCHOOSER_S%d"
#elif defined(DO_XLOCKING)
#   define LOGO_NAME "gnome-lockscreen"
#   define RESNAME "xde-xlock"
#   define RESCLAS "XDE-XLock"
#   define RESTITL "X11 Locker"
#   define SELECTION_ATOM "_XDE_XLOCK_S%d"
#elif defined(DO_CHOOSER)
#   define RESNAME "xde-chooser"
#   define RESCLAS "XDE-Chooser"
#   define RESTITL "XDE X11 Session Chooser"
#   define SELECTION_ATOM "_XDE_CHOOSER_S%d"
#elif defined(DO_LOGOUT)
#   define RESNAME "xde-logout"
#   define RESCLAS "XDE-Logout"
#   define RESTITL "XDE X11 Session Logout"
#   define SELECTION_ATOM "_XDE_LOGOUT_S%d"
#elif defined(DO_XLOGIN)
#   define RESNAME "xde-xlogin"
#   define RESCLAS "XDE-XLogin"
#   define RESTITL "XDMCP Greeter"
#   define SELECTION_ATOM "_XDE_XLOGIN_S%d"
#elif defined(DO_GREETER)
#   define RESNAME "xde-greeter"
#   define RESCLAS "XDE-Greeter"
#   define RESTITL "XDMCP Greeter"
#   define SELECTION_ATOM "_XDE_GREETER_S%d"
#elif defined(DO_AUTOSTART)
#   define RESNAME "xde-autostart"
#   define RESCLAS "XDE-AutoStart"
#   define RESTITL "XDE XDG Auto Start"
#   define SELECTION_ATOM "_XDE_AUTOSTART_S%d"
#elif defined(DO_SESSION)
#   define RESNAME "xde-session"
#   define RESCLAS "XDE-Session"
#   define RESTITL "XDE XDG Session"
#   define SELECTION_ATOM "_XDE_SESSION_S%d"
#elif defined(DO_STARTWM)
#   define RESNAME "xde-startwm"
#   define RESCLAS "XDE-StartWM"
#   define RESTITL "XDE Sindow Manager Startup"
#   define SELECTION_ATOM "_XDE_STARTWM_S%d"
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
	Bool wait;
	char **execute;
	int commands;
	Bool autostart;
	unsigned int pause;
	unsigned int guard;
	unsigned int delay;
	Bool foreground;
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
	.wait = False,
	.execute = NULL,
	.commands = 0,
	.autostart = True,
	.wait = True,
	.pause = 2,
	.splash = True,
	.guard = 200,
	.delay = 0,
	.foreground = False,
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

typedef enum {
	LoginStateInit,
	LoginStateUsername,
	LoginStatePassword,
	LoginStateReady,
} LoginState;

LoginState state = LoginStateInit;

#ifdef DO_XLOCKING
typedef enum {
	LockStateLocked,
	LockStateUnlocked,
	LockStateAborted,
} LockState;

LockState lock_state = LockStateLocked;
struct timeval lock_time = { 0, 0 };

typedef enum {
	LockCommandLock,
	LockCommandUnlock,
	LockCommandQuit,
} LockCommand;

#endif				/* DO_XLOCKING */

typedef enum {
	ChooseResultLogout,
	ChooseResultLaunch,
} ChooseResult;

ChooseResult choose_result;

#ifdef DO_LOGOUT
typedef enum {
	LOGOUT_ACTION_POWEROFF,		/* power off the computer */
	LOGOUT_ACTION_REBOOT,		/* reboot the computer */
	LOGOUT_ACTION_SUSPEND,		/* suspend the computer */
	LOGOUT_ACTION_HIBERNATE,	/* hibernate the computer */
	LOGOUT_ACTION_HYBRIDSLEEP,	/* hybrid sleep the computer */
	LOGOUT_ACTION_SWITCHUSER,	/* switch users */
	LOGOUT_ACTION_SWITCHDESK,	/* switch desktops */
	LOGOUT_ACTION_LOCKSCREEN,	/* lock screen */
	LOGOUT_ACTION_CHECKPOINT,	/* checkpoint the current session */
	LOGOUT_ACTION_SHUTDOWN,		/* checkpoint and shutdown session */
	LOGOUT_ACTION_LOGOUT,		/* logout of current session */
	LOGOUT_ACTION_RESTART,		/* restart current session */
	LOGOUT_ACTION_CANCEL,		/* cancel logout */
	LOGOUT_ACTION_COUNT,
} LogoutActionResult;

LogoutActionResult action_result;
LogoutActionResult logout_result = LOGOUT_ACTION_CANCEL;
#endif				/* DO_LOGOUT */

#if !defined(DO_XLOGIN) & !defined(DO_XCHOOSER) || defined(DO_GREETER)
static SmcConn smcConn;
#endif

Atom _XA_XDE_THEME_NAME;
Atom _XA_GTK_READ_RCFILES;
Atom _XA_XDE_XLOCK_COMMAND;
Atom _XA_XROOTPMAP_ID;
Atom _XA_ESETROOT_PMAP_ID;

#ifdef DO_XLOCKING
int xssEventBase;
int xssErrorBase;
int xssMajorVersion;
int xssMinorVersion;

const char *
xssState(int state)
{
	switch (state) {
	case ScreenSaverOff:
		return ("Off");
	case ScreenSaverOn:
		return ("On");
	case ScreenSaverCycle:
		return ("Cycle");
	case ScreenSaverDisabled:
		return ("Disabled");
	}
	return ("(unknown)");
}

const char *
xssKind(int kind)
{
	switch (kind) {
	case ScreenSaverBlanked:
		return ("Blanked");
	case ScreenSaverInternal:
		return ("Internal");
	case ScreenSaverExternal:
		return ("External");
	}
	return ("(unknown)");
}
#endif

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

/*
 * The run state of the program.  There are several states as follows:
 *
 * RunStateStartup - the program is starting up and initializing.  On completion
 *	check for a window manager and if not present start guard timer and move
 *	to RunStatePhase1.
 *
 * RunStatePhase1 - waiting for a window manager to appear.  Upon expiry of the
 *	guard timer or appearance of a window manager move to RunStatePhase2.
 *	Set the desktop session to the window manager name when one appears.
 *	When none appears, fall back to command-line options of default for
 *	desktop session.  Set DESKTOP_SESSION and XDG_CURRENT_DESKTOP
 *	environment variables.  Start spawned thread to begin reading autostart
 *	database.
 *
 * RunStatePhase2 - post window-manager pause state.  We detect window manager
 *	startup quite quickly, so there is an optional pause after detection
 *	before proceeding.  This state waits for the pause timer to expire
 *	before proceeding to RunStatePhase3.
 *
 * RunStatePhase3 - performing execs.  Read the startup file and perform any
 *	commands, also from command-line arguments (--exec flag).  Still apply
 *	whatever startup detection is possible (appearance of windows with
 *	associated PIDs).  We might be able to get xde-launch to do the job for
 *	us anyway because it can perform startup notification even for basic
 *	commands.  Note that these commands represent independent tasks, not
 *	just a list of simple commands.  It should not really matter in what
 *	order they are executed.  After the last guard/delay timers have
 *	expired, move to RunStatePhase4.
 *
 * RunStatePhase4 - performing autostart.  Read the autostart desktop entry
 *      database.  (Note we can overlap reading of the desktop entries with the
 *      previous phases by spawning a thread and joining it here instead of
 *      serializing.)  Execute eacn autostart task in turn, following command
 *      line options for guard, delay, possibly serialization or parallel start;
 *      once done, move to RunStatePhase5.  Each task has its own state.
 *
 * RunStatePhase5 - awaiting startup completion.  Await the startup completion
 *	or failure of each launched task.  We should provide desktop
 *	notification for tasks that fail or fail to signal completion.  Once all
 *	tasks have finalized, move to RunStateMonitor.
 *
 * RunStateMonitor - monitoring.  Monitor child processes checking whether we
 *	should restart tasks upon failure.  Continue to provide notification of
 *	failures and provide restart options to user.  We can checkpoint with a
 *	session manager when entering this phase.  If the window manager changes
 *	during this phase, we may wish to tear down any children that had an
 *	OnlyShowIn  or NotShowIn that no longer applies, or start a task for one
 *	that newly applies.  We might also perform similar changes on
 *	notification of changes to the autostart directories.  For window
 *	manager changes, we may choose to tear down all autostart tasks and
 *	start again from scratch: this is because the whole autostart process
 *	largely expects tasks to execute _after_ the window manager appears.  So
 *	a window manager change is more like logging out and logging back in
 *	again, which is ok.  So, for a window manager change, tear down all
 *	tasks and go back to Phase 2.
 *
 *
 */
typedef enum {
	RunStateStartup,		/* starting up */
	RunStatePhase1,			/* waiting for window manager */
	RunStatePhase2,			/* post-window-manager pause */
	RunStatePhase3,			/* running execs */
	RunStatePhase4,			/* running autostarts */
	RunStatePhase5,			/* waiting for startup completion */
	RunStateMonitor,		/* monitoring window manager */
} XdeRunState;

XdeRunState runstate = RunStateStartup;

typedef enum {
	StartupPhasePreDisplayServer,
	StartupPhaseInitialization,
	StartupPhaseWindowManager,
	StartupPhaseDesktop,
	StartupPhasePanel,
	StartupPhaseApplication,
} XdeStartupPhase;

#define WaitForNothing		(0)
#define WaitForHelper		(1<<0)
#define WaitForAudio		(1<<1)
#define WaitForManager		(1<<2)
#define WaitForComposite	(1<<3)
#define WaitForTray		(1<<4)
#define WaitForPager		(1<<5)
#define WaitForAll		(WaitForHelper|WaitForAudio|WaitForManager|WaitForComposite|WaitForTray|WaitForPager)
#define WaitForCount		6

typedef struct {
	XdeRunState state;
	guint timer;

} XdeContext;

XdeContext ctx;

typedef enum {
	TaskStateIdle,			/* task is not running yet */
	TaskStateStartup,		/* running but in startup notification */
	TaskStateRunning,		/* completed startup notification */
	TaskStateSignaled,		/* exited on a signal */
	TaskStateFailed,		/* exited with non-zero exit status */
	TaskStateDone,			/* exited with zero exit status */
} TaskState;

typedef struct {
	TaskState state;		/* state of the task */
	GtkWidget *button;		/* button corresponding to this task */
	guint blink;			/* timer source for blinking button */
	const char *appid;		/* application id */
	GKeyFile *entry;		/* key file entry */
	gboolean runnable;		/* whether the entry is runnable */
	int waitfor;			/* what to wait for */
	const char *cmd;		/* command to execute */
} TaskContext;

GHashTable *autostarts;

GtkWidget *splash;
GtkWidget *table;

static const char *StartupNotifyFields[] = {
	"LAUNCHER",
	"LAUNCHEE",
	"SEQUENCE",
	"ID",
	"NAME",
	"ICON",
	"BIN",
	"DESCRIPTION",
	"WMCLASS",
	"SILENT",
	"APPLICATION_ID",
	"DESKTOP",
	"SCREEN",
	"MONITOR",
	"TIMESTAMP",
	"PID",
	"HOSTNAME",
	"COMMAND",
	"ACTION",
	"FILE",
	"URL",
	NULL
};

typedef enum {
	FIELD_OFFSET_LAUNCHER,
	FIELD_OFFSET_LAUNCHEE,
	FIELD_OFFSET_SEQUENCE,
	FIELD_OFFSET_ID,
	FIELD_OFFSET_NAME,
	FIELD_OFFSET_ICON,
	FIELD_OFFSET_BIN,
	FIELD_OFFSET_DESCRIPTION,
	FIELD_OFFSET_WMCLASS,
	FIELD_OFFSET_SILENT,
	FIELD_OFFSET_APPLICATION_ID,
	FIELD_OFFSET_DESKTOP,
	FIELD_OFFSET_SCREEN,
	FIELD_OFFSET_MONITOR,
	FIELD_OFFSET_TIMESTAMP,
	FIELD_OFFSET_PID,
	FIELD_OFFSET_HOSTNAME,
	FIELD_OFFSET_COMMAND,
	FIELD_OFFSET_ACTION,
	FIELD_OFFSET_FILE,
	FIELD_OFFSET_URL,
} FieldOffset;

struct fields {
	char *launcher;
	char *launchee;
	char *sequence;
	char *id;
	char *name;
	char *icon;
	char *bin;
	char *description;
	char *wmclass;
	char *silent;
	char *application_id;
	char *desktop;
	char *screen;
	char *monitor;
	char *timestamp;
	char *pid;
	char *hostname;
	char *command;
	char *action;
	char *file;
	char *url;
};

typedef enum {
	StartupNotifyIdle,
	StartupNotifyNew,
	StartupNotifyChanged,
	StartupNotifyComplete,
} StartupNotifyState;

typedef struct _Client Client;

typedef struct _Sequence {
	struct _Sequence *next;
	int refs;
	StartupNotifyState state;
	Client *client;
	Bool changed;
	union {
		char *fields[sizeof(struct fields) / sizeof(char *)];
		struct fields f;
	};
	struct {
		unsigned screen;
		unsigned monitor;
		unsigned desktop;
		unsigned timestamp;
		unsigned silent;
		unsigned pid;
		unsigned sequence;
	} n;
	gint timer;
} Sequence;

typedef struct {
	Window origin;			/* origin of message sequence */
	char *data;			/* message bytes */
	int len;			/* number of message bytes */
} Message;

Sequence *sequences;			/* sequences for this display */

typedef struct {
	int screen;			/* screen number */
	Window root;			/* root window of screen */
	Window owner;			/* SELECTION_ATOM selection owner (theirs) */
	Window selwin;			/* SELECTION_ATOM selection window (ours) */
	Window netwm_check;		/* _NET_SUPPORTING_WM_CHECK or None */
	Window winwm_check;		/* _WIN_SUPPORTING_WM_CHECK or None */
	Window motif_check;		/* _MOTIF_MW_INFO window or None */
	Window maker_check;		/* _WINDOWMAKER_NOTICEBOARD or None */
	Window icccm_check;		/* WM_S%d selection owner or root */
	Window redir_check;		/* set to root when SubstructureRedirect */
	Window stray_owner;		/* _NET_SYSTEM_TRAY_S%d owner */
	Window pager_owner;		/* _NET_DESKTOP_LAYOUT_S%d owner */
	Window compm_owner;		/* _NET_WM_CM_S%d owner */
	Window audio_owner;		/* PULSE_SERVER owner or root */
	Window shelp_owner;		/* _XDG_ASSIST_S%d owner */
	Atom icccm_atom;		/* WM_S%d atom for this screen */
	Atom stray_atom;		/* _NET_SYSTEM_TRAY_S%d atom this screen */
	Atom pager_atom;		/* _NET_DESKTOP_LAYOUT_S%d atom this screen */
	Atom compm_atom;		/* _NET_WM_CM_S%d atom this screen */
	Atom shelp_atom;		/* _XDG_ASSIST_S%d atom this screen */
	Atom slctn_atom;		/* SELECTION_ATOM atom this screen */
	Bool net_wm_user_time;		/* _NET_WM_USER_TIME is supported */
	Bool net_startup_id;		/* _NET_STARTUP_ID is supported */
	Bool net_startup_info;		/* _NET_STARTUP_INFO is supported */
	Client *clients;		/* clients for this screen */
} XdgScreen;

XdgScreen *xscreens;

Display *dpy = NULL;
int nscr;
XdgScreen *scr;

typedef struct {
	unsigned long state;
	Window icon_window;
} XWMState;

enum {
	XEMBED_MAPPED = (1 << 0),
};

typedef struct {
	unsigned long version;
	unsigned long flags;
} XEmbedInfo;

struct _Client {
	Client *next;			/* next client in list */
	Sequence *seq;			/* associated startup sequence */
	int screen;			/* screen number */
	Window win;			/* client window */
	Window time_win;		/* client time window */
	Window transient_for;		/* transient for */
	Window leader;			/* client leader window */
	Window group;			/* client group window */
	Bool dockapp;			/* client is a dock app */
	Bool statusicon;		/* client is a status icon */
	Bool breadcrumb;		/* for traversing list */
	Bool managed;			/* managed by window manager */
	Bool listed;			/* listed by window manager */
	Bool new;			/* brand new */
	Time active_time;		/* last time active */
	Time focus_time;		/* last time focused */
	Time user_time;			/* last time used */
	Time map_time;			/* last time window was mapped */
	Time last_time;			/* last time something happened to this window */
	pid_t pid;			/* process id */
	char *startup_id;		/* startup id (property) */
	char **command;			/* command */
	char *name;			/* window name */
	char *hostname;			/* client machine */
	char *client_id;		/* session management id */
	char *role;			/* session management role */
	XClassHint ch;			/* class hint */
	XWMHints *wmh;			/* window manager hints */
	XWMState *wms;			/* window manager state */
	XEmbedInfo *xei;		/* _XEMBED_INFO property */
};

Time current_time = CurrentTime;
Time last_user_time = CurrentTime;

Atom _XA_KDE_WM_CHANGE_STATE;
Atom _XA_MANAGER;
Atom _XA_MOTIF_WM_INFO;
Atom _XA_NET_ACTIVE_WINDOW;
Atom _XA_NET_CLIENT_LIST;
Atom _XA_NET_CLIENT_LIST_STACKING;
Atom _XA_NET_CLOSE_WINDOW;
Atom _XA_NET_CURRENT_DESKTOP;
Atom _XA_NET_DESKTOP_LAYOUT;
Atom _XA_NET_MOVERESIZE_WINDOW;
Atom _XA_NET_REQUEST_FRAME_EXTENTS;
Atom _XA_NET_RESTACK_WINDOW;
Atom _XA_NET_STARTUP_ID;
Atom _XA_NET_STARTUP_INFO;
Atom _XA_NET_STARTUP_INFO_BEGIN;
Atom _XA_NET_SUPPORTED;
Atom _XA_NET_SUPPORTING_WM_CHECK;
Atom _XA_NET_VIRTUAL_ROOTS;
Atom _XA_NET_VISIBLE_DESKTOPS;
Atom _XA_NET_WM_ALLOWED_ACTIONS;
Atom _XA_NET_WM_FULLSCREEN_MONITORS;
Atom _XA_NET_WM_ICON_GEOMETRY;
Atom _XA_NET_WM_ICON_NAME;
Atom _XA_NET_WM_MOVERESIZE;
Atom _XA_NET_WM_NAME;
Atom _XA_NET_WM_PID;
Atom _XA_NET_WM_STATE;
Atom _XA_NET_WM_STATE_FOCUSED;
Atom _XA_NET_WM_STATE_HIDDEN;
Atom _XA_NET_WM_USER_TIME;
Atom _XA_NET_WM_USER_TIME_WINDOW;
Atom _XA_NET_WM_VISIBLE_ICON_NAME;
Atom _XA_NET_WM_VISIBLE_NAME;
Atom _XA_PULSE_COOKIE;
Atom _XA_PULSE_ID;
Atom _XA_PULSE_SERVER;
Atom _XA_SM_CLIENT_ID;
Atom _XA__SWM_VROOT;
Atom _XA_TIMESTAMP_PROP;
Atom _XA_WIN_APP_STATE;
Atom _XA_WIN_CLIENT_LIST;
Atom _XA_WIN_CLIENT_MOVING;
Atom _XA_WINDOWMAKER_NOTICEBOARD;
Atom _XA_WIN_FOCUS;
Atom _XA_WIN_HINTS;
Atom _XA_WIN_LAYER;
Atom _XA_WIN_PROTOCOLS;
Atom _XA_WIN_STATE;
Atom _XA_WIN_SUPPORTING_WM_CHECK;
Atom _XA_WIN_WORKSPACE;
Atom _XA_WM_CHANGE_STATE;
Atom _XA_WM_CLIENT_LEADER;
Atom _XA_WM_DELETE_WINDOW;
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_SAVE_YOURSELF;
Atom _XA_WM_STATE;
Atom _XA_WM_WINDOW_ROLE;
Atom _XA_XEMBED;
Atom _XA_XEMBED_INFO;
Atom _XA_UTF8_STRING;

static void pc_handle_MOTIF_WM_INFO(XPropertyEvent *, Client *);
static void pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *, Client *);
static void pc_handle_NET_STARTUP_ID(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTED(XPropertyEvent *, Client *);
static void pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_PID(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_USER_TIME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *, Client *);
static void pc_handle_SM_CLIENT_ID(XPropertyEvent *, Client *);
static void pc_handle_TIMESTAMP_PROP(XPropertyEvent *, Client *);
static void pc_handle_WIN_APP_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_LIST(XPropertyEvent *, Client *);
static void pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *, Client *);
static void pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *, Client *);
static void pc_handle_WIN_FOCUS(XPropertyEvent *, Client *);
static void pc_handle_WIN_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WIN_LAYER(XPropertyEvent *, Client *);
static void pc_handle_WIN_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WIN_STATE(XPropertyEvent *, Client *);
static void pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *, Client *);
static void pc_handle_WIN_WORKSPACE(XPropertyEvent *, Client *);
static void pc_handle_WM_CLASS(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_LEADER(XPropertyEvent *, Client *);
static void pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *, Client *);
static void pc_handle_WM_COMMAND(XPropertyEvent *, Client *);
static void pc_handle_WM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_ICON_SIZE(XPropertyEvent *, Client *);
static void pc_handle_WM_NAME(XPropertyEvent *, Client *);
static void pc_handle_WM_NORMAL_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_PROTOCOLS(XPropertyEvent *, Client *);
static void pc_handle_WM_SIZE_HINTS(XPropertyEvent *, Client *);
static void pc_handle_WM_STATE(XPropertyEvent *, Client *);
static void pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *, Client *);
static void pc_handle_WM_WINDOW_ROLE(XPropertyEvent *, Client *);
static void pc_handle_WM_ZOOM_HINTS(XPropertyEvent *, Client *);
static void pc_handle_XEMBED_INFO(XPropertyEvent *, Client *);

static void cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_MANAGER(XClientMessageEvent *, Client *);
static void cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *, Client *);
static void cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *, Client *);
static void cm_handle_NET_STARTUP_INFO(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *, Client *);
static void cm_handle_NET_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_LAYER(XClientMessageEvent *, Client *);
static void cm_handle_WIN_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WIN_WORKSPACE(XClientMessageEvent *, Client *);
static void cm_handle_WM_CHANGE_STATE(XClientMessageEvent *, Client *);
static void cm_handle_WM_PROTOCOLS(XClientMessageEvent *, Client *);
static void cm_handle_WM_STATE(XClientMessageEvent *, Client *);
static void cm_handle_XEMBED(XClientMessageEvent *, Client *);

typedef void (*pc_handler_t) (XPropertyEvent *, Client *);
typedef void (*cm_handler_t) (XClientMessageEvent *, Client *);

struct atoms {
	char *name;
	Atom *atom;
	pc_handler_t pc_handler;
	cm_handler_t cm_handler;
	Atom value;
} atoms[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "_KDE_WM_CHANGE_STATE",		&_XA_KDE_WM_CHANGE_STATE,	NULL,					&cm_handle_KDE_WM_CHANGE_STATE,		None			},
	{ "MANAGER",				&_XA_MANAGER,			NULL,					&cm_handle_MANAGER,			None			},
	{ "_MOTIF_WM_INFO",			&_XA_MOTIF_WM_INFO,		&pc_handle_MOTIF_WM_INFO,		NULL,					None			},
	{ "_NET_ACTIVE_WINDOW",			&_XA_NET_ACTIVE_WINDOW,		&pc_handle_NET_ACTIVE_WINDOW,		&cm_handle_NET_ACTIVE_WINDOW,		None			},
	{ "_NET_CLIENT_LIST",			&_XA_NET_CLIENT_LIST,		&pc_handle_NET_CLIENT_LIST,		NULL,					None			},
	{ "_NET_CLIENT_LIST_STACKING",		&_XA_NET_CLIENT_LIST_STACKING,	&pc_handle_NET_CLIENT_LIST_STACKING,	NULL,					None			},
	{ "_NET_CLOSE_WINDOW",			&_XA_NET_CLOSE_WINDOW,		NULL,					&cm_handle_NET_CLOSE_WINDOW,		None			},
	{ "_NET_CURRENT_DESKTOP",		&_XA_NET_CURRENT_DESKTOP,	NULL,					NULL,					None			},
	{ "_NET_DESKTOP_LAYOUT",		&_XA_NET_DESKTOP_LAYOUT,	NULL,					NULL,					None			},
	{ "_NET_MOVERESIZE_WINDOW",		&_XA_NET_MOVERESIZE_WINDOW,	NULL,					&cm_handle_NET_MOVERESIZE_WINDOW,	None			},
	{ "_NET_REQUEST_FRAME_EXTENTS",		&_XA_NET_REQUEST_FRAME_EXTENTS,	NULL,					&cm_handle_NET_REQUEST_FRAME_EXTENTS,	None			},
	{ "_NET_RESTACK_WINDOW",		&_XA_NET_RESTACK_WINDOW,	NULL,					&cm_handle_NET_RESTACK_WINDOW,		None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_STARTUP_INFO_BEGIN",		&_XA_NET_STARTUP_INFO_BEGIN,	NULL,					&cm_handle_NET_STARTUP_INFO_BEGIN,	None			},
	{ "_NET_STARTUP_INFO",			&_XA_NET_STARTUP_INFO,		NULL,					&cm_handle_NET_STARTUP_INFO,		None			},
	{ "_NET_SUPPORTED",			&_XA_NET_SUPPORTED,		&pc_handle_NET_SUPPORTED,		NULL,					None			},
	{ "_NET_SUPPORTING_WM_CHECK",		&_XA_NET_SUPPORTING_WM_CHECK,	&pc_handle_NET_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_NET_VIRUAL_ROOTS",			&_XA_NET_VIRTUAL_ROOTS,		NULL,					NULL,					None			},
	{ "_NET_VISIBLE_DESKTOPS",		&_XA_NET_VISIBLE_DESKTOPS,	NULL,					NULL,					None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_WM_FULLSCREEN_MONITORS",	&_XA_NET_WM_FULLSCREEN_MONITORS,&pc_handle_NET_WM_FULLSCREEN_MONITORS,	&cm_handle_NET_WM_FULLSCREEN_MONITORS,	None			},
	{ "_NET_WM_ICON_GEOMETRY",		&_XA_NET_WM_ICON_GEOMETRY,	&pc_handle_NET_WM_ICON_GEOMETRY,	NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ "_NET_WM_MOVERESIZE",			&_XA_NET_WM_MOVERESIZE,		NULL,					&cm_handle_NET_WM_MOVERESIZE,		None			},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE_FOCUSED",		&_XA_NET_WM_STATE_FOCUSED,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE_HIDDEN",		&_XA_NET_WM_STATE_HIDDEN,	NULL,					NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "_NET_WM_VISIBLE_ICON_NAME",		&_XA_NET_WM_VISIBLE_ICON_NAME,	&pc_handle_NET_WM_VISIBLE_ICON_NAME,	NULL,					None			},
	{ "_NET_WM_VISIBLE_NAME",		&_XA_NET_WM_VISIBLE_NAME,	&pc_handle_NET_WM_VISIBLE_NAME,		NULL,					None			},
	{ "PULSE_COOKIE",			&_XA_PULSE_COOKIE,		NULL,					NULL,					None			},
	{ "PULSE_ID",				&_XA_PULSE_ID,			NULL,					NULL,					None			},
	{ "PULSE_SERVER",			&_XA_PULSE_SERVER,		NULL,					NULL,					None			},
	{ "SM_CLIENT_ID",			&_XA_SM_CLIENT_ID,		&pc_handle_SM_CLIENT_ID,		NULL,					None			},
	{ "__SWM_VROOT",			&_XA__SWM_VROOT,		NULL,					NULL,					None			},
	{ "_TIMESTAMP_PROP",			&_XA_TIMESTAMP_PROP,		&pc_handle_TIMESTAMP_PROP,		NULL,					None			},
	{ "_WIN_APP_STATE",			&_XA_WIN_APP_STATE,		&pc_handle_WIN_APP_STATE,		NULL,					None			},
	{ "_WIN_CLIENT_LIST",			&_XA_WIN_CLIENT_LIST,		&pc_handle_WIN_CLIENT_LIST,		NULL,					None			},
	{ "_WIN_CLIENT_MOVING",			&_XA_WIN_CLIENT_MOVING,		&pc_handle_WIN_CLIENT_MOVING,		NULL,					None			},
	{ "_WINDOWMAKER_NOTICEBOARD",		&_XA_WINDOWMAKER_NOTICEBOARD,	&pc_handle_WINDOWMAKER_NOTICEBOARD,	NULL,					None			},
	{ "_WIN_FOCUS",				&_XA_WIN_FOCUS,			&pc_handle_WIN_FOCUS,			NULL,					None			},
	{ "_WIN_HINTS",				&_XA_WIN_HINTS,			&pc_handle_WIN_HINTS,			NULL,					None			},
	{ "_WIN_LAYER",				&_XA_WIN_LAYER,			&pc_handle_WIN_LAYER,			&cm_handle_WIN_LAYER,			None			},
	{ "_WIN_PROTOCOLS",			&_XA_WIN_PROTOCOLS,		&pc_handle_WIN_PROTOCOLS,		NULL,					None			},
	{ "_WIN_STATE",				&_XA_WIN_STATE,			&pc_handle_WIN_STATE,			&cm_handle_WIN_STATE,			None			},
	{ "_WIN_SUPPORTING_WM_CHECK",		&_XA_WIN_SUPPORTING_WM_CHECK,	&pc_handle_WIN_SUPPORTING_WM_CHECK,	NULL,					None			},
	{ "_WIN_WORKSPACE",			&_XA_WIN_WORKSPACE,		&pc_handle_WIN_WORKSPACE,		&cm_handle_WIN_WORKSPACE,		None			},
	{ "WM_CHANGE_STATE",			&_XA_WM_CHANGE_STATE,		NULL,					&cm_handle_WM_CHANGE_STATE,		None			},
	{ "WM_CLASS",				NULL,				&pc_handle_WM_CLASS,			NULL,					XA_WM_CLASS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_DELETE_WINDOW",			&_XA_WM_DELETE_WINDOW,		NULL,					NULL,					None			},
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_ICON_NAME",			NULL,				&pc_handle_WM_ICON_NAME,		NULL,					XA_WM_ICON_NAME		},
	{ "WM_ICON_SIZE",			NULL,				&pc_handle_WM_ICON_SIZE,		NULL,					XA_WM_ICON_SIZE		},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "WM_NORMAL_HINTS",			NULL,				&pc_handle_WM_NORMAL_HINTS,		NULL,					XA_WM_NORMAL_HINTS	},
	{ "WM_PROTOCOLS",			&_XA_WM_PROTOCOLS,		&pc_handle_WM_PROTOCOLS,		&cm_handle_WM_PROTOCOLS,		None			},
	{ "WM_SAVE_YOURSELF",			&_XA_WM_SAVE_YOURSELF,		NULL,					NULL,					None			},
	{ "WM_SIZE_HINTS",			NULL,				&pc_handle_WM_SIZE_HINTS,		NULL,					XA_WM_SIZE_HINTS	},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "WM_TRANSIENT_FOR",			NULL,				&pc_handle_WM_TRANSIENT_FOR,		NULL,					XA_WM_TRANSIENT_FOR	},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "WM_ZOOM_HINTS",			NULL,				&pc_handle_WM_ZOOM_HINTS,		NULL,					XA_WM_ZOOM_HINTS	},
	{ "_XEMBED_INFO",			&_XA_XEMBED_INFO,		&pc_handle_XEMBED_INFO,			NULL,					None			},
	{ "_XEMBED",				&_XA_XEMBED,			NULL,					&cm_handle_XEMBED,			None			},
	{ "UTF8_STRING",			&_XA_UTF8_STRING,		NULL,					NULL,					None			},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

/* These are listed in the order that we want them evaluated. */

struct atoms wmprops[] = {
	/* *INDENT-OFF* */
	/* name					global				pc_handler				cm_handler				atom value		*/
	/* ----					------				----------				----------				---------		*/
	{ "WM_HINTS",				NULL,				&pc_handle_WM_HINTS,			NULL,					XA_WM_HINTS		},
	{ "WM_CLIENT_LEADER",			&_XA_WM_CLIENT_LEADER,		&pc_handle_WM_CLIENT_LEADER,		NULL,					None			},
	{ "WM_STATE",				&_XA_WM_STATE,			&pc_handle_WM_STATE,			&cm_handle_WM_STATE,			None			},
	{ "_NET_WM_USER_TIME_WINDOW",		&_XA_NET_WM_USER_TIME_WINDOW,	&pc_handle_NET_WM_USER_TIME_WINDOW,	NULL,					None			},
	{ "WM_CLIENT_MACHINE",			NULL,				&pc_handle_WM_CLIENT_MACHINE,		NULL,					XA_WM_CLIENT_MACHINE	},
	{ "WM_COMMAND",				NULL,				&pc_handle_WM_COMMAND,			NULL,					XA_WM_COMMAND		},
	{ "WM_WINDOW_ROLE",			&_XA_WM_WINDOW_ROLE,		&pc_handle_WM_WINDOW_ROLE,		NULL,					None			},
	{ "_NET_WM_PID",			&_XA_NET_WM_PID,		&pc_handle_NET_WM_PID,			NULL,					None			},
	{ "_NET_WM_STATE",			&_XA_NET_WM_STATE,		&pc_handle_NET_WM_STATE,		&cm_handle_NET_WM_STATE,		None			},
	{ "_NET_WM_ALLOWED_ACTIONS",		&_XA_NET_WM_ALLOWED_ACTIONS,	&pc_handle_NET_WM_ALLOWED_ACTIONS,	&cm_handle_NET_WM_ALLOWED_ACTIONS,	None			},
	{ "_NET_STARTUP_ID",			&_XA_NET_STARTUP_ID,		&pc_handle_NET_STARTUP_ID,		NULL,					None			},
	{ "_NET_WM_USER_TIME",			&_XA_NET_WM_USER_TIME,		&pc_handle_NET_WM_USER_TIME,		NULL,					None			},
	{ "WM_NAME",				NULL,				&pc_handle_WM_NAME,			NULL,					XA_WM_NAME		},
	{ "_NET_WM_NAME",			&_XA_NET_WM_NAME,		&pc_handle_NET_WM_NAME,			NULL,					None			},
	{ "_NET_WM_ICON_NAME",			&_XA_NET_WM_ICON_NAME,		&pc_handle_NET_WM_ICON_NAME,		NULL,					None			},
	{ NULL,					NULL,				NULL,					NULL,					None			}
	/* *INDENT-ON* */
};

XContext PropertyContext;		/* atom to property handler context */
XContext ClientMessageContext;		/* atom to client message handler context */
XContext ScreenContext;			/* window to screen context */
XContext ClientContext;			/* window to client context */
XContext MessageContext;		/* window to message context */

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

int n_autostarts = 0;

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
				n_autostarts++;
			}
		}
		closedir(D);
	}
	free(path);
	free(dirs);
}

typedef struct {
	int cols;			/* columns in the table */
	int rows;			/* rows in the table */
	int col;			/* column index of the next free cell */
	int row;			/* row index of the next free cell */
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
	c.rows = (n_autostarts + c.cols + 1) / c.cols;
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

static void
intern_atoms()
{
	int i, j, n;
	char **atom_names;
	Atom *atom_values;
	struct atoms *atom;

	for (i = 0, n = 0; atoms[i].name; i++)
		if (atoms[i].atom)
			n++;
	atom_names = calloc(n + 1, sizeof(*atom_names));
	atom_values = calloc(n + 1, sizeof(*atom_values));
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			atom_names[j++] = atoms[i].name;
	XInternAtoms(dpy, atom_names, n, False, atom_values);
	for (i = 0, j = 0; j < n; i++)
		if (atoms[i].atom)
			*atoms[i].atom = atoms[i].value = atom_values[j++];
	free(atom_names);
	free(atom_values);
	for (atom = wmprops; atom->name; atom++)
		if (atom->atom)
			atom->value = *(atom->atom);
	for (atom = atoms; atom->name; atom++) {
		if (atom->pc_handler)
			XSaveContext(dpy, atom->value, PropertyContext, (XPointer) atom->pc_handler);
		if (atom->cm_handler)
			XSaveContext(dpy, atom->value, ClientMessageContext, (XPointer) atom->cm_handler);
	}
}

static int
handler(Display *display, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, NAME, num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

static char *
get_text(Window win, Atom prop)
{
	XTextProperty tp = { NULL, };

	XGetTextProperty(dpy, win, &tp, prop);
	if (tp.value) {
		tp.value[tp.nitems + 1] = '\0';
		return (char *) tp.value;
	}
	return NULL;
}

static long *
get_cardinals(Window win, Atom prop, Atom type, long *n)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	long *data = NULL;

      try_harder:
	if (XGetWindowProperty
	    (dpy, win, prop, 0L, num, False, type, &real, &format, &nitems, &after,
	     (unsigned char **) &data) == Success && format != 0) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			goto try_harder;
		}
		if ((*n = nitems) > 0)
			return data;
		if (data)
			XFree(data);
	} else
		*n = -1;
	return NULL;
}

static Bool
get_cardinal(Window win, Atom prop, Atom type, long *card_ret)
{
	Bool result = False;
	long *data, n;

	if ((data = get_cardinals(win, prop, type, &n)) && n > 0) {
		*card_ret = data[0];
		result = True;
	}
	if (data)
		XFree(data);
	return result;
}

static Window *
get_windows(Window win, Atom prop, Atom type, long *n)
{
	return (Window *) get_cardinals(win, prop, type, n);
}

static Bool
get_window(Window win, Atom prop, Atom type, Window *win_ret)
{
	return get_cardinal(win, prop, type, (long *) win_ret);
}

Time *
get_times(Window win, Atom prop, Atom type, long *n)
{
	return (Time *) get_cardinals(win, prop, type, n);
}

static Bool
get_time(Window win, Atom prop, Atom type, Time *time_ret)
{
	return get_cardinal(win, prop, type, (long *) time_ret);
}

static Atom *
get_atoms(Window win, Atom prop, Atom type, long *n)
{
	return (Atom *) get_cardinals(win, prop, type, n);
}

Bool
get_atom(Window win, Atom prop, Atom type, Atom *atom_ret)
{
	return get_cardinal(win, prop, type, (long *) atom_ret);
}

XWMState *
XGetWMState(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XWMState *wms = NULL;

	if (XGetWindowProperty
	    (d, win, _XA_WM_STATE, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &wms) == Success && format != 0) {
		if (format != 32 || nitems < 2) {
			if (wms) {
				XFree(wms);
				return NULL;
			}
		}
		return wms;
	}
	return NULL;

}

XEmbedInfo *
XGetEmbedInfo(Display *d, Window win)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 2;
	XEmbedInfo *xei = NULL;

	if (XGetWindowProperty
	    (d, win, _XA_XEMBED_INFO, 0L, num, False, AnyPropertyType, &real, &format,
	     &nitems, &after, (unsigned char **) &xei) == Success && format != 0) {
		if (format != 32 || nitems < 2) {
			if (xei) {
				XFree(xei);
				return NULL;
			}
		}
		return xei;
	}
	return NULL;
}

Bool
latertime(Time last, Time time)
{
	if (time == CurrentTime)
		return False;
	if (last == CurrentTime || (int) time - (int) last > 0)
		return True;
	return False;
}

void
pushtime(Time *last, Time time)
{
	if (latertime(*last, time))
		*last = time;
}

void
pulltime(Time *last, Time time)
{
	if (!latertime(*last, time))
		*last = time;
}

/** @brief Check for recursive window properties
  * @param atom - property name
  * @param type - property type
  * @return Window - the recursive window property or None
  */
Window
check_recursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check;

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
			XFree(data);
			data = NULL;
		} else {
			if (data)
				XFree(data);
			return None;
		}
		if (XGetWindowProperty
		    (dpy, check, atom, 0L, 1L, False, type, &real, &format, &nitems,
		     &after, (unsigned char **) &data) == Success && format != 0) {
			if (nitems > 0) {
				if (check != (Window) data[0]) {
					XFree(data);
					return None;
				}
			} else {
				if (data)
					XFree(data);
				return None;
			}
			XFree(data);
		} else
			return None;
	} else
		return None;
	return check;
}

/** @brief Check for non-recusive window properties (that should be recursive).
  * @param atom - property name
  * @param atom - property type
  * @return Window - the recursive window property or None
  */
Window
check_nonrecursive(Atom atom, Atom type)
{
	Atom real;
	int format;
	unsigned long nitems, after;
	unsigned long *data = NULL;
	Window check = None;

	DPRINTF("non-recursive check for atom 0x%lx\n", atom);

	if (XGetWindowProperty(dpy, scr->root, atom, 0L, 1L, False, type, &real, &format,
			       &nitems, &after, (unsigned char **) &data)
	    == Success && format != 0) {
		if (nitems > 0) {
			if ((check = data[0]) && check != scr->root)
				XSelectInput(dpy, check, PropertyChangeMask | StructureNotifyMask);
		}
		if (data)
			XFree(data);
	}
	return check;
}

/** @brief Check if an atom is in a supported atom list.
  * @param atom - list name
  * @param atom - element name
  * @return Bool - true if atom is in list
  */
static Bool
check_supported(Atom protocols, Atom supported)
{
	Atom real;
	int format;
	unsigned long nitems, after, num = 1;
	unsigned long *data = NULL;
	Bool result = False;

	DPRINTF("check for non-compliant NetWM\n");

      try_harder:
	if (XGetWindowProperty(dpy, scr->root, protocols, 0L, num, False, XA_ATOM, &real, &format,
			       &nitems, &after, (unsigned char **) &data) == Success && format) {
		if (after) {
			num += ((after + 1) >> 2);
			XFree(data);
			data = NULL;
			goto try_harder;
		}
		if (nitems > 0) {
			unsigned long i;
			Atom *atoms;

			atoms = (Atom *) data;
			for (i = 0; i < nitems; i++) {
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
	return result;
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
check_netwm_supported()
{
	if (check_supported(_XA_NET_SUPPORTED, _XA_NET_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
}

/** @brief Check for an EWMH/NetWM compliant (sorta) window manager.
  */
static Window
check_netwm()
{
	int i = 0;

	do {
		scr->netwm_check = check_recursive(_XA_NET_SUPPORTING_WM_CHECK, XA_WINDOW);
	} while (i++ < 2 && !scr->netwm_check);

	if (scr->netwm_check && scr->netwm_check != scr->root) {
		XSaveContext(dpy, scr->netwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->netwm_check, PropertyChangeMask | StructureNotifyMask);
	} else
		scr->netwm_check = check_netwm_supported();

	return scr->netwm_check;
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
check_winwm_supported()
{
	if (check_supported(_XA_WIN_PROTOCOLS, _XA_WIN_SUPPORTING_WM_CHECK))
		return scr->root;
	return check_nonrecursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
}

/** @brief Check for a GNOME1/WMH/WinWM compliant window manager.
  */
static Window
check_winwm()
{
	int i = 0;

	do {
		scr->winwm_check = check_recursive(_XA_WIN_SUPPORTING_WM_CHECK, XA_CARDINAL);
	} while (i++ < 2 && !scr->winwm_check);

	if (scr->winwm_check && scr->winwm_check != scr->root) {
		XSaveContext(dpy, scr->winwm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->winwm_check, PropertyChangeMask | StructureNotifyMask);
	} else
		scr->winwm_check = check_winwm_supported();

	return scr->winwm_check;
}

/** @brief Check for a WindowMaker compliant window manager.
  */
static Window
check_maker()
{
	int i = 0;

	do {
		scr->maker_check = check_recursive(_XA_WINDOWMAKER_NOTICEBOARD, XA_WINDOW);
	} while (i++ < 2 && !scr->maker_check);

	if (scr->maker_check && scr->maker_check != scr->root) {
		XSaveContext(dpy, scr->maker_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->maker_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->maker_check;
}

/** @brief Check for an OSF/Motif compliant window manager.
  */
static Window
check_motif()
{
	int i = 0;
	long *data, n = 0;

	do {
		data = get_cardinals(scr->root, _XA_MOTIF_WM_INFO, AnyPropertyType, &n);
	} while (i++ < 2 && !data);

	if (data && n >= 2)
		scr->motif_check = data[1];
	if (scr->motif_check && scr->motif_check != scr->root) {
		XSaveContext(dpy, scr->motif_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->motif_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->motif_check;
}

/** @brief Check for an ICCCM 2.0 compliant window manager.
  */
static Window
check_icccm()
{
	scr->icccm_check = XGetSelectionOwner(dpy, scr->icccm_atom);

	if (scr->icccm_check && scr->icccm_check != scr->root) {
		XSaveContext(dpy, scr->icccm_check, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->icccm_check, PropertyChangeMask | StructureNotifyMask);
	}
	return scr->icccm_check;
}

/** @brief Check whether an ICCCM window manager is present.
  *
  * This pretty much assumes that any ICCCM window manager will select for
  * SubstructureRedirectMask on the root window.
  */
static Window
check_redir()
{
	XWindowAttributes wa;

	DPRINTF("checking direction for screen %d\n", scr->screen);

	scr->redir_check = None;
	if (XGetWindowAttributes(dpy, scr->root, &wa))
		if (wa.all_event_masks & SubstructureRedirectMask)
			scr->redir_check = scr->root;
	return scr->redir_check;
}

/** @brief Find window manager and compliance for the current screen.
  */
static Bool
check_window_manager()
{
	Bool have_wm = False;

	DPRINTF("checking wm compliance for screen %d\n", scr->screen);

	DPRINTF("checking redirection\n");
	if (check_redir()) {
		have_wm = True;
		DPRINTF("redirection on window 0x%lx\n", scr->redir_check);
	}
	DPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_icccm()) {
		have_wm = True;
		DPRINTF("ICCCM 2.0 window 0x%lx\n", scr->icccm_check);
	}
	DPRINTF("checking OSF/Motif compliance\n");
	if (check_motif()) {
		have_wm = True;
		DPRINTF("OSF/Motif window 0x%lx\n", scr->motif_check);
	}
	DPRINTF("checking WindowMaker compliance\n");
	if (check_maker()) {
		have_wm = True;
		DPRINTF("WindowMaker window 0x%lx\n", scr->maker_check);
	}
	DPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm()) {
		have_wm = True;
		DPRINTF("GNOME/WMH window 0x%lx\n", scr->winwm_check);
	}
	scr->net_wm_user_time = False;
	scr->net_startup_id = False;
	scr->net_startup_info = False;
	DPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm()) {
		have_wm = True;
		DPRINTF("NetWM/EWMH window 0x%lx\n", scr->netwm_check);
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_WM_USER_TIME)) {
			DPRINTF("_NET_WM_USER_TIME supported\n");
			scr->net_wm_user_time = True;
		}
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_ID)) {
			DPRINTF("_NET_STARTUP_ID supported\n");
			scr->net_startup_id = True;
		}
		if (check_supported(_XA_NET_SUPPORTED, _XA_NET_STARTUP_INFO)) {
			DPRINTF("_NET_STARTUP_INFO supported\n");
			scr->net_startup_info = True;
		}
	}
	return have_wm;
}

static void
handle_wmchange()
{
	if (!check_window_manager())
		check_window_manager();
}

static void
pc_handle_WINDOWMAKER_NOTICEBOARD(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_MOTIF_WM_INFO(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

Bool
set_screen_of_root(Window sroot)
{
	int s;

	for (s = 0; s < nscr; s++) {
		if (xscreens[s].root == sroot) {
			scr = xscreens + s;
			return True;
		}
	}
	EPRINTF("Could not find screen for root 0x%lx!\n", sroot);
	return False;
}

Bool
find_window_screen(Window w)
{
	Window wroot, dw, *dwp;
	unsigned int du;

	if (!XQueryTree(dpy, w, &wroot, &dw, &dwp, &du))
		return False;
	if (dwp)
		XFree(dwp);

	return set_screen_of_root(wroot);
}

Bool
find_screen(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ScreenContext, (XPointer *) &scr))
		return True;
	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = xscreens + c->screen;
		return True;
	}
	return find_window_screen(w);
}

Client *
find_client(Window w)
{
	Client *c = NULL;

	if (!XFindContext(dpy, w, ClientContext, (XPointer *) &c)) {
		scr = xscreens + c->screen;
		return (c);
	}
	find_screen(w);
	return (c);
}

void
apply_quotes(char **str, char *q)
{
	char *p;

	for (p = *str; *p; p++) ;
	while (*q) {
		if (*q == ' ' || *q == '"' || *q == '\\')
			*p++ = '\\';
		*p++ = *q++;
	}
	*p = '\0';
	*str = p;
}

Bool running;

void
send_msg(char *msg)
{
	XEvent xev;
	int l;
	char *p;

	DPRINTF("Message to 0x%lx is: '%s'\n", scr->root, msg);

	xev.type = ClientMessage;
	xev.xclient.message_type = _XA_NET_STARTUP_INFO_BEGIN;
	xev.xclient.display = dpy;
	xev.xclient.window = scr->selwin;
	xev.xclient.format = 8;

	l = strlen((p = msg)) + 1;

	while (l > 0) {
		strncpy(xev.xclient.data.b, p, 20);
		p += 20;
		l -= 20;
		/* just PropertyChange mask in the spec doesn't work :( */
		if (!XSendEvent(dpy, scr->root, False, StructureNotifyMask |
				SubstructureNotifyMask | SubstructureRedirectMask | PropertyChangeMask, &xev))
			EPRINTF("XSendEvent: failed!\n");
		xev.xclient.message_type = _XA_NET_STARTUP_INFO;
	}
}

struct {
	char *label;
	FieldOffset offset;
} labels[] = {
	/* *INDENT-OFF* */
	{ " NAME=",		FIELD_OFFSET_NAME		},
	{ " ICON=",		FIELD_OFFSET_ICON		},
	{ " BIN=",		FIELD_OFFSET_BIN		},
	{ " DESCRIPTION=",	FIELD_OFFSET_DESCRIPTION	},
	{ " WMCLASS=",		FIELD_OFFSET_WMCLASS		},
	{ " SILENT=",		FIELD_OFFSET_SILENT		},
	{ " APPLICATION_ID=",	FIELD_OFFSET_APPLICATION_ID	},
	{ " DESKTOP=",		FIELD_OFFSET_DESKTOP		},
	{ " SCREEN=",		FIELD_OFFSET_SCREEN		},
	{ " MONITOR=",		FIELD_OFFSET_MONITOR		},
	{ " TIMESTAMP=",	FIELD_OFFSET_TIMESTAMP		},
	{ " PID=",		FIELD_OFFSET_PID		},
	{ " HOSTNAME=",		FIELD_OFFSET_HOSTNAME		},
	{ " COMMAND=",		FIELD_OFFSET_COMMAND		},
        { " ACTION=",           FIELD_OFFSET_ACTION		},
	{ NULL,			FIELD_OFFSET_ID			}
	/* *INDENT-ON* */
};

void
add_field(Sequence *seq, char **p, char *label, FieldOffset offset)
{
	char *value;

	if ((value = seq->fields[offset])) {
		strcat(*p, label);
		apply_quotes(p, value);
	}
}

void
add_fields(Sequence *seq, char *msg)
{
	int i;

	for (i = 0; labels[i].label; i++)
		add_field(seq, &msg, labels[i].label, labels[i].offset);
}

/** @brief send a 'new' startup notification message
  * @param seq - sequence context for which to send new message
  *
  * We do not really use this in this program...
  */
void
send_new(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(BUFSIZ, sizeof(*msg));
	strcat(p, "new:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyNew;
}

/** @brief send a 'change' startup notification message
  * @param seq - sequence context for which to send change message
  *
  * We do not really use this in this program...  However, we could use it to
  * update information determined about the client (if a client is associated)
  * before closing or waiting for the closure of the sequence.
  */
void
send_change(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(BUFSIZ, sizeof(*msg));
	strcat(p, "change:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	add_fields(seq, p);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyChanged;
}

/** @brief send a 'remove' startup notification message
  * @param seq - sequence context for which to send remove message
  *
  * We only need the ID= field to send a remove message.  We do this to close a
  * sequence that is awaiting the mapping of a window.
  */
static void
send_remove(Sequence *seq)
{
	char *msg, *p;

	p = msg = calloc(BUFSIZ, sizeof(*msg));
	strcat(p, "remove:");
	add_field(seq, &p, " ID=", FIELD_OFFSET_ID);
	send_msg(msg);
	free(msg);
	seq->state = StartupNotifyComplete;
}

/** @brief - get a proc file and read it into a buffer
  * @param pid - process id for which to get the proc file
  * @param name - name of the proc file
  * @param size - return the size of the buffer
  * @return char* - the buffer or NULL if it does not exist or error
  *
  * Used to get a proc file for a pid by name and read the entire file into a
  * buffer.  Returns the buffer and the size of the buffer read.
  */
char *
get_proc_file(pid_t pid, char *name, size_t *size)
{
	struct stat st;
	char *file, *buf;
	FILE *f;
	size_t read, total;

	file = calloc(64, sizeof(*file));
	snprintf(file, 64, "/proc/%d/%s", pid, name);

	if (stat(file, &st)) {
		free(file);
		*size = 0;
		return NULL;
	}
	buf = calloc(st.st_size, sizeof(*buf));
	if (!(f = fopen(file, "rb"))) {
		free(file);
		free(buf);
		*size = 0;
		return NULL;
	}
	free(file);
	/* read entire file into buffer */
	for (total = 0; total < st.st_size; total += read)
		if ((read = fread(buf + total, 1, st.st_size - total, f)))
			if (total < st.st_size) {
				free(buf);
				fclose(f);
				*size = 0;
				return NULL;
			}
	fclose(f);
	*size = st.st_size;
	return buf;
}

/** @brief get a process' startup id
  * @param pid - pid of the process
  * @return char* - startup id or NULL
  *
  * Gets the DESKTOP_STARTUP_ID environment variable for a given process from
  * the environment file for the corresponding pid.  This only works when the
  * pid is for a process on the same host as we are.
  *
  * /proc/%d/environ contains the evironment for the process.  The entries are
  * separated by null bytes ('\0'), and there may be a null byte at the end.
  */
char *
get_proc_startup_id(pid_t pid)
{
	char *buf, *pos, *end;
	size_t size;

	if (!(buf = get_proc_file(pid, "environ", &size)))
		return NULL;

	for (pos = buf, end = buf + size; pos < end; pos += strlen(pos) + 1) {
		if (strstr(pos, "DESKTOP_STARTUP_ID=") == pos) {
			pos += strlen("DESKTOP_STARTUP_ID=");
			pos = strdup(pos);
			free(buf);
			return pos;
		}
	}
	free(buf);
	return NULL;
}

/** @brief get the command of a process
  * @param pid - pid of the process
  * @return char* - the command or NULL
  *
  * Obtains the command line of a process.
  */
char *
get_proc_comm(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "comm", &size);
}

/** @brief get the executable of a process
  * @param pid - pid of the process
  * @return char* - the executable path or NULL
  *
  * Obtains the executable path (binary file) executed by a process, or NULL if
  * there is no executable for the specified pid.  This uses the /proc/%d/exe
  * symbolic link to the executable file.  The returned pointer belongs to the
  * caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/exec is a symblolic link containing the actual pathname of the
  * executed command.  This symbolic link can be dereference normally;
  * attempting to open it will open the executable.  You can even execute the
  * file to run another copy of the same executable as is being run by the
  * process.  In a multithreaded process, the contents of this symblic link are
  * not available if the main thread has already terminated.
  */
char *
get_proc_exec(pid_t pid)
{
	struct stat st;
	char *file, *buf;
	ssize_t size;

	file = calloc(PATH_MAX + 1, sizeof(*file));
	snprintf(file, PATH_MAX, "/proc/%d/exe", pid);
	if (stat(file, &st)) {
		free(file);
		return NULL;
	}
	buf = calloc(256, sizeof(*buf));
	if ((size = readlink(file, buf, 256)) < 0 || size > 256 - 1) {
		free(file);
		free(buf);
		return NULL;
	}
	buf[size + 1] = '\0';
	return buf;
}

/** @brief get the first argument of the command line for a process
  * @param pid - pid of the process
  * @return char* - the command line
  *
  * Obtains the arguments of the command line (argv) from the /proc/%d/cmdline
  * file.  This file contains a list of null-terminated strings.  The pointer
  * returned points to the first argument in the list.  The returned pointer
  * belongs to the caller, and must be freed using free() when no longer in use.
  *
  * /proc/%d/cmdline holds the complete command line for the process, unless the
  * process is a zombie.  In the later case there is nothing in this file: that
  * is, a readon on this file will return 0 characters.  The command-line
  * arguments appear in this file as a set of null ('\0') terminated strings.
  */
char *
get_proc_argv0(pid_t pid)
{
	size_t size;

	return get_proc_file(pid, "cmdline", &size);
}

/** @brief test a client against a startup notification sequence
  * @param c - client to test
  * @param seq - sequence against which to test
  * @return Bool - True if the client matches the sequence, False otherwise
  */
Bool
test_client(Client *c, Sequence *seq)
{
	pid_t pid;
	char *str;

	if (c->startup_id) {
		if (!strcmp(c->startup_id, seq->f.id))
			return True;
		else
			return False;
	}
	if ((pid = c->pid) && (!c->hostname || strcmp(seq->f.hostname, c->hostname)))
		pid = 0;
	if (pid && (str = get_proc_startup_id(pid))) {
		if (strcmp(seq->f.id, str))
			return False;
		else
			return True;
		free(str);
	}
	/* correct wmclass */
	if (seq->f.wmclass) {
		if (c->ch.res_name && !strcmp(seq->f.wmclass, c->ch.res_name))
			return True;
		if (c->ch.res_class && !strcmp(seq->f.wmclass, c->ch.res_class))
			return True;
	}
	/* same process id */
	if (pid && atoi(seq->f.pid) == pid)
		return True;
	/* same timestamp to the millisecond */
	if (c->user_time && c->user_time == strtoul(seq->f.timestamp, NULL, 0))
		return True;
	/* correct executable */
	if (pid && seq->f.bin) {
		if ((str = get_proc_comm(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_exec(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
		if ((str = get_proc_argv0(pid)))
			if (!strcmp(seq->f.bin, str)) {
				free(str);
				return True;
			}
		free(str);
	}
	/* NOTE: we use the PID to look in: /proc/[pid]/cmdline for argv[]
	   /proc/[pid]/environ for env[] /proc/[pid]/comm basename of executable
	   /proc/[pid]/exe symbolic link to executable */
	return False;
}

/** @brief assist some clients by adding information missing from window
  *
  * Some applications do not use a full toolkit and do not properly set all of
  * the EWMH properties.  Once we have identified the startup sequence
  * associated with a client, we can set infomration on the client from the
  * startup notification sequence.
  *
  * Also do things like use /proc/[pid]/cmdline to set up WM_COMMAND if not
  * present.
  */
void
setup_client(Client *c)
{
	Sequence *seq;
	long data;

	if (!(seq = c->seq))
		return;

	/* set up _NET_STARTUP_ID */
	if (!c->startup_id && seq->f.id) {
		XChangeProperty(dpy, c->win, _XA_NET_STARTUP_ID, _XA_UTF8_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.id, strlen(seq->f.id));
	}

	/* set up _NET_WM_PID */
	if (!c->pid && seq->n.pid) {
		data = seq->n.pid;
		XChangeProperty(dpy, c->win, _XA_NET_WM_PID, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &data, 1);
		c->pid = seq->n.pid;
	}

	/* set up WM_CLIENT_MACHINE */
	if (!c->hostname && seq->f.hostname) {
		XChangeProperty(dpy, c->win, XA_WM_CLIENT_MACHINE, XA_STRING, 8,
				PropModeReplace, (unsigned char *) seq->f.hostname, strlen(seq->f.hostname));
	}

	/* set up WM_COMMAND */
	if (c->pid && !c->command) {
		char *string;
		char buf[65] = { 0, };
		size_t size;

		gethostname(buf, sizeof(buf) - 1);
		if ((c->hostname && !strcmp(buf, c->hostname)) ||
		    (seq->f.hostname && !strcmp(buf, seq->f.hostname))) {
			if ((string = get_proc_file(c->pid, "cmdline", &size))) {
				XChangeProperty(dpy, c->win, XA_WM_COMMAND,
						XA_STRING, 8, PropModeReplace, (unsigned char *) string, size);
				free(string);
			}
		}
	}
}

static Sequence *ref_sequence(Sequence *seq);

/** @brief find the startup sequence for a client
  * @param c - client to lookup startup sequence for
  * @return Sequence* - the found sequence or NULL if not found
  */
static Sequence *
find_startup_seq(Client *c)
{
	Sequence *seq;

	if ((seq = c->seq))
		return seq;

	/* search by startup id */
	if (c->startup_id) {
		for (seq = sequences; seq; seq = seq->next) {
			if (!seq->f.id) {
				EPRINTF("sequence with null id!\n");
				continue;
			}
			if (!strcmp(c->startup_id, seq->f.id))
				break;
		}
		if (!seq) {
			DPRINTF("cannot find startup id '%s'!\n", c->startup_id);
			return (seq);
		}
		DPRINTF("Found sequence by _NET_STARTUP_ID\n");
		goto found_it;
	}

	/* search by wmclass */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name && !strcmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class && !strcmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF("Found sequence by res_name or res_class\n");
			goto found_it;
		}
	}

	/* search by binary */
	if (c->command) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *binary;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((binary = seq->f.bin)) {
				if (c->command[0] && !strcmp(binary, c->command[0]))
					break;
			}

		}
		if (seq) {
			DPRINTF("Found sequence by command\n");
			goto found_it;
		}
	}

	/* search by wmclass (this time case insensitive) */
	if (c->ch.res_name || c->ch.res_class) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *wmclass;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if ((wmclass = seq->f.wmclass)) {
				if (c->ch.res_name && !strcasecmp(wmclass, c->ch.res_name))
					break;
				if (c->ch.res_class && !strcasecmp(wmclass, c->ch.res_class))
					break;
			}
		}
		if (seq) {
			DPRINTF("Found sequence by res_name or res_class\n");
			goto found_it;
		}
	}

	/* search by pid and hostname */
	if (c->pid && c->hostname) {
		for (seq = sequences; seq; seq = seq->next) {
			const char *hostname;
			pid_t pid;

			if (seq->client)
				continue;
			if (seq->state == StartupNotifyComplete)
				continue;
			if (!(pid = seq->n.pid) || !(hostname = seq->f.hostname))
				continue;
			if (c->pid == pid && !strcmp(c->hostname, hostname))
				break;
		}
		if (seq) {
			DPRINTF("Found sequence by pid and hostname\n");
			goto found_it;
		}
	}
	DPRINTF("could not find startup ID for client\n");
	return NULL;
      found_it:
	seq->client = c;

	c->seq = ref_sequence(seq);
	setup_client(c);
	return (seq);
}

/** @brief detect whether a client is a dockapp
  * @param c - client the check
  * @return Bool - true if the client is a dockapp; false, otherwise.
  */
static Bool
is_dockapp(Client *c)
{
	/* In an attempt to get around GTK+ >= 2.4.0 limitations, some GTK+ dock apps
	   simply set the res_class to "DockApp". */
	if (c->ch.res_class && !strcmp(c->ch.res_class, "DockApp"))
		return True;
	if (!c->wmh)
		return False;
	/* Many libxpm based dockapps use xlib to set the state hint correctly. */
	if ((c->wmh->flags & StateHint) && c->wmh->initial_state == WithdrawnState)
		return True;
	/* Many dockapps that were based on GTK+ < 2.4.0 are having their initial_state
	   changed to NormalState by GTK+ >= 2.4.0, so when the other flags are set,
	   accept anyway (note that IconPositionHint is optional). */
	if ((c->wmh->flags & ~IconPositionHint) == (WindowGroupHint | StateHint | IconWindowHint))
		return True;
	return False;
}

/** @brief detect whether a client is a status icon
  */
Bool
is_statusicon(Client *c)
{
	if (c->xei)
		return True;
	return False;
}

/** @brief update client from X server
  * @param c - the client to update
  *
  * Updates the client from information and properties maintained by the X
  * server.  Care should be taken that information that shoule be obtained from
  * a group window is obtained from that window first and then overwritten by
  * any information contained in the specific window.
  */
static void
update_client(Client *c)
{
#if 1
#if 1
	struct atoms *atom;
#endif

	XSaveContext(dpy, c->win, ScreenContext, (XPointer) scr);
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);

#if 1
	for (atom = wmprops; atom->name; atom++)
		if (atom->pc_handler)
			atom->pc_handler(NULL, c);
#endif
	c->new = False;
#else
	long card;
	int count = 0;

	if (c->wmh)
		XFree(c->wmh);
	c->group = None;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
	}

	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);

	XFetchName(dpy, c->win, &c->name);
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);
	}

	if (c->hostname)
		XFree(c->hostname);
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);

	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);

	free(c->startup_id);
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);

	card = 0;
	if (!get_cardinal(c->win, _XA_NET_WM_PID, XA_CARDINAL, &card) && c->group)
		get_cardinal(c->group, _XA_NET_WM_PID, XA_CARDINAL, &card);
	c->pid = card;

	find_startup_seq(c);
#if 0
	if (test_client(c))
		setup_client(c);
#endif
	XSaveContext(dpy, c->win, ClientContext, (XPointer) c);
	XSelectInput(dpy, c->win,
		     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		     FocusChangeMask | PropertyChangeMask);
	if (c->group) {
		XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->group,
			     ExposureMask | VisibilityChangeMask | StructureNotifyMask |
			     FocusChangeMask | PropertyChangeMask);
	}
	c->new = False;
#endif
}

static Client *
add_client(Window win)
{
	Client *c;

	c = calloc(1, sizeof(*c));
	c->win = win;
	c->next = scr->clients;
	scr->clients = c;
	update_client(c);
	c->new = True;
	return (c);
}

static Sequence *unref_sequence(Sequence *seq);

static void
remove_client(Client *c)
{
	Window *winp;
	int i;

	DPRINT();
	if (c->startup_id) {
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (c->name) {
		XFree(c->name);
		c->name = NULL;
	}
	if (c->hostname) {
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (c->role) {
		XFree(c->role);
		c->role = NULL;
	}
	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (c->wmh) {
		XFree(c->wmh);
		c->wmh = NULL;
	}
	if (c->wms) {
		XFree(c->wms);
		c->wms = NULL;
	}
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	for (i = 0, winp = &c->win; i < 5; i++, winp++) {
		if (*winp) {
			XDeleteContext(dpy, *winp, ScreenContext);
			XDeleteContext(dpy, *winp, ClientContext);
			XSelectInput(dpy, *winp, NoEventMask);
			*winp = None;
		}
	}
	if (c->seq && c->seq->client == c) {
		c->seq->client = NULL;

		unref_sequence(c->seq);
		c->seq = NULL;
	}
	free(c);
}

void
del_client(Client *r)
{
	Client *c, **cp;

	DPRINT();
	for (cp = &scr->clients, c = *cp; c && c != r; cp = &c->next, c = *cp) ;
	if (c == r)
		*cp = c->next;
	remove_client(r);
}

/** @brief update client from startup notification sequence
  * @param seq - the sequence that has changed state
  *
  * Update the client associated with a startup notification sequence.
  */
static void
update_startup_client(Sequence *seq)
{
	Client *c;

	if (!(c = seq->client))
		/* Note that we do not want to go searching for clients because any
		   client that we would find at this point could get a false positive
		   against an client that existed before the startup notification
		   sequence.  We could use creation timestamps to filter out the false
		   positives, but that is for later. */
		return;
	/* TODO: things to update are: _NET_WM_PID, WM_CLIENT_MACHINE, ... Note that
	   _NET_WM_STARTUP_ID should already be set. */
}

static void
convert_sequence_fields(Sequence *seq)
{
	if (seq->f.screen)
		seq->n.screen = atoi(seq->f.screen);
	if (seq->f.monitor)
		seq->n.monitor = atoi(seq->f.monitor);
	if (seq->f.desktop)
		seq->n.desktop = atoi(seq->f.desktop);
	if (seq->f.timestamp)
		seq->n.timestamp = atoi(seq->f.timestamp);
	if (seq->f.silent)
		seq->n.silent = atoi(seq->f.silent);
	if (seq->f.pid)
		seq->n.pid = atoi(seq->f.pid);
	if (seq->f.sequence)
		seq->n.sequence = atoi(seq->f.sequence);
}

static void
free_sequence_fields(Sequence *seq)
{
	int i;

	for (i = 0; i < sizeof(seq->f) / sizeof(char *); i++) {
		free(seq->fields[i]);
		seq->fields[i] = NULL;
	}
}

static void
show_sequence(Sequence *seq)
{
	char **label, **field;

	if (options.debug <= 0)
		return;
	for (label = (char **) StartupNotifyFields, field = seq->fields; *label; label++, field++) {
		if (*field)
			fprintf(stderr, "%s=%s\n", *label, *field);
		else
			fprintf(stderr, "%s=\n", *label);
	}
}

static void
copy_sequence_fields(Sequence *old, Sequence *new)
{
	int i;

	for (i = 0; i < sizeof(old->f) / sizeof(char *); i++) {
		if (new->fields[i]) {
			free(old->fields[i]);
			old->fields[i] = new->fields[i];
		}
	}
	convert_sequence_fields(old);
	DPRINTF("Updated sequence fields:\n");
	show_sequence(old);
}

static Sequence *
find_seq_by_id(char *id)
{
	Sequence *seq;

	for (seq = sequences; seq; seq = seq->next)
		if (!strcmp(seq->f.id, id))
			break;
	return (seq);
}

static void
close_sequence(Sequence *seq)
{
#ifdef HAVE_GLIB_EVENT_LOOP
	if (seq->timer) {
		DPRINTF("removing timer\n");
		g_source_remove(seq->timer);
		seq->timer = 0;
	}
#endif				/* HAVE_GLIB_EVENT_LOOP */
#ifdef SYSTEM_TRAY_STATUS_ICON
	if (seq->status) {
		DPRINTF("removing statusicon\n");
		g_object_unref(G_OBJECT(seq->status));
		seq->status = NULL;
	}
#endif				/* SYSTEM_TRAY_STATUS_ICON */
#ifdef DESKTOP_NOTIFICATIONS
	if (seq->notification) {
		DPRINTF("removing notificiation\n");
		g_object_unref(G_OBJECT(seq->notification));
		seq->notification = NULL;
	}
#endif				/* DESKTOP_NOTIFICATIONS */
}

static Sequence *
unref_sequence(Sequence *seq)
{
	if (seq) {
		if (--seq->refs <= 0) {
			Sequence *s, **prev;

			DPRINTF("deleting sequence\n");
			for (prev = &sequences, s = *prev; s && s != seq; prev = &s->next, s = *prev) ;
			if (s) {
				*prev = s->next;
				s->next = NULL;
			}
			close_sequence(seq);
			free_sequence_fields(seq);
			free(seq);
			return (NULL);
		} else
			DPRINTF("sequence still has %d references\n", seq->refs);
	} else
		EPRINTF("called with null pointer\n");
	return (seq);
}

static Sequence *
ref_sequence(Sequence *seq)
{
	if (seq)
		++seq->refs;
	return (seq);
}

static Sequence *
remove_sequence(Sequence *del)
{
	Sequence *seq, **prev;

	DPRINTF("Removing sequence:\n");
	show_sequence(del);
	for (prev = &sequences, seq = *prev; seq && seq != del; prev = &seq->next, seq = *prev) ;
	if (seq) {
		*prev = seq->next;
		seq->next = NULL;
		close_sequence(seq);
		unref_sequence(seq);
	} else
		EPRINTF("could not find sequence\n");
	return (seq);
}

static gboolean
sequence_timeout_callback(gpointer data)
{
	Sequence *seq = (typeof(seq)) data;

	if (seq->state == StartupNotifyComplete) {
		remove_sequence(seq);
		seq->timer = 0;
		return FALSE;	/* stop timeout interval */
	}
	/* for now, just generate a remove message after the guard-time */
	if (1) {
		send_remove(seq);
		seq->timer = 0;
		return G_SOURCE_REMOVE;	/* remove timeout source */
	}
	return G_SOURCE_CONTINUE;	/* continue timeout interval */
}

/** @brief add a new startup notification sequence to list for screen
  *
  * When a startup notification sequence is added, it is because we have
  * received a 'new:' or 'remove:' message.  What we want to do is to generate a
  * status icon at this point that will be updated and removed as the startup
  * notification sequence is changed or completed.  We add StartupNotifyComplete
  * sequences that start with a 'remove:' message in case the 'new:' message is
  * late.  We always add a guard timer so that the StartupNotifyComplete
  * sequence will always be removed at some point.
  */
static void
add_sequence(Sequence *seq)
{
	seq->refs = 1;
	seq->client = NULL;

	seq->next = sequences;
	sequences = seq;
	if (seq->state == StartupNotifyNew) {
	}
	seq->timer = g_timeout_add(options.guard, sequence_timeout_callback, (gpointer) seq);
	DPRINTF("Added sequence:\n");
	show_sequence(seq);
}

static void
process_startup_msg(Message *m)
{
	Sequence cmd = { NULL, }, *seq;
	char *p = m->data, *k, *v, *q, *copy, *b;
	const char **f;
	int i;
	int escaped, quoted;

	DPRINTF("Got message: %s\n", m->data);
	if (!strncmp(p, "new:", 4)) {
		cmd.state = StartupNotifyNew;
	} else if (!strncmp(p, "change:", 7)) {
		cmd.state = StartupNotifyChanged;
	} else if (!strncmp(p, "remove:", 7)) {
		cmd.state = StartupNotifyComplete;
	} else {
		free(m->data);
		free(m);
		return;
	}
	p = strchr(p, ':') + 1;
	while (*p != '\0') {
		while (*p == ' ')
			p++;
		k = p;
		if (!(v = strchr(k, '='))) {
			free_sequence_fields(&cmd);
			DPRINTF("mangled message\n");
			return;
		} else {
			*v++ = '\0';
			p = q = v;
			escaped = quoted = 0;
			for (;;) {
				if (!escaped) {
					if (*p == '"') {
						p++;
						quoted ^= 1;
						continue;
					} else if (*p == '\\') {
						p++;
						escaped = 1;
						continue;
					} else if (*p == '\0' || (*p == ' ' && !quoted)) {
						if (quoted) {
							free_sequence_fields(&cmd);
							DPRINTF("mangled message\n");
							return;
						}
						if (*p == ' ')
							p++;
						*q = '\0';
						break;
					}
				}
				*q++ = *p++;
				escaped = 0;
			}
			for (i = 0, f = StartupNotifyFields; f[i] != NULL; i++)
				if (strcmp(f[i], k) == 0)
					break;
			if (f[i] != NULL)
				cmd.fields[i] = strdup(v);
		}
	}
	free(m->data);
	free(m);
	if (!cmd.f.id) {
		free_sequence_fields(&cmd);
		DPRINTF("message with no ID= field\n");
		return;
	}
	/* get information from ID */
	do {
		p = q = copy = strdup(cmd.f.id);
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launcher)
			cmd.f.launcher = strdup(q);
		q = p;
		if (!(p = strchr(q, '/')))
			break;
		*p++ = '\0';
		while ((b = strchr(q, '|')))
			*b = '/';
		if (!cmd.f.launchee)
			cmd.f.launchee = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.pid)
			cmd.f.pid = strdup(q);
		q = p;
		if (!(p = strchr(q, '-')))
			break;
		*p++ = '\0';
		if (!cmd.f.sequence)
			cmd.f.sequence = strdup(q);
		q = p;
		if (!(p = strstr(q, "_TIME")))
			break;
		*p++ = '\0';
		if (!cmd.f.hostname)
			cmd.f.hostname = strdup(q);
		q = p + 4;
		if (!cmd.f.timestamp)
			cmd.f.timestamp = strdup(q);
	} while (0);
	free(copy);
	/* get timestamp from ID if necessary */
	if (!cmd.f.timestamp && (p = strstr(cmd.f.id, "_TIME")) != NULL)
		cmd.f.timestamp = strdup(p + 5);
	convert_sequence_fields(&cmd);
	if (!(seq = find_seq_by_id(cmd.f.id))) {
		switch (cmd.state) {
		default:
			free_sequence_fields(&cmd);
			DPRINTF("message out of sequence\n");
			return;
		case StartupNotifyNew:
		case StartupNotifyComplete:
			break;
		}
		if (!(seq = calloc(1, sizeof(*seq)))) {
			free_sequence_fields(&cmd);
			DPRINTF("no memory\n");
			return;
		}
		*seq = cmd;
		add_sequence(seq);
		return;
	}
	switch (seq->state) {
	case StartupNotifyIdle:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
			seq->state = StartupNotifyNew;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyNew:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyChanged:
		switch (cmd.state) {
		case StartupNotifyIdle:
			DPRINTF("message state error\n");
			return;
		case StartupNotifyComplete:
			seq->state = StartupNotifyComplete;
			/* remove sequence */
			break;
		case StartupNotifyNew:
		case StartupNotifyChanged:
			seq->state = StartupNotifyChanged;
			copy_sequence_fields(seq, &cmd);
			if (seq->client)
				update_startup_client(seq);
			return;
		}
		break;
	case StartupNotifyComplete:
		/* remove sequence */
		break;
	}
	/* remove sequence */
	remove_sequence(seq);
}

static void
cm_handle_NET_STARTUP_INFO_BEGIN(XClientMessageEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	DPRINT();
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m) {
		m = calloc(1, sizeof(*m));
		XSaveContext(dpy, from, MessageContext, (XPointer) m);
	}
	free(m->data);
	m->origin = from;
	m->data = calloc(21, sizeof(*m->data));
	m->len = 0;
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

static void
cm_handle_NET_STARTUP_INFO(XClientMessageEvent *e, Client *c)
{
	Window from;
	Message *m = NULL;
	int len;

	DPRINT();
	if (!e || e->type != ClientMessage)
		return;
	from = e->window;
	if (XFindContext(dpy, from, MessageContext, (XPointer *) &m) || !m)
		return;
	m->data = realloc(m->data, m->len + 21);
	/* unpack data */
	len = strnlen(e->data.b, 20);
	strncat(m->data, e->data.b, 20);
	m->len += len;
	if (len < 20) {
		XDeleteContext(dpy, from, MessageContext);
		process_startup_msg(m);
	}
}

void
clean_msgs(Window w)
{
	Message *m = NULL;

	if (XFindContext(dpy, w, MessageContext, (XPointer *) &m))
		return;

	XDeleteContext(dpy, w, MessageContext);
	free(m->data);
	free(m);
}

/** @brief perform actions necessary for a newly managed client
  * @param c - the client
  * @param t - the time that the client was managed or CurrentTime when unknown
  *
  * The client has just become managed, so try to associated it with a startup
  * notfication sequence if it has not already been associated.  If the client
  * can be associated with a startup notification sequence, then terminate the
  * sequence when it has WMCLASS set, or when SILENT is set (indicating no
  * startup notification is pending).  Otherwise, wait for the sequence to
  * complete on its own.
  *
  * If we cannot associated a sequence at this point, the launcher may be
  * defective and only sent the startup notification after the client has
  * already launched, so leave the client around for later checks.  We can
  * compare the map_time of the client to the timestamp of a later startup
  * notification to determine whether the startup notification should have been
  * sent before the client mapped.
  */
static void
managed_client(Client *c, Time t)
{
	Sequence *seq = NULL;

	c->managed = True;
	pulltime(&c->map_time, t ? : c->last_time);
	if (!(seq = c->seq) && !(seq = find_startup_seq(c)))
		return;
	switch (seq->state) {
	case StartupNotifyIdle:
	case StartupNotifyComplete:
		break;
	case StartupNotifyNew:
	case StartupNotifyChanged:
		if (!seq->f.wmclass && !seq->n.silent)
			/* We are expecting that the client will generate startup
			   notification completion on its own, so let the timers run and
			   wait for completion. */
			return;
		/* We are not expecting that the client will generate startup
		   notification completion on its own.  Either we generate the completion 
		   or wait for a supporting window manager to do so. */
		send_remove(seq);
		break;
	}
	/* FIXME: we can remove the startup notification and the client, but perhaps we
	   will just wait for the client to be unmanaged or destroyed. */
}

static void
unmanaged_client(Client *c)
{
	c->managed = False;
	/* FIXME: we can remove the client and any associated startup notification. */
}

/** @brief handle a client list of windows
  * @param e - PropertyNotify event that trigger the change
  * @param atom - the client list atom
  * @param type - the type of the list (XA_WINDOW or XA_CARDINAL)
  *
  * Process a changed client list.  Basically any client window that is on this
  * list is managed if it was not previously managed.  Also, any client that was
  * previously on the list is no longer managed when it does not appear on the
  * list any more (unless it is a dockapp or a statusicon).
  */
static void
pc_handle_CLIENT_LIST(XPropertyEvent *e, Atom atom, Atom type)
{
	Window *list;
	long i, n;
	Client *c, *cn;

	DPRINT();
	if ((list = get_windows(scr->root, atom, type, &n))) {
		for (c = scr->clients; c; c->breadcrumb = False, c->new = False, c = c->next) ;
		for (i = 0; i < n; i++) {
			if (XFindContext(dpy, list[i], ClientContext, (XPointer *) &c) == Success) {
				c->breadcrumb = True;
				c->listed = True;
				managed_client(c, e->time);
			}
		}
		XFree(list);
		for (cn = scr->clients; (c = cn);) {
			cn = c->next;
			if (!c->breadcrumb && c->listed) {
				c->listed = False;
				unmanaged_client(c);
			}
		}
	}
}

/** @brief track the active window
  * @param e - property notification event
  * @param c - client associated with e->xany.window
  *
  * This handler tracks the _NET_ACTIVE_WINDOW property on the root window set
  * by EWMH/NetWM compliant window managers to indicate the active window.  We
  * keep track of the last time that each client window was active.
  */
static void
pc_handle_NET_ACTIVE_WINDOW(XPropertyEvent *e, Client *c)
{
	Window active = None;

	DPRINT();
	if (c || !e || e->window != scr->root || e->state == PropertyDelete)
		return;
	if (get_window(scr->root, _XA_NET_ACTIVE_WINDOW, XA_WINDOW, &active) && active) {
		if (XFindContext(dpy, active, ClientContext, (XPointer *) &c) == Success) {
			pushtime(&c->active_time, e->time);
			managed_client(c, e->time);
		}
	}
}

static void
cm_handle_NET_ACTIVE_WINDOW(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	pushtime(&current_time, e->data.l[1]);
	pushtime(&c->active_time, e->data.l[1]);
	if (c->managed)
		return;
	EPRINTF("_NET_ACTIVE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, e->data.l[1]);
}

static void
pc_handle_NET_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST, XA_WINDOW);
}

static void
pc_handle_NET_CLIENT_LIST_STACKING(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_NET_CLIENT_LIST_STACKING, XA_WINDOW);
}

static void
cm_handle_NET_CLOSE_WINDOW(XClientMessageEvent *e, Client *c)
{
	Time time;

	DPRINT();
	if (!c)
		return;
	time = e ? e->data.l[0] : CurrentTime;
	pushtime(&current_time, time);
	pushtime(&c->active_time, time);

	if (c->managed)
		return;
	EPRINTF("_NET_CLOSE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, time);
}

static void
cm_handle_NET_MOVERESIZE_WINDOW(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_MOVERESIZE_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
cm_handle_NET_REQUEST_FRAME_EXTENTS(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	/* This message, unlike others, is sent before a window is initially mapped
	   (managed). */
	if (!c || !c->managed)
		return;
	EPRINTF("_NET_REQUEST_FRAME_EXTENTS for managed window 0x%lx\n", e->window);
}

static void
cm_handle_NET_RESTACK_WINDOW(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_RESTACK_WINDOW for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_STARTUP_ID(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->startup_id) {
		XFree(c->startup_id);
		c->startup_id = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->startup_id = get_text(c->win, _XA_NET_STARTUP_ID)) && c->group)
		c->startup_id = get_text(c->group, _XA_NET_STARTUP_ID);
}

static void
pc_handle_NET_SUPPORTED(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_NET_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_NET_WM_ALLOWED_ACTIONS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_ALLOWED_ACTIONS(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_FULLSCREEN_MONITORS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_FULLSCREEN_MONITORS(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_ICON_GEOMETRY(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_NET_WM_MOVERESIZE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	EPRINTF("_NET_WM_MOVERSIZE for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_NET_WM_PID(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

/** @brief handle _NET_WM_STATE property change
  *
  * Unlike WM_STATE, it is ok for the client to set this property before mapping
  * a window.  After the window is mapped it must be changd with the
  * _NET_WM_STATE client message.  There are, however, two atoms in the state:
  * _NET_WM_STATE_HIDDEN and _NET_WM_STATE_FOCUSED, that should be treated as
  * read-only by clients and will, therefore, only be set by the window manager.
  * When either of these atoms are set (or removed [TODO]), we should treat the
  * window as managed if it has not already been managed.
  *
  * The window manager is the only one responsible for deleting the property,
  * and WM-SPEC-1.5 says that it should be deleted whenever a window is
  * withdrawn but left in place when the window manager shuts down, is replaced,
  * or restarts.
  */
static void
pc_handle_NET_WM_STATE(XPropertyEvent *e, Client *c)
{
	Atom *atoms = NULL;
	long i, n;

	DPRINT();
	if (!c)
		return;
	if (e && e->state == PropertyDelete) {
		/* only removed by window manager when window withdrawn */
		unmanaged_client(c);
		return;
	}
	if ((atoms = get_atoms(c->win, _XA_NET_WM_STATE, AnyPropertyType, &n))) {
		for (i = 0; i < n; i++) {
			if (atoms[i] == _XA_NET_WM_STATE_FOCUSED) {
				pushtime(&c->focus_time, e->time);
				if (!c->managed)
					managed_client(c, e->time);
			} else if (atoms[i] == _XA_NET_WM_STATE_HIDDEN) {
				if (!c->managed)
					managed_client(c, e->time);
			}
		}
		XFree(atoms);
		if (!c->managed)
			managed_client(c, e->time);
	}
}

/** @brief handle _NET_WM_STATE client message
  *
  * If the client sends _NET_WM_STATE client messages, the client thinks that
  * the window is managed by the window manager because otherwise it would
  * simply set the property itself.  Therefore, this client message indicates
  * that the window should be managed if it has not already.
  *
  * Basically a client message is a client request to alter state while the
  * window is mapped, so mark window managed if it is not already.
  */
static void
cm_handle_NET_WM_STATE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c || c->managed)
		return;
	EPRINTF("_NET_WM_STATE sent for unmanaged window 0x%lx\n", e->window);
	managed_client(c, CurrentTime);
}

static void
pc_handle_NET_WM_USER_TIME_WINDOW(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c)
		return;
	c->time_win = None;
	if (e && e->state == PropertyDelete)
		return;
	if (get_window(c->win, _XA_NET_WM_USER_TIME_WINDOW, XA_WINDOW, &c->time_win)
	    && c->time_win) {
		Time time;

		XSaveContext(dpy, c->time_win, ScreenContext, (XPointer) scr);
		XSaveContext(dpy, c->time_win, ClientContext, (XPointer) c);
		XSelectInput(dpy, c->time_win, StructureNotifyMask | PropertyChangeMask);

		if (get_time(c->time_win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
			pushtime(&c->user_time, time);
			pushtime(&last_user_time, time);
			pushtime(&current_time, time);
		}
	}
}

static void
pc_handle_NET_WM_USER_TIME(XPropertyEvent *e, Client *c)
{
	Time time;

	DPRINT();
	if (!c || (e && e->state == PropertyDelete))
		return;
	if (get_time(c->time_win ? : c->win, _XA_NET_WM_USER_TIME, XA_CARDINAL, &time)) {
		pushtime(&c->user_time, time);
		pushtime(&last_user_time, time);
		pushtime(&current_time, time);
	}
}

/** @brief handle _NET_WM_VISIBLE_ICON_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_ICON_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	DPRINT();
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_ICON_NAME)))
		return;
	EPRINTF("_NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

/** @brief handle _NET_WM_VISIBLE_NAME property changes
  *
  * Only the window manager sets this property, so it being set indicates that
  * the window is managed.
  */
static void
pc_handle_NET_WM_VISIBLE_NAME(XPropertyEvent *e, Client *c)
{
	char *name;

	DPRINT();
	if (!c || c->managed)
		return;
	if (e && e->state == PropertyDelete)
		return;
	if (!(name = get_text(e->window, _XA_NET_WM_VISIBLE_NAME)))
		return;
	EPRINTF("_NET_WM_VISIBLE_ICON_NAME set unmanaged window 0x%lx\n", e->window);
	managed_client(c, e ? e->time : CurrentTime);
	XFree(name);
}

static void
pc_handle_WIN_APP_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_CLIENT_LIST(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (e && (e->window != scr->root || e->state == PropertyDelete))
		return;
	pc_handle_CLIENT_LIST(e, _XA_WIN_CLIENT_LIST, XA_CARDINAL);
}

static void
pc_handle_WIN_CLIENT_MOVING(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_FOCUS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_LAYER(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_WIN_LAYER(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_WIN_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (e && e->state == PropertyDelete)
		return;
	/* Not like WM_STATE and _NET_WM_STATE */
}

static void
cm_handle_WIN_STATE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WIN_SUPPORTING_WM_CHECK(XPropertyEvent *e, Client *c)
{
	DPRINT();
	handle_wmchange();
}

static void
pc_handle_WIN_WORKSPACE(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

static void
cm_handle_WIN_WORKSPACE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_SM_CLIENT_ID(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->client_id) {
		XFree(c->client_id);
		c->client_id = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->client_id = get_text(c->win, _XA_SM_CLIENT_ID)) && c->leader)
		c->client_id = get_text(c->leader, _XA_SM_CLIENT_ID);
	if (!(c->client_id) && c->group)
		c->client_id = get_text(c->group, _XA_SM_CLIENT_ID);
}

static void
pc_handle_WM_CLASS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->ch.res_name) {
		XFree(c->ch.res_name);
		c->ch.res_name = NULL;
	}
	if (c->ch.res_class) {
		XFree(c->ch.res_class);
		c->ch.res_class = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!XGetClassHint(dpy, c->win, &c->ch) && c->group)
		XGetClassHint(dpy, c->group, &c->ch);
	c->dockapp = is_dockapp(c);
}

static void
cm_handle_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_CLIENT_LEADER(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->leader = None;
	if (e && e->state == PropertyDelete)
		return;
	get_window(c->win, _XA_WM_CLIENT_LEADER, AnyPropertyType, &c->leader);
}

static void
pc_handle_WM_CLIENT_MACHINE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->hostname) {
		XFree(c->hostname);
		c->hostname = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!(c->hostname = get_text(c->win, XA_WM_CLIENT_MACHINE)) && c->group)
		c->hostname = get_text(c->group, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->leader)
		c->hostname = get_text(c->leader, XA_WM_CLIENT_MACHINE);
	if (!c->hostname && c->transient_for)
		c->hostname = get_text(c->transient_for, XA_WM_CLIENT_MACHINE);
}

static void
pc_handle_WM_COMMAND(XPropertyEvent *e, Client *c)
{
	DPRINT();
	int count = 0;

	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->command) {
		XFreeStringList(c->command);
		c->command = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	if (!XGetCommand(dpy, c->win, &c->command, &count) && c->group)
		XGetCommand(dpy, c->group, &c->command, &count);
	if (!c->command && c->leader)
		XGetCommand(dpy, c->leader, &c->command, &count);
	if (!c->command && c->transient_for)
		XGetCommand(dpy, c->transient_for, &c->command, &count);
}

static void
pc_handle_WM_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wmh) {
		XFree(c->wmh);
		c->wmh = NULL;
		c->group = None;
	}
	if (e && e->state == PropertyDelete)
		return;
	if ((c->wmh = XGetWMHints(dpy, c->win))) {
		if (c->wmh->flags & WindowGroupHint)
			c->group = c->wmh->window_group;
		if (c->group == c->win)
			c->group = None;
		if (c->group && c->group != scr->root) {
			XSaveContext(dpy, c->group, ClientContext, (XPointer) c);
			XSelectInput(dpy, c->group,
				     ExposureMask | VisibilityChangeMask |
				     StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
		}
		c->dockapp = is_dockapp(c);
	}
}

static void
pc_handle_WM_ICON_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_ICON_SIZE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_NAME(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_NORMAL_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
pc_handle_WM_PROTOCOLS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_WM_PROTOCOLS(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_SIZE_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
}

static void
cm_handle_KDE_WM_CHANGE_STATE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_STATE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->wms) {
		XFree(c->wms);
		c->wms = NULL;
	}
	if (e && e->state == PropertyDelete) {
		unmanaged_client(c);
		return;
	}
	if (!(c->wms = XGetWMState(dpy, c->win))) {
		/* XXX: about the only time this should happed would be if the window was 
		   destroyed out from under us: check that.  We should get a
		   DestroyNotify soon anyway. */
		return;
	}
	if (c->managed) {
		switch (c->wms->state) {
		case WithdrawnState:
			unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* This is merely a state transition between active states. */
			break;
		}
	} else {
		switch (c->wms->state) {
		case WithdrawnState:
			/* The window manager placed a WM_STATE of WithdrawnState on the
			   window which probably means that it is a dock app that was
			   just managed, but only for WindowMaker work-alikes. Otherwise, 
			   per ICCCM, placing withdrawn state on the window means that it 
			   is unmanaged. */
			if ((c->dockapp = is_dockapp(c)) && scr->maker_check)
				managed_client(c, e ? e->time : CurrentTime);
			else
				unmanaged_client(c);
			break;
		case NormalState:
		case IconicState:
		case ZoomState:
		case InactiveState:
		default:
			/* The window manager place a WM_STATE of other than
			   WithdrawnState on the window which means that it was just
			   managed per ICCCM. */
			managed_client(c, e ? e->time : CurrentTime);
			break;
		}
	}
}

static void
cm_handle_WM_STATE(XClientMessageEvent *e, Client *c)
{
	DPRINT();
}

static void
pc_handle_WM_TRANSIENT_FOR(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	c->transient_for = None;
	if (e && e->state == PropertyDelete)
		return;
	get_window(c->win, XA_WM_TRANSIENT_FOR, AnyPropertyType, &c->transient_for);
}

static void
pc_handle_WM_WINDOW_ROLE(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->role) {
		XFree(c->role);
		c->role = NULL;
	}
	if (e && e->state == PropertyDelete)
		return;
	c->role = get_text(c->win, _XA_WM_WINDOW_ROLE);
}

static void
pc_handle_WM_ZOOM_HINTS(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	/* we don't actually process zoom hints */
}

enum {
	XEMBED_EMBEDDED_NOTIFY,
	XEMBED_WINDOW_ACTIVATE,
	XEMBED_WINDOW_DEACTIVATE,
	XEMBED_REQUEST_FOCUS,
	XEMBED_FOCUS_IN,
	XEMBED_FOCUS_OUT,
	XEMBED_FOCUS_NEXT,
	XEMBED_FOCUS_PREV,
	XEMBED_GRAB_KEY,
	XEMBED_UNGRAB_KEY,
	XEMBED_MODALITY_ON,
	XEMBED_MODALITY_OFF,
	XEMBED_REGISTER_ACCELERATOR,
	XEMBED_UNREGISTER_ACCELERATOR,
	XEMBED_ACTIVATE_ACCELERATOR,
};

/* detail for XEMBED_FOCUS_IN: */
enum {
	XEMBED_FOCUS_CURRENT,
	XEMBED_FOCUS_FIRST,
	XEMBED_FOCUS_LAST,
};

enum {
	XEMBED_MODIFIER_SHIFT = (1 << 0),
	XEMBED_MODIFIER_CONTROL = (1 << 1),
	XEMBED_MODIFIER_ALT = (1 << 2),
	XEMBED_MODIFIER_SUPER = (1 << 3),
	XEMBED_MODIFIER_HYPER = (1 << 4),
};

enum {
	XEMBED_ACCELERATOR_OVERLOADED = (1 << 0),
};

static void
pc_handle_XEMBED_INFO(XPropertyEvent *e, Client *c)
{
	DPRINT();
	if (!c || (e && e->type != PropertyNotify))
		return;
	if (c->xei) {
		XFree(c->xei);
		c->xei = NULL;
	}
	if (e && e->state == PropertyDelete) {
		c->statusicon = False;
		return;
	}
	if ((c->xei = XGetEmbedInfo(dpy, c->win))) {
		c->dockapp = False;
		c->statusicon = True;
		return;
	}
}

/* Note:
 *	xclient.data.l[0] = timestamp
 *	xclient.data.l[1] = major opcode
 *	xclient.data.l[2] = detail code
 *	xclient.data.l[3] = data1
 *	xclient.data.l[4] = data2
 *
 * Sent to target window with no event mask and propagation false.
 */

static void
cm_handle_XEMBED(XClientMessageEvent *e, Client *c)
{
	DPRINT();
	if (!c || !e || e->type != ClientMessage)
		return;
	pushtime(&current_time, e->data.l[0]);
	if (c->managed)
		return;
	switch (e->data.l[1]) {
	case XEMBED_EMBEDDED_NOTIFY:
		/* Sent from the embedder to the client on embedding, after reparenting
		   and mapping the client's X window.  A client that receives this
		   message knows that its window was embedded by an XEmbed site and not
		   simply reparented by a window manager. The embedder's window handle is 
		   in data1. data2 is the protocol version in use. */
		managed_client(c, e->data.l[0]);
		return;
	case XEMBED_WINDOW_ACTIVATE:
	case XEMBED_WINDOW_DEACTIVATE:
		/* Sent from embedder to client when the window becomes active or
		   inactive (keyboard focus) */
		break;
	case XEMBED_REQUEST_FOCUS:
		/* Sent from client to embedder to request keyboard focus. */
		break;
	case XEMBED_FOCUS_IN:
	case XEMBED_FOCUS_OUT:
		/* Sent from embedder to client when it gets or loses focus. */
		break;
	case XEMBED_FOCUS_NEXT:
	case XEMBED_FOCUS_PREV:
		/* Sent from the client to embedder at the end or start of tab chain. */
		break;
	case XEMBED_GRAB_KEY:
	case XEMBED_UNGRAB_KEY:
		/* Obsolete */
		break;
	case XEMBED_MODALITY_ON:
	case XEMBED_MODALITY_OFF:
		/* Sent from embedder to client when shadowed or released by a modal
		   dialog. */
		break;
	case XEMBED_REGISTER_ACCELERATOR:
	case XEMBED_UNREGISTER_ACCELERATOR:
	case XEMBED_ACTIVATE_ACCELERATOR:
		/* Sent from client to embedder to register an accelerator. 'detail' is
		   the accelerator_id.  For register, data1 is the X key symbol, and
		   data2 is a bitfield of modifiers. */
		break;
	}
	/* XXX: there is a question whether we should manage the client here or not.
	   These other messages indicate that we have probably missed a management event
	   somehow, so mark it managed anyway. */
	EPRINTF("_XEMBED message for unmanaged client 0x%lx\n", e->window);
	managed_client(c, e->data.l[0]);
}

/** @brief handle MANAGER client message
  *
  * We are expecting MANAGER messages for two selections: WM_S%d and
  * _NET_SYSTEM_TRAY_S%d which correspond to window manager and system tray,
  * respectively.
  */
static void
cm_handle_MANAGER(XClientMessageEvent *e, Client *c)
{
	Window owner;
	Atom selection;
	Time time;

	DPRINT();
	if (!e || e->format != 32)
		return;
	time = e->data.l[0];
	selection = e->data.l[1];
	owner = e->data.l[2];

	(void) time;		/* FIXME: use this somehow */

	if (selection == scr->icccm_atom) {
		if (owner && owner != scr->icccm_check) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("window manager changed from 0x%lx to 0x%lx\n", scr->icccm_check, owner);
			scr->icccm_check = owner;
			handle_wmchange();
		}
	} else if (selection == scr->stray_atom) {
		if (owner && owner != scr->stray_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("system tray changed from 0x%lx to 0x%lx\n", scr->stray_owner, owner);
			scr->stray_owner = owner;
			handle_wmchange();
		}
	} else if (selection == scr->pager_atom) {
		if (owner && owner != scr->pager_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("desktop pager changed from 0x%lx to 0x%lx\n", scr->pager_owner, owner);
			scr->pager_owner = owner;
			handle_wmchange();
		}
	} else if (selection == scr->compm_atom) {
		if (owner && owner != scr->compm_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("composite manager changed from 0x%lx to 0x%lx\n", scr->compm_owner, owner);
			scr->compm_owner = owner;
			handle_wmchange();
		}
	} else if (selection == scr->shelp_atom) {
		if (owner && owner != scr->shelp_owner) {
			XSaveContext(dpy, owner, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, owner, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("startup helper changed from 0x%lx to 0x%lx\n", scr->shelp_owner, owner);
			scr->shelp_owner = owner;
			handle_wmchange();
		}
	}
}

static void
pc_handle_TIMESTAMP_PROP(XPropertyEvent *e, Client *c)
{
	DPRINT();
}

void
pc_handle_atom(XPropertyEvent *e, Client *c)
{
	pc_handler_t handle = NULL;

	if (XFindContext(dpy, e->atom, PropertyContext, (XPointer *) &handle) == Success)
		(*handle) (e, c);
	else
		DPRINTF("no PropertyNotify handler for %s\n", XGetAtomName(dpy, e->atom));
}

void
cm_handle_atom(XClientMessageEvent *e, Client *c)
{
	cm_handler_t handle = NULL;

	if (XFindContext(dpy, e->message_type, ClientMessageContext, (XPointer *) &handle)
	    == Success)
		(*handle) (e, c);
	else
		DPRINTF("no ClientMessage handler for %s\n", XGetAtomName(dpy, e->message_type));
}

/** @brief handle monitoring events
  * @param e - X event to handle
  *
  * If the window mwnager has a client list, we can check for newly mapped
  * window by additions to the client list.  We can calculate the user time by
  * tracking _NET_WM_USER_TIME and _NET_WM_TIME_WINDOW on all clients.  If the
  * window manager supports _NET_WM_STATE_FOCUSED or at least
  * _NET_ACTIVE_WINDOW, coupled with FocusIn and FocusOut events, we should be
  * able to track the last focused window at the time that the app started (not
  * all apps support _NET_WM_USER_TIME).
  */
void
handle_event(XEvent *e)
{
	Client *c;

	switch (e->type) {
	case PropertyNotify:
		XPRINTF("got PropertyNotify event\n");
		pushtime(&current_time, e->xproperty.time);
		if (!find_screen(e->xproperty.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xproperty.window);
			break;
		}
		if ((c = find_client(e->xproperty.window)))
			pushtime(&c->last_time, e->xproperty.time);
		pc_handle_atom(&e->xproperty, c);
		break;
	case FocusIn:
	case FocusOut:
		XPRINTF("got FocusIn/FocusOut event\n");
		if (!find_screen(e->xfocus.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xfocus.window);
			break;
		}
		if ((c = find_client(e->xfocus.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now... */
			managed_client(c, CurrentTime);
		break;
	case Expose:
		XPRINTF("got Expose event\n");
		if (!find_screen(e->xexpose.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xexpose.window);
			break;
		}
		if ((c = find_client(e->xexpose.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now... */
			managed_client(c, CurrentTime);
		break;
	case VisibilityNotify:
		XPRINTF("got VisibilityNotify event\n");
		if (!find_screen(e->xvisibility.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xvisibility.window);
			break;
		}
		if ((c = find_client(e->xvisibility.window)) && !c->managed)
			/* We missed a management event, so treat the window as managed
			   now. */
			managed_client(c, CurrentTime);
		break;
	case CreateNotify:
		XPRINTF("got CreateNotify event\n");
		/* only interested in top-level windows that are not override redirect */
		if (e->xcreatewindow.override_redirect)
			break;
		if (!find_screen(e->xcreatewindow.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xcreatewindow.window);
			break;
		}
		if (!(c = find_client(e->xcreatewindow.window)))
			if (e->xcreatewindow.parent == scr->root)
				c = add_client(e->xcreatewindow.window);
		break;
	case DestroyNotify:
		XPRINTF("got DestroyNotify event\n");
		if (!find_screen(e->xdestroywindow.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xdestroywindow.window);
			break;
		}
		if ((c = find_client(e->xdestroywindow.window)))
			del_client(c);
		clean_msgs(e->xdestroywindow.window);
		break;
	case MapNotify:
		XPRINTF("got MapNotify event\n");
		if (e->xmap.override_redirect)
			break;
		if (!find_screen(e->xmap.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xmap.window);
			break;
		}
		if ((c = find_client(e->xmap.window)) && !c->managed)
			/* Not sure we should do this for anything but dockapps here...
			   Window managers may map normal windows before setting the
			   WM_STATE property, and system trays invariably map window
			   before sending the _XEMBED message. */
			if ((c->dockapp = is_dockapp(c)))
				/* We missed a management event, so treat the window as
				   managed now */
				managed_client(c, CurrentTime);
		break;
	case UnmapNotify:
		XPRINTF("got UnmapNotify event\n");
		/* we can't tell much from a simple unmap event */
		break;
	case ReparentNotify:
		XPRINTF("got ReparentNotify event\n");
		/* any top-level window that is reparented by the window manager should
		   have WM_STATE set eventually (either before or after the reparenting), 
		   or receive an _XEMBED client message it they are a status icon.  The
		   exception is dock apps: some window managers reparent the dock app and 
		   never set WM_STATE on it (even though WindowMaker does). */
		if (e->xreparent.override_redirect)
			break;
		if (!find_screen(e->xreparent.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xreparent.window);
			break;
		}
		if ((c = find_client(e->xreparent.window)) && !c->managed) {
			if (e->xreparent.parent == scr->root)
				/* reparented the wrong way */
				break;
			if ((c->statusicon = is_statusicon(c))) {
				/* This is a status icon. */
				if (scr->stray_owner)
					/* Status icons will receive an _XEMBED message
					   from the system tray when managed. */
					break;
				/* It is questionable whether status icons will be
				   reparented at all when there is no system tray... */
				EPRINTF("status icon 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else if (!(c->dockapp = is_dockapp(c))) {
				/* This is a normal window. */
				if (scr->redir_check)
					/* We will receive WM_STATE property change when
					   managed (if there is a window manager
					   present). */
					break;
				/* It is questionable whether regular windows will be
				   reparented at all when there is no window manager... */
				EPRINTF("normal window 0x%lx reparented to 0x%lx\n",
					e->xreparent.window, e->xreparent.parent);
				break;
			} else {
				/* This is a dock app. */
				if (scr->maker_check)
					/* We will receive a WM_STATE property change
					   when managed when a WindowMaker work-alike is
					   present. */
					break;
				/* Non-WindowMaker window managers reparent dock apps
				   without any further indication that the dock app has
				   been managed. */
				managed_client(c, CurrentTime);
			}
		}
		break;
	case ConfigureNotify:
		XPRINTF("got ConfigureNotify event\n");
		break;
	case ClientMessage:
		XPRINTF("got ClientMessage event\n");
		if (!find_screen(e->xclient.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xclient.window);
		}
		c = find_client(e->xclient.window);
		cm_handle_atom(&e->xclient, c);
		break;
	case MappingNotify:
		XPRINTF("got MappingNotify event\n");
	default:
		EPRINTF("unexpected xevent %d\n", (int) e->type);
		break;
	case SelectionClear:
		XPRINTF("got SelectionClear event\n");
#if 0
		int s;
#endif

		if (!find_screen(e->xselectionclear.window)) {
			EPRINTF("could not find screen for window 0x%lx\n", e->xselectionclear.window);
			break;
		}
		if (e->xselectionclear.selection != scr->slctn_atom)
			break;
		if (e->xselectionclear.window != scr->selwin)
			break;
		XDestroyWindow(dpy, scr->selwin);
		scr->selwin = None;
#if 0
		for (s = 0; s < nscr; s++)
			if (xscreens[s].selwin)
				break;
		if (s < nscr)
			break;
#endif
		DPRINTF("lost " SELECTION_ATOM " selection: exiting\n", scr->screen);
		exit(EXIT_SUCCESS);
	}
}

#if 0
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
#endif

volatile int signum = 0;

void
sighandler(int sig)
{
	signum = sig;
}

static void
autoSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown, int interactStyle, Bool fast)
{
}

static void
autoDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	exit(EXIT_SUCCESS);
}

static void
autoSaveCompleteCB(SmcConn smcConn, SmPointer data)
{
}

static void
autoShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	/* nothing to do really */
}

static IceConn iceConn;
static SmcConn smcConn;

static unsigned long autoCBMask =
    SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

static SmcCallbacks autoCBs = {
	.save_yourself = {
			  .callback = &autoSaveYourselfCB,
			  .client_data = NULL,
			  },
	.die = {
		.callback = &autoDieCB,
		.client_data = NULL,
		},
	.save_complete = {
			  .callback = &autoSaveCompleteCB,
			  .client_data = NULL,
			  },
	.shutdown_cancelled = {
			       .callback = &autoShutdownCancelledCB,
			       .client_data = NULL,
			       },
};

static gboolean
on_proxy_ifd_watch(GIOChannel *chan, GIOCondition cond, pointer data)
{
	SmcConn smcConn = (typeof(smcConn)) data;
	IceConn iceConn = SmcGetIceConnection(smcConn);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceProcessMessages(iceConn, NULL, NULL);
	}
	return G_SOURCE_CONTINUE;	/* keep event source */
}

void
init_proxy(void)
{
	char err[256] = { 0, };
	GIOChannel *chan;
	int ifd, mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	char *env;

	if (smcConn) {
		DPRINTF("already connected\n");
		return;
	}
	if (!(env = getenv("SESSION_MANAGER"))) {
		if (options.clientId)
			EPRINTF("clientId provided but no SESSION_MANAGER\n");
		return;
	}

	smcConn = SmcOpenConnection(env, NULL, SmProtoMajor, SmProtoMinor,
				    autoCBMask, &autoCBs, options.clientId, &options.clientId, sizeof(err), err);

	if (!smcConn) {
		EPRINTF("SmcOpenConnection: %s\n", err);
		exit(EXIT_FAILURE);
	}

	iceConn = SmcGetIceConnection(smcConn);

	ifd = IceConnectionNumber(iceConn);
	chan = g_io_channel_unix_new(ifd);
	g_io_add_watch(chan, mask, on_proxy_ifd_watch, smcConn);
}

/** Wait for Resources */
/** @{ */

/** @brief Check for a system tray.
  */
static Window
check_stray()
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->stray_atom))) {
		if (win != scr->stray_owner) {
			XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
			DPRINTF("system tray changed from 0x%08lx to 0x%08lx\n", scr->stray_owner, win);
			scr->stray_owner = win;
		}
	} else if (scr->stray_owner) {
		DPRINTF("system tray removed from 0x%08lx\n", scr->stray_owner);
		scr->stray_owner = None;
	}
	return scr->stray_owner;
}

static Window
check_pager(void)
{
	Window win;
	long *cards, n = 0;

	if ((win = XGetSelectionOwner(dpy, scr->pager_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	/* selection only held while setting _NET_DESKTOP_LAYOUT */
	if (!win && (cards = get_cardinals(scr->root, _XA_NET_DESKTOP_LAYOUT, XA_CARDINAL, &n))
	    && n >= 4) {
		XFree(cards);
		win = scr->root;
	}
	if (win && win != scr->pager_owner)
		DPRINTF("desktop pager changed from 0x%08lx to 0x%08lx\n", scr->pager_owner, win);
	if (!win && scr->pager_owner)
		DPRINTF("desktop pager removed from 0x%08lx\n", scr->pager_owner);
	return (scr->pager_owner = win);
}

static Window
check_compm(void)
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->compm_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != scr->compm_owner)
		DPRINTF("startup helper changed from 0x%08lx to 0x%08lx\n", scr->compm_owner, win);
	if (!win && scr->compm_owner)
		DPRINTF("startup helper removed from 0x%08lx\n", scr->compm_owner);
	return (scr->compm_owner = win);
}

static Window
check_audio(void)
{
	char *text;

	if (!(text = get_text(scr->root, _XA_PULSE_COOKIE)))
		return (scr->audio_owner = None);
	XFree(text);
	if (!(text = get_text(scr->root, _XA_PULSE_SERVER)))
		return (scr->audio_owner = None);
	XFree(text);
	if (!(text = get_text(scr->root, _XA_PULSE_ID)))
		return (scr->audio_owner = None);
	XFree(text);
	return (scr->audio_owner = scr->root);
}

static Window
check_shelp(void)
{
	Window win;

	if ((win = XGetSelectionOwner(dpy, scr->shelp_atom)))
		XSelectInput(dpy, win, StructureNotifyMask | PropertyChangeMask);
	if (win && win != scr->shelp_owner)
		DPRINTF("startup helper changed from 0x%08lx to 0x%08lx\n", scr->shelp_owner, win);
	if (!win && scr->shelp_owner)
		DPRINTF("startup helper removed from 0x%08lx\n", scr->shelp_owner);
	return (scr->shelp_owner = win);
}

static Bool
wait_for_condition(Window (*until) (void))
{
	int xfd;
	XEvent ev;

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGALRM, sighandler);

	/* main event loop */
	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	while (running) {
		struct pollfd pfd = { xfd, POLLIN | POLLHUP | POLLERR, 0 };

		if (signum) {
			if (signum == SIGALRM)
				return True;
			exit(EXIT_SUCCESS);	/* XXX */
		}
		relax();
		if (poll(&pfd, 1, -1) == -1) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
			case ERESTART:
				continue;
			}
			EPRINTF("poll: %s\n", strerror(errno));
			exit(EXIT_FAILURE);	/* XXX */
		}
		if (pfd.revents & (POLLIN)) {
			while (XPending(dpy) && running) {
				XNextEvent(dpy, &ev);
				handle_event(&ev);
				if (until())
					return True;
			}
		}
	}
	return False;
}

static Window
check_anywm(void)
{
	if (scr->netwm_check)
		return scr->netwm_check;
	if (scr->winwm_check)
		return scr->winwm_check;
	if (scr->maker_check)
		return scr->maker_check;
	if (scr->motif_check)
		return scr->motif_check;
	if (scr->icccm_check)
		return scr->icccm_check;
	if (scr->redir_check)
		return scr->redir_check;
	return None;
}

static Bool
check_for_window_manager(void)
{
	DPRINTF("checking NetWM/EWMH compliance\n");
	if (check_netwm())
		DPRINTF("NetWM/EWMH window 0x%08lx\n", scr->netwm_check);
	DPRINTF("checking GNOME/WMH compliance\n");
	if (check_winwm())
		DPRINTF("GNOME/WMH window 0x%08lx\n", scr->winwm_check);
	DPRINTF("checking WindowMaker compliance\n");
	if (check_netwm())
		DPRINTF("WindowMaker window 0x%08lx\n", scr->maker_check);
	DPRINTF("checking OSF/Motif compliance\n");
	if (check_netwm())
		DPRINTF("OSF/Motif window 0x%08lx\n", scr->motif_check);
	DPRINTF("checking ICCCM 2.0 compliance\n");
	if (check_netwm())
		DPRINTF("ICCCM 2.0 window 0x%08lx\n", scr->icccm_check);
	DPRINTF("checking redirection\n");
	if (check_netwm())
		DPRINTF("Redirection on window 0x%08lx\n", scr->redir_check);
	return check_anywm() ? True : False;
}

static void
wait_for_window_manager(void)
{
	if (check_for_window_manager()) {
		DPRINTF("Have window manager\n");
	} else {
		DPRINTF("Waiting for window manager\n");
		wait_for_condition(&check_anywm);
	}
}

static void
wait_for_system_tray(void)
{
	if (check_stray()) {
		DPRINTF("Have system tray\n");
	} else {
		DPRINTF("Waiting for system tray\n");
		wait_for_condition(&check_stray);
	}
}

static void
wait_for_desktop_pager(void)
{
	if (check_pager()) {
		DPRINTF("Have desktop pager\n");
	} else {
		DPRINTF("Waiting for desktop pager\n");
		wait_for_condition(&check_pager);
	}
}

static void
wait_for_composite_manager(void)
{
	if (check_compm()) {
		DPRINTF("Have composite manager\n");
	} else {
		DPRINTF("Waiting for composite manager\n");
		wait_for_condition(&check_compm);
	}
}

static void
wait_for_audio_server(void)
{
	if (check_audio()) {
		DPRINTF("Have audio server\n");
	} else {
		DPRINTF("Waiting for audio server\n");
		wait_for_condition(&check_audio);
	}
}

static void
wait_for_startup_helper(void)
{
	if (check_shelp()) {
		DPRINTF("Have startup helper\n");
	} else {
		DPRINTF("Waiting for startup helper\n");
		wait_for_condition(&check_shelp);
	}
}

void
wait_for_resources(int need)
{
	if (need & WaitForHelper)
		wait_for_startup_helper();
	if (need & WaitForAudio)
		wait_for_audio_server();
	if (need & WaitForManager)
		wait_for_window_manager();
	if (need & WaitForComposite)
		wait_for_composite_manager();
	if (need & WaitForTray)
		wait_for_system_tray();
	if (need & WaitForPager)
		wait_for_desktop_pager();
}

/** @} */

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
do_startwm(int argc, char *argv[])
{
}

void
do_session(int argc, char *argv[])
{
}

const char *
showBool(Bool boolean)
{
	if (boolean)
		return ("True");
	return ("False");
}

typedef struct {
	int index;
	GtkWidget *align;		/* alignment widget at center of
					   monitor */
	GdkRectangle geom;		/* monitor geometry */
} XdeMonitor;

typedef struct {
	int index;			/* index */
	GdkScreen *scrn;		/* screen */
	GtkWidget *wind;		/* covering window for screen */
	gint width;			/* width of screen */
	gint height;			/* height of screen */
	gint nmon;			/* number of monitors */
	XdeMonitor *mons;		/* monitors for this screen */
	GdkPixmap *pixmap;		/* pixmap for background image */
	GdkPixbuf *pixbuf;		/* pixbuf for background image */
#ifdef DO_XLOCKING
	XScreenSaverInfo info;		/* screen saver info for this screen */
	char selection[32];
	Window selwin;
#endif
} XdeScreen;

XdeScreen *screens;

#ifdef DO_XLOCKING
typedef struct {
	int xfd;
	Display *dpy;
} XdeDisplay;

XdeDisplay display;

const char *
show_state(int state)
{
	switch (state) {
	case ScreenSaverOff:
		return ("Off");
	case ScreenSaverOn:
		return ("On");
	case ScreenSaverDisabled:
		return ("Disabled");
	default:
		return ("(unknown)");
	}
}

const char *
show_kind(int kind)
{
	switch (kind) {
	case ScreenSaverBlanked:
		return ("Blanked");
	case ScreenSaverInternal:
		return ("Internal");
	case ScreenSaverExternal:
		return ("External");
	default:
		return ("(unknown)");
	}
}

void
setup_screensaver(void)
{
	Display *dpy = display.dpy;
	int s, nscr = ScreenCount(dpy);
	char **list, **ext;
	int n, next = 0;
	Bool gotext = False;
	Bool present;
	Status status;
	XdeScreen *xscr;

	if ((list = XListExtensions(dpy, &next)) && next)
		for (n = 0, ext = list; n < next; n++, ext++)
			if (!strcmp(*ext, "MIT-SCREEN-SAVER"))
				gotext = True;
	if (!gotext) {
		DPRINTF("no MIT-SCREEN-SAVER extension\n");
		return;
	}
	if (!(present = XScreenSaverQueryExtension(dpy, &xssEventBase, &xssErrorBase))) {
		DPRINTF("MIT-SCREEN-SAVER extension not present\n");
		return;
	} else {
		DPRINTF("xssEventBase = %d\n", xssEventBase);
		DPRINTF("xssErrorBase = %d\n", xssErrorBase);
	}
	if (!(status = XScreenSaverQueryVersion(dpy, &xssMajorVersion, &xssMinorVersion))) {
		DPRINTF("cannot query MIT-SCREEN-SAVER version\n");
		return;
	} else {
		DPRINTF("xssMajorVersion = %d\n", xssMajorVersion);
		DPRINTF("xssMinorVersion = %d\n", xssMinorVersion);
	}
	for (s = 0, xscr = screens; s < nscr; s++, xscr++) {
		XScreenSaverQueryInfo(dpy, RootWindow(dpy, s), &xscr->info);
		if (options.debug > 1) {
			fprintf(stderr, "Before:\n");
			fprintf(stderr, "\twindow:\t\t0x%08lx\n", xscr->info.window);
			fprintf(stderr, "\tstate:\t\t%s\n", show_state(xscr->info.state));
			fprintf(stderr, "\tkind:\t\t%s\n", show_kind(xscr->info.kind));
			fprintf(stderr, "\ttil_or_since:\t%lu\n", xscr->info.til_or_since);
			fprintf(stderr, "\tidle:\t\t%lu\n", xscr->info.idle);
			fprintf(stderr, "\teventMask:\t0x%08lx\n", xscr->info.eventMask);
		}
		XScreenSaverSelectInput(dpy, RootWindow(dpy, s),
					ScreenSaverNotifyMask | ScreenSaverCycleMask);
		XScreenSaverQueryInfo(dpy, RootWindow(dpy, s), &xscr->info);
		if (options.debug > 1) {
			fprintf(stderr, "After:\n");
			fprintf(stderr, "\twindow:\t\t0x%08lx\n", xscr->info.window);
			fprintf(stderr, "\tstate:\t\t%s\n", show_state(xscr->info.state));
			fprintf(stderr, "\tkind:\t\t%s\n", show_kind(xscr->info.kind));
			fprintf(stderr, "\ttil_or_since:\t%lu\n", xscr->info.til_or_since);
			fprintf(stderr, "\tidle:\t\t%lu\n", xscr->info.idle);
			fprintf(stderr, "\teventMask:\t0x%08lx\n", xscr->info.eventMask);
		}
	}
}

#endif /* DO_XLOCKING */

GDBusProxy *sd_manager = NULL;
GDBusProxy *sd_session = NULL;
GDBusProxy *sd_display = NULL;

#ifdef DO_XLOCKING
static void LockScreen(gboolean hard);
static void UnlockScreen(void);
static void AbortLockScreen(void);
static void AutoLockScreen(void);
static void SystemLockScreen(void);
#endif

void
on_sd_prox_manager_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
			  GVariant *parameters, gpointer user_data)
{
	DPRINTF("received manager proxy signal %s( %s )\n", signal_name,
		g_variant_get_type_string(parameters));

	if (!strcmp(signal_name, "PrepareForSleep")) {
	} else if (!strcmp(signal_name, "PrepareForShutdown")) {
	}
}

void
on_sd_prox_session_signal(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
			  GVariant *parameters, gpointer user_data)
{
	DPRINTF("received session proxy signal %s( %s )\n", signal_name,
		g_variant_get_type_string(parameters));
#ifdef DO_XLOCKING
	if (!strcmp(signal_name, "Lock")) {
		DPRINTF("locking screen due to systemd request\n");
		SystemLockScreen();
	} else if (!strcmp(signal_name, "Unlock")) {
		DPRINTF("unlocking screen due to systemd request\n");
		UnlockScreen();
	}
#endif
}

void
on_sd_prox_session_props_changed(GDBusProxy *proxy, GVariant *changed_properties,
				 GStrv invalidated_properties, gpointer user_data)
{
	GVariantIter iter;
	GVariant *prop;

	DPRINTF("received session proxy properties changed signal ( %s )\n",
		g_variant_get_type_string(changed_properties));

	g_variant_iter_init(&iter, changed_properties);
	while ((prop = g_variant_iter_next_value(&iter))) {
		if (g_variant_is_container(prop)) {
			GVariantIter iter2;
			GVariant *key;
			GVariant *val;
			GVariant *boxed;
			const gchar *name;

			g_variant_iter_init(&iter2, prop);
			if (!(key = g_variant_iter_next_value(&iter2))) {
				EPRINTF("no key!\n");
				continue;
			}
			if (!(name = g_variant_get_string(key, NULL))) {
				EPRINTF("no name!\n");
				g_variant_unref(key);
				continue;
			}
			if (strcmp(name, "Active")) {
				DPRINTF("not looking for %s\n", name);
				g_variant_unref(key);
				continue;
			}
			if (!(val = g_variant_iter_next_value(&iter2))) {
				EPRINTF("no val!\n");
				g_variant_unref(key);
				continue;
			}
			if (!(boxed = g_variant_get_variant(val))) {
				EPRINTF("no value!\n");
				g_variant_unref(key);
				g_variant_unref(val);
				continue;
			}
			if (!g_variant_get_boolean(boxed)) {
#ifdef DO_XLOCKING
				DPRINTF("went inactive, locking screen\n");
				DPRINTF("locking screen due to systemd active\n");
				SystemLockScreen();
#endif
			}
			g_variant_unref(key);
			g_variant_unref(val);
			g_variant_unref(boxed);
			break;
		}
		g_variant_unref(prop);
	}
}

void
setup_systemd(void)
{
	GError *err = NULL;
	gchar *s;
	const char *env;

	DPRINT();
	if (!(sd_manager =
	      g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL, "org.freedesktop.login1",
					    "/org/freedesktop/login1",
					    "org.freedesktop.login1.Manager", NULL, &err)) || err) {
		EPRINTF("could not create DBUS proxy sd_manager: %s\n",
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_signal_connect(G_OBJECT(sd_manager), "g-signal",
			 G_CALLBACK(on_sd_prox_manager_signal), NULL);
	s = g_strdup_printf("/org/freedesktop/login1/session/%s", getenv("XDG_SESSION_ID") ? : "self");
	if (!(sd_session =
	      g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL, "org.freedesktop.login1", s,
					    "org.freedesktop.login1.Session", NULL, &err)) || err) {
		EPRINTF("could not create DBUS proxy sd_session: %s\n",
			err ? err->message : NULL);
		g_clear_error(&err);
		g_free(s);
		return;
	}
	g_signal_connect(G_OBJECT(sd_session), "g-signal",
			 G_CALLBACK(on_sd_prox_session_signal), NULL);
	g_signal_connect(G_OBJECT(sd_session), "g-properties-changed",
			 G_CALLBACK(on_sd_prox_session_props_changed), NULL);
	g_free(s);
	if ((env = getenv("XDG_SEAT_PATH")))
		s = g_strdup(env);
	else if ((env = getenv("XDG_SEAT")))
		s = g_strdup_printf("/org/freedesktop/DisplayManager/%s", env);
	else
		s = g_strdup("/org/freedesktop/DisplayManager/Seat0");
	if (!(sd_display =
	      g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL,
					    "org.freedesktop.DisplayManager", s,
					    "org.freedesktop.DisplayManager.Seat", NULL, &err))
	    || err) {
		EPRINTF("counld not create DBUS proxy sd_display: %s\n",
			err ? err->message : NULL);
		g_clear_error(&err);
		g_free(s);
		return;
	}
	g_free(s);
}

#ifdef DO_XLOCKING

void
setidlehint(gboolean flag)
{
	GError *err = NULL;
	GVariant *result;

	if (!sd_session) {
		EPRINTF("No session proxy!\n");
		return;
	}
	if (!(result =
	      g_dbus_proxy_call_sync(sd_session, "SetIdleHint", g_variant_new("(b)", flag),
				     G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err)) || err) {
		EPRINTF("SetIdleHint: %s: call failed: %s\n", getenv("XDG_SESSION_ID") ? : "self",
			err ? err->message : NULL);
		g_clear_error(&err);
		return;
	}
	g_variant_unref(result);
}

GdkFilterReturn
handle_XScreenSaverNotify(Display *dpy, XEvent *xev)
{
	XScreenSaverNotifyEvent *ev = (typeof(ev)) xev;

	DPRINT();

	if (options.debug > 1) {
		fprintf(stderr, "==> XScreenSaverNotify:\n");
		fprintf(stderr, "    --> send_event = %s\n", showBool(ev->send_event));
		fprintf(stderr, "    --> window = 0x%lx\n", ev->window);
		fprintf(stderr, "    --> root = 0x%lx\n", ev->root);
		fprintf(stderr, "    --> state = %s\n", xssState(ev->state));
		fprintf(stderr, "    --> kind = %s\n", xssKind(ev->kind));
		fprintf(stderr, "    --> forced = %s\n", showBool(ev->forced));
		fprintf(stderr, "    --> time = %lu\n", ev->time);
		fprintf(stderr, "<== XScreenSaverNotify:\n");
	}
	switch (ev->state) {
	case ScreenSaverDisabled:
	default:
		break;
	case ScreenSaverOff:
		/* Note that if we were not locked for more than a few seconds we should
		   simply unlock the screen.  This way if the user presses a key or
		   nudges the mouse fast enought when the screen blanks, they will not
		   have to reenter a password.  This could be coupled with screen fading
		   during the grace period.  */
		setidlehint(FALSE);
		AbortLockScreen();
		break;
	case ScreenSaverOn:
		DPRINTF("auto locking screen to due to screen-saver on\n");
		setidlehint(TRUE);
		AutoLockScreen();
		break;
	case ScreenSaverCycle:
		DPRINTF("auto locking screen to due to screen-saver cycle\n");
		AutoLockScreen();
		break;
	}
	return G_SOURCE_CONTINUE;
}
#endif				/* DO_XLOCKING */

#ifdef DO_XCHOOSER
#define PING_TRIES	3
#define PING_INTERVAL	2	/* 2 seconds */

XdmcpBuffer directBuffer;
XdmcpBuffer broadcastBuffer;

static gpointer
sockaddr_copy_func(gpointer boxed)
{
	struct sockaddr_storage *sa = boxed;
	struct sockaddr_storage *na = NULL;

	if (sa && (na = calloc(1, sizeof(*na))))
		memmove(na, sa, sizeof(*na));
	return (na);
}

static void
sockaddr_free_func(gpointer boxed)
{
	free(boxed);
}

static GType
g_sockaddr_get_type(void)
{
	static int initialized = 0;
	static GType mytype;

	if (!initialized) {
		mytype = g_boxed_type_register_static("sockaddr",
						      sockaddr_copy_func, sockaddr_free_func);
		initialized = 1;
	}
	return mytype;
}

#undef G_TYPE_SOCKADDR
#define G_TYPE_SOCKADDR (g_sockaddr_get_type())

enum {
	XDM_COL_HOSTNAME,		/* the manager hostname */
	XDM_COL_REMOTENAME,		/* the manager remote name */
	XDM_COL_WILLING,		/* the willing status */
	XDM_COL_STATUS,			/* the status */
	XDM_COL_IPADDR,			/* the ip address */
	XDM_COL_CTYPE,			/* the connection type */
	XDM_COL_SERVICE,		/* the service */
	XDM_COL_PORT,			/* the port number */
	XDM_COL_MARKUP,			/* the combined markup description */
	XDM_COL_TOOLTIP,		/* the tooltip information */
	XDM_COL_SOCKADDR,		/* the socket address */
	XDM_COL_SCOPE,			/* the socket address scope */
	XDM_COL_IFINDEX,		/* the socket interface index */
};
#endif				/* DO_XCHOOSER */

#ifdef DO_LOGOUT
typedef enum {
	AvailStatusUndef,		/* undefined */
	AvailStatusUnknown,		/* not known */
	AvailStatusNa,			/* not available */
	AvailStatusNo,			/* available not permitted */
	AvailStatusChallenge,		/* available with password */
	AvailStatusYes,			/* available and permitted */
} AvailStatus;

AvailStatus
status_of_string(const char *string)
{
	if (!string)
		return AvailStatusUndef;
	if (!string || !strcmp(string, "na"))
		return AvailStatusNa;
	if (!strcmp(string, "no"))
		return AvailStatusNo;
	if (!strcmp(string, "yes"))
		return AvailStatusYes;
	if (!strcmp(string, "challenge"))
		return AvailStatusChallenge;
	EPRINTF("unknown availability status %s\n", string);
	return AvailStatusUnknown;
}

static AvailStatus action_can[LOGOUT_ACTION_COUNT] = {
	/* *INDENT-OFF* */
	[LOGOUT_ACTION_POWEROFF]	= AvailStatusUndef,
	[LOGOUT_ACTION_REBOOT]		= AvailStatusUndef,
	[LOGOUT_ACTION_SUSPEND]		= AvailStatusUndef,
	[LOGOUT_ACTION_HIBERNATE]	= AvailStatusUndef,
	[LOGOUT_ACTION_HYBRIDSLEEP]	= AvailStatusUndef,
	[LOGOUT_ACTION_SWITCHUSER]	= AvailStatusUndef,
	[LOGOUT_ACTION_SWITCHDESK]	= AvailStatusUndef,
	[LOGOUT_ACTION_LOCKSCREEN]	= AvailStatusUndef,
	[LOGOUT_ACTION_CHECKPOINT]	= AvailStatusUndef,
	[LOGOUT_ACTION_SHUTDOWN]	= AvailStatusUndef,
	[LOGOUT_ACTION_LOGOUT]		= AvailStatusUndef,
	[LOGOUT_ACTION_RESTART]		= AvailStatusUndef,
	[LOGOUT_ACTION_CANCEL]		= AvailStatusUndef,
	/* *INDENT-ON* */
};
#endif				/* DO_LOGOUT */

enum {
	XSESS_COL_PIXBUF,		/* the icon name for the pixbuf */
	XSESS_COL_NAME,			/* the Name= of the XSession */
	XSESS_COL_COMMENT,		/* the Comment= of the XSession */
	XSESS_COL_MARKUP,		/* Combined Name/Comment markup */
	XSESS_COL_LABEL,		/* the label (appid) of the XSession */
	XSESS_COL_MANAGED,		/* XDE-Managed? editable setting */
	XSESS_COL_ORIGINAL,		/* XDE-Managed? original setting */
	XSESS_COL_FILENAME,		/* the full file name */
	XSESS_COL_TOOLTIP,		/* the tooltip information */
};

static void reparse(Display *dpy, Window root);
static void redo_source(XdeScreen *xscr);

static GdkFilterReturn
event_handler_PropertyNotify(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	DPRINT();
	if (options.debug > 2) {
		fprintf(stderr, "==> PropertyNotify:\n");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xproperty.window);
		fprintf(stderr, "    --> atom = %s\n", XGetAtomName(dpy, xev->xproperty.atom));
		fprintf(stderr, "    --> time = %ld\n", xev->xproperty.time);
		fprintf(stderr, "    --> state = %s\n",
			(xev->xproperty.state == PropertyNewValue) ? "NewValue" : "Delete");
		fprintf(stderr, "<== PropertyNotify:\n");
	}
	if (xev->xproperty.atom == _XA_XDE_THEME_NAME && xev->xproperty.state == PropertyNewValue) {
		DPRINT();
		reparse(dpy, xev->xproperty.window);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	if (xev->xproperty.atom == _XA_XROOTPMAP_ID && xev->xproperty.state == PropertyNewValue) {
		DPRINT();
		if (options.source & BackgroundSourcePixmap)
			redo_source(xscr);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
event_handler_ClientMessage(Display *dpy, XEvent *xev)
{
	DPRINT();
	if (options.debug > 1) {
		fprintf(stderr, "==> ClientMessage:\n");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xclient.window);
		fprintf(stderr, "    --> message_type = %s\n",
			XGetAtomName(dpy, xev->xclient.message_type));
		fprintf(stderr, "    --> format = %d\n", xev->xclient.format);
		switch (xev->xclient.format) {
			int i;

		case 8:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 20; i++)
				fprintf(stderr, " %02x", xev->xclient.data.b[i]);
			fprintf(stderr, "\n");
			break;
		case 16:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 10; i++)
				fprintf(stderr, " %04x", xev->xclient.data.s[i]);
			fprintf(stderr, "\n");
			break;
		case 32:
			fprintf(stderr, "    --> data =");
			for (i = 0; i < 5; i++)
				fprintf(stderr, " %08lx", xev->xclient.data.l[i]);
			fprintf(stderr, "\n");
			break;
		}
		fprintf(stderr, "<== ClientMessage:\n");
	}
	if (xev->xclient.message_type == _XA_GTK_READ_RCFILES) {
		reparse(dpy, xev->xclient.window);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
#ifdef DO_XLOCKING
	if (xev->xclient.message_type == _XA_XDE_XLOCK_COMMAND) {
		switch (xev->xclient.data.l[0]) {
		case LockCommandLock:
			DPRINTF("locking screen due to xclient message\n");
			LockScreen(TRUE);
			return GDK_FILTER_REMOVE;
		case LockCommandUnlock:
			DPRINTF("unlocking screen due to xclient message\n");
			UnlockScreen();
			return GDK_FILTER_REMOVE;
		case LockCommandQuit:
			exit(EXIT_SUCCESS);
		default:
			break;
		}
	}
#endif
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
root_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = data;
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	switch (xev->type) {
	case PropertyNotify:
		return event_handler_PropertyNotify(dpy, xev, xscr);
	default:
#ifdef DO_XLOCKING
		if (xssEventBase && xev->type == xssEventBase + ScreenSaverNotify)
			return handle_XScreenSaverNotify(dpy, xev);
		DPRINTF("unknown event type %d\n", xev->type);
#endif				/* DO_XLOCKING */
		break;
	}
	return GDK_FILTER_CONTINUE;
}

#ifdef DO_XLOCKING
static GdkFilterReturn
event_handler_SelectionClear(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	DPRINT();
	if (options.debug > 1) {
		fprintf(stderr, "==> SelectionClear: %p\n", xscr);
		fprintf(stderr, "    --> send_event = %s\n",
			xev->xselectionclear.send_event ? "true" : "false");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xselectionclear.window);
		fprintf(stderr, "    --> selection = %s\n",
			XGetAtomName(dpy, xev->xselectionclear.selection));
		fprintf(stderr, "    --> time = %lu\n", xev->xselectionclear.time);
		fprintf(stderr, "<== SelectionClear: %p\n", xscr);
	}
	if (xscr && xev->xselectionclear.window == xscr->selwin) {
		XDestroyWindow(dpy, xscr->selwin);
		EPRINTF("selection cleared, exiting\n");
		exit(EXIT_SUCCESS);
	}
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
selwin_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = (typeof(xscr)) data;
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	DPRINT();
	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	switch (xev->type) {
	case SelectionClear:
		return event_handler_SelectionClear(dpy, xev, xscr);
	case ClientMessage:
		break;
	default:
		EPRINTF("wrong message type for handler %d\n", xev->type);
		break;
	}
	return GDK_FILTER_CONTINUE;
}
#endif

static GdkFilterReturn
client_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	Display *dpy = xev->xany.display;

	DPRINT();
	switch (xev->type) {
	case ClientMessage:
		return event_handler_ClientMessage(dpy, xev);
	}
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}

GtkListStore *store;			/* list store for XSessions */
GtkWidget *sess;

#define XDE_ICON_GEOM 32
#define XDE_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR

void
on_render_pixbuf(GtkTreeViewColumn *col, GtkCellRenderer *cell,
		 GtkTreeModel *store, GtkTreeIter *iter, gpointer data)
{
	GValue iname_v = G_VALUE_INIT;
	const gchar *iname;
	gchar *name = NULL, *p;
	gboolean has;
	GValue pixbuf_v = G_VALUE_INIT;

	gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, XSESS_COL_PIXBUF, &iname_v);
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
	XPRINTF("will try to render icon name = \"%s\"\n", name);

	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GdkPixbuf *pixbuf = NULL;

	XPRINTF("checking icon \"%s\"\n", name);
	has = gtk_icon_theme_has_icon(theme, name);
	if (has) {
		XPRINTF("trying to load icon \"%s\"\n", name);
		pixbuf = gtk_icon_theme_load_icon(theme, name, XDE_ICON_GEOM,
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
			pixbuf = gtk_icon_theme_load_icon(theme, name, XDE_ICON_GEOM,
							  GTK_ICON_LOOKUP_GENERIC_FALLBACK |
							  GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
		}
		if (!has || !pixbuf) {
			GtkWidget *image;

			XPRINTF("tyring to load image \"%s\"\n", "gtk-missing-image");
			if ((image = gtk_image_new_from_stock("gtk-missing-image",
							      XDE_ICON_SIZE))) {
				XPRINTF("tyring to load icon \"%s\"\n", "gtk-missing-image");
				pixbuf = gtk_widget_render_icon(GTK_WIDGET(image),
								"gtk-missing-image",
								XDE_ICON_SIZE, NULL);
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
		strcat(dirs, "/.local/share");
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

GKeyFile *
get_xsession_entry(const char *key, const char *file)
{
	GKeyFile *entry;

	if (!(entry = g_key_file_new())) {
		EPRINTF("%s: could not allocate key file\n", file);
		return (NULL);
	}
	if (!g_key_file_load_from_file(entry, file, G_KEY_FILE_NONE, NULL)) {
		EPRINTF("%s: could not load keyfile\n", file);
		g_key_file_unref(entry);
		return (NULL);
	}
	if (!g_key_file_has_group(entry, G_KEY_FILE_DESKTOP_GROUP)) {
		EPRINTF("%s: has no [%s] section\n", file, G_KEY_FILE_DESKTOP_GROUP);
		g_key_file_free(entry);
		return (NULL);
	}
	if (!g_key_file_has_key(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
		EPRINTF("%s: has no %s= entry\n", file, G_KEY_FILE_DESKTOP_KEY_TYPE);
		g_key_file_free(entry);
		return (NULL);
	}
	DPRINTF("got xsession file: %s (%s)\n", key, file);
	return (entry);
}

/** @brief wean out entries that should not be used
  */
gboolean
bad_xsession(const char *appid, GKeyFile *entry)
{
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
			// to much noise
			// DPRINTF("%s: %s: %s\n", appid, file,
			// strerror(errno));
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

static void
xsession_key_free(gpointer data)
{
	free(data);
}

static void
xsession_value_free(gpointer filename)
{
	free(filename);
}

/** @brief just gets the filenames of the xsession files
  *
  * This just gets the filenames of the xsession files to avoid performing a lot
  * of time consuming startup during the login.  We process the actual xsession
  * files and add them to the list out of an idle loop.
  */
GHashTable *
get_xsessions(void)
{
	char **xdg_dirs, **dirs;
	int i, n = 0;
	static const char *suffix = ".desktop";
	static const int suflen = 8;
	static GHashTable *xsessions = NULL;

	if (xsessions)
		return (xsessions);

	if (!(xdg_dirs = get_xsession_dirs(&n)) || !n)
		return (xsessions);

	xsessions = g_hash_table_new_full(g_str_hash, g_str_equal,
					  xsession_key_free, xsession_value_free);

	/* go through them backward */
	for (i = n - 1, dirs = &xdg_dirs[i]; i >= 0; i--, dirs--) {
		char *file, *p;
		DIR *dir;
		struct dirent *d;
		int len;
		char *key;

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
			key = strdup(d->d_name);
			*strstr(key, suffix) = '\0';
			g_hash_table_replace(xsessions, key, file);
		}
		closedir(dir);
	}
	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
	return (xsessions);
}

gboolean
on_idle(gpointer data)
{
	static GHashTable *xsessions = NULL;
	static GHashTableIter xiter;
	GtkListStore *store = data;
	const char *key;
	const char *file;
	GKeyFile *entry;
	GtkTreeIter iter;

	if (!xsessions) {
		if (!(xsessions = get_xsessions())) {
			EPRINTF("cannot build XSessions\n");
			return G_SOURCE_REMOVE;
		}
		if (!g_hash_table_size(xsessions)) {
			EPRINTF("cannot find any XSessions\n");
			return G_SOURCE_REMOVE;
		}
		g_hash_table_iter_init(&xiter, xsessions);
	}
	if (!g_hash_table_iter_next(&xiter, (gpointer *) & key, (gpointer *) & file))
		return G_SOURCE_REMOVE;

	if (!(entry = get_xsession_entry(key, file)))
		return G_SOURCE_CONTINUE;

	if (bad_xsession(key, entry)) {
		g_key_file_free(entry);
		return G_SOURCE_CONTINUE;
	}

	gchar *i, *n, *c, *k, *e, *l, *f, *t;
	gboolean m;

	f = g_strdup(file);
	l = g_strdup(key);
	i = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
				  G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
	n = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					 G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL) ? : g_strdup("");
	c = g_key_file_get_locale_string(entry, G_KEY_FILE_DESKTOP_GROUP,
					 G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL,
					 NULL) ? : g_strdup("");
	k = g_markup_printf_escaped("<b>%s</b>\n%s", n, c);
	e = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP,
				  G_KEY_FILE_DESKTOP_KEY_EXEC, NULL) ? : g_strdup("");
	m = g_key_file_get_boolean(entry, "Window Manager", "X-XDE-Managed", NULL);
	if (options.debug) {
		t = g_markup_printf_escaped("<b>Name:</b> %s" "\n"
					    "<b>Comment:</b> %s" "\n"
					    "<b>Exec:</b> %s" "\n"
					    "<b>Icon:</b> %s" "\n" "<b>file:</b> %s", n, c, e, i,
					    f);
	} else {
		t = g_markup_printf_escaped("<b>%s</b>: %s", n, c);
	}

	gtk_list_store_append(store, &iter);
	/* *INDENT-OFF* */
	gtk_list_store_set(store, &iter,
			XSESS_COL_PIXBUF,	i,
			XSESS_COL_NAME,		n,
			XSESS_COL_COMMENT,	c,
			XSESS_COL_MARKUP,	k,
			XSESS_COL_LABEL,	l,
			XSESS_COL_MANAGED,	m,
			XSESS_COL_ORIGINAL,	m,
			XSESS_COL_FILENAME,	f,
			XSESS_COL_TOOLTIP,	t,
			-1);
	/* *INDENT-ON* */
	g_free(f);
	g_free(l);
	g_free(i);
	g_free(n);
	g_free(k);
	g_free(e);
	g_free(t);
	g_key_file_free(entry);
#ifndef DO_CHOOSER
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					     GTK_SORT_ASCENDING);
#endif

#ifdef DO_CHOOSER
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));

	if (gtk_tree_selection_get_selected(selection, NULL, NULL))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.session, key))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.choice, "choose")
	    && strcmp(options.choice, "default"))
		return G_SOURCE_CONTINUE;
	gtk_tree_selection_select_iter(selection, &iter);
#ifdef DO_ONIDLE
	gchar *string;

	if ((string = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(store), &iter))) {
		GtkTreeView *tree = GTK_TREE_VIEW(sess);
		GtkTreePath *path = gtk_tree_path_new_from_string(string);
		GtkTreeViewColumn *cursor = gtk_tree_view_get_column(tree, 1);

		g_free(string);
		gtk_tree_view_set_cursor_on_cell(tree, path, cursor, NULL, FALSE);
		gtk_tree_view_scroll_to_cell(tree, path, cursor, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
#endif
#else
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(sess)) != -1)
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.session, key))
		return G_SOURCE_CONTINUE;
	if (strcmp(options.choice, key) && strcmp(options.choice, "choose")
	    && strcmp(options.choice, "default"))
		return G_SOURCE_CONTINUE;
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(sess), &iter);
#endif
	return G_SOURCE_CONTINUE;
}

#ifdef DO_XCHOOSER
GtkListStore *model;
GtkWidget *view;
#endif				/* DO_XCHOOSER */

GtkWidget *top;

#if !defined(DO_LOGOUT)
static GtkWidget *buttons[5];
#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM))
static GtkWidget *l_uname;
static GtkWidget *l_pword;
static GtkWidget *l_lstat;
static GtkWidget *user, *pass;
#endif
#endif				/* defined(DO_LOGOUT) */
static GtkWidget *l_greet;

gint
xsession_compare_function(GtkTreeModel *store, GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
	GValue a_v = G_VALUE_INIT;
	GValue b_v = G_VALUE_INIT;
	const gchar *astr;
	const gchar *bstr;
	gint ret;

	gtk_tree_model_get_value(GTK_TREE_MODEL(store), a, XSESS_COL_NAME, &a_v);
	gtk_tree_model_get_value(GTK_TREE_MODEL(store), b, XSESS_COL_NAME, &b_v);
	astr = g_value_get_string(&a_v);
	bstr = g_value_get_string(&b_v);
	ret = g_ascii_strcasecmp(astr, bstr);
	g_value_unset(&a_v);
	g_value_unset(&b_v);
	return (ret);
}

#ifdef DO_XCHOOSER
Bool
CanConnect(struct sockaddr *sa)
{
	int sock;
	socklen_t salen;

	switch (sa->sa_family) {
	case AF_INET:
		salen = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		salen = sizeof(struct sockaddr_in6);
		break;
	case AF_UNIX:
		salen = sizeof(struct sockaddr_un);
		break;
	default:
		EPRINTF("wrong socket family %d\n", (int) sa->sa_family);
		return False;
	}
	if ((sock = socket(sa->sa_family, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: %s\n", strerror(errno));
		return False;
	}
	if (options.debug) {
		char *p, *e, *rawbuf;
		unsigned char *b;
		int i, len;

		len = 2 * salen + 1;
		rawbuf = calloc(len, sizeof(*rawbuf));
		for (i = 0, p = rawbuf, e = rawbuf + len, b = (typeof(b)) sa;
		     i < salen; i++, p += 2, b++)
			snprintf(p, e - p, "%02x", *b);
		DPRINTF("raw socket address for connect: %s\n", rawbuf);
		free(rawbuf);
	}
	if (connect(sock, sa, salen) == -1) {
		DPRINTF("connect: %s\n", strerror(errno));
		close(sock);
		return False;
	}
	if (options.debug) {
		struct sockaddr conn;
		char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };

		if (getsockname(sock, &conn, &salen) == -1) {
			EPRINTF("getsockname: %s\n", strerror(errno));
			close(sock);
			return False;
		}
		switch (conn.sa_family) {
		case AF_INET:
		{
			struct sockaddr_in *sin = (typeof(sin)) & conn;
			int port = ntohs(sin->sin_port);

			inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
			DPRINTF("address is %s port %d\n", ipaddr, port);
			break;
		}
		case AF_INET6:
		{
			struct sockaddr_in6 *sin6 = (typeof(sin6)) & conn;
			int port = ntohs(sin6->sin6_port);

			inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
			DPRINTF("address is %s port %d\n", ipaddr, port);
			break;
		}
		case AF_UNIX:
		{
			struct sockaddr_un *sun = (typeof(sun)) & conn;

			DPRINTF("family is AF_UNIX\n");
			break;
		}
		default:
			EPRINTF("bad connected family %d\n", (int) conn.sa_family);
			close(sock);
			return False;
		}
	}
	close(sock);
	return True;
}
#endif				/* DO_XCHOOSER */

static void
on_managed_toggle(GtkCellRendererToggle *rend, gchar *path, gpointer data)
{
	GtkListStore *store = GTK_LIST_STORE(data);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path)) {
		GValue user_v = G_VALUE_INIT;
		GValue orig_v = G_VALUE_INIT;
		gboolean user;
		gboolean orig;

		gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, XSESS_COL_MANAGED, &user_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, XSESS_COL_ORIGINAL, &orig_v);
		user = g_value_get_boolean(&user_v);
		orig = g_value_get_boolean(&orig_v);
		if (orig) {
			user = user ? FALSE : TRUE;
			g_value_set_boolean(&user_v, user);
			gtk_list_store_set_value(GTK_LIST_STORE(store), &iter,
						 XSESS_COL_MANAGED, &user_v);
		}
		g_value_unset(&user_v);
		g_value_unset(&orig_v);
	}
}

static void
on_logout_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		const gchar *label;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		g_value_unset(&label_v);
	}
	free(options.current);
	options.current = strdup("logout");
	options.managed = False;
	choose_result = ChooseResultLogout;
	gtk_main_quit();
}

static void
on_default_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget **buttons = (typeof(buttons)) user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
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

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);

		if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
			if ((f = fopen(file, "w"))) {
				fprintf(f, "%s\n", label ? : "");
				gtk_widget_set_sensitive(buttons[1], FALSE);
				// gtk_widget_set_sensitive(buttons[2], FALSE);
				gtk_widget_set_sensitive(buttons[3], TRUE);
				fclose(f);
			}
		}
		if (label && label[0] && options.dmrc) {
			char *dmrc;

			len = strlen(home) + strlen("/.dmrc");
			dmrc = calloc(len + 1, sizeof(*dmrc));
			strncpy(dmrc, home, len);
			strncat(dmrc, "/.dmrc", len);

			g_key_file_set_string(options.dmrc, "Desktop", "Session", label);
			g_key_file_save_to_file(options.dmrc, dmrc, NULL);

			free(dmrc);
		}

		g_value_unset(&label_v);
		free(file);
		free(cdir);
	}
}

void
on_select_clicked(GtkButton *button, gpointer data)
{
	GtkTreeSelection *selection;
	GtkListStore *store = GTK_LIST_STORE(data);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (options.session) {
		GtkTreeIter iter;
		gboolean valid;

		for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0);
		     valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter,
						 XSESS_COL_LABEL, &label_v);
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
			     gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(store), &iter))) {
				GtkTreePath *path = gtk_tree_path_new_from_string(string);
				GtkTreeViewColumn *cursor =
				    gtk_tree_view_get_column(GTK_TREE_VIEW(sess), 1);

				g_free(string);
				gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(sess), path, cursor,
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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		gtk_tree_model_get_value(model, &iter, XSESS_COL_MANAGED, &manage_v);
		label = g_value_get_string(&label_v);
		manage = g_value_get_boolean(&manage_v);
		free(options.current);
		options.current = strdup(label);
		options.managed = manage;
		choose_result = ChooseResultLaunch;
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

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		if (label && !strcmp(label, options.session)) {
			gtk_widget_set_sensitive(buttons[1], FALSE);
			// gtk_widget_set_sensitive(buttons[2], FALSE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		} else {
			gtk_widget_set_sensitive(buttons[1], TRUE);
			// gtk_widget_set_sensitive(buttons[2], TRUE);
			gtk_widget_set_sensitive(buttons[3], TRUE);
		}
		g_value_unset(&label_v);
	} else {
		gtk_widget_set_sensitive(buttons[1], FALSE);
		// gtk_widget_set_sensitive(buttons[2], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
	}
}

static void
on_row_activated(GtkTreeView *sess, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GValue label_v = G_VALUE_INIT;
		GValue manage_v = G_VALUE_INIT;
		const gchar *label;
		gboolean manage;

		gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
		gtk_tree_model_get_value(model, &iter, XSESS_COL_MANAGED, &manage_v);
		if ((label = g_value_get_string(&label_v)))
			DPRINTF("Label selected %s\n", label);
		manage = g_value_get_boolean(&manage_v);
		free(options.current);
		options.current = strdup(label);
		options.managed = manage;
		choose_result = ChooseResultLaunch;
		g_value_unset(&label_v);
		g_value_unset(&manage_v);
		gtk_main_quit();
	}
}

static gboolean
on_button_press(GtkWidget *sess, GdkEvent *event, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(sess),
					  event->button.x,
					  event->button.y, &path, &col, NULL, NULL)) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
		gtk_tree_selection_select_path(selection, path);
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			GValue label_v = G_VALUE_INIT;
			const gchar *label;

			gtk_tree_model_get_value(model, &iter, XSESS_COL_LABEL, &label_v);
			if ((label = g_value_get_string(&label_v)))
				DPRINTF("Label clicked was: %s\n", label);
			g_value_unset(&label_v);
		}
	}
	return FALSE;		/* propagate event */
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

gboolean
on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	free(options.current);
	options.current = strdup("logout");
	options.managed = False;
	choose_result = ChooseResultLogout;
	gtk_main_quit();
	return TRUE;		/* propagate */
}

gboolean
on_expose_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	XdeScreen *xscr = data;
	GdkWindow *w;
	GdkWindow *r;
	cairo_t *cr;
	GdkEventExpose *ev;

	w = gtk_widget_get_window(xscr->wind);
	r = gdk_screen_get_root_window(xscr->scrn);
	ev = (typeof(ev)) event;

	cr = gdk_cairo_create(GDK_DRAWABLE(w));
	// gdk_cairo_reset_clip(cr, GDK_DRAWABLE(w));
	if (ev->region)
		gdk_cairo_region(cr, ev->region);
	else
		gdk_cairo_rectangle(cr, &ev->area);
	if (xscr->pixmap) {
		gdk_cairo_set_source_pixmap(cr, xscr->pixmap, 0, 0);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
	} else {
		gdk_cairo_set_source_window(cr, r, 0, 0);
		cairo_paint(cr);
		/* only fade out root window contents */
		GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };
		gdk_cairo_set_source_color(cr, &color);
		cairo_paint_with_alpha(cr, 0.7);
	}
	cairo_destroy(cr);
	return FALSE;
}

gboolean
on_grab_broken(GtkWidget *window, GdkEvent *event, gpointer data)
{
	GdkEventGrabBroken *ev = (typeof(ev)) event;
	DPRINTF("Grab broken!\n");
	DPRINTF("Grab broken on %s\n", ev->keyboard ? "keyboard" : "pointer");
	DPRINTF("Grab broken %s\n", ev->implicit ? "implicit" : "explicit");
	DPRINTF("Grab broken by %s\n", ev->grab_window ? "this application" : "other");
	return TRUE; /* propagate */
}

/** @brief transform window into pointer-grabbed window
  * @param window - window to transform
  *
  * Transform a window into a window that has a grab on the pointer on the
  * window and restricts pointer movement to the window boundary.
  *
  * Note that the window and pointer grabs should not fail: the reason is that
  * we have mapped this window above all others and a fully obscured window
  * cannot hold the pointer or keyboard focus in X.
  */
void
grabbed_window(GtkWidget *window, gpointer user_data)
{
	GdkWindow *win = gtk_widget_get_window(window);
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
	else
		DPRINTF("Grabbed keyboard\n");
	if (gdk_pointer_grab(win, TRUE, mask, win, NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		EPRINTF("Could not grab pointer!\n");
	else
		DPRINTF("Grabbed pointer\n");
#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_STARTWM)||defined(DO_SESSION)) && !defined(DO_LOGOUT)
	if (!grab_broken_handler)
		grab_broken_handler = g_signal_connect(G_OBJECT(window), "grab-broken-event", G_CALLBACK(on_grab_broken), NULL);
#endif
}

/** @brief transform a window away from a grabbed window
  *
  * Transform the window back into a regular window, releasing the pointer and
  * keyboard grab and motion restriction.  window is the GtkWindow that
  * previously had the grabbed_window() method called on it.
  */
void
ungrabbed_window(GtkWidget *window)
{
	GdkWindow *win = gtk_widget_get_window(window);

#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_STARTWM)||defined(DO_SESSION)) && !defined(DO_LOGOUT)
	if (grab_broken_handler) {
		g_signal_handler_disconnect(G_OBJECT(window), grab_broken_handler);
		grab_broken_handler = 0;
	}
//	g_signal_connect(G_OBJECT(window), "grab-broken-event", NULL, NULL);
#endif
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_window_hide(win);
}

/** @brief render a pixbuf into a pixmap for a monitor
  */
void
render_pixbuf_for_mon(cairo_t * cr, GdkPixbuf *pixbuf, double wp, double hp, XdeMonitor *xmon)
{
	double wm = xmon->geom.width;
	double hm = xmon->geom.height;
	GdkPixbuf *scaled = NULL;

	DPRINT();
	gdk_cairo_rectangle(cr, &xmon->geom);
	if (wp >= 0.8 * wm && hp >= 0.8 * hm) {
		/* good size for filling or scaling */
		/* TODO: check aspect ratio before scaling */
		DPRINTF("scaling pixbuf from %dx%d to %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		scaled = gdk_pixbuf_scale_simple(pixbuf,
						 xmon->geom.width,
						 xmon->geom.height, GDK_INTERP_BILINEAR);
		gdk_cairo_set_source_pixbuf(cr, scaled, xmon->geom.x, xmon->geom.y);
	} else if (wp <= 0.5 * wm && hp <= 0.5 * hm) {
		/* good size for tiling */
		DPRINTF("tiling pixbuf at %dx%d into %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		gdk_cairo_set_source_pixbuf(cr, pixbuf, xmon->geom.x, xmon->geom.y);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	} else {
		/* somewhere in between: scale down for integer tile */
		DPRINTF("scaling and tiling pixbuf at %dx%d into %dx%d\n",
			(int) wp, (int) hp, xmon->geom.width, xmon->geom.height);
		scaled = gdk_pixbuf_scale_simple(pixbuf,
						 xmon->geom.width / 2,
						 xmon->geom.height / 2, GDK_INTERP_BILINEAR);
		gdk_cairo_set_source_pixbuf(cr, scaled, xmon->geom.x, xmon->geom.y);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
	}
	cairo_paint(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	if (scaled)
		g_object_unref(G_OBJECT(scaled));
}

void
render_pixbuf_for_scr(GdkPixbuf *pixbuf, GdkPixmap *pixmap, XdeScreen *xscr)
{
	double w = gdk_pixbuf_get_width(pixbuf);
	double h = gdk_pixbuf_get_height(pixbuf);
	cairo_t *cr;
	int m;

	DPRINT();
	cr = gdk_cairo_create(GDK_DRAWABLE(pixmap));
	for (m = 0; m < xscr->nmon; m++) {
		XdeMonitor *xmon = xscr->mons + m;

		render_pixbuf_for_mon(cr, pixbuf, w, h, xmon);
	}
	cairo_destroy(cr);
}

void
clr_source(XdeScreen *xscr)
{
	GdkWindow *w;

	if (xscr->wind && (w = gtk_widget_get_window(xscr->wind))) {
		gint x = 0, y = 0, width = 0, height = 0, depth = 0;

		gdk_window_get_geometry(w, &x, &y, &width, &height, &depth);
		gdk_window_clear_area_e(w, x, y, width, height);
	}
}

GtkStyle *style;

void
set_style(XdeScreen *xscr)
{
	GtkWidget *window = xscr->wind;

	if (!style) {
		style = gtk_widget_get_default_style();
		style = gtk_style_copy(style);
	}
	style->bg_pixmap[GTK_STATE_NORMAL] = xscr->pixmap;
	style->bg_pixmap[GTK_STATE_PRELIGHT] = xscr->pixmap;
	gtk_widget_set_style(window, style);
}

void
update_source(XdeScreen *xscr)
{
	if (options.setstyle)
		clr_source(xscr);
	else
		set_style(xscr);
	if (xscr->pixmap && options.setbg) {
		GdkDisplay *disp;
		GdkWindow *root;
		GdkColormap *cmap;
		GdkPixmap *pixmap;
		Display *dpy;
		Pixmap p;
		int s;
		cairo_t *cr;
		long data;

		s = xscr->index;
		if (!(dpy = XOpenDisplay(NULL))) {
			EPRINTF("cannot open display %s\n", getenv("DISPLAY"));
			return;
		}
		XSetCloseDownMode(dpy, RetainTemporary);
		p = XCreatePixmap(dpy, RootWindow(dpy, s),
				  DisplayWidth(dpy, s),
				  DisplayHeight(dpy, s), DefaultDepth(dpy, s));

		XCloseDisplay(dpy);

		disp = gdk_screen_get_display(xscr->scrn);
		root = gdk_screen_get_root_window(xscr->scrn);
		cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
		pixmap = gdk_pixmap_foreign_new_for_display(disp, p);
		gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), cmap);

		cr = gdk_cairo_create(GDK_DRAWABLE(pixmap));
		gdk_cairo_set_source_pixmap(cr, xscr->pixmap, 0, 0);
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
		cairo_destroy(cr);

		g_object_unref(G_OBJECT(pixmap));

		dpy = GDK_DISPLAY_XDISPLAY(disp);
		XSetWindowBackgroundPixmap(dpy, RootWindow(dpy, s), p);

		data = p;
		XChangeProperty(dpy, RootWindow(dpy, s),
				_XA_XROOTPMAP_ID, XA_PIXMAP,
				32, PropModeReplace, (unsigned char *) &data, 1);
		XKillClient(dpy, AllTemporary);
	}
}

static void
get_source(XdeScreen *xscr)
{
	GdkDisplay *disp = gdk_screen_get_display(xscr->scrn);
	GdkWindow *root = gdk_screen_get_root_window(xscr->scrn);
	GdkColormap *cmap = gdk_drawable_get_colormap(GDK_DRAWABLE(root));
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom prop;

	if (options.source & BackgroundSourcePixmap) {
		if (!(prop = _XA_XROOTPMAP_ID))
			prop = _XA_ESETROOT_PMAP_ID;

		if (prop) {
			Window w = GDK_WINDOW_XID(root);
			Atom actual = None;
			int format = 0;
			unsigned long nitems = 0, after = 0;
			long *data = NULL;

			if (XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_PIXMAP,
					       &actual, &format, &nitems, &after,
					       (unsigned char **) &data) == Success &&
			    format == 32 && actual && nitems >= 1 && data) {
				Pixmap p;

				if ((p = data[0])) {
					xscr->pixmap = gdk_pixmap_foreign_new_for_display(disp, p);
					gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
					update_source(xscr);
				}
			}
			if (data)
				XFree(data);
		}
		if (xscr->pixmap)
			return;
	}
	if (options.source & BackgroundSourceSplash) {
		if (!xscr->pixbuf && options.backdrop) {
			if (!(xscr->pixbuf = gdk_pixbuf_new_from_file(options.backdrop, NULL))) {
				/* cannot use it again */
				free(options.backdrop);
				options.backdrop = NULL;
			}
		}
		if (xscr->pixbuf) {
			if (!xscr->pixmap) {
				xscr->pixmap =
				    gdk_pixmap_new(GDK_DRAWABLE(root), xscr->width, xscr->height,
						   -1);
				gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
				render_pixbuf_for_scr(xscr->pixbuf, xscr->pixmap, xscr);
				update_source(xscr);
			}
			return;
		}
	}
	if (options.source & BackgroundSourceRoot) {
		if (!xscr->pixmap) {
			cairo_t *cr;
			GdkColor color = {.red = 0,.green = 0,.blue = 0,.pixel = 0, };

			xscr->pixmap =
				gdk_pixmap_new(GDK_DRAWABLE(root), xscr->width,
						xscr->height, -1);
			gdk_drawable_set_colormap(GDK_DRAWABLE(xscr->pixmap), cmap);
			cr = gdk_cairo_create(GDK_DRAWABLE(xscr->pixmap));
			gdk_cairo_set_source_window(cr, root, 0, 0);
			cairo_paint(cr);
			gdk_cairo_set_source_color(cr, &color);
			cairo_paint_with_alpha(cr, 0.7);
			update_source(xscr);
		}
	}
}

static void
redo_source(XdeScreen *xscr)
{
	if (xscr->pixmap) {
		g_object_unref(G_OBJECT(xscr->pixmap));
		xscr->pixmap = NULL;
	}
	if (xscr->pixbuf) {
		g_object_unref(G_OBJECT(xscr->pixbuf));
		xscr->pixbuf = NULL;
	}
	get_source(xscr);
}

#if defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)
/** @brief create the selected session
  * @param label - the application id of the XSession
  * @param filename - the desktop entry file name for the XSession
  *
  * Launch the session specified by the label argument with the xsession desktop
  * file pass in the session argument.  This function writes the selection and
  * default to the user's current and default files in
  * $XDG_CONFIG_HOME/xde/current and $XDG_CONFIG_HOME/xde/default, sets the
  * option variables options.current and options.session.  A NULL session
  * pointer means that a logout should be performed instead.
  */
void
create_session(const char *label, const char *filename)
{
	char *home = getenv("HOME") ? : ".";
	char *xhome = getenv("XDG_CONFIG_HOME");
	char *cdir, *file;
	int len, dlen, flen;
	FILE *f;

	len = xhome ? strlen(xhome) : strlen(home) + strlen("/.config");
	dlen = len + strlen("/xde");
	flen = dlen + strlen("/default");
	cdir = calloc(dlen, sizeof(*cdir));
	file = calloc(flen, sizeof(*file));
	if (xhome)
		strcpy(cdir, xhome);
	else {
		strcpy(cdir, home);
		strcat(cdir, "/.config");
	}
	strcat(cdir, "/xde");

	strcpy(file, cdir);
	strcat(file, "/current");
	if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
		if ((f = fopen(file, "w"))) {
			fprintf(f, "%s\n", options.current ? : "");
			fclose(f);
		}
	}

	if (options.setdflt) {
		strcpy(file, cdir);
		strcat(file, "/default");
		if (!access(file, W_OK) || (!mkdir(cdir, 0755) && !access(file, W_OK))) {
			if ((f = fopen(file, "w"))) {
				fprintf(f, "%s\n", options.session ? : "");
				fclose(f);
			}
		}
		if (options.session && options.dmrc) {
			char *dmrc;

			len = strlen(home) + strlen("/.dmrc");
			dmrc = calloc(len + 1, sizeof(*dmrc));
			strncpy(dmrc, home, len);
			strncat(dmrc, "/.dmrc", len);

			g_key_file_set_string(options.dmrc, "Desktop", "Session", options.session);
			g_key_file_save_to_file(options.dmrc, dmrc, NULL);

			free(dmrc);
		}
	}

	free(file);
	free(cdir);
}
#endif

#ifdef DO_XLOCKING
static Window
get_selection(Window selwin, char *selection, int s)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	Atom atom;
	Window owner;

	snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
	atom = XInternAtom(dpy, selection, False);
	if (!(owner = XGetSelectionOwner(dpy, atom)))
		DPRINTF("No owner for %s\n", selection);
	if ((owner && options.replace) || (!owner && selwin)) {
		DPRINTF("Setting owner of %s to 0x%lx from 0x%lx\n", selection, selwin, owner);
		XSetSelectionOwner(dpy, atom, selwin, CurrentTime);
		XSync(dpy, False);
	}
	if (options.replace && selwin) {
		XEvent ev;
		Atom manager = XInternAtom(dpy, "MANAGER", False);
		GdkScreen *scrn = gdk_display_get_screen(disp, s);
		GdkWindow *root = gdk_screen_get_root_window(scrn);
		Window r = GDK_WINDOW_XID(root);
		Atom atom = XInternAtom(dpy, selection, False);

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = r;
		ev.xclient.message_type = manager;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = CurrentTime;
		ev.xclient.data.l[1] = atom;
		ev.xclient.data.l[2] = selwin;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, r, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	return (owner);
}
#endif				/* DO_XLOCKING */

GtkWidget *cont;			/* container of event box */
GtkWidget *ebox;			/* event box window within the screen */

static void
RefreshScreen(XdeScreen *xscr, GdkScreen *scrn)
{
	XdeMonitor *mon;
	GtkWindow *w = GTK_WINDOW(xscr->wind);
	GdkWindow *win = gtk_widget_get_window(xscr->wind);
	char *geom;
	int m, nmon, width, height, index;

	index = gdk_screen_get_number(scrn);
	if (xscr->index != index) {
		EPRINTF("Arrrghhhh! screen index changed from %d to %d\n", xscr->index, index);
		xscr->index = index;
	}

	if (xscr->scrn != scrn) {
		DPRINTF("Arrrghhh! screen pointer changed from %p to %p\n", xscr->scrn, scrn);
		xscr->scrn = scrn;
	}
	width = gdk_screen_get_width(scrn);
	height = gdk_screen_get_height(scrn);
	if (xscr->width != width || xscr->height != height) {
		DPRINTF("Screen %d dimensions changed %dx%d -> %dx%d\n", index,
			xscr->width, xscr->height, width, height);
		gtk_window_set_default_size(w, width, height);
		geom = g_strdup_printf("%dx%d+0+0", width, height);
		gtk_window_parse_geometry(w, geom);
		g_free(geom);
		gtk_widget_set_size_request(GTK_WIDGET(w), width, height);
		gtk_window_resize(w, width, height);
		gtk_window_move(w, 0, 0);
		gdk_window_move_resize(win, 0, 0, width, height);
		xscr->width = width;
		xscr->height = height;
	}
	nmon = gdk_screen_get_n_monitors(scrn);
	if (nmon > xscr->nmon) {
		DPRINTF("Screen %d number of monitors increased from %d to %d\n",
			index, xscr->nmon, nmon);
		xscr->mons = realloc(xscr->mons, nmon * sizeof(*xscr->mons));
		for (m = xscr->nmon; m <= nmon; m++) {
			mon = xscr->mons + m;
			memset(mon, 0, sizeof(*mon));
		}
	} else if (nmon < xscr->nmon) {
		DPRINTF("Screen %d number of monitors decreased from %d to %d\n",
			index, xscr->nmon, nmon);
		for (m = xscr->nmon; m > nmon; m--) {
			mon = xscr->mons + m - 1;
			if (ebox && cont && mon->align == cont) {
				gtk_container_remove(GTK_CONTAINER(cont), ebox);
				cont = NULL;
			}
			if (mon->align) {
				gtk_widget_destroy(mon->align);
				mon->align = NULL;
			}
		}
		xscr->mons = realloc(xscr->mons, nmon * sizeof(*xscr->mons));
	}
	if (nmon != xscr->nmon)
		xscr->nmon = nmon;

	/* always realign center alignment widgets */
	for (m = 0, mon = xscr->mons; m < nmon; m++, mon++) {
		double xrel, yrel;

		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (double) (mon->geom.x + mon->geom.width * options.xposition) / (double) xscr->width;
		yrel = (double) (mon->geom.x + mon->geom.height * options.yposition) / (double) xscr->height;
		if (!mon->align) {
			mon->align = gtk_alignment_new(xrel, yrel, 0, 0);
			gtk_container_add(GTK_CONTAINER(w), mon->align);
		} else
			gtk_alignment_set(GTK_ALIGNMENT(mon->align), xrel, yrel, 0, 0);
	}
	/* always reassign the event box if its containing monitor was removed */
	if (ebox && !cont) {
		GdkDisplay *disp = gdk_screen_get_display(scrn);
		GdkScreen *screen = NULL;
		gint x = 0, y = 0;
		XdeScreen *_xscr;
		XdeMonitor *_xmon;

		DPRINTF("Reassigning event box to new container\n");
		gdk_display_get_pointer(disp, &screen, &x, &y, NULL);
		if (!screen)
			screen = scrn;
		_xscr = screens + gdk_screen_get_number(screen);
		_xmon = _xscr->mons + gdk_screen_get_monitor_at_point(screen, x, y);
		cont = _xmon->align;
		gtk_container_add(GTK_CONTAINER(cont), ebox);
#if 0
		/* FIXME: only if it should be currently displayed */
		gtk_widget_show_all(cont);
		gtk_widget_show_now(cont);
		gtk_widget_grab_default(user);
		gtk_widget_grab_focus(user);
#endif
	}
	/* redo background images */
	redo_source(xscr);
}

/** @brief monitors changed
  *
  * The number and/or size of monitors belonging to a screen have changed.  This
  * may be as a result of RANDR or XINERAMA changes.  Walk through the monitors
  * and adjust the necessary parameters.
  */
static void
on_monitors_changed(GdkScreen *scrn, gpointer data)
{
	XdeScreen *xscr = data;

	RefreshScreen(xscr, scrn);
}

/** @brief screen size changed
  *
  * The size of the screen changed.  This may be as a result of RANDR or
  * XINERAMA changes.  Walk through the screen and the monitors on the screen
  * and adjust the necessary parameters.
  */
static void
on_size_changed(GdkScreen *scrn, gpointer data)
{
	XdeScreen *xscr = data;

	RefreshScreen(xscr, scrn);
}

/** @brief get a covering window for a screen
  */
void
GetScreen(XdeScreen *xscr, int s, GdkScreen *scrn, Bool noshow)
{
	GtkWidget *wind;
	GtkWindow *w;
	int m;
	XdeMonitor *mon;

	xscr->index = s;
	xscr->scrn = scrn;
	xscr->wind = wind = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	xscr->width = gdk_screen_get_width(scrn);
	xscr->height = gdk_screen_get_height(scrn);

	g_signal_connect(G_OBJECT(scrn), "monitors-changed", G_CALLBACK(on_monitors_changed), xscr);
	g_signal_connect(G_OBJECT(scrn), "size-changed", G_CALLBACK(on_size_changed), xscr);

	w = GTK_WINDOW(wind);
	gtk_window_set_screen(w, scrn);
	gtk_window_set_wmclass(w, RESNAME, RESCLAS);
	gtk_window_set_title(w, RESTITL);
	gtk_window_set_modal(w, TRUE);
	gtk_window_set_gravity(w, GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
	gtk_window_set_icon_name(w, "xdm");
	gtk_window_set_skip_pager_hint(w, TRUE);
	gtk_window_set_skip_taskbar_hint(w, TRUE);
	gtk_window_set_position(w, GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_fullscreen(w);
	gtk_window_set_decorated(w, FALSE);
	gtk_window_set_default_size(w, xscr->width, xscr->height);

	char *geom = g_strdup_printf("%dx%d+0+0", xscr->width, xscr->height);

	gtk_window_parse_geometry(w, geom);
	g_free(geom);

	gtk_widget_set_size_request(GTK_WIDGET(w), xscr->width, xscr->height);
	gtk_window_resize(w, xscr->width, xscr->height);
	gtk_window_move(w, 0, 0);

	g_signal_connect(G_OBJECT(w), "destroy", G_CALLBACK(on_destroy), NULL);
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(on_delete_event), NULL);

	if (options.setstyle) {
		gtk_widget_set_app_paintable(wind, TRUE);
		g_signal_connect(G_OBJECT(w), "expose-event", G_CALLBACK(on_expose_event), xscr);
	}

	gtk_window_set_focus_on_map(w, TRUE);
	gtk_window_set_accept_focus(w, TRUE);
	gtk_window_set_keep_above(w, TRUE);
	gtk_window_set_modal(w, TRUE);
	gtk_window_stick(w);
	gtk_window_deiconify(w);

	xscr->nmon = gdk_screen_get_n_monitors(scrn);
	xscr->mons = calloc(xscr->nmon, sizeof(*xscr->mons));
	for (m = 0, mon = xscr->mons; m < xscr->nmon; m++, mon++) {
		double xrel, yrel;

		mon->index = m;
		gdk_screen_get_monitor_geometry(scrn, m, &mon->geom);
		xrel = (double) (mon->geom.x + mon->geom.width * options.xposition) / (double) xscr->width;
		yrel = (double) (mon->geom.x + mon->geom.height * options.yposition) / (double) xscr->height;
		mon->align = gtk_alignment_new(xrel, yrel, 0, 0);
		gtk_container_add(GTK_CONTAINER(w), mon->align);
	}
	redo_source(xscr);
	if (!noshow)
		gtk_widget_show_all(wind);
	gtk_widget_realize(wind);
	GdkWindow *win = gtk_widget_get_window(wind);

	gdk_window_set_override_redirect(win, TRUE);
	gdk_window_move_resize(win, 0, 0, xscr->width, xscr->height);

#ifdef DO_XLOCKING
	GdkDisplay *disp = gdk_screen_get_display(scrn);
#endif
#if 0
	/* does not work well with broken intel video drivers */
	GdkCursor *curs = gdk_cursor_new_for_display(disp, GDK_LEFT_PTR);

	gdk_window_set_cursor(win, curs);
	gdk_cursor_unref(curs);
#endif

	GdkWindow *root = gdk_screen_get_root_window(scrn);
	GdkEventMask mask = gdk_window_get_events(root);

	gdk_window_add_filter(root, root_handler, xscr);
	mask |= GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
	gdk_window_set_events(root, mask);

#ifdef DO_XLOCKING
	Window owner = None;
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);

	xscr->selwin = XCreateSimpleWindow(dpy, GDK_WINDOW_XID(root), 0, 0, 1, 1, 0, 0, 0);
	if ((owner = get_selection(xscr->selwin, xscr->selection, s))) {
		if (!options.replace) {
			XDestroyWindow(dpy, xscr->selwin);
			EPRINTF("%s: instance already running\n", NAME);
			exit(EXIT_FAILURE);
		}
	}
	GdkWindow *sel = gdk_x11_window_foreign_new_for_display(disp, xscr->selwin);

	gdk_window_add_filter(sel, selwin_handler, xscr);
	mask = gdk_window_get_events(sel);
	mask |= GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK;
	gdk_window_set_events(sel, mask);
#endif				/* DO_XLOCKING */
}

static void
reparse(Display *dpy, Window root)
{
	XTextProperty xtp = { NULL, };
	char **list = NULL;
	int strings = 0;

	DPRINT();
	gtk_rc_reparse_all();
	if (!options.usexde)
		return;
	if (XGetTextProperty(dpy, root, &xtp, _XA_XDE_THEME_NAME)) {
		if (Xutf8TextPropertyToTextList(dpy, &xtp, &list, &strings) == Success) {
			if (strings >= 1) {
				static const char *prefix = "gtk-theme-name=\"";
				static const char *suffix = "\"";
				char *rc_string;
				int len;

				len = strlen(prefix) + strlen(list[0]) + strlen(suffix) + 1;
				rc_string = calloc(len, sizeof(*rc_string));
				strncpy(rc_string, prefix, len);
				strncat(rc_string, list[0], len);
				strncat(rc_string, suffix, len);
				gtk_rc_parse_string(rc_string);
				free(rc_string);
			}
			if (list)
				XFreeStringList(list);
		} else
			DPRINTF("could not get text list for property\n");
		if (xtp.value)
			XFree(xtp.value);
	} else
		DPRINTF("could not get _XDE_THEME_NAME for root 0x%lx\n", root);
}

/** @brief get a covering window for each screen
  */
void
GetScreens(Bool noshow)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	screens = calloc(nscr, sizeof(*screens));

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		GetScreen(xscr, s, gdk_display_get_screen(disp, s), noshow);

	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn);

	reparse(dpy, GDK_WINDOW_XID(root));
}

GtkWidget *
GetBanner(void)
{
	GtkWidget *ban = NULL, *bin, *pan, *img;

	if (options.banner && (img = gtk_image_new_from_file(options.banner))) {
		GtkShadowType shadow = (options.transparent) ? GTK_SHADOW_NONE : GTK_SHADOW_ETCHED_IN;
		ban = gtk_vbox_new(FALSE, 0);
		bin = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(bin), shadow);
		gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
		gtk_box_pack_start(GTK_BOX(ban), bin, TRUE, TRUE, 4);
		pan = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
		gtk_container_set_border_width(GTK_CONTAINER(pan), 15);
		gtk_container_add(GTK_CONTAINER(bin), pan);
		gtk_container_add(GTK_CONTAINER(pan), img);
	}
	return (ban);
}

void
on_combo_popdown(GtkComboBox *combo, gpointer data)
{
	GtkWidget *window;

	window = gtk_widget_get_toplevel(GTK_WIDGET(combo));

	relax();
	grabbed_window(window, NULL);
}

#define BB_INT_PADDING  0
#define BB_BOX_SPACING  5
#define BB_BORDER_WIDTH 5
#define BU_BORDER_WIDTH 0
#define BB_PACK_PADDING 0

GtkWidget *
GetPanel(void)
{
	GtkWidget *pan = gtk_vbox_new(FALSE, 0);
	GtkShadowType shadow = (options.transparent) ? GTK_SHADOW_NONE : GTK_SHADOW_ETCHED_IN;

	gtk_container_set_border_width(GTK_CONTAINER(pan), 0);

	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), shadow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 3);
	gtk_box_pack_start(GTK_BOX(pan), sw, TRUE, TRUE, 0);

	/* *INDENT-OFF* */
	store = gtk_list_store_new(9
			,G_TYPE_STRING	/* icon */
			,G_TYPE_STRING	/* Name */
			,G_TYPE_STRING	/* Comment */
			,G_TYPE_STRING	/* Name and Comment Markup */
			,G_TYPE_STRING	/* Label */
			,G_TYPE_BOOLEAN	/* SessionManaged?  XDE-Managed?  */
			,G_TYPE_BOOLEAN	/* X-XDE-managed original setting */
			,G_TYPE_STRING	/* the file name */
			,G_TYPE_STRING	/* tooltip */
	    );
	/* *INDENT-ON* */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
					     XSESS_COL_MARKUP, GTK_SORT_ASCENDING);
	sess = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_widget_set_can_default(sess, TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(sess), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(sess), XSESS_COL_NAME);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(sess), XSESS_COL_TOOLTIP);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sess), FALSE);
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_GRID_LINES_BOTH);
	gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(sess));

	g_signal_connect(G_OBJECT(sess), "row_activated", G_CALLBACK(on_row_activated), NULL);
	g_signal_connect(G_OBJECT(sess), "button_press_event", G_CALLBACK(on_button_press), NULL);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), buttons);

	GtkCellRenderer *rend = gtk_cell_renderer_toggle_new();

	gtk_cell_renderer_toggle_set_activatable(GTK_CELL_RENDERER_TOGGLE(rend), TRUE);
	g_signal_connect(G_OBJECT(rend), "toggled", G_CALLBACK(on_managed_toggle), store);
	GtkTreeViewColumn *col;

	col = gtk_tree_view_column_new_with_attributes("Managed", rend, "active", XSESS_COL_MANAGED,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_COLUMN(col));

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(sess),
						   -1, "Icon", rend, on_render_pixbuf, NULL, NULL);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Window Manager", rend, "markup",
						       XSESS_COL_MARKUP, NULL);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), XSESS_COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sess), GTK_TREE_VIEW_COLUMN(col));

	GtkWidget *bb = gtk_hbutton_box_new();

	gtk_box_set_spacing(GTK_BOX(bb), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end(GTK_BOX(pan), bb, FALSE, TRUE, 0);

	GtkWidget *i;
	GtkWidget *b;

	buttons[0] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((getenv("DISPLAY") ? : "")[0] == ':') {
		if ((i = gtk_image_new_from_stock("gtk-quit", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), i);
		gtk_button_set_label(GTK_BUTTON(b), "Logout");
	} else {
		if ((i = gtk_image_new_from_stock("gtk-disconnect", GTK_ICON_SIZE_BUTTON)))
			gtk_button_set_image(GTK_BUTTON(b), i);
		gtk_button_set_label(GTK_BUTTON(b), "Disconnect");
	}
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_logout_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

	buttons[1] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-save", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Make Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_default_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

#if 0
	buttons[2] = b = gtk_button_new();
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Select Default");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_select_clicked), store);
	gtk_widget_set_sensitive(b, TRUE);
#endif

	buttons[3] = b = gtk_button_new();
	gtk_widget_set_can_default(b, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_button_set_image_position(GTK_BUTTON(b), GTK_POS_LEFT);
	gtk_button_set_alignment(GTK_BUTTON(b), 0.0, 0.5);
	if ((i = gtk_image_new_from_stock("gtk-ok", GTK_ICON_SIZE_BUTTON)))
		gtk_button_set_image(GTK_BUTTON(b), i);
	gtk_button_set_label(GTK_BUTTON(b), "Launch Session");
	gtk_box_pack_start(GTK_BOX(bb), b, TRUE, TRUE, 5);
	g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(on_launch_clicked), buttons);
	gtk_widget_set_sensitive(b, TRUE);

#ifndef DO_LOGOUT
#ifdef DO_XCHOOSER
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), shadow);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(pan), sw, TRUE, TRUE, 4);

	/* *INDENT-OFF* */
	model = gtk_list_store_new(13
			,G_TYPE_STRING		/* hostname */
			,G_TYPE_STRING		/* remotename */
			,G_TYPE_INT		/* willing */
			,G_TYPE_STRING		/* status */
			,G_TYPE_STRING		/* IP Address */
			,G_TYPE_INT		/* connection type */
			,G_TYPE_STRING		/* service */
			,G_TYPE_INT		/* port */
			,G_TYPE_STRING		/* markup */
			,G_TYPE_STRING		/* tooltip */
			,G_TYPE_SOCKADDR	/* socket address */
			,G_TYPE_INT		/* scope */
			,G_TYPE_INT		/* interface index */
	    );
	/* *INDENT-ON* */

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
					     XDM_COL_MARKUP, GTK_SORT_ASCENDING);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), XDM_COL_HOSTNAME);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), XDM_COL_TOOLTIP);

	gtk_container_add(GTK_CONTAINER(sw), view);

	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	int len = strlen("XDCMP Host Menu from ") + strlen(hostname) + 1;
	char *title = calloc(len, sizeof(*title));

	strncpy(title, "XDCMP Host Menu from ", len);
	strncat(title, hostname, len);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(title, renderer,
									     "markup",
									     XDM_COL_MARKUP, NULL);

	free(title);
	gtk_tree_view_column_set_sort_column_id(column, XDM_COL_HOSTNAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(column));
	g_signal_connect(G_OBJECT(view), "row_activated",
			 G_CALLBACK(on_row_activated), (gpointer) NULL);

#endif				/* DO_XCHOOSER */

	if (options.xsession) {
#ifdef DO_ONIDLE
		g_idle_add(on_idle, store);
#else
		while (on_idle(store) != G_SOURCE_REMOVE) ;
#endif
	}
#endif				/* !defined DO_LOGOUT */

	/* TODO: we should really set a timeout and if no user interaction has
	   occured before the timeout, we should continue if we have a viable
	   default or choice. */

	return (pan);
}

GtkWidget *
GetPane(GtkWidget *cont)
{
	GtkRcStyle *style;
	char hostname[64] = { 0, };

	gethostname(hostname, sizeof(hostname));

	if (options.transparent)
		ebox = gtk_hbox_new(FALSE, 0);
	else
		ebox = gtk_event_box_new();

	gtk_container_add(GTK_CONTAINER(cont), ebox);
	gtk_widget_set_size_request(ebox, -1, -1);

	GtkWidget *v = gtk_vbox_new(FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(v), 10);

	gtk_container_add(GTK_CONTAINER(ebox), v);

	l_greet = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l_greet), options.welcome);
	gtk_misc_set_alignment(GTK_MISC(l_greet), 0.5, 0.5);
	gtk_misc_set_padding(GTK_MISC(l_greet), 3, 3);
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideBottom:
		gtk_box_pack_start(GTK_BOX(v), l_greet, FALSE, TRUE, 0);
		break;
	case LogoSideRight:
	case LogoSideTop:
		gtk_box_pack_end(GTK_BOX(v), l_greet, FALSE, TRUE, 0);
		break;
	}
	if ((style = gtk_widget_get_modifier_style(l_greet))) {
		style->font_desc = pango_font_description_copy(resources.greetFace);
		if (options.transparent) {
			if (resources.greetColor) {
				int i;

				for (i = 0; i < 5; i++) {
					style->text[i] = *resources.greetColor;
					style->color_flags[i] |= GTK_RC_TEXT;
					style->fg[i] = *resources.greetColor;
					style->color_flags[i] |= GTK_RC_FG;
					// style->base[i] = *resources.greetColor;
					// style->color_flags[i] |= GTK_RC_BASE;
				}
			} else
				DPRINTF("No resources.greetColor!\n");
		}
		gtk_widget_modify_style(l_greet, style);
	}

	GtkWidget *box;
	
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideRight:
		box = gtk_hbox_new(FALSE, 5);
		break;
	case LogoSideTop:
	case LogoSideBottom:
		box = gtk_vbox_new(TRUE, 5);
		break;
	}

	gtk_box_pack_end(GTK_BOX(v), box, TRUE, TRUE, 0);

	if ((v = GetBanner())) {
		switch (options.side) {
		default:
		case LogoSideLeft:
		case LogoSideTop:
			gtk_box_pack_start(GTK_BOX(box), v, TRUE, TRUE, 0);
			break;
		case LogoSideRight:
		case LogoSideBottom:
			gtk_box_pack_end(GTK_BOX(box), v, TRUE, TRUE, 0);
			break;
		}
	}

	v = GetPanel();
	switch (options.side) {
	default:
	case LogoSideLeft:
	case LogoSideTop:
		gtk_box_pack_end(GTK_BOX(box), v, TRUE, TRUE, 0);
		break;
	case LogoSideRight:
	case LogoSideBottom:
		gtk_box_pack_start(GTK_BOX(box), v, TRUE, TRUE, 0);
		break;
	}

	return (ebox);
}

GtkWidget *
GetWindow(Bool noshow)
{
	GdkDisplay *disp = gdk_display_get_default();
	GdkScreen *scrn = NULL;
	XdeScreen *xscr;
	XdeMonitor *xmon;
	gint x = 0, y = 0;

	GetScreens(noshow);

	gdk_display_get_pointer(disp, &scrn, &x, &y, NULL);
	if (!scrn)
		scrn = gdk_display_get_default_screen(disp);
	xscr = screens + gdk_screen_get_number(scrn);
	xmon = xscr->mons + gdk_screen_get_monitor_at_point(scrn, x, y);

	cont = xmon->align;
	ebox = GetPane(cont);

	gtk_widget_show_all(cont);
	gtk_widget_show_now(cont);

#ifndef DO_LOGOUT
#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM))
	if (options.username) {
		gtk_entry_set_text(GTK_ENTRY(user), options.username);
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_widget_set_sensitive(user, FALSE);
		gtk_widget_set_sensitive(pass, TRUE);
		gtk_widget_set_sensitive(buttons[0], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		if (!noshow) {
			if (!GTK_IS_WIDGET(pass))
				EPRINTF("pass is not a widget\n");
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
		}
	} else {
		gtk_entry_set_text(GTK_ENTRY(user), "");
		gtk_entry_set_text(GTK_ENTRY(pass), "");
		gtk_widget_set_sensitive(user, TRUE);
		gtk_widget_set_sensitive(pass, FALSE);
		gtk_widget_set_sensitive(buttons[0], TRUE);
		gtk_widget_set_sensitive(buttons[3], FALSE);
		if (!noshow) {
			if (!GTK_IS_WIDGET(user))
				EPRINTF("user is not a widget\n");
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
		}
	}

#else
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sess));
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *string;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) &&
	    (string = gtk_tree_model_get_string_from_iter(model, &iter))) {
		GtkTreeView *tree = GTK_TREE_VIEW(sess);
		GtkTreePath *path = gtk_tree_path_new_from_string(string);
		GtkTreeViewColumn *cursor = gtk_tree_view_get_column(tree, 1);

		g_free(string);
		gtk_tree_view_set_cursor_on_cell(tree, path, cursor, NULL, FALSE);
		gtk_tree_view_scroll_to_cell(tree, path, cursor, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
	gtk_widget_grab_default(sess);
	gtk_widget_grab_focus(sess);
#endif
	if (!noshow)
		grabbed_window(xscr->wind, NULL);
#else				/* DO_LOGOUT */
	if (!noshow) {
		if (!GTK_IS_WIDGET(controls[LOGOUT_ACTION_LOGOUT]))
			EPRINTF("controls[LOGOUT_ACTION_LOGOUT] is not a widget\n");
		gtk_widget_grab_default(controls[LOGOUT_ACTION_LOGOUT]);
		gtk_widget_grab_focus(controls[LOGOUT_ACTION_LOGOUT]);
		grabbed_window(xscr->wind, NULL);
	}
#endif				/* DO_LOGOUT */
	return xscr->wind;
}

#ifdef DO_XLOCKING
Bool shutting_down;

void
handle_event(Display *dpy, XEvent *xev)
{
	DPRINTF("got event %d\n", xev->type);
	switch (xev->type) {
	case KeyPress:
	case KeyRelease:
	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
	case EnterNotify:
	case LeaveNotify:
	case FocusIn:
	case FocusOut:
	case KeymapNotify:
	case Expose:
	case GraphicsExpose:
	case NoExpose:
	case VisibilityNotify:
	case CreateNotify:
	case DestroyNotify:
	case UnmapNotify:
	case MapNotify:
	case MapRequest:
	case ReparentNotify:
	case ConfigureNotify:
	case ConfigureRequest:
	case GravityNotify:
	case ResizeRequest:
	case CirculateNotify:
	case CirculateRequest:
	case PropertyNotify:
	case SelectionClear:
	case SelectionRequest:
	case SelectionNotify:
	case ColormapNotify:
	case ClientMessage:
	case MappingNotify:
	case GenericEvent:
		break;
	default:
#ifdef DO_XLOCKING
		if (xssEventBase && xev->type == xssEventBase + ScreenSaverNotify) 
			handle_XScreenSaverNotify(dpy, xev);
		else
			EPRINTF("unknown event type %d\n", xev->type);
#endif
		break;
	}
}

void
handle_events(void)
{
	XEvent ev;

	XSync(display.dpy, False);
	while (XPending(display.dpy) && !shutting_down) {
		XNextEvent(display.dpy, &ev);
		handle_event(display.dpy, &ev);
	}
}

gboolean
on_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL|G_IO_HUP|G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
				(cond & G_IO_NVAL) ? "NVAL" : "",
				(cond & G_IO_HUP) ? "HUP" : "",
				(cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		handle_events();
	}
	return TRUE; /* keep event source */
}
#endif				/* DO_XLOCKING */

void
startup_x11(int argc, char *argv[])
{
	if (options.usexde) {
		static const char *suffix = "/.gtkrc-2.0.xde";
		const char *home;
		int len;
		char *file;

		home = getenv("HOME") ? : ".";
		len = strlen(home) + strlen(suffix) + 1;
		file = calloc(len, sizeof(*file));

		strncpy(file, home, len);
		strncat(file, suffix, len);
		gtk_rc_add_default_file(file);
		free(file);
	}

	gtk_init(&argc, &argv);

	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	GdkAtom atom;

	atom = gdk_atom_intern_static_string("_XDE_THEME_NAME");
	_XA_XDE_THEME_NAME = gdk_x11_atom_to_xatom_for_display(disp, atom);
	atom = gdk_atom_intern_static_string("_GTK_READ_RCFILES");
	_XA_GTK_READ_RCFILES = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);
#ifdef DO_XLOCKING
	atom = gdk_atom_intern_static_string("_XDE_XLOCK_COMMAND");
	_XA_XDE_XLOCK_COMMAND = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);
#endif				/* DO_XLOCKING */
	atom = gdk_atom_intern_static_string("_XROOTPMAP_ID");
	_XA_XROOTPMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);
	atom = gdk_atom_intern_static_string("ESETROOT_PMAP_ID");
	_XA_ESETROOT_PMAP_ID = gdk_x11_atom_to_xatom_for_display(disp, atom);

#ifdef DO_XLOCKING
	if (!(display.dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display\n");
		exit(EXIT_FAILURE);
	}
	display.xfd = ConnectionNumber(display.dpy);
	GIOChannel *chan = g_io_channel_unix_new(display.xfd);
	guint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;

	g_io_add_watch(chan, mask, on_watch, NULL);
#endif				/* DO_XLOCKING */
}

void
ShowScreen(XdeScreen *xscr)
{
	if (xscr->wind) {
		gtk_widget_show_now(GTK_WIDGET(xscr->wind));
#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM))
#ifndef DO_LOGOUT
		if (options.username) {
			gtk_widget_set_sensitive(user, FALSE);
			gtk_widget_set_sensitive(pass, TRUE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			DPRINTF("grabbing password entry widget\n");
			if (!GTK_IS_WIDGET(pass))
				EPRINTF("pass is not a widget\n");
			gtk_widget_grab_default(GTK_WIDGET(pass));
			gtk_widget_grab_focus(GTK_WIDGET(pass));
		} else {
			gtk_widget_set_sensitive(user, TRUE);
			gtk_widget_set_sensitive(pass, FALSE);
			gtk_widget_set_sensitive(buttons[0], TRUE);
			gtk_widget_set_sensitive(buttons[3], FALSE);
			if (!GTK_IS_WIDGET(user))
				EPRINTF("user is not a widget\n");
			DPRINTF("grabbing username entry widget\n");
			gtk_widget_grab_default(GTK_WIDGET(user));
			gtk_widget_grab_focus(GTK_WIDGET(user));
		}
#else
		if (!GTK_IS_WIDGET(controls[LOGOUT_ACTION_LOGOUT]))
			EPRINTF("controls[LOGOUT_ACTION_LOGOUT] is not a widget\n");
		gtk_widget_grab_default(controls[LOGOUT_ACTION_LOGOUT]);
		gtk_widget_grab_focus(controls[LOGOUT_ACTION_LOGOUT]);
#endif
#endif				/* !defined DO_CHOOSER */
		grabbed_window(GTK_WIDGET(xscr->wind), NULL);
	}
}

void
ShowScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		ShowScreen(xscr);
}

void
ShowWindow(void)
{
	ShowScreens();
}

void
HideScreen(XdeScreen *xscr)
{
	if (xscr->wind) {
		ungrabbed_window(GTK_WIDGET(xscr->wind));
		gtk_widget_hide(GTK_WIDGET(xscr->wind));
	}
}

void
HideScreens(void)
{
	GdkDisplay *disp = gdk_display_get_default();
	int s, nscr = gdk_display_get_n_screens(disp);
	XdeScreen *xscr;

	for (s = 0, xscr = screens; s < nscr; s++, xscr++)
		HideScreen(xscr);
}

void
HideWindow(void)
{
	DPRINT();

	HideScreens();
}

void
do_run(int argc, char *argv[])
{
	startup_x11(argc, argv);
	top = GetWindow(False);
	gtk_main();
}

static void
xdeSetProperties(SmcConn smcConn, SmPointer data)
{
	char userID[20];
	int i, j, argc = saveArgc;
	char **argv = saveArgv;
	char *cwd = NULL;
	char hint;
	struct passwd *pw;
	SmPropValue *penv = NULL, *prst = NULL, *pcln = NULL;
	SmPropValue propval[11];
	SmProp prop[11];

	SmProp *props[11] = {
		&prop[0], &prop[1], &prop[2], &prop[3], &prop[4],
		&prop[5], &prop[6], &prop[7], &prop[8], &prop[9],
		&prop[10]
	};

	j = 0;

	/* CloneCommand: This is like the RestartCommand except it restarts a
	   copy of the application.  The only difference is that the
	   application doesn't supply its client id at register time.  On POSIX 
	   systems the type should be a LISTofARRAY8. */
	prop[j].name = SmCloneCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = pcln = calloc(argc, sizeof(*pcln));
	prop[j].num_vals = 0;
	props[j] = &prop[j];
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[j].vals[prop[j].num_vals].value = (SmPointer) argv[i];
			prop[j].vals[prop[j].num_vals++].length = strlen(argv[i]);
		}
	}
	j++;

#if 0
	/* CurrentDirectory: On POSIX-based systems, specifies the value of the 
	   current directory that needs to be set up prior to starting the
	   program and should be of type ARRAY8. */
	prop[j].name = SmCurrentDirectory;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = NULL;
	propval[j].length = 0;
	cwd = calloc(PATH_MAX + 1, sizeof(propval[j].value[0]));
	if (getcwd(cwd, PATH_MAX)) {
		propval[j].value = cwd;
		propval[j].length = strlen(propval[j].value);
		j++;
	} else {
		free(cwd);
		cwd = NULL;
	}
#endif

#if 0
	/* DiscardCommand: The discard command contains a command that when
	   delivered to the host that the client is running on (determined from 
	   the connection), will cause it to discard any information about the
	   current state.  If this command is not specified, the SM will assume 
	   that all of the client's state is encoded in the RestartCommand [and 
	   properties].  On POSIX systems the type should be LISTofARRAY8. */
	prop[j].name = SmDiscardCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

#if 0
	char **env;

	/* Environment: On POSIX based systems, this will be of type
	   LISTofARRAY8 where the ARRAY8s alternate between environment
	   variable name and environment variable value. */
	/* XXX: we might want to filter a few out */
	for (i = 0, env = environ; *env; i += 2, env++) ;
	prop[j].name = SmEnvironment;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = penv = calloc(i, sizeof(*penv));
	prop[j].num_vals = i;
	props[j] = &prop[j];
	for (i = 0, env = environ; *env; i += 2, env++) {
		char *equal;
		int len;

		equal = strchrnul(*env, '=');
		len = (int) (*env - equal);
		if (*equal)
			equal++;
		prop[j].vals[i].value = *env;
		prop[j].vals[i].length = len;
		prop[j].vals[i + 1].value = equal;
		prop[j].vals[i + 1].length = strlen(equal);
	}
	j++;
#endif

#if 0
	char procID[20];

	/* ProcessID: This specifies an OS-specific identifier for the process. 
	   On POSIX systems this should be of type ARRAY8 and contain the
	   return of getpid() turned into a Latin-1 (decimal) string. */
	prop[j].name = SmProcessID;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	snprintf(procID, sizeof(procID), "%ld", (long) getpid());
	propval[j].value = procID;
	propval[j].length = strlen(procID);
	j++;
#endif

	/* Program: The name of the program that is running.  On POSIX systems, 
	   this should eb the first parameter passed to execve(3) and should be 
	   of type ARRAY8. */
	prop[j].name = SmProgram;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = argv[0];
	propval[j].length = strlen(argv[0]);
	j++;

	/* RestartCommand: The restart command contains a command that when
	   delivered to the host that the client is running on (determined from
	   the connection), will cause the client to restart in its current
	   state.  On POSIX-based systems this if of type LISTofARRAY8 and each
	   of the elements in the array represents an element in the argv[]
	   array.  This restart command should ensure that the client restarts
	   with the specified client-ID.  */
	prop[j].name = SmRestartCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = prst = calloc(argc + 4, sizeof(*prst));
	prop[j].num_vals = 0;
	props[j] = &prop[j];
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-clientId") || !strcmp(argv[i], "-restore"))
			i++;
		else {
			prop[j].vals[prop[j].num_vals].value = (SmPointer) argv[i];
			prop[j].vals[prop[j].num_vals++].length = strlen(argv[i]);
		}
	}
	prop[j].vals[prop[j].num_vals].value = (SmPointer) "-clientId";
	prop[j].vals[prop[j].num_vals++].length = 9;
	prop[j].vals[prop[j].num_vals].value = (SmPointer) options.clientId;
	prop[j].vals[prop[j].num_vals++].length = strlen(options.clientId);

	prop[j].vals[prop[j].num_vals].value = (SmPointer) "-restore";
	prop[j].vals[prop[j].num_vals++].length = 9;
	prop[j].vals[prop[j].num_vals].value = (SmPointer) options.saveFile;
	prop[j].vals[prop[j].num_vals++].length = strlen(options.saveFile);
	j++;

#if 0
	/* ResignCommand: A client that sets the RestartStyleHint to
	   RestartAnyway uses this property to specify a command that undoes
	   the effect of the client and removes any saved state. */
	prop[j].name = SmResignCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

	/* RestartStyleHint: If the RestartStyleHint property is present, it
	   will contain the style of restarting the client prefers.  If this
	   flag is not specified, RestartIfRunning is assumed.  The possible
	   values are as follows: RestartIfRunning(0), RestartAnyway(1),
	   RestartImmediately(2), RestartNever(3).  The RestartIfRunning(0)
	   style is used in the usual case.  The client should be restarted in
	   the next session if it is connected to the session manager at the
	   end of the current session. The RestartAnyway(1) style is used to
	   tell the SM that the application should be restarted in the next
	   session even if it exits before the current session is terminated.
	   It should be noted that this is only a hint and the SM will follow
	   the policies specified by its users in determining what applications 
	   to restart.  A client that uses RestartAnyway(1) should also set the
	   ResignCommand and ShutdownCommand properties to the commands that
	   undo the state of the client after it exits.  The
	   RestartImmediately(2) style is like RestartAnyway(1) but in addition,
	   the client is meant to run continuously.  If the client exits, the SM
	   should try to restart it in the current session.  The RestartNever(3)
	   style specifies that the client does not wish to be restarted in the
	   next session. */
	prop[j].name = SmRestartStyleHint;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[0];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	hint = SmRestartNever;
	propval[j].value = &hint;
	propval[j].length = 1;
	j++;

#if 0
	/* ShutdownCommand: This command is executed at shutdown time to clean
	   up after a client that is no longer running but retained its state
	   by setting RestartStyleHint to RestartAnyway(1).  The command must
	   not remove any saved state as the client is still part of the
	   session. */
	prop[j].name = SmShutdownCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = "/bin/true";
	propval[j].length = strlen("/bin/true");
	j++;
#endif

	/* UserID: Specifies the user's ID.  On POSIX-based systems this will
	   contain the user's name (the pw_name field of struct passwd).  */
	errno = 0;
	prop[j].name = SmUserID;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	if ((pw = getpwuid(getuid())))
		strncpy(userID, pw->pw_name, sizeof(userID) - 1);
	else {
		EPRINTF("%s: %s\n", "getpwuid()", strerror(errno));
		snprintf(userID, sizeof(userID), "%ld", (long) getuid());
	}
	endpwent();
	propval[j].value = userID;
	propval[j].length = strlen(userID);
	j++;

	SmcSetProperties(smcConn, j, props);

	free(cwd);
	free(pcln);
	free(prst);
	free(penv);
}

static Bool saving_yourself;
static Bool sm_shutting_down;

static void
xdeSaveYourselfPhase2CB(SmcConn smcConn, SmPointer data)
{
	xdeSetProperties(smcConn, data);
	SmcSaveYourselfDone(smcConn, True);
}

/** @brief save yourself
  *
  * The session manager sends a "Save Yourself" message to a client either to
  * check-point it or just before termination so that it can save its state.
  * The client responds with zero or more calls to SmcSetProperties to update
  * the properties indicating how to restart the client.  When all the
  * properties have been set, the client calls SmcSaveYourselfDone.
  *
  * If interact_type is SmcInteractStyleNone, the client must not interact with
  * the user while saving state.  If interact_style is SmInteractStyleErrors,
  * the client may interact with the user only if an error condition arises.  If
  * interact_style is  SmInteractStyleAny then the client may interact with the
  * user for any purpose.  Because only one client can interact with the user at
  * a time, the client must call SmcInteractRequest and wait for an "Interact"
  * message from the session maanger.  When the client is done interacting with
  * the user, it calls SmcInteractDone.  The client may only call
  * SmcInteractRequest() after it receives a "Save Yourself" message and before
  * it calls SmcSaveYourSelfDone().
  */
static void
xdeSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown,
		     int interactStyle, Bool fast)
{
	if (!(sm_shutting_down = shutdown)) {
		if (!SmcRequestSaveYourselfPhase2(smcConn, xdeSaveYourselfPhase2CB, data))
			SmcSaveYourselfDone(smcConn, False);
		return;
	}
	xdeSetProperties(smcConn, data);
	SmcSaveYourselfDone(smcConn, True);
}

/** @brief die
  *
  * The session manager sends a "Die" message to a client when it wants it to
  * die.  The client should respond by calling SmcCloseConnection.  A session
  * manager that behaves properly will send a "Save Yourself" message before the
  * "Die" message.
  */
static void
xdeDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	sm_shutting_down = False;
	gtk_main_quit();
}

static void
xdeSaveCompleteCB(SmcConn smcConn, SmPointer data)
{
	if (saving_yourself) {
		saving_yourself = False;
		gtk_main_quit();
	}
}

/** @brief shutdown cancelled
  *
  * The session manager sends a "Shutdown Cancelled" message when the user
  * cancelled the shutdown during an interaction (see Section 5.5, "Interacting
  * With the User").  The client can now continue as if the shutdown had never
  * happended.  If the client has not called SmcSaveYourselfDone() yet, it can
  * either abort the save and then send SmcSaveYourselfDone() with the success
  * argument set to False or it can continue with the save and then call
  * SmcSaveYourselfDone() with the success argument set to reflect the outcome
  * of the save.
  */
static void
xdeShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	sm_shutting_down = False;
	gtk_main_quit();
}

static unsigned long xdeCBMask =
    SmcSaveYourselfProcMask | SmcDieProcMask |
    SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

static SmcCallbacks xdeCBs = {
	.save_yourself = {
			  .callback = &xdeSaveYourselfCB,
			  .client_data = NULL,
			  },
	.die = {
		.callback = &xdeDieCB,
		.client_data = NULL,
		},
	.save_complete = {
			  .callback = &xdeSaveCompleteCB,
			  .client_data = NULL,
			  },
	.shutdown_cancelled = {
			       .callback = &xdeShutdownCancelledCB,
			       .client_data = NULL,
			       },
};

static gboolean
on_ifd_watch(GIOChannel *chan, GIOCondition cond, pointer data)
{
	SmcConn smcConn = (typeof(smcConn)) data;
	IceConn iceConn = SmcGetIceConnection(smcConn);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		IceProcessMessages(iceConn, NULL, NULL);
	}
	return G_SOURCE_CONTINUE;	/* keep event source */
}

static void
init_smclient(void)
{
	char err[256] = { 0, };
	GIOChannel *chan;
	int ifd, mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	char *env;
	IceConn iceConn;

	if (!(env = getenv("SESSION_MANAGER"))) {
		if (options.clientId)
			EPRINTF("clientId provided but no SESSION_MANAGER\n");
		return;
	}

	smcConn = SmcOpenConnection(env, NULL, SmProtoMajor, SmProtoMinor,
				    xdeCBMask, &xdeCBs, options.clientId,
				    &options.clientId, sizeof(err), err);
	if (!smcConn) {
		EPRINTF("SmcOpenConnection: %s\n", err);
		return;
	}

	iceConn = SmcGetIceConnection(smcConn);

	ifd = IceConnectionNumber(iceConn);
	chan = g_io_channel_unix_new(ifd);
	g_io_add_watch(chan, mask, on_ifd_watch, smcConn);
}

const char *
choose(int argc, char *argv[])
{
	GHashTable *xsessions;
	char *file = NULL;
	char *p;

	/* initialize session managerment functions */
	init_smclient();

	if (!(xsessions = get_xsessions())) {
		EPRINTF("cannot build XSessions\n");
		return (file);
	}
	if (!g_hash_table_size(xsessions)) {
		EPRINTF("cannot find any XSessions\n");
		return (file);
	}
	if (!options.choice)
		options.choice = strdup("default");
	for (p = options.choice; p && *p; p++)
		*p = tolower(*p);
	if (!strcasecmp(options.choice, "default") && options.session) {
		free(options.choice);
		options.choice = strdup(options.session);
	} else if (!strcasecmp(options.choice, "current") && options.current) {
		free(options.choice);
		options.choice = strdup(options.current);
	}

	if (options.session && !g_hash_table_contains(xsessions, options.session)) {
		DPRINTF("Default %s is not available!\n", options.session);
		if (!strcasecmp(options.choice, options.session)) {
			free(options.choice);
			options.choice = strdup("choose");
		}
	}
	if (!strcasecmp(options.choice, "default") && !options.session) {
		DPRINTF("Default is chosen but there is no default\n");
		free(options.choice);
		options.choice = strdup("choose");
	}
	if (strcasecmp(options.choice, "choose") &&
	    !g_hash_table_contains(xsessions, options.choice)) {
		DPRINTF("Choice %s is not available.\n", options.choice);
		free(options.choice);
		options.choice = strdup("choose");
	}
	if (!strcasecmp(options.choice, "choose"))
		options.prompt = True;

	DPRINTF("The default was: %s\n", options.session);
	DPRINTF("Choosing %s...\n", options.choice);
	if (options.prompt)
		do_run(argc, argv);
	if (strcmp(options.current, "logout")) {
		if (!(file = g_hash_table_lookup(xsessions, options.current))) {
			EPRINTF("What happenned to entry for %s?\n", options.current);
			exit(EXIT_FAILURE);
		}
	}
	return (file);
}

void
do_chooser(int argc, char *argv[])
{
	const char *file;

	if (!(file = choose(argc, argv))) {
		DPRINTF("Logging out...\n");
		fputs("logout", stdout);
		return;
	}
	DPRINTF("Launching session %s...\n", options.current);
	if (!options.filename) {
		char *out = strdup(options.current);
		int i;

		for (i = 0; i < strlen(out); i++)
			out[i] = tolower(out[i]);
		fputs(out, stdout);
		free(out);
	} else {
		fputs(file, stdout);
	}
	create_session(options.current, file);
}

#ifdef DO_XLOCKING
/** @brief quit the running background locker
  */
static void
do_quit(int argc, char *argv[])
{
	GdkDisplay *disp;
	int s, nscr;
	char selection[32];

	startup_x11(argc, argv);
	disp = gdk_display_get_default();
	nscr = gdk_display_get_n_screens(disp);
	for (s = 0; s < nscr; s++)
		get_selection(None, selection, s);
}

/** @brief ask running background locker to lock (or just lock the display)
  */
static void
do_lock(int argc, char *argv[])
{
	Display *dpy;
	int s, nscr;
	char selection[32] = { 0, };
	Bool found = False;

#if 0
	/* unfortunately, these are privileged */
	setup_systemd();
	if (sd_session) {
		GError *err = NULL;
		GVariant *result;

		result = g_dbus_proxy_call_sync(sd_session, "Lock",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
		if (!result || err) {
			EPRINTF("Lock: %s: call failed: %s\n", getenv("XDG_SESSION_ID") ? : "self",
				err ? err->message : NULL);
			g_clear_error(&err);
		} else
			g_variant_unref(result);
	}
#endif
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display %s\n", getenv("DISPLAY") ? : "");
		exit(EXIT_FAILURE);
	}
	nscr = ScreenCount(dpy);
	for (s = 0; s < nscr; s++) {
		Window owner;
		XEvent ev;
		Atom atom;

		snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
		atom = XInternAtom(dpy, selection, True);
		if (!atom) {
			DPRINTF("No '%s' atom on display\n", selection);
			continue;
		}
		owner = XGetSelectionOwner(dpy, atom);
		if (!owner) {
			DPRINTF("No '%s' owner on display\n", selection);
			continue;
		}
		atom = XInternAtom(dpy, "_XDE_XLOCK_COMMAND", False);
		if (!atom) {
			EPRINTF("Could not assign atom _XDE_XLOCK_COMMAND\n");
			continue;
		}
		found = True;

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = owner;
		ev.xclient.message_type = atom;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = LockCommandLock;
		ev.xclient.data.l[1] = 0;
		ev.xclient.data.l[2] = 0;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, owner, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	XSync(dpy, True);
	XCloseDisplay(dpy);
	if (!found) {
		DPRINTF("no background instance found\n");
		options.replace = True;
		if (fork() == 0)
			do_run(argc, argv);
	}
}

/** @brief ask running background locker to unlock (or just quit)
  */
static void
do_unlock(int argc, char *argv[])
{
	Display *dpy;
	int s, nscr;
	char selection[32] = { 0, };
	Bool found = False;

#if 0
	/* unfortunately, these are privileged */
	setup_systemd();
	if (sd_session) {
		GError *err = NULL;
		GVariant *result;

		result = g_dbus_proxy_call_sync(sd_session, "Unlock",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
		if (!result || err) {
			EPRINTF("Lock: %s: call failed: %s\n", getenv("XDG_SESSION_ID") ? : "self",
				err ? err->message : NULL);
			g_clear_error(&err);
		} else
			g_variant_unref(result);
	}
#endif
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("cannot open display %s\n", getenv("DISPLAY") ? : "");
		exit(EXIT_FAILURE);
	}
	nscr = ScreenCount(dpy);
	for (s = 0; s < nscr; s++) {
		Window owner;
		XEvent ev;
		Atom atom;

		snprintf(selection, 32, "_XDE_XLOCK_S%d", s);
		atom = XInternAtom(dpy, selection, True);
		if (!atom) {
			DPRINTF("No '%s' atom on display\n", selection);
			continue;
		}
		owner = XGetSelectionOwner(dpy, atom);
		if (!owner) {
			DPRINTF("No '%s' owner on display\n", selection);
			continue;
		}
		atom = XInternAtom(dpy, "_XDE_XLOCK_COMMAND", False);
		if (!atom) {
			EPRINTF("Could not assign atom _XDE_XLOCK_COMMAND\n");
			continue;
		}
		found = True;

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = owner;
		ev.xclient.message_type = atom;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = LockCommandUnlock;
		ev.xclient.data.l[1] = 0;
		ev.xclient.data.l[2] = 0;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;

		XSendEvent(dpy, owner, False, StructureNotifyMask, &ev);
		XFlush(dpy);
	}
	XSync(dpy, True);
	XCloseDisplay(dpy);
	if (!found) {
		EPRINTF("no background instance found\n");
		exit(EXIT_FAILURE);
	}
}
#endif				/* DO_XLOCKING */

gboolean
on_xfd_watch(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n", (cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		exit(EXIT_FAILURE);
	} else if (cond & (G_IO_IN | G_IO_PRI)) {
		XEvent ev;

		while (XPending(dpy) && running) {
			XNextEvent(dpy, &ev);
			XPRINTF("Handling Event\n");
			handle_event(&ev);
		}
	}
	return TRUE;		/* keep event source */

}

Bool
init_display(void)
{
	if (!dpy) {
		int s;
		char sel[64] = { 0, };

		if (!(dpy = XOpenDisplay(0))) {
			EPRINTF("cannot open display\n");
			exit(EXIT_FAILURE);
		}
		XSetErrorHandler(handler);

		PropertyContext = XUniqueContext();
		ClientMessageContext = XUniqueContext();
		ScreenContext = XUniqueContext();
		ClientContext = XUniqueContext();
		MessageContext = XUniqueContext();

		intern_atoms();

		nscr = ScreenCount(dpy);
		if (!(xscreens = calloc(nscr, sizeof(*xscreens)))) {
			EPRINTF("no memory\n");
			exit(EXIT_FAILURE);
		}
		for (s = 0, scr = xscreens; s < nscr; s++, scr++) {
			scr->screen = s;
			scr->root = RootWindow(dpy, s);
			XSaveContext(dpy, scr->root, ScreenContext, (XPointer) scr);
			XSelectInput(dpy, scr->root,
				     VisibilityChangeMask | StructureNotifyMask |
				     SubstructureNotifyMask | FocusChangeMask | PropertyChangeMask);
			snprintf(sel, sizeof(sel), "WM_S%d", s);
			scr->icccm_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_SYSTEM_TRAY_S%d", s);
			scr->stray_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_DESKTOP_LAYOUT_S%d", s);
			scr->pager_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_NET_WM_CM_S%d", s);
			scr->compm_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), "_XDG_ASSIST_S%d", s);
			scr->shelp_atom = XInternAtom(dpy, sel, False);
			snprintf(sel, sizeof(sel), SELECTION_ATOM, s);
			scr->slctn_atom = XInternAtom(dpy, sel, False);
		}
		s = DefaultScreen(dpy);
		scr = xscreens + s;
	}
	return (dpy ? True : False);
}

static void
setup_main_loop(int argc, char *argv[])
{
	int xfd;
	GIOChannel *chan;
	gint mask = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI;
	guint srce;

	gtk_init(NULL, NULL);
	// gtk_init(argc, argv);

	// gdk_error_trap_push();

	/* do this after gtk_init() otherwise we get gdk_xerror() Xlib error handler */
	init_display();

	running = True;
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	chan = g_io_channel_unix_new(xfd);
	srce = g_io_add_watch(chan, mask, on_xfd_watch, NULL);
	(void) srce;
}

static void
announce_selection(void)
{
	XEvent ev;

	ev.xclient.type = ClientMessage;
	ev.xclient.serial = 0;
	ev.xclient.send_event = False;
	ev.xclient.display = dpy;
	ev.xclient.window = scr->root;
	ev.xclient.message_type = _XA_MANAGER;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;	/* FIXME */
	ev.xclient.data.l[1] = scr->slctn_atom;
	ev.xclient.data.l[2] = scr->selwin;
	ev.xclient.data.l[3] = 2;
	ev.xclient.data.l[4] = 0;

	XSendEvent(dpy, scr->root, False, StructureNotifyMask, (XEvent *) &ev);
	XSync(dpy, False);
}

static Bool
selectionreleased(Display *d, XEvent *e, XPointer arg)
{
	int *n = (typeof(n)) arg;

	if (e->type != DestroyNotify)
		return False;
	if (XFindContext(d, e->xdestroywindow.window, ScreenContext, (XPointer *) &scr))
		return False;
	if (!scr->owner || scr->owner != e->xdestroywindow.window)
		return False;
	scr->owner = None;
	*n = *n - 1;
	return True;
}

#if !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM))
/** @brief run without replacing a running instance
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
  * any screen and aborting when one exists.
  */
void
do_run(int argc, char *argv[])
{
	int s;

	for (s = 0; s < nscr; s++) {
		scr = xscreens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy, s), BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if (!(scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
			XSync(dpy, False);
		}
		XUngrabServer(dpy);

		if (scr->owner) {
			EPRINTF("another instance is running on screen %d\n", s);
			exit(EXIT_FAILURE);
		}
		announce_selection();
	}
}
#endif				/* !(defined(DO_CHOOSER)||defined(DO_AUTOSTART)||defined(DO_SESSION)||defined(DO_STARTWM)) */

/** @brief replace running instance with this one
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
  * each screen and setting the selection to our own window.  When the runing
  * instance detects the selection clear event for a managed screen, it will
  * destroy the selection window and exit when no more screens are managed.
  */
void
do_replace(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = xscreens + s;
		scr->selwin =
		    XCreateSimpleWindow(dpy, scr->root, DisplayWidth(dpy, s),
					DisplayHeight(dpy, s), 1, 1, 0, BlackPixel(dpy, s), BlackPixel(dpy, s));
		XSaveContext(dpy, scr->selwin, ScreenContext, (XPointer) scr);
		XSelectInput(dpy, scr->selwin, StructureNotifyMask | PropertyChangeMask);

		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			selcount++;
		}
		XSetSelectionOwner(dpy, scr->slctn_atom, scr->selwin, CurrentTime);
		XSync(dpy, False);
		XUngrabServer(dpy);

		if (!scr->owner)
			announce_selection();
	}
	while (selcount) {
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
		announce_selection();
	}
}

/** @brief ask running instance to quit
  *
  * This is performed by detecting owners of the SELECTION_ATOM selection for
  * each screen and clearing the selection to None.  When the running instance
  * detects the selection clear event for a managed screen, it will destroy
  * the selection window and exit when no more screens are managed.
  */
void
do_quit(int argc, char *argv[])
{
	int s, selcount = 0;
	XEvent ev;

	for (s = 0; s < nscr; s++) {
		scr = xscreens + s;
		XGrabServer(dpy);
		if ((scr->owner = XGetSelectionOwner(dpy, scr->slctn_atom))) {
			XSelectInput(dpy, scr->owner, StructureNotifyMask);
			XSaveContext(dpy, scr->owner, ScreenContext, (XPointer) scr);
			XSetSelectionOwner(dpy, scr->slctn_atom, None, CurrentTime);
			XSync(dpy, False);
			selcount++;
		}
		XUngrabServer(dpy);
	}
	/* sure, wait for them all to destroy */
	while (selcount)
		XIfEvent(dpy, &ev, &selectionreleased, (XPointer) &selcount);
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
	dirs = calloc(len + 1, sizeof(*dirs));
	if (xhome)
		strncpy(dirs, xhome, len);
	else {
		strncpy(dirs, home, len);
		strncat(dirs, "/.local/share", len);
	}
	strncat(dirs, ":", len);
	strncat(dirs, xdata, len);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end; n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
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
	dirs = calloc(len + 1, sizeof(*dirs));
	if (xhome)
		strncpy(dirs, xhome, len);
	else {
		strncpy(dirs, home, len);
		strncat(dirs, "/.config", len);
	}
	strncat(dirs, ":", len);
	strncat(dirs, xconf, len);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end; n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n + 1, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1)
		xdg_dirs[n] = strdup(pos);
	free(dirs);
	if (np)
		*np = n;
	return (xdg_dirs);
}

char **
get_autostart_dirs(int *np)
{
	char *home, *xhome, *xconf, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	home = getenv("HOME") ? : ".";
	xhome = getenv("XDG_CONFIG_HOME");
	xconf = getenv("XDG_CONFIG_DIRS") ? : "/etc/xdg";

	len = (xhome ? strlen(xhome) : strlen(home) + strlen("/.config")) + strlen(xconf) + 2;
	dirs = calloc(len + 1, sizeof(*dirs));
	if (xhome)
		strncpy(dirs, xhome, len);
	else {
		strncpy(dirs, home, len);
		strncat(dirs, "/.config", len);
	}
	strncat(dirs, ":", len);
	strncat(dirs, xconf, len);
	end = dirs + strlen(dirs);
	for (n = 0, pos = dirs; pos < end; n++, *strchrnul(pos, ':') = '\0', pos += strlen(pos) + 1) ;
	xdg_dirs = calloc(n, sizeof(*xdg_dirs));
	for (n = 0, pos = dirs; pos < end; n++, pos += strlen(pos) + 1) {
		len = strlen(pos) + strlen("/autostart") + 1;
		xdg_dirs[n] = calloc(len + 1, sizeof(*xdg_dirs[n]));
		strncpy(xdg_dirs[n], pos, len);
		strncat(xdg_dirs[n], "/autostart", len);
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
	if (!g_key_file_has_key(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL)) {
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

	if (!(name = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL))) {
		DPRINTF("%s: no Name\n", appid);
		return TRUE;
	}
	g_free(name);
	if (!(exec = g_key_file_get_string(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL))) {
		/* TODO: handle DBus activation */
		DPRINTF("%s: no Exec\n", appid);
		return TRUE;
	}
	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL)) {
		DPRINTF("%s: is Hidden\n", appid);
		return TRUE;
	}
#if 0
	/* NoDisplay can be used to hide desktop entries from the application menu and
	   does not indicate that it should not be executed as an autostart entry.  Use
	   Hidden for that. */
	if (g_key_file_get_boolean(entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL)) {
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

		/* parse the first word of the exec statement and see whether it is
		   executable or can be found in PATH */
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
			char *file = calloc(len + 1, sizeof(*file));

			strncpy(file, dir, len);
			strncat(file, "/", len);
			strncat(file, binary, len);
			if (!access(file, X_OK)) {
				execok = TRUE;
				free(file);
				break;
			}
			// too much noise
			// DPRINTF("%s: %s: %s\n", appid, file,
			// strerror(errno);
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

	autostarts = g_hash_table_new_full(g_str_hash, g_str_equal, autostart_key_free, autostart_value_free);

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
			file = calloc(len + 1, sizeof(*file));
			strncpy(file, *dirs, len);
			strncat(file, "/", len);
			strncat(file, d->d_name, len);
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
	/* don't filter stuff out because we want to show non-autostarted appids as
	   insensitive buttons. */
	g_hash_table_foreach_remove(autostarts, autostarts_filter, NULL);
#endif
	return (autostarts);
}

void
create_splashscreen(TableContext * c)
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

static gboolean
blink_button(gpointer user_data)
{
	TaskContext *task = (typeof(task)) user_data;

	if (!task->button) {
		task->blink = 0;
		return G_SOURCE_REMOVE;
	}
	if (gtk_widget_is_sensitive(task->button))
		gtk_widget_set_sensitive(task->button, FALSE);
	else
		gtk_widget_set_sensitive(task->button, TRUE);
	return G_SOURCE_CONTINUE;
}

void
add_task_to_splash(TableContext *c, TaskContext *task)
{
	char *name;
	GtkWidget *icon, *but;

	name = g_key_file_get_string(task->entry, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
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

	gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(but), c->col, c->col + 1, c->row, c->row + 1);
	gtk_widget_set_sensitive(GTK_WIDGET(but), task->runnable);
	gtk_widget_show_all(GTK_WIDGET(but));
	gtk_widget_show_now(GTK_WIDGET(but));
	relax();

	if (++c->col >= c->cols) {
		c->row++;
		c->col = 0;
	}

	if (task->runnable)
		task->blink = g_timeout_add_seconds(1, blink_button, task);
}

GHashTable *
init_autostarts(TableContext **cp)
{
	GHashTable *autostarts;
	TableContext *c = NULL;
	int count;

	if (!(autostarts = get_autostarts())) {
		EPRINTF("cannot build AutoStarts\n");
		return (autostarts);
	}
	if (!(count = g_hash_table_size(autostarts))) {
		EPRINTF("cannot find any AutoStarts\n");
	}
	if (options.splash) {

		c = calloc(1, sizeof(*c));
		c->cols = 7;	/* seems like a good number */
		c->rows = (count + c->cols - 1) / c->cols;
		c->col = 0;
		c->row = 0;

		create_splashscreen(c);
	}
	*cp = c;
	return (autostarts);
}

void
do_autostarts(XdeStartupPhase want, int need, GHashTable *autostarts, TableContext *c)
{
	TaskContext *task;
	GHashTableIter hiter;
	gpointer key, value;

	g_hash_table_iter_init(&hiter, autostarts);
	while (g_hash_table_iter_next(&hiter, &key, &value)) {
		gchar *phase, *categories;
		XdeStartupPhase have = StartupPhaseApplication;
		int wait = WaitForNothing;

		if ((phase = g_key_file_get_string(value, G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-Autostart-Phase", NULL)) ||
		    (phase = g_key_file_get_string(value, G_KEY_FILE_DESKTOP_GROUP, "AutostartPhase", NULL))) {
			if (!strcmp(phase, "PreDisplayServer")) {
				have = StartupPhasePreDisplayServer;
				wait &= ~WaitForAll;
			} else if (!strcmp(phase, "Initialization")) {
				have = StartupPhaseInitialization;
				wait &= ~WaitForAll;
			} else if (!strcmp(phase, "WindowManager")) {
				have = StartupPhaseWindowManager;
				wait &= ~(WaitForManager|WaitForComposite|WaitForTray|WaitForPager);
			} else if (!strcmp(phase, "Panel")) {
				have = StartupPhasePanel;
				wait &= ~(WaitForTray|WaitForPager);
				wait |= WaitForManager;
			} else if (!strcmp(phase, "Desktop")) {
				have = StartupPhaseDesktop;
				wait |= WaitForManager;
			} else if (!strcmp(phase, "Application") || !strcmp(phase, "Applications")) {
				have = StartupPhaseApplication;
				wait |= WaitForManager;
			} else {
				EPRINTF("unrecognized phase: %s\n", phase);
				have = StartupPhaseApplication;
				wait |= WaitForManager;
			}
			g_free(phase);
		} else {
			have = StartupPhaseApplication;
			wait |= WaitForManager;
		}
		if (have != want)
			continue;

		if ((categories = g_key_file_get_string(value, G_KEY_FILE_DESKTOP_GROUP, "Categories", NULL))) {
			if (strstr(categories, "Audio"))
				wait |= WaitForAudio;
			if (strstr(categories, "TrayIcon"))
				wait |= WaitForTray;
			if (strstr(categories, "DockApp"))
				wait |= WaitForManager;
			if (strstr(categories, "SystemTray"))
				wait &= ~WaitForTray;
			g_free(categories);
		}

		if (need && !(wait & need))
			continue;

		task = calloc(1, sizeof(*task));
		task->appid = key;
		task->entry = value;
		task->waitfor = wait;
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

/** @brief perform pre-autostart executions.
  */
void
do_executes()
{
}

void
do_autostart(int argc, char *argv[])
{
	GHashTable *autostarts;
	TableContext *c;

	/* FIXME: write me */

	setup_main_loop(argc, argv);
	do_executes();
	if ((autostarts = init_autostarts(&c))) {
		XdeStartupPhase want;

		for (want = StartupPhasePreDisplayServer; want <= StartupPhaseApplication; want++) {
			int i, need;

			for (i = 0; i <= WaitForCount; i++) {
				need = (i == WaitForCount) ? WaitForNothing : (1 << i);
				do_autostarts(want, need, autostarts, c);
			}
		}
	}
	gtk_main();
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
"	,argv[0]
	);
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
			{"display",	    required_argument,	NULL, 'd'},
			{"desktop",	    required_argument,	NULL, 'e'},
			{"session",	    required_argument,	NULL, 's'},
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
			{"authfile",	    required_argument,	NULL, 'a'},
#endif
#ifdef DO_XLOCKING
			{"locker",	    no_argument,	NULL, 'L'},
			{"replace",	    no_argument,	NULL, 'r'},
			{"lock",	    no_argument,	NULL, 'l'},
			{"unlock",	    no_argument,	NULL, 'U'},
			{"quit",	    no_argument,	NULL, 'q'},
#endif					/* DO_XLOCKING */
#ifdef DO_XCHOOSER
			{"xdmaddress",	    required_argument,	NULL, 'x'},
			{"clientaddress",   required_argument,	NULL, 'c'},
			{"connectionType",  required_argument,	NULL, 't'},
			{"welcome",	    required_argument,	NULL, 'w'},
#else					/* DO_XCHOOSER */
			{"prompt",	    no_argument,	NULL, 'p'},
#endif					/* DO_XCHOOSER */
			{"banner",	    required_argument,	NULL, 'b'},
			{"splash",	    required_argument,	NULL, 'S'},
			{"side",	    required_argument,	NULL, 's'},
			{"exec",	    required_argument,	NULL, 'x'},
			{"noask",	    no_argument,	NULL, 'n'},
			{"file",	    required_argument,	NULL, 'f'},
			{"noautostart",	    no_argument,	NULL, 'a'},
			{"autostart",	    no_argument,	NULL, '1'},
			{"wait",	    no_argument,	NULL, 'w'},
			{"nowait",	    no_argument,	NULL, '2'},
			{"pause",	    optional_argument,	NULL, 'p'},
			{"nosplash",	    no_argument,	NULL, '4'},

			{"toolwait",	    optional_argument,	NULL, 'W'},

			{"charset",	    required_argument,	NULL, 'c'},
			{"language",	    required_argument,	NULL, 'l'},
			{"default",	    no_argument,	NULL, 'd'},
			{"icons",	    required_argument,	NULL, 'i'},
			{"theme",	    required_argument,	NULL, 't'},
			{"exec",	    no_argument,	NULL, 'e'},
			{"xde-theme",	    no_argument,	NULL, 'x'},

			{"foreground",	    no_argument,	NULL, 'F'},

			{"timeout",	    required_argument,	NULL, 'T'},
			{"filename",	    no_argument,	NULL, 'f'},
			{"vendor",	    required_argument,	NULL, '5'},
			{"xsessions",	    no_argument,	NULL, 'X'},
			{"default",	    required_argument,	NULL, '6'},
#ifndef DO_LOGOUT
			{"username",	    required_argument,	NULL, '7'},
			{"guard",	    required_argument,	NULL, 'g'},
#endif				/* !defined DO_LOGOUT */
			{"setbg",	    no_argument,	NULL, '8'},
			{"transparent",	    no_argument,	NULL, '9'},
			{"tray",	    no_argument,	NULL, 'y'},

			{"clientId",	    required_argument,	NULL, '3'},
			{"restore",	    required_argument,	NULL, '4'},

			{"dry-run",	    no_argument,		NULL, 'n'},
			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
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
#if defined(DO_XLOGIN) || defined(DO_XCHOOSER) || defined(DO_GREETER)
		case 'a':	/* -a, --authfile */
			free(options.authfile);
			options.authfile = strndup(optarg, PATH_MAX);
			break;
#endif
#ifdef DO_XLOCKING
		case 'L':	/* -L, --locker */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandLocker;
			options.command = CommandLocker;
			options.replace = False;
			break;
		case 'r':	/* -r, --replace */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandReplace;
			options.command = CommandReplace;
			options.replace = True;
			break;
		case 'l':	/* -l, --lock */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandLock;
			options.command = CommandLock;
			break;
		case 'U':	/* -U, --unlock */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandUnlock;
			options.command = CommandUnlock;
			break;
		case 'q':	/* -q, --quit */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandQuit;
			options.command = CommandQuit;
			options.replace = True;
			break;
#endif
		case 'p':	/* -p, --prompt */
			options.prompt = True;
			break;
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strdup(optarg);
			break;
		case 'w':	/* -w, --welcome WELCOME */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
			break;
		case 'l':	/* -l, --splash [IMAGE] */
			if (optarg) {
				free(options.banner);
				options.banner = strdup(optarg);
			}
			options.splash = True;
			break;
		case 8:	/* -s, --side {top|bottom|left|right} */
			if (!strncasecmp(optarg, "left", strlen(optarg))) {
				options.side = LogoSideLeft;
				break;
			}
			if (!strncasecmp(optarg, "top", strlen(optarg))) {
				options.side = LogoSideTop;
				break;
			}
			if (!strncasecmp(optarg, "right", strlen(optarg))) {
				options.side = LogoSideRight;
				break;
			}
			if (!strncasecmp(optarg, "bottom", strlen(optarg))) {
				options.side = LogoSideBottom;
				break;
			}
			goto bad_option;
		case 'x':	/* -x, --exec COMMAND */
			options.execute = realloc(options.execute, (options.commands + 1) *
						  sizeof(*options.execute));
			options.execute[options.commands++] = strdup(optarg);
			break;
		case 'n':	/* -n, --noask */
			options.noask = True;
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
		case 1:	/* -p, --pause [PAUSE] */
			if (optarg) {
				if ((val = strtoul(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
					goto bad_option;
				options.pause = val;
			}
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
					if ((val = strtoul(d, &endptr, 0)) < 0 || (endptr && *endptr))
						goto bad_option;
					options.delay = val;
				}
				if (*g) {
					if ((val = strtoul(g, &endptr, 0)) < 0 || (endptr && *endptr))
						goto bad_option;
					options.guard = val;
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
		case 2:	/* -d, --default */
			options.setdflt = True;
			break;
		case 'i':	/* -i, --icons THEME */
			free(options.icon_theme);
			options.icon_theme = strdup(optarg);
			break;
		case 't':	/* -t, --theme THEME */
			free(options.gtk2_theme);
			options.gtk2_theme = strdup(optarg);
			break;
		case 3:	/* -e, --exec */
			options.launch = True;
			break;
		case 9:	/* -x, --xde-theme */
			options.usexde = True;
			break;

		case 'F':	/* -F, --foreground */
			options.foreground = True;
			break;

		case 'T':	/* -T, --timeout TIMEOUT */
			if ((val = strtoul(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			options.timeout = val;
			break;
		case 4:	/* -f, --filename */
			options.filename = True;
			break;
		case '5':	/* --vendor VENDOR */
			free(options.vendor);
			options.vendor = strdup(optarg);
			break;
		case 5:	/* -X, --xsessions */
			options.xsession = True;
			break;
		case '6':	/* --default DEFAULT */
			free(options.choice);
			options.choice = strdup(optarg);
			break;
#ifndef DO_LOGOUT
		case '7':	/* --username USERNAME */
			free(options.username);
			options.username = strdup(optarg);
			break;
		case 'g':	/* -g, --guard SECONDS */
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
				goto bad_option;
			options.protect = val;
			break;
#endif				/* !defined DO_LOGOUT */
		case '8':	/* --setbg */
			options.setbg = True;
			break;
		case '9':	/* --transparent */
			options.transparent = True;
			break;
		case 'y':	/* -y, --tray */
			options.tray = True;
			break;

		case '3':	/* -clientId CLIENTID */
			free(options.clientId);
			options.clientId = strdup(optarg);
			break;
		case 6:	/* -restore SAVEFILE */
			free(options.saveFile);
			options.saveFile = strdup(optarg);
			break;

		case 7:	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			if (optarg == NULL) {
				DPRINTF("%s: increasing debug verbosity\n", argv[0]);
				options.debug++;
				break;
			}
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
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
			if ((val = strtol(optarg, &endptr, 0)) < 0 || (endptr && *endptr))
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
