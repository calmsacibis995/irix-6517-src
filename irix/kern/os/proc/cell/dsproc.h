/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_OS_PROC_DSPROC_H_
#define	_OS_PROC_DSPROC_H_ 1

#include "dproc.h"

typedef struct dsproc {
	TKC_DECL(dsp_tclient, VPROC_NTOKENS);
	TKS_DECL(dsp_tserver, VPROC_NTOKENS);
	bhv_desc_t	dsp_bhv;
	mutex_t		dsp_lock;
	sv_t		dsp_wait;
	short		dsp_flags;
	short		dsp_fsync_cnt;
} dsproc_t;

#define BHV_TO_DSPROC(b) \
		(ASSERT(BHV_POSITION(b) == VPROC_BHV_DS), \
		(dsproc_t *)(BHV_PDATA(b)))

/*
 * Flags
 */
#define	VPROC_EMIGRATING	0x1
#define	VPROC_IMMIGRATING	0x2
#define	VPROC_STATE_CHANGING	0x4
#define	VPROC_CLIENT_CREATING	0x8

#define VPROC_MIGRATION_CHECK(dsp) { \
	dsproc_lock(dsp); \
	while ((dsp)->dsp_flags & VPROC_IMMIGRATING) { \
		sv_wait(&(dsp)->dsp_wait, PZERO, &(dsp)->dsp_lock, 0); \
		dsproc_lock(dsp); \
	} \
	dsproc_unlock(dsp); \
}

#define	dsproc_lock_init(dsp) \
			mutex_init(&(dsp)->dsp_lock, MUTEX_DEFAULT, "dsproc")
#define	dsproc_lock_destroy(dsp) \
			mutex_destroy(&(dsp)->dsp_lock)
#define dsproc_lock(dsp)	mutex_lock(&(dsp)->dsp_lock, PZERO)
#define dsproc_unlock(dsp)	mutex_unlock(&(dsp)->dsp_lock)

#define dsproc_client_create_start(dsp) \
		dsproc_flagsync_start((dsp), \
			VPROC_CLIENT_CREATING, \
			VPROC_STATE_CHANGING);

#define dsproc_client_create_end(dsp) \
		dsproc_flagsync_end((dsp), VPROC_CLIENT_CREATING);

#define dsproc_state_change_start(dsp) { \
	int	_error; \
	do { \
		_error = dsproc_flagsync_start((dsp), \
				VPROC_STATE_CHANGING, \
				VPROC_CLIENT_CREATING); \
	} while (_error != 0); \
}

#define dsproc_state_change_end(dsp) \
		dsproc_flagsync_end((dsp), VPROC_STATE_CHANGING);

extern int vproc_to_dsproc(vproc_t *, dsproc_t **);
extern dsproc_t *dsproc_alloc(vproc_t *);
extern void dsproc_free(vproc_t *, dsproc_t *);

#endif	/* _OS_PROC_DSPROC_H_ */
