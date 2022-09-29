/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_VSHM_MGR_H
#define	_VSHM_MGR_H 1
#ident "$Id: vshm_mgr.h,v 1.7 1997/03/05 20:17:09 beck Exp $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/vshm.h>

/*
 * vshm manager data structs and routines
 * These are the basic lookup routines
 */
/*
 * basic lookup table - shm segments are hashed by 'id'
 */
typedef struct vshmtable {
	kqueue_t	vst_queue;
	mrlock_t	vst_lock;
} vshmtab_t;

#define VSHMTAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->vst_lock, (mode), PZERO); \
				}
#define VSHMTAB_UNLOCK(vq)	mrunlock(&(vq)->vst_lock)

#define SHM_IDMAX	0x7fffffff	/* Largest +ve 32 bit int */

extern void vshm_mgr_init(void);
extern int vshm_lookup_id_local(int, vshm_t **);
extern void vshm_freeid_local(int);
extern void vshm_enter(vshm_t *, int);
extern void vshm_remove(vshm_t *, int);
extern int  vshm_is_removed(vshm_t *);
extern int vshm_getstatus_local(struct shmstat *, struct cred *);
extern int vshm_allocid(int);
extern void vshm_freeid_local(int);
extern void vshm_destroy(vshm_t *);
extern void vshm_qlookup(vshmtab_t *, int, vshm_t **);
extern int vshm_idclear(key_t);
extern int vshm_islocal(bhv_desc_t *);
extern int vshm_create_id(int, int, int, size_t, pid_t, struct cred *, vshm_t **);
#if DEBUG
extern int vshm_onkqueue(kqueue_t *kq, vshm_t *tv, int);
#endif


#endif
