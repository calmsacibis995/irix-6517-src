/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.223 $"

/*
 * IP30 specific miscellaneous functions
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/sbd.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <ksys/hwg.h>
#include <sys/invent.h>
#include <sys/kopt.h>
#include <sys/syssgi.h>
#include <sys/fpu.h>
#include <sys/conf.h>
#include <sys/callo.h>
#include <sys/debug.h>
#include <sys/rtmon.h>
#include <sys/sysinfo.h>
#include <sys/iograph.h>
#include <sys/atomic_ops.h>
#include <sys/nic.h>
#include <sys/clksupport.h>

#include <sys/RACER/IP30.h>
#include <sys/RACER/gda.h>
#include <sys/RACER/racermp.h>

#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#include <sys/arcs/kerncb.h>

#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xwidget.h>
#include <sys/xtalk/xbow.h>
#include <sys/RACER/heart.h>
#include <sys/RACER/heartio.h>
#include <sys/PCI/pcibr.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/pcibr_private.h>

#include <sys/scsi.h>

#include <bsd/sys/tcpipstats.h>

#include <sys/hwperfmacros.h>
#include <sys/nic.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>

#if TRAPLOG_DEBUG
#include <sys/traplog.h>
#endif

#include "RACERkern.h"

#ifdef HEART_COHERENCY_WAR
int heart_read_coherency_war = 0;
int heart_write_coherency_war = 0;
int fibbed_heart = 0;
extern char arg_heart_read_war[];
extern char arg_heart_write_war[];
#endif

#ifdef HEART_INVALIDATE_WAR
int _heart_invalidate_war = 0;
extern char arg_heart_invalidate_war[];
#endif

#ifdef US_DELAY_DEBUG
extern void check_us_delay(void);
#endif

/*
 * Per CPU data structures, indexed by cpu number.
 */
int maxcpus = MAXCPU;

pdaindr_t pdaindr[MAXCPU];		/* processor-dependent data area */
int processor_enabled[MAXCPU];

/* needs to be a spinlock as curthreadp not set @ first boot use */
lock_t ip30ledlock;

/*
 * This array is used when start_slave_loop is called by the master CPU
 * during boot. A CPU is associated with an index using the same
 * mapping as MPCONF array. When a CPU has entered the kernel slave
 * loop and is ready to continue, it sets its entry to 1
 */
char	slave_loop_ready[MAXCPU];

/*
 * For communication between sched and the new processor:
 * the new processor raises and waits to go low
 */
static volatile int	cb_wait = -1;

/*
 * Number of CPUs running when the NMI hit.
 */
int nmi_maxcpus = 0;
/*
 * External Global variables
 */
extern int intstacksize;
extern int valid_dcache_reasons;
extern int valid_icache_reasons;

/*
 * External Procedures
 */
extern struct lf_listvec *str_init_alloc_lfvec(cpuid_t);
extern void str_init_slave(struct lf_listvec *, cpuid_t, cnodeid_t);
extern struct strstat *str_init_alloc_strstat(cpuid_t);
extern void nmi_dump(void);
static void init_board_rev(void);
extern void ecc_init(void);

/*
 * Forward Referenced Procedures
 */
static void init_nmi_dump(void);

/* table of probeable kmem addresses */
#define PROBE_SPACE(X)	PHYS_TO_K1(X)
struct kmem_ioaddr kmem_ioaddr[] = {
	{ 0, 0 },
};

short cputype = 30;	/* integer value for cpu (i.e. xx in IPxx) */
int is_octane_lx;	/* 0 = classic octane, 1 = octane lx */
static uint sys_id;	/* initialized by init_sysid */

heart_piu_t	       *heart_piu = HEART_PIU_K1PTR;
#define	RAW_COUNT()	(heart_piu->h_count)

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))
char eaddr[6];

unsigned char *
etoh(char *enet)
{
	static unsigned char dig[6];
	unsigned char *cp;
	int i;

	for ( i = 0, cp = (unsigned char *)enet; *cp; ) {
		if ( *cp == ':' ) {
			cp++;
			continue;
		}
		else if ( !ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1)) ) {
			return NULL;
		}
		else {
			if ( i >= 6 )
				return NULL;
			dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
			cp += 2;
		}
	}

	return i == 6 ? dig : NULL;
}

/* initialize the serial number.  Called from mlreset.
 */
static void
init_sysid(void)
{
	char *cp;

	cp = (char *)arcs_getenv("eaddr");
	bcopy(etoh(cp), eaddr, 6);

	sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
		 (eaddr[4] << 8) | eaddr[5];

	sprintf(arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
	    (eaddr[0]>>4)&0xf, eaddr[0]&0xf,
	    (eaddr[1]>>4)&0xf, eaddr[1]&0xf,
	    (eaddr[2]>>4)&0xf, eaddr[2]&0xf,
	    (__psunsigned_t)(eaddr[3]>>4)&0xf, (__psunsigned_t)eaddr[3]&0xf,
	    (__psunsigned_t)(eaddr[4]>>4)&0xf, (__psunsigned_t)eaddr[4]&0xf,
	    (__psunsigned_t)(eaddr[5]>>4)&0xf, (__psunsigned_t)eaddr[5]&0xf);
}

/*  Pass back the serial number associated with the system ID prom.
 */
int
getsysid(char *hostident)
{
	/*
	 * serial number is only 4 bytes long on IP30.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */
	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}

static void	ip30_chips_mlreset(void);

/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup.
 */
void
mlreset(int is_slave)
{
	extern int	_prom_cleared;
	extern int	pdcache_size;
	extern int	reboot_on_panic;
	extern int	softpowerup_ok;
	heartreg_t	heart_mode;
	heartreg_t	heart_status;
	int i;
	ioc3_mem_t	*ioc3_mem = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;

	/* clear the r1[02]k hw performance 
	 * control and count registers  
	 */
	r1nkperf_control_register_set(0,0);
	r1nkperf_control_register_set(1,0);
	r1nkperf_data_register_set(0,0);
	r1nkperf_data_register_set(1,0);

	if (IS_R12000()) {
		r1nkperf_control_register_set(2,0);
		r1nkperf_control_register_set(3,0);
		r1nkperf_data_register_set(2,0);
		r1nkperf_data_register_set(3,0);
	}

	if (is_slave) {
		heart_mp_intr_init(getcpuid());	/* enable interrupt */
		flush_tlb(TLBFLUSH_NONPDA);	/* flush all BUT pda */
		flush_cache();			/* flush all caches */

		return;
	}

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1)
		kdebug = 0;

	master_procid = getcpuid();	/* should be 0 w/o PM20 */

	heart_mode = HEART_PIU_K1PTR->h_mode;
	heart_status = HEART_PIU_K1PTR->h_status;
	for (i = 0; i < MAXCPU; i++)
		processor_enabled[i] = !(heart_mode & HM_PROC_DISABLE(i)) &&
			heart_status & HEART_STAT_PROC_ACTIVE(i);

	init_board_rev();		/* read board rev info from mpconf */

	ip30_chips_mlreset();		/* initialize base IO chips */

	init_sysid();
	load_nvram_tab();		/* get a copy of the nvram */

	/* Cachecolormask is set from the bits in a vaddr/paddr
	 * which the cpu looks at to determine if a VCE should
	 * be raised.
	 */
	cachecolormask = pdcache_size / (NBPP * 2) - 1;

	if ((int)cachecolormask < 0)
		cachecolormask = 0;

	softpowerup_ok = 1;		/* for syssgi() */

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	/* R10K has coherent IO and hardware VCE avoidance */
	valid_icache_reasons &= ~CACH_AVOID_VCES;
	valid_dcache_reasons &= ~CACH_AVOID_VCES;

#ifdef HEART_COHERENCY_WAR
	/* This turns on more flushing that we would like, but unfortunately
	 * we do not have a lot of choice.  Remember there still are holes
	 * due to T5 speculation problems.  These values can be overriden
	 * by the kopt variables "heart_read_war" and "heart_write_war".
	 */
	if (heart_rev() <= HEART_REV_B) {
		if (!fibbed_heart)
			heart_read_coherency_war = 1;
		heart_write_coherency_war = 1;
		cachewrback = 1;	/* writeback (I/O noncoherent) */
	}

	/* Be safe for normal systems for the "son of the audio bug" in
	 * the rev C Silicon.  We should try to run machines on campus
	 * naked with rev C hearts to try and flush out any more problems.
	 */
	if (heart_rev() == HEART_REV_C) {
		heart_write_coherency_war = 1;
		cachewrback = 1;	/* writeback (I/O noncoherent) */
	}

	/* Allow flushing modes to be overridden via kopt */
	if (is_specified(arg_heart_read_war))
		heart_read_coherency_war = atoi(arg_heart_read_war);
	if (is_specified(arg_heart_write_war))
		heart_write_coherency_war = atoi(arg_heart_write_war);

	/* really should be &&, but drivers are pretty well covered */
	if (heart_read_coherency_war == 0 || heart_write_coherency_war == 0)
#endif
	{
		valid_icache_reasons &= ~CACH_IO_COHERENCY;
		valid_dcache_reasons &= ~CACH_IO_COHERENCY;

		cachewrback = 0;	/* no writeback (I/O coherent) */
	}

#ifdef HEART_INVALIDATE_WAR
	if (is_specified(arg_heart_invalidate_war)) {
		_heart_invalidate_war = atoi(arg_heart_invalidate_war);
	}
	else {
		volatile struct mpconf_blk	*mpconf;

		for (i = 0; i < MAXCPU; i++) {
			if (!cpu_enabled(i))
				continue;

			mpconf = (mpconf_t *)(MPCONF_ADDR + i * MPCONF_SIZE);

			/* All T5 silicon before 2.7 or 3.1 has the bug.
			 */
			if ((mpconf->pr_id<=C0_MAKE_REVID(C0_IMP_R10000,2,6)) ||
			    (mpconf->pr_id==C0_MAKE_REVID(C0_IMP_R10000,3,0)))
				_heart_invalidate_war = 1;
		}
	}

	/* If the war is on remove 3 prefetches from bzero/bcopy.
	 * If it war is not on, do not call _page* routines, nop them.
	 */
	if (_heart_invalidate_war != 0) {
		extern int bcopy_pref1[], bcopy_pref2[], bzero_pref[];

		*bcopy_pref1 = 0;
		*bcopy_pref2 = 0;
		*bzero_pref = 0;
	}
	else {
		extern void bzero_nopage(void);
		extern void bcopy_nopage(void);
		bzero((void *)bcopy,
		      (__psunsigned_t)bcopy_nopage-(__psunsigned_t)bcopy);
		bzero((void *)bzero,
		      (__psunsigned_t)bzero_nopage-(__psunsigned_t)bzero);
	}
	flush_cache();
#endif

	spinlock_init(&ip30clocklock,"ip30clock");
	spinlock_init(&ip30ledlock,"ip30led");

	/* Catch single CPU systems early to avoid extra memory overhead */
	for (i = MAXCPU - 1; i >= 0; i--) {
		if (cpu_enabled(i)) {
			maxcpus = i + 1;
			break;
		}
	}

	/* need to implement keyboard beep differently on octane lx */
	if (is_octane_lx = (ioc3_mem->gppr_2 == 0)) {
		ioc3_mem->gpcr_c = GPCR_INT_OUT_EN;	/* disable keybd beep */
		ioc3_mem->int_out = 0x1a | INT_OUT_MODE_SQW;
	}

	_prom_cleared = 1;
}

/* Probe for floating point chip.
 */
void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}

/* fp chip initialization.
 */
void
fp_init(void)
{
	set_fpc_csr(0);
}

/* Default to exclusive on write.  On SP move to PG_COHRNT_EXCL. */
static int racer_cachebits = PG_COHRNT_EXLWR;

/* set the cache algorithm pde_t *p */
void
pg_setcachable(pde_t *p)
{
	pg_setpgi(p, (p->pgi & ~PG_CACHMASK) | racer_cachebits);
}

uint
pte_cachebits(void)
{
	return racer_cachebits;
}


/* Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to make
 * it change color.  We don't turn the LED off, or set it to RED.
 */
void
set_leds(int pattern)
{
	ioc3_mem_t *ioc3 = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;
	int s;

	s = mutex_spinlock(&ip30ledlock);

	if (pattern) {
		ioc3->gppr_0 = 0;		/* amber */
		ioc3->gppr_1 = 1;
	}
	else {
		ioc3->gppr_1 = 0;		/* green */
		ioc3->gppr_0 = 1;
	}

	mutex_spinunlock(&ip30ledlock,s);

	flushbus();
}

/*  Initialize led counter and value.
 */
void
reset_leds(void)
{
	set_leds(0);
}

/* Implementation of syssgi(SGI_SETLED,a1) */
void
sys_setled(int a1)
{
	set_leds(a1);
}

/* dobootduty - called by allowboot */
static void
dobootduty(void)
{
	register pda_t	*pda;
	cnodeid_t node;

	ASSERT(cb_wait >= 0 && cb_wait < maxcpus);
	pda = pdaindr[cb_wait].pda;
	node = pda->p_nodeid;

	/*
	 * grab memory for interrupt stack
	 */
	pda->p_intstack = kvpalloc(btoc(intstacksize), VM_DIRECT, 0);
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
	pda->ksaptr=(struct ksa *)kern_calloc_node(1,sizeof(struct ksa),node);
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
	cb_wait = -1;
}

static void
cpu_disable(int id)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	heart_piu->h_mode |= HM_PROC_DISABLE(id);
	processor_enabled[id] = 0;
}

static int
cpu_exists(int id)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	return heart_piu->h_status & HEART_STAT_PROC_ACTIVE(id);
}

int
cpu_enabled(cpuid_t id)
{
	return processor_enabled[id];
}

/*
 * Called when the master processor is satisfied that the system is sane
 * enough to come up with multiple CPU's.
 */
void
allowboot(void)
{
	int	badcpu_count = 0;
	int	cpu;
	int	i;
	int	slave_cpus = 0;
	int	timeout;

	init_timebase();		/* do early for best accuracy */
	ecc_init();			/* master cpu */

	if (showconfig)
		cmn_err(CE_CONT, "Master processor is %d\n", master_procid);

#ifdef HEART_COHERENCY_WAR
	if (is_specified(arg_heart_read_war))
		cmn_err(CE_WARN, "Reset heart_read_coherency_war to %d\n",
			heart_read_coherency_war);
	if (is_specified(arg_heart_write_war))
		cmn_err(CE_WARN, "Reset heart_write_coherency_war to %d\n",
			heart_write_coherency_war);
#endif
#ifdef HEART_INVALIDATE_WAR
	if (is_specified(arg_heart_invalidate_war))
		cmn_err(CE_WARN, "Reset heart_invalidate_war to %d\n",
			_heart_invalidate_war);
#endif

	for (i = 0; i < MAXCPU; i++) {
		extern void			bootstrap(void);
		volatile struct mpconf_blk	*mpconf;

		/* next processor if the current one is inactive */
		if (!cpu_enabled(i)) {
			if (cpu_exists(i))
				cmn_err(CE_WARN,"Processor %d is disabled.",i);
			continue;
		}

		mpconf = (mpconf_t *)(MPCONF_ADDR + i * MPCONF_SIZE);

		/* Warn about pre-2.7 silicon because of coherency bugs */
		if (mpconf->pr_id < C0_MAKE_REVID(C0_IMP_R10000,2,6)) {
#if HEART_COHERENCY_WAR
			cmn_err((heart_rev()<HEART_REV_C) ? CE_WARN : CE_PANIC,
#else
			cmn_err(CE_PANIC,
#endif
				"Processor %d is rev %d.%d < 2.6.  "
				"This may cause data corruption.\n",
				i,
				(mpconf->pr_id&C0_MAJREVMASK)>>C0_MAJREVSHIFT,
				mpconf->pr_id&C0_MINREVMASK);
				
		}

		/* Do not need to start-up the master CPU.
		 */
		if (i == master_procid)
			continue;

		/* verify processor in kernel slave loop */
		if (!slave_loop_ready[i]) {
			cmn_err(CE_WARN,
				"Processor #%d not ready to start\n", i);
			cpu_disable(i);
			continue;
		}

		/* keep track of number of slave CPUs */
		slave_cpus++;

		mpconf->lnch_parm = mpconf;
		mpconf->rendezvous = NULL;
		mpconf->rndv_parm = NULL;
		mpconf->stack = pdaindr[i].pda->p_bootlastframe;

		if (showconfig)
			cmn_err(CE_CONT, "Starting processor #%d\n", i);

		mpconf->launch = (void *)bootstrap;
	}

#ifdef US_DELAY_DEBUG
	check_us_delay();		/* see if this processor clocks ok */
#endif

#define	BOOTTIMEOUT	500000		/* 1/2 sec */
	cpu = slave_cpus;
	timeout = BOOTTIMEOUT * slave_cpus;
	while (--timeout && slave_cpus) {
		if (cb_wait != -1) {
			dobootduty();
			slave_cpus--;
		}
		DELAY(1);		/* 1 us */
	}

	/*
	 * there may be a race condition if the processor handled last in
	 * the previous loop is the first we checked in the next loop
	 */
	if (cpu)
		DELAY(5000);

	/* do a quick check to see that the other processors started up */
	for (i = 0; i < maxcpus; i++) {
		if (cpu_enabled(i) && pdaindr[i].CpuId != i) {
			cmn_err(CE_NOTE, "Processor #%d did not start!", i);
			pdaindr[i].CpuId = -1;
			cpu_disable(i);
			badcpu_count++;
		}
	}
	if (badcpu_count)
		cmn_err(CE_PANIC, "Cannot start without all CPUs\n");

	/* Avoid unnecessary interventions on single CPU system */
	if (numcpus == 1)
		racer_cachebits = PG_COHRNT_EXCL;

	/*
	 * If a processor locked driver is locked to a non-existent cpu
	 * then change it to lock on to the master cpu.
	 */
	for (i = 0; i < bdevcnt; i++) {
		cpu = bdevsw[i].d_cpulock;
		if (!cpu_isvalid(cpu))
			bdevsw[i].d_cpulock = masterpda->p_cpuid;
	}

	for (i = 0; i < cdevcnt; i++) {
		cpu = cdevsw[i].d_cpulock;
		if (!cpu_isvalid(cpu))
			cdevsw[i].d_cpulock = masterpda->p_cpuid;
	}

        /* initialize the core dump NMI vector in GDA */
        init_nmi_dump();
	init_mfhi_war();
}

/*
 * boot routine called from assembly as soon as the bootstrap routine
 * has context set up
 *
 * interrupts are disabled and we're on the boot stack (which is in pda).
 * the pda is already accessible
 */
void
cboot(void)
{
	extern lock_t	bootlck;
	int		id = getcpuid();
	int		s0;
	extern int	utlbmiss[];

	ASSERT(id < maxcpus);
	wirepda(pdaindr[id].pda);
	mlreset(1);

	private.p_cpuid = id;
	CPUMASK_CLRALL(private.p_cpumask);
	CPUMASK_SETB(private.p_cpumask, id);
	/* plug exception handler vecs in */
	private.common_excnorm = get_except_norm();
	private.p_kvfault = 0;

	/* all cpu run at the same speed */
	private.cpufreq = pdaindr[master_procid].pda->cpufreq;
	private.cpufreq_cycles = pdaindr[master_procid].pda->cpufreq_cycles;
	private.decinsperloop = pdaindr[master_procid].pda->decinsperloop;

	/* find out what kind of CPU/FPU we have */
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
	 */
	cb_wait = id;
	while (cb_wait != -1)
		;

	/* now safe to claim we're enabled */
	private.p_flags |= (PDAF_ENABLED | PDAF_ISOLATED);
	numcpus++;
	CPUMASK_SETB(maskcpus, id);
	pdaindr[id].CpuId = id;

	/* do initialization required for each processor */
	stopclocks();

	spinlock_init(&pdaindr[id].pda->p_special, "pdaS");

	tlbinfo_init();

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

	ecc_init();

	/*
	 * Let the CPU dispatcher know that we're here.
	 */
	joinrunq();

#if CELL
	cell_cpu_init();
#endif	

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
 * If the debugger isn't present, set up the NMI crash dump vector.
 */
static void
init_nmi_dump(void)
{

        /* If the debugger hasn't been there, set the vector. */
        if (!(GDA->g_nmivec)) {
                if (IS_KSEG1(nmi_dump))
                        GDA->g_nmivec = PHYS_TO_K1(nmi_dump);
                else
                        GDA->g_nmivec = PHYS_TO_K0(nmi_dump);
        } else {
                return;
        }

        /* Make sure the real master is the one that runs the dump code. */
        GDA->g_masterspnum = getcpuid();

	GDA->g_magic = GDA_MAGIC;
}

/* nmi_dump has set up our stack.  Now we must make everything else
 * ready and call panic.
 */
void
cont_nmi_dump(void)
{
	extern sthread_t *dumpsthread;

        /* Wire in the master's PDA. */
        wirepda(pdaindr[master_procid].pda);

        /* there's only one cpu really running.  Don't wait for the others
         * to reboot.  They have no stacks.
         */
        nmi_maxcpus = maxcpus;
        maxcpus = 1;

	/*
	 * need thread context for mutex code called directly/indirectly by
	 * the NMI dumping code
	 */
	if (!private.p_curkthread)
		private.p_curkthread = ST_TO_KT(dumpsthread);

        cmn_err(CE_PANIC, "User requested vmcore dump (NMI)");
}

/* Return address of exception handler for all exceptions other	then utlbmiss.
 */
inst_t *
get_except_norm(void)
{
	extern inst_t exception[];

	return exception;
}

/* Return the IP30 board revision from the NIC and report as:
 *	(part dash # << 16) | (rev letter)
 *
 * Current P0 board == 30-0887-001 and Revision 'E', so board_rev is
 * set to (1<<16) | 0x45 == 0x10045.
 */
int ip30_rev;
int
ip30_board_rev(void)
{
	return ip30_rev;
}

/* Patterned after io/nic.c, but w/o us_delay use so can be called from
 * mlreset() on IP30.
 */
static int
early_nic_access_mcr32(nic_data_t data, int pulse, int sample, int usec)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	heartreg_t final_count;
	__uint32_t value;

	*(volatile __uint32_t *) data = MCR_PACK(pulse, sample);

	do
		value = *(volatile __uint32_t *)data;
	while (! MCR_DONE(value));

	/* us_delay(usec) -- no need to worry about wrap */
	final_count = heart_piu->h_count +
		((long)usec * (HEART_COUNT_RATE+500000))/1000000;
	while(heart_piu->h_count < final_count)
		;

	return MCR_DATA(value);
}

/* Read the board revision info from NIC, and store formatted version # in
 * memory.
 */
static void
init_board_rev(void)
{
	nic_data_t mcr = (nic_data_t)&BRIDGE_K1PTR->b_nic;
	int dash = 1, rev = 0;
	char buf[256];
	char *p;
	int rc;

	rc = nic_info_get(early_nic_access_mcr32,mcr,buf);
	if (rc != NIC_OK && rc != NIC_DONE) {
		ip30_rev = 0x10045;		/* assume a P0 */
		return;
	}

	if (p = strstr(buf,"Part:")) {
		p += 5;
		if (p = strchr(p,'-')) {
			p++;
			if (p = strchr(p,'-')) {
				p++;
				dash = atoi(p);
			}
		}
	}
	if (p = strstr(buf,"Revision:"))
		rev = *(p+9);

#ifdef HEART_COHERENCY_WAR
	if (heart_rev() == HEART_REV_B)
		fibbed_heart = strstr(buf,"Group:fe") != NULL || rev >= 'J';
#endif	/* HEART_COHERENCY_WAR */

	ip30_rev = (dash<<16) | rev;
}

void
add_ioboard(void)	/* should now be add_ioboards() */
{
}

void
add_cpuboard(void)
{
	int cpuid = cpuid();

	/* For type INV_CPUBOARD, ctrl is CPU board type, unit = -1.
	 * NOTE: all IP30 CPUs have the same clock speed.
	 */
	add_to_inventory(INV_PROCESSOR,INV_CPUBOARD, cpu_mhz_rating(),
			 -1, INV_IP30BOARD);

	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid, 0,
			 private.p_cputype_word);
	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid, 0,
			 private.p_fputype_word);

	/* All IP30 caches are the same size */
	add_to_inventory(INV_MEMORY, INV_SIDCACHE, 0, 0, private.p_scachesize);

	/* A good time for some warnings about downrev hardware and WARs */
#ifdef HEART_COHERENCY_WAR
	if (heart_rev() == HEART_REV_A)
		cmn_err(CE_PANIC,
			"Heart revision A is no longer supported.\n");
	if (heart_rev() <= HEART_REV_C)
		cmn_err(CE_WARN, "Prototype Heart chip, revision %c.",
			'A' + heart_rev() - 1);
#else
	if (heart_rev() <= HEART_REV_C)
		cmn_err(CE_PANIC, "Prototype Heart chip, revision %c.",
			'A' + heart_rev() - 1);
#endif
}

int
cpuboard(void)
{
	return INV_IP30BOARD;
}

/*  On IP30, unix overlays the prom's bss area.  Therefore, only
 * those prom entry points that do not use prom bss or are
 * destructive (i.e. reboot) may be called.  There is however,
 * a period of time before the prom bss is overlayed in which
 * the prom may be called.  After unix wipes out the prom bss
 * and before the console is inited, there is a very small window
 * in which calls to dprintf will cause the system to hang.
 *
 * If symmon is being used, it will plug a pointer to its printf
 * routine into the restart block which the kernel will then use
 * for dprintfs.  The advantage of this is that the kp command
 * will not interfere with the kernel's own print buffers.
 *
 */
# define ARGS	a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt,ARGS)
char *fmt;
long ARGS;
{
	/* Check for presence of symmon.
	 */
	if (SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf)
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	else if (cn_is_inited())	/* implies PROM bss is gone */
		cmn_err(CE_CONT,fmt,ARGS);
	else				/* try thru PROM */
		arcs_printf (fmt,ARGS);
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

#if DEBUG
#if PG_UNCACHED != PG_COHRNT_EXLWR
#error PG_UNCACHED is not set-up for MP!
#endif
#define PTEASSERT(pte)							\
	ASSERT(pte.pte_cc == (PG_COHRNT_EXLWR >> PG_CACHSHFT)  ||	\
		pte.pte_cc == (PG_NONCOHRNT >> PG_CACHSHFT)    ||	\
	    	pte.pte_cc == (PG_COHRNT_EXCL >> PG_CACHSHFT)     ||	\
		pte.pte_cc == (PG_UNCACHED_ACC >> PG_CACHSHFT) ||	\
		(pte.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT) &&	\
		 !is_in_main_memory(ctob(pte.pg_pfn))));

uint
tlbdropin(unsigned char *tlbpidp, caddr_t vaddr, pte_t pte)
{
	uint _tlbdropin(unsigned char *, caddr_t, pte_t);

	PTEASSERT(pte);

	return(_tlbdropin(tlbpidp,vaddr,pte));
}

void
tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
	void _tlbdrop2in(unchar, caddr_t, pte_t, pte_t);

	if (pte.pte_vr)
		PTEASSERT(pte);
	if (pte2.pte_vr)
		PTEASSERT(pte2);

	_tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}
#endif /* DEBUG */

/*
 * The IP30 always has a Heart, a Bridge visible
 * via heart's "widget 15" windows, and an IOC3
 * on the other side of the Bridge. Production
 * IP30s have XBow chips visible as "widget 0"
 * but early units use DIR_CNNCT to attach
 * the Bridge directly to the Heart.
 *
 * Propogate mlreset to each of these chips,
 * and do whatever it takes to get them talking
 * enough that we can continue booting.
 */
static void
ip30_chips_mlreset(void)
{
	xbow_t	       *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);
	bridge_t       *bridge_wid = BRIDGE_K1PTR;
	ioc3_cfg_t     *ioc3_cfg = (ioc3_cfg_t *)IOC3_PCI_CFG_K1PTR;
	ioc3_mem_t     *ioc3_mem = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;

	static xtalk_early_piotrans_addr_f	ip30_early_piotrans_addr;

	xtalk_set_early_piotrans_addr(ip30_early_piotrans_addr);

	/*
	 * Individual chip initializations
	 */
	heart_mlreset();
	xbow_mlreset(xbow_wid);

	ioc3_mlreset(ioc3_cfg, ioc3_mem);

	/*
	 * IP30-specific chip bashing
	 */

	/* RRB allocation: 0&1 get 4+1; 2&4 get 1; 3 gets 3. */
	bridge_wid->b_even_resp = 0x0A9C8888;
	bridge_wid->b_odd_resp = 0x999C8888;

	/* DIR MAP: point it at heart widget, 512MB offset */
	bridge_wid->b_dir_map =
		((HEART_ID << BRIDGE_DIRMAP_W_ID_SHFT) |
		 BRIDGE_DIRMAP_ADD512);

	/* other devices can wait until normal discovery. */

	/*
	 * Nail down preallocated interrupt routes.
	 */
	heart_intr_prealloc((void *)HEART_PIU_K1PTR,
			    IP30_HVEC_WIDERR_XBOW,
			    xbow_intr_preset,
			    (void *)xbow_wid,
			    -1);

	heart_intr_prealloc((void *)HEART_PIU_K1PTR,
			    IP30_HVEC_WIDERR_BASEIO,
			    pcibr_xintr_preset,
			    (void *)bridge_wid,
			    -1);


	heart_intr_prealloc((void *)HEART_PIU_K1PTR,
			    IP30_HVEC_IOC3_ETHERNET,
			    pcibr_xintr_preset,
			    (void *)bridge_wid,
			    IP30_BVEC_IOC3_ETHERNET);

	heart_intr_preconn((void *)HEART_PIU_K1PTR,
			   IP30_HVEC_IOC3_ETHERNET,
			   ioc3_intr,
			   (intr_arg_t)0);

	heart_intr_prealloc((void *)HEART_PIU_K1PTR,
			    IP30_HVEC_IOC3_SERIAL,
			    pcibr_xintr_preset,
			    (void *)bridge_wid,
			    IP30_BVEC_IOC3_SERIAL);

	heart_intr_preconn((void *)HEART_PIU_K1PTR,
			   IP30_HVEC_IOC3_SERIAL,
			   ioc3_intr,
			   (intr_arg_t)0);

	/* Wire-up soft power:
	 */
#ifdef HEART_COHERENCY_WAR
	if (heart_rev() > HEART_REV_A)
#endif
	{
		heart_intr_prealloc((void *)HEART_PIU_K1PTR, IP30_HVEC_ACFAIL,
				    pcibr_xintr_preset, (void *)bridge_wid,
				    IP30_BVEC_ACFAIL);
	}
	heart_intr_prealloc((void *)HEART_PIU_K1PTR, IP30_HVEC_POWER,
			    pcibr_xintr_preset, (void *)bridge_wid,
			    IP30_BVEC_POWER);
	heart_intr_preconn((void *)HEART_PIU_K1PTR, IP30_HVEC_POWER, powerintr,
			   0);
}

/*
 *	heart_early_piotrans_addr:
 *	Some devices (graphics textport!) need to
 *	find PIO addresses before the infrastructure
 *	is really ready to run.
 *	This function searches for the N'th crosstalk
 *	device matching the specified part and mfg
 *	numbers, and figures out a correct PIO address
 *	for the early code to use.
 */
/*ARGSUSED6*/
static caddr_t
ip30_early_piotrans_addr(	xwidget_part_num_t part_num,
				xwidget_mfg_num_t mfg_num,
				int which,
				iopaddr_t xtalk_addr,
				size_t byte_count,
				unsigned flags)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	heartreg_t	status = heart_piu->h_status;
	int		hport = status & HEART_STAT_WIDGET_ID;
	xbow_t	       *xbow = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);
	int		xport;
	widget_cfg_t   *widget;
	widgetreg_t	id;

	extern caddr_t	heart_addr_xio_to_kva(int, iopaddr_t, size_t);

	widget = (widget_cfg_t *)K1_MAIN_WIDGET(hport);
	id = widget->w_id;
	if ((part_num == XWIDGET_PART_NUM(id)) &&
	    (mfg_num == XWIDGET_MFG_NUM(id))) {
	    if (which == 0)
		return heart_addr_xio_to_kva(hport, xtalk_addr, byte_count);
	    which --;
	}

	widget = (widget_cfg_t *)K1_MAIN_WIDGET(0);
	id = widget->w_id;
	if ((part_num == XWIDGET_PART_NUM(id)) &&
	    (mfg_num == XWIDGET_MFG_NUM(id))) {
	    if (which == 0)
		return heart_addr_xio_to_kva(0, xtalk_addr, byte_count);
	    which --;
	}

	for (xport = 15; xport >= 8; xport --)
	    if (xport != hport)
		if ((xbow->xb_link(xport).link_aux_status & XB_AUX_STAT_PRESENT)) {
		    widget = (widget_cfg_t *)K1_MAIN_WIDGET(xport);
		    id = widget->w_id;
		    if ((part_num == XWIDGET_PART_NUM(id)) &&
			(mfg_num == XWIDGET_MFG_NUM(id))) {
			if (which == 0)
			    return heart_addr_xio_to_kva(xport, xtalk_addr, 
							 byte_count);
			which --;
		    }
		}

	return NULL;
}


#if HWG_PERF_CHECK && !DEBUG
extern unsigned		_get_count(void);

unsigned long		hwg_perf_countperms = 0;
unsigned long		hwg_perf_pspercount = 0;
#if HWG_PERF_MIN_US
unsigned long		hwg_perf_min_us = HWG_PERF_MIN_US;
unsigned long		hwg_perf_min = 0;
#endif

static void
hwg_perf_calibrate(void)
{
    unsigned	c[3], *cp;
    unsigned	s;

    cp = c;
    s = spl7();
    us_delay(1000);
    *cp++ = _get_count();
    us_delay(1000);
    *cp++ = _get_count();
    us_delay(2000);
    *cp++ = _get_count();
    splx(s);
    hwg_perf_countperms = (c[2]-c[1]) - (c[1]-c[0]);
    hwg_perf_pspercount = 1000000000 / hwg_perf_countperms;

    /* only print stuff that takes more than 10ms */
#if HWG_PERF_MIN_US
    hwg_perf_min = (hwg_perf_countperms * hwg_perf_min_us) / 1000;
#endif
}

char		hwg_perf_buf[128];

/* pv is "millionths" here */
char *
hwg_perf_fmt_ps(int ld, int rd, unsigned long pv)
{
    char	       *op;
    int			cct;

    if (rd < 6) {
	unsigned long	p10;
	int		t;
	p10 = 1;
	for (t=rd; t<6; t++)
	    p10 *= 10;
	pv = (pv + (p10 / 2)) / p10;
    }

    op = hwg_perf_buf + sizeof hwg_perf_buf;
    *--op = '\0';
    if (rd > 0) {
	do {
	    *--op = '0' + (pv % 10);
	    pv /= 10;
	    rd --;
	} while (rd > 0);
	*--op = '.';
    }
    cct = 0;
    while ((ld > 0) && (pv > 0)) {
	if (cct > 2) {
	    *--op = ',';
	    cct = 0;
	    ld--;
	    continue;
	}
	*--op = '0' + (pv % 10);
	pv /= 10;
	ld--;
	cct ++;
    }
    if (pv > 0)
	*op = '*';
    while (ld-->0)
	*--op = ' ';
    return op;
}

/* d is "T5 counts" here */
static char *
hwg_perf_fmt(int ld, int rd, unsigned d)
{
    return hwg_perf_fmt_ps(ld, rd, d * hwg_perf_pspercount);
}

/* ct is "T5 counts" here */
void
hwg_perf_prS(unsigned ct, char *who)
{
    if (!hwg_perf_countperms)
	hwg_perf_calibrate();
#if HWG_PERF_MIN_US
    if (ct >= hwg_perf_min)
#endif
	printf("%sus %s\n", hwg_perf_fmt(12,2,ct), who);
}

/* d is "T5 counts" here */
static void
hwg_perf_prN(int ld, int rd, unsigned d)
{
    printf("%s,", hwg_perf_fmt(ld, rd, d));
}

#define	pr22(d)		hwg_perf_prN(2,2,(d))
#define	pr12(d)		hwg_perf_prN(1,2,(d))

/* cval is "heart counts" here */
void
hwg_hprint(unsigned long cval, char *nbuf)
{
    cval *= HEART_COUNT_NSECS;
    cmn_err(CE_CONT, "%sms %s\n", hwg_perf_fmt_ps(12,4,cval), nbuf);
}

#endif

/* init_all_devices: driver "xxx_init" routines have
 * been called, time to walk the hardware.
 * but first, we generate verticies for the
 * various devices CURRENTLY IN USE and mark
 * them, so their drivers know to be gentle.
 */

extern void		heart_startup(vertex_hdl_t);

void
init_all_devices(void)
{
    /* REFERENCED */
    graph_error_t	    rc;
    vertex_hdl_t	    node;
    vertex_hdl_t	    cpus;
    vertex_hdl_t	    cpuv;
    vertex_hdl_t	    memv;
    vertex_hdl_t	    baseio;
    vertex_hdl_t	    scsi;
    vertex_hdl_t	    scsi_ctlr;
    vertex_hdl_t	    disk;
    int			    i;
    char		    cpun[10];

#if HWG_PERF_CHECK && !DEBUG
    unsigned long	    cval;
#endif
    /*
     * Create our "node" vertex and set it up.
     * we force its cnodeid to an interesting value
     * which just has to be different from CNODEID_NONE.
     * NOTE: The Heart driver takes over this vertex.
     */

    node = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_NODE, &node);
    ASSERT(rc == GRAPH_SUCCESS);
    ASSERT(node != GRAPH_VERTEX_NONE);

    mark_nodevertex_as_node(node, 0);
#if DEBUG && HWG_DEBUG
    cmn_err(CE_CONT, "node vertex set up: %v\n", node);
#endif

    /*
     * Add the CPU verticies to the graph
     * at /hw/node/cpu/%d (where %d is the cpu ID)
     */
    cpus = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(node, EDGE_LBL_CPU, &cpus);
    ASSERT(rc == GRAPH_SUCCESS);
    ASSERT(cpus != GRAPH_VERTEX_NONE);
#if DEBUG && HWG_DEBUG
    cmn_err(CE_CONT, "%v maxcpus is %d\n", cpus, maxcpus);
#endif

    /* Add PMx0 NIC data @ node -- cheat and use bridge routine as they are
     * the same.
     */
    (void)nic_bridge_vertex_info(node,
		(nic_data_t)&HEART_PIU_K1PTR->h_mlan_ctl + 0x4);

    for (i = 0; i < maxcpus; i++)
	if (cpu_enabled(i)) {
	    sprintf(cpun, "%d", i);
	    rc = hwgraph_path_add(cpus, cpun, &cpuv);
	    ASSERT(rc == GRAPH_SUCCESS);
	    ASSERT(cpuv != GRAPH_VERTEX_NONE);
	    mark_cpuvertex_as_cpu(cpuv, i);
	    device_master_set(cpuv, node);
	    /* XXX- make use of cpuv's fastinfo ... ? */
#if DEBUG && HWG_DEBUG
	    cmn_err(CE_CONT, "%v cpu vertex set up\n", cpuv);
	} else if (i == master_procid) {
	    cmn_err(CE_CONT, "%v master CPU (%d) not enabled???\n",
		    cpus, master_procid);
#endif
	}
    /*
     * Add a vertex to represent the specific
     * hunk(s) of memory hanging off of heart.
     */
    memv = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(node, EDGE_LBL_MEMORY, &memv);
    ASSERT(rc == GRAPH_SUCCESS);
    ASSERT(memv != GRAPH_VERTEX_NONE);

        /* Set up scsi and scsi_ctlr */

	/* scsi vhdl */
	rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI, &scsi);
	ASSERT(rc == GRAPH_SUCCESS);

	/* scsi_ctlr vhdl */
	rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_SCSI_CTLR, &scsi_ctlr);
	ASSERT(rc == GRAPH_SUCCESS);

	/* disk vhdl */
	rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK, &disk);
	ASSERT(rc == GRAPH_SUCCESS);

#if DEBUG && HWG_DEBUG
    cmn_err(CE_CONT, "%v: physical memory vertex set up\n", memv);
#endif

    /*
     * Add a hint to the baseio node so the pcibr
     * driver can get the ioc3 set up correctly:
     * the IOC3 responding in CFG slot 2 on the
     * IP30 baseio bridge also uses the REQ/GNT
     * and INTA lines associated with slot 4.
     */
    rc = hwgraph_path_add(node, EDGE_LBL_XTALK "/15", &baseio);
    ASSERT(rc == GRAPH_SUCCESS);
    pcibr_hints_fix_rrbs(baseio);
    pcibr_hints_dualslot(baseio, 2, 4);
    pcibr_hints_subdevs(baseio, 2,
			IOC3_SDB_ETHER |
			IOC3_SDB_GENERIC |
			IOC3_SDB_NIC |
			IOC3_SDB_KBMS |
			IOC3_SDB_SERIAL |
			IOC3_SDB_ECPP);

    /*
     * Now that the graph has been annotated,
     * we can start attaching devices.
     */
#if HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT();
#endif
    heart_startup(hwgraph_root);
#if HWG_PERF_CHECK && !DEBUG
    cval = RAW_COUNT() - cval;
    hwg_hprint(cval, "heart_startup()");
#endif

    /*
     * The graph has been built;
     * add some convenience links
     * useful for this product.
     *
     */
    {
	vertex_hdl_t		vhdl;

	/*
	 * The devices on xtalk port 15 are collectively
	 * known as "baseio".
	 */
	hwgraph_link_add("node/xtalk/15", "", "baseio");

	/*
	 * The QL driver used to set up /hw/ql/%d as the
	 * canonical path to the QL controlers in PCI
	 * slots zero and one. Hard to do when it might
	 * be getting called on hundreds of different
	 * nodes in a large IP27, so we stop doing it
	 * there. We still do it here, until usage of
	 * the /hw/ql/1/target... names stops.
	 */
	hwgraph_link_add("baseio/pci/0/scsi", "ql", "0");	/* XXX COMPAT */
	hwgraph_link_add("baseio/pci/1/scsi", "ql", "1");	/* XXX COMPAT */

	/*
	 * Give some names to the QL controllers.
	 */
	vhdl = hwgraph_path_to_vertex("/hw/ql/0");
	if (vhdl != GRAPH_VERTEX_NONE)
	    device_controller_num_set(vhdl, 0);
	vhdl = hwgraph_path_to_vertex("/hw/ql/1");
	if (vhdl != GRAPH_VERTEX_NONE)
	    device_controller_num_set(vhdl, 1);

	/*
	 * Give a name to ef0.
	 */
	vhdl = hwgraph_path_to_vertex("/hw/baseio/pci/2/ef");
	if (vhdl != GRAPH_VERTEX_NONE) {
	    extern void		    ef_ifattach(void *ei, int unit);
	    void		   *ei;

	    device_controller_num_set(vhdl, 0);

	    /* Can return NULL if nic does not configure */
	    if (ei = (void *) hwgraph_fastinfo_get(vhdl))
		ef_ifattach((void *) ei, 0);	/* XXX */
	}
    }
#if HWG_PRINT
    {
	extern void		hwgraph_print(void);

	hwgraph_print();
    }
#endif

#if HWG_PERF_CHECK && !DEBUG
    {
	unsigned		s;
	unsigned		csrc[16], cdst[16];
	unsigned		one, two, delta;
	unsigned		cc_min, cc_max, cc_avg;
	unsigned		ch_min, ch_max, ch_avg;
	unsigned		n;
	extern void		dummy_func(void);
	extern void		chill_caches(void);

	if (!hwg_perf_countperms)
	    hwg_perf_calibrate();

	printf("\nHWG PERF CHECK\n");

	/* numbers are printed
	 * as "nn.nn " for coldcache,
	 * and "n.nn " for warmcache.
	 */
	printf("\t     coldcache    \t   warmcache   \toperation\n"
	       "\t  min   avg   max \t min  avg  max \t(times in usec)\n");

#define PERFCHECK(name, operation)					\
									\
	    DELAY(1000000);						\
	    cc_max = cc_avg = 0;					\
	    cc_min = ~0;						\
	    for (n=0; n<20; ++n) {					\
		s = spl7();						\
		chill_caches();						\
		one = _get_count();					\
		operation;						\
		two = _get_count();					\
		splx(s);						\
		delta = two - one;					\
		if (cc_min > delta) cc_min = delta;			\
		if (cc_max < delta) cc_max = delta;			\
		cc_avg += delta;					\
	    }								\
	    cc_avg += n/2;						\
	    cc_avg /= n;						\
									\
	    ch_max = ch_avg = 0;					\
	    ch_min = ~0;						\
	    for (n=0; n<100; ++n) {					\
		s = spl7();						\
		operation;						\
		one = _get_count();					\
		operation;						\
		two = _get_count();					\
		splx(s);						\
		delta = two - one;					\
		if (ch_min > delta) ch_min = delta;			\
		if (ch_max < delta) ch_max = delta;			\
		ch_avg += delta;					\
	    }								\
	    ch_avg += n/2;						\
	    ch_avg /= n;						\
									\
	    printf("\t"); pr22(cc_min); pr22(cc_avg); pr22(cc_max);	\
	    printf("\t"); pr12(ch_min); pr12(ch_avg); pr12(ch_max);	\
	    printf("\t%s\n", name);

	PERFCHECK("dummy function", dummy_func());
	PERFCHECK("64-byte bcopy", bcopy(csrc, cdst, 64));
	PERFCHECK("fastinfo_get", hwgraph_fastinfo_get(baseio));

	{	/* xxx move to a loadable xtalk driver */
	    vertex_hdl_t	    xconn;
	    xtalk_dmamap_t	    xtalk_dmamap;

	    xconn = hwgraph_path_to_vertex("/hw/baseio");

	    PERFCHECK("xwidget_info_get", xwidget_info_get(xconn));

	    xtalk_dmamap = xtalk_dmamap_alloc(xconn, 0, (1<<16), 0);
	    PERFCHECK("xtalk_dmamap_addr(4k)",
		      xtalk_dmamap_addr
		      (xtalk_dmamap, 0x20000000, 4096));
	    PERFCHECK("xtalk_dmamap_addr(64k)",
		      xtalk_dmamap_addr
		      (xtalk_dmamap, 0x20000000, (1<<16)));
	    xtalk_dmamap_free(xtalk_dmamap);

	    PERFCHECK("xtalk_dmatrans_addr(4k)",
		      xtalk_dmatrans_addr
		      (xconn, 0, 0x20000000, 4096, 0));
	    PERFCHECK("xtalk_dmatrans_addr(64k)",
		      xtalk_dmatrans_addr
		      (xconn, 0, 0x20000000, (1<<16), 0));
	}
	printf("\n");
	{	/* xxx move to a loadable pci driver */
	    vertex_hdl_t	    pconn;
	    pciio_dmamap_t	    pciio_dmamap;

	    pconn = hwgraph_path_to_vertex("/hw/baseio/pci/0");

	    PERFCHECK("pciio_info_get", pciio_info_get(pconn));

	    pciio_dmamap = pciio_dmamap_alloc(pconn, 0, (1<<16), 0);
	    PERFCHECK("pciio_dmamap_addr(4k)",
		      pciio_dmamap_addr
		      (pciio_dmamap, 0x20000000, 4096));
	    PERFCHECK("pciio_dmamap_addr(64k)",
		      pciio_dmamap_addr
		      (pciio_dmamap, 0x20000000, (1<<16)));
	    pciio_dmamap_free(pciio_dmamap);

	    pciio_dmamap = pciio_dmamap_alloc(pconn, 0, (1<<16), PCIIO_DMA_A64);
	    PERFCHECK("pciio_dmamap_addr(4k,A64)",
		      pciio_dmamap_addr
		      (pciio_dmamap, 0x20000000, 4096));
	    PERFCHECK("pciio_dmamap_addr(64k,A64)",
		      pciio_dmamap_addr
		      (pciio_dmamap, 0x20000000, (1<<16)));
	    pciio_dmamap_free(pciio_dmamap);

	    PERFCHECK("pciio_dmatrans_addr(4k)",
		      pciio_dmatrans_addr
		      (pconn, 0, 0x20000000, 4096, 0));
	    PERFCHECK("pciio_dmatrans_addr(64k)",
		      pciio_dmatrans_addr
		      (pconn, 0, 0x20000000, (1<<16), 0));

	    PERFCHECK("pciio_dmatrans_addr(4k,A64)",
		      pciio_dmatrans_addr
		      (pconn, 0, 0x20000000, 4096, PCIIO_DMA_A64));
	    PERFCHECK("pciio_dmatrans_addr(64k,A64)",
		      pciio_dmatrans_addr
		      (pconn, 0, 0x20000000, (1<<16), PCIIO_DMA_A64));
	}
	printf("\n");
	printf("HWG_PERF timer info: (~%ld MHz)\n"
	       "\thwg_perf_countperms: %ld\n"
	       "\thwg_perf_pspercount: %ld\n",
	       (hwg_perf_countperms + 250) / 500,
	       hwg_perf_countperms,
	       hwg_perf_pspercount);
    }
#endif
}

/* Send an interrupt to another CPU.
 */
/*ARGSUSED2*/
int
sendintr(cpuid_t destid, unchar status)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	ASSERT(status == DOACTION);
	heart_piu->h_set_isr = HEART_IMR_IPI_(destid);

	return 0;
}

/*
 * Cause all CPUs to stop by sending them each a DEBUG interrupt.
 */
/* ARGSUSED1 */
void
debug_stop_all_cpus(void *stoplist)
{
	heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
	int cpu;

	for (cpu=0; cpu<maxcpus; cpu++) {
		if (cpu == cpuid())
			continue;

		heart_piu->h_set_isr = HEART_IMR_DEBUG_(cpu);
	}
}

/* Timestamped fastick_maint().
 */
static void
tsfastick_maint(eframe_t *ep)
{
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_PROFCOUNTER_INTR, NULL, NULL,
			 NULL, NULL);

	fastick_maint(ep);

	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 TSTAMP_EV_PROFCOUNTER_INTR, NULL, NULL, NULL);
}

/*
 * IP30 interrupt routing: examine the bits in the cause register, and
 * trigger the proper interrupt service routine. Some cause bits go
 * directly to a service, others get routed to the Heart code for
 * further processing.
 */

typedef void (*intvec_func_t)(struct eframe_s *);

struct intvec_s {
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		ipmsk;		/* corresponding bit off in cause */
	uint		swibit;		/* software intr bit to clear */
};

extern	void heart_intr_low(struct eframe_s *);
extern	void heart_intr_med(struct eframe_s *);
extern	void heart_intr_hi(struct eframe_s *);
extern	void heart_intr_err(struct eframe_s *);
extern	void counter_intr(struct eframe_s *);
extern	void timein(void);
extern  void clock(struct eframe_s *);	/* really returns int */

/* Keep poker for soft clocks away from timein().
 */
#if DEBUG
unsigned int softpoke[MAXCPU];
#endif
/*ARGSUSED*/
void
pokesoftclk(struct eframe_s *ep)
{
#if DEBUG
	softpoke[cpuid()]++;
#endif
	siroff(CAUSE_SW2);
	return;
}

#define CAUSE_IP9	(CAUSE_IP8 << 1)	/* ~IP5 sw clock intr */
#define CAUSE_IP10	(CAUSE_IP8 << 2)	/* ~IP6 sw prof intr */

/*
 * C0 interrupt vector table
 *
 * Indexed by the PRIORITY of the interrupt,
 * gives back a function to call and bit masks
 * for the ISR (for spl) and for the cause register.
 */
static struct intvec_s c0vec_tbl[] = {
	0,	    		0,			0,		 0,
	(intvec_func_t)
	timein,  	SR_SCHED_MASK|SR_IEC,	~SR_SCHED_MASK&SR_IMASK, 0,
	pokesoftclk,   	SR_SCHED_MASK|SR_IEC,	~SR_SCHED_MASK&SR_IMASK, 0,
	heart_intr_low,	SR_SCHED_MASK|SR_IEC,	~SR_SCHED_MASK&SR_IMASK, 0,
	heart_intr_med,	SR_SCHED_MASK|SR_IEC,	~SR_SCHED_MASK&SR_IMASK, 0,
	heart_intr_hi, 	SR_SCHED_MASK|SR_IEC,	~SR_SCHED_MASK&SR_IMASK, 0,
	clock, SR_SCHED_MASK|SR_IEC, ~SR_SCHED_MASK&SR_IMASK, CAUSE_IP9,
	(intvec_func_t)
	heartclock_intr,SR_PROF_MASK|SR_IEC,	~SR_PROF_MASK&SR_IMASK,	 0,
	tsfastick_maint,SR_PROF_MASK|SR_IEC,	~SR_PROF_MASK&SR_IMASK,
			CAUSE_IP10,
	counter_intr,	SR_PROF_MASK|SR_IEC,	~SR_PROF_MASK&SR_IMASK,  0,
	heart_intr_err,	SR_BERR_MASK|SR_IEC,	~SR_BERR_MASK&SR_IMASK,  0
};

/*
 * Find first interrupt for Cause register
 *
 * NOTE: the priority ordering of the bits in the cause register is almost
 * but not * quite the same as the bit ordering; so locating the "highest
 * priority" bit in the cause register is not the same as finding the most
 * significant bit.
 *
 * It is also somewhat contrived with the two software levels inserted.
 *
 * See ml/RACER/spl.s for more information.
 */

static char ffintrctbl[2][32] = {
	{ 0, 7,10,10, 9, 9,10,10, 6, 7,10,10, 9, 7,10,10,
	  8, 7,10,10, 9, 7,10,10, 8, 7,10,10, 9, 7,10,10 },
	{ 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 }
};

#ifdef DEBUG
/* count which interrupts we are getting;
 * indexes 0..7 correspond to priorities 1..8 in C0.
 */
unsigned int intrcnt[MAXCPU][10];	/* up to 10 for SW 9..10 */
unsigned int intrcause[MAXCPU][10];	/* Saved clock cause */
int intrcausei[MAXCPU];
#endif /* DEBUG */

/* Called from intr() and before ithread_become() [heart/ioc3.c].
 */
#define p_checktlb	p_led_value	/* unused on RACER */
void
ip30_intr_maint(void)
{
#if DEBUG
	cpuid_t cpu = cpuid();
	int i = atomicAddInt(&intrcausei[cpu],0);	/* atomic read */
	ASSERT(i>=0);
	ASSERT(i<10);
	ASSERT((intrcause[cpu][i] & (CAUSE_IP9|CAUSE_IP10)) == 0);
	atomicAddInt(&intrcausei[cpu],-1);		/* decrement */
#endif

	/* If we cannot service the clock now, make sure we get back to
	 * intr eventually by setting a SW interrupt.
	 */
	if (private.p_clock_ticked || private.p_fclock_ticked)
		siron(CAUSE_SW2);
}

/*
 * intr - handle 1st level interrupts
 *
 * Handle interrupts as specified in the cause
 * register, in priority order.
 */
/*ARGSUSED2*/
int
intr(struct eframe_s *ep, uint code, uint sr, uint cause)
{
	int		cub;		/* cause upper bits */
	unsigned int    pri;            /* int pri, 1..8 + SW 9..10*/
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		ipmsk;		/* interrupts blocked */
	int		check_kp = 0;	/* kernel preemption possible */
	int 		check_exit = 0;
	uint		swcause = 0;	/* software IP9/10 cause */
	uint		swibit;		/* software IP9/10 bit to clear */
#if DEBUG
	int		intri = atomicAddInt(&intrcausei[cpuid()],1);
#endif

	/*
	 * If was isolated then if we're from user mode must do delay action.
	 * This tries to cover the fact we can't pass the state up to the
	 * interrupt handler that does ithread_become(), and should cover
	 * us pretty well.
	 */
	CHECK_DELAY_TLBFLUSH_INTR(ENTRANCE, check_exit);

        LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTRENTRY,NULL,NULL,NULL,NULL);

	/* keep only interrupt mask bits from SR */
	sr &= SR_IMASK;

	/*
	 * While we have enabled interrupt bits in the cause register,
	 * peel and process the highest priority one.
	 *
	 * The loop is a bit tricky to handle soft clock interrupts,
	 * very reminiscent of IP22.
	 */
	while (1) {
		/* Check for SW clock interrupts.  We check spl a on IP5/6
		 * to maintain propper spl levels.
		 */
		if ((private.p_fclock_ticked && (sr & SR_IBIT6)) ||
		    (private.p_clock_ticked && (sr & SR_IBIT5))) {
			int s = splprof();
			if (!(swcause & CAUSE_IP9) &&
			     private.p_clock_ticked &&
			     (sr & SR_IBIT5)) {
				private.p_clock_ticked--;
				swcause |= CAUSE_IP9;
			}
			if (!(swcause & CAUSE_IP10) &&
			     private.p_fclock_ticked &&
			     (sr & SR_IBIT6)) {
				private.p_fclock_ticked--;
				swcause |= CAUSE_IP10;
			}
			/* Stay at splprof for [f]clock interrupts to avoid
			 * taking IP8 and having races on p_[f]_clock_ticked.
			 */
			if ((swcause & (CAUSE_IP9|CAUSE_IP10)) == 0)
				splx(s);
		}

		cause &= sr;
		cause |= swcause;

#if DEBUG	/* Stow cause for checking clock status in ithread code */
		intrcause[cpuid()][intri] = cause;
#endif

		if (!cause)
			break;

		cub = cause >> (4+CAUSE_IPSHIFT+1);	/* 5 bits */
		cause >>= CAUSE_IPSHIFT;
		pri = cub ? ffintrctbl[0][cub] : ffintrctbl[1][cause];
#ifdef DEBUG
		intrcnt[cpuid()][pri-1]++;
#endif
		isr = c0vec_tbl[pri].isr;
		msk = c0vec_tbl[pri].msk;
		ipmsk = c0vec_tbl[pri].ipmsk;
		swibit = c0vec_tbl[pri].swibit;

		/* enable higher priority ints while handling this one.
		 * any higher priority interrupts will be handled by
		 * a re-entrance into intr, which will not handle interrupts
		 * at or below our "pri" since those would be disabled in
		 * the SR that it sees, so we should not get reentrancy
		 * induced priority inversions. Also, disabling the
		 * interrupts after we return probably has no performance
		 * benefit: it would only help if the next interrupt was
		 * higher priority, and came in after we would have disabled
		 * interrupts but before we sampled the cause register.
		 */
		splx(msk);
		(*isr)(ep);

		if (msk == (SR_SCHED_MASK|SR_IEC))
			check_kp = 1;

		if (swibit)
			swcause &= ~swibit;
		else
			/* each time to as anon ithreads do not return */
			atomicAddInt((int *)&SYSINFO.intr_svcd,1);

		/* additional interrupts at or below our priority
		 * may have been posted while we were dealing with
		 * that service routine; re-read the cause register
		 * to pick them up. Failure to do this would not be
		 * fatal, the ints would still be handled in the
		 * right order as we lower spl across them or
		 * return from the interrupt.  need to AND with ipmsk
		 * since it's possible to get the new cause with a higher
		 * level interrupt bit set right before taking the
		 * interrupt, after coming back from the interrupt handler
		 * the higher level interrupt bit in the local variable will
		 * still be set.
		 */
		cause = getcause() & ipmsk;
	}

	ip30_intr_maint();

        /* Should be ok with nested interrupts */
        /* XXX really should delay until back to user mode? */
        CHECK_DELAY_TLBFLUSH_INTR(EXIT, check_exit);

       	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTREXIT,TSTAMP_EV_INTRENTRY,NULL,NULL,NULL);

	/*
	 * At this point, if we handled any interrupts, we will
	 * be leaving interrupts above the level of the last one
	 * we handled, and the global int enable, enabled.
	 */

#ifdef SPLMETER
	 if (check_kp)
		_cancelsplhi();
#endif
	 return check_kp;
}

/*
 * intr_spray_heuristic()
 *
 * Choose an interrupt destination for an interrupt.
 * Currently, we just spread out the non-redirected interrupts in a
 * round-robin fashion. Arbitrary heuristics can be incorporated here.
 */
#if MAXCPU > 2
#error "needs porting to large number of CPUs"
#endif
cpuid_t
intr_spray_heuristic(device_desc_t dev_desc)
{
	static int cpu_intrs[2] = {0, 0};
	vertex_hdl_t intr_target_dev;
	cpuid_t cpu;

	if (dev_desc &&
	    (intr_target_dev = device_desc_intr_target_get(dev_desc)) !=
		GRAPH_VERTEX_NONE) {
		/* A valid device was specified. If it's a particular CPU,
		 * then use that CPU as target.
		 */
		cpu = cpuvertex_to_cpuid(intr_target_dev);
		ASSERT((cpu == 0) || (cpu == 1));
		/*
		 * Master_procid is always available. Fallback to it in
		 * case CPU 1 is requested and not available
		 */
		if (!cpu_enabled(cpu))
			cpu = master_procid;
	} else {
		/* Try to distribute the interrupts equally */
		if (cpu_intrs[0] <= cpu_intrs[1])
			cpu = 0;
		else
			cpu = 1;

		/*
		 * Master_procid is always available. Fallback to it in
		 * case CPU 1 cannot be used (either because it is not
		 * enabled or because it is restricted).
		 */
		if ((!cpu_enabled(cpu)) || (!cpu_allows_intr(cpu)))
			cpu = master_procid;
	}

	cpu_intrs[cpu] ++;
	return (cpu);
}

#define __DEVSTR1 "/hw/ql/"
#define __DEVSTR2 "/target/"
#define __DEVSTR3 "/lun/0/disk/partition/"
void
devnamefromarcs(char *devnm)
{
        int val;
        char tmpnm[MAXDEVNAME];
        char *tmp1, *tmp2;

        val = strncmp(devnm, "dks", 3);
        if (val == 0) {
                tmp1 = devnm + 3;
                strcpy(tmpnm, __DEVSTR1);
                tmp2 = tmpnm + strlen(__DEVSTR1);
                while(*tmp1 != 'd') {
                        if((*tmp2++ = *tmp1++) == '\0')
                                return;
                }
                tmp1++;
                strcpy(tmp2, __DEVSTR2);
                tmp2 += strlen(__DEVSTR2);
                while (*tmp1 != 's') {
                        if((*tmp2++ = *tmp1++) == '\0')
                                return;
                }
                tmp1++;
                strcpy(tmp2, __DEVSTR3);
                tmp2 += strlen(__DEVSTR3);
                while (*tmp2++ = *tmp1++)
                        ;
                tmp2--;
                *tmp2++ = '/';
                strcpy(tmp2, EDGE_LBL_BLOCK);
                strcpy(devnm,tmpnm);
        }
}

#ifdef HEART_COHERENCY_WAR
/* First set is for read flushing, _write_ version for write flushing.
 * Currently (rev A and rev B) the are really the same, but for testing
 * one can tweak either way.
 */
void
heart_dcache_wb_inval(void *addr, int len)
{
	if (heart_read_coherency_war)
		cache_operation(addr,len,CACH_DCACHE|
			CACH_WBACK|CACH_INVAL|CACH_IO_COHERENCY|CACH_FORCE);
}

void
heart_dcache_inval(void *addr, int len)
{
	if (heart_read_coherency_war)
		cache_operation(addr,len,CACH_DCACHE|
				CACH_INVAL|CACH_IO_COHERENCY|CACH_FORCE);
}

void
heart_write_dcache_wb_inval(void *addr, int len)
{
	if (heart_write_coherency_war)
		cache_operation(addr,len,CACH_DCACHE|
			CACH_WBACK|CACH_INVAL|CACH_IO_COHERENCY|CACH_FORCE);
}

void
heart_write_dcache_inval(void *addr, int len)
{
	if (heart_write_coherency_war)
		cache_operation(addr,len,CACH_DCACHE|
				CACH_INVAL|CACH_IO_COHERENCY|CACH_FORCE);
}

/*  Return if cache flushing is needed or not for various heart wars.
 * If cpuread == 1 it means the cpu is reading this buffer, or a IO
 * to memory write.
 */
int
heart_need_flush(int cpuread)
{
	return cpuread ? heart_write_coherency_war : heart_read_coherency_war;
}
#endif	/* HEART_COHERENCY_WAR */

#ifdef HEART_INVALIDATE_WAR
#include <sys/pfdat.h>
#include <sys/rt.h>

void
heart_invalidate_war(void *addr, int len)
{
	void *old_rtpin;
	int s;

	if (_heart_invalidate_war == 0)
		return;

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		old_rtpin = rt_pin_thread();
	else
		s = splhi();

	__heart_invalidate_war(addr,len);

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		rt_unpin_thread(old_rtpin);
	else
		splx(s);
}

/*
 * bp_heart_invalidate_war - code copied from bp_dcache_wbinval, must keep
 * in sync with it!  Do not invalidate the whole cache, must always be hit
 * based.
 *
 * Needed for the QL driver.
 */
void
bp_heart_invalidate_war(buf_t *bp)
{
	struct pfdat *pfd;
	int npgs;
	void *old_rtpin;
	int s;

	if (_heart_invalidate_war == 0)
		return;

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		old_rtpin = rt_pin_thread();
	else
		s = splhi();

	if (BP_ISMAPPED(bp)) {
		__heart_invalidate_war(bp->b_un.b_addr,bp->b_bcount);
		return;
	}

	if ((bp->b_flags & B_UNCACHED) == 0) {
		pfd = bp->b_pages;
		npgs = btoc(bp->b_bufsize);

		while (npgs--) {
			__heart_invalidate_war(
				(void *)PHYS_TO_K0(pfdattophys(pfd)),NBPP);
			pfd = pfd->pf_pchain;
		}
	}

	if (curthreadp && !(private.p_kstackflag != PDA_CURINTSTK))
		rt_unpin_thread(old_rtpin);
	else
		splx(s);
}
#endif	/* HEART_INVALIDATE_WAR */

/* Silences all RAD */

void (*rad_halt_func)(void);

void
ip30_rad_halt(void)
{
	/* the rad driver need to set rad_halt_func */
	if (rad_halt_func)
		(*rad_halt_func)();
}

extern cpu_waitforreset();
void *ip30__x = (void *)cpu_waitforreset;


/* keyboard bell for octane lx */
static toid_t			keybd_bell_timeout_id;
static volatile ioc3reg_t	*ioc3_gpcr_c = (volatile ioc3reg_t *)
					(IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_C);
static volatile ioc3reg_t	*ioc3_gpcr_s = (volatile ioc3reg_t *)
					(IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_S);

/*ARGSUSED*/
static void
stop_bell(int arg)
{
	int s = splhi();	/* ok since octane lx is uni-processor */

	*ioc3_gpcr_c = GPCR_INT_OUT_EN;
	keybd_bell_timeout_id = 0;

	splx(s);
}

/*ARGSUSED*/
int
pckbd_bell(int volume, int pitch, int duration)
{
	int s;

	ASSERT(is_octane_lx);

	if (duration <= 0)
		return 0;

	s = splhi();		/* ok since octane lx is uni-processor */
	if (!keybd_bell_timeout_id) {
		*ioc3_gpcr_s = GPCR_INT_OUT_EN;
		keybd_bell_timeout_id = timeout(stop_bell, 0,
			(duration * HZ) >> 10);
	}
	splx(s);

	return 1;
}
