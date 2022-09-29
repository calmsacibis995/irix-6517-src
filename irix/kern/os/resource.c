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

#ident "$Revision: 1.44 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <ksys/vproc.h>
#include <sys/xlate.h>

/*
 * Resource controls and accounting.
 */
struct setrlimita {
	usysarg_t	which;
	struct		rlimit *lim;
};

int
setrlimit(struct setrlimita *uap)
{
	struct rlimit alim;
	struct irix5_rlimit i5_alim;

	if (uap->which >= RLIM_NLIMITS)
		return EINVAL;

	if (ABI_IS_IRIX5(get_current_abi())) {
		if (copyin(uap->lim, &i5_alim, sizeof(i5_alim)))
			return EFAULT;
		/*
		 * Translate between the user's and the kernel's idea
		 * of RLIM_INFINITY.
		 */
		if (i5_alim.rlim_cur > RLIM32_INFINITY ||
		    i5_alim.rlim_max > RLIM32_INFINITY)
			return EINVAL;
		alim.rlim_cur = RLIM32_CONV(i5_alim.rlim_cur);
		alim.rlim_max = RLIM32_CONV(i5_alim.rlim_max);
	} else {
		if (copyin(uap->lim, &alim, sizeof alim))
			return EFAULT;
	}

	return setrlimitcommon(uap->which, &alim);
}

struct setrlimit64a {
	usysarg_t	which;
	struct	rlimit64 *lim;
};

int
setrlimit64(struct setrlimit64a *uap)
{
	struct rlimit	alim;

	if (uap->which >= RLIM_NLIMITS)
		return EINVAL;

	if (copyin(uap->lim, &alim, sizeof alim))
		return EFAULT;

	return setrlimitcommon(uap->which, &alim);
}

int
setrlimitcommon(usysarg_t which, struct rlimit *alim)
{
	int error;

	if (((__int64_t) alim->rlim_cur < 0L) ||
            ((__int64_t) alim->rlim_max < 0L))
		return EINVAL;

	if (alim->rlim_cur > alim->rlim_max)
		return EINVAL;

	CURVPROC_SETRLIMIT((int)which, alim, error);

	return error;
}

rlim_t
getaslimit(uthread_t *ut, int which)
{
	vasid_t vasid;
	as_getasattr_t asattr;
	rlim_t rlim;

	if (ut == curuthread)
		as_lookup_current(&vasid);
	else {
		if (as_lookup(ut->ut_asid, &vasid))
			return (0);
		VAS_LOCK(vasid, AS_SHARED);
	}
	switch (which) {
	case RLIMIT_RSS:
		VAS_GETASATTR(vasid, AS_RSSMAX, &asattr);
		rlim = asattr.as_rssmax;
		break;
	case RLIMIT_STACK:
		VAS_GETASATTR(vasid, AS_STKMAX, &asattr);
		rlim = asattr.as_stkmax;
		break;
	case RLIMIT_VMEM:
		VAS_GETASATTR(vasid, AS_VMEMMAX, &asattr);
		rlim = asattr.as_vmemmax;
		break;
	case RLIMIT_DATA:
		VAS_GETASATTR(vasid, AS_DATAMAX, &asattr);
		rlim = asattr.as_datamax;
		break;
	default:
		ASSERT(0);
		rlim = 0;
		break;
	}
	if (ut != curuthread) {
		VAS_UNLOCK(vasid);
		as_rele(vasid);
	}
	return rlim;
}

struct getrlimita {
	usysarg_t	which;
	struct	rlimit *rlp;
};

int
getrlimit(struct getrlimita *uap)
{
	struct rlimit rlim;
	struct irix5_rlimit i5_rlim;

	if (uap->which >= RLIM_NLIMITS)
		return EINVAL;

	VPROC_GETRLIMIT(curvprocp, (int)uap->which, &rlim);

	if (ABI_IS_IRIX5(get_current_abi())) {
		/*
		 * Translate between the kernel's and the user's
		 * version of RLIM_INFINITY.  If the limit is not infinity
		 * and we can't fit in the entire number, return the
		 * maximum we can.
		 */
		if (rlim.rlim_cur == RLIM_INFINITY) {
			i5_rlim.rlim_cur = RLIM32_INFINITY;
		} else if (rlim.rlim_cur > UINT_MAX) {
			i5_rlim.rlim_cur = UINT_MAX;
		} else {
			i5_rlim.rlim_cur = rlim.rlim_cur;
		}
		if (rlim.rlim_max == RLIM_INFINITY) {
			i5_rlim.rlim_max = RLIM32_INFINITY;
		} else if (rlim.rlim_max > UINT_MAX) {
			i5_rlim.rlim_max = UINT_MAX;
		} else {
			i5_rlim.rlim_max = rlim.rlim_max;
		}
		if (copyout(&i5_rlim, uap->rlp, sizeof(i5_rlim))) {
			return EFAULT;
		}
	} else {
		if (copyout(&rlim, uap->rlp, sizeof(struct rlimit))) {
			return EFAULT;
		}
	}

	return 0;
}

struct getrlimit64a {
	usysarg_t	which;
	struct	rlimit64 *rlp;
};

int
getrlimit64(struct getrlimit64a *uap)
{
	struct rlimit rlim;

	if (uap->which >= RLIM_NLIMITS)
		return EINVAL;

	VPROC_GETRLIMIT(curvprocp, (int)uap->which, &rlim);

	if (copyout(&rlim, uap->rlp, sizeof(struct rlimit64)))
		return EFAULT;

	return 0;
}

rlim_t
getfsizelimit(void)
{
	struct rlimit rlim;

	ASSERT(curvprocp);
	VPROC_GETRLIMIT(curvprocp, RLIMIT_FSIZE, &rlim);

	return rlim.rlim_cur;
}

void
ruadd(register struct rusage *ru, register struct rusage *ru_from)
{
	register long *ip, *ipfrom;
	register int i;

	timevaladd(&ru->ru_utime, &ru_from->ru_utime);
        timevaladd(&ru->ru_stime, &ru_from->ru_stime);

	if (ru->ru_maxrss < ru_from->ru_maxrss)
		ru->ru_maxrss = ru_from->ru_maxrss;
	ip = &ru->ru_first; ipfrom = &ru_from->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i > 0; i--)
		*ip++ += *ipfrom++;
}
