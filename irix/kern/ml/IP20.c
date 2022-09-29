/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * IP20 specific functions
 */
#ident "$Revision: 1.158 $"

#include <sys/types.h>
#include <ksys/xthread.h>
#include <sys/IP20.h>
#include <sys/IP20nvram.h>
#include <sys/buf.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/dp8573clk.h>
#include <sys/fpu.h>
#include <sys/i8254clock.h>
#include <sys/invent.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/kopt.h>
#include <sys/loaddrs.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/vproc.h>
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
#include <sys/vmereg.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/arcs/spb.h>	/* ARCS System Parameter Block */
#include <sys/arcs/debug_block.h>/* ARCS debug block */
#include <sys/arcs/kerncb.h>
#include <sys/uadmin.h>		/* For HPLC (Indigo Light power down */
#include <sys/hpcplpreg.h>
#include <sys/rtmon.h>
#include <sys/parity.h>
#include <string.h>
#include <sys/vdma.h>
#include <sys/par.h>
#include <sys/atomic_ops.h>
#include <ksys/cacheops.h>
#include <ksys/hwg.h>

int ithreads_enabled = 0;
static int is_thd(int);
static void lclvec_thread_setup(int, int, lclvec_t *);

extern caddr_t map_physaddr(paddr_t);
extern void unmap_physaddr(caddr_t);

uint_t vmeadapters = 0;
unsigned int mc_rev_level;

#define MAXCPU	1
#define RPSS_DIV		2
#define RPSS_DIV_CONST		(RPSS_DIV-1)

int	maxcpus = MAXCPU;

/* processor-dependent data area */
pdaindr_t	pdaindr[MAXCPU];

/* Some local prototypes */
static int _clock_func_read(char *ptr);
static void _clock_func_write(char *ptr);
static void setbadclock(void);
static void clearbadclock(void);
static int isbadclock(void);
int board_rev (void);		/* Not static just in case used elsewhere */
static void load_nvram_tab(void);
static void set_endianness(void);

/* Some prototypes which should probably be in a shared .h */
extern void heart_led_control(int); /* zduart.c just for IP20 */
extern int get_r4k_config(void); /* locore.s */
extern COMPONENT *arcs_findtype(COMPONENT *, CONFIGTYPE); /* arcs.c for IP20 */
extern COMPONENT *arcs_getchild(COMPONENT *); /* arcs.c for IP20 */

int dsp_rev_level;    /* set by get_dsp_hinv_info() */
void get_dsp_hinv_info(void);

/* Global flag to reset gfx boards attached to IP20 */
int GRReset = 0;

static unchar *scsilowmem;
static void *scmaps;
static scsi_npgs_dma = NSCSI_DMA_PGS;
extern int wd93cnt, wd93burstdma;
#define NMAPSPERCHAN	10	/* reasonable ? */

/* HPC1 can't DMA past 256 Mbytes, because it only has 28 bit descriptors;
 * also see contmemall()
 *
 * We bcopy from K0 to K0 and flush the cache on both because it is
 * much faster than K1 to K1 copies if we are doing the >128MB
 * workaround.  We may eventually make a special bcopy that does
 * the cache flush inline...  See scdma_map(), dmabp_map, and 
 * scdma_flush().  rev B MC helps about 1 Mbyte/sec for all 
 * cases where flushing is done.  K1-K1 is ~8.5 MB/s, K0-K1 w/flush
 * ~17.5, and 19 with K0-K0 w/flush (with rev B MC) (all using the
 * BCOPYTEST in tpsc.c w 4.0.5F).
*/
#define MAXDMAPAGE 0x10000

/*
 * Processor interrupt table
 */
extern	void buserror_intr(struct eframe_s *);
extern	void clock(struct eframe_s *, kthread_t *);
static	void lcl1_intr(struct eframe_s *);
static	void lcl0_intr(struct eframe_s *);
extern	void timein(void);
static	void r4kcount_intr(struct eframe_s *);
static	void updatespb(void);

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
	ackkgclock, SR_IMASK6|SR_IEC,	SR_IBIT6 >> 8,	0,  /* hardint 6 */
	(intvec_func_t)
	buserror_intr, SR_IMASK7|SR_IEC,SR_IBIT7 >> 8,	0,  /* hardint 7 */
	(intvec_func_t)
	r4kcount_intr, SR_IMASK8|SR_IEC,SR_IBIT8 >> 8,	0,  /* hardint 8 */
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
static	lcl_intr_func_t gio0_intr;
static	lcl_intr_func_t gio1_intr;
static	lcl_intr_func_t gio2_intr;
static	lcl_intr_func_t powerfail;

/* These arrays must be left in order, and contiguous to each other,
 * since we do some address calculations on them.
 */
lclvec_t lcl0vec_tbl[] = {
	{ 0,		0 } ,		/* table is 1 based*/
	{ lcl_stray,	1, 0x01, },
	{ lcl_stray,	2, 0x02, },	/* really the parallel port */
	{ lcl_stray,	3, 0x04, },
	{ lcl_stray,	4, 0x08, },
	{ lcl_stray,	5, 0x10, },
	{ lcl_stray,	6, 0x20, },
	{ lcl_stray,	7, 0x40, },
	{ lcl_stray,	8, 0x80, },
	};


lclvec_t lcl1vec_tbl[] = {
	{ 0,		0 },
	{ lcl_stray,	1, 0x01, },
	{ lcl_stray,	2, 0x02, },	/* GR1 mode status */
	{ lcl_stray,	3, 0x04, },
	{ lcl_stray,	4, 0x08, },
	{ lcl_stray,	5, 0x10, },
	{ lcl_stray,	6, 0x20, },	/* AC fail */
	{ lcl_stray,	7, 0x40, },	/* Video interrupt */
	{ lcl_stray,	8, 0x80, },
	};

#define GIOLEVELS	3 /* GIO 0 (FIFO), GIO 1 (GE), GIO 2 (VR) */
#define GIOSLOTS	3 /* Graphics = 0, GIO 1 & 2 */

struct giovec_s {
	lcl_intr_func_t *isr;
	__psint_t arg;
};

static struct giovec_s giovec_tbl[GIOSLOTS][GIOLEVELS];

scuzzy_t scuzzy[1] = {
	{
		(u_char *)PHYS_TO_K1(SCSI0A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI0D_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_CTRL_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_BC_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_CBP_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_NBDP_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_PNTR_ADDR),
		(ulong *)PHYS_TO_K1(SCSI0_FIFO_ADDR),
		D_PROMSYNC|D_MAPRETAIN,
		0x80
	}
};

/*	used to sanity check adap values passed in, etc */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))

/* table of probeable kmem addresses */
struct kmem_ioaddr kmem_ioaddr[] = {
        { PHYS_TO_K1(LIO_ADDR), (LIO_GFX_SIZE + LIO_GIO_SIZE) },
	{ 0, 0 },
};

short	cputype = 20;	/* integer value for cpu (i.e. xx in IPxx) */

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

u_int gio64_arb_reg = GIO64_ARB_1_GIO;

static uint memconfig_0, memconfig_1;

static int has_vmax = 0;		/* 1 for V50 */

/* Bss allocated for parallel port driver.  This is an ugly hack to
 * insure that the driver's memory descriptors and receive buffers
 * are below the 128 MB limit on DMA.  low_plp_desc gets aligned on a
 * cache line, low_plp_rbuf gets aligned on a page.
 */
static char lpdm[((NMD + 1) * sizeof(memd_t)) + 256 + 4];
static char lprm[(NBPP*(NRP+1)) + 4];
char *low_plp_desc = lpdm, *low_plp_rbuf = lprm;

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
static void
init_sysid(void)
{
	char *cp; 

	cp = (char *)arcs_getenv("eaddr");
	bcopy (etoh(cp), eaddr, 6);

	sys_id = (eaddr[2] << 24) | (eaddr[3] << 16) |
		 (eaddr[4] << 8) | eaddr[5];
	sprintf (arg_eaddr, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
	    (eaddr[0]>>4)&0xf, eaddr[0]&0xf, (eaddr[1]>>4)&0xf, eaddr[1]&0xf,
	    (eaddr[2]>>4)&0xf, eaddr[2]&0xf, (eaddr[3]>>4)&0xf, eaddr[3]&0xf,
	    (eaddr[4]>>4)&0xf, eaddr[4]&0xf, (eaddr[5]>>4)&0xf, eaddr[5]&0xf);
}

/*
 *	pass back the serial number associated with the system
 *  ID prom. always returns zero.
 */
int
getsysid(char *hostident)
{
	/*
	 * serial number is only 4 bytes long on IP20.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */

	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}


/* 
 *	get memory configuration word for a bank
 *              - bank better be between 0 and 3 inclusive
 */
static uint
get_mem_conf(int bank)
{
    uint memconfig;

    memconfig = (bank <= 1) ? 
            *(volatile uint *)PHYS_TO_K1(MEMCFG0) :
                *(volatile uint *)PHYS_TO_K1(MEMCFG1) ;
    memconfig >>= 16 * (~bank & 1);     /* shift banks 0, 2 */
    return memconfig & 0xffff;
}       /* get_mem_conf */

/*
 * bank_addrlo - determine first address of a memory bank
 */
static int
bank_addrlo(int bank)
{
    return (int)((get_mem_conf(bank) & MEMCFG_ADDR_MASK) << 22);
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
        return (int)(((memconfig & MEMCFG_DRAM_MASK) + 0x0100) << 14);
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

    for (bank = 0; bank < 4; ++bank) {
        if (!(size = banksz(bank))) /* bad memory module if size == 0 */
            continue;

        if (addr >= bank_addrlo(bank) && addr < bank_addrlo(bank) + size)
            return (bank);
    }
    return (-1);        /* address not found */
}   /* addr_to_bank */

static pgno_t memsize = 0;	/* total ram in clicks */
static pgno_t seg0_maxmem;
static pgno_t seg1_maxmem; 

/*
 * szmem - size main memory
 * Returns # physical pages of memory
 */
/* ARGSUSED */
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
    int bank;

    if (!memsize) {
	for (bank = 0; bank < 4; ++bank) 
	    memsize += banksz(bank);		/* sum up bank sizes */
	memsize = btoct(memsize);			/* get pages */

	if (maxpmem)
	    memsize = MIN(memsize, maxpmem);

	seg0_maxmem = memsize;
	seg1_maxmem = 0;

	if (memsize > btoc(SEG0_SIZE)) {
	    seg0_maxmem = btoc(SEG0_SIZE);
	    seg1_maxmem = memsize - btoc(SEG0_SIZE);
	}
    }

    return memsize;
}

#ifdef _MEM_PARITY_WAR
int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return((pfn >= min_pfn &&
		pfn < (btoct(SEG0_BASE) + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
	        pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}

int
is_in_main_memory(pgno_t pfn)
{
	return((pfn >= btoct(SEG0_BASE) &&
		pfn < (btoct(SEG0_BASE) + seg0_maxmem)) ||
	       (pfn >= btoct(SEG1_BASE) &&
		pfn < (btoct(SEG1_BASE) + seg1_maxmem)));
}
#endif /* _MEM_PARITY_WAR */


#ifdef DEBUG
int mapflushcoll;	/* K2 address collision counter */
int mapflushdepth;	/* K2 address collision maximum depth */
#endif

static void
mapflush(void *addr, unsigned int len, unsigned int pfn, unsigned int flags)
{
    void *kvaddr;
    uint lastpgi = 0;
    int s;
    static int *pginuse;

    if (!private.p_cacheop_pages) {
	private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
	pginuse = (int *)kmem_zalloc(sizeof(int) * (cachecolormask+1), 0);
	ASSERT(private.p_cacheop_pages != NULL && pginuse != NULL);
    }

    kvaddr = (void *)((uint)private.p_cacheop_pages +
		  (NBPP * colorof(addr)) + poff(addr));

    s = splhi();
    if (pginuse[colorof(addr)]) {
#ifdef DEBUG
	mapflushcoll++;
	if (pginuse[colorof(addr)] > mapflushdepth)
	    mapflushdepth = pginuse[colorof(addr)];
#endif
	/* save last mapping */
	lastpgi = kvtokptbl(kvaddr)->pgi;
	ASSERT(lastpgi && lastpgi != mkpde(PG_G, 0));
	unmaptlb(0, btoct(kvaddr));
    }
    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
    pginuse[colorof(addr)]++;

    if (flags & CACH_ICACHE) {
	__icache_inval(kvaddr, len);
    } else {
	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK))
		__dcache_wb_inval(kvaddr, len);
	else if (flags & CACH_INVAL)
		__dcache_inval(kvaddr, len);
	else if (flags & CACH_WBACK)
		__dcache_wb(kvaddr, len);
    }

    unmaptlb(0, btoct(kvaddr));
    if (lastpgi) {
	/* restore last mapping */
	ASSERT(lastpgi != mkpde(PG_G, 0));
	pg_setpgi(kvtokptbl(kvaddr), lastpgi);
	tlbdropin(0, kvaddr, kvtokptbl(kvaddr)->pte);
    } else
	pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0));
    pginuse[colorof(addr)]--;

    splx(s);
}

#define	RMI_CACHEMASK	0xfffff		/* mask for 1 meg cache */
#define	RMI_PFN(x)	((x) & btoct(RMI_CACHEMASK))

/*
 * On the IP20, the secondary cache is a unified i/d cache. The
 * r4000 caches must maintain the property that the primary caches
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

	/* old MC's use old rmi workaround */
	if (mc_rev_level == 0) {
	    if (!IS_KSEG0(addr)) {
		ASSERT(pfn);
		addr = (void *)(PHYS_TO_K0(ctob(RMI_PFN(pfn))) | poff(addr));
	    }
	    ASSERT(IS_KSEG0(addr));
	    __cache_wb_inval(addr, len);
	    return;
	}

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((int)addr+len-1)));

	/*
	 * If it is a K2 address, then use it without remapping.
	 * Remap only if CACH_NOADDR is set and addr was not a K0 address
	 * (i.e. only the pfn was passed in).
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		if (pfn >= btoct(SEG1_BASE)) {
		    mapflush(addr, len, pfn, flags & ~CACH_DCACHE);
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
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

	/* old MC's use old rmi workaround */
	if (mc_rev_level == 0) {
	    if (!IS_KSEG0(addr)) {
		ASSERT(pfn);
		addr = (void *)(PHYS_TO_K0(ctob(RMI_PFN(pfn))) | poff(addr));
	    }
	    ASSERT(IS_KSEG0(addr));
	    __cache_wb_inval(addr, len);
	    return;
	}

	/* Note that this routine is called with either a K0 address and
	 * an arbitrary length OR higher level routines will break request
	 * into (at most) page size requests with non-K0 addresses.
	 */
	if ((flags & CACH_NOADDR) && IS_KSEG0(addr)) {
		__cache_wb_inval(addr, len);
		return;
	}
	ASSERT(IS_KSEG0(addr) || (btoct(addr)==btoct((int)addr+len-1)));

	/*
	 * If it is a K2 address, then use it without remapping.
	 * Remap only if CACH_NOADDR is set and addr was not a K0 address
	 * (i.e. only the pfn was passed in).
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
		if (pfn >= btoct(SEG1_BASE)) {
		    mapflush(addr, len, pfn, flags & ~CACH_ICACHE);
		    return;
		} else
		    kvaddr = (char *)(PHYS_TO_K0(ctob(pfn)) | poff(addr));
	} else
		kvaddr = addr;

	if ((flags & (CACH_INVAL|CACH_WBACK)) == (CACH_INVAL|CACH_WBACK)) {
		__dcache_wb_inval(kvaddr, len);
	} else if (flags & CACH_INVAL) {
		__dcache_inval(kvaddr, len);
	} else if (flags & CACH_WBACK) {
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
 *		  This isn't used on IP20.
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
node_getmaxclick(cnodeid_t node)
{
	ASSERT(node == 0);
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);

	if (memsize > btoc(SEG0_SIZE))
		return btoc(SEG1_BASE) + (memsize - btoc(SEG0_SIZE)) - 1;

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
	register pgno_t	hole_size;
	register pgno_t npgs;

	if (memsize <= btoc(SEG0_SIZE))
		return 0;

	pfd += btoc(SEG0_BASE + SEG0_SIZE) - first;
	hole_size = btoc(SEG1_BASE - (SEG0_BASE + SEG0_SIZE)); 

	for (npgs = hole_size; npgs; pfd++, npgs--)
		PG_MARKHOLE(pfd);

	return hole_size;
}

void
lclvec_init(void)
{
	int i;
	for (i=0; i<8; i++) {
		lcl0vec_tbl[i+1].lcl_flags=is_thd( 0+i) ? THD_ISTHREAD|0 : 0;
		lcl1vec_tbl[i+1].lcl_flags=is_thd( 8+i) ? THD_ISTHREAD|1 : 1;
	}
}

/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup
 * also, at this point, the DSP hasn't yet been turned on, so the
 * PROM can be directly accessed.
 */
/* ARGSUSED */
void
mlreset(int junk)
{
	extern uint cachecolormask;
	extern int reboot_on_panic;
	extern int cachewrback;
	unsigned int cpuctrl1tmp;
	volatile uint *cpuctrl0 = (volatile uint *)PHYS_TO_K1(CPUCTRL0);

	updatespb();			/* convert older spb to newer */

	/*
	** just to be safe, clear the parity error register
	** This is done because if any errors are left in the register,
	** when a DBE occurs we will think it is due to a parity error.
	*/
	*(volatile uint *)PHYS_TO_K1( CPU_ERR_STAT ) = 0;
	*(volatile uint *)PHYS_TO_K1( GIO_ERR_STAT ) = 0;
	flushbus();

	/*
	 * Set RPSS divider for increment by 2, the fastest rate at which
	 * the register works  (also see comment for query_cyclecntr).
	 */
	*(volatile uint *)PHYS_TO_K1( RPSS_DIVIDER ) = (1 << 8)|RPSS_DIV_CONST;

	/*
	** disable all parity checking due to a DMUX bug, bad parity is
	** written into memory during vdma.  enable all parity checking if we
	** have the new DMUXs
	*/
	disable_sysad_parity();
	if (board_rev() >= 0x2) {
		*cpuctrl0 |=
			CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR;
	} else {
		*cpuctrl0 &=
			~(CPUCTRL0_GPR|CPUCTRL0_MPR|CPUCTRL0_CPR|CPUCTRL0_CMD_PAR);
#ifdef _MEM_PARITY_WAR
		no_parity_workaround = 1;   /* Never re-enable SysAD parity */
#endif /* _MEM_PARITY_WAR */
	}
	flushbus();

	/*
	 * version 0 of the MC must use rmi_cacheflush() to flush cache, cannot
	 * change the values of LB_TIME and CPU_TIME in the interrupt handler,
	 * ...
	 */
	mc_rev_level = *(volatile uint *)PHYS_TO_K1(SYSID) &
		SYSID_CHIP_REV_MASK;

	/*
	 * set the MC write buffer depth
	 * the effective depth is roughly 17 minus this value (i.e. 6)
	 * NOTE: don't make the write buffer deeper unless you
         * understand what this does to GFXDELAY
	 */
	cpuctrl1tmp = *((volatile unsigned int *)PHYS_TO_K1(CPUCTRL1)) & 0xFFFFFFF0;
	*((volatile unsigned int *)PHYS_TO_K1(CPUCTRL1)) = (cpuctrl1tmp | 0xD);
	flushbus();

	*(volatile u_int *)PHYS_TO_K1(GIO64_ARB) = gio64_arb_reg;
	flushbus();

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
	    kdebug = 0;
	
	/* set initial interrupt vectors */
	*K1_LIO_0_MASK_ADDR = 0;
	*K1_LIO_1_MASK_ADDR = 0;

	lclvec_init();

	setlclvector(VECTOR_ACFAIL, powerfail, 0);

	/* set interrupt DMA burst and delay values */
	i_dma_burst = dma_delay;
	i_dma_delay = dma_burst;

	init_sysid();
	load_nvram_tab();	/* get a copy of the nvram */
	get_dsp_hinv_info();	/* get the prom's hinv info on the dsp */

	cachewrback = 1;
	/*
	 * cachecolormask is set from the bits in a vaddr/paddr
	 * which the r4k looks at to determine if a VCE should
	 * be raised.
	 * Despite what some documents may say, these are
	 * bits 12..14
	 * These are shifted down to give a value between 0..7
	 */
	cachecolormask = R4K_MAXPCACHESIZE/NBPP - 1;

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

	set_endianness();
	VdmaInit();

	memconfig_0 = *(volatile uint *)PHYS_TO_K1(MEMCFG0);
	memconfig_1 = *(volatile uint *)PHYS_TO_K1(MEMCFG1);

	/* Start V50 Initialization */

	/* turn off VME mask, turn on one by one when needed for V50 */
	*K1_VME_0_MASK_ADDR = 0;
	*K1_VME_1_MASK_ADDR = 0;

	/* V50 has no audio */
	if (dsp_rev_level == -1)
	    has_vmax = 1;

	/* End V50 Initialization */
}

static void
set_endianness(void)
{
    int little_endian = 0;
    volatile unsigned int testaddr;
    char *cp = (char *)&testaddr;

    testaddr = 0x11223344;
    if (*cp == 0x44)
	little_endian = 1;

    if (little_endian)
	*(volatile char *)PHYS_TO_K1(HPC_ENDIAN) |= HPC_ALL_LITTLE;
    else
	*(volatile char *)PHYS_TO_K1(HPC_ENDIAN) &= ~HPC_ALL_LITTLE;
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


void
setgiovector(int level, int slot, void (*func)(__psint_t, struct eframe_s *), 
		__psint_t arg)
{
	long s;

	if ( level < 0 || level >= GIOLEVELS )
		cmn_err(CE_PANIC,"GIO vector number out of range (%u)",level);
	
	if ( slot < 0 || slot >= GIOSLOTS )
		cmn_err(CE_PANIC,"GIO slot number out of range (%u)",slot);

	s = splgio2();

	giovec_tbl[slot][level].isr = func;
	giovec_tbl[slot][level].arg = arg;

	if (giovec_tbl[GIO_SLOT_0][level].isr ||
	    giovec_tbl[GIO_SLOT_1][level].isr ||
	    giovec_tbl[GIO_SLOT_GFX][level].isr)
		switch(level) {
			case GIO_INTERRUPT_0:
				setlclvector(VECTOR_GIO0,gio0_intr,0);
				break;
			case GIO_INTERRUPT_1:
				setlclvector(VECTOR_GIO1,gio1_intr,0);
				break;
			case GIO_INTERRUPT_2:
				setlclvector(VECTOR_GIO2,gio2_intr,0);
				break;
		} else
		switch(level) {
			case GIO_INTERRUPT_0:
				setlclvector(VECTOR_GIO0,0,0);
				break;
			case GIO_INTERRUPT_1:
				setlclvector(VECTOR_GIO1,0,0);
				break;
			case GIO_INTERRUPT_2:
				setlclvector(VECTOR_GIO2,0,0);
				break;
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
	u_int mask;

	mask = (GIO64_ARB_EXP0_SIZE_64 | GIO64_ARB_EXP0_RT |
		GIO64_ARB_EXP0_MST | GIO64_ARB_EXP0_PIPED);

	if (slot == GIO_SLOT_1) {
		mask <<= 1;
		flags <<= 1;
	}

	gio64_arb_reg = (gio64_arb_reg & ~mask) | (flags & mask);
	writemcreg(GIO64_ARB, gio64_arb_reg);
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

#ifdef DEBUG
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
intr(struct eframe_s *ep, uint code, uint sr, uint  cause)
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
		} while ( cause );
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

/*
 * lcl?intr - handle local interrupts 0 and 1
 */
static void
lcl0_intr(struct eframe_s *ep)
{
	register lclvec_t *liv;
	char lcl_cause;
	int svcd = 0;

	lcl_cause = *K1_LIO_0_ISR_ADDR & *K1_LIO_0_MASK_ADDR;

	if (lcl_cause) {
		do {
			liv = lcl0vec_tbl+FFINTR(lcl_cause);
			COUNT_L0_INTR;
			LCL_CALL_ISR(liv, ep, K1_LIO_0_MASK_ADDR, threaded);
			svcd++;
			lcl_cause &= ~liv->bit;
		} while (lcl_cause);
	} else {
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
		LCL_CALL_ISR(liv, ep, K1_LIO_1_MASK_ADDR,threaded);
		svcd++;
		lcl_cause &= ~liv->bit;
	}

	/* account for extra one counted in intr */
	atomicAddInt((int *) &SYSINFO.intr_svcd, svcd-1);
}

/*ARGSUSED*/
static void
gio0_intr(int arg, struct eframe_s *ep)
{
	if (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr) {
		((lcl_intr_func_t *)giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_0].arg,ep);
		if (!*K1_LIO_0_ISR_ADDR & LIO_GIO_0)
			return;
	}

	if (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].isr) {
		((lcl_intr_func_t *)giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_0].arg,ep);
		if (!*K1_LIO_0_ISR_ADDR & LIO_GIO_0)
			return;
	}

	if (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr)
		((lcl_intr_func_t *)giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_0].arg,ep);

}

/*ARGSUSED*/
static void
gio1_intr(int arg, struct eframe_s *ep)
{
	if (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_1].arg,ep);
		if (!*K1_LIO_0_ISR_ADDR & LIO_GIO_1)
			return;
	}

	if (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].isr) {
		(*giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_1].arg,ep);
		if (!*K1_LIO_0_ISR_ADDR & LIO_GIO_1)
			return;
	}

	if (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr)
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_1].arg,ep);
}

/*ARGSUSED*/
static void
gio2_intr(int arg, struct eframe_s *ep)
{
	if (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr) {
		(*giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_0][GIO_INTERRUPT_2].arg,ep);
		if (!*K1_LIO_1_ISR_ADDR & LIO_GIO_2)
			return;
	}

	if (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].isr) {
		(*giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_1][GIO_INTERRUPT_2].arg,ep);
		if (!*K1_LIO_1_ISR_ADDR & LIO_GIO_2)
			return;
	}

	if (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr)
		(*giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].isr)
		    (giovec_tbl[GIO_SLOT_GFX][GIO_INTERRUPT_2].arg,ep);
}

/*ARGSUSED*/
static void
r4kcount_intr(struct eframe_s *ep)
{
	clr_r4kcount_intr();
}

static printstray0;


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
powerfail(int arg, struct eframe_s *ep)
{
	int adap;

	/* maybe should spin a bit before complaining?  Sometimes
	 * see this when people power off.  May want to use this
	 * for 'clean' shutdowns if we ever get enough power to
	 * write out part of the superblocks, or whatever.  For
	 * now, reset SCSI bus to attempt to ensure we don't wind
	 * up with a media error due to a block header write being
	 * in progress when the power actually fails. */
	for(adap=0; adap<wd93cnt; adap++) {
		if (scuzzy[adap].d_ctrl)
			wd93_resetbus(adap);
	}
	cmn_err_tag(16,CE_WARN,"Power failure detected\n");
}

/*
** turn on the VME intr mask for bit 'iolv'
** cpunum is always 0
** vmeadap is the VME bus adapter(for machine with multi-VME bus)
*/
/*ARGSUSED*/
int
iointr_at_cpu(int iolv, int cpunum, int vmeadap)
{
	ASSERT(iolv != 0);
	/* Route all VME interrupts through local 0 for now. */
	*K1_VME_0_MASK_ADDR |= (1<<iolv);
	return(0);
}

/*
 * log_perr - log the bank, byte, and SIMM of a detected parity error
 *	addr is the contents of the cpu or dma parity error address register
 *	bytes is the contents of the parity error register
 *
 */
#define	BYTE_PARITY_MASK	0xff
static char bad_simm_string[16];

void
log_perr(uint addr, uint bytes, int no_console, int print_help)
{
    int bank = addr_to_bank(addr);
    int print_prio = print_help ? CE_ALERT : CE_CONT;
    static char *simm[3][4] = {
	" S1C", " S3C", " S2C", " S4C",
	" S1B", " S3B", " S2B", " S4B",
	" S1A", " S3A", " S2A", " S4A",
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
	cmn_err_tag(17, print_prio, "!Parity Error in SIMM %s\n",
		bad_simm_string);
    else
	cmn_err_tag(18, print_prio, "Parity Error in SIMM %s\n",
		bad_simm_string);

    return;
}

extern void fix_bad_parity(volatile uint *);
extern char *dma_par_msg;

/*
 * dobuserre - Common IP20 bus error handling code
 *
 * The MC sends an interrupt whenever bus or parity errors occur.  
 * In addition, if the error happened during a CPU read, it also
 * asserts the bus error pin on the R4K.  Code in VEC_ibe and VEC_dbe
 * save the MC bus error registers and then clear the interrupt
 * when this happens.  If the error happens on a write, then the
 * bus error interrupt handler saves the info and comes here.
 *
 * flag:	0: kernel; 1: kernel - no fault; 2: user
 */

#define	ERR_MASK	0x3f00

uint cpu_err_stat, gio_err_stat;
uint cpu_par_adr, gio_par_adr;
static uint bad_data_hi, bad_data_lo;

static int dobuserr_common(struct eframe_s *, inst_t *, uint, uint);

int
dobuserre(struct eframe_s *ep, inst_t *epc, uint flag)
{
	if (flag == 1)
		return(0); /* nofault */

	return(dobuserr_common(ep,epc,flag,/* is_exception = */ 1));
}

static int
dobuserr_common(struct eframe_s *ep,
		inst_t *epc,
		uint flag,
		uint is_exception)
{
	int cpu_buserr = 0;
	int cpu_memerr = 0;
	int gio_buserr = 0;
	int gio_memerr = 0;
	int fatal_error = 0;
	caddr_t mapped_bad_addr = NULL;
	int sysad_was_enabled;

#ifdef _MEM_PARITY_WAR
	/* Initiate recovery if it is a memory parity error.
	 */
	if ((cpu_err_stat & CPU_ERR_STAT_RD_PAR) == CPU_ERR_STAT_RD_PAR) {
	    if (!no_parity_workaround) {
	    	if (eccf_addr == NULL)
		    if (! perr_save_info(ep,NULL,CACHERR_EE,(k_machreg_t) epc,
				 (is_exception
				   ? (((ep->ef_cause & CAUSE_EXCMASK) ==
				    	EXC_IBE) 
				        ? PERC_IBE_EXCEPTION
				   	: PERC_DBE_EXCEPTION)
				   : PERC_BE_INTERRUPT)))
			cmn_err_tag(19,CE_PANIC,"Fatal parity error\n");
			
	    	ASSERT(eccf_addr);		/* perr_save_info left this */

	    	if (ecc_exception_recovery(ep))
		    	return 1;
	    } else if (dwong_patch(ep))
		    return 1;	/* survived it */
	}
#endif /* _MEM_PARITY_WAR */

	/*
	 * we print all gruesome info for 
	 *   1. debugging kernel
	 *   2. bus errors
	 */
	if (kdebug || (cpu_err_stat & ERR_MASK))
		cmn_err(CE_CONT, "CPU Error/Addr 0x%x<%s%s%s%s%s%s>: 0x%x\n",
			cpu_err_stat,
			cpu_err_stat & CPU_ERR_STAT_RD ? "RD " : "",
			cpu_err_stat & CPU_ERR_STAT_PAR ? "PAR " : "",
			cpu_err_stat & CPU_ERR_STAT_ADDR ? "ADDR " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSAD_PAR ? "SYSAD " : "",
			cpu_err_stat & CPU_ERR_STAT_SYSCMD_PAR ? "SYSCMD " : "",
			cpu_err_stat & CPU_ERR_STAT_BAD_DATA ? "BAD_DATA " : "",
			cpu_par_adr);
	if (kdebug || (gio_err_stat & ERR_MASK))
		cmn_err(CE_CONT, "GIO Error/Addr 0x%x:<%s%s%s%s%s%s> 0x%x\n",
			gio_err_stat,
			gio_err_stat & GIO_ERR_STAT_RD ? "RD " : "",
			gio_err_stat & GIO_ERR_STAT_WR ? "WR " : "",
			gio_err_stat & GIO_ERR_STAT_TIME ? "TIME " : "",
			gio_err_stat & GIO_ERR_STAT_PROM ? "PROM " : "",
			gio_err_stat & GIO_ERR_STAT_ADDR ? "ADDR " : "",
			gio_err_stat & GIO_ERR_STAT_BC ? "BC " : "",
			gio_par_adr);

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
		cpu_memerr = 1;
	else if (cpu_err_stat & ERR_MASK)
		fatal_error = 1;

	if (gio_err_stat & (GIO_ERR_STAT_TIME | GIO_ERR_STAT_PROM))
		gio_buserr = 1;
	else if ((gio_err_stat & GIO_ERR_STAT_RD) == GIO_ERR_STAT_RD)
		gio_memerr = 1;
	else if (gio_err_stat & ERR_MASK)
		fatal_error = 1;

	/* print bank, byte, and SIMM number if it's a parity error
	 */
	if (cpu_memerr) {
		log_perr(cpu_par_adr, cpu_err_stat, 0, 1);

		mapped_bad_addr = map_physaddr((paddr_t) (cpu_par_adr & ~0x7));
		sysad_was_enabled = is_sysad_parity_enabled();
		disable_sysad_parity();
		bad_data_hi = *(volatile uint *)mapped_bad_addr;
		bad_data_lo = *(volatile uint *)(mapped_bad_addr + 4);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
		flushbus();

		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err_tag(20, CE_ALERT,
"Process %d [%s] sent SIGBUS due to Memory Error in SIMM %s\n\tat Physical Address 0x%x, Data: 0x%x/0x%x\n",
				current_pid(), get_current_name(),
				bad_simm_string, 
				cpu_par_adr, bad_data_hi, bad_data_lo);
			fix_bad_parity((volatile uint *) mapped_bad_addr);
		} else
			cmn_err_tag(21, CE_PANIC,
"IRIX Killed due to Memory Error in SIMM %s\n\tat Physical Address 0x%x PC:0x%x ep:0x%x\n\tMemory Configuration registers: 0x%x/0x%x, Data: 0x%x/0x%x\n",
				bad_simm_string, cpu_par_adr, epc, ep, 
				memconfig_0, memconfig_1,
				bad_data_hi, bad_data_lo);
		if (sysad_was_enabled)
			enable_sysad_parity();
		unmap_physaddr(mapped_bad_addr);
	}

	if (gio_memerr) {
		log_perr(gio_par_adr, gio_err_stat, 0, 1);

		mapped_bad_addr = map_physaddr((paddr_t) (gio_par_adr & ~0x7));
		sysad_was_enabled = is_sysad_parity_enabled();
		disable_sysad_parity();
		bad_data_hi = *(volatile uint *)mapped_bad_addr;
		bad_data_lo = *(volatile uint *)(mapped_bad_addr + 4);
		*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
		flushbus();

		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Memory Error in SIMM %s\n\tat Physical Address 0x%x, Data: 0x%x/0x%x\n",
				current_pid(), get_current_name(),
				bad_simm_string, 
				gio_par_adr, bad_data_hi, bad_data_lo);
			fix_bad_parity((volatile uint *) mapped_bad_addr);
		} else
			cmn_err_tag(22, CE_PANIC,
"IRIX Killed due to Memory Error in SIMM %s\n\tat Physical Address 0x%x PC:0x%x ep:0x%x\n\tMemory Configuration registers: 0x%x/0x%x, Data: 0x%x/0x%x\n",
				bad_simm_string, gio_par_adr, epc, ep, 
				memconfig_0, memconfig_1,
				bad_data_hi, bad_data_lo);
		if (sysad_was_enabled)
			enable_sysad_parity();
		unmap_physaddr(mapped_bad_addr);
	}

	if (cpu_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
		else
			cmn_err_tag(23, CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

	if (gio_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
		else
			cmn_err_tag(24, CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

	if (fatal_error)
		cmn_err(CE_PANIC,
"IRIX Killed due to internal Error\n\tat PC:0x%x ep:0x%x\n", 
			epc, ep);

	return(0);
}

/* Save error register information in globals.  Clear
 * MC error interrupt.
 */
void
err_save_regs(void)
{
	/* save parity info */
	cpu_err_stat = *(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT);
	gio_err_stat = *(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT);
	cpu_par_adr  = *(volatile uint *)PHYS_TO_K1(CPU_ERR_ADDR);
	gio_par_adr  = *(volatile uint *)PHYS_TO_K1(GIO_ERR_ADDR);

	/*
	 * Clear the bus error by writing to the parity error register
	 * in case we're here due to parity problems.
	 */
	*(volatile uint *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
	*(volatile uint *)PHYS_TO_K1(GIO_ERR_STAT) = 0;
	flushbus();
}

/*
 * dobuserr - handle bus error interrupt
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 */
int
dobuserr(struct eframe_s *ep, inst_t *epc, uint flag)
{
	err_save_regs();

	return(dobuserr_common(ep, epc, flag, /* is_exception = */ 0));
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
 * this function sets the 'basic' algorithm for a particular machine. IP20
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

/* npage and flags aren't used for IP20, since it is only used for SCSI */
/*ARGSUSED*/
dmamap_t *
dma_mapalloc(int type, int adap, int npage, int flags)
{
	static dmamap_t smap[NSCUZZY];

	if(type != DMA_SCSI || adap >= NSCUZZY)
		return((dmamap_t *)-1);
	smap[adap].dma_adap = adap;
	smap[adap].dma_type = DMA_SCSI;
	return &smap[adap];
}

/* 
 * Copy to/from high memory
 *
 * Addresses are physical, if either is above SEG1_BASE, then map
 * the page into K2 before copying.  Writeback both source and destination
 * of the cache after the copy.
 *
 * The copy must be a page or less, and cannot cross a page boundary.
 */
static void
physcopy(unsigned int from, unsigned int to, int count)
{
    static char *cpymsg = "Out of kernel virtual addresses";
    caddr_t svaddr, dvaddr;
    pfd_t *spfd, *dpfd;

    ASSERT(count <= NBPC);
    ASSERT(from+count < K0BASE);
    ASSERT(to+count < K0BASE);
    ASSERT(btoct(from) == btoct(from+count-1));
    ASSERT(btoct(to) == btoct(to+count-1));

    if (from >= SEG1_BASE) {
	spfd = pfntopfdat(btoct(from));
	svaddr = page_mapin (spfd, VM_NOSLEEP, 0);
	if (!svaddr)
	    cmn_err_tag (25, CE_PANIC, "%s: %s\n", "source", cpymsg);
	svaddr += poff(from);
    } else
	svaddr = (caddr_t)PHYS_TO_K0(from);

    if (to >= SEG1_BASE) {
	dpfd = pfntopfdat(btoct(to));
	dvaddr = page_mapin (dpfd, VM_NOSLEEP, 0);
	if (!dvaddr)
	    cmn_err_tag (26, CE_PANIC, "%s: %s\n", "destination", cpymsg);
	dvaddr += poff(to);
    } else
	dvaddr = (caddr_t)PHYS_TO_K0(to);

    ASSERT(IS_KSEG0(svaddr) || IS_KSEG2(svaddr));
    ASSERT(IS_KSEG0(dvaddr) || IS_KSEG2(dvaddr));

    bcopy (svaddr, dvaddr, count);

    cache_operation (svaddr, count, 
	CACH_DCACHE | CACH_IO_COHERENCY | CACH_INVAL);
    cache_operation (dvaddr, count, 
	CACH_DCACHE | CACH_IO_COHERENCY | CACH_WBACK | CACH_INVAL);

    if (from >= SEG1_BASE)
	page_mapout((caddr_t)ctob(btoct(svaddr)));
    if (to >= SEG1_BASE)
	page_mapout((caddr_t)ctob(btoct(dvaddr)));
}

/* avoid overhead of testing for KDM each time through; this is
	the second part of kvtophys() */
#define KNOTDM_TO_PHYS(X) ((pnum((X) - K2SEG) < (uint)syssegsz) ? \
	(uint)pdetophys((pde_t*)kvtokptbl(X)) + poff(X) : (uint)(X))

/*
 *	Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *	Returns the actual number of bytes mapped.
 *	Since we now need to know the direction for the large memory
 *	workaround code, we look at the passed index, which fortunately
 *	isn't otherwise used until DMA starts.
 */
dma_map(dmamap_t *map, caddr_t addr, int len)
{
	uint bytes, dotrans, npages, partial;
	int readflag;
	scdescr_t *sd, *psd;
	uint physaddr, taddr;

	if((uint)addr & 0x3)
		return(0); /* can only deal with word aligned addresses */

	readflag = map->dma_index;
	map->dma_index = 0;	/* for scsi_go */
	sd = (scdescr_t *)map->dma_virtaddr;
	psd = (scdescr_t *)map->dma_addr;

	if(bytes = poff(addr)) {
		if((bytes = NBPC - bytes) > len) {
			bytes = len;
			npages = 1;
		}
		else
			npages = btoc(len-bytes) + 1;
	}
	else {
		bytes = 0;
		npages = btoc(len);
	}
	if(npages > map->dma_size) {
		npages = map->dma_size;
		partial = 0;
		len = (npages-(bytes>0))*NBPC + bytes;
	}
	else if(bytes == len)
		partial = bytes;	/* only one descr, and that is part of a page */
	else
		partial = (len - bytes) % NBPC;	/* partial page in last descr? */

	if(IS_KSEGDM(addr)) {
		addr = (caddr_t)KDM_TO_PHYS(addr);
		dotrans = 0;
	}
	else
		dotrans = 1;

	if(bytes) {
		npages--;
		physaddr = dotrans ? KNOTDM_TO_PHYS(addr) : (uint)addr;
		if(physaddr & 0xf0000000) {
			/* oops; hpc only has 28 bits... */
			taddr = map[1].dma_addr + (npages<<PNUMSHFT);
			if(!readflag) {
			    physcopy(physaddr, taddr, bytes);
			}
			else
			    sd->ophysaddr = physaddr;
			physaddr = taddr;
		}
		else
			sd->ophysaddr = 0;
		sd->cbp = physaddr;
		addr += bytes;
		sd->bcnt = bytes;
		sd->nbp = (uint)++psd;
		sd->eox = 0;
		sd++;
	}
	while(npages--) {
		unsigned incr = (!npages && partial) ? partial : NBPC;
		physaddr = dotrans ? KNOTDM_TO_PHYS(addr) : (uint)addr;
		if(physaddr & 0xf0000000) {
			/* oops; hpc only has 28 bits... */
			taddr = map[1].dma_addr + (npages<<PNUMSHFT);
			if(!readflag) {
			    physcopy(physaddr, taddr, incr);
			}
			else
			    sd->ophysaddr = physaddr;
			physaddr = taddr;
		}
		else
			sd->ophysaddr = 0;
		sd->cbp = physaddr;
		addr += incr;
		sd->bcnt = incr;
		sd->nbp = (uint)++psd;
		sd->eox = 0;
		sd++;
	}

	(sd-1)->eox = 1;	/* only used when writing */

	/* flush descriptors back to memory so HPC gets right data */
	dki_dcache_wbinval((void *)map->dma_virtaddr,
		(caddr_t)sd-map->dma_virtaddr);

	return len;
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
 */
dma_mapbp(dmamap_t *map, buf_t *bp, int len)
{
	register struct pfdat *pfd = NULL;
	register uint bytes, npages, nbytes;
	register scdescr_t	*sd, *psd;
	int			xoff;	/* bytes already xfered */
	uint physaddr;

	map->dma_index = 0;	/* for wd93_go */
	sd = (scdescr_t *)map->dma_virtaddr;
	psd = (scdescr_t *)map->dma_addr;

	/* compute page offset into xfer (i.e. pages to skip this time) */
	xoff = bp->b_bcount - len;
	npages = btoct(xoff);
	while (npages--) {
		pfd = getnextpg(bp, pfd);
		if (pfd == NULL) {
			cmn_err(CE_WARN, "dma_mapbp: !pfd, bp 0x%x len 0x%x",
				bp, len);
			return -1;
		}
	}
		
	npages = btoc(len);
	if (npages > map->dma_size) {
		npages = map->dma_size;
		len = ctob(npages);
	}
	bytes = len;

	while (npages--) {
		pfd = getnextpg(bp, pfd);
		if (!pfd) {
			/*
			 * This has been observed to happen on occasion
			 * when we somehow get a 45/{48,49} interrupt, but
			 * the count hadn't dropped to 0, and therefore we
			 * try to go past the end of the page list,
			 * hitting invalid pages.  Since this seems to be
			 * happening just a bit more often than I like,
			 * and it is very difficult to reproduce, for now
			 * we issue a warning and fail the request rather
			 * than panic'ing.
			 */
			cmn_err(CE_WARN, "dma_mapbp: Invalid mapping: pfd==0, bp %x len %x",
				bp, len);
			return -1;
		}
		physaddr = pfdattophys(pfd);
		nbytes = MIN(bytes, NBPC);
		if (physaddr & 0xf0000000) {
			/* oops; hpc only has 28 bits... */
			uint taddr = map[1].dma_addr + (npages<<PNUMSHFT);
			if (!(bp->b_flags&B_READ)) {
				physcopy(physaddr, taddr, nbytes);
			}
			else
				sd->ophysaddr = physaddr;
			physaddr = taddr;
		}
		else
			sd->ophysaddr = 0;
		sd->cbp = physaddr;
		sd->bcnt = nbytes;
		bytes -= nbytes;
		sd->nbp = (uint)++psd;
		sd->eox = 0;
		sd++;
	}
	(sd-1)->eox = 1;	/* only used when writing */

	/* flush descriptors back to memory so HPC gets right data */
	dki_dcache_wbinval((void *)map->dma_virtaddr,
		(caddr_t)sd-map->dma_virtaddr);

	return len;
}


#define	ENDIAN_OFFSET		0xc0
#define	LIO_0_OFFSET		0x1c0
#define	SCSI_CTRL_OFFSET	0x94

/*	This is a hack to make the Western Digital chip stop
 *	interrupting before we turn interrupts on.
 *	ALL IP20's have the 93B chip (except some early protos
 *  that were all supposedly upgraded), and the fifo with the
 *  burstmode dma, so just set the variable.
*/
void
wd93_init(void)
{
	int adap, cnt;
	dmamap_t *map;

	ASSERT(NSCUZZY <= SC_MAXADAP);	/* sanity check */
	wd93burstdma = 1;
	wd93cnt = 1;

	/* if more than 128 Mbytes, cut max pages per dma in half, since
	 * we need to allocate buffers in low memory for them, in case the
	 * dma is to an address > 128 Mbytes, since HPC only has 28 bits of
	 * address, and there is a 128 Mbyte hole in memory.  This number
	 * must match up with the code in wd93_getchanmap() that allocates
	 * memory.  128K per dma still covers almost all the i/os that are
	 * done, so we don't do much more remapping.
	 * See dma_map and dma_mapbp for the gross stuff, as well as
	 * scsidma_flush() 
	 */
	/* if less than 128 Mbytes, allocate memory dynamically, the old way.
	 * wd93edtinit will try to call wd93_getchanmap() 128 times for each
	 * adapter (from wd93_ha_structs variable).  Since we can't allocate
	 * 128*wd93cnt disk buffers in low memory, reduce wd93_ha_structs
	 * to 10.  This will cause 8 maps to be allocated.  Irix4 used to
	 * allocate just 7 channel maps per adapter. (kevinm 1/93)
	 */
#ifdef TEST_SCSIDMAMAP
	{
#else
	if(physmem > 0x8000) {
#endif
	    extern int wd93_ha_structs;

	    wd93_ha_structs = NMAPSPERCHAN;

	    scsi_npgs_dma /= 2;
	    scsilowmem = kvpalloc(wd93cnt*NMAPSPERCHAN*scsi_npgs_dma,
		    VM_DIRECT|VM_NOSLEEP, 0);
	    if(scsilowmem == 0)
		    cmn_err(CE_PANIC,
		      "Could not get scsi buffers for > 128 Mbyte system");
	    scsilowmem = (unchar *)KDM_TO_PHYS(scsilowmem);
	    if(((unsigned)scsilowmem+NMAPSPERCHAN*NBPP*scsi_npgs_dma)
		    & 0xf0000000)
		    cmn_err(CE_PANIC,
			"Could not get scsi buffers for > 128"
			" Mbyte system in low memory (%x)", scsilowmem);

	    /* We statically allocate for IP20, because descriptors also have
	     * to be in low memory, although the spec didn't make that clear;
	     * the same logic is used for descriptor fetches as for data DMA.
	     * we don't use dmabuf_malloc because it returns K2 space, which
	     * is slower.  We just waste a partial page of memory; allocate
	     * the maps at the same time, since they fit into the same page(s)
	     */
	    /* The following assertion insures that no descriptors will cross
	     * a page boundary on account of dma maps being a funny size 
	     */
	    ASSERT(2*sizeof(dmamap_t) % 16 == 0);

	    /* Allocate (wd93cnt*NMAPSPERCHAN) sets of maps and descriptors.
	     * For each set the two maps are followed by all of the descriptors.
	     * This makes it pretty easy to allocate maps in getchanmap
	     */
	    scmaps = kvpalloc(
		btoc(wd93cnt*NMAPSPERCHAN*((scsi_npgs_dma+1)*sizeof(scdescr_t)
		    + 2*sizeof(dmamap_t))),
		VM_DIRECT|VM_NOSLEEP, 0);
	    if(!scmaps || (kvtophys(scmaps) & 0xf0000000))
		    cmn_err(CE_PANIC,
		     "Could not get scsi descriptors in low memory (%x)", scmaps);
	    /* Set the type of each pre-allocated map to -1.  This marks
	     * it as free.*/
	    for (map = scmaps, cnt = 0; cnt < wd93cnt*NMAPSPERCHAN; ++cnt) {
		    map->dma_type = -1;
		    map = (dmamap_t *)((unchar *)map + 
			2*sizeof(dmamap_t) +
			(scsi_npgs_dma+1)*sizeof(scdescr_t));
	    }
	}

	/* early init, need low level before high
	 * level devices do their io_init's (needs some
	 * work for multiple SCSI on setlclvector) */
	for(adap=0; adap<wd93cnt; adap++) {
		if(adap == 0 && wd93_earlyinit(adap) == 0)
			setlclvector(VECTOR_SCSI, 
				(lcl_intr_func_t *)wd93intr, 0);
	}
}	

/*
 * Reset the SCSI bus. Interrupt comes back after this one.
 * this also is tied to the reset pin of the SCSI chip.
 */
void
wd93_resetbus(int adap)
{
	*scuzzy[adap].d_ctrl = SCSI_RESET;
	DELAY(25);	/* hold 25 usec to meet SCSI spec */
	*scuzzy[adap].d_ctrl = 0;	/* no dma, no flush */
}


/*
 * Set the DMA direction, and tell the HPC to set up for DMA.
 * Have to be sure flush bit no longer set.
 * dma_index will be zero after dma_map() is called, but may be
 * non-zero after dma_flush, when a target has re-connected partway
 * through a transfer.  This saves us from having to re-create the map
 * each time there is a re-connect.
 * addr arg isn't needed for IP20.
 */
/*ARGSUSED2*/
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{
	scuzzy_t *sc = &scuzzy[map->dma_adap];

	*sc->d_nextbp = (ulong)((scdescr_t*)map->dma_addr + map->dma_index);
	*sc->d_ctrl = readflag ? (SCSI_STARTDMA|SCSI_TO_MEM) : SCSI_STARTDMA;
}


/*	allocate the per-device DMA descriptor list.  Arg is the per adapter
	dmamap
*/
dmamap_t *
wd93_getchanmap(dmamap_t *cmap)
{
	dmamap_t *map;
	caddr_t vaddr;
	int cnt;

	if(!cmap || cmap->dma_adap > (wd93cnt-1))
		return cmap;	/* paranoia */

	if (scsilowmem) {  /* > 128 Mbytes of memory (2 map structs/map) */
		/* Search for a free map/descriptor set */
		for (map=scmaps, cnt=0; cnt < wd93cnt*NMAPSPERCHAN; ++cnt) {
			if (map->dma_type == -1)
				break;
			map = (dmamap_t *)((unchar *)map + 
			    2*sizeof(dmamap_t) +
			    (scsi_npgs_dma+1)*sizeof(scdescr_t));
		}
		if (cnt == wd93cnt*NMAPSPERCHAN)
			return NULL;

		map[1].dma_addr = (uint)scsilowmem + cnt*NBPP*scsi_npgs_dma;
		vaddr = (caddr_t)&map[2];	/* descriptors follow dmamaps */

		/* always aligned OK now that descr struct
		 * now 16 bytes long and we start on page boundary */
		map->dma_virtaddr = vaddr; 
		/* so we don't have to calculate each time in dma_map */
		map->dma_addr = K0_TO_PHYS(vaddr);
		map->dma_adap = cmap->dma_adap;
		/* Setting dma_type also marks the map memory as in use */
		map->dma_type = DMA_SCSI;
		map->dma_size = scsi_npgs_dma;
	} else {
		/* an individual descriptor can't cross a page boundary,
		 * so allocate one extra, and adjust first so that none cross.
		 * Easier to always do, rather than to test if it actually
		 * crossed the boundary.  Thanks to Dan for the idea.
		 * Allocate an extra map struct for dma_map() use */
		map = (dmamap_t *)kern_malloc(2*sizeof(*map));
		vaddr = kmem_alloc((1+NSCSI_DMA_PGS) * sizeof(scdescr_t),
					  VM_CACHEALIGN);
		map->dma_virtaddr = vaddr + 
			((NBPP-poff(vaddr)) % sizeof(scdescr_t));
		/* so we don't have to calculate each time in dma_map */
		map->dma_addr = kvtophys((void*)map->dma_virtaddr);
		map->dma_adap = cmap->dma_adap;
		map->dma_type = DMA_SCSI;
		map->dma_size = NSCSI_DMA_PGS;
	}
	return map;
}


/*
 * Flush the fifo to memory after a DMA read.
 * OK to leave flush set till next DMA starts.  SCSI_STARTDMA bit gets
 * cleared when fifo has been flushed.
 * We 'advance' the dma map chain when we are called, so that when
 * some data was transferred, but more remains we don't have
 * to reprogram the entire descriptor chain.
 * When reading, this is only valid after the flush has completed.  So
 * we check the SCSI_STARTDMA bit to be sure flush has completed.
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
 * Also check for a parity error after a DMA operation.
 * return 1 if there was one
*/
wd93dma_flush(dmamap_t *map, int readflag, int xferd)
{
	scdescr_t *sd;
	scuzzy_t *scp = &scuzzy[map->dma_adap];
#ifdef LATER
	unsigned data;
	static paritychk = 0;	/* so we can easily re-enable while
				   debugging this problem */
#endif
	unsigned fcnt=0;
	if(readflag && xferd) {
		{
			unsigned cnt = 512;	/* normally only 2 or 3 times */
			fcnt = 0;
			*scp->d_ctrl |= SCSI_FLUSH;
			while((*scp->d_ctrl & SCSI_STARTDMA) && --cnt)
				;
			if(cnt == 0)
				cmn_err(CE_PANIC,
				   "SCSI DMA in progress bit never cleared\n");
		}
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		* disconnects without transferring data, and HPC already
		* grabbed the first 64 bytes into it's fifo; otherwise gets
		* the first 64 bytes twice, since it 'notices' that curbp was
		* reset, but not that is the same.  */
		*scp->d_ctrl = 0;
	if(!xferd)
		return 0;	/* disconnect with no i/o */

	/* index from last wd93_go */
	sd = (scdescr_t*)map->dma_virtaddr + map->dma_index;

	/* handle possible partial page in first descr */
	if(xferd >= sd->bcnt) {
		if(readflag && sd->ophysaddr) {
		    physcopy(sd->cbp, sd->ophysaddr, sd->bcnt);
		    /* get it right for next time, in case part of a page */
		    sd->ophysaddr += sd->bcnt;
		}
		xferd -= sd->bcnt;
		sd++;
	}
	if(xferd) {
	    int pgremainder = poff(xferd);
	    if(readflag) {
		while(xferd >= NBPP) {
		    if(sd->ophysaddr) {
			physcopy(sd->cbp, sd->ophysaddr, NBPP);
		    }
		    xferd -= NBPP;
		    sd++;
		}
		if(xferd && sd->ophysaddr) {
		    /* in case this turns out to be a short read */
		    fcnt = MIN(sd->bcnt, xferd);
		    physcopy(sd->cbp, sd->ophysaddr, fcnt);
		    /* get it right for next time, in case part of a page */
		    sd->ophysaddr += xferd;
		}
	    }
	    else
		    sd += xferd / NBPP;
	    sd->bcnt -= pgremainder;
	    sd->cbp += pgremainder;
	    /* flush descriptor back to memory so HPC gets right data */
	    dki_dcache_wbinval((void *)sd, sizeof(*sd));
	}
	/* prepare for rest of dma (if any) after next reconnect */
	map->dma_index = sd - (scdescr_t *)map->dma_virtaddr;

#ifdef	LATER
	if(paritychk && !readflag &&
		(data=(*(uint *)PHYS_TO_K1(PAR_ERR_ADDR) & PAR_DMA))) {
		/* note that the parity error could have been from some other
		 * devices DMA also (parallel, or DSP) */
		readflag = *(volatile int *)PHYS_TO_K1(DMA_PAR_ADDR);
		log_perr(readflag, data, 0, 1);
		cmn_err(CE_WARN, dma_par_msg, readflag<<22);
		*(volatile u_char *)PHYS_TO_K1(PAR_CL_ADDR) = 0;	/* clear it */
		flushbus();
		return(1);
	}
	else
#endif	/* LATER */
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
	private.p_led_counter = 1;

	set_leds(0);
}


/*
 * Implementation of syssgi(SGI_SETLED,a1)
 *
 */
void
sys_setled(a1)
int a1;
{
	set_leds(a1);
}

/*
 * Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.  The SGI_SETLED can be used to turn
 * it on or off, unless we have forced it to the on state via p_led_value.
 */
void
set_leds(int pattern)
{
	if (pattern)		/* amber LED */
		*(volatile char *)PHYS_TO_K1(CPU_AUX_CONTROL) |= CONSOLE_LED;
	else			/* green LED */
		*(volatile char *)PHYS_TO_K1(CPU_AUX_CONTROL) &= ~CONSOLE_LED;
	heart_led_control(pattern);
}

/*
 * On other machines, change leds value to next value.  Used by clock handler
 * to show a visible heartbeat.
 */
void
bump_leds(void)
{
}


/*
 * sendintr - send an interrupt to another CPU; fatal error on IP20, no
 * args used, obviously.
 *
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	panic("sendintr");
	/* NOTREACHED */
}


/* START of Time Of Day clock chip stuff (to "END of Time Of Day clock chip" */

/*
 * Get the time/date from the dp8572/8573, and reinitialize
 * with a guess from the file system if necessary.
 */
void
inittodr(time_t base)
{
	register uint todr;
	long deltat;
	int TODinvalid = 0;
	extern int todcinitted;
	extern u_char miniroot;
	int s;

	s = splhi();

	todr = rtodc(); /* returns seconds from 1970 00:00:00 GMT */
	settime(todr, 0);

	if (miniroot) {	/* no date checking if running from miniroot */
		splx(s);
		return;
	}

	/*
	 * Complain if the TOD clock is obviously out of wack. It
	 * is "obvious" that it is screwed up if the clock is behind
	 * the 1988 calendar currently on my wall.  Hence, TODRZERO
	 * is an arbitrary date in the past.
	 */
	if (todr < TODRZERO) {
		if (todr < base) {
			cmn_err_tag(27, CE_WARN, "lost battery backup clock--using file system time");
			TODinvalid = 1;
		} else {
			cmn_err_tag(28, CE_WARN, "lost battery backup clock");
			setbadclock();
		}
	}

	/*
	 * Complain if the file system time is ahead of the time
	 * of day clock, and reset the time.
	 */
	if (todr < base) {
		if (!TODinvalid)	/* don't complain twice */
			cmn_err_tag(29, CE_WARN, "time of day clock behind file system time--resetting time");
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
		cmn_err_tag(30,CE_WARN,"clock %s %d days",
		    time < base ? "lost" : "gained", deltat/SECDAY);

	if (isbadclock())
		cmn_err_tag(31, CE_WARN, "CHECK AND RESET THE DATE!");

	splx(s);
}


/* 
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.
 */

void
resettodr() {
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

rtodc()
{
	uint month, day, year, hours, mins, secs;
	register int i;
	char	buf[ 6 ];

	if (!(_clock_func_read(buf) & RTC_RUN))
		return(0);

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
	year += YRREF;

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
	char buf[ 7 ];
	register long year_secs = time;
	register month, day, hours, mins, secs, year;

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
	 * Do the next two buf[] entries out of order so that we can
	 * compute the leap year offset before YRREF gets subtracted out
	 */
	buf[ 6 ] = year % 4;

	/*
	 * The number of years since YRREF (1970) is what actually gets
	 * stored in the clock.  Leap years work on the dp8572/73 because
	 * the hardware has a register that keeps track of the leap count
	 */
	year -= YRREF;
	buf[ 5 ] = (char)( ( ( year / 10 ) << 4 ) | ( year % 10 ) );

	_clock_func_write( buf );

	/*
	 * clear the flag in nvram that says whether or not the clock
	 * date is correct or not.
	 */
	clearbadclock();
}

static int
_clock_func_read(char *ptr)
{
	register struct dp8573_clk *clockp;
	char state;
	int i;

	clockp = (struct dp8573_clk *)RT_CLOCK_ADDR;

	/* latch the Time Save RAM */
	clockp->ck_status = RTC_STATINIT;
	clockp->ck_tsavctl0 = RTC_TIMSAVON;
	clockp->ck_tsavctl0 = RTC_TIMSAVOFF;

	/* copy out sec, min, hours, day, and month */
	for (i = 0; i < 5; i++) {
		*ptr++ = (char) (clockp->ck_timsav[i] >> CLK_SHIFT);
	}

	/* get the year */
	*ptr = (char) (clockp->ck_counter[6] >> CLK_SHIFT);

	/* return the real time mode register */
	clockp->ck_status |= RTC_RS;
	state = (char) (clockp->ck_rtime1 >> CLK_SHIFT);
	return(state);
}


static void
_clock_func_write(char *ptr)
{
	register struct dp8573_clk *clockp;
	int i;

	clockp = (struct dp8573_clk *)RT_CLOCK_ADDR;

	clockp->ck_status = RTC_STATINIT;
	clockp->ck_tsavctl0 = RTC_TIMSAVOFF;
	clockp->ck_pflag0 = RTC_PFLAGINIT;
	clockp->ck_status = RTC_RS;
	clockp->ck_int1ctl1 = RTC_INT1INIT;
	clockp->ck_outmode1 = RTC_OUTMODINIT;

	clockp->ck_rtime1 &= ~RTC_RUN;
	for (i = 1; i < 7; i++) {
		clockp->ck_counter[i] = (*ptr++ << CLK_SHIFT);
	}
	clockp->ck_rtime1 = RTC_RUN | (*ptr << CLK_SHIFT);
}


static void
setbadclock(void)
{
	register struct dp8573_clk *clockp;

	clockp = (struct dp8573_clk *)RT_CLOCK_ADDR;
	clockp->ck_cmpram[2] |= RTC_BADCLOCK;
}

static void
clearbadclock(void)
{
	register struct dp8573_clk *clockp;

	clockp = (struct dp8573_clk *)RT_CLOCK_ADDR;
	clockp->ck_cmpram[2] &= ~RTC_BADCLOCK;
}

static int
isbadclock(void)
{
	register struct dp8573_clk *clockp;

	clockp = (struct dp8573_clk *)RT_CLOCK_ADDR;
	return(clockp->ck_cmpram[2] & RTC_BADCLOCK);
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

/*	The prom leaves a pointer to its hinv table in the restart block
	in the creatively named field "rb_occurred".  This routine looks
	through the hinv table for the entry for the DSP and sets the
	global variable "dsp_rev_level".

	If there's no entry for audio in the prom's hinv table, then
	dsp_rev_level is set to -1.

	This code depends on the layout of the inventory struct being the
	same in both the kernel and the dsp.

	If the kernel gets bigger than about 3.5 megabytes, this won't
	work because it depends on the prom's BSS being valid.  People
	say that's OK, cause a lot of other stuff would break as well
	if the kernel got that big.

*/

void get_dsp_hinv_info(void)
{
	COMPONENT *audio;

	audio = arcs_findtype(arcs_getchild(NULL),AudioController);
	dsp_rev_level = (audio) ? audio->Key : -1;
}


/*	Fetch the nvram table from the PROM.
	Must be called before the DSP is enabled.
	The table is static because we can't call kern_malloc at this point,
	and allocating whole pages would be wasteful.  We can at least tell
	if the table is larger than we think it is by the return value.
*/

/* the actual size of the table, including the null entry at the end
	is 25 members as of 8/92 */
static struct nvram_entry nvram_tab[26];

static void
load_nvram_tab(void)
{
#ifdef NEVERDOTHIS
	int size =
#endif
		arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab));

#ifdef NEVERDOTHIS /* happens before private data area setup, dies in printf */
	if(size == -1) /* should NEVER happen */
		cmn_err_tag(32, CE_WARN, "Unable to load nvram information");
#ifdef DEBUG
	else if(size)
		cmn_err(CE_WARN, "nvram table has grown by %d bytes, do not have all info", size);
#endif /* DEBUG */
#endif /* NEVERDOTHIS */

}

/*	START of NVRAM stuff (aprox 350 lines).  It's pretty much the same
	as the standalone code but cleaned up a bit because it's IP20 only.
	assume using the C56/C66 part, the one with 128/256 16-bits
	registers (2048/4096 bits)
*/

/*
 * assign different values to cpu_auxctl to access different NVRAMs, one on
 * the backplane, two possible others on optional GIO ethernet cards
 */
volatile unchar *cpu_auxctl;
#define	CPU_AUXCTL	cpu_auxctl


static unchar console_led_state;
/*
 * enable the serial memory by setting the console chip select
 */
static void
nvram_cs_on(void)
{
	console_led_state = *CPU_AUXCTL & CONSOLE_LED;

	*CPU_AUXCTL &= ~CPU_TO_SER;
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~NVRAM_PRE;
	DELAY(1);
	*CPU_AUXCTL |= CONSOLE_CS;
	*CPU_AUXCTL |= SERCLK;
}

/*
 * turn off the chip select
 */
static void
nvram_cs_off(void)
{
	*CPU_AUXCTL &= ~SERCLK;
	*CPU_AUXCTL &= ~CONSOLE_CS;
	*CPU_AUXCTL |= NVRAM_PRE;
	*CPU_AUXCTL |= SERCLK;

	*CPU_AUXCTL = (*CPU_AUXCTL & ~CONSOLE_LED) | console_led_state;
}

#define	BITS_IN_COMMAND	11
/*
 * clock in the nvram command and the register number.  For the
 * natl semi conducter nv ram chip the op code is 3 bits and
 * the address is 6/8 bits. 
 */
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

/*
 * after write/erase commands, we must wait for the command to complete
 * write cycle time is 10 ms max (~5 ms nom); we timeout after ~20 ms
 *    NVDELAY_TIME * NVDELAY_LIMIT = 20 ms
 */
#define NVDELAY_TIME	100	/* 100 us delay times */
#define NVDELAY_LIMIT	200	/* 200 delay limit */

static int
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



/*
 * get_nvreg -- read a 16 bit register from non-volatile memory.  Bytes
 *  are stored in this string in big-endian order in each 16 bit word.
 */
ushort
get_nvreg(int nv_regnum)
{
	ushort ser_read = 0;
	int i;
	unchar health_led_off = *CPU_AUXCTL & CONSOLE_LED;

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

	*CPU_AUXCTL = (*CPU_AUXCTL & ~CONSOLE_LED) | health_led_off;
	return(ser_read);
}


/*
 * set_nvreg -- writes a 16 bit word into non-volatile memory.  Bytes
 *  are stored in this register in big-endian order in each 16 bit word.
 */
static int
set_nvreg(int nv_regnum, unsigned short val)
{
	int error;
	int i;
	unchar health_led_off = *CPU_AUXCTL & CONSOLE_LED;

	*CPU_AUXCTL &= ~NVRAM_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0);	
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_WRITE, nv_regnum);

	/*
	 * clock the data into serial mem 
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

	*CPU_AUXCTL = (*CPU_AUXCTL & ~CONSOLE_LED) | health_led_off;

	return (error);
}

static char
nvchecksum(void)
{
	register int nv_reg;
	ushort nv_val;
	register signed char checksum;

    /*
     * Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
     * (all-ones) checksum.
     */
	checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
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

/*
 * set_an_nvram -- write string to non-volatile memory
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
	    } else if (!ISXDIGIT(*cp) || !ISXDIGIT(*(cp+1))) {
		return -1;
	    } else {
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


/* Set nv ram variable name to value
 *	return  0 - success, -1 on failure
 * All the real work is done in set_an_nvram().
 * There is no get_nvram because the code that would use
 * it always gets it from kopt_find instead.
 * This is called from syssgi().
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

/* END of NVRAM stuff */

void
init_all_devices(void)
{
        uphwg_init(hwgraph_root);
}

/*
** do the following check to make sure that a wrong config
** in system.gen won't cause the kernel to hang
** ignores the MP stuff and just does some basic sanity setup.
*/
void
allowboot(void)
{
	register int i;	

	for (i=0; i<bdevcnt; i++)
		bdevsw[i].d_cpulock = 0;

	for (i=0; i<cdevcnt; i++)
		cdevsw[i].d_cpulock = 0;
}

/*
 * get_except_norm
 *
 *	return address of exception handler for all exceptions other
 *	then utlbmiss.
 */
inst_t *
get_except_norm()
{
	extern inst_t exception[];

	return exception;
}

/* 
 * return board revision level (0-7)
 */
int
board_rev(void)
{
    register unsigned int rev_reg = 
	    *(volatile unsigned int *)PHYS_TO_K1(BOARD_REV_ADDR);

    return (~rev_reg & REV_MASK) >> REV_SHIFT;
}

/*
 * return 1 for V50, 0 for normal IP20
 */
int
is_v50()
{
    return has_vmax;
}

void
add_ioboard(void){}

void
add_cpuboard(void)
{
	add_to_inventory(INV_PARALLEL, INV_ONBOARD_PLP, 0, 0, 1);

	add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);

	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(),0,
						private.p_cputype_word);
	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(),0,
						private.p_fputype_word);
	/* for type INV_CPUBOARD, ctrl is CPU board type, unit = -1 */
	/* Double the hinv cpu frequency for R4000s */
	add_to_inventory(INV_PROCESSOR,INV_CPUBOARD, 2*private.cpufreq, -1, 
					INV_IP20BOARD);
	/* don't do the jump workaround for rev 3 or greater parts to reduce
	 * overhead.  Still has assorted branches, etc. in various places,
	 * but we still have some 2.2 parts, so we can't compile the kernel
	 * without JUMP_WAR. */
	if(((private.p_cputype_word>>4) & 0xf) >= 3) {
		extern int R4000_jump_war_correct;
		R4000_jump_war_correct = 0;
	}

#define	IS_USING_16M_DRAM(x)	(((x) & MEMCFG_DRAM_MASK) >= MEMCFG_64MRAM)

	/*
	 * 16M DRAM part requires the rev C MC to function correctly
	 * note: cannot do this in mlreset() since it is too early to use
	 * cmn_err() there
	 */
	if (mc_rev_level < 3) {
		if (IS_USING_16M_DRAM(memconfig_0)
		    || IS_USING_16M_DRAM(memconfig_0 >> 16)
		    || IS_USING_16M_DRAM(memconfig_1)
		    || IS_USING_16M_DRAM(memconfig_1 >> 16)) {
			cmn_err(CE_WARN, "This system may require the revision C Memory Controller (MC) chip\n"
			"\tin order to operate correctly with the type of memory SIMMs installed.\n"
			"\tYou do not need the new MC unless you experience memory errors.");
		}
	}
}

int
cpuboard(void)
{
	return INV_IP20BOARD;
}

/*
 * On IP20, unix overlays the prom's bss area.  Therefore, only
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
{
	/*
	 * Check for presence of symmon
	 */
	if ( SPB->DebugBlock && ((db_t *)SPB->DebugBlock)->db_printf )
		(*((db_t *)SPB->DebugBlock)->db_printf)(fmt, ARGS);
	else
	/*
	 * cn_is_inited() implies that PROM bss has been wiped out
	 */
	if (cn_is_inited())
		cmn_err(CE_CONT,fmt,ARGS);
	else
	/*
	 * try printing through the prom
	 */
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

	case ADAP_VME:
		return is_v50(); /* V50 has one vme bus, blackjack none */

	case ADAP_GIO:
		return (1);

	default:
		return(0);

	}
}

piomap_t*
pio_mapalloc (uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
	int	 i;
	piomap_t *piomap;

	if (bus < ADAP_GFX || bus > ADAP_GIO)
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
        for (i = 0; i < 8; i++)
		if (name[i])
			piomap->pio_name[i] = name[i];

	return(piomap);
}

caddr_t 
pio_mapaddr (piomap_t* piomap, iopaddr_t addr)
{
	int vaddr;

	switch (piomap->pio_bus) {
	
	case ADAP_LOCAL:

		vaddr = 0;
		break;

	case ADAP_GIO:

		/* old form of VECTOR lines would have K1 address, while new
		 * form would pass Physical address. So convert to K1 anyway 
		 */ 
		vaddr = PHYS_TO_K1(addr); 
		break;
	}

	return((caddr_t)vaddr);
}

void
pio_mapfree (piomap_t* piomap)
{
	if (piomap)
		kern_free(piomap);
}

static void
updatespb(void)
{
	/*  Old IP20 proms (version 0) had intermediate arcs trasfer
	 * vectors, which need to be swizzled so we can run on it.
	 *
	 *  However, sash may have already swizzled it for us, or
	 * we may have swizzled it already since is_arcsprom() is
	 * called twice with debug kernel startup.  If the vector
	 * has less the 34 entries (4 bytes each) then go ahead
	 * and update it.
	 *
	 * NOTE: The kernel oldtv1_to_latest() macro is minimal.
	 *	 If using more arcs functionality, check the full
	 *	 update macro in <arcs/tvector.h>.
	 */

#if _K64PROM32
	if (SPB32->TVLength < (34*4)) {
		oldFirmwareVector_1 *oldtv = (oldFirmwareVector_1 *)
			SPB32->TransferVector;
		FirmwareVector *tv = SPB32->TransferVector;

		oldtv1_2_latest(SPB32,oldtv,tv);
	}
#else
	if (SPB->TVLength < (34*4)) {
		oldFirmwareVector_1 *oldtv = (oldFirmwareVector_1 *)
			SPB->TransferVector;
		FirmwareVector *tv = SPB->TransferVector;

		oldtv1_2_latest(SPB,oldtv,tv);
	}
#endif
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
	static int sysclk_divider[] = {2, 3, 4, 6, 8};
	unsigned int ec = (get_r4k_config() & CONFIG_EC) >> 28;

	*cycle = RPSS_DIV * 1000000 * sysclk_divider[ec] /
		(private.cpufreq * 2);
	return PHYS_TO_K1(RPSS_CTR);
}

/*
 * This routine returns the value of the cycle counter. It needs to
 * be the same counter that the user sees when mapping in the CC
 */
__int64_t
absolute_rtc_current_time()
{
	unsigned int time;
	time =  *(unsigned int *) PHYS_TO_K1(RPSS_CTR);
	return((__int64_t) time);
}

#if DEBUG

#include "sys/immu.h"

extern uint _tlbdropin(unsigned char *, caddr_t, pte_t);
extern void _tlbdrop2in(unchar, caddr_t, pte_t, pte_t);

uint tlbdropin(
	unsigned char *tlbpidp,
	caddr_t vaddr,
	pte_t pte)
{
	if (pte.pte_vr)
		ASSERT(pte.pte_cc == (PG_NONCOHRNT >> PG_CACHSHFT) || \
				pte.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
	return(_tlbdropin(tlbpidp, vaddr, pte));
}

void tlbdrop2in(unsigned char tlb_pid, caddr_t vaddr, pte_t pte, pte_t pte2)
{
	if (pte.pte_vr)
		ASSERT(pte.pte_cc ==  (PG_NONCOHRNT >> PG_CACHSHFT) || \
				pte.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
	if (pte2.pte_vr)
		ASSERT(pte2.pte_cc ==  (PG_NONCOHRNT >> PG_CACHSHFT) || \
				pte2.pte_cc == (PG_UNCACHED >> PG_CACHSHFT));
	_tlbdrop2in(tlb_pid, vaddr, pte, pte2);
}


#endif	/* DEBUG */

/*
 * timer_freq is the frequency of the 32 bit counter timestamping source.
 * timer_high_unit is the XXX
 * Routines that references these two are not called as often as
 * other timer primitives. This is a compromise to keep the code
 * well contained.
 * Must be called early, before main().
 */
void
timestamp_init(void)
{
	timer_freq = findcpufreq() * 1000000;
	timer_unit = NSEC_PER_SEC/timer_freq;
	timer_high_unit = timer_freq;	/* don't change this */
	timer_maxsec = TIMER_MAX/timer_freq;
}

int
cpu_isvalid(int cpu)
{
	return !((cpu < 0) || (cpu >= maxcpus) || (pdaindr[cpu].CpuId == -1));
}

/*
 * This is just a stub on an IP20.
 */
/*ARGSUSED*/
void
debug_stop_all_cpus(void *stoplist)
{
	return;
}

static int
is_thd(int level)
{
	switch(level) {

	/* Threaded */
	case VECTOR_SCSI:
	case VECTOR_ENET:
	case VECTOR_PLP:
	case VECTOR_GIO1:
	case VECTOR_GIO2:
	case VECTOR_VIDEO:
	case VECTOR_DUART:
		return 1;

	/* Not threaded */
	case VECTOR_GIO0:
	case VECTOR_ACFAIL:

	/* Untested as threads */
	case VECTOR_GDMA:

	/* ? */
	case VECTOR_VME0:
	case VECTOR_ILOCK:
	case VECTOR_VME1:
	case VECTOR_DSP:
	default:
		return 0;
	} /* switch level */
} /* is_thd */

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
	} else {
		int_mask = K1_LIO_1_MASK_ADDR;
	}

	s = disableintr();
	*int_mask |= (l->bit);
	enableintr(s);

	/* Wait for next interrupt */
	ipsema(&l->lcl_isync);	/* Thread restarts at top. */
} /* lcl_intrd */

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
		
}
