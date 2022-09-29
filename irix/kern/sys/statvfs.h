/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_STATVFS_H	/* wrapper symbol for kernel use */
#define _FS_STATVFS_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif
#ident	"$Revision: 1.18 $"
/*
 * Structure returned by statvfs(2).
 */

#include <standards.h>
#include <sys/types.h>

#if _SGIAPI
#define	FSTYPSZ		16
#define _FSTYPSZ	FSTYPSZ
#else
#define	_FSTYPSZ	16
#endif

typedef struct statvfs {
	ulong_t	f_bsize;	/* fundamental file system block size */
	ulong_t	f_frsize;	/* fragment size */
#if (_KERNEL || _SGIAPI || _XOPEN5)
	fsblkcnt_t f_blocks;	/* total # of blocks of f_frsize on fs */
	fsblkcnt_t f_bfree;	/* total # of free blocks of f_frsize */
	fsblkcnt_t f_bavail;	/* # of free blocks avail to non-superuser */
	fsfilcnt_t f_files;	/* total # of file nodes (inodes) */
	fsfilcnt_t f_ffree;	/* total # of free file nodes */
	fsfilcnt_t f_favail;	/* # of free nodes avail to non-superuser */
#elif (_MIPS_SIM == _ABIN32)
#define	_F_BLOCKS_HACK
	ulong_t	__f_blocks_high;
	ulong_t	f_blocks;	/* total # of blocks of f_frsize on fs */
	ulong_t	__f_bfree_high;
	ulong_t	f_bfree;	/* total # of free blocks of f_frsize */
	ulong_t	__f_bavail_high;
	ulong_t	f_bavail;	/* # of free blocks avail to non-superuser */
	ulong_t	__f_files_high;
	ulong_t	f_files;	/* total # of file nodes (inodes) */
	ulong_t	__f_ffree_high;
	ulong_t	f_ffree;	/* total # of free file nodes */
	ulong_t	__f_favail_high;
	ulong_t	f_favail;	/* # of free nodes avail to non-superuser */
#else
	ulong_t	f_blocks;	/* total # of blocks of f_frsize on fs */
	ulong_t	f_bfree;	/* total # of free blocks of f_frsize */
	ulong_t	f_bavail;	/* # of free blocks avail to non-superuser */
	ulong_t	f_files;	/* total # of file nodes (inodes) */
	ulong_t	f_ffree;	/* total # of free file nodes */
	ulong_t	f_favail;	/* # of free nodes avail to non-superuser */
#endif
	ulong_t	f_fsid;		/* file system id (dev for now) */
	char	f_basetype[_FSTYPSZ]; /* target fs type name, null-terminated */
	ulong_t	f_flag;		/* bit-mask of flags */
	ulong_t	f_namemax;	/* maximum file name length */
	char	f_fstr[32];	/* filesystem-specific string */
	ulong_t	f_filler[16];	/* reserved for future expansion */
} statvfs_t;

#if _LFAPI
typedef struct statvfs64 {
	ulong_t	f_bsize;	/* fundamental file system block size */
	ulong_t	f_frsize;	/* fragment size */
	fsblkcnt64_t f_blocks;	/* total # of blocks of f_frsize on fs */
	fsblkcnt64_t f_bfree;	/* total # of free blocks of f_frsize */
	fsblkcnt64_t f_bavail;	/* # of free blocks avail to non-superuser */
	fsfilcnt64_t f_files;	/* total # of file nodes (inodes) */
	fsfilcnt64_t f_ffree;	/* total # of free file nodes */
	fsfilcnt64_t f_favail;	/* # of free nodes avail to non-superuser */
	ulong_t	f_fsid;		/* file system id (dev for now) */
	char	f_basetype[_FSTYPSZ]; /* target fs type name, null-terminated */
	ulong_t	f_flag;		/* bit-mask of flags */
	ulong_t	f_namemax;	/* maximum file name length */
	char	f_fstr[32];	/* filesystem-specific string */
	ulong_t	f_filler[16];	/* reserved for future expansion */
} statvfs64_t;
#endif /* _LFAPI */

/*
 * Flag definitions.
 */

#define	ST_RDONLY	0x01	/* read-only file system */
#define	ST_NOSUID	0x02	/* does not support setuid/setgid semantics */
#define ST_NOTRUNC	0x04	/* does not truncate long file names */
#define ST_DMI          0x08    /* DMI is enabled for current mount of FS */
#define	ST_NODEV	0x20000000	/* disallow opening of device files */
#define	ST_GRPID	0x40000000	/* group-ID assigned from directory */
#define	ST_LOCAL	0x80000000	/* local filesystem, for find */

#ifndef _KERNEL
#if defined(_F_BLOCKS_HACK)

#undef _F_BLOCKS_HACK
#define _EOVERFLOW	79		/* see EOVERFLOW from errno.h */

int _statvfs(const char *, struct statvfs *);
int _fstatvfs(int, struct statvfs *);

/* REFERENCED */
static int
statvfs(const char *__path, struct statvfs *__buf)
{
	int __i = _statvfs(__path, __buf);
	if (__i == 0 &&
	    (__buf->__f_blocks_high || __buf->__f_bfree_high ||
	     __buf->__f_bavail_high || __buf->__f_files_high ||
	     __buf->__f_ffree_high || __buf->__f_favail_high))
		return _EOVERFLOW;
	return __i;
}

/* REFERENCED */
static int
fstatvfs(int __fd, struct statvfs *__buf)
{
	int __i = _fstatvfs(__fd, __buf);
	if (__i == 0 &&
	    (__buf->__f_blocks_high || __buf->__f_bfree_high ||
	     __buf->__f_bavail_high || __buf->__f_files_high ||
	     __buf->__f_ffree_high || __buf->__f_favail_high))
		return _EOVERFLOW;
	return __i;
}

#else

int statvfs(const char *, struct statvfs *);
int fstatvfs(int, struct statvfs *);

#endif /* _F_BLOCKS_HACK */

#if _LFAPI
int statvfs64(const char *, struct statvfs64 *);
int fstatvfs64(int, struct statvfs64 *);
#endif /* _LFAPI */
#endif /* _KERNEL */

#ifdef _KERNEL
#define STATVFS_VERS_32	0
#define	STATVFS_VERS_64	1

typedef struct irix5_statvfs {
	app32_ulong_t	f_bsize;
	app32_ulong_t	f_frsize;
	app32_ulong_t	f_blocks;
	app32_ulong_t	f_bfree;
	app32_ulong_t	f_bavail;
	app32_ulong_t	f_files;
	app32_ulong_t	f_ffree;
	app32_ulong_t	f_favail;
	app32_ulong_t	f_fsid;	
	char		f_basetype[FSTYPSZ];
	app32_ulong_t	f_flag;
	app32_ulong_t	f_namemax;
	char		f_fstr[32];
	app32_ulong_t	f_filler[16];
} irix5_statvfs_t;

typedef struct irix5_statvfs64 {
	app32_ulong_t		f_bsize;
	app32_ulong_t		f_frsize;
	app32_ulong_long_t	f_blocks;
	app32_ulong_long_t	f_bfree;
	app32_ulong_long_t	f_bavail;
	app32_ulong_long_t	f_files;
	app32_ulong_long_t	f_ffree;
	app32_ulong_long_t	f_favail;
	app32_ulong_t		f_fsid;	
	char			f_basetype[FSTYPSZ];
	app32_ulong_t		f_flag;
	app32_ulong_t		f_namemax;
	char			f_fstr[32];
	app32_ulong_t		f_filler[16];
} irix5_statvfs64_t, irix5_n32_statvfs_t;
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _FS_STATVFS_H */
