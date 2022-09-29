/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comfwd.h,v $
 * Revision 65.1  1997/10/20 19:16:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.37.2  1996/02/18  22:55:43  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:53  marty]
 *
 * Revision 1.1.37.1  1995/12/08  00:18:13  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:07  root]
 * 
 * Revision 1.1.35.2  1994/04/15  18:57:25  tom
 * 	Support for dced (OT 10352):
 * 	 Add actuuid parameter to rpc_fwd_map_fn_t.
 * 	 Add rpc_e_fwd_delayed to rpc_fwd_action_t.
 * 	 Add protype of rpc__server_fwd_resolve_delayed.
 * 	[1994/04/14  18:47:46  tom]
 * 
 * Revision 1.1.35.1  1994/01/21  22:35:18  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:06  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:00:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:41  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:20:16  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:22  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:07  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMFWD_H
#define _COMFWD_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      comfwd.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Private interface to the Common Communications Forwarding Service for use
**  by RPC Protocol Services and Local Location Broker.  This service is
**  in its own file (rather then com.h) so that the llb does not have to
**  include other runtime internal include files.
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/***********************************************************************/
/*
 * The beginning of this file specifies the "public" (visible to the llbd)
 * portion of the fwd interface.
 */

/*
 * The signature of a server forwarding map function.
 *
 * The function determines an appropriate forwarding address for the
 * packet based on the provided info.  The "fwd_action" output parameter
 * determines the disposition of the packet; "drop" means just drop the
 * packet (don't forward, don't send anything back to the client), "reject"
 * means send a rejection back to the client (and drop the packet), and
 * "forward" means that the packet should be forwarded to the address
 * specified in the "fwd_addr" output parameter.
 *
 * WARNING:  This should be a relatively light weight, non-blocking
 * function as the runtime may (will likely) be calling the function
 * from the context of the runtime's listener thread (i.e. handling
 * of additional incoming received packets is suspended while this
 * provided function is executing).
 */

#ifdef __cplusplus
extern "C" {
#endif
   

typedef enum 
{
    rpc_e_fwd_drop,
    rpc_e_fwd_reject,
    rpc_e_fwd_forward,
    rpc_e_fwd_delayed
} rpc_fwd_action_t;

typedef void (*rpc_fwd_map_fn_t) _DCE_PROTOTYPE_ ((
        /* [in] */    uuid_p_t           /*obj_uuid*/,
        /* [in] */    rpc_if_id_p_t      /*if_id*/,
        /* [in] */    rpc_syntax_id_p_t  /*data_rep*/,
        /* [in] */    rpc_protocol_id_t  /*rpc_protocol*/,
        /* [in] */    unsigned32         /*rpc_protocol_vers_major*/,
        /* [in] */    unsigned32         /*rpc_protocol_vers_minor*/,
        /* [in] */    rpc_addr_p_t       /*addr*/,
        /* [in] */    uuid_p_t           /*actuuid*/,
        /* [out] */   rpc_addr_p_t      * /*fwd_addr*/,
        /* [out] */   rpc_fwd_action_t  * /*fwd_action*/,
        /* [out] */   unsigned32        * /*status*/
    ));

/*
 * Register a forwarding map function with the runtime.  This registered
 * function will be called by the protocol services to determine an
 * appropriate forwarding endpoint for a received pkt that is not for
 * any of the server's registered interfaces.
 */
PRIVATE void rpc__server_register_fwd_map _DCE_PROTOTYPE_ ((
        /* [in] */    rpc_fwd_map_fn_t    /*map_fn*/,
        /* [out] */   unsigned32          * /*status*/
    ));

PRIVATE void rpc__server_fwd_resolve_delayed _DCE_PROTOTYPE_ ((
	/* [in] */   uuid_p_t            /*actuuid*/,
        /* [in] */   rpc_addr_p_t        /*fwd_addr*/,
        /* [in] */   rpc_fwd_action_t  * /*fwd_action*/,
        /* [out] */  unsigned32        * /*status*/
    ));

/***********************************************************************/
/*
 * The following are to be considered internal to the runtime.
 */

/*
 * R P C _ G _ F W D _ F N
 *
 * The global forwarding map function variable.  Its value indicates
 * whether or not the RPC runtime should be performing forwarding services
 * and if so, the forwarding map function to use. Its definition is in comp.c.
 */
EXTERNAL rpc_fwd_map_fn_t       rpc_g_fwd_fn;

#endif /* _COMFWD_H_ */
