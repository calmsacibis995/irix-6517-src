/*
 * Copyright 1990-1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.338 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/callo.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/dir.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/ksignal.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <ksys/vpgrp.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/profil.h>
#include <sys/resource.h>
#include <sys/sat.h>
#include <ksys/vsession.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/ktimes.h>
#include <sys/tuneable.h>
#include <sys/ulimit.h>
#include <sys/unistd.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <sys/runq.h>
#include <sys/strsubr.h>
#include <sys/xlate.h>
#include <sys/capability.h>
#include <ksys/pid.h>
#include <sys/cmn_err.h>
#include <ksys/vproc.h>
#include <ksys/exception.h>
#include "os/proc/pproc_private.h"
#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif

extern int acl_enabled;
extern int sat_enabled;
extern int mac_enabled;
extern int cap_enabled;
extern int sesmgr_enabled;
extern int pmqomax;

/*
 * Everything in this file is a routine implementing a system call.
 */
/* ARGSUSED */
int
gtime(void *uap, rval_t *rvp)
{
	struct timeval tv;

	microtime(&tv);		/* get the most accurate time */
	rvp->r_time = tv.tv_sec;
	return 0;
}

struct stimea {
	sysarg_t time;
};

int
stime(struct stimea *uap)
{
	cpu_cookie_t was_running;

	if (!_CAP_ABLE(CAP_TIME_MGT))
		return EPERM;
	was_running = setmustrun(clock_processor);
	settime((time_t)uap->time, 0);
	wtodc();
	restoremustrun(was_running);
	_SAT_CLOCK((time_t)uap->time,0);
	return 0;
}

struct setida {
	sysarg_t id;
};

int
setuid(struct setida *uap)
{
	uid_t uid;
	int error;

	uid = uap->id;
	if (uid < 0 || uid > MAXUID)
		return EINVAL;
	CURVPROC_SETUID(-1, uid, VSUID_SYSV, error);
	return error;
}

/*
 * 4.3BSD set real and effective user IDs.
 */
struct setreida {
	sysarg_t rid;
	sysarg_t eid;
};

int
setreuid(struct setreida *uap)
{
	uid_t ruid = uap->rid;	/* real user ID */
	uid_t euid = uap->eid;	/* effective user ID */
	cred_t *crp = get_current_cred();
	int error;

	if (ruid == (uid_t)-1)
		ruid = crp->cr_ruid;
	if (euid == (uid_t)-1)
		euid = crp->cr_uid;

	/* check after checking for -1 */
	if (ruid < 0 || ruid > MAXUID || euid < 0 || euid > MAXUID)
		return EINVAL;

	CURVPROC_SETUID(ruid, euid, 0, error);
	return error;
}

/* ARGSUSED */
int
getuid(void *uap, rval_t *rvp)
{
	cred_t *cr = get_current_cred();

	rvp->r_val1 = cr->cr_ruid;
	rvp->r_val2 = cr->cr_uid;
	return 0;
}

int
setgid(struct setida *uap)
{
	gid_t gid;
	int error;

	gid = uap->id;
	if (gid < 0 || gid > MAXUID)
		return EINVAL;

	CURVPROC_SETGID(-1, gid, VSUID_SYSV, error);
	return error;
}

/* Service function for BSD setregid, and XPG4 version: xpg4_setregid. */
static int
k_setregid(struct setreida *uap, int is_xpg4)
{
	gid_t rgid = uap->rid;	/* real group ID */
	gid_t egid = uap->eid;	/* effective group ID */
	cred_t *cr = get_current_cred();
	int error;

	if (rgid == (gid_t)-1)
		rgid = cr->cr_rgid;
	if (egid == (gid_t)-1)
		egid = cr->cr_gid;

	if (rgid < 0 || rgid > MAXUID || egid < 0 || egid > MAXUID)
		return EINVAL;

	CURVPROC_SETGID(rgid, egid, is_xpg4 ? VSUID_XPG4 : 0, error);
	return error;
}

/*
 * 4.3BSD set real and effective group IDs.
 */
int
setregid(struct setreida *uap)
{
	return k_setregid(uap, 0);
}

/* XPG4 wrapper for setregid - to allow us to get to the slightly
 * different xpg4 semantics.
 */
int
xpg4_setregid(struct setreida *uap)
{
	/* Flag argument of 1 indicates xpg4 calling */
	return k_setregid(uap, 1);
}

/* ARGSUSED */
int
getgid(void *uap, rval_t *rvp)
{
	cred_t *cr = get_current_cred();

	rvp->r_val1 = cr->cr_rgid;
	rvp->r_val2 = cr->cr_gid;
	return 0;
}

/*
 * getgroups(), setgroups() and groupmember()
 * implement POSIX-style multiple groups.
 * BSD-style multiple groups (gidset is of type int rather than gid_t)
 * are implemented as a library layer on top of this system call.
 */

/*
 * getgroups doesn't need to grab the s_rupdlock, because it is taking a
 * snapshot of its own credentials, and only it can update them from the
 * shared area (upon exit from or entrance to the kernel).  Note the
 * POSIX-added functionality of gidsetsize == 0, which returns the number
 * of groups but doesn't modify the gidset array.
 */
int
getgroups(int gidsetsize, gid_t *gidset, rval_t *rvp)
{
	struct cred *cr = get_current_cred();
	int n;
	gid_t *grp;

	n = cr->cr_ngroups;
	grp = cr->cr_groups; 

	if (gidsetsize != 0) {
		if (gidsetsize < n)
			return EINVAL;
		if (copyout(grp, gidset, n * sizeof(gid_t)))
			return EFAULT;
	}

	rvp->r_val1 = n;
	return 0;
}

/* permit AFS to replace setgroups */
extern int setgroups(int gidsetsize, gid_t *gidset);
int (*setgroupsp)(int, gid_t *) = setgroups;

int
setgroups(int gidsetsize, gid_t *gidset)
{
	gid_t newgids[NGROUPS_UMAX];
	int i;

	if (!_CAP_CRABLE(get_current_cred(), CAP_SETGID))
		return EPERM;

	if (gidsetsize < 0 || gidsetsize > ngroups_max)
		return EINVAL;

	if (gidsetsize != 0
	    && copyin(gidset, newgids, gidsetsize * sizeof(gid_t))) {
		return EFAULT;
	}

	for (i = 0; i < gidsetsize; i++) {
		if (newgids[i] < 0 || newgids[i] > MAXUID) {
			_SAT_SETGROUPS(gidsetsize, newgids, EINVAL);
			return EINVAL;
		}
	}

	CURVPROC_SETGROUPS(gidsetsize, newgids);

	return 0;
}

/* ARGSUSED */
int
getpid(void *uap, rval_t *rvp)
{
	rvp->r_val1 = current_pid();
	rvp->r_val2 = curprocp->p_ppid;

	return 0;
}

/*
 * set/get process group (System V style)
 */
struct setpgrpa {
	sysarg_t flag;
};

int
setpgrp(struct setpgrpa *uap, rval_t *rvp)
{
	pid_t new_pgid;
	int error;
	/* argument of 0 indicates getpgrp call */
	if (uap->flag == 0) {
		vp_get_attr_t attr;

		VPROC_GET_ATTR(curvprocp, VGATTR_PGID, &attr);
	       	rvp->r_val1 = attr.va_pgid;

		_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, current_pid(),
			get_current_cred(), 0, -1);
		return 0;
	}

	CURVPROC_SETPGRP(VSPGRP_SYSV, &new_pgid, error);

        rvp->r_val1 = new_pgid;
	return error;
}

/*
 * get process group (4.3 BSD style)
 */
struct getpgrpa {
	sysarg_t pid;
};

int
BSDgetpgrp(struct getpgrpa *uap, rval_t *rvp)
{
	vproc_t *vpr;
	struct proc *p;
	pid_t pid;
	cred_t *cr;
	int error = 0;

	if ((pid = uap->pid) == 0)
		pid = current_pid();

	vpr = VPROC_LOOKUP(pid);
	if (vpr == 0) {
		_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, pid, NULL, ESRCH, -1);
		return ESRCH;
	}

	/* XXX for _MAC_ACCESS, _SAT_PROC_ACCESS */
	VPROC_GET_PROC(vpr, &p);

	cr = pcred_access(p);
	if (_MAC_ACCESS(cr->cr_mac, get_current_cred(), MACREAD)) {
		error = EACCES;
		pcred_unaccess(p);
	} else {
		vp_get_attr_t attr;

		/* can't hold this lock when calling getattr */
		pcred_unaccess(p);
		VPROC_GET_ATTR(vpr, VGATTR_PGID, &attr);
		rvp->r_val1 = attr.va_pgid;
	}

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, pid, cr, error, -1);

	VPROC_RELE(vpr);

	return error;
}

/*
 * set process group (4.3 BSD style)
 */
struct BSDsetpgrpa {
	sysarg_t pid;
	sysarg_t pgrp;
};

int setpgid(pid_t pid, pid_t pgid);

int
BSDsetpgrp(struct BSDsetpgrpa *uap)
{
	pid_t pid, pgrp;
	int error;

	pid = uap->pid;
	pgrp = uap->pgrp;

	if (pgrp < 0 || pgrp > MAXPID) {
		_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, pid, NULL, EINVAL, -1);
		return EINVAL;
	}

	if (pid == 0)
		pid = current_pid();

	/*
	 * If pgrp is zero and pid is the calling process,
	 * it should behave identically to setsid(). (Or at least
	 * almost identically - there is some extra semantic checking
	 * in the setsid path - hence this calls CURVPROC_SETPGRP,
	 * which it shares with SysV setpgrp.)
	 */
	if (pgrp == 0 && pid == current_pid()) {
		pid_t new_pgid;

		CURVPROC_SETPGRP(0, &new_pgid, error);

		return error;
	}

	return setpgid(pid, pgrp);
}

/*
 * POSIX set-session system call.
 */
/* ARGSUSED */
int
setsid(void *uap, rval_t *rvp)
{
	int		error;
	pid_t		new_pgid;

	CURVPROC_SETSID(&new_pgid, error);

	if (error)
		return error;

        rvp->r_val1 = new_pgid;
	return 0;
}

/*
 * POSIX set-process-group system call.  Also implements BSD 4.3Lite setpgrp.
 */
int
setpgid(pid_t pid, pid_t pgid)
{
	vproc_t *vpr;
	int error;
	pid_t sid;

	if (pgid < 0 || pgid > MAXPID)
		return EINVAL;

	if (pid == 0)
		pid = current_pid();
	
	if (pid == current_pid())
		vpr = curvprocp;
	else {
		vp_get_attr_t attr;

		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		/* Since we are acting on a process other than ourself,
		 * we need to supply our sid to the setpgid op.
		 */
		VPROC_GET_ATTR(curvprocp, VGATTR_SID, &attr);
		sid = attr.va_sid;
	}

	VPROC_SETPGID(vpr, pgid, current_pid(), sid, error);

	if (pid != current_pid())
		VPROC_RELE(vpr);

	return error;
}

struct nicea {
	sysarg_t niceness;
};

int
nice(struct nicea *uap, rval_t *rvp)
{
	int incr;
	int error;

	/*
	 * This one is (fairly) safe, since only clock cares if we
	 * change the nice value.  Since clock accesses the field
	 * read-only, and we only store over the old value, this is
	 * a safe operation (semantics are maintained).
	 */
	incr = uap->niceness;

	if ((incr < 0 || incr > 2 * NZERO) && !_CAP_ABLE(CAP_SCHED_MGT))
		return EPERM;

	VPROC_SET_NICE(curvprocp, &incr, get_current_cred(), VNICE_INCR, error);

	if (!error)
		rvp->r_val1 = incr - NZERO;

	return error;
}

struct timesa {
	struct tms *tmsp;
};

int
times(struct timesa *uap, rval_t *rvp)
{
	struct tms rtime;

	CURVPROC_TIMES(&rtime);

	rvp->r_time = (irix5_clock_t) lbolt;
	if (copyout(&rtime, uap->tmsp, sizeof(struct tms)))
		return EFAULT;

	return 0;
}

#if _MIPS_SIM == _ABI64
/*
 * The arguments to profil(2) are passed in registers, so they
 * show up as sysarg_t. This is different from 32 bit programs
 * calling sprofil(2) which will pass in a struct that contains
 * 32 bit pointers.
 */
struct irix5_profila {
	sysarg_t pr_base;
	sysarg_t pr_size;
	sysarg_t pr_off;
	sysarg_t pr_scale;
};

static void bcopy_irix5_to_prof(struct irix5_profila *, struct prof *);
static int irix5_to_prof(enum xlate_mode, void *, int, xlate_info_t *);
#else
#define bcopy_irix5_to_prof(i5_p, p)	bcopy(i5_p, p, sizeof(struct prof))
#endif

/*
 * Profiling for shared libraries (sprofil(2)). This is implemented via the
 * syssgi(SGI_PROFILE, arg1, arg2, arg3, arg4) call in os/syssgi.c.
 */
/* ARGSUSED */
int
sprofil(void *profp,
	int profcnt,
	void *tvp,
	unsigned int flags,
	int calltype)		/* 0 => profil(); 1=> sprofil() */
{
	struct prof *prp, *newprp, tmp;
	int i, j, enable = 0, sortcnt;
	struct timeval rtv;
	register int s;
	int error = 0;
	proc_t	*p = curprocp;
	extern int restrict_fastprof;	/* 1 ==> don't allow fast profiling */
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

	if ((flags & PROF_FAST) && restrict_fastprof == 1)
		return EACCES;
	if (profcnt <= 0 || profcnt > nprofile)
		return E2BIG;
	newprp = (struct prof *)kern_malloc(profcnt * sizeof(struct prof));
	if (calltype == 0) {
		(void) bcopy_irix5_to_prof(profp, newprp);
	} else {
		if (COPYIN_XLATE(profp, newprp, profcnt * sizeof *newprp,
				 irix5_to_prof, abi, profcnt)) {
			error = EFAULT;
			goto errexit;
		}
	}
	for (i = 0, prp = newprp; i < profcnt; i++, prp++) {
		if (prp->pr_scale & ~1)
			enable = 1;
		if ((prp->pr_scale == 0x0002) && (prp->pr_off == 0) &&
		    (i != profcnt - 1)) {
			error = EINVAL;
			goto errexit;
		}
	}
	if (tvp) {
		tick_to_timeval(1, &rtv, USEC_PER_TICK);
		if (flags & PROF_FAST)
			rtv.tv_usec = rtv.tv_usec / PROF_FAST_CNT;
		if (XLATE_COPYOUT(&rtv, tvp, sizeof rtv,
				  timeval_to_irix5_xlate, abi, 1)){
			error = EFAULT;
			goto errexit;
		}
	}

	ASSERT(((p->p_profn == 0) && (p->p_profp == NULL)) || \
	       ((p->p_profn != 0) && (p->p_profp != NULL)));
	/*
	 * Manipulate p_flag, p_profn and p_profp.  Once we have reached
	 * here we cannot fail for any reason.  Profiling is possible
	 * iff at least the (pr_scale & ~1) is non-zero for at least one 
	 * segment. If this is the case, then we make p_profn non-zero. 
	 * While manipulating p_profn, we must not take any clock interrupts.
	 */
	prp = p->p_profp;
	if (enable) {
		/*
		 * Sort by increasing pr_off. Exclude overflow bin
		 * since it needs to be the last one.
		 */
		sortcnt = profcnt;
		if (newprp[profcnt - 1].pr_off == 0 &&
		    newprp[profcnt - 1].pr_scale == 0x0002)
			sortcnt--;

		for (i = 1; i < sortcnt; i++) {
			tmp = newprp[i];
			for (j = i - 1;
			     j >= 0 && newprp[j].pr_off > tmp.pr_off;
			     j--)
				newprp[j+1] = newprp[j];
			newprp[j+1] = tmp;
		}

		s = p_lock(p);
		p->p_profn = profcnt;
		p->p_profp = newprp;
		p->p_fastprofcnt = 0;
		newprp = (struct prof *)NULL;

		if (flags & PROF_UINT) {
			p->p_flag |= SPROF|SPROF32;	  /* 32-bit buckets */
		} else {
			p->p_flag |= SPROF;
			p->p_flag &= ~SPROF32; /* 16-bit buckets */
		}

		if (flags & PROF_FAST) {
			p->p_flag |= SPROFFAST;  /* fast user prof */
			startprfclk();
		}
		p_unlock(p, s);
	} else {
		s = p_lock(p);

		/*
		 * Since we are never going to update any counters
		 * (pr_scale & ~1 == 0), we might as well turn profiling off.
		 */
		if (p->p_flag & SPROFFAST)
			stopprfclk();

		p->p_flag &= ~(SPROFFAST|SPROF|SPROF32);
		p->p_profn = 0;
		p->p_fastprofcnt = 0;
		p->p_profp = NULL;
		p_unlock(p, s);

		VPROC_FLAG_THREADS(curvprocp, UT_OWEUPC, UTHREAD_FLAG_OFF);
	}

	if (prp)
		kern_free(prp);

	if (newprp)
errexit:
		kern_free(newprp);

	return error;
}

/*
 * Profiling
 */
int
profil(void *uap)
{
	int error;

	error = sprofil(uap, 1, NULL, PROF_USHORT, 0);
	ASSERT(((curprocp->p_profn == 0) && (curprocp->p_profp == NULL)) || \
	       ((curprocp->p_profn != 0) && (curprocp->p_profp != NULL)));
	return error;
}

#ifdef R10000
typedef struct evc_apply {
	hwperf_profevctrarg_t *arg;
	int *rvalp;
} evc_apply_t;

static int
evc_enable_counters(uthread_t *ut, void *args)
{
	evc_apply_t *evap = (evc_apply_t *)args;
	int error;

	/* get the counters and set them up */

	error = hwperf_enable_counters(cpumon_alloc(ut),
				       evap->arg, NULL,
				       HWPERF_PROC|HWPERF_PROFILING,
				       evap->rvalp);
	return error;
}

/*ARGSUSED*/
static int
evc_disable_counters(uthread_t *ut, void *args)
{
	if (ut->ut_cpumon)
		hwperf_disable_counters(ut->ut_cpumon);
	return 0;
}

/*
 * profiling based upon R10000 event counters rather than the clock.
 */
int
evc_profil(hwperf_profevctrarg_t *evctrargp, 
	   void *profp,
	   int profcnt,
	   unsigned int flags,
	   int *rvalp)
{
	struct prof 	*prp, *newprp, tmp;
	int 		i, j, enable = 0;
	int 		sortcnt, s;
	int		error = 0;
	uthread_t	*ut = curuthread;
	proc_t		*p = UT_TO_PROC(ut);
	evc_apply_t	evc;

	if (rvalp)
		*rvalp = -1;

	if (profcnt <= 0 || profcnt > nprofile)
		return E2BIG;
	newprp = (struct prof *)kern_malloc(profcnt * sizeof(struct prof));
	if (COPYIN_XLATE(profp, newprp, profcnt * sizeof *newprp,
			 irix5_to_prof, get_current_abi(), profcnt)) {
		kern_free(newprp);
		return EFAULT;
	}
	for (i = 0, prp = newprp; i < profcnt; i++, prp++) {
		if (prp->pr_scale & ~1)
			enable = 1;
		if ((prp->pr_scale == 0x0002) && (prp->pr_off == 0) &&
		    (i != profcnt - 1)) {
			kern_free(newprp);
			return EINVAL;
		}
	}

	ASSERT(((p->p_profn == 0) == (p->p_profp == NULL)));

	/*
	 * Manipulate p_flag, p_profn and p_profp.  Once we have reached
	 * here we cannot fail for any reason.  Profiling is possible
	 * iff at least the (pr_scale & ~1) is non-zero for at least one 
	 * segment. If this is the case, then we make p_profn non-zero. 
	 * While manipulating p_profn, we must not take any clock interrupts.
	 */
	prp = p->p_profp;
	if (enable) {
		/*
		 * Since it is possible to be doing clock-based
		 * profiling at the same time as counter-based
		 * profiling, be careful about clearing OWEUPC.
		 */
		if (!(p->p_flag & SPROF))
			VPROC_FLAG_THREADS(curvprocp,
					   UT_OWEUPC, UTHREAD_FLAG_OFF);

		/*
		 * Sort by increasing pr_off. Exclude overflow bin
		 * since it needs to be the last one.
		 */
		sortcnt = profcnt;
		if (newprp[profcnt - 1].pr_off == 0 &&
		    newprp[profcnt - 1].pr_scale == 0x0002)
			sortcnt--;

		for (i = 1; i < sortcnt; i++) {
			tmp = newprp[i];
			for (j = i - 1;
			     j >= 0 && newprp[j].pr_off > tmp.pr_off;
			     j--)
				newprp[j+1] = newprp[j];
			newprp[j+1] = tmp;
		}

		s = p_lock(p);
		p->p_profn = profcnt;
		p->p_profp = newprp;
		newprp = (struct prof *)NULL;

		if (flags & PROF_UINT)
			p->p_flag |= SPROF32;	  /* 32-bit buckets */
		else
			p->p_flag &= ~(SPROF32); /* 16-bit buckets */
		p_unlock(p, s);

		/* get the counters and set them up */
		evc.arg = evctrargp;
		evc.rvalp = rvalp;
		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
				      evc_enable_counters, (void *)&evc);
	} else {
		uthread_apply(&p->p_proxy, UT_ID_NULL,
			      evc_disable_counters, NULL);
		if (rvalp)
			*rvalp = 0;

		if (!(p->p_flag & SPROF))
			VPROC_FLAG_THREADS(curvprocp,
					   UT_OWEUPC, UTHREAD_FLAG_OFF);
		/*
		 * Since we are never going to update any counters
		 * (pr_scale & ~1 == 0), we might as well turn profiling off.
		 */
		s = p_lock(p);
		p->p_flag &= ~SPROF32;
		p->p_profn = 0;
		p->p_profp = NULL;
		p_unlock(p, s);
	}

	if (prp)
		kern_free(prp);
	if (newprp)
		kern_free(newprp);
	return error;
}
#endif /* R10000 */

/*
 * alarm clock signal.  Modified to deal with itimers instead.
 */
struct alarma {
	usysarg_t deltat;
};

int
alarm(struct alarma *uap, rval_t *rvp)
{
	struct proc *p = curprocp;
	struct timeval tv;
	__int64_t ticknum;
	int s;

	timerclear(&tv);

	s = mutex_spinlock_spl(&p->p_itimerlock, splprof);

	/* figure out # of ticks remaining in callout list */

	if (timerisset(&p->p_realtimer.it_value)) {
		/* only look on the slow callout list */
		ticknum = chktimeout_tick(C_NORM, p->p_itimer_toid, NULL, 0);
		tick_to_timeval((int)ticknum, &tv, USEC_PER_TICK);
	}

	/* remove the previous alarm call */
	untimeout(p->p_itimer_toid);
	p->p_itimer_toid = 0;
	timerclear(&p->p_realtimer.it_interval);

	/* argument = 0; just clean out timer data
	 * and return last set value
	 */
	p->p_realtimer.it_value.tv_usec = 0;
	if (p->p_realtimer.it_value.tv_sec = uap->deltat)
		p->p_itimer_toid = timeout(realitexpire, p, hzto(&p->p_realtimer.it_value));
	mutex_spinunlock(&p->p_itimerlock, s);

	/*
	 *  Nominally, we return the number of seconds remaining in the last
	 *  aborted timeout, or zero.  Posix says if the time remaining in
	 *  that timeout is less than 1 second, then return 1 anyway, to
	 *  indicate that a previous timeout was in fact pending.
	 */
	rvp->r_val1 = (tv.tv_sec == 0  &&  tv.tv_usec != 0) ? 1 : tv.tv_sec;
	return 0;
}

/*
 * indefinite wait.
 * no one should wakeup k_timewait
 */
int
pause(void)
{
	/*
	 * Use private wait variable.
	 */
	kthread_t *kt = private.p_curkthread;
	uthread_t *ut = curuthread;

	for (;;) {
		if (ut_sleepsig(ut, &kt->k_timewait, 0, ut_lock(ut)) == -1)
			return EINTR;
	}
	/* NOTREACHED */
}

/*
 * mode mask for creation of files
 */
struct umaska {
	usysarg_t mask;
};

int
umask(struct umaska *uap, rval_t *rvp)
{
	uthread_t *ut = curuthread;
	short omask = ut->ut_cmask;
	/* REFERENCED */
	int error;

	ut->ut_cmask = uap->mask & PERMMASK;

	/*
	 * CURVPROC_UMASK will update omask only if the process shares
	 * umask with other processes or many uthreads.
	 */
	CURVPROC_UMASK(ut->ut_cmask, &omask, error);

	ASSERT(!error);

	rvp->r_val1 = omask;
	return 0;
}

struct ulimita {
	sysarg_t cmd;
	sysarg_t arg;
};

int
ulimit(struct ulimita *uap, rval_t *rvp)
{
	struct rlimit rlim;
	off_t newlim;
	int abi = get_current_abi();
	int error;

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	switch (uap->cmd) {

	case UL_SFILLIM:	/* Set new file size limit. */
		newlim = uap->arg;
		if (!ABI_IS_IRIX5_64(abi)) {
			ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));
			if (newlim >= (off_t)OFFTOBB(RLIM32_INFINITY))
				newlim = RLIM_INFINITY;
		}
		if (newlim < 0)
			return EINVAL;
		else if (newlim >= OFFTOBBT(RLIM_INFINITY))
			newlim = RLIM_INFINITY;
		else
			newlim = BBTOOFF(newlim);

		rlim.rlim_cur = newlim;
		rlim.rlim_max = newlim;

		CURVPROC_SETRLIMIT(RLIMIT_FSIZE, &rlim, error);
		if (error)
			return error;

		/* FALL THROUGH */

	case UL_GFILLIM:	/* Return current file size limit. */
	    {
		rlim_t limit;

		limit = getfsizelimit();
		if (!ABI_IS_IRIX5_64(abi)) {
			ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));
			if (limit >= RLIM32_INFINITY)
				rvp->r_off = OFFTOBBT(RLIM32_INFINITY);
			else
				rvp->r_off = OFFTOBBT(limit);
		} else {
			rvp->r_off = BTOBBT(limit);
		}
		break;
	    }

	case UL_GMEMLIM:	/* Return maximum possible break value. */
	{
		vasid_t vasid;
		as_getasattr_t asattr;

		as_lookup_current(&vasid);
		VAS_GETASATTR(vasid, AS_BRKMAX, &asattr);
		rvp->r_off = (off_t)asattr.as_brkmax;
		break;
	}
	case UL_GDESLIM:	/* Return configured value of NOFILES. */

		VPROC_GETRLIMIT(curvprocp, RLIMIT_NOFILE, &rlim);
		rvp->r_val1 = rlim.rlim_cur;
		break;

	default:
		return EINVAL;
	}
	return 0;
}

/*
 * BSD vhangup() routine
 *
 * Sends SIGHUP to the process group currently connected to the
 * controlling terminal of this process.  Invalidates the vnode
 * for the controlling terminal.
 */
int
vhangup()
{
	if (!_CAP_ABLE(CAP_DEVICE_MGT))
		return EPERM;
	CURVPROC_CTTY_HANGUP();
	return(0);
}


/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* 			BEGIN POSIX SIGNAL SYSTEM CALLS			     */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * note that POSIX does NOT prevent users from blocking SIGCONT--the signal
 * will cause the process to be started regardless, but if a handler was
 * installed it will NOT be called until the signal is unblocked.
 */


/*
 * return the set of signals currently pending to process in structure in
 * user space, pointed to by *set*.
 */
struct sigpendinga {
	sigset_t *set;
};

int
sigpending(struct sigpendinga *uap)
{
	uthread_t *ut = curuthread;
	k_sigset_t p_hold;
	sigvec_t *sigvp;
	sigset_t uset;
	k_sigset_t kset;
	int s;

	s = ut_lock(ut);
	p_hold = *ut->ut_sighold;

#if (_MIPS_SIM != _ABIO32)
	kset = ut->ut_sig;
#else
	sigemptyset(&kset);
	sigorset(&kset, &ut->ut_sig);
#endif
	ut_unlock(ut, s);

	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_PEEK, sigvp);
#if (_MIPS_SIM != _ABIO32)
	kset |= sigvp->sv_sig;
#else
	sigorset(&kset, &sigvp->sv_sig);
#endif
	sigandset(&kset, &p_hold);
	sigktou(&kset, &uset);
	if (copyout(&uset, uap->set, sizeof uset))
		return EFAULT;
	return 0;
}

/*
 * Replace the currently-active p_hold mask with the incoming one, and
 * sleep until an unmasked signal is delivered.  Returns with the original
 * mask reinstated.  (Note this is done in psig().)
 */

struct sigsuspenda {
	sigset_t *set;
};

int
sigsuspend(struct sigsuspenda *uap)
{
	uthread_t *ut = curuthread;
	int s;
	sigset_t uset;
	k_sigset_t kset;

	if (copyin(uap->set, &uset, sizeof uset))
		return EFAULT;
	sigutok(&uset, &kset);
	sigdiffset(&kset, &cantmask);	/* can't block KILL or STOP */


	s = ut_lock(ut);
	ut->ut_flags |= UT_SIGSUSPEND;
	ut->ut_suspmask = *ut->ut_sighold;
	*ut->ut_sighold = kset;

	/* Since we have changed our hold mask, we need to evaluate
	 * if a signal is pending that is now not held - otherwise
	 * we will sleep without seeing the signal.
	 */
	if (!thread_interrupted(UT_TO_KT(ut)) && sigisready()) {
		ut_unlock(ut, s);
		(void)issig(0, SIG_ALLSIGNALS);
		s = ut_lock(ut);
	}

	for (;;) {
		if (ut_sleepsig(ut, &(UT_TO_KT(ut))->k_timewait, 0, s) == -1)
			break;
		s = ut_lock(ut);
	}
	return EINTR;
}

static int
k_sigprocmask(int how,			/* SIG_BLOCK, UNBLOCK, or SETMASK */
	      k_sigset_t *newset,	/* new mask for signals */
	      k_sigset_t *oldset)	/* return old mask in this struct */
{
	uthread_t *ut = curuthread;
	int s;

	/* first check for invalid *how* parameter (only if newset != NULL) */
	if ( (newset != NULL) &&
	     (how != SIG_BLOCK) && (how != SIG_UNBLOCK) &&
	     (how != SIG_SETMASK) && (how != SIG_SETMASK32)) {
		return EINVAL;
	}

	if (oldset != NULL)	/* user wants old set returned */
		*oldset = *ut->ut_sighold;

	if (newset == NULL)	/* no changes, we're done */
		return 0;

	sigdiffset(newset,&cantmask); 	/* clear KILL and STOP */
	s = ut_lock(ut);

	switch(how) {
	case SIG_SETMASK:	/* setting 64 bits */
		*ut->ut_sighold = *newset;
		break;

	case SIG_SETMASK32:	/* setting 32 bits, used by BSD sigsetmask */
#if (_MIPS_SIM != _ABIO32)
		/*
		 * If upper 32 bits are set, they want to block everything.
		 */
		if ((*newset & 0xFFFFFFFF00000000LL) == 0xFFFFFFFF00000000LL) {
			*ut->ut_sighold = *newset;
		} else {
			/* 
			 * preserves the upper 32 signals from sigsetmask
			 * since it's not supposed to know about them
			 */
			*ut->ut_sighold =
				(*ut->ut_sighold & 0xFFFFFFFF00000000LL)
				      | (*newset & 0x00000000FFFFFFFFLL);
		}
#else
                if (newset->sigbits[1] == -1)
                        *ut->ut_sighold = *newset;
                else
			/*
			 * preserves the upper 32 signals from sigsetmask
			 * since it's not supposed to know about them
			 */
                        ut->ut_sighold->sigbits[0] = newset->sigbits[0];
#endif
		break;

	case SIG_BLOCK:
		sigorset(ut->ut_sighold,newset);
		break;

	case SIG_UNBLOCK:
		sigdiffset(ut->ut_sighold,newset);
		break;
	}
	ut_unlock(ut, s);
	return 0;
}

/*
 * Returns and alters signal block-mask for calling process.  If *oldset*
 * is not NULL, returns current mask in the structure it points to.  If
 * *newset* is not NULL, performs *how* action on the active mask.
 */
struct sigprocmaska {
	sysarg_t how;
	sigset_t *set;
	sigset_t *oset;
};

int
sigprocmask(struct sigprocmaska *uap)
{
	k_sigset_t kset, okset;
	sigset_t uset;
	int error;

	if (uap->set) {
		if (copyin(uap->set, &uset, sizeof uset))
			return EFAULT;
		sigutok(&uset, &kset);
		error = k_sigprocmask(uap->how, &kset, &okset);
	} else {
		error = k_sigprocmask(uap->how, NULL, &okset);
	}
	if (!error && uap->oset) {
		sigktou(&okset, &uset);
		if (copyout(&uset, uap->oset, sizeof uset))
			error = EFAULT;
	}
	return error;
}


/*
 * irix5_sigaction_utok - convert a irix5_sigaction_t to a k_sigaction_t
 */
static void
irix5_sigaction_utok(irix5_sigaction_t *uact, k_sigaction_t *kact)
{
	kact->k_sa_handler = (void (*)())(__psunsigned_t)uact->k_sa_handler;
	sigutok(&uact->sa_mask, &kact->sa_mask);
	kact->sa_flags = uact->sa_flags;
}

/*
 * irix5_sigaction_ktou - convert a k_sigaction_t to a irix5_sigaction_t
 */
static void
irix5_sigaction_ktou(k_sigaction_t *kact, irix5_sigaction_t *uact)
{
	uact->k_sa_handler = (app32_ptr_t)(__psunsigned_t)kact->k_sa_handler;
	sigktou(&kact->sa_mask, &uact->sa_mask);
	uact->sa_flags = kact->sa_flags;
}

#if _MIPS_SIM == _ABI64
/*
 * irix5_64_sigaction_utok - convert a irix5_64_sigaction_t to a k_sigaction_t
 */
static void
irix5_64_sigaction_utok(irix5_64_sigaction_t *uact, k_sigaction_t *kact)
{
	kact->k_sa_handler = (void (*)())uact->k_sa_handler;
	sigutok(&uact->sa_mask, &kact->sa_mask);
	kact->sa_flags = uact->sa_flags;
}

/*
 * irix5_64_sigaction_ktou - convert a k_sigaction_t to a irix5_64_sigaction_t
 */
static void
irix5_64_sigaction_ktou(k_sigaction_t *kact, irix5_64_sigaction_t *uact)
{
	uact->k_sa_handler = (app64_ptr_t)kact->k_sa_handler;
	sigktou(&kact->sa_mask, &uact->sa_mask);
	uact->sa_flags = kact->sa_flags;
}
#endif	/* _ABI64 */

/*
 * Alters the way *signal* is treated.  If *oldaction* is not NULL, old
 * treatment of the signal is returned in the struct pointed-to by it.
 * If *newaction* is not NULL, alters the handling of the signal as
 * the structure specifies.
 */
struct sigactiona {
	sysarg_t	sig;
	sigaction_t	*act;
	sigaction_t	*oact;
	int		(*sigtramp)();
};

int
sigaction(struct sigactiona *uap)
{
	irix5_sigaction_t i5_uact;
#if _MIPS_SIM == _ABI64
	irix5_64_sigaction_t i5_64_uact;
#endif
	k_sigaction_t kact, koact;
	int sig = uap->sig;
	/* REFERENCED */
	int abi = get_current_abi();

	if (uap->act != NULL) {  /* user wants to change treatment */

#if _MIPS_SIM == _ABI64
		if (abi == ABI_IRIX5_64) {
			if (copyin(uap->act, &i5_64_uact, sizeof i5_64_uact))
				return EFAULT;
			irix5_64_sigaction_utok(&i5_64_uact, &kact);
		} else
#endif
		{
			ASSERT(abi == ABI_IRIX5 || abi == ABI_IRIX5_N32);
			if (copyin(uap->act, &i5_uact, sizeof i5_uact))
				return EFAULT;
			irix5_sigaction_utok(&i5_uact, &kact);
		}
	}

	if (sig <= 0
	    || sig >= NSIG
	    || uap->act != NULL && sigismember(&cantmask, sig)
				&& kact.k_sa_handler != SIG_DFL) {
		return EINVAL;
	}

	if (uap->oact != NULL) {	/* user wants old treatment returned */

		CURVPROC_GET_SIGACT(sig, &koact);

#if _MIPS_SIM == _ABI64
		if (abi == ABI_IRIX5_64) {
			irix5_64_sigaction_ktou(&koact, &i5_64_uact);
			if (copyout(&i5_64_uact, uap->oact, sizeof i5_64_uact))
				return EFAULT;
		} else
#endif
		{
			ASSERT(abi == ABI_IRIX5 || abi == ABI_IRIX5_N32);
			irix5_sigaction_ktou(&koact, &i5_uact);
			if (copyout(&i5_uact, uap->oact, sizeof i5_uact))
				return EFAULT;
		}
	}

	if (uap->act != NULL) {  	/* user wants to change treatment */
		CURVPROC_SET_SIGACT(sig, kact.k_sa_handler, &kact.sa_mask,
					 kact.sa_flags, uap->sigtramp);
	}

	return 0;
}

static int irix5_to_sigstack(enum xlate_mode, void *, int, xlate_info_t *);
static int sigstack_to_irix5(void *, int, xlate_info_t *);

struct sigstacka {
	struct sigstack *nss;
	struct sigstack *oss;
};

int
sigstack(struct sigstacka *uap)
{
	struct sigstack nss;
	struct sigstack oss;
	proc_proxy_t	*prxy = curuthread->ut_pproxy;

	/* allow nss and oss to be same address */
	oss.ss_sp = prxy->prxy_sigsp;
	oss.ss_onstack = prxy->prxy_ssflags & SS_ONSTACK;
	if (uap->nss) {
		/* Xopen mandates that one cannot modify an active stack -
		 * the Xopen spec covers both sigstack and sigaltstack.
		 */
		if (prxy->prxy_ssflags & SS_ONSTACK)
			return EPERM;

		if (COPYIN_XLATE(uap->nss, &nss, sizeof nss,
				irix5_to_sigstack, prxy->prxy_abi, 1))
			return EFAULT;
		prxy->prxy_spsize = 0;	/* not applicable */
		if (nss.ss_onstack)
			prxy->prxy_ssflags |= SS_ONSTACK;
		else
			prxy->prxy_ssflags &= ~SS_ONSTACK;
		prxy->prxy_sigsp = nss.ss_sp;
		prxy->prxy_siglb = 0;	/* not applicable */
	}
	if (uap->oss) {
		if (XLATE_COPYOUT(&oss, uap->oss, sizeof oss,
				  sigstack_to_irix5, prxy->prxy_abi, 1))
			return EFAULT;
	}
	return 0;
}

/*
 * SVR4 sigaltstack syscall
 */
static int irix5_to_sigaltstack(enum xlate_mode, void *, int, xlate_info_t *);
static int sigaltstack_to_irix5(void *, int, xlate_info_t *);

struct sigaltstacka {
	struct sigaltstack *nss;
	struct sigaltstack *oss;
};

static int
k_sigaltstack(struct sigaltstacka *uap, int is_xpg4)
{
	stack_t uss;
	proc_proxy_t	*prxy = curuthread->ut_pproxy;

	/*
	 * User nss and oss can be the same addr. Copy in first before copyout.
	 */
	if (uap->nss) {
		if (prxy->prxy_ssflags & SS_ONSTACK)
			return EPERM;
		if (COPYIN_XLATE(uap->nss, &uss, sizeof uss,
				irix5_to_sigaltstack, prxy->prxy_abi, 1))
			return EFAULT;
		if (!(uss.ss_flags & SS_DISABLE) && uss.ss_size < MINSIGSTKSZ)
			return ENOMEM;
		/* Only allow the SS_DISABLE flag to be specified. */
		if (uss.ss_flags & ~SS_DISABLE)
			return EINVAL;
	}
	if (uap->oss) {
		stack_t kss, *kssp;

		if (is_xpg4) {
			kss.ss_sp = (char *)prxy->prxy_sigsp -
							prxy->prxy_spsize + 1;
			kss.ss_flags = prxy->prxy_ssflags;
			kss.ss_size = prxy->prxy_spsize;
			kssp = &kss;
		} else {
			kssp = &prxy->prxy_sigstack;
		}

		if (XLATE_COPYOUT(kssp, uap->oss, sizeof uss,
				  sigaltstack_to_irix5, prxy->prxy_abi, 1))
			return EFAULT;
	}
	if (uap->nss) {
		prxy->prxy_ssflags = uss.ss_flags;
		prxy->prxy_spsize = uss.ss_size;
		if (is_xpg4) {
			/* Xpg4 specifies the correct behavior - that one
			 * specifies a stack base and stack size, and the
			 * system worries about direction of growth.
			 * Irix implemented this incorrectly, forcing
			 * the user to deal with stack direction.
			 * NOTE: XPG4 requires that ss_sp value never
			 * point to ss_sp+ss_size but must be less than this.
			 */
			prxy->prxy_sigsp = (char *)uss.ss_sp + prxy->prxy_spsize - 1;
			prxy->prxy_siglb = uss.ss_sp;
		} else {
			prxy->prxy_sigsp = uss.ss_sp;
			prxy->prxy_siglb =
				(char *)prxy->prxy_sigsp - prxy->prxy_spsize;
		}
	}
	return 0;
}

int
xpg4_sigaltstack(struct sigaltstacka *uap)
{
	/* 2nd arg of 1 indicates xpg4 calling */
	return k_sigaltstack(uap, 1);
}

int
sigaltstack(struct sigaltstacka *uap)
{
	return k_sigaltstack(uap, 0);
}

/*
 * POSIX sysconf() sys call: returns the compile-time and lboot values
 * which are subject to change (and therefore can't be correctly returned
 * by the sysconf libc wrapper).
 */
int
sysconf(int name, rval_t *rvp)
{
	extern int maxup, ncargs;
	extern int maxsigq;
	extern int softpowerup_ok;

	switch(name) {
	case _SC_ARG_MAX:
		rvp->r_val1 = ncargs;
		break;
	case _SC_CHILD_MAX:
		rvp->r_val1 = maxup;
		break;
	case _SC_NGROUPS_MAX:
		rvp->r_val1 = ngroups_max;
		break;
	case _SC_OPEN_MAX:
	    {
		struct rlimit rlim;

		VPROC_GETRLIMIT(curvprocp, RLIMIT_NOFILE, &rlim);
		rvp->r_val1 = rlim.rlim_cur;
		break;
	    }
	case _SC_MQ_OPEN_MAX:
		rvp->r_val1 = pmqomax;
		break;
	case _SC_VERSION:
		rvp->r_val1 = _POSIX_VERSION;	/* param.h */
		break;
	case _SC_XOPEN_VERSION:
		rvp->r_val1 = _XOPEN_VERSION;	/* unistd.h */
		break;
	case _SC_CLK_TCK:
		rvp->r_val1 = HZ;		/* tunable some day? */
		break;
	case _SC_MMAP_FIXED_ALIGNMENT:
#if _VCE_AVOIDANCE
	/* VCE avoidance code only allows MAP_FIXED mmap calls on cache
	 * boundries.
	 */
	{
		extern int vce_avoidance;

		if (vce_avoidance) {
			extern int two_set_pcaches;
			extern int pdcache_size;

			rvp->r_val1 = two_set_pcaches ? two_set_pcaches :
							pdcache_size;
			break;
		}
	}
#endif
		/*FALLSTHRU*/
	case _SC_PAGESIZE:
		rvp->r_val1 = NBPP;
		break;
	case _SC_SIGQUEUE_MAX:
		rvp->r_val1 = maxsigq;		/* mtune/kernel */
		break;
	case _SC_ACL:
		rvp->r_val1 = acl_enabled;	/* Access Control Lists */
		break;
	case _SC_AUDIT:
		rvp->r_val1 = sat_enabled;	/* System Audit Trail */
		break;
	case _SC_INF:
		rvp->r_val1 = 0;		/* Information Labels */
		break;
	case _SC_MAC:
		rvp->r_val1 = mac_enabled;	/* Mandatory Access Control */
		break;
	case _SC_CAP:
		/* Least Privilege */
		rvp->r_val1 = cap_enabled;	/* Privilege "style" */
		break;
	case _SC_IP_SECOPTS:
		rvp->r_val1 = sesmgr_enabled;	/* IP Security Options */
		break;
	case _SC_KERN_POINTERS:
		rvp->r_val1 = sizeof(void *) * NBBY;
		break;
	case _SC_KERN_SIM:      /* Extended for the kernel (Subprogram Interface Model) */
                rvp->r_val1 = _MIPS_SIM;
                break;
	case _SC_SOFTPOWER:
		rvp->r_val1 = softpowerup_ok;
		break;
	case _SC_IOV_MAX:
		rvp->r_val1 = _MAX_IOVEC;
		break;
	case _SC_XBS5_LP64_OFF64:
	case _SC_XBS5_LPBIG_OFFBIG:
#if _MIPS_SIM == _ABI64
		rvp->r_val1 = 1;
#else
		rvp->r_val1 = -1;
#endif
		break;
	default:
		return EINVAL;
	}
	return 0;
}

#if _MIPS_SIM == _ABI64
static void
bcopy_irix5_to_prof(
	struct irix5_profila *i5_profp,
	struct prof *profp)
{
	profp->pr_base = (void *)(__psunsigned_t)i5_profp->pr_base;
	profp->pr_size = i5_profp->pr_size;
	profp->pr_off = i5_profp->pr_off;
	profp->pr_scale = i5_profp->pr_scale;
}

static int
irix5_to_prof(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register struct prof *profp;
	register struct irix5_prof *i5_profp;
	int i;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_prof) * count <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					    sizeof(struct irix5_prof) * count);
		info->copysize = sizeof(struct irix5_prof) * count;
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_prof) * count);
	ASSERT(info->copybuf != NULL);

	profp = to;
	i5_profp = info->copybuf;

	for (i = 0; i < count; i++, i5_profp++, profp++) {
		profp->pr_base = (void *)(__psunsigned_t)i5_profp->pr_base;
		profp->pr_size = i5_profp->pr_size;
		profp->pr_off = i5_profp->pr_off;
		profp->pr_scale = i5_profp->pr_scale;
	}

	return 0;
}

/* ARGSUSED */
static int
irix5_to_sigaltstack(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_sigaltstack, sigaltstack);

	target->ss_sp = (char *)(__psunsigned_t)source->ss_sp;
	target->ss_size = source->ss_size;
	target->ss_flags = source->ss_flags;

	return 0;
}

/* ARGSUSED */
static int
sigaltstack_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_sigaltstack *i5_sigaltstack;
	register struct sigaltstack *sigaltstack = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_sigaltstack) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_sigaltstack));
	info->copysize = sizeof(struct irix5_sigaltstack);
	i5_sigaltstack = info->copybuf;

	i5_sigaltstack->ss_sp = (__psunsigned_t)sigaltstack->ss_sp;
	i5_sigaltstack->ss_size = sigaltstack->ss_size;
	i5_sigaltstack->ss_flags = sigaltstack->ss_flags;

	return 0;

}


/* ARGSUSED */
static int
irix5_to_sigstack(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_sigstack, sigstack);

	target->ss_sp = (char *)(__psunsigned_t)source->ss_sp;
	target->ss_onstack = source->ss_onstack;

	return 0;
}

/* ARGSUSED */
static int
sigstack_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_sigstack *i5_sigstack;
	register struct sigstack *sigstack = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_sigstack) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_sigstack));
	info->copysize = sizeof(struct irix5_sigstack);
	i5_sigstack = info->copybuf;

	i5_sigstack->ss_sp = (__psunsigned_t)sigstack->ss_sp;
	i5_sigstack->ss_onstack = sigstack->ss_onstack;

	return 0;
}
#endif	/* _ABI64 */
