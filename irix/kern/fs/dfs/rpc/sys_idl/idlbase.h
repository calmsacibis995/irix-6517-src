/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: idlbase.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/30 15:53:47  lmc
 * Include sys/types.h instead of stdlib.h to pick up a typedef for
 * size_t.  stdlib.h causes conflicts with sys/systm.h because of
 * differences between kernel and user space functions.
 *
 * Revision 65.1  1997/10/20  19:16:17  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.942.2  1996/02/18  22:57:19  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:13  marty]
 *
 * Revision 1.1.942.1  1995/12/08  00:23:11  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/jrr_1.2_mothra/1  1995/11/21  16:34 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:10 UTC  dat  /main/dat_xidl2/1]
 * 
 * Revision 1.1.937.3  1994/06/22  20:41:38  tom
 * 	Bug 10979 - Make handle_t a true opaque type.
 * 	[1994/06/22  20:41:21  tom]
 * 
 * Revision 1.1.937.1  1994/01/21  22:39:59  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:30:50  cbrooks]
 * 
 * Revision 1.1.8.3  1993/07/07  20:12:08  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:40:23  ganni]
 * 
 * Revision 1.1.8.2  1993/05/24  20:47:39  cjd
 * 	Submitting 102-dme port to 103i
 * 	[1993/05/24  20:15:25  cjd]
 * 
 * Revision 1.1.6.2  1993/05/12  02:19:38  jd
 * 	Initial 4868 port.
 * 	[1993/05/12  02:17:59  jd]
 * 
 * Revision 1.1.4.4  1993/01/03  23:56:23  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:14:56  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  21:19:48  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:46:38  zeliff]
 * 
 * Revision 1.1.4.2  1992/12/10  21:05:43  hinxman
 * 	OT4497 - Add prototypes for rpc_sm_... routines
 * 	[1992/12/10  21:02:16  hinxman]
 * 
 * Revision 1.1.2.3  1992/05/01  16:05:57  rsalz
 * 	Edited as part of rpc6 drop.
 * 	[1992/05/01  00:19:59  rsalz]
 * 
 * Revision 1.1.2.2  1992/04/06  19:50:32  harrow
 * 	Add prototype for rpc_ss_client_free function.
 * 
 * 	Add idl_size_t and correct rpc_ss_allocate to use it
 * 	to provide a consistant interface with malloc in a portable
 * 	manner.
 * 
 * 	Add definitions of DEC Alpha architecture.
 * 	[1992/04/01  17:58:11  harrow]
 * 
 * Revision 1.1  1992/01/19  03:13:59  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991 by
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
**      idlbase.h
**
**  FACILITY:
**
**      IDL Stub Support Include File
**
**  ABSTRACT:
**
**  This file is #include'd by all ".h" files emitted by the IDL compiler.
**  This file defines various primitives that are missing from C but
**  present in IDL (e.g. booleans, handles).
**
*/

#ifndef IDLBASE_H
#define IDLBASE_H 	1
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#ifndef _DCE_PROTOTYPE_
#define _DCE_PROTOTYPE_(arg) arg 
#define _DCE_PROTO_
#ifndef TRUE 
#define TRUE 1
#define FALSE 0
#endif  /* TRUE */
#endif /* _DCE_PROTOTYPE_ */

#ifdef __cplusplus
    extern "C" {
#endif

/************************** Preprocessor variables *************************/

/*
 * The following variables are defined by the environment somehow:
 *
 *   MSDOS
 *       Means that the system is MS/DOS compatible.
 *   M_I86
 *       Means that the system uses an Intel 8086 cpu.
 *   cray
 *       Means that the system is CRAY/Unicos compatible.
 *   vax (__VAX for ANSI C)
 *       Means that the system uses the VAX architecture.
 *   vaxc
 *       Means that the system uses the VAXC C compiler.
 *   MIPSEL (__MIPSEL for ANSI C)
 *       Means a MIPS processor with little-endian integers
 *   apollo
 *      Means that the system is an Apollo.
 *   __STDC__
 *      Means that ANSI C prototypes are enabled.
 *
 * The following variables are defined (and undefined) within this file
 * to control the definition of macros which are emitted into header
 * files by the IDL compiler.  For each variable there is a set of default
 * definitions which is used unless a target system specific section
 * #undef-s it and supplies an alternate set of definitions.  Exactly
 * which macro definitions are governed by each variable is listed below.
 *
 *   USE_DEFAULT_NDR_REPS
 *      Controls the definition of the macros which assign a particular
 *      target system type to each NDR scalar type.  The following macros
 *      need to be defined if USE_DEFAULT_NDR_REPS is #undef-ed:
 *          ndr_boolean
 *          ndr_false
 *          ndr_true
 *          ndr_byte
 *          ndr_char
 *          ndr_small_int
 *          ndr_short_int
 *          ndr_long_int
 *          ndr_hyper_int
 *          ndr_usmall_int
 *          ndr_ushort_int
 *          ndr_ulong_int
 *          ndr_uhyper_int
 *          ndr_short_float
 *          ndr_long_float
 *
 */

/***************************************************************************/

/*
 * Calling standard macros.  All APIs and SPIs should be decorated with these
 * macros so that the correct calling standard is used on relevant platforms
 * irrespective of compiler flags used.
 */

#if (defined(WIN32) || defined(_MSDOS)) && !defined(_ALPHA_)
#define IDL_ENTRY __stdcall
#define IDL_ALLOC_ENTRY __cdecl
#define IDL_STD_STDCALL     __stdcall
#define IDL_STD_FASTCALL    __fastcall
#define IDL_STD_FORTRAN     __fortran
#define IDL_STD_PASCAL      __pascal
#define IDL_STD_CDECL       __cdecl
#else
#define IDL_ENTRY 
#define IDL_ALLOC_ENTRY 
#define IDL_STD_STDCALL     
#define IDL_STD_FASTCALL    
#define IDL_STD_FORTRAN     
#define IDL_STD_PASCAL      
#define IDL_STD_CDECL       
#endif

/***************************************************************************/

/*
 * Work around C's flawed model for global variable definitions.
 * 1/10/93 CBrooks 
 * this definition now depends on the preprocessor variable 
 * HAS_GLOBALDEF 
 * which should be defined in platform specific dce.h file 
 *
 *
 * To eliminate the dependancy on dce.h, define HAS_GLOBALDEF here
 * if appropriate - 1/12/95 viv
 */

#ifdef vaxc
#  define HAS_GLOBALDEF
#endif

#ifndef HAS_GLOBALDEF
#  define globaldef
#  define globalref extern
#endif /* HAS_GLOBALDEF */

/***************************************************************************/

/*
 * Unless otherwise stated, don't innocously redefine "volatile"
 * (redefining it for compilers that really support it will cause nasty
 * program bugs).  There are several compilers (it's wrong to think in
 * terms of hw platforms) that support volatile yet they don't define
 * "__STDC__", so we can't just use that.
 *
 * So, unless your compiler is explicitly listed below we don't mess
 * with "volatile".  Expressing things in this fashion errs on the cautious
 * side... at worst your compiler will complain and you can enhance the
 * list and/or add "-Dvolatile" to the cc command line.
 *
 * 1/10/93 CBrooks 
 * this definition now depends on the preprocessor variable 
 * VOLATILE_NOT_SUPPORTED 
 * which should be defined in platform specific dce.h file 
 *
 */

#ifdef VOLATILE_NOT_SUPPORTED 
#  define volatile
#endif

/***************************************************************************/

/*
 *  Define IDL_PROTOTYPES to control function prototyping.
 *  Define IDL_NO_PROTOTYPES to hide prototypes regardless of conditions.
 *  Define NIDL_PROTOTYPES for compatibility.
 *
 * 12/20/83 CBrooks - use _DCE_PROTOTYPE_ instead, since this should 
 * apply to all of DCE for a particular platform
 */

#ifdef _DCE_PROTO_
#if ! defined(IDL_PROTOTYPES) && ! defined(IDL_NO_PROTOTYPES)
#define IDL_PROTOTYPES 1
#endif
#endif

/***************************************************************************/


/*
 * define true and false
 */

#ifndef true

#ifdef NIDL_bug_boolean_def
#   define true        0xFF
#else
#   define true        TRUE
#endif /* NIDL_bug_boolean_def */

#endif /* true */

#ifndef false
#   define false       FALSE 
#endif /* false */

/***************************************************************************/

/*
 * The definition of the primitive "handle_t" IDL type.
 */

#if defined(_MSDOS) || defined(WIN32)
  typedef void *handle_t;
#else
  typedef struct rpc_handle_s_t *handle_t;
#endif

/***************************************************************************/

/*
 * Use the default definitions for representations for NDR scalar types
 * (unless some target specific section below #undef's the symbol) of
 * these symbols.
 *
 * 1/10/94 CBrooks 
 * for DCE 1.1, we include the platform specific file ndrtypes.h 
 *
 */

#include <dce/ndrtypes.h>

/***************************************************************************/

typedef ndr_boolean		idl_boolean ;

#define idl_false ndr_false 
#define idl_true  ndr_true 

typedef ndr_byte		idl_byte ;

/*
 * when compiling DCE programs and/or libraries, we want the base type
 * of idl_char to be "unsigned char" (IDL doesn't support signed chars).
 * However, when compiling external programs, we want idl_char to have 
 * the char type native to the platform on which the program is being
 * compiled. So use a macro that should only be defined if we 
 * are building the RPC runtime of the IDL compiler.
 */

#ifndef idl_char 
#ifndef IDL_CHAR_IS_CHAR 
    typedef unsigned char idl_char ;
#else 
    typedef char idl_char ;
#endif /* IDL_CHAR_IS_CHAR */
#endif /* idl_char */

typedef unsigned char           idl_uchar ;

typedef ndr_small_int		idl_small_int ;

typedef ndr_usmall_int		idl_usmall_int ;

typedef ndr_short_int		idl_short_int ;

typedef ndr_ushort_int		idl_ushort_int ;

typedef ndr_long_int		idl_long_int ;

typedef ndr_ulong_int		idl_ulong_int ;

typedef ndr_hyper_int		idl_hyper_int ;

typedef ndr_uhyper_int		idl_uhyper_int ;

typedef ndr_short_float		idl_short_float ;

typedef ndr_long_float		idl_long_float ;


/* In order for rpc_ss_allocate and malloc to be used interchangably 
 * <stdlib.h> must be pulled in to reference size_t directly, as this
 * is the correct architectural way to fix this...
 */

#ifdef __hpux
  typedef ndr_ulong_int         idl_size_t;
#else
#ifndef SGIMIPS
  #include <stdlib.h>
#else
  #include <sys/types.h>
#endif
  typedef size_t                idl_size_t;
#endif /* __hpux */


/* 12/20/93 CBrooks - used pointer_t  */

typedef void * idl_void_p_t ;

/*
 *  Opaque data types
 */

typedef idl_void_p_t rpc_ss_context_t;
typedef idl_void_p_t rpc_ss_pipe_state_t;
typedef idl_void_p_t ndr_void_p_t;

/*
 *  Allocate and free node storage
 */

idl_void_p_t IDL_ALLOC_ENTRY rpc_ss_allocate _DCE_PROTOTYPE_ ( (idl_size_t));

void IDL_ALLOC_ENTRY rpc_ss_free _DCE_PROTOTYPE_ ( (idl_void_p_t) );

void IDL_ENTRY rpc_ss_client_free _DCE_PROTOTYPE_ ( (idl_void_p_t) );

/*
 *  Helper thread support
 */

typedef idl_void_p_t rpc_ss_thread_handle_t;

rpc_ss_thread_handle_t IDL_ENTRY rpc_ss_get_thread_handle _DCE_PROTOTYPE_ ( (void) );

void IDL_ENTRY rpc_ss_set_thread_handle _DCE_PROTOTYPE_ ( (rpc_ss_thread_handle_t) );

void IDL_ENTRY rpc_ss_set_client_alloc_free _DCE_PROTOTYPE_ ((
     idl_void_p_t (IDL_ALLOC_ENTRY *)(idl_size_t),
    void (IDL_ALLOC_ENTRY *)(idl_void_p_t)
));

 
void IDL_ENTRY rpc_ss_swap_client_alloc_free _DCE_PROTOTYPE_ ((
    idl_void_p_t (IDL_ALLOC_ENTRY *)(idl_size_t),
    void (IDL_ALLOC_ENTRY *)(idl_void_p_t),
    idl_void_p_t (IDL_ALLOC_ENTRY **)(idl_size_t),
    void (IDL_ALLOC_ENTRY **)( idl_void_p_t)
));

void IDL_ENTRY rpc_ss_enable_allocate _DCE_PROTOTYPE_ ( (void) );

void IDL_ENTRY rpc_ss_disable_allocate _DCE_PROTOTYPE_ ( (void) );

/*
 * Destroy an unusable client context handle
 */
void IDL_ENTRY rpc_ss_destroy_client_context _DCE_PROTOTYPE_ ( (rpc_ss_context_t *) );



/*
 *  Prototypes for rpc_sm_... routines
 */

idl_void_p_t IDL_ENTRY rpc_sm_allocate _DCE_PROTOTYPE_ ((idl_size_t, idl_ulong_int *));

void IDL_ENTRY rpc_sm_client_free _DCE_PROTOTYPE_ ((idl_void_p_t, idl_ulong_int *));

void IDL_ENTRY rpc_sm_destroy_client_context  _DCE_PROTOTYPE_ ((
    rpc_ss_context_t *,
    idl_ulong_int * 
));

void IDL_ENTRY rpc_sm_disable_allocate _DCE_PROTOTYPE_ ( (idl_ulong_int * ) );

void IDL_ENTRY rpc_sm_enable_allocate _DCE_PROTOTYPE_ ( (  idl_ulong_int * ) );

void IDL_ENTRY rpc_sm_free _DCE_PROTOTYPE_ ( (idl_void_p_t, idl_ulong_int * ) );

rpc_ss_thread_handle_t IDL_ENTRY rpc_sm_get_thread_handle _DCE_PROTOTYPE_ ( (idl_ulong_int * ) );

void IDL_ENTRY rpc_sm_set_client_alloc_free  _DCE_PROTOTYPE_ ((
    idl_void_p_t (IDL_ALLOC_ENTRY *)(idl_size_t),
    void (IDL_ALLOC_ENTRY *)(idl_void_p_t ),
    idl_ulong_int *
));

void IDL_ENTRY rpc_sm_set_thread_handle _DCE_PROTOTYPE_ ( ( rpc_ss_thread_handle_t , idl_ulong_int * ) );

void IDL_ENTRY rpc_sm_swap_client_alloc_free _DCE_PROTOTYPE_ ((
     idl_void_p_t (IDL_ALLOC_ENTRY *)(idl_size_t),
    void (IDL_ALLOC_ENTRY *)(idl_void_p_t),
    idl_void_p_t (IDL_ALLOC_ENTRY **)(idl_size_t),
    void (IDL_ALLOC_ENTRY **)(idl_void_p_t),
    idl_ulong_int *
));

/* International character machinery */

typedef enum {
    idl_cs_no_convert,          /* No codeset conversion required */
    idl_cs_in_place_convert,    /* Codeset conversion can be done in a single
                                    storage area */
    idl_cs_new_buffer_convert   /* The converted data must be written to a
                                    new storage area */
} idl_cs_convert_t;

#ifdef __cplusplus
    }
#endif

#ifdef DCEPrototypesDefinedLocally
#undef DCEPrototypesDefinedLocally
#endif

#endif /* IDLBASE_H */

/***************************************************************************/

/*
 *  Cause a syntax error if compiling on VAX and G_float not specified when any
 *  doubles are used.  Note that on VAX G_float *must* be used by the
 *  application, or incorrect double floating point values will be returned. 
 *  This test should remain outside the IDLBASE_H conditional, because it
 *  needs to be checked on every include.
 *
 *  This need not be done for DEC C or DEC C++
 */

#if defined(IDL_DOUBLE_USED) && ( defined(VAX) || defined(__VAX) || defined(vax) || defined(__vax) )

#if defined(vaxc) && !defined(__cplusplus) && !defined(__DECC)
#  if CC$gfloat != 1
#    if defined(vms) || defined(__VMS)
 #error "RPC requires VAX G_float data format: build with /G_FLOAT qualifier"
#    else
 #error "RPC requires VAX G_float data format: build with -Mg option"
#    endif /* vsm OR __VMS  */
#  endif   /* CC$gloat */
#endif /* vaxc && ! c++ && ! __DECC */

#if defined(__cplusplus) || defined(__DECC)
#  if __G_FLOAT != 1
 #error "RPC requires VAX G_float data format: build with /FLOAT=G_FLOAT qualifier"
#  endif/* __G_FLOAT */
#endif /* c++ or __DECC */

#if defined(ultrix) && defined(vax) && !defined(vaxc)
#  if GFLOAT != 1
 #error \ 
"RPC requires GFLOAT:RPC requires VAX G_float data format: build with -Mg option"
#  endif/* GFLOAT */
#endif /* ultrix && vax && ! vaxc */

#endif /* IDL_DOUBLE_USED */

