/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_KSA_H__
#define __SYS_KSA_H__
#ident	"$Revision: 3.27 $"
 
/*
 * structure ksa defines the data structure of system activity data file
 */
#include <sys/sysinfo.h>
#include <sys/dnlc.h>	/* for ncstats */

/*
 * structs defining private statistics
 */
/*
 * igetstats - stats from iget()/getinode()
 * Really just for efs
 */
struct igetstats {
	unsigned int	ig_attempts;	/* # calls to iget() */
	unsigned int	ig_found;	/* found in hash list */
	unsigned int	ig_frecycle;	/* found but was recycled before lock */
	unsigned int	ig_missed;	/* # times missed - alloc new */
	unsigned int	ig_dup;		/* someone else placed inode on
					 * list that we were
					 */
	unsigned int	ig_reclaims;	/* # calls to ireclaim */
	unsigned int	ig_itobp;	/* # calls to efs_itobp */
	unsigned int	ig_itobpf;	/* # calls to efs_itobp that
					 * found cached bp
					 */
	unsigned int	ig_iupdat;	/* # calls to efs_iupdat */
	unsigned int	ig_iupacc;	/* # calls to efs_iupdat for IACC */
	unsigned int	ig_iupupd;	/* # calls to efs_iupdat for IUPD */
	unsigned int	ig_iupchg;	/* # calls to efs_iupdat for ICHG */
	unsigned int	ig_iupmod;	/* # calls to efs_iupdat for IMOD */
	unsigned int	ig_iupunk;	/* # calls to efs_iupdat for ?? */
	unsigned int	ig_iallocrd;	/* # breads for ialloc */
	unsigned int	ig_iallocrdf;	/* # breads for ialloc in buf cache */
	unsigned int	ig_ialloccoll;	/* # times file create collided */
	unsigned int	ig_bmaprd;	/* bmap reads */
	unsigned int	ig_bmapfbm;	/* bmap reads found in bm cache */
	unsigned int	ig_bmapfbc;	/* bmap reads found in buf cache */
	unsigned int	ig_dirupd;	/* inode updated cause dir updated */
	unsigned int	ig_truncs;	/* # truncates that do something */
	unsigned int	ig_icreat;	/* # inode creations */
	unsigned int	ig_attrchg;	/* inode updated cause attrs chged */
	unsigned int	ig_readcancel;	/* reads canceled in efs_strategy */
};


/*
 * Per-processor xfs statistics.
 */
struct xfsstats
{
					/* extent allocation */
	unsigned int	xs_allocx;
	unsigned int	xs_allocb;
	unsigned int	xs_freex;
	unsigned int	xs_freeb;
					/* allocation btree */
	unsigned int	xs_abt_lookup;
	unsigned int	xs_abt_compare;
	unsigned int	xs_abt_insrec;
	unsigned int	xs_abt_delrec;
					/* block mapping */
	unsigned int	xs_blk_mapr;
	unsigned int	xs_blk_mapw;
	unsigned int	xs_blk_unmap;
	unsigned int	xs_add_exlist;
	unsigned int	xs_del_exlist;
	unsigned int	xs_look_exlist;
	unsigned int	xs_cmp_exlist;
					/* block map btree */
	unsigned int	xs_bmbt_lookup;
	unsigned int	xs_bmbt_compare;
	unsigned int	xs_bmbt_insrec;
	unsigned int	xs_bmbt_delrec;
					/* directory operations */
	unsigned int	xs_dir_lookup;
	unsigned int	xs_dir_create;
	unsigned int	xs_dir_remove;
	unsigned int	xs_dir_getdents;
					/* transactions */
	unsigned int	xs_trans_sync;
	unsigned int	xs_trans_async;
	unsigned int	xs_trans_empty;
					/* inode operations */
	unsigned int	xs_ig_attempts;
	unsigned int	xs_ig_found;
	unsigned int	xs_ig_frecycle;
	unsigned int	xs_ig_missed;
	unsigned int	xs_ig_dup;
	unsigned int	xs_ig_reclaims;
	unsigned int	xs_ig_attrchg;
					/* log operations */
	unsigned int	xs_log_writes;
	unsigned int	xs_log_blocks;
	unsigned int	xs_log_noiclogs;
	unsigned int	xs_log_force;
	unsigned int	xs_log_force_sleep;
					/* tail-pushing stats */
	unsigned int	xs_try_logspace;
	unsigned int	xs_sleep_logspace;
	unsigned int	xs_push_ail;
	unsigned int	xs_push_ail_success;
	unsigned int	xs_push_ail_pushbuf;
	unsigned int	xs_push_ail_pinned;
	unsigned int	xs_push_ail_locked;
	unsigned int	xs_push_ail_flushing;
	unsigned int	xs_push_ail_restarts;
	unsigned int	xs_push_ail_flush;
					/* xfsd work */
	unsigned int	xs_xfsd_bufs;
	unsigned int	xs_xstrat_bytes;
	unsigned int	xs_xstrat_quick;
	unsigned int	xs_xstrat_split;
					/* read/write stats */
	unsigned int	xs_write_calls;
	unsigned int	xs_write_bytes;
	unsigned int	xs_write_bufs;
	unsigned int	xs_read_calls;
	unsigned int	xs_read_bytes;
	unsigned int	xs_read_bufs;
					/* attribute operations */
	unsigned int	xs_attr_get;
	unsigned int	xs_attr_set;
	unsigned int	xs_attr_remove;
	unsigned int	xs_attr_list;
					/* quota operations */
	unsigned int	xs_qm_dqreclaims;
	unsigned int	xs_qm_dqreclaim_misses;
	unsigned int	xs_qm_dquot_dups;
	unsigned int 	xs_qm_dqcachemisses;
	unsigned int 	xs_qm_dqcachehits;
	unsigned int	xs_qm_dqwants;
	unsigned int 	xs_qm_dqshake_reclaims;
	unsigned int	xs_qm_dqinact_reclaims;
					/* for xfs_inode clustering */
	unsigned int	xs_iflush_count;
	unsigned int	xs_icluster_flushzero;
					/* allocated for future expansion */
	unsigned int	xs_pad[2];
};


/*
 * getblk /flush stats
 */
struct getblkstats {
	unsigned int	getblks;	/* # getblks */
	unsigned int	getblockmiss;	/* # times b_lock was missed */
	unsigned int	getfound;	/* # times buffer found in cache */
	unsigned int	getbchg;	/* # times buffer chged while waiting */
	unsigned int	getloops;	/* # times back to top of getblk */
	unsigned int	getfree;	/* # times fell thru to freelist code */
	unsigned int	getfreeempty;	/* # times freelist empty */
	unsigned int	getfreehmiss;	/* # times couldn't get old hash */
	unsigned int	getfreehmissx;	/* # times couldn't get old hash 20x */
	unsigned int	getfreealllck;	/* # times all free bufs were locked */
	unsigned int	getfreedelwri;	/* # times first free buf was DELWRI */
	unsigned int	flush;		/* # times flushing occurred */
	unsigned int	flushloops;	/* # times flushing looped */
	unsigned int	getfreeref;	/* # times first free buf was ref */
	unsigned int	getfreerelse;	/* # times first free buf had relse */
	unsigned int	getoverlap;	/* # times overlapping buffer found */
	unsigned int	clusters;	/* # times clustering attempted */
	unsigned int	clustered;	/* # clustered buffers */
	unsigned int    getfrag;        /* # page fragments read */
	unsigned int    getpatch;       /* # partial buffers patched */
	unsigned int	trimmed;	/* # of buffers made smaller */
	unsigned int	inserts;	/* chunk inserts */
	unsigned int	irotates;	/* rotates during inserts */
	unsigned int	deletes;	/* chunk deletes */
	unsigned int	drotates;	/* rotates during inserts */
	unsigned int	decomms;	/* chunk decommissions */
	unsigned int	flush_decomms;	/* chunk decommissions that flushed */
	unsigned int	delrsv;		/* delalloc_reserve calls */
	unsigned int	delrsvfree;	/* reserved w/out tossing */
	unsigned int	delrsvclean;	/* tossed clean buffer */
	unsigned int	delrsvdirty;	/* tossed dirty buffer */
	unsigned int	delrsvwait;	/* waited for buffer to be freed */
	unsigned int	sync_commits;	/* synchronous nfs commits */
	unsigned int	commits;	/* total nfs commits */
	unsigned int	getfreecommit;	/* times free buf was NFS_UNSTABLE */
	unsigned int	inactive;	/* buffer put on inactive list */
	unsigned int	active;		/* buffer taken off inactive list */
	unsigned int	force;		/* # of calls to vfs_force_pinned */
	unsigned int	xs_pad[4];	/* allocated for future expansion */
};

/*
 * vnode stats
 */
struct vnodestats {
	int	vn_vnodes;	/* total # vnodes, target */
	int	vn_extant;	/* total # vnodes currently allocated */
	int	vn_active;	/* # vnodes not on free lists */
	unsigned int	vn_alloc;	/* # times vn_alloc called */
	unsigned int	vn_aheap;	/* # times alloc from heap */
	unsigned int	vn_afree;	/* # times alloc from free list */
	unsigned int	vn_afreeloops;	/* # times pass on free list vnode */
	unsigned int	vn_get;		/* # times vn_get called */
	unsigned int	vn_gchg;	/* vn_get called, vnode changed */
	unsigned int	vn_gfree;	/* vn_get called, on free list */
	unsigned int	vn_rele;	/* # times vn_rele called */
	unsigned int	vn_reclaim;	/* # times vn_reclaim called */
	unsigned int	vn_destroy;	/* # times vnode struct removed */
	unsigned int	vn_afreemiss;	/* # times missed on free list search */
	unsigned int	vn_pad[4];	/* allocated for future expansion */
};

/* Statistics on driver locking */
struct drvlock {
	unsigned int		p_indcdev;
	unsigned int		p_indcdevsw;
	unsigned int		p_indbdev;
	unsigned int		p_indbdevsw;
	unsigned int		p_indstr;
	unsigned int		p_indstrsw;
	unsigned int		p_pad[2];	/* for future expansion */
};

#ifdef _KERNEL

struct ksa {
	struct	sysinfo si;	/* defined in /usr/include/sys/sysinfo.h  */
	struct	minfo	mi;	/* defined in /usr/include/sys/sysinfo.h */
	struct	dinfo	di;	/* defined in /usr/include/sys/sysinfo.h */
	struct  syserr  se;	/* defined in /usr/include/sys/sysinfo.h */
	struct  ncstats ncstats;	/* defined in sys/dnlc.h */
	struct	igetstats p_igetstats;
	struct	getblkstats p_getblkstats;
	struct	vnodestats p_vnodestats;
	struct	drvlock	dlock;
	struct	xfsstats p_xfsstats;
};

#include <sys/pda.h>	/* for private macro */
#include <sys/immu.h>	/* for PDAPAGE, inside private macro */

#define SYSINFO		private.ksaptr->si
#define MINFO		private.ksaptr->mi
#define DINFO		private.ksaptr->di
#define SYSERR		private.ksaptr->se
#define NCSTATS		private.ksaptr->ncstats
#define IGETINFO	private.ksaptr->p_igetstats
#define BUFINFO		private.ksaptr->p_getblkstats
#define VOPINFO		private.ksaptr->p_vnodestats
#define DRVLOCK		private.ksaptr->dlock
#define	XFSSTATS	private.ksaptr->p_xfsstats

#endif /* _KERNEL */
#endif /* __SYS_KSA_H__ */
