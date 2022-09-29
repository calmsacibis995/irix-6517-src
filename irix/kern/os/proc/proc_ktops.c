/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/uthread.h>
#include <sys/kthread.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include "pproc_private.h"

/*
 * KTOPS for procs
 */
static cred_t *
proc_get_cred(kthread_t *kt)
{
	ASSERT(KT_ISUTHREAD(kt));
	return KT_TO_UT(kt)->ut_cred;
}

static void
proc_set_cred(kthread_t *kt, cred_t *cr)
{
	ASSERT(KT_ISUTHREAD(kt));
	ASSERT(cr->cr_ref > 0);
	KT_TO_UT(kt)->ut_cred = cr;
}

static char *
proc_get_name(kthread_t *kt)
{
	ASSERT(KT_ISUTHREAD(kt));
	return UT_TO_PROC(KT_TO_UT(kt))->p_comm;
}

static void
proc_update_inblock(kthread_t *kt, long val)
{
	ASSERT(KT_ISUTHREAD(kt));
	atomicAddLong(&KT_TO_UT(kt)->ut_pproxy->prxy_ru.ru_inblock, val);
}

static void
proc_update_oublock(kthread_t *kt, long val)
{
	ASSERT(KT_ISUTHREAD(kt));
	atomicAddLong(&KT_TO_UT(kt)->ut_pproxy->prxy_ru.ru_oublock, val);
}

static void
proc_update_msgsnd(kthread_t *kt, long val)
{
	ASSERT(KT_ISUTHREAD(kt));
	atomicAddLong(&KT_TO_UT(kt)->ut_pproxy->prxy_ru.ru_msgsnd, val);
}

static void
proc_update_msgrcv(kthread_t *kt, long val)
{
	ASSERT(KT_ISUTHREAD(kt));
	atomicAddLong(&KT_TO_UT(kt)->ut_pproxy->prxy_ru.ru_msgrcv, val);
}

kt_ops_t proc_ktops = {
	proc_get_cred,
	proc_set_cred,
	proc_get_name,
	proc_update_inblock,
	proc_update_oublock,
	proc_update_msgsnd,
	proc_update_msgrcv
};

