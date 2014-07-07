/*****************************************************************************

 Copyright (c) 2008-2013  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; version 3 of the License.

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

#ifndef __XDE_XSESSION_H__
#define __XDE_XSESSION_H__

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
#include <X11/SM/SMlib.h>
#ifdef STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif
#include <gtk/gtk.h>
#include <cairo.h>
#include <glib.h>

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

typedef enum {
	LOGO_SIDE_LEFT,
	LOGO_SIDE_RIGHT,
	LOGO_SIDE_TOP,
	LOGO_SIDE_BOTTOM,
} LogoSide;

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

typedef struct {
	int output;
	int debug;
	CommandType command;
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
	char *charset;
	char *language;
	char *vendor;
	Bool splash;
	char *image;
	char *banner;
	struct {
		LogoSide splash;
		LogoSide chooser;
		LogoSide logout;
	} side;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	Bool usexde;
	Bool xinit;
        Bool managed;
	char *choice;
        char *current;
        char *session;
	struct {
		struct {
			char *text;
			char *markup;
		} chooser, logout;
	} prompt;
	struct {
		Bool session;
		Bool proxy;
		Bool dockapps;
		Bool systray;
	} manage;
	struct {
		Bool lock;
		char *program;
	} slock;
	Bool assist;
	Bool xsettings;
	Bool xinput;
	struct {
		Bool theme;
		Bool dockapps;
		Bool systray;
	} style;
} Options;

extern Options options;

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
	char *charset;
	char *language;
	char *vendor;
	char *splash;
	char *image;
	char *banner;
	struct {
		char *splash;
		char *chooser;
		char *logout;
	} side;
	char *icon_theme;
	char *gtk2_theme;
	char *curs_theme;
	char *usexde;
	char *xinit;
        char *managed;
	char *choice;
        char *current;
        char *session;
	struct {
		struct {
			char *text;
			char *markup;
		} chooser, logout;
	} prompt;
	struct {
		char *session;
		char *proxy;
		char *dockapps;
		char *systray;
	} manage;
	struct {
		char *lock;
		char *program;
	} slock;
	char *assist;
	char *xsettings;
	char *xinput;
	struct {
		char *theme;
		char *dockapps;
		char *systray;
	} style;
} Optargs;

extern Optargs optargs;
extern Optargs defaults;

typedef struct {
	struct {
		char *home;
		char *dirs;
		char *both;
	} conf, data;
} XdgDirectories;

extern XdgDirectories xdg_dirs;

#endif				/* __XDE_XSESSION_H__ */
