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
#ident  "$Id: dfile_private.h,v 1.12 1997/11/10 23:41:54 ethan Exp $"
#ifndef	_FILE_DFILE_PRIVATE_H_
#define	_FILE_DFILE_PRIVATE_H_

/*
 * Internal definitions for the distributed file table
 */
#include <ksys/kqueue.h>
#include <ksys/cell/tkm.h>
#include <sys/kmem.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/relocation.h>

/*
 * Tokens - there is a shared (multiple reader) existence token, and a 
 * not shared (single writer) offset token. 
 */
#define	DFILE_EXIST_NUM		0
#define DFILE_OFFSET_NUM	1
#define	DFILE_NTOKENS		2

#define	DFILE_EXISTENCE_TOKENSET	TK_MAKE(DFILE_EXIST_NUM, TK_READ)
#define	DFILE_EXISTENCE_TOKEN	TK_MAKE_SINGLETON(DFILE_EXIST_NUM, TK_READ)

#define DFILE_OFFSET_TOKENSET	TK_MAKE(DFILE_OFFSET_NUM, TK_WRITE)
#define DFILE_OFFSET_TOKEN	TK_MAKE_SINGLETON(DFILE_OFFSET_NUM, TK_WRITE)

#define VFILE_POSITION_DS	BHV_POSITION_BASE+1
#define VFILE_POSITION_DC	BHV_POSITION_BASE+2

/* 
 * Client-side private data.
 */

typedef struct dcfile {
	kqueue_t	dcf_kqueue;		/* chain of dcfile_t's for hash */
	                                        /* MUST BE FIRST */
	bhv_desc_t	dcf_bd;			/* behavior descriptor */
	off_t		dcf_offset;		/* cached file offset */
	mutex_t		dcf_offlock;		/* mutex for dcf_offset */
	obj_handle_t	dcf_handle;		/* handle to server object */
	int		dcf_offset_valid;	/* debug */
	TKC_DECL(dcf_tclient, DFILE_NTOKENS);
} dcfile_t;

typedef struct dcfiletable {
        kqueue_t        dct_queue;
        mrlock_t        dct_lock;
} dcfiletab_t;

#define DCFILETAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->dct_lock, (mode), PZERO); \
				}
#define DCFILETAB_UNLOCK(vq)	mrunlock(&(vq)->dct_lock)

#define DCFILE_HASH(handle)	\
		(((unsigned long)HANDLE_TO_OBJID(handle)) % dcfiletabsz)

typedef struct dsfiletable {
        kqueue_t        dct_queue;
        mrlock_t        dct_lock;
} dsfiletab_t;

#define DSFILETAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->dct_lock, (mode), PZERO); \
				}
#define DSFILETAB_UNLOCK(vq)	mrunlock(&(vq)->dct_lock)

#define DSFILE_HASH(handle)	\
		(((unsigned long)HANDLE_TO_OBJID(handle)) % dsfiletabsz)


extern void dcfile_cell_init(void);
extern void dsfile_cell_init(void);

/* 
 * Server-side private data.
 */

typedef struct dsfile {
	kqueue_t	dsf_kqueue;		/* chain of dsfile_t's for hash */
	                                        /* MUST BE FIRST */
	bhv_desc_t	dsf_bd;			/* behavior descriptor */
	mutex_t		dsf_flaglock;		/* mutex for setflags op */
	long		dsf_offset_fetches;	/* number of remote offset ops */
	obj_handle_t	dsf_handle;		/* server handle */
	obj_state_t	dsf_obj_state;
	int		dsf_remote_refs;	/* remote ref count */
	TKC_DECL(dsf_tclient, DFILE_NTOKENS);
	TKS_DECL(dsf_tserver, DFILE_NTOKENS);
} dsfile_t;

/*
 * Client-side macros.
 */

#define	DCFILE_TO_BHV(dcp)	(&((dcp)->dcf_bd))
#define BHV_TO_DCFILE(bdp) \
        (ASSERT(BHV_OPS(bdp) == &dcfile_ops), \
        (dcfile_t *)(BHV_PDATA(bdp)))
#define DCFILE_TO_SERVICE(dcp)	HANDLE_TO_SERVICE(dcp->dcf_handle)
#define DCFILE_TO_OBJID(dcp)	HANDLE_TO_OBJID(dcp->dcf_handle)
#define DCFILE_TO_VFILE(dcp)	(vfile_t *)(BHV_TO_VFILE(DCFILE_TO_BHV(dcp)))

/*
 * Server-side macros.
 */

#define	DSFILE_TO_BHV(dsp)	(&((dsp)->dsf_bd))
#define BHV_TO_DSFILE(bdp) \
        (ASSERT(BHV_OPS(bdp) == &dsfile_ops), \
        (dsfile_t *)(BHV_PDATA(bdp)))
#define DSFILE_TO_VFILE(dsp)	(vfile_t *)(BHV_TO_VFILE(DSFILE_TO_BHV(dsp)))
#define VFILE_TO_DSFILE(vfp)	\
	(ASSERT(BHV_OPS(BHV_HEAD_FIRST(&vfp->vf_bh)) == &dsfile_ops), \
	(dsfile_t *)(BHV_PDATA(BHV_HEAD_FIRST(&vfp->vf_bh))))

/*
 * Generic externs.
 */
extern vfileops_t  dcfile_ops;
extern vfileops_t  dsfile_ops;
extern vfileops_t  pfile_ops;

/*
 * Client-side externs.
 */
extern void dcfile_insert_handle (obj_handle_t *, dcfile_t *);
extern int vfile_obj_target_prepare(obj_manifest_t *, void **);
extern int vfile_obj_target_unbag(obj_manifest_t*, obj_bag_t);
void dcfile_create (dcfile_t **, vfile_t *, obj_handle_t *);
extern int dcfile_lookup_handle (obj_handle_t *, dcfile_t **);


/*
 * Server-side externs.
 */
extern int dsfile_reference_handle (obj_handle_t *, vfile_t **);
extern void dsfile_insert_handle (obj_handle_t *, dsfile_t *);
extern dsfile_t * dsfile_interpose (vfile_t *, cell_t);

/*
 * pfile externs
 */
extern void pfile_getoffset (bhv_desc_t * bdp, off_t *offp);
extern void pfile_setoffset (bhv_desc_t * bdp, off_t off);
extern void pfile_getoffset_locked (bhv_desc_t * bdp, int lock, off_t *offp);
extern void pfile_setoffset_locked (bhv_desc_t * bdp, int lock, off_t off);
extern void pfile_teardown (bhv_desc_t * bdp);
extern void pfile_target_migrate (vfile_t *, off_t);
extern int vfile_import_mft(obj_manifest_t*, vfile_t **);

/*
 * vfile bag structure
 * the vnode/vsock stuff follows this in the bag
 */
typedef struct vfile_ei_bag1 {
	obj_handle_t	server_handle;
	vfile_t		server_vfile;	/* copy of server vfile */
	int		existence_token_granted;
	void		*v;	/* source V object */
} vfile_ei_bag1_t;

typedef struct vfile_ei_bag2 {
	obj_handle_t	server_handle;
	vfile_t		server_vfile;	/* copy of server vfile */
	off_t		offset;		/* offset if migrate_vp */
	int		remote_ref_count;
} vfile_ei_bag2_t;

/*
 * Vfile import/export support
 */

#include <sys/vsocket.h>
#include <fs/cfs/cfs.h>
#include <bsd/vsock/cell/dsvsock.h>

typedef struct vfile_export {
	obj_handle_t	vfe_handle;
	obj_handle_t	vfe_exporter;
	int		vfe_flag;
	union {
		cfs_handle_t	vu_cfs;
		vsock_handle_t	vu_vsock;
	} vfe_u;
} vfile_export_t;

#define	vfe_vnexport		vfe_u.vu_cfs
#define vfe_vsock_export	vfe_u.vu_vsock

extern void vfile_export(vfile_t *, vfile_export_t *);
extern void vfile_export_end(vfile_t *);
extern void vfile_import(vfile_export_t *, vfile_t **);

extern dsfile_t *vfile_to_dsfile(vfile_t *);

#if DEBUG
#define VFILE_DBG(a) a++
#else
#define VFILE_DBG(a)
#endif

#endif	/* _FILE_DFILE_PRIVATE_H_ */
