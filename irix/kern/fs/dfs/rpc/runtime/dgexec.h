/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgexec.h,v $
 * Revision 65.1  1997/10/20 19:16:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.304.2  1996/02/18  22:56:09  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:17  marty]
 *
 * Revision 1.1.304.1  1995/12/08  00:19:42  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:58  root]
 * 
 * Revision 1.1.302.1  1994/01/21  22:36:48  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:57  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:06  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:59  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:47:54  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:46  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:16  devrcs
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
**      dgexec.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  
**
**
*/

#ifndef _DGEXEC_H
#define _DGEXEC_H

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

PRIVATE void rpc__dg_execute_call    _DCE_PROTOTYPE_((
        pointer_t  /*scall_*/,
        boolean32  /*call_was_queued*/
    ));

/*
 * To implement backward compatibilty, declare a pointer to a routine
 * that can call into pre-v2 server stubs.  We declare this function
 * in this indirect way so that it is possible to build servers that
 * don't support backward compatibility (and thus save space).  The
 * compatibility code will only be linked into a server if the server
 * application code itself calls a compatibility function, most likely
 * rpc_$register.  rpc_$register is then responsible for initializing
 * this function pointer so that dg_execute_call can find the compatibilty
 * function.  In this way, libnck has no direct references to the
 * compatibilty code.
 */

typedef void (*rpc__dg_pre_v2_server_fn_t) _DCE_PROTOTYPE_ ((
        rpc_if_rep_p_t  /*ifspec*/,
        unsigned32  /*opnum*/,
        handle_t  /*h*/,
        rpc_call_handle_t  /*call_h*/,
        rpc_iovector_elt_p_t  /*iove_ins*/,
        ndr_format_t  /*ndr_format*/,
        rpc_v2_server_stub_epv_t  /*server_stub_epv*/,
        rpc_mgr_epv_t  /*mgr_epv*/,
        unsigned32 * /*st*/
    )); 
                                                               
#endif /* _DGEXEC_H */
