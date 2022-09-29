/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgxq.h,v $
 * Revision 65.1  1997/10/20 19:17:00  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.331.2  1996/02/18  22:56:23  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:29  marty]
 *
 * Revision 1.1.331.1  1995/12/08  00:20:30  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:25  root]
 * 
 * Revision 1.1.329.2  1994/05/27  15:36:45  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added the status to rpc__dg_xmitq_append_pp().
 * 	[1994/04/29  19:22:22  tatsu_s]
 * 
 * Revision 1.1.329.1  1994/01/21  22:37:37  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:45  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:25:12  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:50  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:50:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:36  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:25  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _DGXQ_H
#define _DGXQ_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgxq.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


/*
 * R P C _ D G _ X M I T Q _ R E I N I T
 *
 * Reinitialize a transmit queue.
 */

#define RPC_DG_XMITQ_REINIT(xq, call) { \
    if ((xq)->head != NULL) rpc__dg_xmitq_free(xq, call); \
    rpc__dg_xmitq_reinit(xq); \
}

/*
 * R P C _ D G _ X M I T Q _ A W A I T I N G _ A C K _ S E T
 *
 * Mark a transmit queue as wanting an acknowledgement event (fack, response,
 * working, etc.)
 */

#define RPC_DG_XMITQ_AWAITING_ACK_SET(xq) ( \
    (xq)->awaiting_ack = true, \
    xq->awaiting_ack_timestamp = rpc__clock_stamp() \
)

/*
 * R P C _ D G _ X M I T Q _ A W A I T I N G _ A C K _ C L R
 *
 * Mark a transmit queue as no longer wanting an acknowledgement event.
 */

#define RPC_DG_XMITQ_AWAITING_ACK_CLR(xq) ( \
    (xq)->awaiting_ack = false \
)

PRIVATE void rpc__dg_xmitq_elt_xmit _DCE_PROTOTYPE_((
        rpc_dg_xmitq_elt_p_t  /*xqe*/,
        rpc_dg_call_p_t  /*call*/,
        boolean32  /*block*/
    ));

PRIVATE void rpc__dg_xmitq_init _DCE_PROTOTYPE_((
        rpc_dg_xmitq_p_t  /*xq*/
    ));

PRIVATE void rpc__dg_xmitq_reinit _DCE_PROTOTYPE_((
        rpc_dg_xmitq_p_t  /*xq*/
    ));

PRIVATE void rpc__dg_xmitq_free _DCE_PROTOTYPE_((
        rpc_dg_xmitq_p_t  /*xq*/,
        rpc_dg_call_p_t  /*call*/
    ));

PRIVATE void rpc__dg_xmitq_append_pp _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        unsigned32 * /*st*/
    ));

PRIVATE boolean rpc__dg_xmitq_awaiting_ack_tmo _DCE_PROTOTYPE_((
        rpc_dg_xmitq_p_t  /*xq*/,
        unsigned32  /*com_timeout_knob*/
    ));

PRIVATE void rpc__dg_xmitq_restart _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/
    ));

#endif /* _DGXQ_H */
