/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comp.h,v $
 * Revision 65.2  1998/02/09 17:45:21  lmc
 * Add a cast in the macro RPC_AUTHN_IN_RANGE so that unsigned32 defined
 * parameters will have a meaningful comparison against 0.
 *
 * Revision 65.1  1997/10/20  19:16:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.503.2  1996/02/18  22:55:51  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:59  marty]
 *
 * Revision 1.1.503.1  1995/12/08  00:18:35  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:58:21  root]
 * 
 * Revision 1.1.501.1  1994/01/21  22:35:45  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:43  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:22:39  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:42  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:44:47  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:34:26  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:02:31  rsalz
 * 	 24-apr-92 nm        Add RPC_AUTHN_INQ_SUPPORTED_RPC_PROT, RPC_AUTHN_CHECK_SUPPORTED_RPC_PROT.
 * 	  6-mar-92 wh        DCE 1.0.1 merge.
 * 	 08-nov-91 mishkin   - move RPC_AUTHN_CHECK_SUPPORTED macro.
 * 	                     - add RPC_AUTHN_IN_RANGE.
 * 	 23-may-91 ko        Add RPC protocol epv point vector table
 * 	                     pointer to the auth prot ID table entry typedef.
 * 	                     Add accessor macros for auth protocol ID table:
 * 	                     - RPC_AUTHN_INQ_SUPPORTED
 * 	                     - RPC_AUTHN_INQ_EPV
 * 	                     - RPC_AUTHN_INQ_RPC_PROT_TBL
 * 	                     - RPC_AUTHN_INQ_RPC_PROT_EPV
 * 	[1992/05/01  16:50:11  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:08:51  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMP_H
#define _COMP_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      comp.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to the Common Communications 
**  Service component of the RPC runtime.
**
**
*/


/***********************************************************************/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

#include <comprot.h>    /* Externals for Protocol Services sub-component*/
#include <comnaf.h>     /* Externals for NAF Services sub-component     */
#include <comauth.h>    /* Externals for Auth. Services sub-component   */

/*
 * Accessor macros for the RPC Protocol Sequence ID table.
 */


#define RPC_PROTSEQ_INQ_SUPPORTED(id) \
       (boolean)rpc_g_protseq_id[id].supported
#define RPC_PROTSEQ_INQ_PROTSEQ_ID(id)      rpc_g_protseq_id[id].rpc_protseq_id
#define RPC_PROTSEQ_INQ_PROT_ID(id)         rpc_g_protseq_id[id].rpc_protocol_id
#define RPC_PROTSEQ_INQ_NAF_ID(id)          rpc_g_protseq_id[id].naf_id
#define RPC_PROTSEQ_INQ_PROTSEQ(id)         rpc_g_protseq_id[id].rpc_protseq
#define RPC_PROTSEQ_INQ_NET_IF_ID(id)       rpc_g_protseq_id[id].network_if_id


/***********************************************************************/
/*
 * R P C _ P R O T O C O L _ I D _ E L T _ T
 *
 * The RPC Protocol ID table element structure.  An element describes
 * a single RPC Protocol.
 * 
 * The fields are:
 *
 *      prot_init       The address of an initialization routine in the
 *                      Protocol Service that will be called by rpc__init.
 * 
 *      prot_fork_handler  The address of a routine to call to handle 
 *                      protocol specific, fork-related processing.
 *
 *      rpc_protocol_id A constant identifier for this RPC Protocol.
 *      
 *      call_epv        An entry point vector for the Call Services in
 *                      the Protocol Service.
 *      
 *      mgmt_epv        An entry point vector for the Management Services in 
 *                      the Protocol Service.
 *      
 *      binding_epv     An entry point vector for the Binding Services
 *                      in the Protocol Service.
 *      
 *      network_epv     An entry point vector for the Network Services
 *                      in the Protocol Service.
 */
typedef struct
{
    rpc_prot_init_fn_t          prot_init;
    rpc_prot_fork_handler_fn_t  prot_fork_handler;
    rpc_protocol_id_t           rpc_protocol_id;
    rpc_prot_call_epv_t         *call_epv;
    rpc_prot_mgmt_epv_t         *mgmt_epv;
    rpc_prot_binding_epv_t      *binding_epv;
    rpc_prot_network_epv_t      *network_epv;
} rpc_protocol_id_elt_t, *rpc_protocol_id_elt_p_t;

/*
 * Accessor macros for the RPC Protocol ID table.
 */
#define RPC_PROTOCOL_INQ_SUPPORTED(id) \
      (rpc_g_protocol_id[id].prot_init != NULL)
#define RPC_PROTOCOL_INQ_CALL_EPV(id)       rpc_g_protocol_id[id].call_epv
#define RPC_PROTOCOL_INQ_MGMT_EPV(id)       rpc_g_protocol_id[id].mgmt_epv
#define RPC_PROTOCOL_INQ_BINDING_EPV(id)    rpc_g_protocol_id[id].binding_epv
#define RPC_PROTOCOL_INQ_NETWORK_EPV(id)    rpc_g_protocol_id[id].network_epv

/***********************************************************************/
/*
 * R P C _ G _ P R O T O C O L _ I D
 *
 * The RPC Protocol ID table.  Each RPC Protocol has an entry in this
 * table.  This table is index by RPC Protocol ID.
 *
 * Note that the ".rpc_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 */
EXTERNAL rpc_protocol_id_elt_t   rpc_g_protocol_id[];


/***********************************************************************/
/*
 * R P C _ N A F _ I D _ E L T _ T
 *
 * The Network Address Family ID table element structure.  An element
 * describes a single Network Address Family Extension.
 * 
 * The fields are:
 *
 *      naf_init        The address of an initialization routine in the
 *                      NAF Service that will be called by rpc__init
 *
 *      naf_id          A constant identifier for this NAF.
 *
 *      net_if_id       A constant identifier for the network interface
 *                      type used in the NAF initialization routine (when
 *                      determining if this NAF is supported).
 *      
 *      naf_epv         An entry point vector for the NAF Service.
 */
typedef struct
{
    rpc_naf_init_fn_t        naf_init;
    rpc_naf_id_t             naf_id;
    rpc_network_if_id_t      network_if_id;
    rpc_naf_epv_t            *epv;
} rpc_naf_id_elt_t, *rpc_naf_id_elt_p_t;

/*
 * Accessor macros for the Network Address Family ID table.
 */
#define RPC_NAF_INQ_SUPPORTED(id)       (rpc_g_naf_id[id].naf_init != NULL)
#define RPC_NAF_INQ_EPV(id)             rpc_g_naf_id[id].epv

/***********************************************************************/
/*
 * R P C _ G _ N A F _ I D
 *
 * The Network Address Family ID table.  This table is indexed by a NAF
 * ID.
 *
 * Each Network Address Family Extension has an entry in this table.
 * Note that this is a sparse table because it uses the Unix Address
 * Family ID's as NAF ID's.
 *
 * Note that the ".naf_id" field of i'th element in the table is always
 * "i".  While redundant, this is useful so that you can pass pointers
 * to individual table elements.
 */
EXTERNAL rpc_naf_id_elt_t   rpc_g_naf_id[];

/***********************************************************************/
/*
 * R P C _ A U T H N _ P R O T O C O L _ I D _ E L T _ T
 *
 * The RPC Authentication Protocol ID table element structure.
 * 
 * The fields are:
 *
 *      auth_init           The address of an initialization routine in the
 *                          Authentication Service that will be called by 
 *                          rpc__init.
 *
 *      authn_protocol_id   A constant identifier for this Authentication Service.
 *
 *      dce_rpc_authn_protocol_id_t
 *                          The value that goes into RPC protocol messages to
 *                          identify which authentication protocol is in use.
 *
 *      epv                 An entry point vector for the Authentication Service
 *                          functions.
 *      rpc_prot_epv_tbl    A table, indexed by RPC protocol ID,
 *                          containing that RPC protocol's specific
 *                          interface to the Authentication Service.
 *
 * Note that the ".auth_protocol_id" contains API values (see
 * "rpc_c_authn_..." constants in "rpc.idl").
 * "dce_rpc_authn_protocol_id_t" contains architectural values that appear
 * in network messages (see "dce_c_rpc_authn_protocol_..." constants in
 * "nbase.idl").
 */
typedef struct
{
    rpc_auth_init_fn_t      auth_init;
    rpc_authn_protocol_id_t authn_protocol_id;
    dce_rpc_authn_protocol_id_t 
                            dce_rpc_authn_protocol_id;
    rpc_auth_epv_t          *epv;
    rpc_auth_rpc_prot_epv_tbl_t
                            rpc_prot_epv_tbl;
} rpc_authn_protocol_id_elt_t, *rpc_authn_protocol_id_elt_p_t;


/***********************************************************************/
/*
 * R P C _ G _ A U T H N _ P R O T O C O L _ I D
 *
 * The RPC Authentication Protocol ID table.  This table is indexed by
 * auth protocol ID.
 *
 * Note that the ".auth_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 */
EXTERNAL rpc_authn_protocol_id_elt_t  rpc_g_authn_protocol_id[];

/*
 * Accessor macros for the Auth Protocol ID table.
 */
#define RPC_AUTHN_INQ_SUPPORTED(id)             (rpc_g_authn_protocol_id[id].auth_init != NULL)
#define RPC_AUTHN_INQ_EPV(id)                   rpc_g_authn_protocol_id[id].epv
#define RPC_AUTHN_INQ_RPC_PROT_EPV_TBL(id)      rpc_g_authn_protocol_id[id].rpc_prot_epv_tbl
#define RPC_AUTHN_INQ_RPC_PROT_EPV(id, rpc_id)  (RPC_AUTHN_INQ_RPC_PROT_EPV_TBL(id))[rpc_id]
#define RPC_AUTHN_INQ_SUPPORTED_RPC_PROT(id, rpc_id) (RPC_AUTHN_INQ_RPC_PROT_EPV(id, rpc_id) != NULL)

/*
 * Make sure the specified authentication protocol is valid.  If we're
 * asked for the "default" authentication protocol, use DCE private
 * authentication.  (Obviously this will have to be generalized in the
 * future.)
 */

#define RPC_AUTHN_IN_RANGE(id) \
    ((int)(id) >= 0 && (id) < RPC_C_AUTHN_PROTOCOL_ID_MAX)

#define RPC_AUTHN_CHECK_SUPPORTED(id, st) \
{ \
    if ((id) == rpc_c_authn_default) \
    { \
        id = rpc_c_authn_dce_private; \
    } \
    else if (! RPC_AUTHN_IN_RANGE(id) || ! RPC_AUTHN_INQ_SUPPORTED(id)) \
    { \
        *(st) = rpc_s_unknown_auth_protocol; \
        return; \
    } \
}

#define RPC_AUTHN_CHECK_SUPPORTED_RPC_PROT(id, rpc_id, st) \
{ \
    RPC_AUTHN_CHECK_SUPPORTED(id, st); \
    if (! RPC_AUTHN_INQ_SUPPORTED_RPC_PROT(id, rpc_id)) \
    { \
        *(st) = rpc_s_proto_unsupp_by_auth; \
        return; \
    } \
}



/***********************************************************************/
/*
 * R P C _ G _ S E R V E R _ P T H R E A D _ A T T R
 *
 * A pthread attribute for server thread creation.
 * Initialized by rpc_init().
 */
EXTERNAL pthread_attr_t     rpc_g_server_pthread_attr;


/***********************************************************************/
/*
 * Macros for checking the validity of bindings.  Note that these live
 * here instead of "com.h" because they use RPC_PROTOCOL_INQ_SUPPORTED
 * which is defined here.
 */

/*
 * The following macro is used to determine the validity of a binding
 * handle passed into the runtime.  Along with the normal sanity checks,
 * it will also check whether the handle is being used for the first
 * time in the child of a fork.  In the case of a handle crossing a fork,
 * a protocol specific routine is called to clean up any dangling state
 * that might have been carried across the fork.  Any other failed checks
 * result in a bad status being set.
 */

#define RPC_BINDING_VALIDATE(binding_rep, st) \
{ \
    if ((binding_rep) == NULL || \
        (binding_rep)->protocol_id >= RPC_C_PROTOCOL_ID_MAX || \
        ! RPC_PROTOCOL_INQ_SUPPORTED((binding_rep)->protocol_id)) \
    { \
        *(st) = rpc_s_invalid_binding; \
    } \
    else if ((binding_rep)->fork_count != rpc_g_fork_count) \
    { \
        rpc__binding_cross_fork(binding_rep, st); \
    } \
    else \
        *(st) = rpc_s_ok; \
} 

/*
 * The following macros are for use by callers that want to verify
 * that, along with being valid, the handle in question is of the
 * right type.  Note that the check for the correct type is only
 * done if the sanity checks carried out by the VALIDATE macro pass.
 */
#define RPC_BINDING_VALIDATE_SERVER(binding_rep, st) \
{ \
    RPC_BINDING_VALIDATE(binding_rep, st) \
    if (*(st) == rpc_s_ok && RPC_BINDING_IS_CLIENT(binding_rep)) \
        *(st) = rpc_s_wrong_kind_of_binding; \
}

#define RPC_BINDING_VALIDATE_CLIENT(binding_rep, st) \
{ \
    RPC_BINDING_VALIDATE(binding_rep, st) \
    if (*(st) == rpc_s_ok && RPC_BINDING_IS_SERVER(binding_rep)) \
        *(st) = rpc_s_wrong_kind_of_binding; \
}

/***********************************************************************/
/*
 * Prototypes for Common Communications Services routines that are used
 * across sub-components of the service.
 */

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void rpc__if_inq_endpoint _DCE_PROTOTYPE_ ((
        rpc_if_rep_p_t          /*ifspec*/,
        rpc_protseq_id_t        /*protseq_id*/,
        unsigned_char_t         ** /*endpoint*/,
        unsigned32              * /*status*/
    ));

#ifdef __cplusplus
}
#endif


#endif /* _COMP_H */
