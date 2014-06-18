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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include "xde-xchooser.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <X11/Xdmcp.h>
#include <X11/Xauth.h>

#ifdef _GNU_SOURCE
#include <getopt.h>
#endif

struct Options {
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	char *banner;
	char *welcome;
	int debug;
	int output;
};

struct Options options = {
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet,
	.banner = NULL,		/* "/usr/lib/X11/xdm/banner.png" */
	.welcome = NULL,
	.debug = 0,
	.output = 1,
};

GtkListStore *model;
GtkWidget *view;
GtkWindow *top;

#define PING_TRIES	3
#define PING_INTERVAL	2	/* 2 seconds */

XdmcpBuffer directBuffer;
XdmcpBuffer broadcastBuffer;

enum {
	OBEYSESS_DISPLAY = 0,		/* obey multipleSessions resources */
	REMANAGE_DISPLAY = 1,	/* force remanage */
	UNMANAGE_DISPLAY = 2,	/* force deletion */
	RESERVER_DISPLAY = 3,	/* force server termination */
	OPENFAILED_DISPLAY = 4,	/* XOpenDisplay failed, retry */
};

enum {
	COLUMN_HOSTNAME,
	COLUMN_REMOTENAME,
	COLUMN_WILLING,
	COLUMN_STATUS,
	COLUMN_IPADDR,
	COLUMN_CTYPE,
	COLUMN_SERVICE,
	COLUMN_PORT,
	COLUMN_MARKUP,
	COLUMN_TOOLTIP,
};

Bool
AddHost(struct sockaddr *sa, xdmOpCode opc, ARRAY8 * authname_a, ARRAY8 * hostname_a, ARRAY8 * status_a)
{
	int ctype;
	sa_family_t family;
	short port;
	char remotename[NI_MAXHOST + 1] = { 0, };
	char service[NI_MAXSERV + 1] = { 0, };
	char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };
	char hostname[NI_MAXHOST + 1] = { 0, };
	socklen_t len;

	len = hostname_a->length;
	if (len > NI_MAXHOST)
		len = NI_MAXHOST;
	strncpy(hostname, (char *) hostname_a->data, len);

	switch ((family = sa->sa_family)) {
	case AF_INET:
	{
		struct sockaddr_in *sin = (typeof(sin)) sa;

		ctype = FamilyInternet;
		port = sin->sin_port;
		inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6 = (typeof(sin6)) sa;

		ctype = FamilyInternet6;
		port = sin6->sin6_port;
		inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
		break;
	}
	case AF_UNIX:
	{
		struct sockaddr_un *sun = (typeof(sun)) sa;

		ctype = FamilyLocal;
		port = 0;
		break;
	}
	default:
		return False;
	}
	if (options.connectionType != ctype)
		return False;
	if (getnameinfo(sa, len, remotename, NI_MAXHOST, service, NI_MAXSERV, NI_DGRAM) == -1)
		return False;

	GtkTreeIter iter;
	gboolean valid;

	for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, 0); valid; valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
		GValue addr_v = G_VALUE_INIT;
		GValue name_v = G_VALUE_INIT;
		const gchar *addr;
		const gchar *name;

		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_IPADDR, &addr_v);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, COLUMN_HOSTNAME, &name_v);
		addr = g_value_get_string(&addr_v);
		name = g_value_get_string(&name_v);
		if (!strcmp(addr, ipaddr) && !strcmp(name, hostname)) {
			g_value_unset(&addr_v);
			g_value_unset(&name_v);
			break;
		}
		g_value_unset(&addr_v);
		g_value_unset(&name_v);
	}
	if (!valid)
		gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			   COLUMN_IPADDR, ipaddr,
			   COLUMN_HOSTNAME, hostname,
			   COLUMN_REMOTENAME, remotename,
			   COLUMN_WILLING, opc,
			   COLUMN_STATUS, 0,
			   COLUMN_CTYPE, ctype,
			   COLUMN_SERVICE, service,
			   COLUMN_PORT, port, -1);

	const char *conntype;

	switch (ctype) {
	case FamilyLocal:
		conntype = "UNIX Domain";
		break;
	case FamilyInternet:
		conntype = "TCP (IP Version 4)";
		break;
	case FamilyInternet6:
		conntype = "TCP (IP Version 6)";
		break;
	default:
		conntype = "";
		break;
	}

	(void) conntype;
	return True;

}

gboolean
ReceivePacket(GIOChannel * source, GIOCondition condition, gpointer data)
{
	XdmcpBuffer *buffer = (XdmcpBuffer *) data;
	XdmcpHeader header;
	ARRAY8 authenticationName = { 0, NULL };
	ARRAY8 hostname = { 0, NULL };
	ARRAY8 status = { 0, NULL };
	int saveHostname = 0;
	struct sockaddr_storage addr;
	int addrlen, sfd;

	sfd = g_io_channel_unix_get_fd(source);
	addrlen = sizeof(addr);
	do {
		if (!XdmcpFill(sfd, buffer, (XdmcpNetaddr) & addr, &addrlen))
			break;
		if (!XdmcpReadHeader(buffer, &header))
			break;
		if (header.version != XDM_PROTOCOL_VERSION)
			break;
		switch (header.opcode) {
		case WILLING:
			if (XdmcpReadARRAY8(buffer, &authenticationName) && XdmcpReadARRAY8(buffer, &hostname) && XdmcpReadARRAY8(buffer, &status)) {
				if (header.length == 6 + authenticationName.length + hostname.length + status.length) {
					if (AddHost((struct sockaddr *) &addr, header.opcode, &authenticationName, &hostname, &status))
						saveHostname = 1;
				}
			}
			break;
		case UNWILLING:
			if (XdmcpReadARRAY8(buffer, &hostname) && XdmcpReadARRAY8(buffer, &status)) {
				if (header.length == 4 + hostname.length + status.length) {
					if (AddHost((struct sockaddr *) &addr, header.opcode, &authenticationName, &hostname, &status))
						saveHostname = 1;
				}
			}
			break;
		default:
			break;
		}
		if (!saveHostname) {
			XdmcpDisposeARRAY8(&authenticationName);
			XdmcpDisposeARRAY8(&hostname);
			XdmcpDisposeARRAY8(&status);
		}
	} while (0);
	return TRUE;
}

typedef struct _hostAddr {
	struct _hostAddr *next;
	struct sockaddr_storage addr;
	int addrlen;
	int sfd;
	xdmOpCode type;
} HostAddr;

HostAddr *hostAddrdb;
int pingTry = 0;
gint pingid = 0;

gboolean
PingHosts(gpointer data)
{
	HostAddr *ha;

	if (options.debug)
		fprintf(stderr, "PingHost...\n");
	for (ha = hostAddrdb; ha; ha = ha->next) {
		int sfd;
		struct sockaddr *addr;
		sa_family_t family;
		char buf[INET6_ADDRSTRLEN];

		(void) buf;
		if (!(sfd = ha->sfd))
			continue;
		addr = (typeof(addr)) &ha->addr;
		family = addr->sa_family;
		switch (family) {
		case AF_INET:
			break;
		case AF_INET6:
			break;
		}
		if (ha->type == QUERY)
			XdmcpFlush(ha->sfd, &directBuffer, (XdmcpNetaddr) & ha->addr, ha->addrlen);
		else
			XdmcpFlush(ha->sfd, &broadcastBuffer, (XdmcpNetaddr) & ha->addr, ha->addrlen);
	}
	if (++pingTry < PING_TRIES)
		pingid = g_timeout_add_seconds(PING_INTERVAL, PingHosts, (gpointer) NULL);
	return TRUE;
}

gint srce4, srce6;

Bool
InitXDMCP(char *argv[], int argc)
{
	int sock4, sock6, value;
	GIOChannel *chan4, *chan6;
	XdmcpBuffer *buffer4, *buffer6;
	int i;
	char **arg;

	if (options.debug)
		fprintf(stderr, "InitXDMCP...\n");
	if ((sock4 = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "socket: Could not create IPv4 socket: %s\n", strerror(errno));
		return False;
	}
	value = 1;
	if (setsockopt(sock4, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		fprintf(stderr, "setsockopt: Could not set IPv4 broadcast: %s\n", strerror(errno));
	}
	if ((sock6 = socket(PF_INET6, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "socket: Could not create IPv6 socket: %s\n", strerror(errno));
	}
	if (setsockopt(sock6, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		fprintf(stderr, "setsockopt: Could not set IPv6 broadcast: %s\n", strerror(errno));
	}
	chan4 = g_io_channel_unix_new(sock4);
	chan6 = g_io_channel_unix_new(sock6);

	buffer4 = calloc(1, sizeof(*buffer4));
	buffer6 = calloc(1, sizeof(*buffer6));

	srce4 = g_io_add_watch(chan4, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI, ReceivePacket, (gpointer) buffer4);
	srce6 = g_io_add_watch(chan6, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI, ReceivePacket, (gpointer) buffer6);

	for (i = 0, arg = argv; i < argc; i++, arg++) {
		if (!strcmp(*arg, "BROADCAST")) {
			struct ifaddrs *ifa, *ifas = NULL;
			HostAddr *ha;

			if (getifaddrs(&ifas) == 0) {
				for (ifa = ifas; ifa; ifa = ifa->ifa_next) {
					sa_family_t family;
					socklen_t addrlen;

					if (ifa->ifa_flags & IFF_LOOPBACK)
						continue;
					if (!(ifa->ifa_flags & IFF_BROADCAST))
						continue;
					family = ifa->ifa_broadaddr->sa_family;
					if (family == AF_INET)
						addrlen = sizeof(struct sockaddr_in);
					else if (family == AF_INET6)
						addrlen = sizeof(struct sockaddr_in6);
					else
						continue;
					ha = calloc(1, sizeof(*ha));
					memcpy(&ha->addr, ifa->ifa_broadaddr, addrlen);
					ha->addrlen = addrlen;
					ha->sfd = (family == AF_INET) ? sock4 : sock6;
					ha->type = BROADCAST_QUERY;
					ha->next = hostAddrdb;
					hostAddrdb = ha;
				}
				freeifaddrs(ifas);
			}
		} else if (strspn(*arg, "0123456789abcdefABCDEF") == strlen(*arg) && strlen(*arg) == 8) {
			char addr[4];
			char *p, *o, c, b;
			Bool ok = True;

			for (p = *arg, o = addr; *p; p += 2, o++) {
				c = tolower(p[0]);
				if (!isxdigit(c)) {
					ok = False;
					break;
				}
				b = ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
				b <<= 4;
				c = tolower(p[1]);
				if (!isxdigit(c)) {
					ok = False;
					break;
				}
				b += ('0' <= c && c <= '9') ? c - '0' : c - 'a' + 10;
				*o = b;
			}
			if (ok) {
				HostAddr *ha;
				struct sockaddr_in *sin;

				ha = calloc(1, sizeof(*ha));
				sin = (typeof(sin)) & ha->addr;
				sin->sin_family = AF_INET;
				sin->sin_port = XDM_UDP_PORT;
				memcpy(&sin->sin_addr, addr, 4);
				ha->addrlen = sizeof(*sin);
				ha->sfd = sock4;
				ha->type = QUERY;
				ha->next = hostAddrdb;
				hostAddrdb = ha;
			}
		} else {
			struct addrinfo hints, *result, *ai;

			hints.ai_flags = AI_ADDRCONFIG;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			hints.ai_addrlen = 0;
			hints.ai_addr = NULL;
			hints.ai_canonname = NULL;
			hints.ai_next = NULL;

			if (getaddrinfo(*arg, "xdmcp", &hints, &result) == 0) {
				HostAddr *ha;

				for (ai = result; ai; ai = ai->ai_next) {
					if (ai->ai_family == AF_INET) {
						ha = calloc(1, sizeof(*ha));
						memcpy(&ha->addr, ai->ai_addr, ai->ai_addrlen);
						ha->addrlen = ai->ai_addrlen;
						ha->sfd = sock4;
						ha->type = QUERY;
						ha->next = hostAddrdb;
						hostAddrdb = ha;
					} else if (ai->ai_family == AF_INET6) {
						ha = calloc(1, sizeof(*ha));
						memcpy(&ha->addr, ai->ai_addr, ai->ai_addrlen);
						ha->addrlen = ai->ai_addrlen;
						ha->sfd = sock6;
						ha->type = QUERY;
						ha->next = hostAddrdb;
						hostAddrdb = ha;
					}
				}
				freeaddrinfo(result);
			}
		}
	}
	pingTry = 0;
	PingHosts((gpointer) NULL);
	return True;
}

gboolean
on_msg_timeout(gpointer data)
{
	gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_NONE);
	return FALSE;
}

void
Choose(short type, char *name)
{
	CARD16 connectionType = htons(type);
	int status;

	ARRAY8 hostAddress = {
		htons((short) strlen(name)),
		(CARD8 *) name
	};

	if (options.xdmAddress.data) {
		struct sockaddr_in in_addr;
		struct sockaddr_in6 in6_addr;
		struct sockaddr *addr = NULL;
		int family, len = 0, fd;
		char buf[1024];
		XdmcpBuffer buffer;
		char *xdm;

		/* 
		 * Connect to XDM and output result
		 */
		xdm = (char *) options.xdmAddress.data;
		family = ((int) xdm[0] << 8) + xdm[1];
		switch (family) {
		case AF_INET:
			in_addr.sin_family = family;
			memmove(&in_addr.sin_port, xdm + 2, 2);
			memmove(&in_addr.sin_addr, xdm + 4, 4);
			addr = (struct sockaddr *) &in_addr;
			len = sizeof(in_addr);
			break;
		case AF_INET6:
			memset(&in6_addr, 0, sizeof(in6_addr));
			in6_addr.sin6_family = family;
			memmove(&in6_addr.sin6_port, xdm + 2, 2);
			memmove(&in6_addr.sin6_port, xdm + 4, 16);
			addr = (struct sockaddr *) &in6_addr;
			len = sizeof(in6_addr);
			break;
		default:
		{
			/* should not happen: we should not be displaying incompatible address
			   families */
			GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top), GTK_DIALOG_DESTROY_WITH_PARENT,
									    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
									    "<b>%s</b>\nNumber: %d\n", "Bad address family!",
									    family);
			gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

			gtk_dialog_run(GTK_DIALOG(msg));
			g_source_remove(id);
			return;
		}
		}
		if ((fd = socket(family, SOCK_STREAM, 0)) == -1) {
			fprintf(stderr, "Cannot create reponse socket: %s\n", strerror(errno));
			exit(REMANAGE_DISPLAY);
		}
		if (connect(fd, addr, len) == -1) {
			fprintf(stderr, "Cannot connect to xdm: %s\n", strerror(errno));
			exit(REMANAGE_DISPLAY);
		}
		buffer.data = (BYTE *) buf;
		buffer.size = sizeof(buf);
		buffer.pointer = 0;
		buffer.count = 0;
		XdmcpWriteARRAY8(&buffer, &options.clientAddress);
		XdmcpWriteCARD16(&buffer, connectionType);
		XdmcpWriteARRAY8(&buffer, &hostAddress);
		status = write(fd, (char *) buffer.data, buffer.pointer);
		(void) status;
		close(fd);
	} else {
		int len, i;
#if 0
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top), GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
								    "<b>%s</b>: %s\n", "Would have connected to",
								    ipAddress);
		gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_dialog_run(GTK_DIALOG(msg));
		g_source_remove(id);
#endif

		fprintf(stdout, "%u\n", (unsigned int) type);
		len = strlen(name);
		for (i = 0; i < len; i++)
			fprintf(stdout, "%u%s", (unsigned int) name[i], i == len - 1 ? "\n" : " ");
	}
	exit(OBEYSESS_DISPLAY);
}

void
DoAccept(GtkButton * button, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	GValue ctype = G_VALUE_INIT;
	GValue ipaddr = G_VALUE_INIT;
	GValue willing = G_VALUE_INIT;
	gint willingness, connectionType;
	gchar *ipAddress;

	if (!view)
		return;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top), GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n", "No selection!",
								    "Click on a list entry to make a selection.");
		gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_dialog_run(GTK_DIALOG(msg));
		g_source_remove(id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, COLUMN_WILLING, &willing);
	willingness = g_value_get_int(&willing);
	g_value_unset(&willing);

	if (willingness != WILLING) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top), GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								    "<b>%s</b>\n%d != %d\n%s\n",
								    "Host is not willing!", willingness, (int) WILLING,
								    "Please select another host.");
		gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_dialog_run(GTK_DIALOG(msg));
		g_source_remove(id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, COLUMN_CTYPE, &ctype);
	connectionType = g_value_get_int(&ctype);
	g_value_unset(&ctype);
	if (connectionType != options.connectionType) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top), GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n", "Host has wrong connection type!",
								    "Please select another host.");
		gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

		gtk_dialog_run(GTK_DIALOG(msg));
		g_source_remove(id);
		return;
	}

	gtk_tree_model_get_value(model, &iter, COLUMN_IPADDR, &ipaddr);
	ipAddress = g_value_dup_string(&ipaddr);
	g_value_unset(&ipaddr);

	Choose(connectionType, ipAddress);
}

void
DoCancel(GtkButton * button, gpointer data)
{
	exit(OBEYSESS_DISPLAY);
}

typedef struct {
	int willing;
} PingHost;

Bool
DoCheckWilling(PingHost * host)
{
	return (host->willing == WILLING);
}

void
DoPing(GtkButton * button, gpointer data)
{
	if (pingTry == PING_TRIES) {
		pingTry = 0;
		PingHosts((gpointer) NULL);
	}
}

static gboolean
on_destroy(GtkWidget *widget, gpointer user_data)
{
	return FALSE;
}

static void
on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer
		user_data)
{
}

GtkWindow *
GetWindow()
{
	GtkWidget *win, *vbox;
	GValue val = G_VALUE_INIT;
	char hostname[64] = { 0, };

	win = gtk_window_new(GTK_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(win), "xde-xchooser", "XDE-XChooser");
	gtk_window_set_title(GTK_WINDOW(win), "XDMCP Chooser");
	gtk_window_set_gravity(GTK_WINDOW(win), GDK_GRAVITY_CENTER);
	gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);
	g_value_init(&val, G_TYPE_BOOLEAN);
	g_value_set_boolean(&val, FALSE);
	g_object_set_property(G_OBJECT(win), "allow-grow", &val);
	g_object_set_property(G_OBJECT(win), "allow-shrink", &val);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), (gpointer) NULL);
	gtk_container_set_border_width(GTK_CONTAINER(win), 0);

	gethostname(hostname, sizeof(hostname));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
	{
		GtkWidget *lab = gtk_label_new(NULL);
		gchar *markup;

		if (options.welcome) {
			markup = g_markup_printf_escaped("<span font=\"Liberation Sans 14\"><b><i>%s</i></b></span>", options.welcome);
		} else {
			markup = g_markup_printf_escaped("<span font=\"Liberation Sans 14\"><b><i>Welcome to %s!</i></b></span>", hostname);
		}
		gtk_label_set_markup(GTK_LABEL(lab), markup);
		g_free(markup);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(lab), FALSE, TRUE, 0);
	}
	{
		GtkWidget *tab = gtk_table_new(1, 2, TRUE);

		gtk_table_set_col_spacings(GTK_TABLE(tab), 5);
		gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(tab), TRUE, TRUE, 0);
		{
			GtkWidget *bin = gtk_frame_new(NULL);

			gtk_frame_set_shadow_type(GTK_FRAME(bin), GTK_SHADOW_ETCHED_IN);
			gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
			gtk_table_attach_defaults(GTK_TABLE(tab), GTK_WIDGET(bin), 0, 1, 0, 1);

			GtkWidget *pan = gtk_frame_new(NULL);

			gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
			gtk_container_set_border_width(GTK_CONTAINER(pan), 0);
			gtk_container_add(GTK_CONTAINER(bin), GTK_WIDGET(pan));
		}
		{
			GtkWidget *vbox2 = gtk_vbox_new(FALSE, 0);

			gtk_table_attach_defaults(GTK_TABLE(tab), GTK_WIDGET(vbox2), 1, 2, 0, 1);
			{
				GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

				gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
				gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(sw), TRUE, TRUE, 0);

				/* *INDENT-OFF* */
				model = gtk_list_store_new(10
						,G_TYPE_STRING	/* hostname */
						,G_TYPE_STRING	/* remotename */
						,G_TYPE_INT	/* willing */
						,G_TYPE_STRING	/* status */
						,G_TYPE_STRING	/* IP Address */
						,G_TYPE_INT	/* connection type */
						,G_TYPE_STRING	/* service */
						,G_TYPE_INT	/* port */
						,G_TYPE_STRING	/* markup */
						,G_TYPE_STRING	/* tooltip */
				    );
				/* *INDENT-ON* */
				view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
				gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
				gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COLUMN_HOSTNAME);
				gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), COLUMN_TOOLTIP);

				gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(view));

				int len = strlen("XDCMP Host Menu from ") + strlen(hostname) + 1;
				char *title = calloc(len, sizeof(*title));

				strncpy(title, "XDCMP Host Menu from ", len);
				strncat(title, hostname, len);

				GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(title, renderer, "markup", COLUMN_MARKUP, NULL);

				free(title);
				gtk_tree_view_column_set_sort_column_id(column, COLUMN_MARKUP);
				g_signal_connect(G_OBJECT(view), "row_activated", G_CALLBACK(on_row_activated), (gpointer) NULL);

				gtk_widget_set_size_request(GTK_WIDGET(sw), -1, 300);
			}
			{
				GtkWidget *bbox = gtk_vbutton_box_new();

				gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
				gtk_box_set_spacing(GTK_BOX(bbox), 5);
				gtk_box_pack_end(GTK_BOX(vbox2), GTK_WIDGET(bbox), FALSE, TRUE, 0);

				GtkWidget *button;

				button = gtk_button_new_from_stock("gtk-refresh");
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoPing), (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

				button = gtk_button_new_from_stock("gtk-cancel");
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoCancel), (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

				button = gtk_button_new_from_stock("gtk-connect");
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoAccept), (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

			}
		}
	}
	gtk_window_set_default_size(GTK_WINDOW(win), -1, 350);
	gtk_widget_show_all(GTK_WIDGET(win));
	return GTK_WINDOW(win);
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

void
help(int argc, char *argv[])
{
}

void
version(int argc, char *argv[])
{
}

void
copying(int argc, char *argv[])
{
}

void
usage(int argc, char *argv[])
{
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
			{"connectionType",  required_argument,	NULL, 't'},
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

		c = getopt_long_only(argc, argv, "x:c:b:w:D::v::hVCH?", long_options, &option_index);
#else
		c = getopt(argc, argv, "x:c:b:w:DvhVCH?");
#endif
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
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
			else if (!strcmp(optarg, "FamilyInternet6") || atoi(optarg) == FamilyInternet6)
				options.connectionType = FamilyInternet6;
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
