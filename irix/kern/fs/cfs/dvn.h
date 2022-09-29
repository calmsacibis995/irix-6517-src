/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_CFS_DVN_H_
#define	_CFS_DVN_H_
#ident	"$Id: dvn.h,v 1.55 1997/09/29 03:07:02 steiner Exp $"

/*
 * Internal vnode-related header file for the Cell File System.
 */
#include "dv.h"
#include <ksys/kqueue.h>
#include <sys/kmem.h>
#include <sys/ktrace.h>
#include <sys/pfdat.h>
#include <sys/alenlist.h>
#include <fs/cfs/cfs.h>
#include <fs/cell/fsc_types.h>
#include <fs/cell/dvn_tokens.h>
#include <sys/timers.h>

/*
 * Types.
 */
typedef uint	dvn_flags_t;	


/*
 * The following structure is used to pass times back
 * to the server when the shared times token is revoked.
 */

typedef struct ds_times {
	int	   ds_flags;
	timespec_t ds_atime;
	timespec_t ds_mtime;
	timespec_t ds_ctime;
} ds_times_t;

/* 
 * Client-side private data.
 * It helps performance a bit if the dcv_bd field is first (see BHV_TO_DCVN).
 */
struct dcvn {
	kqueue_t	dcv_kqueue;		/* chain of dcvn for hash */
	                                        /* MUST BE FIRST - queueing */
	dcvn_t          *dcv_next;		/* chain of dcvn for same vfs*/
	dcvn_t   	*dcv_prev;		/* chain of dcvn for same vfs*/
	bhv_desc_t	dcv_bd;			/* behavior descriptor */
	dvn_flags_t	dcv_flags;		/* dcvn flags */
	cfs_handle_t	dcv_handle;		/* handle to server object */
        dcvfs_t         *dcv_dcvfs;             /* dcvfs for file system */
        dcxvn_t         *dcv_dcxvn;             /* cxfs client vnode data */
	uint		dcv_vrgen;		/* VREMAPPING generation */
	uchar_t		dcv_empty_pcache_cnt;	/* count of pcache ops with empty pcache */
	vattr_t		dcv_vattr;		/* Cache copy of vattr */
	int		dcv_error;		/* error returned during token get */
	lock_t		dcv_lock;		/* spin lock when changing flag/times */
	TKC_DECL(dcv_tclient, DVN_NTOKENS);
};

/* 
 * Server-side private data.
 */
struct dsvn {
	kqueue_t	dsv_kqueue;		/* chain of dsvn's for hash */
	                                        /* MUST BE FIRST - queueing */
	bhv_desc_t	dsv_bd;			/* behavior descriptor */
	dvn_flags_t	dsv_flags;		/* dsvn flags */
	cfs_gen_t	dsv_gen;		/* dsvn generation number */
	mutex_t		dsv_mutex;		/* synchronization mutex */
        dsxvn_t         *dsv_dsxvn;             /* cxfs server vnode data */
	char		dsv_vrgen_flags;	/* flags for vrgen */
	TKC_DECL(dsv_tclient, DVN_NTOKENS);
	TKS_DECL(dsv_tserver, DVN_NTOKENS);
};

/*
 * Flag values for vrgen_flags
 */
#define VRGEN_ACTIVE		1


/*
 * Op code for client-side flush/inval operations.
 */
#define CFS_TOSS		1		 /* tosspages */
#define CFS_FLUSH		2		 /* flush*/
#define CFS_FLUSHINVAL		3	 	 /* flushinval */
#define CFS_INVALFREE		4		 /* invalfree */
#define CFS_SETHOLE		5		 /* sethole */


/*
 * Array size used to push modified pages pfn's back to
 * the server. Size of array should be as big as possible while
 * still allowing the RPC to fit within the XPC inline message.
 */
#define DSVN_PFN_LIST_SIZE	64


/*
 * If the server sends too many RPCs to a client that has no
 * pages in its page cache,  the client will return its PCACHE
 * token. This constant sets the threshold.
 */
#define DSV_EMPTY_PCACHE_RECALL_THRESHOLD	4


/*
 * Flags for all of: 
 * - dcvn's dcv_flags field 
 * - dsvn's dsv_flags field 
 * - flags out argument to lookup, create, open, and obtain_exist RPC's.
 *   (flags are needed whenever an existence token is given to a client;
 *   also see DSVN_CLIENT_FLAGS macro)
 *
 * When setting/clearing dcv_flags or dsv_flags, must use corresponding
 * flagset/flagclr macros.
 */
#define DVN_LOCK		0x01	/* lock bit for bitlock */
#define	DVN_MOUNTPOINT		0x02	/* a covered directory */
#define	DVN_NOTFOUND		0x04	/* vnode torn down or migr. */
#define	DVN_LINKZERO		0x08	/* vnode has no names */
#define	DVN_INACTIVE_TEARDOWN	0x10	/* teardown at inactive time */
#define	DVN_NOSWAP		0x20	/* file can't be used for swap */
#define	DVN_PRIVATE		0x40	/* not part of global vnode cache */
#define	DVN_DOCMP		0x80	/* vnode has special VOP_CMP impl. */
#define	DVN_FRLOCKS		0x100	/* vnode has FR locks applied */
#define	DVN_ENF_LOCKING		0x200	/* vnode has enforced mode locking */

#define	DVN_ATIME_MOD  		0x400	/* vnode has atime modified. */
#define	DVN_MTIME_MOD  		0x800	/* vnode has mtime modified. */
#define	DVN_CTIME_MOD  		0x1000	/* vnode has ctime modified. */

/*
 * Client-side macros.
 */
#define	DCVN_TO_BHV(dcp)	(&((dcp)->dcv_bd))
#define	DCVN_TO_VNODE(dcp)	BHV_TO_VNODE(&((dcp)->dcv_bd))
#define	DCVN_TO_HANDLE(dcp)	((dcp)->dcv_handle)
#define	DCVN_TO_SERVICE(dcp)	CFS_HANDLE_TO_SERVICE((dcp)->dcv_handle)
#define	DCVN_TO_OBJID(dcp)	CFS_HANDLE_TO_OBJID((dcp)->dcv_handle)
#define VNODE_TO_DCVN(vp)	dcvn_from_vnode(vp)
#define BHV_IS_DCVN(bdp)	(BHV_OPS(bdp) == &cfs_vnodeops)
#define BHV_TO_DCVN(bdp) 						\
	(ASSERT(BHV_IS_DCVN(bdp)), (dcvn_t *)BHV_PDATA(bdp))

/* dcvn spinlock manipulation */
#define	DCVN_LOCK(dcp)		mutex_bitlock(&(dcp)->dcv_flags, DVN_LOCK)
#define	DCVN_UNLOCK(dcp,s)	mutex_bitunlock(&(dcp)->dcv_flags, DVN_LOCK, s)
#define DCVN_FLAGSET(dcp,b)	bitlock_set(&(dcp)->dcv_flags, DVN_LOCK, b)
#define DCVN_FLAGCLR(dcp,b)	bitlock_clr(&(dcp)->dcv_flags, DVN_LOCK, b)

/*
 * Server-side macros.
 */
#define	DSVN_TO_BHV(dsp)	(&((dsp)->dsv_bd))
#define	DSVN_TO_VNODE(dsp)	BHV_TO_VNODE(DSVN_TO_BHV(dsp))
#define	DSVN_TO_OBJID(dsp)	((objid_t)DSVN_TO_BHV(dsp))
#define	DSVN_TO_GEN(dsp)	((dsp)->dsv_gen)
#define DSVN_HANDLE_MAKE(handle, dsp)					\
{									\
        CFS_HANDLE_MAKE(handle, DSVN_TO_BHV(dsp), DSVN_TO_GEN(dsp));	\
}
#define BHV_IS_DSVN(bdp)	(BHV_OPS(bdp) == &dsvn_ops)
#define BHV_TO_DSVN(bdp) 						\
	(ASSERT(BHV_IS_DSVN(bdp)), (dsvn_t *)BHV_PDATA(bdp))

/* dsvn spinlock manipulation */
#define	DSVN_LOCK(dsp)		mutex_bitlock(&(dsp)->dsv_flags, DVN_LOCK)
#define	DSVN_UNLOCK(dsp,s)	mutex_bitunlock(&(dsp)->dsv_flags, DVN_LOCK, s)
#define DSVN_FLAGSET(dsp,b)	bitlock_set(&(dsp)->dsv_flags, DVN_LOCK, b)
#define DSVN_FLAGCLR(dsp,b)	bitlock_clr(&(dsp)->dsv_flags, DVN_LOCK, b)

/* 
 * Get vnode/dsvn flags for a client.
 *
 * The caller should have an existence token on behalf of the client 
 * being given the flags.  This is to guarantee that the client will 
 * be updated with any flag changes that occur after DSVN_CLIENT_FLAGS
 * completes.  Conversely, any code that changes a flag should do so first 
 * _and then_ contact all clients that have existence tokens (and need to 
 * be aware of the flag change).  This ordering guarantees consistent flag 
 * state across all clients.
 */
#define DSVN_CLIENT_FLAGS(dsp, vp, flags)				\
{									\
        flags = (dsp)->dsv_flags;					\
        if ((vp)->v_vfsmountedhere)					\
		flags |= DVN_MOUNTPOINT;				\
        if ((vp)->v_flag & VFRLOCKS)					\
		flags |= DVN_FRLOCKS;					\
        if ((vp)->v_flag & VENF_LOCKING)				\
		flags |= DVN_ENF_LOCKING;				\
}

/*
 * Client-side externs.
 */
extern struct vnodeops 	cfs_vnodeops;
extern tkc_ifstate_t	dcvn_tclient_iface;

struct cred;

extern dcvn_t *         dcvn_import_force(cfs_handle_t *);
extern dcvn_t *         dcvn_from_vnode(vnode_t *);
extern int 		dcvn_handle_to_dcvn(cfs_handle_t *, tk_set_t,
					    enum vtype, dev_t, dcvfs_t *, 
					    int, int, char *, size_t, dcvn_t **);
extern int		dcvn_newdc_setup(dcvn_t *, vfs_t *, enum vtype, 
					 dev_t, int);
extern void		dcvn_newdc_install(dcvn_t *, tk_set_t, dcvfs_t *);
extern dcvn_t * 	dcvn_handle_lookup(cfs_handle_t *);
extern void		dcvn_handle_enter(dcvn_t *);
extern void		dcvn_handle_remove(dcvn_t *);
extern void		dcvn_destroy(dcvn_t *);

extern int		dcvn_update_page_flags(dcvn_t *, int, int);

extern void		dcvn_checkattr(dcvn_t *);
extern void		dcvn_count_return(tk_set_t);
extern void		dcvn_count_obtain(tk_set_t);
extern void		dcvn_count_obt_opti(tk_set_t);


/*
 * Server-side externs.
 */
extern tks_ifstate_t	dsvn_tserver_iface;
extern dsvn_t *		dsvn_vnode_to_dsvn(vnode_t *);
extern dsvn_t *		dsvn_vnode_to_dsvn_nolock(vnode_t *);
extern dsvn_t * 	dsvn_handle_lookup(cfs_handle_t *);
extern void		dsvn_handle_enter(dsvn_t *);
extern void		dsvn_handle_remove(dsvn_t *);
extern dsvn_t * 	dsvn_alloc(vnode_t *);
extern void		dsvn_destroy(dsvn_t *);
extern bhv_desc_t *	dsvn_vnode_to_dsbhv(vnode_t *);

/* Server-side vnode ops */
extern void		dsvn_teardown(dsvn_t *, bhv_desc_t *);
extern int		dsvn_read(bhv_desc_t *, struct uio *, int,
				struct cred *, struct flid *);
extern int		dsvn_write(bhv_desc_t *, struct uio *, int,
				struct cred *, struct flid *);
extern int		dsvn_getattr(bhv_desc_t *, struct vattr *, int,
				struct cred *);
extern int		dsvn_setattr(bhv_desc_t *, struct vattr *, int,
				struct cred *);
extern int		dsvn_create(bhv_desc_t *, char *, struct vattr *, int,
				int, vnode_t **, struct cred *);
extern int		dsvn_remove(bhv_desc_t *, char *, struct cred *);
extern int		dsvn_link(bhv_desc_t *, vnode_t *, char *,
				struct cred *);
extern int		dsvn_rename(bhv_desc_t *, char *, vnode_t *, char *,
				struct pathname *npnp, struct cred *);
extern int		dsvn_mkdir(bhv_desc_t *, char *, struct vattr *,
				vnode_t **, struct cred *);
extern int		dsvn_rmdir(bhv_desc_t *, char *, vnode_t *,
				struct cred *);
extern int		dsvn_readdir(bhv_desc_t *, struct uio *, struct cred *,
				int *);
extern int		dsvn_symlink(bhv_desc_t *, char *, struct vattr *,
				char *, struct cred *);
extern int		dsvn_readlink(bhv_desc_t *, struct uio *,
				struct cred *);
extern int		dsvn_allocstore(bhv_desc_t *, off_t, size_t, struct cred *);
extern int		dsvn_fcntl(bhv_desc_t *, int, void *, int, off_t,
				struct cred *, union rval *);
extern int		dsvn_inactive(bhv_desc_t *, struct cred *);
extern int		dsvn_reclaim(bhv_desc_t *, int);
extern void		dsvn_link_removed(bhv_desc_t *, vnode_t *, int);
extern void		dsvn_vnode_change(bhv_desc_t *, vchange_t, __psint_t);

extern void 		dsvn_tosspages(bhv_desc_t *, off_t, off_t, int);
extern void 		dsvn_flushinval_pages(bhv_desc_t *, off_t, off_t, int);
extern int 		dsvn_flush_pages(bhv_desc_t *, off_t, off_t, uint64_t, int);
extern void 		dsvn_invalfree_pages(bhv_desc_t *, off_t);
extern void 		dsvn_pages_sethole(bhv_desc_t *, pfd_t*, int, int, off_t);
extern void 		dsvn_remapf(bhv_desc_t *, off_t, off_t, int);
extern void		dsvn_strgetmsg(bhv_desc_t *, struct strbuf *,
					struct strbuf *, unsigned char *,
					int  *, int, union rval *, int *);
extern void		dsvn_strputmsg(bhv_desc_t *, struct strbuf *,
					struct strbuf *, unsigned char,
					int, int, int *);

/*
 * Tracing for dcvn's and dsvn's.
 */
#if DEBUG
#define DVN_TRACING 1
#else
#define DVN_TRACING 0
#endif

#if DVN_TRACING
#include <ksys/vproc.h>		/* for current_pid() */

#define DVN_TRACE_SIZE		1000
extern ktrace_t *dvn_trace_buf;
extern int	dvn_trace_mask;

/* Mask values for dvn_trace_mask. */
#define	DVN_TRACE_HASH		0x0001
#define	DVN_TRACE_RECYCLE	0x0002
#define	DVN_TRACE_RELOCATE	0x0004
#define	DVN_TRACE_DCVOP		0x0008
#define	DVN_TRACE_DSVOP		0x0010
#define	DVN_TRACE_TOKEN		0x0020
#define	DVN_TRACE_MISC		0x0040
#define	DVN_TRACE_DCRPC		0x0080
#define	DVN_TRACE_DSRPC		0x0100
#define	DVN_TRACE_ALL		0xFFFF

#define	DVN_KTRACE2(mask,a,b) { \
	if (dvn_trace_mask & mask) { \
		KTRACE6(dvn_trace_buf, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid()); \
	} \
}
#define	DVN_KTRACE4(mask,a,b,c,d) { \
	if (dvn_trace_mask & mask) { \
		KTRACE8(dvn_trace_buf, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d)); \
	} \
}
#define	DVN_KTRACE6(mask,a,b,c,d,e,f) { \
	if (dvn_trace_mask & mask) { \
		KTRACE10(dvn_trace_buf, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f)); \
	} \
}
#define	DVN_KTRACE8(mask,a,b,c,d,e,f,g,h) { \
	if (dvn_trace_mask & mask) { \
		KTRACE12(dvn_trace_buf, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h)); \
	} \
}
#define	DVN_KTRACE10(mask,a,b,c,d,e,f,g,h,i,j) { \
	if (dvn_trace_mask & mask) { \
		KTRACE14(dvn_trace_buf, \
			(a), (b), \
			"pid", current_pid(), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h), \
			(i), (j)); \
	} \
}

#else	/* DVN_TRACING */

#define	DVN_KTRACE2(mask,a,b)
#define	DVN_KTRACE4(mask,a,b,c,d)
#define	DVN_KTRACE6(mask,a,b,c,d,e,f)
#define	DVN_KTRACE8(mask,a,b,c,d,e,f,g,h)
#define	DVN_KTRACE10(mask,a,b,c,d,e,f,g,h,i,j) 

#endif	/* DVN_TRACING */

#endif	/* _CFS_DVN_H_ */
