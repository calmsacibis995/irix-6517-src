#ident	"$Revision: 1.51 $"

#ifdef IP28	/* whole file */

#include <sys/types.h>
#include <sys/IP22.h>
#include <sys/debug.h>
#include <sys/z8530.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>
#include <sys/immu.h>
#include <sys/eisa.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/types.h>
#include <sys/param.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/cfgtree.h>
#include <arcs/time.h>
#include <sys/RACER/gda.h>


#define	INB(x)		(*(volatile u_char *)(x))
#define	OUTB(x,y)	(*(volatile u_char *)(x) = (y))

#if DEBUG
extern int Debug;
#define	DPRINTF	if (Debug) printf
#else
#define	DPRINTF
#endif

#define	NUMBANKS    3	/* 3 main mem banks on IP28.  4th is used for ECC */

/* Exports */
void		cpu_install(COMPONENT *);
unsigned int	cpu_get_memsize(void);
void		cpu_acpanic(char *str);
int		cpuid(void);
int		ip22_board_rev(void);
void		ecc_error_decode(u_int);
void		ip22_setup_gio64_arb(void);
void ip26_clear_ecc(void);
static __uint32_t get_mem_conf(int bank);

/* Private */
static void eisa_refreshoff(void);
static void init_ip28_nmi(void);

#ifdef US_DELAY_DEBUG
static void check_us_delay(void);
#endif

extern unsigned _sidcache_size;

unsigned mc_rev;

/*
 * IP22 board registers saved during exceptions
 *
 * These are referenced by the assembly language fault handler
 *
 */
int	_cpu_parerr_save, _gio_parerr_save;
int	_hpc3_intstat_save, _hpc3_bus_err_stat_save, _hpc3_ext_io_save;
int	_power_intstat_save;
char	_liointr0_save, _liointr1_save, _liointr2_save;
__uint32_t	_cpu_paraddr_save, _gio_paraddr_save;

/* software copy of hardware write-only registers */
uint _hpc3_write1;
uint _hpc3_write2;
uint _gio64_arb;		/* not ro, but not always correct */

/* default values for different revs of base board */
uint _gio64_arb_004 =	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST;

struct reg_desc _cpu_parerr_desc[] = {
	/* mask		shift	name		format  values */
	{ 0x01,		0,	"B0",		NULL,	NULL },
	{ 0x02,		-1,	"B1",		NULL,	NULL },
	{ 0x04,		-2,	"B2",		NULL,	NULL },
	{ 0x08,		-3,	"B3",		NULL,	NULL },
	{ 0x10,		-4,	"B4",		NULL,	NULL },
	{ 0x20,		-5,	"B5",		NULL,	NULL },
	{ 0x40,		-6,	"B6",		NULL,	NULL },
	{ 0x80,		-7,	"B7",		NULL,	NULL },
	{ 0x300,	-8,	"MEM_PAR",	NULL,	NULL },
	{ 0x400,	-10,	"ADDR",		NULL,	NULL },
	{ 0x800,	-11,	"SYSAD_PAR",	NULL,	NULL },
	{ 0x1000,	-12,	"SYSCMD_PAR",	NULL,	NULL },
	{ 0x2000,	-13,	"BAD_DATA",	NULL,	NULL },
	{ 0,		0,	NULL,		NULL,	NULL },
};

struct reg_desc _gio_parerr_desc[] = {
	/* mask		shift	name		format  values */
	{ 0x01,		0,	"B0",		NULL,	NULL },
	{ 0x02,		-1,	"B1",		NULL,	NULL },
	{ 0x04,		-2,	"B2",		NULL,	NULL },
	{ 0x08,		-3,	"B3",		NULL,	NULL },
	{ 0x10,		-4,	"B4",		NULL,	NULL },
	{ 0x20,		-5,	"B5",		NULL,	NULL },
	{ 0x40,		-6,	"B6",		NULL,	NULL },
	{ 0x80,		-7,	"B7",		NULL,	NULL },
	{ 0x100,	-8,	"RD_PAR",	NULL,	NULL },
	{ 0x200,	-9,	"WR_PAR",	NULL,	NULL },
	{ 0x400,	-10,	"TIME",		NULL,	NULL },
	{ 0x800,	-11,	"PROM",		NULL,	NULL },
	{ 0x1000,	-12,	"ADDR",		NULL,	NULL },
	{ 0x2000,	-13,	"BC",		NULL,	NULL },
	{ 0x4000,	-14,	"PIO_RD",	NULL,	NULL },
	{ 0x8000,	-15,	"PIO_WR",	NULL,	NULL },
	{ 0,		0,	NULL,		NULL,	NULL },
};

/* used by INT2 diagnostic */
struct reg_desc _liointr0_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x1,	0,	"FIFO/GIO0",	NULL,	NULL },
	{ 0x2,	-1,	"SCSI0",	NULL,	NULL },
	{ 0x4,	-2,	"SCSI1",	NULL,	NULL },
	{ 0x8,	-3,	"ENET",		NULL,	NULL },
	{ 0x10,	-4,	"GDMA",		NULL,	NULL },
	{ 0x20,	-5,	"CENTR",	NULL,	NULL },
	{ 0x40, -6,	"GE/GIO1",	NULL,	NULL },
	{ 0x80,	-7,	"VME0",		NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

/* used by INT2 diagnostic */
struct reg_desc _liointr1_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x2,	-1,	"POWER",	NULL,	NULL },
	{ 0x8,	-3,	"LIO3",		NULL,	NULL },
	{ 0x10,	-4,	"HPC3",		NULL,	NULL },
	{ 0x20,	-5,	"ACFAIL",	NULL,	NULL },
	{ 0x40, -6,	"VIDEO",	NULL,	NULL },
	{ 0x80,	-7,	"VR/GIO2",	NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

/* used by INT2 diagnostic */
struct reg_desc _liointr2_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x8,	-3,	"EISA",		NULL,	NULL },
	{ 0x10,	-4,	"KBD/MOUSE",	NULL,	NULL },
	{ 0x20,	-5,	"DUART",	NULL,	NULL },
	{ 0x40, -6,	"SLOT0",	NULL,	NULL },
	{ 0x80,	-7,	"SLOT1",	NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

/* used by HPC3 diagnostic */
struct reg_desc _hpc3_intstat_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x1,	 0,	"PBUS0",	NULL,	NULL },
	{ 0x2,	-1,	"PBUS1",	NULL,	NULL },
	{ 0x4,	-2,	"PBUS2",	NULL,	NULL },
	{ 0x8,	-3,	"PBUS3",	NULL,	NULL },
	{ 0x10,	-4,	"PBUS4",	NULL,	NULL },
	{ 0x20,	-5,	"PBUS5",	NULL,	NULL },
	{ 0x40, -6,	"PBUS6",	NULL,	NULL },
	{ 0x80,	-7,	"PBUS6",	NULL,	NULL },
	{ 0x100,-8,	"SCSI0",	NULL,	NULL },
	{ 0x200,-9,	"SCSI1",	NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

#define BELLFQ		1810
#define FQVAL(v)	(1193000/(v))
#define FQVAL_MSB(v)	(((v)&0xff00)>>8)
#define FQVAL_LSB(v)	((v)&0xff)

/* Decide between IP28 and IP26 memory timing from baseboard revision and
 * memory configuation.
 */
uint ip28_memacc_flag;	/* default (0) -> IP28 timing */
static void
ip28_memtiming(void)
{
#define is64MB(bank)	((bank & (MEMCFG_DRAM_MASK|MEMCFG_BNK|MEMCFG_VLD)) == \
			 (MEMCFG_128MRAM|MEMCFG_BNK|MEMCFG_VLD))
	int bank0, bank1, bank2, nvalid;

	bank0 = get_mem_conf(0);
	bank1 = get_mem_conf(1);
	bank2 = get_mem_conf(2);

	nvalid = ((bank0 & MEMCFG_VLD) != 0) +
		 ((bank1 & MEMCFG_VLD) != 0) +
		 ((bank2 & MEMCFG_VLD) != 0);

	/* Have at least 2 banks with at least 1 64MB SIMM bank.
	 */
	if (is_ip26bb() ||
	    ((nvalid > 1) && (is64MB(bank0)||is64MB(bank1)||is64MB(bank2)))) {
		ip28_memacc_flag = 1;
	}
}

/*
 * cpu_reset - perform any board specific start up initialization
 *
 */
void
cpu_reset(void)
{
	extern int cachewrback, _prom;
	void scsiinit(void);

	cachewrback = 1;

	mc_rev = *(volatile uint *)PHYS_TO_COMPATK1(SYSID)&SYSID_CHIP_REV_MASK;

	if (!_hpc3_write1 || !_hpc3_write2) {
		_hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET |
				LED_GREEN;	/* also green for guinness */
		_hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH |
			ETHER_UTP_STP | ETHER_PORT_SEL | UART0_ARC_MODE |
			UART1_ARC_MODE;
		*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE1) = _hpc3_write1;
		*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE2) = _hpc3_write2;
	}

	/* init _gio64_arb variable, then set the register */
	ip22_setup_gio64_arb();
	*(volatile unsigned int *)PHYS_TO_COMPATK1(GIO64_ARB) = _gio64_arb;
	wbflush();

	/* Initialize burst/delay register for graphics and i/o DMA. 33MHz */
	*(volatile uint *)PHYS_TO_COMPATK1(LB_TIME) = LB_TIME_DEFAULT;
	*(volatile uint *)PHYS_TO_COMPATK1(CPU_TIME) = CPU_TIME_DEFAULT;

	/* mask out all interrupts except for power switch */
	*K1_LIO_0_MASK_ADDR = 0x0;
	*K1_LIO_1_MASK_ADDR = LIO_POWER;
	*K1_LIO_2_MASK_ADDR = 0x0;
	*K1_LIO_3_MASK_ADDR = 0x0;

	wbflush();

#if 0
	/* GIO parity cannot be enabled on pacecar.  The MC also uses this
	 * bit for turning on other parity, which does not work with memory
	 * and SysAD parity off.  Left code as a reminder.
	 */
	*(volatile uint *)PHYS_TO_COMPATK1(CPUCTRL0) |=
			CPUCTRL0_R4K_CHK_PAR_N | CPUCTRL0_GPR;
#endif
	*(volatile unsigned int *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	wbflush();

	ip28_memtiming();

	init_ip28_nmi();		/* enable ECC error handlers */
	ip26_clear_ecc();		/* clear any lingering ECC errors */
	ip28_ecc_correct();		/* enable ECC error correction */
	ip28_enable_eccerr();		/* enable ECC errors on IP28 bb */

	kbdbell_init();

	if (_prom) eisa_refreshoff();

	scsiinit();

#ifdef US_DELAY_DEBUG
	check_us_delay();
#endif
}

static void
init_ip28_nmi(void)
{
	extern void ip28_nmi();

	/*
	 * Initialize NMI vector.  Prom jumps to the location
	 * specified here when it receives an NMI.
	 */
	GDA->g_nmivec = (__psunsigned_t)ip28_nmi;
}

/* Initialize keyboard bell */
static int bellok;
void
kbdbell_init(void)
{
	int cpuctrl0, ctrld;
	unsigned char nmi;
	extern int _prom;
	int val = 0;

	/* EISA power-up hang hack -- use MC watchdog to reset us if we hang */
	if (_prom) {
		cpuctrl0 = *(volatile int *)PHYS_TO_COMPATK1(CPUCTRL0);
		ctrld = *(volatile int *)PHYS_TO_COMPATK1(CTRLD);

		/* clear dog, enable dog, bump refresh */
		*(volatile int *)PHYS_TO_COMPATK1(DOGC) = 0;
		*(volatile int *)PHYS_TO_COMPATK1(CPUCTRL0) =
			cpuctrl0 | CPUCTRL0_DOG;
		*(volatile int *)PHYS_TO_COMPATK1(CTRLD) = ctrld >> 3;
		flushbus();
	}

	/* IP28 always has EIU/eisa so don't worry about badaddr().  The
	 * EISA hang does not timeout, just hangs the GIO bus.
	 */
	nmi = INB(EISAIO_TO_COMPATK1(NMIStatus));

	if (_prom) {
		/* restore refresh, disable watchdog */
		*(volatile int *)PHYS_TO_COMPATK1(CTRLD) = ctrld;
		*(volatile int *)PHYS_TO_COMPATK1(CPUCTRL0) = cpuctrl0;
		flushbus();
	}

	/* do ASAP since will quiet potentially run-away bell */
	nmi |= EISA_C2_ENB;
	nmi &= ~EISA_SPK_ENB;
	OUTB(EISAIO_TO_COMPATK1(NMIStatus),nmi);

	val = FQVAL(BELLFQ);

	OUTB(EISAIO_TO_COMPATK1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C2));
	OUTB(EISAIO_TO_COMPATK1(IntTimer1SpeakerTone),FQVAL_LSB(val));
	flushbus();
	delay(1);
	OUTB(EISAIO_TO_COMPATK1(IntTimer1SpeakerTone),FQVAL_MSB(val));
	flushbus();

	bellok = 1;
}

static void
eisa_refreshoff(void)
{
	if (badaddr((void *)EISAIO_TO_COMPATK1(IntTimer1CommandMode),1))
		return;
	
	/* Turn EISA refresh off early as hardware has trouble with it on
	 * and it really does not need to be on anyway.
	 */
	OUTB(EISAIO_TO_COMPATK1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C1));
	OUTB(EISAIO_TO_COMPATK1(IntTimer1RefreshRequest), 0);
	flushbus();
	flushbus();			/* pseudo-us_delay(1) */
	OUTB(EISAIO_TO_COMPATK1(IntTimer1RefreshRequest), 0); /* refresh off! */
	flushbus();
}

/* LED_GREEN == LED_RED_OFF == 0x10 */
/* LED_AMBER == LED_GREEN_OFF == 0x20 */

static const char	led_masks[4] = {
	0,
	LED_GREEN,
	LED_AMBER,
	LED_AMBER
};

/*
 * set the main cpu led to the given color
 *	0 = black
 *	1 = green
 *	2 = amber
 *	3 = red		(red on Indy)
 */
void
ip22_set_cpuled(int color)
{
	_hpc3_write1 &= ~(LED_GREEN|LED_AMBER);
	_hpc3_write1 |= led_masks[color&0x03];
	*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE1) = _hpc3_write1;
	return;
}

/*
 *	get memory configuration word for a bank
 *              - bank better be between 0 and 3 inclusive
 */
static __uint32_t
get_mem_conf(int bank)
{
    __uint32_t memconfig;

    memconfig = (bank <= 1) ? 
	*(volatile __uint32_t *)PHYS_TO_COMPATK1(MEMCFG0) :
	*(volatile __uint32_t *)PHYS_TO_COMPATK1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    memconfig &= 0xffff;

    return memconfig;
}

/*
 * banksz - determine the size of a memory bank
 */

#define mcshift(oldshift)	((oldshift)+2)

static __uint32_t
banksz(int bank)
{
    __uint32_t memconfig = get_mem_conf(bank);

    /* weed out bad module sizes */
    if (!(memconfig & MEMCFG_VLD))
        return 0;
    
    return ((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << mcshift(14);
}       /* banksz */

/* Determine first address of a memory bank.
 */
static uint
bank_addrlo(int bank)
{
	return (get_mem_conf(bank) & MEMCFG_ADDR_MASK) << mcshift(22);
}

/*
 * ip22_addr_to_bank - find bank in which the given addr belongs to
 */
int
ip22_addr_to_bank(__uint32_t addr)	/* phys addr is only 32 bits */
{
	__uint32_t base, size;
	int i;

	for (i = 0; i < NUMBANKS; i++) {
		if (!(size = banksz(i)))
			continue;
		base = bank_addrlo(i);
		if (addr >= base && addr < (base + size))
			return i;
	}

	return -1;
}

/*
    IP22/IP24 specific hardware inventory routine
*/

#define K1_SYS_ID	(volatile uint *)PHYS_TO_COMPATK1(HPC3_SYS_ID)

/* 
 * return board revision level (0-7)
 */
int
ip22_board_rev(void)
{
	return (*K1_SYS_ID & BOARD_REV_MASK) >> BOARD_REV_SHIFT;
}

int
is_fullhouse(void)
{
	return 1;			/* No Indy T5 */
}

/*
 * return revision (> 0) if IOC1 chip, 0 if not IOC.
 */
int
is_ioc1(void)
{
	return 0;			/* No Indy T5 */
}


#ifdef DEBUG
/*
 * cpu_showconfig - print hinv-style info about cpu board dependent stuff
 */
void
cpu_showconfig(void)
{
    printf("                CPU board: Revision %d\n", ip22_board_rev());
    return;
}
#endif	/* DEBUG */

/* for cache keys */
static int
log2(int x)
{
	int n,v;
	for(n=0,v=1;v<x;v<<=1,n++) ;
	return(n);
}

#define SYSBOARDIDLEN	9
#define SYSBOARDID	"SGI-IP28"

static COMPONENT IP28tmpl = {
	SystemClass,		/* Class */
	ARC,			/* Type */
	0,			/* Flags */
	SGI_ARCS_VERS,		/* Version */
	SGI_ARCS_REV,		/* Revision */
	0,			/* Key */
	0,			/* Affinity */
	0,			/* ConfigurationDataSize */
	SYSBOARDIDLEN,		/* IdentifierLength */
	SYSBOARDID		/* Identifier */
};
	
cfgnode_t *
cpu_makecfgroot(void)
{
	COMPONENT *root;

	root = AddChild(NULL,&IP28tmpl,(void *)NULL);
	if (root == (COMPONENT *)NULL) cpu_acpanic("root");
	return((cfgnode_t *)root);
}

static COMPONENT cputmpl = {
	ProcessorClass,			/* Class */
	CPU,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	12,				/* IdentifierLength */
	"MIPS-R10000"			/* Identifier */
};
static COMPONENT fputmpl = {
	ProcessorClass,			/* Class */
	FPU,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	15,				/* IdentifierLength */
	"MIPS-R10000FPC"		/* Identifier */
};
static COMPONENT cachetmpl = {
	CacheClass,			/* Class */
	PrimaryICache,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};
static COMPONENT memtmpl = {
	MemoryClass,			/* Class */
	Memory,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* Identifier Length */
	NULL				/* Identifier */
};

void
cpu_install(COMPONENT *root)
{
	extern unsigned _dcache_size;
	extern unsigned _icache_size;
	extern unsigned _dcache_linesize;
	extern unsigned _icache_linesize;
	extern unsigned _scache_linesize;
	COMPONENT *c, *tmp;
	union key_u key;

	c = AddChild(root,&cputmpl,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("cpu");
	c->Key = cpuid();

	tmp = AddChild(c,&fputmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("fpu");
	tmp->Key = cpuid();

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("icache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(_icache_linesize);
	key.cache.c_size = log2(_icache_size >> 12);
	tmp->Key = key.FullKey;

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("dcache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(_dcache_linesize);
	key.cache.c_size = log2(_dcache_size >> 12);
	tmp->Type = PrimaryDCache;
	tmp->Key = key.FullKey;

	if (_sidcache_size != 0) {
		tmp = AddChild(c,&cachetmpl,(void *)NULL);
		if (tmp == (COMPONENT *)NULL) cpu_acpanic("s_cache");
		key.cache.c_bsize = 1;
		key.cache.c_lsize = log2(_scache_linesize);
		key.cache.c_size = log2(_sidcache_size >> 12);
		tmp->Type = SecondaryCache;
		tmp->Key = key.FullKey;
	}

	tmp = AddChild(c,&memtmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("main memory");
	tmp->Key = cpu_get_memsize() >> ARCS_BPPSHIFT;	/* number of 4k pages */
}

/*
 * cpu_hardreset - hit the system reset bit on the cpu
 */
void
cpu_hardreset(void)
{
    *(volatile unsigned int *)PHYS_TO_COMPATK1(CPUCTRL0) |= CPUCTRL0_SIN;
    wbflush();
    while (1)
	;
}

int
is_ip26bb(void)
{
	return (ip22_board_rev() <= (IP26_ECCSYSID>>BOARD_REV_SHIFT));
}

int
is_ip26pbb(void)
{
	return (ip22_board_rev() == (IP26P_ECCSYSID>>BOARD_REV_SHIFT));
}

void ip28_write_pal(int);		/* IP28asm.s */

/*
 * ip26_clear_ecc
 * Clear old single-bit errors and uncached writes
 */
void
ip26_clear_ecc(void)
{
	ip28_write_pal(ECC_CTRL_CLR_INT);
}

/* Enable ecc error reporting via NMI on IP28 baseboards
 */
void
ip28_enable_eccerr(void)
{
	if (is_ip28bb())
		ip28_write_pal(ECC_CTRL_CHK_ON);
}

/* Disable ecc error reporting via NMI on IP28 baseboards
 */
void
ip28_disable_eccerr(void)
{
	if (is_ip28bb())
		ip28_write_pal(ECC_CTRL_CHK_OFF);
}

/* used in ide */
char *simm_map_fh[3][4] = {
	{" <SIMM S12>", " <SIMM S11>", " <SIMM S10>", " <SIMM S9>"},
	{" <SIMM S8>", " <SIMM S7>", " <SIMM S6>", " <SIMM S5>"},
	{" <SIMM S4>", " <SIMM S3>", " <SIMM S2>", " <SIMM S1>"},
};

#define	GIO_ERRMASK	0xff00
#define	CPU_ERRMASK	0x3f00

void
cpu_show_fault(unsigned long saved_cause)
{
	/* On IP26 board, errors are not enabled, so take a chance and
	 * try to clear it on any error.
	 */
	if (is_ip26bb()) ip26_clear_ecc();

	if (saved_cause & CAUSE_BERRINTR) {
	    if (_cpu_parerr_save & CPU_ERR_STAT_ADDR) {
		printf("CPU Bus Error Interrupt\n");
	    }
	    else if (_cpu_parerr_save & CPU_ERRMASK) {
		/* This should not happen, even on IP26 bb */
		printf("CPU Parity Error Interrupt\n");
	    }
	    else if (_gio_parerr_save & (GIO_ERR_STAT_RD|GIO_ERR_STAT_WR|
			GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR)) {
		/* This should not happen, even on IP26 bb */
		printf("GIO Parity Error Interrupt\n");
	    }
	    else if (_gio_parerr_save & GIO_ERR_STAT_TIME)
		printf("GIO Timeout Interrupt\n");
	    else if (_gio_parerr_save & GIO_ERR_STAT_PROM)
		printf("PROM Write Interrupt\n");
	    else if (_gio_parerr_save & GIO_ERRMASK)
		printf("GIO Bus Error Interrupt\n");
	    else if (_hpc3_ext_io_save & EXTIO_EISA_BUSERR)
		printf("EISA Bus Error Interrupt\n");
	    else if (_hpc3_ext_io_save & EXTIO_HPC3_BUSERR)
		printf("HPC3 Bus Error Interrupt\n");
	    else
		printf("Bus Error?\n");
	}
		
	if (_liointr0_save) {
		printf("Local I/O interrupt register 0: 0x%R\n",
			_liointr0_save, _liointr0_desc);
	}
	if (_liointr1_save) {
		printf("Local I/O interrupt register 1: 0x%R\n",
			_liointr1_save, _liointr1_desc);
	}
	if (_liointr2_save) {
		printf("Local I/O interrupt register 2: 0x%R\n",
			_liointr2_save, _liointr2_desc);
	}

	if (_hpc3_intstat_save) {
		printf("HPC3 interrupt status register: 0x%R\n",
			_hpc3_intstat_save, _hpc3_intstat_desc);
	}
	if (_hpc3_ext_io_save & EXTIO_HPC3_BUSERR) {
		printf("HPC3 bus error status register: 0x%x\n",
			_hpc3_bus_err_stat_save);
	}
	if (_cpu_parerr_save & CPU_ERRMASK) {
		/* No need to try and diagnose SIMMs here on IP28 */
		printf("CPU Error Stat: %R\n",
			_cpu_parerr_save, _cpu_parerr_desc);
		printf("CPU Error Stat Addr: 0x%x\n",
			(unsigned long)_cpu_paraddr_save);
		*(volatile int *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	}

	if (_gio_parerr_save & GIO_ERRMASK) {
		/* No need to try and diagnose SIMMs here on IP28 */
		printf("GIO Error Stat: %R\n",
			_gio_parerr_save, _gio_parerr_desc );
		printf("GIO Error Stat Addr: 0x%x\n",
			(unsigned long)_gio_paraddr_save );
		*(volatile int *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	}

	wbflush();

	/* Re-enable power interrupt */
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	set_SR(SR_PROMBASE|SR_IE|SR_IBIT4|SR_BERRBIT);
	wbflush();
}

/* Rescan low/high side of a memory bank to see if we can determine if
 * the error is a transient, or a hard error.  We may get unlucky and
 * hit a second multi-bit error, but there isn't much we can do.
 */
static void
ecc_rescan(volatile __uint64_t *ptr, int size)
{
	printf("Rescanning @ 0x%x for %d double words.\n", ptr,size);
	while (size--) {
		*ptr;
		ptr += 2;	/* only scan 1 leaf of memory */
	}
}

volatile int ip28_nested_NMI;
void
ip28_ecc_error(void)
{
	static char *simmpair[4][2] = {
		{ " unknown", " unknown" },
		{ " S9/S10", " S11/S12" },
		{ " S5/S6", " S7/S8" },
		{ " S1/S2", " S3/S4" }
	};
	int bank, ecc_stat;

	/* Read ECC status bits */
	ecc_stat = *K1_ECC_STATUS;
	ecc_stat &= ECC_STATUS_MASK;
	bank = (ecc_stat & ECC_STATUS_BANK) >> ECC_BANK_SHIFT;

	if (ecc_stat & ECC_STATUS_UC_WR) {
		printf("*** Uncached write in fast mode! [0x%x]\n",ecc_stat);
		ip26_clear_ecc();		/* clear memory error */
		return;
	}

	/* Multi-bit ECC error */
	printf("*** Multi-bit ECC error (%s) in SIMM pair(s)%s%s [0x%x]\n",
			ecc_stat & ECC_STATUS_GIO ? "GIO" : "CPU",
			ecc_stat & ECC_STATUS_LOW ? simmpair[bank][0] : "",
			ecc_stat & ECC_STATUS_HIGH ? simmpair[bank][1] : "",
			ecc_stat);

	if (ip28_nested_NMI++ == 0) {
		__uint64_t *ptr;
		int size;

		ip26_clear_ecc();		/* clear memory error */

		bank--;			/* bank calls are 0 based */
		
		ptr = (__uint64_t *)PHYS_TO_K1(bank_addrlo(bank));
		size = banksz(bank) >> (1+3);	/* 1/2 of bank, doubles */

		if (size != 0) {
			if (ecc_stat & ECC_STATUS_LOW) {
				ecc_rescan(ptr,size);
			}
			if (ecc_stat & ECC_STATUS_HIGH) {
				ecc_rescan(ptr+1,size);
			}
		}
		else {
			printf("*** Bank syndrome error, rescan all RAM.\n"); 

			for (bank = 0; bank < 3; bank++) {
				size = banksz(bank) >> (1+3);
				ptr = (__uint64_t *)
					PHYS_TO_K1(bank_addrlo(bank));
				
				if (size != 0) {
					ecc_rescan(ptr,size);
					ecc_rescan(ptr+1,size);
				}
			}
		}
	}

	printf("*** Memory error appears to be %s.\n",
		(ip28_nested_NMI == 1) ? "transient" : "hard");

	/* Finishes with normal exception dump. */
}

/* function to shut off flat panel, set in ng1_init.c */
void (*fp_poweroff)(void);

void
cpu_soft_powerdown(void)
{
	volatile uint *p = (uint *)PHYS_TO_COMPATK1(HPC3_PANEL);
	ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	volatile int dummy;

	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
		(*fp_poweroff)();
	
	ip22_power_switch_off();		/* disable auto-poweron */

	/* wait for power switch to be released */
	do {
		*(volatile int *)PHYS_TO_COMPATK1(HPC3_PANEL) = POWER_ON;
		flushbus();
		us_delay(5000);
	} while (*K1_LIO_1_ISR_ADDR & LIO_POWER);

	while (1) {
		*p = ~POWER_SUP_INHIBIT;	/* sets POWER_INT */
		flushbus();
		DELAY(10);

		/* If power not out, maybe "wakeupat" time hit the window.
		 */
		if (clock->command & WTR_TDF) {
			/* Disable and clear alarm. */
			clock->command |= WTR_DEAC_TDM;
			flushbus();
			dummy = clock->hour;
		}
	}
	/*NOTREACHED*/
	/* power is now off */
}

int
cpuid(void)
{
	return 0;		/* always cpuid = 0 */
}

unsigned int memsize;

unsigned int
cpu_get_memsize(void)
{
    int bank;

    if (!memsize) {
	for (bank = 0; bank < NUMBANKS; ++bank) {
	    memsize += banksz (bank);	/* sum up bank sizes */
	}
    }
    return memsize;
}

#define cpu_get_membase() ((unsigned int)PHYS_RAMBASE)

char *
cpu_get_disp_str(void)
{
	return("video()");
}

char *
cpu_get_kbd_str(void)
{
	return("keyboard()");
}
char *
cpu_get_serial(void)
{
	return("serial(0)");
}

void
cpu_errputc(char c)
{
	z8530cons_errputc(c);
}

void
cpu_errputs(char *s)
{
	while (*s)
		z8530cons_errputc (*s++);
}

void
cpu_mem_init(void)
{
#define NPHYS0_PAGES	2
	int lowmem = cpu_get_memsize();		/* all mem mapped high */

	/* add all of main memory with the exception of two pages that
	 * will be also mapped to physical 0
	 */
	md_add(cpu_get_membase()+arcs_ptob(NPHYS0_PAGES),
	       arcs_btop(lowmem) - NPHYS0_PAGES, FreeMemory);

	md_add(0, NPHYS0_PAGES, FreeMemory);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP28 cannot save configuration to NVRAM */
LONG
cpu_savecfg(void)
{
	return(ENOSPC);
}

char *
cpu_get_mouse(void)
{
	return("pointer()");
}

#ifdef DEBUG
int no_ecc_fault;
#endif	/* DEBUG */

/*  Needs address to find double word side of cache.  Both datums
 * need to be read from double aligned addresses.
 *
 *  For modules with x18 IBM SSRAMS:
 *
 *      cache	SRAM	addr	bytes
 *	-----	----	----	-----
 *	00-15	U6	0x08	6/7
 *	16-31	U519	0x08	4/5
 *	32-47	U4	0x08	2/3
 *	48-63	U529	0x08	0/1
 *	64-79	U5	0x00	6/7
 *	80-95	U522	0x00	4/5
 *	96-111	U2	0x00	2/3
 *	112-127	U521	0x00	0/1
 */
__uint64_t
ip28_ssram_swizzle(__uint64_t data)
{
	return ((data & 0xffffffff00000000) >> 32) |
	       ((data & 0x00000000ffffffff) << 32);
}
char *
ip28_ssram(void *addr,__uint64_t expected, __uint64_t actual)
{
	static char *ssrams_x18[2][4] = {
		{ "U6 ", "U519 ", "U4 ", "U529 " },	/* low addr */
		{ "U5 ", "U522 ", "U2 ", "U521 " }	/* high addr */
	};
	int i, side = ((__psunsigned_t)addr & 8) == 0;	/* 8 -> 0, 0 -> 1 */
	static char buf[(4*4)+1];
	char **ssrams = ssrams_x18[side];

	*buf = '\0';

	for (i=0; i < 4; i++) {
		if ((expected ^ actual) & 0xffff) {
			strcat(buf,ssrams[i]);
		}
		expected >>= 16;
		actual >>= 16;
	}

	return buf;
}

/* On R10K, we cannot pinpoint SSRAM on multi-bit failure.  Higher level
 * code does a simple state dump.
 */
/* ARGSUSED */
void
ecc_error_decode(u_int ecc_error_reg)
{
}

void
cpu_scandevs(void)
{
	unsigned char intr = *(unsigned char *)PHYS_TO_COMPATK1(LIO_1_ISR_ADDR);

	if (intr & LIO_MASK_POWER) {
		cpu_soft_powerdown();
	}
}

void
bell(void)
{
	unsigned char stat;

	if (!bellok)
		return;

	/* Turn on speaker */
	stat = INB(EISAIO_TO_COMPATK1(NMIStatus));
	OUTB(EISAIO_TO_COMPATK1(NMIStatus),stat|EISA_SPK_ENB);

	DELAY(50000);			/* let ring */

	/* Turn off speaker */
	OUTB(EISAIO_TO_COMPATK1(NMIStatus),stat);
}

/* check if address is inside a "protected" area */
/*ARGSUSED*/
int
is_protected_adrs(unsigned long low, unsigned long high)
{
	return 0;		/* not protected */
}

/* determine speed of GIO bus */
int
ip22_gio_is_33MHZ(void)
{
	return *(volatile int *)PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR) &
		EXTIO_GIO_33MHZ;
}

/*  Set-up software copy of GIO64_ARB.  For prom it was already
 * set by IP22prom/csu.s
 */
void
ip22_setup_gio64_arb(void)
{
	_gio64_arb = _gio64_arb_004;
}

#ifdef US_DELAY_DEBUG
/* rough check of us_delay.  Is not too accurate as done in C.  Caller needs
 * to convert from C0_COUNT ticks @ 1/2 freqency of CPU to determine time.
 */
__uint32_t us_before, us_after;
static void
check_us_delay(void)
{
	int i;

	us_delay(1);		/* prime cache */

	for (i = 0; i < 10; i++) {
		us_delay(i);
		printf("us_delay(%d) took a little less than 0x%x - 0x%x = "
			"%d ticks.\n",
			i,(__psunsigned_t)us_before,(__psunsigned_t)us_after,
			us_after-us_before);
	}

	return;
}
#endif

#endif /*  IP28 (whole file) */
