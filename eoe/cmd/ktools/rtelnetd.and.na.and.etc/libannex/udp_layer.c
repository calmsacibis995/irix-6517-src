/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	UDP Layer of Xenix UDP SL/IP
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/udp_layer.c,v 1.3 1996/10/04 12:09:36 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/udp_layer.c,v $
 *
 * Revision History:
 *
 * $Log: udp_layer.c,v $
 * Revision 1.3  1996/10/04 12:09:36  cwilson
 * latest rev
 *
 * Revision 1.6  1994/08/04  10:59:14  sasson
 * SPR 3211: "debug" variable defined in multiple places.
 *
 * Revision 1.5  1993/12/30  13:14:35  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.4  1989/04/05  12:40:27  loverso
 * Changed copyright notice
 *
 * Revision 1.3  88/05/31  17:12:47  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:36:06  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:59:30  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:09:36 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/udp_layer.c,v 1.3 1996/10/04 12:09:36 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include "../inc/config.h"
#include "port/port.h"
#include "../inc/slip/slip_user.h"
#include "../inc/slip/slip_system.h"


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

extern int _network_mtu;
extern UINT32 _my_inet_address;
extern u_short _in_cksum();
extern int debug;


/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/

int _udp_input(iobuf, cc, buf, dport, from)
struct sockiobuf *iobuf;
int *cc;			/* Returned, less the UDP header */
char **buf;
int dport;
struct sockaddr_in *from;	/* Returned with src port filled in */
{
    int error;
    struct udpiphdr *u;

    while(1) {
	error = _ip_input(iobuf, cc, buf, from, dport);
	if(error)
	    return(error);
	u = (struct udpiphdr *)*buf;

	/* Verify the essentials of the header */
	if(*cc < sizeof(*u)) {
	    if(debug)
		fprintf(stderr, "_udp_input: pkt too small: %d\n", *cc);
	    continue;
	    }
	else if(u->ui_pr != IPPROTO_UDP) {
	    if(debug)
		fprintf(stderr, "_udp_input: unknown protocol: %02x\n",
			u->ui_pr);
	    continue;
	    }

	/* Verify the destination */
	else if(u->ui_dport != dport) {
	    if(debug)
		fprintf(stderr, "_udp_input: wrong dst port: %d, want %d\n",
		    ntohs(u->ui_dport), ntohs(dport));
	    continue;
	    }

	/* Verify the source */
	else if(from->sin_port && (u->ui_sport != from->sin_port)) {
	    if(debug)
		fprintf(stderr, "_udp_input: wrong src port: %d, want %d\n",
		    ntohs(u->ui_sport), ntohs(from->sin_port));
	    continue;
	    }

	else
	    break;
	}

    from->sin_port = u->ui_sport;
    *cc -= sizeof(*u);
    *buf += sizeof(*u);
    return(0);
    }


int _udp_output(iobuf, sport, to)
struct sockiobuf *iobuf;
int sport;
struct sockaddr_in *to;
{
    struct udpiphdr *u;
    int error;

    if(iobuf->sb_len + sizeof(*u) > _network_mtu) {
	if(debug)
	    fprintf(stderr,
                    "_udp_output: msg too long: %d, mtu is %d, hdr is %d\n",
		    iobuf->sb_len, _network_mtu, sizeof(*u));
	return(EMSGSIZE);
	}

    iobuf->sb_curr -= sizeof(*u);
    u = (struct udpiphdr *)iobuf->sb_curr;
    bzero(iobuf->sb_curr, sizeof(*u));

    u->ui_next = u->ui_prev = 0;
    u->ui_x1 = 0;
    u->ui_pr = IPPROTO_UDP;
    u->ui_len = htons(iobuf->sb_len + sizeof(struct udphdr));
    u->ui_src.s_addr = _my_inet_address;
    u->ui_dst.s_addr = to->sin_addr.s_addr;
    u->ui_sport = sport;
    u->ui_dport = to->sin_port;
    u->ui_ulen = u->ui_len;
    u->ui_sum = 0;
    u->ui_sum = _in_cksum((u_char *)u, iobuf->sb_len + sizeof(*u));

    iobuf->sb_len += sizeof(*u);
    error = _ip_output(iobuf);
    if(error)
	return(error);
    return(0);
    }
