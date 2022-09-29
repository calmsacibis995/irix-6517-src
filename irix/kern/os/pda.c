/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.301 $"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <ksys/ddmap.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/i8254clock.h>
#include <sys/immu.h>
#include <sys/invent.h>
#include <sys/mount.h>
#include <sys/sema_private.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/rtmon.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <bsd/sys/tcpipstats.h>
#include <sys/pfdat.h>
#include <sys/nodepda.h>
#include <sys/atomic_ops.h>
#include <sys/traplog.h>
#include <ksys/vm_pool.h>
#include <sys/lpage.h>
#if defined (MH_R10000_SPECULATION_WAR) || defined(MH_R10000_DMA_CHECK)
#include <sys/pfdat.h>
#include <sys/var.h>
extern is_in_pfdat(pgno_t pfn);
#endif /* MH_R10000_SPECULATION_WAR || MH_R10000_DMA_CHECK */
#include <sys/mload.h>

#ifdef NUMA_BASE
#include <sys/pmo.h>
#endif /* NUMA_BASE */

#ifdef R4600
extern int	get_cpu_irr(void);
#endif /* R4600 */
#if (defined(DEBUG) || defined(DEBUG_INTR_TSTAMP_DEBUG))  && (defined(EVEREST) || defined(SN0))
#define	START_SPLHI_TIMER(x)	x = GET_LOCAL_RTC
#if EVEREST
#define	END_SPLHI_TIMER(x)	x = (GET_LOCAL_RTC - x)*21/1000
#endif
#if SN0
#define	END_SPLHI_TIMER(x)	x = (GET_LOCAL_RTC - x)
#endif
#define	CHECK_SPLHI_TIME	1
#else
#define	START_SPLHI_TIMER(x)
#define	END_SPLHI_TIMER(x)
#undef	CHECK_SPLHI_TIME
#endif

/*
 * master interrupt stack 
 */
#if _MIPS_SIM == _ABI64
#define INTSTACKSIZE	16384
#define	BSTCKSZ		2048
#else
#define INTSTACKSIZE	8192
#define	BSTCKSZ		1024
#endif
#define	K0_XUT_VEC	(K0BASE+0x80)	/* should really be in sys/sbd.h */

int intstacksize = INTSTACKSIZE;
k_machreg_t intstack[INTSTACKSIZE/sizeof(k_machreg_t)] = { 0 } ;
		/* in .data so can ref it pronto */
k_machreg_t *eintstack =
		 &intstack[(INTSTACKSIZE-sizeof(eframe_t))/sizeof(k_machreg_t)];

lock_t	bootlck;			/* lock for booting */

mutex_t	isolate_sema;
cpumask_t isolate_cpumask;
cpumask_t allset_cpumask;
cpumask_t allclr_cpumask;
extern int global_panic_count;

#ifdef ISOLATE_DEBUG
int isolate_debug = 1;
int isolate_drop = 0;
#endif

extern unchar	prod_assertion_level;

/*
 * Private data areas and indirection table.
 */
pda_t		*masterpda;		/* master processor's pda */
cpuid_t		master_procid = 0;	/* master processor's id */
int		numcpus;		/* number of configured processors */
cpumask_t	maskcpus;		/* mask of configured processors */
int		numnodes;		/* number of configured nodes */
int		maxnodes;		/* maximum configured nodes */
cnodemask_t     boot_cnodemask;		/* mask of booted nodes */
cpuid_t		clock_processor;	/* ID of processor that handles clock */
cpuid_t		fastclock_processor;	/* processor that handles fast clock */
int		cachewrback;		/* caches must be written out to mem */
extern short	kdebug;
extern int 	fastclock;

#ifdef	NUMA_BASE
nodepda_t	*Nodepdaindr[MAX_COMPACT_NODES];
#else
nodepda_t	*Nodepdaindr;
#endif


struct kna kna;				/* master kna */
struct ksa ksa;				/* master ksa */

/* On SN0 machines the following global values are used to indicate the
 * the initial state of the utlbmiss handler prolog, based on whether the
 * kernel is a DEBUG kernel or has idbg loaded.  The values used once the 
 * slave cpus are booted are contained in the nodepda.
 *
 * On non-SN0 systems these global values are still used after the slave
 * cpus boot.
 */
volatile int	need_utlbmiss_patch;
volatile uint	utlbmiss_patched;

#if SN0
#define	UTLBMISS_LOCK		NODEPDA(cnodeid())->node_utlbswitchlock
#define	UTLBMISS_NEED_PATCH	NODEPDA(cnodeid())->node_need_utlbmiss_patch
#define	UTLBMISS_PATCHED	NODEPDA(cnodeid())->node_utlbmiss_patched
#define	UTLBMISS_FLUSH		NODEPDA(cnodeid())->node_utlbmiss_flush
#else /* !SN0 */
#define	UTLBMISS_LOCK		utlbswitchlock
#define	UTLBMISS_NEED_PATCH	need_utlbmiss_patch
#define	UTLBMISS_PATCHED	utlbmiss_patched
#define	UTLBMISS_FLUSH		utlbmiss_flush

volatile cpumask_t	utlbmiss_flush;
lock_t		utlbswitchlock;
#endif /* !SN0 */

static int	cdrv_lock(struct cdevsw *, cpu_cookie_t *was_running);
static void	cdrv_unlock(int lock_type, cpu_cookie_t was_running);
static int	bdrv_lock(struct bdevsw *, cpu_cookie_t *was_running);
static void	bdrv_unlock(int lock_type, cpu_cookie_t was_running);
static void	pda_actioninit(int);

extern void	str_init_master(cpuid_t);
extern void	slpinit(void);
extern void	delay_calibrate(void);
extern void	flushicache(void);

#if SW_FAST_CACHE_SYNCH
extern void sw_cachesynch_patch(void);
#endif

/*
 * Frame Scheduler
 */
extern void* frs_alloc(int cpu);

/*
 * Called extremely early in initialization so that the boot CPU
 * has some personal context that it needs.
 * NOTE: semaphores initialized here currently do NOT get any history/meter
 * struct allocated since the kernel heap hasn't been initialized yet
 */
void
sinit(void)
{
	extern lock_t	histlock;
	extern sema_t	synclock;
	extern sema_t	uasem;
	extern mutex_t	aclock;
	extern mutex_t	mpclkchg;	/* single thread clock changes */
	extern sv_t	runout;		/* sched sync variables */

	spinlock_init(&histlock, "histlk");
	spinlock_init(&timelock, "timelk");
	spinlock_init(&putbuflck, "putbuf");
#ifndef SN0
	spinlock_init(&utlbswitchlock, "utlbsw");	/* UTLB swtch lock */
#endif
	mutex_init(&isolate_sema, MUTEX_DEFAULT, "isolate");
	sv_init(&runout, SV_DEFAULT, "runout");
	initnsema(&synclock, 1, "synclck");
	initnsema(&uasem, 1, "uadmin");
	mutex_init(&aclock, MUTEX_DEFAULT, "acctlck");
	mutex_init(&mpclkchg, MUTEX_DEFAULT, "clkchg");

	slpinit();
}

/* Initialize the semaphores in the cdevsw/bdevsw tables */
void
devswinit(void)
{
}

#if R4000 || R10000
extern	int utlbmiss_prolog_size;
#ifdef R4600
extern	inst_t utlbmiss_r4600[], eutlbmiss_r4600[];
extern	inst_t utlbmiss_r4600_1[], eutlbmiss_r4600_1[];
#endif /* R4600 */
#ifdef _R5000_BADVADDR_WAR
extern	inst_t utlbmiss_r5000[];
extern	inst_t eutlbmiss_r5000[];
extern	inst_t utlbmiss2_r5000[];
extern	inst_t eutlbmiss2_r5000[];
extern	inst_t utlbmiss1_r5000[];
extern	inst_t eutlbmiss1_r5000[];
extern	inst_t utlbmiss3_r5000[];
extern	inst_t eutlbmiss3_r5000[];
extern  int _r5000_badvaddr_war;
#endif /* _R5000_BADVADDR_WAR */
#if MP || IP28
extern	inst_t utlbmiss_prolog_mp[], eutlbmiss_prolog_mp[];
#endif /* MP || IP28 */
extern	inst_t utlbmiss[], eutlbmiss[];
extern	inst_t utlbmiss_prolog_patch[];
extern	inst_t utlbmissj[], eutlbmissj[];

#if	((defined(R4000) || defined(R10000)) && defined(PTE_64BIT))
/*
 * Large page tlbmiss handler and the combinations for
 * both overlay and counting features.
 */
extern	inst_t lpg_utlbmiss[], elpg_utlbmiss[];
extern	inst_t lpg_overlay_utlbmiss[], elpg_overlay_utlbmiss[];
extern	inst_t lpg_counting_utlbmiss[], elpg_counting_utlbmiss[];
extern	inst_t lpg_overlay_counting_utlbmiss[], 
		elpg_overlay_counting_utlbmiss[];
#endif


#if R4000 && (IP19 || IP22)
extern	inst_t utlbmiss_250mhz[], eutlbmiss_250mhz[];
extern	inst_t utlbmiss1_250mhz[], eutlbmiss1_250mhz[];
extern	inst_t utlbmiss2_250mhz[], eutlbmiss2_250mhz[];
extern	inst_t utlbmiss3_250mhz[], eutlbmiss3_250mhz[];
extern	int has_250;
#endif	/* R4000 && (IP19 || IP22) */
#endif /* R4000 || R10000 */
extern	inst_t utlbmiss[], eutlbmiss[];
extern	inst_t utlbmiss1[], eutlbmiss1[];
extern	inst_t utlbmiss2[], eutlbmiss2[];
extern	inst_t utlbmiss3[], eutlbmiss3[];
#if TFP
extern	int trap_table(void), trap_table_vector(void);
extern	int trap_table2(void);
#endif

struct utlbmiss_swtch utlbmiss_swtch[NUTLBMISS_HNDLRS] = {
	{ utlbmiss,	eutlbmiss } ,	/* standard utlbmiss handler */
	{ utlbmiss1,	eutlbmiss1 } ,	/* overlay segments handler */
	{ utlbmiss2,	eutlbmiss2 } ,	/* utlbmiss counting handler */

#if	((defined(R4000) || defined(R10000)) && defined(PTE_64BIT))
	{ utlbmiss3,	eutlbmiss3 },	/* overlay with counting */
	/* Large page size utlbmiss handler */
	{ lpg_utlbmiss,	elpg_utlbmiss }, 

	/* Large page size counting utlbmiss handler */
	{ lpg_counting_utlbmiss, elpg_counting_utlbmiss },

	/* Large page size overlay utlbmiss handler */
	{ lpg_overlay_utlbmiss, elpg_overlay_utlbmiss },

	/* Large page size overlay and counting utlbmiss handler */
	{ lpg_overlay_counting_utlbmiss, elpg_overlay_counting_utlbmiss}
#else
	{ utlbmiss3,	eutlbmiss3 }	/* overlay with counting */
#endif
};

extern int r10k_check_count;

void
alloc_cpupda(int i)
{
	cnodeid_t	node;
        nasid_t         nasid;
	pfd_t		*pfd;
	int		color;

	pdaindr[i].CpuId = -1;

	node = get_cpu_cnode(i);
        nasid = COMPACT_TO_NASID_NODEID(node);

	/* In general, the pda page is accessed both from K0 and
	 * PDAPAGE (0xffffb000). Places which go through
	 * pdaindr[X].pda are using the K0 address.
	 * For the R4000, this means that the VCE problem can
	 * occur if the K0 and PDAPAGE addresses don't hit
	 * on the same 'cache color'. This case is particularly
	 * bad since the VCE would occur very early in
	 * the exception handler (where we access PDAPAGE),
	 * before we are in any shape to handle nested exceptions.
	 */

	if (i != master_procid ||
	    pdaindr[i].pda == NULL) {

		/* There should be no reason for this to fail this early on. */

		if (reservemem(GLOBAL_POOL, 1, 1, 1))
			cmn_err(CE_PANIC, 
				"Can't reservemem for pda for cpu %d", i);

#ifndef IP19
		color = colorof(PDAPAGE);
#ifdef SN
		/*
		 * On SN systems, try to assign a unique scache color to each PDA page.
		 */
		color += i*(cachecolormask+1) + pfntocachekey(kvatopfn(masterpda), NBPP);
#endif
		pfd = pagealloc_node(node, color, 0);
#else /* IP19 */
		/* Note that currently pagealloc_node performs VM_VACOLOR
		 * allocations modulo secondary cache size.  We only want to
		 * avoid VCEs so we want it modulo primary cache color mask
		 * size.  Note that the other allocation (scache size) 
		 * causes us to hit the IP19 MA chip hardware hang (552719)
		 * so we kludge around it here.
		 */
		pfd = pagealloc_node(node,
				     colorof(PDAPAGE)+i*(cachecolormask+1), 0);
#endif /* IP19 */

		if (pfd == NULL)
			cmn_err(CE_PANIC,
				"Can't allocate pda page for cpu %d", i);

		pfd->pf_flags |= P_DUMP;
		pdaindr[i].pda = (pda_t *) PHYS_TO_K0(pfdattophys(pfd) +
						      PDAOFFSET);

#ifndef SABLE
		/* zero out our portion of pda */
		bzero((caddr_t) pdaindr[i].pda + sizeof(pdaindr[i].pda->db),
		      PDASIZE - sizeof(pdaindr[i].pda->db));
#endif
	}

	pdaindr[i].pda->p_nodeid = node;
        pdaindr[i].pda->p_nasid  = nasid;
	pdaindr[i].pda->p_panicking = 0;
	pdaindr[i].pda->p_cpunamestr = (char *)NULL;

#ifdef	NUMA_BASE
	/* p_nodepda defined only for NUMA systems */
	pdaindr[i].pda->p_nodepda = NODEPDA_GLOBAL(node); 
#endif	/* NUMA_BASE */

	/* init my boot stack pointers */
	pdaindr[i].pda->p_bootstack = (k_machreg_t *)
			(((__psunsigned_t)pdaindr[i].pda + PDASIZE) - BSTCKSZ);
	/* watch out for cmn_err args!! (ref: 18*4) */
	pdaindr[i].pda->p_bootlastframe = (k_machreg_t *)
		((__psunsigned_t)pdaindr[i].pda->p_bootstack+BSTCKSZ-(18*4));

	/*
	 * Frame Scheduler
	 */
	pdaindr[i].pda->p_frs_flags = 0;
	pdaindr[i].pda->p_frs_objp = frs_alloc(i);
	pdaindr[i].pda->p_tstamp_objp = NULL;

	pdaindr[i].pda->p_switchuthread = 0;
	pdaindr[i].pda->p_switching = 0;

#ifndef SN0
	mutex_init(&pdaindr[i].pda->p_pcopy_lock, MUTEX_DEFAULT, "pcopy");
#endif

        /*
         * mem_tick activation
         */
#ifdef NUMA_BASE
	pdaindr[i].pda->p_mem_tick_flags = 0;
	pdaindr[i].pda->p_mem_tick_quiesce = 0;
	pdaindr[i].pda->p_mem_tick_base_period = mem_tick_base_period;
	pdaindr[i].pda->p_mem_tick_counter = mem_tick_base_period;
	pdaindr[i].pda->p_mem_tick_cpu_numpfns = 0;
#endif /* NUMA_BASE */
        
#ifdef ULI
	pdaindr[i].pda->p_curuli = NULL;
#endif

	/*
	 * Product assertion support
	 */
	pdaindr[i].pda->p_asslevel = prod_assertion_level;

#if !(EVEREST && DEBUG) && !TFP && !BEAST && !(SN0 && DEBUG)
	/*
	 * This code doesn't work for DEBUG Everest kernels.
	 * It will create an infinite loop in utlbmissj by
	 * loading private.p_utlbmisshndlr and jumping to it,
	 * since that pointer will point back at utlbmissj.
	 */
	if ((long)eutlbmiss - (long)utlbmiss > SIZE_EXCVEC) {
		utlbmiss_swtch[0].u_start = utlbmissj;
		utlbmiss_swtch[0].u_end = eutlbmissj;
	}
#endif

	pdaindr[i].pda->p_utlbmissswtch = 0;
	pdaindr[i].pda->p_utlbmisshndlr = utlbmiss_swtch[0].u_start;

	{
		extern cpuid_t nointr_list[]; /* from lboot via master.c */
		int nointr_scan;
		cpuid_t scan_cpuid;

		/* 
		 * Disallow sprayed interrupts on this 
		 * CPU if flagged NOINTR.
		 */
		for (nointr_scan=0; 
		     (scan_cpuid=nointr_list[nointr_scan]) != CPU_NONE; 
		     nointr_scan++) {
			if (scan_cpuid == i) {
				pdaindr[i].pda->p_flags |= PDAF_NOINTR;
				break;
			}
		}
	}

	if (i != master_procid) {
		extern void calloutinit_cpu(cpuid_t);

		calloutinit_cpu(i);

		pda_actioninit(i);
	}

#ifndef SN0
	mutex_init(&pdaindr[i].pda->p_cacheop_pageslock, MUTEX_DEFAULT,
		"cacheop_pages");
#endif

#if _MIPS_SIM == _ABI64
	{
		extern int  (* const causevec[])();
		/*
		 * Copy in the exception handler vector.
		 */
		bcopy((void *)causevec,
		      (void *)pdaindr[i].pda->p_causevec,
		      sizeof(pdaindr[i].pda->p_causevec));
	}
#endif

#if SN0
	{
		extern int  (* const nofault_pc[])();
		/*
		 * Copy in the nofault PC array.
		 */
		bcopy((void *)nofault_pc,
		      (void *)pdaindr[i].pda->p_nofault_pc,
		      sizeof(pdaindr[i].pda->p_nofault_pc));
	}
	pdaindr[i].pda->p_slice = get_cpu_slice(i);
#endif

#if defined (SN0)
	pdaindr[i].pda->p_r10kcheck_cnt = r10k_check_count;
#endif /* SN0 */

	/* Init rtmon/utrace lock */
	mrlock_init(&pdaindr[i].pda->p_tstamp_lock, MRLOCK_DEFAULT, 
		    "tstamp", i);

	return;
}

/*
 * Initialize the pda's for all the other cpus.  The master pda has
 * already been allocated and initialized.
 */

void
initpdas()
{
	cpuid_t	i;

	for (i = 0; i < maxcpus; i++) 
		if (i != master_procid)
			if (cpu_enabled(i))
				alloc_cpupda(i);
			else
				pdaindr[i].CpuId = -1;
}


/*
 * initmasterpda - set up master pda, interrupt stack and prepare
 * for locks and semaphores
 * Called early in mlsetup - we are still on 'startup' stack
 * The master processor is defined by master_procid
 */
pfn_t
initmasterpda(register pfn_t fpage)
{
	extern inst_t exception[];
	extern int boot_sidcache_size;
	extern int boot_sdcache_size;
	register cpuid_t id = getcpuid();
#if defined(R4000) || defined(LARGE_CPU_COUNT)
	int i;
#endif

#ifdef R4600
	rev_id_t ri;

	ri.ri_uint = get_cpu_irr();
	switch (ri.ri_imp) {
#ifdef TRITON
	case C0_IMP_TRITON:
	case C0_IMP_RM5271:
#ifdef _R5000_BADVADDR_WAR
		utlbmiss_swtch[0].u_start = utlbmiss_r5000;
		utlbmiss_swtch[0].u_end = eutlbmiss_r5000;
		utlbmiss_swtch[2].u_start = utlbmiss2_r5000;
		utlbmiss_swtch[2].u_end = eutlbmiss2_r5000;
		utlbmiss_swtch[1].u_start = utlbmiss1_r5000;
		utlbmiss_swtch[1].u_end = eutlbmiss1_r5000;
		utlbmiss_swtch[3].u_start = utlbmiss3_r5000;
		utlbmiss_swtch[3].u_end = eutlbmiss3_r5000;
		break;
#endif /* _R5000_BADVADDR_WAR */
#endif /* TRITON */
	case C0_IMP_R4700:
	case C0_IMP_R4600:
		/* select fast R4600 utlbmiss */
		utlbmiss_swtch[0].u_start = utlbmiss_r4600;
		utlbmiss_swtch[0].u_end = eutlbmiss_r4600;
		utlbmiss_swtch[1].u_start = utlbmiss_r4600_1;
		utlbmiss_swtch[1].u_end = eutlbmiss_r4600_1;
		break;
	default:
		break;
	}
#endif /* R4600 */
#if R4000 && (IP19 || IP22)
	if (has_250) {
		/* Use 250MHz workaround handler, if one's present */
		utlbmiss_swtch[0].u_start = utlbmiss_250mhz;
		utlbmiss_swtch[0].u_end = eutlbmiss_250mhz;
		utlbmiss_swtch[1].u_start = utlbmiss1_250mhz;
		utlbmiss_swtch[1].u_end = eutlbmiss1_250mhz;
		utlbmiss_swtch[2].u_start = utlbmiss2_250mhz;
		utlbmiss_swtch[2].u_end = eutlbmiss2_250mhz;
		utlbmiss_swtch[3].u_start = utlbmiss3_250mhz;
		utlbmiss_swtch[3].u_end = eutlbmiss3_250mhz;
	}
#endif

#if SW_FAST_CACHE_SYNCH
	(void) sw_cachesynch_patch();
#endif

	ASSERT_ALWAYS(id == master_procid);

	alloc_cpupda(id);

	CPUMASK_CLRALL(maskcpus);
	CPUMASK_SETB(maskcpus, id);

	numcpus = 1;
	CPUMASK_CLRALL(isolate_cpumask);
#if LARGE_CPU_COUNT
	CPUMASK_CLRALL(allclr_cpumask);
	for (i=0; i<CPUMASK_SIZE; i++)
		allset_cpumask._bits[i] = -1LL;
#else
	allclr_cpumask = (cpumask_t)0;
	allset_cpumask = (cpumask_t)-1;
#endif

	pdaindr[id].CpuId = id;
	wirepda(pdaindr[id].pda);

#if (TRAPLOG_DEBUG) && (_PAGESZ == 16384)
	*(long *)TRAPLOG_PTR = TRAPLOG_BUFSTART;
#endif

	/* plug exc handler vecs in */
	private.common_excnorm = get_except_norm();

	/* set up master interrupt stack */
	/* say we're on the interrupt stack, so can handle exceptions
	 * while sizing memory, etc.
	 */
	private.p_kstackflag = PDA_CURINTSTK; /* fake out for now */
	private.p_intstack = intstack;
	private.p_intlastframe = eintstack;

	private.p_cpuid = id;
	private.p_cpumask = maskcpus;
	private.p_flags = PDAF_MASTER | PDAF_CLOCK | PDAF_ENABLED;
	private.p_kvfault = 0;
#ifdef R4000
	for (i=0; i < CACHECOLORSIZE; i++)
		private.p_clrkvflt[i] = 0;
#endif
	/*
         * the only time when fast clock is set this early is
         * on HW with no counter so we have to turn on the fast clock
         * always by default to simulate a free running counter
         * for process timing.
         */
        if (fastclock)
                private.p_flags |= PDAF_FASTCLOCK;
	fastclock_processor = clock_processor = private.p_cpuid;
	private.p_vmeipl = 0;		/* turn on only when needed */
	private.ksaptr = &ksa;
	private.knaptr = (char *)&kna;

	private.p_scachesize = ((boot_sidcache_size != 0) ? 
				boot_sidcache_size : boot_sdcache_size);
#if R4000 && DEBUG
	private.p_vcelog = (char *)PHYS_TO_K0(ctob(fpage));
	private.p_vcelog_offset = 0;
	private.p_vcecount = 0;
	fpage++;
#endif

	_hook_exceptions();	/* Now safe to hook, VPDA_EXCNORM is setup */

        /* Initialize streams buffers for the master cpu */
	str_init_master(id);

	delay_calibrate();
	cache_preempt_limit();
	private.cpufreq = findcpufreq();
#if IP22
	private.ust_cpufreq = findcpufreq_raw();
#endif

	spinlock_init(&pdaindr[id].pda->p_special, "pdaM");

	/* init boot lock - leave it 'free' - the mpconf table will make
	 * sure no-one boots until we're ready
	 */
	initnlock(&bootlck, "bootlck");		/* bootup lock */
	masterpda = pdaindr[id].pda;
	return(fpage);
}

void
wirepda(pda_t *pda)
{
	pde_t	pde;
#if R4000 || R10000
	pde_t	gbit;
	__psint_t	pdapage_aligned;
#endif

	pde.pgi = 0;
        pg_setpgi(&pde, pte_cachebits());
        pg_setpfn(&pde, pnum(K0_TO_PHYS(pda)));
	pg_setmod(&pde);
	pg_setglob(&pde);
	pg_setvalid(&pde);
#if R4000 || R10000
	gbit.pgi = 0;
	pg_setglob(&gbit);
	pdapage_aligned = ctob(btoct(PDAPAGE));
	if (pdapage_aligned & NBPP)	/* pda is an odd page */
		tlbwired(PDATLBINDEX, 0, (caddr_t)pdapage_aligned,
				gbit.pte, pde.pte, TLBPGMASK_MASK);
	else
		tlbwired(PDATLBINDEX, 0, (caddr_t)pdapage_aligned,
				pde.pte, gbit.pte, TLBPGMASK_MASK);

#if EXTKSTKSIZE == 1
	/* Save for later tlb loads to map kernel stack extension page */
	private.p_pdalo.pgi = pde.pgi;
#endif	/* EXTKSTKSIZE == 1 */
#if TLBKSLOTS == 0
	private.p_pdalo.pgi = pde.pgi;	/* Save for later tlb loads */
#endif	/* TLBKSTLOTS == 0 */

#elif TFP
	tlbwired(0, 0, (caddr_t)PDAPAGE, pde.pte);
#elif BEAST
	pg_set_page_mask_index(&pde, PAGE_SIZE_TO_MASK_INDEX(NBPP));
	tlbwired(PDATLBINDEX, 0, (caddr_t)PDAPAGE, pde.pte);
#else
#error "Invalid CPU type"
#endif	/* R4000 || R10000 */
}


void
utlbmiss_resume_nopin(utas_t *utas)
{

#if R4000 || R10000
	/*REFERENCED*/
	register int s;
	/*REFERENCED*/
	register int *target;
#endif
	ASSERT(utas->utas_utlbmissswtch);

#if TFP
	private.p_utlbmisshndlr = utlbmiss_swtch[utas->utas_utlbmissswtch].u_start;

	/* Use the shared utlbmiss handler (supports overlays)  */
	if (utas->utas_utlbmissswtch == UTLBMISS_DEFER)
		set_trapbase((__psunsigned_t)trap_table2);
	else {
		/* Use one of the counting utlbmiss handler(s)  */
		set_trapbase((__psunsigned_t)trap_table_vector);
	}
	private.p_utlbmissswtch = utas->utas_utlbmissswtch;
#endif /* TFP */

#if R4000 || R10000
#if MP || IP28
	/*
	 * copy utlbmiss handler entry to pda,
	 * then overwrite utlbmiss, if necessary,
	 * and flush icache
	 */
	private.p_utlbmisshndlr =
	    utlbmiss_swtch[utas->utas_utlbmissswtch].u_start;

	/*
	 * ... or from standard to (any) special handler.
	 */
	if (private.p_utlbmissswtch == 0) {
		ASSERT(UTLBMISS_NEED_PATCH >= 0);
		s = mutex_spinlock_spl(&UTLBMISS_LOCK, splprof);
		UTLBMISS_NEED_PATCH++;
		if (!UTLBMISS_PATCHED) {
			UTLBMISS_PATCHED = 1;
			CPUMASK_CPYNOTM(UTLBMISS_FLUSH, cpumask());
			target = (int *)UT_VEC;
			*target = utlbmiss_prolog_patch[0];
			*(int *)XUT_VEC = utlbmiss_prolog_patch[0];
			mutex_spinunlock(&UTLBMISS_LOCK, s);
			cache_operation((void *)K0_UT_VEC,
					sizeof(int),
					CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
			cache_operation((void *)K0_XUT_VEC,
					sizeof(int),
					CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
		} else if (CPUMASK_TSTB(UTLBMISS_FLUSH, cpuid())) {
			CPUMASK_CLRB(UTLBMISS_FLUSH, cpuid());
			mutex_spinunlock(&UTLBMISS_LOCK, s);
			cache_operation((void *)K0_UT_VEC,
					sizeof(int),
					CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
			cache_operation((void *)K0_XUT_VEC,
					sizeof(int),
					CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
		} else
		    mutex_spinunlock(&UTLBMISS_LOCK, s);
	}
#else	/* !MP */
	{ /* maxcpus == 1 */

		/*
		 * copy in correct utlbmiss handler to UT_VEC
		 */
		register inst_t	*start, *end;
		int size;

		ASSERT(private.p_utlbmissswtch != utas->utas_utlbmissswtch);
		target = (int *)(UT_VEC + utlbmiss_prolog_size);
		start = utlbmiss_swtch[utas->utas_utlbmissswtch].u_start;
		end = utlbmiss_swtch[utas->utas_utlbmissswtch].u_end;
		size = end - start;
		size *= sizeof(int);
		ASSERT_ALWAYS((utlbmiss_prolog_size + size) <= 0x80);
		s = splprof();

		while (start < end)
			*target++ = *start++;

		/* clean_{i,d}cache require K0SEG or K1SEG addrs */
		cache_operation((void *)(K0_UT_VEC + utlbmiss_prolog_size),
				size, CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);

		bcopy((void *)(UT_VEC+utlbmiss_prolog_size),
		      (void *)(XUT_VEC+utlbmiss_prolog_size), size);
		cache_operation((void *)(K0_XUT_VEC + utlbmiss_prolog_size),
				size, CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);

		splx(s);
	}

#endif /* !MP */

	private.p_utlbmissswtch = utas->utas_utlbmissswtch;

#endif 	/* R4000 || R10000 */

#if defined (BEAST)

	cmn_err(CE_PANIC, "utlbmiss_resume need fixing");
#endif

}

/*
 * When a process needs a special utlbmiss handler (needs utlbmiss
 * counting or requires special handling of pmap sentries), its
 * proc struct's p_utlbmisshndlr...
 * If a process needs a special utlbmiss handler, the
 * special handler code will be copied to the pda and
 * first word of the standard utlbmiss handler will
 * be overwritten with a patch that will cause the
 * standard
 *
 * To allow this to work with multiple processors (which
 * share the utlbmiss code), all pdas must have a utbmiss
 * handler installed in the pda at all times.  When a
 * branch instruction is overlayed, then, all processes...
 */
void
utlbmiss_resume(utas_t *utas)
{
	void *old_rtpin;

	/* only access private.p_utlbmissswtch when context
	 * switching, or else when protected from kernel preemption
	 * via rt_pin_thread()
	 */
	old_rtpin = rt_pin_thread();
	utlbmiss_resume_nopin(utas);
	rt_unpin_thread(old_rtpin);
}

/*
 * Reset utlbmiss handler to standard handler.
 */
void
utlbmiss_reset(void)
{
#if R4000 || R10000
	register int s;
	register int *target;
#endif


	if (private.p_utlbmissswtch == 0) return;
#if TFP
	private.p_utlbmisshndlr = utlbmiss_swtch[0].u_start;

	/*
	 * Use the standard utlbmiss handler
	 */
	set_trapbase( (__psunsigned_t)trap_table );

	private.p_utlbmissswtch = 0;
#endif /* TFP */

#if R4000 || R10000
#if MP || IP28
	/*
	 * copy utlbmiss handler entry to pda,
	 * then overwrite utlbmiss, if necessary,
	 * and flush icache
	 */
	private.p_utlbmisshndlr = utlbmiss_swtch[0].u_start;

	/*
	 * State change from (any) special utlbmiss
	 * handler to the standard utlbmiss handler
	 */
	ASSERT(UTLBMISS_NEED_PATCH > 0);
	s = mutex_spinlock_spl(&UTLBMISS_LOCK, splprof);
	/*
	 * If no other process needs a special handler now,
	 * reinstate original utlbmiss instruction and
	 * clear "flush {i,d}cache" flag.
	 */
	if (--UTLBMISS_NEED_PATCH == 0) {
		target = (int *)UT_VEC;
		*target = utlbmiss_prolog_mp[0];
		*(int *)XUT_VEC = utlbmiss_prolog_mp[0];
		CPUMASK_CLRALL(UTLBMISS_FLUSH);
		UTLBMISS_PATCHED = 0;
#ifdef NOTDEF
		Not worth the bother to clean the caches,
		because the worst that will happen is that
		once or twice the user will execute the
		utlbmiss_patch instruction and waste a
		half-dozen cycles.
		    cache_operation((void *)K0_UT_VEC,
				    sizeof(int),
				    CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
#endif
	}
	mutex_spinunlock(&UTLBMISS_LOCK, s);
#else	/* !MP */
	{ /* maxcpus == 1 */

		/*
		 * copy in standard utlbmiss handler to UT_VEC
		 */
		register inst_t	*start, *end;
		int size;

		target = (int *)(UT_VEC + utlbmiss_prolog_size);
		start = utlbmiss_swtch[0].u_start;
		end = utlbmiss_swtch[0].u_end;
		size = end - start;
		size *= sizeof(int);
		ASSERT_ALWAYS((utlbmiss_prolog_size + size) <= 0x80);
		s = splprof();

		while (start < end)
			*target++ = *start++;

		/* clean_{i,d}cache require K0SEG or K1SEG addrs */
		cache_operation((void *)(K0_UT_VEC + utlbmiss_prolog_size),
				size, CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
		bcopy((void *)(UT_VEC+utlbmiss_prolog_size),
		      (void *)(XUT_VEC+utlbmiss_prolog_size), size);
		cache_operation((void *)(K0_XUT_VEC + utlbmiss_prolog_size),
				size, CACH_ICACHE_COHERENCY|CACH_LOCAL_ONLY);
		splx(s);
	}

#endif /* !MP */

	private.p_utlbmissswtch = 0;
#endif 	/* R4000 || R10000 */
#if defined (BEAST)

	cmn_err(CE_PANIC, "utlbmiss_reset need fixing");
#endif

}


/*
 * Common code for character doing driver locking.  It returns the lock
 * type and old cpu to be passed to drv_unlock().
 */
static int
cdrv_lock(
	register struct cdevsw 	*my_cdevsw,
	cpu_cookie_t		*was_running)
{
	register int	lock_type;

	/*
	 * Do appropriate driver locking.
	 */
	switch (lock_type = (*(my_cdevsw->d_flags) & DLOCK_MASK)) {

	case D_MT:	/* driver is thread-aware */
	case D_MP:	/* driver does own semaphoring */
		break;

	case D_PROCESSOR:	/* force routine onto master processor */
		if (cpuid() != my_cdevsw->d_cpulock)
			DRVLOCK.p_indcdevsw++;
		*was_running = setmustrun(my_cdevsw->d_cpulock);
		ASSERT(cpuid() == my_cdevsw->d_cpulock);
		break;

	default:
		cmn_err_tag(95,CE_PANIC, "illegal driver locking type");
		break;
	}

	return lock_type;
}

/*
 * Common code for character driver unlocking.  Just restore the process
 * to running on the old cpu if it has D_PROCESSOR locking.
 *
 * Lock_type and was_running should be the values returned from
 * a previous call to drv_lock().
 */
static void
cdrv_unlock(
	register int	lock_type,
	cpu_cookie_t	was_running)
{
	/*
	 * Do driver unlocking.
	 */
	if (lock_type == D_PROCESSOR) {
		/* reset processor selection */
		restoremustrun(was_running);
	}
}

/*
 * Centralized cdevsw[].d_map routine.
 */
int
cdmap(
	register struct cdevsw *my_cdevsw,
	dev_t		rdev,
	vhandl_t	*vt,
	off_t		off,
	size_t		len,
	uint_t		prot)
{
	int	retval;
	int	lock_type;
	cpu_cookie_t was_running;

	DRVLOCK.p_indcdev++;

	lock_type = cdrv_lock(my_cdevsw, &was_running);

	retval = ((int(*)(dev_t,vhandl_t*,off_t,size_t,uint_t))
		  my_cdevsw->d_map)(rdev, vt, off, len, prot);

	cdrv_unlock(lock_type, was_running);

	return retval;
}

/*
 * Centralized cdevsw[].d_unmap routine.
 */
int
cdunmap(
	register struct cdevsw *my_cdevsw,
	dev_t		rdev,
	vhandl_t	*vt)
{
	int	retval;
	int	lock_type;
	cpu_cookie_t was_running;

	DRVLOCK.p_indcdev++;

	lock_type = cdrv_lock(my_cdevsw, &was_running);

	retval = ((int(*)(dev_t,vhandl_t*))my_cdevsw->d_unmap)(rdev, vt);

	cdrv_unlock(lock_type, was_running);

	return retval;
}

/*
 * Centralized cdevsw[].d_mmap routine.
 */
int
cdmmap(
	register struct cdevsw *my_cdevsw,
	dev_t		rdev,
	off_t		off,
	uint_t		prot)
{
	int	retval;
	int	lock_type;
	cpu_cookie_t was_running;

	DRVLOCK.p_indcdev++;

	lock_type = cdrv_lock(my_cdevsw, &was_running);

	retval = ((int(*)(dev_t,off_t,uint_t))
		  my_cdevsw->d_mmap)(rdev, off, prot);

	cdrv_unlock(lock_type, was_running);

	return retval;
}

int
cdrv(register struct cdevsw *my_cdevsw, int routine, ...)
{
	va_list ap;
	void *a1, *a2, *a3, *a4, *a5, *a6;
	typedef int (*cdevfunc_t)(void *, void *, void *, void *, void *, void *);
	register cdevfunc_t func;
	register lock_type;
	cpu_cookie_t was_running;
	register int retval;
	register int release=0;

	void	mload_driver_hold(cfg_desc_t *desc);
	void	mload_driver_release(cfg_desc_t *desc);
	int	mload_driver_load(cfg_desc_t *);
	
	if (*my_cdevsw->d_flags & D_OBSOLD)
		return ENOTSUP;
	DRVLOCK.p_indcdev++;

	va_start(ap, routine);
	a1 = va_arg(ap, void *);
	a2 = va_arg(ap, void *);
	a3 = va_arg(ap, void *);
	a4 = va_arg(ap, void *);
	a5 = va_arg(ap, void *);
	a6 = va_arg(ap, void *);
	switch (routine) {
	case DC_OPEN:
		if (!cdstatic(my_cdevsw)) {
			/*
			 * To avoid the possibility of the driver being
			 * unloaded before we can actually make the open
			 * call, we increment the refcnt. We don't use the
			 * "usual" cdhold/cdrele interface because of the
			 * goofy implementation of devhold which could
			 * cause a deadlock if the _attach() routine is
			 * called directly from a loadable driver's _reg() routine.
			 */
			mload_driver_hold(my_cdevsw->d_desc);
			++release;

			/* 
			 * open func being NULL means the driver has not
			 * been loaded. Load the driver and then try the
			 * load func again. If M_TRANSITION and
			 * M_REGISTERED are both on, it means someone else
			 * is loading the driver and we need to
			 * synchronize. If M_TRANSITION is on but
			 * M_REGISTERED isn't, we're in a startup situation
			 * where lboot is trying to register the driver and
			 * the _attach() call came as a result of a CDL
			 * match. In such a case we don't want to load the
			 * driver, because it's already loaded, but in a
			 * "pseudo-transitional" state .... yes, yuck!!!
			 */
			if ((func = (cdevfunc_t)my_cdevsw->d_open) == NULL ||
			    (my_cdevsw->d_desc->m_flags & (M_TRANSITION | M_REGISTERED) == (M_TRANSITION | M_REGISTERED)))
			{
				if (retval = mload_driver_load(my_cdevsw->d_desc)) {
					mload_driver_release(my_cdevsw->d_desc);
					return(retval);
				}
				func = (cdevfunc_t)my_cdevsw->d_open;
			}
		}
		else
			func = (cdevfunc_t)my_cdevsw->d_open;
		break;
	case DC_CLOSE:
		func = (cdevfunc_t)my_cdevsw->d_close;
		break;
	case DC_READ:
		func = (cdevfunc_t)my_cdevsw->d_read;
		break;
	case DC_WRITE:
		func = (cdevfunc_t)my_cdevsw->d_write;
		break;
	case DC_IOCTL:
		func = (cdevfunc_t)my_cdevsw->d_ioctl;
		break;
	case DC_POLL:
		func = (cdevfunc_t)my_cdevsw->d_poll;
		break;
	case DC_ATTACH:
		if (!cdstatic(my_cdevsw)) {
			/*
			 * To avoid the possibility of the driver being
			 * unloaded before we can actually make the open
			 * call, we increment the refcnt. We don't use the
			 * "usual" cdhold/cdrele interface because of the
			 * goofy implementation of devhold which could
			 * cause a deadlock if the _attach() routine is
			 * called directly from a loadable driver's _reg() routine.
			 */
			mload_driver_hold(my_cdevsw->d_desc);
			++release;

			/* 
			 * open func being NULL means the driver has not
			 * been loaded. Load the driver and then try the
			 * load func again. If M_TRANSITION and
			 * M_REGISTERED are both on, it means someone else
			 * is loading the driver and we need to
			 * synchronize. If M_TRANSITION is on but
			 * M_REGISTERED isn't, we're in a startup situation
			 * where lboot is trying to register the driver and
			 * the _attach() call came as a result of a CDL
			 * match. In such a case we don't want to load the
			 * driver, because it's already loaded, but in a
			 * "pseudo-transitional" state .... yes, yuck!!!  
			 */
			if ((func = (cdevfunc_t)my_cdevsw->d_attach) == NULL ||
			    (my_cdevsw->d_desc->m_flags & (M_TRANSITION | M_REGISTERED) == (M_TRANSITION | M_REGISTERED)))
			{
				if (retval = mload_driver_load(my_cdevsw->d_desc)) {
					mload_driver_release(my_cdevsw->d_desc);
					return(retval);
				}
				func = (cdevfunc_t)my_cdevsw->d_attach;
			}
		}
		else
			func = (cdevfunc_t)my_cdevsw->d_attach;
		break;
	case DC_DETACH:
		func = (cdevfunc_t)my_cdevsw->d_detach;
		break;
	case DC_ENABLE:
		func = (cdevfunc_t)my_cdevsw->d_enable;
		break;
	case DC_DISABLE:
		func = (cdevfunc_t)my_cdevsw->d_disable;
		break;
	default:
		cmn_err(CE_PANIC, "cdrv\n");
		break;
	}

	/* Do appropriate driver locking. */
	lock_type = cdrv_lock(my_cdevsw, &was_running);
	retval = (*func)(a1, a2, a3, a4, a5, a6);
	cdrv_unlock(lock_type, was_running);

	if (release)
		mload_driver_release(my_cdevsw->d_desc);

	return retval;
}

/*
 * Common code for doing block driver locking.  It returns the lock
 * type and old cpu to be passed to bdrv_unlock().
 */
static int
bdrv_lock(
	register struct bdevsw 	*my_bdevsw,
	cpu_cookie_t		*was_running)
{
	register int	lock_type;

	/*
	 * Do appropriate driver locking.
	 */
	switch (lock_type = (*(my_bdevsw->d_flags) & DLOCK_MASK)) {

	case D_MT:	/* driver is thread-aware */
	case D_MP:	/* driver does own semaphoring */
		break;

	case D_PROCESSOR:	/* force routine onto master processor */
		if (cpuid() != my_bdevsw->d_cpulock)
			DRVLOCK.p_indbdevsw++;
		*was_running = setmustrun(my_bdevsw->d_cpulock);
		ASSERT(cpuid() == my_bdevsw->d_cpulock);
		break;

	default:
		cmn_err_tag(96,CE_PANIC, "illegal driver locking type");
		break;
	}

	return lock_type;
}

/*
 * Common code for block driver unlocking.  Just restore the process
 * to running on the old cpu if it has D_PROCESSOR locking.
 *
 * Lock_type and was_running should be the values returned from
 * a previous call to bdrv_lock().
 */
static void
bdrv_unlock(
	register int	lock_type,
	cpu_cookie_t	was_running)
{
	/*
	 * Do driver unlocking.
	 */
	if (lock_type == D_PROCESSOR) {
		/* reset processor selection */
		restoremustrun(was_running);
	}
}

/*
 * Centralized bdevsw[].d_map routine.
 */
int
bdmap(
	register struct bdevsw *my_bdevsw,
	vhandl_t	*vt,
	off_t		off,
	size_t		len,
	uint_t		prot)
{
	int	retval;
	int	lock_type;
	cpu_cookie_t	was_running;

	DRVLOCK.p_indcdev++;

	lock_type = bdrv_lock(my_bdevsw, &was_running);

	retval = ((int(*)(vhandl_t*,off_t,u_int,u_int))
		  my_bdevsw->d_map)(vt, off, len, prot);

	bdrv_unlock(lock_type, was_running);

	return retval;
}

/*
 * Centralized bdevsw[].d_unmap routine.
 */
int
bdunmap(
	register struct bdevsw *my_bdevsw,
	vhandl_t	*vt)
{
	int	retval;
	int	lock_type;
	cpu_cookie_t	was_running;

	DRVLOCK.p_indcdev++;

	lock_type = bdrv_lock(my_bdevsw, &was_running);

	retval = ((int(*)(vhandl_t*))my_bdevsw->d_unmap)(vt);

	bdrv_unlock(lock_type, was_running);

	return retval;
}

#ifdef MH_R10000_SPECULATION_WAR
static void mh_r10000_check_read_bp(struct buf *);
#endif /* MH_R10000_SPECULATION_WAR */

int
bdrv(register struct bdevsw *my_bdevsw, int routine, ...)
{
	va_list ap;
	void  *a1, *a2, *a3, *a4, *a5;
	typedef int (*bdevfunc_t)(void *, void *, void *, void *, void *);
	register bdevfunc_t func;
	register int lock_type;
	cpu_cookie_t was_running;
	register int retval;

	if (*my_bdevsw->d_flags & D_OBSOLD)
		return ENOTSUP;
	DRVLOCK.p_indbdev++;

	va_start(ap, routine);
	a1 = va_arg(ap, void *);
	a2 = va_arg(ap, void *);
	a3 = va_arg(ap, void *);
	a4 = va_arg(ap, void *);
	a5 = va_arg(ap, void *);

	switch (routine) {
	case DC_DUMP:
		/*
		 * if dumping, no locking/moving
		 */
		return ((bdevfunc_t)my_bdevsw->d_dump)(a1, a2, a3, a4, a5);
	case DC_OPEN:
		func = (bdevfunc_t)my_bdevsw->d_open;
		break;
	case DC_CLOSE:
		func = (bdevfunc_t)my_bdevsw->d_close;
		break;

	case DC_STRAT:
		/*
		 * In the interest of 3rd party driver compatibility,
		 * continue to allow the DC_STRAT case, although we're
		 * more susceptible to being given a bad bdevsw...
		 * Compiled routines will call "bdstrat" directly.
		 */
		return (bdstrat(my_bdevsw, (struct buf *)a1));

	case DC_PRINT:
		func = (bdevfunc_t)my_bdevsw->d_print;
		break;
	case DC_SIZE:
		func = (bdevfunc_t)my_bdevsw->d_size;
		break;
	case DC_SIZE64:
		func = (bdevfunc_t)my_bdevsw->d_size64;
		break;
	default:
		cmn_err_tag(97,CE_PANIC, "bdrv\n");
		break;
	}
	/*
	 * Do appropriate driver locking.
	 */
	lock_type = bdrv_lock(my_bdevsw, &was_running);

	/*
	 * The function call, at last!
	 */
	retval = (*func)(a1, a2, a3, a4, a5);

	/*
	 * Do driver unlocking.
	 */
	bdrv_unlock(lock_type, was_running);

	return retval;
}


/*
 * Block device strategy.
 * Moved out of "bdrv" in order to better protect
 * against being called with bogus (NULL) bdevsw's,
 * as well as making it a little faster.
 */
int
bdstrat(register struct bdevsw *my_bdevsw, register struct buf *bp)
{
	register int		lock_type;
	register int		retval;
		 cpu_cookie_t	was_running;

	ASSERT(my_bdevsw != NULL);

	if (my_bdevsw == NULL) {

		bp->b_flags |= B_ERROR;
		bp->b_error  = ENXIO;

		return ENXIO;
	}

	if (*my_bdevsw->d_flags & D_OBSOLD) {

		bp->b_flags |= B_ERROR;
		bp->b_error  = ENOTSUP;

		return ENOTSUP;
	}

	DRVLOCK.p_indbdev++;

	if (cachewrback) {
		/*
		 * If it is a B_WRITE, and the WBACK flag is NOT specified,
		 * write back the cache before calling strat.
		 */
		if (   ! (bp->b_flags & B_READ)
		    && ! (*(my_bdevsw->d_flags) & D_WBACK) ) {

			(void)bp_dcache_wbinval(bp);
		}
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000() && (bp->b_flags & B_READ))
		mh_r10000_check_read_bp(bp);
#endif /* MH_R10000_SPECULATION_WAR */

	/*
	 * Do appropriate driver locking.
	 */
	lock_type = bdrv_lock(my_bdevsw, &was_running);

	retval = (*my_bdevsw->d_strategy)(bp);

	/*
	 * Do driver unlocking.
	 */
	bdrv_unlock(lock_type, was_running);

#ifdef	SN0
	sn0_poq_war((void *)bp);
#endif	/* SN0 */
		
	return retval;
}

/*
 * ACTION:
 *   Routines to allow you to queue an action for another processor to handle
 * This queue is run at the top of disp.
 *
 *   Routines are :
 *	cpuaction()   -- call to request another cpu to do action
 *	actioninit()  -- initializes the action freelist (actually a table)
 *	allocaction() -- get next action_t from free list
 *	freeaction()  -- returns action_t to freelist
 */
/* for cross-cpu actions */
extern action_t *actionlist;
extern int nactions;

/* pda_actioninit()
 *	initializes the table of free action_t 's .
 *	It is called in mktables(), since we call doactions() out of clock().
 */
static void
pda_actioninit(int i)
{
#ifdef MP
	action_t *act;

	initnlock(&pdaindr[i].pda->p_actionlist.actionlock, "actlck");

	/* First we queue up the local action blocks */

	pdaindr[i].pda->p_actionlist.firstaction =
		&pdaindr[i].pda->p_actionlist.act_blocks[0];

	pdaindr[i].pda->p_actionlist.act_blocks[0].donext =
		&pdaindr[i].pda->p_actionlist.act_blocks[1];

	pdaindr[i].pda->p_actionlist.act_blocks[1].donext = NULL;

	/*
	 * If action list has been preallocated, link them in now.
	 */
	if (actionlist) {
		pdaindr[i].pda->p_actionlist.act_blocks[1].donext =
			&actionlist[i * nactions];
	
		act = &actionlist[i * nactions];
		for (i = 0; i < (nactions-1); act++, i++) 
			act->donext = act+1;
		act->donext = NULL;
	}
#endif
} 

/* pda_actioninit_ext()
 * called during cpu init to allocate additional action buffers & link them
 * to the local pda action list. 
 */
void
pda_actioninit_ext(void)
{
#ifdef MP
	action_t	*act, **free_actp;
	int		s, i;

	/*
	 * If the action list was not preallocated, allocate it now for
	 * this cpu & link it to the end of the free action blocks.
	 *	NOTE: The original implementation always preallocated
	 *	the actionlist. However, this doesnt work on large SN0
	 *	systems (overflows low memory). Rather than test all 
	 *	platforms, deferrring allocation is only done on SN 
	 *	systems. It should work on other platforms but needs
	 *	to be tested.
	 */
	if (!actionlist) {
		act = kmem_zalloc(nactions * sizeof(action_t), KM_NOSLEEP);

		s = splhi();
		free_actp = &private.p_actionlist.firstaction;
		while (*free_actp)
			free_actp = &((*free_actp)->donext);
		*free_actp = act;
		for (i = 0; i < (nactions-1); act++, i++) 
			act->donext = act+1;
		act->donext = NULL;
		splx(s);
	}
#endif
} 

void
actioninit(void)
{
	pda_actioninit(master_procid);
} 

void
doactions(void)
{
	register action_t *curact;
	lock_t *actlock;
#if CHECK_SPLHI_TIME
	long long t1;
#endif 

#ifdef MP
	ASSERT(issplhi(getsr()));

	actlock = &private.p_actionlist.actionlock;
	/* 
	** must call the action routine at splhi()
	** must have the lock, since the list can be modified
	** by other cpu as well
	*/
	while (private.p_actionlist.todolist != NULL) {

		cpuacfunc_t dofunc;
		void *a0, *a1, *a2, *a3;

		nested_spinlock(actlock);
		curact = private.p_actionlist.todolist;
		private.p_actionlist.todolist = curact->donext;

		dofunc = curact->dofunc;
		a0 = curact->doarg0;
		a1 = curact->doarg1;
		a2 = curact->doarg2;
		a3 = curact->doarg3;

		/* actionfree() */
		curact->donext = private.p_actionlist.firstaction;
		private.p_actionlist.firstaction = curact;

		nested_spinunlock(actlock);

		START_SPLHI_TIMER(t1);

		(*dofunc)(a0,a1,a2,a3);


		END_SPLHI_TIMER(t1);
#if CHECK_SPLHI_TIME
		if (t1 > 1000)
			cmn_err(CE_WARN, "doactions: splhi for %d microsec 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			t1,dofunc, a0, a1, a2, a3);
#endif
	}
#endif /* MP */
}


/*
 * cpuaction:
 *	This nifty little number lets you queue a function to be run
 *	on the cpu of your choice. The queue of actions is kept in 
 *	each pda, and serviced at the top of disp().
 * 	Actions are taken in the order they were queued.
 * 	NOTE : cpuaction() should not be used on CPUs that 
 * 	are inactive, or not yet initialized.
 *
 * nested_cpuaction:
 *	Optimized version of cpuaction which can be efficiently utilized
 *	in loops where we're already at splhi() and can pass in the
 *	parameters rather than using va_arg() processing.
 *
 */
void
nested_cpuaction(cpuid_t who, cpuacfunc_t what,
	      void *p0, void *p1, void *p2, void *p3)
{
#ifdef MP
	register action_t *newact;
	actionlist_t *alist;

	ASSERT(issplhi(getsr()));
	ASSERT(pdaindr[who].CpuId != private.p_cpuid);
#ifdef ISOLATE_DEBUG
	if (pdaindr[who].pda->p_flags & PDAF_ISOLATED && isolate_debug) {
		cmn_err(CE_WARN,
		"Isolated processor %d executes function 0x%x\n",
			who, what);
		if (pdaindr[who].pda->p_flags & PDAF_BROADCAST_OFF)
			cmn_err(CE_WARN, "Broadcast is off\n");
		if (isolate_drop)
			debug(0);
	}
#endif
		
	/* Get next free action_t structure */

	ASSERT(global_panic_count || who < maxcpus &&  pdaindr[who].CpuId != -1);

	alist = &pdaindr[who].pda->p_actionlist;
	nested_spinlock(&alist->actionlock);

	/* This "if" coded this way to get normal path to be in-line code */
	 
	if (newact = alist->firstaction) {
		alist->firstaction = newact->donext;

		newact->donext = NULL;	/* fill structure */
		newact->dofunc = what;
		newact->doarg0 = p0;
		newact->doarg1 = p1;
		newact->doarg2 = p2;
		newact->doarg3 = p3;

		if (alist->todolist == NULL)
			alist->todolist = newact;
		else
			alist->lasttodo->donext = newact;

		alist->lasttodo = newact;
		nested_spinunlock(&alist->actionlock);
		sendintr(who, DOACTION);
		return;
	} 
	nested_spinunlock(&alist->actionlock);
	cmn_err_tag(98,CE_PANIC, "Ran out of action blocks");
#else /* !MP */
	cmn_err_tag(99,CE_PANIC, "Uniprocessor kernel invoked nested_cpuaction\n");
#endif /* !MP */
}

void
cpuaction(cpuid_t who, cpuacfunc_t what, int flags, ...)
{
	va_list ap;
	caddr_t a1, a2, a3, a4;
	register int s;
#if CHECK_SPLHI_TIME
	long long t1;
#endif /* XYZZY */

	va_start(ap, flags);
	s = splhi();
	a1 = va_arg(ap, caddr_t);
	a2 = va_arg(ap, caddr_t);
	a3 = va_arg(ap, caddr_t);
	a4 = va_arg(ap, caddr_t);
	va_end(ap);

	if (pdaindr[who].CpuId != private.p_cpuid) {
		/* no one should be using anything other than A_NOW */
		ASSERT(flags & A_NOW);

		nested_cpuaction(who, what, a1, a2, a3, a4);
		splx(s);
		return;
	} 
	START_SPLHI_TIMER(t1);

	(*what)(a1, a2, a3, a4);

	END_SPLHI_TIMER(t1);
#if CHECK_SPLHI_TIME
	if (t1 > 1000)
		cmn_err(CE_WARN, "cpuaction: splhi for %d microsec 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			t1, what, a1, a2, a3, a4);
#endif
	splx(s);
}

/*
 * Simplified version of cpuaction utility - Just set a bit representing
 * an op-code in the pda of the cpu, then send an intr. That cpu loops
 * through the vector, calling the routines indicated by the set bits.
 * Provided to prevent running out of action blocks.
 */

#define ispow2(x)	(((x) & ((x) - 1)) == 0)

extern void tlbflush_rand(void);
extern void tlbflush(uint64_t asgen);
extern void panicspin(void);
extern void stopspin(void);

#ifdef OBSOLETE
/* Only have a few bits defined so code is faster with explicit checks */
void (*acfunc[])(void) = {
	panicspin,	/* 0x01  VA_PANICSPIN */
	NULL,
	force_resched,	/* 0x04  VA_FORCE_RESCHED */
	tlbflush_rand,	/* 0x08  VA_TLBFLUSH_RAND */
};
#endif /* OBSOLETE */

#if defined(DEBUG) || defined(ISOLATE_DEBUG)

void
cpuvaction(register cpuid_t who, register uint opcode)
{

	ASSERT(who < MAX_NUMBER_OF_CPUS() && pdaindr[who].CpuId != CPU_NONE);

#ifdef ISOLATE_DEBUG
	if (pdaindr[who].pda->p_flags & PDAF_ISOLATED && isolate_debug) {
		cmn_err(CE_WARN,
			"Isolated processor executes vaction opcode 0x%x\n",
			(int)opcode);
		if (isolate_drop)
			debug((char *)0);
	}
#endif
	/* This DEBUG code is ineffecient but the non-DEBUG code is
	 * "in-line" and fast and does not require atomic update (could
	 * use array except that causes addressing problems on R8000).
	 */
	if (opcode == VA_PANICSPIN)
		pdaindr[who].pda->p_va_panicspin = 1;
	else if (opcode == VA_FORCE_RESCHED)
		pdaindr[who].pda->p_va_force_resched = 1;
#ifdef MC3_CFG_READ_WAR
	else if (opcode == VA_FORCE_STOP)
		pdaindr[who].pda->p_va_force_stop = 1;
#endif
	else
		cmn_err(CE_PANIC, "illegal cpuvaction: code 0x%x\n", opcode);

	if (pdaindr[who].CpuId == private.p_cpuid)
		doacvec_splhi();
	else
		sendintr(who, DOACTION);
}
#endif /* DEBUG || ISOLATE_DEBUG */

void
doacvec_splhi()
{
	register int s;

	s = splhi();
	doacvec();
	splx(s);
}

#ifdef EVEREST
extern cpumask_t tlbflushmask;
#endif /* EVEREST */

void
doacvec(void)
{
	uint64_t asgen;

	/* 
	** must call the action routine at splhi()
	** must have the lock, since the list can be modified
	** by other cpu as well
	*/

	ASSERT(issplhi(getsr()));

	/* If we're panicing, no real need to clear the bit */
	if (private.p_va_panicspin)
		panicspin();

	if (private.p_va_force_resched) {
		private.p_va_force_resched = 0;
		private.p_runrun = 1;
	}

	/* We no longer set VA_TLBFLUSH_RAND, just check tlbflush_cnt */

#ifdef EVEREST
	if ((private.p_tlbflush_cnt) || CPUMASK_IS_NONZERO(tlbflushmask))
#else
	if (private.p_tlbflush_cnt)
#endif
		tlbflush_rand();

	if (asgen = private.p_tlbsync_asgen) {
		private.p_tlbsync_asgen = 0;	/* let another request in */
		tlbflush( asgen );
	}

#ifdef MC3_CFG_READ_WAR
	if (private.p_va_force_stop) {
		private.p_va_force_stop = 0;
		stopspin();
	}
#endif
#if IP19
	/* Ideally we would only invoke this check if we found no valid
	 * reason for this interrupt (like we do in the 6.2 patch).
	 * But for now this is safe and we don't want to miss cache error
	 * panics.
	 */
	{
	extern void doacvec_check_ecc_logs(void);

	doacvec_check_ecc_logs();
	}
#endif /* IP19 */

}

int     panic_cpu = -1;           /* reset by first cpu in panic_lock */
int     global_panic_count = 0;   /* global panic interlock word */
int     nested_panic_count = 0;	  /* track nested calls to enter_panic_mode */
ktimersnap_t panic_snap;	  /* time panic occurred */

void
panic_lock(void)
{
	int	i;
        /*
         * Use global_panic_count to allow only 1 cpu to proceed
         * (allow double panics on same cpu)
         */
        if (1 != atomicAddInt(&global_panic_count,1)) {
                /*
                 * Some other cpu has set the lock
                 */
                if  (panic_cpu != getcpuid()) {
                        panicspin();
                }
        }

        panic_cpu = getcpuid();
        ++nested_panic_count;
}

void
panic_unlock(void)
{
        if (panic_cpu == getcpuid()) {
                if (--nested_panic_count == 0) {
                        panic_cpu = -1;
                        global_panic_count = 0;
                }
        }
}


void
enter_panic_mode(void)
{
	cpuid_t cpu;

        panic_lock();
	private.p_panicking = 1;
	/*
	 * Enter one last UTRACE, then turn them off on all CPUs so
	 * traces generated while dumping don't fill the buffer
	 */
	UTRACE(RTMON_ALL, UTN('PANI','Cmde'), private.p_tstamp_mask, 0);
	for (cpu = 0; cpu < maxcpus; cpu++)
		if (pdaindr[cpu].CpuId != -1)
			pdaindr[cpu].pda->p_tstamp_mask = 0;

	/*
	 * Save time of panic
	 */
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
	panic_snap = GET_LOCAL_RTC;
#else
	panic_snap.secs = time;
	panic_snap.rtc = get_timestamp();
#endif

}

void
exit_panic_mode(void)
{
        panic_unlock();
	private.p_panicking = 0;

	/* Don't bother resetting the tstamp mask.  exit_panic_mode()
	 * is only called when we're about to panic but need to unlock
	 * temporarily, and from from _assfail, where the hacker is on
	 * their own.  Correctly resetting the mask would require
	 * taking a tstamp lock, which could hang.
	 */
}

int
in_panic_mode(void)
{
	return (private.p_panicking);
}
/* Following routines are useful to prevent situations
 * where we want to turn off promlogging temporarily
 * and turn it back on again a bit later.
 */

/* Go into promlog mode */
void
enter_promlog_mode(void)
{
	private.p_promlogging = 1;
}
/* Come out of promlog mode */
void
exit_promlog_mode(void)
{
	private.p_promlogging = 0;
}

/* Check whether we are in promlog mode */
int
in_promlog_mode(void)
{
	return (private.p_promlogging);
}

void
enter_dump_mode(void)
{
	if (private.p_panicking == 1)
	    private.p_panicking = 2;
}

int
in_dump_mode(void)
{
	return (private.p_panicking == 2);
}

void
exit_dump_mode(void)
{
	if (private.p_panicking == 2)
	    private.p_panicking = 1;
}

#ifdef MH_R10000_SPECULATION_WAR
static void
mh_r10000_check_read_bp(struct buf *bp)
{
        size_t remaining = bp->b_bcount;
        int offset = poff(bp->b_un.b_addr);
        caddr_t baddr;
#ifdef MH_R10000_DMA_CHECK
        caddr_t base_baddr;
        pgno_t pagenum;
#endif /* MH_R10000_DMA_CHECK */
        int curlen;
        int pde_counts[3];
        pfd_t *pfd;
        pde_t *pd;

        /* check that all pages are protected */
        if (bp->b_flags & B_PAGEIO) {
                pfd = NULL;
                while (remaining > 0) {
                        pfd = getnextpg(bp, pfd);
#ifdef MH_R10000_DMA_CHECK
                        if (pfd < pfd_lowmem ||
                            pfd->pf_flags & P_NO_DMA) {
                                pagenum = pfdat_to_eccpfn(pfd);
                        do_dma_panic:
                                cmn_err_tag(100,CE_PANIC,"DMA read to non-DMA page 0x%x in buffer at 0x%x",pagenum,bp);
                        }
#endif /* MH_R10000_DMA_CHECK */
                        curlen = NBPP - offset;
                        if (curlen > remaining)
                                break;
                        remaining -= curlen;
                        offset = 0;
                }
                return;
        }

        if (bp->b_flags & B_MAPPED)
                baddr = bp->b_dmaaddr;
        else
                baddr = bp->b_un.b_addr;
        ASSERT(baddr != NULL);

#ifdef MH_R10000_DMA_CHECK
        base_baddr = baddr;
        while (remaining > 0) {
                if (IS_KSEGDM(baddr))
                        pagenum = pnum(KDM_TO_PHYS(baddr));
                else if (iskvir(baddr))
                        pagenum = pfn_to_eccpfn(pdetopfn(kvtokptbl(baddr)));
                else
                        goto next_kdm_page;
                if (pagenum <= SMALLMEM_K0_R10000_PFNMAX ||
                    (is_in_pfdat(pagenum) &&
                     pfdat[pagenum].pf_flags & P_NO_DMA))
                        goto do_dma_panic;

        next_kdm_page:
                curlen = NBPP - offset;
                if (curlen > remaining)
                        break;
                remaining -= curlen;
                baddr += curlen;
                offset = 0;
        }
        remaining = bp->b_bcount;
        offset = poff(bp->b_un.b_addr);
        baddr = base_baddr;
#endif /* MH_R10000_DMA_CHECK */

        if (! iskvir(baddr))
                return;

        pde_counts[2] = 0;
        while (remaining > 0) {
                pd = kvtokptbl(baddr);
                if (pd->pte.pg_vr) {
                        invalidate_range_references((void *) baddr,
                                                    remaining,
                                                    CACH_DCACHE|CACH_SCACHE|CACH_WBACK|
                                                    CACH_INVAL|CACH_IO_COHERENCY,
                                                    0);
                        break;
                }
                pfd = pdetopfdat(pd);

                curlen = NBPP - offset;
                if (curlen > remaining)
                        curlen = remaining;
                pde_counts[0] = 0;
                pde_counts[1] = 0;
                krpf_visit_references(pfd, invalidate_kptbl_entry,
			pde_counts);
                if (pde_counts[1] > 0)
                        clean_dcache((void *) poff(baddr),
                                     curlen,
                                     pdetopfn(pd),
                                     CACH_DCACHE|CACH_SCACHE|CACH_WBACK|
                                     CACH_INVAL|CACH_IO_COHERENCY|
                                     CACH_NOADDR);
                remaining -= curlen;
                baddr += curlen;
                offset = 0;
        }
}
#endif /* MH_R10000_SPECULATION_WAR */

#ifdef MC3_CFG_READ_WAR
void
special_doacvec()
{
	int s;

	if (private.p_va_force_stop) {
		private.p_va_force_stop = 0;
		s = splhi();
		stopspin();
		splx(s);
	}
	else {
		us_delay(100);
	} 
}
#endif
