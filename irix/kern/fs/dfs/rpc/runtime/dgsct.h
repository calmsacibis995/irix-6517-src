/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgsct.h,v $
 * Revision 65.1  1997/10/20 19:16:58  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.821.2  1996/02/18  22:56:16  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:24  marty]
 *
 * Revision 1.1.821.1  1995/12/08  00:20:16  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/10/18  15:17 UTC  jss
 * 	Merge fix for CHFts257.
 * 
 * 	HP revision /main/jss_dced_acl/1  1995/10/18  14:56 UTC  jss
 * 	Fix for CHFts16257. Define a constant for maximum refcnt so that code
 * 	used for checking for maximum maybe calls will keep working if refcnt
 * 	changes.
 * 	[1995/12/07  23:59:16  root]
 * 
 * Revision 1.1.819.1  1994/01/21  22:37:18  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:34  cbrooks]
 * 
 * Revision 1.1.7.1  1993/10/04  18:17:38  markar
 * 	     OT CR 1236 fix: Make back-to-back maybe calls work.
 * 	[1993/09/30  13:39:41  markar]
 * 
 * Revision 1.1.5.3  1993/01/03  23:24:47  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:10  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:49:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:55  zeliff]
 * 
 * Revision 1.1.2.4  1992/05/15  17:23:31  mishkin
 * 	- Rename "force" param to rpc__dg_sct_way_validate() to be "force_way_auth"
 * 	  so its purpose is clearer.
 * 	[1992/05/15  17:03:49  mishkin]
 * 
 * Revision 1.1.2.3  1992/05/12  16:18:59  mishkin
 * 	- Convert RPC_DG_SCT_INQ_SCALL, RPC_DG_SCT_NEW_CALL, and
 * 	  RPC_DG_SCT_BACKOUT_NEW_CALL from macros to functions.
 * 	- Make RPC_DG_SCT_IS_WAY_VALIDATED test scall for client_needs_sboot condition.
 * 	[1992/05/08  12:48:15  mishkin]
 * 
 * Revision 1.1.2.2  1992/05/01  17:21:19  rsalz
 * 	 13-dec-91 sommerfeld    Add "force" arg to rpc__dg_sct_way_validate().
 * 	[1992/05/01  17:16:53  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:11:58  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _DGSCT_H
#define _DGSCT_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgsct.h
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
 * R P C _ D G _ S C T _ I S _ W A Y _ V A L I D A T E D
 *
 * Return true only if the connection has a WAY validated seq and the
 * client doesn't require us to WAY it just to get it the server's boot
 * time.
 *
 * It's ok to look at the flag without the lock;  once it's set, it
 * never becomes unset - if the test fails we'll end up doing extra
 * work when we may not have needed to.
 */

#define RPC_DG_SCT_IS_WAY_VALIDATED(scte) \
( \
    (scte)->high_seq_is_way_validated && \
    ! (scte)->scall->client_needs_sboot \
)

/*
 * R P C _ D G _ S C T _ R E F E R E N C E
 *
 * Increment the reference count for the SCTE.
 */
#define RPC_DG_SCT_REFCNT_MAX  255

#define RPC_DG_SCT_REFERENCE(scte) { \
    assert((scte)->refcnt < RPC_DG_SCT_REFCNT_MAX); \
    (scte)->refcnt++; \
}

/*
 * R P C _ D G _ S C T _ R E L E A S E
 *
 * Release a currently inuse SCTE.
 *
 * If the reference count goes to one, the SCTE is now available for
 * reuse / or GCing.  Update the SCTE's last used timestamp.
 */

#define RPC_DG_SCT_RELEASE(scte) { \
    RPC_LOCK_ASSERT(0); \
    assert((*(scte))->refcnt > 0); \
    if (--(*scte)->refcnt <= 1) \
        (*(scte))->timestamp = rpc__clock_stamp(); \
    *(scte) = NULL; \
}


PRIVATE void rpc__dg_sct_inq_scall _DCE_PROTOTYPE_((
        rpc_dg_sct_elt_p_t  /*scte*/,
        rpc_dg_scall_p_t * /*scallp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

PRIVATE void rpc__dg_sct_new_call _DCE_PROTOTYPE_((
        rpc_dg_sct_elt_p_t  /*scte*/,
        rpc_dg_sock_pool_elt_p_t  /*si*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t * /*scallp*/
    ));

PRIVATE void rpc__dg_sct_backout_new_call _DCE_PROTOTYPE_((
        rpc_dg_sct_elt_p_t  /*scte*/,
        unsigned32  /*seq*/
    ));

PRIVATE rpc_dg_sct_elt_p_t rpc__dg_sct_lookup _DCE_PROTOTYPE_((
        uuid_p_t  /*actid*/,
        unsigned32  /*probe_hint*/
    ));

PRIVATE rpc_dg_sct_elt_p_t rpc__dg_sct_get _DCE_PROTOTYPE_((
        uuid_p_t  /*actid*/,
        unsigned32  /*probe_hint*/,
        unsigned32  /*seq*/
    ));

PRIVATE rpc_binding_handle_t rpc__dg_sct_make_way_binding _DCE_PROTOTYPE_((
        rpc_dg_sct_elt_p_t  /*scte*/,
        unsigned32 * /*st*/
    ));

PRIVATE void rpc__dg_sct_way_validate _DCE_PROTOTYPE_ ((
        rpc_dg_sct_elt_p_t  /*scte*/,
        unsigned32       /*force_way_auth*/,
        unsigned32      * /*st*/
    ));
      
PRIVATE void rpc__dg_sct_fork_handler _DCE_PROTOTYPE_((
        rpc_fork_stage_id_t  /*stage*/
    ));

#endif /* _DGSCT_H */
