/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_icmp.c,v 1.2 1996/07/06 17:50:25 nn Exp $"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "snoop.h"

void
interpret_icmp(flags, ip, iplen, ilen)
	struct icmp *ip;
	int iplen, ilen;
{
	char *pt, *pc;
	char *line;
	char buff[64];
	extern char *prot_nest_prefix;

	if (ilen < sizeof (struct icmp))
		return;

	pt = "Unknown";
	pc = "";

	switch (ip->icmp_type) {
	case ICMP_ECHOREPLY:
		pt = "Echo reply";
		break;
	case ICMP_UNREACH:
		pt = "Destination unreachable";
		switch (ip->icmp_code) {
		case ICMP_UNREACH_NET:
			pc = "Bad net";
			break;
		case ICMP_UNREACH_HOST:
			pc = "Bad host";
			break;
		case ICMP_UNREACH_PROTOCOL:
			pc = "Bad protocol";
			break;
		case ICMP_UNREACH_PORT:
			pc = "Bad port";
			break;
		case ICMP_UNREACH_NEEDFRAG:
			pc = "Needed to fragment";
			break;
		case ICMP_UNREACH_SRCFAIL:
			pc = "Source route failed";
			break;
		default:
			break;
		}
		break;
	case ICMP_SOURCEQUENCH:
		pt = "Packet lost, slow down";
		break;
	case ICMP_REDIRECT:
		pt = "Redirect";
		switch (ip->icmp_code) {
		case ICMP_REDIRECT_NET:
			pc = "for network";
			break;
		case ICMP_REDIRECT_HOST:
			pc = "for host";
			break;
		case ICMP_REDIRECT_TOSNET:
			pc = "for tos and net";
			break;
		case ICMP_REDIRECT_TOSHOST:
			pc = "for tos and host";
			break;
		default:
			break;
		}
		(void) sprintf(buff, "%s to %s",
			pc, addrtoname(ip->icmp_gwaddr));
		pc = buff;
		break;
	case ICMP_ECHO:
		pt = "Echo request";
		break;
	case ICMP_TIMXCEED:
		pt = "Time exceeded";
		switch (ip->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			pc = "in transit";
			break;
		case ICMP_TIMXCEED_REASS:
			pc = "in reassembly";
			break;
		default:
			break;
		}
		break;
	case ICMP_PARAMPROB:
		pt = "IP header bad";
		break;
	case ICMP_TSTAMP:
		pt = "Timestamp request";
		break;
	case ICMP_TSTAMPREPLY:
		pt = "Timestamp reply";
		break;
	case ICMP_IREQ:
		pt = "Information request";
		break;
	case ICMP_IREQREPLY:
		pt = "Information reply";
		break;
	case ICMP_MASKREQ:
		pt = "Address mask request";
		break;
	case ICMP_MASKREPLY:
		pt = "Address mask reply";
		break;
	default:
		break;
	}

	if (flags & F_SUM) {
		line = get_sum_line();
		if (*pc)
			(void) sprintf(line, "ICMP %s (%s)", pt, pc);
		else
			(void) sprintf(line, "ICMP %s", pt);
	}

	if (flags & F_DTAIL) {
		show_header("ICMP:  ", "ICMP Header", ilen);
		show_space();
		(void) sprintf(get_line(0, 0),
			"Type = %d (%s)",
			ip->icmp_type, pt);
		if (*pc)
			(void) sprintf(get_line(0, 0),
				"Code = %d (%s)",
				ip->icmp_code, pc);
		else
			(void) sprintf(get_line(0, 0),
				"Code = %d",
				ip->icmp_code);
		(void) sprintf(get_line(0, 0),
			"Checksum = %x",
			ntohs(ip->icmp_cksum));

		if (ip->icmp_type == ICMP_UNREACH ||
		    ip->icmp_type == ICMP_REDIRECT) {
			if (ilen > 28) {
				show_space();
				(void) sprintf(get_line(0, 0),
				"[ subject header follows ]");
				show_space();
				prot_nest_prefix = "ICMP:";
				interpret_ip(flags, ip->icmp_data, 28);
				prot_nest_prefix = "";
			}
		}
		show_space();
	}
	/*
	 * XXX  Need to display extra info for PARAMPROB
	 */
}
