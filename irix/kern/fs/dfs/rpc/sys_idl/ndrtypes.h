/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* 
 * HISTORY 
 * $Log: ndrtypes.h,v $
 * Revision 65.1  1997/10/24 14:30:01  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:31  jdoak
 * Initial revision
 *
 * Revision 64.2  1997/04/17  20:58:51  lmc
 * #477625: Redefined ndr_long_int to be "long int" for 32-bit.
 * Redefined ndr_ulong_int to be "unsigned long int" for 32-bit.
 * 64-bit definitions of the above types were retained as "int" and
 * "unsigned int" respectively.
 *
 * Revision 64.1  1997/02/14  19:45:21  dce
 * *** empty log message ***
 *
 * Revision 1.3  1995/10/24  01:57:48  dcebuild
 * 64 bit change
 *
 * Revision 1.2  1995/04/24  19:46:23  dcebuild
 * File added by SGI
 *
 * Revision 1.1.129.3  1994/06/10  20:54:54  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:20  devsrc]
 *
 * Revision 1.1.129.2  1994/04/06  22:05:36  tom
 * 	Bug 9965 - ndr_true should be defined to lowercase true.
 * 	[1994/04/06  21:57:22  tom]
 * 
 * Revision 1.1.129.1  1994/01/21  22:39:48  cbrooks
 * 	platform dependent NDR data types
 * 	[1994/01/21  17:14:56  cbrooks]
 * 
 * $EndLog$
 */
/*
**  NAME:
**
**      ndrtypes.h
**
**  FACILITY:
**
**      IDL Stub Support Include File
**
**  ABSTRACT:
**
**  This file is new for DCE 1.1. This is a platform specific file that
**  defines the base level ndr types. This file is indirectly included 
**  in all files via the idlbase.h file. 
**
*/

/*
 * This particular file defines the NDR types for a big-endian
 * architecture. This file also depends on the presence of a ANSI 
 * C compiler, in that it uses the signed keyword to create the 
 * ndr_small_int type.
 */


#ifndef _NDR_TYPES_H
#define _NDR_TYPES_H

#include <limits.h>

typedef unsigned char     ndr_boolean ;
#define ndr_false false
#define ndr_true  true 

typedef unsigned char     ndr_byte ;

typedef unsigned char     ndr_char ;

typedef signed   char     ndr_small_int;

typedef unsigned char     ndr_usmall_int;

typedef short    int      ndr_short_int ;

typedef unsigned short int ndr_ushort_int; 

#if (LONG_BIT >= 64)
typedef int ndr_long_int;

typedef unsigned int ndr_ulong_int;

typedef long ndr_hyper_int;
typedef unsigned long ndr_uhyper_int;

#else
typedef long int ndr_long_int;

typedef unsigned long int ndr_ulong_int;

struct ndr_hyper_int_rep_s_t   {
    ndr_long_int high; 
    ndr_ulong_int low;
};
typedef struct ndr_hyper_int_rep_s_t ndr_hyper_int;

struct ndr_uhyper_int_rep_s_t  {
    ndr_ulong_int high; 
    ndr_ulong_int low;
};
typedef struct ndr_uhyper_int_rep_s_t ndr_uhyper_int;

#endif
typedef float		ndr_short_float; 
typedef double 		ndr_long_float;

#endif /* _NDR_TYPES_H */
