/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dghnd.c,v 65.4 1998/03/23 17:29:24 gwehrman Exp $";
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
 * $Log: dghnd.c,v $
 * Revision 65.4  1998/03/23 17:29:24  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:22  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:25  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:56  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.511.1  1996/10/15  20:47:19  arvind
 * 	CHFts20197: host_bound is not initialized.
 * 	[1996/10/09  18:30 UTC  sommerfeld  /main/sommerfeld_pk_kdc_2/1]
 *
 * Revision 1.1.509.2  1996/02/18  00:03:44  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:14  marty]
 * 
 * Revision 1.1.509.1  1995/12/08  00:19:52  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:32 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 	[1995/12/07  23:59:02  root]
 * 
 * Revision 1.1.507.3  1994/06/21  21:53:58  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:31:35  hopkins]
 * 
 * Revision 1.1.507.2  1994/05/27  15:36:17  tatsu_s
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:02:58  tatsu_s]
 * 
 * Revision 1.1.507.1  1994/01/21  22:36:58  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:02  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:24:15  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:19  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:48:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:05  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/07  19:06:11  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:29:56  markar]
 * 
 * Revision 1.1  1992/01/19  03:08:30  devrcs
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
**      dghnd.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for manipulating Datagram Specific RPC bindings.
**
**
*/

#include <dg.h>
#include <dghnd.h>
#include <dgcall.h>
#include <dgccall.h>

/* ======================================================================== */

/*
 * R E L E A S E _ C A C H E D _ C C A L L
 *
 * Release any client call binding we might have in the binding binding.
 */

INTERNAL void release_cached_ccall _DCE_PROTOTYPE_((
        rpc_dg_binding_client_p_t /*h*/
    ));

INTERNAL void release_cached_ccall
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_client_p_t h                       
)
#else
(h)
rpc_dg_binding_client_p_t h;                       
#endif
{
    if (h->ccall == NULL) 
        return;

    RPC_DG_CALL_LOCK(&h->ccall->c);
    rpc__dg_ccall_free_prep(h->ccall);
    RPC_DG_CCALL_RELEASE(&h->ccall);
    /* unlocks as a side effect */
}


/*
 * R P C _ _ D G _ B I N D I N G _ A L L O C
 *
 * Allocate a DG-specific binding rep.
 */

PRIVATE rpc_binding_rep_p_t rpc__dg_binding_alloc
#ifdef _DCE_PROTO_
(
    boolean32 is_server,
    unsigned32 *st
)
#else
(is_server, st)
boolean32 is_server;
unsigned32 *st;
#endif
{
    rpc_binding_rep_p_t h;

    *st = rpc_s_ok;

    if (is_server)
    {
        RPC_MEM_ALLOC(h, rpc_binding_rep_p_t, sizeof(rpc_dg_binding_server_t), 
                        RPC_C_MEM_DG_SHAND, RPC_C_MEM_NOWAIT);
	if (h == NULL)
	    *st = rpc_s_no_memory;
    }
    else
    {
        RPC_MEM_ALLOC(h, rpc_binding_rep_p_t, sizeof(rpc_dg_binding_client_t), 
                        RPC_C_MEM_DG_CHAND, RPC_C_MEM_NOWAIT);
	if (h == NULL)
	    *st = rpc_s_no_memory;
    }

    return(h);
}


/*
 * R P C _ _ D G _ B I N D I N G _ I N I T
 *
 * Initialize the DG-specific part of a client binding binding.
 */

PRIVATE void rpc__dg_binding_init
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h_,
    unsigned32 *st
)
#else
(h_, st)
rpc_binding_rep_p_t h_;
unsigned32 *st;
#endif
{
    *st = rpc_s_ok;

    if (RPC_BINDING_IS_SERVER(h_))
    {
        rpc_dg_binding_server_p_t h = (rpc_dg_binding_server_p_t) h_;

        h->chand        = NULL;
        h->scall        = NULL;
    }
    else
    {
        rpc_dg_binding_client_p_t h = (rpc_dg_binding_client_p_t) h_;

        h->ccall          = NULL;
        h->server_boot    = 0;
	h->host_bound	  = false;
        h->shand          = NULL; 
        h->maint_binding  = NULL;  
        h->is_WAY_binding = 0;
    }
}


/*
 * R P C _ _ D G _ B I N D I N G _ F R E E
 *
 * Free a DG-specific, client-side binding binding.
 */

PRIVATE void rpc__dg_binding_free
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t *h_,
    unsigned32 *st
)
#else
(h_, st)
rpc_binding_rep_p_t *h_;
unsigned32 *st;
#endif
{
    /*
     * The binding and the call binding are public at this point (remember
     * the call monitor) hence we need to ensure that things remain
     * orderly.
     */
    RPC_LOCK_ASSERT(0);

    if ((*h_)->is_server == 0)
    {
        release_cached_ccall((rpc_dg_binding_client_p_t) *h_);
        RPC_MEM_FREE(*h_, RPC_C_MEM_DG_CHAND);
    }
    else
    {
        RPC_MEM_FREE(*h_, RPC_C_MEM_DG_SHAND);
    }

    *h_ = NULL;

    *st = rpc_s_ok;
}


/*
 * R P C _ _ D G _ B I N D I N G _ C H A N G E D
 *
 * 
 */

PRIVATE void rpc__dg_binding_changed
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h,
    unsigned32 *st
)
#else
(h, st)
rpc_binding_rep_p_t h;
unsigned32 *st;
#endif
{
    RPC_LOCK(0);    /* !!! this should really be done in common code */

    release_cached_ccall((rpc_dg_binding_client_p_t) h);    /* !!! Crude but effective.  Sufficient? */
    *st = rpc_s_ok;

    RPC_UNLOCK(0);
}


/*
 * R P C _ _ D G _ B I N D I N G _ I N Q _ A D D R
 *
 * 
 */

PRIVATE void rpc__dg_binding_inq_addr
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h,
    rpc_addr_p_t *addr,
    unsigned32 *st
)
#else
(h, addr, st)
rpc_binding_rep_p_t h;
rpc_addr_p_t *addr;
unsigned32 *st;
#endif
{
    /*
     * rpc_m_unimp_call
     * "(%s) Call not implemented"
     */
    RPC_DCE_SVC_PRINTF ((
	DCE_SVC(RPC__SVC_HANDLE, "%s"),
	rpc_svc_general,
	svc_c_sev_fatal | svc_c_action_abort,
	rpc_m_unimp_call,
        "rpc__dg_binding_inq_addr" ));
}


/*
 * R P C _ _ D G _ B I N D I N G _ R E S E T
 *
 * Dissociate the binding from a bound server instance.
 */

PRIVATE void rpc__dg_binding_reset
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h_,
    unsigned32 *st
)
#else
(h_, st)
rpc_binding_rep_p_t h_;
unsigned32 *st;
#endif
{
    rpc_dg_binding_client_p_t h = (rpc_dg_binding_client_p_t) h_;

    RPC_LOCK(0);    /* !!! this should really be done in common code */

    h->server_boot   = 0;
    release_cached_ccall(h);    /* Crude but effective */
    *st = rpc_s_ok;

    RPC_UNLOCK(0);
}


/*
 * R P C _ _ D G _ B I N D I N G _ C O P Y
 *
 * Copy DG private part of a binding.
 */

PRIVATE void rpc__dg_binding_copy
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t src_h_,
    rpc_binding_rep_p_t dst_h_,
    unsigned32 *st
)
#else
(src_h_, dst_h_, st)
rpc_binding_rep_p_t src_h_;
rpc_binding_rep_p_t dst_h_;
unsigned32 *st;
#endif
{
    rpc_dg_binding_client_p_t src_h = (rpc_dg_binding_client_p_t) src_h_;
    rpc_dg_binding_client_p_t dst_h = (rpc_dg_binding_client_p_t) dst_h_;

    RPC_LOCK(0);    /* !!! this should really be done in common code */

    dst_h->server_boot = src_h->server_boot;
    dst_h->host_bound  = src_h->host_bound;

    *st = rpc_s_ok;

    RPC_UNLOCK(0);
}


/*
 * R P C _ _ D G _ B I N D I N G _ S E R V E R _ T O _ C L I E N T
 *
 * Create a client binding binding suitable for doing callbacks from a
 * server to a client.  The server binding binding of the current call
 * is used as a template for the new client binding.
 */ 

PRIVATE rpc_dg_binding_client_p_t rpc__dg_binding_srvr_to_client
#ifdef _DCE_PROTO_
(
    rpc_dg_binding_server_p_t shand,                                               
    unsigned32 *st
)
#else
(shand, st)
rpc_dg_binding_server_p_t shand;                                               
unsigned32 *st;
#endif
{                        
    rpc_dg_binding_client_p_t chand;
    rpc_addr_p_t   client_addr;

    *st = rpc_s_ok;    

    /*
     * If we've already made a callback on this binding binding,
     * then use the client binding binding that was previously
     * created.
     */

    if (shand->chand != NULL)
        return(shand->chand);

    /*
     * Otherwise, allocate a client binding binding and bind it to the
     * address associated with the server binding binding.
     */

    rpc__naf_addr_copy(shand->c.c.rpc_addr, &client_addr, st);                   
    chand = (rpc_dg_binding_client_p_t) 
        rpc__binding_alloc(false, &UUID_NIL, RPC_C_PROTOCOL_ID_NCADG,
                                   client_addr, st);
    if (*st != rpc_s_ok)
        return(NULL);
   
    /*
     * Link the binding bindings.  
     */

    chand->shand = shand; 
    shand->chand = chand;

    return(chand);
}

/*
 * R P C _ _ D G _ B I N D I N G _ C R O S S _ F O R K
 *
 * This routine makes it possible for children of forks to use
 * binding handles inherited from their parents.  It does this
 * by vaporizing any state associated with the handle, so that
 * the child is forced to rebuild its own state from scratch.
 */

PRIVATE void rpc__dg_binding_cross_fork
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t h_,
    unsigned32 *st
)
#else
(h_, st)
rpc_binding_rep_p_t h_;
unsigned32 *st;
#endif
{   
    rpc_dg_binding_client_p_t h = (rpc_dg_binding_client_p_t) h_;

    *st = rpc_s_ok;
                            
    /*
     * Drop any state associated with this handle.  This state
     * survives in the parent of the fork, and will be processed
     * correctly there.
     */
    h->ccall = NULL;
    h->shand = NULL;
    h->maint_binding = NULL;
}


