#ifndef _FS_XFS_IMAP_H
#define	_FS_XFS_IMAP_H

#ident "$Revision: 1.3 $"

struct xfs_mount;
struct xfs_trans;

/*
 * This is the structure passed to xfs_imap() to map
 * an inode number to its on disk location.
 */
typedef struct xfs_imap {
	daddr_t		im_blkno;	/* starting BB of inode chunk */
	uint		im_len;		/* length in BBs of inode chunk */
	xfs_agblock_t	im_agblkno;	/* logical block of inode chunk in ag */
	ushort		im_ioffset;	/* inode offset in block in "inodes" */
	ushort		im_boffset;	/* inode offset in block in bytes */
} xfs_imap_t;
	
int	xfs_imap(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
		 xfs_imap_t *, uint);

#endif	/* !_FS_XFS_IMAP_H */
