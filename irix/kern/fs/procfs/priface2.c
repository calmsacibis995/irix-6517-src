/*
 * priface2.c
 *
 * 	Functions for the procfs that uses the "copin_xlate" model
 *	rather than the "dual-compile" model of priface.c
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.80 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/cred.h>
#include <ksys/vsession.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/xlate.h>
#include <sys/ksignal.h>
#include <sys/kucontext.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/fpu.h>
#include <ksys/exception.h>
#include "procfs_n32.h"
#include "prdata.h"
#include "prsystm.h"
#include "procfs.h"
#include "priface.h"
#include "os/proc/pproc_private.h"

/* xlate functions */
static int irix5_to_prmap_sgi_arg(enum xlate_mode, void *, int, xlate_info_t *);
static int prmap_sgi_to_32(void *, int, xlate_info_t *);
static int prmap_sgi_to_prmap(void *, int, xlate_info_t *);

extern int prstatusio(proc_t *, void *, void *);

#if (_MIPS_SIM == _ABIO32 )

/*
 * Same functions as in priface.c, but o32 kernels need to get/set
 * 64 bit registers for n32 aware programs.
 */
void
prgetregs_irix5_64(uthread_t *ut, register irix5_64_greg_t *rp)
{
	exception_t *up = ut->ut_exception;
	register int i;

	*rp = 0;
	for (i = 1; i < 32; i++)
		*(rp+i) = (&up->u_eframe.ef_at)[i-1];
	*(rp+CTX_EPC) = up->u_eframe.ef_epc;
	*(rp+CTX_MDLO) = up->u_eframe.ef_mdlo;
	*(rp+CTX_MDHI) = up->u_eframe.ef_mdhi;
	*(rp+CTX_CAUSE) = up->u_eframe.ef_cause;
}

void
prsetregs_irix5_64(uthread_t *ut, register irix5_64_greg_t *rp)
{
	exception_t *up = ut->ut_exception;
	register k_machreg_t *urp;
	register int i;

	for (urp = &up->u_eframe.ef_at, i = 1; i < 32; i++, urp++) {
		*urp = *(rp+i);
	}

	up->u_eframe.ef_epc = *(rp+CTX_EPC);
	up->u_eframe.ef_mdlo = *(rp+CTX_MDLO);
	up->u_eframe.ef_mdhi = *(rp+CTX_MDHI);
	up->u_eframe.ef_cause = *(rp+CTX_CAUSE);
}

int
prgetfpregs_irix5_64(uthread_t *ut, irix5_64_fpregset_t *fp)
{
	exception_t *up = ut->ut_exception;
	register int i;
	register k_fpreg_t *k_fpreg;
	register irix5_64_fpreg_t *u_fpreg;

	/* check for system procs? up == NULL */

	u_fpreg = fp->fp_r.fp_regs;
	k_fpreg = up->u_pcb.pcb_fpregs;
	for (i = 0; i < 32; i++, k_fpreg++, u_fpreg++)
		*u_fpreg = *k_fpreg;

	fp->fp_csr = up->u_pcb.pcb_fpc_csr;
	return 0;
}

int
prsetfpregs_irix5_64(uthread_t *ut, irix5_64_fpregset_t *fp)
{
	exception_t *up = ut->ut_exception;
	register int i;
	register k_fpreg_t *k_fpreg;
	register irix5_64_fpreg_t *u_fpreg;

	/* check for system procs? up == NULL */

	checkfp(ut, 1);	/* Toss ownership of fp unit */
	u_fpreg = fp->fp_r.fp_regs;
	k_fpreg = up->u_pcb.pcb_fpregs;
	for (i = 0; i < 32; i++, k_fpreg++, u_fpreg++)
		*k_fpreg = *u_fpreg;

	up->u_pcb.pcb_fpc_csr = fp->fp_csr & ~FPCSR_EXCEPTIONS;
	return 0;
}

#endif /* _ABIO32 */

static void
fillinprinfo(exception_t *up, uthread_t *ut, struct siginfo *s)
{
	register pwatch_t *pw = ut->ut_watch;

	bzero(s, sizeof *s);
	if (ut->ut_whystop == PR_FAULTED) {
		if (ut->ut_whatstop == FLTWATCH ||
		    ut->ut_whatstop == FLTSCWATCH ||
		    ut->ut_whatstop == FLTKWATCH) {
			s->si_addr = pw->pw_curaddr;
			s->si_code = 0;
			if (pw && (pw->pw_curmode & W_WRITE))
				s->si_code |= MA_WRITE;
			if (pw && (pw->pw_curmode & W_READ))
				s->si_code |= MA_READ;
			if (pw && (pw->pw_curmode & W_EXEC))
				s->si_code |= MA_EXEC;
		} else {
			s->si_addr = (caddr_t)up->u_eframe.ef_badvaddr;
			s->si_code = ut->ut_code;
		}
	}
}

extern int syscall_has_64bit_arg(int, sysarg_t );
/*
 * copy syscall args from p_scallargs to pr_sysarg.
 * In o32/n32 kernels, 64 bit args from n32 processes are packed
 * in p_scallargs in a special way because p_scallargs is only 32 bits.
 * This function decodes that packing and puts everything in the
 * nice 64 bit pr_sysarg.
 * Returns the number of arguments for the syscall.
 */
static int
fillinprsysarg(u_int sysnum, __int64_t *pr_sysarg, uthread_t *ut)
{
	extern struct sysent sysent[];
	struct sysent *sy = &sysent[sysnum];
	int i;

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	int bitmask, si;

	if ((ABI_IS_IRIX5_N32(ut->ut_pproxy->prxy_abi)) &&
	    (sy->sy_flags & SY_64BIT_ARG) &&
	    (bitmask = syscall_has_64bit_arg((int)sysnum, ut->ut_scallargs[0])))
	{
	    for (i = 0, si = 0; i < sy->sy_narg; i++) {
		if (bitmask & (1 << i)) {
		    if (i & 1)
			si++;
		    pr_sysarg[i] = ((__uint64_t) ut->ut_scallargs[si++] << 32) |
				   ((__uint32_t) ut->ut_scallargs[si++]);
		} else {
			pr_sysarg[i] = ut->ut_scallargs[si++];
		}
	    }
	} else
#endif
		for (i = 0; i < sy->sy_narg; i++)
			pr_sysarg[i] = ut->ut_scallargs[i];

	return (sy->sy_narg);
}

/*
 * Return process status.
 */
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
int
prgetstatus(
	register uthread_t *ut, 
	int whole_proc, 
	register prstatus_t *sp)
#else
prgetstatus(
	register uthread_t *ut, 
	int whole_proc, 
	register irix5_n32_prstatus_t *sp)
#endif
{
	proc_t *p = UT_TO_PROC(ut);
	exception_t *up = ut->ut_exception;
	int s;
	u_int signo;
	timespec_t utime, stime;
        int accum_times;

	/*
	 * since isstopped() excludes job controlled stops!
	 */
	s = ut_lock(ut);
	accum_times = (whole_proc && (ut->ut_flags & UT_PTHREAD));
	if ((ut->ut_flags & UT_STOP) || isstopped(ut)) {
		sp->pr_flags = PR_STOPPED;
		if (ut->ut_flags & UT_STOP) {	/* can run in user space */
			sp->pr_flags |= PR_RETURN;
		}
		/*
		 * cannot hold siglck across dbprstatus -
		 * it can sleep - anyway, the process is
		 * stopped, so what can change??
		 */
		ut_unlock(ut, s);
		if (prstatusio(p, (void *)(&sp->pr_instr),
				(void *)(up->u_eframe.ef_epc))) {
			sp->pr_flags |= PR_PCINVAL;
			sp->pr_instr = 0;
		}
		s = ut_lock(ut);
		if (((ut->ut_whystop == 0) && (ut->ut_flags & UT_PRSTOPBITS)) ||
		    ((ut->ut_whystop) && (ut->ut_whystop != JOBCONTROL)))
			sp->pr_flags |= PR_ISTOP;
	} else {
		sp->pr_flags = PR_PCINVAL;
	}

	sp->pr_who = ut->ut_id;

	if (ut->ut_flags & UT_PRSTOPBITS)
		sp->pr_flags |= PR_DSTOP;
	if (p->p_flag & STRC)
		sp->pr_flags |= PR_PTRACE;
	if (ut->ut_flags & UT_STEP)
		sp->pr_flags |= PR_STEP;

	if (thread_interruptible(UT_TO_KT(ut)))
		sp->pr_flags |= PR_ASLEEP;

	if (p->p_dbgflags & KPRIFORK)
		sp->pr_flags |= PR_FORK;
	if (p->p_dbgflags & KPRRLC)
		sp->pr_flags |= PR_RLC;
	if (p->p_dbgflags & KPRKLC)
		sp->pr_flags |= PR_KLC;
	if (p->p_dbgflags & KPRJSTOP)
		sp->pr_flags |= PR_JOINTSTOP;
	if (p->p_dbgflags & KPRJPOLL)
		sp->pr_flags |= PR_JOINTPOLL;

	if (ut->ut_whystop == 0 && isstopped(ut) && 
	    (ut->ut_flags & UT_PRSTOPBITS))
		sp->pr_why = REQUESTED;
	else
		sp->pr_why = ut->ut_whystop;
	sp->pr_what = ut->ut_whatstop;

	/*
	 * heres a howdy hack - to maintain compatibility
	 * with /debug syscall numbers - we need to add 1
	 * to the numbers we get here
	 * exit (==1) is 0th bit in /proc and 1st bit in /debug
	 * So /proc needs to set EXIT = 2, which will be
	 * decremented in praddset to 1 and become the 1st bit
	 * just like /debug
	 */
	if (((ut->ut_whystop == PR_SYSENTRY) ||
	     (ut->ut_whystop == PR_SYSEXIT)  ||
	     (sp->pr_flags & PR_ASLEEP) ||
	     ((sp->pr_why == PR_FAULTED) && (sp->pr_what == FLTKWATCH))) &&
	     (ut->ut_syscallno != (u_int)-1 )) {
		sp->pr_syscall = ut->ut_syscallno;
		sp->pr_nsysarg = fillinprsysarg(ut->ut_syscallno,
						sp->pr_sysarg, ut);
		ASSERT(p->p_trmasks);
		sp->pr_errno = ut->ut_errno;
		sp->pr_rval1 = ut->ut_rval1;
		sp->pr_rval2 = ut->ut_rval2;

		sp->pr_syscall++;
		if ((ut->ut_whystop == PR_SYSENTRY) ||
		    (ut->ut_whystop == PR_SYSEXIT))
			sp->pr_what = sp->pr_syscall;
	} else {
		sp->pr_syscall = 0;
		sp->pr_errno = 0;
	}

	if (ut->ut_whystop == PR_SIGNALLED || ut->ut_whystop == PR_FAULTED)
#if (_MIPS_SIM == _ABI64)
		fillinprinfo(up, ut, &sp->pr_info);
#else
		fillinprinfo(up, ut, (struct siginfo *) &sp->pr_info);
#endif     
	signo = ut->ut_cursig;
	sp->pr_cursig = signo;
	/* XXX thread signals pr_thsigpend? or-in all ut_sig-s? */
	sigktou(&ut->ut_sig, &sp->pr_sigpend);
	sigktou(ut->ut_sighold, &sp->pr_sighold);

	/* setup pr_altstack */
	sp->pr_altstack.ss_flags = p->p_proxy.prxy_ssflags;
	sp->pr_altstack.ss_size = p->p_proxy.prxy_spsize;
	sp->pr_altstack.ss_sp = p->p_proxy.prxy_sigsp;

	if (signo) {	/* avoid using -1 (signo-1) as an index */
#if (_MIPS_SIM == _ABI64 /*OLSON??*/ || _MIPS_SIM == _ABIN32)
		sp->pr_action.sa_handler = p->p_sigvec.sv_hndlr[signo-1];
#else
		sp->pr_action.k_sa_handler =
			(app32_ptr_t)p->p_sigvec.sv_hndlr[signo-1];
#endif
		sigktou(&p->p_sigvec.sv_sigmasks[signo - 1],
			&sp->pr_action.sa_mask);
	}

	if ((signo == SIGCLD) && (p->p_sigvec.sv_flags & SNOCLDSTOP))
		sp->pr_action.sa_flags |= SA_NOCLDSTOP;
	else
		sp->pr_action.sa_flags &= ~SA_NOCLDSTOP;
	
	sp->pr_pid = p->p_pid;
	sp->pr_ppid = p->p_ppid;
	sp->pr_pgrp = p->p_pgid;
	sp->pr_sid = p->p_sid;
	
	TIMEVAL_TO_TIMESPEC(&p->p_cru.ru_utime, &sp->pr_cutime);
	TIMEVAL_TO_TIMESPEC(&p->p_cru.ru_stime, &sp->pr_cstime);
	
	sp->pr_clname[0] = 'T';
	sp->pr_clname[1] = 'S';
	sp->pr_clname[2] = 0;

#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	prgetregs(ut, sp->pr_reg);
#else
	prgetregs_irix5_64(ut, sp->pr_reg);	
#endif
	ut_unlock(ut, s);

	if (accum_times) {
		VPROC_READ_US_TIMERS(PROC_TO_VPROC(p), &utime, &stime);
        } else {
		/*
		 * This is now safe to read without ut_lock held.
		 */
		ktimer_read(UT_TO_KT(ut), AS_USR_RUN, &utime);
		ktimer_read(UT_TO_KT(ut), AS_SYS_RUN, &stime);
	}

	sp->pr_utime.tv_sec = utime.tv_sec;
	sp->pr_utime.tv_nsec = utime.tv_nsec;
	sp->pr_stime.tv_sec = stime.tv_sec;
	sp->pr_stime.tv_nsec = stime.tv_nsec;

	return 0;
}

/*
 * Fill an array of structures with extended memory map information.
 * Return the # of structures filled in to the caller or an error.
 */
int
prgetmap_sgi(caddr_t cmaddr, int *nmaps, int target_abi, vasid_t vasid)
{
#if (_MIPS_SIM == _ABI64)
	prmap_sgi_t pmp, *kbuf;		/* use the biggest map type */
	int kern_abi = ABI_IRIX5_64;
#elif (_MIPS_SIM == _ABIN32 || _MIPS_SIM == _ABIO32)
	irix5_n32_prmap_sgi_t pmp, *kbuf;	/* use the biggest map type */
	int kern_abi = ABI_IRIX5_N32;
#endif
	caddr_t ubuf;
	prmap_sgi_arg_t argbuf;
	int error = 0, mapsgisz, size, cnt;
	as_getattrin_t asin;
	as_getattr_t asres;

	if (ABI_IS_IRIX5_64(target_abi))
		mapsgisz = sizeof(prmap_sgi_t);
	else if (ABI_IS_IRIX5_N32(target_abi))
		mapsgisz = sizeof(irix5_n32_prmap_sgi_t);
	else
		mapsgisz = sizeof(irix5_prmap_sgi_t);

	/*
	 * Copy in user's buffer address and length
	 * Note that prmap_sgi_arg is the same for O32/N32
	 */
	if (copyin_xlate(cmaddr, (caddr_t)&argbuf, sizeof argbuf,
			 irix5_to_prmap_sgi_arg, target_abi, kern_abi, 1)) {
		return EFAULT;
	}
	ubuf = argbuf.pr_vaddr;

	/* Calculate max # of maps we can put in user's buffer */
	size = argbuf.pr_size / mapsgisz - 1;

	/* lets make this sane */
	if (size <= 0 || size > 5000)
		return EINVAL;

	/* allocate kernel buffer */
	kbuf = kmem_zalloc(size * sizeof(pmp), KM_SLEEP);

	asin.as_prattr.as_maxentries = size;
	asin.as_prattr.as_data = kbuf;

	if (error = VAS_GETATTR(vasid, AS_GET_PRATTR, &asin, &asres)) {
		kmem_free(kbuf, size * sizeof(pmp));
		return error;
	}
	cnt = asres.as_nentries;

	if (cnt && xlate_copyout(kbuf, ubuf, cnt * mapsgisz,
			  prmap_sgi_to_32, target_abi, kern_abi, cnt))
		error = EFAULT;

	kmem_free(kbuf, size * sizeof(pmp));

	/* Put in zero filled end marker */
	if (uzero(ubuf + (cnt * mapsgisz), mapsgisz))
		error = EFAULT;
	*nmaps = cnt;

	return error;
}

/*
 * Fill an array of structures with memory map information.
 * Return the # of structures filled in to the caller or an error.
 * Note that AS only gives extended info (prmap_sgi) so we must
 * translate
 * NOTE2: this interface is seriously busted since there is no
 * max length passed by the caller.
 */
int
prgetmap(caddr_t cmaddr, int target_abi, vasid_t vasid)
{
#if (_MIPS_SIM == _ABI64)
	prmap_sgi_t pmp, *kbuf;		/* use the biggest map type */
#elif (_MIPS_SIM == _ABIN32 || _MIPS_SIM == _ABIO32)
	irix5_n32_prmap_sgi_t pmp, *kbuf;	/* use the biggest map type */
#endif
	caddr_t ubuf;
	int error = 0, mapsgisz, size, cnt;
	as_getattrin_t asin;
	as_getattr_t asres;

	if (ABI_IS_IRIX5_64(target_abi))
		mapsgisz = sizeof(prmap_t);
	else if (ABI_IS_IRIX5_N32(target_abi))
		mapsgisz = sizeof(irix5_n32_prmap_t);
	else
		mapsgisz = sizeof(irix5_prmap_t);

	ubuf = cmaddr;

	/* Calculate max # of maps we can put in user's buffer */
	VAS_LOCK(vasid, AS_SHARED);
	size = prnsegs(vasid);
	VAS_UNLOCK(vasid);

	/* lets make this sane */
	if ((size > 5000) || (size <= 0))
		return EINVAL;

	/* allocate kernel buffer */
	kbuf = kmem_zalloc(size * sizeof(pmp), KM_SLEEP);

	asin.as_prattr.as_maxentries = size;
	asin.as_prattr.as_data = kbuf;

	if (error = VAS_GETATTR(vasid, AS_GET_PRATTR, &asin, &asres)) {
		kmem_free(kbuf, size * sizeof(pmp));
		return error;
	}
	cnt = asres.as_nentries;

	/*
	 * we now need to xlate and copyout. Since we have prmap_sgi_t's
	 * we have to xlate regardless of ABI
	 */
	if (cnt && xlate_copyout(kbuf, ubuf, cnt * mapsgisz,
			  prmap_sgi_to_prmap, target_abi, 0, cnt))
		error = EFAULT;

	kmem_free(kbuf, size * sizeof(pmp));

	/* Put in zero filled end marker */
	if (uzero(ubuf + (cnt * mapsgisz), mapsgisz))
		error = EFAULT;

	return error;
}

static void gregset_to_irix5(irix5_64_greg_t *, irix5_greg_t *);

#if (_MIPS_SIM == _ABI64)

/* ARGSUSED */
static int
prstatus_to_irix5_n32(
	void *from,
	int count,
	xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_PROLOGUE(prstatus, irix5_n32_prstatus);
	ASSERT(count == 1);

	bzero(target, sizeof(irix5_n32_prstatus_t));

	target->pr_flags = source->pr_flags;
	target->pr_why = source->pr_why;
	target->pr_what = source->pr_what;
	target->pr_cursig = source->pr_cursig;

	prassignset(&target->pr_sigpend, &source->pr_sigpend);
	prassignset(&target->pr_sighold, &source->pr_sighold);

	/* pr_info - siginfo structure */
	target->pr_info.si_addr =
			(app32_ptr_t)(__psint_t)source->pr_info.si_addr;
	target->pr_info.si_code = source->pr_info.si_code;

	/* pr_action - sigaction structure */
	target->pr_action.k_sa_handler =
			(app32_ptr_t)(__psint_t)source->pr_action.sa_handler;
	target->pr_action.sa_flags = source->pr_action.sa_flags;
	prassignset(&target->pr_action.sa_mask, &source->pr_action.sa_mask);

	target->pr_syscall = source->pr_syscall;
	target->pr_nsysarg = source->pr_nsysarg;
	target->pr_errno = source->pr_errno;
	target->pr_rval1 = source->pr_rval1;
	target->pr_rval2 = source->pr_rval2;

	for (i = 0; i < PRSYSARGS; i++)
		target->pr_sysarg[i] = source->pr_sysarg[i];

	target->pr_pid = source->pr_pid;
	target->pr_ppid = source->pr_ppid;
	target->pr_pgrp = source->pr_pgrp;
	target->pr_sid = source->pr_sid;
	TIMESPEC_TO_IRIX5(&source->pr_utime, &target->pr_utime);
	TIMESPEC_TO_IRIX5(&source->pr_stime, &target->pr_stime);
	TIMESPEC_TO_IRIX5(&source->pr_cutime, &target->pr_cutime);
	TIMESPEC_TO_IRIX5(&source->pr_cstime, &target->pr_cstime);
	bcopy(source->pr_clname, target->pr_clname, 8);
	target->pr_instr = source->pr_instr;

	prassignset(&target->pr_thsigpend, &source->pr_thsigpend);
	target->pr_who = source->pr_who;

	/* n32 uses the same register set as 64bit, so a bcopy works here */
	bcopy(source->pr_reg, target->pr_reg, sizeof(irix5_64_gregset_t));

	return 0;
}

/* ARGSUSED */
static int
prstatus_to_irix5(
	void *from,
	int count,
	xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_PROLOGUE(prstatus, irix5_prstatus);
	ASSERT(count == 1);

	bzero(target, sizeof(irix5_prstatus_t));

	target->pr_flags = source->pr_flags;
	target->pr_why = source->pr_why;
	target->pr_what = source->pr_what;
	target->pr_cursig = source->pr_cursig;

	prassignset(&target->pr_sigpend, &source->pr_sigpend);
	prassignset(&target->pr_sighold, &source->pr_sighold);

	/* pr_info - siginfo structure */
	target->pr_info.si_addr =
			(app32_ptr_t)(__psint_t)source->pr_info.si_addr;
	target->pr_info.si_code = source->pr_info.si_code;

	/* pr_action - sigaction structure */
	target->pr_action.k_sa_handler =
			(app32_ptr_t)(__psint_t)source->pr_action.sa_handler;
	target->pr_action.sa_flags = source->pr_action.sa_flags;
	prassignset(&target->pr_action.sa_mask, &source->pr_action.sa_mask);

	target->pr_syscall = source->pr_syscall;
	target->pr_nsysarg = source->pr_nsysarg;
	target->pr_errno = source->pr_errno;
	target->pr_rval1 = source->pr_rval1;
	target->pr_rval2 = source->pr_rval2;

	for (i = 0; i < PRSYSARGS; i++)
		target->pr_sysarg[i] = source->pr_sysarg[i];

	target->pr_pid = source->pr_pid;
	target->pr_ppid = source->pr_ppid;
	target->pr_pgrp = source->pr_pgrp;
	target->pr_sid = source->pr_sid;
	TIMESPEC_TO_IRIX5(&source->pr_utime, &target->pr_utime);
	TIMESPEC_TO_IRIX5(&source->pr_stime, &target->pr_stime);
	TIMESPEC_TO_IRIX5(&source->pr_cutime, &target->pr_cutime);
	TIMESPEC_TO_IRIX5(&source->pr_cstime, &target->pr_cstime);
	bcopy(source->pr_clname, target->pr_clname, 8);
	target->pr_instr = source->pr_instr;

	prassignset(&target->pr_thsigpend, &source->pr_thsigpend);
	target->pr_who = source->pr_who;

	/* translate register set to irix5 model */
	gregset_to_irix5(source->pr_reg, target->pr_reg);

	return 0;
}

int
prstatus_to_32(
	void *from,
	int count,
	xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));

	if (ABI_IS_IRIX5_N32(info->abi))
		return prstatus_to_irix5_n32(from, count, info);
	else
		return prstatus_to_irix5(from, count, info);
}

#elif (_MIPS_SIM == _ABIO32 || _MIPS_SIM == _ABIN32) /* OLSON: wrong */

static int
prstatus_to_irix5(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_PROLOGUE(irix5_n32_prstatus, irix5_prstatus);
	ASSERT(count == 1);

	bzero(target, sizeof(irix5_prstatus_t));

	target->pr_flags = source->pr_flags;
	target->pr_why = source->pr_why;
	target->pr_what = source->pr_what;
	target->pr_cursig = source->pr_cursig;

	prassignset(&target->pr_sigpend, &source->pr_sigpend);
	prassignset(&target->pr_sighold, &source->pr_sighold);

	/* pr_info - siginfo structure */
	target->pr_info.si_addr = source->pr_info.si_addr;
	target->pr_info.si_code = source->pr_info.si_code;

	/* pr_action - sigaction structure */
	target->pr_action.k_sa_handler = source->pr_action.k_sa_handler;
	target->pr_action.sa_flags = source->pr_action.sa_flags;
	prassignset(&target->pr_action.sa_mask, &source->pr_action.sa_mask);

	target->pr_syscall = source->pr_syscall;
	target->pr_nsysarg = source->pr_nsysarg;
	target->pr_errno = source->pr_errno;
	target->pr_rval1 = source->pr_rval1;
	target->pr_rval2 = source->pr_rval2;

	for (i = 0; i < PRSYSARGS; i++)
		target->pr_sysarg[i] = source->pr_sysarg[i];

	target->pr_pid = source->pr_pid;
	target->pr_ppid = source->pr_ppid;
	target->pr_pgrp = source->pr_pgrp;
	target->pr_sid = source->pr_sid;
	target->pr_utime = source->pr_utime;
	target->pr_stime = source->pr_stime;
	target->pr_cutime = source->pr_cutime;
	target->pr_cstime = source->pr_cstime;
	bcopy(source->pr_clname, target->pr_clname, 8);
	target->pr_instr = source->pr_instr;

	/* translate register set to irix5 model */
	gregset_to_irix5(source->pr_reg, target->pr_reg);

	return 0;
}

int
prstatus_to_32(	void *from, int count, xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5(info->abi));
	return prstatus_to_irix5(from, count, info);
}

#endif

static void
gregset_to_irix5(irix5_64_greg_t *source, irix5_greg_t *target)
{
	int i;

	for(i = 1; i < 32; i++)
		*(target+i) = *(source+i);

	*(target+CTX_EPC) = *(source+CTX_EPC);
	*(target+CTX_MDLO) = *(source+CTX_MDLO);
	*(target+CTX_MDHI) = *(source+CTX_MDHI);
	*(target+CTX_CAUSE) = *(source+CTX_CAUSE);
}

/* ARGSUSED */
int
irix5_to_prmap_sgi_arg(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prmap_sgi_arg, prmap_sgi_arg);

	target->pr_vaddr = (caddr_t)(__psunsigned_t)source->pr_vaddr;
	target->pr_size = source->pr_size;

	return 0;
}

#if (_MIPS_SIM == _ABI64)

/* ARGSUSED */
int
irix5_to_prrun(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prrun, prrun);

	target->pr_flags = source->pr_flags;
	prassignset(&target->pr_trace, &source->pr_trace);
	prassignset(&target->pr_sighold, &source->pr_sighold);
	prassignset(&target->pr_fault, &source->pr_fault);
	target->pr_vaddr = (caddr_t)(__psunsigned_t)source->pr_vaddr;

	return 0;
}

/* ARGSUSED */
int
irix5_to_prwatch(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prwatch, prwatch);

	target->pr_vaddr = (caddr_t)(__psunsigned_t) source->pr_vaddr;
	target->pr_size = source->pr_size;
	target->pr_wflags = source->pr_wflags;

	return 0;
}

/* ARGSUSED */
int
irix5_to_prthreadctl(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prthreadctl, prthreadctl);

	target->pt_cmd = source->pt_cmd;
	target->pt_tid = source->pt_tid;
	target->pt_flags = source->pt_flags;
	target->pt_data = (caddr_t)(__psunsigned_t) source->pt_data;
	return 0;
}

/*
 * convert prmap_sgi to 32 bit ABIs
 * Note that this is real painful for caller ABI != Kernel ABI
 * since we end up mallocing 2 buffers 
 */
static int
prmap_sgi_to_irix5(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(prmap_sgi, irix5_prmap_sgi,
					sizeof(irix5_prmap_sgi_t) * count);

	bzero(target, sizeof(irix5_prmap_sgi_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target->pr_vsize = (app32_ulong_t)source->pr_vsize;
		target->pr_psize = (app32_ulong_t)source->pr_psize;
		target->pr_wsize = (app32_ulong_t)source->pr_wsize;
		target->pr_rsize = (app32_ulong_t)source->pr_rsize;
		target->pr_msize = (app32_ulong_t)source->pr_msize;
		target->pr_dev = (app32_ulong_t)source->pr_dev;
		target->pr_ino = (app32_ulong_t)source->pr_ino;
		target->pr_lockcnt = source->pr_lockcnt;
		target++;
		source++;
	}
	return 0;
}

static int
prmap_sgi_to_irix5_n32(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(prmap_sgi, irix5_n32_prmap_sgi,
				sizeof(irix5_n32_prmap_sgi_t) * count);

	bzero(target, sizeof(irix5_n32_prmap_sgi_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target->pr_vsize = (app32_ulong_t)source->pr_vsize;
		target->pr_psize = (app32_ulong_t)source->pr_psize;
		target->pr_wsize = (app32_ulong_t)source->pr_wsize;
		target->pr_rsize = (app32_ulong_t)source->pr_rsize;
		target->pr_msize = (app32_ulong_t)source->pr_msize;
		target->pr_dev = (app32_ulong_t)source->pr_dev;
		target->pr_ino = (app32_ulong_t)source->pr_ino;
		target->pr_lockcnt = source->pr_lockcnt;
		target++;
		source++;
	}
	return 0;
}

int
prmap_sgi_to_32(void *from, int count, xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));

	if (ABI_IS_IRIX5_N32(info->abi))
		return prmap_sgi_to_irix5_n32(from, count, info);
	else
		return prmap_sgi_to_irix5(from, count, info);
}

/*
 * convert prmap_sgi to prmap for all ABIs
 */
static int
prmap_sgi_to_prmap_irix5(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(prmap_sgi, irix5_prmap,
					sizeof(irix5_prmap_t) * count);

	bzero(target, sizeof(irix5_prmap_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target++;
		source++;
	}
	return 0;
}

static int
prmap_sgi_to_prmap_irix5_n32(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(prmap_sgi, irix5_n32_prmap,
				sizeof(irix5_n32_prmap_t) * count);

	bzero(target, sizeof(irix5_n32_prmap_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target++;
		source++;
	}
	return 0;
}

static int
prmap_sgi_to_prmap_irix5_64(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(prmap_sgi, prmap,
				sizeof(prmap_t) * count);

	bzero(target, sizeof(prmap_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target++;
		source++;
	}
	return 0;
}

int
prmap_sgi_to_prmap(void *from, int count, xlate_info_t *info)
{
	if (ABI_IS_IRIX5_N32(info->abi))
		return prmap_sgi_to_prmap_irix5_n32(from, count, info);
	else if (ABI_IS_IRIX5(info->abi))
		return prmap_sgi_to_prmap_irix5(from, count, info);
	else
		return prmap_sgi_to_prmap_irix5_64(from, count, info);
}

#elif (_MIPS_SIM == _ABIO32 || _MIPS_SIM == _ABIN32)

/*
 * The buffer passed in is already type irix5_n32_prmap_sgi.
 * Note that we faked kernel_abi in prgetmap_sgi to be N32
 * so we should only get here if the target_abi is O32
 * These functions are identical for N32 and O32 kernels
 */
static int
prmap_sgi_to_irix5(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(irix5_n32_prmap_sgi, irix5_prmap_sgi,
					sizeof(irix5_prmap_sgi_t) * count);

	bzero(target, sizeof(irix5_prmap_sgi_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target->pr_vsize = (app32_ulong_t)source->pr_vsize;
		target->pr_psize = (app32_ulong_t)source->pr_psize;
		target->pr_wsize = (app32_ulong_t)source->pr_wsize;
		target->pr_rsize = (app32_ulong_t)source->pr_rsize;
		target->pr_msize = (app32_ulong_t)source->pr_msize;
		target->pr_dev = (app32_ulong_t)source->pr_dev;
		target->pr_ino = (app32_ulong_t)source->pr_ino;
		target->pr_lockcnt = source->pr_lockcnt;
		target++;
		source++;
	}
	return 0;
}

static int
prmap_sgi_to_32(
	void *from,
	int count,
	xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5(info->abi));

	return prmap_sgi_to_irix5(from, count, info);
}

/*
 * convert prmap_sgi to prmap for all ABIs
 * O32 and N32 kernels both have irix5_n32_prmap_sgi as the native type
 */
static int
prmap_sgi_to_prmap_irix5(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(irix5_n32_prmap_sgi, irix5_prmap,
					sizeof(irix5_prmap_t) * count);

	bzero(target, sizeof(irix5_prmap_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target++;
		source++;
	}
	return 0;
}

static int
prmap_sgi_to_prmap_irix5_n32(void *from, int count, xlate_info_t *info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE(irix5_n32_prmap_sgi, irix5_n32_prmap,
				sizeof(irix5_n32_prmap_t) * count);

	bzero(target, sizeof(irix5_n32_prmap_t) * count);
	for (i = 0; i < count; i++) {
		target->pr_vaddr = (__psint_t)source->pr_vaddr;
		target->pr_size = source->pr_size;
		target->pr_off = source->pr_off;
		target->pr_mflags = source->pr_mflags;
		target++;
		source++;
	}
	return 0;
}

int
prmap_sgi_to_prmap(void *from, int count, xlate_info_t *info)
{
	if (ABI_IS_IRIX5_N32(info->abi))
		return prmap_sgi_to_prmap_irix5_n32(from, count, info);
	else {
		ASSERT(ABI_IS_IRIX5(info->abi));
		return prmap_sgi_to_prmap_irix5(from, count, info);
	}
}

#endif /* _MIPS_SIM == _ABIO32 || _MIPS_SIM == _ABIN32 */
