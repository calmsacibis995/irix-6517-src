/* Copyright 1986-1999 Silicon Graphics Inc., Mountain View, CA. */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.218 $"

/*
 *	UNIX Operating System Profiler
 *
 *	Sorted Kernel text addresses are written into driver.  At each
 *	clock interrupt a binary search locates the counter for the
 *	interval containing the captured PC and increments it.
 *	The last counter is used to hold the User mode counts.
 */

#include <bstring.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/loaddrs.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/sbd.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/capability.h>
#include <sys/sema.h>
#include <sys/kuio.h>
#include <sys/kfcntl.h>
#include <sys/atomic_ops.h>
#include <sys/ktime.h> /* for fastick_callback_required_flags */
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/prf.h>
#include <sys/rtmon.h>
#include <sys/sema_private.h>
#include <sys/space.h>
#include <sys/buf.h>
#include "os/proc/pproc_private.h"
#include <sys/hwperfmacros.h>
#include <sys/ioctl.h>
#include <sys/termio.h>


/*
 *	Profiling support definitions
 *	=============================
 */
typedef __psunsigned_t 	symaddr_t;

int prfdevflag = D_MP;

extern unsigned int prfstat;	/* state of profiler, in master.d/kernel */
static int prfmax;		/* number of loaded text symbols */
static symaddr_t *prfsym;	/* text symbols */
static mutex_t prf_lock;

static int prf_setproftype(unsigned int proftype);
static int prf_setrange(unsigned int oldrange, unsigned int newrange);
static int prf_setdomain(unsigned int olddomain, unsigned int newdomain);
static void prfcpuaction(void (*action)(void));
void user_code(void), low_ipl(void), loadable_driver(void);

#define PRF_SWTCH_SAMPLERATE	20


/*
 *	Par support definitions
 *	=======================
 */
#define	FAWLTMAX	((NUMB_KERNEL_TSTAMPS-2)*sizeof (tstamp_event_entry_t))
static int	par_maxindbytes = 0;

static int fawlty_start(pid_t, __int64_t, int flags, cred_t *);
static int fawlty_stop(pid_t, __int64_t, cred_t *);
static int fawltyunscan(proc_t *, void *, int);
static int fawlty_control(pid_t pid, int allow);
static int getstrlen(usysarg_t);
static int getargsize(sysargdesc_t, sysarg_t *, int, int, sysarg_t);
static void sysrec(event_t, short, struct sysent *,
		sysarg_t *, sysarg_t *, int);
static int isinputarg(sysargdesc_t);


/*
 *	/dev/prf and /dev/par driver interface routines
 *	===============================================
 */
void
prfinit(void)
{
	mutex_init(&prf_lock, MUTEX_DEFAULT, "prf_lock");
#if DEBUG
	if (kdebug) {
		startprfclk();
		prfstat |= PRF_DEBUG;
		prf_setproftype(PRF_PROFTYPE_NONE);
	}
#endif
}

/* ARGSUSED */
int
prfopen(dev_t *devp, int flag, int otype, struct cred *crp)
{
	/*
	 * allow opens of /dev/par for writing no matter what their ABI is
	 */
	if (minor(*devp) == 100)
		return 0;
#if _MIPS_SIM == _ABI64
	if (!ABI_IS_64BIT(get_current_abi()))
		return EINVAL;
#endif
	return 0;
}

/* ARGSUSED */
int
prfclose(dev_t dev, int flag, int otype, struct cred *crp)
{
	return 0;
}

/*
 * prfread(dev_t dev, uio_t *uiop, cred_t *crp)
 *
 * Read out symbols and function counter buckets.
 */
/* ARGSUSED */
int
prfread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	int error;
	cpuid_t cpu;
	size_t symbufsize, ctrbufsize, iosize;
	extern int numcpus;

	if (minor(dev) != 0)
		return ENXIO;

	mutex_lock(&prf_lock, PZERO);
	if ((prfstat & PRF_VALID_SYMTAB) == 0
	    || (prfstat & PRF_RANGE_MASK) != PRF_RANGE_PC) {
		mutex_unlock(&prf_lock);
		return ENXIO;
	}

	/* compute size required:
	 * prfmax symbols +
	 * numcpus ints +
	 * numcpus * prfmax counters
	 */
	symbufsize = prfmax * sizeof(symaddr_t);
	ctrbufsize = prfmax * sizeof(uint);
	iosize = symbufsize + numcpus * (sizeof(int) + ctrbufsize);

	if (uiop->uio_resid < iosize) {
		mutex_unlock(&prf_lock);
		return EINVAL;
	}

	/* copy out addresses */
	error = uiomove((caddr_t) prfsym, symbufsize, UIO_READ, uiop);
	if (error) {
		mutex_unlock(&prf_lock);
		return error;
	}

	for (cpu = 0; cpu < maxcpus; cpu++) {
		int icpu;
		if (pdaindr[cpu].CpuId == -1)
			continue;
		ASSERT(pdaindr[cpu].pda->p_prfptr);

		icpu = cpu;
		error = uiomove((caddr_t) &icpu, sizeof(int), UIO_READ, uiop);
		if (error)
			break;
		error = uiomove((caddr_t)pdaindr[cpu].pda->p_prfptr,
				ctrbufsize, UIO_READ, uiop);
		if (error)
			break;
	}
	mutex_unlock(&prf_lock);
	return error;
}

/*
 * prfwrite(dev_t dev, uio_t *uiop, cred_t *crp)
 *
 * Load kernel text symbol addresses.
 */
/* ARGSUSED */
int
prfwrite(dev_t dev, uio_t *uiop, cred_t *crp)
{
	int error;

	if (minor(dev) != 0)
		return ENXIO;

	mutex_lock(&prf_lock, PZERO);
	error = 0;

	if ((prfstat & PRF_PROFTYPE_MASK) != PRF_PROFTYPE_NONE) {
		error = EBUSY;
		goto prf_exit;
	}
	if (uiop->uio_resid & (sizeof(symaddr_t) - 1) ||
	    uiop->uio_resid < 3 * sizeof(symaddr_t)) {
		error = E2BIG;
		goto prf_exit;
	}

	/*
	 * Deallocate any old symbole table and allocate space for the new
	 * symbol table.
	 */
	prfmax = uiop->uio_resid/sizeof(symaddr_t);
	if (prfsym)
		kern_free(prfsym);
	if ((prfsym = kern_malloc(uiop->uio_resid)) == NULL) {
		error = ENOSPC;
		goto prf_exit;
	}

	/*
	 * Read in new symbol table and verify that all the addresses within
	 * it are strictly increasing ... should probably also check to make
	 * sure they're valid kernel text addresses ...
	 */
	error = uiomove((caddr_t) prfsym, uiop->uio_resid, UIO_WRITE, uiop);
	if (!error) {
		symaddr_t *psp;
		for (psp = &prfsym[1]; psp != &prfsym[prfmax]; psp++)
			if (psp[0] < psp[-1]) {
				error = EINVAL;
				break;
			}
	}

 prf_exit:
	if (error) {
		if (error != EBUSY  &&	error != E2BIG)
			prfstat &= ~PRF_VALID_SYMTAB;
	} else
		prfstat |= PRF_VALID_SYMTAB;
	mutex_unlock(&prf_lock);
	return error;
}

/* ARGSUSED */
int
prfioctl(dev_t dev, int cmd, __psint_t arg, int mode, cred_t *crp, int *rval)
{
	struct {
		__int64_t cookie;
		pid_t	  pid;
	} parargs;

	switch (cmd) {
	    default:
		/*
		 * This also catches PAR_OLDSYSCALL and PAR_OLDSYSCALLINH
		 * along with invalid combinations of PAR_SYSCALL, PAR_SWTCH
		 * and PAR_INHERIT.
		 */
		return EINVAL;

	    /*
	     * /dev/prf ioctl()'s
	     */
	    case PRF_GETSTATUS:
		*rval = prfstat & ~PRF_DEBUG;
		break;

	    case PRF_GETNSYMS:
		*rval = prfmax;
		break;

	    case PRF_SETPROFTYPE:
		return prf_setproftype(arg);

	    /*
	     * /dev/par ioctl()'s
	     */
	    case PAR_SYSCALL:
	    case PAR_SWTCH:
	    case PAR_SYSCALL|PAR_SWTCH:
	    case PAR_INHERIT|PAR_SYSCALL:
	    case PAR_INHERIT|PAR_SWTCH:
	    case PAR_INHERIT|PAR_SYSCALL|PAR_SWTCH:
		if (copyin((void*) arg, &parargs, sizeof parargs))
			return EFAULT;
		return fawlty_start(parargs.pid, parargs.cookie,
				    (((cmd & PAR_SYSCALL) ? SPARSYS   : 0) |
				     ((cmd & PAR_SWTCH)   ? SPARSWTCH : 0) |
				     ((cmd & PAR_INHERIT) ? SPARINH   : 0)),
				    crp);

	    case PAR_DISABLE:
		if (copyin((void*) arg, &parargs, sizeof parargs))
			return EFAULT;
		return fawlty_stop(parargs.pid, parargs.cookie, crp);

	    case PAR_DISALLOW:
	    case PAR_ALLOW:
		return fawlty_control(arg, cmd == PAR_ALLOW);

	    case PAR_SETMAXIND:
		if (arg > PAR_MAXINDBYTES)
			arg = PAR_MAXINDBYTES;
		else if (arg < 0)
			arg = 0;
		par_maxindbytes = arg;
		break;
	}
	return 0;
}


/*
 *	Profiling support.
 *	==================
 */

/*
 * int prf_setproftype(unsigned int proftype)
 *
 * Sets the current profiling type and enables/disables whatever infrastructure
 * is necessary to support the profiling type.  For instance, for profiling in
 * the time domain the fast tick callback needs to be set/unset and the various
 * CPUs' clocks need to be cranked up.  Or, another example, profiling PCs
 * requires allocating function counter buckets.  If no errors occur, 0 is
 * returned; otherwise the errno is returned and all profiling is disabled
 * (the profiling type is reset to PRF_PROFTYPE_NONE).
 */
static int
prf_setproftype(unsigned int proftype)
{
	int error = 0;
	unsigned int olddomain = prfstat  & PRF_DOMAIN_MASK;
	unsigned int newdomain = proftype & PRF_DOMAIN_MASK;
	unsigned int oldrange  = prfstat  & PRF_RANGE_MASK;
	unsigned int newrange  = proftype & PRF_RANGE_MASK;

	/*
	 * No bits other than profiling type information may be set and the
	 * profiling domain and range must either both be NONE or both be
	 * not NONE.  Also need to be sure that no unsupported domains or
	 * ranges are specified.
	 */
	if ((proftype & ~PRF_PROFTYPE_MASK) ||
	    (((proftype & PRF_DOMAIN_MASK) == PRF_DOMAIN_NONE)
	     != ((proftype & PRF_RANGE_MASK) == PRF_RANGE_NONE)))
		return EINVAL;
	switch (newdomain) {
	    case PRF_DOMAIN_NONE:
	    case PRF_DOMAIN_TIME:
	    case PRF_DOMAIN_SWTCH:
	    case PRF_DOMAIN_IPL:
		break;

	    case PRF_DOMAIN_DCACHE1:
	    case PRF_DOMAIN_DCACHE2:
	    case PRF_DOMAIN_ICACHE1:
	    case PRF_DOMAIN_ICACHE2:
	    case PRF_DOMAIN_SCFAIL:
	    case PRF_DOMAIN_CYCLES:
	    case PRF_DOMAIN_BRMISS:
	    case PRF_DOMAIN_UPGCLEAN:
	    case PRF_DOMAIN_UPGSHARED:
#if defined(R10000)
		break;
#else
		return ENOTSUP;
#endif

	    default:
		return EINVAL;
	}
	switch (newrange) {
	    case PRF_RANGE_NONE:
	    case PRF_RANGE_PC:
	    case PRF_RANGE_STACK:
		break;

	    default:
		return EINVAL;
	}


	mutex_lock(&prf_lock, PZERO);

	/*
	 * Can't set the profiling mode if we don't have a valid symbol table.
	 */
	if (!(prfstat & PRF_VALID_SYMTAB)) {
		mutex_unlock(&prf_lock);
		return EINVALSTATE;
	}

	/*
	 * If we're not changing the profiling mode then we can simply return
	 * here.  If we are changing the profiling mode then we need to set
	 * the current profiling status to indicate that profiling is disabled
	 * before doing anything to prevent prfintr() and friends from becoming
	 * confused.
	 */
	if ((prfstat & PRF_PROFTYPE_MASK) == (proftype & PRF_PROFTYPE_MASK)) {
		mutex_unlock(&prf_lock);
		return 0;
	}
	prfstat = (prfstat & ~PRF_PROFTYPE_MASK) | PRF_PROFTYPE_NONE;

	/*
	 * Change the profiling range and domain.  If any errors occur, tear
	 * down all profiling support and return with the errno and profiling
	 * disabled.
	 */
	error = prf_setrange(oldrange, newrange);
	if (error) {
		/* tear down olddomain support */
		prf_setdomain(olddomain, PRF_DOMAIN_NONE);
		mutex_unlock(&prf_lock);
		return error;
	}
	error = prf_setdomain(olddomain, newdomain);
	if (error) {
		/* tear down new range support */
		prf_setrange(newrange, PRF_RANGE_NONE);
		mutex_unlock(&prf_lock);
		return error;
	}

	/*
	 * All profiling support for the new profiling type is in place: set
	 * the new profiling type and return successfully.
	 */
	prfstat = (prfstat & ~PRF_PROFTYPE_MASK) | proftype;
	mutex_unlock(&prf_lock);
	return 0;
}

/*
 * int prf_setrange(unsigned int oldrange, unsigned int newrange)
 *
 * Changes the current profiling range from oldrange to new range.  This
 * tears down any support needed for oldrange and sets any support needed
 * for newrange.  If no errors occur, 0 is returned; otherwise the errno is
 * returned and all profiling range support is dismantled.
 */
static int
prf_setrange(unsigned int oldrange, unsigned int newrange)
{
	if (oldrange == newrange)
		return 0;

	/*
	 * The profiling ranges are different so we'll need to tear down
	 * any stuff needed by the old profiling range and build up anything
	 * needed by the new range.
	 */
	switch (oldrange) {
	    case PRF_RANGE_NONE:
	    case PRF_RANGE_STACK:
		break;

	    case PRF_RANGE_PC: {
		cpuid_t cpu;
		/*
		 * Delay 1 tick just to be sure that we don't catch any
		 * prfintr() instance on another CPU incrementing a function
		 * bucket just as we delete them.
		 */
		delay(1);
		for (cpu = 0; cpu < maxcpus; cpu++)
			if (pdaindr[cpu].CpuId != -1 && pdaindr[cpu].pda->p_prfptr) {
				kern_free(pdaindr[cpu].pda->p_prfptr);
				pdaindr[cpu].pda->p_prfptr = NULL;
			}
		break;
	    }

	    default:
		cmn_err(CE_PANIC, "prf_setproftype: bogus current"
			" profiling range %d\n", oldrange);
		/*NOTREACHED*/
	}

	/*
	 * Now construct any support need by the new range.
	 */
	switch (newrange) {
	    case PRF_RANGE_NONE:
	    case PRF_RANGE_STACK:
		break;

	    case PRF_RANGE_PC: {
		size_t ctrbufsize = prfmax*sizeof(uint);
		cpuid_t cpu;
		for (cpu = 0; cpu < maxcpus; cpu++)
			if (pdaindr[cpu].CpuId != -1) {
				pdaindr[cpu].pda->p_prfptr =
				    kern_malloc_node(ctrbufsize,
						     pdaindr[cpu].pda->p_nodeid);
				if (!pdaindr[cpu].pda->p_prfptr) { 
					pdaindr[cpu].pda->p_prfptr = kern_malloc(ctrbufsize);
					if (!pdaindr[cpu].pda->p_prfptr) {
						/*
					 	 * Ran out of space for function
					 	 * counter buckets.  Free up any
					 	 * already allocated and return the
					 	 * error.
					 	 */
						while (--cpu >= 0)
							kern_free(pdaindr[cpu].pda->p_prfptr);
						return ENOSPC;
					}
				}
				bzero(pdaindr[cpu].pda->p_prfptr, ctrbufsize);
			}
		break;
	    }
	}
	return 0;
}

/*
 * int prf_setdomain(unsigned int olddomain, unsigned int newdomain)
 *
 * Changes the current profiling domain from olddomain to new domain.  This
 * tears down any support needed for olddomain and sets any support needed
 * for newdomain.  If no errors occur, 0 is returned; otherwise the errno is
 * returned and all profiling domain support is dismantled.
 */
static int
prf_setdomain(unsigned int olddomain, unsigned int newdomain)
{
	if (olddomain == newdomain)
		return 0;

	/*
	 * N.B. The IPL domain uses the TIME domain 1ms interrupt to
	 * statistically sample in the TIME domain, but only those samples
	 * which happen while the interrupt priority level is non-zero.  This
	 * let's us rapidly find the sections of code which are holding
	 * interrupts off the most.
	 */

	/*
	 * The profiling domains of the previous and new profiling modes
	 * are different, so we need to turn off any profiling support
	 * associated with the previous profiling domain and set up any
	 * needed by the new profiling domain.
	 */
	switch (olddomain) {
	    case PRF_DOMAIN_NONE:
		break;

	    case PRF_DOMAIN_TIME:
	    case PRF_DOMAIN_IPL: /* uses the TIME domain interrupt */
		atomicClearUint(&fastick_callback_required_flags,
				FASTICK_CALLBACK_REQUIRED_PRF_MASK);
		prfcpuaction(stopprfclk);
		break;

#if defined(R10000)
	    case PRF_DOMAIN_DCACHE1:
	    case PRF_DOMAIN_DCACHE2:
	    case PRF_DOMAIN_ICACHE1:
	    case PRF_DOMAIN_ICACHE2:
	    case PRF_DOMAIN_SCFAIL:
	    case PRF_DOMAIN_CYCLES:
	    case PRF_DOMAIN_BRMISS:
	    case PRF_DOMAIN_UPGCLEAN:
	    case PRF_DOMAIN_UPGSHARED:
		hwperf_disable_sys_counters();
		break;
#endif

	    case PRF_DOMAIN_SWTCH: {
		cpuid_t cpu;
		for (cpu = 0; cpu < maxcpus; cpu++)
		    if (pdaindr[cpu].CpuId != -1)
			pdaindr[cpu].pda->p_prfswtch = 0;
		break;
	    }

	    default:
		cmn_err(CE_PANIC, "prf_setproftype: bogus current"
			" profiling domain %d\n", olddomain);
		/*NOTREACHED*/
	}

	/*
	 * Now enable any profiling support needed by the new profiling
	 * domain.
	 */
	switch (newdomain) {
#if defined(R10000)
	    int event, eindx, efreq;
#endif
	    case PRF_DOMAIN_NONE:
		break;

	    case PRF_DOMAIN_TIME:
	    case PRF_DOMAIN_IPL: /* uses the TIME domain interrupt */
		atomicSetUint(&fastick_callback_required_flags,
			      FASTICK_CALLBACK_REQUIRED_PRF_MASK);
		prfcpuaction(startprfclk);
		break;

#if defined(R10000)
	    /*
	     * Performance event counter based profiling.  We can cause the
	     * CPU to count certain events and interrupt when a certain
	     * number have occured.  Unfortunately the interface to the
	     * hardware counters stuff is something of a disaster as an API
	     * -- please pardon the following grot.
	     */
	    case PRF_DOMAIN_DCACHE1:
		event = (IS_R12000() ? HWPERF_PRFCNT_PDCMISS : HWPERF_C0PRFCNT1_PDCMISS);
		eindx = event + HWPERF_CNT1BASE;
		efreq = 500;
		goto setup_evc_interrupt;
	    case PRF_DOMAIN_DCACHE2:
		event = (IS_R12000() ? HWPERF_PRFCNT_SDCMISS : HWPERF_C0PRFCNT1_SDCMISS);
		eindx = event + HWPERF_CNT1BASE;
		efreq = 500;
		goto setup_evc_interrupt;
	    case PRF_DOMAIN_ICACHE1:
		event = HWPERF_C0PRFCNT0_PICMISS;
		eindx = event;
		efreq = 2500;
		goto setup_evc_interrupt;
	    case PRF_DOMAIN_ICACHE2:
		event = HWPERF_C0PRFCNT0_SICMISS;
		eindx = event;
		efreq = 100;
		goto setup_evc_interrupt;
	    case PRF_DOMAIN_SCFAIL:
		event = HWPERF_C0PRFCNT0_FAILSC;
		eindx = event;
		efreq = 100;
		goto setup_evc_interrupt;
	    case PRF_DOMAIN_CYCLES:
		event = HWPERF_C0PRFCNT0_CYCLES;
		eindx = event;
		efreq = 200000;
		goto setup_evc_interrupt;

	    case PRF_DOMAIN_BRMISS:
		event = (IS_R12000() ? HWPERF_PRFCNT_BRMISS : HWPERF_C0PRFCNT1_BRMISS) ;
		eindx = event + HWPERF_CNT1BASE;
		efreq = 1000;
		goto setup_evc_interrupt;

	    case PRF_DOMAIN_UPGCLEAN:
	    	if (!IS_R12000()) {
			event = HWPERF_C0PRFCNT1_SPEXCLEAN;
			eindx = event + HWPERF_CNT1BASE;
			efreq = 500;
		}
		else
			return ENOTSUP;
		goto setup_evc_interrupt;

	    case PRF_DOMAIN_UPGSHARED:
	    	if (!IS_R12000()) {
			event = HWPERF_C0PRFCNT1_SPEXSHR;
			eindx = event + HWPERF_CNT1BASE;
			efreq = 500;
		}
		else
			return ENOTSUP;
		goto setup_evc_interrupt;

	    setup_evc_interrupt: {
		hwperf_profevctrarg_t evc;
		int cm_gen;

		bzero(&evc, sizeof evc);
		evc.hwp_evctrargs.hwp_evctrl[eindx].hwperf_spec =
			HWPERF_CNTEN_U|HWPERF_CNTEN_K|HWPERF_CNTEN_E
			|HWPERF_CNTEN_IE
			|(event << HWPERF_EVSHIFT);
		evc.hwp_ovflw_freq[eindx] = efreq;
		evc.hwp_ovflw_sig = 0;
		return hwperf_enable_sys_counters(&evc, NULL,
						  HWPERF_PROFILING, &cm_gen);
	    }
#endif

	    case PRF_DOMAIN_SWTCH: {
		cpuid_t cpu;
		for (cpu = 0; cpu < maxcpus; cpu++)
		    if (pdaindr[cpu].CpuId != -1) {
			pdaindr[cpu].pda->p_prfswtchcnt = PRF_SWTCH_SAMPLERATE;
			pdaindr[cpu].pda->p_prfswtch = 1;
		    }
		break;
	    }
	}
	return 0;
}


/*
 * prfcpuaction(cpuacfunc_t action)
 *
 * Executes the indicated function on every enabled CPU; dropping and
 * reacquiring the prf_lock around the calls.
 */
static void
prfcpuaction(void (*action)(void))
{
	cpuid_t cpu;

	mutex_unlock(&prf_lock);
	action();
	for (cpu = 0; cpu < maxcpus; cpu++) {
		if (pdaindr[cpu].CpuId == -1 ||
		    pdaindr[cpu].CpuId == private.p_cpuid)
			continue;
		cpuaction(cpu, (cpuacfunc_t)action, A_NOW);
	}
	mutex_lock(&prf_lock, PZERO);
}


/*
 * If we take a profiling sample while executing in user code, we
 * attribute the sample to user_code().  If we take a profiling sample
 * in the IPL domain and we're at at BASEPRI(sr), we attribute the
 * sample to low_ipl().  If we find end up with a loadable driver PC we
 * use loadable_driver().
 */
void
user_code(void)
{
}

void
low_ipl(void)
{
}

void
loadable_driver(void)
{
}


/*
 * int findfunc(symaddr_t pc, int skipoob)
 *
 * Find index of symbol table entry that covers the indicated PC value.
 * prfld(1) loads a 0 into prfsym[0] and the value of "_end" into
 * prfsym[prfmax-1]; prfsym[1] holds the first legal text address.  If
 * skipoob is TRUE, PC values which are out of the text bounds [prfsym[1],
 * prfsym[prfmax-1]) will result in a -1 return.  Otherwise, PC's less than
 * prfsym[1] will return 0 and those greater than or equal to
 * prfsym[prfmax-1] will return prfmax-1.
 */
static __inline int
findfunc(symaddr_t pc, int skipoob)
{
	int l, h, m;

	ASSERT(prfsym);
	if (pc < prfsym[1] || pc >= prfsym[prfmax-1])
		return skipoob ? -1 : pc < prfsym[1] ? 0 : prfmax-1;

	l = 1;
	h = prfmax-1;
	while ((m = (l + h) / 2) != l)
		if (pc < prfsym[m])
			h = m;
		else
			l = m;
	return m;
}


/*
 * void
 * prfintr(k_machreg_t pc, k_machreg_t sp,
 *	   k_machreg_t ra, k_machreg_t sr)
 *
 * Implement a profiling ``interrupt''.  Decode the profiling range and
 * invoke the appropriate profiling code.
 */
void
prfintr(__psunsigned_t pc, __psunsigned_t sp,
	__psunsigned_t ra, k_machreg_t sr)
{
	unsigned int domain = prfstat & PRF_DOMAIN_MASK;
	unsigned int range  = prfstat & PRF_RANGE_MASK;
#if defined(DEBUG) || defined(DEBUG_CONPOLL)
	if (kdebug && dbgconpoll) {
		/* if free-running then for debugger entry every tick,
		 * else every XX/XXXths of a sec
		 */
		if (private.fclock_freq == 0 || private.p_dbgcntdown++ > 0x4f) {
			private.p_dbgcntdown = 0x0;
			du_conpoll();
		}
	}
#endif
	switch (range) {
	    case PRF_RANGE_PC:{
		/*
		 * Increment the bucket associated with the exception frame
		 * PC.  For exceptions from user mode we increment the last
		 * bucket.  The buckets are assigned one per function rather
		 * than per range of PCs as is done in user PC profiling.
		 * This is good enough since stack profiling provides much
		 * better data if finer grained profiling insight is
		 * desired.
		 */
		unsigned int *prfctr;

		if (IS_KSEG1(pc))
			pc = K1_TO_K0(pc);
		if (!IS_MAPPED_KERN_SPACE(pc) && IS_KSEG2(pc))
			pc = (__psunsigned_t)loadable_driver;

		/*
		 * XXX we may want to precompute findfunc(user_code, 0) and
		 * XXX findfunc(low_ipl, 0) when the symbol table is loaded
		 * XXX in order to avoid computing it here ...
		 */
		prfctr = private.p_prfptr;
		if (USERMODE(sr))
			prfctr[findfunc((symaddr_t)user_code, 0)]++;
		else if (domain == PRF_DOMAIN_IPL && BASEPRI(sr))
			prfctr[findfunc((symaddr_t)low_ipl, 0)]++;
		else
			prfctr[findfunc(pc, 0)]++;
		break;
	    }

/* number of PC values per rtmon argument */
#define NPCRTARG (sizeof(uint64_t)/sizeof(__psunsigned_t))

#if _MIPS_SIM == _ABI64
#define PROF_STACK	TSTAMP_EV_PROF_STACK64
#else
#define PROF_STACK	TSTAMP_EV_PROF_STACK32
#endif

	    case PRF_RANGE_STACK: {
		/* only do the backtrace if RTMON_PROFILE is on ... */
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_PROFILE)) {
			union {
				__psunsigned_t	stack[PRF_MAXSTACK];
				uint64_t rtarg[howmany(PRF_MAXSTACK, NPCRTARG)];
			} tracebuf;
			unsigned int si;

			if (USERMODE(sr)) {
				tracebuf.stack[0] = (__psunsigned_t)user_code;
				si = 1;
			} else if (domain == PRF_DOMAIN_IPL && BASEPRI(sr)) {
				tracebuf.stack[0] = (__psunsigned_t)low_ipl;
				si = 1;
			} else
				si = prfstacktrace(pc, sp, ra,
						   tracebuf.stack, PRF_MAXSTACK);
			if (si > 0) {
				/*
				 * Mark the first and last stack PCs and
				 * emit the stack trace in as many rtmon
				 * events as necessary to convey the stack
				 * backtrace.
				 */
				int arg, nargs;
				tracebuf.stack[0]    |= PRF_STACKSTART;
				tracebuf.stack[si-1] |= PRF_STACKEND;
				for (nargs = howmany(si, NPCRTARG), arg = 0;
				     arg < nargs; arg += 4)
					LOG_TSTAMP_EVENT(RTMON_PROFILE,
							 PROF_STACK,
							 tracebuf.rtarg[arg+0],
							 tracebuf.rtarg[arg+1],
							 tracebuf.rtarg[arg+2],
							 tracebuf.rtarg[arg+3]);
			}
		}
		break;
	    }

#undef NPCRTARG
#undef PROF_STACK

	    case PRF_RANGE_NONE:
		break;

	    default:
		cmn_err(CE_PANIC, "prfintr: bogus current profiling range %d\n",
			prfstat & PRF_RANGE_MASK);
		/*NOTREACHED*/
	}
	if (domain == PRF_DOMAIN_SWTCH)
		private.p_prfswtchcnt = PRF_SWTCH_SAMPLERATE;
}


/*
 * unsigned int
 * prfstacktrace(__psunsigned_t pc, __psunsigned_t sp, __psunsigned_t ra,
 *		 __psunsigned_t *stack, unsigned int nsp)
 *
 * Backtrace the stack starting from the exception point described by pc, sp,
 * ra, and sr and store the resulting list of callstack addresses into the
 * buffer stack (no more than nsp); return the number of callstack addresses
 * decoded.
 *
 * Caveats:
 *  1. Doesn't backtrace through exception frames.
 */

/*
 * Instruction definitions used to find stack frame creation and return
 * address storage instruction sequences.  For each stack frame backtrace we
 * search forward from the function start looking for instruction sequences
 * that decrement the stack pointer to create the new stack frame.  Following
 * that, we continue searching forward looking for a store of the return
 * address into the new stack frame.  For both searches, the current PC within
 * the function serves as a barrier beyond which we won't search.
 *
 * There are two general forms of stack frame creation sequences:
 *
 *     addiu sp, sp, #
 * and
 *     ori   at, #
 *     subu  sp, sp, at
 */
#if _MIPS_SIM == _ABI64

#define ADDIU_SP		0x67bd0000	/* daddiu sp, sp, framesize */
#define ADDU_SP_AT		0x03a1e82d	/* daddu sp, sp, at */
#define SUBU_SP_AT		0x03a1e82f	/* dsubu sp, sp, at */
#define RA_BIAS			0		/* RA = raoffset+RA_BIAS(sp) */

#else /* _ABIN32 */

/*
 * The compiler uses SD instructions to store the RA into the stack.  As a
 * result, the offset contained in the SD instruction points at the upper,
 * zero word of the 32-bit address.  Because of this we need to introduce
 * the concept of an RA_BIAS which biases the offset to result in a pointer
 * to the lower 32-bits which contain the actual return address.
 */
#define ADDIU_SP		0x27bd0000	/* addiu sp, sp, framesize */
#define ADDU_SP_AT		0x03a1e821	/* addu sp, sp, at */
#define SUBU_SP_AT		0x03a1e823	/* subu sp, sp, at */
#define RA_BIAS			4		/* RA = raoffset+RA_BIAS(sp) */

#endif /* _ABIN32 */

#define SD_RA_SP		0xffbf0000	/* sd ra, raoffset(sp) */
#define ORI_AT_ZERO		0x34010000	/* ori at, zero, framesize */
#define I_INST_OPCODE(inst)	((inst)&0xffff0000)
#define I_INST_VALUE(inst)	((int16_t)((inst)&0x0000ffff))

unsigned int
prfstacktrace(__psunsigned_t pc, __psunsigned_t sp, __psunsigned_t ra,
	      __psunsigned_t *stack, unsigned int nsp)
{
	unsigned int si;

	if (!prfsym) {
		/* if no symbol table the best we can do is return the pc */
		stack[0] = pc;
		return 1;
	}

	/*
	 * If we end up using the ra within the backtrace we really want the
	 * callsite, so we back it up by two instructions here.  This is only
	 * a single register extra instruction if we don't end up using it, so
	 * it's worth the simplification of not having to do this multiple
	 * times below.
	 */
	 ra -= 2 * sizeof(inst_t);

	/*
	 * Iteratively translate (pc, sp) to (pc', sp') by backtracing the
	 * stack.  This involves determining the stack frame size and the
	 * offset of the return address within the stack frame.  Note that
	 * when we pick up an RA for the next PC, we always use RA - 2
	 * instructions in order to get the callsite instead of the return
	 * address.
	 */
	for (si = 0;;) {
		int fidx, framesize, raoffset;
		inst_t *faddr, inst;
		__psunsigned_t addr;

	    newframe:
		/*
		 * Find the function that pc belongs to.  If it isn't a legal
		 * instruction address or we can't find it in the function
		 * table, ignore it and end the backtrace.  (Illegal
		 * instruction addresses can happen for a variety of reasons
		 * but the most common is when a function pops the stack frame
		 * before jumping to the return address and we happen to catch
		 * it there.  There's really not much we can do about this in
		 * terms of recovering the backtrace so we just give up.  We
		 * catch patently bogus addresses here, and non-text addresses
		 * at the top of the loop.)
		 */
		if (!IS_MAPPED_KERN_SPACE(pc) && IS_KSEG2(pc)) {
			stack[si++] = (__psunsigned_t)loadable_driver;
			return si;
		}
		if (IS_KSEG1(pc))
			pc = K1_TO_K0(pc);
		if ((pc & 0x3) || (fidx = findfunc(pc, 1)) == -1) {
			/*
			 * We got a bogus pc.  If we're on the first backtrace
			 * and we fail, it might be because we saw a store of
			 * the ra into the stack frame but it was on a
			 * different code path than the one actually taken.
			 * We can recover from this by trying to use the ra.
			 * Obviously we only want to try this once ...
			 */
			if (si > 1 || pc == ra || pc == K1_TO_K0(ra))
				return si;
			pc = ra;
			goto newframe;
		}

		/*
		 * Store the current pc into the stack backtrace being built
		 * and exit if we've reached our backtrace limit.
		 */
		stack[si++] = pc;
		if (si >= nsp)
			return si;

		/*
		 * Search for the stack frame decrement starting at the
		 * function start address and going no further than the pc.
		 * There are two possible patterns for the decrement:
		 * ``addiu sp, -N'' and ``li at, -N; addu sp, at.''  If we
		 * don't find the frame decrement we're dealing with an
		 * exception and we'll just give up for now.
		 */
		faddr = (inst_t *)(prfsym[fidx]);
		for (;;) {
			if (faddr >= (inst_t *)pc)
				if (si == 1) {
					/*
					 * Interrupted a LEAF or before stack
					 * frame created: use exception frame
					 * return address for next PC and loop.
					 */
#if defined(IDLESTACK_FASTINT)
					/*
					 * If we're interrupted while
					 * private.p_idleflag is non-zero,
					 * the interrupt code saves as little
					 * state as it can -- which includes
					 * skipping the save of the RA into the
					 * exception frame.  This affects our
					 * ability to backtrace out of any
					 * interrupted leaf routine ... so we
					 * attribute all such calls to idle().
					 */
					if (private.p_intr_resumeidle)
						pc = (__psunsigned_t)idle;
					else
#endif /* IDLESTACK_FASTINT */
					pc = ra;
					goto newframe;
				} else
					return si;
			inst = *faddr++;
			if (I_INST_OPCODE(inst) == ADDIU_SP) {
				framesize = -I_INST_VALUE(inst);
				break;
			}
			if (inst == SUBU_SP_AT) {
				inst = faddr[-2];
				if (I_INST_OPCODE(inst) != ORI_AT_ZERO)
					return si;
				framesize = I_INST_VALUE(inst);
				break;
			} 
		}
		if (framesize < 0)
			return si;

		/*
		 * Search for the store of the return address in the stack
		 * frame; searching no further than pc.  This takes the form
		 * of ``st ra, N(sp).''  Again, if we don't find it we're
		 * dealing with an exception frame which we'll simply stop
		 * the backtrace.
		 */
		for (;;) {
			if (faddr >= (inst_t *)pc)
				if (si == 1) {
					/*
					 * Interrupted a LEAF or before return
					 * address was stored in frame: use
					 * exception frame return address for
					 * next PC, pop the stack frame, and
					 * loop.
					 */
					pc = ra;
					sp += framesize;
					goto newframe;
				} else
					return si;
			inst = *faddr++;
			if (I_INST_OPCODE(inst) == SD_RA_SP) {
				raoffset = I_INST_VALUE(inst) + RA_BIAS;
				break;
			}
#if _MIPS_SIM == _ABIN32
			/*
			 * XXX Bug #460589: the compile generates sw/lw
			 * XXX sequences for RA when __return_address is
			 * XXX present in a function.  This code should be
			 * XXX removed as soon as the 7.2+ compilers are
			 * XXX being used to build the kernel.
			 */
#define SW_RA_SP		0xafbf0000	/* sw ra, raoffset(sp) */
			if (I_INST_OPCODE(inst) == SW_RA_SP) {
				raoffset = I_INST_VALUE(inst);
				break;
			}
#undef SW_RA_SP
#endif
		}
		if (raoffset < 0 || raoffset >= framesize)
			return si;

		/*
		 * Use frame size and frame return address offset to
		 * calulate the next PC/SP pair and loop.  Terminate if the
		 * computed address of the return address in the stack is
		 * above the top of stack.
		 */
		addr = sp + raoffset;
		if (addr >= (__psunsigned_t)KERNELSTACK)
			return si;
		pc = *(__psunsigned_t *)addr - 2*sizeof(inst_t);
		sp += framesize;
	}
	/*NOTREACHED*/
}
#undef ADDIU_SP
#undef SUBU_AP_AT
#undef SD_RA_SP
#undef RA_BIAS

#undef ORI_AT_ZERO
#undef I_INST_OPCODE
#undef I_INST_VALUE


/*
 *	/dev/par support
 *	================
 */

/*
 * Return 1 if tracer credentials are enough to trace tracee.  We are pretty
 * restrictive -- if the tracer isn't privileged we only let one trace ones
 * own programs.  This is effectively the same check as for accessing a
 * process via /proc But we don't view setgid as a security risk
 */
static int
cantrace(cred_t *tracer, cred_t *tracee)
{
	return (_CAP_CRABLE(tracer, CAP_DAC_READ_SEARCH) ||
		(tracer->cr_uid == tracee->cr_ruid &&
		 tracer->cr_uid == tracee->cr_suid));
}

struct scanargs {
	int	flags;			/* flags to turn on */
	int64_t	cookie;			/* trace security cookie */
	cred_t	*crp;			/* credentials of tracer/untracer */
	int	error;			/* error result of trace/untrace */
};

/*
 * Enable system call and/or scheduler tracing for the specified process.
 */
static int
fawltyscan(proc_t *p, void *arg, int mode)
{
	if (mode == 1) {
		struct scanargs *sa = (struct scanargs*)arg;
		int s = p_lock(p);
		/*
		 * Don't trace those processes we don't have permission for
		 * and those that are marked as untracable (usually rtmond,
		 * etc.).  Note that if a process is already being traced the
		 * result of this call will usurp the previous user's tracing.
		 * We do all of this under the process lock to avoid races and
		 * security holes ...
		 */
		if (!cantrace(sa->crp, p->p_cred) || (p->p_flag & SPARDIS))
			sa->error = EPERM;
		else {
			p->p_parcookie = sa->cookie;
			p->p_flag |= sa->flags;
			sa->error = 0;
		}
		p_unlock(p, s);
	}
	return 0;
}

/*
 * Start tracing one or more processes.
 */
static int
fawlty_start(pid_t pid, int64_t cookie, int flags, cred_t *crp)
{
	vproc_t *vpr;
	struct proc *pp;
	struct scanargs args;

	ASSERT((flags & ~(SPARSYS|SPARSWTCH|SPARINH)) == 0);
	args.flags = flags;
	args.cookie = cookie;
	args.crp = crp;
	/*
	 * If we're privileged, mark traced processes so we see exec()'s of
	 * SUID images, etc.
	 */
	if (_CAP_ABLE(CAP_DAC_READ_SEARCH))
		args.flags |= SPARPRIV;

	if (pid != -2) {
		/*
		 * Trace a particular process.
		 */
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &pp);
		fawltyscan(pp, &args, 1);
		VPROC_RELE(vpr);
		return args.error;
	} else {
		/*
		 * Trace all procs we have permission to trace.  Mark existing
		 * ones explicitly; new ones will get marked during fork.
		 * Note that for privileged callers this will yank away any
		 * other par tracing going on in the system from the current
		 * users.  Since this will almost always result in an EPERM
		 * for some processes we don't bother returning the error.
		 */
		args.flags |= SPARINH;	/* force inheritance */
		(void) procscan(fawltyscan, &args);
		return 0;
	}
	/*NOTREACHED*/
}

/*
 * Disable par-related tracing for a process.
 */
static int
fawltyunscan(proc_t *p, void *arg, int mode)
{
	if (mode == 1) {
		struct scanargs *sa = (struct scanargs*)arg;
		int s = p_lock(p);
		/*
		 * Don't turn off tracing for those processes we don't have
		 * permission for or weren't respoinsible for tracing in the
		 * first place.
		 */
		if (!cantrace(sa->crp, p->p_cred) || sa->cookie != p->p_parcookie)
			sa->error = EPERM;
		else {
			p->p_parcookie = 0;
			p->p_flag &= ~(SPARSYS|SPARSWTCH|SPARINH|SPARPRIV);
			sa->error = 0;
		}
		p_unlock(p, s);
	}
	return 0;
}

/*
 * Stop tracing one or more processes.
 */
static int
fawlty_stop(pid_t pid, __int64_t cookie, cred_t *crp)
{
	vproc_t *vpr;
	struct proc *pp;
	struct scanargs args;

	args.cookie = cookie;
	args.crp = crp;

	if (pid != -2) {
		/*
		 * Turn off tracing for a particular process.
		 */
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &pp);
		fawltyunscan(pp, &args, 1);
		VPROC_RELE(vpr);
		return args.error;
	} else {
		/*
		 * Turn off tracing for all processes on system we have
		 * permissions for.  Note that for privileged callers this
		 * will yank away any other par tracing going on in the system
		 * from the current users.  Since this will almost always
		 * result in an EPERM for some processes we don't bother
		 * returning the error.
		 */
		(void) procscan(fawltyunscan, &args);
		return 0;
	}
	/*NOTREACHED*/
}

/*
 * Control whether or not a process can be marked for system
 * call tracing.  This is used by event collection programs
 * to avoid having their work included in traces.
 */
static int
fawlty_control(pid_t pid, int allow)
{
	/* only root can do this */
	if (!_CAP_ABLE(CAP_SYSINFO_MGT))
		return EPERM;
	if (pid != 0) {			/* specific process */
		struct proc *pp;
		vproc_t *vpr = VPROC_LOOKUP(pid);;
		if (vpr == NULL)
			return ESRCH;
		VPROC_GET_PROC(vpr, &pp);
		if (allow)
		    p_flagclr(pp, SPARDIS);
		else
		    p_flagset(pp, SPARDIS);
		VPROC_RELE(vpr);
	} else {			/* current process */
		if (allow)
		    p_flagclr(curprocp, SPARDIS);
		else
		    p_flagset(curprocp, SPARDIS);
	}
	return 0;
}

#if (_MIPS_SIM != _ABI64)
/*
 * Because 64 bit arguments from n32 processes are packed into
 * a 32 bit sysarg array in o32 and n32 kernels, we need to recalculate
 * how many params there are for syscalls that have SY_64BIT_ARG set
 * in their sysent.sy_flags field.
 */
extern int syscall_has_64bit_arg(int, sysarg_t);

static int
recalc_paramno(int sysnum, sysarg_t cmd, int paramno)
{
	int bitmap = syscall_has_64bit_arg(sysnum, cmd);
	if (bitmap != 0) {
		int i;
		/*
		 * 64 bit args that land on odd params need two extra
		 * param slots because of padding.
		 */
		for (i=0; i < FAWLTY_MAX_PARAMS; i++) {
			if (bitmap & 1)
				paramno += 1+(i&1);
			bitmap >>= 1;
		}
	}
	return (paramno > FAWLTY_MAX_PARAMS ? FAWLTY_MAX_PARAMS : paramno);
}
#endif

/*
 * Generate trace record for a system call
 * A system call record consists of:
 *	tstamp_event_syscall_data_t record (always)
 *	args (as many as there are)
 *	(optional) indirect data - these are bytelen<>value sets.
 *		length specifiers are 2 bytes and not aligned
 */
void
fawlty_sys(short callno,
	struct sysent *callp,
	sysarg_t *argp)
{
	int paramno = callp->sy_narg;
#if (_MIPS_SIM != _ABI64)
	if ((ABI_IS_IRIX5_N32(get_current_abi())) &&
	    (callp->sy_flags & SY_64BIT_ARG))
		paramno = recalc_paramno(callno, argp[0], paramno);
#endif
	sysrec(TSTAMP_EV_SYSCALL_BEGIN, callno, callp, argp, argp, paramno);
}

/*
 * Generate trace record for a system call return
 */
void
fawlty_sysend(short callno,
	struct sysent *callp,
	sysarg_t *argp,
	__int64_t retvalue,
	__int64_t ret2, 
	int error)
{
	sysarg_t uargp[5];

	uargp[0] = retvalue;
	uargp[1] = error;
	uargp[2] = ret2;
#if (_MIPS_SIM != _ABI64)
	/*
	 * o32 and n32 kernels have 32 bit sysarg_t's but 64
	 * bit rvals; so we need a few more elements to pass
	 * everything up.
	 */
	uargp[3] = retvalue >> 32;
	uargp[4] = ret2 >> 32;
	sysrec(TSTAMP_EV_SYSCALL_END, callno, callp, argp, uargp, 5);
#else
	sysrec(TSTAMP_EV_SYSCALL_END, callno, callp, argp, uargp, 3);
#endif
}

static void
sysrec(event_t evt,
	short callno,
	struct sysent *callp,
	sysarg_t *argp,		/* real user args */
	sysarg_t *pargp,	/* args to pass to par */
	int paramno)		/* # of args to pass to par */
{
	union {
		usysarg_t buf[200/sizeof(usysarg_t)];
		__int64_t qual[TSTAMP_NUM_QUALS];
	} u;
	caddr_t abuf, bp;
	tstamp_event_syscall_data_t* ev;
	int i;
	int allocsize, recsize;
	sysargdesc_t dp, *dpp;
	ushort asizes[FAWLTY_MAX_PARAMS];

	/* compute size required - hopefully it will fit in our stack buffer */
	recsize = offsetof(tstamp_event_syscall_data_t, params[0])
		+ paramno*sizeof(ev->params[0]);
	bzero(asizes, sizeof(asizes));

	/* add indirect args */
	for (dpp = callp->sy_argdesc; dpp && (dp = *dpp); dpp++) {
		int size = getargsize(dp, argp, evt == TSTAMP_EV_SYSCALL_BEGIN,
				get_current_abi(), pargp[0]);
		if (size) {
			ASSERT(SY_GETARG(dp) < FAWLTY_MAX_PARAMS);
			ASSERT(SY_GETARG(dp) >= 0);
			asizes[SY_GETARG(dp)] = size;
			recsize += size + 2 + sizeof(sysargdesc_t);/* +2 for length */
		}
	}
	/* lets be a bit paranoid ... */
	if (recsize > FAWLTMAX/2) {	/* too big; probably a bogus param */
		recsize = offsetof(tstamp_event_syscall_data_t, params[0])
			+ paramno*sizeof(ev->params[0]);
		bzero(asizes, sizeof(asizes));
	}
	if (recsize > sizeof(u.buf)) {
		abuf = kmem_alloc(recsize, KM_SLEEP);
		allocsize = recsize;
		ev = (tstamp_event_syscall_data_t*) abuf;
	} else {
		ev = (tstamp_event_syscall_data_t*) u.buf;
		abuf = NULL;
	}
	ev->k_id = curthreadp->k_id;
	ev->pid = current_pid();
	ev->cookie = curprocp->p_parcookie;
	ev->abi = get_current_abi();
	ev->pad1 = 0;				/*XXX*/
	ev->callno = callno;
	ev->numparams = paramno;

	for (i = 0; i < paramno; i++)
		ev->params[i] = pargp[i];

	ev->numiparams = 0;
	ev->pad2 = 0;				/*XXX*/
	bp = (caddr_t) &ev->params[paramno];
	/* now read in any arguments that we want to dereference */
	for (dpp = callp->sy_argdesc; dpp && (dp = *dpp); dpp++) {
		caddr_t ap;
		int sz, argn;

		/* ignore descriptors that don't apply */
		if (evt == TSTAMP_EV_SYSCALL_BEGIN) {
			if (!isinputarg(dp))
				continue;
		} else
			if (isinputarg(dp))
				continue;
		argn = SY_GETARG(dp);
		ap = (caddr_t)(argp[argn]);
		sz = asizes[argn];
		if (sz == 0) {
			/*
			 * for some reason we couldn't get a size - pretend
			 * the descriptor doesn't exist
			 */
			continue;
		}

		/*
		 * we try to be careful about missing args (some system calls
		 * permit NULL pointers).
		 */
		if (ap == NULL) {
			/* decrease recsize */
			recsize -= (sz + 2 + sizeof(sysargdesc_t));
		} else {
			if (copyin(ap, bp+2+sizeof(sysargdesc_t), sz)) {
				/* if get any errors - same as above */
				recsize -= (sz + 2 + sizeof(sysargdesc_t));
			} else {
				/* set argdesc */
				*bp++ = (dp >> 8) & 0xff;
				*bp++ = dp & 0xff;

				/* set length */
				*bp++ = (sz >> 8) & 0xff;
				*bp++ = sz & 0xff;
				bp += sz;
				ev->numiparams++;
			}
		}
	}

#ifdef DEBUG
	if ((bp - (char*) ev) != recsize) {
		cmn_err(CE_NOTE,
		    "bp 0x%x crecp 0x%x recsize 0x%x asizes 0x%x\n",
		    bp, ev, recsize, asizes);
		debug("");
	} else
		ASSERT((bp - (char*) ev) == recsize);
#endif
	/*
	 * NB: we test if event collection is active again
	 * here (we also tested previously in the syscall
	 * handler before doing the work to pack up the event)
	 * to minimize a race between recording an event and
	 * tearing down the tstamp event collection state.
	 */
	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_SYSCALL)) {
		/*
		 * We could put this check down in log_tstamp_vevent
		 * but then all callers would be required to pass in
		 * 64-bit-aligned data (which probably is true anyway).
		 */
		if (recsize > sizeof (u.qual))
			log_tstamp_vevent(evt, ev, recsize);
		else
			/* NB: this assumes proper alignment of ev */
			log_tstamp_event(evt,
			    ((__int64_t*) ev)[0], ((__int64_t*) ev)[1],
			    ((__int64_t*) ev)[2], ((__int64_t*) ev)[3]);
	}
	if (abuf)
		kmem_free(abuf, allocsize);
}

/* 
 * special decode tables - these are plugged into sysent and the
 * sptable below
 */
/*ARGSUSED*/
static int
select_decode(sysarg_t *argp, int input, int abi)
{
	/*
	 * first arg says max FD --> # of bits
	 * However we need to always give a multiple of fd_mask size
	 * since we are bigendian
	 */
	return (argp[0] <= 0 ?
	    0 : howmany(argp[0]-1, NFDBITS) * sizeof(fd_mask));
}

/*ARGSUSED*/
static int
fcntl_decode(sysarg_t *argp, int input, int abi)
{
	int size = 0;

	if (input) {
		switch (argp[1]) {
		case F_SETLK:
		case F_SETBSDLK:
		case F_SETLKW:
		case F_SETBSDLKW:
		case F_RSETLK:
		case F_RSETLKW:
		case F_ALLOCSP:
		case F_FREESP:
		case F_RESVSP:
		case F_UNRESVSP:
#if _MIPS_SIM == _ABI64
			if (ABI_IS_IRIX5(abi))
				size = sizeof(struct irix5_flock);
			else if (ABI_IS_IRIX5_N32(abi))
				size = sizeof(struct irix5_n32_flock);
			else
				size = sizeof(struct flock);
#else
			size = sizeof(struct irix5_flock);
#endif

			break;
		case F_SETLK64:
		case F_SETLKW64:
		case F_ALLOCSP64:
		case F_FREESP64:
		case F_RESVSP64:
		case F_UNRESVSP64:
#if _MIPS_SIM == _ABI64
			if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi))
				size = sizeof(struct irix5_n32_flock);
			else
#endif
				size = sizeof(struct flock64);
			break;
		case F_FSSETXATTR:
			size = sizeof(struct fsxattr);
			break;
		case F_GETBMAP:
			size = sizeof(struct getbmap);
			break;
		case F_FSSETDM:
			size = sizeof(struct fsdmidata);
			break;
		}
	} else {
		switch (argp[1]) {
		case F_GETLK:
		case F_RGETLK:
#if _MIPS_SIM == _ABI64
			if (ABI_IS_IRIX5(abi))
				size = sizeof(struct irix5_flock);
			else if (ABI_IS_IRIX5_N32(abi))
				size = sizeof(struct irix5_n32_flock);
			else
				size = sizeof(struct flock);
#else
			size = sizeof(struct irix5_flock);
#endif
			break;
		case F_GETLK64:
#if _MIPS_SIM == _ABI64
			if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi))
				size = sizeof(struct irix5_n32_flock);
			else
#endif
				size = sizeof(struct flock64);
			break;			
		case F_DIOINFO:
			size = sizeof(struct dioattr);
			break;
		case F_FSGETXATTR:
			size = sizeof(struct fsxattr);
			break;
		case F_GETBMAP:
			size = sizeof(struct getbmap);
			break;
		}
	}
	return size;
}

/* ARGSUSED */
static int
poll_decode(sysarg_t *argp, int input, int abi)
{
	return (argp[0] <= 0 ? 0 : argp[0] * 8 /* sizeof(struct pollfd) */);
}

/* ARGSUSED */
static int
rwv_decode(sysarg_t *argp, int input, int abi)
{
	if (argp[3] <= 0)
		return 0;

#if _MIPS_SIM == _ABI64
	if (ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi))
		return argp[3] * sizeof(struct irix5_iovec);
	else
#endif
		return argp[3] * sizeof(struct iovec);
}

static int
ioctl_decode(sysarg_t *argp, int input, int abi)
{
	int size = 0;
	sysarg_t cmd = argp[1];
	/*
	 * Command param specifies if 3rd arg is in, out,
	 * or in+out, and what the size is.  If this is
	 * an ioctl that doesn't have the command encoding
	 * then we just bail unless it's particularly
	 * interesting (termio stuff).
	 */
	(void) abi;
	switch (cmd & (IOC_OUT|IOC_IN)) {
	case IOC_OUT:
		if (!input)
			size = (cmd>>16) & IOCPARM_MASK;
		break;
	case IOC_IN:
		if (input)
			size = (cmd>>16) & IOCPARM_MASK;
		break;
	case IOC_INOUT:
		size = (cmd>>16) & IOCPARM_MASK;
		break;
	case IOC_VOID:
		break;
	default:
		/*
		 * Special case handle some old-style
		 * cmds that occur frequently.
		 */
		switch (cmd) {
		case __NEW_TCGETA:
			if (!input)
				size = sizeof (struct __new_termio);
			break;
		case __NEW_TCSETA:
		case __NEW_TCSETAW:
		case __NEW_TCSETAF:
			if (input)
				size = sizeof (struct __new_termio);
			break;
		case __OLD_TCGETA:
			if (!input)
				size = sizeof (struct __old_termio);
			break;
		case __OLD_TCSETA:
		case __OLD_TCSETAW:
		case __OLD_TCSETAF:
			if (input)
				size = sizeof (struct __old_termio);
			break;
		case TCSBRK:
		case TCXONC:
		case TCFLSH:
			if (input)
				size = sizeof (int);
			break;
		}
	}
	return (size);
}

/*
 * get length in bytes of an indirect argument
 * If the type is SY_INSPECIAL/SY_OUTSPECIAL,
 * we switch out to a table that does whatever.
 * The sptable is indexed via the SPIDX in the descriptor
 */
static int (*sptable[])(sysarg_t *, int, int) = {
	select_decode,
	fcntl_decode,
	poll_decode,
	rwv_decode,
	ioctl_decode,
};
static int nsptables = sizeof(sptable)/sizeof(int (*)());

static int
getargsize(sysargdesc_t dp, sysarg_t *argp, int input, int abi, sysarg_t rv)
{
	int size = 0;

	if ((input ^ isinputarg(dp)) == 0) {
		switch (SY_GETTYPE(dp)) {
		case SY_OUTSTRING:
		case SY_STRING:
			/*
			 * if an arg is specified it implies a maximum size
			 */
			size = getstrlen(argp[SY_GETARG(dp)]);
			/* believe it or not - this works - don't be tempted to
			 * case the -1!
			 */
			if (SY_GETVARG(dp) != -1)
				size = MIN(size, argp[SY_GETVARG(dp)]);
			break;
		case SY_OUT:
		case SY_IN:
			size = SY_GETSIZE(dp);
			break;
		case SY_OUTV:
		case SY_INV:
			size = argp[SY_GETVARG(dp)];
			if (size > par_maxindbytes)
				size = par_maxindbytes;
			break;
		case SY_OUTVI:
			if (SY_GETVARG(dp) == -1) {
				/* use rval1 */
				size = rv;
			} else {
				int *szp;
				szp = (int *)argp[SY_GETVARG(dp)];
				if (copyin(szp, &size, sizeof(*szp)))
					size = 0;
			}
			if (size > par_maxindbytes)
				size = par_maxindbytes;
			break;
		case SY_OUTSPECIAL:
		case SY_INSPECIAL:
			if (SY_GETSPIDX(dp) < nsptables) {
				/* 
				 * NB: special decoders are responsible
				 *     for clamping size if appropriate.
				 */
				size = (*(sptable[SY_GETSPIDX(dp)]))
				    (argp, input, abi);
				if (size > par_maxindbytes)
					size = par_maxindbytes;
			}
			break;
		}
	}
	return (size < 0 ? 0 : size);
}

static int
isinputarg(sysargdesc_t dp)
{
	switch (SY_GETTYPE(dp)) {
	case SY_STRING:
	case SY_IN:
	case SY_INV:
	case SY_INSPECIAL:
		return 1;
	}
	return 0;
}

/*
 * Get length of a string parameter
 * XXX use useracc and strlen to avoid 80 hack
 */
static int
getstrlen(usysarg_t argp)
{
	caddr_t cp = (caddr_t)argp;

	if (cp != NULL) {
		char buf[80];
		size_t len;
		int error = copyinstr(cp, buf, 80, &len);
		return (error == ENAMETOOLONG ? 80 : error ? 0 : len);
	} else
		return 0;
}

/*
 * Called when a process fork is successful.  Generate an
 * event that identifies the new process and it's name.
 * This information is used to track process relationships
 * and to avoid a race in finding the name of a process
 * (by the time a user-level process checks for a process's
 * name the process may have terminated and/or changed it
 * through an exec.
 */
void
fawlty_fork(int64_t pkid, int64_t ckid, const char* fname)
{
	int size;
	union {
	    tstamp_event_fork_data_t ev;
	    __uint64_t	qual[TSTAMP_NUM_QUALS];
	} u;

	/*
	 * NB: This is the only place (so far) where PIDs
	 *     are treated as 32-bit values; everywhere
	 *     else they are handled as (potential) 64-bit
	 *     values.
	 */
	size = strlen(fname);
	u.ev.pkid = (__int64_t) pkid;
	u.ev.ckid = (__int64_t) ckid;
	strncpy(u.ev.name, fname, sizeof u.ev.name);
	if (size > sizeof (u.ev.name))
		u.ev.name[sizeof (u.ev.name) -1] = '\0';
	log_tstamp_event(TSTAMP_EV_FORK,
	    u.qual[0], u.qual[1], u.qual[2], u.qual[3]);

}

/*
 * Called when an exec call is successful.  Generate an
 * event that gives the new name of the process.  We
 * should also pass a flag to indicate if tracing is
 * turned off as a result of exec'ing a setuid app that
 * changes the uid's but for now we don't.
 */
void
fawlty_exec(const char* fname)
{
	union {
	    tstamp_event_exec_data_t ev;
	    __uint64_t	qual[TSTAMP_NUM_QUALS];
	} u;

	u.ev.k_id = curthreadp->k_id;
	u.ev.pid = current_pid();
	strncpy(u.ev.name, fname, sizeof (u.ev.name));
	if (strlen(fname) > sizeof (u.ev.name))
		u.ev.name[sizeof (u.ev.name) -1] = '\0';
	log_tstamp_event(TSTAMP_EV_EXEC,
	    u.qual[0], u.qual[1], u.qual[2], u.qual[3]);

}

/*
 * Emit an EVENT_TASK_STATECHANGE event.  We include
 * wait channel name in the event if available.
 */
void
fawlty_resched(kthread_t* kt, int flags)
{
	__psunsigned_t wchan = (__psunsigned_t) kt->k_wchan;
	__psunsigned_t wchan2 = (__psunsigned_t) kt->k_w2chan;
	const char* cp;

	/*
	 * If thread is being rescheduled 'cuz it's waiting on
	 * something we can give a "name" for, then emit a jumbo
	 * event with the wait channel name string.  Otherwise,
	 * put out the normal event record.  Decoders look at
	 * the event record size to identify whether or not this
	 * event has the name string in it.
	 */
	if ((flags & RESCHED_LOCK) &&
	    (cp = wchanname((void*) wchan, kt->k_flags))[0] != '\0') {
		tstamp_event_resched_data_t red;
		red.k_id = kt->k_id;
		red.stuff = RTMON_PACKPRI(kt, flags);
		red.wchan = wchan;
		red.wchan2 = wchan2;
		strncpy(red.wchanname, cp, sizeof (red.wchanname));
		/*
		 * NB: recheck if event gathering is active since
		 * we may have spent time gathering the wait channel
		 * name and been preempted.  As other places, this
		 * doesn't fix the race condition, just shrink the
		 * window in which it can occur (sigh).
		 */
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK|RTMON_TASKPROC))
			log_tstamp_vevent(EVENT_TASK_STATECHANGE,
			    &red, sizeof (red));
	} else {			/* no name to include */
		log_tstamp_event(EVENT_TASK_STATECHANGE,
				 kt->k_id, RTMON_PACKPRI(kt, flags), wchan, wchan2);
	}
}

/*
 * Record a disk event; called via the DISKSTATE macro.
 */
void
fawlty_disk(int event, struct buf *bp)
{
	ASSERT(bp != NULL);
	log_tstamp_event(event,
		curthreadp->k_id,
		bp->b_flags,
		(((uint64_t) bp->b_edev)<<32)|bp->b_bcount,
		bp->b_blkno
	);
}
