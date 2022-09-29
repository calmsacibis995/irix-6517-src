/*
 * $Revision: 1.67 $
 */
/*      @(#)rnode.h	1.4 88/07/15 NFSSRC4.0 from 1.21 88/02/08 SMI      */
/*	Copyright (C) 1988, Sun Microsystems Inc. */

#ifndef __NFS_RNODE_H__
#define __NFS_RNODE_H__

#include <sys/sema.h>

#define NFS_FHANDLE_LEN 64

typedef struct nfs_fhandle {
	int fh_len;
	char fh_buf[NFS_FHANDLE_LEN];
} nfs_fhandle;


typedef struct symlink_cache {
	char *contents;         /* contents of the symbolic link */
	int len;                /* length of the contents */
	int size;               /* size of the allocated buffer */
} symlink_cache;

/*
 * Commit info.
 */
struct commit {
	mutex_t	c_mutex;	/* protects the following */
	uint32	c_flags;
	offset3	c_count;
	offset3	c_offset;
	union {
		writeverf3	cu_verf1; /* version 3 write verifier */
		__uint64_t	cu_verf2; /* for efficiency */
	} c_verf_u;
	time_t	c_flushtime;
	short	c_flushpercent;	/* % of range to background commit */
	offset3	c_flushcount;
	offset3	c_flushoffset;
}; 
typedef struct commit commit_t;
#define c_verf	c_verf_u.cu_verf2
#define COMMIT_UNSTABLE		0x1	/* (count,offset,verf) valid */
#define COMMIT_FAILING		0x2	/* encountered bad verifier */
#define COMMIT_FLUSHING		0x4	/* unlocked async commit in progress */
#define COMMIT_FLUSH_ASYNC	0x8	/* flush in async queue, not yet begun */
#define IS_UNSTABLE(rp)		((rp)->r_commit.c_flags & COMMIT_UNSTABLE)
#define IS_FAILING(rp)		((rp)->r_commit.c_flags & COMMIT_FAILING)
#define IS_FLUSHING(rp)		((rp)->r_commit.c_flags & COMMIT_FLUSHING)
#define COMMIT_FLUSH_INTERVAL	(HZ/2)
#define COMMIT_FLUSH_PERCENTAGE_MAX 80
#define COMMIT_FLUSH_PERCENTAGE_MIN 20
#define COMMIT_FLUSH_PERCENTAGE_INTERVAL 10

struct access_cache;
struct rddir_cache;
struct xattr_cache;
/*
 * Remote file information structure.
 * The rnode is the "inode" for remote files.  It contains all the
 * information necessary to handle remote files on the client side.
 */
struct rnode {
	struct rnode	*r_next;	/* active rnode list */
	struct rnode	**r_prevp;	/* back-pointer in active list */
	struct rnode	*r_mnext;	/* next rnode in mntinfo list */
	struct rnode	**r_mprevp;	/* and previous rnode's &r_mnext */
	struct mntinfo	*r_mi;		/* points back to mntinfo struct */
	bhv_desc_t	r_bhv_desc;	/* nfs/nfs3 behavior descriptor */
	union {
	    fhandle_t	r_fh2;		/* file handle */
	    nfs_fhandle	r_fh3;
	} r_ufh;
	short		r_error;	/* async write error */
	uint		r_flags;	/* flags, see below (must be uint) */
	daddr_t		r_lastr;	/* last block read (read-ahead) */
	struct cred	*r_cred;	/* current credentials */
	union {
	    struct {
		struct cred	*ruu_unlcred;	/* unlinked credentials */
		char		*ruu_unlname;	/* unlinked regular file name */
		struct rnode	*ruu_unldrp;	/* unlinked file's parent dir */
	    } ru_unl;
	    struct {
		time_t		rus_symtime;	/* symlink expiration time */
		u_int		rus_symlen;	/* symlink value length */
		char		*rus_symval;	/* cached symlink contents */
	    } ru_sym;
	} r_u;
	off_t		r_size;		/* client's idea of file size */
	off_t		r_localsize;	/* client's highest local mapping */
	union {
	    struct nfsfattr	r_nfsattr2;	/* cached nfs attributes */
	    struct vattr	r_attr3;	/* cached vnode attributes */
	} r_a;
	time_t		r_attrtime;	/* time attributes become invalid */
	time_t		r_mtime;	/* client time file last modified */
	struct timeval	r_nfsattrtime;	/* time attributes become invalid */
	int		r_iocount;	/* io operations in progress */
	sv_t		r_iowait;	/* io synchronization */
	uint64_t	r_rwlockid;	/* id of thread owning r_rwlock */
	short		r_locktrips;	/* number of r_rwlock reacquisitions */
	mutex_t		r_rwlock;	/* vop_rwlock for nfs */
	mutex_t		r_statelock;	/* protects (most of) rnode contents */
	struct access_cache *r_acc;	/* cache of access results */
	struct rddir_cache *r_dir;	/* cache of readdir responses */
	struct rddir_cache *r_direof;	/* pointer to the EOF entry */
	symlink_cache	r_symlink;	/* cached readlink response */
	commit_t	r_commit;	/* commit information */
	cookieverf3     r_cookieverf;	/* cookie verifier from last readdir* */
	struct vsocket	*r_bds;		/* bulk data service */
	ushort		r_bds_vers;	/* bds protocol version */
	ushort		r_bds_flags;	/* RBDS_ flags */
	int		r_bds_oflags;	/* oflags of the last read/write req */
	uint64_t	r_bds_buflen;	/* buflen to request of the server */
	u_char		r_bds_priority;	/* BDS priority */
	mrlock_t	r_otw_attr_lock;
	struct xattr_cache *r_xattr;	/* critical extended attributes */
};
typedef struct rnode	rnode_t;
#define r_fh	r_ufh.r_fh2

#define r_nfsattr	r_a.r_nfsattr2
#define r_attr		r_a.r_attr3

#define r_unlcred	r_u.ru_unl.ruu_unlcred
#define r_unlname	r_u.ru_unl.ruu_unlname
#define r_unldrp	r_u.ru_unl.ruu_unldrp
#define r_symval	r_u.ru_sym.rus_symval
#define r_symlen	r_u.ru_sym.rus_symlen
#define r_symtime	r_u.ru_sym.rus_symtime

/*
 * Flags
 */
#define RLOCKED		0x00001		/* rnode is in use */
#define REOF		0x00008		/* EOF encountered on read */
#define RDIRTY		0x00010		/* dirty buffers may be in buf cache */
#define RVNODEWAIT	0x00100		/* waiting for iget in makenfsnode */
#define RRECLAIMED	0x04000		/* file locks have been reclaimed */
#define RCLEANLOCKS 	0x08000		/* file locks must be cleaned remotely */
#define RV3 		0x10000		/* version 3 rnode */


#define	RBDS_WBEHIND		0x0001	/* BDS writes are being ack'd early */
#define	RBDS_DIRTY		0x0002	/* BDS has outstanding, unsync'd writes */
#define	RBDS_SPECIFIED		0x0004	/* user set r_bds_oflags explicitly */
#define	RBDS_NEED_REOPEN	0x0008	/* oflags has changed -- must reopen */
#define RBDS_PEND_ERR		0x0010	/* error not yet reported to client */

/*
 * Maximum allowed file offset for NFSv2.
 */
#define	NFS_MAX_FILE_OFFSET	0x7fffffff

/*
 * Convert between vnode and rnode
 */
#define rtov(rp)	((struct vnode *)BHV_VOBJNULL(rtobhv(rp)))
#define bhvtor(bdp)	((struct rnode *)(BHV_PDATA((bdp))))
#define	rtobhv(rp)	(&((rp)->r_bhv_desc))
#define bhvtofh(bdp)	(&(bhvtor(bdp)->r_fh))
#define rtofh(rp)	(&(rp)->r_fh)

#define nohang()    (curuthread ? \
			 (curuthread->ut_pproxy->prxy_flags & PRXY_NOHANG) : 0)
#ifdef DEBUG_NOHANG
#define debug_rlock()         (curuthread ? \
			 (curuthread->ut_pproxy->prxy_flags & PRXY_RLCKDEBUG) : 0)
#define debug_rwlock()        (curuthread ? \
			 (curuthread->ut_pproxy->prxy_flags & PRXY_RWLCKDEBUG) : 0)
#endif

/*
 * Lock and unlock rnodes.
 */
#define rnode_is_locked(rp)	(mrislocked_any(&(rp)->r_otw_attr_lock))
#define rnode_is_rwlocked(rp)	(mutex_owned(&(rp)->r_rwlock))

extern int raccess(rnode_t *);
extern void rlock(rnode_t *);
extern int rlock_nohang(rnode_t *);
extern void runlock(rnode_t *);

extern void	initrnodes(void);
extern void	rdestroy(struct rnode *, struct mntinfo *);
extern int	rflush(struct mntinfo *);
extern int	makenfsnode(struct vfs *, mntinfo_t *, fhandle_t *, 
				struct nfsfattr *,  bhv_desc_t **); 
extern struct rnode *rfind(struct vfs *, fhandle_t *);
extern int	make_rnode(fhandle_t *fh, struct vfs *vfsp,
		           struct vnodeops *vops, int *newnode,
			   struct mntinfo *mi, bhv_desc_t **bdpp,
			   struct nfsfattr *attr);
			   
extern void	rinactive(rnode_t *);

extern mutex_t rnodemonitor;
extern sv_t vnodewait;

/*
 * Set and clear rnode flags.
 */
#define rsetflag(rp, f)		bitlock_set(&(rp)->r_flags, RLOCKED, (f))
#define rclrflag(rp, f)		bitlock_clr(&(rp)->r_flags, RLOCKED, (f))

#define nfsattr_inval(bdp)	(bhvtor(bdp)->r_nfsattrtime.tv_sec = 0)

#endif /* __NFS_RNODE_H__ */
