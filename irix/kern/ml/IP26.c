 /**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * IP26 specific functions
 */
#ident "$Revision: 1.168 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/IP26.h>
#include <sys/IP22nvram.h>		/* share NVRAM layout for compat */
#include <sys/buf.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/ds1286.h>
#include <sys/fpu.h>
#include <sys/i8254clock.h>
#include <sys/invent.h>
#include <sys/kabi.h>
#include <sys/kopt.h>
#include <sys/loaddrs.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/ksignal.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/scsi.h>
#include <sys/wd93.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/var.h>
#include <sys/vdma.h>
#include <sys/vmereg.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/eisa.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#include <sys/hal2.h>
#include <sys/arcs/kerncb.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/rtmon.h>
#include <sys/atomic_ops.h>
#include <ksys/cacheops.h>
#include <ksys/hwg.h>

int ithreads_enabled = 0;
static int is_thd(int);
static void lclvec_thread_setup(int, int, lclvec_t *);

extern int clean_shutdown_time; 	/* mtune/kernel */

typedef caddr_t dmavaddr_t;
typedef paddr_t dmamaddr_t;

#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
extern int picache_size;
extern int pdcache_size;

extern int ip26_allow_ucmem;	/* systune ecc boot time on/off switch */

uint_t vmeadapters = 0;

#define RPSS_DIV		2
#define RPSS_DIV_CONST		(RPSS_DIV-1)

/* Per CPU (only one in our case) data structures, indexed by cpu number.
 */
#define MAXCPU	1
int maxcpus = MAXCPU;

pdaindr_t pdaindr[MAXCPU];		/* processor-dependent data area */

long _tcc_intr_save, _tcc_error_save, _tcc_parity_save, _tcc_be_addr_save;
long tcc_gcache_normal = 0, tcc_gcache_slow = 0;

unsigned int mc_rev_level;

/* Local ECC chip routines.  (Some are in IP26asm.s, too.)
 */
static void ip26_clear_ecc(void);
static int ip26_ecc_mbit_intr(struct eframe_s *ep, int flags);
static int ip26_ecc_uc_write_intr(struct eframe_s *ep, int flags);
#define ECC_INTR	0x00			/* from IP8 -- dedicated ECC */
#define	ECC_ISBERR	0x01			/* from IP7 -- parity error */
#define ECC_ISGIO	0x02			/* from IP7 -- gio parity */
#define ECC_NOFAULT	0x04			/* egun hook */
int ip26_isecc(void);

/* Some machine specific local prototypes.
 */
static int get_dayof_week(time_t year_secs);
void set_work1(unsigned int), set_config(k_machreg_t);
k_machreg_t get_config(void);
void set_autopoweron(void);
void resettodr(void);
void set_leds(int);
int get_cpu_irr(void);

/* Low-level cache op prototypes */
void tfp_flush_icache(void *index, unsigned int lines);
void __cache_wb(void *, int);

static void cleartccerrors(long arg);

/* Global flag to reset gfx boards */
int GRReset = 0;

/* software copy of write-only registers */
uint _hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
uint _hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
	ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;
static int memconfig_0, memconfig_1;

/*
 * Processor interrupt table
 */
extern	void ip26_ecc_intr(struct eframe_s *);
extern	void buserror_intr(struct eframe_s *);
extern	void clock(struct eframe_s *, kthread_t *);
static	void lcl1_intr(struct eframe_s *);
static	void lcl0_intr(struct eframe_s *);
extern	void timein(void);
static	void tfpcount_intr(struct eframe_s *);
static	void tfpgcache_intr(struct eframe_s *);
static	void load_nvram_tab(void);
static	void power_bell(void);
static	void flash_led(int);

typedef void (*intvec_func_t)(struct eframe_s *, kthread_t *);

struct intvec_s {
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		bit;		/* corresponding bit in sr */
};

static struct intvec_s c0vec_tbl[] = {
	0,		0,			0,		/* (1 based) */
	(intvec_func_t)
	timein,		SR_IMASK5|SR_IE,	SR_IBIT1>>8,	/* softint 1 */
	(intvec_func_t)
	0,		SR_IMASK5|SR_IE,	SR_IBIT2>>8,	/* softint 2 */
	(intvec_func_t)
	lcl0_intr,	SR_IMASK5|SR_IE,	SR_IBIT3>>8,	/* hardint 3 */
	(intvec_func_t)
	lcl1_intr,	SR_IMASK5|SR_IE,	SR_IBIT4>>8,	/* hardint 4 */
	clock,		SR_IMASK5|SR_IE,	SR_IBIT5>>8,	/* hardint 5 */
	(intvec_func_t)
	ackkgclock,	SR_IMASK6|SR_IE,	SR_IBIT6>>8,	/* hardint 6 */
	(intvec_func_t)
	buserror_intr,	SR_IMASK7|SR_IE,	SR_IBIT7>>8,	/* hardint 7 */
	(intvec_func_t)
	ip26_ecc_intr,	SR_IMASK8|SR_IE,	SR_IBIT8>>8,	/* hardint 8 */
	(intvec_func_t)
	tfpgcache_intr,	SR_IMASK9|SR_IE,	SR_IBIT9>>8,	/* hardint 9 */
	(intvec_func_t)
	tfpgcache_intr,	SR_IMASK10|SR_IE,	SR_IBIT10>>8,	/* hardint 10*/
	(intvec_func_t)
	tfpcount_intr,	SR_IMASK11|SR_IE,	SR_IBIT11>>8,	/* hardint 11*/
};

/*  Hook to control interrupt 8 -- the ECC interrupt.  On some baseboards
 * (early ECC IP26 baseboards) can get rampant ECC errors with the 33Mhz
 * GIO bus.  For these boards we always keep SR_IBIT8 == 0 to mask the
 * (potentially) false error.
 * set so bit can be flipped with an xor), so they do * not flood the CPU.
 *
 *  This flag is also maintained in C0_WORK1.
 *
 *  Also, keep SR_BIT11 (TFP count) interrupt off so we can use it as
 * a 32-bit timestamp.
 */
unsigned long ip26_sr_mask = ~(SR_IBIT8|SR_IBIT11);

/*
 * Local interrupt table
 *
 * The isr field is the address of the interrupt service routine.  It may
 * be changed dynamically by use of the setlclvector() call.
 *
 * The 'bit' value is the bit in the local interrupt status register that
 * causes the interrupt.  It is in the table to save a few instructions
 * in lcl_intr().
 */
static	lcl_intr_func_t lcl_stray;
static  lcl_intr_func_t hpcdmastray;
static	lcl_intr_func_t hpcdmastray_scsi;
static  lcl_intr_func_t straytcc;

static	lcl_intr_func_t gio0_intr;
static	lcl_intr_func_t gio1_intr;
static	lcl_intr_func_t gio2_intr;
static 	lcl_intr_func_t lcl2_intr;
static	lcl_intr_func_t powerintr, powerfail;
#if _USE_LCL3
static	lcl_intr_func_t	lcl3_intr;
#endif

lcl_intr_func_t hpcdma_intr;

/* These arrays must be left in order, and contiguous to each other,
 * since we do some address calculations on them.
 */
lclvec_t lcl0vec_tbl[] = {
	{ 0,		0 } ,		/* table is 1 based */
	{ lcl_stray,	1, 0x01 },
	{ lcl_stray,	2, 0x02 },
	{ lcl_stray,	3, 0x04 },
	{ lcl_stray,	4, 0x08 },
	{ lcl_stray,	5, 0x10 },
	{ lcl_stray,	6, 0x20 },
	{ lcl_stray,	7, 0x40 },
	{ lcl_stray,	8, 0x80 },
	{ straytcc,	9, 0x100 },	/* INTR_FIFO_HW -- on TCC, not INT2 */
	{ straytcc,	10,0x200 }	/* INTR_FIFO_LW -- on TCC, not INT2 */
};


lclvec_t lcl1vec_tbl[] = {
	{ 0,		0 },		/* table is 1 based */
	{ lcl_stray,	8+1, 0x01 },
	{ lcl_stray,	8+2, 0x02 },
	{ lcl_stray,	8+3, 0x04 },
	{ lcl_stray,	8+4, 0x08 },
	{ lcl_stray,	8+5, 0x10 },
	{ lcl_stray,	8+6, 0x20 },
	{ lcl_stray,	8+7, 0x40 },
	{ lcl_stray,	8+8, 0x80 }
};

lclvec_t lcl2vec_tbl[] = {
	{ 0,		0 },		/* table is 1 based */
	{ lcl_stray,	16+1, 0x01 },
	{ lcl_stray,	16+2, 0x02 },
	{ lcl_stray,	16+3, 0x04 },
	{ lcl_stray,	16+4, 0x08 },
	{ lcl_stray,	16+5, 0x10 },
	{ lcl_stray,	16+6, 0x20 },
	{ lcl_stray,	16+7, 0x40 },
	{ lcl_stray,	16+8, 0x80 }
};

static lclvec_t hpcdmavec_tbl[] = {
	{ 0,			0 },		/* table is 1 based */
	{ hpcdmastray,		1, 0x01 },
	{ hpcdmastray,		2, 0x02 },
	{ hpcdmastray,		3, 0x04 },
	{ hpcdmastray,		4, 0x08 },
	{ hpcdmastray,		5, 0x10 },
	{ hpcdmastray,		6, 0x20 },
	{ hpcdmastray,		7, 0x40 },
	{ hpcdmastray,		8, 0x80 },
	{ hpcdmastray_scsi,	9, 0x100 },
	{ hpcdmastray_scsi,	10,0x200 }
};
#define HPCDMAVECTBL_SLOTS (sizeof(hpcdmavec_tbl) / sizeof(lclvec_t) - 1)

#define GIOLEVELS	3 /* GIO 0 (FIFO), GIO 1 (GE), GIO 2 (VR) */
#define GIOSLOTS	3 /* slot 0, missing, gfx */

struct giovec_s {
	lcl_intr_func_t *isr;
	__psint_t	arg;
};

static struct giovec_s giovec_tbl[GIOSLOTS][GIOLEVELS];

scuzzy_t scuzzy[] = {
	{
		(u_char *)PHYS_TO_K1(SCSI0A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI0D_ADDR),
		(uint *)PHYS_TO_K1(SCSI0_CTRL_ADDR),
		(uint *)PHYS_TO_K1(SCSI0_BC_ADDR),
		(uint *)PHYS_TO_K1(SCSI0_CBP_ADDR),
		(uint *)PHYS_TO_K1(SCSI0_NBDP_ADDR),
		(uint *)PHYS_TO_K1(HPC3_SCSI_DMACFG0),
		(uint *)PHYS_TO_K1(HPC3_SCSI_PIOCFG0),
		D_PROMSYNC|D_MAPRETAIN,
		0x80, VECTOR_SCSI
	},
	{
		(u_char *)PHYS_TO_K1(SCSI1A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI1D_ADDR),
		(uint *)PHYS_TO_K1(SCSI1_CTRL_ADDR),
		(uint *)PHYS_TO_K1(SCSI1_BC_ADDR),
		(uint *)PHYS_TO_K1(SCSI1_CBP_ADDR),
		(uint *)PHYS_TO_K1(SCSI1_NBDP_ADDR),
		(uint *)PHYS_TO_K1(HPC3_SCSI_DMACFG1),
		(uint *)PHYS_TO_K1(HPC3_SCSI_PIOCFG1),
		D_PROMSYNC|D_MAPRETAIN,
		0x80, VECTOR_SCSI1
	}
};

/*	used to sanity check adap values passed in, etc */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))

/* table of probeable kmem addresses */
#define PROBE_SPACE(X)	PHYS_TO_K1(X)
struct kmem_ioaddr kmem_ioaddr[] = {
	{ PROBE_SPACE(LIO_ADDR), (LIO_GFX_SIZE + LIO_GIO_SIZE) },
	{ PROBE_SPACE(HPC3_SYS_ID), sizeof (int) },
	{ PROBE_SPACE(HAL2_ADDR), (HAL2_REV - HAL2_ADDR + sizeof(int))},
	{ 0, 0 },
};

short cputype = 26;	/* integer value for cpu (i.e. xx in IPxx) */

static uint sys_id;	/* initialized by init_sysid */
long _tbus_fast_ufos;	/* count of tbus fastpath errors (sync hack) */
long _tbus_slow_ufos;	/* count of tbus slowpath errors (sync hang) */

/* I/O and graphics DMA burst & delay periods;  The value is
 * register_value * GIO clock period
 */
/*
 * due to a bug in the MC, reading of GIO64_ARB/LB_TIME/CPU_TIME registers
 * sometimes returns corrupted values.  therefore, writing to these
 * registers should always be done through the software copy
 */
#define	GIO_CYCLE	30		/* assume 33 MHZ clock for now */
unsigned short dma_burst = 8000 / GIO_CYCLE;	/* 8 usecs */
unsigned short dma_delay = 700 / GIO_CYCLE;	/* .7 usec */
unsigned short i_dma_burst, i_dma_delay;	/* intr handler versions */

/* TFP requires final GIO config */
#define GIO64_ARB_004 ( GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |\
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |\
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST )
uint gio64_arb_reg;

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))
char eaddr[6];

unsigned char *
etoh (char *enet)
{
	static unsigned char dig[6];
	unsigned char *cp;
	int i;

	for ( i = 0, cp = (unsigned char *)enet; *cp; ) {
		if ( *cp == ':' ) {
			cp++;
			continue;
		}
		else if ( !ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1)) ) {
			return NULL;
		}
		else {
			if ( i >= 6 )
				return NULL;
			dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
			cp += 2;
		}
	}
    
	return i == 6 ? dig : NULL;
}

/* initialize the serial number.  Called from mlreset.
 */
void
init_sysid(void)
{
	char *cp; 

	cp = (char *)arcs_getenv("eaddr");
	bcopy(etoh(cp), eaddr, 6);

	sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
		 (eaddr[4] << 8) | eaddr[5];

	sprintf(arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
	    (eaddr[0]>>4)&0xf, eaddr[0]&0xf,
	    (eaddr[1]>>4)&0xf, eaddr[1]&0xf,
	    (eaddr[2]>>4)&0xf, eaddr[2]&0xf,
	    (__psunsigned_t)(eaddr[3]>>4)&0xf, (__psunsigned_t)eaddr[3]&0xf,
	    (__psunsigned_t)(eaddr[4]>>4)&0xf, (__psunsigned_t)eaddr[4]&0xf,
	    (__psunsigned_t)(eaddr[5]>>4)&0xf, (__psunsigned_t)eaddr[5]&0xf);
}

/*  Pass back the serial number associated with the system ID prom.
 */
int
getsysid(char *hostident)
{
	/*
	 * serial number is only 4 bytes long on IP26.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */

	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}

/* For IP22 src code compatability.
 */
int is_fullhouse(void) { return(1); }
int is_ioc1(void) { return(0); }

/* Get memory configuration word for a bank where bank is between 0 and
 * MAX_MEM_BANKS-1 inclusive.
 */
static uint
get_mem_conf(int bank)
{
	uint memconfig;

	memconfig = (bank <= 1) ? *(volatile uint *)PHYS_TO_K1(MEMCFG0)
				: *(volatile uint *)PHYS_TO_K1(MEMCFG1) ;
	memconfig >>= 16 * (~bank & 1);		/* shift banks 0, 2 */
	return memconfig & 0xffff;
}

/* Determine first address of a memory bank.
 */
static int
bank_addrlo(int bank)
{
    return (int)((get_mem_conf(bank) & MEMCFG_ADDR_MASK) << 
	(mc_rev_level >= 5 ? 24 : 22));
}

/* Determine the size of a memory bank.
 */
static int
banksz(int bank)
{
	uint memconfig = get_mem_conf(bank);

	/* weed out bad module sizes */
	if (!memconfig)
		return 0;

        return (int)(((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << 
		(mc_rev_level >= 5 ? 16 : 14));
}

/*  Determine which physical memory bank contains an address.  Returns 0-3
 * or -1 for an invalid address.
 */
int
addr_to_bank(uint addr)
{
	int bank;
	uint size;

	for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
		if (!(size = banksz(bank))) /* bad memory module if size == 0 */
			continue;

		if (addr >= bank_addrlo(bank) && addr < bank_addrlo(bank)+size)
			return(bank);
	}

	return(-1);        /* address not found */
}

static pgno_t memsize = 0;	/* total ram in clicks */
static pgno_t seg0_maxmem;
static pgno_t seg1_maxmem;

/*  Size main memory.  Returns # physical pages of memory.
 */
/* ARGSUSED */
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
    extern pfn_t low_maxclick;
    int bank;
    int bank_size;

    if (!memsize) {
	for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
	    bank_size = banksz(bank);
	    memsize += bank_size;		/* sum up bank sizes */
	    if (bank_size) {
		if (bank_addrlo(bank) >= SEG1_BASE)
			seg1_maxmem += bank_size;
		else
			seg0_maxmem += bank_size;
	    }
	}
	memsize = btoct(memsize);			/* get pages */
	seg0_maxmem = btoct(seg0_maxmem);
	seg1_maxmem = btoct(seg1_maxmem);

	if (maxpmem) {
	    memsize = MIN(memsize, maxpmem);
	    seg0_maxmem = MIN(memsize, seg0_maxmem);
	    seg1_maxmem = MIN(memsize - seg0_maxmem, seg1_maxmem);
	}

	low_maxclick = (pfn_t)(btoct(SEG0_BASE) + seg0_maxmem);
    }

    return memsize;
}

/* Used in some no uncached memory write WARs */
int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return((pfn >= min_pfn &&
		pfn < (min_pfn + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
	        pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}

/*  Teton only currently supports a 2MB secondary cache (in theory could
 * double to 4MB), so just hardcode the cache size.
 */
long
size_2nd_cache(void)
{
	return(0x200000);
}

static void (*cache_funcs[2][4])(void *, int) = {
	{ 0, __dcache_inval, __dcache_wb, __dcache_wb_inval },
	{ 0, __cache_wb_inval,  __cache_wb,  __cache_wb_inval }
};

/*  On tfp, the ICache isn't a strict subset of the secondary (GCache) cache
 * like the R4X00, so we have to flush it seperately.  It's index invalidated
 * first (there are no hit ops on the icache), then we do the requested
 * op on the GCache.
 */
void
clean_icache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	__psunsigned_t iaddr;
	unsigned int ilen;

	if (len == 0) return;			/* skip nop requests */

	ASSERT(flags & CACH_OPMASK);

	/*  Index invalidate the ICache -- convert addr/len to index/nlines.
	 */
	iaddr = ((__psunsigned_t)addr) & ~(__psunsigned_t)ICACHE_LINEMASK;
	ilen = min(len, picache_size) + (__psunsigned_t)addr - iaddr;
	tfp_flush_icache((void *)(iaddr & (ICACHE_SIZE-1)),
			 (ilen + ICACHE_LINEMASK) / ICACHE_LINESIZE);

	clean_dcache(addr,len,pfn,flags|CACH_WBACK);
}

/*  On teton (not tfp in general) the DCache is flushed as a side effect
 * of flushing the GCache.
 *
 *  We don't have to worry about VCIs since the GCache flush uses physical
 * addresses unlike the R4X00.
 *
 *  Note that this routine is called with either a K0 address and
 * an arbitrary length OR higher level routines will break request
 * into (at most) page size requests with non-K0 addresses.
 */
void
clean_dcache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	if (len == 0) return;			/* skip nop requests */

	ASSERT(flags & CACH_OPMASK);

	/* In K0 or contained on a single page */
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*  If we don't have a K0 address, we need to get the physical
	 * address to flush from the pfn.  The teton cache routines will
	 * take a physical address and don't need to convert it to a
	 * virtual address, and don't need to worry about VCIs.
	 *
	 *  Also turn off CACH_NOADDR (bp_dcache_wbinval() will turn it on)
	 * since we known the pfn to flush, and wholesale cache flushes
	 * need to pass a k0 addr.  This avoids using the slower index
	 * based ops (4 lw per index), and keeps the 3 remaining sets.
	 */
	if (!IS_KSEG0(addr)) {
		ASSERT(pfn);
		flags &= ~CACH_NOADDR;
		addr = (char *)(ctob(pfn) | poff(addr));
	}

	cache_funcs[(flags&CACH_NOADDR) == CACH_NOADDR]
		   [flags&(CACH_INVAL|CACH_WBACK)](addr,len);

#if CACH_INVAL != 1 || CACH_WBACK != 2
#error CACH_INVAL or CACH_WBACK defines have changed!
#endif
}

/*  Flush both the I and DCache.
 */
void
flush_cache(void)
{
	tfp_flush_icache(0, ICACHE_SIZE / ICACHE_LINESIZE);
	__cache_wb_inval(0, private.p_scachesize);
}

/*  Returns pfn of first page of ram.
 */
pfn_t
pmem_getfirstclick(void)
{
	return btoc(SEG0_BASE);
}

/*
 * getfirstfree - returns pfn of first page of unused ram.  This allows
 *		  prom code on some platforms to put things in low memory
 *		  that the kernel must skip over when its starting up.
 *		  This isn't used on IP26.
 */

/*ARGSUSED*/
pfn_t
node_getfirstfree(cnodeid_t node)
{
	ASSERT(node == 0);
	return btoc(SEG0_BASE);
}

/*  Returns pfn of last page of ram.
 */
/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t cnode)
{
	ASSERT(cnode == 0);
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);

	if (seg1_maxmem)
		return btoc(SEG1_BASE) + seg1_maxmem - 1;

	return btoc(SEG0_BASE) + memsize - 1;
}

/*  Mark the hole in memory between seg 0 and seg 1.  Return count of
 * non-existent pages.
 */
/*ARGSUSED3*/
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	register pgno_t	hole_size;
	register pgno_t npgs;

	if (!seg1_maxmem)
		return 0;

	pfd += btoc(SEG0_BASE) + seg0_maxmem - first;
	hole_size = btoc(SEG1_BASE) - (btoc(SEG0_BASE) + seg0_maxmem); 

	for (npgs = hole_size; npgs; pfd++, npgs--)
		PG_MARKHOLE(pfd);

	return hole_size;
}

int
is_memory(long phys)
{
	return	(phys >= SEG0_BASE && phys < (SEG0_BASE+SEG0_SIZE)) ||
		(phys >= SEG1_BASE && phys < (SEG1_BASE+SEG1_SIZE));
}

/*  Sanity check mmmap_addrs[] array for no uncached memory accesses.  This
 * is a bit of paranoia, but we'll play it safe.
 */
void
check_mmmap(void)
{
	struct mmmap {
		unsigned long m_size;
		unsigned long m_addr;
		int m_prot;
	};
	extern struct mmmap mmmap_addrs[];	/* defined in master.d/mem */
	struct mmmap *m = mmmap_addrs;
	long phys;

	if (ip26_allow_ucmem) return;		/* does not matter */

	while (m->m_size) {
		if (IS_KSEG1(m->m_addr)) {
			phys = K1_TO_PHYS(m->m_addr);
			if (is_memory(phys))
				cmn_err(CE_WARN,
	"Detected uncached memory address 0x%x in\n"
	"\t/var/sysgen/master.d/mem.  Writing to this region will\n"
	"\tcause the system to crash unless the ip26_allow_ucmem\n"
	"\tsystune flag is set.\n",m->m_addr);
		}
		m++;
	}
}

void
lclvec_init(void)
{
	int i;
	for (i=0; i<8; i++) {
		lcl0vec_tbl[i+1].lcl_flags=is_thd( 0+i) ? THD_ISTHREAD|0 : 0;
		lcl1vec_tbl[i+1].lcl_flags=is_thd( 8+i) ? THD_ISTHREAD|1 : 1;
		lcl2vec_tbl[i+1].lcl_flags=is_thd(16+i) ? THD_ISTHREAD|2 : 2;
	}
	lcl0vec_tbl[9].lcl_flags=0;	/* TCC intr -not threaded. */
	lcl0vec_tbl[10].lcl_flags=0;	/* TCC intr -not threaded. */
}

/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup.
 */
/* ARGSUSED */
void
mlreset(int junk)
{
	volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
	extern int valid_dcache_reasons, valid_icache_reasons;
	extern int softpowerup_ok, cachewrback;
	extern int reboot_on_panic;
	unsigned int cpuctrl1tmp;

	/*  Just to be safe, clear the parity error register.
	 * This is done because if any errors are left in the register,
	 * when a DBE occurs we will think it is due to a parity error.
	 */
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	flushbus();

	*(volatile uint *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
	*(volatile uint *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
	flushbus();

	/* HPC3_EXT_IO_ADDR is 16 bits wide
	 */
	*(volatile int *)PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6)) |= HPC3_CFGPIO_DS_16;

	/*  Set RPSS divider for increment by 2, the fastest rate at which
	 * the register works  (also see comment for query_cyclecntr).
	 */
	*(volatile uint *)PHYS_TO_K1(RPSS_DIVIDER) = (1<<8)|RPSS_DIV_CONST;

	/*  Enable all parity checking except SYSAD.  We turn this on after
	 * we probe for devices on the GIO bus as we can get this error on
	 * empty GIO slots by getting floating data.
	 */
	*cpuctrl0 |= CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR|
			CPUCTRL0_R4K_CHK_PAR_N;
	flushbus();

	/*  Rev A + B MCs are unsupported in IP26, but global still needed
	 * in gr2 code.
	 */
	mc_rev_level = *(volatile uint *)PHYS_TO_K1(SYSID) &
		SYSID_CHIP_REV_MASK;

	/*  Set the MC write buffer depth.  The effective depth is roughly 17
	 * minus this value (ie 6).
	 *
	 * NOTE: Don't make the write buffer deeper unless you understand
	 *	 what this does to GFXDELAY.
	 */
	cpuctrl1tmp = *(volatile uint *)PHYS_TO_K1(CPUCTRL1) & 0xfffffff0;
	*(volatile uint *)PHYS_TO_K1(CPUCTRL1) = (cpuctrl1tmp|0xd);
	flushbus();

	gio64_arb_reg = GIO64_ARB_004;		/* can support for old board */
	*(volatile uint *)PHYS_TO_K1(GIO64_ARB) = gio64_arb_reg;
	flushbus();

	/*  If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
	    kdebug = 0;
	
	/* Set initial interrupt vectors.
	 */
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = 0;
	*K1_LIO_2_MASK_ADDR = 0;
	*K1_LIO_3_MASK_ADDR = 0;

	lclvec_init();

	setlclvector(VECTOR_ACFAIL, powerfail, 0);
	setlclvector(VECTOR_POWER, powerintr, 0);
	setlclvector(VECTOR_LCL2, lcl2_intr, 0);
#if _USE_LCL3
	setlclvector(VECTOR_LCL3, lcl3_intr, 0);	/* currently unused */
#endif

	/* set interrupt DMA burst and delay values */
	i_dma_burst = dma_delay;
	i_dma_delay = dma_burst;

	init_sysid();
	load_nvram_tab();			/* get a copy of the nvram */

	cachewrback = 1;

	/*  Cachecolormask is set from the bits in a vaddr/paddr
	 * which the cpu looks at to determine if a VCI should
	 * be raised.
	 */
	cachecolormask = (pdcache_size / NBPP) - 1;
#ifdef _VCE_AVOIDANCE
	vce_avoidance = 1;
#else
#if NBPP != IO_NBPP
	/*  With LARGE pages, and _VCE_AVOIDANCE off, we don't have to flush
	 * for VCE's since the 16KB DCache cannot hold multiple virtual
	 * addresses that map to seperate physical pages.
	 */
	valid_dcache_reasons &= ~CACH_AVOID_VCES;
	valid_icache_reasons &= ~CACH_AVOID_VCES;
#endif
#endif

	softpowerup_ok = 1;		/* for syssgi() */

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	*(volatile char *)PHYS_TO_K1(HPC3_MISC_ADDR) &= ~HPC3_DES_ENDIAN;

	VdmaInit();
	eisa_init();

	memconfig_0 = *(volatile uint *)PHYS_TO_K1(MEMCFG0);
	memconfig_1 = *(volatile uint *)PHYS_TO_K1(MEMCFG1);

	/*  Enable prefetch on all IU revisions.  There is a missing interlock
	 * on the index register of the register index prefetch (ie A of
	 * prefetch A(B)).  This requires two super-scalar cycles between
	 * the calculation of A and the prefetch instruction.  It was
	 * thought we could prefetch from the wrong address, and use it
	 * at the intended address, but we really just get the wrong address,
	 * which is harmless.  This allows us to always enable prefetch.
	 */
	*(volatile long *)PHYS_TO_K1(TCC_PREFETCH) = PRE_DEFAULT;

	/*  Enable the TCC fifo early.  Graphics should do this, but old
	 * proms do not initialize this, which prevents the textport
	 * from comming up on these machines.
	 */
	settccfifos(0,0);

	/*  Calculate and initialize TCC_GCACHE to optimal values.  When
	 * booting from the prom, we are running in rmw slow mode.  Also
	 * set-up slow mode values for switching code.
	 *
	 *  If global ip26_allow_ucmem is set by systune, then always
	 * stay in slow mode for driver compatibility.
	 */
	tcc_gcache_normal = tune_tcc_gcache();
	tcc_gcache_slow = (tcc_gcache_normal & ~GCACHE_REL_DELAY) |
		GCACHE_RR_FULL | GCACHE_WB_RESTART;

	/* On new baseboards (> 1st ECC rev), or baseboards @ 25Mhz enable
	 * ECC error reporting via IP8.  It's no harm to leave it disabled
	 * on old IP22 baseboards.
	 */
	if ((board_rev() > (IP26_ECCSYSID>>BOARD_REV_SHIFT)) ||
	    (!(*(volatile int *)PHYS_TO_K1(HPC3_EXT_IO_ADDR)&EXTIO_GIO_33MHZ)))
		ip26_sr_mask |= SR_IBIT8;
	set_work1(ip26_sr_mask);

	/* make sure count still counts after tripping interrupt. */
	set_config(get_config() & ~CONFIG_ICE);

	ip26_clear_ecc();		/* clear any acumulated errors */
	(void)ip26_enable_ucmem();	/* in slow mode till allowintrs() */
}

/* Set interrupt handler for the profiling clock.
 */
void
setkgvector(void (*func)())
{
	c0vec_tbl[6].isr = func;
}


/* Set interrupt handler for given local interrupt level.
 */
void
setlclvector(int level, void (*func)(__psint_t, struct eframe_s *),
	     __psint_t arg)
{
	register lclvec_t *lv_p; /* Which table entry to update */
	volatile unchar *mask_reg;
	int lcl_id;		/* which lcl intr */
	int s;

	
	/* Depending on the level, we need to set up a mask, and put
	 * the function into a particular jump table.
	 */
	if (level >= VECTOR_TCCHW) {
		level = level - VECTOR_TCCHW + 9;

		/*
		 * Only plug in handlers, don't enable or disable
		 * the interrupt on TCC since for HW/LW interrupts
		 * must be changed by the gfx driver constantly.
		 */
		if (func) {
			lcl0vec_tbl[level].isr = func;
			lcl0vec_tbl[level].arg = arg;
		}
		else {
			lcl0vec_tbl[level].isr = straytcc;
			lcl0vec_tbl[level].arg = level + 8;
		}
		return;
	}
	/* Not TCCHW level */

	lcl_id = level/8;
	level &= 7;

	if (lcl_id == 0) {
		lv_p = &lcl0vec_tbl[level+1];
		mask_reg = K1_LIO_0_MASK_ADDR;
	} else if (lcl_id == 1) {
		lv_p = &lcl1vec_tbl[level+1];
		mask_reg = K1_LIO_1_MASK_ADDR;
	} else if (lcl_id == 2) {
		lv_p = &lcl2vec_tbl[level+1];
		mask_reg = K1_LIO_2_MASK_ADDR;
	} else {
		ASSERT(0);	/* No lcl3 interrupts yet. */
	}
	
	if (func) {
		if (lv_p->lcl_flags & THD_ISTHREAD) {
			atomicSetInt(&lv_p->lcl_flags, THD_REG);
			if (ithreads_enabled)
				lclvec_thread_setup(level, lcl_id, lv_p);
		}
		s = splhintr();
		lv_p->isr = func;
		lv_p->arg = arg;
		*mask_reg |= 1 << level;
	} else {
/* If we're going to eliminate the thread handling this interrupt, this
 * is the place to do it.
 */
		s = splhintr();
		lv_p->isr = lcl_stray;
		lv_p->arg = lcl_id*8 + level + 1;
		*mask_reg &= ~(1 << level);
	}
	flushbus();
	splx(s);
}

static struct {
	long vect;
	void (*func)(__psint_t, struct eframe_s *);
} giointr[] = {
	VECTOR_GIO0, gio0_intr,			/* GIO_INTERRUPT_0 */
	VECTOR_GIO1, gio1_intr,			/* GIO_INTERRUPT_1 */
	VECTOR_GIO2, gio2_intr			/* GIO_INTERRUPT_2 */
};

#if GIO_INTERRUPT_2 != 2
#error giointr[] depends on GIO_INTERRUPT_X layout!
#endif

void
setgiovector(int level, int slot, void (*func)(__psint_t, struct eframe_s *),
	     __psint_t arg)
{
	int s;

	if ( level < 0 || level >= GIOLEVELS )
		cmn_err(CE_PANIC,"GIO vector number out of range (%u)",level);
	
	if ( slot < 0 || slot >= GIOSLOTS || slot == GIO_SLOT_1)
		cmn_err(CE_PANIC,"GIO slot number out of range (%u)",slot);

	s = splgio2();

	/* IP26: There are two identical gio slots that all share
	 *	 interrupts at INT2.  A seperate register is read
	 *	 at interrupt time to determine which slot[s] have
	 *	 interrupts pending, so we need to call a fan out
	 *	 function first.
	 */
		giovec_tbl[slot][level].isr = func;
		giovec_tbl[slot][level].arg = arg;

		if (giovec_tbl[GIO_SLOT_GFX][level].isr ||
		    giovec_tbl[GIO_SLOT_0][level].isr ||
		    giovec_tbl[GIO_SLOT_1][level].isr) {
			setlclvector(giointr[level].vect,
				     giointr[level].func,
				     PHYS_TO_K1(HPC3_EXT_IO_ADDR));
		}
		else {
			setlclvector(giointr[level].vect,0,0);
		}

	splx(s);
}

/*  Set GIO slot configuration register.  GIO_SLOT_1 does not exist on
 * the TFP Indigo2 (since GIO_SLOT_GFX is really 'slot 0' in our confused
 * naming).
 */
void
setgioconfig(int slot, int flags)
{
	static int slots = 0;
	uint mask;

	mask = (GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
		GIO64_ARB_EXP0_MST);

	switch (slot) {
	case GIO_SLOT_0:
		slots |= 2;
		break;
	case GIO_SLOT_GFX:
		slots |= 1;
		mask >>= 1;
		flags >>= 1;
		break;
	default:
		cmn_err(CE_PANIC,"GIO slot number out of range (%u)",slot);
	}

        gio64_arb_reg = (gio64_arb_reg & ~mask) | (flags & mask);
	writemcreg(GIO64_ARB, gio64_arb_reg);

	/*  The IP26 baseboard only does the VINO bug fix on GIO slot
	 * 0 and HPC3, not the GFX (lowest) slot.  With the old backplane,
	 * FDDI and Extreme graphics you get a system that will crash
	 * with the VINO bug under FDDI and VDMA stress.  Print a warning
	 * if this happens.
	 */
	if ((gio64_arb_reg & GIO64_ARB_GRX_RT) &&
	    !(gio64_arb_reg & GIO64_ARB_EXP0_RT) && (slots == 3)) {
		cmn_err(CE_CONT,
		"WARNING setgioconfig: Bottom GIO slot configured as a "
		"real-time\n"
		"device while top slot is configured as a long-burst device.\n"
		"The cards should be reversed if the backplane insalled\n"
		"allows this.  If your backplane or configuration does not\n"
		"allow for this, contact your service provider.\n\n");
	}
}

/* Register a HPC dmadone interrupt handler for the given channel.
 */
void
sethpcdmaintr(int channel, void (*func)(__psint_t, struct eframe_s *),
	      __psint_t arg)
{
	int s;
    
	if (channel < 0 || channel >= HPCDMAVECTBL_SLOTS) {
		cmn_err(CE_WARN, "sethpcdmaintr: bad hpcdmadone channel %d, "
			"ignored", channel);
		return;
	}

	s = splhintr();
	if (func) {
		hpcdmavec_tbl[channel+1].isr = func;
		hpcdmavec_tbl[channel+1].arg = arg;
	}
	else {
		hpcdmavec_tbl[channel+1].isr = hpcdmastray;
		hpcdmavec_tbl[channel+1].arg = 0;
	}
	flushbus();
	splx(s);
}

/* find first interrupt
 *
 * This is an inline version of the ffintr routine.  It differs in
 * that it takes a single 11 bit value instead of a shifted value.
 */
static char ffintrtbl[][64] = {
	{ 0, 7, 8, 8, 9, 9, 9, 9,10,10,10,10,10,10,10,10,
	 11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 }
};
#define FFINTR(c) (ffintrtbl[0][c>>6]?ffintrtbl[0][c>>6]:ffintrtbl[1][c&0x3f])

#if DEBUG || INTRDEBUG
/* count which interrupts we are getting, not just total.  Subtract
 * one because handler array starts at offset 1.
 */
unsigned int intrcnt[11], intrl0cnt[10], intrl1cnt[8];
#define COUNT_INTR	intrcnt[(iv-c0vec_tbl)-1]++
#define COUNT_L0_INTR	intrl0cnt[(liv-lcl0vec_tbl)-1]++
#define COUNT_L1_INTR	intrl1cnt[(liv-lcl1vec_tbl)-1]++
#else
#define COUNT_INTR
#define COUNT_L0_INTR
#define COUNT_L1_INTR
#endif /* DEBUG */

/*  Handle 1st level interrupts.
 */
/*ARGSUSED2*/
int
intr(struct eframe_s *ep, uint code, uint sr, uint cause)
{
	int check_kp = 0;

	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTRENTRY,NULL,NULL,NULL,NULL);
	
	while (cause = (cause&sr&SR_IMASK) >> CAUSE_IPSHIFT) {
		do {
			register struct intvec_s *iv = c0vec_tbl+FFINTR(cause);

			COUNT_INTR;
			splx(iv->msk);
			(*iv->isr)(ep,curthreadp);
			if (iv->msk == (SR_IMASK5|SR_IE))
				check_kp = 1;
			atomicAddInt((int *)&SYSINFO.intr_svcd,1);
			cause &= ~iv->bit;
		} while (cause);
		cause = getcause();
	}

	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTREXIT,TSTAMP_EV_INTRENTRY,
			 NULL, NULL, NULL);

	return check_kp;
}

#ifdef ITHREAD_LATENCY
#define SET_ISTAMP(liv) xthread_set_istamp(liv->lcl_lat)
#else
#define SET_ISTAMP(liv)
#endif /* ITHREAD_LATENCY */

#define LCL_CALL_ISR(liv,ep,maskaddr,threaded)			\
	if (liv->lcl_flags & THD_OK) {				\
		SET_ISTAMP(liv);				\
		*(maskaddr) &= ~(liv->bit);			\
		vsema(&liv->lcl_isync);				\
	} else {						\
		(*liv->isr)(liv->arg,ep);			\
	}

/*  Handle local interrupts 0.
 */
static void
lcl0_intr(struct eframe_s *ep)
{
	long tcc_intr = *(volatile long *)PHYS_TO_K1(TCC_INTR);
	register lclvec_t *liv;
	int lcl_cause;
	int svcd = 0;

	/* Merge IP3 (IP<2> in hardware terms from INT2 and TCC (HW+LW)).
	 * tcc<lw,hw> interrupts have masks 0x200,0x100 
	 */
	lcl_cause = (*K1_LIO_0_ISR_ADDR & *K1_LIO_0_MASK_ADDR) |
		    (((tcc_intr>>3) & (tcc_intr<<2)) & 0x300);

	while (lcl_cause) {
		liv = lcl0vec_tbl+FFINTR(lcl_cause);
		COUNT_L0_INTR;
		LCL_CALL_ISR(liv, ep, K1_LIO_0_MASK_ADDR, threaded);
		svcd++;
		lcl_cause &= ~liv->bit;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

/*  Handle local interrupts 1.
 */
static void
lcl1_intr(struct eframe_s *ep)
{
	register lclvec_t *liv;
	char lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_1_ISR_ADDR & *K1_LIO_1_MASK_ADDR;

	while (lcl_cause) {
		liv = lcl1vec_tbl+FFINTR(lcl_cause);
		COUNT_L1_INTR;
		LCL_CALL_ISR(liv, ep, K1_LIO_1_MASK_ADDR, threaded);
		svcd++;
		lcl_cause &= ~liv->bit;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

/*  Handle local interrupts 2.
 */
/*ARGSUSED1*/
static void
lcl2_intr(__psint_t arg, struct eframe_s *ep)
{
	register lclvec_t *liv;
	char lc2_cause;
	int svcd = 0;

	lc2_cause = *K1_LIO_2_ISR_ADDR & *K1_LIO_2_MASK_ADDR;

	while (lc2_cause) {
		liv = lcl2vec_tbl+FFINTR(lc2_cause);
		LCL_CALL_ISR(liv, ep, K1_LIO_2_MASK_ADDR, threaded);
		svcd++;
		lc2_cause &= ~liv->bit;
	}

	/* one counted in lcl[01]_intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

#define HPC3_INTSTAT_ADDR2 0x1fbb000c

/*ARGSUSED1*/
void
hpcdma_intr(__psint_t arg, struct eframe_s *ep)
{
	uint dmadone, dmadone2, selector = 1;
	register lclvec_t *liv;
	int offset = 1;

	/*  Bug in HPC3 requires me to read two registers and 'or' the halves
	 * together.
	 */
	dmadone = * (uint *) PHYS_TO_K1(HPC3_INTSTAT_ADDR);
	dmadone2 = * (uint *) PHYS_TO_K1(HPC3_INTSTAT_ADDR2);
	dmadone = (dmadone2 & 0x3e0) | (dmadone & 0x1f);

	if (dmadone) do {
		if (dmadone & selector) {
			liv = &hpcdmavec_tbl[offset];
			(*liv->isr)(liv->arg,ep);
			dmadone &= ~selector;
		}
		selector = selector << 1;
		offset++;
	} while (dmadone);
}

static void
gio0_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	if ( ((extio & EXTIO_SG_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr ) {
		((lcl_intr_func_t*)giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].arg,ep);
	}

	if ( ((extio & EXTIO_S0_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr ) {
		((lcl_intr_func_t*)giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].arg,ep);
	}
}

static void
gio1_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	if ( ((extio & EXTIO_SG_IRQ_1) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr ) {
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].arg,ep);
	}

	if ( ((extio & EXTIO_S0_IRQ_1) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr ) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].arg,ep);
	}
}

static void
gio2_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	if ( ((extio & EXTIO_SG_RETRACE) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr ) {
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].arg,ep);
	}

	if ( ((extio & EXTIO_S0_RETRACE) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr ) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].arg,ep);
	}
}

/*ARGSUSED*/
static void
lcl_stray(__psint_t lvl, struct eframe_s *ep)
{
	int lcl_id = (lvl-1)/8;
	lvl = (lvl-1)&7;

	cmn_err(CE_WARN, "stray %d interrupt #%d\n", lcl_id, lvl);
}

/*ARGSUSED*/
static void
straytcc(__psint_t adap, struct eframe_s *ep)
{
	cleartccintrs(INTR_FIFO_HW|INTR_FIFO_LW);
	cmn_err(CE_WARN,"stray TCC interrupt #%d\n", adap);
}

/*ARGSUSED*/
static void
hpcdmastray(__psint_t adap, struct eframe_s *ep)
{
	cmn_err(CE_WARN,"stray HPC dmadone interrupt on pbus chan %d\n", adap);
}

/*ARGSUSED*/
static void
hpcdmastray_scsi(__psint_t adap, struct eframe_s *ep)
{
	uint clear;

	if (adap == 9)
		clear = *(uint *)PHYS_TO_K1(HPC3_SCSI_CONTROL0);
	else
		clear = *(uint *)PHYS_TO_K1(HPC3_SCSI_CONTROL1);
	clear &= 0xff;

	if (clear & SCPARITY_ERR)
		cmn_err(CE_WARN, "stray parity error on scsi chan %d, control "
			"bits 0x%x", adap - 9, clear);
	else
		cmn_err(CE_WARN, "stray HPC dmadone interrupt on scsi chan %d,"
			" control bits 0x%x", adap - 9, clear);
}

/*ARGSUSED*/
static void
powerfail(__psint_t arg, struct eframe_s *ep)
{
	extern int wd93cnt;
	int adap;
	int i;

	/*  Some power supplies will give spurious short lived (~250ns)
	 * ACFAIL interrupts.  Check twice to see if the interrupt is
	 * still present.  If not, we probably have a flakey supply,
	 * so disable the ACFAIL interrupt.  We need to try twice since
	 * if we get unlucky we can check when there is more noise
	 * on the signal.  Spikes are usually ~9.5us apart, and the
	 * loop here takes ~4us, so we should land between spikes.
	 */
	for (i=0; i < 2; i++) {
		DELAY(2);
		if (!((*K1_LIO_1_ISR_ADDR)&LIO_AC)) {
			if (showconfig)
				cmn_err(CE_WARN,"spurious power failure "
					"interrupt -- Check power supply.\n");
			setlclvector(VECTOR_ACFAIL,0,0);
			return;
		}
	}

	/*  Maybe should spin a bit before complaining?  Sometimes
	 * see this when people power off.  May want to use this
	 * for 'clean' shutdowns if we ever get enough power to
	 * write out part of the superblocks, or whatever.  For
	 * now, reset SCSI bus to attempt to ensure we don't wind
	 * up with a media error due to a block header write being
	 * in progress when the power actually fails.
	 */
	for(adap=0; adap < wd93cnt; adap++)
		wd93_resetbus(adap);

	cmn_err(CE_WARN,"Power failure detected\n");
}

/*  High level TFP COUNT interrupt handler -- clear the interrupt from
 * the cause in the ep in case we get another exception (like a GCache
 * parity error), then call the low-level handler [in IP26asm.s].
 */
/*ARGSUSED*/
static void
tfpcount_intr(struct eframe_s *ep)
{
	extern void _tfpcount_intr(struct eframe_s *);

	/* Shouldn't get an interrupt.  Make sure work1 is still set. */
	set_work1(ip26_sr_mask);

	ep->ef_cause &= ~CAUSE_IP11;
	_tfpcount_intr(ep);

#ifdef DEBUG
	/* We may want to assume code will clean-up and let us contine */
	cmn_err(CE_PANIC,"CPU count interrupt occured SR=%x!",ep->ef_sr);
#endif
}

/* Shoot down the current process or the whole system as appropriate.
 */
static void
shotgun_gcache(struct eframe_s *ep)
{
#if !_TETON_GCACHE_WAR
	/*  If we cannot locate the error, kill the current process if
	 * running in user mode, else panic the system.
	 */
	if (!USERMODE(ep->ef_sr) || (curvprocp == 0)) {
		cmn_err(CE_PANIC,
			"IRIX Killed due to secondary cache parity error.\n");
	}

	cmn_err(CE_CONT, "Warning: Process %d [%s] sent SIGBUS due to secondary"
		" cache parity error\n", current_pid(), get_current_name());
	sigtouthread(curuthread, SIGBUS, (k_siginfo_t *)NULL);
#endif

	return;
}

/*  Handle GCache errors detected by TCC.
 */
void
tccgcache_intr(struct eframe_s *ep, long tcc_error, long tcc_parity)
{
	int set = (tcc_error&ERROR_PARITY_SET)>>ERROR_PARITY_SET_SHIFT;
	int off = (tcc_error&ERROR_OFFSET)>>ERROR_OFFSET_SHIFT;
	int idx = (tcc_error&ERROR_INDEX)>>ERROR_INDEX_SHIFT;
	static char *srams[][5] = {
		{"S15|S16", "S6|S12", "S10|S11",  "S4|S5", "even unknown"},
		{"S13|S14", "S3|S9",  "S7|S8",    "S1|S2", "odd unknown"}
	};
	volatile long *taga;
	int sram = 4;
	long tag;

	/* Clear the machine check on the TCC.
	 */
	*(volatile long *)PHYS_TO_K1(TCC_INTR) = _tcc_intr_save
				& (INTR_MACH_CHECK|0xf800);
	*(volatile long *)PHYS_TO_K1(TCC_ERROR) = ERROR_NESTED_MC;

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

	cmn_err(CE_WARN, "TCC detected GCache parity error "
		"index=%d set=%d offset=%d SRAMs %s [%s]\n",
		idx, set, off, tcc_parity, srams[off&1][sram],
		off&1?"odd":"even");

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

	shotgun_gcache(ep);
}

/*  Handle CPU detected parity error interrupt.
 */
static void
tfpgcache_intr(struct eframe_s *ep)
{
	k_machreg_t scause;

	scause = ep->ef_cause;
	ep->ef_cause &= ~(CAUSE_IP9|CAUSE_IP10);
	setcause(getcause() & ~(CAUSE_IP9|CAUSE_IP10));

	if (!USERMODE(ep->ef_sr) || (curvprocp == 0))
		cmn_err(CE_WARN, "CPU detected GCache%s%s parity error in "
			"kernel @ epc 0x%x\n",
			scause & CAUSE_IP9 ?  " [even]"  : "",
			scause & CAUSE_IP10 ? " [odd]"   : "",
			ep->ef_epc);
	else
		cmn_err(CE_WARN, "CPU detected GCache%s%s parity error in "
			"%s [pid=%d] @ epc 0x%x\n",
			scause & CAUSE_IP9 ?  " [even]"  : "",
			scause & CAUSE_IP10 ? " [odd]"   : "",
			get_current_name(), current_pid(), ep->ef_epc);

	/* Flush entire cache, so hopefully TCC will catch it, and we
	 * can really know where it is.  Need to lower spl so we can
	 * take the TCC exception.
	 */
	setsr(getsr() | SR_IBIT7);
	flush_cache();

	shotgun_gcache(ep);
}

/* function to shut off flat panel.  Set in gr2_init.c */
int (*fp_poweroff)(void);

/*  Can be called via a timeout from powerintr(), direct from prom_reboot
 * in arcs.c, or from mdboot() as a result of a uadmin call.
 */
void
cpu_powerdown(void)
{
	register ds1286_clk_t *clock = (ds1286_clk_t *)RTDS_CLOCK_ADDR;
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	volatile int dummy;

	/* shut off the flat panel if one is attached */
	if (fp_poweroff)
		(*fp_poweroff)();

	/* Some power supplies give ACFAIL when soft powered down
	 */
	setlclvector(VECTOR_ACFAIL,0,0);

	/* Disable watchdog output so machine knows it was turned off
	 */
	clock->command |= WTR_DEAC_WAM;
	clock->watch_hundreth_sec = 0;
	clock->watch_sec = 0;

	splhi();

	while(1) {
		*p = ~POWER_SUP_INHIBIT;	/* sets POWER_INT */
		flushbus();
		DELAY(10);

		/* If power not out, maybe "wakeupat" time hit the window.
		 */
		if (clock->command & WTR_TDF) {
			/* Disable and clear alarm */
			clock->command |= WTR_DEAC_TDM;
			flushbus();
			dummy = clock->hour_alarm;
		}
	}
}

static void
cpu_godown(void)
{
	cmn_err(CE_WARN,
		"init 0 did not shut down after %d seconds, powering off.\n",
		clean_shutdown_time);
	DELAY(500000);	/* give them 1/2 second to see it */
	cpu_powerdown();
}

/*  While looping, check if the power button is being pressed.  Used to
 * keep the power switch working after a panic.
 */
void
cpu_waitforreset(void)
{
	while (1)
		if (*K1_LIO_1_ISR_ADDR & LIO_POWER) cpu_powerdown();
}

/*  Handle power button interrupt by shutting down the machine (signal init)
 * on the first press, and killing the power on after a second press.
 */
/*ARGSUSED*/
static void
powerintr(__psint_t arg, struct eframe_s *ep)
{
	volatile uint *p = (uint *)PHYS_TO_K1(HPC3_PANEL);
	static time_t doublepower;
	extern int power_off_flag;	/* set in powerintr(), or uadmin() */

	power_off_flag = 1;		/* for prom_reboot() in ars.c */

	/* re-enable the interrupt */
	*p = POWER_INT|POWER_SUP_INHIBIT;
	flushbus();

	if (!doublepower) {
		power_bell();		/* ack button press */
		flash_led(3);

		/* wait for switch to be released */
		while (*K1_LIO_1_ISR_ADDR & LIO_POWER) {
			*p = POWER_ON;
			flushbus();
			DELAY(5);
		}

		/*  Send SIGINT to init, which is the same as
		 * '/etc/init 0'.  Set a timeout
		 * in case init wedges.  Set doublepower to handle
		 * quick (with a little debounce room) 'doubleclick'
		 * on the power switch for an immediate shutdown.
		 */
		if (!sigtopid(INIT_PID, SIGINT, SIG_ISKERN, 0, 0, 0)) {
			timeout(cpu_godown, 0, clean_shutdown_time*HZ);
			doublepower = lbolt + HZ;
			set_autopoweron();
		}
		else {
			DELAY(10000);		/* 10ms debounce time */
			cpu_powerdown();
		}
	}
	else if (lbolt > doublepower) {
		power_bell();

		/* wait for switch to be released */
		while (*K1_LIO_1_ISR_ADDR & LIO_POWER) {
			*p = POWER_ON;
			flushbus();
			DELAY(5);
		}

		DELAY(10000);		/* 10ms debounce time */

		cmn_err(CE_NOTE, "Power switch pressed twice -- "
				 "shutting off power now.\n");

		cpu_powerdown();
	}

	return;
}

/*  Flash LED from green to off to indicate system powerdown is in
 * progress.
 */
static void
flash_led(int on)
{
	set_leds(on);
	timeout(flash_led,(void *)(on ? 0 : (__psint_t)3),HZ/4);
}

/* Log the SIMMs of a detected multi-bit ECC error.
 *	addr  - the contents of the cpu or gio parity error address register.
 * With an ECC error, we only know which leaf (2 SIMMS) it is in, since ECC
 *  is calculated across 64 bits.
 */
static char ecc_bad_simm_string[16];

void
log_eccerr(uint addr)
{
	int bank = addr_to_bank(addr);
	static char *simm[3][2] = {
		"S11 and S12 ",	"S9 and S10 ",
		"S7 and S8 ",	"S5 and S6 ",
		"S3 and S4 ",	"S1 and S2 "
	};
	ecc_bad_simm_string[0] = '\0';

	if (bank < 0) {
		cmn_err(CE_CONT, "Multi-bit ECC Error in Unknown SIMMs\n");
		return;
	}

	if (!(addr & 0x8))
		strcat(ecc_bad_simm_string, simm[bank][0]);
	else
		strcat(ecc_bad_simm_string, simm[bank][1]);

	cmn_err(CE_CONT, "Multi-bit ECC Error in SIMMs %s\n",
		ecc_bad_simm_string);

	return;
}

/* Log the bank, byte, and SIMM of a detected parity error.
 *	addr  - the contents of the cpu or dma parity error address register.
 *	bytes - the contents of the parity error register.
 */
#define	BYTE_PARITY_MASK	0xff
static char bad_simm_string[16];

void
log_perr(uint addr, uint bytes, int no_console)
{
	int bank = addr_to_bank(addr);
	static char *simm[3][4] = {
		"S12 ", "S11 ", "S10 ", "S9 ",
		"S8 ",  "S7 ",  "S6 ",  "S5 ",
		"S4 ",  "S3 ",  "S2 ",  "S1 ",
	};
	bad_simm_string[0] = '\0';

	if (bank < 0) {
		if (no_console)
			cmn_err(CE_CONT, "!Parity Error in Unknown SIMM\n");
		else
			cmn_err(CE_CONT, "Parity Error in Unknown SIMM\n");
		return;
	}

	if ((bytes & 0xf0) && !(addr & 0x8))
		strcat(bad_simm_string, simm[bank][0]);
	if ((bytes & 0x0f) && !(addr & 0x8))
		strcat(bad_simm_string, simm[bank][1]);
	if ((bytes & 0xf0) && (addr & 0x8))
		strcat(bad_simm_string, simm[bank][2]);
	if ((bytes & 0x0f) && (addr & 0x8))
		strcat(bad_simm_string, simm[bank][3]);

    	if (no_console)
		cmn_err(CE_ALERT, "!Memory Parity Error in SIMM %s",
			bad_simm_string);
	else
		cmn_err(CE_CONT, "Warning: Memory Parity Error in SIMM %s\n",
			bad_simm_string);

	return;
}

/*
 * fix bad parity by writing to the locations at fault, kv_addr must be
 * an uncached kernel virtual address
 */
static void
fix_bad_parity(__psunsigned_t kv_addr)
{
	kv_addr &= ~0x7;	/* kv_addr may or may not be a mapped kaddr */
	*(volatile uint *)(kv_addr) = *(volatile uint *)(kv_addr);
	*(volatile uint *)(kv_addr + 4) = *(volatile uint *)(kv_addr + 4);

	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	flushbus();
}

#if LATER
/* a global so wd93dma_flush can use same message */
char *dma_par_msg = "DMA Parity Error detected in Memory\n\tat Physical Address 0x%x\n";
#endif

/*  Descriptions of some TCC registers.
 */
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

static struct reg_desc _tcc_intr_desc[] = {
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


/*		Common IP26 bus error handling code
 *
 *  The MC sends an interrupt whenever bus or parity errors occur.  
 * In addition, if the error happened during a CPU read, it also
 * asserts the bus error pin on the SysAD.  Code in VEC_ibe and VEC_dbe
 * save the MC/TCC bus error registers and then clear the interrupt.
 * If the error happens on a write, then the bus error interrupt handler
 * saves the info and comes here.
 *
 *  On IP26 the TCC also reports a number of 'machine checks' which are
 * reported in the same manner as MC bus errors.  TCC bus errors are
 * reported via the CAUSE_BE bit.
 *
 * flag:	0: kernel; 1: kernel - no fault; 2: user
 */

#define GIO_ERRMASK	0xff00
#define CPU_ERRMASK	0x3f00

uint hpc3_ext_io, hpc3_bus_err_stat;
uint cpu_err_stat, gio_err_stat;
uint cpu_par_adr, gio_par_adr;
static __psunsigned_t mapped_bad_addr;
static uint bad_data_hi, bad_data_lo;

/*  Re-read address where an invalid ECC error occured to try and determine
 * if the address has a soft or a hard error.
 */
int
ecc_reread(unsigned int physaddr)
{
	volatile unsigned long *p = (unsigned long *)PHYS_TO_K1(physaddr & ~7);
	volatile uint *cpu_err = (uint *)PHYS_TO_K1(CPU_ERR_STAT);
	int rc = 0;

	/*  Flush the primary cache to insure we do not get a false hit
	 * with the uncached read due to TFP oddities.  On a dirty hit,
	 * this may affect clear out our error, but we will not get
	 * the right answer there anyway.
	 */
	__dcache_wb_inval((void *)(long)physaddr,TCC_LINESIZE);

	*cpu_err = 0;
	flushbus();

	*p;			/* read physaddr */
	flushbus();

	rc = (*cpu_err == 0);

	*cpu_err = 0;
	flushbus();

	return rc;
}

#define ECC_BAD_CPU	0x0001
#define ECC_BAD_GIO	0x0002
#define ECC_MBIT_UNKN	0x0004
#define ECC_MBIT_CPU	0x0008
#define ECC_MBIT_GIO	0x0010
#define ECC_CPU_TRANS	0x0020
#define ECC_GIO_TRANS	0x0040
#define ECC_UCWRITE	0x0080
/*  Print all panic information in on cmn_err() call so all data gets
 * flushed to the console propperly.  Use memory @ 0 for buffer as
 * TFP does not use it for exception processing, we cannot allocate
 * enough space on the stack, and do not want to malloc.
 */
static void
ecc_panic(int eccpanic)
{
	char *buf = (char *)K0_RAMBASE;
	char *p = buf;

	*p = 0;

	if (eccpanic & ECC_BAD_CPU) {
		sprintf(p,"Hardware error (%s) on IP26 baseboard "
			"CPU stat: 0x%x.\n",
			(eccpanic & ECC_CPU_TRANS) ? "soft" : "hard",
			cpu_err_stat);
		p += strlen(p);
	}

	if (eccpanic & ECC_BAD_GIO) {
		sprintf(p,"Hardware error (%s) on IP26 baseboard "
			"GIO stat: 0x%x.\n",
			(eccpanic & ECC_GIO_TRANS) ? "soft" : "hard",
			gio_err_stat);
		p += strlen(p);
	}

	if (eccpanic & ECC_MBIT_UNKN) {
		sprintf(p,"IRIX detected multi-bit ECC error.\n");
		p += strlen(p);
	}

	if (eccpanic & ECC_MBIT_CPU) {
		sprintf(p,"IRIX detected multi-bit ECC error (CPU) at 0x%x.\n",
			cpu_par_adr);
		p += strlen(p);
	}

	if (eccpanic & ECC_MBIT_GIO) {
		sprintf(p,"IRIX detected multi-bit ECC error (GIO) at 0x%x.\n",
			gio_par_adr);
		p += strlen(p);
	}

	if (eccpanic & ECC_UCWRITE) {
		sprintf(p, "\n%s", IP26_UC_WRITE_MSG);
		p += strlen(p);
	}

	cmn_err_tag(41,CE_PANIC,
		    "%s\nIRIX Killed due to fatal memory ECC error.\n",
		    buf);
}

int
dobuserre(struct eframe_s *ep, inst_t *epc, uint flag)
{
	char *tcc_sysaderr = 0;
	int fatal_error = 0;
	int cpu_buserr = 0;
	int cpu_memerr = 0;
	int gio_buserr = 0;
	int eccpanic = 0;

	_tcc_intr_save = *(volatile long *)PHYS_TO_K1(TCC_INTR);
	_tcc_error_save = *(volatile long *)PHYS_TO_K1(TCC_ERROR);
	_tcc_parity_save = *(volatile long *)PHYS_TO_K1(TCC_PARITY);
	_tcc_be_addr_save = *(volatile long *)PHYS_TO_K1(TCC_BE_ADDR);

	/*  First check for a TCC unknown T-bus command interrupt.  We
	 * unfortunately get this on a 'sync' instruction, so we just
	 * clear the interrupt and continue.  A program with a lot of
	 * syncs will perform poorly, but it will run.  There is some
	 * risk to this since we may have another problem somewhere
	 * down the line, so for DEBUG kernels, print a message on
	 * the console.
	 */
	if ((_tcc_intr_save & INTR_MACH_CHECK) &&
	    ((_tcc_error_save & ERROR_TYPE) ==
	     (ERROR_TBUS_UFO<<ERROR_TYPE_SHIFT))) {
#if DEBUG
		cmn_err(CE_WARN,
			"^TCC detected unknown T-Bus command "
			"(presumed sync instruction).");
#endif
		cleartccerrors(INTR_MACH_CHECK);
		_tbus_slow_ufos++;
		return(1);
	}

	/* Second handle TCC detected GCache parity error.
	 */
	if ((_tcc_intr_save & INTR_MACH_CHECK) &&
	    ((_tcc_error_save&ERROR_TYPE) >> ERROR_TYPE_SHIFT) ==
	    ERROR_GCACHE_PARITY) {
		tccgcache_intr(ep,_tcc_error_save,_tcc_parity_save);
		return(1);		/* ignore error if we return */
	}

	/* Third check for ECC errors
	 *
	 * If we're on an IP26, ECC/memory errors come in as parity errors,
	 * then there are 2 bus errors which we need to deal with.
	 */
	if (((cpu_err_stat & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) &&
	    ip26_isecc()) {
		cleartccerrors(INTR_MACH_CHECK);	/* from parity err */

		if ((cpu_err_stat & ECC_MUST_BE_ZERO)) {
			if (ecc_reread(cpu_par_adr))
				eccpanic |= ECC_CPU_TRANS;
			eccpanic |= ECC_BAD_CPU;
		}
		if (cpu_err_stat & ECC_MULTI_BIT_ERR)
			eccpanic |= ip26_ecc_mbit_intr(ep,ECC_ISBERR);
		if (cpu_err_stat & ECC_FAST_UC_WRITE) {
#ifdef DEBUG
			/* return on ecc nofault */
	    		if (ip26_ecc_uc_write_intr(ep,
				ECC_ISBERR|ECC_NOFAULT) == 1)
				return(1);
#endif
	  		eccpanic |= ip26_ecc_uc_write_intr(ep, ECC_ISBERR);
		}

		/* GIO timeouts with bit 31 on are odd.  This isn't
		 * always latched correctly in MC, so this is only
		 * a partial fix.
		 */
		if ((gio_err_stat & GIO_ERR_STAT_TIME) && 
		    (gio_par_adr & 0x80000000)) {
#ifdef DEBUG
			cmn_err(CE_WARN,"Looks like we have a GIO timeout "
					"with bit 31 set.\n");
#endif
			eccpanic = 0;
		}
	}

	if ((gio_err_stat & GIO_ERR_STAT_RD) && ip26_isecc()) {
		cleartccerrors(INTR_MACH_CHECK);	/* from parity err */
		if ((gio_err_stat & ECC_MUST_BE_ZERO)) {
			if (ecc_reread(gio_par_adr))
				eccpanic |= ECC_GIO_TRANS;
			eccpanic |= ECC_BAD_GIO;
		}
		if (gio_err_stat & ECC_MULTI_BIT_ERR)
			eccpanic |= ip26_ecc_mbit_intr(ep,
					ECC_ISBERR|ECC_ISGIO);
		if (gio_err_stat & ECC_FAST_UC_WRITE) {
#ifdef DEBUG
			/* return on ecc nofault */
			if (ip26_ecc_uc_write_intr(ep,
				ECC_ISBERR|ECC_ISGIO|ECC_NOFAULT) == 1)
				return(1);
#endif
			eccpanic |= ip26_ecc_uc_write_intr(ep,
							ECC_ISBERR|ECC_ISGIO);
		}
	}

	/*  Forth, check for nofault.  We used to check this first but with
	 * moving more stuff into dobuserr, we have to clear the other odd
	 * interrupts first.
	 */
	if (flag == 1)
		return(0);			/* nofault */

	/* We print all gruesome info for:
	 *   1. debugging kernel
	 *   2. bus errors
	 */
	if (kdebug || (_tcc_intr_save & (INTR_BUSERROR|INTR_MACH_CHECK))) {
		/*  If we really got a TCC detected error, we need to clear it
		 * before going too far (and doing things like cmn_err()).
		 */
		if (_tcc_intr_save & (INTR_BUSERROR|INTR_MACH_CHECK)) {
			cleartccerrors(INTR_BUSERROR|INTR_MACH_CHECK);
		}

		cmn_err(CE_CONT,"TCC Intr %R\n",
			_tcc_intr_save, _tcc_intr_desc);
		if (_tcc_intr_save & INTR_BUSERROR)
			cmn_err(CE_CONT,"TCC BusError 0x%x\nTCC Parity 0x%x\n",
				_tcc_be_addr_save, _tcc_parity_save);
		if (_tcc_intr_save & INTR_MACH_CHECK)
			cmn_err(CE_CONT,"TCC Error %R\n",
				_tcc_error_save, _tcc_error_desc);
	}
	if (kdebug) {
		unsigned int memacc = *(volatile unsigned int *)
					PHYS_TO_K1(CPU_MEMACC);
		cmn_err(CE_CONT,"TCC_GCACHE 0x%x, CPU_MEMACC 0x%x -> %s mode\n",
			*(volatile long *)PHYS_TO_K1(TCC_GCACHE), memacc,
			(memacc == CPU_MEMACC_SLOW) ? "slow" : "normal");
	}
	if (kdebug || (cpu_err_stat & CPU_ERRMASK))
		cmn_err(CE_CONT,
			"CPU Error/Addr %sactive 0x%x<%s%s%s%s%s%s>: 0x%x\n",
			hpc3_ext_io & EXTIO_MC_BUSERR ? "" : "in",
			cpu_err_stat,
			cpu_err_stat & CPU_ERR_STAT_RD ? "RD " : "",
			cpu_err_stat & CPU_ERR_STAT_PAR ? "PAR " : "",
			cpu_err_stat & CPU_ERR_STAT_ADDR ? "ADDR " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSAD_PAR ? "SYSAD " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSCMD_PAR ? "SYSCMD " : "",
			cpu_err_stat & CPU_ERR_STAT_BAD_DATA ? "BAD_DATA " : "",
			(__psunsigned_t)cpu_par_adr);
	if (kdebug || (gio_err_stat & GIO_ERRMASK))
		cmn_err(CE_CONT,"GIO Error/Addr 0x%x:<%s%s%s%s%s%s%s%s> 0x%x\n",
			gio_err_stat,
			gio_err_stat & GIO_ERR_STAT_RD ? "RD " : "",
			gio_err_stat & GIO_ERR_STAT_WR ? "WR " : "",
			gio_err_stat & GIO_ERR_STAT_TIME ? "TIME " : "",
			gio_err_stat & GIO_ERR_STAT_PROM ? "PROM " : "",
			gio_err_stat & GIO_ERR_STAT_ADDR ? "ADDR " : "",
			gio_err_stat & GIO_ERR_STAT_BC ? "BC " : "",
			gio_err_stat & GIO_ERR_STAT_PIO_RD ? "PIO_RD " : "",
			gio_err_stat & GIO_ERR_STAT_PIO_WR ? "PIO_WR " : "",
			(__psunsigned_t)gio_par_adr);
	if (kdebug || (hpc3_ext_io & EXTIO_HPC3_BUSERR)) {
		gio64_buserr_stat_t be = *(gio64_buserr_stat_t *)
			&hpc3_bus_err_stat;
		cmn_err(CE_CONT,
			"HPC3 Bus Error %sactive 0x%x:<id=0x%x,%s,lane=0x%x>\n",
			hpc3_ext_io & EXTIO_HPC3_BUSERR ? "" : "in",
			hpc3_bus_err_stat,
			be.parity_id,
			be.pio_dma ? "PIO" : "DMA",
			be.byte_lane_err);
	}
	if (hpc3_ext_io & EXTIO_EISA_BUSERR)
		cmn_err(CE_CONT,"EISA Bus Error.\n");

	/* Other TCC_ERROR cases have already been handled by dobuserr().
	 */
	if (_tcc_intr_save & INTR_MACH_CHECK) {
		switch (_tcc_error_save&ERROR_TYPE) {
		case ERROR_SYSAD_TDB<<ERROR_TYPE_SHIFT:
		case ERROR_SYSAD_TCC<<ERROR_TYPE_SHIFT:
			tcc_sysaderr = "sysAD parity error";
			break;
		case ERROR_SYSAD_CMD<<ERROR_TYPE_SHIFT:
			tcc_sysaderr = "sysCMD parity error";
			fatal_error = 1;
			break;
		case ERROR_SYSAD_UFO<<ERROR_TYPE_SHIFT:
			tcc_sysaderr = "unknown sysAD command";
			fatal_error = 1;
			break;
		}
	}
	else if (_tcc_intr_save & INTR_BUSERROR)
		cpu_buserr = 1;

	/*  We are here because the CPU did something bad - if it
	 * read a parity error, then the CPU bit will be on and
	 * the CPU_ERR_ADDR register will contain the faulting word
	 * address. If a parity error happened during an GIO transaction,
	 * then GIO_ERR_ADDR will contain the bad memory address.
	 */
	if (cpu_err_stat & CPU_ERR_STAT_ADDR)
		cpu_buserr = 1;
	else if (cpu_err_stat&(CPU_ERR_STAT_SYSAD_PAR|CPU_ERR_STAT_SYSCMD_PAR))
		cpu_memerr = 2;
	else if (cpu_err_stat & CPU_ERRMASK)
		fatal_error = 1;

	if (gio_err_stat & (GIO_ERR_STAT_TIME | GIO_ERR_STAT_PROM))
		gio_buserr = 1;
	else if (gio_err_stat & GIO_ERRMASK)
		fatal_error = 1;

	if (hpc3_ext_io & (EXTIO_HPC3_BUSERR|EXTIO_EISA_BUSERR))
		fatal_error = 1;

	if (eccpanic)
		ecc_panic(eccpanic);

	if (tcc_sysaderr) {
		if (fatal_error)
			cmn_err_tag(42,CE_PANIC,
				    "IRIX Killed due to TCC detected %s\n",
				    tcc_sysaderr);

		/* SysAD parity error is not fatal as we might be able to get
		 * information from MC if it was a memory error, so just note
		 * that TDB detected the error and see if we get more info
		 * from the MC dump.
		 */
		cmn_err(CE_WARN, "TDB detected %s\n", tcc_sysaderr);
	}

	if (cpu_memerr == 2) {
		cmn_err(CE_PANIC,
			"IRIX Killed due to MC detected %s Parity Error "
			"(byte lanes 0x%x).\n",
			(cpu_err_stat & CPU_ERR_STAT_SYSAD_PAR) ?
			"SysAD": "SysCMD", cpu_err_stat & 0xff);
	}

	mapped_bad_addr = 0x0;
	
	if (cpu_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT, "Process %d [%s] sent SIGBUS due to "
				"Bus Error\n",
				current_pid(), get_current_name());
		else
			cmn_err_tag(43,CE_PANIC, 
				    "IRIX Killed due to Bus Error\n"
				    "\tat PC:0x%x ep:0x%x\n", epc, ep);
	}

	if (gio_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT, "Process %d [%s] sent SIGBUS due to "
				"Bus Error\n", current_pid(),
				get_current_name());
		else
			cmn_err_tag(44,CE_PANIC, 
				    "IRIX Killed due to Bus Error\n"
				    "\tat PC:0x%x ep:0x%x\n", epc, ep);
	}

	if (fatal_error)
	    cmn_err_tag(45,CE_PANIC, "IRIX Killed due to internal error\n"
			"\tat PC:0x%x ep:0x%x\n", epc, ep);

	return(0);
}

#define	CPU_RD_PAR_ERR	(CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)

/*  Handle bus error interrupt -- save info and call dobuserre().
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 */
int
dobuserr(struct eframe_s *ep, inst_t *epc, uint flag)
{
	/*  Save machine info for dobuserre() (done at lower level for
	 * exceptions).
	 */
	hpc3_ext_io = *(volatile uint *)PHYS_TO_K1(HPC3_EXT_IO_ADDR);
	cpu_err_stat = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	gio_err_stat = *(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT);
	cpu_par_adr  = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
	gio_par_adr  = *(volatile uint *)PHYS_TO_K1(GIO_ERR_ADDR);
	hpc3_bus_err_stat = *(volatile uint *)PHYS_TO_K1(HPC3_BUSERR_STAT_ADDR);

	/* If we're on an IP26 (ecc motherboard), and we got a buserror
	 * due to a fast-mode write, the ECC hardware will continue to
	 * generate buserrs on every memory read until we clear the ECC HW.
	 * This must be done before clearing the corresponding MC regs, or
	 * we could have another error show up there.
	 */
	if ((ip26_isecc()) &&
	    (((cpu_err_stat & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) ||
	     (gio_err_stat & GIO_ERR_STAT_RD)))
		ip26_clear_ecc();

	/*  Clear the bus error by writing to the parity error register
	 * in case we're here due to parity problems.
	 */
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	flushbus();

	return dobuserre(ep, epc, flag);
}

/*  Should never receive an exception (other than interrupt) while
 * running on the idle stack -- Check for memory errors.
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
	if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	    (cause & CAUSE_EXCMASK) == EXC_DBE ||
	    (cause & CAUSE_BE) )
		dobuserre(NULL, epc, 0);
	else if (cause & CAUSE_BERRINTR)
		dobuserr(NULL, epc, 0);

	cmn_err_tag(46,CE_PANIC,
	"exception on IDLE stack k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
		k1, epc, cause, sp, getbadvaddr());
	/* NOTREACHED */
}

/* Handle very early global faults.  Returns:
 * 	1 if should do nofault
 *	0 if not
 */
int
earlynofault(struct eframe_s *ep, uint code)
{
	if (code != EXC_DBE)
		return(0);

	dobuserre(ep, (inst_t *)ep->ef_epc, 1);

	return(1);
}

/* Probe for floating point chip.
 */
void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}

/* fp chip initialization.
 */
void
fp_init(void)
{
	set_fpc_csr(0);
}

/*  Set the cache algorithm pde_t *p. Since there are several cache algorithms
 * for TFP, (non-coherent, update, invalidate, etc), this function sets the
 * 'basic' algorithm for a particular machine.  IP26 uses non-coherent.
 */
void
pg_setcachable(pde_t *p)
{
	pg_setnoncohrnt(p);
}

uint
pte_cachebits(void)
{
	return PG_NONCOHRNT;
}

/*  Allocate a dma map of the requested type.  Npage and flags are not used
 * for IP26, since it is only used for SCSI and EISA.
 */
/*ARGSUSED4*/
dmamap_t *
dma_mapalloc(int type, int adap, int npage, int flags)
{
	static dmamap_t smap[NSCUZZY];
	dmamap_t *dmamap;

	switch (type) {

	case DMA_SCSI:
		if (adap >= NSCUZZY)
			break;				/* failure */
		smap[adap].dma_type = DMA_SCSI;
		smap[adap].dma_adap = adap;
		return &smap[adap];

	case DMA_EISA:
		if (adap != 0)
			break;				/* failure */
		dmamap = (dmamap_t *)kern_calloc(1, sizeof(dmamap_t));

		dmamap->dma_type = DMA_EISA;
		dmamap->dma_adap = 0;
		dmamap->dma_size = npage;
		return (dmamap);
	}

	return((dmamap_t *)-1L);			/* -1 -> failure */
}

/*  Macros to support multiple maximum HPC SCSI DMA sizes.  For large
 * pages (16K+) we can map the page in 4K or 8K DMA descriptors.
 */
#define HPC4KSCSIDMA	1		/* MC page mode not verified on 8K */
#if NBPP == 4096			/* 4K pages, do max 4K transfers */
#define HPC_NBPP	NBPC
#define HPC_BPCSHIFT	BPCSHIFT
#define HPC_NBPC	NBPC
#define HPC_POFFMASK	POFFMASK
#define HPC_PGFCTR	PGFCTR
#define hpc_poff(x)	poff(x)
#define hpc_ctob(x)	ctob(x)
#define hpc_btoc(x)	btoc(x)
#define hpc_btoct(x)	btoct(x)
#elif HPC4KSCSIDMA			/* large pages, but limit dmasize */
#define _USE_HPCSUBPAGE	1
#define HPC_NBPP	IO_NBPP
#define HPC_BPCSHIFT	IO_BPCSHIFT
#define HPC_NBPC	IO_NBPC
#define HPC_POFFMASK	IO_POFFMASK
#define HPC_PGFCTR	PGFCTR
#define hpc_poff(x)	io_poff(x)
#define hpc_ctob(x)	io_ctob(x)
#define hpc_btoc(x)	io_btoc(x)
#define hpc_btoct(x)	io_btoct(x)
#else					/* large pages, use max DMA size */
#define _USE_HPCSUBPAGE	1
#define HPC_NBPP	8192
#define HPC_BPCSHIFT	13
#define HPC_NBPC	HPC_NBPP
#define HPC_POFFMASK	(HPC_NBPP - 1)
#define HPC_PGFCTR	(NBPP / HPC_NBPP)
#define hpc_poff(x)	((__psunsigned_t)(x) & HPC_POFFMASK)
#define hpc_ctob(x)	((x)<<HPC_BPCSHIFT)
#define hpc_btoc(x)	(((__psunsigned_t)(x)+(HPC_NBPC-1))>>HPC_BPCSHIFT)
#define hpc_btoct(x)	((__psunsigned_t)(x)>>HPC_BPCSHIFT)
#undef NSCSI_DMA_PGS			/* limit the # of DMA maps */
#define NSCSI_DMA_PGS	(64/2)
#endif

#if _USE_HPCSUBPAGE
/* Macro for extracting HPC 'sub-page' on large page sized systems */
#define HPC_PAGENUM(A)		(((__psunsigned_t)(A)&POFFMASK)&~HPC_POFFMASK)
#define HPC_PAGEMAX		HPC_PAGENUM(0xffffffff)
#endif

/*  Map `len' bytes from starting address `addr' for a SCSI DMA op.  Returns
 * the actual number of bytes mapped.
 */
int
dma_map(dmamap_t *map, caddr_t addr, int len)
{
	caddr_t taddr;
	uint npages;

	switch (map->dma_type) {
	case DMA_SCSI: {
		unsigned int incr, bytes, dotrans, npages, partial;
		unsigned int *cbp, *bcnt;
		pde_t *pde;

		/* can only deal with word aligned addresses */
		if ((__psunsigned_t)addr & 0x3)
			return(0);

		/* setup descriptor pointers */
		cbp = (uint *)(map->dma_virtaddr + HPC3_CBP_OFFSET);
		bcnt = (uint *)(map->dma_virtaddr + HPC3_BCNT_OFFSET);

		/* for wd93_go */
		map->dma_index = 0;

		if (bytes = hpc_poff(addr)) {
			if ((bytes = (HPC_NBPC - bytes)) > len) {
				bytes = len;
				npages = 1;
			}
			else
				npages = hpc_btoc(len-bytes) + 1;
		}
		else {
			bytes = 0;
			npages = hpc_btoc(len);
		}
		if (npages > (map->dma_size-1)) {
			/* reserve for HPC3_EOX_VALUE */
			npages = map->dma_size-1;
			partial = 0;
			len = (npages-(bytes>0))*HPC_NBPC + bytes;
		}
		else if (bytes == len)
			/* only one descr, and that is part of a page */
			partial = bytes;
		else
			/* partial page in last descr? */
			partial = (len - bytes) % HPC_NBPC;

		/* We either have KSEG2 or KSEG0/1/Phys, figure out now */
		if (IS_KSEG2(addr)) {
			pde = kvtokptbl(addr);
			dotrans = 1;
		}
		else {
			/* We either have KSEG0/1 or Physical */
			if (IS_KSEGDM(addr)) {
				addr = (caddr_t)KDM_TO_PHYS(addr);
			}
			dotrans = 0;
		}

		/* Generate SCSI dma descriptors for this request */
		if (bytes) {
#if !_USE_HPCSUBPAGE
			*cbp = dotrans ? pdetophys(pde++) + poff(addr)
				       : (__psunsigned_t)addr;
#else
			if (dotrans) {
				/* Use poff to keep io sub-page */
				*cbp = pdetophys(pde) | poff(addr);
				if (HPC_PAGENUM(addr) == HPC_PAGEMAX) pde++;
			}
			else
				*cbp = (__psunsigned_t)addr;
#endif
			addr += bytes;
			*bcnt = bytes;
			cbp += sizeof (scdescr_t) / sizeof (uint);
			bcnt += sizeof (scdescr_t) / sizeof (uint);
			npages--;
		}
		while (npages--) {
#if !_USE_HPCSUBPAGE
			*cbp = dotrans ? pdetophys(pde++)
				       : (__psunsigned_t)addr;
#else
			if (dotrans) {
				__psint_t io_pn = HPC_PAGENUM(addr);
				*cbp = pdetophys(pde) | io_pn;
				if (io_pn == HPC_PAGEMAX) pde++;
			}
			else
				*cbp = (__psunsigned_t)addr;
#endif
			incr = (!npages && partial) ? partial : HPC_NBPC;
			addr += incr;
			*bcnt = incr;
			cbp += sizeof (scdescr_t) / sizeof (uint);
			bcnt += sizeof (scdescr_t) / sizeof (uint);
		}

		/* set EOX flag and zero BCNT on hpc3 for 25mhz workaround
		 */
		*bcnt = HPC3_EOX_VALUE;

		/* flush descriptors back to memory so HPC gets right data */
		__dcache_wb_inval((void *)map->dma_virtaddr,
			(dmavaddr_t)(bcnt+2) - map->dma_virtaddr);

		return len;
	}

	case DMA_EISA:
		map->dma_addr = (__psunsigned_t)addr;
		npages = numpages(addr, len);

		if (IS_KSEGDM(addr)) {
			if (npages > map->dma_size)
				len = map->dma_size*NBPC - poff(addr);
			/* only need to check last page to see if we crossed */
			taddr = addr + len - 1;
		}
		else {
			if (npages > 1)
				len = NBPC - poff(addr);
			/* only 1 page per map for tlb mapped pages*/
			taddr = (caddr_t)kvtophys(addr);
		}

		/* We place the burdon on drivers to not use buffers > 256MB
		 * of high memory to DMA to.  Print a warning if we see one
		 * of these.
		 */
		if ((__psunsigned_t)taddr >= 0x30000000) {
			cmn_err(CE_WARN, "dma_map: Cannot DMA to address 0x%x "
				"for an EISA device.  "
				"Address must be < 0x30000000\n",taddr);
		}

		return (len);

	default:
		return 0;
	}
}

/*  Avoid overhead of testing for KDM each time through; this is
 * the second part of kvtophys().
 */
#define KNOTDM_TO_PHYS(X) \
	((pnum((X) - K2SEG) < (__psunsigned_t)syssegsz) ? \
		(__psunsigned_t)pdetophys((pde_t*)kvtokptbl(X))+poff(X) : \
		(__psunsigned_t)(X))

dmamaddr_t
dma_mapaddr(dmamap_t *map, caddr_t addr)
{
	if (map->dma_type != DMA_EISA)
		return 0;

	if (IS_KSEGDM(addr))
		return (KDM_TO_PHYS(addr));

	return (KNOTDM_TO_PHYS(addr));
}

void
dma_mapfree(dmamap_t *map)
{
	if (map && map->dma_type == DMA_EISA)
		kern_free(map);
}

/*  Map `len' bytes of a buffer for a SCSI DMA op.  Returns the actual
 * number of bytes mapped.
 *
 *  Only called when a SCSI request is started, unlike the other
 * machines, except when the request is larger than we can fit
 * in a single dma map.  In those cases, the offset will always be
 * page aligned, same as at the start of a request.
 * While starting addresses will always be page aligned, the last
 * page may be a partial page.
 */
int
dma_mapbp(dmamap_t *map, buf_t *bp, int len)
{
	register struct pfdat *pfd = NULL;
	register uint bytes, npages;
	register unsigned *cbp, *bcnt;
	int xoff;			/* bytes already xfered */
#if _USE_HPCSUBPAGE
	int io_pfd_cnt = HPC_PGFCTR;	/* count of HPC_NBPP sub-pages */
#endif

	/* for wd93_go */
	map->dma_index = 0;

	/* setup descriptor pointers */
	cbp = (uint *)(map->dma_virtaddr + HPC3_CBP_OFFSET);
	bcnt = (uint *)(map->dma_virtaddr + HPC3_BCNT_OFFSET);

	/* compute page offset into xfer (i.e. pages to skip this time) */
	xoff = bp->b_bcount - len;
	npages = hpc_btoct(xoff);
	while (npages--) {
#if !_USE_HPCSUBPAGE
		pfd = getnextpg(bp, pfd);
		{
#else
		if (++io_pfd_cnt >= HPC_PGFCTR) {
			pfd = getnextpg(bp, pfd);
			io_pfd_cnt = 0;
#endif
			if (pfd == NULL) {
				cmn_err(CE_WARN,
					"dma_mapbp: !pfd, bp 0x%x len 0x%x",
					bp, len);
				return -1;
			}
		}
	}

	npages = hpc_btoc(len);
	if (npages > (map->dma_size-1)) {
		/* reserve for HPC3_EOX_VALUE */
		npages = map->dma_size-1;
		len = hpc_ctob(npages);
	}
	bytes = len;

	while (npages--) {
#if !_USE_HPCSUBPAGE
		pfd = getnextpg(bp, pfd);
		{
#else
		if (++io_pfd_cnt >= HPC_PGFCTR) {
			pfd = getnextpg(bp, pfd);
			io_pfd_cnt = 0;
#endif
			if (!pfd) {
				/*  This has been observed to happen on
				 * occasion when we somehow get a 45/{48,49}
				 * interrupt, but the count hadn't dropped
				 * to 0, and therefore we try to go past the
				 * end of the page list, hitting invalid pages.
				 * Since this seems to be happening just a bit
				 * more often than I like, and it is very
				 * difficult to reproduce, for now we issue a
				 * warning and fail the request rather than
				 * panic'ing.
				 */
				cmn_err(CE_WARN, "dma_mapbp: Invalid mapping: "
						 "pfd==0, bp %x len %x",
					bp, len);
				return -1;
			}
		}

#if !_USE_HPCSUBPAGE
		*cbp = pfdattophys(pfd);
#else
		*cbp = pfdattophys(pfd) | (io_pfd_cnt*HPC_NBPP);
#endif
		*bcnt = MIN(bytes, HPC_NBPC);
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		bytes -= HPC_NBPC;
	}

	/*  Need a final descriptor with byte cnt of zero and EOX flag set
	 * for the HPC3 to work right at 25Mhz.
	 */
	*bcnt = HPC3_EOX_VALUE;

	/* flush descriptors back to memory so HPC gets right data */
	__dcache_wb_inval((void *)map->dma_virtaddr,
		(dmavaddr_t)(bcnt + 2) - map->dma_virtaddr);

	return len;
}

/*  This is a hack to make the Western Digital chip stop interrupting
 * before we turn interrupts on.  ALL IP26s have the 93B chip, and the
 * fifo with the burstmode dma, so just set the variable.
 */
void
wd93_init(void)
{
	extern int wd93burstdma, wd93cnt;
	scuzzy_t *scp;
	int adap;

	ASSERT(NSCUZZY <= SC_MAXADAP);	/* sanity check */
	wd93burstdma = 1;

	/* Two built in SCSI channels on Indigo2 */
	wd93cnt = 2;

	/* Early init, need low level before high level devices do their
	 * io_init's (needs some work for multiple SCSI on setlclvector)
	 */
	for (adap=0; adap<wd93cnt; adap++) {
		/*  Init mother board channels (must start with adap 0).
		 */
		if (wd93_earlyinit(adap) == 0) {
			scp = &scuzzy[adap];
			setlclvector(scp->d_vector,
				     (lcl_intr_func_t *)wd93intr,adap);
		}
	}
}

/*  Reset the SCSI bus. Interrupt comes back after this one.
 * This also is tied to the reset pin of the SCSI chip.
 */
void
wd93_resetbus(int adap)
{
	scuzzy_t *sc = &scuzzy[adap];

	*sc->d_ctrl = SCSI_RESET;	/* write in the reset code */
	DELAY(25);			/* hold 25 usec to meet SCSI spec */
	*sc->d_ctrl = 0;		/* no dma, no flush */
}


/*  Set the DMA direction, and tell the HPC to set up for DMA.  Have to be
 * sure flush bit no longer set.  Dma_index will be zero after dma_map()
 * is called, but may be non-zero after dma_flush, when a target has
 * re-connected partway through a transfer.  This saves us from having
 * to re-create the map each time there is a re-connect.
 */
/*ARGSUSED*/
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{
	scuzzy_t *sc = &scuzzy[map->dma_adap];

	*sc->d_nextbp = (ulong)((scdescr_t*)map->dma_addr + map->dma_index);
	*sc->d_ctrl = readflag ? SCSI_STARTDMA : (SCSI_STARTDMA|SCDIROUT);
}


/*  Allocate the per-device DMA descriptor list.  Arg is the per adapter
 * dmamap.
 */
dmamap_t *
wd93_getchanmap(dmamap_t *cmap)
{
	register scdescr_t *scp, *pscp;
	dmamap_t *map;
	int i;

	if (!cmap)
		return cmap;		/* paranoia */

	/*  An individual descriptor can't cross a page boundary, but
	 * VM_CACHEALIGN will align to TCC_LINESIZE which is good enough.
	 */
	map = (dmamap_t *)kern_malloc(sizeof(*map));
	map->dma_virtaddr = (dmavaddr_t)kmem_alloc(NSCSI_DMA_PGS*sizeof(scdescr_t),
						   VM_CACHEALIGN);

	/* so we don't have to calculate each time in dma_map */
	map->dma_addr = kvtophys((void*)map->dma_virtaddr);
	map->dma_adap = cmap->dma_adap;
	map->dma_type = DMA_SCSI;
	map->dma_size = NSCSI_DMA_PGS;

	/* fill in the next-buffer values now, we never change them */
	scp = (scdescr_t *)map->dma_virtaddr;
	pscp = (scdescr_t *)map->dma_addr + 1;
	for (i = 0; i < NSCSI_DMA_PGS; ++i, ++scp, ++pscp) {
		scp->nbp = KDM_TO_IOPHYS(pscp);
	}

	return map;
}

/*  Flush the fifo to memory after a DMA read.  OK to leave flush set till
 * next DMA starts.  SCSI_STARTDMA bit gets cleared when fifo has been
 * flushed. We 'advance' the dma map chain when we are called, so that when
 * some data was transferred, but more remains we don't have to reprogram
 * the entire descriptor chain.  When reading, this is only valid after
 * the flush has completed.  So we check the SCSI_STARTDMA bit to be sure
 * flush has completed.
 *
 * Note that when writing, the curbp can be considerably off,
 * since the WD chip could have asked for up to 15 extra bytes,
 * plus the HPC could have asked for up to 64 extra bytes.
 * Thus we might not even be on the 'correct' descriptor any
 * longer when writing.  We must therefore advance the map index
 * by calculation.  This is still faster than rebuilding the map.
 * Unlike standalone, keep the 'faster' method of figuring out where
 * we are when reading.
 *
 * Also check for a parity error after a DMA operation. Return 1 if there
 * was one. (Currently unimplemented).
 */
wd93dma_flush(dmamap_t *map, int readflag, int xferd)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned *bcnt, *cbp, index, fcnt=0;
#if LATER
	static paritychk = 0;			/* for debugger toggling */
#endif

	if (readflag && xferd) {
		unsigned cnt = 512;	/* normally only 2 or 3 times */
		fcnt = 0;
		*scp->d_ctrl |= SCSI_FLUSH;
		while ((*scp->d_ctrl & SCSI_STARTDMA) && --cnt)
			;
		if (cnt == 0)
			cmn_err(CE_PANIC,
				"SCSI DMA in progress bit never cleared\n");
	}
	else
		/*  Turn off dma bit; mainly for cases where drive 
		 * disconnects without transferring data, and HPC already
		 * grabbed the first 64 bytes into it's fifo; otherwise gets
		 * the first 64 bytes twice, since it 'notices' that curbp was
		 * reset, but not that is the same.
		 */
		*scp->d_ctrl = 0;

	/* disconnect with no i/o */
	if (!xferd)
		return 0;

	/* index from last wd93_go */
	index = map->dma_index;
	fcnt = index * sizeof (scdescr_t);
	cbp = (uint *)(map->dma_virtaddr + HPC3_CBP_OFFSET + fcnt);
	bcnt = (uint *)(map->dma_virtaddr + HPC3_BCNT_OFFSET + fcnt);

	/* handle possible partial page in first descriptor */
	fcnt = *bcnt & (HPC_NBPP | (HPC_NBPP - 1));
	if (xferd >= fcnt) {
		xferd -= fcnt;
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		++index;
	}
	if (xferd) {
		/* skip whole descriptors */
		fcnt = xferd / HPC_NBPP;
		index += fcnt;
		fcnt *= sizeof (scdescr_t) / sizeof (uint);
		cbp += fcnt;
		bcnt += fcnt;

		/* create first partial page descriptor */
		*bcnt -= hpc_poff(xferd);
		*cbp += hpc_poff(xferd);

		/* flush descriptor back to memory so HPC gets right data */
		__dcache_wb_inval((void *)
			((scdescr_t *)map->dma_virtaddr + index),
			sizeof (scdescr_t));
	}

	/* prepare for rest of dma (if any) after next reconnect */
	map->dma_index = index;

#ifdef	LATER
	if(paritychk && !readflag &&
		(data=(*(uint *)PHYS_TO_K1(PAR_ERR_ADDR) & PAR_DMA))) {
		/* note that the parity error could have been from some other
		 * devices DMA also (parallel, or DSP) */
		readflag = *(volatile int *)PHYS_TO_K1(DMA_PAR_ADDR);
		log_perr(readflag, data, 0);
		cmn_err(CE_WARN, dma_par_msg, readflag<<22);
		*(volatile u_char *)PHYS_TO_K1(PAR_CL_ADDR) = 0;	/* clear it */
		flushbus();
		return(1);
	}
#endif	/* LATER */

	return(0);
}

/*  Is a wd93 interrupt present?  In machine specific code because of IP4.
 */
wd93_int_present(wd93dev_t *dp)
{
	/* reads the aux register */
	return(*dp->d_addr & AUX_INT);
}

/*  Initialize led counter and value.
 */
void
reset_leds(void)
{
	set_leds(0);
}


/* Implementation of syssgi(SGI_SETLED,a1)
 */
void
sys_setled(int a1)
{
	set_leds(a1);
}

/*  Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to make
 * it change color.  We don't allow UNIX to turn the LED off.  Also, UNIX
 * is not allowed to turn the LED *RED* (a big no no in Europe).
 */
void
set_leds(int pattern)
{
	_hpc3_write1 &= ~(LED_AMBER|LED_GREEN);
	_hpc3_write1 |= pattern ? LED_AMBER : LED_GREEN;
	*(volatile uint *)PHYS_TO_K1(HPC3_WRITE1) = _hpc3_write1;
}

void bump_leds(void) {}

/*  Send an interrupt to another CPU -- fatal error on IP26 (UP).
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	panic("sendintr");
	/* NOTREACHED */
}

static void _clock_func(int func, char *ptr);
static void setbadclock(void), clearbadclock(void);
static int isbadclock(void);
extern u_char miniroot;

/*  Get the time/date from the ds1286.  Reinitialize with a guess from the
 * file system if necessary.
 */
void
inittodr(time_t base)
{
	register uint todr;
	long deltat;
	int TODinvalid = 0;
	extern int todcinitted;
	int s;

	s = mutex_spinlock_spl(&timelock, splprof);

	todr = rtodc();		/* returns seconds from 1970 00:00:00 GMT */
	settime(todr,0);

	if (miniroot) {		/* no date checking if running from miniroot */
		mutex_spinunlock(&timelock, s);
		return;
	}

	/*  Complain if the TOD clock is obviously out of wack. It
	 * is "obvious" that it is screwed up if the clock is behind
	 * the 1988 calendar currently on my wall.  Hence, TODRZERO
	 * is an arbitrary date in the past.
	 */
	if (todr < TODRZERO) {
		if (todr < base) {
			cmn_err(CE_WARN, "lost battery backup clock--"
				"using file system time");
			TODinvalid = 1;
		}
		else {
			cmn_err(CE_WARN, "lost battery backup clock");
			setbadclock();
		}
	}

	/* Complain if the file system time is ahead of the time
	 * of day clock, and reset the time.
	 */
	if (todr < base) {
		if (!TODinvalid)	/* don't complain twice */
			cmn_err(CE_WARN, "time of day clock behind file system"
				" time--resetting time");
		settime(base, 0);
		resettodr();
		setbadclock();
	}
	todcinitted = 1; 	/* for chktimedrift() in clock.c */

	/* See if we gained/lost two or more days: 
	 * If so, assume something is amiss.
	 */
	deltat = todr - base;
	if (deltat < 0)
		deltat = -deltat;
	if (deltat > 2*SECDAY)
		cmn_err(CE_WARN,"clock %s %d days",
		    time < base ? "lost" : "gained", deltat/SECDAY);

	if (isbadclock())
		cmn_err(CE_WARN, "CHECK AND RESET THE DATE!");

	mutex_spinunlock(&timelock, s);
}

/*  Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */
void
resettodr(void)
{
	wtodc();
}

static int month_days[12] = {
	31,	/* January */
	28,	/* February */
	31,	/* March */
	30,	/* April */
	31,	/* May */
	30,	/* June */
	31,	/* July */
	31,	/* August */
	30,	/* September */
	31,	/* October */
	30,	/* November */
	31	/* December */
};

static void _clock_func(int func, char *ptr);
static int dallas_yrref = DALLAS_YRREF;

/*  Read time of day clock.
 */
int
rtodc(void)
{
	uint month, day, year, hours, mins, secs;
	register int i;
	char buf[7];

	_clock_func(WTR_READ, buf);

	secs = (uint)(buf[0] & 0xf);
	secs += (uint)(buf[0] >> 4) * 10;

	mins = (uint)(buf[1] & 0xf);
	mins += (uint)(buf[1] >> 4) * 10;

	/* We use 24 hr mode, so bits 4 and 5 represent the 2 tens-hour bits.
	 */
	hours = (uint)(buf[2] & 0xf);
	hours += (uint)( ( buf[2] >> 4 ) & 0x3 ) * 10;

	/* Actually day of month.
	 */
	day = (uint)(buf[3] & 0xf);
	day += (uint)(buf[3] >> 4) * 10;

	month = (uint)(buf[4] & 0xf);
	month += (uint)(buf[4] >> 4) * 10;

	year = (uint)(buf[5] & 0xf);
	year += (uint)(buf[5] >> 4) * 10;
	/*  Up till now, we stored the year as years since 1970.  This confuses
	 * dallas clock wrt leap year.  It will think 1994 is a leap year,
	 * instead of 1996.  If we appear to still be using YRREF as a base,
	 * update it to DALLAS_YRREF.
	 *
	 *  This shouldn't happen for teton, but we have to deal with R4k
	 * upgrades...Also do not update the timebase in the miniroot.
	 */
	if (year < 45) {		/* 5.2 -> 13.7 in 2016 will break */
		year += (YRREF - DALLAS_YRREF);
		if (!miniroot) {
			buf[5] = (char)( ((year/10)<<4) | (year%10) );
			_clock_func(WTR_WRITE, buf);
		}
		else
			dallas_yrref = YRREF;	/* keep timebase in miniroot */
	}
	year += DALLAS_YRREF;

	/* Sum up seconds from 00:00:00 GMT 1970
	 */
	secs += mins * SECMIN;
	secs += hours * SECHOUR;
	secs += (day-1) * SECDAY;

	if (LEAPYEAR(year))
		month_days[1]++;
	for (i = 0; i < month-1; i++)
		 secs += month_days[i] * SECDAY;
	if (LEAPYEAR(year))
		month_days[1]--;

	while (--year >= YRREF) {
		secs += SECYR;
		if (LEAPYEAR(year))
			secs += SECDAY;
	}

	return(secs);
}

/*  Write time of day clock.
 */
void
wtodc(void)
{
	register long year_secs = time;
	register month, day, hours, mins, secs, year, dow;
	char buf[7];

	/* calculate the day of the week
	 */
	dow = get_dayof_week(year_secs);

	/* Whittle the time down to an offset in the current year,
	 * by subtracting off whole years as long as possible.
	 */
	year = YRREF;
	for (;;) {
		register secyr = SECYR;
		if (LEAPYEAR(year))
			secyr += SECDAY;
		if (year_secs < secyr)
			break;
		year_secs -= secyr;
		year++;
	}

	/* Break seconds in year into month, day, hours, minutes, seconds.
	 */
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 0; year_secs >= month_days[month]*SECDAY;
	     year_secs -= month_days[month++]*SECDAY)
		continue;
	month++;
	if (LEAPYEAR(year))
		month_days[1]--;

	for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
		continue;

	for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
		continue;

	for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
		continue;

	secs = year_secs;

	buf[0] = (char)( ((secs/10) << 4) | (secs%10) );

	buf[1] = (char)( ((mins/10) << 4) | (mins%10) );

	buf[2] = (char)( ((hours/10) << 4) | (hours%10) );

	buf[3] = (char)( ((day/10) << 4) | (day%10) );

	buf[4] = (char)( ((month/10) << 4) | (month%10) );

	/*  The number of years since DALLAS_YRREF (1940) is what actually
	 * gets stored in the clock.  Leap years work on the ds1286 because
	 * the hardware handles the leap year correctly.
	 */
	year -= dallas_yrref;
	buf[5] = (char)( ((year/10) << 4) | (year%10) );

	/* Now that we are done with year stuff, plug in day_of_week
	 */
	buf[6] = (char)dow;

	_clock_func(WTR_WRITE, buf);

	/* clear the flag in nvram that says whether or not the clock
	 * date is correct or not.
	 */
	clearbadclock();
}

static void
_clock_func(int func, char *ptr)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();

	if (func == WTR_READ) {
		clock->command &= ~WTR_TE;		/* freeze external */

		/* read data */
		*ptr++ = (char)clock->sec;
		*ptr++ = (char)clock->min;
		*ptr++ = (char)clock->hour & 0x3f;
		*ptr++ = (char)clock->date & 0x3f;
		*ptr++ = (char)clock->month & 0x1f;
		*ptr++ = (char)clock->year;
		*ptr++ = (char)clock->day & 0x7;

	} else {
		/* make sure clock oscilltor is going. */
		clock->month &= ~WTR_EOSC_N;

		clock->command &= ~WTR_TE;		/* freeze external */

		/* write data */
		clock->sec = *ptr++;
		clock->min = *ptr++;
		clock->hour = *ptr++ & 0x3f;		/* make sure 24 hr */
		clock->date = *ptr++;
		clock->month = *ptr++;
		clock->year = *ptr++;
		clock->day = *ptr++;
	}

	clock->command |= WTR_TE;			/* thaw external */

	splx(s);
}

static void
setbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();
	clock->ram[RT_RAM_FLAGS] |= RT_FLAGS_INVALID;
	splx(s);
}

static void
clearbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int s = splhi();
	clock->ram[RT_RAM_FLAGS] &= ~RT_FLAGS_INVALID;
	splx(s);
}

static int
isbadclock(void)
{
	register ds1286_clk_t *clock = RTDS_CLOCK_ADDR;
	int rc, s = splhi();
	rc = clock->ram[RT_RAM_FLAGS] & RT_FLAGS_INVALID;
	splx(s);

	return(rc);
}

/*  Called from prom_reinit(), in case syssgi(SET_AUTOPWRON) was done.
 * If time of the day alarm vars are to be set then program RTC to
 * power system back on using time-of-day alarm, unless the current
 * time is already past that value.
 */
void
set_autopoweron(void)
{
	register ds1286_clk_t *clock = (ds1286_clk_t *) RTDS_CLOCK_ADDR;
	int month, day, hours, mins, year, day_of_week, command;
	extern time_t autopoweron_alarm;
	long year_secs;

	if((unsigned long)time > (unsigned long)autopoweron_alarm)
		return;	/* not requested, or requested time is already past */

	year_secs = autopoweron_alarm;

	/* calculate the day of the week 
	 */
	day_of_week = get_dayof_week(year_secs);

	/* Whittle the time down to an offset in the current year, by
	 * subtracting off whole years as long as possible. 
	 */
	year = YRREF;
	for (;;) {
		register secyr = SECYR;
		if (LEAPYEAR(year))
			secyr += SECDAY;
		if (year_secs < secyr)
			break;
		year_secs -= secyr;
		year++;
	}

	/* Break seconds in year into month, day, hours, minutes,
	 * seconds 
	 */
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 0;
	     year_secs >= month_days[month] * SECDAY;
	     year_secs -= month_days[month++] * SECDAY)
		continue;
	month++;
	if (LEAPYEAR(year))
		month_days[1]--;

	for (day = 1; year_secs >= SECDAY; day++, year_secs -= SECDAY)
		continue;

	for (hours = 0; year_secs >= SECHOUR; hours++, year_secs -= SECHOUR)
		continue;

	for (mins = 0; year_secs >= SECMIN; mins++, year_secs -= SECMIN)
		continue;

	if (!((mins > 59) || (hours > 23) || (day_of_week > 7))) {
		/* Set alarm to activate when minutes, hour and day match.
		 */
		clock->min_alarm = (char)( ((mins/10) << 4) | (mins%10) );
		clock->hour_alarm = (char)( ((hours/10) << 4) | (hours%10) );
		clock->day_alarm = (char)( ((day_of_week/10) << 4) |
					    (day_of_week%10) );

		command = clock->command;
		command |= WTR_TIME_DAY_INTA;		/* Enable INTA */
		command &= ~WTR_DEAC_TDM;		/* Activate TDM */
		clock->command = command;
	}
}

/* Converts year_secs to the day of the week (i.e. Sun = 1 and Sat = 7).
 */
static int
get_dayof_week(time_t year_secs)
{
        int days,yr=YRREF;
        int year_day;
#define EXTRA_DAYS      3		/* used to get day of week */

        days = 0;
        for (;;) {
                register secyear = SECYR;
                if (LEAPYEAR(yr))
                        secyear += SECDAY;

                if (year_secs >=secyear)
                        year_day = secyear;
                else
                        year_day = year_secs;
                for (;;) {
                        if (year_day < SECDAY)
                                break;

                        year_day -= SECDAY;
                        days++;
                }

                if (year_secs < secyear)
                        break;
                year_secs -= secyear;
                yr++;
        }

        if (days > EXTRA_DAYS) {
                days -=EXTRA_DAYS;
                days %= 7;
                days %= 7;
        }
	else
                days +=EXTRA_DAYS+1;
        return(days+1);

}

/*  Called from clock() periodically to see if system time is drifting from
 * the real time clock chip.  Only do if different by two seconds or more.
 * One second difference often occurs due to slight differences between
 * kernel and chip--two to play it safe.
 */
void
chktimedrift(void)
{
	long todtimediff;

	todtimediff = rtodc() - time;
	if ((todtimediff > 2) || (todtimediff < -2))
		(void)doadjtime(todtimediff * USEC_PER_SEC);
}

/*  Fetch the nvram table from the PROM.  The table is static because we
 * can not call kern_malloc yet.  We can at least tell if the actual table
 * is larger than we think by the return value.
 *
 * NOTE: The actual size of the table, including the null entry at the
 *	 end is 30 members as of 9/94.
 */
static struct nvram_entry nvram_tab[31];

void
load_nvram_tab(void)
{
	if (arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab)) > 0)
		cmn_err(CE_WARN,"IP26 nvram table has grown!\n");
}

/*  Assign different values to cpu_auxctl to access different NVRAMs.  This
 * allows us to use NVRAMs on GIO cards.
 */
volatile unchar *cpu_auxctl;
#define	CPU_AUXCTL	cpu_auxctl

/*  Enable the serial memory by setting the console chip select.
 */
static void
nvram_cs_on(void)
{
	*CPU_AUXCTL &= ~CPU_TO_SER;
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~NVRAM_PRE;
	DELAY(1);
	*CPU_AUXCTL |= CONSOLE_CS;
	*CPU_AUXCTL |= SERCLK;
}

/*  Turn off the chip select.
 */
static void
nvram_cs_off(void)
{
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~CONSOLE_CS;
	*CPU_AUXCTL |= NVRAM_PRE;
	*CPU_AUXCTL |= SERCLK;
}

/*  Clock in the nvram command and the register number.  For the national
 * semi conducter NVRAM chip the op code is 3 bits and the address is
 * 6/8 bits. 
 */
#define	BITS_IN_COMMAND	11
static void
nvram_cmd(uint cmd, uint reg)
{
	ushort ser_cmd;
	int i;

	ser_cmd = cmd | (reg << (16 - BITS_IN_COMMAND));
	for (i = 0; i < BITS_IN_COMMAND; i++) {
		if (ser_cmd & 0x8000)	/* if high order bit set */
			*CPU_AUXCTL |= CPU_TO_SER;
		else
			*CPU_AUXCTL &= ~CPU_TO_SER;
		*CPU_AUXCTL &= ~SERCLK;
		*CPU_AUXCTL |= SERCLK;
		ser_cmd <<= 1;
	}
	*CPU_AUXCTL &= ~CPU_TO_SER;	/* see data sheet timing diagram */
}

/*  After write/erase commands, we must wait for the command to complete.
 * Write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *
 * 	NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */
static
nvram_hold(void)
{
	int error = 0;
	int timeout = NVDELAY_LIMIT;

	nvram_cs_on();

	while (!(*CPU_AUXCTL & SER_TO_CPU) && timeout--)
		DELAY(NVDELAY_TIME);

	if (!(*CPU_AUXCTL & SER_TO_CPU))
		error = -1;

	nvram_cs_off();

	return (error);
}



/*  Read a 16 bit register from non-volatile memory.  Bytes are stored in
 * this string in big-endian order in each 16 bit word.
 */
ushort
get_nvreg(int nv_regnum)
{
	ushort ser_read = 0;
	int i;

	*CPU_AUXCTL &= ~NVRAM_PRE;
	nvram_cs_on();			/* enable chip select */
	nvram_cmd(SER_READ, nv_regnum);

	/* clock the data ouf of serial mem */
	for (i = 0; i < 16; i++) {
		*CPU_AUXCTL &= ~SERCLK;
		*CPU_AUXCTL |= SERCLK;
		ser_read <<= 1;
		ser_read |= (*CPU_AUXCTL & SER_TO_CPU) ? 1 : 0;
	}
		
	nvram_cs_off();

	return(ser_read);
}


/*  Writes a 16 bit word into non-volatile memory.  Bytes
 *  are stored in this register in big-endian order in each 16 bit word.
 */
static int
set_nvreg(int nv_regnum, unsigned short val)
{
	int error;
	int i;

	*CPU_AUXCTL &= ~NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0);	
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_WRITE, nv_regnum);

	/* Clock the data into serial mem.
	 */
	for (i = 0; i < 16; i++) {
		if (val & 0x8000)	/* pull the bit out of high order pos */
			*CPU_AUXCTL |= CPU_TO_SER;
		else
			*CPU_AUXCTL &= ~CPU_TO_SER;
		*CPU_AUXCTL &= ~SERCLK;
		*CPU_AUXCTL |= SERCLK;
		val <<= 1;
	}
	*CPU_AUXCTL &= ~CPU_TO_SER;	/* see data sheet timing diagram */
		
	nvram_cs_off();
	error = nvram_hold();

	nvram_cs_on();
	nvram_cmd(SER_WDS, 0);
	nvram_cs_off();

	return (error);
}

static char
nvchecksum(void)
{
	register signed char checksum;
	register int nv_reg;
	ushort nv_val;

	/* Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
	 * (all-ones) checksum.
	 */
	checksum = 0xa5;

	/* Do the checksum on all of the nvram -- skip the checksum byte!!
	 */
	for(nv_reg = 0; nv_reg < NVLEN_MAX / 2; nv_reg++) {
		nv_val = get_nvreg(nv_reg);
		if(nv_reg == (NVOFF_CHECKSUM / 2))
#if NVOFF_CHECKSUM & 1
				checksum ^= nv_val >> 8;
#else
				checksum ^= nv_val & 0xff;
#endif
		else
			checksum ^= (nv_val >> 8) ^ (nv_val & 0xff);
		/* following is a tricky way to rotate */
		checksum = (checksum << 1) | (checksum < 0);
	}

	return (char)checksum;
}

/* Write string to non-volatile memory.
 */
static
set_an_nvram(register int nv_off, register int nv_len, register char *string)
{
	unsigned short curval;
	char checksum[2];
	int nv_off_save; 
	int i;

	nv_off_save = nv_off;

	if(nv_off % 2 == 1 && nv_len > 0) {
		curval  = get_nvreg(nv_off / 2);
		curval &= 0xff00;
		curval |= *string;
		if (set_nvreg(nv_off / 2, curval))
			return (-1);
		string++;
		nv_off++;
		nv_len--;
	}

	for (i = 0; i < nv_len / 2; i++) {
		curval = (unsigned short) *string++ << 8;
		curval |= *string;
		string++;
		if (set_nvreg(nv_off / 2 + i, curval))
			return (-1);
	}

	if (nv_len % 2 == 1) {
		curval = get_nvreg((nv_off + nv_len) / 2);
		curval &= 0x00ff;
		curval |= (unsigned short) *string << 8;
		if (set_nvreg((nv_off + nv_len) / 2, curval))
			return (-1);
	}

	if (nv_off_save != NVOFF_CHECKSUM) {
		checksum[0] = nvchecksum();
		checksum[1] = 0;
		return (set_an_nvram(NVOFF_CHECKSUM, NVLEN_CHECKSUM, checksum));
	}
	else
		return (0);
}

#ifdef MFG_EADDR
/*
 * set_eaddr - set ethernet address in prom
 */
#define TOHEX(c) ((('0'<=(c))&&((c)<='9')) ? ((c)-'0') : ((c)-'a'+10))
int
set_eaddr (char *value)
{
	char digit[6], *cp;
	int dcount;

	/* expect value to be the address of an ethernet address string
	 * of the form xx:xx:xx:xx:xx:xx (lower case only)
	 */
	for (dcount = 0, cp = value; *cp ; ) {
		if (*cp == ':') {
			cp++;
			continue;
		}
		else if (!ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1))) {
			return -1;
		}
		else {
			if (dcount >= 6)
				return -1;
			digit[dcount++] = (TOHEX(*cp)<<4) + TOHEX(*(cp+1));
			cp += 2;
		}
	}

	cpu_auxctl = (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL);

	for (dcount = 0; dcount < NVLEN_ENET; ++dcount)
		if (set_an_nvram(NVOFF_ENET+dcount, 1, &digit[dcount]))
			return -1;

	return 0;
}
#endif /* MFG_EADDR */


/*  Set nv ram variable name to value.  All the real work is done in
 * set_an_nvram().  There is no get_nvram because the code that
 * would use it always gets it from kopt_find instead.
 *
 * This is called from syssgi().
 *
 * Negative return values indicate failure.
 */
int
set_nvram(char *name, char *value)
{
	register struct nvram_entry *nvt;
	int valuelen = strlen(value);
	char _value[20];
	int _valuelen=0;

	if(!strcmp("eaddr", name))
#ifdef MFG_EADDR
		return set_eaddr(value);
#else
		return -2;
#endif /* MFG_EADDR */

	/* Don't allow setting the password from Unix, only clearing. */
	if (!strcmp(name, "passwd_key") && valuelen)
		return -2;

	/* change the netaddr to the nvram format */
	if (strcmp(name, "netaddr") == 0) {
		char buf[4];
		char *ptr = value;
		int i;

		strncpy(_value, value, 19);
		value[19] = '\0';
		_valuelen = valuelen;

		/* to the end of the string */
		while (*ptr)
			ptr++;

		/* convert string to number, one at a time */
		for (i = 3; i >= 0; i--) {
			while (*ptr != '.' && ptr >= value)
				ptr--;
			buf[i] = atoi(ptr + 1);
			if (ptr > value)
				*ptr = 0;
		}
		value[0] = buf[0];
		value[1] = buf[1];
		value[2] = buf[2];
		value[3] = buf[3];
		valuelen = 4;
	}

	cpu_auxctl = (volatile unchar *)PHYS_TO_K1(CPU_AUX_CONTROL);

	/* check to see if it is a valid nvram name */
	for(nvt = nvram_tab; nvt->nt_nvlen; nvt++)
		if(!strcmp(nvt->nt_name, name)) {
			int status;

			if(valuelen > nvt->nt_nvlen)
				return NULL;
			if(valuelen < nvt->nt_nvlen)
				++valuelen;	/* write out NULL */
			status = set_an_nvram(nvt->nt_nvaddr, valuelen, value);

			/* reset value if changed */
			if (_valuelen)
				strcpy(value, _value);

			return status;
		}

	return -1;
}

void
init_all_devices(void)
{
        uphwg_init(hwgraph_root);
}

/*  Do the following initialization to make sure that a wrong config
 * in system.gen won't cause the kernel to hang.  Basically clears
 * some of the MP device locks.
 *
 *  Also initialize ICache PID management here.  This is a little
 * goofy, but it is done out of allowboot() on IP21, and mlreset()
 * is too early since PDAs are not set-up.
 */
void
allowboot(void)
{
	register int i;	

	for (i=0; i<bdevcnt; i++)
		bdevsw[i].d_cpulock = 0;

	for (i=0; i<cdevcnt; i++)
		cdevsw[i].d_cpulock = 0;

	icacheinfo_init();
}

/* Return address of exception handler for all exceptions other	then utlbmiss.
 */
inst_t *
get_except_norm(void)
{
	extern inst_t exception[];

	return exception;
}

/* return board revision level (0-7)
 */
int
board_rev(void)
{
	return ( (*(volatile uint *)PHYS_TO_K1(HPC3_SYS_ID) & BOARD_REV_MASK)
		 >> BOARD_REV_SHIFT );
}

void
add_ioboard(void)	/* should now be add_ioboards() */
{
	add_to_inventory(INV_BUS,INV_BUS_EISA,0,0,4);
}

void
add_cpuboard(void)
{
	int cpuid = cpuid();

	add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);

	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid, 0,
			 private.p_fputype_word);
	if ((private.p_cputype_word & 0xff) < 0x22)
		cmn_err(CE_WARN, "CPU is downrev.  Not for customer use.\n");
	if (!ip26_isecc())
		cmn_err(CE_WARN, "CPU baseboard downrev (IP22 not IP26).  Not for customer use.\n");
	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid, 0,
			 private.p_cputype_word);
	/* for type INV_CPUBOARD, ctrl is CPU board type, unit = -1 */
	add_to_inventory(INV_PROCESSOR,INV_CPUBOARD, private.cpufreq, -1,
			INV_IP26BOARD);
}

int
cpuboard(void)
{
	return INV_IP26BOARD;
}

/*  On IP26, unix overlays the prom's bss area.  Therefore, only
 * those prom entry points that do not use prom bss or are 
 * destructive (i.e. reboot) may be called.  There is however,
 * a period of time before the prom bss is overlayed in which
 * the prom may be called.  After unix wipes out the prom bss
 * and before the console is inited, there is a very small window
 * in which calls to dprintf will cause the system to hang.
 *
 * If symmon is being used, it will plug a pointer to its printf
 * routine into the restart block which the kernel will then use
 * for dprintfs.  The advantage of this is that the kp command
 * will not interfere with the kernel's own print buffers.
 *
 */
# define ARGS	a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15
void
dprintf(fmt,ARGS)
char *fmt;
long ARGS;
{
	/* Check for presence of symmon.
	 */
	if ( SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf )
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	else if (cn_is_inited())	/* imples PROM bss is gone */
		cmn_err(CE_CONT,fmt,ARGS);
	else				/* try thru PROM */
		arcs_printf (fmt,ARGS);
}

int
readadapters(uint_t bustype)
{
	switch (bustype) {
		
	case ADAP_SCSI:
		return(wd93cnt);

	case ADAP_LOCAL:
		return(1);

	case ADAP_EISA:
		return(1);

	default:
		return(0);

	}
}

piomap_t*
pio_mapalloc(uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
	piomap_t *piomap;
	int i;

	if (bus < ADAP_GFX || bus > ADAP_EISA)
		return(0);

	/* Allocates a handle */

	piomap = (piomap_t*) kern_calloc(1, sizeof(piomap_t));

	/* fills the handle */

	piomap->pio_bus		= bus;
	piomap->pio_adap	= adap;
	piomap->pio_type	= iospace->ios_type;
	piomap->pio_iopaddr	= iospace->ios_iopaddr;
	piomap->pio_size	= iospace->ios_size;
	piomap->pio_flag	= flag;
	piomap->pio_reg		= PIOREG_NULL;
        for (i = 0; i < 7; i++)
		if (name[i])
			piomap->pio_name[i] = name[i];

	if (bus == ADAP_EISA)
		switch (iospace->ios_type) {
		case PIOMAP_EISA_IO:
			piomap->pio_vaddr =
				(caddr_t)EISAIO_TO_K1(iospace->ios_iopaddr);
			break;

		case PIOMAP_EISA_MEM:
			piomap->pio_vaddr =
				(caddr_t)EISAMEM_TO_K1(iospace->ios_iopaddr);
			eisa_refresh_on();
			break;

		default:
			kern_free(piomap);
			return 0;
		}

	return(piomap);
}

caddr_t 
pio_mapaddr(piomap_t* piomap, iopaddr_t addr)
{
	switch (piomap->pio_bus) {
	case ADAP_GIO:
		return((caddr_t)PHYS_TO_K1(addr));

	default:
		return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
	}
}

void
pio_mapfree(piomap_t* piomap)
{
	if (piomap)
		kern_free(piomap);
}

/*  Compute the number of picosecs per cycle counter tick.
 *
 * NOTE: If the MC clock does not divide nicely into 2000000,
 *	 the cycle count will not be exact.  On teton we should
 *	 always be able to run the MC at 50.0 Mhz regardless of
 *	 TFP speed (TCC is an asychronous boundry).
 */
__psint_t
query_cyclecntr(uint *cycle)
{
	*cycle = 20000;				/* 50Mhz == 20ns */
	return PHYS_TO_K1(TCC_COUNT+4);		/* set-up word access! */
}
/*
 * This routine returns the value of the cycle counter. It needs to
 * be the same counter that the user sees when mapping in the CC
 */
__int64_t
absolute_rtc_current_time()
{
	__int64_t time;
	time =  *(__int64_t *) PHYS_TO_K1(TCC_COUNT);
	return(time);
}


/* Keyboard bell using Intel EISA chipset 8254 timer.
 */
#define MAXFREQ		12500
#define FQVAL(v)	(1193000/(v))
#define FQVAL_MSB(v)	(((v)&0xff00)>>8)
#define FQVAL_LSB(v)	((v)&0xff)

#define MAXBELLQ	32			/* must be a power of 2 */
#define pckbd_qnext(X)	(((X)+1) & (~MAXBELLQ))

#define INB(x)		(*(volatile u_char *)(EISAIO_TO_K1(x)))
#define OUTB(x,y)	(*(volatile u_char *)(EISAIO_TO_K1(x)) = (y))

struct {
	unsigned short pitch;
	unsigned short duration;
} bellq[MAXBELLQ];

static void start_bell(int pitch, int duration, int notimeout);

static struct {
	int head;			/* head of bellq */
	int tail;			/* tail of bellq */
	int pitch;			/* current pitch */
	toid_t tid;			/* timeout id */
	int flags;
#define BELLASLEEP	0x01
} bell;

int bellok;				/* needed by csu.s */

void
kbdbell_init(void)
{
	unsigned char tmp;

	/* No bell if EIU missing */
	if (badaddr((volatile void *)EISAIO_TO_K1(IntTimer1CommandMode),1))
		return;

	OUTB(IntTimer1CommandMode, (EISA_CTL_SQ|EISA_CTL_LM|EISA_CTL_C2));
	tmp = INB(NMIStatus);
	OUTB(NMIStatus,tmp|EISA_C2_ENB);

	
	bell.flags = bell.head = bell.tail = 0;
	bellok = 1;
}

static void
quiet_bell(void)
{
	unsigned char tmp;

	/* turn off speaker. */
	tmp = INB(NMIStatus);
	tmp &= ~EISA_SPK_ENB;
	OUTB(NMIStatus,tmp);
}

/*ARGSUSED*/
static void
stop_bell(int arg)
{
	int s = splhi();

	quiet_bell();

	bell.tail = pckbd_qnext(bell.tail);
	if (bell.tail != bell.head) {
		start_bell(bellq[bell.tail].pitch,bellq[bell.tail].duration,0);
	}
	if (bell.flags & BELLASLEEP) {
		bell.flags &= ~BELLASLEEP;
		wakeup(bellq);
		splx(s);
		return;
	}
	splx(s);
}

static void
start_bell(int pitch, int duration, int notimeout)
{
	unsigned char tmp;
	int val;

	ASSERT(pitch);

	/* if changing the pitch, re-write hardware.
	 */
	if (pitch != bell.pitch) {
		bell.pitch = pitch;
		val = FQVAL(pitch);
		OUTB(IntTimer1SpeakerTone,FQVAL_LSB(val));
		flushbus();
		OUTB(IntTimer1SpeakerTone,FQVAL_MSB(val));
		flushbus();
	}

	/* approximate duration ms in 1/HZs, and enable generator.
	 */
	if (!notimeout)
		bell.tid = timeout(stop_bell,0,(duration*HZ)>>10);
	tmp = INB(NMIStatus);
	OUTB(NMIStatus,tmp|EISA_SPK_ENB);
}

/*ARGSUSED*/
int
pckbd_bell(int volume, int pitch, int duration)
{
	int last, new, s;
	unsigned int sum;

	if (!bellok || !pitch || !duration)
		return 0;

	if (pitch > MAXFREQ)
		pitch = MAXFREQ;

	s = splhi();

	/* Insure queue space, or sleep until space (or signal).
	 */
	if ((new=pckbd_qnext(bell.head)) == bell.tail) {
		bell.flags |= BELLASLEEP;
		if (sleep(bellq,PZERO)) {
			splx(s);
			return 0;
		}
	}

	/* If bell inactive, start it (and enqueue junk) else enqueue data.
	 */
	if (bell.head == bell.tail) {
		start_bell(pitch, duration, 0);
		bellq[bell.head].pitch = pitch;
	}
	else {
		/*  Try to coalesce with last entry, and handle back-off
		 * if coalese case, and if current bell matches frequency.
		 * We back off the duration since SGI keyboard bells dont
		 * seem to always ring if re-rung while on.
		 */
		last = bell.head - 1;
		if ((last >= 0) && (bellq[last].pitch == pitch)) {
			if (!(duration >>= 4)) duration = 1;
			if ((last != bell.tail)) {
				sum = bellq[last].duration + duration;
				if (sum < 0xffff) {
					bellq[last].duration = sum;
					goto done;
				}
			}
		}

		bellq[bell.head].duration = duration;
		bellq[bell.head].pitch = pitch;
	}

	bell.head = new;

done:
	splx(s);
	return(1);
}

/*  Make a quick beep to ack the power button press.  Override any
 * current bells temporarily.
 */
static void
power_bell(void)
{
	/*  Toss any current bell, and queue, then play a short beep.
	 * We need to explictly wait since switch debouncing and
	 * power double click make using timeout() impossible.
	 */
	if (bell.head != bell.tail)
		untimeout(bell.tid);
	bell.head = pckbd_qnext(bell.tail = 0);
	quiet_bell();
	start_bell(500, 0, 1);
	DELAY(50000);
	quiet_bell();
}

/* stubs for if_ec2.c that were in the duart for Indigo */
u_int enet_collision;
int enet_carrier_on() { return 1; }
void reset_enet_carrier() {}

/*  Set-up the timestamp units based on the C0_COUNT register.
 *
 *	timer_freq 	- frequency of the 32 bit counter timestamping source.
 *	timer_high_unit	- ???
 *
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 *
 * NOTE: Must be called early, before main().
 */
void
timestamp_init(void)
{
	extern int findcpufreq(void);

	timer_freq = findcpufreq() * 1000000;
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;		/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}

int
cpu_isvalid(int cpu)
{
	return (cpu == 0 && (pdaindr[cpu].CpuId != -1));
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

	us = (s=*t) + (us*50);

	if (us < s) {				/* handle wrap */
		while (*t >= s)
			;
		s = 0;
	}

	while ((v=*t) <= (unsigned long)us && (v > s))
		;
}

/*  Routines to manipulate the TCC fifo controls.  Used by the graphics drivers.
 *
 *	settccfifos(lw,hw)
 *		- Set tcc low and high water fifo levels.
 *	enabletccintrs(mask)
 *		- Enables [disables] HW/LW interrupts.  Valid mask bits
 *		  are INTR_FIFO_HW and INTR_FIFO_LW (SW converts to *_EN).
 *	cleartccintrs(mask)
 *		- Clears the HW/LW interrupts on in mask.  Valid mask bits
 *		  are INTR_FIFO_HW and INTR_FIFO_LW.
 */
#define	TCC_INTR_KEEP	(INTR_TIMER_EN|INTR_BUSERROR_EN|INTR_MACH_CHECK_EN)
void
settccfifos(int lw, int hw)
{
	volatile long *tcc_fifo = (volatile long *)PHYS_TO_K1(TCC_FIFO);

	*tcc_fifo = (lw << FIFO_LW_SHIFT) | (hw << FIFO_HW_SHIFT) |
		    (FIFO_INPUT_EN|FIFO_OUTPUT_EN);
}

void
enabletccintrs(int mask)
{
	volatile long *tcc_intr = (volatile long *)PHYS_TO_K1(TCC_INTR);
	long current = *tcc_intr;

	current &= TCC_INTR_KEEP;	/* do not clearr pending interrupts */
	current |= (mask<<5);		/* convert HW -> HW_EN */
	*tcc_intr = current;
}

void
cleartccintrs(int mask)
{
	volatile long *tcc_intr = (volatile long *)PHYS_TO_K1(TCC_INTR);
	long current = *tcc_intr;

	current &= TCC_INTR_KEEP|INTR_FIFO_LW_EN|INTR_FIFO_HW_EN;
	current |= mask;
	*tcc_intr = current;
}

/* Clear TCC bus errors/machine checks.
 * 	- arg == errors to clear INTR_BUSERROR | INTR_MACH_CHECK.
 */
static void
cleartccerrors(long arg)
{
	volatile long *tcc_error = (volatile long *)PHYS_TO_K1(TCC_ERROR);
	volatile long *tcc_intr  = (volatile long *)PHYS_TO_K1(TCC_INTR);

	*tcc_intr &= (0xf800 | arg);
	*tcc_error = arg >> 9;		/* shift TCC_INTR bits -> TCC_ERROR */
	flushbus();
}


/* Turn on SysAD parity checking.  This hooks into the name used by the
 * _MEM_PARITY_WAR code for the R4X00.  Teton can always have parity
 * checking on, except during GIO probing.
 */
void
perr_init(void)
{
	check_mmmap();		/* hook here so we get called main */

	*(volatile uint *)PHYS_TO_K1(CPUCTRL0) &= ~CPUCTRL0_R4K_CHK_PAR_N;
	flushbus();
}

#if DEBUG
uint
tlbdropin(unsigned char *tlbpidp, caddr_t vaddr, pte_t pte)
{
	uint _tlbdropin(unsigned char *, caddr_t, pte_t);

	ASSERT(pte.pte_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT) || \
	       pte.pte_cc == (PG_NONCOHRNT >> PG_CACHSHFT)    || \
	       pte.pte_cc == (PG_UNC_PROCORD >> PG_CACHSHFT));
	return(_tlbdropin(tlbpidp,vaddr,pte));
}
#endif /* DEBUG */

/* Enable/disable SysAD parity checking through MC.
 *
 * Include these routines for teton for GIO cards that don't always
 * driver 32/64-bits on the GIO bus and cause parity errors on PIO
 * reads.  It's also needed for later driver compatability with IP22.
 */
static int sysad_parity_enabled;
int
is_sysad_parity_enabled(void)
{
    return sysad_parity_enabled;
}

void
enable_sysad_parity(void)
{
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    *cpuctrl0 &= ~CPUCTRL0_R4K_CHK_PAR_N;
    sysad_parity_enabled = 1;
    flushbus();
}

void
disable_sysad_parity(void)
{
    volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);
    *cpuctrl0 |= CPUCTRL0_R4K_CHK_PAR_N;
    sysad_parity_enabled = 0;
    flushbus();
}

/* Some global data related to ECC parts and error handling.
 */

#ifdef DEBUG
/* In the follwing cases, the "ignore" variables really mean "don't panic"
 *  and just print an error message instead.  Sort of like nofault code.
 */
int ignore_uc_write=0; /* Ignore an uncached write in fast mode error */

/* Flags set by the interrupt handler to verify an interrupt has been
 * fielded.
 */
int saw_uc_write=0;
int saw_multi_ecc=0;

int saw_ecc_intr=0;
#endif

/* Code for handling ECC parts on IP26 baseboards.
 */

/*
 * ip26_isecc
 * Returns true if running on ECC baseboard.
 * Don't make this a macro, since it's used by the cachectl() syscall.
 */
int
ip26_isecc(void)
{
	return (board_rev() >= (IP26_ECCSYSID>>BOARD_REV_SHIFT));
}

/*
 * ip26_clear_ecc
 * Clear old single-bit errors and uncached writes
 */
static void
ip26_clear_ecc(void)
{
	if (!ip26_isecc())
		return;

	*(volatile __uint64_t *)PHYS_TO_K1(ECC_CTRL_REG) = ECC_CTRL_CLR_INT;
	flushbus();
}

#define MINUTE		(HZ*60)
#define	HOUR		(MINUTE*60)
#define DAY		(HOUR*24)

#define ECC_1BIT_WAIT	(MINUTE*10) /* Wait 10 minutes between reporting errs */
#define ECC_1BIT_WAIT_M	(DAY)  /* Wait 1 day max between reporting errs */
#define ECC_1BIT_STORE	(100)       /* Only print msg if this many seen */
#define ECC_1BIT_FLOOD_MIN (1000000) /* Must see this many, and ...*/
#define ECC_1BIT_FLOOD_RATE (10000)   /*  this many/sec to be a torrent. */

ulong ecc_num_1bit = 0;	    /* Num single bit errs seen since boot */
ulong ecc_recent_1bit = 0;  /* Num single bit errs since last report */
time_t ecc_last_1bit = 0;   /* lbolt time of last 1bit error report */
int ecc_wait_1bit = ECC_1BIT_WAIT; /* Time to wait between messages */

#define	ECC_1BIT_MSG \
	"One bit memory errors are being corrected by ECC hardware"
char ecc_1bit_msg[100];	    /* Statistics part of the message */

/* ip26_ecc_1bit_intr
 * Routine called when ECC hw tells us it has corrected a single-bit error
 * on a read from main memory.  This always comes in as an interrupt, so
 * we never have an address associated with the error.
 */
/*ARGSUSED*/
static void
ip26_ecc_1bit_intr(struct eframe_s *ep, int flags)
{
	time_t	err_interval;
	ulong	errs_per_interval;
	
	ecc_num_1bit++;
	ecc_recent_1bit++;
	
	/* If we're being overwhelmed by single-bit errors, we must have
	 * a bad SIMM or badly seated SIMM somewhere.  Alert the user,
	 * and panic for now.  Probably the best thing to do is alert the
	 * user, identify the SIMM, and then turn off the single-bit
	 * interrupt completely.
	 */
	if ((ecc_recent_1bit >= ECC_1BIT_FLOOD_MIN) &&
	    (ecc_recent_1bit >= ECC_1BIT_FLOOD_RATE*(lbolt-ecc_last_1bit)/HZ))
	{
		cmn_err_tag(47,CE_PANIC, "Single-bit errors are overwhelming the CPU.  Please check memory SIMMs.");
	}

	if (ecc_recent_1bit >=ECC_1BIT_STORE &&
	    ((lbolt > ecc_last_1bit+ecc_wait_1bit) ||
	     ecc_last_1bit==0)) {
		err_interval = lbolt - ecc_last_1bit;
		errs_per_interval = ecc_recent_1bit;
		ecc_last_1bit = lbolt;
		ecc_recent_1bit = 0;

		/* Keep increasing the delay between messages until the
		 * max delay has been reached.
		 */
		ecc_wait_1bit *=2;
		if (ecc_wait_1bit > ECC_1BIT_WAIT_M)
		    ecc_wait_1bit = ECC_1BIT_WAIT_M;

		if (err_interval >= DAY) {
		    int days = err_interval/DAY;
		    if (errs_per_interval > days) {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d per day",
				 errs_per_interval/days);
				
		    } else {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d days per error",
				 days/errs_per_interval);
		    }
		} else if (err_interval >= HOUR) {
		    int hours = err_interval/HOUR;
		    if (errs_per_interval > hours) {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d per hour",
				 errs_per_interval/hours);
				
		    } else {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d hours per error",
				 hours/errs_per_interval);
		    }
		} else if (err_interval >= HZ) {
		    int secs = err_interval/HZ;
		    if (errs_per_interval > secs) {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d per second",
				 errs_per_interval/secs);
				
		    } else {
			sprintf (ecc_1bit_msg,
				 " at the rate of %d seconds per error",
				 secs/errs_per_interval);
		    }
		} else {
		    ecc_1bit_msg[0]='\0';
		}

		cmn_err (CE_WARN, "%s%s.\n", ECC_1BIT_MSG, ecc_1bit_msg);
	}
}

/*  Routine called when ECC hw detects a multi-bit (uncorrectable) error
 * in memory.  This might be called from either the bus error handler, or
 * from the ecc interrupt, depending on what version of PALs is on the
 * baseboard.
 *
 * Never ignore a multibit error from GIO.
 */
/*ARGSUSED1*/
static int
ip26_ecc_mbit_intr(struct eframe_s *ep, int flags)
{
#ifdef DEBUG
	saw_multi_ecc = 1;
#endif

	if (flags & ECC_ISBERR) { /* addr is set to bad address */
		log_eccerr((flags & ECC_ISGIO) ? gio_par_adr : cpu_par_adr);
		return (flags & ECC_ISGIO) ? ECC_MBIT_GIO : ECC_MBIT_CPU;
	}
	return ECC_MBIT_UNKN;
}

/*  Routine called when ECC hw tells us it has seen an uncached write
 * to main memory while running in fast mode.  If the ignore_uc_write
 * global variable is set, then this is a nofault case, and we just
 * return the propper error code (to panic in common ecc panic code).
 */
/*ARGSUSED1*/
static int
ip26_ecc_uc_write_intr(struct eframe_s *ep, int flags)
{
#ifdef DEBUG
	uint addr = (flags & ECC_ISGIO) ? gio_par_adr : cpu_par_adr;

	saw_uc_write = 1;

	if (ignore_uc_write) {
	    ignore_uc_write = 0;  /* Only ignore one of them! */
	    if (flags & ECC_ISBERR) { /* addr is set to bad address */
		cmn_err (CE_CONT,
		  "Uncached write in fast mode detected by %s read from 0x%x and ignored.\n",
		  (flags & ECC_ISGIO) ? "GIO" : "CPU", addr);
	    } else {
		cmn_err (CE_CONT,
		    "Uncached write in fast mode detected and ignored.\n");
	    }
	    return 0;
	}
#endif

	if ((flags & ECC_NOFAULT) == 0) {
		return ECC_UCWRITE;		/* force a panic */
	}

	return 0;
}

/*
 * ip26_ecc_intr -- handle single bit errors.
 */
void
ip26_ecc_intr(struct eframe_s *ep)
{
#ifdef DEBUG
	saw_ecc_intr = 1;
#endif

	ip26_clear_ecc();

#ifdef DEBUG
	/* Let's check the cause reg for sanity reasons. */
	if (!(ep->ef_cause & SR_IBIT8))
	    cmn_err (CE_CONT, "ip26_ecc_intr got called without cause.\n");
#endif

	ip26_ecc_1bit_intr(ep, ECC_INTR);
}

#ifdef DEBUG
/*
 * ecc_gun
 * 
 * Test memory ECC logic, and software handlers as much as possible.
 * Currently, akes one of 2 commands:
 *  1) EGUN_MODE -- Switch between fast and slow modes.
 *  2) EGUN_UC_WRITE -- Do uncached write, and see if we get the error.
 *  3) EGUN_UC_WRITE_I -- Do uncached write, but ignore the error.
 *  4) EGUN_1BIT_INT -- Call ip26_ecc_1bit_intr()
 */
union ecc_mem_u {
	volatile ulong	lp[NBPP/sizeof(long)];
	volatile uint	ip[NBPP/sizeof(int)];
	volatile ushort	sp[NBPP/sizeof(short)];
};

/*ARGSUSED*/
int
ecc_gun(int cmd, void *cmd_data, rval_t *rval)
{
    static int ecc_gun_initialized = 0;
    static union ecc_mem_u *ecc_gun_mem;
    /* REFERENCED */
    ulong junkvar;
    register int loopvar;

    /* This assumes we're single-threaded, but that's pretty safe! */
    if (!ecc_gun_initialized) {
	/* Get 1 uncached page of memory */
	ecc_gun_mem = (union ecc_mem_u *)kvpalloc (1, VM_UNCACHED|VM_DIRECT, 0);
	if (ecc_gun_mem == NULL)
	    return ENOMEM;
	ASSERT (IS_KSEG1(ecc_gun_mem));

	ecc_gun_initialized = 1;
    }

    switch (cmd) {
    case (EGUN_MODE):
	{
	    ulong prev_ecc_mode = ip26_enable_ucmem();
	    ip26_return_ucmem(prev_ecc_mode);
	}
	break;

    case (EGUN_UC_WRITE_I):
	ignore_uc_write = 1;  /* Intr routine will reset this. */
    case (EGUN_UC_WRITE):
	saw_ecc_intr = 0;
	saw_uc_write = 0;

	/* Write 0 to a double-word.
	 */
	ecc_gun_mem->lp[0]=0;
	/* Now, trigger the ucwrite error. */
	junkvar = ecc_gun_mem->ip[512];  /* just somewhere far away */

	for (loopvar=0; loopvar<100; loopvar++) {
	    if (saw_uc_write) break;
	    DELAY(10);
	}

	if (!saw_uc_write) {
	    cmn_err(CE_CONT, "ecc_gun: expected ucwrite error.\nMaybe we're in slow mode.\n");
	    ignore_uc_write = 0;
	    return EINTR;
	}
	break;

    case (EGUN_1BIT_INT):
	ip26_ecc_1bit_intr(NULL, 0);
	break;

    default:
	cmn_err (CE_CONT, "ecc_gun: bad cmd %d.\n", cmd);
	return EINVAL;
    }
    
    return 0;
}
#endif	/* DEBUG */

/*  Hook in main() that's call right before going to spl0 for the
 * first time.  This is a perfect time for us to jump from slow to
 * normal mode if we are not stuck in slow mode.
 */
void
allowintrs(void)
{
	if (!ip26_allow_ucmem)
		(void)ip26_disable_ucmem();
}

/*  Hook to reset all the GIO slots before we call into the PROM.  This
 * makes sure nothing is dependent on memory when the PROM clear it.
 */
void
gioreset(void)
{
	volatile unsigned int *cfg = (unsigned int *)PHYS_TO_K1(CPU_CONFIG);
	*cfg &= ~CONFIG_GRRESET;
	flushbus();
	us_delay(5);
	*cfg |= CONFIG_GRRESET;
	flushbus();
}

/*
 *  The following functions allow for sharing GIO_SLOT_0 at
 *  GIO_INTERRUPT_1 between to compatible boards.  The ISR of each
 *  board is registered via setgiosharedvector(), which at interrupt
 *  time will result in a fanout function invoking each ISR.
 *
 *  If only one ISR is currently registered, then it is registered directly
 *  with GIO_INTERRUPT_1 at GIO_SLOT_0, otherwise if two are registered
 *  then sharedfanout() is registered via setgiovector() for later
 *  fanout.
 */

/*
 *  Keep ISR address and argument for both the upper and lower physical
 *  slots of physical GIO_SLOT_0.
 */
typedef struct shared_isr {
	void (*isr)(__psint_t, struct eframe_s *);
	__psint_t arg;
} shared_isr_t;

static shared_isr_t giofuncs[2];


/*
 *  Fanout function invoked for GIO_INTERRUPT_1, GIO_SLOT_0.
 */
/*ARGSUSED1*/
static void
sharedfanout(__psint_t arg, struct eframe_s *ep)
{
	if (giofuncs[GIO_SLOT_0_LOWER].isr)
		(*giofuncs[GIO_SLOT_0_LOWER].isr)
		 (giofuncs[GIO_SLOT_0_LOWER].arg, ep);

	if (giofuncs[GIO_SLOT_0_UPPER].isr)
		(*giofuncs[GIO_SLOT_0_UPPER].isr)
		 (giofuncs[GIO_SLOT_0_UPPER].arg, ep);

	return;
}  /* End of sharedfanout() */


/*
 *  ISR registration function for shared GIO slot 0.  It will introduce
 *  a fanout function if necessary in order to differentiate the ISRs
 *  of the boards sharing this slot.
 *
 *  Registration with the kernel is made using setgiovector() at
 *  GIO_INTERRUPT_1 for GIO_SLOT_0.
 */
void
setgiosharedvector(int position, void (*func)(__psint_t, struct eframe_s *),
 __psint_t arg)
{
	int s;
	int other;

	if (position < 0 || position > GIO_SLOT_0_UPPER) {
		cmn_err(CE_WARN,
		 "setgiosharedvector: position out of range (%d)\n",
		 position);
		return;
	}

	/*
	 *  Can't be used with Indy.
	 */
	if (!is_fullhouse()) {
		cmn_err(CE_WARN,
		 "setgiosharedvector: Improper system type - usage ignored\n");
		return;
	}

	other = (position == GIO_SLOT_0_LOWER ? GIO_SLOT_0_UPPER :
	 GIO_SLOT_0_LOWER);

	s = splgio1();

	giofuncs[position].isr = func;
	giofuncs[position].arg = arg;

	/*
	 *  If non-NULL, then an ISR is to be registered.
	 */
	if (func) {

		/*
		 *  If something has already been registered for the other
		 *  slot position, then we must register the fanout function.
		 */
		if (giofuncs[other].isr)
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, sharedfanout,
			 arg);

		/*
		 *  Otherwise, directly register the one ISR associated with
		 *  the slot.
		 */
		else
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, func, arg);
	}

	/*
	 *  De-register the ISR for the position.
	 */
	else {

		/*
		 *  If something has been registered for the other position,
		 *  then register it directly, effectively unfanning out.
		 */
		if (giofuncs[other].isr)
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0,
			 giofuncs[other].isr, giofuncs[other].arg);

		/*
		 *  Otherwise, de-register the one ISR associated with
		 *  the slot.
		 */
		else
			setgiovector(GIO_INTERRUPT_1, GIO_SLOT_0, func, arg);
	}

	splx(s);
	return;

}  /* End of setgiosharedvector() */

int
is_thd(int level)
{
	int retval;
	switch(level) {

	/* Threaded */
	case VECTOR_SCSI:
	case VECTOR_SCSI1:
	case VECTOR_ENET:
	case VECTOR_PLP:
	case VECTOR_GIO1:
	case VECTOR_HPCDMA:
	case VECTOR_DUART:
	case VECTOR_VIDEO:
	case VECTOR_KBDMS:
	case VECTOR_EISA:
		retval = 1;
		break;

	/* Not threaded */
	case VECTOR_GIO0:
	case VECTOR_LCL2:
	case VECTOR_POWER:
	case VECTOR_LCL3:
	case VECTOR_ACFAIL:
	case VECTOR_TCCHW:
	case VECTOR_TCCLW:

	/* Broken as threads */
	case VECTOR_GIO2: /* Vertical Retrace */

	/* Untested as threads */
	case VECTOR_GDMA:
	case VECTOR_ISDN_ISAC:
	case VECTOR_ISDN_HSCX:

	/* ? */
	case VECTOR_DRAIN0: /* VECTOR_GIOEXP0 has same number */
	case VECTOR_DRAIN1: /* VECTOR_GIOEXP0 has same number */
	default:
		retval = 0;
		break;
	} /* switch level */
	return retval;
}

/*
 * lcl_intrd
 * This routine is the base point for lcl interrupts which run as threads.
 * The ipsema() call is a bit magical, since it is not really a
 * semaphore.  Every time it is called, the thread running it restarts
 * itself at the beginning.
 */
void
lcl_intrd(lclvec_t *l)
{
	volatile unchar *int_mask;
	int s;

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(l->lcl_lat);
#endif
	
	l->isr(l->arg, NULL);
	if ((l->lcl_flags&THD_HWDEP) == 0) {
		int_mask = K1_LIO_0_MASK_ADDR;
	} else if ((l->lcl_flags&THD_HWDEP) == 1) {
		int_mask = K1_LIO_1_MASK_ADDR;
	} else {
		int_mask = K1_LIO_2_MASK_ADDR;
	}

	s = disableintr();
	*int_mask |= (l->bit);
	enableintr(s);

	/* Wait for next interrupt */
	ipsema(&l->lcl_isync);	/* Thread always restarts at top. */
}

void
lcl_intrd_init(lclvec_t *lvp)
{
	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)lcl_intrd, lvp);
	atomicSetInt(&lvp->lcl_flags, THD_INIT);

	/* The ipsema will always come out in lcl_intrd, now.
	 * It will sleep if the semaphore has not been signalled.
	 */
	ipsema(&lvp->lcl_isync);
	/* NOTREACHED */
}

/*
 * lclvec_thread_setup -
 * this routine handles thread setup for the given lclvec entry
 */
static void
lclvec_thread_setup(int level, int lclid, lclvec_t *lvp)
{
	char thread_name[32];
	int pri;

	sprintf(thread_name, "lclintr%d.%d", lclid, level+1);
	/* XXX TODO - pri calculation/band checking */
	pri = (250 - lclid);
	xthread_setup(thread_name, pri, &lvp->lcl_tinfo,
			(xt_func_t *)lcl_intrd_init, (void *)lvp);
}

/*
 * Because it's possible to register an interrupt handler before
 * it's possible to setup ithreads, we need the following to handle
 * such cases.  The ithreads_enabled switch gets set to 1 when it's
 * "safe" for calls to setlclvector() to use lclvec_thread_setup()
 * directly.  The enable_ithreads() routine is used to scan the
 * lclvec tables and thread any interrupts that have THD_REG set.
 */
void
enable_ithreads(void)
{
	lclvec_t *lvp;
	int lvl;

	ithreads_enabled = 1;
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl0vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 0, lvp);
		}
	}
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl1vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 1, lvp);
		}
	}
	for (lvl = 0; lvl < 8; lvl++) {
		lvp = &lcl2vec_tbl[lvl+1];
		if (lvp->lcl_flags & THD_REG) {
			ASSERT ((lvp->lcl_flags & THD_ISTHREAD));
			ASSERT (!(lvp->lcl_flags & THD_OK));
			lclvec_thread_setup(lvl, 2, lvp);
		}
	}
}
