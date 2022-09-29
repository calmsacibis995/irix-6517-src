/* errno.h - errno numbers */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)errno.h	7.1 (Berkeley) 6/4/86
 */

/*
modification history
--------------------
01m,15nov93,dvs  added ENOSYS and ECANCELED
01l,13nov92,dnw  added conditional definition of errno for non-ANSI (SPR #1771)
01k,02oct92,srh  added c++ version of errno macro
01j,22sep92,rrr  added support for c++
01i,18jul92,smb  finished ANSI merge
01i,08jul92,smb  Phase 1 of ANSI merge
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01e,25oct90,dnw  removed some unused definitions.
01d,05oct90,shl  added copyright notice.
01c,24sep90,elr  ifdef'd errno declaration for assembly.
01b,13jul90,dnw  added declaration of errno for ANSI compliance
*/

#ifndef __INCerrnoh
#define __INCerrnoh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

/*
 * POSIX Error codes
 */

#define	EPERM		1		/* Not owner */
#define	ENOENT		2		/* No such file or directory */
#define	ESRCH		3		/* No such process */
#define	EINTR		4		/* Interrupted system call */
#define	EIO		5		/* I/O error */
#define	ENXIO		6		/* No such device or address */
#define	E2BIG		7		/* Arg list too long */
#define	ENOEXEC		8		/* Exec format error */
#define	EBADF		9		/* Bad file number */
#define	ECHILD		10		/* No children */
#define	EAGAIN		11		/* No more processes */
#define	ENOMEM		12		/* Not enough core */
#define	EACCES		13		/* Permission denied */
#define	EFAULT		14		/* Bad address */
#define	ENOTEMPTY	15		/* Directory not empty */
#define	EBUSY		16		/* Mount device busy */
#define	EEXIST		17		/* File exists */
#define	EXDEV		18		/* Cross-device link */
#define	ENODEV		19		/* No such device */
#define	ENOTDIR		20		/* Not a directory*/
#define	EISDIR		21		/* Is a directory */
#define	EINVAL		22		/* Invalid argument */
#define	ENFILE		23		/* File table overflow */
#define	EMFILE		24		/* Too many open files */
#define	ENOTTY		25		/* Not a typewriter */
#define	ENAMETOOLONG	26		/* File name too long */
#define	EFBIG		27		/* File too large */
#define	ENOSPC		28		/* No space left on device */
#define	ESPIPE		29		/* Illegal seek */
#define	EROFS		30		/* Read-only file system */
#define	EMLINK		31		/* Too many links */
#define	EPIPE		32		/* Broken pipe */
#define	EDEADLK		33		/* Resource deadlock avoided */
#define	ENOLCK		34		/* No locks available */
#define	ENOTSUP		35		/* Unsupported value */
#define	EMSGSIZE	36		/* Message size */

/* ANSI math software */
#define	EDOM		37		/* Argument too large */
#define	ERANGE		38		/* Result too large */

/* ipc/network software */

	/* argument errors */
#define	EDESTADDRREQ	40		/* Destination address required */
#define	EPROTOTYPE	41		/* Protocol wrong type for socket */
#define	ENOPROTOOPT	42		/* Protocol not available */
#define	EPROTONOSUPPORT	43		/* Protocol not supported */
#define	ESOCKTNOSUPPORT	44		/* Socket type not supported */
#define	EOPNOTSUPP	45		/* Operation not supported on socket */
#define	EPFNOSUPPORT	46		/* Protocol family not supported */
#define	EAFNOSUPPORT	47		/* Addr family not supported */
#define	EADDRINUSE	48		/* Address already in use */
#define	EADDRNOTAVAIL	49		/* Can't assign requested address */
#define	ENOTSOCK	50		/* Socket operation on non-socket */

	/* operational errors */
#define	ENETUNREACH	51		/* Network is unreachable */
#define	ENETRESET	52		/* Network dropped connection on reset*/
#define	ECONNABORTED	53		/* Software caused connection abort */
#define	ECONNRESET	54		/* Connection reset by peer */
#define	ENOBUFS		55		/* No buffer space available */
#define	EISCONN		56		/* Socket is already connected */
#define	ENOTCONN	57		/* Socket is not connected */
#define	ESHUTDOWN	58		/* Can't send after socket shutdown */
#define	ETOOMANYREFS	59		/* Too many references: can't splice */
#define	ETIMEDOUT	60		/* Connection timed out */
#define	ECONNREFUSED	61		/* Connection refused */
#define	ENETDOWN	62		/* Network is down */
#define	ETXTBSY		63		/* Text file busy */
#define	ELOOP		64		/* Too many levels of symbolic links */
#define	EHOSTUNREACH	65		/* No route to host */
#define	ENOTBLK		66		/* Block device required */
#define	EHOSTDOWN	67		/* Host is down */

/* non-blocking and interrupt i/o */
#define	EINPROGRESS	68		/* Operation now in progress */
#define	EALREADY	69		/* Operation already in progress */
#define	EWOULDBLOCK	70		/* Operation would block */

#define	ENOSYS		71		/* Function not implemented */

/* aio errors (should be under posix) */
#define	ECANCELED	72		/* Operation canceled */

#define	ERRMAX		73


/* Definition of errno */

#ifndef _ASMLANGUAGE

#if defined(__STDC__) || defined(__cplusplus)
extern int *	__errno(void);
#else
extern int *	__errno();
#endif	/* __STDC__ */


#ifdef __cplusplus

inline int &__errnoRef()
    {
    return (*__errno());
    }

#define errno   __errnoRef()

#else

#define errno	(*__errno())

#endif  /* __cplusplus */

#endif	/* _ASMLANGUAGE */


#ifdef __cplusplus
}
#endif

#endif /* __INCerrnoh */
