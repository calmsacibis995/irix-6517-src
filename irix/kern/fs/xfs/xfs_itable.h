#ifndef _FS_XFS_ITABLE_H
#define	_FS_XFS_ITABLE_H

#ident	"$Revision: 1.22 $"

struct xfs_mount;
struct xfs_trans;

/*
 * Structures returned from xfs_bulkstat syssgi routine.
 */

/*
 * This is just like a timespec_t but the size is the same 
 * for 32 and 64 bit applications.
 */
typedef struct xfs_bstime
{
	time_t		tv_sec;		/* seconds */
	__int32_t	tv_nsec;	/* and nanoseconds */
} xfs_bstime_t;

typedef struct xfs_bstat
{
	ino64_t		bs_ino;		/* inode number */
	mode_t		bs_mode;	/* type and mode */
	nlink_t		bs_nlink;	/* number of links */
	uid_t		bs_uid;		/* user id */
	gid_t		bs_gid;		/* group id */
	dev_t		bs_rdev;	/* device value */
	__int32_t	bs_blksize;	/* block size */		
	off64_t		bs_size;	/* file size */
	xfs_bstime_t	bs_atime;	/* access time */
	xfs_bstime_t	bs_mtime;	/* modify time */
	xfs_bstime_t	bs_ctime;	/* inode change time */
	__int64_t	bs_blocks;	/* number of blocks */
	__uint32_t	bs_xflags;	/* extended flags */
	__int32_t	bs_extsize;	/* extent size */
	__int32_t	bs_extents;	/* number of extents */
	__uint32_t	bs_gen;		/* generation count */
	u_int16_t	bs_projid;	/* project id */
	char		bs_pad[14];	/* pad space, unused */
	__uint32_t	bs_dmevmask;	/* DMIG event mask */
	ushort_t	bs_dmstate;	/* DMIG state info */
	ushort_t	bs_aextents;	/* attribute number of extents */
} xfs_bstat_t;

/*
 * Flags for the bs_xflags field
 * There should be a one-to-one correspondence between these flags and the
 * XFS_DIFLAG_s.
 */
#define XFS_XFLAG_REALTIME	0x1
#define	XFS_XFLAG_PREALLOC	0x2
				/* no XFLAG for NEWRTBM */
#define	XFS_XFLAG_HASATTR	0x80000000	/* no DIFLAG for this */
#define XFS_XFLAG_ALL		\
	( XFS_XFLAG_REALTIME|XFS_XFLAG_PREALLOC|XFS_XFLAG_HASATTR )

/*
 * Structures returned from xfs_inumbers syssgi routine.
 */
typedef struct xfs_inogrp
{
	ino64_t		xi_startino;	/* starting inode number */
	int		xi_alloccount;	/* count of bits set in allocmask */
	__uint64_t	xi_allocmask;	/* mask of allocated inodes */
} xfs_inogrp_t;

#ifdef _KERNEL
/*
 * Prototypes for visible xfs_itable.c routines.
 */

/*
 * Convert file descriptor of a file in the filesystem to
 * a mount structure pointer.
 */
int					/* error status */
xfs_fd_to_mp(
	int			fd,	/* file descriptor */
	int			wperm,	/* need write perm on device fd */
	struct xfs_mount	**mpp,	/* output: mount structure pointer */
	int			rperm);	/* need root perm on file fd */

/*
 * Syssgi interface for bulkstat and inode-table.
 */
int					/* error status */
xfs_itable(
	int		opc,		/* op code */
	int		fd,		/* file descriptor of file in fs. */
	void		*lastip,	/* last inode number pointer */
	int		icount,		/* count of entries in buffer */
	void		*ubuffer,	/* buffer with inode descriptions */
	void		*ocount);	/* output count */

/* 
 * xfs_bulkstat() is used to fill in xfs_bstat structures as well as dm_stat
 * structures (by the dmi library). This is a pointer to a formatter function
 * that will iget the inode and fill in the appropriate structure.
 * see xfs_bulkstat_one() and dm_bulkstat_one() in dmi_xfs.c
 */
typedef int (*bulkstat_one_pf)(struct xfs_mount	*mp, 
			       struct xfs_trans	*tp,
			       xfs_ino_t   	ino,
			       void	     	*buffer,
			       daddr_t		bno,
			       void		*dip,
			       int		*stat);
/*
 * Values for stat return value.
 */
#define	BULKSTAT_RV_NOTHING	0
#define	BULKSTAT_RV_DIDONE	1
#define	BULKSTAT_RV_GIVEUP	2

/*
 * Values for bulkstat flag argument.
 */
#define	BULKSTAT_FG_IGET	0	/* Go through the buffer cache */
#define	BULKSTAT_FG_QUICK	1	/* No iget, walk the dinode cluster */

/*
 * Return stat information in bulk (by-inode) for the filesystem.
 */
int					/* error status */
xfs_bulkstat(
	struct xfs_mount	*mp,	/* mount point for filesystem */
	struct xfs_trans	*tp,	/* transaction pointer */
	ino64_t		*lastino,	/* last inode returned */
	int		*count,		/* size of buffer/count returned */
	bulkstat_one_pf formatter,	/* func that'd fill a single buf */
	size_t		statstruct_size,/* sizeof struct that we're filling */
	caddr_t		ubuffer,	/* buffer with inode stats */
	int		flag,		/* flag to control access method */
	int		*done);		/* 1 if there're more stats to get */

#endif	/* _KERNEL */

#endif	/* !_FS_XFS_ITABLE_H */
