/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sec_authn_krpc.c,v 65.10 1999/02/05 15:44:20 mek Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: sec_authn_krpc.c,v $
 * Revision 65.10  1999/02/05 15:44:20  mek
 * Add sec_des_is_weak_key() as a method to call des_is_weak_key() when
 * using external DES symbols.
 *
 * Revision 65.9  1999/02/04 22:37:19  mek
 * Support for external DES library.
 *
 * Revision 65.8  1999/01/19 20:28:15  gwehrman
 * Replaced include of krb5/des_inline.h with kdes_inline.h for kernel builds.
 * Removed SGIMIPS ifdefs inside SGIMIPS_DES_INLINE ifdefs.
 *
 * Revision 65.7  1998/04/01 14:16:57  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed type cast of idl_malloc_krpc as a void pointer in calls
 * 	to sec_id_pac_t_unpickle.  The data type already matches the
 * 	expected parameter type.  When using server_name in a READ_NAME()
 * 	call, cast it as a unsigned char pointer pointer instead of
 * 	a char pointer pointer to match the expected parameter type.
 *
 * Revision 65.6  1998/03/23  17:26:30  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/01/07 17:20:49  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.4  1997/12/16  17:20:11  lmc
 * Changed the way we get the current uid and the users credentials for kudzu.
 * There is no longer a "u" structure.  Also prototyped functions so that we
 * wouldn't get warning messages and changed an int to error_status_t.
 *
 * Revision 65.3  1997/11/06  19:56:43  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/27  17:17:59  jdoak
 * 6.4 + 1.2.2 merge - inline des code
 *
 * Revision 65.1  1997/10/20  19:16:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.2.71.2  1996/08/09  12:02:26  arvind
 * 	u2u changes
 * 	[1996/07/10  17:18 UTC  burati  /main/DCE_1.2.2/mb_u2u2/2]
 *
 * 	Fix errors
 * 	[1996/07/10  17:18 UTC  burati  /main/DCE_1.2.2/mb_u2u2/2]
 *
 * 	[1996/07/10  15:00 UTC  burati  /main/DCE_1.2.2/mb_u2u2/1]
 *
 * 	u2u code cleanup changes
 * 	[1996/05/24  23:11 UTC  sommerfeld  /main/DCE_1.2.2/1]
 *
 * Revision 1.2.71.1  1996/06/04  21:53:17  arvind
 * 	Merge u2u signature change to sec_krb_get_cred() from mb_u2u
 * 	[1996/05/07  17:00 UTC  burati  /main/DCE_1.2/2]
 * 
 * 	u2u changes
 * 	[1996/05/03  14:41 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 * 
 * 	Fix typo for OSF DCE 1.2.1 drop to be built on IBM
 * 	[1995/11/07  19:14 UTC  psn  /main/HPDCE02/jrr_1.2_mothra/1]
 * 
 * 	merge Wallnut Creek port to HPDCE02 mainline
 * 	[1995/09/14  19:46 UTC  maunsell_c  /main/HPDCE02/2]
 * 
 * 	merge out current HPDCE02 version to merge back in Walnut Creek port
 * 	[1995/09/14  19:33 UTC  maunsell_c  /main/maunsell_WC_port/8]
 * 
 * 	remove extra #include for krpc_osi_mach.h
 * 	[1995/09/14  13:11 UTC  maunsell_c  /main/maunsell_WC_port/7]
 * 
 * 	ifdef use of kprc_osi_mach.h so other platform
 * 	builds will still work
 * 	[1995/08/22  18:56 UTC  maunsell_c  /main/maunsell_WC_port/6]
 * 
 * 	use krpc_ vs osi_ calls
 * 	[1995/08/22  17:47 UTC  maunsell_c  /main/maunsell_WC_port/5]
 * 
 * 	need osi_getucred w/osi_GetUID() use
 * 	[1995/08/09  17:07 UTC  maunsell_c  /main/maunsell_WC_port/4]
 * 
 * 	use osi_xxx calls vs. kthread_iface ones...
 * 	[1995/08/08  20:17 UTC  maunsell_c  /main/maunsell_WC_port/3]
 * 
 * 	walnut creek port
 * 	[1995/07/18  16:46 UTC  maunsell_c  /main/maunsell_WC_port/2]
 * 
 * Revision 1.2.69.2  1996/02/18  00:00:50  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:53  marty]
 * 
 * Revision 1.2.69.1  1995/12/08  00:15:28  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/07  19:14 UTC  psn
 * 	Fix typo for OSF DCE 1.2.1 drop to be built on IBM
 * 
 * 	HP revision /main/HPDCE02/2  1995/09/14  19:46 UTC  maunsell_c
 * 	merge Wallnut Creek port to HPDCE02 mainline
 * 
 * 	HP revision /main/maunsell_WC_port/8  1995/09/14  19:33 UTC  maunsell_c
 * 	merge out current HPDCE02 version to merge back in Walnut Creek port
 * 
 * 	HP revision /main/HPDCE02/1  1995/09/01  14:57 UTC  sommerfeld
 * 	Remove misleading comment at request of Mike Burati.
 * 	[1995/08/29  22:08 UTC  sommerfeld  /main/sommerfeld_dfsperf/3]
 * 
 * 	sec_krb_ccache_free tweaks:
 * 	reduce locking window (free memory after releasing lock)
 * 	add test to assertion (to avoid faulting in assertion in the most likely
 * 	failure case..)
 * 	[1995/07/30  16:03 UTC  sommerfeld  /main/sommerfeld_CHFts15483/2]
 * 
 * 	Add caching of ccache handles to allow more memory sharing in krpc as
 * 	used by DFS..
 * 	[1995/07/29  07:58 UTC  sommerfeld  /main/sommerfeld_CHFts15483/1]
 * 
 * 	HP revision /main/maunsell_WC_port/7  1995/09/14  13:11 UTC  maunsell_c
 * 	remove extra #include for krpc_osi_mach.h
 * 
 * 	HP revision /main/maunsell_WC_port/6  1995/08/22  18:56 UTC  maunsell_c
 * 	ifdef use of kprc_osi_mach.h so other platform
 * 	builds will still work
 * 
 * 	HP revision /main/maunsell_WC_port/5  1995/08/22  17:47 UTC  maunsell_c
 * 	use krpc_ vs osi_ calls
 * 
 * 	HP revision /main/maunsell_WC_port/4  1995/08/09  17:07 UTC  maunsell_c
 * 	need osi_getucred w/osi_GetUID() use
 * 
 * 	HP revision /main/maunsell_WC_port/3  1995/08/08  20:17 UTC  maunsell_c
 * 	use osi_xxx calls vs. kthread_iface ones...
 * 
 * 	HP revision /main/maunsell_WC_port/2  1995/07/18  16:46 UTC  maunsell_c
 * 	walnut creek port
 * 	[1995/12/07  23:56:32  root]
 * 
 * Revision 1.2.67.10  1994/09/30  15:52:08  rsarbo
 * 	pass authz_fmt up to dfsbind in sec_krb_dg_decode_message_kern
 * 	[1994/09/30  15:49:01  rsarbo]
 * 
 * Revision 1.2.67.9  1994/08/24  21:34:32  burati
 * 	CR11861 Write authz_fmt out to userspace in sec_krb_dg_build_message
 * 	[1994/08/24  21:24:32  burati]
 * 
 * Revision 1.2.67.8  1994/08/09  17:32:33  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  17:04:16  burati]
 * 
 * Revision 1.2.67.7  1994/07/26  15:48:43  burati
 * 	   Big auth info / DFS EPAC work
 * 	   [1994/07/21  18:39:22  burati]
 * 
 * Revision 1.2.67.6  1994/06/02  20:41:42  mdf
 * 	Merged with changes from 1.2.67.5
 * 	[1994/06/02  20:41:31  mdf]
 * 
 * 	Merged with changes from 1.2.67.4
 * 	[1994/06/02  20:11:13  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:12:47  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.2.4.4  1993/01/03  22:36:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:34  bbelch]
 * 
 * Revision 1.2.4.3  1992/12/23  19:40:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:19  zeliff]
 * 
 * Revision 1.2.4.2  1992/12/03  22:03:40  delgado
 * 	New kprc helper interface.
 * 	[1992/12/03  22:02:51  delgado]
 * 
 * Revision 1.2.2.2  1992/05/01  17:56:54  rsalz
 * 	 27-apr-92 nm    Add/fix casts for gcc (via toml)
 * 	[1992/05/01  17:51:17  rsalz]
 * 
 * Revision 1.2  1992/01/19  22:13:37  devrcs
 * 	Dropping DCE1.0 OSF1_misc port archive
 * 
 * Revision 1.2.67.4  1994/06/02  19:16:55  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:12:47  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.2.4.4  1993/01/03  22:36:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:34  bbelch]
 * 
 * Revision 1.2.4.3  1992/12/23  19:40:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:19  zeliff]
 * 
 * Revision 1.2.4.2  1992/12/03  22:03:40  delgado
 * 	New kprc helper interface.
 * 	[1992/12/03  22:02:51  delgado]
 * 
 * Revision 1.2.2.2  1992/05/01  17:56:54  rsalz
 * 	 27-apr-92 nm    Add/fix casts for gcc (via toml)
 * 	[1992/05/01  17:51:17  rsalz]
 * 
 * Revision 1.2  1992/01/19  22:13:37  devrcs
 * 	Dropping DCE1.0 OSF1_misc port archive
 * 
 * Revision 1.2.67.5  1994/06/02  20:19:49  mdf
 * 	Merged with changes from 1.2.67.4
 * 	[1994/06/02  20:11:13  mdf]
 * 
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:12:47  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.2.4.4  1993/01/03  22:36:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:34  bbelch]
 * 
 * Revision 1.2.4.3  1992/12/23  19:40:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:19  zeliff]
 * 
 * Revision 1.2.4.2  1992/12/03  22:03:40  delgado
 * 	New kprc helper interface.
 * 	[1992/12/03  22:02:51  delgado]
 * 
 * Revision 1.2.2.2  1992/05/01  17:56:54  rsalz
 * 	 27-apr-92 nm    Add/fix casts for gcc (via toml)
 * 	[1992/05/01  17:51:17  rsalz]
 * 
 * Revision 1.2  1992/01/19  22:13:37  devrcs
 * 	Dropping DCE1.0 OSF1_misc port archive
 * 
 * Revision 1.2.67.4  1994/06/02  19:16:55  mdf
 * 	hp_sec_to_osf_3 drop, merge up with latest.
 * 	[1994/05/24  15:12:47  mdf]
 * 
 * 	hp_sec_to_osf_3 drop
 * 
 * Revision 1.2.4.4  1993/01/03  22:36:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:34  bbelch]
 * 
 * Revision 1.2.4.3  1992/12/23  19:40:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:19  zeliff]
 * 
 * Revision 1.2.4.2  1992/12/03  22:03:40  delgado
 * 	New kprc helper interface.
 * 	[1992/12/03  22:02:51  delgado]
 * 
 * Revision 1.2.2.2  1992/05/01  17:56:54  rsalz
 * 	 27-apr-92 nm    Add/fix casts for gcc (via toml)
 * 	[1992/05/01  17:51:17  rsalz]
 * 
 * Revision 1.2  1992/01/19  22:13:37  devrcs
 * 	Dropping DCE1.0 OSF1_misc port archive
 * 
 * Revision 1.2.67.3  1994/02/02  20:55:39  cbrooks
 * 	Change argument types via cast/declaration
 * 	[1994/02/02  20:48:02  cbrooks]
 * 
 * Revision 1.2.67.2  1994/01/28  23:09:18  burati
 * 	Big EPAC work - add client_creds to sec_krb_dg_decode_message (dlg_bl1)
 * 	[1994/01/24  21:16:17  burati]
 * 
 * Revision 1.2.67.1  1994/01/21  22:32:31  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:39  cbrooks]
 * 
 * Revision 1.2.4.4  1993/01/03  22:36:55  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:34  bbelch]
 * 
 * Revision 1.2.4.3  1992/12/23  19:40:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:19  zeliff]
 * 
 * Revision 1.2.4.2  1992/12/03  22:03:40  delgado
 * 	New kprc helper interface.
 * 	[1992/12/03  22:02:51  delgado]
 * 
 * Revision 1.2.2.2  1992/05/01  17:56:54  rsalz
 * 	 27-apr-92 nm    Add/fix casts for gcc (via toml)
 * 	[1992/05/01  17:51:17  rsalz]
 * 
 * Revision 1.2  1992/01/19  22:13:37  devrcs
 * 	Dropping DCE1.0 OSF1_misc port archive
 * 
 * $EndLog$
 */
/*
 *  OSF DCE Version 1.1
 */
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**  Copyright (c) 1994 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**  NAME
**
**      sec_authn_krpc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Implementation of sec_authn interface for kernel clients.
**  
**  Some of the routines implemented in this module call up to a 
**  user-space helper process to do all the "real" work; these routines are
**  effectively hand-coded "client stubs", which have to match the 
**  hand-coded server stubs in src/security/helper/auth_helper.c .
** 
**  If you change any of the stubs or the marshalling machinery in this file,
**  you have to change auth_helper.c as well; if you make incompatible changes
**  to the helper protocol, you *must* arrange for a new version of the helper
**  process to be installed at the same time the new kernel or kernel 
**  extension is installed.
**
**  Other routines in this file implement things which can be done directly
**  in the kernel.  Name parsing in the kernel is fudged (and deferred up to 
**  user space), where the helper process maintains a cache of recently used
**  security information.
**
**
*/

#include <commonp.h>
#include <com.h>
#include <dce/ker/krpc_helper.h>
#include <idl_ss_krpc.h>
#include <md5_krpc.h>
#include <dce/sec_authn.h>
#include <dce/secsts.h>
#include <sec_pkl_krpc.h>
#ifndef SGIMIPS
#include <sys/user.h>
#endif
#include <../../security/h/sec_cred_internal.h>
#ifdef HPUX
#include <dce/ker/krpc_osi_mach.h>
#else
#ifdef SGIMIPS
#define krpc_osi_GetUID(c)                   get_current_cred()->cr_uid
#define krpc_osi_getucred()                  curuthread->ut_cred
#else
#define krpc_osi_GetUID(c)           (c->cr_uid)
#define krpc_osi_getucred()                  u.u_cred
#endif
#endif


typedef struct ccache_handle 
{
    unsigned32 pag;
    unsigned32 uid;
    int refcount;
    struct ccache_handle *next;
} cch, *cchp;

cchp sec_krb_cred_list = NULL;
static rpc_mutex_t credlist_lock;
#ifdef SGIMIPS
#include <des.h>
#endif

#define SEC_KRB_GET_PAC 1                       /* OBSOLETE */
#define SEC_KRB_GET_CC 2                        /* OBSOLETE */
#define SEC_KRB_GET_CRED 3
#define SEC_KRB_GET_NAME 4                      /* OBSOLETE */
#define SEC_KRB_BUILD_MESSAGE 5                 /* OBSOLETE */
#define SEC_KRB_DECODE_MESSAGE 6                /* OBSOLETE */
#define SEC_KRB_REGISTER_SERVER 7
#define SEC_KRB_GEN_KEY 8
#define SEC_KRB_GEN_BLOCK 9
#define SEC_KRB_DG_BUILD_MESSAGE 10
#define SEC_KRB_DG_DECODE_MESSAGE 11

#define SETUP(h, opcode) \
    {            \
       error_status_t st = 0;  \
       struct krpc_helper *h = krpc_GetHelper(opcode); \
       int opcode_1 = opcode; \
       RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(sec_auth) setup opcode %d\n", opcode)); \
       if (!h) { \
           st = rpc_s_helper_not_running; \
       } else { \
           WRITE_LONG(h, opcode); \

#define TRANSCEIVE(h) 

#define FINISH(h) \
           out: krpc_PutHelper(h); \
       } \
       RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(sec_auth) done opcode %d, st %d\n", opcode_1, st)); \
       return st; \
    }

#define WRITE_UUID(h, uuid) \
    { \
    krpc_WriteHelper(h, (unsigned_char_p_t)uuid, sizeof(uuid_t)); \
    }

#ifdef SGIMIPS
#define WRITE_LONG(h, i) \
    { \
    signed32 _i = (i);  \
    krpc_WriteHelper(h, (unsigned_char_p_t)&_i, sizeof(signed32)); \
    }
#else
#define WRITE_LONG(h, i) \
    { \
    long _i = (i);  \
    krpc_WriteHelper(h, (unsigned_char_p_t)&_i, sizeof(long)); \
    }
#endif /* SGIMIPS */

#ifdef SGIMIPS
#define READ_LONG(h, i) \
    { \
        unsigned32 _i; \
        st = krpc_ReadHelper(h, (unsigned_char_p_t)&_i, \
            sizeof(unsigned32), (long *)NULL); \
        if (st) goto out; \
        i = _i; \
    }
#else
#define READ_LONG(h, i) \
    { \
        long _i; \
        st = krpc_ReadHelper(h, (unsigned_char_p_t)&_i, \
            sizeof(long), (long *)NULL); \
        if (st) goto out; \
        i = _i; \
    }
#endif /* SGIMIPS */

#define READ_PAC(h, pac) \
   { st = sec_read_pac(h, pac); if (st) goto out; }

#define WRITE_PAC(h, pac) \
   { sec_write_pac(h, pac); }  

#define READ_NAME(h, name) \
    { st = sec_read_string(h, name); if (st) goto out; }
     
#define WRITE_NAME(h, name) \
    { sec_write_string(h, name); }
  
#define READ_CRED(h, cred) \
    { st = sec_read_cred(h, cred); if (st) goto out; }    

#define WRITE_CRED(h, cred) \
    { sec_write_cred(h, cred); }

#define READ_MESSAGE(h, message) \
{ \
    int _len; \
    READ_LONG(h, _len); \
    message->length = _len; \
    RPC_MEM_ALLOC(message->data, unsigned_char_p_t, message->length, \
        RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK); \
    st = krpc_ReadHelper(h, message->data, message->length, (long *)NULL); \
    if (st) goto out; \
}

#define WRITE_MESSAGE(h, message) \
  { \
      WRITE_LONG(h, message->length); \
      krpc_WriteHelper(h, message->data, message->length); \
  }

#define READ_BLOCK(h, block) \
   { \
      st = krpc_ReadHelper(h, (unsigned_char_p_t)block, \
          sizeof(sec_des_block), (long *)NULL); \
      if (st) goto out; \
   }
      
#define WRITE_BLOCK(h, block) \
   {  \
      krpc_WriteHelper(h, block, sizeof(sec_des_block)); \
   }

#define READ_SEC_BYTES(h, data) \
{ \
    int _len; \
    READ_LONG(h, _len); \
    (data)->num_bytes = _len; \
    RPC_MEM_ALLOC((data)->bytes, unsigned_char_p_t, (data)->num_bytes, \
        RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK); \
    st = krpc_ReadHelper(h, (data)->bytes, (data)->num_bytes, (long *)NULL); \
}

/*
 * Pass up the "process authentication group".
 */

#define WRITE_IDH(h, ai) \
    { unsigned32 *pagp = (unsigned32 *) ai; \
      WRITE_LONG(h, pagp[0]); \
      WRITE_LONG(h, pagp[1]); \
    }

/*
 * The local rep of a ccache is an array of two unsigned32 (the "pag",
 * and the "euid").
 * 
 * The local rep of a cred is a structure containing a copy of the parameters 
 * used to pick it out.
 */

typedef struct {
    unsigned32 pag;
    unsigned32 euid;
    unsigned   char *server_name;
    unsigned32 authn_level;
    unsigned32 authz_proto;
#ifdef SGIMIPS
    unsigned32 expiration;
#else
    long       expiration;
#endif /* SGIMIPS */
} sec_krb_cred_rep;

static unsigned_char_p_t first_server_name;

/* 
 * Read string from helper.
 */

error_status_t sec_read_string (
    struct krpc_helper *h,
    unsigned char **string
)
{
    int len, st;
    unsigned char *s;
    
    READ_LONG(h, len);

    RPC_MEM_ALLOC (s, unsigned char *, len,
        RPC_C_MEM_STRING, RPC_C_MEM_WAITOK);
    st = krpc_ReadHelper (h, s, len, (long *)NULL);
    if (st) {
        RPC_MEM_FREE(s, RPC_C_MEM_STRING);
    out:
        *string = 0;
        return st;
    }
    *string = s;
    return 0;
}

/* 
 * Write string to helper process.
 */

void sec_write_string (
    struct krpc_helper *h,
    unsigned char *string
)
{
    if (string == NULL) {
        WRITE_LONG(h, 1);
        krpc_WriteHelper(h, (unsigned char *)"", 1);   
    } else {
#ifdef SGIMIPS
	/*  len will never be bigger than 32bits.  */
        int len = (int)strlen ((char *)string) + 1;
#else
        int len = strlen ((char *)string) + 1;
#endif
        WRITE_LONG(h, len);
        krpc_WriteHelper(h, string, len);
    }
}

/* 
 * Read (pickled) pac from helper and unpickle.
 */

error_status_t sec_read_pac (
    struct krpc_helper *h,
    sec_id_pac_t *pac
)
{
    int length;
    unsigned32 st;
    unsigned8 (*temp)[] = 0;
    
    READ_LONG (h, length);
    RPC_MEM_ALLOC (temp, unsigned8 (*)[], length,
        RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);

    st = krpc_ReadHelper(h, (unsigned char *)temp, length, (long *)NULL);
    if (st) goto out;
#ifdef SGIMIPS
    sec_id_pac_t_unpickle (temp, idl_malloc_krpc, pac, &st);
#else
    sec_id_pac_t_unpickle (temp, (void *)idl_malloc_krpc, pac, &st);
#endif
    out:
    if (temp)
    {
        RPC_MEM_FREE (temp, RPC_C_MEM_UTIL);
    }
    return st;
    
}

/* 
 * Pickle pac, write byte string to helper, and free pickle.
 */

void sec_write_pac (
    struct krpc_helper *h,
    sec_id_pac_t *pac
)
{
    unsigned8 (*pickle)[];
    unsigned32 len;
    error_status_t st;
    
#ifdef SGIMIPS
    sec_id_pac_t_pickle(pac, 0, idl_malloc_krpc, 8, &pickle, &len, &st);
#else
    sec_id_pac_t_pickle(pac, 0, (void *)idl_malloc_krpc, 8, &pickle, &len, &st);
#endif
    if (st != 0)
        panic("sec_write_pac");
    
    WRITE_LONG (h, len);
    krpc_WriteHelper(h, (unsigned char *) pickle, len);
    idl_free_krpc((void *)pickle);
}

/* 
 * Read the over-the-firewall representation of a credential from 
 * user space.
 */

error_status_t sec_read_cred (
    struct krpc_helper *h,
    sec_krb_cred *cred
)
{
    sec_krb_cred_rep *credrep;
#ifdef SGIMIPS
    error_status_t st = 0;
#else
    long st = 0;
#endif

    *cred = 0;
    RPC_MEM_ALLOC (credrep, sec_krb_cred_rep *, sizeof (*credrep),
                   RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);

    credrep->server_name = NULL;

    *cred = (sec_krb_cred) credrep;

    READ_LONG (h, credrep->pag);
    READ_LONG (h, credrep->euid);
    READ_LONG (h, credrep->authn_level);
    READ_LONG (h, credrep->authz_proto);
    READ_LONG (h, credrep->expiration);
    READ_NAME (h, &credrep->server_name);

out:
    if (st) {
        sec_krb_cred_free (cred);
    }
    return st;
}

/* 
 * Write the over-the-firewall representation of a credential to 
 * user space.
 */

void sec_write_cred (
    struct krpc_helper *h,
    sec_krb_cred cred
)
{
    sec_krb_cred_rep *credrep = (sec_krb_cred_rep *) cred;

    if (credrep == NULL) 
      {
        WRITE_LONG(h, 0);
        WRITE_LONG(h, 0);
        WRITE_LONG(h, 0);
        WRITE_LONG(h, 0);
        WRITE_LONG(h, 0);
        WRITE_NAME(h, 0);
      }
    else 
      {
        WRITE_LONG (h, credrep->pag);
        WRITE_LONG (h, credrep->euid);
        WRITE_LONG (h, credrep->authn_level);
        WRITE_LONG (h, credrep->authz_proto);
        WRITE_LONG (h, credrep->expiration);
        WRITE_NAME (h, credrep->server_name);
      }
}    

/*
 * Initialize the world.  We dont have much in the way of state here; 
 * just initialize the helper mechanism.
 */

void sec_krb_init( void )
{
    krpc_helper_init();
    RPC_MUTEX_INIT(credlist_lock);
}

/*
 * Upcall stubs for sec_authn routines.
 */

#ifdef SGIMIPS
error_status_t sec_krb_get_cred (
    sec_krb_ccache ccache,
    sec_krb_parsed_name server_name,
    unsigned32 authn_level,
    unsigned32 authz_proto,
    unsigned32 tgt_length,
    unsigned_char_p_t tgt_data,
    sec_krb_cred *cred,
    unsigned32 *expiration)
#else
error_status_t sec_krb_get_cred (ccache, server_name, authn_level, authz_proto,
    tgt_length, tgt_data, cred, expiration)
    sec_krb_ccache ccache;
    sec_krb_parsed_name server_name;
    unsigned32 authn_level;
    unsigned32 authz_proto;
    unsigned32 tgt_length;
    unsigned_char_p_t tgt_data;
    sec_krb_cred *cred;
    unsigned32 *expiration;
#endif
{
    SETUP(hp, SEC_KRB_GET_CRED);
    WRITE_IDH(hp, ccache);
    WRITE_NAME(hp, server_name);
    WRITE_LONG(hp, authn_level);
    WRITE_LONG(hp, authz_proto);
    TRANSCEIVE(hp);
    READ_CRED(hp, cred);
    READ_LONG(hp, *expiration);
    FINISH(hp);
}

#ifdef SGIMIPS
error_status_t sec_krb_dg_build_message (
    sec_krb_ccache ccache,
    sec_krb_cred cred,
    sec_des_block *challenge,
    unsigned32 authn_level,
    unsigned32 authz_proto,
    unsigned32 ksno,
    sec_des_key *key,
    sec_des_block *ivec,
    unsigned32 authz_fmt,
    sec_krb_message *message)
#else
error_status_t sec_krb_dg_build_message (ccache, cred, challenge,
    authn_level, authz_proto,
    ksno, key, ivec,
    authz_fmt,					 
    message)
    sec_krb_ccache ccache;
    sec_krb_cred cred;
    sec_des_block *challenge;
    unsigned32 authn_level;
    unsigned32 authz_proto;
    unsigned32 ksno;
    sec_des_key *key;
    sec_des_block *ivec;
    unsigned32 authz_fmt;
    sec_krb_message *message;
#endif
{
    message->data = 0;
    message->length = 0;
    SETUP(hp, SEC_KRB_DG_BUILD_MESSAGE);
    WRITE_IDH(hp, ccache);
    WRITE_CRED(hp, cred);
    WRITE_BLOCK(hp, (unsigned char *)challenge);
    WRITE_LONG (hp, authn_level);
    WRITE_LONG (hp, authz_proto);
    WRITE_LONG (hp, ksno);
    WRITE_BLOCK (hp, (unsigned char *)key);
    WRITE_BLOCK (hp, (unsigned char *)ivec);
    WRITE_LONG (hp, authz_fmt);
    TRANSCEIVE(hp);
    READ_MESSAGE(hp, message);
    FINISH(hp);
}

#ifdef SGIMIPS
error_status_t sec_krb_dg_decode_message_kern (
    sec_krb_message            	*message,
    sec_des_block              	*challenge,
    unsigned32                 	authz_fmt,
    handle_t                   	h,
    uuid_t                     	*actuid,
    unsigned32			boot_time,
    error_status_t		completion_status,
    unsigned char            	**client_name,
    sec_id_pac_t             	*client_pac,
    rpc_authz_cred_handle_t 	*client_creds,
    sec_krb_parsed_name      	*server_name,
    unsigned32               	*authn_level,
    unsigned32              	*authz_proto,
    unsigned32               	*key_seq,
    sec_des_key              	*key,
    sec_des_block            	*ivec,
    unsigned32               	*expiration)
#else
error_status_t sec_krb_dg_decode_message_kern (message, challenge, authz_fmt,
    h, actuid, boot_time, completion_status, client_name, client_pac,
    client_creds, server_name, authn_level, authz_proto, key_seq,
    key, ivec, expiration)
    sec_krb_message            	*message;
    sec_des_block              	*challenge;
    unsigned32                 	authz_fmt;
    handle_t                   	h;
    uuid_t                     	*actuid;
    unsigned32			boot_time;
    error_status_t		completion_status;

    unsigned char            	**client_name;
    sec_id_pac_t             	*client_pac;
    rpc_authz_cred_handle_t 	*client_creds;
    sec_krb_parsed_name      	*server_name;
    unsigned32               	*authn_level;
    unsigned32              	*authz_proto;
    unsigned32               	*key_seq;
    sec_des_key              	*key;
    sec_des_block            	*ivec;
    unsigned32               	*expiration;
#endif
{
    sec_bytes_t                 raw_epac_set;
    sec__cred_auth_info_t       auth_info, *authP;

    authP = &auth_info;
    memset(authP, 0, sizeof(sec__cred_auth_info_t));

    /* FAKE-EPAC
     * Don't know what to do with EPACS in the kernel yet, so just
     * ignore the cred handle.
     */
    memset(client_creds, 0, sizeof(*client_creds));

    SETUP(hp, SEC_KRB_DG_DECODE_MESSAGE);
    WRITE_MESSAGE(hp, message);
    WRITE_BLOCK (hp, (unsigned char *)challenge);
    WRITE_LONG (hp, authz_fmt);
    
    /* New for DCE1.1 - Large auth info support. */
    WRITE_LONG(hp, completion_status);
    if (completion_status == rpc_s_partial_credentials) {
	unsigned_char_p_t str_bdg;
	WRITE_LONG(hp, boot_time);
	rpc_binding_to_string_binding(h, &str_bdg, &st);
	WRITE_NAME(hp, str_bdg);
	WRITE_UUID(hp, actuid);
    }
	
    TRANSCEIVE(hp);
    READ_NAME(hp, client_name);
    READ_PAC(hp, client_pac);
#ifdef SGIMIPS
    READ_NAME(hp, (unsigned char **)server_name);
#else
    READ_NAME(hp, (char **)server_name);
#endif
    READ_LONG(hp, *authn_level);
    READ_LONG(hp, *authz_proto);
    READ_LONG(hp, *key_seq);
    READ_BLOCK(hp, key);
    READ_BLOCK(hp, ivec);
    READ_LONG(hp, *expiration);
    READ_SEC_BYTES(hp, &raw_epac_set);
    authP->authn_svc = *authn_level;
    authP->authz_svc = *authz_proto;
    authP->handle_type = sec__cred_handle_type_server;
    authP->authenticated = client_pac->authenticated;
    authP->server_princ_name = *server_name;
    st = sec__cred_create_authz_handle(authP, &raw_epac_set, NULL, NULL,
        client_creds);
    FINISH(hp);
}

#ifdef SGIMIPS
error_status_t sec_krb_register_server (
    unsigned_char_p_t                  server_princ_name,
    rpc_auth_key_retrieval_fn_t        get_key_func,
    void                               *arg)
#else
error_status_t sec_krb_register_server (server_princ_name, get_key_func, arg)
    unsigned_char_p_t                  server_princ_name;
    rpc_auth_key_retrieval_fn_t        get_key_func;
    void                               *arg;
#endif
{
    unsigned_char_p_t canon_name;
    if (get_key_func || arg) 
        return rpc_s_not_in_kernel;
    
    SETUP(hp, SEC_KRB_REGISTER_SERVER);
    WRITE_NAME(hp, server_princ_name);

    TRANSCEIVE(hp);
    READ_NAME(hp, &canon_name);
    if (!first_server_name) {
        first_server_name = canon_name;
    } else {
        RPC_MEM_FREE(canon_name, RPC_C_MEM_STRING);
    }
    FINISH(hp);
}


#ifdef SGIMIPS
void sec_krb_parsed_name_free (sec_krb_parsed_name *pname)
#else
void sec_krb_parsed_name_free (pname)
    sec_krb_parsed_name *pname;
#endif
{
    unsigned char *name = (unsigned char *) *pname;
    error_status_t st;

    *pname = 0;
    if (name != NULL)
        rpc_string_free(&name, &st);
}

#ifdef SGIMIPS
void sec_krb_cred_free (sec_krb_cred *cred)
#else
void sec_krb_cred_free (cred)
    sec_krb_cred *cred;
#endif
{
    error_status_t st;
    sec_krb_cred_rep *credrep = (sec_krb_cred_rep *) *cred;

    *cred = 0;    

    if (credrep != NULL) {
      if (credrep->server_name) {

        rpc_string_free (&credrep->server_name, &st);
        credrep->server_name = 0;
      }
      RPC_MEM_FREE (credrep, RPC_C_MEM_UTIL);
    }
}

#ifdef SGIMIPS
void sec_krb_message_free ( sec_krb_message *message)
#else
void sec_krb_message_free (message)
    sec_krb_message *message;
#endif
{
    if (message->data != 0) {
        RPC_MEM_FREE(message->data, RPC_C_MEM_UTIL);
    }
    message->data = 0;
    message->length = 0;
}

#ifdef SGIMIPS
error_status_t sec_krb_unparse_name(
    sec_krb_parsed_name pname,
    unsigned char **name)
#else
error_status_t sec_krb_unparse_name(pname, name)
    sec_krb_parsed_name pname;
    unsigned char **name;
#endif
{
    *name = rpc__stralloc ((unsigned char *) pname);
    return 0;
}

#ifdef NOTUSED
error_status_t sec_krb_parse_name(name, pname)
    unsigned char *name;
    sec_krb_parsed_name *pname;
{
    *pname = (sec_krb_parsed_name) rpc__stralloc (name);
    return 0;
}
#endif

/*
 * Note that we ignore the auth_ident and authn_level parameters here,
 * and let user space do all the work for us.
 */

#ifdef SGIMIPS
error_status_t sec_krb_sec_parse_name(
    rpc_auth_identity_handle_t auth_ident,
    unsigned32 authn_level,
    unsigned char *name,
    sec_krb_parsed_name *pname)
#else
error_status_t sec_krb_sec_parse_name(auth_ident, authn_level, name, pname)
    rpc_auth_identity_handle_t auth_ident;
    unsigned32 authn_level;
    unsigned char *name;
    sec_krb_parsed_name *pname;
#endif
{
    *pname = (sec_krb_parsed_name) rpc__stralloc (name);
    return 0;
}

#ifndef SGIMIPS_DES_INLINE

/*
 * Crunch key into expanded key schedule.
 */
error_status_t sec_des_key_sched (key, sched)
    sec_des_key *key;
    sec_des_key_schedule sched;
{
#ifndef SGIMIPS_DES_EXTERNAL
    return des_key_sched ((des_cblock *)key, (unsigned32 *)sched);
#else /* SGIMIPS_DES_EXTERNAL */
    return des_key_sched (DES_FUNC_KEY1, DES_FUNC_KEY2, DES_FUNC_KEY3,
	(des_cblock *)key, (unsigned32 *)sched);
#endif /* SGIMIPS_DES_EXTERNAL */
}

/*
 * Determine if key is weak
 */
int sec_des_is_weak_key (key)
    unsigned char *key;
{
#ifndef SGIMIPS_DES_EXTERNAL
    return des_is_weak_key ((des_cblock)key);
#else /* SGIMIPS_DES_EXTERNAL */
    return des_is_weak_key (DES_FUNC_KEY1, DES_FUNC_KEY2, DES_FUNC_KEY3, key);
#endif /* SGIMIPS_DES_EXTERNAL */
}

#endif /* !SGIMIPS_DES_INLINE */

/*
 * Random key generation mechanism.
 *
 * We rely on the user-space helper process to give us a 
 * "good" random key seed, which we then hang on to for the rest of
 * our lifetime.
 * 
 * The algorithm used is essentially the same as that used in the 
 * user-space DES package.
 */

static int key_seeded = 0;
#ifdef SGIMIPS
static sec_des_key random_seed;
#else
static sec_des_block random_seed;
#endif /* SGIMIPS */
static sec_des_key_schedule random_key;
static unsigned8 odd_parity[256] = {
  1, 
  1,   2,   2,   4,   4,   7,   7,   8, 
  8,  11,  11,  13,  13,  14,  14,  16, 
 16,  19,  19,  21,  21,  22,  22,  25, 
 25,  26,  26,  28,  28,  31,  31,  32, 
 32,  35,  35,  37,  37,  38,  38,  41, 
 41,  42,  42,  44,  44,  47,  47,  49, 
 49,  50,  50,  52,  52,  55,  55,  56, 
 56,  59,  59,  61,  61,  62,  62,  64, 
 64,  67,  67,  69,  69,  70,  70,  73, 
 73,  74,  74,  76,  76,  79,  79,  81, 
 81,  82,  82,  84,  84,  87,  87,  88, 
 88,  91,  91,  93,  93,  94,  94,  97, 
 97,  98,  98, 100, 100, 103, 103, 104, 
104, 107, 107, 109, 109, 110, 110, 112, 
112, 115, 115, 117, 117, 118, 118, 121, 
121, 122, 122, 124, 124, 127, 127, 128, 
128, 131, 131, 133, 133, 134, 134, 137, 
137, 138, 138, 140, 140, 143, 143, 145, 
145, 146, 146, 148, 148, 151, 151, 152, 
152, 155, 155, 157, 157, 158, 158, 161, 
161, 162, 162, 164, 164, 167, 167, 168, 
168, 171, 171, 173, 173, 174, 174, 176, 
176, 179, 179, 181, 181, 182, 182, 185, 
185, 186, 186, 188, 188, 191, 191, 193, 
193, 194, 194, 196, 196, 199, 199, 200, 
200, 203, 203, 205, 205, 206, 206, 208, 
208, 211, 211, 213, 213, 214, 214, 217, 
217, 218, 218, 220, 220, 223, 223, 224, 
224, 227, 227, 229, 229, 230, 230, 233, 
233, 234, 234, 236, 236, 239, 239, 241, 
241, 242, 242, 244, 244, 247, 247, 248, 
248, 251, 251, 253, 253, 254, 254};

/*
 * sec_des_fixup_key_parity: Forces odd parity per byte.
 */

#ifdef SGIMIPS
void sec_des_fixup_key_parity( register sec_des_block *key)
#else
void sec_des_fixup_key_parity(key)
    register sec_des_block *key;
#endif
{
    int i;

    for (i=0; i<sizeof(sec_des_block); i++)
      key->bits[i] = odd_parity[key->bits[i]];

    return;
}

#ifdef SGIMIPS
static error_status_t real_sec_des_new_random_key (sec_des_key *key)
#else
static error_status_t real_sec_des_new_random_key (key)
    sec_des_key *key;
#endif /* SGIMIPS */
{
    SETUP(hp, SEC_KRB_GEN_KEY);
    TRANSCEIVE(hp);
    READ_BLOCK(hp, key);
    FINISH(hp);
}

#ifdef SGIMIPS
error_status_t sec_des_generate_random_block ( sec_des_block *key)
#else
error_status_t sec_des_generate_random_block (key)
    sec_des_block *key;
#endif
{
    int i;
#ifdef SGIMIPS
    sec_des_key temp_key;
#else
    sec_des_block temp_key;
#endif /* SGIMIPS */
    error_status_t status;
    
    /* !!! following should perhaps occur in pthread_once */
    if (!key_seeded) {
        status = real_sec_des_new_random_key(&temp_key);
        if (status)
            return status;
        
        status = real_sec_des_new_random_key(&random_seed);
        if (status)
            return status;
        
#ifndef SGIMIPS_DES_EXTERNAL
        des_key_sched((des_cblock *)&temp_key, (unsigned32 *)random_key);
#else /* SGIMIPS_DES_EXTERNAL */
        sec_des_key_sched (&temp_key, random_key);
#endif /* SGIMIPS_DES_EXTERNAL */
        key_seeded = 1;
    }
    /* !!! following should occur under lock */
    /* crank seed by one */
    for (i=0; i<8; i++) {
        random_seed.bits[i]++;
        if (random_seed.bits[i])
            break;
    }
    /* !!! end lock */
    sec_des_ecb_encrypt ((sec_des_block *)&random_seed, key, random_key, sec_des_encrypt);
    return error_status_ok;
}

#ifdef SGIMIPS
error_status_t sec_des_new_random_key( sec_des_key *key)
#else
error_status_t sec_des_new_random_key(key)
    sec_des_key *key;
#endif
{
    error_status_t status;
#ifdef SGIMIPS
    extern void sec_des_fixup_key_parity(sec_des_block *); 
#endif /* SGIMIPS */
    
    do {
        status = sec_des_generate_random_block((sec_des_block *)key);
        if (status)
            return status;
        sec_des_fixup_key_parity((sec_des_block *)key);
    } while (sec_des_is_weak_key((unsigned char *)key));
    return error_status_ok;
}

/*
 * Glue to encryption routines.
 * These should really be replaced by macros included into the krb*.c 
 * modules, but that would be a bit painful to do at the moment.
 * (some day...)
 */
    
#ifndef SGIMIPS_DES_INLINE
void sec_des_cbc_cksum (plaintext, cksum, length, key, ivec)
    sec_des_block *plaintext;
    sec_des_block *cksum;
    signed32 length;
    sec_des_key_schedule key;
    sec_des_block *ivec;
{
#ifndef SGIMIPS_DES_EXTERNAL
    des_cbc_cksum ((des_cblock *)plaintext, (des_cblock *)cksum, length,
	(unsigned32 *)key, (unsigned char *)ivec);
#else /* SGIMIPS_DES_EXTERNAL */
    des_cbc_cksum (DES_FUNC_KEY1, DES_FUNC_KEY2, DES_FUNC_KEY3,
	(des_cblock *)plaintext, (des_cblock *)cksum, length,
	(unsigned32 *)key, (unsigned char *)ivec);
#endif /* SGIMIPS_DES_EXTERNAL */
}

void sec_des_ecb_encrypt (inblock, outblock, key, direction)
    sec_des_block                      *inblock;
    sec_des_block                     *outblock;
    sec_des_key_schedule               key;
    signed32                           direction;
{
#ifndef SGIMIPS_DES_EXTERNAL
    des_ecb_encrypt ((des_cblock *)inblock , (des_cblock *)outblock,
	(unsigned32 *)key, direction);
#else /* SGIMIPS_DES_EXTERNAL */
    des_ecb_encrypt (DES_FUNC_KEY1, DES_FUNC_KEY2, DES_FUNC_KEY3,
	(des_cblock *)inblock , (des_cblock *)outblock,
	(unsigned32 *)key, direction);
#endif /* SGIMIPS_DES_EXTERNAL */
}

void sec_des_cbc_encrypt (inblocks, outblocks, length, key, ivec, direction)
    sec_des_block                      *inblocks;
    sec_des_block                     *outblocks;
    signed32                           length;
    sec_des_key_schedule               key;
    sec_des_block                      *ivec;
    signed32                           direction;
{
#ifndef SGIMIPS_DES_EXTERNAL
    des_cbc_encrypt ((des_cblock *)inblocks, (des_cblock *)outblocks,
	length, (unsigned32 *)key, (unsigned char *)ivec, direction);
#else /* SGIMIPS_DES_EXTERNAL */
    des_cbc_encrypt (DES_FUNC_KEY1, DES_FUNC_KEY2, DES_FUNC_KEY3,
	(des_cblock *)inblocks, (des_cblock *)outblocks,
	length, (unsigned32 *)key, (unsigned char *)ivec, direction);
#endif /* SGIMIPS_DES_EXTERNAL */
}

#endif /* !SGIMIPS_DES_INLINE */

/*
 * Get server name, if any.
 */

#ifdef SGIMIPS
error_status_t sec_krb_get_server (unsigned_char_p_t *server_name)
#else
error_status_t sec_krb_get_server (server_name)
    unsigned_char_p_t *server_name;
#endif
{
    unsigned_char_p_t name = first_server_name;
    *server_name = name;
    if (name)
        return error_status_ok;
    else
        return sec_s_none_registered;
}

/*
 * Glue to MD5 routines.
 */

#ifdef SGIMIPS
void sec_md_begin( sec_md_ptr state)
#else
void sec_md_begin(state)
    sec_md_ptr state;
#endif
{
    MD5Init((MD5_CTX *)state);
}

#ifdef SGIMIPS
void sec_md_update (
    sec_md_ptr state,
    byte_p_t data,
    unsigned32 len)
#else
void sec_md_update (state, data, len)
    sec_md_ptr state;
    byte_p_t data;
    unsigned32 len;
#endif
{
    MD5Update((MD5_CTX *)state, data, len);
}

#ifdef SGIMIPS
void sec_md_final ( sec_md_ptr state)
#else
void sec_md_final (state)
    sec_md_ptr state;
#endif
{
    MD5Final((MD5_CTX *)state);
}

/*
 * sec_krb_get_cc: create reference to credential cache.
 *
 * This just copies the information, if any, and allocates memory for it.
 */

#ifdef SGIMIPS
error_status_t sec_krb_get_cc (
    rpc_auth_identity_handle_t auth_ident,
    sec_krb_ccache *ccache)
#else
error_status_t sec_krb_get_cc (auth_ident, ccache)
    rpc_auth_identity_handle_t auth_ident;
    sec_krb_ccache *ccache;
#endif
{
    cchp crp;
    unsigned32 uid, pag;

    if (auth_ident == NULL) {
        uid = krpc_osi_GetUID(krpc_osi_getucred());
	pag = uid;
    } else {
        unsigned32 *ppagp = (unsigned32 *)auth_ident;
        pag = ppagp[0];
        uid = ppagp[1];
    }

    RPC_MUTEX_LOCK(credlist_lock);
    for (crp = sec_krb_cred_list; crp; crp = crp->next) 
    {
	if ((crp->pag == pag) && (crp->uid == uid)) 
	{
	    /* bump refcount */
	    crp->refcount++;
	    break;
	}
    }
    
    if (crp == NULL) 
    {
	RPC_MEM_ALLOC(crp, cchp, sizeof(cch),
		      RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
	crp->next = sec_krb_cred_list;
	crp->uid = uid;
	crp->pag = pag;
	crp->refcount = 1;
	sec_krb_cred_list = crp;
    }
    RPC_MUTEX_UNLOCK(credlist_lock);
    *ccache = (sec_krb_ccache) crp;
    return error_status_ok;
}

/*
 * sec_krb_ccache_free: drop reference to credential cache.
 *
 * Free memory used, if any.
 */

#ifdef SGIMIPS
void sec_krb_ccache_free ( sec_krb_ccache *ccache)
#else
void sec_krb_ccache_free (ccache)
    sec_krb_ccache *ccache;
#endif
{
    cchp crp = (cchp)*ccache;
    cchp crpp;
    int ref;

    if (crp == NULL)
	return;
    *ccache = NULL;
    RPC_MUTEX_LOCK(credlist_lock);
    ref = crp->refcount-1;
    crp->refcount = ref;
    if (ref != 0)
	crp = NULL;		/* don't free crp.. */
    else {
	if (crp == sec_krb_cred_list)
	    sec_krb_cred_list = crp->next;
	else 
	{
	    for (crpp = sec_krb_cred_list; crpp; crpp=crpp->next)
		if (crpp->next == crp)
		    break;
	    if ((!crpp) || (crpp->next != crp))
		panic("sec_authn_krpc: cred list botch");
	    crpp->next = crp->next;
	}
    }
    RPC_MUTEX_UNLOCK(credlist_lock);
    if (crp)
	RPC_MEM_FREE(crp, RPC_C_MEM_UTIL);
}
    
boolean32 sec_krb_must_use_u2u(
    error_status_t status
)
{
    return ( false );
}


boolean32 sec_krb_never_valid(
    error_status_t status
)
{
    return ( false );
}
