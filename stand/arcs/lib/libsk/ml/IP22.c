#ident	"$Revision: 1.224 $"

#ifdef IP22	/* whole file */

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

#define	INB(x)		(*(volatile u_char *)(x))
#define	OUTB(x,y)	(*(volatile u_char *)(x) = (y))

#if DEBUG
extern int Debug;
#define	DPRINTF	if (Debug) printf
#else
#define	DPRINTF
#endif

#ifdef US_DELAY_DEBUG
static void check_us_delay(void);
#endif

/* Exports */
void		cpu_install(COMPONENT *);
unsigned int	cpu_get_memsize(void);
void		cpu_acpanic(char *str);
int		cpuid(void);
int		ip22_board_rev(void);
void		ecc_error_decode(u_int);
void		ip22_setup_gio64_arb(void);

/* Private */
static unsigned	cpu_get_membase(void);
static void eisa_refreshoff(void);

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
uint _gio64_arb_003 =	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |
			GIO64_ARB_EXP1_MST	| GIO64_ARB_EXP1_RT;
uint _gio64_arb_004 =	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST;
uint _gio64_arb_gns =	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_1_GIO;

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
	{ 0x1,	0,	"ISDN_ISAC",	NULL,	NULL },
	{ 0x2,	-1,	"POWER",	NULL,	NULL },
	{ 0x4,	-2,	"ISDN_HSCX",	NULL,	NULL },
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

/* POWER/PANEL register*/
struct reg_desc _power_intstat_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x80,	-7,	"VOL_UP_ACT",	NULL,	NULL },
	{ 0x40, -6,	"VOL_UP_INT",	NULL,	NULL },
	{ 0x20,	-5,	"VOL_DOWN_ACT",	NULL,	NULL },
	{ 0x10,	-4,	"VOL_DOWN_INT",	NULL,	NULL },
	{ 0x8,	-3,	"?",		NULL,	NULL },
	{ 0x4,	-2,	"?",		NULL,	NULL },
	{ 0x2,	-1,	"PWR_INT",	NULL,	NULL },
	{ 0x1,	 0,	"PWR_INHIBIT",	NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

#define BELLFQ		1810
#define FQVAL(v)	(1193000/(v))
#define FQVAL_MSB(v)	(((v)&0xff00)>>8)
#define FQVAL_LSB(v)	((v)&0xff)

/*
 * cpu_reset - perform any board specific start up initialization
 *
 */
void
cpu_reset(void)
{
#define WBFLUSH wbflush
	extern int cachewrback, _prom;
#ifdef R4600
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
#endif
	cachewrback = 1;

	mc_rev = *(volatile uint *)PHYS_TO_K1(SYSID) & SYSID_CHIP_REV_MASK;

	if (!_hpc3_write1 || !_hpc3_write2) {
		_hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET |
				LED_GREEN;	/* also green for guinness */
		_hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH |
			ETHER_UTP_STP | ETHER_PORT_SEL | UART0_ARC_MODE |
			UART1_ARC_MODE;
		*(volatile uint *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
		*(volatile uint *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
	}

	/* init _gio64_arb variable, then set the register */
	ip22_setup_gio64_arb();
	*(volatile unsigned int *)PHYS_TO_K1(GIO64_ARB) = _gio64_arb;
	WBFLUSH();

	/* Initialize burst/delay register for graphics and i/o DMA. 33MHz */
	*(volatile uint *)PHYS_TO_K1(LB_TIME) = LB_TIME_DEFAULT;
	*(volatile uint *)PHYS_TO_K1(CPU_TIME) = CPU_TIME_DEFAULT;

	/* mask out all interrupts except for power switch */
	*K1_LIO_0_MASK_ADDR = 0x0;
	*K1_LIO_1_MASK_ADDR = LIO_POWER;
	*K1_LIO_2_MASK_ADDR = 0x0;
	*K1_LIO_3_MASK_ADDR = 0x0;

	WBFLUSH();

#ifdef R4600
	if (orion | r4700 | r5000) {
		*(volatile uint *)PHYS_TO_K1(CPUCTRL0) |=
			CPUCTRL0_R4K_CHK_PAR_N |
			CPUCTRL0_GPR|CPUCTRL0_MPR;
		if (getenv("R4600_PARITY") != NULL) {
			WBFLUSH();
			*(volatile uint *)PHYS_TO_K1(CPUCTRL0) |=
				CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
		}
	} else
#endif
	*(volatile uint *)PHYS_TO_K1(CPUCTRL0) |= CPUCTRL0_R4K_CHK_PAR_N |
		CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
	WBFLUSH();

	kbdbell_init();

#ifndef IP24
	if (_prom && is_fullhouse()) eisa_refreshoff();
#endif

	scsiinit();

#ifdef US_DELAY_DEBUG
	check_us_delay();
#endif
}

/* Initialize keyboard bell */
static int bellok;
void
kbdbell_init(void)
{
	unsigned char nmi;
	extern int _prom;
	int val = 0;

	/* No bell if on guinness or EIU is missing */
	if (!is_fullhouse())
		return;

	/* EISA power-up hang hack -- use MC watchdog to reset us if we hang */
	if (_prom) {
		int cpuctrl0 = *(volatile int *)PHYS_TO_K1(CPUCTRL0);
		int ctrld = *(volatile int *)PHYS_TO_K1(CTRLD);
		int rc;

		/* clear dog, enable dog, bump refresh */
		*(volatile int *)PHYS_TO_K1(DOGC) = 0;
		*(volatile int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0 | CPUCTRL0_DOG;
		*(volatile int *)PHYS_TO_K1(CTRLD) = ctrld >> 3;
		flushbus();

		/* check EISA */
		rc = badaddr((void *)EISAIO_TO_K1(IntTimer1CommandMode),1);

		/* restore refresh, disable watchdog */
		*(volatile int *)PHYS_TO_K1(CTRLD) = ctrld;
		*(volatile int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
		flushbus();

		if (rc) cpu_hardreset();
	}
	else if (badaddr((void *)EISAIO_TO_K1(IntTimer1CommandMode),1))
		return;

	/* do ASAP since will quiet potentially run-away bell */
	nmi = INB(EISAIO_TO_K1(NMIStatus));
	nmi |= EISA_C2_ENB;
	nmi &= ~EISA_SPK_ENB;
	OUTB(EISAIO_TO_K1(NMIStatus),nmi);

	val = FQVAL(BELLFQ);

	OUTB(EISAIO_TO_K1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C2));
	OUTB(EISAIO_TO_K1(IntTimer1SpeakerTone),FQVAL_LSB(val));
	flushbus();
	delay(1);
	OUTB(EISAIO_TO_K1(IntTimer1SpeakerTone),FQVAL_MSB(val));
	flushbus();

	bellok = 1;
}

#ifndef IP24
static void
eisa_refreshoff(void)
{
	if (badaddr((void *)EISAIO_TO_K1(IntTimer1CommandMode),1))
		return;
	
	/* Turn EISA refresh off early as hardware has trouble with it on
	 * and it really does not need to be on anyway.
	 */
	OUTB(EISAIO_TO_K1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C1));
	OUTB(EISAIO_TO_K1(IntTimer1RefreshRequest), 0);
	flushbus();
	flushbus();			/* pseudo-us_delay(1) */
	OUTB(EISAIO_TO_K1(IntTimer1RefreshRequest), 0); /* refresh off! */
	flushbus();
}
#endif

/* LED_GREEN == LED_RED_OFF == 0x10 */
/* LED_AMBER == LED_GREEN_OFF == 0x20 */

static const char	led_masks[4][2] = {
	/* Fullhouse	 Guinness */
	{ 0,		LED_RED_OFF | LED_GREEN_OFF	},
	{ LED_GREEN,	LED_RED_OFF	/* green */	},
	{ LED_AMBER,	0 		/* amber */	},
	{ LED_AMBER,	LED_GREEN_OFF	/* red */	}
};

#define LED_bits	(LED_GREEN | LED_AMBER)		/* all relevent bits */

/*
 * set the main cpu led to the given color
 *	0 = black
 *	1 = green
 *	2 = amber
 *	3 = red		(amber on fullhouse)
 */
void
ip22_set_cpuled(int color)
{
	_hpc3_write1 &= ~LED_bits;
	_hpc3_write1 |= led_masks[color&0x03][is_fullhouse() == 0];
	*(volatile uint *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
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
	*(volatile __int32_t *)PHYS_TO_K1(MEMCFG0) :
	*(volatile __int32_t *)PHYS_TO_K1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    memconfig &= 0xffff;

    return memconfig;
}

/*
 * banksz - determine the size of a memory bank
 */
static __uint32_t
banksz(int bank)
{
    __uint32_t memconfig = get_mem_conf(bank);

    /* weed out bad module sizes */
    if (!(memconfig & MEMCFG_VLD))
        return 0;
    
    return ((memconfig & MEMCFG_DRAM_MASK) + 0x0100) <<
	(mc_rev >= 5 ? 16 : 14);
}       /* banksz */

/* Determine first address of a memory bank.
 */
static uint
bank_addrlo(int bank)
{
	return ((get_mem_conf(bank) & MEMCFG_ADDR_MASK) <<
		(mc_rev >= 5 ? 24 : 22));
}

/*
 * ip22_addr_to_bank - find bank in which the given addr belongs to
 */
int
ip22_addr_to_bank(__uint32_t addr)	/* phys addr is only 32 bits */
{
	__uint32_t base, size;
	int i;

	for (i = 0; i < 3; i++) {
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

#define K1_SYS_ID	(volatile uint *)PHYS_TO_K1(HPC3_SYS_ID)

/* 
 * return board revision level (0-7)
 */
int
ip22_board_rev(void)
{
	return (*K1_SYS_ID & BOARD_REV_MASK) >> BOARD_REV_SHIFT;
}

/*
 * return 1 if fullhouse system, 0 if guinness system
 * Since this routine is called about a zillion times, cache
 * the info for quicker returns (about 100 times faster).
 */
#if 	0
static char _is_fullhouse;
#endif
int
is_fullhouse(void)
{
#if	0		/* I need to double check if ram is fully available */
			/* early and this is re-init'd with init_prom_env() */
	register char is;

	if (is = _is_fullhouse)
		return (is == 1);
	is = (*K1_SYS_ID & BOARD_ID_MASK) == BOARD_IP22 ? 1 : 2;
#else
	return (*K1_SYS_ID & BOARD_ID_MASK) == BOARD_IP22;
#endif
}

int
is_indyelan(void)
{
        if((*K1_SYS_ID & BOARD_ID_MASK) == BOARD_IP24)
                return 1;
        else
                return 0;
}

/*
 * return revision (> 0) if IOC1 chip, 0 if not IOC.
 */
int
is_ioc1(void)
{
	uint id = *K1_SYS_ID;

	if ((id & CHIP_REV_MASK) == CHIP_IOC1) {
		/* IP22 boards with IOC also have IOC1.2 (aka IOC 2)
		 * P1 (and later) Guinness boards (ID>=2) all have IOC1.2
		 */
		if ( ((id & BOARD_ID_MASK) == BOARD_IP22) ||
		     (((id & BOARD_REV_MASK) >> BOARD_REV_SHIFT) >= 2) )
			return 2;

		/* Must be an old IOC1.1 (Guinness P0) */
		return 1;
	}
	
	/* Pre-006 Fullhouse system */
	return 0;
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
#define SYSBOARDID	"SGI-IP22"

static COMPONENT IP22tmpl = {
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

#ifdef HPC3_EXT_IO_ADDR_PX
	hpc3_ext_io_addr = (ip22_board_rev() == 0) ? HPC3_EXT_IO_ADDR_PX :
						     HPC3_EXT_IO_ADDR_P0;
#endif

	root = AddChild(NULL,&IP22tmpl,(void *)NULL);
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
	11,				/* IdentifierLength */
	"MIPS-R4000"			/* Identifier */
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
	14,				/* IdentifierLength */
	"MIPS-R4000FPC"			/* Identifier */
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
	extern unsigned _sidcache_size;
	extern unsigned _dcache_linesize;
	extern unsigned _icache_linesize;
	extern unsigned _scache_linesize;
#ifdef R4600
	extern unsigned _r4600sc_sidcache_size;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	int r4400 = (((get_prid()&0xf0)>>4) >= 4) &&
		    (! (orion | r4700 | r5000));
#else
	int r4400 = (((get_prid()&0xf0)>>4) >= 4);
#endif
	COMPONENT *c, *tmp;
	union key_u key;

	c = AddChild(root,&cputmpl,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("cpu");
	c->Key = cpuid();
	if (r4400) c->Identifier[7] = '4';
#ifdef R4600
	if (orion) c->Identifier[7] = '6';
	if (r4700) c->Identifier[7] = '7';
	if (r5000) c->Identifier[7] = '0';
	if (r5000) c->Identifier[6] = '5';
#endif

	tmp = AddChild(c,&fputmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("fpu");
	tmp->Key = cpuid();
	if (r4400) tmp->Identifier[7] = '4';
#ifdef R4600
	if (orion) tmp->Identifier[7] = '6';
	if (r4700) tmp->Identifier[7] = '7';
	if (r5000) tmp->Identifier[7] = '0';
	if (r5000) tmp->Identifier[6] = '5';
#endif

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("icache");
	key.cache.c_bsize = 1;
#ifdef R4600
	if (orion | r4700 | r5000) key.cache.c_bsize = 2;
#endif
	key.cache.c_lsize = log2(_icache_linesize);	/* R4000 */
	key.cache.c_size = log2(_icache_size >> 12);
	tmp->Key = key.FullKey;

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("dcache");
	key.cache.c_bsize = 1;
#ifdef R4600
	if (orion | r4700 | r5000) key.cache.c_bsize = 2;
#endif
	key.cache.c_lsize = log2(_dcache_linesize);	/* R4000 */
	key.cache.c_size = log2(_dcache_size >> 12);
	tmp->Type = PrimaryDCache;
	tmp->Key = key.FullKey;

	if (_sidcache_size != 0) {
		tmp = AddChild(c,&cachetmpl,(void *)NULL);
		if (tmp == (COMPONENT *)NULL) cpu_acpanic("s_cache");
		key.cache.c_bsize = 1;
		key.cache.c_lsize = log2(_scache_linesize);	/* R4000 */
		key.cache.c_size = log2(_sidcache_size >> 12);
		tmp->Type = SecondaryCache;
		tmp->Key = key.FullKey;
	}
#ifdef R4600
	else{	/* for R4600 with 2nd cache */
	   if (_r4600sc_sidcache_size != 0) {
		tmp = AddChild(c,&cachetmpl,(void *)NULL);
		if (tmp == (COMPONENT *)NULL) cpu_acpanic("s_cache");
		key.cache.c_bsize = 1;
		key.cache.c_lsize = log2(_scache_linesize);	/* R4000 */
		key.cache.c_size = log2(_r4600sc_sidcache_size >> 12);
		tmp->Type = SecondaryCache;
		tmp->Key = key.FullKey;
	   }
	}
#endif

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
    *(unsigned int *)PHYS_TO_K1(CPUCTRL0) = CPUCTRL0_SIN;
    WBFLUSH();
    while (1)
	;
}


char *simm_map_fh[3][4] = {
	{" <SIMM S12>", " <SIMM S11>", " <SIMM S10>", " <SIMM S9>"},
	{" <SIMM S8>", " <SIMM S7>", " <SIMM S6>", " <SIMM S5>"},
	{" <SIMM S4>", " <SIMM S3>", " <SIMM S2>", " <SIMM S1>"},
};

char *simm_map_g[2][4] = {
	{" <SIMM S1>", " <SIMM S2>", " <SIMM S3>", " <SIMM S4>"},
	{" <SIMM S5>", " <SIMM S6>", " <SIMM S7>", " <SIMM S8>"},
};

static void
_print_simms(int simms, int bank)
{
	int fh = is_fullhouse();

	if (simms & 0x1)
	    printf("%s ", fh? simm_map_fh[bank][0] : simm_map_g[bank][0]);
	if (simms & 0x2)
	    printf("%s ", fh? simm_map_fh[bank][1] : simm_map_g[bank][1]);
	if (simms & 0x4)
	    printf("%s ", fh? simm_map_fh[bank][2] : simm_map_g[bank][2]);
	if (simms & 0x8)
	    printf("%s ", fh? simm_map_fh[bank][3] : simm_map_g[bank][3]);
}

#define	GIO_ERRMASK	0xff00
#define	CPU_ERRMASK	0x3f00
void
cpu_show_fault(unsigned long saved_cause)
{
	if (saved_cause & CAUSE_BERRINTR) {
	    if (_cpu_parerr_save & CPU_ERR_STAT_ADDR)
		printf("CPU Bus Error Interrupt\n");
	    else if (_cpu_parerr_save & CPU_ERRMASK)
		printf("CPU Parity Error Interrupt\n");
	    else if (_gio_parerr_save & (GIO_ERR_STAT_RD|GIO_ERR_STAT_WR|
		GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR))
		printf("GIO Parity Error Interrupt\n");
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
		printf("Bus Error ?\n");
	}
		
#define WBFLUSH wbflush
	if (_liointr0_save) {
	printf("Local I/O interrupt register 0: 0x%x ", _liointr0_save );
	printf( "%r\n", _liointr0_save, _liointr0_desc );
	}
	if (_liointr1_save) {
	printf("Local I/O interrupt register 1: 0x%x ", _liointr1_save );
	printf( "%r\n", _liointr1_save, _liointr1_desc );
	    if(_liointr1_save & LIO_POWER) {
		printf("Power/Panel register: 0x%x %r\n", _power_intstat_save,
		_power_intstat_save ^ 0xF3 /* XXX - HACK */, _power_intstat_desc );
	    }
	}
	if (_liointr2_save) {
	printf("Local I/O interrupt register 2: 0x%x ", _liointr2_save );
	printf( "%r\n", _liointr2_save, _liointr2_desc );
	}

	if (_hpc3_intstat_save) {
	printf("HPC3 interrupt status register: 0x%x \n", _hpc3_intstat_save );
        printf( "%r\n", _hpc3_intstat_save, _hpc3_intstat_desc );
	}
	if (_hpc3_ext_io_save & EXTIO_HPC3_BUSERR)
	printf("HPC3 bus error status register: 0x%x \n", _hpc3_bus_err_stat_save );
	if ( _cpu_parerr_save & CPU_ERRMASK ) {
	    printf("CPU parity error register: %R\n", _cpu_parerr_save, _cpu_parerr_desc );
	    if (_cpu_parerr_save & CPU_ERR_STAT_ADDR)
		printf("CPU bus error: address: 0x%x\n", _cpu_paraddr_save );
	    else {
		printf("CPU parity error: address: 0x%x", _cpu_paraddr_save );

	    	/* print out bad SIMM location on read parity error */
	    	if (_cpu_parerr_save & (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)
		    == (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)) {

		    int bank = ip22_addr_to_bank(_cpu_paraddr_save);
		    uint bit;
		    int i;
		    uint simms = 0;

		    for (i = 0, bit = 0x1; bit <= 0x80; bit <<= 1, i++) {
		    	uint bad_addr;

		    	if (!(bit & _cpu_parerr_save))
			    continue;

#ifdef _MIPSEB
		    	bad_addr = (_cpu_paraddr_save & ~0x7) + (7 - i);
#else
		    	bad_addr = (_cpu_paraddr_save & ~0x7) + i;
#endif	/* _MIPSEB */

		    	simms |= 1 << ((bad_addr & 0xc) >> 2);
		    }

		    if (simms)
			_print_simms(simms, bank);

	        }

		printf("\n");
	    }
		
	    *(volatile int *)PHYS_TO_K1( CPU_ERR_STAT ) = 0;
	}

	if ( _gio_parerr_save & GIO_ERRMASK ) {
	    printf("GIO parity error register: %R\n", _gio_parerr_save, _gio_parerr_desc );
	    if (_gio_parerr_save & GIO_ERR_STAT_RD) {
		int bank = ip22_addr_to_bank(_gio_paraddr_save);
		uint bit;
		int i;
		uint simms = 0;

		printf("GIO parity error: address: 0x%x", _gio_paraddr_save );

		for (i = 0, bit = 0x1; bit <= 0x80; bit <<= 1, i++) {
		    uint bad_addr;

		    if (!(bit & _gio_parerr_save))
			continue;

#ifdef _MIPSEB
		    bad_addr = (_gio_paraddr_save & ~0x7) + (7 - i);
#else
		    bad_addr = (_gio_paraddr_save & ~0x7) + i;
#endif	/* _MIPSEB */

		    simms |= 1 << ((bad_addr & 0xc) >> 2);
		}

		if (simms)
		    _print_simms(simms, bank);

		printf("\n");
	    } else if (_gio_parerr_save & (GIO_ERR_STAT_WR|
		GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR))
		printf("GIO parity error: address: 0x%x\n", _gio_paraddr_save );
	    else
		printf("GIO bus error: address: 0x%x\n", _gio_paraddr_save );
		
	    *(volatile int *)PHYS_TO_K1( GIO_ERR_STAT ) = 0;
	}

	WBFLUSH();

	/* Re-enable power interrupt */
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	set_SR(SR_PROMBASE|SR_IE|SR_IBIT4|SR_BERRBIT);
	WBFLUSH();
}

/* function to shut off flat panel, set in ng1_init.c */
void (*fp_poweroff)(void);

void
cpu_soft_powerdown(void)
{
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	volatile int dummy;

	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
	  (*fp_poweroff)();
	
	ip22_power_switch_off();		/* disable auto-poweron */

#if DEBUG
	{ int pause = 200000; while (*p && --pause) ; }	/* XXX debug */
#endif
	/* wait for power switch to be released */
	do {
		*(volatile int *)PHYS_TO_K1(HPC3_PANEL) = POWER_ON;
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
	for (bank = 0; bank < 4; ++bank) {
	    memsize += banksz (bank);	/* sum up bank sizes */
	}
    }
    return memsize;
}

static unsigned int
cpu_get_low_memsize(void)
{
	unsigned lowmem = 0;
	int bank;

	for (bank = 0; bank < 4; ++bank) {
		if (bank_addrlo(bank) < SEG1_BASE)	/* is mapped low */
			lowmem += banksz (bank);	/* sum up bank sizes */
	}

	return lowmem;
}

static unsigned int
cpu_get_membase(void)
{
    return (unsigned int)PHYS_RAMBASE;
}

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
    z8530cons_errputc (c);
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
    /* add all of main memory with the exception of two pages that
     * will be also mapped to physical 0
     */
#define NPHYS0_PAGES	2
	int lowmem = cpu_get_low_memsize();

	/* add all of main memory with the exception of two pages that
	 * will be also mapped to physical 0
	 */
	md_add(cpu_get_membase()+ptob(NPHYS0_PAGES),
	       btop(_min(lowmem,0x10000000)) - NPHYS0_PAGES, FreeMemory);

	md_add(0, NPHYS0_PAGES, FreeMemory);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP22 cannot save configuration to NVRAM */
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

static u_char data_syndrome[] = {
	0x13, 0x23, 0x43, 0x83, 0x2f, 0xf1, 0x0d, 0x07,	/* bits 00-07 */
	0xd0, 0x70, 0x4f, 0xf8, 0x61, 0x62, 0x64, 0x68,	/* bits 08-15 */
	0x1c, 0x2c, 0x4c, 0x8c, 0x15, 0x25, 0x45, 0x85,	/* bits 16-23 */
	0x19, 0x29, 0x49, 0x89, 0x1a, 0x2a, 0x4a, 0x8a,	/* bits 24-31 */
	0x51, 0x52, 0x54, 0x58, 0x91, 0x92, 0x94, 0x98,	/* bits 32-39 */
	0xa1, 0xa2, 0xa4, 0xa8, 0x31, 0x32, 0x34, 0x38,	/* bits 40-47 */
	0x16, 0x26, 0x46, 0x86, 0x1f, 0xf2, 0x0b, 0x0e,	/* bits 48-55 */
	0xb0, 0xe0, 0x8f, 0xf4, 0xc1, 0xc2, 0xc4, 0xc8,	/* bits 56-63 */

	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,	/* check bits */
};

static u_int data_parity_matrix[8][2] = {
	{0xff280ff0, 0x88880928},
	{0xfa24000f, 0x4444ff24},
	{0x0b22ff00, 0x2222fa32},
	{0x0931f0ff, 0x11110b21},
	{0x84d08888, 0xff0f8c50},
	{0x4c9f4444, 0x00ff44d0},
	{0x24ff2222, 0xf000249f},
	{0x14501111, 0x0ff014ff},
};

static u_char tag_syndrome[] = {
	0x07, 0x16, 0x26, 0x46, 0x0d, 0x0e, 0x1c, 0x4c,	/* bits 00-07 */
	0x31, 0x32, 0x38, 0x70, 0x61, 0x62, 0x64, 0x68,	/* bits 08-15 */
	0x0b, 0x15, 0x23, 0x45, 0x29, 0x51, 0x13, 0x49,	/* bits 16-23 */
	0x25,						/* bit 24 */
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,	/* check bits */
};

static u_int tag_parity_matrix[7] = {
	{0x0a8f888},
	{0x114ff04},
	{0x0620f42},
	{0x09184f0},
	{0x10a40ff},
	{0x045222f},
	{0x1ff1111},
};

static char *position[] = {
	"U5", "U3", "U2", "U1", "U11", "U9", "U8", "U6"
};

#ifdef DEBUG
int no_ecc_fault;
#endif	/* DEBUG */

/* ECC checking should be turned off ? */
void
ecc_error_decode(u_int ecc_error_reg)
{
	int	bad_bit;
	__psunsigned_t	bad_cache_addr =
			PHYS_TO_K0(ecc_error_reg & CACHERR_SIDX_MASK);
	u_int	data_hi_read;
	u_int	data_lo_read;
	u_int	ecc;
	u_int	ecc_computed;
	u_int	ecc_read;
	int	i;
	int	j;
	k_machreg_t oldSR;
	int	one_count;
	u_int	phy_addr;
	u_int	syndrome;
	u_int	tag[2];
	u_int	tag_read;

#ifdef DEBUG
	no_ecc_fault = 1;
#endif	/* DEBUG */

	/* no need to decode primary cache error or SYSAD bus error */
	if (!(ecc_error_reg & CACHERR_EC) || (ecc_error_reg & CACHERR_EE))
		return;

	oldSR = get_SR();
	set_SR(oldSR | SR_DE);
	
	ecc_read = _read_tag(CACH_SD, bad_cache_addr, tag) & 0xff;
	phy_addr = ((tag[0] & SADDRMASK) << 4) | (bad_cache_addr & 0x1ffff);

	/* decode secondary cache data error */
	if (!(ecc_error_reg & CACHERR_ED))
		goto check_tag;

	/* read possible bad data from from the cache */
	data_hi_read = *(u_int *)PHYS_TO_K0(phy_addr);
	data_lo_read = *(u_int *)PHYS_TO_K0(phy_addr + 4);

	/* computed the ECC using the data read */
	ecc_computed = 0;
	for (i = 0; i < 8; i++) {
		ecc = (data_hi_read & data_parity_matrix[i][0]) ^
			(data_lo_read & data_parity_matrix[i][1]);

		one_count = 0;
		while (ecc) {
			if (ecc & 0x1)
				one_count++;
			ecc >>= 1;
		}

		/* even parity */
		ecc_computed = (ecc_computed << 1) | (one_count & 0x1);
	}
	syndrome = ecc_read ^ ecc_computed;
#ifdef DEBUG
	printf("data: syndrome(0x%x) ecc_read(0x%x) ecc_computed(0x%x), data_hi(0x%x), data_lo(0x%x)\n", syndrome, ecc_read, ecc_computed, data_hi_read, data_lo_read);
#endif

	if (syndrome == 0x0)
		goto check_tag;

#ifdef DEBUG
	no_ecc_fault = 0;
#endif	/* DEBUG */

	bad_bit = -1;
	for (j = 0; j < sizeof(data_syndrome) / sizeof(u_char); j++) {
		if (syndrome == data_syndrome[j]) {
			bad_bit = j;
			break;
		}
	}

	if (bad_bit == -1) {
		/*
		 * if the cache line state is clean exclusive, we can
		 * read the data from memory and check for bad bits
		 */
		if ((tag[0] & SSTATEMASK) == SCLEANEXCL) {
			u_int uc_data_hi_read;
			u_int uc_data_lo_read;
			u_int bad_bits_mask[2];

			uc_data_hi_read = *(u_int *)PHYS_TO_K1(phy_addr);
			uc_data_lo_read = *(u_int *)PHYS_TO_K1(phy_addr + 4);

			bad_bits_mask[0] = uc_data_hi_read ^ data_hi_read;
			bad_bits_mask[1] = uc_data_lo_read ^ data_lo_read;

			if (bad_bits_mask[0] || bad_bits_mask[1]) {
#ifdef DEBUG
				printf("memory (0x%x, 0x%x) does not match cache (0x%x, 0x%x)\n",
					uc_data_hi_read, uc_data_lo_read,
					data_hi_read, data_lo_read);
#endif
				if (bad_cache_addr & 0x8)
					i = 4;
				else
					i = 0;

				printf("Check/Replace static RAMs");
				if (bad_bits_mask[1] & 0xffff)
					printf(" %s", position[i + 0]);
				if ((bad_bits_mask[1] >> 16) & 0xffff)
					printf(" %s", position[i + 1]);
				if (bad_bits_mask[0] & 0xffff)
					printf(" %s", position[i + 2]);
				if ((bad_bits_mask[0] >> 16) & 0xffff)
					printf(" %s", position[i + 3]);
				printf(".\n");
			} else
				/* if data is ok, must be the ECC */
				printf("Check/Replace static RAM U4.\n");
		} else 
			printf("Unrecoverable multiple bits error.\n");
	} else {
		if (bad_bit <= 63) {
			i = bad_bit / 16;
			if (bad_cache_addr & 0x8)
				i += 4;
#ifdef DEBUG
			printf("Bit %d in secondary cache data field is bad\n",
				bad_bit);
#endif
			printf("Check/Replace static RAM %s.\n", position[i]);
		} else {
#ifdef DEBUG
			printf("Bit %d in secondary cache ECC field is bad\n", bad_bit - 64);
#endif
			printf("Check/Replace static RAM U4.\n");
		}
	}

check_tag:
	/* decode secondary cache tag error */
	if (!(ecc_error_reg & CACHERR_ET))
		goto done;

	ecc_read = tag[0] & 0x7f;
	tag_read = (tag[0] >> 13) | (((tag[0] >> 7) & 0x3f) << 19);

	ecc_computed = 0;
	for (i = 0; i < 7; i++) {
		ecc = tag_read & tag_parity_matrix[i];

		one_count = 0;
		while (ecc) {
			if (ecc & 0x1)
				one_count++;
			ecc >>= 1;
		}

		/* even parity */
		ecc_computed = (ecc_computed << 1) | (one_count & 0x1);
	}
	syndrome = ecc_read ^ ecc_computed;
#ifdef DEBUG
	printf("tag syndrome(0x%x) ecc_read(0x%x) ecc_computed(0x%x)\n", syndrome, ecc_read, ecc_computed);
#endif

	if (syndrome == 0x0)
		goto done;

#ifdef DEBUG
	no_ecc_fault = 0;
#endif	/* DEBUG */

	bad_bit = -1;
	for (j = 0; j < sizeof(tag_syndrome) / sizeof(u_char); j++) {
		if (syndrome == tag_syndrome[j]) {
			bad_bit = j;
			break;
		}
	}

	if (bad_bit == -1) {
		u_int bad_bits_mask = 0;

		for (i = 0x80; i != 0x0; i <<= 1) {
			tag[0] = i;
			_write_tag(CACH_SD, bad_cache_addr, tag);
			_read_tag(CACH_SD, bad_cache_addr, tag);
			if (tag[0] != i)
				bad_bits_mask |= i;
		}

		if (bad_bits_mask & 0xffff0000)
			printf("Check/Replace static RAM U7.\n");
		else
			printf("Check/Replace static RAM U10.\n");

	} else {
		if (bad_bit < 25) {
#ifdef DEBUG
			printf("Bit %d in secondary cache tag field is bad\n", bad_bit);
#endif
			if (bad_bit <= 8)
				printf("Check/Replace static RAM U10.\n");
			else
				printf("Check/Replace static RAM U7.\n");
		} else
#ifdef DEBUG
			printf("Bit %d in secondary cache tag ECC field is bad\n", bad_bit - 25);
#endif
			printf("Check/Replace static RAM U10.\n");
	}

done:
	set_SR(oldSR);
}

void
cpu_scandevs(void)
{
	unsigned char intr = *(unsigned char *)PHYS_TO_K1(LIO_1_ISR_ADDR);

	if (intr & LIO_MASK_POWER) {
		/* Guinness sets the POWER_INT low but Indigo2 does not. */
		if ( !(*(volatile uint *)PHYS_TO_K1(HPC3_PANEL) & POWER_INT)
		|| is_fullhouse()) {
#if DEBUG
			printf("Power switch detected in cpu_scandevs()\n");
#endif
			cpu_soft_powerdown();
	}
	}
}

void
bell(void)
{
	unsigned char stat;

	if (!bellok)
		return;

	/* Turn on speaker */
	stat = INB(EISAIO_TO_K1(NMIStatus));
	OUTB(EISAIO_TO_K1(NMIStatus),stat|EISA_SPK_ENB);

	DELAY(50000);			/* let ring */

	/* Turn off speaker */
	OUTB(EISAIO_TO_K1(NMIStatus),stat);
}

/* check if address is inside a "protected" area */
int
is_protected_adrs(unsigned long low, unsigned long high)
{
#ifndef	DEBUG			/* turn off protection in DEBUG PROM */
	if (is_fullhouse())
	    return 0;		/* no protected areas? */
	
	/* Guinness */

	/* first protected area is in nvram */
	if (high < (unsigned long) NVRAM_ADRS(NVFUSE_START) ||
	     low > (unsigned long) NVRAM_ADRS(NVLEN_MAX))
	   /* ok */;
	else
	   return 1;		/* protected */

	/* anything else? */
#endif
	return 0;		/* not protected */
}

/* determine speed of GIO bus */
int
ip22_gio_is_33MHZ(void)
{
	if (is_fullhouse()) {
		return *(volatile int *)PHYS_TO_K1(HPC3_EXT_IO_ADDR) &
			EXTIO_GIO_33MHZ;
	}
	return *(volatile int *)PHYS_TO_K1(HPC3_GEN_CONTROL)&GC_GIO_33MHZ;
}

/*  Set-up software copy of GIO64_ARB.  For prom it was already
 * set by IP22prom/csu.s
 */
void
ip22_setup_gio64_arb(void)
{
	if (is_fullhouse())
		_gio64_arb = (ip22_board_rev() >= 2) ? _gio64_arb_004 :
							   _gio64_arb_003 ;
	else
		_gio64_arb = _gio64_arb_gns;
}

/* calculate ticks per 1024 instructions on a guinness P0 board */
int
_ioc1_ticksper1024inst()
{
    int freq = cpu_get_freq(0);
    int count = _timerticksper1024inst();

    return (count+((freq/2)-1)/freq);
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


#endif /*  IP22 (whole file) */
