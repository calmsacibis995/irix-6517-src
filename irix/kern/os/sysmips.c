/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 *	Mips specific system calls
 */

#ident	"$Revision: 3.67 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/sat.h>
#include <sys/systeminfo.h>
#include <sys/sysmips.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/vnode.h>
#include <sys/runq.h>
#include <sys/ktime.h>
#include <sys/pwioctl.h>
#include <sys/lio.h>
#include <sys/capability.h>
#include <sys/pda.h>
#include <ksys/vproc.h>

struct systeminfoa;
extern int systeminfo(struct systeminfoa *, rval_t *);

extern int maxcpus;
extern int icache_size, dcache_size;

struct sysmipsa {
	sysarg_t	cmd;
	sysarg_t	arg1, arg2, arg3;
};

extern int klistio32(unsigned int nents, parg_t *physioarg, int arg, rval_t *rvp);
extern int klistio64(unsigned int nents, parg_t *physioarg, int arg, rval_t *rvp);

int
sysmips(struct sysmipsa *uap, rval_t *rvp)
{

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	switch (uap->cmd) {

	case SETNAME:	/* rename the system */
		uap->cmd = SI_SET_HOSTNAME;
		return systeminfo((struct systeminfoa *)uap, rvp);
 
	case STIME:	/* set internal time, not hardware clock */
	{
		cpu_cookie_t was_running;

		if (!_CAP_ABLE(CAP_TIME_MGT))
			return EPERM;
		was_running = setmustrun(clock_processor);
		settime(uap->arg1, 0);
		restoremustrun(was_running);
		_SAT_CLOCK(uap->arg1,0);
		break;
	}

	case FLUSH_CACHE:	/* flush all of both caches */
		cache_operation((caddr_t)K0SEG, icache_size, CACH_ICACHE|CACH_NOADDR);
		cache_operation((caddr_t)K0SEG, dcache_size, CACH_DCACHE|CACH_NOADDR);
		break;
	
	case MIPS_FPSIGINTR:
		/* This is complete nonsense for multi-threaded apps */
		if (curuthread->ut_pproxy->prxy_shmask & PR_THREADS)
			return EINVAL;
		curuthread->ut_pproxy->prxy_fp.pfp_fp = uap->arg1;
		break;

	case MIPS_FIXADE:
		if (uap->arg1) {
			VPROC_FLAG_THREADS(curvprocp, UT_FIXADE,
							UTHREAD_FLAG_ON);
		} else {
			VPROC_FLAG_THREADS(curvprocp, UT_FIXADE,
							UTHREAD_FLAG_OFF);
		}
		break;
	
	case POSTWAIT:
		return pwsysmips(uap->arg1, uap->arg2, uap->arg3, rvp);

	case PPHYSIO:
		return klistio32(uap->arg1, (parg_t *)uap->arg2, uap->arg3, rvp);

	case PPHYSIO64:
		return klistio64(uap->arg1, (parg_t *)uap->arg2, uap->arg3, rvp);

	default:
		return EINVAL;
	}
	return 0;
}
