/*
 * IP28.c -- machine dependent prom routines
 */

#ident	"$Revision: 1.30 $"

#include <sys/types.h>
#include <setjmp.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsk.h>

#include <sys/RACER/gda.h>		/* shared with speed racer */

extern jmp_buf restart_buf;
extern int Verbose, Debug;

static void alloc_memdesc(void);
#ifdef ENETBOOT
static void clear_memory(int);
#endif
static void domfgstartup(char *);
extern void main(void);

void init_tlb(void);
void config_cache(void);

void ip22_setup_gio64_arb(void);

void play_hello_tune(int);
void wait_hello_tune(void);
void wait_clear_mem(void);
void init_prom_soft(int);
int pon_morediags(int);
int pon_caches(void), pon_scache(void);

#ifdef VERBOSE
#define VPUTS(str)	puts(str)
#else
#define	VPUTS(str)
#endif

#define CPUBOARD "IP28"

#ifdef ENETBOOT
#define ENET_SR_BEV	0
#else
#define ENET_SR_BEV	SR_BEV
#endif

void ip26_clear_ecc(void);

/*
 * sysinit - finish system initialization and run diags
 */
void
sysinit(void)
{
	extern unsigned int _hpc3_write1;
	extern unsigned int _hpc3_write2;
	extern void ip28_nmi(void);
	extern unsigned mc_rev;
	int diagsfailed=0;
	extern unsigned int _sidcache_size;
#ifdef ENETBOOT
#ifdef GOOD_COMPILER
	extern _ftext[];
#else
	extern int start();
#endif
#endif

	/* wait for power switch to be released */
	do {
		*(volatile int *)PHYS_TO_K1(HPC3_PANEL) = POWER_ON;
		flushbus();
		us_delay(5000);
	} while (*K1_LIO_1_ISR_ADDR & LIO_POWER);
	VPUTS("Power switch released.\r\n");
	ip22_power_switch_on();			/* set-up for AC fails */
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;

	/* Just make sure our ECC errors are cleared before enabling, we
	 * sometimes still have errors when we get here.
	 */
	ip26_clear_ecc();
	*(volatile unsigned int *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	*(volatile unsigned int *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	wbflush();

	/*  Initialize GDA -- only hook on IP28 baseboard since we must
	 * save context via sd instructions uncached, potentially in
	 * fast mode.
	 */
	if (is_ip28bb()) {
		GDA->g_magic = GDA_MAGIC;
		GDA->g_promop = 0;
		GDA->g_nmivec = 0;
		GDA->g_masterspnum = 0;
		GDA->g_magic = GDA_MAGIC;
	}

	set_SR(SR_PROMBASE|ENET_SR_BEV|SR_DE|SR_IE|SR_IBIT4);

	_hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
	_hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
		       ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;

	/* ECC_STATUS is 16 bits wide */
	*(volatile int *)PHYS_TO_COMPATK1(HPC3_PBUS_CFG_ADDR(6)) |=
		HPC3_CFGPIO_DS_16;

	flushbus();

	/* SR_DE must be set at this point */
	config_cache();		/* init_cache done from csu.s */

#if ENETBOOT && GOOD_COMPILER
	bzero((void *)K1_RAMBASE, (int)(KDM_TO_PHYS(_ftext) - PHYS_RAMBASE));
#elif ENETBOOT
	bzero((void *)K1_RAMBASE, (int)(KDM_TO_PHYS(start) - PHYS_RAMBASE));
#endif

	VPUTS ("Initializing exception handlers.\r\n");
	_hook_exceptions();		/* setup exception vectors */

	/* Hook to higher level handler early just in case */
	GDA->g_nmivec = (__psunsigned_t)ip28_nmi;

	ip22_setup_gio64_arb();		/* init gio64_arb variable */

	/* run cache tests early with SR_DE set */
	VPUTS("Run pon_scache...");
	diagsfailed = pon_scache();
	VPUTS("pon_caches.\r\n");
	diagsfailed |= pon_caches();

	VPUTS("Clear SR_DE & SR_BEV.\r\n");
	set_SR(SR_PROMBASE|SR_IBIT4);

	if (diagsfailed == 0)  {
		extern int decinsperloop, uc_decinsperloop;

		VPUTS ("Enable cached K0.\r\n");
		cache_K0();
		decinsperloop = 0;		/* not needed for synth */
		uc_decinsperloop = 0;

		/* Touch EISA for early EISA hang hack.  Flick amber led for
		 * some feedback.  Does not seem to hang when run uncached,
		 * and we want to be before the tune.
		 */
		VPUTS("Touching EISA.\r\n");
		ip22_set_cpuled(2);
		kbdbell_init();
		ip22_set_cpuled(1);

		VPUTS ("Playing startup tune.\r\n");
		play_hello_tune(0);		/* boot tune */
	}
	else
		diagsfailed = FAIL_CACHES;	/* pon_* returns count */

	VPUTS ("Initialize tlb.\r\n");
	init_tlb();			/* clean-out tlb */

#ifndef ENETBOOT
	wait_clear_mem();		/* wait for upper memory to clear */
#endif

	VPUTS ("Initializing environment.\r\n");
	init_env();		/* initialize environment for morediags */

	VPUTS ("Executing remainder of diagnostic.\r\n");
	diagsfailed |= pon_morediags(diagsfailed);	/* run more diags  */

	/*  Make sure we have a rev D MC to support high mapping.
	 */
	mc_rev = *(volatile uint *)PHYS_TO_COMPATK1(SYSID)&SYSID_CHIP_REV_MASK;
	if (mc_rev < 5) {
		printf( "FATAL ERROR: Rev A/BC MC detected--"
			"Need rev D or greater.\n" );
		while (1) ;				/* sit and spin */
	}

	VPUTS ("Wait for tune.\r\n");
	wait_hello_tune();

	/* clear memory cached unless diag fails 
	 */
#ifdef ENETBOOT
	VPUTS ("Clearing memory.\r\n");
	clear_memory (diagsfailed & FAIL_CACHES);
#endif

	VPUTS ("Initializing software and devices.\r\n");
	init_prom_soft(1);

	VPUTS ("All initialization and diagnostic completed.\r\n");
	if (diagsfailed) {
		/* Hook for GE Medical on Indigo2 -- This lets the system
		 * not stop for user intervention when power-on diags fail.
		 *
		 * diagmode=dc (diags continue) triggers this
		 */
		char *dm = getenv("diagmode");

		if (dm && *dm == 'd' && *(dm+1) == 'c') {
			char buf[16];

			sprintf(buf,"%x",diagsfailed);
			printf( "\nDiagnostics failed.\n"
				"Continuing with diagmode=dc.");
			setenv("ponfailed",buf);
			startup();	/* no need to check diagmode=Mx */
		}
		else {
			setTpButtonAction(main,TPBCONTINUE,WBNOW);
			printf( "\nDiagnostics failed.\n"
				"[Press any key to continue.]");
			getchar();
			main();
		}
	}
	else {
		char *dm = getenv("diagmode");
		if ( dm && *dm == 'M' )
			domfgstartup(dm);
		startup();
	}
}

static void
domfgstartup(char *mode)
{
	char *cmdv[8];
	int cmdc;
	int i;

	cmdc = 0;
	cmdv[cmdc++] = "boot";

	switch (*(mode+1)) {
		case '1':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "ide." CPUBOARD;
			break;
		case '2':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()ide." CPUBOARD;
			break;
		case '3':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "sash" CPUBOARD;
			break;
		case '4':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()sash" CPUBOARD;
			break;
		case '5':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "sash" CPUBOARD;
			cmdv[cmdc++] = "unix." CPUBOARD;
			break;
		case '6':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()unix." CPUBOARD;
			break;
		case '7':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()mgras.ide." CPUBOARD;
			break;
		default:
			return;
	}
	cmdv[cmdc] = 0;

	for (i = 0; i < cmdc; i++) {
		puts(cmdv[i]);
		putchar(' ');
	}
	putchar('\n');
	p_curson();

	for ( i = 0; i < 500; i++ )
		if ( GetReadStatus(0) == ESUCCESS )
			return;
		else
			us_delay(1000);

	Verbose = 1;
	boot(cmdc,cmdv,0,0);
	
	setTpButtonAction(main,TPBCONTINUE,WBNOW);
	printf("\nMfg auto boot failed.\n[Press any key to continue.]");
	getchar();
	main();

	return;
}

/* 
 * init_prom_soft - configure prom software 
 */
void
init_prom_soft(int no_init_env)
{
	set_SR(SR_PROMBASE|SR_DE|ENET_SR_BEV);

	VPUTS ("config_cache()\r\n");
	config_cache();
	VPUTS ("flush_cache()\r\n");
	flush_cache();

	_hook_exceptions();
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	*K1_LIO_2_MASK_ADDR = 0;
	*K1_LIO_3_MASK_ADDR = 0;
	flushbus();
	set_SR(SR_PROMBASE|SR_IBIT4);

	if (!no_init_env)
		init_env();		/* initialize environment */

#if DEBUG
	atob(getenv("DEBUG"), &Debug);	/* initialize debugging flags */
	atob(getenv("VERBOSE"), &Verbose);
#endif

	_init_saio();		/* initialize saio library */
	alloc_memdesc();	/* allocate memory descriptors for prom */

	init_tlb();		/* clear tlb */

	*(volatile unsigned int *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	*(volatile unsigned int *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	wbflush();
}

/*
 * halt - bring the system to a quiescent state
 */
void
halt(void)
{
	play_hello_tune(1);
	if (doGui()) setGuiMode(1,GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
		    "Press RESET button to restart.",
		    0,DIALOGINFO,DIALOGCENTER|DIALOGBIGFONT);
	/* do not need to wait for power down tune since we spin here */
}

/*
 * restart - attempt to restart
 */
void
restart(void)
{
	static int restart_buttons[] = {DIALOGRESTART,-1};

	play_hello_tune(1);
	if (doGui()) setGuiMode(1,GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
		    "Press any key to restart.",
		    restart_buttons,DIALOGINFO,
		    DIALOGCENTER|DIALOGBIGFONT);
	while (GetReadStatus(0) == ESUCCESS) getchar();	/* eat function keys */
	wait_hello_tune();
	if (doGui()) setGuiMode(0,GUI_RESET);
	/*  Set-up for AC fails -- need to wait until continue so user can
	 * power off system and keep setting of alarm for iauto power on.
	 */
	ip22_power_switch_on();			/* set-up for AC fails */
	startup();
}

/*
 * reboot - attempt a reboot
 */
void
reboot(void)
{
	if (!Verbose && doGui()) {
		setGuiMode(1,0);
	}
	else {
		p_panelmode();
		p_cursoff();
	}

	if (autoboot(0, 0, 0) != ESUCCESS) {
		play_hello_tune(1);
		putchar('\n');
		setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
		p_curson();
		p_printf("Unable to boot; press any key to continue: ");
		getchar();
		putchar('\n');
		wait_hello_tune();
	}

	/* If reboot fails, reinitialize software in case a partially
	 * booted program has messed up the prom state.
	 */
	EnterInteractiveMode();		/* should not get here */
}

void
enter_imode(void)
{
	set_SR(SR_PROMBASE);
	_hook_exceptions();
	_init_saio();		/* initialize saio library */
	alloc_memdesc();	/* allocate memory descriptors for prom */
	*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile unsigned int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	wbflush();
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	set_SR(SR_PROMBASE|SR_IBIT4);
	main();
	EnterInteractiveMode();	/* shouldn't get here */
}

static void
alloc_memdesc(void)
{
#define STKSZ	16384 		/* 16K stack (bigger than 32 bit prom) */

#ifdef	ENETBOOT
#ifdef GOOD_COMPILER
	/*  Add what we know to be the dprom's text, data as FirmwarePermanent,
	 * and bss (rounded up from data page) + stack as Firmware as
	 * FirmwareTemporary.
	 */
	extern _ftext[], _edata[], _end[];
	__psunsigned_t bsspage1 = ((__psunsigned_t)_edata&~(ARCS_NBPP-1)) + ARCS_NBPP;

	md_alloc(KDM_TO_PHYS(_ftext), 
		 arcs_btop(KDM_TO_PHYS((__psunsigned_t)_edata -
				  (__psunsigned_t)_ftext)),
		 FirmwarePermanent);
	md_alloc(KDM_TO_PHYS(bsspage1),
		 arcs_btop(KDM_TO_PHYS((__psunsigned_t)_end -
				  (__psunsigned_t)bsspage1)),
		 FirmwareTemporary);
	md_alloc(KDM_TO_PHYS(PROM_STACK - STKSZ), arcs_btop(STKSZ),
		 FirmwareTemporary);
#else
	/*  Add what we know to be the dprom's text, data as FirmwarePermanent,
	 * and bss (rounded up from data page) + stack as Firmware as
	 * FirmwareTemporary.
	 */
	extern int start();
	extern _ftext[], _edata[], _end[];
#define TEXTDATA_SIZE	0x80000

	md_alloc(KDM_TO_PHYS(start), arcs_btop(DBSSADDR-(__psunsigned_t)start),
		 FirmwarePermanent);
	md_alloc(KDM_TO_PHYS(DBSSADDR), arcs_btop(0x100000), FirmwareTemporary);
#endif
	md_alloc(KDM_TO_PHYS(PROM_STACK - STKSZ), arcs_btop(STKSZ),
		 FirmwareTemporary);
#else
	/* add what we know to be the prom's stack and bss.  Text and
	 * data are safe in prom in I/O space.
	 */
	md_alloc(KDM_TO_PHYS(PROM_BSS),
		 arcs_btop(KDM_TO_PHYS(PROM_STACK)-KDM_TO_PHYS(PROM_BSS)),
		 FirmwareTemporary);
#endif
}

#ifdef ENETBOOT
/* clear_memory - clear memory from PROM_STACK up */
static void
clear_memory(int uncached)
{
	extern int hmclear(void);
	int (*clrfunc)(__psunsigned_t, __psunsigned_t);
	unsigned int memsize = cpu_get_memsize();

	if (uncached)
		clrfunc = (int (*)())hmclear;
	else
		clrfunc = (int (*)())K1_TO_K0(hmclear);

	/* clear from PROM_STACK to top of memory */
	(*clrfunc)(PROM_STACK, K1_RAMBASE+_min(memsize,0x10000000));
}
#endif

/*ARGSUSED*/
int
system_cmd(int argc, char **argv)
{
	unsigned int config = r4k_getconfig();
	extern int is_ip26bb(void);
	static char *div[] = { "?", "1", "1.5", "2", "2.5", "3", "3.5", "4",
			       "4.5", "5", "5.5", "6", "6.5" };
	char *bb;

	if (is_ip26pbb())
		bb = "IP28A";
	else
		bb = is_ip26bb() ? "IP26" : "IP28B";
	

	printf("IP28 system:\n");
	printf("   CPU speed ~%sMhz\n",cpu_get_freq_str(0));
	printf("   Cache speed divisor %s\n",
		div[(config & CONFIG_SC) >> CONFIG_SC_SHFT]);
	printf("   SysAD speed divisor %s\n",
		div[(config & CONFIG_EC) >> CONFIG_EC_SHFT]);
	/* Assume IP26 bb have x18 as ECC_STATUS is not implemented on IP26 */
	printf("   Module has %s SSRAMs\n",
		((*K1_ECC_STATUS & ECC_STATUS_SSRAMx36) && is_ip28bb()) ?
		"x36" : "x18");
	printf("   GIO speed %dMhz [speculative read %sprotected]\n",
		ip22_gio_is_33MHZ()? 33 : 25,
		badaddr((void *)PHYS_TO_K0(CPUCTRL0),4) ? "" : "un");
	printf("   %d outstanding read(s)\n",
		((config & CONFIG_PM) >> CONFIG_PM_SHFT) + 1);
		
	printf("   %s baseboard\n",bb);

	return(0);
}

/*ARGSUSED*/
int
config_cmd(int argc, char **argv)
{
	extern struct reg_desc config_desc[];

	printf("C0_CONFIG %R\n",r4k_getconfig(),config_desc);
	return 0;
}
