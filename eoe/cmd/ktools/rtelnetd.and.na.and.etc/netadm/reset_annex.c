/******************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *  %$(Description)$%
 *
 * Original Author: %$(author)$%    Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_annex.c,v 1.3 1996/10/04 12:13:56 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_annex.c,v $
 *
 * Revision History:
 *
 * $Log: reset_annex.c,v $
 * Revision 1.3  1996/10/04 12:13:56  cwilson
 * latest rev
 *
 * Revision 1.3  1991/04/09  00:13:31  emond
 * Accommodate generic TLI interface
 *
 * Revision 1.2  89/04/05  12:44:30  loverso
 * Changed copyright notice
 * 
 * Revision 1.1  88/06/01  16:36:37  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:13:56 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_annex.c,v 1.3 1996/10/04 12:13:56 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"
#include "netadm.h"
#include "netadm_err.h"

/* External Data Declarations */


/* Defines and Macros */

#define OUTGOING_COUNT  1

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


reset_annex(Pinet_addr, subsystem)
    struct sockaddr_in *Pinet_addr;
    u_short         subsystem;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the subsystem to reset. */

    param_one = htons(subsystem);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_RESET_ANNEX, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}   /* reset_annex() */
