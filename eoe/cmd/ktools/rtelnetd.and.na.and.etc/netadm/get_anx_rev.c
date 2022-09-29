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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_rev.c,v 1.3 1996/10/04 12:12:04 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_rev.c,v $
 *
 * Revision History:
 *
 * $Log: get_anx_rev.c,v $
 * Revision 1.3  1996/10/04 12:12:04  cwilson
 * latest rev
 *
 * Revision 1.7  1991/04/09  00:11:21  emond
 * Accommodate generic TLI interface
 *
 * Revision 1.6  89/04/05  12:44:15  loverso
 * Changed copyright notice
 * 
 * Revision 1.5  88/05/24  18:40:11  parker
 * Changes for new install-annex script
 * 
 * Revision 1.4  88/05/04  23:14:39  harris
 * Use rpc() interface.
 * 
 * Revision 1.3  88/04/15  12:36:02  mattes
 * SL/IP integration
 * 
 * Revision 1.2  87/06/10  18:09:27  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 1.1  86/12/08  12:06:16  parker
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:12:04 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_anx_rev.c,v 1.3 1996/10/04 12:12:04 cwilson Exp $"
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


get_annex_rev(Pinet_addr, type, Pdata)
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

    return rpc(Pinet_addr, RPROC_GET_REV, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_annex_rev() */
