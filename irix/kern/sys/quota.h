/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_QUOTA_H__
#define __SYS_QUOTA_H__

#ident "$Revision: 1.18 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/sema.h>
/*
 * The following constants define the default amount of time given a user
 * before the soft limits are treated as hard limits (usually resulting
 * in an allocation failure). These may be  modified by the quotactl
 * system call with the Q_SETQLIM/Q_XSETQLIM commands.
 */

#define	DQ_FTIMELIMIT	(7 * 24*60*60)		/* 1 week */
#define	DQ_BTIMELIMIT	(7 * 24*60*60)		/* 1 week */

/*
 * Definitions for the 'quotactl' system call for file system quotas.
 */
#ifndef _KERNEL
extern int quotactl(int, caddr_t , int, caddr_t);
#endif /* !_KERNEL */

/*
 * Old EFS style flags. These are still supported, but hopefully will never be
 * actually used..
 */
#define Q_QUOTAON	1	/* turn quotas on */
#define Q_QUOTAOFF	2	/* turn quotas off */
#define Q_SETQUOTA	3	/* set disk limits & usage */
#define Q_GETQUOTA	4	/* get disk limits & usage */
#define Q_SETQLIM	5	/* set disk limits only */
#define Q_SYNC		6	/* update disk copy of quota usages */
#define Q_ACTIVATE	7	/* update quotas without enabling */

/*
 * These flags take and return fs_disk_quota structures intead of the
 * old dqblk_t's.
 */
#define Q_XGETQUOTA	11	/* get disk limits & usage */
#define Q_XGETPQUOTA	12	/* get disk limits & usage for project quota */
#define Q_XSETQLIM	13	/* set disk limits only */
#define Q_XSETPQLIM	14	/* set disk limits only for project quota */
	
#define Q_GETPQUOTA	15	/* Old style support for project quotas */
#define Q_SETPQLIM	16	/* ditto */

/*
 * Extended disk quota features supported only by XFS.
 */
#define Q_GETQSTAT	20	/* returns fs_quota_stat_t struct */
#define Q_QUOTARM	21	/* free space taken by quotas:"rm quota file(s)" */
#define Q_WARN		22	/* add a warning to a user if quota exceeded */
#define Q_BULKQSTAT	23	/* get a buffer full of active dquots */
	
/*
 * fs_disk_quota structure:
 *
 * This contains all the current quota information regarding a user or project.
 * It is 64-bit aligned, and all the blk units are in BBs (Basic Blocks) of
 * 512 bytes. Both EFS and XFS recognize this structure, and as long as the
 * Q_X* flags above are used.
 */
#define FS_DQUOT_VERSION	0x01	/* d_version to distinguish future 
					   changes to the structure */
typedef struct  fs_disk_quota {
	int8_t		d_version;	/* version of this structure */
	int8_t		d_flags;	/* type - XFS_{USER, PROJ}_QUOTA */
	u_int16_t	d_fieldmask;	/* field specifier */
	__uint32_t	d_id;		/* user id or proj id */
	__uint64_t	d_blk_hardlimit;/* absolute limit on disk blks */
	__uint64_t	d_blk_softlimit;/* preferred limit on disk blks */
	__uint64_t	d_ino_hardlimit;/* maximum # allocated inodes */
	__uint64_t	d_ino_softlimit;/* preferred inode limit */
	__uint64_t	d_bcount;	/* # disk blocks owned by the user */
	__uint64_t	d_icount;	/* # inodes owned by the user */
	__int32_t	d_itimer;	/* zero if within inode limits if not, 
					   this is when we refuse service */
	__int32_t	d_btimer;	/* similar to above; for disk blocks */
	u_int16_t  	d_iwarns;       /* # warnings issued wrt num inodes */
	u_int16_t  	d_bwarns;       /* # warnings issued wrt disk blocks */
	int32_t		d_padding2;	/* padding2 - for future use */
	__uint64_t	d_rtb_hardlimit;/* absolute limit on realtime blks */
	__uint64_t	d_rtb_softlimit;/* preferred limit on RT disk blks */
	__uint64_t	d_rtbcount;	/* # realtime blocks owned */
	__int32_t	d_rtbtimer;	/* similar to above; for RT disk blks */
	u_int16_t  	d_rtbwarns;     /* # warnings issued wrt RT disk blks */
	int16_t		d_padding3;	/* padding3 - for future use */	
	int8_t		d_padding4[8];	/* yet more padding */
} fs_disk_quota_t;

/*
 * These fields are sent to Q_XSETQLIM to specify the fields that need to change.
 */
#define FS_DQ_ISOFT	0x0001
#define FS_DQ_IHARD	0x0002
#define FS_DQ_BSOFT	0x0004
#define FS_DQ_BHARD 	0x0008
#define FS_DQ_RTBSOFT	0x0010
#define FS_DQ_RTBHARD	0x0020
#define FS_DQ_LIMIT_MASK	(FS_DQ_ISOFT | FS_DQ_IHARD | FS_DQ_BSOFT | \
				 FS_DQ_BHARD | FS_DQ_RTBSOFT | FS_DQ_RTBHARD)
/*
 * These timers can only be set in super user's dquot. For others, timers are
 * automatically started and stopped. Superusers timer values set the limits for
 * the rest. In case these values are zero, the DQ_*TIMELIMIT values defined 
 * above are used. 
 * These values also apply only to the d_fieldmask field for Q_XSETQLIM.
 */
#define FS_DQ_BTIMER	0x0040
#define FS_DQ_ITIMER	0x0080
#define FS_DQ_RTBTIMER 	0x0100
#define FS_DQ_TIMER_MASK	(FS_DQ_BTIMER | FS_DQ_ITIMER | FS_DQ_RTBTIMER)
	 
/*
 * Various flags needed for quotactl syscall. Only relevant to XFS filesystems.
 */
#define XFS_QUOTA_UDQ_ACCT	0x0001  /* user quota accounting */
#define XFS_QUOTA_UDQ_ENFD	0x0002  /* user quota limits enforcement */
#define XFS_QUOTA_PDQ_ACCT	0x0004  /* project quota accounting */
#define XFS_QUOTA_PDQ_ENFD	0x0008  /* proj quota limits enforcement */
#define XFS_QUOTA_ALL		(XFS_QUOTA_UDQ_ACCT|XFS_QUOTA_UDQ_ENFD| \
				 XFS_QUOTA_PDQ_ACCT|XFS_QUOTA_PDQ_ENFD)

#define XFS_USER_QUOTA		0x0001	/* user quota type */
#define XFS_PROJ_QUOTA		0x0002	/* project quota type */

/*
 * fs_quota_stat is the structure returned in Q_GETQSTAT for a given file system.
 * It provides a centralized way to get meta infomation about the quota subsystem.
 * eg. disk space taken up for user and project quotas, number of dquots currently
 * incore.
 */
#define FS_QSTAT_VERSION	0x01	/* qs_version for fs_quota_stat */

/*
 * Some basic infomation about 'quota files'.
 */
typedef struct fs_qfilestat {
	ino64_t		qfs_ino;	/* inode number */
	__uint64_t	qfs_nblks;	/* number of BBs 512-byte-blks */
	__uint32_t	qfs_nextents;	/* number of extents */
} fs_qfilestat_t;
	
typedef struct fs_quota_stat {
	int8_t		qs_version;	/* version to distinguish future changes */
	u_int16_t	qs_flags;	/* XFS_QUOTA_*DQ_ACCT, ENFD */
	int8_t		qs_pad;		/* unused */
	fs_qfilestat_t	qs_uquota;	/* user quota storage information */
	fs_qfilestat_t	qs_pquota;	/* project quota storage information */
	__uint32_t	qs_incoredqs;	/* number of dquots incore */
	__int32_t	qs_btimelimit;  /* limit for blks timer */	
	__int32_t	qs_itimelimit;  /* limit for inodes timer */	
	__int32_t	qs_rtbtimelimit;/* limit for rt blks timer */	
	u_int16_t	qs_bwarnlimit;	/* limit for num warnings */
	u_int16_t	qs_iwarnlimit;	/* limit for num warnings */
} fs_quota_stat_t;

/*
 * Q_BULKQSTAT is a mechanism to iterate over all active quota entries
 * in a filesystem. It takes in the following structure as an argument,
 * and fills in the buffer pointed to by bqs_buflen.
 */
#define FS_BQSTAT_VERSION	0x01	/* bqs_version for fs_bulkqstat */
typedef struct fs_bulkqstat {
	int8_t		bqs_version;
	u_int16_t	bqs_flags;	/* XFS_USER_QUOTA or XFS_PROJ_QUOTA */
	int8_t		bqs_status;	/* return status: 0 done, 1 otherwise */
	__uint32_t	bqs_pad;	/* filler, unused */
	__uint32_t	bqs_id;		/* starting user id or proj id */
	__uint32_t	bqs_buflen;	/* size of the buffer passed in */
	fs_disk_quota_t	*bqs_buf;	/* buffer of quota structs to fill in */
} fs_bulkqstat_t;


/*----------------- EFS SPECIFIC QUOTA INFORMATION  -------------------*/

/*
 * There is one quota file per file system and the dqblk structure
 * defines the format of an entry for one user in the disk quota file
 * (as it appears on disk). This index into the file for a particular uid
 * is calculated by qt_dqoff(). This had to change from Berkeley/Sun's 
 * implementation since EFS does not support holes in files.
 * In the kernel, we keep track of the quotas in basic blocks (512 bytes),
 * All user commands obtain numbers in bbs from the kernel, but display 
 * and manipulate figures in kbytes.
 */

struct	dqblk {
	__uint32_t	dqb_bhardlimit;	/* absolute limit on disk blks alloc */
	__uint32_t	dqb_bsoftlimit;	/* preferred limit on disk blks */
	__uint32_t	dqb_curblocks;	/* current block count */
	__uint32_t	dqb_fhardlimit;	/* maximum # allocated files + 1 */
	__uint32_t	dqb_fsoftlimit;	/* preferred file limit */
	__uint32_t	dqb_curfiles;	/* current # allocated files */
	__uint32_t	dqb_btimelimit;	/* timer when usage exceeds disk blks limit */
	__uint32_t	dqb_ftimelimit;	/* timer when usage exceeds file limit */
};

/*
 * The quota file has a header, followed by an index
 * (list of uids), followed by the on-disk quota information which is
 * kept for those users that have quotas. The index size lies in the range
 * Q_MININDEX-Q_MAXINDEX. The header has a magic number, followed by the
 * quota file index size and then the number of users whose uids appear
 * in the index.
 */
struct qt_header {
	u_short qh_magic;	/* quota file magic number */
	u_short qh_index;	/* quota file index size */
	u_short qh_nusers;	/* number of users in this quota file */
	u_short qh_uid[1];	/* list of uid's. 0 is always the first one */
};
#define Q_HEADER	(sizeof(struct qt_header)-sizeof(u_short))
#define Q_MININDEX	4096		/* min index size for quotas file */
#define Q_MAXINDEX	4096*10		/* max index size for quotas file */
#define Q_MAGIC		0x8765		/* magic number */
#define Q_MAXUSERS	((Q_MAXINDEX - Q_HEADER)/sizeof(u_short))

#ifdef _KERNEL
/*
 * The incore dquot structure records disk usage for a user on a filesystem.
 * A cache is kept of recently used entries. Active inodes with quotas
 * associated with them have a pointer to a incore dquot structure.
 */
struct	dquot {
	struct	dquot *dq_forw, *dq_back;/* hash list, MUST be first entry */
	struct	dquot *dq_freef, *dq_freeb; /* free list */
	u_short	dq_flags;
#define	DQ_MOD		0x01	/* this quota modified since read */
	u_short	dq_cnt;		/* count of active references */
	u_short	dq_uid;		/* user this applies to */
	off_t dq_off;		/* offset into quotas file */
	struct mount *dq_mp;	/* filesystem this relates to */
	struct dqblk dq_dqb;	/* actual usage & quotas */
	mutex_t dq_sema;	/* mutex for quota lock */
};

#define	dq_bhardlimit	dq_dqb.dqb_bhardlimit
#define	dq_bsoftlimit	dq_dqb.dqb_bsoftlimit
#define	dq_curblocks	dq_dqb.dqb_curblocks
#define	dq_fhardlimit	dq_dqb.dqb_fhardlimit
#define	dq_fsoftlimit	dq_dqb.dqb_fsoftlimit
#define	dq_curfiles	dq_dqb.dqb_curfiles
#define	dq_btimelimit	dq_dqb.dqb_btimelimit
#define	dq_ftimelimit	dq_dqb.dqb_ftimelimit

struct inode;
struct mount;

extern struct dquot *qt_getinoquota(struct inode *ip);
extern int qt_chkdq(struct inode *ip, long change, int force, int *spaceleft);
extern int qt_chkiq(struct mount *mp, struct inode *ip, u_int uid, int force);
extern void qt_dqrele(struct dquot *dqp);
extern int qt_closedq(struct mount *mp, int umounting);
extern void qt_dqput(struct dquot *dqp);
extern void qt_dqupdate(struct dquot *dqp);
extern void qt_dqinval(struct dquot *dqp);
extern int qt_getdiskquota(u_int uid, struct mount *mp, int allocate,
	struct dquot **dqpp);

extern struct dquot dqfreelist;		/* start of the free list */
extern struct dquot *dquot;		/* start of the incore quota table */

#define qfree_sema (dqfreelist.dq_sema)

#define DQLOCK(dqp) mutex_lock(&(dqp)->dq_sema, PZERO)

#define DQUNLOCK(dqp) { \
	ASSERT(mutex_owner(&(dqp)->dq_sema)); \
	mutex_unlock(&(dqp)->dq_sema); \
}

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_QUOTA_H__ */
