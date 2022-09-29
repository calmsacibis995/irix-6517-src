/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_tcp.c,v 1.3 1998/11/25 20:09:56 jlan Exp $"

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
#include <netinet/tcp.h>
#include "snoop.h"

extern char *dlc_header;
char *getportname();
char *tohex();

void
interpret_tcp(flags, tcp, iplen, fraglen)
	int flags;
	struct tcphdr *tcp;
	int iplen, fraglen;
{
	char *data;
	int hdrlen, tcplen;
	int sunrpc = 0;
	char *pname;
	char buff[32];
	char *line;
	void print_tcpoptions();

	hdrlen = tcp->th_off * 4;
	data = (char *)tcp + hdrlen;
	tcplen = iplen - hdrlen;
	fraglen -= hdrlen;
	if (fraglen < 0)
		return;		/* incomplete header */
	if (fraglen > tcplen)
		fraglen = tcplen;

	if (flags & F_SUM) {
		line = (char *) get_sum_line();
		(void) sprintf(line,
			"TCP D=%d S=%d",
			ntohs(tcp->th_dport),
			ntohs(tcp->th_sport));
		line += strlen(line);

		if (tcp->th_flags & TH_SYN)
			(void) strcpy(line, " Syn");
		if (tcp->th_flags & TH_FIN)
			(void) strcpy(line, " Fin");
		if (tcp->th_flags & TH_RST)
			(void) strcpy(line, " Rst");
		if (!(tcp->th_flags & (TH_SYN | TH_FIN | TH_RST)))
			(void) strcpy(line, "    ");
		line += strlen(line);
		if (tcp->th_flags & TH_ACK) {
			(void) sprintf(line, " Ack=%u",
				ntohl(tcp->th_ack));
			line += strlen(line);
		}
		if (ntohl(tcp->th_seq)) {
			(void) sprintf(line, " Seq=%u Len=%d",
				ntohl(tcp->th_seq), tcplen);
			line += strlen(line);
		}
		(void) sprintf(line, " Win=%d", ntohs(tcp->th_win));
	}


	sunrpc = !reservedport(IPPROTO_TCP, ntohs(tcp->th_dport)) &&
		!reservedport(IPPROTO_TCP, ntohs(tcp->th_sport)) &&
		valid_rpc(data + 4, fraglen - 4);

	if (flags & F_DTAIL) {

	show_header("TCP:  ", "TCP Header", tcplen);
	show_space();
	(void) sprintf(get_line((char *)tcp->th_sport - dlc_header, 2),
		"Source port = %d",
		ntohs(tcp->th_sport));

	if (sunrpc) {
		pname = "(Sun RPC)";
	} else {
		pname = getportname(IPPROTO_TCP, ntohs(tcp->th_dport));
		if (pname == NULL) {
			pname = "";
		} else {
			(void) sprintf(buff, "(%s)", pname);
			pname = buff;
		}
	}
	(void) sprintf(get_line((char *)tcp->th_dport - dlc_header, 2),
		"Destination port = %d %s",
		ntohs(tcp->th_dport), pname);
	(void) sprintf(get_line((char *)tcp->th_seq - dlc_header, 4),
		"Sequence number = %u",
		ntohl(tcp->th_seq));
	(void) sprintf(get_line((char *)tcp->th_ack - dlc_header, 4),
		"Acknowledgement number = %u",
		ntohl(tcp->th_ack));
	(void) sprintf(get_line(((char *)tcp->th_ack - dlc_header) + 4, 1),
		"Data offset = %d bytes",
		tcp->th_off * 4);
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"Flags = 0x%02x",
		tcp->th_flags);
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_URG,
			"Urgent pointer", "No urgent pointer"));
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_ACK,
			"Acknowledgement", "No acknowledgement"));
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_PUSH,
			"Push", "No push"));
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_RST,
			"Reset", "No reset"));
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_SYN,
			"Syn", "No Syn"));
	(void) sprintf(get_line(((char *)tcp->th_flags - dlc_header) + 4, 1),
		"      %s",
		getflag(tcp->th_flags, TH_FIN,
			"Fin", "No Fin"));
	(void) sprintf(get_line(((char *)tcp->th_win - dlc_header) + 4, 1),
		"Window = %d",
		ntohs(tcp->th_win));
	/* XXX need to compute checksum and print whether correct */
	(void) sprintf(get_line(((char *)tcp->th_sum - dlc_header) + 4, 1),
		"Checksum = 0x%04x",
		ntohs(tcp->th_sum));
	(void) sprintf(get_line(((char *)tcp->th_urp - dlc_header) + 4, 1),
		"Urgent pointer = %d",
		ntohs(tcp->th_urp));

	/* Print TCP options - if any */

	print_tcpoptions(tcp + 1, tcp->th_off * 4 - sizeof (struct tcphdr));

	show_space();
	}

	/* go to the next protocol layer */

	if (!interpret_reserved(flags, IPPROTO_TCP,
		ntohs(tcp->th_sport),
		ntohs(tcp->th_dport),
		data, fraglen)) {
		if (sunrpc && fraglen > 0)
			interpret_rpc(flags, data, fraglen, IPPROTO_TCP);
	}

	return;
}

void
print_tcpoptions(opt, optlen)
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
		case TCPOPT_EOL:
			(void) strcpy(line, "  - End of option list");
			return;
		case TCPOPT_NOP:
			(void) strcpy(line, "  - No operation");
			len = 1;
			break;
		case TCPOPT_MAXSEG:
			(void) sprintf(line,
			"  - Maximum segment size = %d bytes",
				(opt[2] << 8) + opt[3]);
			break;
		default:
			(void) sprintf(line,
			"  - Option %d (unknown - %d bytes) %s",
				opt[0],
				len - 2,
				tohex(&opt[2], len - 2));
			break;
		}
		opt += len;
		optlen -= len;
	}
}
