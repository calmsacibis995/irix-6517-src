/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_dhcp.c,v 1.1 1996/06/19 20:33:36 nn Exp $"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <netinet/in.h>
#ifdef sgi
#include <dhcp.h>
#else
#include <netinet/dhcp.h>
#endif
#include <dhcp_gen.h>
#include "snoop.h"

typedef unsigned char ether_addr_t[6];

extern char *dlc_header;
char *show_htype(int);
void display_ip(int, char *, unsigned char **);
void display_ascii(char *, unsigned char **);
void display_number(char *, unsigned char **);
void display_hex(char *, unsigned char **);
void display_ascii_hex(char *, unsigned char **);
void display_unamed_options(char *, u_char **);
unsigned char bootmagic[] = BOOTMAGIC;	/* rfc 1048 */

static char *option_types[] = {
"",					/* 0 */
"Subnet Mask",				/* 1 */
"UTC Time Offset",			/* 2 */
"Routers",				/* 3 */
"RFC868 Time Servers",			/* 4 */
"IEN 116 Name Servers",			/* 5 */
"DNS Servers",				/* 6 */
"UDP LOG Servers",			/* 7 */
"RFC 865 Cookie Servers",		/* 8 */
"RFC 1179 Line Printer Servers (LPR)",	/* 9 */
"Impress Servers",			/* 10 */
"RFC 887 Resource Location Servers",	/* 11 */
"Client Hostname",			/* 12 */
"Boot File size in 512 byte Blocks",	/* 13 */
"Merit Dump File",			/* 14 */
"DNS Domain Name",			/* 15 */
"SWAP Server",				/* 16 */
"Client Root Path",			/* 17 */
"BOOTP options extensions path",	/* 18 */
"IP Forwarding Flag",			/* 19 */
"NonLocal Source Routing Flag",		/* 20 */
"Policy Filters for NonLocal Routing",	/* 21 */
"Maximum Datagram Reassembly Size",	/* 22 */
"Default IP Time To Live",		/* 23 */
"Path MTU Aging Timeout",		/* 24 */
"Path MTU Size Plateau Table",		/* 25 */
"Interface MTU Size",			/* 26 */
"All Subnets are Local Flag",		/* 27 */
"Broadcast Address",			/* 28 */
"Perform Mask Discovery Flag",		/* 29 */
"Mask Supplier Flag",			/* 30 */
"Perform Router Discovery Flag",	/* 31 */
"Router Solicitation Address",		/* 32 */
"Static Routes",			/* 33 */
"Trailer Encapsulation Flag",		/* 34 */
"ARP Cache Timeout Seconds",		/* 35 */
"Ethernet Encapsulation Flag",		/* 36 */
"TCP Default Time To Live",		/* 37 */
"TCP Keepalive Interval Seconds",	/* 38 */
"TCP Keepalive Garbage Flag",		/* 39 */
"NIS Domainname",			/* 40 */
"NIS Servers",				/* 41 */
"Network Time Protocol Servers",	/* 42 */
"Vendor Specific Options",		/* 43 */
"NetBIOS RFC 1001/1002 Name Servers",	/* 44 */
"NetBIOS Datagram Dist. Servers",	/* 45 */
"NetBIOS Node Type",			/* 46 */
"NetBIOS Scope",			/* 47 */
"X Window Font Servers",		/* 48 */
"X Window Display Manager Servers",	/* 49 */
"Requested IP Address",			/* 50 */
"IP Address Lease Time",		/* 51 */
"Option Field Overload Flag",		/* 52 */
"DHCP Message Type",			/* 53 */
"DHCP Server Identifier",		/* 54 */
"Option Request List",			/* 55 */
"Error Message",			/* 56 */
"Maximum DHCP Message Size",		/* 57 */
"Renewal (T1) Time Value",		/* 58 */
"Rebinding (T2) Time Value",		/* 59 */
"Client Class Identifier",		/* 60 */
"Client Identifier"			/* 61 */
};

/*
 * Converts an octet string into an ASCII string.
 */
static int
octet_to_ascii(u_char *nump, int nlen, char *bufp, int *blen)
{
	register int	i;
	register char	*bp;
	register u_char	*np;
	static char	ascii_conv[] = {'0', '1', '2', '3', '4', '5', '6',
	    '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	if (nump == (u_char *)NULL || bufp == (char *)NULL)
		return (-1);

	if (nlen > ((*blen * 2) + 1))
		return (-1);

	for (i = 0, bp = bufp, np = nump; i < nlen; i++) {
		*bp++ = ascii_conv[(np[i] >> 4) & 0x0f];
		*bp++ = ascii_conv[np[i] & 0x0f];
	}
	*bp = '\0';
	*blen = i * 2;
	return (0);
}

int
interpret_dhcp(int flags, PKT *dp, int len)
{
	if (flags & F_SUM) {
		switch (ntohs(dp->op)) {
		case BOOTREQUEST:
			(void) sprintf(get_sum_line(),
			    "DHCP/BOOTP BOOTREQUEST  server=%s file=%s", dp->sname, dp->file);
			break;
		case BOOTREPLY:
			(void) sprintf(get_sum_line(),
			    "DHCP/BOOTP BOOTREPLY  server=%s", dp->sname);
			break;
		}
	}
	if (flags & F_DTAIL) {
		show_header("DHCP: ", "Dynamic Host Configuration Protocol",
		    len);
		show_space();
		(void) sprintf(get_line((char *)dp->htype - dlc_header, 1),
		    "Hardware address type (htype) =  %d (%s)", dp->htype,
		    show_htype(dp->htype));
		(void) sprintf(get_line((char *)dp->hlen - dlc_header, 1),
		    "Hardware address length (hlen) = %d octets", dp->hlen);
		(void) sprintf(get_line((char *)dp->hops - dlc_header, 1),
		    "Relay agent hops = %d", dp->hops);
		(void) sprintf(get_line((char *)dp->xid - dlc_header, 4),
		    "Transaction ID = 0x%x", dp->xid);
		(void) sprintf(get_line((char *)dp->secs - dlc_header, 2),
		    "Time since boot = %d seconds", dp->secs);
		(void) sprintf(get_line((char *)dp->flags - dlc_header, 2),
		    "Flags = 0x%.4x", dp->flags);
		(void) sprintf(get_line((char *)&dp->ciaddr - dlc_header, 4),
		    "Client address (ciaddr) = %s", inet_ntoa(dp->ciaddr));
		(void) sprintf(get_line((char *)&dp->yiaddr - dlc_header, 4),
		    "Your client address (yiaddr) = %s",
		    inet_ntoa(dp->yiaddr));
		(void) sprintf(get_line((char *)&dp->siaddr - dlc_header, 4),
		    "Next server address (siaddr) = %s",
		    inet_ntoa(dp->siaddr));
		(void) sprintf(get_line((char *)&dp->giaddr - dlc_header, 4),
		    "Relay agent address (giaddr) = %s",
		    inet_ntoa(dp->giaddr));
		if (dp->htype == 1) {
			(void) sprintf(get_line((char *)dp->chaddr -
			    dlc_header, dp->hlen),
	"Client hardware address (chaddr) = %.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
			    *(ether_addr_t *) dp->chaddr[0],
			    *(ether_addr_t *) dp->chaddr[1],
			    *(ether_addr_t *) dp->chaddr[2],
			    *(ether_addr_t *) dp->chaddr[3],
			    *(ether_addr_t *) dp->chaddr[4],
			    *(ether_addr_t *) dp->chaddr[5]);
		}
		/*
		 * Check cookie, process options
		 */
		if (memcmp(dp->cookie, bootmagic, sizeof (bootmagic)) != 0) {
			(void) sprintf(get_line(0, 0),
			    "Unrecognized cookie: 0x%x\n",
			    /* ALIGNED */
			    *(unsigned long *)(dp->cookie));
			return (0);
		}
		show_space();
		show_header("DHCP: ", "(Options) field options", len);
		show_space();
		switch (show_options(dp->options, (len - BASE_PKT_SIZE))) {
		case 0:
			/* No option overloading */
			if (*(unsigned char *)(dp->sname) != '\0') {
				(void) sprintf(get_line(0, 0),
				    "Server Name = %s", dp->sname);
			}
			if (*(unsigned char *)(dp->file) != '\0') {
				(void) sprintf(get_line(0, 0),
				    "Boot File Name = %s", dp->file);
			}
			break;
		case 1:
			/* file field used */
			if (*(unsigned char *)(dp->sname) != '\0') {
				(void) sprintf(get_line(0, 0),
				    "Server Name = %s", dp->sname);
			}
			show_space();
			show_header("DHCP: ", "(File) field options", len);
			show_space();
			(void) show_options(dp->file, 128);
			break;
		case 2:
			/* sname field used for options */
			if (*(unsigned char *)(dp->file) != '\0') {
				(void) sprintf(get_line(0, 0),
				    "Boot File Name = %s", dp->file);
			}
			show_space();
			show_header("DHCP: ", "(Sname) field options", len);
			show_space();
			(void) show_options(dp->sname, 64);
			break;
		case 3:
			show_space();
			show_header("DHCP: ", "(File) field options", len);
			show_space();
			(void) show_options(dp->file, 128);
			show_space();
			show_header("DHCP: ", "(Sname) field options", len);
			show_space();
			(void) show_options(dp->sname, 64);
			break;
		};
	}
	return (len);
}
int
show_options(unsigned char  *cp, int len)
{
	register unsigned char *end, *vend;
	unsigned char *start;
	register int items, i;
	register int nooverload = 0;
	u_short	s_buf;
	char scratch[64];

	start = cp;
	end = (unsigned char *)cp + len;

	while (start < end) {
		if (*start == CD_PAD) {
			start++;
			continue;
		}
		if (*start == CD_END)
			break;	/* done */
		if (*start > DHCP_LAST_OPT) {
			(void) sprintf(get_line(0, 0),
			    "Site Specific Option = %d, length = %d bytes",
			    *start, *(u_char *)((u_char *)start + 1));
			start++;
			display_hex("Value = ", &start);
			continue;
		}
		switch (*start) {
		/* Network order IP address(es) */
		case CD_SUBNETMASK:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Subnet mask");
			} else {
				start++;
				display_ip(1, "Subnet mask = %s", &start);
			}
			break;
		case CD_TIMEOFFSET:
			++start;
			display_number(
			    "Universal Coordinated time offset = %d seconds",
			    &start);
			break;
		case CD_ROUTER:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad ROUTER address");
			} else {
				display_ip((*start++ / 4), "Router at = %s",
				    &start);
			}
			break;
		case CD_TIMESERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad RFC868 TIME SERVER address");
			} else {
				display_ip((*start++ / 4),
				    "RFC868 server at = %s", &start);
			}
			break;
		case CD_IEN116_NAME_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad IEN116 Name server address");
			} else {
				display_ip((*start++ / 4),
				    "IEN116 Name server at = %s", &start);
			}
			break;
		case CD_DNSSERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad DNS server address");
			} else {
				display_ip((*start++ / 4),
				    "DNS server at = %s", &start);
			}
			break;
		case CD_LOG_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad LOG server address");
			} else {
				display_ip((*start++ / 4),
				    "LOG server at = %s", &start);
			}
			break;
		case CD_COOKIE_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad COOKIE server address");
			} else {
				display_ip((*start++ / 4),
				    "COOKIE server at = %s", &start);
			}
			break;
		case CD_LPR_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad LPR server address");
			} else {
				display_ip((*start++ / 4),
				    "LPR server at = %s", &start);
			}
			break;
		case CD_IMPRESS_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad IMPRESS server address");
			} else {
				display_ip((*start++ / 4),
				    "IMPRESS server at = %s", &start);
			}
			break;
		case CD_RESOURCE_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad RESOURCE server address");
			} else {
				display_ip((*start++ / 4),
				    "RESOURCE server at = %s", &start);
			}
			break;
		case CD_HOSTNAME:
			++start;
			display_ascii("Hostname = %s", &start);
			break;
		case CD_BOOT_SIZE:
			++start;
			display_number("Boot file size = %d 512 byte blocks",
			    &start);
			break;
		case CD_DUMP_FILE:
			++start;
			display_ascii("Dump file = %s", &start);
			break;
		case CD_DNSDOMAIN:
			++start;
			display_ascii("DNS Domain = %s", &start);
			break;
		case CD_SWAP_SERV:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad SWAP server address");
			} else {
				start++;
				display_ip(1, "SWAP server at = %s", &start);
			}
			break;
		case CD_ROOT_PATH:
			++start;
			display_ascii("Root Path = %s", &start);
			break;
		case CD_IP_FORWARDING_ON:
			++start;
			display_number("IP forwarding flag = 0x%x", &start);
			break;
		case CD_NON_LCL_ROUTE_ON:
			++start;
			display_number(
			    "Non-local Source Routing flag = 0x%x", &start);
			break;
		case CD_POLICY_FILTER:
			++start;
			if ((*start % 8) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Policy Filter option");
			} else {
				items = *start++ / 8;
				for (i = 0; i < items; i++) {
					display_ip(1,
					    "Policy Destination = %s",
					    &start);
					display_ip(1, "Mask = %s", &start);
				}
			}
			break;
		case CD_MAXIPSIZE:
			++start;
			display_number(
			    "Max IP Datagram Reassembly size = %d bytes",
			    &start);
			break;
		case CD_IPTTL:
			++start;
			display_number(
			    "Default IP Time To Live = %d seconds", &start);
			break;
		case CD_PATH_MTU_TIMEOUT:
			++start;
			display_number("Path MTU Aging Timeout = %d seconds",
			    &start);
			break;
		case CD_PATH_MTU_TABLE_SZ:
			++start;
			if (*start % 2 != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Path MTU Table");
			} else {
				(void) sprintf(get_line(0, 0),
				    "\tPath MTU Plateau Table:");
				(void) sprintf(get_line(0, 0),
				    "\t=======================");
				items = *start / sizeof (u_short);
				++start;
				for (i = 0; i < items; i++) {
					if (!((u_int)(*start) & 1)) {
						s_buf = *(u_short *)start;
					} else {
						memcpy((char *)&s_buf,
						    start, sizeof (short));
					}
					(void) sprintf(get_line(0, 0),
					    "\t\tEntry %d:\t\t%d", i, s_buf);
					start += sizeof (u_short);
				}
			}
			break;
		case CD_MTU:
			++start;
			display_number("Max MTU size = %d bytes", &start);
			break;
		case CD_ALL_SUBNETS_LCL_ON:
			++start;
			display_number("All Subnets are Local flag = 0x%x",
			    &start);
			break;
		case CD_BROADCASTADDR:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad BROADCAST address");
			} else {
				start++;
				display_ip(1,
				    "Network broadcast address = %s",
				    &start);
			}
			break;
		case CD_MASK_DISCVRY_ON:
			++start;
			display_number("Perform Mask Discovery flag = 0x%x",
			    &start);
			break;
		case CD_MASK_SUPPLIER_ON:
			++start;
			display_number("Mask Supplier flag = 0x%x", &start);
			break;
		case CD_ROUTER_DISCVRY_ON:
			++start;
			display_number("Router Discovery flag = 0x%x",
			    &start);
			break;
		case CD_ROUTER_SOLICIT_SERV:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Router Solicitation address");
			} else {
				start++;
				display_ip(1, "Router Solicitation at = %s",
				    &start);
			}
			break;
		case CD_STATIC_ROUTE:
			++start;
			if ((*start % 8) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Static Route option: %d",
				    *start);
			} else {
				items = *start++ / 8;
				for (i = 0; i < items; i++) {
					(void) strcpy(scratch,
					    (char*)inet_ntoa(*(int*)start));
					start += sizeof (u_long);
					(void) sprintf(get_line(0, 0),
					    "Static route from %s to %s",
					    inet_ntoa(*(int*)start), scratch);
					start += sizeof (u_long);
				}
			}
			break;
		case CD_TRAILER_ENCAPS_ON:
			++start;
			display_number("Trailer Encapsulation flag = 0x%x",
			    &start);
			break;
		case CD_ARP_TIMEOUT:
			++start;
			display_number("ARP cache timeout = %d seconds",
			    &start);
			break;
		case CD_ETHERNET_ENCAPS_ON:
			++start;
			display_number("Ethernet Encapsulation flag = 0x%x",
			    &start);
			break;
		case CD_TCP_TTL:
			++start;
			display_number(
			    "TCP default Time To Live = %d seconds", &start);
			break;
		case CD_TCP_KALIVE_INTVL:
			++start;
			display_number("TCP Keepalive Interval = %d seconds",
			    &start);
			break;
		case CD_TCP_KALIVE_GRBG_ON:
			++start;
			display_number("TCP Keepalive Garbage flag = 0x%x",
			    &start);
			break;
		case CD_NIS_DOMAIN:
			++start;
			display_ascii("NIS Domain = %s", &start);
			break;
		case CD_NIS_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad NIS server address");
			} else {
				display_ip((*start++ / 4),
				    "NIS server at = %s", &start);
			}
			break;
		case CD_NTP_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad NTP server address");
			} else {
				display_ip((*start++ / 4),
				    "NTP server at = %s", &start);
			}
			break;
		case CD_VENDOR_SPEC:
			++start;
			i = *start++;
			(void) sprintf(get_line(0, 0),
			    "Vendor-specific Options (%d total octets):", i);
			/*
			 * We don't know what these things are, so just
			 * display the option number, length, and value
			 * (hex).
			 */
			vend = (u_char *)((u_char *)start + i);
			while (start < vend && *start != CD_END) {
				if (*start == CD_PAD) {
					start++;
					continue;
				}
				(void) sprintf(scratch,
				    "\t(%.2d) %.2d octets", *start,
				    *(u_char *)((u_char *)start + 1));
				start++;
				display_ascii_hex(scratch, &start);
			}
			start = vend;	/* in case CD_END found */
			break;
		case CD_NETBIOS_NAME_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad '%s' parameter",
				    option_types[CD_NETBIOS_NAME_SERV]);
			} else {
				display_ip((*start++ / 4),
				    "NetBIOS Name Server = %s", &start);
			}
			break;
		case CD_NETBIOS_DIST_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad '%s' parameter",
				    option_types[CD_NETBIOS_DIST_SERV]);
			} else {
				display_ip((*start++ / 4),
				    "NetBIOS Datagram Dist. Server = %s",
				    &start);
			}
			break;
		case CD_NETBIOS_NODE_TYPE:
			++start;
			if (*start != 1) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad '%s' parameter",
				    option_types[CD_NETBIOS_NODE_TYPE]);
			} else {
				char *type;
				start++;
				switch (*start) {
				case 0x1:
					type = "Broadcast Node";
					break;
				case 0x2:
					type = "Point To Point Node";
					break;
				case 0x4:
					type = "Mixed Mode Node";
					break;
				case 0x8:
					type = "Hibrid Node";
					break;
				default:
					type = "??? Node";
					break;
				};
				(void) sprintf(get_line(0, 0),
				    "%s = %s (%d)",
				    option_types[CD_NETBIOS_NODE_TYPE],
				    type, *start);
				start++;
			}
			break;
		case CD_NETBIOS_SCOPE:
			++start;
			display_ascii("NetBIOS Scope = ", &start);
			break;
		case CD_XWIN_FONT_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad '%s' parameter",
				    option_types[CD_XWIN_FONT_SERV]);
			} else {
				display_ip((*start ++ / 4),
				    "X Windows Font Server = %s", &start);
			}
			break;
		case CD_XWIN_DISP_SERV:
			++start;
			if ((*start % 4) != 0) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad '%s' parameter",
				    option_types[CD_XWIN_DISP_SERV]);
			} else {
				display_ip((*start++ / 4),
				    "X Windows Display Manager = %s",
				    &start);
			}
			break;
		case CD_REQUESTED_IP_ADDR:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad Requested IP address");
			} else {
				start++;
				display_ip(1, "Requested IP = %s", &start);
			}
			break;
		case CD_LEASE_TIME:
			++start;
			display_number("DHCP Lease time = %d seconds",
			    &start);
			break;
		case CD_OPTION_OVERLOAD:
			++start;
			if (*start != 1) {
				(void) sprintf(get_line(0, 0),
				    "Bad Option Overload value.");
			} else {
				start++;
				nooverload = *start++;
			}
			break;
		case CD_DHCP_TYPE:
			++start;
			if (*start < 1 || *start > 7) {
				(void) sprintf(get_line(0, 0),
				    "Bad DHCP Message Type.");
			} else {
				start++;
				switch (*start) {
				case DISCOVER:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPDISCOVER");
					break;
				case OFFER:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPOFFER");
					break;
				case REQUEST:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPREQUEST");
					break;
				case DECLINE:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPDECLINE");
					break;
				case ACK:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPACK");
					break;
				case NAK:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPNAK");
					break;
				case RELEASE:
					(void) sprintf(get_line(0, 0),
					    "Message type = DHCPRELEASE");
					break;
				};
				start++;
			}
			break;
		case CD_SERVER_ID:
			++start;
			if (*start != 4) {
				(void) sprintf(get_line(0, 0),
				    "Error: Bad SERVER ID address");
			} else {
				start++;
				display_ip(1, "Server Identifier  = %s",
				    &start);
			}
			break;
		case CD_REQUEST_LIST:
			++start;
			len = *start++;
			(void) sprintf(get_line(0, 0),
			    "Requested Options:");
			for (i = 0; i < len; i++) {
				(void) sprintf(get_line(0, 0),
				    "\t%2d (%s)", *start,
				    option_types[*start]);
				start++;
			}
			break;
		case CD_MESSAGE:
			++start;
			display_ascii("Error Message = %s", &start);
			break;
		case CD_MAX_DHCP_SIZE:
			++start;
			display_number("MAX DHCP Message size = %d bytes",
			    &start);
			break;
		case CD_T1_TIME:
			++start;
			display_number(
			    "Lease Renewal (T1) Time = %d seconds", &start);
			break;
		case CD_T2_TIME:
			++start;
			display_number(
			    "Lease Rebinding (T2) Time = %d seconds",
			    &start);
			break;
		case CD_CLASS_ID:
			++start;
			display_ascii_hex("Client Class Identifier = ",
			    &start);
			break;
		case CD_CLIENT_ID:
			++start;
			display_ascii_hex("Client Identifier = ", &start);
			break;
		};
	}
	return (nooverload);
}
char *
show_htype(int t)
{
	switch (t) {
	case 1:
		return ("Ethernet (10Mb)");
	case 2:
		return ("Experimental Ethernet (3MB)");
	case 3:
		return ("Amateur Radio AX.25");
	case 4:
		return ("Proteon ProNET Token Ring");
	case 5:
		return ("Chaos");
	case 6:
		return ("IEEE 802");
	case 7:
		return ("ARCNET");
	case 8:
		return ("Hyperchannel");
	case 9:
		return ("Lanstar");
	case 10:
		return ("Autonet");
	case 11:
		return ("LocalTalk");
	case 12:
		return ("LocalNet");
	case 13:
		return ("Ultra Link");
	case 14:
		return ("SMDS");
	case 15:
		return ("Frame Relay");
	case 16:
		return ("ATM");
	};
	return ("UNKNOWN");
}
void
display_ip(int items, char *msg, unsigned char **opt)
{
	struct in_addr tmp;
	register int i;

	for (i = 0; i < items; i++) {
		memcpy((char *)&tmp, *opt, sizeof (struct in_addr));
		(void) sprintf(get_line(0, 0), msg, inet_ntoa(tmp));
		*opt += 4;
	}
}
void
display_ascii(char *msg, unsigned char **opt)
{
	static unsigned char buf[256];
	register unsigned char len = **opt;

	(*opt)++;
	memcpy(buf, *opt, len);
	*(unsigned char *)(buf + len) = '\0';
	(void) sprintf(get_line(0, 0), msg, buf);
	(*opt) += len;
}
void
display_number(char *msg, unsigned char **opt)
{
	register int len = **opt;
	unsigned long l_buf = 0;
	unsigned short s_buf = 0;

	if (len > 4) {
		(void) sprintf(get_line(0, 0), msg, 0xdeadbeef);
		return;
	}
	switch (len) {
	case sizeof (u_char):
		(*opt)++;
		(void) sprintf(get_line(0, 0), msg, **opt);
		break;
	case sizeof (u_short):
		(*opt)++;
		if (!((u_int)(*opt) & 1))
			s_buf = *(unsigned short *)*opt;
		else
			memcpy((char *)&s_buf, *opt, len);
		(void) sprintf(get_line(0, 0), msg, ntohs(s_buf));
		break;
	case sizeof (u_long):
		(*opt)++;
		if (!((u_int)(*opt) & 3))
			l_buf = *(unsigned long *)*opt;
		else
			memcpy((char *)&l_buf, *opt, len);
		(void) sprintf(get_line(0, 0), msg, ntohl(l_buf));
		break;
	}
	(*opt) += len;
}
void
display_hex(char *msg, unsigned char **opt)
{
	char *start, *line;
	int i, len = **opt;

	start = line = get_line(0, 0);

	if (len > 256) {
		(void) sprintf(line, msg, "TOO BIG");
		return;
	}

	line += sprintf(line, msg);
	line += sprintf(line, "0x");
	(*opt)++;

	i = (int)(MAXLINE - (line - start));

	(void) octet_to_ascii(*opt, len, line, &i);
	(*opt) += len;
}
void
display_ascii_hex(char *msg, unsigned char **opt)
{
	register int printable;
	char	buffer[257];
	char  *line, *tmp, *ap;
	int	i, len = **opt;

	line = get_line(0, 0);

	if (len > 256) {
		(void) sprintf(line, "\t%s <TOO LONG>", msg);
		return;
	}

	(*opt)++;

	for (printable = 1, tmp = (char *)(*opt), ap = buffer;
	    tmp < (char *)&((*opt)[len]); tmp++) {
		if (isprint(*tmp))
			*ap++ = *tmp;
		else {
			*ap++ = '.';
			printable = 0;
		}
	}
	*ap = '\0';

	if (!printable) {
		i = 257;
		(void) octet_to_ascii(*opt, len, buffer, &i);
		(void) sprintf(line, "%s\t0x%s (unprintable)", msg, buffer);
	} else
		(void) sprintf(line, "%s\t%s", msg, buffer);
	(*opt) += len;
}
