/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_cred_internal.c,v 65.6 1998/04/01 14:17:12 gwehrman Exp $";
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
 * $Log: sec_cred_internal.c,v $
 * Revision 65.6  1998/04/01 14:17:12  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed extern declarations for FREE() and MALLOC().
 *
 * Revision 65.5  1998/03/23  17:31:51  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 18:14:21  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  20:19:05  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:48:34  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.4.2  1996/03/11  01:42:56  marty
 * 	Update OSF copyright years
 * 	[1996/03/10  19:14:06  marty]
 *
 * Revision 1.1.4.1  1995/12/08  18:03:00  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/03/27  19:34 UTC  cuti
 * 	CHFts14466 (ot12799) bug fix
 * 
 * 	HP revision /main/cuti_mothra_bug/1  1995/03/23  18:09 UTC  cuti
 * 	OT12799 (DTS 14466): Initialize the magic cookie of cred handle to 0
 * 	[1995/12/08  17:23:09  root]
 * 
 * Revision 1.1.2.7  1994/08/18  20:25:48  greg
 * 	Pass the malloc_shim() routine instead of native malloc
 * 	in calls to routines that expect an allocator whose
 * 	parameter is of type idl_size_t.  The malloc_shim()
 * 	routine will use the appropriate user-space or kernel
 * 	allocator.
 * 	[1994/08/17  13:53:28  greg]
 * 
 * 	This can simply be outdated since, after the merge, if matched
 * 	exactly the version on the osf mainline.
 * 	[1994/08/17  13:34:44  greg]
 * 
 * Revision 1.1.2.6  1994/08/09  17:32:53  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/08  22:03:08  burati]
 * 
 * Revision 1.1.2.5  1994/08/04  16:15:03  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/28  17:17:05  mdf]
 * 
 * Revision 1.1.2.4  1994/07/15  15:03:14  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/14  17:24:26  mdf]
 * 
 * Revision 1.1.2.3  1994/06/02  21:18:01  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:21:05  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, AGAIN!
 * 
 * 	hp_sec_to_osf_3 drop
 * 	[1994/05/18  12:17:39  mdf]
 * 
 * 	HP revision /main/ODESSA_2/9  1994/05/11  20:48 UTC  greg
 * 	apply delegation token memory leak fix  from ODESSA_1_1_FIX branch
 * 
 * 	HP revision /main/ODESSA_2/8  1994/05/06  14:36 UTC  greg
 * 	merge impersonation work from .../greg_dlg_imp
 * 
 * 	HP revision /main/ODESSA_2/greg_dlg_imp/1  1994/05/03  21:13 UTC  greg
 * 	add sec__cred_delegation_type_permitted().  Internally at least, this will
 * 	enable the caller to determine the delegation type permitted for an
 * 	entire delegation chain without iterating over all delegates to arrive
 * 	at the last one.  We might consider exporting this through the sec_cred
 * 	API proper.
 * 
 * 	HP revision /main/ODESSA_2/7  1994/05/03  18:35 UTC  mdf
 * 	submit for Mike B.
 * 
 * 	HP revision /main/ODESSA_2/mdf_odessa_era_bl35/1  1994/05/03  18:20 UTC  mdf
 * 	Check for NULL and make use of something... See Mike Burati.
 * 
 * 	HP revision /main/ODESSA_2/6  1994/04/26  20:09 UTC  cuti
 * 	To merge authz_name work.
 * 
 * 	HP revision /main/ODESSA_2/cuti_authz_name/2  1994/04/26  19:03 UTC  cuti
 * 	To merge ODESSA_2 main line work.
 * 
 * 	HP revision /main/ODESSA_2/cuti_authz_name/1  1994/04/05  17:26 UTC  cuti
 * 	Continue Greg's work on authz_name work.
 * 
 * 	HP revision /main/ODESSA_2/5  1994/03/29  20:30 UTC  greg
 * 	merge from dlg_bl2
 * 
 * 	HP revision /main/ODESSA_2/dlg_bl2/6  1994/03/24  21:11 UTC  burati
 * 	More delegation token work
 * 
 * 	HP revision /main/ODESSA_2/dlg_bl2/5  1994/03/22  22:44 UTC  burati
 * 	Add internal function to get delegation token
 * 
 * 	HP revision /main/ODESSA_2/4  1994/03/16  22:25 UTC  burati
 * 	merge from dlg_bl2
 * 
 * 	HP revision /main/ODESSA_2/dlg_bl2/4  1994/03/16  21:29 UTC  greg
 * 	merge from greg_rpriv_dlg_bl2 (sec_encode interface changes)
 * 
 * 	HP revision /main/ODESSA_2/dlg_bl2/greg_rpriv_dlg_bl2/1  1994/03/16  20:49 UTC  greg
 * 	add memory management function parameters to sec_encode interfaces
 * 
 * 	HP revision /main/ODESSA_2/3  1994/03/02  03:06 UTC  burati
 * 	     Don't decode epac set if already done.
 * 
 * 	HP revision /main/ODESSA_2/2  1994/03/01  22:42 UTC  burati
 * 	     merge from dlg_bl2 to pick up necessary fix for ODSS1.0
 * 
 * 	HP revision /main/ODESSA_2/dlg_bl2/1  1994/03/01  22:40 UTC  burati
 * 	     Fix it.
 * 
 * 	HP revision /main/ODESSA_2/sec_cred_authz_name/1  1994/02/28  20:51 UTC  greg
 * 	checked in for safety: does not compile
 * 	HP revision /main/ODESSA_2/1  1994/01/26  20:03  root
 * 	     Move ODESSA stake from kk_beta1 to kk_final
 * 
 * $EndLog$
 */

/* sec_cred_internal.c
 * Internal support routines for public sec_cred module.
 *
 * Copyright (c) Hewlett-Packard Company 1993
 * Unpublished work. All Rights Reserved.
 */

#if !defined(KERNEL) || !defined(SGIMIPS)
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#endif
#include <macros.h>
#ifdef	SGIMIPS
#include <strings.h>
#endif
#include <dce/sec_cred.h>
#include <sec_cred_internal.h>
#include <dce/idlbase.h>
#include <sec_encode.h>

extern pthread_once_t   cred_mutex_once;
extern boolean32        cred_mutex_inited;
extern pthread_mutex_t  cred_mutex;


/* PRIVATE functions */

/*  sec___cred_epac_to_pac
 *
 * !!BEWARE!!
 *
 * We save some cycles by aliasing the epac data in the pac.  Applications are
 * *never* supposed to muck directly with the data returned via the sec_cred
 * interface.  If they do, they've got bigger problems than this bit of
 * aliasing.
 *
 * Due to differences in the layout of foreign groups in pacs and epacs, we
 * can't do any straightforward aliasing.  We are forced to dynamically
 * allocate foreign group storage in the pac.   This storage *must* be cleaned
 * up by the cred_handle destructor function.
 */

PUBLIC sec___cred_epac_to_pac (
    sec_id_epac_data_t  *epac_data,     /* [in] */
    boolean32           authenticated,  /* [in] */
    sec_id_pac_t        *pac,           /* [out] */
    error_status_t      *stp            /* [out] */
)
{
    sec_id_pa_t   pa = epac_data->pa;
    unsigned16    i, j, cur_group;

    memset(pac, 0, sizeof(sec_id_pac_t));

    pac->authenticated = authenticated;
    pac->pac_type = sec_id_pac_format_v1;
    pac->realm = pa.realm;
    pac->principal = pa.principal;
    pac->group = pa.group;
    pac->num_groups = pa.num_groups;
    pac->groups = pa.groups;

    /* Need to "flatten" the epac foreign groupset
     * structure into a v1_pac structure
     */

    /* count the total number of foreign groups in the epac */
    pac->num_foreign_groups = 0;
    for (i = 0; i < pa.num_foreign_groupsets; i++)
	pac->num_foreign_groups += pa.foreign_groupsets[i].num_groups;

    if (pac->num_foreign_groups > 0) {
	pac->foreign_groups = (sec_id_foreign_t *)
	    MALLOC( pac->num_foreign_groups * sizeof(sec_id_foreign_t));

	if (pac->foreign_groups == NULL) {
	    SET_STATUS(stp, sec_s_no_memory);
#ifdef KERNEL
	    return 0; 	/* hush up the 6.4 mongoose compiler */
#else /* ! KERNEL */
	    return;
#endif /* KERNEL */
	}

	/* now do the "copy" */
	cur_group = 0;
	for (i = 0; i < pa.num_foreign_groupsets; i++) {
	    for (j = 0; j < pa.foreign_groupsets[i].num_groups; j++) {
		pac->foreign_groups[cur_group].realm
		    = pa.foreign_groupsets[i].realm;
		pac->foreign_groups[cur_group].id
		    = pa.foreign_groupsets[i].groups[j];
		cur_group++;
	    }
	}
    }
#ifdef KERNEL
    return 0; 	/* hush up the ficus 6.4 mongoosed compiler */
#endif /* KERNEL */
}


/* sec___cred_pac_free
 *
 */
PRIVATE  sec___cred_pac_free(
    sec_id_pac_t  *pac
)
{
    if (pac->num_foreign_groups > 0 && pac->foreign_groups != NULL) {
	FREE(pac->foreign_groups);
    }
#ifdef KERNEL
    return 0; 	/* hush up the ficus 6.4 mongoosed compiler */
#endif /* KERNEL */
}


/* sec___cred_free_handle
 *
 * no integrity checks performed.  Assume that the
 * cred handle has been properly initialized so we
 * won't follow any bogus pointers.
 *
 */
PRIVATE void  sec___cred_free_handle(
    sec__cred_handle_t *chp
)
{
    unsigned32 i;
    
    if (!chp) {
	return;
    }

    if (chp->auth_info.handle_type == sec__cred_handle_type_server) {

	if (chp->auth_info.server_princ_name != NULL) {
	    FREE(chp->auth_info.server_princ_name);
	    chp->auth_info.server_princ_name = NULL;
	 }

	switch(chp->auth_info.authz_svc) {
	case rpc_c_authz_dce:
	    if (DLG_CHAIN(chp).num_bytes && DLG_CHAIN(chp).bytes) {
		FREE(DLG_CHAIN(chp).bytes);
	    }
	    
	    if (DLG_TOKEN(chp).num_tokens > 0
		&& DLG_TOKEN(chp).tokens != NULL) {
		for (i = 0; i < DLG_TOKEN(chp).num_tokens; i++) {
		    if (DLG_TOKEN(chp).tokens[i].token_bytes.num_bytes
			&& DLG_TOKEN(chp).tokens[i].token_bytes.bytes) {
			FREE(DLG_TOKEN(chp).tokens[i].token_bytes.bytes);
		    }
		}
		FREE(DLG_TOKEN(chp).tokens);
		DLG_TOKEN(chp).num_tokens = 0;
	    }
	    break;
	case rpc_c_authz_name:
	    if CLIENT_PRINC(chp) {
		FREE(CLIENT_PRINC(chp));
		CLIENT_PRINC(chp) = NULL;
	    }
	    break;
	default:
	    break;
	}
    }

    if (chp->epac_set_decoded) {
	/*
	 * free up the individual epacs.  The epac_decoded
	 * array is the last epac-related memory allocated
         * in the handle, so if it is non-null, the rest of the
         * priv data are legit as well
         */
	if (chp->epac_decoded != NULL) {
	    for (i = 0; i < chp->decoded_epac_set.num_epacs; i++) {
		if (chp->epac_decoded[i]) {
		    /*
                     * reset the magic so dangling pa handle references
		     * wil produce a sensible outcome.
		     */
		    chp->epacs[i].magic = 0;
		    /* 
		     * dispose of the epac itself
		     */
		    sec_encode_epac_data_free(FREE_PTR, &chp->epacs[i].epac);
                    /*
                     * Can't use the generic pac destructor
                     * routine because most of the pac is
                     * just aliases into the epac.
		     */
		    sec___cred_pac_free(&chp->epacs[i].v1_pac);
		    
		}
            }
            FREE(chp->epac_decoded);
	    FREE(chp->epacs);
	}

	sec_encode_epac_set_free(FREE_PTR, &chp->decoded_epac_set);
    }
    /* free up any rock data */
    if (chp->rocks) {
	(*(sec_cred_get_rock_destructor(chp->rocks->rock_key)))(chp->rocks->rock_data);
	 FREE(chp->rocks);
    }

    FREE((char *) chp);
}


PRIVATE void  sec___cred_decode_epac_set (
    sec__cred_handle_t  *chp,
    error_status_t      *stp
)
{
    CLEAR_STATUS(stp);
    if (!chp->epac_set_decoded) {
	sec_id_epac_set_decode(malloc_shim, FREE_PTR,
                               DLG_CHAIN(chp).num_bytes,
			       DLG_CHAIN(chp).bytes,
			       &chp->decoded_epac_set, stp);

        if (STATUS_OK(stp) && chp->decoded_epac_set.num_epacs != 0) {
          /*
           * we know how many epacs we've got, so we can
           * go ahead and allocate the necessary storage
           * arrays
           */
          chp->epacs = (sec__cred_priv_t *) MALLOC (sizeof(sec__cred_priv_t) *
            chp->decoded_epac_set.num_epacs);
          if (chp->epacs == NULL) {
            SET_STATUS(stp, sec_s_no_memory);
            sec_encode_epac_set_free(FREE_PTR, &chp->decoded_epac_set);
            return;
          }

          memset(chp->epacs, 0, sizeof(sec__cred_priv_t) *
            chp->decoded_epac_set.num_epacs);

          chp->epac_decoded = (boolean32 *) MALLOC(sizeof(boolean32) *
            chp->decoded_epac_set.num_epacs);
          if (chp->epac_decoded == NULL) {
            SET_STATUS(stp, sec_s_no_memory);
            sec_encode_epac_set_free(FREE_PTR, &chp->decoded_epac_set);
            FREE(chp->epacs);
            return;
          }
          memset(chp->epac_decoded, 0, sizeof(boolean32) *
            chp->decoded_epac_set.num_epacs);
        }
    }


    chp->epac_set_decoded = true;
}


/* PUBLIC routines */

/* sec__cred_decode_epac
 *
 */
PUBLIC void sec__cred_decode_epac(
    sec__cred_handle_t  *chp,
    unsigned32          index,
    error_status_t      *stp
)
{
    CLEAR_STATUS(stp);

    /* make sure the epac set is decoded */
    sec___cred_decode_epac_set(chp, stp);
    if (!STATUS_OK(stp)) return;

    if (index >= chp->decoded_epac_set.num_epacs) {
	SET_STATUS(stp, sec_cred_s_invalid_auth_handle);
	return;
    }

    if (chp->epac_decoded[index]) {
	return;  /* already decoded */
    }

    sec_id_epac_data_decode(malloc_shim, FREE_PTR,
	        chp->decoded_epac_set.epacs[index].pickled_epac_data.num_bytes,
		chp->decoded_epac_set.epacs[index].pickled_epac_data.bytes,
	        &chp->epacs[index].epac, stp);
    if (STATUS_OK(stp)) {
	/*
	 * generate the v1_pac now as well.
 	 */
	sec___cred_epac_to_pac(&chp->epacs[index].epac, 
			       chp->auth_info.authenticated,
			       &chp->epacs[index].v1_pac, stp);
	if (STATUS_OK(stp)) {
	    chp->epac_decoded[index] = true;
	    chp->epacs[index].magic = SEC__CRED_PA_HANDLE_MAGIC;
	} else {
	    sec_encode_epac_data_free(FREE_PTR, &chp->epacs[index].epac);
	}
    }
}

    
PUBLIC  error_status_t  sec__cred_create_authz_handle (
    sec__cred_auth_info_t    *auth_info,
    sec_bytes_t              *delegation_chain,
    sec_dlg_token_set_t      *dlg_token_set,
    sec_id_seal_set_t        *seal_set,
    rpc_authz_cred_handle_t  *authz_cred_handle
)
{
    sec__cred_handle_t      *cred_handle = NULL;
    int                     i;
    error_status_t          rst;

    authz_cred_handle->magic = 0; /* initialized to 0 */
    cred_handle = (sec__cred_handle_t *) MALLOC(sizeof(sec__cred_handle_t));
    if (cred_handle == NULL) {
	return sec_s_no_memory;
    }

    CLEAR_STATUS(&rst);
    memset(cred_handle, 0, sizeof(*cred_handle));

    cred_handle->auth_info.handle_type = auth_info->handle_type;

    /* Initialize sec_cred_get_authz_session_info() specific variables */
    uuid_create_nil(&cred_handle->session_id, &rst);
    cred_handle->session_expiration = 0;

    /* 
     * if no authorization service info, then just return.  Any attempt
     * to extract any authz info of any kind from the cred handle will
     * result in sec_cred_s_authz_svc_cannot_comply errors
     */
    if (auth_info->authz_svc == rpc_c_authz_none) {
	/* 
	 * if no authorization service info, then just return.  Any attempt
	 * to extract any authz info of any kind from the cred handle will
	 * result in sec_cred_s_authz_svc_cannot_comply errors
	 */
	cred_handle->auth_info.authz_svc = auth_info->authn_svc;
	return rst;

    }

    /* 
     * if this is a client side handle, then all we use is the delegation
     * chain, and we decode it right away.
     */
    if (auth_info->handle_type == sec__cred_handle_type_client) {
	/* 
	 * The only legitimate authz_svc value for a client cred handle is
	 * rpc_c_authz_dce.  We rely on the caller to call us correctly, 
         * so if we spot anything funny, we behave as though the 
	 * authz_svc were "none".  Any attempt to use the resulting
         * cred handle will result in sec_cred_s_authz_svc_cannot_comply
         * errors, but that's okay because this is an internal error
         * condition which should never arise in the first place.
	 */
	if (auth_info->authz_svc != rpc_c_authz_dce) {
	    cred_handle->auth_info.authz_svc = rpc_c_authz_none;
	    return rst;
	}

	cred_handle->auth_info.authz_svc = rpc_c_authz_dce;

	/* 
	 * alias the delegation chain storage in the auth_info parameter
	 * so we can decode it
	 */
	DLG_CHAIN(cred_handle) = *delegation_chain;
	sec___cred_decode_epac_set(cred_handle, &rst);
	/*
         * now undo the alasing so we won't free up storage that isn't ours
         * somewhere down the line.
         */
 	DLG_CHAIN(cred_handle).num_bytes = 0;
	DLG_CHAIN(cred_handle).bytes = NULL;
    }

    cred_handle->auth_info.authenticated = auth_info->authenticated;

    /*
     * Server-side cred handles use everything in the auth_info block
     */
    if (auth_info->handle_type == sec__cred_handle_type_server) {
#ifndef KERNEL
        if (auth_info->server_princ_name != NULL) {
            cred_handle->auth_info.server_princ_name = (unsigned_char_p_t)
	        strdup((char *)auth_info->server_princ_name);
        } else {
            cred_handle->auth_info.server_princ_name = (unsigned_char_p_t)NULL;
        }
#else
        cred_handle->auth_info.server_princ_name = (unsigned_char_p_t)NULL;
#endif /*KERNEL*/
	cred_handle->auth_info.protect_level = auth_info->protect_level;
	cred_handle->auth_info.authn_svc = auth_info->authn_svc;
	cred_handle->auth_info.authz_svc = auth_info->authz_svc;

	if (auth_info->authz_svc == rpc_c_authz_name) {
#ifndef KERNEL
	    /*
	     * need to make a copy of the client principal name
	     */
            if (auth_info->authz_info.client_princ_name != NULL) {
                CLIENT_PRINC(cred_handle) = (unsigned_char_p_t)
		    strdup((char *)auth_info->authz_info.client_princ_name);
            } else {
                CLIENT_PRINC(cred_handle) = (unsigned_char_p_t) NULL;
            }
#else
            CLIENT_PRINC(cred_handle) = (unsigned_char_p_t) NULL;
#endif /*KERNEL*/
	} else if (auth_info->authz_svc == rpc_c_authz_dce) {
	/*
	 * Need to stash away a copy of the delegation token for future use in
	 * priv server delegation calls.
	 */
	if (dlg_token_set != NULL) {
	    if ((DLG_TOKEN(cred_handle).num_tokens =
                dlg_token_set->num_tokens) == 0) {
                DLG_TOKEN(cred_handle).tokens = NULL;
            } else {
                /* Make room for the array of tokens */
                DLG_TOKEN(cred_handle).tokens = (sec_dlg_token_t *)
                  MALLOC(dlg_token_set->num_tokens * sizeof(sec_dlg_token_t));
                if (DLG_TOKEN(cred_handle).tokens == NULL) {
                    FREE(cred_handle);
		    return(sec_s_no_memory);
                }
                memset(DLG_TOKEN(cred_handle).tokens, 0,
                    (dlg_token_set->num_tokens * sizeof(sec_dlg_token_t)));


                /* Copy each individual token entry */
                for (i = 0; i < dlg_token_set->num_tokens; i++) {
                   DLG_TOKEN(cred_handle).tokens[i].expiration =
                        dlg_token_set->tokens[i].expiration;
                    if ((DLG_TOKEN(cred_handle).tokens[i].token_bytes.num_bytes = dlg_token_set->tokens[i].token_bytes.num_bytes) == 0) {
                       DLG_TOKEN(cred_handle).tokens[i].token_bytes.bytes = NULL;
                    } else {
                        DLG_TOKEN(cred_handle).tokens[i].token_bytes.bytes = (idl_byte *) MALLOC(dlg_token_set->tokens[i].token_bytes.num_bytes);
                        memcpy(DLG_TOKEN(cred_handle).tokens[i].token_bytes.bytes,
                            dlg_token_set->tokens[i].token_bytes.bytes,
                            dlg_token_set->tokens[i].token_bytes.num_bytes);
                    }
                }
	    }
	}


	    /* FAKE-EPAC
	     * Need to do something about "backdoor" authentication
	     * info here (krb5 session key and ticket expiration times,
	     * etc.  This info will be stored only if the server principal
	     * in the request is one of the architectural service names
	     * for the security service.
	     */

	    /* 
	     * If we're creating the handle on behalf of a server
	     * we have to stash away a copy of the delegation chain because
	     * it might be needed later by an intermediary for use in priv
	     * server delegation calls.  All decoding will  be done
	     * on demand as accessor functions are called.
	     */
	    if (GOOD_STATUS(&rst) && delegation_chain->num_bytes > 0) {
		DLG_CHAIN(cred_handle).num_bytes = delegation_chain->num_bytes;
		DLG_CHAIN(cred_handle).bytes = (idl_byte *) MALLOC(delegation_chain->num_bytes);
		if (DLG_CHAIN(cred_handle).bytes != NULL) {
		    memcpy(DLG_CHAIN(cred_handle).bytes, 
			   delegation_chain->bytes,
			   delegation_chain->num_bytes);
		    DLG_CHAIN(cred_handle).num_bytes = delegation_chain->num_bytes;
		} else { 
                    SET_STATUS(&rst, sec_s_no_memory);
                }
	    }
	    
	}
    }   

    if (BAD_STATUS(&rst)) {
	if (auth_info->handle_type == sec__cred_handle_type_client) {
	    sec___cred_free_handle(cred_handle);

	} else { /* handle_type == sec__cred_handle_type_server */
	    if (DLG_CHAIN(cred_handle).bytes != NULL) {
		FREE(DLG_CHAIN(cred_handle).bytes);
	    }
	    if (DLG_TOKEN(cred_handle).tokens != NULL) {
                for (i = 0; i < dlg_token_set->num_tokens; i++) {
                    if (DLG_TOKEN(cred_handle).tokens[i].token_bytes.bytes != NULL)
                        FREE(DLG_TOKEN(cred_handle).tokens[i].token_bytes.bytes);
                }
                FREE(DLG_TOKEN(cred_handle).tokens);
	    }
	}
	return rst;
    }

    /*
     * store a copy of our authz_cred_handle reference locally
     * for use in future handle integrity checks.
     */
    cred_handle->magic = cred_handle;

    /*
     * fill in the authz_cred_handle
     * !!! Probably need some logic here to either reject
     *      reallocation of an already valid cred handle
     *      or call the destructor function to get rid of
     *      the existing resources.
     */
    LOCK_CRED_KERNEL {
	authz_cred_handle->data = (idl_void_p_t) cred_handle;
	authz_cred_handle->magic = AUTHZ_CRED_HANDLE_MAGIC;
    } UNLOCK_CRED_KERNEL;

    return error_status_ok;

	
}


PUBLIC  error_status_t  sec__cred_free_cred_handle (
    rpc_authz_cred_handle_t  *cred_handle
)
{
    sec__cred_handle_t    *chp;
    error_status_t        rst;

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(cred_handle, &rst);
	if (STATUS_OK(&rst)) {
	    /*
	     * reset the internal magic so all future
             * attempts to use this handle will fail.
             * We can give up the lock after we do this
             */
	    chp->magic = 0;
	    cred_handle->magic = 0;
	}
    } UNLOCK_CRED_KERNEL;

    if (STATUS_OK(&rst)) {
	sec___cred_free_handle(chp);
    }

    return error_status_ok;
}


PUBLIC  sec__cred_handle_t *sec__cred_check_handle (
    rpc_authz_cred_handle_t  *authz_cred_handle,
    error_status_t           *stp
)
{
    sec__cred_handle_t *chp = authz_cred_handle->data;

    CLEAR_STATUS(stp);

    if (authz_cred_handle->magic ==  AUTHZ_CRED_HANDLE_MAGIC
    && authz_cred_handle->data == chp->magic) {
	return chp;
    } else {
	SET_STATUS(stp, sec_cred_s_invalid_auth_handle);
	return NULL;
    }
}

/* sec__cred_check_pa_handle
 *
 */
sec__cred_pa_handle_t  sec__cred_check_pa_handle (
    sec_cred_pa_handle_t   caller_pas,
    error_status_t         *stp
)
{
    sec__cred_pa_handle_t  pah = (sec__cred_pa_handle_t) caller_pas;

    CLEAR_STATUS(stp);

    if (pah == NULL || pah->magic != SEC__CRED_PA_HANDLE_MAGIC)
	SET_STATUS(stp, sec_cred_s_invalid_pa_handle);

    return pah;
}


#ifndef KERNEL
/* sec__cred_check_attr_cursor
 *
 * Check the validity of an attribute cursor.
 *
 * Assumptions
 *    The pa handle provided by the caller is valid
 *    Caller has provided the necessary concurrency control.
 */
void  sec__cred_check_attr_cursor (
    sec__cred_int_cursor_t *icp,
    sec__cred_pa_handle_t  pah,
    error_status_t         *stp
)
{
    CLEAR_STATUS(stp);

    if (icp == NULL || icp->cursor_type != sec__cred_cursor_type_attr) {
	SET_STATUS(stp, sec_cred_s_invalid_cursor);
    } else if (icp->next >= PRIVS(pah).num_attrs) {
	    SET_STATUS(stp, sec_cred_s_no_more_entries);
    }
    return;
}
#endif /*KERNEL*/


/* sec__cred_check_dlg_cursor
 *
 * Check an the validity of an attribute cursor.
 *
 * Assumptions
 *    The cred handle provided by the caller is valid
 *    Caller has provided the necessary concurrency control.
 */
void  sec__cred_check_dlg_cursor (
    sec__cred_int_cursor_t *icp,
    sec__cred_handle_t     *chp,
    error_status_t         *stp
)
{
    CLEAR_STATUS(stp);

    if (icp == NULL || icp->cursor_type != sec__cred_cursor_type_dlg) {
	SET_STATUS(stp, sec_cred_s_invalid_cursor);
    } else if (icp->next >= chp->decoded_epac_set.num_epacs) {
	    SET_STATUS(stp, sec_cred_s_no_more_entries);
    }
    return;
}


extern void sec__cred_set_session_info (
    rpc_authz_cred_handle_t  *cred_handle,  /* [in] */ 
    uuid_t              *session_id,
    sec_timeval_sec_t   *session_expiration,
    error_status_t      *stp
)
{
    sec__cred_handle_t  *chp;

    CLEAR_STATUS(stp);

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(cred_handle, stp);
	if (GOOD_STATUS(stp)) {
	    if (session_id) {
		chp->session_id = *session_id;
	    }
	    if (session_expiration) {
		chp->session_expiration = *session_expiration;
	    }
	} UNLOCK_CRED_KERNEL;
    }

}


/*
 * "Backdoor" functions
 *
 * These functions provide access to data in a cred handle
 * that is available only to the runtime system.
 */

/* sec__cred_get_deleg_chain
 *
 * Return the encoded delegation chain.  It's up to
 * the caller to decode it if necessary.
 *
 * The bytes field of the returned delegation chain
 * aliases storage in the runtime, so do NOT muck
 * with it.  If you must muck with it, make a copy.
 */
PUBLIC void sec__cred_get_deleg_chain (
    rpc_authz_cred_handle_t  *cred_handle,  /* [in] */
    sec_bytes_t      *encoded_deleg_chain,  /* [out] */
    error_status_t   *stp                   /* [out] */
)
{
    sec__cred_handle_t *chp;

    CLEAR_STATUS(stp);

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(cred_handle, stp);
	if (GOOD_STATUS(stp)) {
	    /*
             * only server cred handles store the
             * delegation chain, so reject the call
             * if the handle was generated by a client
             * or the handle represents authz_name
             * authorization.
             */
	    if (chp->auth_info.handle_type == sec__cred_handle_type_client
	    || chp->auth_info.authz_svc != rpc_c_authz_dce) {
		SET_STATUS(stp, sec_cred_s_invalid_auth_handle);
	    } else {
		*encoded_deleg_chain = DLG_CHAIN(chp);
	    }
	} UNLOCK_CRED_KERNEL;
    }
}

/* sec__cred_get_deleg_token
 *
 * Return the encoded delegation chain.  It's up to the caller to decode it if
 * necessary.
 *
 * The bytes field of the returned delegation chain aliases storage in the
 * runtime, so do NOT muck with it.  If you must muck with it, make a copy.
 */

PUBLIC void sec__cred_get_deleg_token (
    rpc_authz_cred_handle_t  *cred_handle,      /* [in]  */
    sec_dlg_token_set_t      *dlg_token_set,    /* [out] */
    error_status_t           *stp               /* [out] */
)
{
    sec__cred_handle_t *chp;

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(cred_handle, stp);
	if (GOOD_STATUS(stp)) {
	    /* only server cred handles store the delegation token, so reject
	     * the call if the handle as generated by a client (via sec_login-
	     * calls)
             */
	    if (chp->auth_info.handle_type == sec__cred_handle_type_client) {
		SET_STATUS(stp, sec_cred_s_invalid_auth_handle);
	    } else {
		*dlg_token_set = DLG_TOKEN(chp);
	    }
	} UNLOCK_CRED_KERNEL;
    }
}

/* sec__cred_deleg_type_permitted
 *
 * Return the delegation type permitted by the last
 * caller in the delegation chain represented  by a
 * rpz_authz_cred_handle_t.
 *
 */
PUBLIC sec_id_delegation_type_t  sec__cred_deleg_type_permitted (
    rpc_authz_cred_handle_t  *cred_handle,      /* [in]  */
    error_status_t           *stp               /* [out] */
)
{
    sec__cred_handle_t *chp;
    sec_id_delegation_type_t  deleg_type = sec_id_deleg_type_none;

    LOCK_CRED_KERNEL {
	chp = sec__cred_check_handle(cred_handle, stp);
	if (GOOD_STATUS(stp)
	&& chp->auth_info.authz_svc ==  rpc_c_authz_dce ) { 	
	    sec___cred_decode_epac_set(chp, stp);

	    /* 
	     * First make sure the delegation chain itself has 
             * been decoded
             */
	    if (GOOD_STATUS(stp)) {
		/*
                 * decode the epac of the last delegate in the
                 * chain--the delegation type permitted by the
                 * last delegate determines what future delegation
                 * operations are legal.
		 */
		sec__cred_decode_epac(chp,
		    chp->decoded_epac_set.num_epacs - 1,
		    stp);
	    }

	    if (GOOD_STATUS(stp)) {
		deleg_type = 
                    chp->epacs[chp->decoded_epac_set.num_epacs - 1].epac.deleg_type;
	    }
	} UNLOCK_CRED_KERNEL;
    }

    return deleg_type;
}
