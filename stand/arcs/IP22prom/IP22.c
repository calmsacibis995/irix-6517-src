#ident	"IP22prom/IP22.c:  $Revision: 1.76 $"

/*
 * IP22.c -- machine dependent prom routines
 */

#include <sys/types.h>
#include <setjmp.h>
#include <libsc.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <gfxgui.h>
#include <style.h>
#include <libsk.h>

extern jmp_buf restart_buf;
extern int Verbose, Debug;
extern void config_cache();

static void alloc_memdesc(void);
#ifdef ENETBOOT
static void clear_memory(int);
#endif
static void init_caches(void);
static void domfgstartup(char *);
extern void main(void);
extern int pon_caches(void);

void init_load_icache(void);
void init_load_dcache(void);
void init_load_scache(void);
void config_cache(void);
void init_tlb(void);
void setstackseg(__psunsigned_t);

void ip22_setup_gio64_arb(void);
void r4400_hack_clear_DE(void);

void play_hello_tune(int);
void wait_hello_tune(void);

void init_prom_soft(int);
int pon_morediags(int);

#ifdef VERBOSE
#define VPRINTF(arg) printf arg
unsigned char *iocmsg[] = {"", "IOC1 ", "IOC1.2 " };
#else
#define	VPRINTF(arg)
#endif

#ifdef IP24
#define CPUBOARD "IP24"
#else
#define CPUBOARD "IP22"
#endif

#ifdef ENETBOOT
#define ENET_SR_BEV	0
#else
#define ENET_SR_BEV	SR_BEV
#endif

/*
 * sysinit - finish system initialization and run diags
 */
void
sysinit(void)
{
    int diagsfailed;
#ifdef ENETBOOT
    extern _ftext[];
#endif
    extern unsigned int _hpc3_write1;		/* reg 'RESET' on guinness */
    extern unsigned int _hpc3_write2;		/* reg 'WRITE' on guinness */
    extern unsigned int _sidcache_size;
    void (*fptr0)(void);
    void (*fptr1)(void);

    /* wait for power switch to be released */
    do {
	*(volatile int *)PHYS_TO_K1(HPC3_PANEL) = POWER_ON;
	flushbus();
	us_delay(5000);
    } while (*K1_LIO_1_ISR_ADDR & LIO_POWER);
    VPRINTF(("Power switch released.\r\n"));
    ip22_power_switch_on();			/* set-up for AC fails */
    *K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
    set_SR(SR_PROMBASE|ENET_SR_BEV|SR_DE|SR_IE|SR_IBIT4);

#if IP24		/* Leave VIDEO and ISDN Reset on */
    _hpc3_write1 = PAR_RESET | KBD_MS_RESET | LED_RED_OFF;
#else
    _hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
#endif
    _hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
	ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;
    *(unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
    *(unsigned int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;

#if VERBOSE
    VPRINTF(( "============ IP%d P%d %d-bit %s============\r\n",
#if IP24
	    24, ip22_board_rev() - 1,
#else
	    22, ip22_board_rev(),
#endif
	    get_SR()&SR_KX ? 64 : 32,
	    iocmsg[is_ioc1()]));
#endif

    /* SR_DE should be set at this point.  set in csu.s */
    VPRINTF(("Initializing primary/secondary caches.\r\n"));
    fptr0 = (void (*)())K0_TO_K1(config_cache);
    fptr1 = (void (*)())K0_TO_K1(init_caches);
    cache_K0();
    (*fptr0)();
    (*fptr1)();
    uncache_K0();

#if IP24			/* I2 always has 1MB */
    VPRINTF(("Secondary cache size is 0x%x (%d Kbytes)\r\n",
	_sidcache_size, _sidcache_size/1024));
#endif

    VPRINTF(("Initializing exception handlers.\r\n"));
#ifdef	ENETBOOT
    bzero((void *)K1_RAMBASE, KDM_TO_PHYS(_ftext) - PHYS_RAMBASE);
#endif
    _hook_exceptions();		/* setup exception vectors */
    ip22_setup_gio64_arb();	/* init gio64_arb variable */

    VPRINTF(("Initializing Status Register.\r\n"));
    r4400_hack_clear_DE();
    set_SR(SR_PROMBASE|SR_IE|SR_IBIT4|SR_BERRBIT);

#ifdef IP24
    /* enable dma channels for guinness P1 and later */

    if (ip22_board_rev() >= 2) {

	VPRINTF(("Enabling PLP, ISDN B/A DMA channels.\r\n"));
	*(volatile int *)PHYS_TO_K1(HPC3_DMA_SELECT) =
	    DMA_SELECT_PARALLEL | DMA_SELECT_ISDN_B | DMA_SELECT_ISDN_A;

	VPRINTF(("Selecting Gen Control register bits.\r\n"));
	*(volatile int *)PHYS_TO_K1(HPC3_GC_SELECT) = GC_SELECT_INIT;

	VPRINTF(("Initialize Map Polarity.\r\n"));
	*(volatile int *)PHYS_TO_K1(HPC3_INT3_MAP_POLARITY) = HPC3_MP_INIT;

	VPRINTF(("Deasserting VIDEO Reset.\r\n"));
	/*
	 * (Note that the ISDN reset remains active until the kernel driver 
	 * or the IDE deasserts it.)
	 */
	_hpc3_write1 |= VIDEO_RESET;
	*(unsigned int *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
    }
#endif

#ifdef IP24
    VPRINTF ("Playing startup tune.\r\n");
    play_hello_tune(0);		/* boot tune */
#else
    /*  Run cache diag early so we can use the cache during the hello tune.
     * Skip tune if cache diags fail to keep things moving along.
     */
    diagsfailed = ((int (*)(void))K0_TO_K1(pon_caches))();
    if (diagsfailed == 0)  {
	extern int decinsperloop, uc_decinsperloop;

	cache_K0();
	IS_KSEG0(sysinit) ? run_cached() : run_uncached();

	decinsperloop = 0;		/* not needed for synth */
	uc_decinsperloop = 0;

	VPRINTF (("Playing startup tune.\r\n"));
	play_hello_tune(0);		/* boot tune */
    }
#endif

    VPRINTF (("Initializing environment.\r\n"));
    init_env();			/* initialize environment for morediags*/

    VPRINTF (("Executing remainder of diagnostic.\r\n"));
    diagsfailed = pon_morediags(diagsfailed);	/* run more diags  */

#ifndef IP24
    /*  Make sure we have a rev B or greater MC chip for fullhouse since
     * many of the workarounds have been taken out to streamline things
     */
    if ((*(volatile uint *)PHYS_TO_K1(SYSID) & SYSID_CHIP_REV_MASK) == 0) {
	printf("Fatal Error:  Rev A MC detected.  Need rev B or greater.\n");
	while (1) ;		/* sit and spin */
    }
#endif

    /* clear memory cached unless diag fails 
     */
    wait_hello_tune();
#ifdef ENETBOOT
    VPRINTF(("Clearing memory.\r\n"));
    clear_memory (diagsfailed & FAIL_CACHES);
#endif

    VPRINTF(("Initializing software and devices.\r\n"));
    init_prom_soft (1);

    VPRINTF(("All initialization and diagnostic completed.\r\n"));
    if (diagsfailed) {
#ifndef IP24
	/* Hook for GE Medical on Indigo2 -- This lets the system
	 * not stop for user intervention when power-on diags fail.
	 *
	 * diagmode=dc (diags continue) triggers this
	 */
	char *dm = getenv("diagmode");

	if (dm && *dm == 'd' && *(dm+1) == 'c') {
		char buf[16];

		sprintf(buf,"%x",diagsfailed);
		printf("\nDiagnostics failed.\nContinuing with diagmode=dc.");
		setenv("ponfailed",buf);
		startup();	/* no need to check diagmode=Mx */
	}
	else
#endif
	{
		setTpButtonAction(main,TPBCONTINUE,WBNOW);
		printf("\nDiagnostics failed.\n[Press any key to continue.]");
		getchar();
		main();
	}
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
	return;
}

/* 
 * init_prom_soft - configure prom software 
 */
void
init_prom_soft(int no_init_env)
{
#ifdef	ENETBOOT
    static int in_data_section = 1;
#endif

    set_SR(SR_PROMBASE|SR_DE|(get_SR()&SR_BEV));

    VPRINTF(("config_cache()\r\n"));
    config_cache();
    VPRINTF(("flush_cache()\r\n"));
    flush_cache();

    VPRINTF(("_hook_exceptions()\r\n"));
    _hook_exceptions();
    *K1_LIO_0_MASK_ADDR = 0;
    *K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
    *K1_LIO_2_MASK_ADDR = 0;
    *K1_LIO_3_MASK_ADDR = 0;
    set_SR(SR_PROMBASE|SR_IE|SR_IBIT4|SR_BERRBIT);

    if (!no_init_env) {
	VPRINTF(("init_env()\r\n"));
    	init_env();			/*  initialize environment */
    }
#if DEBUG
    atob(getenv("DEBUG"), &Debug);	/* initialize debugging flags */
    atob(getenv("VERBOSE"), &Verbose);
#endif

    VPRINTF(("_init_saio()\r\n"));
    _init_saio();		/*  initialize saio library */
    VPRINTF(("alloc_memdesc()\r\n"));
    alloc_memdesc();		/*  allocate memory descriptors for prom */

#ifdef	ENETBOOT
    /* clear the tlb if we're not running mapped
     */
    if (IS_KSEG0(&in_data_section) || IS_KSEG1(&in_data_section))
#endif
	init_tlb(); 		/*  clear tlb */

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
    if (ESUCCESS != autoboot(0, 0, 0)) {
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
    set_SR(SR_PROMBASE|ENET_SR_BEV);	
    _hook_exceptions();
    _init_saio();		/*  initialize saio library */
    alloc_memdesc();		/*  allocate memory descriptors for prom */
    *(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
    *(volatile unsigned int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
    wbflush();
    *K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
    set_SR(SR_PROMBASE|SR_IE|SR_IBIT4|SR_BERRBIT);
    main();
    EnterInteractiveMode();	/* shouldn't get here */
}

static void
alloc_memdesc(void)
{
#define STKSZ	8192 		/* 8K stack */

#ifdef	ENETBOOT
    /*  Add what we know to be the dprom's text, data as FirmwarePermanent,
     * and bss (rounded up from data page) + stack as Firmware as
     * FirmwareTemporary.
     */
    extern _ftext[], _edata[], _end[];
    unsigned bsspage1 = ((__psunsigned_t)_edata & ~(ARCS_NBPP-1)) + ARCS_NBPP;

    md_alloc(KDM_TO_PHYS(_ftext),
	     arcs_btop(KDM_TO_PHYS((__psunsigned_t)_edata -
				   (__psunsigned_t)_ftext)),
	      FirmwarePermanent);
    md_alloc(KDM_TO_PHYS(bsspage1),
	     arcs_btop(KDM_TO_PHYS((__psunsigned_t)_end -
				   (__psunsigned_t)bsspage1)),
	      FirmwareTemporary);
    md_alloc(KDM_TO_PHYS(PROM_STACK-STKSZ),arcs_btop(STKSZ),FirmwareTemporary);
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
#define CLRSIZE 0x40		/* matches size in lmem_conf.s */
static void
clear_memory(int uncached)
{
    extern unsigned int memsize;	/* from ml/IP22.c */
    extern int hmclear();
    int (*clrfunc)(__psunsigned_t, __psunsigned_t);

    if (uncached)
	clrfunc = (int (*)())hmclear;
    else
	clrfunc = (int (*)())K1_TO_K0(hmclear);

    /* clear from PROM_STACK to top of memory */
    (*clrfunc)(PROM_STACK, K1_RAMBASE+_min(memsize,0x10000000));
}
#endif


/*
 * Called when the caches are in the random power-up state.
 * Tags are random, as are the data arrays and check bits
 * associated with the cache. The data arrays have to be
 * initialized with the SR DE bit on, so not to cause ECC
 * or parity errors.
 *
 * Called *after* memory is known to be working.
 * This is particularly important for the icache, since there
 * is no way to initialize the icache except for fills from scache
 * or mem.
 *
 * Initialize all caches. All lines are initialized, setting the ECC or
 * parity, and each line is set to 'invalid'.
 */
static void
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
	if (_sidcache_size)
		init_load_scache();
	init_load_dcache();
	init_load_icache();

	/* Now go back through and invalidate all of the caches. */
	invalidate_caches();
}

/*ARGSUSED*/
int
gioinfo(int argc, char **argv)
{

	printf("gio speed %dMhz\n", ip22_gio_is_33MHZ()? 33 : 25);
	return(0);
}

#if _K64PROM32
int _isK64PROM32(void) { return 0; }
#endif

#if IP24
/* call dallas routines */
#define	NVRAM_FUNC(type,name,args,pargs,cargs,return)	\
extern type dallas_ ## name pargs;			\
type name args { return dallas_ ## name cargs;	}

#else
/* call eeprom routines */
#define	NVRAM_FUNC(type,name,args,pargs,cargs,return)	\
extern type eeprom_ ## name pargs;			\
type name args { return eeprom_ ## name cargs;	}

#endif

#include <PROTOnvram.h>
/* === */
