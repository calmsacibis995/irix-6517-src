/***********************************************************************\
*	File:		EVEREST.c					*
*									*
* 	This file contains the the definitions of the cpu-dependent	*
*	functions for Everest-based systems.  For the most part,	*
*	this file should work for both IP19 and IP21 based machines.	*
*									*
\***********************************************************************/

#include <sys/i8251uart.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/time.h>
#include <sys/ktime.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/iotlb.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/i8254clock.h>
#include <sys/z8530.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/cfgtree.h>
#include <arcs/types.h>
#include <arcs/time.h>

extern int	month_days[12];		/* in libsc.a */
extern int	sk_sable, s1_num;

static unsigned int master_io4(void);
/*
 * cpu_makecfgroot()
 *	Creates the Root node of the Everest ARCS configuration
 *	tree.
 */

static int
log2(int x)
{
    int n, v;
    for (n = 0, v = 1; v < x; v <<= 1, n++) ;
    return n;
}

#define SYSBOARDIDLEN   9 

static COMPONENT root_template = {
    SystemClass,	
    ARC,
    (IDENTIFIERFLAG)0,
    SGI_ARCS_VERS,
    SGI_ARCS_REV,
    0,
    0,
    0,
    SYSBOARDIDLEN,
    0
};

cfgnode_t *
cpu_makecfgroot(void)
{
    COMPONENT *root = &root_template;
    int	i = cpuid();
 
    if ((MPCONF[i].pr_id & C0_IMPMASK) == 0x1000) {
	root->Identifier = "SGI-IP21";
    } else if ((MPCONF[i].pr_id & C0_IMPMASK) == 0x0900) {
	root->Identifier = "SGI-IP25";
    } else if ((MPCONF[i].pr_id & C0_REVMASK) >= 0x40) {
	root->Identifier = "SGI-IP19";
    } else {
	root->Identifier = "SGI-IP19";
    }

    root = AddChild(NULL, &root_template, (void*)NULL);
    if (root == (COMPONENT*)NULL)
	cpu_acpanic("root");

    return ((cfgnode_t*)root);
}


/*
 * cpu_install()
 *	Adds nodes in the ARCS configuration tree for
 *	all of the cpus and their subcomponents (FPUS and
 *	caches).  This routine deals with both IP19 and
 *	IP21 based boards (and hopefully their descendents,
 *	if any ever show up).
 */ 

static COMPONENT cputmpl = {
    ProcessorClass,             /* Class */
    CPU,                        /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                         	/* ConfigurationDataSize */
    0,         			/* IdentifierLength */
    NULL	                /* Identifier */
};
static COMPONENT fputmpl = {
    ProcessorClass,             /* Class */
    FPU,                        /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* IdentifierLength */
    NULL                 	/* Identifier */
};
static COMPONENT cachetmpl = {
    CacheClass,                 /* Class */
    PrimaryICache,              /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* IdentifierLength */
    NULL                        /* Identifier */
};

static COMPONENT memtmpl = {
    MemoryClass,                /* Class */
    Memory,                     /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* Identifier Length */
    NULL                        /* Identifier */
};

/*
 * All of this information should be drawn from the MPCONF data
 * structure.  For the timebeing, however, we're going to wimp
 * out.
 */
void
cpu_install(COMPONENT *root)
{
    COMPONENT *c, *tmp;
    union key_u key;
    int i;
    enum {
	cpuR8000, 
	cpuR4000, 
	cpuR4400, 
	cpuR10000
    }   cpuType;

#if TFP
    extern int _picache_size;
    extern int _pdcache_size;
    extern int _sidcache_size;
#endif

    for (i = 0; i < EV_MAX_CPUS; i++) {

	/* Skip this MPCONF entry if it doesn't look correct */

	if (MPCONF[i].mpconf_magic != MPCONF_MAGIC || MPCONF[i].virt_id != i)
	    continue;

	/*
	 * Don't change the Identifier. The "hinv" command knows
	 * that the first 5 characters are "MIPS-" and will not
	 * work correctly if you change this to, say, "SGI-".
	 */

	if ((MPCONF[i].pr_id & C0_IMPMASK) == 0x1000) {
	    cpuType = cpuR8000;
	    cputmpl.Identifier = "MIPS-R8000";
	    fputmpl.Identifier = "MIPS-R8000FPC";
	} else if ((MPCONF[i].pr_id & C0_IMPMASK) == 0x0900) {
	    cpuType = cpuR10000;
	    cputmpl.Identifier = "MIPS-R10000";
	    fputmpl.Identifier = "MIPS-R10000FPC";
	} else if ((MPCONF[i].pr_id & C0_REVMASK) >= 0x40) {
	    cpuType = cpuR4400;
	    cputmpl.Identifier = "MIPS-R4400";
	    fputmpl.Identifier = "MIPS-R4400FPC";
	} else {
	    cpuType = cpuR4000;
	    cputmpl.Identifier = "MIPS-R4000";
	    fputmpl.Identifier = "MIPS-R4000FPC";
	}

	cputmpl.IdentifierLength = strlen(cputmpl.Identifier);
	fputmpl.IdentifierLength = strlen(fputmpl.Identifier);

	c = AddChild(root, &cputmpl, (void*) NULL);
	if (c == (COMPONENT*) NULL) cpu_acpanic("cpu");
	c->Key = i;			/* HACK */

	tmp = AddChild(c,&fputmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("fpu");
	tmp->Key = i;

	/* Add I-cache information */

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("icache");

	key.cache.c_bsize = 1;

	switch(cpuType) {
	case cpuR4000:
	    key.cache.c_lsize = 4;		/* R4000 */
	    key.cache.c_size = 1;
	    break;
	case cpuR4400:
	    key.cache.c_lsize = 4;		/* R4000 */
	    key.cache.c_size =  2; 
	    break;	
	case cpuR8000:
#ifdef	TFP
	    key.cache.c_lsize = log2(32);
	    key.cache.c_size =  log2(_picache_size / 4096);
#endif
	    break;
	case cpuR10000:
#if defined(R10000)
	    key.cache.c_lsize = 4;		/* R4000 */
	    key.cache.c_size = log2(iCacheSize() / 4096);
#endif
	    break;
	}

	tmp->Type = PrimaryICache;
	tmp->Key = key.FullKey;

	/* Add D-cache information */ 

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("dcache");

	key.cache.c_bsize = 1;
	switch(cpuType) {
	case cpuR4000:
	    key.cache.c_lsize = 4;
	    key.cache.c_size = 1;
	    break;
	case cpuR4400:
	    key.cache.c_lsize = 4;
	    key.cache.c_size =  2;
	    break;
	case cpuR8000:
#ifdef	TFP
	    key.cache.c_lsize = log2(32);
	    key.cache.c_size =  log2(_pdcache_size / 4096);
#endif
	    break;
	case cpuR10000:
#if defined(R10000)
	    key.cache.c_lsize = 4;		/* R4000 */
	    key.cache.c_size = log2(dCacheSize() / 4096);
#endif
	    break;
	}

	tmp->Type = PrimaryDCache;
	tmp->Key = key.FullKey;

	/* Add secondary cache information */

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("s_cache");

	key.cache.c_bsize = 1;
	key.cache.c_size =  log2( (1 << MPCONF[i].scache_size) / 4096);

	switch(cpuType) {
	case cpuR4000:
	case cpuR4400:
	case cpuR10000:
	    key.cache.c_lsize = 7;		/* R4000 */
	    break;
	case cpuR8000:
	    key.cache.c_lsize = log2(512);
	    break;
	}

	tmp->Type = SecondaryCache;
	tmp->Key = key.FullKey;
    }

    tmp = AddChild(root, &memtmpl, (void *)NULL);
    if (tmp == (COMPONENT *)NULL) cpu_acpanic("main memory ");
    tmp->Key = EVCFGINFO->ecfg_memsize >> 4;       /* number of 4k pages */
}

/*
 * cpu_reset
 *	Perform any board-specific start up initialization.
 */

void
cpu_reset(void)
{
	initsplocks();		/* initialize spin lock subsystem */
}


/*
 * cpu_hardreset
 *	Performs a hard reset by slamming the Kamikaze reset
 *	vector.
 */

void
cpu_hardreset(void)
{
    EV_SET_LOCAL(EV_KZRESET, 1);

    for (;;) /* Loop Forever */ ;
}


/*
 * cpu_show_fault()
 */

/*ARGSUSED*/
void
cpu_show_fault(unsigned long saved_cause)
{
}


/*
 * cpu_soft_powerdown()
 */

void
cpu_soft_powerdown(void)
{
}


/*
 * cpuid() returns logical ID of executing processor, as
 * set up by prom.
 */
int
cpuid(void)
{
	register evreg_t slotproc;
	register uint slot; 
	register uint proc; 

	slotproc = (evreg_t)EV_GET_LOCAL(EV_SPNUM);
	slot = (uint)((slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
	proc = (uint)((slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

#ifdef LARGE_CPU_COUNT_EVEREST
	{
		int cpuid2;
		cpuid2 = (int)EVCFG_CPUID(slot,proc);
		if ((cpuid2 < REAL_EV_MAX_CPUS) && (cpuid2 & 0x01))
			return( EV_MAX_CPUS-cpuid2 );
	}
#endif /* LARGE_CPU_COUNT_EVEREST */
	return((int)EVCFG_CPUID(slot,proc));
}


/*
 * cpu_get_memsize()
 */

unsigned int
cpu_get_memsize(void)
{
	return (EVCFGINFO->ecfg_memsize >> 4);
}

/*
 * cpu_get_disp_str
 */

char *
cpu_get_disp_str(void)
{
    return "video()";
}


/*
 * cpu_get_kbd_str()
 */

char *
cpu_get_kbd_str(void)
{
    return "keyboard()";
}


/*
 * cpu_get_serial()
 */

char *
cpu_get_serial(void)
{
    if (EVCFGINFO->ecfg_debugsw & VDS_MANUMODE)
 	return "multi(0)serial(6)";
    else
	return "multi(0)serial(0)";
}


/*
 * cpu_errputc()
 *
 * Print a single character to the system's console.  The console
 * can be switched between the Manufacturing mode console, which
 * hangs off the the System Controller, and the System console, which
 * hangs off of the serial port.  We switch dynamically based on
 * an as yet undecided global.
 */

void
cpu_errputc(char c)
{
    int timeleft;
    if (sk_sable)
	timeleft = 1;
    else
	timeleft = 10000;

    if (EVCFGINFO->ecfg_debugsw & VDS_MANUMODE) {
	while (timeleft-- && !(load_double_lo((long long *)EV_UART_CMD)&I8251_TXRDY))
	    /* spin */ ;

	store_double_lo((long long *)EV_UART_DATA, c);
    } else {
	static volatile char *cntrl = NULL;
	static volatile char *data  = (char*) NULL;

	if (cntrl == NULL) {
	    cntrl = (volatile char*) DUART2A_CNTRL;
	    data  = (volatile char*) DUART2A_DATA;
	} 

	while (timeleft-- && !(RD_RR0(cntrl) & RR0_TX_EMPTY))
	    /* spin */ ;

	WR_DATA(data, c);
    }
}


/*
 * cpu_mem_init()
 */

void
cpu_mem_init(void)
{
    unsigned int numfreepages;
    extern int _ftext[];
    extern int _end[];


    /* Allocate space for the IO4 PROM.  And its various pieces (GFXPROM,
     * ENET bufs, etc.  We use the _ftext and _end defines so that we
     * can move the PROM around in memory without having to recompile.
     */
    md_add(PHYS_RAMBASE, 2, FirmwareTemporary);
    md_add(K0_TO_PHYS(_ftext),
	   arcs_btop(K0_TO_PHYS(_end) - K0_TO_PHYS(_ftext) + IO4STACK_SIZE),
	   FirmwareTemporary);
    md_add(K1_TO_PHYS(EVCFGINFO_ADDR), 1, FirmwareTemporary);
    md_add(K1_TO_PHYS(MPCONF_ADDR), 1, FirmwareTemporary);
    md_add(K1_TO_PHYS(FLASHBUF_BASE), arcs_btop(FLASHBUF_SIZE), FirmwareTemporary);
    md_add(K0_TO_PHYS(GFXPROM_BASE), arcs_btop(GFXPROM_SIZE), FirmwareTemporary);
    md_add(K0_TO_PHYS(SLAVESTACK_BASE), arcs_btop(SLAVESTACK_SIZE), 
	   FirmwareTemporary);
    md_add(ENETBUFS_BASE, arcs_btop(ENETBUFS_SIZE), FirmwareTemporary);

    /* Put a permanent memory descriptor in for the IP19 PROM 
     */
    md_add(K0_TO_PHYS(PROM_BASE), arcs_btop(PROM_SIZE), FirmwarePermanent);

    /* Free all of the space between the beginning of the NODEBUGUNIX 
     * address and the base of the flash prom buffer.
     */
    md_add(K0_TO_PHYS(NODEBUGUNIX_ADDR), arcs_btop(FLASHBUF_BASE - NODEBUGUNIX_ADDR),
	   FreeMemory);

    /* Put in a memory descriptor for the space occupied by the 
     * debugging PROM.
     */
    md_add(K0_TO_PHYS(DPROM_BASE), arcs_btop(DPROM_SIZE), FreeMemory);

    /* Finally, all remaining memory above 32 meg (if any exists) is
     * available for use.  Because the PROM is limited by the size of
     * Kseg0, we never allow more than 100,000 free pages.
     */
    numfreepages = (unsigned int)cpu_get_memsize() 
		    - (unsigned int)arcs_btop(K0_TO_PHYS(FREEMEM_BASE));
    if (numfreepages > 100000)
	numfreepages = 100000;

    if (numfreepages  > 0)
	md_add(K0_TO_PHYS(FREEMEM_BASE), numfreepages, FreeMemory); 
}


/*
 * cpu_acpanic()
 */

void
cpu_acpanic(char *str)
{
    printf("Cannot malloc %s component of config tree\n", str);
    panic("Incomplete config tree -- cannot continue.\n");
}


/*
 * cpu_savecfg()
 */

LONG
cpu_savecfg(void)
{
    return ENOSPC;
}


/*
 * cpu_get_mouse()
 */

char *
cpu_get_mouse(void)
{
    return("serial(3)pointer()");
}


/*
 * find the frequency (in HZ) of the running CPU
 */
unsigned int
cpu_get_freq(int cpuid)
{
	register evreg_t slotproc;
	register uint slot; 
	register uint proc; 

	slotproc = MPCONF[cpuid].phys_id;
	slot = (uint)((slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
	proc = (uint)((slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

	return(1000000*EVCFG_CPUSPEED(slot,proc));
}


/*
 * Clear all appropriate error latches in the system.
 * To be used mainly in conjunction with "nofault"
 * accesses.
 */
void
cpu_clear_errors(void)
{
	/* TBD */
}


/*
 * Print all appropriate error state in system.
 */
void
cpu_print_errors(void)
{
	/* TBD */ }


/*
 * EPC support
 */

static COMPONENT epctmpl = {
	AdapterClass,			/* Class */
	MultiFunctionAdapter,		/* Type */
	(IDENTIFIERFLAG)0,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	7,				/* IdentifierLength */
	"EPC1.0"			/* Identifier */
};

/* Move to epc.c? */
void
epc_install(COMPONENT *root)
{
	root = AddChild(root,&epctmpl,(void *)NULL);
	if (root == (COMPONENT *)NULL) cpu_acpanic("EPC");

	return;
}


/*
 * cpu_get_tod -- get current time-of-day from RTC and prom info, and 
 * return it in ARCS TIMEINFO format.  the libsc routine get_tod will 
 * convert it to seconds for those who need it in that form.
 */
void
cpu_get_tod(TIMEINFO *t)
{
	int month, day, year, hours, mins, secs;
	timespec_t tvp;
	evreg_t elapsed_nsec;
	unsigned int nsec_per_cycle;

	/*
	 * Calculate number of nanoseconds in 1 clock cycle 
	 */
/*
	nsec_per_cycle = NSEC_PER_SEC / EVCFGINFO->ecfg_clkfreq;
*/
	nsec_per_cycle = NSEC_PER_SEC / 50000000; /* FOR NOW */
#if	TOD_DEUBG
	printf("cpu_tod: nsec_per_cyc %d\n", nsec_per_cycle);
#endif
	/*
	 * Read nanosecond
	 */
	tvp.tv_sec = EVCFGINFO->ecfg_secs;
	tvp.tv_nsec = EVCFGINFO->ecfg_nanosecs;

	elapsed_nsec = EV_GET_LOCAL(EV_RTC) * nsec_per_cycle;

#if	TOD_DEUBG
	printf("elap_n 0x%llx, YRREF %d, year %d, month %d day %d\n",
		elapsed_nsec, YRREF, year, month, day);
#endif

	/*
	 * Update second and nanosecond counts.
	 */ 
	while (elapsed_nsec >= NSEC_PER_SEC) {
	    elapsed_nsec -= NSEC_PER_SEC;
	    tvp.tv_sec++;
        }
	tvp.tv_nsec += elapsed_nsec;
		
	/*
	 * Break seconds in year into month, day, hours, minutes, seconds
	 */
	year = 0;			/* XXX year is not really set! */
	for (month = 0;
	   tvp.tv_sec >= month_days[month]*SECDAY;
	   tvp.tv_sec -= month_days[month++]*SECDAY)
		continue;
	month++;

	for (day = 1; tvp.tv_sec >= SECDAY; day++, tvp.tv_sec -= SECDAY)
		continue;

	for (hours = 0;
	    tvp.tv_sec >= SECHOUR;
	    hours++, tvp.tv_sec -= SECHOUR)
		continue;

	for (mins = 0;
	    tvp.tv_sec >= SECMIN;
	    mins++, tvp.tv_sec -= SECMIN)
		continue;
	
	secs = tvp.tv_sec;

	/* set up ARCS time structure */
	t->Month = month;
	t->Day = day;
	t->Year = year + YRREF;
	t->Hour = hours;
	t->Minutes = mins;
	t->Seconds = secs;
	t->Milliseconds = (unsigned short)(tvp.tv_nsec/1000);

#if	TOD_DEUBG
	printf(" ctod: year %d (%d), mo %d day %d hours %d mins %d secs %d\n",
		year, year + YRREF, month, day, hours, mins, secs);
	printf(" t->: year %d, month %d, day %d hour %d min %d sec %d\n",
		(int)t->Year,(int)t->Month,(int)t->Day, 
		(int)t->Hour, (int)t->Minutes, (int)t->Seconds);
#endif

	return;
}

/*ARGSUSED*/
void
spunlock(lock_t lock, int ospl)
{
/* TBD */
}


/*
 * cpu_get_eaddr()
 *	Returns the system ethernet address.
 *	Expects the prom in IO2/IO3 format (part number 070-0355-001)
 */
void
cpu_get_eaddr(u_char eaddr[])
{
    int i;

#define	PAD 16
    for (i = 0; i < 6; i++) {
	if (sk_sable)
	    eaddr[i] = 0;
	else
	    eaddr[i] = (u_char)EPC_GET(EPC_REGION, EPC_ADAPTER,
				       EPC_ENETPROM + (5 - i) * PAD);
    }
}


/*
 * ecc_error_decode()
 *	Does something, don't know exactly what.
 */

/*ARGSUSED*/
void
ecc_error_decode(u_int ecc_error_reg)
{
    printf("Not Yet Implemented.  Sorry, dude.\n");
}


/*
 * dump_evconfig
 *	Prints out the contents of the everest configuration table.
 */

static void 
dump_general(evbrdinfo_t *brd)
{
    printf("   Rev: %d\tInventory: 0x%x\tDiag Value: 0x%x, %s\n",
	   brd->eb_rev, brd->eb_inventory, brd->eb_diagval,
	   (brd->eb_enabled ? "Enabled" : "Disabled"));
}

 
static void 
dump_ip19(evbrdinfo_t *brd)
{
    int i;
    evcpucfg_t *cpu;

    dump_general(brd);

    for (i = 0; i < EV_MAX_CPUS_BOARD; i++) {
	cpu = &(brd->eb_cpuarr[i]);
 
	printf("\tCPU %d: Inventory 0x%x, DiagVal 0x%x, Info %x\n",
	       i, cpu->cpu_inventory, cpu->cpu_diagval, 
	       cpu->cpu_info);
	printf("\t      VPID %d, Speed %d MHz, Cache Size %d MB, %s\n",
	       cpu->cpu_vpid, cpu->cpu_speed, 
	       1 << (cpu->cpu_cachesz - 10),
	       (cpu->cpu_enable ? "Enabled" : "Disabled"));
    } 
}

static void
dump_ip21(evbrdinfo_t *brd)
{
    dump_general(brd);
}

static void
dump_ip25(evbrdinfo_t *brd)
{
    dump_general(brd);
}

static void 
dump_io4(evbrdinfo_t *brd)
{
    int i;
    evioacfg_t *ioa;

    dump_general(brd);
    printf("  Window Number: %d\n", brd->eb_io.eb_winnum);

    for (i = 1; i < IO4_MAX_PADAPS; i++) {
	
	ioa = &(brd->eb_ioarr[i]);

	printf("\tPADAP %d: ", i);
	switch(ioa->ioa_type) {
	case IO4_ADAP_EPC:
	    printf("EPC");
	    break;
	case IO4_ADAP_SCSI:
	    printf("S1");
	    break;
	case IO4_ADAP_SCIP:
	    printf("SCIP");
	    break;
	case IO4_ADAP_FCHIP:
	    printf("F");
	    break;
	default:
	    printf("N/A");
	    break;
	}

	printf(" (0x%x), Inventory 0x%x, DiagVal 0x%x, VirtID %d, %s\n",
	       ioa->ioa_type, ioa->ioa_inventory, 
	       ioa->ioa_diagval, ioa->ioa_virtid, 
	       (ioa->ioa_enable ? "Enabled" : "Disabled"));
    }
}


static void 
dump_mc3(evbrdinfo_t *brd)
{
    int i;
    evbnkcfg_t *mem;

    dump_general(brd);

    printf("   Virtual MC3: %d\n", brd->eb_mem.eb_mc3num);
	
    for (i = 0; i < MC3_NUM_BANKS; i++) {
	mem = &(brd->eb_banks[i]);

	printf("\tBank %d: IP %d, IF %d, Size %d, Bloc 0x%x\n",
	       i, mem->bnk_ip, mem->bnk_if, mem->bnk_size, 
	       mem->bnk_bloc);
	printf("\t        Inventory 0x%x, DiagVal 0x%x, %s\n",
	       mem->bnk_inventory, mem->bnk_diagval,
	       (mem->bnk_enable ? "Enabled" : "Disabled"));
    }
}


void
dump_evconfig(void)
{
    int slot;	
    evbrdinfo_t *brd;

    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		
	brd = &(EVCFGINFO->ecfg_board[slot]);

	printf("Slot %d: Type = 0x%x, Name = ", slot, brd->eb_type);
	switch(brd->eb_type) {
	case EVTYPE_IP19:
	    printf("IP19\n");
	    dump_ip19(brd);
	    break;

	case EVTYPE_IP21:
	    printf("IP21\n");
	    dump_ip21(brd);
	    break;

	case EVTYPE_IP25:
	    printf("IP25\n");
	    dump_ip25(brd);
	    break;

	case EVTYPE_IO4:
	    printf("IO4\n");
	    dump_io4(brd);
	    break;

	case EVTYPE_MC3:
	    printf("MC3\n");
	    dump_mc3(brd);
	    break;

	case EVTYPE_EMPTY:
	    printf("EMPTY\n");
	    dump_general(brd);
	    break;
	default:
	    printf("Unrecognized board type\n");
	    dump_general(brd);
	    break;
	}
    }
} 


/*
 * ev_scsiedtinit()
 * 	Do Everest-specific SCSI probe and initialization code.
 */

void
ev_scsiedtinit(COMPONENT *c)
{
    extern void scsi_init(void);
    int 	slot;
    uint	padap;
    evbrdinfo_t *brd;
    uint 	found = 0;

    /*
     * Now we initialize the s1 chips on the IO4.  We cruise
     * through all of the code and poke things appropriately.
     * We actually should move this code down into io4_config,
     * but at this point it isn't quite ready for this.
     */
    s1_num = 0;			/* Start off thinking 0 S1 chips */
    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {

	/* Skip this board if it isn't an IO4 or it is disabled */
	brd = &(EVCFGINFO->ecfg_board[slot]);
	if (brd->eb_type != EVTYPE_IO4 || !brd->eb_enabled)
	    continue;

	found++;

	for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
	    if (brd->eb_ioarr[padap].ioa_enable) {
		if (brd->eb_ioarr[padap].ioa_type == IO4_ADAP_SCIP) {
		    printf("Checking SCIP IOA %d on IO4 in slot %d --", padap, slot);
		    if (scip_diag(slot, padap, brd->eb_io.eb_winnum))
			printf("-*** failed ***\n");
		    else
			printf(" passed.\n");
		    s1_init(slot, padap, brd->eb_io.eb_winnum);
		}
		else if (brd->eb_ioarr[padap].ioa_type == IO4_ADAP_SCSI)
		    s1_init(slot, padap, brd->eb_io.eb_winnum);
	    }
	}
    }

    if (!found)
	printf("\nWARNING: No IO4 boards found!\n\n");
    else {
	scsi_init();
	us_delay(500000);		/* 1/2 second delay - required to 
					  let scsi bus recover after reset */
	scsiedtinit(c);
    }
}

/*
 * prom_rev
 *	Returns the current IP19 prom revision.
 */

int 
prom_rev(void)
{
    evreg_t physid;
    uint slot, slice;

    physid = EV_GET_LOCAL(EV_SPNUM);
    slot =  (uint)((physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT);
    slice = (uint)((physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT);

    return EVCFG_CPUPROMREV(slot, slice); 
}


/*
 * master_io4()
 *	Finds the master IO4 board in the system and returns its slot
 *	number.
 */

static unsigned int
master_io4(void)
{
    int slot;
    static unsigned found_slot = 0;
 
    if (!found_slot) {	
	for (slot = EV_MAX_SLOTS; slot >= 0; slot--) {
   	    evbrdinfo_t *brd = &(EVCFGINFO->ecfg_board[slot]);

	    if (((brd->eb_type & EVCLASS_MASK) == EVCLASS_IO) && 
		brd->eb_enabled) 
		found_slot = slot;
	}
    }

    return found_slot;
}


/*
 * ev_flush_caches
 *	Flush all the caches in the Everest system.
 */

void
FlushAllCaches(void)
{
#if TFP
    flush_cache();
#endif
    ev_flush_caches();
}

void
ev_flush_caches(void)
{
    flush_iocache(master_io4());
}

int
flush_iocache(int slot)
{
    int i;
    volatile unsigned int cache_tag, data;
    volatile unsigned int *line;

    /* Flush the IO4 cache lines by reading the physical tags
     * and then generating a coherent block read from the cpu. 
     */
    for (i = IO4_CONF_CACHETAG0L; i <= IO4_CONF_CACHETAG3U; i += 2) {
	cache_tag = (unsigned int)IO4_GETCONF_REG(slot, i+1);
	if (cache_tag & 0x02) {
	    cache_tag = (unsigned int)IO4_GETCONF_REG(slot, i);
	    line = (volatile unsigned int *)PHYS_TO_K0(cache_tag << 7);
	    data = *line;
	    *line = data;
	}
    }

    /* Flush the processor data caches */
    flush_cache();
    return 0;
}

/*
 * MPC_v_to_p returns "vid"'s hw-defined identification number 
 * from the mpconf struct after doing some validity checks.
 * This routine therefore requires only a valid mpconf struct 
 * (which is initialized by prom at boottime) to work.  Debugging
 * output (if enabled) must use only a stack-buffer and _errputs().
 */
int
MPC_v_to_p(int vid)
{
#if defined(PDADEBUG)
    char buf[128];
#endif /* PDADEBUG */
    register uint mpc_vid, phys_idx;
    int mpconfvid;

#if LARGE_CPU_COUNT_EVEREST
    if (vid & 0x01) {
	    /* We consider all low numbered odd vids to
	     * be invalid since we're re-mapping them
	     * to the high end of the cpu number range.
	     * High numbered odd vids need to access the
	     * MPCONF info using the original low cpu
	     * numbers since MPCONF does not contain the
	     * entries for high numbered cpus.
	     */
	    if (vid < REAL_EV_MAX_CPUS) {
	    	return(-1);
	    }
	    mpconfvid = EV_MAX_CPUS - vid;
    } else
#endif 
	    mpconfvid = vid;
 
    if (MPCONF[mpconfvid].mpconf_magic != MPCONF_MAGIC) {
#if defined(PDADEBUG)
	sprintf(buf,"  MPC_v_to_p: bad mpconf magic (0x%x) on vid %d\n",
		MPCONF[mpconfvid].mpconf_magic, vid);
	_errputs(buf);
#endif /* PDADEBUG */
	return(-1);
    }
    phys_idx = MPCONF[mpconfvid].phys_id;
    mpc_vid = MPCONF[mpconfvid].virt_id;
#ifdef LARGE_CPU_COUNT_EVEREST
    if (vid &0x01)
	mpc_vid = EV_MAX_CPUS - mpc_vid;
#endif
    if (vid != mpc_vid) {
#if defined(PDADEBUG)
	if (mpc_vid) {
	    sprintf(buf,"  ERROR, mpconf[%d].virt_id == %d (not %d)\n",
		vid, mpc_vid, vid);
	    _errputs(buf);
	}
#endif /* PDADEBUG */
	return(-2);
    }
#if defined(PDADEBUG)
    if (mpc_vid && (vid != mpc_vid)) {	/* show vid0 but not inval mpc_vids */
	sprintf(buf,"vid %d --> phys_idx %d\n", mpc_vid, phys_idx);
	_errputs(buf);
    }
#endif /* PDADEBUG */

    return((int)phys_idx);

} /* MPC_v_to_p */

/*
 * Make an entry in the TLB to map the IOspace corresponding to IO4 'window '
 * and adapter 'anum' into K2 space address
 */
/*
 * Function : tlbentry
 * Description :
 * Make an entry in the TLB for the given window and adapter No,
 * and return the virtual address mapped.
 * This is a simpleton tlb mapping, and is used mostly to
 * map the diagnostic accesses to the Large window address.
 *
 * IP19:
 * Page size of each entry is assumed to be 16 Mbytes, thus
 * each entry maps to 32 Mbytes. ( R4000 maps 2 pages in one shot)
 *
 * IP21:
 * TFP Maps to 64K, 1 page per shot.  When/if we can get the larger
 * page sizes, (1, 4, 16 MB) working, the code should be changed
 * to reflect this
 *
 * Starting Virtual address is IOMAP_BASE (0xC0000000)
 * Each entry in tlb_entries maps  0x01FFFFFF
 * Returns the mapped virtual address if successful, else panic
 *
 * Caution : User of tlbentry should make sure to remove this entry
 *           after its use. Otherwise the table may overflow.
 */

/*
 * NOTE :
 * Graphics cannot use this routine since it needs a fixed address to 
 * be remapped a few times to different physical address .. 
 * Graphics now uses address 0xf0000000 and DONT use that address 
 */
#define	FIRST_TLB_ENTRY	2
#define	MAX_TLBSZ	8


#define	VADDR_BASE	( IOMAP_BASE + 0x20000000 )

#ifdef TFP
caddr_t tlb_entries[MAX_TLBSZ];

caddr_t
tlbentry(int window, int anum, unsigned offset)
/* offset should be < IOMAP_VPAGESIZE */
{
    caddr_t 	vpn2;
    unsigned 	pfno;
    pde_t	pfnevn;
    int         i, indx;

    /* Get the address we can use.      */
    for (i=0; i < MAX_TLBSZ; i++){
        if (tlb_entries[i] == 0){
            tlb_entries[i] = (caddr_t) 1L;
            break;
        }
    }

    if (i == MAX_TLBSZ)
        panic("TLB entries exhausted...\n");
   

    /* no doubling a la R4000, since only one page per entry */
    vpn2  = (caddr_t)(VADDR_BASE + ( i << (IOMAP_VPAGESHIFT)));
    tlb_entries[i] = vpn2;

    /* add offset - if less than 0x10000, will be wiped */
    pfno  = LWIN_PFN(window, anum) + (offset >> PTE_PFNSHFT);

    pfnevn.pgi =
	(pfno << 8) | 0x58; /* UNCACHED | MODIFIED | VALID */

    printf("vpn2: 0x%llx, pfno: 0x%x, pgi: 0x%x\n",
	    (long long)vpn2, pfno, pfnevn.pgi);

    /* just hardwire the CPU to use 16 MB page size */
    /* set_pagesize () values:
	0 is 4k, 1 is 8k, 2 is 16k, 3 is 64k, 4 is 64k (256 k broken),
	5 is 1 MB, 6 is 4 MB, 7 is 16M
    */
    set_pagesize(3);

    /* no page masking needed here - no variable size page masks */

    tlbwired(i+FIRST_TLB_ENTRY, 0, (caddr_t)vpn2, pfnevn.pte);

    return((caddr_t)vpn2);
}
#else /* IP19*/
unsigned tlb_entries[MAX_TLBSZ];

caddr_t
tlbentry(int window, int anum, unsigned offset)
/* offset should be < IOMAP_VPAGESIZE */
{
    unsigned    vpn2 ,old_pgmask, pfno;
    pde_t	pfnevn, pfnodd;
    int         i;

    /* Get the address we can use.      */
    for (i=0; i < MAX_TLBSZ; i++){
        if (tlb_entries[i] == 0){
            tlb_entries[i] = 1;
            break;
        }
    }

    if (i == MAX_TLBSZ)
        panic("TLB entries exhausted...\n");
   

    vpn2  = (unsigned) (VADDR_BASE + ( i << (IOMAP_VPAGESHIFT + 1)));
    tlb_entries[i] = vpn2;

    pfno  = LWIN_PFN(window, anum) + (offset >> 12);
    pfnevn.pgi = (pfno << 6 ) | 0x17; /* UNCACHED|MODIFIED|VALID|GLOBAL */
    pfno  += (IOMAP_VPAGESIZE >> 12);
    pfnodd.pgi = (pfno << 6 ) | 0x17;
    old_pgmask = get_pgmask();
    set_pgmask(IOMAP_TLBPAGEMASK);
    tlbwired(i+FIRST_TLB_ENTRY, 0, (caddr_t)vpn2, pfnevn.pgi, pfnodd.pgi);
    set_pgmask(old_pgmask);

    return((caddr_t)vpn2);
}
#endif	/* TFP */

int
tlbrmentry(caddr_t vaddr)
{
    __scunsigned_t  i;
    
#ifdef TFP
    i = ((__scunsigned_t)vaddr - VADDR_BASE) >> (IOMAP_VPAGESHIFT) ;
#else
    i = ((__scunsigned_t)vaddr - VADDR_BASE) >> (IOMAP_VPAGESHIFT + 1) ;
#endif

    if (tlb_entries[i] == 0)  
        return(-1);

    invaltlb(i+FIRST_TLB_ENTRY);
    tlb_entries[i] = 0;

    return (0);
}

#if !TFP
/*
 * just call in an ascending loop till returns != 0
 * added for ide - don't want ide to know about internals over here
 */
int
inval_tlbentry(uint slot)
{
    if (slot >= MAX_TLBSZ)
        return(-1);

    invaltlb(slot+FIRST_TLB_ENTRY);
    tlb_entries[slot] = 0;

    return (0);
}
#endif	/* !TFP */

void
tlbrall(void)
{
    int i;

    /* zero all tlb entries */
    for (i=0; i < MAX_TLBSZ; i++) {
	invaltlb(i+FIRST_TLB_ENTRY);
	tlb_entries[i] = 0;
    }
}

void
cpu_scandevs(void)
{
}

/* check if address is inside a "protected" area */
/*ARGSUSED*/
int
is_protected_adrs(unsigned long low, unsigned long high)
{
    return 0;	/* none */
}

/*
 * Get the secondary cache size of the specified CPU
 */
uint getcachesz(int virtid) {
	int slot, slice;
	int physid;
	uint cache_size;

	physid = MPCONF[virtid].phys_id;

	slot  = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	slice = (physid & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	/* If value falls out of range, use maximum value.  This is safest
	 * course of action, since typical use of this value is for cache
	 * flushing routines and it's better to flush too much rather than
	 * too little.
	 */
	cache_size = 1 << EVCFG_CPUSTRUCT(slot, slice).cpu_cachesz;
	if ((cache_size < MINCACHE) || (cache_size > MAXCACHE))
		cache_size = MAXCACHE;
	return(cache_size);
}

#if IP19
/*
 * Checksum the first 48 bytes of the EAROM.
 */
ushort calc_cksum()
{
	short sum;
	int i;
 
	sum = load_double_lo((long long *)EV_EAROM_BASE) | EAROM_BE_MASK;
	 
	for (i = 1; i < EV_CHKSUM_LEN; i++) {
		sum += load_double_lo((long long *)(EV_EAROM_BASE + i*8));
	}

	return sum;
}
#endif

#if IP21
static unchar cc_revs[EV_BOARD_MAX][EV_MAX_CPUS_BOARD];
#endif

void
set_cc_rev(void)
{
#if IP21
	/*
	 * The cc rev number is in the spnum register so other processors
	 * can't look at it. we have to save it here so the master can look
	 * at it later.
	 */
	uint rev, slot, proc;

	rev = load_double_lo((long long *)EV_SPNUM);
	slot = (rev & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	proc = (rev & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
	rev = (rev >> EV_CCREVNUM_SHFT) & 0xff;	/* yuck! */
	cc_revs[slot][proc] = rev + 1;	/* double yuck! */
#endif
}

unchar
get_cc_rev(int slot, int slice)
{
#if IP19
	unchar old_val;
	unchar version;

	old_val = (unchar)EV_GETCONFIG_REG(slot, slice, 
					   EV_CCREV_REG);
	EV_SETCONFIG_REG(slot, slice, EV_CCREV_REG, 0x1f);
	version = (unchar)EV_GETCONFIG_REG(slot, slice, 
					   EV_CCREV_REG);
	EV_SETCONFIG_REG(slot, slice, EV_CCREV_REG, old_val);

	if (version == EV_CCREV_1)
		return (unchar)1;
	else if (version == EV_CCREV_2)
		return (unchar)2;
	else
		return (unchar)0xff;

#elif IP21
	return cc_revs[slot][slice];
#elif IP25
	unchar	version;

	version = (unchar)(EV_GETCONFIG_REG(slot,slice,EV_IWTRIG)>>28);
	version = (version & 0xf) + 1;
	return(version);
#else
#   error "Invalid build type"
#endif

}
