/*
 * Copyright 1989-1995 Silicon Graphics Inc.  All rights reserved.
 *
 * This file defines MAXNAMLEN for the kernel and for BSD-based usercode.
 * It declares the 4.3BSD directory(3C) interface if compiled user-level.
 * The libc entry points are renamed by macros at the bottom of this file
 * to BSDopendir, BSDreaddir, etc.
 */
#ifndef __SYS_DIR_H__
#define __SYS_DIR_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.17 $"

#include <standards.h>
#include <sgidefs.h>

/*
 * The upper bound on a directory entry's name length.
 */
#define MAXNAMLEN	255

#ifndef _KERNEL
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dir.h	7.2 (Berkeley) 12/22/86
 */

/*
 * A directory consists of some number of blocks of DIRBLKSIZ
 * bytes, where DIRBLKSIZ is chosen such that it can be transferred
 * to disk in a single atomic operation (e.g. 512 bytes on most machines).
 *
 * Each DIRBLKSIZ byte block contains some number of directory entry
 * structures, which are of variable length.  Each directory entry has
 * a struct direct at the front of it, containing its inode number,
 * the length of the entry, and the length of the name contained in
 * the entry.  These are followed by the name padded to a 4 byte boundary
 * with null bytes.  All names are guaranteed null terminated.
 * The maximum length of a name in a directory is MAXNAMLEN.
 *
 * The macro DIRSIZ(dp) gives the amount of space required to represent
 * a directory entry.  Free space in a directory is represented by
 * entries which have dp->d_reclen > DIRSIZ(dp).  All DIRBLKSIZ bytes
 * in a directory block are claimed by the directory entries.  This
 * usually results in the last entry in a directory having a large
 * dp->d_reclen.  When entries are deleted from a directory, the
 * space is returned to the previous entry in the same directory
 * block by increasing its dp->d_reclen.  If the first entry of
 * a directory block is free, then its dp->d_ino is set to 0.
 * Entries other than the first in a directory do not normally have
 * dp->d_ino set to 0.
 */
#define	DIRBLKSIZ	4096

#if (_MIPS_SIM == _ABIN32)
typedef	__uint64_t		__bsdino;
#else
typedef	u_long			__bsdino;
#endif

struct	direct {
	__bsdino d_ino;			/* inode number of entry */
	u_short	d_reclen;		/* length of this record */
	u_short	d_namlen;		/* length of string in d_name */
	char	d_name[MAXNAMLEN + 1];	/* name must be no longer than this */
};

/*
 * The DIRSIZ macro gives the minimum record length which will hold
 * the directory entry.  This requires the amount of space in struct direct
 * without the d_name field, plus enough space for the name with a terminating
 * null byte (dp->d_namlen+1), rounded up to a (sizeof __bsdino) byte boundary.
 */
#undef DIRSIZ
#define	__DIRALIGN	(sizeof(__bsdino)-1)
#define DIRSIZ(dp) \
	((sizeof (struct direct) - (MAXNAMLEN+1)) + \
	 (((dp)->d_namlen+1 + __DIRALIGN) &~ __DIRALIGN))

/*
 * Definitions for library routines operating on directories.
 */
typedef struct _dirdesc {
	int	dd_fd;
	long	dd_loc;
	long	dd_size;
#if (_MIPS_SIM == _ABIN32)
	int	dd_pad0;	/* force dd_buf to 8-byte alignment */
				/* so d_ino in dirent will be aligned */
#endif
	char	dd_buf[DIRBLKSIZ];
	struct	direct dd_direct;
} DIR;

#define dirfd(dirp)	((dirp)->dd_fd)

#ifndef NULL
#define NULL 0L
#endif

#if _NO_ABIAPI
/*
 * Functions defined on directories.
 */
#define	opendir		BSDopendir
#define	readdir		BSDreaddir
#define	telldir		BSDtelldir
#define	seekdir		BSDseekdir
#define	rewinddir(dirp)	seekdir((dirp), 0L)
#define	closedir	BSDclosedir
#define	scandir		BSDscandir
#define	alphasort	BSDalphasort

extern DIR  *BSDopendir(const char *);
extern struct direct *BSDreaddir(DIR *);
extern long BSDtelldir(DIR *);
extern void BSDseekdir(DIR *, long);
extern void BSDclosedir(DIR *);
extern int  BSDscandir(const char *, struct direct **[], 
	int (*)(struct direct *), int (*)(struct direct **, struct direct **));
extern int  BSDalphasort(struct direct **, struct direct **);
#endif /* _NO_ABIAPI */

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_DIR_H__ */
