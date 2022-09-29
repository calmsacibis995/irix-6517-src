#ident	"$Revision: 1.63 $"

/*
 * Driver for Everest external interrupts
 */
#ifdef EVEREST

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/groupintr.h>
#include <sys/reg.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/ei.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/invent.h>
#include <sys/par.h>
#include <sys/ksignal.h>
#include <sys/major.h>
#include <sys/edt.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/xlate.h>
#include <sys/atomic_ops.h>
#include <sys/kabi.h>
#ifdef ULI
#include <sys/uli.h>
#include <ksys/uli.h>
#endif
#include <ksys/ddmap.h>
#include <sys/runq.h>
#include <sys/hwgraph.h>

#if DEBUG_INTR_TSTAMP_DEBUG
#include <sys/debug.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif

int eidevflag;

/*
 * driver globals
 */
static caddr_t epcbase;		/* base addr of epc */
static caddr_t counterpage;	/* page containing user counter */
static lock_t eilock;		/* mutex for pstate_list access */
static int intr_enabled;	/* intr enabled at hardware level */
static evreg_t intr_timestamp;	/* RTC at which intr was last enabled */

/* keep a copy of configured intr cpu and syscall cpu */
static int config_syscallcpu, config_intrcpu;
#ifdef ULI
static int runtime_intrcpu;
#endif

static void eiintr(struct eframe_s *ep, unsigned int arg);
static int setintrcpu(int cpu);
static int setsyscpu(int cpu);

#define EILOCK() io_splockspl(eilock, spl3)
#define EIUNLOCK(s) io_spunlock(eilock, s)

/* enable/disable intrs at the hardware level */
#define ENABLE_INTR()  *((__int32_t *)(epcbase + EPC_IMSET + 4)) = (1 << 4)
#define DISABLE_INTR() *((__int32_t *)(epcbase + EPC_IMRST + 4)) = (1 << 4)

/* default values for nominal pulse width and pulse stuck detection
 * in microseconds
 */
#define PULSEWIDTH 10
#define PULSESTUCK 500

/* convert microseconds to Everest bus cycles */
#define USEC_TO_BC(x) ((x) * 1000 / NSEC_PER_CYCLE)

/* timing variables in microseconds */
static int outpulsewidth = PULSEWIDTH;	/* outgoing pulse width to generate */
static int outpulsewidth_bc = USEC_TO_BC(PULSEWIDTH);
static int inpulsewidth = PULSEWIDTH;	/* incoming pulse width to expect */
static int inpulsewidth_bc = USEC_TO_BC(PULSEWIDTH);
static int inpulsestuck = PULSESTUCK;	/* assume input is stuck on if intr occurs */
					/* again within this time interval */
/* number of bus cycles in inpulsestuck usec. */
static int inpulsestuck_bc = USEC_TO_BC(PULSESTUCK);

/* debugging stats */
static struct eistats eistats;

/* user counter is all by its lonesome on the counterpage... */
#define usercounter ((__int32_t *)counterpage)

/* atomic increment and decrement */
#define ATOMINC(x) atomicAddInt(&(x), 1)
#define ATOMDEC(x) atomicAddInt(&(x), -1)

/* page from addr */
#define PAGE(x) (caddr_t)((__psunsigned_t)(x) & (~(NBPP-1)))

/* debug printing */
#ifdef DPRINTF
#define dprintf(x) {printf x; conbuf_flush(CONBUF_UNLOCKED);}
#else
#define dprintf(x)
#endif

/* normal timestamp */
#define TIMESTAMP EV_GET_LOCAL_RTC

/* debugging timestamp */
#ifdef DTS
#define DTIMESTAMP(x) x = TIMESTAMP
#else
#define DTIMESTAMP(x)
#endif

extern void frs_handle_eintr(void);

/* per process state maintained by the driver.
 */
static struct pstate {
	pid_t pstate_pid;		/* pid owning this struct */
	struct pstate *next;
	int privcounter;		/* interrupt queue for this process */
	int signal;			/* signal to send proc on interrupt */
	lock_t lock;
	sv_t wait;
} *pstate_list;

#if _MIPS_SIM == _ABI64
int irix5_to_eiargs_xlate(enum xlate_mode, void *, int,	xlate_info_t *);
int eiargs_to_irix5_xlate(void *, int, xlate_info_t *);
#endif

#ifdef DEBUG_INTR_TSTAMP
#ifdef ULI_TSTAMP
<<< BOMB - can not define DEBUG_INTR_TSTAMP and ULI_TSTAMP>>>
#endif
/* We allocate an array, but only use element number 64.  This guarentees that
 * the entry is in a cacheline by itself.
 */
#define DINTR_CNTIDX	32
#define DINTR_TSTAMP1	48
#define	DINTR_TSTAMP2	64
volatile long long dintr_tstamp_cnt[128];
int dintr_debug_output=0;


#if DEBUG_INTR_TSTAMP_DEBUG
int dintr_enter_symmon=50000;	/* 50000 cycles is about 1 millisecond */

int
idbg_tstamp_debug()
{
	qprintf("TSTAMP1 (sender) 0x%x  TSTAMP2 (recv) 0x%x  delta 0x%x\n",
		dintr_tstamp_cnt[DINTR_TSTAMP1],
		dintr_tstamp_cnt[DINTR_TSTAMP2],
		dintr_tstamp_cnt[DINTR_TSTAMP2]-dintr_tstamp_cnt[DINTR_TSTAMP1]);
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
	{
	extern int idbg_splx_logtime(uint64_t);

	qprintf("Interrupt sent at %d  received at %d\n",
		idbg_splx_logtime(dintr_tstamp_cnt[DINTR_TSTAMP1]),
		idbg_splx_logtime(dintr_tstamp_cnt[DINTR_TSTAMP2]));
	}
#endif /* SPLDEBUG || SPLDEBUG_CPU_EVENTS */
	return(0);
}
#endif /* DEBUG_INTR_TSTAMP_DEBUG */

/*ARGSUSED*/
static void
dintr_tstamp_intr(eframe_t *ep, void *arg)
{
#if DEBUG_INTR_TSTAMP_DEBUG
	dintr_tstamp_cnt[DINTR_TSTAMP2] =  *((volatile long long*)EV_RTC);
	if ((dintr_tstamp_cnt[DINTR_TSTAMP2] - dintr_tstamp_cnt[DINTR_TSTAMP1])
	    > dintr_enter_symmon) {
#ifdef SPLDEBUG
		extern int spldebug_log_off;

		spldebug_log_off = 1;
#endif
		debug("ring");
#ifdef SPLDEBUG
		spldebug_log_off = 0;
#endif
	}
#endif
	dintr_tstamp_cnt[DINTR_CNTIDX]++;
}

int dintr_connected=0;

int
dintr_tstamp_noithread()
{
	extern void thrd_evintr_noithread(int);

	if (!dintr_connected)
		return -1;

	thrd_evintr_noithread(EVINTR_LEVEL_EPC_DUARTS4);
	return 0;
}

int
dintr_tstamp_noistack()
{
	extern void thrd_evintr_noistack(int);

	if (!dintr_connected)
		return -1;

	thrd_evintr_noistack(EVINTR_LEVEL_EPC_DUARTS4);
	return 0;
}

/* called to send intr_cnt interrupts to destcpu.
 * If destcpu == CPU_NONE, then routine picks one of the other cpus.
 * Returns time (in cycles) from sending interrupt until receiving cpu
 * responds.
 * If intr_cnt > 1, then mintime and maxtime return min and max response
 * times and TOTAL response time is returned to caller.
 */
long long
dintr_tstamp( cpuid_t destcpu, int intr_cnt, int *maxtime, int *mintime )
{
	int s, old_cnt;
	long long ts1, ts2;
	int totaltime, mint, maxt;

	if (intr_cnt == 0)
		return (long long)0;

	if (!dintr_connected) {
		dintr_connected++;

		evintr_connect
			(0,
			 EVINTR_LEVEL_EPC_DUARTS4,
			 SPLHIDEV,
			 maxcpus-1,	/* will be mustrun on last cpu */
			 dintr_tstamp_intr,
			 (void *)0);
		dintr_connected++;
		return (long long) 0;
	}


	/* block all interrupts and don't let us migrate */
	s = spl7();
	if (destcpu == CPU_NONE) {
		destcpu = maxcpus - 1;
		if (destcpu == cpuid())
			destcpu--;
	}
	/* if user specified cpu, and it's us then return error */
	if (destcpu == cpuid()) {
		splx(s);
		return ((long long) -1);
	}
	if (intr_cnt == 1) {
		old_cnt = dintr_tstamp_cnt[DINTR_CNTIDX];
		ts1 = *((volatile long long*)EV_RTC);
#if DEBUG_INTR_TSTAMP_DEBUG
		dintr_tstamp_cnt[DINTR_TSTAMP1] =  ts1;
#endif
	
		/* trigger the interrupt */
		EV_SET_LOCAL(EV_SENDINT,
			EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUARTS4,
				EVINTR_CPUDEST(destcpu)));

#if DEBUG_INTR_TSTAMP_DEBUG
		/* we can drop spl since we will use receiver's timestamp.
		 * Avoids problem with deadlock-ing on tlbsync (a cpuaction)
		 * which can occur if we're sending to an ithread since the
		 * ithread will not run until the tlbsync completes IFF the
		 * tlbsync was initiated by the cpu we're sending to.
		 */
		splx(s);
#endif

		while (dintr_tstamp_cnt[DINTR_CNTIDX] == old_cnt)
			;

#if DEBUG_INTR_TSTAMP_DEBUG
		ts2 = dintr_tstamp_cnt[DINTR_TSTAMP2];
#else
		ts2 = *((volatile long long*)EV_RTC);

		splx(s);
#endif
		if ((ts2-ts1) < 0)
			cmn_err(CE_WARN, "BAD RTC 0x%x 0x%x\n", ts1, ts2);

		return(ts2 - ts1);
	}

	/* following code handles multiple interrupt events */
	maxt = totaltime = 0;
	mint = 1000000;
	old_cnt = dintr_tstamp_cnt[DINTR_CNTIDX];

	while (intr_cnt) {

		ts1 = *((volatile long long*)EV_RTC);
#if DEBUG_INTR_TSTAMP_DEBUG
		dintr_tstamp_cnt[DINTR_TSTAMP1] =  ts1;
#endif

		/* trigger the interrupt */
		EV_SET_LOCAL(EV_SENDINT,
			EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUARTS4,
				EVINTR_CPUDEST(destcpu)));

		while (dintr_tstamp_cnt[DINTR_CNTIDX] == old_cnt)
			;

		ts2 = *((volatile long long*)EV_RTC);
		ts2 -= ts1;
		totaltime += ts2;
		if (ts2 > maxt)
			maxt = ts2;
		if (ts2 < mint)
			mint = ts2;
		intr_cnt--;
		old_cnt++;
	}
	splx(s);
	*maxtime = maxt;
	*mintime = mint;
	return totaltime;

}
#endif /* DEBUG_INTR_TSTAMP */

#ifdef ULI
/* id of user level interrupt attached to the ei */
static int ulinum;
#define ULI_ISFREE(ulinum) (ulinum == -1)
#define FREE_ULI(ulinum) (ulinum = -1)
#define ULI_ISACTIVE(ulinum) (ulinum >= 0)
#define RESERVE_ULI(ulinum) (ulinum = -2)
#endif

#ifdef ULI_TSTAMP
static int trigger_ts;
static int ulits_ulinum;

/*ARGSUSED*/
static void
uli_tstamp_intr(struct eframe_s *ep, unsigned int arg)
{
	if (ULI_ISACTIVE(ulits_ulinum))
		uli_callup(ulits_ulinum);
}

/*ARGSUSED*/
static void
uli_tstamp_teardown(struct uli *uli)
{
	FREE_ULI(ulits_ulinum);
}

#endif /* ULI_TSTAMP */

/* called when a process which had previously opened the EI device exits.
 * We free up the process's pstate struct here
 */
static void
ei_exit(void)
{
	int s;
	struct pstate **ptmpp, *ptmp;

	dprintf(("ei_exit pid %d\n", current_pid()));
	s = EILOCK();

	/* clear the pstate for this proc */
	for (ptmpp = &pstate_list; *ptmpp; ptmpp = &((*ptmpp)->next)) {
		ptmp = *ptmpp;

		if (ptmp->pstate_pid == current_pid()) {
			*ptmpp = ptmp->next;
			spinlock_destroy(&ptmp->lock);
			sv_destroy(&ptmp->wait);
			kmem_free(ptmp, sizeof(struct pstate));
			break;
		}
	}
	EIUNLOCK(s);
}

/* find the pstate for the current vproc, or allocate one if there is none.
 */
static struct pstate *
find_pstate(void)
{
    struct pstate *ptmp;
    int s;
    /* REFERENCED */
    int err;

    s = EILOCK();
    for(ptmp = pstate_list; ptmp; ptmp = ptmp->next)
	if (ptmp->pstate_pid == current_pid()) {
	    EIUNLOCK(s);
	    return(ptmp);
	}
    EIUNLOCK(s);

    /* This is the first time this proc has accessed the device. Set
     * up an exit callback to ensure that the pstate is torn down
     * when the proc exits
     */
    err = add_exit_callback(current_pid(), 0, (void (*)(void *))ei_exit, 0);
    ASSERT(err == 0);
    dprintf(("first EI access by proc: add exit func\n"));

    if ((ptmp = (struct pstate*)kmem_zalloc(sizeof(struct pstate), 0)) == 0)
	return(0);
    ptmp->pstate_pid = current_pid();
    spinlock_init(&ptmp->lock, "eilock");
    sv_init(&ptmp->wait, SV_DEFAULT, "eiwait");

    s = EILOCK();
    ptmp->next = pstate_list;
    pstate_list = ptmp;
    EIUNLOCK(s);

    dprintf(("new pstate pid %d\n", current_pid()));
    return(ptmp);
}

/* enable interrupts at the hardware level */
static void
enable_intr(void)
{
	/*
	 * use a lock to ensure that intr_enabled is always in sync with
	 * the hardware state
	 */
	int s = EILOCK();
	ENABLE_INTR();
	intr_enabled = 1;
	EIUNLOCK(s);
}

/* disable interrupts at the hardware level */
static void
disable_intr(void)
{
	int s = EILOCK();
	DISABLE_INTR();
	intr_enabled = 0;
	EIUNLOCK(s);
}

/* retry enabling interrupts. This is called from the callout list
 * when the input line is stuck on.
 */
static void
intr_retry()
{
	int s = EILOCK();

	/* make sure the interrupts weren't disabled since the last
	 * call to timeout
	 */
	if (intr_enabled) {
		/* get the time at which we're enabling the interrupt so
		 * the interrupt handler can determine if the line
		 * is stuck on
		 */
		intr_timestamp = TIMESTAMP;
		ENABLE_INTR();
	}
	EIUNLOCK(s);
}

/*
 * Frame Scheduler
 * Setting up interrupts to be sent to a frame scheduler intrgroup
 */

void
frs_eintr_set(intrgroup_t* intrgroup)
{
	int s;

	ASSERT(intrgroup != 0);
	s = EILOCK();
	DISABLE_INTR();
	evintr_connect((evreg_t *)(epcbase + EPC_IIDINTR4),
		   EVINTR_LEVEL_EPC_INTR4,
		   SPLDEV,
		   EV_MAX_CPUS + intrgroup->groupid,
		   (EvIntrFunc)eiintr,
		   (void *)0);
	ENABLE_INTR();
	setsyscpu(-1);
	intr_enabled = 1;
	EIUNLOCK(s);
}

/*
 * Frame Scheduler
 * Disconnecting the link between ei's and frs
 */
void
frs_eintr_clear(void)
{
	disable_intr();
	setintrcpu(config_intrcpu);
	setsyscpu(config_syscallcpu);
}



/* ei interrupt handler. Since the interrupt hardware for external
 * interrupts is level sensitive, the handler must check how much time
 * has elapsed since the last interrupt came in. If the input line is
 * stuck on, the time will be something in the range of 30 usec, since
 * the interrupt is generated again as soon as the handler returns. When
 * this condition is detected, we go into a periodic retry mode in which
 * we set a timeout and try enabling the interrupt again after a while.
 * Eventually the line will be deasserted and we can resume normal
 * processing. To help prevent this condition from happening
 * accidentally, we constrain the handler to never return before the full
 * expected pulse duration has elapsed.
 */

/*
 * This intr routine may be simultaneously executed on several
 * cpus. We need to provide per-cpu static variables.
 */

static int retryinterval[EV_MAX_CPUS];
static int warninginterval[EV_MAX_CPUS];
static int warningnum[EV_MAX_CPUS];

/* ARGSUSED */
static void
eiintr(struct eframe_s *ep, unsigned int arg)
{
    int lock;
    evreg_t now, delta;
    struct pstate *ptmp;
    int s, thiscpu;
    evreg_t hpil;
    
    /* get the current time */
    now = TIMESTAMP;

    thiscpu = cpuid();
    
    DTIMESTAMP(eistats.longvals[3]);
    dprintf(("intr on cpu %d time %lld\n", cpuid(), now));
    
    /* see how much time has elapsed since the last interrupt occurred.
     * if the interval was less than some threshold, assume that the
     * input line is stuck and go into periodic retry mode
     */
    delta = now - intr_timestamp;
    
    if (delta < inpulsestuck_bc) {
	
	/* keep track of how long the input line has been stuck.
	 * issue a warning after 1 minute, then every 10 minutes
	 * thereafter
	 */
	warninginterval[thiscpu] += retryinterval[thiscpu];
	if (warningnum[thiscpu] == 0) {
	    if (warninginterval[thiscpu] > 60 * HZ) {
		warningnum[thiscpu] = 1;
		warninginterval[thiscpu] = 0;
	    }
	}
	else if (warninginterval[thiscpu] > 600 * HZ)
	    warninginterval[thiscpu] = 0;
	
	if (warninginterval[thiscpu] == 0)
	    printf("warning: external interrupt input asserted\n");
	
	dprintf(("intr occurred at %lld, retry in %d\n", delta,
		 retryinterval[thiscpu]));
	
	/* we'll try again later... */
	timeout(intr_retry, 0, retryinterval[thiscpu]);
	
	/* increase the retry interval until it reaches 5 seconds */
	retryinterval[thiscpu] *= 2;
	if (retryinterval[thiscpu] > 5 * HZ)
	    retryinterval[thiscpu] = 5 * HZ;
	
	return;
    }
    
    /* at this point, we have a valid interrupt */
    
    if (private.p_frs_flags) {
	    /*
	     * Frame Scheduler
	     */
	    frs_handle_eintr();
	    ENABLE_INTR();
	    goto eiintr_wait;
    }

#ifdef ULI
    if (ULI_ISACTIVE(ulinum)) {
	uli_callup(ulinum);
	ENABLE_INTR();
	goto eiintr_wait;
    }
#endif
    
    dprintf(("interval %lld ok\n", delta));
    
    retryinterval[thiscpu] = 1;
    warninginterval[thiscpu] = warningnum[thiscpu] = 0;
    
    DTIMESTAMP(eistats.longvals[4]);
    
    /* increment user counter */
    (*usercounter)++;
    dprintf(("cnt now %d\n", *usercounter));
    
    lock = EILOCK();
    
    /* increment privcounter, send signal or do wakeup if necessary
     * on each process
     */
    for (ptmp = pstate_list; ptmp; ptmp = ptmp->next) {
	ATOMINC(ptmp->privcounter);
	if (ptmp->signal) {
	    dprintf(("sending signal %d to pid %d\n",
		     ptmp->signal, ptmp->pstate_pid));

	    /* potential race if the process is exiting. Since we are
	     * holding the EILOCK here, the process will stall in
	     * ei_exit() if it tries to exit while we are signalling it
	     */
	    sigtopid(ptmp->pstate_pid, ptmp->signal,
			SIG_ISKERN|SIG_NOSLEEP, 0, 0, 0);
	}
        s = mutex_spinlock(&ptmp->lock);
	sv_signal(&ptmp->wait);
        mutex_spinunlock(&ptmp->lock, s);
    }
    
    DTIMESTAMP(eistats.longvals[9]);
    
    /* re-enable the interrupt */
    ENABLE_INTR();
    
    EIUNLOCK(lock);
    
  eiintr_wait:
    /* make sure we wait until the expected incoming pulse duration 
     * has elapsed before returning, otherwise we'll just jump back into
     * the intr handler again. For any reasonably narrow pulse, this
     * will probably be a nop.
     */
    do {
	intr_timestamp = TIMESTAMP;
	hpil = EV_GET_LOCAL_HPIL;
    }
    while(hpil >= EVINTR_LEVEL_EPC_INTR4 &&
	  intr_timestamp - now < inpulsewidth_bc);

    /* If interrupt is still pending, try to clear it. If it doesn't
     * clear here we'll interrupt again and go through the timeout
     * process
     */
    if (hpil >= EVINTR_LEVEL_EPC_INTR4) {
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_EPC_INTR4);

	/* wait for write gatherer to flush */
	(void)EV_GET_LOCAL(EV_CIPL0);

	/* XXX I don't fully understand this.. but when we clear the
	 * interrupt in the CIPL0, we seem to need to reenable the
	 * interrupt at the EPC level.
	 */
	lock = EILOCK();
	ENABLE_INTR();
	EIUNLOCK(lock);
    }
    
    DTIMESTAMP(eistats.longvals[5]);
}

/* route interrupts to the given cpu if valid. This overrides the
 * configured routing
 */
static int
setintrcpu(int cpu)
{
    int s;

    if (!cpu_isvalid(cpu))
	return(EINVAL);

    /* If assigning to a NOINTR cpu, must be privileged */
    if (!cpu_allows_intr(cpu) &&
	!_CAP_ABLE(CAP_DEVICE_MGT))
	return(EPERM);

    s = EILOCK();

#ifdef ULI
    if (runtime_intrcpu == cpu) {
	EIUNLOCK(s);
	return(0);
    }

    /* resetting the interrupt cpu while a ULI is registered
     * or reserved would be decidedly uncool
     */
    if ( ! ULI_ISFREE(ulinum)) {
	EIUNLOCK(s);
	return(EBUSY);
    }
#endif

    /* disable incoming interrupts momentarily */
    DISABLE_INTR();

    /* reassign the interrupt cpu */
    EV_SET_REG((evreg_t *)(epcbase + EPC_IIDINTR4),
	       EVINTR_VECTOR(EVINTR_LEVEL_EPC_INTR4,
			     EVINTR_CPUDEST(cpu)));
#ifdef ULI
    runtime_intrcpu = cpu;
#endif

    /* reenable interrupts */
    if (intr_enabled)
	ENABLE_INTR();
    EIUNLOCK(s);

    return(0);
}

/* if cpu < 0, allow driver to run on any cpu for syscalls,
 * otherwise constrain to the given cpu if valid. This overrides
 * the configured routing
 */
static int
setsyscpu(int cpu)
{
    if (cpu < 0) {
	/* let it run on any cpu */
	eidevflag |= D_MP;
    }
    else {
	if (!cpu_isvalid(cpu))
	    return(EINVAL);
	cdevsw[MAJOR[EPCEI_MAJOR]].d_cpulock = cpu;
	eidevflag &= (~D_MP);
    }

    return(0);
}

/* ei initialization routine, called at system startup */
void
eiedtinit(struct edt *ep)
{
    int slot, x;
    short *outaddr;
    int cpu;
    /*REFERENCED*/
    int rc;
    char name[32];
    vertex_hdl_t vhdl;

    /* initialize static variables used in eiintr */
    for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {
	    retryinterval[cpu] = 1;
	    warninginterval[cpu] = 0;
	    warningnum[cpu] = 0;
    }

    /* find the master IO4 board and get the base address of its epc */
    for(slot = EV_MAX_SLOTS - 1; slot >= 0; slot--)
	if (epcbase = epc_probe(slot))
	    break;

    dprintf(("epcbase %x\n", epcbase));

    if (epcbase) {
	/* allocate a private page to hold the user counter. */
	if ((counterpage = kvpalloc(1, VM_VM, 0)) == 0)
	    return;
	
	/* always default to MP syscalls */
	config_syscallcpu = -1;
	setsyscpu(-1);

	/* get interrupt handler cpu */
	config_intrcpu = ep->v_cpuintr;
#ifdef ULI
	runtime_intrcpu = ep->v_cpuintr;
#endif
	dprintf(("ei: intr to cpu %d\n", config_intrcpu));
	
	add_to_inventory(INV_MISC, INV_MISC_EPC_EINT, slot, 0, 0);

	/* set up the EI interrupt */
	evintr_connect((evreg_t *)(epcbase + EPC_IIDINTR4),
		       EVINTR_LEVEL_EPC_INTR4,
		       SPLDEV,
		       config_intrcpu,
		       (EvIntrFunc)eiintr,
		       (void *)0);

	spinlock_init(&eilock, "eilock");

	/* make sure all output lines are deasserted and disable intrs */
	outaddr = (short*)(epcbase + EPC_EXTINT + 6);
	for(x = 0; x < 4; x++) {
	    *((volatile short*)outaddr) = 0;
	    outaddr += 4; /* 4*2=8 */
	}

	disable_intr();

	/* create a hwgraph node */
	sprintf(name, "%s/1", EI_HWG_DIR);
	rc = hwgraph_char_device_add(hwgraph_root, name, "ei", &vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(vhdl, HWGRAPH_PERM_EXTERNAL_INT);
    }
#ifdef ULI
    FREE_ULI(ulinum);
#endif
#ifdef ULI_TSTAMP
    FREE_ULI(ulits_ulinum);
    evintr_connect(0,
		   EVINTR_LEVEL_EPC_DUARTS4,
		   SPLHIDEV,
		   TSTAMP_CPU,
		   (EvIntrFunc)uli_tstamp_intr,
		   (void *)0);
#endif
}

/* ARGSUSED */
int
eiopen(dev_t *devp, int mode)
{
    struct pstate *ptmp;

    dprintf(("called eiopen on cpu %d\n", cpuid()));
    if (epcbase == 0)
	return(ENODEV);

    /* allocate a pstate if there isn't already one for this proc */
    ptmp = find_pstate();
    if (ptmp == 0)
	return(ENOMEM);

    return(0);
}

/* on last close of the device, we know all processes have gone away,
 * so nuke the entire pstate_list. Also, reset everything to default 
 * values
 */
/* ARGSUSED */
int
eiclose(dev_t dev)
{
    int x;
    short *outaddr;

    dprintf(("called eiclose on cpu %d\n", cpuid()));

    /* disable interrupts, deassert outgoing lines */
    disable_intr();
    
#ifdef ULI
    /* if we called eiclose with a ULI registered, we have to do
     * the ULI unregistration here, otherwise setintrcpu()
     * below fails. Anyway, it's meaningless to have a ULI registered
     * to a device whose interrupts have been disabled.
     */
    FREE_ULI(ulinum);
#endif

    outaddr = (short*)(epcbase + EPC_EXTINT + 6);
    for(x = 0; x < 4; x++) {
	*((volatile short*)outaddr) = 0;
	outaddr += 4; /* 4*2=8 */
    }

    /* reset some default values */
    outpulsewidth = PULSEWIDTH;
    outpulsewidth_bc = USEC_TO_BC(outpulsewidth);
    inpulsewidth = PULSEWIDTH;
    inpulsewidth_bc = USEC_TO_BC(inpulsewidth);
    inpulsestuck = PULSESTUCK;
    inpulsestuck_bc = USEC_TO_BC(inpulsestuck);

    setintrcpu(config_intrcpu);
    setsyscpu(config_syscallcpu);

    return(0);
}

#ifdef ULI

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* teardown func called when ULI proc exits */
/* ARGSUSED */
static void
ei_uli_teardown(struct uli *uli)
{
    FREE_ULI(ulinum);
    dprintf(("tore down ULI for ei\n"));
}

/* register a ULI to handle the ei */
static int
register_uli(__psint_t arg)
{
    struct uliargs args;
    struct uli *uli;
    int s, err;

    if (COPYIN_XLATE((void*)arg, &args, sizeof(args),
		     irix5_to_uliargs, get_current_abi(), 1)) {
	eprintf(("eiioctl: bad copyin\n"));
	return(EFAULT);
    }

    dprintf(("eiioctl args pc 0x%x sp 0x%x funcarg 0x%x\n",
	     args.pc, args.sp, args.funcarg));
    
    /* keep another process from changing runtime_intrcpu while we
     * call new_uli by reserving the ulinum.
     */
    s = EILOCK();
    if ( ! ULI_ISFREE(ulinum)) {
	EIUNLOCK(s);
	return(EBUSY);
    }
    RESERVE_ULI(ulinum);
    EIUNLOCK(s);

    if (err = new_uli(&args, &uli, runtime_intrcpu)) {
	eprintf(("eiioctl: new_uli failed\n"));
	FREE_ULI(ulinum);
	return(err);
    }
    
    uli->teardown = ei_uli_teardown;
    
    args.id = uli->index;
    if (XLATE_COPYOUT(&args, (void*)arg, sizeof(args),
		      uliargs_to_irix5, get_current_abi(), 1)) {
	eprintf(("eiioctl: bad copyout\n"));
	free_uli(uli);
	FREE_ULI(ulinum);
	return(EFAULT);
    }

    /* Once the ulinum is set, the ULI may be invoked at any time,
     * so only do this when everything is set to go. No need to lock
     * here since ulinum is reserved so nobody else can touch it.
     */
    ulinum = uli->index;

    dprintf(("eiioctl: return normal\n"));
    return(0);
}
#endif

/* ARGSUSED */
int
eiioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *cr, int rvp)
{
    struct pstate *ptmp = find_pstate();

    dprintf(("called eiioctl on cpu %d minor %d\n", cpuid(), minor(dev)));


    switch(cmd) {
	
	/* generate outgoing pulse. The pulse width is determined by
	 * the variable outpulsewidth. This cpu hangs during pulse
	 * generation.
	 */
      case EIIOCSTROBE: {
	  short *outaddr;
	  int s, x, flags;
	  evreg_t start;

	  DTIMESTAMP(eistats.longvals[0]);

	  flags = arg;

	  outaddr = (short*)(epcbase + EPC_EXTINT + 6);
	  dprintf(("outaddr %x\n", outaddr));

	  /* lock down interrupts so we don't take an interrupt and leave
	   * the output asserted for too long
	   */
	  s = spl7();
	  DTIMESTAMP(eistats.longvals[1]);
	  start = TIMESTAMP;
	  for(x = 0; x < 4; x++) {
	      if (flags & 1)
		  *((volatile short*)outaddr) = 1;

	      flags >>= 1;
	      outaddr += 4;
	  }
	  flags = arg;
	  
	  while(TIMESTAMP - start < outpulsewidth_bc);

	  for(x = 0; x < 4; x++) {
	      if (flags & 1)
		  *((volatile short*)outaddr) = 0;

	      flags >>= 1;
	      outaddr += 4; /* 4*2=8 */
	  }
	  splx(s);

	  DTIMESTAMP(eistats.longvals[2]);
	  return(0);
      }
      case EIIOCSETHI: {
	  short *outaddr;
	  int x;

	  outaddr = (short*)(epcbase + EPC_EXTINT + 6);
	  for(x = 0; x < 4; x++) {
	      if (arg & 1) {
		  dprintf(("sethi to %x\n", outaddr));
		  *((volatile short*)outaddr) = 1;
	      }
	      arg >>= 1;
	      outaddr += 4; /* 4*2=8 */
	  }
	  return(0);
      }
      case EIIOCSETLO: {
	  short *outaddr;
	  int x;
	  
	  outaddr = (short*)(epcbase + EPC_EXTINT + 6);
	  for(x = 0; x < 4; x++) {
	      if (arg & 1) {
		  dprintf(("setlo to %x\n", outaddr));
		  *((volatile short*)outaddr) = 0;
	      }
	      arg >>= 1;
	      outaddr += 4; /* 4*2=8 */
	  }
	  return(0);
      }
      case EIIOCENABLE:
	/* enable incoming interrupts */
	enable_intr();
	return(0);

      case EIIOCDISABLE:
	/* disable incoming interrupts */
	disable_intr();
	return(0);

      case EIIOCRECV: {
	  struct eiargs eiargs;
	  int rv, haveto, s;
	  timespec_t ts, rts;

	  DTIMESTAMP(eistats.longvals[10]);

	  if (ptmp == 0)
	      return(ENOMEM);

	  if (COPYIN_XLATE((char *)arg, (char*)&eiargs, sizeof(eiargs),
	  			irix5_to_eiargs_xlate, get_current_abi(),
				1))
	      return(EFAULT);

	  /* if caller passed in intval true, flush the intr queue. This
	   * doesn't need a lock because increments and decrements of
	   * this counter are done with ll-sc
	   */
	  if (eiargs.intval)
	      ptmp->privcounter = 0;

	  /* go to sleep if the queue is empty and the user requested it */
	  if (ptmp->privcounter == 0 &&
	      (eiargs.timeval.tv_sec || eiargs.timeval.tv_usec)) {

	      /* if wakeup is not infinite, set a timer */
	      haveto = 0;
	      if (eiargs.timeval.tv_sec != -1) {
		  if (hzto(&eiargs.timeval) == 0) {
			/* provide minimum timeout */
			tick_to_timespec(1, &ts, NSEC_PER_USEC);
		  } else {
			ts.tv_sec = eiargs.timeval.tv_sec;
			ts.tv_nsec = eiargs.timeval.tv_usec * NSEC_PER_USEC;
		  }
		  haveto = 1;
	      }

	      /* sleep until either the counter is incremented (by the
	       * interrupt handler) or our timeout, if it exists,
	       * expires, or a signal comes in
	       */
	      s = mutex_spinlock(&ptmp->lock);
	      while (ptmp->privcounter == 0) {

		  if (haveto) {
			rv = sv_timedwait_sig(&ptmp->wait, 0,
			    &ptmp->lock, s, 1, &ts, &rts);
		  } else {
			rv = sv_wait_sig(&ptmp->wait, 0, &ptmp->lock, s);
		  }
		  if (rv == -2) {
		      /*
		       * got signal but it was cancelled - keep going
		       */
		      ts = rts;
		  } else {
		      /* got signal or timeout - time to quit */
		      break;
		  }
	          s = mutex_spinlock(&ptmp->lock);
	      }
	  }

	  /* if we got an intr, dequeue it and notify the caller */
	  if (ptmp->privcounter > 0) {
	      ATOMDEC(ptmp->privcounter);
	      dprintf(("cnt now down to %d\n", ptmp->privcounter));
	      eiargs.intval = 1;
	  }
	  else
	      eiargs.intval = 0;

	  XLATE_COPYOUT((char*)&eiargs, (char *)arg, sizeof(eiargs),
	  		eiargs_to_irix5_xlate, get_current_abi(), 1);
	  DTIMESTAMP(eistats.longvals[7]);
	  return(0);
      }

      case EIIOCSETOPW:
	if (arg < 2 || arg > 1000)
	    return(EINVAL);
	outpulsewidth = arg;
	outpulsewidth_bc = USEC_TO_BC(outpulsewidth);
	return(0);

      case EIIOCGETOPW:
	if (copyout((char*)&outpulsewidth, (char*)arg, sizeof(outpulsewidth)))
	    return(EFAULT);
	return(0);

      case EIIOCSETIPW:
	if (arg < 2 || arg > 1000)
	    return(EINVAL);
	inpulsewidth = arg;
	inpulsewidth_bc = USEC_TO_BC(inpulsewidth);
	return(0);

      case EIIOCGETIPW:
	if (copyout((char*)&inpulsewidth, (char*)arg, sizeof(inpulsewidth)))
	    return(EFAULT);
	return(0);

      case EIIOCSETSPW:
	if (arg < 5 || arg > 1000000)
	    return(EINVAL);
	inpulsestuck = arg;
	inpulsestuck_bc = inpulsestuck * 1000 / NSEC_PER_CYCLE;
	return(0);

      case EIIOCGETSPW:
	if (copyout((char*)&inpulsestuck, (char*)arg, sizeof(inpulsestuck)))
	    return(EFAULT);
	return(0);
	
      case EIIOCSETSIG: {
	  if (ptmp == 0)
	      return(ENOMEM);

	  ptmp->signal = arg;
	  return(0);
      }

      case EIIOCCLRSTATS:
	bzero(&eistats, sizeof(eistats));
	return(0);

      case EIIOCGETSTATS:
	DTIMESTAMP(eistats.longvals[11]);
	if (copyout((char*)&eistats, (char*)arg, sizeof(eistats)))
	    return(EFAULT);
	return(0);

      case EIIOCSETINTRCPU:
	return(setintrcpu(arg));

      case EIIOCSETSYSCPU:
	return(setsyscpu(arg));

#ifdef ULI
      case EIIOCREGISTERULI:
	return(register_uli(arg));
#endif

#ifdef ULI_TSTAMP
	/* a bunch of internal use only ioctls to get ULI and
	 * interrupt latencies.
	 */
      case 1000: {
	  int s;

	  /* trigger the interrupt */
	  s = spl7();
	  EV_SET_LOCAL(EV_SENDINT,
		       EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUARTS4,
				     EVINTR_CPUDEST(TSTAMP_CPU)));

	  /* keep a separate copy of this timestamp rather than writing
	   * directly to the timestamp array, which would invalidate that
	   * cache line on the interrupt cpu
	   */
	  trigger_ts = *(((volatile int*)EV_RTC)+1);
	  splx(s);
	  return(0);
      }

      case 1001:
	/* get the tstamps */
	if (copyout((char*)&trigger_ts, (char*)arg, sizeof(int))
#ifdef ULI_TSTAMP1
	    || copyout((char*)&uli_saved_tstamps[1], (char*)(((int*)arg)+1),
		       sizeof(int) * (TS_MAX - 1))
#endif
	    )
	    return(EFAULT);
	return(0);

      case 1002: {
	  /* attach the ULI */
	  struct uliargs args;
	  struct uli *uli;
	  int err;
	  
	  if (COPYIN_XLATE((void*)arg, &args, sizeof(args),
			   irix5_to_uliargs, get_current_abi(), 1))
	      return(EFAULT);
	  
	  if ( ! ULI_ISFREE(ulits_ulinum))
	      return(EBUSY);
	  
	  if (err = new_uli(&args, &uli, TSTAMP_CPU))
	      return(err);
	  
	  uli->teardown = uli_tstamp_teardown;
	  
	  args.id = uli->index;
	  if (XLATE_COPYOUT(&args, (void*)arg, sizeof(args),
			    uliargs_to_irix5, get_current_abi(), 1)) {
	      free_uli(uli);
	      return(EFAULT);
	  }
	  ulits_ulinum = uli->index;
	  return(0);
      }

#ifdef ULI_TSTAMP1
      case 1003:
	return(0);
#endif
	
#endif /* ULI_TSTAMP */

      default:
	dprintf(("unknown cmd %d\n", cmd));
	return(EINVAL);
    }
}

/* map the counterpage into the caller's address space. The counter
 * page is used to hold a global counter that is incremented each
 * time an interrupt is received. This counter is never reset. The
 * eic library routines implement a busywait feature by examining
 * this counter and waiting for it to be incremented. A per-process
 * queue is implemented in usermode by keeping a separate counter
 * and comparing it to the global counter.
 */
/* ARGSUSED */
int
eimap(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
    caddr_t mapaddr;

    dprintf(("map dev %x off %x len %d\n", dev, off, len));
    
    /* len must be 1 page */
    if (len != NBPP)
	return(EINVAL);

    /* if off is 0, request is to map the counter page for the
     * calling process
     */
    if (off == 0) {
	auto int max;
	/* don't allow writing to the kernel page */
	v_getprot(vt, NULL, &max, NULL);
	if (max & PROT_WRITE)
	    return(EACCES);
	mapaddr = counterpage;
    }

    else
	return(EINVAL);
	
    return(v_mapphys(vt, mapaddr, len));
}

/* ARGSUSED */
int
eiunmap(dev_t dev, vhandl_t *vt)
{
    return(0);
}

#if _MIPS_SIM == _ABI64

struct irix5_eiargs {
	int			intval;
	struct irix5_timeval	timeval;
};

/*ARGSUSED*/
int
irix5_to_eiargs_xlate(enum xlate_mode mode, void *to, int count,
					register xlate_info_t *info)
{
	struct eiargs *ei_to = (struct eiargs *)to;
	struct irix5_eiargs *ei_from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_eiargs) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
						sizeof(struct irix5_eiargs));
		info->copysize = sizeof(struct irix5_eiargs);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_eiargs));
	ASSERT(info->copybuf != NULL);

	ei_from = (struct irix5_eiargs *)info->copybuf;
	ei_to->intval = ei_from->intval;
	irix5_to_timeval(&ei_to->timeval, &ei_from->timeval);
	return 0;
}

/*ARGSUSED*/
int
eiargs_to_irix5_xlate(void *to, int count, register xlate_info_t *info)
{
	struct eiargs *ei_from = (struct eiargs *)to;
	struct irix5_eiargs *ei_to;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_eiargs) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_eiargs));
	info->copysize = sizeof(struct irix5_eiargs);

	ei_to = (struct irix5_eiargs *)info->copybuf;
	ei_to->intval = ei_from->intval;
	timeval_to_irix5(&ei_from->timeval, &ei_to->timeval);

	return 0;
}
#endif /* _ABI64 */
#endif /* EVEREST */
