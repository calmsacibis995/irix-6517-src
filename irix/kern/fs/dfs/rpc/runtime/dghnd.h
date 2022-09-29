/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dghnd.h,v $
 * Revision 65.1  1997/10/20 19:16:56  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.310.2  1996/02/18  22:56:12  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:20  marty]
 *
 * Revision 1.1.310.1  1995/12/08  00:19:54  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:03  root]
 * 
 * Revision 1.1.308.1  1994/01/21  22:37:00  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:03  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:17  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:23  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:48:24  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:08  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:16  devrcs
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
**      dghnd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Include file for "dghnd.c".
**
**
*/

#ifndef _DGHND_H
#define _DGHND_H

/*
 * R P C _ D G _ B I N D I N G _ S E R V E R _ R E I N I T 
 *
 * Reinitialize the per-call variant information in a cached handle.
 * The binding handle is bound to the scall; the scall is bound to an
 * actid; the actid is bound to the auth identity - so the auth identity
 * can't change.  The object id can change.  The address of the client
 * can change (actid's aren't even bound to a protseq let alone specific
 * network-addr/endpoint pairs; sigh).
 */
#define RPC_DG_BINDING_SERVER_REINIT(h) { \
    unsigned32 st; \
    \
    (h)->c.c.obj = (h)->scall->c.call_object; \
    rpc__naf_addr_overcopy((h)->scall->c.addr, &(h)->c.c.rpc_addr, &st); \
}

/* ========================================================================= */

rpc_binding_rep_t *rpc__dg_binding_alloc    _DCE_PROTOTYPE_((
        boolean32  /*is_server*/,
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_init    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*h*/, 
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_reset    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_changed    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_free    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t * /*h*/,
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_inq_addr    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*h*/,
        rpc_addr_p_t * /*rpc_addr*/,
        unsigned32 * /*st*/
    ));
void rpc__dg_binding_copy    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*src_h*/,
        rpc_binding_rep_p_t  /*dst_h*/,
        unsigned32 * /*st*/
    ));

rpc_dg_binding_client_p_t rpc__dg_binding_srvr_to_client    _DCE_PROTOTYPE_((
        rpc_dg_binding_server_p_t  /*shand*/,
        unsigned32 * /*st*/
    ));

void rpc__dg_binding_cross_fork    _DCE_PROTOTYPE_((
        rpc_binding_rep_p_t  /*h*/,
        unsigned32 * /*st*/
    ));

#endif /* _DGHND_H */

