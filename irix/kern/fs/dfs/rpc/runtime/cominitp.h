/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cominitp.h,v $
 * Revision 65.1  1997/10/20 19:16:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.40.2  1996/02/18  22:55:45  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:55  marty]
 *
 * Revision 1.1.40.1  1995/12/08  00:18:21  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:12  root]
 * 
 * Revision 1.1.38.1  1994/01/21  22:35:28  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:13  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:22:14  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:02  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:44:02  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:44  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:08  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMINITP_H
#define _COMINITP_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      cominitp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Private interface to the Common Communications Service Initialization
**  Service.
**
**
*/

/***********************************************************************/
/*
 * Note: these are defined for later use of shared images
 */
#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


PRIVATE rpc_naf_init_fn_t rpc__load_naf _DCE_PROTOTYPE_ ((
        rpc_naf_id_elt_p_t              /*naf*/, 
        unsigned32                      * /*st*/
    ));

PRIVATE rpc_prot_init_fn_t rpc__load_prot _DCE_PROTOTYPE_ ((
        rpc_protocol_id_elt_p_t         /*rpc_protocol*/,
        unsigned32                      * /*st*/
    ));

PRIVATE rpc_auth_init_fn_t rpc__load_auth _DCE_PROTOTYPE_ ((
        rpc_authn_protocol_id_elt_p_t   /*auth_protocol*/,
        unsigned32                      * /*st*/
    ));

#ifdef __cplusplus
}
#endif


#endif /* _COMINITP_H */
