/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgfwd.h,v $
 * Revision 65.1  1997/10/20 19:16:56  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.306.2  1996/02/18  22:56:10  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:18  marty]
 *
 * Revision 1.1.306.1  1995/12/08  00:19:47  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:00  root]
 * 
 * Revision 1.1.304.2  1994/04/15  18:57:27  tom
 * 	Support for dced (OT 10352):
 * 	  Add return values for rpc__dg_fwd_pkt.
 * 	  Add prototype for rpc__dg_fwd_init.
 * 	[1994/04/14  18:47:52  tom]
 * 
 * Revision 1.1.304.1  1994/01/21  22:36:52  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:59  cbrooks]
 * 
 * Revision 1.1.2.4  1993/03/01  21:14:34  markar
 * 	    OT CR 7243 fix: clean up handling of forwarded packets.
 * 	[1993/03/01  19:05:24  markar]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:10  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:08  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:48:05  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:53  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:25  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _DGFWD_H
#define _DGFWD_H

/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgfwd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG Packet Forwarding handler
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/*
 * R P C _ _ D G _ F W D _ P K T
 *
 * Forwarding Service.
 */

PRIVATE unsigned32 rpc__dg_fwd_pkt    _DCE_PROTOTYPE_ ((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

/*
 * Can return three values:
 *     FWD_PKT_NOTDONE  - caller should handle packet
 *     FWD_PKT_DONE     - we handled the packet, ok to free it
 *     FWD_PKT_DELAYED  - we saved it, don't handle it, don't free it.
 */
#define	FWD_PKT_NOTDONE		0
#define FWD_PKT_DONE		1
#define FWD_PKT_DELAYED		2

/*
 * R P C _ _ D G _ F W D _ I N I T
 *
 * Initialize forwarding service private mutex.
 */

PRIVATE void rpc__dg_fwd_init _DCE_PROTOTYPE_ ((void));

#endif /* _DGFWD_H */
