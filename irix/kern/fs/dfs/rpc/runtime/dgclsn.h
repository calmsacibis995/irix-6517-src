/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgclsn.h,v $
 * Revision 65.1  1997/10/20 19:16:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.512.2  1996/02/18  22:56:08  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:16  marty]
 *
 * Revision 1.1.512.1  1995/12/08  00:19:39  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:56  root]
 * 
 * Revision 1.1.510.1  1994/01/21  22:36:44  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:54  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:24:00  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:50  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:47:45  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:37  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/12  16:17:16  mishkin
 * 	- RPC_DG_CLSN_CHK_SBOOT -> dgclsn.c (as internal function).
 * 	[1992/05/08  12:42:20  mishkin]
 * 
 * Revision 1.1  1992/01/19  03:08:23  devrcs
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
**      dgclsn.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Client-oriented routines that run in the 
**  listener thread.
**
**
*/

#ifndef _DGCLSN_H
#define _DGCLSN_H	1

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


PRIVATE boolean rpc__dg_do_common_response _DCE_PROTOTYPE_ ((               
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_reject _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_fault _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_response _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_working _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_nocall _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

PRIVATE boolean rpc__dg_do_quack _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_ccall_p_t  /*ccall*/
    ));

#ifdef __cplusplus
}
#endif

#endif /* _DGCLSN_H */
