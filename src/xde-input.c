/*****************************************************************************

 Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>
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
#include <wordexp.h>
#include <execinfo.h>
#include <math.h>

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
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/xf86misc.h>
#include <X11/XKBlib.h>
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <X11/SM/SMlib.h>
#include <gio/gio.h>
#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include <pwd.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#define XPRINTF(_args...) do { } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
		fprintf(stderr, NAME ": D: %12s: +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
		fprintf(stderr, NAME ": E: %12s +%4d : %s() : ", __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output > _num) { \
		fprintf(stdout, NAME ": I: "); \
		fprintf(stdout, _args); fflush(stdout); } } while (0)

#define PTRACE(_num) do { if (options.debug >= _num || options.output >= _num) { \
		fprintf(stderr, NAME ": T: %12s +%4d : %s()\n", __FILE__, __LINE__, __func__); \
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

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define EXIT_SYNTAXERR	2

const char *program = NAME;

#define XA_SELECTION_NAME	"_XDE_INPUT_S%d"

static int saveArgc;
static char **saveArgv;

#define RESNAME "xde-input"
#define RESCLAS "XDE-Input"
#define RESTITL "X11 Input"

#define USRDFLT "%s/.config/" RESNAME "/rc"
#define APPDFLT "/usr/share/X11/app-defaults/" RESCLAS

static Atom _XA_XDE_THEME_NAME;
static Atom _XA_GTK_READ_RCFILES;
static Atom _XA_XDE_INPUT_EDIT;

typedef enum {
	CommandDefault = 0,
	CommandRun,
	CommandReplace,
	CommandQuit,
	CommandEditor,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

Display *dpy;

typedef struct {
	int debug;
	int output;
	char *display;
	int screen;
	int button;
	Bool dryrun;
	char *filename;
	Command command;
	char *clientId;
	char *saveFile;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.display = NULL,
	.screen = -1,
	.button = 0,
	.dryrun = False,
	.filename = NULL,
	.command = CommandDefault,
	.clientId = NULL,
	.saveFile = NULL,
};

typedef struct {
	int index;	    /* index */
	GdkDisplay *disp;
	GdkScreen *scrn;    /* screen */
	GdkWindow *root;
	char *theme;	    /* XDE theme name */
	Window selwin;	    /* selection owner window */
	Atom atom;	    /* selection atom for this screen */
} XdeScreen;

XdeScreen *screens;			/* array of screens */

typedef struct {
	Bool Keyboard;	    /* support for core Keyboard */
	Bool Pointer;	    /* support for core Pointer */
	Bool ScreenSaver;   /* support for extension ScreenSaver */
	Bool DPMS;	    /* support for extension DPMS */
	Bool XKeyboard;	    /* support for extension XKEYBOARD */
	Bool XF86Misc;	    /* support for extension XF86MISC */
} Support;

Support support;

typedef struct {
	XKeyboardState Keyboard;
	struct {
		int accel_numerator;
		int accel_denominator;
		int threshold;
	} Pointer;
	struct {
		int opcode;
		int event;
		int error;
		int major_version;
		int minor_version;
		XkbDescPtr desc;
	} XKeyboard;
	struct {
		int event; /* event base */
		int error; /* error base */
		int major_version;
		int minor_version;
		int timeout;
		int interval;
		int prefer_blanking;
		int allow_exposures;
		XScreenSaverInfo info;
	} ScreenSaver;
	struct {
		int event; /* event base */
		int error; /* error base */
		int major_version;
		int minor_version;
		CARD16 power_level;
		BOOL state;
		CARD16 standby;
		CARD16 suspend;
		CARD16 off;
	} DPMS;
	struct {
		int event; /* event base */
		int error; /* error base */
		int major_version;
		int minor_version;
		XF86MiscMouseSettings mouse;
		XF86MiscKbdSettings keyboard;
	} XF86Misc;
} State;

State state;

GKeyFile *file = NULL;

typedef struct {
	struct {
		GtkWidget *GlobalAutoRepeat;
		GtkWidget *KeyClickPercent;
		GtkWidget *BellPercent;
		GtkWidget *BellPitch;
		GtkWidget *BellDuration;
	} Keyboard;
	struct {
		GtkWidget *AccelerationNumerator;
		GtkWidget *AccelerationDenominator;
		GtkWidget *Threshold;
	} Pointer;
	struct {
		GtkWidget *Timeout;
		GtkWidget *Interval;
		GtkWidget *Preferblanking;
		GtkWidget *Allowexposures;
	} ScreenSaver;
	struct {
		GtkWidget *State;
		GtkWidget *StandbyTimeout;
		GtkWidget *SuspendTimeout;
		GtkWidget *OffTimeout;
	} DPMS;
	struct {
		GtkWidget *RepeatKeysEnabled;
		GtkWidget *RepeatDelay;
		GtkWidget *RepeatInterval;
		GtkWidget *SlowKeysEnabled;
		GtkWidget *SlowKeysDelay;
		GtkWidget *BounceKeysEnabled;
		GtkWidget *DebounceDelay;
		GtkWidget *StickyKeysEnabled;
		GtkWidget *MouseKeysEnabled;
		GtkWidget *MouseKeysDfltBtn;
		GtkWidget *MouseKeysAccelEnabled;
		GtkWidget *MouseKeysDelay;
		GtkWidget *MouseKeysInterval;
		GtkWidget *MouseKeysTimeToMax;
		GtkWidget *MouseKeysMaxSpeed;
		GtkWidget *MouseKeysCurve;
	} XKeyboard;
	struct {
		GtkWidget *KeyboardRate;
		GtkWidget *KeyboardDelay;
		GtkWidget *MouseEmulate3Buttons;
		GtkWidget *MouseEmulate3Timeout;
		GtkWidget *MouseChordMiddle;
	} XF86Misc;
} Controls;

Controls controls;

GtkWindow *editor = NULL;

typedef struct {
	struct {
		Bool GlobalAutoRepeat;
		int KeyClickPercent;
		int BellPercent;
		unsigned int BellPitch;
		unsigned int BellDuration;
	} Keyboard;
	struct {
		unsigned int AccelerationNumerator;
		unsigned int AccelerationDenominator;
		unsigned int Threshold;
	} Pointer;
	struct {
		unsigned int Timeout;
		unsigned int Interval;
		unsigned int Preferblanking;
		unsigned int Allowexposures;
	} ScreenSaver;
	struct {
		int State;
		unsigned int StandbyTimeout;
		unsigned int SuspendTimeout;
		unsigned int OffTimeout;
	} DPMS;
	struct {
		Bool RepeatKeysEnabled;
		unsigned int RepeatDelay;
		unsigned int RepeatInterval;
		Bool SlowKeysEnabled;
		unsigned int SlowKeysDelay;
		Bool BounceKeysEnabled;
		unsigned int DebounceDelay;
		Bool StickyKeysEnabled;
		Bool MouseKeysEnabled;
		unsigned int MouseKeysDfltBtn;
		Bool MouseKeysAccelEnabled;
		unsigned int MouseKeysDelay;
		unsigned int MouseKeysInterval;
		unsigned int MouseKeysTimeToMax;
		unsigned int MouseKeysMaxSpeed;
		unsigned int MouseKeysCurve;
	} XKeyboard;
	struct {
		unsigned int KeyboardRate;
		unsigned int KeyboardDelay;
		Bool MouseEmulate3Buttons;
		unsigned int MouseEmulate3Timeout;
		Bool MouseChordMiddle;
	} XF86Misc;
} Resources;

Resources resources = {
};

static const char *KFG_Pointer = "Pointer";
static const char *KFG_Keyboard = "Keyboard";
static const char *KFG_XKeyboard = "XKeyboard";
static const char *KFG_ScreenSaver = "ScreenSaver";
static const char *KFG_DPMS = "DPMS";
static const char *KFG_XF86Misc = "XF86Misc";

static const char *KFK_Pointer_AccelerationDenominator = "AccelerationDenominator";
static const char *KFK_Pointer_AccelerationNumerator = "AccelerationNumerator";
static const char *KFK_Pointer_Threshold = "Threshold";

const char *KFK_Keyboard_AutoRepeats = "AutoRepeats";
static const char *KFK_Keyboard_BellDuration = "BellDuration";
static const char *KFK_Keyboard_BellPercent = "BellPercent";
static const char *KFK_Keyboard_BellPitch = "BellPitch";
static const char *KFK_Keyboard_GlobalAutoRepeat = "GlobalAutoRepeat";
static const char *KFK_Keyboard_KeyClickPercent = "KeyClickPercent";
const char *KFK_Keyboard_LEDMask = "LEDMask";

static const char *KFK_XKeyboard_AccessXFeedbackMaskEnabled = "AccessXFeedbackMaskEnabled";
static const char *KFK_XKeyboard_AccessXKeysEnabled = "AccessXKeysEnabled";
static const char *KFK_XKeyboard_AccessXOptions = "AccessXOptions";
static const char *KFK_XKeyboard_AccessXOptionsEnabled = "AccessXOptionsEnabled";
static const char *KFK_XKeyboard_AccessXTimeout = "AccessXTimeout";
static const char *KFK_XKeyboard_AccessXTimeoutMask = "AccessXTimeoutMask";
static const char *KFK_XKeyboard_AccessXTimeoutMaskEnabled = "AccessXTimeoutMaskEnabled";
static const char *KFK_XKeyboard_AccessXTimeoutOptionsMask = "AccessXTimeoutOptionsMask";
static const char *KFK_XKeyboard_AccessXTimeoutOptionsValues = "AccessXTimeoutOptionsValues";
static const char *KFK_XKeyboard_AccessXTimeoutValues = "AccessXTimeoutValues";
static const char *KFK_XKeyboard_AudibleBellMaskEnabled = "AudibleBellMaskEnabled";
static const char *KFK_XKeyboard_BounceKeysEnabled = "BounceKeysEnabled";
static const char *KFK_XKeyboard_ControlsEnabledEnabled = "ControlsEnabledEnabled";
static const char *KFK_XKeyboard_DebounceDelay = "DebounceDelay";
static const char *KFK_XKeyboard_GroupsWrapEnabled = "GroupsWrapEnabled";
static const char *KFK_XKeyboard_IgnoreGroupLockModsEnabled = "IgnoreGroupLockModsEnabled";
static const char *KFK_XKeyboard_IgnoreLockModsEnabled = "IgnoreLockModsEnabled";
static const char *KFK_XKeyboard_InternalModsEnabled = "InternalModsEnabled";
static const char *KFK_XKeyboard_MouseKeysAccelEnabled = "MouseKeysAccelEnabled";
static const char *KFK_XKeyboard_MouseKeysCurve = "MouseKeysCurve";
static const char *KFK_XKeyboard_MouseKeysDelay = "MouseKeysDelay";
static const char *KFK_XKeyboard_MouseKeysDfltBtn = "MouseKeysDfltBtn";
static const char *KFK_XKeyboard_MouseKeysEnabled = "MouseKeysEnabled";
static const char *KFK_XKeyboard_MouseKeysInterval = "MouseKeysInterval";
static const char *KFK_XKeyboard_MouseKeysMaxSpeed = "MouseKeysMaxSpeed";
static const char *KFK_XKeyboard_MouseKeysTimeToMax = "MouseKeysTimeToMax";
static const char *KFK_XKeyboard_Overlay1MaskEnabled = "Overlay1MaskEnabled";
static const char *KFK_XKeyboard_Overlay2MaskEnabled = "Overlay2MaskENabled";
static const char *KFK_XKeyboard_PerKeyRepeatEnabled = "PerKeyRepeatEnabled";
const char *KFK_XKeyboard_PerKeyRepeat = "PerKeyRepeat";
static const char *KFK_XKeyboard_RepeatDelay = "RepeatDelay";
static const char *KFK_XKeyboard_RepeatInterval = "RepeatInterval";
static const char *KFK_XKeyboard_RepeatKeysEnabled = "RepeatKeysEnabled";
const char *KFK_XKeyboard_RepeatRate = "RepeatRate";
static const char *KFK_XKeyboard_SlowKeysDelay = "SlowKeysDelay";
static const char *KFK_XKeyboard_SlowKeysEnabled = "SlowKeysEnabled";
static const char *KFK_XKeyboard_StickyKeysEnabled = "StickyKeysEnabled";

static const char *KFK_ScreenSaver_AllowExposures = "AllowExposures";
static const char *KFK_ScreenSaver_Interval = "Interval";
static const char *KFK_ScreenSaver_PreferBlanking = "PreferBlanking";
static const char *KFK_ScreenSaver_Timeout = "Timeout";

static const char *KFK_DPMS_OffTimeout = "OffTimeout";
static const char *KFK_DPMS_PowerLevel = "PowerLevel";
static const char *KFK_DPMS_StandbyTimeout = "StandbyTimeout";
static const char *KFK_DPMS_State = "State";
static const char *KFK_DPMS_SuspendTimeout = "SuspendTimeout";

static const char *KFK_XF86Misc_KeyboardRate = "KeyboardRate";
static const char *KFK_XF86Misc_KeyboardDelay = "KeyboardDelay";
static const char *KFK_XF86Misc_MouseEmulate3Buttons = "MouseEmulate3Buttons";
static const char *KFK_XF86Misc_MouseEmulate3Timeout = "MouseEmulate3Timeout";
static const char *KFK_XF86Misc_MouseChordMiddle = "MouseChordMiddle";


static void
edit_set_values()
{
	int value;
	gboolean flag;

	if (!file) {
		EPRINTF("KEY FILE NOT ALLOCATED! <============\n");
		return;
	}
	if (support.Keyboard) {
		value = g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_KeyClickPercent, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Keyboard.KeyClickPercent), value);
		value = g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellPercent, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Keyboard.BellPercent), value);
		value = g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellPitch, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Keyboard.BellPitch), value);
		value = g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellDuration, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Keyboard.BellDuration), value);
		flag = g_key_file_get_boolean(file, KFG_Keyboard, KFK_Keyboard_GlobalAutoRepeat, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.Keyboard.GlobalAutoRepeat), flag);
	}
	if (support.Pointer) {
		value = g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_AccelerationNumerator, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Pointer.AccelerationNumerator), value);
		value = g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_AccelerationDenominator, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Pointer.AccelerationDenominator), value);
		value = g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_Threshold, NULL);
		gtk_range_set_value(GTK_RANGE(controls.Pointer.Threshold), value);
	}
	if (support.XKeyboard) {
		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_RepeatKeysEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.RepeatKeysEnabled), flag);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_RepeatDelay, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.RepeatDelay), value);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_RepeatInterval, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.RepeatInterval), value);

		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_SlowKeysEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.SlowKeysEnabled), flag);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_SlowKeysDelay, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.SlowKeysDelay), value);

		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_StickyKeysEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.StickyKeysEnabled), flag);

		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_BounceKeysEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.BounceKeysEnabled), flag);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_DebounceDelay, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.DebounceDelay), value);

		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.MouseKeysEnabled), flag);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysDfltBtn, NULL);
		gtk_combo_box_set_active(GTK_COMBO_BOX(controls.XKeyboard.MouseKeysDfltBtn), value - 1);

		flag = g_key_file_get_boolean(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysAccelEnabled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XKeyboard.MouseKeysAccelEnabled), flag);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysDelay, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.MouseKeysDelay), value);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysInterval, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.MouseKeysInterval), value);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysTimeToMax, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.MouseKeysTimeToMax), value);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysMaxSpeed, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.MouseKeysMaxSpeed), value);
		value = g_key_file_get_integer(file, KFG_XKeyboard, KFK_XKeyboard_MouseKeysCurve, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XKeyboard.MouseKeysCurve), value);
	}
	if (support.ScreenSaver) {
		value = g_key_file_get_integer(file, KFG_ScreenSaver, KFK_ScreenSaver_Timeout, NULL);
		gtk_range_set_value(GTK_RANGE(controls.ScreenSaver.Timeout), value);
		value = g_key_file_get_integer(file, KFG_ScreenSaver, KFK_ScreenSaver_Interval, NULL);
		gtk_range_set_value(GTK_RANGE(controls.ScreenSaver.Interval), value);
		flag = g_key_file_get_boolean(file, KFG_ScreenSaver, KFK_ScreenSaver_PreferBlanking, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.ScreenSaver.Preferblanking), flag);
		flag = g_key_file_get_boolean(file, KFG_ScreenSaver, KFK_ScreenSaver_AllowExposures, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.ScreenSaver.Allowexposures), flag);
	}
	if (support.DPMS) {
		value = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_StandbyTimeout, NULL);
		gtk_range_set_value(GTK_RANGE(controls.DPMS.StandbyTimeout), value);
		value = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_SuspendTimeout, NULL);
		gtk_range_set_value(GTK_RANGE(controls.DPMS.SuspendTimeout), value);
		value = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_OffTimeout, NULL);
		gtk_range_set_value(GTK_RANGE(controls.DPMS.OffTimeout), value);
		flag = g_key_file_get_boolean(file, KFG_DPMS, KFK_DPMS_State, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.DPMS.State), flag);
	}
	if (support.XF86Misc) {
		value = g_key_file_get_integer(file, KFG_XF86Misc, KFK_XF86Misc_KeyboardRate, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XF86Misc.KeyboardRate), value);
		value = g_key_file_get_integer(file, KFG_XF86Misc, KFK_XF86Misc_KeyboardDelay, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XF86Misc.KeyboardDelay), value);
		flag = g_key_file_get_boolean(file, KFG_XF86Misc, KFK_XF86Misc_MouseEmulate3Buttons, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XF86Misc.MouseEmulate3Buttons), flag);
		value = g_key_file_get_integer(file, KFG_XF86Misc, KFK_XF86Misc_MouseEmulate3Timeout, NULL);
		gtk_range_set_value(GTK_RANGE(controls.XF86Misc.MouseEmulate3Timeout), value);
		flag = g_key_file_get_boolean(file, KFG_XF86Misc, KFK_XF86Misc_MouseChordMiddle, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.XF86Misc.MouseChordMiddle), flag);
	}
}

static void
process_errors()
{
}

static void
purge_queue()
{
}

/** @brief read input settings
  * 
  * Read the input settings from the configuration file.  Simple and direct.
  * The file is an .ini-style keyfile.  Use glib to read in the values.
  */
static void
read_input(const char *filename)
{
	GError *error = NULL;

	PTRACE(5);
	if (!file && (file = g_key_file_new())) {
		g_key_file_load_from_file(file, filename, G_KEY_FILE_NONE, &error);
		if (error) {
			EPRINTF("COULD NOT LOAD KEY FILE %s! <============= %s\n", filename, error->message);
			g_error_free(error);
		}
	}
}

static void
get_input()
{
	char buf[256] = { 0, };
	int i, j;

	PTRACE(5);
	read_input(options.filename);
	if (!file) {
		EPRINTF("KEY FILE NOT ALLOCATED! <============\n");
		return;
	}
	if (support.Keyboard) {
		XGetKeyboardControl(dpy, &state.Keyboard);

		if (options.debug) {
			fputs("Keyboard Control:\n", stderr);
			fprintf(stderr, "\tkey-click-percent: %d\n",
				state.Keyboard.key_click_percent);
			fprintf(stderr, "\tbell-percent: %d\n", state.Keyboard.bell_percent);
			fprintf(stderr, "\tbell-pitch: %u Hz\n", state.Keyboard.bell_pitch);
			fprintf(stderr, "\tbell-duration: %u milliseconds\n",
				state.Keyboard.bell_duration);
			fprintf(stderr, "\tled-mask: 0x%08lx\n", state.Keyboard.led_mask);
			fprintf(stderr, "\tglobal-auto-repeat: %s\n",
				state.Keyboard.global_auto_repeat ? "Yes" : "No");
			fputs("\tauto-repeats: ", stderr);
			for (i = 0; i < 32; i++)
				fprintf(stderr, "%02X", state.Keyboard.auto_repeats[i]);
			fputs("\n", stderr);
		}

		resources.Keyboard.KeyClickPercent = state.Keyboard.key_click_percent;
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_KeyClickPercent,
				       state.Keyboard.key_click_percent);

		resources.Keyboard.BellPercent = state.Keyboard.bell_percent;
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellPercent, state.Keyboard.bell_percent);

		resources.Keyboard.BellPitch = state.Keyboard.bell_pitch;
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellPitch, state.Keyboard.bell_pitch);

		resources.Keyboard.BellDuration = state.Keyboard.bell_duration;
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellDuration, state.Keyboard.bell_duration);

		/* FIXME: do resources */
		g_key_file_set_integer(file, KFG_Keyboard,
				KFK_Keyboard_LEDMask, state.Keyboard.led_mask);

		resources.Keyboard.GlobalAutoRepeat = state.Keyboard.global_auto_repeat ? True : False;
		g_key_file_set_boolean(file, KFG_Keyboard,
				       KFK_Keyboard_GlobalAutoRepeat,
				       state.Keyboard.global_auto_repeat);

		/* FIXME: do resources */
		for (i = 0, j = 0; i < 32; i++, j += 2)
			snprintf(buf + j, sizeof(buf) - j, "%02X", state.Keyboard.auto_repeats[i]);
		g_key_file_set_string(file, KFG_Keyboard, KFK_Keyboard_AutoRepeats, buf);
	}

	if (support.Pointer) {
		XGetPointerControl(dpy, &state.Pointer.accel_numerator,
				   &state.Pointer.accel_denominator, &state.Pointer.threshold);

		if (options.debug) {
			fputs("Pointer Control:\n", stderr);
			fprintf(stderr, "\tacceleration-numerator: %d\n",
				state.Pointer.accel_numerator);
			fprintf(stderr, "\tacceleration-denominator: %d\n",
				state.Pointer.accel_denominator);
			fprintf(stderr, "\tthreshold: %d\n", state.Pointer.threshold);
		}

		resources.Pointer.AccelerationDenominator = state.Pointer.accel_denominator;
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_AccelerationDenominator,
				       state.Pointer.accel_denominator);
		resources.Pointer.AccelerationNumerator = state.Pointer.accel_numerator;
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_AccelerationNumerator,
				       state.Pointer.accel_numerator);
		resources.Pointer.Threshold = state.Pointer.threshold;
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_Threshold, state.Pointer.threshold);
	}

	if (support.ScreenSaver) {

		XGetScreenSaver(dpy, &state.ScreenSaver.timeout, &state.ScreenSaver.interval,
				&state.ScreenSaver.prefer_blanking,
				&state.ScreenSaver.allow_exposures);

		if (options.debug) {
			fputs("Screen Saver:\n", stderr);
			fprintf(stderr, "\ttimeout: %d seconds\n", state.ScreenSaver.timeout);
			fprintf(stderr, "\tinterval: %d seconds\n", state.ScreenSaver.interval);
			fputs("\tprefer-blanking: ", stderr);
			switch (state.ScreenSaver.prefer_blanking) {
			case DontPreferBlanking:
				fputs("DontPreferBlanking\n", stderr);
				break;
			case PreferBlanking:
				fputs("PreferBlanking\n", stderr);
				break;
			case DefaultBlanking:
				fputs("DefaultBlanking\n", stderr);
				break;
			default:
				fprintf(stderr, "(unknown) %d\n",
					state.ScreenSaver.prefer_blanking);
				break;
			}
			fputs("\tallow-exposures: ", stderr);
			switch (state.ScreenSaver.allow_exposures) {
			case DontAllowExposures:
				fputs("DontAllowExposures\n", stderr);
				break;
			case AllowExposures:
				fputs("AllowExposures\n", stderr);
				break;
			case DefaultExposures:
				fputs("DefaultExposures\n", stderr);
				break;
			default:
				fprintf(stderr, "(unknown) %d\n",
					state.ScreenSaver.allow_exposures);
				break;
			}
		}

		switch (state.ScreenSaver.allow_exposures) {
		case DontAllowExposures:
			strncpy(buf, "DontAllowExposures", sizeof(buf));
			break;
		case AllowExposures:
			strncpy(buf, "AllowExposures", sizeof(buf));
			break;
		case DefaultExposures:
			strncpy(buf, "DefaultExposures", sizeof(buf));
			break;
		default:
			snprintf(buf, sizeof(buf), "%d", state.ScreenSaver.allow_exposures);
			break;
		}
		resources.ScreenSaver.Allowexposures = state.ScreenSaver.allow_exposures;
		g_key_file_set_string(file, KFG_ScreenSaver, KFK_ScreenSaver_AllowExposures, buf);

		resources.ScreenSaver.Interval = state.ScreenSaver.interval;
		g_key_file_set_integer(file, KFG_ScreenSaver,
				       KFK_ScreenSaver_Interval, state.ScreenSaver.interval);

		switch (state.ScreenSaver.prefer_blanking) {
		case DontPreferBlanking:
			strncpy(buf, "DontPreferBlanking", sizeof(buf));
			break;
		case PreferBlanking:
			strncpy(buf, "PreferBlanking", sizeof(buf));
			break;
		case DefaultBlanking:
			strncpy(buf, "DefaultBlanking", sizeof(buf));
			break;
		default:
			snprintf(buf, sizeof(buf), "%d", state.ScreenSaver.prefer_blanking);
			break;
		}
		resources.ScreenSaver.Preferblanking = state.ScreenSaver.prefer_blanking;
		g_key_file_set_string(file, KFG_ScreenSaver, KFK_ScreenSaver_PreferBlanking, buf);
		resources.ScreenSaver.Timeout = state.ScreenSaver.timeout;
		g_key_file_set_integer(file, KFG_ScreenSaver,
				       KFK_ScreenSaver_Timeout, state.ScreenSaver.timeout);
	}

	if (support.DPMS) {
		DPMSGetTimeouts(dpy, &state.DPMS.standby, &state.DPMS.suspend, &state.DPMS.off);
		if (options.debug) {
			fputs("DPMS:\n", stderr);
			fprintf(stderr, "\tDPMS Version: %d.%d\n", state.DPMS.major_version,
				state.DPMS.minor_version);
			fputs("\tpower-level: ", stderr);
			switch (state.DPMS.power_level) {
			case DPMSModeOn:
				fputs("DPMSModeOn\n", stderr);
				break;
			case DPMSModeStandby:
				fputs("DPMSModeStandby\n", stderr);
				break;
			case DPMSModeSuspend:
				fputs("DPMSModeSuspend\n", stderr);
				break;
			case DPMSModeOff:
				fputs("DPMSModeOff\n", stderr);
				break;
			default:
				fprintf(stderr, "%d (unknown)\n", state.DPMS.power_level);
				break;
			}
			fprintf(stderr, "\tstate: %s\n", state.DPMS.state ? "True" : "False");
			fprintf(stderr, "\tstandby-timeout: %hu seconds\n", state.DPMS.standby);
			fprintf(stderr, "\tsuspend-timeout: %hu seconds\n", state.DPMS.suspend);
			fprintf(stderr, "\toff-timeout: %hu seconds\n", state.DPMS.off);
		}

		switch (state.DPMS.power_level) {
		case DPMSModeOn:
			strncpy(buf, "DPMSModeOn", sizeof(buf));
			break;
		case DPMSModeStandby:
			strncpy(buf, "DPMSModeStandby", sizeof(buf));
			break;
		case DPMSModeSuspend:
			strncpy(buf, "DPMSModeSuspend", sizeof(buf));
			break;
		case DPMSModeOff:
			strncpy(buf, "DPMSModeOff", sizeof(buf));
			break;
		default:
			snprintf(buf, sizeof(buf), "%d (unknown)", state.DPMS.power_level);
			break;
		}
		/* FIXME: resources.DPMS.PowerLevel = state.DPMS.power_level; */
		g_key_file_set_string(file, KFG_DPMS, KFK_DPMS_PowerLevel, buf);
		resources.DPMS.State = state.DPMS.state;
		g_key_file_set_boolean(file, KFG_DPMS,
				       KFK_DPMS_State, state.DPMS.state ? TRUE : FALSE);
		resources.DPMS.StandbyTimeout = state.DPMS.standby;
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_StandbyTimeout, state.DPMS.standby);
		resources.DPMS.SuspendTimeout = state.DPMS.suspend;
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_SuspendTimeout, state.DPMS.suspend);
		resources.DPMS.OffTimeout = state.DPMS.off;
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_OffTimeout, state.DPMS.off);
	}

	PTRACE(5);
	if (support.XKeyboard) {
		state.XKeyboard.desc = XkbGetKeyboard(dpy, XkbControlsMask, XkbUseCoreKbd);
		if (!state.XKeyboard.desc) {
			EPRINTF("NO XKEYBOARD DESCRIPTION!\n");
			exit(EXIT_FAILURE);
		}
		XkbGetControls(dpy, XkbAllControlsMask, state.XKeyboard.desc);
		if (!state.XKeyboard.desc->ctrls) {
			EPRINTF("NO XKEYBOARD DESCRIPTION CONTROLS!\n");
			exit(EXIT_FAILURE);
		}
#if 0
		unsigned int which = XkbControlsMask;
#endif

		PTRACE(5);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysDfltBtn,
				       state.XKeyboard.desc->ctrls->mk_dflt_btn);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_RepeatKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbRepeatKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_SlowKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbSlowKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_BounceKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbBounceKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_StickyKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbStickyKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbMouseKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysAccelEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbMouseKeysAccelMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXTimeoutMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXTimeoutMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXFeedbackMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXFeedbackMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AudibleBellMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAudibleBellMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_Overlay1MaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbOverlay1Mask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_Overlay2MaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbOverlay2Mask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_IgnoreGroupLockModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbIgnoreGroupLockMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_GroupsWrapEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbGroupsWrapMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_InternalModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbInternalModsMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_IgnoreLockModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbIgnoreLockModsMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_PerKeyRepeatEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbPerKeyRepeatMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_ControlsEnabledEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbControlsEnabledMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXOptionsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXOptionsMask ? TRUE : FALSE);

		{
			unsigned int repeat_delay, repeat_interval;

			XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &repeat_delay, &repeat_interval);

			g_key_file_set_integer(file, KFG_XKeyboard,
					       KFK_XKeyboard_RepeatDelay,
					       state.XKeyboard.desc->ctrls->repeat_delay);
			g_key_file_set_integer(file, KFG_XKeyboard,
					       KFK_XKeyboard_RepeatInterval,
					       state.XKeyboard.desc->ctrls->repeat_interval);
		}

		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_SlowKeysDelay,
				       state.XKeyboard.desc->ctrls->slow_keys_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_DebounceDelay,
				       state.XKeyboard.desc->ctrls->debounce_delay);

		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysDelay,
				       state.XKeyboard.desc->ctrls->mk_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysInterval,
				       state.XKeyboard.desc->ctrls->mk_interval);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysTimeToMax,
				       state.XKeyboard.desc->ctrls->mk_time_to_max);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysMaxSpeed,
				       state.XKeyboard.desc->ctrls->mk_max_speed);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysCurve,
				       state.XKeyboard.desc->ctrls->mk_curve);

		static struct {
			unsigned short mask;
			char *name;
		} axoptions[12] = {
			{
			XkbAX_SKPressFBMask, "SlowKeysPress"}, {
			XkbAX_SKAcceptFBMask, "SlowKeysAccept"}, {
			XkbAX_FeatureFBMask, "Feature"}, {
			XkbAX_SlowWarnFBMask, "SlowWarn"}, {
			XkbAX_IndicatorFBMask, "Indicator"}, {
			XkbAX_StickyKeysFBMask, "StickyKeys"}, {
			XkbAX_TwoKeysMask, "TwoKeys"}, {
			XkbAX_LatchToLockMask, "LatchToLock"}, {
			XkbAX_SKReleaseFBMask, "SlowKeysRelease"}, {
			XkbAX_SKRejectFBMask, "SlowKeysReject"}, {
			XkbAX_BKRejectFBMask, "BounceKeysReject"}, {
			XkbAX_DumbBellFBMask, "DumbBell"}
		};

		for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
			if (state.XKeyboard.desc->ctrls->ax_options & axoptions[i].mask) {
				if (j++)
					strncat(buf, ";", sizeof(buf) - 1);
				strncat(buf, axoptions[i].name, sizeof(buf) - 1);
			}
		}
		g_key_file_set_string(file, KFG_XKeyboard, KFK_XKeyboard_AccessXOptions, buf);

		/* XkbAX_SKPressFBMask */
		/* XkbAX_SKAcceptFBMask */
		/* XkbAX_FeatureFBMask */
		/* XkbAX_SlowWarnFBMask */
		/* XkbAX_IndicatorFBMask */
		/* XkbAX_StickKeysFBMask */
		/* XkbAX_TwoKeysMask */
		/* XkbAX_LatchToLockMask */
		/* XkbAX_SKReleaseFBMask */
		/* XkbAX_SKRejectFBMask */
		/* XkbAX_BKRejectFBMask */
		/* XkbAX_DumbBellFBMask */

		/* XkbAX_FBOptionsMask */
		/* XkbAX_SKOptionsMask */
		/* XkbAX_AllOptiosMask */

		{
#if 0
			int ax_timeout, axt_ctrls_mask, axt_ctrls_values, axt_opts_mask,
			    axt_opts_values;

			XkbGetAccessXTimeout(dpy, XkbUseCoreKbd, &ax_timeout, &axt_ctrls_mask,
					     &axt_ctrls_values, &axt_opts_mask, &axt_opts_values);
#endif

			g_key_file_set_integer(file, KFG_XKeyboard,
					       KFK_XKeyboard_AccessXTimeout,
					       state.XKeyboard.desc->ctrls->ax_timeout);
			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_opts_mask & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf) - 1);
					strncat(buf, axoptions[i].name, sizeof(buf) - 1);
				}

			}
			g_key_file_set_string(file, KFG_XKeyboard,
					      KFK_XKeyboard_AccessXTimeoutOptionsMask, buf);

			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->
				    axt_opts_values & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf) - 1);
					strncat(buf, axoptions[i].name, sizeof(buf) - 1);
				}

			}
			g_key_file_set_string(file, KFG_XKeyboard,
					      KFK_XKeyboard_AccessXTimeoutOptionsValues, buf);

			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_ctrls_mask & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf) - 1);
					strncat(buf, axoptions[i].name, sizeof(buf) - 1);
				}

			}
			g_key_file_set_string(file, KFG_XKeyboard,
					      KFK_XKeyboard_AccessXTimeoutMask, buf);

			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->
				    axt_ctrls_values & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf) - 1);
					strncat(buf, axoptions[i].name, sizeof(buf) - 1);
				}

			}
			g_key_file_set_string(file, KFG_XKeyboard,
					      KFK_XKeyboard_AccessXTimeoutValues, buf);
		}
	}
	if (support.XF86Misc) {
		XF86MiscGetKbdSettings(dpy, &state.XF86Misc.keyboard);
		g_key_file_set_integer(file, KFG_XF86Misc,
				       KFK_XF86Misc_KeyboardRate, state.XF86Misc.keyboard.rate);
		g_key_file_set_integer(file, KFG_XF86Misc,
				       KFK_XF86Misc_KeyboardDelay, state.XF86Misc.keyboard.delay);

		XF86MiscGetMouseSettings(dpy, &state.XF86Misc.mouse);
		g_key_file_set_boolean(file, KFG_XF86Misc,
				       KFK_XF86Misc_MouseEmulate3Buttons,
				       state.XF86Misc.mouse.emulate3buttons ? TRUE : FALSE);
		g_key_file_set_integer(file, KFG_XF86Misc,
				       KFK_XF86Misc_MouseEmulate3Timeout,
				       state.XF86Misc.mouse.emulate3timeout);
		g_key_file_set_boolean(file, KFG_XF86Misc,
				       KFK_XF86Misc_MouseChordMiddle,
				       state.XF86Misc.mouse.chordmiddle ? TRUE : FALSE);
	}
	PTRACE(5);
}

/** @brief write input settings
  *
  * Write the input settings back to the configuration file.  Simple and direct.
  * The file is an .ini-style keyfile.  Use glib to write out the values.
  */
void
write_input()
{
	PTRACE(5);
}

static void
set_input(const char *filename)
{
	PTRACE(5);
	read_input(filename);
	if (!file) {
		EPRINTF("KEY FILE NOT ALLOCATED! <============\n");
		return;
	}
	PTRACE(5);
	if (g_key_file_has_group(file, KFG_Keyboard) && support.Keyboard) {
		XKeyboardControl kbd = { 0, };
		unsigned long value_mask = 0;

		PTRACE(5);
		if ((kbd.key_click_percent =
		     g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_KeyClickPercent,
					    NULL)))
			value_mask |= KBKeyClickPercent;
		if ((kbd.bell_percent =
		     g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellPercent, NULL)))
			value_mask |= KBBellPercent;
		if ((kbd.bell_pitch =
		     g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellPitch, NULL)))
			value_mask |= KBBellPitch;
		if ((kbd.bell_duration =
		     g_key_file_get_integer(file, KFG_Keyboard, KFK_Keyboard_BellDuration, NULL)))
			value_mask |= KBBellDuration;
		if ((kbd.auto_repeat_mode =
		     g_key_file_get_boolean(file, KFG_Keyboard, KFK_Keyboard_GlobalAutoRepeat,
					    NULL)))
			value_mask |= KBAutoRepeatMode;
		if (value_mask)
			XChangeKeyboardControl(dpy, value_mask, &kbd);
	}
	PTRACE(5);
	if (g_key_file_has_group(file, KFG_Pointer) && support.Pointer) {
		Bool do_accel, do_threshold;
		int accel_numerator, accel_denominator, threshold;

		PTRACE(5);
		accel_denominator =
		    g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_AccelerationDenominator,
					   NULL);
		accel_numerator =
		    g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_AccelerationNumerator,
					   NULL);
		threshold = g_key_file_get_integer(file, KFG_Pointer, KFK_Pointer_Threshold, NULL);
		do_accel = (accel_denominator && accel_numerator) ? True : False;
		do_threshold = threshold ? True : False;
		XChangePointerControl(dpy, do_accel, do_threshold, accel_numerator,
				      accel_denominator, threshold);
	}
	PTRACE(5);
	if (g_key_file_has_group(file, KFG_XKeyboard) && support.XKeyboard) {
		PTRACE(5);
	}
	PTRACE(5);
	if (g_key_file_has_group(file, KFG_ScreenSaver) && support.ScreenSaver) {
		int timeout, interval, prefer_blanking, allow_exposures;

		PTRACE(5);
		timeout =
		    g_key_file_get_integer(file, KFG_ScreenSaver, KFK_ScreenSaver_Timeout, NULL);
		interval =
		    g_key_file_get_integer(file, KFG_ScreenSaver, KFK_ScreenSaver_Interval, NULL);
		prefer_blanking =
		    g_key_file_get_boolean(file, KFG_ScreenSaver, KFK_ScreenSaver_PreferBlanking,
					   NULL);
		allow_exposures =
		    g_key_file_get_boolean(file, KFG_ScreenSaver, KFK_ScreenSaver_AllowExposures,
					   NULL);
		XSetScreenSaver(dpy, timeout, interval, prefer_blanking, allow_exposures);
	}
	PTRACE(5);
	if (g_key_file_has_group(file, KFG_DPMS) && support.DPMS) {
		int standby, suspend, off, level;

		PTRACE(5);
		standby = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_StandbyTimeout, NULL);
		suspend = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_SuspendTimeout, NULL);
		off = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_OffTimeout, NULL);
		DPMSSetTimeouts(dpy, standby, suspend, off);
		if (g_key_file_get_boolean(file, KFG_DPMS, KFK_DPMS_State, NULL)) {
			DPMSEnable(dpy);
		} else {
			DPMSDisable(dpy);
		}
		level = g_key_file_get_integer(file, KFG_DPMS, KFK_DPMS_PowerLevel, NULL);
		DPMSForceLevel(dpy, level);
	}
	PTRACE(5);
}

void
reprocess_input()
{
	PTRACE(5);
	get_input();
	edit_set_values();
	process_errors();
	purge_queue();
}

static void
startitup()
{
	Window window = RootWindow(dpy, options.screen);
	Bool missing = False;

	PTRACE(5);
	support.Keyboard = True;
	DPRINTF(1, "Keyboard: core protocol support\n");
	support.Pointer = True;
	DPRINTF(1, "Pointer: core protocol support\n");
	if (XkbQueryExtension(dpy, &state.XKeyboard.opcode, &state.XKeyboard.event,
			      &state.XKeyboard.error, &state.XKeyboard.major_version,
			      &state.XKeyboard.minor_version)) {
		support.XKeyboard = True;
		DPRINTF(1, "XKeyboard: opcode=%d, event=%d, error=%d, major=%d, minor=%d\n",
			state.XKeyboard.opcode, state.XKeyboard.event, state.XKeyboard.error,
			state.XKeyboard.major_version, state.XKeyboard.minor_version);
	} else {
		missing = True;
		support.XKeyboard = False;
		DPRINTF(1, "XKeyboard: not supported\n");
	}
	if (XScreenSaverQueryExtension(dpy, &state.ScreenSaver.event, &state.ScreenSaver.error) &&
	    XScreenSaverQueryVersion(dpy, &state.ScreenSaver.major_version,
				     &state.ScreenSaver.minor_version)) {
		support.ScreenSaver = True;
		DPRINTF(1, "ScreenSaver: event=%d, error=%d, major=%d, minor=%d\n",
			state.ScreenSaver.event, state.ScreenSaver.error,
			state.ScreenSaver.major_version, state.ScreenSaver.minor_version);
		XScreenSaverQueryInfo(dpy, window, &state.ScreenSaver.info);
		if (options.debug) {
			fputs("Screen Saver:\n", stderr);
			fputs("\tstate: ", stderr);
			switch (state.ScreenSaver.info.state) {
			case ScreenSaverOff:
				fputs("Off\n", stderr);
				break;
			case ScreenSaverOn:
				fputs("On\n", stderr);
				break;
			case ScreenSaverDisabled:
				fputs("Disabled\n", stderr);
				break;
			default:
				fprintf(stderr, "%d (unknown)\n", state.ScreenSaver.info.state);
				break;
			}
			fputs("\tkind: ", stderr);
			switch (state.ScreenSaver.info.kind) {
			case ScreenSaverBlanked:
				fputs("Blanked\n", stderr);
				break;
			case ScreenSaverInternal:
				fputs("Internal\n", stderr);
				break;
			case ScreenSaverExternal:
				fputs("External\n", stderr);
				break;
			default:
				fprintf(stderr, "%d (unknown)\n", state.ScreenSaver.info.kind);
				break;
			}
			fprintf(stderr, "\ttil-or-since: %lu milliseconds\n",
				state.ScreenSaver.info.til_or_since);
			fprintf(stderr, "\tidle: %lu milliseconds\n", state.ScreenSaver.info.idle);
			fputs("\tevent-mask: ", stderr);
			if (state.ScreenSaver.info.eventMask & ScreenSaverNotifyMask)
				fputs("NotifyMask ", stderr);
			if (state.ScreenSaver.info.eventMask & ScreenSaverCycleMask)
				fputs("CycleMask", stderr);
			fputs("\n", stderr);
		}
	} else {
		missing = True;
		support.ScreenSaver = False;
		DPRINTF(1, "ScreenSaver: not supported\n");
	}
	if (DPMSQueryExtension(dpy, &state.DPMS.event, &state.DPMS.error) &&
	    DPMSGetVersion(dpy, &state.DPMS.major_version, &state.DPMS.minor_version)) {
		support.DPMS = True;
		DPRINTF(1, "DPMS: event=%d, error=%d, major=%d, minor=%d\n",
			state.DPMS.event, state.DPMS.error, state.DPMS.major_version,
			state.DPMS.minor_version);
	} else {
		missing = True;
		support.DPMS = False;
		DPRINTF(1, "DPMS: not supported\n");
	}
	if (XF86MiscQueryExtension(dpy, &state.XF86Misc.event, &state.XF86Misc.error) &&
	    XF86MiscQueryVersion(dpy, &state.XF86Misc.major_version,
				 &state.XF86Misc.minor_version)) {
		support.XF86Misc = True;
		DPRINTF(1, "XF86Misc: event=%d, error=%d, major=%d, minor=%d\n",
			state.XF86Misc.event, state.XF86Misc.error, state.XF86Misc.major_version,
			state.XF86Misc.minor_version);
	} else {
		missing = True;
		support.XF86Misc = False;
		DPRINTF(1, "XF86Misc: not supported\n");
	}
	if (missing && options.debug > 0) {
		char **list;
		int i, n = 0;

		if ((list = XListExtensions(dpy, &n))) {
			fputs("Extensions are:", stderr);
			for (i = 0; i < n; i++)
				fprintf(stderr, " %s", list[i]);
			fputs("\n", stderr);
			XFreeExtensionList(list);
		}
	}
}

static gchar *
format_value_milliseconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%.6g ms", /* gtk_scale_get_digits(scale), */ value);
}

static gchar *
format_value_seconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%.6g s", /* gtk_scale_get_digits(scale), */ value);
}

static gchar *
format_value_percent(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%.6g%%", /* gtk_scale_get_digits(scale), */ value);
}

static char *
format_value_hertz(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%.6g Hz", /* gtk_scale_get_digits(scale), */ value);
}

static void
accel_numerator_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Pointer.accel_numerator) {
		PTRACE(5);
		XChangePointerControl(dpy, True, False, val, state.Pointer.accel_denominator,
				      state.Pointer.threshold);
		reprocess_input();
	}
}

static void
accel_denominator_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Pointer.accel_denominator) {
		PTRACE(5);
		XChangePointerControl(dpy, True, False, state.Pointer.accel_numerator, val,
				      state.Pointer.threshold);
		reprocess_input();
	}
}

static void
threshold_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value;
	int val;

	value = gtk_range_get_value(range);
	val = round(value);
	if (val != state.Pointer.threshold) {
		PTRACE(5);
		XChangePointerControl(dpy, False, True,
				      state.Pointer.accel_denominator,
				      state.Pointer.accel_numerator, val);
		reprocess_input();
	}
}

static void
global_autorepeat_toggled(GtkToggleButton * button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	int val = active ? AutoRepeatModeOn : AutoRepeatModeOff;

	if (val != state.Keyboard.global_auto_repeat) {
		XKeyboardControl kb = {
			.auto_repeat_mode = val,
		};
		PTRACE(5);
		XChangeKeyboardControl(dpy, KBAutoRepeatMode, &kb);
		reprocess_input();
	}

}

static void
keyclick_percent_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Keyboard.key_click_percent) {
		XKeyboardControl kb = {
			.key_click_percent = val,
		};
		PTRACE(5);
		XChangeKeyboardControl(dpy, KBKeyClickPercent, &kb);
		reprocess_input();
	}
}

static void
bell_percent_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Keyboard.bell_percent) {
		XKeyboardControl kb = {
			.bell_percent = val,
		};
		PTRACE(5);
		XChangeKeyboardControl(dpy, KBBellPercent, &kb);
		reprocess_input();
	}
}

static void
bell_pitch_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Keyboard.bell_pitch) {
		XKeyboardControl kb = {
			.bell_pitch = val,
		};
		PTRACE(5);
		XChangeKeyboardControl(dpy, KBBellPitch, &kb);
		reprocess_input();
	}
}

static void
ring_bell_clicked(GtkButton *button, gpointer user_data)
{
	XBell(dpy, 0);
	XFlush(dpy);
}


static void
bell_duration_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Keyboard.bell_duration) {
		XKeyboardControl kb = {
			.bell_duration = val,
		};
		PTRACE(5);
		XChangeKeyboardControl(dpy, KBBellDuration, &kb);
		reprocess_input();
	}
}

static void
repeat_keys_toggled(GtkToggleButton * button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbRepeatKeysMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbRepeatKeysMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbRepeatKeysMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbRepeatKeysMask;
		XkbSetControls(dpy, XkbRepeatKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
repeat_delay_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->repeat_delay) {
		state.XKeyboard.desc->ctrls->repeat_delay = val;
#if 0
		XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, state.XKeyboard.desc->ctrls->repeat_delay,
				     state.XKeyboard.desc->ctrls->repeat_interval);
#else
		PTRACE(5);
		XkbSetControls(dpy, XkbRepeatKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
repeat_interval_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->repeat_interval) {
		state.XKeyboard.desc->ctrls->repeat_interval = val;
#if 0
		XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, state.XKeyboard.desc->ctrls->repeat_delay,
				     state.XKeyboard.desc->ctrls->repeat_interval);
#else
		PTRACE(5);
		XkbSetControls(dpy, XkbRepeatKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
slow_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbSlowKeysMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbSlowKeysMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbSlowKeysMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbSlowKeysMask;
		XkbSetControls(dpy, XkbSlowKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
slow_keys_delay_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->slow_keys_delay) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->slow_keys_delay = val;
		XkbSetControls(dpy, XkbSlowKeysMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
bounce_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbBounceKeysMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbBounceKeysMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbBounceKeysMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbBounceKeysMask;
		XkbSetControls(dpy, XkbBounceKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
debounce_delay_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->debounce_delay) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->debounce_delay = val;
		XkbSetControls(dpy, XkbBounceKeysMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
sticky_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbStickyKeysMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbStickyKeysMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbStickyKeysMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbStickyKeysMask;
		XkbSetControls(dpy, XkbStickyKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
mouse_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbMouseKeysMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbMouseKeysMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbMouseKeysMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbMouseKeysMask;
		XkbSetControls(dpy, XkbMouseKeysMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
default_mouse_button_changed(GtkComboBox *box, gpointer user_data)
{
	gint value = gtk_combo_box_get_active(box);
	unsigned char val = value + 1;

	if (val != state.XKeyboard.desc->ctrls->mk_dflt_btn) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_dflt_btn = val;
		XkbSetControls(dpy, XkbMouseKeysMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
mouse_keys_accel_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	unsigned int value = active ? XkbMouseKeysAccelMask : 0;

	if (value != (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbMouseKeysAccelMask)) {
#if 1
		PTRACE(5);
		XkbChangeEnabledControls(dpy, XkbUseCoreKbd, XkbMouseKeysAccelMask, value);
#else
		state.XKeyboard.desc->ctrls->enabled_ctrls ^= XkbMouseKeysAccelMask;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
#endif
		reprocess_input();
	}
}

static void
mouse_keys_delay_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->mk_delay) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_delay = val;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
mouse_keys_interval_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->mk_interval) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_interval = val;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
mouse_keys_time_to_max_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->mk_time_to_max) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_time_to_max = val;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
mouse_keys_max_speed_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	unsigned short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->mk_max_speed) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_max_speed = val;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
mouse_keys_curve_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	short val = round(value);

	if (val != state.XKeyboard.desc->ctrls->mk_curve) {
		PTRACE(5);
		state.XKeyboard.desc->ctrls->mk_curve = val;
		XkbSetControls(dpy, XkbMouseKeysAccelMask, state.XKeyboard.desc);
		reprocess_input();
	}
}

static void
screensaver_timeout_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.ScreenSaver.timeout) {
		PTRACE(5);
		state.ScreenSaver.timeout = val;
		XSetScreenSaver(dpy,
				state.ScreenSaver.timeout,
				state.ScreenSaver.interval,
				state.ScreenSaver.prefer_blanking,
				state.ScreenSaver.allow_exposures);
		reprocess_input();
	}
}

static void
activate_screensaver_clicked(GtkButton *button, gpointer user_data)
{
	PTRACE(5);
	XForceScreenSaver(dpy, ScreenSaverActive);
	XFlush(dpy);
}

static void
screensaver_interval_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.ScreenSaver.interval) {
		PTRACE(5);
		state.ScreenSaver.interval = val;
		XSetScreenSaver(dpy,
				state.ScreenSaver.timeout,
				state.ScreenSaver.interval,
				state.ScreenSaver.prefer_blanking,
				state.ScreenSaver.allow_exposures);
		reprocess_input();
	}
}

static void
rotate_screensaver_clicked(GtkButton *button, gpointer user_data)
{
	PTRACE(5);
	XForceScreenSaver(dpy, ScreenSaverActive);
	XFlush(dpy);
}

static void
prefer_blanking_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	int value = active ? PreferBlanking : DontPreferBlanking;

	if (value != state.ScreenSaver.prefer_blanking) {
		PTRACE(5);
		state.ScreenSaver.prefer_blanking = value;
		XSetScreenSaver(dpy,
				state.ScreenSaver.timeout,
				state.ScreenSaver.interval,
				state.ScreenSaver.prefer_blanking,
				state.ScreenSaver.allow_exposures);
		reprocess_input();
	}
}

static void
allow_exposures_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	int value = active ? AllowExposures : DontAllowExposures;

	if (value != state.ScreenSaver.allow_exposures) {
		PTRACE(5);
		state.ScreenSaver.allow_exposures = value;
		XSetScreenSaver(dpy,
				state.ScreenSaver.timeout,
				state.ScreenSaver.interval,
				state.ScreenSaver.prefer_blanking,
				state.ScreenSaver.allow_exposures);
		reprocess_input();
	}
}

static void
dpms_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	PTRACE(5);
	if (active)
		DPMSEnable(dpy);
	else
		DPMSDisable(dpy);
	XFlush(dpy);
	reprocess_input();
}

static void
standby_timeout_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	CARD16 val = round(value);

	if (val != state.DPMS.standby) {
		PTRACE(5);
		state.DPMS.standby = val;
		DPMSSetTimeouts(dpy,
				state.DPMS.standby,
				state.DPMS.suspend,
				state.DPMS.off);
		reprocess_input();
	}
}

static void
activate_standby_clicked(GtkButton *button, gpointer user_data)
{
	PTRACE(5);
	DPMSForceLevel(dpy, DPMSModeStandby);
}

static void
suspend_timeout_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	CARD16 val = round(value);

	if (val != state.DPMS.suspend) {
		PTRACE(5);
		state.DPMS.suspend = val;
		DPMSSetTimeouts(dpy,
				state.DPMS.standby,
				state.DPMS.suspend,
				state.DPMS.off);
		reprocess_input();
	}
}

static void
activate_suspend_clicked(GtkButton *button, gpointer user_data)
{
	DPMSForceLevel(dpy, DPMSModeSuspend);
}

static void
off_timeout_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	CARD16 val = round(value);

	if (val != state.DPMS.off) {
		PTRACE(5);
		state.DPMS.off = val;
		DPMSSetTimeouts(dpy,
				state.DPMS.standby,
				state.DPMS.suspend,
				state.DPMS.off);
		reprocess_input();
	}
}

static void
activate_off_clicked(GtkButton *button, gpointer user_data)
{
	PTRACE(5);
	DPMSForceLevel(dpy, DPMSModeOff);
}

static void
keyboard_rate_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);
	if (val != state.XF86Misc.keyboard.rate) {
		PTRACE(5);
		state.XF86Misc.keyboard.rate = val;
		XF86MiscSetKbdSettings(dpy, &state.XF86Misc.keyboard);
	}
	reprocess_input();
}

static void
keyboard_delay_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.XF86Misc.keyboard.delay) {
		PTRACE(5);
		state.XF86Misc.keyboard.delay = val;
		XF86MiscSetKbdSettings(dpy, &state.XF86Misc.keyboard);
	}
	reprocess_input();
}

static void
emulate_3_buttons_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	PTRACE(5);
	if (active) {
		state.XF86Misc.mouse.emulate3buttons = True;
	} else {
		state.XF86Misc.mouse.emulate3buttons = False;
	}
	XF86MiscSetMouseSettings(dpy, &state.XF86Misc.mouse);
	reprocess_input();
}

static void
emulate_3_timeout_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.XF86Misc.mouse.emulate3timeout) {
		PTRACE(5);
		state.XF86Misc.mouse.emulate3timeout = val;
		XF86MiscSetMouseSettings(dpy, &state.XF86Misc.mouse);
	}
	reprocess_input();
}

static void
chord_middle_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	PTRACE(5);
	if (active) {
		state.XF86Misc.mouse.chordmiddle = True;
	} else {
		state.XF86Misc.mouse.chordmiddle = False;
	}
	XF86MiscSetMouseSettings(dpy, &state.XF86Misc.mouse);
	reprocess_input();
}



GtkWindow *
create_window()
{
	GtkWindow *w;
	GtkWidget *h, *n, *v, *l, *f, *u, *s, *q;

	w = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_wmclass(w, "xde-input", "Xde-input");
	gtk_window_set_title(w, "XDE X Input Control");
	gtk_window_set_gravity(w, GDK_GRAVITY_CENTER);
	gtk_window_set_type_hint(w, GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(w), 10);
	gtk_window_set_skip_pager_hint(w, FALSE);
	gtk_window_set_skip_taskbar_hint(w, FALSE);
	gtk_window_set_position(w, GTK_WIN_POS_CENTER_ALWAYS);
	g_signal_connect(G_OBJECT(w), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	h = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(w), h);

	n = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(h), n, FALSE, FALSE, 0);

	if (support.Pointer) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Pointer");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

		f = gtk_frame_new("Acceleration Numerator");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(1.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the acceleration numerator.  The effective\n\
acceleration factor is the numerator divided by\n\
the denominator.  A typical value is 24.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(accel_numerator_value_changed), NULL);
		controls.Pointer.AccelerationNumerator = h;

		f = gtk_frame_new("Acceleration Denominator");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(1.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_widget_set_tooltip_markup(h, "\
Set the acceleration denominator.  The effective\n\
acceleration factor is the numerator divided by\n\
the denominator.  A typical value is 10.");
		gtk_container_add(GTK_CONTAINER(f), h);
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(accel_denominator_value_changed), NULL);
		controls.Pointer.AccelerationDenominator = h;

		f = gtk_frame_new("Threshold");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(1.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_widget_set_tooltip_markup(h, "\
Set the number of pixels moved before acceleration\n\
begins.  A typical and usable value is 10 pixels.");
		gtk_container_add(GTK_CONTAINER(f), h);
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(threshold_value_changed), NULL);
		controls.Pointer.Threshold = h;
	}

	if (support.Keyboard) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Keyboard");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Global Auto Repeat");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When enabled, all keyboard keys will auto-repeat;\n\
otherwise, only per-key autorepeat settings are\n\
observed.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(global_autorepeat_toggled), NULL);
		controls.Keyboard.GlobalAutoRepeat = u;

		f = gtk_frame_new("Key Click Percent (%)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_percent), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the key click volume as a percentage of\n\
maximum volume: from 0% to 100%.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(keyclick_percent_value_changed), NULL);
		controls.Keyboard.KeyClickPercent = h;

		f = gtk_frame_new("Bell Percent (%)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_percent), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the bell volume as a percentage of\n\
maximum volume: from 0% to 100%.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(bell_percent_value_changed), NULL);
		controls.Keyboard.BellPercent = h;

		f = gtk_frame_new("Bell Pitch (Hz)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(60.0, 2000.0, 20.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_hertz), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the bell pitch in Hertz.  Usable values\n\
are from 200 to 800 Hz.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(bell_pitch_value_changed), NULL);
		controls.Keyboard.BellPitch = h;

		f = gtk_frame_new("Bell Duration (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(10.0, 500.0, 10.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the bell duration in milliseconds.  Usable\n\
values are 100 to 300 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(bell_duration_value_changed), NULL);
		controls.Keyboard.BellDuration = h;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Ring Bell");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "Press to ring bell.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(ring_bell_clicked), NULL);
	}

	if (support.XKeyboard) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("XKeyboard");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);
		s = gtk_notebook_new();
		gtk_box_pack_start(GTK_BOX(v), s, TRUE, TRUE, 0);
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Repeat Keys");
		gtk_notebook_append_page(GTK_NOTEBOOK(s), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Repeat Keys Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When enabled, all keyboard keys will auto-repeat;\n\
otherwise, only per-key autorepeat settings are\n\
observed.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(repeat_keys_toggled), NULL);
		controls.XKeyboard.RepeatKeysEnabled = u;

		f = gtk_frame_new("Repeat Delay (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the delay after key press before auto-repeat\n\
begins in milliseconds.  Usable values are from\n\
250 to 500 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(repeat_delay_value_changed), NULL);
		controls.XKeyboard.RepeatDelay = h;

		f = gtk_frame_new("Repeat Interval (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(10.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the interval between repeats after auto-repeat\n\
has begun.  Usable values are from 10 to 100\n\
milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(repeat_interval_value_changed), NULL);
		controls.XKeyboard.RepeatInterval = h;

		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Slow Keys");
		gtk_notebook_append_page(GTK_NOTEBOOK(s), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Slow Keys Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, slow keys are enabled;\n\
otherwise slow keys are disabled.\n\
When enabled, keys pressed and released\n\
before the slow keys delay expires will\n\
be ignored.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(slow_keys_toggled), NULL);
		controls.XKeyboard.SlowKeysEnabled = u;

		f = gtk_frame_new("Slow Keys Delay (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the duration in milliseconds for which a\n\
key must remain pressed to be considered\n\
a key press.  Usable values are 100 to 300\n\
milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(slow_keys_delay_value_changed), NULL);
		controls.XKeyboard.SlowKeysDelay = h;

		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Bounce Keys");
		gtk_notebook_append_page(GTK_NOTEBOOK(s), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Bounce Keys Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, keys that are repeatedly\n\
pressed within the debounce delay will be\n\
ignored; otherwise, keys are not debounced.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(bounce_keys_toggled), NULL);
		controls.XKeyboard.BounceKeysEnabled = u;

		f = gtk_frame_new("Debounce Delay (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Ignores repeated key presses and releases\n\
that occur within the debounce delay after\n\
the key was released.  Usable values are\n\
300 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(debounce_delay_value_changed), NULL);
		controls.XKeyboard.DebounceDelay = h;

		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Sticky Keys");
		gtk_notebook_append_page(GTK_NOTEBOOK(s), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Sticky Keys Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, sticky keys are enabled;\n\
otherwise, sticky keys are disabled.\n\
When enabled, modifier keys will stick\n\
when pressed and released until a non-\n\
modifier key is pressed.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(sticky_keys_toggled), NULL);
		controls.XKeyboard.StickyKeysEnabled = u;

		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Mouse Keys");
		gtk_notebook_append_page(GTK_NOTEBOOK(s), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Mouse Keys Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, mouse keys are enabled;\n\
otherwise they are disabled.  Mouse\n\
keys permit operating the pointer using\n\
only the keyboard.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(mouse_keys_toggled), NULL);
		controls.XKeyboard.MouseKeysEnabled = u;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_hbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		l = gtk_label_new("Default Mouse Button");
		gtk_box_pack_start(GTK_BOX(q), l, FALSE, FALSE, 5);
		u = gtk_combo_box_new_text();
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "1");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "2");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "3");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "4");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "5");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "6");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "7");
		gtk_combo_box_append_text(GTK_COMBO_BOX(u), "8");
		gtk_box_pack_end(GTK_BOX(q), u, TRUE, TRUE, 0);
		gtk_widget_set_tooltip_markup(u, "Select the default mouse button.");
		g_signal_connect(G_OBJECT(u), "changed", G_CALLBACK(default_mouse_button_changed), NULL);
		controls.XKeyboard.MouseKeysDfltBtn = u;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Mouse Keys Accel Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, mouse key acceleration\n\
is enabled; otherwise it is disabled.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(mouse_keys_accel_toggled), NULL);
		controls.XKeyboard.MouseKeysAccelEnabled = u;

		f = gtk_frame_new("Mouse Keys Delay (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the amount of time in milliseconds\n\
between the initial key press and the first\n\
repeated motion event.  A usable value is\n\
about 160 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(mouse_keys_delay_value_changed), NULL);
		controls.XKeyboard.MouseKeysDelay = h;

		f = gtk_frame_new("Mouse Keys Interval (milliseconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the amount of time in milliseconds\n\
between repeated mouse key events.  Usable\n\
values are from 10 to 40 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(mouse_keys_interval_value_changed), NULL);
		controls.XKeyboard.MouseKeysInterval = h;

		f = gtk_frame_new("Mouse Keys Time to Maximum (count)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(1.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Sets the number of key presses after which the\n\
mouse key acceleration will be at the maximum.\n\
Usable values are from 10 to 40.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(mouse_keys_time_to_max_value_changed), NULL);
		controls.XKeyboard.MouseKeysTimeToMax = h;

		f = gtk_frame_new("Mouse Keys Maximum Speed (multiplier)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the multiplier for mouse events at\n\
the maximum speed.  Usable values are\n\
from 10 to 40.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(mouse_keys_max_speed_value_changed), NULL);
		controls.XKeyboard.MouseKeysMaxSpeed = h;

		f = gtk_frame_new("Mouse Keys Curve (factor)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(-1000.0, 1000.0, 50.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Sets the curve ramp up to maximum acceleration.\n\
Negative values ramp sharply to the maximum;\n\
positive values ramp slowly.  Usable values are\n\
from -1000 to 1000.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(mouse_keys_curve_value_changed), NULL);
		controls.XKeyboard.MouseKeysCurve = h;
	}

	if (support.ScreenSaver) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Screen Saver");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

		f = gtk_frame_new("Timeout (seconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		h = gtk_hscale_new_with_range(0.0, 3600.0, 30.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_seconds), NULL);
		gtk_box_pack_start(GTK_BOX(q), h, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Activate Screen Saver");
		gtk_box_pack_start(GTK_BOX(q), u, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_markup(u, "Click to activate the screen saver.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(activate_screensaver_clicked), NULL);
		gtk_widget_set_tooltip_markup(h, "\
Specify the time in seconds that pointer and keyboard\n\
must be idle before the screensaver is activated.\n\
Typical values are 600 seconds (10 minutes).  Set\n\
to zero to disable the screensaver.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(screensaver_timeout_value_changed), NULL);
		controls.ScreenSaver.Timeout = h;

		f = gtk_frame_new("Interval (seconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		h = gtk_hscale_new_with_range(0.0, 3600.0, 30.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_seconds), NULL);
		gtk_box_pack_start(GTK_BOX(q), h, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Rotate Screen Saver");
		gtk_box_pack_start(GTK_BOX(q), u, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_markup(u, "Click to rotate the screen saver.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(rotate_screensaver_clicked), NULL);
		gtk_widget_set_tooltip_markup(h, "\
Specify the time in seconds after which the screen\n\
saver will change (if other than blanking).  A\n\
typical value is 600 seconds (10 minutes).");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(screensaver_interval_value_changed), NULL);
		controls.ScreenSaver.Interval = h;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Prefer Blanking");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, blank the scfreen instead of using\n\
a screen saver; otherwise, use a screen saver if\n\
enabled.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(prefer_blanking_toggled), NULL);
		controls.ScreenSaver.Preferblanking = u;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		gtk_check_button_new_with_label("Allow Exposures");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When set, use a screensaver even when the server\n\
is not capable of performing screen saving without\n\
sending exposure events to existing clients.  Not\n\
normally needed nowadays.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(allow_exposures_toggled), NULL);
		controls.ScreenSaver.Allowexposures = u;
	}

	if (support.DPMS) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("DPMS");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("DPMS Enabled");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When checked, enable DPMS functions;\n\
otherwise disabled.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(dpms_toggled), NULL);
		controls.DPMS.State = u;

		f = gtk_frame_new("Standby Timeout (seconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		h = gtk_hscale_new_with_range(0.0, 3600.0, 30.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_seconds), NULL);
		gtk_box_pack_start(GTK_BOX(q), h, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Activate Standby");
		gtk_box_pack_start(GTK_BOX(q), u, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_markup(u, "Click to activate standby mode.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(activate_standby_clicked), NULL);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the period of inactivity after which the\n\
monitor will enter standby mode.  A typical value\n\
is 600 seconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(standby_timeout_value_changed), NULL);
		controls.DPMS.StandbyTimeout = h;

		f = gtk_frame_new("Suspend Timeout (seconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		h = gtk_hscale_new_with_range(0.0, 3600.0, 30.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_seconds), NULL);
		gtk_box_pack_start(GTK_BOX(q), h, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Activate Suspend");
		gtk_box_pack_start(GTK_BOX(q), u, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_markup(u, "Click to activate suspend mode.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(activate_suspend_clicked), NULL);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the period of inactivity after which the\n\
monitor will enter suspend mode.  A typical value\n\
is 1200 seconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(suspend_timeout_value_changed), NULL);
		controls.DPMS.SuspendTimeout = h;

		f = gtk_frame_new("Off Timeout (seconds)");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);
		h = gtk_hscale_new_with_range(0.0, 3600.0, 30.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_seconds), NULL);
		gtk_box_pack_start(GTK_BOX(q), h, FALSE, FALSE, 0);
		u = gtk_button_new_with_label("Activate Off");
		gtk_box_pack_start(GTK_BOX(q), u, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_markup(u, "Click to activate off mode.");
		g_signal_connect(G_OBJECT(u), "clicked", G_CALLBACK(activate_off_clicked), NULL);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the period of inactivity after which the\n\
monitor will be turned off.  A typical value is\n\
1800 seconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(off_timeout_value_changed), NULL);
		controls.DPMS.OffTimeout = h;
	}
	if (support.XF86Misc) {
		v = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(v), 5);
		l = gtk_label_new("Miscellaneous");
		gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

		f = gtk_frame_new("Keyboard");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);

		f = gtk_frame_new("Rate");
		gtk_box_pack_start(GTK_BOX(q), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 1000.0, 1.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_hertz), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Set the number of key repeats per second.  Usable\n\
values are between 0 and 300.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(keyboard_rate_value_changed), NULL);
		controls.XF86Misc.KeyboardRate = h;

		f = gtk_frame_new("Delay");
		gtk_box_pack_start(GTK_BOX(q), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 2000.0, 10.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the delay in milliseconds after a key is\n\
pressed before the key starts repeating.  Usable\n\
values are between 10 and 500 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(keyboard_delay_value_changed), NULL);
		controls.XF86Misc.KeyboardDelay = h;

		f = gtk_frame_new("Mouse");
		gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
		q = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(f), q);

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(q), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Emulate 3 Buttons");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When enabled, emulate a 3-button mouse by clicking buttons\n\
1 and 3 at about the same time; otherwise, no emulation is\n\
performed.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(emulate_3_buttons_toggled), NULL);
		controls.XF86Misc.MouseEmulate3Buttons = u;

		f = gtk_frame_new("Emulate 3 Buttons Timeout");
		gtk_box_pack_start(GTK_BOX(q), f, FALSE, FALSE, 0);
		h = gtk_hscale_new_with_range(0.0, 2000.0, 10.0);
		gtk_scale_set_draw_value(GTK_SCALE(h), TRUE);
		g_signal_connect(G_OBJECT(h), "format-value", G_CALLBACK(format_value_milliseconds), NULL);
		gtk_container_add(GTK_CONTAINER(f), h);
		gtk_widget_set_tooltip_markup(h, "\
Specifies the delay after button 1 or 3 is pressed after\n\
which the button presses will be considered independent\n\
and not a 3-button emulation.  Usable values are from\n\
10 to 200 milliseconds.");
		g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(emulate_3_timeout_value_changed), NULL);
		controls.XF86Misc.MouseEmulate3Timeout = h;

		f = gtk_frame_new(NULL);
		gtk_box_pack_start(GTK_BOX(q), f, FALSE, FALSE, 0);
		u = gtk_check_button_new_with_label("Chord Middle");
		gtk_container_add(GTK_CONTAINER(f), u);
		gtk_widget_set_tooltip_markup(u, "\
When enabled, treat buttons 1 and 3 pressed at the same\n\
time as button 2; otherwise, buttons 1 and 3 are\n\
considered independent.");
		g_signal_connect(G_OBJECT(u), "toggled", G_CALLBACK(chord_middle_toggled), NULL);
		controls.XF86Misc.MouseChordMiddle = u;
	}

	return (w);
}

static void
pop_editor(XdeScreen *xscr)
{
	if (editor || (editor = create_window())) {
		gtk_window_set_screen(editor, xscr->scrn);
		edit_set_values();
		gtk_widget_show_all(GTK_WIDGET(editor));
	}
}

static void
update_theme(XdeScreen *xscr, Atom prop)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY(xscr->disp);
	Window root = RootWindow(dpy, xscr->index);
	XTextProperty xtp = { NULL, };
	char **list = NULL;
	int strings = 0;
	Bool changed = False;
	GtkSettings *set;

	PTRACE(5);
	gtk_rc_reparse_all();
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
				if (!xscr->theme || strcmp(xscr->theme, list[0])) {
					free(xscr->theme);
					xscr->theme = strdup(list[0]);
					changed = True;
				}
			}
			if (list)
				XFreeStringList(list);
		} else
			EPRINTF("could not get text list for property\n");
		if (xtp.value)
			XFree(xtp.value);
	} else
		DPRINTF(1, "could not get _XDE_THEME_NAME for root 0x%lx\n", root);
	if ((set = gtk_settings_get_for_screen(xscr->scrn))) {
		GValue theme_v = G_VALUE_INIT;
		const char *theme;

		g_value_init(&theme_v, G_TYPE_STRING);
		g_object_get_property(G_OBJECT(set), "gtk-theme-name", &theme_v);
		theme = g_value_get_string(&theme_v);
		if (theme && (!xscr->theme || strcmp(xscr->theme, theme))) {
			free(xscr->theme);
			xscr->theme = strdup(theme);
			changed = True;
		}
		g_value_unset(&theme_v);
	}
	if (changed) {
		DPRINTF(1, "New theme is %s\n", xscr->theme);
		/* FIXME: do something more about it. */
	}
}

static GdkFilterReturn
event_handler_SelectionClear(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	PTRACE(5);
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
	PTRACE(5);
	if (xscr && xev->xselectionclear.window == xscr->selwin) {
		XDestroyWindow(dpy, xscr->selwin);
		EPRINTF("selection cleared, exiting\n");
		exit(EXIT_SUCCESS);
	}
	PTRACE(5);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
selwin_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = data;
	Display *dpy = GDK_DISPLAY_XDISPLAY(xscr->disp);

	PTRACE(5);
	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	PTRACE(5);
	switch (xev->type) {
	case SelectionClear:
		return event_handler_SelectionClear(dpy, xev, xscr);
	}
	PTRACE(5);
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
event_handler_PropertyNotify(Display *dpy, XEvent *xev, XdeScreen *xscr)
{
	PTRACE(5);
	if (options.debug > 2) {
		fprintf(stderr, "==> PropertyNotify:\n");
		fprintf(stderr, "    --> window = 0x%08lx\n", xev->xproperty.window);
		fprintf(stderr, "    --> atom = %s\n", XGetAtomName(dpy, xev->xproperty.atom));
		fprintf(stderr, "    --> time = %ld\n", xev->xproperty.time);
		fprintf(stderr, "    --> state = %s\n",
			(xev->xproperty.state == PropertyNewValue) ? "NewValue" : "Delete");
		fprintf(stderr, "<== PropertyNotify:\n");
	}
	PTRACE(5);
	if (xev->xproperty.atom == _XA_XDE_THEME_NAME
	    && xev->xproperty.state == PropertyNewValue) {
		PTRACE(5);
		update_theme(xscr, xev->xproperty.atom);
		return GDK_FILTER_REMOVE;	/* event handled */
	}
	PTRACE(5);
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
root_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	XdeScreen *xscr = (typeof(xscr)) data;
	Display *dpy = GDK_DISPLAY_XDISPLAY(xscr->disp);

	PTRACE(5);
	if (!xscr) {
		EPRINTF("xscr is NULL\n");
		exit(EXIT_FAILURE);
	}
	PTRACE(5);
	switch (xev->type) {
	case PropertyNotify:
		PTRACE(5);
		return event_handler_PropertyNotify(dpy, xev, xscr);
	}
	PTRACE(5);
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
event_handler_ClientMessage(Display *dpy, XEvent *xev)
{
	XdeScreen *xscr = NULL;
	int s, nscr = ScreenCount(dpy);

	PTRACE(5);
	for (s = 0; s < nscr; s++)
		if (xev->xclient.window == RootWindow(dpy, s)) {
			xscr = screens + s;
			break;
		}

	PTRACE(5);
	if (options.debug > 1) {
		fprintf(stderr, "==> ClientMessage: %p\n", xscr);
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
		fprintf(stderr, "<== ClientMessage: %p\n", xscr);
	}
	PTRACE(5);
	if (xscr) {
		PTRACE(5);
		if (xev->xclient.message_type == _XA_GTK_READ_RCFILES) {
			PTRACE(5);
			update_theme(xscr, xev->xclient.message_type);
			PTRACE(5);
			return GDK_FILTER_REMOVE;	/* event handled */
		} else
		if (xev->xclient.message_type == _XA_XDE_INPUT_EDIT) {
			PTRACE(5);
			pop_editor(xscr);
			PTRACE(5);
			return GDK_FILTER_REMOVE;
		}
		PTRACE(5);
	}
	PTRACE(5);
	return GDK_FILTER_CONTINUE;	/* event not handled */
}

static GdkFilterReturn
client_handler(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev = (typeof(xev)) xevent;
	Display *dpy = (typeof(dpy)) data;

	PTRACE(5);
	switch (xev->type) {
	case ClientMessage:
		PTRACE(5);
		return event_handler_ClientMessage(dpy, xev);
	}
	PTRACE(5);
	EPRINTF("wrong message type for handler %d\n", xev->type);
	return GDK_FILTER_CONTINUE;	/* event not handled, continue processing */
}

static Window
get_selection(Bool replace, Window selwin)
{
	char selection[64] = { 0, };
	GdkDisplay *disp;
	Display *dpy;
	int s, nscr;
	Window owner;
	Atom atom;
	Window gotone = None;

	PTRACE(5);
	disp = gdk_display_get_default();
	nscr = gdk_display_get_n_screens(disp);

	dpy = GDK_DISPLAY_XDISPLAY(disp);

	for (s = 0; s < nscr; s++) {
		snprintf(selection, sizeof(selection), XA_SELECTION_NAME, s);
		atom = XInternAtom(dpy, selection, False);
		if (!(owner = XGetSelectionOwner(dpy, atom)))
			DPRINTF(1, "No owner for %s\n", selection);
		if ((owner && replace) || (!owner && selwin)) {
			DPRINTF(1, "Setting owner of %s to 0x%08lx from 0x%08lx\n", selection,
				selwin, owner);
			XSetSelectionOwner(dpy, atom, selwin, CurrentTime);
			XSync(dpy, False);
		}
		if (!gotone && owner)
			gotone = owner;
	}
	if (replace) {
		if (gotone) {
			if (selwin)
				DPRINTF(1, "%s: replacing running instance\n", NAME);
			else
				DPRINTF(1, "%s: quitting running instance\n", NAME);
		} else {
			if (selwin)
				DPRINTF(1, "%s: no running instance to replace\n", NAME);
			else
				DPRINTF(1, "%s: no running instance to quit\n", NAME);
		}
		if (selwin) {
			XEvent ev = { 0, };
			Atom manager = XInternAtom(dpy, "MANAGER", False);
			GdkScreen *scrn;
			Window root;

			for (s = 0; s < nscr; s++) {
				scrn = gdk_display_get_screen(disp, s);
				root = GDK_WINDOW_XID(gdk_screen_get_root_window(scrn));
				snprintf(selection, sizeof(selection), XA_SELECTION_NAME, s);
				atom = XInternAtom(dpy, selection, False);

				ev.xclient.type = ClientMessage;
				ev.xclient.serial = 0;
				ev.xclient.send_event = False;
				ev.xclient.display = dpy;
				ev.xclient.window = root;
				ev.xclient.message_type = manager;
				ev.xclient.format = 32;
				ev.xclient.data.l[0] = CurrentTime;	/* FIXME:
									   mimestamp */
				ev.xclient.data.l[1] = atom;
				ev.xclient.data.l[2] = selwin;
				ev.xclient.data.l[3] = 0;
				ev.xclient.data.l[4] = 0;

				XSendEvent(dpy, root, False, StructureNotifyMask, &ev);
				XFlush(dpy);
			}
		}
	} else if (gotone)
		DPRINTF(1, "%s: not replacing running instance\n", NAME);
	return (gotone);
}

int
handler(Display *display, XErrorEvent *xev)
{
	if (options.debug) {
		char msg[80], req[80], num[80], def[80];

		snprintf(num, sizeof(num), "%d", xev->request_code);
		snprintf(def, sizeof(def), "[request_code=%d]", xev->request_code);
		XGetErrorDatabaseText(dpy, "xdg-launch", num, def, req, sizeof(req));
		if (XGetErrorText(dpy, xev->error_code, msg, sizeof(msg)) != Success)
			msg[0] = '\0';
		fprintf(stderr, "X error %s(0x%08lx): %s\n", req, xev->resourceid, msg);
	}
	return (0);
}

int
iohandler(Display *display)
{
	void *buffer[1024];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 1023)) && (strings = backtrace_symbols(buffer, nptr)))
		for (i = 0; i < nptr; i++)
			fprintf(stderr, "backtrace> %s\n", strings[i]);
	exit(EXIT_FAILURE);
}

int (*oldhandler) (Display *, XErrorEvent *) = NULL;
int (*oldiohandler) (Display *) = NULL;

static void
do_run(int argc, char *argv[], Bool replace)
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn), *sel;
	char selection[64] = { 0, };
	Window selwin, owner;
	XdeScreen *xscr;
	int s, nscr;

	PTRACE(5);
	selwin = XCreateSimpleWindow(dpy, GDK_WINDOW_XID(root), 0, 0, 1, 1, 0, 0, 0);

	if ((owner = get_selection(replace, selwin))) {
		if (!replace) {
			XDestroyWindow(dpy, selwin);
			EPRINTF("%s: instance already running\n", NAME);
			exit(EXIT_FAILURE);
		}
	}
	XSelectInput(dpy, selwin,
		     StructureNotifyMask | SubstructureNotifyMask | PropertyChangeMask);

	oldhandler = XSetErrorHandler(handler);
	oldiohandler = XSetIOErrorHandler(iohandler);

	nscr = gdk_display_get_n_screens(disp);
	screens = calloc(nscr, sizeof(*screens));

	sel = gdk_x11_window_foreign_new_for_display(disp, selwin);
	gdk_window_add_filter(sel, selwin_handler, screens);

	for (s = 0, xscr = screens; s < nscr; s++, xscr++) {
		snprintf(selection, sizeof(selection), XA_SELECTION_NAME, s);
		xscr->index = s;
		xscr->atom = XInternAtom(dpy, selection, False);
		xscr->disp = disp;
		xscr->scrn = gdk_display_get_screen(disp, s);
		xscr->root = gdk_screen_get_root_window(xscr->scrn);
		xscr->selwin = selwin;
		gdk_window_add_filter(xscr->root, root_handler, xscr);
		update_theme(xscr, None);
	}
	PTRACE(5);
	startitup();
	PTRACE(5);
	get_input();
	PTRACE(5);
	set_input(options.filename);
	PTRACE(5);
	gtk_main();
	PTRACE(5);
}

/** @brief Ask a running instance to quit.
  *
  * This is performed by checking for an owner of the selection and clearing the
  * selection if it exists.
  */
static void
do_quit(int argc, char *argv[])
{
	PTRACE(5);
	get_selection(True, None);
}

/** @brief Ask running instance to launch an editor (or run a new instance and
  * launch the editor)
  */
static void
do_editor(int argc, char *argv[])
{
	GdkDisplay *disp = gdk_display_get_default();
	Display *dpy = GDK_DISPLAY_XDISPLAY(disp);
	GdkScreen *scrn = gdk_display_get_default_screen(disp);
	GdkWindow *root = gdk_screen_get_root_window(scrn), *sel;
	char selection[64] = { 0, };
	Window selwin, owner;
	XdeScreen *xscr;
	int s, nscr;

	PTRACE(5);
	selwin = XCreateSimpleWindow(dpy, GDK_WINDOW_XID(root), 0, 0, 1, 1, 0, 0, 0);

	if ((owner = get_selection(False, selwin))) {
		XEvent ev = { 0, };

		/* existing instance running, ask it to launch editor */
		XDestroyWindow(dpy, selwin);
		DPRINTF(1, "%s: instance running: asking it to launch editor\n", NAME);
		scrn = gdk_display_get_screen(disp, options.screen);
		root = gdk_screen_get_root_window(scrn);

		ev.xclient.type = ClientMessage;
		ev.xclient.serial = 0;
		ev.xclient.send_event = False;
		ev.xclient.display = dpy;
		ev.xclient.window = GDK_WINDOW_XID(root);
		ev.xclient.message_type = _XA_XDE_INPUT_EDIT;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = CurrentTime;
		ev.xclient.data.l[1] = 0;
		ev.xclient.data.l[2] = 0;
		ev.xclient.data.l[3] = 0;
		ev.xclient.data.l[4] = 0;
		XSendEvent(dpy, GDK_WINDOW_XID(root), False, StructureNotifyMask, &ev);
		XSync(dpy, False);
		exit(EXIT_SUCCESS);
	}
	XSelectInput(dpy, selwin,
		     StructureNotifyMask | SubstructureNotifyMask | PropertyChangeMask);

	oldhandler = XSetErrorHandler(handler);
	oldiohandler = XSetIOErrorHandler(iohandler);

	nscr = gdk_display_get_n_screens(disp);
	screens = calloc(nscr, sizeof(*screens));

	sel = gdk_x11_window_foreign_new_for_display(disp, selwin);
	gdk_window_add_filter(sel, selwin_handler, screens);

	for (s = 0, xscr = screens; s < nscr; s++, xscr++) {
		snprintf(selection, sizeof(selection), XA_SELECTION_NAME, s);
		xscr->index = s;
		xscr->atom = XInternAtom(dpy, selection, False);
		xscr->disp = disp;
		xscr->scrn = gdk_display_get_screen(disp, s);
		xscr->root = gdk_screen_get_root_window(xscr->scrn);
		xscr->selwin = selwin;
		gdk_window_add_filter(xscr->root, root_handler, xscr);
		update_theme(xscr, None);
	}
	PTRACE(5);
	startitup();
	PTRACE(5);
	get_input();
	PTRACE(5);
	set_input(options.filename);
	PTRACE(5);
	xscr = screens + options.screen;
	pop_editor(xscr);
	PTRACE(5);
	gtk_main();
	PTRACE(5);
}

static void
clientSetProperties(SmcConn smcConn, SmPointer data)
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

	/* CloneCommand: This is like the RestartCommand except it restarts a copy of the 
	   application.  The only difference is that the application doesn't supply its
	   client id at register time.  On POSIX systems the type should be a
	   LISTofARRAY8. */
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
	/* CurrentDirectory: On POSIX-based systems, specifies the value of the current
	   directory that needs to be set up prior to starting the program and should be
	   of type ARRAY8. */
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
	/* DiscardCommand: The discard command contains a command that when delivered to
	   the host that the client is running on (determined from the connection), will
	   cause it to discard any information about the current state.  If this command
	   is not specified, the SM will assume that all of the client's state is encoded
	   in the RestartCommand [and properties].  On POSIX systems the type should be
	   LISTofARRAY8. */
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

	/* Environment: On POSIX based systems, this will be of type LISTofARRAY8 where
	   the ARRAY8s alternate between environment variable name and environment
	   variable value. */
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

	/* ProcessID: This specifies an OS-specific identifier for the process. On POSIX
	   systems this should be of type ARRAY8 and contain the return of getpid()
	   turned into a Latin-1 (decimal) string. */
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

	/* Program: The name of the program that is running.  On POSIX systems, this
	   should eb the first parameter passed to execve(3) and should be of type
	   ARRAY8. */
	prop[j].name = SmProgram;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[j];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	propval[j].value = argv[0];
	propval[j].length = strlen(argv[0]);
	j++;

	/* RestartCommand: The restart command contains a command that when delivered to
	   the host that the client is running on (determined from the connection), will
	   cause the client to restart in its current state.  On POSIX-based systems this 
	   if of type LISTofARRAY8 and each of the elements in the array represents an
	   element in the argv[] array.  This restart command should ensure that the
	   client restarts with the specified client-ID.  */
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

	/* ResignCommand: A client that sets the RestartStyleHint to RestartAnyway uses
	   this property to specify a command that undoes the effect of the client and
	   removes any saved state. */
	prop[j].name = SmResignCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = calloc(2, sizeof(*prop[j].vals));
	prop[j].num_vals = 2;
	props[j] = &prop[j];
	prop[j].vals[0].value = "/usr/bin/xde-pager";
	prop[j].vals[0].length = strlen("/usr/bin/xde-pager");
	prop[j].vals[1].value = "-quit";
	prop[j].vals[1].length = strlen("-quit");
	j++;

	/* RestartStyleHint: If the RestartStyleHint property is present, it will contain 
	   the style of restarting the client prefers.  If this flag is not specified,
	   RestartIfRunning is assumed.  The possible values are as follows:
	   RestartIfRunning(0), RestartAnyway(1), RestartImmediately(2), RestartNever(3). 
	   The RestartIfRunning(0) style is used in the usual case.  The client should be 
	   restarted in the next session if it is connected to the session manager at the
	   end of the current session. The RestartAnyway(1) style is used to tell the SM
	   that the application should be restarted in the next session even if it exits
	   before the current session is terminated. It should be noted that this is only
	   a hint and the SM will follow the policies specified by its users in
	   determining what applications to restart.  A client that uses RestartAnyway(1)
	   should also set the ResignCommand and ShutdownCommand properties to the
	   commands that undo the state of the client after it exits.  The
	   RestartImmediately(2) style is like RestartAnyway(1) but in addition, the
	   client is meant to run continuously.  If the client exits, the SM should try to 
	   restart it in the current session.  The RestartNever(3) style specifies that
	   the client does not wish to be restarted in the next session. */
	prop[j].name = SmRestartStyleHint;
	prop[j].type = SmARRAY8;
	prop[j].vals = &propval[0];
	prop[j].num_vals = 1;
	props[j] = &prop[j];
	hint = SmRestartImmediately;	/* <--- */
	propval[j].value = &hint;
	propval[j].length = 1;
	j++;

	/* ShutdownCommand: This command is executed at shutdown time to clean up after a 
	   client that is no longer running but retained its state by setting
	   RestartStyleHint to RestartAnyway(1).  The command must not remove any saved
	   state as the client is still part of the session. */
	prop[j].name = SmShutdownCommand;
	prop[j].type = SmLISTofARRAY8;
	prop[j].vals = calloc(2, sizeof(*prop[j].vals));
	prop[j].num_vals = 2;
	props[j] = &prop[j];
	prop[j].vals[0].value = "/usr/bin/xde-pager";
	prop[j].vals[0].length = strlen("/usr/bin/xde-pager");
	prop[j].vals[1].value = "-quit";
	prop[j].vals[1].length = strlen("-quit");
	j++;

	/* UserID: Specifies the user's ID.  On POSIX-based systems this will contain the 
	   user's name (the pw_name field of struct passwd).  */
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
static Bool shutting_down;

static void
clientSaveYourselfPhase2CB(SmcConn smcConn, SmPointer data)
{
	clientSetProperties(smcConn, data);
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
clientSaveYourselfCB(SmcConn smcConn, SmPointer data, int saveType, Bool shutdown,
		     int interactStyle, Bool fast)
{
	if (!(shutting_down = shutdown)) {
		if (!SmcRequestSaveYourselfPhase2(smcConn, clientSaveYourselfPhase2CB, data))
			SmcSaveYourselfDone(smcConn, False);
		return;
	}
	clientSetProperties(smcConn, data);
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
clientDieCB(SmcConn smcConn, SmPointer data)
{
	SmcCloseConnection(smcConn, 0, NULL);
	shutting_down = False;
	gtk_main_quit();
}

static void
clientSaveCompleteCB(SmcConn smcConn, SmPointer data)
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
clientShutdownCancelledCB(SmcConn smcConn, SmPointer data)
{
	shutting_down = False;
	gtk_main_quit();
}

/* *INDENT-OFF* */
static unsigned long clientCBMask =
	SmcSaveYourselfProcMask |
	SmcDieProcMask |
	SmcSaveCompleteProcMask |
	SmcShutdownCancelledProcMask;

static SmcCallbacks clientCBs = {
	.save_yourself = {
		.callback = &clientSaveYourselfCB,
		.client_data = NULL,
	},
	.die = {
		.callback = &clientDieCB,
		.client_data = NULL,
	},
	.save_complete = {
		.callback = &clientSaveCompleteCB,
		.client_data = NULL,
	},
	.shutdown_cancelled = {
		.callback = &clientShutdownCancelledCB,
		.client_data = NULL,
	},
};
/* *INDENT-ON* */

static gboolean
on_ifd_watch(GIOChannel *chan, GIOCondition cond, pointer data)
{
	SmcConn smcConn = data;
	IceConn iceConn = SmcGetIceConnection(smcConn);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		EPRINTF("poll failed: %s %s %s\n",
			(cond & G_IO_NVAL) ? "NVAL" : "",
			(cond & G_IO_HUP) ? "HUP" : "", (cond & G_IO_ERR) ? "ERR" : "");
		return G_SOURCE_REMOVE;	/* remove event source */
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
	SmcConn smcConn;
	IceConn iceConn;

	if (!(env = getenv("SESSION_MANAGER"))) {
		if (options.clientId)
			EPRINTF("clientId provided but no SESSION_MANAGER\n");
		return;
	}
	smcConn = SmcOpenConnection(env, NULL, SmProtoMajor, SmProtoMinor,
				    clientCBMask, &clientCBs, options.clientId,
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

static void
startup(int argc, char *argv[])
{
	GdkAtom atom;
	GdkEventMask mask;
	GdkDisplay *disp;
	GdkScreen *scrn;
	GdkWindow *root;
	char *file;
	int nscr;

	file = g_strdup_printf("%s/.gtkrc-2.0.xde", g_get_home_dir());
	gtk_rc_add_default_file(file);
	g_free(file);

	init_smclient();

	gtk_init(&argc, &argv);

	disp = gdk_display_get_default();
	nscr = gdk_display_get_n_screens(disp);

	if (options.screen >= 0 && options.screen >= nscr) {
		EPRINTF("bad screen specified: %d\n", options.screen);
		exit(EXIT_FAILURE);
	}

	dpy = GDK_DISPLAY_XDISPLAY(disp);

	atom = gdk_atom_intern_static_string("_XDE_THEME_NAME");
	_XA_XDE_THEME_NAME = gdk_x11_atom_to_xatom_for_display(disp, atom);

	atom = gdk_atom_intern_static_string("_GTK_READ_RCFILES");
	_XA_GTK_READ_RCFILES = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);

	atom = gdk_atom_intern_static_string("_XDE_INPUT_EDIT");
	_XA_XDE_INPUT_EDIT = gdk_x11_atom_to_xatom_for_display(disp, atom);
	gdk_display_add_client_message_filter(disp, atom, client_handler, dpy);

	scrn = gdk_display_get_default_screen(disp);
	root = gdk_screen_get_root_window(scrn);
	mask = gdk_window_get_events(root);
	mask |= GDK_PROPERTY_CHANGE_MASK;
	gdk_window_set_events(root, mask);
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
Copyright (c) 2008-2016  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016  Monavacon Limited.\n\
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
    %1$s [options]\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Command options:\n\
    -e, --editor\n\
        launch the settings editor\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -f, --filename FILENAME\n\
        use an alternate configuration file\n\
    -n, --dry-run\n\
        do not change anything, just report actions\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL [default: 0]\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL [default: 1]\n\
        this option may be repeated.\n\
", argv[0]);
}

void
put_nc_resource(XrmDatabase xrdb, const char *prefix, const char *resource, const char *value)
{
	static char specifier[64];

	snprintf(specifier, sizeof(specifier), "%s.%s", prefix, resource);
	XrmPutStringResource(&xrdb, specifier, value);
}

void
put_resource(XrmDatabase xrdb, const char *resource, const char *value)
{
	put_nc_resource(xrdb, RESCLAS, resource, value);
}

char *
putXrmInt(int integer)
{
	return g_strdup_printf("%d", integer);
}

char *
putXrmUint(unsigned int integer)
{
	return g_strdup_printf("%u", integer);
}

char *
putXrmBlanking(unsigned int integer)
{
	switch (integer) {
	case DontPreferBlanking:
		return g_strdup("DontPreferBlanking");
	case PreferBlanking:
		return g_strdup("PreferBlanking");
	case DefaultBlanking:
		return g_strdup("DefaultBlanking");
	default:
		return g_strdup_printf("%u", integer);
	}
}

char *
putXrmExposures(unsigned int integer)
{
	switch (integer) {
	case DontAllowExposures:
		return g_strdup("DontAllowExposures");
	case AllowExposures:
		return g_strdup("AllowExposures");
	case DefaultExposures:
		return g_strdup("DefaultExposures");
	default:
		return g_strdup_printf("%u", integer);
	}
}

char *
putXrmButton(unsigned int integer)
{
	switch (integer) {
	case Button1:
		return g_strdup("Button1");
	case Button2:
		return g_strdup("Button2");
	case Button3:
		return g_strdup("Button3");
	case Button4:
		return g_strdup("Button4");
	case Button5:
		return g_strdup("Button5");
	default:
		return g_strdup_printf("%u", integer);
	}
}

char *
putXrmPowerLevel(unsigned integer)
{
	switch (integer) {
	case DPMSModeOn:
		return g_strdup("DPMSModeOn");
	case DPMSModeStandby:
		return g_strdup("DPMSModeStandby");
	case DPMSModeSuspend:
		return g_strdup("DPMSModeSuspend");
	case DPMSModeOff:
		return g_strdup("DPMSModeOff");
	default:
		return putXrmUint(integer);
	}
}

char *
putXrmDouble(double floating)
{
	return g_strdup_printf("%f", floating);
}

char *
putXrmBool(Bool boolean)
{
	return g_strdup(boolean ? "true" : "false");
}

char *
putXrmString(const char *string)
{
	return g_strdup(string);
}

void
put_resources(void)
{
	XrmDatabase rdb;
	char *val, *usrdb;

	usrdb = g_strdup_printf("%s/.config/xde/inputrc", getenv("HOME"));

	rdb = XrmGetStringDatabase("");
	if (!rdb) {
		DPRINTF(1, "no resource manager database allocated\n");
		return;
	}
	/* put a bunch of resources */
	if ((val = putXrmBool(resources.Keyboard.GlobalAutoRepeat))) {
		put_resource(rdb, "keyboard.globalAutoRepeat", val);
		g_free(val);
	}
	if ((val = putXrmInt(resources.Keyboard.KeyClickPercent))) {
		put_resource(rdb, "keyboard.keyClickPercent", val);
		g_free(val);
	}
	if ((val = putXrmInt(resources.Keyboard.BellPercent))) {
		put_resource(rdb, "keyboard.bellPercent", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.Keyboard.BellPitch))) {
		put_resource(rdb, "keyboard.bellPitch", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.Keyboard.BellDuration))) {
		put_resource(rdb, "keyboard.bellDuration", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.Pointer.AccelerationNumerator))) {
		put_resource(rdb, "pointer.accelerationNumerator", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.Pointer.AccelerationDenominator))) {
		put_resource(rdb, "pointer.accelerationDenominator", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.Pointer.Threshold))) {
		put_resource(rdb, "pointer.threshold", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.ScreenSaver.Timeout))) {
		put_resource(rdb, "screenSaver.timeout", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.ScreenSaver.Interval))) {
		put_resource(rdb, "screenSaver.interval", val);
		g_free(val);
	}
	if ((val = putXrmBlanking(resources.ScreenSaver.Preferblanking))) {
		put_resource(rdb, "screenSaver.preferBlanking", val);
		g_free(val);
	}
	if ((val = putXrmExposures(resources.ScreenSaver.Allowexposures))) {
		put_resource(rdb, "screenSaver.allowExposures", val);
		g_free(val);
	}
	if ((val = putXrmInt(resources.DPMS.State))) {
		put_resource(rdb, "dPMS.state", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.DPMS.StandbyTimeout))) {
		put_resource(rdb, "dPMS.standbyTimeout", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.DPMS.SuspendTimeout))) {
		put_resource(rdb, "dPMS.suspendTimeout", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.DPMS.OffTimeout))) {
		put_resource(rdb, "dPMS.offTimeout", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.RepeatKeysEnabled))) {
		put_resource(rdb, "xKeyboard.repeatKeysEnabled", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.RepeatDelay))) {
		put_resource(rdb, "xKeyboard.repeatDelay", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.RepeatInterval))) {
		put_resource(rdb, "xKeyboard.repeatInterval", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.SlowKeysEnabled))) {
		put_resource(rdb, "xKeyboard.slowKeysEnabled", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.SlowKeysDelay))) {
		put_resource(rdb, "xKeyboard.slowKeysDelay", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.BounceKeysEnabled))) {
		put_resource(rdb, "xKeyboard.bounceKeysEnabled", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.DebounceDelay))) {
		put_resource(rdb, "xKeyboard.debounceDelay", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.StickyKeysEnabled))) {
		put_resource(rdb, "xKeyboard.stickyKeysEnabled", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.MouseKeysEnabled))) {
		put_resource(rdb, "xKeyboard.mouseKeysEnabled", val);
		g_free(val);
	}
	if ((val = putXrmButton(resources.XKeyboard.MouseKeysDfltBtn))) {
		put_resource(rdb, "xKeyboard.mouseKeysDfltBtn", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XKeyboard.MouseKeysAccelEnabled))) {
		put_resource(rdb, "xKeyboard.mouseKeysAccelEnabled", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.MouseKeysDelay))) {
		put_resource(rdb, "xKeyboard.mouseKeysDelay", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.MouseKeysInterval))) {
		put_resource(rdb, "xKeyboard.mouseKeysInterval", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.MouseKeysTimeToMax))) {
		put_resource(rdb, "xKeyboard.mouseKeysTimeToMax", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.MouseKeysMaxSpeed))) {
		put_resource(rdb, "xKeyboard.mouseKeysMaxSpeed", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XKeyboard.MouseKeysCurve))) {
		put_resource(rdb, "xKeyboard.mouseKeysCurve", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XF86Misc.KeyboardRate))) {
		put_resource(rdb, "xF86Misc.keyboardRate", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XF86Misc.KeyboardDelay))) {
		put_resource(rdb, "xF86Misc.keyboardDelay", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XF86Misc.MouseEmulate3Buttons))) {
		put_resource(rdb, "xF86Misc.mouseEmulate3Buttons", val);
		g_free(val);
	}
	if ((val = putXrmUint(resources.XF86Misc.MouseEmulate3Timeout))) {
		put_resource(rdb, "xF86Misc.mouseEmulate3Timeout", val);
		g_free(val);
	}
	if ((val = putXrmBool(resources.XF86Misc.MouseChordMiddle))) {
		put_resource(rdb, "xF86Misc.mouseChordMiddle", val);
		g_free(val);
	}
	XrmPutFileDatabase(rdb, usrdb);
	XrmDestroyDatabase(rdb);
	return;
}

void
put_keyfile(void)
{
	char *val, buf[256] = { 0, };

	if (!file) {
		EPRINTF("KEY FILE NOT ALLOCATED! <============\n");
		return;
	}
	if (support.Keyboard) {
		int i, j;

		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_KeyClickPercent,
				       state.Keyboard.key_click_percent);
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellPercent, state.Keyboard.bell_percent);
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellPitch, state.Keyboard.bell_pitch);
		g_key_file_set_integer(file, KFG_Keyboard,
				       KFK_Keyboard_BellDuration, state.Keyboard.bell_duration);
		g_key_file_set_integer(file, KFG_Keyboard,
				KFK_Keyboard_LEDMask, state.Keyboard.led_mask);
		g_key_file_set_boolean(file, KFG_Keyboard,
				       KFK_Keyboard_GlobalAutoRepeat,
				       state.Keyboard.global_auto_repeat);
		for (i = 0, j = 0; i < 32; i++, j += 2)
			snprintf(buf + j, sizeof(buf) - j, "%02X", state.Keyboard.auto_repeats[i]);
		g_key_file_set_value(file, KFG_Keyboard, KFK_Keyboard_AutoRepeats, buf);
	}
	if (support.Pointer) {
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_AccelerationDenominator,
				       state.Pointer.accel_denominator);
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_AccelerationNumerator,
				       state.Pointer.accel_numerator);
		g_key_file_set_integer(file, KFG_Pointer,
				       KFK_Pointer_Threshold, state.Pointer.threshold);
	}
	if (support.ScreenSaver) {
		if ((val = putXrmExposures(state.ScreenSaver.allow_exposures))) {
			g_key_file_set_value(file, KFG_ScreenSaver, KFK_ScreenSaver_AllowExposures, val);
			g_free(val);
		}
		g_key_file_set_integer(file, KFG_ScreenSaver,
				       KFK_ScreenSaver_Interval, state.ScreenSaver.interval);
		if ((val = putXrmBlanking(state.ScreenSaver.prefer_blanking))) {
			g_key_file_set_value(file, KFG_ScreenSaver, KFK_ScreenSaver_PreferBlanking, val);
			g_free(val);
		}
		g_key_file_set_integer(file, KFG_ScreenSaver,
				       KFK_ScreenSaver_Timeout, state.ScreenSaver.timeout);
	}
	if (support.DPMS) {
		if ((val = putXrmPowerLevel(state.DPMS.power_level))) {
			g_key_file_set_value(file, KFG_DPMS, KFK_DPMS_PowerLevel, val);
			g_free(val);
		}
		g_key_file_set_boolean(file, KFG_DPMS,
				       KFK_DPMS_State, state.DPMS.state ? TRUE : FALSE);
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_StandbyTimeout, state.DPMS.standby);
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_SuspendTimeout, state.DPMS.suspend);
		g_key_file_set_integer(file, KFG_DPMS, KFK_DPMS_OffTimeout, state.DPMS.off);
	}
	if (support.XKeyboard) {
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysDfltBtn,
				       state.XKeyboard.desc->ctrls->mk_dflt_btn);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_RepeatKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbRepeatKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_SlowKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbSlowKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_BounceKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbBounceKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_StickyKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbStickyKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbMouseKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysAccelEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbMouseKeysAccelMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXKeysEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXKeysMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXTimeoutMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXTimeoutMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXFeedbackMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXFeedbackMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AudibleBellMaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAudibleBellMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_Overlay1MaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbOverlay1Mask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_Overlay2MaskEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbOverlay2Mask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_IgnoreGroupLockModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbIgnoreGroupLockMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_GroupsWrapEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbGroupsWrapMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_InternalModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbInternalModsMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_IgnoreLockModsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbIgnoreLockModsMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_PerKeyRepeatEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbPerKeyRepeatMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_ControlsEnabledEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbControlsEnabledMask ? TRUE : FALSE);
		g_key_file_set_boolean(file, KFG_XKeyboard,
				       KFK_XKeyboard_AccessXOptionsEnabled,
				       state.XKeyboard.desc->ctrls->enabled_ctrls &
				       XkbAccessXOptionsMask ? TRUE : FALSE);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_RepeatDelay,
				       state.XKeyboard.desc->ctrls->repeat_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_RepeatInterval,
				       state.XKeyboard.desc->ctrls->repeat_interval);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_SlowKeysDelay,
				       state.XKeyboard.desc->ctrls->slow_keys_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_DebounceDelay,
				       state.XKeyboard.desc->ctrls->debounce_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysDelay,
				       state.XKeyboard.desc->ctrls->mk_delay);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysInterval,
				       state.XKeyboard.desc->ctrls->mk_interval);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysTimeToMax,
				       state.XKeyboard.desc->ctrls->mk_time_to_max);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysMaxSpeed,
				       state.XKeyboard.desc->ctrls->mk_max_speed);
		g_key_file_set_integer(file, KFG_XKeyboard,
				       KFK_XKeyboard_MouseKeysCurve,
				       state.XKeyboard.desc->ctrls->mk_curve);
	}
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
			DPRINTF(1, "%s:\t\t%s\n", clas, value.addr);
			return (const char *) value.addr;
		} else
			DPRINTF(1, "%s:\t\t%s\n", clas, value.addr);
	} else
		DPRINTF(1, "%s:\t\t%s\n", clas, "ERROR!");
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

Bool
getXrmInt(const char *val, int *integer)
{
	char *endptr = NULL;
	int value;

	value = strtol(val, &endptr, 0);
	if (endptr && !*endptr) {
		*integer = value;
		return True;
	}
	return False;
}

Bool
getXrmUint(const char *val, unsigned int *integer)
{
	char *endptr = NULL;
	unsigned int value;

	value = strtoul(val, &endptr, 0);
	if (endptr && !*endptr) {
		*integer = value;
		return True;
	}
	return False;
}

Bool
getXrmBlanking(const char *val, unsigned int *integer)
{
	if (!strcasecmp(val, "DontPreferBlanking")) {
		*integer = DontPreferBlanking;
		return True;
	}
	if (!strcasecmp(val, "PreferBlanking")) {
		*integer = PreferBlanking;
		return True;
	}
	if (!strcasecmp(val, "DefaultBlanking")) {
		*integer = DefaultBlanking;
		return True;
	}
	return getXrmUint(val, integer);
}

Bool
getXrmExposures(const char *val, unsigned int *integer)
{
	if (!strcasecmp(val, "DontAllowExposures")) {
		*integer = DontAllowExposures;
		return True;
	}
	if (!strcasecmp(val, "AllowExposures")) {
		*integer = AllowExposures;
		return True;
	}
	if (!strcasecmp(val, "DefaultExposures")) {
		*integer = DefaultExposures;
		return True;
	}
	return getXrmUint(val, integer);
}

Bool
getXrmButton(const char *val, unsigned int *integer)
{
	if (!strcasecmp(val, "Button1")) {
		*integer = 1;
		return True;
	}
	if (!strcasecmp(val, "Button2")) {
		*integer = 2;
		return True;
	}
	if (!strcasecmp(val, "Button3")) {
		*integer = 3;
		return True;
	}
	if (!strcasecmp(val, "Button4")) {
		*integer = 4;
		return True;
	}
	if (!strcasecmp(val, "Button5")) {
		*integer = 5;
		return True;
	}
	return getXrmUint(val, integer);
}

Bool
getXrmPowerLevel(const char *val, unsigned int *integer)
{
	if (!strcasecmp(val, "DPMSModeOn")) {
		*integer = DPMSModeOn;
		return True;
	}
	if (!strcasecmp(val, "DPMSModeStandby")) {
		*integer = DPMSModeStandby;
		return True;
	}
	if (!strcasecmp(val, "DPMSModeSuspend")) {
		*integer = DPMSModeSuspend;
		return True;
	}
	if (!strcasecmp(val, "DPMSModeOff")) {
		*integer = DPMSModeOff;
		return True;
	}
	return getXrmUint(val, integer);
}

Bool
getXrmDouble(const char *val, double *floating)
{
	const struct lconv *lc = localeconv();
	char radix, *copy = strdup(val);

	if ((radix = lc->decimal_point[0]) != '.' && strchr(copy, '.'))
		*strchr(copy, '.') = radix;

	*floating = strtod(copy, NULL);
	DPRINTF(1, "Got decimal value %s, translates to %f\n", val, *floating);
	free(copy);
	return True;
}

Bool
getXrmBool(const char *val, Bool *boolean)
{
	int len;

	if ((len = strlen(val))) {
		if (!strncasecmp(val, "true", len)) {
			*boolean = True;
			return True;
		}
		if (!strncasecmp(val, "false", len)) {
			*boolean = False;
			return True;
		}
	}
	EPRINTF("could not parse boolean'%s'\n", val);
	return False;
}

Bool
getXrmString(const char *val, char **string)
{
	char *tmp;

	if ((tmp = strdup(val))) {
		free(*string);
		*string = tmp;
		return True;
	}
	return False;
}

void
get_resources(int argc, char *argv[])
{
	XrmDatabase rdb;
	Display *dpy;
	const char *val;
	char *usrdflt;

	PTRACE(5);
	XrmInitialize();
	if (getenv("DISPLAY")) {
		if (!(dpy = XOpenDisplay(NULL))) {
			EPRINTF("could not open display %s\n", getenv("DISPLAY"));
			exit(EXIT_FAILURE);
		}
		rdb = XrmGetDatabase(dpy);
		if (!rdb)
			DPRINTF(1, "no resource manager database allocated\n");
		XCloseDisplay(dpy);
	}
	usrdflt = g_strdup_printf(USRDFLT, getenv("HOME"));
	if (!XrmCombineFileDatabase(usrdflt, &rdb, False))
		DPRINTF(1, "could not open rcfile %s\n", usrdflt);
	g_free(usrdflt);
	if (!XrmCombineFileDatabase(APPDFLT, &rdb, False))
		DPRINTF(1, "could not open rcfile %s\n", APPDFLT);
	if (!rdb) {
		DPRINTF(1, "no resource manager database found\n");
		rdb = XrmGetStringDatabase("");
	}
	if ((val = get_resource(rdb, "debug", "0"))) {
		getXrmInt(val, &options.debug);
	}
	/* get a bunch of resources */
	if ((val = get_resource(rdb, "keyboard.globalAutoRepeat", NULL)))
		getXrmBool(val, &resources.Keyboard.GlobalAutoRepeat);
	if ((val = get_resource(rdb, "keyboard.keyClickPercent", NULL)))
		getXrmInt(val, &resources.Keyboard.KeyClickPercent);
	if ((val = get_resource(rdb, "keyboard.bellPercent", NULL)))
		getXrmInt(val, &resources.Keyboard.BellPercent);
	if ((val = get_resource(rdb, "keyboard.bellPitch", NULL)))
		getXrmUint(val, &resources.Keyboard.BellPitch);
	if ((val = get_resource(rdb, "keyboard.bellDuration", NULL)))
		getXrmUint(val, &resources.Keyboard.BellDuration);
	if ((val = get_resource(rdb, "pointer.accelerationNumerator", NULL)))
		getXrmUint(val, &resources.Pointer.AccelerationNumerator);
	if ((val = get_resource(rdb, "pointer.accelerationDenominator", NULL)))
		getXrmUint(val, &resources.Pointer.AccelerationDenominator);
	if ((val = get_resource(rdb, "pointer.threshold", NULL)))
		getXrmUint(val, &resources.Pointer.Threshold);
	if ((val = get_resource(rdb, "screenSaver.timeout", NULL)))
		getXrmUint(val, &resources.ScreenSaver.Timeout);
	if ((val = get_resource(rdb, "screenSaver.interval", NULL)))
		getXrmUint(val, &resources.ScreenSaver.Interval);
	if ((val = get_resource(rdb, "screenSaver.preferBlanking", NULL)))
		getXrmBlanking(val, &resources.ScreenSaver.Preferblanking);
	if ((val = get_resource(rdb, "screenSaver.allowExposures", NULL)))
		getXrmExposures(val, &resources.ScreenSaver.Allowexposures);
	if ((val = get_resource(rdb, "dPMS.state", NULL)))
		getXrmInt(val, &resources.DPMS.State);
	if ((val = get_resource(rdb, "dPMS.standbyTimeout", NULL)))
		getXrmUint(val, &resources.DPMS.StandbyTimeout);
	if ((val = get_resource(rdb, "dPMS.suspendTimeout", NULL)))
		getXrmUint(val, &resources.DPMS.SuspendTimeout);
	if ((val = get_resource(rdb, "dPMS.offTimeout", NULL)))
		getXrmUint(val, &resources.DPMS.OffTimeout);
	if ((val = get_resource(rdb, "xKeyboard.repeatKeysEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.RepeatKeysEnabled);
	if ((val = get_resource(rdb, "xKeyboard.repeatDelay", NULL)))
		getXrmUint(val, &resources.XKeyboard.RepeatDelay);
	if ((val = get_resource(rdb, "xKeyboard.repeatInterval", NULL)))
		getXrmUint(val, &resources.XKeyboard.RepeatInterval);
	if ((val = get_resource(rdb, "xKeyboard.slowKeysEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.SlowKeysEnabled);
	if ((val = get_resource(rdb, "xKeyboard.slowKeysDelay", NULL)))
		getXrmUint(val, &resources.XKeyboard.SlowKeysDelay);
	if ((val = get_resource(rdb, "xKeyboard.bounceKeysEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.BounceKeysEnabled);
	if ((val = get_resource(rdb, "xKeyboard.debounceDelay", NULL)))
		getXrmUint(val, &resources.XKeyboard.DebounceDelay);
	if ((val = get_resource(rdb, "xKeyboard.stickyKeysEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.StickyKeysEnabled);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.MouseKeysEnabled);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysDfltBtn", NULL)))
		getXrmButton(val, &resources.XKeyboard.MouseKeysDfltBtn);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysAccelEnabled", NULL)))
		getXrmBool(val, &resources.XKeyboard.MouseKeysAccelEnabled);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysDelay", NULL)))
		getXrmUint(val, &resources.XKeyboard.MouseKeysDelay);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysInterval", NULL)))
		getXrmUint(val, &resources.XKeyboard.MouseKeysInterval);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysTimeToMax", NULL)))
		getXrmUint(val, &resources.XKeyboard.MouseKeysTimeToMax);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysMaxSpeed", NULL)))
		getXrmUint(val, &resources.XKeyboard.MouseKeysMaxSpeed);
	if ((val = get_resource(rdb, "xKeyboard.mouseKeysCurve", NULL)))
		getXrmUint(val, &resources.XKeyboard.MouseKeysCurve);
	if ((val = get_resource(rdb, "xF86Misc.keyboardRate", NULL)))
		getXrmUint(val, &resources.XF86Misc.KeyboardRate);
	if ((val = get_resource(rdb, "xF86Misc.keyboardDelay", NULL)))
		getXrmUint(val, &resources.XF86Misc.KeyboardDelay);
	if ((val = get_resource(rdb, "xF86Misc.mouseEmulate3Buttons", NULL)))
		getXrmBool(val, &resources.XF86Misc.MouseEmulate3Buttons);
	if ((val = get_resource(rdb, "xF86Misc.mouseEmulate3Timeout", NULL)))
		getXrmUint(val, &resources.XF86Misc.MouseEmulate3Timeout);
	if ((val = get_resource(rdb, "xF86Misc.mouseChordMiddle", NULL)))
		getXrmBool(val, &resources.XF86Misc.MouseChordMiddle);

	XrmDestroyDatabase(rdb);
}

void
get_keyfile(void)
{
}

static void
set_defaults(void)
{
	char *file, *p;
	const char *env;

	if ((env = getenv("DISPLAY")))
		options.display = strdup(env);
	if ((p = getenv("XDE_DEBUG")))
		options.debug = atoi(p);
	file = calloc(PATH_MAX, sizeof(*file));
	if ((env = getenv("XDG_CONFIG_HOME")))
		strcpy(file, env);
	else if ((env = getenv("HOME"))) {
		strcpy(file, env);
		strcat(file, "/.config");
	} else {
		strcpy(file, ".");
	}
	strcat(file, "/xde/input.ini");
	options.filename = strdup(file);
	free(file);
}

static int
find_pointer_screen(void)
{
	return 0; /* FIXME: write this */
}

static int
find_focus_screen(void)
{
	return 0; /* FIXME: write this */
}

static void
get_defaults(void)
{
	const char *p;
	int n;

	if (!options.display) {
		EPRINTF("No DISPLAY environment variable nor --display option\n");
		exit(EXIT_FAILURE);
	}
	if (options.screen < 0 && (p = strrchr(options.display, '.'))
	    && (n = strspn(++p, "0123456789")) && *(p + n) == '\0')
		options.screen = atoi(p);
	if (options.screen < 0 && options.command == CommandEditor) {
		if (options.button)
			options.screen = find_pointer_screen();
		else
			options.screen = find_focus_screen();
	}
	if (options.command == CommandDefault)
		options.command = CommandRun;
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;

	setlocale(LC_ALL, "");

	set_defaults();

	saveArgc = argc;
	saveArgv = argv;

	get_resources(argc, argv);

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"display",	required_argument,	NULL, 'd'},
			{"screen",	required_argument,	NULL, 's'},
			{"button",	required_argument,	NULL, 'b'},
			{"replace",	no_argument,		NULL, 'r'},
			{"quit",	no_argument,		NULL, 'q'},
			{"editor",	no_argument,		NULL, 'e'},

			{"clientId",	required_argument,	NULL, '8'},
			{"restore",	required_argument,	NULL, '9'},

			{"filename",	required_argument,	NULL, 'f'},
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

		c = getopt_long_only(argc, argv, "nD::v::hVCH?", long_options, &option_index);
#else				/* defined _GNU_SOURCE */
		c = getopt(argc, argv, "nDvhVC?");
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
			setenv("DISPLAY", optarg, TRUE);
			free(options.display);
			options.display = strdup(optarg);
			break;
		case 's':	/* -s, --screen SCREEN */
			options.screen = strtoul(optarg, &endptr, 0);
			if (endptr && *endptr)
				goto bad_option;
			break;
		case 'b':	/* -b, --button BUTTON */
			options.button = strtoul(optarg, &endptr, 0);
			if (endptr && *endptr)
				goto bad_option;
			if (options.button < 0 || options.button > 5)
				goto bad_option;
			break;
		case 'r':	/* -r, --replace */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandReplace;
			options.command = CommandReplace;
			break;
		case 'q':	/* -q, --quit */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandQuit;
			options.command = CommandQuit;
			break;
		case 'e':	/* -e, --editor */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandEditor;
			options.command = CommandEditor;
			break;

		case 'f':	/* -f, --filename FILENAME */
			free(options.filename);
			options.filename = strdup(optarg);
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
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandVersion;
			options.command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			if (options.command != CommandDefault)
				goto bad_command;
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
		      bad_command:
			fprintf(stderr, "%s: only one command option allowed\n", argv[0]);
			goto bad_usage;
		}
	}
	DPRINTF(1, "%s: option index = %d\n", argv[0], optind);
	DPRINTF(1, "%s: option count = %d\n", argv[0], argc);
	if (optind < argc) {
		fprintf(stderr, "%s: excess non-option arguments near '", argv[0]);
		while (optind < argc) {
			fprintf(stderr, "%s", argv[optind++]);
			fprintf(stderr, "%s", (optind < argc) ? " " : "");
		}
		fprintf(stderr, "'\n");
		usage(argc, argv);
		exit(EXIT_SYNTAXERR);
	}
	get_defaults();
	startup(argc, argv);
	switch (command) {
	case CommandDefault:
	case CommandRun:
		DPRINTF(1, "%s: running a new instance\n", argv[0]);
		do_run(argc, argv, False);
		break;
	case CommandReplace:
		DPRINTF(1, "%s: replacing existing instance\n", argv[0]);
		do_run(argc, argv, True);
		break;
	case CommandQuit:
		DPRINTF(1, "%s: asking xisting instance to quit\n", argv[0]);
		do_quit(argc, argv);
		break;
	case CommandEditor:
		DPRINTF(1, "%s: invoking the editor\n", argv[0]);
		do_editor(argc, argv);
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
	default:
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
