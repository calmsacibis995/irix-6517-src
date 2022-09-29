/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgccall.h,v $
 * Revision 65.1  1997/10/20 19:16:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.505.2  1996/02/18  22:56:05  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:13  marty]
 *
 * Revision 1.1.505.1  1995/12/08  00:19:28  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:32 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1995/08/29  19:18 UTC  tatsu_s
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.psock_timeout.b0/1  1995/08/16  20:26 UTC  tatsu_s
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:15 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/2  1995/01/16  14:26 UTC  markar
 * 	don't reinit -> sock in set_idl
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  18:25 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:58:49  root]
 * 
 * Revision 1.1.503.1  1994/01/21  22:36:30  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:30  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:23:43  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:23  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:47:06  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:09  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/12  16:15:39  mishkin
 * 	Turn RPC_DG_CCALL_LSCT_INQ_SCALL & RPC_DG_CCALL_LSCT_NEW_CALL into functions.
 * 	[1992/05/08  12:38:12  mishkin]
 * 
 * Revision 1.1  1992/01/19  03:08:21  devrcs
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
**      dgccall.h
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

#ifndef _DGCCALL_H
#define _DGCCALL_H	1

#ifndef _DCE__DCE_PROTOTYPE__
#include <dce/dce.h>
#endif

#include <dgccallt.h>

/*
 * R P C _ D G _ C C A L L _ S E T _ S T A T E _ I D L E
 *
 * Remove the call handle from the CCALLT.  Release our reference to
 * our CCTE.  (In the case of CCALLs created to do server callbacks there
 * won't be a ccte_ref.)  Change to the idle state.  If you're trying
 * to get rid of the ccall use rpc__dg_ccall_free_prep() instead.
 */

#define RPC_DG_CCALL_SET_STATE_IDLE(ccall) { \
    if ((ccall)->c.state == rpc_e_dg_cs_final) \
        rpc__dg_ccall_ack(ccall); \
    rpc__dg_ccallt_remove(ccall); \
    if (! (ccall)->c.is_cbk)\
        RPC_DG_CCT_RELEASE(&(ccall)->ccte_ref); \
    RPC_DG_CALL_SET_STATE(&(ccall)->c, rpc_e_dg_cs_idle); \
}
/*
 * R P C _ D G _ C C A L L _ R E L E A S E
 *
 * Decrement the reference count for the CCALL and
 * NULL the reference.
 */

#define RPC_DG_CCALL_RELEASE(ccall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(ccall))->c); \
    assert((*(ccall))->c.refcnt > 0); \
    if (--(*(ccall))->c.refcnt == 0) \
        rpc__dg_ccall_free(*(ccall)); \
    else \
        RPC_DG_CALL_UNLOCK(&(*(ccall))->c); \
    *(ccall) = NULL; \
}

/*
 * R P C _ D G _ C C A L L _ R E L E A S E _ N O _ U N L O C K
 *
 * Like RPC_DG_CCALL_RELEASE, except doesn't unlock the CCALL.  Note
 * that the referencing counting model requires that this macro can be
 * used iff the release will not be the "last one" (i.e., the one that
 * would normally cause the CCALL to be freed).
 */

#define RPC_DG_CCALL_RELEASE_NO_UNLOCK(ccall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(ccall))->c); \
    assert((*(ccall))->c.refcnt > 1); \
    --(*(ccall))->c.refcnt; \
    *(ccall) = NULL; \
}


#ifdef __cplusplus
extern "C" {
#endif


PRIVATE void rpc__dg_ccall_lsct_inq_scall _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/,
        rpc_dg_scall_p_t * /*scallp*/
    ));

PRIVATE void rpc__dg_ccall_lsct_new_call _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/,
        rpc_dg_sock_pool_elt_p_t  /*si*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t * /*scallp*/
    ));

PRIVATE void rpc__dg_ccall_ack _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_ccall_free _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_ccall_free_prep _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_ccall_timer _DCE_PROTOTYPE_(( pointer_t /*p*/ ));

PRIVATE void rpc__dg_ccall_xmit_cancel_quit _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/,
        unsigned32 /*cancel_id*/
    ));

PRIVATE void rpc__dg_ccall_setup_cancel_tmo _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_ccall_xmit_ping _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t /*ccall*/
    ));
#ifdef __cplusplus
}
#endif


#endif /* _DGCCALL_H */
