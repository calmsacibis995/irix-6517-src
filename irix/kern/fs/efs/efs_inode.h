#ifndef __EFS_INODE_H__
#define	__EFS_INODE_H__
/*
 * The in-core EFS inode structure.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 * $Revision: 3.38 $
 */
#ifdef _KERNEL
#include <sys/sema.h>
#include "efs_ino.h"		/* disk inode and extent structs */

/*
 * Version of a regular, directory, or symbolic link EFS inode as it appears
 * in memory.  This is a template for the actual data structure that contains
 * i_numextents worth of i_extents.
 */
struct inode {
	/* hashing, linking, and numbering */
	struct ihash	*i_hash;	/* pointer to hash chain header */
	struct inode	*i_next;	/* inode hash chain linkage */
	struct inode	**i_prevp;	/* pointer to previous i_next */
	struct mount	*i_mount;	/* pointer to containing mount */
	struct inode	*i_mnext;	/* next inode in mount's list */
	struct inode	**i_mprevp;	/* pointer to previous i_mnext */
	struct bhv_desc i_bhv_desc;	/* inode behavior descriptor */
	efs_ino_t	i_number;	/* inode number on device */
	dev_t		i_dev;		/* device containing this inode */

	/* locking and miscellaneous state */
	mutex_t		i_lock;		/* mutual exclusion lock */
	uint64_t	i_lockid;	/* id of thread owning i_lock */
	short		i_locktrips;	/* number of lock reacquisitions */
	efs_daddr_t	i_nextbn;	/* next block in sequential read */
	efs_daddr_t	i_reada;	/* trigger readahead at this block */
	struct dquot	*i_dquot;	/* quota structure for this file */
	u_long		i_vcode;	/* version code token (for RFS) */
	u_short		i_flags;	/* random flags -- see below */
	scoff_t		i_remember;	/* i_size which is perm on disk */

	/* internalized efs_dinode attributes */
	scoff_t		i_size;		/* size of file */
	u_short		i_mode;		/* file type and protection mode */
	short		i_nlink;	/* number of directory entries */
	uid_t		i_uid;		/* owner identity */
	gid_t		i_gid;		/* group identity */
	time_t		i_atime;	/* time last accessed */
	time_t		i_mtime;	/* time last modified */
	time_t		i_ctime;	/* time created or changed */
	int		i_updtimes;	/* update to a time required */
	__uint32_t	i_gen;		/* generation number (for NFS) */

	/* internalized efs_dinode extent info */
	short		i_numextents;	/* number of extents */
	short		i_numindirs;	/* number of indirect extents */
	size_t		i_directbytes;	/* bytes in direct extents */
	size_t		i_indirbytes;	/* bytes in indir extents */
	extent		*i_extents;	/* pointer to direct extents */
	extent		*i_indir;	/* malloc'd indirect extent info */
	long		i_blocks;	/* number of direct extent blocks */
	extent		i_direct[4];	/* inline direct extent space */
	dev_t		i_rdev;		/* device number if special */
	void		*i_afs;		/* pointer to AFS specific stuff */
	u_char		i_version;	/* inode version AFS */
	u_char		i_spare;	/* copy of di_spare AFS */
	time_t		i_mod;		/* time inode last changed */
	__int32_t	i_umtime;	/* nsecs of time last modified */
};

#define ISLINK(type) (((type)==IFLNK) || \
			((type)==IFCHRLNK) || \
			((type)==IFBLKLNK))


/*
 * Maximum file size for an EFS file.
 */
#define	EFS_MAX_FILE_OFFSET	0x7fffffff

/*
 * Inode flags.
 */
#define	IUPD		0x0001		/* file has been modified */
#define	IACC		0x0002		/* inode access time to be updated */
#define	ICHG		0x0004		/* inode has been changed */
#define	ISYN		0x0008		/* do synchronous write for iupdat */
#define	INODELAY 	0x0010		/* don't do delayed write in iupdat */
#define	IRWLOCK		0x0020		/* VOP_RWLOCK has been called */
#define	ITRUNC		0x0040		/* inode may need truncation */
#define	IINCORE		0x0080		/* indirect extents in core */
#define IMOD		0x0100		/* inode has been modified */
#define INOUPDAT	0x0200		/* track inode changes for fsr */

/* flag for iflush */
#define FLUSH_ALL	1		/* flush all inactive inodes even if
					 * some are active
					 */

/*
 * Convenient macros.
 */
#define	itov(ip)	BHV_TO_VNODE(itobhv(ip))
#define	itom(ip)	((ip)->i_mount)
#define	itobhv(ip)	(&((ip)->i_bhv_desc))
#define	bhvtoi(bhvp)	((struct inode *)(BHV_PDATA(bhvp)))

#define	ihold(ip)	vn_hold(itov(ip))
#define	irele(ip)	vn_rele(itov(ip))
#define	itovfs(ip)	(itov(ip)->v_vfsp)
#define	itoefs(ip)	mtoefs(itom(ip))

/*
 * EFS file identifier.
 */
struct efid {
	u_short		efid_len;	/* length of remainder */
	u_short		efid_pad;	/* padding, must be zero */
	__uint32_t	efid_ino;	/* inode number */
	__uint32_t	efid_gen;	/* generation number */
};


struct bmapval;
struct cred;
struct efs;
struct mount;

/* generic in-core inode functions */
extern void	ihashinit(void);
extern int	iget(struct mount *, efs_ino_t, struct inode **);
extern void	irealloc(struct inode *, size_t);
extern void	iput(struct inode *);
extern void	idrop(struct inode *);
extern void	iinactive(struct inode *);
extern void	ireclaim(struct inode *);
extern int	iflush(struct mount *, int);
extern void	ilock(struct inode *);
extern int	ilock_nowait(struct inode *);
extern void	iunlock(struct inode *);

/* EFS-specific inode operations */
extern int	efs_bmap(bhv_desc_t *, off_t, ssize_t, int, struct cred *,
			 struct bmapval *, int *);
extern int	efs_iaccess(struct inode *, mode_t, struct cred *);
extern int	efs_ialloc(struct inode *, mode_t, short, dev_t,
			   struct inode **, struct cred *);
extern int	efs_icreate(struct inode *, mode_t, short, dev_t,
			    struct inode *, struct cred *);
extern int	efs_ichange_type(struct inode *, u_short);
extern void	efs_idestroy(struct inode *);
extern void	efs_ifree(struct inode *ip);
extern int	efs_imap(struct efs *, efs_ino_t, struct bmapval *);
extern int	efs_iread(struct mount *, efs_ino_t, struct inode **);
extern int	efs_itrunc(struct inode *, scoff_t, int);
extern int	efs_iupdat(struct inode *);
extern int	efs_readindir(struct inode *);

extern struct zone *efs_inode_zone;
extern struct zone *efs_extent_zones[];
extern extent	*efs_alloc_extent(int);
extern extent	*efs_realloc_extent(extent *, int, int);
extern void	efs_free_extent(extent *, int);

extern void	efs_inode_check(struct inode *, char *);
extern int	doexlist_trash;	/* extra inode checking to find a bug */

extern struct vnodeops efs_vnodeops;

#endif	/* _KERNEL */
#endif	/* __EFS_INODE_H__ */
