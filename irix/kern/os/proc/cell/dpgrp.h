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
#ifndef	_PGRP_DPGRP_H_
#define	_PGRP_DPGRP_H_
#ident "$Id: dpgrp.h,v 1.12 1997/10/14 14:29:04 cp Exp $"

#include <ksys/behavior.h>
#include <ksys/cell/service.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>
#include <ksys/vpgrp.h>
#include <ksys/cell/wp.h>
#include <ksys/cell/object.h>
#include <ksys/cell/relocation.h>
#include "dproc.h"

/*
 * Token class for distributed process group synchronization.
 */
#define	VPGRP_TOKEN_EXIST	0
#define	VPGRP_TOKEN_MEMBER	1
#define	VPGRP_TOKEN_NONORPHAN	2
#define	VPGRP_TOKEN_ITERATE	3	/* aka MEMBER_RELOCATING */
#define	VPGRP_NTOKENS		4

/*
 * The EXISTENCE token ensures that a pgrp exists.
 * Although it may be empty.
 */
#define	VPGRP_EXISTENCE_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_EXIST, TK_READ)
#define	VPGRP_EXISTENCE_TOKENSET	\
	TK_MAKE(VPGRP_TOKEN_EXIST, TK_READ)

/*
 * The MEMBER token is held by each cell where a group member resides.
 * When the MEMBER token is obtained (from the server) the current
 * signal sequence number is returned. This sequence number is subsequently
 * updated locally on member sites and is used to synchronize exit/wait
 * in the case where both parent and child are in the same pgrp and parent
 * must have had posted all pgrp (more especially job control) signals seen
 * by the child.
 */
#define	VPGRP_MEMBER_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_MEMBER, TK_READ)
#define	VPGRP_MEMBER_TOKENSET		\
	TK_MAKE(VPGRP_TOKEN_MEMBER, TK_READ)

/*
 * The NONORPHAN token is held by any (member) cell with a member
 * or members which prevent the pgrp being orphaned (that is, a
 * member process exists with a parent outside the pgrp but in the
 * same session).
 */
#define	VPGRP_NONORPHAN_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_NONORPHAN, TK_READ)
#define	VPGRP_NONORPHAN_TOKENSET	\
	TK_MAKE(VPGRP_TOKEN_NONORPHAN, TK_READ)

/*
 * The NO_MEMBER_RELOCATING token gives the server permission to
 * iterate over member cells. It is temporarily revoked (by a pgrp member
 * taking the opposing multi-writer token, MEMBER_RELOCATING) during
 * process migration. In particular, this prevents pgrp signals being
 * missed or duplicated during migration.
 */
#define	VPGRP_NO_MEMBER_RELOCATING_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_ITERATE, TK_READ)
#define	VPGRP_NO_MEMBER_RELOCATING_TOKENSET		\
	TK_MAKE(VPGRP_TOKEN_ITERATE, TK_READ)
#define	VPGRP_MEMBER_RELOCATING_TOKEN			\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_ITERATE, TK_SWRITE)
#define	VPGRP_MEMBER_RELOCATING_TOKENSET		\
	TK_MAKE(VPGRP_TOKEN_ITERATE, TK_SWRITE)

/*
 * XXX Not implemented.
 * The ATTR token must be held (at the appropriate level) to access/update
 * pgrp attributes: batch state.
 */
#define VPGRP_ATTR_READ_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_ATTR, TK_READ)
#define VPGRP_ATTR_READ_TOKENSET	\
	TK_MAKE(VPGRP_TOKEN_ATTR, TK_READ)
#define VPGRP_ATTR_WRITE_TOKEN		\
	TK_MAKE_SINGLETON(VPGRP_TOKEN_ATTR, TK_WRITE)
#define VPGRP_ATTR_WRITE_TOKENSET	\
	TK_MAKE(VPGRP_TOKEN_ATTR, TK_WRITE)
 
typedef struct dcpgrp {
	TKC_DECL(dcpg_tclient, VPGRP_NTOKENS);
	obj_handle_t	dcpg_handle;
	bhv_desc_t	dcpg_bhv;
} dcpgrp_t;

typedef struct dspgrp {
	TKC_DECL(dspg_tclient, VPGRP_NTOKENS);
	TKS_DECL(dspg_tserver, VPGRP_NTOKENS);
	mutex_t		dspg_mem_lock;
	int		dspg_notify_idle;
	obj_state_t	dspg_obj_state;
	bhv_desc_t	dspg_bhv;
} dspgrp_t;

#define DSPGRP_MEM_LOCK_INIT(dsp)	\
	mutex_init(&(dsp)->dspg_mem_lock, MUTEX_DEFAULT, "pgrp_mem")
#define DSPGRP_MEM_LOCK_DESTROY(dsp)	mutex_destroy(&(dsp)->dspg_mem_lock)
#define DSPGRP_MEM_IS_LOCKED(dsp)	mutex_mine(&(dsp)->dspg_mem_lock)
#define DSPGRP_MEM_LOCK(dsp)		mutex_lock(&(dsp)->dspg_mem_lock, PZERO)
#define DSPGRP_MEM_UNLOCK(dsp)		mutex_unlock(&(dsp)->dspg_mem_lock)

/*
 * Macros.
 */
#define BHV_TO_DCPGRP(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dcpgrp_ops), \
	(dcpgrp_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DSPGRP(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dspgrp_ops), \
	(dspgrp_t *)(BHV_PDATA(bdp)))

#define	DCPGRP_TO_SERVICE(dcp)	HANDLE_TO_SERVICE((dcp)->dcpg_handle)
#define	DCPGRP_TO_OBJID(dcp)	HANDLE_TO_OBJID((dcp)->dcpg_handle)

extern pgrp_ops_t dspgrp_ops;
extern pgrp_ops_t dcpgrp_ops;

static __inline service_t
pgid_to_service(pid_t pgid)
{
	service_t	svc = pid_to_service(pgid);
	svc.s_svcnum &= ~SVC_PROCESS_MGMT;
	svc.s_svcnum = SVC_PGRP;
	return svc;
}
#define pgid_is_local(pgid)	pid_is_local(pgid)

#define DSPGRP_RETARGET_CHECK(dsp)				\
	OBJ_SVR_RETARGET_CHECK(&((dsp)->dspg_obj_state))

extern int dcpgrp_create(int, service_t, vpgrp_t **);
extern vpgrp_t *dspgrp_create(pid_t, pid_t);
extern void vpgrp_interpose(vpgrp_t *vpgrp);

extern void idbg_dspgrp_print(struct dspgrp *);
extern void idbg_dcpgrp_print(struct dcpgrp *);
extern void idbg_ppgrp_print(struct pgrp *);

#endif	/* _PGRP_DPGRP_H_ */
