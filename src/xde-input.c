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
		int major_version;
		int minor_version;
		CARD16 power_level;
		BOOL state;
		CARD16 standby;
		CARD16 suspend;
		CARD16 off;
	} DPMS;
	struct {
		int opcode;
		int event;
		int error;
		int major_version;
		int minor_version;
		XkbDescPtr desc;
	} XKeyboard;
} State;

State state;

typedef struct {
	struct {
		char *KeyClickPercent;
		char *BellPercent;
		char *BellPitch;
		char *BellDuration;
		char *LEDMask;
		char *GlobalAutoRepeat;
		char *AutoRepeats;
	} Keyboard;
	struct {
		char *AccelerationDenominator;
		char *AccelerationNumerator;
		char *Threshold;
	} Pointer;
	struct {
		char *Allowexposures;
		char *Interval;
		char *Preferblanking;
		char *Timeout;
	} ScreenSaver;
	struct {
		char *OffTimeout;
		char *PowerLevel;
		char *StandbyTimeout;
		char *State;
		char *SuspendTimeout;
	} DPMS;
	struct {
		char *AccessXFeedbackMaskEnabled;
		char *AccessXKeysEnabled;
		char *AccessXOptions;
		char *AccessXOptionsEnabled;
		char *AccessXTimeout;
		char *AccessXTimeoutMask;
		char *AccessXTimeoutMaskEnabled;
		char *AccessXTimeoutOptionsMask;
		char *AccessXTimeoutOptionsValues;
		char *AccessXTimeoutValues;
		char *AudibleBellMaskEnabled;
		char *BounceKeysEnabled;
		char *ControlsEnabledEnabled;
		char *DebounceDelay;
		char *GroupsWrapEnabled;
		char *IgnoreGroupLockModsEnabled;
		char *IgnoreLockModsEnabled;
		char *InternalModsEnabled;
		char *MouseKeysAccelEnabled;
		char *MouseKeysCurve;
		char *MouseKeysDelay;
		char *MouseKeysDfltBtn;
		char *MouseKeysEnabled;
		char *MouseKeysInterval;
		char *MouseKeysMaxSpeed;
		char *MouseKeysTimeToMax;
		char *Overlay1MaskEnabled;
		char *Overlay2MaskEnabled;
		char *PerKeyRepeat;
		char *PerKeyRepeatEnabled;
		char *RepeatDelay;
		char *RepeatInterval;
		char *RepeatKeysEnabled;
		char *RepeatRate;
		char *SlowKeysDelay;
		char *SlowKeysEnabled;
		char *StickyKeysEnabled;
	} XKeyboard;
} Config;

Config config;

typedef struct {
	struct {
		GtkWidget *GlobalAutoRepeat;
		GtkWidget *KeyClickPercent;
		GtkWidget *BellPercent;
		GtkWidget *BellPitch;
		GtkWidget *BellDuration;
	} Keyboard;
	struct {
		GtkWidget *AccelerationDenominator;
		GtkWidget *AccelerationNumerator;
		GtkWidget *Threshold;
	} Pointer;
	struct {
		GtkWidget *Timeout;
		GtkWidget *Interval;
		GtkWidget *PreferBlank;
		GtkWidget *AllowExpose;
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
} Controls;

Controls controls;


void
get_input()
{
	char buf[256] = { 0, };
	int i, j;

	XGetKeyboardControl(dpy, &state.Keyboard);

	if (options.debug) {
		fputs("Keyboard Control:\n", stderr);
		fprintf(stderr, "\tkey-click-percent: %d\n", state.Keyboard.key_click_percent);
		fprintf(stderr, "\tbell-percent: %d\n", state.Keyboard.bell_percent);
		fprintf(stderr, "\tbell-pitch: %u Hz\n", state.Keyboard.bell_pitch);
		fprintf(stderr, "\tbell-duration: %u milliseconds\n", state.Keyboard.bell_duration);
		fprintf(stderr, "\tled-mask: 0x%08lx\n", state.Keyboard.led_mask);
		fprintf(stderr, "\tglobal-auto-repeat: %s\n", state.Keyboard.global_auto_repeat ? "Yes" : "No");
		fputs("\tauto-repeats: ", stderr);
		for (i = 0; i < 32; i++)
			fprintf(stderr, "%02X", state.Keyboard.auto_repeats[i]);
		fputs("\n", stderr);
	}

	free(config.Keyboard.KeyClickPercent);
	snprintf(buf, sizeof(buf), "%d", state.Keyboard.key_click_percent);
	config.Keyboard.KeyClickPercent = strndup(buf, sizeof(buf));

	free(config.Keyboard.BellPercent);
	snprintf(buf, sizeof(buf), "%d", state.Keyboard.bell_percent);
	config.Keyboard.BellPercent = strndup(buf, sizeof(buf));

	free(config.Keyboard.BellPitch);
	snprintf(buf, sizeof(buf), "%u", state.Keyboard.bell_pitch);
	config.Keyboard.BellPitch = strndup(buf, sizeof(buf));

	free(config.Keyboard.BellDuration);
	snprintf(buf, sizeof(buf), "%u", state.Keyboard.bell_duration);
	config.Keyboard.BellDuration = strndup(buf, sizeof(buf));

	free(config.Keyboard.LEDMask);
	snprintf(buf, sizeof(buf), "0x%lx", state.Keyboard.led_mask);
	config.Keyboard.LEDMask = strndup(buf, sizeof(buf));

	free(config.Keyboard.GlobalAutoRepeat);
	snprintf(buf, sizeof(buf), "%s", state.Keyboard.global_auto_repeat ? "On" : "Off");
	config.Keyboard.GlobalAutoRepeat = strndup(buf, sizeof(buf));

	free(config.Keyboard.AutoRepeats);
	for (i = 0, j = 0; i < 32; i++, j += 2)
		snprintf(buf + j, sizeof(buf) - j, "%02X", state.Keyboard.auto_repeats[i]);
	config.Keyboard.AutoRepeats = strndup(buf, sizeof(buf));

	XGetPointerControl(dpy, &state.Pointer.accel_numerator, &state.Pointer.accel_denominator, &state.Pointer.threshold);

	if (options.debug) {
		fputs("Pointer Control:\n", stderr);
		fprintf(stderr, "\tacceleration-numerator: %d\n", state.Pointer.accel_numerator);
		fprintf(stderr, "\tacceleration-denominator: %d\n", state.Pointer.accel_denominator);
		fprintf(stderr, "\tthreshold: %d\n", state.Pointer.threshold);
	}

	free(config.Pointer.AccelerationDenominator);
	snprintf(buf, sizeof(buf), "%d", state.Pointer.accel_denominator);
	config.Pointer.AccelerationDenominator = strndup(buf, sizeof(buf));

	free(config.Pointer.AccelerationNumerator);
	snprintf(buf, sizeof(buf), "%d", state.Pointer.accel_numerator);
	config.Pointer.AccelerationNumerator = strndup(buf, sizeof(buf));

	free(config.Pointer.Threshold);
	snprintf(buf, sizeof(buf), "%d", state.Pointer.threshold);
	config.Pointer.Threshold = strndup(buf, sizeof(buf));

	XGetScreenSaver(dpy, &state.ScreenSaver.timeout, &state.ScreenSaver.interval, &state.ScreenSaver.prefer_blanking, &state.ScreenSaver.allow_exposures);

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
			fprintf(stderr, "(unknown) %d\n", state.ScreenSaver.prefer_blanking);
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
			fprintf(stderr, "(unknown) %d\n", state.ScreenSaver.allow_exposures);
			break;
		}
	}

	free(config.ScreenSaver.Allowexposures);
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
	config.ScreenSaver.Allowexposures = strndup(buf, sizeof(buf));

	free(config.ScreenSaver.Interval);
	snprintf(buf, sizeof(buf), "%d", state.ScreenSaver.interval);
	config.ScreenSaver.Interval = strndup(buf, sizeof(buf));

	free(config.ScreenSaver.Preferblanking);
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
	config.ScreenSaver.Preferblanking = strndup(buf, sizeof(buf));

	free(config.ScreenSaver.Timeout);
	snprintf(buf, sizeof(buf), "%d", state.ScreenSaver.timeout);
	config.ScreenSaver.Timeout = strndup(buf, sizeof(buf));

	if (DPMSGetVersion(dpy, &state.DPMS.major_version, &state.DPMS.minor_version)) {
		DPMSInfo(dpy, &state.DPMS.power_level, &state.DPMS.state);
		DPMSGetTimeouts(dpy, &state.DPMS.standby, &state.DPMS.suspend, &state.DPMS.off);
		if (options.debug) {
			fputs("DPMS:\n", stderr);
			fprintf(stderr, "\tDPMS Version: %d.%d\n", state.DPMS.major_version, state.DPMS.minor_version);
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

		free(config.DPMS.PowerLevel);
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
		config.DPMS.PowerLevel = strndup(buf, sizeof(buf));

		free(config.DPMS.State);
		snprintf(buf, sizeof(buf), "%s", state.DPMS.state ? "True" : "False");
		config.DPMS.State = strndup(buf, sizeof(buf));

		free(config.DPMS.StandbyTimeout);
		snprintf(buf, sizeof(buf), "%hu", state.DPMS.standby);
		config.DPMS.StandbyTimeout = strndup(buf, sizeof(buf));

		free(config.DPMS.SuspendTimeout);
		snprintf(buf, sizeof(buf), "%hu", state.DPMS.suspend);
		config.DPMS.SuspendTimeout = strndup(buf, sizeof(buf));

		free(config.DPMS.OffTimeout);
		snprintf(buf, sizeof(buf), "%hu", state.DPMS.off);
		config.DPMS.OffTimeout = strndup(buf, sizeof(buf));
	}

	if (XkbQueryExtension(dpy, &state.XKeyboard.opcode, &state.XKeyboard.event,
			      &state.XKeyboard.error, &state.XKeyboard.major_version, &state.XKeyboard.minor_version)) {

		static const char *_true = "true";
		static const char *_false = "false";
#if 0
		unsigned int which = XkbControlsMask;
#endif

		state.XKeyboard.desc = XkbGetKeyboard(dpy, XkbControlsMask, XkbUseCoreKbd);

		free(config.XKeyboard.MouseKeysDfltBtn);
		snprintf(buf, sizeof(buf), "%hhu", state.XKeyboard.desc->ctrls->mk_dflt_btn);
		config.XKeyboard.MouseKeysDfltBtn = strdup(buf);

		free(config.XKeyboard.RepeatKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbRepeatKeysMask) ? _true : _false);
		config.XKeyboard.RepeatKeysEnabled = strdup(buf);

		free(config.XKeyboard.SlowKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbSlowKeysMask) ? _true : _false);
		config.XKeyboard.SlowKeysEnabled = strdup(buf);

		free(config.XKeyboard.BounceKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbBounceKeysMask) ? _true : _false);
		config.XKeyboard.BounceKeysEnabled = strdup(buf);

		free(config.XKeyboard.StickyKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbStickyKeysMask) ? _true : _false);
		config.XKeyboard.StickyKeysEnabled = strdup(buf);

		free(config.XKeyboard.MouseKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbMouseKeysMask) ? _true : _false);
		config.XKeyboard.MouseKeysEnabled = strdup(buf);

		free(config.XKeyboard.MouseKeysAccelEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbMouseKeysAccelMask) ? _true : _false);
		config.XKeyboard.MouseKeysAccelEnabled = strdup(buf);

		free(config.XKeyboard.AccessXKeysEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbAccessXKeysMask) ? _true : _false);
		config.XKeyboard.AccessXKeysEnabled = strdup(buf);

		free(config.XKeyboard.AccessXTimeoutMaskEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask) ? _true : _false);
		config.XKeyboard.AccessXTimeoutMaskEnabled = strdup(buf);

		free(config.XKeyboard.AccessXFeedbackMaskEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbAccessXFeedbackMask) ? _true : _false);
		config.XKeyboard.AccessXFeedbackMaskEnabled = strdup(buf);

		free(config.XKeyboard.AudibleBellMaskEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbAudibleBellMask) ? _true : _false);
		config.XKeyboard.AudibleBellMaskEnabled = strdup(buf);

		free(config.XKeyboard.Overlay1MaskEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbOverlay1Mask) ? _true : _false);
		config.XKeyboard.Overlay1MaskEnabled = strdup(buf);

		free(config.XKeyboard.Overlay2MaskEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbOverlay2Mask) ? _true : _false);
		config.XKeyboard.Overlay2MaskEnabled = strdup(buf);

		free(config.XKeyboard.IgnoreGroupLockModsEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbIgnoreGroupLockMask) ? _true : _false);
		config.XKeyboard.IgnoreGroupLockModsEnabled = strdup(buf);

		free(config.XKeyboard.GroupsWrapEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbGroupsWrapMask) ? _true : _false);
		config.XKeyboard.GroupsWrapEnabled = strdup(buf);

		free(config.XKeyboard.InternalModsEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbInternalModsMask) ? _true : _false);
		config.XKeyboard.InternalModsEnabled = strdup(buf);

		free(config.XKeyboard.IgnoreLockModsEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbIgnoreLockModsMask) ? _true : _false);
		config.XKeyboard.IgnoreLockModsEnabled = strdup(buf);

		free(config.XKeyboard.PerKeyRepeatEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbPerKeyRepeatMask) ? _true : _false);
		config.XKeyboard.PerKeyRepeatEnabled = strdup(buf);

		free(config.XKeyboard.ControlsEnabledEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbControlsEnabledMask) ? _true : _false);
		config.XKeyboard.ControlsEnabledEnabled = strdup(buf);

		free(config.XKeyboard.AccessXOptionsEnabled);
		strcpy(buf, (state.XKeyboard.desc->ctrls->enabled_ctrls & XkbAccessXOptionsMask) ? _true : _false);
		config.XKeyboard.AccessXOptionsEnabled = strdup(buf);

		{
			unsigned int repeat_delay, repeat_interval;

			XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &repeat_delay, &repeat_interval);

			free(config.XKeyboard.RepeatDelay);
			snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->repeat_delay);
			config.XKeyboard.RepeatDelay = strdup(buf);

			free(config.XKeyboard.RepeatInterval);
			free(config.XKeyboard.RepeatRate);
			snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->repeat_interval);
			config.XKeyboard.RepeatInterval = strdup(buf);
		}

#if 0
		{
			int slow_keys_delay;

			XkbGetSlowKeysDelay(dpy, XkbUseCoreKbd, &slow_keys_delay);

			free(config.XKeyboard.SlowKeysDelay);
			snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->slow_keys_delay);
			config.XKeyboard.SlowKeysDelay = strdup(buf);
		}

		{
			int debounce_delay;

			XkbGetBoundKeysDelay(dpy, XkbUseCoreKbd, &debounce_delay);

			free(config.XKeyboard.DebounceDelay);
			snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->debounce_delay);
			config.XKeyboard.DebounceDelay = strdup(buf);
		}
#endif

		free(config.XKeyboard.MouseKeysDelay);
		snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->mk_delay);
		config.XKeyboard.MouseKeysDelay = strdup(buf);

		free(config.XKeyboard.MouseKeysInterval);
		snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->mk_interval);
		config.XKeyboard.MouseKeysInterval = strdup(buf);

		free(config.XKeyboard.MouseKeysTimeToMax);
		snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->mk_time_to_max);
		config.XKeyboard.MouseKeysTimeToMax = strdup(buf);

		free(config.XKeyboard.MouseKeysMaxSpeed);
		snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->mk_max_speed);
		config.XKeyboard.MouseKeysMaxSpeed = strdup(buf);

		free(config.XKeyboard.MouseKeysCurve);
		snprintf(buf, sizeof(buf), "%hd", state.XKeyboard.desc->ctrls->mk_curve);
		config.XKeyboard.MouseKeysCurve = strdup(buf);

		static struct {
			unsigned short mask;
			char *name;
		} axoptions[12] = {
			{ XkbAX_SKPressFBMask, "SlowKeysPress" },
			{ XkbAX_SKAcceptFBMask, "SlowKeysAccept" },
			{ XkbAX_FeatureFBMask, "Feature" },
			{ XkbAX_SlowWarnFBMask, "SlowWarn" },
			{ XkbAX_IndicatorFBMask, "Indicator" },
			{ XkbAX_StickyKeysFBMask, "StickyKeys" },
			{ XkbAX_TwoKeysMask, "TwoKeys" },
			{ XkbAX_LatchToLockMask, "LatchToLock" },
			{ XkbAX_SKReleaseFBMask, "SlowKeysRelease" },
			{ XkbAX_SKRejectFBMask, "SlowKeysReject" },
			{ XkbAX_BKRejectFBMask, "BounceKeysReject" },
			{ XkbAX_DumbBellFBMask, "DumbBell" }
		};

		free(config.XKeyboard.AccessXOptions);
		for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
			if (state.XKeyboard.desc->ctrls->ax_options & axoptions[i].mask) {
				if (j++)
					strncat(buf, ";", sizeof(buf)-1);
				strncat(buf, axoptions[i].name, sizeof(buf)-1);
			}
		}
		config.XKeyboard.AccessXOptions = strdup(buf);

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
			int ax_timeout, axt_ctrls_mask, axt_ctrls_values, axt_opts_mask, axt_opts_values;

			XkbGetAccessXTimeout(dpy, XkbUseCoreKbd, &ax_timeout, &axt_ctrls_mask, &axt_ctrls_values, &axt_opts_mask, &axt_opts_values);
#endif

			free(config.XKeyboard.AccessXTimeout);
			snprintf(buf, sizeof(buf), "%hu", state.XKeyboard.desc->ctrls->ax_timeout);
			config.XKeyboard.AccessXTimeout = strdup(buf);

			free(config.XKeyboard.AccessXTimeoutOptionsMask);
			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_opts_mask & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf)-1);
					strncat(buf, axoptions[i].name, sizeof(buf)-1);
				}

			}
			config.XKeyboard.AccessXTimeoutOptionsMask = strdup(buf);

			free(config.XKeyboard.AccessXTimeoutOptionsValues);
			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_opts_values & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf)-1);
					strncat(buf, axoptions[i].name, sizeof(buf)-1);
				}

			}
			config.XKeyboard.AccessXTimeoutOptionsValues = strdup(buf);

			free(config.XKeyboard.AccessXTimeoutMask);
			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_ctrls_mask & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf)-1);
					strncat(buf, axoptions[i].name, sizeof(buf)-1);
				}

			}
			config.XKeyboard.AccessXTimeoutMask = strdup(buf);

			free(config.XKeyboard.AccessXTimeoutValues);
			for (*buf = '\0', j = 0, i = 0; i < 12; i++) {
				if (state.XKeyboard.desc->ctrls->axt_ctrls_values & axoptions[i].mask) {
					if (j++)
						strncat(buf, ";", sizeof(buf)-1);
					strncat(buf, axoptions[i].name, sizeof(buf)-1);
				}

			}
			config.XKeyboard.AccessXTimeoutValues = strdup(buf);
		}
	}
}

void
startup()
{
	Window window = None;

	if (XScreenSaverQueryExtension(dpy, &state.ScreenSaver.event, &state.ScreenSaver.error)) {
		support.ScreenSaver = True;
		XScreenSaverQueryVersion(dpy, &state.ScreenSaver.major_version,
				&state.ScreenSaver.minor_version);
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
			fprintf(stderr, "\tidle: %lu milliseconds\n",
					state.ScreenSaver.info.idle);
			fputs("\tevent-mask: ", stderr);
			if (state.ScreenSaver.info.eventMask & ScreenSaverNotifyMask)
				fputs("NotifyMask ", stderr);
			if (state.ScreenSaver.info.eventMask & ScreenSaverCycleMask)
				fputs("CycleMask", stderr);
			fputs("\n", stderr);
		}
	}

	if (XkbQueryExtension(dpy, &state.XKeyboard.opcode, &state.XKeyboard.event,
			&state.XKeyboard.error, &state.XKeyboard.major_version,
			&state.XKeyboard.minor_version)) {
		support.XKeyboard = True;
	}
	if (DPMSGetVersion(dpy, &state.DPMS.major_version, &state.DPMS.minor_version)) {
	}
}

gchar *
format_value_milliseconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g ms", gtk_scale_get_digits(scale), value);
}

gchar *
format_value_seconds(GtkScale *scale, gdouble value, gpointer user_data)
{
	return g_strdup_printf("%0.*g s", gtk_scale_get_digits(scale), value);
}

void
accel_numerator_value_changed(GtkRange *range, gpointer user_data)
{
}

void
accel_denominator_value_changed(GtkRange *range, gpointer user_data)
{
}

void
threshold_value_changed(GtkRange *range, gpointer user_data)
{
}

void
keyclick_percent_value_changed(GtkRange *range, gpointer user_data)
{
}

void
bell_percent_value_changed(GtkRange *range, gpointer user_data)
{
}

void
bell_pitch_value_changed(GtkRange *range, gpointer user_data)
{
}

void
bell_duration_value_changed(GtkRange *range, gpointer user_data)
{
}

void
repeat_delay_value_changed(GtkRange *range, gpointer user_data)
{
}

void
repeat_interval_value_changed(GtkRange *range, gpointer user_data)
{
}

void
slow_keys_delay_value_changed(GtkRange *range, gpointer user_data)
{
}

void
debounce_delay_value_changed(GtkRange *range, gpointer user_data)
{
}

void
mouse_keys_delay_value_changed(GtkRange *range, gpointer user_data)
{
}

void
mouse_keys_interval_value_changed(GtkRange *range, gpointer user_data)
{
}

void
mouse_keys_time_to_max_value_changed(GtkRange *range, gpointer user_data)
{
}

void
mouse_keys_max_speed_value_changed(GtkRange *range, gpointer user_data)
{
}

void
mouse_keys_curve_value_changed(GtkRange *range, gpointer user_data)
{
}

void
screensaver_timeout_value_changed(GtkRange *range, gpointer user_data)
{
}

void
rotate_screensaver_value_changed(GtkRange *range, gpointer user_data)
{
}

void
standby_timeout_value_changed(GtkRange *range, gpointer user_data)
{
}

void
suspend_timeout_value_changed(GtkRange *range, gpointer user_data)
{
}

void
off_timeout_value_changed(GtkRange *range, gpointer user_data)
{
}

void
global_autorepeat_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
repeat_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
slow_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
bounce_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
sticky_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
mouse_keys_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
mouse_keys_accel_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
prefer_blanking_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
allow_exposures_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
dpms_toggled(GtkToggleButton *button, gpointer user_data)
{
}

void
ring_bell_clicked(GtkButton *button, gpointer user_data)
{
}

void
activate_screensaver_clicked(GtkButton *button, gpointer user_data)
{
}

void
rotate_screensaver_clicked(GtkButton *button, gpointer user_data)
{
}

void
activate_standby_clicked(GtkButton *button, gpointer user_data)
{
}

void
activate_suspend_clicked(GtkButton *button, gpointer user_data)
{
}

void
activate_off_clicked(GtkButton *button, gpointer user_data)
{
}

void
default_mouse_button_changed(GtkComboBox *box, gpointer user_data)
{
}



GtkWindow *
create_window()
{
	GtkWindow *w;

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

	GtkWidget *h = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(w), h);

	GtkWidget *n = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(h), n, FALSE, FALSE, 0);

	GtkWidget *v = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(v), 5);

	GtkWidget *l = gtk_label_new("Pointer");
	gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

	GtkWidget *f = gtk_frame_new("Acceleration Numerator");
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

	v = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(v), 5);
	l = gtk_label_new("Keyboard");
	gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);

	f = gtk_frame_new(NULL);
	gtk_box_pack_start(GTK_BOX(v), f, FALSE, FALSE, 0);
	GtkWidget *u = gtk_check_button_new_with_label("Global Auto Repeat");
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

	// FIXME: if have XKEYBOARD extension.
	v = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(v), 5);
	l = gtk_label_new("XKeyboard");
	gtk_notebook_append_page(GTK_NOTEBOOK(n), v, l);
	GtkWidget *s = gtk_notebook_new();
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
	GtkWidget *q = gtk_hbox_new(FALSE, 0);
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
	g_signal_connect(G_OBJECT(h), "value-changed", G_CALLBACK(rotate_screensaver_value_changed), NULL);
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
	controls.ScreenSaver.PreferBlank = u;

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
	controls.ScreenSaver.AllowExpose = u;

	// FIXME: only when DPMS extension supported

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
set_defaults(void)
{
	char *file;
	const char *s;

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
