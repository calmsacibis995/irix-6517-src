#ident	"IP30prom/IP30.c:  $Revision: 1.111 $"

/*
 * IP30.c -- machine dependent prom routines
 */

#include <sys/types.h>
#include <setjmp.h>
#include <sys/cpu.h>
#include <sys/RACER/racermp.h>
#include <sys/RACER/IP30nvram.h>
#include <sys/RACER/IP30addrs.h>
#include <sys/sbd.h>
#include <sys/RACER/gda.h>
#include <sys/nic.h>
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <gfxgui.h>
#include <style.h>
#include <alocks.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/RACER/sflash.h>

#include <sys/mgrashw.h>		/* for system command */
#include <sys/xtalk/hq4.h>

lock_t arcs_ui_lock;                    /* in bss! */

extern jmp_buf restart_buf;
extern int Verbose, Debug;
extern __uint64_t memsize;

extern void cache_K0(void);
extern void config_cache(void);
extern void hmclear(__psunsigned_t, __psunsigned_t);
extern void init_dcache(void);
extern void init_icache(void);
extern void init_scache(void);
extern void init_tlb(void);
extern void main(void);
extern void setstackseg(__psunsigned_t);
extern void startup(void);
extern void ip30_init_cpu_disable(void);

#ifndef IP30_RPROM
extern int pon_scache(void);
extern int pon_caches(void);
extern int pon_morediags(void);
extern int pon_audio_wait(void);
#else
#define pon_scache()	0
#define pon_caches()	0
#define	pon_morediags() 0
#define play_hello_tune(x)
#define wait_hello_tune()
#define pon_audio_wait() 0
#endif

static int cachepromtext(void);

void init_caches(void);
void init_prom_soft(int);

static void alloc_memdesc(void);
#ifdef ENETBOOT
static void clear_memory(int);
#endif
static void domfgstartup(char *);
static void init_gda(void);
static int slave_processor_spin(int);
static int wakeup_slave_processors(uint_t, int);

static volatile unsigned short cpumask;
extern int master_cpuid;
int cache0_diag_failed[MAXCPU];
int cache1_diag_failed[MAXCPU];
static int cache_diags_done[MAXCPU];
static int cache_diags_started[MAXCPU];

#define	CACHE_DIAG_IN_PROGRESS	-1
#define	CACHE_DIAG_DONE		1
#define	CACHE_DIAG_DONE		1

#ifdef VERBOSE
#define	VPRINTF(arg)	printf arg
#define VPUTS(str)	pon_puts(str)
#define VCPUPUTS(cpu,str)	\
	pon_puts("CPU ");	\
	pon_putc(cpu + '0');	\
	pon_puts(str)
#
#else
#define	VPRINTF(arg)
#define VPUTS(str)
#define VCPUPUTS(cpu,str)
#endif

#define CPUBOARD 	"IP30"

#ifdef ENETBOOT
#define ENET_SR_BEV	0
#else
#define ENET_SR_BEV	SR_BEV
#endif

#define	PROCESSOR_IS_INSTALLED(x)	(heart_piu->h_status &		\
				 	 HEART_STAT_PROC_ACTIVE(x))
#define	PROCESSOR_IS_DISABLED(x)	(heart_piu->h_mode &		\
				 	 HM_PROC_DISABLE(x))
#define	PROCESSOR_IS_ACTIVE(x)		(PROCESSOR_IS_INSTALLED(x) &&	\
					 !PROCESSOR_IS_DISABLED(x))
#define	PROCESSOR_IS_BAD(x)		(k1_cache0_diag_failed[x] ||	\
					 k1_cache1_diag_failed[x])

libsc_private_t *_libsc_private;
libsc_private_t ip30_tmp_libsc_private;

/*
 * sysinit - finish system initialization and run diags
 */
void
sysinit(void)
{
	int	 		diagsfailed;
	u_char			fan_setting;
	register heart_piu_t	*heart_piu = HEART_PIU_K1PTR;
	int	 		i;
	int	 		id = cpuid();
	volatile int		*k1_cache_diags_done =
					(int *)K0_TO_K1(cache_diags_done);
	volatile int 		*k1_cache0_diag_failed = 
					(int *)K0_TO_K1(cache0_diag_failed);
	volatile int 		*k1_cache1_diag_failed = 
					(int *)K0_TO_K1(cache1_diag_failed);
	volatile int		*k1_cache_diags_started =
					(int *)K0_TO_K1(cache_diags_started);
	int			*k1_master_cpuid =
					(int *)K0_TO_K1(&master_cpuid);
	volatile mpconf_t	*mpconf_ptr;
	char	 		*ptr;
	int			running_cached = 0;
#ifdef IP30_RPROM
	volatile __psunsigned_t *rprom_slave =
		(volatile __psunsigned_t *)PHYS_TO_K1(IP30_RPROM_SLAVE);
#endif
#ifdef ENETBOOT
	extern int		start();

	run_uncached();		/* needed for the cache diags */
#endif


	/*
	 * initially, processor with the smallest id is the master processor
	 */
	if (!(heart_piu->h_status & HEART_STAT_PROC_ACTIVE_MSK &
	      (HEART_STAT_PROC_ACTIVE(id) - 1))) {

		*k1_master_cpuid = id;
		VCPUPUTS(id," is the bootmaster.\r\n");

#ifdef IP30_RPROM
		*rprom_slave = 0;
		if (flash_fprom_reloc_chk()) {
			extern void reloc_to_fprom(void);

#ifndef ENETBOOT
			/* relaunch all slaves to fprom */
			*rprom_slave = (__psunsigned_t)reloc_to_fprom;
			heart_piu->h_mode &= ~HM_PROC_DISABLE_MSK;
			wakeup_slave_processors(0xf, 0);
		
			VPUTS("RPROM relocating to FPROM\r\n");
			reloc_to_fprom();
#else
			VPUTS("DRPROM, Not relocating to FPROM\r\n");
#endif
		} else
			pon_puts("FPROM Invalid, Starting Recovery\r\n");
#endif	/* IP30_RPROM */

		init_caches();
		k1_cache1_diag_failed[id] = pon_scache();
		k1_cache0_diag_failed[id] = pon_caches();

		for (i = 0; i < MAXCPU; i++) {
			heartreg_t	timeout;

			if (i == id || !PROCESSOR_IS_INSTALLED(i))
				continue;

			heart_piu->h_mode &= ~HM_PROC_DISABLE(i);
			flushbus();
			wakeup_slave_processors(0x1 << i, 0);

			/* 1 second to timeout, 12.5MHz clock */
			timeout = heart_piu->h_count + 12500000;
			while (!k1_cache_diags_started[i] &&
			       heart_piu->h_count < timeout)
				;

			if (!k1_cache_diags_started[i]) {
				heart_piu->h_mode |= HM_PROC_DISABLE(i);
				VCPUPUTS(i," timed out waiting to start the "
					"cache diagnostic.\r\n");
				continue;
			}

			/*
			 * wait for cache diagnostics to finish, no
			 * timeout here since the slave processor may
			 * output lot of error message
			 */
			while (!k1_cache_diags_done[i])
				;

#if MAXCPU > 2
			/*
			 * disable slave processor to save some bus
			 * bandwidth
			 */
			heart_piu->h_mode |= HM_PROC_DISABLE(i);
#endif

			/*
			 * switch master processor if the current master
			 * processor failed the cache diagnostics
			 */
			if (PROCESSOR_IS_BAD(*k1_master_cpuid) &&
			    !PROCESSOR_IS_BAD(i)) {
				VCPUPUTS(i," is now the bootmaster due to a "
					"cache diagnostic failure.\r\n");
				*k1_master_cpuid = i;
			}
		}
#if MAXCPU > 2
		heart_piu->h_mode &= ~HM_PROC_DISABLE_MSK;
#endif
		wakeup_slave_processors(0xf, 0);
	}
	else {
		/* clear own inter-processor interrupt */
		heart_piu->h_clr_isr = HEART_ISR_IPI;

		init_caches();

		k1_cache_diags_started[id] = 1;
		k1_cache1_diag_failed[id] = pon_scache();
		k1_cache0_diag_failed[id] = pon_caches();
		k1_cache_diags_done[id] = 1;

		(void)slave_processor_spin(0);
	}

	/* possible new bootmaster */
	if (id != *k1_master_cpuid) {
		(void)slave_processor_spin(0);
	}
	else {
		/* XXX may want to do more sensible thing */
		if (PROCESSOR_IS_BAD(id)) {
			void pon_flash_led(void);

			pon_puts("PANIC: None of the processor(s) passed "
				 "the cache diagnostics.\r\n");
			pon_flash_led();
			/*NOTREACHED*/
		}

		/* disable slave processors to stop bus contention */
		heart_piu->h_mode |=
			HM_PROC_DISABLE_MSK ^ HM_PROC_DISABLE(id);

		VPUTS("Caching the rest of the PROM [");
		VPUTS(IS_KSEG1(sysinit) || IS_COMPATK1(sysinit) ?
			"uncached" : "cached");
		VPUTS(" prom].\r\n");

		heart_piu->h_mode |= HM_CACHED_PROM_EN;
		if (cachepromtext()) {
			cache_K0();
#ifdef ENETBOOT
			run_cached();
#endif
		}
		setstackseg(K0BASE);
		running_cached = 1;

		/* since we want to use printf early */
		_libsc_private = &ip30_tmp_libsc_private;
		initsplocks();
		initlock(&arcs_ui_lock);

		/* set cache related global variables */
		config_cache();

		/* Enable power switch before memory clear, which is slow.
		 * Need to make sure h_imrs are cleared before initial call
		 * as it does enable heart interrupts.
		 */
		VPRINTF(("Enabling power switch.\r\n"));
#ifndef ENETBOOT
		/* Initialize heart/bridge base interrupt stuff just enough */
		heart_piu->h_imr[0] = 0;
		heart_piu->h_imr[1] = 0;
		heart_piu->h_imr[2] = 0;
		heart_piu->h_imr[3] = 0;
		heart_piu->h_mode |= HM_INT_EN;
		BRIDGE_K1PTR->b_wid_int_upper = INT_DEST_UPPER(9); /*bogus vec*/
		BRIDGE_K1PTR->b_wid_int_lower = INT_DEST_LOWER;
#endif
		ip30_setup_power();
		set_SR(get_SR() | SR_IE);

		VPRINTF(("Initializing the rest of memory.\r\n"));
#define	PHYS_TO_UNCACHED_ACC(x)	((__psunsigned_t)(x)|0xb800000000000000)

#if ENETBOOT
#ifdef SYMMON
#define ZERO_BASE	(PHYS_RAMBASE+0x180000)		/* 1.5MB for symmon */
#else
#define	ZERO_BASE	(PHYS_RAMBASE+SYSTEM_MEMORY_ALIAS_SIZE)
#endif	/* SYMMON */
		bzero((void *)PHYS_TO_K1(ZERO_BASE),
		      (int)(KDM_TO_PHYS(start) - ZERO_BASE));
#endif	/* ENETBOOT */

#if !ENETBOOT
		hmclear(PHYS_TO_UNCACHED_ACC(0x0),
			PHYS_TO_UNCACHED_ACC(SYSTEM_MEMORY_ALIAS_SIZE));
		hmclear(PHYS_TO_UNCACHED_ACC(PHYS_RAMBASE+SYSTEM_MEMORY_ALIAS_SIZE),
			PHYS_TO_UNCACHED_ACC(KDM_TO_PHYS(PROM_BSS)));
		hmclear(PHYS_TO_UNCACHED_ACC(KDM_TO_PHYS(PROM_STACK)),
			PHYS_TO_UNCACHED_ACC(PHYS_RAMBASE+memsize));
#endif	/* !ENETBOOT */

		init_gda();
		VPRINTF(("Initializing exception handlers.\r\n"));
		_hook_exceptions();		/* setup exception vectors */
		set_SR(get_SR() & ~SR_BEV);	/* use our vectors now */

		VPRINTF(("Initializing IP30 ASICs.\r\n"));
		MPCONF->fanloads = 0;		/* for ENETBOOT */
		init_ip30_chips();

		VPRINTF(("Initializing Status Register.\r\n"));
		set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR | SR_POWER);

		/*
	 	 * nvram variable override our own selection only if it
		 * select a good processor
	 	 */
		VPRINTF(("Initializing environment.\r\n"));
		init_env();

		ptr = getenv("bootmaster");
		if (ptr) {
			i = *ptr - '0';
			if (i >= 0 && i < MAXCPU && PROCESSOR_IS_INSTALLED(i) &&
			    !PROCESSOR_IS_BAD(i))
				master_cpuid = i;
		}

		GDA->g_masterspnum = master_cpuid;	/* reset master CPU */

		/* Read cpumask while code is single threaded */
		ptr = cpu_get_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE);
		cpumask = (*ptr << 8) | *(ptr+1);
		if ((cpumask & CPU_DISABLE_MAGIC_MASK) != CPU_DISABLE_MAGIC)
			cpumask = 0;		/* ignore bogus magic */


		if (id != master_cpuid) {
			VPRINTF(("CPU %d is the new bootmaster.\r\n",
				master_cpuid));

			/* Need to flush master CPUs .bss, and stop running
			 * cached if we are going to switch.  We will be
			 * cached again shortly.
			 */
			running_cached = 0;
			uncache_K0();
#ifdef ENETBOOT
			run_uncached();
#endif
			flush_cache();
		}

		for (i = 0; i < MAXCPU; i++) {
			if (!PROCESSOR_IS_INSTALLED(i) || PROCESSOR_IS_BAD(i))
				continue;

			heart_piu->h_mode &= ~HM_PROC_DISABLE(i);
			wakeup_slave_processors(0x1 << i, 0);
		}
	}

	if (!running_cached) {		/* really means bootmaster switched */
		if (cachepromtext()) {
			cache_K0();
#ifdef ENETBOOT
			run_cached();
#endif
		}
		setstackseg(K0BASE);

		/* this assumes we can have a maximum of 2 processors */
		if (id == master_cpuid) {
			heartreg_t	master_imr;
			int		old_master = id ^ 0x1;

			master_imr = heart_piu->h_imr[old_master];
			master_imr ^= HEART_IMR_BERR_(old_master);

			heart_piu->h_imr[id] = master_imr |
				HEART_IMR_BERR_(id);
			heart_piu->h_imr[old_master] =
				HEART_IMR_BERR_(old_master);
		}
	}

	if (id == master_cpuid)
		set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR | SR_POWER);
	else
		set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR);

	init_tlb();

	/* set up the MPCONF block */
	mpconf_ptr = (volatile mpconf_t *)
		(MPCONF_ADDR + MPCONF_SIZE * id);
	mpconf_ptr->pr_id = get_prid();
	mpconf_ptr->phys_id = mpconf_ptr->virt_id = id;
	mpconf_ptr->scache_size = ((r4k_getconfig() & CONFIG_SS) >>
		CONFIG_SS_SHFT) + CONFIG_SCACHE_POW2_BASE;
	mpconf_ptr->mpconf_magic = MPCONF_MAGIC;

	/* slave goes into slave idle loop or are disabled */
	if (id == master_cpuid) {
		/* check if any slaves should be disabled */
		for (i = 0; i < MAXCPU; i++) {
			if ((i != id) && cpumask & (1<<i)) {
				VCPUPUTS(i," disabled by disable cmd.\r\n");
				heart_piu->h_mode |= HM_PROC_DISABLE(i);
			}
		}
	}
	else
		slave_loop();

	VPRINTF(("Playing startup tune.\r\n"));
	play_hello_tune(0);		/* boot tune */

	VPRINTF(("Executing remainder of diagnostic.\r\n"));
	diagsfailed = pon_morediags();	/* run more diags  */

#ifndef IP30_RPROM
	/* Fan control stuff, done after pon_morediags() as that is where
	 * gfx is counted.
	 */
	ptr = getenv("fastfan");
	if (ptr && (*ptr == '1' || *ptr == 'y')) {
		VPRINTF(("Enabling fast fan via environment.\r\n"));
		MPCONF->fanloads += 1000;
		fan_setting = FAN_SPEED_HI;
	}
	else if (MPCONF->fanloads <= 1) {
		/* single headed SI/SE gfx with no additional XIO device */
		VPRINTF(("Lowering fan speed via config.\r\n"));
		fan_setting = FAN_SPEED_LO;
	}
	else if (!badaddr((volatile void *)(K1_MAIN_WIDGET(9)+WIDGET_ID), 4)) {
		/* quadrant D is occupied, enable fast fan */
		VPRINTF(("Enabling fast fan via config.\r\n"));
		MPCONF->fanloads += 1000;
		fan_setting = FAN_SPEED_HI;
	}
	else
		fan_setting = 0x0;
	*IP30_VOLTAGE_CTRL = fan_setting |
		PWR_SUPPLY_MARGIN_LO_NORMAL | PWR_SUPPLY_MARGIN_HI_NORMAL;
#endif	/* IP30_RPROM */

	VPRINTF(("Wait for tune.\r\n"));
	if (pon_audio_wait())
	    diagsfailed |= FAIL_AUDIO;

	/* clear memory cached unless diag fails */
#ifdef ENETBOOT
	VPRINTF(("Clearing memory.\r\n"));
	clear_memory(diagsfailed & FAIL_CACHES);
#endif

	VPRINTF(("Initializing software and devices.\r\n"));
	init_prom_soft(1);

	VPRINTF(("All initialization and diagnostic completed.\r\n"));

#ifndef IP30_RPROM
	if (diagsfailed) {
		/* Continue the Hook for GE Medical we did for I2
		 * This lets the system not stop for user intervention
		 *  when power-on diags fail
		 *
		 * diagmode=dc (diags continue) triggers this
		 */

		char *dm = getenv("diagmode");

		if (dm && (strstr(dm,"dc"))) {

			printf("\nDiagnostics failed.\nContinuing with diagmode=dc.");
			startup();      /* no need to check diagmode=mfg */
		} else {

			setTpButtonAction(main, TPBCONTINUE, WBNOW);
			printf("\nDiagnostics failed.\n[Press any key to continue.]");
			getchar();
			main();
		}
	}
	else {
		char *dm = getenv("diagmode");

		if (dm && (strstr(dm,"mfg"))) {
			domfgstartup(dm);
			main();
		} else {
			startup();
		}
	}
#else	/* IP30_RPROM */
	main();
#endif	/* !IP30_RPROM */
}

#ifndef IP30_RPROM
static void
domfgstartup(char *mode)
{
	char *p = getenv("idebinary");
	char bootfile[128];
	char *cmdv[4];
	int cmdc;
	int i;

	if (!p) return;

	strcpy(bootfile,"bootp()");
	strcat(bootfile,p);

	cmdc = 3;
	cmdv[0] = "boot";
	cmdv[1] = "-f";
	cmdv[2] = bootfile;
	cmdv[3] = 0;

	printf("mfg startup: ");
	for (i = 0; i < cmdc; i++) {
		puts(cmdv[i]);
		putchar(' ');
	}
	putchar('\n');
	p_curson();

	for (i = 0; i < 500; i++)
		if (GetReadStatus(0) == ESUCCESS)
			return;
		else
			us_delay(1000);

	Verbose = 1;

	boot(cmdc, cmdv, 0, 0);

	setTpButtonAction(main, TPBCONTINUE, WBNOW);
	printf("\nMfg auto boot failed.\n[Press any key to continue.]");
	getchar();
	return;
}
#endif	/* !IP30_RPROM */

/* 
 * init_prom_soft - configure prom software, only master processor should
 * call this routine
 */
void
init_prom_soft(int no_init_env)
{
	/* slave processor only */
	if (cpuid() != GDA->g_masterspnum) {
		HEART_PIU_K1PTR->h_clr_isr = HEART_ISR_IPI;
		flush_cache();
		init_tlb();

		/* enable bus error interrupt */
		set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR);

		slave_loop();
		/* NOTREACHED */
	}

	/*
	 * except sysinit(), all routines calling init_prom_soft() should
	 * set no_init_env to zero
	 */
	if (!no_init_env) {
		master_cpuid = cpuid();

		/*
	 	 * do this to get debug printf working before _init_saio()
		 * is called since the calling routine probably just clears
		 * the PROM bss
	 	 */
		_libsc_private = &ip30_tmp_libsc_private;
		initsplocks();
		initlock(&arcs_ui_lock);

		/*
		 * initialize cache related global variables and the tlb
		 * cache has already been flushed by the calling routine
		 */
		config_cache();
		init_tlb();

		/* set to use memory based exception handlers */
		VPRINTF(("_hook_exceptions()\r\n"));
		_hook_exceptions();
		set_SR(get_SR() & ~SR_BEV);

		VPRINTF(("Re-Initializing IP30 ASICs.\r\n"));
		init_ip30_chips();

		/* enable bus/widget error and softpower interrupt */
		set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR | SR_POWER);

		/* initialize environment variables */
		VPRINTF(("Re-initializing environment.\r\n"));
		init_env();

		if (!wakeup_slave_processors((0x1 << MAXCPU) - 1, 10)) {
			VPRINTF(("Slaves did not wakeup, resetting\r\n"));
			cpu_hardreset();
		}
	}

#if DEBUG
	atob(getenv("DEBUG"), &Debug);	/* initialize debugging flags */
	atob(getenv("VERBOSE"), &Verbose);
#endif

	VPRINTF(("_init_saio()\r\n"));
	_init_saio();		/*  initialize saio library */
	VPRINTF(("alloc_memdesc()\r\n"));
	alloc_memdesc();	/*  allocate memory descriptors for prom */
}

#ifndef IP30_RPROM
/*
 * halt - bring the system to a quiescent state
 */
void
halt(void)
{
	play_hello_tune(1);		/* shutdown tune */
	if (doGui())
		setGuiMode(1, GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
	    "Press RESET button to restart.",
	    0, DIALOGINFO, DIALOGCENTER | DIALOGBIGFONT);
	/* do not need to wait for power down tune since we spin here */
}

/*
 * restart - attempt to restart
 */
void
restart(void)
{
	static int restart_buttons[] = {
		DIALOGRESTART, -1	};

	play_hello_tune(1);		/* shutdown tune */
	if (doGui())
		setGuiMode(1, GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
	    "Press any key to restart.",
	    restart_buttons, DIALOGINFO,
	    DIALOGCENTER | DIALOGBIGFONT);
	wait_hello_tune();
	if (doGui())
		setGuiMode(0, GUI_RESET);
	/*  Set-up for AC fails -- need to wait until continue so user can
	 * power off system and keep setting of alarm for iauto power on.
	 */
	ip30_power_switch_on();		/* set-up for AC fails */
	startup();
}

/*
 * reboot - attempt a reboot
 */
void
reboot(void)
{
	if (!Verbose && doGui()) {
		setGuiMode(1, 0);
	}
	else {
		p_panelmode();
		p_cursoff();
	}
	if (ESUCCESS != autoboot(0, 0, 0)) {
		play_hello_tune(1);		/* shutdown tune */
		putchar('\n');
		setTpButtonAction(EnterInteractiveMode, TPBCONTINUE, WBNOW);
		p_curson();
		p_printf("Unable to boot; press any key to continue: ");
		getchar();
		putchar('\n');
		wait_hello_tune();
	}

	/*
	 * If reboot fails, reinitialize software in case a partially
	 * booted program has messed up the prom state.
	 */
	EnterInteractiveMode();		/* should not get here */
}

void
enter_imode(void)
{
	set_SR(SR_PROMBASE | ENET_SR_BEV | SR_IE);	/* for power */
	_hook_exceptions();
	_init_saio();		/*  initialize saio library */
	alloc_memdesc();	/*  allocate memory descriptors for prom */
	wbflush();
	set_SR(SR_PROMBASE | SR_IE | SR_IBIT_BERR | SR_POWER);
	main();
	EnterInteractiveMode();	/* shouldn't get here */
}
#endif	/*!IP30_RPROM */

static void
alloc_memdesc(void)
{
#define STKSZ	16384 		/* 16K stack (bigger than 32 bit prom) */

#ifdef	ENETBOOT
	/*
	 * Add what we know to be the dprom's text, data as FirmwarePermanent,
	 * and bss (rounded up from data page) + stack as Firmware as
	 * FirmwareTemporary.
	 */
	extern int start();

	md_alloc(KDM_TO_PHYS(start),
		arcs_btop(DBSSADDR - (__psunsigned_t)start),
		FirmwarePermanent);
	md_alloc(KDM_TO_PHYS(DBSSADDR),
		arcs_btop(0x100000), FirmwareTemporary);
	md_alloc(KDM_TO_PHYS(PROM_STACK - STKSZ), arcs_btop(STKSZ),
		FirmwareTemporary);
#else
	/*
	 * add what we know to be the prom's stack and bss.  Text and
	 * data are safe in prom in I/O space.
	 */
	md_alloc(KDM_TO_PHYS(PROM_BSS),
		arcs_btop(KDM_TO_PHYS(PROM_STACK) - KDM_TO_PHYS(PROM_BSS)),
		FirmwareTemporary);
#endif
}

#ifdef ENETBOOT
/* clear_memory - clear memory from PROM_STACK up */
static void
clear_memory(int uncached)
{
	int (*clrfunc)(__psunsigned_t, __psunsigned_t);

	if (uncached)
		clrfunc = (int (*)())hmclear;
	else
		clrfunc = (int (*)())K1_TO_K0(hmclear);

	/* clear from PROM_STACK to top of memory */
	(*clrfunc)(PHYS_TO_UNCACHED_ACC(KDM_TO_PHYS(PROM_STACK)),
		PHYS_TO_UNCACHED_ACC(PHYS_RAMBASE + memsize));
}
#endif /* ENETBOOT */


/*
 * Called when the caches are in the random power-up state.
 * Tags are random, as are the data arrays and check bits
 * associated with the cache. The data arrays have to be
 * initialized with the SR DE bit on, so not to cause ECC
 * or parity errors.
 *
 * Initialize all caches. All lines are initialized, setting the ECC or
 * parity, and each line is set to 'invalid'.
 */
void
init_caches(void)
{
	extern unsigned int _sidcache_size;

	/* set all tags of each cache to be invalid. */
	invalidate_caches();

	/*
	 * now go through each cache, and execute a load (or 'fill'
	 * for the icache), to cause the data for that cache line
	 * to be initialized with consistent ECC/parity.
	 */
	init_scache();
	init_dcache();
	init_icache();
}

static void
init_gda(void)
{
	/* Setup the GDA */
	GDA->g_promop = 0;
	GDA->g_nmivec = 0;
	GDA->g_masterspnum = master_cpuid;
	GDA->g_count = 0;
	GDA->g_magic = GDA_MAGIC;		/* needs to be last */
}

/*
 * slave processor spins until it receives an inter-processor interrupt
 * from the master processor, or until timeout is reached if it is set.
 * it returns a 0 if no interrupt from the master processor is received, a 1
 * otherwise.  timeout is in unit of millisecond
 */
static int
slave_processor_spin(int timeout)
{
	register heart_piu_t	*heart_piu = HEART_PIU_K1PTR;
	register heartreg_t	terminal_count;

	if (timeout)
		terminal_count = heart_piu->h_count +
			timeout * (1000000 / HEART_COUNT_NSECS);
	else
		terminal_count = 0xfffffffffffff;

	while (!(heart_piu->h_isr & HEART_ISR_IPI) &&
	    heart_piu->h_count < terminal_count)
		;

	if (heart_piu->h_isr & HEART_ISR_IPI) {
		heart_piu->h_clr_isr = HEART_ISR_IPI;
		return 1;
	}
	else
		return 0;
}

#define	IPI_SHIFT(x)	(HEART_INT_L2SHIFT + HEART_INT_IPISHFT + (x))
/*
 * master processor wakes up all specified slave processors by sending them
 * an inter-processor interrupt, then wait for response from them or until
 * timeout is reached if it is set.  it returns a 0 if it fails to wake up
 * one of the processors before timing out, a 1 otherwise.  timeout is in
 * unit of millisecond
 */
static int
wakeup_slave_processors(uint_t cpu_mask, int timeout)
{
	int			error = 0;
	register heart_piu_t	*heart_piu = HEART_PIU_K1PTR;
	int			i;
	register heartreg_t	terminal_count;

	/* make sure all specified slave processors get the interrupt */
	for (i = 0; i < MAXCPU; i++) {
		if (i == cpuid() || !(cpu_mask & (0x1 << i)) ||
		    !PROCESSOR_IS_ACTIVE(i))
			continue;

		if (timeout)
			terminal_count = heart_piu->h_count +
				timeout * (1000000 / HEART_COUNT_NSECS);
		else
			terminal_count = 0xfffffffffffff;

		heart_piu->h_set_isr = 0x1L << IPI_SHIFT(i);
		while (heart_piu->h_isr & (0x1L << IPI_SHIFT(i)) &&
		    heart_piu->h_count < terminal_count)
			;

		if (heart_piu->h_isr & (0x1L << IPI_SHIFT(i)))
			error = 1;
	}

	return error == 0;
}

#ifndef IP30_RPROM
static void
dump_nic_info(char *buf,int dumpflag)
{
	int n = 0;

	if (dumpflag) {
		printf("%s\n",buf);
		return;
	}

	while (*buf != '\0') {
		switch (*buf) {
		case ':':
			printf(": ");
			buf++;
			while (*buf != '\0' && *buf != ';') {
				putchar(*buf);
				buf++;
			}
			printf("  ");
			n++;
			break;
		default:
			if (strncmp(buf, "Part:", 5) == 0 ||
			    strncmp(buf, "NIC:", 4) == 0) {
				puts("\n      ");
				n = 0;
			}
			else if (n == 3) {
				puts("\n        ");
				n = 0;
			}
			putchar(*buf);
			break;
		}
		buf++;
	}

	putchar('\n');
}

#define MB	(1024 * 1024)

int
system_cmd(int argc, char **argv)
{
	volatile struct mpconf_blk *mpconf = MPCONF;
	unsigned int config = r4k_getconfig();
	static char *div[] = { "?", "1", "1.5", "2", "2.5", "3", "3.5", "4",
			       "4.5", "5", "5.5", "6", "6.5" };
	int dumpflag = 0;
	nic_data_t mcr;
	char *buf;
	int loads;
	int port;
	int xrev;

	if (argc == 2 && !strcmp(argv[1],"-d"))
		dumpflag = 1;

	printf("IP30 system:\n");
	printf("   CPU speed ~%sMhz\n",cpu_get_freq_str(0));
	printf("   Cache speed divisor %s\n",
		div[(config & CONFIG_SC) >> CONFIG_SC_SHFT]);
	printf("   SysAD speed divisor %s\n",
		div[(config & CONFIG_EC) >> CONFIG_EC_SHFT]);
	printf("   %d outstanding read(s)\n",
		((config & CONFIG_PM) >> CONFIG_PM_SHFT) + 1);
	/*
	 * This will have to be revisited when R14000 etc come out.
	 */
	if (!IS_R12000())
		printf("   R10K Revision: %d.%d",
			(mpconf[0].pr_id&C0_MAJREVMASK)>>C0_MAJREVSHIFT,
			mpconf[0].pr_id&C0_MINREVMASK);
	if (IS_R12000())
		printf("   R12K Revision: %d.%d",
			(mpconf[0].pr_id&C0_MAJREVMASK)>>C0_MAJREVSHIFT,
			mpconf[0].pr_id&C0_MINREVMASK);
	if (mpconf[1].mpconf_magic == MPCONF_MAGIC)
		printf(", CPU 1 %d.%d",
			(mpconf[1].pr_id&C0_MAJREVMASK)>>C0_MAJREVSHIFT,
			mpconf[1].pr_id&C0_MINREVMASK);
	printf("\n");
		
	printf("   Password jumper %s\n", jumper_off() ? "missing" : "on");

	buf = getenv("fastfan");
	loads = mpconf[0].fanloads;
	if (loads < 0)
		loads = -loads;
	else if (loads > 1000)
		loads -= 1000;
	printf("   Number of XIO fan loads %d (%d,env=%s)\n",
		loads, mpconf[0].fanloads, buf ? buf : "unset");

	printf("Chips/NICs:\n");
	buf = malloc(1024);
	if (buf != 0) {
		printf("   heart(rev %c): ", 'A' + heart_rev() - 1);
		mcr = (nic_data_t)&HEART_PIU_K1PTR->h_mlan_ctl + 0x4;
		cfg_get_nic_info(mcr, buf);
		dump_nic_info(buf,dumpflag);

		xrev = XWIDGET_REV_NUM(XBOW_K1PTR->xb_wid_id);

		if (xrev <= XBOW_REV_2_0)
			printf("   xbow(rev 1.%d):\n", xrev - 1);
		else
			printf("   xbow(rev 2.%d):\n", xrev - XBOW_REV_2_0);

		printf("   bridge(rev %c): ",
			'A' + XWIDGET_REV_NUM(BRIDGE_K1PTR->b_wid_id) - 1);
		mcr = (nic_data_t)&BRIDGE_K1PTR->b_nic;
		cfg_get_nic_info(mcr, buf);
		dump_nic_info(buf,dumpflag);

		printf("   ioc3(rev %d): eaddr %s ",
			*(ioc3reg_t *)(IOC3_PCI_DEVIO_K1PTR+IOC3_PCI_REV)&0xf,
			getenv("eaddr"));
		mcr = (nic_data_t)IOC3_PCI_DEVIO_K1PTR + IOC3_MCR;
		cfg_get_nic_info(mcr, buf);
		dump_nic_info(buf,dumpflag);

		/* look for other bridge/HQ4 NICs on xtalk */
		for (port = BRIDGE_ID-1; port > HEART_ID; port--) {
			widget_cfg_t *widget;
			int id;

			if (!xtalk_probe(XBOW_K1PTR, port))
				continue;

			widget = (widget_cfg_t *)K1_MAIN_WIDGET(port);
			id = ((widget->w_id) & WIDGET_PART_NUM) >>
				WIDGET_PART_NUM_SHFT;

			printf("   xtalk 0x%x ",port);
			if (id == BRIDGE_WIDGET_PART_NUM) {
				bridge_t *bridge = (bridge_t *)widget;
				printf("bridge(rev %c): ",
				   'A' + XWIDGET_REV_NUM(bridge->b_wid_id) - 1);
				mcr = (nic_data_t)&bridge->b_nic;
				cfg_get_nic_info(mcr, buf);
				dump_nic_info(buf,dumpflag);
			}
			else if (id == HQ4_WIDGET_PART_NUM) {
				mgras_hw *hq4 = (mgras_hw *)widget;
				printf("HQ4: ");
				mcr = (nic_data_t)&hq4->microlan_access;
				cfg_get_nic_info(mcr, buf);
				dump_nic_info(buf,dumpflag);
			}
			else
				printf("unknown widget partno=0x%x\n",id);
		}

		free(buf);
	}

	return(0);
}

static void
able_stat(void)
{
	heart_piu_t *heart_piu = HEART_PIU_K1PTR;
	extern int master_cpuid;
	unsigned short cpumask;
	int env_bootmaster;
	char *p;
	int i;

	p = cpu_get_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE);
	cpumask = (*p << 8) | *(p+1);

	printf("Processor status:\n");

	if (p = getenv("bootmaster")) {
		env_bootmaster = atoi(p);
		if (env_bootmaster < 0 || env_bootmaster >= MAXCPU)
			env_bootmaster = 0;
	}

	if (master_cpuid == env_bootmaster)
		env_bootmaster = MAXCPU;	/* same as current */

	for (i=0; i < MAXCPU; i++) {
		if (!PROCESSOR_IS_INSTALLED(i)) continue;

		printf("   CPU %d is marked %sabled, is currently %sabled. %s%s\n",
			i,
			cpumask & (1<<i) ? "dis" : "en",
			PROCESSOR_IS_DISABLED(i) ? "dis" : "en",
			i == master_cpuid ? "[current bootmaster]" : "",
			i == env_bootmaster ? "[next bootmaster]" : "");
	}
}

/*ARGSUSED*/
int
disable_cmd(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
	heart_piu_t *heart_piu = HEART_PIU_K1PTR;
	unsigned short cpumask;
	char *p;
	int cpu;
	int i;

	if (argc == 1) {
		able_stat();
		return 0;
	}

	if (argc != 2)
		return 1;

	cpu = atoi(argv[1]);

	if ((cpu >= MAXCPU) || !PROCESSOR_IS_INSTALLED(cpu)) {
		printf("CPU %d does not exist.\n",cpu);
		return 0;
	}

	if (cpu_probe_all() == 1) {
		printf("Only one CPU is present, please do not disable it.\n");
		return 0;
	}

	p = cpu_get_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE);
	cpumask = (*p << 8) | *(p+1);
	if ((cpumask & CPU_DISABLE_MAGIC_MASK) != CPU_DISABLE_MAGIC) {
		printf("warning: cpumask had invalid magic.\n");
		ip30_init_cpu_disable();
		cpumask = CPU_DISABLE_MAGIC;
	}

	/* If trying to disable the current CPU, we have to do some extra
	 * work with the bootmaster and require a reboot.
	 */
	if (cpu == cpuid()) {
		int env_bootmaster = 0;
		int new_bootmaster;
		char buf[2];
		char *ptr;

		if (ptr = getenv("bootmaster")) {
			env_bootmaster = atoi(ptr);
			if (env_bootmaster < 0 || env_bootmaster >= MAXCPU)
				env_bootmaster = 0;
		}
		if (env_bootmaster == cpu) {

			new_bootmaster = MAXCPU;

			for (i = 0 ; i < MAXCPU; i++) {
				if (i == env_bootmaster) continue;
				if (PROCESSOR_IS_INSTALLED(i) &&
				    !(cpumask & (1<<i)))
					new_bootmaster = i;
			}

			if (new_bootmaster < MAXCPU) {
				printf("Setting bootmaster to CPU %d to allow "
				       "CPU %d (current bootmaster) to be "
				       "disabled.\n",new_bootmaster,cpu);
				buf[0] = '0' + new_bootmaster;
				buf[1] = '\0';
				_setenv("bootmaster",buf,0);
			}
			else {
				printf("Cannot disable the bootmaster.  "
				       "There are no other enabled CPUs.\n");
				able_stat();
				return 0;
			}
		}
	}

	/* ensure at least one other processor is enabled in mask */
	for (i = 0; i < MAXCPU; i++) {
		if (i == cpu) continue;
		if (PROCESSOR_IS_INSTALLED(i) && !(cpumask & (1<<i)))
			break;
	}
	if (i == MAXCPU) {
		printf("Cannot disable CPU %d, "
		       "it is the only CPU marked active.\n",cpu);
		able_stat();
		return 0;
	}

	if (PROCESSOR_IS_DISABLED(cpu)) {
		if ((cpumask & (1<<cpu)) != 0) {
			printf("CPU %d is already disabled.\n",cpu);
			able_stat();
			return 0;
		}
		else
			printf("CPU %d is already disabled.  "
			       "Note that it may have failed diagnostics.\n",
			       cpu);
	}

	cpumask |= (1<<cpu);

	*(p+1) = cpumask & 0xff;

	cpu_set_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE,p);

	if (cpu != cpuid()) {
		printf("Disabling slave CPU %d now.\n",cpu);
		heart_piu->h_mode |= HM_PROC_DISABLE(cpu);
	}
	else {
		printf("Disabling CPU %d.  "
	               "The machine must be reset for this to take effect.\n",
		       cpu);
	}

	return 0;
}

/*ARGSUSED*/
int
enable_cmd(int argc, char **argv, char **argp, struct cmd_table *xxx)
{
	heart_piu_t *heart_piu = HEART_PIU_K1PTR;
	unsigned short cpumask;
	int cpu;
	char *p;

	if (argc == 1) {
		able_stat();
		return 0;
	}

	if (argc != 2)
		return 1;

	cpu = atoi(argv[1]);

	if ((cpu >= MAXCPU) || !PROCESSOR_IS_INSTALLED(cpu)) {
		printf("CPU %d does not exist.\n",cpu);
		return 0;
	}

	p = cpu_get_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE);
	cpumask = (*p << 8) | *(p+1);
	if ((cpumask & CPU_DISABLE_MAGIC_MASK) != CPU_DISABLE_MAGIC) {
		printf("warning: cpumask had invalid magic.\n");
		ip30_init_cpu_disable();
		cpumask = CPU_DISABLE_MAGIC;
	}

	/* if we are enabling the current CPU, perhaps reset bootmaster */
	if (cpu == cpuid()) {
		int env_bootmaster = 0;
		int new_bootmaster;
		char buf[2];
		char *ptr;

		if (ptr = getenv("bootmaster")) {
			env_bootmaster = atoi(ptr);
			if (env_bootmaster < 0 || env_bootmaster >= MAXCPU)
				env_bootmaster = 0;
		}
		if (env_bootmaster != cpu) {
			printf("Resetting bootmaster to CPU %d, the current "
			       "bootmaster.\n",cpu);
			buf[0] = '0' + cpu;
			buf[1] = '\0';
			_setenv("bootmaster",buf,0);
		}
	}

	if ((cpumask & (1<<cpu)) == 0) {
		if (PROCESSOR_IS_DISABLED(cpu))
			printf("CPU %d is marked enabled but is disabled.\n"
			       "Ensure that it has passed diagnostics.\n",cpu);
		else
			printf("CPU %d is already enabled.\n",cpu);
		able_stat();
		return 0;
	}

	cpumask &= cpumask & ~(1<<cpu);

	*(p+1) = cpumask & 0xff;

	cpu_set_nvram_offset(NVOFF_CPUDISABLE,NVLEN_CPUDISABLE,p);

	printf("Enabling CPU %d.",cpu);
	if (PROCESSOR_IS_DISABLED(cpu))
	       printf("  The machine must be reset for this to take effect.");
	printf("\n");

	return 0;
}
#endif	/* !IP30_RPROM */

/* Check nvram to see if prom text should be cached or not, This 
 * is mostly for LA debugging odd prom failures.
 */
static int
cachepromtext(void)
{
	char *p;

	p = cpu_get_nvram_offset(NVOFF_UNCACHEDPROM,NVLEN_UNCACHEDPROM);
	return (p && (*p == '1' || *p == 'y')) == 0;
}
