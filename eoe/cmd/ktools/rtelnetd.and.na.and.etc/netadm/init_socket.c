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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/init_socket.c,v 1.3 1996/10/04 12:13:29 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/init_socket.c,v $
 *
 * Revision History:
 *
 * $Log: init_socket.c,v $
 * Revision 1.3  1996/10/04 12:13:29  cwilson
 * latest rev
 *
 * Revision 2.9  1991/04/09  00:12:49  emond
 * Accommodate generic TLI interface
 *
 * Revision 2.8  90/02/15  15:36:48  loverso
 * clobber #else/#endif TAG
 * 
 * Revision 2.7  89/04/05  12:44:22  loverso
 * Changed copyright notice
 * 
 * Revision 2.6  88/05/24  18:40:36  parker
 * Changes for new install-annex script
 * 
 * Revision 2.5  88/05/04  23:18:05  harris
 * No longer uses globals.  Call socket() with the right number of args.
 * 
 * Revision 2.4  88/04/15  12:37:58  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:34  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:37:56  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:20:00  goodmon
 * Changes for broadcast command.`
 * 
 * Revision 2.0  86/02/21  11:35:46  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:51:22  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:13:29 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/init_socket.c,v 1.3 1996/10/04 12:13:29 cwilson Exp $"
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
#include <stdio.h>

/* External Data Declarations */


/* Defines and Macros */


/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


init_socket(sock)

int	*sock;
{
    int	s;
    char *app_nam="init_socket";
    struct sockaddr_in our_addr;

    our_addr.sin_family = AF_INET;
    our_addr.sin_addr.s_addr = 0;
    our_addr.sin_port = 0;

    s = api_open(IPPROTO_UDP, &our_addr, app_nam, FALSE);
    if (s == -1)
        return NAE_SOCK;

    /* No address is specified so TLI will assign an appropriate
       address to be bound to. (No address is specified because of
       the our_addr.sin_addr.s_addr=0; and our_addr.sin_port=0)
     */
    if (api_bind(s,(struct t_bind *)NULL, &our_addr, app_nam, FALSE) != 0)
       	return NAE_SOCK; 

    *sock = s;
    return NAE_SUCC;

}   /* init_socket() */
