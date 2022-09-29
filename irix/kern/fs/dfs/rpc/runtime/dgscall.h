/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgscall.h,v $
 * Revision 65.1  1997/10/20 19:16:58  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.319.2  1996/02/18  22:56:15  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:23  marty]
 *
 * Revision 1.1.319.1  1995/12/08  00:20:13  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:14  root]
 * 
 * Revision 1.1.317.1  1994/01/21  22:37:15  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:32  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:42  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:01  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:49:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:47  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:31  devrcs
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
**      dgscall.h
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

#ifndef _DGSCALL_H
#define _DGSCALL_H

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


/*
 * R P C _ D G _ S C A L L _ R E L E A S E
 *
 * Decrement the reference count for the SCALL and
 * NULL the reference.
 */

#define RPC_DG_SCALL_RELEASE(scall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(scall))->c); \
    assert((*(scall))->c.refcnt > 0); \
    if (--(*(scall))->c.refcnt == 0) \
        rpc__dg_scall_free(*(scall)); \
    else \
        RPC_DG_CALL_UNLOCK(&(*(scall))->c); \
    *(scall) = NULL; \
}

/*
 * R P C _ D G _ S C A L L _ R E L E A S E _ N O _ U N L O C K
 *
 * Like RPC_DG_SCALL_RELEASE, except doesn't unlock the SCALL.  Note
 * that the referencing counting model requires that this macro can be
 * used iff the release will not be the "last one" (i.e., the one that
 * would normally cause the SCALL to be freed).
 */

#define RPC_DG_SCALL_RELEASE_NO_UNLOCK(scall) { \
    RPC_DG_CALL_LOCK_ASSERT(&(*(scall))->c); \
    assert((*(scall))->c.refcnt > 1); \
    --(*(scall))->c.refcnt; \
    *(scall) = NULL; \
}


PRIVATE void rpc__dg_scall_free _DCE_PROTOTYPE_((rpc_dg_scall_p_t  /*scall*/));


PRIVATE void rpc__dg_scall_reinit _DCE_PROTOTYPE_((
        rpc_dg_scall_p_t  /*scall*/,
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

PRIVATE rpc_dg_scall_p_t rpc__dg_scall_alloc _DCE_PROTOTYPE_((
        rpc_dg_sct_elt_p_t  /*scte*/,
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

PRIVATE rpc_dg_scall_p_t rpc__dg_scall_cbk_alloc _DCE_PROTOTYPE_((
        rpc_dg_ccall_p_t  /*ccall*/,
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

PRIVATE void rpc__dg_scall_orphan_call _DCE_PROTOTYPE_((
	rpc_dg_scall_p_t  /*scall*/
    ));

#endif /* _DGSCALL_H */
