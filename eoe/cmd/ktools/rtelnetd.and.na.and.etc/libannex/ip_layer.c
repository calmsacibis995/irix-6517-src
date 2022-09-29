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
 *	IP layer of Xenix UDP SL/IP
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/ip_layer.c,v 1.3 1996/10/04 12:08:47 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/ip_layer.c,v $
 *
 * Revision History:
 *
 * $Log: ip_layer.c,v $
 * Revision 1.3  1996/10/04 12:08:47  cwilson
 * latest rev
 *
 * Revision 1.6  1994/08/04  10:57:47  sasson
 * SPR 3211: "debug" variable defined in multiple places.
 *
 * Revision 1.5  1993/12/30  13:14:04  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.4  1989/04/05  12:40:18  loverso
 * Changed copyright notice
 *
 * Revision 1.3  88/05/31  17:12:21  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:35:48  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:57:56  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:08:47 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/ip_layer.c,v 1.3 1996/10/04 12:08:47 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
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
 * Local data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/

int _ip_input(iobuf, cc, buf, from, port)
struct sockiobuf *iobuf;
int *cc;
char **buf;
struct sockaddr_in *from;	/* Returned with src addr filled in */
int port;
{
    int error;
    struct ip *i;

    while(1) {
	error = _sl_input(iobuf, cc, buf, port);
	if(error)
	    return(error);
	i = (struct ip *)*buf;

	/* Verify the packet essentials */
	if(*cc < sizeof(*i)) {
	    if(debug)
		fprintf(stderr, "_ip_input: packet too small: %d\n", *cc);
	    continue;
	    }
	else if(ntohs(i->ip_len) != *cc) {
	    if(debug)
		fprintf(stderr, "_ip_input: ip_len = %d, packet len = %d\n",
			ntohs(i->ip_len), *cc);
		continue;
		}
	else if(_in_cksum((u_char *)i, sizeof(*i))) {
	    if(debug)
		fprintf(stderr, "_ip_input: header checksum error\n");
	    continue;
	    }

	/* Verify the destination */
	else if(i->ip_dst.s_addr != _my_inet_address) {
	    if(debug)
		fprintf(stderr, "_ip_input: wrong dest: %lx, want %lx\n",
		    i->ip_dst.s_addr, _my_inet_address);
	    continue;
	    }

	/* Verify the source */
	else if(from->sin_addr.s_addr &&
		(i->ip_src.s_addr != from->sin_addr.s_addr)) {
	    if(debug)
		fprintf(stderr, "_ip_input: wrong src: %lx, want %lx\n",
		    i->ip_src.s_addr, from->sin_addr.s_addr);
	    continue;
	    }

	else
	    break;
	}

    from->sin_addr.s_addr = i->ip_src.s_addr;
    return(0);
    }


int _ip_output(iobuf)
struct sockiobuf *iobuf;
{
    struct ip *i;
    int error;

    if(debug)
    	fprintf(stderr, "_ip_output\n");

    i = (struct ip *)iobuf->sb_curr;

    i->ip_hl = sizeof(*i) >> 2;
    i->ip_v = IPVERSION;
    i->ip_len = htons(iobuf->sb_len);
    i->ip_ttl = MAXTTL;
    i->ip_off = 0;
    i->ip_id = 0;
    i->ip_tos = 0;
    i->ip_sum = 0;
    i->ip_sum = _in_cksum((u_char *)i, sizeof(*i));

    error = _sl_output(iobuf);
    if(error)
	return(error);
    return(0);
    }
