/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__EFS_INO_
#define	__EFS_INO_

#ident "$Revision: 3.27 $"

/*
 * Definitions for the on-volume version of extent filesystem inodes.
 * Inodes consist of some number of extents, as contained in
 * di_numextents.  When di_numextents exceeds EFS_DIRECTEXTENTS, then the
 * extents are kept elsewhere.  ``Elsewhere'' is defined as a list of up
 * to EFS_DIRECTEXTENTS contiguous blocks of indirect extents.  An
 * indirect extent is an extent descriptor (bn,len) which points to the
 * disk blocks holding the actual extents.  For direct extents, the
 * ex_offset field contains the logical offset into the file that the
 * extent covers.  For indirect extents the field
 * di_u.di_extents[0].ex_offset contains the number of indirect extents.
 *
 * If the ex_bn is less than fs_firstcg, or greater than or equal to the
 * fs_size, then the block # is out of range.  If the ex_length is zero
 * or larger than EFS_MAXEXTENTLEN, then the extent is bad.
 * If the (ex_offset, ex_length) tuple overlaps any other extent, then
 * the extent is bad.
 */

/*
 * An efs block number.
 */
typedef	__int32_t	efs_daddr_t;

/*
 * An efs inode number.
 */
typedef __uint32_t	efs_ino_t;

/*
 * Layout of an extent, in memory and on disk.
 * This structure is laid out to take exactly 8 bytes.
 */
typedef struct	extent {
unsigned int	ex_magic:8,	/* magic # (MUST BE ZERO) */
		ex_bn:24,	/* basic block # */
		ex_length:8,	/* length of this extent, in bb's */
		ex_offset:24;	/* logical bb offset into file */
} extent;

/* # of directly mappable extents (also in fact # of possible indirect
 * extents since these live in the direct extent table). 
 */
#define	EFS_DIRECTEXTENTS	12

/* The inode code expects to be able to handle indirect extents in ONE
 * buffer. So impose a limit on the size of an indirect extent. 
 * (Note this is a first minimum-change hack for long files. We'll
 * probably remove the one-buffer restriction later, in which case
 * EFS_MAXINDIRBBS can get larger).
 */

#define EFS_MAXINDIRBBS 64

/*
 * Therefore follows as the night the day a computable number for the
 * maximum number of extents possible for an inode.  Unfortunately,
 * since i_numextents is a signed short, the real value for numextents
 * is MIN(32767, ((EFS_DIRECTEXTENTS * EFS_MAXINDIRBBS * BBSIZE) / 
 *		  sizeof(struct extent))
 * In an ideal world, we would change numextents to an unsigned short,
 * but we're a little pressed for time at this point, so we're just
 * going to leave it.  If you decide to change the type of numextents,
 * you should check fsck and its ilk to ensure that they do the right thing. 
 */

#define	EFS_MAXEXTENTS   32767

/* # of inodes per page */
#define	EFS_INODESPERPAGE	(NBPC / sizeof(struct efs_dinode))

/* maximum length of a single extent */
#define	EFS_MAXEXTENTLEN	(256 - 8)

/* with the advent of extended dev_t, we need to interoperate with on-disk
 * dev inodes created by earlier kernels. We modify the union so that
 * an o_dev_t field sits where the older kernels expect it, and add
 * a new (extended) dev_t field. 
 */

struct edevs	{
	o_dev_t	odev;
	dev_t	ndev;
	};

/*
 * Extent based filesystem inode as it appears on disk.  The efs inode
 * is exactly 128 bytes long.
 */
struct	efs_dinode {
	u_short		di_mode;	/* mode and type of file */
	short		di_nlink;	/* number of links to file */
	u_short		di_uid;		/* owner's user id */
	u_short		di_gid;		/* owner's group id */
	__int32_t	di_size;	/* number of bytes in file */
	__int32_t	di_atime;	/* time last accessed */
	__int32_t	di_mtime;	/* time last modified */
	__int32_t	di_ctime;	/* time created */
	__uint32_t	di_gen;		/* generation number */
	short		di_numextents;	/* # of extents */
	u_char		di_version;	/* version of inode */
	u_char		di_spare;	/* spare - used by AFS */
	union di_addr {
		extent	di_extents[EFS_DIRECTEXTENTS];
		struct edevs	di_dev;	/* device for IFCHR/IFBLK */
	} di_u;
};

#define EFS_MAX_INLINE	(EFS_DIRECTEXTENTS * sizeof(extent))

/* sizeof (struct efs_dinode), log2 */
#define	EFS_EFSINOSHIFT		7

/*
 * File types (inode formats).
 * These are shared by the disk and incore inode.
 */
#define	IFMT		0170000		/* type of file */
#define	IFIFO		0010000		/* named pipe (fifo) */
#define	IFCHR		0020000		/* character special */
#define IFCHRLNK	0030000		/* character special link */
#define	IFDIR		0040000		/* directory */
#define	IFBLK		0060000		/* block special */
#define	IFBLKLNK	0070000		/* block special link */
#define	IFREG		0100000		/* regular */
#define	IFLNK		0120000		/* symbolic link */
#define	IFSOCK		0140000		/* socket */

/* 
 * Convert from EFS mode to standard stat mode's.
 */
#define EFSTOSTAT(type) (((type)==IFCHRLNK) ? IFCHR : \
			(((type)==IFBLKLNK) ? IFBLK : (type)))

#define SET_ITYPE(ip, type) ip->i_mode = ((((ip)->i_mode) & ~IFMT) | (type))

/*
 * File execution and access modes.
 * These are shared by the disk and incore inode.
 */
#define	ISUID		04000		/* set user id on execution */
#define	ISGID		02000		/* set group id on execution */
#define	ISVTX		01000		/* sticky directory (old: save text) */
#define	IREAD		0400		/* read, write, execute permissions */
#define	IWRITE		0200
#define	IEXEC		0100

/*
 * Versions of inode
 */
#define EFS_IVER_EFS		0	/* EFS inode */
#define EFS_IVER_AFSSPEC	1	/* AFS SPECIAL inode */
#define EFS_IVER_AFSINO		2	/* AFS normal inode */

/*
 * NOTE: AFS inodes use the following fields
 *
 * We use the 12 8-bit unused ex_magic fields!
 * Plus 2 values of di_version
 * di_version = 0 - current EFS
 *          1 - AFS INODESPECIAL
 *          2 - AFS inode
 * AFS inode:
 * magic[0-3] - VOLid
 * magic[4-6] - vnode number (24 bits)
 * magic[7-9] - disk uniqifier (24 bits)
 * magic[10-11]+di_spare - data version (24 bits)
 *
 * INODESPECIAL:
 * magic[0-3] - VOLid
 * magic[4-7] - parent
 * magic[8]   - type
 */
#endif	/* __EFS_INO_ */
