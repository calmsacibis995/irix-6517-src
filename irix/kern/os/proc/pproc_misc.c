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

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/errno.h>
#include <sys/kaio.h>
#include <sys/sem.h>
#include "fs/procfs/prsystm.h"
#include "pproc.h"
#include "pproc_private.h"
#include <ksys/fdt.h>

struct kaiocbhd *
pproc_getkaiocbhd(
	bhv_desc_t	*bhv)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	return p->p_kaiocbhd;
}

struct kaiocbhd *
pproc_setkaiocbhd(
	bhv_desc_t	*bhv,
	struct kaiocbhd *new_kbhd)
{
	proc_t	*p;
	int s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);


	/* Must synchronize on setting p->p_kaiocbhd. Only one
	 * uthread can ever set it for the life of the process.
	 * Once it is set, it stays set until the proc exits.
	 */

	if (p->p_kaiocbhd != NULL)
		return p->p_kaiocbhd;

	/* XXX convert to proc handy lock when available */
	s = p_lock(p);
	if (p->p_kaiocbhd == NULL)
		p->p_kaiocbhd = new_kbhd;

	p_unlock(p, s);

	return p->p_kaiocbhd;
}

int
pproc_set_fpflags(
	bhv_desc_t	*bhv,
	u_char		fpflag,
	int		flags)
{
	proc_t	*p;
	int s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* Some of the flags defined are not set by this interface - 
	 * the HDR flags, or P_FP_FR.
	 */
#ifdef TFP
	ASSERT(fpflag == P_FP_IMPRECISE_EXCP || fpflag == P_FP_PRESERVE ||
		fpflag == P_FP_SMM || fpflag == P_FP_SPECULATIVE);
#else
	ASSERT(fpflag == P_FP_PRESERVE || fpflag == P_FP_SPECULATIVE);
#endif
		
	if ((flags & VFP_SINGLETHREADED) && p->p_proxy.prxy_nthreads > 1)
		return EINVAL;

	s = p_lock(p);

	if (flags & VFP_FLAG_ON)
		p->p_proxy.prxy_fp.pfp_fpflags |= fpflag;
	else
		p->p_proxy.prxy_fp.pfp_fpflags &= ~fpflag;
	p_unlock(p, s);

	return 0;
}

void
pproc_set_proxy(
	bhv_desc_t	*bhv,
	int		attribute,
	__psint_t	arg1,
	__psint_t	arg2)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	switch(attribute) {
	case VSETP_NOFPE:
		p->p_proxy.prxy_fp.pfp_nofpefrom = (caddr_t)arg1;
		p->p_proxy.prxy_fp.pfp_nofpeto = (caddr_t)arg2;
		break;

	case VSETP_EXCCNT:
		p->p_proxy.prxy_fp.pfp_dismissed_exc_cnt = (int)arg1;
		break;

	case VSETP_SPIPE:
		if (arg1)
			prxy_flagset(&p->p_proxy, PRXY_SPIPE);
		else
			prxy_flagclr(&p->p_proxy, PRXY_SPIPE);
		break;

	case VSETP_USERVME:
		prxy_flagset(&p->p_proxy, PRXY_USERVME);
		break;

	default:
		ASSERT(0);
	}
}

/* ARGSUSED */
int
pproc_fdt_dup(
	bhv_desc_t	*bhv,
	struct vfile	*vf,
	int		*newfp)
{
	int		error;

	ASSERT(BHV_TO_PROC(bhv)->p_pid == current_pid());
	error = fdt_dup(0, vf, newfp);
	return(error);
}

void
pproc_prnode(
	bhv_desc_t	*bhv,
        int             flag,
        struct vnode    **vpp)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

        *vpp = prbuild(p, flag);
}
