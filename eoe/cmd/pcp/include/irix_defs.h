/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * $Id: irix_defs.h,v 1.3 1999/05/11 00:28:03 kenmcd Exp $
 */

#ifndef _IRIX_DEFS_H
#define _IRIX_DEFS_H

/*
 * IRIX specific platform definitions and features
 */
#if !defined(sgi)
 # error platform_defs.h is currently the wrong platform
#endif

/* HAVE_NETWORK_BYTEORDER for big endian systems (not Intel) */
#include <sys/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define HAVE_NETWORK_BYTEORDER
#endif

/*
 * The type of timestamps in struct stat: either HAVE_STAT_TIMESPEC,
 * HAVE_STAT_TIMESTRUC or HAVE_STAT_TIME_T
 */
#if defined(IRIX5_3) || defined(IRIX6_2) || defined(IRIX6_4)
#define HAVE_STAT_TIMESTRUC
#else
#define HAVE_STAT_TIMESPEC /* IRIX6_5 or later uses struct timespec_t */
#endif

/* HAVE_ST_MTIME if stat.st_mtime has an ``e'' on the end */
/* #define HAVE_ST_MTIME_WITH_E */

/* if your compiler supports LL sufix on 64 bit int constants */
#define HAVE_CONST_LONGLONG 

/* if dlopen et al are available */
#define HAVE_DLFCN

/* if HPUX style shl_* shared library functions are available */
/* #define HAVE_SHL */

/* if hstrerror() is available */
#define HAVE_HSTRERROR

/* if flexlm is available */
#define HAVE_FLEXLM

/*
 * HAVE_UNDERBAR_ENVIRON if extern char **_environ is defined
 * (else extern char **environ will be used)
 */
#define HAVE_UNDERBAR_ENVIRON

/* HAVE_ISNANF if isnanf() is available */
#define HAVE_ISNANF

/* HAVE_ISNAND if isnand() is available */
#define HAVE_ISNAND

/* HAVE_IEEEFP_H if ieeefp.h header is available */
#define HAVE_IEEEFP_H

/*
 * If the pointer-to-function arguments to scandir()
 * are (*scanfn)(const struct dirent *dep)
 * Otherwise ``const'' will be omitted.
 */
/* #define HAVE_CONST_DIRENT */

/* if compiler can cast __uint64_t to double */
#define HAVE_CAST_U64_DOUBLE

/* if atexit() is available and works */
#define HAVE_ATEXIT

/* if the ndbm.h header is available */
#define HAVE_NDBM_H

/* if ndbm is available */
#define HAVE_NDBM

/* if readdir64 is available */
#define HAVE_READDIR64

/* if wait3() is available */
#define HAVE_WAIT3

/* if waitpid is available and supports WNOHANG */
#define HAVE_WAITPID

/* if brk() and sbrk() are available */
#define HAVE_SBRK

/* if termio signals are supported */
#define HAVE_TERMIO_SIGNALS

/* if the /proc pseudo filesystem exists */
#define HAVE_PROCFS

/* if the /proc/pinfo/<pid> should be used (IRIX only?) */
#define HAVE_PROCFS_PINFO

/* Location of pcplog */
#define PCPLOGDIR "/var/adm/pcplog"

/* if sproc() is available (IRIX only) */
#define HAVE_SPROC 

/* if __clone() is available (linux only) */
/* #define HAVE___CLONE */

/* if sched.h is available (for linux __clone()) */
/* #define HAVE_SCHED_H */


#endif /* _IRIX_DEFS_H */
