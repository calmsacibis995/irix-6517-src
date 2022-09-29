/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: stubbase.h,v $
 * Revision 65.1  1997/10/20 19:16:21  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.135.2  1996/02/18  22:57:26  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:20  marty]
 *
 * Revision 1.1.135.1  1995/12/08  00:23:45  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/20  22:15 UTC  jrr
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:11 UTC  dat  /main/dat_xidl2/1]
 * 
 * 	HP revision /main/HPDCE02/1  1995/08/02  15:56 UTC  lmm
 * 	merge stack saving mods
 * 
 * 	HP revision /main/lmm_reduce_rpc_stack/1  1995/06/27  21:28 UTC  lmm
 * 	define _HP_IDL_SAVE_STACK in KRPC
 * 	[1995/12/08  00:01:21  root]
 * 
 * Revision 1.1.130.5  1994/08/12  20:57:31  tom
 * 	Bug 10411 - Remove unsigned from pipe_number, next_{in,out}_pipe
 * 	  arguments for rpc_ss_initialize_callee_pipe.
 * 	[1994/08/12  20:57:13  tom]
 * 
 * Revision 1.1.130.4  1994/08/09  15:34:35  rico
 * 	Added volatile to the arguments of rpc_ss_init_node_table() and
 * 	rpc_ss_mts_client_estab_alloc() in function prototypes and definitions.
 * 	[1994/08/09  15:17:44  rico]
 * 
 * Revision 1.1.130.3  1994/07/26  17:21:11  rico
 * 	Create rpc_ss_mem_alloc for exc_handling/mutex locking bug
 * 	in rpc_ss_allocate
 * 	[1994/07/26  17:16:23  rico]
 * 
 * Revision 1.1.130.2  1994/05/02  22:43:38  tom
 * 	From DEC: Add binding callout function:
 * 	 Add rpc_ss_bind_authn_client prototype.
 * 	[1994/05/02  21:06:31  tom]
 * 
 * Revision 1.1.130.1  1994/01/21  22:40:09  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:30:54  cbrooks]
 * 
 * Revision 1.1.11.4  1993/07/16  14:15:44  tom
 * 	Bug 8289 - Add close comment.
 * 	[1993/07/16  13:38:19  tom]
 * 
 * Revision 1.1.11.3  1993/07/07  20:12:54  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:40:49  ganni]
 * 
 * Revision 1.1.11.2  1993/05/24  20:47:52  cjd
 * 	Submitting 102-dme port to 103i
 * 	[1993/05/24  20:15:32  cjd]
 * 
 * Revision 1.1.9.2  1993/05/12  02:19:31  jd
 * 	Initial 486 port.
 * 	[1993/05/12  02:18:38  jd]
 * 
 * Revision 1.1.5.6  1993/01/04  00:10:37  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:16:17  bbelch]
 * 
 * Revision 1.1.5.5  1992/12/23  21:21:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:47:59  zeliff]
 * 
 * Revision 1.1.5.4  1992/12/10  21:06:09  hinxman
 * 	OT5940 - Correct prototype for p_allocate function pointers
 * 	[1992/12/10  21:03:48  hinxman]
 * 
 * Revision 1.1.5.3  1992/11/05  15:33:03  hinxman
 * 	Fix memory leak in per-thread context used for memory management.
 * 	Correct conditionals for IEEE floating point on Alpha AXP.
 * 	[1992/11/04  19:33:20  hinxman]
 * 
 * Revision 1.1.5.2  1992/09/29  21:14:02  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:45:53  weisman]
 * 
 * Revision 1.1.2.4  1992/05/01  16:06:26  rsalz
 * 	     #include <string.h>, <stdlib.h>
 * 	[1992/05/01  00:20:38  rsalz]
 * 
 * Revision 1.1.2.3  1992/04/06  19:50:55  harrow
 * 	Remove prototype for rpc_ss_ee_ctx_to_wire_locking as it is
 * 	no longer needed.
 * 
 * 	Add DEC Alpha un/marshalling macros.
 * 	[1992/04/01  16:50:25  harrow]
 * 
 * Revision 1.1.2.2  1992/03/27  23:45:40  harrow
 * 	     Add rpc_{marshall,unmarshall,convert}_v1_enum macros for un/marshaling
 * 	     of [v1_enum] enums, and remove unused marshalling macros.
 * 
 * 	     Add prototypes for stub support for reference counting of
 * 	     active context handles.
 * 
 * 	     Enable status mapping for VMS systems.
 * 	     [1992/03/27  23:44:20  harrow]
 * 
 * Revision 1.1  1992/01/19  03:14:01  devrcs
 * 	     Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989, 1990, 1991, 1992, 1993 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**  NAME:
**
**      stubbase.h
**
**  FACILITY:
**
**      IDL Stub Support Include File
**
**  ABSTRACT:
**
**  Definitions private to IDL-generated stubs.  This file is #include'd
**  by all stub files emitted by the IDL compiler.  The stub ".c" files
**  for interface "foo" do:
**
**      #include <foo.h>
**      #include <dce/stubbase.h>
**
**  <foo.h> does:
**
**      #include <dce/idlbase.h>
**      #include <dce/rpc.h>
**
**  Note that this file is also #include'd by the RPC runtime library
**  sources to allow the discreet sharing of definitions between the
**  stubs, stub-support library, and the RPC runtime.  The symbol NCK
**  is defined by the RPC runtime library sources so we can #ifdef out
**  the "unshared" parts of this file (which might contain stuff that
**  we don't want to deal with in the RPC runtime library compilation).
**
*/

#ifndef STUBBASE_H
#define STUBBASE_H	1

#include <dce/dce.h>

/***************************************************************************/

/*
 * Data structures that define the internal representation of an interface
 * specifier (what the opaque pointer that "rpc.idl" defines points to).
 * These are the definitions used by the runtime and the runtime source
 * includes this file to get these definitions.
 */

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct
{
    unsigned_char_p_t           rpc_protseq;
    unsigned_char_p_t           endpoint;
} rpc_endpoint_vector_elt_t, *rpc_endpoint_vector_elt_p_t;

typedef struct
{
    unsigned long               count;
    rpc_endpoint_vector_elt_t   *endpoint_vector_elt;
} rpc_endpoint_vector_t, *rpc_endpoint_vector_p_t;

typedef struct
{
    unsigned long               count;
    rpc_syntax_id_t             *syntax_id;
} rpc_syntax_vector_t, *rpc_syntax_vector_p_t;


typedef struct
{
    unsigned short              ifspec_vers;
    unsigned short              opcnt;
    unsigned long               vers;
    uuid_t                      id;
    unsigned short              stub_rtl_if_vers;
    rpc_endpoint_vector_t       endpoint_vector;
    rpc_syntax_vector_t         syntax_vector;
    rpc_v2_server_stub_epv_t    server_epv;
    rpc_mgr_epv_t               manager_epv;
} rpc_if_rep_t, *rpc_if_rep_p_t;





/***************************************************************************/

/*
 * Define the structure of a translation table.
 */
typedef unsigned char rpc_trans_tab_t [256];
typedef rpc_trans_tab_t * rpc_trans_tab_p_t;


/***************************************************************************/

/*
 * Define the local scalar data representations used.
 */

#include <dce/ndr_rep.h>

/*
 * By default, the stub macros use the pthreads API, but there are hooks
 * to let the CMA API be used.
 */

#define STUBS_USE_PTHREADS	1

#if defined(VMS) || defined(__VMS)
#pragma nostandard
#define IDL_ENABLE_STATUS_MAPPING
#endif /* VMS */



/* 
 * Force stubs that are linked into the Apollo global library to use the 
 * shared version of malloc.  
 */  

#ifdef STUBS_NEED_SHARED_MALLOC
#  include <local/shlib.h>
#  include <stdlib.h>
#endif

/***************************************************************************/

/*
 * #include various files, but don't complicate the problems of compiling
 * the RPC runtime sources--don't do the #include's if NCK is defined.
 */

#ifndef NCK

#ifdef STUBS_USE_PTHREADS

#ifndef _KERNEL
#ifdef DOS_FILENAME_RESTRICTIONS
#  include <dce/pthread_.h>
#else
#  include <dce/pthread_exc.h>
#endif /* DOS_FILENAME_RESTRICTIONS */
#else
#  include <dce/ker/pthread_exc.h>
#endif /* KERNEL  */
#else  
#include <cma.h>
#endif /* STUBS_USE_PTHREADS */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if !defined(offsetof)
#define offsetof(_type, _member) \
    (((char *)&((_type *)0)->_member) - (char *)((_type *)0))
#endif /* defined(offsetof) */


#define IDL_offsetofarr(_type, _member) \
    (((char *)((_type *)0)->_member) - (char *)((_type *)0))

#if defined(__VMS) && ( defined(__DECC) || defined(__cplusplus) )
#pragma extern_model __save
#pragma extern_model __common_block __shr
#endif /* VMS or DECC or C++ */

#include <dce/rpcexc.h>

#ifdef _KERNEL
/*
 * get the runtime's (private) exception ids for the kernel.
 */
#include <dce/ker/exc_handling_ids_krpc.h>

/*
 * Redefine malloc and free so that stubs and libidl use something
 * appropriate in a kernel environment.
 */
#include <dce/ker/idl_ss_krpc.h>

#define malloc  idl_malloc_krpc
#define free    idl_free_krpc

/* save stack space in kernel RPCs */ 
#ifndef _HP_IDL_SAVE_STACK
#define _HP_IDL_SAVE_STACK	1
#endif

#endif /* _KERNEL  */

#else /* NCK defined */

#if defined(__VMS) && (defined(__DECC) || defined(__cplusplus))
#pragma extern_model __save
#pragma extern_model __common_block __shr
#endif /* VMS, DEC C, or C++ */

#endif  /* NCK */

/***************************************************************************/

/*
 * The rest of this file is typically not needed by RPC runtime library
 * source files.  However, some of that library's modules need the
 * marshalling macros; these modules #define NCK_NEED_MARSHALLING.  All
 * other RPC runtime library modules end up not seeing the rest of this
 * file.
 */

#if !defined(NCK) || defined(NCK_NEED_MARSHALLING)

/***************************************************************************/

/*
 * Untyped pointer for portability
 */

typedef pointer_t rpc_void_p_t ;


/***************************************************************************/

/*
 * The following variables are defined (and undefined) within this file
 * to control the definition of macros which are emitted into stub
 * files by the IDL compiler.  For each variable there is a set of default
 * definitions which is used unless a target system specific section
 * #undef-s it and supplies an alternate set of definitions.  Exactly
 * which macro definitions are governed by each variable is listed below.
 *
 *   USE_DEFAULT_MP_REP
 *      Controls the definition of a type and the macros which define
 *      the marshalling pointer scheme used on a particular target system.
 *      The following macros need to be defined if USE_DEFAULT_MP_REP
 *      is #undef-ed:
 *       rpc_init_mp
 *       rpc_init_op
 *       rpc_advance_mp
 *       rpc_advance_op
 *       rpc_advance_mop
 *       rpc_align_mp
 *       rpc_align_op
 *       rpc_align_mop
 *       rpc_synchronize_mp
 *
 *      and the following types need to be typedef-ed:
 *       rpc_mp_t
 *       rpc_op_t
 *
 *   USE_DEFAULT_MACROS
 *      Controls the definition of the macros which define how to marshall,
 *      unmarshall and convert values of each NDR scalar type as well
 *      as NDR string types.  The following macros need to be defined
 *      if USE_DEFAULT_MACROS is #undef-ed:
 *
 *       rpc_marshall_boolean, rpc_unmarshall_boolean, rpc_convert_boolean
 *       rpc_marshall_byte, rpc_unmarshall_byte, rpc_convert_byte
 *       rpc_marshall_char, rpc_unmarshall_char, rpc_convert_char
 *       rpc_marshall_enum, rpc_unmarshall_enum, rpc_convert_enum
 *       rpc_marshall_v1_enum, rpc_unmarshall_v1_enum, rpc_convert_v1_enum
 *       rpc_marshall_small_int, rpc_unmarshall_small_int, rpc_convert_small_int
 *       rpc_marshall_usmall_int, rpc_unmarshall_usmall_int, rpc_convert_usmall_int
 *       rpc_marshall_short_int, rpc_unmarshall_short_int, rpc_convert_short_int
 *       rpc_marshall_ushort_int, rpc_unmarshall_ushort_int, rpc_convert_ushort_int
 *       rpc_marshall_long_int, rpc_unmarshall_long_int, rpc_convert_long_int
 *       rpc_marshall_ulong_int, rpc_unmarshall_ulong_int, rpc_convert_ulong_int
 *       rpc_marshall_hyper_int, rpc_unmarshall_hyper_int, rpc_convert_hyper_int
 *       rpc_marshall_uhyper_int, rpc_unmarshall_uhyper_int, rpc_convert_uhyper_int
 *       rpc_marshall_short_float, rpc_unmarshall_short_float, rpc_convert_short_float
 *       rpc_marshall_long_float, rpc_unmarshall_long_float, rpc_convert_long_float
 */

#define USE_DEFAULT_MP_REP	1
#define USE_DEFAULT_MACROS	1
#define PACKED_SCALAR_ARRAYS	1
#define PACKED_CHAR_ARRAYS	1
#define PACKED_BYTE_ARRAYS	1

/***************************************************************************/

#if (defined(VMS) || defined(__VMS)) && (defined(__DECC) || defined(__DECCXX))
#pragma extern_model save
#pragma extern_model strict_refdef "NDR_G_LOCAL_DREP"
extern ndr_format_t ndr_g_local_drep;
#pragma extern_model restore
#else
globalref ndr_format_t ndr_g_local_drep;
#endif

/*
 * Macro used by stubs to compare two rpc_$drep_t's.
 */

#define rpc_equal_drep(drep1, drep2)\
    (drep1.int_rep==drep2.int_rep)&&(drep1.char_rep==drep2.char_rep)&&(drep1.float_rep==drep2.float_rep)

/***************************************************************************/

/*
 * Define the versions of stubs that this instance of idl_base.h supports.
 */

#define IDL_BASE_SUPPORTS_V1	1

/***************************************************************************/

/*
 * Define some macros that stub code uses for brevity or clarity.
 */

#ifndef NULL
#define NULL 0
#endif /* NULL */

#define SIGNAL(code) {\
    error_status_t st;\
    st.all = code;\
    pfm_signal (st);\
    }


/***************************************************************************/

/*
 * include machine specific marshalling code 
 */

#include <dce/marshall.h>

/***************************************************************************/

/****
 **** Definition of the default marshalling pointer representation
 ****/

#ifdef USE_DEFAULT_MP_REP

typedef char *rpc_mp_t;
typedef ndr_ulong_int rpc_op_t;

#define rpc_init_mp(mp, bufp)\
    mp = (rpc_mp_t)bufp

#define rpc_align_mp(mp, alignment)\
    mp = (rpc_mp_t)(((mp - (rpc_mp_t)0) + (alignment-1)) & ~(alignment-1))

#define rpc_advance_mp(mp, delta)\
    mp += delta

#define rpc_init_op(op)\
    op = 0

#define rpc_advance_op(op, delta)\
    op += delta

#define rpc_align_op(op, alignment)\
    op = ((op + (alignment-1)) & ~(alignment-1))

#define rpc_advance_mop(mp, op, delta)\
    rpc_advance_mp(mp, delta);\
    rpc_advance_op(op, delta)

#define rpc_align_mop(mp, op, alignment)\
    rpc_align_mp(mp, alignment);\
    rpc_align_op(op, alignment)

#define rpc_synchronize_mp(mp, op, alignment)\
    mp += (((op & alignment-1)-(mp-(rpc_mp_t)0 & alignment-1)) &\
           (alignment-1))

#endif  /* USE_DEFAULT_MP_REP */

/***************************************************************************/
/*

 ****
 **** Definitions of the default marshall, unmarshall, and convert macros.
 ****/

#ifdef USE_DEFAULT_MACROS

#define rpc_marshall_boolean(mp, src)\
    *(ndr_boolean *)mp = src

#define rpc_unmarshall_boolean(mp, dst)\
    dst = *(ndr_boolean *)mp

#define rpc_convert_boolean(src_drep, dst_drep, mp, dst)\
    rpc_unmarshall_boolean(mp, dst)



#define rpc_marshall_byte(mp, src)\
    *(ndr_byte *)mp = src

#define rpc_unmarshall_byte(mp, dst)\
    dst = *(ndr_byte *)mp

#define rpc_convert_byte(src_drep, dst_drep, mp, dst)\
    rpc_unmarshall_byte(mp, dst)



#define rpc_marshall_char(mp, src)\
    *(ndr_char *)mp = src

#define rpc_unmarshall_char(mp, dst)\
    dst = *(ndr_char *)mp

#define rpc_convert_char(src_drep, dst_drep, mp, dst)\
    if (src_drep.char_rep == dst_drep.char_rep)\
        rpc_unmarshall_char(mp, dst);\
    else if (dst_drep.char_rep == ndr_c_char_ascii)\
        dst = (*ndr_g_ebcdic_to_ascii) [*(ndr_char *)mp];\
    else\
        dst = (*ndr_g_ascii_to_ebcdic) [*(ndr_char *)mp]


#define rpc_marshall_enum(mp, src)\
    *(ndr_short_int *)mp = (ndr_short_int)src

#define rpc_unmarshall_enum(mp, dst)\
    dst = *(ndr_short_int *)mp

#define rpc_convert_enum(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_enum(mp, dst);\
    else {\
        ndr_short_int _sh;\
        ndr_byte *_d = (ndr_byte *) &_sh;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        dst = _sh;\
        }



#ifdef TWO_BYTE_ENUMS
#define rpc_marshall_v1_enum(mp, src)\
    rpc_marshall_ushort(mp, 0);\
    rpc_marshall_enum((mp+2), src)

#define rpc_unmarshall_v1_enum(mp, dst)\
    rpc_unmarshall_enum((mp+2), src)

#define rpc_convert_v1_enum(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_v1_enum(mp, dst);\
    else {\
        ndr_ushort_int _sh;\
        ndr_byte *_d = (ndr_byte *) &_sh;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        dst = _sh;\
        }
#else
#define rpc_marshall_v1_enum(mp, src)\
    *(ndr_ulong_int *)mp = (ndr_ulong_int)src

#define rpc_unmarshall_v1_enum(mp, dst)\
    dst = *(ndr_ulong_int *)mp

#define rpc_convert_v1_enum(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_v1_enum(mp, dst);\
    else {\
        ndr_ulong_int _l;\
        ndr_byte *_d = (ndr_byte *) &_l;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        dst = _l;\
        }
#endif /* TWO_BYTE_ENUMS */


#define rpc_marshall_small_int(mp, src)\
    *(ndr_small_int *)mp = src

#define rpc_unmarshall_small_int(mp, dst)\
    dst = *(ndr_small_int *)mp

#define rpc_convert_small_int(src_drep, dst_drep, mp, dst)\
    rpc_unmarshall_small_int(mp, dst)



#define rpc_marshall_usmall_int(mp, src)\
    *(ndr_usmall_int *)mp = src

#define rpc_unmarshall_usmall_int(mp, dst)\
    dst = *(ndr_usmall_int *)mp

#define rpc_convert_usmall_int(src_drep, dst_drep, mp, dst)\
    rpc_unmarshall_usmall_int(mp, dst)



#define rpc_marshall_short_int(mp, src)\
    *(ndr_short_int *)mp = src

#define rpc_unmarshall_short_int(mp, dst)\
    dst = *(ndr_short_int *)mp

#define rpc_convert_short_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_short_int(mp, dst);\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_marshall_ushort_int(mp, src)\
    *(ndr_ushort_int *)mp = (ndr_ushort_int)src

#define rpc_unmarshall_ushort_int(mp, dst)\
    dst = *(ndr_ushort_int *)mp

#define rpc_convert_ushort_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_ushort_int(mp, dst);\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_marshall_long_int(mp, src)\
    *(ndr_long_int *)mp = src

#define rpc_unmarshall_long_int(mp, dst)\
    dst = *(ndr_long_int *)mp

#define rpc_convert_long_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_long_int(mp, dst);\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_marshall_ulong_int(mp, src)\
    *(ndr_ulong_int *)mp = (ndr_ulong_int)src

#define rpc_unmarshall_ulong_int(mp, dst)\
    dst = *(ndr_ulong_int *)mp

#define rpc_convert_ulong_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_ulong_int(mp, dst);\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_marshall_hyper_int(mp, src) {\
    *(ndr_hyper_int *)mp = *(ndr_hyper_int *)&src;\
    }

#define rpc_unmarshall_hyper_int(mp, dst) {\
    *(ndr_hyper_int *)&dst = *(ndr_hyper_int *)mp;\
    }

#define rpc_convert_hyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_hyper_int(mp, dst)\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_marshall_uhyper_int(mp, src) {\
    *(ndr_uhyper_int *)mp = *(ndr_uhyper_int *)&src;\
    }

#define rpc_unmarshall_uhyper_int(mp, dst) {\
    *(ndr_uhyper_int *)&dst = *(ndr_uhyper_int *)mp;\
    }

#define rpc_convert_uhyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_unmarshall_uhyper_int(mp, dst)\
    else {\
        ndr_byte *_d = (ndr_byte *) &dst;\
        ndr_byte *_s = (ndr_byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_marshall_short_float(mp, src) {\
    ndr_short_float tmp;\
    tmp = src;\
    *(ndr_short_float *)mp = tmp;\
    }

#define rpc_unmarshall_short_float(mp, dst)\
    dst = *(ndr_short_float *)mp

#define rpc_convert_short_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_unmarshall_short_float(mp, dst);\
    else {\
        ndr_cvt_short_float (src_drep, dst_drep,\
            (short_float_p_t)mp,\
            (short_float_p_t)&dst);\
        }



#define rpc_marshall_long_float(mp, src)\
    *(ndr_long_float *)mp = src

#define rpc_unmarshall_long_float(mp, dst)\
    dst = *(ndr_long_float *)mp

#define rpc_convert_long_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_unmarshall_long_float(mp, dst);\
    else\
        ndr_cvt_long_float (src_drep, dst_drep,\
            (long_float_p_t)mp,\
            (long_float_p_t)&dst)

#endif  /* USE_DEFAULT_MACROS */

/***************************************************************************/
/***************************************************************************/

/*
 *  Macros to give independence from threading package
 */

#define RPC_SS_EXC_MATCHES exc_matches

#ifdef STUBS_USE_PTHREADS

#define RPC_SS_THREADS_INIT

#define RPC_SS_THREADS_ONCE_T pthread_once_t

#define RPC_SS_THREADS_ONCE_INIT pthread_once_init

#define RPC_SS_THREADS_ONCE(once_block_addr,init_routine) pthread_once( \
    once_block_addr,(pthread_initroutine_t)init_routine)


#define RPC_SS_THREADS_MUTEX_T pthread_mutex_t

#define RPC_SS_THREADS_MUTEX_CREATE(mutex_addr) pthread_mutex_init( \
    mutex_addr, pthread_mutexattr_default )

#define RPC_SS_THREADS_MUTEX_LOCK(mutex_addr) pthread_mutex_lock(mutex_addr)

#define RPC_SS_THREADS_MUTEX_UNLOCK(mutex_addr) pthread_mutex_unlock(mutex_addr)

#define RPC_SS_THREADS_MUTEX_DELETE(mutex_addr) pthread_mutex_destroy(mutex_addr)

typedef void *rpc_ss_threads_dest_arg_t;

#define RPC_SS_THREADS_KEY_T pthread_key_t

#define RPC_SS_THREADS_KEY_CREATE(key_addr,destructor) pthread_keycreate( \
    key_addr,destructor)

#define RPC_SS_THREADS_KEY_SET_CONTEXT(key,value) pthread_setspecific( \
    key,(pthread_addr_t)(value))

#define RPC_SS_THREADS_KEY_GET_CONTEXT(key,value_addr) pthread_getspecific( \
    key,(pthread_addr_t*)(value_addr))

#define RPC_SS_THREADS_CONDITION_T pthread_cond_t

#define RPC_SS_THREADS_CONDITION_CREATE(condition_addr) pthread_cond_init( \
    condition_addr,pthread_condattr_default)

#define RPC_SS_THREADS_CONDITION_SIGNAL(condition_addr) pthread_cond_signal( \
    condition_addr)

#define RPC_SS_THREADS_CONDITION_WAIT(condition_addr,mutex_addr) pthread_cond_wait( \
    condition_addr,mutex_addr)

#define RPC_SS_THREADS_CONDITION_DELETE(condition_addr) pthread_cond_destroy( \
    condition_addr)

#define RPC_SS_THREADS_X_CANCELLED pthread_cancel_e

#define RPC_SS_THREADS_CANCEL_STATE_T int

#define RPC_SS_THREADS_DISABLE_ASYNC(state) state=pthread_setasynccancel(CANCEL_OFF)

#define RPC_SS_THREADS_RESTORE_ASYNC(state) pthread_setasynccancel(state)

#define RPC_SS_THREADS_ENABLE_GENERAL(state) state=pthread_setcancel(CANCEL_ON)

#define RPC_SS_THREADS_RESTORE_GENERAL(state) state=pthread_setcancel(state)

#else

#define RPC_SS_THREADS_INIT cma_init()

#define RPC_SS_THREADS_ONCE_T cma_t_once

#define RPC_SS_THREADS_ONCE_INIT cma_once_init

#define RPC_SS_THREADS_ONCE(once_block_addr,init_routine) cma_once( \
    once_block_addr,(cma_t_init_routine)init_routine,&cma_c_null)

#define RPC_SS_THREADS_MUTEX_T cma_t_mutex

#define RPC_SS_THREADS_MUTEX_CREATE(mutex_addr) cma_mutex_create( \
    mutex_addr, &cma_c_null )

#define RPC_SS_THREADS_MUTEX_LOCK(mutex_addr) cma_mutex_lock((mutex_addr))

#define RPC_SS_THREADS_MUTEX_UNLOCK(mutex_addr) cma_mutex_unlock((mutex_addr))

#define RPC_SS_THREADS_MUTEX_DELETE(mutex_addr) cma_mutex_delete(mutex_addr)

typedef cma_t_address rpc_ss_threads_dest_arg_t;

#define RPC_SS_THREADS_KEY_T cma_t_key

#define RPC_SS_THREADS_KEY_CREATE(key_addr,destructor) cma_key_create( \
    key_addr,&cma_c_null,destructor)

#define RPC_SS_THREADS_KEY_SET_CONTEXT(key,value) cma_key_set_context( \
    key,(cma_t_address)value)

#define RPC_SS_THREADS_KEY_GET_CONTEXT(key,value_addr) cma_key_get_context( \
    key,(cma_t_address *)value_addr)

#define RPC_SS_THREADS_CONDITION_T cma_t_cond

#define RPC_SS_THREADS_CONDITION_CREATE(condition_addr) cma_cond_create( \
    condition_addr,&cma_c_null)

#define RPC_SS_THREADS_CONDITION_SIGNAL(condition_addr) cma_cond_signal( \
    condition_addr)

#define RPC_SS_THREADS_CONDITION_WAIT(condition_addr,mutex_addr) cma_cond_wait( \
    condition_addr,mutex_addr)

#define RPC_SS_THREADS_CONDITION_DELETE(condition_addr) cma_cond_delete( \
    condition_addr)

#define RPC_SS_THREADS_X_CANCELLED cma_e_alerted

#define RPC_SS_THREADS_CANCEL_STATE_T cma_t_alert_state

#define RPC_SS_THREADS_DISABLE_ASYNC(state) cma_alert_disable_asynch(&state)

#define RPC_SS_THREADS_RESTORE_ASYNC(state) cma_alert_restore(&state)

#define RPC_SS_THREADS_ENABLE_GENERAL(state) cma_alert_enable_general(&state)

#define RPC_SS_THREADS_RESTORE_GENERAL(state) cma_alert_restore(&state)

#endif  /* STUBS_USE_PTHREADS */

/*
 * A macro to set up an iovector of appropriate size
 */

#define IoVec_t(n)                 \
    struct                         \
    {                              \
        unsigned16 num_elt;        \
        rpc_iovector_elt_t elt[n]; \
    }

#define NULL_FREE_RTN (rpc_buff_dealloc_fn_t)0

typedef rpc_buff_dealloc_fn_t rpc_ss_dealloc_t;

/*
 * Memory allocation stub support definitions
 */

/*
 * Wrapper needed for free() routine assignments in generated files.
 */

void IDL_ENTRY rpc_ss_call_free   _DCE_PROTOTYPE_ (( rpc_void_p_t ));

/*
 * Table of caller nodes that are marshalled during an operation
 */

typedef byte_p_t rpc_ss_node_table_t;

/*
 * An allocation handle; initialize to NULL
 */

typedef volatile struct {
    rpc_void_p_t memory;
    rpc_ss_node_table_t node_table;
} rpc_ss_mem_handle;

/*
 * rpc_ss_mem_alloc
 *
 * Allocates and returns a pointer to a block of memory
 * aligned to an eight-byte boundary containing bytes bytes
 * Returns NULL if unable to allocate
 */

byte_p_t IDL_ENTRY rpc_ss_mem_alloc   _DCE_PROTOTYPE_ ((
    rpc_ss_mem_handle *,  /* The (initially NULL) allocation handle */
    unsigned               /* Number of bytes to allocate */
));

/*
 * rpc_sm_mem_alloc
 *
 * Allocates and returns a pointer to a block of memory
 * aligned to an eight-byte boundary containing bytes bytes
 * Returns NULL if unable to allocate
 * returns rpc_s_no_memory instead of Raise( rpc_x_no_memory)
 */
byte_p_t IDL_ENTRY rpc_sm_mem_alloc   _DCE_PROTOTYPE_ ((
    rpc_ss_mem_handle *,    /* The (initially NULL) allocation handle */
    unsigned,               /* Number of bytes to allocate */
    error_status_t *        /*The status parameter if alloc returns NULL */
));


/*
 * rpc_ss_mem_free
 *
 * Frees all memory associated with the handle passed
 */

void IDL_ENTRY rpc_ss_mem_free   _DCE_PROTOTYPE_ ((rpc_ss_mem_handle *));

/*
 * rpc_ss_mem_release
 *
 * Removes a block of memory from the list associated with a handle
 * Frees the memory if 'free' is non-zero
 */

void IDL_ENTRY rpc_ss_mem_release   _DCE_PROTOTYPE_ ((
    rpc_ss_mem_handle *,
    byte_p_t ,/* The aligned address of the memory to be
                          released */
    int /*freeit*/         /* Non-zero if the memory should be freed */
));

#ifdef MIA
/*
 * rpc_ss_mem_item_free
 *
 * Same functionality as rpc_ss_mem_release with freeit != 0
 */
void IDL_ENTRY rpc_ss_mem_item_free   _DCE_PROTOTYPE_ ((
    rpc_ss_mem_handle *,
    byte_p_t  /* The aligned address of the memory to be
                          released */
));
#endif /* MIA  */


/*
 * rpc_ss_mem_dealloc
 *
 * A routine to pass to the runtime: passed an aligned data address,
 * frees the original block of memory containing the address.
 */
void IDL_ENTRY rpc_ss_mem_dealloc   _DCE_PROTOTYPE_ (( byte_p_t ));

/*
 * To avoid passing parameter lists of excessive length, a set of values
 * needed while unmarshalling a complex parameter is held in a parameter
 * block
 */
typedef struct rpc_ss_marsh_state_t
{
    rpc_mp_t mp;                      /* unmarshalling pointer */
    unsigned long op;                 /* offset from start of parameters */
    ndr_format_t src_drep;            /* sender's data representation */
    rpc_iovector_elt_t *p_rcvd_data;  /* address of received data descriptor */
    rpc_ss_mem_handle *p_mem_h;       /* ptr to stub memory management handle */
    rpc_call_handle_t call_h;
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate) _DCE_PROTOTYPE_ (( idl_size_t ));
    void (IDL_ALLOC_ENTRY *p_free) _DCE_PROTOTYPE_ (( rpc_void_p_t )) ;
    rpc_ss_node_table_t node_table;   /* node number to pointer table */
    unsigned long space_in_buff;      /* Space left in buffer */
    rpc_iovector_t  *p_iovec;         /* Address of I/O vector */
    error_status_t  *p_st;            /* Return status */
    unsigned long   version;          /* Version number field */
} rpc_ss_marsh_state_t;


/*
 * MARSHALLING AND UNMARSHALLING OF NODES
 */

typedef enum { rpc_ss_mutable_node_k, rpc_ss_old_ref_node_k,
               rpc_ss_new_ref_node_k, rpc_ss_alloc_ref_node_k,
               rpc_ss_unique_node_k
} rpc_ss_node_type_k_t;

/* 
 * Use address of a function as "an address which cannot be a valid data
 *   address" 
 */

#define RPC_SS_NEW_UNIQUE_NODE (-1)

/*
 * Support operations for pointer <-> node number mapping
 */

void IDL_ENTRY rpc_ss_init_node_table   _DCE_PROTOTYPE_ ((
    volatile rpc_ss_node_table_t *, 
    rpc_ss_mem_handle *
));

void IDL_ENTRY rpc_ss_enable_reflect_deletes  _DCE_PROTOTYPE_ ((
 rpc_ss_node_table_t 
));

long IDL_ENTRY rpc_ss_register_node   _DCE_PROTOTYPE_ ((
    rpc_ss_node_table_t  /*tab*/,
    byte_p_t  /*ptr*/,
    long  /*marshalling*/,
    long * /*has_been_marshalled*/
));


byte_p_t IDL_ENTRY rpc_ss_lookup_node_by_num   _DCE_PROTOTYPE_ ((
    rpc_ss_node_table_t  /*tab*/,
    unsigned long /*num*/
));

byte_p_t IDL_ENTRY rpc_ss_lookup_pointer_to_node   _DCE_PROTOTYPE_ ((
    rpc_ss_node_table_t  /*tab*/,
    unsigned long  /*num*/,
    long * /*has_been_unmarshalled*/
));

byte_p_t IDL_ENTRY rpc_ss_inquire_pointer_to_node  _DCE_PROTOTYPE_ ((
    rpc_ss_node_table_t /*tab*/,
    unsigned long /*num*/,
    long * /*has_been_unmarshalled*/
));

void IDL_ENTRY rpc_ss_unregister_node   _DCE_PROTOTYPE_ ((
    rpc_ss_node_table_t  /*tab*/,
    byte_p_t  /*ptr*/
));


#define NIDL_BUFF_SIZE 2048

void IDL_ENTRY rpc_ss_marsh_change_buff   _DCE_PROTOTYPE_ ((
    rpc_ss_marsh_state_t * /*msp*/,          /* marshalling state */
    unsigned long /*size_next_structure*/   /* Size needed in marshalling buffer */
));

/*
 *  Stub initialization macros
 */
#if defined(WIN32) && !defined(DCE_INSIDE_DLL)

extern ndr_boolean *rpc_ss_client_is_set_up_dll;
#define rpc_ss_client_is_set_up (*rpc_ss_client_is_set_up_dll)
extern ndr_boolean *rpc_ss_server_is_set_up_dll;
#define rpc_ss_server_is_set_up (*rpc_ss_server_is_set_up_dll)
extern ndr_boolean *rpc_ss_allocate_is_set_up_dll;
#define rpc_ss_allocate_is_set_up (*rpc_ss_allocate_is_set_up_dll)
extern ndr_boolean *rpc_ss_context_is_set_up_dll;
#define rpc_ss_context_is_set_up (*rpc_ss_context_is_set_up_dll)

#else

extern ndr_boolean rpc_ss_client_is_set_up;
extern ndr_boolean rpc_ss_server_is_set_up;
extern ndr_boolean rpc_ss_allocate_is_set_up;
extern ndr_boolean rpc_ss_context_is_set_up;

#endif

void IDL_ENTRY rpc_ss_init_client_once   _DCE_PROTOTYPE_ ((void));

void IDL_ENTRY rpc_ss_init_server_once   _DCE_PROTOTYPE_ ((void));


void IDL_ENTRY rpc_ss_init_allocate_once  _DCE_PROTOTYPE_ ((void));

void IDL_ENTRY rpc_ss_init_context_once   _DCE_PROTOTYPE_ ((void));


#define RPC_SS_INIT_CLIENT if(!rpc_ss_client_is_set_up)rpc_ss_init_client_once();

#define RPC_SS_INIT_SERVER if(!rpc_ss_server_is_set_up)rpc_ss_init_server_once();

#define RPC_SS_INIT_ALLOCATE if(!rpc_ss_allocate_is_set_up)rpc_ss_init_allocate_once();

#define RPC_SS_INIT_CONTEXT if(!rpc_ss_context_is_set_up)rpc_ss_init_context_once();

#if defined(VMS) || defined(__VMS)
#    define IDL_ENABLE_STATUS_MAPPING
#    undef RPC_SS_INIT_CLIENT
#    define RPC_SS_INIT_CLIENT if(!rpc_ss_client_is_set_up) \
        { \
            rpc_ss_init_client_once(); \
            rpc_ss_client_is_set_up = ndr_true; \
        }
#    undef RPC_SS_INIT_SERVER
#    define RPC_SS_INIT_SERVER if(!rpc_ss_server_is_set_up) \
        { \
            rpc_ss_init_server_once(); \
            rpc_ss_server_is_set_up = ndr_true; \
        }
#    undef RPC_SS_INIT_ALLOCATE
#    define RPC_SS_INIT_ALLOCATE if(!rpc_ss_allocate_is_set_up) \
        { \
            rpc_ss_init_allocate_once(); \
            rpc_ss_allocate_is_set_up = ndr_true; \
        }
#    undef RPC_SS_INIT_CONTEXT
#    define RPC_SS_INIT_CONTEXT if(!rpc_ss_context_is_set_up) \
        { \
            rpc_ss_init_context_once(); \
            rpc_ss_context_is_set_up = ndr_true; \
        }
#endif /* VMS */

#ifdef MEMORY_NOT_WRITTEN_SERIALLY

#    undef RPC_SS_INIT_CLIENT
#    define RPC_SS_INIT_CLIENT rpc_ss_init_client_once();

#    undef RPC_SS_INIT_SERVER
#    define RPC_SS_INIT_SERVER rpc_ss_init_server_once();
#endif

/*
 *  CMA SUPPORT ROUTINES
 */

void IDL_ENTRY rpc_ss_send_server_exception   _DCE_PROTOTYPE_ ((
    rpc_call_handle_t,
    EXCEPTION *
));

void IDL_ENTRY  rpc_ss_report_error   _DCE_PROTOTYPE_ ((
    ndr_ulong_int  /*fault_code*/,
    ndr_ulong_int  /*result_code*/,
    RPC_SS_THREADS_CANCEL_STATE_T  /*async_cancel_state*/,
    error_status_t * /*p_comm_status*/,
    error_status_t * /*p_fault_status*/
));

/*
 * OPTIMIZATION SUPPORT
 */
void IDL_ENTRY rpc_ss_new_recv_buff   _DCE_PROTOTYPE_ ((
    rpc_iovector_elt_t * /*elt*/,
    rpc_call_handle_t  /*call_h*/,
    rpc_mp_t * /*p_mp*/,
    volatile error_status_t * /*st*/
));


/*
 * SUPPORT FOR MULTI-THREADING
 */

/*
 * Thread local structure to give node support routines access to stub
 * variables
 */

typedef struct rpc_ss_thread_support_ptrs_t
{
    RPC_SS_THREADS_MUTEX_T mutex;    /* Helper thread protection */
    rpc_ss_mem_handle *p_mem_h;
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate) _DCE_PROTOTYPE_ ((idl_size_t));
    void (IDL_ALLOC_ENTRY *p_free) _DCE_PROTOTYPE_ (( rpc_void_p_t)) ;
} rpc_ss_thread_support_ptrs_t;

/*
 *  Thread local storage to indirect to the support pointers structure
 */
typedef struct rpc_ss_thread_indirection_t
{
    rpc_ss_thread_support_ptrs_t *indirection;
    idl_boolean free_referents;     /* TRUE => release the referents of
                                    indirection at thread termination */
} rpc_ss_thread_indirection_t;

extern RPC_SS_THREADS_KEY_T rpc_ss_thread_supp_key;

void IDL_ENTRY rpc_ss_build_indirection_struct  _DCE_PROTOTYPE_ ((
    rpc_ss_thread_support_ptrs_t * /*p_thread_support_ptrs*/,
    rpc_ss_mem_handle * /*p_mem_handle*/,
    idl_boolean  /*free_referents*/
));

void IDL_ENTRY rpc_ss_create_support_ptrs   _DCE_PROTOTYPE_ ((
    rpc_ss_thread_support_ptrs_t * /*p_thread_support_ptrs*/,
    rpc_ss_mem_handle * /*p_mem_handle*/
));

void IDL_ENTRY rpc_ss_get_support_ptrs  _DCE_PROTOTYPE_ ((
    rpc_ss_thread_support_ptrs_t ** /*p_p_thread_support_ptrs*/
));

void IDL_ENTRY rpc_ss_destroy_support_ptrs   _DCE_PROTOTYPE_ ((void));


void IDL_ENTRY rpc_ss_client_establish_alloc  _DCE_PROTOTYPE_ ((rpc_ss_marsh_state_t *));

/*
 *    MARSHALLING AND UNMARSHALLING PIPES
 */

#define NIDL_PIPE_BUFF_SIZE 2048

/*
 * Data structure for maintaining pipe status
 */

typedef struct rpc_ss_ee_pipe_state_t
{
    long pipe_number;
    long next_in_pipe;               /* if -ve, next pipe is [out] */
    long next_out_pipe;
    long *p_current_pipe;            /* +ve curr pipe is [in], -ve for [out] */
    unsigned long left_in_wire_array;
    rpc_mp_t *p_mp;                  /* address of marshalling pointer */
    rpc_op_t *p_op;                  /* address of offset pointer */
    ndr_format_t src_drep;           /* sender's data representation */
    rpc_iovector_elt_t *p_rcvd_data; /* addr of received data descriptor */
    rpc_ss_mem_handle *p_mem_h;      /* ptr to stub memory management handle */
    rpc_call_handle_t call_h;
    ndr_boolean pipe_drained;         /* used only when pipe is [in] */
    ndr_boolean pipe_filled;          /* used only when pipe is [out] */
    error_status_t *p_st;             /* address of status in the stub */
} rpc_ss_ee_pipe_state_t;

void IDL_ENTRY rpc_ss_initialize_callee_pipe   _DCE_PROTOTYPE_ ((
    long ,        /* index of pipe in set of pipes in the
                                        operation's parameter list */
    long ,      /* index of next [in] pipe to process */
    long ,     /* index of next [out] pipe to process */
    long *,            /* ptr to index num and dirn of active pipe */
    rpc_mp_t *,                  /* ptr to marshalling pointer */
    rpc_op_t *,                  /* ptr to offset pointer */
    ndr_format_t ,           /* sender's data representation */
    rpc_iovector_elt_t *, /* Addr of received data descriptor */
    rpc_ss_mem_handle *,      /* ptr to stub memory allocation handle */
    rpc_call_handle_t ,
    rpc_ss_ee_pipe_state_t **, /* address of ptr to pipe state block */
    error_status_t *
));

#define rpc_p_pipe_state ((rpc_ss_ee_pipe_state_t *)state)


/*
 *    CONTEXT HANDLE STUFF
 */

typedef struct
{
    ndr_context_handle context_on_wire;
    handle_t using_handle;
} rpc_ss_caller_context_element_t;

extern RPC_SS_THREADS_MUTEX_T rpc_ss_context_table_mutex;

/*
 * typedef for context rundown procedure 
 */

typedef void (IDL_ENTRY *ctx_rundown_fn_p_t)(rpc_ss_context_t);

void IDL_ENTRY rpc_ss_er_ctx_to_wire   _DCE_PROTOTYPE_ ((
    rpc_ss_context_t        ,  /* [in] opaque pointer */
    ndr_context_handle      *, /* [out] ndr_context_handle */
    handle_t                ,               /* binding handle */
    ndr_boolean             ,          /* TRUE for [in, out] parameters */
    volatile error_status_t *
));

void IDL_ENTRY rpc_ss_er_ctx_from_wire   _DCE_PROTOTYPE_ ((
    ndr_context_handle      *,   /* [in] ndr_context_handle */
    rpc_ss_context_t        *, /* [out] opaque pointer */
    handle_t                ,                 /* binding handle */
    ndr_boolean             ,            /* TRUE for [in, out] parameters */
    volatile error_status_t *
));



void IDL_ENTRY rpc_ss_ee_ctx_to_wire   _DCE_PROTOTYPE_ ((
    rpc_ss_context_t        ,   /* [in] opaque pointer */
    ndr_context_handle      *,  /* [out] ndr_context_handle */
    handle_t                ,   /* binding handle */
    ctx_rundown_fn_p_t      ,   /* Pointer to context rundown routine */
    ndr_boolean             ,   /* TRUE for [in, out] parameters */
    volatile error_status_t *
));

void IDL_ENTRY rpc_ss_ee_ctx_from_wire   _DCE_PROTOTYPE_ ((
    ndr_context_handle      *,   /* [in] ndr_context_handle */
    rpc_ss_context_t        *, /* [out] opaque pointer */
    volatile error_status_t *
));

void IDL_ENTRY rpc_ss_ctx_client_ref_count_inc   _DCE_PROTOTYPE_ ((
  handle_t ,
  error_status_t *
));

void IDL_ENTRY rpc_ss_ctx_client_ref_count_dec   _DCE_PROTOTYPE_ ((
  handle_t h, 
  error_status_t *
));

void IDL_ENTRY rpc_ss_ctx_client_ref_count_i_2   _DCE_PROTOTYPE_ ((
    handle_t ,
    rpc_client_handle_t *,
    error_status_t *
));

void IDL_ENTRY rpc_ss_ctx_client_ref_count_d_2 _DCE_PROTOTYPE_ ((
    handle_t ,
    rpc_client_handle_t
));

void rpc_ss_init_callee_ctx_tables   _DCE_PROTOTYPE_ ((void));

#define uuid_tOmr rpc_ss_m_uuid
#define uuid_tOur rpc_ss_u_uuid
#define uuid_tOme rpc_ss_m_uuid
#define uuid_tOue rpc_ss_u_uuid

void IDL_ENTRY rpc_ss_m_uuid  _DCE_PROTOTYPE_ ((
   uuid_t *, 
   rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_u_uuid   _DCE_PROTOTYPE_ ((
   uuid_t *, 
   rpc_ss_marsh_state_t *
));

/*
 *    MARSHALLING AND UNMARSHALLING OF POINTED AT SCALARS
 */

void IDL_ENTRY rpc_ss_mr_boolean   _DCE_PROTOTYPE_ ((
     idl_boolean *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_boolean   _DCE_PROTOTYPE_ ((
    idl_boolean **,
    rpc_ss_node_type_k_t, 
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_boolean   _DCE_PROTOTYPE_ ((
    idl_boolean *,
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_boolean   _DCE_PROTOTYPE_ ((
    idl_boolean **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_byte   _DCE_PROTOTYPE_ ((
    idl_byte *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_byte   _DCE_PROTOTYPE_ ((
    idl_byte **, 
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_byte   _DCE_PROTOTYPE_ ((
    idl_byte *,
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY  rpc_ss_ue_byte   _DCE_PROTOTYPE_ ((
    idl_byte **, 
    rpc_ss_node_type_k_t , 
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_char   _DCE_PROTOTYPE_ ((
    idl_char *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_char   _DCE_PROTOTYPE_ ((
    idl_char **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_char   _DCE_PROTOTYPE_ ((
    idl_char *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_char   _DCE_PROTOTYPE_ ((
    idl_char **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));


void IDL_ENTRY rpc_ss_mr_enum   _DCE_PROTOTYPE_ ((
    int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_enum   _DCE_PROTOTYPE_ ((
    int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_enum   _DCE_PROTOTYPE_ ((
    int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_enum   _DCE_PROTOTYPE_ ((
    int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_small_int   _DCE_PROTOTYPE_ ((
    idl_small_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_small_int   _DCE_PROTOTYPE_ ((
    idl_small_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_small_int   _DCE_PROTOTYPE_ ((
    idl_small_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_small_int   _DCE_PROTOTYPE_ ((
    idl_small_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_short_int   _DCE_PROTOTYPE_ ((
    idl_short_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_short_int   _DCE_PROTOTYPE_ ((
    idl_short_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_short_int  _DCE_PROTOTYPE_ ((
    idl_short_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_short_int  _DCE_PROTOTYPE_ ((
    idl_short_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_long_int  _DCE_PROTOTYPE_ ((
    idl_long_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_long_int  _DCE_PROTOTYPE_ ((
    idl_long_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_long_int  _DCE_PROTOTYPE_ ((
    idl_long_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_long_int  _DCE_PROTOTYPE_ ((
    idl_long_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));


void  IDL_ENTRY rpc_ss_mr_hyper_int  _DCE_PROTOTYPE_ ((
    idl_hyper_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_hyper_int  _DCE_PROTOTYPE_ ((
    idl_hyper_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_hyper_int  _DCE_PROTOTYPE_ ((
    idl_hyper_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_hyper_int  _DCE_PROTOTYPE_ ((
    idl_hyper_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_usmall_int  _DCE_PROTOTYPE_ ((
    idl_usmall_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_usmall_int  _DCE_PROTOTYPE_ ((
    idl_usmall_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));


void IDL_ENTRY rpc_ss_me_usmall_int   _DCE_PROTOTYPE_ ((
    idl_usmall_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_usmall_int  _DCE_PROTOTYPE_ ((
    idl_usmall_int **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_ushort_int  _DCE_PROTOTYPE_ ((
    idl_ushort_int *, 
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_ushort_int  _DCE_PROTOTYPE_ ((
    idl_ushort_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_ushort_int  _DCE_PROTOTYPE_ ((
    idl_ushort_int *,
    rpc_ss_node_type_k_t, 
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_ushort_int  _DCE_PROTOTYPE_ ((
    idl_ushort_int **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_ulong_int  _DCE_PROTOTYPE_ ((
    idl_ulong_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_ulong_int  _DCE_PROTOTYPE_ ((
    idl_ulong_int **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_ulong_int  _DCE_PROTOTYPE_ ((
    idl_ulong_int *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_ulong_int  _DCE_PROTOTYPE_ ((
    idl_ulong_int **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_uhyper_int  _DCE_PROTOTYPE_ ((
    idl_uhyper_int *, 
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_uhyper_int  _DCE_PROTOTYPE_ ((
    idl_uhyper_int **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_uhyper_int _DCE_PROTOTYPE_ ((
    idl_uhyper_int *,
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_uhyper_int  _DCE_PROTOTYPE_ ((
    idl_uhyper_int **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_short_float  _DCE_PROTOTYPE_ ((
    idl_short_float *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_short_float  _DCE_PROTOTYPE_ ((
    idl_short_float **,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_short_float  _DCE_PROTOTYPE_ ((
    idl_short_float *,
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_short_float  _DCE_PROTOTYPE_ ((
    idl_short_float **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_mr_long_float  _DCE_PROTOTYPE_ ((
    idl_long_float *, 
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ur_long_float   _DCE_PROTOTYPE_ ((
    idl_long_float **,
    rpc_ss_node_type_k_t,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_me_long_float  _DCE_PROTOTYPE_ ((
    idl_long_float *, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

void IDL_ENTRY rpc_ss_ue_long_float  _DCE_PROTOTYPE_ ((
    idl_long_float **, 
    rpc_ss_node_type_k_t ,
    rpc_ss_marsh_state_t *
));

/*
 *  AUTO_HANDLE SUPPORT
 */

void IDL_ENTRY rpc_ss_make_import_cursor_valid  _DCE_PROTOTYPE_ ((
     RPC_SS_THREADS_MUTEX_T *, 
     rpc_ns_import_handle_t  *, 
     rpc_if_handle_t ,
     error_status_t *
));

void IDL_ENTRY rpc_ss_import_cursor_advance   _DCE_PROTOTYPE_ ((
    RPC_SS_THREADS_MUTEX_T *,
    idl_boolean *,
    rpc_ns_import_handle_t *,
    rpc_if_handle_t ,
    ndr_boolean *,
    rpc_binding_handle_t *,
    rpc_binding_handle_t *,
    error_status_t *,
    error_status_t *
));

void IDL_ENTRY rpc_ss_flag_error_on_binding   _DCE_PROTOTYPE_ ((
   RPC_SS_THREADS_MUTEX_T *,
    ndr_boolean *,
    rpc_binding_handle_t *,
    rpc_binding_handle_t *
));

/*
 *  CALL END, GETTING FAULT IF THERE IS ONE
 */

void IDL_ENTRY rpc_ss_call_end   _DCE_PROTOTYPE_ ((
    volatile rpc_call_handle_t *,
    volatile ndr_ulong_int *,
    volatile error_status_t *
));

void IDL_ENTRY rpc_ss_call_end_2   _DCE_PROTOTYPE_ ((
    volatile rpc_call_handle_t *,
    volatile ndr_ulong_int *,
    volatile ndr_ulong_int *,
    volatile error_status_t *
));


/*
 * NDR CONVERSIONS
 */

#ifndef NDRCHARP_C
#if (defined(VMS) || defined(__VMS)) && (defined(__DECC) || defined(__DECCXX))
#pragma extern_model save
#pragma extern_model strict_refdef "NDR_G_ASCII_TO_EBCDIC"
extern rpc_trans_tab_p_t ndr_g_ascii_to_ebcdic;
#pragma extern_model strict_refdef "NDR_G_EBCDIC_TO_ASCII"
extern rpc_trans_tab_p_t ndr_g_ebcdic_to_ascii;
#pragma extern_model restore
#else

globalref rpc_trans_tab_p_t ndr_g_ascii_to_ebcdic;
globalref rpc_trans_tab_p_t ndr_g_ebcdic_to_ascii;

#endif
#endif

void IDL_ENTRY ndr_cvt_string   _DCE_PROTOTYPE_ ((
        ndr_format_t ,
        ndr_format_t ,
        char_p_t ,
        char_p_t 
));

void IDL_ENTRY ndr_cvt_short_float   _DCE_PROTOTYPE_ ((
        ndr_format_t, 
        ndr_format_t, 
        short_float_p_t,
        short_float_p_t
));

void IDL_ENTRY ndr_cvt_long_float   _DCE_PROTOTYPE_ ((
        ndr_format_t ,
        ndr_format_t ,
        long_float_p_t ,
        long_float_p_t 
));


/*
 *  Support routines for marshalling and unmarshalling [string]
 */

idl_ulong_int IDL_ENTRY rpc_ss_strsiz   _DCE_PROTOTYPE_ (( idl_char *, idl_ulong_int));




/*
 * STATUS CODE CONVERSIONS
 */

#ifdef IDL_ENABLE_STATUS_MAPPING

void IDL_ENTRY rpc_ss_map_dce_to_local_status  _DCE_PROTOTYPE_ ((
   error_status_t *   /* [in,out] pointer to DCE status -> local status */
));

void IDL_ENTRY rpc_ss_map_local_to_dce_status    _DCE_PROTOTYPE_ ((
    error_status_t *   /* [in,out] pointer to local status -> DCE status */
));

#endif /* IDL_ENABLE_STATUS_MAPPING */

/*
 * Canned routines that can be used in an ACF [binding_callout] attribute.
 * These routines are called from the client stub.
 */
void IDL_ENTRY rpc_ss_bind_authn_client _DCE_PROTOTYPE_ ((
    rpc_binding_handle_t    *,      /* [io] Binding handle */
    rpc_if_handle_t         ,       /* [in] Interface handle */
    error_status_t          *       /*[out] Return status */
));


#endif  /* !defined(NCK) || defined(NCK_NEED_MARSHALLING) */

/* Matching pragma for one specified above */

#if defined(VMS) || defined(__VMS)

#if defined(__DECC) || defined(__cplusplus)
#pragma extern_model __restore
#endif /* DEC C or C++ */

#pragma standard
#endif /* VMS  */

#ifdef __cplusplus
    }
#endif
#endif  /* STUBBASE_H */
