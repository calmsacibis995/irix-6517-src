#ifndef _SYS_FNCTL_H
#define _SYS_FNCTL_H

/* Copyright (C) 1989 Silicon Graphics, Inc. All rights reserved.  */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/fcntl.h	10.3"*/
#ident	"$Revision: 3.73 $"

/*
 * This is POSIX/XPG Header - watch for Name space pollution
 */
/*#include <standards.h>*/

#ifndef _SYS_TYPES_H
#include <sys/types.h>
#endif

#if _SGIAPI && !defined(_F_FLAGS)
/* Irix compatibility */
#define _F_FLAGS
/*
 * flags for F_GETFL, F_SETFL
 */
#define	FNDELAY		0x04	/* Non-blocking I/O */
#define	FAPPEND		0x08	/* append (writes guaranteed at the end) */
#define	FSYNC		0x10	/* synchronous write option for files */
#define	FDSYNC		0x20	/* synchronous write option for data */
#define	FRSYNC		0x40	/* synchronous data integrity read option */
#define	FNONBLOCK	0x80	/* Non-blocking I/O */
#define	FASYNC		0x1000	/* interrupt-driven I/O for sockets */
#define	FLARGEFILE	0x2000	/* open is large file aware */
#define	FNONBLK		FNONBLOCK
#define	FDIRECT		0x8000
#define FBULK		0x10000	/* loosen semantics for sequential bandwidth */
#define FLCINVAL	0x20000 /* flush and invalidate cache on last close */
#define FLCFLUSH	0x40000 /* flush on last close */
#ifdef _KERNEL
#define	FDIRENT64	0x8000	/* getdents64 in use, same as FDIRECT */
#endif	/* _KERNEL */

/*
 * open only modes
 */
#define	FCREAT	0x0100		/* create if nonexistent */
#define	FTRUNC	0x0200		/* truncate to zero length */
#define	FEXCL	0x0400		/* error if already created */
#define	FNOCTTY	0x0800		/* don't make this tty control term */
#endif /* _SGIAPI && !defined(_F_FLAGS) */

/*
 * Flag values accessible to open(2) and fcntl(2)
 * (the first three and O_DIRECT can only be set by open).
 */
#define	O_RDONLY	0
#define	O_WRONLY	1
#define	O_RDWR		2
#define	O_NDELAY	0x04	/* non-blocking I/O */
#define	O_APPEND	0x08	/* append (writes guaranteed at the end) */
#define	O_SYNC		0x10	/* synchronous write option (POSIX) */
#define	O_DSYNC		0x20	/* synchronous write option for data (POSIX)*/
#define	O_RSYNC		0x40	/* synchronous data integrity read (POSIX) */
#define	O_NONBLOCK	0x80	/* non-blocking I/O (POSIX) */
#define	O_LARGEFILE	0x2000	/* allow large file opens */
#define O_DIRECT	0x8000	/* direct I/O */
#define O_BULK		0x10000	/* loosen semantics for sequential bandwidth */
#define O_LCINVAL	0x20000 /* flush and invalidate on last close */
#define O_LCFLUSH	0x40000 /* flush on last close */
/*
 * Flag values accessible only to open(2).
 */
#define	O_CREAT		0x100	/* open with file create (uses third open arg) */
#define	O_TRUNC		0x200	/* open with truncation */
#define	O_EXCL		0x400	/* exclusive open */
#define	O_NOCTTY	0x800	/* don't allocate controlling tty (POSIX) */

/* fcntl(2) requests */
#define	F_DUPFD		0	/* Duplicate fildes */
#define	F_GETFD		1	/* Get fildes flags */
#define	F_SETFD		2	/* Set fildes flags */
#define	F_GETFL		3	/* Get file flags */
#define	F_SETFL		4	/* Set file flags */

#define	F_SETLK		6	/* Set file lock */
#define	F_SETLKW	7	/* Set file lock and wait */
#define	F_CHKFL		8	/* Unused */

#define	F_ALLOCSP	10	/* Reserved */
#define	F_FREESP	11	/* Free file space */
#define	F_SETBSDLK	12	/* Set Berkeley record lock */
#define	F_SETBSDLKW	13	/* Set Berkeley record lock and wait */
#define	F_GETLK		14	/* Get file lock */
#define	F_CHKLK		15	/* check for file locks - internal use only */
#define	F_CHKLKW	16	/* check for file locks - internal use only */
#define	F_CLNLK		17	/* clean file locks - internal use only */

#define F_RSETLK	20	/* Remote SETLK for NFS */
#define F_RGETLK	21	/* Remote GETLK for NFS */
#define F_RSETLKW	22	/* Remote SETLKW for NFS */
#define	F_GETOWN	23	/* Get owner (sock emulation) - sockets only */
#define	F_SETOWN	24	/* Set owner (sock emulation) - sockets only */

#define	F_DIOINFO	30	/* get direct I/O parameters */
#define	F_FSGETXATTR	31	/* get extended file attributes (xFS) */
#define	F_FSSETXATTR	32	/* set extended file attributes (xFS) */
#define	F_GETLK64	33	/* Get 64 bit file lock */
#define	F_SETLK64	34	/* Set 64 bit file lock */
#define	F_SETLKW64	35	/* Set 64 bit file lock and wait */
#define	F_ALLOCSP64	36	/* Alloc 64 bit file space */
#define	F_FREESP64	37	/* Free 64 bit file space */
#define	F_GETBMAP	38	/* Get block map (64 bit only) */
#define	F_FSSETDM	39	/* Set DMI event mask and state (XFS only) */
#define F_RESVSP	40	/* Reserve file space. Allocate space for the 
				 * file without zeroing file data or changing 
				 * file size */
#define F_UNRESVSP	41	/* Remove file space. */
#define F_RESVSP64	42	/* Reserv 64 bit file space */
#define F_UNRESVSP64	43	/* Remove 64 bit file space */
#define	F_GETBMAPA	44	/* Get block map for attributes (64 bit only) */
#define	F_FSGETXATTRA	45	/* get extended file attributes (xFS-A) */
#define F_SETBIOSIZE	46	/* set preferred buffered I/O size (XFS-only) */
#define F_GETBIOSIZE	47	/* get preferred buffered I/O size (XFS-only) */

#define F_GETOPS        50      /* get operation table */
#define	F_DMAPI		51	/* get XFS DMAPI info (internal use only) */
#define	F_FSYNC		52	/* fsync a range of pages */
#define	F_FSYNC64	53	/* fsync a range of pages */

#define	F_GETBDSATTR	54	/* get attributes for a BDS filesystem */
#define	F_SETBDSATTR	55	/* set attributes for a BDS filesystem */
#define F_GETBMAPX	56	/* Get block map (extended interface) */
#define F_SETPRIO	57	/* set file io priority */
#define F_GETPRIO	58	/* get file io priority */


#if !defined(LANGUAGE_C_PLUS_PLUS) || !defined(_BSD_COMPAT)
/*
 * File segment locking set data type - information passed to system by user.
 */
typedef struct flock {
	short	l_type;
	short	l_whence;
	off_t	l_start;
	off_t	l_len;		/* len == 0 means until end of file */
        long	l_sysid;
        pid_t	l_pid;
	long	l_pad[4];		/* reserve area */
} flock_t;

#if _LFAPI
/*
 * File segment locking set data type for 64 bit access.
 */
typedef struct flock64 {
	short	l_type;
	short	l_whence;
	off64_t	l_start;
	off64_t	l_len;		/* len == 0 means until end of file */
        long	l_sysid;
        pid_t	l_pid;
	long	l_pad[4];		/* reserve area */
} flock64_t;
#endif /* _LFAPI */

#if defined(_KERNEL) || defined(_KMEMUSER)
typedef struct o_flock {
	short	l_type;
	short	l_whence;
	long	l_start;
	long	l_len;		/* len == 0 means until end of file */
        short   l_sysid;
        o_pid_t l_pid;
} o_flock_t;
#endif	/* defined(_KERNEL) */

#endif	/* !C++ || !BSD */

/*
 * File segment locking types.
 */
#define	F_RDLCK	01	/* Read lock */
#define	F_WRLCK	02	/* Write lock */
#define	F_UNLCK	03	/* Remove lock(s) */

/*
 * POSIX constants 
 */

#define	O_ACCMODE	3	/* Mask for file access modes */
#define	FD_CLOEXEC	1	/* close on exec flag */

#if _SGIAPI

/*
 * NOTE: This flag is checked only for graphics processes.  It is for
 *       SGI internal use only.
 */
#define FD_NODUP_FORK	4	/* don't dup fd on fork (sproc overrides) */

struct biosize {
	__uint32_t 	biosz_flags;
	__int32_t	biosz_read;
	__int32_t	biosz_write;
	__int32_t	dfl_biosz_read;
	__int32_t	dfl_biosz_write;
};
	
/* 
 * direct I/O attribute record used with F_DIOINFO
 * d_miniosz is the min xfer size, xfer size multiple and file seek offset
 * alignment.
 */
struct dioattr {
	unsigned	d_mem;		/* data buffer memory alignment */
	unsigned	d_miniosz;	/* min xfer size */
	unsigned	d_maxiosz;	/* max xfer size */
};

/*
 * Structure for F_FSGETXATTR[A] and F_FSSETXATTR.
 */
struct fsxattr {
	__uint32_t 	fsx_xflags;	/* xflags field value (get/set) */
	__uint32_t 	fsx_extsize;	/* extsize field value (get/set) */
	__uint32_t 	fsx_nextents;	/* nextents field value (get) */
	char		fsx_pad[16];
};

/*
 * Structure for F_GETBMAP.
 * On input, fill in bmv_offset and bmv_length of the first structure
 * to indicate the area of interest in the file, and bmv_entry with the
 * number of array elements given.  The first structure is updated on
 * return to give the offset and length for the next call.
 */
struct getbmap {
	__int64_t	bmv_offset;	/* file offset of segment in blocks */
	__int64_t	bmv_block;	/* starting block (64-bit daddr_t) */
	__int64_t	bmv_length;	/* length of segment, blocks */
	__int32_t	bmv_count;	/* # of entries in array incl. first */
	__int32_t	bmv_entries;	/* # of entries filled in (output) */
};

/*
 *	Structure for F_GETBMAPX.  The fields bmv_offset through bmv_entries
 *	are used exactly as in the getbmap structure.  The getbmapx structure
 *	has additional bmv_iflags and bmv_oflags fields. The bmv_iflags field
 *	is only used for the first structure.  It contains input flags 
 *	specifying F_GETBMAPX actions.  The bmv_oflags field is filled in
 *	by the F_GETBMAPX command for each returned structure after the first.
 */
struct getbmapx {
	__int64_t	bmv_offset;	/* file offset of segment in blocks */
	__int64_t	bmv_block;	/* starting block (64-bit daddr_t) */
	__int64_t	bmv_length;	/* length of segment, blocks */
	__int32_t	bmv_count;	/* # of entries in array incl. first */
	__int32_t	bmv_entries;	/* # of entries filled in (output). */
	__int32_t	bmv_iflags;	/* input flags (1st structure) */
	__int32_t	bmv_oflags;	/* output flags (after 1st structure) */
	__int32_t	bmv_unused1;	/* future use */
	__int32_t	bmv_unused2;	/* future use */
};

/*	bmv_iflags values - set by F_GETBMAPX caller.	*/

#define	BMV_IF_ATTRFORK		0x1	/* return attr fork rather than data */
#define BMV_IF_NO_DMAPI_READ	0x2	/* Do not generate DMAPI read event */
#define BMV_IF_PREALLOC		0x4	/* return status BMV_OF_PREALLOC if required*/

#define BMV_IF_VALID		(BMV_IF_ATTRFORK|BMV_IF_NO_DMAPI_READ|BMV_IF_PREALLOC)

/*	bmv_oflags values - returned from F_GETBMAPX for each non-header segment */

#define BMV_OF_PREALLOC		0x1	/* segment is unwritten pre-allocation */

/*	Convert getbmap <-> getbmapx - move fields from p1 to p2. */

#define	GETBMAP_CONVERT(p1,p2) { \
	p2.bmv_offset = p1.bmv_offset; \
	p2.bmv_block = p1.bmv_block; \
	p2.bmv_length = p1.bmv_length; \
	p2.bmv_count = p1.bmv_count; \
	p2.bmv_entries = p1.bmv_entries;  }

#ifdef	_KERNEL

/*	Kernel only bmv_iflags value.	*/
#define	BMV_IF_EXTENDED	0x40000000	/* getpmapx if set */
#endif	/* _KERNEL */


/*
 * Structure for F_FSSETDM.
 * For use by backup and restore programs to set the XFS on-disk inode
 * fields di_dmevmask and di_dmstate.  These must be set to exactly and
 * only values previously obtained via xfs_bulkstat!  (Specifically the
 * xfs_bstat_t fields bs_dmevmask and bs_dmstate.)
 */
struct fsdmidata {
	__int32_t	fsd_dmevmask;	/* corresponds to di_dmevmask */
	unsigned short	fsd_padding;
	unsigned short	fsd_dmstate;	/* corresponds to di_dmstate  */
};

#endif /* _SGIAPI */

#endif /* !__SYS_FNCTL_H__ */
