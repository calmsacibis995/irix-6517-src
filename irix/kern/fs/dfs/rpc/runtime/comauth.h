/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comauth.h,v $
 * Revision 65.1  1997/10/20 19:16:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.61.1  1996/06/04  21:54:22  arvind
 * 	u2u: Add proto for (*rpc_auth_inq_my_pname_tgt_fn_t) and
 * 	rpc__auth_inq_my_princ_name_tgt()
 * 	[1996/05/06  20:17 UTC  burati  /main/DCE_1.2/1]
 *
 * 	merge u2u work
 * 	[1996/04/29  20:20 UTC  burati  /main/mb_u2u/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:07 UTC  burati  /main/mb_mothra8/1]
 *
 * Revision 1.1.59.2  1996/02/18  22:55:41  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:14:51  marty]
 * 
 * Revision 1.1.59.1  1995/12/08  00:18:00  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:57:58  root]
 * 
 * Revision 1.1.57.1  1994/01/21  22:35:03  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:54:55  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:59:47  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:12  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:18:58  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:32:52  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:01:17  rsalz
 * 	 27-mar-92 nm         Fix signature of rpc_auth_release_identity_fn_t.
 * 	  5-mar-92 wh         DCE 1.0.1 merge.
 * 	 05-feb-92 sommerfeld explain stylistic inconsistancy.
 * 	 22-jan-92 sommerfeld add new operations to auth epv for:
 * 	                     - release key_info
 * 	                     - resolve, release auth_identity.
 * 	[1992/05/01  16:49:06  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:08:11  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COMAUTH_H
#define _COMAUTH_H	1
/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      comauth.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Generic interface to authentication services
**
**
*/

/***********************************************************************/
/*
 * Function pointer types of functions exported by an authentication
 * service.
 */

/*
 * The next four function pointer types correspond to the similarly
 * named functions in the API.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rpc_auth_bnd_set_auth_info_fn_t) _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */   /*server_princ_name*/,
        rpc_authn_level_t                   /* in  */   /*authn_level*/,
        rpc_auth_identity_handle_t          /* in  */   /*auth_identity*/,
        rpc_authz_protocol_id_t             /* in  */   /*authz_protocol*/,
        rpc_binding_handle_t                /* in  */   /*binding_h*/,   
        rpc_auth_info_p_t                   /* out*/   * /*auth_info*/,
        unsigned32                          /* out */   *st
    ));

typedef void (*rpc_auth_serv_reg_auth_info_fn_t) _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */   /*server_princ_name*/,
        rpc_auth_key_retrieval_fn_t         /* in  */   /*get_key_func*/,
        pointer_t                           /* in  */   /*arg*/,
        unsigned32                          /* out */   *st
    ));

typedef void (*rpc_auth_serv_reg_auth_u2u_fn_t) _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                   /* in  */   /*server_princ_name*/,
        rpc_auth_identity_handle_t          /* in  */   /*id_h*/,
        unsigned32                          /* out */   *st
    ));

typedef void (*rpc_mgmt_inq_def_authn_lvl_fn_t) _DCE_PROTOTYPE_ ((
        unsigned32                          /* out */   * /*authn_level*/,
        unsigned32                          /* out */   *st
    ));

/*
 * This function pointer type describes an authentication
 * procedure that frees the auth info memory and resources.
 */

typedef void (*rpc_auth_free_info_fn_t) _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t                   /* in/out */  * /*auth_info*/
    ));
/*
 * This function pointer type defines a procedure which frees key info
 * memory and resources.
 */

typedef void (*rpc_auth_free_key_fn_t) _DCE_PROTOTYPE_ ((
        rpc_key_info_p_t                    /* in/out */ * /*key_info*/
    ));

/*
 * These function pointer types resolve a passed-in "auth_identity"
 * into a reference to a "real" auth identity, and release the
 * reference.  Note that these return the error status in the return
 * value (instead of in an output argument) because this means that
 * the function pointer has the exact same signature as the routine in
 * the security component which implements this function.
 */
    
typedef error_status_t (*rpc_auth_resolve_identity_fn_t) _DCE_PROTOTYPE_ ((
        rpc_auth_identity_handle_t                 /* in */ /*in_identity*/,
        rpc_auth_identity_handle_t                 /* out */ *out_identity
    ));
    
typedef void (*rpc_auth_release_identity_fn_t) _DCE_PROTOTYPE_ ((
        rpc_auth_identity_handle_t                 /* in/out */ * /*identity*/
    ));
    
/*
 * This function pointer type describes an authentication service procedure
 * that returns the principal name (possibly just one of many) that server
 * is running as.
 */

typedef void (*rpc_auth_inq_my_princ_name_fn_t) _DCE_PROTOTYPE_ ((
        unsigned32                          /* in */    /*princ_name_size*/,
        unsigned_char_p_t                   /* out */   /*princ_name*/,
        unsigned32                          /* out */   * /*st*/
    ));

/*
 * This function pointer type describes an authentication service procedure
 * that returns the principal name (possibly just one of many) and tgt that
 * server is running as.
 */

typedef void (*rpc_auth_inq_my_pname_tgt_fn_t) _DCE_PROTOTYPE_ ((
        unsigned32                          /* in  */   /* princ_name_size */,
        unsigned32                          /* in  */   /* max_tgt_size    */,
        unsigned_char_p_t                   /* out */   /* princ_name      */,
        unsigned32                          /* out */ * /* tgt_size        */,
        unsigned_char_p_t                   /* out */   /* tgt_data        */,
        unsigned32                          /* out */ * /* st              */
    ));

/***********************************************************************/
/*
 * RPC authentication service API EPV.
 */

typedef struct
{                                   
    rpc_auth_bnd_set_auth_info_fn_t     binding_set_auth_info;
    rpc_auth_serv_reg_auth_info_fn_t    server_register_auth_info;
    rpc_mgmt_inq_def_authn_lvl_fn_t     mgmt_inq_dflt_auth_level;
    rpc_auth_inq_my_princ_name_fn_t     inq_my_princ_name;
    rpc_auth_free_info_fn_t             free_info;
    rpc_auth_free_key_fn_t              free_key;
    rpc_auth_resolve_identity_fn_t      resolve_id;
    rpc_auth_release_identity_fn_t      release_id;
    rpc_auth_serv_reg_auth_u2u_fn_t     server_register_auth_u2u;
    rpc_auth_inq_my_pname_tgt_fn_t      inq_my_princ_name_tgt;
} rpc_auth_epv_t, *rpc_auth_epv_p_t;

/***********************************************************************/

PRIVATE void rpc__auth_info_binding_release _DCE_PROTOTYPE_ ((
        rpc_binding_rep_p_t     binding_rep
    ));

PRIVATE void rpc__auth_inq_my_princ_name _DCE_PROTOTYPE_ ((
        unsigned32              /*dce_rpc_authn_protocol*/,
        unsigned32              /*princ_name_size*/,
        unsigned_char_p_t       /*princ_name*/,
        unsigned32              * /*status*/
    ));

PRIVATE void rpc__auth_inq_my_princ_name_tgt _DCE_PROTOTYPE_ ((
        unsigned32              /*dce_rpc_authn_protocol*/,
        unsigned32              /*princ_name_size*/,
        unsigned32              /*max_tgt_size*/,
        unsigned_char_p_t       /*princ_name*/,
        unsigned32              * /*tgt_length*/,
        unsigned_char_p_t       /*tgt_data*/,
        unsigned32              * /*status*/
    ));

PRIVATE void rpc__auth_info_cache_init _DCE_PROTOTYPE_ (( 
        unsigned32              * /*status*/
    ));
        
/***********************************************************************/

/*
 * Signature of the init routine provided.  Each authentication service
 * must return both an API and a common EPV.
 */

typedef void (*rpc_auth_init_fn_t) _DCE_PROTOTYPE_ ((
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    ));

/*
 * Declarations of the RPC authentication service init routines.
 */

void rpc__krb_init _DCE_PROTOTYPE_ ((
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    ));

void rpc__noauth_init _DCE_PROTOTYPE_ ((
        rpc_auth_epv_p_t            * /*auth_epv*/,
        rpc_auth_rpc_prot_epv_tbl_t * /*auth_rpc_prot_epv_tbl*/,
        unsigned32                  * /*status*/
    ));

void rpc__key_info_release _DCE_PROTOTYPE_ ((
    rpc_key_info_p_t *
));

void rpc__key_info_reference _DCE_PROTOTYPE_ ((
    rpc_key_info_p_t
));

#ifdef _cplusplus
}
#endif

#endif /*  _COMAUTH_H */
