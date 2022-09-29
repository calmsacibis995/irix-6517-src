/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgslsn.h,v $
 * Revision 65.1  1997/10/20 19:16:59  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.525.2  1996/02/18  22:56:19  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:26  marty]
 *
 * Revision 1.1.525.1  1995/12/08  00:20:22  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:20  root]
 * 
 * Revision 1.1.523.1  1994/01/21  22:37:25  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:39  cbrooks]
 * 
 * Revision 1.1.4.4  1993/01/03  23:24:57  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:26  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  20:49:48  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:11  zeliff]
 * 
 * Revision 1.1.4.2  1992/10/13  20:55:25  weisman
 * 	      30-jul-92 magid    Rename rpc__dg_server_chk_and_set_sboot ->
 * 	                         rpc__dg_svr_chk_and_set_sboot
 * 	[1992/10/13  20:47:51  weisman]
 * 
 * Revision 1.1.2.2  1992/05/12  16:19:54  mishkin
 * 	RPC_DG_SLSN_CHK_AND_SET_SBOOT macro -> rpc__dg_server_chk_and_set_sboot function
 * 	[1992/05/08  12:49:53  mishkin]
 * 
 * Revision 1.1  1992/01/19  03:08:33  devrcs
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
**      dgslsn.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG protocol service routines.  Server-oriented routines that run in the 
**  listener thread.
**
**
*/

#ifndef _DGSLSN_H
#define _DGSLSN_H

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


PRIVATE boolean32 rpc__dg_svr_chk_and_set_sboot _DCE_PROTOTYPE_((
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_sock_pool_elt_p_t  /*sp*/
    ));

PRIVATE boolean rpc__dg_do_quit _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t  /*scall*/
    ));

PRIVATE boolean rpc__dg_do_ack _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t  /*scall*/
     ));

PRIVATE boolean rpc__dg_do_ping _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/,
        rpc_dg_scall_p_t  /*scall*/
    ));

PRIVATE boolean rpc__dg_do_request _DCE_PROTOTYPE_((
        rpc_dg_sock_pool_elt_p_t  /*sp*/,
        rpc_dg_recvq_elt_p_t  /*rqe*/
    ));

#endif /* _DGSLSN_H */
