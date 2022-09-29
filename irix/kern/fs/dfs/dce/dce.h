/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dce.h,v $
 * Revision 65.4  1997/12/31 19:08:10  gwehrman
 * Now include standards.h before sys/endian.h so that the proper definitions
 * are picked up.
 *
 * Revision 65.3  1997/12/18  14:16:56  gwehrman
 * Removing the definitions for byte order is not good as several files
 * need these definitions.  Rather than redefine the byte order definitions,
 * pick them up by including the system include file where they are defined,
 * sys/endian.h.
 *
 * Revision 65.2  1997/12/16  17:10:09  lmc
 * Removed definition for byte order from dce.h for SGIMIPS.  We are already
 * picking up these definitions from a system include file.
 *
 * Revision 65.1  1997/10/21  20:05:16  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:30  jdoak
 * Initial revision
 *
 * Revision 64.2  1997/03/28  15:34:13  lord
 * Allow control over debug options
 *
 * Revision 64.1  1997/02/14  19:44:27  dce
 * *** empty log message ***
 *
 * Revision 1.4  1997/02/11  19:10:41  bhim
 * #463894: Customized comments for IRIX.
 *
 * Revision 1.3  1995/10/24  01:55:10  dcebuild
 * Changed 64bit and 128bit defines for SGIMIPS
 *
 * Revision 1.2  1995/05/06  01:56:06  dcebuild
 * File added by SGI
 *
 * Revision 1.1.4.6  1994/09/23  19:59:55  tom
 * 	Back out change of unsigned/signed64 to typedef hyper
 * 	[1994/09/23  19:26:42  tom]
 *
 * Revision 1.1.4.4  1994/08/16  20:53:13  cbrooks
 * 	11494 add 128 and 48 bit data types
 * 	[1994/08/16  20:52:59  cbrooks]
 * 
 * Revision 1.1.4.3  1994/03/23  22:17:46  tom
 * 	Bug 10079 - include param.h to get MIN and MAX defined so
 * 	later inclusion of param.h wont give redef warnings.
 * 	[1994/03/23  22:17:18  tom]
 * 
 * Revision 1.1.4.2  1994/01/19  17:42:38  cbrooks
 * 	Code Cleanup
 * 	[1994/01/18  22:33:47  cbrooks]
 * 
 * Revision 1.1.4.1  1994/01/14  14:59:30  rousseau
 * 	Changed to use new DCE RFC 34.2 ENDIAN convention.
 * 	[1994/01/14  14:58:59  rousseau]
 * 
 * Revision 1.1.2.1  1993/12/09  23:40:33  melman
 * 	Made dce.h machine specific
 * 	[1993/12/09  23:40:16  melman]
 * 
 * $EndLog$
 */
#if !defined(_DCE_H)
#define _DCE_H


/*
 * Common definitions for DCE
 */ 

#include <standards.h>	/* must be included before sys/endian.h PV 511301 */
#include <sys/endian.h>


/* Only one place needed for DCE to define these */
#define FALSE 0
#define TRUE 1

/* 
 * In IRIX, MIN and MAX are defined in sys/param.h.
 * Keep the definitions below.
 */

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H
#include <sys/param.h>
#undef _SYS_SIGNAL_H
#else
#include <sys/param.h>
#endif
#include <limits.h>

#if !defined(MIN)
#  define MIN(x, y)         ((x) < (y) ? (x) : (y))
#endif

#if !defined(MAX)
#  define MAX(x, y)         ((x) > (y) ? (x) : (y))
#endif

/* 
 * The following allows for the support of both old and new style 
 * function definitions and prototypes.  All DCE code is required to 
 * be ANSI C compliant and to use prototypes.  For those components 
 * that wish to support old-style definitions, the following macros 
 * must be used.
 *
 *  Declare a prototype like this (don't use variables):
 *      int foo _DCE_PROTOTYPE_((int, void *, struct bar *))
 *  
 *  Define a function like this:
 *      int foo 
 *      #if defined(_DCE_PROTO_)
 *              (
 *              int a, 
 *              void *b,
 *              struct bar *c
 *              )
 *      #else
 *              (a, b, c)
 *              int a;
 *              void *b;
 *              struct bar *c;
 *      #endif
 */
#if defined(__STDC__)                   /* other conditionals can be tested */
#  define _DCE_PROTO_
#endif                                  /* defined(__STDC__) */

#if defined(_DCE_PROTO_)
#  define _DCE_PROTOTYPE_(arg) arg 
#else                                   /* defined(_DCE_PROTO_) */
#  define _DCE_PROTOTYPE_(arg) ()
#endif                                  /* defined(_DCE_PROTO_) */

/* 
 * For those components wishing to support platforms where void 
 * pointers are not available, they can use the following typedef for 
 * a generic pointer type.  If they are supporting such platforms they 
 * must use this.
 */
#if defined(__STDC__)
#  define _DCE_VOID_
#endif                                  /* defined(__STDC__) */

#if defined(_DCE_VOID_)
  typedef void * pointer_t;
#else                                   /* defined(_DCE_VOID_) */
  typedef char * pointer_t;
#endif                                  /* defined(_DCE_VOID_) */

/* 
 * Here is a macro that can be used to support token concatenation in 
 * an ANSI and non-ANSI environment.  Support of non-ANSI environments 
 * is not required, but where done, this macro must be used.
 */
#if defined(__STDC__)
#  define _DCE_TOKENCONCAT_
#endif

#if defined(_DCE_TOKENCONCAT_)
#  define DCE_CONCAT(a, b)      a ## b
#else                                   /* defined(_DCE_TOKENCONCAT_) */
#  define DCE_CONCAT(a, b)      a/**/b
#endif                                  /* defined(_DCE_TOKENCONCAT_) */

/*
 * Define the dcelocal and dceshared directories
 */
extern const char *dcelocal_path;
extern const char *dceshared_path;

/* If DCE_DEBUG is defined then debugging code is activated. */
#if (defined(PRERELEASE) && !defined(DCE_DEBUG)) || !defined(_KERNEL)
#define DCE_DEBUG 
#endif

/* 
 * Machine dependent typedefs for boolean, byte, and (un)signed integers.
 * All DCE code should be using these typedefs where applicable.
 * The following are defined in nbase.h:
 *     unsigned8       unsigned  8 bit integer
 *     unsigned16      unsigned 16 bit integer
 *     unsigned32      unsigned 32 bit integer
 *     signed8           signed  8 bit integer       
 *     signed16          signed 16 bit integer
 *     signed32          signed 32 bit integer
 * Define the following from idl types in idlbase.h (which is included 
 * by nbase.h:
 *     byte            unsigned  8 bits
 *     boolean         unsigned  8 bits   
 * Define (un)signed64 to be used with the U64* macros
 */

#include <dce/nbase.h>

typedef idl_byte        byte;
typedef idl_boolean     boolean;

#if (LONG_BIT >= 64)
typedef unsigned long unsigned64;
typedef long signed64;

typedef struct unsigned128_s_t {
    unsigned long lo;
    unsigned long hi;
} unsigned128;

#else
typedef struct unsigned64_s_t {
    unsigned long hi;
    unsigned long lo;
} unsigned64;

typedef struct signed64_s_t {
    unsigned long hi;
    unsigned long lo;
} signed64;

typedef struct unsigned128_s_t {
    unsigned long lolo;
    unsigned long lohi;
    unsigned long hilo;
    unsigned long hihi;
} unsigned128;

#endif

typedef struct unsigned48_s_t {
    unsigned long  int  lo;             /* least significant 32 bits */
	unsigned short int  hi;             /* most significant 16 bits */
} unsigned48;

/* 
 * Serviceability and perhaps other DCE-wide include files 
 * will be included here.  This is a sample only.
 */
#include <dce/dce_svc.h>

#endif                                  /* _DCE_H */
