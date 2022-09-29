/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgcall.h,v $
 * Revision 65.2  1998/03/21 19:05:10  lmc
 * Changed all "#ifdef DEBUG" to use "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)".  This allows RPC debugging to be
 * turned on in the kernel without turning on any kernel
 * debugging.
 *
 * Revision 65.1  1997/10/20  19:16:53  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.403.2  1996/02/18  22:56:04  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:12  marty]
 *
 * Revision 1.1.403.1  1995/12/08  00:19:23  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:32 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/08/29  19:18 UTC  tatsu_s
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 
 * 	HP revision /main/tatsu_s.psock_timeout.b0/1  1995/08/16  20:26 UTC  tatsu_s
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/12/07  23:58:46  root]
 * 
 * Revision 1.1.401.4  1994/06/24  15:49:28  tatsu_s
 * 	Turned off the first fack waiting for non-MBF.
 * 	[1994/06/20  00:41:37  tatsu_s]
 * 
 * Revision 1.1.401.3  1994/05/27  15:36:06  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added RPC_DG_CALL_SET_MAX_FRAG_SIZE().
 * 	[1994/04/29  18:58:29  tatsu_s]
 * 
 * Revision 1.1.401.2  1994/03/15  20:29:12  markar
 * 	     private client socket mods
 * 	[1994/02/22  17:47:22  markar]
 * 
 * Revision 1.1.401.1  1994/01/21  22:36:25  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:27  cbrooks]
 * 
 * Revision 1.1.3.4  1993/01/13  17:39:40  mishkin
 * 	Add "is_nocall" parameter to rpc__dg_call_xmit_fack().
 * 	[1992/12/11  22:04:03  mishkin]
 * 
 * Revision 1.1.3.3  1993/01/03  23:23:37  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:14  bbelch]
 * 
 * Revision 1.1.3.2  1992/12/23  20:46:52  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:00  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:14  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgcall.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.
**
**
*/

#ifndef _DGCALL_H
#define _DGCALL_H

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/* ======================================================================= */

/*
 * Call wait flags.
 *
 * This enumeration defines the types of events that calls need to wait
 * for when calling the call_wait routine.  These values indicate whether
 * the caller is waiting for 1) a network event, such as the arrival
 * of data, facks, etc., or 2) an internal event, such as  
 * packets/reservations to become available.  
 * 
 * Note that this distinction is only meaningful for users of private sockets.
 * Call threads using private sockets wait for network events by sleeping in 
 * recvfrom, and wait for internal events by sleeping on a condition variable.
 * Call threads that use shared sockets always wait on the call handle's
 * condition variable (i.e., always specify rpc_e_dg_wait_on_internal_event).
 */
typedef enum {
    rpc_e_dg_wait_on_internal_event,
    rpc_e_dg_wait_on_network_event
} rpc_dg_wait_event_t;

/* ======================================================================= */


/*
 * R P C _ D G _ C A L L _ R E F E R E N C E
 *
 * Increment the reference count for the CALL.  Note that there's no
 * RPC_DG_CALL_RELEASE; you must use one of RPC_DG_CCALL_RELEASE or
 * RPC_DG_SCALL_RELEASE depending on whether you have a client or server
 * call handle.  (The release function has to call the server/client
 * specific "free" function.)
 */

#define RPC_DG_CALL_REFERENCE(call) { \
    RPC_DG_CALL_LOCK_ASSERT(call); \
    assert((call)->refcnt < 255); \
    (call)->refcnt++; \
}

/*
 * R P C _ D G _ C A L L _ R E I N I T
 *
 * Reinitialize the common part of a call handle.
 * If we are not going to use MBF, turn off the first fack waiting.
 */

#define RPC_DG_CALL_REINIT(call) { \
    (call)->last_rcv_timestamp = 0; \
    (call)->start_time = rpc__clock_stamp(); \
    (call)->last_ping_timestamp = 0; \
    (call)->last_rcv_priv_timestamp = 0; \
    (call)->status = rpc_s_ok; \
    (call)->blocked_in_receive = false; \
    (call)->priv_cond_signal = false; \
    RPC_DG_XMITQ_REINIT(&(call)->xq, (call)); \
    RPC_DG_RECVQ_REINIT(&(call)->rq); \
    if ((call)->xq.max_frag_size <= RPC_C_DG_MUST_RECV_FRAG_SIZE) \
        (call)->xq.first_fack_seen = true; \
}

/*
 * R P C _ D G _ C A L L _ S E T _ T I M E R
 *
 * Set up a timer for a call handle.  Bump the reference count of the call
 * handle since the reference from the timer queue counts as a reference.
 */

#define RPC_DG_CALL_SET_TIMER(call, proc, freq) { \
    rpc__timer_set(&(call)->timer, (proc), (pointer_t) (call), (freq)); \
    RPC_DG_CALL_REFERENCE(call); \
}

/*
 * R P C _ D G _ C A L L _ S T O P _ T I M E R
 *
 * "Request" that a call's timer be stopped.  This amounts to setting
 * a bit in the call handle.  We use this macro instead of calling
 * "rpc__timer_clear" directly since we really want the timer to stop
 * itself so it can eliminate its reference to the call handle in a
 * race-free fashion.
 */

#define RPC_DG_CALL_STOP_TIMER(call) \
    (call)->stop_timer = true

#ifdef __cplusplus
extern "C" {
#endif

/*
 * R P C _ D G _ C A L L _ S E T _ M A X _ F R A G _ S I Z E
 *
 * Set the max_frag_size and max_resvs in a call handle.
 * Also, if we are not going to use MBF, turn off the first fack
 * waiting.
 */

#define RPC_DG_CALL_SET_MAX_FRAG_SIZE(call, st) \
{ \
    rpc__naf_inq_max_frag_size((call)->addr, \
                               &((call)->xq.max_frag_size), (st)); \
    if (*st != rpc_s_ok) \
        (call)->xq.max_frag_size = RPC_C_DG_MUST_RECV_FRAG_SIZE; \
    else if ((call)->xq.max_frag_size > RPC_C_DG_MAX_FRAG_SIZE) \
        (call)->xq.max_frag_size = RPC_C_DG_MAX_FRAG_SIZE; \
    if ((call)->xq.max_frag_size <= RPC_C_DG_MUST_RECV_FRAG_SIZE) \
        (call)->xq.first_fack_seen = true; \
    RPC_DG_FRAG_SIZE_TO_NUM_PKTS((call)->xq.max_frag_size, \
                                 (call)->max_resvs); \
}

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))

PRIVATE char *rpc__dg_call_state_name _DCE_PROTOTYPE_((
        rpc_dg_call_state_t state
    ));

#else

#define rpc__dg_call_state_name(junk) ""

#endif

PRIVATE void rpc__dg_call_xmit_fack _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        boolean32 /*is_nocall*/
    ));

PRIVATE void rpc__dg_call_xmit _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        boolean32 /*block*/
    ));

PRIVATE void rpc__dg_call_xmitq_timer _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/
    ));

PRIVATE void rpc__dg_call_init _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/
    ));

PRIVATE void rpc__dg_call_free _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/
    ));

PRIVATE void rpc__dg_call_wait _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/,
        rpc_dg_wait_event_t /*event*/,
        unsigned32 * /*st*/
    ));

PRIVATE void rpc__dg_call_signal _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/
    ));

PRIVATE void rpc__dg_call_xmitq_push _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/,
        unsigned32 * /*st*/
    ));

PRIVATE boolean rpc__dg_call_recvq_insert _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        boolean * /*rqe_is_head_inorder*/
    ));

PRIVATE void rpc__dg_call_signal_failure _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/,
        unsigned32 /*stcode*/
    ));

PRIVATE void rpc__dg_call_local_cancel _DCE_PROTOTYPE_((
        rpc_dg_call_p_t /*call*/
    ));

#ifdef __cplusplus
}
#endif


#endif
