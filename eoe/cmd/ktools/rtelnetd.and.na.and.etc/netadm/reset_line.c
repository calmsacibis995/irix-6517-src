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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_line.c,v 1.3 1996/10/04 12:14:04 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_line.c,v $
 *
 * Revision History:
 *
 * $Log: reset_line.c,v $
 * Revision 1.3  1996/10/04 12:14:04  cwilson
 * latest rev
 *
 * Revision 2.8  1991/04/09  00:13:44  emond
 * Accommodate generic TLI interface
 *
 * Revision 2.7  89/04/05  12:44:31  loverso
 * Changed copyright notice
 * 
 * Revision 2.6  88/05/24  18:40:47  parker
 * Changes for new install-annex script
 * 
 * Revision 2.5  88/05/04  23:23:29  harris
 * Use rpc() interface.
 * 
 * Revision 2.4  88/04/15  12:38:31  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:40  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:38:47  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:20:49  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:12  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:51:48  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:14:04 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/reset_line.c,v 1.3 1996/10/04 12:14:04 cwilson Exp $"
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


reset_line(Pinet_addr, device_type, line_number)
    struct sockaddr_in *Pinet_addr;
    u_short         device_type;
    u_short         line_number;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one,
                    param_two;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the device_type.
       outgoing[2] contains the line_number. */

    param_one = htons(device_type);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(line_number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_RESET_LINE, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}   /* reset_line() */
