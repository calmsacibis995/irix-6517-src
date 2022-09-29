/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE). See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_cred_internal.h,v $
 * Revision 65.5  1998/04/01 14:17:07  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed extern declarations for mkmalloc() and mkfree() to ansi
 * 	style.  Added extern declarations for sec__cred_decode_epac() and
 * 	sec__cred_check_dlg_cursor().
 *
 * Revision 65.4  1997/10/27  19:49:34  jdoak
 * 6.4 + 1.2.2 merge
 *
 * Revision 65.3  1997/10/24  22:03:12  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.2  1997/10/20  19:46:24  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.6.1  1996/10/03  14:51:36  arvind
 * 	clean up gcc warnings.
 * 	[1996/09/16  23:41 UTC  sommerfeld  /main/sommerfeld_pk_kdc_1/1]
 *
 * Revision 1.1.4.2  1996/03/11  01:42:51  marty
 * 	Update OSF copyright years
 * 	[1996/03/10  19:14:02  marty]
 * 
 * Revision 1.1.4.1  1995/12/08  17:29:02  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:54:14  root]
 * 
 * 	     add support for rock interfaces
 * 	Revision 1.1.2.3  1994/08/04  16:12:50  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/28  17:16:52  mdf]
 * 
 * Revision 1.1.2.4  1994/08/09  17:32:36  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  16:58:27  burati]
 * 
 * Revision 1.1.2.2  1994/07/15  14:59:59  mdf
 * 	Hewlett Packard Security Code Drop
 * 	[1994/07/14  17:17:19  mdf]
 * 
 * Revision 1.1.2.1  1994/06/02  21:40:47  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:15:34  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * $EndLog$
 */

/* sec_cred_internal.h
 * Internal support routines for public sec_cred module.
 *
 * Copyright (c) Hewlett-Packard Company 1993, 1994
 * Unpublished work. All Rights Reserved.
 */

#ifndef sec_cred_internal__included
#define sec_cred_internal__included

#include <dce/prvnbase.h>
#include <dce/sec_cred.h>
#ifdef KERNEL
#include <dce/ker/pthread.h>
#ifdef	SGIMIPS
#include <sys/systm.h>
#else
#include <sys/malloc.h>
#endif /*SGIMIPS*/
#else
#include <dce/pthread.h>
#endif /*KERNEL*/

#define AUTHZ_CRED_HANDLE_MAGIC    0x13245768  /* whatever ... */
#define SEC__CRED_PA_HANDLE_MAGIC  0x81726354  /* whatever ... */

#define  sec__cred_cursor_type_attr  0  /* attribute cursor */
#define  sec__cred_cursor_type_dlg   1  /* delegate cursor */
typedef struct {
    unsigned32     cursor_type;
    unsigned32     next;
} sec__cred_int_cursor_t;


typedef  unsigned32  sec__cred_handle_type_t;
#define  sec__cred_handle_type_client  0
#define  sec__cred_handle_type_server  1

typedef struct {
    sec__cred_handle_type_t    handle_type;
    boolean32                  authenticated;
    unsigned_char_p_t          server_princ_name;
    unsigned32                 protect_level;
    unsigned32                 authn_svc;
    unsigned32                 authz_svc;
    /* 
     * the contents of the authz_info union depend on the
     * value of the authz_svc field above.  If authz_name then
     * the client_princ_name arms applies.  If authz_dce, then
     * the dce arm applies.
     */
    union {
	unsigned_char_p_t      client_princ_name; 
	struct {
	        sec_bytes_t            dlg_chain; /* encoded_epac_set */ 
		sec_dlg_token_set_t    delegation_token_set;
	    } dce;
    } authz_info;
} sec__cred_auth_info_t, *sec__cred_auth_info_p_t;

typedef struct  _sec__cred_priv_t {
    unsigned32          magic;
    sec_id_epac_data_t  epac;
    sec_id_pac_t        v1_pac;
} sec__cred_priv_t;

typedef  sec__cred_priv_t  *sec__cred_pa_handle_t;

typedef struct rock_struct {
    unsigned32                rock_key;
    void                      *rock_data;   /* application private data */
    struct rock_struct        *next;
} rock_struct_t, *rock_struct_p_t;

typedef struct _sec__cred_handle_t  {
    void                       *magic;
    uuid_t                     session_id;
    sec_timeval_sec_t          session_expiration;
    sec__cred_auth_info_t      auth_info;
    boolean32                  epac_set_decoded;
#if 0
    boolean32                  dlg_chain_decoded; /* old epac_set_decode */
#endif
    boolean32                  *epac_decoded;  /* for each individual epac */
    sec_id_epac_set_t          decoded_epac_set;
#if 0
    sec_id_epac_set_t          decoded_dlg_chain; /* old decoded_epac_set */
#endif
    sec__cred_priv_t           *epacs;
    rock_struct_p_t            rocks;
} sec__cred_handle_t;

/* and some convenience macros fopr referencing fields within the cred handle */
#define DLG_CHAIN(ch) (ch->auth_info.authz_info.dce.dlg_chain)
#define DLG_TOKEN(ch) (ch->auth_info.authz_info.dce.delegation_token_set)
#define CLIENT_PRINC(ch) (ch->auth_info.authz_info.client_princ_name)
#define SERVER_PRINC(ch) (ch->auth_info.server_princ_name)

/* Macros for dealing with mem allocation in the kernel */
#undef MALLOC
#undef FREE
#ifdef KERNEL
#define MALLOC(s)	mkmalloc(s)
#define FREE(a) 	mkfree(a)

#define MALLOC_PTR	mkmalloc
#define FREE_PTR	mkfree

#ifdef SGIMIPS
extern void *		mkmalloc(unsigned int size);
extern void 		mkfree(void *a);
#else
extern char *		mkmalloc();
extern void		mkfree();
#endif

#else
#define MALLOC(s)	malloc(s)
#define FREE(a) 	free(a)

#define MALLOC_PTR	malloc
#define FREE_PTR	free
#endif /*KERNEL*/


/* convenience macro for the epac data associated with an internal pa handle */
#define PRIVS(pah) (pah->epac)

/* FAKE-EPAC locking mechanism */
void create_cred_mutex();

#define LOCK_CRED_KERNEL \
    if (!cred_mutex_inited) { \
        pthread_once(&cred_mutex_once, create_cred_mutex); \
    } \
    pthread_mutex_lock(&cred_mutex);

#define UNLOCK_CRED_KERNEL pthread_mutex_unlock(&cred_mutex);

/*
 * Note the use of the comma operator in the following macro
 * which allows us to make it behave like a function that
 * both sets a status and returns a boolean.
 */
/*
#define CHECK_AUTHZ_COMPLIANCE(chp,service,stp) \
    (((chp)->auth_info.authz_svc) == (service) ? STATUS_OK(CLEAR_STATUS((stp))) \
        : STATUS_OK(SET_STATUS((stp), sec_cred_s_authz_cannot_comply)))
*/
sec__cred_handle_t *sec__cred_check_handle (
    rpc_authz_cred_handle_t  *authz_cred_handle,
    error_status_t           *stp
);

sec__cred_pa_handle_t  sec__cred_check_pa_handle (
    sec_cred_pa_handle_t   callers_pas,
    error_status_t         *stp
);

error_status_t  sec__cred_free_cred_handle (
    rpc_authz_cred_handle_t  *cred_handle
);

error_status_t  sec__cred_create_authz_handle (
    sec__cred_auth_info_t    *auth_info,
    sec_bytes_t              *delegation_chain,
    sec_dlg_token_set_t      *delegation_token_set,
    sec_id_seal_set_t        *seal_set,
    rpc_authz_cred_handle_t  *authz_cred_handle
);

extern void sec__cred_set_session_info (
    rpc_authz_cred_handle_t  *cred_handle,  /* [in] */ 
    uuid_t              *session_id,
    sec_timeval_sec_t   *session_expiration,
    error_status_t      *stp
);

#ifdef SGIMIPS
extern void sec__cred_decode_epac(
    sec__cred_handle_t  *chp,
    unsigned32          index,
    error_status_t      *stp
);

extern void  sec__cred_check_dlg_cursor (
    sec__cred_int_cursor_t *icp,
    sec__cred_handle_t     *chp,
    error_status_t         *stp
);
#endif

/*
 * "Backdoor" functions
 *
 * These functions provide access to data in a cred handle
 * that is available only to the runtime system.
 */

/* sec__cred_get_deleg_chain
 *
 * Return the encoded delegation chain.  It's up to the caller to decode it
 * if necessary.
 *
 * The bytes field of the returned delegation chain aliases storage in the
 * runtime, so the caller should not muck around in any way, shape, or form,
 * with it.  If you must muck with it, make a copy.
 */
void sec__cred_get_deleg_chain (
    rpc_authz_cred_handle_t  *cred_handle,  /* [in] */
    sec_bytes_t      *encoded_deleg_chain,  /* [out] */
    error_status_t   *stp                   /* [out] */
);

/* sec__cred_get_deleg_token
 *
 * Return the encoded delegation token.  It's up to the caller to decode it
 * if necessary.
 *
 * The bytes field of the returned delegation token aliases storage in the
 * runtime, so the caller should not muck around in any way, shape, or form,
 * with it.  If you must muck with it, make a copy.
 */
void sec__cred_get_deleg_token (
    rpc_authz_cred_handle_t  *cred_handle,      /* [in]  */
    sec_dlg_token_set_t      *dlg_token_set,    /* [out] */
    error_status_t           *stp               /* [out] */
);


/* sec__cred_deleg_type_permitted
 *
 * Return the delegation type permitted by the last
 * caller in the delegation chain represented  by a
 * rpc_authz_cred_handle_t.  It is the delegation 
 * type of the last delegate that determines whether
 * or not the credentials may be delegated further.
 * This routine saves the trouble of iterating over
 * all delegates in the chain to acquire the delegation
 * type of the last delegate.
 */
sec_id_delegation_type_t  sec__cred_deleg_type_permitted (
    rpc_authz_cred_handle_t  *cred_handle,      /* [in]  */
    error_status_t           *stp               /* [out] */
);

sec_cred_destruct_fn_t sec_cred_get_rock_destructor(
    unsigned32               rock_key      /* [in] */
);
#endif
