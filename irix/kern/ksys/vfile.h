/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: vfile.h,v 1.23 1998/04/23 23:25:10 kanoj Exp $"
#ifndef _KSYS_VFILE_H     /* wrapper symbol for kernel use */
#define _KSYS_VFILE_H     /* subject to change without notice */

#include <sys/types.h>
#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <sys/vnode.h>
#include <sys/flock.h>
#include <sys/file.h>

/*
 * One file structure is allocated for each open/creat/pipe call.
 * Main use is to hold the read/write pointer associated with
 * each open file.
 * f_lock  protects:
 *	vf_flag
 *	vf_count
 *	vf_msgcount
 */
typedef struct vfile {
	bhv_head_t	vf_bh;		/* behavior head */
	lock_t		vf_lock;	/* spinlock for vf_flag and vf_count */
	int		vf_flag;
	int		vf_count;	/* reference count */
	int		vf_msgcount;	/* Unix domain msg reference count */
	void		*__vf_data__;	/* DON'T ACCESS DIRECTLY */
	struct cred	*vf_cred;	/* credentials of user who opened it */
#ifdef CKPT
	union {
		caddr_t	cu_mate;	/* sockets mate (socketpair) */
		int	cu_ckpt;	/* ckpt lookup info */
	} vf_cpr;
#endif
	short		vf_iopri;	/* file io priority */
#ifdef DEBUG
	struct vfile	*vf_next;	/* pointer to next entry */
	struct vfile	*vf_prev;	/* pointer to previous entry */
#endif
} vfile_t;

#define	vf_ckpt	vf_cpr.cu_ckpt
/*
 * Macros for accessing a vfile's vnode or vsocket.
 * It's one or the other, so must Always check which one it is.
 */
#define VF_TO_VNODE(vfp)	\
	(ASSERT(!((vfp)->vf_flag & FSOCKET)), (vnode_t *)(vfp)->__vf_data__)
#define VF_TO_VSOCK(vfp)	\
	(ASSERT((vfp)->vf_flag & FSOCKET), (vsock_t *)(vfp)->__vf_data__)

#define VF_IS_VNODE(vfp)	(!((vfp)->vf_flag & FSOCKET))
#define VF_IS_VSOCK(vfp)	((vfp)->vf_flag & FSOCKET)

#define VF_SET_DATA(vfp, vp_or_vs) ((vfp)->__vf_data__ = (void *)(vp_or_vs))
#define VF_DATA_EQU(vfp, vp_or_vs) ((vfp)->__vf_data__ == (void *)(vp_or_vs))

/*
 * V conversion macros.
 */
#define BHV_TO_VFILE(bdp) \
	((vfile_t *)BHV_VOBJ(bdp))

/*
 * Definition of the vfile ops list
 */
typedef struct vfileops {
        bhv_position_t  vfile_position;   /* position within behavior chain */
        void    (*vfile_setflags)(bhv_desc_t *, int, int);
        void    (*vfile_getoffset)(bhv_desc_t *, off_t *);
        void    (*vfile_setoffset)(bhv_desc_t *, off_t);
        void    (*vfile_getoffset_locked)(bhv_desc_t *, int, off_t *);
        void    (*vfile_setoffset_locked)(bhv_desc_t *, int, off_t);
        void    (*vfile_teardown)(bhv_desc_t *);
	int	(*vfile_close)(bhv_desc_t *, int);
} vfileops_t ;

#define vf_fbhv        vf_bh.bh_first         /* first behavior */
#define vf_fops        vf_bh.bh_first->bd_ops /* ops for first behavior */

#define VFILE_SETFLAGS(v, am, om) \
        { \
        BHV_READ_LOCK(&(v)->vf_bh);      \
        (*((vfileops_t *)(v)->vf_fops)->vfile_setflags)((v)->vf_fbhv, am, om); \
        BHV_READ_UNLOCK(&(v)->vf_bh);    \
        }

#define VFILE_GETOFFSET(v, o) \
        { \
        BHV_READ_LOCK(&(v)->vf_bh);      \
        (*((vfileops_t *)(v)->vf_fops)->vfile_getoffset)((v)->vf_fbhv, o); \
        BHV_READ_UNLOCK(&(v)->vf_bh);    \
        }

#define VFILE_SETOFFSET(v, o) \
        { \
        BHV_READ_LOCK(&(v)->vf_bh);      \
        (*((vfileops_t *)(v)->vf_fops)->vfile_setoffset)((v)->vf_fbhv, o); \
        BHV_READ_UNLOCK(&(v)->vf_bh);    \
        }

#define VFILE_GETOFFSET_LOCKED(v, l, o) \
        { \
        BHV_READ_LOCK(&(v)->vf_bh);      \
        (*((vfileops_t *)(v)->vf_fops)->vfile_getoffset_locked)((v)->vf_fbhv, l, o); \
        /* unlock done in VFILE_SETOFFSET_LOCKED */ \
        }

#define VFILE_SETOFFSET_LOCKED(v, l, o) \
        { \
        /* lock done in VFILE_GETOFFSET_LOCKED */ \
        (*((vfileops_t *)(v)->vf_fops)->vfile_setoffset_locked)((v)->vf_fbhv, l, o); \
        BHV_READ_UNLOCK(&(v)->vf_bh);    \
        }

#define VFILE_TEARDOWN(v) \
        { \
        BHV_WRITE_LOCK(&(v)->vf_bh);      \
        (*((vfileops_t *)(v)->vf_fops)->vfile_teardown)((v)->vf_fbhv); \
        BHV_WRITE_UNLOCK(&(v)->vf_bh);    \
        }

#define VFILE_CLOSE(v, f, rv) \
        { \
        BHV_READ_LOCK(&(v)->vf_bh);      \
        rv = (*((vfileops_t *)(v)->vf_fops)->vfile_close)((v)->vf_fbhv, f); \
        BHV_READ_UNLOCK(&(v)->vf_bh);    \
        }

#define	VFILE_SETPRI(v, pri)	v->vf_iopri = pri
#define	VFILE_GETPRI(v, pri)	pri = v->vf_iopri

/* Invalid file offset - indicates do not update offset */
#define	VFILE_NO_OFFSET	-1

/*
 * Values for flag arg to VFILE_CLOSE.
 */
#define VFILE_FIRSTCLOSE 	0	/* first of 2 calls from vfile_close */
#define	VFILE_SECONDCLOSE	1	/* 2nd of 2 calls from vfile_close */

/*
 * lastclose_info flags returned from VFILE_CLOSE.
 */
#define VFILE_ISLASTCLOSE 	0x1	/* lastclose=true in VOP_CLOSE call */
#define	VFILE_SKIPCLOSE		0x2	/* skip VOP_CLOSE call */
#define	VFILE_REFREL		0x4	/* called for ref release, not close */

/*
 * Routines and macros dealing with user per-open file flags and
 * user open files.  
 */
#define	VFLOCK(vfp)		mp_mutex_spinlock(&(vfp)->vf_lock)
#define	VFUNLOCK(vfp, s)	mp_mutex_spinunlock(&(vfp)->vf_lock, s)

#define	VFILE_REF_HOLD(vfp) { int _fhold_s = VFLOCK(vfp); \
			  ASSERT((vfp)->vf_count); (vfp)->vf_count++; \
			  VFUNLOCK(vfp, _fhold_s); }
#define	VFILE_REF_RELEASE(vfp)	vfile_ref_release(vfp)

/* vfile externs */
extern void 		vfile_init(void);
extern int 		vfile_alloc(int, vfile_t **, int *);
extern vfile_t * 	vfile_create(int);
extern void 		vfile_destroy(vfile_t *);
extern void 		vfile_ready(vfile_t *, void *);
extern void	 	vfile_alloc_undo(int, vfile_t *);
extern int 		vfile_close(vfile_t *);
extern int 		vfile_assign(struct vnode **, int, int*);
extern void 		vfile_cell_init(void);
extern void 		vfile_ref_release(vfile_t *);

/*
 * VFILE tag in object bag
 */
#define VFILE_BAG_TAG OBJ_SVC_TAG(SVC_VFILE, 0)

/* pfile externs */
extern void 		pfile_create(vfile_t *);

#endif  /* _KSYS_VFILE_H */
