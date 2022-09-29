/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.97 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/mips_addrspace.h>
#include <sys/proc.h>
#include <ksys/pid.h>
#include <sys/systm.h>
#include <ksys/vproc.h>
#include "pproc.h"
#include "vproc_private.h"
#include "pproc_private.h"
#ifdef _SHAREII
# include <sys/shareIIstubs.h>
#endif /* _SHAREII */
#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif

#ifdef DEBUG
#include <sys/prctl.h>
#define PRIVATE
#else
#define PRIVATE static
#endif

PRIVATE void pproc_noop(void){}

PRIVATE vproc_ops_t pproc_ops = {
	BHV_IDENTITY_INIT_POSITION(VPROC_BHV_PP),
	pproc_destroy,			/* VPROC_DESTROY */
	pproc_get_proc,			/* VPROC_GET_PROC */
	pproc_get_nice,			/* VPROC_GET_NICE */
	pproc_set_nice,			/* VPROC_SET_NICE */
	pproc_set_dir,			/* CURVPROC_SET_DIR */
	pproc_umask,			/* CURVPROC_UMASK */
	pproc_get_attr,			/* VPROC_GET_ATTR */
	pproc_parent_notify,		/* VPROC_PARENT_NOTIFY */
	pproc_reparent,			/* VPROC_REPARENT */
	pproc_reap,			/* VPROC_REAP */
	pproc_add_exit_callback,	/* VPROC_ADD_EXIT_CALLBACK */
	pproc_run_exitfuncs,		/* VPROC_RUN_EXITFUNCS */
	pproc_set_state,		/* VPROC_SET_STATE */
	pproc_thread_state,		/* VPROC_THREAD_STATE */
	pproc_sendsig,			/* VPROC_SENDSIG */
	pproc_sig_threads,		/* VPROC_SIG_THREADS */
	pproc_sig_thread,		/* CURVPROC_SIG_THREAD */
	pproc_prstop_threads,		/* VPROC_PRSTOP_THREADS */
	pproc_prwstop_threads,		/* VPROC_PRWSTOP_THREADS */
	pproc_prstart_threads,		/* VPROC_PRSTART_THREADS */
	pproc_setpgid,			/* VPROC_SETPGID */
	pproc_pgrp_linkage,		/* VPROC_PGRP_LINKAGE */
	pproc_setuid,			/* CURVPROC_SETUID */
	pproc_setgid,			/* CURVPROC_SETGID */
	pproc_setgroups,		/* CURVPROC_SETGROUPS */
	pproc_setsid,			/* CURVPROC_SETSID */
	pproc_setpgrp,			/* CURVPROC_SETPGRP */
	pproc_ctty_access,		/* VPROC_CTTY_ACCESS */
	pproc_ctty_clear,		/* VPROC_CTTY_CLEAR */
	pproc_ctty_hangup,		/* VPROC_CTTY_HANGUP */
	pproc_get_sigact,		/* CURVPROC_GET_SIGACT */
	pproc_set_sigact,		/* CURVPROC_SET_SIGACT */
	pproc_get_sigvec,		/* VPROC_GET_SIGVEC */
	pproc_put_sigvec,		/* VPROC_PUT_SIGVEC */
	pproc_trysig_thread,		/* VPROC_TRYSIG_THREAD */
	pproc_flag_threads,		/* VPROC_FLAG_THREADS */
	pproc_getpagg,			/* VPROC_GETPAGG */
	pproc_setpagg,			/* VPROC_SETPAGG */
	pproc_getkaiocbhd,		/* CURVPROC_GETKAIOCBHD */
	pproc_setkaiocbhd,		/* CURVPROC_SETKAIOCBHD */
	pproc_getrlimit,		/* CURVPROC_GETRLIMIT */
	pproc_setrlimit,		/* CURVPROC_SETRLIMIT */
	pproc_exec,			/* VPROC_EXEC */
	pproc_set_fpflags,		/* CURVPROC_SET_FPFLAGS */
	pproc_set_proxy,		/* CURVPROC_SET_PROXY */
	pproc_times,			/* CURVPROC_TIMES */
	pproc_read_us_timers,		/* VPROC_READ_US_TIMERS */	
	pproc_getrusage,
	pproc_get_extacct,
	pproc_get_acct_timers,
	pproc_sched_setparam,
	pproc_sched_getparam,
	pproc_sched_setscheduler,
	pproc_sched_getscheduler,
	pproc_sched_rr_get_interval,
	pproc_setinfo_runq,
	pproc_getrtpri,
	pproc_setmaster,
	pproc_schedmode,
	pproc_sched_aff,
        pproc_prnode,
	pproc_noop,			/* VPROC_EXITWAKE deleted */
	pproc_procblk,
	pproc_prctl,
	pproc_set_unblkonexecpid,
	pproc_unblkpid,
	pproc_unblock_thread,		/* CURVPROC_UNBLOCK_THREAD */
	pproc_fdt_dup,
#ifdef CKPT
	pproc_ckpt_shmget,		/* CURVPROC_CKPT_SHMGET */
	pproc_get_ckpt,			/* PROC_GET_CKPT */
#endif
	pproc_poll_wakeup_thread,
	pproc_getcomm,
	pproc_get_xattr,		/* VPROC_GET_XATTR */
	pproc_read_all_timers,		/* VPROC_READ_ALL_TIMERS */
	/* Add new entries before pproc_noop */
	pproc_noop,
};

zone_t *pproc_zone;		/* proc structures */

void
pproc_init(void)
{
	extern void pshare_init(void);

	pproc_zone = kmem_zone_init(sizeof(struct proc), "pproc");
	ASSERT(pproc_zone);
	pshare_init();
}

/*
 * pproc_create - allocate a new proc entry.
 */
proc_t *
pproc_create(struct vproc *vp)
{
	proc_t *p;

	p = kmem_zone_zalloc(pproc_zone, KM_SLEEP|VM_DIRECT);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&p->p_bhv, p, vp, &pproc_ops);
	bhv_insert_initial(VPROC_BHV_HEAD(vp), &p->p_bhv);

	return p;
}

/*
 * pproc_recreate - allocate a proc entry on target cell of migration.
 */
proc_t *
pproc_recreate(struct vproc *vp)
{
	proc_t *p;

	p = kmem_zone_zalloc(pproc_zone, KM_SLEEP|VM_DIRECT);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&p->p_bhv, p, vp, &pproc_ops);
	bhv_insert(VPROC_BHV_HEAD(vp), &p->p_bhv);

	return p;
}

/*
 * pproc_return --
 *
 * called only by procnew, to deal with early failures in procnew,
 * before state is initialized in the proc structure. Once the
 * structure is initialize, procfree must be used to free the
 * struct. This routine undoes the work done by procalloc.
 */
void
pproc_return(proc_t *p)
{
	struct vproc	*vp = BHV_TO_VPROC(&p->p_bhv);

	ASSERT(prxy_to_thread(&p->p_proxy) == NULL);

	bhv_remove(VPROC_BHV_HEAD(vp), &p->p_bhv);

	kmem_zone_free(pproc_zone, p);
}

/*
 * pproc_destroy --
 *
 * Called after a proc struct is fully initialized - this tears
 * down the locks, etc, before releasing the proc struct.
 */
void
pproc_destroy(bhv_desc_t *bhv)
{
	struct proc	*p;
	struct vproc	*vp;

	p = BHV_TO_PROC(bhv);
	vp = BHV_TO_VPROC(bhv);

	ASSERT(p->p_vpgrp == NULL);

	/* Track number of processes, for proc limit management */
	(void) atomicAddInt(&npalloc, -1);

#ifdef _SHAREII
	SHR_PROCDESTROY(p);
#endif /* _SHAREII */

	/* No p_cred if failure during procnew */
	if (p->p_cred) {
		uidact_decr(p->p_cred->cr_ruid);
		crfree(p->p_cred);
	}
	destroy_bitlock(&p->p_flag);
	spinlock_destroy(&p->p_itimerlock);
	mutex_destroy(&p->p_childlock);
	sv_destroy(&p->p_wait);
	mrfree(&p->p_who);
	mrfree(&p->p_credlock);

	/*
	 * XXX sigvec is initted in procdup but torn down here - ripe for
	 * a bug in various error paths.
	 * sigvec teardown
	 */
	mrfree(&p->p_sigvec.sv_lock);

	/* 'proxy' teardown */
	mrfree(&p->p_proxy.prxy_thrdlock);
	sv_destroy(&p->p_proxy.prxy_thrdwait);
	destroy_bitlock(&p->p_proxy.prxy_sched.prs_flags);
	mutex_destroy(&p->p_proxy.prxy_coredump_mutex);

	mutex_destroy(&p->p_proxy.prxy_semlock);

#ifdef R10000
	if (p->p_proxy.prxy_cpumon) {
		hwperf_cpu_monitor_info_free(p->p_proxy.prxy_cpumon);
		p->p_proxy.prxy_cpumon = NULL;
	}
#endif
#ifdef CKPT
	/* checkpoint teardon */
	mrfree(&p->p_ckptlck);
#endif
	bhv_remove(VPROC_BHV_HEAD(vp), &p->p_bhv);
#ifdef DEBUG
	bzero(p, sizeof(*p));
#endif
	kmem_zone_free(pproc_zone, p);
}

/* Set up the variety of locks, etc. associated with a proc
 * structure.
 */
void
pproc_struct_init(proc_t *p)
{
	int pid = (int)p->p_pid;

	init_spinlock(&p->p_itimerlock, "pri", pid);
	init_bitlock(&p->p_flag, STATELOCK, "pfl", pid);
	init_mutex(&p->p_childlock, MUTEX_DEFAULT, "prl", pid);
	init_sv(&p->p_wait, SV_DEFAULT, "procw", pid);
	mrlock_init(&p->p_who, MRLOCK_DEFAULT, "per", pid);
	mrlock_init(&p->p_credlock, MRLOCK_DEFAULT, "cred", pid);

	/* 'proxy' initialization */
	mrlock_init(&p->p_sigvec.sv_lock, MRLOCK_DEFAULT, "sigveclock", pid);
	mrlock_init(&p->p_proxy.prxy_thrdlock, MRLOCK_DEFAULT, "prxylk", pid);
	init_sv(&p->p_proxy.prxy_thrdwait, SV_DEFAULT, "prxywt", pid);
	init_bitlock(&p->p_proxy.prxy_sched.prs_flags, PRSLOCK, "prs", pid);
	mutex_init(&p->p_proxy.prxy_coredump_mutex, MUTEX_DEFAULT, 0);
#ifdef CKPT
	/* checkpoint initialization */
	mrinit(&p->p_ckptlck, "ckpt");
#endif
	init_mutex(&p->p_proxy.prxy_semlock, MUTEX_DEFAULT, "prxy_semlock", pid);
	p->p_proxy.prxy_semundo = NULL;
}

void
pproc_set_state(
	bhv_desc_t	*bhv,
	int		state)
{
	ASSERT(state == SINVAL || state == SZOMB || state == SRUN);

	vproc_set_state(BHV_TO_VPROC(bhv), state);

	BHV_TO_PROC(bhv)->p_stat = state;
}
