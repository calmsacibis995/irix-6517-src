
/*
 *****************************************************************************
 *
 *        Copyright 1993 Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 * 	Blah Blah Blah
 *
 * Original Author: Owain Phillips      Created on: November 16th 1993
 *
 * Revision Control Information:
 *
 * $Id: get_sy_param.c,v 1.2 1996/10/04 12:13:11 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_sy_param.c,v $
 *
 * Revision History:
 *
 * $Log: get_sy_param.c,v $
 * Revision 1.2  1996/10/04 12:13:11  cwilson
 * latest rev
 *
 * Revision 1.1  1994/01/06  15:37:42  wang
 * Initial revision
 *
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */


#define RCSDATE $Date: 1996/10/04 12:13:11 $
#define RCSREV  $Revision: 1.2 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_sy_param.c,v 1.2 1996/10/04 12:13:11 cwilson Exp $"
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

#define OUTGOING_COUNT  4

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


get_sy_param(Pinet_addr, device_type, line_number, cat, number, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         device_type;
    u_short         line_number;
    u_short         cat;
    u_short         number;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one,
                    param_two,
                    param_three,
                    param_four;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the device_type.
       outgoing[2] contains the sync line_number.
       outgoing[3] contains the catagory.
       outgoing[4] contains the sync line param number. */

    param_one = htons(device_type);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(line_number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    param_three = htons(cat);
    outgoing[3].iov_base = (caddr_t)&param_three;
    outgoing[3].iov_len = sizeof(param_three);

    param_four = htons(number);
    outgoing[4].iov_base = (caddr_t)&param_four;
    outgoing[4].iov_len = sizeof(param_four);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_GET_SYNC_PARAM, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_sy_param() */
