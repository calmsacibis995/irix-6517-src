/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_CACHEFS_FS_H
#define	_SYS_FS_CACHEFS_FS_H

/* #pragma ident	"@(#)cachefs_fs.h	1.62	94/01/05 SMI" */

#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <string.h>
#include <sys/dirent.h>
#if _KERNEL
#include <sys/atomic_ops.h>
#include <sys/kmem.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#endif /* _KERNEL */

#ifdef __cplusplus
extern "C" {
#endif

#define CFS_DEF_DIR			"/cache"
#define CFS_DEF_ACREGMIN    3
#define CFS_DEF_ACREGMAX    60
#define CFS_DEF_ACDIRMIN    30
#define CFS_DEF_ACDIRMAX    60

/*
 * CacheFS block size definitions
 */
#define FRONT_BSIZE(CP)		(C_TO_FSCACHE(CP)->fs_cache->c_bsize)
#define BACK_BSIZE(CP)		(C_TO_FSCACHE(CP)->fs_bsize)
#define CFS_BMAPSIZE(CP)	(C_TO_FSCACHE(CP)->fs_bmapsize)
#define CFS_BOFFSET(CP)		(C_TO_FSCACHE(CP)->fs_bmapsize - 1)
#define CFS_BMASK(CP)		(~CFS_BOFFSET(CP))
#define CFS_SAME_BLOCK(CP, OFF1, OFF2)	\
	(((OFF1) & CFS_BMASK(CP)) == ((OFF2) & CFS_BMASK(CP)))

#define MAXCFSREADAHEAD	10
#define NCFSREADMAPS	MAXCFSREADAHEAD + 1
#define NCFSWRITEMAPS	1
extern int cachefs_readahead;
extern int front_dio;

#if defined(ROUNDUP)
#undef ROUNDUP
#endif /* ROUNDUP */

/*
 * round a up to the nearest b, where a and b area arbitrary integers
 */
#define ROUNDUP(a, b)	((((a) + (b) - 1) / (b)) * (b))

/*
 * flags for cachefs_flushvp()
 */
#define CFS_INVAL		0x1
#define CFS_FREE		0x2
#define CFS_TRUNC		0x4

#define GCWAIT_TIME		60

#define CACHEFS_LOGMODE		0600
#define CACHEFS_FILEMODE	0600
#define CACHEFS_DIRMODE		0700

/*
 * type definition for longlong
 */
typedef __uint64_t longlong_t;

/*
 * alias kcred to sys_cred
 */
#define kcred	sys_cred

#ifdef DEBUG

#define _CNODE_TRACE		/* enable cnode tracing */

#define CFS_TRACEBUFLEN  16

struct trace_record {
	int		tr_id;			/* trace record ID */
	void	*tr_addr;		/* calling function */
	int		tr_line;		/* source line number */
	long	tr_val[3];		/* arguments */
};

typedef struct trace_record tracerec_t;

struct cfstrace {
	int         ct_index;
	int         ct_wrap;
	tracerec_t  ct_buffer[CFS_TRACEBUFLEN];
	lock_t		ct_lock;
};

typedef struct cfstrace cfstrace_t;
#endif /* DEBUG */

/* Some default values */
#define	DEF_POP_SIZE		0x10000		/* 64K */
#define	CFSVERSION			3
#define	CACHELABEL_NAME		".cfs_label"
#define	BACKMNT_NAME		".cfs_mnt_points"
#define	CACHEFS_ROOTNAME	"root"

/*
 * cachefs_sys commands
 */
#define CACHEFSSYS_REPLACEMENT		1	/* replacement daemon */
#define CACHEFSSYS_CHECKFID			2	/* check a FID to see if it is valid */
#define CACHEFSSYS_RECONNECT		3	/* reconnection daemon */
#define CACHEFSSYS_MOUNT			4	/* disconnected mount completion */

/*
 * all the stuff needed to manage a queue of requests to be processed
 * by async threads.
 */
struct cachefs_workq {
	struct cachefs_req	*wq_head;		/* head of work q */
	struct cachefs_req	*wq_tail;		/* tail of work q */
	int					wq_length;		/* # of requests on q */
	int					wq_thread_count;	/* total # of threads */
	int					wq_thread_busy;		/* # of busy threads */
	int					wq_max_len;		/* longest queue */
	u_int				wq_halt_request:1;	/* halt requested */
	u_int				wq_keepone:1;		/* keep one thread */
	sv_t				wq_req_sv;		/* wait on work to do */
	sv_t				wq_halt_sv;		/* wait/signal halt */
	lock_t				wq_queue_lock;		/* protect queue */
};

typedef struct cachefscache cachefscache_t;

#define	CNODE_BUCKET_SIZE	1597
#define FSID_MAP_SLOTS		31

struct fsidmap {
	dev_t			fsid_back;    /* back FS attributed fsid */
	dev_t			fsid_cachefs; /* cachefs-unique fsid */
	struct fsidmap  *fsid_next; /* other mappings */
};

/*
 * CacheFS minor device number allocation bitmap.
 */
#define MINORMAPSIZE	(256/NBBY)

struct minormap {
	u_char	vec[MINORMAPSIZE];
};

/*
 * fscache structure contains per-filesystem information, both filesystem
 * cache directory information and mount-specific information.
 */
struct fscache {
	char				 *fs_cacheid;	/* File system ID */
	int					 fs_flags;
	struct cachefscache	*fs_cache;	/* back ptr to cache struct */
	u_int				 fs_options;	/* mount options */
	bhv_desc_t			fs_bhv;	/* vfs cachefs behavior */
	struct vfs			*fs_backvfsp;	/* back vfsp */
	struct vfs			*fs_frontvfsp;	/* front vfsp */
	struct vnode		*fs_rootvp;	/* root vnode ptr */
	struct cnode		*fs_root;	/* root cnode ptr */
	int					 fs_vnoderef;	/* activate vnode ref count */
	struct vnode		*fs_cacheidvp;	/* front FS cacheid dir vnode */
	struct cachefsops	*fs_cfsops;	/* cfsops vector pointer */
	u_int				 fs_acregmin;	/* same as nfs values */
	u_int				 fs_acregmax;
	u_int				 fs_acdirmin;
	u_int				 fs_acdirmax;
	struct fscache		*fs_next;	/* ptr to next fscache */
	struct cnode		*fs_cnode[CNODE_BUCKET_SIZE]; /* hash buckets */
	struct cnode		*fs_cnode_by_fid[CNODE_BUCKET_SIZE];
	struct cnode		*fs_reclaim;	/* list of cnodes to be freed */
	u_int				fs_hashvers;	/* hash version */
	off_t				fs_bsize;	/* back FS block size */
	off_t				fs_bmapsize;	/* FS block size for bmap */
	u_int				fs_sync_running;	/* sync is running or not */
	struct fsidmap		*fs_fsidmap[FSID_MAP_SLOTS];
	mutex_t				fs_fsidmaplock;	/* protects fsid map */
	lock_t				fs_fslock;
	/* protect contents - everyting xcept the cnode hash */
	mutex_t				fs_cnodelock;	/* protects creation of cnodes */
	sv_t				fs_sync_wait;	/* wait point for umount sync */
	sv_t				fs_reconnect;	/* wait for reconnection */
};
typedef struct fscache fscache_t;

/* valid fscache flags */
#define	CFS_FS_MOUNTED		1	/* fscache is mounted */
#define	CFS_FS_READ			2	/* fscache can be read */
#define	CFS_FS_WRITE		4	/* fscache can be written */

#define FSC_TO_BACKVFS(FSCP)	(FSCP)->fs_backvfsp
#define FSC_TO_FRONTVFS(FSCP)	(FSCP)->fs_frontvfsp
#define FSC_TO_VFS(FSCP)	bhvtovfs((&(FSCP)->fs_bhv))

#define	CFSOP_INIT_COBJECT(FSCP, CP, VAP, CR)	\
	(*(FSCP)->fs_cfsops->co_init_cobject)(FSCP, CP, VAP, CR)
#define	CFSOP_CHECK_COBJECT(FSCP, CP, VAP, CR, LM)	\
	(*(FSCP)->fs_cfsops->co_check_cobject)(FSCP, CP, VAP, CR, LM)
#define	CFSOP_MODIFY_COBJECT(FSCP, CP, CR)	\
	(*(FSCP)->fs_cfsops->co_modify_cobject)(FSCP, CP, CR)
#define	CFSOP_INVALIDATE_COBJECT(FSCP, CP, CR)	\
	(*(FSCP)->fs_cfsops->co_invalidate_cobject)(FSCP, CP, CR)
#define	CFSOP_EXPIRE_COBJECT(FSCP, CP, CR)	\
	(*(FSCP)->fs_cfsops->co_expire_cobject)(FSCP, CP, CR)

#define	C_ISFS_WRITE_AROUND(FSCP) ((FSCP)->fs_options & CFS_WRITE_AROUND)
#define	C_ISFS_STRICT(FSCP) \
	(((FSCP)->fs_options & CFS_WRITE_AROUND) && \
	(((FSCP)->fs_options & CFS_NOCONST_MODE) == 0))
#define	C_ISFS_SINGLE(FSCP) ((FSCP)->fs_options & CFS_DUAL_WRITE)
#define	C_ISFS_NOCONST(FSCP) ((FSCP)->fs_options & CFS_NOCONST_MODE)
#define C_ISFS_DISCONNECT(FSCP)	((FSCP)->fs_options & CFS_DISCONNECT)
#define C_ISFS_NONBLOCK(FSCP)	((FSCP)->fs_options & CFS_NONBLOCK)
#define C_ISFS_PRIVATE(FSCP)	((FSCP)->fs_options & CFS_PRIVATE)

/*
 * Front FS vnode operation macros.
 */
#define FRONT_VOP_GETATTR(cp, attrp, flag, credp) \
	cachefs_frontop_getattr(cp, attrp, flag, credp)
#define FRONT_VOP_SETATTR(cp, attrp, flags, credp) \
	cachefs_frontop_setattr(cp, attrp, flags, credp)
#define FRONT_VOP_FSYNC(cp, syncflag, credp, start, stop) \
	cachefs_frontop_fsync(cp, syncflag, credp, start, stop)
#define FRONT_VOP_LOOKUP(dcp, nm, vpp, pnp, flags, rdir, credp) \
	cachefs_frontop_lookup(dcp, nm, vpp, pnp, flags, rdir, credp)
#define FRONT_VOP_LINK(tdcp, cp, tnm, credp) \
	cachefs_frontop_link(tdcp, cp, tnm, credp)
#define FRONT_VOP_LOOKUP(dcp, nm, vpp, pnp, flags, rdir, credp) \
	cachefs_frontop_lookup(dcp, nm, vpp, pnp, flags, rdir, credp)
#define FRONT_VOP_RENAME(odcp, onm, ndcp, nnm, pnp, credp) \
	cachefs_frontop_rename(odcp, onm, ndcp, nnm, pnp, credp)
#define FRONT_VOP_REMOVE(dcp, nm, credp) cachefs_frontop_remove(dcp, nm, credp)
#define FRONT_VOP_FID(cp, fidpp) cachefs_frontop_fid(cp, fidpp)
#define FRONT_VOP_FID2(cp, fidp) cachefs_frontop_fid2(cp, fidp)

/*
 * Back FS vnode operation macros.
 */
enum backop_mode {BACKOP_BLOCK, BACKOP_NONBLOCK};

#define BACK_VOP_GETATTR(cp, attrp, flag, credp, mode, errorp) \
	cachefs_backop_getattr(cp, attrp, flag, credp, mode, errorp)
#define BACK_VOP_READDIR(cp, uiop, credp, eofp, mode, errorp) \
	cachefs_backop_readdir(cp, uiop, credp, eofp, mode, errorp)
#define BACK_VOP_SETATTR(cp, attrp, flags, credp, mode, errorp) \
	cachefs_backop_setattr(cp, attrp, flags, credp, mode, errorp)
#define BACK_VOP_ACCESS(cp, mode, credp, opmode, errorp) \
	cachefs_backop_access(cp, mode, credp, opmode, errorp)
#define BACK_VOP_READLINK(cp, uiop, credp, mode, errorp) \
	cachefs_backop_readlink(cp, uiop, credp, mode, errorp)
#define BACK_VOP_CREATE(dcp, nm, vap, excl, mode, devvpp, credp, opmode, errorp)\
	cachefs_backop_create(dcp, nm, vap, excl, mode, devvpp, credp, opmode, \
		errorp)
#define BACK_VOP_REMOVE(dcp, nm, credp, mode, errorp) \
	cachefs_backop_remove(dcp, nm, credp, mode, errorp)
#define BACK_VOP_LINK(tdcp, cp, tnm, credp, mode, errorp) \
	cachefs_backop_link(tdcp, cp, tnm, credp, mode, errorp)
#define BACK_VOP_RENAME(odcp, onm, ndcp, nnm, pnp, credp, mode, errorp) \
	cachefs_backop_rename(odcp, onm, ndcp, nnm, pnp, credp, mode, errorp)
#define BACK_VOP_MKDIR(dcp, nm, va, vpp, credp, mode, errorp) \
	cachefs_backop_mkdir(dcp, nm, va, vpp, credp, mode, errorp)
#define BACK_VOP_RMDIR( dcp, nm, cdir, credp, mode, errorp) \
	cachefs_backop_rmdir(dcp, nm, cdir, credp, mode, errorp)
#define BACK_VOP_SYMLINK(dcp, lnm, tva, tnm, credp, mode, errorp) \
	cachefs_backop_symlink(dcp, lnm, tva, tnm, credp, mode, errorp)
#define BACK_VOP_FRLOCK(cp, cmd, bfp, flag, offset, credp, mode, vrwlock, errorp) \
	cachefs_backop_frlock(cp, cmd, bfp, flag, offset, credp, mode, vrwlock, errorp)
#define BACK_VOP_FCNTL(cp, cmd, arg, flags, offset, cr, rvp, mode, errorp) \
	cachefs_backop_fcntl(cp, cmd, arg, flags, offset, cr, rvp, mode, errorp)
#define BACK_VOP_VNODE_CHANGE(cp, cmd, val, mode) \
	cachefs_backop_vnode_change(cp, cmd, val, mode)

#define BACK_VOP_ATTR_GET(cp, name, value, valuelenp, flags, credp, mode, errorp) \
	cachefs_backop_attr_get(cp, name, value, valuelenp, flags, credp, mode, errorp)
#define BACK_VOP_ATTR_SET(cp, name, value, valuelen, flags, credp, mode, errorp) \
	cachefs_backop_attr_set(cp, name, value, valuelen, flags, credp, mode, errorp)
#define BACK_VOP_ATTR_REMOVE(cp, name, flags, credp, mode, errorp) \
	cachefs_backop_attr_remove(cp, name, flags, credp, mode, errorp)
#define BACK_VOP_ATTR_LIST(cp, buffer, bufsize, flags, cursor, credp, mode, errorp) \
	cachefs_backop_attr_list(cp, buffer, bufsize, flags, cursor, credp, mode, errorp)

/*
 * back FS vfs operations
 */
#define BACK_VFS_STATVFS(fscp, sbp, opmode, errorp) \
	cachefs_backop_statvfs(fscp, sbp, opmode, errorp)

#define NET_VOP_FID(vp, fidpp, opmode, error, status) { \
	int retry = 0; \
	do { \
		CACHEFS_STATS->cs_backvnops++; \
		VOP_FID(vp, fidpp, error); \
		switch (error) { \
			case 0: \
				status = BACKOP_SUCCESS; \
				retry = 0; \
				break; \
			case ETIMEDOUT: \
			case ENETDOWN: \
			case ENETUNREACH: \
			case ECONNREFUSED: \
			case ENOBUFS: \
			case ECONNABORTED: \
			case ENETRESET: \
			case ECONNRESET: \
			case ENONET: \
			case ESHUTDOWN: \
			case EHOSTDOWN: \
			case EHOSTUNREACH: \
				if (opmode == BACKOP_BLOCK) { \
					retry = 1; \
				} else { \
					retry = 0; \
				} \
				status = BACKOP_NETERR; \
				CACHEFS_STATS->cs_neterror++; \
				CFS_DEBUG(CFSDEBUG_NETERR, \
					printf("NET_VOP_GETATTR(%s, line %d): network error %d %s %s\n", \
						__FILE__, __LINE__, error, \
						(opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : \
						"BACKOP_NONBLOCK", retry ? "retry" : "")); \
				break; \
			case ESTALE: \
			default: \
				status = BACKOP_FAILURE; \
				retry = 0; \
		} \
	} while (retry); \
}

#define NET_VOP_GETATTR_NOSTAT(vp, attrp, flag, credp, opmode, error) { \
	int retry = 0; \
	do { \
		CACHEFS_STATS->cs_backvnops++; \
		CACHEFS_STATS->cs_backops.cb_get++; \
		VOP_GETATTR(vp, attrp, flag, credp, error); \
		switch (error) { \
			case 0: \
				retry = 0; \
				break; \
			case ETIMEDOUT: \
			case ENETDOWN: \
			case ENETUNREACH: \
			case ECONNREFUSED: \
			case ENOBUFS: \
			case ECONNABORTED: \
			case ENETRESET: \
			case ECONNRESET: \
			case ENONET: \
			case ESHUTDOWN: \
			case EHOSTDOWN: \
			case EHOSTUNREACH: \
				if (opmode == BACKOP_BLOCK) { \
					retry = 1; \
				} else { \
					retry = 0; \
				} \
				CACHEFS_STATS->cs_neterror++; \
				CFS_DEBUG(CFSDEBUG_NETERR, \
					printf("NET_VOP_GETATTR(%s, line %d): network error %d %s %s\n", \
						__FILE__, __LINE__, error, \
						(opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : \
						"BACKOP_NONBLOCK", retry ? "retry" : "")); \
				break; \
			case ESTALE: \
			default: \
				retry = 0; \
		} \
	} while (retry); \
}

#define NET_VOP_GETATTR(vp, attrp, flag, credp, opmode, error, status) { \
	int retry = 0; \
	do { \
		CACHEFS_STATS->cs_backvnops++; \
		CACHEFS_STATS->cs_backops.cb_get++; \
		VOP_GETATTR(vp, attrp, flag, credp, error); \
		switch (error) { \
			case 0: \
				status = BACKOP_SUCCESS; \
				retry = 0; \
				break; \
			case ETIMEDOUT: \
			case ENETDOWN: \
			case ENETUNREACH: \
			case ECONNREFUSED: \
			case ENOBUFS: \
			case ECONNABORTED: \
			case ENETRESET: \
			case ECONNRESET: \
			case ENONET: \
			case ESHUTDOWN: \
			case EHOSTDOWN: \
			case EHOSTUNREACH: \
				if (opmode == BACKOP_BLOCK) { \
					retry = 1; \
				} else { \
					retry = 0; \
				} \
				status = BACKOP_NETERR; \
				CACHEFS_STATS->cs_neterror++; \
				CFS_DEBUG(CFSDEBUG_NETERR, \
					printf("NET_VOP_GETATTR(%s, line %d): network error %d %s %s\n", \
						__FILE__, __LINE__, error, \
						(opmode == BACKOP_BLOCK) ? "BACKOP_BLOCK" : \
						"BACKOP_NONBLOCK", retry ? "retry" : "")); \
				break; \
			case ESTALE: \
			default: \
				status = BACKOP_FAILURE; \
				retry = 0; \
		} \
	} while (retry); \
}

#define NET_VFS_STATVFS(vfsp, sbp, vp, opmode, error, status) { \
	int retry = 0; \
	do { \
		CACHEFS_STATS->cs_backvfsops++; \
		VFS_STATVFS(vfsp, sbp, vp, error); \
		switch (error) { \
			case 0: \
				status = BACKOP_SUCCESS; \
				retry = 0; \
				break; \
			case ETIMEDOUT: \
			case ENETDOWN: \
			case ENETUNREACH: \
			case ECONNREFUSED: \
			case ENOBUFS: \
			case ECONNABORTED: \
			case ENETRESET: \
			case ECONNRESET: \
			case ENONET: \
			case ESHUTDOWN: \
			case EHOSTDOWN: \
			case EHOSTUNREACH: \
				if (opmode == BACKOP_BLOCK) { \
					retry = 1; \
				} else { \
					retry = 0; \
				} \
				status = BACKOP_NETERR; \
				CACHEFS_STATS->cs_neterror++; \
				CFS_DEBUG(CFSDEBUG_NETERR, \
					printf("NET_VFS_STATVFS(%s, line %d): network error %d %s %s\n", \
						__FILE__, __LINE__, error, \
						(opmode == BACKOP_BLOCK) ? \
						"BACKOP_BLOCK" : "BACKOP_NONBLOCK", \
						retry ? "retry" : "")); \
				break; \
			case ESTALE: \
			default: \
				status = BACKOP_FAILURE; \
				retry = 0; \
		} \
	} while (retry); \
}

/*
 * One cache_label structure per cache. Contains mainly user defined or
 * default values for cache resource management. Contents is static.
 */
struct cache_label {
	u_int	cl_cfsversion:16;/* cfs version number */
	u_int	cl_cfslongsize:8;/* size of type long for which cache was created */
	u_int	cl_cfsmetasize:8;/* size of metadata structure in file headers */
	int		cl_minio;		/* minimum direct I/O transfer size */
	int		cl_align;		/* direct I/O alignment */
	int		cl_maxblks;	/* max blocks to be used by cache */
	int		cl_blkhiwat;	/* high water-mark for block usage */
	int		cl_blklowat;	/* low water-mark for block usage */
	int		cl_maxfiles;	/* max front files/dirs to be used by cache */
	int		cl_filehiwat;	/* high water-mark for inode usage */
	int		cl_filelowat;	/* low water-mark for indoe usage */
};

/*
 * struct cache contains cache-wide information, and provides access
 * to lower level info. There is one cache structure per cache.
 */
struct cachefscache {
	struct cachefscache	*c_next;	/* list of caches */
	u_int			c_flags;	/* misc flags */
	char				*c_name;
	struct cache_label	c_label;	/* cache resource info */
	struct vnode		*c_labelvp;	/* label file vp */
	struct vnode		*c_dirvp;	/* cache directory vp */
	int					c_syncs;	/* number of syncs in progress */
	int					c_refcnt;	/* active fs ref count */
	struct fscache		*c_fslist;	/* fscache list head */
	struct cachefs_workq	c_workq;	/* async work */
	lock_t			c_contentslock; /* protect cache struct */
	mutex_t			c_fslistlock;	/* protect fscache list */
	off_t			c_bsize;	/* block size to use for front file system */
	off_t			c_bmapsize;	/* block size to use for bmap */
	sv_t			c_replacement_sv;
	sv_t			c_repwait_sv;
#if defined(DEBUG) && defined(_CACHE_TRACE)
	cfstrace_t		c_trace;	/* cachefscache trace records */
#endif /* DEBUG && _CACHE_TRACE */
};

/*
 * Various cache structure flags.
 */
#define CACHE_GCDAEMON_HALT		0x01	/* replacement daemon should exit */
#define CACHE_GCDAEMON_ACTIVE	0x02	/* replacement daemon is active */
#define CACHE_GARBAGE_COLLECT	0x04	/* replacement is needed */
#define CACHE_GCDAEMON_WAITING	0x08	/* replacement daemon is waiting */

/*
 * Values for the cachefs options flag.
 */
/*
 * Mount options
 */
#define	CFS_WRITE_AROUND	0x001	/* write-around */
#define	CFS_DUAL_WRITE		0x002	/* write to cache and back file */
#define	CFS_NOCONST_MODE	0x008	/* no-op consistency mode */
#define	CFS_ACCESS_BACKFS	0x010	/* pass VOP_ACCESS to backfs */
#define	CFS_PURGE			0x020	/* force purge of cache */
#define CFS_DISCONNECT		0x040	/* disconnected operation */
#define CFS_NONBLOCK		0x080	/* non-blocking for all back FS ops */
#define CFS_ADD_BACK		0x100	/* add back FS */
#define CFS_PRIVATE			0x200	/* do file and record locking locally */
#define CFS_NO_BACKFS		0x400	/* no back file system mounted */

/*
 * cachefs mount structure and related data
 */
struct cachefs_mountargs {
	u_int			cfs_options;	/* consistency modes, etc. */
	char			*cfs_cacheid;	/* CFS fscdir name */
	char			*cfs_cachedir;	/* path for this cache dir */
	char			*cfs_backfs;	/* back filesystem dir */
	u_int			cfs_acregmin;	/* same as nfs values */
	u_int			cfs_acregmax;
	u_int			cfs_acdirmin;
	u_int			cfs_acdirmax;
};

#ifdef _KERNEL
/*
 * IRIX 5 mount args
 */
struct irix5_cachefs_mountargs {
	u_int			cfs_options;	/* consistency modes, etc. */
	app32_ptr_t		cfs_cacheid;	/* CFS fscdir name */
	app32_ptr_t		cfs_cachedir;	/* path for this cache dir */
	app32_ptr_t		cfs_backfs;		/* back filesystem dir */
	u_int			cfs_acregmin;	/* same as nfs values */
	u_int			cfs_acregmax;
	u_int			cfs_acdirmin;
	u_int			cfs_acdirmax;
};
#endif /* _KERNEL */

/*
 * replaceent arguments, etc.
 */

/*
 * replacement list entries
 */
struct cachefs_replace {
	char	*rep_cacheid;			/* cache ID for purged file */
	char	*rep_path;				/* path name of front file */
};
typedef struct cachefs_replace cachefsrep_t;

struct cachefsrepl_arg {
	char			*ra_cachedir;
	int				ra_ents;
	cachefsrep_t	*ra_list;
	int				ra_timeout;
};
typedef struct cachefsrepl_arg replarg_t;

#ifdef _KERNEL
struct irix5_cachefs_replace {
	app32_ptr_t	rep_cacheid;			/* cache ID for purged file */
	app32_ptr_t	rep_path;				/* path name of front file */
};
typedef struct irix5_cachefs_replace irix5_cachefsrep_t;

struct irix5_cachefsrepl_arg {
	app32_ptr_t		ra_cachedir;
	int				ra_ents;
	app32_ptr_t		ra_list;
	int				ra_timeout;
};
typedef struct irix5_cachefsrepl_arg irix5_replarg_t;
#endif /* _KERNEL */

/*
 * checkfid args, etc.
 */
struct checkfidarg {
	dev_t		cf_fsid;			/* the dev num of the mounted file system */
	fid_t		cf_fid;				/* the file ID to be checked */
};
typedef struct checkfidarg checkfidarg_t;

#if _KERNEL
enum lockmode { READLOCKED, WRITELOCKED, UNLOCKED, NOLOCK };
typedef enum lockmode lockmode_t;

/*
 * struct cachefsops - consistency modules.
 */
struct cachefsops {
	int		(* co_init_cobject)(struct fscache *, struct cnode *, vattr_t *,
				cred_t *);
	int		(* co_check_cobject)(struct fscache *, struct cnode *, vattr_t *,
				cred_t *, lockmode_t);
	void	(* co_modify_cobject)(struct fscache *, struct cnode *, cred_t *);
	void	(* co_invalidate_cobject)(struct fscache *, struct cnode *,
				cred_t *);
	void	(* co_expire_cobject)(struct fscache *, struct cnode *, cred_t *);
};
#endif /* _KERNEL */

/*
 * Stuff for the file header.
 */

#define FILEHEADER_SIZE	BBSIZE

/*
 * Cache consistency mechanism token size.  This size is expressed in
 * long units.  It is expressed this way for alignment purposes.
 */
#define MAXTOKENSIZE	4

typedef long		ctoken_t[MAXTOKENSIZE];

/*
 * File block allocation map entry.
 * File allocation accounting is done in the number of units specified in
 * the metadata.  This value is derived from what the front FS said was its
 * preferred I/O size.  Specifically, it is the preferred I/O size divided
 * by BBSIZE (i.e., the number of basic blocks per preferred I/O size).
 */
struct allocation_map {
	off_t		am_start_off;	/* starting offset of contiguous bytes */
	off_t		am_size;		/* number of contiguous bytes */
};
typedef struct allocation_map allocmap_t;

/*
 * Cachefs file attributes.
 */
struct cattr {
#if _KERNEL
	vtype_t		ca_type;	/* vnode type (for create) */
#else
	int		ca_type;
#endif
	__uint64_t	ca_mode;	/* file access mode */
	__uint64_t	ca_uid;		/* owner user id */
	__uint64_t	ca_gid;		/* owner group id */
	__uint64_t	ca_fsid;	/* file system id */
	__uint64_t	ca_nodeid;	/* node id */
	__uint64_t	ca_nlink;	/* number of references to file */
	__uint64_t	ca_size;	/* file size in bytes */
	timespec_t	ca_atime;	/* time of last access */
	timespec_t	ca_mtime;	/* time of last modification */
	timespec_t	ca_ctime;	/* time file ``created'' */
	__uint64_t	ca_rdev;	/* device the file represents */
	__uint64_t	ca_nblocks;	/* # of blocks allocated */
	__uint64_t	ca_vcode;	/* version code */
};
typedef struct cattr cattr_t;

#define VATTR_TO_CATTR(VP, VA, CA, FL)	{ \
	(CA)->ca_type = (VA)->va_type; \
	(CA)->ca_mode = (__uint64_t)(VA)->va_mode; \
	(CA)->ca_uid = (__uint64_t)(VA)->va_uid; \
	(CA)->ca_gid = (__uint64_t)(VA)->va_gid; \
	(CA)->ca_fsid = (__uint64_t)(VA)->va_fsid; \
	(CA)->ca_nodeid = (__uint64_t)(VA)->va_nodeid; \
	(CA)->ca_nlink = (__uint64_t)(VA)->va_nlink; \
	(CA)->ca_atime = (VA)->va_atime; \
	(CA)->ca_mtime = (VA)->va_mtime; \
	(CA)->ca_ctime = (VA)->va_ctime; \
	(CA)->ca_rdev = (__uint64_t)(VA)->va_rdev; \
	(CA)->ca_nblocks = (__uint64_t)(VA)->va_nblocks; \
	(CA)->ca_vcode = (__uint64_t)(VA)->va_vcode; \
	if ((FL) & AT_SIZE) (CA)->ca_size = (__uint64_t)(VA)->va_size; \
        if (MANDLOCK(VP, (CA)->ca_mode) != ((VP)->v_flag & VENF_LOCKING)) \
                VOP_VNODE_CHANGE(VP, VCHANGE_FLAGS_ENF_LOCKING, \
                                 MANDLOCK(VP, (CA)->ca_mode)); \
}

#define CATTR_TO_VATTR(CA, VA)	{ \
	bzero(VA, sizeof(vattr_t)); \
	(VA)->va_type = (CA)->ca_type; \
	(VA)->va_mode = (mode_t)(CA)->ca_mode; \
	(VA)->va_uid = (uid_t)(CA)->ca_uid; \
	(VA)->va_gid = (gid_t)(CA)->ca_gid; \
	(VA)->va_fsid = (dev_t)(CA)->ca_fsid; \
	(VA)->va_nodeid = (ino_t)(CA)->ca_nodeid; \
	(VA)->va_nlink = (nlink_t)(CA)->ca_nlink; \
	(VA)->va_size = (off_t)(CA)->ca_size; \
	(VA)->va_atime = (CA)->ca_atime; \
	(VA)->va_mtime = (CA)->ca_mtime; \
	(VA)->va_ctime = (CA)->ca_ctime; \
	(VA)->va_rdev = (dev_t)(CA)->ca_rdev; \
	(VA)->va_nblocks = (blkcnt_t)(CA)->ca_nblocks; \
	(VA)->va_vcode = (u_long)(CA)->ca_vcode; \
}

/*
 * File state and attributes.
 * This structure has been constructed so that the first four fields may
 * be accessed by user programs regardless of what size long those programs
 * use.  The size of this initial segment is 44 bytes.
 */
struct cachefs_metadata {
	u_int		md_checksum;		/* file header checksum */
	u_short		md_state;			/* file state (see flags below) */
	u_short		md_allocents;		/* number of valid allocation map entries */
	fid_t		md_backfid;			/* back file ID */
	ctoken_t	md_token;			/* cache consistency token */
	cattr_t		md_attributes;		/* file attributes from the back FS */
};
typedef struct cachefs_metadata cachefsmeta_t;

/*
 * File state flags.
 */
#define MD_POPULATED		0x0001	/* the file has been populated */
#define MD_MDVALID			0x0002	/* the file metadata contents are valid */
#define MD_NOALLOCMAP		0x0004	/* file/symlink data is in allocation map */
#define MD_KEEP				0x0010	/* do not discard the metadata */
#define MD_INIT				0x0020	/* initial metadata */
#define MD_NOTRUNC			0x0040	/* filenames are not truncated */

#define VALID_MD_STATE(state)	(((state) & ~(MD_POPULATED | MD_MDVALID | MD_NOALLOCMAP | MD_KEEP | MD_INIT | MD_NOTRUNC)) == 0)

/*
 * The allocation map size is dependent upon the size of cachefsmeta_t.
 * The entire file header must fit into FILEHEADER_SIZE bytes.  There is
 * also the constraint that the allocation map must be 8-byte aligned.
 */
#define OFFSET_MASK		(sizeof(off_t) - 1)
#define ALLOCMAP_SIZE	\
	((FILEHEADER_SIZE - ((sizeof(cachefsmeta_t) + OFFSET_MASK) & ~OFFSET_MASK)) / sizeof(allocmap_t))

struct file_header {
	cachefsmeta_t	fh_metadata;
	allocmap_t		fh_allocmap[ALLOCMAP_SIZE];
};
typedef struct file_header fileheader_t;

/*
 * the file header occupies a block of minimum size defined by the minimum
 * direct I/O transfer size
 * file data starts after the file header
 */
#define FILEHEADER_BLOCK_SIZE(cachep)	FILEHEADER_SIZE
#define DATA_START(cachep)				FILEHEADER_BLOCK_SIZE(cachep)
#define DIO_ALIGNMENT(cachep)			((cachep)->c_label.cl_align)
#define DIO_MINIO(cachep)				((cachep)->c_label.cl_minio)

/*
 * cnode structure, one per file. Cnode hash bucket kept in fscache
 * structure below.
 */

#if _KERNEL
/*
 * LOCKS:	rwlock		Read / Write serialization
 *		statelock	Protects other fields
 */
struct cnode {
	int					c_flags;
	vtype_t				c_type;
	int					c_refcnt;
	struct cnode		*c_hash;	/* hash list pointer */
	struct cnode		*c_hash_by_fid;	/* hash list pointer */
	struct cnode		*c_dcp;		/* parent directory cnode */
	struct vnode		*c_frontvp;	/* front vnode pointer */
	struct vnode		*c_frontdirvp;	/* front dir vnode pointer */
	struct vnode		*c_backvp;	/* back vnode pointer */
	struct vnode		*c_vnode;	/* vnode itself */
	bhv_desc_t			c_bhv_desc;	/* CacheFS behavior */
	fid_t				c_frontfid;	/* front file ID */
	fid_t				c_frontdirfid;	/* front directory ID */
	fid_t				*c_backfid;		/* back file ID/hash key */
	fileheader_t		*c_fileheader;	/* file header data */
	cattr_t				*c_attr;	/* cached attributes */
	off_t				c_size;		/* client view of the size */
	ctoken_t			*c_token;		/* consistency token */
	int					c_error;
	off_t				c_written;	/* pending write byte count */
	int					c_nio;		/* Number of io's pending */
	u_int				c_ioflags;
	cred_t				*c_cred;
	fscache_t			*c_fscache;	/* pointer to fscache structure for cnode */
	sv_t				c_iosv;		/* IO sync var. */
	krwlock_t			c_rwlock;	/* serialize lock */
	lock_t				c_statelock;	/* statelock */
	lock_t				c_iolock;
	sv_t				c_popwait_sv;
	sv_t				c_valloc_wait;
#ifdef DEBUG
#ifdef _CNODE_TRACE
	cfstrace_t			c_trace;		/* cnode tracing */
#endif
	u_int				c_inhash;		/* in or out of hash indicator */
	u_int				c_infidhash;	/* in or out of fid hash indicator */
#endif /* DEBUG */
};

typedef struct cnode cnode_t;

/*
 * Conversion macros
 */
#define	BHVTOC(BDP)		((struct cnode *)BHV_PDATA(BDP))
#define	CTOBHV(CP)		(&((CP)->c_bhv_desc))
#define	CTOV(CP)		((vnode_t *)((void *)(((CP)->c_vnode))))
#define	BHVTOFSCACHE(VFSP)	((struct fscache *)BHV_PDATA(bdp))
#define	C_TO_FSCACHE(CP)	((CP)->c_fscache)
#define C_DIRTY(CP)		(VN_DIRTY(CTOV(CP)) || ((CP)->c_written != 0))

#define CN_HOLD(CP)		atomicAddInt(&(CP)->c_refcnt, 1);
#define CN_RELE(CP)		atomicAddInt(&(CP)->c_refcnt, -1);

#define VALID_CACHEFS_VNODE(vp)	\
	(((vp)->v_type != VNON) && ((vp)->v_op == &cachefs_vnodeops) && (vp)->v_vfsp && (vp)->v_data && ((void *)VTOC(vp)->c_fscache == (vp)->v_vfsp->vfs_data))

/*
 * Various flags stored in the flags field of the cnode structure.
 */
#define	CN_NOCACHE				0x0001	/* the object is not being cached */
#define	CN_DESTROY				0x0002	/* destroy cnode when inactive */
#define	CN_ROOT					0x0004	/* root of the file system */
#define	CN_INVAL				0x0008	/* object invalidation in progress */
#define	CN_MODIFIED				0x0010	/* Object has been written to */
#define CN_UPDATED				0x0020	/* metadata has been updated */
#define CN_POPULATION_PENDING	0x0040	/* population in progress */
#define CN_SYNC					0x0080	/* cnode is being synced */
#define CN_UPDATE_PENDING		0x0100	/* metadata update in progress */
#define CN_NEEDINVAL			0x0200	/* need to do object invalidation */
#define CN_REPLACE				0x0400	/* front file removed by replacement */
#define CN_WRITTEN				0x0800	/* file written to since last sync */
#define CN_FRLOCK				0x1000	/* file and record locking performed */
#define CN_RECLAIM				0x2000	/* cnode is being reclaimed */
#define CN_VALLOC				0x4000	/* vnode allocation in progress */

#define C_CACHING(CP)	(!((CP)->c_flags & CN_NOCACHE) && \
	(CP)->c_frontfid.fid_len)

/*
 * io flags (in c_ioflag)
 */
#define	CIO_ASYNCWRITE	0x1		/* async write pending: off==0, len==0 */
#define	CIO_ASYNCREAD	0x2		/* async read pending: off==0, len==0 */

#define	CHASH(FILENO) \
	((int)(((((FILENO) >> 22) & 0x7ff) ^ (((FILENO) >> 11) & 0x7ff) ^ \
		((FILENO) & 0x7ff)) % (CNODE_BUCKET_SIZE)))

#define FID_MATCH(fp1, fp2) \
	(((fp1)->fid_len == (fp2)->fid_len) && \
	(bcmp((caddr_t)(fp1)->fid_data, (caddr_t)(fp2)->fid_data, \
		(fp1)->fid_len) == 0))

extern int cachefs_max_threads;

#define	CFS_ASYNC_TIMEOUT	60

enum cachefs_cmd {
	CFS_POPULATE_DIR,
	CFS_POPULATE_FILE,
	CFS_CACHE_SYNC,
	CFS_ASYNCWRITE,
	CFS_ASYNCREAD,
	CFS_NOOP
};

struct cachefs_fs_sync_req {
	struct cachefscache *cf_cachep;
};

struct cachefs_popfile_req {
	cnode_t *cpf_cp;
};

struct cachefs_popdir_req {
	cnode_t *cpd_cp;
};

struct cachefs_io_req {
	cnode_t *cp_cp;
	struct buf	*cp_buf;
	int cp_flags;
};

struct cachefs_req {
	struct cachefs_req	*cfs_next;
	enum cachefs_cmd	cfs_cmd;	/* Command to execute */
	cred_t *cfs_cr;
	union {
		struct cachefs_fs_sync_req cu_fs_sync;
		struct cachefs_io_req cu_io;
		struct cachefs_popfile_req cu_popfile;
		struct cachefs_popdir_req cu_popdir;
	} cfs_req_u;
};

enum backop_stat {BACKOP_SUCCESS, BACKOP_NETERR, BACKOP_FAILURE};
#endif /* _KERNEL */

/*
 * statistics
 */
struct cachefs_stats {
	u_int	cs_inval;			/* invalidations */
	u_int	cs_nocache;			/* nocache */
	u_int	cs_reclaims;		/* reclaims */
	u_int	cs_dnlchit;			/* dnlc lookup hit */
	u_int	cs_shorthit;		/* lookup shortcut hits */
	u_int	cs_shortmiss;		/* lookup shortcut miss */
	u_int	cs_lookuphit;		/* cache lookup hits */
	u_int	cs_lookupstale;		/* stale lookup hits */
	u_int	cs_lookupmiss;		/* cache lookup miss */
	u_int	cs_nocachelookup;	/* nocache lookups */
	u_int	cs_lookups;			/* total number of lookups */
	u_int	cs_readhit;			/* data read hit */
	u_int	cs_readmiss;		/* data read miss */
	u_int	cs_reads;			/* total reads at strategy level */
	u_int	cs_nocachereads;	/* nocache reads */
	u_int	cs_readerrors;		/* read errors */
	u_int	cs_writes;			/* total writes at strategy level */
	u_int	cs_nocachewrites;	/* nocache writes */
	u_int	cs_writeerrors;		/* write errors */
	u_int	cs_readdirhit;		/* readdir hits */
	u_int	cs_readdirmiss;		/* readdir miss */
	u_int	cs_readdirs;		/* total readdirs */
	u_int	cs_vnops;			/* vnode operations */
	u_int	cs_vfsops;			/* vfs operations */
	u_int	cs_objinits;		/* object initializations */
	u_int	cs_objmods;			/* object modification */
	u_int	cs_objinvals;		/* object invalidations */
	u_int	cs_objexpires;		/* explicit object expirations */
	u_int	cs_objchecks;		/* object checks */
	u_int	cs_backchecks;		/* object checks to back FS */
	u_int	cs_newcnodes;		/* count of cnode creations */
	u_int	cs_makecnode;		/* count of calls to makecachefsnode */
	u_int	cs_nocnode;			/* count of ENOENT returns for shortcut */
	u_int	cs_cnodehit;		/* cnodes found by makecachefsnode */
	u_int	cs_cnoderestart;	/* count for number of search restarts */
	u_int	cs_cnodelookagain;	/* count for number of second searches */
	u_int	cs_cnodetoss;		/* cnodes tossed due to duplication */
	u_int	cs_neterror;		/* count of network errors */
	u_int	cs_fronterror;		/* front file errors */
	u_int	cs_shortrdlnk;		/* fast readlink from metadata */
	u_int	cs_longrdlnk;		/* long readlink */
	u_int	cs_backvnops;		/* back FS vnode op calls */
	u_int	cs_backvfsops;		/* back FS vnode op calls */
	u_int	cs_getbackvp;		/* get back vnode requests */
	u_int	cs_asyncreqs;		/* async operation reuests */
	u_int	cs_replacements;	/* replacement requests */
	struct {					/* bad file headers */
		u_int	cbh_checksum;	/* bad checksum */
		u_int	cbh_readerr;	/* read error */
		u_int	cbh_data;		/* invalid header data */
		u_int	cbh_short;		/* short file header */
	}		cs_badheader;
	struct {					/* cnode race collision statistics */
		u_int	cr_reclaim;		/* collisions with reclaim */
		u_int	cr_destroy;		/* collisions with destruction */
	}		cs_race;
	struct {					/* back vnode operations */
		u_int	cb_look;		/* lookup */
		u_int	cb_fsy;			/* fsync */
		u_int	cb_rdl;			/* readlink */
		u_int	cb_acc;			/* access */
		u_int	cb_get;			/* getattr */
		u_int	cb_set;			/* setattr */
		u_int	cb_clo;			/* close */
		u_int	cb_opn;			/* open */
		u_int	cb_rdd;			/* readdir */
		u_int	cb_cre;			/* create */
		u_int	cb_rem;			/* remove */
		u_int	cb_lnk;			/* link */
		u_int	cb_ren;			/* rename */
		u_int	cb_mkd;			/* mkdir */
		u_int	cb_rmd;			/* rmdir */
		u_int	cb_sym;			/* symlink */
		u_int	cb_frl;			/* frlock */
		u_int	cb_rdv;			/* read */
		u_int	cb_wrv;			/* write */
	}		cs_backops;
	struct {					/* fileheader cache operations */
		u_int	cf_hits;		/* file header cache hits */
		u_int	cf_misses;		/* file header cache misses */
		u_int	cf_reads;		/* file header reads */
		u_int	cf_writes;		/* file header writes */
		u_int	cf_cacheenters;	/* new entries to the cache */
		u_int	cf_cacheremoves;/* removals of entries from the cache */
		u_int	cf_lruenters;	/* new entries lru */
		u_int	cf_lruremoves;	/* removals of entries from the lru */
		u_int	cf_purges;		/* purges */
		u_int	cf_releases;	/* releases */
	} cs_fileheaders;
};

/*
 * file header cache entry structure
 */
struct filheader_cache_entry {
	fileheader_t					*fce_header;	/* file header  */
	struct filheader_cache_entry	*fce_next;		/* next cache entry  */
	struct filheader_cache_entry	*fce_prev;		/* prev cache entry */
	struct filheader_cache_entry	*fce_lrunext;	/* next LRU entry */
	struct filheader_cache_entry	*fce_lruprev;	/* prev LRU entry */
	fid_t							*fce_fid;		/* front file ID */
	sv_t							fce_wait;		/* sync point */
	u_int							fce_ref;		/* reference count */
	u_int							fce_flags;		/* entry state flags */
	u_int							fce_slot;		/* cache slot */
#ifdef DEBUG
	void							*fce_caller;	/* caller address */
#endif /* DEBUG */
};

/*
 * cache entry flags
 */
#define ENTRY_NEW			0x01
#define ENTRY_DESTROY		0x02
#define ENTRY_VALID			0x04
#define ENTRY_LRU			0x08
#define ENTRY_CACHED		0x10

#if _KERNEL
#define CACHEFS_STATS	((struct cachefs_stats *)private.cfsstat)

extern int cachefsfstyp;

#define UIO_SETUP(uiop, iovp, base, len, offset, seg, mode, ulim) { \
	(iovp)->iov_base = base; \
	(iovp)->iov_len = len; \
	(uiop)->uio_iov = iovp; \
	(uiop)->uio_iovcnt = 1; \
	(uiop)->uio_offset = offset; \
	(uiop)->uio_segflg = seg; \
	(uiop)->uio_resid = len; \
	(uiop)->uio_limit = ulim; \
	(uiop)->uio_pmp = NULL; \
	(uiop)->uio_pio = 0; \
	(uiop)->uio_readiolog = 0; \
	(uiop)->uio_writeiolog = 0; \
	(uiop)->uio_pbuf = 0; \
	(uiop)->uio_sigpipe = 0; \
	(uiop)->uio_fmode = mode; \
}

#define READ_VP(vp, uiop, ioflag, cr, fl, error) { \
	VOP_RWLOCK(vp, VRWLOCK_READ); \
	VOP_READ(vp, uiop, ioflag | IO_ISLOCKED, cr, fl, error); \
	VOP_RWUNLOCK(vp, VRWLOCK_READ); \
}

#define WRITE_VP(vp, uiop, ioflag, cr, fl, error) { \
	VOP_RWLOCK(vp, \
		(ioflag & IO_DIRECT) ? VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE); \
	VOP_WRITE(vp, uiop, ioflag | IO_ISLOCKED, cr, fl, error); \
	VOP_RWUNLOCK(vp, \
		(ioflag & IO_DIRECT) ? VRWLOCK_WRITE_DIRECT : VRWLOCK_WRITE); \
}

#define FRONT_READ_VP(cp, uiop, ioflag, cr) \
	cachefs_frontop_readvp(cp, uiop, ioflag, cr)

#define FRONT_WRITE_VP(cp, uiop, ioflag, cr) \
	cachefs_frontop_writevp(cp, uiop, ioflag, cr)

#define BACK_READ_VP(cp, uiop, ioflag, cr, mode, errorp) \
	cachefs_backop_readvp(cp, uiop, ioflag, cr, mode, errorp)

#define BACK_WRITE_VP(cp, uiop, ioflag, cr, mode, errorp) \
	cachefs_backop_writevp(cp, uiop, ioflag, cr, mode, errorp)
#endif /* _KERNEL */

/*
 * cachefs function prototypes
 */
#if defined(_KERNEL) && defined(__STDC__)

/* cachefs_cnode.c */
void cachefs_remfidhash(struct cnode *cp);
void cachefs_remhash(struct cnode *);
cnode_t *cfind_by_fid(struct fid *, struct fscache *);
cnode_t *cfind(fid_t *, struct fscache *);
int cachefs_lookup_cnode( cnode_t *, char *, cnode_t **, cred_t * );
int cachefs_get_cnode_files( cnode_t *, vnode_t *, vnode_t *, vnode_t * );
int makecachefsnode( cnode_t *, fid_t *, struct fscache *, fileheader_t *,
	vnode_t *, vnode_t *, vnode_t *, vattr_t *, cred_t *, int,
	struct cnode ** );
void cachefs_cnode_free( struct cnode * );
int valid_file_header(fileheader_t *fhp, fid_t *backfid);
int valid_allocmap( cnode_t *cp );
#ifdef DEBUG
void validate_fileheader(cnode_t *cp, char *file, int line);
#endif

/* cachefs_fscache.c */
void fscache_destroy(fscache_t *);
fscache_t *fscache_create(cachefscache_t *, char *, struct cachefs_mountargs *,
	vnode_t *, vfs_t *);
void fscache_hold(fscache_t *);
void fscache_rele(fscache_t *);
void fscache_sync(struct fscache *, int);
void fscache_free_cnodes(struct fscache *);
fscache_t *fscache_list_find(cachefscache_t *, char *);
void fscache_list_add(cachefscache_t *, fscache_t *);
void fscache_list_remove(cachefscache_t *, fscache_t *);

/* cachefs_subr.c */
int activate_cache(cachefscache_t *);
void deactivate_cache(cachefscache_t *);
int cachefs_cache_create(char **, cachefscache_t **, vnode_t *);
void cachefs_cache_destroy(cachefscache_t *cachep);
void cachefs_cache_sync(struct cachefscache *cachep);
void cachefs_do_req(struct cachefs_req *);
int cachefs_cnode_cnt(int);
int cachefs_nocache(cnode_t *);
int cachefs_inval_object(cnode_t *, lockmode_t);
int cachefs_async_halt(struct cachefs_workq *);
int cachefs_check_allocmap(cnode_t *, off_t, off_t);
int cachefs_update_allocmap(cnode_t *, off_t, off_t);
u_long cachefs_gettime_cached_object(struct fscache *, vtype_t, u_long);
cachefscache_t *cachefscache_list_find(char *);
int cachefs_populate_dir(cnode_t *cp, cred_t *cr);
int cachefsio_write(struct cnode *cp, struct buf *bp, cred_t *cr);
int cachefsio_read(struct cnode *cp, struct buf *bp, cred_t *cr);
void cachefs_workq_init(struct cachefs_workq *);
void cachefs_workq_destroy(struct cachefs_workq *);
int cachefs_addqueue(struct cachefs_req *, struct cachefs_workq *);
void cachefs_inactivate(struct cnode *);
int cachefs_flushvp(bhv_desc_t *, off_t, size_t, int, uint64_t);
int cachefs_async_start(cachefscache_t *);
void cachefs_async_daemon(struct cachefs_workq *);
void make_ascii_name(fid_t *fidp, char *strp);
u_int fidhash(struct fid *, u_int);
#ifdef DEBUG
int find_fileheader_by_fid(fid_t *, fileheader_t *);
void idbg_headercache(__psint_t);
void idbg_checklru(__psint_t);
#endif /* DEBUG */
void cachefs_mem_check(void);
fileheader_t *alloc_fileheader(vnode_t *vp);
void release_fileheader(fid_t *fidp, fileheader_t *fhp);
int cachefs_write_file_header(cnode_t *, vnode_t *, fileheader_t *, int, int);
int cachefs_write_header(cnode_t *cp, int force);
int cachefs_read_file_header(vnode_t *, fileheader_t **, vtype_t,
	cachefscache_t *);
int cachefs_lookup_frontdir(vnode_t *, fid_t *, vnode_t **);
int cachefs_create_frontdir(cachefscache_t *, vnode_t *, fid_t *, vnode_t **);
int cachefs_create_frontfile(cachefscache_t *, vnode_t *, char *, vnode_t **,
	fid_t *, fileheader_t **, vtype_t);
int cachefs_remove_frontfile(vnode_t *dvp, char *nm);
int cachefs_remove_frontdir(fscache_t *fscp, fid_t *backfid, int);
int cachefs_update_directory(cnode_t *dcp, int flag, cred_t *cr);
int cachefs_irix5_fmtdirent(void **, struct dirent *, int);
int await_population(cnode_t *);
int cachefs_search_dir(cnode_t *dcp, char *nm, lockmode_t lm);
int cachefs_sys(int, void *, rval_t *);
#ifdef DEBUG
int cachefs_name_is_legal(char *);
int valid_dirents( dirent_t *dep, int dircount );
#endif

/* cachefs_resource.c */
int cachefs_allocblocks(cachefscache_t *, int, cred_t *);
int cachefs_allocfile(cachefscache_t *);
void cachefs_replacement_halt(cachefscache_t *);
int cachefs_replacement(void *, rval_t *);
int cachefs_reconnect(void *, rval_t *);
int cachefs_checkfid(void *, rval_t *);
int cachefs_replace(cachefscache_t *cachep, int wait_for_replace);

/* cachefs_vnops.c */
int cachefs_fsync(bhv_desc_t *, int, cred_t *, off_t, off_t);

/* cachefs_vfsops.c */
int cachefs_complete_mount(void *, rval_t *);
dev_t make_cachefs_fsid(fscache_t *, dev_t);
void release_minor_number(int);

extern zone_t *Cachefs_fileheader_zone;
extern zone_t *Cachefs_attr_zone;
extern zone_t *Cachefs_cnode_zone;
extern zone_t *Cachefs_fid_zone;
extern zone_t *Cachefs_path_zone;
extern zone_t *Cachefs_fhcache_zone;
extern zone_t *Cachefs_req_zone;
extern zone_t *Cachefs_fsidmap_zone;

extern mutex_t cachefs_cachelock;
extern vattr_t Cachefs_file_attr;
extern vattr_t Cachefs_dir_attr;

extern int replacement_timeout;

/* cachefs_backops.c */
enum backop_stat cachefs_backop_getattr(cnode_t *, vattr_t *, int, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_readdir(cnode_t *, uio_t *, cred_t *, int *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_setattr(cnode_t *, vattr_t *, int, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_access(cnode_t *, int, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_readlink(cnode_t *, uio_t *, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_lookup_back(cnode_t *, char *, struct vnode **,
	fid_t **, int, struct vnode *, cred_t *, enum backop_mode, int *);
enum backop_stat cachefs_backop_create(cnode_t *, char *, vattr_t *,
	int, int, vnode_t **, cred_t *, enum backop_mode, int *);
enum backop_stat cachefs_backop_remove(cnode_t *, char *, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_link(cnode_t *, cnode_t *, char *, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_rename(cnode_t *, char *, cnode_t *, char *,
	struct pathname *, cred_t *, enum backop_mode, int *);
enum backop_stat cachefs_backop_mkdir(cnode_t *, char *, vattr_t *, vnode_t **,
	cred_t *, enum backop_mode, int *);
enum backop_stat cachefs_backop_rmdir(cnode_t *, char *, vnode_t *, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_symlink(cnode_t *, char *, vattr_t *, char *,
	cred_t *, enum backop_mode, int *);
enum backop_stat cachefs_backop_frlock(cnode_t *, int, struct flock *, int,
	off_t, cred_t *, enum backop_mode, vrwlock_t, int *);
enum backop_stat cachefs_backop_readvp(cnode_t *, uio_t *, int, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_writevp(cnode_t *, uio_t *, int, cred_t *,
	enum backop_mode, int *);
enum backop_stat cachefs_getbackvp(fscache_t *fscp, fid_t *backfid,
	vnode_t **vpp, enum backop_mode, int *);
enum backop_stat cachefs_backop_statvfs(fscache_t *, struct statvfs *,
	enum backop_mode, int *);
enum backop_stat cachefs_backop_fcntl(cnode_t *cp, int cmd, void *arg,
	int flags, off_t offset, cred_t *cr, rval_t *rvp, enum backop_mode opmode,
	int *errorp);
enum backop_stat cachefs_backop_vnode_change(cnode_t *, vchange_t, __psint_t,
	enum backop_mode);
enum backop_stat cachefs_backop_attr_get(cnode_t *, char *, char *, int *,
					 int, cred_t *, enum backop_mode,
					 int *);
enum backop_stat cachefs_backop_attr_set(cnode_t *, char *, char *, int,
					 int, cred_t *, enum backop_mode,
					 int *);
enum backop_stat cachefs_backop_attr_remove(cnode_t *, char *, int, cred_t *,
					    enum backop_mode, int *);
struct attrlist_cursor_kern;
enum backop_stat cachefs_backop_attr_list(cnode_t *, char *, int, int,
					  struct attrlist_cursor_kern *,
					  cred_t *, enum backop_mode, int *);

/* cachefs_frontops.c */
int cachefs_frontop_getattr(cnode_t *, vattr_t *, int, cred_t *);
int cachefs_frontop_setattr(cnode_t *, vattr_t *, int, cred_t *);
int cachefs_frontop_fsync(cnode_t *, int, cred_t *, off_t, off_t);
int cachefs_frontop_link(cnode_t *, cnode_t *, char *, cred_t *);
int cachefs_frontop_readvp(cnode_t *, uio_t *, int, cred_t *);
int cachefs_frontop_writevp(cnode_t *, uio_t *, int, cred_t *);
int cachefs_frontop_statvfs(fscache_t *, struct statvfs *);
int cachefs_getfrontdirvp(struct cnode *cp);
int cachefs_getfrontvp(struct cnode *cp);
int cachefs_frontop_lookup(cnode_t *, char *, struct vnode **,
	struct pathname *, int, struct vnode *, cred_t *);
int cachefs_frontop_remove(cnode_t *, char *, cred_t *);
int cachefs_frontop_rename(cnode_t *, char *, cnode_t *, char *,
	struct pathname *, cred_t *);
int cachefs_frontop_fid(cnode_t *, struct fid **);
int cachefs_frontop_fid2(cnode_t *, struct fid *);

#ifdef DEBUG
struct km_wrap {
	int kw_size;
	int kw_req;
	void *kw_caller;
	struct km_wrap *kw_other;
	struct km_wrap *kw_next;
	struct km_wrap *kw_prev;
};

#define WRAPSIZE	((sizeof(struct km_wrap) + sizeof(void *) - 1) & \
	~(sizeof(void *) - 1))

extern struct km_wrap *Cachefs_memlist;
extern int cachefs_mem_usage;
extern int cachefs_zone_usage;

#define CACHEFS_KMEM_ALLOC(size, flag)	cachefs_kmem_alloc(kmem_alloc, size, \
	flag)
#define CACHEFS_KMEM_ZALLOC(size, flag)	cachefs_kmem_alloc(kmem_zalloc, size, \
	flag)
#define CACHEFS_KMEM_FREE(addr, len) \
	cachefs_kmem_free((caddr_t)addr, len); addr = NULL
#define CACHEFS_ZONE_ALLOC(zone, flag)	cachefs_zone_alloc(zone, flag)
#define CACHEFS_ZONE_ZALLOC(zone, flag)	cachefs_zone_zalloc(zone, flag)
#define CACHEFS_ZONE_FREE(zone, addr) \
	cachefs_zone_free(zone, (void *)addr) ; addr = NULL

extern void *cachefs_kmem_alloc(void *(*)(size_t, int), int, int);
extern void cachefs_kmem_free(caddr_t, int);
extern void *cachefs_zone_alloc(zone_t *, int);
extern void *cachefs_zone_zalloc(zone_t *, int);
extern void cachefs_zone_free(zone_t *, void *);
extern int cachefs_zone_validate(caddr_t, int);
extern int cachefs_kmem_validate(caddr_t, int);
#else /* DEBUG */

#define CACHEFS_KMEM_ALLOC(size, flag)	kmem_alloc(size, flag)
#define CACHEFS_KMEM_ZALLOC(size, flag)	kmem_zalloc(size, flag)
#define CACHEFS_KMEM_FREE(addr, len) \
	kmem_free((void *)addr, len) ; addr = NULL
#define CACHEFS_ZONE_ALLOC(zone, flag)	kmem_zone_alloc(zone, flag)
#define CACHEFS_ZONE_ZALLOC(zone, flag)	kmem_zone_zalloc(zone, flag)
#define CACHEFS_ZONE_FREE(zone, addr) \
	kmem_zone_free(zone, (void *)addr) ; addr = NULL

#endif /* DEBUG */

#define CACHEFS_RELEASE_FILEHEADER(fidp, fhp) \
	release_fileheader(fidp, fhp) ; fhp = NULL;

extern void cachefs_idbg_init(void);
#ifdef DEBUG
extern void cache_trace( int, cfstrace_t *, void *, long, long, long, int );
#endif /* DEBUG */

extern int fork(void *uap, rval_t *rvp);

#endif /* defined (_KERNEL) && defined (__STDC__) */



#ifdef DEBUG
#define	CFSDEBUG_ALL			0xffffffff	/* all debugging */
#define	CFSDEBUG_NONE			0x00000000	/* no debugging */
#define	CFSDEBUG_SUBR			0x00000001	/* debug cachefs_subr.c */
#define	CFSDEBUG_CNODE			0x00000002	/* debug cachefs_cnode.c */
#define	CFSDEBUG_RDWR			0x00000004	/* debug read/write ops */
#define	CFSDEBUG_VOPS			0x00000008	/* debug vnode ops */
#define	CFSDEBUG_VFSOP			0x00000010	/* debug vfs ops */
#define	CFSDEBUG_ERROR			0x00000020	/* debug front FS errors */
#define CFSDEBUG_FSCACHE		0x00000040	/* debug fscache ops */
#define CFSDEBUG_PROCS			0x00000080	/* debug processes */
#define CFSDEBUG_FILEHEADER		0x00000100	/* debug file header */
#define CFSDEBUG_STALE			0x00000200	/* debug ESTALE errors */
#define CFSDEBUG_RESOURCE		0x00000400
#define CFSDEBUG_POPULATE		0x00000800
#define CFSDEBUG_DIO			0x00001000	/* debug direct I/O */
#define CFSDEBUG_FLUSH			0x00002000
#define CFSDEBUG_READLINK		0x00004000
#define CFSDEBUG_BMAP			0x00008000
#define CFSDEBUG_CFSOPS			0x00010000
#define CFSDEBUG_LOOKUP			0x00020000
#define CFSDEBUG_POPDIR			0x00040000	/* debug directory population */
#define CFSDEBUG_REPLACEMENT	0x00080000	/* debug replacement */
#define	CFSDEBUG_DIR			0x00100000	/* debug directory ops */
#define CFSDEBUG_READDIR		0x00100000	/* debug readdir internal ops */
#define CFSDEBUG_INVAL			0x00200000
#define CFSDEBUG_ASYNC			0x00400000
#define CFSDEBUG_NETERR			0x00800000	/* debug network errors */
#define CFSDEBUG_NOCACHE		0x01000000
#define CFSDEBUG_ATTR			0x02000000
#define CFSDEBUG_VALIDATE		0x04000000	/* perform file header validation */
#define CFSDEBUG_WRITEHDR		0x08000000	/* debug file header writes */
#define CFSDEBUG_BACKOPS		0x10000000	/* debug back FS ops */
#define CFSDEBUG_SIZE			0x20000000	/* debug file size changes */
#define CFSDEBUG_ERROR2			0x40000000	/* debug non-zero errors */
#define CFSDEBUG_FRONTOPS		0x80000000	/* debug front FS ops */

#define CNTRACE_ALL			0xffffffff
#define CNTRACE_NONE		0x0

/*
 * trace IDs
 * all trace IDs must be unique and have no common bits
 */
#define CNTRACE_ACT			0x000001	/* trace inactive/reactive */
#define CNTRACE_WORKQ		0x000002	/* trace workq activity */
#define CNTRACE_UPDATE		0x000004	/* trace metadata updating */
#define CNTRACE_DESTROY		0x000008	/* trace CN_DESTROY */
#define CNTRACE_DIRHOLD		0x000010	/* trace parent dir VN_HOLD */
#define CNTRACE_HOLD		0x000080	/* trace VN_HOLD operations */
#define CNTRACE_WRITE		0x000100	/* trace writes */
#define CNTRACE_READ		0x000200	/* trace reads */
#define CNTRACE_SIZE		0x000400	/* trace size changes */
#define CNTRACE_ALLOCMAP	0x000800	/* trace allocmap changes */
#define CNTRACE_FILEHEADER	0x001000	/* trace changes to c_fileheader */
#define CNTRACE_ATTR		0x002000	/* trace changes to attributes */
#define CNTRACE_ALLOCENTS	0x004000	/* trace md_allocents changes */
#define CNTRACE_WRITEHDR	0x008000	/* trace header writes */
#define CNTRACE_DIOWRITE	0x010000	/* trace header writes */
#define CNTRACE_FSYNC		0x020000	/* trace fsync */
#define CNTRACE_LOOKUP		0x040000	/* trace lookup */
#define CNTRACE_NOCACHE     0x080000 	/* trace CN_NOCACHE */
#define CNTRACE_POPDIR		0x100000	/* trace dir populations */
#define CNTRACE_FRONTSIZE	0x200000	/* trace front file size changes */
#define CNTRACE_INVAL		0x400000	/* trace object invalidation */

extern int cnodetrace;

/*
 * must hold c_statelock as writer when using this macro
 */
#ifdef _CNODE_TRACE
#define CNODE_TRACE(id, cp, addr, v1, v2) \
	if ( cnodetrace & id) \
		cache_trace(id, &((cp)->c_trace), (void *)addr, (long)(v1), \
			(long)(v2), (long)0, __LINE__)
#else
#define CNODE_TRACE(id, cp, addr, v1, v2)
#endif

#define CACHETRACE_ALL		0xffffffff
#define CACHETRACE_NONE		0x0

extern int cachetrace;

/*
 * must hold c_contentslock when using this macro
 */
#ifdef _CACHE_TRACE
#define CACHE_TRACE(id, cachep, addr, v1, v2, v3) \
	if ( cachetrace & id) \
		cache_trace(id, &((cachep)->c_trace), (void *)addr, (long)(v1), \
			(long)(v2), (long)(v3), __LINE__)
#else
#define CACHE_TRACE(id, cp, addr, v1, v2, v3)
#endif

extern cfstrace_t Cachefs_functrace;
extern int cachefunctrace;

#define CACHEFS_FUNCTION	0x80000000

#define CFTRACE_ALL			0xffffffff
#define CFTRACE_NONE		0

#define CFTRACE_CREATE		0x00000001
#define CFTRACE_WRITEHDR	0x00000002
#define CFTRACE_WRITE		0x00000004
#define CFTRACE_ALLOC		0x00000008
#define CFTRACE_OTHER		0x80000000

#define CACHEFUNC_TRACE(id, addr, arg1, arg2, arg3) \
	if ( cachefunctrace & id ) \
		cache_trace(CACHEFS_FUNCTION, &Cachefs_functrace, (void *)addr, \
			(long)(arg1), (long)(arg2), (long)(arg3), __LINE__);

extern int cachefsdebug;

#define	CFS_DEBUG(N, OP)    if (cachefsdebug & (N)) OP

extern uint _ftext[];
extern uint _etext[];

/*
 * a valid address is not in kernel text and is not in page 0
 */
#define VALID_ADDR(addr)	((((u_long)(addr) > (u_long)_etext) || \
								((u_long)(addr) < (u_long)_ftext)) && \
								(pnum(addr) != 0))
#else /* DEBUG */
#define CFS_DEBUG(N, OP)
#define CNODE_TRACE(id, cp, addr, v1, v2)
#define CACHE_TRACE(id, cp, addr, v1, v2, v3)
#define CACHEFUNC_TRACE(id, addr, arg1, arg2, arg3)
#endif /* DEBUG */

#define	C_ISVDEV(t) ((t == VBLK) || (t == VCHR) || (t == VFIFO))

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FS_CACHEFS_FS_H */
