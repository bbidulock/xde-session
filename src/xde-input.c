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
#include <gtk/gtk.h>
#include <cairo.h>

#define XPRINTF(args...) do { } while (0)
#define OPRINTF(args...) do { if (options.output > 1) { \
	fprintf(stderr, "I: "); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define DPRINTF(args...) do { if (options.debug) { \
	fprintf(stderr, "D: %s +%d %s(): ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr); } } while (0)
#define EPRINTF(args...) do { \
	fprintf(stderr, "E: %s +%d %s(): ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, args); \
	fflush(stderr);   } while (0)
#define DPRINT() do { if (options.debug) { \
	fprintf(stderr, "D: %s +%d %s()\n", __FILE__, __LINE__, __func__); \
	fflush(stderr); } } while (0)


#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

#define RESNAME "xde-input"
#define RESCLAS "XDE-Input"
#define RESTITL "X11 Input"

#define APPDFLT "/usr/share/X11/app-defaults/" RESCLAS

Display *dpy;

typedef struct {
	int output;
	int debug;
	Bool dryrun;
	Bool editor;
	char *filename;
} Options;

Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.editor = False,
	.filename = NULL,
};

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

GKeyFile *file;

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

static void
get_input()
{
	char buf[256] = { 0, };
	int i, j;

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

	if (support.XKeyboard) {
#if 0
		unsigned int which = XkbControlsMask;
#endif

		state.XKeyboard.desc = XkbGetKeyboard(dpy, XkbControlsMask, XkbUseCoreKbd);

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
}

/** @brief read input settings
  * 
  * Read the input settings from the configuration file.  Simple and direct.
  * The file is an .ini-style keyfile.  Use glib to read in the values.
  */
void
read_input(const char *filename)
{
	GError *error = NULL;

	file = g_key_file_new();
	if (!g_key_file_load_from_file(file, filename, G_KEY_FILE_NONE, &error)) {
		return;
	}
}

/** @brief write input settings
  *
  * Write the input settings back to the configuration file.  Simple and direct.
  * The file is an .ini-style keyfile.  Use glib to write out the values.
  */
void
write_input()
{
}

void
set_input(const char *filename)
{
	read_input(filename);
	if (g_key_file_has_group(file, KFG_Keyboard) && support.Keyboard) {
		XKeyboardControl kbd = { 0, };
		unsigned long value_mask = 0;

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
	if (g_key_file_has_group(file, KFG_Pointer) && support.Pointer) {
		Bool do_accel, do_threshold;
		int accel_numerator, accel_denominator, threshold;

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
	if (g_key_file_has_group(file, KFG_XKeyboard) && support.XKeyboard) {
	}
	if (g_key_file_has_group(file, KFG_ScreenSaver) && support.ScreenSaver) {
		int timeout, interval, prefer_blanking, allow_exposures;

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
	if (g_key_file_has_group(file, KFG_DPMS) && support.DPMS) {
		int standby, suspend, off, level;

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
}

void
reprocess_input()
{
	get_input();
	edit_set_values();
	process_errors();
	purge_queue();
}

void
startup()
{
	Window window = None;

	support.Keyboard = True;
	DPRINTF("Keyboard: core protocol support\n");
	support.Pointer = True;
	DPRINTF("Pointer: core protocol support\n");
	if (XkbQueryExtension(dpy, &state.XKeyboard.opcode, &state.XKeyboard.event,
			      &state.XKeyboard.error, &state.XKeyboard.major_version,
			      &state.XKeyboard.minor_version)) {
		support.XKeyboard = True;
		DPRINTF("XKeyboard: opcode=%d, event=%d, error=%d, major=%d, minor=%d\n",
			state.XKeyboard.opcode, state.XKeyboard.event, state.XKeyboard.error,
			state.XKeyboard.major_version, state.XKeyboard.minor_version);
	} else {
		support.XKeyboard = False;
		DPRINTF("XKeyboard: not supported\n");
	}
	if (XScreenSaverQueryExtension(dpy, &state.ScreenSaver.event, &state.ScreenSaver.error) &&
	    XScreenSaverQueryVersion(dpy, &state.ScreenSaver.major_version,
				     &state.ScreenSaver.minor_version)) {
		support.ScreenSaver = True;
		DPRINTF("ScreenSaver: event=%d, error=%d, major=%d, minor=%d\n",
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
		support.ScreenSaver = False;
		DPRINTF("ScreenSaver: not supported\n");
	}
	if (DPMSQueryExtension(dpy, &state.DPMS.event, &state.DPMS.error) &&
	    DPMSGetVersion(dpy, &state.DPMS.major_version, &state.DPMS.minor_version)) {
		support.DPMS = True;
		DPRINTF("DPMS: event=%d, error=%d, major=%d, minor=%d\n",
			state.DPMS.event, state.DPMS.error, state.DPMS.major_version,
			state.DPMS.minor_version);
	} else {
		support.DPMS = False;
		DPRINTF("DPMS: not supported\n");
	}
	if (XF86MiscQueryExtension(dpy, &state.XF86Misc.event, &state.XF86Misc.error) &&
	    XF86MiscQueryVersion(dpy, &state.XF86Misc.major_version,
				 &state.XF86Misc.minor_version)) {
		support.XF86Misc = True;
		DPRINTF("XF86Misc: event=%d, error=%d, major=%d, minor=%d\n",
			state.XF86Misc.event, state.XF86Misc.error, state.XF86Misc.major_version,
			state.XF86Misc.minor_version);
	} else {
		support.XF86Misc = False;
		DPRINTF("XF86Misc: not supported\n");
	}
}

static gchar *
format_value_milliseconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g ms", gtk_scale_get_digits(scale), value);
}

static gchar *
format_value_seconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g s", gtk_scale_get_digits(scale), value);
}

static gchar *
format_value_percent(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g%%", gtk_scale_get_digits(scale), value);
}

static char *
format_value_hertz(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g Hz", gtk_scale_get_digits(scale), value);
}

static void
accel_numerator_value_changed(GtkRange * range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.Pointer.accel_numerator) {
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
	XForceScreenSaver(dpy, ScreenSaverActive);
	XFlush(dpy);
}

static void
screensaver_interval_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);

	if (val != state.ScreenSaver.interval) {
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
	XForceScreenSaver(dpy, ScreenSaverActive);
	XFlush(dpy);
}

static void
prefer_blanking_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	int value = active ? PreferBlanking : DontPreferBlanking;

	if (value != state.ScreenSaver.prefer_blanking) {
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
	DPMSForceLevel(dpy, DPMSModeStandby);
}

static void
suspend_timeout_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	CARD16 val = round(value);

	if (val != state.DPMS.suspend) {
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
	DPMSForceLevel(dpy, DPMSModeOff);
}

static void
keyboard_rate_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble value = gtk_range_get_value(range);
	int val = round(value);
	if (val != state.XF86Misc.keyboard.rate) {
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
		state.XF86Misc.keyboard.delay = val;
		XF86MiscSetKbdSettings(dpy, &state.XF86Misc.keyboard);
	}
	reprocess_input();
}

static void
emulate_3_buttons_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
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
		state.XF86Misc.mouse.emulate3timeout = val;
		XF86MiscSetMouseSettings(dpy, &state.XF86Misc.mouse);
	}
	reprocess_input();
}

static void
chord_middle_toggled(GtkToggleButton *button, gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
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
		DPRINTF("no resource manager database allocated\n");
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
	DPRINTF("Got decimal value %s, translates to %f\n", val, *floating);
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
	Display *dpy;
	XrmDatabase rdb;
	const char *val;
	char *sysdb, *usrdb;

	sysdb = g_strdup_printf("/usr/share/X11/app-defaults/%s", RESCLAS);
	usrdb = g_strdup_printf("%s/.config/xde/inputrc", getenv("HOME"));

	DPRINT();
	if (!(dpy = XOpenDisplay(NULL))) {
		EPRINTF("could not open display %s\n", getenv("DISPLAY"));
		exit(EXIT_FAILURE);
	}
	XrmInitialize();
#if 0
	{
		Window root;
		Atom atom;
		XTextProperty xtp;

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
		rdb = XrmGetStringDatabase((char *) xtp.value);
	}
#else
	XrmGetDatabase(dpy);
#endif
	XrmCombineFileDatabase(usrdb, &rdb, False);
	XrmCombineFileDatabase(sysdb, &rdb, False);
#if 0
	XFree(xtp.value);
#endif
	if (!rdb) {
		DPRINTF("no resource manager database allocated\n");
		XCloseDisplay(dpy);
		return;
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
	XCloseDisplay(dpy);
	return;
}

void
get_keyfile(void)
{
}

void
set_defaults(void)
{
	char *file, *p;
	const char *s;

	if ((p = getenv("XDE_DEBUG")))
		options.debug = atoi(p);
	file = calloc(PATH_MAX, sizeof(*file));
	if ((s = getenv("XDG_CONFIG_HOME")))
		strcpy(file, s);
	else if ((s = getenv("HOME"))) {
		strcpy(file, s);
		strcat(file, "/.config");
	} else {
		strcpy(file, ".");
	}
	strcat(file, "/xde/input.ini");
	options.filename = strdup(file);
	free(file);
}

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	set_defaults();
	get_resources(argc, argv);

	while(1) {
		int c, val;
#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"editor",	no_argument,		NULL, 'e'},
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

		c = getopt_long_only(argc, argv, "nD::v::hVCH?",
				     long_options, &option_index);
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

		case 'e':	/* -e, --editor */
			options.editor = True;
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
				fprintf(stderr, "%s: increasing debug verbosity\n",
					argv[0]);
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
				fprintf(stderr, "%s: increasing output verbosity\n",
					argv[0]);
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
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(0);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n",
					argv[0]);
			version(argc, argv);
			exit(0);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n",
					argv[0]);
			copying(argc, argv);
			exit(0);
		case '?':
		default:
		      bad_option:
			optind--;
			goto bad_nonopt;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '",
						argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "'\n");
				} else {
					fprintf(stderr, "%s: missing option or argument",
						argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind < argc) {
		goto bad_nonopt;
	}
	exit(0);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
