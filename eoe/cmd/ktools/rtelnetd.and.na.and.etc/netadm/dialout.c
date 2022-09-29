/******************************************************************************
 *
 *        Copyright 1989, 1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale is strictly prohibited.
 *
 * Module Function:
 *
 *	Select SRPC interface, make a DIALOUT remote procedure call
 *
 * Original Author: $Author $    Created on: $Date $
 *
 * Revision Control Information:
 *
 * $Id: dialout.c,v 1.2 1996/10/04 12:11:39 cwilson Exp $
 *
 * This file created by RCS from: 
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/dialout.c,v $
 *
 * Revision History:
 *
 * $Log: dialout.c,v $
 * Revision 1.2  1996/10/04 12:11:39  cwilson
 * latest rev
 *
 * Revision 1.4  1994/04/14  16:55:38  raison
 * remove extern of system function.
 *
 * Revision 1.3  1994/02/16  11:26:58  defina
 * Bug fix for dialback and acp "spr 2369". Key sent to srpc_create
 * is derived from acp_keys file and is read from cache via "annex_key".
 *
 * Revision 1.2  1993/12/30  14:15:43  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.1  1993/03/05  14:57:56  sasson
 * Initial revision
 *
 * 
 * This file is currently under revision by:
 * $Locker:  $
 *
 ******************************************************************************/

/* Include Files */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"
#include <netdb.h>
#include <strings.h>

#include <stdio.h>

#ifdef SYS_V
#include <termio.h>
#else
#include <sgtty.h>
#endif

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"

#define	INIT

#include "netadm.h"
#include "../libannex/srpc.h"

/* Defines and Macros */

/* Structure Definitions */

/* External Declarations */

extern char		*malloc(), *inet_ntoa();
extern UINT32		inet_addr();
extern int debug;
extern int init_socket();
extern int srpc_create();
extern KEYDATA *annex_key();

/* Forward Routine Declarations */

/* Static Declarations */

/*
 *  dialout_srpc_open()
 *
 *  open a socket, create a srpc connection
 *  by sending RPROC_SRPC_OPEN to the Annex.
 *
 * returns error code from init_socket() or srpc_create().
 */

dialout_srpc_open(srpc, s, Pinet_addr, cat, version)

SRPC	*srpc;
int	*s;			/* socket descriptor */
struct	sockaddr_in *Pinet_addr;
int	cat;			/* category: currently COURRPN_ACP */
int	version;

{
	int	return_code;		/* final return code for caller */
	KEYDATA         *key;           /* encryption key table */

	if((return_code = init_socket(s)) != 0)
		return return_code;

	/* If ACP enabled, this gets the acp_key that was cached. Note
	   this key originated from the acp_keys file. */
        key = annex_key(Pinet_addr->sin_addr.s_addr);

	if (debug) {
		printf("dialout.c:  Calling srpc_create.\n");
		printf("   sin_addr=0x%08x, sin_family=%d (%d), sin_port=%d.\n",
			Pinet_addr->sin_addr.s_addr, Pinet_addr->sin_family,
			AF_INET, ntohs(Pinet_addr->sin_port));

		}

	return_code = 
	srpc_create(srpc, *s, Pinet_addr, getpid(), cat,
		    version, RPROC_SRPC_OPEN, key);
	if (debug)
	    printf("netadm/dialout.c:  srpc_create returns %d.\n",return_code);

	return return_code;

}   /* acp_request_dialout() */

