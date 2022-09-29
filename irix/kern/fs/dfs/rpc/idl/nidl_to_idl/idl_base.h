/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: idl_base.h,v $
 * Revision 65.1  1997/10/20 19:15:47  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.4.2  1996/02/18  23:46:03  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:49  marty]
 *
 * Revision 1.1.4.1  1995/12/07  22:36:10  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:18:07  root]
 * 
 * Revision 1.1.2.3  1993/01/03  22:14:53  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:47:54  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:15:32  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  01:11:05  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:00:29  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
 *
 * ========================================================================== 
 * Confidential and Proprietary.  Copyright 1987 by Apollo Computer Inc.,
 * Chelmsford, Massachusetts.  Unpublished -- All Rights Reserved Under
 * Copyright Laws Of The United States.
 *
 * Apollo Computer Inc. reserves all rights, title and interest with respect 
 * to copying, modification or the distribution of such software programs and
 * associated documentation, except those rights specifically granted by Apollo
 * in a Product Software Program License, Source Code License or Commercial
 * License Agreement (APOLLO NETWORK COMPUTING SYSTEM) between Apollo and 
 * Licensee.  Without such license agreements, such software programs may not
 * be used, copied, modified or distributed in source or object code form.
 * Further, the copyright notice must appear on the media, the supporting
 * documentation and packaging as set forth in such agreements.  Such License
 * Agreements do not grant any rights to use Apollo Computer's name or trademarks
 * in advertising or publicity, with respect to the distribution of the software
 * programs without the specific prior written permission of Apollo.  Trademark 
 * agreements may be obtained in a separate Trademark License Agreement.
 * ========================================================================== 
 */


/*
 * This file is #include'd by ALL C files emitted by the NIDL compiler.
 *
 * This file serves two roles:  (1) It defines various primitives that
 * are missing from C but present in NIDL (e.g. booleans, handles); (2)
 * It defines certain things referred to by generated stub code.
 */

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
 *   vax
 *       Means that the system uses the VAX architecture.
 *   vaxc
 *       Means that the system uses the VAXC C compiler.
 *   apollo
 *      Means that the system is an Apollo.
 *   hpux
 *      Means that the system is an HP-UX.
 *   hp9000s800
 *      Means that the system is a HP9000/8xx
 *   __STDC__
 *      Means that ANSI C prototypes are enabled.
 *   NIDL_CSTUB
 *      Means that a NIDL generated client stub is being compiled.
 *   NIDL_SSTUB
 *      Means that a NIDL generated server stub is being compiled.
 *
 * The following variables are defined (and undefined) within this file
 * to control the definition of macros which are emitted into header
 * files and stub code by the NIDL compiler.  For each variable there
 * is a set of default definitions which is used unless a target system
 * specific section #undef-s it and supplies an alternate set of
 * definitions.  Exactly which macro definitions are governed by each
 * variable is listed below.
 *
 *   USE_DEFAULT_NDR_REPS
 *      Controls the definition of the macros which assign a particular
 *      target system type to each NDR scalar type.  The following macros
 *      need to be defined if USE_DEFAULT_NDR_REPS is #undef-ed:
 *          ndr_$boolean
 *          ndr_$false
 *          ndr_$true
 *          ndr_$byte
 *          ndr_$char
 *          ndr_$small_int
 *          ndr_$short_int
 *          ndr_$long_int
 *          ndr_$hyper_int
 *          ndr_$usmall_int
 *          ndr_$ushort_int
 *          ndr_$ulong_int
 *          ndr_$uhyper_int
 *          ndr_$short_float
 *          ndr_$long_float
 *
 *   USE_DEFAULT_MP_REP
 *      Controls the definition of a type and the macros which define
 *      the marshalling pointer scheme used on a particular target system.
 *      The following macros need to be defined if USE_DEFAULT_MP_REP
 *      is #undef-ed:
 *          rpc_$init_mp
 *          rpc_$advance_mp
 *          rpc_$align_ptr_relative
 *
 *      and the following type needs to be typedef-ed:
 *          rpc_$mp_t 
 *
 *   USE_DEFAULT_MACROS
 *      Controls the definition of the macros which define how to marshall,
 *      unmarshall and convert values of each NDR scalar type as well
 *      as NDR string types.  The following macros need to be defined
 *      if USE_DEFAULT_MACROS is #undef-ed:
 *          rpc_$marshall_boolean, rpc_$unmarshall_boolean, rpc_$convert_boolean
 *          rpc_$marshall_byte, rpc_$unmarshall_byte, rpc_$convert_byte
 *          rpc_$marshall_char, rpc_$unmarshall_char, rpc_$convert_char
 *          rpc_$marshall_small_int, rpc_$unmarshall_small_int, rpc_$convert_small_int
 *          rpc_$marshall_usmall_int, rpc_$unmarshall_usmall_int, rpc_$convert_usmall_int
 *          rpc_$marshall_short_int, rpc_$unmarshall_short_int, rpc_$convert_short_int
 *          rpc_$marshall_ushort_int, rpc_$unmarshall_ushort_int, rpc_$convert_ushort_int
 *          rpc_$marshall_long_int, rpc_$unmarshall_long_int, rpc_$convert_long_int
 *          rpc_$marshall_ulong_int, rpc_$unmarshall_ulong_int, rpc_$convert_ulong_int
 *          rpc_$marshall_hyper_int, rpc_$unmarshall_hyper_int, rpc_$convert_hyper_int
 *          rpc_$marshall_uhyper_int, rpc_$unmarshall_uhyper_int, rpc_$convert_uhyper_int
 *          rpc_$marshall_short_float, rpc_$unmarshall_short_float, rpc_$convert_short_float
 *          rpc_$marshall_long_float, rpc_$unmarshall_long_float, rpc_$convert_long_float
 *          rpc_$marshall_string, rpc_$unmarshall_string, rpc_$convert_string
 *
 *
 * The following variable is used to support an optimization in marshalling
 * and unmarshalling arrays of scalars.
 *   ALIGNED_SCALAR_ARRAYS
 *      Defining this variable asserts that the target C compiler's layout
 *      for arrays of scalars matches NDR's.  The NIDL compiler emits
 *      code to marshall and unmarshall arrays of scalars which is
 *      conditioned on this variable: if ALIGNED_SCALAR_ARRAYS is defined
 *      then the code uses rpc_$block_copy (which must be defined whenever
 *      ALIGNED_SCALAR_ARRAYS is defined); if ALIGNED_SCALAR_ARRAYS is
 *      not defined then the NIDL compiler emits a loop to marshall or
 *      unmarshall the array element-wise.  Note that converting arrays
 *      of scalars must always be done element-wise.
 */

/***************************************************************************/ 
/***************************************************************************/ 

#ifndef idl_base

#define idl_base

/***************************************************************************/ 

/*
 * Cause a syntax error if wrong options are used with Vax C compiler.
 */

#ifdef vaxc
#  if CC$gfloat != 1
        "VAX/C NCK MUST BE BUILT WITH /G_FLOAT (VMS) or -Mg (ULTRIX)"
#  endif
#endif

/***************************************************************************/ 

/*
 * Work around C's flawed model for global variable definitions.
 */

#ifndef vaxc
#  define globaldef
#  define globalref extern
#endif

/***************************************************************************/ 

/*
 * Innocuously define volatile modifier in C implementations that don't support it.
 */

#if (!defined(__STDC__) || defined(apollo)) && !defined(vaxc) && !defined(hpux)
#  define volatile
#endif

/***************************************************************************/ 

/*
 * The definition of the primitive "handle_t" NIDL type.    
 */ 

typedef struct {
    unsigned short data_offset;
} *handle_t;

/***************************************************************************/ 

/*
 * The definition of the primitive "boolean" NIDL type and its two values.
 */ 

#ifndef true
   typedef unsigned char boolean;
#  define true  ((unsigned char) 0xff)
#  define false ((unsigned char) 0x00)
#endif                                    

/***************************************************************************/ 

/* 
 * macro for distinguishing rpc comm failure statuses from other rpc status codes.
 */
 
#define rpc_$is_comm_failure_status(st)\
    (((st).all & 0xffff0000) == rpc_$mod)
/*    ((st).all == rpc_$comm_failure) */

/***************************************************************************/ 

/* Data representation descriptor type.  
 *
 * This type would be defined in "rpc.idl" except NIDL doesn't yet
 * support packed structs.   (It's packed only for historical reasons.)
 * Note well:  Don't be confused into thinking that this struct defines
 * the bit layout of the drep in the packet header; it doesn't.
 */
 
typedef struct {
    unsigned int int_rep: 4;
    unsigned int char_rep: 4;
    unsigned int float_rep: 8;
    unsigned int reserved: 16;
} rpc_$drep_t;


/*
 * Macro used by stubs to compare two rpc_$drep_t's.
 */

#define rpc_$equal_drep(drep1, drep2)\
    (drep1.int_rep==drep2.int_rep)&&(drep1.char_rep==drep2.char_rep)&&(drep1.float_rep==drep2.float_rep)

/***************************************************************************/ 

/*
 * Hack to make the following variables read-only, global, and statically
 * initialized.  For Apollo global libraries.
 */

#ifdef apollo
#  define rpc_$local_drep_packed  rpc_$local_drep_p_pure_data$
#  define rpc_$local_drep         rpc_$local_drep_pure_data$
#  define rpc_$ascii_to_ebcdic    rpc_$ascii_to_ebcdic_pure_data$
#  define rpc_$ebcdic_to_ascii    rpc_$ebcdic_to_ascii_pure_data$
#endif

/***************************************************************************/ 

/*
 * Variable describing the local system's data representation.  The static
 * initialization of this variable is determined by values in "sysdep.h".
 */

globalref rpc_$drep_t rpc_$local_drep;


/*
 * ASCII <-> EBCDIC conversion tables.
 */

globalref char             rpc_$ascii_to_ebcdic[];
globalref char             rpc_$ebcdic_to_ascii[];


/***************************************************************************/ 
/***************************************************************************/ 

/*
 * Use the default definitions for:
 *
 *    1) representations for NDR scalar types,
 *    2) marshalling pointer represenation and manipulation,
 *    3) marshalling, unmarshalling and conversion methods
 *       for NDR scalar and string types
 *
 * unless some target specific section below #undef's one or more 
 * of these symbols.
 */

#define USE_DEFAULT_NDR_REPS
#define USE_DEFAULT_MP_REP
#define USE_DEFAULT_MACROS
#define ALIGNED_SCALAR_ARRAYS

/***************************************************************************/ 

/****
 **** Cray specific definitions of the default reps for NDR scalar types.
 ****/

#if defined(cray) || defined(CRAY) || defined(CRAY1) || defined(CRAY2)

#undef USE_DEFAULT_NDR_REPS

/*
 * ========= WARNING =========
 * ndr_${,u}small_int are not "chars" because no signed char is available
 * Applications developers MUST use shorts or ndr_${u,}small_int in their
 * code since the marshalling/unmarshalling/conversion code expects this.
 */
#define ndr_$boolean        unsigned char
#define ndr_$false          ((unsigned char) 00)
#define ndr_$true           ((unsigned char) 0xff)
#define ndr_$byte           unsigned char
#define ndr_$char           unsigned char
#define ndr_$small_int      short int           /* 24/32 bits => XMP/2 */
#define ndr_$short_int      short int           /* 24/32 bits => XMP/2 */
#define ndr_$long_int       int                 /* 64/32 bits => XMP/2 */
#define ndr_$hyper_int      long int
#define ndr_$usmall_int     unsigned short int  /* 24/32 bits => XMP/2 */
#define ndr_$ushort_int     unsigned short int  /* 24/32 bits => XMP/2 */
#define ndr_$ulong_int      unsigned int        /* 64/32 bits => XMP/2 */
#define ndr_$uhyper_int     unsigned long int
#define ndr_$short_float    float
#define ndr_$long_float     float

#endif

/***************************************************************************/ 

/****
 **** Definitions of the vax and M_I86 reps for NDR scalar types.
 ****/

#if defined(vax) || defined(M_I86)

#undef USE_DEFAULT_NDR_REPS

#define ndr_$boolean        unsigned char
#define ndr_$false          ((unsigned char) 0x00)
#define ndr_$true           ((unsigned char) 0xff)
#define ndr_$byte           unsigned char
#define ndr_$char           unsigned char
#define ndr_$small_int      char
#define ndr_$usmall_int     unsigned char
#define ndr_$short_int      short int
#define ndr_$ushort_int     unsigned short int
#define ndr_$long_int       long int
#define ndr_$ulong_int      unsigned long int

/* the reps for hyper must match the little-endian NDR rep since
   defined(vax) || defined(M_I86) => defined(ALIGNED_SCALAR_ARRAYS) */
struct ndr_$hyper_int_rep   {ndr_$ulong_int low; ndr_$long_int high;};
#define ndr_$hyper_int      struct ndr_$hyper_int_rep
struct ndr_$uhyper_int_rep  {ndr_$ulong_int low; ndr_$ulong_int high;};
#define ndr_$uhyper_int     struct ndr_$uhyper_int_rep

#define ndr_$short_float    float
#define ndr_$long_float     double

#endif

/***************************************************************************/ 

/****
 **** Definitions of the default reps for NDR scalar types.
 ****/

#ifdef USE_DEFAULT_NDR_REPS

#define ndr_$boolean        unsigned char
#define ndr_$false          ((unsigned char) 0x00)
#define ndr_$true           ((unsigned char) 0xff)
#define ndr_$byte           unsigned char
#define ndr_$char           unsigned char
#define ndr_$small_int      char
#define ndr_$usmall_int     unsigned char
#define ndr_$short_int      short int
#define ndr_$ushort_int     unsigned short int
#define ndr_$long_int       long int
#define ndr_$ulong_int      unsigned long int

struct ndr_$hyper_int_rep   {ndr_$long_int high; ndr_$ulong_int low;};
#define ndr_$hyper_int      struct ndr_$hyper_int_rep
struct ndr_$uhyper_int_rep  {ndr_$ulong_int high; ndr_$ulong_int low;};
#define ndr_$uhyper_int     struct ndr_$uhyper_int_rep

#define ndr_$short_float    float
#define ndr_$long_float     double

#endif

/***************************************************************************
 ****
 **** The rest of this file is only included in NIDL generated stub code.
 ****
 ***************************************************************************/


#if defined(NIDL_CSTUB) || defined(NIDL_SSTUB) || defined(NIDL_MRSHL)

/*
 * Define the versions of stubs that this instance of idl_base.h supports.
 */

#define IDL_BASE_SUPPORTS_V1

/*
 * Declare external C library routines that are used by stubs.
 */

#ifdef __STDC__
    char *strcpy(ndr_$char *, ndr_$char *);
    int strlen(ndr_$char *);
#else
    char *strcpy();
    int strlen();
#endif

/*
 * Declare external NCK library routines that are not declarable in
 * rpc.idl that are used by stubs.
 *
 * On Apollo, prior to sr10.2, the /lib/ddslib global library did not
 * define rpc_$malloc/free.  Since we want the stubs (which refer to these
 * routines) to run on pre-sr10.2 systems, we define rpc_$malloc/free
 * macros to do the job.  Note that the rpc_$malloc macro doesn't behave
 * exactly like the rpc_$malloc routine in that the latter raises an
 * exception if it can't get the storage and the former does not (outside
 * the range of possible C macro hacks).  If you want the correct behavior
 * and you have sr10.2 or later, define the preprocessor symbol
 * DDSLIB_HAS_RPC_MALLOC_AND_FREE.  Note also that it's important to use
 * the rws_ routines and not malloc/free since the latter aren't reentrant.
 */

#if defined(apollo) && !defined(DDSLIB_HAS_RPC_MALLOC_AND_FREE)

typedef short enum {
        rws_$std_pool, 
        rws_$stream_tm_pool, 
        rws_$global_pool, 
        rws_$nmr_pool
} rws_$pool_t;

void *rws_$alloc_heap_pool (
        rws_$pool_t     &apool ,
        long            &bytes
);

void rws_$release_heap_pool (
        void                 *&p ,    /* must be ptr returned by alloc */
        rws_$pool_t          &apool ,
        long /* status_$t */ *status
);

#define rpc_$malloc(n) \
    rws_$alloc_heap_pool(rws_$stream_tm_pool, (long) (n))

#define rpc_$free(p) { \
    long status; \
    rws_$release_heap_pool((p), rws_$stream_tm_pool, &status); \
}

#else

void *rpc_$malloc(
#ifdef __STDC__
    int n
#endif
);
void rpc_$free(
#ifdef __STDC__
    void *p
#endif
);

#endif


/*
 * Define some macros that stub code uses for brevity or clarity.
 */

#define NULL 0

#define SIGNAL(code) {\
    status_$t st;\
    st.all = code;\
    pfm_$signal (st);\
    }

/***************************************************************************/ 

/****
 **** Definitons specific to the i8086 with Microsoft C.
 ****/

#if defined(M_I86) && defined(MSDOS)

#undef USE_DEFAULT_MP_REP

#define normalize_ptr(p)\
    *(((unsigned *)&p)+1) += *((unsigned *)&p) >> 4;\
    *((unsigned *)&p) &= 0x000F

typedef char *rpc_$mp_t;

#define rpc_$init_mp(mp, dbp, bufp, offset)\
    mp = (rpc_$mp_t)bufp;\
    normalize_ptr (mp);\
    mp += offset;\
    normalize_ptr (mp);\
    dbp = mp

#define rpc_$advance_mp(mp, delta)\
    mp += delta

#define rpc_$align_ptr_relative(p, base, alignment)\
    p = base + (((p-base) + (alignment-1)) & ~(alignment-1))

#endif

/***************************************************************************/ 

/****
 **** Definitions specific to the vaxc and Microsoft C (MSDOS) C compilers.
 **** These compilers don't handle left hand side casts (which are necessary
 **** when unmarshalling unsigned short and long ints) so some ad hoc hacks
 **** occur in this section.  Apple's AUX also needs this treatment.
 ****/

#if defined(vaxc) || defined(MSDOS) || defined(apple_aux) || defined(hp9000s800) || defined(__hp9000s800)

#undef USE_DEFAULT_MACROS

#define rpc_$marshall_boolean(mp, src)\
    *(ndr_$boolean *)mp = src

#define rpc_$unmarshall_boolean(mp, dst)\
    dst = *(ndr_$boolean *)mp

#define rpc_$convert_boolean(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_boolean(mp, dst)



#define rpc_$marshall_byte(mp, src)\
    *(ndr_$byte *)mp = src

#define rpc_$unmarshall_byte(mp, dst)\
    dst = *(ndr_$byte *)mp

#define rpc_$convert_byte(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_byte(mp, dst)



#define rpc_$marshall_char(mp, src)\
    *(ndr_$char *)mp = src

#define rpc_$unmarshall_char(mp, dst)\
    dst = *(ndr_$char *)mp

#define rpc_$convert_char(src_drep, dst_drep, mp, dst)\
    if (src_drep.char_rep == dst_drep.char_rep)\
        rpc_$unmarshall_char(mp, dst);\
    else if (dst_drep.char_rep == rpc_$drep_char_ascii)\
        dst = rpc_$ebcdic_to_ascii [*(ndr_$char *)mp];\
    else\
        dst = rpc_$ascii_to_ebcdic [*(ndr_$char *)mp]



#define rpc_$marshall_small_int(mp, src)\
    *(ndr_$small_int *)mp = src

#define rpc_$unmarshall_small_int(mp, dst)\
    dst = *(ndr_$small_int *)mp

#define rpc_$convert_small_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_small_int(mp, dst)



#define rpc_$marshall_usmall_int(mp, src)\
    *(ndr_$usmall_int *)mp = src

#define rpc_$unmarshall_usmall_int(mp, dst)\
    dst = *(ndr_$usmall_int *)mp

#define rpc_$convert_usmall_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_usmall_int(mp, dst)



#define rpc_$marshall_short_int(mp, src)\
    *(ndr_$short_int *)mp = src

#define rpc_$unmarshall_short_int(mp, dst)\
    dst = *(ndr_$short_int *)mp

#define rpc_$convert_short_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_short_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_$marshall_ushort_int(mp, src)\
    *(ndr_$ushort_int *)mp = (ndr_$ushort_int)src

#define rpc_$unmarshall_ushort_int(mp, dst)\
    *((ndr_$ushort_int *)&dst) = *(ndr_$ushort_int *)mp

#define rpc_$convert_ushort_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_ushort_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_$marshall_long_int(mp, src)\
    *(ndr_$long_int *)mp = src

#define rpc_$unmarshall_long_int(mp, dst)\
    dst = *(ndr_$long_int *)mp

#define rpc_$convert_long_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_long_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_$marshall_ulong_int(mp, src)\
    *(ndr_$ulong_int *)mp = (ndr_$ulong_int)src

#define rpc_$unmarshall_ulong_int(mp, dst)\
    *((ndr_$ulong_int *)&dst) = *(ndr_$ulong_int *)mp

#define rpc_$convert_ulong_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_ulong_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_$marshall_hyper_int(mp, src) {\
    *(struct ndr_$hyper_int_rep *)mp = *(struct ndr_$hyper_int_rep *)&src;\
    }

#define rpc_$unmarshall_hyper_int(mp, dst) {\
    *(struct ndr_$hyper_int_rep *)&dst = *(struct ndr_$hyper_int_rep *)mp;\
    }

#define rpc_$convert_hyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_hyper_int(mp, dst)\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_$marshall_uhyper_int(mp, src) {\
    *(struct ndr_$uhyper_int_rep *)mp = *(struct ndr_$uhyper_int_rep *)&src;\
    }

#define rpc_$unmarshall_uhyper_int(mp, dst) {\
    *(struct ndr_$uhyper_int_rep *)&dst = *(struct ndr_$uhyper_int_rep *)mp;\
    }

#define rpc_$convert_uhyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_uhyper_int(mp, dst)\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_$marshall_short_float(mp, src) {\
    ndr_$short_float tmp;\
    tmp = src;\
    *(ndr_$short_float *)mp = tmp;\
    }

#define rpc_$unmarshall_short_float(mp, dst)\
    dst = *(ndr_$short_float *)mp

#define rpc_$convert_short_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_$unmarshall_short_float(mp, dst);\
    else {\
        rpc_$cvt_short_float (src_drep, dst_drep,\
            (rpc_$short_float_p_t)mp,\
            (rpc_$short_float_p_t)&dst);\
        }



#define rpc_$marshall_long_float(mp, src)\
    *(ndr_$long_float *)mp = src

#define rpc_$unmarshall_long_float(mp, dst)\
    dst = *(ndr_$long_float *)mp

#define rpc_$convert_long_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_$unmarshall_long_float(mp, dst);\
    else\
        rpc_$cvt_long_float (src_drep, dst_drep,\
            (rpc_$long_float_p_t)mp,\
            (rpc_$long_float_p_t)&dst)



#define rpc_$marshall_string(lenvar, maxlen, mp, src)\
    {\
    if ((lenvar = strlen((ndr_$char *)src)) >= maxlen)\
        SIGNAL(nca_status_$string_too_long)\
    rpc_$marshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)mp, (ndr_$char *)src);\
    }

#define rpc_$unmarshall_string(lenvar, mp, dst)\
    {\
    rpc_$unmarshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    }

#define rpc_$convert_string(src_drep, dst_drep, lenvar, mp, dst)\
    {\
    rpc_$convert_ushort_int (src_drep, dst_drep, mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    if (src_drep.char_rep == dst_drep.char_rep)\
        strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    else\
        rpc_$cvt_string (src_drep, dst_drep, (rpc_$char_p_t)mp, (rpc_$char_p_t)dst);\
    }

#endif

/***************************************************************************/

/****
 **** Definitions specific to Crays.
 ****
 **** Cray specific issues:
 ****     cray data types don't exactly match ndr data types
 ****         cray's don't have 8 bit signed integers, nor 16 bit
 ****             signed/unsigned ints, nor 32 bit signed/unsigned
 ****             ints (in the case of the XMP)
 ****         cray's floats are all 64 bits and ndr knows this
 ****     cray XMP and cray 2 data types have different precisions
 ****     with the exception of character pointers, the address of
 ****         *any* data type results in a 64 bit aligned address
 ****         (which means that the pointer doesn't typically point
 ****         to the first byte of the data element).
 ****/

#if defined(cray) || defined(CRAY) || defined(CRAY1) || defined(CRAY2)

#undef USE_DEFAULT_MACROS
#undef ALIGNED_SCALAR_ARRAYS

/*
 * first some macros for some repetitive sequences
 */
#define rpc_$put_2(mp, src)\
    {\
    unsigned long buf = src; /* necessary since src isn't always an lvalue */\
    mp[0] = ((rpc_$mp_t)&buf)[6];\
    mp[1] = ((rpc_$mp_t)&buf)[7];\
    }

#define rpc_$get_2(mp, dst)\
    {\
    ((rpc_$mp_t)&dst)[6] = mp[0];\
    ((rpc_$mp_t)&dst)[7] = mp[1];\
    }

#define rpc_$get_2_swab(mp, dst)\
    {\
    ((rpc_$mp_t)&dst)[6] = mp[1];\
    ((rpc_$mp_t)&dst)[7] = mp[0];\
    }

#define rpc_$put_4(mp, src)\
    {\
    unsigned long buf = src; /* necessary since src isn't always an lvalue */\
    mp[0] = ((rpc_$mp_t)&buf)[4];\
    mp[1] = ((rpc_$mp_t)&buf)[5];\
    mp[2] = ((rpc_$mp_t)&buf)[6];\
    mp[3] = ((rpc_$mp_t)&buf)[7];\
    }

#define rpc_$get_4(mp, dst)\
    {\
    ((rpc_$mp_t)&dst)[4] = mp[0];\
    ((rpc_$mp_t)&dst)[5] = mp[1];\
    ((rpc_$mp_t)&dst)[6] = mp[2];\
    ((rpc_$mp_t)&dst)[7] = mp[3];\
    }

#define rpc_$get_4_swab(mp, dst)\
    {\
    ((rpc_$mp_t)&dst)[4] = mp[3];\
    ((rpc_$mp_t)&dst)[5] = mp[2];\
    ((rpc_$mp_t)&dst)[6] = mp[1];\
    ((rpc_$mp_t)&dst)[7] = mp[0];\
    }

#define rpc_$get_8_swab(mp, dst)\
    {\
    ((rpc_$mp_t)&dst)[0] = mp[7];\
    ((rpc_$mp_t)&dst)[1] = mp[6];\
    ((rpc_$mp_t)&dst)[2] = mp[5];\
    ((rpc_$mp_t)&dst)[3] = mp[4];\
    ((rpc_$mp_t)&dst)[4] = mp[3];\
    ((rpc_$mp_t)&dst)[5] = mp[2];\
    ((rpc_$mp_t)&dst)[6] = mp[1];\
    ((rpc_$mp_t)&dst)[7] = mp[0];\
    }

/*
 * now for the macros nidl uses
 */

#define rpc_$marshall_boolean(mp, src)\
    *(ndr_$boolean *)mp = src

#define rpc_$unmarshall_boolean(mp, dst)\
    dst = *(ndr_$boolean *)mp

#define rpc_$convert_boolean(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_boolean(mp, dst)



#define rpc_$marshall_byte(mp, src)\
    *(ndr_$byte *)mp = src

#define rpc_$unmarshall_byte(mp, dst)\
    dst = *(ndr_$byte *)mp

#define rpc_$convert_byte(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_byte(mp, dst)



#define rpc_$marshall_char(mp, src)\
    *(ndr_$char *)mp = src

#define rpc_$unmarshall_char(mp, dst)\
    dst = *(ndr_$char *)mp

#define rpc_$convert_char(src_drep, dst_drep, mp, dst)\
    if (src_drep.char_rep == dst_drep.char_rep){\
        rpc_$unmarshall_char(mp, dst);\
    } else if (dst_drep.char_rep == rpc_$drep_char_ascii)\
        (ndr_$char)dst = rpc_$ebcdic_to_ascii [*(ndr_$char *)mp];\
    else\
        (ndr_$char)dst = rpc_$ascii_to_ebcdic [*(ndr_$char *)mp]



/*
 * ndr_$small_int is larger than 8 bits
 */
#define rpc_$marshall_small_int(mp, src)\
    *(char *)mp = (char)src

#define rpc_$unmarshall_small_int(mp, dst)\
    {\
    dst = (*mp & 0x80) ? -1 : 0;\
    ((char *)&dst)[7] = *mp;\
    }

#define rpc_$convert_small_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_small_int(mp, dst)



/*
 * ndr_$usmall_int is larger than 8 bits
 */
#define rpc_$marshall_usmall_int(mp, src)\
    *(char *)mp = (char)src

#define rpc_$unmarshall_usmall_int(mp, dst)\
    {\
    dst = 0;\
    ((char *)&dst)[7] = *mp;\
    }

#define rpc_$convert_usmall_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_usmall_int(mp, dst)




/*
 * ndr_$short_int is larger than 16 bits
 */
#define rpc_$marshall_short_int(mp, src)\
    rpc_$put_2(mp, src)

#define rpc_$unmarshall_short_int(mp, dst)\
    {\
    dst = (*mp & 0x80) ? -1 : 0;\
    rpc_$get_2(mp, dst);\
    }

#define rpc_$convert_short_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_short_int(mp, dst);\
    } else {\
        dst = (mp[1] & 0x80) ? -1 : 0;\
        rpc_$get_2_swab(mp, dst);\
    }



/*
 * ndr_$ushort_int is larger than 16 bits
 */
#define rpc_$marshall_ushort_int(mp, src)\
    rpc_$put_2(mp, src)

#define rpc_$unmarshall_ushort_int(mp, dst)\
    {\
    dst = 0;\
    rpc_$get_2(mp, dst);\
    }

#define rpc_$convert_ushort_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_ushort_int(mp, dst);\
    } else {\
        dst = 0;\
        rpc_$get_2_swab(mp, dst);\
    }



/*
 * ndr_$long_int is larger than 32 bits on a XMP
 * (could special case for the cray 2 and get rid of the sign
 * extension processing)
 */
#define rpc_$marshall_long_int(mp, src)\
    rpc_$put_4(mp, src)

#define rpc_$unmarshall_long_int(mp, dst)\
    {\
    dst = (*mp & 0x80) ? -1 : 0;\
    rpc_$get_4(mp, dst);\
    }

#define rpc_$convert_long_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_long_int(mp, dst);\
    } else {\
        dst = (mp[3] & 0x80) ? -1 : 0;\
        rpc_$get_4_swab(mp, dst);\
    }



/*
 * ndr_$ulong_int is larger than 32 bits on a XMP
 * (could special case for the cray 2 and get rid of the 0 fill
 * extension procession)
 */
#define rpc_$marshall_ulong_int(mp, src)\
    rpc_$put_4(mp, src)

#define rpc_$unmarshall_ulong_int(mp, dst)\
    {\
    dst = 0;\
    rpc_$get_4(mp, dst);\
    }

#define rpc_$convert_ulong_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_ulong_int(mp, dst);\
    } else {\
        dst = 0;\
        rpc_$get_4_swab(mp, dst);\
    }




#define rpc_$marshall_hyper_int(mp, src)\
    *(ndr_$hyper_int *)mp = src

#define rpc_$unmarshall_hyper_int(mp, dst)\
    dst = *(ndr_$hyper_int *)mp

#define rpc_$convert_hyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_hyper_int(mp, dst);\
    } else {\
        rpc_$get_8_swab(mp, dst);\
    }



#define rpc_$marshall_uhyper_int(mp, src)\
    *(ndr_$uhyper_int *)mp = src

#define rpc_$unmarshall_uhyper_int(mp, dst)\
    dst = *(ndr_$uhyper_int *)mp

#define rpc_$convert_uhyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep){\
        rpc_$unmarshall_uhyper_int(mp, dst);\
    } else {\
        rpc_$get_8_swab(mp, dst);\
    }



/*
 * since cray's don't support a 32 bit flt pt format,
 * convert to IEEE 32 bit format and stick that in the packet.
 */
#define rpc_$marshall_short_float(mp, src)\
{\
    unsigned long ieee_32;\
    extern void rpc_$cray64_to_ieee32();\
\
    rpc_$cray64_to_ieee32(&src, &ieee_32);\
    rpc_$put_4(mp, ieee_32);\
}

/*
 * The packet has a 32 bit IEEE value (what the above macro decided
 * to stick in the packet). A conversion to 64 bit cray float is
 * required.
 */
#define rpc_$unmarshall_short_float(mp, dst)\
{\
    unsigned long ieee_32;\
    extern void rpc_$ieee32_to_cray64();\
\
    rpc_$get_4(mp, ieee_32);\
    rpc_$ieee32_to_cray64(&ieee_32, &dst);\
}

#define rpc_$convert_short_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep)){\
        rpc_$unmarshall_short_float(mp, dst);\
    } else {\
        unsigned long buf;\
        rpc_$get_4(mp, buf);\
        rpc_$cvt_short_float (\
            src_drep,\
            dst_drep,\
            (rpc_$short_float_p_t)&buf,\
            (rpc_$short_float_p_t)&dst); \
    }




#define rpc_$marshall_long_float(mp, src)\
    *(ndr_$long_float *)mp = src

#define rpc_$unmarshall_long_float(mp, dst)\
    dst = *(ndr_$long_float *)mp

#define rpc_$convert_long_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep)){\
        rpc_$unmarshall_long_float(mp, dst);\
    } else {\
        rpc_$cvt_long_float (\
            src_drep,\
            dst_drep,\
            (rpc_$long_float_p_t)mp,\
            (rpc_$long_float_p_t)&dst);\
    }



#define rpc_$marshall_string(lenvar, maxlen, mp, src)\
    {\
    if ((lenvar = strlen((ndr_$char *)src)) >= maxlen)\
        SIGNAL(nca_status_$string_too_long)\
    rpc_$marshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)mp, (ndr_$char *)src);\
    }

#define rpc_$unmarshall_string(lenvar, mp, dst)\
    {\
    rpc_$unmarshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    }

#define rpc_$convert_string(src_drep, dst_drep, lenvar, mp, dst)\
    {\
    rpc_$convert_ushort_int (src_drep, dst_drep, mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    if (src_drep.char_rep == dst_drep.char_rep)\
        strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    else\
        rpc_$cvt_string (src_drep, dst_drep, (rpc_$char_p_t)mp, (rpc_$char_p_t)dst);\
    }

#endif

/***************************************************************************/ 

/****
 **** Definition of the default marshalling pointer representation
 ****/

#ifdef USE_DEFAULT_MP_REP

typedef char *rpc_$mp_t;

#define rpc_$init_mp(mp, dbp, bufp, offset)\
    mp = dbp = (rpc_$mp_t)bufp + offset
         
#define rpc_$advance_mp(mp, delta)\
    mp += delta

#define rpc_$align_ptr_relative(p, base, alignment)\
    p = base + (((p-base) + (alignment-1)) & ~(alignment-1))

#endif

/***************************************************************************/ 

/****
 **** Definitions of the default marshall, unmarshall, and convert macros.
 ****/

#ifdef USE_DEFAULT_MACROS

#define rpc_$marshall_boolean(mp, src)\
    *(ndr_$boolean *)mp = src

#define rpc_$unmarshall_boolean(mp, dst)\
    dst = *(ndr_$boolean *)mp

#define rpc_$convert_boolean(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_boolean(mp, dst)



#define rpc_$marshall_byte(mp, src)\
    *(ndr_$byte *)mp = src

#define rpc_$unmarshall_byte(mp, dst)\
    dst = *(ndr_$byte *)mp

#define rpc_$convert_byte(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_byte(mp, dst)



#define rpc_$marshall_char(mp, src)\
    *(ndr_$char *)mp = src

#define rpc_$unmarshall_char(mp, dst)\
    dst = *(ndr_$char *)mp

#define rpc_$convert_char(src_drep, dst_drep, mp, dst)\
    if (src_drep.char_rep == dst_drep.char_rep)\
        rpc_$unmarshall_char(mp, dst);\
    else if (dst_drep.char_rep == rpc_$drep_char_ascii)\
        (ndr_$char)dst = rpc_$ebcdic_to_ascii [*(ndr_$char *)mp];\
    else\
        (ndr_$char)dst = rpc_$ascii_to_ebcdic [*(ndr_$char *)mp]



#define rpc_$marshall_small_int(mp, src)\
    *(ndr_$small_int *)mp = src

#define rpc_$unmarshall_small_int(mp, dst)\
    dst = *(ndr_$small_int *)mp

#define rpc_$convert_small_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_small_int(mp, dst)



#define rpc_$marshall_usmall_int(mp, src)\
    *(ndr_$usmall_int *)mp = src

#define rpc_$unmarshall_usmall_int(mp, dst)\
    dst = *(ndr_$usmall_int *)mp

#define rpc_$convert_usmall_int(src_drep, dst_drep, mp, dst)\
    rpc_$unmarshall_usmall_int(mp, dst)



#define rpc_$marshall_short_int(mp, src)\
    *(ndr_$short_int *)mp = src

#define rpc_$unmarshall_short_int(mp, dst)\
    dst = *(ndr_$short_int *)mp

#define rpc_$convert_short_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_short_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_$marshall_ushort_int(mp, src)\
    *(ndr_$ushort_int *)mp = (ndr_$ushort_int)src

#define rpc_$unmarshall_ushort_int(mp, dst)\
    (ndr_$ushort_int)dst = *(ndr_$ushort_int *)mp

#define rpc_$convert_ushort_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_ushort_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[1]; _d[1]=_s[0];\
        }



#define rpc_$marshall_long_int(mp, src)\
    *(ndr_$long_int *)mp = src

#define rpc_$unmarshall_long_int(mp, dst)\
    dst = *(ndr_$long_int *)mp

#define rpc_$convert_long_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_long_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_$marshall_ulong_int(mp, src)\
    *(ndr_$ulong_int *)mp = (ndr_$ulong_int)src

#define rpc_$unmarshall_ulong_int(mp, dst)\
    (ndr_$ulong_int)dst = *(ndr_$ulong_int *)mp

#define rpc_$convert_ulong_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_ulong_int(mp, dst);\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[3]; _d[1]=_s[2]; _d[2]=_s[1]; _d[3]=_s[0];\
        }



#define rpc_$marshall_hyper_int(mp, src) {\
    *(struct ndr_$hyper_int_rep *)mp = *(struct ndr_$hyper_int_rep *)&src;\
    }

#define rpc_$unmarshall_hyper_int(mp, dst) {\
    *(struct ndr_$hyper_int_rep *)&dst = *(struct ndr_$hyper_int_rep *)mp;\
    }

#define rpc_$convert_hyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_hyper_int(mp, dst)\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_$marshall_uhyper_int(mp, src) {\
    *(struct ndr_$uhyper_int_rep *)mp = *(struct ndr_$uhyper_int_rep *)&src;\
    }

#define rpc_$unmarshall_uhyper_int(mp, dst) {\
    *(struct ndr_$uhyper_int_rep *)&dst = *(struct ndr_$uhyper_int_rep *)mp;\
    }

#define rpc_$convert_uhyper_int(src_drep, dst_drep, mp, dst)\
    if (src_drep.int_rep == dst_drep.int_rep)\
        rpc_$unmarshall_uhyper_int(mp, dst)\
    else {\
        ndr_$byte *_d = (ndr_$byte *) &dst;\
        ndr_$byte *_s = (ndr_$byte *) mp;\
        _d[0]=_s[7]; _d[1]=_s[6]; _d[2]=_s[5]; _d[3]=_s[4];\
        _d[4]=_s[3]; _d[5]=_s[2]; _d[6]=_s[1]; _d[7]=_s[0];\
        }



#define rpc_$marshall_short_float(mp, src) {\
    ndr_$short_float tmp;\
    tmp = src;\
    *(ndr_$short_float *)mp = tmp;\
    }

#define rpc_$unmarshall_short_float(mp, dst)\
    dst = *(ndr_$short_float *)mp

#define rpc_$convert_short_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_$unmarshall_short_float(mp, dst);\
    else {\
        rpc_$cvt_short_float (src_drep, dst_drep,\
            (rpc_$short_float_p_t)mp,\
            (rpc_$short_float_p_t)&dst);\
        }



#define rpc_$marshall_long_float(mp, src)\
    *(ndr_$long_float *)mp = src

#define rpc_$unmarshall_long_float(mp, dst)\
    dst = *(ndr_$long_float *)mp

#define rpc_$convert_long_float(src_drep, dst_drep, mp, dst)\
    if ((src_drep.float_rep == dst_drep.float_rep) &&\
        (src_drep.int_rep   == dst_drep.int_rep))\
        rpc_$unmarshall_long_float(mp, dst);\
    else\
        rpc_$cvt_long_float (src_drep, dst_drep,\
            (rpc_$long_float_p_t)mp,\
            (rpc_$long_float_p_t)&dst)



#define rpc_$marshall_string(lenvar, maxlen, mp, src)\
    {\
    if ((lenvar = strlen((ndr_$char *)src)) >= maxlen)\
        SIGNAL(nca_status_$string_too_long)\
    rpc_$marshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)mp, (ndr_$char *)src);\
    }

#define rpc_$unmarshall_string(lenvar, mp, dst)\
    {\
    rpc_$unmarshall_ushort_int (mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    }

#define rpc_$convert_string(src_drep, dst_drep, lenvar, mp, dst)\
    {\
    rpc_$convert_ushort_int (src_drep, dst_drep, mp, lenvar);\
    rpc_$advance_mp(mp, 2);\
    if (src_drep.char_rep == dst_drep.char_rep)\
        strcpy ((ndr_$char *)dst, (ndr_$char *)mp);\
    else\
        rpc_$cvt_string (src_drep, dst_drep, (rpc_$char_p_t)mp, (rpc_$char_p_t)dst);\
    }

#endif    
  
/***************************************************************************/ 
/***************************************************************************/ 

#endif

#endif
