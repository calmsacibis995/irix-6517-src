#ifndef _FS_XFS_GROW_H
#define	_FS_XFS_GROW_H

#ident	"$Revision: 1.11 $"

/*
 * File system growth interfaces
 */

#define	XFS_FS_GEOMETRY_V1	0	/* get filesystem geometry pre-irix6.5*/
#define	XFS_GROWFS_DATA		1	/* grow data area */
#define	XFS_GROWFS_LOG		2	/* grow log, new log is internal */
#define	XFS_GROWFS_RT		3	/* grow realtime area */
#define	XFS_FS_COUNTS		4	/* get filesystem dynamic counts */
#define	XFS_SET_RESBLKS		5	/* set reserved block count */
#define	XFS_GET_RESBLKS		6	/* get reserved block counts */
#define	XFS_FS_GEOMETRY_V2	7	/* get filesystem geometry irix6.5 */
#define	XFS_FS_GEOMETRY		8	/* get filesystem geometry>=irix6.5.3 */
#define	XFS_FSOPS_COUNT		9	/* count of operations */

/*
 * Minimum and maximum sizes need for growth checks
 */
#define	XFS_MIN_LOG_BLOCKS	512
#define	XFS_MIN_LOG_BYTES	(256 * 1024)
#define	XFS_MIN_AG_BLOCKS	64
#define	XFS_MAX_LOG_BLOCKS	(64 * 1024)
#define	XFS_MAX_LOG_BYTES	(128 * 1024 * 1024)

/*
 * Input and output structures
 */

/*
 * Output for XFS_FS_GEOMETRY
 */
typedef struct xfs_fsop_geom
{
	__uint32_t	blocksize;	/* filesystem (data) block size */
	__uint32_t	rtextsize;	/* realtime extent size */
	__uint32_t	agblocks;	/* fsblocks in an allocation group */
	__uint32_t	agcount;	/* number of allocation groups */
	__uint32_t	logblocks;	/* fsblocks in the log */
	__uint32_t	sectsize;	/* (data) sector size, bytes */
	__uint32_t	inodesize;	/* inode size in bytes */
	__uint32_t	imaxpct;	/* max allowed space for inodes (%) */
	__uint64_t	datablocks;	/* fsblocks in the data subvolume */
	__uint64_t	rtblocks;	/* fsblocks in the realtime subvolume */
	__uint64_t	rtextents;	/* rt extents in the realtime subvol */
	__uint64_t	logstart;	/* starting fsblock of the log */
	uuid_t		uuid;		/* unique id of the filesystem */
	__uint32_t	sunit;		/* stripe unit, fsblocks */
	__uint32_t	swidth;		/* stripe width, fsblocks */
	__int32_t	version;	/* structure version */
	__uint32_t	flags;		/* superblock version flags */
	__uint32_t	logsectsize;	/* log sector size, bytes */
	__uint32_t	rtsectsize;	/* realtime sector size, bytes */
	__uint32_t	dirblocksize;	/* directory block size, bytes */
} xfs_fsop_geom_t;
#define	XFS_FSOP_GEOM_VERSION	0

#define	XFS_FSOP_GEOM_FLAGS_ATTR	0x01	/* attributes in use */
#define	XFS_FSOP_GEOM_FLAGS_NLINK	0x02	/* 32-bit nlink values */
#define	XFS_FSOP_GEOM_FLAGS_QUOTA	0x04	/* quota accounting enabled */
#define	XFS_FSOP_GEOM_FLAGS_IALIGN	0x08	/* inode alignment */
#define	XFS_FSOP_GEOM_FLAGS_DALIGN	0x10	/* large data alignment */
#define	XFS_FSOP_GEOM_FLAGS_SHARED	0x20	/* read-only shared */
#define	XFS_FSOP_GEOM_FLAGS_EXTFLG	0x40	/* special extent flag */
#define	XFS_FSOP_GEOM_FLAGS_DIRV2	0x80	/* directory version 2 */

/*
 * This version has two new fields sunit and swidth. 
 * Added in irix6.5
 */
typedef struct xfs_fsop_geom_v2
{
	__uint32_t	blocksize;	/* filesystem (data) block size */
	__uint32_t	rtextsize;	/* realtime extent size */
	__uint32_t	agblocks;	/* fsblocks in an allocation group */
	__uint32_t	agcount;	/* number of allocation groups */
	__uint32_t	logblocks;	/* fsblocks in the log */
	__uint32_t	sectsize;	/* (data) sector size, bytes */
	__uint32_t	inodesize;	/* inode size in bytes */
	__uint32_t	imaxpct;	/* max allowed space for inodes (%) */
	__uint64_t	datablocks;	/* fsblocks in the data subvolume */
	__uint64_t	rtblocks;	/* fsblocks in the realtime subvolume */
	__uint64_t	rtextents;	/* rt extents in the realtime subvol */
	__uint64_t	logstart;	/* starting fsblock of the log */
	uuid_t		uuid;		/* unique id of the filesystem */
	__uint32_t	sunit;		/* stripe unit, fsblocks */
	__uint32_t	swidth;		/* stripe width, fsblocks */
} xfs_fsop_geom_v2_t;

/*
 * This version of xfs_fsop_geom existed prior to Irix 6.5
 */
typedef struct xfs_fsop_geom_v1
{
	__uint32_t	blocksize;	/* filesystem (data) block size */
	__uint32_t	rtextsize;	/* realtime extent size */
	__uint32_t	agblocks;	/* fsblocks in an allocation group */
	__uint32_t	agcount;	/* number of allocation groups */
	__uint32_t	logblocks;	/* fsblocks in the log */
	__uint32_t	sectsize;	/* (data) sector size, bytes */
	__uint32_t	inodesize;	/* inode size in bytes */
	__uint32_t	imaxpct;	/* max allowed space for inodes (%) */
	__uint64_t	datablocks;	/* fsblocks in the data subvolume */
	__uint64_t	rtblocks;	/* fsblocks in the realtime subvolume */
	__uint64_t	rtextents;	/* rt extents in the realtime subvol */
	__uint64_t	logstart;	/* starting fsblock of the log */
	uuid_t		uuid;		/* unique id of the filesystem */
} xfs_fsop_geom_v1_t;

/* Output for XFS_FS_COUNTS */
typedef struct xfs_fsop_counts
{
	__uint64_t	freedata;	/* free data section blocks */
	__uint64_t	freertx;	/* free rt extents */
	__uint64_t	freeino;	/* free inodes */
	__uint64_t	allocino;	/* total allocated inodes */
} xfs_fsop_counts_t;

/* Output for XFS_GET_RESBLKS */
typedef struct xfs_fsop_get_resblks
{
	__uint64_t  resblks;
	__uint64_t  resblks_avail;
} xfs_fsops_getblks_t;

/* Input for growfs data op */
typedef struct xfs_growfs_data
{
	__uint64_t	newblocks;	/* new data subvol size, fsblocks */
	__uint32_t	imaxpct;	/* new inode space percentage limit */
} xfs_growfs_data_t;

/* Input for growfs log op */
typedef struct xfs_growfs_log
{
	__uint32_t	newblocks;	/* new log size, fsblocks */
	__int32_t	isint;		/* 1 if new log is internal */
} xfs_growfs_log_t;

/* Input for growfs rt op */
typedef struct xfs_growfs_rt
{
	__uint64_t	newblocks;	/* new realtime size, fsblocks */
	__uint32_t	extsize;	/* new realtime extent size, fsblocks */
} xfs_growfs_rt_t;

#ifdef _KERNEL
int					/* error status */
xfs_fsoperations(
	int		fd,		/* file descriptor for fs */
	int		opcode,		/* operation code */
	void		*in,		/* input structure */
	void		*out);		/* output structure */
#endif	/* _KERNEL */

#endif	/* _FS_XFS_GROW_H */
