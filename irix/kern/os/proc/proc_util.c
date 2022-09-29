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

#ident "$Revision: 1.61 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include "pproc_private.h"
#include <sys/atomic_ops.h>
#if CELL_IRIX
#include <ksys/cell.h>
#endif
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <ksys/fdt.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/childlist.h>
#include <ksys/vpgrp.h>
#include <sys/vnode.h>
#include <sys/runq.h>
#include <ksys/vsession.h>
#include <sys/sched.h>
#include "pproc.h"

struct rlimit rlimits[RLIM_NLIMITS];
extern rlim_t  rlimit_cpu_cur;
extern rlim_t  rlimit_cpu_max;
extern rlim_t  rlimit_fsize_cur;
extern rlim_t  rlimit_fsize_max;
extern rlim_t  rlimit_data_max;
extern rlim_t  rlimit_stack_max;
extern rlim_t  rlimit_core_cur;
extern rlim_t  rlimit_core_max;
extern rlim_t  rlimit_vmem_max;
extern rlim_t  rlimit_rss_max;
extern rlim_t  rlimit_nofile_cur;
extern rlim_t  rlimit_nofile_max;
extern rlim_t  rlimit_pthread_cur;
extern rlim_t  rlimit_pthread_max;

/*
 * reparent_children - reparent process p's children to init. dispose of
 * zombies at this time.
 */
void
reparent_children(proc_t *p)
{
	vproc_t *vpr;
	child_pidlist_t *cpid;
	struct rusage rusage;
	int rtflags;

restart_scan:
	/* We don't hold p_childlock across VPROC ops, so have to reacquire
	 * and restart scan at beginning of list.
	 */
	mutex_lock(&p->p_childlock, PZERO);
	while (cpid = p->p_childpids) {

		/* remove entry from list before we do any VPROC ops - so
		 * we can drop p_childlock before the VPROC op.
		 */
		p->p_childpids = cpid->cp_next;

		vpr = VPROC_LOOKUP_STATE(cpid->cp_pid, ZYES);
		if (vpr == NULL) {
			/* Since exiting children look up their
			 * parent using ZNO, if the parent is exiting
			 * at the same time as the child, the child will
			 * not be able to notify the parent of its exit.
			 * Since, in this case, the child does not notify 
			 * the parent of its exit, there is a child list
			 * entry for a child that has already exited - in
			 * which case, the VPROC_LOOKUP will return null.
			 */
			kmem_zone_free(child_pid_zone, cpid);
			continue;
		}

		mutex_unlock(&p->p_childlock);

		switch (cpid->cp_wcode) {
		case CLD_EXITED:
		case CLD_DUMPED:
		case CLD_KILLED:
			/* This child has notified the parent (p) of
			 * its exit - it is therefore a zombie (or in
			 * the process of becoming one.) Since it notified
			 * the parent of its exit, is expects the parent
			 * to free its process structure and pid.
			 */

			VPROC_REAP(vpr, VREAP_IGNORE, &rusage, NULL, &rtflags);

			/* No VPROC_RELE - VPROC_REAP has removed
			 * the proc and vproc
			 */
			kmem_zone_free(child_pid_zone, cpid);
			goto restart_scan;

		default:
			VPROC_REPARENT(vpr, p->p_pgid); 
			VPROC_RELE(vpr);

			kmem_zone_free(child_pid_zone, cpid);
			goto restart_scan;
		}
	}

	mutex_unlock(&p->p_childlock);
}

/*
 * freechildren - free any unwaited for children when a process
 * ignores SIGCLD.
 */
void
freechildren(proc_t *p)
{
	child_pidlist_t **list;
	child_pidlist_t *cpid;
	vproc_t *vpr;
	struct rusage rusage;
	int rtflags;

restart_scan:
	/* We don't hold p_childlock across VPROC ops, so have to reacquire
	 * and restart scan at beginning of list.
	 */
	mutex_lock(&p->p_childlock, PZERO);
	list = &p->p_childpids;

	while (cpid = *list) {
		switch (cpid->cp_wcode) {
		default:
			list = &cpid->cp_next;
			continue;

		case CLD_EXITED:
		case CLD_DUMPED:
		case CLD_KILLED: {
			/* Delete this entry from child chain */
			*list = cpid->cp_next;

			vpr = VPROC_LOOKUP_STATE(cpid->cp_pid, ZYES);
			ASSERT(vpr != NULL);

			/* Free the list lock for the VPROC call */
			mutex_unlock(&p->p_childlock);

			VPROC_REAP(vpr, VREAP_IGNORE, &rusage, NULL, &rtflags);
			/* VPROC_REAP deletes the server-side vproc, so
			 * no VPROC_RELE.
			 */

			/* free this childlist structure. */
			kmem_zone_free(child_pid_zone, cpid);
			goto restart_scan;
		    }
		}
	}

	mutex_unlock(&p->p_childlock);
}

/*
 * Is srchpid an ancestor of the process indicated by curpid?
 *
 *	Used by killall to avoid blowing away parent wsh of a csh, since
 *	process groups no longer do the trick with job control.
 *
 *	Nothing is locked here, since the answer is only a snapshot
 *	anyway.
 */
int
ancestor(pid_t srchpid, pid_t curpid)
{
	pid_t tpid;
	vproc_t *vpr;
	vp_get_attr_t attr;

	/* Don't kill init */
	if (srchpid == INIT_PID)
		return 1;

	/* If its equal to our parents pid, don't signal */
	if (srchpid == curpid)
		return 1;

	tpid = curpid;

	for (;;) {
		if (tpid == srchpid)
			return 1;
		if (tpid == INIT_PID || tpid == 0)
			return 0;
		if ((vpr = VPROC_LOOKUP(tpid)) == NULL)
			return 0;
		VPROC_GET_ATTR(vpr, VGATTR_PPID, &attr);
		VPROC_RELE(vpr);
		tpid = attr.va_ppid;
	}

	/* NOTREACHED */
}

/*
 * scan process p's child list to see if
 * some child has died.
 */
int
anydead(proc_t *p, k_siginfo_t *sip)
{
	int dead = 0;
	child_pidlist_t *cpid;

	mutex_lock(&p->p_childlock, PZERO);

	for (cpid = p->p_childpids; cpid != NULL; cpid = cpid->cp_next) {
		switch(cpid->cp_wcode) {
		default:
			continue;

		case CLD_EXITED:
		case CLD_DUMPED:
		case CLD_KILLED:
			if (sip) {
				sip->si_code = cpid->cp_wcode;
				sip->si_status = cpid->cp_wdata;
				sip->si_pid = cpid->cp_pid;
				sip->si_signo = SIGCLD;
				sip->si_utime = hzto(&cpid->cp_utime);
				sip->si_stime = hzto(&cpid->cp_stime);
			}
			dead = 1;
			break;
		}
	}

	mutex_unlock(&p->p_childlock);

	return dead;
}

/* Ptrace grot - find pid in proc p's child list, return an
 * vproc pointer to that proc.
 */
vproc_t *
ptrsrch(proc_t *p, pid_t pid)
{
	child_pidlist_t *cpid;
	vproc_t *vpr = NULL;

	mutex_lock(&p->p_childlock, PZERO);

	for (cpid = p->p_childpids; cpid != NULL; cpid = cpid->cp_next) {
		if (cpid->cp_pid == pid) {
			vpr = VPROC_LOOKUP_STATE(pid, ZYES);
			break;
		}
	}

	mutex_unlock(&p->p_childlock);

	return vpr;
}
#ifdef CKPT
/*
 * Find pid in proc p's child list, return exit status
 */
int
getxstat(proc_t *p, pid_t pid, short *xstat)
{
	child_pidlist_t *cpid;

	mutex_lock(&p->p_childlock, PZERO);

	for (cpid = p->p_childpids; cpid != NULL; cpid = cpid->cp_next) {
		if (cpid->cp_pid == pid) {
			*xstat = cpid->cp_xstat;
			mutex_unlock(&p->p_childlock);
			return (0);
		}
	}
	mutex_unlock(&p->p_childlock);
	return -1;
}
#endif

extern void as_p0exit(void);
extern void as_p0init(proc_t *);
extern void p0_launch(void);

/*
 * Set up proc0 - this is just used to have enough context to spawn
 * proc1.
 */
void
p0init(void)
{
	vproc_t		*vp;
	vpgrp_t		*vpg;
	vsession_t	*vsp;
	pid_t		newpid;
	struct proc	*p;
	uthread_t	*ut;
	kthread_t	*kt;
        int             s;
	extern int	slice_size;
	extern int	trigger_user_nonactive_timers;

	vp = vproc_create();
	ASSERT(vp);
	p = pproc_create(vp);
	pproc_struct_init(p);

	(void) uthread_create(p, 0, &ut, UT_ID_NULL);
	ASSERT(ut);
	/*
	 * uthread_create figures that caller will initialize exception struct
	 */
	bzero(ut->ut_exception, sizeof(pcb_t) + sizeof(eframe_t));

	/* NOTE: p is all zeros now except for uthread pointer */
	kt = UT_TO_KT(ut);
	ktimer_init(kt, AS_SYS_RUN);

	pid_alloc(0, &newpid);
#if defined(CELL_IRIX)
	if (cellid() == golden_cell)	/* Only the golden cell starts pid #0 */
#endif
	ASSERT(newpid == 0);
	p->p_pid = newpid;
	vp->vp_pid = newpid;
	pid_associate(newpid, vp);

	/*
	 * Max rlimits are held in proc, cur rlimits are kept
	 * in appropirate object
	 */
	rlimits[RLIMIT_CPU].rlim_max = rlimit_cpu_max;
	rlimits[RLIMIT_FSIZE].rlim_max = rlimit_fsize_max;
	rlimits[RLIMIT_DATA].rlim_max = rlimit_data_max;
	rlimits[RLIMIT_STACK].rlim_max = rlimit_stack_max;
	rlimits[RLIMIT_CORE].rlim_max = rlimit_core_max;
	rlimits[RLIMIT_NOFILE].rlim_max = rlimit_nofile_max;
	rlimits[RLIMIT_VMEM].rlim_max = rlimit_vmem_max;
	rlimits[RLIMIT_RSS].rlim_max = rlimit_rss_max;
	rlimits[RLIMIT_PTHREAD].rlim_max = rlimit_pthread_max;

	rlimits[RLIMIT_CPU].rlim_cur = rlimit_cpu_cur;
	rlimits[RLIMIT_FSIZE].rlim_cur = rlimit_fsize_cur;
	rlimits[RLIMIT_CORE].rlim_cur = rlimit_core_cur;
	rlimits[RLIMIT_NOFILE].rlim_cur = rlimit_nofile_cur;
	rlimits[RLIMIT_PTHREAD].rlim_cur = rlimit_pthread_cur;
	bcopy(rlimits, p->p_rlimit, sizeof(p->p_rlimit));

	/*
	 * make us a high non-degrading priority process so we
	 * don't get preempted by normal procs. Hi RT guys are permitted
	 * to run before us. Also, mark to be run under new scheduler.
	 */
	kt_initialize_pri(kt, 255);
	kt->k_flags |= KT_PS;
	kt->k_cpuset = 1;
	/*
	 * ALL p fields start at 0
	 */
	p->p_stat = SRUN;
	p->p_flag = SIGNORED;
	p->p_proxy.prxy_sched.prs_nice = NZERO;
	p->p_start = time;
	ut->ut_policy = SCHED_TS;
	p->p_vpagg = &vpagg0;   /* later init'ed by arsess_init */

	/* create new session/pgrp */
	vsp = VSESSION_CREATE(p->p_pid);
	ASSERT(vsp);

	vpg = VPGRP_CREATE(p->p_pid, vsp->vs_sid);
	ASSERT(vpg);

	VPGRP_JOIN(vpg, p, 0);
	VPGRP_RELE(vpg);		/* from create */
	p->p_vpgrp = vpg;
	p->p_pgid = p->p_pid;

	p->p_sid = vsp->vs_sid;
	VSESSION_RELE(vsp);

	/*
	 * Set the credential structure for this process to
	 * the system credential structure.
	 * Increment sys_cred reference once for p_cred, and once for ut_cred.
	 */
	crhold(sys_cred), crhold(sys_cred);
	ut->ut_cred = p->p_cred = sys_cred;

	/*
	 * Initialize additional security attributes in the system
	 * credentials. In the case where MAC is on and CAP is off
	 * init does NOT run with sys_cred.
	 */
	cap_empower_cred(sys_cred);
	_MAC_INIT_CRED();

	/* allocate fdt */
	p->p_fdt = fdt0_init();

	/*
	 * XXX	All this uthread initialization crud should be pushed into
	 * XXX	a routine that can be used by cross-cell migration code, too.
	 */
	/* set up cur directory */
	ut->ut_cdir = rootdir;
	VN_HOLD(ut->ut_cdir);
	ut->ut_rdir = NULL;

	/*
	 * The tunable scheduler will have it's own ideas about what
	 * makes a reasonable time slice.  For now, initialize everybody's
	 * to the configured value.
	 */
	ut->ut_tslice = slice_size;
	ut->ut_cmask = CMASK;
	ASSERT(ut->ut_as.utas_tlbid);
	setup_wired_tlb_notme(&ut->ut_as, 1);

	as_p0init(p);

#ifdef _SHAREII
	ut->ut_shareT = 0;
	p->p_shareP = 0;
#endif /*_SHAREII*/

	/* 
         * start at p0_launch() - stack set up in kthread_init 
         * Make sure the spl in PCB_SR is hi to avoid pre-emption interrupt
	 * while p_switching is one.  p0_lauch sets does an spl0 after
	 * resetting p_switching.
         */
        s = splhi();
	kt->k_regs[PCB_PC] = (k_smachreg_t)&p0_launch;
	kt->k_regs[PCB_SR] = getsr();
        splx(s);
	s = ut_lock(ut);
	putrunq(kt, CPU_NONE);
	ut_unlock(ut, s);

	return;
}

void
p0exit(void)
{
	proc_t *p = UT_TO_PROC(KT_TO_UT(curthreadp));

	PID_PROC_FREE(p->p_pid);

	mrlock(&p->p_who, MR_UPDATE, PZERO);
	ASSERT(p->p_vpgrp);
	VPGRP_HOLD(p->p_vpgrp);
	VPGRP_LEAVE(p->p_vpgrp, p, 1);
	VPGRP_RELE(p->p_vpgrp);
	p->p_vpgrp = NULL;
	p->p_pgid = -1;
	p->p_sid = -1;

	mrunlock(&p->p_who);
	as_p0exit();

	p_lock(p);
	p->p_stat = SINVAL;
	p_nested_unlock(p);

	VPROC_HOLD_STATE(PROC_TO_VPROC(p), ZYES);	/* for vproc_destroy */

	ut0exitswtch();
}
