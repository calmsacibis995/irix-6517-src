/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgccall.c,v 65.5 1998/03/23 17:29:00 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgccall.c,v $
 * Revision 65.5  1998/03/23 17:29:00  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/26 17:05:33  lmc
 * Added prototyping and casting.
 *
 * Revision 65.3  1998/01/07  17:21:17  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:20  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:53  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.806.1  1996/05/10  13:11:14  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:13 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:42 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Release the global mutex while calling rpc__dg_network_use_protseq().
 * 	[1995/11/03  14:08 UTC  tatsu_s  /main/HPDCE02/tatsu_s.mothra_fix.b0/1]
 *
 * 	CHFts16197: Always reset frag size to minimum at start of each call,
 * 	since shrinking the frag size mid-call is impossible.
 * 	[1995/08/29  18:34 UTC  sommerfeld  /main/HPDCE02/sommerfeld_dfsperf/1]
 *
 * 	Re-submitted the timeout fix.
 * 	[1995/09/05  19:19 UTC  tatsu_s  /main/HPDCE02/15]
 *
 * 	Fixed the timestamp comparison.
 * 	[1995/09/05  19:16 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/2]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:00 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:32 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	CHFts16197: Always reset frag size to minimum at start of each call,
 * 	since shrinking the frag size mid-call is impossible.
 * 	[1995/08/29  18:34 UTC  sommerfeld  /main/HPDCE02/sommerfeld_dfsperf/1]
 *
 * 	Re-submitted the timeout fix.
 * 	[1995/09/05  19:19 UTC  tatsu_s  /main/HPDCE02/15]
 *
 * 	Fixed the timestamp comparison.
 * 	[1995/09/05  19:16 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/2]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:18 UTC  tatsu_s  /main/HPDCE02/14]
 *
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 *
 * 	Submitted the fix for CHFts15685.
 * 	[1995/07/14  19:07 UTC  tatsu_s  /main/HPDCE02/13]
 *
 * 	Added volatile.
 * 	The max_ping_time must be longer than 1+ seconds.
 * 	[1995/07/11  21:43 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b4/2]
 *
 * 	Submitted the fix for dced deadlock.
 * 	[1995/07/06  17:02 UTC  tatsu_s  /main/HPDCE02/12]
 *
 * 	Release a call lock while resolving ep.
 * 	[1995/07/05  20:30 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b4/1]
 *
 * 	Submitted the fix for CHFts15450/CHFts15591.
 * 	[1995/06/30  20:50 UTC  tatsu_s  /main/HPDCE02/11]
 *
 * 	Use the right ccall in alloc_client_socket().
 * 	[1995/06/23  23:04 UTC  tatsu_s  /main/HPDCE02/tatsu_s.fix_ot12885.b1/1]
 *
 * 	Submitted the fix for CHFts15498.
 * 	[1995/06/15  20:13 UTC  tatsu_s  /main/HPDCE02/10]
 *
 * 	Cleaned up the socket allocation.
 * 	[1995/06/13  19:01 UTC  tatsu_s  /main/HPDCE02/tatsu_s.svc.b0/2]
 *
 * 	Added more tracing messages.
 * 	[1995/06/12  19:00 UTC  tatsu_s  /main/HPDCE02/tatsu_s.svc.b0/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/9]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:01 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 *
 * 	Submitted the fix for CHFts15302.
 * 	[1995/05/23  20:34 UTC  tatsu_s  /main/HPDCE02/8]
 *
 * 	Avoid an erroneous address calculation.
 * 	[1995/05/18  16:50 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b2/2]
 *
 * 	Free a cached ccall if ep_resolve fails in alloc_client_socket().
 * 	[1995/05/17  21:12 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b2/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:14 UTC  lmm  /main/HPDCE02/7]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:18 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:19 UTC  tatsu_s  /main/HPDCE02/6]
 *
 * 	Fixed the local auth pre_call.
 * 	[1995/03/01  19:32 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/2]
 *
 * 	Local RPC Security Bypass.
 * 	[1995/02/22  22:31 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/1]
 *
 * 	Submitted CHFtsthe fix for CHFts14267 and misc. itimer fixes.
 * 	[1995/02/07  20:56 UTC  tatsu_s  /main/HPDCE02/5]
 *
 * 	Fixed CHFts14267.
 * 	[1995/02/06  17:54 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b1/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	Changed the signature of rpc__timer_itimer_enable().
 * 	Fixed the callback.
 * 	[1995/01/28  02:36 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/2]
 *
 * 	Added itimer.
 * 	[1995/01/25  20:34 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	ifdef out call to resolve binding for KRPC
 * 	[1995/01/16  19:15 UT
 *
 * Revision 1.1.802.7  1994/08/25  21:54:08  tatsu_s
 * 	MBF: Always make the max pkt reservation.
 * 	     Keep the fragment size between calls.
 * 	[1994/08/23  13:04:25  tatsu_s]
 * 
 * Revision 1.1.802.6  1994/06/24  15:49:29  tatsu_s
 * 	Merged up to DCE1_1.
 * 	[1994/06/22  13:49:26  tatsu_s]
 * 
 * 	FIX OT 10960: If xq.snd_frag_size is based on
 * 	rq.high_rcv_frag_size, make sure it's a multiple of 8 bytes.
 * 	[1994/06/17  13:52:42  tatsu_s]
 * 
 * 	Always make the reservation for the WAY callback.
 * 	(This is another way of fixing OT 10876.)
 * 	[1994/06/08  14:57:17  tatsu_s]
 * 
 * 	MBF code cleanup.
 * 	[1994/06/03  17:57:28  tatsu_s]
 * 
 * Revision 1.1.802.5  1994/06/21  21:53:53  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:30:58  hopkins]
 * 
 * Revision 1.1.802.4  1994/05/27  15:36:08  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:54:35  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  18:59:08  tatsu_s]
 * 
 * Revision 1.1.802.3  1994/03/15  20:29:47  markar
 * 	     private client socket mods
 * 	[1994/02/22  17:50:27  markar]
 * 
 * Revision 1.1.802.2  1994/01/28  23:09:26  burati
 * 	rpc_c_mem_dg_epac is now uppercase for code cleanup
 * 	[1994/01/28  04:22:33  burati]
 * 
 * 	EPAC changes - add ccall->auth_way_info (dlg_bl1)
 * 	[1994/01/24  22:32:29  burati]
 * 
 * Revision 1.1.802.1  1994/01/21  22:36:27  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:28  cbrooks]
 * 
 * Revision 1.1.6.4  1993/02/11  21:11:49  markar
 * 	     OT CR 7188 fix: bump xmitq's serial number count for ping packets.
 * 	[1993/02/11  18:29:48  markar]
 * 
 * Revision 1.1.6.3  1993/01/03  23:23:40  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:18  bbelch]
 * 
 * Revision 1.1.6.2  1992/12/23  20:47:01  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:05  zeliff]
 * 
 * Revision 1.1.2.5  1992/05/15  17:22:12  mishkin
 * 	Fix logic around call to rpc__dg_cct_get() since that routine now initializes
 * 	CCT-related CCALL fields itself.
 * 	[1992/05/14  19:38:57  mishkin]
 * 
 * 	- Turn RPC_DG_CCALL_LSCT_INQ_SCALL & RPC_DG_CCALL_LSCT_NEW_CALL into functions.
 * 	- Remove "force" logic in calls to rpc__dg_cct_get().
 * 	[1992/05/08  12:35:50  mishkin]
 * 
 * Revision 1.1.2.4  1992/05/12  16:14:58  mishkin
 * 	bmerge rationing from mainline
 * 	[1992/05/08  20:49:51  mishkin]
 * 
 * Revision 1.1.2.3  1992/05/07  17:54:34  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:25:09  markar]
 * 
 * Revision 1.1.2.2  1992/05/01  17:20:31  rsalz
 * 	 06-feb-92 sommerfeld    fix domain C implicit member problem.
 * 	 05-feb-92 sommerfeld    key reorg.
 * 	[1992/05/01  17:16:10  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:16  devrcs
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
**      dgccall.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handle client call (CCALL) handles.
**
**
*/

#include <dg.h>
#include <dgcall.h>
#include <dgccall.h>
#include <dgcct.h>
#include <dgpkt.h>
#include <dgrq.h>
#include <dgxq.h>
#include <dghnd.h>
#include <dgscall.h>

#include <dce/ep.h>             /* derived from ep.idl */
#include <dce/assert.h>

/* ========================================================================= */

/*
 * ccall specific com_timeout controllable parameters...
 */

typedef struct {
    unsigned32 max_ping_time;
} com_timeout_params_t;


/* ========================================================================= */

INTERNAL void ccall_common_init _DCE_PROTOTYPE_((
        rpc_dg_binding_client_p_t  /*h*/,
        rpc_dg_ccall_p_t  /*ccall*/,
        unsigned32  /*options*/,
        rpc_if_rep_p_t  /*ifspec*/,
        unsigned32  /*opnum*/,
        unsigned32 * /*st*/
    ));

INTERNAL rpc_dg_ccall_p_t ccall_alloc _DCE_PROTOTYPE_((
        rpc_dg_binding_client_p_t  /*h*/,
        unsigned32  /*options*/,
        rpc_if_rep_p_t  /*ifspec*/,
        unsigned32  /*opnum*/,
        unsigned32 * /*st*/
    ));

INTERNAL rpc_dg_ccall_p_t ccall_reinit _DCE_PROTOTYPE_((
        rpc_dg_binding_client_p_t  /*h*/,
        unsigned32  /*options*/,
        rpc_if_rep_p_t  /*ifspec*/,
        unsigned32  /*opnum*/,
        boolean * /*insert_in_ccallt*/,
        unsigned32 * /*st*/
    ));

INTERNAL void xmit_orphan_quit _DCE_PROTOTYPE_(( rpc_dg_ccall_p_t  /*ccall*/));

INTERNAL void recv_state_timer _DCE_PROTOTYPE_((rpc_dg_ccall_p_t  /*ccall*/));

INTERNAL void ccall_orphan_timer _DCE_PROTOTYPE_((rpc_dg_ccall_p_t  /*ccall*/));

INTERNAL void ccall_cancel_timer _DCE_PROTOTYPE_((rpc_dg_ccall_p_t  /*ccall*/));

INTERNAL void ccall_uncache _DCE_PROTOTYPE_((rpc_dg_ccall_p_t  /*ccall*/));

INTERNAL void ccall_binding_serialize _DCE_PROTOTYPE_((
        rpc_dg_binding_client_p_t  /*h*/,
        boolean32  /*is_brdcst*/,
        volatile unsigned32 * /*cancel_cnt*/,
        volatile rpc_clock_p_t  /*cancel_time*/,
        unsigned32 * /*st*/
    ));

INTERNAL void ccall_initial_cancel_setup _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/,
        unsigned32  /*cancel_cnt*/,
        rpc_clock_t  /*cancel_time*/
    ));

INTERNAL void ccall_timeout_timer _DCE_PROTOTYPE_ ((
        rpc_dg_ccall_p_t  /*ccall*/
    ));

/* ========================================================================= */

/*
 * R P C _ _ D G _ C C A L L _ L S C T _ I N Q _ S C A L L
 *
 * Return the connection's locked scall.  For callbacks, the ccall is
 * the connection's logical SCT.
 * 
 * Since the ccall->c.high_seq field is being overloaded to be both the
 * logical scte and the call's general proxy seq tracker, there is the
 * possibility that the scall's seq won't match the general proxy seq.
 * If the scall appears old, ignore it.
 */

PRIVATE void rpc__dg_ccall_lsct_inq_scall
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall,
    rpc_dg_scall_p_t *scallp
)    
#else
(ccall, scallp) 
rpc_dg_ccall_p_t ccall;
rpc_dg_scall_p_t *scallp;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    if (ccall->c.is_cbk)
        *scallp = NULL;
    else
    {
        *scallp = ccall->cbk_scall;
        if (*scallp != NULL)
        {
            RPC_DG_CALL_LOCK(&(*scallp)->c);
            if ((*scallp)->c.call_seq != ccall->c.high_seq)
            {
                RPC_DG_CALL_UNLOCK(&(*scallp)->c);
                *scallp = NULL;
            }
        }
    }
}


/*
 * R P C _ _ D G _ C C A L L _ L S C T _ N E W _ C A L L
 *
 * Setup the connection for the new call to be run.  For callbacks, the
 * ccall is the connection's logical SCT.
 * 
 * Return a locked scall for the new call (create one if necessary).
 * Make certain that no one does anything crazy with the connection
 * sequence number.
 */

PRIVATE void rpc__dg_ccall_lsct_new_call
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall,
    rpc_dg_sock_pool_elt_p_t si,
    rpc_dg_recvq_elt_p_t rqe,
    rpc_dg_scall_p_t *scallp
)
#else
(ccall, si, rqe, scallp) 
rpc_dg_ccall_p_t ccall;
rpc_dg_sock_pool_elt_p_t si;
rpc_dg_recvq_elt_p_t rqe;
rpc_dg_scall_p_t *scallp;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);
    assert(ccall->c.is_cbk == false);

    *scallp = ccall->cbk_scall;

    if (*scallp != NULL)
        rpc__dg_scall_reinit(*scallp, si, rqe);
    else
        *scallp = rpc__dg_scall_cbk_alloc(ccall, si, rqe);

    if (*scallp != NULL)
    {
        if (RPC_DG_SEQ_IS_LT(rqe->hdrp->seq, ccall->c.high_seq))
        {
	    /*
	     * rpc_m_invalid_seqnum
	     * "(%s) Invalid call sequence number"
	     */
	    RPC_DCE_SVC_PRINTF ((
		DCE_SVC(RPC__SVC_HANDLE, "%s"),
		rpc_svc_general,
		svc_c_sev_fatal | svc_c_action_abort,
		rpc_m_invalid_seqnum,
		"rpc__dg_ccall_lsct_new_call" ));
        }
        ccall->c.high_seq = rqe->hdrp->seq;
    }
}


/*
 * C C A L L _ C O M M O N _ I N I T
 *
 * Common ccall initialization done by ccall_alloc and ccall_reinit.
 */

INTERNAL void ccall_common_init
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_client_p_t h,
    rpc_dg_ccall_p_t ccall,
    unsigned32 options,
    rpc_if_rep_p_t ifspec,
    unsigned32 opnum,
    unsigned32 *st
)
#else
(h, ccall, options, ifspec, opnum, st)
rpc_dg_binding_client_p_t h;
rpc_dg_ccall_p_t ccall;
unsigned32 options;
rpc_if_rep_p_t ifspec;
unsigned32 opnum;
unsigned32 *st;
#endif
{
    boolean doing_callback = ccall->c.is_cbk;

    RPC_LOCK_ASSERT(0);
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    if (doing_callback)
        RPC_DG_CALL_LOCK_ASSERT(&ccall->cbk_scall->c);

    /*
     * Finish filling in the call handle with stuff that changes on each
     * call.  NOTE: "normal" idempotent calls with large ins eventually
     * get tagged non-idempotent (see rpc__dg_xmitq_append_pp()).  If this
     * is a broadcast call, then the mark the binding and call handles to
     * indicate that a server instance is not bound.
     */

    ccall->c.call_opnum             = opnum;
    ccall->c.xq.base_flags          = 0;
    ccall->c.xq.base_flags2         = 0;
    ccall->response_info_updated    = false;
    ccall->cbk_start                = false;
    ccall->reject_status            = 0;

    ccall->cancel.next_time             = 0;
    ccall->cancel.server_is_accepting   = true;
    ccall->cancel.server_had_pending    = false;
    ccall->cancel.local_count           = 0;
    ccall->cancel.server_count          = 0;
    ccall->cancel.timeout_time          = 0;

    ccall->quit.quack_rcvd          = false;

    ccall->auth_way_info            = NULL;
    ccall->auth_way_info_len        = 0;

    /*
     * every call starts out using the binding's (user's) specified
     * com timeout knob value.
     *
     * at least for a while, make sure that the binding's timeout is sane
     */
#ifdef SGIMIPS
    assert((int)h->c.c.timeout >= rpc_c_binding_min_timeout);
#else
    assert(h->c.c.timeout >= rpc_c_binding_min_timeout);
#endif
    assert(h->c.c.timeout <= rpc_c_binding_infinite_timeout);
    ccall->c.com_timeout_knob       = h->c.c.timeout;

    if (h->c.c.call_timeout_time == 0)
    {
        ccall->timeout_stamp = 0;
    }
    else
    {
        ccall->timeout_stamp = rpc__clock_stamp() + h->c.c.call_timeout_time;
    }

    if ((options & rpc_c_call_brdcst) != 0)
    {
        ccall->c.xq.base_flags |= RPC_C_DG_PF_BROADCAST | RPC_C_DG_PF_IDEMPOTENT;
        h->c.c.bound_server_instance = false;
    }

    if ((options & rpc_c_call_idempotent) != 0)
        ccall->c.xq.base_flags |= RPC_C_DG_PF_IDEMPOTENT;
    if ((options & rpc_c_call_maybe) != 0)
        ccall->c.xq.base_flags |= RPC_C_DG_PF_MAYBE | RPC_C_DG_PF_IDEMPOTENT;

    ccall->c.has_inpipe = ((options & rpc_c_call_in_pipe) != 0);
    ccall->c.has_outpipe = ((options & rpc_c_call_out_pipe) != 0);
    /*
     * If this call is using a private socket, we need to store the call's
     * thread ID in the call handle.  Since calls which use private sockets
     * are allowed to block in recvfrom, the only way of getting their
     * attention for things like retransmissions or timeouts is to post
     * a cancel against the thread.
     */
    if (ccall->c.sock_ref->is_private)
        ccall->c.thread_id = pthread_self();

    /*
     * Make sure that if the binding doesn't specify a bound server instance,
     * we enable processing to determine and establish one.
     */
    ccall->server_bound = h->c.c.bound_server_instance;

    /*
     * If a bound server instance does not exist and an endpoint isn't
     * specified then try a few other locating techniques. If the interface
     * has a well known endpoint, use it; otherwise use the forwarder's
     * port.
     */
    if (ccall->server_bound == false && h->c.c.addr_has_endpoint == false)
    {
        rpc__if_set_wk_endpoint(ifspec, &ccall->c.addr, st);

        if (*st != rpc_s_ok)
        {
            rpc__if_set_wk_endpoint
                ((rpc_if_rep_p_t) ept_v3_0_c_ifspec, &ccall->c.addr, st);
            if (*st != rpc_s_ok)
                return;
        }
    } 
                                 
    /*
     * If this is not a callback, initialize the seq number from the
     * ccte.  Otherwise, bump the sequence number stored in the CCALL,
     * and update the high-water mark for sequence numbers for the SCALL.
     */

    if (doing_callback)
    {
        ccall->c.high_seq = ccall->c.call_seq = ++ccall->cbk_scall->c.high_seq;

        /*
         * This is essentially a turnaround. The server, which has been the
         * receiver, becomes the sender.
         *
         * Instead of sending a small fragment, we start with the largest
         * fragment size seen so far. We assume that the client can receive
         * the fragment as large as what it has been sending; since the
         * client's xq.snd_frag_size is bounded by its xq.max_frag_size.
         *
         * We inherit high_rcv_frag_size from the original scall.
         */
        ccall->c.rq.high_rcv_frag_size =
            ccall->cbk_scall->c.rq.high_rcv_frag_size;

        if (ccall->c.rq.high_rcv_frag_size > ccall->cbk_scall->c.xq.snd_frag_size)
        {
            ccall->c.xq.snd_frag_size = MIN(ccall->c.rq.high_rcv_frag_size,
                                            ccall->c.xq.max_snd_tsdu);
            /*
             * Fragment sizes must be 0 MOD 8.  Make it so.
             */
            ccall->c.xq.snd_frag_size &= ~ 0x7;

            /*
             * If we are going to use high_rcv_frag_size, then it implies that
             * we don't need to wait for the first fack.
             */
            ccall->c.xq.first_fack_seen = true;
        }
        else
            ccall->c.xq.snd_frag_size = ccall->cbk_scall->c.xq.snd_frag_size;

        /*
         * Also we inherit the reservation from the original scall, which
         * gives us enough packets for snd_frag_size.
         */
        ccall->c.n_resvs = ccall->cbk_scall->c.n_resvs;
    }
    else
    {
        ccall->c.high_seq = ccall->c.call_seq = ccall->ccte_ref.ccte->seq++;

	/*
	 * Reset high_rcv_frag_size and snd_frag_size since we start
	 * from the minimum fragment size.
	 */
	ccall->c.rq.high_rcv_frag_size = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
	ccall->c.xq.snd_frag_size = MIN(RPC_C_DG_MUST_RECV_FRAG_SIZE,
					ccall->c.xq.snd_frag_size);
    }

    RPC_DBG_PRINTF(rpc_e_dbg_xmit, 6,
                   ("(ccall_common_init) Set snd fs %lu, high rcv fs %lu\n",
                   ccall->c.xq.snd_frag_size, ccall->c.rq.high_rcv_frag_size));

    *st = rpc_s_ok;
}


/*
 * C C A L L _ A L L O C
 *
 * Allocate and initialize a ccall for the input binding handle.
 */

INTERNAL rpc_dg_ccall_p_t ccall_alloc
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_client_p_t h,
    unsigned32 options,
    rpc_if_rep_p_t ifspec,
    unsigned32 opnum,
    unsigned32 *st
)
#else
(h, options, ifspec, opnum, st)
rpc_dg_binding_client_p_t h;
unsigned32 options;
rpc_if_rep_p_t ifspec;
unsigned32 opnum;
unsigned32 *st;
#endif
{
    rpc_dg_pkt_hdr_p_t hdrp;
    rpc_dg_sock_pool_elt_p_t sp;
    rpc_dg_ccall_p_t ccall;
    boolean doing_callback = (h->shand != NULL);

    RPC_LOCK_ASSERT(0);
    if (doing_callback)
        RPC_DG_CALL_LOCK_ASSERT(&h->shand->scall->c);

    /* 
     * Get a reference to a socket pool element.  (For callbacks, just use the 
     * one stored in the associated scall.)  use_protseq will bump the refcnt
     * for the socket pool entry automatically, in the callback case, do it
     * by hand.
     */
    if (! doing_callback)
    {
        rpc__dg_network_use_protseq_cl(h->c.c.rpc_addr->rpc_protseq_id,
                              &sp);
        if (sp == NULL) {
            *st = rpc_s_cant_create_sock;
            return (NULL);
        }
    }  
    else                
    {
        sp = ((rpc_dg_binding_server_p_t) (h->shand))->scall->c.sock_ref;
        rpc__dg_network_sock_reference(sp);
    }
    
    RPC_MEM_ALLOC(ccall, rpc_dg_ccall_p_t, sizeof *ccall, 
                    RPC_C_MEM_DG_CCALL, RPC_C_MEM_NOWAIT);
    if (ccall == NULL){
	rpc__dg_network_sock_release(&sp);
	*st = rpc_s_no_memory;
	return(NULL);
    }

    /*
     * Initialize the common call handle header.
     */

    rpc__dg_call_init(&ccall->c);

    RPC_DG_CALL_LOCK(&ccall->c);
    RPC_DG_CALL_REFERENCE(&ccall->c);       /* For the ref we're going to return */

    RPC_DG_CCTE_REF_INIT(&ccall->ccte_ref);
                            
    /*
     * Store our reference to a socket pool entry.
     */
    ccall->c.sock_ref = sp;

    /*
     * If we were given a private socket, store the address of our call
     * handle in the socket info element.  This information will be needed
     * by the listen/dispatch code to do call handle locking.
     */
    if (sp->is_private)
        sp->ccall = ccall;

    /*
     * If this isn't a call back, get a ccte reference.  And fill in those
     * fields that are dependent on the callback state.
     */

    if (! doing_callback)
    {
        rpc__dg_cct_get(h->c.c.auth_info, ccall);
	if (ccall->ccte_ref.ccte == NULL){
	    RPC_DG_CCALL_RELEASE(&ccall); 
	    rpc__dg_network_sock_release(&sp);
	    *st = rpc_s_no_memory;
	    return(NULL);
	}
        
        ccall->c.is_cbk         = false;
        ccall->cbk_scall        = NULL;

    }
    else
    {   
        rpc_dg_scall_p_t scall = ((rpc_dg_binding_server_p_t) (h->shand))->scall;

        /*
         * Otherwise, fill in the appropriate fields from information
         * associated with the original SCALL.
         */

        /*
         * !!! Callbacks aren't authenticated at the moment.
         * We used to copy the auth_info from the scall and
         * increment its reference count.  It appeared to work at one point,
         * but I can't figure out how it did.
         */
        ccall->c.key_info       = 0;
        ccall->c.auth_epv       = 0;
        ccall->c.call_actid     = scall->c.call_actid;
        ccall->c.actid_hash     = scall->c.actid_hash;
        ccall->c.rq.window_size = scall->c.rq.window_size;
        ccall->c.is_cbk         = true;
        
        /*
         * Point the two CALL's at each other.
         */

        RPC_DG_CALL_REFERENCE(&scall->c);    
        ccall->cbk_scall = scall;
        RPC_DG_CALL_REFERENCE(&ccall->c);    
        scall->cbk_ccall = ccall;

        ccall->c.xq.max_rcv_tsdu = scall->c.xq.max_rcv_tsdu;
        ccall->c.xq.max_snd_tsdu = scall->c.xq.max_snd_tsdu;
        ccall->c.xq.max_frag_size = scall->c.xq.max_frag_size;
        ccall->c.max_resvs = scall->c.max_resvs;
    }

    ccall->c.c.is_server    = false;

    rpc__naf_addr_copy(h->c.c.rpc_addr, &ccall->c.addr, st);

    /*
     * Initialize the fields of the common call handle header that
     * are really part of the prototype packet header.
     */

    ccall->c.call_object    = h->c.c.obj;
    ccall->c.call_if_id     = ifspec->id; 
    ccall->c.call_if_vers   = ifspec->vers; 
    ccall->c.call_ihint     = RPC_C_DG_NO_HINT;

    /*
     * Initialize the rest of the prototype header.
     */

    hdrp = &ccall->c.xq.hdr;

    RPC_DG_HDR_SET_PTYPE(hdrp, RPC_C_DG_PT_REQUEST);

    hdrp->flags2        = 0;
    hdrp->server_boot   = h->server_boot;
    hdrp->ahint         = RPC_C_DG_NO_HINT;

    /*
     * Initialize some of the fields that are uniquely part of the
     * CLIENT call handle. 
     */

    ccall->h                = (struct rpc_dg_binding_client_t *) h;
    ccall->server_bound     = false;
    ccall->fault_rqe        = NULL;

    /*
     * Initialize the max_tsdu field for this call.  We can do this
     * even if we don't yet know the server address since this value
     * only depends on the address family.
     */

    if (! doing_callback)
    {
        rpc__naf_inq_max_tsdu(ccall->c.addr->rpc_protseq_id,
                              &ccall->c.xq.max_rcv_tsdu, st); 
        ccall->c.xq.max_snd_tsdu = ccall->c.xq.max_rcv_tsdu;
        ccall->c.xq.max_rcv_tsdu = MIN(ccall->c.xq.max_rcv_tsdu,
                                       ccall->c.sock_ref->rcvbuf);
        ccall->c.xq.max_snd_tsdu = MIN(ccall->c.xq.max_snd_tsdu,
                                       ccall->c.sock_ref->sndbuf);

        RPC_DBG_PRINTF(rpc_e_dbg_xmit, 6,
                       ("(ccall_alloc) Set rcv tsdu %lu, snd tsdu %lu\n",
                        ccall->c.xq.max_rcv_tsdu, ccall->c.xq.max_snd_tsdu));
    }

    /*    
     * If this is not a broadcast, initialize the max_frag_size, and
     * snd_frag_size fields.  In the broadcast case, they'll get
     * updated when we update the server address in the handle.  Note
     * that the snd_frag_size field, which is the one actually used
     * to determine the size of xmit'd packets, is left at the default
     * initialization value.  It will only be increased when a fack
     * is received that indicates a larger packet size can be used.                   
     */
    
    if ((options & rpc_c_call_brdcst) == 0)
    {
        if (! doing_callback)
        {
            RPC_DG_CALL_SET_MAX_FRAG_SIZE(&ccall->c, st);
            RPC_DBG_PRINTF(rpc_e_dbg_xmit, 6,
                           ("(ccall_alloc) Set max fs %lu\n",
                           ccall->c.xq.max_frag_size));
    }
    }

    if (! doing_callback)
    {
        RPC_DG_RBUF_SIZE_TO_WINDOW_SIZE(sp->rcvbuf,
                                        sp->is_private,
                                        ccall->c.xq.max_frag_size,
                                        ccall->c.rq.window_size);
        RPC_DBG_PRINTF(rpc_e_dbg_xmit, 6,
                   ("(ccall_alloc) Set ws %lu, rcvbuf %lu, max fs %lu\n",
              ccall->c.rq.window_size, sp->rcvbuf, ccall->c.xq.max_frag_size));
    }

    ccall_common_init(h, ccall, options, ifspec, opnum, st);
    if (*st != rpc_s_ok)
    {
        RPC_DG_CCALL_RELEASE(&ccall); 
        return (NULL);
    }

    /*
     * Start up a timer for this call.  Note that the timer routine
     * will must not run until we're done initializing things in
     * this routine.  It won't because it needs the call lock, which
     * we're currently holding.
     */

    RPC_DG_CALL_SET_TIMER(&ccall->c, rpc__dg_ccall_timer, RPC_CLOCK_SEC(1));

    return (ccall);
}


/*
 * C C A L L _ R E I N I T
 *
 * Re-initialize the input binding handle's ccall.  Return true iff
 * the ccall needs to be re-inserted in the CCALLT.
 */

INTERNAL rpc_dg_ccall_p_t ccall_reinit
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_client_p_t h,
    unsigned32 options,
    rpc_if_rep_p_t ifspec,
    unsigned32 opnum,
    boolean *insert_in_ccallt,
    unsigned32 *st
)
#else
(h, options, ifspec, opnum, insert_in_ccallt, st)
rpc_dg_binding_client_p_t h;
unsigned32 options;
rpc_if_rep_p_t ifspec;
unsigned32 opnum;
boolean *insert_in_ccallt;
unsigned32 *st;
#endif
{
    rpc_dg_call_state_t prev_state;
    rpc_dg_ccall_p_t ccall;
    boolean doing_callback = (h->shand != NULL);

    *insert_in_ccallt = false;
    RPC_LOCK_ASSERT(0);
    if (doing_callback)
        RPC_DG_CALL_LOCK_ASSERT(&h->shand->scall->c);

    /*
     * Copy out the reference to the cached CCALL.  Note that we have no
     * net change in references since we clear the reference from the
     * binding handle.
     */

    ccall = h->ccall;
    h->ccall = NULL;

    /*
     * The call handle is public in the sense that a call monitor
     * is running out there.
     */

    RPC_DG_CALL_LOCK(&ccall->c);
    
    RPC_DG_CALL_REINIT(&ccall->c);

    /*
     * If the call is not still "in-progress" (state != final), the call
     * must be in the idle state and we need to reacquire a CCTE.
     */

    if (ccall->c.state != rpc_e_dg_cs_final)
    {
        assert(ccall->c.state == rpc_e_dg_cs_idle);

        if (! doing_callback)
        {
            rpc__dg_cct_get(h->c.c.auth_info, ccall);
	if (ccall->ccte_ref.ccte == NULL){
	    rpc__dg_network_sock_release(&ccall->c.sock_ref);
	    RPC_DG_CALL_UNLOCK(&ccall->c);
	    *st = rpc_s_no_memory;
	    return(NULL);
	}
        }
    }

    prev_state = ccall->c.state;

    RPC_DG_CALL_SET_STATE(&ccall->c, rpc_e_dg_cs_init);

    /*
     * !!! some re-init really needs to be performed here or as part
     * of end call processing
     */

    if (ifspec->vers != ccall->c.call_if_vers 
        || ! UUID_EQ(ifspec->id, ccall->c.call_if_id, st)) 
    {
        ccall->c.call_ihint   = RPC_C_DG_NO_HINT;
        ccall->c.call_if_id   = ifspec->id; 
        ccall->c.call_if_vers = ifspec->vers; 
    }

    /*
     * Free any memory associated with a previously used EPAC.
     */
    if (ccall->auth_way_info != NULL)
    {
        RPC_MEM_FREE(ccall->auth_way_info, RPC_C_MEM_DG_EPAC);
    }

    ccall_common_init(h, ccall, options, ifspec, opnum, st);
    if (*st != rpc_s_ok)
    {
        RPC_DG_CALL_UNLOCK(&ccall->c);
        return (NULL);
    }

    /*
     * If the call used to not be in the final state, it'll need to be
     * re-inserted into the CCALLT.
     */

    *insert_in_ccallt = (prev_state != rpc_e_dg_cs_final);

    return (ccall);
}    


/*
 * C C A L L _ B I N D I N G _ S E R I A L I Z E
 *
 * Serialize calls on this binding handle until the handle has a bound
 * server instance.  This is necessary to maintain the promise: "All
 * calls to using a specific binding handle will go to the same server
 * instance".  Broadcast calls must always wait until there are no calls
 * in progress since they essentially unbind an existing server instance.
 */    

INTERNAL void ccall_binding_serialize
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_client_p_t h,
    boolean32 is_brdcst,
    volatile unsigned32  *cancel_cnt,
    volatile rpc_clock_p_t cancel_timeout_time,
    unsigned32 *st
)
#else
(h, is_brdcst, cancel_cnt, cancel_timeout_time, st)
rpc_dg_binding_client_p_t h;
boolean32 is_brdcst;
volatile unsigned32  *cancel_cnt;
volatile rpc_clock_p_t cancel_timeout_time;
unsigned32 *st;
#endif
{
    volatile boolean    is_awaiting_timeout = false;
    volatile boolean    has_timed_out = false;
    volatile struct timespec     delta, abstime;
    struct timespec     zero_delta, curtime;

    RPC_LOCK_ASSERT(0);        

    *st = rpc_s_ok;

    *cancel_cnt = 0;
    *cancel_timeout_time = RPC_CLOCK_SEC(0);

    zero_delta.tv_sec = 0;
    zero_delta.tv_nsec = 0;
    delta.tv_sec = 0;
    delta.tv_nsec = 0;

    /*
     * Get the thread's cancel timeout value (so we don't block forever).
     */
    RPC_GET_CANCEL_TIMEOUT(delta.tv_sec, st);
    if (*st != rpc_s_ok)
        return;

    /*
     * Wait for a useable binding handle.  If the user attempts to cancel
     * the call, don't wait longer than the cancel timeout time.
     */
    while (has_timed_out == false 
        && ((h->c.c.bound_server_instance == false || is_brdcst)
             && h->c.c.calls_in_progress != 0))
    {
#ifdef _PTHREAD_NO_CANCEL_SUPPORT
        /* 
         * avoid the TRY/CATCH overhead and just wait...
         * probably needs to be enhanced in the kernel to use
         * the "max call timeout".
         */
        RPC_BINDING_COND_WAIT(0);
#else
        TRY
            if (! is_awaiting_timeout)
            {
                RPC_BINDING_COND_WAIT(0);
            }
            else
            {
                RPC_BINDING_COND_TIMED_WAIT(&abstime);
                pthread_get_expiration_np(&zero_delta, &curtime);
                if (curtime.tv_sec == abstime.tv_sec)
                    has_timed_out = true;
            }
        CATCH(pthread_cancel_e)
            /*
             * Track the cancels and setup a cancel timeout value 
             * (if appropriate).
             */

            RPC_DBG_PRINTF(rpc_e_dbg_cancel, 5,
                ("(ccall_binding_serialize) cancel detected\n"));

            if (delta.tv_sec == 0)
                has_timed_out = true;
            else if (delta.tv_sec == rpc_c_cancel_infinite_timeout)
                ;   /* we never timeout */
            else
            {
                /*
                 * Mompute the max timeout time for the wait.
                 * Generate a cancel time stamp for use by the caller
                 * in subsequently setting up the call's cancel state.
                 */
                if (is_awaiting_timeout == false)
                {
                    RPC_DBG_PRINTF(rpc_e_dbg_cancel, 5,
                    ("(ccall_binding_serialize) %d sec cancel timeout setup\n",
                            delta.tv_sec));

                    pthread_get_expiration_np(&delta, &abstime);
                    *cancel_timeout_time = rpc__clock_stamp() + 
                                            RPC_CLOCK_SEC(delta.tv_sec);
                }
                is_awaiting_timeout = true;
            }

            *cancel_cnt += 1;

        /* 
         * Any other type of exception is something serious; just let it 
         * propagate and we die in our usual unclean fashion.
         */
        ENDTRY
#endif /* _PTHREAD_NO_CANCEL_SUPPORT */
    }

    if (has_timed_out)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_cancel, 5,
                ("(ccall_binding_serialize) cancel timeout\n"));

        *st = rpc_s_cancel_timeout;
    }
}

/*
 * C C A L L _ I N I T I A L _ C A N C E L _ S E T U P
 *
 * In the event that cancels were posted and detected before the typical
 * cancel detection code in call_wait(), setup the call now.  A ccall
 * value of NULL means that there is no call and we should repost previously
 * detected cancels to the thread.
 */

INTERNAL void ccall_initial_cancel_setup
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall,
    unsigned32 cancel_cnt,
    rpc_clock_t cancel_timeout_time
)
#else
(ccall, cancel_cnt, cancel_timeout_time)
rpc_dg_ccall_p_t ccall;
unsigned32 cancel_cnt;
rpc_clock_t cancel_timeout_time;
#endif
{

    if (cancel_cnt == 0)
        return;

    if (ccall == NULL)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_cancel, 5, 
            ("(ccall_initial_cancel_setup) reposting cancels\n"));

        for (; cancel_cnt--; )
            pthread_cancel(pthread_self());
        return;
    }

    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    ccall->cancel.timeout_time = cancel_timeout_time;
    ccall->cancel.local_count = cancel_cnt;
}


/*
 * R P C _ _ D G _ C C A L L _ S E T U P _ C A N C E L _ T M O
 *
 * Setup the call's cancel timeout mechanism based on the thread's
 * cancel timeout desires.
 */

#ifdef SGIMIPS
PRIVATE void rpc__dg_ccall_setup_cancel_tmo(
rpc_dg_ccall_p_t ccall)
#else
PRIVATE void rpc__dg_ccall_setup_cancel_tmo(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    signed32 ctmo = 0;
    unsigned32 tst;

    /*
     * Internally, a timeout_time of 0 indicates an infinite
     * timeout; it also means that a timeout has yet to be
     * established.
     */

    if (ccall->cancel.timeout_time == 0)
    {
        RPC_GET_CANCEL_TIMEOUT(ctmo, &tst);
        if (ctmo != rpc_c_cancel_infinite_timeout)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_cancel, 10,
                ("(rpc__dg_ccall_setup_cancel_tmo) %d sec cancel timeout setup\n",
                ctmo));

            ccall->cancel.timeout_time = rpc__clock_stamp() + 
                RPC_CLOCK_SEC(ctmo);
        }
    }
}


/*
 * R P C _ _ D G _ C A L L _ S T A R T
 *
 * Begin an RPC call. Returns an opaque call block.
 */

PRIVATE rpc_call_rep_p_t rpc__dg_call_start
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h_,
    unsigned32 options,
    rpc_if_rep_p_t ifspec,
    unsigned32 opnum,
    rpc_transfer_syntax_t *transfer_syntax,
    unsigned32 *st
)
#else
(h_, options, ifspec, opnum, transfer_syntax, st)
rpc_binding_rep_p_t h_;
unsigned32 options;
rpc_if_rep_p_t ifspec;
unsigned32 opnum;
rpc_transfer_syntax_t *transfer_syntax;
unsigned32 *st;
#endif
{
    rpc_dg_binding_client_p_t h = (rpc_dg_binding_client_p_t) h_;
    rpc_dg_ccall_p_t ccall;
    boolean doing_callback = RPC_BINDING_IS_SERVER(&h->c.c);
    unsigned32 cancel_cnt = 0;
    rpc_clock_t cancel_timeout_time;
    rpc_dg_scall_p_t scall;
    boolean32 is_brdcst = (options & rpc_c_call_brdcst) != 0;

    RPC_DG_STATS_INCR(calls_sent);

    /*
     * Initialize transfer syntax output param to be NDR, which is all
     * it can be at the moment (since we don't do transfer syntax
     * negotiation).
     */

    *transfer_syntax = ndr_g_transfer_syntax;

    /* 
     * If this is a call back, create a client binding handle for the call.
     * Lock the original SCALL.  Note that handle_server_to_client requires
     * that the global lock NOT be held.
     */

    if (doing_callback)
    {
        h = rpc__dg_binding_srvr_to_client( (rpc_dg_binding_server_p_t) h_, st);
        if (*st != rpc_s_ok)
            return(NULL);
        scall = ((rpc_dg_binding_server_p_t) (h->shand))->scall;
    }

    /*
     * Big hammer! But, we are losing time...
     */
    if (!rpc_g_is_single_threaded)
	rpc__timer_update_clock();

    RPC_LOCK(0);

    /*
     * Serialize calls on this binding handle until the handle has a bound
     * server instance.  This is necessary to maintain the promise: "All
     * calls to using a specific binding handle will go to the same server
     * instance".  Broadcast calls must always wait until there are no calls
     * in progress since they essentially unbind an existing server instance.
     * Note: it would probably be wrong to avoid the serialization in the case
     * where the endpoint was known to be the llb's - this would prevent the
     * introduction / use of other "forwarding servers" without modifications
     * to the client runtime.
     */

    if ((h->c.c.bound_server_instance == false || is_brdcst)
        && h->c.c.calls_in_progress != 0)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_general, 5, 
            ("(rpc__dg_call_start) serializing call...\n"));

        ccall_binding_serialize(h, is_brdcst, 
            &cancel_cnt, &cancel_timeout_time, st);
        if (*st != rpc_s_ok)
        {
            RPC_UNLOCK(0);
            ccall_initial_cancel_setup(NULL, cancel_cnt, 0);
            return (NULL);
        }
    }

    /*                                              
     * There are two situations in which we don't want to reuse a
     * a cached call handle.   The first is if the socket pool 
     * entry associated with the handle has been disabled.  The
     * second is if the auth_info in the cached call doesn't match 
     * the auth_info in the handle.  In either case, release the
     * cached call.
     *
     * Note that the auth_info in the call is referenced through the
     * key_info, which may be null, hence the complicated use of the
     * ternary operator to generate a truth value.
     */
    ccall = h->ccall;
    if (ccall != NULL)
    {
        rpc_key_info_p_t key_info = ccall->c.key_info;
        rpc_auth_info_p_t auth_info = h->c.c.auth_info;
        
        if (RPC_DG_SOCK_IS_DISABLED(ccall->c.sock_ref) || 
            ((auth_info != NULL) ?
                ((key_info == NULL) || (key_info->auth_info != auth_info))
            : (key_info != NULL)))          
        {
            RPC_DG_CALL_LOCK(&ccall->c);
            RPC_DG_CCALL_RELEASE(&h->ccall)
        }
    }

    /*
     * If this is for a callback, we're gonna need the associated scall.
     * To avoid subsequent locking hierarchy problems, just get the lock
     * now.  Note the locking hierarchy for this type of call handle
     * pairing is:  scall, is_cbk ccall (see dg.h).
     */

    if (doing_callback)
        RPC_DG_CALL_LOCK(&scall->c);

    /*
     * If we don't happen to have a call handle laying around in the binding
     * handle, then create a new one; otherwise reuse the old one.
     */

    if (h->ccall == NULL)
    {
        ccall = ccall_alloc(h, options, ifspec, opnum, st);
        if (*st != rpc_s_ok)
        {
            if (doing_callback)
                RPC_DG_CALL_UNLOCK(&scall->c);
            RPC_UNLOCK(0);
            ccall_initial_cancel_setup(NULL, cancel_cnt, 0);
            return (NULL);
        }
        rpc__dg_ccallt_insert(ccall);
    }
    else
    {
        boolean insert_in_ccallt;

        ccall = ccall_reinit(h, options, ifspec, opnum, &insert_in_ccallt, st);
        if (*st != rpc_s_ok)
        {
            if (doing_callback)
                RPC_DG_CALL_UNLOCK(&scall->c);
            RPC_UNLOCK(0);
            ccall_initial_cancel_setup(NULL, cancel_cnt, 0);
            return (NULL);
        }
        if (insert_in_ccallt)
        {
            rpc__dg_ccallt_insert(ccall);
        }
    }

    /*
     * The binding handle is actively being used in a call.
     */
    RPC_BINDING_CALL_START(&h->c.c);

    /*
     * If binding handle call serialization resulted in cancel detections,
     * setup the call's cancel state.
     */
    if (cancel_cnt)
        ccall_initial_cancel_setup(ccall, cancel_cnt, cancel_timeout_time);
          
    /*
     * We always start with the maximum reservation because we no longer
     * reset high_rcv_frag_size and snd_frag_size between the calls.
     * (The previous call may have used/advertised the larger fragment
     * size.)
     *
     * This is fine in the user space since the packet rationing will
     * never happen. (We assume that there are always enough packet
     * buffers available.)
     *
     * This may accelerate the packet rationing in the kernel, though
     * (iff MBF is turned on). Unfortunately, we can't start with the
     * minimum reservation in the kernel because the other end may be a
     * user space.
         */
    if (is_brdcst)
    {
        rpc__dg_pkt_adjust_reservation(&ccall->c,
                                       RPC_C_DG_MAX_NUM_PKTS_IN_FRAG, true);
    }
    else
    {
        rpc__dg_pkt_adjust_reservation(&ccall->c, ccall->c.max_resvs, true);
    }
     
    /*
     * Free all the lock we acquired.
     */

    if (doing_callback)
    {
        RPC_DG_CALL_UNLOCK(&scall->c);
    }
    RPC_DG_CALL_UNLOCK(&ccall->c);
    RPC_UNLOCK(0);

    /*
     * If we're using authentication, invoke the pre_call routine now.
     *
     * If it fails, mark the call as having failed already; our
     * callers will clean up.
     */
    
    if (!doing_callback &&
        ccall->c.key_info != NULL)
    {
        rpc_dg_auth_epv_p_t epv = ccall->c.auth_epv;
        error_status_t xst;
        
        (*epv->pre_call) (ccall->c.key_info, (handle_t)h, &xst);
	/*
	 * Auth pre_call may make an rpc which ends up starting the itimer.
	 * Not the best solution, but works...
	 */
	if (rpc_g_is_single_threaded)
	    (void)rpc__timer_itimer_stop();
        if (xst != rpc_s_ok) 
        {
            RPC_DG_CALL_LOCK(&ccall->c);
            rpc__dg_call_signal_failure(&ccall->c, xst);
            RPC_DG_CALL_UNLOCK(&ccall->c);
        }
    }

    *st = rpc_s_ok;
    return ((rpc_call_rep_p_t) ccall);
}


/*
 * R P C _ _ D G _ C A L L _ E N D
 *
 * RPC call complete, however, it may have a delayed ack pending.  Set
 * the caller's thread cancel state as required.  Opaque context block
 * no longer valid (from the caller's perspective).
 */

PRIVATE void rpc__dg_call_end
#ifdef _DCE_PROTO_
(
    rpc_call_rep_p_t *call_,
    unsigned32 *st
)
#else
(call_, st)
rpc_call_rep_p_t *call_;
unsigned32 *st;
#endif
{
    rpc_dg_ccall_p_t ccall;
    rpc_dg_binding_client_p_t h;

    *st = rpc_s_ok;

    ccall = (rpc_dg_ccall_p_t) *call_;
    *call_ = NULL;

    assert(RPC_DG_CALL_IS_CLIENT(&ccall->c));

    RPC_LOCK(0);
    RPC_DG_CALL_LOCK(&ccall->c);

    h = (rpc_dg_binding_client_p_t) ccall->h;

    /*
     * Cancel the packet reservation that was made for this call.
     * This call may no longer allocate transmit packets, or queue
     * received packets.
     */
  
    rpc__dg_pkt_cancel_reservation(&ccall->c); 

    /*
     * Perform the callback / non-callback ccall specific bookkeeping.
     * This is easier to do early (before we start changing the state
     * of things) given the locking hierarchy issues.
     */

    if (ccall->c.is_cbk)
    {
        rpc_dg_scall_p_t scall = ccall->cbk_scall;

        /*
         * This is a server side callback ccall.
         */

        /*
         * Note the locking hierarchy for this type of call handle
         * pairing is:  scall, is_cbk ccall (see dg.h).
         */

        RPC_DG_CALL_UNLOCK(&ccall->c);
        RPC_DG_CALL_LOCK(&scall->c);
        RPC_DG_CALL_LOCK(&ccall->c);

        /*
         * Update the original (server side) SCALL with the sequence
         * number returned as a result of this callback.
         */

        if (ccall->response_info_updated && ccall->cbk_scall != NULL)
            ccall->cbk_scall->c.high_seq = ccall->c.high_seq;

        /*
         * Update the original (server side) SCALL's high_rcv_frag_size
         * and snd_frag_size.
         */

        if (ccall->cbk_scall != NULL)
        {
            ccall->cbk_scall->c.rq.high_rcv_frag_size =
                ccall->c.rq.high_rcv_frag_size;
            ccall->cbk_scall->c.xq.snd_frag_size = ccall->c.xq.snd_frag_size;
        }

        RPC_DG_CALL_UNLOCK(&scall->c);
    }
    else
    {
        /*
         * This is a normal (client side) ccall.
         */
                         
        /*
         * Update the CCT with a new sequence number, if necessary.  This
         * condition will exist when the server manager made "proxy"
         * calls on our behalf (i.e. calls / callbacks using our
         * activity id).
         */

        if (RPC_DG_SEQ_IS_LT(ccall->c.call_seq, ccall->c.high_seq))
            ccall->ccte_ref.ccte->seq = ccall->c.high_seq + 1;

        /*
         * If this originating CCALL ended up handling a callback scall who's
         * resources (which are no longer necessary) have yet to be freed,
         * free them now.  Be sure to pickup the callback sequence number
         * if necessary (this condition should only exist if call_end() is
         * being called before the server had a chance to tell us it's proxy
         * sequence number).  Blindly picking up the sequence number would
         * be wrong because all proxy calls are not necessarily callbacks
         * (i.e. this call's response sequence number may be greater than
         * the callback's sequence number).  In the future, we may decide
         * to cache "normal" callback resources across calls.
         */

        if (ccall->cbk_scall != NULL)
        {
            rpc_dg_scall_p_t scall = ccall->cbk_scall;
            unsigned32 tst;
        
            assert(scall->cbk_ccall == ccall);
        
            ccall->cbk_start = false;

            /*
             * Note the locking hierarchy for this type of call handle
             * pairing is:  ccall, is_cbk scall (see dg.h).
             */
            RPC_DG_CALL_LOCK(&scall->c);

            /*
             * Release the reference from the server binding handle to the SCALL.
             * And dealloc the server binding handle.
             */

            RPC_DG_SCALL_RELEASE_NO_UNLOCK(&scall->h->scall);
            RPC_BINDING_RELEASE((rpc_binding_rep_p_t *) &scall->h, &tst);

            RPC_DG_CALL_STOP_TIMER(&scall->c);

            if (RPC_DG_SEQ_IS_LT(ccall->c.high_seq, scall->c.call_seq))
                ccall->c.high_seq = scall->c.call_seq;

            RPC_DG_CCALL_RELEASE_NO_UNLOCK(&scall->cbk_ccall);

            RPC_DG_SCALL_RELEASE(&ccall->cbk_scall);
        }
    }

    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    /*
     * The binding handle is no longer actively being used in a call
     * (this does not release the ccall's reference to the handle).
     */

    RPC_BINDING_CALL_END(&h->c.c);

    /*
     * Free any fault_rqe that may not have been processed.  I think
     * there are some circumstances where this can occur (even though
     * you'd like to think that the world was synchronous).
     */
    if (ccall->fault_rqe != NULL)
    {
        rpc__dg_pkt_free_rqe(ccall->fault_rqe, &ccall->c);
        ccall->fault_rqe = NULL;
    }

    /*
     * Determine if the call completed with a "cancel pending".  If so,
     * re-post the cancel.
     */
    if (ccall->cancel.server_had_pending
        || (ccall->cancel.local_count > ccall->cancel.server_count))
    {
        RPC_DBG_PRINTF(rpc_e_dbg_cancel, 5, 
            ("(rpc__dg_call_end) reposting cancel\n"));
        pthread_cancel(pthread_self());
    }

    /*
     * If the call is complete and non-idempotent, we now transition
     * to the "final" state (indicating a delayed acknowlegement is
     * pending).  Complete idempotent calls with large outs are handled
     * similarly so as to lessen the time a server has to retain /
     * retransmit the outs. Otherwise, the call is now "idle" and we
     * can clean up a few more things.  No need to keep calls with an
     * error hanging around any longer.
     *
     * In the case where the call ended prematurely, tell the server that it
     * is now orphaned.
     */
       
    if ((ccall->c.rq.all_pkts_recvd == true)
        && (ccall->c.status == rpc_s_ok)
        && ((ccall->c.xq.base_flags & RPC_C_DG_PF_IDEMPOTENT) == 0 
        || ccall->c.rq.recving_frags))
    {
        RPC_DG_CALL_SET_STATE(&ccall->c, rpc_e_dg_cs_final);
    }
    else 
    {
        /*
         * The call will transition to the idle state; but first...
         *
         * Check for a premature end-of-call.  If detected, the call
         * transitions to the orphan state and call_end() will wait for a
         * quack or quit timeout (be sure to not allow cancel delivery at
         * this point).
         */

        if (ccall->c.rq.all_pkts_recvd == false)
        {
            ccall->quit.next_time = rpc__clock_stamp() + RPC_CLOCK_SEC(1);
     
            RPC_DG_CALL_SET_STATE(&ccall->c, rpc_e_dg_cs_orphan);

            RPC_DBG_GPRINTF(("(rpc__dg_call_end) Sending orphan quit\n"));
            xmit_orphan_quit(ccall);
     
            RPC_UNLOCK(0);

            while (ccall->quit.quack_rcvd != true)
            {
                int oc;

                /*
                 * We don't use rpc__dg_call_wait() because we might be
                 * in this state because the call failed (call_wait()
                 * would immediately return).
                 */
                oc = pthread_setcancel(CANCEL_OFF);
		/*
		 * If the call is using a private socket, wait for a
		 * network event because the timer thread is not
		 * running.
		 */
		if (rpc_g_is_single_threaded && ccall->c.sock_ref->is_private)
		{
		    unsigned32 tst;

		    RPC_DBG_PRINTF(rpc_e_dbg_dg_sockets, 5, (
			"(rpc__dg_call_end) waiting on network event\n"));

		    rpc__dg_network_select_dispatch(ccall->c.sock_ref->sock, 
						    (pointer_t) ccall->c.sock_ref,
						    (boolean32) true, &tst);
		}
		else
                RPC_DG_CALL_COND_WAIT(&ccall->c);
                pthread_setcancel(oc);
            }

            /*
             * Reacquire the global lock.  Maintain the locking hierarchy
             * do prevent deadlocks (unlocking the call lock at this point
             * will not open a window that would expose non-sensical invariants
             * or otherwise allow others to mess up our state; I hope :-)
             */
            RPC_DG_CALL_UNLOCK(&ccall->c);
            RPC_LOCK(0);
            RPC_DG_CALL_LOCK(&ccall->c);

            /*
             * Since the call was orphaned, we don't really know what the
             * state of cancels are on the server.  Therefore, we behave
             * as if a forwarded cancel remains pending.
             */
            if (ccall->cancel.local_count > 0)
                pthread_cancel(pthread_self());
        }

        RPC_DG_CCALL_SET_STATE_IDLE(ccall);
    }


    /*
     * If we already have a cached call handle for the (binding) handle,
     * then free this one.  Otherwise, stash away the current call handle
     * in the handle.
     */

    if (h->ccall != NULL) 
    {
        rpc__dg_ccall_free_prep(ccall);
    }
    else 
    {
        RPC_DG_CALL_REFERENCE(&ccall->c);   /* For the following assignment */
        h->ccall = ccall;
    }

    RPC_DG_CCALL_RELEASE(&ccall);           /* Because our caller's reference is now invalid */

    RPC_UNLOCK(0);
}


/*
 * X M I T _ P I N G
 *
 * Transmit a "ping" appropriate to the state of the passed "call".
 */

PRIVATE void rpc__dg_ccall_xmit_ping
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    rpc_dg_xmitq_p_t xq = &ccall->c.xq;

    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    /*
     * Bump the serial number for this packet.  Since a ping may induce the server
     * to return a fack body, we want to make sure the serial number contained in
     * the fack in accurate.
     */
    xq->next_serial_num++;
    xq->hdr.serial_hi = (xq->next_serial_num & 0xFF00) >> 8;
    xq->hdr.serial_lo = xq->next_serial_num & 0xFF;

    RPC_DBG_PRINTF(rpc_e_dbg_xmit, 6,
		   ("(xmit_ping) Ping [%s]\n",
		    rpc__dg_act_seq_string(&ccall->c.xq.hdr)));

    rpc__dg_xmit_hdr_only_pkt(ccall->c.sock_ref->sock, ccall->c.addr, 
                              &xq->hdr, RPC_C_DG_PT_PING);
    ccall->c.last_ping_timestamp = rpc__clock_stamp();   
}


/*
 * R P C _ _ D G _ C C A L L _ A C K
 *
 * Transmit a "ack" (for calls in the final state).
 */

PRIVATE void rpc__dg_ccall_ack
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    assert(ccall->c.state == rpc_e_dg_cs_final);
         
    RPC_DBG_PRINTF(rpc_e_dbg_xmit, 5,
		   ("(rpc__dg_ccall_ack) sending ack [%s]\n",
		    rpc__dg_act_seq_string(&ccall->c.xq.hdr)));

    rpc__dg_xmit_hdr_only_pkt(ccall->c.sock_ref->sock, ccall->c.addr, 
                              &ccall->c.xq.hdr, 
        RPC_C_DG_PT_ACK);
}

/*
 * X M I T _ O R P H A N _ Q U I T
 *
 * Transmit a "quit" (for calls in the orphan state).
 */

INTERNAL void xmit_orphan_quit
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    rpc__dg_xmit_hdr_only_pkt(ccall->c.sock_ref->sock, ccall->c.addr, 
                              &ccall->c.xq.hdr, 
        RPC_C_DG_PT_QUIT);
}


/*
 * R P C _ _ D G _ C C A L L _ X M I T _ C A N C E L _ Q U I T
 *
 * Whip up a cancel-quit for the specified cancel event (cancel_id) and
 * send it on it's way.  Pre 2.0 receivers of a cancel-quit will treat
 * this as a plain quit (orphan-quit); which is the best they can do.
 */

PRIVATE void rpc__dg_ccall_xmit_cancel_quit
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall,
    unsigned32 cancel_id
)
#else
(ccall, cancel_id)
rpc_dg_ccall_p_t ccall;
unsigned32 cancel_id;
#endif
{
    rpc_socket_iovec_t iov[2];
    rpc_dg_pkt_hdr_t hdr;
#ifndef MISPACKED_HDR
    rpc_dg_quitpkt_body_t body;
#else
    rpc_dg_raw_quitpkt_body_t body;
#endif
    boolean b;

    /*
     * Create a pkt header initialized with the prototype hdr.
     */

    hdr = ccall->c.xq.hdr;

    RPC_DG_HDR_SET_PTYPE(&hdr, RPC_C_DG_PT_QUIT);

    hdr.flags       = 0;
    hdr.len         = RPC_C_DG_RAW_QUITPKT_BODY_SIZE;

    RPC_DG_HDR_SET_DREP(&hdr);

    /*
     * Create the quit packet's body.
     */

#ifndef MISPACKED_HDR
    body.vers       = RPC_C_DG_QUITPKT_BODY_VERS;
    body.cancel_id  = cancel_id;
#else
    !!!
#endif

    /*
     * Setup the iov and send the packet.
     */

    iov[0].base = (byte_p_t) &hdr;
    iov[0].len  = RPC_C_DG_RAW_PKT_HDR_SIZE;
    iov[1].base = (byte_p_t) &body;
    iov[1].len  = hdr.len;

    rpc__dg_xmit_pkt(ccall->c.sock_ref->sock, ccall->c.addr, iov, 2, &b);
}


/*
 * C C A L L _ C A N C E L _ T I M E R
 *
 * See if the cancel timeout has expired; if so, blow out the call.
 * If necessary, (re)send a cancel request to the server.
 */

INTERNAL void ccall_cancel_timer
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    rpc_clock_t now;
    /*
     * # of seconds between resending cancel requests.
     */
    static rpc_clock_t inter_cancel_time = RPC_CLOCK_SEC(2);

    /*
     * If in the typical case that no cancels have occurred, we're done.
     */
    if (ccall->cancel.local_count == 0)
        return;

    /*
     * If the call has already failed, we don't want to do any of this.
     */
    if (ccall->c.status != rpc_s_ok)
        return;

    now = rpc__clock_stamp();

    /*
     * Check to see if we've exceeded the time we're willing to wait
     * for the server to complete the call.
     *
     * If we did time out (while we don't know if the manager did or did not
     * see the cancel) we want to be certain that a cancel pending condition
     * exists when the call completes.
     *
     * Finally, signal a call failure.
     */
    if (ccall->cancel.timeout_time != 0
        && now >= ccall->cancel.timeout_time)
    {
        RPC_DBG_GPRINTF(("(ccall_cancel_timer) cancel timeout [%s]\n",
                rpc__dg_act_seq_string(&ccall->c.xq.hdr)));

        ccall->cancel.server_is_accepting = false;
        ccall->cancel.server_had_pending = true;

        rpc__dg_call_signal_failure(&ccall->c, rpc_s_cancel_timeout);
        return;
    }

    /*
     * (Re)transmit a cancel-request if we need to.  If we have
     * unack'ed cancel-requests and the server is still accepting em
     * send one along.  Make sure we don't do this too often (we don't
     * bother "backing-down" since this isn't too fast; why bother).
     *
     * Each cancel request contains the monotically incremented "cancel_id" 
     * of the unique cancel event that it represents.
     */
    if (ccall->cancel.local_count > ccall->cancel.server_count
        && ccall->cancel.server_is_accepting 
        && now >= ccall->cancel.next_time)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_cancel, 10,
                ("(ccall_cancel_timer) Sending cancel id: %d [%s]\n",
                ccall->cancel.local_count,
                rpc__dg_act_seq_string(&ccall->c.xq.hdr)));

        ccall->cancel.next_time = now + inter_cancel_time;

        rpc__dg_ccall_xmit_cancel_quit(ccall, ccall->cancel.local_count);
    }
}


/*
 * R E C V _ S T A T E _ T I M E R
 *
 * Timer processing for CCALLs in the "recv" state.  Checks for call
 * expiration.  Maybe send a ping to a server to see if it's alive and
 * doing some useful work for us.  If we get no responses to our pings
 * or if we keep getting nocall responses to our pings for an extended
 * period give up.
 */

INTERNAL void recv_state_timer
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    rpc_dg_ping_info_t *ping = &ccall->ping;
    rpc_clock_t now;
    rpc_dg_xmitq_p_t xq = &ccall->c.xq;
    rpc_dg_recvq_p_t rq = &ccall->c.rq;
    /*
     * # of seconds we'll ping for and the max inter ping time (for backing
     * off ping retransmits).
     * # of seconds we'll wait for a broadcast response.
     */
    static rpc_clock_t broadcast_wait_time = RPC_CLOCK_SEC(3);
    static com_timeout_params_t ccall_com_timeout_params[] = {
                            /* max_ping_time */
    /*
     * The client doesn't ping the server until 1 second has
     * passed, so the max_ping_time must be longer than 1+
     * seconds. Otherwise the server can never respond in
     * time to avoid the timeout.
     */
        /*  0 min */        {RPC_CLOCK_SEC(2)},
        /*  1 */            {RPC_CLOCK_SEC(3)},
        /*  2 */            {RPC_CLOCK_SEC(4)},
        /*  3 */            {RPC_CLOCK_SEC(8)},
        /*  4 */            {RPC_CLOCK_SEC(15)},
        /*  5 def */        {RPC_CLOCK_SEC(30)},
        /*  6 */            {RPC_CLOCK_SEC(2*30)},
        /*  7 */            {RPC_CLOCK_SEC(4*30)},
        /*  8 */            {RPC_CLOCK_SEC(8*30)},
        /*  9 */            {RPC_CLOCK_SEC(16*30)},
        /* 10 infinite */   {RPC_CLOCK_SEC(0)}
    };

    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    /*
     * If the call is in an error state or if we've received all packets
     * (but are still in the recv state because the stub hasn't gotten
     * to pulling out all of the data yet) then there's no need to keep
     * or start pinging.
     */

    if (ccall->c.status != rpc_s_ok || rq->all_pkts_recvd)
    {
        return;
    }

    /*
     * Deal with cancel-request (Re)transmissions.
     */
    ccall_cancel_timer(ccall);

    /*
     * If this is a broadcast call, see if we've waited long enough.  In
     * any case, return right away -- there's nothing for us to do.
     */

    if ((xq->base_flags & RPC_C_DG_PF_BROADCAST) != 0)
    {
        if (rpc__clock_aged(ccall->c.start_time, broadcast_wait_time))
        {
            RPC_DBG_GPRINTF(("(recv_state_timer) Call expiration time reached [%s]\n", 
                rpc__dg_act_seq_string(&xq->hdr)));
            rpc__dg_call_signal_failure(&ccall->c, rpc_s_comm_failure);
        }
        return;
    }

    /*
     * If we're not currently pinging ...
     */

    if (! ping->pinging)
    {
        /*
         * If it's time to start pinging...
         */

        now = rpc__clock_stamp();
        if (now >= ping->next_time)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                ("(recv_state_timer) Starting to ping (rq->next_fragnum=%u) [%s]\n", 
                ccall->c.rq.next_fragnum,
                rpc__dg_act_seq_string(&xq->hdr)));

            /*
             * If we're awaiting an acknowledgment and we've waited too long,
             * then blow off the call.  Otherwise start pinging.
             */

            if (rpc__dg_xmitq_awaiting_ack_tmo(xq, ccall->c.com_timeout_knob))
            {
		rpc_dg_sock_pool_elt_p_t sp = ccall->c.sock_ref;

		if (!(rpc_g_is_single_threaded &&
		      ccall->c.blocked_in_receive) &&
		    RPC_CLOCK_IS_LT(ccall->c.last_rcv_priv_timestamp,
				    xq->timestamp) &&
		    RPC_DG_SOCK_IS_BLOCKING_IO(sp))
		{
		    /*
		     * We havn't called recv_pkt_private() yet, probably
		     * because a callout to an [out] pipe push routine
		     * gets blocked.
		     *
		     * We will not timeout unless we have called
		     * recv_pkt_private() at least once.
		     *
		     * If single-threaded, we are probably
		     * in recv_pkt_private().
		     */
		    RPC_DBG_GPRINTF(
	("(recv_state_timer) &ccall->c=%08x ignore ack timeout, no recv called %lu < %lu\n", 
			 &ccall->c, ccall->c.last_rcv_priv_timestamp,
			 xq->timestamp));
		}
		else
		{
                rpc__dg_call_signal_failure(&ccall->c, rpc_s_comm_failure);
		    return;
		}
	    }
                ping->pinging = true;
                ping->count = 0;
                ping->start_time = now;
                RPC_PING_INFO_NEXT(ping, now);
                rpc__dg_ccall_xmit_ping(ccall);
        }
    }
    else
    {
        /*
         * We're already pinging.  See if we've pinged too long.  If
         * we have blow off the call.  Otherwise, ping again.
         */

        if (rpc__clock_aged(ping->start_time, 
            ccall_com_timeout_params[ccall->c.com_timeout_knob].max_ping_time)
            && ccall->c.com_timeout_knob != rpc_c_binding_infinite_timeout)
        {
            RPC_DBG_GPRINTF(("(recv_state_timer) Ping timeout [%s]\n", 
                    rpc__dg_act_seq_string(&xq->hdr)));
            rpc__dg_call_signal_failure(&ccall->c, rpc_s_comm_failure);
        }
        else
        {
            now = rpc__clock_stamp();
            if (now >= ping->next_time)
            {
                RPC_DBG_PRINTF(rpc_e_dbg_general, 2, 
                    ("(recv_state_timer) Re-pinging (rq->next_fragnum=%u) [%s]\n", 
                    ccall->c.rq.next_fragnum,
                    rpc__dg_act_seq_string(&xq->hdr)));
                RPC_PING_INFO_NEXT(ping, now);
                rpc__dg_ccall_xmit_ping(ccall);
            }
        }
    }
}

/*
 * C C A L L _ O R P H A N _ T I M E R
 *
 * Resend an orphan quit packet to the server.  
 */

INTERNAL void ccall_orphan_timer
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    struct rpc_dg_quit_info_t *quit = &ccall->quit;
    rpc_clock_t now;
    /*
     * # of seconds between resending quit packets.
     */
    static rpc_clock_t inter_quit_time = RPC_CLOCK_SEC(1);

    now = rpc__clock_stamp();
    if (now >= quit->next_time)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_cancel, 10, 
                ("(ccall_orphan_timer) Resending orphan quit [%s]\n", 
                rpc__dg_act_seq_string(&ccall->c.xq.hdr)));
        quit->next_time = now + inter_quit_time;
        xmit_orphan_quit(ccall);
    }
}

/*
 * C C A L L _ T I M E O U T _ T I M E R
 *
 * Check for and handle a call execution time timeout.
 */
INTERNAL void ccall_timeout_timer
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    rpc_clock_t now;

    if (ccall->timeout_stamp == 0)
        return;

    now = rpc__clock_stamp();
    if (now >= ccall->timeout_stamp)
    {
        RPC_DBG_GPRINTF(("(ccall_timeout_timer) call timeout\n"));
        rpc__dg_call_signal_failure(&ccall->c, rpc_s_call_timeout);
    }
}

/*
 * R P C _ _ D G _ C C A L L _ F R E E
 *
 * Free the resources associated with a CCALL as well as the CCALL
 * object's storage.
 *
 * This is *the* routine to use to free a CCALL.
 */

PRIVATE void rpc__dg_ccall_free
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    assert(ccall->c.refcnt == 0);

    /*
     * Generate a pending delated ack if necessary.
     */

    if (ccall->c.state == rpc_e_dg_cs_final) 
    {
        rpc__dg_ccall_ack(ccall);
    }

    assert(ccall->cbk_scall == NULL);

    /*
     * If this client needed to use a multi-packet PAC,
     * free the storage assiciated with that packet.
     */
    if (ccall->auth_way_info != NULL)
    {
        RPC_MEM_FREE(ccall->auth_way_info, RPC_C_MEM_DG_EPAC);
    }

    /*
     * Release all CALL common resources.
     * This must be the last step.
     */

    rpc__dg_call_free(&ccall->c);

    RPC_MEM_FREE(ccall, RPC_C_MEM_DG_CCALL);
    /* ccall may no longer be referenced */
}


/*
 * R P C _ _ D G _ C C A L L _ F R E E _ P R E P
 *
 * Perform the standard processing to prepare for the freeing
 * of a ccall.  Shut down the ccall's timer and transition to the idle
 * state.
 */

PRIVATE void rpc__dg_ccall_free_prep
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    RPC_DG_CALL_STOP_TIMER(&ccall->c);
    if (ccall->c.state != rpc_e_dg_cs_idle)
        RPC_DG_CCALL_SET_STATE_IDLE(ccall);
}


/*
 * C C A L L _ U N C A C H E
 *
 * Remove the reference a binding handle has to the specified CCALL.  This
 * routine is called from (and can only be called from) the CCALL timer.
 */

INTERNAL void ccall_uncache
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{


    RPC_LOCK_ASSERT(0);
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);

    assert(ccall->c.state == rpc_e_dg_cs_idle);

    /*
     * If this CCALL was being cached, remove it from the cache.
     */

    if (ccall->h != NULL && ccall->h->ccall == ccall) 
    {
        RPC_DG_CCALL_RELEASE_NO_UNLOCK(&ccall->h->ccall);
    }

    rpc__timer_clear(&ccall->c.timer);
    RPC_DG_CCALL_RELEASE(&ccall); 
}


/*
 * R P C _ _ D G _ C C A L L _ T I M E R
 *
 * Timer routine that runs periodically to do timer-based things on client calls.
 */

PRIVATE void rpc__dg_ccall_timer
#ifdef _DCE_PROTO_
(
    pointer_t p
)
#else
(p)
pointer_t p;
#endif
{
    rpc_dg_ccall_p_t ccall = (rpc_dg_ccall_p_t) p;
   /*
    * # of seconds we'll cache an idle CCALL before freeing it
    * # of seconds we'll delay sending a call acknowledgement
    * # of seconds we'll remain in the orphan state.
    */
    static rpc_clock_t max_idle_time = RPC_CLOCK_SEC(30);
    static rpc_clock_t max_final_time = RPC_CLOCK_SEC(3);
    static rpc_clock_t max_orphan_time = RPC_CLOCK_SEC(3);

    RPC_LOCK(0);
    RPC_DG_CALL_LOCK(&ccall->c);

    if (ccall->c.stop_timer)
    {
        rpc__timer_clear(&ccall->c.timer);
        RPC_DG_CCALL_RELEASE(&ccall); 
        RPC_UNLOCK(0);
        return;
    }

    switch (ccall->c.state) 
    {
        case rpc_e_dg_cs_init:
            ccall_cancel_timer(ccall);
            ccall_timeout_timer(ccall);
            break;

        case rpc_e_dg_cs_idle:
            /*
             * If the call has been idle for a long time, stop caching
             * it.  In the case of a callback CCALL, do nothing; the
             * originating SCALL's processing dictates when this cached
             * CCALL finally gets freed.
             */

            if (! ccall->c.is_cbk)
            {
                if (rpc__clock_aged(ccall->c.state_timestamp, 
                                    max_idle_time))
                {
                    ccall_uncache(ccall);
                    RPC_UNLOCK(0);
                    return;
                }
            }
            break;

        case rpc_e_dg_cs_xmit:
            /*
             * Retransmit frags and cancels if necessary.
             */
            rpc__dg_call_xmitq_timer(&ccall->c);
            ccall_cancel_timer(ccall);
            ccall_timeout_timer(ccall);
            break;

        case rpc_e_dg_cs_recv:
            recv_state_timer(ccall);
            ccall_timeout_timer(ccall);
            break;

        case rpc_e_dg_cs_final:
            /*        
             * Send an ACK if it's time.
             */
            if (rpc__clock_aged(ccall->c.state_timestamp, max_final_time))
            {
                RPC_DG_CCALL_SET_STATE_IDLE(ccall);
            }
            break;

        case rpc_e_dg_cs_orphan:
            /*
             * Send orphan quits if necessary.  If no "quack" is received,
             * wakeup the call executor in rpc__dg_call_end() to clean
             * up the resources held by the call. The transition to the
             * idle state is made in rpc__dg_call_end().
             */
            if (rpc__clock_aged(ccall->c.state_timestamp, max_orphan_time))
            {
                RPC_DBG_GPRINTF(("rpc__dg_ccall_timer) Orphan timeout\n"));
                ccall->quit.quack_rcvd = true;
                rpc__dg_call_signal_failure(&ccall->c, rpc_s_comm_failure);
            }
            else
            {  
                ccall_orphan_timer(ccall);
            }
            break;
    }

    RPC_DG_CALL_UNLOCK(&ccall->c);
    RPC_UNLOCK(0);
}
