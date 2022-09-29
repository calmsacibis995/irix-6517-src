/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_encode.h,v $
 * Revision 65.1  1997/10/20 19:46:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.1  1996/08/09  12:05:33  arvind
 * 	Merge Registry support for KDC private key storage
 * 	[1996/07/17  14:04 UTC  aha  /main/aha_pk6/2]
 *
 * 	Do not include password encoding for KERNEL
 * 	[1996/07/13  20:50 UTC  aha  /main/aha_pk6/1]
 *
 * 	Merge-out and bugfixes and Registry work
 * 	[1996/02/18  23:01:00  marty  1.1.4.2]
 *
 * Revision 1.1.4.2  1996/02/18  23:01:00  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:19:21  marty]
 * 
 * Revision 1.1.4.1  1995/12/08  17:29:07  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/03  22:04 UTC  mullan_s
 * 	Binary Compatibility Merge
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/1  1995/01/11  18:16 UTC  mullan_s
 * 	remove sec_encode_type_v1_1_lc_info.
 * 	[1995/12/08  16:54:15  root]
 * 
 * Revision 1.1.2.4  1994/08/18  20:25:26  greg
 * 	Add types/routines to allow a caller to test
 * 	the type of an NDR-encoded buffer.
 * 	[1994/08/16  19:02:13  greg]
 * 
 * Revision 1.1.2.3  1994/06/17  18:42:09  devsrc
 * 	cr10872 - fix copyright
 * 	[1994/06/17  18:08:46  devsrc]
 * 
 * Revision 1.1.2.2  1994/05/11  19:08:15  ahop
 * 	hp_sec_to_osf_2 drop
 * 	add encoding routines for login context data
 * 	add v1.1 authorization data encoding/decoding prototypes
 * 	add memory management function parameters to sec_encode interfaces
 * 	[1994/04/29  21:01:48  ahop]
 * 
 * Revision 1.1.2.1  1994/01/28  23:10:32  burati
 * 	Initial version (dlg_bl1)
 * 	[1994/01/18  21:54:17  burati]
 * 
 * $EndLog$
 */

/*
 * Copyright (c) Hewlett-Packard Company 1993, 1994
 * Unpublished work. All Rights Reserved.
 */

#ifndef sec_encode_h__included
#define sec_encode_h__included

#include <dce/id_epac.h>
#include <dce/prvnbase.h>
#include <dce/authz_base.h>
#include <dce/sec_login_encode.h>

/* Memory Management
 *
 * All encode and decode routines take allocator
 * and deallocator functions as input parameters.
 * All destructor functions take a deallocator
 * function as input.
 *
 * Callers may pass NULL function parameters,
 * causing the routine in question to use the
 * RPC client memory management routines in
 * effect ***IN THE CURRENT THREAD AT THE TIME
 * THE CALL IS MADE***.
 *
 * You should NOT choose to use the defaults
 * unless you thoroughly understand the details
 * of RPC memory management and know that it is
 * safe to use the default RPC client memory
 * management routines in your particular situation.
 * Horrible things will happen to your process if 
 * you choose to ignore this advice.
 *
 * If a routine takes both an allocator and a 
 * deallocator function, either both must be NULL
 * or both must be non-NULL.  Horrible things
 * will happen to your process if you violate this
 * rule.
 *
 * Be careful to maintain consistency between memory
 * mamnegent functions passed to encoding/decoding
 * routines and deallocator functions passed to 
 * destructor functions.  
 */

/*
 * There is a problem (at least on HP/UX) having to do with
 * differences in the prototype for the native malloc() 
 * function and the prototype for the RPC memory management
 * functions.  The native malloc takes an (unisgned int) 
 * whereas the  RPC routines take an idl_size_t, which 
 * is an alias for an (unsigned long).  These are different
 * types that probably have the same precision on 32 bit
 * architectures, but probably have different precisions
 * on 64 bit architectures.
 *
 * We use idl_size_t in the prototype for function parameters
 * to our encoding/decoding routines because it matches the
 * protoypes for funtion parameters passed to the RPC
 * memory management routines called beneath this API.
 * The compiler rightly complains when we pass the native
 * HP/UX malloc function.  To avoid any possible problems
 * due to precision  mismtaches down the line, we provide a 
 * "shim" malloc routine that prototypes malloc with
 * idl_size_t and internally coerces the idl_size_t
 * into a native size_t before calling the native malloc.
 * For those platforms where differences in precision
 * between a size_t and a idl_size_t aren't an issue,
 * this can be a macro that just expands to malloc()
 *
 * Caller's of this interface should pass malloc_shim()
 * instead of malloc() when specifying the native malloc().
 */

idl_void_p_t malloc_shim(idl_size_t size);

/* 
 * When a routine receives an encoded pickle,
 * it needs to know what decoding routine to
 * call.  Often this can be determined from
 * context i.e., each pickled_epac_data entry
 * in an epac_set is is assumed to have been 
 * encoded with the sec_id_epac_data_encode()
 * routine and it is therefore assumed that 
 * it can be safely and correctly decoded 
 * using the sec_id_epac_data_decode()
 * routine.
 *
 * In some cases it is impossible to tell
 * from context which decoding routine to use.
 * It is therefore necesary to be able to 
 * determine, from the pickle contents alone,
 * the type of the pickle.  The pickle type
 * determines which decoding routine should be
 * used to decode the pickle.
 *
 * We provide an encoding id abstraction that
 * allows the caller to identify the encoding routine 
 * used to  encode the data, and consequently, the 
 * correct routine with which to decode the data.
 *
 * NOTE: the sec_encode_type_t is NOT a flag set.  
 * An instance of a sec_encode_type_t should only
 * be tested for equality with one of the defined
 * constants.
 */

typedef unsigned16 sec_encode_type_t;

#define sec_encode_type_unknown          0x0
#define sec_encode_type_epac_data        0x1
#define sec_encode_type_epac             0x2
#define sec_encode_type_epac_set         0x3
#define sec_encode_type_dlg_token_set    0x4
#define sec_encode_type_v1_1_authz_data  0x5
#ifndef KERNEL
#define sec_encode_type_pwd              0x6
#endif /*KERNEL*/

sec_encode_type_t sec_encode_get_encoding_type (
    unsigned32          num_bytes,                     /* [in]  */
    idl_byte            *bytes                         /* [in]  */
);


/*
 * The actual encoding/decoding routines
 */

void sec_id_epac_data_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_data_t  *epac_data,                    /* [in]  */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
);

void sec_id_epac_data_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in]  */
    idl_byte            *bytes,                        /* [in]  */
    sec_id_epac_data_t  *epac_data,                    /* [out] */
    error_status_t      *stp                           /* [out] */
);

void sec_id_epac_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_t       *epac,                         /* [in]  */
    unsigned32          *num_bytes,                    /* [out] */ 
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
);

void sec_id_epac_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in]  */
    idl_byte            *bytes,                        /* [in]  */
    sec_id_epac_t       *epac,                         /* [out] */      
    error_status_t      *stp                           /* [out] */
);

void sec_id_epac_set_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_set_t   *epac_set,                     /* [in]  */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
);    

void sec_id_epac_set_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in]  */
    idl_byte            *bytes,                        /* [in]  */
    sec_id_epac_set_t   *epac_set,                     /* [out] */ 
    error_status_t      *stp        
);

void sec_dlg_token_set_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_dlg_token_set_t *dlg_token_set,                /* [in]  */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
);

void sec_dlg_token_set_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in]  */
    idl_byte            *bytes,                        /* [in]  */
    sec_dlg_token_set_t *dlg_token_set,                /* [out] */
    error_status_t      *stp                           /* [out] */
);

void sec_v1_1_authz_data_encode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_v1_1_authz_data_t *v1_1_authz_data,              /* [in] */
    unsigned32            *num_bytes,                    /* [out] */
    idl_byte              **bytes,                       /* [out] */
    error_status_t        *stp                           /* [out] */
);

void sec_v1_1_authz_data_decode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32            num_bytes,                     /* [in] */
    idl_byte              *bytes,                        /* [in] */
    sec_v1_1_authz_data_t *v1_1_authz_data,              /* [out] */
    error_status_t        *stp                           /* [out] */
);
#ifndef KERNEL
void sec_pwd_encode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_passwd_rec_t      *pwd_rec_p,                    /* [in] */
    unsigned32            *num_bytes,                    /* [out] */
    idl_byte              **bytes,                       /* [out] */
    error_status_t        *stp                           /* [out] */
);

void sec_pwd_decode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32            num_bytes,                     /* [in] */
    idl_byte              *bytes,                        /* [in] */
    sec_passwd_rec_t      *pwd_rec_p,                    /* [out] */
    error_status_t        *stp                           /* [out] */
);
#endif /*KERNEL*/

/*
 * Destructor functions for decoded types.
 */

void sec_encode_epac_data_free (
    void               (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_data_t  *epac_data
);

void sec_encode_epac_free (
    void           (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_t  *epac
);

void sec_encode_epac_set_free (
    void               (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_set_t  *epac_set
);

void  sec_encode_dlg_token_set_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_dlg_token_set_t  *ts
);

void  sec_encode_v1_1_authz_data_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_v1_1_authz_data_t  *azd
);

void sec_encode_v1_1_lc_info_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_login__v1_1_info_t    *v1_1_lc_info
);

void sec_encode_buffer_free (
    void      (*dealloc)(idl_void_p_t ptr),  /* [in] */
    idl_byte  **bytes
);

#ifndef KERNEL
void sec_encode_pwd_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_passwd_rec_t      *pwd_rec_p
);
#endif /*KERNEL*/

/*
 * The following routines allow us to reach inside
 * some of the decoded structures and free some substructure
 * which we need to do in some cases.
 */

void  sec_encode_opt_req_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_opt_req_t  *res
);

void  sec_encode_restriction_set_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_restriction_set_t *res_set
);

#endif  /* sec_encode_h__included */
