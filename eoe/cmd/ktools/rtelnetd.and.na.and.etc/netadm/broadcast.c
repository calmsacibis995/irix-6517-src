/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Description::
 *
 * 	%$(Description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Module Reviewers:
 *
 *	%$(reviewers)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/broadcast.c,v 1.3 1996/10/04 12:11:32 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/broadcast.c,v $
 *
 * Revision History:
 *
 * $Log: broadcast.c,v $
 * Revision 1.3  1996/10/04 12:11:32  cwilson
 * latest rev
 *
 * Revision 1.8  1991/04/09  00:10:43  emond
 * Accommodate generic TLI interface
 *
 * Revision 1.7  89/04/05  12:44:13  loverso
 * Changed copyright notice
 * 
 * Revision 1.6  88/05/24  18:40:04  parker
 * Changes for new install-annex script
 * 
 * Revision 1.5  88/05/04  23:13:47  harris
 * Use rpc() interface.
 * 
 * Revision 1.4  88/04/15  12:35:46  mattes
 * SL/IP integration
 * 
 * Revision 1.3  87/06/10  18:09:23  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 1.2  86/12/03  16:35:27  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 1.1  86/05/07  11:17:31  goodmon
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:11:32 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/broadcast.c,v 1.3 1996/10/04 12:11:32 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"

#include "../inc/erpc/netadmp.h"
#include "../inc/courier/courier.h"
#include "netadm.h"
#include "netadm_err.h"

/* External Data Declarations */


/* Defines and Macros */

#define OUTGOING_COUNT  3

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


broadcast(Pinet_addr, device_type, line_number, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         device_type;
    u_short         line_number;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one,
		    param_two,
    	            string_length;

    char            param_three[MESSAGE_LENGTH + sizeof(u_short)];

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the device_type.
       outgoing[2] contains the line_number.
       outgoing[3] contains the message. */

    param_one = htons(device_type);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(line_number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    string_length = *(u_short *)Pdata;
    *(u_short *)param_three = htons(string_length);
    (void)bcopy(&Pdata[sizeof(u_short)], &param_three[sizeof(u_short)],
     (int)string_length);
    outgoing[3].iov_base = (caddr_t)&param_three[0];
    outgoing[3].iov_len = sizeof(u_short) + string_length;

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_BCAST_TO_PORT, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}   /* broadcast() */
