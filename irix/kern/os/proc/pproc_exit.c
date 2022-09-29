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

#ident "$Revision: 1.74 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/acct.h>
#include <ksys/vpag.h>
#include <ksys/cell.h>
#include <sys/time.h>
#include <ksys/childlist.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <ksys/fdt.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/imon.h>
#include <sys/ksignal.h>
#include <sys/space.h>
#include "ksys/vpgrp.h"
#include "ksys/vsession.h"
#include <procfs/prsystm.h>
#include <sys/proc.h>
#include "pproc.h"
#include <ksys/vproc.h>
#include "pproc_private.h"
#include "proc_trace.h"
#include "vproc_private.h"
#include <sys/rtmon.h>
#include <sys/sat.h>
#include <sys/space.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/numa.h>
#ifdef CKPT
#include <sys/ckpt.h>
#endif
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /* _SHAREII */

extern int do_procacct, do_extpacct, do_sessacct;

/*
 * Notify parent that process 'pid' has exited with status wcode and
 * return value wdata.
 * If parent is ignoring sigcld, set child's SIGNORED flag bit.
 */
/* ARGSUSED */
int
pproc_parent_notify(
	bhv_desc_t *bhv,
	pid_t pid,
	int wcode,
	int wdata,
	struct timeval utime,
	struct timeval stime,
	pid_t pgid,
	sequence_num_t pgsigseq,
	short xstat)
{
	proc_t *p;
	child_pidlist_t **list;
	child_pidlist_t *cpid;
	k_siginfo_t si, *sip;
	int rv, dosigcld = 0;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	mutex_lock(&p->p_childlock, PZERO);

	/* Scan child list looking for pid */
	list = &p->p_childpids;
	while (*list && (*list)->cp_pid != pid)
		list = &((*list)->cp_next);

	ASSERT(*list);
	if (*list == NULL) {
		mutex_unlock(&p->p_childlock);
		return 0;
	}

	ASSERT((*list)->cp_pid == pid);
	cpid = *list;

	/* Delete entry from list. If not freeing entry, put it at the
	 * head of the p_childpids list. This gives us FIFO ordering
	 * for wait/waitpid if multiple children have terminated.
	 * Although this behavior is not required by an standard, it
	 * benefits SPEC95 runs.
	 */
	*list = cpid->cp_next;

	if ((p->p_sigvec.sv_flags & SNOWAIT) &&
	    (wcode == CLD_EXITED ||
	     wcode == CLD_KILLED ||
	     wcode == CLD_DUMPED)) {
		kmem_zone_free(child_pid_zone, cpid);
		rv = SNOWAIT;
	} else {
		cpid->cp_wcode = wcode;
		cpid->cp_wdata = wdata;
		cpid->cp_utime = utime;
		cpid->cp_stime = stime;
		cpid->cp_pgid = pgid;
		cpid->cp_pgsigseq = pgsigseq;
		cpid->cp_xstat = xstat;

		/* Relink entry to tail of p_childpids */
		while (*list)
			list = &((*list)->cp_next);
		cpid->cp_next = NULL;
		*list = cpid;
		rv = 0;
	}

	/* Signal parent if:
	 *	wcode is one of: CLD_EXITED, CLD_KILLED or CLD_DUMPED.
	 *	wcode is CLD_STOPPED and the parent wants to see
	 *	SIGCLD for stopping children.
	 *
	 * By default signal/sigset turn on the POSIX SNOCLDSTOP bit,
	 * thus preventing normal sysV parents from getting unwanted SIGCLDs.
	 * To provide for parents that do not catch SIGCLD
	 * but are expecting to be woken out of a wait -- we
	 * also kick a waiting parent
	 */
	switch (wcode) {
	case CLD_CONTINUED:
		mutex_unlock(&p->p_childlock);
		return rv;

	case CLD_TRAPPED:
		break;

	case CLD_STOPPED:
		if (p->p_sigvec.sv_flags & SNOCLDSTOP)
			break;
		/* FALLTHROUGH */
	default:
		if (sigismember(&p->p_sigvec.sv_sainfo, SIGCLD)) {
			sip = &si;
			bzero((caddr_t)sip, sizeof(si));
			si.si_signo = SIGCLD;
			si.si_code = wcode;
			si.si_pid = pid;
			si.si_utime = hzto(&utime);
			si.si_stime = hzto(&stime);
			si.si_status = wdata;
		} else
			sip = NULL;

		dosigcld = 1;
	}

	/* Wake up the parent and notify as to child state change */
	p_flagclr(p, SWSRCH);
	sv_broadcast(&p->p_wait);
	if (dosigcld)
		sigtoproc(p, SIGCLD, sip);
	mutex_unlock(&p->p_childlock);
	return rv;
}

/*
 * detach that child's process group if the process group will become
 * orphaned without its parent.
 */
void
detach_child(
	proc_t		*cp)
{
	/*
	 * This prevents cp from changing its process group.
	 */
	mrlock(&cp->p_who, MR_ACCESS, PZERO);

	if (cp->p_vpgrp)
		VPGRP_DETACH(cp->p_vpgrp, cp);

	mrunlock(&cp->p_who);
}

void
pproc_reparent(
	bhv_desc_t	*bhv,
	int		to_detach)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/*
	 * Detach the child's process group if the process
	 * group will become orphaned without its parent
	 */
	if (to_detach)
		detach_child(p);

	/* Change parent to init - although this doesn't really
	 * matter - since init is inheriting this process, this
	 * process will never send SIGCHLD to init.
	 *
	 * Locking for p_ppid and the SIGNORED bit: These fields
	 * are only modified by the parent of a process - and only
	 * after the parent process has begun to exit. So these fields
	 * can be read unambiguosly by first taking a reference
	 * VPROC_LOOKUP(ppid, ZNO) on the parent. While the reference
	 * is held the fields cannot change.
	 * If the reference fails on the parent, then one assumes that
	 * p_ppid will be set to 1, and the SIGNORED bit will be set.
	 */

	p_flagset(p, SIGNORED);

	/* Set the SIGNORED bit before setting p_ppid to INITPID -
	 * This closes a small window where a child could
	 * do a VPROC_LOOKUP on ppid, and get a reference in init, then
	 * check SIGNORED, and find it clear - the child would then think
	 * it was not an 'adopted' process, but in reality it was. Doing
	 * things in this order elimiates that problem.
	 */
	p->p_ppid = 1;

	/* check if child is marked to die when parent does */
	if (p->p_flag & SCEXIT)
		sigtoproc(p, SIGHUP, (k_siginfo_t *)NULL);

	if (p->p_flag & STRC) {
		/*
		 * Since parent is exiting, there's no one left to
		 * to ptrace this process (init doesn't use ptrace
		 * so the child will hang forever).  Clearing
		 * the STRC bit causes the SIGKILL to actually
		 * take effect in the child, otherwise it will
		 * just keep calling stop().
		 */
		p_flagclr(p, STRC);
		sigtoproc(p, SIGKILL, (k_siginfo_t *)NULL);
	}
}

/*
 * Wait on zombie children. Free their proc structs and associated resources.
 */
/* ARGSUSED */
void
pproc_reap(
	bhv_desc_t *bhv,
	int flags,
	struct rusage *rusage,
	cpu_mon_t *hw_events,
	int *rtflags)
{
	proc_t		*p;
	vproc_t		*vp;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	/*
	 * Unlock the behavior chain lock. This is to allow someone to
	 * interpose. This is OK as no-one is allowed to reference any
	 * data structures when we return.
	 */
	vp = BHV_TO_VPROC(bhv);
	BHV_READ_UNLOCK(VPROC_BHV_HEAD(vp));

	/* Remove the process from the pid table - process will no
	 * longer be found by VPROC_LOOKUP.
	 */
	vproc_lookup_prevent(vp);

	/*
	 * If we are being freed up by a parent either waiting
	 * or ignoring SIGCLD, make sure that programs like ps do not
	 * hold any reference to us.
	 */
	ASSERT(p->p_stat == SZOMB);

	VPROC_SET_STATE(vp, 0);

	prexit(p);

	if (!(flags & VREAP_IGNORE)) {
		bcopy(&p->p_proxy.prxy_ru, rusage, sizeof(struct rusage));
		*rtflags = VWC_RUSAGE;
#ifdef R10000
		/*
		 * hw_events points to parent's cpumon (target).
		 */
		if (hw_events && p->p_proxy.prxy_cpumon) {
			ADD_HWPERF_COUNTERS(p->p_proxy.prxy_cpumon, hw_events);
		}
#endif
	}

#ifdef PRCLEAR
	/*
	 * please note that we should wait here for the prnode refcnt
	 * to drop to 0
	 */
	/*
	 * remove prnodes from process
	 */
	s = p_lock(p);
	prclear(p);
	p_unlock(p, s);
#endif

	/* Delete vpgrp and vsession structures */
	mrlock(&p->p_who, MR_UPDATE, PZERO);

	ASSERT(p->p_vpgrp);
	VPGRP_HOLD(p->p_vpgrp);
	VPGRP_LEAVE(p->p_vpgrp, p, 1);
	VPGRP_RELE(p->p_vpgrp);
	p->p_vpgrp = NULL;
	p->p_pgid = -1;
	p->p_sid = -1;

	mrunlock(&p->p_who);

	vproc_destroy(vp);
}

extern zone_t *ecb_zone;

/* register a callback function to be called when the specified
 * process exits.
 */
int
pproc_add_exit_callback(
	bhv_desc_t *bhv,
	int flags,
	void (* func)(void *),
	void *arg)
{
	proc_t *p;
	struct exit_callback *ecb;	/* newly allocated exit_callback */
	struct exit_callback *tecb;	/* loop variable */
	struct exit_callback *pecb;	/* previous loop variable */

	p = BHV_TO_PROC(bhv);

	if (flags & ADDEXIT_REMOVE) {
		mrupdate(&p->p_who);
		for (pecb = 0, tecb = p->p_ecblist;
		     tecb;
		     pecb = tecb, tecb = tecb->ecb_next) {
			if (tecb->ecb_func == func && tecb->ecb_arg == arg) {
				if (pecb) {
					pecb->ecb_next = tecb->ecb_next;
				} else {
					p->p_ecblist = tecb->ecb_next;
				}
				mrunlock(&p->p_who);
				kmem_zone_free(ecb_zone, tecb);
				return 0;
			}
		}
		mrunlock(&p->p_who);
		return ENOENT;
	}

	ecb = kmem_zone_alloc(ecb_zone, KM_SLEEP);

	/*
	 * We need to lock the list when traversing, because the
	 * process can have more than one thread.
	 */
	mrupdate(&p->p_who);
	if (flags & ADDEXIT_CHECK_DUPS)
		for (tecb = p->p_ecblist; tecb; tecb = tecb->ecb_next)
			if (tecb->ecb_func == func) {
				mrunlock(&p->p_who);
				kmem_zone_free(ecb_zone, ecb);
				return EALREADY;
			}

	ecb->ecb_func = func;
	ecb->ecb_arg = arg;
	ecb->ecb_next = p->p_ecblist;
	p->p_ecblist = ecb;

	mrunlock(&p->p_who);

	PROC_TRACE6(PROC_TRACE_REFERENCE, "pproc_add_exit_callback", p->p_pid,
			 "func", func, "arg", arg);

	return 0;
}

/* execute all of the callbacks registered for curproc */
void
pproc_run_exitfuncs(bhv_desc_t *bhv)
{
	struct exit_callback *ecb;
	proc_t *p;

	p = BHV_TO_PROC(bhv);

	ASSERT(p->p_stat == SZOMB);

	/* We don't need to protect the exitfunc list with any locking -
	 * since this process is exiting, no other process can modify
	 * the list.
	 */
	ecb = p->p_ecblist;
	while (ecb = p->p_ecblist) {
		p->p_ecblist = ecb->ecb_next;

		PROC_TRACE6(PROC_TRACE_REFERENCE, "pproc_run_exitfuncs",
				p->p_pid, "func", ecb->ecb_func,
				"arg", ecb->ecb_arg);

		ecb->ecb_func(ecb->ecb_arg);
		kmem_zone_free(ecb_zone, ecb);
	}
}

/* convert code/data pair into old style wait status */
static int
wstat(int code, int data)
{
	int stat = (data & 0377);

	switch (code) {
	case CLD_EXITED:
		stat <<= 8;
		break;
	case CLD_DUMPED:
		stat |= WCOREFLAG;
		break;
	case CLD_KILLED:
		break;
	case CLD_TRAPPED:
	case CLD_STOPPED:
		stat <<= 8;
		stat |= WSTOPFLG;
		break;
	case CLD_CONTINUED:
		stat = WCONTFLG;
		break;
	}
	return stat;
}

void
pproc_exit(
	uthread_t *ut,
	int why,
	int what,
	int *ut_exitflags)
{
	proc_t *p;
	int i;
	int s;
	int xstat;
	struct vnode *scriptvp;
	vproc_t *vpr;
	vproc_t *pvpr;
	int retnflags = 0;

	ASSERT(ut == curuthread);
	p = UT_TO_PROC(ut);
	vpr = PROC_TO_VPROC(p);
	/*
	 * Discard the realtimer timeout, if any.
	 */
	s = mutex_spinlock(&p->p_itimerlock);
	untimeout(p->p_itimer_toid);
	p->p_itimer_toid = 0;
	timerclear(&p->p_realtimer.it_value);
	timerclear(&p->p_realtimer.it_interval);
	mutex_spinunlock(&p->p_itimerlock, s);

	if (p->p_flag & SPROFFAST)
		stopprfclk();
	p_flagclr(p, SPROFFAST|STRC|SPROF|SPROF32);

	/* Trusted IRIX */
	_SAT_EXIT(what, 0);		/* audit record */

	if (p->p_trace)
		prpollwakeup(p->p_trace,
				POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLRDBAND);

	/*
	 * Discard sigqueue infos if any left -- this could happens if a
	 * process masks these signals, then exits.
	 * There's no reason to fiddle with any signal bits embedded in the
	 * proc and/or uthread structure, as these structures get tossed.
	 */
	sigqfree(&p->p_sigvec.sv_sigqueue);

	if (p->p_profp) {
		(void) kern_free(p->p_profp);
		p->p_profp = NULL;
		p->p_profn = 0;
	}

	/*
	 * Generate process accounting records before calling freectty().
	 */
	xstat = wstat(why, what);
	if (do_procacct)
		acct(xstat);
	if (do_extpacct || do_sessacct)
		extacct(p);
	if (p->p_shacct != NULL) {	/* Done with shadow acct data */
		(void) kern_free(p->p_shacct);
		p->p_shacct = NULL;
	}

	/* It is now safe to leave the process aggregate */
	if (p->p_vpagg) {
		VPAG_LEAVE(p->p_vpagg, p->p_pid);
		p->p_vpagg = 0;
	}

	/*
	 * If the process is a session leader, notify the session
	 * that it is exiting.
	 */
	if (p->p_sid == p->p_pid) {	/* if session leader */
		vsession_t	*vsp;

		vsp = VSESSION_LOOKUP(p->p_sid);
		ASSERT(vsp);
		VSESSION_CTTY_DEALLOC(vsp, SESSTTY_HANGUP);
		VSESSION_RELE(vsp);
	}

	/*
	 * Close all open files.
	 * This only closes private fd's (which for processes sharing
	 * fd's should be 0. Shared fd's are cleaned up in detachshaddr
	 */
	fdt_exit();
#ifdef CKPT
	ckpt_exit(p);
#endif
#ifdef _SHAREII
	/*
	 * Do exit-time processing
	 */
	 SHR_PCNTDN(ut);
	 SHR_EXIT(UT_TO_PROC(ut));
#endif /*_SHAREII*/
				   
	/* if unblock on exec/exit flag is set, do that now */
	if (p->p_unblkonexecpid) {
		vproc_t *unblkvp = VPROC_LOOKUP(p->p_unblkonexecpid);

		if (unblkvp != NULL) {
			VPROC_UNBLKPID(unblkvp);
			VPROC_RELE(unblkvp);
		}
		p->p_unblkonexecpid = 0;
	}

 	/*
	 * insert calls to "exitfunc" functions.
	 */

	/* call any exit functions registered for this proc */
	VPROC_RUN_EXITFUNCS(vpr);

	/*
	 * Delete pshare structure if process was multi-threaded.
	 */
	if (p->p_proxy.prxy_utshare)
		detachpshare(&p->p_proxy);

	/*
	 * Must VN_RELE before uthread_exit -- need ut-to-proc for creds.
	 */
	if (p->p_script) {
		scriptvp = p->p_script;

		s = VN_LOCK(scriptvp);
		i = --scriptvp->v_intpcount;
		VN_UNLOCK(scriptvp, s);
		if (!i)
			IMON_EVENT(scriptvp, ut->ut_cred, IMON_EXIT);
		VN_RELE(scriptvp);
		p->p_script = NULL;
	}

	/* 
	 * release original exec vnode - accessers must VPROC_LOOKUP
	 * the process to prevent exit.
	 */
	if (p->p_exec) {
		mrupdate(&p->p_who);
		VN_RELE(p->p_exec);
		p->p_exec = NULL;
		mrunlock(&p->p_who);
	}

	/* Give all children to proc 1. */
	reparent_children(p);

	/*
	 * If the parent of this child is ignoring SIGCLD, then this
	 * child will free its own proc struct. Otherwise it will hang
	 * around as a zombie, for the parent to wait on.
	 *
	 * If the parent has already started to exit, this child will
	 * similarly delete its own proc struct.  If parent has exited,
	 * it will have set SIGNORED, but only after it has become a
	 * zombie (in reparent_children, directly above).
	 *
	 * We don't send signals to init to wait on processes - if
	 * we find we have been inherited by init, this process frees
	 * its own proc struct.
	 *
	 * Note in particular that SIGNORED is only set in the
	 * child by an exiting parent.
	 */

	if (!(p->p_flag & SIGNORED)) {
		if (pvpr = VPROC_LOOKUP(p->p_ppid)) {
			/*
			 * VPROC_PARENT_NOTIFY returns non-zero if the
			 * parent is ignoring SIGCLD
			 */
			if (!(p->p_flag & SIGNORED)) {
				VPROC_PARENT_NOTIFY(pvpr, p->p_pid, why, what,
					  p->p_cru.ru_utime, p->p_cru.ru_stime,
					  p->p_pgid, 0, xstat, s);
			}
			VPROC_RELE(pvpr);
			if (s)
				goto ignored;
		} else {
			/*
			 * Parent is exiting, thus our parent will become
			 * init, so just pretend that that has already
			 * happened.
			 */
ignored:
			p_flagset(p, SIGNORED);
		}
	}

	if (!(p->p_flag & SIGNORED)) {
		/* Collect rusage stats
		 * Note that rusage stats for any distributed
		 * proxy should have been collected already
		 */
		ruadd(&p->p_proxy.prxy_ru, &p->p_cru);
	} else {
		/* Process will no longer be found by VPROC_LOOKUP */
		vproc_lookup_prevent(vpr);

		/*
		 * Remove the pgroup and session structures
		 * after the process is off the active chain and
		 * we are sure no other process will try to look
		 * at these structures while signalling.
		 */
		mrupdate(&p->p_who);
		ASSERT(p->p_vpgrp);
		VPGRP_HOLD(p->p_vpgrp);
		VPGRP_LEAVE(p->p_vpgrp, p, 1);
		VPGRP_RELE(p->p_vpgrp);
		p->p_vpgrp = NULL;
		p->p_pgid = -1;
		p->p_sid = -1;

		mrunlock(&p->p_who);

		/*
		 * Hang around until current references drop. This process
		 * has been removed from the active process list and
		 * the pid table, so no new references can occur.
		 */
		VPROC_SET_STATE(vpr, SINVAL);

		prexit(p);
		retnflags |= UT_VPROC_DESTROY;
	}

	/*
	 * Notify rtmond that we're exiting.  This actually generates more
	 * events than we typically want since this will cause events to be
	 * generated for every process in the system when we often are just
	 * tracing a small handful of processes.  We may need to think about
	 * ways of trying to avoid unneeded events ...
	 */
	LOG_TSTAMP_EVENT(RTMON_TASK|RTMON_TASKPROC, TSTAMP_EV_EXIT,
			 p->p_pid, NULL, NULL,NULL);

	if (IS_SPROC(&p->p_proxy)) {
		/*
		 * If the process is exiting 'normally' and SABORTSIG is
		 * indicated, don't send p_exitsig.
		 */
		if (p->p_flag & SABORTSIG && why == CLD_EXITED)
			p->p_exitsig = 0;
		if (detachshaddr(p, SHDEXIT | EXIT_CODE(why)))
			retnflags |= UT_DETACHSTK;
	} else
		retnflags |= UT_DETACHSTK;

	*ut_exitflags = retnflags;
}

