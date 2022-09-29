/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.3 $"

#include <sys/kthread.h>
#include <ksys/xthread.h>
#include <ksys/cell/subsysid.h>
#include "kthread_cell.h"
#include "I_thread_stubs.h"
#include "invk_thread_stubs.h"

void
thread_remote_init()
{
	mesg_handler_register(thread_msg_dispatcher, DTHREAD_SUBSYSID);
}

int
thread_remote_interrupt(kthread_t *kt, int *s)
{
	int		interrupted;
	/* REFERENCED */
	int		msgerr;
	service_t	svc;

	svc = kt->k_remote_svc;
	if (SERVICE_IS_NULL(svc))
		return(0);

	/*
	 * Keep hitting on the remote cell until we get the thread or
	 * it moves on. If it moves on then it will carry the interrupted
	 * bit with it so we don't need to do any more.
	 */
	while (SERVICE_EQUAL(kt->k_remote_svc, svc)) {
		kt_unlock(kt, *s);
		msgerr = invk_thread_interrupt(svc, kt, cellid(), &interrupted);
		ASSERT(!msgerr);
		*s = kt_lock(kt);
		if (interrupted)
			return(1);
	}
	return(0);
}

void
I_thread_interrupt(
	void	*id,
	cell_t	cell,
	int	*interrupted)
{
	xthread_t	*xt;
	int		spl;
	extern xthread_t xthreadlist;
	extern lock_t xt_list_lock;

	*interrupted = 0;
	spl = mutex_spinlock(&xt_list_lock);
        for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		transinfo_t	*ti;

                if (!xt->xt_info)
                        continue;
                ti = (transinfo_t *)xt->xt_info;
		kt_nested_lock(XT_TO_KT(xt));
		if (ti->ti_trans == NULL) {
			kt_nested_unlock(XT_TO_KT(xt));
			continue; /* Missed it */
		}
		if (ti->ti_trans->tr_id == id &&
		    ti->ti_cell == cell) {
			nested_spinunlock(&xt_list_lock);
			thread_interrupt(XT_TO_KT(xt), &spl);
			*interrupted = 1;
			kt_unlock(XT_TO_KT(xt), spl);
			return;
		}
		kt_nested_unlock(XT_TO_KT(xt));
        }
	mutex_spinunlock(&xt_list_lock, spl);
}
