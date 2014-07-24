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
	int output;
	int debug;
	Bool dryrun;
	ARRAY8 xdmAddress;
	ARRAY8 clientAddress;
	CARD16 connectionType;
	char *banner;
	char *welcome;
};

struct Options options = {
	.output = 1,
	.debug = 0,
	.dryrun = False,
	.xdmAddress = {0, NULL},
	.clientAddress = {0, NULL},
	.connectionType = FamilyInternet,
	.banner = NULL,		/* "/usr/lib/X11/xdm/banner.png" */
	.welcome = NULL,
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

void
relax()
{
	while (gtk_events_pending())
		gtk_main_iteration();
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
	char *xhome, *xdata, *dirs, *pos, *end, **xdg_dirs;
	int len, n;

	xhome = "/usr/lib/X11/xdm";
	xdata = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";

	len = strlen(xhome) + 1 + strlen(xdata) + 1;
	dirs = calloc(len, sizeof(*dirs));
	strcpy(dirs, xhome);
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

Bool
AddHost(struct sockaddr *sa, xdmOpCode opc, ARRAY8 *authname_a, ARRAY8 *hostname_a,
	ARRAY8 *status_a)
{
	int ctype;
	sa_family_t family;
	short port;
	char remotename[NI_MAXHOST + 1] = { 0, };
	char service[NI_MAXSERV + 1] = { 0, };
	char ipaddr[INET6_ADDRSTRLEN + 1] = { 0, };
	char hostname[NI_MAXHOST + 1] = { 0, };
	char markup[BUFSIZ + 1] = { 0, };
	char tooltip[BUFSIZ + 1] = { 0, };
	char status[256] = { 0, };
	socklen_t len;

	DPRINT();
	len = hostname_a->length;
	if (len > NI_MAXHOST)
		len = NI_MAXHOST;
	strncpy(hostname, (char *) hostname_a->data, len);
	DPRINTF("hostname is %s\n", hostname);

	len = status_a->length;
	if (len > sizeof(status) - 1)
		len = sizeof(status) - 1;
	strncpy(status, (char *) status_a->data, len);
	DPRINTF("status is %s\n", status);

	switch ((family = sa->sa_family)) {
	case AF_INET:
	{
		struct sockaddr_in *sin = (typeof(sin)) sa;

		DPRINTF("family is AF_INET\n");
		ctype = FamilyInternet;
		port = ntohs(sin->sin_port);
		inet_ntop(AF_INET, &sin->sin_addr, ipaddr, INET_ADDRSTRLEN);
		DPRINTF("address is %s\n", ipaddr);
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6 = (typeof(sin6)) sa;

		DPRINTF("family is AF_INET6\n");
		ctype = FamilyInternet6;
		port = ntohs(sin6->sin6_port);
		inet_ntop(AF_INET6, &sin6->sin6_addr, ipaddr, INET6_ADDRSTRLEN);
		DPRINTF("address is %s\n", ipaddr);
		break;
	}
	case AF_UNIX:
	{
		struct sockaddr_un *sun = (typeof(sun)) sa;

		DPRINTF("family is AF_UNIX\n");
		ctype = FamilyLocal;
		port = 0;
		/* FIXME: display address in debug mode */
		break;
	}
	default:
		return False;
	}
	if (options.connectionType != ctype) {
		DPRINTF("wrong connection type\n");
		return False;
	}
	if (getnameinfo(sa, len, remotename, NI_MAXHOST, service, NI_MAXSERV, NI_DGRAM) == -1) {
		DPRINTF("getnameinfo: %s\n", strerror(errno));
		return False;
	}

	GtkTreeIter iter;
	gboolean valid;

	for (valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, NULL, 0); valid;
	     valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
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
			   COLUMN_HOSTNAME, hostname,
			   COLUMN_REMOTENAME, remotename,
			   COLUMN_WILLING, opc,
			   COLUMN_STATUS, status,
			   COLUMN_IPADDR, ipaddr,
			   COLUMN_CTYPE, ctype,
			   COLUMN_SERVICE, service,
			   COLUMN_PORT, port,
			   -1);

	const char *conntype;

	strncpy(markup, "", sizeof(markup));
	strncpy(tooltip, "", sizeof(tooltip));

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

	strncat(tooltip, "<small><b>Hostname:</b>\t", BUFSIZ);
	strncat(tooltip, hostname, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Alias:</b>\t\t", BUFSIZ);
	strncat(tooltip, remotename, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	if (opc == WILLING) {
		strncat(markup, "<span foreground=\"black\"><b>", BUFSIZ);
		strncat(markup, hostname, BUFSIZ);
		strncat(markup, "</b></span>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"black\">(", BUFSIZ);
		strncat(markup, remotename, BUFSIZ);
		strncat(markup, ")</span></small>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"black\"><i>", BUFSIZ);
		strncat(markup, status, BUFSIZ);
		strncat(markup, "</i></span></small>", BUFSIZ);

		strncat(tooltip, "<small><b>Willing:</b>\t\t", BUFSIZ);
		len = strlen(tooltip);
		snprintf(tooltip + len, BUFSIZ - len, "Willing(%d)", (int) opc);
		strncat(tooltip, "</small>\n", BUFSIZ);

		strncat(tooltip, "<small><b>Status:</b>\t\t", BUFSIZ);
		strncat(tooltip, status, BUFSIZ);
		strncat(tooltip, "</small>\n", BUFSIZ);
	} else {
		strncat(markup, "<span foreground=\"grey\"><b>", BUFSIZ);
		strncat(markup, hostname, BUFSIZ);
		strncat(markup, "</b></span>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"grey\">(", BUFSIZ);
		strncat(markup, remotename, BUFSIZ);
		strncat(markup, "</span></small>\n", BUFSIZ);

		strncat(markup, "<small><span foreground=\"grey\"><i>", BUFSIZ);
		strncat(markup, "Unwilling(6)", BUFSIZ);
		strncat(markup, "</i></span></small>", BUFSIZ);

		strncat(tooltip, "<small><b>Willing:</b>\t\t", BUFSIZ);
		len = strlen(tooltip);
		snprintf(tooltip + len, BUFSIZ - len, "Unwilling(%d)", (int) opc);
		strncat(tooltip, "</small>\n", BUFSIZ);
	}

	strncat(tooltip, "<small><b>IP Address:</b>\t", BUFSIZ);
	strncat(tooltip, ipaddr, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>ConnType:</b>\t", BUFSIZ);
	strncat(tooltip, conntype, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Service:</b>\t\t", BUFSIZ);
	strncat(tooltip, service, BUFSIZ);
	strncat(tooltip, "</small>\n", BUFSIZ);

	strncat(tooltip, "<small><b>Port:</b>\t\t", BUFSIZ);
	len = strlen(tooltip);
	snprintf(tooltip + len, BUFSIZ - len, "%d", (int)port);
	strncat(tooltip, "</small>", BUFSIZ);

	DPRINTF("markup is:\n%s\n", markup);
	DPRINTF("tooltip is:\n%s\n", tooltip);

	gtk_list_store_set(model, &iter,
			COLUMN_MARKUP, markup,
			COLUMN_TOOLTIP, tooltip,
			-1);

	relax();
	return True;

}

gboolean
ReceivePacket(GIOChannel *source, GIOCondition condition, gpointer data)
{
	XdmcpBuffer *buffer = (XdmcpBuffer *) data;
	XdmcpHeader header;
	ARRAY8 authenticationName = { 0, NULL };
	ARRAY8 hostname = { 0, NULL };
	ARRAY8 status = { 0, NULL };
	struct sockaddr_storage addr;
	int addrlen, sfd;

	DPRINT();
	sfd = g_io_channel_unix_get_fd(source);
	addrlen = sizeof(addr);
	if (!XdmcpFill(sfd, buffer, (XdmcpNetaddr) & addr, &addrlen)) {
		EPRINTF("could not fill buffer!\n");
		return G_SOURCE_CONTINUE;
	}
	if (!XdmcpReadHeader(buffer, &header)) {
		EPRINTF("could not read header!\n");
		return G_SOURCE_CONTINUE;
	}
	if (header.version != XDM_PROTOCOL_VERSION) {
		EPRINTF("wrong header version!\n");
		return G_SOURCE_CONTINUE;
	}
	switch (header.opcode) {
	case WILLING:
		DPRINTF("host is WILLING\n");
		if (XdmcpReadARRAY8(buffer, &authenticationName)
		    && XdmcpReadARRAY8(buffer, &hostname)
		    && XdmcpReadARRAY8(buffer, &status)) {
			if (header.length == 6 + authenticationName.length +
			    hostname.length + status.length)
				AddHost((struct sockaddr *) &addr, header.opcode,
					&authenticationName, &hostname, &status);
			else
				EPRINTF("message is the wrong length\n");
		} else
			EPRINTF("could not parse message\n");
		break;
	case UNWILLING:
		DPRINTF("host is UNWILLING\n");
		if (XdmcpReadARRAY8(buffer, &hostname) && XdmcpReadARRAY8(buffer, &status)) {
			if (header.length == 4 + hostname.length + status.length)
				AddHost((struct sockaddr *) &addr, header.opcode,
					&authenticationName, &hostname, &status);
			else
				EPRINTF("message is the wrong length\n");
		} else
			EPRINTF("could not parse message\n");
		break;
	default:
		break;
	}
	XdmcpDisposeARRAY8(&authenticationName);
	XdmcpDisposeARRAY8(&hostname);
	XdmcpDisposeARRAY8(&status);
	return G_SOURCE_CONTINUE;
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

	DPRINT();
	for (ha = hostAddrdb; ha; ha = ha->next) {
		int sfd;
		struct sockaddr *addr;
		sa_family_t family;
		char buf[INET6_ADDRSTRLEN];

		(void) buf;
		if (!(sfd = ha->sfd))
			continue;
		addr = (typeof(addr)) & ha->addr;
		family = addr->sa_family;
		switch (family) {
		case AF_INET:
			DPRINTF("ping address is AF_INET\n");
			break;
		case AF_INET6:
			DPRINTF("ping address is AF_INET6\n");
			break;
		}
		if (options.debug) {
			char *p;
			int i;

			DPRINTF("message is:");
			for (i = 0, p = (char *)&ha->addr; i < ha->addrlen; i++, p++)
				fprintf(stderr, " %02X", (unsigned int) *p);

		}
		if (ha->type == QUERY) {
			DPRINTF("ping type is QUERY\n");
			XdmcpFlush(ha->sfd, &directBuffer,
					(XdmcpNetaddr) & ha->addr, ha->addrlen);
		} else {
			DPRINTF("ping type is BROADCAST_QUERY\n");
			XdmcpFlush(ha->sfd, &broadcastBuffer,
					(XdmcpNetaddr) & ha->addr, ha->addrlen);
		}
	}
	if (++pingTry < PING_TRIES)
		pingid = g_timeout_add_seconds(PING_INTERVAL, PingHosts, (gpointer) NULL);
	return TRUE;
}

gint srce4, srce6;

static ARRAYofARRAY8 AuthenticationNames;

Bool
InitXDMCP(char *argv[], int argc)
{
	int sock4, sock6, value;
	GIOChannel *chan4, *chan6;
	XdmcpBuffer *buffer4, *buffer6;
	XdmcpHeader header;
	int i;
	char **arg;

	DPRINT();

	header.version = XDM_PROTOCOL_VERSION;
	header.opcode = (CARD16) BROADCAST_QUERY;
	header.length = 1;
	for (i = 0; i < (int)AuthenticationNames.length; i++)
		header.length += 2 + AuthenticationNames.data[i].length;
	XdmcpWriteHeader(&broadcastBuffer, &header);
	XdmcpWriteARRAYofARRAY8(&broadcastBuffer, &AuthenticationNames);

	header.version = XDM_PROTOCOL_VERSION;
	header.opcode = (CARD16) QUERY;
	header.length = 1;
	for (i = 0; i < (int)AuthenticationNames.length; i++)
		header.length += 2 + AuthenticationNames.data[i].length;
	XdmcpWriteHeader(&directBuffer, &header);
	XdmcpWriteARRAYofARRAY8(&directBuffer, &AuthenticationNames);

	if ((sock4 = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: Could not create IPv4 socket: %s\n", strerror(errno));
		return False;
	}
	value = 1;
	if (setsockopt(sock4, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		EPRINTF("setsockopt: Could not set IPv4 broadcast: %s\n", strerror(errno));
	}
	if ((sock6 = socket(PF_INET6, SOCK_DGRAM, 0)) == -1) {
		EPRINTF("socket: Could not create IPv6 socket: %s\n", strerror(errno));
	}
	if (setsockopt(sock6, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1) {
		EPRINTF("setsockopt: Could not set IPv6 broadcast: %s\n", strerror(errno));
	}
	chan4 = g_io_channel_unix_new(sock4);
	chan6 = g_io_channel_unix_new(sock6);

	buffer4 = calloc(1, sizeof(*buffer4));
	buffer6 = calloc(1, sizeof(*buffer6));

	srce4 = g_io_add_watch(chan4, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI,
			       ReceivePacket, (gpointer) buffer4);
	srce6 = g_io_add_watch(chan6, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_PRI,
			       ReceivePacket, (gpointer) buffer6);

	for (i = 0, arg = argv; i < argc; i++, arg++) {
		if (!strcmp(*arg, "BROADCAST")) {
			struct ifaddrs *ifa, *ifas = NULL;
			HostAddr *ha;

			if (getifaddrs(&ifas) == 0) {
				for (ifa = ifas; ifa; ifa = ifa->ifa_next) {
					sa_family_t family;
					socklen_t addrlen;
					struct sockaddr *ifa_addr;

					(void) index;
					if (ifa->ifa_flags & IFF_LOOPBACK) {
						DPRINTF("interface %s is a loopback interface\n",
							ifa->ifa_name);
						continue;
					}
					if (!(ifa_addr = ifa->ifa_addr)) {
						EPRINTF("interface %s has no address\n",
							ifa->ifa_name);
						continue;
					} else {
						if (ifa_addr->sa_family == AF_INET)
							DPRINTF("interface %s is AF_INET\n",
								ifa->ifa_name);
						else if (ifa_addr->sa_family == AF_INET6)
							DPRINTF("interface %s is AF_INET6\n",
								ifa->ifa_name);
						else if (ifa_addr->sa_family == AF_PACKET)
							DPRINTF("interface %s is AF_PACKET\n",
								ifa->ifa_name);
					}
					if (!(ifa->ifa_flags & IFF_BROADCAST)) {
						DPRINTF("interface %s has no broadcast\n",
							ifa->ifa_name);
						continue;
					}
					if (!(ifa_addr = ifa->ifa_broadaddr)) {
						EPRINTF("interface %s has missing broadcast\n",
							ifa->ifa_name);
						continue;
					}
					family = ifa_addr->sa_family;
					if (family == AF_INET)
						addrlen = sizeof(struct sockaddr_in);
					else if (family == AF_INET6)
						addrlen = sizeof(struct sockaddr_in6);
					else {
						DPRINTF("interface %s has wrong family %d\n",
							ifa->ifa_name, (int) family);
						continue;
					}
					DPRINTF("interace %s is ok\n", ifa->ifa_name);
					ha = calloc(1, sizeof(*ha));
					memcpy(&ha->addr, ifa_addr, addrlen);
					if (family == AF_INET) {
						struct sockaddr_in *sin;
						sin = (typeof(sin)) &ha->addr;
						sin->sin_port = htons(XDM_UDP_PORT);
					} else {
						struct sockaddr_in6 *sin6;
						sin6 = (typeof(sin6)) &ha->addr;
						sin6->sin6_port = htons(XDM_UDP_PORT);
					}
					ha->addrlen = addrlen;
					ha->sfd = (family == AF_INET) ? sock4 : sock6;
					ha->type = BROADCAST_QUERY;
					ha->next = hostAddrdb;
					hostAddrdb = ha;
				}
				freeifaddrs(ifas);
			}
		} else if (strspn(*arg, "0123456789abcdefABCDEF") == strlen(*arg)
			   && strlen(*arg) == 8) {
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
		case AF_UNIX:
			/* FIXME: why not AF_UNIX? If the Display happens to be
			 * on the local host, why not offer a AF_UNIX
			 * connection? */
		default:
		{
			/* should not happen: we should not be displaying
			   incompatible address families */
			GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
									    GTK_DIALOG_DESTROY_WITH_PARENT,
									    GTK_MESSAGE_ERROR,
									    GTK_BUTTONS_OK,
									    "<b>%s</b>\nNumber: %d\n",
									    "Bad address family!",
									    family);
			gint id = g_timeout_add_seconds(3, on_msg_timeout, (gpointer) msg);

			gtk_dialog_run(GTK_DIALOG(msg));
			g_source_remove(id);
			return;
		}
		}
		if ((fd = socket(family, SOCK_STREAM, 0)) == -1) {
			EPRINTF("Cannot create reponse socket: %s\n", strerror(errno));
			exit(REMANAGE_DISPLAY);
		}
		if (connect(fd, addr, len) == -1) {
			EPRINTF("Cannot connect to xdm: %s\n", strerror(errno));
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
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_INFO,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>: %s\n",
								    "Would have connected to",
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
DoAccept(GtkButton *button, gpointer data)
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
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n",
								    "No selection!",
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
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%d != %d\n%s\n",
								    "Host is not willing!",
								    willingness,
								    (int) WILLING,
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
		GtkWidget *msg = gtk_message_dialog_new_with_markup(GTK_WINDOW(top),
								    GTK_DIALOG_DESTROY_WITH_PARENT,
								    GTK_MESSAGE_ERROR,
								    GTK_BUTTONS_OK,
								    "<b>%s</b>\n%s\n",
								    "Host has wrong connection type!",
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
DoCancel(GtkButton *button, gpointer data)
{
	exit(OBEYSESS_DISPLAY);
}

typedef struct {
	int willing;
} PingHost;

Bool
DoCheckWilling(PingHost *host)
{
	return (host->willing == WILLING);
}

void
DoPing(GtkButton *button, gpointer data)
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

GtkWidget *user;
GtkWidget *pass;

GtkWindow *
GetWindow()
{
	GtkWidget *win, *vbox;
	GValue val = G_VALUE_INIT;
	char hostname[64] = { 0, };

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(win), "xde-xchooser", "XDE-XChooser");
	gtk_window_set_title(GTK_WINDOW(win), "XDMCP Chooser");
	gtk_window_set_gravity(GTK_WINDOW(win), GDK_GRAVITY_CENTER);
	gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ALWAYS);
	// gtk_window_set_default_size(GTK_WINDOW(win), 800, 600);
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_value_init(&val, G_TYPE_BOOLEAN);
	g_value_set_boolean(&val, FALSE);
	g_object_set_property(G_OBJECT(win), "allow-grow", &val);
	g_object_set_property(G_OBJECT(win), "allow-shrink", &val);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), (gpointer) NULL);
	gtk_container_set_border_width(GTK_CONTAINER(win), 5);

	gethostname(hostname, sizeof(hostname));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
	{
		GtkWidget *lab = gtk_label_new(NULL);
		gchar *markup;

		markup = g_markup_printf_escaped
		    ("<span font=\"Liberation Sans 12\"><b><i>%s</i></b></span>", options.welcome);
		gtk_label_set_markup(GTK_LABEL(lab), markup);
		gtk_misc_set_alignment(GTK_MISC(lab), 0.5, 0.5);
		gtk_misc_set_padding(GTK_MISC(lab), 3, 3);
		g_free(markup);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(lab), FALSE, TRUE, 0);
	}
	{
		GtkWidget *tab = gtk_table_new(1, 2, FALSE);

		gtk_table_set_col_spacings(GTK_TABLE(tab), 5);
		gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(tab), TRUE, TRUE, 0);
		{
			GtkWidget *vbox2 = gtk_vbox_new(FALSE, 0);

			gtk_table_attach_defaults(GTK_TABLE(tab), GTK_WIDGET(vbox2), 0, 1, 0, 1);

			GtkWidget *bin = gtk_frame_new(NULL);

			gtk_frame_set_shadow_type(GTK_FRAME(bin), GTK_SHADOW_ETCHED_IN);
			gtk_container_set_border_width(GTK_CONTAINER(bin), 0);
			gtk_box_pack_start(GTK_BOX(vbox2), bin, TRUE, TRUE, 4);

			GtkWidget *pan = gtk_frame_new(NULL);

			gtk_frame_set_shadow_type(GTK_FRAME(pan), GTK_SHADOW_NONE);
			gtk_container_set_border_width(GTK_CONTAINER(pan), 15);
			gtk_container_add(GTK_CONTAINER(bin), GTK_WIDGET(pan));

			GtkWidget *img = gtk_image_new_from_file(options.banner);

			gtk_container_add(GTK_CONTAINER(pan), GTK_WIDGET(img));

			GtkWidget *bbox = gtk_hbutton_box_new();

			gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
			//gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_EDGE);
			gtk_box_set_spacing(GTK_BOX(bbox), 2);
			gtk_box_pack_end(GTK_BOX(vbox2), GTK_WIDGET(bbox), FALSE, TRUE, 4);

			GtkWidget *button;

			button = gtk_option_menu_new();
			gtk_button_set_label(GTK_BUTTON(button), GTK_STOCK_EXECUTE);
			gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
			gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

#if 0
			button = gtk_option_menu_new();
			gtk_button_set_label(GTK_BUTTON(button), GTK_STOCK_PREFERENCES);
			gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
			gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));
#endif

			button = gtk_option_menu_new();
			gtk_button_set_label(GTK_BUTTON(button), GTK_STOCK_JUMP_TO);
			gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
			gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

		}
		{
			GtkWidget *vbox2 = gtk_vbox_new(FALSE, 0);

			gtk_table_attach_defaults(GTK_TABLE(tab), GTK_WIDGET(vbox2), 1, 2, 0, 1);
			{
				GtkWidget *inp = gtk_frame_new(NULL);

				gtk_frame_set_shadow_type(GTK_FRAME(inp), GTK_SHADOW_ETCHED_IN);
				gtk_container_set_border_width(GTK_CONTAINER(inp), 0);

				gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(inp), FALSE, FALSE, 4);

				GtkWidget *login = gtk_table_new(3, 2, FALSE);

				gtk_container_set_border_width(GTK_CONTAINER(login), 5);
				gtk_table_set_col_spacings(GTK_TABLE(login), 5);
				gtk_table_set_row_spacings(GTK_TABLE(login), 5);
				gtk_table_set_col_spacing(GTK_TABLE(login), 0, 0);
				gtk_container_add(GTK_CONTAINER(inp), login);

				GtkWidget *uname = gtk_label_new(NULL);
				gtk_label_set_markup(GTK_LABEL(uname), "<span font=\"Liberation Sans 9\"><b>Username:</b></span>");
				gtk_misc_set_alignment(GTK_MISC(uname), 1.0, 0.5);
				gtk_misc_set_padding(GTK_MISC(uname), 5, 2);

				gtk_table_attach_defaults(GTK_TABLE(login), uname, 0, 1, 0, 1);

				user = gtk_entry_new();
				gtk_entry_set_visibility(GTK_ENTRY(user), TRUE);
				gtk_widget_set_can_default(GTK_WIDGET(user), TRUE);
				gtk_widget_set_can_focus(GTK_WIDGET(user), TRUE);

				gtk_table_attach_defaults(GTK_TABLE(login), user, 1, 2, 0, 1);

				GtkWidget *pword = gtk_label_new(NULL);
				gtk_label_set_markup(GTK_LABEL(pword), "<span font=\"Liberation Sans 9\"><b>Password:</b></span>");
				gtk_misc_set_alignment(GTK_MISC(pword), 1.0, 0.5);
				gtk_misc_set_padding(GTK_MISC(pword), 5, 2);

				gtk_table_attach_defaults(GTK_TABLE(login), pword, 0, 1, 1, 2);

				pass = gtk_entry_new();
				gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
				gtk_widget_set_can_default(GTK_WIDGET(pass), TRUE);
				gtk_widget_set_can_focus(GTK_WIDGET(pass), TRUE);

				gtk_table_attach_defaults(GTK_TABLE(login), pass, 1, 2, 1, 2);

				GtkWidget *xsess = gtk_label_new(NULL);
				gtk_label_set_markup(GTK_LABEL(xsess), "<span font=\"Liberation Sans 9\"><b>XSession:</b></span>");
				gtk_misc_set_alignment(GTK_MISC(xsess), 1.0, 0.5);
				gtk_misc_set_padding(GTK_MISC(xsess), 5, 2);

				gtk_table_attach_defaults(GTK_TABLE(login), xsess, 0, 1, 2, 3);

				GtkWidget *sess = gtk_option_menu_new();
				// gtk_button_set_label(GTK_BUTTON(sess), GTK_STOCK_PREFERENCES);
				// gtk_button_set_use_stock(GTK_BUTTON(sess), TRUE);

				gtk_table_attach_defaults(GTK_TABLE(login), sess, 1, 2, 2, 3);

				// g_signal_connect(G_OBJECT(user), "activate", G_CALLBACK(on_user_activate), pass);
				// g_signal_connect(G_OBJECT(pass), "activate", G_CALLBACK(on_pass_activate), user);

			}
			{
				GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

				gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
								    GTK_SHADOW_ETCHED_IN);
				gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
							       GTK_POLICY_NEVER,
							       GTK_POLICY_AUTOMATIC);
				gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(sw), TRUE, TRUE, 4);

				/* *INDENT-OFF* */
				model = gtk_list_store_new(10
						,GTK_TYPE_STRING	/* hostname */
						,GTK_TYPE_STRING	/* remotename */
						,GTK_TYPE_INT		/* willing */
						,GTK_TYPE_STRING	/* status */
						,GTK_TYPE_STRING	/* IP Address */
						,GTK_TYPE_INT		/* connection type */
						,GTK_TYPE_STRING	/* service */
						,GTK_TYPE_INT		/* port */
						,GTK_TYPE_STRING	/* markup */
						,GTK_TYPE_STRING	/* tooltip */
				    );
				/* *INDENT-ON* */
				gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
						COLUMN_MARKUP, GTK_SORT_ASCENDING);
				view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
				gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
				gtk_tree_view_set_search_column(GTK_TREE_VIEW(view),
								COLUMN_HOSTNAME);
				gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view),
								 COLUMN_TOOLTIP);

				gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(view));

				int len = strlen("XDCMP Host Menu from ") + strlen(hostname) + 1;
				char *title = calloc(len, sizeof(*title));

				strncpy(title, "XDCMP Host Menu from ", len);
				strncat(title, hostname, len);

				GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn *column =
				    gtk_tree_view_column_new_with_attributes(title, renderer,
									     "markup",
									     COLUMN_MARKUP, NULL);

				free(title);
				gtk_tree_view_column_set_sort_column_id(column, COLUMN_HOSTNAME);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(column));
				g_signal_connect(G_OBJECT(view), "row_activated",
						 G_CALLBACK(on_row_activated), (gpointer) NULL);

				// gtk_widget_set_size_request(GTK_WIDGET(sw), -1, 300);
			}
			{
				GtkWidget *bbox = gtk_hbutton_box_new();

				gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
				//gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_EDGE);
				gtk_box_set_spacing(GTK_BOX(bbox), 2);
				gtk_box_pack_end(GTK_BOX(vbox2), GTK_WIDGET(bbox), FALSE, TRUE, 4);

				GtkWidget *button;

				button = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoPing),
						 (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

				button = gtk_button_new_from_stock(GTK_STOCK_QUIT);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoCancel),
						 (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

				button = gtk_button_new_from_stock(GTK_STOCK_CONNECT);
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(DoAccept),
						 (gpointer) win);
				gtk_container_add(GTK_CONTAINER(bbox), GTK_WIDGET(button));

			}
		}
	}
	// gtk_window_set_default_size(GTK_WINDOW(win), -1, 400);

	gtk_window_set_focus_on_map(GTK_WINDOW(win), TRUE);
	gtk_window_set_accept_focus(GTK_WINDOW(win), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_window_stick(GTK_WINDOW(win));
	gtk_window_deiconify(GTK_WINDOW(win));
	gtk_widget_show_all(GTK_WIDGET(win));
	gtk_widget_show_now(GTK_WIDGET(win));
	relax();
	gtk_widget_grab_default(GTK_WIDGET(user));
	gtk_widget_grab_focus(GTK_WIDGET(user));
	return GTK_WINDOW(win);
}

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

static void
run_xchooser(int argc, char *argv[])
{
	gtk_init(NULL, NULL);
	top = GetWindow();
	InitXDMCP(argv, argc);
	gtk_main();
	exit(REMANAGE_DISPLAY);
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
Copyright (c) 2008-2014  Monavacon Limited <http://www.monavacon.com/>\n\
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
Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014  Monavacon Limited.\n\
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
    %1$s [options] ADDRESS [...]\n\
    %1$s [options] {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    ADDRESS [...]\n\
        host names of display managers or \"BROADCAST\"\n\
Command options:\n\
    -h, --help, -?, --?\n\
        print this usage information and exit\n\
    -V, --version\n\
        print version and exit\n\
    -C, --copying\n\
        print copying permission and exit\n\
General options:\n\
    -b, --banner PNGFILE\n\
        banner graphic to display\n\
	(%2$s)\n\
    -w, --welcome TEXT\n\
        text to be display in title area of host menu\n\
	(%3$s)\n\
    -x, --xdmAddress ADDRESS\n\
        address of xdm socket\n\
    -c, --clientAddress IPADDR\n\
        client address that initiated the request\n\
    -t, --connectionType TYPE\n\
        connection type supported by the client\n\
    -n, --dry-run\n\
        do not act: only print intentions (%4$s)\n\
    -D, --debug [LEVEL]\n\
        increment or set debug LEVEL (%5$d)\n\
    -v, --verbose [LEVEL]\n\
        increment or set output verbosity LEVEL (%6$d)\n\
        this option may be repeated.\n\
", argv[0], options.banner, options.welcome, (options.dryrun ? "true" : "false"), options.debug, options.output);
}

void
set_default_banner(void)
{
	static const char *exts[] = { ".xpm", ".png", ".jpg", ".svg" };
	char **xdg_dirs, **dirs, *file, *pfx, *suffix;
	int i, j, n = 0;

	if (!(xdg_dirs = get_data_dirs(&n)) || !n)
		return;

	free(options.banner);
	options.banner = NULL;

	file = calloc(PATH_MAX + 1, sizeof(*file));

	for (pfx = getenv("XDG_MENU_PREFIX") ? : ""; pfx; pfx = *pfx ? "" : NULL) {
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
		if (options.banner)
			break;
	}

	free(file);

	for (i = 0; i < n; i++)
		free(xdg_dirs[i]);
	free(xdg_dirs);
}

void
set_default_welcome(void)
{
	char hostname[64] = { 0, };
	char *buf;
	int len;
	static char *format = "Welcome to %s!";

	free(options.welcome);
	gethostname(hostname, sizeof(hostname));
	len = strlen(format) + strnlen(hostname, sizeof(hostname)) + 1;
	buf = options.welcome = calloc(len, sizeof(*buf));
	snprintf(buf, len, format, hostname);
}

void
set_defaults(void)
{
	set_default_banner();
	set_default_welcome();
}

typedef enum {
	COMMAND_XCHOOSE,
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_COPYING,
} CommandType;

int
main(int argc, char *argv[])
{
	CommandType command = COMMAND_XCHOOSE;

	set_defaults();

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

			{"dry-run",	    no_argument,	NULL, 'n'},
			{"debug",	    optional_argument,	NULL, 'D'},
			{"verbose",	    optional_argument,	NULL, 'v'},
			{"help",	    no_argument,	NULL, 'h'},
			{"version",	    no_argument,	NULL, 'V'},
			{"copying",	    no_argument,	NULL, 'C'},
			{"?",		    no_argument,	NULL, 'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "x:c:t:b:w:nD::v::hVCH?", long_options,
				     &option_index);
#else
		c = getopt(argc, argv, "x:c:t:b:w:nDvhVCH?");
#endif
		if (c == -1) {
			DPRINTF("%s: done options processing\n", argv[0]);
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
			else if (!strcmp(optarg, "FamilyInternet6")
				 || atoi(optarg) == FamilyInternet6)
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
		case 'n':	/* -n, --dry-run */
			options.dryrun = True;
			break;
		case 'D':	/* -D, --debug [level] */
			DPRINTF("%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
			} else {
				if ((val = strtol(optarg, NULL, 0)) < 0)
					goto bad_option;
				options.debug = val;
			}
			break;
		case 'v':	/* -v, --verbose [level] */
			DPRINTF("%s: increasing output verbosity\n", argv[0]);
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
			if (command != COMMAND_XCHOOSE)
				goto bad_option;
			command = COMMAND_HELP;
			break;
		case 'V':	/* -V, --version */
			if (command != COMMAND_XCHOOSE)
				goto bad_option;
			command = COMMAND_VERSION;
			break;
		case 'C':	/* -C, --copying */
			if (command != COMMAND_XCHOOSE)
				goto bad_option;
			command = COMMAND_COPYING;
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
			exit(REMANAGE_DISPLAY);
		}
	}
	DPRINTF("%s: option index = %d\n", argv[0], optind);
	DPRINTF("%s: option count = %d\n", argv[0], argc);
	if (command == COMMAND_XCHOOSE && optind >= argc) {
		EPRINTF("%s: missing non-option argument\n", argv[0]);
		usage(argc, argv);
		exit(REMANAGE_DISPLAY);
	}

	switch (command) {
	case COMMAND_XCHOOSE:
		DPRINTF("%s: running xchooser\n", argv[0]);
		run_xchooser(argc - optind, &argv[optind]);
		break;
	case COMMAND_HELP:
		DPRINTF("%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case COMMAND_VERSION:
		DPRINTF("%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case COMMAND_COPYING:
		DPRINTF("%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	}
	exit(OBEYSESS_DISPLAY);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
