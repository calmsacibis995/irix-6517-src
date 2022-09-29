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
#ifndef	_SHM_DSHM_H_
#define	_SHM_DSHM_H_
#ident "$Id: dshm.h,v 1.21 1997/08/18 02:14:23 steiner Exp $"

#include <ksys/behavior.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/relocation.h>
#include <ksys/cell/tkm.h>
#include <ksys/vshm.h>
#include <ksys/cell/wp.h>

#define	VSHM_TOKEN_EXIST	0
#define	VSHM_NTOKENS		1

#define	VSHM_EXISTENCE_TOKEN	TK_MAKE_SINGLETON(VSHM_TOKEN_EXIST, TK_READ)
#define	VSHM_EXISTENCE_TOKENSET	TK_MAKE(VSHM_TOKEN_EXIST, TK_READ)

typedef struct dcshm {
	TKC_DECL(dcsm_tclient, VSHM_NTOKENS);
	obj_handle_t	dcsm_handle;
	bhv_desc_t	dcsm_bd;
} dcshm_t;

typedef struct dsshm {
	TKC_DECL(dssm_tclient, VSHM_NTOKENS);
	TKS_DECL(dssm_tserver, VSHM_NTOKENS);
	obj_state_t	dssm_obj_state;
	bhv_desc_t	dssm_bd;
} dsshm_t;

/*
 * Macros:
 */
#define	DCSHM_TO_SERVICE(dcp)	HANDLE_TO_SERVICE((dcp)->dcsm_handle)
#define	DCSHM_TO_OBJID(dcp)	HANDLE_TO_OBJID((dcp)->dcsm_handle)
#define	OBJID_TO_DSSHM(objid)	BHV_TO_DSSHM(OBJID_TO_BHV(objid))
#define	DSSHM_TO_VSHM(dsp)	BHV_TO_VSHM(&(dsp)->dssm_bd)

/*
 * Check for race with relocating object, waiting if we've
 * arrived at the target cell before the object has.
 */
#define DSSHM_RETARGET_CHECK(dsp)					\
	OBJ_SVR_RETARGET_CHECK(&((dsp)->dssm_obj_state))

extern cell_t vshmid_to_server_cell(int);
extern int vshm_getstatus_remote(service_t, struct cred *, struct shmstat *);
extern int dcshm_create(int, service_t, vshm_t **);
extern void vshm_interpose(vshm_t *vshm);
extern void vshm_freeid_remote(service_t, int);

extern wp_domain_t shm_wp_iddomain;

#if defined(DEBUG) || defined(SHMDEBUG)
extern void	idbg_dcshm_print(dcshm_t *);
extern void	idbg_dsshm_print(dsshm_t *);
#endif
#endif	/* _SHM_DSHM_H_ */
