/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgccallt.c,v 65.4 1998/03/23 17:29:43 gwehrman Exp $";
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
 * $Log: dgccallt.c,v $
 * Revision 65.4  1998/03/23 17:29:43  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:28  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:31  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.306.2  1996/02/18  00:03:26  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:00  marty]
 *
 * Revision 1.1.306.1  1995/12/08  00:19:30  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:49  root]
 * 
 * Revision 1.1.304.1  1994/01/21  22:36:32  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:31  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:23:45  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:26  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:47:11  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:13  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:11  devrcs
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
**      dgccallt.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Handle client call table (CCALLT).
**
**
*/

#include <dg.h>
#include <dgcall.h>
#include <dgccall.h>
#include <dgccallt.h>

#include <dce/assert.h>

/*
 * R P C _ _ D G _ C C A L L T _ I N S E R T
 *
 * Add a client call handle to the CCALLT.  Increment the call handle's reference
 * count.
 */

PRIVATE void rpc__dg_ccallt_insert
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    unsigned16 probe = ccall->c.actid_hash % RPC_DG_CCALLT_SIZE;

    RPC_LOCK_ASSERT(0);

    ccall->c.next = (rpc_dg_call_p_t) rpc_g_dg_ccallt[probe];
    rpc_g_dg_ccallt[probe] = ccall;
    RPC_DG_CALL_REFERENCE(&ccall->c);
}


/*
 * R P C _ _ D G _ C C A L L T _ R E M O V E
 *
 * Remove a client call handle from the CCALLT.  Decrement the call
 * handle's reference count.  This routine can be called only if there
 * are references to the CCALL other than the one from the CCALLT.
 */

PRIVATE void rpc__dg_ccallt_remove
#ifdef _DCE_PROTO_
(
    rpc_dg_ccall_p_t ccall
)
#else
(ccall)
rpc_dg_ccall_p_t ccall;
#endif
{
    unsigned16 probe = ccall->c.actid_hash % RPC_DG_CCALLT_SIZE;
    rpc_dg_ccall_p_t scan_ccall, prev_scan_ccall;

    RPC_LOCK_ASSERT(0);
    RPC_DG_CALL_LOCK_ASSERT(&ccall->c);
    assert(ccall->c.refcnt > 1);

    /*
     * Scan down the hash chain.
     */

    scan_ccall = rpc_g_dg_ccallt[probe];
    prev_scan_ccall = NULL;

    while (scan_ccall != NULL) {
        if (scan_ccall == ccall) {
            if (prev_scan_ccall == NULL)
                rpc_g_dg_ccallt[probe] = (rpc_dg_ccall_p_t) scan_ccall->c.next;
            else
                prev_scan_ccall->c.next = scan_ccall->c.next;
            RPC_DG_CCALL_RELEASE_NO_UNLOCK(&scan_ccall);
            return;
        }
        else {
            prev_scan_ccall = scan_ccall;
            scan_ccall = (rpc_dg_ccall_p_t) scan_ccall->c.next;
        }
    }

    assert(false);          /* Shouldn't ever fail to find the ccall */
}


/*
 * R P C _ _ D G _ C C A L L T _ L O O K U P
 *
 * Find a client call handle in the CCALLT based on an activity ID and
 * probe hint (ahint).  Return the ccall or NULL if no matching entry
 * is found.  Note that a probe_hint of RPC_C_DG_NO_HINT can be used if
 * the caller doesn't have a hint.  If we're returning a ccall, it will
 * be locked and have it's reference count incremented already.
 */

PRIVATE rpc_dg_ccall_p_t rpc__dg_ccallt_lookup
#ifdef _DCE_PROTO_
(
    uuid_p_t actid,
    unsigned32 probe_hint
)
#else
(actid, probe_hint)
uuid_p_t actid;
unsigned32 probe_hint;
#endif
{
    rpc_dg_ccall_p_t ccall;
    unsigned16 probe;
    unsigned32 st;
    boolean once = false;

    RPC_LOCK_ASSERT(0);

    /*
     * Determine the hash chain to use
     */

    if (probe_hint == RPC_C_DG_NO_HINT || probe_hint >= RPC_DG_CCALLT_SIZE)
        probe = rpc__dg_uuid_hash(actid) % RPC_DG_CCALLT_SIZE;
    else
        probe = probe_hint;

    /*
     * Scan down the probe chain, reserve and return a matching SCTE.
     */

RETRY:
    ccall = rpc_g_dg_ccallt[probe];

    while (ccall != NULL) 
    {
        if (UUID_EQ(*actid, ccall->c.call_actid, &st)) 
        {
            RPC_DG_CALL_LOCK(&ccall->c);
            RPC_DG_CALL_REFERENCE(&ccall->c);
            return(ccall);
        }
        else
            ccall = (rpc_dg_ccall_p_t) ccall->c.next;
    }

    /*
     * No matching entry found.  If we used the provided hint, try
     * recomputing the probe and if this yields a new probe, give it
     * one more try.
     */

    if (probe == probe_hint && !once) 
    {
        once = true;
        probe = rpc__dg_uuid_hash(actid) % RPC_DG_CCALLT_SIZE;
        if (probe != probe_hint)
            goto RETRY;
    }

    return(NULL);
}

/*
 * R P C _ _ D G _ C C A L L T _ F O R K _ H A N D L E R 
 *
 * Handle fork related processing for this module.
 */
PRIVATE void rpc__dg_ccallt_fork_handler
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

    switch ((int)stage)
    {
        case RPC_C_PREFORK:
            break;
        case RPC_C_POSTFORK_PARENT:
            break;
        case RPC_C_POSTFORK_CHILD:  
            /*
             * Clear out the Client Call Handle Table
             */                                
        
            for (i = 0; i < RPC_DG_CCALLT_SIZE; i++)
                rpc_g_dg_ccallt[i] = NULL;
            break;
    }
}

