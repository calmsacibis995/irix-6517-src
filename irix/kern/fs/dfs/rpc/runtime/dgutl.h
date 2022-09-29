/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgutl.h,v $
 * Revision 65.1  1997/10/20 19:17:00  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.329.2  1996/02/18  22:56:22  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:28  marty]
 *
 * Revision 1.1.329.1  1995/12/08  00:20:28  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:16 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  17:13 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:59:24  root]
 * 
 * Revision 1.1.327.2  1994/05/19  21:14:46  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:41:34  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:21:25  hopkins]
 * 
 * Revision 1.1.327.1  1994/01/21  22:37:34  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:42  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:25:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:42  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:50:07  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:27  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:33  devrcs
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
**      dgutl.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Utility routines for the NCA RPC datagram protocol implementation.
**
**
*/

#ifndef _DGUTL_H
#define _DGUTL_H

/* ========================================================================= */

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif


#ifndef RPC_DG_PLOG

#define RPC_DG_PLOG_RECVFROM_PKT(hdrp, bodyp)
#define RPC_DG_PLOG_SENDMSG_PKT(iov, iovlen)
#define RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iovlen, lossy_action)
#define rpc__dg_plog_pkt(hdrp, bodyp, recv, lossy_action)

#else

#define RPC_DG_PLOG_RECVFROM_PKT(hdrp, bodyp) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((hdrp), (bodyp), true, 0); \
    }

#define RPC_DG_PLOG_SENDMSG_PKT(iov, iovlen) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((rpc_dg_raw_pkt_hdr_p_t) (iov)[0].base,  \
                    (iovlen) < 2 ? NULL : (rpc_dg_pkt_body_p_t) (iov)[1].base,  \
                    false, 3); \
    }

#define RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iovlen, lossy_action) \
    { \
        if (RPC_DBG(rpc_es_dbg_dg_pktlog, 100)) \
            rpc__dg_plog_pkt((rpc_dg_raw_pkt_hdr_p_t) (iov)[0].base,  \
                    (iovlen) < 2 ? NULL : (rpc_dg_pkt_body_p_t) (iov)[1].base,  \
                    false, lossy_action); \
    }


PRIVATE void rpc__dg_plog_pkt _DCE_PROTOTYPE_((
        rpc_dg_raw_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_pkt_body_p_t  /*bodyp*/,
        boolean32  /*recv*/,
        unsigned32  /*lossy_action*/
    ));

PRIVATE void rpc__dg_plog_dump _DCE_PROTOTYPE_((
         /*void*/
    ));

#endif

/* ========================================================================= */

PRIVATE void rpc__dg_xmit_pkt _DCE_PROTOTYPE_((
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_socket_iovec_p_t  /*iov*/,
        int  /*iovlen*/,
        boolean * /*b*/
    ));

PRIVATE void rpc__dg_xmit_hdr_only_pkt _DCE_PROTOTYPE_((
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_dg_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_ptype_t  /*ptype*/
    ));

PRIVATE void rpc__dg_xmit_error_body_pkt _DCE_PROTOTYPE_((
        rpc_socket_t  /*sock*/,
        rpc_addr_p_t  /*addr*/,
        rpc_dg_pkt_hdr_p_t  /*hdrp*/,
        rpc_dg_ptype_t  /*ptype*/,
        unsigned32  /*errst*/
    ));

PRIVATE char *rpc__dg_act_seq_string _DCE_PROTOTYPE_((
        rpc_dg_pkt_hdr_p_t  /*hdrp*/
    ));

PRIVATE char *rpc__dg_pkt_name _DCE_PROTOTYPE_((
        rpc_dg_ptype_t  /*ptype*/
    ));

PRIVATE unsigned16 rpc__dg_uuid_hash _DCE_PROTOTYPE_((
        uuid_p_t  /*uuid*/
    ));

#endif /* _DGUTL_H */
