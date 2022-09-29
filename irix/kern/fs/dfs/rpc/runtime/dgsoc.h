/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgsoc.h,v $
 * Revision 65.1  1997/10/20 19:16:59  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.329.2  1996/06/04  21:54:33  arvind
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:27  marty]
 *
 * 	Submit OSF/DCE 1.2.1
 * 	HP 	[1995/11/16  21:33 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Remove Mothra specific code
 * 	HP 	[1995/08/29  19:18 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	HP 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 *
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	HP 	[1995/05/05  20:50 UTC  tatsu_s  /main/HPDCE02/3]
 *
 * 	Submitted the fix for CHFts14944.
 * 	HP 	[1995/05/03  18:38 UTC  tatsu_s  /main/HPDCE02/tatsu_s.scale_fix.b0/1]
 *
 * 	Reduced the number of cached DG sockets.
 * 	HP 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Submitted the local rpc cleanup.
 * 	HP 	[1995/01/25  20:35 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Added rpc__dg_network_sock_ref_slc() and rpc__dg_network_sock_release_slc().
 * 	HP 	[1995/01/16  19:16 UTC  markar  /main/HPDCE02/1]
 *
 * 	Merging Local RPC mods
 * 	HP 	[1995/01/05  20:37 UTC  markar  /main/markar_local_rpc/3]
 *
 * 	Move last_ccall from local_elt up to _elt
 * 	HP 	[1994/12/21  14:47 UTC  tatsu_s  /main/markar_local_rpc/2]
 *
 * 	Local rpc mods: check point.
 * 	HP 	[1994/12/19  22:04 UTC  tatsu_s  /main/markar_local_rpc/1]
 *
 * 	Local rpc mods: check point.
 * 	[1995/12/07  23:59:22  root]
 *
 * Revision 1.1.329.1  1996/05/10  13:12:08  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 * 
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:14 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:42 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 * 
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:18 UTC  tatsu_s  /main/HPDCE02/4]
 * 
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:02 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:33 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 * 
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:18 UTC  tatsu_s  /main/HPDCE02/4]
 * 
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 * 
 * 	Submitted the fix for CHFts14944.
 * 	[1995/05/05  20:50 UTC  tatsu_s  /main/HPDCE02/3]
 * 
 * 	Reduced the number of cached DG sockets.
 * 	[1995/05/03  18:38 UTC  tatsu_s  /main/HPDCE02/tatsu_s.scale_fix.b0/1]
 * 
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/2]
 * 
 * 	Added rpc__dg_network_sock_ref_slc() and rpc__dg_network_sock_release_slc().
 * 	[1995/01/25  20:35 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 * 
 * 	Merging Local RPC mods
 * 	[1995/01/16  19:16 UTC  markar  /main/HPDCE02/1]
 * 
 * 	Move last_ccall from local_elt up to _elt
 * 	[1995/01/05  20:37 UTC  markar  /main/markar_local_rpc/3]
 * 
 * 	Local rpc mods: check point.
 * 	[1994/12/21  14:47 UTC  tatsu_s  /main/markar_local_rpc/2]
 * 
 * 	Local rpc mods: check point.
 * 	[1994/12/19  22:04 UTC  tatsu_s  /main/markar_local_rpc/1]
 * 
 * Revision 1.1.325.3  1994/05/27  15:36:39  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added sndbuf, rqe_list and rqe_list_len.
 * 	[1994/04/29  19:19:31  tatsu_s]
 * 
 * Revision 1.1.325.2  1994/03/15  20:33:19  markar
 * 	     private client socket mods
 * 	[1994/03/01  20:42:24  markar]
 * 
 * 	     private client socket mods
 * 
 * Revision 1.1.325.1  1994/01/21  22:37:29  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:41  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:25:03  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:34  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:49:56  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:19  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:30  devrcs
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
**      dgsoc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for managing the socket pool.
**
**
*/

#ifndef _DGSOC_H
#define _DGSOC_H

/*
 * The Socket Pool
 *
 * Sockets are allocated by a call to rpc__dg_network_use_protseq_<cl,sv>.
 * For servers, a new socket is opened on each call.  Clients will reuse
 * an already open socket if it is of the correct protocol sequence,
 * and it was opened as a client socket.  Either way, we start by
 * finding/creating a suitable new/used entry in the socket pool.  
 *
 * Pool elements are reference counted.  When the only remaining reference
 * count is the pool's internal table, the pool element is freed (including
 * closing its associated socket descriptor). Thus, socket users holding
 * a reference are not forced to lock the socket pool before using a
 * descriptor to prevent the descriptor from disappearing.
 *
 * In the event of multiple consecutive errors, a socket can be set to
 * disabled.  Disabling removes the socket from the pool of descriptors
 * being listened on by the listener thread.  For socket users, the
 * disabled flag is advisory.  Since the socket user has a refcnt, it
 * needn't worry that the socket could be closed between the check of
 * the disabled flag and the actual use of the socket.
 *
 * Before performing a socket operation, threads should always check
 * that the socket they are using has not been marked disabled.  This
 * condition is advisory, but it does indicate that subsequent use of the
 * socket is pointless; use RPC_DG_NETWORK_IS_DISABLED_DESC().
 * 
 * Locking considerations:
 *
 * All fields within the socket pool and its entries are protected by
 * the the socket pool lock except:
 * 
 *     The error_cnt field can be updated by any thread with a reference
 *     to the pool entry.  We accept the possibility that an update may
 *     get lost.  However, it is presumed that while a socket is
 *     functioning correctly, all writers to this field will be writing
 *     0, and if the socket fails, all writers will be doing increments
 *     until the maximum number of consecutive errors has been reached.
 *     It is not critical that every increment be successful, whereas
 *     it is critical, for performance reasons, that we don't force socket
 *     users to take a lock to update this field every time they read/write
 *     from a socket.
 *
 *     Note that we force storage unit alignment of the "error_cnt".  This
 *     is done because we must worry about hardware environments in which
 *     the code that references less-than-memory-word units ends up
 *     dragging some larger unit from main memory, modifying some portion
 *     and storing the unit back.  This would end up reading and writing
 *     other pool_elt bits which are protected by the pool mutex (which
 *     we won't be holding).
 *
 *     The disabled flag for a pool entry can be read at anytime by threads
 *     which hold a reference to the entry.  The reference count guarantees
 *     the socket is still open, the disabled flag is merely an effort
 *     to inform users of the socket that they needn't bother trying
 *     to use the socket any further.  However, no harm will result if
 *     they miss the flag and try to use the the socket anyway.
 *
 *     Similarly, the is_private field can be read by any call with a
 *     reference to the entry.  This value never changes.
 *
 *     The xqe and rqe pointers are only used by private sockets.  They
 *     are initialized at the time the socket pool element is created.
 *     After that, they are protected by the call handle lock of the
 *     call that is using (holds the reference to) the private socket.
 */

typedef struct rpc_dg_sock_pool_elt_t {
    struct rpc_dg_sock_pool_elt_t *next;
    rpc_protseq_id_t pseq_id;
    rpc_socket_t sock;
    unsigned32 rcvbuf;
    unsigned32 sndbuf;
    rpc_addr_vector_p_t brd_addrs;
    rpc_dg_recvq_elt_p_t rqe;
    rpc_dg_xmitq_elt_p_t xqe;
    rpc_dg_recvq_elt_p_t rqe_list;  /* cached pkts */
    unsigned32 rqe_list_len;
    rpc_dg_ccall_p_t ccall;         /* only valid if is_private   */
    unsigned32 refcnt;                
    unsigned        : 0;            /* force alignment; see above */
    unsigned8 error_cnt;
    unsigned        : 0;            /* force alignment; see above */
    unsigned is_server: 1;
    unsigned is_disabled: 1;
    unsigned is_private: 1;
    unsigned rqe_available: 1;
} rpc_dg_sock_pool_elt_t, *rpc_dg_sock_pool_elt_p_t;


typedef struct rpc_dg_sock_pool_t { 
    unsigned32  num_entries;
    rpc_mutex_t sp_mutex;
    rpc_dg_sock_pool_elt_p_t private_sockets;
    rpc_dg_sock_pool_elt_p_t shared_sockets;
} rpc_dg_sock_pool_t;
                        
/*
 * Number of consecutive EIO errors we'll allow before disabling a socket.
 */   
#ifndef RPC_C_DG_SOCK_MAX_ERRORS
#define RPC_C_DG_SOCK_MAX_ERRORS 10
#endif

/*
 * Number of simultaneous requests we expect a single socket to support.
 */   
#ifndef RPC_C_DG_SOCK_REQS_LOAD
#define RPC_C_DG_SOCK_REQS_LOAD	5
#endif

/*
 * Number of private client sockets allowed for each address family.
 * Currently disabled for kernel RPC
 */
#ifndef RPC_C_DG_SOCK_MAX_PRIV_SOCKS
#ifdef _KERNEL
#define RPC_C_DG_SOCK_MAX_PRIV_SOCKS 0
#else
#define RPC_C_DG_SOCK_MAX_PRIV_SOCKS 3
#endif
#endif

/*
 * Return whether or not the sock pool elt's descriptor is disabled.
 * Done as a macro just to isolate users in case we want to change
 * the locking requirements (see above).
 */
#define RPC_DG_SOCK_IS_DISABLED(sp_elt)  ((sp_elt)->is_disabled)
   
/*
 * This macro should be called after each send/receive on a socket.  If
 * the call was successful, the count of consecutive errors is reset.
 * On error, the count is incremented, and if the count exceeds the maximum
 * allowed, the socket is disabled.
 *
 * We only check for EIO errors.  In the future we may want to check
 * for other errors as well.
 */
#define RPC_DG_SOCK_UPDATE_ERR_COUNT(sp_elt, serr) \
{ \
    if ((serr) != RPC_C_SOCKET_EIO) \
        (sp_elt)->error_cnt = 0; \
    else \
    { \
        if (++((sp_elt)->error_cnt) >= RPC_C_DG_SOCK_MAX_ERRORS) \
            rpc__dg_network_disable_desc(sp_elt); \
    } \
}

#define RPC_DG_SOCK_IS_BLOCKING_IO(sp_elt) \
    ((sp_elt)->is_private) 

/* 
 * Macros for locking/unlocking the socket pool's mutex.
 */                                     

#define RPC_DG_SOCK_POOL_LOCK(junk)    RPC_MUTEX_LOCK(rpc_g_dg_sock_pool.sp_mutex)
#define RPC_DG_SOCK_POOL_UNLOCK(junk)  RPC_MUTEX_UNLOCK(rpc_g_dg_sock_pool.sp_mutex)
#define RPC_DG_SOCK_POOL_LOCK_ASSERT(junk) \
                          RPC_MUTEX_LOCK_ASSERT(rpc_g_dg_sock_pool.sp_mutex)
                                
/* ======================================================================== */

PRIVATE void rpc__dg_network_use_protseq_sv _DCE_PROTOTYPE_((
        rpc_protseq_id_t  /*pseq_id*/,
        unsigned32  /*max_calls*/,
        rpc_addr_p_t  /*rpc_addr*/,
        unsigned_char_p_t  /*endpoint*/,
        unsigned32 * /*st*/
    ));

PRIVATE void rpc__dg_network_use_protseq_cl _DCE_PROTOTYPE_((
        rpc_protseq_id_t  /*pseq_id*/,
        rpc_dg_sock_pool_elt_p_t * /*sp_elt*/
    ));                                  

PRIVATE void rpc__dg_network_disable_desc _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp_elt*/
    ));

PRIVATE void rpc__dg_network_init _DCE_PROTOTYPE_((void));

PRIVATE void rpc__dg_network_fork_handler _DCE_PROTOTYPE_((
        rpc_fork_stage_id_t  /*stage*/
    ));

PRIVATE void rpc__dg_network_sock_release _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t * /*sp*/
    ));

PRIVATE void rpc__dg_network_sock_reference _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/
    ));

PRIVATE void rpc__dg_network_sock_release_slc _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/
    ));

PRIVATE void rpc__dg_network_sock_ref_slc _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  * /*sp*/
    ));


#endif /* _DGSOC_H */
