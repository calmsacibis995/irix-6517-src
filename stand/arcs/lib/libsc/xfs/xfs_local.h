#ident "$Revision: 1.3 $"

/*
 * xfs_local.h
 *	XFS stand alone library data structures.
 *
 */

typedef struct xfs_info {
	xfs_mount_t	*xfs_mp;
	xfs_inode_t	*xfs_ip;
} xfs_info_t;

typedef struct extent {
	daddr_t	ex_bn;
	int	ex_length;
	long	ex_offset;
} extent;

typedef struct xfs_dirent {
	xfs_ino_t	ino;
	int		namelen;
	char		name[MAXNAMELEN];
} xfs_dirent_t;

#define fsb_to_mount(fsb)	(((xfs_info_t *)((fsb)->FsPtr))->xfs_mp)
#define fsb_to_inode(fsb)	(((xfs_info_t *)((fsb)->FsPtr))->xfs_ip)

#define	MAX_XFS_DMA	(64 * 1024)	/* 4MB should work but O2000 fails */
