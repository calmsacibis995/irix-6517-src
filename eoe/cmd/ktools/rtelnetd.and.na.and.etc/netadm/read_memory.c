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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/read_memory.c,v 1.3 1996/10/04 12:13:45 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/read_memory.c,v $
 *
 * Revision History:
 *
 * $Log: read_memory.c,v $
 * Revision 1.3  1996/10/04 12:13:45  cwilson
 * latest rev
 *
 * Revision 2.15  1993/12/30  14:15:58  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.14  1991/04/09  00:13:02  emond
 * Accommodate generic TLI interface
 *
 * Revision 2.13  89/04/11  01:03:29  loverso
 * fix extern declarations, inet_addr is u_long, remove extern htonl et al
 * 
 * Revision 2.12  89/04/05  12:44:28  loverso
 * Changed copyright notice
 * 
 * Revision 2.11  88/06/22  10:33:33  mattes
 * Added dummy parameter to conform with dumb packet definition
 * 
 * Revision 2.10  88/05/24  18:40:39  parker
 * Changes for new install-annex script
 * 
 * Revision 2.9  88/05/04  23:21:00  harris
 * Use new rpc() interface.  Remove lint - was sockaddr (wrong) - sockaddr_in.
 * 
 * Revision 2.8  88/04/15  12:38:14  mattes
 * SL/IP integration
 * 
 * Revision 2.7  88/02/24  12:35:00  harris
 * Usual ntohl change for SUN and MACH, no longer needs special define.
 * 
 * Revision 2.6  87/09/22  11:49:46  parker
 * fixed bug uncovered by SUN build.
 * 
 * Revision 2.6  87/09/16  13:45:14  parker
 * Fixed problem with ntohl macro on SUN
 * 
 * Revision 2.5  87/06/29  16:48:25  parker
 * Bug fix the last bug fix.
 * 
 * Revision 2.4  87/06/23  15:07:15  parker
 * Bug fix: param_one should be u_short and param_two a u_long.
 * 
 * Revision 2.3  87/06/10  18:09:36  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:38:14  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:20:18  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:01  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:51:32  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:13:45 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/read_memory.c,v 1.3 1996/10/04 12:13:45 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include "port/port.h"
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

#define OUTGOING_COUNT  3

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


read_memory(Pinet_addr, offset, Pdata, count)
    struct sockaddr_in	*Pinet_addr;
    UINT32		offset;
    char		*Pdata;
    u_short		count;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one;
    u_short	    param_hole;
    UINT32          param_two;

    /* Check parameters. */

    if (count > READ_MEM_MAX)
        return NAE_CNT;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the byte count.
       outgoing[2] contains a hole.
       outgoing[3] contains the offset. */

    param_one = htons(count);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_hole = 0;
    outgoing[2].iov_base = (caddr_t)&param_hole;
    outgoing[2].iov_len = sizeof(param_hole);

    param_two = htonl(offset);
    outgoing[3].iov_base = (caddr_t)&param_two;
    outgoing[3].iov_len = sizeof(param_two);

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_READ_MEMORY, OUTGOING_COUNT, outgoing,
	       Pdata, RAW_BLOCK_P);

}   /* read_memory() */
