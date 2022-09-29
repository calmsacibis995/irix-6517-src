/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgpkt.h,v $
 * Revision 65.2  1997/10/20 19:16:57  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.815.2  1996/02/18  22:56:13  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:21  marty]
 *
 * Revision 1.1.815.1  1995/12/08  00:20:08  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:10  root]
 * 
 * Revision 1.1.813.3  1994/05/27  15:36:27  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added rpc__dg_pkt_is_rationing().
 * 	Replaced rpc__dg_pkt_make_reservation() with
 * 	rpc__dg_pkt_adjust_reservation().
 * 	Added max_resv_pkt, is_rationing and low_on_pkts flags.
 * 	[1994/04/29  19:15:33  tatsu_s]
 * 
 * Revision 1.1.813.2  1994/03/15  20:32:22  markar
 * 	     private client socket mods
 * 	[1994/02/22  18:24:31  markar]
 * 
 * Revision 1.1.813.1  1994/01/21  22:37:10  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:11  cbrooks]
 * 
 * Revision 1.1.7.2  1993/11/09  22:22:59  markar
 * 	         Since the packet assertion failure has cropped up again, we're
 * 	         rolling back to the is_on_free_list flag work-around.
 * 	         I've also added a pad field to fix the problem IBM described in
 * 	         CR 7755.
 * 	[1993/11/09  22:10:41  markar]
 * 
 * Revision 1.1.7.1  1993/09/28  13:17:32  tatsu_s
 * 	     Bug 7755 - Removed is_on_free_list from rpc_dg_pkt_pool_elt_t.
 * 	     [1993/09/27  16:05:10  tatsu_s]
 * 
 * Revision 1.1.5.6  1993/03/31  22:27:39  markar
 * 	     OT CR 7465 partial fix: There are still code paths in which packets end
 * 	            up getting freed twice.  Add a new is_on_free_list flag to help
 * 	            avoid doing double frees.
 * 	[1993/03/25  21:44:14  markar]
 * 
 * Revision 1.1.5.5  1993/01/14  20:52:38  markar
 * 	     OT CR 6433 fix: add max_pkt_count field to packet pool.
 * 	[1993/01/14  19:17:38  markar]
 * 
 * Revision 1.1.5.4  1993/01/13  21:05:29  markar
 * 	     OT CR 6175 fix: fix packet rationing deadlock
 * 	[1992/12/16  14:44:56  markar]
 * 
 * Revision 1.1.5.3  1993/01/03  23:24:33  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:46  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:48:56  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:32  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/07  19:22:53  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:32:23  markar]
 * 
 * Revision 1.1  1992/01/19  03:07:23  devrcs
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
**      dgpkt.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for managing the datagram packet pool.
**
**
*/

#ifndef _DGPKT_H
#define _DGPKT_H

/* ========================================================================= */
/*
 * Datagram Packet Rationing
 *
 * Every active call requires a packet pool "reservation".  The purpose
 * of this reservation is to guarantee that the call can always make
 * progress.
 *
 * "Progress" is defined as the ability to exchange at least one packet
 * at a time over a conversation.
 * 
 * "Rationing" is the condition where calls are only allowed to use
 * their reserved packet.  
 * 
 * The condition that invokes rationing is: the number of available
 * packets in the free pool is less than or equal to the number of
 * packet reservations.
 */

/*
 * Define the maximum number of packet's (used for both xqe's and rqe's)
 * that we'll ever allocate.
 */
           
#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifndef RPC_C_DG_PKT_MAX
#define RPC_C_DG_PKT_MAX    100000
#endif

/*
 * Define the number of packets that should be allocated during
 * initialization to seed the free list.
 */    

#ifndef RPC_C_DG_PKT_INIT_CNT
#define RPC_C_DG_PKT_INIT_CNT       48
#endif
  
/*
 * One of the conditions on which the packet rationing scheme is based is
 * that, when rationing is necessary, all RPC threads will be able to drain
 * their queues and use only the packets they have reserved.  One possible
 * scenario is which this wouldn't be the case is if in a group of N
 * inter-communicating processes (where N can be 1, in the case of KRPC)
 * all packets got queued up by clients before the rationing state was
 * entered.  In such a case, there would be no reservations to grant to
 * the server sides of the calls, and thus no way for the clients to drain.
 * 
 * Avoiding this potential deadlock requires that we guarantee that at least
 * one server call can be started for such a group of N processes.  Since N can
 * be 1, it follows that every process must be guaranteed that it can always
 * start at least one server call.
 * 
 * We guarantee the capability of running server calls by setting aside
 * reservations for that purpose at startup.  A single reservation would be
 * sufficient to guarantee the correctness of the algorithm, but making a few
 * reservations will increase the fairness in the worst of cases.
 */
#ifndef RPC_C_DG_PKT_INIT_SERVER_RESVS
#define RPC_C_DG_PKT_INIT_SERVER_RESVS  5
#endif

/*
 * The  following structure is used to build the free packet pool.  Since
 * the pool contains both xqe's and rqe's, we need a third, neutral pointer
 * which can be used to access either type of transmission element.
 */
                       
typedef struct rpc_dg_pkt_pool_elt_t {
    union {
        struct rpc_dg_pkt_pool_elt_t *next; /* for building a free list */
        struct {
            rpc_dg_xmitq_elt_t xqe;
#ifndef INLINE_PACKETS
            rpc_dg_pkt_body_t  pkt;
#endif
        } xqe;                             /* for handling xqe's       */
        struct {
            rpc_dg_recvq_elt_t rqe;
#ifndef INLINE_PACKETS
            rpc_dg_raw_pkt_t   pkt;
            unsigned32         pad1;       /* Need some padding to get */ 
            unsigned32         pad2;       /* 0 MOD 8 byte alignment   */
#endif
            rpc_dg_sock_pool_elt_p_t sock_ref;
        } rqe;                             /* for handling rqe's       */
    } u;
    boolean is_on_free_list;     
} rpc_dg_pkt_pool_elt_t, *rpc_dg_pkt_pool_elt_p_t;

/*
 * The packet pool is a global structure which contains:
 *       - a mutex to lock when modifying this structure
 *       - the max # of packets that can be allocated
 *       - a counter of the number of packets remaining to be allocated 
 *       - number of packet reservations currently held
 *       - counters of the number of currently active xqe's/rqe's
 *         and failed rqe allocations (this should always be 0 if things
 *         are working) and xqe allocation attempts that had to block.
 *       - a counter of the number of packets on the free list
 *       - a linked list of free packets
 *       - a pointer to the tail of the free list
 *       - pointers to the head and tail of a linked list of call
 *         handles that are waiting to be signalled when a packet
 *         becomes available.
 *       - pointers to the head and tail of a linked list of call
 *         handles that are waiting to be signalled when a packet
 *         reservation becomes available.
 */

typedef struct rpc_dg_pkt_pool_t {
    rpc_mutex_t             pkt_mutex;
    unsigned32              max_pkt_count; 
    unsigned32              pkt_count; 
    unsigned32              reservations;
    unsigned32              srv_resv_avail;
    unsigned32              max_resv_pkt; /* # of pkts for the largest fragment */
    unsigned32              active_rqes;
    unsigned32              active_xqes;
    unsigned32              failed_alloc_rqe;
    unsigned32              blocked_alloc_xqe;
    unsigned32              free_count;
    unsigned                is_rationing: 1;    /* T => is rationing */
    unsigned                low_on_pkts: 1;     /* T => pkt pool is low */
    rpc_dg_pkt_pool_elt_p_t free_list;
    rpc_dg_pkt_pool_elt_p_t free_list_tail;
    rpc_dg_call_p_t pkt_waiters_head, pkt_waiters_tail;        
    rpc_dg_call_p_t rsv_waiters_head, rsv_waiters_tail;
} rpc_dg_pkt_pool_t;  

EXTERNAL rpc_dg_pkt_pool_t rpc_g_dg_pkt_pool;
                   
/* 
 * Macros for locking/unlocking the packet pool's mutex.
 */                                     

#define RPC_DG_PKT_POOL_LOCK(junk)    RPC_MUTEX_LOCK(rpc_g_dg_pkt_pool.pkt_mutex)
#define RPC_DG_PKT_POOL_UNLOCK(junk)  RPC_MUTEX_UNLOCK(rpc_g_dg_pkt_pool.pkt_mutex)
#define RPC_DG_PKT_POOL_LOCK_ASSERT(junk) \
                          RPC_MUTEX_LOCK_ASSERT(rpc_g_dg_pkt_pool.pkt_mutex)

/*
 * Macro to determine if we are in the packet rationing state.  
 *
 * This macro requires that the packet pool is locked.
 */ 
                                                         
#define RPC_DG_PKT_RATIONING(junk) \
        (rpc_g_dg_pkt_pool.free_count + rpc_g_dg_pkt_pool.pkt_count <= \
                   rpc_g_dg_pkt_pool.reservations)

/* ========================================================================= */

PRIVATE void rpc__dg_pkt_pool_init    _DCE_PROTOTYPE_((void));


PRIVATE void rpc__dg_pkt_pool_fork_handler    _DCE_PROTOTYPE_((
        rpc_fork_stage_id_t  /*stage*/
    ));

PRIVATE rpc_dg_xmitq_elt_p_t rpc__dg_pkt_alloc_xqe    _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        unsigned32 * /*st*/
    ));

PRIVATE rpc_dg_recvq_elt_p_t rpc__dg_pkt_alloc_rqe    _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE void rpc__dg_pkt_free_xqe    _DCE_PROTOTYPE_((                  
        rpc_dg_xmitq_elt_p_t  /*pkt*/,
        rpc_dg_call_p_t  /*call*/
    ));

PRIVATE void rpc__dg_pkt_free_rqe_for_stub    _DCE_PROTOTYPE_((
        rpc_dg_recvq_elt_p_t  /*pkt*/
    ));

PRIVATE void rpc__dg_pkt_free_rqe    _DCE_PROTOTYPE_((
        rpc_dg_recvq_elt_p_t  /*pkt*/,
        rpc_dg_call_p_t  /*call*/
    ));
     
PRIVATE boolean32 rpc__dg_pkt_adjust_reservation    _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        unsigned32 /*nreq*/,
        boolean32  /*block*/
    ));

PRIVATE void rpc__dg_pkt_cancel_reservation    _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/
    ));

PRIVATE boolean32 rpc__dg_pkt_is_rationing    _DCE_PROTOTYPE_((
        boolean32 * /*low_on_pkts*/
    ));
#endif /* _DGPKT_H */
