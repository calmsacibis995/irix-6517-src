/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1999, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "$Revision: 1.146 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/pda.h>
#include <sys/sysinfo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/atomic_ops.h>
#include <ksys/xthread.h>
#include <sys/schedctl.h>	/* For thread priority defs */
#include <sys/runq.h>		/* For setmustrun() prototype */
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evintr_struct.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/groupintr.h>
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/inst.h>
void do_splx_log(int, int);
#endif /* SPLDEBUG */
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
void spldebug_log_event(int);
#endif /* SPLDEBUG || SPLDEBUG_CPU_EVENTS */

#ifdef ULI_TSTAMP1
#include <ksys/uli.h>
#define ULI_GET_TS(which) if (cpuid() == TSTAMP_CPU) \
	uli_tstamps[which] = *(((volatile int*)EV_RTC)+1)
#endif

/*
 * REACT/Pro
 */
#include <sys/rtmon.h>
 
#if SABLE_RTL
#define	DEBUGGING_INTR_CODE 1	/* Faster for small number of TFP ints */
#endif
static void init_intr_tables(void);
static void evintr_thread_setup(int);
static void thd_evintr_init(int,int);
int ithreads_enabled=0;

/*
 * Given an everest level, translate to an interrupt handler to
 * be executed at a given spl level, and passed a given argument.
 *
 * TBD: We can permit a given everest level to map to different
 * handlers on different CPUs by placing this in the pda.  For now,
 * a single level maps to a single handler regardless of which cpu
 * fields the interrupt.
 */
evintr_t evintr [EVINTR_MAX_LEVELS];

/*
 * Given a sofware spl value, translate to the highest everest
 * level that interrupts at that spl level.
 */
unchar ev_spltocel[NSPL];
extern int nvmecc;

/* ARGSUSED */
void
evintr_stray (eframe_t *ep, void *arg)
{
#if defined(MULTIKERNEL)

	extern int evmk_kgclock_state;

	/* In MULTIKERNEL system, golden cell may have started the kgclock
	 * but current cell hasn't initialized it yet, so ignore the
	 * erroneous "stray interrupt".
	 */
	if ((evmk_cellid != evmk_golden_cellid)  && (evmk_kgclock_state==0) &&
	    ((long long)arg == EVINTR_LEVEL_EPC_PROFTIM))
		return;


	if ((uint64_t) arg != 118)
#endif /* MULTIKERNEL */

	cmn_err (CE_WARN,"Everest Stray Interrupt - level %d to cpu %d", 
					arg, cpuid());
}

void
evintr_init(void)
{
	int level;

	for (level = 0; level < EVINTR_MAX_LEVELS; level++) {
		evintr[level].evi_level		= level;
		evintr[level].evi_spl		= 0;
		evintr[level].evi_dest		= master_procid;
		evintr[level].evi_regaddr	= 0;
		evintr[level].evi_func		= evintr_stray;
		evintr[level].evi_arg		= (void *)(__psint_t)level;
		thd_evintr_init(level,1);
	}

	init_intr_tables();
	intrgroup_init();
}

/*
 * TBD:
 * Everest should really wait until *after* cpus/pdas are initialized
 * before it initializes I/O and tries to assign interrupts to cpus.
 * Then it could just use the standard cpu_allow_intr macro.  'Till
 * this is fixed, we'll just peek at the nointr_list.  [When fixing
 * this, evio_init should become the init_all_devices routine for
 * Everest.  Then, need to handle console initialization.]
 */
static int
_cpu_allows_intr(cpuid_t cpuid)
{
	extern cpuid_t nointr_list[];
	int nointr_scan = 0;
	int scan_cpuid;

	while ((scan_cpuid = nointr_list[nointr_scan++]) != CPU_NONE)
		if (scan_cpuid == cpuid)
			return(0);

	return(1);
}

/*
 * Choose a cpuid to which to send this level interrupt.  If
 * available, send the interrupt to the specified "preferred cpu".
 * Arbitrary heuristics can be encapsulated here.
 */
/* ARGSUSED */
static cpuid_t
spray_heuristic(int level)
{
	static cpuid_t last_sprayed = 1;
	cpuid_t cpu;

	for (cpu=last_sprayed+1; cpu < EV_MAX_CPUS; cpu++) {
		if (!_cpu_allows_intr(cpu))
			continue;
		if (cpu_enabled(cpu))
			goto out;
	}

	for (cpu=0; cpu <= last_sprayed; cpu++) {
		if (!_cpu_allows_intr(cpu))
			continue;
		if (cpu_enabled(cpu))
			goto out;
	}

	ASSERT(0);
	/* NOTREACHED */

out:
	last_sprayed = cpu;
	return(cpu);
}

#if DEBUG
int existing_level, new_level, existing_spl, new_spl;
#endif

struct	level0dbg {
	EvIntrFunc	func;
	evreg_t		*regaddr;
} evintr_level0 = { (EvIntrFunc)0, (evreg_t *)0};

int
evintr_connect (evreg_t *regaddr,
		int	level,
		int	spl,
		cpuid_t	dest,	 /* XXX allowboot() comes _after_ calls here */
		EvIntrFunc func, 
		void	*arg)
{
	int i;
	ulong intrdest;

#if 0
	/* Extreme gfx changes its interrupt handler during execution, so we
	 * can't do this assert */
	ASSERT ((evintr[level].evi_func == evintr_stray) ||
		(evintr[level].evi_func == func));
#endif

	/* If there is a request for attaching level 0, keep track of it
	 * This will come in handy while looking for stray interrupt problems
	 * on level 0
	 * Since this could be called very early, no printfs.
	 */
	
	if (level == 0){
		evintr_level0.func = func;
		evintr_level0.regaddr = regaddr;
	}
	if (dest == EVINTR_ANY) {
		dest = spray_heuristic(level);
		intrdest = EVINTR_CPUDEST(dest);
        } else if (dest == EVINTR_GLOBALDEST) {
                intrdest = EVINTR_GLOBALDEST;
        } else if (dest >= EV_MAX_CPUS) {
                intrdest = EVINTR_GROUPDEST(dest - EV_MAX_CPUS);
        } else {
		/* Pick an enabled processor (preferably dest) */
		ASSERT(dest >= 0);
		ASSERT(dest < EV_MAX_CPUS);

		if (cpu_enabled(dest) &&
		    (_cpu_allows_intr(dest)) ||
#ifdef DEBUG_INTR_TSTAMP
		    (level == EVINTR_LEVEL_EPC_DUARTS4) ||
#endif
		    (level <= EVINTR_LEVEL_VMEMAX)) {
                        intrdest = EVINTR_CPUDEST(dest);
		} else {
			dest = spray_heuristic(level);
			intrdest = EVINTR_CPUDEST(dest);
		}
	}

	/* program the h/w register */
	if (regaddr) {
		EV_SET_REG(regaddr, EVINTR_VECTOR(level, intrdest));
	}

	ASSERT((level > 0) && (level < EVINTR_MAX_LEVELS));
	evintr[level].evi_level = level;

	ASSERT((spl > 0) && (spl < NSPL));
	evintr[level].evi_spl = spl;


	/*
	 * Verify that spl levels specified are ordered
	 * according to everest interrupt levels.
	 */
	for (i=level-1; (i>=0) && 
		(evintr[i].evi_func == evintr_stray); i--)
		;

#if DEBUG
	existing_level = i;
	existing_spl = evintr[i].evi_spl;
	new_level = level;
	new_spl = spl;
#endif
	if (i>=0) {
		ASSERT(evintr[i].evi_spl <= spl);
	}



	for (i=level+1; (i<EVINTR_LEVEL_MAXHIDEV) && 
		(evintr[i].evi_func == evintr_stray); i++)
		;

	if (i<EVINTR_LEVEL_MAXHIDEV)
		ASSERT(evintr[i].evi_spl >= spl);

	if (level > ev_spltocel[spl])
		ev_spltocel[spl] = (unchar)level;

	evintr[level].evi_dest		= dest;
	evintr[level].evi_regaddr	= regaddr;
	evintr[level].evi_func		= func;
	evintr[level].evi_arg		= arg;
/* fix: re-initialize thread info, as nvmecc may have changed, which 
	would affect the macro */
	if (func != evintr_stray)	
		thd_evintr_init(level,0);

	/*
	 * If we're threaded handler, set up the thread here.
	 * If !ithreads_enabled, then it's early, and enable_ithreads()
	 * will come by and setup the thread for us.
	 */
#ifdef DEBUG
	if ((evintr[level].evi_flags & THD_ISTHREAD) && (func != evintr_stray)) {
#else
	if (evintr[level].evi_flags & THD_ISTHREAD) {
#endif
		atomicSetInt(&evintr[level].evi_flags, THD_REG);
		if (ithreads_enabled)
			evintr_thread_setup(level);
	}
	return level;
}			

void
evintr_disconnect()
{
	/* Should supply this routine */
}

static void evnointr(void);
static void ip19crazy(void);

#if SABLE
extern void ccuartintr();
extern void sdintr();
#else
extern void syscintr();
#endif
extern void timein();
extern void evdev_intr(eframe_t *, void *);
extern void early_counter_intr(eframe_t *, void *);
extern void wgtimo();
extern void evdebug(eframe_t *);
extern void tfp_gcache_intr(eframe_t *, void *);
extern void tlbintr(eframe_t *, void *);

/*
 * The "interrupt information array" specifies handling details 
 * for each possible interrupt.  The "setvector" call can change
 * information in this array.
 */
static struct {
	EvIntrFunc	ii_handler;	/* handler for given level */
	int		ii_splvalue;	/* spl value to splset to */
	int		ii_causemask;	/* mask to remove cause bit when done */
} intr_info[] = {
	{0,			0,	0},				/*  0 */
	{(EvIntrFunc)ip19crazy,		II_SPL7|II_CEL7, CAUSE_IPMASK},	/*  1 */
	{timein,		II_SPLHI|II_CELHI,	~(SRB_TIMOCLK|SRB_SWTIMO)},/*  2 */
	{(EvIntrFunc)ip19crazy,		II_SPL7|II_CEL7, CAUSE_IPMASK},	/*  3 */
	{evdev_intr,		II_SPLHI|II_CELHI,	~SRB_DEV},	/*  4 */
	{evdev_intr,		II_SPLHI|II_CELHI,	~SRB_DEV},	/*  5 */
#if SABLE
	{ccuartintr,		II_SPLHI|II_CELHI,	~SRB_UART},	/*  6 */
#else
	{syscintr,		II_SPLHI|II_CELHI,	~SRB_UART},	/*  6 */
#endif
	{early_counter_intr,	II_SPLHI|II_CELHI,	~SRB_SCHEDCLK},	/*  7 */
	{(EvIntrFunc)evdebug,	II_SPL7|II_CEL7,	~SRB_DEV},	/*  8 */
	{evdev_intr,		II_SPLHI|II_CELHI,	~SRB_DEV},	/*  9 */
	{evdev_intr,		II_SPLPROF|II_CELPROF,	~SRB_DEV},	/* 10 */
	{wgtimo,		II_SPL7|II_CEL7,	~SRB_WGTIMO},	/* 11 */
	{everest_error_handler,	II_SPL7|II_CEL7,	~SRB_ERR},	/* 12 */
#ifdef SABLE
	{sdintr,		II_SPL7|II_CEL7,	~SRB_DEV},	/* 13 */
#else
	{evdev_intr,		II_SPL7|II_CEL7,	~SRB_DEV},	/* 13 */
#endif
	{evdev_intr,		II_SPLHI|II_CELHI,	~SRB_DEV},	/* 14 */
#if TFP
	{tfp_gcache_intr,	II_SPL7|II_CEL7,	~(SRB_GPARITYE|SRB_GPARITYO)},	/* 15 */
#else
	{(EvIntrFunc)ip19crazy,		II_SPL7|II_CEL7, CAUSE_IPMASK},	/*  15 */
#endif
	{tlbintr,		II_SPLTLB|II_CELTLB,	~SRB_DEV},	/*  16 */
};
#define INTR_EVNOINTR		0
#define INTR_EVCRAZY		1
#define INTR_TIMEIN		2
#define INTR_NETINTR		3 /* no longer used */
#define INTR_LODEV_INTR		4
#define INTR_HIDEV_INTR		5
#define INTR_CCUART_INTR	6
#define INTR_COUNTER_INTR	7
#define	INTR_DEBUG		8
#define INTR_CPUACTION		9
#define INTR_PROFCLOCK	 	10
#define INTR_WGTIMO	 	11
#define INTR_EV_ERROR		12
#ifdef SABLE
#define INTR_SABLEDSK_INTR	13
#else
#define INTR_HIDUART		13
#endif
#define INTR_GROUPACTION        14
#define	INTR_TFP_GCACHE		15
#define	INTR_TLB_INTR		16

/*
 * Simple-minded decryption of the status register/HPIL register pair
 * into software interrupt level (which is used as an index into the
 * intr_info table.
 */
int
get_intr_level(int ev_level, int cause)
{
	if ((cause & SR_IMASK) == 0)
		return(INTR_EVNOINTR);

#if TFP
	if (cause & (SRB_GPARITYE|SRB_GPARITYO))
		return(INTR_TFP_GCACHE);
#endif /* TFP */

	if (cause & SRB_ERR)
		return(INTR_EV_ERROR);

#if SABLE
	if (cause & SRB_DEV)
		return(INTR_SABLEDSK_INTR);
#else
	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_EPC_HIDUART))
		return(INTR_HIDUART);
#endif

	if ((cause & SRB_DEV) && (ev_level >= EVINTR_LEVEL_ERR))
		return(INTR_EV_ERROR);

	if (cause & SRB_WGTIMO)
		return(INTR_WGTIMO);

	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_EPC_PROFTIM))
		return(INTR_PROFCLOCK);

	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_CPUACTION))
		return(INTR_CPUACTION);

	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_GROUPACTION))
		return(INTR_GROUPACTION);

	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_TLB))
		return(INTR_TLB_INTR);

	if ((cause & SRB_DEV) && (ev_level == EVINTR_LEVEL_DEBUG))
		return(INTR_DEBUG);

	if (cause & SRB_SCHEDCLK)
		return(INTR_COUNTER_INTR);

#if SABLE
	if (SR_IMASK & SRB_UART)
#endif
	if (cause & SRB_UART)
		return(INTR_CCUART_INTR);

	if ((cause & SRB_DEV) && (ev_level > EVINTR_LEVEL_MAXLODEV))
		return(INTR_HIDEV_INTR);

	if (cause & SRB_DEV)
		return(INTR_LODEV_INTR);

	if (cause & (SRB_TIMOCLK|SRB_SWTIMO))
		return(INTR_TIMEIN);

	return(INTR_EVCRAZY);
}

#if DEBUGGING_INTR_CODE
int
intr_level(int ev_level, int cause)
{
	return(get_intr_level(ev_level, cause));
}


static void
init_intr_tables()
{
}
#else
/*
 * Convert CAUSE/HPIL pair to a software interrupt level.
 */ 
#define INTR_TBL_LODEV		0
#define INTR_TBL_HIDEV		1
#define INTR_TBL_PROFTIM	2
#define INTR_TBL_CPUACTION	3
#define INTR_TBL_DEBUG		4
#define INTR_TBL_ERR		5
#define INTR_TBL_HIDUART	6
#define INTR_TBL_GROUPACTION    7
#define	INTR_TBL_TLB		8
#define INTR_NUM_TBLS 9

/*
 * Number of values for the R4000 cause register.
 */
#if R4000 || R10000
#define NUM_CAUSE 	256
#endif
#if TFP
#define NUM_CAUSE 	2048
#endif
#define SR_ISHFT	8

char intr_lvl_tbl[INTR_NUM_TBLS][NUM_CAUSE];

/*
 * Convert an HPIL value into a small integer that can be used as
 * an index into the intr_lvl_tbl (see above).
 */
char intr_which_tbl[EVINTR_MAX_LEVELS];

/*
 * Given a value for HPIL and CAUSE register, return an index
 * into the intr_info table.
 */
#define intr_level(ev_level,cause) \
	(intr_lvl_tbl[intr_which_tbl[(ev_level)]][((cause)&SR_IMASK)>>SR_ISHFT])

void
init_intr_tables(void)
{
	int cause;
	int i;

	for (cause=0; cause<NUM_CAUSE; cause++) {
		intr_lvl_tbl[INTR_TBL_LODEV][cause] = 
			get_intr_level(EVINTR_LEVEL_MAXLODEV, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_HIDEV][cause] = 
			get_intr_level(EVINTR_LEVEL_MAXHIDEV, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_PROFTIM][cause] = 
			get_intr_level(EVINTR_LEVEL_EPC_PROFTIM, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_CPUACTION][cause] = 
			get_intr_level(EVINTR_LEVEL_CPUACTION, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_ERR][cause] = 
			get_intr_level(EVINTR_LEVEL_ERR, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_DEBUG][cause] = 
			get_intr_level(EVINTR_LEVEL_DEBUG, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_HIDUART][cause] = 
			get_intr_level(EVINTR_LEVEL_EPC_HIDUART, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_GROUPACTION][cause] = 
			get_intr_level(EVINTR_LEVEL_GROUPACTION, cause<<SR_ISHFT);

		intr_lvl_tbl[INTR_TBL_TLB][cause] = 
			get_intr_level(EVINTR_LEVEL_TLB, cause<<SR_ISHFT);
	    }

	for (i=0; i<EVINTR_LEVEL_MAXLODEV; i++)
		intr_which_tbl[i] = INTR_TBL_LODEV;

	for (i=EVINTR_LEVEL_MAXLODEV+1; i<EVINTR_LEVEL_MAXHIDEV; i++)
		intr_which_tbl[i] = INTR_TBL_HIDEV;

	intr_which_tbl[EVINTR_LEVEL_EPC_PROFTIM] = INTR_TBL_PROFTIM;

	intr_which_tbl[EVINTR_LEVEL_CPUACTION] = INTR_TBL_CPUACTION;

	intr_which_tbl[EVINTR_LEVEL_GROUPACTION] = INTR_TBL_GROUPACTION;

	for (i=EVINTR_LEVEL_ERR; i<EVINTR_MAX_LEVELS; i++)
		intr_which_tbl[i] = INTR_TBL_ERR;

	intr_which_tbl[EVINTR_LEVEL_EPC_HIDUART] = INTR_TBL_HIDUART;

	intr_which_tbl[EVINTR_LEVEL_DEBUG] = INTR_TBL_DEBUG;
	intr_which_tbl[EVINTR_LEVEL_TLB] = INTR_TBL_TLB;

}
#endif

#if DEBUG
int missed_count;
void
missed_intr(void)
{
	missed_count++;
}

int extra_count;
void
extra_intr_call(void)
{
	extra_count++;
}
#endif

/*
 * Called from VEC_int, this routine loops through all pending
 * interrupts of sufficiently high level, and dispatches to the
 * appropriate interrupt handler.  
 *
 * On entrance, interrupts are disabled.
 */

/* ARGSUSED */
int
intr(
	eframe_t *ep,
	uint code,
	uint sr,
	register uint cause)
{
	int base_intrlevel;		/* sw level we're currently handling */
	int old_CEL = (int)ep->ef_cel;/* CEL register at time of interrupt */
	int intrlevel;			/* software interrupt level */
	int ev_level;			/* everest interrupt level */
	int check_kp = 0;		/* check for kernel preemption */
	int check_exit = 0;		/* check for delay tlbflush on exit */
	int splvalue;

#ifdef ULI_TSTAMP1
	ULI_GET_TS(TS_INTR);
#endif

#if R4000 || R10000
	/* avoid a hardware timing race in splset below. Normally when
	 * we first enter this routine, all SR_IMASK bits are clear
	 * (all intrs blocked) and the CEL is set to some value
	 * insufficiently high to block out this interrupt. In the
	 * call to splset below, we set some of the SR_IMASK bits
	 * (usually enabling device interrupts), and then raise the
	 * CEL to a value which will mask further occurrences of this
	 * interrupt. The problem is that it takes the CEL longer to
	 * take effect than the SR, so there is a very brief window
	 * when this same interrupt is re-enabled, and we take it
	 * again. I.e. *every* device interrupt is taken twice, and
	 * the second one is discarded. By setting the CEL to the max
	 * here, we avoid this window since the CEL is being lowered
	 * in splset rather than raised.  The other alternative would
	 * be to wait for the write to the CEL to flush out in splset,
	 * but that would be far more costly than doing a buffered
	 * write here.
	 * Note: it takes significantly fewer instructions to do this
	 * here rather than in locore.s
	 */
	EV_SET_LOCAL(EV_CEL, EVINTR_LEVEL_MAX);
	private.p_CEL_shadow = EVINTR_LEVEL_MAX;
	private.p_CEL_hw = EVINTR_LEVEL_MAX;
#endif

	/*
	 * if was isolated then if we're from user mode must do delay action
	 * to maintain sanity
	 */
	CHECK_DELAY_TLBFLUSH_INTR(ENTRANCE, check_exit);

	ev_level = EV_GET_LOCAL_HPIL;
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
	spldebug_log_event(ep->ef_epc);
	spldebug_log_event(ev_level);
#endif 

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTRENTRY, NULL, NULL,
			 NULL, NULL);
	
#ifdef SABLE
	{
	static int first=0;

	if (first) {
		cmn_err(CE_NOTE, "intr: cause 0x%x ev_level 0x%x\n",
			 cause, ev_level);
		first=0;
	      }
	}
	/* For now, SABLE is always returning "3" for EV_HPIL */
	ev_level = 0;
#endif /* SABLE */
	/*
	 * It's possible that we got into here after "cause" was saved,
	 * but before a new device interrupt.  Furthermore, it's possible
	 * that we received a new interrupt while waiting for a store to
	 * the CEL register to take effect.  In such a case, the cause 
	 * register bit is cleared after we've already entered the interrupt
	 * handler.
	 *
	 * Bottom line: If HPIL is non-zero, there actually IS a DEV 
	 * interrupt pending, so modify our copy of the cause register 
	 * appropriately.
	 */
	if (ev_level >= old_CEL)
		cause |= SRB_DEV;


again:

	ASSERT((getsr() & SR_IMASK) == 0); /* everything now blocked */

	if ((cause & sr & SR_IMASK) == 0) {
		/* TFP still gets these - needs to be fixed like IP19 */
		cmn_err(CE_WARN|CE_CPUID,"intr: EVNOINTR cause 0x%x sr 0x%x\n",
			cause, sr);
		evnointr();
		goto exit_intr;
	}

	intrlevel = intr_level(ev_level, cause);

	/*
 	* Remember the base interrupt level that a processor is currently 
	* handling. This is so the 128 Everest levels can be divided into 
	* ranges in such a way that when an interrupt is taken at some 
	* hardware level, all interrupts that share the same software level 
	* are handled as well.  However, interrupts that use a higher software 
	* level can still occur.  Lower level interrupts are left for the
	* original instantiation of intr to handle.
 	*/
	base_intrlevel = intrlevel;

	while (intrlevel == base_intrlevel) {

#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)
		spldebug_log_event(base_intrlevel);
#endif
		ASSERT(intrlevel != INTR_EVNOINTR);
		if (ev_level == EVINTR_LEVEL_EPC_ENET) {
			LOG_TSTAMP_EVENT(RTMON_INTR,
					 TSTAMP_EV_NETINTR, NULL, NULL,
					 NULL, NULL);
			LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_NETINTR, NULL, NULL, NULL);
		}
#ifdef ULI_TSTAMP1
		/* When taking exception timestamps, guard against
		 * corrupt data from nested interrupts by preventing
		 * nested interrupts in the first place. We'll leave all
		 * interrupts masked here
		 */
		if (cpuid() != TSTAMP_CPU)
#endif
		splset(splvalue = intr_info[intrlevel].ii_splvalue);
		(*intr_info[intrlevel].ii_handler)(ep, NULL);
		if (splvalue == (II_SPLHI|II_CELHI))
			check_kp = 1;
		cause &= intr_info[intrlevel].ii_causemask;
		SYSINFO.intr_svcd++;

		ev_level = EV_GET_LOCAL_HPIL;
#ifdef SABLE
	ev_level = 0;
#endif /* SABLE */

		if (ev_level >= old_CEL)
			cause |= SRB_DEV;
		intrlevel = intr_level(ev_level, cause);
#if DEBUG
		if (intrlevel > base_intrlevel)
			missed_intr();
#endif
	}

	/*
	 * We're about to return to whatever we were doing, but before
	 * we do, let's have one last look at the cause register to see
	 * if we're about to get interrupted.
	 */
	cause = getcause();
	if (ev_level >= old_CEL)
		cause |= SRB_DEV;
	if (cause & sr & SR_IMASK) {
		/*
		 * We fetched "cause" the first time in order to see if
		 * it was even worth doing the spl7.
		 */
		spl7();
		cause = getcause();
		ep->ef_cause = cause;
		ev_level = EV_GET_LOCAL_HPIL;
#ifdef SABLE
	ev_level = 0;
#endif /* SABLE */
		if (ev_level >= old_CEL)
			cause |= SRB_DEV;
		if (cause & sr & SR_IMASK)
			goto again;
	}
exit_intr:

	CHECK_DELAY_TLBFLUSH_INTR(EXIT, check_exit);

	/*
	 * Frame Scheduler
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_INTRENTRY, NULL,
			 NULL, NULL);

	return check_kp;
}

/*
 * setvector - set local interrupt handler for given level
 */
static void
setvector(
	int level,
	EvIntrFunc func)
{
	intr_info[level].ii_handler = func;
}

/*
 * setkgvector - set the interrupt handler for the profiling clock.
 */
void
setkgvector(void (*func)()) 
{ 
	setvector(INTR_PROFCLOCK, (EvIntrFunc)func); 
}

/*
 * setrtvector - set the interrupt handler for the scheduling clock.
 */
void
setrtvector(EvIntrFunc func)
{
	setvector(INTR_COUNTER_INTR, func);
}

int bad_intr_count;
/*
 * Handle very small window in spl routines where an interrupt can occur,
 * but shouldn't:  an mtc0 to the SR requires a "few" cycles (<=4) to take
 * effect, and an event which arrives within that window will generate an
 * interrupt based upon the old SR, not the new SR.  However, the exception
 * handler reads the new SR, which may show   (SR & CAUSE) == 0   and make
 * intr() wonder why an interrupt was generated.  Just ignore the interrupt
 * and presume that we'll handle it when the SR really wants to enable it.
 */
void
evnointr(void)
{
	bad_intr_count++;
}

/*
 * Handle impossible combinations of cause register and HPIL.
 * Example: HPIL shows a profiling clock interrupt has occured,
 * but the cause register does not have the everest device interrupt
 * bit set.
 */
void
ip19crazy(void)
{
	cmn_err(CE_WARN, "crazy cause register/HPIL pair\n");
}

/*
 * Handle interrupts on 128 everest "device" interrupt levels.
 * The profiling timer interrupt is one of the 128 levels, but it's
 * handled separately.
 * 
 * The 128 levels may be divided into sub-groups.  A single invocation
 * of evdev_intr handles all interrupts within a given sub-group.  The
 * intr routine may invoke evdev_intr multiple times if interrupts in
 * different subgroups are received simultaneously.
 */
/* ARGSUSED */
void
evdev_intr(eframe_t *ep, void *arg)
{
	evreg_t ev_level;
	evintr_t *devintr;
	int intrlevel;		/* software interrupt level */
	int base_intrlevel;	/* sw level we're currently handling */

#ifdef ULI_TSTAMP1
	ULI_GET_TS(TS_DEVINTR);
#endif

	/*
	 * Loop continuously, handling whatever new interrupts
	 * arrive at the current software interrupt level.
	 */
	ev_level = EV_GET_LOCAL_HPIL;
#if DEBUG
	if (ev_level == 0) {
		if (EV_GET_LOCAL(EV_IP0) && 1)
			debug("ringd");
		extra_intr_call();
		return;
	}
#endif
	base_intrlevel = intrlevel = intr_level(ev_level, SRB_DEV);

	do {
		devintr = &evintr[ev_level];
		EV_SET_LOCAL(EV_CIPL0, ev_level);

		if (devintr->evi_flags & THD_OK) {
#ifdef ITHREAD_LATENCY
			xthread_set_istamp(devintr->evi_lat);
#endif
			vsema(&devintr->evi_isync);

		} else	/* Do the call the non-threaded way */
			devintr->evi_func(ep, devintr->evi_arg);

		SYSINFO.intr_svcd++;

		if (!(ev_level = EV_GET_LOCAL_HPIL))
			break;

		intrlevel = intr_level(ev_level, SRB_DEV);
	} while (intrlevel == base_intrlevel);

	/*
	 * adjust accounting, since one extra was added in "intr".
	 */
	SYSINFO.intr_svcd--;
}

/*
 * Poll the interrupt register to see if another cpu has asked us
 * to drop into the debugger (without lowering spl).
 */
void
chkdebug(void)
{
	evreg_t ev_level = EV_GET_LOCAL_HPIL;
	if (get_intr_level(ev_level, SRB_DEV) == INTR_DEBUG)
		evdebug((eframe_t *)NULL);
}

/* ARGSUSED */
void
evdebug(eframe_t *ep)
{
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_DEBUG);

	debug("zing");

	return;
}

/*
 * Generate an interprocessor interrupt to the specified destination.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	ulong intrdest;
	int intlevel;
#ifdef IP25
	int intrlockspl;
#endif

	ASSERT((status == DOACTION) || (status == DOTLBACTION));

	if (destid == EVINTR_GLOBALDEST)
		intrdest = EVINTR_GLOBALDEST;
	else if (destid >= EV_MAX_CPUS)
		intrdest = EVINTR_GROUPDEST(destid - EV_MAX_CPUS);
	else
		intrdest = EVINTR_CPUDEST(destid);

	intlevel = (status == DOACTION) ?
				EVINTR_LEVEL_CPUACTION : EVINTR_LEVEL_TLB;
	EV_INTR_LOCK(intrlockspl);
	EV_SET_LOCAL(EV_SENDINT,
			EVINTR_VECTOR(intlevel, intrdest));
	EV_INTR_UNLOCK(intrlockspl);
	return 0;
}

/*
 * Should never receive an exception while running on the idle 
 * stack.  It IS possible to handle *interrupts* while on the
 * idle stack, but an *exception* is a problem.
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
#if R4000 || R10000
	if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	    (cause & CAUSE_EXCMASK) == EXC_DBE)
#endif
#if TFP
	if (((cause & CAUSE_EXCMASK) == EXC_INT) && (cause & CAUSE_BE))
#endif
		(void)dobuserre(NULL, epc, 0);

	cmn_err(CE_PANIC,
	"exception on IDLE stack k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
		k1, epc, cause, sp, getbadvaddr());
	/* NOTREACHED */
}


/*
 * earlynofault - handle very early global faults - usually just while
 *	sizing memory
 * Returns: 1 if should do nofault
 *	    0 if not
 */
/* ARGSUSED */
int
earlynofault(eframe_t *ep, uint code)
{
	switch(code) {
	case EXC_DBE:
		return(1);
	default:
		return(0);
	}
}


extern int  everest_error_intr_reason;

/* ARGSUSED */
void
cpuintr(eframe_t *ep, uint arg)
{
	/*
	 * Frame Scheduler
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_CPUINTR, NULL, NULL,
			 NULL, NULL);
	if (everest_error_intr_reason)
		everest_error_intr();
	doacvec();
	doactions();
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_CPUINTR, NULL,
			 NULL, NULL);
}

/* This interrupt is above splhi() and below splprof(), thus avoiding
 * some of the interrupt hold-offs due to splhi().  This is reasonsable
 * since we never dispatch an ithread and the amount of work performed
 * is relatively small.
 *
 * NOTE: This should really be regarded as an extension of the HW, performing
 * a function which could possibly be performed by an external cpu agent.
 *
 * Currently we only flush all of the random tlb entries.
 */
/* ARGSUSED */
void
tlbintr(eframe_t *ep, void *arg)
{
	extern void tlbflush_rand(void);

	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_TLB);

	tlbflush_rand();
}


/*
 * Cause all CPUs to stop by sending them each a DEBUG interrupt.
 * Parameter is actually a (cpumask_t *).
 */
void
debug_stop_all_cpus(void *stoplist)
{
	int cpu;
	ulong intrdest;

#if MULTIKERNEL
	for (cpu=0; cpu<(maxcpus + evmk.numcells - 1); cpu++) {
#else
	for (cpu=0; cpu<maxcpus; cpu++) {
#endif
		if (cpu == cpuid())
			continue;
		/* "-1" is the old style parameter OR could be the new style
		 * if no-one is currently stopped.  We only stop the
		 * requested cpus, the others are already stopped (probably
		 * at a breakpoint).
		 */
		if (((cpumask_t *)stoplist != (cpumask_t *)-1LL) &&
		    (!CPUMASK_TSTB(*(cpumask_t*)stoplist, cpu)))
			continue;

		intrdest=EVINTR_CPUDEST(cpu);

		EV_SET_LOCAL(EV_SENDINT,
			EVINTR_VECTOR(EVINTR_LEVEL_DEBUG, intrdest));
	}
}

/*
 * is_thd
 *
 * Function which tells whether a given everest interrupt number should
 * be threaded.  The following interrupts are not handled via ithreads:
 *	FCG and DANG interrupts (graphics)
 *	EPC_INTR4 (uli interrupt)
 *	EPC_DUARTS4 (uli interrupt) unless DEBUG_INTR_TSTAMP is defined
 *	a bunch of error handlers
 * Consult evintr.h for a better idea on how is_thd() is abusing
 * the order of evintr levels.
 *
 * The fix for bug #690661 turns of threading of VME interrupts at this
 * level of interrupt handling and shunts the threading decision to the
 * VME level in irix/kern/os/vme.c. This allows VME device interrupt
 * handlers to be threaded when needed and keep VME uli's from running
 * threaded as they did in 6.2, which is the expected behavior.
 */
#define DANG_OR_FCG_LVL(x)	(x > EVINTR_LEVEL_VMEMAX && \
				 x < EVINTR_LEVEL_EPC_ENET)
#ifdef DEBUG_INTR_TSTAMP
#define ULI_LVL(x)		(x == EVINTR_LEVEL_EPC_INTR4)
#else
#define ULI_LVL(x)		(x == EVINTR_LEVEL_EPC_INTR4 || \
				 x == EVINTR_LEVEL_EPC_DUARTS4)
#endif

#if defined(ULI)
#define VME_LVL(x)              (x <= EVINTR_LEVEL_VMEMAX && \
				 x >= EVINTR_LEVEL_VMECC_START)
#else
#define VME_LVL(x)              0
#endif

#define ERR_LVL(x)		(x > EVINTR_LEVEL_EPC_DUARTS4)

int
is_thd(int lvl)
{	
	if (DANG_OR_FCG_LVL(lvl) || ULI_LVL(lvl) ||
	    ERR_LVL(lvl) || VME_LVL(lvl))
		return 0;
	return 1;
}

static void
thd_evintr_init(int level, int force)
{
	if (force)
		evintr[level].evi_flags = is_thd(level)?THD_ISTHREAD:0;
	else if (is_thd(level)) {
		/* If already a thread, do not alter existing flags */
		if (!(evintr[level].evi_flags & THD_ISTHREAD))
			evintr[level].evi_flags = THD_ISTHREAD;
	} else
		evintr[level].evi_flags = 0;
	
} /* thd_evintr_init */

/*
 * This routine is called once during startup.  Some (most!) interrupts
 * get registered before threads can be setup on their behalf.  We don't
 * worry about locking, since this is done while still single-threaded,
 * and if an interrupt comes in while we're traversing the table, it will
 * either run the old way, or, if it sees the THD_OK bit, then we've
 * already set up the thread, so it can just signal the thread's sema.
 */
void
enable_ithreads (void)
{
	int level;
#if defined(ULI)
	/*
	 * fix for #690661
	 */
	extern void vme_enable_ithreads(void);
#endif /* defined(ULI) */

	ithreads_enabled=1;
	for (level=0; level<EVINTR_MAX_LEVELS;level++) {
		if (evintr[level].evi_flags & THD_REG) {
			ASSERT ((evintr[level].evi_flags & THD_ISTHREAD));
			ASSERT (!(evintr[level].evi_flags & THD_OK));
			evintr_thread_setup(level);
		}
	}
#if defined(ULI)
        /*
         * fix for #690661
         */
	vme_enable_ithreads();
#endif /* defined(ULI) */
}

/*
 * evdev_intrd
 *
 * Wrapper for evdev interrupt threads.
 * Note: ipsema is not a normal p operation.  On each call, the ithread
 * is restarted.
 */
static void
evdev_intrd(evintr_t *evp)
{
	ASSERT(cpuid() == evp->evi_dest);  /* Called on each restart */

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(evp->evi_lat);
#endif /* ITHREAD_LATENCY */
	evp->evi_func(NULL, evp->evi_arg);
	ipsema(&evp->evi_isync);
	/* NOTREACHED */
}

/* Do the one-time thread initialization work to set up the containing
 * ithread.  Then, move into the tight interrupt handling loop.
 * By changing the ithread function, we make the thread restart directly
 * into the evdev_intrd function after the ipsema() call.
 */
static void
evdev_intrd_start(evintr_t *evp)
{
	setmustrun(evp->evi_dest);
	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)evdev_intrd, evp);
	atomicSetInt(&evp->evi_flags, THD_INIT);
	ipsema(&evp->evi_isync);  /* Comes out in evdev_intrd */
	/* NOTREACHED */
}

static void
evintr_thread_setup(int level) {
	char thread_name[32];
	int pri;
	sprintf(thread_name, "evdev_intrd%d", level);

	if (evintr[level].evi_level == EVINTR_LEVEL_EPC_DUART1)
	    pri = 255;
	else
	    pri = 230;

	/* XXX need to map lvl to relative pri */
	xthread_setup(thread_name,
		      pri,
		      &evintr[level].evi_tinfo,
		      (xt_func_t *)evdev_intrd_start, &evintr[level]);
}

#ifdef DEBUG_INTR_TSTAMP
/*
 * The following routine lets the device handler change from ithread
 * to interrupt stack operation.
 * NOTE: Assumes that driver knows what it's doing and that driver
 * has already performed a evintr_connect() call on a threaded level.
 */
void
thrd_evintr_noithread(int level)
{
	atomicClearInt(&evintr[level].evi_flags, THD_OK);
}


/*
 * The following routine lets the device handler change from 
 * interrupt stack to ithread operation.
 * NOTE: Assumes that driver knows what it's doing and that driver
 * has already performed a thrd_evintr_connect call().
 */

void
thrd_evintr_noistack(int level)
{
	atomicSetInt(&evintr[level].evi_flags, THD_OK);
}
#endif /* DEBUG_INTR_TSTAMP */
