/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comnaf.h,v $
 * Revision 65.1  1997/10/20 19:16:48  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.84.2  1996/02/18  22:55:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:57  marty]
 *
 * Revision 1.1.84.1  1995/12/08  00:18:26  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:31 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:15 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  17:01 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:58:15  root]
 * 
 * Revision 1.1.82.2  1994/05/27  15:35:54  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added void (*rpc_naf_inq_max_frag_size_fn_t)().
 * 	[1994/04/29  18:57:00  tatsu_s]
 * 
 * Revision 1.1.82.1  1994/01/21  22:35:32  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:34  cbrooks]
 * 
 * Revision 1.1.7.1  1993/09/28  21:58:11  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:35  weisman]
 * 
 * Revision 1.1.4.3  1993/01/03  23:22:22  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:15  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:44:21  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:59  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:02:11  rsalz
 * 	  5-mar-92 wh        DCE 1.0.1 merge.
 * 	 28-jan-92 ko        Add rpc_naf_desc_inq_peer_addr_fn_t.
 * 	[1992/05/01  16:49:55  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:06:09  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMNAF_H
#define _COMNAF_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      comnaf.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Generic interface to Network Address Family Extension Services
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/***********************************************************************/
/*
 * The Network Address Family EPV.
 */


typedef void (*rpc_naf_addr_alloc_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            rpc_protseq_id,
        rpc_naf_id_t                naf_id,
        unsigned_char_p_t           endpoint,
        unsigned_char_p_t           netaddr,
        unsigned_char_p_t           network_options,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_copy_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                src_rpc_addr,
        rpc_addr_p_t                *dst_rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_free_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_set_endpoint_fn_t)
    _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           endpoint,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_inq_endpoint_fn_t) 
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *endpoint,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_set_netaddr_fn_t)
    _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           netaddr,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_inq_netaddr_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *netaddr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_set_options_fn_t)
    _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           network_options,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_addr_inq_options_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                rpc_addr,
        unsigned_char_p_t           *network_options,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_desc_inq_addr_fn_t)
    _DCE_PROTOTYPE_ ((              
        rpc_protseq_id_t            protseq_id,
        rpc_socket_t                desc,
        rpc_addr_vector_p_t         *rpc_addr_vec,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_inq_max_tsdu_fn_t)
    _DCE_PROTOTYPE_ ((                           
        rpc_naf_id_t                naf_id,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tsdu,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_get_broadcast_fn_t)
    _DCE_PROTOTYPE_ ((                         
        rpc_naf_id_t                naf_id,
        rpc_protseq_id_t            protseq_id,
        rpc_addr_vector_p_t         *rpc_addrs,
        unsigned32                  *status
    ));

typedef boolean (*rpc_naf_addr_compare_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                addr1,
        rpc_addr_p_t                addr2,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_inq_pth_unfrg_tpdu_fn_t)
    _DCE_PROTOTYPE_ ((                     
        rpc_addr_p_t                rpc_addr,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tpdu,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_inq_loc_unfrg_tpdu_fn_t)
    _DCE_PROTOTYPE_ ((              
        rpc_naf_id_t                naf_id,
        rpc_network_if_id_t         iftype,
        rpc_network_protocol_id_t   protocol,
        unsigned32                  *max_tpdu,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_desc_inq_network_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_socket_t                desc,
        rpc_network_if_id_t         *socket_type,
        rpc_network_protocol_id_t   *protocol_id,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_set_pkt_nodelay_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_socket_t                desc,
        unsigned32                  *status
    ));

typedef boolean (*rpc_naf_is_connect_closed_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_socket_t                desc,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_twr_flrs_from_addr_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                rpc_addr,
        twr_p_t                     *lower_flrs,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_twr_flrs_to_addr_fn_t)
    _DCE_PROTOTYPE_ ((
        byte_p_t                    tower_octet_string,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_desc_inq_peer_addr_fn_t)
    _DCE_PROTOTYPE_ ((              
        rpc_protseq_id_t            protseq_id,
        rpc_socket_t                desc,
        rpc_addr_p_t                *rpc_addr,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_set_port_restriction_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            protseq_id,
        unsigned32                  n_elements,
        unsigned_char_p_t           *first_port_name_list,
        unsigned_char_p_t           *last_port_name_list,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_get_next_restricted_port_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            protseq_id,
        unsigned_char_p_t           *port_name,
        unsigned32                  *status
    ));

typedef void (*rpc_naf_inq_max_frag_size_fn_t)
    _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                rpc_addr,
        unsigned32                  *max_frag_size,
        unsigned32                  *status
    ));


typedef struct
{
    rpc_naf_addr_alloc_fn_t         naf_addr_alloc;
    rpc_naf_addr_copy_fn_t          naf_addr_copy;
    rpc_naf_addr_free_fn_t          naf_addr_free;
    rpc_naf_addr_set_endpoint_fn_t  naf_addr_set_endpoint;
    rpc_naf_addr_inq_endpoint_fn_t  naf_addr_inq_endpoint;
    rpc_naf_addr_set_netaddr_fn_t   naf_addr_set_netaddr;
    rpc_naf_addr_inq_netaddr_fn_t   naf_addr_inq_netaddr;
    rpc_naf_addr_set_options_fn_t   naf_addr_set_options;
    rpc_naf_addr_inq_options_fn_t   naf_addr_inq_options;
    rpc_naf_desc_inq_addr_fn_t      naf_desc_inq_addr;
    rpc_naf_desc_inq_network_fn_t   naf_desc_inq_network;
    rpc_naf_inq_max_tsdu_fn_t       naf_inq_max_tsdu;
    rpc_naf_get_broadcast_fn_t      naf_get_broadcast;
    rpc_naf_addr_compare_fn_t       naf_addr_compare;
    rpc_naf_inq_pth_unfrg_tpdu_fn_t naf_inq_max_pth_unfrg_tpdu;
    rpc_naf_inq_loc_unfrg_tpdu_fn_t naf_inq_max_loc_unfrg_tpdu;
    rpc_naf_set_pkt_nodelay_fn_t    naf_set_pkt_nodelay;
    rpc_naf_is_connect_closed_fn_t  naf_is_connect_closed;
    rpc_naf_twr_flrs_from_addr_fn_t naf_tower_flrs_from_addr;
    rpc_naf_twr_flrs_to_addr_fn_t   naf_tower_flrs_to_addr;
    rpc_naf_desc_inq_peer_addr_fn_t naf_desc_inq_peer_addr;
    rpc_naf_set_port_restriction_fn_t naf_set_port_restriction;
    rpc_naf_get_next_restricted_port_fn_t naf_get_next_restricted_port;
    rpc_naf_inq_max_frag_size_fn_t  naf_inq_max_frag_size;
} rpc_naf_epv_t, *rpc_naf_epv_p_t;


/***********************************************************************/
/*
 * Signature of the init routine provided.
 */
typedef void (*rpc_naf_init_fn_t) _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));

/*
 * Declarations of the Network Address Family Extension Service init
 * routines. 
 */
PRIVATE void rpc__unix_init _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));

PRIVATE void rpc__ip_init _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));

PRIVATE void rpc__dnet_init _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));

PRIVATE void rpc__osi_init _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));

PRIVATE void rpc__dds_init _DCE_PROTOTYPE_ ((
        rpc_naf_epv_p_t              * /*naf_epv*/ ,
        unsigned32                   * /*status*/
    ));
      
PRIVATE void rpc__naf_set_port_restriction _DCE_PROTOTYPE_ (( 
    rpc_protseq_id_t,
    unsigned32,
    unsigned_char_p_t *,
    unsigned_char_p_t *,
    unsigned32 *
));

PRIVATE void rpc__naf_get_next_restricted_port  _DCE_PROTOTYPE_ (( 
    rpc_protseq_id_t,
    unsigned_char_p_t *,
    unsigned32 *
));


#endif /* _COMNAF_H */
