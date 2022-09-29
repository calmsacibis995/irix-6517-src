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
 * IP22 specific functions
 */

#ident "$Revision: 1.32 $"

#include <sys/types.h>
#include <sys/IPMHSIM.h>
#include <sys/buf.h>
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/i8254clock.h>
/*#include <sys/ds1286.h>*/
#include <sys/invent.h>
#include <sys/kopt.h>
#include <sys/loaddrs.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/vproc.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/scsi.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <sys/kabi.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/var.h>
/*#include <sys/vmereg.h>*/
#include <sys/edt.h>
#include <sys/pio.h>
/*#include <sys/eisa.h>*/
/*#include <sys/arcs/spb.h>*/
/*#include <sys/arcs/debug_block.h>*/
/*#include <sys/hal2.h>*/
#include <sys/parity.h>

extern caddr_t map_physaddr(paddr_t);
extern void unmap_physaddr(caddr_t);

typedef caddr_t dmavaddr_t;
typedef paddr_t dmamaddr_t;

#ifdef _VCE_AVOIDANCE
extern int vce_avoidance;
#endif
#ifdef R4000PC
extern int pdcache_size;
#endif
#ifdef R4600
extern int two_set_pcaches;
#endif

uint_t vmeadapters = 0;

#define MAXCPU	1
#define RPSS_DIV		2
#define RPSS_DIV_CONST		(RPSS_DIV-1)

int	maxcpus = MAXCPU;

extern unsigned long cause_ip5_count;
extern unsigned long cause_ip6_count;
unsigned long r4k_intr_req_flag = 0;

volatile unchar *cpu_auxctl;

/* processor-dependent data area */
pdaindr_t	pdaindr[MAXCPU];

void set_leds(int);
void set_autopoweron(void);
void resettodr(void);


static uint scdescr, scmaps;

unsigned int mc_rev_level;

int	use_sableio = 0;

/*
 * Processor interrupt table
 */
extern	void buserror_intr(struct eframe_s *);
extern	void clock(struct eframe_s *);
extern	void ackkgclock(void);
static	void lcl1_intr(struct eframe_s *);
static	void lcl0_intr(struct eframe_s *);
extern	void timein(struct eframe_s *);
extern	void r4kcount_intr(struct eframe_s *);
static	void power_bell(void);
static	void flash_led(int);

typedef void (*intvec_func_t)(struct eframe_s *);

struct intvec_s {
	intvec_func_t	isr;		/* interrupt service routine */
	uint		msk;		/* spl level for isr */
	uint		bit;		/* corresponding bit in sr */
	int		fil;		/* make sizeof this struct 2^n */
};

static struct intvec_s c0vec_tbl[] = {
	0,	    0,			0,		0,  /* (1 based) */
	timein,     SR_IMASK1|SR_IEC,   SR_IBIT1 >> 8,	0,  /* softint 1 */
	0,	    SR_IMASK2|SR_IEC,	SR_IBIT2 >> 8,	0,  /* softint 2 */
	lcl0_intr,  SR_IMASK3|SR_IEC,	SR_IBIT3 >> 8,	0,  /* hardint 3 */
	lcl1_intr,  SR_IMASK4|SR_IEC,	SR_IBIT4 >> 8,	0,  /* hardint 4 */
	clock,	    SR_IMASK5|SR_IEC,	SR_IBIT5 >> 8,	0,  /* hardint 5 */
							    /* hardint 6 */
	(intvec_func_t)ackkgclock, SR_IMASK6|SR_IEC,	SR_IBIT6 >> 8,	0,
	buserror_intr, SR_IMASK7|SR_IEC,SR_IBIT7 >> 8,	0,  /* hardint 7 */
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
static	void stray0(int,struct eframe_s *);
static	void stray1(int, struct eframe_s *);
static  void stray2(int, struct eframe_s *);
static	void powerfail(int, struct eframe_s *);
static  void powerintr(int, struct eframe_s *);

static struct lclvec_s {
					/* interrupt service routine */
	void	(*isr)(int, struct eframe_s *);
	uint	bit;			/* corresponding bit in sr */
	int	arg;			/* arg to pass.  Needed for 2nd
					 * scsi/ethernet/parallel interface.
					 * 0=unchanged by setlclvector() */
	uint	fill;			/* pad to power of 2 */
};

static struct lclvec_s lcl0vec_tbl[] = {
	{ 0,		0 } ,		/* table is 1 based */
	{ stray0,	0x01, 1 },
	{ stray0,	0x02, 2 },
	{ stray0,	0x04, 3 },
	{ stray0,	0x08, 4 },
	{ stray0,	0x10, 5 },
	{ stray0,	0x20, 6 },
	{ stray0,	0x40, 7 },
	{ stray0,	0x80, 8 },
	};


static struct lclvec_s lcl1vec_tbl[] = {
	{ 0,		0 },		/* table is 1 based */
	{ stray1,	0x01, 1 },
	{ stray1,	0x02, 2 },
	{ stray1,	0x04, 3 },
	{ stray1,	0x08, 4 },
	{ stray1,	0x10, 5 },
	{ stray1,	0x20, 6 },
	{ stray1,	0x40, 7 },
	{ stray1,	0x80, 8 },
	};



scuzzy_t scuzzy[] = {
        {
		(u_char *)PHYS_TO_K1(MHSIM_SCSI0A_ADDR),
		(u_char *)PHYS_TO_K1(MHSIM_SCSI0D_ADDR),
		VECTOR_SCSI
        },
};


/*	used to sanity check adap values passed in, etc */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))

/* table of probeable kmem addresses */

struct kmem_ioaddr kmem_ioaddr[] = {
        { 0, 0 },
};

short	cputype = 32;   /* integer value for cpu (i.e. xx in IPxx) */

static uint sys_id;	/* initialized by init_sysid */

u_short i_dma_burst, i_dma_delay;
u_short dma_burst = 0, dma_delay = 0;

#define ISXDIGIT(c) ((('a'<=(c))&&((c)<='f'))||(('0'<=(c))&&((c)<='9')))

#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0')  : ((c)-'a'+10))
char eaddr[6];


long arcs_write(unsigned long fd, void *buf, unsigned long n, unsigned long *cnt);
char *arcs_getenv(char *name);
void *arcs_eaddr(unsigned char *cp);
void arcs_setuproot(void);
void prom_rexec(struct promexec_args *uparg);
void prom_reinit(void);



/*
 * initialize the serial number.  Called from mlreset.
 */
init_sysid()
{
        eaddr[0] = 0;
        eaddr[1] = 1;
        eaddr[2] = 2;
        eaddr[3] = 3;
        eaddr[4] = 4;
        eaddr[5] = 5;
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
	 * serial number is only 4 bytes long on IP22.  Left justify
	 * in memory passed in...  Zero out the balance.
	 */

	*(uint *) hostident = sys_id;
	bzero(hostident + 4, MAXSYSIDSIZE - 4);

	return 0;
}

/*
 * get_nvreg -- read a 16 bit register from non-volatile memory.  Bytes
 *  are stored in this string in big-endian order in each 16 bit word.
 */
ushort
get_nvreg(int nv_regnum)
{

#define CACHESZ_REG	0x11	/* see cacheops.s */

   if (nv_regnum == CACHESZ_REG) {
      /*
       * CACHESZ_REG is the only nvreg defined for IPMHSIM
       */
      rev_id_t ri;
      int cfg;
      extern int get_cpu_irr(void);
      extern int get_r4k_config(void);

      ri.ri_uint = get_cpu_irr();
      if (ri.ri_imp != C0_IMP_TRITON && ri.ri_imp != C0_IMP_RM5271) {
	      if (use_sableio)
		      return(0);
	      else
		      return (*(ushort *) MHSIM_SC_SIZE_ADDR) / NBPP;
      }
      cfg = get_r4k_config();
      if (cfg & CONFIG_TR_SC) /* 1 == Scache not present */
	      return(0);
      return(((512 * 1024) / NBPP) <<
	     ((cfg & CONFIG_TR_SS) >> CONFIG_TR_SS_SHFT));
   }
   return 0;
}

/* Indigo2/Indy boolean routines for machine dependant routines.
 */
int is_fullhouse_flag = 0;

/*
 * return 1 if fullhouse system, 0 if guinness system
 */
int
is_fullhouse(void)
{
	return(0);
}

static pgno_t memsize = 0;	/* total ram in clicks */

/*
 * szmem - size main memory
 * Returns # physical pages of memory
 */
/* ARGSUSED */
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
    int bank;
    int bank_size;

    if (!memsize) {
	    if (use_sableio) {
		    if (maxpmem)
			    memsize = maxpmem;
		    else
			    memsize = (8 * 1024 * 1024) / NBPP; /* 8 MB default */
	    } else
		    memsize = (*(uint *) MHSIM_MEMSIZE_ADDR)/NBPP;
    }

    if (maxpmem)
	    memsize = MIN(memsize,maxpmem);
    return memsize;
}

#ifndef _MEM_PARITY_WAR
static
#endif /* _MEM_PARITY_WAR */
int
is_in_pfdat(pgno_t pfn)
{
	pgno_t min_pfn = btoct(kpbase);

	return(pfn >= min_pfn &&
	       pfn < (min_pfn + maxclick));
}


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
    int current_color;
    static int *pginuse;

    if (!private.p_cacheop_pages) {
	private.p_cacheop_pages = kvalloc(cachecolormask+1, VM_VACOLOR, 0);
	pginuse = (int *)kmem_zalloc(sizeof(int) * (cachecolormask+1), 0);
	ASSERT(private.p_cacheop_pages != NULL && pginuse != NULL);
    }
#ifdef _VCE_AVOIDANCE
    if (vce_avoidance && is_in_pfdat(pfn))
	current_color = pfd_to_vcolor(pfntopfdat(pfn));
    else
	current_color = -1;
    if (current_color == -1)
	current_color = colorof(addr);
#else
    current_color = colorof(addr);
#endif

    kvaddr = (void *)((uint)private.p_cacheop_pages +
		  (NBPP * current_color) + poff(addr));

    s = splhi();
    if (pginuse[current_color]) {
#ifdef DEBUG
	mapflushcoll++;
	if (pginuse[current_color] > mapflushdepth)
	    mapflushdepth = pginuse[current_color];
#endif
	/* save last mapping */
	lastpgi = kvtokptbl(kvaddr)->pgi;
	ASSERT(lastpgi && lastpgi != mkpde(PG_G, 0));
	unmaptlb(0, btoct(kvaddr));
    }
    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn));
    pginuse[current_color]++;

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
    pginuse[current_color]--;
    splx(s);
}

/*
 * On the IP32, the secondary cache is a unified i/d cache. The
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
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance && is_in_pfdat(pfn)) {
		    mapflush(addr, len, pfn, flags & ~CACH_DCACHE);
		    return;
		} else
#endif /* _VCE_AVOIDANCE */
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
	 * Remap the page to flush if
	 * 	the page is mapped and vce_avoidance is enabled, or
	 *	the page is in high memory
	 */

	if (IS_KUSEG(addr) || (flags & CACH_NOADDR)) {
		ASSERT(pfn);
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance && is_in_pfdat(pfn)) {
		    mapflush(addr, len, pfn, flags & ~CACH_ICACHE);
		    return;
		} else
#endif /* _VCE_AVOIDANCE */
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
   return 0;
}

/*
 * getmaxclick - returns pfn of last page of ram
 */
/*ARGSUSED*/
pfn_t
node_getmaxclick(cnodeid_t node)
{
	/* 
	 * szmem must be called before getmaxclick because of
	 * its depencency on maxpmem
	 */
	ASSERT(memsize);
	
	return memsize - 1;
}

is_in_main_memory(pgno_t pfn)
{
	return(pfn >= 0 && pfn <= memsize);
}

/*
 * setupbadmem - mark the hole in memory between seg 0 and seg 1.
 *		 return count of non-existent pages.
 */

pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
   return 0;
}



/*
 * mlreset - very early machine reset - at this point NO interrupts have been
 * enabled; nor is memory, tlb, p0, etc setup.
 */
#ifdef R4600
int	enable_orion_parity = 0;
#endif
/* ARGSUSED */
void
mlreset(int junk)
{
	extern int reboot_on_panic;
	extern uint cachecolormask;
	extern int softpowerup_ok;
	extern int cachewrback;
	unsigned int cpuctrl1tmp;
	extern int _triton_use_invall;

	mc_rev_level = 0;

	use_sableio = is_enabled(arg_sableio);
	_triton_use_invall = is_enabled(arg_triton_invall);

	/* If kdebug has not been set, dbgmon hasn't been loaded
	 * and we should turn kdebug off.
	 */
	if (kdebug == -1) 
	    kdebug = 0;
	
	if (! use_sableio) {
		/* set initial interrupt vectors */
		*K1_LIO_0_MASK_ADDR = 0;
		*K1_LIO_1_MASK_ADDR = 0;
#if 0			/* XXX - no one on lcl3 yet */
		setlclvector(VECTOR_LCL3, lcl3_intr, 0);
#endif
	}

	/* set interrupt DMA burst and delay values */
	i_dma_burst = dma_burst;
	i_dma_delay = dma_delay;

	init_sysid();

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
#ifdef R4000PC
	if (private.p_scachesize == 0)
		cachecolormask = (pdcache_size / NBPP) - 1;
#endif
#ifdef R4600
	if (two_set_pcaches)
		cachecolormask = (two_set_pcaches / NBPP) - 1;
#endif
#ifdef _VCE_AVOIDANCE
	if (private.p_scachesize == 0)
		vce_avoidance = 1;
#ifdef R4600SC
	if (two_set_pcaches)
		vce_avoidance = 1;
#endif
#endif
#ifdef R4600
	/* 
	 * On the R4600, be sure the wait sequence is on a single cache line 
	 */
	if (two_set_pcaches) {
		extern int wait_for_interrupt_fix_loc[];
		int	i;

		if ((((int) &wait_for_interrupt_fix_loc[0]) & 0x1f) >= 0x18) {
			for (i = 6; i >= 0; i--)
				wait_for_interrupt_fix_loc[i + 2] = 
					wait_for_interrupt_fix_loc[i];
			wait_for_interrupt_fix_loc[0] =
				wait_for_interrupt_fix_loc[9];
			wait_for_interrupt_fix_loc[1] =
				wait_for_interrupt_fix_loc[9];
			__dcache_wb_inval((void *) &wait_for_interrupt_fix_loc[0],
					  9 * sizeof(int));
			__icache_inval((void *) &wait_for_interrupt_fix_loc[0],
					  9 * sizeof(int));

		}
	}
#endif

	/* If not stuned, do not automatically reboot on panic */
	if (reboot_on_panic == -1)
		reboot_on_panic = 0;

#ifdef notdef
	softpowerup_ok = 1;		/* for syssgi() */
	VdmaInit();
#endif
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
 * so that doesn't matter (IP6 version only needs 2 args).
 */
void
setlclvector(int level, void (*func)(int, struct eframe_s *), int arg)
{
	long s = splhintr();
	if ( level < 8 )
		if (func) {
		    lcl0vec_tbl[level+1].isr = func;
		    lcl0vec_tbl[level+1].arg = arg;
		    if (! use_sableio)
			    *K1_LIO_0_MASK_ADDR |= 1 << level;
		} else {
		    lcl0vec_tbl[level+1].isr = stray0;
		    lcl0vec_tbl[level+1].arg = 0;
		    if (! use_sableio)
			    *K1_LIO_0_MASK_ADDR &= ~(1 << level);
		}
	else if ( level < 16 ) {
		if (func) {
		    lcl1vec_tbl[(level-8)+1].isr = func;
		    lcl1vec_tbl[(level-8)+1].arg = arg;
		    if (! use_sableio)
			    *K1_LIO_1_MASK_ADDR |= 1 << (level-8);
		} else {
		    lcl1vec_tbl[(level-8)+1].isr = stray1;
		    lcl1vec_tbl[(level-8)+1].arg = 0;
		    if (! use_sableio)
			    *K1_LIO_1_MASK_ADDR &= ~(1<< (level-8));
		}
	}
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
void
intr(struct eframe_s *ep, uint code, uint sr, uint cause)
{
	int svcd = 0;
	int s;
	extern unsigned long get_r4k_counter();
	volatile unsigned long base_counter = get_r4k_counter(); 

	while (1) {
		register struct intvec_s *iv;

		if ((cause_ip5_count && (sr & CAUSE_IP5)) ||
		    (cause_ip6_count && (sr & CAUSE_IP6))) {
			s = spl7();
			if (! (cause & CAUSE_IP5) &&
			    cause_ip5_count &&
			    (sr & CAUSE_IP5)) {
				cause_ip5_count--;
				cause |= CAUSE_IP5;
			}
			if (! (cause & CAUSE_IP6) &&
			    cause_ip6_count &&
			    (sr & CAUSE_IP6)) {
				cause_ip6_count--;
				cause |= CAUSE_IP6;
			}
			if (! (cause & (CAUSE_IP5 | CAUSE_IP6)))
				splx (s);
		}

		cause = (cause & sr) & SR_IMASK;
		if (! cause)
			break;

		iv = c0vec_tbl+FFINTR((cause >> CAUSE_IPSHIFT));
		COUNT_INTR;
		splx(iv->msk);
		(*iv->isr)(ep);
		svcd  += 1;
		cause &= ~(iv->bit << CAUSE_IPSHIFT);

		cause |= getcause();
	}

	SYSINFO.intr_svcd += svcd;
	/*
	 * try to trigger a later interrupt, if there are pending
	 * pseudo-interrupts we cannot deliver (due to their being
	 * masked), in case they are masked due to a raised spl
	 * at process level.
	 */
	if (cause_ip5_count || cause_ip6_count)
		setsoftclock(); 
}

/*
 * lcl?intr - handle local interrupts 0 and 1
 */
static void
lcl0_intr(struct eframe_s *ep)
{
	register struct lclvec_s *liv;
	char lcl_cause;
	int svcd = 0;

	if (use_sableio)
		lcl_cause = (1 << VECTOR_DUART);
	else
		lcl_cause = *K1_LIO_0_ISR_ADDR & *K1_LIO_0_MASK_ADDR;

	if (lcl_cause) do {
		liv = lcl0vec_tbl+FFINTR(lcl_cause);
		COUNT_L0_INTR;
		(*liv->isr)(liv->arg,ep);
		svcd += 1;
		lcl_cause &= ~liv->bit;
		} while (lcl_cause);
	else	{
		/*
		 * INT2 fails to latch fifo full in the local interrupt
		 * status register, but it does latch it internally and
		 * needs to be cleared by masking it.  So if it looks like
		 * we've received a phantom local 0 interrupt, handle it
		 * as a fifo full.
		 */
#ifdef notdef
		liv = &lcl0vec_tbl[VECTOR_GIO0+1];
		(liv->isr)(liv->arg,ep);
#endif
		return;
	}

	SYSINFO.intr_svcd += svcd-1; /* account for extra one counted in intr */
}

static void
lcl1_intr(struct eframe_s *ep)
{
	char lcl_cause;
	int svcd = 0;

	if (use_sableio) 
		lcl_cause = (1 << 0); /* CRIME interrupt */
	else
		lcl_cause = *K1_LIO_1_ISR_ADDR & *K1_LIO_1_MASK_ADDR;

	if (lcl_cause) do {
		register struct lclvec_s *liv = lcl1vec_tbl+FFINTR(lcl_cause);
		COUNT_L1_INTR;
		(*liv->isr)(liv->arg,ep);
		svcd += 1;
		lcl_cause &= ~liv->bit;
	} while (lcl_cause);

	SYSINFO.intr_svcd += svcd-1; /* account for extra one counted in intr */
}


static printstray0;

/*ARGSUSED*/
static void
stray0(int adap, struct eframe_s *ep)
{
	static stray0ints;

	if(printstray0)	/* so we can turn on with symmon or dbx */
		cmn_err(CE_WARN,"stray local 0 interrupt #%d\n",adap);
	stray0ints++;	/* for crash dumps, etc. */
}

/*ARGSUSED*/
static void
stray1(int adap, struct eframe_s *ep)
{
	cmn_err(CE_WARN,"stray local 1 interrupt #%d\n",adap);
}

/*ARGSUSED*/
static void
stray2(int adap, struct eframe_s *ep)
{
	cmn_err(CE_WARN,"stray local 2 interrupt #%d\n",adap);
}


/* can be called via a timeout from powerintr(), direct from prom_reboot
 * in arcs.c, or from mdboot() as a result of a uadmin call.
*/
void
cpu_powerdown()
{
}


/* While looping, check if the power button is being pressed. */
void
ip22_waitforreset()
{
}

/*ARGSUSED*/
static void
powerintr(int arg, struct eframe_s *ep)
{
}

/*  Flash LED from green to off to indicate system powerdown is in
 * progress.
 */
static void
flash_led(int on)
{
	set_leds(on);
	timeout(flash_led,(void *)(on ? 0 : 3),HZ/4);
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
   int print_prio = print_help ? CE_ALERT : CE_CONT;
   cmn_err(print_prio, "Memory Parity Error in SIMM\n");
}

extern void fix_bad_parity(volatile uint *);
extern char *dma_par_msg;

/*
 * dobuserre - Common IP22 bus error handling code
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

#define CPU_ERRMASK	0x3f00

uint hpc3_ext_io, hpc3_bus_err_stat;
uint cpu_err_stat, gio_err_stat;
uint cpu_par_adr, gio_par_adr;
static uint bad_data_hi, bad_data_lo;

static int
dobuserr_common(struct eframe_s *ep,
		inst_t *epc,
		uint flag,
		uint is_exception)
{
	int cpu_buserr = 0;
	int cpu_memerr = 0;
	int fatal_error = 0;
#ifdef R4600SC
	int r4600sc_scache_disabled = 1;

	if (two_set_pcaches && private.p_scachesize)
		r4600sc_scache_disabled = _r4600sc_disable_scache();
#endif
	
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
			cmn_err(CE_PANIC,"Fatal parity error\n");
			
	    	ASSERT(eccf_addr);		/* perr_save_info left this */

	    	if (ecc_exception_recovery(ep)) {
#ifdef R4600SC
			if (!r4600sc_scache_disabled)
				_r4600sc_enable_scache();
#endif
		    	return 1;
	        }
		
	    } else if (dwong_patch(ep)) {
#ifdef R4600SC
			if (!r4600sc_scache_disabled)
				_r4600sc_enable_scache();
#endif
			return 1;	/* survived it */
	    }
	}
#endif /* _MEM_PARITY_WAR */

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
	else if (cpu_err_stat & CPU_ERRMASK)
		fatal_error = 1;

	/* print bank, byte, and SIMM number if it's a parity error
	 */
#ifdef _MEM_PARITY_WAR
	if (cpu_memerr && !ecc_perr_is_forced()) {
#else
	if (cpu_memerr) {
#endif
		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_ALERT,
"Process %d [%s] sent SIGBUS due to Memory Error at Physical Address 0x%x\n",
				current_pid(), get_current_name(),
				cpu_par_adr);
		} else
			cmn_err(CE_PANIC,
"IRIX Killed due to Memory Error at Physical Address 0x%x PC:0x%x ep:0x%x\n",
				cpu_par_adr, epc, ep); 
	}

#ifdef _MEM_PARITY_WAR
	if (cpu_memerr && ecc_perr_is_forced()) {
		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err(CE_ALERT,
"Process %d [%s] sent SIGBUS due to uncorrectable cache error\n",
				current_pid(), get_current_name());
		} else
			cmn_err(CE_PANIC,
"IRIX Killed due to uncorrectable cache error\n");
	}
#endif

	if (fatal_error)
		cmn_err(CE_PANIC,
"IRIX Killed due to internal Error\n\tat PC:0x%x ep:0x%x\n", 
			epc, ep);

#ifdef R4600SC
	if (!r4600sc_scache_disabled)
		_r4600sc_enable_scache();
#endif
	return(0);
}

#ifndef _MEM_PARITY_WAR
#define	CPU_RD_PAR_ERR	(CPU_ERR_STAT_RD | CPU_ERR_STAT_PAR)
ulong kv_initial_from;
ulong kv_initial_to;
ulong initial_count;
#endif /* _MEM_PARITY_WAR */

/*
 * dobuserr - handle bus error interrupt
 *
 * flag:  0 = kernel; 1 = kernel - no fault; 2 = user
 *
 * Save error register information in globals.  Clear
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
		k1, epc, cause, sp);
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
        union rev_id ri;

	ri.ri_uint = private.p_fputype_word = get_fpc_irr();
        /*
         * Hack for the RM5271 processor. We simply mimic the
         * R5000 since it's interface is identical.
         */
        if (ri.ri_imp == C0_IMP_RM5271) {
                private.p_fputype_word &= ~C0_IMPMASK;
                private.p_fputype_word |= (C0_IMP_R5000 << C0_IMPSHIFT);
        }

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

/* npage and flags aren't used for IP22, since it is only used for SCSI */
dmamap_t *
dma_mapalloc(int type, int adap, int npage, int flags)
{
	static dmamap_t smap[NSCUZZY];
	dmamap_t *dmamap;

	switch (type) {

	    case DMA_SCSI:
		if (adap >= NSCUZZY)
			return((dmamap_t *)-1);
		smap[adap].dma_type = DMA_SCSI;
		smap[adap].dma_adap = adap;
		return &smap[adap];

	    case DMA_EISA:
		if (adap != 0)
			return((dmamap_t *)-1);
		dmamap = (dmamap_t *) kern_calloc(1, sizeof(dmamap_t));

		dmamap->dma_type = DMA_EISA;
		dmamap->dma_adap = 0;
		dmamap->dma_size = npage;
		return (dmamap);

	    default:
		return((dmamap_t *)-1);
	}
}

/*
 *	Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *	Returns the actual number of bytes mapped.
 */
dma_map(dmamap_t *map, caddr_t addr, int len)
{
}

#ifdef notdef
/* avoid overhead of testing for KDM each time through; this is
	the second part of kvtophys() */
#define KNOTDM_TO_PHYS(X) ((pnum((X) - K2SEG) < (uint)v.v_syssegsz) ? \
	(uint)pdetophys((pde_t*)kvtokptbl(X)) + poff(X) : (uint)(X))

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
 */
dma_mapbp(dmamap_t *map, buf_t *bp, int len)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	register struct pfdat *pfd = NULL;
	register uint bytes, npages;
	register unsigned *cbp, *bcnt;
	register int		xoff;	/* bytes already xfered */

	/* for wd93_go */
	map->dma_index = 0;

	/* setup descriptor pointers */
	cbp = (uint *)(map->dma_virtaddr + scp->d_cbpoff);
	bcnt = (uint *)(map->dma_virtaddr + scp->d_cntoff);

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
		
	xoff = poff(xoff);
	npages = btoc(len + xoff);
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
		*cbp = pfdattophys(pfd) + xoff;
		*bcnt = MIN(bytes, NBPC - xoff);
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		bytes -= NBPC - xoff;
		xoff = 0;
	}

	/* set EOX flag and zero BCNT on hpc3 for 25mhz workaround */
	if (scp->d_reset == SCSI_RESET) {
		*bcnt = HPC3_EOX_VALUE;
	} else {
		/* go back and set EOX on last desc for HPC1 */
		cbp -= sizeof (scdescr_t) / sizeof (uint);
		*cbp |= HPC1_EOX_VALUE;
	}

	/*
	 * Need a final descriptor with byte cnt of zero and EOX flag set
	 * for the HPC3 to work right.  Should be harmless on others...
	 */

	/* flush descriptors back to memory so HPC gets right data */
	__dcache_wb_inval((void *)map->dma_virtaddr,
		(dmavaddr_t)(bcnt + 1) - map->dma_virtaddr);

	return len;
}
#endif

int wd93hpc1highmemok = 0;

/*
 * This is an init routine for the GIO SCSI expansion board for INDY.
 */
static
wd93_hpc1init(int adap)
{

}	


int    wd95cnt;

void
giogfx_intr(int unused, struct eframe_s *ep)
{
}

void
setgiogfxvec(const uint number, void (*isr)(int, struct eframe_s *), int arg)
{
}

/*	This is a hack to make the Western Digital chip stop
 *	interrupting before we turn interrupts on.
 *	ALL IP22's have the 93B chip, and the fifo with the
 *	burstmode dma, so just set the variable.
 */
wd93_init()
{

}	

/*
 * Reset the SCSI bus. Interrupt comes back after this one.
 * this also is tied to the reset pin of the SCSI chip.
 */
void
wd93_resetbus(int adap)
{

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
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{

}


#ifdef notdef
/*
 * Allocate the per-device DMA descriptor list.
 * cmap is the dmamap returned from dma_mapalloc.
 */
dmamap_t *
scsi_getchanmap(dmamap_t *cmap, int npages)
{
	register scdescr_t *scp, *pscp;
	register dmamap_t *map;
	register int i;

	if (!cmap)
		return cmap;	/* paranoia */
	if (npages == 0)
		npages = v.v_maxdmasz;

	/*
	 * An individual descriptor can't cross a page boundary.
	 * This code is predicated on 16-byte descriptors and 128 byte
	 * cachelines.  We should never have a page descriptor that
	 * crosses a page boundary.
	 */
	map = (dmamap_t *) kern_malloc(sizeof(*map));
	i = (1 + npages) * sizeof(scdescr_t);
	map->dma_virtaddr = (uint) kmem_alloc(i, VM_CACHEALIGN | VM_PHYSCONTIG);

	/* so we don't have to calculate each time in dma_map */
	map->dma_addr = kvtophys((void *)map->dma_virtaddr);
	map->dma_adap = cmap->dma_adap;
	map->dma_type = DMA_SCSI;
	map->dma_size = npages;

	/* fill in the next-buffer values now, we never change them */
	scp = (scdescr_t *)map->dma_virtaddr;
	pscp = (scdescr_t *)map->dma_addr + 1;
	for (i = 0; i < npages; ++i, ++scp, ++pscp) {
		scp->nbp = (uint)pscp;
	}

	return map;
}

dmamap_t *
wd93_getchanmap(register dmamap_t *cmap)
{
	return scsi_getchanmap(cmap, NSCSI_DMA_PGS);
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
int
scsidma_flush(dmamap_t *map, int readflag)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned fcnt;

	if (readflag) {
		fcnt = 512;	/* normally only 2 or 3 times */
		*scp->d_ctrl |= scp->d_flush;
		while((*scp->d_ctrl & scp->d_busy) && --fcnt)
			;
		if(fcnt == 0)
			cmn_err(CE_PANIC,
			   "SCSI DMA in progress bit never cleared\n");
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		 * disconnects without transferring data, and HPC already
		 * grabbed the first 64 bytes into it's fifo; otherwise gets
		 * the first 64 bytes twice, since it 'notices' that curbp was
		 * reset, but not that is the same.
		 */
		*scp->d_ctrl = 0;
	return 0;
}

int
wd93dma_flush(dmamap_t *map, int readflag, int xferd)
{
	scuzzy_t *scp = &scuzzy[map->dma_adap];
	unsigned *bcnt, *cbp, index, fcnt;

	if(readflag && xferd) {
		fcnt = 512;	/* normally only 2 or 3 times */
		*scp->d_ctrl |= scp->d_flush;
		while((*scp->d_ctrl & scp->d_busy) && --fcnt)
			;
		if(fcnt == 0)
			cmn_err(CE_PANIC,
			   "SCSI DMA in progress bit never cleared\n");
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
	cbp = (uint *)(map->dma_virtaddr + scp->d_cbpoff + fcnt);
	bcnt = (uint *)(map->dma_virtaddr + scp->d_cntoff + fcnt);

	/* handle possible partial page in first descriptor */
	fcnt = *bcnt & (NBPP | (NBPP - 1));
	if (xferd >= fcnt) {
		xferd -= fcnt;
		cbp += sizeof (scdescr_t) / sizeof (uint);
		bcnt += sizeof (scdescr_t) / sizeof (uint);
		++index;
	}
	if (xferd) {
		/* skip whole descriptors */
		fcnt = xferd / NBPP;
		index += fcnt;
		fcnt *= sizeof (scdescr_t) / sizeof (uint);
		cbp += fcnt;
		bcnt += fcnt;

		/* create first partial page descriptor */
		*bcnt -= poff(xferd);
		*cbp += poff(xferd);

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

#endif


/*
 * Routines for LED handling
 */

/*
 * Initialize led counter and value
 */
void
reset_leds(void)
{
}


/*
 * Implementation of syssgi(SGI_SETLED,a1)
 *
 */
void
sys_setled(int a1)
{
}

void
set_leds(int pattern)
{
}

void bump_leds(void) {}

/*
 * sendintr - send an interrupt to another CPU; fatal error on IP22.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
	panic("sendintr");
}


/* START of Time Of Day clock chip stuff (to "END of Time Of Day clock chip" */

static void setbadclock(), clearbadclock();
static int isbadclock();
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

	s = splprof();
	spsema(timelock);

	todr = rtodc(); /* returns seconds from 1970 00:00:00 GMT */
	settime(todr,0);

	if (miniroot) {	/* no date checking if running from miniroot */
		svsema(timelock);
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


	svsema(timelock);
	splx(s);
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

#define SABLE_NVRAM_BASE 0x1d000000

#define	NVRAM_SECONDS		0
#define	NVRAM_MINUTES		2
#define NVRAM_HOURS		4
#define NVRAM_DAY_OF_WEEK	6
#define NVRAM_DAY_OF_MONTH	7
#define NVRAM_MONTH		8
#define NVRAM_YEAR		9
#define NVRAM_REGD		13

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

int
rtodc()
{
	volatile unsigned int *nvram = ((volatile unsigned int *) 
					 PHYS_TO_K1(SABLE_NVRAM_BASE));
	uint month, day, year, hours, mins, secs;
	register int i;

	if (! use_sableio)
		return *(int *)(MHSIM_GETTIMEOFDAY_ADDR);
	
	secs = nvram[NVRAM_SECONDS];
	mins = nvram[NVRAM_MINUTES];
	hours = nvram[NVRAM_HOURS];
	day = nvram[NVRAM_DAY_OF_MONTH];
	month = nvram[NVRAM_MONTH];
	year = nvram[NVRAM_YEAR];
	if (year == 72)
		year = (95 - 40); /* no real year in sable */
	year += 1940;
	if (month == 0)
		month = 1;
	if (day == 0)
		day = 1;

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
}

static void
setbadclock()
{
}

static void
clearbadclock()
{
}

static int
isbadclock()
{
   return 0;
}


/* END of Time Of Day clock chip stuff */

/*	called from  clock() periodically to see if system time is 
	drifting from the real time clock chip. Seems to take about
	about 30 seconds to make a 1 second adjustment, judging
	by my debug printfs.  Only do if different by 2 seconds or more.
	1 second difference often occurs due to slight differences between
	kernel and chip, 2 to play safe.
	Empty routine if not IP6 because other machines rtdoc() routines do
	not return time since the epoch; also the TOD chips may not be as accurate.
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

int
set_nvram(char *name, char *value)
{
   return 0;
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

int
board_rev(void)
{
	return 0;
}

void
add_ioboard(void)	/* should now be add_ioboards() */
{
}

void
add_cpuboard(void)
{
	add_to_inventory(INV_SERIAL, INV_ONBOARD, 0, 0, 2);

	add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(),0,
						private.p_cputype_word);
	add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(),0,
						private.p_fputype_word);
	/* for type INV_CPUBOARD, ctrl is CPU freq, unit = -1 */
	/* Double the hinv cpu frequency for R4000s */
	add_to_inventory(INV_PROCESSOR,INV_CPUBOARD, 2 * private.cpufreq,
							-1,INV_IPMHSIMBOARD);
}

int
cpuboard(void)
{
	return INV_IPMHSIMBOARD;
}

/*
 * On IP22, unix overlays the prom's bss area.  Therefore, only
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
	 * cn_is_inited() implies that PROM bss has been wiped out
	 */
	if (cn_is_inited())
		cmn_err(CE_CONT,fmt,ARGS);
}

int
readadapters(uint_t bustype)
{
	switch (bustype) {
		
	case ADAP_SCSI:
	        return 1;
		break;

	case ADAP_LOCAL:

		return(1);
		break;

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

	return(piomap);
}

caddr_t 
pio_mapaddr (piomap_t* piomap, iopaddr_t addr)
{
	switch(piomap->pio_bus){

	default: 
		return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
		break;
	}
}

void
pio_mapfree (piomap_t* piomap)
{
	if (piomap)
		kern_free(piomap);
}

prom_eaddr(char *buf)
{
	arcs_eaddr(buf);
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
/*	return PHYS_TO_K1(RPSS_CTR);*/
	return 0;
}
__int64_t
absolute_rtc_current_time()
{
	return(0);
}
/* XXX -- stubs for ec2/duart until ec2 fixed ported to IP22 */
int enet_collision;
void enet_carrier_on() {}
void reset_enet_carrier() {}

#if DEBUG

#include "sys/immu.h"

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
   return (cpu == 0);
}

/*
 * This is just a stub on an IPMHSIM.
 */
void
debug_stop_all_cpus(void *stoplist)
{
	return;
}


#define MHSIM_PROM_PUTC(c) \
{                            \
   *(int *)(MHSIM_DUART_SYNC_WR_ADDR) = (c); \
}

long
arcs_write(unsigned long fd, void *buf, unsigned long n, unsigned long *cnt)
{
   char c = *(char *) buf;

   MHSIM_PROM_PUTC(c);
   return 0;
}


#ifdef notdef
char
*arcs_getenv(char *name)
{
   return(__TV->GetEnvironmentVariable(name));
}
#endif

extern char eaddr[];

void
*arcs_eaddr(unsigned char *cp)
{
   bcopy (eaddr, cp, 6);
}

void
arcs_setuproot(void)
{
   /*
    * Hardcode the root disk 
    */
   strcpy(arg_root, "dks0d1s0");
}

/*ARGSUSED*/
void
prom_rexec(struct promexec_args *uparg)
{
   panic("prom_rexec called\n");
}

void
cpu_waitforreset(void)
{
	while (1)
	;
}
