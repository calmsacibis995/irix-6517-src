/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgrq.h,v $
 * Revision 65.1  1997/10/20 19:16:58  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.517.2  1996/02/18  22:56:14  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:22  marty]
 *
 * Revision 1.1.517.1  1995/12/08  00:20:10  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:12  root]
 * 
 * Revision 1.1.515.2  1994/05/27  15:36:29  tatsu_s
 * 	DG multi-buffer fragments.
 * 	[1994/04/29  19:17:05  tatsu_s]
 * 
 * Revision 1.1.515.1  1994/01/21  22:37:13  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:28  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:24:37  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:53  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:49:05  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:39  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/07  19:38:47  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:34:12  markar]
 * 
 * Revision 1.1  1992/01/19  03:06:28  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _DGRQ_H
#define _DGRQ_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dgrq.h
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

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/*
 * R P C _ D G _ R E C V Q _ E L T _ F R O M _ I O V E C T O R _ E L T
 *
 * Given an IO vector element, return the associated receive queue element.
 * This macro is used by internal callers of the "comm_receive/transceive"
 * path so that can get back a receieve queue element and look at the
 * header.  This macro depends on how "comm_receive" works.
 */

#define RPC_DG_RECVQ_ELT_FROM_IOVECTOR_ELT(iove) \
    ((rpc_dg_recvq_elt_p_t) (iove)->buff_addr)

/*
 * R P C _ D G _ R E C V Q _ R E I N I T
 *
 * Reinitialize a receive queue.
 */

#define RPC_DG_RECVQ_REINIT(rq) { \
    if ((rq)->head != NULL) rpc__dg_recvq_free(rq); \
    rpc__dg_recvq_init(rq);     /* !!! Maybe be smarter later -- this may be losing "history" */ \
}

/*
 * R P C _ D G _ R E C V Q _ I O V E C T O R _ S E T U P
 *
 * Setup the return iovector element.
 *
 * NOTE WELL that other logic depends on the fact that the "buff_addr"
 * field of iovector elements points to an "rpc_dg_recvq_elt_t" (rqe).
 * See comments by RPC_DG_RECVQ_ELT_FROM_IOVECTOR_ELT.
 */

#define RPC_DG_RECVQ_IOVECTOR_SETUP(data, rqe) { \
    (data)->buff_dealloc  = (rpc_buff_dealloc_fn_t) rpc__dg_pkt_free_rqe_for_stub; \
    (data)->buff_addr     = (byte_p_t) (rqe); \
    (data)->buff_len      = sizeof *(rqe); \
    (data)->data_addr     = (byte_p_t) &(rqe)->pkt->body; \
    (data)->data_len      = ((rqe)->hdrp != NULL) ? \
                                MIN((rqe)->hdrp->len, \
                                    (rqe)->pkt_len - RPC_C_DG_RAW_PKT_HDR_SIZE) : \
                                (rqe)->pkt_len; \
}

PRIVATE void rpc__dg_recvq_init _DCE_PROTOTYPE_(( rpc_dg_recvq_p_t  /*rq*/));


PRIVATE void rpc__dg_recvq_free _DCE_PROTOTYPE_(( rpc_dg_recvq_p_t  /*rq*/));

#endif /* _DGRQ_H */
