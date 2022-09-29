/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_KSYS_REMOTE_H_
#define	_KSYS_REMOTE_H_	1

#ifndef CELL
#error included by non-CELL configuration
#endif

#include <ksys/cell.h>
#include <sys/systm.h>
#include <ksys/vproc.h>
#include <ksys/xthread.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <ksys/cred.h>
#include <ksys/cell/service.h>

extern void	mesg_svc_push(kthread_t *);
extern void 	mesg_svc_pop(kthread_t *);


typedef struct {
	void	*tr_id;
	pid_t	tr_pid;
	credid_t tr_credid;
	uthreadid_t tr_utid;
	u_char	tr_interrupted;
	u_char	tr_abi;
} trans_t;

typedef int	tres_t;

typedef struct transinfo {
	uint		ti_id;
	cell_t		ti_cell;
	trans_t		*ti_trans;
	cred_t		*ti_cred;
	int		ti_channo;
	/* The following are only needed for mesglist */
	time_t		ti_start;
	uint		ti_subsys:16,
			ti_opcode:16;
} transinfo_t;

__inline credid_t
get_current_credid(void)
{
	if (!curuthread) {
		if (KT_CUR_ISXTHREAD() &&
		    KT_TO_XT(curthreadp)->xt_info) {
			transinfo_t	*ti;

			ti = (transinfo_t *)KT_TO_XT(curthreadp)->xt_info;
			if(ti->ti_trans)
				return(ti->ti_trans->tr_credid);
		}
		return(CREDID_NULLID);
	}

	return(cred_getid(get_current_cred()));
}

__inline void
remote_transaction_start(
	service_t	svc,
	trans_t		*trans,
	credid_t	credid)
{
	if (!SERVICE_IS_NULL(curthreadp->k_remote_svc))
		mesg_svc_push(curthreadp);
	curthreadp->k_remote_svc = svc;
	trans->tr_interrupted = curthreadp->k_flags & KT_INTERRUPTED;
	trans->tr_id = (void *)curthreadp;
	trans->tr_pid = current_pid();
	trans->tr_abi = get_current_abi();
	trans->tr_utid = current_utid();
	trans->tr_credid = credid;
				
}

__inline void
remote_transaction_end(
	tres_t		*tres,
	int		*res)
{
	if (curthreadp->k_mesg_stack)
		mesg_svc_pop(curthreadp);
	else
		SERVICE_MAKE_NULL(curthreadp->k_remote_svc);

	if (res)
		*res = *tres;
}

__inline void
start_remote_service(
	trans_t		*trans)
{
	transinfo_t	*ti;
	int		s;

	/*
	 * If we are a uthread then this must be a callback
	 * so we don't do all this stuff
	 */
	if (curuthread == NULL) {
		ASSERT(KT_TO_XT(curthreadp)->xt_info);
		ti = KT_TO_XT(curthreadp)->xt_info;
		ti->ti_cred = NULL;
		/*
		 * Synchronize with remote interruption
		 */
		s = kt_lock(curthreadp);
		ti->ti_trans = trans;
		if (trans->tr_interrupted)
			curthreadp->k_flags |= KT_INTERRUPTED;
		kt_unlock(curthreadp, s);
	}
}

__inline void
end_remote_service(
	tres_t		*tres,
	int		res)
{
	transinfo_t	*ti;
	int		s;

	/*
	 * If we are a uthread then this must be a callback
	 * so we don't do all this stuff
	 */
	if (curuthread == NULL) {
		ti = KT_TO_XT(curthreadp)->xt_info;
		/*
		 * Synchronize with remote interruption
		 */
		s = kt_lock(curthreadp);
		ti->ti_trans = NULL;
		curthreadp->k_flags &= ~KT_INTERRUPTED; /* just in case */
		kt_unlock(curthreadp, s);

		if (ti->ti_cred)
			crfree(ti->ti_cred);
	}
	*tres = res;
}

#endif	/* _KSYS_REMOTE_H_ */
