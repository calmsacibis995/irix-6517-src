/***********************************************************************\
*	File:		sysinit.c					*
*									*
*	Contains the code which completes system startup, runs 		*
*	standard diags, checks the system configuration state,		*
*	and finally calls main.						*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h> 
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/diagval_strs.i>

extern int stdio_init;
extern void EnterInteractiveMode(void);
extern void sysctlr_message(char*);
extern void update_boot_stat(void);
extern int set_diagval_disable(int, int);

extern ushort calc_cksum(void);
extern void compare_checksums(void);
extern void update_checksum(void);

void main(void);
void fputest(void);
void init_prom_soft(int);
void reboot(void);
void config_cache(void);
void io4_config(void);
void fix_vpids(void);
void init_tlb(void);
void check_inventory(void);

int  version(void);

extern int _prom;
extern int multi_io4;
extern int verbose_faults;
extern int doass;
extern int stdio_init;
extern struct string_list environ_str;

extern int read_serial(char*);
extern unchar get_cc_rev(int, int);
extern void set_cc_rev(void);
extern void replace_str(char *, char *, struct string_list *);

static void alloc_memdesc(void);
static void start_slave_loop(void);
#ifndef	IP25
static void fix_ecc(int);
#endif
static void show_halt_msg(void);
static void fix_urgent(void);
#if IP19
static void store_my_checksums(void);
#endif /* IP19 */
static void init_piggyback(void);

void reboot(void);

#define MPCONF_LAUNCH_LEVEL 8

#define SLAVE_STACKSIZE 4096

#ifdef IP21
static void early_init_wg(void);
#endif

#if defined(DEBUG)
#   define	DPRINTF(x)	printf x
#else 
#   define	DPRINTF(x)
#endif

/*
 * Set master control flags in the prom.
 */

void 
sysinit(void)
{
    int restart, i;

#ifdef SABLE
    EVCFGINFO->ecfg_debugsw |= VDS_MANUMODE + VDS_DEBUG_PROM;
    EVCFGINFO->ecfg_board[0].eb_type = EVTYPE_IP25;
    EVCFGINFO->ecfg_board[0].eb_cpuarr[0].cpu_enable = 1;
    EVCFGINFO->ecfg_memsize = 32768;	/* 8 mb */
    MPCONF[0].mpconf_magic = MPCONF_MAGIC;
    MPCONF[0].virt_id = 0;
    MPCONF[0].pr_id = get_prid();
#endif

    /* Display the PROM header */
    (void) version();

    /* Initialize the globals pertaining to cache size */
    printf("Sizing caches...\n");
    sysctlr_message("Sizing caches...");
    config_cache();
 
    /* These need to be set to get stdio and certain prom initialization
     * to behave correctly.
     */
    _prom = 1;
    stdio_init = 0;

    /* It takes too long to start probe all of the SCSI busses on
     * large Everest systems, so we set multi_io4 to 0 to force it
     * to only probe the master IO4.
     */
    multi_io4 = 0;

    /* If the debug switch is set we dump verbose fault state.
     * Otherwise, make errors terse.
     */
    if (EVCFGINFO->ecfg_debugsw & VDS_DEBUG_PROM)
	verbose_faults = 1;
    else
        verbose_faults = 0;

    printf("Initializing exception vectors.\r\n");
    _hook_exceptions();
    set_SR(NORMAL_SR);

#ifdef IP21
    printf("Initializing write-gatherer.\n");
    early_init_wg();
#endif

    printf("Initializing IO4 subsystems.\n");
    (void) io4_config();

    printf("Fixing vpids...\n");
    (void) fix_vpids();
 
    /* Set-up environment */
    printf("Initializing environment\n");
    sysctlr_message("Initing environment");
    init_env();

    update_boot_stat();

    /* Initialize system serial number */
    if (read_serial(EVCFGINFO->ecfg_snum) == -1)
	printf("WARNING: serial number is invalid!!\n");

#if IP19 || IP25
    /* Check CC chip revs (can't do this on IP21 until slaves are up) */
    init_piggyback();
#endif

    /*
     * Run additional diags
     */
    
    /* Initialize rest of the system software */
    printf("Initializing software and devices.\r\n");
    init_prom_soft(1);


    printf("All initialization and diagnostics completed.\r\n");

    sysctlr_message("Starting slaves...");
    start_slave_loop();

    /* Fix up the urgent timeout values */
    fix_urgent();

#if IP19
    /* Have each processor store its EAROM checksums. 
     * The IP21 prom does this now, also on TFP can't touch
     * EAROM at this time.
     */
    store_my_checksums();
#endif /* IP19 */

    /* Avoid cache snooping problems. */
    delay(200);

    /*
     * Be sure we do not have and pending interrupts from the slaves
     * entering their slave loops. This can happen using the "pod" 
     * command - where the slaves then send a "i'm in pod" interrupt.
     */
    for (i = 0; i < 128; i++) {
	EV_SET_LOCAL(EV_CIPL0, (__uint64_t)i);
    }

#if IP21
    /* Check CC chip revs (can't do this on IP21 until slaves are up) */
    set_cc_rev();
    init_piggyback();
#endif

    /* If we're on a graphics system, and the NOGFX bit is set,
     * reboot the system.  This is a failsafe for those times when
     * graphics are down and no terminal is available.
     */
    if (EVCFGINFO->ecfg_debugsw & VDS_NOGFX) {
	replace_str("console", "d", &environ_str);
	autoboot(0, "\nThe NOGFX debug switch is set; rebooting system...\n\n",
		 NULL);
    }

    compare_checksums();
    
    check_inventory();

    sysctlr_message("Startup complete.");

    /* Do the right thing depending on how the RESTART stuff is set */
    restart = get_nvreg(NVOFF_RESTART);
    set_nvreg(NVOFF_RESTART, 0);
    update_checksum();
    switch (restart) {
	case PROMOP_RESTART:
	    EnterInteractiveMode();
	    break;

	case PROMOP_REBOOT:
	    reboot();
	    break;

	default: 
	    startup();
	    break;
    }
}


/*
 * init_prom_soft()
 *	Calls the initialization routines for the major
 *	PROM subsystems.
 */

void
init_prom_soft(int no_init_env)
{
    sysctlr_message("Reiniting caches..");
    config_cache();
    flush_cache();

    set_SR(NORMAL_SR);		/* Set SR to its "normal" state */
    _hook_exceptions();

    if (!no_init_env)
	init_env();

    sysctlr_message("Initing saio...");
    _init_saio();
    
    init_tlb();
    alloc_memdesc();
}


/*
 * show_halt_msg()
 *	Display a nice friendly halt message for the user
 */

static void
show_halt_msg(void)
{
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
		    "Press RESET button to restart.",
		    0,DIALOGINFO,DIALOGCENTER|DIALOGBIGFONT);
}


/*
 * halt()
 *	Bring the system to a quiescent state
 */

void
halt()
{
    show_halt_msg();
}


/*
 * powerdown()
 *	Attempt a soft power down of the system.
 */

void
powerdown(void)
{
    cpu_soft_powerdown();
    show_halt_msg();
}


/*
 * restart()
 *	Attempt to restart the machine.
 */

void
restart(void)
{
    static int restart_buttons[] = {DIALOGRESTART,-1};
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nYou may shutdown the system.\n\nPress any key to restart.",
		 restart_buttons, DIALOGINFO, DIALOGCENTER|DIALOGBIGFONT);
    if (doGui()) setGuiMode(0, GUI_RESET);
    startup();
}


/*
 * reboot()
 *	This is supposed to try to reproduce the last system boot
 *	command.
 */

void
reboot(void)
{
    int Verbose = 0;	/* ??? */

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
    EnterInteractiveMode();             /* should not get here */
}


/*
 * enter_imode()
 *	Drop back into Interactive Mode.
 */

void
enter_imode(void)
{
    _hook_exceptions();
    _init_saio();               /*  initialize saio library */
    alloc_memdesc();            /*  allocate memory descriptors for prom */
    set_SR(NORMAL_SR);
    main();
    EnterInteractiveMode();     /* shouldn't get here */
}


/*
 * alloc_memdesc()
 *	Allocate memory descriptors.
 */

static void
alloc_memdesc(void)
{

}

#ifndef	IP25
static void
fix_ecc(int print)
{
	int slot, proc;
	uint spnum;
	int vpid;
	int ecchkdis;

	spnum = load_double_lo((long long *)EV_SPNUM);
	slot = (spnum & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	proc = (spnum & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	if ((get_prid() & C0_REVMASK) >= 0x40)
		ecchkdis = 0;
	else
		ecchkdis = 1;

	EV_SETCONFIG_REG(slot, proc, EV_ECCHKDIS, ecchkdis);

	vpid = EVCFGINFO->ecfg_board[slot].eb_cpuarr[proc].cpu_vpid;
	MPCONF[vpid].pr_id = (int)get_prid();

	if (print)
		printf("Set slot %d proc %d to %d\n", slot, proc, ecchkdis);
}
#endif

/*
 * boot_slaves()
 *	A simple stub routine which clears interrupts before dropping
 *	into the slave loop.
 */

static void 
boot_slaves()
{
#if	IP19
    extern	void	ip19_cache_init(void);
#endif
    int i;

#if IP19
    /* The IP21 prom already does this and the EAROM can't be
     * touched safely at this time on TFP.
     */
    store_my_checksums();
    ip19_cache_init();
#elif IP21
    /* The cc rev number is in the spnum register so other processors
     * can't look at it. we have to save it here so the master can look
     * at it later.
     */
    set_cc_rev();
#endif /* IP21 */

    for (i = 0; i < EVINTR_MAX_LEVELS; i++)
	EV_SET_LOCAL(EV_CIPL0, i);

#if TFP
    fputest();
#endif
    slave_loop();
}



/*
 * void start_slave_loop()
 *    Instructs the slave processors to jump to the slave_loop
 *    routine.
 */

static void
start_slave_loop(void)
{
    uint cpu;
    __psunsigned_t stack;

    /* Don't start slave processors if the NO MP debug switch is set */
    if (EVCFGINFO->ecfg_debugsw & VDS_NOMP) {
	printf("NOTE: NO MP switch set -- skipping slave processor startup\n");
	return;
    }

    for (cpu = 0; cpu < EV_MAX_CPUS; cpu++) {
	if (cpu == cpuid()) {
	    printf("Bootmaster processor already started.\n");
#ifndef	IP25
	    fix_ecc(0);
#endif
	    continue;
        }

	if (MPCONF[cpu].mpconf_magic != MPCONF_MAGIC ||
	   MPCONF[cpu].virt_id != cpu) 
	    continue;

	stack = SLAVESTACK_BASE + ((cpu + 1) * SLAVE_STACKSIZE) - 1;
	stack &= ~0xf;

	printf("Starting processor #%d\n", cpu);

	launch_slave(cpu, (void (*)(int)) boot_slaves, 0, 0, 0,
		     (void*) stack);

	/* Now actually launch the slave processors */
	EV_SET_LOCAL(EV_SENDINT, 
		     EVINTR_VECTOR(MPCONF_LAUNCH_LEVEL, MPCONF[cpu].phys_id));
    }
}

/*
 * fix_urgent
 *	Sets the urgent timeout values for all of the processors in
 *	the system.
 */

static void
fix_urgent(void)
{
    int slot, type;
    int tout_value = ((_get_numcpus() > 15) ? 0xc80 : 0x80);

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {

	type = BOARD(slot)->eb_type;
	if ((type == EVTYPE_IP19 || type == EVTYPE_IP21 || type == EVTYPE_IP25) && 
	    BOARD(slot)->eb_enabled) {

	    EV_SET_CONFIG(slot, EV_A_URGENT_TIMEOUT, tout_value);
	}
    }
}


#if IP19

static void
store_my_checksums(void)
{
    int byte0;
    int vpid;
    unsigned short stored_checksum;

    vpid = cpuid();

    MPCONF[vpid].earom_cksum = calc_cksum();
    delay(2);
    stored_checksum = (load_double_lo((void *)EV_CKSUM1_LOC) << 8);
    delay(2);
    stored_checksum += load_double_lo((void *)EV_CKSUM0_LOC);
    delay(2);

    /* We want to update this guy atomically */
    MPCONF[vpid].stored_cksum = stored_checksum;

    byte0 = load_double_lo((void *)EV_EAROM_BASE);
    delay(2);

    /* If byte0 is wrong, correct it and set our diagval. */

#ifdef _MIPSEL
    if (byte0 != (EV_EAROM_BYTE0 & ~EAROM_BE_MASK)) {
	store_double_lo((void *)EV_EAROM_BASE,
			(EV_EAROM_BYTE0 & ~EAROM_BE_MASK));
	delay(2);
	set_diagval_disable(vpid, EVDIAG_EAROM_REPAIRED);
    }

#else

    if (byte0 != (EV_EAROM_BYTE0 | EAROM_BE_MASK)) {
	store_double_lo((void *)EV_EAROM_BASE,
			(EV_EAROM_BYTE0 | EAROM_BE_MASK));
	delay(2);
	set_diagval_disable(vpid, EVDIAG_EAROM_REPAIRED);
    }

#endif /* _MIPSEL */

}
#endif /* IP19 */

static void
init_piggyback(void)
{
#if defined(IP19)
#   define	MINREV	1
#elif defined(IP21)
#   define	MINREV	1
#elif defined(IP25)
#   define	MINREV	3
#endif

    char *piggy;
    unsigned char minrev;
    int slot, slice, type;

    minrev = 0xff;

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	for (slice = 0; slice < EV_CPU_PER_BOARD; slice++) {
	    if (BOARD(slot)->eb_cpuarr[slice].cpu_diagval == EVDIAG_NOTFOUND){
		continue;
	    }
	    type = BOARD(slot)->eb_type;
	    switch(type) {
	    case EVTYPE_IP19:
	    case EVTYPE_IP21:
	    case EVTYPE_IP25:
		if (BOARD(slot)->eb_enabled) {
		    unsigned char rev;
		    if (!BOARD(slot)->eb_cpuarr[slice].cpu_enable) {
			continue;
		    }
		    rev = get_cc_rev(slot, slice);
		    BOARD(slot)->eb_un.ebun_cpu.eb_ccrev = rev;
		    DPRINTF(("0x%x/0x%x: CC rev(%d)\n", slot, slice, rev));
		    if (rev < minrev) {
			minrev = rev;
		    }
		}
	    }
	}
    }

    piggy = getenv("piggyback_reads");
    if (piggy) {
	if (*piggy == '0') {
	    DPRINTF(("Piggyback reads manually disabled\n"));
	    minrev = 1;	/* old board, piggy back reads are broken... */
	} else if (*piggy == '1') {
	    DPRINTF(("Piggyback reads manually enabled\n"));
	    minrev = MINREV + 1;
	}
    }

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	type = BOARD(slot)->eb_type;
	if ((type==EVTYPE_IP19 || type==EVTYPE_IP21 || type==EVTYPE_IP25) &&
	    BOARD(slot)->eb_enabled) {
	    if (minrev > 1)
		BOARD(slot)->eb_un.ebun_cpu.eb_brdflags |= EV_BFLAG_PIGGYBACK;
	    else
		BOARD(slot)->eb_un.ebun_cpu.eb_brdflags &= ~EV_BFLAG_PIGGYBACK;

	    for (slice = 0; slice < EV_CPU_PER_BOARD; slice++) {
		if ((BOARD(slot)->eb_cpuarr[slice].cpu_diagval ==
		     EVDIAG_NOTFOUND) ||
		    !BOARD(slot)->eb_cpuarr[slice].cpu_enable)
		    continue;
		if (minrev > MINREV) {
		    EV_SETCONFIG_REG(slot, slice << EV_PROCNUM_SHFT, 
				     EV_PGBRDEN, 1);
		} else {
		    EV_SETCONFIG_REG(slot, slice << EV_PROCNUM_SHFT, 
				     EV_PGBRDEN, 0);
		}
	    }
	}
    }
    
    if (minrev > MINREV) {
	printf("Piggyback reads enabled.\n");
    } else {
	DPRINTF(("Piggyback reads disabled\n"));
    }
}

#ifdef IP21
/*
 * early_init_wg -- this function writes 64 words of data to the write gatherer,
 *                  making it flush both of its a&b buffers and insuring that
 *                  all the words in the DB chips have correct parity.
 *
 * This function should be called for each the processor in the system,
 * by that processor (i.e. it doesn't do any good to run it 4 times on
 * cpu 0, it needs to be run on each individual cpu).
 */
#define ROUNDUP(addr, alignment) \
              (void *)((((__psunsigned_t)&addr) + (alignment - 1)) & ~(alignment - 1))

#define SETWG(addr)  EV_SET_LOCAL(EV_WGDST, addr)


#define FLUSH                                                             \
        {                                                                 \
	  volatile int i;                            \
	                                                                  \
	  *(volatile int *)0x8000000018300020 = 0;    /* write to flush offset */  \
          do {                                                            \
	    *(volatile int *)0x8000000018300028 = 0;  /* write to sync offset */   \
	  } while (*(volatile long *)0x9000000018300000 & (0x3f3f)); \
	                                                                  \
	  for(i=0; i < 3000; i++);                                        \
        }


static void
early_init_wg(void)
{
  int i, *iptr;
  char *ptr;
  long count, success=1;

  ptr = (char *)WGTESTMEM_ADDR;
  EV_SET_LOCAL(EV_WGDST, ptr);
  
  /*
   * cause a flush to happen to clear any junk that might
   * happen to be in the write-gatherer after a reset.
   *
   * This could potentially cause a DB chip parity error
   * and a data sent error on channel zero.  If you get
   * those, you're on your own.  You could ignore them since
   * they're only for junk data that we don't care about,
   * but then again, I'm not sure.
   */
  FLUSH;

  count = 0;
  while (count < 50)  /* keep going until we succeed or the test gets boring */
   {
     success = 1;
     
     for(i=0; i < 64; i++)         /* write 64 words to flush out the write gather (both a&b buffers) */
      {
	*(volatile int *)0x8000000018300000 = i;
      }

     FLUSH;

     for(i=0, iptr = (int *)ptr; i < 32; i+=2)
      {
	if (iptr[i] != i+1+32)
	 {
#ifdef DEBUG
	   printf("1(%d): early_init_wg failed in checking data (0x%x != 0x%x)\n", count, iptr[i], i+1+32);
#endif
	   success = 0;
	 }
	if (iptr[i+1] != i+32)
	 {
#ifdef DEBUG
	   printf("2(%d): early_init_wg failed in checking data (0x%x != 0x%x)\n", count, iptr[i+1], i+32);
#endif
	   success = 0;
	 }
      }

     count++;
     if (success == 1)
       break;
   }

  if (count > 1)
   {
     printf("  WARNING: Write gatherer initialization failed %d time(s) before succeeding.\n",
	    count-1);
   }
    
}
#endif

