/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgcct.h,v $
 * Revision 65.1  1997/10/20 19:16:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.609.2  1996/02/18  22:56:07  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:15  marty]
 *
 * Revision 1.1.609.1  1995/12/08  00:19:34  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:52  root]
 * 
 * Revision 1.1.607.1  1994/01/21  22:36:38  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:50  cbrooks]
 * 
 * Revision 1.1.5.3  1993/01/03  23:23:52  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:38  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  20:47:29  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:24  zeliff]
 * 
 * Revision 1.1.2.3  1992/05/15  17:23:04  mishkin
 * 	Pass ccall to rpc__dg_cct_get() so we can update the CCT-related fields
 * 	ourselves rather than letting our caller do it (or not do it [bug]
 * 	which is how I came to the conclusion that it needed to be changed).
 * 	[1992/05/14  19:40:42  mishkin]
 * 
 * Revision 1.1.2.2  1992/05/12  16:16:31  mishkin
 * 	Remove "force_new" parameter from rpc__dg_cct_get().
 * 	[1992/05/08  12:40:35  mishkin]
 * 
 * Revision 1.1  1992/01/19  03:07:20  devrcs
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
**      dgcct.h
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

#ifndef _DGCCT_H
#define _DGCCT_H

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


/*
 * R P C _ D G _ C C T E _ R E F _ I N I T
 *
 * Initialize a CCTE soft reference.
 */

#define RPC_DG_CCTE_REF_INIT(rp) ( \
    (rp)->ccte = NULL, \
    (rp)->gc_count = 0 \
)

/*
 * R P C _ D G _ C C T _ R E F E R E N C E
 *
 * Increment the reference count for the CCTE. 
 */

#define RPC_DG_CCT_REFERENCE(ccte) { \
    assert((ccte)->refcnt < 255); \
    (ccte)->refcnt++; \
}

/*
 * R P C _ D G _ C C T _ R E L E A S E
 *
 * Release a CCTE and update its last time used (for CCT GC aging).
 * Retain the soft reference so we can reuse it on subsequent calls.
 */

#define RPC_DG_CCT_RELEASE(ccte_ref) { \
    rpc_dg_cct_elt_p_t ccte = (ccte_ref)->ccte; \
    assert(ccte->refcnt > 1); \
    if (--ccte->refcnt <= 1) \
        ccte->timestamp = rpc__clock_stamp(); \
}


#ifdef __cplusplus
extern "C" {
#endif


PRIVATE void rpc__dg_cct_get _DCE_PROTOTYPE_((
        rpc_auth_info_p_t /*auth_info*/,
        rpc_dg_ccall_p_t /*ccall*/
    ));

PRIVATE void rpc__dg_cct_release _DCE_PROTOTYPE_((
        rpc_dg_ccte_ref_p_t /*ccte_ref*/
    ));

PRIVATE void rpc__dg_cct_fork_handler _DCE_PROTOTYPE_((
        rpc_fork_stage_id_t /*stage*/
    ));

#ifdef __cplusplus
}
#endif

#endif /* _DGCCT_H */
