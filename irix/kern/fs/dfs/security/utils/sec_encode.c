/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_encode.c,v 65.5 1998/03/23 17:31:42 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_encode.c,v $
 * Revision 65.5  1998/03/23 17:31:42  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/20 17:29:19  lmc
 * If we are compiling for the kernel, don't pick up stdio.h or stdlib.h.  These
 * pick up conflicting definitions for things like printf.
 *
 * Revision 65.3  1998/01/07  18:14:19  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  20:19:03  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:48:34  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.7.2  1996/10/03  15:14:53  arvind
 * 	Deal with compiler warnings about live variables across setjmp()
 * 	[1996/09/16  23:32 UTC  sommerfeld  /main/DCE_1.2.2/sommerfeld_pk_kdc_1/1]
 *
 * 	CHFts19896: public key version support
 * 	[1996/09/13  23:44 UTC  aha  /main/DCE_1.2.2/aha_pk9_3/1]
 *
 * 	Change sec_passwd_privkey -> sec_passwd_pubkey
 * 	[1996/09/10  21:08 UTC  aha  /main/DCE_1.2.2/aha_pk9_2/1]
 *
 * 	Merge Registry support for KDC private key storage
 * 	[1996/07/17  14:03 UTC  aha  /main/aha_pk6/3]
 *
 * 	Do not include passwd encoding for kernel
 * 	[1996/07/16  20:09 UTC  aha  /main/aha_pk6/2]
 *
 * 	Include rgymacro.h to pick up PASSWD_TYPE()
 * 	[1996/07/13  20:50 UTC  aha  /main/aha_pk6/1]
 *
 * 	Merge-out and bugfixes and Registry work
 * 	[1996/02/18  00:22:55  marty  1.1.5.2]
 *
 * Revision 1.1.7.1  1996/08/09  12:12:23  arvind
 * 	Merge Registry support for KDC private key storage
 * 	[1996/07/17  14:03 UTC  aha  /main/aha_pk6/3]
 * 
 * 	Do not include passwd encoding for kernel
 * 	[1996/07/16  20:09 UTC  aha  /main/aha_pk6/2]
 * 
 * 	Include rgymacro.h to pick up PASSWD_TYPE()
 * 	[1996/07/13  20:50 UTC  aha  /main/aha_pk6/1]
 * 
 * 	Merge-out and bugfixes and Registry work
 * 	[1996/02/18  00:22:55  marty  1.1.5.2]
 * 
 * Revision 1.1.5.2  1996/02/18  00:22:55  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  23:10:30  marty]
 * 
 * Revision 1.1.5.1  1995/12/08  18:03:08  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/5  1995/10/16  14:25 UTC  dnguyen
 * 	Merge fix for CHFts16298
 * 
 * 	HP revision /main/HPDCE02/dnguyen_CHFts16298/2  1995/10/13  12:43 UTC  dnguyen
 * 	Add checks before freeing and reset counts and ptrs after freeing.
 * 
 * 	HP revision /main/HPDCE02/dnguyen_CHFts16298/1  1995/10/04  13:51 UTC  dnguyen
 * 	Fix OT 13009: Mem leak sec_encode_epac_set_free()
 * 
 * 	HP revision /main/HPDCE02/4  1995/06/15  22:01 UTC  burati
 * 	Merge fix for chfts15431
 * 
 * 	HP revision /main/HPDCE02/jrr_sec_chfts15431/4  1995/06/15  20:57 UTC  burati
 * 	Fix mem leak for kernel too
 * 
 * 	HP revision /main/HPDCE02/jrr_sec_chfts15431/3  1995/06/14  20:58 UTC  burati
 * 	Previous alloc changes should only be done in user space
 * 
 * 	HP revision /main/HPDCE02/jrr_sec_chfts15431/2  1995/06/08  23:25 UTC  jrr
 * 	More ALLOC/FREE macro work.
 * 
 * 	HP revision /main/HPDCE02/jrr_sec_chfts15431/1  1995/06/05  21:38 UTC  jrr
 * 	Modify the SET_ALLOC_FREE and RESET_ALLOC_FREE macros to stop memory leak.
 * 
 * 	HP revision /main/HPDCE02/2  1995/06/05  21:40 UTC  jrr
 * 	Fix memory  leaks
 * 	[1995/05/25  21:45 UTC  burati  /main/HPDCE02/mb_mothra4/1]
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/03  22:15 UTC  mullan_s
 * 	Binary Compatibility Merge
 * 
 * 	HP revision /main/mullan_mothra_bin_compat/1  1995/01/11  18:17 UTC  mullan_s
 * 	Remove sec_v1_1_lc_info_encode/decode - no longer necessary.
 * 	[1995/12/08  17:23:10  root]
 * 
 * 	KERNEL changes for libksec.a
 * 	[1994/06/24  22:01:53  rsarbo]
 * 
 * Revision 1.1.2.14  1994/09/27  15:07:01  burati
 * 	CR12346 Put previous fixes back in now that rs_auth.c is fixed
 * 	[1994/09/26  21:43:32  burati]
 * 
 * Revision 1.1.2.13  1994/09/23  18:58:54  burati
 * 	CR12323 Back out previous fixes for now until we figure out the problem
 * 	[1994/09/23  18:54:08  burati]
 * 
 * Revision 1.1.2.12  1994/09/21  22:01:35  burati
 * 	CR10924 Fix memory leaks not freeing epacs & seal sets
 * 	[1994/09/21  21:00:47  burati]
 * 
 * Revision 1.1.2.11  1994/09/20  20:47:12  burati
 * 	CR10924 Fix memory leaks in sec_encode_pa_data_free()
 * 	[1994/09/20  20:30:42  burati]
 * 
 * Revision 1.1.2.10  1994/09/16  21:51:43  sommerfeld
 * 	[OT11915]	sec_login_encode version changed.
 * 	[1994/09/16  21:50:40  sommerfeld]
 * 
 * Revision 1.1.2.9  1994/08/19  20:47:07  burati
 * 	CR11782 Don't build new encoding type inquiry routines in the kernel
 * 	[1994/08/19  20:46:31  burati]
 * 
 * Revision 1.1.2.8  1994/08/18  20:25:50  greg
 * 	Kernel and user space malloc()s nedd different
 * 	casts in the malloc_shim() routine.
 * 	[1994/08/17  14:30:16  greg]
 * 
 * 	Finish the malloc_shim() replacements that were
 * 	missed the last time around.
 * 	[1994/08/17  14:12:18  greg]
 * 
 * 	Fix stupid typo in previous version.
 * 	[1994/08/17  14:05:35  greg]
 * 
 * 	Add abstract  encoding type evalutaion routines.
 * 	[1994/08/17  13:29:19  greg]
 * 
 * Revision 1.1.2.7  1994/08/16  14:42:42  burati
 * 	Add encoding util rtn to strip out non-kernel information.
 * 	[1994/08/16  14:34:43  burati]
 * 
 * Revision 1.1.2.6  1994/08/09  17:32:54  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/08  22:04:02  burati]
 * 
 * Revision 1.1.2.5  1994/06/17  18:43:05  devsrc
 * 	cr10872 - fix copyright
 * 	[1994/06/17  18:20:21  devsrc]
 * 
 * Revision 1.1.2.4  1994/06/10  15:07:27  greg
 * 	intercell fixes for DCE1.1 beta
 * 	[1994/06/03  20:50:41  greg]
 * 
 * Revision 1.1.2.3  1994/06/02  21:18:04  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:21:11  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.1.2.2  1994/05/11  19:36:01  ahop
 * 	hp_sec_to_osf_2 drop
 * 	add encoding routines for login context data
 * 	add v1.1 authorization data encoding/decoding routines
 * 	Add dlg_token_set
 * 	add memory management function parameters to sec_encode interfaces
 * 	[1994/04/29  21:51:59  ahop]
 * 
 * Revision 1.1.2.1  1994/01/28  23:11:37  burati
 * 	Include <dce/exc_handling.h>, not <exc_handling.h>
 * 	[1994/01/21  20:37:30  burati]
 * 
 * 	Initial version (dlg_bl1)
 * 	[1994/01/19  22:49:35  burati]
 * 
 * $EndLog$
 */

/* sec_encode.c
 * Copyright (c) Hewlett-Packard Company 1993, 1994, 1995
 * Unpublished work. All Rights Reserved.
 */

#if defined(SGIMIPS) && !defined(KERNEL)
#include <stdlib.h>
#endif
#include <rgymacro.h>
#include <sec_encode.h>
#include <dce/sec_attr_util.h>
#include <macros.h>
#ifdef KERNEL
#include <dce/ker/exc_handling.h>
#else
#include <dce/exc_handling.h>
#endif /*KERNEL*/
#include <dce/idl_es.h>
#include <dce/id_encode.h>
#include <dce/authz_encode.h>
#include <dce/sec_login_encode.h>
#include <sec_cred_internal.h>
#ifndef KERNEL
#include <dce/passwd_encode.h>
#include <dce/sec_pk_base.h>
#endif /*KERNEL*/

#define P(mutex) pthread_mutex_lock((&mutex))
#define V(mutex) pthread_mutex_unlock((&mutex))

/*
 * if function pointer is NULL use rpc_ss_client_free
 */
#define DEALLOCATE(deallocator,ptr) \
         { if (deallocator) (*(deallocator))((ptr)); else rpc_ss_client_free((ptr)); }

/* convenience typedefs */
typedef idl_void_p_t (*allocate_fn_t)(idl_size_t size);
typedef void (*free_fn_t)(idl_void_p_t ptr);

#ifdef KERNEL
#define SET_ALLOC_FREE(allocate,deallocate,stp) { \
    allocate_fn_t               old_allocate; \
    free_fn_t                   old_free; \
    boolean32                   swap_alloc_free; \
    rpc_ss_thread_handle_t      thread_handle = NULL; \
    idl_boolean                 enabled_allocator; \
    thread_handle = rpc_ss_get_thread_handle(); \
    if (thread_handle == NULL) { \
        enabled_allocator = idl_true; \
        rpc_ss_enable_allocate(); \
    } \
    else \
        enabled_allocator = idl_false; \
    if (swap_alloc_free = ((allocate) && (deallocate))) { \
        rpc_sm_swap_client_alloc_free((allocate), (deallocate), \
				      &old_allocate, &old_free, (stp)); \
    } \
    if (STATUS_OK((stp)))

#else /* ! KERNEL */
#define SET_ALLOC_FREE(allocate,deallocate,stp) { \
    allocate_fn_t               old_allocate; \
    free_fn_t                   old_free; \
    volatile boolean32                   swap_alloc_free; \
    volatile rpc_ss_thread_handle_t      thread_handle; \
    volatile idl_boolean                 enabled_allocator; \
    TRY \
        thread_handle = rpc_ss_get_thread_handle(); \
    CATCH(pthread_badparam_e); \
        thread_handle = NULL; \
    ENDTRY; \
    if (thread_handle == NULL) { \
        enabled_allocator = idl_true; \
        rpc_ss_enable_allocate(); \
    } \
    else \
        enabled_allocator = idl_false; \
    if (swap_alloc_free = ((allocate) && (deallocate))) { \
        rpc_sm_swap_client_alloc_free((allocate), (deallocate), \
				      &old_allocate, &old_free, (stp)); \
    } \
    if (STATUS_OK((stp)))

#endif /* #ELSE ! KERNEL */

#define RESET_ALLOC_FREE \
    if (swap_alloc_free) { \
        rpc_sm_set_client_alloc_free(old_allocate, old_free, (stp)); \
    }; \
    if (enabled_allocator) { \
        rpc_ss_disable_allocate(); \
    } \
}

/*
 * There may be a better way of enforcing error_status rather
 * than exception handling as the error detection mechanism
 * for encoding routines, but using TRY/CATCH seems to work,
 * so encapsulate that model in macros.
 */
#define ENCODE_TRY(stp) { \
    volatile error_status_t ENCODE_TRY_st; \
    CLEAR_STATUS(&ENCODE_TRY_st); \
    TRY {

#define ENCODE_ENDTRY \
    } CATCH_ALL { \
        error_status_t ENCODE_ENDTRY_lst = error_status_ok; \
        /*  \
	 * if the exception has been mapped to a status value, use that \
	 * as our return status. Otherwise, set our return status \
	 * to rpc_s_unknown_error \
	 */ \
	 ENCODE_TRY_st = exc_get_status(THIS_CATCH, &ENCODE_ENDTRY_lst) \
	                     ? rpc_s_unknown_error  : ENCODE_ENDTRY_lst; \
    } ENDTRY; \
    *(stp) =  ENCODE_TRY_st; \
    }

#ifndef _KERNEL
/*
 * encoding type identification routines
 */

/*
 * For each idl encoding interface, we define a table that
 * maps each operation in the interface into the corresponding
 * abstract encoding type.  Since operations in an idl interface
 * are identified by operation number, beginning with 0, a static zero origin
 * array of sec_encode_type_t is a convenient way to implement the table.
 * We can simply index into the array by op_num to obtain the abstract
 * encoding type.
 */

/* sec_id_encode interface (id_encode.idl) */
#define NUM_ID_EPAC_ENCODE_OPS 4
static sec_encode_type_t id_epac_encode_types[NUM_ID_EPAC_ENCODE_OPS] = {
    sec_encode_type_epac_data,      /* op 0 - id_epac_data_encode()     */
    sec_encode_type_epac,           /* op 1 - id_epac_encode()          */
    sec_encode_type_epac_set,       /* op 2 - id_epac_set_encode()      */
    sec_encode_type_dlg_token_set   /* op 3 - id_dlg_token_set_encode() */
};

/* sec_authz_encode interface (authz_encode.idl) */
#define NUM_AUTHZ_ENCODE_OPS 1
static sec_encode_type_t authz_encode_types[NUM_AUTHZ_ENCODE_OPS] = {
    sec_encode_type_v1_1_authz_data /* op 0 - v1_1_authz_data_encode()  */
};

#ifndef KERNEL
/* sec_passwd_encode interface (passwd_encode.idl) */
#define NUM_PASSWD_ENCODE_OPS 1
static sec_encode_type_t passwd_encode_types[NUM_PASSWD_ENCODE_OPS] = {
    sec_encode_type_pwd             /* op 0 - sec_pwd_encode()          */
};
#endif /*KERNEL*/

/*
 * Define a type to match the operation->encoding type tables above
 * with the correct idl encoding interface.
 */
typedef struct if_encoding_type_entry_t {
    rpc_if_handle_t         *if_handle;
    rpc_if_id_t             *if_id;
    unsigned16              num_ops;
    sec_encode_type_t       *type_vec;  /* vector of op_num -> encoding
                                           type mappings */
} if_encoding_type_entry_t;


/*
 * Define a table of encoding interfaces, each entry of which
 * maps an idl interface to a operation->sencoding_type table.
 *
 * To determine the abstract encoding type of an encoded buffer
 * the following steps are taken.
 *
 * 1. extract the interface id and operation number from the
 *    enccoded buffer.
 * 2. find the entry in the encoding interface table corresponding
 *    to the interface id extracted in (1)
 * 3. index into the type vector of the entry obtained in (2)
 *    using the operation number obtained in (1).  This yields
 *    the abstract encoding type.
 */
#ifndef KERNEL
#define NUM_ENCODING_IFS 3
#else
#define NUM_ENCODING_IFS 2
#endif /*KERNEL*/
static  if_encoding_type_entry_t  encoding_types[NUM_ENCODING_IFS] = {
    {
        &sec_id_encode_v0_0_c_ifspec,
        NULL,
	NUM_ID_EPAC_ENCODE_OPS,
	id_epac_encode_types
    },
    {
      &sec_authz_encode_v0_0_c_ifspec,
      NULL,
      NUM_AUTHZ_ENCODE_OPS,
      authz_encode_types
    }
#ifndef KERNEL
,
    {
        &sec_passwd_encode_v0_0_c_ifspec,
        NULL,
        NUM_PASSWD_ENCODE_OPS,
        passwd_encode_types
    }
#endif /*KERNEL*/
};

/*
 * At runtime, we need to walk the encoding_type vector
 * and initialize the if_id of each entry,  We perform the initialization
 * under a mutex, which is created under in a pthread_once routine.
 * with a pthread_init_once routine.
 */
static pthread_once_t   encoding_type_mutex_once = pthread_once_init;
static boolean32        encoding_type_mutex_inited = false;
static pthread_mutex_t  encoding_type_mutex;

/*
 * A "once" routine to create the encoding type
 * mutex
 */
static void init_encoding_type_mutex()
{
    if (!encoding_type_mutex_inited) {
        pthread_mutex_init(&encoding_type_mutex, pthread_mutexattr_default);
	encoding_type_mutex_inited = true;
    }
}


/*
**++
**
**  ROUTINE NAME:       init_encoding_type_entry
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**
**  Initialize the interface id field of an entry in the
**  global encoding types table.
**
**  INPUTS:
**
**      entry_p      An entry in the global encoding_types table.
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine:
**                      error_status_ok - OK
**                      sec_s_no_memory - unable to allocate storage
**
**                      any error status returned by rpc_if_inq_id()
**                      are passed back to the caller.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

static void init_encoding_type_entry (
    if_encoding_type_entry_t  *entry_p,
    error_status_t            *stp
)
{
    CLEAR_STATUS(stp);

    /*
     * make certain the mutex has been created before using it
     */
    if (!encoding_type_mutex_inited) {
	pthread_once(&encoding_type_mutex_once, init_encoding_type_mutex);
    }

    P(encoding_type_mutex);
    if (entry_p->if_id == NULL) {
	/*
         * Need to allocate storage for the if_id
         */
	entry_p->if_id = (rpc_if_id_t *) MALLOC(sizeof(*(entry_p->if_id)));
	if (entry_p->if_id == NULL) {
	    SET_STATUS(stp, sec_s_no_memory);
	} else {
	    /*
             * extract the if_id from the if_handle
             */
	    rpc_if_inq_id(*(entry_p->if_handle), entry_p->if_id, stp);
	    /*
             * There's absolutely no reason for rpc_if_inq_id
	     * to fail (the man page lists rpc_s_ok as the only
             * possible status return) but we check anyway and
             * restore the "uninitialized" state if anything went wrong
             */
	    if (!STATUS_OK(stp)) {
		free(entry_p->if_id);
		entry_p->if_id = NULL;
	    }
	}
    }
    V(encoding_type_mutex);
}


/*
**++
**
**  ROUTINE NAME:       sec_encode_get_encoding_type
**
**  SCOPE:              PUBLIC (to the security subsystem)
**
**  DESCRIPTION:
**
**  Given a buffer containing an encoded NDR data stream, identify
**  the abstract encoding type (i.e the operation used to
**  encode the data).
**
**  INPUTS:
**
**      num_bytes      number of bytes in the buffer
**      bytes          The encoded byte stream
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     sec_encode_type_t
**
**  The abtract encoding type of the operation used to
**  encode the bytes.  In the event that the encoding
**  type can not be determined (for any reason) the
**  return value is sec_encode_type_unknown.  The specific
**  cause of failure is not reported to the caller.
**
**  SIDE EFFECTS:
**
**  The global encoding_types table is initialized, if necessary.
**
**--
**/

sec_encode_type_t  sec_encode_get_encoding_type (
    unsigned32 num_bytes,  /* [in]  */
    idl_byte *bytes        /* [in] */
)
{
    sec_encode_type_t encoding_type = sec_encode_type_unknown;
    int               i;
    idl_es_handle_t   h;
    rpc_if_id_t       if_id;
    idl_ulong_int     op_num;
    error_status_t    st, ignore_st;

    /*
     * extract the encoding operation specification
     * from the caller's buffer so we can compare it to
     * the specifications known to us.
     */
    idl_es_decode_buffer(bytes, num_bytes, &h, &st);
    if (STATUS_OK(&st)) {
	idl_es_inq_encoding_id(h, &if_id, &op_num, &st);
	/* We're done with the decoding handle, in any event */
	idl_es_handle_free(&h, &ignore_st);
    }

    /*
     * If the attempt to extract the encoding operation specification
     * from the caller's buffer fails it is probably because the
     * caller's buffer is bad, so we just return sec_encode_type_unknown.
     */
    if (!STATUS_OK(&st)) {
	return encoding_type;
    }

    /*
     * Loop through our set of known encoding interfaces looking
     * for an interface spec and operation number that matches
     * the caller's
     */
    for (i = 0; i < NUM_ENCODING_IFS; i++) {
	/*
         * make certain this entry has been properly initialized
         */
	if (encoding_types[i].if_id == NULL) {
	    init_encoding_type_entry(&encoding_types[i], &st);
	    /*
             * if initialization of this entry failed (there is no
             * reason that it should, other than lack of basic
             * resources on the host), continue on, hoping that one of
             * the remaining entries (if any) will identify the encoding
	     * type.  If not, the caller will get a return value of
	     * sec_encode_type_unknown
             */
	    if (!STATUS_OK(&st)) {
		CLEAR_STATUS(&st);
		continue;
	    }
	}

	/*
	 * Now we  compare the if_id extracted from
	 * the caller's buffer with the if_id of the current
	 * encoding_type entry.  A succesfull match occurs if
	 * the interface uuids are equal and the major version
	 * numbers are equal.  We ignore the minor version number
	 * for the following reasons.  (1) As new operations are added
	 * to an interface, the semantics of existing operations
	 * *must* be preserved *unless* the major version number
	 * is changed. (2) As long as the major version number
	 * matches, all we care about is whether or not
	 * we know about the operation within that interface,
	 * because (1) guarantees that even if the minor version
	 * is difeferent, none of the operation we know about
	 * have changed.  And since we're obliged to deal explicitly
	 * with operation numbers in our application we'll find
	 * out whether or not the operation is known to us
	 * when we attempt to convert the operation number to
	 * to an abstract sec_encode_type_t.
	 */
	if (uuid_equal(&if_id.uuid, &encoding_types[i].if_id->uuid, &ignore_st)
	    && (if_id.vers_major == encoding_types[i].if_id->vers_major)) {
	    /*
	     * The interface in the current entry is compatible with
	     * the interface used to encode the callers buffer, so check
	     * the current entry's type vec for a match on the
	     * op_num obtained from the caller's buffer.
	     */
	    if (op_num < encoding_types[i].num_ops) {
		/*
		 * The caller's op_num is within the range of
		 * operations supported in our (minor) version of the
		 * interface, so map the op_num to the corresponding
		 * abstract encoding type.
		 */
		encoding_type = encoding_types[i].type_vec[op_num];
		break;
	    }
	}
    }
    return encoding_type;
}
#endif /* _KERNEL */


/*
**++
**
**  ROUTINE NAME:       malloc_shim
**
**  SCOPE:              PUBLIC within security
**
**  DESCRIPTION:
**
**  A simple malloc wrapper function that takes a size
**  argument of type idl_size_t and coerces it to
**  size_t in a call to the native malloc() routine.
**
**  Use this shim function when you want to pass the native
**  malloc to a routine that takes a memory allocation function
**  parameter whose signature is typed using idl_size_t.
**  In such cases, if you pass the native malloc directly and
**  a size_t is of a difference precision than an idl_size_t,
**  the compiler will issue a warning (a warning that, for
**  portability to architectures of arbitrary word size, should
**  be taken seriously).
**
**  INPUTS:
**
**      size            The number of bytes to be allocated
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     idl_void_p_t
**
**  pointer to allocated memory, or NULL, if the allocation failed
**  for any reason.
**
**  SIDE EFFECTS:       none
**
**--
**/

extern  idl_void_p_t malloc_shim (
    idl_size_t size
)
{
    /* just call the native malloc and coerce
     * the idl_size_t to a size_t.  This avoids any possible
     * precision problems.
     */

#ifdef KERNEL
    return MALLOC((unsigned int) size);
#else
    return MALLOC((size_t) size);
#endif
}


/*
 * Encoding/Decoding routines
 */
extern void sec_id_epac_data_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_data_t  *epac_data,                    /* [in] */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
)
{
    idl_es_handle_t         h;
    error_status_t          est = error_status_ok;

    *stp = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_data_encode(h, epac_data);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. otherwise we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


/*
 * sec_id_epac_data_decode
 */
extern void sec_id_epac_data_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in] */
    idl_byte            *bytes,                        /* [in] */
    sec_id_epac_data_t  *epac_data,                    /* [out] */
    error_status_t      *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_data_encode(h, epac_data);

	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. otherwise we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


/*
 * sec_id_epac_t routines
 */

extern void sec_id_epac_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_t       *epac,                         /* [in] */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_encode(h, epac);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. otherwise we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_id_epac_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,
    idl_byte            *bytes,
    sec_id_epac_t       *epac,
    error_status_t      *stp
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_encode(h, epac);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_id_epac_set_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_id_epac_set_t   *epac_set,
    unsigned32          *num_bytes,
    idl_byte            **bytes,
    error_status_t      *stp
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_set_encode(h, epac_set);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_id_epac_set_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,
    idl_byte            *bytes,
    sec_id_epac_set_t   *epac_set,
    error_status_t      *stp
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_epac_set_encode(h, epac_set);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_dlg_token_set_encode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_dlg_token_set_t *dlg_token_set,                /* [in] */
    unsigned32          *num_bytes,                    /* [out] */
    idl_byte            **bytes,                       /* [out] */
    error_status_t      *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_dlg_token_set_encode(h, dlg_token_set);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_dlg_token_set_decode(
    idl_void_p_t        (*alloc)(idl_size_t size),     /* [in] */
    void                (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32          num_bytes,                     /* [in] */
    idl_byte            *bytes,                        /* [in] */
    sec_dlg_token_set_t *dlg_token_set,                /* [out] */
    error_status_t      *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    id_dlg_token_set_encode(h, dlg_token_set);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_v1_1_authz_data_encode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_v1_1_authz_data_t *v1_1_authz_data,              /* [in] */
    unsigned32            *num_bytes,                    /* [out] */
    idl_byte              **bytes,                       /* [out] */
    error_status_t        *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    v1_1_authz_data_encode(h, v1_1_authz_data);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_v1_1_authz_data_decode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32            num_bytes,                     /* [in] */
    idl_byte              *bytes,                        /* [in] */
    sec_v1_1_authz_data_t *v1_1_authz_data,              /* [out] */
    error_status_t        *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    v1_1_authz_data_encode(h, v1_1_authz_data);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}

#ifndef KERNEL
extern void sec_pwd_encode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    sec_passwd_rec_t      *pwd_rec_p,                    /* [in] */
    unsigned32            *num_bytes,                    /* [out] */
    idl_byte              **bytes,                       /* [out] */
    error_status_t        *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_encode_dyn_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    pwd_encode(h, pwd_rec_p);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}


extern void sec_pwd_decode(
    idl_void_p_t          (*alloc)(idl_size_t size),     /* [in] */
    void                  (*dealloc)(idl_void_p_t ptr),  /* [in] */
    unsigned32            num_bytes,                     /* [in] */
    idl_byte              *bytes,                        /* [in] */
    sec_passwd_rec_t      *pwd_rec_p,                    /* [out] */
    error_status_t        *stp                           /* [out] */
)
{
    idl_es_handle_t h;
    error_status_t  est = error_status_ok;

    idl_es_decode_buffer(bytes, num_bytes, &h, stp);
    if (BAD_STATUS(stp)) {
        return;
    }

    SET_ALLOC_FREE(alloc, dealloc, &est) {
	ENCODE_TRY(&est) {
	    pwd_encode(h, pwd_rec_p);
	} ENCODE_ENDTRY;
    } RESET_ALLOC_FREE;

    idl_es_handle_free(&h, stp);

    /*
     * If the status of the encoding operation was bad, we report
     * that back. Otherwise, we report back the status of the
     * handle free operation.
     */
    if (BAD_STATUS(&est)) {
	*stp = est;
    }
}
#endif /*KERNEL*/

/*
 * Destructor functions for decoded types
 */

/*
 * The first set of functions are private for now, but
 * some of them may be become public if encoding routines
 * for that type are added to the interface.
 */
static void  sec_encode_id_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_t  *id
)
{
    if (id && id->name) {
	DEALLOCATE(dealloc, id->name);
	id->name = NULL;
    }
}


static void sec_encode_seal_set_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_seal_set_p_t  seal_set
)
{
    if (seal_set && seal_set->num_seals > 0 && seal_set->seals) {
	unsigned32     i;
	sec_id_seal_t  *sp = &seal_set->seals[0];

	for (i = 0; i < seal_set->num_seals; i++, sp++) {
	    if (sp->seal_len > 0 && sp->seal_data != NULL) {
		DEALLOCATE(dealloc, sp->seal_data);
		sp->seal_data = NULL;
                sp->seal_len = 0;
	    }
	}
	DEALLOCATE(dealloc, seal_set->seals);
	seal_set->seals = NULL;
	seal_set->num_seals = 0;
    }
}


static   void sec_encode_pa_data_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_pa_t  *pa
)
{
    if (pa) {
	sec_encode_id_free(dealloc, &pa->realm);
	sec_encode_id_free(dealloc, &pa->principal);
	sec_encode_id_free(dealloc, &pa->group);
	if (pa->num_groups > 0 && pa->groups) {
	    unsigned16 i;
	    for (i = 0; i < pa->num_groups; i++) {
		sec_encode_id_free(dealloc, &pa->groups[i]);
	    }
	    DEALLOCATE(dealloc, pa->groups);
	}
	if (pa->num_foreign_groupsets > 0 && pa->foreign_groupsets) {
	    unsigned16 i;
	    sec_id_foreign_groupset_t  *fgp = &pa->foreign_groupsets[0];

	    for (i = 0; i < pa->num_foreign_groupsets; i++, fgp++) {
		sec_encode_id_free(dealloc, &fgp->realm);
		if (fgp->num_groups > 0 && fgp->groups) {
		    unsigned16 j;

		    for (j = 0 ; j < fgp->num_groups; j++) {
			sec_encode_id_free(dealloc, &fgp->groups[j]);
		    }
		    DEALLOCATE(dealloc, fgp->groups);
		}
	    }
	    DEALLOCATE(dealloc, pa->foreign_groupsets);
	}
    }
}


extern  void  sec_encode_opt_req_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_opt_req_t  *res
)
{
    if (res && res->restriction_len > 0 && res->restrictions) {
	DEALLOCATE(dealloc, res->restrictions);
	res->restrictions = NULL;
	res->restriction_len = 0;
    }
}


extern  void  sec_encode_restriction_set_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_restriction_set_t *res_set
)
{
    if (res_set && res_set->num_restrictions > 0 && res_set->restrictions) {
	unsigned16 i;
	sec_id_restriction_t *rp = &res_set->restrictions[0];

	for (i = 0; i < res_set->num_restrictions; i++, rp++) {
	    switch (rp->entry_info.entry_type) {
	    case sec_rstr_e_type_user:
	    case sec_rstr_e_type_group:
	    case sec_rstr_e_type_foreign_other:
		sec_encode_id_free(dealloc, &rp->entry_info.tagged_union.id);
		break;

	    case sec_rstr_e_type_foreign_user:
	    case sec_rstr_e_type_foreign_group:
		sec_encode_id_free(dealloc,
		    &rp->entry_info.tagged_union.foreign_id.id);
		sec_encode_id_free(dealloc,
		    &rp->entry_info.tagged_union.foreign_id.realm);
	    }
	}
	res_set->num_restrictions = 0;
	DEALLOCATE(dealloc, res_set->restrictions);
	res_set->restrictions = NULL;
    }
}


extern void sec_encode_epac_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_epac_t  *epac
)
{
    if (epac) {
	if (epac->pickled_epac_data.num_bytes > 0
	&& epac->pickled_epac_data.bytes) {
	    DEALLOCATE(dealloc, epac->pickled_epac_data.bytes);
	    epac->pickled_epac_data.num_bytes = 0;
	    epac->pickled_epac_data.bytes = NULL;
	}

        if (epac->seals != NULL) {
            sec_encode_seal_set_free(dealloc, epac->seals);
            DEALLOCATE(dealloc, epac->seals);
            epac->seals = NULL;
        }
    }
}


extern void sec_encode_epac_set_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_epac_set_t  *epac_set
)
{
    if (epac_set && epac_set->num_epacs > 0) {
	unsigned32  i;
	
	for (i = 0; i < epac_set->num_epacs; i++) {
	    sec_encode_epac_free(dealloc, &epac_set->epacs[i]);
	}
	DEALLOCATE(dealloc, epac_set->epacs);
	epac_set->num_epacs = 0;
	epac_set->epacs = NULL;
    }
}


extern void sec_encode_epac_data_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_id_epac_data_t  *epac_data
)
{
    unsigned32  attr_index;
    if (epac_data) {
	sec_encode_pa_data_free(dealloc, &epac_data->pa);
	sec_encode_opt_req_free(dealloc, &epac_data->opt_restrictions);
	sec_encode_opt_req_free(dealloc, &epac_data->req_restrictions);
	sec_encode_restriction_set_free(dealloc, &epac_data->deleg_restrictions);
	sec_encode_restriction_set_free(dealloc, &epac_data->target_restrictions);
#ifdef KERNEL
	/*
	 * Extended attrs should have been stripped in user space
	 */
	(!(epac_data->num_attrs == 0) ?
		(printf ("assertion failed: line %d, file %s\n",
		 __LINE__,__FILE__), panic("assert"), 0) : 0);
#else
	for (attr_index = 0; attr_index < epac_data->num_attrs; attr_index++) {
	    /*
             * need to choose a deallocator because sec_attr_util_free
             * uses a different default deallocator than we do
             */
	    sec_attr_util_free((dealloc ? dealloc : rpc_ss_client_free),
			       &epac_data->attrs[attr_index]);
	}
	if (epac_data->num_attrs > 0) {
	    DEALLOCATE(dealloc, epac_data->attrs );
	    epac_data->num_attrs = 0;
	    epac_data->attrs = NULL;
	}
#endif /*KERNEL*/
    }
}


void  sec_encode_dlg_token_set_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_dlg_token_set_t  *ts
)
{
    if (!ts) return;

    if (ts->num_tokens && ts->tokens) {
	unsigned32 i;

	for (i = 0; i < ts->num_tokens; i++) {
	    if (ts->tokens[i].token_bytes.num_bytes
            && ts->tokens[i].token_bytes.bytes) {
		DEALLOCATE(dealloc, ts->tokens[i].token_bytes.bytes);
		ts->tokens[i].token_bytes.bytes = NULL;
		ts->tokens[i].token_bytes.num_bytes = 0;
	    }
	}
	DEALLOCATE(dealloc, ts->tokens);
	ts->tokens = NULL;
	ts->num_tokens = 0;
    }
}


void  sec_encode_v1_1_authz_data_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_v1_1_authz_data_t  *azd
)
{
    if (!azd) return;

    if (azd->seals) {
	sec_encode_seal_set_free(dealloc, azd->seals);
	DEALLOCATE(dealloc, azd->seals);
	azd->seals = NULL;
   }
    if (azd->deleg_tokens) {
	sec_encode_dlg_token_set_free(dealloc, azd->deleg_tokens);
	DEALLOCATE(dealloc, azd->deleg_tokens);
	azd->deleg_tokens = NULL;
    }
    if (azd->extended_info
    && azd->extended_info->num_bytes
    && azd->extended_info->bytes) {
	DEALLOCATE(dealloc, azd->extended_info->bytes);
	azd->extended_info->bytes = NULL;
	azd->extended_info->num_bytes = 0;
	DEALLOCATE(dealloc, azd->extended_info);
        azd->extended_info = NULL;
    }
}


extern  void  sec_encode_v1_1_lc_info_free (
    void (*dealloc)(idl_void_p_t ptr),
    sec_login__v1_1_info_t    *v1_1_lc_info
)
{
    if (!v1_1_lc_info) {
	return;
    }

    if (v1_1_lc_info->epac_chain.bytes
    && v1_1_lc_info->epac_chain.num_bytes) {
	DEALLOCATE(dealloc, v1_1_lc_info->epac_chain.bytes);
    }

    if (v1_1_lc_info->dlg_req_type == sec__login_c_dlg_req_traced
    || v1_1_lc_info->dlg_req_type == sec__login_c_dlg_req_imp) {
	sec__login_dlg_req_info_t *drip = &v1_1_lc_info->dlg_req_info.info;

	if (drip->dlg_chain.num_bytes && drip->dlg_chain.bytes) {
	    DEALLOCATE(dealloc, drip->dlg_chain.bytes);
	}
	sec_encode_dlg_token_set_free(dealloc, &drip->dlg_token_set);
	sec_encode_restriction_set_free(dealloc, &drip->dlg_rstrs);
	sec_encode_restriction_set_free(dealloc, &drip->tgt_rstrs);
	sec_encode_opt_req_free(dealloc, &drip->opt_rstrs);
	sec_encode_opt_req_free(dealloc, &drip->req_rstrs);
    }
}


extern void sec_encode_buffer_free (
    void (*dealloc)(idl_void_p_t ptr),
    idl_byte  **bytes
)
{
    if (bytes != NULL && *bytes != NULL) {
	DEALLOCATE(dealloc, *bytes);
	*bytes = NULL;
    }
}

#ifndef KERNEL
extern void sec_encode_pwd_free(
    void (*dealloc)(idl_void_p_t ptr),
    sec_passwd_rec_t      *pwd_rec_p
)
{
    if (!pwd_rec_p) return;
    if (pwd_rec_p->pepper) {
        DEALLOCATE(dealloc, pwd_rec_p->pepper);
        pwd_rec_p->pepper=NULL;
    }
    switch (PASSWD_TYPE(pwd_rec_p)) {
        case sec_passwd_plain:
            if (pwd_rec_p->key.tagged_union.plain) {
                DEALLOCATE(dealloc, pwd_rec_p->key.tagged_union.plain);
                pwd_rec_p->key.tagged_union.plain=NULL;
            }
            break;
        case sec_passwd_pubkey:
            if (pwd_rec_p->key.tagged_union.pub_key.data) {
                DEALLOCATE(dealloc, pwd_rec_p->key.tagged_union.pub_key.data);
                pwd_rec_p->key.tagged_union.pub_key.data=NULL;
                pwd_rec_p->key.tagged_union.pub_key.len=0;
            }
            break;
        case sec_passwd_genprivkey:
            break;
        default:
            break;
    }
}
#endif /*KERNEL*/

#ifndef KERNEL
void sec__encode_strip_undesirables(
    sec_bytes_t    *epac_setp,
    error_status_t *stp
)
{
    sec_id_epac_set_t  epac_set;
    sec_id_epac_data_t epac_data;
    int                i, j;

    /* Don't do anything if there's no data to operate on */
    if (epac_setp && (epac_setp->num_bytes > 0) && epac_setp->bytes) {

	/* Decode the EPAC set */
	sec_id_epac_set_decode(malloc_shim, FREE_PTR, epac_setp->num_bytes,
	    epac_setp->bytes, &epac_set, stp);
	if (BAD_STATUS(stp))
	    return;

	/* Process each EPAC in the set separately */
	for (i = 0; GOOD_STATUS(stp) && (i < epac_set.num_epacs); i++) {

	    /* Decode an EPAC */
	    sec_id_epac_data_decode(malloc_shim, FREE_PTR,
		epac_set.epacs[i].pickled_epac_data.num_bytes,
		epac_set.epacs[i].pickled_epac_data.bytes, &epac_data, stp);
	    if (BAD_STATUS(stp)) {
		sec_encode_epac_set_free(FREE_PTR, &epac_set);
		return;
	    }

	    /* Don't bother doing anything to this EPAC if nothing to strip */
	    if ((epac_data.num_attrs == 0) &&
		(epac_data.opt_restrictions.restriction_len == 0) &&
		(epac_data.req_restrictions.restriction_len == 0) &&
		(epac_data.deleg_restrictions.num_restrictions == 0)) {
		sec_encode_epac_data_free(FREE_PTR, &epac_data);
		continue;
	    }

	    /* If any attrs, free them, then remove them from the epac */
	    if (epac_data.num_attrs > 0) {
                for (j = 0; j < epac_data.num_attrs; j++) {
		    sec_attr_util_free(FREE_PTR, &epac_data.attrs[j]);
		}
		DEALLOCATE(FREE_PTR, epac_data.attrs );
		epac_data.num_attrs = 0;
		epac_data.attrs = NULL;
	    }

	    /* Free any opt/req/deleg restrictions */
	    sec_encode_opt_req_free(FREE_PTR, &epac_data.opt_restrictions);
	    sec_encode_opt_req_free(FREE_PTR, &epac_data.req_restrictions);
	    sec_encode_restriction_set_free(FREE_PTR,
		&epac_data.deleg_restrictions);

	    /* Free the old EPAC data at this index in the array */
	    FREE(epac_set.epacs[i].pickled_epac_data.bytes);
	    epac_set.epacs[i].pickled_epac_data.bytes = NULL;
	    epac_set.epacs[i].pickled_epac_data.num_bytes = 0;

	    /* Encode the newly trimmed down EPAC data */
	    sec_id_epac_data_encode(malloc_shim, FREE_PTR, &epac_data,
		&epac_set.epacs[i].pickled_epac_data.num_bytes,
		&epac_set.epacs[i].pickled_epac_data.bytes, stp);

	    sec_encode_epac_data_free(FREE_PTR, &epac_data);
	}

	/* Re-encode the EPAC set */
	if (GOOD_STATUS(stp)) {
	    FREE(epac_setp->bytes);
	    epac_setp->num_bytes = 0;
	    sec_id_epac_set_encode(malloc_shim, FREE_PTR, &epac_set,
		&epac_setp->num_bytes, &epac_setp->bytes, stp);
	}
	sec_encode_epac_set_free(FREE_PTR, &epac_set);
    }
}
#endif
