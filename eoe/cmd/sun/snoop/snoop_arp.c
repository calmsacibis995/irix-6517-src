/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_arp.c,v 1.2 1996/07/06 17:46:53 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include "snoop.h"

#ifdef sgi
#define	REVARP_REQUEST	3
#define	REVARP_REPLY	4
#endif

extern char *dlc_header;
extern jmp_buf xdr_err;

char *printip();
char *printether();
char *addrtoname_align();

char *opname[] = {
	"",
	"ARP Request",
	"ARP Reply",
	"REVARP Request",
	"REVARP Reply",
};

void
interpret_arp(flags, ap, alen)
	struct ether_arp *ap;
	int alen;
{
	char *line;
	extern char *src_name, *dst_name;

	line = get_sum_line();

	src_name = addrtoname_align(ap->arp_spa);

	if (flags & F_SUM) {
		switch (ntohs(ap->arp_op)) {
		case ARPOP_REQUEST:
			(void) sprintf(line, "ARP C Who is %s ?",
				printip(ap->arp_tpa));
			break;
		case ARPOP_REPLY:
			(void) sprintf(line, "ARP R %s is %s",
				printip(ap->arp_spa),
				printether(&ap->arp_sha));
			dst_name = addrtoname_align(ap->arp_tpa);
			break;
		case REVARP_REQUEST:
			(void) sprintf(line, "RARP C Who is %s ?",
				printether(&ap->arp_tha));
			break;
		case REVARP_REPLY:
			(void) sprintf(line, "RARP R %s is %s",
				printether(&ap->arp_tha),
				printip(ap->arp_tpa));
			dst_name = addrtoname_align(ap->arp_tpa);
			break;
		}
	}

	if (flags & F_DTAIL) {
		show_header("ARP:  ", "ARP/RARP Frame", alen);
		show_space();
		(void) sprintf(get_line(0, 0),
			"Hardware type = %d",
			ap->arp_hrd);
		(void) sprintf(get_line(0, 0),
			"Protocol type = %04x (%s)",
			ntohs(ap->arp_pro),
			print_ethertype(ntohs(ap->arp_pro)));
		(void) sprintf(get_line(0, 0),
			"Length of hardware address = %d bytes",
			ap->arp_hln);
		(void) sprintf(get_line(0, 0),
			"Length of protocol address = %d bytes",
			ap->arp_pln);
		(void) sprintf(get_line(0, 0),
			"Opcode %d (%s)",
			ntohs(ap->arp_op),
			opname[ntohs(ap->arp_op)]);

		if (ntohs(ap->arp_hrd) == ARPHRD_ETHER &&
		    ntohs(ap->arp_pro) == ETHERTYPE_IP) {
			(void) sprintf(get_line(0, 0),
				"Sender's hardware address = %s",
				printether(&ap->arp_sha));
			(void) sprintf(get_line(0, 0),
				"Sender's protocol address = %s",
				printip(ap->arp_spa));
			(void) sprintf(get_line(0, 0),
				"Target hardware address = %s",
				ntohs(ap->arp_op) == ARPOP_REQUEST ? "?" :
				printether(&ap->arp_tha));
			(void) sprintf(get_line(0, 0),
				"Target protocol address = %s",
				ntohs(ap->arp_op) == REVARP_REQUEST ? "?" :
				printip(ap->arp_tpa));
		}
		show_trailer();
	}
}

char *
printip(p)
	unsigned char *p;
{
	static char buff[128];
	char *ap, *np;
	struct in_addr a;

	memcpy(&a, p, 4);
	ap = (char *) inet_ntoa(a);
	np = (char *) addrtoname(a);
	(void) sprintf(buff, "%s, %s", ap, np);
	return (buff);
}

char *
addrtoname_align(p)
	unsigned char *p;
{
	struct in_addr a;

	memcpy(&a, p, 4);
	return ((char *) addrtoname(a));
}
