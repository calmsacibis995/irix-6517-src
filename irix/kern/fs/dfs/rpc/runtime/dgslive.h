/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgslive.h,v $
 * Revision 65.1  1997/10/20 19:16:59  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.323.2  1996/02/18  22:56:18  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:25  marty]
 *
 * Revision 1.1.323.1  1995/12/08  00:20:19  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:18  root]
 * 
 * Revision 1.1.321.1  1994/01/21  22:37:21  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:36  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:51  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:17  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:49:41  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:03  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:30  devrcs
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
**      dgslive.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG maintain/monitor liveness service routines.
**
**
*/

#ifndef _DGSLIVE_H
#define _DGSLIVE_H


/*
 * R P C _ D G _ C L I E N T _ R E L E A S E
 *
 * Release a reference to a client handle.  This macro is called by the SCT
 * monitor when it is time to free an SCT entry.  It the entry has a reference
 * to a client handle, that reference is decremented.  If the count falls to 1,
 * meaning the only other reference is the client_rep table, the client_free
 * routine is called to free the handle.
 */

#define RPC_DG_CLIENT_RELEASE(scte) { \
    if ((scte)->client != NULL) \
    { \
        rpc_dg_client_rep_p_t client = (scte)->client; \
        assert(client->refcnt > 1); \
        if (--client->refcnt == 1) \
            rpc__dg_client_free((rpc_client_handle_t) (scte)->client);\
        (scte)->client = NULL; \
    } \
}

PRIVATE void rpc__dg_binding_inq_client _DCE_PROTOTYPE_((   
        rpc_binding_rep_p_t  /*binding_r*/,
        rpc_client_handle_t * /*client_h*/,
        unsigned32 * /*st*/
    ));

PRIVATE void rpc__dg_client_free _DCE_PROTOTYPE_((   
        rpc_client_handle_t  /*client_h*/
    ));

#endif /* _DGSLIVE_H */
