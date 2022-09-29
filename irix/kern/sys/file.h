/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_FILE_H	/* wrapper symbol for kernel use */
#define _FS_FILE_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)uts-3b2:fs/file.h	1.5"*/
#ident	"$Revision: 3.71 $"

#include <standards.h>
#if defined(_KERNEL) || defined(_KMEMUSER)
#include <sys/types.h>
#include <sys/sema.h>
#endif

#ifndef _KERNEL
#include <sys/fcntl.h>			/* Irix and BSD compatibility */
#endif

/* flags */
#define	FMASK		0x790FF

#define	FOPEN		0xFFFFFFFF
#define	FREAD		0x01
#define	FWRITE		0x02

#ifndef	_F_FLAGS
#define	_F_FLAGS
#define	FNDELAY		0x04
#define	FAPPEND		0x08
#define	FSYNC		0x10
#define FDSYNC		0x20
#define FRSYNC		0x40
#define	FNONBLOCK	0x80
#define	FASYNC		0x1000
#if _SGIAPI
#define	FNONBLK		FNONBLOCK
/*
 * When we really turn on FLARGEFILE, we need to modify FMASK above.
 */
#define	FLARGEFILE	0x2000
#define FDIRECT		0x8000
#define FBULK		0x10000	/* loosen semantics for sequential bandwidth */
#define FLCINVAL	0x20000 /* flush and invalidate cache on last close */
#define FLCFLUSH	0x40000 /* flush on last close */
#endif

/* open-only modes */
#define	FCREAT		0x0100
#define	FTRUNC		0x0200
#define	FEXCL		0x0400
#define	FNOCTTY		0x0800
#endif	/* _F_FLAGS */

#if defined(_KERNEL) || defined(_KMEMUSER)

/* more open file flags */
#define FINVIS		0x0100		/* don't update timestamps - XFS */
#define	FSOCKET		0x0200		/* open file refers to a vsocket */
#define FINPROGRESS	0x0400		/* being opened */
#define FPRIORITY	0x0800  	/* file is rate guaranteed */
#define FPRIO		0x4000  	/* file has non regular io priority */

/*
 * This flag goes only into uio_fmode for directories (in getdents).
 */
#define	FDIRENT64	0x8000
#endif

/* file descriptor flags */
#define	FCLOSEXEC	0x01	/* close on exec */

#ifndef _KERNEL
/* The following are extensions for 4.3BSD compatibility. */

/*
 * Flock(3) call.
 */
#define	LOCK_SH		1	/* shared lock */
#define	LOCK_EX		2	/* exclusive lock */
#define	LOCK_NB		4	/* don't block when locking */
#define	LOCK_UN		8	/* unlock */

#if !defined(__cplusplus) || defined(_BSD_COMPAT)
/*
 * In C++, struct flock from <sys/fcntl.h> collides with flock(3B)'s name.
 * Require C++ users to define _BSD_COMPAT to get the flock prototype.
 */
extern int flock(int, int);
#endif /* !__cplusplus || _BSD_COMPAT */

/*
 * Lseek call.	See the POSIX equivalents in <unistd.h>.
 */
#ifndef L_SET
#define	L_SET	0	/* for lseek */
#endif
#ifndef L_INCR
#define	L_INCR		1	/* relative to current offset */
#endif
#ifndef L_XTND
#define	L_XTND		2	/* relative to end of file */
#endif

#endif	/* !_KERNEL */

/*
 * Access call.  Also defined in <unistd.h>
 */
#ifndef F_OK
#define	F_OK		0	/* does file exist */
#define	X_OK		1	/* is it executable by caller */
#define	W_OK		2	/* writable by caller */
#define	R_OK		4	/* readable by caller */
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _FS_FILE_H */
