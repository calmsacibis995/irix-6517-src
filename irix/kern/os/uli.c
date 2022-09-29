#ifdef ULI

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* These functions provide support for user level interrupts. A user
 * level interrupt is an interrupt for which the handler function resides
 * in a user process. In order to execute this handler function, a 5th
 * fundamental system mode has been created. The 4 original modes are
 * user, kernel, interrupt and idle. The new mode is called, (not
 * surprisingly) user level interrupt or ULI mode. ULI mode executes in
 * usermode, so the processor may not execute privileged instructions and
 * is limited to the address space of a user process. However it is a
 * true interrupt mode in that it cannot be preempted and may only be
 * interrupted by a higher priority interrupt. ULI mode may not
 * transition into kernel mode since to do so would violate the
 * non-preemptability of the kernel (process A is interrupted while
 * executing in the kernel, the interrupt is a ULI so we call into
 * process B, process B takes a fault of some sort, enters the kernel,
 * and process B has now preempted process A in the kernel). Thus all
 * faults are fatal to the ULI handler, except for tlb faults. The ULI
 * process is placed permanently in segment table mode so that tlb faults
 * can be handled entirely in locore without needing to get onto the
 * kernel stack.
 * 
 * Entry into ULI mode involves setting up the address space of the ULI
 * process (the process whose address space contains the handler
 * function). This generally involves some tlb manipulations and setting
 * of curuthread so the tlbmiss code knows whose mappings to load. Also,
 * kstackflag is set to reflect the new mode.
 * 
 * Return from the ULI handler is accomplished via a special syscall
 * feature. Ordinarily system calls are disallowed in the ULI handler
 * again, to prevent violation of the non-preemptability of the kernel.
 * but there is a special kernel call facility which executes on the
 * interrupt stack and provides any access to kernel calls which is
 * required by the ULI handler. This kernel call is accomplished via a
 * syssgi call, but is diverted at the systrap level and handled
 * specially. One of the functions provided by this facility is returning
 * from the ULI handler. The ULI handler runs in usermode and thus has no
 * way of returning to its calling point on the interrupt stack. A
 * syscall returns the processor to kernel mode, after which we take a
 * longjmp back to the interrupt stack to continue with other interrupt
 * processing.
 * 
 * ULI mode may be interrupted out of by a higher priority interrupt.
 * When a nested interrupt occurs, the tlb and any other state which was
 * modified when entering ULI mode is restored to its previous state.
 * This is done so that any assumptions that the nested interrupt handler
 * may make about the state of the system still hold true. The nested
 * interrupt handler should not be able to tell that it interrupted a
 * ULI, only that it interrupted another interrupt handler. Similarly,
 * when the aforementioned kernel call facility is used to make kernel
 * calls on the interrupt stack from ULI mode, the previous system state
 * is restored before making the kernel call, and returned to the ULI
 * state before returning to the ULI.
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <ksys/as.h>
#include <sys/uli.h>
#include <ksys/uli.h>
#include <sys/sema.h>
#include <sys/errno.h>
#include <sys/runq.h>
#include <sys/kmem.h>
#include <sys/reg.h>
#include <sys/sbd.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/syssgi.h>
#include <sys/vmereg.h>
#include <sys/edt.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/rtmon.h>
#include <ksys/exception.h>
#include <ksys/vproc.h>
#include <sys/debug.h>

lock_t ulimutex;
#define ULILOCK() mp_mutex_spinlock(&ulimutex)
#define ULIUNLOCK(s) mp_mutex_spinunlock(&ulimutex, s)

/* eprintf: report on errors */

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* dprintf: general info */

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#ifdef DEBUG
/* idbg variables */
int uli_during_swtch, initsgtbl_uli, real_invalid, bogus_invalid;
int intuliframe, nested_int_during_uli;
int real_nested_int_during_uli, nested_uli;
int nested_return_to_uli, defer_resched, exc_keruliframe;
int segtblrace;
#endif

static struct uli *uli_table[MAX_ULIS];
static int first_free_uli;

#ifdef TFP
extern k_machreg_t get_config(void);
#endif

#ifdef TFP
#define PRESERVEBITS (SR_KPSMASK | SR_UPSMASK | SR_RE | SR_XX | SR_UX | SR_KU)
#define SETBITS (SR_EXL | SR_IE)
#endif
#if defined(R4000) || defined(R10000)
#define PRESERVEBITS (SR_KX | SR_UX | SR_KSU_MSK | SR_RE)
#define SETBITS (SR_EXL | SR_IE)
#endif

#if defined (PSEUDO_BEAST)
#define PRESERVEBITS (SR_KSU_MSK | SR_RE)
#define SETBITS (SR_EXL | SR_IE)
#endif /*PSEUDO_BEAST*/
#define ULISR(sr) ((sr & PRESERVEBITS) | SETBITS)

#ifdef ULI_TSTAMP1
int uli_tstamps[TS_MAX];
int uli_saved_tstamps[TS_MAX];
#endif
#ifdef ULI_TSTAMP
int tstamp_cpu;
#endif

static void uli_exit(void);

void
uli_init()
{
    spinlock_init(&ulimutex, "ulimutex");
}

/* ULI semaphore routines. We don't use the generic semaphores
 * since we have the peculiarity here that we want the count to
 * max out at 1 rather than 0 or unbounded
 */
static void
uli_sema_init(struct ulisema *sp)
{
    sp->count = 0;
    spinlock_init(&sp->mutex, "uli->mutex");
    sv_init(&sp->sv, 0, "uli->sv");
}

static void
uli_sema_destroy(struct ulisema *sp)
{
    /* wait for anyone who might be accessing this sema
     * before nuking it. The race here is with uli_sleep. uli_sleep
     * locks ulilock, then calls uli_spinunlock_psema which trades the
     * ulilock for the semalock, and then goes to sleep. By spinning
     * waiting for the semalock here, we ensure that the process calling
     * uli_sleep gets to sleep before we do the sv_broadcast, so it
     * isn't left sleeping forever.
     */
#ifdef DEBUG
    int s = splhi(); /* Keep ASSERT in dhardlocks happy */
#endif
    nested_spinlock(&sp->mutex);
#ifdef DEBUG
    splx(s);
#endif

    /* wake up any leftover sleepers */
    sv_broadcast(&sp->sv);

    spinlock_destroy(&sp->mutex);
    sv_destroy(&sp->sv);
}

static int
uli_spinunlock_psema(struct ulisema *sp, lock_t *mutex, int ospl)
{
    /* trade the passed in mutex for the sema mutex */
    nested_spinlock(&sp->mutex);
    nested_spinunlock(mutex);

    sp->count--;
    if (sp->count < 0) {
	if (sv_wait_sig(&sp->sv, PZERO+1, &sp->mutex, ospl))
	    return(EINTR);
    }
    else
	mutex_spinunlock(&sp->mutex, ospl);

    return(0);
}

static void
uli_vsema(struct ulisema *sp)
{
    int s;

    s = mutex_spinlock(&sp->mutex);
    ASSERT(sp->count <= 1);
    if (sp->count < 1) {
	sp->count++;
	if (sp->count <= 0)
	    sv_signal(&sp->sv);
    }
    mutex_spinunlock(&sp->mutex, s);
}

/* allocate a ULI struct with the specified number of semaphores. */
static struct uli *
uli_alloc(int nsemas)
{
    struct uli *newuli;
    int s;

    ASSERT(nsemas >= 0);

    /* Cachealign the struct for best performance in realtime critical path */
    if ((newuli =
	 (struct uli*)kmem_alloc(ULI_SIZE(nsemas), KM_CACHEALIGN)) == 0) {
	eprintf(("uli_alloc can't alloc uli\n"));
	return(0);
    }
    newuli->nsemas = nsemas;

    s = ULILOCK();
    ASSERT(first_free_uli <= MAX_ULIS && first_free_uli >= 0);
    if (first_free_uli == MAX_ULIS) {
	ULIUNLOCK(s);
	kmem_free(newuli, ULI_SIZE(nsemas));
	eprintf(("uli_alloc no more slots\n"));
	return(0);
    }

    ASSERT(uli_table[first_free_uli] == 0);
    newuli->index = first_free_uli;
    uli_table[first_free_uli] = newuli;

    do {
	first_free_uli++;
    } while (first_free_uli < MAX_ULIS && uli_table[first_free_uli]);

    ULIUNLOCK(s);

    while(--nsemas >= 0)
	uli_sema_init(&newuli->sema[nsemas]);

    dprintf(("uli_alloc return normal\n"));
    return(newuli);
}

/* free a uli struct */
static void
uli_free(struct uli *uli)
{
    int s, idx;

    idx = uli->index;
    ASSERT(uli_table[idx] == uli);
    s = ULILOCK();
    uli_table[idx] = 0;
    if (idx < first_free_uli)
	first_free_uli = idx;
    ULIUNLOCK(s);

    for(idx = 0; idx < uli->nsemas; idx++)
	uli_sema_destroy(&uli->sema[idx]);

    kmem_free(uli, ULI_SIZE(uli->nsemas));
    dprintf(("uli_free return normal\n"));
}

/* process an exception in the ULI handler. Figure out what signal to
 * send based on the cause register. This function runs in ULI mode
 * at exception level, on the interrupt stack. Gack.
 */
void
uli_badtrap()
{
    int sig;
    struct uli *uli = private.p_curuli;

#ifdef EPRINTF
    extern void idbg_eframe(__psint_t);

    printf("ULI bad trap, pte 0x%x\n", uli->bunk);
    idbg_eframe((__psint_t)&uli->uli_eframe);
#endif
    switch(uli->uli_eframe.ef_cause & CAUSE_EXCMASK) {

	/* bus errors */
      case EXC_RADE:
      case EXC_WADE:
      case EXC_IBE:
      case EXC_DBE:
#if defined(R4000) || defined(R10000)
      case EXC_VCEI:
      case EXC_VCED:
#endif
	sig = SIGBUS;
	break;

	/* illegal instructions */
      case EXC_II:
      case EXC_CPU:
      case EXC_SYSCALL:
      case EXC_BREAK:
#if defined(R4000) || defined(R10000)
      case EXC_FPE:
#endif
      case EXC_TRAP:
	sig = SIGILL;
	break;

	/* overflow */
      case EXC_OV:
	sig = SIGFPE;
	break;

	/* page fault */
      case EXC_MOD:

	/* bad address references */
      case EXC_RMISS:
      case EXC_WMISS:
	sig = SIGSEGV;

      default:
	sig = SIGSEGV;
    }

    uli_return(sig);
}

/* Call up into the ULI handler from the interrupt stack. */
int
uli_callup(int uliidx)
{
    k_machreg_t sr;
    struct uli *prev_curuli;
    struct uli *ulip = uli_table[uliidx];

#ifdef ULI_TSTAMP1
    uli_saved_tstamps[TS_CALLUP] =	get_timestamp();
    uli_saved_tstamps[TS_GENEX] =	uli_tstamps[TS_GENEX];
    uli_saved_tstamps[TS_EXCEPTION] =	uli_tstamps[TS_EXCEPTION];
    uli_saved_tstamps[TS_INTRNORM] =	uli_tstamps[TS_INTRNORM];
    uli_saved_tstamps[TS_INTR] =	uli_tstamps[TS_INTR];
    uli_saved_tstamps[TS_DEVINTR] =	uli_tstamps[TS_DEVINTR];
#endif

    dprintf(("ulicallup\n"));
    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_ULI, NULL, NULL, NULL, NULL);
    if (cpuid() != ulip->ulicpu || private.p_kstackflag != PDA_CURINTSTK) {
	static int last_time;
	if (last_time != time) {
	    if (cpuid() != ulip->ulicpu)
		printf("ERROR: ULI: intr taken on cpu %d, expected on cpu %d\n",
		       cpuid(), ulip->ulicpu);
	    if (private.p_kstackflag != PDA_CURINTSTK)
		printf("ERROR: ULI: attempted to run on threaded interrupt\n");
	    last_time = time;
	}
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_ULI, NULL, NULL, NULL);
	return(0);
    }

    /* disable interrupts */
    sr = getsr();
    setsr(sr & (~(SR_IE)));
    
#ifdef TFP
    ulip->saved_config = get_config();
    dprintf(("kernel saved config 0x%x\n", ulip->saved_config));
#endif

    /* save previous curuli in local stack frame in case we nest ULIs */
    prev_curuli = private.p_curuli;
#ifdef DEBUG
    if (prev_curuli)
	nested_uli++;
#endif

    /* establish a return point to return to when the ULI handler
     * has completed
     */
    if (setjmp(ulip->jmpbuf)) {
	ASSERT(private.p_curuli == ulip);

	/* If the ULI handler overran its time allotment, it will 
	 * abort from within the nested clock interrupt handler,
	 * in which case curuthread and tlbpid will already have been
	 * restored, so this assertion will not be true. Thus ignore
	 * the assert if there is a SIGXCPU pending
	 */
	ASSERT(ulip->sig == SIGXCPU || 
	       (curuthread == ulip->uli_uthread &&
		uli_getasids() == ulip->new_asids));
	dprintf(("return to ulicallup\n"));

#ifdef ULI_PANIC_ON_BADTRAP
	if (ulip->sig) {
	    setsr(sr & (~SR_IE));
	    panic(" ep: 0x%x\n", &ulip->uli_eframe);
	}
#endif
	/* restore tlbpid and, for TFP, icachepid */
	uli_setasids(ulip->saved_asids);

	/* back in interrupt mode */
	private.p_kstackflag = PDA_CURINTSTK;

	/* restore saved state */
	private.p_curuli = prev_curuli;
	private.p_curuthread = ulip->uli_saved_curuthread;

	/* restore saved status register */
	setsr(sr);

	/* check if we're expected to signal the ULI process */
	if (ulip->sig)
		sigtouthread(ulip->uli_uthread, ulip->sig, NULL);

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_ULI, NULL, NULL, NULL);
	return(0);
    }

    /* save current system state */
    ulip->uli_saved_curuthread = curuthread;

    /* save current interrupt stack pointer */
    ulip->saved_intstack = getsp();

    /* saved current address space */
    ulip->saved_asids = uli_getasids();

    /* enter ULI mode */
    private.p_curuli = ulip;
    private.p_kstackflag = PDA_CURULISTK;

    /* set up ULI proc's address space */
    private.p_curuthread = ulip->uli_uthread;
    uli_setasids(ulip->new_asids);

    dprintf(("callup with sr 0x%x, ulisr 0x%x\n", sr, ulip->sr));
    dprintf(("  pc 0x%x sp 0x%x gp 0x%x\n", ulip->pc, ulip->sp,
	     ulip->gp));

    /* get a timestamp so clock intr can determine if this ULI
     * has been running too long and abort it
     */
    ulip->tstamp = lbolt;

    /* jump into ULI handler */
    uli_eret(ulip, sr);
    /*NOTREACHED*/
}

/* return to interrupt stack when ULI handler has completed */
void
uli_return(int sig)
{
    ASSERT(sig >= 0);
    
    dprintf(("uli_return %d\n", sig));

    /* ensure we are returning with the same spl level that we 
     * entered the ULI handler at. If not, somebody somewhere is
     * screwing up the spl, possibly breaking critical regions
     */
    ASSERT(((private.p_curuli)->jmpbuf[JB_SR] & SR_IMASK) ==
	   (getsr() & SR_IMASK));
#if EVEREST
    ASSERT((unchar)(private.p_curuli)->jmpbuf[JB_CEL] == private.p_CEL_shadow);
#endif

#ifdef TFP
    ASSERT(sig == SIGXCPU || get_config() == private.p_curuli->saved_config);
#endif

    /* save sig to be sent to the ULI process, if any */
    private.p_curuli->sig = sig;
    longjmp((private.p_curuli)->jmpbuf, 1);
}

static void
chainuli(struct uli *up)
{
    uthread_t *ut = curuthread;
    up->next = ut->ut_uli;
    up->prev = 0;

    if (ut->ut_uli)
	ut->ut_uli->prev = up;
    ut->ut_uli = up;
}

static void
unchainuli(struct uli *up)
{
    if (up->next)
	up->next->prev = up->prev;

    if (up->prev)
	up->prev->next = up->next;
    else
	curuthread->ut_uli = up->next;
}

/* allocate and initialize a ULI */
int
new_uli(struct uliargs *args, struct uli **retuli, int intrcpu)
{
    struct uli *newuli;
    struct uthread_s *ut = curuthread;
    cpu_cookie_t was_running;
    /* REFERENCED */
    int err;
    int error = 0;
    tlbpid_t new_tlbpid;
    as_setattr_t attr;
    vasid_t vasid;
#ifdef TFP
    icachepid_t new_icachepid;
#endif

    ASSERT(cpu_isvalid(intrcpu));

    /* all ULI registration must pass through this point: require root
     * or DEVICE_MGT capability
     */
    if (!_CAP_ABLE(CAP_DEVICE_MGT))
	return(EPERM);

    if (args->nsemas < 0) {
	eprintf(("new_uli bad nsemas %d\n", args->nsemas));
	return(EINVAL);
    }

    if ((newuli = uli_alloc(args->nsemas)) == 0) {
	eprintf(("new_uli can't alloc uli\n"));
	return(EBUSY);
    }

    /* if this is the first ULI registered by this proc, add
     * an exit func to tear it and any others subsequently registered
     * down when the process exits
     * This routine will be called each time a process goes from having
     * NO ULI handlers to having one or more.
     */
    if (ut->ut_uli == 0) {
	err = uthread_add_exit_callback(ADDEXIT_CHECK_DUPS,
				(void (*)(void *))uli_exit, 0);
	ASSERT(err == 0 || err == EALREADY);
	dprintf(("first ULI for this proc: add exit func\n"));
	/*
	 * tell AS layer we're doing ULI stuff.
	 * This will also set us into segtbl mode and inform our utas
	 * that we should never zap the segtbl
	 */
	as_lookup_current(&vasid);
	attr.as_op = AS_SET_ULI;
	attr.as_set_uli = 1;
	VAS_SETATTR(vasid, NULL, &attr);
    }

    /* we need to be running on the cpu where the interrupt will be
     * taken in order to allocate the private tlbpid
     */
    was_running = setmustrun(intrcpu);
    if ((new_tlbpid = alloc_private_tlbpid()) == 0) {
	error = EBUSY;
	eprintf(("register: out of tlbpids\n"));
	goto bad;
    }
    dprintf(("allocate private tlbpid %d\n", new_tlbpid));
    newuli->new_asids = new_tlbpid;

#ifdef TFP
    if ((new_icachepid = alloc_private_icachepid()) == 0) {
	free_private_tlbpid(new_tlbpid);
	error = EBUSY;
	eprintf(("register: out of icachepids\n"));
	goto bad;
    }
    dprintf(("allocate private icachepid %d\n", new_icachepid));
    newuli->new_asids |= (new_icachepid << 8);
#endif
    dprintf(("new asids 0x%x\n", newuli->new_asids));
    restoremustrun(was_running);
    
    newuli->ulicpu = intrcpu;
    newuli->uli_uthread = ut;

    /* we need to know the gp of the ULI process so we can set it when
     * jumping into the ULI handler. Snarf it here from the syscall eframe
     */
    newuli->gp = (caddr_t)curexceptionp->u_eframe.ef_gp;

    /* prep sr that will be used for ULI mode */
    newuli->sr = ULISR(curexceptionp->u_eframe.ef_sr);

    dprintf(("ULI sr 0x%x\n", newuli->sr));
#ifdef TFP
    newuli->config = curexceptionp->u_eframe.ef_config;
    dprintf(("ULI config 0x%x\n", newuli->config));
#endif

    newuli->sp = args->sp;
    newuli->pc = args->pc;
    newuli->funcarg = args->funcarg;
    newuli->uli_eframe_valid = 0;
    newuli->teardown = 0;

    chainuli(newuli);
    dprintf(("uthread 0x%x for proc %d now in segtbl mode segflags %d, ut_uli 0x%x\n",
	     ut, current_pid(), ut->ut_as.utas_segflags, ut->ut_uli));

    dprintf(("new_uli return normal\n"));

    *retuli = newuli;

    return(0);

bad:
    restoremustrun(was_running);
    uli_free(newuli);

    if (ut->ut_uli == 0) {
	as_lookup_current(&vasid);
	attr.as_op = AS_SET_ULI;
	attr.as_set_uli = 0;
	VAS_SETATTR(vasid, NULL, &attr);
    }
    return error;
}

/* free and deallocate a ULI */
int
free_uli(struct uli *uli)
{
    int s;
    as_setattr_t attr;
    vasid_t vasid;
    cpu_cookie_t was_running;

    ASSERT(cpu_isvalid(uli->ulicpu));
    unchainuli(uli);

    /* we must run on the cpu where the tlbpid was allocated in order
     * to free it. Also, we need to be running on the cpu where the 
     * intr is delivered in order to ensure that it is not running
     * while we tear down the ULI
     */
    was_running = setmustrun(uli->ulicpu);
    free_private_tlbpid((tlbpid_t)(uli->new_asids & 0xff));
#ifdef TFP
    free_private_icachepid((icachepid_t)(uli->new_asids >> 8));
#endif

    /* block the interrupt and disconnect it */
    if (uli->teardown) {
	s = splhi();
	uli->teardown(uli);
	splx(s);
    }

    restoremustrun(was_running);
    
    uli_free(uli);
    dprintf(("free_uli return normal\n"));
    /*
     * if last ULI, disable ULI mode
     * XXX this kind of implies that new/free are single threaded
     */
    if (curuthread->ut_uli == NULL) {
	as_lookup_current(&vasid);
	attr.as_op = AS_SET_ULI;
	attr.as_set_uli = 0;
	VAS_SETATTR(vasid, NULL, &attr);
    }
    return(0);
}

/* called when a uthread exits. Disconnect any ULIs the process has
 * registered.
 */
static void
uli_exit()
{
    uthread_t *ut = curuthread;
    struct uli *uli, *nextuli;
    int s;
    cpu_cookie_t was_running;

    for(uli = ut->ut_uli; uli; uli = nextuli) {
	nextuli = uli->next;
	ASSERT(cpu_isvalid(uli->ulicpu));
	ASSERT(uli->uli_uthread == curuthread);

	/* must be running on cpu where tlbpid was allocated in
	 * order to free it and to block the intr while the ULI
	 * is torn down
	 */
	was_running = setmustrun(uli->ulicpu);
	free_private_tlbpid((tlbpid_t)(uli->new_asids & 0xff));
#ifdef TFP
	free_private_icachepid((icachepid_t)(uli->new_asids >> 8));
#endif
	    
	if (uli->teardown) {
	    s = splhi();
	    uli->teardown(uli);
	    splx(s);
	}
	restoremustrun(was_running);
	    
	uli_free(uli);
    }
    dprintf(("uli_exit return normal\n"));
    ut->ut_uli = 0; /* make this function idempotent */
}

/* go to sleep on the given semaphore for the given ULI */
int
uli_sleep(int idx, int sema)
{
    int s, err;
    struct uli *uli;

    if (idx < 0 || idx >= MAX_ULIS || sema < 0) {
	eprintf(("uli_sleep: bad idx %d or sema %d\n", idx, sema));
	return(EINVAL);
    }

    s = ULILOCK();
    if ((uli = uli_table[idx]) == 0) {
	ULIUNLOCK(s);
	eprintf(("uli_sleep bad idx %d\n", idx));
	return(EINVAL);
    }
    if (sema >= uli->nsemas) {
	ULIUNLOCK(s);
	eprintf(("uli_sleep bad sema %d\n", sema));
	return(EINVAL);
    }

    dprintf(("uli_sleep sleeping\n"));
    err = uli_spinunlock_psema(&uli->sema[sema], &ulimutex, s);
    dprintf(("uli_sleep woke\n"));
    return(err);
}

/* Wakeup the next process sleeping on the given sema for the given ULI */
int
uli_wakeup(int sema, int idx)
{
    struct uli *uli;

    if (idx < 0 || idx >= MAX_ULIS || sema < 0) {
	eprintf(("uli_wakeup: bad idx %d or sema %d\n", idx, sema));
	return(EINVAL);
    }

    if ((uli = uli_table[idx]) == 0) {
	eprintf(("uli_wakeup bad idx %d\n", idx));
	return(EINVAL);
    }
    if (sema >= uli->nsemas) {
	eprintf(("uli_wakeup bad sema %d\n", sema));
	return(EINVAL);
    }

    uli_vsema(&uli->sema[sema]);
    dprintf(("uli_wakeup return normal\n"));

    return(0);
}

/* Grab ULI lock before calling uli_wakeup. This is used when in the
 * context of a process. At interrupt level, we don't need the lock.
 */
int
uli_wakeup_lock(int sema, int idx)
{
    int err;
    int s = ULILOCK();
    err = uli_wakeup(sema, idx);
    ULIUNLOCK(s);
    return(err);
}

/*
 * ULI_syscall. This is the "kernel call" available to the ULI
 * handler. Before this code is called, we restore all state to what it
 * was before the ULI, so this is running in interrupt mode on the
 * interrupt stack. Consequently we no longer have access to the ULI
 * address space, so we can't do copyin/out.
 *
 * Protocol: on success, return 0 and set *retp to any return value
 * desired. On error, return the errno.
 * 
 * NOTE: ULI_RETURN is handled separately in a locore fastpath.
 */
/* ARGSUSED */
int
ULI_syscall(k_machreg_t *retp, __psint_t arg1, __psint_t arg2, __psint_t arg3)
{
    dprintf(("ulisyscall %x %d %d %d\n", retp, arg1, arg2, arg3));
    switch(arg1) {
      case ULI_CPUID:
	*retp = (k_machreg_t)cpuid();
	return(0);
      case ULI_WAKEUP:
	return(uli_wakeup(arg2, arg3));
    }
    return(EINVAL);
}

#ifndef IP30

int vmeuli_inited;
lock_t ivec_mutex;

/* ensure that an adapter/ipl/vec combination has been configured into
 * the kernel in a VECTOR line
 */
int
vmeuli_validate_vector(int adap, int ipl, int vec)
{
    extern struct edt edt[];
    extern int nedt;
    extern int (*vmeuliintrp)(int);
    int x;

    for(x = 0; x < nedt; x++) {
	if (edt[x].e_bus_type == ADAP_VME) {
	    struct vme_intrs *intrs = (struct vme_intrs*)edt[x].e_bus_info;
	    if (intrs->v_vintr == vmeuliintrp &&
		(intrs->v_vec == vec || vec == -1) &&
		intrs->v_brl == ipl &&
		edt[x].e_adap == adap) {
		dprintf(("hard vec %d ipl %d adap %d matches edt entry %d\n",
			 vec, ipl, adap, x));
		return(0);
	    }
	}
    }
    eprintf(("no match for hard vec %d ipl %d adap %d in edt table\n",
	     vec, ipl, adap));
    return(1);
}

/* modify the argument for a vme ivec entry. (i.e. modify the argument
 * that will be passed to the handler func for that ivec)
 */
int
vmeuli_change_ivec(int adap, int vec, int arg)
{
    int err, s;

    if (!vmeuli_inited)
	return(ENODEV);

    s = mutex_spinlock(&ivec_mutex);
    err = vmeuli_ivec_reset(adap, vec, arg);
    mutex_spinunlock(&ivec_mutex, s);
    return(err);
}
#endif

/* copy saved registers at time of fault in ULI handler over
 * to the u_eframe so that core() can dump them properly
 */
void
uli_core()
{
    struct uli *uli;

    /* find the first ULI that has valid fault data. If there is
     * more than one, too bad, we'll just take the first one
     */

    /* NOTE make sure this stays a noop for non-ULI processes! */

    for (uli = curuthread->ut_uli; uli; uli = uli->next) {
	if (uli->uli_eframe_valid) {
	    bcopy(&uli->uli_eframe, &curexceptionp->u_eframe, sizeof(eframe_t));
	    return;
	}
    }
}

#if _MIPS_SIM == _ABI64
/* ARGSUSED */
void
uliargs32_to_uliargs(struct uliargs32 *from, struct uliargs *to)
{
    /* double cast to keep compiler from complaining about mixing
     * pointer sizes. These pointers are all KUSEG so it's safe
     * to just typecast them from 32 to 64 bits 
     */
    to->pc = (caddr_t)((long)from->pc);
    to->sp = (caddr_t)((long)from->sp);
    to->funcarg = (void*)((long)from->funcarg);
    to->dd.vme.ipl = from->dd.vme.ipl;
    to->dd.vme.vec = from->dd.vme.vec;
    to->nsemas = from->nsemas;
}

void
uliargs_to_uliargs32(struct uliargs *from, struct uliargs32 *to)
{
    to->dd.vme.vec = from->dd.vme.vec;
    to->id = from->id;
}

/* ARGSUSED */
int
irix5_to_uliargs(enum xlate_mode mode,
		 void *to,
		 int count,
		 xlate_info_t *info)
{
    COPYIN_XLATE_PROLOGUE(uliargs32, uliargs);

    uliargs32_to_uliargs(source, target);
    return(0);
}

/* ARGSUSED */
int
uliargs_to_irix5(void *from,
		 int count,
		 xlate_info_t *info)
{
    XLATE_COPYOUT_PROLOGUE(uliargs, uliargs32);
    
    uliargs_to_uliargs32(source, target);
    return(0);
}
#endif /* _ABI64 */

#endif /* ULI */
