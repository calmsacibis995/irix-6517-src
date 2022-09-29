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
 * 
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

#ifndef _FS_VFS_H	/* wrapper symbol for kernel use */
#define _FS_VFS_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:fs/vfs.h	1.6"*/
#ident	"$Revision: 1.57 $"

/*
 * Data associated with mounted file systems.
 */

#include <sys/types.h>
#include <sys/sema.h>
#include <ksys/behavior.h>
#include <sys/uio.h>

/*
 * File system identifier. Should be unique (at least per machine).
 */
typedef struct {
	uint val[2];			/* file system id type */
} fsid_t;

/*
 * File identifier.  Should be unique per filesystem on a single
 * machine.  This is typically called by a stateless file server
 * in order to generate "file handles".
 */
#define MAXFIDSZ	46
#define freefid(fidp) \
  kmem_free((caddr_t)(fidp), sizeof (struct fid) - MAXFIDSZ + (fidp)->fid_len)

typedef struct fid {
	u_short		fid_len;		/* length of data in bytes */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
} fid_t;

/*
 * Structure per mounted file system.  Each mounted file system has
 * an array of operations and an instance record.  The file systems
 * are kept on a doubly linked list headed by "rootvfs" and terminated
 * by NULL.
 *
 * MP locking protocols:
 *	vfs_next, vfs_prevp			vfslock global spinlock
 *	vfs_vnodecovered			vfs_lock/vfs_unlock
 *	vfs_flag 				vfslock global spinlock
 *	vfs_nsubmounts				vfs_lock/vfs_unlock
 *	vfs_bcount				unprotected
 *	vfs_busycnt				vfslock global spinlock
 *	all others				readonly once mounted
 */
typedef struct vfs {
	struct vfs	*vfs_next;		/* next VFS in VFS list */
	struct vfs	**vfs_prevp;		/* ptr to previous vfs_next */ 
	struct vnode	*vfs_vnodecovered;	/* vnode mounted on */
	int		old_vfs_dcount;		/* count of dirty pages */
	u_int		vfs_flag;		/* flags */
	dev_t		vfs_dev;		/* device of mounted VFS */
	u_short		vfs_nsubmounts;		/* immediate sub-mount count */
	short		vfs_busycnt;		/* # of accesses in progress */
	sv_t		vfs_wait;		/* busy mount point sync */
	u_int		vfs_bsize;		/* native block size */
	int		vfs_fstype;		/* file system type index */
	fsid_t		vfs_fsid;		/* file system id */
	u_long		vfs_bcount;		/* I/O count (accounting) */
	struct mac_vfs	*vfs_mac;		/* MAC information */
	struct acl_vfs	*vfs_acl;		/* UNUSED: placeholder for
						   future functionality */
	struct cap_vfs	*vfs_cap;		/* UNUSED: placeholder for
						   future functionality */
	fsid_t		*vfs_altfsid;		/* An ID fixed for life of FS */
	bhv_head_t	vfs_bh;			/* head of vfs behavior chain */
	__uint64_t	vfs_opsver;             /* vfsops/vnodeops version in
						   effect for vfs and all of
                                                   its vnode's */
	__uint64_t      vfs_opsflags;           /* vfsops/vnodeops flags */
/*
 *      The following fields are used in the Nexus mount logic.  They form
 *      a communication path on the directory cell among various instances 
 *      of the image of a file system on various cells.
 */
#if CELL
	cell_t		vfs_cell;		/* device cell for this FS */
        char            *vfs_expinfo;           /* data exported by home instance
                                                   of file system. */
        size_t          vfs_eisize;             /* amount of data exported by
                                                   home instance of file system */
#endif

	int		vfs_dcount;		/* count of dirty pages */
} vfs_t;

#define vfs_fbhv	vfs_bh.bh_first		/* 1st on vfs behavior chain */
#define VFS_FOPS(vfsp)	\
        ((vfsops_t *)((vfsp)->vfs_fbhv->bd_ops))/* ops for 1st behavior */
#define bhvtovfs(bdp)	((struct vfs *)BHV_VOBJ(bdp))
#define VFS_BHVHEAD(vfsp) &(vfsp)->vfs_bh
#define VFS_CHECK_OPSVER(vfsp, ver) \
	((vfsp)->vfs_opsver >= (ver))	/* check interface version */
#define VFS_CHECK_OPSFLAG(vfsp, flag) \
        ((vfsp)->vfs_opsflags & (flag)) /* check interface flag */
#define VFS_EILIMIT     256             /* maximum size for exported info */

/*
 * VFS flags.
 */
#define VFS_RDONLY	0x0001		/* read-only vfs */
#define VFS_NOSUID	0x0002		/* setuid disallowed */
#define VFS_NODEV	0x0004		/* disallow opening of device files */
#define VFS_GRPID	0x0008		/* group-ID assigned from directory */
#define VFS_REMOUNT	0x0010		/* modify mount options only */
#define VFS_NOTRUNC	0x0020		/* does not truncate long file names */
#define VFS_UNLINKABLE	0x0040		/* unlink(2) can be applied to root */
#define VFS_BADBLOCK	0x0080		/* disk based fs has bad block */
#define VFS_MLOCK	0x0100		/* lock vfs so that subtree is stable */
#define VFS_MWAIT	0x0200		/* waiting for access lock */
#define VFS_MWANT	0x0400		/* waiting for update */
#define VFS_CELLULAR	0x0800		/* fs is cellularized */
#define VFS_LOCAL	0x1000		/* local filesystem, for find */
#define VFS_OFFLINE	0x2000		/* filesystem is being unmounted */
#define VFS_DMI		0x4000		/* filesystem has the DMI enabled */
#define VFS_CELLROOT    0x8000          /* root vnode is cell-local and should
                                           not be exported */
#define VFS_DOXATTR	0x10000		/* tell server to trust us with
					   attributes */
#define VFS_DEFXATTR	0x20000		/* use default values for
					   system attributes */

/* 
 * the scope of these bits is within a cell, when adding new bits to
 * vfs_flag one should also add those bits in VFS_CEEL_BITS
 */
#define VFS_CELL_BITS \
	(VFS_REMOUNT|VFS_MLOCK|VFS_MWAIT|VFS_MWANT|VFS_OFFLINE)
#define VFS_FLAGS_PRESET -1		/* don't change vfs_flags in vfs_add */

/*
 * VFS_SYNC flags.
 */
#define SYNC_NOWAIT	0x0000		/* start delayed writes */
#define SYNC_ATTR	0x0001		/* sync attributes */
#define SYNC_CLOSE	0x0002		/* close file system down */
#define SYNC_DELWRI	0x0004		/* look at delayed writes */
#define SYNC_WAIT	0x0008		/* wait for i/o to complete */
#define SYNC_BDFLUSH	0x0010		/* BDFLUSH is calling -- don't block */
#define SYNC_FSDATA	0x0020		/* flush fs data (e.g. superblocks) */
#define SYNC_PDFLUSH	0x0040		/* push v_dpages */
#define SYNC_UNMOUNT	0x0080		/* unmounting filesystem */

/*
 * VFS_UNMOUNT flags
 */
#define UNMOUNT_CKONLY	0x01            /* only check for use.  don't complete
                                           the unmount */
#define UNMOUNT_FINAL   0x02            /* complete unmount. Can not fail */

#ifdef _KERNEL
/*
 * Argument structure for mount(2).
 */
struct mounta {
	char	*spec;
	char	*dir;
	sysarg_t flags;
	char	*fstype;
	char	*dataptr;
	sysarg_t datalen;
};
#endif

/*
 * Reasons for calling the vfs_mountroot() operation.
 */
enum whymountroot { ROOT_INIT, ROOT_REMOUNT, ROOT_UNMOUNT };
typedef enum whymountroot whymountroot_t;
struct vnode;
struct statvfs;
struct mounta;
struct cred;

/*
 * Operations supported on virtual file system.
 */
typedef struct vfsops {
	bhv_position_t	vf_position;	/* position within behavior chain */
	int	(*vfs_mount)(struct vfs *, struct vnode *,
			     struct mounta *, char *, struct cred *);
					/* mount file system */
	int	(*vfs_rootinit)(struct vfs *); 
					/* 1st mount of root fs */
	int	(*vfs_mntupdate)(bhv_desc_t *, struct vnode *,
				 struct mounta *, struct cred *);
                                        /* remount file system */
	int	(*vfs_dounmount)(bhv_desc_t *, int, struct vnode *, 
				 struct cred *);
					/* preparation and unmount */
	int	(*vfs_unmount)(bhv_desc_t *, int, struct cred *);
					/* unmount file system */
	int	(*vfs_root)(bhv_desc_t *, struct vnode **);
					/* get root vnode */
	int	(*vfs_statvfs)(bhv_desc_t *, struct statvfs *, struct vnode *);
					/* get file system statistics */
	int	(*vfs_sync)(bhv_desc_t *, int, struct cred *);
					/* flush files */
	int	(*vfs_vget)(bhv_desc_t *, struct vnode **, struct fid *);
					/* get vnode from fid */
	int	(*vfs_mountroot)(bhv_desc_t *, enum whymountroot);
					/* mount the root filesystem */
	int	(*vfs_realvfsops)(vfs_t *, struct mounta *, struct vfsops **);
					/* remap the original vfsops */
        int     (*vfs_import)(vfs_t *);
	
	int	(*vfs_quotactl)(bhv_desc_t *, int, int, caddr_t);
	                                /* disk quota */
	int	(*vfs_force_pinned)(bhv_desc_t *);
					/* force all pinned buffers */
	int     vfs_ops_magic;          /* magic number for intfc extensions */
        __uint64_t   vfs_ops_version;	/* interface version number */
        __uint64_t   vfs_ops_flags;	/* interface flags */

} vfsops_t;

#define VFS_MNTUPDATE(vfsp, mvp, uap, cr, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_mntupdate))((vfsp)->vfs_fbhv, mvp, uap, cr);    \
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_DOUNMOUNT(vfsp,f,vp,cr, rv)	\
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_dounmount))((vfsp)->vfs_fbhv, f, vp, cr);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_UNMOUNT(vfsp,f,cr, rv)	\
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_unmount))((vfsp)->vfs_fbhv, f, cr);		\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_ROOT(vfsp, vpp, rv)		\
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_root))((vfsp)->vfs_fbhv, vpp);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_STATVFS(vfsp, sp, vp, rv) 	\
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_statvfs))((vfsp)->vfs_fbhv, sp, vp);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_SYNC(vfsp, flag, cr, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_sync))((vfsp)->vfs_fbhv, flag, cr);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_VGET(vfsp, vpp, fidp, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_vget))((vfsp)->vfs_fbhv, vpp, fidp);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_MOUNTROOT(vfsp, why, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_mountroot))((vfsp)->vfs_fbhv, why);	\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_QUOTACTL(vfsp, cmd, id, addr, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_quotactl))((vfsp)->vfs_fbhv, cmd, id, addr);\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}
#define VFS_FORCE_PINNED(vfsp, rv) \
{	\
	BHV_READ_LOCK(&(vfsp)->vfs_bh); \
	rv = (*(VFS_FOPS(vfsp)->vfs_force_pinned))((vfsp)->vfs_fbhv);\
	BHV_READ_UNLOCK(&(vfsp)->vfs_bh); \
}

#define VFSOPS_MOUNT(vfs_op, vfsp, mvp, uap, attrs, cr, rv) \
	rv = (*(vfs_op)->vfs_mount)(vfsp, mvp, uap, attrs, cr)
#define VFSOPS_ROOTINIT(vfs_op, vfsp, rv) \
	rv = (*(vfs_op)->vfs_rootinit)(vfsp)
#define VFSOPS_REALVFSOPS(vfs_op, vfsp, uap, nvfsop, rv) \
	rv = (*(vfs_op)->vfs_realvfsops)(vfsp, uap, nvfsop)
#define VFSOPS_IMPORT(vfs_op, vfsp, rv) \
        rv = (*(vfs_op)->vfs_import)(vfsp)
#define VFSOPS_MAGIC	410199865

/*
 * Registration for the positions at which the different vfs behaviors 
 * are chained.  When on the same chain, a behavior with a higher position 
 * number is invoked before one with a lower position number.
 * 
 * Typically, the traditional file systems (xfs, procfs,...) will be the
 * BASE behavior.
 */
#define	VFS_POSITION_BASE	BHV_POSITION_BASE   	/* chain bottom */
#define	VFS_POSITION_TOP	BHV_POSITION_TOP    	/* chain top */
#define	VFS_POSITION_INVALID	BHV_POSITION_INVALID   	/* invalid pos. num */

#define	VFS_POSITION_CFS_DS	VFS_POSITION_TOP  	/* CFS server */
#define	VFS_POSITION_CFS_DC	VFS_POSITION_BASE  	/* CFS client */
#define VFS_POSITION_DIST_LC    VFS_POSITION_BASE+1  	/* Local client interposer
                                                           for distributed fs's */ 
#define VFS_POSITION_DIST_LS    VFS_POSITION_BASE+2  	/* Local server interposer
                                                           for distributed fs's */ 

/*
 * Filesystem type switch table.
 */
typedef struct vfssw {
	char		*vsw_name;		/* type name string */
	int		(*vsw_init)(struct vfssw *, int); /* init routine */
	vfsops_t	*vsw_vfsops;		/* filesystem operations */
	struct vnodeops	*vsw_vnodeops;		/* file operations */
	void		*vsw_fill;		/* was dying vnodeops - lboot
						 * still wants to fill in
						 */
	long		vsw_flag;		/* flags */
} vfssw_t;

#ifdef _KERNEL

#define VFS_FSTYPE_ANY		-1	/* fstype arg to vfs_devsearch* , 	
					   vfs_busydev if the filesystem type
					   is irrelevant for the search */

/*
 * Public operations.
 */
struct vnode;
union rval;
extern void	vfs_mountroot(void);	/* mount the root */
extern void	vfs_add(struct vnode *, struct vfs *, int);
extern void	vfs_simpleadd(struct vfs *);
					/* add a new vfs to mounted vfs list */
extern void	vfs_remove(struct vfs *);
					/* remove a vfs from mounted vfs list */
extern vfs_t    *vfs_allocate(void);    /* Allocate a new vfs. */
extern vfs_t    *vfs_allocate_dummy(int, dev_t);
					/* Allocate a new dummy vfs. */
extern void     vfs_deallocate(vfs_t *);/* Deallocate a vfs. */
extern int	vfs_lock(struct vfs *);	/* lock and unlock a vfs */
extern void	vfs_unlock(struct vfs *);
extern int	vfs_force_pinned(dev_t); /* force pinned buffers on device */
extern int	vfs_busy(struct vfs *);	/* mark busy for serial sync/unmount */
extern int      vfs_busy_trywait_mrlock(struct vfs *, mrlock_t *, int *);
extern int      vfs_busy_trywait_vnlock(struct vfs *, struct vnode *, 
					int, int *);
extern vfs_t	*vfs_busydev(dev_t, int);/* to keep dev from unmounting */
extern void	vfs_unbusy(struct vfs *);
extern vfs_t 	*getvfs(fsid_t *);	/* return vfs given fsid */
extern vfs_t 	*vfs_devsearch(dev_t, int);/* find vfs given device & opt. type */
extern vfs_t 	*vfs_devsearch_nolock(dev_t, int); /* find vfs given device:nolock */
extern vfssw_t 	*vfs_getvfssw(char *);	/* find vfssw ptr given fstype name */
extern u_long	vf_to_stf(u_long);	/* map VFS flags to statfs flags */
extern int	dounmount(struct vfs *, int, struct cred *);
					/* unmount a vfs */
extern int cmount(struct mounta *, union rval *, char *, enum uio_seg);
extern void vfs_insertbhv(vfs_t *, bhv_desc_t *, vfsops_t *, void *);
					/* link vfs base behavior with vfs */
extern int	vfs_lock_offline(struct vfs *);
extern void 	vfs_unbusy_wakeup(struct vfs *);
extern void 	vfs_mounthwgfs(void);    /* mount the root */
extern void 	vfs_syncall(int);
extern void 	vfs_syncroot(int);
/*
 * Callers must use kmem_zalloc to allocate a vfs.
 * initialize the bhv head  structure, really just the
 * behavior chain mrlock. Also initializing vfs_wait mutex to queue waiters 
 * for vfs_busycnt.
 */
#define VFS_INIT(vfsp) { \
	sv_init(&(vfsp)->vfs_wait, SV_DEFAULT, "vfs_wait"); \
        bhv_head_init(VFS_BHVHEAD(vfsp),"vfs"); \
}

#define VFS_FREE(vfsp) {	\
	sv_destroy(&(vfsp)->vfs_wait); \
	bhv_head_destroy(VFS_BHVHEAD(vfsp)); \
}

#define VFS_REMOVEBHV(vfsp, bdp)\
{	\
	bhv_remove(VFS_BHVHEAD(vfsp), bdp); \
}

/*
 * Set and clear vfs_flag bits.
 */
#define vfs_setflag(vfsp,f)	{ int _vfs_s = mp_mutex_spinlock(&vfslock); \
				  (vfsp)->vfs_flag |= (f); \
				  mp_mutex_spinunlock(&vfslock, _vfs_s); }
#define vfs_clrflag(vfsp,f)	{ int _vfs_s = mp_mutex_spinlock(&vfslock); \
				  (vfsp)->vfs_flag &= ~(f); \
				  mp_mutex_spinunlock(&vfslock, _vfs_s); }

#define vfs_spinlock()		mp_mutex_spinlock(&vfslock)
#define vfs_spinunlock(s)	mp_mutex_spinunlock(&vfslock,s)
#define vfsp_wait(V,P,S)	mp_sv_wait(&(V)->vfs_wait,P,&vfslock,s)
#define vfsp_waitsig(V,P,S)	mp_sv_wait_sig(&(V)->vfs_wait,P,&vfslock,s)

/*
 * Managing changes to the vfssw table
 */

#define vfssw_spinlock()	mp_mutex_spinlock(&vfsswlock)
#define vfssw_spinunlock(s)	mp_mutex_spinunlock(&vfsswlock,s)

/*
 * Setting export info which is only needed in the cell configuration
 */
#if     CELL
#define VFS_EXPINFO(vfsp, addr, siz) { \
        (vfsp)->vfs_expinfo = (char *) (addr); \
        (vfsp)->vfs_eisize = (siz); \
}
#else  /* CELL */
#define VFS_EXPINFO(vfsp, addr, siz)
#endif  /* CELL */

/*
 * Globals.
 */
struct zone;
extern vfs_t 		rootvfs_data;	/* root vfs */
extern vfs_t 		*rootvfs;	/* ptr to root vfs */
extern lock_t 		vfslock;	/* protects rootvfs and vfs_flag */
extern lock_t 		vfsswlock;	/* protects vfssw table itself */
extern struct zone 	*pn_zone;	/* pathname zone */

extern struct vfssw vfssw[];		/* table of filesystem types */
extern char rootfstype[];		/* name of root fstype */
extern int nfstype;			/* # of elements in vfssw array */

#endif /* _KERNEL */

#endif	/* _FS_VFS_H */






