#ident	"$Revision: 1.50 $"

#ifdef IP26	/* whole file */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/z8530.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/ds1286.h>
#include <sys/immu.h>
#include <sys/eisa.h>
#include <arcs/spb.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/cfgtree.h>
#include <arcs/time.h>
#include <genpda.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/IP22nvram.h>

#define	INB(x)		(*(volatile u_char *)(x))
#define	OUTB(x,y)	(*(volatile u_char *)(x) = (y))

#if DEBUG
extern int Debug;
#define	DPRINTF	if (Debug) printf
#else
#define	DPRINTF
#endif

#define	NUMBANKS    3	/* 3 main mem banks on IP22.  4th is used for ECC */

static int teton_gcache_check(long, long, long);
static unsigned	cpu_get_membase(void);
static void eisa_refreshoff(void);
void ip22_setup_gio64_arb(void);

unsigned mc_rev;

/*
 * IP26 board registers saved during exceptions
 *
 * These are referenced by the assembly language fault handler
 *
 */
int	_hpc3_intstat_save, _hpc3_bus_err_stat_save, _hpc3_ext_io_save;
long	_tcc_intr_save, _tcc_error_save, _tcc_be_addr_save, _tcc_parity_save;
int	_cpu_parerr_save, _gio_parerr_save;
char	_liointr0_save, _liointr1_save, _liointr2_save;
__uint32_t _cpu_paraddr_save, _gio_paraddr_save;

/* software copy of hardware write-only registers */
uint _hpc3_write1;
uint _hpc3_write2;
uint _gio64_arb;		/* not ro, but not always correct */

/* default values for different revs of base board */
uint _gio64_arb_004 =	GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST;

int sk_sable = 0;	/* hack since we do not use delay.c */

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
	/* mask	shift	name		format	values */
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
	/* mask	shift	name		format	values */
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
	/* mask	shift	name		format	values */
	{ 0x8,	-3,	"EISA",		NULL,	NULL },
	{ 0x10,	-4,	"KBD/MOUSE",	NULL,	NULL },
	{ 0x20,	-5,	"DUART",	NULL,	NULL },
	{ 0x40, -6,	"SLOT0",	NULL,	NULL },
	{ 0x80,	-7,	"SLOT1",	NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

/* used by HPC3 diagnostic */
struct reg_desc _hpc3_intstat_desc[] = {
	/* mask	shift	name		format	values */
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

static struct reg_values tcc_errortype[] = {
	{ ERROR_SYSAD_TDB,	"sysAD data parity (TDB)" },
	{ ERROR_SYSAD_TCC,	"sysAD data parity (TCC)" },
	{ ERROR_SYSAD_CMD,	"sysAD cmd parity" },
	{ ERROR_SYSAD_UFO,	"Unknown sysAD cmd" },
	{ ERROR_GCACHE_PARITY,	"GCache data parity error" },
	{ ERROR_TBUS_UFO,	"Unknown T-Bus cmd" },
	{ 0,			NULL }
};

struct reg_desc _tcc_error_desc[] = {
	{ ERROR_NESTED_BE,	0,			"2nd-BUSERR",
	  NULL,	NULL },
	{ ERROR_NESTED_MC,	0,			"2nd-MACHCK",
	  NULL,	NULL },
	{ ERROR_TYPE, 		-ERROR_TYPE_SHIFT,	"Type",	
	  NULL,	tcc_errortype },
	{ ERROR_OFFSET,		-ERROR_OFFSET_SHIFT,	"Offset",
	  "%x",	NULL },
	{ ERROR_INDEX,		-ERROR_INDEX_SHIFT,	"Index",
	  "%x",	NULL },
	{ ERROR_PARITY_SET,	-ERROR_PARITY_SET_SHIFT,"Set",	
	  "%x",	NULL },
	{ ERROR_SYSCMD,		-ERROR_SYSCMD_SHIFT,	"SysCmd",
	  "%x",	NULL },
	{ 0, 0, 0, NULL, NULL }
};

struct reg_desc _tcc_intr_desc[] = {
	{ 0x0001,		0,	"IP3",		NULL, NULL },
	{ 0x0002,		0,	"IP4",		NULL, NULL },
	{ 0x0004,		0,	"IP5",		NULL, NULL },
	{ 0x0008,		0,	"IP6",		NULL, NULL },
	{ 0x0010,		0,	"IP7",		NULL, NULL },
	{ 0x0020,		0,	"IP8",		NULL, NULL },
	{ INTR_FIFO_HW,		0,	"HW",		NULL, NULL },
	{ INTR_FIFO_LW,		0,	"LW",		NULL, NULL },
	{ INTR_TIMER,		0,	"TIMER",	NULL, NULL },
	{ INTR_BUSERROR,	0,	"BusError",	NULL, NULL },
	{ INTR_MACH_CHECK,	0,	"MachCheck",	NULL, NULL },
	{ INTR_FIFO_HW_EN,	0,	"HWen",		NULL, NULL },
	{ INTR_FIFO_LW_EN,	0,	"LWen",		NULL, NULL },
	{ INTR_TIMER_EN,	0,	"TIMEen",	NULL, NULL },
	{ INTR_BUSERROR_EN,	0,	"BEen",		NULL, NULL },
	{ INTR_MACH_CHECK_EN,	0,	"MCen",		NULL, NULL },
	{ 0, 0, 0, NULL, NULL }
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
	extern int cachewrback, _prom;

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
	wbflush();

	/* Initialize burst/delay register for graphics and i/o DMA. 33MHz */
	*(volatile uint *)PHYS_TO_K1(LB_TIME) = LB_TIME_DEFAULT;
	*(volatile uint *)PHYS_TO_K1(CPU_TIME) = CPU_TIME_DEFAULT;

	/* mask out all interrupts except for power switch */
	*K1_LIO_0_MASK_ADDR = 0x0;
	*K1_LIO_1_MASK_ADDR = LIO_POWER;
	*K1_LIO_2_MASK_ADDR = 0x0;
	*K1_LIO_3_MASK_ADDR = 0x0;
	wbflush();

	*(volatile uint *)PHYS_TO_K1(CPUCTRL0) |= CPUCTRL0_R4K_CHK_PAR_N |
		CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
	wbflush();

	kbdbell_init();

	if (_prom) eisa_refreshoff();

	scsiinit();

	/*  Need to ensure restart transfer vectors are uncached so they
	 * can recover from running in 'normal' kernel mode with an
	 * aggressive setting in TCC_GCACHE.
	 */
	if (_prom) {
		FirmwareVector *tv = SPB->TransferVector;
		tv->Halt = (void (*)())K0_TO_K1(tv->Halt);
		tv->PowerDown = (void (*)())K0_TO_K1(tv->PowerDown);
		tv->Restart = (void (*)())K0_TO_K1(tv->Restart);
		tv->Reboot = (void (*)())K0_TO_K1(tv->Reboot);
		tv->EnterInteractiveMode =
			(void (*)())K0_TO_K1(tv->EnterInteractiveMode);
	}
}

/* Initialize keyboard bell to freq.
 */
void
setbell(int freq)
{
	unsigned char nmi;
	int val = 0;

	/* do ASAP since will quiet potentially run-away bell */
	nmi = INB(EISAIO_TO_K1(NMIStatus));
	nmi |= EISA_C2_ENB;
	nmi &= ~EISA_SPK_ENB;
	OUTB(EISAIO_TO_K1(NMIStatus),nmi);

	val = FQVAL(freq);

	OUTB(EISAIO_TO_K1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C2));
	OUTB(EISAIO_TO_K1(IntTimer1SpeakerTone),FQVAL_LSB(val));
	flushbus();
	delay(1);
	OUTB(EISAIO_TO_K1(IntTimer1SpeakerTone),FQVAL_MSB(val));
	flushbus();
}

void
eisa_refreshoff(void)
{
	/* Turn EISA refresh off early as hardware has trouble with it on
	 * and it really does not need to be on anyway.
	 */
	OUTB(EISAIO_TO_K1(IntTimer1CommandMode),
		(EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C1));
	flushbus();
	OUTB(EISAIO_TO_K1(IntTimer1RefreshRequest), 0);
	flushbus();
	flushbus();			/* pseudo-us_delay(1) */
	OUTB(EISAIO_TO_K1(IntTimer1RefreshRequest), 0); /* refresh off! */
	flushbus();
}

/* Initialize bell to normal keyboard bell.
 */
void
kbdbell_init(void)
{
	extern int _prom;

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

	setbell(BELLFQ);
}


/* LED_GREEN == LED_RED_OFF == 0x10 */
/* LED_AMBER == LED_GREEN_OFF == 0x20 */

static const char led_masks[4] = {
	0,		/* off */
	LED_GREEN,	/* green */
	LED_AMBER,	/* amber */
	LED_AMBER	/* amber (red on indy) */
};

/*
 * set the main cpu led to the given color
 *	0 = black
 *	1 = green
 *	2 = amber
 *	3 = amber	(red on indy)
 */
void
ip22_set_cpuled(int color)
{
	_hpc3_write1 &= ~(LED_GREEN|LED_AMBER);
	_hpc3_write1 |= led_masks[color&0x03];
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
		*(volatile __uint32_t *)PHYS_TO_K1(MEMCFG0) :
		*(volatile __uint32_t *)PHYS_TO_K1(MEMCFG1) ;
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
}

/* Determine first address of a memory bank.
 */
static __uint32_t
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
	uint base;
	uint size;
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
 *  IP26 specific hardware inventory routine
 */

/* 
 * return board revision level (0-7)
 */
int
ip22_board_rev(void)
{
	return (*(volatile uint *)PHYS_TO_K1(HPC3_SYS_ID) & BOARD_REV_MASK)
	       >> BOARD_REV_SHIFT;
}

/* For compatability with some IP22 code.  Always 1 for teton.
 */
int
is_fullhouse(void)
{
	return 1;
}

/* For compatability with some IP22 code.  Always 0 for teton.
 */
int
is_ioc1(void)
{
	return 0;		/* no IOC base fullhouse */
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
#define SYSBOARDID	"SGI-IP26"

static COMPONENT IP26tmpl = {
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

	root = AddChild(NULL,&IP26tmpl,(void *)NULL);
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
	10,				/* IdentifierLength */
	"MIPS-R8000"			/* Identifier */
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
	10,				/* IdentifierLength */
	"MIPS-R8000"			/* Identifier */
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
	extern int _picache_size, _pdcache_size, _sidcache_size;
	machreg_t config = tfp_getconfig();
	COMPONENT *c, *tmp;
	union key_u key;
	int ls;

	c = AddChild(root,&cputmpl,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("cpu");
	c->Key = cpuid();

	tmp = AddChild(c,&fputmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("fpu");
	tmp->Key = cpuid();

	/*  config_cache sizes caches but does not save line sizes, so
	 * get them out of the config register, or hard code them.
	 */
	ls = 1 << (((config&CONFIG_IB)>>5)+4);
	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("icache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(ls);
	key.cache.c_size = log2(_picache_size >> 12);
	tmp->Key = key.FullKey;

	ls = 1 << (((config&CONFIG_DB)>>4)+4);
	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("dcache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(ls);
	key.cache.c_size = log2(_pdcache_size >> 12);
	tmp->Type = PrimaryDCache;
	tmp->Key = key.FullKey;

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("s_cache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(128);
	key.cache.c_size = log2(_sidcache_size >> 12);
	tmp->Type = SecondaryCache;
	tmp->Key = key.FullKey;

	tmp = AddChild(c,&memtmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("main memory ");
	tmp->Key = cpu_get_memsize() >> ARCS_BPPSHIFT;	/* number of 4k pages */
}

/*
 * cpu_hardreset - hit the system reset bit on the cpu
 */
void
cpu_hardreset(void)
{
	*(unsigned int *)PHYS_TO_K1(CPUCTRL0) = CPUCTRL0_SIN;
	wbflush();
	while (get_prid())			/* spin */
		;
}

/*
 * ip26_clear_ecc
 * Clear old single-bit errors and uncached writes
 */
static void
ip26_clear_ecc(void)
{
	*(volatile __uint64_t *)PHYS_TO_K1(ECC_CTRL_REG) = ECC_CTRL_CLR_INT;
	flushbus();
}

char *simm_map_fh[3][4] = {
	{"S12", "S11", "S10", "S9"},
	{"S8", "S7", "S6", "S5"},
	{"S4", "S3", "S2", "S1"},
};

static void
_print_simms(int simms, int bank)
{
	if (simms & 0x1) printf(" %s", simm_map_fh[bank][0]);
	if (simms & 0x2) printf(" %s", simm_map_fh[bank][1]);
	if (simms & 0x4) printf(" %s", simm_map_fh[bank][2]);
	if (simms & 0x8) printf(" %s", simm_map_fh[bank][3]);
}

static void
log_memerr(unsigned int addr)
{
	int bank = ip22_addr_to_bank(addr);
	uint bit, simms = 0;
	uint bad_addr;
	int i;

	if (ip26_isecc()) {
		simms = (!(addr & 0x8)) ? 0x3 : 0xc;
	}
	else {
		for (i = 0, bit = 0x1; bit <= 0x80; bit <<= 1, i++) {
			if (!(bit & addr))
				continue;

			bad_addr = (addr & ~0x7) + (7 - i);

			simms |= 1 << ((bad_addr & 0xc) >> 2);
		}
	}

	if (simms)
		_print_simms(simms, bank);

	printf("\n");
}

#define	GIO_ERRMASK	0xff00
#define	CPU_ERRMASK	0x3f00
void
cpu_show_fault(unsigned long saved_cause)
{
	void _resume(void);
	int resume = 0;

	if (saved_cause & (CAUSE_IP9|CAUSE_IP10)) {
		set_cause(get_cause() & ~(CAUSE_IP9 | CAUSE_IP10));
		gen_pda_tab[0].gdata.regs[R_CAUSE] &= ~(CAUSE_IP9|CAUSE_IP10);
		printf("*** GCache parity error.\n");
		resume = 1;
		goto done;
	}

	if (saved_cause & CAUSE_IP8) {
		ip26_clear_ecc();

		printf("Single bit errors have been corrected\n");
	}

	if (saved_cause & (CAUSE_BERRINTR|CAUSE_IP7)) {
		if (_tcc_intr_save & INTR_BUSERROR)
			printf("CPU (TCC) Bus Error Interrupt\n");
		else if (_tcc_intr_save & INTR_MACH_CHECK)
			printf("CPU Machine Check Interrupt\n");

		if (_cpu_parerr_save & CPU_ERR_STAT_ADDR)
			printf("CPU (MC) Bus Error Interrupt\n");
		else if (_cpu_parerr_save & CPU_ERRMASK) {
			if (ip26_isecc() &&
			    _cpu_parerr_save & CPU_ERR_STAT_RD_PAR) {
				ip26_clear_ecc();
				if (_cpu_parerr_save & ECC_MULTI_BIT_ERR)
					printf("CPU Multi-bit ECC Error\n");
				if (_cpu_parerr_save & ECC_FAST_UC_WRITE)
					printf("Illegal uncached write\n");
			}
			else
				printf("CPU Parity Error Interrupt\n");
		}

		if (_gio_parerr_save & (GIO_ERR_STAT_RD|GIO_ERR_STAT_WR|
			     GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR)) {
			if (ip26_isecc() && _gio_parerr_save&GIO_ERR_STAT_RD) {
				if (_gio_parerr_save & ECC_MULTI_BIT_ERR)
					printf("GIO Multi-bit ECC Error\n");
				if (_gio_parerr_save & ECC_FAST_UC_WRITE)
					printf("Illegal uncached write\n");
			}
			else
				printf("GIO Parity Error Interrupt\n");
		}
		else if (_gio_parerr_save & GIO_ERR_STAT_TIME)
			printf("GIO Timeout Interrupt\n");
		else if (_gio_parerr_save & GIO_ERR_STAT_PROM)
			printf("PROM Write Interrupt\n");
		else if (_gio_parerr_save & GIO_ERRMASK)
			printf("GIO Bus Error Interrupt\n");

		if (_hpc3_ext_io_save & EXTIO_EISA_BUSERR)
			printf("EISA Bus Error Interrupt\n");
		else if (_hpc3_ext_io_save & EXTIO_HPC3_BUSERR)
			printf("HPC3 Bus Error Interrupt\n");
	}
		
	if (_liointr0_save)
		printf("Local I/O interrupt register 0: %R\n",
		       _liointr0_save, _liointr0_desc);
	if (_liointr1_save)
		printf("Local I/O interrupt register 1: 0x%R\n",
		       _liointr1_save, _liointr1_desc);
	if (_liointr2_save)
		printf("Local I/O interrupt register 2: %R\n",
		       _liointr2_save, _liointr2_desc);

	if (_tcc_intr_save & 0x07ff)
		printf("TCC Interrupt register %R\n",
		       _tcc_intr_save, _tcc_intr_desc);

	if (_tcc_intr_save & (INTR_BUSERROR|INTR_MACH_CHECK)) {
		printf("TCC Bus Error register: 0x%x\n"
		       "TCC Parity register: 0x%x\n",
			_tcc_be_addr_save, _tcc_parity_save);

		if (_tcc_intr_save & INTR_MACH_CHECK)
			printf("TCC Error register: %R\n",
				_tcc_error_save, _tcc_error_desc);

		/* Clear bus error and machine check flags
		 */
		*(volatile long *)PHYS_TO_K1(TCC_INTR) = _tcc_intr_save &
				(INTR_BUSERROR|INTR_MACH_CHECK|0xf800);
		*(volatile long *)PHYS_TO_K1(TCC_ERROR) =
				(ERROR_NESTED_MC|ERROR_NESTED_BE);

		resume = teton_gcache_check(_tcc_intr_save,_tcc_error_save,
					    _tcc_parity_save);
	}

	if (_hpc3_intstat_save)
		printf("HPC3 interrupt status register: %R\n",
		       _hpc3_intstat_save, _hpc3_intstat_desc);
	if (_hpc3_ext_io_save & EXTIO_HPC3_BUSERR)
		printf("HPC3 bus error status register: 0x%x \n",
		       _hpc3_bus_err_stat_save);

	if (_cpu_parerr_save & CPU_ERRMASK) {
		printf("CPU Error register: %R\n",
		       _cpu_parerr_save, _cpu_parerr_desc);
		if (_cpu_parerr_save & CPU_ERR_STAT_ADDR)
			printf("CPU bus error: address: 0x%x\n",
			       (unsigned long)_cpu_paraddr_save);
		else {
			printf("CPU Error: address: 0x%x",
			       (unsigned long)_cpu_paraddr_save);

			/* print out bad SIMM location on read parity error
			 */
			if (_cpu_parerr_save&(CPU_ERR_STAT_RD|CPU_ERR_STAT_PAR)
			    == (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)) {
				log_memerr(_cpu_parerr_save);
			}

			printf("\n");
		}
		
		*(volatile int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	}

	if (_gio_parerr_save & GIO_ERRMASK) {
		printf("GIO Error register: %R\n", _gio_parerr_save,
		       _gio_parerr_desc);

		if (_gio_parerr_save & GIO_ERR_STAT_RD) {
			printf("GIO Error address: 0x%x", _gio_paraddr_save );
			log_memerr(_gio_parerr_save);
		}
		else if (_gio_parerr_save & (GIO_ERR_STAT_WR|
				GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR))
			printf("GIO Error address: 0x%x\n",
			       (unsigned long)_gio_paraddr_save);
		else
			printf("GIO Bus error address: 0x%x\n",
			       (unsigned long)_gio_paraddr_save);
		
		*(volatile int *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	}

	wbflush();

	/* Re-enable power interrupt */
	*K1_LIO_1_MASK_ADDR = LIO_MASK_POWER;
	set_SR(SR_PROMBASE|SR_IBIT4);
	wbflush();

done:
	if (resume) {
		clear_nofault();
		_resume();
	}
}

/* function to shut off flat panel, set in ng1_init.c or gr2_init.c */
void (*fp_poweroff)();

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

	/* wait for power switch to be released */
	do {
		*(volatile int *)PHYS_TO_K1(HPC3_PANEL) = POWER_ON;
		flushbus();
		us_delay(5000);
	} while (*K1_LIO_1_ISR_ADDR & LIO_POWER);

	while (get_prid()) {
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
	for (bank = 0; bank < NUMBANKS; ++bank) {
	    memsize += banksz (bank);	/* sum up bank sizes */
	}
    }
    return memsize;
}

unsigned int
cpu_get_low_memsize(void)
{
	unsigned int lowmem = 0;
	int bank;

	for (bank = 0; bank < NUMBANKS; ++bank) {
		if (bank_addrlo(bank) < SEG1_BASE)	/* is mapped low */
			lowmem += banksz(bank);		/* sum up bank sizes */
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
	/* get the console environment variable directly from the nvram */
	/* this resolves an issue that the memory version and the nvram */	
	/* version of the console environment variable may differ in the */
	/* case when the password jumper has been removed */
	if (!strcmp(cpu_get_nvram_offset(NVOFF_CONSOLE, NVLEN_CONSOLE), "d2"))
		return("serial(1)");
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
	int lowmem = cpu_get_low_memsize();
	int highmem = cpu_get_memsize() - lowmem;

	/* add all of main memory with the exception of two pages that
	 * will be also mapped to physical 0
	 */
	md_add(cpu_get_membase()+arcs_ptob(NPHYS0_PAGES),
	       arcs_btop(lowmem) - NPHYS0_PAGES, FreeMemory);

	md_add(0, NPHYS0_PAGES, FreeMemory);

	if (highmem > 0)
		md_add(SEG1_BASE, arcs_btop(highmem), FreeMemory);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP26 cannot save configuration to NVRAM
 */
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

void
cpu_scandevs(void)
{
	if (*(unsigned char *)PHYS_TO_K1(LIO_1_ISR_ADDR) & LIO_MASK_POWER) {
#if DEBUG
		printf("Power switch detected in cpu_scandevs()\n");
#endif
		cpu_soft_powerdown();
	}
}

/* Playbell is used by the prom for the reduced fat badgfx tune.
 */
void
playbell(int delay)
{
	unsigned char stat;

	/* Turn on speaker */
	stat = INB(EISAIO_TO_K1(NMIStatus));
	OUTB(EISAIO_TO_K1(NMIStatus),stat|EISA_SPK_ENB);

	DELAY(delay);			/* let ring */

	/* Turn off speaker */
	OUTB(EISAIO_TO_K1(NMIStatus),stat);
}

/* Normal keyboard bell interface.
 */
void
bell(void)
{
	playbell(50000);
}

/* check if address is inside a "protected" area */
/*ARGSUSED*/
int
is_protected_adrs(unsigned long low, unsigned long high)
{
	return 0;		/* no protected areas? */
}

/* determine speed of GIO bus */
int
ip22_gio_is_33MHZ(void)
{
	return *(volatile int *)PHYS_TO_K1(HPC3_EXT_IO_ADDR) & EXTIO_GIO_33MHZ;
}

/*  Set-up software copy of GIO64_ARB.  For prom it was already
 * set by IP26prom/csu.s
 */
void
ip22_setup_gio64_arb(void)
{
	/* teton only works with newer (>=007) boards */
	_gio64_arb = _gio64_arb_004;
}

/* Used in pon/diags. */
char *srams[][5] = {
	{"S15|S16", "S6|S12", "S10|S11", "S4|S5", "even unknown"},
	{"S13|S14", "S3|S9",  "S7|S8",    "S1|S2", "odd unknown"}
};

/*  Check for TCC detected GCache parity error -- print a message and return
 * true if we had a GCache error.
 */
static int
teton_gcache_check(long tcc_intr, long tcc_error, long tcc_parity)
{
	volatile long *taga;
	long idx, set, off;
	int sram = 4;
	long tag;

	if ((tcc_intr & INTR_MACH_CHECK) == 0 ||
	    ((tcc_error&ERROR_TYPE)>>ERROR_TYPE_SHIFT) != ERROR_GCACHE_PARITY)
		return 0;

	set = (tcc_error&ERROR_PARITY_SET)>>ERROR_PARITY_SET_SHIFT;
	off = (tcc_error&ERROR_OFFSET)>>ERROR_OFFSET_SHIFT;
	idx = (tcc_error&ERROR_INDEX)>>ERROR_INDEX_SHIFT;

	/*  Find faulting SRAM -- GCache is 16 bytes wide, one byte
	 * per SRAM, parity over 16 bits (can only narrow it down
	 * to two srams).
	 *
	 * 	dblword = ERROR_INDEX | ERROR_OFFSET >> 2
	 *	ERROR_OFFSET >> ERROR_OFFSET_SHIFT & 1 ? odd : even
	 *
	 *	SYND_DB0[0] -> halfword 3
	 *	SYND_DB0[1] -> halfword 1	
	 *	SYND_DB1[0] -> halfword 2
	 *	SYND_DB1[1] -> halfword 0
	 *
	 * Ld/st data  Parity  Logical  Physical  TCC_ERROR  TCC_PARITY
	 *    bits      bit     SRAM      SRAM      bit 5     bit # on
	 * ------------------------------------------------------------
	 * Even<63:56> Even<3>  SRAME7     S4         0          27 (3)
	 * Even<55:48> Even<3>  SRAME6     S5         0          27 (3)
	 * Even<47:40> Even<2>  SRAME5     S6         0          11 (1)
	 * Even<39:32> Even<2>  SRAME4     S12        0          11 (1)
	 * Even<31:24> Even<1>  SRAME3     S10        0          26 (2)
	 * Even<23:16> Even<1>  SRAME2     S11        0          26 (2)
	 * Even<15:8>  Even<0>  SRAME1     S15        0          10 (0)
	 * Even<7:0>   Even<0>  SRAME0     S16        0          10 (0)
	 * Odd<63:56>  Odd<3>   SRAMO7     S1         1          27 (3)
	 * Odd<55:48>  Odd<3>   SRAMO6     S2         1          27 (3)
	 * Odd<47:40>  Odd<2>   SRAMO5     S3         1          11 (1)
	 * Odd<39:32>  Odd<2>   SRAMO4     S9         1          11 (1)
	 * Odd<31:24>  Odd<1>   SRAMO3     S7         1          26 (2)
	 * Odd<23:16>  Odd<1>   SRAMO2     S8         1          26 (2)
	 * Odd<15:8>   Odd<0>   SRAMO1     S13        1          10 (0)
	 * Odd<7:0>    Odd<0>   SRAMO0     S14        1          10 (0)
	 */

	if (tcc_parity & 0x400)
		sram = 0;
	else if (tcc_parity & 0x800)
		sram = 1;
	else if (tcc_parity & 0x04000000)
		sram = 2;
	else if (tcc_parity & 0x08000000)
		sram = 3;

	printf("*** GCache parity error: index=%d set=%d offset=%d SRAMs %s\n",
	       idx, set, off, srams[off&1][sram]);

	/*  Check tags at index[set].  If line is valid and not dirty we
	 * need to invalidate it since the error is will still be valid
	 * on a writeback w/o an invalidate.  If the line is dirty,
	 * then somebody got to it before us.
	 */
	taga = (volatile long *)PHYS_TO_K1(tcc_tagaddr(
			(off&1) ? TCC_OTAG_ST : TCC_ETAG_ST, idx, set));
	tag = *taga;
	if ((tag & GCACHE_STATE_EN) && ((tag & GCACHE_DIRTY) == 0) &&
	    ((tag & GCACHE_STATE) >> GCACHE_STATE_SHIFT) == GCACHE_VALID)
		*taga = tag & ~GCACHE_STATE;

	return 1;
}

/*  Use TCC_COUNT as a basis for us_delay().  Since we know the maximum
 * frequency of the MC/SysAD is 50Mhz, and TCC_COUNT ticks once per
 * SysAD cycle, we can wait us*50 TCC_COUNT ticks to get delay.
 */
void
us_delay(uint us)
{
	volatile unsigned long *t = (unsigned long *)PHYS_TO_K1(TCC_COUNT);
	unsigned long s,v;

	us = (uint)((s=*t) + (us*50));		/* want truncation */

	if (us < s) {				/* handle wrap */
		while (*t >= s)
			;
		s = 0;
	}

	while ((v=*t) <= (unsigned long)us && (v > s))
		;
}

#endif /*  IP26 (whole file) */
