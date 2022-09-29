/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ifndef _FS_VNODE_H	/* wrapper symbol for kernel use */
#define _FS_VNODE_H	/* subject to change without notice */

#ident	"$Revision: 1.197 $"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sema.h>
#include <sys/uio.h>
#include <sys/repl_vnode.h>
#include <sys/vfs.h>
#include <ksys/behavior.h>

#include <ksys/vnode_pcache.h>
#include <sys/ckpt.h>

/*
 * The vnode is the focus of all file activity in UNIX.
 * A vnode is allocated for each active file, each current
 * directory, each mounted-on file, and the root.
 */

/*
 * vnode types.  VNON means no type.  These values are unrelated to
 * values in on-disk inodes.
 */
typedef enum vtype {
	VNON	= 0,
	VREG	= 1,
	VDIR	= 2,
	VBLK	= 3,
	VCHR	= 4,
	VLNK	= 5,
	VFIFO	= 6,
	VBAD	= 7,
	VSOCK	= 8
} vtype_t;

#define ISVDEV(t) \
	((t) == VCHR || (t) == VBLK || (t) == VFIFO)

/*
 * The prefix of a vnode struct is its freelist linkage.  The freelist
 * header has two pointers so we can insert at either end.
 */
typedef struct vnlist {
	struct vnode	*vl_next;
	struct vnode	*vl_prev;
} vnlist_t;

typedef uint64_t vnumber_t;

/*
 * Define the type of behavior head used by vnodes.  
 */
#ifdef VNODE_BHV_SYNCH
#define vn_bhv_head_t	bhv2_head_t
#else
#define vn_bhv_head_t	bhv_head_t
#endif

/*
 * MP locking protocols:
 *	v_flag, v_count, v_vfsmountedhere	VN_LOCK/VN_UNLOCK
 *	v_vfsmountedhere			VN_LOCK covered vnode before
 *						acquiring vfslock around any
 *						v_vfsmountedhere dereference
 *	v_vfsp					VN_LOCK/VN_UNLOCK
 *	v_stream				streams monitor
 *	v_pgcnt					mem_lock/mem_unlock
 *	v_type					read-only or fs-dependent
 *	v_filocks				vfilockmon, in reclock()
 *	v_list, v_hashp, v_hashn		freelist lock
 *	v_intpcount				VN_LOCK/VN_UNLOCK
 */
typedef struct vnode {
	struct vnlist	v_list;			/* freelist linkage */
	uint		v_flag;			/* vnode flags (see below) */
	int		v_count;		/* reference count */
	short		v_listid;		/* free list id */
	int		v_intpcount;		/* interp. refcount for imon */
	enum vtype	v_type;			/* vnode type */
	dev_t		v_rdev;			/* device (VCHR, VBLK) */
	struct vfs	*v_vfsmountedhere;	/* ptr to vfs mounted here */
	struct vfs	*v_vfsp;		/* ptr to containing VFS */
	struct stdata	*v_stream;		/* associated stream */
	struct filock	*v_filocks;		/* ptr to filock list */
	mutex_t		v_filocksem;		/* mutex for list */
	vnumber_t	v_number;		/* in-core vnode number */
	vn_bhv_head_t	v_bh;			/* behavior head */

	uint64_t	v_namecap;		/* name cache capability */

	/*
	 * Used only by global cache.
	 */
	struct vnode	*v_hashp;		/* hash list for lookup */
	struct vnode	*v_hashn;		/* hash list for lookup */

	/*
	 * Values manipulated only by VM and
	 * the page/buffer caches.  Note that v_traceix, which
         * has been shoehorned in here to use otherwise-unused space
	 * is an exception.
	 */
	struct pregion	*v_mreg;		/* mapped file region pointer */
	struct pregion	*v_mregb;		/* mapped file region pointer */
	pgno_t		v_pgcnt;		/* pages hashed to vnode */
	struct pfdat	*v_dpages;		/* delwri pages */
	unsigned int	v_dpages_gen;		/* delwri pages generation no */
	int		v_dbuf;			/* delwri buffer count */
	struct buf	*v_buf;			/* vnode buffer tree head */
	unsigned int	v_bufgen;		/* buf list generation number */
	unsigned int	v_traceix;              /* trace table index */
	mutex_t		v_buf_lock;		/* mutex for buffer tree */

	vnode_pcache_t	v_pc;			/* Page cache structure. 
						 * per vnode. Refer to
						 * vnode_pcache.h 
						 * for details.
						 */
#ifdef VNODE_TRACING
	struct ktrace 	*v_trace;		/* trace header structure    */
#endif /* VNODE_TRACING */
#ifdef CKPT
	ckpt_handle_t	v_ckpt;			/* ckpt lookup info */
#endif
} vnode_t;

#define	v_next		v_list.vl_next
#define	v_prev		v_list.vl_prev

/*
 * Vnode flags.
 * XXX When a new flag is added or an old one deleted, 
 *     *tab_vflags[] in io/idbg.c needs be kept in sync.
 *
 * XXX Also, must consider whether a flag is "local only" or "single
 *     system image" as described below.  Consult an expert if nec.
 *
 * The vnode flags fall into two categories:  
 * 1) Local only -
 *    Flags that are relevant only to a particular cell
 * 2) Single system image -
 *    Flags that must be maintained coherent across all cells
 */
/* Local only flags */
#define VDUP		0x1	/* file should be dup'ed rather then opened */
#define VINACT		0x2	/* vnode is being inactivated */
#define VRECLM		0x4	/* vnode is being reclaimed */
#define VLOCK		0x8	/* lock bit for vnode */
#define VEVICT 		0x10	/* vnode will be inactivated */
#define VWAIT		0x20	/* waiting for VINACT or VRECLM to finish */
#define VFLUSH		0x40	/* vnode being flushed */
#define VGONE		0x80	/* vnode isn't really here */
#define	VREMAPPING	0x100	/* file data flush/inval in progress */
#define	VMOUNTING	0x200	/* mount in progress on vnode */
#define VLOCKHOLD	0x400	/* VN_HOLD for remote locks */
#define VDFSCONVERTED	0x800   /* vnops have been converted to DFS format */
#define VROOTMOUNTOK	0x1000  /* ok to mount on top of this root */
#define	VINACTIVE_TEARDOWN 0x2000 /* vnode torn down at inactive time */
#define VSEMAPHORE	0x4000	/* vnode represents a Posix named semaphore */
#define VUSYNC		0x8000  /* vnode aspace represents usync objects */

/* Single system image flags */
#define VROOT		0x100000 /* root of its file system */
#define VNOSWAP		0x200000 /* file cannot be used as virt. swap device */
#define VISSWAP		0x400000 /* vnode is part of virtual swap device */
#define	VREPLICABLE	0x800000 /* Vnode can have replicated pages */
#define	VNONREPLICABLE	0x1000000 /* Vnode has writers. Don't replicate	*/
#define	VDOCMP		0x2000000 /* Vnode has special VOP_CMP impl. */
#define VSHARE		0x4000000 /* vnode part of global cache */
#define VFRLOCKS	0x8000000 /* vnode has FR locks applied */
#define VENF_LOCKING	0x10000000 /* enf. mode FR locking in effect */

/*
 * Flags for vnode operations.
 */
enum rm		{ RMFILE, RMDIRECTORY };	/* rm or rmdir (remove) */
enum symfollow	{ NO_FOLLOW, FOLLOW };		/* follow symlinks (or not) */
enum create	{ CROPEN = -1, CRCREAT, CRMKNOD, CRMKDIR, CRCORE };
						/* reason for create */
enum vrwlock	{ VRWLOCK_NONE, VRWLOCK_READ, VRWLOCK_WRITE, 
			VRWLOCK_WRITE_DIRECT }; /* type of rw lock */
enum vaddmap	{ VNEWMAP, VITERMAP, VDUPMAP };
						/* op for ADDMAP */

/*
 * flags for vn_create/VOP_CREATE/vn_open
 */
#define VEXCL	0x0001
#define VZFS	0x0002		/* caller has a 0 RLIMIT_FSIZE */

typedef enum rm		rm_t;
typedef enum symfollow	symfollow_t;
typedef enum create	create_t;
typedef enum vrwlock	vrwlock_t;
typedef enum vaddmap	vaddmap_t;

/*
 * FROM_VN_KILL is a special 'kill' flag to VOP_CLOSE to signify a call 
 * from vn_kill. This is passed as the lastclose field
 */
typedef enum { L_FALSE, L_TRUE, FROM_VN_KILL } lastclose_t;

/*
 * Return values for VOP_INACTIVE.  A return value of
 * VN_INACTIVE_NOCACHE implies that the file system behavior
 * has disassociated its state and bhv_desc_t from the vnode.
 * To return VN_INACTIVE_NOCACHE, the vnode must have the 
 * VINACTIVE_TEARDOWN flag set.
 */
#define	VN_INACTIVE_CACHE	0
#define	VN_INACTIVE_NOCACHE	1

/*
 * Values for the cmd code given to VOP_VNODE_CHANGE.
 */
typedef enum vchange {
	VCHANGE_FLAGS_FRLOCKS		= 0,
	VCHANGE_FLAGS_ENF_LOCKING	= 1
#ifdef CELL
	, VCHANGE_FLAGS_TRUNCATED	= 2
#endif
} vchange_t;

/*
 * Macros for dealing with the behavior descriptor inside of the vnode.
 *
 * VNODE_BHV_SYNCH indicates that the vnode's behavior head has
 * synchronization enabled (i.e., changes to the behavior chain are
 * synchronized with ops-in-progress on the vnode).
 */
#define BHV_TO_VNODE(bdp)	((vnode_t *)BHV_VOBJ(bdp))

#ifdef VNODE_BHV_SYNCH

#define VNODE_TO_FIRST_BHV(vp)		(BHV2_HEAD_FIRST(&(vp)->v_bh))
#define	VN_BHV_HEAD(vp)			((vn_bhv_head_t *)(&((vp)->v_bh)))
#define VN_BHV_READ_LOCK(bhp)		BHV2_READ_LOCK(bhp)		
#define VN_BHV_READ_UNLOCK(bhp)		BHV2_READ_UNLOCK(bhp)
#define VN_BHV_WRITE_LOCK(bhp)		BHV2_WRITE_LOCK(bhp)
#define VN_BHV_WRITE_UNLOCK(bhp)	BHV2_WRITE_UNLOCK(bhp)
#define VN_BHV_WRITE_TO_READ(bhp)	BHV2_WRITE_TO_READ(bhp)
#define VN_BHV_IS_READ_LOCKED(bhp)	BHV2_IS_READ_LOCKED(bhp)	
#define VN_BHV_NOT_READ_LOCKED(bhp)	BHV2_NOT_READ_LOCKED(bhp)	
#define VN_BHV_IS_WRITE_LOCKED(bhp)	BHV2_IS_WRITE_LOCKED(bhp)	
#define VN_BHV_NOT_WRITE_LOCKED(bhp)	BHV2_NOT_WRITE_LOCKED(bhp)
#define vn_bhv_lock_init(bhp,name)	bhv2_lock_init(bhp,name)
#define vn_bhv_lock_free(bhp)		bhv2_lock_free(bhp)
#define vn_bhv_head_init(bhp,name)	bhv2_head_init(bhp,name)
#define vn_bhv_head_destroy(bhp)	bhv2_head_destroy(bhp)
#define vn_bhv_head_reinit(bhp)		bhv2_head_reinit(bhp)
#define vn_bhv_insert_initial(bhp,bdp)	bhv2_insert_initial(bhp,bdp)
#define vn_bhv_remove(bhp,bdp)		bhv2_remove(bhp,bdp)
#define vn_bhv_insert(bhp,bdp)		bhv2_insert(bhp,bdp)
#define vn_bhv_remove_not_first(bhp,bdp) bhv2_remove_not_first(bhp,bdp) 
#define vn_bhv_lookup(bhp,ops)		bhv2_lookup(bhp,ops)
#define	vn_bhv_lookup_unlocked(bhp,ops)	bhv2_lookup_unlocked(bhp,ops)
#define vn_bhv_base_unlocked(bhp)	bhv2_base_unlocked(bhp)

#define v_fbhv		v_bh.bh2_first		/* first behavior */
#define v_fops		v_bh.bh2_first->bd_ops  /* ops for first behavior */

#else  /* VNODE_BHV_SYNCH */

#define VNODE_TO_FIRST_BHV(vp)		(BHV_HEAD_FIRST(&(vp)->v_bh))
#define	VN_BHV_HEAD(vp)			((vn_bhv_head_t *)(&((vp)->v_bh)))
#define VN_BHV_READ_LOCK(bhp)		BHV_READ_LOCK(bhp)		
#define VN_BHV_READ_UNLOCK(bhp)		BHV_READ_UNLOCK(bhp)
#define VN_BHV_WRITE_LOCK(bhp)		BHV_WRITE_LOCK(bhp)
#define VN_BHV_WRITE_UNLOCK(bhp)	BHV_WRITE_UNLOCK(bhp)
#define VN_BHV_WRITE_TO_READ(bhp)	BHV_WRITE_TO_READ(bhp)
#define VN_BHV_IS_READ_LOCKED(bhp)	BHV_IS_READ_LOCKED(bhp)	
#define VN_BHV_NOT_READ_LOCKED(bhp)	BHV_NOT_READ_LOCKED(bhp)	
#define VN_BHV_IS_WRITE_LOCKED(bhp)	BHV_IS_WRITE_LOCKED(bhp)	
#define VN_BHV_NOT_WRITE_LOCKED(bhp)	BHV_NOT_WRITE_LOCKED(bhp)
#define vn_bhv_lock_init(bhp,name)	bhv_lock_init(bhp,name)
#define vn_bhv_lock_free(bhp)		bhv_lock_free(bhp)
#define vn_bhv_head_init(bhp,name)	bhv_head_init(bhp,name)
#define vn_bhv_head_destroy(bhp)	bhv_head_destroy(bhp)
#define vn_bhv_head_reinit(bhp)		bhv_head_reinit(bhp)
#define vn_bhv_insert_initial(bhp,bdp)	bhv_insert_initial(bhp,bdp)
#define vn_bhv_remove(bhp,bdp)		bhv_remove(bhp,bdp)
#define vn_bhv_insert(bhp,bdp)		bhv_insert(bhp,bdp)
#define vn_bhv_remove_not_first(bhp,bdp) bhv_remove_not_first(bhp,bdp) 
#define vn_bhv_lookup(bhp,ops)		bhv_lookup(bhp,ops)
#define	vn_bhv_lookup_unlocked(bhp,ops)	bhv_lookup_unlocked(bhp,ops)
#define vn_bhv_base_unlocked(bhp)	bhv_base_unlocked(bhp)

#define v_fbhv		v_bh.bh_first	       /* first behavior */
#define v_fops		v_bh.bh_first->bd_ops  /* ops for first behavior */

#endif /* VNODE_BHV_SYNCH */


/*
 * Operations on vnodes.
 */
struct cred;
struct vfs;
struct buf;
struct vattr;
struct flock;
struct pathname;
struct bmapval;
struct pollhead;
struct fid;
struct flid;
struct pfdat;
union rval;
struct attrlist_cursor_kern;
struct __vhandl_s;
struct vopbd;
struct strbuf;

#define VNODE_CHECK_OPSVER(vp, ver)	VFS_CHECK_OPSVER((vp)->v_vfsp, ver)
#define VNODE_CHECK_OPSFLAG(vp, flag)	VFS_CHECK_OPSFLAG((vp->v_vfsp, flag)


typedef	int	(*vop_open_t)(bhv_desc_t *, vnode_t **, mode_t, struct cred *);
typedef	int	(*vop_close_t)(bhv_desc_t *, int, lastclose_t, struct cred *);
typedef	int	(*vop_read_t)(bhv_desc_t *, struct uio *, int, struct cred *,
				struct flid *);
typedef	int	(*vop_write_t)(bhv_desc_t *, struct uio *, int, struct cred *,
				struct flid *);
typedef	int	(*vop_ioctl_t)(bhv_desc_t *, int, void *, int, struct cred *,
				int *, struct vopbd *);
typedef	int	(*vop_setfl_t)(bhv_desc_t *, int, int, struct cred *);
typedef	int	(*vop_getattr_t)(bhv_desc_t *, struct vattr *, int,
				struct cred *);
typedef	int	(*vop_setattr_t)(bhv_desc_t *, struct vattr *, int,
				struct cred *);
typedef	int	(*vop_access_t)(bhv_desc_t *, int, struct cred *);
typedef	int	(*vop_lookup_t)(bhv_desc_t *, char *, vnode_t **,
				struct pathname *, int, vnode_t *,
				struct cred *);
typedef	int	(*vop_create_t)(bhv_desc_t *, char *, struct vattr *, int, int,
				vnode_t **, struct cred *);
typedef	int	(*vop_remove_t)(bhv_desc_t *, char *, struct cred *);
typedef	int	(*vop_link_t)(bhv_desc_t *, vnode_t *, char *, struct cred *);
typedef	int	(*vop_rename_t)(bhv_desc_t *, char *, vnode_t *, char *,
				struct pathname *npnp, struct cred *);
typedef	int	(*vop_mkdir_t)(bhv_desc_t *, char *, struct vattr *, vnode_t **,
				struct cred *);
typedef	int	(*vop_rmdir_t)(bhv_desc_t *, char *, vnode_t *, struct cred *);
typedef	int	(*vop_readdir_t)(bhv_desc_t *, struct uio *, struct cred *,
				int *);
typedef	int	(*vop_symlink_t)(bhv_desc_t *, char *, struct vattr *, char *,
				struct cred *);
typedef	int	(*vop_readlink_t)(bhv_desc_t *, struct uio *, struct cred *);
typedef	int	(*vop_fsync_t)(bhv_desc_t *, int, struct cred *, off_t, off_t);
typedef	int	(*vop_inactive_t)(bhv_desc_t *, struct cred *);
typedef	int	(*vop_fid_t)(bhv_desc_t *, struct fid **);
typedef	int	(*vop_fid2_t)(bhv_desc_t *, struct fid *);
typedef	void	(*vop_rwlock_t)(bhv_desc_t *, vrwlock_t);
typedef	void	(*vop_rwunlock_t)(bhv_desc_t *, vrwlock_t);
typedef	int	(*vop_seek_t)(bhv_desc_t *, off_t, off_t*);
typedef	int	(*vop_cmp_t)(bhv_desc_t *, vnode_t *);
typedef	int	(*vop_frlock_t)(bhv_desc_t *, int, struct flock *, int, off_t,
				vrwlock_t, struct cred *);
typedef	int	(*vop_realvp_t)(bhv_desc_t *, vnode_t **);
typedef	int	(*vop_bmap_t)(bhv_desc_t *, off_t, ssize_t, int, struct cred *,
				struct bmapval *, int *);
typedef	void	(*vop_strategy_t)(bhv_desc_t *, struct buf *);
typedef	int	(*vop_map_t)(bhv_desc_t *, off_t, size_t, mprot_t, u_int,
				struct cred *, vnode_t **);
typedef	int	(*vop_addmap_t)(bhv_desc_t *, vaddmap_t, struct __vhandl_s *,
				pgno_t *, off_t, size_t, mprot_t,
				struct cred *);
typedef	int	(*vop_delmap_t)(bhv_desc_t *, struct __vhandl_s *, size_t,
				struct cred *);
typedef	int	(*vop_poll_t)(bhv_desc_t *, short, int, short *,
				struct pollhead **, unsigned int *);
typedef	int	(*vop_dump_t)(bhv_desc_t *, caddr_t, daddr_t, u_int);
typedef	int	(*vop_pathconf_t)(bhv_desc_t *, int, long *, struct cred *);
typedef	int	(*vop_allocstore_t)(bhv_desc_t *, off_t, size_t, struct cred *);
typedef	int	(*vop_fcntl_t)(bhv_desc_t *, int, void *, int, off_t,
				struct cred *, union rval *);
typedef	int	(*vop_reclaim_t)(bhv_desc_t *, int);
typedef	int	(*vop_attr_get_t)(bhv_desc_t *, char *, char *, int *, int,
				struct cred *);
typedef	int	(*vop_attr_set_t)(bhv_desc_t *, char *, char *, int, int,
				struct cred *);
typedef	int	(*vop_attr_remove_t)(bhv_desc_t *, char *, int, struct cred *);
typedef	int	(*vop_attr_list_t)(bhv_desc_t *, char *, int, int,
				struct attrlist_cursor_kern *, struct cred *);
typedef	int	(*vop_cover_t)(bhv_desc_t *, struct mounta *, char *,
				struct cred *);
typedef	void	(*vop_link_removed_t)(bhv_desc_t *, vnode_t *, int);
typedef	void	(*vop_vnode_change_t)(bhv_desc_t *, vchange_t, __psint_t);
typedef	void	(*vop_ptossvp_t)(bhv_desc_t *, off_t, off_t, int);
typedef	void	(*vop_pflushinvalvp_t)(bhv_desc_t *, off_t, off_t, int);
typedef	int	(*vop_pflushvp_t)(bhv_desc_t *, off_t, off_t, uint64_t, int);
typedef	void	(*vop_pinvalfree_t)(bhv_desc_t *, off_t);
typedef	void	(*vop_sethole_t)(bhv_desc_t *, struct pfdat*, int, int, off_t);
typedef int	(*vop_commit_t)(bhv_desc_t *, struct buf *);
typedef int	(*vop_readbuf_t)(bhv_desc_t *, off_t, ssize_t, int,
				struct cred *, struct flid *,
				struct buf **, int *, int *);
typedef	int	(*vop_strgetmsg_t)(bhv_desc_t *,
				struct strbuf *, struct strbuf *,
				unsigned char *, int *, int, union rval *);
typedef	int	(*vop_strputmsg_t)(bhv_desc_t *,
				struct strbuf *, struct strbuf *,
				unsigned char, int, int);

typedef struct vnodeops {
	bhv_position_t	vn_position;	/* position within behavior chain */
	vop_open_t		vop_open;
	vop_close_t		vop_close;
	vop_read_t		vop_read;
	vop_write_t		vop_write;
	vop_ioctl_t		vop_ioctl;
	vop_setfl_t		vop_setfl;
	vop_getattr_t		vop_getattr;
	vop_setattr_t		vop_setattr;
	vop_access_t		vop_access;
	vop_lookup_t		vop_lookup;
	vop_create_t		vop_create;
	vop_remove_t		vop_remove;
	vop_link_t		vop_link;
	vop_rename_t		vop_rename;
	vop_mkdir_t		vop_mkdir;
	vop_rmdir_t		vop_rmdir;
	vop_readdir_t		vop_readdir;
	vop_symlink_t		vop_symlink;
	vop_readlink_t		vop_readlink;
	vop_fsync_t		vop_fsync;
	vop_inactive_t		vop_inactive;
	vop_fid_t		vop_fid;
	vop_fid2_t		vop_fid2;
	vop_rwlock_t		vop_rwlock;
	vop_rwunlock_t		vop_rwunlock;
	vop_seek_t		vop_seek;
	vop_cmp_t		vop_cmp;
	vop_frlock_t		vop_frlock;
	vop_realvp_t		vop_realvp;
	vop_bmap_t		vop_bmap;
	vop_strategy_t		vop_strategy;
	vop_map_t		vop_map;
	vop_addmap_t		vop_addmap;
	vop_delmap_t		vop_delmap;
	vop_poll_t		vop_poll;
	vop_dump_t		vop_dump;
	vop_pathconf_t		vop_pathconf;
	vop_allocstore_t	vop_allocstore;
	vop_fcntl_t		vop_fcntl;
	vop_reclaim_t		vop_reclaim;
	vop_attr_get_t		vop_attr_get;
	vop_attr_set_t		vop_attr_set;
	vop_attr_remove_t	vop_attr_remove;
	vop_attr_list_t		vop_attr_list;
	vop_cover_t		vop_cover;
	vop_link_removed_t	vop_link_removed;
	vop_vnode_change_t	vop_vnode_change;
	vop_ptossvp_t		vop_tosspages;
	vop_pflushinvalvp_t	vop_flushinval_pages;
	vop_pflushvp_t		vop_flush_pages;
	vop_pinvalfree_t	vop_invalfree_pages;
	vop_sethole_t		vop_pages_sethole;
	vop_commit_t		vop_commit;
	vop_readbuf_t		vop_readbuf;
	vop_strgetmsg_t		vop_strgetmsg;
	vop_strputmsg_t		vop_strputmsg;
} vnodeops_t;

/*
 * VOP's.  
 */
#define _VOP_(op, vp)	(*((vnodeops_t *)(vp)->v_fops)->op)

/* 
 * Be careful with VOP_OPEN, since we're holding the chain lock on the
 * original vnode and VOP_OPEN semantic allows the new vnode to be returned
 * in vpp. The practice of passing &vp for vpp just doesn't work.
 */
#define	VOP_OPEN(vp, vpp, mode, cr, rv) 				\
{									\
	ASSERT(&(vp) != vpp);						\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_open, vp)((vp)->v_fbhv, vpp, mode, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_CLOSE(vp,f,c,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_close, vp)((vp)->v_fbhv,f,c,cr);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_READ(vp,uiop,iof,cr,fl,rv) 					\
{       								\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_read, vp)((vp)->v_fbhv,uiop,iof,cr,fl);      	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_WRITE(vp,uiop,iof,cr,fl,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_write, vp)((vp)->v_fbhv,uiop,iof,cr,fl);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_IOCTL(vp,cmd,a,f,cr,rvp,vopbdp,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_ioctl, vp)((vp)->v_fbhv,cmd,a,f,cr,rvp,vopbdp); 	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SETFL(vp, f, a, cr, rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_setfl, vp)((vp)->v_fbhv, f, a, cr); 		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_GETATTR(vp, vap, f, cr, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_getattr, vp)((vp)->v_fbhv, vap, f, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SETATTR(vp, vap, f, cr, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_setattr, vp)((vp)->v_fbhv, vap, f, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ACCESS(vp, mode, cr, rv)	 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_access, vp)((vp)->v_fbhv, mode, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_LOOKUP(vp,cp,vpp,pnp,f,rdir,cr,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_lookup, vp)((vp)->v_fbhv,cp,vpp,pnp,f,rdir,cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_CREATE(dvp,p,vap,ex,mode,vpp,cr,rv) 			\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_create, dvp)((dvp)->v_fbhv,p,vap,ex,mode,vpp,cr);\
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_REMOVE(dvp,p,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_remove, dvp)((dvp)->v_fbhv,p,cr);		\
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_LINK(tdvp,fvp,p,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(tdvp)->v_bh);				\
	rv = _VOP_(vop_link, tdvp)((tdvp)->v_fbhv,fvp,p,cr);		\
	VN_BHV_READ_UNLOCK(&(tdvp)->v_bh);				\
}
#define	VOP_RENAME(fvp,fnm,tdvp,tnm,tpnp,cr,rv) 			\
{									\
	VN_BHV_READ_LOCK(&(fvp)->v_bh);					\
	rv = _VOP_(vop_rename, fvp)((fvp)->v_fbhv,fnm,tdvp,tnm,tpnp,cr);\
	VN_BHV_READ_UNLOCK(&(fvp)->v_bh);				\
}
#define	VOP_MKDIR(dp,p,vap,vpp,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dp)->v_bh);					\
	rv = _VOP_(vop_mkdir, dp)((dp)->v_fbhv,p,vap,vpp,cr);		\
	VN_BHV_READ_UNLOCK(&(dp)->v_bh);				\
}
#define	VOP_RMDIR(dp,p,cdir,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dp)->v_bh);					\
	rv = _VOP_(vop_rmdir, dp)((dp)->v_fbhv,p,cdir,cr);		\
	VN_BHV_READ_UNLOCK(&(dp)->v_bh);				\
}
#define	VOP_READDIR(vp,uiop,cr,eofp,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_readdir, vp)((vp)->v_fbhv,uiop,cr,eofp);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SYMLINK(dvp,lnm,vap,tnm,cr,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_symlink, dvp) ((dvp)->v_fbhv,lnm,vap,tnm,cr);	\
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_READLINK(vp,uiop,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_readlink, vp)((vp)->v_fbhv,uiop,cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_FSYNC(vp,f,cr,b,e,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fsync, vp)((vp)->v_fbhv,f,cr,b,e);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_INACTIVE(vp, cr, rv) 					\
{	/* vnode not reference-able, so no need to lock chain */ 	\
	rv = _VOP_(vop_inactive, vp)((vp)->v_fbhv, cr); 		\
}
#define	VOP_FID(vp, fidpp, rv) 						\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fid, vp)((vp)->v_fbhv, fidpp);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_FID2(vp, fidp, rv) 						\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fid2, vp)((vp)->v_fbhv, fidp);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_RWLOCK(vp,i) 						\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	(void)_VOP_(vop_rwlock, vp)((vp)->v_fbhv, i); 			\
	/* "allow" is done by rwunlock */				\
}
#define	VOP_RWUNLOCK(vp,i) 						\
{	/* "prevent" was done by rwlock */    				\
	(void)_VOP_(vop_rwunlock, vp)((vp)->v_fbhv, i);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SEEK(vp, ooff, noffp, rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_seek, vp)((vp)->v_fbhv, ooff, noffp);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_FRLOCK(vp,cmd,a,f,o,rwl,cr,rv)				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_frlock, vp)((vp)->v_fbhv,cmd,a,f,o,rwl,cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_REALVP(vp1, vp2, rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp1)->v_bh);					\
	rv = _VOP_(vop_realvp, vp1)((vp1)->v_fbhv, vp2);		\
	VN_BHV_READ_UNLOCK(&(vp1)->v_bh);				\
}
/*
 * The vnode pointer passed to this macro sometimes changes during the 
 * execution of the underlying vop_strategy op.  => save the vp in a local var.
 */
#define	VOP_STRATEGY(vp,bp) 						\
{									\
	vnode_t *_myvp_ = vp;						\
	VN_BHV_READ_LOCK(&(_myvp_)->v_bh);				\
	(void)_VOP_(vop_strategy, _myvp_)((_myvp_)->v_fbhv, bp);	\
	VN_BHV_READ_UNLOCK(&(_myvp_)->v_bh);				\
}
#define VOP_CMP(vp1,vp2,rv) 						\
{									\
	VN_BHV_READ_LOCK(&(vp1)->v_bh);					\
	rv = _VOP_(vop_cmp, vp1)((vp1)->v_fbhv, vp2); 			\
	VN_BHV_READ_UNLOCK(&(vp1)->v_bh);				\
}
#define	VOP_BMAP(vp,of,sz,rw,cr,b,n, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_bmap, vp)((vp)->v_fbhv,of,sz,rw,cr,b,n);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_MAP(vp,of,sz,p,fl,cr,nvp,rv) 				\
{									\
	vnode_t	*_myvp_ = vp;						\
	VN_BHV_READ_LOCK(&(_myvp_)->v_bh);				\
	rv = _VOP_(vop_map, _myvp_) ((_myvp_)->v_fbhv,of,sz,p,fl,cr,nvp); \
	VN_BHV_READ_UNLOCK(&(_myvp_)->v_bh);				\
}
#define	VOP_ADDMAP(vp,op,vt,pg,of,sz,p,cr,rv)	 			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_addmap, vp) ((vp)->v_fbhv,op,vt,pg,of,sz,p,cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_DELMAP(vp,vt,sz,cr,rv)		 			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_delmap, vp) ((vp)->v_fbhv,vt,sz,cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_POLL(vp, events, anyyet, reventsp, phpp, genp, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_poll, vp)((vp)->v_fbhv,events,anyyet,reventsp,phpp,genp); \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_DUMP(vp,addr,bn,count,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_dump, vp)((vp)->v_fbhv,addr,bn,count);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_PATHCONF(vp, cmd, valp, cr, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_pathconf, vp)((vp)->v_fbhv, cmd, valp, cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_ALLOCSTORE(vp,off,len,cr,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_allocstore, vp)((vp)->v_fbhv,off,len,cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_FCNTL(vp,cmd,a,f,of,cr,rvp,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fcntl, vp)((vp)->v_fbhv,cmd,a,f,of,cr,rvp);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_RECLAIM(vp, flag, rv) 					\
{	/* vnode not reference-able, so no need to lock chain */ 	\
	ASSERT(!((vp)->v_flag & VINACTIVE_TEARDOWN));			\
	rv = _VOP_(vop_reclaim, vp)((vp)->v_fbhv, flag);		\
}
#define	VOP_ATTR_GET(vp, name, val, vallenp, fl, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_get, vp)((vp)->v_fbhv,name,val,vallenp,fl,cred); \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_SET(vp, name, val, vallen, fl, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_set, vp)((vp)->v_fbhv,name,val,vallen,fl,cred); \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_REMOVE(vp, name, flags, cred, rv) 			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_remove, vp)((vp)->v_fbhv,name,flags,cred);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_LIST(vp, buf, buflen, fl, cursor, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_list, vp)((vp)->v_fbhv,buf,buflen,fl,cursor,cred);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
/* 
 * We don't hold the bhv chain lock for VOP_COVER because we don't
 * want to hold off inserting a new interposer on the covered vnode
 * (because that can happen from within the VOP_COVER path, in the
 * distributed case at least), and it's the case that inserting
 * an interposer on the covered vnode does not require synchronization 
 * with a VOP_COVER.  XXX But what about relocation of the covered vnode?
 */
#define VOP_COVER(cvp, uap, attrs, cr, rv) 				\
{       								\
        rv = _VOP_(vop_cover, cvp)((cvp)->v_fbhv, uap, attrs, cr); 	\
}
#define VOP_LINK_REMOVED(vp, dvp, linkzero) 				\
{       								\
        VN_BHV_READ_LOCK(&(vp)->v_bh);       				\
        (void)_VOP_(vop_link_removed, vp)((vp)->v_fbhv, dvp, linkzero); \
        VN_BHV_READ_UNLOCK(&(vp)->v_bh); 				\
}
#define	VOP_VNODE_CHANGE(vp, cmd, val)					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	(void)_VOP_(vop_vnode_change, vp)((vp)->v_fbhv,cmd,val);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
/*
 * These are page cache functions that now go thru VOPs.
 */
#define VOP_TOSS_PAGES(vp, first, last, fiopt)				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_tosspages, vp)((vp)->v_fbhv,first,last,fiopt);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_FLUSHINVAL_PAGES(vp, first, last, fiopt)			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_flushinval_pages, vp)((vp)->v_fbhv,first,last,fiopt);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_FLUSH_PAGES(vp, first, last, flags, fiopt, rv)			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_flush_pages, vp)((vp)->v_fbhv,first,last,flags,fiopt);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_INVALFREE_PAGES(vp, off)					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_invalfree_pages, vp)((vp)->v_fbhv,off);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_PAGES_SETHOLE(vp, pfd, cnt, doremap, remapoffset)		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_pages_sethole, vp)((vp)->v_fbhv,pfd,cnt,doremap,remapoffset);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}

/*
 * Currently, used only by nfs V3.
 */
#define VOP_COMMIT(vp,bp,rv)						\
{									\
        VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_commit, vp)((vp)->v_fbhv, bp);			\
        VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}

#define VOP_READBUF(vp, offset, count, flags, credp, flid,		\
			bp, pboff, pblen, rv)				\
{									\
        VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_readbuf, vp)((vp)->v_fbhv,offset,count,flags,	\
			credp,flid,bp,pboff,pblen);			\
        VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}

/*
 * Two special VOP's needed for distributed streams msgs.
 */
#define VOP_STRGETMSG(vp,mctl,mdata,prip,flagsp,fmode,rvp,rv)		\
{       								\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_strgetmsg, vp)((vp)->v_fbhv,mctl,mdata,prip,flagsp,fmode,rvp);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_STRPUTMSG(vp,mctl,mdata,pri,flags,fmode,rv)		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_strputmsg, vp)((vp)->v_fbhv,mctl,mdata,pri,flags,fmode);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}

/*
 * Registration for the positions at which the different vnode behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 *
 * Typically, the traditional file systems (xfs, procfs,...) will be the
 * BASE behavior.
 */
#define	VNODE_POSITION_BASE	BHV_POSITION_BASE   	/* chain bottom */
#define	VNODE_POSITION_TOP	BHV_POSITION_TOP    	/* chain top */
#define	VNODE_POSITION_INVALID	BHV_POSITION_INVALID   	/* invalid pos. num */

/*
 * To avoid disrupting kudzu, the current values are left for the non-CELL
 * case although there is no reason known that the newer arrangement will
 * not work fine.  After the tree split the #ifdef chould be removed.
 */
#if CELL
#define	VNODE_POSITION_IMON	 (VNODE_POSITION_BASE+7)  /* inode monitor     */
#define	VNODE_POSITION_REPL	 (VNODE_POSITION_BASE+6)  /* replication layer */
#define	VNODE_POSITION_SPECFS_DC (VNODE_POSITION_BASE+5)  /* specfs DC layer   */
#define	VNODE_POSITION_CFS_DS	 (VNODE_POSITION_BASE+4)  /* CFS server	       */
#define	VNODE_POSITION_CFS_DC	 (VNODE_POSITION_BASE+3)  /* CFS client	       */
#define	VNODE_POSITION_SPECFS_DS (VNODE_POSITION_BASE+2)  /* specfs DS layer   */
#define	VNODE_POSITION_SPECFS_AT (VNODE_POSITION_BASE+1)  /* specfs AT layer   */
#else /* CELL */
#define	VNODE_POSITION_REPL	 (VNODE_POSITION_BASE+1)  /* replication layer */
#define	VNODE_POSITION_IMON	 (VNODE_POSITION_BASE+2)  /* inode monitor     */
#define	VNODE_POSITION_SPECFS_DC (VNODE_POSITION_TOP-1)   /* specfs DC layer   */
#define	VNODE_POSITION_SPECFS_DS (VNODE_POSITION_TOP-2)   /* specfs DS layer   */
#define	VNODE_POSITION_SPECFS_AT (VNODE_POSITION_TOP-3)   /* specfs AT layer   */
#define	VNODE_POSITION_CFS_DS	  VNODE_POSITION_TOP	  /* CFS server	       */
#define	VNODE_POSITION_CFS_DC	  VNODE_POSITION_TOP	  /* CFS client	       */
#endif /* CELL */

/*
 * I/O flags for VOP_READ, VOP_WRITE, and VOP_READBUF
 */
#define IO_APPEND	0x00001	/* append write (VOP_WRITE) */
#define IO_SYNC		0x00002	/* sync file I/O (VOP_WRITE) */
#define IO_DIRECT	0x00004	/* bypass page cache */
#define IO_IGNCACHE	0x00008	/* ignore page cache coherency when doing i/o
					(IO_DIRECT) */
#define IO_GRIO		0x00010	/* this is a guaranteed rate request */
#define IO_INVIS	0x00020	/* don't update inode timestamps */
#define IO_DSYNC	0x00040	/* sync data I/O (VOP_WRITE) */
#define IO_RSYNC	0x00080	/* sync data I/O (VOP_READ) */
#define IO_NFS          0x00100 /* I/O from the NFS v2 server */
#define IO_TRUSTEDDIO   0x00200	/* direct I/O from a trusted client
     				   so block zeroing is unnecessary */
#define IO_PRIORITY	0x00400	/* I/O is priority */
#define IO_ISLOCKED     0x00800 /* for VOP_READ/WRITE, VOP_RWLOCK/RWUNLOCK is
				   being done by higher layer - file system 
				   shouldn't do locking */
#define IO_BULK		0x01000	/* loosen semantics for sequential bandwidth */
#define IO_NFS3		0x02000	/* I/O from the NFS v3 server */
#define IO_UIOSZ	0x04000	/* respect i/o size flags in uio struct */
#define IO_ONEPAGE	0x08000	/* I/O must be fit into one page */
#define IO_MTTHREAD	0x10000	/* I/O coming from threaded application, only
				   used by paging to indicate that fs can
				   return EAGAIN if this would deadlock. */
#ifdef _KERNEL
#define IO_PFUSE_SAFE	0x20000 /* VOP_WRITE/VOP_READ: vnode can take addr's,
				   kvatopfdat them, bump pf_use, and continue
				   to reference data after return from VOP_.
				   If IO_SYNC, only concern is kvatopfdat
				   returns legal pfdat. */
#endif

/*
 * Flags for VOP_LOOKUP.
 */
#define LOOKUP_DIR	0x01	/* want parent dir vp */

/*
 * Flush/Invalidate options for VOP_TOSS_PAGES, VOP_FLUSHINVAL_PAGES and
 * 	VOP_FLUSH_PAGES.
 */
#define FI_NONE			0	/* none */
#define FI_REMAPF		1	/* Do a remapf prior to the operation */
#define FI_REMAPF_LOCKED	2	/* Do a remapf prior to the operation.
					   Prevent VM access to the pages until the
					   operation completes. */

/*
 * Vnode attributes.  A bit-mask is supplied as part of the
 * structure to indicate the attributes the caller wants to
 * set (setattr) or extract (getattr).
 */
typedef struct vattr {
	int		va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* file access mode */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	dev_t		va_fsid;	/* file system id (dev for now) */
	ino_t		va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	off_t		va_size;	/* file size in bytes */
	timespec_t	va_atime;	/* time of last access */
	timespec_t	va_mtime;	/* time of last modification */
	timespec_t	va_ctime;	/* time file ``created'' */
	dev_t		va_rdev;	/* device the file represents */
	u_long		va_blksize;	/* fundamental block size */
	blkcnt_t	va_nblocks;	/* # of blocks allocated */
	u_long		va_vcode;	/* version code */
	u_long		va_xflags;	/* random extended file flags */
	u_long		va_extsize;	/* file extent size */
	u_long		va_nextents;	/* number of extents in file */
	u_long		va_anextents;	/* number of attr extents in file */
	int		va_projid;	/* project id */
	u_int		va_gencount;	/* object generation count */
} vattr_t;

/*
 * Attributes of interest to the caller of setattr or getattr.
 */
#define	AT_TYPE		0x00000001
#define	AT_MODE		0x00000002
#define	AT_UID		0x00000004
#define	AT_GID		0x00000008
#define	AT_FSID		0x00000010
#define	AT_NODEID	0x00000020
#define	AT_NLINK	0x00000040
#define	AT_SIZE		0x00000080
#define	AT_ATIME	0x00000100
#define	AT_MTIME	0x00000200
#define	AT_CTIME	0x00000400
#define	AT_RDEV		0x00000800
#define AT_BLKSIZE	0x00001000
#define AT_NBLOCKS	0x00002000
#define AT_VCODE	0x00004000
#define AT_MAC		0x00008000
#define AT_UPDATIME	0x00010000
#define AT_UPDMTIME	0x00020000
#define AT_UPDCTIME	0x00040000
#define AT_ACL		0x00080000
#define AT_CAP		0x00100000
#define AT_INF		0x00200000
#define	AT_XFLAGS	0x00400000
#define	AT_EXTSIZE	0x00800000
#define	AT_NEXTENTS	0x01000000
#define	AT_ANEXTENTS	0x02000000
#define AT_PROJID	0x04000000
#define	AT_SIZE_NOPERM	0x08000000
#define	AT_GENCOUNT	0x10000000

#define	AT_ALL	(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
		AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|\
		AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_MAC|AT_ACL|AT_CAP|\
		AT_INF|AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|\
		AT_PROJID|AT_GENCOUNT)

#define	AT_STAT	(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|AT_NLINK|\
		AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|AT_BLKSIZE|\
		AT_NBLOCKS|AT_PROJID)

#define	AT_TIMES (AT_ATIME|AT_MTIME|AT_CTIME)

#define	AT_UPDTIMES (AT_UPDATIME|AT_UPDMTIME|AT_UPDCTIME)

#define	AT_NOSET (AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
		 AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_NEXTENTS|AT_ANEXTENTS|\
		 AT_GENCOUNT)
	
/*
 *  Modes.  Some values same as S_xxx entries from stat.h for convenience.
 */
#define	VSUID		04000		/* set user id on execution */
#define	VSGID		02000		/* set group id on execution */
#define VSVTX		01000		/* save swapped text even after use */

/*
 * Permissions.
 */
#define	VREAD		00400
#define	VWRITE		00200
#define	VEXEC		00100

#define	MODEMASK	07777		/* mode bits plus permission bits */
#define	PERMMASK	00777		/* permission bits */

/*
 * Check whether mandatory file locking is enabled.
 */
#define MANDLOCK(vp, mode)	\
	((vp)->v_type == VREG && ((mode) & (VSGID|(VEXEC>>3))) == VSGID)

/*
 * VOP_BMAP result parameter type / chunk interfaces input type.
 *
 * The bn, offset and length fields are expressed in BBSIZE blocks
 * (defined in sys/param.h).
 * The length field specifies the size of the underlying backing store
 * for the particular mapping.
 * The bsize, pbsize and pboff fields are expressed in bytes and indicate
 * the size of the mapping, the number of bytes that are valid to access
 * (read or write), and the offset into the mapping, given the offset
 * parameter passed to VOP_BMAP.
 * The pmp value is a pointer to a memory management policy module
 * structure.  When returning a bmapval from VOP_BMAP it should be
 * NULL.  It is usually copied from the uio structure into a bmapval
 * during a VOP_READ or VOP_WRITE call before calling chunkread/getchunk.
 *
 * When a request is made to read beyond the logical end of the object,
 * pbsize may be set to 0, but offset and length should be set to reflect
 * the actual amount of underlying storage that has been allocated, if any.
 */
typedef struct bmapval {
	daddr_t		bn;	/* block number in vfs */
	off_t		offset;	/* logical block offset of this mapping */
	struct pm	*pmp;	/* Policy Module pointer */
	int		length;	/* length of this mapping in blocks */
	int		bsize;	/* length of this mapping in bytes */
	int		pbsize;	/* bytes in block mapped for i/o */
	int		pboff;	/* byte offset into block for i/o */
	dev_t		pbdev;	/* real ("physical") device for i/o */
	char		eof;	/* last mapping of object */
} bmapval_t;

/*
 * The eof field of the bmapval structure is really a flags
 * field.  Here are the valid values.
 */
#define	BMAP_EOF		0x01	/* mapping contains EOF */
#define	BMAP_HOLE		0x02	/* mapping covers a hole */
#define	BMAP_DELAY		0x04	/* mapping covers delalloc region */
#define	BMAP_FLUSH_OVERLAPS	0x08	/* Don't flush overlapping buffers */
#define	BMAP_READAHEAD		0x10	/* return NULL if necessary */
#define	BMAP_UNWRITTEN		0x20	/* mapping covers allocated */
					/* but uninitialized XFS data */
#define BMAP_DONTALLOC		0x40	/* don't allocate pfdats in buf_t's */

#ifdef _KERNEL

/*
 * This macro determines if a write is actually allowed
 * on the node.  This macro is used to check if a file's
 * access time can be modified.
 */
#define	WRITEALLOWED(vp, cr) \
	((vp)->v_vfsp && ((vp)->v_vfsp->vfs_flag & VFS_RDONLY) == 0)

/*
 * Public vnode manipulation functions.
 */

extern int	vn_open(char *, enum uio_seg, int, mode_t, struct vnode **,
			enum create, int, int *);
extern int	vn_create(char *, enum uio_seg, struct vattr *, int,
			  int mode, struct vnode **, enum create, int *);
extern int	vn_rdwr(enum uio_rw, struct vnode *, caddr_t, size_t, off_t,
			enum uio_seg, int, off_t, struct cred *, ssize_t *,
			struct flid *);
extern int	vn_link(char *, char *, enum uio_seg, enum symfollow);
extern int	vn_rename(char *, char *, enum uio_seg);
extern int	vn_remove(char *, enum uio_seg, enum rm);
extern struct pfdat *vn_pfind(vnode_t *, pgno_t, int, void *);

/*
 * Global vnode allocation:
 *
 *	vp = vn_alloc(vfsp, type, rdev);
 *	vn_free(vp);
 *
 * Inactive vnodes are kept on an LRU freelist managed by vn_alloc, vn_free,
 * vn_get, vn_purge, and vn_rele.  When vn_rele inactivates a vnode,
 * it puts the vnode at the end of the list unless there are no behaviors
 * attached to it, which tells vn_rele to insert at the beginning of the
 * freelist.  When vn_get acquires an inactive vnode, it unlinks the vnode
 * from the list;
 * vn_purge puts inactive dead vnodes at the front of the list for rapid reuse.
 *
 * If the freelist is empty, vn_alloc dynamically allocates another vnode.
 * Call vn_free to destroy a vn_alloc'd vnode that has no other references
 * and no valid private data.  Do not call vn_free from within VOP_INACTIVE;
 * just remove the behaviors and vn_rele will do the right thing.
 *
 * A vnode might be deallocated after it is put on the freelist (after
 * a VOP_RECLAIM, of course).  In this case, the vn_epoch value is
 * incremented to define a new vnode epoch.
 */
extern vnode_t	*vn_alloc(struct vfs *, enum vtype, dev_t);
extern void	vn_free(struct vnode *);

/*
 * Acquiring and invalidating vnodes:
 *
 *	if (vn_get(vp, version, 0))
 *		...;
 *	vn_purge(vp, version);
 *
 * vn_get and vn_purge must be called with vmap_t arguments, sampled
 * while a lock that the vnode's VOP_RECLAIM function acquires is
 * held, to ensure that the vnode sampled with the lock held isn't
 * recycled (VOP_RECLAIMed) or deallocated between the release of the lock
 * and the subsequent vn_get or vn_purge.
 */

/*
 * vnode_map structures _must_ match vn_epoch and vnode structure sizes.
 */
typedef struct vnode_map {
	vnumber_t	v_number;		/* in-core vnode number */
	int		v_epoch;		/* epoch in vnode history */
	int		v_id;			/* freeelist id */
} vmap_t;

#define	VMAP(vp, vmap)	{(vmap).v_number = (vp)->v_number, \
			 (vmap).v_id = (vp)->v_listid, \
			 (vmap).v_epoch = vn_epoch; }

extern void	vn_kill(struct vnode *);
extern void	vn_gone(struct vnode *);
extern int	vn_evict(struct vnode *);
extern void	vn_purge(struct vnode *, vmap_t *);
extern vnode_t *vn_get(struct vnode *, vmap_t *, uint);

/*
 * Flags for vn_get().
 */
#define	VN_GET_NOWAIT	0x1	/* Don't wait for inactive or reclaim */

/*
 * Vnode reference counting functions (and macros for compatibility).
 */
extern vnode_t	*vn_hold(struct vnode *);
extern void	vn_rele(struct vnode *);

#ifdef VNODE_TRACING
#define VN_HOLD(vp)		\
	((void)vn_hold(vp), \
	  vn_trace_hold(vp, __FILE__, __LINE__, (inst_t *)__return_address))
#define VN_RELE(vp)		\
	  (vn_trace_rele(vp, __FILE__, __LINE__, (inst_t *)__return_address), \
	   vn_rele(vp))
#else
#define VN_HOLD(vp)		((void)vn_hold(vp))
#define VN_RELE(vp)		(vn_rele(vp))
#endif

/*
 * Vnode spinlock manipulation.
 */
#define	VN_LOCK(vp)		mutex_bitlock(&(vp)->v_flag, VLOCK)
#define	VN_UNLOCK(vp,s)		mutex_bitunlock(&(vp)->v_flag, VLOCK, s)
#define VN_FLAGSET(vp,b)	bitlock_set(&(vp)->v_flag, VLOCK, b)
#define VN_FLAGCLR(vp,b)	bitlock_clr(&(vp)->v_flag, VLOCK, b)

/*
 * Vnode buffer tree lock manipulation
 */
#define	VN_BUF_LOCK(vp, s)	mutex_lock(&(vp)->v_buf_lock, PZERO)
#define	VN_BUF_UNLOCK(vp, s)	mutex_unlock(&(vp)->v_buf_lock)

/*
 * lock the vfs given the vnode pointer
 */
#define VN_VFSLOCK(vp)		vfs_lock((vp)->v_vfsp)
#define VN_VFSUNLOCK(vp)	vfs_unlock((vp)->v_vfsp)

/*
 * Some useful predicates.
 */
#define	VN_MAPPED(vp)	((vp)->v_mreg != (struct pregion *)(vp))
#define	VN_DIRTY(vp)	((vp)->v_dbuf || (vp)->v_dpages)

#ifdef	NUMA_REPLICATION
/*
 * Macros to set and clear bits indicating if the vnode is replicable. 
 * It's the responsibility of the caller of macro to hold the appropriate
 * locks while setting/clearing the bits.
 * 
 * These two bits should be mutually exclusive. So, while setting one
 * it's necessary to make sure that other is not set. Since this involves
 * two operations, it's not possible to use VN_FLAGSET macros for
 * setting/clearing these bits.. 
 */
#define	VN_SETREPLICABLE(vp)		((vp)->v_flag |= VREPLICABLE) 
#define	VN_CLRREPLICABLE(vp)		((vp)->v_flag &= ~VREPLICABLE)

#define	VN_SETNONREPLICABLE(vp)		((vp)->v_flag |= VNONREPLICABLE)
#define	VN_CLRNONREPLICABLE(vp)		((vp)->v_flag &= ~VNONREPLICABLE)

#define	VN_ISREPLICABLE(vp)	((vp)->v_flag & VREPLICABLE)
#define	VN_ISNONREPLICABLE(vp)	((vp)->v_flag & VNONREPLICABLE)

#else	/* NUMA_RELICATION */

#define	VN_SETREPLICABLE(vp)	
#define	VN_CLRREPLICABLE(vp)	

#define	VN_SETNONREPLICABLE(vp)	
#define	VN_CLRNONREPLICABLE(vp)	

#define	VN_ISREPLICABLE(vp)		(0)
#define	VN_ISNONREPLICABLE(vp)		(1)

#endif	/* NUMA_REPLICATION */


#define	VN_CACHED(vp)	((vp)->v_pgcnt != 0)

/*
 * Compare two vnodes for equality.  In general this macro should be used
 * in preference to calling VOP_CMP directly.
 * 
 * The standard vp1 == vp2 is sufficient for most file systems, and for
 * the ones that it's not, they must set the VDOCMP flag in the vnode.
 */
extern int	vn_cmp(vnode_t *vp1, vnode_t *vp2);
#define VN_CMP(VP1, VP2)						\
        ((VP1) == (VP2) ||						\
	 ((VP1) && (VP2) &&						\
	   (((VP1)->v_flag & VDOCMP) || ((VP2)->v_flag & VDOCMP)) 	\
	   && vn_cmp((VP1), (VP2))))

/*
 * Flags to VOP_SETATTR/VOP_GETATTR.
 */
#define	ATTR_UTIME	0x01	/* non-default utime(2) request */
#define	ATTR_EXEC	0x02	/* invocation from exec(2) */
#define	ATTR_COMM	0x04	/* yield common vp attributes */
#define	ATTR_DMI	0x08	/* invocation from a DMI function */
#define	ATTR_LAZY	0x80	/* set/get attributes lazily */
#define	ATTR_NONBLOCK	0x100	/* return EAGAIN if operation would block */

/*
 * Flags to VOP_FSYNC and VOP_RECLAIM.
 */
#define FSYNC_NOWAIT	0	/* asynchronous flush */
#define FSYNC_WAIT	0x1	/* synchronous fsync or forced reclaim */
#define FSYNC_INVAL	0x2	/* flush and invalidate cached data */
#define FSYNC_DATA	0x4	/* synchronous fsync of data only */

/*
 * Generally useful macros.
 */
#define	VBSIZE(vp)	((vp)->v_vfsp->vfs_bsize)
#define	NULLVP		((struct vnode *)0)
#define	NULLVPP		((struct vnode **)0)

struct pathname;
extern int lookupname(char *, enum uio_seg, enum symfollow, vnode_t **,
		      vnode_t **, ckpt_handle_t *);
extern int lookuppn(struct pathname *, enum symfollow, vnode_t **, vnode_t **,
		    ckpt_handle_t *);
extern int traverse(vnode_t **);

extern int namesetattr(char *, enum symfollow, struct vattr *, int);
extern int fdsetattr(int, vattr_t *, int);

struct irix5_stat;
struct irix5_64_stat;
struct stat64;
extern int xcstat(void *, void *, int, int, struct cred *);
extern int irix5_64_xcstat(vnode_t *, struct irix5_64_stat *, struct cred *);

/*
 * Vnode list ops.
 */
#define	vn_append(vp,vl)	vn_insert(vp, (struct vnlist *)(vl)->vl_prev)

extern void vn_initlist(struct vnlist *);
extern void vn_insert(struct vnode *, struct vnlist *);
extern void vn_unlink(struct vnode *);

extern vnodeops_t *vn_passthrup;	/* vnode pass-thru ops */

#ifdef VNODE_TRACING
/*
 * Tracing entries.
 */
#define	VNODE_KTRACE_ENTRY	1
#define	VNODE_KTRACE_HOLD	2
#define	VNODE_KTRACE_REF	3
#define	VNODE_KTRACE_RELE	4

#define	VNODE_TRACE_SIZE	64		/* number of trace entries */
extern void vn_trace_entry(struct vnode *, char *, inst_t *);
extern void vn_trace_hold(struct vnode *, char *, int, inst_t *);
extern void vn_trace_ref(struct vnode *, char *, int, inst_t *);
extern void vn_trace_rele(struct vnode *, char *, int, inst_t *);
#define	VN_TRACE(vp)		\
	vn_trace_ref(vp, __FILE__, __LINE__, (inst_t *)__return_address)
#else
#define	vn_trace_entry(a,b,c)
#define	vn_trace_hold(a,b,c,d)
#define	vn_trace_ref(a,b,c,d)
#define	vn_trace_rele(a,b,c,d)
#define	VN_TRACE(vp)
#endif	/* VNODE_TRACING */

/*
 * Vnode pagecache operations.
 * (Note: ptossvp, pflushinvalvp, pflushvp, pinvalfree & pageshole
 *  have been replaced with VOPs)
 */
#define pfind(vp, pgno, flag)           vnode_pfind(vp, pgno, flag)
#define pinsert(pfd, vp, pgno, flag)    vnode_pinsert(vp, pfd, pgno, flag)
#define pinsert_nolock(pfd, vp, pgno, flag, locktoken) \
                        vnode_pinsert_nolock(pfd, vp, pgno, flag, locktoken)
#define pinsert_try(pfd, vp, pgno)      vnode_pinsert_try(vp, pfd, pgno)
#define preplace(opfd, npfd)            vnode_page_replace(opfd, npfd)
#define premove(pfd)                    vnode_premove((pfd)->pf_vp, pfd)
#define premove_nolock(pfd)             vnode_premove((pfd)->pf_vp, pfd)
#define pagesrelease(pfd, cnt, flag)    vnode_pagesrelease((pfd)->pf_vp, pfd, cnt, flag)

extern lock_t 	mreg_lock;	/* spinlock protecting all vp->v_mreg */

extern vnode_t	*rootdir;	/* pointer to root vnode */
extern int	vn_epoch;	/* incore vnode capability - changed when */
				/*   vnodes are deallocated */
extern struct vfreelist_s *vfreelist; /* ptr to array of freelist structs */

#endif	/* _KERNEL */

#endif	/* _FS_VNODE_H */


