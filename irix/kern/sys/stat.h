/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#ident	"$Revision: 3.62 $"

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#include <sys/types.h>
#include <sys/timespec.h>

#if _SGIAPI || _ABIAPI
/*
 * Historic Unix has sys/time.h being included here. XPG really doesn't
 * want it included. Alas, many programs assume struct timeval is available
 * when simply including this file...
 */
#include <sys/time.h>
#endif

#define _ST_FSTYPSZ	16	/* array size for file system type name */
#define	_ST_PAD1SZ	3	/* num longs in st_pad1 */
#define	_ST_PAD2SZ	2	/* num longs in st_pad2 */
#define	_ST_PAD4SZ	7	/* num longs in st_pad3 */

/*
 * stat structure, used by stat(2) and fstat(2)
 */
struct	stat {
	dev_t	st_dev;
	long	st_pad1[_ST_PAD1SZ];	/* reserved for network id */
	ino_t	st_ino;
	mode_t	st_mode;
	nlink_t st_nlink;
	uid_t 	st_uid;
	gid_t 	st_gid;
	dev_t	st_rdev;
	long	st_pad2[_ST_PAD2SZ];	/* dev and off_t expansion */
	off_t	st_size;
	long	st_pad3;	/* future off_t expansion */
	timespec_t st_atim;	
	timespec_t st_mtim;	
	timespec_t st_ctim;	
	long	st_blksize;
#if (_KERNEL || _SGIAPI || _XOPEN5)
	blkcnt_t st_blocks;
#elif (_MIPS_SIM == _ABIN32)
#define	_ST_BLOCKS_HACK
	union {
		blkcnt_t	__st_blocks_ll;
		struct {
			long	__st_blocks_high;
			long	__st_blocks_low;
		} __st_blocks_s;
	} __st_blocks_u;
#define	st_blocks	__st_blocks_u.__st_blocks_s.__st_blocks_low
#else
	long	st_blocks;
#endif
	char	st_fstype[_ST_FSTYPSZ];
	long	st_projid;
	long	st_pad4[_ST_PAD4SZ];	/* expansion area */
};

#if _LFAPI
/*
 * Stat structure used by stat64(2), fstat64(2), and lstat64(2).
 */
struct	stat64 {
	dev_t	st_dev;
	long	st_pad1[_ST_PAD1SZ];	/* reserved for network id */
	ino64_t	st_ino;
	mode_t	st_mode;
	nlink_t st_nlink;
	uid_t 	st_uid;
	gid_t 	st_gid;
	dev_t	st_rdev;
	long	st_pad2[_ST_PAD2SZ];	/* dev and off_t expansion */
	off64_t	st_size;
	long	st_pad3;	/* future off_t expansion */
	timespec_t st_atim;	
	timespec_t st_mtim;	
	timespec_t st_ctim;	
	long	st_blksize;
	blkcnt64_t st_blocks;
	char	st_fstype[_ST_FSTYPSZ];
	long	st_projid;
	long	st_pad4[_ST_PAD4SZ];	/* expansion area */
};
#endif /* _LFAPI */

#ifdef tv_sec
#define st_atime	st_atim.__tv_sec
#define st_mtime	st_mtim.__tv_sec
#define st_ctime	st_ctim.__tv_sec
#else
#define st_atime	st_atim.tv_sec
#define st_mtime	st_mtim.tv_sec
#define st_ctime	st_ctim.tv_sec
#endif

/* MODE MASKS */

/* de facto standard definitions */

#define	S_IFMT		0xF000	/* type of file */
#define S_IAMB		0x1FF	/* access mode bits */
#define	S_IFIFO		0x1000	/* fifo */
#define	S_IFCHR		0x2000	/* character special */
#define	S_IFDIR		0x4000	/* directory */
/*
 * The Xenix constructs are not supported by IRIX.
 */
#define	S_IFNAM		0x5000  /* XENIX special named file */
#define		S_INSEM 0x1	/* XENIX semaphore subtype of IFNAM */
#define		S_INSHD 0x2	/* XENIX shared data subtype of IFNAM */

#define	S_IFBLK		0x6000	/* block special */
#define	S_IFREG		0x8000	/* regular */
#define	S_IFLNK		0xA000	/* symbolic link */
#define	S_IFSOCK	0xC000	/* socket */

#ifndef S_ISUID
#define	S_ISUID		0x800	/* set user id on execution */
#define	S_ISGID		0x400	/* set group id on execution */
#endif	/* S_ISUID */

#define	S_ISVTX		0x200	/* save swapped text even after use */

#define	S_IREAD		00400	/* read permission, owner */
#define	S_IWRITE	00200	/* write permission, owner */
#define	S_IEXEC		00100	/* execute/search permission, owner */
#define	S_ENFMT		S_ISGID	/* record locking enforcement flag */

/* the following macros are for POSIX conformance */

#ifndef S_IRWXU
#define	S_IRWXU	00700		/* read, write, execute: owner */
#define	S_IRUSR	00400		/* read permission: owner */
#define	S_IWUSR	00200		/* write permission: owner */
#define	S_IXUSR	00100		/* execute permission: owner */
#define	S_IRWXG	00070		/* read, write, execute: group */
#define	S_IRGRP	00040		/* read permission: group */
#define	S_IWGRP	00020		/* write permission: group */
#define	S_IXGRP	00010		/* execute permission: group */
#define	S_IRWXO	00007		/* read, write, execute: other */
#define	S_IROTH	00004		/* read permission: other */
#define	S_IWOTH	00002		/* write permission: other */
#define	S_IXOTH	00001		/* execute permission: other */
#endif	/* S_IRWXU */

#define S_ISFIFO(mode)	((mode&S_IFMT) == S_IFIFO)
#define S_ISCHR(mode)	((mode&S_IFMT) == S_IFCHR)
#define S_ISDIR(mode)	((mode&S_IFMT) == S_IFDIR)
#define S_ISBLK(mode)	((mode&S_IFMT) == S_IFBLK)
#define S_ISREG(mode)	((mode&S_IFMT) == S_IFREG) 
#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)

#if _POSIX93 || _XOPEN5
/*
 * The following macros do not yet do anything as they refer to
 * POSIX options that are not yet implemented
 */
#define S_TYPEISMQ(buf)		(0)
#define S_TYPEISSEM(buf)	(0)
#define S_TYPEISSHM(buf) 	(0)
#endif

/* a version number is included in the SVR4 stat and mknod interfaces. */

#define _R3_MKNOD_VER 1		/* SVR3.0 mknod */
#define _MKNOD_VER 2		/* current version of mknod */
#define _R3_STAT_VER 1		/* SVR3.0 stat */
#define _STAT_VER 2		/* current version of stat */
#define	_STAT64_VER 3		/* [lf]stat64 system calls */	

#if !defined(_KERNEL)

int _fxstat(const int, int, struct stat *);
int _xstat(const int, const char *, struct stat *);
int _lxstat(const int, const char *, struct stat *);
int _xmknod(const int, const char *, mode_t, dev_t);

#if _XOPEN4UX
int fchmod(int, mode_t);
#endif	/* _XOPEN4UX */

extern int chmod(const char *, mode_t);
extern int mkdir(const char *, mode_t);
extern int mkfifo(const char *, mode_t);
extern mode_t umask(mode_t);

#if _ABIAPI
/*
 * ABI versions are required to call _xxx
 *
 *
 * NOTE: Application software should NOT program
 * to the _xstat interface.
 */
/* REFERENCED */
static int
stat(const char *__path, struct stat *__buf)
{
	return _xstat(_STAT_VER, __path, __buf);
}
/* REFERENCED */
static int
fstat(int __fd, struct stat *__buf)
{
        return _fxstat(_STAT_VER, __fd, __buf);
}

#if _XOPEN4UX
/* REFERENCED */
static int
lstat(const char *__path, struct stat *__buf)
{
        return _lxstat(_STAT_VER, __path, __buf);
}

/* REFERENCED */
static int
mknod(const char *__path, mode_t __mode, dev_t __dev)
{
        return _xmknod(_MKNOD_VER, __path, __mode, __dev);
}
#endif /* _XOPEN4UX */

#elif defined(_ST_BLOCKS_HACK)
#undef _ST_BLOCKS_HACK
#define _EOVERFLOW	79		/* see EOVERFLOW from errno.h */
#define	_LONG_MAX	2147483647L	/* N32 so long is 32 bits */
/* REFERENCED */
static int
stat(const char *__path, struct stat *__buf)
{
	int __i = _xstat(_STAT_VER, __path, __buf);
	if (__i == 0 && (__buf->__st_blocks_u.__st_blocks_ll > _LONG_MAX))
		return _EOVERFLOW;
	return __i;
}
/* REFERENCED */
static int
fstat(int __fd, struct stat *__buf)
{
        int __i = _fxstat(_STAT_VER, __fd, __buf);
	if (__i == 0 && (__buf->__st_blocks_u.__st_blocks_ll > _LONG_MAX))
		return _EOVERFLOW;
	return __i;
}
#if _XOPEN4UX
/* REFERENCED */
static int
lstat(const char *__path, struct stat *__buf)
{
        int __i = _lxstat(_STAT_VER, __path, __buf);
	if (__i == 0 && (__buf->__st_blocks_u.__st_blocks_ll > _LONG_MAX))
		return _EOVERFLOW;
	return __i;
}
int mknod(const char *, mode_t, dev_t);
#endif /* _XOPEN4UX */

#else /* !_ABIAPI && !_ST_BLOCKS_HACK */

int fstat(int, struct stat *);
int stat(const char *, struct stat *);
#if _XOPEN4UX
int lstat(const char *, struct stat *);
int mknod(const char *, mode_t, dev_t);
#endif /* _XOPEN4UX */
#endif /* _ABIAPI, _ST_BLOCKS_HACK */

#if _LFAPI
int fstat64(int, struct stat64 *);
int lstat64(const char *, struct stat64 *);
int stat64(const char *, struct stat64 *);
#endif

#endif /* !defined(_KERNEL) */

#if defined(_KERNEL)
#include "sys/kstat.h"
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STAT_H */
