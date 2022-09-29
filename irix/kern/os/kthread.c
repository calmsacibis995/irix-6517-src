/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.90 $"

/*
 * kthreads: thread creation, etc.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/sema_private.h>
#include <sys/threadcfg.h>
#include <sys/cmn_err.h>
#include <sys/numa.h>
#include <sys/mmci.h>

#include <ksys/sthread.h>
#include <ksys/xthread.h>

#ifdef CELL
#include <ksys/cell.h>
#endif

/*
 * unique id given to each sthread. We don't worry about wrapping.
 * We start above the highest possible 32 bit pid so that we can use
 * this as a 'locking' id (to implement recursive locks).
 */
static uint64_t kid = 0x100000000ULL;	/* unique id generator */

extern int add_kthread_stack;

/*
 * init kthread stuff
 */
void
kthread_init(
	kthread_t *kt,
	caddr_t stack_addr,
	uint_t stack_size,
	int type,
	pri_t pri,
	uint_t flags,
	kt_ops_t *ktops)
{
	uint_t ssize;
	bzero(kt, sizeof(*kt));

	kt->k_type = type;

	/* leave this here until KTOP_GET_NAME works */
	switch (type) {
	case KT_UTHREAD:
	case KT_STHREAD:
	case KT_XTHREAD:
		break;
#ifdef DEBUG
	default:
		panic("unknown kthread type");
		break;
#endif
	}

	kt->k_sonproc = CPU_NONE;
	kt->k_onrq = -1;
	kt->k_preempt = 0;
	kt->k_pri = kt->k_basepri = kt_getcfg(kt, THREADCFG_PRIO, pri);
	kt->k_id = atomicAddUint64(&kid, 1ULL);
	kt->k_cpuset = 1;
	kt->k_prtn = 1;
	kt->k_copri = 0;
	/* init_bitlock isn't allowed to touch any except the lock bit */
	kt->k_flags = flags & (KT_PS|KT_BIND);
	init_bitlock(&kt->k_flags, KT_LOCK, "k_flag", (long)kt & 0xffff);
	if (kt->k_flags & KT_PS)
		kt->k_flags |= KT_BASEPS;
	if (KT_ISSTHREAD(kt) || KT_ISXTHREAD(kt))
		kt->k_flags |= KT_BIND;
	kt->k_flink = kt->k_blink = kt;
	sv_init(&kt->k_timewait, SV_DEFAULT, "ktwait");
	if (stack_addr == NULL) {
		kt->k_stacksize = 
		    kt_getcfg(kt,
			      THREADCFG_STACKSIZE,
			      stack_size ? stack_size : KTHREAD_DEF_STACKSZ);
		kt->k_flags |= KT_STACK_MALLOC;
		ssize = kt->k_stacksize + add_kthread_stack;
#if R4000 && MP
		ssize = ctob(btoc(ssize));
		ASSERT(ssize == ctob(1));
#ifdef PRHEAP
		/* give room for PRHEAP header */
		ssize -= 256;
#endif
#endif
		/*
		 * Kernel stack addresses cannot come from k2 space.
		 * At least not yet.
		 */
		kt->k_stack = (caddr_t)kmem_zalloc(ssize,
					KM_SLEEP|VM_DIRECT|VM_NO_DMA);
		ASSERT(kt->k_stack);

#if R4000
		/*
		 * we need to avoid VCEs on stacks - otherwise we'll
		 * trash registers in locore.
		 * For R4K MP, we have allocated a multiple of a page
		 * knowing that kmem_alloc will ALWAYS make that on
		 * a page boundary. This means we can use sync_dcaches_excl
		 * (and avoid broadcast interrupts)
		 * Note that for MP systems we are assuming:
		 * 1) at least 16K pages (since stacks can be only 1 page)
		 * 2) 64 bit mode since we assume k_stack is in KSEG0
		 */
#if MP
#ifndef PRHEAP
		ASSERT(poff(kt->k_stack) == 0);
#endif
		ASSERT(IS_KSEG0(kt->k_stack));
		sync_dcaches_excl(kt->k_stack, ssize,
			btoct(K0_TO_PHYS(kt->k_stack)),
			CACH_NOADDR|CACH_AVOID_VCES);
#else
		cache_operation(kt->k_stack, ssize,
			CACH_ICACHE|CACH_DCACHE|
			CACH_INVAL|CACH_WBACK|CACH_AVOID_VCES);
#endif /* MP */
#endif /* R4000 */
	} else {
		kt->k_stacksize = stack_size;
		kt->k_stack = stack_addr;
	}
	kt->k_regs[PCB_SP] = (k_machreg_t)PTR_EXT(kt->k_stack + kt->k_stacksize
								- ARGSAVEFRM);
	kt->k_regs[PCB_SP] &= ~0xf;	/* ensure doubleword alignment */
	kt->k_ops = ktops;
	kt->k_mustrun = kt_getcfg(kt, THREADCFG_MUSTRUN, PDA_RUNANYWHERE);
	kt->k_lastrun = CPU_NONE;
	kt->k_binding = CPU_NONE;
	kt->k_rblink = kt;
	kt->k_rflink = kt;
#ifdef CELL
	kt->k_bla.kb_lockpp = kt->k_bla.kb_lockp; /* init behavior lock pointer */
	SERVICE_MAKE_NULL(kt->k_remote_svc);
#endif
        kt->k_mldlink = 0;
	kt->k_affnode = CNODEID_NONE;
        CNODEMASK_CLRALL(kt->k_maffbv);
        kt->k_nodemask = CNODEMASK_BOOTED_MASK;
	kt->k_qkey = 0;
	ASSERT(CNODEMASK_IS_NONZERO(kt->k_nodemask));

	/* initialize mrlock inheritance structures */
	init_mria(&kt->k_mria, kt);
}

/*
 * release any resources EXCEPT the kthread struct itself
 */
void
kthread_destroy(kthread_t *kt)
{
        kthread_afflink_unlink(kt, 0);
	destroy_bitlock(&kt->k_flags);
	sv_destroy(&kt->k_timewait);
	destroy_mria(&kt->k_mria);
	if (kt->k_flags & KT_STACK_MALLOC)
		kmem_free(kt->k_stack, kt->k_stacksize);
#ifdef DEBUG
	kt->k_stack = (void *)0xefefeL;
#endif
}

#if CELL
#include "cell/kthread_cell.h"
#endif

#ifndef	CELL
/* ARGSUSED */
#endif
int
thread_interrupt(
	kthread_t	*kt,
	int		*s)
{

	kt->k_flags |= KT_INTERRUPTED;
	if (thread_interruptible(kt)) {
		setrun(kt);
		return(1);
	}
#if	CELL
	return(thread_remote_interrupt(kt, s));
#else
	return(0);
#endif
}

/* ARGSUSED */
int
kthread_syscall(sysarg_t cmd, sysarg_t arg1, rval_t *rvp)
{
	switch (cmd) {
	default:
		return (EINVAL);
	} /* cmd */
}

/*
 * return current thread's unique id
 * This is used for:
 * 1) marking owners of locks for recursive locks
 * 2) storing current owners of locks
 */
uint64_t
kthread_getid(void)
{
	return private.p_curkthread ? private.p_curkthread->k_id : (uint64_t)-1;
}

/*
 * static thread configuration
 *
 * no need to lock the list, it's statically
 * allocated and read only.
 *
 */
int
kt_getcfg(kthread_t *kt, int prop, int defval)
{
	char	*name = NULL;
	int	retval = defval;

	thread_cfg_t	*lp;

	switch (kt->k_type) {
	case KT_STHREAD:
	    name = KT_TO_ST(kt)->st_name;
	    break;
	case KT_XTHREAD:
	    name = KT_TO_XT(kt)->xt_name;
	    break;
	default:
	    cmn_err(CE_CONT,
			"kt_getcfg: doesn't know what this is %d\n",
			kt->k_type);
	case KT_UTHREAD:	
	    return defval;
	}

	switch (thread_cfg_rev) {
	case 1:
	    for (lp = thread_init; lp->name; lp++)
		if (strcmp(lp->name, name) == 0) {
		    switch (prop) {
		    case THREADCFG_PRIO:
			retval = lp->pri;
			break;
		    case THREADCFG_STACKSIZE:
			retval = lp->stack;
			break;
		    case THREADCFG_MUSTRUN:
			retval = lp->mustrun;
			break;
		    default:
			break;
		    }
		}
	    break;
	default:
	    break;
	}

	return retval;
}

#if !CELL

pid_t
kthread_get_pid()
{
	return(0);
}

#else /* CELL */

#include <ksys/cell/mesg.h>

pid_t
kthread_get_pid()
{
	trans_t	*tr;

	if ((tr = mesg_thread_info()) == NULL)
		return(0);
	else
		return(tr->tr_pid);
}

#endif /* CELL */
