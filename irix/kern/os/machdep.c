/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1999 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.331 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <ksys/vfile.h>
#include <sys/fpu.h>
#include <sys/immu.h>
#include <sys/kabi.h>
#include <sys/kopt.h>
#include <sys/ksignal.h>
#include <sys/ktime.h>
#include <sys/kucontext.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/prctl.h>
#include <sys/reg.h>
#include <sys/runq.h>
#include <sys/space.h>
#include <sys/sbd.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/softfp.h>
#include <sys/rt.h>
#include <sys/kthread.h>
#include <sys/par.h>
#include <sys/rtmon.h>
#include "os/scheduler/utility.h" /* ugly */

#if defined (SN0)
#include <sys/page.h>
#endif /* SN0 */

#if defined (R10000) && defined (R10000_MFHI_WAR)
#include <sys/inst.h>
#endif

static sema_t cachesema;
int valid_icache_reasons = CACH_REASONS; /* by default all reasons valid */
int valid_dcache_reasons = CACH_REASONS; /* by default all reasons valid */

extern int dcache_size;
extern int icache_size;

#if TFP
extern k_machreg_t get_config(void);
#endif

#ifdef MP
extern int sync_dcaches(void *, unsigned int, pfn_t, unsigned int);
#endif

#ifdef HEART_COHERENCY_WAR
#include <sys/cpu.h>
#endif

/*
#define	ri_imp		Ri.Ri_imp
#define	ri_majrev	Ri.Ri_majrev
#define	ri_minrev	Ri.Ri_minrev
*/

#ifdef JUMP_WAR
extern int R4000_jump_war_correct;
extern int R4000_div_eop_correct;
#ifdef _R5000_JUMP_WAR
extern int R5000_jump_war_correct;
#endif /* _R5000_JUMP_WAR */
#endif
extern int	get_cpu_irr(void);
extern void	fp_find(void);
extern void	fp_init(void);

/* used for execution of mips4 on triton processor  */
__uint32_t _irix5_mips4 = 0;

#if defined(SN0) || defined(SN1) || defined(IP30) || defined(IP32)
/* Allow systune of FS bit in these platforms */
extern int      fpcsr_fs_bit ;
#endif /* SN0 */

/* The code assumes that any number greater than the number
 * in the table is that type of processor.  They must be in
 * decending numerical order.  Stolen from irix/cmd/hinv/hinv.c
 */
struct imp_tbl {
	char *it_name;
	unsigned it_imp;
} cpu_imp_tbl[] = {
	{ "Unknown",	C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ "R5000",	C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ "R4650",	C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ "R4700",	C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ "R4600",	C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ "R8000",	C0_MAKE_REVID(C0_IMP_R8000,0,0) },
	{ "R12000",	C0_MAKE_REVID(C0_IMP_R12000,0,0) },
	{ "R10000",	C0_MAKE_REVID(C0_IMP_R10000,0,0) },
	{ "R6000A",	C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
	{ "R4400",	C0_MAKE_REVID(C0_IMP_R4400,C0_MAJREVMIN_R4400,0) },
	{ "R4000",	C0_MAKE_REVID(C0_IMP_R4000,0,0) },
	{ "R6000",	C0_MAKE_REVID(C0_IMP_R6000,0,0) },
	{ "R3000A",	C0_MAKE_REVID(C0_IMP_R3000A,C0_MAJREVMIN_R3000A,0) },
	{ "R3000",	C0_MAKE_REVID(C0_IMP_R3000,C0_MAJREVMIN_R3000,0) },
	{ "R2000A",	C0_MAKE_REVID(C0_IMP_R2000A,C0_MAJREVMIN_R2000A,0) },
	{ "R2000",	C0_MAKE_REVID(C0_IMP_R2000,0,0) },
	{ 0,			0 }
};


char *
cpu_rev_find(int rev, int *major, int *minor)
{
	struct imp_tbl *itp;
	union rev_id ri;

	ri.ri_uint = rev;
	if (ri.ri_imp == 0) {
		ri.ri_majrev = 1;
		ri.ri_minrev = 5;
	}
	*major = ri.ri_majrev;
	*minor = ri.ri_minrev;
	for (itp = cpu_imp_tbl; itp->it_name; itp++)
		if (((ri.ri_imp << 8) | (ri.ri_majrev << 4) | (ri.ri_minrev))
							 >= itp->it_imp)
			return(itp->it_name);
	return("Unknown implementation");
}

extern int scache_present;
#ifdef _R5000_CVT_WAR
extern int R5000_cvt_war;
#endif /* _R5000_CVT_WAR */

#ifdef _R5000_BADVADDR_WAR
extern int     _r5000_badvaddr_war;
#endif /* _R5000_BADVADDR_WAR */

void
coproc_find(void)
{
	union rev_id ri;

	ri.ri_uint = private.p_cputype_word = get_cpu_irr();
	/*
	 * Hack for the RM5271 processor. We simply mimic the
	 * R5000 since it's interface is identical.
	 */
	if (ri.ri_imp == C0_IMP_RM5271) {
		private.p_cputype_word &= ~C0_IMPMASK;
		private.p_cputype_word |= (C0_IMP_R5000 << C0_IMPSHIFT);
	}

	if (ri.ri_imp == 0) {
		ri.ri_majrev = 1;
		ri.ri_minrev = 5;
	}
#if R4000 && !JUMP_WAR
	if ((ri.ri_imp == C0_IMP_R4000) &&
	    (ri.ri_majrev <= 2) && 
	    (ri.ri_minrev <= 2))
		cmn_err(CE_PANIC,
		"Kernel built without JUMP_WAR, but using Pre-3.0 R4000 parts.");
#endif
#ifdef JUMP_WAR
#ifdef R4600
	if (ri.ri_imp == C0_IMP_R4000) {
#endif
	if (ri.ri_majrev == 2 && ri.ri_minrev == 2)
		R4000_jump_war_correct = 1;

	/* Configure the div-at-eop workaround - this is disabled
	 * in master.d/kernel. Turn it on for all SC rev 2
	 * and rev 3 parts. (Bug is fixed in all R4400's, and does
	 * not occur in PC parts.)
	 */
	if (scache_present && ri.ri_majrev <= 3)
		R4000_div_eop_correct = 1;
#ifdef R4600
    	}
#endif
#ifdef _R5000_JUMP_WAR
	if (ri.ri_imp == C0_IMP_R5000 &&
	    ri.ri_majrev == 1 &&
	    ri.ri_minrev == 0)
		R5000_jump_war_correct = 1;
#endif /* _R5000_JUMP_WAR */
#endif

#ifdef IP32
	_irix5_mips4 = SR_CU3;
#else /* IP32 */
#ifdef TRITON
	if (ri.ri_imp == C0_IMP_R5000 || ri.ri_imp == C0_IMP_RM5271) {
		_irix5_mips4 = SR_CU3;
	}
#endif /* TRITON */
#endif /* IP32 */

#ifdef _R5000_CVT_WAR
	if (! (ri.ri_imp == C0_IMP_R5000 &&
		ri.ri_majrev == 1))
		R5000_cvt_war = 0;
	if (is_specified(arg_r5000_cvt_war))
		R5000_cvt_war = atoi(arg_r5000_cvt_war);
#endif /* _R5000_CVT_WAR */

#ifdef _R5000_BADVADDR_WAR
	if (is_specified(arg_rm5271_badvaddr_war))  {
		_r5000_badvaddr_war = atoi(arg_rm5271_badvaddr_war);
	}
#endif /* _R5000_BADVADDR_WAR */

	fp_find();
	if (private.p_fputype_word)
		fp_init();
}

/*
 * Start clocks
 */
void
clkstart(void)
{
	int s;

	startrtclock();
	s = spl7();
	if (private.p_flags & PDAF_FASTCLOCK) {

		ASSERT(fastclock_processor == cpuid());
		enable_fastclock();
	}
	else
		slowkgclock();	/* missed tick counter & enter into debugger */

	/*
	 * Turning off the clock using stopclocks() stops the profiling clock.
	 * Turn it back on if it was enabled before stopclocks() was called.
	 * NOTE: This fix should remain in sync with startprfclk().
	 * bug #653451
	 */
	if (private.p_prf_enabled)
	  startkgclock();

	splx(s);
}

/*
 * Stop the clock.
 */
void
clkreld(void)
{
	/*	Stop the interval timer.  */
	stopclocks();
}

#ifdef R4600
extern void wait_for_interrupt(kthread_t **);
#else
/*ARGSUSED*/
static void
wait_for_interrupt(kthread_t **nextthread)
{
	extern int local_idle(void);
	extern int idler(void);
	while (local_idle() && !idler()) ;
}
#endif

#ifdef SABLE
static void
sable_actions(void)
{
	void hubuart_service(void), ccuart_service(void),
	     speedup_counter(void);
#if SABLE_ASYNCDISK
	sdservice();
#endif /* SABLE_ASYNCDISK */
#ifdef SN
	if (cpuid() == master_procid)
		hubuart_service();
#else /* !SN */
	ccuart_service();
#endif /* !SN */
	speedup_counter();	/* Artificially speedup next tick */
}
#else
#define sable_actions()
#endif /* SABLE */

void
flush_console(void)
{
	if (*(volatile int *)&constrlen && (private.p_flags & PDAF_MASTER))
		conbuf_flush(CONBUF_UNLOCKED);
}


int	__sched_gfxdelay_backoff	= 1;
/*
 * Sit and spin, waiting for someone to kick us out of idle.
 * If we're the master processor, flush console output if any.
 */
kthread_t *
idle(void)
{
#ifdef __BTOOL__
#pragma btool_prevent
#endif
	kthread_t *kt;

	/*
	 * Note that we want to use RTMON_TASK instead of
	 * RTMON_TASK|RTMON_TASKPROC as we do normally because we know
	 * that we're not contex switching a uthread here ...
	 */
	LOG_TSTAMP_EVENT(RTMON_TASK,
	       EVENT_WIND_EXIT_IDLE, private.p_idletkn,
	       NULL, NULL, NULL);
#if DEBUG
	private.p_switching = 0;
#endif

again:

	while (!(kt = private.p_nextthread)) {
		(void)spl0();

		sable_actions();
		flush_console();
		delay_for_intr();
		if (__sched_gfxdelay_backoff && (private.p_idletkn & NO_GFX))
			DELAY(__sched_gfxdelay_backoff);


		/* If interrupt occurs, restart through resumeidle() */
		private.p_intr_resumeidle = 1;
		while (idlerunq())
			wait_for_interrupt(&private.p_nextthread);
		private.p_intr_resumeidle = 0;
		(void)splhi();
		kt = getrunq();
		if (kt) {
#if DEBUG
			private.p_switching = 1;
#endif
			return kt;
		}
	}

	if (private.p_frs_flags) {
		/*
		 * The FRS cannot take advantage of the scheduler fast path.
		 * Therefore, we must force kt on the runq and clear it from
		 * p_nextthread.
		 */
no_fast_path:
		ASSERT(kt == private.p_nextthread);
		kt_nested_lock(kt);
		ASSERT(kt->k_onrq == CPU_NONE);

		putrunq(kt, CPU_NONE);

		ASSERT(kt->k_onrq != CPU_NONE);
		kt_nested_unlock(kt);
		ASSERT(kt == private.p_nextthread);
		private.p_nextthread = 0;
		goto again;
	}

	if (private.p_runrun) {
		kthread_t *gkt = getrunq();
		if (gkt) {
#if DEBUG
			private.p_switching = 1;
#endif
			return gkt;
		}
	}
	while (kt->k_sqself) {
		ASSERT(!curthreadp);
		DELAY(1);
		SYNCHRONIZE();
	}
	/*
	 * For graphics processes (see bug #681355):
	 * If kt was found from p_nextthread, then we have to call gfx_disp
	 * to check if it is still ok to schedule the thread. Since kt
	 * has now got off its stack, rrm_suspend has been executed; and
	 * rrm_suspend may have decided that the gfx pipe is backed up. If so,
	 * we have to consult gfx_disp, again (again == it is not enough if
	 * runcond_ok/gfx_disp was called at the time of putrunq() at which time
	 * rrm_suspend is not guaranteed to have been called, esp. on user_resched).
	 */
	if (!runcond_ok(kt))
		goto no_fast_path;

	kt_nested_lock(kt);
	ASSERT(kt->k_onrq == CPU_NONE);
	kt_startrun(kt);
#if DEBUG
	private.p_switching = 1;
#endif
	return kt;
#ifdef __BTOOL__
#pragma btool_unprevent
#endif
}

/*
 * Clear registers on exec
 */
/* ARGSUSED */
void
setregs(caddr_t entry,
	int is_isa_mips3,
	int is_isa_mips4,
	int is_fpset_32)
{
	uthread_t *ut = curuthread;
	k_machreg_t *p_int;
	eframe_t *ep = &curexceptionp->u_eframe;

	/* release FP if held so new values can take affect */
	checkfp(curuthread, 1);

	ut->ut_pproxy->prxy_fp.pfp_fp = 0;

	/* Single-threaded at this point, so don't need to lock
	 * proxy struct.
	 */
	ut->ut_pproxy->prxy_fp.pfp_dismissed_exc_cnt = 0;

	for (p_int = &USER_REG(EF_AT); p_int < &USER_REG(EF_GP); )
		*p_int++ = 0;
	ep->ef_fp = 0;
	ep->ef_ra = 0;
	ep->ef_mdlo = 0;
	ep->ef_mdhi = 0;
	ep->ef_epc = (k_smachreg_t)((__psint_t)entry);
	ep->ef_sr |= SR_UX;

#ifdef TRITON
	ep->ef_sr |= _irix5_mips4;
#endif /* TRITON */

#if R10000 && (! R4000)
	/*
	 * We need to turn on the XX bits for mips3 programs because
	 * some mips3 programs may use mips4 shared libraries.
	 */
	if IS_R10000() {
		if (is_isa_mips3 || is_isa_mips4)
			ep->ef_sr |= SR_XX;
		else
			ep->ef_sr &= ~SR_XX;
	}
#endif

	if (is_fpset_32) {
		ep->ef_sr |= SR_FR;
		ut->ut_pproxy->prxy_fp.pfp_fpflags |= P_FP_FR;
	} else {
		ep->ef_sr &= ~SR_FR;
		ut->ut_pproxy->prxy_fp.pfp_fpflags &= ~P_FP_FR;
	}

#if TFP
	/*
	 * We need to turn on the XX bits for mips3 programs because
	 * some mips3 programs may use mips4 shared libraries.
	 */
	if (is_isa_mips3 || is_isa_mips4)
		ep->ef_sr |= SR_XX;
	else
		ep->ef_sr &= ~SR_XX;

	if (!(ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_PRESERVE)) {
		if (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_PRECISE_EXCP_HDR)
			ut->ut_pproxy->prxy_fp.pfp_fpflags &=
							~P_FP_IMPRECISE_EXCP;
		else
			ut->ut_pproxy->prxy_fp.pfp_fpflags |=
							P_FP_IMPRECISE_EXCP;
	}

	if (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_IMPRECISE_EXCP)
		ep->ef_sr &= ~SR_DM;
	else
		ep->ef_sr |= SR_DM;

	ep->ef_config = get_config();

	if (!(ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_PRESERVE)) {
		if (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SMM_HDR) {
			ep->ef_config |= CONFIG_SMM;
			ut->ut_pproxy->prxy_fp.pfp_fpflags |= P_FP_SMM;
		} else {
			ep->ef_config &= ~CONFIG_SMM;
			ut->ut_pproxy->prxy_fp.pfp_fpflags &= ~P_FP_SMM;
		}
	}
#endif	/* TFP */

	bzero(PCB(pcb_fpregs), 32*sizeof(PCB(pcb_fpregs[0])));

#if TFP
	/*
	 * Default setting for "performance" mode FP is to set FS.
	 * We need to set the bit here since the FPU will generate
	 * an fp interrupt on every denorm flush to zero if the bit
	 * is not set.
	 */
	if (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_IMPRECISE_EXCP)
		PCB(pcb_fpc_csr) = CSR_FSBITSET;
	else
#elif defined(MIPS4_ISA)
	/* For all platforms supporting the mips4 isa, the FS bit
	 * is set only for mips4 binaries.
	 */
	 if (is_isa_mips4) {
#if defined(SN0) || defined(SN1) || defined(IP30) || defined(IP32)
		if (fpcsr_fs_bit == 1) {
			/* Default. Set this bit for all cpus. */
			PCB(pcb_fpc_csr) = CSR_FSBITSET;
		} else if (fpcsr_fs_bit == 0) {
			/* Set this bit to 0. Always works for mixed
			   r10k and r12k systems. Works for all systems in
			   general. Handled in sw by fp trap. */
			PCB(pcb_fpc_csr) = 0 ;
		}
#else
		PCB(pcb_fpc_csr) = CSR_FSBITSET ;
#endif /* SN0 */
	}
	else
#endif
		PCB(pcb_fpc_csr) = 0;

	PCB(pcb_fpc_eir) = 0;
	PCB(pcb_ownedfp) = 0;
	PCB(pcb_ssi.ssi_cnt) = 0;
}

int
min(int a, int b)
{
	return( (a<b) ? (a):(b) );
}

int
max(int a, int b)
{
	return( (a>b) ? (a):(b) );
}

#ifdef DEBUG_R4600SC
int cacheop_blast = 0;
int cacheop_wback = 0;
int cacheop_inval = 0;
#endif

/*
 * clean_icache_action, clean_dache_action - routines called via cpuaction()
 *		to flush caches on remote processors.
 */
static void
clean_icache_action(void *addr, unsigned int bcnt, pfn_t pfn,
		    unsigned int flags)
{
	clean_icache(addr, bcnt, pfn, flags);
	vsema(&cachesema);
}

/*
 * clean_isolated_icache - force a brute force icache flush to the set of
 * isolated processors identified by the cpumask
 *
 * Used to hit cpus skipped by _bclean_caches
 *
 * On r4k and r10k systems, this call is not needed if _bclean_caches was
 * called to flush icaches.  On those systems, exclusive ownership rather than
 * cpuactions used to invalidate the icache
 */
void
clean_isolated_icache(cpumask_t flushmask)
{
	int i;
	int id;

	for (i = 0; i < maxcpus; i++) {
		if ((id = pdaindr[i].CpuId) != -1) {
			if (CPUMASK_TSTB(flushmask, id)) {
				psema(&cachesema, PZERO);
				cpuaction(id, (cpuacfunc_t)clean_icache_action,
					A_NOW, (void *)K0SEG, icache_size, 0,
					CACH_NOADDR);
			}
		}
        }
}

#if MP || IP20 || IP22 || IP32
#define _ALWAYS_PASS_PFN	1	/* some MP systems always want pfn */
#endif

/*
 * Note that becuase of isolated cpus, this routine basically assumes it
 * is called on the current process only ...
 */
void
_bclean_caches(void *vaddr, unsigned int bcnt, pfn_t pfn,
	      unsigned int flags)
{
	register int i;
	register int id;
	int icaches_synced=0;
	void *old_rtpin;

	/*
	 * flush dcache if requested
	 */

	/* Must not migrate to another cpu during this operation, though
	 * preemption is OK. Cache related operation don't fair well if
	 * the cache you're operating on changes.
	 * Interrupt stack routines might need to invoke cache ops and
	 * we can't call rt_pin_thread(). If we're on ISTACK then we
	 * know we won't be pre-empted.
	 */
	if (private.p_kstackflag != PDA_CURINTSTK)
		old_rtpin = rt_pin_thread();

	if ((flags & CACH_DCACHE) && (flags & valid_dcache_reasons)) {
		if (flags & valid_dcache_reasons &
		    (CACH_ICACHE_COHERENCY|CACH_FORCE)) {
#if MP
			if ((maxcpus > 1) && !(flags & CACH_LOCAL_ONLY))
				if (!(flags & CACH_NOADDR) ||
				    (btoct(vaddr) == btoct((__psint_t)vaddr+bcnt-1))) {
				/*
				 * Only reachable on an MP machines
				 *
				 * Force potentially dirty primary data cache
				 * lines on other CPUs to within the system's
				 * coherency boundary.
				 * Note that EITHER CACH_NOADDR is clear OR
				 * we must be flushing a single page -- several
				 * callers try to flush entire cache by specifying
				 * CACH_NOADDR on K0SEG for cache_size and we
				 * can't do this in sync_dcaches.
				 */
				icaches_synced =
					sync_dcaches(vaddr, bcnt, pfn, flags);
			      }
#endif
			/* If we're flushing icache too (for icache coherency)
			 * then the low level icache flush will take care of
			 * writing back the primary dcache, if needed.
			 */
			if ((!(flags & CACH_ICACHE)) ||
			    (!(flags & valid_icache_reasons)))
				clean_dcache(vaddr, bcnt, pfn, flags);
		}	/* end - valid reason is "icache coherency" */
		else
			clean_dcache(vaddr, bcnt, pfn, flags);
	}

	/*
	 * flush icache if requested
	 *
	 * let cpuaction handle local case - since we might actually
	 * switch cpus while waiting at the psema
	 */
	if ((flags & CACH_ICACHE) && (flags & valid_icache_reasons))
	  if ((maxcpus == 1) || (flags & CACH_LOCAL_ONLY) || icaches_synced) {
		clean_icache(vaddr, bcnt, pfn, flags);
	  } else for (i = 0; i < maxcpus; i++) {
		if ((id = pdaindr[i].CpuId) != -1) {
			/*
			 * processes don't migrate in/out of isolated proc
			 * when they do we'll flush it then
			 */
		    	if ((pdaindr[i].pda->p_flags & PDAF_ISOLATED) == 0) {
				psema(&cachesema, PZERO);
				cpuaction(id, (cpuacfunc_t)clean_icache_action,
					  A_NOW, vaddr, bcnt, pfn,flags);
			} else {
				/*
				 * We force a cpuaction to the isolated proc
				 * iff we are in the context of share group
				 * that has an isolated member on that processor
				 */
				vasid_t vasid;
				as_getasattr_t asattr;

				/*
				 * At this point we really had better only
				 * get user virtual addresses - no one else
				 * should be calling this for icache flushing
				 * XXX except when we are flushing
				 * lots of cache, cache_operation might
				 * call us with a K0 address. - this interface
				 * is aweful!
				ASSERT(IS_KUSEG(vaddr));
				 */
				ASSERT(curuthread);

				as_lookup_current(&vasid);
				VAS_GETASATTR(vasid, AS_ISOMASK, &asattr);

				if (CPUMASK_TSTB(asattr.as_isomask, i)) {
					psema(&cachesema, PZERO);
					cpuaction(id, (cpuacfunc_t)clean_icache_action,
						  A_NOW, vaddr, bcnt, pfn,flags);
				}
			}
		}
	}

	if (private.p_kstackflag != PDA_CURINTSTK)
		rt_unpin_thread(old_rtpin);
}

#ifdef _VCE_AVOIDANCE
static caddr_t
do_other_color_flush(void *addr, uint pfn, size_t len, unsigned int flags)
{
	caddr_t mapped_address;
	extern uint cachecolorsize;

	if (cachecolorsize > 2)
		return (caddr_t)NULL;

	mapped_address = (caddr_t) page_mapin_pfn(pfntopfdat(pfn),
		(VM_NOSLEEP | VM_OTHER_VACOLOR), (colorof(addr) ^ 1), pfn);

	if (mapped_address) {
		_bclean_caches(mapped_address + poff(addr), len,
			pfn, flags | CACH_VADDR);
		page_mapout(mapped_address);
	}
	return mapped_address;
}
#endif

/*
 * cache_operation - write back and/or invalidate range of virtual addresses
 *
 *	addr and bcnt specify an arbtrary address and length
 *	flags specifies type of operation and reason for operation
 *
 *	If reason for operation is not a valid reason on this system
 *	then operation is not performed.
 *
 *	If a cache is specified in the flags (CACH_ICACHE or CACH_DCACHE)
 *	then we assume caller only wishes to operate on the specified cache.
 *	If a cache is NOT specified, this routine will determine which
 *	caches need to be operated on based upon the reason (also contained
 *	in the flags).
 *
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE: Only flushes pages that are CACHED - not K1SEG addresses!!
 */

#ifdef MH_R10000_SPECULATION_WAR
extern is_in_pfdat(pgno_t pfn);
#endif /* MH_R10000_SPECULATION_WAR */

void
cache_operation(void *addr, unsigned bcnt, unsigned int flags)
{
#ifdef R4600SC
	extern int two_set_pcaches, pdcache_size, picache_size, boot_sidcache_size;
	register int dsize, isize;
	register int do_writeback = 0;
#endif
	register unsigned int rem;
	unsigned int reasons;
	int error;
	as_setrangeattr_t as;
	vasid_t vasid;
	pde_t *pd;

	/* If caller doesn't specify which cache, then operate on both
	 * if the flush reason requires it.
	 */

	if ((flags & (CACH_DCACHE|CACH_ICACHE)) == 0)
		flags |= (CACH_DCACHE|CACH_ICACHE);

	/* If caller didn't specify reason, the use CACH_FORCE.  Based upon
	 * which caches are specified and which reasons are given, see if
	 * ANY valid reason exists for operating on any of the caches.
	 */

	if ((reasons = (flags & CACH_REASONS)) == 0)
		reasons = CACH_FORCE;	/* caller didn't specify */

	flags &= ~CACH_REASONS;		/* Preserve all non-reason flags */
	if (flags & CACH_ICACHE) {
		if (reasons & valid_icache_reasons)
			flags |=  (reasons & valid_icache_reasons);
		else
			flags &= ~CACH_ICACHE;
	}
	if (flags & CACH_DCACHE) {
		if (reasons & valid_dcache_reasons)
			flags |=  (reasons & valid_dcache_reasons);
		else
			flags &= ~CACH_DCACHE;
	}

	/* If no valid reason to flush on this machine, just return */

	if (!(flags & CACH_REASONS))
		return;

	/* If caller didn't specify the type of operation, try to determine
	 * which cache ops make sense based upon "reason" for cache operation.
	 */
	if (!(flags & CACH_OPMASK)) {
		uint	dcache_reasons = flags & valid_dcache_reasons;

	  	if (flags & CACH_DCACHE) {
			if (dcache_reasons & (CACH_IO_COHERENCY|CACH_FORCE))
				flags |= CACH_WBACK|CACH_INVAL;
			else if (dcache_reasons & CACH_ICACHE_COHERENCY)
				flags |= CACH_WBACK;
		}
	  	if (flags & CACH_ICACHE) {
			if (flags & valid_icache_reasons)
				flags |= CACH_INVAL;
		}
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000() &&
		(flags & CACH_IO_COHERENCY) &&
		(flags & CACH_INVAL) &&
		(iskvir(addr) &&
		is_in_pfdat(kvatopfn(addr)))) {

		/* Must apply the R10000 speculative miss workaround */
		(void) invalidate_range_references(addr, bcnt, flags, 0);
		return;
	}
#endif /* MH_R10000_SPECULATION_WAR */

	/* If we finally get here, then we have cache operation which is
	 * required on this system.
	 *
	 * If K0 segment then just call bclean_caches
	 */

	if (IS_KSEG0(addr)
#ifdef _VCE_AVOIDANCE
		&& (! (vce_avoidance && (flags & CACH_OTHER_COLORS)))
#endif /* _VCE_AVOIDANCE */
		) {
		_bclean_caches(addr, bcnt,
#if _ALWAYS_PASS_PFN
			       btoct(K0_TO_PHYS(addr)),	/* setup pfn */
#else
			       0,
#endif
			       flags | CACH_VADDR);
		return;
	} else if (IS_KSEG1(addr)) {
		/* not cached */
		return;
	}

	/* If operating on more than half the cache OR if we get an
	 * error from dovflush, just operate on the whole cache.
	 */
#ifdef R4600SC
	/*
	 * On a 4600SC if the operation is a data cache writeback w/o
	 * invalidate we want to operate on the primary cache *only*
	 * and we want to do an index invalidate on the entire primary data
	 * cache if the object is larger than half the pdcache size.
	 */
	if ((flags & (CACH_DCACHE|CACH_ICACHE|CACH_INVAL|CACH_WBACK)) == 
	    (CACH_DCACHE|CACH_WBACK)) {
#ifdef DEBUG_R4600SC
		cacheop_wback++;
#endif
		do_writeback = (two_set_pcaches && boot_sidcache_size);
		dsize = do_writeback ? pdcache_size : dcache_size;
		isize = do_writeback ? picache_size : icache_size;
	    
	} else {
#ifdef DEBUG_R4600SC
		cacheop_inval++;
#endif
		dsize = dcache_size;
		isize = icache_size;
	}
	
	if ((bcnt > (MIN(dsize,isize) / 2)))
#else
	/* Require flush request size > cache size before we flush the
	 * entire cache.  Blindly flushing the entire cache might make
	 * this request faster but has a large impact on rest of system
	 * performance.  In addition, on MP systems we end up broadcasting
	 * a flush request to ALL cpus, but keeping to an explicit address
	 * flush lets us flush only the specified lines and we can do
	 * this locally (through sync_dcache_excl) instead of broadcasting
	 * the request.
	 * NOTE: The correct lower bound for invoking full cache flush is
	 * TBD and most likely much larger than the value chosen here.  
	 */
	if (bcnt > 2*(MIN(dcache_size,icache_size)))
#endif
		goto flushall;

	/* Note that there is a 'bug' in this routine for the
	 * R4000: we use the K0 address rather than the virtual address
	 * for KU and K2 addresses. This could be a problem, since
	 * the r4k primary caches are indexed virtually. Therefore, the
	 * k0 address may index a different part of the cache than the
	 * KU or K2 address. This is avoided since the actual flushing
	 * and writeback routines use 'hit' cache ops on the secondary
	 * cache and rely on the R4000 to writeback/invalidate from
	 * the primary where needed. This will not work for the 'index'
	 * cache ops, or if we issued 'hit' cache ops for the primary
	 * cache, or for the r4k small package.
	 */
	for (; bcnt; bcnt -= rem, addr = (caddr_t)addr+rem) {
		/*
		 * calculate bytes to flush in this page
		 */
		rem = NBPP - poff(addr);
		if (rem > bcnt)
			rem = bcnt;

		/*
		 * calculate appropriate physical address
		 */
		if (IS_KUSEG(addr)) {
			as.as_op = AS_CACHEFLUSH;
			as.as_cache_flags = flags;
			as_lookup_current(&vasid);
			error = VAS_SETRANGEATTR(vasid, addr, rem, &as, NULL);
			if (error == ENOENT) {
				/*
				 * can't resolve
				 * so just flush entire cache and give up
				 */
#ifdef DEBUG
				cmn_err(CE_CONT, "vflush flush entire cache for uvaddr:%x \n", addr);
#endif
				goto flushall;
			} else if (error == EDOM) {
				/* no pte or non-cached attributes */
				continue;
			} else if (error == EXDEV) {
				/*
				 * 1) page not valid
				 * 2) can't hold pfdat
				 * 3) can't get mapped_address for small mem
				 *		systems/VCE_AVOIDANCE
				 */
				goto flushall;
			}
		} else if (iskvir(addr)) {
			caddr_t	vaddr = addr;
			pd = kvtokptbl(vaddr);

			if (flags & CACH_SN0_POQ_WAR) {
				vaddr = (char *)(PHYS_TO_K0(ctob(pg_getpfn(pd))) 
							+ poff(addr));
			} 
			if (!pg_isvalid(pd)){
				if (flags & CACH_SN0_POQ_WAR)
					continue;
				cmn_err_tag(130,CE_PANIC, "vflush3");
			}


			if (pg_isnoncache(pd)
#ifdef R10000_SPECULATION_WAR	/* need to force flushes of uncached ranges */
				&& !((flags & CACH_FORCE)
#if MH_R10000_SPECULATION_WAR
				&& IS_R10000()
#endif /* MH_R10000_SPECULATION_WAR */
				)
#endif /* R10000_SPECULATION_WAR */
				)
				continue;
#ifdef _VCE_AVOIDANCE
			if (vce_avoidance && (flags & CACH_OTHER_COLORS)) {
				if (do_other_color_flush(vaddr, pg_getpfn(pd),
						rem, flags) == NULL)
					goto flushall;
			} else
#endif /* _VCE_AVOIDANCE */
			_bclean_caches(vaddr, rem, pg_getpfn(pd), 
				flags|CACH_VADDR);
#ifdef _VCE_AVOIDANCE
		} else if (IS_KSEG0(addr)) {
			uint pfn = kvatopfn(addr);
			if (vce_avoidance && (flags & CACH_OTHER_COLORS))
				if (do_other_color_flush(addr, pfn,
						rem, flags) == NULL)
					goto flushall;
#endif /* _VCE_AVOIDANCE */
		} else {
			/* who knows where it is??? */
#ifdef DEBUG
			cmn_err(CE_CONT, "vflush flush entire cache for vaddr:%x \n", addr);
#endif
			goto flushall;
		}
	}
	return;

flushall:
	{
		extern char _physmem_start[];
		caddr_t k0_rambase = (caddr_t)PHYS_TO_K0(_physmem_start);

#ifdef DEBUG_R4600SC
		cacheop_blast++;
#endif
		
		if (icache_size != dcache_size) {
			if (flags & CACH_DCACHE)
				_bclean_caches(k0_rambase, dcache_size,
					btoct(_physmem_start),
					(flags & ~CACH_ICACHE)|CACH_NOADDR);
			if (flags & CACH_ICACHE)
				_bclean_caches(k0_rambase, icache_size,
					btoct(_physmem_start),
					(flags & ~CACH_DCACHE)|CACH_NOADDR);
		} else
			_bclean_caches(k0_rambase, dcache_size,
				       btoct(_physmem_start),
#ifdef R4600SC
				/*
				 * for R4600SC *do not* set CACH_NOADDR
				 * if we are doing a dcache writeback.
				 * this prevents the lower layer from
				 * calling cache_wb_inval() and blasting
				 * the entire cache; instead it will call
				 * the desired __dcache_wb().
				 */
				flags | (do_writeback ? 0 : CACH_NOADDR));
#else
				flags | CACH_NOADDR);
#endif
	}
}

/*
 * bp_dcache_wbinval - write back and invalidate range of virtual addresses
 *                  for the specified bp (for I/O coherency)
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE: Only flushes pages that are CACHED - not K1SEG addresses!!
 */
void
bp_dcache_wbinval(buf_t *bp)
{

#ifdef HEART_COHERENCY_WAR
	/* different revisions of IP30 prototypes need a mix of flushing */
	if (heart_need_flush((bp->b_flags & B_READ) == 0) == 0)
		return;
#define BP_WARFLAGS	CACH_FORCE
#else
#define	BP_WARFLAGS	0
#endif	/* HEART_COHERENCY_WAR */

	bp_dcache_flush(bp, CACH_INVAL);
}

/*
 * This call always writes back the data pointed to by the buffer,
 * add the caller is allowed to pass other CACH_XXX flags in to be
 * passed along to the cache_operation. This is normally just CACHE_INVAL.
 */
void
bp_dcache_flush(buf_t *bp, uint other_ops)
{
	register struct pfdat *pfd;
	register int npgs;
#ifdef MH_R10000_SPECULATION_WAR
	int need_sync = 0;
#endif /* MH_R10000_SPECULATION_WAR */

	if (BP_ISMAPPED(bp)) {
		cache_operation(bp->b_un.b_addr, bp->b_bcount,
			CACH_DCACHE|CACH_WBACK|CACH_IO_COHERENCY|
			BP_WARFLAGS|other_ops);
		return;
	}
	if (bp->b_flags & B_UNCACHED)
		return;
	pfd = bp->b_pages;
	npgs = btoc(bp->b_bufsize);

	if (bp->b_bufsize > (dcache_size / 2)) {
		extern char _physmem_start[];
		caddr_t k0_rambase = (caddr_t)PHYS_TO_K0(_physmem_start);

		/* lets just blow away the whole thing */
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000()) {
			while (npgs--) {
				need_sync |=
					invalidate_page_references(pfd,
						CACH_DCACHE|CACH_WBACK|
						CACH_NOADDR|CACH_IO_COHERENCY|
						other_ops, 0);
				pfd = pfd->pf_pchain;
			}
			if (need_sync)
				tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
		}
#endif /* MH_R10000_SPECULATION_WAR */
		cache_operation(k0_rambase, dcache_size,
			CACH_DCACHE|CACH_WBACK|CACH_NOADDR|
			CACH_IO_COHERENCY|BP_WARFLAGS|other_ops);
		return;
	}
	while (npgs--) {
#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000())
			need_sync |= invalidate_page_references(pfd,
				CACH_DCACHE|CACH_WBACK|CACH_NOADDR|
				CACH_IO_COHERENCY|other_ops, 0);
#endif /* MH_R10000_SPECULATION_WAR */
		_bclean_caches( 0, NBPP, pfdattopfn(pfd),
			CACH_DCACHE|CACH_WBACK|CACH_NOADDR|
			CACH_IO_COHERENCY|BP_WARFLAGS|other_ops);
		pfd = pfd->pf_pchain;
	}
#ifdef MH_R10000_SPECULATION_WAR
	if (need_sync)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
#endif /* MH_R10000_SPECULATION_WAR */

#if HEART_COHERENCY_WAR
#undef cache_operation
#endif
}

/*
 *  cacheinit() - initialize semaphore for regulating use of cpuaction blocks
 *		by bclean_Xcache().
 *  called in main() must be after allowboot().
 */
void
cacheinit(void)
{
	extern int ncache_actions;

	ncache_actions = numcpus;
	initnsema(&cachesema, ncache_actions, "cachesm");
}

/* OBSOLETE procedures -- for backwards compatibility */

/*
 * data_cache_operation - write back and/or invalidate range of virtual addresses
 *
 *	addr and bcnt specify an arbtrary address and length
 *	flags specifies type of operation and reason for operation
 *
 *	If reason for operation is not a valid reason on this system
 *	then operation is not performed.
 *
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE: Only flushes pages that are CACHED - not K1SEG addresses!!
 */
void
data_cache_operation(void *addr, unsigned bcnt, unsigned int flags)
{
	cache_operation(addr, bcnt, flags|CACH_DCACHE);
}


/*
 * inst_cache_operation - invalidate range of virtual addresses
 *
 *	addr and bcnt specify an arbtrary address and length
 *	flags specifies type of operation and reason for operation
 *
 *	If reason for operation is not a valid reason on this system
 *	then operation is not performed.
 *
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE: Only flushes pages that are CACHED - not K1SEG addresses!!
 */
void
inst_cache_operation(void *addr, unsigned bcnt, unsigned int flags)
{
	cache_operation(addr, bcnt, flags|CACH_ICACHE);
}

/*
 * icache_inval - invalidate range of virtual addresses in icache
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE2: ONLY FLUSHES ICACHE
 * NOTE3: Only flushes pages that are CACHED - not K1SEG addresses!!
 * OBSOLETE INTERFACE
 */
void
icache_inval(register void *addr, register unsigned bcnt)
{
	cache_operation( addr, bcnt, CACH_ICACHE|CACH_INVAL|CACH_FORCE);
}

/*
 * dcache_inval - invalidate range of virtual addresses in dcache
 * NOTE: for KUSEG addrs, the aspacelock MUST NOT be set
 * NOTE2: ONLY FLUSHES DCACHE
 * NOTE3: Only flushes pages that are CACHED - not K1SEG addresses!!
 * OBSOLETE INTERFACE
 */
void
dcache_inval(register void *addr, register unsigned bcnt)
{
	dki_dcache_inval( addr, bcnt );
}

void
bclean_caches(void *vaddr, unsigned int bcnt, unsigned int pfn,
	      unsigned int flags)
{

	unsigned int reasons;

	/* If caller doesn't specify which cache, then operate on both
	 * if the flush reason requires it.
	 */

	if ((flags & (CACH_DCACHE|CACH_ICACHE)) == 0)
		flags |= (CACH_DCACHE|CACH_ICACHE);

	/* If caller didn't specify reason, the use CACH_FORCE.  Based upon
	 * which caches are specified and which reasons are given, see if
	 * ANY valid reason exists for operating on any of the caches.
	 */

	if ((reasons = (flags & CACH_REASONS)) == 0)
		reasons = CACH_FORCE;	/* caller didn't specify */

	flags &= ~CACH_REASONS;		/* Preserve all non-reason flags */
	if (flags & CACH_ICACHE) {
		if (reasons & valid_icache_reasons)
			flags |=  (reasons & valid_icache_reasons);
		else
			flags &= ~CACH_ICACHE;
	}
	if (flags & CACH_DCACHE) {
		if (reasons & valid_dcache_reasons)
			flags |=  (reasons & valid_dcache_reasons);
		else
			flags &= ~CACH_DCACHE;
	}

	/* If caller didn't specify the type of operation, try to determine
	 * which cache ops make sense based upon "reason" for cache operation.
	 */

	if (!(flags & CACH_OPMASK)) {
		unsigned int dcache_reasons = flags & CACH_REASONS;

	  	if (flags & CACH_DCACHE) {
			if (dcache_reasons & CACH_IO_COHERENCY)
				flags |= CACH_WBACK|CACH_INVAL;
			else if (dcache_reasons & CACH_ICACHE_COHERENCY)
				flags |= CACH_WBACK;
		}
	  	if (flags & CACH_ICACHE) {
			if (flags & valid_icache_reasons)
				flags |= CACH_INVAL;
		}
	}

#if _ALWAYS_PASS_PFN
	if (!pfn && IS_KSEG0(vaddr))
		pfn = kvatopfn(vaddr);
#endif
	_bclean_caches(vaddr, bcnt, pfn, flags);
}


/*ARGSUSED*/
void
machine_error_dump(char *fru_hint)
{
#ifdef EVEREST
        extern void everest_error_dump(void);
        everest_error_dump();
#endif
#ifdef SN0
        extern void sn0_error_dump(char *);
        sn0_error_dump(fru_hint);
#endif
}

#ifdef MH_R10000_SPECULATION_WAR
extern void _dki_dcache_inval(void *, int);

void
dki_dcache_inval(void *addr, int bcnt)
{
	if (IS_R10000())
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    0);
	else
		_dki_dcache_inval( addr, bcnt );
}

extern void _dki_dcache_wbinval(void *, int);

void
dki_dcache_wbinval(void *addr, int bcnt)
{
	if (IS_R10000())
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|CACH_WBACK|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    0);
	else
		_dki_dcache_wbinval( addr, bcnt );
}
#endif /* MH_R10000_SPECULATION_WAR */


static void do_cache_operation_pfnlist(pfn_t *, int, unsigned int, int);

void
cache_operation_pfnlist(pfn_t *pfn_list,
			int pfn_count,
			unsigned int flags)
{
	flags |= CACH_NOADDR;	/* no addr is implied */
	flags &= ~CACH_VADDR;	/* vaddr is ignored */

	ASSERT((flags & (CACH_ICACHE|CACH_DCACHE)) == CACH_DCACHE);
	/* There's nothing scientific about using dcache_size/2 as the
	 * threshold value here.  It was chosen because the test program
	 * "dmedia/lib/libdvl/examples/vtos -m mapped -m cache" seemed
	 * to like that value with all combinations of other command
	 * line options.  That is, worst-case sys/cpu usage (as reported
	 * by osview) was never more than 6% peak and 4% average
	 * on an R5000/IP32.
	 *   A larger threshold resulted in very high cpu usage (in excess 
	 * of 50% in some cases).
	 *   A smaller threshold actually will produce marginally lower
	 * CPU usage, but excessive flushing of the entire cache will
	 * impact overall system performance. -wsm11/8/96.
	 */
	if(ctob(pfn_count) >= (dcache_size>>1))
	{
		if (IS_R10000()) {
		    do_cache_operation_pfnlist(pfn_list, pfn_count, flags, 0);
		}
		_bclean_caches((void *)K0SEG, dcache_size, 0, flags);
        } else {
		do_cache_operation_pfnlist(pfn_list, pfn_count, flags, 1);
	}
}



/* Walk the pfnlist and:
 *   1) flush the pages from the cache if do_cache_op is set
 *   2) turn off the valid bit if this is an R10k
 * This is in a seperate function to avoid having two nearly identical loops
 * in cache_operation_pfnlist().
 */
static void
do_cache_operation_pfnlist(pfn_t *pfn_list,
			   int pfn_count,
			   unsigned int flags, int do_cache_op)
{
#ifdef MH_R10000_SPECULATION_WAR
	int	need_sync = 0;
#endif /* MH_R10000_SPECULATION_WAR */

	while (--pfn_count >= 0) {
		pfn_t pfn = *pfn_list;

#ifdef MH_R10000_SPECULATION_WAR
		if (IS_R10000())
		    need_sync |= invalidate_page_references(pfntopfdat(pfn),
							     0 /* ignored! */,
							    0);
#endif /* MH_R10000_SPECULATION_WAR */
		if (do_cache_op) {
			/* Brute force for now.  We can be clever later
			 * and call lower-level routines. -wsm10/15/96. 
			 */
			cache_operation((void *)PHYS_TO_K0(pfn<<PNUMSHFT), 
					NBPP, flags);
		}
		++pfn_list;
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (need_sync)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
#endif /* MH_R10000_SPECULATION_WAR */
}

/*
 * Invalidate cache for I/O, leaving memory addressable read-only
 */

void
xdki_dcache_inval_readonly(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    INV_REF_READONLY);
	} else 
		_dki_dcache_inval( addr, bcnt );
#else /* MH_R10000_SPECULATION_WAR */
	dki_dcache_inval( addr, bcnt );
#endif /* MH_R10000_SPECULATION_WAR */
}


/*
 * Writeback-invalidate cache for I/O, leaving memory addressable read-only
 */

void
xdki_dcache_wbinval_readonly(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		invalidate_range_references(addr,bcnt,
					    CACH_DCACHE|CACH_SCACHE|CACH_WBACK|
					    CACH_INVAL|CACH_IO_COHERENCY,
					    INV_REF_READONLY);
	} else 
		_dki_dcache_wbinval( addr, bcnt );
#else /* MH_R10000_SPECULATION_WAR */
	dki_dcache_wbinval( addr, bcnt );
#endif /* MH_R10000_SPECULATION_WAR */
}


/*
 * Revalidate address
 */

#ifdef MH_R10000_SPECULATION_WAR
static void
revalidate_address_range(void *addr, int bcnt, int readonly)
{
	__psunsigned_t vaddr;
	__psunsigned_t vlimit;
	pde_t *pde;

	if (! iskvir(addr))
		return;

	for (vaddr = ((__psunsigned_t) addr),
	     vlimit = ((__psunsigned_t) addr) + bcnt;
	     vaddr < vlimit;
	     vaddr += (_PAGESZ - poff(vaddr))) {
		pde = kvtokptbl(vaddr);
		if (! pg_isvalid(pde))
			continue;
		if (! readonly &&
		    ! pg_ismod(pde)) {
			pg_setmod(pde);
			pg_sethrdvalid(pde);
		} else if (! pg_ishrdvalid(pde)) 
			pg_sethrdvalid(pde);
		else
			continue;
		tlbdropin(0,(caddr_t) vaddr,pde->pte);
	}
}
#endif /* MH_R10000_SPECULATION_WAR */

/*
 * Revalidate address, read-write
 */

/*ARGSUSED*/
void
xdki_dcache_validate(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000())
		revalidate_address_range(addr,bcnt,0);
#endif /* MH_R10000_SPECULATION_WAR */
}

/*ARGSUSED*/
void
xdki_dcache_validate_readonly(void *addr,int bcnt)
{
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000())
		revalidate_address_range(addr,bcnt,1);
#endif /* MH_R10000_SPECULATION_WAR */
}


#if SN0
/*
 * Workaround for the SN0 POQ Overflow problem found in pre-rev2.3 hubs.
 */
#define	POQ_CACH_BITS	(CACH_DCACHE|CACH_WBACK|CACH_FORCE|CACH_LOCAL_ONLY|CACH_SN0_POQ_WAR)

void
sn0_poq_war(void *ptr)
{
	caddr_t addr;
	struct pfdat *pfd;
	int npgs;
	struct buf *bp = (struct buf *)ptr;
  	

	if (WAR_HUB_POQ_DISABLED())
	    return;

	if (iskvir(bp))
	    return;

	/* If this is a write operation and if we're on an old hub, handle 
	 * approrpriately
	 */

	if (!(bp->b_flags & B_READ)) {
		void *old_rtpin;

		if (private.p_kstackflag != PDA_CURINTSTK)
			old_rtpin = rt_pin_thread();

		if (BP_ISMAPPED(bp)) {
			addr = bp->b_un.b_addr;
			if (iskvir(addr))
				cache_operation(addr, bp->b_bcount,
						POQ_CACH_BITS);
		} else {
			npgs = btoc(bp->b_bufsize);
			pfd = bp->b_pages;
			while (pfd && npgs) {
				pfn_t pfn;
				
				if (!page_validate_pfdat(pfd))
				    break;

				pfn = pfdattopfn(pfd);
				addr = (char *)(PHYS_TO_K0(ctob(pfn)));

				cache_operation(addr, NBPP, POQ_CACH_BITS);

				pfd = pfd->pf_pchain;
				npgs--;
			}
		}

		if (private.p_kstackflag != PDA_CURINTSTK)
			rt_unpin_thread(old_rtpin);

		return;
	}
}
#endif /* SN0_POQ_WAR */


/*
 * Setup a per cpu patch area where we can copy an instruction for the
 * R10k mfhi workaround (PV 516598)
 * This is used in intrnorm to do the workaround.
 */
void
init_mfhi_war()
{
#if defined (R10000) && defined (R10000_MFHI_WAR)

	union mips_instruction	inst;
	rev_id_t                ri;

#define MFHI_WAR_INSTRUCTIONS	128	/* size of scache line */

	ri.ri_uint = get_cpu_irr();

	if (ri.ri_imp != C0_IMP_R10000)
		private.p_r10kwar_bits |= R10K_MFHI_WAR_DISABLE;

	private.p_mfhi_patch_buf = 
	    (__psunsigned_t)kmem_zalloc(MFHI_WAR_INSTRUCTIONS,
					VM_CACHEALIGN | VM_DIRECT);
	
	if (!private.p_mfhi_patch_buf) 
	    cmn_err_tag(131,CE_PANIC, "Unable to allocate memory for mfhi war");

	inst.word = 0;
	inst.r_format.opcode = spec_op;
	inst.r_format.func   = jr_op;
	inst.r_format.rs     = 26;	/* k0 */

	/*
	 * The first instruction of the patch buffer is a jr k0
	 */
	*(inst_t *)private.p_mfhi_patch_buf = inst.word;

#endif /*defined (R10000) && defined (R10000_MFHI_WAR) */
}

