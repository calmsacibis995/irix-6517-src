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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_inet_add.c,v 1.3 1996/10/04 12:14:39 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_inet_add.c,v $
 *
 * Revision History:
 *
 * $Log: set_inet_add.c,v $
 * Revision 1.3  1996/10/04 12:14:39  cwilson
 * latest rev
 *
 * Revision 2.8  1991/04/09  00:23:14  emond
 * Accommodate for generic API interface
 *
 * Revision 2.7  89/04/05  12:44:37  loverso
 * Changed copyright notice
 * 
 * Revision 2.6  88/05/24  18:40:59  parker
 * Changes for new install-annex script
 * 
 * Revision 2.5  88/05/04  23:29:31  harris
 * Use rpc() interface.  Fix lint by calling a sockaddr_in a sockaddr_in!
 * 
 * Revision 2.4  88/04/15  12:38:57  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:46  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:40:11  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:22:16  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:30  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:52:11  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:14:39 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_inet_add.c,v 1.3 1996/10/04 12:14:39 cwilson Exp $"
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


set_inet_addr(Pinet_addr, Penet_addr)
    struct sockaddr_in	*Pinet_addr;
    u_short		*Penet_addr;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Enter the new internet address to ethernet address association in
       the local data base. */

    wazoo(Pinet_addr, Penet_addr);

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp(). */

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_SET_INET_ADDR, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}   /* set_inet_addr() */
