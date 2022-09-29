/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_ip.c,v 1.3 1998/11/25 20:09:56 jlan Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include "snoop.h"

extern char *dlc_header;
extern char *addrtoname();
extern char *tohex();

u_short ip_chksum;
void print_route(u_char *opt);

u_short
ip_checksum(ip, iplen)
	struct ip *ip;
	u_short iplen;
{
	u_short sum = 0;
	register int i;
	struct {	/* IP Pseudo header */
		struct	in_addr ph_src, ph_dst;
		u_short	ph_proto;
		u_short	ph_len;
	} ph;

	ph.ph_src.s_addr = ip->ip_src.s_addr;
	ph.ph_dst.s_addr = ip->ip_dst.s_addr;
	ph.ph_proto = ip->ip_p;
	ph.ph_len   = iplen;
	for (i = 0; i < 6; i++)
		sum += ((u_short *)&ph)[i];

	return (sum);
}

extern char *inet_ntoa();

interpret_ip(flags, ip, fraglen)
	int flags;
	struct ip *ip;
	int fraglen;
{
	char *getproto();
	char *data;
	char buff[24];
	int isfrag;
	int hdrlen, iplen;
	void print_ipoptions();
	extern char *src_name, *dst_name;

	hdrlen = ip->ip_hl * 4;
	data = ((char *) ip) + hdrlen;
	iplen = ntohs(ip->ip_len) - hdrlen;
	fraglen -= hdrlen;
	if (fraglen > iplen)
		fraglen = iplen;
	isfrag = ntohs(ip->ip_off) & 0x1FFF;
	ip_chksum = ip_checksum(ip, iplen);

	src_name = addrtoname(ip->ip_src);
	dst_name = addrtoname(ip->ip_dst);

	if (flags & F_SUM) {
		if (isfrag) {
			(void) sprintf(get_sum_line(),
				"%s continuation ID=%d",
				getproto(ip->ip_p),
				ntohs(ip->ip_id));
		} else {
			(void) strcpy(buff, inet_ntoa(ip->ip_dst));
			(void) sprintf(get_sum_line(),
				"IP  D=%s S=%s LEN=%d, ID=%d",
				buff,
				inet_ntoa(ip->ip_src),
				ntohs(ip->ip_len),
				ntohs(ip->ip_id));
		}
	}

	if (flags & F_DTAIL) {

	show_header("IP:   ", "IP Header", iplen);
	show_space();
	(void) sprintf(get_line((char *)ip - dlc_header, 1),
		"Version = %d",
		ip->ip_v);
	(void) sprintf(get_line((char *)ip - dlc_header, 1),
		"Header length = %d bytes",
		hdrlen);
	(void) sprintf(get_line((char *)&ip->ip_tos - dlc_header, 1),
		"Type of service = 0x%02x",
		ip->ip_tos);
	(void) sprintf(get_line((char *)&ip->ip_tos - dlc_header, 1),
		"      xxx. .... = %d (precedence)", ip->ip_tos >> 5);
	(void) sprintf(get_line((char *)&ip->ip_tos - dlc_header, 1),
		"      %s",
		getflag(ip->ip_tos, 0x10, "low delay", "normal delay"));
	(void) sprintf(get_line((char *)&ip->ip_tos - dlc_header, 1),
		"      %s",
		getflag(ip->ip_tos, 0x08,
			"high throughput",
			"normal throughput"));
	(void) sprintf(get_line((char *)&ip->ip_tos - dlc_header, 1),
		"      %s",
		getflag(ip->ip_tos, 0x04,
			"high reliability",
			"normal reliability"));
	(void) sprintf(get_line((char *)&ip->ip_len - dlc_header, 2),
		"Total length = %d bytes",
		ntohs(ip->ip_len));
	(void) sprintf(get_line((char *)&ip->ip_id - dlc_header, 2),
		"Identification = %d",
		ntohs(ip->ip_id));
	(void) sprintf(get_line((char *)&ip->ip_off - dlc_header, 1),
		"Flags = 0x%x",
		ntohs(ip->ip_off) >> 12);
	(void) sprintf(get_line((char *)&ip->ip_off - dlc_header, 1),
		"      %s",
		getflag(ntohs(ip->ip_off) >> 8, IP_DF >> 8, "do not fragment",
			"may fragment"));
	(void) sprintf(get_line((char *)&ip->ip_off - dlc_header, 1),
		"      %s",
		getflag(ntohs(ip->ip_off) >> 8, IP_MF >> 8, "more fragments",
			"last fragment"));
	(void) sprintf(get_line((char *)&ip->ip_off - dlc_header, 2),
		"Fragment offset = %d bytes",
		(ntohs(ip->ip_off) & 0x1FFF) * 8);
	(void) sprintf(get_line((char *)&ip->ip_ttl - dlc_header, 1),
		"Time to live = %d seconds/hops",
		ip->ip_ttl);
	(void) sprintf(get_line((char *)&ip->ip_p - dlc_header, 1),
		"Protocol = %d (%s)",
		ip->ip_p, getproto(ip->ip_p));
	/* XXX need to compute checksum and print whether it's correct */
	(void) sprintf(get_line((char *)&ip->ip_sum - dlc_header, 1),
		"Header checksum = %04x",
		ip->ip_sum);
	(void) sprintf(get_line((char *)&ip->ip_src - dlc_header, 1),
		"Source address = %s, %s",
		inet_ntoa(ip->ip_src), addrtoname(ip->ip_src));
	(void) sprintf(get_line((char *)&ip->ip_dst - dlc_header, 1),
		"Destination address = %s, %s",
		inet_ntoa(ip->ip_dst), addrtoname(ip->ip_dst));

	/* Print IP options - if any */

	print_ipoptions(ip + 1, hdrlen - sizeof (struct ip));
	show_space();
	}

	if (isfrag) {
		if (flags & F_DTAIL) {
			(void) sprintf(get_detail_line(
				data - dlc_header, iplen),
		"%s:  [%d byte(s) of data, continuation of IP ident=%d]",
				getproto(ip->ip_p),
				iplen,
				ntohs(ip->ip_id));
		}
	} else {
		/* go to the next protocol layer */

		if (fraglen > 0) {
			switch (ip->ip_p) {
			case IPPROTO_IP:
				break;
			case IPPROTO_ICMP:
				interpret_icmp(flags, data, iplen, fraglen);
				break;
			case IPPROTO_IGMP:
			case IPPROTO_GGP:
				break;
			case IPPROTO_TCP:
				interpret_tcp(flags, data, iplen, fraglen);
				break;

			case IPPROTO_EGP:
			case IPPROTO_PUP:
				break;
			case IPPROTO_UDP:
				interpret_udp(flags, data, iplen, fraglen);
				break;

			case IPPROTO_IDP:
			case IPPROTO_HELLO:
			case IPPROTO_ND:
			case IPPROTO_RAW:
				break;
			}
		}
	}

	return (iplen);
}

void
print_ipoptions(opt, optlen)
	u_char *opt;
	int optlen;
{
	int len;
	char *line;

	if (optlen <= 0) {
		(void) sprintf(get_line((char *)&opt - dlc_header, 1),
		"No options");
		return;
	}

	(void) sprintf(get_line((char *)&opt - dlc_header, 1),
	"Options: (%d bytes)", optlen);

	while (optlen > 0) {
		line = get_line((char *)&opt - dlc_header, 1);
		len = opt[1];
		switch (opt[0]) {
		case IPOPT_EOL:
			(void) strcpy(line, "  - End of option list");
			return;
		case IPOPT_NOP:
			(void) strcpy(line, "  - No op");
			len = 1;
			break;
		case IPOPT_RR:
			(void) sprintf(line, "  - Record route (%d bytes)",
				len);
			print_route(opt);
			break;
		case IPOPT_TS:
			(void) sprintf(line, "  - Time stamp (%d bytes)", len);
			break;
		case IPOPT_SECURITY:
			(void) sprintf(line, "  - Security (%d bytes)", len);
			break;
		case IPOPT_LSRR:
			(void) sprintf(line,
				"  - Loose source route (%d bytes)", len);
			print_route(opt);
			break;
		case IPOPT_SATID:
			(void) sprintf(line, "  - SATNET Stream id (%d bytes)",
				len);
			break;
		case IPOPT_SSRR:
			(void) sprintf(line,
				"  - Strict source route, (%d bytes)", len);
			print_route(opt);
			break;
		default:
			sprintf(line,
			"  - Option %d (unknown - %d bytes) %s",
				opt[0],
				len,
				tohex(&opt[2], len - 2));
			break;
		}
		opt += len;
		optlen -= len;
	}
}

void
print_route(opt)
	u_char *opt;
{
	int len, pointer;
	struct in_addr addr;
	char *line;

	len = opt[1];
	pointer = opt[2];

	(void) sprintf(get_line((char *)(&opt + 2) - dlc_header, 1),
		"    Pointer = %d", pointer);

	pointer -= IPOPT_MINOFF;
	opt += (IPOPT_OFFSET + 1);
	len -= (IPOPT_OFFSET + 1);


	while (len > 0) {
		line = get_line((char *) &(opt) - dlc_header, 4);
		memcpy((char *)&addr, opt, sizeof (addr));
		if (addr.s_addr == INADDR_ANY)
			(void) strcpy(line, "      -");
		else
			(void) sprintf(line, "      %s",
				addrtoname(addr));
		if (pointer == 0)
			(void) strcat(line, "  <-- (current)");

		opt += sizeof (addr);
		len -= sizeof (addr);
		pointer -= sizeof (addr);
	}

}

char *
getproto(p)
	int p;
{
	switch (p) {
	case IPPROTO_IP:	return ("IP");
	case IPPROTO_ICMP:	return ("ICMP");
	case IPPROTO_IGMP:	return ("IGMP");
	case IPPROTO_GGP:	return ("GGP");
	case IPPROTO_TCP:	return ("TCP");
	case IPPROTO_EGP:	return ("EGP");
	case IPPROTO_PUP:	return ("PUP");
	case IPPROTO_UDP:	return ("UDP");
	case IPPROTO_IDP:	return ("IDP");
	case IPPROTO_HELLO:	return ("HELLO");
	case IPPROTO_ND:	return ("ND");
	case IPPROTO_RAW:	return ("RAW");
	default:		return ("");
	}
}
