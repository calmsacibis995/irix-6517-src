#ifndef __XFS_QM_H__
#define __XFS_QM_H__

#ident "$Revision: 1.11 $"

struct  xfs_dqhash;
struct  xfs_inode;
struct  xfs_dquot;
struct  xfs_qm;

/* 
 * The global quota manager. There is only one of these for the entire
 * system, _not_ one per file system. XQM keeps track of the overall
 * quota functionality, including maintaining the freelist and hash
 * tables of dquots.
 */
extern	struct xfs_qm		*xfs_Gqm;		
extern	mutex_t			xfs_Gqm_lock;

/*
 * Used in xfs_qm_sync called by xfs_sync to count the max times that it can
 * iterate over the mountpt's dquot list in one call.
 */
#define XFS_QM_SYNC_MAX_RESTARTS	7

/*
 * Ditto, for xfs_qm_dqreclaim_one.
 */
#define XFS_QM_RECLAIM_MAX_RESTARTS	4

/*
 * Ideal ratio of free to in use dquots. Quota manager makes an attempt
 * to keep this balance.
 */
#define XFS_QM_DQFREE_RATIO		2

/*
 * Dquot hashtable constants/threshold values.
 */
#define XFS_QM_NCSIZE_THRESHOLD		5000
#define XFS_QM_HASHSIZE_LOW		32
#define XFS_QM_HASHSIZE_HIGH		64

/*
 * We output a cmn_err when quotachecking a quota file with more than
 * this many fsbs.
 */
#define XFS_QM_BIG_QCHECK_NBLKS		500

/*
 * This defines the unit of allocation of dquots. 
 * Currently, it is just one file system block, and a 4K blk contains 30 
 * (136 * 30 = 4080) dquots. It's probably not worth trying to make 
 * this more dynamic.
 * XXXsup However, if this number is changed, we have to make sure that we don't
 * implicitly assume that we do allocations in chunks of a single filesystem
 * block in the dquot/xqm code.
 */
#define XFS_DQUOT_CLUSTER_SIZE_FSB	(xfs_filblks_t)1
/*
 * When doing a quotacheck, we log dquot clusters of this many FSBs at most
 * in a single transaction. We don't want to ask for too huge a log reservation.
 */
#define XFS_QM_MAX_DQCLUSTER_LOGSZ	3

typedef xfs_dqhash_t	xfs_dqlist_t;
/*
 * The freelist head. The first two fields match the first two in the
 * xfs_dquot_t structure (in xfs_dqmarker_t)
 */
typedef struct xfs_frlist {
       struct xfs_dquot	*qh_next;
       struct xfs_dquot	*qh_prev;
       mutex_t	 	 qh_lock;
       uint      	 qh_version;
       uint	 	 qh_nelems;
} xfs_frlist_t;

/*
 * Quota Manager (global) structure. Lives only in core.
 */
typedef struct xfs_qm {
	xfs_dqlist_t	*qm_usr_dqhtable;/* udquot hash table */
	xfs_dqlist_t	*qm_prj_dqhtable;/* pdquot hash table */
	uint		 qm_dqhashmask;  /* # buckets in dq hashtab - 1 */
	xfs_frlist_t	 qm_dqfreelist;  /* freelist of dquots */
	uint		 qm_totaldquots; /* total incore dquots */
	uint		 qm_nrefs;	 /* file systems with quota on */
	int		 qm_dqfree_ratio;/* ratio of free to inuse dquots */
	zone_t		*qm_dqzone;	 /* dquot mem-alloc zone */
	zone_t		*qm_dqtrxzone;	 /* t_dqinfo of transactions */
} xfs_qm_t;

/*
 * Various quota information for individual filesystems.
 * The mount structure keeps a pointer to this.
 */
typedef struct xfs_quotainfo {
 	xfs_inode_t	*qi_uquotaip;	 /* user quota inode */
	xfs_inode_t	*qi_pquotaip;	 /* project quota inode */
	lock_t           qi_pinlock;     /* dquot pinning mutex */
	xfs_dqlist_t	 qi_dqlist;      /* all dquots in filesys */
	int		 qi_dqreclaims;  /* a change here indicates
					    a removal in the dqlist */
	time_t		 qi_btimelimit;  /* limit for blks timer */	
	time_t		 qi_itimelimit;  /* limit for inodes timer */	
	time_t		 qi_rtbtimelimit;/* limit for rt blks timer */	
	xfs_qwarncnt_t	 qi_bwarnlimit;	 /* limit for num warnings */
	xfs_qwarncnt_t	 qi_iwarnlimit;	 /* limit for num warnings */
	mutex_t		 qi_quotaofflock;/* to serialize quotaoff */
	/* Some useful precalculated constants */
	xfs_filblks_t	 qi_dqchunklen;  /* # BBs in a chunk of dqs */
	uint		 qi_dqperchunk;  /* # ondisk dqs in above chunk */
} xfs_quotainfo_t;	


/*
 * The structure kept inside the xfs_trans_t keep track of dquot changes
 * within a transaction and apply them later.
 */
typedef struct xfs_dqtrx {
	struct xfs_dquot *qt_dquot;	  /* the dquot this refers to */
	ulong		qt_blk_res;	  /* blks reserved on a dquot */
	ulong		qt_blk_res_used;  /* blks used from the reservation */
	ulong		qt_ino_res;	  /* inode reserved on a dquot */
	ulong		qt_ino_res_used;  /* inodes used from the reservation */
	long		qt_bcount_delta;  /* dquot blk count changes */
	long		qt_delbcnt_delta; /* delayed dquot blk count changes */
	long		qt_icount_delta;  /* dquot inode count changes */
	ulong		qt_rtblk_res;	  /* # blks reserved on a dquot */
	ulong		qt_rtblk_res_used;/* # blks used from reservation */
	long		qt_rtbcount_delta;/* dquot realtime blk changes */
	long		qt_delrtb_delta;  /* delayed RT blk count changes */
} xfs_dqtrx_t;
 
/*
 * We keep the usr and prj dquots separately so that locking will be easier
 * to do at commit time. All transactions that we know of at this point
 * affect no more than two dquots of one type. Hence, the TRANS_MAXDQS value.
 */
#define XFS_QM_TRANS_MAXDQS		2
typedef struct xfs_dquot_acct {
	xfs_dqtrx_t	dqa_usrdquots[XFS_QM_TRANS_MAXDQS];
	xfs_dqtrx_t	dqa_prjdquots[XFS_QM_TRANS_MAXDQS];
} xfs_dquot_acct_t;

/*
 * Users are allowed to have a usage exceeding their softlimit for
 * a period this long.
 */
#define XFS_QM_BTIMELIMIT	DQ_BTIMELIMIT
#define XFS_QM_RTBTIMELIMIT	DQ_BTIMELIMIT
#define XFS_QM_ITIMELIMIT	DQ_FTIMELIMIT

#define XFS_QM_BWARNLIMIT	5
#define XFS_QM_IWARNLIMIT	5

#define XFS_QM_LOCK(xqm)	(mutex_lock(&xqm##_lock, PINOD))
#define XFS_QM_UNLOCK(xqm)	(mutex_unlock(&xqm##_lock))
#define XFS_QM_HOLD(xqm)	((xqm)->qm_nrefs++)
#define XFS_QM_RELE(xqm)	((xqm)->qm_nrefs--)

#if DEBUG
extern int 		xfs_quotadebug;
extern int		xfs_qm_internalqcheck(xfs_mount_t *);
#endif 

extern int		xfs_qm_init_quotainfo(xfs_mount_t *);
extern void 		xfs_qm_destroy_quotainfo(xfs_mount_t *);
extern void		xfs_qm_dqunlink(xfs_dquot_t *);
extern boolean_t	xfs_qm_dqalloc_incore(xfs_dquot_t **);
extern int 		xfs_qm_write_sb_changes(xfs_mount_t *, __int64_t);
extern int		xfs_qm_scall_quotaoff(xfs_mount_t *, uint, boolean_t);

/* list stuff */
extern void		xfs_qm_freelist_init(xfs_frlist_t *);
extern void		xfs_qm_freelist_destroy(xfs_frlist_t *);
extern void		xfs_qm_freelist_insert(xfs_frlist_t *, xfs_dquot_t *);
extern void 		xfs_qm_freelist_append(xfs_frlist_t *, xfs_dquot_t *);
extern void		xfs_qm_freelist_unlink(xfs_dquot_t *);
extern int		xfs_qm_freelist_lock_nowait(xfs_qm_t *);
extern int		xfs_qm_mplist_nowait(xfs_mount_t *);
extern int		xfs_qm_dqhashlock_nowait(xfs_dquot_t *);

#ifdef QUOTADEBUG
extern void		xfs_qm_freelist_print(xfs_frlist_t *, char *);
#else
#define xfs_qm_freelist_print(a, b)
#endif

#endif /* __XFS_QM_H__ */
