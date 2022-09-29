/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_rip.c,v 1.1 1996/06/19 20:34:08 nn Exp $"

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/routed.h>
#include "snoop.h"

#ifdef sgi
/*
 * Things in the Solaris routed.h but not in the IRIX 6.2 routed.h
 */
#define	RIPCMD_POLLENTRY	6

struct entryinfo {
	struct	sockaddr rtu_dst;
	struct	sockaddr rtu_router;
	short	rtu_flags;
	short	rtu_state;
	int	rtu_timer;
	int	rtu_metric;
	int	int_flags;
	char	int_name[16];
};
#endif

extern char *dlc_header;
char *show_cmd();
char *addrtoname();

interpret_rip(flags, rip, fraglen)
	int flags;
	struct rip *rip;
	int fraglen;
{
	char *p;
	struct netinfo *n;
	struct entryinfo *ep;
	int len, count;
	struct sockaddr_in *sin;

	if (flags & F_SUM) {
		switch (rip->rip_cmd) {
		case RIPCMD_REQUEST:	p = "C";		break;
		case RIPCMD_RESPONSE:	p = "R";		break;
		case RIPCMD_TRACEON:	p = "Traceon";		break;
		case RIPCMD_TRACEOFF:	p = "Traceoff";		break;
		case RIPCMD_POLL:	p = "Poll";		break;
		case RIPCMD_POLLENTRY:	p = "Poll entry";	break;
		default: p = "?"; break;
		}

		switch (rip->rip_cmd) {
		case RIPCMD_REQUEST:
		case RIPCMD_RESPONSE:
		case RIPCMD_POLL:
			len = fraglen - 4;
			count = 0;
			for (n = rip->rip_nets;
				len >= sizeof (struct netinfo); n++) {
				count++;
				len -= sizeof (struct netinfo);
			}
			(void) sprintf(get_sum_line(),
					"RIP %s (%d destinations)",
				p, count);
			break;
		case RIPCMD_TRACEON:
		case RIPCMD_TRACEOFF:
			(void) sprintf(get_sum_line(), "RIP %s File=\"%s\"",
				p, rip->rip_tracefile);
			break;
		default:
			(void) sprintf(get_sum_line(), "RIP %s", p);
			break;
		}
	}

	if (flags & F_DTAIL) {

	show_header("RIP:  ", "Routing Information Protocol", fraglen);
	show_space();
	(void) sprintf(get_line((char *)rip->rip_cmd - dlc_header, 1),
		"Opcode = %d (%s)",
		rip->rip_cmd,
		show_cmd(rip->rip_cmd));
	(void) sprintf(get_line((char *)rip->rip_vers - dlc_header, 1),
		"Version = %d",
		rip->rip_vers);

	switch (rip->rip_cmd) {
	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
	case RIPCMD_POLL:
		show_space();
		(void) sprintf(get_line(0, 0),
			"Address                        Port   Metric");
		len = fraglen - 4;
		for (n = rip->rip_nets; len >= sizeof (struct netinfo); n++) {
			if (rip->rip_vers > 0) {
				n->rip_dst.sa_family =
					ntohs(n->rip_dst.sa_family);
				n->rip_metric = ntohl((u_long) n->rip_metric);
			}
			sin = (struct sockaddr_in *) &n->rip_dst;
			(void) sprintf(
				get_line((char *) n - dlc_header,
					sizeof (struct netinfo)),
				"%-16s%-16s %d     %d%s",
				inet_ntoa(sin->sin_addr),
				sin->sin_addr.s_addr == htonl(INADDR_ANY) ?
				"(default)" : addrtoname(sin->sin_addr),
				htons(sin->sin_port) & 0xFFFF,
				n->rip_metric,
				n->rip_metric == 16 ? " (not reachable)":"");
			len -= sizeof (struct netinfo);
		}
		break;

	case RIPCMD_POLLENTRY:
		if (fraglen - 4 < sizeof (struct entryinfo))
			break;
		ep = (struct entryinfo *) (rip->rip_tracefile);
		sin = (struct sockaddr_in *) &ep->rtu_dst;
		(void) sprintf(
			get_line((char *) sin - dlc_header,
				sizeof (struct sockaddr)),
			"Destination = %s %s",
			inet_ntoa(sin->sin_addr),
			addrtoname(sin->sin_addr));
		sin = (struct sockaddr_in *) &ep->rtu_router;
		(void) sprintf(
			get_line((char *) sin - dlc_header,
				sizeof (struct sockaddr)),
			"Router      = %s %s",
			inet_ntoa(sin->sin_addr),
			addrtoname(sin->sin_addr));
		(void) sprintf(
			get_line((char *) &ep->rtu_flags - dlc_header, 2),
			"Flags = %4x",
			ep->rtu_flags);
		(void) sprintf(
			get_line((char *) &ep->rtu_state - dlc_header, 2),
			"State = %d",
			ep->rtu_state);
		(void) sprintf(
			get_line((char *) &ep->rtu_timer - dlc_header, 4),
			"Timer = %d",
			ep->rtu_timer);
		(void) sprintf(
			get_line((char *) &ep->rtu_metric - dlc_header, 4),
			"Metric = %d",
			ep->rtu_metric);
		(void) sprintf(
			get_line((char *) &ep->int_flags - dlc_header, 4),
			"Int flags = %8x",
			ep->int_flags);
		(void) sprintf(
			get_line((char *) ep->int_name - dlc_header, 16),
			"Int name = \"%s\"",
			ep->int_name);
		break;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
		(void) sprintf(
			get_line((char *)rip->rip_tracefile - dlc_header, 2),
			"Trace file = %s",
			rip->rip_tracefile);
		break;
	}
	}

	return (fraglen);
}

char *
show_cmd(c)
	int c;
{
	switch (c) {
	case RIPCMD_REQUEST:	return ("route request");
	case RIPCMD_RESPONSE:	return ("route response");
	case RIPCMD_TRACEON:	return ("route trace on");
	case RIPCMD_TRACEOFF:	return ("route trace off");
	case RIPCMD_POLL:	return ("route poll");
	case RIPCMD_POLLENTRY:	return ("route poll entry");
	}
	return ("?");
}
