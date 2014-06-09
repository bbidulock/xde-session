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

#include "xde-xlogin.h"

#include <X11/Xdmcp.h>

typedef struct {
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	char *banner;
	char *welcome;
	int debug;
	int verbose;
} Options;

Options options = {
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet,
	.banner = NULL,		/* /usr/lib/X11/xde/banner.png */
	.welcome = NULL,
	.debug = 0,
	.verbose = 1,
};

typedef enum {
	BackgroundStyleStretch,
	BackgroundStyleTile,
} BackgroundStyle;

typedef struct {
	struct {
		GdkColor color;
		char *fontname;
		XftFont *font;
		struct {
			int xoffset;
			int yoffset;
			GdkColor color;
		} shadow;
		int x;
		int y;
	} msg;
	BackgroundStyle background_style;
	struct {
		struct {
			int x;
			int y;
		} panel, name, pass;
		char *fontname;
		XftFont *font;
		GdkColor color, bgcolor, fgcolor;
	} input;
	struct {
		char *msg;
		char *fontname;
		XftFont *font;
		GdkColor color;
		struct {
			int xoffset;
			int yoffset;
			GdkColor color;
		} shadow;
		int x;
		int y;
	} username, password, welcome;

} Theme;

Theme theme = {
	.msg = {
		.color = {255 * 257, 255 * 257, 255 * 257, 0},
		.fontname = "Verdana:size=16:bold",
		.font = NULL,
		.shadow = {
			   .xoffset = 2,
			   .yoffset = 2,
			   .color = {
				     .red = 0xaa * 257,
				     .green = 0xaa * 257,
				     .blue = 0xaa * 257,
				     .pixel = 0,
				     },
			   },
		.x = 50,	/* 37% */
		.y = 450,	/* 450 */
		},
	.background_style = BackgroundStyleStretch,
	.input = {
		  .panel = {50, 0},
		  .name = {203, 302},
		  .pass = {203, 348},
		  .fontname = "Verdana:size=12",
		  .font = NULL,
		  .color = {
			    .red = 0x00 * 257,
			    .green = 0x04 * 257,
			    .blue = 0x04 * 257,
			    .pixel = 0,
			    },
		  .bgcolor = {
			      .red = 0xff * 257,
			      .green = 0xff * 257,
			      .blue = 0xff * 257,
			      .pixel = 0,
			      },
		  .fgcolor = {
			      .red = 0x00 * 257,
			      .green = 0x00 * 257,
			      .blue = 0x00 * 257,
			      .pixel = 0,
			      },
		  },
	.username = {
		     .msg = "username:",
		     .fontname = "Verdana:size=16:bold",
		     .font = NULL,
		     .color = {
			       .red = 0xf5 * 257,
			       .green = 0xf4 * 257,
			       .blue = 0xfd * 257,
			       .pixel = 0,
			       },
		     .shadow = {
				.xoffset = 0,
				.yoffset = 0,
				.color = {
					  .red = 0x00 * 257,
					  .green = 0x00 * 257,
					  .blue = 0x00 * 257,
					  .pixel = 0,
					  },
				},
		     .x = 50,
		     .y = 303,
		     },
	.password = {
		     .msg = "password:",
		     .fontname = "Verdana:size=16:bold",
		     .font = NULL,
		     .color = {
			       .red = 0xf5 * 257,
			       .green = 0xf4 * 257,
			       .blue = 0xfd * 257,
			       .pixel = 0,
			       },
		     .shadow = {
				.xoffset = 0,
				.yoffset = 0,
				.color = {
					  .red = 0x00 * 257,
					  .green = 0x00 * 257,
					  .blue = 0x00 * 257,
					  .pixel = 0,
					  },
				},
		     .x = 50,
		     .y = 350,
		     },
	.welcome = {
		    .msg = "root: root, uexicon: unexicon",
		    .fontname = "Verdana:size=16:bold",
		    .font = NULL,
		    .color = {
			      .red = 0xff * 257,
			      .green = 0xff * 257,
			      .blue = 0xff * 257,
			      .pixel = 0,
			      },
		    .shadow = {
			       .xoffset = 2,
			       .yoffset = 2,
			       .color = {
					 .red = 0xaa * 257,
					 .green = 0xaa * 257,
					 .blue = 0xaa * 257,
					 .pixel = 0,
					 },
			       },
		    .x = 50,	/* 37% */
		    .y = 400,
		    },
};

void
chomp(char *s)
{
	char *p;

	for (p = s + strlen(s); p > s && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n'); p--) ;
	*p = '\0';
}

void
read_file(char *filename)
{
	FILE *f;
	char buffer[BUFSIZ + 1], *p, *q;
	unsigned long num;

	if (!(f = fopen(filename, "r"))) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return;
	}
	while ((fgets(buffer, BUFSIZ, f))) {
		p = buffer + strspn(buffer, " \t");
		if (*p == '#')
			continue;
		if (!strncmp(p, "msg_", 4)) {
			p += 4;
			if (!strncmp(p, "color", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.msg.color))
					gdk_colormap_alloc_color(colormap, &theme.msg.color, FALSE, TRUE);
			} else if (!strncmp(p, "font", 4)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.msg.fontname = strdup(p);
			} else if (!strncmp(p, "x", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenWidth(dpy, screen);
					num /= 100;
				}
				theme.msg.x = num;
			} else if (!strncmp(p, "y", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenHeight(dpy, screen);
					num /= 100;
				}
				theme.msg.y = num;
			} else if (!strncmp(p, "shadow_", 7)) {
				p += 7;
				if (!strncmp(p, "xoffset", 7)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					theme.msg.shadow.xoffset = stroul(p, NULL, 0);
				} else if (!strncmp(p, "yoffset", 7)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					theme.msg.shadow.yoffset = stroul(p, NULL, 0);
				} else if (!strncmp(p, "color", 5)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if (gdk_color_parse(p, &theme.msg.shadow.color))
						gdk_colormap_alloc_color(colormap, &theme.msg.shadow.color, FALSE, TRUE);
				}
			} else
				goto unrecognized;
		} else if (!strncmp(p, "background_", 11)) {
			p += 11;
			if (!strncmp(p, "style", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '\n')))
					*q = '\0';
				if (!strcmp(p, "stretch"))
					theme.background.style = BackgroundStyleStretch;
				else if (!strcmp(p, "tile"))
					theme.background.style = BackgroundStyleTile;
			} else
				goto unrecognized;
		} else if (!strncmp(p, "input_", 6)) {
			p += 6;
			if (!strncmp(p, "panel_", 6)) {
				p += 6;
				if (!strncmp(p, "x", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenWidth(dpy, screen);
						num /= 100;
					}
					theme.input.panel.x = num;
				} else if (!strncmp(p, "y", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenHeight(dpy, screen);
						num /= 100;
					}
					theme.input.panel.y = num;
				} else
					goto unrecognized;
			} else if (!strncmp(p, "name_", 5)) {
				p += 5;
				if (!strncmp(p, "x", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenWidth(dpy, screen);
						num /= 100;
					}
					theme.input.name.x = num;
				} else if (!strncmp(p, "y", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenHeight(dpy, screen);
						num /= 100;
					}
					theme.input.name.y = num;
				} else
					goto unrecognized;
			} else if (!strncmp(p, "pass_", 5)) {
				p += 5;
				if (!strncmp(p, "x", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenWidth(dpy, screen);
						num /= 100;
					}
					theme.input.pass.x = num;
				} else if (!strncmp(p, "y", 1)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					num = stroul(p, NULL, 0);
					if (q) {
						num *= ScreenHeight(dpy, screen);
						num /= 100;
					}
					theme.input.pass.y = num;
				} else
					goto unrecognized;
			} else if (!strncmp(p, "font", 4)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.input.fontname = strdup(p);
			} else if (!strncmp(p, "color", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.input.color))
					gdk_colormap_alloc_color(colormap, &theme.input.color, FALSE, TRUE);
			} else if (!strncmp(p, "bgcolor", 7)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.input.bgcolor))
					gdk_colormap_alloc_color(colormap, &theme.input.bgcolor, FALSE, TRUE);
			} else if (!strncmp(p, "ggcolor", 7)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.input.fgcolor))
					gdk_colormap_alloc_color(colormap, &theme.input.fgcolor, FALSE, TRUE);
			} else
				goto unrecognized;
		} else if (!strncmp(p, "username_", 9)) {
			p += 9;
			if (!strncmp(p, "msg", 3)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.username.msg = strdup(p);
			} else if (!strncmp(p, "font", 4)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.username.fontname = strdup(p);
			} else if (!strncmp(p, "color", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.username.color))
					gdk_colormap_alloc_color(colormap, &theme.username.color, FALSE, TRUE);
			} else if (!strncmp(p, "x", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenWidth(dpy, screen);
					num /= 100;
				}
				theme.username.x = num;
			} else if (!strncmp(p, "y", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenHeight(dpy, screen);
					num /= 100;
				}
				theme.username.y = num;
			} else
				goto unrecognized;
		} else if (!strncmp(p, "password_", 9)) {
			p += 9;
			if (!strncmp(p, "msg", 3)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.password.msg = strdup(p);
			} else if (!strncmp(p, "font", 4)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.password.fontname = strdup(p);
			} else if (!strncmp(p, "color", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.password.color))
					gdk_colormap_alloc_color(colormap, &theme.password.color, FALSE, TRUE);
			} else if (!strncmp(p, "x", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenWidth(dpy, screen);
					num /= 100;
				}
				theme.password.x = num;
			} else if (!strncmp(p, "y", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenHeight(dpy, screen);
					num /= 100;
				}
				theme.password.y = num;
			} else
				goto unrecognized;
		} else if (!strncmp(p, "welcome_", 8)) {
			p += 8;
			if (!strncmp(p, "msg", 3)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.welcome.msg = strdup(p);
			} else if (!strncmp(p, "font", 4)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				chomp(p);
				theme.welcome.fontname = strdup(p);
			} else if (!strncmp(p, "color", 5)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if (gdk_color_parse(p, &theme.welcome.color))
					gdk_colormap_alloc_color(colormap, &theme.welcome.color, FALSE, TRUE);
			} else if (!strncmp(p, "x", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenWidth(dpy, screen);
					num /= 100;
				}
				theme.welcome.x = num;
			} else if (!strncmp(p, "y", 1)) {
				if (!strspn(p, " \t"))
					goto unrecognized;
				p += strspn(p, " \t");
				if ((q = strrchr(p, '%')))
					*q = '\0';
				num = stroul(p, NULL, 0);
				if (q) {
					num *= ScreenHeight(dpy, screen);
					num /= 100;
				}
				theme.welcome.y = num;
			} else if (!strncmp(p, "shadow_", 7)) {
				p += 7;
				if (!strncmp(p, "xoffset", 7)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					theme.welcome.shadow.xoffset = stroul(p, NULL, 0);
				} else if (!strncmp(p, "yoffset", 7)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if ((q = strrchr(p, '%')))
						*q = '\0';
					theme.welcome.shadow.yoffset = stroul(p, NULL, 0);
				} else if (!strncmp(p, "color", 5)) {
					if (!strspn(p, " \t"))
						goto unrecognized;
					p += strspn(p, " \t");
					if (gdk_color_parse(p, &theme.welcome.shadow.color))
						gdk_colormap_alloc_color(colormap, &theme.welcome.shadow.color, FALSE, TRUE);
				}
			} else
				goto unrecognized;
		} else {
		      unrecognized:
			fprintf(stderr, "unrecognized directive: %s\n", p);
			continue;
		}

	}
	fclose(f);
}

Bool
HexToARRAY8(ARRAY8 * array, char *hex)
{
	short len;
	CARD8 *o, b;
	char *p, c;

	len = strlen(hex);
	if (len & 0x01)
		return False;
	len >>= 1;
	array->length = len;
	array->data = calloc(len, sizeof(CARD8));
	for (p = hex, o = array->data; *p; p += 2, o++) {
		c = tolower(p[0]);
		if (!ishexdigit(c))
			return False;
		b = ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		b <<= 4;
		c = tolower(p[1]);
		if (!ishexdigit(c))
			return False;
		b += ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
		*o = b;
	}
	return True;
}

int
main(int argc, char *argv[])
{
	while (1) {
		int c, val;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"xdmAddress",	    required_argument,	NULL, 'x'},
			{"clientAddress",   required_argument,	NULL, 'c'},
			{"connectionType",  required_argmuent,	NULL, 't'},
			{"banner",	    required_argument,	NULL, 'b'},
			{"welcome",	    required_argument,	NULL, 'w'},

			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "x:c:t:b:w:D::v::hVCH?", long_options, &option_index);
#else
		c = getopt(argc, argv, "x:c:t:b:w:DvhVC?");
#endif
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done option processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;

		case 'x':	/* -xdmAddress HEXBYTES */
			if (options.xdmAddress.length)
				goto bad_option;
			if (!HexToARRAY8(&options.xdmAddress, optarg))
				goto bad_option;
			break;
		case 'c':	/* -clientAddress HEXBYTES */
			if (options.clientAddress.length)
				goto bad_option;
			if (!HexToARRAY8(&options.clientAddress, optarg))
				goto bad_option;
			break;
		case 't':	/* -connectionType TYPE */
			if (!strcmp(optarg, "FamilyInternet") || atoi(optarg) == FamilyInternet)
				options.connectionType = FamilyInternet;
			else if (!strmcp(optarg, "FamilyInternet6") || atoi(optarg) == FamilyInternet6)
				options.connectionType = FamilyIntetnet6;
			else
				goto bad_option;
			break;
		case 'b':	/* -b, --banner BANNER */
			free(options.banner);
			options.banner = strndup(optarg, 256);
			break;
		case 'w':	/* -w, --welcome WELCOME */
			free(options.welcome);
			options.welcome = strndup(optarg, 256);
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
			if (options.debug)
				fprintf(stderr, "%s: printing help message\n", argv[0]);
			help(argc, argv);
			exit(OBEYSESS_DISPLAY);
		case 'V':	/* -V, --version */
			if (options.debug)
				fprintf(stderr, "%s: printing version message\n", argv[0]);
			version(argc, argv);
			exit(OBEYSESS_DISPLAY);
		case 'C':	/* -C, --copying */
			if (options.debug)
				fprintf(stderr, "%s: printing copying message\n", argv[0]);
			copying(argc, argv);
			exit(OBEYSESS_DISPLAY);
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
			exit(REMANAGE_DISPLAY);
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (optind >= argc) {
		fprintf(stderr, "%s: missing non-option argument\n", argv[0]);
		usage(argc, argv);
		exit(REMANAGE_DISPLAY);
	}

	top = GetWindow();
	InitXDMCP(&argv[optind], argc - optind);
	gtk_main();
	exit(REMANAGE_DISPLAY);
}
