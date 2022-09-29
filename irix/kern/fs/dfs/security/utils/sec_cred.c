/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_cred.c,v 65.7 1998/04/01 14:17:11 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE). See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_cred.c,v $
 * Revision 65.7  1998/04/01 14:17:11  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added type cast to idl_void_p_t for pointer argument in calls to
 * 	sec_id_get_anonymous_epac().
 *
 * Revision 65.6  1998/03/23  17:32:07  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/01/20 17:29:11  lmc
 * If we are compiling for the kernel, don't pick up stdio.h or stdlib.h.  These
 * pick up conflicting definitions for things like printf.
 *
 * Revision 65.4  1998/01/07  18:14:25  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/12/23  17:26:14  lmc
 * ifdef the include for stdio.h to be only user space so that there is
 * no conflict between the sprintf, vprintf, etc between stdio.h and system.h.
 * Also added a type cast for a MALLOC.
 *
 * Revision 65.2  1997/11/06  20:19:09  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:48:33  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.1  1996/10/03  15:14:41  arvind
 * 	Deal with compiler warnings.
 * 	[1996/09/16  23:32 UTC  sommerfeld  /main/sommerfeld_pk_kdc_1/1]
 *
 * Revision 1.1.4.2  1996/03/11  01:42:54  marty
 * 	Update OSF copyright years
 * 	[1996/03/10  19:14:05  marty]
 * 
 * Revision 1.1.4.1  1995/12/08  18:02:54  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  17:23:07  root]
 * 
 * Revision 1.1.2.6  1994/09/07  15:28:35  burati
 * 	CR11909 Skip over confidential byte ERAs in sec_cred_get_extended_attrs
 * 	[1994/09/06  23:44:54  burati]
 * 
 * Revision 1.1.2.5  1994/08/09  17:32:51  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/08  22:02:01  burati]
 * 
 * Revision 1.1.2.4  1994/08/04  16:15:02  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/28  11:41:17  mdf]
 * 
 * Revision 1.1.2.3  1994/07/15  15:03:12  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/14  17:24:19  mdf]
 * 
 * Revision 1.1.2.2  1994/06/02  21:17:59  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:21:02  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, AGAIN!
 * 
 * 	hp_sec_to_osf_3 drop
 * 	[1994/05/18  12:17:37  mdf]
 * 
 * 	HP revision /main/ODESSA_2/2  1994/04/26  19:27 UTC  cuti
 * 	Finish up sec_cred's interface.
 * 
 * 	HP revision /main/ODESSA_2/cuti_authz_name/1  1994/04/26  19:04 UTC  cuti
 * 	To merge ODESSA_2 main line work.
 * 
 * 	HP revision /main/ODESSA_2/sec_cred_authz_name/1  1994/02/28  20:50 UTC  greg
 * 	checked in for safety: does not compile
 * 
 * 	HP revision /main/ODESSA_2/1  1994/01/26  20:03  root
 * 	     Move ODESSA stake from kk_beta1 to kk_final
 * 
 * $EndLog$
 */

/* sec_cred.c
 * Module for retrieval of Privilege Attribute information from Extended PACs
 *
 * Copyright (c) Hewlett-Packard Company 1993
 * Unpublished work. All Rights Reserved.
 */

#if defined(SGIMIPS) && !defined(_KERNEL)
#include <stdio.h>
#endif
#include <fcntl.h>
#if defined(SGIMIPS) && !defined(_KERNEL)
#include <stdlib.h>
#endif
#include <macros.h>
#include <dce/sec_cred.h>
#include <sec_cred_internal.h>
#include <restrictions.h>
#include <dce/idlbase.h>
#include <dce/secsts.h>
#include <sec_encode.h>

pthread_once_t   cred_mutex_once = pthread_once_init;
boolean32        cred_mutex_inited = false;
pthread_mutex_t  cred_mutex;

/*
 * c r e a t e _ c r e d _ m u t e x
 */
void create_cred_mutex()
{
   if (!cred_mutex_inited) {
        pthread_mutex_init(&cred_mutex, pthread_mutexattr_default);
        cred_mutex_inited = true;
    }
}


boolean32 check_authz_compliance (
    sec__cred_handle_t     *chp,
    unsigned32             authz_svc,
    error_status_t         *error_status
)
{

    if (chp->auth_info.authz_svc == authz_svc) 
        CLEAR_STATUS(error_status);
    else
        SET_STATUS(error_status,  sec_cred_s_authz_cannot_comply);
 
    return (STATUS_OK(error_status));
} 

/* sec_cred_is_authenticated
 *
 * This function is used to extract the initiator's privilege attributes from
 * the RPC runtime.
 * Semantics of arguments
 *   callers_identity - specifies the identity of the server's RPC client as
 *                      obtained from the RPC runtime.
 *   error_status -     error status.
 *
 * Return Values
 *   true  - crdentials are authenticated
 *   false - credentials are not authenticated
 */
boolean32 sec_cred_is_authenticated (
    rpc_authz_cred_handle_t  callers_identity,
    error_status_t	     *error_status
)
{
    sec__cred_handle_t     *chp;
    boolean32              authenticated = false;

    CLEAR_STATUS(error_status);
    
    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(&callers_identity, error_status);
	if (STATUS_OK(error_status)) {
            authenticated = chp->auth_info.authenticated;
	}
    } UNLOCK_CRED_KERNEL;
    
    return authenticated;
}
 
/*
 * Macro to assign through a pointer iff the pointer is non-NULL.  Note
 * that we depend on the fact that "val" is not evaluated if "ptr" is
 * NULL (i.e., this must be a macro, not a function).
 */

#define ASSIGN(ptr, val) \
( \
    (ptr) != NULL ? *(ptr) = (val) : 0 \
)

/* sec_cred_inquire_auth_service_info
 *
 * returns info regarding the authentication/authorization
 * services used in an authenticated RPC.
 *
 * Semantics of arguments
 *   callers_identity - specifies the identity of the server's  client as
 *                      obtained from the RPC runtime.
 *   server_princ_name - Server name that caller authenticated to.
 *   authn_svc - the authentication service  
 *   authz_svc - the authorization service
 *   
 *   error_status -     error status.
 */

void  sec_cred_inq_auth_service_info (
    /*[in]*/ rpc_authz_cred_handle_t callers_identity,
    /*[out]*/ unsigned_char_p_t      *server_princ_name,
    /*[out]*/ unsigned32             *authn_svc,
    /*[out]*/ unsigned32             *authz_svc,
    /*[out]*/ error_status_t	     *error_status
)
{
    sec__cred_handle_t     *chp;

    CLEAR_STATUS(error_status);

    LOCK_CRED_KERNEL {
        chp = sec__cred_check_handle(&callers_identity, error_status);
        if (STATUS_OK(error_status)) {
            if (server_princ_name != NULL) {
                if (chp->auth_info.server_princ_name == NULL) {
                    ASSIGN(server_princ_name,   NULL);
                }
                else {
                    ASSIGN(server_princ_name, chp->auth_info.server_princ_name);
                }
            }

            ASSIGN(authn_svc,           chp->auth_info.authn_svc);
            ASSIGN(authz_svc,           chp->auth_info.authz_svc);
        }
    } UNLOCK_CRED_KERNEL;

}

#ifndef KERNEL
/* sec_cred_get_client_princ_name
 *
 * This function is used to extract the principal name of a server's
 * RPC client, if the authorization service can provide it.
 * If not, a sec_cred_s_authz_cannot_comply status is returned
 *
 * Semantics of arguments
 *   callers_identity - specifies the identity of the server's RPC client as
 *                      obtained from the RPC runtime.
 *   error_status -     error status.
 */
void sec_cred_get_client_princ_name (
    /*[in]*/ rpc_authz_cred_handle_t callers_identity,
    /*[out]*/ unsigned_char_p_t      *client_princ_name,
    /*[out]*/  error_status_t	     *error_status
)
{
    sec__cred_handle_t     *chp;

    CLEAR_STATUS(error_status);

    LOCK_CRED_KERNEL {
        chp = sec__cred_check_handle(&callers_identity, error_status);
        if (STATUS_OK(error_status)
           && check_authz_compliance(chp, rpc_c_authz_name, error_status)) {
           if (client_princ_name != NULL) {
               if (chp->auth_info.authz_info.client_princ_name == NULL) {
                   ASSIGN(client_princ_name, NULL);
               }
               else {
                   ASSIGN(client_princ_name, chp->auth_info.authz_info.client_princ_name);
               } 
           }
        }
     }  UNLOCK_CRED_KERNEL;

}
#endif /*KERNEL*/


/* sec_cred_get_initiator
 *
 * This function is used to extract the initiator's privilege attributes from
 * the RPC runtime.
 * Semantics of arguments
 *   callers_identity - specifies the identity of the server's RPC client as
 *                      obtained from the RPC runtime.
 *   error_status -     error status.
 */
sec_cred_pa_handle_t sec_cred_get_initiator(
    rpc_authz_cred_handle_t      callers_identity,
    error_status_t	         *error_status
)
{
    sec__cred_pa_handle_t  pah=NULL;
    sec__cred_handle_t     *chp;

    CLEAR_STATUS(error_status);
    
    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(&callers_identity, error_status);
	if (STATUS_OK(error_status) 
	&& check_authz_compliance(chp, rpc_c_authz_dce, error_status)) {
	    sec__cred_decode_epac(chp, 0, error_status);
	    if (STATUS_OK(error_status)) {
                pah = &chp->epacs[0];

		if (pah->epac.target_restrictions.restrictions) { /* Non_NULL */
		    if (!sec_id_any_other_rstr_in(&pah->epac.target_restrictions)) {
			/* No any_other rstr, so mask out pa */
#ifdef SGIMIPS
			sec_id_get_anonymous_epac(FREE_PTR, (idl_void_p_t)&pah->epac, error_status);
#else
			sec_id_get_anonymous_epac(FREE_PTR, &pah->epac, error_status);
#endif
		    }
		}
            }
	}
    } UNLOCK_CRED_KERNEL;
    
    return (sec_cred_pa_handle_t) pah;
}

/* sec_cred_get_delegate
 *
 * This function is used to iterate through and extract the privilege
 * attributes of the delegates involved in this operation from the RPC runtime.
 * Semantics of arguments
 *   callers_identity - specifies the identity of the server's RPC client as 
 *                      obtained from the RPC runtime.
 *   cursor -           an input/output cursor used to iterate through the set 
 *                      of delegates involved in this operation.
 *   error_status -     error status.
 */
sec_cred_pa_handle_t sec_cred_get_delegate (
    rpc_authz_cred_handle_t callers_identity,
    sec_cred_cursor_t       *cursor,
    error_status_t          *error_status
)
{
    sec__cred_handle_t      *chp;
    sec__cred_int_cursor_t  *icp = (sec__cred_int_cursor_t *) *cursor;
    sec__cred_pa_handle_t   pah=NULL;

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(&callers_identity, error_status);
	if (STATUS_OK(error_status) 
	&& check_authz_compliance(chp, rpc_c_authz_dce, error_status)) {
	    sec__cred_check_dlg_cursor(icp, chp, error_status);
	    if (STATUS_OK(error_status)) {
	        sec__cred_decode_epac(chp, icp->next, error_status);
	        if (STATUS_OK(error_status)) {
	            pah = &chp->epacs[icp->next];
		    if (pah->epac.target_restrictions.restrictions) { /* Non_NULL */
			if (!sec_id_any_other_rstr_in(&pah->epac.target_restrictions)) {
			    /* No any_other rstr, so mask out pa */
#ifdef SGIMIPS
			    sec_id_get_anonymous_epac(FREE_PTR, (idl_void_p_t)&pah->epac, error_status);
#else
			    sec_id_get_anonymous_epac(FREE_PTR, &pah->epac, error_status);
#endif
			}
		    }

		    icp->next++;
                }
	    }
	}
    } UNLOCK_CRED_KERNEL

    return (sec_cred_pa_handle_t) pah;
}


extern void sec_cred_get_authz_session_info (
    rpc_authz_cred_handle_t callers_identity,            /* [in] */
    uuid_t                  *session_id,                 /* [out] */
    sec_timeval_sec_t       *session_expiration,         /* [out] */        
    error_status_t          *stp                         /* [out] */
)
{
    sec__cred_handle_t     *chp;

    CLEAR_STATUS(stp);

    LOCK_CRED_KERNEL {
        chp = sec__cred_check_handle(&callers_identity, stp);
        if (STATUS_OK(stp)
	    && check_authz_compliance(chp, rpc_c_authz_dce, stp)) {
	    if (uuid_is_nil(&chp->session_id, stp)) {
		*stp = sec_cred_s_authz_cannot_comply;
	    }
	    *session_id = chp->session_id;
	    *session_expiration = chp->session_expiration;
        }
     }  UNLOCK_CRED_KERNEL;

}



#ifndef KERNEL
/* sec_cred_get_v1_pac
 *
 * This function is used to extract a version 1 DCE PAC from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas - specifies the handle to a set of privilege attributes.
 *   error_status - error statuses.
 */
sec_id_pac_t *sec_cred_get_v1_pac (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec_id_pac_t            *pac_p = NULL;
    sec__cred_pa_handle_t   pah;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	    pac_p = (sec_id_pac_t *) &pah->v1_pac;
	}
    } UNLOCK_CRED_KERNEL ;
	
    return pac_p;
}
#endif /*KERNEL*/


/* sec_cred_get_pa_data
 * 
 * This function is used to extract identity information from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas - specifies the handle to a set of privilege attributes.
 *   error_status - error statuses.
 */
sec_id_pa_t *sec_cred_get_pa_data (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t  pah;
    sec_id_pa_t            *pap = NULL;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	    pap = (sec_id_pa_t *) &PRIVS(pah).pa;
	}
    } UNLOCK_CRED_KERNEL;

    return pap;
}


#ifndef KERNEL
/* sec_cred_get_extended_attrs
 * 
 * This function is used to extract extended attributes from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas - specifies the handle to a set of privilege attributes.
 *   cursor - an input/output cursor used to iterate through the set
 *            of extended attributes.
 *   attr -   the next available extended attribute (these attributes use the
 *            same syntax and are the same datatype as the extended registry
 *            attributes defined in DCE RFC 6.).
 *   error_status -  error statuses.
 */
void sec_cred_get_extended_attrs (
    sec_cred_pa_handle_t    callers_pas,
    sec_cred_attr_cursor_t  *cursor,
    sec_attr_t              *attr,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t  pah;
    sec__cred_int_cursor_t *icp = (sec__cred_int_cursor_t *) *cursor;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	    do {
		sec__cred_check_attr_cursor(icp, pah, error_status);
		if (STATUS_OK(error_status)) {
		    *attr = PRIVS(pah).attrs[icp->next];
		    icp->next++;
		}
	    } while ( STATUS_OK(error_status) &&
			(attr->attr_value.attr_encoding ==
			sec_attr_enc_confidential_bytes) );
	}
    } UNLOCK_CRED_KERNEL;
}
#endif /*KERNEL*/

/* sec_cred_get_delegation_type
 *
 * This function is used to extract the allowed delegation type from a
 * privilege attribute handle.
 * Semantics of arguments
 *   callers_pas -   specifies the handle to a set of privilege attributes.
 *   error_status -  error status.
 */
sec_id_delegation_type_t sec_cred_get_delegation_type (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t     pah;
    sec_id_delegation_type_t  deleg_type = 0;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	    deleg_type =  PRIVS(pah).deleg_type;
	}
    } UNLOCK_CRED_KERNEL;

    return deleg_type;
}


/* sec_cred_get_tgt_restrictions
 *
 * This function is used to extract target restrictions from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas -   specifies the handle to a set of privilege attributes.
 *   error_status -  error status.
 */
sec_id_restriction_set_t *sec_cred_get_tgt_restrictions (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t     pah;
    sec_id_restriction_set_t  *restrictions = NULL;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	   restrictions  =  &PRIVS(pah).target_restrictions;
	}
    } UNLOCK_CRED_KERNEL;

    return restrictions;
}


/* sec_cred_get_deleg_restrictions
 *
 * This function is used to extract delegate restrictions from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas -   specifies the handle to a set of privilege attributes.
 *   error_status -  error status.
 */
sec_id_restriction_set_t *sec_cred_get_deleg_restrictions (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t     pah;
    sec_id_restriction_set_t  *restrictions = NULL;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	   restrictions  =  &PRIVS(pah).deleg_restrictions;
	}
    } UNLOCK_CRED_KERNEL;

    return restrictions;
}


/* sec_cred_get_opt_restrictions
 *
 * This function is used to extract optional restrictions from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas -   specifies the handle to a set of privilege attributes.
 *   error_status -  error status.
 */
sec_id_opt_req_t *sec_cred_get_opt_restrictions (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t  pah;
    sec_id_opt_req_t       *restrictions = NULL;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	   restrictions  =  &PRIVS(pah).opt_restrictions;
	}
    } UNLOCK_CRED_KERNEL;

    return restrictions;
}


/* sec_cred_get_req_restrictions
 * 
 * This function is used to extract required restrictions from a privilege
 * attribute handle.
 * Semantics of arguments
 *   callers_pas -   specifies the handle to a set of privilege attributes.
 *   error_status -  error status.
 */
sec_id_opt_req_t *sec_cred_get_req_restrictions (
    sec_cred_pa_handle_t    callers_pas,
    error_status_t          *error_status
)
{
    sec__cred_pa_handle_t  pah;
    sec_id_opt_req_t       *restrictions = NULL;

    LOCK_CRED_KERNEL {
	pah = sec__cred_check_pa_handle(callers_pas, error_status);
	if (STATUS_OK(error_status)) {
	   restrictions  =  &PRIVS(pah).req_restrictions;
	}
    } UNLOCK_CRED_KERNEL;

    return restrictions;

}


/* 
 * Constructor/Destructor functions for types specific to the 
 * sec_cred module (handles)
 */


/* sec_cred_initialize_cursor
 *
 * This function is used to initialize a sec_cred_cursor_t for use in calls to
 * the iterative routine sec_cred_get_delegate.
 * Semantics of arguments
 *   cursor - an input/output cursor used to iterate through the list of
 *            delegates.
 *   error_status - error status.
 */
void sec_cred_initialize_cursor (
    sec_cred_cursor_t       *cursor,
    error_status_t          *error_status
)
{
    sec__cred_int_cursor_t  *icp;

    icp = (sec__cred_int_cursor_t *) MALLOC(sizeof(sec__cred_int_cursor_t));
    if (icp == NULL) {
	SET_STATUS(error_status, sec_s_no_memory);
	*cursor = NULL;
	return;
    }

    CLEAR_STATUS(error_status);
    icp->cursor_type = sec__cred_cursor_type_dlg;
    icp->next = 1;
    *cursor = (sec__cred_int_cursor_t *) icp;
}

void sec_cred_free_cursor (
    sec_cred_cursor_t  *cursor,
    error_status_t     *error_status
)
{
    CLEAR_STATUS(error_status);

    if (cursor == NULL)
	return;
    
    LOCK_CRED_KERNEL {
	if (cursor != NULL && *cursor != NULL) {
	    FREE((char *) *cursor);
	    *cursor = NULL;
	}
    } UNLOCK_CRED_KERNEL;
}

#ifndef KERNEL
void sec_cred_initialize_attr_cursor (
    sec_cred_attr_cursor_t  *cursor,
    error_status_t          *error_status
)
{
    sec__cred_int_cursor_t  *icp;

    icp = MALLOC(sizeof(sec__cred_int_cursor_t));
    if (icp == NULL) {
	SET_STATUS(error_status, sec_s_no_memory);
	*cursor = NULL;
	return;
    }

    CLEAR_STATUS(error_status);
    icp->cursor_type = sec__cred_cursor_type_attr;
    icp->next = 0;
    *cursor = (sec__cred_int_cursor_t *) icp;
}


void sec_cred_free_attr_cursor (
    sec_cred_attr_cursor_t  *cursor,
    error_status_t          *error_status
)
{
   
    CLEAR_STATUS(error_status);

    if (cursor == NULL)
	return;

    LOCK_CRED_KERNEL {
	if (cursor != NULL && *cursor != NULL) {
		FREE((char *) *cursor);
		*cursor = NULL;
	    }
    } UNLOCK_CRED_KERNEL;
}
#endif /*!KERNEL*/

void sec_cred_free_pa_handle (
    sec_cred_pa_handle_t *pa_handle,
    error_status_t      *error_status
)
{
    /*
     * basically a no-op since, in the current implementation
     * there is no internal structure associated with a pa_handle.
     * just make it NULL.
     */ 
    if (pa_handle != NULL) {
	*pa_handle = NULL;
    }

    CLEAR_STATUS(error_status);
}    

/* rock 
 */
#define ROCK_KEY_BASE 962578

struct rock_internal_struct;

typedef struct rock_internal_struct {
    uuid_t                        app_id;
    sec_cred_destruct_fn_t        destructor;
    unsigned32                    key;
    struct rock_internal_struct   *priv;
    struct rock_internal_struct   *next;
} rock_internal_struct_t, *rock_internal_struct_p_t;

rock_internal_struct_p_t rock_head = NULL;
rock_internal_struct_t  rocks;
static unsigned32 rock_key = ROCK_KEY_BASE;


/* sec_cred_get_key
 * 
 * Return a unique integer key used by an application server to associate 
 * application-specific data with an rpc_authz_cred_handle_t.
 * 
 * Semantics of arguments 
 * 	app_id - application's subsystem uuid.  If this application
 *               subsystem uuid is already associated with an
 *               existing key, that existing key is returned.
 *               Otherwise a new key is created and associated
 *               with this application subsystem uuid.
 *      destructor_fn - A destructor function that is called to cleanup
 *			the data associated with the returned key.  May
 * 			be NULL, if no destructive action is required.
 *	error_status - sec_s_no_mem
 *
 * Return Values
 * 	A newly created unique cred key or previously used unique key.
 *
 */


unsigned32 sec_cred_get_key (
    uuid_t			*app_id,
    sec_cred_destruct_fn_t      destructor_fn,
    error_status_t	        *error_status
)
{
    error_status_t dummy;
    unsigned32     key=0;

    CLEAR_STATUS(error_status);

    LOCK_CRED_KERNEL {
	if (!rock_head) {
	    rock_head = &rocks;

	    rocks.app_id = *app_id;
	    rocks.destructor = destructor_fn;
	    rocks.key = rock_key++;
	    key = rocks.key;
	    rocks.priv = NULL;
	    rocks.next = NULL;
	}
	else { 
            /* FAKE:One rock only.  
             * Real implementation need to walk through destructors.next chain
	     */
	    if (!uuid_equal(app_id, &rocks.app_id, &dummy)) {
		SET_STATUS(error_status, sec_cred_s_no_more_key);
	    }
	    else {
		key = rocks.key;
	    }
	}

    } UNLOCK_CRED_KERNEL;

    
    return(key);


}


/* sec_cred_set_app_private_data
 *
 * Associate an application-specific <key, data> pair with a 
 * credential handle.  A credential handle may contain only
 * one reference to a given key.  If the credential handle already
 * contans a reference to the specified key, the data associated
 * with that key is replaced with the specified app_private_data.  
 *
 * Semantics of arguments 
 *      callers_identity - the credential handle in which the 
 *                         <key, data> pair should be stored
 *      key - the cred key to be stored in the callers_identity
 *      app_private_data - the application-specific data associated with
 *                         the cred key
 *	error_status - sec_cred_s_invalid_auth_handle
 *                     sec_cred_s_key_not_found
 *                     sec_cred_s_invalid_key		       
 *                     sec_s_no_memory
 */


void sec_cred_set_app_private_data (
    rpc_authz_cred_handle_t	callers_identity,
    unsigned32			key,
    void			*app_private_data,
    error_status_t		*error_status
)
{
    sec__cred_handle_t *chp;

    CLEAR_STATUS(error_status);

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(&callers_identity, error_status);
	if (STATUS_OK(error_status)) {
	    /* FAKE: one rock only 
	     * Real implementation need to walk through rocks.next chain
	     */
	    if (key != rocks.key) {
		SET_STATUS(error_status, sec_cred_s_invalid_key);
	    }
	    else {
		if (chp->rocks == NULL) { /* no rocks yet */
#ifdef SGIMIPS
		    chp->rocks = (rock_struct_p_t)MALLOC(sizeof(rock_internal_struct_t));
#else
		    chp->rocks = MALLOC(sizeof(rock_internal_struct_t));
#endif
		    if (chp->rocks == NULL) {
			SET_STATUS(error_status, sec_s_no_memory);
		    }
		    else {
			chp->rocks->rock_key = key;
			chp->rocks->rock_data = app_private_data; 
			chp->rocks->next = NULL;
		    }
		}
		else {
		    if (key != chp->rocks->rock_key) {
			/* FAKE: only one rock allowed for now
			   If this happened, an internal error */
			SET_STATUS(error_status, sec_s_no_memory);
		    }
		    else { /* rock exists, overwrite data */
			/* destruct old data first */
			(*(rocks.destructor))(chp->rocks->rock_data);
			chp->rocks->rock_data = app_private_data;
		    }
		}
	    }
	}
    } UNLOCK_CRED_KERNEL;


}


/* sec_cred_get_app_private_data
 *
 * Retrieve an application-specific <key, data> pair from a  
 * credential handle.  
 *
 * Semantics of arguments
 *      callers_identity - the credential handle in which the 
 *                         <key, data> pair is stored
 *      key - the cred key of the data to be retrieved from the 
 *            callers_identity 
 *	app_private_data -  the application-specific data associated with
 * 			    the cred key.  If it's NULL, the data has
 *                          not been set yet.
 *      error_status - sec_cred_s_invalid_auth_handle
 *                     sec_cred_s_invalid_key (?)
 *                     sec_cred_s_key_not_found
 */


void sec_cred_get_app_private_data (
    rpc_authz_cred_handle_t	callers_identity,
    unsigned32			key,
    void			**app_private_data,
    error_status_t		*error_status
) 
{
    sec__cred_handle_t *chp;

    CLEAR_STATUS(error_status);

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(&callers_identity, error_status);
	if (STATUS_OK(error_status)) {
	    /* FAKE: one rock only
	     * For real implementation, need to walk through rocks.next chain
	     */
	    if ((chp->rocks == NULL) || (key != chp->rocks->rock_key)) {
		*app_private_data = NULL;
		SET_STATUS(error_status, sec_cred_s_key_not_found);
	    }
	    else {
		*app_private_data = chp->rocks->rock_data; 
	    }
	}
    } UNLOCK_CRED_KERNEL;

}


/*
 * Return a application destructor function.
 */

sec_cred_destruct_fn_t sec_cred_get_rock_destructor(
    unsigned32               rock_key      /* [in] */
)
{
    /* FAKE: one rock only for now.
     * For real implementation, need to walk through destructor.next chain
     */


    return(rocks.destructor);


}
