/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * IP28 specific functions
 */

#ident "$Revision: 1.106 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/cpu.h>
#include <sys/IP22nvram.h>
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
#include <sys/kopt.h>
#include <sys/loaddrs.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/kabi.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
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
#include <sys/uthread.h>
#include <sys/var.h>
#include <sys/vdma.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/eisa.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#include <sys/arcs/kerncb.h>
#include <sys/hal2.h>
#include <string.h>
#include <sys/rtmon.h>
#include <sys/RACER/gda.h>
#include <sys/hwperfmacros.h>
#include <sys/atomic_ops.h>
#include <ksys/cacheops.h>
#include <ksys/hwg.h>
#ifdef R10000_SPECULATION_WAR
#include <sys/ksynch.h>
static lock_t mpin_cookies_lock;
extern lock_t locklistlock;
#endif
#include <sys/ksignal.h>

int ithreads_enabled = 0;
static int is_thd(int);
static void lclvec_thread_setup(int, int, lclvec_t *);

extern int clean_shutdown_time; 	/* mtune/kernel */

typedef caddr_t dmavaddr_t;
typedef paddr_t dmamaddr_t;

extern int ip26_allow_ucmem;    /* systune ecc boot time on/off switch */

#define MAXCPU	1
#define RPSS_DIV		2
#define RPSS_DIV_CONST		(RPSS_DIV-1)

int	maxcpus = MAXCPU;

/* processor-dependent data area */
pdaindr_t	pdaindr[MAXCPU];

/* last tlbpid assigned for each processor */
unsigned char	cur_tlbpid[MAXCPU];

/*
 * Number of CPUs running when the NMI hit.
 */
int		nmi_maxcpus = MAXCPU;
static void	init_nmi_dump(void);
extern void	nmi_dump(void);
extern cpuid_t  master_procid;                  /* master processor's id */

/* flag indicate that delay calibration has been done on this processor */
int calibrated[MAXCPU];	

extern int cpu_mhz_rating(void);
static int local_cpu_mhz_rating;

void cpu_waitforreset(void);

void ip28_write_pal(int);		/* IP28asm.s */
void unmap_ecc(void);

void branch_prediction_off(void);
void check_us_delay(void);

void set_leds(int);
void set_autopoweron(void);
void resettodr(void);
static int get_dayof_week(time_t year_secs);
static void set_endianness(void);

/* Local ECC chip routines.  (Some are in IP28asm.s, too.)
 */
void ip28_ecc_error(void);
static void ip28_clear_ecc(void);

__uint32_t get_r10k_config(void);

/* Global flag to reset gfx boards */
int GRReset = 0;

#if DEBUG
#define	CACHEFLUSH_DEBUG	0
#endif

#if CACHEFLUSH_DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
static void idbg_cachelog(__psint_t p);
void docachelog(long a,int len,char *string);
#endif

#if CONSISTANCY_CHECK
#define	CCEFIXUP		0
#define	CCELINES_LIMIT		500
#define	CCF_REQUIRE_NOHIT	0x01
#define	CCF_REQUIRE_NODIRTY	0x02

void consistancy_check(unsigned pa, unsigned ct, char *where, unsigned flags);
#else
#define consistancy_check(a,l,w,f)
#endif

#if CONSISTANCY_CHECK_WD93
static void consistancy_check_wd93_list(unsigned *cbp, unsigned *bcnt, char *where, unsigned flags);
#define consistancy_check_wd93(a,l,w,f)		consistancy_check(a,l,w,f)
#else
#define consistancy_check_wd93_list(a,l,w,f)
#define consistancy_check_wd93(a,l,w,f)
#endif

/* software copy of write-only registers */
uint _hpc3_write1 = PAR_RESET | KBD_MS_RESET | EISA_RESET | LED_GREEN;
uint _hpc3_write2 = ETHER_AUTO_SEL | ETHER_NORM_THRESH | ETHER_UTP_STP |
	ETHER_PORT_SEL | UART0_ARC_MODE | UART1_ARC_MODE;

unsigned int mc_rev_level;
u_int enet_collision;


/* T5 sysclk divide by: 1, 1.5, 2, 2.5, 3, 3.5, 4 (n == 0 -> undefined) */
static char sysclk_div_n[] = {  1, 1, 2, 1, 2, 1, 2, 1,
				2, 1, 2, 1, 2, 1, 2, 1 };
static char sysclk_div_d[] = {  1, 1, 3, 2, 5, 3, 7, 4,
				9, 5,11, 6,13, 7,15, 8 };

/*
 * Processor interrupt table
 */
extern	void pcount_intr(struct eframe_s *, kthread_t *);
extern	void buserror_intr(struct eframe_s *);
static	void _buserror_intr(struct eframe_s *);
extern	void clock(struct eframe_s *, kthread_t *);
extern	void ackkgclock(void);
static	void lcl1_intr(struct eframe_s *);
static	void lcl0_intr(struct eframe_s *);
extern	void timein(void);
extern	void load_nvram_tab(void);
static	void power_bell(void);
static	void flash_led(int);

typedef void (*intvec_func_t)(struct eframe_s *, kthread_t *);

struct intvec_s {
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		bit;		/* corresponding bit in sr */
	int		fil;		/* make sizeof this struct 2^n */
};

static struct intvec_s c0vec_tbl[] = {
	0,	    0,			0,		0,  /* (1 based) */
	(intvec_func_t)
	timein,     SR_IMASK5|SR_IEC,   SR_IBIT1 >> 8,	0,  /* softint 1 */
	(intvec_func_t)
	0,	    SR_IMASK5|SR_IEC,	SR_IBIT2 >> 8,	0,  /* softint 2 */
	(intvec_func_t)
	lcl0_intr,  SR_IMASK5|SR_IEC,	SR_IBIT3 >> 8,	0,  /* hardint 3 */
	(intvec_func_t)
	lcl1_intr,  SR_IMASK5|SR_IEC,	SR_IBIT4 >> 8,	0,  /* hardint 4 */
	clock,	    SR_IMASK5|SR_IEC,	SR_IBIT5 >> 8,	0,  /* hardint 5 */
	(intvec_func_t)
	ackkgclock,SR_IMASK6|SR_IEC,SR_IBIT6 >> 8, 0,/* 6 */
	(intvec_func_t)
	_buserror_intr,SR_IMASK7|SR_IEC,SR_IBIT7 >> 8,	0,  /* hardint 7 */
	pcount_intr, SR_IMASK8|SR_IE,	SR_IBIT8>>8,	0,  /* hardint 8 */
};

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
static  lcl_intr_func_t hpcdmastray_scsi;
static	lcl_intr_func_t ip28_gio0_intr;
static	lcl_intr_func_t ip28_gio1_intr;
static	lcl_intr_func_t ip28_gio2_intr;
static  lcl_intr_func_t lcl2_intr;
static	lcl_intr_func_t powerfail;
static  lcl_intr_func_t powerintr;

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
	{ lcl_stray,	8, 0x80 }
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
#define GIOSLOTS	3 /* slot 0, slot 1 (IP22B/IP24 only), gfx */

static struct {
	void		(*isr)(__psint_t,struct eframe_s *);
	__psint_t	arg;
} giovec_tbl[GIOSLOTS][GIOLEVELS];

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
		0x80,
		VECTOR_SCSI
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
		0x80,
		VECTOR_SCSI1
        }
};

/*	used to sanity check adap values passed in, etc */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))

#define PROBE_SPACE(X)	PHYS_TO_K1(X)

/* table of probeable kmem addresses */
struct kmem_ioaddr kmem_ioaddr[] = {
	{ PROBE_SPACE(LIO_ADDR), (LIO_GFX_SIZE + LIO_GIO_SIZE) },
	{ PROBE_SPACE(HPC3_SYS_ID), sizeof (int) },
	{ PROBE_SPACE(HAL2_ADDR), (HAL2_REV - HAL2_ADDR + sizeof(int))},
	{ 0, 0 },
};

short	cputype = 28;   /* integer value for cpu (i.e. xx in IPxx) */

static uint sys_id;	/* initialized by init_sysid */

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

u_short i_dma_burst, i_dma_delay;

/* default values for different revs of base board */
#define GIO64_ARB_004 ( GIO64_ARB_HPC_SIZE_64	| GIO64_ARB_HPC_EXP_SIZE_64 |\
			GIO64_ARB_1_GIO		| GIO64_ARB_EXP0_PIPED |\
			GIO64_ARB_EXP1_PIPED	| GIO64_ARB_EISA_MST )
u_int gio64_arb_reg;

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))
char eaddr[6];

unsigned char *
etoh (char *enet)
{
    static unsigned char dig[6], *cp;
    int i;

    for ( i = 0, cp = (unsigned char *)enet; *cp; ) {
	if ( *cp == ':' ) {
	    cp++;
	    continue;
	} else if ( !ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1)) ) {
	    return NULL;
	} else {
	    if ( i >= 6 )
		return NULL;
	    dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
	    cp += 2;
	}
    }
    
    return i == 6 ? dig : NULL;
}

/*
 * initialize the serial number.  Called from mlreset.
 */
void
init_sysid(void)
{
	char *cp; 

	cp = (char *)arcs_getenv("eaddr");
	bcopy (etoh(cp), eaddr, 6);

	sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
		 (eaddr[4] << 8) | eaddr[5];
	sprintf (arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
	    (eaddr[0]>>4)&0xf, eaddr[0]&0xf,
	    (eaddr[1]>>4)&0xf, eaddr[1]&0xf,
	    (eaddr[2]>>4)&0xf, eaddr[2]&0xf,
	    (__psunsigned_t)(eaddr[3]>>4)&0xf, (__psunsigned_t)eaddr[3]&0xf,
	    (__psunsigned_t)(eaddr[4]>>4)&0xf, (__psunsigned_t)eaddr[4]&0xf,
	    (__psunsigned_t)(eaddr[5]>>4)&0xf, (__psunsigned_t)eaddr[5]&0xf);
}

/*
 *	pass back the serial number associated with the system
 *  ID prom. always returns zero.
 */
int
getsysid(char *hostident)
{
	/*  Serial number is only 4 bytes long on IP28  Left justify
	 * in memory passed in...  Zero out the balance.
	 */
	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}

/* gr2_init.c is calling is_fullhouse() 
 */
int is_fullhouse(void) { return(1); }
int is_ioc1(void) { return(0); }

/* 
 *	get memory configuration word for a bank
 *              - bank better be between 0 and 3 inclusive
 */
static uint
get_mem_conf(int bank)
{
    uint memconfig;

    memconfig = (bank <= 1) ? 
            *(volatile uint *)PHYS_TO_COMPATK1(MEMCFG0) :
                *(volatile uint *)PHYS_TO_COMPATK1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    return memconfig & 0xffff;
}       /* get_mem_conf */

/*
 * bank_addrlo - determine first address of a memory bank
 */
static int
bank_addrlo(int bank)
{
    return (int)((get_mem_conf(bank) & MEMCFG_ADDR_MASK) << 24); 
}       /* bank_addrlo */

/*
 * banksz - determine the size of a memory bank
 */
static int
banksz(int bank)
{
    uint memconfig = get_mem_conf(bank);

    /* weed out bad module sizes */
    if (!memconfig)
        return 0;
    else
        return (int)(((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << 16); 
}       /* banksz */

/*
 * addr_to_bank - determine which physical memory bank contains an address
 *              returns 0-3 or -1 for bad address
 */
int
addr_to_bank(uint addr)
{
    int bank;
    uint size;

    for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
        if (!(size = banksz(bank))) /* bad memory module if size == 0 */
            continue;

        if (addr >= bank_addrlo(bank) && addr < bank_addrlo(bank) + size)
            return (bank);
    }
    return (-1);        /* address not found */
}   /* addr_to_bank */

static int memsize = 0;	/* total ram in clicks */
static int seg0_maxmem;

/*
 * szmem - size main memory
 * Returns # physical pages of memory
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
		seg0_maxmem += bank_size;
	    }
	}
	memsize = btoct(memsize);			/* get pages */
	seg0_maxmem = btoct(seg0_maxmem);

	if (maxpmem) {
	    memsize = MIN(memsize, maxpmem);
	    seg0_maxmem = MIN(memsize, seg0_maxmem);
	}
    }

    return memsize;
}

/*  Size memory to find last low page, which isn't used by irix, to prevent
 * crash in ECC handler unit ecc_init() is called.
 */
char *
early_ecc_page(void)
{
	int low_mem = 0;
	int bank_size;
	int bank;

	for (bank = 0; bank < MAX_MEM_BANKS; ++bank) {
	    bank_size = banksz(bank);
	    if (bank_size) {
		low_mem += bank_size;
	    }
	}

	return (char *)PHYS_TO_K1(SEG0_BASE + low_mem - NBPP);
}

int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return((pfn >= min_pfn && pfn < (btoct(SEG0_BASE) + seg0_maxmem)));
}

/*
 * On the IP28, the secondary cache is a unified i/d cache. The
 * R10000 caches must maintain the property that the primary caches
 * are a subset of the secondary cache - any line that is present in
 * the primary must also be present in the secondary. A line can be
 * dirty in the primary and clean in the secondary, of coarse, but
 * the subset property must be maintained. Therefore, if we invalidate
 * lines in the secondary, we must also invalidate lines in both the
 * primary i and d caches. So when we use the 'index invalidate' or
 * 'index writeback invalidate' cache operations, those operations
 * must be targetted at both the primary caches.
 *
 * When we use the 'hit' operations, the operation can be targetted
 * selectively at the primary i-cache or the d-cache.
 */

void
clean_icache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*  Remap the page to flush if the page is a mapped address, or
	 * full address is not given.  This works with hit based
	 * __icache_inval() due to primary sub-set rules.
	 */
	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	}
	else
		kvaddr = addr;

	__icache_inval(kvaddr, len);
}

void
clean_dcache(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
	char *kvaddr;

	/* Low level routines can't handle length of zero */
	if (len == 0)
		return;

	ASSERT(flags & CACH_OPMASK);
	ASSERT(!IS_KSEG1(addr));	/* catch PHYS_TO_K0 on high addrs */

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((__psint_t)addr+len-1)));

	/*  Remap the page to flush if the page is a mapped address, or
	 * full address is not given.
	 */
	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	}
	else
		kvaddr = addr;

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	}
	else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	}
	else if (flags & CACH_WBACK) {
		__dcache_wb(kvaddr, len);
	}
}

/*
 * getfirstclick - returns pfn of first page of ram
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
 *		  This isn't used on IP28.
 */
/*ARGSUSED*/
pfn_t
node_getfirstfree(cnodeid_t node)
{
	ASSERT(node == 0);
	return btoc(SEG0_BASE);
}

/*
 * getmaxclick - returns pfn of last page of ram
 */

/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t unused)
{
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);

	return btoc(SEG0_BASE) + memsize - 1;
}

/*
 * setupbadmem - mark the hole in memory between seg 0 and seg 1.
 *		 return count of non-existent pages.
 */

/*ARGSUSED3*/
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	register pgno_t	hole_size = 0;
	register pgno_t npgs;

	/*  Mark last page as bad so the kernel will not use it on the
	 * behalf of user processes.  Hopefully this will reduce the
	 * risk of speculatively running off the end of memory and causing
	 * MC error interrupts.
	 */
	pfd += btoc(SEG0_BASE) + seg0_maxmem - first - 1;
	pfd->pf_flags |= P_BAD;			/* do not use this page */
	pfd->pf_flags &= ~P_DUMP;		/* do not touch on dumps */

	return hole_size;
}

int
is_memory(long phys)
{
        return  (phys >= SEG0_BASE && phys < (SEG0_BASE+SEG0_SIZE));
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
        extern struct mmmap mmmap_addrs[];      /* defined in master.d/mem */
        struct mmmap *m = mmmap_addrs;
        long phys;

        if (ip26_allow_ucmem) return;           /* does not matter */

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
}

/* Decide between IP28 and IP26 memory timing from baseboard revision and
 * memory configuation.
 */
uint ip28_memacc_slow = CPU_MEMACC_SLOW;
uint ip28_memacc_norm = CPU_MEMACC_NORMAL;
static void
ip28_memtiming(void)
{
#define is64MB(bank)	((bank & (MEMCFG_DRAM_MASK|MEMCFG_BNK|MEMCFG_VLD)) == \
			 (MEMCFG_64MRAM|MEMCFG_VLD))
	int bank0, bank1, bank2, nvalid;

	bank0 = get_mem_conf(0);
	bank1 = get_mem_conf(1);
	bank2 = get_mem_conf(2);

	nvalid = ((bank0 & MEMCFG_VLD) != 0) +
		 ((bank1 & MEMCFG_VLD) != 0) +
		 ((bank2 & MEMCFG_VLD) != 0);

	/* Have at least 2 banks with at least 1 64MB SIMM bank.
	 */
	if ((is_ip26bb() && !is_ip26pbb()) ||
	    ((nvalid > 1) && (is64MB(bank0)||is64MB(bank1)||is64MB(bank2)))) {
		ip28_memacc_slow = CPU_MEMACC_SLOW_IP26;
		ip28_memacc_norm = CPU_MEMACC_NORMAL_IP26;
	}
}

/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup.
 */
/* ARGSUSED */
void
mlreset(int junk)
{
	extern int get_cpu_irr(void);
	void early_ecc_init(char *);
	extern int valid_dcache_reasons;
	extern int valid_icache_reasons;
	extern int reboot_on_panic;
	extern uint cachecolormask;
	extern int softpowerup_ok;
	extern int cachewrback;
	extern int pdcache_size;
	unsigned int cpuctrl1tmp;
	char *p;

	/*
	** just to be safe, clear the parity error register
	** This is done because if any errors are left in the register,
	** when a DBE occurs we will think it is due to a parity error.
	*/
	*(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	flushbus();

	/* ensure ecc is not mapped (newer proms do this ok) */
	unmap_ecc();

	/* memory balistics */
	ip28_memtiming();

#ifdef DEBUG
	/* For now, margin power supply low as high VCC problem is feared
	 * to be causing the T5 dropout we have had.  If you have a 3.3V
	 * set-point supply, margining will hang the system.  If you have
	 * a non-MG supply this does not affect the 3.3V (only 5V).
	 *
	 * Use diagmode=xm to margin automatically low 
	 * Use diagmode=xM to margin automatically high
	 *
	 * Normally you would set diagmode to 'vm'.
	 */
	p = kopt_find("diagmode");
	if (p && (*p != '\0')) {
		if (*(p+1) == 'm')
			_hpc3_write2 |= MARGIN_LO;
		else if (*(p+1) == 'M')
			_hpc3_write2 |= MARGIN_HI;
	}
#endif	/* DEBUG */

	*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE1) = _hpc3_write1;
	*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE2) = _hpc3_write2;
	*(volatile uint *)PHYS_TO_COMPATK1(HPC3_BUSERR_STAT_ADDR);
	flushbus();

	/* HPC3_EXT_IO_ADDR is 16 bits wide */
	*(volatile int *)PHYS_TO_COMPATK1(HPC3_PBUS_CFG_ADDR(6)) |= HPC3_CFGPIO_DS_16;

	/*
	 * Set RPSS divider for increment by 2, the fastest rate at which
	 * the register works  (also see comment for query_cyclecntr).
	 */
	*(volatile uint *)PHYS_TO_COMPATK1(RPSS_DIVIDER) = (1 << 8)|RPSS_DIV_CONST;

#if 0
	/* GIO parity cannot be enabled on pacecar.  The MC also uses this
	 * bit for turning on other parity, which does not work with memory
	 * and SysAD parity off.  Left code as a reminder.
	 *
	 * Yep, from the MC's view of the world is naked...
	 */
	*cpuctrl0 |= CPUCTRL0_GPR|CPUCTRL0_R4K_CHK_PAR_N;
#endif
	flushbus();

	/* Rev A MC unsupported in IP28, but global still needed in gr2 code.
	 */
	mc_rev_level = *(volatile uint *)PHYS_TO_COMPATK1(SYSID) &
		SYSID_CHIP_REV_MASK;

	/*
	 * set the MC write buffer depth
	 * the effective depth is roughly 17 minus this value (i.e. 6)
	 * NOTE: don't make the write buffer deeper unless you
         * understand what this does to GFXDELAY
	 */
	cpuctrl1tmp = *(volatile unsigned int *)PHYS_TO_COMPATK1(CPUCTRL1);
	cpuctrl1tmp &= 0xFFFFFFF0;
	*(volatile unsigned int *)PHYS_TO_COMPATK1(CPUCTRL1) = cpuctrl1tmp|0xD;
	flushbus();

	gio64_arb_reg = GIO64_ARB_004;
	*(volatile u_int *)PHYS_TO_COMPATK1(GIO64_ARB) = gio64_arb_reg;
	flushbus();

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
		kdebug = 0;
	
	/* set initial interrupt vectors */
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = 0;
	*K1_LIO_2_MASK_ADDR = 0;
	*K1_LIO_3_MASK_ADDR = 0;

	lclvec_init();

	setlclvector(VECTOR_ACFAIL, powerfail, 0);
	setlclvector(VECTOR_POWER, powerintr, 0);
	setlclvector(VECTOR_LCL2, lcl2_intr, 0);

	/* set interrupt DMA burst and delay values */
	i_dma_burst = dma_delay;
	i_dma_delay = dma_burst;

	/* Early ECC handler until ecc_init is called later */
	early_ecc_init(early_ecc_page());

	init_sysid();
	load_nvram_tab();	/* get a copy of the nvram */

	cachewrback = 1;

	/* cachecolormask is set from the bits in a vaddr/paddr
	 * which the processor looks at to determine if a VCE
	 * should be raised.  Luckily the T5 handles this in HW,
	 * so for current T5/kerne's cachecolormask = 0;
	 */
	cachecolormask = pdcache_size / (NBPP * 2) - 1;

	if ((int)cachecolormask < 0)
		cachecolormask = 0;

	valid_icache_reasons &= ~CACH_AVOID_VCES;
	valid_dcache_reasons &= ~CACH_AVOID_VCES;

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	set_endianness();
	softpowerup_ok = 1;		/* for syssgi() */
	VdmaInit();

	eisa_init();

        init_nmi_dump();		/* install kernel NMI vector */

	ip28_clear_ecc();		/* clear any acumulated errors */
	ip28_ecc_correct();		/* ensure ECC correction is on */
	ip28_enable_eccerr();		/* ensure ECC reporting is on */
	ip28_clear_ecc();		/* clear any acumulated errors */
	(void)ip28_enable_ucmem();	/* in slow mode till allowintrs() */

	/* clear the r10k hw performace 
	 * control and count registers 
	 */
	r1nkperf_control_register_set(0,0);
	r1nkperf_control_register_set(1,0);
	r1nkperf_data_register_set(0,0);
	r1nkperf_data_register_set(1,0);

	LOCK_INIT( &mpin_cookies_lock, -1, plbase, (lkinfo_t *) -1L );
	spinlock_init(&locklistlock, "locklistlock");
}


static void
set_endianness(void)
{
	/* Assume Big Endian */
	*(volatile char *)PHYS_TO_COMPATK1(HPC3_MISC_ADDR) &= ~HPC3_DES_ENDIAN;
}

/*
 * setkgvector - set interrupt handler for the profiling clock
 */
void
setkgvector(void (*func)())
{
	c0vec_tbl[6].isr = func;
}

/*
 * setlclvector - set interrupt handler for given local interrupt level
 * Note that the graphics code only calls this with 2 args, but the
 * graphics interrupt routine that is passed doesn't expect an arg,
 * so that doesn't matter.
 */
void
setlclvector(int level, lcl_intr_func_t *func, __psint_t arg)
{
	int s;
	register lclvec_t *lv_p;	/* Which table entry to update */
	volatile unchar *mask_reg;	/* Addr of mask to enable int */
	int lcl_id;			/* Which lcl intr */

	/* Depending on the level, we need to set up a mask, and put
	 * the function into a particular jump table.  Each mask register
	 * and jump table has 8 entries.
	 */
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
	void (*ip28_func)(__psint_t, struct eframe_s *);
} giointr[] = {
	VECTOR_GIO0, ip28_gio0_intr,			/* GIO_INTERRUPT_0 */
	VECTOR_GIO1, ip28_gio1_intr,			/* GIO_INTERRUPT_1 */
	VECTOR_GIO2, ip28_gio2_intr			/* GIO_INTERRUPT_2 */
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
	
	if ( slot < 0 || slot >= GIOSLOTS )
		cmn_err(CE_PANIC,"GIO slot number out of range (%u)",slot);

	s = splgio2();

	giovec_tbl[slot][level].isr = func;
	giovec_tbl[slot][level].arg = arg;

	if (giovec_tbl[GIO_SLOT_GFX][level].isr ||
		giovec_tbl[GIO_SLOT_0][level].isr ||
		giovec_tbl[GIO_SLOT_1][level].isr) {
		setlclvector(giointr[level].vect,
			giointr[level].ip28_func,
			PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR));
	}
	else {
		setlclvector(giointr[level].vect,0,0);
	}

	splx(s);
}

/*
 * setgioconfig - Set GIO slot configuration register
 *
 */
void
setgioconfig(int slot, int flags)
{
	static int slots = 0;
	u_int mask;

	mask = (GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
		GIO64_ARB_EXP0_MST);

	switch (slot) {
	case GIO_SLOT_0:
		break;
	case GIO_SLOT_1:
		mask <<= 1;
		flags <<= 1;
		break;
	case GIO_SLOT_GFX:
		mask >>= 1;
		flags >>= 1;
		break;
	default:
		ASSERT(slot == GIO_SLOT_0 || slot == GIO_SLOT_1 || slot == GIO_SLOT_GFX);
		return;
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
            !(gio64_arb_reg & GIO64_ARB_EXP0_RT) && (slots == 3) &&
            is_ip26bb()) {
                cmn_err(CE_CONT,
                "WARNING setgioconfig: Bottom GIO slot configured as a "
                "real-time\n"
                "device while top slot is configured as a long-burst device.\n"
                "The cards should be reversed if the backplane insalled\n"
                "allows this.  If your backplane or configuration does not\n"
                "allow for this, contact your service provider.\n\n");
        }
}

/*
 * Register a HPC dmadone interrupt handler for the given channel.
 */
void
sethpcdmaintr(int channel, void (*func)(__psint_t, struct eframe_s *),
	      __psint_t arg)
{
    int s;
    int i;
    void (*hpcdma_handler)(__psint_t, struct eframe_s *);
    
    if (channel < 0 || channel >= HPCDMAVECTBL_SLOTS) {
	 cmn_err(CE_WARN, "sethpcdmaintr: bad hpcdmadone channel %d, ignored",
		 channel);
	 return;
    }

    s = splhintr();
    if (func) {
	 hpcdmavec_tbl[channel+1].isr = func;
	 hpcdmavec_tbl[channel+1].arg = arg;
    } else {
	 hpcdmavec_tbl[channel+1].isr = hpcdmastray;
	 hpcdmavec_tbl[channel+1].arg = 0;
    }
    hpcdma_handler = NULL;
    for (i = 0; i < HPCDMAVECTBL_SLOTS; i++)
    {
	void (*isr)(__psint_t, struct eframe_s *) = hpcdmavec_tbl[channel+i+1].isr;

	if ((__psunsigned_t)isr != (__psunsigned_t)hpcdmastray && 
	    (__psunsigned_t)isr != (__psunsigned_t)hpcdmastray_scsi) {
		hpcdma_handler = hpcdma_intr;
	    	break;
	}
    }
    setlclvector(VECTOR_HPCDMA, hpcdma_handler, 0);
    flushbus();
    splx(s);
}

/*
 * find first interrupt
 *
 * This is an inline version of the ffintr routine.  It differs in
 * that it takes a single 8 bit value instead of a shifted value.
 */
static char ffintrtbl[][16] = { { 0,5,6,6,7,7,7,7,8,8,8,8,8,8,8,8 },
			        { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 } };
#define FFINTR(c) (ffintrtbl[0][c>>4]?ffintrtbl[0][c>>4]:ffintrtbl[1][c&0xf])

#if DEBUG
/* count which interrupts we are getting, not just total.  Subtract
 * one because handler array starts at offset 1. */
unsigned intrcnt[8], intrl0cnt[8], intrl1cnt[8];
#define COUNT_INTR	intrcnt[(iv-c0vec_tbl)-1]++
#define COUNT_L0_INTR	intrl0cnt[(liv-lcl0vec_tbl)-1]++
#define COUNT_L1_INTR	intrl1cnt[(liv-lcl1vec_tbl)-1]++
#else
#define COUNT_INTR
#define COUNT_L0_INTR
#define COUNT_L1_INTR
#endif /* DEBUG */

/*
 * intr - handle 1st level interrupts
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
			if (iv->msk == (SR_IMASK5|SR_IEC))
				check_kp = 1;
			atomicAddInt((int *)&SYSINFO.intr_svcd,1);
                        cause &= ~iv->bit;
                } while (cause);
                cause = getcause();
        }

	LOG_TSTAMP_EVENT(RTMON_INTR,TSTAMP_EV_INTREXIT,TSTAMP_EV_INTRENTRY,
			 NULL,NULL,NULL);

	return check_kp;
}

#ifdef ITHREAD_LATENCY
#define SET_ISTAMP(liv) xthread_set_istamp(liv->lcl_lat)
#else
#define SET_ISTAMP(liv)
#endif /* ITHREAD_LATENCY */

#define LCL_CALL_ISR(liv,ep,maskaddr,svcd)			\
	if (liv->lcl_flags & THD_OK) {				\
		SET_ISTAMP(liv);				\
		*(maskaddr) &= ~(liv->bit);			\
		vsema(&liv->lcl_isync);				\
	} else {						\
		(*liv->isr)(liv->arg,ep);			\
	}

/*
 * lcl?intr - handle local interrupts 0 and 1
 */
static void
lcl0_intr(struct eframe_s *ep)
{
	register lclvec_t *liv;
	int lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_0_ISR_ADDR & *K1_LIO_0_MASK_ADDR;

	if (lcl_cause) {
		do {
			liv = lcl0vec_tbl+FFINTR(lcl_cause);
			COUNT_L0_INTR;
			LCL_CALL_ISR(liv,ep,K1_LIO_0_MASK_ADDR,threaded);
			svcd++;
			lcl_cause &= ~liv->bit;
		} while (lcl_cause);
	}
	else	{
		/*
		 * INT2 fails to latch fifo full in the local interrupt
		 * status register, but it does latch it internally and
		 * needs to be cleared by masking it.  So if it looks like
		 * we've received a phantom local 0 interrupt, handle it
		 * as a fifo full.
		 */
		liv = &lcl0vec_tbl[VECTOR_GIO0+1];

		(*liv->isr)(liv->arg,ep);	/* this guy is not threaded */

		return;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

static void
lcl1_intr(struct eframe_s *ep)
{
	char lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_1_ISR_ADDR & *K1_LIO_1_MASK_ADDR;

	while (lcl_cause) {
		register lclvec_t *liv = lcl1vec_tbl+FFINTR(lcl_cause);
		COUNT_L1_INTR;
		LCL_CALL_ISR(liv,ep,K1_LIO_1_MASK_ADDR,threaded);
		svcd++;
		lcl_cause &= ~liv->bit;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

/*ARGSUSED1*/
static void
lcl2_intr(__psint_t arg, struct eframe_s *ep)
{
	char lc2_cause;
	int svcd = 0;

	lc2_cause = *K1_LIO_2_ISR_ADDR & *K1_LIO_2_MASK_ADDR;

	while (lc2_cause) {
		register lclvec_t *liv = lcl2vec_tbl+FFINTR(lc2_cause);
		LCL_CALL_ISR(liv,ep,K1_LIO_2_MASK_ADDR,threaded);
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
     uint dmadone, dmadone2, selector=1;
     int offset=1;
     register lclvec_t *liv;

     /* 
      * Bug in HPC3 requires me to read two registers and 'or' the halves
      * together.
      */
     dmadone = *(volatile uint *)PHYS_TO_COMPATK1(HPC3_INTSTAT_ADDR);
     flushbus();		/* ensure reads not too close */
     dmadone2 = *(volatile uint *)PHYS_TO_COMPATK1(HPC3_INTSTAT_ADDR2);
     dmadone = (dmadone2 & 0x3e0) | (dmadone & 0x1f);

     if (dmadone) {
	  do {
	       if (dmadone & selector) {
		    liv = &hpcdmavec_tbl[offset];
		    (*liv->isr)(liv->arg,ep);
		    dmadone &= ~selector;
	       }
	       selector = selector << 1;
	       offset++;
	  } while (dmadone);
     }
}



static void
ip28_gio0_intr(__psint_t arg_extio, struct eframe_s *ep)
{
	uint extio = *(volatile uint *)arg_extio;

	/* mask the interrupt off */
	*((volatile unchar *)PHYS_TO_COMPATK1(LIO_0_MASK_ADDR)) &= ~LIO_FIFO;

	if ( ((extio & EXTIO_SG_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr ) {
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].arg,ep);
	}

	if ( ((extio & EXTIO_S0_IRQ_2) == 0) &&
	     giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr ) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].arg,ep);
	}

	/* re-enable interrupt */
	*((volatile unchar *)PHYS_TO_COMPATK1(LIO_0_MASK_ADDR)) |= LIO_FIFO;
}

static void
ip28_gio1_intr(__psint_t arg_extio, struct eframe_s *ep)
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
ip28_gio2_intr(__psint_t arg_extio, struct eframe_s *ep)
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

static int printstray0;

/*ARGSUSED*/
static void
lcl_stray(__psint_t lvl, struct eframe_s *ep)
{
	static int stray0ints;
	int lcl_id = (lvl-1)/8;
	lvl = (lvl-1)&7;

	if (lcl_id == 0) {
		if(printstray0)	/* so we can turn on with symmon or dbx */
			cmn_err(CE_WARN, "stray local 0 interrupt #%d\n", lvl);
		stray0ints++;	/* for crash dumps, etc. */
	} else {
		cmn_err(CE_WARN, "stray local %d interrupt #%d\n",lcl_id, lvl);
	}
}

/*ARGSUSED*/
static void
hpcdmastray(__psint_t adap, struct eframe_s *ep)
{
	cmn_err(CE_WARN,"stray HPC dmadone interrupt on pbus chan %d\n",adap);
}

/*ARGSUSED*/
static void
hpcdmastray_scsi(__psint_t adap, struct eframe_s *ep)
{
     uint clear;

     if (adap == 9) {
	  clear = *(volatile uint *)PHYS_TO_COMPATK1(HPC3_SCSI_CONTROL0);
     }
     else {
	  clear = *(volatile uint *)PHYS_TO_COMPATK1(HPC3_SCSI_CONTROL1);
     }
     clear &= 0xff;

     if (clear & SCPARITY_ERR) {
	  cmn_err(CE_WARN, "stray parity error on scsi chan %d, control bits 0x%x", adap - 9, clear);
     } else {
	  cmn_err(CE_WARN, "stray HPC dmadone interrupt on scsi chan %d, control bits 0x%x", adap - 9, clear);
     }
}

/*ARGSUSED*/
static void
powerfail(__psint_t arg, struct eframe_s *ep)
{
	int adap;
	extern int wd93cnt;
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

	/* maybe should spin a bit before complaining?  Sometimes
	 * see this when people power off.  May want to use this
	 * for 'clean' shutdowns if we ever get enough power to
	 * write out part of the superblocks, or whatever.  For
	 * now, reset SCSI bus to attempt to ensure we don't wind
	 * up with a media error due to a block header write being
	 * in progress when the power actually fails. */
	for(adap=0; adap<wd93cnt; adap++)
		wd93_resetbus(adap);
	cmn_err(CE_WARN,"Power failure detected\n");
}


/* function to shut off flat panel, set in newport_init.c */
int (*fp_poweroff)(void);

/* can be called via a timeout from powerintr(), direct from prom_reboot
 * in arcs.c, or from mdboot() as a result of a uadmin call.
*/
void
cpu_powerdown(void)
{
	volatile uint *p = (uint *)PHYS_TO_COMPATK1(HPC3_PANEL);
	register ds1286_clk_t *clock = (ds1286_clk_t *)RTDS_CLOCK_ADDR;
	volatile int dummy;
	
	/* shut off the flat panel, if one attached */
	if (fp_poweroff)
	  (*fp_poweroff)();

	/*  Some power supplies give ACFAIL when soft powered down */
	setlclvector(VECTOR_ACFAIL,0,0);

	/* disable watchdog output so machine knows it was turned off */
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

/* While looping, check if the power button is being pressed. */
void
cpu_waitforreset(void)
{
	volatile unchar *i = (unchar *)PHYS_TO_COMPATK1(LIO_1_ISR_ADDR);

	while (1) {
	    if (*i & LIO_POWER)
		    cpu_powerdown();
	}
}

/*ARGSUSED*/
static void
powerintr(__psint_t arg, struct eframe_s *ep)
{
	volatile uint *p = (uint *)PHYS_TO_COMPATK1(HPC3_PANEL);
	static time_t doublepower;
	extern int power_off_flag;	/* set in powerintr(), or uadmin() */

	/* for prom_reboot() in ars.c */
	power_off_flag = 1;

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
		} else {
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

#define GIO_ERRMASK	0xff00
#define CPU_ERRMASK	0x3f00

uint hpc3_ext_io, hpc3_bus_err_stat;
uint cpu_err_stat, gio_err_stat;
uint cpu_par_adr, gio_par_adr;

/* common bus error handling */
static int
dobuserr_common(struct eframe_s *ep, inst_t *epc, uint flag)
{
	int cpu_buserr = 0;
	int cpu_memerr = 0;
	int gio_buserr = 0;
	int gio_memerr = 0;
	int fatal_error = 0;

	/*
	 * we print all gruesome info for 
	 *   1. debugging kernel
	 *   2. bus errors
	 */
	if (kdebug || ((cpu_err_stat & CPU_ERRMASK)))
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
	if (kdebug || (hpc3_ext_io & EXTIO_HPC3_BUSERR))
		cmn_err(CE_CONT, "HPC3 Bus Error %sactive 0x%x\n",
			hpc3_ext_io & EXTIO_HPC3_BUSERR ? "" : "in",
			hpc3_bus_err_stat);
	if (kdebug || (hpc3_ext_io & EXTIO_EISA_BUSERR))
		cmn_err(CE_CONT, "EISA Bus Error %sactive\n",
			hpc3_ext_io & EXTIO_EISA_BUSERR ? "" : "in");

	/*
	 * We are here because the CPU did something bad - if it
	 * read a parity error, then the CPU bit will be on and
	 * the CPU_ERR_ADDR register will contain the faulting word
	 * address. If a parity error happened during an GIO transaction, then
	 * GIO_ERR_ADDR will contain the bad memory address.
	 */
	if (cpu_err_stat & CPU_ERR_STAT_ADDR)
		cpu_buserr = 1;
	else if ((cpu_err_stat & (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR))
	    == (CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR))
		cpu_memerr = 1;			/* Should not happen on IP28 */
	else if (cpu_err_stat & CPU_ERRMASK)
		fatal_error = 1;

	if (gio_err_stat & (GIO_ERR_STAT_TIME | GIO_ERR_STAT_PROM))
		gio_buserr = 1;
	else if (gio_err_stat & (GIO_ERR_STAT_RD|GIO_ERR_STAT_PIO_RD))
		gio_memerr = 1;			/* Should not happen on IP28 */
	else if (gio_err_stat & GIO_ERRMASK)
		fatal_error = 1;

	if (hpc3_ext_io & (EXTIO_HPC3_BUSERR|EXTIO_EISA_BUSERR))
		fatal_error = 1;

	if (cpu_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
			"Process %d [%s] sent SIGBUS due to Bus Error\n",
			current_pid(), get_current_name());
		else
			cmn_err(CE_PANIC,
			"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
			epc, ep);
	}

	if (gio_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
			"Process %d [%s] sent SIGBUS due to Bus Error\n",
			current_pid(), get_current_name());
		else
			cmn_err(CE_PANIC,
			"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
			epc, ep);
	}

	/* The following 'memory' errors should not happen on IP28, but try
	 * to have a good diagnostic incase there are problems with the IP28
	 * board.
	 */
	if (cpu_memerr) {
		/* Make sure error is cleared. */
		*(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
		flushbus();

		cmn_err(CE_PANIC,
		"IRIX Killed due to IP28 CPU_ERR_STAT error: 0x%x\n"
		"\tat PC:0x%x ep:0x%x\n", cpu_err_stat, epc, ep);
	}

	if (gio_memerr) {
		/* Make sure error is cleared. */
		*(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
		flushbus();

		cmn_err(CE_PANIC,
		"IRIX Killed due to IP28 GIO_ERR_STAT error: 0x%x\n"
		"\tat PC:0x%x ep:0x%x\n", gio_err_stat, epc, ep);
	}

	if (fatal_error) {
		cmn_err(CE_PANIC,
		"IRIX Killed due to internal error\n\tat PC:0x%x ep:0x%x\n", 
		epc, ep);
	}

	return(0);
}

int ip28_cpu_specerrs;		/* count of speculative mem misses */
int ip28_gio_specerrs;
int ip28_hpc3_dmaerrs;

/* Local buserror interrupt hook handler to clear speculative exectution
 * errors to MC.  Done here as we often brk in dobuserr but there is
 * a race clearing this error in symmon vs the kernel, so do it a bit
 * early to avoid the race.  Hence, one should probably avoid setting
 * a break point here.
 */
static void
_buserror_intr(struct eframe_s *ef)
{
	uint err = *(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT);

	/*  Speculative cached loads outside of the bounds of memory are
	 * partially handled by hardware by giving an error responce.  For
	 * speculative loads, these will be tossed by the processor, but
	 * we still get a bus error interrupt from MC.
	 */
	if (err & CPU_ERR_STAT_ADDR) {
		ip28_cpu_specerrs++;
		*(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
		flushbus();
		flushbus();		/* wait a bit more for intr to clear */
		return;
	}

	/*  Turns out the CPU can speculate to the GIO side of the MC as
	 * well.  We always thought this was true, but hoped it wasn't.
	 * This covers accesses to non-existant hardware, not if we hit
	 * something that does not like a cache miss.
	 *
	 *  The good news is that non speculative accesses still get DBEs
	 * in addition to the IP7, so we don't loose timeouts completely.
	 */
	err = *(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT);
	if (err & GIO_ERR_STAT_TIME) {
		ip28_gio_specerrs++;
		*(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
		flushbus();
		flushbus();		/* wait a bit more for intr to clear */
		return;
	}

	/*  Seem to get a lot of HPC3 errors.  This doesn't look like
	 * speculation as it seems to be DMA related.  I think it's
	 * related to the GIO parity issue.  Only clear DMA errors.
	 */
	hpc3_ext_io = *(volatile uint *)PHYS_TO_COMPATK1(HPC3_EXT_IO_ADDR);
	flushbus();				/* make sure branch resolved */
	if (hpc3_ext_io & EXTIO_HPC3_BUSERR) {
		hpc3_bus_err_stat = *(volatile uint *)
			PHYS_TO_COMPATK1(HPC3_BUSERR_STAT_ADDR);

		/* This bit is set on DMAs */
		if (hpc3_bus_err_stat & 0x100) {
			ip28_hpc3_dmaerrs++;
			flushbus();		/* read clears error */
			flushbus();
			return;
		}
	}

	buserror_intr(ef);		/* normal intr path */
}

/*
 * dobuserre - Common IP28 bus error handling code
 *
 * The MC sends an interrupt whenever bus or parity errors occur.
 * Unlike the R4000 we will not get here on a parity error on a
 * memory ready as SysAD parity if off.  We may however get here
 * if we non-speculatively read to an illegal addresss as the SysAD
 * PAL will give an Err respoce to the T5, causing a Bus error.
 *
 * Code in VEC_ibe and VEC_dbe saves the MC bus error registers and
 * then clear the interrupt when this happens.
 *
 * flag:	0: kernel; 1: kernel - no fault; 2: user
 */
int
dobuserre(struct eframe_s *ep, inst_t *epc, uint flag)
{
	if (flag == 1)
		return(0); /* nofault */

	return(dobuserr_common(ep,epc,flag));
}

/*
 * dobuserr - handle bus error interrupt
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 */
int
dobuserr(struct eframe_s *ep, inst_t *epc, uint flag)
{
	/* save MC/HPC3 error info -- ext_io should be read 1st */
	cpu_err_stat = *(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT);
	gio_err_stat = *(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT);
	cpu_par_adr  = *(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_ADDR);
	gio_par_adr  = *(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_ADDR);

	/* If we are on the IP26 motherboard, we are hurting.  We may have
	 * gotten an ECC error that could not be reported sometime in the
	 * past, so go ahead and attempt to clear errrors here.
	 */
	if (is_ip26bb()) ip28_clear_ecc();

	/*
	 * Clear the bus error by writing to the parity error register
	 * in case we're here due to parity problems.
	 */
	*(volatile uint *)PHYS_TO_COMPATK1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_COMPATK1(GIO_ERR_STAT) = 0;
	flushbus();

	return(dobuserr_common(ep, epc, flag));
}

/*
 * Should never receive an exception (other than interrupt) while
 * running on the idle stack.
 * Check for memory errors
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
	if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	    (cause & CAUSE_EXCMASK) == EXC_DBE)
		dobuserre(NULL, epc, 0);
	else if (cause & CAUSE_BERRINTR)
		dobuserr(NULL, epc, 0);

	cmn_err(CE_PANIC,
	"exception on IDLE stack k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
		k1, epc, cause, sp, getbadvaddr());
	/* NOTREACHED */
}

/*
 * earlynofault - handle very early global faults
 * Returns: 1 if should do nofault
 *	    0 if not
 */
int
earlynofault(struct eframe_s *ep, uint code)
{
	switch(code) {
	case EXC_DBE:
		dobuserre(ep, (inst_t *)ep->ef_epc, 1);
		break;
	default:
		return(0);
	}
	return(1);
}

/*
 * fp_find - probe for floating point chip
 */
void
fp_find(void)
{
	private.p_fputype_word = get_fpc_irr();
}

/*
 * fp chip initialization
 */
void
fp_init(void)
{
	set_fpc_csr(0);
}

/*
 * pg_setcachable set the cache algorithm pde_t *p. Since there are
 * several cache algorithms for the R4000, (non-coherent, update, invalidate),
 * this function sets the 'basic' algorithm for a particular machine. IP22
 * uses non-coherent.
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

/*  npage and flags aren't used for IP28, since it is only used for SCSI
 * and EISA.
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
			return((dmamap_t *)-1L);
		smap[adap].dma_type = DMA_SCSI;
		smap[adap].dma_adap = adap;
		return &smap[adap];

	    case DMA_EISA:
		if (adap != 0)
			return((dmamap_t *)-1L);
		dmamap = (dmamap_t *) kern_calloc(1, sizeof(dmamap_t));

		dmamap->dma_type = DMA_EISA;
		dmamap->dma_adap = 0;
		dmamap->dma_size = npage;
		return (dmamap);

	    default:
		return((dmamap_t *)-1L);
	}
}

/*  Macros to support multiple 4096 byte HPC SCSI DMA sizes.  For large
 * pages (16K+) we can map the page in 4K or 8K DMA descriptors with HPC3.
 * 8K pages seem to work well, even though the hardware guys are not sure
 * so, use them as it makes the jj
 */
#if NBPP != 16384
#error "IP28 assumed 16KB pages!"
#endif
#define _USE_HPCSUBPAGE	1
#ifndef USE4KSCSIDMA
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
#else
#define HPC_NBPP	IO_NBPP
#define HPC_BPCSHIFT	IO_BPCSHIFT
#define HPC_NBPC	IO_NBPC
#define HPC_POFFMASK	IO_POFFMASK
#define HPC_PGFCTR	PGFCTR
#define hpc_poff(x)	io_poff(x)
#define hpc_ctob(x)	io_ctob(x)
#define hpc_btoc(x)	io_btoc(x)
#define hpc_btoct(x)	io_btoct(x)
#endif

#if _USE_HPCSUBPAGE
/* Macro for extracting HPC 'sub-page' on large page sized systems */
#define HPC_PAGENUM(A)		(((__psunsigned_t)(A)&POFFMASK)&~HPC_POFFMASK)
#define HPC_PAGEMAX		HPC_PAGENUM(0xffffffff)
#endif

/*
 *	Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *	Returns the actual number of bytes mapped.
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
			/* -1 for HPC3_EOX_VALUE */
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
			consistancy_check_wd93(*cbp,*bcnt,"dma_map leading partial",CCF_REQUIRE_NODIRTY);
#if CACHEFLUSH_DEBUG
			docachelog(PHYS_TO_K0(*cbp),bytes,"dma_map");
#endif
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
			consistancy_check_wd93(*cbp,*bcnt,"dma_map",CCF_REQUIRE_NODIRTY);
#if CACHEFLUSH_DEBUG
			docachelog(PHYS_TO_K0(*cbp),incr,"dma_map");
#endif
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

/* avoid overhead of testing for KDM each time through; this is
	the second part of kvtophys() */
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
	else
		return (KNOTDM_TO_PHYS(addr));
}

void
dma_mapfree(dmamap_t *map)
{
	if (map && map->dma_type == DMA_EISA)
		kern_free(map);
}

/*
 * dma_mapbp -	Map `len' bytes of a buffer for a SCSI DMA op.
 *		Returns the actual number of bytes mapped.
 * Only called when a SCSI request is started, unlike the other
 * machines, except when the request is larger than we can fit
 * in a single dma map.  In those cases, the offset will always be
 * page aligned, same as at the start of a request.
 * While starting addresses will always be page aligned, the last
 * page may be a partial page.
 *
 *	NOTE: Can't use HPC3 8K pages on IP22 for HPC1 compatability.
 */
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

	xoff = hpc_poff(xoff);
	npages = hpc_btoc(len + xoff);
	if (npages > (map->dma_size-1)) {
		/* -1 for HPC3_EOX_VALUE */
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
		*cbp = pfdattophys(pfd) + xoff;
#else
		*cbp = pfdattophys(pfd) | (io_pfd_cnt*HPC_NBPP) | xoff;
#endif
		*bcnt = MIN(bytes, HPC_NBPC - xoff);

		consistancy_check_wd93(*cbp,*bcnt,"dma_mapbp",CCF_REQUIRE_NODIRTY);
#if CACHEFLUSH_DEBUG
		docachelog(PHYS_TO_K0(*cbp),*bcnt,"dma_mapbp");
#endif

		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);

		bytes -= HPC_NBPC - xoff;
		xoff = 0;
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

/*	This is a hack to make the Western Digital chip stop
 *	interrupting before we turn interrupts on.
 *	ALL IP22's have the 93B chip, and the fifo with the
 *	burstmode dma, so just set the variable.
 */
void
wd93_init(void)
{
	int		 adap;
	scuzzy_t	*scp;
	extern int	 wd93_earlyinit(int);
	extern int	 wd93burstdma, wd93cnt;

	ASSERT(NSCUZZY <= SCSI_MAXCTLR);	/* sanity check */
	wd93burstdma = 1;

	wd93cnt = 2;

	/* early init, need low level before high
	 * level devices do their io_init's (needs some
	 * work for multiple SCSI on setlclvector) */
	for (adap=0; adap<wd93cnt; adap++) {
		/* init mother board channels (must start with adap 0) */
		if (wd93_earlyinit(adap) == 0) {
			scp = &scuzzy[adap];
			setlclvector(scp->d_vector,
				(lcl_intr_func_t *)wd93intr,
				adap);
		}
	}
}	

/*
 * Reset the SCSI bus. Interrupt comes back after this one.
 * this also is tied to the reset pin of the SCSI chip.
 */
void
wd93_resetbus(int adap)
{
	scuzzy_t *sc = &scuzzy[adap];

	*sc->d_ctrl = SCSI_RESET;	/* write in the reset code */
	DELAY(25);			/* hold 25 usec to meet SCSI spec */
	*sc->d_ctrl = 0;		/* no dma, no flush */
}


/*
 * Set the DMA direction, and tell the HPC to set up for DMA.
 * Have to be sure flush bit no longer set.
 * dma_index will be zero after dma_map() is called, but may be
 * non-zero after dma_flush, when a target has re-connected partway
 * through a transfer.  This saves us from having to re-create the map
 * each time there is a re-connect.
 * addr arg isn't needed for IP22.
 */
/*ARGSUSED2*/
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{
	scuzzy_t *sc = &scuzzy[map->dma_adap];

#if CONSISTANCY_CHECK_WD93
	unsigned index = map->dma_index;
	unsigned fcnt = index * sizeof (scdescr_t);
	unsigned long dvba = (unsigned long)map->dma_virtaddr + fcnt;

	consistancy_check_wd93_list((unsigned *)(dvba + HPC3_CBP_OFFSET),
				    (unsigned *)(dvba + HPC3_BCNT_OFFSET),
				    readflag?"wd93_go rd":"wd93_go wr",
				    CCF_REQUIRE_NODIRTY);
#endif
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
	 * VM_CACHEALIGN will align to CACHE_SLINE_SIZE which is good enough.
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
int
wd93dma_flush(dmamap_t *map, int readflag, int xferd)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned *bcnt, *cbp, index, fcnt;
#if R10000_SPECULATION_WAR
	unsigned *nbcnt, *ncbp, addr, cnt;
#endif

	if (readflag && xferd) {
		fcnt = 512;	/* normally only 2 or 3 times */
		*scp->d_ctrl |= SCSI_FLUSH;

		while((*scp->d_ctrl & SCSI_STARTDMA) && --fcnt)
			;
		if(fcnt == 0)
			cmn_err(CE_PANIC,
			   "SCSI DMA in progress bit never cleared\n");
#if R10000_SPECULATION_WAR
#if USE4KSCSIDMA
#error "wd93dma_flush depends on 8K IO pages!"
#endif
		/* index from last wd93_go */
		index = map->dma_index;
		fcnt = index * sizeof (scdescr_t);
		cbp = (uint *)(map->dma_virtaddr + HPC3_CBP_OFFSET + fcnt);
		bcnt = (uint *)(map->dma_virtaddr + HPC3_BCNT_OFFSET + fcnt);

		fcnt = xferd;
		while (fcnt > 0) {
			if (*bcnt == HPC3_EOX_VALUE)
				break;
			addr = *cbp;
			/* try to merge two IO pages into one phys page */
			if (fcnt >= NBPP && poff(*cbp) == 0 &&
			    *bcnt == HPC_NBPP) {
				ncbp = cbp + sizeof(scdescr_t) / sizeof(uint);
				nbcnt = bcnt + sizeof(scdescr_t) / sizeof(uint);
				if ((*nbcnt == HPC_NBPP) &&
				    (*ncbp == (*cbp + HPC_NBPP))) {
					cbp = ncbp;	/* bumped 2x by below */
					bcnt = nbcnt;
					cnt = NBPP;
				}
				else
					cnt = *bcnt;
			}
			else
			if (fcnt < *bcnt)
				cnt = fcnt;
			else
				cnt = *bcnt;

			__dcache_inval((void *)PHYS_TO_K0(addr),cnt);
			consistancy_check_wd93(addr,cnt,"wd93dma_flush",CCF_REQUIRE_NODIRTY);

			fcnt -= cnt;
			cbp  += sizeof(scdescr_t) / sizeof(uint);
			bcnt += sizeof(scdescr_t) / sizeof(uint);
		}

		/* ok to keep descriptors cached as is consistant with mem */
#endif /* R10000_SPECULATION_WAR */
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		* disconnects without transferring data, and HPC already
		* grabbed the first 64 bytes into it's fifo; otherwise gets
		* the first 64 bytes twice, since it 'notices' that curbp was
		* reset, but not that is the same.  */
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

		consistancy_check_wd93(*cbp,*bcnt,"wd93dma_flush new partial",CCF_REQUIRE_NODIRTY);

		/*  Flush descriptor back to memory so HPC gets right data.
		 * Only one descriptor, and will not cross a page so use
		 * quick-n-dirty routine.
		 */
		__dcache_line_wb_inval((void *)
			((scdescr_t *)map->dma_virtaddr + index));
	}

	/* prepare for rest of dma (if any) after next reconnect */
	map->dma_index = index;

	return(0);
}


/*
 * wd93_int_present - is a wd93 interrupt present
 * In machine specific code because of IP4
 */
wd93_int_present(wd93dev_t *dp)
{
	/* reads the aux register */
	return(*dp->d_addr & AUX_INT);
}


/*
 * Routines for LED handling
 */

/*
 * Initialize led counter and value
 */
void
reset_leds(void)
{
	set_leds(0);
}


/*
 * Implementation of syssgi(SGI_SETLED,a1)
 *
 */
void
sys_setled(int a1)
{
	set_leds(a1);
}

/*
 * Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to make
 * it change color.  We don't allow UNIX to turn the LED off.  Also, UNIX
 * is not allowed to turn the LED *RED* (a big no no in Europe).
 */

void
set_leds(int pattern)
{
	_hpc3_write1 &= ~(LED_AMBER|LED_GREEN);
	_hpc3_write1 |= pattern ? LED_AMBER : LED_GREEN;
	*(volatile uint *)PHYS_TO_COMPATK1(HPC3_WRITE1) = _hpc3_write1;
}

/*
 * sendintr - send an interrupt to another CPU; fatal error on IP22.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	panic("sendintr");
	/*NOTREACHED*/
}


/* START of Time Of Day clock chip stuff (to "END of Time Of Day clock chip" */

static void _clock_func(int func, char *ptr);
static void setbadclock(void), clearbadclock(void);
static int isbadclock(void);
extern u_char miniroot;

/*
 * Get the time/date from the ds1286, and reinitialize
 * with a guess from the file system if necessary.
 */
void
inittodr(time_t base)
{
	register uint todr;
	long deltat;
	int TODinvalid = 0;
	extern int todcinitted;
	int s,dow;
	char buf[7];

	s = mutex_spinlock_spl(&timelock, splprof);

	todr = rtodc(); /* returns seconds from 1970 00:00:00 GMT */
	settime(todr,0);

	if (miniroot) {	/* no date checking if running from miniroot */
		mutex_spinunlock(&timelock, s);
		return;
	}

	/*
	 * If day of week not set then set it.
 	 */
	dow = get_dayof_week(todr);
	_clock_func(WTR_READ, buf);
	if (dow != buf[6]) {
#if DEBUG
		cmn_err(CE_DEBUG,"resetting dallas clock day of week.\n");
#endif
		buf[6] = dow;
		_clock_func( WTR_WRITE, buf );
	}

	/*
	 * Complain if the TOD clock is obviously out of wack. It
	 * is "obvious" that it is screwed up if the clock is behind
	 * the 1988 calendar currently on my wall.  Hence, TODRZERO
	 * is an arbitrary date in the past.
	 */
	if (todr < TODRZERO) {
		if (todr < base) {
			cmn_err(CE_WARN, "lost battery backup clock--using file system time");
			TODinvalid = 1;
		} else {
			cmn_err(CE_WARN, "lost battery backup clock");
			setbadclock();
		}
	}

	/*
	 * Complain if the file system time is ahead of the time
	 * of day clock, and reset the time.
	 */
	if (todr < base) {
		if (!TODinvalid)	/* don't complain twice */
			cmn_err(CE_WARN, "time of day clock behind file system time--resetting time");
		settime(base, 0);
		resettodr();
		setbadclock();
	}
	todcinitted = 1; 	/* for chktimedrift() in clock.c */

	/*
	 * See if we gained/lost two or more days: 
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


/* 
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */

void
resettodr(void) {
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

int
rtodc(void)
{
	uint month, day, year, hours, mins, secs;
	register int i;
	char buf[7];

	_clock_func(WTR_READ, buf);

	secs = (uint)( buf[ 0 ] & 0xf );
	secs += (uint)( buf[ 0 ] >> 4 ) * 10;

	mins = (uint)( buf[ 1 ] & 0xf );
	mins += (uint)( buf[ 1 ] >> 4 ) * 10;

	/*
	 * we use 24 hr mode, so bits 4 and 5 represent the 2 tens-hour bits
	 */
	hours = (uint)( buf[ 2 ] & 0xf );
	hours += (uint)( ( buf[ 2 ] >> 4 ) & 0x3 ) * 10;

	/*
	 * actually day of month
	 */
	day = (uint)( buf[ 3 ] & 0xf );
	day += (uint)( buf[ 3 ] >> 4 ) * 10;

	month = (uint)( buf[ 4 ] & 0xf );
	month += (uint)( buf[ 4 ] >> 4 ) * 10;

	year = (uint)( buf[ 5 ] & 0xf );
	year += (uint)( buf[ 5 ] >> 4 ) * 10;
	year += DALLAS_YRREF;

	/*
	 * Sum up seconds from 00:00:00 GMT 1970
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

void
wtodc(void)
{
	register long year_secs = time;
	register month, day, hours, mins, secs, year, dow;
	char buf[7];

        /*
         * calculate the day of the week
         */
        dow = get_dayof_week(year_secs);

	/*
	 * Whittle the time down to an offset in the current year,
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
	/*
	 * Break seconds in year into month, day, hours, minutes, seconds
	 */
	if (LEAPYEAR(year))
		month_days[1]++;
	for (month = 0;
	   year_secs >= month_days[month]*SECDAY;
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

	buf[ 0 ] = (char)( ( ( secs / 10 ) << 4 ) | ( secs % 10 ) );

	buf[ 1 ] = (char)( ( ( mins / 10 ) << 4 ) | ( mins % 10 ) );

	buf[ 2 ] = (char)( ( ( hours / 10 ) << 4 ) | ( hours % 10 ) );

	buf[ 3 ] = (char)( ( ( day / 10 ) << 4 ) | ( day % 10 ) );

	buf[ 4 ] = (char)( ( ( month / 10 ) << 4 ) | ( month % 10 ) );

	/*
	 * The number of years since DALLAS_YRREF (1940) is what actually
	 * gets stored in the clock.  Leap years work on the ds1286 because
	 * the hardware handles the leap year correctly.
	 */
	year -= DALLAS_YRREF;
	buf[ 5 ] = (char)( ( ( year / 10 ) << 4 ) | ( year % 10 ) );
	/*
	 * Now that we are done with year stuff, plug in day_of_week
	 */
	buf[ 6 ] = (char) dow;

	_clock_func( WTR_WRITE, buf );

	/*
	 * clear the flag in nvram that says whether or not the clock
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
		clock->month = *ptr++ & 0x1f;		/* make sure ESQW=0 */
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

/*
 * Called from prom_reinit(), in case syssgi(SET_AUTOPWRON) was done.
 * If time of the day alarm vars are to be set then program
 * RTC to power system back on using time-of-day alarm, unless the
 * current time is already past that value.
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

	/*
	 * calculate the day of the week 
	 */
	day_of_week = get_dayof_week(year_secs);
	/*
	 * Whittle the time down to an offset in the current year, by
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
	/*
	 * Break seconds in year into month, day, hours, minutes,
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
		clock->min_alarm = (char) (((mins / 10) << 4) | (mins % 10));
		clock->hour_alarm = (char) (((hours / 10) << 4) | (hours % 10));
		clock->day_alarm = (char) (((day_of_week / 10) << 4) |
					    (day_of_week % 10));

		command = clock->command;
		command |= WTR_TIME_DAY_INTA;		/* Enable INTA */
		command &= ~WTR_DEAC_TDM;		/* Activate TDM */
		clock->command = command;
	}
}

/*
 * converts year_secs to the day of the week 
 * (i.e. Sun = 1 and Sat = 7)
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
        } else
                days +=EXTRA_DAYS+1;
        return(days+1);

}

/* END of Time Of Day clock chip stuff */

/*	called from  clock() periodically to see if system time is 
	drifting from the real time clock chip. Seems to take about
	about 30 seconds to make a 1 second adjustment, judging
	by my debug printfs.  Only do if different by 2 seconds or more.
	1 second difference often occurs due to slight differences between
	kernel and chip, 2 to play safe.
	Main culprit seems to be graphics stuff keeping interrupts off for
	'long' times; it's easy to lose a tick when they are happening every
	milli-second.  Olson, 8/88
*/
void
chktimedrift(void)
{
	long todtimediff;

	todtimediff = rtodc() - time;
	if((todtimediff > 2) || (todtimediff < -2))
		(void)doadjtime(todtimediff * USEC_PER_SEC);
}

/*	Fetch the nvram table from the PROM.
 *	The table is static because we can't call kern_malloc at this point,
 *	and allocating whole pages would be wasteful.  We can at least tell
 *	if the table is larger than we think it is by the return value.
 * NOTE: The actual size of the table, including the null entry at the
 *       end is 30 members as of 9/94.
*/

#define NVRAMTAB 31
static struct nvram_entry nvram_tab[NVRAMTAB];

void
load_nvram_tab(void)
{
#if DEBUG
	if (arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab)) > 0)
		cmn_err(CE_WARN,"IP28 nvram table has grown!\n");
#else
	arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab));
#endif
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

/*
 * Do the following check to make sure that a wrong config
 * in system.gen won't cause the kernel to hang.  Ignores
 * the MP stuff and just does some basic sanity setup.
 *
 * Also initalize the ECC handler.
 */
void
allowboot(void)
{
	void ecc_init(void);
	int i;	

	ecc_init();		/* initialize ECC handler */

	for (i=0; i<bdevcnt; i++)
		bdevsw[i].d_cpulock = 0;

	for (i=0; i<cdevcnt; i++)
		cdevsw[i].d_cpulock = 0;

#ifdef US_DELAY_DEBUG
	check_us_delay();
#endif

#if CACHEFLUSH_DEBUG
	idbg_addfunc("cachelog",idbg_cachelog);
#endif
}

/*
 * If the debugger isn't present, set up the NMI crash dump vector.
 */
static void
init_nmi_dump(void)
{
	/* If the debugger hasn't been there, set the vector.  We know this
	 * as symmon links at 0x98, while others are in compat space,
	 * uncached, or in normal K0 (0xa8).
	 */
	if ((GDA->g_nmivec & 0xf800000000000000) == 0x9800000000000000)
		return;

	/* set the vector */
	GDA->g_nmivec = (__psunsigned_t)nmi_dump;

	/*  Make sure the real master is the one that runs the dump code.
	 * Since we are single processor set that to cpu 0.
	 */
	GDA->g_masterspnum = (uint)0;
}

/* nmi_dump has set up our stack.  Now we must make everything else
 * ready and call panic.
 */
void
cont_nmi_dump(void)
{
	/* Wire in the master's PDA. */
	wirepda(pdaindr[master_procid].pda);

	/*  ECC multi-bit handling -- normally won't return unless we
	 * have a lab generated NMI.
	 */
	(void)ip28_ecc_error();

	cmn_err_tag(48,CE_PANIC, "User requested vmcore dump (NMI)");
}

/*
 * get_except_norm
 *
 *	return address of exception handler for all exceptions other
 *	then utlbmiss.
 */
inst_t *
get_except_norm(void)
{
	extern inst_t exception[];

	return exception;
}

/* 
 * return board revision level (0-7)
 */
#define K1_SYS_ID	(volatile uint *)PHYS_TO_COMPATK1(HPC3_SYS_ID)
int
board_rev(void)
{
	static int rev = -1;

	if (rev == -1)
		rev = (*K1_SYS_ID & BOARD_REV_MASK) >> BOARD_REV_SHIFT;

	return rev;
}

static void
check_ctrld(void)
{
	volatile uint *ctrldp = (volatile uint *)PHYS_TO_K1(CTRLD);
	unsigned int ec, ctrld;

	if (*ctrldp >= 0xd00) {
		ec = (get_r10k_config()&CONFIG_EC) >> CONFIG_EC_SHFT;
		ctrld = ((cpu_mhz_rating() * sysclk_div_n[ec]) * 125) /
			(2 * sysclk_div_d[ec]);

		*ctrldp = ctrld;
		flushbus();
		cmn_err(CE_WARN,"Old IP28 prom installed -- memory refresh was"
			" incorrectly configured [fixed].");
	}
}

void
add_ioboard(void)	/* should now be add_ioboards() */
{
	add_to_inventory(INV_BUS,INV_BUS_EISA,0,0,4);
}

int ip22_bufpio_adjustment;
char *cpu_rev_find( int, int *, int * );

void
add_cpuboard(void)
{
	int maxreads;

	add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);

	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(), 0,
			 private.p_fputype_word);
	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(), 0,
			 private.p_cputype_word);

	ip22_bufpio_adjustment = 0;

	add_to_inventory(INV_PROCESSOR, INV_CPUBOARD,
			 local_cpu_mhz_rating = cpu_mhz_rating(), -1,
			 INV_IP28BOARD);

	/* Check for IP26 baseboard */
	if (is_ip26bb() && !is_ip26pbb()) {
		/* IP26+ is covered under this message. */
		cmn_err(CE_WARN, "CPU baseboard downrev (IP26 not IP28)."
				 "  Not for customer use.");
	}

	maxreads = ((get_r10k_config() & CONFIG_PM) >> CONFIG_PM_SHFT) + 1;
	if (maxreads != 1) {
		cmn_err(CE_WARN, "Prototype R10000 config.  %d != 1 "
				 "outstanding reads.",maxreads);
	}

	if (badaddr((void *)PHYS_TO_K0(CPUCTRL0),4) == 0) {
		cmn_err(CE_WARN, "Processor Module allows GIO speculation.\n");
	}

	/* notify if margining is set */
	if (_hpc3_write2 & MARGIN_LO)
		cmn_err(CE_WARN,"Power supply is margined low.\n");
	else if (_hpc3_write2 & MARGIN_HI)
		cmn_err(CE_WARN,"Power supply is margined high.\n");
}

int
cpuboard(void)
{
	return INV_IP28BOARD;
}

/*
 * On IP28, unix overlays the prom's bss area.  Therefore, only
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
	/*
	 * Check for presence of symmon
	 */
	if ( SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf ) {
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	} else {
		/*
		 * cn_is_inited() implies that PROM bss has been wiped out
		 */
		if (cn_is_inited())
			cmn_err(CE_CONT,fmt,ARGS);
		else {
			/*
			 * try printing through the prom
			 */
			arcs_printf (fmt,ARGS);
		}
	}
}

int
readadapters(uint_t bustype)
{
	switch (bustype) {
		
	case ADAP_SCSI:
		return wd93cnt;

	case ADAP_LOCAL:
		return(1);

	case ADAP_EISA:
		return(1);

	case ADAP_GIO:
		return(1);

	default:
		return(0);

	}
}

piomap_t*
pio_mapalloc (uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
	int	 i;
	piomap_t *piomap;

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
			piomap->pio_vaddr = (caddr_t)(EISAIO_TO_K1(iospace->ios_iopaddr));
			break;

		    case PIOMAP_EISA_MEM:
			piomap->pio_vaddr = (caddr_t)(EISAMEM_TO_K1(iospace->ios_iopaddr));
			eisa_refresh_on();
			break;

		    default:
			kern_free(piomap);
			return 0;
		}

	return(piomap);
}

caddr_t 
pio_mapaddr(piomap_t *piomap, iopaddr_t addr)
{
	switch(piomap->pio_bus){

	/* Change needed to support New form of VECTOR lines. */
	case ADAP_GIO:	
		return((caddr_t)(PHYS_TO_K1(addr)));

	default: 
		return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
	}
}

void
pio_mapfree (piomap_t* piomap)
{
	if (piomap)
		kern_free(piomap);
}

/* Compute the number of picosecs per cycle counter tick.
 *
 * Note that if the cpu clock does not divide nicely into 2000000,
 * the cycle count will not be exact.  (Random samples of possible
 * clock rates show the difference will be less than .003% of the
 * exact cycle count).
 */
__psint_t
query_cyclecntr(uint *cycle)
{
	unsigned int ec = (get_r10k_config() & CONFIG_EC) >> CONFIG_EC_SHFT;

	ASSERT(ec != 0);

	*cycle = (RPSS_DIV * 1000000 * sysclk_div_d[ec]) /
		 (local_cpu_mhz_rating * sysclk_div_n[ec]);

	return PHYS_TO_K1(RPSS_CTR);
}
/*
 * This routine returns the value of the cycle counter. It needs to
 * be the same counter that the user sees when mapping in the CC
 */
__int64_t
absolute_rtc_current_time(void)
{
	unsigned int time;
	time =  *(unsigned int *) PHYS_TO_COMPATK1(RPSS_CTR);
	return((__int64_t) time);
}


/* Keyboard bell using EIU 8254 timer.  This is only for Fullhouse.
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
	int s = splhi();;

	quiet_bell();

	bell.tail = pckbd_qnext(bell.tail);
	if (bell.tail != bell.head) {
		start_bell(bellq[bell.tail].pitch,bellq[bell.tail].duration,0);
	}
	if (bell.flags & BELLASLEEP) {
		bell.flags &= ~BELLASLEEP;
		splx(s);
		wakeup(bellq);
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

	/* insure queue space, or sleep until space (or signal).
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

/* On IP28, with the Schroder PAL, we cannot turn on SysAD parity checking
 * as the PALs do not forward this into correct SysAD ECC.  Since the memory
 * system is ECC memory is ok, and these errors are reported seperately.  GIO
 * parity errors will come in on IP7.  We still use hook to check mmap table.
 */
void
perr_init(void)
{
	check_ctrld();		/* fix and old prom problem with mem refresh */
        check_mmmap();          /* hook here so we get called main */
}

#if DEBUG

#include "sys/immu.h"

#define pteccvalid(pte)		\
	(pte.pte_cc == (PG_NONCOHRNT >> PG_CACHSHFT) || \
	 pte.pte_cc == (PG_UNCACHED >> PG_CACHSHFT)  || \
	 pte.pte_cc == (PG_COHRNT_EXLWR >> PG_CACHSHFT))

uint tlbdropin(
	unsigned char *tlbpidp,
	caddr_t vaddr,
	pte_t pte)
{
	uint _tlbdropin(unsigned char *, caddr_t, pte_t);

	if (pte.pte_vr)
		ASSERT(pteccvalid(pte));

	return(_tlbdropin(tlbpidp, vaddr, pte));
}

void tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
	void _tlbdrop2in(unsigned char, caddr_t, pte_t, pte_t);

	if (pte.pte_vr)
		ASSERT(pteccvalid(pte));
	if (pte2.pte_vr)
		ASSERT(pteccvalid(pte2));

	_tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}


#endif	/* DEBUG */

/*
 * timer_freq is the frequency of the 32 bit counter timestamping source.
 *
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 *
 * Must be called early, before main().
 */
void
timestamp_init(void)
{
	extern int findcpufreq_raw(void);

	timer_freq = findcpufreq_raw();
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;	/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

/* Code for handling ECC parts on IP26/IP28 baseboards.
 */

/* is IP26 or IP26+ baseboard */
int
is_ip26bb(void)
{
	return (board_rev() <= (IP26_ECCSYSID >> BOARD_REV_SHIFT));
}

/* is IP26+ baseboard */
int
is_ip26pbb(void)
{
	return (board_rev() == (IP26P_ECCSYSID >> BOARD_REV_SHIFT));
}

/*
 * ip28_clear_ecc
 * Clear old single-bit errors and uncached writes
 */
static void
ip28_clear_ecc(void)
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

/* Rescan low/high side of a memory bank to see if we can determine if
 * the error is a transient, or a hard error.  We may get unlucky and
 * hit a second multi-bit error, but there isn't much we can do.
 */
static void
ecc_rescan(volatile __uint64_t *ptr, int size)
{
	cmn_err(CE_WARN,"Rescanning @ 0x%x for %d double words.\n", ptr,size);
	while (size--) {
		*ptr;
		ptr += 2;	/* only scan 1 leaf of memory */
	}
}

/*
 * This routine gets called whenever the ECC support PALs detect a notable
 * condition.   These can be multi-bit ECC errors or uncached writes to
 * memory in "fast mode".  Single-bit errors are no longer reported.
 *
 * These errors are reported to the CPU via the NMI handler.
 */
/*ARGSUSED1*/
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

	/* one would think to call ip28_clear_ecc() here, but it hangs */

	if (ecc_stat & ECC_STATUS_UC_WR) {
		/* Uncached write */
		cmn_err(CE_PANIC,"\n%s\n"
			"IRIX Killed due to fatal memory ECC error. [0x%x]\n",
			IP26_UC_WRITE_MSG,ecc_stat);
		/*NOTREACHED*/
	}

	/* Multi-bit ECC error */
	cmn_err(CE_WARN,
		"IRIX detected multi-bit ECC error (%s) in SIMM pair%s%s."
		"[0x%x]\n",
		ecc_stat & ECC_STATUS_GIO ? "GIO" : "CPU",
		ecc_stat & ECC_STATUS_LOW ? simmpair[bank][0] : "",
		ecc_stat & ECC_STATUS_HIGH ? simmpair[bank][1] : "",
		ecc_stat);

	if (ip28_nested_NMI++ == 0) {
		__uint64_t *ptr;
		int size;

		ip28_clear_ecc();		/* clear memory error */
		bank--;				/* bank calls are 0 based */
		
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
			cmn_err(CE_WARN,
				"Bank syndrome error, rescan all RAM.\n"); 

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

	cmn_err_tag(49,CE_PANIC,
		    "IRIX Killed due to fatal memory ECC error. [%s]\n",
		    (ip28_nested_NMI == 1) ? "transient" : "hard");

	/*NOTREACHED*/
}

#if DEBUG
#include <sys/errno.h>
/*
 * ecc_gun
 *
 * Test memory ECC logic, and software handlers as much as possible.
 * Currently, takes one of 3 commands:
 *  1) EGUN_MODE -- Switch between fast and slow modes.
 *  2) EGUN_UC_WRITE -- Do uncached write, and see if we get the error.
 *  3) EGUN_UC_WRITE_D -- Do 64-bit uncached write -- should work!
 */
/*ARGSUSED*/
int
ecc_gun(int cmd, void *cmd_data, rval_t *rval)
{
#define SCRATCH_PAD	0x0c00
	ulong prev;
	int s;

	switch (cmd) {
	case EGUN_MODE:			/* stress test mode switching code */
		prev = ip28_enable_ucmem();
		*(volatile int *)PHYS_TO_K1(SCRATCH_PAD) = 0;
		ip28_return_ucmem(prev);
        	break;

	case EGUN_UC_WRITE:		/* expect panic */
		*(volatile int *)PHYS_TO_K1(SCRATCH_PAD) = 0;
		flushbus();
		us_delay(10);
		cmn_err(CE_CONT, "ecc_gun: expected ucwrite panic -- "
			"Maybe we are in slow mode.\n");
		return EBUSY;

	case EGUN_UC_WRITE_D:		/* should work */
		s = splhi();
		*(volatile long *)PHYS_TO_K1(SCRATCH_PAD) =
			0x0123456789abcdef;
		*(volatile long *)PHYS_TO_K1(SCRATCH_PAD+8) =
			0x5a5a5a5a5a5a5a5a;
		flushbus();

		if ((*(volatile long *)PHYS_TO_K1(SCRATCH_PAD) !=
			0x0123456789abcdef) ||
		    (*(volatile long *)PHYS_TO_K1(SCRATCH_PAD+8) !=
			0x5a5a5a5a5a5a5a5a)) {
			cmn_err(CE_WARN,"ecc_gun: bad data on dword ucwrite."
				"test\n");
		}
		splx(s);
		return EBUSY;

	default:
		cmn_err(CE_CONT, "ecc_gun: bad cmd %d.\n", cmd);
		return EINVAL;
	}

	return 0;
}
#endif  /* DEBUG */

/*  Hook in main() that's call right before going to spl0 for the
 * first time.  This is a perfect time for us to jump from slow to
 * normal mode if we are not stuck in slow mode.
 */
void
allowintrs(void)
{
        if (!ip26_allow_ucmem)
                (void)ip28_disable_ucmem();
}

/*  Hook to reset all the GIO slots before we call into the PROM.  This
 * makes sure nothing is dependent on memory when the PROM clear it.
 */
void
gioreset(void)
{
	volatile uint *cfg = (uint *)PHYS_TO_COMPATK1(CPU_CONFIG);
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
}


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
}

#ifdef US_DELAY_DEBUG
/* rough check of us_delay.  Is not too accurate as done in C.  Caller needs
 * to convert from C0_COUNT ticks @ 1/2 freqency of CPU to determine time.
 */
__uint32_t us_before, us_after;
void
check_us_delay(void)
{
	int i;

	us_delay(1);		/* prime cache */

	for (i = 0; i < 10; i++) {
		us_delay(i);
		cmn_err(CE_CONT,
			"\\us_delay(%d) took a little less than 0x%x - 0x%x = "
			"%d ticks.\n",
			i,(__psunsigned_t)us_before,(__psunsigned_t)us_after,
			us_after-us_before);
	}

	return;
}
#endif /* US_DELAY_DEBUG */

#if CONSISTANCY_CHECK

#define	CACHELINE_ABITS		7
#define	CACHELINE_SIZE		(1<<CACHELINE_ABITS)
#define	CACHELINE_MASK		(CACHELINE_SIZE-1)

typedef union {
    unsigned char		b[CACHELINE_SIZE];
    unsigned long		l[CACHELINE_SIZE / sizeof (long)];
} cacheline_t;

/*
 * copy a cache line. don't use a loop
 * so T5 does not speculate off to an
 * adjoining line. scramble the order
 * to defeat any prefetchers.
 */
#define	copy_cacheline(__srca,__dsta)					\
{									\
    volatile cacheline_t *__src = (volatile cacheline_t *)(__srca);	\
    volatile cacheline_t *__dst = (volatile cacheline_t *)(__dsta);	\
    __dst->l[0xF] = __src->l[0xF];    __dst->l[0x0] = __src->l[0x0];	\
    __dst->l[0xE] = __src->l[0xE];    __dst->l[0x1] = __src->l[0x1];	\
    __dst->l[0xD] = __src->l[0xD];    __dst->l[0x2] = __src->l[0x2];	\
    __dst->l[0xC] = __src->l[0xC];    __dst->l[0x3] = __src->l[0x3];	\
    __dst->l[0xB] = __src->l[0xB];    __dst->l[0x4] = __src->l[0x4];	\
    __dst->l[0xA] = __src->l[0xA];    __dst->l[0x5] = __src->l[0x5];	\
    __dst->l[0x9] = __src->l[0x9];    __dst->l[0x6] = __src->l[0x6];	\
    __dst->l[0x8] = __src->l[0x8];    __dst->l[0x7] = __src->l[0x7];	\
}

extern unsigned long	get_scache_tag(unsigned long pa);
typedef union {
    unsigned long	ul;
    struct {
	unsigned	mru:1;		/* youngest way */
	unsigned	_z0:27;
	unsigned	stag1:4;	/* pa 39:36 */
	unsigned	stag0:18;	/* pa 35:18 */
	unsigned	_z1:2;
	unsigned	sstate:2;	/* inv,shd,cex,dex */
	unsigned	_z2:1;
	unsigned	vindex:2;	/* va 13:12 */
	unsigned	ecc:7;
    }			f;
} scachetag_t;

extern unsigned long	get_dcache_tag(unsigned long va);
typedef union {
    unsigned long	ul;
    struct {
	unsigned	refill:1;	/* being refilled */
	unsigned	dirty:1;	/* needs writeback */
	unsigned	clean:1;	/* neither of the above */
	unsigned	_z0:25;
	unsigned	dtag1:4;	/* pa 39:36 */
	unsigned	dtag0:24;	/* pa 35:12 */
	unsigned	dstate:2;	/* inv,shd,cex,dex or rcl/ugs/ugc/rfd */
	unsigned	_z1:2;
	unsigned	lru:1;		/* eldest way */
	unsigned	sp:1;		/* state parity */
	unsigned	way:1;		/* corr. way in scache */
	unsigned	tp:1;		/* tag parity */
    }			f;
} dcachetag_t;

int		ccelines;

void
consistancy_check(unsigned pa, unsigned ct, char *where, unsigned flags)
{
    volatile cacheline_t *k0;
    volatile cacheline_t *k1;
    scachetag_t		st0, st1;
    dcachetag_t		dt0, dt1;
    cacheline_t		cd, md;
    unsigned		stag1;
    unsigned		stag0;
    /*REFERENCED*/
    unsigned		dtag1;
    /*REFERENCED*/
    unsigned		dtag0;
    int			ix;
    unsigned		bpa = pa;
    int			notest;

    int			st0hit, st1hit, dt0hit, dt1hit;
    int			st0dirty, st1dirty, dt0dirty, dt1dirty;
    int			badline;
    unsigned		s;
    char		digits[17] = "0123456789ABCDEF";
    char		edigits[17] = ".123456789ABCDEF";
    char		omd[16][17];
    char		ocd[16][17];
    char		oed[16][17];

#if CCELINES_LIMIT
    if (ccelines > CCELINES_LIMIT) return;
#endif

    ct &= 0x0FFFFFFF;	/* mask off WD93 EOX, XIE */
    if (ct < 1) return;

    if (ccelines == 0) {
	cmn_err(CE_WARN,"consistancy_check (%s): ### active ###",where);
	ccelines ++;
    }

    notest = 0;

    if (pa & CACHELINE_MASK) {
	ct += pa & CACHELINE_MASK;
	pa -= pa & CACHELINE_MASK;
    }
    if (ct & CACHELINE_MASK) {
	ct += CACHELINE_MASK;
	ct -= ct & CACHELINE_MASK;
    }

    if (pa < SEG0_BASE) {	/* 512M */
	cmn_err(CE_WARN,"consistancy_check (%s):\n\tpa 0x%x below memory\n",where,pa);
	ccelines ++;
	notest = 1;
    }
    if (ct > 0x04000000) {	/* 64M */
	cmn_err(CE_WARN,"consistancy_check (%s):\n\tct 0x%x way too big\n",where,ct);
	ccelines ++;
	notest = 1;
    }
    if ((pa+ct) > SEG0_BASE + ctob(seg0_maxmem)) {	/* top of memory */
	cmn_err(CE_WARN,"consistancy_check (%s):\n\tpa 0x%x above memory\n",where,pa+ct);
	ccelines ++;
	notest = 1;
    }
    if (((pa&0x3ffff)+ct)>0x40000) {
	cmn_err(CE_WARN,"consistancy_check (%s):\n\tpa 0x%x ct 0x%x crosses cache edge",where,pa,ct);
	ccelines ++;
	notest = 1;
    }
#if CCELINES_LIMIT
    if (ccelines > CCELINES_LIMIT) {
	cmn_err(CE_CONT,"consistancy_check (%s): disabling\n",where);
	notest = 1;
    }
#endif
    if (notest)
	return;

    ct /= CACHELINE_SIZE;
    k0 = (cacheline_t *)PHYS_TO_K0(pa);
    k1 = (cacheline_t *)PHYS_TO_K1(pa);

    stag1 = 0;	/* (pa>>36)&0xf; */
    stag0 = (pa>>18)&0x0003FFFF;
    dtag1 = 0;	/* (pa>>36)&0xf; */
    dtag0 = (pa>>12)&0x00FFFFFF;

    while (ct-- > 0) {

	s = splhi();		/* START CRITICAL CODE SECTION */

	st0.ul = get_scache_tag((unsigned long)k0 +0);
	st1.ul = get_scache_tag((unsigned long)k0 +1);
	dt0.ul = get_dcache_tag((unsigned long)k0 +0);
	dt1.ul = get_dcache_tag((unsigned long)k0 +1);

	st0hit = st0.f.sstate && (st0.f.stag0 == stag0) && (st0.f.stag1 == stag1);
	st1hit = st1.f.sstate && (st1.f.stag0 == stag0) && (st1.f.stag1 == stag1);
	dt0hit = (dt0.f.refill || dt0.f.dstate) &&
	    (dt0.f.dtag0 == dtag0) && (dt0.f.dtag1 == dtag1);
	dt1hit = (dt1.f.refill || dt1.f.dstate) &&
	    (dt1.f.dtag0 == dtag0) && (dt1.f.dtag1 == dtag1);

	if (st0hit || st1hit || dt0hit || dt1hit) {
	    copy_cacheline(k0, &cd);
	    copy_cacheline(k1, &md);
	}

	splx(s);		/* END CRITICAL CODE SECTION */

	st0dirty = st0hit && (st0.f.sstate == 3);
	st1dirty = st1hit && (st1.f.sstate == 3);
	dt0dirty = dt0hit && (dt0.f.dirty || (dt0.f.dstate == 3));
	dt1dirty = dt1hit && (dt1.f.dirty || (dt1.f.dstate == 3));

	badline = 0;

	if ((dt0hit || dt1hit) && !(st0hit || st1hit)) {
	    cmn_err(CE_WARN,
		    "consistancy_check failure in %s (INCLUSION%s%s, not in SCACHE)",
		    where,
		    dt0hit?", DCache0":"",
		    dt1hit?", DCache1":"");
	    ccelines ++;
	    badline = 1;
	}

	if (st0hit && st1hit) {
	    cmn_err(CE_WARN,
		    "consistancy_check failure in %s (HITS BOTH WAYS OF SCACHE)",
		    where);
	    ccelines ++;
	    badline = 1;
	}

	if (dt0hit && dt1hit) {
	    cmn_err(CE_WARN,
		    "consistancy_check failure in %s (HITS BOTH WAYS OF DCACHE)",
		    where);
	    ccelines ++;
	    badline = 1;
	}

	if ((flags & CCF_REQUIRE_NODIRTY) && (st0dirty || st1dirty || dt0dirty || dt1dirty)) {
	    cmn_err(CE_WARN,
		    "consistancy_check failure in %s (CCF_REQUIRE_NODIRTY%s%s%s%s)",
		    where,
		    st0dirty?", SCache0":"",
		    st1dirty?", SCache1":"",
		    dt0dirty?", DCache0":"",
		    dt1dirty?", DCache1":"");
	    ccelines ++;
	    badline = 1;
	}

	if ((flags & CCF_REQUIRE_NOHIT) && (st0hit || st1hit || dt0hit || dt1hit)) {
	    cmn_err(CE_WARN,
		    "consistancy_check failure in %s (CCF_REQUIRE_NOHIT%s%s%s%s)",
		    where,
		    st0hit?", SCache0":"",
		    st1hit?", SCache1":"",
		    dt0hit?", DCache0":"",
		    dt1hit?", DCache1":"");
	    ccelines ++;
	    badline = 1;
	}

	if (st0hit || st1hit || dt0hit || dt1hit) {
	    for (ix = 0; ix < CACHELINE_SIZE; ++ix) {
		if (cd.b[ix] != md.b[ix]) {
		    cmn_err(CE_WARN,
			    "consistancy_check failure in %s (COMPARE PROBLEM)",
			    where);
		    for (ix=0; ix<CACHELINE_SIZE; ++ix) {
			omd[ix>>3][2*(ix&7)+0] = digits[md.b[ix]>>4];
			omd[ix>>3][2*(ix&7)+1] = digits[md.b[ix]&15];
			omd[ix>>3][2*(ix&7)+2] = 0;
		    }
		    cmn_err(CE_CONT, "mdata: %s %s %s %s\n", omd[0x0], omd[0x1], omd[0x2], omd[0x3]);
		    cmn_err(CE_CONT, "       %s %s %s %s\n", omd[0x4], omd[0x5], omd[0x6], omd[0x7]);
		    cmn_err(CE_CONT, "       %s %s %s %s\n", omd[0x8], omd[0x9], omd[0xA], omd[0xB]);
		    cmn_err(CE_CONT, "       %s %s %s %s\n", omd[0xC], omd[0xD], omd[0xE], omd[0xF]);
		    ccelines += 5;
		    badline = 1;
		    break;
		}
	    }
	}

	if (badline) {
	    if (st0hit || st1hit || dt0hit || dt1hit) {
		for (ix=0; ix<CACHELINE_SIZE; ++ix) {
		    ocd[ix>>3][2*(ix&7)+0] = digits[cd.b[ix]>>4];
		    if (ocd[ix>>3][2*(ix&7)+0] == omd[ix>>3][2*(ix&7)+0])
			ocd[ix>>3][2*(ix&7)+0] = '.';
		    ocd[ix>>3][2*(ix&7)+1] = digits[cd.b[ix]&15];
		    if (ocd[ix>>3][2*(ix&7)+1] == omd[ix>>3][2*(ix&7)+1])
			ocd[ix>>3][2*(ix&7)+1] = '.';
		    ocd[ix>>3][2*(ix&7)+2] = 0;

		    oed[ix>>3][2*(ix&7)+0] = edigits[(cd.b[ix]^md.b[ix])>>4];
		    oed[ix>>3][2*(ix&7)+1] = edigits[(cd.b[ix]^md.b[ix])&15];
		    oed[ix>>3][2*(ix&7)+2] = 0;
		}
		cmn_err(CE_CONT, "cdata: %s %s %s %s\n", ocd[0x0], ocd[0x1], ocd[0x2], ocd[0x3]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", ocd[0x4], ocd[0x5], ocd[0x6], ocd[0x7]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", ocd[0x8], ocd[0x9], ocd[0xA], ocd[0xB]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", ocd[0xC], ocd[0xD], ocd[0xE], ocd[0xF]);
		cmn_err(CE_CONT, "error: %s %s %s %s\n", oed[0x0], oed[0x1], oed[0x2], oed[0x3]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", oed[0x4], oed[0x5], oed[0x6], oed[0x7]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", oed[0x8], oed[0x9], oed[0xA], oed[0xB]);
		cmn_err(CE_CONT, "       %s %s %s %s\n", oed[0xC], oed[0xD], oed[0xE], oed[0xF]);
		ccelines += 8;
	    }
	    cmn_err(CE_CONT,
		    "\tbpa=%x, pa=%x k0=%x k1=%x\n",
		    (unsigned long)bpa, (unsigned long)pa, (unsigned long)k0, (unsigned long)k1);
	    ccelines += 1;
	    if (st0hit) {
		cmn_err(CE_CONT,
			"\taddress hits scache way 0, state %d [raw tag: %x]\n",
			st0.f.sstate, st0.ul);
		ccelines ++;
	    }
	    if (st1hit) {
		cmn_err(CE_CONT,
			"\taddress hits scache way 1, state %d [raw tag: %x]\n",
			st1.f.sstate, st1.ul);
		ccelines ++;
	    }
	    if (!st0hit && !st1hit) {
		cmn_err(CE_CONT,
			"\tlooking for stag1=%x and stag0=%x:\n",
			stag1, stag0);
		cmn_err(CE_CONT,
			"\taddress misses scache way 0, stag1=%x, stag0=%x [raw tag: %x]\n",
			st0.f.stag1, st0.f.stag0, st0.ul);
		cmn_err(CE_CONT,
			"\taddress misses scache way 1, stag1=%x, stag0=%x state %d [raw tag: %x]\n",
			st1.f.stag1, st1.f.stag0, st1.ul);
	    }
	    if (dt0hit) {
		cmn_err(CE_CONT,
			"\taddress hits dcache way 0, state %d%s%s [raw tag: %x]\n",
			dt0.f.dstate,
			dt0.f.refill ? ", sub=REFILL":"",
			dt0.f.dirty ? ", sub=DIRTY":"",
			dt0.ul);
		ccelines ++;
	    }
	    if (dt1hit) {
		cmn_err(CE_CONT,
			"\taddress hits dcache way 1, state %d%s%s [raw tag: %x]\n",
			dt1.f.dstate,
			dt1.f.refill ? ", sub=REFILL":"",
			dt1.f.dirty ? ", sub=DIRTY":"",
			dt1.ul);
		ccelines ++;
	    }

#if CCEFIXUP
	    __dcache_wb_inval(k0, CACHELINE_SIZE);
#endif

#if CCELINES_LIMIT
	    if (ccelines > CCELINES_LIMIT) {
		cmn_err(CE_CONT,"consistancy_check (%s): disabling.\n",where);
		break;
	    }
#endif
	}

	pa += CACHELINE_SIZE;
	k0 ++;
	k1 ++;
    }

}

#endif	/* CONSISTANCY_CHECK */

#if	CONSISTANCY_CHECK_WD93

static void
consistancy_check_wd93_list(unsigned *cbp, unsigned *bcnt, char *where, unsigned flags)
{
    while (*bcnt != HPC3_EOX_VALUE) {
	consistancy_check_wd93(*cbp, *bcnt, where, flags);
	bcnt += sizeof (scdescr_t) / sizeof (unsigned);
	cbp += sizeof (scdescr_t) / sizeof (unsigned);
    }
}

#endif	/* CONSISTANCY_CHECK */

#if CACHEFLUSH_DEBUG
/* if turning this on change chache-op names to have a trailing _ */
void __dcache_inval_(void *a, long l);
void __dcache_wb_(void *a, long l);
void __dcache_wb_inval_(void *a, long l);
void __icache_inval_(void *a, long l);
void __cache_wb_inval_(void *a, long l);

#define LOGSIZE 10000
struct {
	__psunsigned_t addr;
	char *str;
	long len;
} cachelog[LOGSIZE];

int cachelogi;

#define CACHELOG(a,l,string) \
{\
	int s = splhi();\
	cachelogi = (cachelogi + 1) % LOGSIZE;\
	cachelog[cachelogi].str = string;\
	cachelog[cachelogi].len = l;\
	cachelog[cachelogi].addr = (__psunsigned_t)a;\
	splx(s);\
}

void
docachelog(long a,int len,char *string)
{
	CACHELOG(a,len,string);
}

void
__dcache_inval(void *a, int l)
{
	CACHELOG(a,l,"__dcache_inval");
	__dcache_inval_(a,l);
}
void
__dcache_wb(void *a, long l)
{
	CACHELOG(a,l,"__dcache_wb");
	__dcache_wb_(a,l);
}
void
__dcache_wb_inval(void *a, long l)
{
	CACHELOG(a,l,"__dcache_wb_inval");
	__dcache_wb_inval_(a,l);
}
void
__icache_inval(void *a, long l)
{
	CACHELOG(a,l,"__icache_inval");
	__icache_inval_(a,l);
}
void
__cache_wb_inval(void *a, long l)
{
	CACHELOG(a,l,"__cache_wb_inval");
	__cache_wb_inval_(a,l);
}

int
interesting(int i, __psunsigned_t p)
{
	__psunsigned_t addr;
	int len;

	addr = cachelog[i].addr & 0x7ffff;		/* matching index */
	len = cachelog[i].len;
	if (addr % 0x80) {
		len += addr % 0x80;
		addr &= 0x7ffff80;
	}
	if (len % 0x80)
		len = len + 0x80 - (len%80);

	p &= 0x7ffff;

	return (p >= addr && p <= (addr+len));
}

char *
hitindex(int i, __psunsigned_t p)
{
	__psunsigned_t addr;
	int len;

	addr = cachelog[i].addr;
	len = cachelog[i].len;
	if (addr % 0x80) {
		len += addr % 0x80;
		addr &= ~0x7f;
	}
	if (len % 0x80)
		len = len + 0x80 - (len%80);

	return (p >= addr && p <= (addr+len)) ? "hit" : "index match";
}

static void
idbg_cachelog(__psint_t p)
{
	int s = 0;
	int e = cachelogi;
	int i;

	if (p == -1) p = 0;

	printf("p == 0x%x\n",p);

	if (p && ((uint)p < LOGSIZE)) {
		s = (int)p;
		e = s + 10;
		p = 0;
	}

	for (i=s; i != e; i = (i+1) % LOGSIZE) {
		if (p == 0 || interesting(i,(__psunsigned_t)p))
			printf("%d: %s(0x%x,0x%x) %s\n",i,
				cachelog[i].str ? cachelog[i].str : "none",
				cachelog[i].addr,cachelog[i].len,
				hitindex(i,(__psunsigned_t)p));
	}
	printf("total ops: %d [%s]\n",cachelogi,
		cachelogi / LOGSIZE ? "wrapped" : "unwrapped");
}
#endif /* CACHEFLUSH_DEBUG */

void
unmaptlb_range(caddr_t vaddr, size_t size)
{
	pgno_t vpn;
        pgcnt_t npages;

        vpn = btoct(vaddr);
        npages = numpages(vaddr,size);
	
        for (; npages > 0; vpn++, npages--)
                unmaptlb(curuthread->ut_as.utas_tlbpid, vpn);
}

int
is_thd(int level)
{
	switch(level) {

	/* Threaded */
	case VECTOR_SCSI:
	case VECTOR_SCSI1:
	case VECTOR_ENET:
	case VECTOR_PLP:
	case VECTOR_GIO1:
	case VECTOR_HPCDMA:
	case VECTOR_GIO2:
	case VECTOR_DUART:
	case VECTOR_VIDEO:
	case VECTOR_KBDMS:
	case VECTOR_GIOEXP0:
	case VECTOR_GIOEXP1:
	case VECTOR_EISA:
		return 1;
		/* NOTREACHED */
		break;

	/* Not threaded */
	case VECTOR_GIO0:
	case VECTOR_LCL2:
	case VECTOR_POWER:
	case VECTOR_LCL3:
	case VECTOR_ACFAIL:

	/* Untested as threads */
	case VECTOR_GDMA:
	case VECTOR_ISDN_ISAC:
	case VECTOR_ISDN_HSCX:

	/* ? */
	default:
		return 0;
		/* NOTREACHED */
		break;
	} /* switch level */
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
 * enable_ithreads -
 * scan the lclvec tables and setup any register interrupt threads
 * called out of main()
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

#ifdef R10000_SPECULATION_WAR
#include <sys/mman.h>

/*
** the following definitions and two routines are to support 
** the user dma war for when we're dma'ing directly into user buffers
*/

#define N_MPIN_COOKIES	5
struct cookie_entry {
	pid_t	procpid;		/* proc id */
	caddr_t	base;			/* virtual address */
	size_t	size;			/* value 0 is 'available' */
	void *	cookie;			/* mpin_lockdown cookie */
	int 	userdma_cookie;		/* overridden dma cookie */
};
struct mpin_cookie_list {
	struct mpin_cookie_list * next;
	struct cookie_entry cookies[N_MPIN_COOKIES];
};
static struct mpin_cookie_list mpin_cookies = {
	NULL,
	{ { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 } },
};

int
specwar_fast_userdma( caddr_t base, size_t count, int * userdma_cookie )
{
	struct mpin_cookie_list * cooklist;
	void * cookie;
	int handled;
	int opaque;
	int i;			/* find empty cookie entry */

	/* lock down the pages */
	cookie = mpin_lockdown( base, count );

	if (cookie == NULL) {
		return -1;
	}

	if (userdma_cookie != NULL) {
		struct cookie_entry * tmpcookie;

		/*
		** if they're using the "fast" userdma code, then (ab)use
		** the cookie the caller is going to hang onto.  we'll
		** squirel all the information away, and return it when
		** they call fast_undma()
		*/
		tmpcookie = kmem_alloc(sizeof(struct cookie_entry),
							KM_SLEEP|VM_DIRECT);
		tmpcookie->base = base;
		tmpcookie->size = count;
		tmpcookie->userdma_cookie = *userdma_cookie;
		tmpcookie->cookie = cookie;

		*userdma_cookie = (int)KDM_TO_PHYS(tmpcookie);	/* phys < 32b */

		return 0;
	}

	/* Keep track of things locally higher level code is not passing
	 * the cookie around.
	 */
	handled = 0;
	cooklist = &mpin_cookies;

	opaque = LOCK( &mpin_cookies_lock, plbase );

	/*
	 * the basic idea is to sequentially search all
	 * the existing arrays of tuples, and find one
	 * that isn't already used ... and use it.
	 *
	 * size == 0 is the flag that says that a
	 * tuple is not in use (what good would it be
	 * to lock down 0 bytes?)
	 */
	while (handled == 0) {
		for (i=0; i<N_MPIN_COOKIES; i++) {
			if (cooklist->cookies[i].size == 0) {
				handled = 1;
				cooklist->cookies[i].procpid = current_pid();
				cooklist->cookies[i].base = base;
				cooklist->cookies[i].size = count;
				cooklist->cookies[i].cookie = cookie;
				break;
			}
		}

		if (handled == 0) {
			/*
			 * we didn't find a free tuple yet, step
			 * to the next array in the linked list
			 */
			if (cooklist->next != NULL) {
				cooklist = cooklist->next;
			}
			else {
				/*
				 * there isn't a next array ... yet.
				 * so we need to allocate one.  don't
				 * hold the tuple lock over the call to 
				 * kmem_alloc() because we might sleep,
				 * and we don't want to stop everyone
				 * else's progress
				 */
				struct mpin_cookie_list * tmp;

				UNLOCK( &mpin_cookies_lock, opaque );

				tmp = kmem_alloc(
					sizeof(struct mpin_cookie_list),
					KM_SLEEP);
				ASSERT(tmp);

				opaque = LOCK( &mpin_cookies_lock, plbase );

				/*
				 * initialize and insert the new
				 * array into the linked list, and throw
				 * that tuple in there too
				 */
				if (cooklist->next == NULL) {
					cooklist->next = tmp;
					tmp->next = NULL;
					tmp->cookies[0].procpid = current_pid();
					tmp->cookies[0].base = base;
					tmp->cookies[0].size = count;
					tmp->cookies[0].cookie = cookie;
					handled = 1;
					for (i=1;i<N_MPIN_COOKIES;i++){
						tmp->cookies[i].procpid = 0;
						tmp->cookies[i].base = 0;
						tmp->cookies[i].size = 0;
						tmp->cookies[i].cookie = 0;
					}
				}
				else {
					/*
					 * totally bizarre case that I don't
					 * expect to happen, but since we
					 * released the lock, it might ...
					 *
					 * handle the case where someone else
					 * allocated a new array for us, and
					 * then back to the top and try and
					 * find an free tuple in it
					 */
					kmem_free(tmp,
					   sizeof(struct mpin_cookie_list));
					cooklist = cooklist->next;
				}
			}
		}
	}
	UNLOCK( &mpin_cookies_lock, opaque );

	return 0;
}

void
specwar_fast_undma( caddr_t base, size_t count, int * undma_cookie )
{
	int i;			/* step through arrays in list */
	void * cookie;
	int handled = 0;
	int opaque;		/* return from LOCK */
	struct mpin_cookie_list * cooklist = &mpin_cookies;
	struct cookie_entry * tmpcookie;

	/*
	 * see if we're using the fast routines, and that we squirreled
	 * away information before in the specwar_fast_userdma() routine
	 */
	if (undma_cookie != NULL) {
		tmpcookie = (struct cookie_entry *)PHYS_TO_K0(*undma_cookie);
		/*
		 * not much we can do if the caller didn't keep the cookie
		 * stuff valid for us ... or if they attempt to free multiple
		 * either way, fall through and check long route
		 */
		mpin_unlockdown( tmpcookie->cookie, 
				tmpcookie->base, tmpcookie->size );
		*undma_cookie = tmpcookie->userdma_cookie;
		kmem_free( tmpcookie, sizeof(struct cookie_entry) );
		return;
	}

	opaque = LOCK( &mpin_cookies_lock, plbase );

	/*
	 * step through the linked list of arrays of
	 * address/length/cookie tuples to find the one that
	 * we need to call mpin_unlockdown() with.
	 *
	 * if we don't find the tuple, just exit gracefully.
	 * we might have an address space permanently locked
	 * down though.  :(
	 */
	while ((handled == 0) && (cooklist != NULL)) {
		for (i=0; i<N_MPIN_COOKIES; i++) {
			if ((cooklist->cookies[i].procpid == current_pid()) &&
			    (cooklist->cookies[i].base == (caddr_t) base) && 
			    (cooklist->cookies[i].size == count)) {
				mpin_unlockdown( cooklist->cookies[i].cookie,
								base, count);

				cooklist->cookies[i].size = 0;
				handled = 1;
				break;
			}
		}

		if (handled == 0) {
			cooklist = cooklist->next;
		}
	}

	UNLOCK( &mpin_cookies_lock, opaque );

	return;
}

#endif /* R10000_SPECULATION_WAR */
