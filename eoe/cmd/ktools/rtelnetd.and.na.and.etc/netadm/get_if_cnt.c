/******************************************************************************
*
*        Copyright 1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
*
* ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
* This software is made available solely pursuant to the terms of a
* software license agreement which governs its use. 
* Unauthorized duplication, distribution or sale are strictly prohibited.
*
* Configured System: src/netadm
*
* Original Author: David Wang	Created on: Mar 22, 1993
*
* Revision Control Information:
* $Id: get_if_cnt.c,v 1.2 1996/10/04 12:12:31 cwilson Exp $
*
* This file created by RCS from
* $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_if_cnt.c,v $
*
* Revision History:
* $Log: get_if_cnt.c,v $
* Revision 1.2  1996/10/04 12:12:31  cwilson
* latest rev
*
 * Revision 1.1  1993/03/24  15:11:47  wang
 * Initial revision
 *
* This file is currently under revision by: $Locker:  $
*
*****************************************************************************
*/
#define RCSDATE $Date: 1996/10/04 12:12:31 $
#define RCSREV  $Revision: 1.2 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_if_cnt.c,v 1.2 1996/10/04 12:12:31 cwilson Exp $"

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


get_interface_count(Pinet_addr, type, Pdata)
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

    return rpc(Pinet_addr, RPROC_GET_IFS, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_interface_count() */
