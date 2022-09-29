#ifndef	_SYS_DIRENT_H
#define	_SYS_DIRENT_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/dirent.h	10.5"*/
#ident	"$Revision: 3.21 $"

#include <sys/types.h>

/*
 * The following structure defines the file
 * system independent directory entry.
 *
 */

typedef struct dirent {			/* data from readdir() */
	ino_t		d_ino;		/* inode number of entry */
	off_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent_t;

#if _LFAPI
#if defined(_SGI_COMPILING_LIBC) && (_MIPS_SIM != _ABIO32)
typedef dirent_t dirent64_t;
#else
typedef struct dirent64 {		/* data from readdir64() */
#if (_MIPS_SIM == _ABIO32)
	ino64_t		d_ino;		/* inode number of entry */
	off64_t		d_off;		/* offset of disk directory entry */
#else
	ino_t		d_ino;		/* inode number of entry */
	off_t		d_off;		/* offset of disk directory entry */
#endif
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent64_t;
#endif /* _SGI_COMPILING_LIBC && _MIPS_SIM != _ABIO32 */

extern int getdents(int, dirent_t *, unsigned);
extern int getdents64(int, dirent64_t *, unsigned);
extern int ngetdents(int, dirent_t *, unsigned, int *);
extern int ngetdents64(int, dirent64_t *, unsigned, int *);

#define	DIRENTBASESIZE \
	(((dirent_t *)0)->d_name - (char *)0)
#define	DIRENTSIZE(namelen) \
	((DIRENTBASESIZE + (namelen) + sizeof(off_t)) & ~(sizeof(off_t) - 1))

#define	DIRENT64BASESIZE \
	(((dirent64_t *)0)->d_name - (char *)0)
#define	DIRENT64SIZE(namelen) \
	((DIRENT64BASESIZE + (namelen) + sizeof(off64_t)) & \
	 ~(sizeof(off64_t) - 1))
#endif /* _LFAPI */

#ifdef _KERNEL
#include <sys/ktypes.h>

/* Irix5 view of dirent structure. */
typedef struct irix5_dirent {		/* data from readdir() */
	app32_ulong_t	d_ino;		/* inode number of entry */
	irix5_off_t	d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} irix5_dirent_t;

#define	IRIX5_DIRENTBASESIZE \
	(((irix5_dirent_t *)0)->d_name - (char *)0)
#define	IRIX5_DIRENTSIZE(namelen) \
	((IRIX5_DIRENTBASESIZE + (namelen) + sizeof(irix5_off_t)) & \
	 ~(sizeof(irix5_off_t) - 1))

/*
 * Pick the right target abi based on the input (process's) abi,
 * the getdents64 flag, and the segflg.
 */
#define	GETDENTS_ABI(abi, uiop)	\
	(((uiop)->uio_segflg == UIO_USERSPACE && ABI_IS_IRIX5(abi) && \
	  ((uiop)->uio_fmode & FDIRENT64) == 0) ? ABI_IRIX5 : ABI_IRIX5_64)

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* !_SYS_DIRENT_H */
