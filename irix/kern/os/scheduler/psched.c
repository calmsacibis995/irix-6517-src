/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/par.h>
#include <sys/space.h>
#include <sys/sched.h>
#include <sys/timers.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/kthread.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>

struct sched_setparama {
	sysarg_t pid;
	struct sched_param *param;
};

int
sched_setparam(struct sched_setparama *uap)
{
	vproc_t *vpr;
	struct sched_param kparam;
	int pri;
	pid_t pid = uap->pid;
	int error = 0;

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
		return EPERM;

	if (pid == 0 || pid == current_pid()) {
		pid = 0;
		vpr = curvprocp;
	} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ESRCH;

	if (copyin(uap->param, &kparam, sizeof(kparam)))
		return EFAULT;
	pri = kparam.sched_priority;

	VPROC_SCHED_SETPARAM(vpr, pri, &error);

	if (pid != 0)
		VPROC_RELE(vpr);

	if (error)
		return error;

	if (pid == 0)		/* current pid */
		qswtch(MUSTRUNCPU);

	return 0;
}

struct sched_getparama {
	sysarg_t pid;
	struct sched_param *param;
};

int
sched_getparam(struct sched_getparama *uap)
{
	vproc_t *vpr;
	struct sched_param kparam;
	cred_t *cr = get_current_cred();
	pid_t pid = uap->pid;
	int error = 0;

	if (pid == 0 || pid == current_pid()) {
		pid = 0;
		vpr = curvprocp;
	} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ESRCH;

	VPROC_SCHED_GETPARAM(vpr, cr, &kparam.sched_priority, &error);

	if (pid != 0)
		VPROC_RELE(vpr);

	if (error)
		return error;

	if (copyout(&kparam, uap->param, sizeof kparam))
		return EFAULT;

	return 0;
}

struct sched_setschedulera {
	sysarg_t pid;
	sysarg_t policy;
	struct sched_param *param;
};

int
sched_setscheduler(struct sched_setschedulera *uap, rval_t *rvp)
{
	vproc_t *vpr;
	struct sched_param kparam;
	pid_t pid = uap->pid;
	int error = 0;
	int pri;

	if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
		return EPERM;

	if (copyin(uap->param, &kparam, sizeof(kparam)))
		return EFAULT;
	pri = kparam.sched_priority;

	if (pid == 0 || pid == current_pid()) {
		pid = 0;
		vpr = curvprocp;
	} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ESRCH;

	VPROC_SCHED_SETSCHEDULER(vpr, pri, uap->policy, &rvp->r_val1, &error);

	if (pid != 0)
		VPROC_RELE(vpr);

	if (error)
		return error;

	if (pid == 0)			/* current pid */
		qswtch(MUSTRUNCPU);

	return 0;
}

struct sched_getschedulera {
	sysarg_t pid;
};

int
sched_getscheduler(struct sched_getschedulera *uap, rval_t *rvp)
{
	vproc_t *vpr;
	cred_t *cr;
	pid_t pid = uap->pid;
	int error = 0;

	if (pid == 0 || pid == current_pid()) {
		pid = 0;
		vpr = curvprocp;
	} else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ESRCH;

	cr = get_current_cred();

	VPROC_SCHED_GETSCHEDULER(vpr, cr, &rvp->r_val1, &error);

	if (pid != 0)
		VPROC_RELE(vpr);

	return error;
}

int
sched_yield(void)
{
	user_resched(RESCHED_Y);
	return 0;
}

struct sched_rr_get_intervala {
	sysarg_t pid;
	struct timespec *interval;
};

int
sched_rr_get_interval(struct sched_rr_get_intervala *uap)
{
	vproc_t *vpr;
	struct timespec timeslice;
	pid_t pid = uap->pid;

	if (pid == 0 || pid == current_pid()) {
		tick_to_timespec(curuthread->ut_tslice,
				 &timeslice, NSEC_PER_TICK);
	} else {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_SCHED_RR_GET_INTERVAL(vpr, &timeslice);

		VPROC_RELE(vpr);
	}

	if (XLATE_COPYOUT(&timeslice, uap->interval, sizeof timeslice,
			  timespec_to_irix5, get_current_abi(), 1))
		return EFAULT;

	return 0;
}
