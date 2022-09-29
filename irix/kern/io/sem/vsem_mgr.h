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
#ifndef	_VSEM_MGR_H
#define	_VSEM_MGR_H 1
#ident "$Id: vsem_mgr.h,v 1.1 1997/07/30 22:19:27 sp Exp $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/vsem.h>

/*
 * vsem manager data structs and routines
 * These are the basic lookup routines
 */
/*
 * basic lookup table - sem segments are hashed by 'id'
 */
typedef struct vsemtable {
	kqueue_t	vst_queue;
	mrlock_t	vst_lock;
} vsemtab_t;

#define VSEMTAB_LOCK(vq, mode)	{ \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->vst_lock, (mode), PZERO); \
				}
#define VSEMTAB_UNLOCK(vq)	mrunlock(&(vq)->vst_lock)

#define SEM_IDMAX	0x7fffffff	/* Largest +ve 32 bit int */

extern void vsem_mgr_init(void);
extern int vsem_lookup_id_local(int, vsem_t **);
extern void vsem_freeid_local(int);
extern void vsem_enter(vsem_t *, int);
extern void vsem_remove(vsem_t *, int);
extern int  vsem_is_removed(vsem_t *);
extern int vsem_getstatus_local(struct semstat *, struct cred *);
extern int vsem_allocid(void);
extern void vsem_freeid_local(int);
extern void vsem_destroy(vsem_t *);
extern void vsem_qlookup(vsemtab_t *, int, vsem_t **);
extern int vsem_idclear(key_t);
extern int vsem_islocal(bhv_desc_t *);
extern int vsem_create_id(int, int, int, int, struct cred *, vsem_t **);
#if DEBUG
extern int vsem_onkqueue(kqueue_t *kq, vsem_t *tv, int);
#endif


#endif
