/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krbdgp.h,v $
 * Revision 65.2  1997/10/20 19:17:03  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.625.3  1996/02/18  22:56:27  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:32  marty]
 *
 * Revision 1.1.625.2  1995/12/08  00:20:53  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1995/03/06  17:20 UTC  tatsu_s
 * 	Submitted the local rpc security bypass.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b1/2  1995/02/27  22:05 UTC  tatsu_s
 * 	Added the trusted parameter to rpc__krb_dg_way_handler().
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b1/1  1995/02/22  22:32 UTC  tatsu_s
 * 	Local RPC Security Bypass.
 * 
 * 	HP revision /main/HPDCE02/1  1994/10/19  16:49 UTC  tatsu_s
 * 	Merged the fix for OT12540, 12609 & 10454.
 * 
 * 	HP revision /main/tatsu_s.fix_ot12540.b0/1  1994/10/12  16:16 UTC  tatsu_s
 * 	Fixed OT12540: Added first_frag argument to rpc__krb_dg_recv_ck().
 * 	[1995/12/07  23:59:40  root]
 * 
 * Revision 1.1.619.2  1994/01/28  23:09:36  burati
 * 	EPAC changes - out_data is now ** in way_handler args (dlg_bl1)
 * 	[1994/01/25  00:02:50  burati]
 * 
 * Revision 1.1.619.1  1994/01/21  22:37:58  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:16  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:26:08  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:18  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:52:13  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:04  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  16:37:30  rsalz
 * 	 04-feb-92 sommerfeld    locking reorganization.
 * 	[1992/05/01  16:29:21  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:51  devrcs
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
**  NAME
**
**      krbdgp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of types private to the kerberos-datagram glue module.
**
**
*/

#ifndef _KRBDGP_H
#define _KRBDGP_H 1

#define NCK_NEED_MARSHALLING	1

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

#include <dg.h>
#include <krbp.h>
#include <dce/conv.h>
    
#ifndef __GNUC__
#include <netinet/in.h>
#endif

/*
 * For various reasons, it's painful to get at the NDR tag of the
 * underlying data, so we cheat and just encode it in big-endian order.
 */

#ifdef SGIMIPS
#define rpc_marshall_be_long_int(mp, bei) \
{       signed32 temp = htonl(bei);            \
        rpc_marshall_long_int (mp, temp);  \
}
#else
#define rpc_marshall_be_long_int(mp, bei) \
{       long temp = htonl(bei);            \
        rpc_marshall_long_int (mp, temp);  \
}
#endif /* SGIMIPS */
      
#define rpc_convert_be_long_int(mp, bei) \
{                                       \
    rpc_unmarshall_long_int(mp, bei);   \
    bei = ntohl(bei);                   \
}


/*
 * DG EPV routines.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rpc__krb_dg_pre_call _DCE_PROTOTYPE_((                         
        rpc_key_info_p_t               ,
        handle_t                        ,
        unsigned32                      *
    ));

rpc_key_info_p_t rpc__krb_dg_create _DCE_PROTOTYPE_((                         
        unsigned32                      * /*st*/
    ));

void rpc__krb_dg_encrypt _DCE_PROTOTYPE_((
        rpc_key_info_p_t               /*info*/,
        rpc_dg_xmitq_elt_p_t            ,
        unsigned32                      */*st*/
    ));

void rpc__krb_dg_pre_send _DCE_PROTOTYPE_((                         
        rpc_key_info_p_t                /*info*/,
        rpc_dg_xmitq_elt_p_t             /*pkt*/,
        rpc_dg_pkt_hdr_p_t               /*hdrp*/,
        rpc_socket_iovec_p_t             /*iov*/,
        int                              /*iovlen*/,
        pointer_t                        /*cksum*/,
        unsigned32                      * /*st*/
    ));

void rpc__krb_dg_recv_ck _DCE_PROTOTYPE_((                         
        rpc_key_info_p_t                 /*info*/,
        rpc_dg_recvq_elt_p_t             /*pkt*/,
        pointer_t                        /*cksum*/,
        boolean32                        /*first_frag*/,
        error_status_t                  * /*st*/
    ));

void rpc__krb_dg_who_are_you _DCE_PROTOTYPE_((                         
        rpc_key_info_p_t                 /*info*/,
        handle_t                        ,
        uuid_t                          *,
        unsigned32                      ,
        unsigned32                      *,
        uuid_t                          *,
        unsigned32                      *
    ));

void rpc__krb_dg_way_handler _DCE_PROTOTYPE_((
        rpc_key_info_p_t                 /*info*/,
        ndr_byte                        * /*in_data*/,
        signed32                         /*in_len*/,
        signed32                         /*out_max_len*/,
        ndr_byte                        **/*out_data*/,
        signed32                        * /*out_len*/,
        unsigned32                      * /*st*/
    ));

rpc_key_info_p_t rpc__krb_dg_new_key _DCE_PROTOTYPE_((
        rpc_auth_info_p_t                /*info*/
    ));

#ifdef __cplusplus
}
#endif

#endif /* _KRBDGP_H */
