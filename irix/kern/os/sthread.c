/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.49 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/ksignal.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include <sys/rt.h>
#include <sys/runq.h>
#include <sys/schedctl.h>
#include <sys/systm.h>
#include <ksys/sthread.h>
#include <ksys/isthread.h>

/*
 * Service Threads.
 * These can run most low level code - filesystems, networking, device drivers.
 *
 * - They don't handle signals.
 * - They can be traced with par (currently show as proc 0 ..)
 * - fuser can't find them.
 *
 * TODO (Potentially)
 * - mustrun (or should that be part of kthread?)
 * - really fix par to handle kthread ids
 * - /proc/sthread interface
 */

sthread_t sthreadlist;		/* the thread list */
static zone_t *st_zone;		/* sthreads allocation pool */
lock_t st_list_lock;		/* controls list */
static kt_ops_t sthreadops;

void
sthread_init(void)
{
	int zone_size = 1;

        /* XXX
	 * Work-around until zone allocator
	 * guarantees double word alignment
	 */
	while (zone_size < sizeof(sthread_t))
		zone_size *= 2;

	st_zone = kmem_zone_init(zone_size, "sthread");
	spinlock_init(&st_list_lock, "st_list_lock");
	sthreadlist.st_next = sthreadlist.st_prev = &sthreadlist;
}

/* ARGSUSED */
int
sthread_create(char *name,
		caddr_t stack_addr,
		uint_t stack_size,
		uint_t flags,
		uint_t pri,
		uint_t schedflags,
		st_func_t func,
		void *arg0,
		void *arg1,
		void *arg2,
		void *arg3)
{
	sthread_t *st;
	kthread_t *kt;
	int s;

	/* XXX will need to have perhaps a pool or no-sleep options .. */
	st = kmem_zone_zalloc(st_zone, KM_SLEEP|VM_DIRECT);
	ASSERT(((__psint_t)st & 0x7) == 0); /* must be double aligned */
	st->st_cred = sys_cred;
	if (name)
		strncpy(st->st_name, name, sizeof(st->st_name));

	kt = ST_TO_KT(st);
	kthread_init(kt, stack_addr,
		stack_size, KT_STHREAD, pri, schedflags, &sthreadops);

	s = mutex_spinlock(&st_list_lock);
	/* add to list */
	sthreadlist.st_next->st_prev = st;
	st->st_next = sthreadlist.st_next;
	sthreadlist.st_next = st;
	st->st_prev = &sthreadlist;
	mutex_spinunlock(&st_list_lock, s);

	/*
	 * set up registers to start and to call sthread_launch
	 * PCB_SP set up in kthread_init
	 * For 32 bit kernels make sure function pointers get sign extended
	 */
	kt->k_regs[PCB_S0] = (k_machreg_t)PTR_EXT(arg0);
	kt->k_regs[PCB_S1] = (k_machreg_t)PTR_EXT(arg1);
	kt->k_regs[PCB_S2] = (k_machreg_t)PTR_EXT(arg2);
	kt->k_regs[PCB_S3] = (k_machreg_t)PTR_EXT(arg3);
	kt->k_regs[PCB_FP] = (k_smachreg_t)PTR_EXT(func);
	kt->k_regs[PCB_PC] = (k_smachreg_t)PTR_EXT(&sthread_launch);

	/* place on runq */
	s = kt_lock(kt);
	if (kt->k_flags & KT_BIND)
		start_rt(kt, kt->k_pri);
	ktimer_init(kt, AS_RUNQ_WAIT);
	putrunq(kt, CPU_NONE);
	kt_unlock(kt, s);
	return 0;
}

kthread_t *
sthread0_init(void)
{
	sthread_t *st;
	kthread_t *kt;
	int s;

	/* XXX will need to have perhaps a pool or no-sleep options .. */
	st = kmem_zone_zalloc(st_zone, KM_SLEEP|VM_DIRECT);
	ASSERT(((__psint_t)st & 0x7) == 0); /* must be double aligned */
	st->st_cred = sys_cred;
	strncpy(st->st_name, "thread0", sizeof(st->st_name));

	kt = ST_TO_KT(st);
	kthread_init(kt, NULL, (2 * KTHREAD_DEF_STACKSZ),
		KT_STHREAD, 255, KT_PS, &sthreadops);

	s = mutex_spinlock(&st_list_lock);
	/* add to list */
	sthreadlist.st_next->st_prev = st;
	st->st_next = sthreadlist.st_next;
	sthreadlist.st_next = st;
	st->st_prev = &sthreadlist;
	mutex_spinunlock(&st_list_lock, s);
	return kt;
}

void
sthread_exit(void)
{
	int s;
	kthread_t *kt = curthreadp;
	sthread_t *st = KT_TO_ST(kt);

	s = kt_lock(kt);
	end_rt(kt);
	kt_unlock(kt, s);

	/* remove from list */
	s = mutex_spinlock(&st_list_lock);
	st->st_next->st_prev = st->st_prev;
	st->st_prev->st_next = st->st_next;
	mutex_spinunlock(&st_list_lock, s);

	/*
	 * this gets us off our stack and onto the IDLE stack
	 * which queues us for the reaper thread to call stdestroy
	 * to finally remove all the memory
	 */
	istexitswtch();
	/* NOTREACHED */
}

/*
 * final throes of sthread - this is called from the reaper thread
 */
void
stdestroy(sthread_t *st)
{
	/* release any kthread stuff */
	kthread_destroy(ST_TO_KT(st));
	kmem_zone_free(st_zone, st);
}

/*
 * KTOPS for sthreads
 */
static cred_t *
sthread_get_cred(kthread_t *kt)
{
	return KT_TO_ST(kt)->st_cred;
}

static void
sthread_set_cred(kthread_t *kt, cred_t *cr)
{
	ASSERT(cr->cr_ref > 0);
	KT_TO_ST(kt)->st_cred = cr;
}

static char *
sthread_get_name(kthread_t *kt)
{
	return KT_TO_ST(kt)->st_name;
}

/*ARGSUSED*/
static void
sthread_update_null(kthread_t *kt, long val)
{
	ASSERT(KT_ISSTHREAD(kt));
}

static kt_ops_t sthreadops = {
	sthread_get_cred,
	sthread_set_cred,
	sthread_get_name,
	sthread_update_null,
	sthread_update_null,
	sthread_update_null,
	sthread_update_null
};
