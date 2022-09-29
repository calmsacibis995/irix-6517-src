/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.142 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <sys/fpu.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/sema_private.h>
#include <sys/runq.h>
#include <ksys/vsession.h>
#include <sys/sysmacros.h>
#include <sys/schedctl.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#include <string.h>
#include <values.h>
#include "prdata.h"
#include "procfs.h"
#include "priface.h"
#include "os/proc/pproc_private.h"
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /* _SHAREII */

#if _INTERFACE_NATIVE
/* Structure wrappers */
#define PRIFACE_greg_t		greg_t
#define PRIFACE_gregset_t	gregset_t
#define PRIFACE_fpreg_t		fpreg_t
#define PRIFACE_fpregset_t	fpregset_t
#define PRIFACE_siginfo		siginfo
#define PRIFACE_prpsinfo_t	prpsinfo_t
#define PRIFACE_sigaction_t	sigaction_t
#define PRIFACE_prpgd_sgi_t	prpgd_sgi_t
#define PRIFACE_prwatch_t	prwatch_t
#define PRIFACE_prusage_t	prusage_t
#define PRIFACE_timespec_t	timespec_t

/* Structure member wrappers */
#define PRIFACE_sa_handler	sa_handler

/* Function wrappers */
#define PRIFACE_prgetregs	prgetregs
#define PRIFACE_prsetregs	prsetregs
#define PRIFACE_prgetfpregs	prgetfpregs
#define PRIFACE_prsetfpregs	prsetfpregs
#define PRIFACE_prgetpsinfo	prgetpsinfo
#define PRIFACE_prgetaction	prgetaction
#define PRIFACE_prgetpgd_sgi	prgetpgd_sgi
#define PRIFACE_watchcopy	watchcopy
#define PRIFACE_prgetusage	prgetusage
#define PRIFACE_prgetptimer	prgetptimer

/* Oddball stuff */
#define PRIFACE_app_ptr(ptr)	(ptr)

#ifdef _SHAREII
#  define SHR_PRIFACE_PRGETPSINFO SHR__PRGETPSINFO
#endif /* _SHAREII */

#endif	/* _INTERFACE_NATIVE */

#if ((_MIPS_SIM == _ABI64 ||  _MIPS_SIM == _ABIN32) && _INTERFACE_IRIX5)
/* Structure wrappers */
#define PRIFACE_greg_t		irix5_greg_t
#define PRIFACE_gregset_t	irix5_gregset_t
#define PRIFACE_fpreg_t		irix5_fpreg_t
#define PRIFACE_fpregset_t	irix5_fpregset_t
#define PRIFACE_siginfo		irix5_siginfo
#define PRIFACE_prpsinfo_t	irix5_prpsinfo_t
#define PRIFACE_sigaction_t	irix5_sigaction_t
#define PRIFACE_prpgd_sgi_t	irix5_prpgd_sgi_t
#define PRIFACE_prwatch_t	irix5_prwatch_t
#define PRIFACE_prusage_t	irix5_prusage_t
#define PRIFACE_timespec_t	irix5_timespec_t

/* Structure member wrappers */
#define PRIFACE_sa_handler	k_sa_handler

/* Function wrappers */
#define PRIFACE_prgetregs	irix5_prgetregs
#define PRIFACE_prsetregs	irix5_prsetregs
#define PRIFACE_prgetfpregs	irix5_prgetfpregs
#define PRIFACE_prsetfpregs	irix5_prsetfpregs
#define PRIFACE_prgetpsinfo	irix5_prgetpsinfo
#define PRIFACE_prgetaction	irix5_prgetaction
#define PRIFACE_prgetpgd_sgi	irix5_prgetpgd_sgi
#define PRIFACE_watchcopy	irix5_watchcopy
#define PRIFACE_prgetusage	irix5_prgetusage
#define PRIFACE_prgetptimer	irix5_prgetptimer


#ifdef _SHAREII
#  define SHR_PRIFACE_PRGETPSINFO SHR_irix5_PRGETPSINFO
#endif /* _SHAREII */

/* Oddball stuff */
#define PRIFACE_app_ptr(ptr)	(app32_ptr_t)(__psint_t)(ptr)

#endif

/*
 * The rest of this  file gets compiled twice for _ABI64 and _ABIN32; once
 * with _INTERFACE_NATIVE and once with _INTERFACE_IRIX5.
 * Otherwise, the contents are ignored except when _INTERFACE_NATIVE
 * is defined.
 */
#if ((_MIPS_SIM == _ABI64 ||  _MIPS_SIM == _ABIN32) || _INTERFACE_NATIVE)

/*
 * Return general registers.  The exception block must already have been mapped
 * in by the caller.
 */
void
PRIFACE_prgetregs(uthread_t *ut, register PRIFACE_greg_t *rp)
{
	exception_t	*up = ut->ut_exception;
	int		i;

	*rp = 0;
	for (i = 1; i < 32; i++)
		*(rp+i) = (&up->u_eframe.ef_at)[i-1];
	*(rp+CTX_EPC) = up->u_eframe.ef_epc;
	*(rp+CTX_MDLO) = up->u_eframe.ef_mdlo;
	*(rp+CTX_MDHI) = up->u_eframe.ef_mdhi;
	*(rp+CTX_CAUSE) = up->u_eframe.ef_cause;
}

/*
 * Set general registers.  The exception block must already have been mapped
 * in by the caller.
 */
void
PRIFACE_prsetregs(uthread_t *ut, register PRIFACE_greg_t *rp)
{
	exception_t	*up = ut->ut_exception;
	register k_machreg_t *urp;
	register int i;

	/* Copy back gen registers (note: must sign extend in o32 mode!) */
	for (urp = &up->u_eframe.ef_at, i = 1; i < 32; i++, urp++) {
#if _MIPS_SIM == _ABIO32 || defined(INTERFACE_IRIX5)
		*urp = (signed int) *(rp+i);
#else
		*urp = *(rp+i);
#endif
	}

	/* but don't sign extend these in o32 or we go boom...! */
	up->u_eframe.ef_epc = *(rp+CTX_EPC);
	up->u_eframe.ef_mdlo = *(rp+CTX_MDLO);
	up->u_eframe.ef_mdhi = *(rp+CTX_MDHI);
	up->u_eframe.ef_cause = *(rp+CTX_CAUSE);
}

/*
 * Get floating-point registers.  The exception block is mapped in here (not by
 * the caller).
 */
int
PRIFACE_prgetfpregs(uthread_t *ut, register PRIFACE_fpregset_t *fp)
{
	exception_t *up = ut->ut_exception;
	register int i;
	register k_fpreg_t *k_fpreg;
	register PRIFACE_fpreg_t *u_fpreg;

	u_fpreg = fp->fp_r.fp_regs;
	k_fpreg = up->u_pcb.pcb_fpregs;
	for (i = 0; i < 32; i++, k_fpreg++, u_fpreg++)
		*u_fpreg = *k_fpreg;

	fp->fp_csr = up->u_pcb.pcb_fpc_csr;
	return 0;
}

/*
 * Set floating-point registers.  The exception block is mapped in here (not by
 * the caller).
 */
int
PRIFACE_prsetfpregs(uthread_t *ut, register PRIFACE_fpregset_t *fp)
{
	exception_t *up = ut->ut_exception;
	register int i;
	register k_fpreg_t *k_fpreg;
	register PRIFACE_fpreg_t *u_fpreg;

	checkfp(ut, 1);		/* Toss ownership of fp unit */
	u_fpreg = fp->fp_r.fp_regs;
	k_fpreg = up->u_pcb.pcb_fpregs;
	for (i = 0; i < 32; i++, k_fpreg++, u_fpreg++)
		*k_fpreg = *u_fpreg;

	up->u_pcb.pcb_fpc_csr = fp->fp_csr & ~FPCSR_EXCEPTIONS;
	return 0;
}

/*
 * Return information used by ps(1).
 * Note - be very careful of exitting processes since they can still
 * be running through exit while we are here.
 * We need prnode because zombies may not have uthreads.
 */
int
PRIFACE_prgetpsinfo(uthread_t *ut, struct prnode *pnp, int whole_proc,
		    PRIFACE_prpsinfo_t *psp)
{
	register char c, state;
	proc_t *p = pnp->pr_proc;
	cred_t *cr;
	dev_t d;
	timespec_t utime, stime;
	auto pgcnt_t size, rss;

	ASSERT(whole_proc || ut);
	ASSERT(p->p_stat);
	state = p->p_stat;

	bzero((caddr_t)psp, sizeof(PRIFACE_prpsinfo_t));
	switch (state) {
	case SRUN:
		ASSERT(ut);
		if (ut->ut_pproxy->prxy_coredump_vp) 
			c = 'C';
		else if (UT_TO_KT(ut)->k_flags & KT_SLEEP) {
			if (ut->ut_flags & UT_SXBRK)
				c = 'X';
			else
				c = 'S';
		} else if (ut->ut_flags & UT_STOP) {
			c = 'T';
		} else
			c = 'R';
		break;

	case SZOMB:
		c = 'Z';
		break;
	default:
		c = '?';
		break;
	}

	psp->pr_state = state;
	psp->pr_sname = c;
	psp->pr_zomb = (state == SZOMB);
	psp->pr_flag = p->p_flag & PR_FLAG_PROC;
	cr = pcred_access(p);
	psp->pr_uid = cr->cr_ruid;
	psp->pr_gid = cr->cr_rgid;
	pcred_unaccess(p);
	psp->pr_pid = p->p_pid;
	psp->pr_spid = p->p_pid;	/* spid is defunct */
	psp->pr_thds = p->p_proxy.prxy_nthreads;
	psp->pr_ppid = p->p_ppid;
	psp->pr_pgrp = p->p_pgid;

	psp->pr_addr = 0;
	if (whole_proc) {
		VPROC_READ_US_TIMERS(PROC_TO_VPROC(p), &utime, &stime);
	} else {
		ktimer_read(UT_TO_KT(ut), AS_USR_RUN, &utime);
		ktimer_read(UT_TO_KT(ut), AS_SYS_RUN, &stime);
	}
	timespec_add(&stime, &utime);
	psp->pr_time.tv_sec = stime.tv_sec;
	psp->pr_time.tv_nsec = stime.tv_nsec;

	if (state != SRUN) {
		psp->pr_sonproc = CPU_NONE;
		psp->pr_ttydev = PRNODEV;
	} else {
		psp->pr_sid = p->p_sid;

		ASSERT(ut);
#define NODEINFO
#if defined(NODEINFO) && defined(SN)
		/*
		 * Temporary hack for returning the affinity node & lastrun node.
		 * This is a completely undocumented & experimental interface -
		 * no released code uses this at this time. The purpose of
		 * the code is to simplify monitoring whether processes are actually
		 * running on their assigned nodes.
		 */
		{
			int nodeinfo, cpu;
			nodeinfo = ((UT_TO_KT(ut)->k_affnode)&0xffff) << 16;
			cpu = UT_TO_KT(ut)->k_lastrun;
			if (cpu != CPU_NONE && pdaindr[cpu].pda)
				nodeinfo |= cputocnode(cpu);
			else
				nodeinfo |= 0xffff;
			psp->pr_spare1 = nodeinfo;
		}
#endif /* NODEINFO */
		as_getassize(ut->ut_asid, &size, &rss);

		psp->pr_sonproc = UT_TO_KT(ut)->k_sonproc;
		psp->pr_size = size;
		psp->pr_rssize = rss;

		psp->pr_wchan = PRIFACE_app_ptr(UT_TO_KT(ut)->k_wchan);
		if (psp->pr_wchan)
			strncpy(psp->pr_wname,
				wchanname(UT_TO_KT(ut)->k_wchan,
					  UT_TO_KT(ut)->k_flags),
				sizeof psp->pr_wname);
		psp->pr_nice = 0;		/* default */
		if ((psp->pr_oldpri = kt_pri(UT_TO_KT(ut))) < 0)
			psp->pr_oldpri = p->p_proxy.prxy_sched.prs_nice;

		if (IS_SPROC(&p->p_proxy) && p->p_shaddr->s_sched != SGS_FREE) {
			strcpy(psp->pr_clname, "GN");
		} else if (is_weightless(UT_TO_KT(ut))) {
			strcpy(psp->pr_clname, "WL");
		} else if (KT_ISBASEPS(UT_TO_KT(ut))) {
			strcpy(psp->pr_clname, "RT");
		} else if (is_batch(UT_TO_KT(ut))) {
			strcpy(psp->pr_clname, "B");
		} else if (is_bcritical(UT_TO_KT(ut))) {
			strcpy(psp->pr_clname, "BC");
		} else {
			strcpy(psp->pr_clname, "TS");
			psp->pr_nice = 2*NZERO - p->p_proxy.prxy_sched.prs_nice;
		}
		psp->pr_pri = psp->pr_oldpri;	/* XXX for now */
		psp->pr_start.tv_sec = p->p_start;
		psp->pr_start.tv_nsec = 0L;
		psp->pr_ttydev = ((d = cttydev(p)) == NODEV) ? PRNODEV : d;

		bcopy(p->p_comm, psp->pr_fname,
			min(sizeof(p->p_comm), sizeof(psp->pr_fname)-1));
		bcopy(p->p_psargs, psp->pr_psargs, sizeof(psp->pr_psargs) - 1);
		TIMEVAL_TO_TIMESPEC(&p->p_cru.ru_utime, &utime);
		TIMEVAL_TO_TIMESPEC(&p->p_cru.ru_stime, &stime);
		timespec_add((timespec_t *)&stime, (timespec_t *)&utime);
		psp->pr_ctime.tv_sec = stime.tv_sec;
		psp->pr_ctime.tv_nsec = stime.tv_nsec;
	}
#ifdef _SHAREII
	/*
	 * Under Share, extra information is needed by ps, for example, 
	 * a process can have a different uid from the lnode it 
	 * is attached to.  This call asks Share to fill in the extra info.
	 */
	SHR_PRIFACE_PRGETPSINFO(p, psp);
#endif /* _SHAREII */
	
	return 0;
}

/*
 * Get the sigaction structure for the specified signal.
 */
void
PRIFACE_prgetaction(
	register proc_t *p,
	register u_int sig,
	register PRIFACE_sigaction_t *sp)
{
	sp->PRIFACE_sa_handler = PRIFACE_app_ptr(SIG_DFL);
	premptyset(&sp->sa_mask);
	sp->sa_flags = 0;
	if (sig > 0 && sig < NSIG) {

		sigvec_acclock(&p->p_sigvec);
		
		sp->PRIFACE_sa_handler =
			PRIFACE_app_ptr(p->p_sigvec.sv_hndlr[sig-1]);
		sigktou(&p->p_sigvec.sv_sigmasks[sig-1], &sp->sa_mask);
		if (sigismember(&p->p_proxy.prxy_sigonstack, sig))
			sp->sa_flags |= SA_ONSTACK;
		if (sigismember(&p->p_sigvec.sv_sigresethand, sig))
			sp->sa_flags |= SA_RESETHAND;
		if (sigismember(&p->p_sigvec.sv_sigrestart, sig))
			sp->sa_flags |= SA_RESTART;
		if (sigismember(&p->p_sigvec.sv_sainfo, sig))
			sp->sa_flags |= SA_SIGINFO;
		if (sigismember(&p->p_sigvec.sv_signodefer, sig))
			sp->sa_flags |= SA_NODEFER;
		if (sig == SIGCLD) {
			if (p->p_sigvec.sv_flags & SNOWAIT)
				sp->sa_flags |= SA_NOCLDWAIT;
			if (p->p_sigvec.sv_flags & SNOCLDSTOP)
				sp->sa_flags |= SA_NOCLDSTOP;
		}

		sigvec_unlock(&p->p_sigvec);
	}
}

/*
 * Fill an array of structures with page table information.  The array
 * has already been zero-filled by the caller.
 * NB: caller must acquire and release aspacelock()
 */
PRIFACE_prgetpgd_sgi(
	caddr_t cmaddr,
	vasid_t vasid,
	PRIFACE_prpgd_sgi_t *prpgdp)
{
	pgcnt_t size, csize;
	PRIFACE_prpgd_sgi_t prhdr;
	uvaddr_t vaddr;
	int error;
	as_getrangeattrin_t asin;
	as_getrangeattr_t asres;
	as_getvaddrattr_t attrres;

	/* Copy in header */
	if (copyin(cmaddr, (caddr_t)&prhdr, sizeof prhdr))
		return -EFAULT;

	vaddr = (uvaddr_t)(__psunsigned_t)prhdr.pr_vaddr;

	if (error = VAS_GETVADDRATTR(vasid, AS_PINFO, vaddr, &attrres))
		return -error;

	/* Number of pages */
	size = btoc(attrres.as_pinfo.as_len - (vaddr - attrres.as_pinfo.as_base));
	if (size > prhdr.pr_pglen)
		size = prhdr.pr_pglen;

	/* Calculate number of bytes needed */
	csize = sizeof prhdr + (size - 1) * sizeof (pgd_t);
	if (prpgdp == NULL)
		return (csize);

	/* Update page data array */
	if (copyin(cmaddr, (caddr_t)prpgdp, csize))
		return -EFAULT;

	prpgdp->pr_vaddr = PRIFACE_app_ptr((caddr_t)ctob(btoct(vaddr)));
	prpgdp->pr_pglen = size;

	asin.as_pgd_data = prpgdp->pr_data;

	if ((error = VAS_GETRANGEATTR(vasid, vaddr, ctob(size), AS_GET_PGD, &asin, &asres)))
		return -error;

	if (asres.as_pgd_flush)
		tlbsync(0LL, allclr_cpumask, FLUSH_MAPPINGS);

	return 0;
}

/*
 * Copyout one watchpoint struct.
 */
int
PRIFACE_watchcopy(
	caddr_t vaddr,
	ulong length,
	ulong mode,
	caddr_t *targvaddr)
{
	PRIFACE_prwatch_t wp;

	wp.pr_vaddr = PRIFACE_app_ptr(vaddr);
	wp.pr_size = length;
	wp.pr_wflags = 0;
	if (mode & W_WRITE)
		wp.pr_wflags |= MA_WRITE;
	if (mode & W_READ)
		wp.pr_wflags |= MA_READ;
	if (mode & W_EXEC)
		wp.pr_wflags |= MA_EXEC;
	if (copyout((caddr_t) &wp, *targvaddr, sizeof(wp))) {
		return(EFAULT);
	}
	*targvaddr += sizeof(wp);
	return(0);
}

void
PRIFACE_prgetusage(
	register proc_t *p,
	register PRIFACE_prusage_t *prusage)
{
	timespec_t utime, stime;
	auto pgcnt_t size, rss;
	struct rusage ru;
	proc_acct_t acct;
	vproc_t *vp;

	vp = PROC_TO_VPROC(p);

	/* fill in prusage structure */
	nanotime(&utime);
	prusage->pu_tstamp.tv_sec = utime.tv_sec;
	prusage->pu_tstamp.tv_nsec = utime.tv_nsec;
	prusage->pu_starttime.tv_sec = p->p_start;

	VPROC_READ_US_TIMERS(vp, &utime, &stime);
	prusage->pu_utime.tv_sec = utime.tv_sec;
	prusage->pu_utime.tv_nsec = utime.tv_nsec;
	prusage->pu_stime.tv_sec = stime.tv_sec;
	prusage->pu_stime.tv_nsec = stime.tv_nsec;

	/* prgetusage is called in a path where the process may have
	 * been prlocked with the ZYES flag, hence the prxy_threads
	 * field may be invalid.
	 */
	if (p->p_stat == SZOMB)
		size = 0;
	else
		as_getassize(prxy_to_thread(&p->p_proxy)->ut_asid, &size, &rss);

	prusage->pu_size = size;
	prusage->pu_rss = rss;

	VPROC_GETRUSAGE(vp, VRUSAGE_SELF, &ru);

	prusage->pu_minf = ru.ru_minflt;
	prusage->pu_majf = ru.ru_majflt;
	prusage->pu_nswap = ru.ru_nswap;
	prusage->pu_sigs = ru.ru_nsignals;
	prusage->pu_vctx = ru.ru_nvcsw;
	prusage->pu_ictx =  ru.ru_nivcsw;
	prusage->pu_inblock = ru.ru_inblock;
	prusage->pu_oublock = ru.ru_oublock;

	VPROC_GET_EXTACCT(vp, &acct);

	prusage->pu_utlb = acct.pr_ufaults;
	prusage->pu_gbread = acct.pr_bread / (1024LL * 1024LL * 1024LL);
	prusage->pu_bread = acct.pr_bread % (1024LL * 1024LL * 1024LL);
	prusage->pu_gbwrit = acct.pr_bwrit / (1024LL * 1024LL * 1024LL);
	prusage->pu_bwrit =  acct.pr_bwrit % (1024LL * 1024LL * 1024LL);
	prusage->pu_sysc =  acct.pr_sysc;
	prusage->pu_syscr =  acct.pr_syscr;
	prusage->pu_syscw = acct.pr_syscw;
	prusage->pu_syscps = acct.pr_syscps;
	prusage->pu_sysci = acct.pr_sysci;
	prusage->pu_graphfifo = acct.pr_graphfifo;
	prusage->pu_vfault = acct.pr_vfaults;
	prusage->pu_ktlb = acct.pr_kfaults;
}

/*
 * prgetptimer()
 * read all process timers, convert them to timespec.
 */
/* ARGSUSED */
void
PRIFACE_prgetptimer(proc_t *p, PRIFACE_timespec_t *ptime)
{
#if _INTERFACE_NATIVE || _MIPS_SIM != _ABI64
	/*
	 * The 32-bit kernels share the same timespec_t definition with all
	 * of the 32-bit user ABIs so no translation is needed.
	 */
	VPROC_READ_ALL_TIMERS(PROC_TO_VPROC(p), (timespec_t *)ptime)
#else
	{
		/*
		 * We're a 64-bit kernel and we're dealing with a 32-bit
		 * user ABI application and need to translate the internal
		 * kernel 64-bit timespec_t's into the user 32-bit
		 * timespec_t's.
		 */
		timespec_t timers[MAX_PROCTIMER];
		int timer;

		VPROC_READ_ALL_TIMERS(PROC_TO_VPROC(p), timers)
		for (timer = 0; timer < MAX_PROCTIMER; timer++) {
			ptime[timer].tv_sec = timers[timer].tv_sec;
			ptime[timer].tv_nsec = timers[timer].tv_nsec;
		}
	}
#endif
}
#endif /* _MIPS_SIM == _ABI64 || _INTERFACE_NATIVE */


