/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgsoc.c,v 65.8 1998/04/01 14:17:02 gwehrman Exp $";
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
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: dgsoc.c,v $
 * Revision 65.8  1998/04/01 14:17:02  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed the definition of st in sock_free() from unsigned to
 * 	unsigned32 to match its use.
 *
 * Revision 65.7  1998/03/23  17:28:20  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/03/21 19:15:07  lmc
 * Changed the use of "#ifdef DEBUG" to "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)" to turn on debugging in rpc.
 * This allows debug rpc messages to be turned on without
 * getting the kernel debugging that comes with "DEBUG".
 * "DEBUG" can change the size of some kernel data structures.
 *
 * Revision 65.4  1998/01/07  17:21:07  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:07  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:59  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.928.1  1996/05/10  13:11:55  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:14 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:42 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	merge local endpoint fix
 * 	[1995/10/03  22:20 UTC  lmm  /main/HPDCE02/14]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:02 UTC  psn
 * 	Wrap rpc__dg_ena_priv_sockets() call with _KERNEL.
 * 	[1995/11/17  15:32 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/2]
 *
 * 	Merge in from mothra.
 * 	[1995/11/16  21:33 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	merge local endpoint fix
 * 	[1995/10/03  22:20 UTC  lmm  /main/HPDCE02/14]
 *
 * 	don't create local rpc endpoint when network interface is disabled
 * 	[1995/10/03  22:14 UTC  lmm  /main/HPDCE02/lmm_rpc_loopback_fix/1]
 *
 * 	Yet another submit for CHFts16024.
 * 	[1995/09/12  16:16 UTC  tatsu_s  /main/HPDCE02/13]
 *
 * 	Mark the socket disconnected regardless of the outcome of disconnet.
 * 	[1995/09/11  16:15 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/3]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:18 UTC  tatsu_s  /main/HPDCE02/12]
 *
 * 	Added Transarc's rpc__dg_ena/disa_priv_sockets().
 * 	[1995/08/18  15:02 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/2]
 *
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 *
 * 	Submitted the fix for CHFts15498.
 * 	[1995/06/15  20:14 UTC  tatsu_s  /main/HPDCE02/11]
 *
 * 	Accept ENOENT as a legal errno for the disconnet.
 * 	[1995/06/14  15:09 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b3/3]
 *
 * 	Unbound the sockets if necessary.
 * 	[1995/06/13  19:00 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b3/2]
 *
 * 	Reset is_bound to false if necessary in sock_lookup().
 * 	Fixed the private local socket caching.
 * 	[1995/06/12  19:06 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b3/1]
 *
 * 	Submitted the fix for CHFts15303.
 * 	[1995/05/23  20:34 UTC  tatsu_s  /main/HPDCE02/10]
 *
 * 	Fixed double-free'ing a private rqe.
 * 	[1995/05/18  16:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b2/1]
 *
 * 	Submitted the fix for CHFts14944.
 * 	[1995/05/05  20:50 UTC  tatsu_s  /main/HPDCE02/9]
 *
 * 	Reduced the number of cached DG sockets.
 * 	[1995/05/03  18:37 UTC  tatsu_s  /main/HPDCE02/tatsu_s.scale_fix.b0/2]
 *
 * 	Fixed max_call_requests to work in rpc_server_use_*protseq*().
 * 	[1995/04/21  20:37 UTC  tatsu_s  /main/HPDCE02/tatsu_s.scale_fix.b0/1]
 *
 * 	fix typo
 * 	[1995/04/03  13:20 UTC  mothra  /main/HPDCE02/8]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:15 UTC  lmm  /main/HPDCE02/7]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:19 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:20 UTC  tatsu_s  /main/HPDCE02/6]
 *
 * 	Fixed the local auth pre_call.
 * 	[1995/03/01  19:32 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/1]
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
 * 	Fixed KRPC.
 * 	[1995/01/31  14:49 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/3]
 *
 * 	Fixed the SLC's reference count.
 * 	[1995/01/26  21:58 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/2]
 *
 * 	Merging in fix to disable private client sockets in KRPC
 * 	[1995/01/23  19:52 UTC  markar  /main/HPDCE02/3]
 *
 * 	Disabling private client sockets in KRPC
 * 	[1995/01/23  19:46 UTC  markar  /main/HPDCE02/markar_kpriv_sock_fix/1]
 *
 * 	Added rpc__dg_network_sock_ref_slc() and rpc__dg_network_sock_release_slc().
 * 	[1995/01/25  20:35 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Merging Local RPC mods
 * 	[1995/01/16  19:16 UTC  markar  /main/HPDCE02/2]
 *
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/01/16  14:20 UTC  markar  /main/HPDCE02/markar_local_rpc/3]
 *
 * 	Local rpc mods: check point.
 * 	[1994/12/21  14:47 UTC  tatsu_s  /main/HPDCE02/markar_local_rpc/2]
 *
 * 	Local rpc mods: check point.
 * 	[1994/12/19  22:04 UTC  tatsu_s  /main/HPDCE02/markar_local_rpc/1]
 *
 * 	Submitted rfc31.0: Sing
 *
 * Revision 1.1.924.3  1994/05/27  15:36:37  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:56:49  tatsu_s]
 * 
 * 	Fixed double-free'ing in sock_free().
 * 	[1994/05/05  18:07:12  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:19:08  tatsu_s]
 * 
 * Revision 1.1.924.2  1994/03/15  20:32:51  markar
 * 	     Initialize rqe.sock_ref and rqe_available
 * 	[1994/03/15  17:40:42  markar]
 * 
 * 	     private client socket mods
 * 	[1994/02/22  18:28:47  markar]
 * 
 * Revision 1.1.924.1  1994/01/21  22:37:27  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:40  cbrooks]
 * 
 * Revision 1.1.8.1  1993/09/28  21:58:15  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:39  weisman]
 * 
 * Revision 1.1.5.4  1993/01/03  23:25:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:30  bbelch]
 * 
 * Revision 1.1.5.3  1992/12/23  20:49:50  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:15  zeliff]
 * 
 * Revision 1.1.5.2  1992/09/29  20:41:47  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:45:41  weisman]
 * 
 * Revision 1.1.2.2  1992/05/01  17:21:26  rsalz
 * 	 09-apr-92 nm    Fix error path in use_protseq.
 * 	[1992/05/01  17:16:58  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:28  devrcs
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
**      dgsoc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG socket manipulation routines.
**
**
*/

#include <dg.h>
#include <dgsoc.h>
#include <dgpkt.h>

/* ========================================================================= */
/*
 * The private socket may be disabled by setting the environment variable
 * "RPC_DISABLE_PRIVATE".
 */

INTERNAL volatile unsigned32 max_priv_socks = RPC_C_DG_SOCK_MAX_PRIV_SOCKS;


#ifndef _KERNEL
/*
 * From Transarc. Hidden APIs for disabling private datagram sockets.
 *
 * These APIs rpc__dg_disa/ena_priv_sockets() allow
 * applications to disable/enable private datagram
 * sockets. This is needed on the client side when using
 * RPCs with pipe parameters if the pull or push routine
 * might block for any significant amount of time.  In
 * the private socket case this has the effect of
 * stalling the DG protocol.  With shared sockets, the
 * listener thread keepsthe machinery running while the
 * push/pull is blocked.
 */

void rpc__dg_disa_priv_sockets(void)
{
    RPC_DG_SOCK_POOL_LOCK(0);
    max_priv_socks = 0;
    RPC_DG_SOCK_POOL_UNLOCK(0);
}

void rpc__dg_ena_priv_sockets(void)
{
    RPC_DG_SOCK_POOL_LOCK(0);
    max_priv_socks = RPC_C_DG_SOCK_MAX_PRIV_SOCKS;
    RPC_DG_SOCK_POOL_UNLOCK(0);
}
#endif	/* _KERNEL */

/* ========================================================================= */


INTERNAL void sock_free _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t * /*sp_elt*/
    ));

INTERNAL void use_protseq _DCE_PROTOTYPE_((
        boolean32  /*is_server*/,
        rpc_protseq_id_t  /*pseq_id*/,
        rpc_addr_p_t  /*rpc_addr*/,
        unsigned_char_p_t  /*endpoint*/,
        rpc_dg_sock_pool_elt_p_t * /*sock_pool_elt*/,
        unsigned32 * /*st*/
    )); 

/* ========================================================================= */


/*
 * S O C K _ F R E E
 *
 * This routine is called to *really* cleanup and free a pool elt.
 */

INTERNAL void sock_free
#ifdef _DCE_PROTO_
(
    rpc_dg_sock_pool_elt_p_t *sp
)
#else
(sp)
rpc_dg_sock_pool_elt_p_t *sp;
#endif
{
    rpc_socket_error_t serr;
    rpc_dg_sock_pool_elt_p_t eltp, *peltp;
    boolean32 is_private_socket = (*sp)->is_private;
#ifdef SGIMIPS
    unsigned32 st;
#endif /* SGIMIPS */


    RPC_DG_SOCK_POOL_LOCK_ASSERT(0);
           
    /*
     * Close the socket desc.
     */
    serr = rpc__socket_close((*sp)->sock);
    if (RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DBG_GPRINTF((
            "(sock_free) Error closing socket %d, error=%d\n",
            (*sp)->sock, RPC_SOCKET_ETOI(serr)));
    }

    /*
     * Decide whether this socket will be on the private or shared list,
     * and then remove it from the pool.
     */
    peltp = (is_private_socket) ? &rpc_g_dg_sock_pool.private_sockets :
                                  &rpc_g_dg_sock_pool.shared_sockets;

    if (*sp == *peltp)
    {
        *peltp = (*sp)->next;
    }
    else
    {
        for (eltp = *peltp; eltp != NULL; eltp = eltp->next)
        {
            if (eltp->next == *sp)
            {
                eltp->next = (*sp)->next;
                break;
            }
        }
    }

    /*
     * For private sockets, free the private packets associated with
     * the socket.
     */
    if (is_private_socket)
    {
        if ((*sp)->rqe_list_len > 0 && (*sp)->rqe_list != NULL)
            rpc__dg_pkt_free_rqe((*sp)->rqe_list, (rpc_dg_call_p_t)(*sp)->ccall);
        if ((*sp)->xqe != NULL)
             RPC_MEM_FREE((*sp)->xqe, RPC_C_MEM_DG_PKT_POOL_ELT);
        if ((*sp)->rqe != NULL)
            RPC_MEM_FREE((*sp)->rqe, RPC_C_MEM_DG_PKT_POOL_ELT);
    }

    rpc_g_dg_sock_pool.num_entries--;
#ifdef SGIMIPS
    if ( (*sp)->brd_addrs) {
        rpc__naf_addr_vector_free(&(*sp)->brd_addrs, &st);
    }
#endif /* SGIMIPS */

    
    /*
     * Free up the elt's memory.
     */
    RPC_MEM_FREE(*sp, RPC_C_MEM_DG_SOCK_POOL_ELT);

    *sp = NULL;
}

           
/*
 * U S E _ P R O T S E Q
 *
 * Common, internal socket pool allocation routine.  Find/create a suitable
 * socket pool entry and add it to the listener's database.
 */

INTERNAL void use_protseq
#ifdef _DCE_PROTO_
(
    boolean32 is_server,
    rpc_protseq_id_t pseq_id,
    rpc_addr_p_t rpc_addr,
    unsigned_char_p_t endpoint,
    rpc_dg_sock_pool_elt_p_t *sock_pool_elt,
    unsigned32 *st
)
#else
(is_server, pseq_id, rpc_addr, endpoint, sock_pool_elt, st)
boolean32 is_server;
rpc_protseq_id_t pseq_id;
rpc_addr_p_t rpc_addr;
unsigned_char_p_t endpoint;
rpc_dg_sock_pool_elt_p_t *sock_pool_elt;
unsigned32 *st;
#endif
{
    boolean32 sock_open = false;
    boolean32 creating_private_socket = false;
    rpc_socket_error_t serr;
    rpc_socket_t socket_desc;
    rpc_dg_sock_pool_elt_p_t eltp;
    unsigned32 sndbuf, rcvbuf;
    unsigned32 priv_sock_count = 0;
    unsigned32 desired_sndbuf, desired_rcvbuf;
    
    *st = rpc_s_ok;

    RPC_DG_SOCK_POOL_LOCK(0);
    
    /*
     * For clients, first see if there's already an open socket of the
     * correct type and if so, return a ref to it. Note: servers always
     * create new entries.
     */
                                    
    if (! is_server)
    {
        unsigned32 max_priv_socks = RPC_C_DG_SOCK_MAX_PRIV_SOCKS;
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
        if (RPC_DBG (rpc_es_dbg_dg_max_psock, 1))
        {
            max_priv_socks =
                (unsigned32)(rpc_g_dbg_switches[(int)rpc_es_dbg_dg_max_psock])
                    - 1;
        }
#endif
        /*
         * First, look in the list of private sockets.  If we find one
         * that is only referenced by the socket pool, use it.  While
         * looking, keep a count of how many private sockets exist for
         * this protseq.  Since there's a limit on the number of private
         * sockets we'll create, we'll use this count later (if we have
         * to create a socket pool entry) to decide whether to create
         * a private or shared socket.
         */

        for (eltp = rpc_g_dg_sock_pool.private_sockets; eltp != NULL;
             eltp = eltp->next)
        {
            if (eltp->pseq_id == pseq_id)
            {
                priv_sock_count++;

                if (! eltp->is_disabled && eltp->refcnt == 1)
                {
                    eltp->refcnt++;     /* for the ref we're returning */
                    *sock_pool_elt = eltp;
                    RPC_DG_SOCK_POOL_UNLOCK(0);

                    RPC_DBG_PRINTF(rpc_e_dbg_dg_sockets, 3, (
                            "(use_protseq) using private socket\n"));

                    return;
                }
            }
        }

        /*
         * Didn't find a private socket.  If we haven't already created
         * the maximum number of private sockets for this protseq, then
         * we'll create another one.
         */

        if (priv_sock_count < max_priv_socks)
        {
            creating_private_socket = true;
        }
        else
        {
            /*
             * Else, we've maxed out the private sockets.  See if we
             * have a shared socket for this protseq.  If so, return
             * it.  If not, drop through and create a shared socket.
             */
            for (eltp = rpc_g_dg_sock_pool.shared_sockets; eltp != NULL;
                 eltp = eltp->next)
            {
                if (! eltp->is_server && eltp->pseq_id == pseq_id &&
                    ! eltp->is_disabled)
                {
                    eltp->refcnt++;     /* for the ref we're returning */
                    *sock_pool_elt = eltp;
                    RPC_DG_SOCK_POOL_UNLOCK(0);

                    RPC_DBG_PRINTF(rpc_e_dbg_dg_sockets, 3, (
                            "(use_protseq) using shared socket\n"));

                    return;
                }
            }
        }
    }  
    

    RPC_DBG_PRINTF(rpc_e_dbg_dg_sockets, 3, (
            "(use_protseq) allocating a %s socket\n",
            creating_private_socket ? "private" : "shared"));

    /* 
     * Allocate a new pool entry and initialize it.
     */ 
    RPC_MEM_ALLOC(eltp, rpc_dg_sock_pool_elt_p_t, 
        sizeof(rpc_dg_sock_pool_elt_t), RPC_C_MEM_DG_SOCK_POOL_ELT, 
        RPC_C_MEM_NOWAIT);
    if (eltp == NULL){
	*st = rpc_s_no_memory;
	goto CLEANUP;
    }
    
    /*
     * Create a network descriptor for this RPC Protocol Sequence.
     */
    serr = rpc__socket_open(pseq_id, &socket_desc);

    if (RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DBG_GPRINTF(("(use_protseq) Can't create socket, error=%d\n",
            RPC_SOCKET_ETOI(serr)));
        *st = rpc_s_cant_create_sock;
        goto CLEANUP;
    }
    sock_open = true;
     
    /*
     * Bind the socket (Network descriptor) to the RPC address.
     */ 

    rpc_addr->rpc_protseq_id = pseq_id;

    serr = rpc__socket_bind(socket_desc, rpc_addr);

    if (RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DBG_GPRINTF(("(use_protseq) Can't bind socket, error=%d\n",
            RPC_SOCKET_ETOI(serr)));
        *st = rpc_s_cant_bind_sock;
        goto CLEANUP;
    }

    /*
     * Request a change in the amount of system buffering provided
     * to the socket.
     */
    if (creating_private_socket)
    {
        desired_sndbuf = RPC_C_DG_MAX_WINDOW_SIZE_BYTES;
        desired_rcvbuf = RPC_C_DG_MAX_WINDOW_SIZE_BYTES;
    }
    else
    {
        desired_sndbuf = RPC_C_DG_SOCK_DESIRED_SNDBUF;
        desired_rcvbuf = RPC_C_DG_SOCK_DESIRED_RCVBUF;
    }

    serr = rpc__socket_set_bufs(socket_desc,
                                desired_sndbuf, desired_rcvbuf,
                                &sndbuf, &rcvbuf);
            
    if (RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DBG_GPRINTF((
            "(use_protseq) WARNING: Can't set socket bufs, error=%d\n",
            RPC_SOCKET_ETOI(serr)));
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
        "(use_protseq) desired_sndbuf %u, desired_rcvbuf %u\n",
        desired_sndbuf, desired_rcvbuf));
    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, (
        "(use_protseq) actual sndbuf %lu, actual rcvbuf %lu\n",
        sndbuf, rcvbuf));
      
    /*
     * For shared sockets, set the socket to do non-blocking IO.
     */

    if (! creating_private_socket)
    {
        rpc__socket_set_nbio(socket_desc);
    }

    /*
     * Set the socket to close itself on exec.
     */
    rpc__socket_set_close_on_exec(socket_desc);

    /*
     * For private sockets, allocate an xqe/rqe pair.  These packets
     * allow calls with small ins/outs to avoid going through the packet
     * pool reservation processing.
     */

    if (creating_private_socket)
    {
        rpc_dg_pkt_pool_elt_p_t   pkt;

        RPC_MEM_ALLOC(pkt, rpc_dg_pkt_pool_elt_p_t,
            sizeof(rpc_dg_pkt_pool_elt_t), RPC_C_MEM_DG_PKT_POOL_ELT,
            RPC_C_MEM_NOWAIT);
	if (pkt == NULL){
	    *st = rpc_s_no_memory;
	    goto CLEANUP;
	}

        eltp->xqe             = &pkt->u.xqe.xqe;
#ifndef INLINE_PACKETS
        eltp->xqe->body       = &pkt->u.xqe.pkt;
#endif /* INLINE_PACKETS */
        eltp->xqe->next       = NULL;
        eltp->xqe->more_data  = NULL;
        eltp->xqe->frag_len   = 0;
        eltp->xqe->flags      = 0;
        eltp->xqe->body_len   = 0;
        eltp->xqe->serial_num = 0;
        eltp->xqe->in_cwindow = false;

        RPC_MEM_ALLOC(pkt, rpc_dg_pkt_pool_elt_p_t,
            sizeof(rpc_dg_pkt_pool_elt_t), RPC_C_MEM_DG_PKT_POOL_ELT,
            RPC_C_MEM_NOWAIT);
	if (pkt == NULL){
	    *st = rpc_s_no_memory;
	    goto CLEANUP;
	}

        eltp->rqe = &pkt->u.rqe.rqe;
#ifndef INLINE_PACKETS
        eltp->rqe->pkt_real = &pkt->u.rqe.pkt;
        eltp->rqe->pkt = (rpc_dg_raw_pkt_p_t) RPC_DG_ALIGN_8(eltp->rqe->pkt_real);
#endif /* INLINE_PACKETS */
        eltp->rqe->next   = NULL;
        eltp->rqe->more_data  = NULL;
        eltp->rqe->frag_len   = 0;
        eltp->rqe->hdrp   = NULL;
 
        pkt->u.rqe.sock_ref = eltp;
        eltp->rqe_available = true;
    }
    else
    {
        eltp->xqe = NULL;
        eltp->rqe = NULL;
        eltp->rqe_available = false;
    }

    /*
     * Initialize all fields in the socket pool entry.
     */
    eltp->sock      = socket_desc;
    eltp->rcvbuf    = rcvbuf;
    eltp->sndbuf    = sndbuf;
    eltp->brd_addrs = NULL;
    eltp->ccall     = NULL;
    eltp->rqe_list     = NULL;
    eltp->rqe_list_len = 0;
    eltp->error_cnt = 0;
    eltp->pseq_id   = pseq_id;
    eltp->refcnt    = 2;    /* 1 for table ref + 1 for returned ref */
    eltp->is_server = is_server;
    eltp->is_disabled  = 0;
    eltp->is_private   = creating_private_socket;

    /*
     * Add the new elt to the appropriate list within the socket pool
     * before we unlock it (so others will see it);  note we still have
     * our ref to it.  After this point, call sock_free() to clean things
     * up.
     */
    if (creating_private_socket)
    {
        eltp->next = rpc_g_dg_sock_pool.private_sockets;
        rpc_g_dg_sock_pool.private_sockets = eltp;
    }
    else
    {
        eltp->next = rpc_g_dg_sock_pool.shared_sockets;
        rpc_g_dg_sock_pool.shared_sockets = eltp;
    }

    rpc_g_dg_sock_pool.num_entries++;

    /*
     * Probably best to unlock the pool before calling into the
     * listener abstraction.
     */
    RPC_DG_SOCK_POOL_UNLOCK(0);

    /*
     * For shared sockets, tell the network listener to start listening
     * on this socket (add a reference because we're giving one to it).
     * If this fails, just blow away the new elt.  Note that private
     * sockets do not require the presence of the listener thread.
     */
    if (! creating_private_socket)
    {
        rpc__dg_network_sock_reference(eltp);
        rpc__network_add_desc(socket_desc, is_server, (endpoint == NULL),
                                pseq_id, (pointer_t) eltp, st);

        if (*st != rpc_s_ok)
        {
            RPC_DG_SOCK_POOL_LOCK(0);
            sock_free(&eltp);
            RPC_DG_SOCK_POOL_UNLOCK(0);
        }
    }

    *sock_pool_elt = eltp;
    return;

CLEANUP:

    /*
     * A failure happened; clean things up, returning the previously
     * setup status.
     */
    if (sock_open)
        serr = rpc__socket_close(socket_desc);

    RPC_MEM_FREE(eltp, RPC_C_MEM_DG_SOCK_POOL_ELT);
    RPC_DG_SOCK_POOL_UNLOCK(0);
    return;
}

         
/*
 * R P C _ _ D G _ N E T W O R K _ U S E _ P R O T S E Q _ S V
 *
 * Server side entry point into socket pool allocation routine. 
 */

PRIVATE void rpc__dg_network_use_protseq_sv
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t pseq_id,
    unsigned32 max_calls,
    rpc_addr_p_t rpc_addr,
    unsigned_char_p_t endpoint,
    unsigned32 *st
)
#else
(pseq_id, max_calls, rpc_addr, endpoint, st)
rpc_protseq_id_t pseq_id;
unsigned32 max_calls;
rpc_addr_p_t rpc_addr;
unsigned_char_p_t endpoint;
unsigned32 *st;
#endif
{   
    rpc_dg_sock_pool_elt_p_t sp_elt;
    unsigned32 num_socks;
    unsigned32 opened = 0;
    unsigned32 i;

    if (max_calls == rpc_c_protseq_max_reqs_default)
	max_calls = 1;
    else if (endpoint != NULL)
    {
	/*
	 * when using a well-known endpoint, we can open only one socket.
	 */
        if (max_calls > rpc_c_listen_max_calls_default)
        {
	    *st = rpc_s_calls_too_large_for_wk_ep; 
	    return;
	}
    }

    /*
     * Create as many sockets as are required to fufill the
     * server's request to be able to handle "max_calls"
     * concurrent call requests. For DG, this means having
     * enough socket receive buffering. Since we guess our
     * socket's expected load to be RPC_C_DG_SOCK_LOAD, the
     * most conservative thing to do would be:
     *
     * num_socks = (max_calls + RPC_C_DG_SOCK_LOAD - 1)/RPC_C_DG_SOCK_LOAD;
     *
     * However, we don't really trust our load measure well
     * enough (or have no better alternative). Also, it may
     * break an existing application which is using
     * something other than rpc_c_protseq_max_reqs_default
     * or mistakenly using rpc_c_listen_max_calls_default.
     *
     * Here is a clumsy solution.
     * RPC_C_DG_SOCK_REQS_LOAD (> RPC_C_DG_SOCK_LOAD) is yet another
     * guess'timate.
     */
    if (max_calls <= rpc_c_listen_max_calls_default)
	num_socks = 1;
    else
    {
	max_calls -= rpc_c_listen_max_calls_default;
	num_socks =
	    1 + (max_calls +
		 (RPC_C_DG_SOCK_REQS_LOAD) - 1)/(RPC_C_DG_SOCK_REQS_LOAD);
    }

    /*
     * Since we don't really trust our load measure well
     * enough, if we have at least one socket open we are
     * successful.
     */
    assert(num_socks > 0);
    for (i = 0; i < num_socks; i++)
    {
	use_protseq(true, pseq_id, rpc_addr, endpoint, &sp_elt, st);
	if (*st == rpc_s_ok)
	{
	    rpc__dg_network_sock_release(&sp_elt);
	    opened++;
	}
    }
    if (opened)
	*st = rpc_s_ok;
}


/*
 * R P C _ _ D G _ N E T W O R K _ U S E _ P R O T S E Q _ C L
 *
 * Client side entry point into socket pool allocation routine. 
 * Create a NULL address for the bind, and call the internal
 * use_protseq routine.
 */

PRIVATE void rpc__dg_network_use_protseq_cl
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t pseq_id,
    rpc_dg_sock_pool_elt_p_t *sp
)
#else
(pseq_id, sp)
rpc_protseq_id_t pseq_id;
rpc_dg_sock_pool_elt_p_t *sp;
#endif
{                          
    rpc_addr_il_t addr;
    unsigned32 st;
        
    *sp = NULL;

    /*
     * Specify a null address to bind to.  We do this so
     * the socket gets an endpoint assigned right away.
     */
    addr.len = sizeof addr.sa;
    memset((char *) &addr.sa, 0, addr.len);
    addr.sa.family = rpc_g_protseq_id[pseq_id].naf_id;
        
    use_protseq(false, pseq_id, (rpc_addr_p_t) &addr, NULL, sp, &st);
}


/* 
 * R P C _ _ D G _ N E T W O R K _ D I S A B L E _ D E S C
 *
 * This routine is responsible for removing a socket from the pool of
 * sockets being monitored by the listener thread.  It is called from
 * the macro which updates the error count associated with a socket pool
 * entry, when that count reaches the maximum allowed.
 * Note: the caller's socket pool reference remains intact.
 */

PRIVATE void rpc__dg_network_disable_desc
#ifdef _DCE_PROTO_
(
    rpc_dg_sock_pool_elt_p_t sp
)
#else
(sp)
rpc_dg_sock_pool_elt_p_t sp;
#endif
{                                               
    unsigned32 st;
    boolean is_private;

    RPC_DG_SOCK_POOL_LOCK(0); 

    is_private = sp->is_private;

    /*
     * Make certain it's not already disabled.
     */
    if (sp->is_disabled)
    {
        RPC_DG_SOCK_POOL_UNLOCK(0); 
        return;
    }
       
    RPC_DBG_GPRINTF(("(rpc__dg_network_disable_desc) Disabing socket, error=%d\n",
        sp->sock));

    sp->is_disabled = true;

    RPC_DG_SOCK_POOL_UNLOCK(0); 

    /*
     * For shared sockets, remove the sock from the listener's database
     * and release the reference that we previously gave it.  Note:
     * remove_desc() doesn't return until the listener has completely
     * ceased using the socket/reference (which is why we can safely
     * release its reference).
     */
    if (is_private == false)
    {
        rpc__network_remove_desc(sp->sock, &st);
        rpc__dg_network_sock_release(&sp);
    }
}


/*
 * R P C _ _ D G _ N E T W O R K _ I N I T
 *
 * This routine is called as part of the rpc__init startup 
 * initializations.  It creates the mutex that protects the
 * socket pool.
 */
PRIVATE void rpc__dg_network_init(void)
{
    RPC_MUTEX_INIT(rpc_g_dg_sock_pool.sp_mutex);
}


/*
 * R P C _ _ D G _ N E T W O R K _ F O R K _ H A N D L E R
 * 
 * This routine handles socket pool related fork processing.
 */

PRIVATE void rpc__dg_network_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{ 
    unsigned32 i;
    rpc_dg_sock_pool_elt_p_t eltp, neltp;
#ifdef SGIMIPS
    /* REFERENCED */
#endif
    rpc_socket_error_t serr;

    switch ((int)stage)
    {
        case RPC_C_PREFORK:
            break;
        case RPC_C_POSTFORK_PARENT:
            break;
        case RPC_C_POSTFORK_CHILD:  
            /*
             * In the child of a fork, scan the socket pool
             * for in_use socket descriptors.
             *
             * Note that some of the code below would normally
             * require the socket pool lock.  However, after
             * the fork we are no longer running in a threaded
             * environment.
             */
            for (eltp = rpc_g_dg_sock_pool.private_sockets; eltp != NULL;
                 eltp = neltp)
            {
                serr = rpc__socket_close(eltp->sock);
                neltp = eltp->next;
                RPC_MEM_FREE(eltp, RPC_C_MEM_DG_SOCK_POOL_ELT);
            }
            rpc_g_dg_sock_pool.private_sockets = NULL;

            for (eltp = rpc_g_dg_sock_pool.shared_sockets; eltp != NULL;
                 eltp = neltp)
            {
                serr = rpc__socket_close(eltp->sock);
                neltp = eltp->next;
                RPC_MEM_FREE(eltp, RPC_C_MEM_DG_SOCK_POOL_ELT);
            }
            rpc_g_dg_sock_pool.shared_sockets = NULL;
            rpc_g_dg_sock_pool.num_entries = 0;
            break;
    }
}


/*
 * R P C _ _ D G _ N E T W O R K _ S O C K _ R E F E R E N C E
 *
 * Increment the reference count associated with an entry in the socket pool.
 */
#ifdef SGIMIPS
PRIVATE void rpc__dg_network_sock_reference(
rpc_dg_sock_pool_elt_p_t sp)  
#else
PRIVATE void rpc__dg_network_sock_reference(sp)
rpc_dg_sock_pool_elt_p_t sp;  
#endif
{  
    RPC_DG_SOCK_POOL_LOCK(0); 
    sp->refcnt++; 
    RPC_DG_SOCK_POOL_UNLOCK(0); 
}


/*
 * R P C _ _ D G _ N E T W O R K _ S O C K _ R E L E A S E
 *
 * Decrement the reference count associated with an entry in the socket pool.
 * If the count falls to 1, only the internal pool's ref remains; if the socket
 * has been marked disabled, free the entry.
 */

PRIVATE void rpc__dg_network_sock_release
#ifdef _DCE_PROTO_
(
    rpc_dg_sock_pool_elt_p_t *sp
)
#else
(sp)
rpc_dg_sock_pool_elt_p_t *sp;  
#endif
{  
    RPC_DG_SOCK_POOL_LOCK(0); 

    if (--((*sp)->refcnt) == 1)
    {
	if ((*sp)->is_disabled)
	    sock_free(sp); 
	else
	    (*sp)->ccall = NULL;
    }

    RPC_DG_SOCK_POOL_UNLOCK(0); 
    *sp = NULL;
}

/*
 * R P C _ _ D G _ N E T W O R K _ S O C K _ R E F _ S L C
 *
 * Associate the "shared local client" socket with an entry in the socket pool
 * and increment the reference count of the SLC socket.
 */
PRIVATE void rpc__dg_network_sock_ref_slc
#ifdef _DCE_PROTO_
(
    rpc_dg_sock_pool_elt_p_t *sp
)
#else
(sp)
rpc_dg_sock_pool_elt_p_t *sp;  
#endif
{  
    unsigned32 st;

}

/*
 * R P C _ _ D G _ N E T W O R K _ S O C K _ R E L E A S E _S L C
 *
 * Decrement the reference count of the "shared local client" socket
 * associated with an entry in the socket pool.
 */

PRIVATE void rpc__dg_network_sock_release_slc
#ifdef _DCE_PROTO_
(
    rpc_dg_sock_pool_elt_p_t sp
)
#else
(sp)
rpc_dg_sock_pool_elt_p_t sp;
#endif
{
}

