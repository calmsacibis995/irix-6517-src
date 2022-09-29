/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.99 $"

#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "ksys/as.h"
#include "sys/buf.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "ksys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include <ksys/exception.h>
#include <sys/extacct.h>
#include "ksys/fdt.h"
#include "ksys/vfile.h"
#include "sys/kmem.h"
#include "sys/kusharena.h"
#include "limits.h"
#include "sys/lock.h"
#include "sys/numa.h"
#include "sys/proc.h"
#include "sys/prctl.h"
#include "sys/resource.h"
#include "sys/runq.h"
#include "sys/splock.h"
#include "sys/sat.h"
#include "sys/sema.h"
#include "ksys/vsession.h"
#include "sys/schedctl.h"
#include "sys/sched.h"
#include "sys/space.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/timers.h"
#include "sys/var.h"
#include "sys/vnode.h"
#include "ksys/vproc.h"
#include "os/proc/pproc_private.h"
#include "fs/procfs/prsystm.h"	/* prawake() */
#include "ksys/vm_pool.h"
#ifdef R10000
#include "sys/hwperfmacros.h"
#endif
#include <ksys/xthread.h>
#include <sys/kthread.h>
#include <sys/frs.h>
#include <os/numa/pm.h>
#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /*_SHAREII*/

extern void gfx_shalloc(void);
extern void gfx_shfree(void);

/*
 * File contains:
 *
 */
static void dosigvecsync(void);
static void dosigsync(void);

static void uthread_divest(uthread_t *, int, int);

/*
 * share group routines
 */

static zone_t	*pshare_zone;		/* process shared struct */
static zone_t	*uthread_exzone;	/* exception structs */
static zone_t	*uthread_zone;		/* uthread structs */


void
pshare_init(void)
{
	pshare_zone = kmem_zone_init(sizeof(pshare_t), "pshare");
	/* XXX why the extra 0x100? */
        uthread_exzone = kmem_zone_init(sizeof(exception_t)+0x100, "exception");
        uthread_zone = kmem_zone_init(sizeof(struct uthread_s), "uthread");
}

/*
 * allocshaddr - allocate a process sharing structure for caller
 *
 * Always returns successfully.
 */
void
allocpshare(void)
{
	pshare_t *ps;
	uthread_t *ut = curuthread;
	proc_proxy_t *prxy = ut->ut_pproxy;

	ASSERT(prxy->prxy_utshare == NULL);
	ASSERT(!IS_SPROC(prxy));

	/* not yet initialized alloc room for shaddr struct and open files */
	ps = (pshare_t *) kmem_zone_alloc(pshare_zone, KM_SLEEP);

	/* init shadowed versions of share group resources */
	ps->ps_cmask = ut->ut_cmask;

	/*
	 * Grab share group's references on current/root directory.
	 */
	if (ps->ps_rdir = ut->ut_rdir) {
		ASSERT(ps->ps_rdir->v_count >= 1);
		VN_HOLD(ps->ps_rdir);
	}

	if (ps->ps_cdir = ut->ut_cdir) {
		ASSERT(ps->ps_cdir->v_count >= 1);
		VN_HOLD(ps->ps_cdir);
	}

	/* init shared directory update sema */
	init_mutex(&ps->ps_fupdsema, MUTEX_DEFAULT, "pshd", current_pid());

	/* init misc update lock */
	init_spinlock(&ps->ps_rupdlock, "pshr", current_pid());

	/* initialize stuff for single-threading single-stepping */
	init_mutex(&ps->ps_stepmutex, MUTEX_DEFAULT, "pshstepm", current_pid());
        sv_init(&ps->ps_stepsv, SV_DEFAULT, "pstepsv");
        ps->ps_stepflags = 0;
	ps->ps_steput = NULL;

	sigfillset(&ps->ps_sighold);

	/*
	 * The parent shares all attributes (except signal hold mask).
	 * Sync process scope scheduling attrs.
	 */
	prxy->prxy_shmask = PR_THREADS | (PR_SALL & ~PR_SPROC);
	if ((prxy->prxy_sched.prs_policy = ut->ut_policy) != SCHED_TS) {
		prxy->prxy_sched.prs_priority = UT_TO_KT(ut)->k_basepri;
	}
	prxy->prxy_utshare = ps;

	gfx_shalloc();
}

void
detachpshare(proc_proxy_t *prxy)
{
	pshare_t *ps = prxy->prxy_utshare;

	ASSERT(ps);

	/*
	 * give back share group's references on things like
	 * current directory
	 */
	if (ps->ps_rdir) {
		ASSERT(ps->ps_rdir->v_count >= 1);
		VN_RELE(ps->ps_rdir);
	}
	if (ps->ps_cdir) {
		ASSERT(ps->ps_cdir->v_count >= 1);
		VN_RELE(ps->ps_cdir);
	}

	mutex_destroy(&ps->ps_fupdsema);
	mutex_destroy(&ps->ps_stepmutex);
	sv_destroy(&ps->ps_stepsv);
	spinlock_destroy(&ps->ps_rupdlock);

	prxy->prxy_shmask = 0;
	prxy->prxy_utshare = NULL;

	kmem_zone_free(pshare_zone, ps);

	gfx_shfree();
}

/*
 * kstack_page_alloc(ut) 
 *	Allocates kernel stack pages for uthread ut.
 *	Returns 1 for success, 0 otherwise.
 *	Fills in kernel stack pde entries in ut->ut_kstkpgs[0..1]
 *
 *	Tlb wired entries use:
 *	<KSTKSIZE>	map kernel stack at &exp
 *	<rest> ? map kpdeseg entries for user to reduce double tlb misses
 */
static int
kstack_page_alloc(
	uthread_t	*ut)
{
	pde_t	*pdptr;
	pfd_t	*pfd;

	for (;;) {
		if (reservemem(GLOBAL_POOL, KSTKSIZE, KSTKSIZE, KSTKSIZE)) {
			nomemmsg("fork while allocating uarea");
			return(0);
		}

		/*
		 * WARNING
		 * We may not have a valid pm at this stage, so we
		 * cannot use the pm method to allocate memory.
		 */
		
		pdptr = &ut->ut_kstkpgs[KSTKIDX];
		if (pfd = 
#ifdef	_VCE_AVOIDANCE
			vce_avoidance 
			? pagealloc(colorof(KSTACKPAGE), 0)
			:
#endif
#ifdef	KSTACKPOOL
			kstack_alloc()
#else
			pagealloc(0, VM_UNSEQ)
#endif
			)
			break;

		unreservemem(GLOBAL_POOL, KSTKSIZE, KSTKSIZE, KSTKSIZE);
		setsxbrk();
		/* continue */
	}
	COLOR_VALIDATION(pfd,colorof(KSTACKPAGE),0,0);

	/* The kernel stack is something we want to dump on crashes.
	 */
	pfd_setflags(pfd, P_DUMP); 

	pdptr->pte.pg_pfn = pfdattopfn(pfd);
	/* 
	 * Don't want tlbmod faults on kstack;
	 * want global bit set.
	 */
	pg_setmod(pdptr);
	pg_setglob(pdptr);
	pg_setvalid(pdptr);
	pg_setcachable(pdptr);

	return(1);
}

/*
 * kstack_page_free(ut) 
 *	Frees up uthread ut's kernel stack pages.
 */
static void
kstack_page_free(
	uthread_t	*ut)
{
	int	i;

	for (i = 0; i < KSTKSIZE; i++) {
		ASSERT(pfntopfdat(ut->ut_kstkpgs[i].pte.pg_pfn)->
							pf_flags & P_DUMP);

		/* We don't need to dump this page anymore. */
		pfd_clrflags(pfntopfdat(ut->ut_kstkpgs[i].pte.pg_pfn), P_DUMP);
#if KSTACKPOOL
		if (i == KSTKIDX)
			kstack_free(pfntopfdat(ut->ut_kstkpgs[i].pte.pg_pfn));
		else
#endif
			pagefree(pfntopfdat(ut->ut_kstkpgs[i].pte.pg_pfn));
		ut->ut_kstkpgs[i].pgi = 0;
	}

	unreservemem(GLOBAL_POOL, KSTKSIZE, KSTKSIZE, KSTKSIZE);
}

void
uthread_destroy(uthread_t *ut)
{
	kthread_destroy(UT_TO_KT(ut));

	if (ut->ut_as.utas_tlbpid)
		tlbpid_memfree(ut->ut_as.utas_tlbpid);
#ifdef TFP
	if (ut->ut_as.utas_icachepid)
		icachepid_memfree(ut->ut_as.utas_icachepid);
#endif

	kstack_page_free(ut);
	if (ut->ut_watch)
		kmem_free(ut->ut_watch, sizeof(pwatch_t));

	destroy_bitlock(&ut->ut_as.utas_flag);
	sv_destroy(&ut->ut_pblock.s_wait);

	spinlock_destroy(&ut->ut_pollrotorlock);

	/* Frame scheduler */
	frs_thread_destroy(ut);

	ASSERT((__psunsigned_t)ut->ut_exception > K0BASE);
	kmem_zone_free(uthread_exzone, ut->ut_exception);
	kmem_zone_free(uthread_zone, ut);
}

/*
 * uthead_create - attach a uthread to a process
 *
 * p:		process to which to attach uthread (XXX change to proxy pointer)
 * sleepflag:	KM_SLEEP/KM_NOSLEEP
 * shmask:	sharing model mask
 * utp:		indirect return
 */
int
uthread_create(proc_t *p, uint_t shmask, uthread_t **utp, uthreadid_t ut_id)
{
	uthread_t *tut, *ut;
	short tid;
	proc_proxy_t *prxy = &p->p_proxy;
	extern kt_ops_t proc_ktops;     /* proc ops for kthreads */
	vasid_t         vasid;
	as_new_t        asn;
	as_newres_t     asnres;
	pm_t            *pm;
	int		error = 0;

	/*
	 * XXX	If shmask & PR_EV, check whether there's unused
	 * XXX	uthreads available?
	 */
	ut = kmem_zone_zalloc(uthread_zone, KM_SLEEP|VM_DIRECT);
	ut->ut_exception = kmem_zone_alloc(uthread_exzone, KM_SLEEP|VM_DIRECT);
	ut->ut_pproxy = prxy;

	if (!kstack_page_alloc(ut)) {
		kmem_zone_free(uthread_exzone, ut->ut_exception);
		kmem_zone_free(uthread_zone, ut);
		return EAGAIN;
	}

	/* ut->ut_prev = ut->ut_next = NULL; ...zalloc */
	ut->ut_as.utas_tlbpid = tlbpid_memalloc();
#ifdef TFP
	ut->ut_as.utas_icachepid = icachepid_memalloc();
#endif
	ut->ut_as.utas_tlbid = newtlbid();
	init_bitlock(&ut->ut_as.utas_flag, UTAS_LCK, "utaslck", 0L);
	ut->ut_as.utas_ut = ut; /* XXX temporary */

#ifdef DEBUG
	if ((__psunsigned_t)ut->ut_exception <= K0BASE)
		cmn_err(CE_PANIC, "up 0x%x <= 0x%x", ut->ut_exception, K0BASE);
#endif
	ASSERT(IS_KSEG0(ut->ut_exception)
#ifdef MH_R10000_SPECULATION_WAR
			|| (kvatopfn(ut->ut_exception) < pfn_lowmem)
#endif
	);

	init_sv(&ut->ut_pblock.s_wait, SV_DEFAULT, "pblwt", 0L);

	/* Frame Scheduler */
	frs_thread_init(ut);

	/* init underlying kthread */
        kthread_init(UT_TO_KT(ut),
#if EXTKSTKSIZE == 1
		(caddr_t)KSTACKPAGE-NBPC, 2*NBPC,
#else
		(caddr_t)KSTACKPAGE, NBPC,
#endif
		KT_UTHREAD, 0, 0, &proc_ktops);

	/*
	 * If this is a single-threaded process, the held-signal mask
	 * pointer points to the process (proxy) sigvec structure, and the
	 * child inherits the calling process (uthread) held-signal mask.
	 * If this is a multi-threaded process, set the held-signal mask
	 * pointer to the pshare, which holds a mask in which all signals
	 * are blocked.  The child will reset this to its prda (and will
	 * copy the parent uthread's held-signal mask) after it successfully
	 * locks its own prda.
	 */
	if (shmask & PR_THREADS) {
		ASSERT(prxy->prxy_utshare);
		ut->ut_sighold = &prxy->prxy_utshare->ps_sighold;
		ut->ut_flags = UT_PTHREAD;
		ut->ut_update = UTSYNCFLAGS;
	} else {
		ut->ut_sighold = &p->p_sigvec.sv_sighold;
		if (curuthread)
			*ut->ut_sighold = *curuthread->ut_sighold;
	}

	*utp = ut;
	ut->ut_vproc = PROC_TO_VPROC(p);
	ut->ut_proc = p;
	ut->ut_gstate = GANG_UNDEF;
	ut->ut_gbinding = CPU_NONE;
	/* need here if vas_new fails and uthread_destroy is called */
	init_spinlock(&ut->ut_pollrotorlock, "pollrotor", 0);

	/*
	 * Warning: AS_NULL isn't NULL
	 */
	AS_NULL(&ut->ut_asid);

	if (!(shmask & PR_THREADS)) {
		/*
		 * fast path for non MT creates.
		 */
		prxy->prxy_threads = ut;
		prxy->prxy_nthreads = prxy->prxy_nlive = 1;
		return 0;
	}

	/*
	 * initialize AS module
	 * For fork() we get a new AS, for sproc() we join our parent's
	 * -- but for uthread-create?
	 */
	as_lookup_current(&vasid);

	asn.as_utas = &ut->ut_as;
	asn.as_op = AS_UTHREAD;

	if (error = VAS_NEW(vasid, &asn, &asnres)) {
		uthread_destroy(ut);
		return error;
	}
	ut->ut_asid = asnres.as_casid;
	/* set up affinity link */
	pm = asnres.as_pm;
	ASSERT(pm);
	kthread_afflink_new(UT_TO_KT(ut), UT_TO_KT(curuthread), pm);
	aspm_releasepm(pm);

	uscan_update(prxy);

	/*
	 * If a jobcontrol stop is going on, wait for that to complete.
	 */
	while (prxy->prxy_flags & PRXY_CREATWT) {
		create_wait(prxy);
		uscan_update(prxy);
	}

	/*
	 * Pass on the watchpoint list to child only in pthread case.
	 * Pick an uthread from the group to act as parent.
	 */
	if (shmask & PR_THREADS) {
		passwatch(prxy_to_thread(prxy), ut);
	}

	/*
	 * If process is exitting/execing, don't create any more uthreads.
	 * The uscan lock doesn't protect prxy_flags, but the
	 * PRXY_EXIT bit will be set before the exitter acquires
	 * the uscan lock to send each uthread a signal.
	 */
	if (prxy->prxy_flags & (PRXY_EXIT|PRXY_EXEC)) {
		uscan_unlock(prxy);
		uthread_divest(ut, 0, 0);
		uthread_destroy(ut);
		return EAGAIN;
	}
	/*
	 * prxy_utidlow is always less than or equal to the lowest
	 * unused uthread id.  Note that the high tid bits reflect
	 * the cell id on multi-cell systems.
	 */
	if (ut_id == UT_ID_NULL) {
		tid = prxy->prxy_utidlow;
again:
                uscan_forloop(prxy, tut) {
			if (tut->ut_id == tid) {
				tid++;
				goto again;
			}
		}
		prxy->prxy_utidlow = tid + 1;
	} else {
		tid = ut_id;

                uscan_forloop(prxy, tut) {
			if (tut->ut_id == tid) {
				uscan_unlock(prxy);
				uthread_divest(ut, 0, 0);
				uthread_destroy(ut);
				return (EBUSY);
			}
		}
		if (tid == prxy->prxy_utidlow)
			prxy->prxy_utidlow++;
	}
	ut->ut_id = tid;

	prxy->prxy_utidlow = tid + 1;
        prxy->prxy_nthreads++;
	atomicAddInt(&prxy->prxy_nlive, 1);

	/*
	 * If the process is in the middle of a joint stop or has 
	 * completed one, make sure the child can not go back to user
	 * space. 
	 */
	if (prxy->prxy_flags & (PRXY_JSARMED | PRXY_JSTOPPED)) {
		int s, ss;

		ss = ut_lock(ut);
		s = prxy_stoplock(prxy);
		if (prxy->prxy_flags & PRXY_JSARMED) {
			ut->ut_flags |= ((prxy->prxy_flags & PRXY_PSTOP) ?
						UT_PRSTOPX : UT_PRSTOPJ);
		} else if (prxy->prxy_flags & PRXY_JSTOPPED) {
			/*
			 * Could copy the parent's prstopbits, but do
			 * not want to risk PIOCTHREADs on the parent
			 * changing those bits.
			 */
			ut->ut_flags |= UT_PRSTOPX;
		}
		prxy_stopunlock(prxy, s);
		ut_unlock(ut, ss);
	}
	ASSERT(ut->ut_prev == NULL);
	ut->ut_next = prxy->prxy_threads;
	if (ut->ut_next)
		ut->ut_next->ut_prev = ut;
	prxy->prxy_threads = ut;

	uscan_unlock(prxy);

	return 0;
}

/*
 * Set up separate signal mask for the uthread.
 */
void
uthread_setsigmask(uthread_t *ut, k_sigset_t *newset)
{
	int s;

	ASSERT(ut->ut_prda);

	sigemptyset(&ut->ut_prda->t_sys.t_hold);

	s = ut_lock(ut);
	sigorset(&ut->ut_prda->t_sys.t_hold, newset);
	ut->ut_sighold = &ut->ut_prda->t_sys.t_hold;
	ut_unlock(ut, s);

#if	(_MIPS_SIM == _ABIO32)
	/*
	 * The special version of sigprocmask() in libc uses T_HOLD_KSIG_O32
	 * to determine if the undelying kernel is using the old or new
	 * definition of k_sigset_t.
	 */
	ut->ut_prda->t_sys.t_flags |= T_HOLD_VALID | T_HOLD_KSIG_O32;
#else
	ut->ut_prda->t_sys.t_flags |= T_HOLD_VALID;
#endif
}

/*
 * uthread is exitting or being tossed -- shed its possessions;
 */
static void
uthread_divest(uthread_t *ut, int flags, int freesat)
{
	vasid_t vasid;
	as_deletespace_t asd;
	kusharena_t *kusp;

	/* exit nanothread group */
	if (((kusp = ut->ut_sharena) != NULL) && ut->ut_prda) {

		atomicAddUint((uint_t *)&(kusp->nallocated), -1);

		if (ut->ut_prda->sys_prda.prda_sys.t_nid != NULL_NID) {
			atomicClearUint64(
				&kusp->rbits[ut->ut_prda->t_sys.t_nid /
						bitsizeof(uint64_t)],
				1LL<<(ut->ut_prda->t_sys.t_nid % bitsizeof(uint64_t)));
		}
	}

	/*
	 * Drop any signals that were delivered but not acted on.
	 * I think this can only happen if the thread is being traced.
	 */
	sigqfree(&ut->ut_sigpend.s_sigqueue);

	if (ut->ut_curinfo)
		sigqueue_free(ut->ut_curinfo);

	if (ut->ut_polldat)
		(void) kern_free(ut->ut_polldat);

	if (ut->ut_cred)
		crfree(ut->ut_cred);

#if R10000
	if (ut->ut_cpumon) {
		hwperf_disable_counters(ut->ut_cpumon);
		hwperf_cpu_monitor_info_free(ut->ut_cpumon);
		ut->ut_cpumon = NULL;
	}
#endif

	/*
	 * Release directories --
	 * ut_cdir could be null if the uthread creation aborted.
	 */
 	if (ut->ut_cdir) {
		VN_RELE(ut->ut_cdir);

		/*
		 * Now that ut->ut_cdir is stale, reassign it a safe value
		 * in case someone looks while we sleep.  Don't hold rootdir
		 * because it would need to be released and it won't go away
		 * anyway.
		 */
		ut->ut_cdir = rootdir;
	}

 	if (ut->ut_rdir) {
		VN_RELE(ut->ut_rdir);
		ut->ut_rdir = NULLVP;
	}

	/*
	 * wake up any /proc processes waiting on us
	 */
	if (ut->ut_flags & UT_TRWAIT) {
		ut_flagclr(ut, UT_TRWAIT);
		prawake();
	}


	/*
	 * If this is a multi-threaded process, ut->ut_sighold is pointing
	 * into its prda -- we'll have to fix this before we null the prda,
	 * because nfs actually calls assign_cursighold (which manipilates
	 * the held-signal mask).  We can get into the bowels of nfs via
	 * the VAS_DELETESPACE() call, below.
	 */
	if (ut->ut_prda && ut->ut_sighold == &ut->ut_prda->t_sys.t_hold) {
		/*
		 * The proc structure could already be gone by the time we
		 * get here -- can't point into it!
		 */
		sigfillset(&ut->ut_suspmask);
		ut->ut_sighold = &ut->ut_suspmask;
	}

	/*
	 * Must test whether asid has been installed -- could be error cleanup.
	 */
	if (!AS_ISNULL(&ut->ut_asid)) {
		kthread_afflink_unlink(UT_TO_KT(ut), &ut->ut_asid);
		asd.as_vrelvm_prda = ut->ut_prda;
		ut->ut_prda = 0;

		zapsegtbl(&ut->ut_as);
		ut->ut_as.utas_segtbl = NULL;
		ut->ut_as.utas_shrdsegtbl = NULL;

		asd.as_vrelvm_detachstk = (flags & UT_DETACHSTK)
#if USE_PTHREAD_RSA
					&& !ut->ut_sharena
#endif
					? 1 : 0;

		/* remove address space */
		if (ut == curuthread) {
			as_lookup_current(&vasid);
		} else {
			as_lookup(ut->ut_asid, &vasid);
			as_rele(vasid);
		}
		AS_NULL(&ut->ut_asid);

		asd.as_op = AS_DEL_EXIT;
		VAS_DELETESPACE(vasid, &asd, NULL);

		as_rele(vasid);
	}

	/*
	 * Clean up any SAT resources for this process.
	 * NO SAT CALLS BEYOND THIS POINT!
	 */
	if (freesat)
		_SAT_PROC_EXIT(ut);
}

static void uthread_run_exitfuncs(uthread_t *);

void
uthread_exit(uthread_t *ut, int flags)
{
	proc_proxy_t *prxy = ut->ut_pproxy;
        int pwait = 0, pollwake = 0, s;
	struct vnode *vp;

	if (ut->ut_ecblist)
		uthread_run_exitfuncs(ut);

	frs_thread_exit(ut);

	/*
	 * remove ut from shared list
	 */
	uscan_update(prxy);

	/* Resign as recipient of "special signal delivery".
	 */
	if (prxy->prxy_sigthread == ut) {
		prxy->prxy_sigthread = 0;
	}

	/* if this is a threaded process dumping core and this is the
	 * last uthread to exit, close the coredump vp and release it
	 */
	if (vp = prxy->prxy_coredump_vp) {
		/* If this is a multi-threaded process, we wait until
		 * the last thread is exiting before closing the vp,
		 * this way each thread can dump its info to the
		 * corefile individually.
		 */
		if (prxy->prxy_nthreads == 1) {
			/*REFERENCED*/
			int closerr;
			uscan_unlock(prxy);
			VOP_CLOSE(vp, FWRITE, L_TRUE, ut->ut_cred, closerr);
			VN_RELE(vp);
			uscan_update(prxy);
		}
	}

	/*
	 * ...but if process is being prheld, wait.
	 */
	s = prxy_hlock(prxy);
	while (prxy->prxy_hold) {
		prxy_nflagset(prxy, PRXY_WAIT);
		uscan_unlock(prxy);
		uscan_wait(prxy, s);
		uscan_update(prxy);
		s = prxy_hlock(prxy);
	}

	ASSERT(prxy->prxy_threads);
	ASSERT(ut->ut_prev || ut->ut_next || prxy->prxy_threads == ut);

	if (ut->ut_prev)
		ut->ut_prev->ut_next = ut->ut_next;
	if (ut->ut_next)
		ut->ut_next->ut_prev = ut->ut_prev;
	if (prxy->prxy_threads == ut)
		prxy->prxy_threads = ut->ut_next;

	if (prxy->prxy_utidlow > ut->ut_id)
		prxy->prxy_utidlow = ut->ut_id;

	ASSERT(prxy->prxy_nthreads > 0);
	prxy->prxy_nthreads--;
	prxy_hunlock(prxy, s);

	if ((prxy->prxy_flags & PRXY_LONEWT) && (prxy->prxy_nthreads == 1)) {
		prxy_flagclr(prxy, PRXY_LONEWT);
		lone_wake(prxy);
	}

	if ((prxy->prxy_flags & PRXY_JSARMED) && (ut->ut_flags & UT_HOLDJS)) {
		/*
		 * May have to account for myself. Safe to see whether
		 * I have been accounted without utlock since no one 
		 * can be switching that bit on me as I am not in the
		 * utlist anymore. Also, people who switch that bit
		 * need uscan_update.
		 */
		s = ut_lock(ut);
		ASSERT(ut->ut_flags & UT_HOLDJS);
		ut->ut_flags &= ~UT_HOLDJS;
		ut_unlock(ut, s);
		s = prxy_stoplock(prxy);
		pollwake = jsacct(prxy, 1, &pwait);
		prxy_stopunlock(prxy, s);
	}

	uscan_unlock(prxy);

	if (ut->ut_proc->p_trace && pollwake && (prxy->prxy_flags & PRXY_JPOLL))
		prpollwakeup(ut->ut_proc->p_trace, POLLIN|POLLPRI|POLLOUT|
							POLLRDNORM|POLLRDBAND);
	if (pwait)
		prawake();

	/* toss accumulated baggage */
	uthread_divest(ut, flags, 1);

	if (ut != curuthread) {
		ASSERT(!(flags & UT_VPROC_DESTROY));
		ut->ut_proc = NULL;	/* XXX */
		exitrunq(UT_TO_KT(ut));
#ifdef _SHAREII
		/* 
		** Tell Share the thread is exiting, so it can clean up
		** Share data in the uthread_t.
		** Must happen *after* the exitrunq() which assumes 
		** that data is still there.
		*/
		SHR_THREADEXIT(ut);
#endif /*_SHAREII*/
		uthread_destroy(ut);
		return;
	}

	/*
	 * We can't afford to sleep or be preempted from this point out.
	 */
	splhi();
	if (!(flags & UT_VPROC_DESTROY)) {
		VPROC_RELE(ut->ut_vproc);
		ut->ut_vproc = (vproc_t *) NULL;
	}
	checkfp(ut, 1);	/* release and toss FP registers */
	utexitswtch();
	ASSERT(0);
}

static void
syncdir(vnode_t **vpp,		/* pointer to process's c/r dir */
	vnode_t *svp)		/* vnode to share */
{
	if (svp && svp != *vpp) {
		if (*vpp) {
			/* remove old one */
			VN_RELE(*vpp);
		}
		VN_HOLD(svp);
		*vpp = svp;
	}
}

static void
dodsync(void)
{
	uthread_t *ut = curuthread;
	pshare_t *ps = ut->ut_pproxy->prxy_utshare;

	mutex_lock(&ps->ps_fupdsema, PZERO);
	syncdir(&ut->ut_rdir, ps->ps_rdir);
	syncdir(&ut->ut_cdir, ps->ps_cdir);
	mutex_unlock(&ps->ps_fupdsema);
}

/*
 * setpsync - arrange for all uthreads in a process to sync up
 */
void
setpsync(proc_proxy_t *prxy, int sflag)	/* which flag in ut_flags to set */
{
	uthread_t *ut;

	uscan_access(prxy);
	uscan_forloop(prxy, ut) {
		ut_updset(ut, sflag);
	}
	uscan_unlock(prxy);
}

/*
 * dosync - handle synchronization requests for various resources
 */
void
dousync(int tflag)
{
	extern void dosync(int);

	if (IS_SPROC(curuthread->ut_pproxy)) {
		dosync(tflag);
		return;
	}

	if (tflag & UT_UPDDIR) {
		dodsync();			/* cur/root dir */
		_SAT_PN_BOOK(-1, curuthread);	/* update cwd/crd */
		if (tflag & UT_SAT_CWD)		/* for audit trail */
			_SAT_UPDATE_RWDIR(SAT_CHDIR,curuthread);
		if (tflag & UT_SAT_CRD)
			_SAT_UPDATE_RWDIR(SAT_CHROOT,curuthread);
	}

#ifdef notneeded
	if (tflag & UT_UPDUID)
		doidsync();		/* uid/gid */
	if (tflag & UT_UPDULIMIT)
		doulimitsync();		/* ulimit */
	if (tflag & UT_UPDUMASK)
		doumasksync();		/* umask */
#endif
	if (tflag & UT_UPDSIGVEC)
		dosigvecsync();		/* sigvecs */
	if (tflag & UT_UPDSIG)
		dosigsync();		/* signals */
	if (tflag & UT_UPDCRED)
		credsync();		/* ut_cred */
}

/*
 * update umask
 */
void
credsync(void)
{
	uthread_t *ut = curuthread;
/*
	cmn_err(CE_CONT, "!pid %d ut 0x%x 0x%x [%d] 0x%x [%d]\n",
		UT_TO_PROC(ut)->p_pid, ut, ut->ut_cred, ut->ut_cred->cr_ref,
		UT_TO_PROC(ut)->p_cred, UT_TO_PROC(ut)->p_cred->cr_ref);
*/
	crfree(ut->ut_cred);
	ut->ut_cred = pcred_access(UT_TO_PROC(ut));
	crhold(ut->ut_cred);
	pcred_unaccess(UT_TO_PROC(ut));
}

/*
 * sync signals:  Process got signal(s) for which it couldn't find
 * a target uthread -- check whether any of them are of interest to
 * this uthread.
 */
static void
dosigsync(void)
{
	uthread_t *ut = curuthread;
	sigvec_t *sigvp;
	k_sigset_t signals;
	int sig;
	__uint64_t sigbit;

	/*
	 * See if there are any signals deposited in the shared signals
	 * bucket that this uthread can handle.  We don't need to hold
	 * a lock while looking at ut_sighold -- only this thread can
	 * change it.  Likewise, we don't have to do any more checking
	 * than whether the signal is being held -- if is a 'fatal'
	 * signal, it would have been delivered directly, and the
	 * uthread certainly isn't sigwaiting for the signal right now.
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);
	signals = sigvp->sv_sig;
	sigdiffset(&signals, ut->ut_sighold);
	if (sigisempty(&signals))
		goto out;
	VPROC_PUT_SIGVEC(curvprocp);

	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	signals = sigvp->sv_sig;
	sigdiffset(&signals, ut->ut_sighold);

	if (!sigisempty(&signals)) {
		for (sig = 1, sigbit = sigmask(1); sig < NSIG; ) {
			/*
			 * Note that we don't update sig and sigbit
			 * in the continuation portion of the for
			 * loop -- sigdeq will reset ut_sig if
			 * there's more than one siginfo queued.
			 */
			if (!sigbitismember(&signals, sigbit, sig)) {
				sig++;
				sigbit <<= 1;
				continue;
			}
			sigbitdelset(&sigvp->sv_sig, sigbit, sig);
			sigtouthread_common(ut, ut_lock(ut), sig, sigvp,
					    sigdeq(&sigvp->sv_sigpend,
						   sig, sigvp));
			/*
			 * refresh in case sigdeq reset signal bit
			 */
			signals = sigvp->sv_sig;
			sigdiffset(&signals, ut->ut_sighold);
		}
	}
out:
	/*
	 * Reinstate UT_SHSIGSYN bit if there are any signals still unclaimed.
	 */
	if (!sigisempty(&sigvp->sv_sig)) {
		ut_updset(ut, UT_UPDSIG);
	}

	VPROC_PUT_SIGVEC(curvprocp);
}

/*
 * sync sigvecs -- process sigvec bits changed, so might need to
 * toss some pending signals?
 */
static void
dosigvecsync(void)
{
	uthread_t *ut = curuthread;
	sigvec_t *sigvp;
	k_sigset_t ignored;
	int sig, s;
	__uint64_t sigbit;

	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);
	s = ut_lock(ut);

	/*
	 * Drop any pending signals that are now to be ignored.
	 * If there were any signals that had been delivered to this
	 * uthread but have been tossed, toss their corresponding siginfos.
	 *
	 * XXX	This may only be possible for job-control signals, as
	 * XXX	no other signals are delivered to uthreads in a
	 * XXX	multi-threaded process when the uthread isn't executing
	 * XXX	in the kernel -- and these signals should be handled before
	 * XXX	the uthread leaves the kernel.
	 */
	ignored = ut->ut_sig;
	sigandset(&ignored, &sigvp->sv_sigign);
	sigdiffset(&ut->ut_sig, &sigvp->sv_sigign);

	if (ut->ut_sigqueue && !sigisempty(&ignored)) {
		for (sig = 1, sigbit = sigmask(1);
		     sig < NSIG;
		     sig++, sigbit <<= 1) {
			if (sigbitismember(&ignored, sigbit, sig))
				sigdelq(&ut->ut_sigqueue, sig, sigvp);
		}
	}

	ut_unlock(ut, s);
	VPROC_PUT_SIGVEC(curvprocp);
}

/*
 * This guarantees no uthread may leave the list, ie it guarantees 
 * nthreads will not decrease till the call to uscan_rele.
 */
void
uscan_hold(proc_proxy_t *prxy)
{
	int s;

	ASSERT(prxy->prxy_hold >= 0);
	s = prxy_hlock(prxy);
	prxy->prxy_hold++;
	prxy_hunlock(prxy, s);
	ASSERT(prxy->prxy_hold > 0);
}

void
uscan_rele(proc_proxy_t *prxy)
{
	int s, dowake=0;

	ASSERT(prxy->prxy_hold > 0);
	s = prxy_hlock(prxy);
	if ((--prxy->prxy_hold == 0) && (prxy->prxy_flags & PRXY_WAIT)) {
		prxy_nflagclr(prxy, PRXY_WAIT);
		dowake = 1;
	}
	ASSERT(prxy->prxy_hold >= 0);
	prxy_hunlock(prxy, s);
	if (dowake)
		uscan_wake(prxy);

}

/*
 * uthread_lookup
 * Takes a reference on a uthread to prevent it from exiting.
 * Returns a uthread on success and NULL on failure.
 */
uthread_t *
uthread_lookup(
	register proc_proxy_t *prxy,
	register uthreadid_t ut_id)
{
	uthread_t *ut;

	uscan_access(prxy);
	uscan_forloop(prxy, ut) {
		if (ut->ut_id == ut_id) {
			uscan_hold(prxy);
			break;
		}
	}
	uscan_unlock(prxy);

	return (ut);
}

/*
 * uthread_apply - apply a routine to every/ uthread in a process
 *	If the routine returns non-zero, stop the scan and return.
 *	Returns non-zero if aborted by the called routine, zero otherwise.
 */
int
uthread_apply(
	register proc_proxy_t *prxy,
	register uthreadid_t ut_id,
	register int (*pfunc)(uthread_t *, void *),
	register void *arg)
{
	register uthread_t *ut;
	register int tmp;

	uscan_access(prxy);

	if (ut_id == UT_ID_NULL) {
		tmp = 0;	/* default success */
                uscan_forloop(prxy, ut) {
			if (tmp = (*pfunc)(ut, arg))
				break;
		}
	} else {
		tmp = ESRCH;	/* default failure */
                uscan_forloop(prxy, ut) {
			if (ut->ut_id == ut_id) {
				tmp = (*pfunc)(ut, arg);




				break;
			}
		}
	}

	uscan_unlock(prxy);

	return(tmp);
}

/*
 * Kill all the uthreads on the local cell.
 * Each cell will call this for its own uthreads.
 */
static int
kill_uthreads(uthread_t *ut, void *arg)
{
	ushort_t tid = *(ushort_t *)arg;

	if (ut->ut_id != tid && !(ut->ut_flags & UT_INACTIVE)) {
		sigtouthread(ut, SIGKILL, (k_siginfo_t *)NULL);
	}

	return 0;
}

/*
 * Send kill signals to all the local uthreads, and wait for them
 * to exit.
 */
void
uthread_reap(void)
{
	proc_proxy_t	*prxy = curuthread->ut_pproxy;

	/*
	 * proxy's exit bit is set, so no more uthreads can be added
	 * to the uthread list
	 */
	(void)uthread_apply(curuthread->ut_pproxy, UT_ID_NULL,
			    kill_uthreads, (void *)&curuthread->ut_id);

	for ( ; ; ) {
#ifdef not_needed_yet
		uthread_t	*ut;
again:
		uscan_update(prxy);
                uscan_forloop(prxy, ut) {
			if (!(ut->ut_flags & UT_INACTIVE))
				continue;

			if (ut->ut_prev)
				ut->ut_prev->ut_next = ut->ut_next;
			if (ut->ut_next)
				ut->ut_next->ut_prev = ut->ut_prev;

			if (prxy->prxy_threads == ut)
				prxy->prxy_threads = ut->ut_next;

			ASSERT(prxy->prxy_nthreads > 0);
			prxy->prxy_nthreads--;
			uscan_unlock(prxy);

			uthread_divest(ut, 0, 1);
			exitrunq(UT_TO_KT(ut));
			uthread_destroy(ut);
			goto again;
		}
#else
		uscan_update(prxy);
#endif
		/*
		 * The other uthreads havn't detached yet.
		 */
		if (prxy->prxy_nthreads > 1) {
			prxy_flagset(prxy, PRXY_LONEWT);
			lone_wait(prxy);	/* releases uscan lock */
		} else {
			break;
		}
	}

	/* Zap the default job.
	 */
	if (prxy->prxy_sched.prs_job) {
		job_uncache(prxy->prxy_sched.prs_job);
		prxy->prxy_sched.prs_job = 0;
	}
	uscan_unlock(prxy);
}

extern zone_t *ecb_zone;

/*
 * register a callback function to be called when the specified
 * process exits.
 */
int
uthread_add_exit_callback(
	int flags,
	void (* func)(void *),
	void *arg)
{
	uthread_t *ut = curuthread;
	struct exit_callback *ecb;

	/* No locking is required to walk this list, since this
	 * routine is only called by curuthread.
	 */
	if (flags & ADDEXIT_CHECK_DUPS)
		for (ecb = ut->ut_ecblist; ecb; ecb = ecb->ecb_next)
			if (ecb->ecb_func == func)
				return EALREADY;

	ecb = kmem_zone_alloc(ecb_zone, KM_SLEEP);

	ecb->ecb_func = func;
	ecb->ecb_arg = arg;

	/* To insert ecb's onto this list, we use atomicPush,
	 * which does not have to change the spl level.
	 */
	(void) atomicPush(&ut->ut_ecblist, ecb, ecb);

	return 0;
}

/* execute all of the callbacks registered for curuthread */
static void
uthread_run_exitfuncs(uthread_t *ut)
{
	struct exit_callback *ecb;

	/*
	 * We don't need to protect the exitfunc list with any locking -
	 * the uthread is exitting, and only it can queue functions.
	 */
	ecb = ut->ut_ecblist;
	while (ecb = ut->ut_ecblist) {
		ut->ut_ecblist = ecb->ecb_next;

		ecb->ecb_func(ecb->ecb_arg);
		kmem_zone_free(ecb_zone, ecb);
	}
}

void
dump_timers_to_proxy(uthread_t *ut)
{
	kthread_t *kt = UT_TO_KT(ut);
	proc_proxy_t *prxy = ut->ut_pproxy;
	acct_timers_t *prxy_accum = &prxy->prxy_exit_accum;
	timespec_t utimespec;
	timespec_t stimespec;
	timespec_t bio_timespec;
	timespec_t physio_timespec;
	timespec_t rq_timespec;
	struct timeval tval1, tval2;
	int s;

	ktimer_read(kt, AS_USR_RUN, &utimespec);
	ktimer_read(kt, AS_SYS_RUN, &stimespec);
	ktimer_read(kt, AS_BIO_WAIT, &bio_timespec);
	ktimer_read(kt, AS_PHYSIO_WAIT, &physio_timespec);
	ktimer_read(kt, AS_RUNQ_WAIT, &rq_timespec);

	TIMESPEC_TO_TIMEVAL(&utimespec, &tval1);
	TIMESPEC_TO_TIMEVAL(&stimespec, &tval2);

	s = prxy_lock(prxy);

	timevaladd(&prxy->prxy_ru.ru_utime, &tval1);
	timevaladd(&prxy->prxy_ru.ru_stime, &tval2);

	prxy_accum->ac_utime += TIMESPEC_TO_ACCUM(&utimespec);
	prxy_accum->ac_stime += TIMESPEC_TO_ACCUM(&stimespec);
	prxy_accum->ac_bwtime += TIMESPEC_TO_ACCUM(&bio_timespec);
	prxy_accum->ac_rwtime += TIMESPEC_TO_ACCUM(&physio_timespec);
	prxy_accum->ac_qwtime += TIMESPEC_TO_ACCUM(&rq_timespec);

#ifdef R10000
	if (ut->ut_cpumon) {
		if (ut == curuthread) {
			STOP_HWPERF_COUNTERS(ut);
		}
		prxy_unlock(prxy, s);

		ASSERT(prxy->prxy_cpumon);
		ADD_HWPERF_COUNTERS(ut->ut_cpumon, prxy->prxy_cpumon);
	} else
#endif
	prxy_unlock(prxy, s);
}

/*
 * Accumulate accounting from uthread. This is used to aggregate
 * uthread stats for a process.
 */ 
int
uthread_acct(uthread_t *ut, void *arg)
{
	ut_acct_t	*acct = arg;

	INCR_IOCH(acct->ua_ioch, ut->ut_acct.ua_ioch);	/* ioch pegs at max */

#define ACCT_ACCUM(ua)	acct->ua += ut->ut_acct.ua

	ACCT_ACCUM(ua_mem);
	ACCT_ACCUM(ua_bread);
	ACCT_ACCUM(ua_bwrit);
	ACCT_ACCUM(ua_sysc);
	ACCT_ACCUM(ua_syscr);
	ACCT_ACCUM(ua_syscw);
	ACCT_ACCUM(ua_syscps);
	ACCT_ACCUM(ua_sysci);
	ACCT_ACCUM(ua_graphfifo);
	ACCT_ACCUM(ua_tfaults);
	ACCT_ACCUM(ua_vfaults);
	ACCT_ACCUM(ua_ufaults);
	ACCT_ACCUM(ua_kfaults);

	return 0;
}

#if !CELL
char
get_current_abi()
{
	return(curuthread ? curuthread->ut_pproxy->prxy_abi : 0);
}

uthreadid_t
get_current_utid()
{
	return(0);
}

void
get_current_flid(flid_t *flidp)
{
	ASSERT(curuthread);
	*flidp = curuthread->ut_flid;
}
#else

#include <ksys/cell/mesg.h>

char
get_current_abi()
{
	if (curuthread == NULL) {
		trans_t	*tr;

		if ((tr = mesg_thread_info()) == NULL)
			return(0);
		else
			return(tr->tr_abi);
	}
	return(curuthread->ut_pproxy->prxy_abi);
}

uthreadid_t
get_current_utid()
{
	trans_t	*tr;

	if ((tr = mesg_thread_info()) == NULL)
		return(0);
	else
		return(tr->tr_utid);
}

#endif /* CELL */

#if PTHREAD_TRACING

#define PTT_SIZE  1024
#define PTT_MASK  1023

struct ptt_entry {
        __int32_t ptt_info1;
        __int32_t ptt_info2;
        __int64_t ptt_info3;
        void *ptt_target;
	uthread_t *ptt_doer;
};

#define PTT_JSCOUNT	0x10000000
#define PTT_PXSET	0x20000000
#define PTT_PXCLEAR	0x30000000
#define PTT_UNLOCK	0x40000000

int ptt_index;
struct ptt_entry ptt_table[PTT_SIZE];

int 
pthread_tracing_jscount(
       proc_proxy_t *px,
       int inc,
       int line,
       char *file)
{
       int v = atomicAddInt(&(px)->prxy_jscount, inc);
       int ix = atomicAddInt(&ptt_index, 1) & PTT_MASK;

       ptt_table[ix].ptt_info1 = PTT_JSCOUNT | 
	                         ((line & 0xfff) << 16) | 
	                         (inc & 0xffff);
       ptt_table[ix].ptt_info2 = v;
       ptt_table[ix].ptt_info3 = (__int64_t) file;
       ptt_table[ix].ptt_target = px;
       ptt_table[ix].ptt_doer = curuthread;
       ASSERT(v >= 0);
       return (v);
}

uint 
pthread_tracing_pxset(
       proc_proxy_t *px,
       uint flags,
       int line,
       char *file)
{
       uint v = bitlock_set((uint *)(&(px)->prxy_flags), PRXY_LOCK, flags);
       int ix;

       if (flags & (PRXY_PWAIT | PRXY_PSTOP | 
		    PRXY_JSTOP | PRXY_JPOLL | PRXY_JSARMED) == 0)
	       return (v);
       ix = atomicAddInt(&ptt_index, 1) & PTT_MASK;

       ptt_table[ix].ptt_info1 = PTT_PXSET | 
	                         ((line & 0xfff) << 16) | 
	                         (flags & 0xffff);
       ptt_table[ix].ptt_info2 = v;
       ptt_table[ix].ptt_info3 = (__int64_t) file;
       ptt_table[ix].ptt_target = px;
       ptt_table[ix].ptt_doer = curuthread;
       return (v);
}

uint 
pthread_tracing_pxclear(
       proc_proxy_t *px,
       uint flags,
       int line,
       char *file)
{
       uint v = bitlock_clr((uint *)(&(px)->prxy_flags), PRXY_LOCK, flags);
       int ix;

       if (flags & (PRXY_PWAIT | PRXY_PSTOP | 
		    PRXY_JSTOP | PRXY_JPOLL | PRXY_JSARMED) == 0)
	       return (v);
       ix = atomicAddInt(&ptt_index, 1) & PTT_MASK;

       ptt_table[ix].ptt_info1 = PTT_PXCLEAR | 
	                         ((line & 0xfff) << 16) | 
	                         (flags & 0xffff);
       ptt_table[ix].ptt_info2 = v;
       ptt_table[ix].ptt_info3 = (__int64_t) file;
       ptt_table[ix].ptt_target = px;
       ptt_table[ix].ptt_doer = curuthread;
       return (v);
}

void 
pthread_tracing_unlock(
       uthread_t *ut,
       int s,
       int line)
{
       int ix;

       if ((ut->ut_pproxy->prxy_flags & (PRXY_PWAIT | PRXY_PSTOP | 
					 PRXY_JSTOP | PRXY_JPOLL | 
					 PRXY_JSARMED) == 0) &&
	   (ut->ut_pproxy->prxy_jscount == 0)) {
               kt_unlock(UT_TO_KT(ut),s);
	       return;
       }
       ix = atomicAddInt(&ptt_index, 1) & PTT_MASK;

       ptt_table[ix].ptt_info1 = PTT_UNLOCK | line;
       ptt_table[ix].ptt_info2 = ut->ut_flags;
       ptt_table[ix].ptt_info3 = ut->ut_id;
       ptt_table[ix].ptt_target = ut;
       ptt_table[ix].ptt_doer = curuthread;
       kt_unlock(UT_TO_KT(ut),s);
       return;
}
#endif
