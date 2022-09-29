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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_port_cnt.c,v 1.3 1996/10/04 12:12:57 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_port_cnt.c,v $
 *
 * Revision History:
 *
 * $Log: get_port_cnt.c,v $
 * Revision 1.3  1996/10/04 12:12:57  cwilson
 * latest rev
 *
 * Revision 1.6  1991/04/09  00:12:13  emond
 * Accommodate generic TLI interface
 *
 * Revision 1.5  89/04/05  12:44:21  loverso
 * Changed copyright notice
 * 
 * Revision 1.4  88/05/24  18:40:30  parker
 * Changes for new install-annex script
 * 
 * Revision 1.3  88/05/04  23:17:36  harris
 * Use rpc() interface.
 * 
 * Revision 1.2  88/04/15  12:37:43  mattes
 * SL/IP integration
 * 
 * Revision 1.1  87/08/20  10:37:21  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:12:57 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_port_cnt.c,v 1.3 1996/10/04 12:12:57 cwilson Exp $"
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

#define OUTGOING_COUNT  0

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


get_port_count(Pinet_addr, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
     *  outgoing[0] is only used by erpc_callresp().
     */

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_GET_PORTS, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_port_count() */
