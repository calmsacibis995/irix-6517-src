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
 * (Include file) OR (Module) description:
 *	%$(description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_opt.c,v 1.3 1996/10/04 12:11:56 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_opt.c,v $
 *
 * Revision History:
 *
 * $Log: get_anx_opt.c,v $
 * Revision 1.3  1996/10/04 12:11:56  cwilson
 * latest rev
 *
 * Revision 1.2  1991/04/09  00:11:10  emond
 * Accommodate generic TLI interface
 *
 * Revision 1.1  90/12/14  14:42:25  raison
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE: 	$Date: 1996/10/04 12:11:56 $
 *  REVISION:	$Revision: 1.3 $
 *
 ****************************************************************************
 */
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_opt.c,v 1.3 1996/10/04 12:11:56 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#ifndef TLI
#include <sys/socket.h>
#endif

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


get_annex_opt(Pinet_addr, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return (NAE_ADDR);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return(rpc(Pinet_addr,RPROC_GET_OPTS,OUTGOING_COUNT,outgoing,Pdata,type));

}   /* get_annex_opt() */
