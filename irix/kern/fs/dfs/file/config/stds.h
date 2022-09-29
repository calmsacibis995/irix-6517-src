/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: stds.h,v $
 * Revision 65.7  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.6  1998/03/19 23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.5  1998/03/02  22:30:29  bdr
 * 64bit macro changes.
 *
 * Revision 65.4  1998/02/27  00:05:12  lmc
 * Changes to compile using 64bit macros.  Copied from bdr, and needed
 * for compiles.
 *
 * Revision 65.1  1997/10/20  19:21:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.21.1  1996/10/02  17:14:46  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:06:35  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1990 Transarc Corporation - All rights reserved. */

/*
 * stds.h -- Definitions used to comply with the general Transarc coding
 *     standards.
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/config/RCS/stds.h,v 65.7 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef TRANSARC_CONFIG_STDS_H
#define TRANSARC_CONFIG_STDS_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

/* This header file is worse than useless without the correct platform specific
 * definitions, so make sure we have param.h included. */

#include <dcedfs/param.h>
#ifdef SGIMIPS 
#include <sgidefs.h>
#endif /* SGIMIPS */

/* The following defines are provided for compatibility only.  The should not
 * be used in new code. */

/*
 * The following two groups (except for IMPORT) were taken from
 * /afs/transarc.com/kansas/src/stds/transarc_stds.h.
 *
 *	IN		Indicates an input parameter
 *	OUT		Indicates an output parameter
 *	INOUT		Indicates a parameter for both input & output
 *
 *	EXPORT		Available to everyone.
 *	HIDDEN		Used in a PUBLIC macro, but not exported
 *	SHARED		Shared between module source files
 *	PRIVATE		Private to a source (.c) file
 *	IMPORT		Defined in another file
 */

#define IN
#define OUT
#define INOUT

#define EXPORT
#define HIDDEN
#define SHARED
#define PRIVATE	static
#define IMPORT	extern


#if !defined(AFS_OSF_ENV) || (defined(AFS_OSF_ENV) && !defined(_KERNEL))
#define MACRO_BEGIN	do {
#define MACRO_END	} while (/*CONSTCOND*/0)
#endif /* AFS_OSF_ENV */

/*
 * The following are provided for ANSI C compatibility*/

/*
 * AIX 'cc' does not define __STDC__ but does handle void* properly.  We
 * recognize it by checking for _IBMR2.
 */
#if defined(__STDC__) || defined(__cplusplus) || defined(_IBMR2)
typedef void *opaque;
#else /* __STDC__ */
#define const
typedef char *opaque;
#endif /* __STDC__ */

/* Use _TAKES with double parentheses around an ANSI C param list */
/* MIPS compilers understand ANSI prototypes (and check 'em too!) */

#ifndef _TAKES
#if defined(__STDC__) || defined(__cplusplus) || defined(mips) || defined(_IBMR2)
#define _TAKES(x) x
#else /* __STDC__ */
#define _TAKES(x) ()
#endif /* __STDC__ */
#endif /* _TAKES */


/* These are the new afsl trace and debug macros */

struct afsl_tracePackage {		/* cotrol for tracing a package */
    long enabledTypes;			/* mask of enabled types */
    long enabledFiles;			/* mask of enabled files */
    char *name;				/* package's name */
    struct afsl_tracePackage *next;	/* list of all packages */
};

#define AFSL_TR_VERSION 1
#define AFSL_TR_BUFFER_LENGTH 100
struct afsl_trace {
    unsigned char version;		/* version # of structure */
    unsigned char bufLen;		/* buffer length */
    unsigned char file;			/* number of this file */
    char flags;				/* flags */
    struct afsl_tracePackage *control;	/* per package control block */
    char *fileName;			/* filename from fileid */
    char *revision;			/* revision of file */
    int fileMask;			/* mask of file in package */
    char *fileid;			/* actual file id string */
    unsigned fileIdType;		/* file id' type, from defines below */
    char buffer[AFSL_TR_BUFFER_LENGTH]; /* for excerpts of fileId string */
};

/*
 * FILEID -- makes including the file id in object files less painful. Put this
 *     near the beginning of .c files (not .h files).  Do NOT follow it with a
 *     semi-colon.  The argument should be a double quoted string containing
 *     the file id.
 *
 * PERFORMANCE -- On many platorms whose compilers aren't too clever, much of
 *     the following cleverness may not be necessary.
 */

#ifdef AFSL_USE_RCS_ID
#define AFSL_ID_TYPE	1		/* Use RCS ids */
#else
#define AFSL_ID_TYPE	0		/* Unknown id type */
#endif

/* Believe it or not the following seems to supress all warnings from all
 * compilers I've checked.  I've tried the HC compile with normal, -g and -o
 * switched.  Also gcc on the pmax with "-O -Wall" and so far so good. -ota
 * 900426 */

#ifdef SGIMIPS
/*  All fields below are changed to unsigned long for 64-bit issues. */
#ifdef __DATE__
#define FILEID(x) \
  static struct afsl_trace afsl_trace = { \
      AFSL_TR_VERSION, AFSL_TR_BUFFER_LENGTH, /*file#*/0, /*flags*/0, \
      /*control*/0, /*fileName*/__FILE__, /*revision*/__DATE__, /*filemask*/0,\
      /*fileid*/x, /*fileIdType*/AFSL_ID_TYPE }; \
  static unsigned long (*__fileid)(); \
  static unsigned long  _fileid () \
  { static unsigned long a; \
    a = (unsigned long)&afsl_trace; __fileid = _fileid; a = (unsigned long)__fileid; return a; }
#else /* __DATE__ */
#define FILEID(x) \
  static struct afsl_trace afsl_trace = { \
      AFSL_TR_VERSION, AFSL_TR_BUFFER_LENGTH, /*file#*/0, /*flags*/0, \
      /*control*/0, /*fileName*/__FILE__, /*revision*/0, /*filemask*/0,\
      /*fileid*/x, /*fileIdType*/AFSL_ID_TYPE }; \
  static unsigned long (*__fileid)(); \
  static unsigned long _fileid () \
  { static unsigned long a; \
    a = (unsigned long)&afsl_trace; __fileid = _fileid; a = (unsigned long)__fileid; return a; }
#endif /* __DATE__ */
#else

#ifdef __DATE__
#define FILEID(x) \
  static struct afsl_trace afsl_trace = { \
      AFSL_TR_VERSION, AFSL_TR_BUFFER_LENGTH, /*file#*/0, /*flags*/0, \
      /*control*/0, /*fileName*/__FILE__, /*revision*/__DATE__, /*filemask*/0,\
      /*fileid*/x, /*fileIdType*/AFSL_ID_TYPE }; \
  static int (*__fileid)(); \
  static int _fileid () \
  { static int a; \
    a = (int)&afsl_trace; __fileid = _fileid; a = (int)__fileid; return a; }
#else /* __DATE__ */
#define FILEID(x) \
  static struct afsl_trace afsl_trace = { \
      AFSL_TR_VERSION, AFSL_TR_BUFFER_LENGTH, /*file#*/0, /*flags*/0, \
      /*control*/0, /*fileName*/__FILE__, /*revision*/0, /*filemask*/0,\
      /*fileid*/x, /*fileIdType*/AFSL_ID_TYPE }; \
  static int (*__fileid)(); \
  static int _fileid () \
  { static int a; \
    a = (int)&afsl_trace; __fileid = _fileid; a = (int)__fileid; return a; }
#endif /* __DATE__ */
#endif
/* afsl_Fileid -- returns the local file's fileid string. */
#define afsl_Fileid() (afsl_trace.fileid)

/* Define RCSID temporarily until we convert everything to FILEID */
#define RCSID(x)	FILEID(x)

/*
 * Some types to enhance portability.  These are to be used on the wire
 * or when laying out shared structures on disk.
 */

/* Imagine that this worked...
 * #if (sizeof(long) != 4) || (sizeof(short) != 2)
 * #error We require size of long and pointers to be equal
 * #endif
 */

typedef short		int16;
typedef unsigned short	u_int16;

/*
 * The Sun RPC include files define u_int32 with a typedef and this caused
 * problems for the NFS Translator.  Starting with Solaris 2.4 they also define
 * int32 with a typedef.  Users of those include files should just
 * #undef these.
 */

#ifdef SGIMIPS
#define int32 int
#define u_int32 unsigned int
#else
#define int32 long
/* typedef long			int32;	 */
#define u_int32 unsigned long
/* typedef unsigned long	u_int32; */
#endif

/* Make the 64bit hyper type a 64bit scalar when possible.  Provide both types
 * of macros here. */

/* The afs_hyper_t is an unsigned 64-bit integer type. */

/* The following macros provide uniform access methods whether a scalar type is
 * available nor not:
 *
 * int AFS_hcmp (afs_hyper_t a, afs_hyper_t b) -- returns (-1,0,1) if a is
 *     (less, equal, or greater) b.  This is an unsigned comparison.  In
 *     otherwords, (a <oper> b) can be expressed as (hcmp(a, b) <oper> 0) where
 *     <operator> is one of { "<", "<=", "==", ">", ">=" }.
 * int AFS_hcmp64 (afs_hyper_t a, u_int32 hi, u_int32 lo) -- like hcmp but
 *     compares a with (hi<<32 + lo).
 * int AFS_hsame(afs_hyper_t a, afs_hyper_t b) -- returns a non-zero value
 *     (TRUE) iff a has the same value as b.
 * int AFS_hiszero(afs_hyper_t a) -- returns TRUE iff a is zero.
 * int AFS_hfitsinu32(afs_hyper_t a) -- returns TRUE iff 0 <= a < 2^32.
 * int AFS_hfitsin32(afs_hyper_t a) -- returns TRUE iff -2^31 <= a < 2^31.
 * void AFS_hzero(afs_hyper_t a) -- sets a to zero.
 * void AFS_hzerohi(afs_hyper_t a) -- clears the most significant 32 bits of a.
 * u_int32 AFS_hgetlo(afs_hyper_t a) -- returns the least significant 32 bits
 *     of a.
 * u_int32 AFS_hgethi(afs_hyper_t a) -- returns the most significant 32 bits of
 *     a.
 * void AFS_hset64(afs_hyper_t a, u_int32 hi, u_int32 lo) -- sets a to (hi<<32
 *     + lo).  So that AFS_hset64(h, AFS_hgethi(h), AFS_hgetlo(h)) leaves "h"
 *     unchanged.
 * AFS_HINIT(u_int32 hi, u_int32 lo) -- an initializer of type afs_hyper_t.
 * void AFS_hleftshift(afs_hyper_t a, u_int amt) -- shifts a left by amt bits;
 *     where 0 < amt < 64.
 * void AFS_hrightshift(afs_hyper_t a, u_int amt) -- logically shifts a right
 *     by amt bits; where 0 < amt < 64.
 * void AFS_hset32(afs_hyper_t a, int32 i) -- sets a to the 64bit sign
 *     extended value of i.  If "i" is unsigned use AFSA_hset64(a,0,i).
 * void AFS_hadd32(afs_hyper_t a, int32 i) -- adds i to a.
 * void AFS_hadd(afs_hyper_t a, afs_hyper_t b) -- adds b to a.
 * void AFS_hsub(afs_hyper_t a, afs_hyper_t b) -- subtracts b from a.
 * void AFS_hnegate(afs_hyper_t a) - sets a to its twos complement.
 * void AFS_HOP(afs_hyper_t a, <op>, afs_hyper_t b) -- like a = a <op> b where
 *     <op> should be one of {|,&,^,&~}.
 * void AFS_HOP32(afs_hyper_t a, <op>, u_int32 b) -- works like AFS_HOP except
 *     that b is logically extended to 64 bits by prepending 32 zero bits (ie
 *     no sign extension).
 * void AFS_hincr(afs_hyper_t a) -- short for AFS_hadd32(a,1).
 * void AFS_hdecr(afs_hyper_t a) -- short for AFS_hadd32(a,-1).
 * int AFS_hissubset(afs_hyper_t a, afs_hyper_t b) -- returns TRUE iff all the
 *     bits set in a are also set in b [a subset of b].
 */

/* A handy short-hand for calling printf */
#define AFS_HGETBOTH(a) AFS_hgethi(a), AFS_hgetlo(a)

/* These are the same in both worlds */
#define AFS_hcmp64(a,hi,lo) \
    (AFS_hgethi(a) < (u_int32)(hi) ? -1 : \
     (AFS_hgethi(a) > (u_int32)(hi) ? 1 : \
      (AFS_hgetlo(a) < (u_int32)(lo) ? -1 : \
       (AFS_hgetlo(a) > (u_int32)(lo) ? 1 : 0))))
#define AFS_hfitsinu32(a) (AFS_hgethi(a) == 0)
#define AFS_hfitsin32(a) \
    (AFS_hgethi(a) == ((AFS_hgetlo(a)&0x80000000) ? -1 : 0))
#define AFS_hzerohi(a) AFS_hset64(a,0,AFS_hgetlo(a))

#ifdef AFS_64BIT_HAS_SCALAR_TYPE

/* Rename platform specific 64bit scalar to generic afs_hyper_t. */
typedef afs_64bit_scalar_type afs_hyper_t;

/* Platform specific support must also define:
 *     afs_64bit_signed_scalar_type
 *     AFS_64BIT_ZERO
 *     AFS_64BIT_LOW_MASK */

#define AFS_hcmp(a,b) ((a)<(b) ? -1 : ((a)>(b) ? 1 : 0))
#define AFS_hsame(a,b) ((a) == (b))
#define AFS_hiszero(a) ((a) == AFS_64BIT_ZERO)
#define AFS_hzero(a) ((a) = AFS_64BIT_ZERO)
#define AFS_hgetlo(a) ((u_int32)((a) & AFS_64BIT_LOW_MASK))
#define AFS_hgethi(a) ((u_int32)(((a) >> 32) & AFS_64BIT_LOW_MASK))
#define AFS_hset64(a,hi,lo) \
    ((a) = ((((afs_hyper_t)(hi))<<32) | \
	    ((afs_hyper_t)(lo) & AFS_64BIT_LOW_MASK)))
#define AFS_hleftshift(a,amt) ((a) <<= (amt))
#define AFS_hrightshift(a,amt) ((a) >>= (amt))
#define AFS_hset32(a,i) (a = ((afs_hyper_t)((int32)(i))))
#define AFS_HINIT(hi, lo) \
    (((afs_hyper_t)(hi)<<32) | ((afs_hyper_t)(lo) & AFS_64BIT_LOW_MASK))
#define AFS_hadd32(a,i) ((a) += (i))
#define AFS_hadd(a,b) ((a) += (b))
#define AFS_hsub(a,b) ((a) -= (b))
#define AFS_hincr(a) ((a)++)
#define AFS_hdecr(a) ((a)--)
#ifdef SGIMIPS 
#define AFS_hnegate(a) ((a) = ~((afs_64bit_scalar_type)(a)))
#else  /* SGIMIPS */
#define AFS_hnegate(a) ((a) = -((afs_64bit_signed_scalar_type)(a)))
#endif /* SGIMIPS */

#define AFS_HOP(a,op,b) ((a) = (a) op (b))
#define AFS_HOP32(a,op,lo) ((a) = (a) op ((lo) & AFS_64BIT_LOW_MASK))
#define AFS_hissubset(a,b) (((a) & ~(b)) == 0)

#else /* AFS_64BIT_HAS_SCALAR_TYPE */

/* Create a generic 64-bit int from two 32-bit parts. */
typedef struct {
    u_int32 h_high;
    u_int32 h_low;
} afs_hyper_t;

#define AFS_hcmp(a,b) \
    ((a).h_high < (b).h_high ? -1 : \
     ((a).h_high > (b).h_high ? 1 : \
      ((a).h_low < (b).h_low ? -1 : ((a).h_low > (b).h_low ? 1 : 0))))
#define AFS_hsame(a,b) ((a).h_low == (b).h_low && (a).h_high == (b).h_high)
#define AFS_hiszero(a) ((a).h_low == 0 && (a).h_high == 0)
#define AFS_hzero(a) ((a).h_low = 0, (a).h_high = 0)
#define AFS_hgetlo(a) ((a).h_low)
#define AFS_hgethi(a) ((a).h_high)
#define AFS_hset64(a,hi,lo) ((a).h_low = (lo), (a).h_high = (hi))
#define AFS_hleftshift(a,amt) \
    (((amt) >= 32) ? ((a).h_high = (a).h_low << ((amt)-32), (a).h_low = 0) \
     /* ams < 32 */: ((a).h_high <<= (amt), \
		      (a).h_high |= (((a).h_low) >> (32 - (amt))), \
		      (a).h_low <<= (amt)))
#define AFS_hrightshift(a,amt) \
    (((amt) >= 32) ? ((a).h_low = (a).h_high >> ((amt)-32), \
		      (a).h_high = 0) \
     /* amt < 32 */: ((a).h_low >>= (amt), \
		      (a).h_low |= (a).h_high << (32 - (amt)), \
		      (a).h_high = (a).h_high >> (amt)))
#define AFS_hset32(a,i) \
    ((a).h_high = (((int32)(i)<0) ? -1 : 0), (a).h_low = (u_int32)(i))
#define AFS_HINIT(high, low) { (u_int32)(high), (u_int32)(low) }
#define AFS_hadd32(a,i) \
    (((int32)(i) > 0 ? ((a).h_high += ((i) > (0xffffffffU - (a).h_low))) \
      /* i < 0 */    : ((a).h_high -= ((a).h_low < -(i)))), \
     (a).h_low += (i))
#define AFS_hadd(a,b) \
    ((a).h_high += (b).h_high + ((b).h_low > (0xffffffffU - (a).h_low)), \
     (a).h_low += (b).h_low)
#define AFS_hsub(a,b) \
    ((a).h_high -= (b).h_high + ((a).h_low < (b).h_low), \
     (a).h_low -= (b).h_low)
#define AFS_hincr(a) (!(++(a).h_low) && (a).h_high++)
#define AFS_hdecr(a) (!((a).h_low--) && (a).h_high--)
#define AFS_hnegate(a) \
    ((a).h_high = ~(a).h_high, (a).h_low = ~(a).h_low, AFS_hincr(a))

#define AFS_HOP(a,op,b) \
    ((a).h_high = (a).h_high op (b).h_high, (a).h_low = (a).h_low op (b).h_low)
#define AFS_HOP32(a,op,lo) \
    ((a).h_high = (a).h_high op 0, (a).h_low = (a).h_low op (lo))
#define AFS_hissubset(a,b) \
    (!((a).h_low & ~(b).h_low) && !((a).h_high & ~(b).h_high))

#endif /* AFS_64BIT_HAS_SCALAR_TYPE */


/*
 * The caller will still have to include <netinet/in.h> to make these
 * work.
 */

#define hton32 htonl
#define hton16 htons
#define ntoh32 ntohl
#define ntoh16 ntohs

/*
 * Define a portable concatenation macro.
 */
#if defined(__STDC__) || defined(__cplusplus)

#define CONC(a, b) a ## b
#define UTIL_STRING(a)  # a

#else /* defined(__STDC__) || defined(__cplusplus) */

#ifdef _IBMR2
/* This definition should go away once the RS6000 compiler compiles with
 * its documentation.
 */

#define CONC(a, b) a/**/b

#else /* ! _IBMR2 */

#define UTIL_IDENT(a) a
#define CONC(a, b) UTIL_IDENT(a)b

#endif /* _IBMR2 */

#define UTIL_STRING(a) "a"

#endif /* defined(__STDC__) || defined(__cplusplus) */


#endif /* TRANSARC_CONFIG_STDS_H */
