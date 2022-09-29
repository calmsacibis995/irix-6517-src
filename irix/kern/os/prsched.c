/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.133 $"

/*
 * Tunnable scheduler module.  Herein lies the code to implement
 * process priority maintainence.
 */
#include "sys/types.h"
#include "ksys/as.h"
#include "sys/systm.h"
#include "sys/sema.h"
#include "sys/proc.h"
#include "sys/pda.h"
#include "sys/par.h"
#include "sys/errno.h"
#include "ksys/cred.h"
#include "sys/debug.h"
#include "sys/schedctl.h"
#include "sys/param.h"
#include "limits.h"
#include "sys/buf.h"
#include "sys/prctl.h"
#include "sys/time.h"
#include "sys/runq.h"
#include "sys/sat.h"
#include "sys/capability.h"
#include "sys/ktime.h"
#include "sys/space.h"
#include "ksys/vproc.h"
#include "ksys/vpgrp.h"
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "sys/cmn_err.h"

/*
 * Frame Scheduler
 */
#include <sys/frs.h>

/*
 * Structure used for general communication with the process list
 * scanner.
 */
#define	TSSCAN_RENICE_USER	5
#define	TSSCAN_GTNICE_USER	8

#define	tonice(x)	((x) + NZERO)
#define	fromnice(x)	((x) - NZERO)

typedef struct {
	int		cmd;	/* command to perform */
	int		pid;	/* process looking for */
	int		value;	/* the thing to do */
	int		rval;	/* the return code */
	int		cnt;	/* number of processes zapped */
	int		error;
	int		found;  /* found an item */
} scan_t;

static int plistscanner(proc_t *, void *, int);

static int
plistscan(scan_t *sp)
{
	sp->error = 0;
	if (!procscan(plistscanner, sp) || sp->error)
		return sp->error;
	return 0;
}

int
schedctl(int cmd, void *arg1, void *arg2, rval_t *rvp)
{
	scan_t scanstuff;
	vproc_t *vpr;
	int error, rval;
	int aff_flag;
	long rval1;
	pid_t pid;

	switch (cmd) {
	case MPTS_RTPRI:	/* arg1 = pid, arg2 = value */
	{
		/*
		 * Check valid priority
		 */
		int new_rtpri;
		vpgrp_t *vpgrp;
		vp_get_attr_t attr;
		int is_batch;
		pid = (pid_t)(__psint_t)arg1;
		new_rtpri = (int)(__psint_t)arg2;

		if (new_rtpri != 0 &&
		   (new_rtpri < NDPHIMAX || new_rtpri > NDPLOMIN))
			return EINVAL;
	
		
		if (pid == current_pid())
			pid = 0;

		/*
		 * Set the real-time priority of a process.  You have to
		 * be super-user to do this, unless setting the calling
		 * process's rt pri to zero (i.e. turning it off), or
		 * setting within a permissable range.
		 */
		if (pid != 0 || (new_rtpri != 0 && new_rtpri <= NDPNORMMIN)) {
			if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
				return EPERM;
		}

		if (pid == 0)
			vpr = curvprocp;
		else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_ATTR(vpr, VGATTR_PGID, &attr);
		vpgrp = VPGRP_LOOKUP(attr.va_pgid);
		if (vpgrp) {
			VPGRP_GETATTR(vpgrp, NULL, NULL, &is_batch);
			if (is_batch) {
				VPGRP_RELE(vpgrp);
				if (pid != 0)
					VPROC_RELE(vpr);
				return EPERM;
			}
			VPGRP_RELE(vpgrp);
		}

		VPROC_SETINFORUNQ(vpr, RQRTPRI, new_rtpri, &rvp->r_val1);

		if (pid != 0)
			VPROC_RELE(vpr);

		return 0;
	}
	case MPTS_GETRTPRI:	/* a1 = pid */
	{
		vproc_t *vpr;

		pid = (pid_t)(__psint_t)arg1;

		if (pid == current_pid())
			pid = 0;
		/*
		 * If request is for this process...
		 */
		if (pid == 0)
			vpr = curvprocp;
		else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;

		VPROC_GETRTPRI(vpr, &rvp->r_val1);

		if (pid)
			VPROC_RELE(vpr);

		return 0;
	}
	case MPTS_SLICE:
	{
		int slice = (int)(__psint_t)arg2;

		pid = (pid_t)(__psint_t)arg1;
		/*
		 * The time slice must be a valid value.  For now
		 * this is arbitrarily less than 1000 ticks (10 seconds).
		 */

		if (slice <= 0 || slice > 1000)
			return EINVAL;
		/*
		 * Set the time slice for a process.  You always have
		 * to be super-user to to this.
		 */
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
			return EPERM;

		if (pid == 0 || pid == current_pid()) {
			pid = 0;
			vpr = curvprocp;
		} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;

		VPROC_SETINFORUNQ(vpr, RQSLICE, slice, &rvp->r_val1);

		if (pid != 0)
			VPROC_RELE(vpr);

		return 0;
	}
	case MPTS_RENICE:
	{
		int pri = (int)(__psint_t)arg2;

		pid = (pid_t)(__psint_t)arg1;

		if (pri < 0 || pri >= NZERO * 2)
			return EINVAL;

		if (pid == current_pid())
			pid = 0;

		if (pid == 0)
			vpr = curvprocp;
		else {
			vpr = VPROC_LOOKUP(pid);
			if (vpr == NULL)
				return ESRCH;
		}

		VPROC_SET_NICE(vpr, &pri, get_current_cred(), 0, error);

		if (pid != 0)
			VPROC_RELE(vpr);

		if (error)
			return error;

		/* return old nice value */
		rvp->r_val1 = pri;
		return 0;
	}

	/*
	 * The following six conditions implement the exact syntax and
	 * semantics of the BSD4.3 getpriority() and setpriority() system
	 * calls.
	 */
	case MPTS_RENICE_PROC:
	{
		int pri = (int)(__psint_t)arg2;

		pid = (pid_t)(__psint_t)arg1;

		if (pid < 0)
			return EINVAL;	

		if (pid == current_pid())
			pid = 0;

		if (pid == 0)
			vpr = curvprocp;
		else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;

		pri = tonice(pri);
		VPROC_SET_NICE(vpr, &pri, get_current_cred(), 0, error);

		if (pid != 0)
			VPROC_RELE(vpr);

		if (error)
			return error;

		rvp->r_val1 = 0;
		return 0;
	}
	case MPTS_RENICE_PGRP:
	{
		pid_t pgid = (pid_t)(__psint_t)arg1;
		int pri = (int)(__psint_t)arg2;
		int count;
		struct vpgrp *vpg;

		if (pgid < 0)
			return EINVAL;

		if (pgid == 0)
			pgid = curprocp->p_pgid;

		vpg = VPGRP_LOOKUP(pgid);
		if (vpg == NULL)
			return ESRCH;

		pri = tonice(pri);
		VPGRP_NICE(vpg, VPG_SET_NICE, &pri, &count,
						get_current_cred(),error);
		VPGRP_RELE(vpg);

		if (error)
			return error;
		if (count == 0)
			return ESRCH;

		rvp->r_val1 = 0;
		return 0;
	}
	case MPTS_RENICE_USER:
	{
		uid_t uid = (uid_t)(__psint_t)arg1;
		int pri = (int)(__psint_t)arg2;

		if (uid < 0)
			return EINVAL;

		scanstuff.found = 0;
		scanstuff.cmd = TSSCAN_RENICE_USER;
		scanstuff.pid = (uid == 0 ? get_current_cred()->cr_uid : uid);
		scanstuff.value = tonice(pri);
		if (error = plistscan(&scanstuff))
			return error;
		if (scanstuff.found == 0)
			return ESRCH;
		rvp->r_val1 = 0;
		return 0;
	}
	case MPTS_GTNICE_PROC:
	{
		int pri;

		pid = (pid_t)(__psint_t)arg1;

		if (pid < 0)
			return EINVAL;

		if (pid == current_pid())
			pid = 0;

		if (pid == 0)
			vpr = curvprocp;
		else {
			vpr = VPROC_LOOKUP(pid);
			if (vpr == NULL)
				return ESRCH;
		}

		VPROC_GET_NICE(vpr, &pri, get_current_cred(), error);

		if (pid != 0)
			VPROC_RELE(vpr);

		if (error)
			return error;
		rvp->r_val1 = fromnice(pri);
		return 0;
	}
	case MPTS_GTNICE_PGRP:
	{
		pid_t pgid = (pid_t)(__psint_t)arg1;
		int pri;
		int count;
		struct vpgrp *vpg;

		if (pgid < 0)
			return EINVAL;

		if (pgid == 0)
			pgid = curprocp->p_pgid;

		vpg = VPGRP_LOOKUP(pgid);
		if (vpg == NULL)
			return ESRCH;

		VPGRP_NICE(vpg, VPG_GET_NICE, &pri, &count,
						get_current_cred(),error);
		VPGRP_RELE(vpg);

		if (error)
			return error;
		if (count == 0)
			return ESRCH;

		rvp->r_val1 = fromnice(pri);
		return 0;
	}
	case MPTS_GTNICE_USER:
	{
		uid_t uid = (uid_t)(__psint_t)arg1;

		if (uid < 0)
			return EINVAL;

		scanstuff.found = 0;
		scanstuff.cmd = TSSCAN_GTNICE_USER;
		scanstuff.value = MAXINT;
		scanstuff.pid = (uid == 0 ? get_current_cred()->cr_uid : uid);
		if (error = plistscan(&scanstuff))
			return error;

		if (scanstuff.found == 0)
			return ESRCH;
		rvp->r_val1 = fromnice(scanstuff.value);
		return 0;
	}
	/*
	 * End of hooks for getpriority and setpriority emulation.
	 */

	case MPTS_SETHINTS:	/* a1 = version # */
		if (curuthread->ut_prda == NULL) {
			if (error = lockprda(&curuthread->ut_prda)) {
				return 0; /* XXX */
			}
			ASSERT(curuthread->ut_prda->t_sys.t_pid ==
								current_pid());
		}
		curuthread->ut_prda->t_sys.t_hint = 0;
		rvp->r_val1 = (__psint_t)&PRDA->t_sys;
#ifdef USE_PTHREAD_RSA
		if (!IS_KSEG0(curuthread->ut_prda) &&
		     (colorof(PRDAADDR) !=
		      pfncolorof(kvatopfn(curuthread->ut_prda)))) {
			curuthread->ut_rsa_locore = 0;
			cmn_err_tag(316,CE_WARN,"MPTS_SETHITS: PRDA bad color, set ut_rsa_locore to zero\n");
		}
#endif /* USE_PTHREAD_RSA */
		return 0;

	case MPTS_SETMASTER:	/* a1 = pid */

		pid = (pid_t)(__psint_t)arg1;
		/*
		 * Turn the supplied PID into a proc pointer (returns locked).
		 */
		if (pid == 0 || pid == current_pid()) {
			pid = 0;
			vpr = curvprocp;
		} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;

		VPROC_SETMASTER(vpr, current_pid(), &error);

		if (pid != 0)
			VPROC_RELE(vpr);

		rvp->r_val1 = 0;
		return error;

	case MPTS_SCHEDMODE:	/* a1 = value */
	{
		uint a1;

		a1 = (uint)(__psint_t)arg1;

		if (a1 < SGS_FREE || a1 > SGS_GANG)
			return EINVAL;

		CURVPROC_SCHEDMODE(a1, &rvp->r_val1, &error);

		return error;
	}

	case MPTS_AFFINITY_ON:
		aff_flag = VSCHED_AFF_ON;
		goto check_aff;

	case MPTS_AFFINITY_OFF:
		aff_flag = VSCHED_AFF_OFF;

check_aff:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
			return EPERM;
		goto do_aff;

	case MPTS_AFFINITY_GET:
		aff_flag = VSCHED_AFF_GET;

do_aff:
		pid = (pid_t)(__psint_t)arg1;

		if (pid == 0 || pid == current_pid()) {
			pid = 0;
			vpr = curvprocp;
		} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return EINVAL;

		VPROC_SCHED_AFF(vpr, aff_flag, &rvp->r_val1);

		if (pid != 0)
			VPROC_RELE(vpr);
		return 0;

        /*
         * Old Frame Scheduler Calls
         * We return "not supported"
         */
        case MPTS_OLDFRS_CREATE:
        case MPTS_OLDFRS_ENQUEUE:
        case MPTS_OLDFRS_DEQUEUE:
        case MPTS_OLDFRS_START:
        case MPTS_OLDFRS_STOP:
        case MPTS_OLDFRS_DESTROY:
        case MPTS_OLDFRS_YIELD:
        case MPTS_OLDFRS_JOIN:
        case MPTS_OLDFRS_INTR:
        case MPTS_OLDFRS_GETQUEUELEN:
        case MPTS_OLDFRS_READQUEUE:
        case MPTS_OLDFRS_PREMOVE:
        case MPTS_OLDFRS_PINSERT:
        case MPTS_OLDFRS_RESUME:
        case MPTS_OLDFRS_SETATTR:
        case MPTS_OLDFRS_GETATTR:
                return ENOTSUP;

	/*
	 * Frame Scheduler
	 */
        case MPTS_FRS_CREATE:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_create(arg1, arg2));
        case MPTS_FRS_ENQUEUE:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_enqueue(arg1));
        case MPTS_FRS_JOIN:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        rval = frs_user_join((pid_t)(__psint_t)arg1, &rval1);
		rvp->r_val1 = rval1;
		return rval;
        case MPTS_FRS_DEQUEUE:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_dequeue());
        case MPTS_FRS_START:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_start((pid_t)(__psint_t)arg1));
        case MPTS_FRS_STOP:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_stop((pid_t)(__psint_t)arg1));
        case MPTS_FRS_DESTROY:
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_destroy((pid_t)(__psint_t)arg1)); 
        case MPTS_FRS_YIELD:
        	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        rval = frs_user_yield(&rval1);
		rvp->r_val1 = rval1;
		return rval;
        case MPTS_FRS_INTR:
        	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_eventintr((pid_t)(__psint_t)arg1));
        case MPTS_FRS_GETQUEUELEN:
        	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        rval = frs_user_getqueuelen(arg1, &rval1);
	        rvp->r_val1 = rval1;
	        return rval;
        case MPTS_FRS_READQUEUE:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        rval = frs_user_readqueue(arg1, arg2, &rval1);
		rvp->r_val1 = rval1;
		return rval;
        case MPTS_FRS_PREMOVE:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_premove(arg1));
        case MPTS_FRS_PINSERT:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_pinsert(arg1, arg2));
        case MPTS_FRS_RESUME:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_resume((pid_t)(__psint_t)arg1));
        case MPTS_FRS_SETATTR:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_setattr(arg1));
        case MPTS_FRS_GETATTR:
	        if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT)) {
			return EPERM;
		}
	        return (frs_user_getattr(arg1));
        default:
		return EINVAL;
	}
}

/*
 * General purpose scanner we use for our various dirty deeds.
 */

static int
plistscanner(proc_t *pp, void *arg, int mode)
{
	scan_t *sp = (scan_t *)arg;
	cred_t *cr;
	vproc_t *vpr;
	int nice;

	switch (mode) {
	case 0:
		sp->rval = 0;
		sp->cnt = 0;
		break;
	case 2:
		return sp->cnt;
	case 1:
		switch (sp->cmd) {

		case TSSCAN_RENICE_USER:
			/*
			 * Hold p_lock of subject process to avoid
			 * getting a stale cred pointer if it does a 
			 * crcopy() at the same time.  Note that its
			 * uid could still change by the time we actually
			 * apply the new nice value, but this is no 
			 * different than the usual race you have with
			 * such system calls.
			 */
			cr = pcred_access(pp);
			if (cr->cr_uid != sp->pid) {
				pcred_unaccess(pp);
				break;
			}
			pcred_unaccess(pp);
			sp->found++;
			vpr = PROC_TO_VPROC(pp);
			if (VPROC_HOLD(vpr) == ESRCH)
				break;
			nice = sp->value;	/* writable copy */
			sp->error = proc_set_nice(pp, &nice,
						  get_current_cred(), 0);
			VPROC_RELE(vpr);

			if (sp->error)
				return 1;

			sp->cnt++;
			break;

		case TSSCAN_GTNICE_USER:
			cr = pcred_access(pp);
			if (cr->cr_uid != sp->pid) {
				pcred_unaccess(pp);
				break;
			}
			pcred_unaccess(pp);
			sp->found++;

			vpr = PROC_TO_VPROC(pp);
			if (VPROC_HOLD(vpr) == ESRCH)
				break;
			sp->error = proc_get_nice(pp, &nice,
						  get_current_cred());
			VPROC_RELE(vpr);

			if (sp->error)
				return 1;

			if (nice < sp->value)
				sp->value = nice;
			sp->cnt++;
			break;
		}
	}
	return 0;
}
