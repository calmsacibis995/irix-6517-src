/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Misc support routines for multiprocessor operations.
 */

#ident "$Revision: 1.135 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/hwgraph.h>
#include <sys/immu.h>
#include <sys/iograph.h>
#include <sys/map.h>
#include <sys/dmamap.h>
#include <sys/proc.h>	/* shaddr_t for sys/runq.h */
#include <sys/runq.h>
#include <sys/sbd.h>
#include <sys/sysinfo.h>
#include <sys/uadmin.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/invent.h>
#include <sys/runq.h>
#include <sys/cpumask.h>
#include <sys/kopt.h>
#include <bsd/sys/tcpipstats.h>
#include <sys/strstat.h>
#include <sys/SN/agent.h>
#include <sys/SN/nmi.h>
#include <sys/SN/kldir.h>
#include <sys/SN/launch.h>
#include <sys/SN/module.h>
#include "sn_private.h"
#if defined (SN0)
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/SN0/ip27log.h>
#endif /*SN0 */
#include <sys/clksupport.h>	/* init_timebase */
#if TRAPLOG_DEBUG
#include <sys/traplog.h>
#endif
#include "os/proc/pproc_private.h"
#if CELL
#include <ksys/cell.h>
#endif

extern void reset_leds(void);
extern void init_module_info(void);

extern struct lf_listvec *str_init_alloc_lfvec(cpuid_t);
extern void str_init_slave(struct lf_listvec *, cpuid_t, cnodeid_t);
extern struct strstat *str_init_alloc_strstat(cpuid_t);
extern pda_actioninit_ext(void);

/* Forward declaration */
static void cpu_io_setup(void);
extern void ecc_init(void);

extern void sn0_error_dump(char *);

/*
 * For communication between sched and the new processor:
 * the new processor raises and waits to go low
 */
static volatile int	cb_wait = 0;

#ifdef SABLE

/*
 * In sable simulations, writing a nonzero value here will launch all
 * slaves into bootstrap.
 */
volatile int slave_release = 0;
volatile int sable_mastercounter = 0;

#endif

/*
 * Number of CPUs running when the NMI hit.
 */
int	nmi_maxcpus = 0;

/*
 * _Number_ of CPUs present = maxcpus - disabled CPUs
 */
int	num_cpus = 0;

/*
 * CPU mask for a barrier.
 */
static volatile cpumask_t boot_barrier;

/*
 * Has the system been NMIed ?
 */
int	nmied = 0;
cpumask_t nmied_cpus;			/* Indicates if some cpus were NMIed by the kernel */

int	pokeboot = 1;
#ifdef SABLE
#define BOOTTIMEOUT	2000		/* A short time */
#else
#define BOOTTIMEOUT	1000000		/* 1 sec */
#endif

/*
 * mlreset uses this to determine if a CPU has really started.
 */
extern char slave_loop_ready[MAXCPUS];

#ifdef SABLE
void
sdintr(eframe_t *ep, void *arg);
#endif


/*
 * dobootduty - called by allowboot
 */
static void
dobootduty(void)
{
	extern int intstacksize;
	register pda_t *pda;
	cnodeid_t node;

	/* cb_wait = cpunum + 1 */
	ASSERT_ALWAYS(cb_wait > 0 && cb_wait <= maxcpus);
	pda = pdaindr[cb_wait - 1].pda;
	node = pda->p_nodeid;

	/*
	 * grab memory for interrupt stack
	 */
	pda->p_intstack = kvpalloc_node(node, btoc(intstacksize),VM_DIRECT|VM_NOSLEEP,0);
	ASSERT(pda->p_intstack);
	pda->p_intlastframe =
		&pda->p_intstack[(intstacksize-EF_SIZE)/sizeof(k_machreg_t)];
	/*
	 * Allocate storage for this cpu for the following kernel areas:
	 * ksaptr => Kernel system activity statistics
	 * knaptr => Kernel network activity statistics
	 * kstr_lfvecp => Kernel Streams buffer list vectors
	 * kstr_statsp => Kernel Streams activity statistics
	 * NOTE: We assume that kern_calloc returns a zero'ed out area
	 */
	pda->ksaptr=(struct ksa *)kern_calloc_node(1, sizeof(struct ksa),node);
	if (pda->ksaptr == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: pda->ksaptr NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Initialize tcp/ip statistics area for this cpu
	 */
	pda->knaptr = (char *)kern_calloc_node(1, sizeof(struct kna), node);
	if (pda->knaptr == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: pda->knaptr NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Initialize streams buffers for this cpu
	 */
	pda->kstr_lfvecp = (char *)str_init_alloc_lfvec(pda->p_cpuid);
	if (pda->kstr_lfvecp == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: kstr_lfvecp NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * Allocate an initialize streams statistics block for this cpu
	 */
	pda->kstr_statsp = (char *)str_init_alloc_strstat(pda->p_cpuid);
	if (pda->kstr_statsp == 0) { /* failed */
		cmn_err(CE_PANIC,
			"dobootduty: kstr_statsp NULL; pda 0x%x, cpu_id %d\n",
			pda, pda->p_cpuid);
	}
	/*
	 * kstr_lfvecp MUST be set properly prior to init the slave processor!
	 */
	str_init_slave((struct lf_listvec *)pda->kstr_lfvecp, pda->p_cpuid,
		node);

	/* all done - requestor can proceed */
	cb_wait = 0;
	return;
}

/*
 * One-time setup for MP SN0.
 * Allocate per-node and per-cpu data, slurp prom klconfig information and
 * convert it to hwgraph information.
 */
static void
sn0_mp_setup(void)
{
	cnodeid_t	cnode;
	cpuid_t		cpu;

	/*
	 * Before we let the other processors run, set up the platform specific
	 * stuff in the nodepda.
	 */
	for (cnode = 0; cnode < maxnodes; cnode++) {
		/* Set up platform-dependent nodepda fields. */
		init_platform_nodepda(Nodepdaindr[cnode], cnode);

                /* Clear out our interrupt registers. */
                intr_clear_all(COMPACT_TO_NASID_NODEID(cnode));
	}

	/*
	 * Now set up platform specific stuff in the processor PDA.
	 */
	for (cpu = 0; cpu < maxcpus; cpu++) {
		/* Skip holes in CPU space */
		if (CPUMASK_TSTB(boot_cpumask, cpu)) {
			init_platform_pda(pdaindr[cpu].pda, cpu);
		}
	}

	/*
	 * Initialize platform-dependent vertices in the hwgraph:
	 *	module
	 *	node
	 *	cpu
	 *	memory
	 *	slot
	 *	hub
	 *	router
	 *	xbow
	 */

	module_init();

	klhwg_add_all_modules(hwgraph_root);
	klhwg_add_all_nodes(hwgraph_root);

#if 0
	/*
	 * temporary disable 
	 */
	{
	extern int  cerr_flags;
	extern void cache_err_init(void);

	if (cerr_flags & 0x0001) 
		cache_err_init();
	}
#endif
}

int
is_prom_downrev(cpuid_t cpu)
{
	int slice;
	hubreg_t err_status0;
	slice = cputoslice(cpu);
	err_status0 = REMOTE_HUB_L(cputonasid(cpu), slice?PI_ERR_STATUS0_B
			       :PI_ERR_STATUS0_A);
	/*
	 * Get doubleword address from PI_ERR_STATUS0 register. Compare it with
	 * ERR_STS_WAR_PHYSADDR (shifted into a doubleword address).
	 * If they're not equal, we have an old prom.
	 * If the valid bit is clear, we also have an old prom.
	 */
	if ((((err_status0 & PI_ERR_ST0_ADDR_MASK) >> PI_ERR_ST0_ADDR_SHFT) != 
	      (ERR_STS_WAR_PHYSADDR >> 3))
	    || !(err_status0 & PI_ERR_ST0_VALID_MASK)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "CPU %d has a downrev prom.", cpu);
#endif
		return 1;
	} else {
		return 0;
	}
}


extern int r10k_intervene;
extern int r10k_progress_nmi;
extern int mem_mixed_hidensity_banks(void);
int	force_fire_and_forget = 0;

/*
 * Called when the master processor is satisfied that the system is sane
 * enough to come up with multiple CPU's.
 */
void
allowboot(void)
{
	cpuid_t		cpu;
	int		badcpu_count = 0;
	int		timeout;
	int		cpucount;
	int		real_maxcpus = 1;
	cnodeid_t	cnode;
	extern volatile int ignore_conveyor_override;
	
        /*
         * machine dependent initialization that required more resources
         * than at mlreset() level can be done here.
         */
#if defined (SN0)
	if (IP27CONFIG.freq_rtc != IP27C_KHZ(IP27_RTC_FREQ)) 
	    cmn_err(CE_PANIC, 
		    "RTC frequency incorrect. Please upgrade your proms");
#endif /* SN0 */	    

#if defined (SN0)
	/*
	 * ml/clksupport.c has code for ORIGIN that checks for cpus not
	 * making progress.... when debugger is installed, always disable
	 * the checking code.
	 */
	if (kdebug && r10k_intervene) {
		r10k_intervene = 0;
	}
#endif /* SN0 */

	init_timebase();		/* do early for best accuracy */

	if (showconfig) {
#if defined (SN0)
#if defined (SN0_USE_BTE)
		cmn_err(CE_NOTE, "The BTE is enabled.");
#else
		cmn_err(CE_NOTE, "The BTE is disabled.");
#endif
#endif
		cmn_err(CE_CONT, "Master processor is 0x%x\n", master_procid);
		cmn_err(CE_CONT, "Master node is NASID 0x%x\n",
			COMPACT_TO_NASID_NODEID(cputocnode(master_procid)));
	}

#ifdef DEBUG
	/*
	 * On debug kernels, do this early.  Otherwise, they don't get installed
	 * 'til driver init time.
	 */
	install_klidbg_functions();
#endif
	/*
	 * Check HUB_ERR_STS_WAR.  Need to do this first because
	 * hub_error_init clears the regsiters causing a race.
	 */
	for (cpu = 0; cpu < maxcpus; cpu++) {
		/* Skip holes in CPU space */
		if (CPUMASK_TSTB(boot_cpumask, cpu)) {
			/* Since the hub PI chip drops WRBs on write
			 * errors when the PI_ERR_STATUS[01]_[AB]
			 * register is clear, and since dropping WRBs
			 * messes up the PIO conveyor belt, we must
			 * force an error into the ERR_STATUS register
			 * of every CPU in the prom in case the prom					 * hits a write error.  If the prom hasn't done
			 * this, we have an old version and we can't
			 * run with the PIO conveyor belt enabled.
			 */
			if (is_prom_downrev(cpu))
				force_fire_and_forget = 1;
		}
	}
		
	sn0_mp_setup();

	/* Run code that must be run by each CPU. */
	per_cpu_init();

#if defined (SN0)
	/* 
	 * bte_lateinit should be after calling per_cpu_init() since
	 * the interrupt vectors have to be setup and is done in 
	 * per_cpu_init()
	 */
	bte_lateinit();
#endif /* SN0 */

#ifdef SABLE
	{
		if (intr_reserve_level(master_procid, SDISK_INTR, 0, 0, "Sable Disk") != SDISK_INTR)
#if defined (NOUART_SABLE)
			cmn_err(CE_WARN,
				"Couldn't reserve sabledsk interrupt!");

#else
			cmn_err(CE_PANIC,
				"Couldn't reserve sabledsk interrupt!");
#endif
		intr_connect_level(master_procid, SDISK_INTR, 0,
				   (intr_func_t)sdintr, NULL, NULL);
	}
 
#endif

#ifdef MAPPED_KERNEL
	/* Replicate the kernel text now. */
	replicate_kernel_text(numnodes);
#endif

	/* allocate the rest of the action blocks */
	pda_actioninit_ext();

	ecc_init();

	/* Let other processors come in. */
#ifdef SABLE

	if (fake_prom) {

		/* Give slaves a chance to increment sable_mastercounter. */
		{
			volatile int i;
			for (i = 0; i < 400; i++)
				i++;
		}
	
		/* On SABLE this will release all slaves into the kernel. */
		slave_release = 1;
		num_cpus = real_maxcpus = sable_mastercounter;
	} else
#endif /* SABLE */
	{
#ifdef SABLE_SYMMON
		/*
		 * fake_prom == 1 means kernel is executing solo under SABLE
		 *		and need to have special slave launch code
		 *		which is symmon specific.
		 *
		 * fake_prom == 0  AND (SABLE_SYMMON_INITPC+4) != 0
		 *		means that the kernel & symmon are executing
		 *		without prom code under SABLE and symmon
		 *		will handle slave_launch through special
		 *		entry points.
		 * If symmon is present, we call symmon entry point to launch
		 * slaves
		 */
		if ((*(int *)(SABLE_SYMMON_INITPC+4)) != 0) {
			for (cpu = 0; cpu < maxcpus; cpu++) {
				if (cpu == cpuid()) {
					num_cpus++;
					continue;
				}
				/* Skip holes in CPU space */
				if (CPUMASK_TSTB(boot_cpumask, cpu)) {
					num_cpus++;
					sable_symmon_slave_launch( cpu );
				}
			}
			cmn_err(CE_CONT,"tried to launch %d slave cpus\n", num_cpus-1);
		} else
#endif /* SABLE_SYMMON */

		/* Set all CPUs' bits in the barrier. */
		boot_barrier = boot_cpumask;

		/* Launch slaves. */
#if DEBUG
		dprintf("Starting remaining CPUS\n");
#endif
		for (cpu = 0; cpu < maxcpus; cpu++) {

			if (cpu == cpuid()) {
				num_cpus++;
				/* We're already started, clear our bit */ 
				CPUMASK_CLRB(boot_barrier, cpu);
				continue;
			}

			/* Skip holes in CPU space */
			if (CPUMASK_TSTB(boot_cpumask, cpu)) {
				num_cpus++;

				/*
				 * Launch a slave into bootstrap().
				 * It doesn't take an argument, and we'll
				 * take care of sp and gp when we get there.
				 */
				LAUNCH_SLAVE(cputonasid(cpu), cputoslice(cpu),
					     (launch_proc_t)
						MAPPED_KERN_RO_TO_K0(bootstrap),
					      0,
					      pdaindr[cpu].pda->
						p_bootlastframe,
					      0); /* Don't need to set the gp*/
			}
		}
		real_maxcpus = maxcpus;
	}


	/*
	** Now wait for a couple seconds. That should leave enough time
	** for other processors to start.  But put a cap on the delay.
	*/
	timeout = BOOTTIMEOUT * (num_cpus < 40 ? num_cpus : 40);
	cpucount = num_cpus - 1;
	while (--timeout && cpucount)  {
		if (cb_wait) {
			dobootduty();
			cpucount--;
		}
		DELAY(1);		/* 1 us */
	}

	timeout = BOOTTIMEOUT * (num_cpus < 40 ? num_cpus : 40);
	while (--timeout && CPUMASK_IS_NONZERO(boot_barrier))  {
		DELAY(1);		/* 1 us */
	}

#ifndef SABLE
	/*
	 *  Do a quick check to see that the other processors started up.
	 */
	for (cpu = 0; cpu < maxcpus; cpu++) {
	    if (cpu_enabled(cpu)  &&  pdaindr[cpu].CpuId != cpu) {
		cmn_err(CE_NOTE, "Processor #%d did not start!", cpu);
		pdaindr[cpu].CpuId = -1;
		/*
		 * XXX - Everest does this, but we just panic anyway.
		 * If we ever make cpu_unenable() write to NVRAM, this would
		 * be worthwhile.
		 */
		cpu_unenable(cpu);
		badcpu_count++;
	    }
	}
#else
	badcpu_count = cpucount;
#endif
	if (badcpu_count)
#if defined(SABLE)
		cmn_err(CE_WARN, "%d CPUs missing, but continuing boot\n",badcpu_count);
#else
		cmn_err(CE_PANIC, "Cannot start without all CPUs\n");
#endif

	/*
	 *  If high-numbered processors have been disabled, then reset
	 *  "maxcpus" to indicate the last processor which must be
	 *  considered by the kernel from now on.
	 */
	maxcpus = real_maxcpus;

	cpu_io_setup();

	for (cnode = 0; cnode < maxnodes; cnode++) {
		if (cnodetocpu(cnode) == -1) {
			cmn_err(CE_NOTE,"Initializing headless hub,cnode %d",
				cnode);
			per_hub_init(cnode);
		}
	}

#ifdef HUB_ERR_STS_WAR
	if (force_fire_and_forget) {
		cmn_err(CE_WARN, "Some CPUs have old firmware.  "
				 "Please update node board flash proms.");
#ifdef DEBUG
	} else {
		cmn_err(CE_NOTE, "PIO conveyor belt is enabled.");
#endif /* DEBUG */
	}

#else /* HUB_ERR_STS_WAR */
	/*
	 * If the workaround isn't enabled, we can't use the PIO conveyor belt.
	 */
	force_fire_and_forget = 1;
#endif /* HUB_ERR_STS_WAR */


	init_mfhi_war();

#if defined(SN0)
	{
#pragma weak		cache_debug_init
	extern void     cache_debug_init (void);

	  if (cache_debug_init) {
		cache_debug_init();
	  }
	}
#endif
}

void
disallowboot(void)
{
	/* XXX - Don't have this yet. */
}

/*
 * Boot routine called from assembly as soon as the bootstrap routine
 * has context set up.
 *
 * Interrupts are disabled and we're on the boot stack (which is in pda)
 * The pda is already accessible.
 */
void
cboot()
{
	extern int utlbmiss[];
	extern lock_t bootlck;
	register cpuid_t id = getcpuid();
	register int s0;

	wirepda(pdaindr[id].pda);

	private.p_cpuid = id;
	private.p_kstackflag = PDA_CURIDLSTK;
	private.p_scachesize = getcachesz(id);

	CPUMASK_CLRALL(private.p_cpumask);
	CPUMASK_SETB(private.p_cpumask, id);

	private.common_excnorm = get_except_norm(); /* plug exception handler vecs in */

	ASSERT(id < maxcpus);

	mlreset(1);

	/* Must call _hook_exceptions on _all_ CPUs these days. */
	_hook_exceptions();

	/* Run code that must be run by all CPUs. */
	per_cpu_init();

	private.p_kvfault = 0;

	cache_preempt_limit();

	/* each cpu might run at a different speed */
	private.cpufreq = findcpufreq();

	/* Find out what kind of CPU/FPU we have. */
	coproc_find();

	/* single thread booting from this point */
	s0 = splock(bootlck);

	/*
	 * we are not allowed to do anything that might
	 * sleep or cause an interrupt,
	 * so we need to ask a process to get
	 * memory for us - our interrupt stack and
	 * ksa struct
	 * we of course spin waiting
	 * this implies that only 1 process can boot at once
	 * We can't even do a cvsema since that lowers the spl
	 * we let clock help us out
	 * We add one to cb_wait to allow CPU 0 to be a slave processor
	 * if necessary.  The master need not be CPU 0.
	 */
	cb_wait = id + 1;
	while (cb_wait) {
#if 0
		register int wait = 0;

		if (wait++ > 40000) {
			if (kdebug == 1) {
				prom_printf(".");
			}
			wait = 0;
		}
		du_conpoll();
		;
#endif
	}

	/* now safe to claim we're enabled */
	private.p_flags |= (PDAF_ENABLED | PDAF_ISOLATED);
	numcpus++;
	CPUMASK_SETB(maskcpus, id);
	pdaindr[id].CpuId = id;

	CPUMASK_ATOMCLRB(boot_barrier, private.p_cpuid);

	/* allocate the rest of the action blocks */
	pda_actioninit_ext();

	ecc_init();
	/* do initialization required for each processor */
	stopclocks();

	reset_leds();

	spinlock_init(&pdaindr[id].pda->p_special, "pdaS");

	tlbinfo_init();

#if defined (SN0)
	/*
	 * This should be after setting up the CpuID, since
	 * bte_lateinit ends up doing kmem_alloc_node.
	 */
	bte_lateinit();
#endif
	clkstart();

	/* allow more processors to boot */
	spunlock(bootlck, s0);

#if TRAPLOG_DEBUG
	*(long *)TRAPLOG_PTR = TRAPLOG_BUFSTART;
#endif

	/*
	 * Problem: After enabling this processor but before doing
	 * a kickidle, the master processor does a contmemall.  It
	 * then hangs in flushcaches waiting for us to flush our
	 * icache.  Unfortunately, we aren't receiving interrupts,
	 * so we never find out about the cpu action request and
	 * the entire system hangs.
	 *
	 * Solution: Mark ourselves as ISOLATED (done above) until 
	 * we're able to accept interrupts.  That way, flushcaches 
	 * knows not to interrupt/wait for us.
	 */
	private.p_flags &= ~PDAF_ISOLATED;
	allowintrs();

	/*
	 * Let the CPU dispatcher know that we're here.
	 */
	joinrunq();

#if CELL
	cell_cpu_init();
#endif
	/* initialize the stuff needed for R10k mfhi war */
	init_mfhi_war();

	/* everything set up - call idle */
	spl0();				/* Ensure that SR_IE is set */
	(void)splhi();
	ASSERT(getsr() & SR_IE);
	resumeidle(0);
	/* NOTREACHED */
	cmn_err(CE_PANIC, "cboot\n");
}

/*
 * Finish setting up the I/O for allowboot()
 */
static void
cpu_io_setup(void)
{
	register int i;
	int cpu;

	/*
	 * If a processor locked driver is locked to a non-existent cpu
	 * then change it to lock on to the master cpu
	 * A driver that wants its intr to locked onto a non-existent cpu
	 * is handled by iointr_at_cpu()
	 */
	for (i = bdevcnt; --i >= 0; ) {
		cpu = bdevsw[i].d_cpulock;
		/* cpu starts from 0 */
		if (!cpu_isvalid(cpu))
			bdevsw[i].d_cpulock = masterpda->p_cpuid;
	}

	for (i = cdevcnt; --i >= 0; ) {
		cpu = cdevsw[i].d_cpulock;
		if (!cpu_isvalid(cpu))
			cdevsw[i].d_cpulock = masterpda->p_cpuid;
	}
}

/*
 * If the debugger isn't present, set up the NMI crash dump vector.
 * Also, if "nmiprom" variable is set, install invalid address so
 * we go directly to POD mode on NMI.
 */
void
install_cpu_nmi_handler(int slice)
{
	nmi_t *nmi_addr;
	extern int dbgmon_nmiprom;

	nmi_addr = (nmi_t *)NMI_ADDR(get_nasid(), slice);

	if (dbgmon_nmiprom) {
	    nmi_addr->call_addr_c = (addr_t *) (__psint_t) 1;
	    return;
	}

	if (nmi_addr->call_addr)
		return;

	nmi_addr->magic = NMI_MAGIC;
	nmi_addr->call_addr = (addr_t *)nmi_dump;
	nmi_addr->call_addr_c = 
	    (addr_t *)(~((__psunsigned_t)(nmi_addr->call_addr)));
	nmi_addr->call_parm = 0;
}

/*
 * Copy the cpu registers which have been saved in the IP27prom format
 * into the eframe format for the node under consideration.
 */

eframe_t *
nmi_cpu_eframe_save(nasid_t nasid,
		    int	    slice)
{
	int 		prom_reg, eframe_reg, numberof_nmi_cpu_regs;
	machreg_t	*prom_format, *eframe_format;
	
	/* eframe_register_map specifies how register x of the prom
	 * maps to the eframe register.
	 * A -1 in the mapping means that there is no equivalent 
	 * eframe register.
	 *	Thus prom_cpu_register[x] == eframe[eframe_index_map[x]].
	 * NOTE: This table depends on the layout of the reg_struct.
	 */
	static int	prom_to_eframe_register_map[]  = 
	{-1,
	 EF_AT,EF_V0,EF_V1,
	 EF_A0,EF_A1,EF_A2,EF_A3,	
	 EF_A4,EF_A5,EF_A6,EF_A7,
	 EF_T0,EF_T1,EF_T2,EF_T3,
	 EF_S0,EF_S1,EF_S2,EF_S3,
	 EF_S4,EF_S5,EF_S6,EF_S7,
	 EF_T8,EF_T9,EF_K0,EF_K1,
	 EF_GP,EF_SP,EF_FP,EF_RA,
	 -1,EF_CAUSE,EF_EPC,EF_BADVADDR,
	 EF_ERROR_EPC,-1,EF_SR};

					       
	/* Get the total number of registers being saved by the prom */
	numberof_nmi_cpu_regs = sizeof(struct reg_struct) / sizeof(machreg_t);
	
	/* Make sure that we are always in sync with the current prom
	 * register format size
	 */
	if (numberof_nmi_cpu_regs != 
	    (sizeof(prom_to_eframe_register_map) / sizeof(int))) {
		cmn_err(CE_WARN,"Prom nmi register format has changed\n");
		return NULL;
	}
	/* Get the pointer to the current cpu's register set. */
	prom_format = 
	    (machreg_t *)(TO_UNCAC(TO_NODE(nasid, IP27_NMI_KREGS_OFFSET)) +
			  slice * IP27_NMI_KREGS_CPU_SIZE);

	/* Get the pointer to the current cpu's eframe save area */
	eframe_format = 
	    (machreg_t *)(TO_UNCAC(TO_NODE(nasid, IP27_NMI_EFRAME_OFFSET)) +
			  slice * IP27_NMI_EFRAME_SIZE);

	/* Do the actual copy */
	for (prom_reg = 0 ; prom_reg < numberof_nmi_cpu_regs; prom_reg++) {
		if ((eframe_reg = prom_to_eframe_register_map[prom_reg]) == -1)
			continue;
		eframe_format[eframe_reg] = prom_format[prom_reg];
	}
	return (eframe_t *)eframe_format;
}	

/*
 * Copy the cpu registers which have been saved in the IP27prom format
 * into the eframe format for the node under consideration.
 */
void
nmi_node_eframe_save(cnodeid_t  cnode)
{
	int		cpu;
	nasid_t		nasid;

	/* Make sure that we have a valid node */
	if (cnode == CNODEID_NONE)
		return;

	nasid = COMPACT_TO_NASID_NODEID(cnode);
	if (nasid == INVALID_NASID)
		return;

	/* Save the registers into eframe for each cpu */
	for(cpu = 0; cpu < CNODE_NUM_CPUS(cnode); cpu++) 
		nmi_cpu_eframe_save(nasid, cpu);
}

/*
 * Save the nmi cpu registers for all cpus in the system.
 */
void
nmi_eframes_save(void)
{
	cnodeid_t	cnode;

	for(cnode = 0 ; cnode < numnodes; cnode++) 
		nmi_node_eframe_save(cnode);
}

void
cont_nmi_dump(void)
{
	cpuid_t cpu;
	cnodeid_t node;
	eframe_t *ef;
	char *prog_name;
	pid_t pid;
	int i, n;
	extern int dump_in_progress;

	/* Check all the router links */
	if (probe_routers() != PROBE_RESULT_GOOD) {
		return;
	}

	cpu = getcpuid();
	wirepda(pdaindr[cpu].pda);

	if (r10k_intervene && r10k_progress_nmi && private.p_r10k_ack) {
	    ef = nmi_cpu_eframe_save((cputonasid(cpu)), (cputoslice(cpu)));

	    if (USERMODE(ef->ef_sr)) {
		if (curprocp) 
		    prog_name = curprocp->p_comm, pid = curprocp->p_pid;
		else 
		    prog_name = "null curproc", pid = 0;

		cmn_err(CE_CONT,
			"Please contact your SGI customer representative\n");

		cmn_err(CE_PANIC | CE_CPUID,
			"Progress intervention from cpu %d while running %s (pid %d). EPC 0x%x (sr 0x%x)",
			private.p_r10k_ack - 1, prog_name, pid,
			ef->ef_error_epc, ef->ef_sr);

	    } else {
		cmn_err(CE_PANIC | CE_CPUID,
			"Progress intervention in kernel mode EPC 0x%x (sr 0x%x)", 
			ef->ef_error_epc, ef->ef_sr);
	    }
	    /* cant get here */
	}

	/* 
	 * Use enter_panic_mode to allow only 1 cpu to proceed
	 */
	enter_panic_mode();

	/* Turn off stack sanity checking, since we're on the dump stack.
	 * It would be better to just switch to a pre-inited kthread.
	 */
	dump_in_progress = 1;

	/* 
	 * We blindly force curkstack to CURKERSTACK which is assumed 
	 * in the lock routines.  And we clear p_idlstkdepth so the
	 * interrupt handlers don't think the processor was idle and
	 * reset p_kstackflag.
	 */
	private.p_kstackflag = PDA_CURKERSTK;
	private.p_idlstkdepth = 0;

	/*
	 * Wait up to 15 seconds for the other cpus to respond to the NMI.
	 * If a cpu has not responded after 10 sec, send it 1 additional NMI.
	 * This is for 2 reasons:
	 *	- sometimes a MMSC fail to NMI all cpus.
	 *	- on 512p SN0 system, the MMSC will only send NMIs to
	 *	  half the cpus. Unfortunately, we dont know which cpus may be
	 *	  NMIed - it depends on how the site chooses to configure.
	 * 
	 * Note: it has been measure that it takes the MMSC up to 2.3 secs to
	 * send NMIs to all cpus on a 256p system.
	 */
	for (i=0; i < 1500; i++) {
		for (node=0; node < numnodes; node++)
			if (NODEPDA(node)->dump_count == 0)
				break;
		if (node == numnodes)
			break;
		if (i == 1000) {
			for (node=0; node < numnodes; node++)
				if (NODEPDA(node)->dump_count == 0) {
					cpu = CNODE_TO_CPU_BASE(node);
					for (n=0; n < CNODE_NUM_CPUS(node); cpu++, n++) {
						CPUMASK_SETB(nmied_cpus, cpu);
						SEND_NMI((cputonasid(cpu)), (cputoslice(cpu)));
					}
				}
					
		}
		us_delay(10000);
	}

	/* there's only one cpu really running.  Don't wait for the others
	 * to reboot.  They have no stacks.
	 */
	nmi_maxcpus = maxcpus;
	maxcpus = 1;
	nmied = 1;

	/*
	 * Save the nmi cpu registers for all cpu in the eframe format.
	 */
	nmi_eframes_save();

	/* save error state and call the FRU Analyzer before we panic */
	sn0_error_dump("nmi");
	kdebug = 0;

	cmn_err(CE_PANIC,
		"User requested vmcore dump (NMI), cpu %d handling",
		getcpuid());
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

void
adjust_nmi_handler(int cmd)
{
	static addr_t 	*saveaddr = 0;
	addr_t		*addr;
	cpuid_t		cpu, cpusl;
	nmi_t		*nmi_addr;
	paddr_t		pa;
	nasid_t		nid, mynid = get_nasid();

	/*
	 * Remember the symmon handler address from the first cpu.
	 */
	cpu = getcpuid();
	cpusl = cputoslice(cpu);
	nmi_addr = (nmi_t *)NMI_ADDR(COMPACT_TO_NASID_NODEID(CPUID_TO_COMPACT_NODEID(cpu)), cpusl);
	addr = (addr_t *)nmi_addr->call_addr;
	if (saveaddr == 0) saveaddr = (addr_t *)nmi_addr->call_addr;
	if (cmd < 0) {
		if (addr == (addr_t *)(nmi_dump))
			qprintf("Kernel dump on nmi\n");
		else
			qprintf("Symmon on nmi\n");
		return;
	}

	/*
	 * First, turn on permissions to write to pages on other nodes.
	 * Write the new handler, and turn perms back off.
	 */
	for (cpu = 0; cpu < maxcpus; cpu++) {
		cpusl = cputoslice(cpu);
		nid = COMPACT_TO_NASID_NODEID(CPUID_TO_COMPACT_NODEID(cpu));
		nmi_addr = (nmi_t *)NMI_ADDR(nid, cpusl);
		pa = NODE_OFFSET(nid) + NMI_OFFSET(nid, cpusl);
		*(__psunsigned_t *)BDPRT_ENTRY(pa, NASID_TO_REGION(mynid)) = MD_PROT_RW;
		addr = (addr_t *)nmi_addr->call_addr;
		qprintf("Switching nmi behavior on cpu %d\n", cpu);
		if (addr == saveaddr) {
			nmi_addr->call_addr = (addr_t *)nmi_dump;
		} else {
			nmi_addr->call_addr = saveaddr;
		}
		nmi_addr->call_addr_c = 
			(addr_t *)(~((__psunsigned_t)nmi_addr->call_addr));
		if (nid != mynid) 
			*(__psunsigned_t *)BDPRT_ENTRY(pa, NASID_TO_REGION(mynid)) = MD_PROT_NO;
	}
}
