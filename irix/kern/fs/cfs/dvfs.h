/**************************************************************************
 *									  *
 *		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_CFS_DVFS_H_
#define	_CFS_DVFS_H_

#ident	"$Revision: 1.19 $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/cell/tkm.h>
#include <sys/vfs.h>		/* for bhvtovfs */
#include <fs/cfs/cfs.h>
#include <fs/cell/fsc_types.h>

/*
 * Tokens
 */
#define DVFS_EXIST		0
#define DVFS_TRAVERSE		1
#define DVFS_NTOKENS		2

#define DVFS_EXIST_RD		TK_MAKE(DVFS_EXIST, TK_READ)
#define DVFS_EXIST_RD_SNGL	TK_MAKE_SINGLETON(DVFS_EXIST, TK_READ)
#define DVFS_EXIST_WR		TK_MAKE(DVFS_EXIST, TK_WRITE)
#define DVFS_EXIST_WR_SNGL	TK_MAKE_SINGLETON(DVFS_EXIST, TK_WRITE)
#define DVFS_TRAVERSE_RD	TK_MAKE(DVFS_TRAVERSE, TK_READ)
#define DVFS_TRAVERSE_RD_SNGL	TK_MAKE_SINGLETON(DVFS_TRAVERSE, TK_READ)
#define DVFS_TRAVERSE_WR	TK_MAKE(DVFS_TRAVERSE, TK_WRITE)
#define DVFS_TRAVERSE_WR_SNGL	TK_MAKE_SINGLETON(DVFS_TRAVERSE, TK_WRITE)
#define DVFS_EXIST_TRAVERSE_RD	TK_ADD_SET(DVFS_EXIST_RD, DVFS_TRAVERSE_RD)

/*
 * VFS DC mount structure(Client-side private data).
 */
struct dcvfs	{
	kqueue_t	dcvfs_kqueue;		/* must be first */
	bhv_desc_t	dcvfs_bhv;		/* behavior descriptor */
	cfs_handle_t	dcvfs_handle;		/* handle to server object */
	TKC_DECL(dcvfs_tclient, DVFS_NTOKENS);
	lock_t		dcvfs_lock;		/* protect dcvfs_dcvnq */
	dcvn_t  	*dcvfs_dcvlist;		/* client dcvn list */
	vnode_t		*dcvfs_rootvp;		/* client root diretory vnode */
	uint_t		dcvfs_reclaims;		/* number of vnode reclaims */
	dcxvfs_t	*dcvfs_xvfs;		/* cxfs client vfs data */
        cxfs_sinfo_t    *dcvfs_sinfo;           /* For server-capable cxfs fs's,
                                                   cxfs server info structure. */
	int		dcvfs_fstype;		/* fs type we are serving
						   (XFS, ...) */
};

 
/*
 * VFS DS mount structure(Server-side private data).
 */
struct dsvfs {
        bhv_desc_t      dsvfs_bhv;              /* behavir descriptor */
	cfs_gen_t	dsvfs_gen;
	int		dsvfs_flags;	
	lock_t		dsvfs_lock;
        cell_t          dsvfs_unmounter;
        TKC_DECL(dsvfs_tclient, DVFS_NTOKENS);
        TKS_DECL(dsvfs_tserver, DVFS_NTOKENS);
};

/* dsvfs_flags */
#define DSVFS_UNMOUNTING	1


/* Client side macros */
#define VFSOPS_IS_DCVFS(ops)		((ops) == &cfs_vfsops)
#define BHV_TO_DCVFS(bhv)		\
	(ASSERT(BHV_OPS(bhv) == &cfs_vfsops), (dcvfs_t *)BHV_PDATA(bhv))
#define DCVFS_TO_BHV(dcp) 		(&(dcp)->dcvfs_bhv)
#define DCVFS_TO_OBJID(dcp)		CFS_HANDLE_TO_OBJID(dcp->dcvfs_handle)
#define DCVFS_TO_SERVICE(dcp)		CFS_HANDLE_TO_SERVICE((dcp)->dcvfs_handle)
#define DCVFS_TO_VFS(dcp)		bhvtovfs(DCVFS_TO_BHV(dcp))

/* Server side macros */
#define DSVFS_TO_GEN(dsp)		(cfs_gen_t)(dsp->dsvfs_gen)
#define DSVFS_TO_OBJID(dsp)		(objid_t)(&dsp->dsvfs_bhv)
#define DSVFS_MKHANDLE(hdl, dsp)	\
	CFS_HANDLE_MAKE(hdl, DSVFS_TO_OBJID(dsp), DSVFS_TO_GEN(dsp))
#define VFSOPS_IS_DSVFS(ops)		((ops) == &cfs_dsvfsops)
#define DSVFS_TO_BHV(dsp) 		(&(dsp)->dsvfs_bhv)
#define DSVFS_TO_VFS(dsp)		bhvtovfs(DSVFS_TO_BHV(dsp))
#define BHV_TO_DSVFS(bhv)		\
	(ASSERT(BHV_OPS(bhv) == &cfs_dsvfsops), (dsvfs_t *)BHV_PDATA(bhv))

extern int xfs_fstype;

#define DS_FS_IS_XFS(vp)		((vp)->v_vfsp->vfs_fstype == xfs_fstype)
#define DC_FS_IS_XFS(dcvfsp)		((dcvfsp)->dcvfs_fstype == xfs_fstype)

/*
 * The following macro returns true if the file system
 * allows remote caching of it's attributes.
 */
extern int nfs_fstype;

#define DC_FS_ATTR_CACHE(dcvfsp)	((dcvfsp)->dcvfs_fstype != nfs_fstype)

struct cred;

extern vfs_t *dcvfs_import_mounthierarchy(cfs_handle_t *);
extern int dcvfs_import_mountpoint(cfs_handle_t *, vfs_t *, cfs_handle_t *);
extern dcvfs_t *dcvfs_handle_lookup(cfs_handle_t *);
extern int dcvfs_delayed_mount(dcvn_t *);
extern int dcvfs_prepare_unmount(dcvfs_t *, cell_t);
extern void dcvfs_commit_unmount(dcvfs_t *, cell_t);
extern int dcvfs_check_unmount(dcvfs_t *);
extern void dcvfs_dcvn_insert(dcvn_t *, dcvfs_t *);
extern void dcvfs_dcvn_delete(dcvn_t *, dcvfs_t *);
extern int dcvfs_vnbusy(dcvfs_t *);
extern void dcvfs_dummy(int, cfs_handle_t *);
extern dcvfs_t *dcvfs_vfs_to_dcvfs(vfs_t *);

extern tks_iter_t dsvfs_abort_unmount(void *, tks_ch_t, tk_set_t, va_list); 
extern int dsvfs_dounmount_subr(bhv_desc_t *, int, struct vnode *, 
				struct cred *, cell_t);
extern void dsvfs_dummy_setup(void);
extern dsvfs_t *dsvfs_allocate(vfs_t *);

extern vfsops_t	cfs_vfsops;
extern vfsops_t	cfs_dsvfsops;

extern tks_ifstate_t dsvfs_tsif;


#endif /* _DVFS_H_ */

