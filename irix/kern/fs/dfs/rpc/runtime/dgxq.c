/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgxq.c,v 65.5 1998/03/23 17:29:14 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgxq.c,v $
 * Revision 65.5  1998/03/23 17:29:14  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:20  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:23  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:00  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.530.2  1996/02/18  00:04:13  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:34  marty]
 *
 * Revision 1.1.530.1  1995/12/08  00:20:29  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/4  1995/08/29  19:19 UTC  tatsu_s
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.psock_timeout.b0/1  1995/08/16  20:26 UTC  tatsu_s
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 
 * 	HP revision /main/HPDCE02/3  1995/05/25  21:41 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:01 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:16 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  20:06 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:59:24  root]
 * 
 * Revision 1.1.528.3  1994/06/24  15:49:39  tatsu_s
 * 	Turned off the first fack wait logic.
 * 	[1994/06/24  01:56:55  tatsu_s]
 * 
 * Revision 1.1.528.2  1994/05/27  15:36:43  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:57:12  tatsu_s]
 * 
 * 	Fixed the (re-)initialization of xq->first_fack_seen.
 * 	[1994/05/16  21:15:04  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:20:00  tatsu_s]
 * 
 * Revision 1.1.528.1  1994/01/21  22:37:35  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:43  cbrooks]
 * 
 * Revision 1.1.4.4  1993/03/11  20:52:48  markar
 * 	     OT CR 7420 fix: Clear retransmit state in xmitq_free()
 * 	[1993/03/11  19:19:49  markar]
 * 
 * Revision 1.1.4.3  1993/01/03  23:25:10  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:46  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:50:14  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:32  zeliff]
 * 
 * Revision 1.1.2.3  1992/05/07  20:00:51  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:39:01  markar]
 * 
 * Revision 1.1.2.2  1992/05/01  17:21:33  rsalz
 * 	 05-feb-92 sommerfeld    Clean up stylistic problems.
 * 	 05-feb-92 sommerfeld    key reorg.
 * 	 31-oct-91 sommerfeld    don't authenticate non-data packets.
 * 	[1992/05/01  17:17:03  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:00  devrcs
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
**      dgxq.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handles transmit queues.
**
**
*/

#include <dg.h>
#include <dgxq.h>
#include <dgpkt.h>
#include <dgcall.h>

#include <dce/assert.h>

/*========================================================================= */
              
#define MAX_SENDMSG_RETRIES 5

/*========================================================================= */

#define XQE_FREE_LIST_MAX_LENGTH    32

INTERNAL struct {
    rpc_dg_xmitq_elt_p_t head;
    unsigned16 length;
} xqe_free_list;

/*========================================================================= */

/*
 * com_timeout controllable parameters...
 *
 * max_xq_awaiting_ack_time:
 *     The maximum time that we will attempt to retransmit pkts to a receiver
 *     without any apparent type of acknowledgment.  This is a general xq
 *     related value and mechanism that removes the need for the old server
 *     specific "final" state timeout processing.
 */

typedef struct {
    unsigned32 max_xq_awaiting_ack_time;
} com_timeout_params_t;

/*========================================================================= */

/*
 * R P C _ _ D G _ X M I T Q _ A W A I T I N G _ A C K _ T M O
 *
 * Return true iff the xmitq is awaiting an ack and it's been waiting
 * too long.
 */

PRIVATE boolean rpc__dg_xmitq_awaiting_ack_tmo
#ifdef _DCE_PROTO_
(
    rpc_dg_xmitq_p_t xq,
    unsigned32 com_timeout_knob
)
#else
(xq, com_timeout_knob)
rpc_dg_xmitq_p_t xq;
unsigned32 com_timeout_knob;
#endif
{
    rpc_clock_t timestamp, wait_time;
    static com_timeout_params_t xq_com_timeout_params[] = {
                            /* max_xq_awaiting_ack_time */
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

    timestamp = xq->awaiting_ack_timestamp;
    wait_time = xq_com_timeout_params[com_timeout_knob].max_xq_awaiting_ack_time;

    if (xq->awaiting_ack && rpc__clock_aged(timestamp, wait_time) 
        && com_timeout_knob != rpc_c_binding_infinite_timeout)
    {
        RPC_DBG_GPRINTF(
            ("(rpc__dg_xmitq_awaiting_ack_tmo) timeout (timestamp=%lu, wait_time=%lu, now=%lu) [%s]\n", 
            timestamp, wait_time, rpc__clock_stamp(),
            rpc__dg_act_seq_string(&xq->hdr)));
        return (true);
    }
    else
    {
        return (false);
    }
}


/*
 * S E N D _ B R O A D C A S T
 *
 * Send a datagram out all the appropriate broadcast addresses.
 */

INTERNAL void send_broadcast _DCE_PROTOTYPE_((
        rpc_dg_call_p_t  /*call*/,
        rpc_socket_iovec_p_t  /*iov*/,
        int  /*iovlen*/
    ));

INTERNAL void send_broadcast
#ifdef _DCE_PROTO_
(
    rpc_dg_call_p_t call,
    rpc_socket_iovec_p_t iov,
    int iovlen
)
#else
(call, iov, iovlen)
rpc_dg_call_p_t call;
rpc_socket_iovec_p_t iov;
int iovlen;
#endif
{
    unsigned32 st;
    rpc_socket_error_t serr;
    int i, sentcc, sendcc;
    rpc_dg_sock_pool_elt_p_t sp;       
    unsigned_char_p_t endpoint = NULL;
    rpc_dg_ccall_p_t ccall = (rpc_dg_ccall_p_t) call;
           
    assert(RPC_DG_CALL_IS_CLIENT(call));

    sp = ccall->c.sock_ref;

    /*
     * See if we have already inquired about the broadcast addresses
     * for this socket.  If not, enable broadcasts on the socket (if
     * necessary) and call the NAF routine for the broadcast addresses.
     * (Note that we're ignoring the result of "rpc__socket_set_broadcast"
     * because what's the worst that can happen?)
     */
  
    if (sp->brd_addrs == NULL)
    {
        (void) rpc__socket_set_broadcast(sp->sock);

        rpc__naf_get_broadcast(rpc_g_protseq_id[sp->pseq_id].naf_id, 
                               ccall->h->c.c.rpc_addr->rpc_protseq_id, 
                               &sp->brd_addrs, &st);
        if (st != rpc_s_ok)
        {
            return;
        }
    }

    sendcc = 0;
    for (i = 0; i < iovlen; i++)
        sendcc += iov[i].len;

    rpc__naf_addr_inq_endpoint(call->addr, &endpoint, &st);
    if (st == rpc_s_no_memory){
	RPC_DBG_GPRINTF(("(send_broadcast) rpc__naf_addr_inq_endpoint failed, status = %x\n",
			st));
	return;
    }

    for (i = 0; i < sp->brd_addrs->len; i++) 
    {   
        rpc__naf_addr_set_endpoint(endpoint, &sp->brd_addrs->addrs[i], &st);                                   

        RPC_DG_SOCKET_SENDMSG_OOL(sp->sock, iov, iovlen, 
                sp->brd_addrs->addrs[i], &sentcc, &serr);

        RPC_DG_SOCK_UPDATE_ERR_COUNT(sp, serr);

        if (RPC_SOCKET_IS_ERR(serr) || sentcc != sendcc)
        {
            RPC_DBG_GPRINTF(("(send_broadcast) sendmsg failed, sendcc = %d, sentcc = %d, error = %d\n", 
                sendcc, sentcc, RPC_SOCKET_ETOI(serr)));
            break; 
        }
        RPC_DG_STATS_INCR(pkts_sent);
        RPC_DG_STATS_INCR(brds_sent);
    }

    RPC_MEM_FREE(endpoint, RPC_C_MEM_STRING);
}    


/*
 * R P C _ _ D G _ X M I T Q _ E L T _ X M I T
 *
 * Send the request/response packet denoted by the transmit queue element
 * on the specified call.
 */

PRIVATE void rpc__dg_xmitq_elt_xmit
#ifdef _DCE_PROTO_
(
    rpc_dg_xmitq_elt_p_t xqe,
    rpc_dg_call_p_t call,                               
    boolean32 block
)
#else
(xqe, call, block)
rpc_dg_xmitq_elt_p_t xqe;
rpc_dg_call_p_t call;                               
boolean32 block;
#endif
{
    rpc_socket_iovec_t iov[RPC_C_DG_MAX_NUM_PKTS_IN_FRAG+1];
    int iovlen;
    rpc_dg_xmitq_elt_p_t last_xqe = xqe;
    rpc_dg_xmitq_p_t xq = &call->xq;
    rpc_key_info_p_t key_info;
    rpc_dg_auth_epv_p_t auth_epv;
    int sentcc, sendcc;
    unsigned32  original_seq;
    rpc_socket_error_t serr;
    unsigned16 i;
    int ptype;
#ifdef MISPACKED_HDR
    rpc_dg_raw_pkt_hdr_t raw_hdr;
#endif

    RPC_DG_CALL_LOCK_ASSERT(call);

    /* 
     * First, make sure the socket we're about to use has not been
     * disabled.  If it has been, we might as well fault the call 
     * now.
     */

    if (RPC_DG_SOCK_IS_DISABLED(call->sock_ref))
    {                        
        RPC_DBG_PRINTF(rpc_e_dbg_xmit, 5, 
            ("(rpc__dg_xmitq_elt_xmit) socket %d has been disabled\n", 
            call->sock_ref->sock));
        rpc__dg_call_signal_failure(call, rpc_s_socket_failure);
        return;
    }      

    key_info = call->key_info;
    auth_epv = call->auth_epv;

    /*
     * Fill in the prototype header in the transmit queue using values
     * from transmit queue element.
     */

    xq->hdr.fragnum = xqe->fragnum;
    xq->hdr.flags   = xq->base_flags | xqe->flags;
    xq->hdr.flags2  = xq->base_flags2;
    xq->hdr.len     = xqe->frag_len;
    xq->hdr.auth_proto = 0;

    /*
     * Tag this packet with a serial number, and record that number
     * into the packet header.
     */

    xqe->serial_num = xq->next_serial_num++;
    xq->hdr.serial_hi = (xqe->serial_num & 0xFF00) >> 8;
    xq->hdr.serial_lo = xqe->serial_num & 0xFF;;

    
    ptype = RPC_DG_HDR_INQ_PTYPE(&xq->hdr);

    /* 
     * For response packets we need to use the highest sequence number
     * we've seen up to this time, so that the client can keep track of
     * the sequence number space.  To work with 1.5.1 clients, we still
     * need to use the original sequence number for facks, ping responses,
     * etc.
     */

    if (ptype == RPC_C_DG_PT_RESPONSE)
    {
        original_seq = xq->hdr.seq;
        xq->hdr.seq = call->high_seq;
    }

    /*
     * Only authenticate data packets -- setting key_info to NULL
     * disables authentication machinery for this packet.
     */

    if (!RPC_DG_PT_IS_DATA(ptype))
        key_info = NULL;
    
    /*
     * Set "last frag" bit as necessary.  (Note that we make sure that
     * last frags never ask for facks.  Also, we don't set any of the
     * frag bits if all the data is being sent in a single packet [i.e.,
     * we're NOT fragmenting at all]).
     */

    if (xq->push && xqe == xq->tail && 
            RPC_DG_FLAG_IS_SET(xq->base_flags, RPC_C_DG_PF_FRAG))
        xq->hdr.flags |= RPC_C_DG_PF_LAST_FRAG | RPC_C_DG_PF_NO_FACK;

    /*
     * If this is the call's final xqe (fragmented or not) or we're
     * requesting a fack, then start up the awaiting_ack detection
     * machinery (if it isn't already setup).  The receiver needs to
     * send a xmitq acknowledgment (i.e. a fack, working, response or
     * ack pkt) sooner or later.
     */

    if (! xq->awaiting_ack 
        && (! RPC_DG_FLAG_IS_SET(xq->base_flags, RPC_C_DG_PF_FRAG)
            || RPC_DG_HDR_FLAG_IS_SET(&xq->hdr, RPC_C_DG_PF_LAST_FRAG)
            || ! RPC_DG_HDR_FLAG_IS_SET(&xq->hdr, RPC_C_DG_PF_NO_FACK)))
    {
        RPC_DG_XMITQ_AWAITING_ACK_SET(xq);
    }

    /*
     * If we're authenticating, set auth_proto field in packet header
     * to indicate which auth protocol we're using.
     */
    if (key_info != NULL)
        xq->hdr.auth_proto = auth_epv->auth_proto;
    
    /*
     * Set up an I/O vector with two elements:  The first points to the
     * header (from the transmit queue) and the second points to the
     * body (from the transmit queue element).  Note that in the case
     * of "mispacked header" systems, we have to make a correctly formatted
     * header before we can transmit.
     */

    iov[0].base = (byte_p_t) &xq->hdr;
    iov[0].len  = RPC_C_DG_RAW_PKT_HDR_SIZE;
    iovlen = 1;
    sendcc = iov[0].len;

    iov[1].base = (byte_p_t) xqe->body;
    iov[1].len  = xqe->body_len;

    if (xqe->body_len != 0)
    {
        iov[iovlen].base = (byte_p_t) last_xqe->body;
        iov[iovlen].len  = last_xqe->body_len;
        sendcc += iov[iovlen++].len;

        while (last_xqe->more_data != NULL)
        {
            iov[iovlen].base = (byte_p_t) last_xqe->more_data->body;
            iov[iovlen].len  = last_xqe->more_data->body_len;
            sendcc += iov[iovlen++].len;
            last_xqe = last_xqe->more_data;
    }
    }

    assert(iovlen > 0 && iovlen <= RPC_C_DG_MAX_NUM_PKTS_IN_FRAG + 1);
    assert(xqe->frag_len == 0 ||
           sendcc == (xqe->frag_len + RPC_C_DG_RAW_PKT_HDR_SIZE));

    xqe->frag_len = xq->hdr.len = sendcc - RPC_C_DG_RAW_PKT_HDR_SIZE;
                           
    RPC_DBG_PRINTF(rpc_e_dbg_xmit, 5, 
        ("(rpc__dg_xmitq_elt_xmit) %s %lu.%u.%u len=%lu %s\n", 
        rpc__dg_pkt_name(RPC_DG_HDR_INQ_PTYPE(&call->xq.hdr)), 
        call->xq.hdr.seq, xqe->fragnum, 
        xqe->serial_num, xq->hdr.len, 
        RPC_DG_HDR_FLAG_IS_SET(&xq->hdr, RPC_C_DG_PF_NO_FACK) ? "" : "frq"));

#ifdef MISPACKED_HDR
    /* !!! ...compress hdr pointed to by iov[0] into raw_hdr... !!! */
    hdrp = iov[0].base;
    compress_hdr(&xq->hdr, &raw_hdr);
    iov[0].base = (byte_p_t) &raw_hdr;
#endif

    /*
     * Attach any authentication information now.
     */

    if (key_info != NULL) 
    {
        unsigned32 st;
        int overhead;
        
#ifdef INLINE_PACKETS
        pointer_t cksum = last_xqe->body + last_xqe->body_len;
#else
        pointer_t cksum = last_xqe->body->args + last_xqe->body_len;
#endif

        (*auth_epv->pre_send) (key_info, xqe, &xq->hdr, iov, iovlen, cksum, &st);
        if (st != 0) 
        {
            RPC_DBG_GPRINTF(
                ("(rpc__dg_xmitq_elt_xmit) auth pre_send failed, status = %x\n",
                st));
            rpc__dg_call_signal_failure (call, st);
            return;
        }
        overhead = auth_epv->overhead;
        
        if (iovlen == 1)
        {
        iovlen = 2;
        iov[1].len += overhead;
        }
        else
        {
            iov[iovlen-1].len += overhead;
        }
        sendcc += overhead;
    }
    RPC_DBG_PRINTF(rpc_e_dbg_xmit, 5,
        ("(rpc__dg_xmitq_elt_xmit) iovlen %lu, sendcc %lu\n",
         iovlen, sendcc));
    
    /*
     * Send out the datagram (checking whether we should broadcast it).
     * For the non-broadcast cast, we accept a few EWOULDBLOCK send
     * failures (by sleeping for a bit and then trying again).  We wish
     * we could have the socket in blocking I/O mode, but the receive
     * path really wants the socket in NON-blocking mode.
     */

    if (RPC_DG_FLAG_IS_SET(call->xq.base_flags, RPC_C_DG_PF_BROADCAST))
    {
        send_broadcast(call, iov, iovlen);
    }
    else
    {
        for (i = 0; i < MAX_SENDMSG_RETRIES; i++)
        {
            RPC_DG_SOCKET_SENDMSG(call->sock_ref->sock, iov, iovlen, 
                               call->addr, &sentcc, &serr);

            RPC_DG_SOCK_UPDATE_ERR_COUNT(call->sock_ref, serr);

            if (! RPC_SOCKET_IS_ERR(serr) && sentcc == sendcc)
            {
                RPC_DG_STATS_INCR(pkts_sent);
                RPC_DG_STATS_INCR(pstats[RPC_DG_HDR_INQ_PTYPE(&xq->hdr)].sent);
                break;
            }

            if (! RPC_SOCKET_ERR_EQ(serr, RPC_C_SOCKET_EWOULDBLOCK))
            {
                RPC_DBG_GPRINTF(
                    ("(rpc__dg_xmitq_elt_xmit) sendmsg failed, sendcc = %d, sentcc = %d, error = %d\n", 
                    sendcc, sentcc, RPC_SOCKET_ETOI(serr)));
                break;
            }  

            if (! block)
                break;

            /*
             * Handle EWOULDBLOCKs waiting for the condition to go away.
             */
        
            RPC_DBG_PRINTF(rpc_e_dbg_xmit, 2, 
                ("(rpc__dg_xmitq_elt_xmit) sendmsg failed with EWOULDBLOCK; waiting\n"));

            rpc__socket_nowriteblock_wait(call->sock_ref->sock, NULL);
        }    
    }

    xq->timestamp = rpc__clock_stamp();   

    if (RPC_DG_HDR_INQ_PTYPE(&xq->hdr) == RPC_C_DG_PT_RESPONSE)
    {
        xq->hdr.seq = original_seq;
    }
}


/*
 * R P C _ _ D G _ X M I T Q _ I N I T
 *
 * Initialize a transmit queue (rpc_dg_xmit_q_t).  Note that we DON'T
 * fill in (all) the prototype packet header here because we don't have
 * enough info to do it at this point.
 */

PRIVATE void rpc__dg_xmitq_init
#ifdef _DCE_PROTO_
(
    rpc_dg_xmitq_p_t xq
)
#else
(xq)
rpc_dg_xmitq_p_t xq;
#endif
{
    /*
     * Presumably the call is either locked or "private" at this point
     * RPC_DG_CALL_LOCK_ASSERT(call);
     */
      
    rpc__dg_xmitq_reinit(xq);

    xq->max_rcv_tsdu                = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
    xq->max_snd_tsdu                = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
    xq->max_frag_size               = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
    xq->snd_frag_size               = RPC_C_DG_INITIAL_MAX_PKT_SIZE;
    xq->max_blast_size              = RPC_C_DG_INITIAL_MAX_BLAST_SIZE;
    xq->xq_timer                    = RPC_C_DG_INITIAL_XQ_TIMER;
    xq->xq_timer_throttle           = 1;
    xq->high_cwindow                = 0;

    /*
     * Initialize some highly constant fields (RPC protocol version and
     * local NDR drep) in the xmitq's prototype packet header.
     */

    RPC_DG_HDR_SET_VERS(&xq->hdr);
    RPC_DG_HDR_SET_DREP(&xq->hdr);

    xq->hdr.auth_proto = 0;
}


/*
 * R P C _ _ D G _ X M I T Q _ R E I N I T
 *
 * Reinitialize a transmit queue (rpc_dg_xmit_q_t).  Note that we DON'T
 * fill in the prototype packet header here because we don't have enough
 * info to do it at this point.  Calling this routine will retain some
 * state between calls; see xmitq_init for which state is retained.
 */

PRIVATE void rpc__dg_xmitq_reinit
#ifdef _DCE_PROTO_
(
    rpc_dg_xmitq_p_t xq
)
#else
(xq)
rpc_dg_xmitq_p_t xq;
#endif
{
    /*
     * Presumably the call is either locked or "private" at this point
     * RPC_DG_CALL_LOCK_ASSERT(call);
     */

    xq->head = xq->first_unsent = xq->tail = xq->part_xqe = xq->rexmitq = NULL;
                          
    xq->next_fragnum                = 0;
    xq->next_serial_num             = 0;
    xq->last_fack_serial            = -1;
    xq->cwindow_size                = 0;
    xq->window_size                 = RPC_C_DG_INITIAL_WINDOW_SIZE;
    xq->blast_size                  = 0;
    xq->freqs_out                   = 0;
    xq->push                        = false;
    xq->awaiting_ack                = false;
    xq->rexmit_timeout              = RPC_C_DG_INITIAL_REXMIT_TIMEOUT;
    /*
     * Temporarily, we turn off the first fack wait logic for recovering
     * the small in/out performance. We'll revisit it later.
     */
    xq->first_fack_seen             = true;
}


/*
 * R P C _ _ D G _ X M I T Q _ F R E E
 *
 * Frees data referenced by a transmit queue (rpc_dg_xmit_q_t).  The
 * transmit queue itself is NOT freed, since it's (assumed to be) part
 * of a larger structure.  Clearly this means any sort of xmitq related
 * ack must have arrived.
 */

PRIVATE void rpc__dg_xmitq_free
#ifdef _DCE_PROTO_
(
    rpc_dg_xmitq_p_t xq,
    rpc_dg_call_p_t call
)
#else
(xq, call)
rpc_dg_xmitq_p_t xq;
rpc_dg_call_p_t call;
#endif
{
    /*
     * presumably the call is either locked or 'private' at this point
     * RPC_DG_CALL_LOCK_ASSERT(call);
     */

    RPC_DG_XMITQ_AWAITING_ACK_CLR(xq);

    while (xq->head != NULL) 
    {
        rpc_dg_xmitq_elt_p_t xqe = xq->head;

        xq->head = xqe->next;
        rpc__dg_pkt_free_xqe(xqe, call);
    }
    xq->first_unsent = xq->tail = xq->rexmitq = NULL;

    /*
     * Clear any previously set blast_size.
     */

    xq->blast_size = 0;
}


/*
 * R P C _ _ D G _ X M I T Q _ A P P E N D _ P P
 *
 * Append a transmit queue's partial packet to the transmit queue itself.
 */

PRIVATE void rpc__dg_xmitq_append_pp
#ifdef _DCE_PROTO_
(
        rpc_dg_call_p_t call,
        unsigned32 *st
)
#else
(call, st)
    rpc_dg_call_p_t call;
    unsigned32 *st;
#endif
{
    rpc_dg_xmitq_p_t xq = &call->xq;
    rpc_dg_xmitq_elt_p_t xqe = xq->part_xqe;
    rpc_key_info_p_t key_info = call->key_info;
    int ptype;
    unsigned32 frag_length = 0; /* # bytes in the body of the fragment */
    rpc_dg_xmitq_elt_p_t last_xqe = xqe;
    unsigned32 inc_fragnum;
    
    *st = rpc_s_ok;

    RPC_DG_CALL_LOCK_ASSERT(call);

    if (xqe == NULL)
        return;

    /*
     * Compute the fragment length and store it.
     */

    frag_length = last_xqe->body_len;

    while (last_xqe->more_data != NULL)
    {
        frag_length += last_xqe->more_data->body_len;
        last_xqe = last_xqe->more_data;
    }            

    xqe->frag_len = frag_length;

    xqe->next       = NULL;
    xqe->fragnum    = xq->next_fragnum++;
    xqe->flags      = 0;

    /*
     * Add the partial xqe to the queue.
     */

    if (xq->head == NULL)
    {
        xq->head = xq->tail = xq->first_unsent = xqe;
    }
    else 
    {
        xq->tail->next = xqe;
        xq->tail = xqe; 
        if (xq->first_unsent == NULL)
            xq->first_unsent = xqe;
    }

    /*
     * "Normal" idempotent operations with *large ins* get tagged as
     * non-idempotent calls.  Old (1.5.1) servers (and 2.0 as well) can
     * dispose of their response packet in the case of idempotent calls
     * with "small outs".  V2.0 clients dispose of acknowledged input
     * stream frags hence, they are unable to rerun an idempotent call 
     * with large ins in the event that the server response is lost.
     *
     * The condition below is triggered for fragnum 0 only if we aren't
     * 'pushing' the queue; ie. there are more packets to follow so we
     * can assume the call has large INS.
     */

    if (xqe->fragnum == 0 && ! xq->push) 
    {
        if (RPC_DG_HDR_INQ_PTYPE(&xq->hdr) == RPC_C_DG_PT_REQUEST 
            && ! RPC_DG_FLAG_IS_SET(xq->base_flags, RPC_C_DG_PF_MAYBE))
        {
            xq->base_flags &= ~RPC_C_DG_PF_IDEMPOTENT;
        }
    }

    /*
     * Set the "frag" bit appropriately.  The only time we don't set
     * this bit is when the first packet is being pushed from xmitq_push.
     * If a packet is appended at any time before we begin "pushing,"
     * we can be sure that the xmit is fragmented.  Once the flag is
     * set, it never gets unset, so no need to worry that the last frag
     * will reset the flag.            
     */

    if (! xq->push)
    {
        xq->base_flags |= RPC_C_DG_PF_FRAG;
    }

    xq->part_xqe = NULL;
    
    /*
     * Only encrypt data packets -- setting key_info to NULL
     * disables encrypt machinery for this packet.
     */

    ptype = RPC_DG_HDR_INQ_PTYPE(&xq->hdr);
    if (!RPC_DG_PT_IS_DATA(ptype))
        key_info = NULL;

    if (key_info != NULL)
    {
        rpc_dg_auth_epv_p_t auth_epv = call->auth_epv;
        unsigned32 blocksize = auth_epv->blocksize;
        
        /*
         * If the packet length isn't a multiple of the encryption block
         * size, round it up now.
         */
        frag_length = (((frag_length) + blocksize - 1)
                       / blocksize) * blocksize;
        last_xqe->body_len += (frag_length - xqe->frag_len);
        xqe->frag_len = frag_length;

        assert(RPC_C_DG_RAW_PKT_HDR_SIZE + frag_length + auth_epv->overhead <= xq->snd_frag_size);

        if (last_xqe->body_len + auth_epv->overhead
            > RPC_C_DG_MAX_PKT_BODY_SIZE)
        {
            /*
             * This can happen if the fragment gets pushed.
             */
            last_xqe->more_data = rpc__dg_pkt_alloc_xqe(call, st);
            if (*st != rpc_s_ok)
                return;
        }
        (*auth_epv->encrypt) (key_info, xqe, st);
        if (*st != rpc_s_ok)
            return;
    }
}


/*
 * R P C _ _ D G _ X M I T Q _ R E S T A R T 
 *
 * Timeout everything on the transmit queue.  This routine is used when
 * we infer that the receiver has lost some of the data previously sent
 * it.  Situations in which we are forced to *infer* data loss include
 * receiving a no-call in response to a ping, and receiving a ping while
 * we are in the xmit of final states.  In such situations we recover
 * by beginning to send the xmitq again, and hoping the flow control
 * login will kick in.
 */

PRIVATE void rpc__dg_xmitq_restart
#ifdef _DCE_PROTO_
(
    rpc_dg_call_p_t call
)
#else
(call)
rpc_dg_call_p_t call;
#endif
{  
    rpc_dg_xmitq_p_t xq = &call->xq;
    rpc_dg_xmitq_elt_p_t xqe, tail;
    unsigned32 rexmit_cnt = 0;
               
    /*
     * If the xmitq has already been set up to do a transmission, leave
     * it alone.   Since 'restarting' the queue is a meat-axe approach 
     * to error recovery, we'll defer to any 'normal' processing that
     * might also be trying to handle the xmitq.
     */

    if (RPC_DG_CALL_READY_TO_SEND(call))
    {
        RPC_DG_START_XMIT(call);     
        return;   
    }

    for (xqe = xq->head; xqe != NULL && xqe != xq->first_unsent; 
         xqe = xqe->next)
    {
        rexmit_cnt++;  
        
        /*
         * If the packets is counted in the current congestion window,
         * remove it.  Also check to see if the packet counted as one
         * of our outstanding fack requests.
         */

        if (xqe->in_cwindow)
        {
            xqe->in_cwindow = false;
            xq->cwindow_size--;
            if (! RPC_DG_FLAG_IS_SET(xqe->flags, RPC_C_DG_PF_NO_FACK) ||
                RPC_DG_FLAG_IS_SET(xqe->flags, RPC_C_DG_PF_LAST_FRAG))
            {
                xq->freqs_out--;
            }
        }
        if (rexmit_cnt == 1)
            xq->rexmitq = xqe;
        else
            tail->next_rexmit = xqe;
                             
        xqe->next_rexmit = NULL;
        tail = xqe;             
    }
     
    /*
     * If we didn't find any packets to retransmit, then let's send
     * the first unsent packet.
     */

    if (rexmit_cnt == 0 && xq->first_unsent != NULL)
    { 
        rexmit_cnt = 1;
    }                  

    xq->blast_size = MIN(rexmit_cnt, RPC_C_DG_INITIAL_BLAST_SIZE);

    if (RPC_DG_CALL_READY_TO_SEND(call))
        RPC_DG_START_XMIT(call);     
}                        


