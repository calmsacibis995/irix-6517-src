#ident	"IP20prom/IP20.c:  $Revision: 1.43 $"

/*
 * IP20.c -- machine dependent prom routines
 */

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

extern jmp_buf restart_buf;

#ifdef VERBOSE
#define VPRINTF printf
#else
#define	VPRINTF
#endif

static void alloc_memdesc(void);
#ifdef ENETBOOT
static void clear_memory(int);
#endif
static void init_caches(void);
static void domfgstartup(char *);
extern void main();

void init_prom_soft(int);

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

    /* SR_DE should be set at this point.  set in csu.s */
    VPRINTF ("Initializing primary/secondary caches.\r\n");
    config_cache();
    init_caches();

    VPRINTF ("Initializing exception handlers.\r\n");
#ifdef	ENETBOOT
    bzero((void *)K1_RAMBASE, (unsigned int)_ftext - K1_RAMBASE);
#endif
    _hook_exceptions();
    set_SR (0);			/* turn off BEV and enable handlers  */

    /* Set up the endianness of the system.  Do it here before diags
     * are run because they may need to know if they have to swap
     * bytes.
     */
    VPRINTF ("Setting endianness.\r\n");
    hpc_set_endianness();

    VPRINTF ("Initializing environment.\r\n");
    init_env();			/* initialize environment for morediags */
    play_hello_tune(0,0);

    VPRINTF ("Executing remainder of diagnostic.\r\n");
    diagsfailed = pon_morediags();	/* run more diags  */

#ifdef ENETBOOT
    /* clear memory cached unless diag fails 
     */
    VPRINTF ("Clearing memory.\r\n");
    clear_memory (diagsfailed & FAIL_CACHES);
#endif

    VPRINTF ("Initializing software and devices.\r\n");
    init_prom_soft (1);
    VPRINTF ("All initialization and diagnostic completed.\r\n");

    if (diagsfailed) {

	setTpButtonAction(main,TPBCONTINUE,WBNOW);
	printf("\nDiagnostics failed.\n[Press any key to continue.]");
	getchar();
	main ();
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
	extern int Verbose;
	char *cmdv[8];
	int cmdc;
	int i;

	cmdc = 0;
	cmdv[cmdc++] = "boot";

	switch (*(mode+1)) {
		case '1':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "ide.IP20";
			break;
		case '2':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()ide.IP20";
			break;
		case '3':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "sashIP20";
			break;
		case '4':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()sashIP20";
			break;
		case '5':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "sashIP20";
			cmdv[cmdc++] = "unix.IP20";
			break;
		case '6':
			cmdv[cmdc++] = "-f";
			cmdv[cmdc++] = "bootp()unix.IP20";
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
	main ();

	return;
}

/* 
 * init_prom_soft - configure prom software 
 */
void
init_prom_soft(int no_init_env)
{
    static int in_data_section = 1;

    config_cache();
    flush_cache();
    set_SR (0);
    _hook_exceptions();
    if (!no_init_env)
    	init_env();		/*  initialize environment */
    _init_saio();		/*  initialize saio library */
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
    set_SR (SR_BERRBIT|SR_IEC|SR_CU1);
}

/* _halt(): work needed by various halt-like routines.
 */
static void
_halt(void)
{
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nOkay to power off the system now.\n\n"
		"Press RESET button to restart.",
		0,DIALOGINFO,DIALOGCENTER|DIALOGBIGFONT);
}

/*
 * halt - bring the system to a quiescent state
 */
void
halt(void)
{
    play_hello_tune(0,1);
    _halt();
}

/*
 * powerdown - attempt a soft power down of the system, alternatively halt
 */
void
powerdown(void)
{
    play_hello_tune(0,1);
    cpu_soft_powerdown();
    _halt();
}

/*
 * restart - attempt to restart
 */
void
restart(void)
{
    static int restart_buttons[] = {DIALOGRESTART,-1};
    play_hello_tune(0,1);
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nOkay to power off the system now.\n\n"
		"Press any key to restart.",
		restart_buttons,DIALOGINFO,
		DIALOGCENTER|DIALOGBIGFONT);
    if (doGui()) setGuiMode(0,GUI_RESET);
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
    extern int Verbose;

    if (!Verbose && doGui()) {
	    setGuiMode(1,0);
    }
    else {
	    p_panelmode();
	    p_cursoff();
    }
    if (ESUCCESS != autoboot(0, 0, 0)) {
	putchar('\n');
	setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
	p_curson();
	p_printf("Unable to boot; press any key to continue: ");
	getchar();
	putchar('\n');
    }

    /* If reboot fails, reinitialize software in case a partially
     * booted program has messed up the prom state.
     */
    EnterInteractiveMode();		/* should not get here */
}

void
enter_imode(void)
{
    _hook_exceptions();
    _init_saio();		/*  initialize saio library */
    alloc_memdesc();		/*  allocate memory descriptors for prom */
    *(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
    *(volatile unsigned int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
    wbflush();
    set_SR (SR_BERRBIT|SR_IEC|SR_CU1);
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
    unsigned bsspage1 = ((unsigned)_edata & ~(NBPP-1)) + NBPP;

    md_alloc (KDM_TO_PHYS(_ftext), 
	      btop(KDM_TO_PHYS((unsigned)_edata - (unsigned)_ftext)),
	      FirmwarePermanent);
    md_alloc (KDM_TO_PHYS(bsspage1),
	      btop(KDM_TO_PHYS((unsigned)_end - (unsigned)bsspage1)),
	      FirmwareTemporary);
    md_alloc (KDM_TO_PHYS(PROM_STACK - STKSZ), btop(STKSZ), FirmwareTemporary);
#else
    /* add what we know to be the prom's stack and bss.  Text and
     * data are safe in prom in I/O space.
     */
    md_alloc (KDM_TO_PHYS(PROM_BSS),
	    btop(KDM_TO_PHYS(PROM_STACK)-KDM_TO_PHYS(PROM_BSS)),
	    FirmwareTemporary);
#endif
}

#ifdef ENETBOOT
/* clear_memory - clear memory from PROM_STACK up */
#define CLRSIZE 0x40		/* matches size in lmem_conf.s */
static void
clear_memory(int uncached)
{
    extern unsigned int memsize;	/* from ml/IP20.c */
    static int clrmem[CLRSIZE / sizeof(int)];
    extern int hmclear(), hmclearend();
    int (*clrfunc)();

    bcopy (hmclear, clrmem, (unsigned)hmclearend - (unsigned)hmclear);
    if (uncached)
	clrfunc = (int (*)())clrmem;
    else
	clrfunc = (int (*)())K1_TO_K0(clrmem);

    /* clear from PROM_STACK to top of memory */
    (*clrfunc)(PROM_STACK, K1_RAMBASE+memsize);
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
