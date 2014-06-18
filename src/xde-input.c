#include "xde-input.h"

Display *dpy;

typedef struct {
	int debug;
} Options;

Options options;

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

int
main(int argc, char *argv[])
{
	return (0);
}
