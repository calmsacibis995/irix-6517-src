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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_da_param.c,v 1.3 1996/10/04 12:12:14 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_da_param.c,v $
 *
 * Revision History:
 *
 * $Log: get_da_param.c,v $
 * Revision 1.3  1996/10/04 12:12:14  cwilson
 * latest rev
 *
 * Revision 2.8  1991/04/09  00:11:33  emond
 * Accommodate generic TLI interface
 *
 * Revision 2.7  89/04/05  12:44:16  loverso
 * Changed copyright notice
 * 
 * Revision 2.6  88/05/24  18:40:15  parker
 * Changes for new install-annex script
 * 
 * Revision 2.5  88/05/04  23:15:13  harris
 * Use rpc() interface.
 * 
 * Revision 2.4  88/04/15  12:36:10  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:29  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:36:38  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:18:39  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:38:27  parker
 * First development revision for release 2.
 * 
 * Revision 1.1  85/11/01  17:46:35  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:12:14 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_da_param.c,v 1.3 1996/10/04 12:12:14 cwilson Exp $"
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

#define OUTGOING_COUNT  2

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


get_dla_param(Pinet_addr, cat, number, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short	    cat;
    u_short         number;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one,
                    param_two;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the catagory.
       outgoing[2] contains the dla param number. */

    param_one = htons(cat);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_GET_DLA_PARAM, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_dla_param() */
