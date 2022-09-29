#ident	"saio/ml/IP20.c:  $Revision: 1.68 $"

#ifdef IP20	/* whole file */
extern int Debug;/*OLSON*/

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/z8530.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/types.h>
#include <sys/param.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/cfgtree.h>
#include <libsc.h>
#include <libsk.h>

/* Private */
static unsigned	cpu_get_membase(void);

unsigned mc_rev;

/* If we still must support the HPC1.0, define _OLDHPC so 
 * byte swapping will be done for dma.
 */
#define _OLDHPC
int needs_dma_swap;

/*
 * IP20 board registers saved during exceptions
 *
 * These are referenced by the assembly language fault handler
 *
 */
int	_cpu_parerr_save, _gio_parerr_save;
char	_liointr0_save, _liointr1_save;
int	_cpu_paraddr_save, _gio_paraddr_save;

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
	{ 0x1,	0,	"FIFO",		NULL,	NULL },
	{ 0x2,	-1,	"CENTR",	NULL,	NULL },
	{ 0x4,	-2,	"SCSI",		NULL,	NULL },
	{ 0x8,	-3,	"ENET",		NULL,	NULL },
	{ 0x20,	-5,	"DUART",	NULL,	NULL },
	{ 0x40, -6,	"GE",		NULL,	NULL },
	{ 0x80,	-7,	"VME0",		NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

/* used by INT2 diagnostic */
struct reg_desc _liointr1_desc[] = {
	/* mask	shift	name		format	vlaues */
	{ 0x2,	-1,	"GR1MODE",	NULL,	NULL },
	{ 0x8,	-3,	"VME1",		NULL,	NULL },
	{ 0x10,	-4,	"DSP",		NULL,	NULL },
	{ 0x20,	-5,	"ACFAIL",	NULL,	NULL },
	{ 0x40, -6,	"VIDEO",	NULL,	NULL },
	{ 0x80,	-7,	"VR",		NULL,	NULL },
	{ 0,	0,	NULL,		NULL,	NULL },
};

uint _gio64_arb;                /* not ro, but not always correct */

/*
 * cpu_reset - perform any board specific start up initialization
 *
 */
void
cpu_reset()
{
#define WBFLUSH wbflush

	extern int cachewrback;

	cachewrback = 1;

	mc_rev = *(volatile uint *)PHYS_TO_K1(SYSID) & SYSID_CHIP_REV_MASK;

	/* Initialize burst/delay register for graphics and i/o DMA. 33MHz */
	*(volatile uint *)PHYS_TO_K1(LB_TIME) = LB_TIME_DEFAULT;
	*(volatile uint *)PHYS_TO_K1(CPU_TIME) = CPU_TIME_DEFAULT;

	/* mask out all interrupts */
	*(volatile u_char *)PHYS_TO_K1(LIO_0_MASK_ADDR) = 0x0;
	*(volatile u_char *)PHYS_TO_K1(LIO_1_MASK_ADDR) = 0x0;
	WBFLUSH();

	_gio64_arb = GIO64_ARB_1_GIO;		/* match boot up state */

	 *(volatile uint *)PHYS_TO_K1(CPUCTRL0) |= CPUCTRL0_R4K_CHK_PAR_N;
	 if (ip20_board_rev() >= 0x2) {
	 	*(volatile uint *)PHYS_TO_K1(CPUCTRL0) |=
			CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
	 } else {
		/* disable parity checking due to a DMUX bug */
	 	*(volatile uint *)PHYS_TO_K1(CPUCTRL0) &=
			~(CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR);
	}
	WBFLUSH();

	hpc_set_endianness();

	scsiinit();
}

/*
 * led code
 */
void
ip20_set_sickled(value)
{
	ip20_set_cpuled(value?1:2);
}

/*
 * set the main cpu led to the given color
 *	0 = off
 *	1 = amber
 *	2 = green
 */
void
ip20_set_cpuled(int color)
{
	unsigned char val;
	volatile unsigned char *ducntrl;
	volatile unsigned char *ledcntrl;

	ledcntrl = (unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	ducntrl = (unsigned char *)PHYS_TO_K1(DUART0A_CNTRL);

	switch (color) {
	case 0:
	    *ledcntrl |= CONSOLE_LED;
	    val = RD_CNTRL(ducntrl, WR5) & ~WR5_RTS;
	    WR_CNTRL(ducntrl, WR5, val);
	    break;
	case 1:
	    *ledcntrl |= CONSOLE_LED;
	    val = RD_CNTRL(ducntrl, WR5) | WR5_RTS;
	    WR_CNTRL(ducntrl, WR5, val);
	    break;
	case 2:
	    *ledcntrl &= ~CONSOLE_LED;
	    val = RD_CNTRL(ducntrl, WR5) & ~WR5_RTS;
	    WR_CNTRL(ducntrl, WR5, val);
	    break;
	}
}

/* 
 *	get memory configuration word for a bank
 *              - bank better be between 0 and 3 inclusive
 */
static unsigned long
get_mem_conf (int bank)
{
    unsigned long memconfig;

    memconfig = (bank <= 1) ? 
	*(volatile unsigned long *)PHYS_TO_K1(MEMCFG0) :
	*(volatile unsigned long *)PHYS_TO_K1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    memconfig &= 0xffff;
    return memconfig;
}       /* get_mem_conf */

/*
 * banksz - determine the size of a memory bank
 */
static uint
banksz (int bank)
{
    unsigned long memconfig = get_mem_conf (bank);

    /* weed out bad module sizes */
    if (!(memconfig & MEMCFG_VLD))
        return 0;
    else
        return ((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << 14;
}       /* banksz */

/*
 * ip20_addr_to_bank - find bank in which the given addr belongs to
 */
int
ip20_addr_to_bank(uint addr)
{
	uint base;
	int i;
	uint memconfig;
	uint size;

	for (i = 0; i < 3; i++) {
		if (!(size = banksz(i)))
			continue;
		memconfig = get_mem_conf(i);
		base = (memconfig & MEMCFG_ADDR_MASK) << 22;
		if (addr >= base && addr < (base + size))
			return i;
	}

	return -1;
}

/*
    IP20 specific hardware inventory routine
*/

/* 
 * return board revision level (0-7)
 */
int
ip20_board_rev ()
{
    register unsigned int rev_reg = 
	    *(volatile unsigned int *)PHYS_TO_K1(BOARD_REV_ADDR);

    return (~rev_reg & REV_MASK) >> REV_SHIFT;
}

/* board revs of IP20 are: 1 & 2 were early blackjack spins
 * 10 decimal == 0x0a == 1010binary == VME IP 20 board (V50)
 * V50 board lacks audio hardware
 * The upper bit used to change the standard (rev 2) board
 * to the V50 designation (i.e the 0x8 bit) used to be known
 * internally as the 'Hollywood processor 1 bit' or hp1 bit
 */
int
is_v50()
{
    register unsigned int rev_reg = 
	    *(volatile unsigned int *)PHYS_TO_K1(BOARD_REV_ADDR);

    return ip20_board_rev() == 2 && (~rev_reg & REV_HP1 ? 1 : 0);
}

#ifdef DEBUG
/*
 * cpu_showconfig - print hinv-style info about cpu board dependent stuff
 */
void
cpu_showconfig(void)
{
    printf("                CPU board: Revision %d\n", ip20_board_rev());
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
#define SYSBOARDID	"SGI-IP20"

static COMPONENT IP20tmpl = {
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

	root = AddChild(NULL,&IP20tmpl,(void *)NULL);
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
	int r4400 = ((get_prid()&0xf0)>>4) >= 4;
	COMPONENT *c, *tmp;
	union key_u key;

	c = AddChild(root,&cputmpl,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("cpu");
	c->Key = cpuid();
	if (r4400) c->Identifier[7] = '4';

	tmp = AddChild(c,&fputmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("fpu");
	tmp->Key = cpuid();
	if (r4400) tmp->Identifier[7] = '4';

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("icache");
	key.cache.c_bsize = 1;
	key.cache.c_lsize = log2(_icache_linesize);	/* R4000 */
	key.cache.c_size = log2(_icache_size >> 12);
	tmp->Key = key.FullKey;

	tmp = AddChild(c,&cachetmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("dcache");
	key.cache.c_bsize = 1;
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

	tmp = AddChild(c,&memtmpl,(void *)NULL);
	if (tmp == (COMPONENT *)NULL) cpu_acpanic("main memory ");
	tmp->Key = cpu_get_memsize() >> ARCS_BPPSHIFT;	/* number of pages */
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

char *simm_map[3][4] = {
	{"s1c", "s3c", "s2c", "s4c"},
	{"s1b", "s3b", "s2b", "s4b"},
	{"s1a", "s3a", "s2a", "s4a"},
};

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
	}

	if ( _cpu_parerr_save & CPU_ERRMASK ) {
	    printf("CPU parity error register: %R\n", _cpu_parerr_save, _cpu_parerr_desc );
	    if (_cpu_parerr_save & CPU_ERR_STAT_ADDR)
		printf("CPU bus error: address: 0x%x\n", _cpu_paraddr_save );
	    else {
		printf("CPU parity error: address: 0x%x", _cpu_paraddr_save );

	    	/* print out bad SIMM location on read parity error */
	    	if (_cpu_parerr_save & (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)
		    == (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)) {

		    int bank = ip20_addr_to_bank(_cpu_paraddr_save);
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

		    if (simms) {
		    	printf(" ( ");
		    	if (simms & 0x1)
		    	    printf("%s ", simm_map[bank][0]);
		    	if (simms & 0x2)
		    	    printf("%s ", simm_map[bank][1]);
		    	if (simms & 0x4)
		    	    printf("%s ", simm_map[bank][2]);
		    	if (simms & 0x8)
		    	    printf("%s ", simm_map[bank][3]);
		    	printf(")");
		    }
	        }

		printf("\n");
	    }
		
	    *(volatile int *)PHYS_TO_K1( CPU_ERR_STAT ) = 0;
	}

	if ( _gio_parerr_save & GIO_ERRMASK ) {
	    printf("GIO parity error register: %R\n", _gio_parerr_save, _gio_parerr_desc );
	    if (_gio_parerr_save & GIO_ERR_STAT_RD) {
		int bank = ip20_addr_to_bank(_gio_paraddr_save);
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

		if (simms) {
		    printf(" ( ");
		    if (simms & 0x1)
		    	printf("%s ", simm_map[bank][0]);
		    if (simms & 0x2)
		    	printf("%s ", simm_map[bank][1]);
		    if (simms & 0x4)
		    	printf("%s ", simm_map[bank][2]);
		    if (simms & 0x8)
		    	printf("%s ", simm_map[bank][3]);
		    printf(")");
		}
		printf("\n");
	    } else if (_gio_parerr_save & (GIO_ERR_STAT_WR|
		 GIO_ERR_STAT_PIO_RD|GIO_ERR_STAT_PIO_WR))
		printf("GIO parity error: address: 0x%x\n", _gio_paraddr_save );
	    else
		printf("GIO bus error: address: 0x%x\n", _gio_paraddr_save );
		
	    *(volatile int *)PHYS_TO_K1( GIO_ERR_STAT ) = 0;
	}

	WBFLUSH();
}

void
cpu_soft_powerdown(void)
{
	return;
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
	z8530cons_errputc(c);
}

void
cpu_mem_init(void)
{
    /* add all of main memory with the exception of two pages that
     * will be also mapped to physical 0
     */
#define NPHYS0_PAGES	2
    /* limit the top of memory for memory descriptors to be 128MB to work around 
     * an HPC bug that won't allow dma into mem above 128MB on IP20.
     */
    md_add (cpu_get_membase()+arcs_ptob(NPHYS0_PAGES),
	    arcs_btop(_min(cpu_get_memsize(), 0x08000000)) - NPHYS0_PAGES,
	    FreeMemory);
    md_add (0, NPHYS0_PAGES, FreeMemory);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP20 cannot save configuration to NVRAM */
LONG
cpu_savecfg(void)
{
	return(ENOSPC);
}

char *
cpu_get_mouse(void)
{
	return("serial(3)pointer()");
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
	u_int	bad_cache_addr =
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
	return;
}

/* check if address is inside a "protected" area */
is_protected_adrs(unsigned long low, unsigned long high)
{
	return 0;	/* none */
}
#endif /*  IP20 (whole file) */
