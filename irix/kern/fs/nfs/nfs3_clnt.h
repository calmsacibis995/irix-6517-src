#ifndef __SYS_NFS3_CLNT_H__
#define __SYS_NFS3_CLNT_H__

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*      @(#)nfs3_clnt.h  1.2 88/07/15 NFSSRC4.0 from 2.19 88/02/08 SMI      */
/*      Copyright (C) 1988, Sun Microsystems Inc.       */

#ident "$Revision: 1.29 $"

#ifdef _KERNEL

#define	ACMINMAX	3600	/* 1 hr is longest min timeout */
#define	ACMAXMAX	36000	/* 10 hr is longest max timeout */

/*
 * Fake errno passed back from rfscall to indicate transfer size adjustment
 */
#define	ENFS_TRYAGAIN	999

/*
 * vnode behavior pointer to mount info
 */
#define RTOMI(rp)       rtomi(rp)
#define VN_BHVTOMI(bdp) vn_bhvtomi(bdp)


/* Async io (biod) data structures */

enum iotype {NFS_COMMIT, NFS_READDIR, NFS_BIO };

struct nfs_commit_req {
	off_t offset;			/* offset in file */
	count3 count;			/* size of request */
	struct buf *bp;			/* buffer pointer */
};

struct	rddir_cache;
struct nfs_readdir_req {
	int (*readdir)(bhv_desc_t *, struct rddir_cache *, struct cred *, u_char);		/* pointer to readdir function */
	struct rddir_cache *rdc;	/* pointer to cache entry to fill */
	u_char	abi;
};

struct nfs_bio_req {
	void (*bio)(bhv_desc_t *, struct buf *, stable_how *, struct cred *);	/* pointer to bio func */
	struct buf *bp;			/* pointer to buffer */
};

struct nfs_async_reqs {
	struct nfs_async_reqs *a_next;	/* pointer to next arg struct */
	struct bhv_desc *a_bdp;		/* behavior descriptor pointer */
	struct cred *a_cred;		/* cred pointer */
	enum iotype a_io;		/* i/o type */
	union {
		struct nfs_commit_req a_commit_args;
		struct nfs_readdir_req a_readdir_args;
		struct nfs_bio_req a_bio_args;
	} a_args;
};

#define	a_nfs_offset a_args.a_commit_args.offset
#define	a_nfs_count a_args.a_commit_args.count
#define	a_nfs_bufp  a_args.a_commit_args.bp

#define	a_nfs_readdir a_args.a_readdir_args.readdir
#define	a_nfs_rdc a_args.a_readdir_args.rdc
#define a_nfs_abi a_args.a_readdir_args.abi

#define	a_nfs_bio	a_args.a_bio_args.bio
#define	a_nfs_bp	a_args.a_bio_args.bp

/*
 * Mark cached attributes as timed out
 */
extern time_t time;
#define PURGE_ATTRCACHE(bdp) {					\
	struct rnode *rp;					\
	rp = bhvtor((bdp));					\
	mutex_enter(&rp->r_statelock);				\
	rp->r_attrtime = time;					\
	xattr_cache_invalidate (rp->r_xattr);			\
	mutex_exit(&rp->r_statelock);				\
}

/*
 * Mark cached extended attributes as invalid
 */
#define PURGE_XATTRCACHE(bdp)					\
	do {							\
		struct rnode *rp = bhvtor((bdp));		\
		xattr_cache_invalidate (rp->r_xattr);		\
	} while (0)

/*
 * Mark cached attributes as uninitialized (must purge all caches first)
 */
#define INVAL_ATTRCACHE(bdp) {                                   \
	bhvtor(bdp)->r_attrtime = 0;                     		\
}


/*
 * If returned error is ESTALE flush all caches.
 */
#define	PURGE_STALE_FH(errno, bdp, cr)				\
	if ((errno) == ESTALE) {				\
		struct rnode *rp = bhvtor(bdp);			\
		rsetflag(rp, RDONTWRITE);			\
		VOP_RWLOCK(BHV_TO_VNODE(bdp), VRWLOCK_WRITE);	\
		if (BHV_TO_VNODE(bdp)->v_type != VCHR)		\
			VOP_TOSS_PAGES(BHV_TO_VNODE(bdp), 0, 	\
			rp->r_localsize - 1, FI_REMAPF_LOCKED);		\
		VOP_RWUNLOCK(BHV_TO_VNODE(bdp), VRWLOCK_WRITE);	\
		nfs_purge_caches(bdp, cr);			\
}

/*
 * Is cache valid?
 * Swap is always valid, if no attributes (attrtime == 0) or
 * if mtime matches cached mtime it is valid
 * NOTE: mtime is now a timespec_t.
 */
#define MACRO_CACHE_VALID(rp, mtime, fsize)			\
	((rtov(rp)->v_flag & VISSWAP) == VISSWAP ||             \
	(rp->r_attrtime && 					\
	(((mtime).tv_sec == (rp)->r_attr.va_mtime.tv_sec &&     \
	(mtime).tv_nsec == (rp)->r_attr.va_mtime.tv_nsec) &&    \
	((fsize) == (rp)->r_attr.va_size))))
#ifndef NFSCACHEDEBUG
#define CACHE_VALID(a,b,c) MACRO_CACHE_VALID(a,b,c)
#else
#define CACHE_VALID(rp,mtime,fsize)   nfs3_cache_valid(rp,mtime,fsize)
#endif /* NFSCACHEDEBUG */

#define	roundto64int(x) ((__uint64_t)((x) + sizeof (__uint64_t) - 1) \
		& ~(sizeof (__uint64_t) - 1))

#define	roundto32int(x) ((__uint64_t)((x) + sizeof (__uint32_t) - 1) \
		& ~(sizeof (__uint32_t) - 1))
extern dev_t	makedevice(major_t, minor_t);
extern dev_t 	nfs3_major;
extern struct vnodeops nfs3_vnodeops;

#endif	/* _KERNEL */
#endif /* !__SYS_NFS3_CLNT_H__ */
