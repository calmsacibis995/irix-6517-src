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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_ln_stat.c,v 1.3 1996/10/04 12:12:52 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_ln_stat.c,v $
 *
 * Revision History:
 *
 * $Log: get_ln_stat.c,v $
 * Revision 1.3  1996/10/04 12:12:52  cwilson
 * latest rev
 *
 * Revision 2.6  1989/04/05  12:44:19  loverso
 * Changed copyright notice
 *
 * Revision 2.5  88/05/24  18:40:25  parker
 * Changes for new install-annex script
 * 
 * Revision 2.4  88/05/04  23:17:14  harris
 * Use rpc() interface.
 * 
 * Revision 2.3  88/04/15  12:37:34  mattes
 * SL/IP integration
 * 
 * Revision 2.2  86/12/03  16:37:39  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:19:31  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:35:34  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:50:55  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:12:52 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/get_ln_stat.c,v 1.3 1996/10/04 12:12:52 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>

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


get_ln_stat(Pinet_addr, device_type, line_number, cat, number, type, Pdata)
    struct sockaddr *Pinet_addr;
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

    if (Pinet_addr->sa_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the device_type.
       outgoing[2] contains the line_number.
       outgoing[3] contains the catagory.
       outgoing[4] contains the dla stat number. */

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

    return rpc(Pinet_addr, RPROC_GET_LINE_STAT, OUTGOING_COUNT, outgoing,
	       Pdata, type);

}   /* get_ln_stat() */
