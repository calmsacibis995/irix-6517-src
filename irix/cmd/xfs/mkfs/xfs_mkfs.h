#ifndef _CMD_XFS_MKFS_XFS_MKFS_H
#define	_CMD_XFS_MKFS_XFS_MKFS_H

#ident	"$Revision: 1.10 $"

#define	XFS_DFL_BLOCKSIZE_LOG	12		/* 4096 byte blocks */
#define	XFS_DINODE_DFL_LOG	8		/* 256 byte inodes */
#define	XFS_MIN_DATA_BLOCKS	100
#define	XFS_MIN_INODE_PERBLOCK	2		/* min inodes per block */
#define	XFS_DFL_IMAXIMUM_PCT	25		/* max % of space for inodes */
#define	XFS_IFLAG_ALIGN		1		/* -i align defaults on */
#define	XFS_MIN_REC_DIRSIZE	12		/* 4096 byte dirblocks (V2) */
#define	XFS_DFL_DIR_VERSION	1		/* default directory version */
#define	XFS_DFL_LOG_SIZE	1000		/* default log size, blocks */
#define	XFS_MIN_LOG_FACTOR	3		/* min log size factor */
#define	XFS_DFL_LOG_FACTOR	16		/* default log size, factor */
						/* with max trans reservation */

/*
 * Nonsim function prototypes
 */
extern int growfile(int fd, long long size);
extern long filesize(int fd);

#endif	/* _CMD_XFS_MKFS_XFS_MKFS_H */
