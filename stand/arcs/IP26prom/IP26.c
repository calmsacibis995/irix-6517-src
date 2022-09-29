/*
 * IP26.c -- machine dependent prom routines
 */

#ident	"$Revision: 1.33 $"

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

extern jmp_buf restart_buf;
extern int Verbose, Debug;

static void alloc_memdesc(void);
#ifdef ENETBOOT
static void clear_memory(int);
#endif
static void domfgstartup(char *);
extern void main(void);

void init_load_icache(void);
void init_load_dcache(void);
void init_load_scache(void);
void config_cache(void);

void ip22_setup_gio64_arb(void);

void play_hello_tune(int);
void wait_hello_tune(void);

void wait_clear_seg1(void);

void init_prom_soft(int);
int pon_morediags(void);
long get_gerror(void);

#ifdef VERBOSE
#define VPUTS(str)	puts(str)
#else
#define	VPUTS(str)
#endif

#define CPUBOARD "IP26"

/*
 * sysinit - finish system initialization and run diags
 */
void
sysinit(void)
{
	extern unsigned int _hpc3_write1;
	extern unsigned int _hpc3_write2;
	extern unsigned mc_rev;
	int diagsfailed=0;
	volatile long *p;
	long tmp;
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
	*(volatile __uint64_t *)PHYS_TO_K1(ECC_CTRL_REG) = ECC_CTRL_CLR_INT;
	wbflush();
	*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile unsigned int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	wbflush();
	p = (volatile long *)PHYS_TO_K1(TCC_INTR);
	*p = *p & (INTR_BUSERROR|INTR_MACH_CHECK|0xf800);
	*(volatile long *)PHYS_TO_K1(TCC_ERROR) =
		(ERROR_NESTED_MC|ERROR_NESTED_BE);
	wbflush();

	set_SR(SR_PROMBASE|SR_IBIT4);

	_hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
	_hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
		       ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
	flushbus();

	config_cache();

	VPUTS ("Initializing exception handlers.\r\n");
#ifdef	ENETBOOT
#ifdef GOOD_COMPILER
	bzero((void *)K1_RAMBASE, (int)(KDM_TO_PHYS(_ftext) - PHYS_RAMBASE));
#else
	bzero((void *)K1_RAMBASE, (int)(KDM_TO_PHYS(start) - PHYS_RAMBASE));
#endif
#endif
	_hook_exceptions();		/* setup exception vectors */
	play_hello_tune(0);		/* boot tune */
	ip22_setup_gio64_arb();		/* init gio64_arb variable */

#ifndef ENETBOOT
	wait_clear_seg1();		/* wait for seg1 memory to clear */
#endif

	if (tmp=get_gerror()) {
		long tage, datae;
		int i;

		if (tmp & (GERROR_TAG|GERROR_DATA))
			diagsfailed |= FAIL_CACHES;

		tage = (tmp & GERROR_TAG) >> GERROR_TAG_SHIFT;
		datae = (tmp & GERROR_DATA) >> GERROR_DATA_SHIFT;

		printf("\nGCache errors detected: parity=%d tag=%d%s data=%d%s.\n",
		       tmp & GERROR_PARITY,
		       tage, tage >= GERROR_MAXPONERR ? "[max]" : "",
		       datae, datae >= GERROR_MAXPONERR ? "[max]" : "");

		for (i=0;i < 4; i++) {
			if (tmp & (GERROR_SET0<<i))
				printf("\tSet %d disabled.\n",i);
		}

		puts("\n");
	}

	VPUTS ("Initializing environment.\r\n");
	init_env();		/* initialize environment for morediags */

	VPUTS ("Executing remainder of diagnostic.\r\n");
	diagsfailed |= pon_morediags();	/* run more diags  */

	/*  Make sure we have a rev B or greater MC chip for fullhouse since
	 * many of the workarounds have been taken out to streamline things
	 */
	if ((*(volatile uint *)PHYS_TO_K1(SYSID) & SYSID_CHIP_REV_MASK) <= 1) {
		printf( "FATAL ERROR: Rev A/B MC detected--"
			"Need rev C or greater.\n" );
		while (*(volatile uint *)PHYS_TO_K1(SYSID)) ;/* sit and spin */
	}

	/* clear memory cached unless diag fails 
	 */
	wait_hello_tune();

	mc_rev = *(volatile uint *)PHYS_TO_K1(SYSID) & SYSID_CHIP_REV_MASK;

#ifdef ENETBOOT
	VPUTS ("Clearing memory.\r\n");
	clear_memory (diagsfailed & FAIL_CACHES);
#endif

	VPUTS ("Initializing software and devices.\r\n");
	init_prom_soft(1);

	VPUTS ("All initialization and diagnostic completed.\r\n");
	if (diagsfailed) {
		setTpButtonAction(main,TPBCONTINUE,WBNOW);
		printf("\nDiagnostics failed.\n[Press any key to continue.]");
		getchar();
		main();
	}
	else {
		char *dm = getenv("diagmode");
		if ( dm && *dm == 'M' )
			domfgstartup(dm);
		startup ();
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
	set_SR(SR_PROMBASE);

	config_cache();
	flush_cache();

	_hook_exceptions();
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	*K1_LIO_2_MASK_ADDR = 0;
	*K1_LIO_3_MASK_ADDR = 0;
	set_SR(SR_PROMBASE|SR_IBIT4);

	/* Make sure all TCC interrupts are disabled.
	 */
	*(volatile long *)PHYS_TO_K1(TCC_INTR) =
		INTR_BUSERROR_EN|INTR_MACH_CHECK_EN;

	if (!no_init_env)
		init_env();		/* initialize environment */

	/*  Invalidate prefetch buffers and make sure prefetch is
	 * disabled (kernel turns it on, but we want it off in
	 * standalone).
	 */
	*(volatile long *)PHYS_TO_K1(TCC_PREFETCH) = PRE_INV;

	/*  Enable TCC fifo with 0 hw/lw levels.  We need to do this
	 * here so we can load programs with text in chandra space,
	 * which is the same as processor ordered stores go to, since
	 * when the fifo is not enabled writes are silently dropped.
	 */
	*(volatile long *)PHYS_TO_K1(TCC_FIFO) = FIFO_INPUT_EN|FIFO_OUTPUT_EN;

#if DEBUG
	atob(getenv("DEBUG"), &Debug);	/* initialize debugging flags */
	atob(getenv("VERBOSE"), &Verbose);
#endif

	_init_saio();		/* initialize saio library */
	alloc_memdesc();	/* allocate memory descriptors for prom */

	*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile unsigned int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
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
 *
 * XXX This is supposed to try to reproduce the last system boot
 * command, but it doesn't.
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
/* clear_memory - clear low memory from PROM_STACK up */
static void
clear_memory(int uncached)
{
	extern int cpu_get_low_memsize(void), hmclear(void);
	int (*clrfunc)(__psunsigned_t, __psunsigned_t);
	unsigned int memsize = cpu_get_low_memsize();

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
gioinfo(int argc, char **argv)
{
	printf("gio speed %dMhz\n", ip22_gio_is_33MHZ()? 33 : 25);
	return(0);
}
