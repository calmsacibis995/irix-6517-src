/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.484 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <ksys/cacheops.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/edt.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/invent.h>
#include <sys/kopt.h>
#include <sys/map.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sbd.h>
#include <sys/schedctl.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <ksys/sthread.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/vmereg.h>
#include <sys/scsi.h>
#include <sys/mload.h>
#include <sys/atomic_ops.h>
#include <sys/major.h>
#include <sys/nodepda.h>
#include <string.h>
#include <values.h>
#include <limits.h>
#include <sys/lpage.h>
#include <sys/numa.h>
#include <ksys/exception.h>
#include <ksys/hwg.h>
#include <sys/space.h>
#include <sys/rt.h>
#include <sys/sched.h>
#include <sys/iograph.h>
#include <sys/mapped_kernel.h>
#if CELL
#include <ksys/cell.h>
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#endif
#include <sys/idbgentry.h>
#ifdef SN
#include <sys/SN/error.h>
#endif
#include <sys/tile.h>
#ifdef DEBUG_BUFTRACE
#include <sys/ktrace.h>
#endif
#if defined(EVEREST) && defined(ULI)
#include <ksys/xthread.h>
#endif

#if defined(_MEM_PARITY_WAR) || IP20 || IP22
extern pfn_t allocate_ecc_info(pfn_t);
#endif /* _MEM_PARITY_WAR */
#ifndef SCACHE_SET_ASSOC
#define	SCACHE_SET_ASSOC	1
#endif

/*
 * Routine that initializes the useful portion of 
 * sysreg structure. This part is needed to make 
 * kvpalloc/kvpfree sequence work.
 */
extern void sysreg_earlyinit(void);

int	icache_size;	/* Size of icache that requires flushing (scache_size
			 * on IP17, primary icache size on others).
			 */
int	dcache_size;	/* Size of dcache that requires flushing (scache_size
			 * on IP17, pdcache_size on others).
			 */
int	picache_size;		/* primary icache */
int	pdcache_size;		/* primary dcache */
int	boot_sdcache_size;	/* second level data cache (boot cpu) */
int	boot_sicache_size;	/* second level instruction cache (boot cpu) */
int	boot_sidcache_size;	/* second level unified cache (boot cpu) */
int	scache_linemask;	/* second level cache line size mask */
int	scache_present;		/* true if secondary cache is present */
pfn_t	max_pageord;		/* maximum page ordinal number */
pfd_t	*ordinal_ptr;		/* needed for ordinal number calculation */

uint	cachecolormask = CACHECOLORMASK;
				/* for R4000 cache coloring algorithms.
				 * The mask is for use with vpn's and pfn's,
				 * not full vaddrs.
				 */
uint	cachecolorsize = CACHECOLORSIZE;
				/* actual number of cache colors;
				 * set to (cachecolormask + 1)
				 */

#ifdef R4600
int	two_set_pcaches = 0;	/* xor bit for two-set cache selection */
#endif
#ifdef _VCE_AVOIDANCE
int	vce_avoidance = 0;	/* boolean; set if VCE's must be avoided,
				 * as on R4000PC's
				 */
pfde_t *pfdat_extension = NULL;
#endif	

#ifdef MH_R10000_SPECULATION_WAR
pfd_t   *pfd_lowmem = NULL;
pfn_t   pfn_lowmem = 0;
int	low_k2mem = 0;
extern	pgno_t seg1_maxmem;

extern	void *extk0_mem_alloc(int, char *);

tile_data_t *tile_lowmem = NULL;
#endif /* MH_R10000_SPECULATION_WAR */

#if DEBUG
short dbgconpoll = 1;
#else
short dbgconpoll = 0;		/* for .data to fit with kdebug in 1 word */
#endif /* DEBUG */

short	kdebug = -1;		/* if set then we have a dbgmon under us */
int	doass = 1;		/* if cleared then ignore ALL ASSERTs */
int	diskless;
int	splockmeter;		/* is spinlock metering enabled? */

action_t *actionlist;
struct vme_ivec *vme_ivec;	/* for allocating vectors */
int *vmevec_alloc;		/* for allocating vectors */

semahist_t	*semahist;		/* array of history records */
int ncache_actions;
int conbufsz, putbufsz, errbufsz;

/*
 * File system cache tuneable structures.
 */
extern buf_t		*global_buf_table;
extern hbuf_t		*global_buf_hash;
extern dhbuf_t		*global_dev_hash;

extern char *conbuf;
extern char *putbuf;
extern char *errbuf;
extern struct tune tune;
extern struct var v;

extern int conbuf_cpusz, putbuf_cpusz, errbuf_cpusz;
extern int conbuf_maxsz, putbuf_maxsz, errbuf_maxsz;
extern int nactions;
extern long flckrec;			
extern uint histmax;			/* maximum entries before wrap */

extern int nbuf;
extern int nproc;
extern int narsess;
extern int maxup;
extern int syssegsz;
extern int maxpmem;
extern int maxdmasz;
extern int maxpglst;
extern int ndquot;
extern int gpgslo;
extern int gpgshi;
extern int gpgsmsk;
extern int maxsc;
extern int maxfc;
extern int maxdc;
extern int bdflushr;
extern int minarmem;
extern int minasmem;
extern int maxlkmem;
extern int tlbdrop;
extern int rsshogfrac;
extern int rsshogslop;
extern int dwcluster;
extern int autoup;

struct sysbitmap sptbitmap;	/* system vaddr map */
extern char *	kvswap_map;	/* swappable sysseg pages */

/*
 * loader can't always correctly set etext, etc so we have some variables
 * that everyone uses and mlsetup sets them appropriately
 * Helper utilities like setphdr_elf set these for some configurations
 */
extern char etext[], end[], edata[];
char *ketext = etext, *kedata = edata, *kend = end;

#ifdef R4000
int colormap_size;
int clrsz;
#else
#define clrsz 0		/* eliminate need for more ifdefs later */
#endif

#ifdef R10000
int colormap_size; 	/* Temp. declaration */
#endif

#if R10000 && R4000
int ntlbentries = MAX_NTLBENTRIES;
int nrandomentries = MAX_NRANDOMENTRIES;
#endif	/* R10000 && R4000 */

/* the number of pages that are trully for kvalloc */
int sptsz;

static __psunsigned_t t0init(void);
static void tlbinit(pfn_t);
static void init_kptbl(pde_t *, pgno_t);
static void globaldatast_init(void);
static pfn_t setup_lowmem(cnodeid_t, pfn_t, pfn_t);
static caddr_t mktables(cnodeid_t node, pfn_t);
static void tunetableinit(void);
static void initmaxpglst(pfn_t);
extern char *dksc_partition_to_canonical_special(char *, char *);

extern void	cn_init(int, int);
extern void	init_kheap(void);
extern void	config_cache(void);
extern int	idbg_setup(void);
extern void	timestamp_init(void);
extern void	reset_leds(void);
extern void	sinit(void);
extern void	vme_ivec_init(void);
extern void	arcs_setuproot(void);
extern pfn_t	initsplocks(pfn_t);
extern pfn_t	initmasterpda(pfn_t);
extern void	initpdas(void);
extern void	str_init_memsize(void);
extern pfn_t	szmem(pfn_t, pfn_t);
extern void	meminit(void);
extern void	node_meminit(cnodeid_t, pfn_t, pfn_t);
extern pfn_t 	init_ecc_sp(pfn_t);
extern void	dprintf(char *, ...);
extern void	customs_init(void);

#ifdef	NUMA_BASE
extern int	memsched_init(void);
#endif	/* NUMA_BASE */

/*
 * Initialize exception handling vectors
 */
extern int VEC_syscall(), VEC_cpfault(), VEC_trap(), VEC_int(), VEC_tlbmod();
extern int VEC_tlbmiss(), VEC_breakpoint(), VEC_unexp(), VEC_ibe(), VEC_dbe();
#ifdef	_R5000_CVT_WAR
extern int VEC_ii();
#endif	/* _R5000_CVT_WAR */

/* Making this const means it can be replicated on NUMA platforms */
#if R4000 || R10000
extern int VEC_fpe();
int  (* const causevec[32])() =
#else
int  (* const causevec[16])() =
#endif
{
	/*  0: EXC_INT */		VEC_int,
	/*  1: EXC_MOD */		VEC_tlbmod,
	/*  2: EXC_RMISS */		VEC_tlbmiss,
	/*  3: EXC_WMISS */		VEC_tlbmiss,
	/*  4: EXC_RADE */		VEC_trap,
	/*  5: EXC_WADE */		VEC_trap,
	/*  6: EXC_IBE */		VEC_ibe,
	/*  7: EXC_DBE */		VEC_dbe,
	/*  8: EXC_SYSCALL */	 	VEC_syscall,
	/*  9: EXC_BREAK */		VEC_breakpoint,
#ifdef	_R5000_CVT_WAR
	/* 10: EXC_II */		VEC_ii,
#else
	/* 10: EXC_II */		VEC_trap,
#endif	/* _R5000_CVT_WAR */
	/* 11: EXC_CPU */		VEC_cpfault,
	/* 12: EXC_OV */		VEC_trap,
#if !R4000 && !R10000
#if TFP
	/* 13: EXC_TRAP */		VEC_trap,
#else
	/* 13: undefined */		VEC_unexp,
#endif
	/* 14: undefined */		VEC_unexp,
	/* 15: undefined */		VEC_unexp,
#else /* !R4000 && !R10000 */
	/* 13: EXC_TRAP */		VEC_trap,
#ifdef R4000
	/* 14: EXC_VCEI */		VEC_trap,
#else
	/* 14: undefined */		VEC_unexp,
#endif /* R4000 */
	/* 15: EXC_FPE */		VEC_fpe,
	/* 16: undefined */		VEC_unexp,
	/* 17: undefined */		VEC_unexp,
	/* 18: undefined */		VEC_unexp,
	/* 19: undefined */		VEC_unexp,
	/* 20: undefined */		VEC_unexp,
	/* 21: undefined */		VEC_unexp,
	/* 22: undefined */		VEC_unexp,
	/* 23: EXC_WATCH */		VEC_trap,
	/* 24: undefined */		VEC_unexp,
	/* 25: undefined */		VEC_unexp,
	/* 26: undefined */		VEC_unexp,
	/* 27: undefined */		VEC_unexp,
	/* 28: undefined */		VEC_unexp,
	/* 29: undefined */		VEC_unexp,
	/* 30: undefined */		VEC_unexp,
#ifdef R4000
	/* 31: EXC_VCED */		VEC_trap,
#else
	/* 31: undefined */		VEC_unexp,
#endif /* R4000 */
#endif /* !R4000 && !R10000 */
};

extern int baerror(), cerror(), adderr(), uerror(), uaerror(), softfp_adderr(),
	reviderror(), softfp_insterr(), earlybaerror(), idbg_error(),
	fixade_error(), suerror(), bzerror();

int  (* const nofault_pc[NF_NENTRIES])() = {
	/* unused */		0,
	/* NF_BADADDR */	baerror,
	/* NF_COPYIO */		cerror,
	/* NF_ADDUPC */		adderr,
	/* NF_FSUMEM */		uerror,
	/* NF_BZERO */		bzerror,
	/* NF_SOFTFP */		softfp_adderr,
	/* NF_REVID */		reviderror,
	/* NF_SOFTFPI */	softfp_insterr,
	/* NF_EARLYBADADDR */	earlybaerror,
	/* NF_IDBG */		idbg_error,
	/* NF_FIXADE */		fixade_error,
	/* NF_SUERROR */	suerror,
	/* NF_BADVADDR */	uerror,
	/* NF_DUMPCOPY */	cerror
};


#ifdef NUMA_BASE
#define lowmem_alloc_nodematch(node, target_node)	\
		( ((node) == (target_node)) || \
		  ((target_node) >= numnodes && (node) == ((numnodes)-1)) )
#if 1	/* for now, keep everything on node 0 */
#define LOWMEM_ALLOC_NODE_KPTBL		0
#define LOWMEM_ALLOC_NODE_SYSBITMAP	0
#define LOWMEM_ALLOC_NODE_KVSWAP	0
#define LOWMEM_ALLOC_NODE_CONBUF	0
#define LOWMEM_ALLOC_NODE_ACTIONLIST	0
#define LOWMEM_ALLOC_NODE_VME		0
#define LOWMEM_ALLOC_NODE_SEMAHIST	0
#define LOWMEM_ALLOC_NODE_PGLST		0
#define LOWMEM_ALLOC_NODE_KRPF		0
#ifdef SN
#define LOWMEM_ALLOC_NODE_SNERRBUF	0
#endif	/* SN */
#else	
#define LOWMEM_ALLOC_NODE_KPTBL		1
#define LOWMEM_ALLOC_NODE_SYSBITMAP	2
#define LOWMEM_ALLOC_NODE_KVSWAP	3
#define LOWMEM_ALLOC_NODE_CONBUF	4
#define LOWMEM_ALLOC_NODE_ACTIONLIST	5
#define LOWMEM_ALLOC_NODE_VME		6
#define LOWMEM_ALLOC_NODE_SEMAHIST	7
#define LOWMEM_ALLOC_NODE_PGLST		8
#define LOWMEM_ALLOC_NODE_KRPF		9
#ifdef SN
#define LOWMEM_ALLOC_NODE_SNERRBUF	10
#endif	/* SN */
#endif
#else
#define lowmem_alloc_nodematch(node, target_node)	(1)
#endif

#ifdef LOWMEMDATA_DEBUG

#ifdef SN0
#define LOWMEMDATA_TABLESIZE	4096
#else
#define LOWMEMDATA_TABLESIZE	256
#endif
#define LOWMEMDATA_NAMELEN	16

int lowmemdata_index;

struct {
	char	name[LOWMEMDATA_NAMELEN];
	int	node;
	int	size;
	caddr_t	addr;
} lowmemdata[LOWMEMDATA_TABLESIZE];

void
lowmemdata_record(char *name, cnodeid_t node, int size, caddr_t addr)
{
	int i;

	if (lowmemdata_index >= LOWMEMDATA_TABLESIZE)
		return;
	
	/* copy the name, pad with spaces for nice printing */
	for (i=0; i<LOWMEMDATA_NAMELEN-1 && *(name+i); ++i)
		lowmemdata[lowmemdata_index].name[i] = *(name+i);
	for (   ; i<LOWMEMDATA_NAMELEN-1; ++i)
		lowmemdata[lowmemdata_index].name[i] = ' ';
	lowmemdata[lowmemdata_index].name[LOWMEMDATA_NAMELEN-1] = 0;

	/* copy the node, size and addr */
	lowmemdata[lowmemdata_index].node = (int) node;
	lowmemdata[lowmemdata_index].size = size;
	lowmemdata[lowmemdata_index].addr = addr;

	/* increment the index */
	++lowmemdata_index;
	if (lowmemdata_index == LOWMEMDATA_TABLESIZE-1) {
		lowmemdata_record("END_OF_TABLE", node, 0, 0);
	}

	return;
}

void
lowmemdata_print(cnodeid_t cnode)
{
	int ii;

	if (cnode < -1 || cnode > numnodes-1) {
		qprintf("  lowmemdata [cnode]\n");
		return;
	}
	qprintf("  Node\tName            0xSize\tAddress \n");
	for (ii=0; ii<lowmemdata_index; ++ii) {
		if (cnode == -1 || cnode == lowmemdata[ii].node) {
			qprintf("  %d\t%s\t%x\t%x\n",
				lowmemdata[ii].node,
				lowmemdata[ii].name,
				lowmemdata[ii].size,
				lowmemdata[ii].addr);
		}
	}
#ifdef DEBUG
	qprintf("Used %d of %d table slots.\n",
		lowmemdata_index, LOWMEMDATA_TABLESIZE);
#endif
}

#endif	/* LOWMEMDATA_DEBUG */



/* 
 * Routine called before main, to initialize various data structures.
 */

void otherprda(struct prda *);
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
extern redo_cells(int argc, __promptr_t *argv,
		  __promptr_t *environ);
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

/*
 * Very Early Startup Code.
 * We are on the interrupt stack and interrupts are off
 * On return we have already lowered the spl at least to splhi - but not lower
 */

__psunsigned_t
mlsetup(
	int argc,
	__promptr_t *argv,
	__promptr_t *environ)
{
	pfn_t fpage;	/* first free physical page */
	__psunsigned_t newsp;

#if defined(EVEREST) && defined(MULTIKERNEL)
#define	PDA0	EVMK_CELL_MASTER_CPUID(evmk_cellid)
#elif IP30
#define PDA0	getcpuid()		/* can boot on CPU 1 */
#else
#define	PDA0	0
#endif

#if !defined(SABLE) && !_SYSTEM_SIMULATION
	bzero(kedata, kend - kedata);	/* zero bss */
#endif
	config_cache();
#ifdef R10000
#ifdef R4000
	if (IS_R10000())
#endif /* R4000 */
#ifndef SABLE
#if defined (LOGICAL_CELLS)

	boot_sidcache_size = icache_size = dcache_size = getcachesz(getcpuid());
#else  /* !LOGICAL_CELLS */
	boot_sidcache_size = icache_size = dcache_size = getcachesz(0);
#endif /* !LOGICAL_CELLS */

#else
	boot_sidcache_size = icache_size = dcache_size = 0x100000;
#endif
#endif /* R10000 */

	/* first page phys mem */
	fpage = btoc(EARLY_MAPPED_KERN_RW_TO_PHYS((__psunsigned_t)kend));
	lowmemdata_record("mlsetup", 0, 0, (caddr_t)ctob(fpage));

#if !SABLE_RTL || R10000 || IP30
#if defined(EVEREST) && defined(MULTIKERNEL)

	/* for this cell we must set the correct master_procid */

	master_procid = EVMK_CELL_MASTER_CPUID(evmk_cellid);

	/* For MULTIKERNEL systems the slave cells already have the
	 * kernel variables parsed and replicated.
	 */
	if (evmk_cellid == evmk_golden_cellid)
#endif

	getargs(argc, argv, environ);	/* cache needed environment vars */

#ifdef IP32
	/* zero partial page to set ecc for any speculative references */
	bzero(kend, ctob(btoc(kend)) - (__psunsigned_t)kend);

	ecc_meminit(fpage, last_phys_pfn());
#endif /* IP32 */

	/*
	 * Initialize spin lock subsystem early on, since they may
	 * be desired in mlreset.
	 */
	fpage = initsplocks(fpage);

	/*
	 * This hack is to get the pda ready for splockmeter
	 * Also need to wire pda for use by splock reference to CELSHADOW.
	 */
	fpage = pagecoloralign(fpage, btoct(PDAPAGE));
	pdaindr[PDA0].pda = (pda_t *) PHYS_TO_K0(ctob(fpage) + PDAOFFSET);
#if !defined(_SYSTEM_SIMULATION) && !defined(SABLE_RTL)
	/* zero out our portion of pda */
	bzero((caddr_t) pdaindr[PDA0].pda + sizeof(pdaindr[PDA0].pda->db),
	      PDASIZE - sizeof(pdaindr[PDA0].pda->db));
#endif  /* _SYSTEM_SIMULATION || SABLE_RTL */
#if defined(EVEREST) && defined(MULTIKERNEL)
	/* For slave cells we need to "hook" symmon's exception handler
	 * into the "new" pda page since we're moving away from the
	 * evml_temp_pda we've used until now.
	 */
	if ((evmk_cellid != evmk_golden_cellid) && (SPB->DebugBlock)) {
		pdaindr[PDA0].pda->common_excnorm =
			(inst_t*)((db_t*)SPB->DebugBlock)->db_excaddr;
	}
#endif /* EVEREST && MULTIKERNEL */
	wirepda(pdaindr[PDA0].pda);
	fpage++;
#endif

	/*
	 * Now that the pda is mapped, store the secondary cache size
	 * in it. It has been temporarily held in either boot_sidcache_size 
	 * or boot_sdcache_size and so either of these variables reflect the
	 * secondary cache size for the boot cpu only. This needs to
	 * go into the pda now because flush_cache() needs it. 
	 */
	if (boot_sidcache_size)
		private.p_scachesize = boot_sidcache_size;
	else if (boot_sdcache_size)
		private.p_scachesize = boot_sdcache_size;

#if R10000 && R4000
	if (IS_R10000()) {
		ntlbentries = R10K_NTLBENTRIES;
		nrandomentries = R10K_NRANDOMENTRIES;
	} else {
		ntlbentries = R4K_NTLBENTRIES;
		nrandomentries = R4K_NRANDOMENTRIES;
	}
#endif /* R10000 && R4000 */

	flush_cache();

	/*
	 * do certain machine dependent things that MUST before interrupts
	 * are ever enabled
	 */
#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
	redo_cells(argc, argv, environ);
#endif

	mlreset(0);

	cachecolorsize = cachecolormask + 1;
	scache_present = (private.p_scachesize > 0);

#ifdef _VCE_AVOIDANCE
	if (! scache_present)
		vce_avoidance = 1;
#endif

#if defined(_MEM_PARITY_WAR) || IP20 || IP22
	fpage = allocate_ecc_info(fpage);
#endif
	
	/*
	 * On the EVEREST machines, the everror structure is extended in 
	 * low memory. Also, the fru analyzer gets called in the 
	 * cache error exception path. To give sufficient space for the 
	 * exception stack and to prevent it from growing into the pda,
	 * we allocate a separate page for the cache exception stack.
	 */
#if defined (IP19)
#if defined(MULTIKERNEL)
	if (evmk_cellid == evmk_golden_cellid)
#endif
		fpage = init_ecc_sp(fpage);
#endif

	/*
	 * wirepda, plug in exc handlers vecs, set interrupt stack
	 * kernel starts handling exceptions in initmasterpda
	 * Locks are initialized here
	 * This is the first time interrupts are actually turned on (splhi)
	 */
	/* by now we should know what kind of hardware timer is available */
	timestamp_init();

	fpage = initmasterpda(fpage);	/* also calls _hook_exceptions */

	/* must be done after _hook_exceptions in case of brkpt */
	idbg_setup();

	reset_leds();

	/*
	 * Set up minimal MP state.
	 */
	sinit();		/* initialize first locks and sems */
	stopclocks();

	tlbinit(fpage);		/* initialize tlb, physmem, etc */

#ifdef SN
	errorste_reset();
#endif
	initpdas();
	/*
	 * NOTE: The call here to the Streams allocator for the master cpu only
	 * and is required since the size of physical memory is determined
	 * in tlbinit() which sets the global variable we require.
	 */
	str_init_memsize();
#if CELL
	cell_cpu_init();	/* init boot cell */
#endif /* CELL */

	newsp = t0init();

#if defined(EVEREST) & defined(MULTIKERNEL)
	if (evmk_cellid == evmk_golden_cellid)
#endif

	/* Early initialization of sysreg data structure */
	sysreg_earlyinit();

	/* init system console AFTER callouts */
	cn_init(0, 0);

#ifdef IP32
	/* enable ECC checking in CRIME */
	ecc_enable();
#endif /* IP32 */

	otherprda(NULL);	/* 422425 */
	/* at this point physical & kernel virtual is set up */
	coproc_find();

	return newsp;
}


/*
 * tlbinit:
 *	Initialize tlb, physmem, etc. The kernel now
 * has itself mapped virtual to physical from address 0x100000 to the end
 * end of bss.
 */
static void
tlbinit(pfn_t fpage)
{
	/* REFERENCED */
	pfn_t firstfree;			/* First free click. */
	cnodeid_t node;

#ifdef LOWMEMDATA_DEBUG
	idbg_addfunc("lowmemdata", lowmemdata_print);
#endif

	lowmemdata_record("tlbinit", 0, 0, (caddr_t)ctob(fpage));
	/*
	 * Invalidate entire tlb (including wired entries).
	 * except that hard wired entries (those below
	 *     TLBWIREDBASE) are set up by whomever owns them.
	 */
	flush_tlb(TLBFLUSH_NONPDA);

#if defined(EVEREST) && defined(MULTIKERNEL)
	{
		extern void evmk_setup_numcells(void);

		evmk_setup_numcells();
	}
#endif /* EVEREST && MULTIKERNEL */
	/*
	 * Use the physical memory size specified in the tune structure
	 * as an absolute value.  This means don't even THINK about
	 * touching memory above that (if, of course, it's a
	 * reasonable value ...).  We now allow specifying at boot time,
	 * primarily for people doing benchmarking for machines with
	 * different memory configurations.  It also allows you to recover
	 * from a machine configured with a bad MAXPMEM value.
	 */
	if (is_specified(arg_maxpmem))
		maxpmem = atoi(arg_maxpmem);

	if (maxpmem && maxpmem < (8*1024*1024)/NBPP)   
		maxpmem = 0; 		/* require at least 8 MB */

	/*
	 * Size memory
	 */
	physmem = szmem(fpage, maxpmem);
	
	/*
	 * This will be refined for non NUMA systems in setup_lowmem.
	 */
	max_pageord = physmem;

        /* 
	 * autoconfigure the tunable variables after physmem is set 
	 */
	tunetableinit();

	/*
	 * Set up memory parameters.
	 */
	maxclick = node_getmaxclick((cnodeid_t) 0);
	firstfree = fpage;

	ASSERT(firstfree < maxclick);

#ifdef R4000
	ASSERT(colormap_size);
	/*
	 * "clrsz" is number of bits in any particular color map.
	 * It must be rounded to a long-word size to accomodate
	 * long-oriented bit operations.
	 */
	clrsz = colormap_size / CACHECOLORSIZE;
	clrsz &= ~(BITSPERLONG - 1);
	colormap_size = clrsz * CACHECOLORSIZE;
	/*
	 * On Everest systems, syssegsz and colormap_size must be
	 * multiples of NBBY for efficient fault history checking
	 * Bitmap code makes more stringent requirements.
	 */
	ASSERT((colormap_size & ~(BITSPERLONG - 1)) == colormap_size);

	sptsz = syssegsz - colormap_size;
#else
	sptsz = syssegsz;
#endif

	/*
	 * Allocate static kernel data structures that are sized
	 * at boot time.  Each node can have static data in the
	 * lower portion of its memory.
	 */

#if !NUMA_BASE
	numnodes = maxnodes = 1;
	boot_cnodemask = CNODEMASK_FROM_NUMNODES(numnodes);
#endif

        /*
         * Node #0 is special: we've used 0-bound memory to load the
         * kernel text and static data.
         */

        node = 0;
        maxclick = node_getmaxclick(node);
        firstfree = setup_lowmem(node, firstfree, maxclick);

        node_meminit(node, firstfree, maxclick);        
        
        /*
         * Now we go for the rest of the nodes
         */

	for (node = 1; node < numnodes; node++) {
                firstfree = node_getfirstfree(node);
		maxclick = node_getmaxclick(node);
		firstfree = setup_lowmem(node, firstfree, maxclick);

		node_meminit(node, firstfree, maxclick);
	}

#ifdef SN0
	/*
	 * On SN0, kptbl can't be allocated in mktables (in setup_lowmem)
	 * because on large memory system it won't fit under the pfdats.
	 */
	{
	int kptbl_size;
	pgno_t kptbl_click;
	ASSERT(syssegsz > 0);

	kptbl_size = (syssegsz + btoc(MAPPED_KERN_SIZE)) * sizeof(pde_t);
	kptbl_click = contmemall_node(LOWMEM_ALLOC_NODE_KPTBL, btoc(kptbl_size),
				      1, VM_NOSLEEP | VM_DIRECT);
	if (kptbl_click == 0) {
		/* just in case one node is smaller */
		kptbl_click = contmemall(btoc(kptbl_size),
				      1, VM_NOSLEEP | VM_DIRECT);
		ASSERT(kptbl_click);
	}
	kptbl = (pde_t *) PHYS_TO_K0(ctob(kptbl_click));
	lowmemdata_record("kptbl", PFN_TO_CNODEID(kvatopfn(kptbl)), kptbl_size,
							(caddr_t)kptbl);
	init_kptbl(kptbl, syssegsz + btoc(MAPPED_KERN_SIZE));
	}
#endif

	/* Initialize global memory related data structures and locks.  */
	meminit();

	/* limit maxdma size to valid values, to save locking down
	 * all the pages, and avoiding the problem (mostly) where
	 * a process blocks 'forever' when the dma size approaches
	 * the max possible.
	 */
	if (maxdmasz > syssegsz)
		maxdmasz = syssegsz;
	if (maxdmasz > (maxmem+128))
		maxdmasz = maxmem+128;

#ifdef	NUMA_BASE
	memsched_init();
#endif
	/* init dynamic memory */
	init_kheap();

#if CELL
	/* init import/export mechanism */
	customs_init();
#endif

	/* Init tlb manager information */
	tlbinfo_init();
#if TFP
	icacheinfo_init();
#endif
	globaldatast_init();
	
}

static void
initsptmap(
	register struct bitmap *map,
	__psunsigned_t start,
	u_int size)
{
	init_bitlock(&map->m_lock, MAP_LOCK|MAP_URGENT,
		    "sptlock", METER_NO_SEQ);

	map->m_unit0 = start;
	map->m_lowbit = map->m_size = size;
	/*
	map->m_highbit = 0;
	*/
}

static void
init_kptbl(register pde_t *pde, pgno_t size)
{
	while (--size >= 0) {
		pg_setpgi(pde, mkpde(PG_G, 0));
		pde++;
	}
#if TFP
	/* used by kernel tlbmiss handler */
	set_kptbl((__psunsigned_t)kptbl); 
#endif
}

struct mem_cache {
	struct mem_cache *next;
	int	size;
};
static struct mem_cache *mcache;

/* ARGSUSED2 */
void *
low_mem_alloc(register int need, caddr_t *alternate, char *name)
{
	register struct mem_cache *cache, **cp;
	register caddr_t ret;

	/* need to always align to 64 bit boundaries, because some
	 * of the allocated data structs can have members that are 
	 * stored into as long long, so the base address needs to be aligned
	 * properly */
	need += sizeof(app64_ptr_t) - 1;
	need &= ~(sizeof(app64_ptr_t) - 1);

	cp = &mcache;
	while (cache = *cp) {
		if (cache->size >= need) {
			ret = (caddr_t)cache;
			cache->size -= need;
			if (cache->size >= sizeof(struct mem_cache)) {
				*cp = (struct mem_cache *)(ret + need);
				(*cp)->size = cache->size;
				(*cp)->next = cache->next;
			}
			else
				*cp = cache->next;

#if SABLE_RTL
			/* RTL simulators zero memory initially.  We only
			 * need to re-zero the portion of the buffer which
			 * was used as the queue-head.
			 */
			bzero(ret, sizeof(struct mem_cache));
#else
			/*
			 * If the cached memory was put on the mcache list
			 * before tlbinit was called, it may not have
			 * been zeroed, so do it here.  And first word
			 * has pointer to next cached piece of memory!
			 */
			bzero(ret, need);
#endif	/* !SABLE_RTL */
			lowmemdata_record(name, PFN_TO_CNODEID(kvatopfn(ret)),
						need, ret);
			return ret;
		}

		cp = &cache->next;
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000() && (ret = extk0_mem_alloc(need, name)))
		return ret;
#endif /* MH_R10000_SPECULATION_WAR */

	ret = *alternate;
	*alternate += need;

	lowmemdata_record(name, PFN_TO_CNODEID(kvatopfn(ret)), need, ret);
	return ret;
}


pfn_t
pagecoloralign(pfn_t pfn, __psunsigned_t vpn)
{
	struct mem_cache *cache;
	int	size = 0;

	while ((vpn & cachecolormask) != (pfn & cachecolormask)) {
		if (size++ == 0)
			cache = (struct mem_cache *)PHYS_TO_K0(ctob(pfn));
		pfn++;
	}

	if (size) {
		cache->next = mcache;
		mcache = cache;
		cache->size = ctob(size);
	}

	return pfn;
}

/* Initialize the maps used to allocate kernel virtual address space. */
/* ARGSUSED4 */
static void
init_sysbitmap(
	caddr_t *nb,		/* ptr to 'nextbyte' of memory to use */
	int size,		/* size of map in pages */
	struct sysbitmap *bm,	/* sysmap to initialize */
	pgno_t start,		/* starting page number */
	int csize)		/* size of any color maps */
{
	int m;
	register char *p;

	sv_init(&bm->spt_wait.wait, SV_DEFAULT, "sptwait");
	initnsema_mutex(&bm->sptmaplock, "sptmaps");

	initsptmap(&bm->stale_sptmap, start, size);
	initsptmap(&bm->temp_sptmap, start, size);
	initsptmap(&bm->aged_sptmap, start, size);
	initsptmap(&bm->sptmap, start, size);

	/*
	 * Allocate the maps contiguously just to make sure that
	 * (by some horrid luck) we don't get pages that collide
	 * in the second-level cache.
	 */
	m = (size + NBBY - 1) / NBBY;
	p = low_mem_alloc(m*4, nb, "sptmaps");

	bm->stale_sptmap.m_map = p, p += m;
	bm->temp_sptmap.m_map = p, p += m;
	bm->aged_sptmap.m_map = p, p += m;
	bm->sptmap.m_map = p;

	/* sptmap starts totally full, the others are of course empty */
	while (m--)
		*p++ = 0xff;
	/* setting lowbit to -1 implies use rotor array */
	bm->sptmap.m_lowbit = -1;
	bm->sptmap.m_highbit = bm->sptmap.m_count = size;

#ifdef R4000
	{ int nmaps;
	pgno_t st;
	struct bitmap *bp;

	ASSERT(SPT_MAXCOLORS >= cachecolorsize);
	bp = (struct bitmap *)low_mem_alloc(CACHECOLORSIZE*sizeof(struct bitmap)*4, nb, "m_color[...]");
	for (nmaps = 0; nmaps < CACHECOLORSIZE; nmaps++) {
		bm->stale_sptmap.m_color[nmaps] = bp++;
		bm->aged_sptmap.m_color[nmaps] = bp++;
		bm->temp_sptmap.m_color[nmaps] = bp++;
		bm->sptmap.m_color[nmaps] = bp++;
	}

	for (nmaps = 0, st = start + sptsz;
				nmaps < CACHECOLORSIZE;
				nmaps++, st++) {
		register struct bitmap *bitmap;

		m = (csize + NBBY - 1) / NBBY;
		p = low_mem_alloc(m * 4, nb, "m_map");

		bitmap = bm->stale_sptmap.m_color[nmaps];
		initsptmap(bitmap, st, csize);
		bitmap->m_map = p, p += m;

		bitmap = bm->aged_sptmap.m_color[nmaps];
		initsptmap(bitmap, st, csize);
		bitmap->m_map = p, p += m;

		bitmap = bm->temp_sptmap.m_color[nmaps];
		initsptmap(bitmap, st, csize);
		bitmap->m_map = p, p += m;

		bitmap = bm->sptmap.m_color[nmaps];
		initsptmap(bitmap, st, csize);
		bitmap->m_map = p;

		/* sptmap starts totally full, the others are of course empty */
		bitmap->m_lowbit = 0;
		bitmap->m_highbit = bitmap->m_count = csize;
		while (m--)
			*p++ = 0xff;
	}
	}
#endif
}

/*
 * Allocate static tables and perform other boot time memory initialization
 * for the given node's memory.
 */

/*ARGSUSED*/
static pfn_t
setup_lowmem(cnodeid_t node, pfn_t nextfree, pfn_t last)
{
	caddr_t			nextbyte;
	register int		m;
	int			page_size_index;
	int			pagesize;
	pg_free_t		*pfl;
	nodepda_t		*node_pdap;
	int			bitmap_size;
#ifdef DISCONTIG_PHYSMEM
	int			index;
	int			slot;
#endif
	extern pfn_t		low_maxclick;	/* max click before mem hole */
	pfn_t			clrmem;
	pfn_t			maxclearclick;

	lowmemdata_record("setup_lowmem", node, 0, (caddr_t)ctob(nextfree));

	/*
	 * Some of the memory we will be allocating in lowmem,
	 * both directly and via init routines,
	 * is unfortunately assumed to be zero'd.
	 * A check is made at the end to make sure we cleared enough.
	 */

#ifdef SN0

	/*
	 * Clear up to the pfdat table.
	 */
	maxclearclick = pfn_to_node_base_pfn(nextfree) +
			btoc(NODE_PFDAT_OFFSET);
	clrmem = maxclearclick - nextfree;

#else /* not SN0 */

	/*
	 * Clear 32MB.
	 * (also avoids all the hacks recently added for everest...)
	 *
	 * Some machines (IP26) can support large memory, with under
	 * 32MB mapped low and large amounts mapped high.
	 */

#ifdef IP30
	/*
	 * Incident #768932
	 * For Octanes with >6G memory installed it was
	 * discovered that clearing 32M is not quite enough.
	 * 8G configs (the max possible) uses about 35M.
	 * so clear a bit more (40M) just to be safe.
	 */
#define MAXCLEAREDMEM	(0x2800000/NBPP)	/* 40MB */
#else
#define MAXCLEAREDMEM	(0x2000000/NBPP)	/* 32MB */
#endif
	maxclearclick = low_maxclick ? low_maxclick : maxclick;
	clrmem = maxclearclick - nextfree;
	if (clrmem > MAXCLEAREDMEM) {
		clrmem = MAXCLEAREDMEM;
		maxclearclick = nextfree + clrmem;
	}

#endif /* not SN0 */

#if !defined(SABLE_RTL) && !defined(SABLE) && !defined(_SYSTEM_SIMULATION)
#ifdef IP32
	/* clear it here only if ecc_meminit() hasn't */
	if ((clrmem + nextfree) > ecc_meminited)
#endif  /* IP32 */
	{
		/*
		 * No need to zero anything on simulators where we want to avoid
		 * excess cycles -- they initialize memory to zero already.
		 */
		bzero((caddr_t)PHYS_TO_K0(ctob(nextfree)), ctob(clrmem));
		flush_cache(); /* for non-cache-coherent machines */
	}
#endif	/* !SABLE_RTL && !SABLE && !_SYSTEM_SIMULATION */

	lowmemdata_record("lowmemCleared", node, ctob(clrmem),
				(caddr_t)ctob(nextfree));

	nextbyte = mktables(node, nextfree);
#if !defined(NUMA_BASE)
	ASSERT(node == 0);
#endif	/* NUMA_BASE */

	/* 
	 * Allocate space for Per node data structure 
	 * Initialize NUMA portion of nodepda 
	 */
	NODEPDA_GLOBAL(node) = (nodepda_t *) low_mem_alloc(sizeof(nodepda_t),
						&nextbyte, "NODEPDA_GLOBAL");
#ifdef	NUMA_BASE
	/* 
	 * Drop nodepda into master pda.
	 * This value is used early on init_kheap . For other CPUs,
	 * this assignment is done in initpdas(), which is called after 
	 * tlbinit() returns.
	 * NOTE: p_nodepda defined only for NUMA systems
	 */
	if (node == 0)
		masterpda->p_nodepda = NODEPDA_GLOBAL(node); 
#endif	/* NUMA_BASE */

	node_pdap = NODEPDA_GLOBAL(node);

#ifdef	NUMA_BASE
	node_pdap->pernode_pdaindr = (nodepda_t **)
			low_mem_alloc(sizeof(void *) * numnodes, &nextbyte,
					"pernode_pdaindr");
#endif	/* NUMA_BASE */

	/*
	 * Setup our node pda. Most of further NODEPDA references are
	 * for our own nodepda, and this will take care of it.
	 */
	NODEPDA(node) = NODEPDA_GLOBAL(node); 

	nodepda_numa_init(node, &nextbyte);

	node_pdap->node_pg_data.pg_freelst = (struct pg_free *)
                low_mem_alloc(numnodes * sizeof(struct pg_free), &nextbyte,
			"node_pg_data.pg_freelst");

	/*
	 * Allocate bitmaps for the large page code.
	 * For discontiguous memory, each slot gets its own bitmaps.
	 */
#ifdef	DISCONTIG_PHYSMEM
	for (slot=0; slot<MAX_MEM_SLOTS; ++slot) {
		bitmap_size = slot_getsize(node,slot) / NBBY;
		index = page_bitmaps_index(COMPACT_TO_NASID_NODEID(node),slot);
		if (bitmap_size == 0) {
			page_bitmaps_pure[index] = (bitmap_t)0;
			page_bitmaps_tainted[index] = (bitmap_t)0;
		} else {
			page_bitmaps_pure[index] = \
				(bitmap_t)low_mem_alloc(bitmap_size, &nextbyte,
						"page_bitmaps_pure");
			page_bitmaps_tainted[index] = \
				(bitmap_t)low_mem_alloc(bitmap_size, &nextbyte,
						"page_bitmaps_tainted");
		}
	}
#else
	/*
	 * Compute the bitmap_size. It is the maximum pfn for that node + 1.
	 * Also align it to the nearest byte.
	 */
	bitmap_size = (((pfn_nasidoffset(last) + 1) + NBBY)/NBBY);

	page_bitmap_pure =    (bitmap_t)low_mem_alloc(bitmap_size, &nextbyte,
						"page_bitmap_pure");
	page_bitmap_tainted = (bitmap_t)low_mem_alloc(bitmap_size, &nextbyte,
						"page_bitmap_tainted");
#endif


	/*
	 * Populate the pg_freelst of the current node which should be 
	 * master node. It will then be copied to other nodes in meminit();
	 */

	pfl = (pg_free_t *)&(NODEPDA_GLOBAL(0)->node_pg_data.pg_freelst[(node)]);

	/*
	 * Allocations common to all nodes should go starting here.
	 */

	/*	Allocate space for free page buckets.
	 *	Size of largest cache (in pages) MUST be power of 2.
	 */

	m = MAX(picache_size/NBPP, pdcache_size/NBPP);
	m = MAX(m, boot_sdcache_size/NBPP);
	m = MAX(m, boot_sicache_size/NBPP);
	m = MAX(m, boot_sidcache_size/SCACHE_SET_ASSOC/NBPP);
#ifdef R4600
	if (two_set_pcaches)
		m = two_set_pcaches / NBPP;
#endif
	ASSERT((m & m - 1) == 0);
	page_size_index = MIN_PGSZ_INDEX;
	node_pdap->node_pg_data.pheadmask[page_size_index] = m - 1;

#ifdef IP19
	/* Need to reserve some K2 addresses for handling cache ECC errors
	 * in order to avoid VCEs.
	 */
	{
		extern ip19_init_ecc_info( __psunsigned_t );
		__psunsigned_t ecc_vcecolor;

		ecc_vcecolor = (__psunsigned_t)kvalloc(cachecolormask+1,
						       VM_VACOLOR, 0);
		
		ip19_init_ecc_info( ecc_vcecolor );
	}
#endif /* IP19 */

#ifdef TILES_TO_LPAGES
	/* we keep our 4K pages on 3 distinct sets of pheads */
	pfl->phead[page_size_index] = (struct phead *)
			low_mem_alloc(NTPHEADS * m * sizeof(struct phead),
				&nextbyte, "pfl->phead[page_size_index]");
#else
	pfl->phead[page_size_index] = (struct phead *)
			low_mem_alloc(m * sizeof(struct phead), &nextbyte,
				"pfl->phead[page_size_index]");
#endif

	page_size_index++;

	/*
	 * Compute the pheadmask for the rest of the page sizes
	 * and allocate the free list headers.
	 */

	for ( pagesize = NBPP << PAGE_SIZE_SHIFT;
              pagesize <= MAX_PAGE_SIZE; 
              pagesize <<= PAGE_SIZE_SHIFT) {
                
		m >>= PAGE_SIZE_SHIFT;
	
		/*
		 * If page size is greater than cache size then allocate
		 * only one list.
		 */

		if ( !m) m = 1;

		node_pdap->node_pg_data.pheadmask[page_size_index] = m - 1;
		pfl->phead[page_size_index] = (struct phead *)
			low_mem_alloc(m * sizeof(struct phead), &nextbyte,
					"pfl->phead[page_size_index]");
		page_size_index++;
	}

#ifdef MH_R10000_SPECULATION_WAR
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_KRPF)) {
		krpf_early_init(&nextbyte);
	}
#endif /* MH_R10000_SPECULATION_WAR */

	/*
	 * Allocate space for the pfdat.  DO NOT ADD ANY NEW DATA ALLOCATIONS
	 * BETWEEN HERE AND THE END OF THIS FUNCTION.
	 *
	 * For the discontiguous memory case, the pfdat array is allocated
	 * in node_meminit().
	 */

#if DISCONTIG_PHYSMEM

	nextbyte = (caddr_t) K0_TO_PHYS(nextbyte);

#else
#ifdef TILES_TO_LPAGES
	{
		int total_tiles, unref_tiles, alloc_tiles;
		int curclick = btoct(K0_TO_PHYS(nextbyte));
		int m;

		/*
		 * Like pfdat's we will never address tiles below
		 * "nextbyte", and we want tiles to be 0-based for fast
		 * indexing.  Compute how many tiles we really need to
		 * allocate and then adjust tile_data back for the tiles
		 * that will never be referenced.
		 *
		 * Because tiles are tile-aligned we add 2 to alloc_tiles to
		 * allow for possible partial tiles at the beginning and end.
		 */
		total_tiles = maxclick / TILE_PAGES;
		unref_tiles = curclick / TILE_PAGES;
		alloc_tiles = total_tiles - unref_tiles + 2; /* +2 for slop */
		m = alloc_tiles * sizeof(tile_data_t);
		tile_data = (tile_data_t *)low_mem_alloc(m, &nextbyte,
							 "tiledata");
		tile_data -= unref_tiles;
	}
#endif /* TILES_TO_LPAGES */
#ifdef MH_R10000_SPECULATION_WAR
	if (IS_R10000()) {
		int need;
		pfn_t curclick;

		nextbyte = (caddr_t) K0_TO_PHYS(nextbyte);
		curclick = btoct(nextbyte);
		need = (last - curclick + 1) * sizeof(struct pfdat);

		pfdat = (struct pfdat *) extk0_mem_alloc(need, "pfdat[]");
		pfdat -= curclick;
	} else
#endif /* MH_R10000_SPECULATION_WAR */
	{
	ordinal_ptr = pfdat = (struct pfdat *) nextbyte;

	/*	Since the pfdat array will never be addressed for
	 *	pages below ``nextbyte'', instead of subtracting
	 *	the kernel page base offset from every pfn when
	 *	calculating the pfdat from the page frame number,
	 *	we just adjust the pfdat downwards here.
	 */

	nextbyte = (caddr_t) K0_TO_PHYS(nextbyte);
	pfdat -= btoct(nextbyte);

	/*	Allocate just enough pfdats to map pageable memory.
	 *	We don't need pfdat entries for the tables we have
	 *	just built.
	 */

	max_pageord = (last - btoct(nextbyte) + 1);
	nextbyte += max_pageord * sizeof(struct pfdat);
	}
#endif /* DISCONTIG_PHYSMEM */

#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000()) {
                pfd_lowmem = pfdat + SMALLMEM_K0_R10000_PFNMAX + 1;
                pfn_lowmem = SMALLMEM_K0_R10000_PFNMAX + 1;
		tile_lowmem = pfntotile(pfn_lowmem + 1);
        } else {
                pfd_lowmem = pfdat;
                pfn_lowmem = 0;
		tile_lowmem = tile_data;
        }
#endif /* MH_R10000_SPECULATION_WAR */

        /*
         * Verify we're not using more memory than what we've cleared.
         */
	if (btoc(nextbyte) > maxclearclick) {
                cmn_err_tag(1772, CE_WARN, "Allocated more memory (%x pages) in mktables than cleared!\n",
                        btoc(nextbyte)-nextfree);
	}

	/*	DO NOT ADD ANY DATA ALLOCATIONS _AFTER_ THE PFDAT!!!
	 *
	 *	If you need to allocate data here, do it
	 *	_before_ pfdat.  Pfdat is used to map all memory that
	 *	can be allocated and freed via the page subsystem.
	 *	Care is taken not to allocate pfdat structures for
	 *	memory pinned in low memory.  If you add anything after
	 *	here, you'll just waste pfdats.  ("You know that a pfdat
	 *	is a bad thing to waste.")
	 */


	/*	STOP!!! DO NOT ADD ANY DATA ALLOCATIONS HERE!!!
	 *	READ THE NICE COMMENT ABOVE!!!  WE ARE RUNNING
	 *	OUT OF CAPITAL LETTERS -- THIS IS YOUR LAST WARNING!!!
	 */

	/*	Return next available physical page.
	 */

	lowmemdata_record("setup_lowmem$", node, 0, nextbyte);
	return btoc(nextbyte);
}

/*
 * Create system space to hold page allocation and
 * buffer mapping structures and hash tables
 */
static caddr_t 
mktables(cnodeid_t node, pfn_t nextfree)
{
	int i;
	register uint_t vmeadapters;
	caddr_t	nextbyte;

	/*	MIPS -- system tables are allocated in k0seg
	 *	instead of "system virtual space".  They don't
	 *	grow or move so they don't need to be mapped.
	 *	But they MUST be page aligned and page extended
	 *	since they will be attached to system region.
	 */

	/*
	 * NUMA is a new concept to the kernel.
	 * This routine makes a basic attempt to spread
	 * memory allocations across a few different nodes.
	 * A better long-term answer is probably to distribute subsystems
	 * such that their allocations are changed to be dynamic within
	 * the appropriate node's memory.
	 */

#ifndef SN0
	/*
	 *	Another reason that kptbl needs to be page
	 *	aligned: The R4000 handles K2SEG misses with
	 *	the fast tlb refill vector. This means that kptbl
	 *	gets mapped into KPTESEG (at addr KPTE_KBASE) and
	 *	thus must be page aligned.
	 *
	 *	For the R4000, we need to color align kptbl with
	 *	the virtual addresess at which it will be referenced --
	 *	KPTE_KBASE and this K0SEG address -- since it would
	 *	be difficult to handle a VCE in the inline kernel
	 *	tlbmiss handler.
	 */
	nextfree = pagecoloralign(nextfree, btoct(KPTE_KBASE));
	nextbyte = (caddr_t)PHYS_TO_K0(ctob(nextfree));

	{
	int kptbl_size;
	ASSERT(syssegsz > 0);
	kptbl_size = (syssegsz + btoc(MAPPED_KERN_SIZE)) * sizeof(pde_t);
 	kptbl = (pde_t *) nextbyte;
	nextbyte += kptbl_size;
	lowmemdata_record("kptbl", 0, kptbl_size, (caddr_t)kptbl);
	init_kptbl(kptbl, syssegsz + btoc(MAPPED_KERN_SIZE));
	}
#else	/* SN0 */
	/*
	 * For SN0, we have to allocate kptbl later,
	 * because it might be too big to fit under
	 * the pfdats.  It should really be distributed.
	 */

	if (node != 0) {
		/*
		 * The fragmented low memory cache is initialized in
		 * pagecoloralign().  However, it doesn't know about different
		 * nodes, and now we're doing allocations for other nodes.
		 */
		mcache = NULL;
	}
	nextbyte = (caddr_t)PHYS_TO_K0(ctob(nextfree));
#endif	/* SNO */

	/*
	 * Initialize the maps used to allocate kernel virtual address space.
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_SYSBITMAP)) {
		init_sysbitmap(&nextbyte, sptsz, &sptbitmap, btoc(K2SEG),clrsz);
	}

	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_KVSWAP)) {
		kvswap_map = (char *)low_mem_alloc(syssegsz/NBBY, &nextbyte,
						   "kvswap_map");
	}

	/*
	 *	Allocate space for conbuf, putbuf and errbuf.
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_CONBUF)) {
		putbufsz = putbuf_cpusz * maxcpus;
		if (putbufsz > putbuf_maxsz) 
		    putbufsz = putbuf_maxsz;

		conbufsz = conbuf_cpusz * maxcpus;
		if (conbufsz > conbuf_maxsz) 
		    conbufsz = conbuf_maxsz;

		errbufsz = errbuf_cpusz * maxcpus;
		if (errbufsz > errbuf_maxsz)
		    errbufsz = errbuf_maxsz;
		{
		    /* We can more efficiently manage circular buffers whose
		     * size is a power of two because we can use & to wrap
		     * rather than %.
		     * Round putbufsz up to the next highest power of two.
		     */
		    int x;
		    for(x = 1; x < putbufsz; x <<= 1);
		    putbufsz = x;
		}

		conbuf = (char *)low_mem_alloc(conbufsz, &nextbyte, "conbuf");
		putbuf = (char *)low_mem_alloc(putbufsz, &nextbyte, "putbuf");
		errbuf = (char *)low_mem_alloc(errbufsz, &nextbyte, "errbuf");

		/* Zero conbuf, putbuf & errbuf for ease of field debugging */
#if !SABLE_RTL
		bzero(conbuf, conbufsz);
		bzero(putbuf, putbufsz);
		bzero(errbuf, errbufsz);
#endif
	}

#ifdef SN
	if (lowmem_alloc_nodematch(node, LOWMEM_ALLOC_NODE_SNERRBUF)) {
		sneb_size = snerrste_pages_per_cube * SN_ERRSTATE_BUFSZ_CUBE *
			ROUND_CUBE(maxnodes);

		sneb_buf = (char *) low_mem_alloc(sneb_size, &nextbyte,
			"snerrste_buf");

		bzero(sneb_buf, sneb_size);
	}
#endif

	/*
	 * Allocate space for action list.
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_ACTIONLIST)) {
#ifndef SN
		/*
		 * Actionlist is too big on SN platforms to allocate here.
		 * Instead, only the action blocks contained in the PDA
		 * will be available for use until later in boot.
		 */
		actionlist = (action_t *)
			low_mem_alloc(maxcpus * nactions * sizeof(action_t),
					&nextbyte, "actionlist");
#endif
		actioninit();
	}
	
	/*
	 * Allocate space for vme_ivec[] and vmevec_alloc[]
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_VME)) {
		if ((vmeadapters = readadapters (ADAP_VME)) > 0) {
			vme_ivec = (struct vme_ivec *)
				low_mem_alloc( vmeadapters * MAX_VME_VECTS *
					       sizeof(struct vme_ivec),
							&nextbyte, "vme_ivec");
#if defined(EVEREST) && defined(ULI)
                        for(i=0;i<vmeadapters * MAX_VME_VECTS;i++)
                                vme_ivec[i].vm_tinfo=(struct thd_int_s *)
                                        low_mem_alloc(sizeof(struct thd_int_s),
                                                &nextbyte,"vm_tinfo");
#endif /* EVEREST && ULI */

			vmevec_alloc = (int *)
				low_mem_alloc(vmeadapters * sizeof(int),
						&nextbyte, "vmevec_alloc");
			vme_ivec_init();
		}
	}

	/*
	 * Allocate space for semaphore history.
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_SEMAHIST)) {
		semahist = (semahist_t *)
			low_mem_alloc((histmax+1) * sizeof(semahist_t),
				      &nextbyte, "semahist");
	}


	/*	NEW DATA ALLOCATIONS GO ABOVE HERE!!!
	 *	The code below does some sizing based on what
	 *	already has been allocated.
	 */

	/*
	 * Set up the value for maxpglist based on our current
	 * understanding of the sizing of the system.  At this
	 * point, we know about everything except the pfdat array,
	 * the vhand/getpages structures associated with the maxpglst
	 * sizing and the p0 data structures.
	 *
	 * Setting the value prior to those initializations is probably
	 * OK since setting a value based on the memory sizing will
	 * result in an integer divide by a reasonably large number
	 * (currently 512) which further dilutes an exact dependency
	 * on its current value.
	 */
	if (node == 0) {
		initmaxpglst(physmem - (btoc((caddr_t)K0_TO_PHYS(nextbyte)) - 
				pmem_getfirstclick()));
	}


	/*
	 * Allocate vhand/getpages structures.
	 */
	if (lowmem_alloc_nodematch(node,LOWMEM_ALLOC_NODE_PGLST)) {
		int m;
		m = maxpglst * NPGLSTS;
		pglst = (pglst_t *)
			low_mem_alloc(m * sizeof(pglst_t), &nextbyte, "pglst");
		gprglst = (gprgl_t *)
			low_mem_alloc(m * sizeof(gprgl_t), &nextbyte,"gprglst");
	}

#ifdef  EVEREST
	/*
	 * Just use all of memory for the size of the array.  This
	 * keeps us from having to play the games that we play with
	 * the pfdat table pointer below.
	 */
	if (io4ia_war && io4ia_userdma_war) {
		int m;
		m = physmem;
		num_page_counters = m;
		page_counters = (page_counter_t *)
			low_mem_alloc(m * sizeof(page_counter_t), &nextbyte,
					"page_counters");
	}
#endif /* EVEREST */

	return nextbyte;
}

/*
 * Set up first thread
 */
static __psunsigned_t
t0init(void)
{
	kthread_t *kt;
	extern void sthread_init(void);
	extern kthread_t *sthread0_init(void);

	cred_init();		/* ensure that sthread0 has a credential */
	sthread_init();
	kt = sthread0_init();

	/*
	 * sthread0_init doesn't do any scheduling so we must
	 * pre-init some stuff here
	 */
	kt->k_sqself = 1;	/* will be put on stack in main */
	kt->k_sonproc = cpuid();
	kt->k_mustrun = PDA_RUNANYWHERE;
	kt->k_lastrun = CPU_NONE;

	private.p_curkthread = kt;
	private.p_nextthread = kt;
	ktimer_init(kt, AS_SYS_RUN);
	return (__psunsigned_t) kt->k_regs[PCB_SP];
}

/* Boolean for showconfig variable. */
int showconfig;

/*
 * Parse the users disk name into a dev_t.  Names are either new-style
 * hwgraph-aware names ("/hw/....") or old-style names with the format
 *	<device-name-prefix><controller-number>d<drive-number>s<fs-number>
 *	For example:	ips0d0s0 means the "ips" device, controller 0, drive 0,
 *			fs 0
 * Return 0 if the name is bad, otherwise return the appropriate dev.
 *
 * WARNING: old names assume some knowledge about the layout of the minor device
 *	    for disk drivers.
 */
static dev_t
devparse(register char *cp)
{
	dev_t dev;


	/*
	 * See if this is a hwgraph path.
	 */
	dev = hwgraph_path_to_dev(cp);
	if (dev != NODEV) {	
		hwgraph_vertex_unref(dev);
		return(dev);
	}
	printf("Unknown device name \"%s\".\n", cp);
	return (0);
}

int devavail(dev_t);

/*
 * baddev - print error msg that device is not available and
 * list possible devs and reboot
 */
void
baddev(char *var, char *name)
{
	
	/* tell user possible devices and stop */
	if (is_specified(var))
		printf("%s device %s not available; reset and try again or enter",
			name, var);
	else
		printf("Enter %s device", name);
	printf(" on boot command line\n");
	printf("Configured device names are:\n dks ");
	printf("\n You may also want to check the value of prom nvram \n");
	/*
	 * Delay before rebooting, so they have a chance to read the
	 * messages if on a graphics tube.
	 */
	if (kdebug)
		debug("ring");
	DELAY(15000000);
	prom_reboot();
	/* NOTREACHED */
}

/*
 * devnm_to_dev
 * Convert 'dks?d?s?' to dev_t (i.e. a hwgraph vertex handle).
 */
static dev_t
devnm_to_dev(char *devnm)
{
	char devnm2[MAXDEVNAME];
	dev_t hwdisk;
	int rc;

	/*
	 * We need to call devavail on the volume header in order to
	 * load the partition table for the root disk. This keeps
	 * the driver from needing to add bogus partition entries.
	 */
	devnamefromarcs(devnm);
	strcpy(devnm2, devnm);
	dksc_partition_to_canonical_special(devnm2, EDGE_LBL_VOLUME);
	hwdisk = hwgraph_path_to_dev(devnm2);
	ASSERT(hwdisk != NODEV);
	hwgraph_vertex_unref(hwdisk);
	rc = devavail(hwdisk);
	ASSERT(rc); rc = rc;
	hwdisk = devparse(devnm);

	return (hwdisk);
}

/* XXX- should operate on the graph, not the name.
 */
char *
partitiontoother(char *name, int from, int to)
{
	char *tmp, *tmp2;
	static char devnm[MAXDEVNAME];
	char num[100];
	tmp = strstr(name,EDGE_LBL_PARTITION);
	/* advance to the first character after "partition" */
	if (tmp == NULL)
		return (NULL);
	sprintf(num, "%d/",from);
	tmp += strlen(EDGE_LBL_PARTITION);
	strncpy(devnm, name, tmp - name);
	tmp2 = devnm + (tmp - name);
	tmp++;  /* Get past '/' */
	if (from >= 0 && strncmp(tmp, num, strlen(num)) != 0)
		return(NULL);
	while(*tmp != '/')
		tmp++;
	sprintf(tmp2,"/%d%s", to, tmp);
	strcpy(name, devnm);
	return(devnm);

}

/*
 * devinit - make sure rootdev make sense permitting
 * user to override (if specified the "root" command line option)
 */
void
devinit(void)
{
	extern int icode_file[];
	extern char *bootswapfile, *bootrootfile;
	int didsomething;
	extern u_char miniroot;
	char devnm[MAXDEVNAME];
	dev_t dev, hwdisk, hwrdisk;
	int rc;

	if (is_specified(arg_initfile)) {
		/*
		 * Use user supplied name for init program
		 * according to value of "initfile"
		 */
		if (!is_true(arg_initfile)) {
			if (strlen(arg_initfile) < 32) {
				/* name is okay - copy to icode */
				strcpy((char *) icode_file, arg_initfile);
			} else {
				printf("Filename for init is too long.\n");
				strcpy(arg_initfile, "");
			}
		} else
			arg_initfile[0] = 0;

		if (strlen(arg_initfile) == 0) {
			printf("Enter full path name for program to be used instead of /etc/init on command line\n");
			prom_reboot();
			/* NOTREACHED */
		}
		printf("Using \"%s\" instead of /etc/init.\n",
			      (char *) icode_file);
	}

	/* (See vfs_mountroot) */
	if (diskless = atoi(kopt_find("diskless")))
		return;

	didsomething = 0;

	/*
	 * Setup rootdev.
	 *
	 * Check arg_root first.  If it's specified, but doesn't
	 * represent a valid device, then we call baddev() and bail.
	 *
	 * If arg_root isn't set, then we try the pathname specified
	 * in the system(4) file (bootrootfile).  There are only 2
	 * supported values for this string: /dev/root (the default) and
	 * /dev/dsk/dks?d?s?.  If it's the former, then we default to
	 * OSLoadPartition.
	 *
	 * If all else fails, try OSLoadpartition.
	 */
	if (showconfig)
		cmn_err(CE_CONT, "default root device is %s\n",
			(bootrootfile ? bootrootfile : "NULL"));

	if (is_specified(arg_root)) {
		if (is_true(arg_root)) {
			/* gave null name */
			baddev(arg_root, "Root");
			/* NOTREACHED */
		}
		strcpy(devnm, arg_root);
		rootdev = devnm_to_dev(devnm);
		if (miniroot) {
			if (partitiontoother(dev_to_name(rootdev, devnm,
							 MAXDEVNAME), 0, 1)) 
				rootdev = devparse(devnm);
			didsomething++;
		}
		if (!devavail(rootdev)) {
			cmn_err_tag(1773, CE_NOTE, "Root device %s not available; trying default\n", devnm);
			rootdev = NODEV;
		}
	} else if (strncmp(bootrootfile, "/dev/dsk/", 9) == 0) {
		strcpy(devnm, bootrootfile+9);
		rootdev = devnm_to_dev(devnm);
		if (!devavail(rootdev))
			rootdev = NODEV;
	}

	/* 
	 * If we still don't have it, then try OSLoadPartition.
	 * If root is not set when booting the miniroot we will
	  * come here and look at the OSLoadPartition for the correct value.
	 */
	if (rootdev == NODEV) {
		arcs_setuproot();
		strcpy(devnm, arg_root);
		rootdev = devnm_to_dev(devnm);
		if (miniroot) {
			if (partitiontoother(dev_to_name(rootdev, devnm,
							 MAXDEVNAME), 0, 1))
				rootdev = devparse(devnm);
		}
		if (!devavail(rootdev))
			baddev(devnm, "Root");
	}

	if (showconfig)
		cmn_err(CE_CONT, "using root dev 0x%x path %V\n",
			rootdev, rootdev);

	/*
	 * create /hw/disk & /hw/rdisk ...
	 */
	if ((hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK)) == NODEV) {
		rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK , &hwdisk);
		ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		hwgraph_vertex_unref(hwdisk);
	}
	if ((hwrdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK)) == NODEV) {
		rc = hwgraph_path_add(hwgraph_root, EDGE_LBL_RDISK , &hwrdisk);
		ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		hwgraph_vertex_unref(hwrdisk);
	}

	/*
	 * Setup swap partition.
	 *
	 * If arg_swap is set, try to use it.  If it's in arcs format
	 * (i.e. dks?d?s?), convert it to hwgraph form and use the result
	 * to create the /hw/disk/swap alias.  If it's in some other
	 * form (i.e. an absolute pathname) then trust that we can addkswap
	 * it as is later on.
	 *
	 * If arg_swap is not set, then assume that the default swap should
	 * be partition 1 on the rootdev disk.
	 * 
	 * bootswapfile will either be NULL (in which case addkswap will
	 * use /dev/swap), /dev/swap (the default), or will point to the
	 * full path of the primary swap device to be used by addkswap.
	 */
	dev = NODEV;
	if (is_specified(arg_swap)) {
		if ( strcmp ( arg_swap, arg_root ) == 0 ) {
			cmn_err_tag(1774, CE_WARN,"value for PROM variable swap "
				"\"%s\" must not be equal to value "
				"specified for root \"%s\". "
				"( rebooting ... )\n",
				arg_swap , arg_root);
				/*
				 *   delay 10 seconds so that on a graphics
				 *   tube the message could be read.
				 */ 
				DELAY(10000000);
				prom_reboot();
				/* NOT REACHED */
		} else if (strncmp(arg_swap, "dks", 3) == 0) {
			strcpy(devnm, arg_swap);
			dev = devnm_to_dev(devnm);
			if (!devavail(dev))
				bootswapfile = NULL;
		} else {
			bootswapfile = arg_swap;
		}
		didsomething++;
	}

	if ((bootswapfile && (strcmp(bootswapfile, "/dev/swap") == 0)) &&
	    dev_is_vertex(rootdev)) {
		/* we need /hw/disk/swap in the mini root */
		if (miniroot || dev != NODEV ||
		    partitiontoother(devnm, 0, 1)) {
			if (dev == NODEV)
				dev = hwgraph_path_to_dev(devnm);
			if (dev != NODEV){
#ifdef DEBUG
				printf("/hw/disk/swap is alias for %s\n",
				       devnm);
#endif /* DEBUG */
				rc = hwgraph_edge_add(hwdisk,dev,"swap");
				ASSERT(rc == GRAPH_SUCCESS); rc = rc;
				hwgraph_vertex_unref(dev);
				dev = blocktochar(dev);
				if (dev != NODEV) {
					rc = hwgraph_edge_add(hwrdisk,
							      dev, "swap");
					hwgraph_vertex_unref(dev);
					ASSERT(rc == GRAPH_SUCCESS); rc = rc;
				}
			}
		}
	}

	if (is_specified(arg_swplo)) {
		swplo = atoi(arg_swplo);
		didsomething = 1;
	}
	if (is_specified(arg_nswap)) {
		nswap = atoi(arg_nswap);
		didsomething = 1;
	}

	/*
	 * By this time we should have rootdev in HWG form ... 
	 */
	if (dev_is_vertex(rootdev)) {
		/*
		 * Create aliases in /hw to root, usr and the volume header
		 */
		dev_to_name(rootdev, devnm, MAXDEVNAME);
#ifdef DEBUG
		printf("/hw/disk/root is alias for %s\n",devnm);
#endif /* DEBUG */
		rc = hwgraph_edge_add(hwdisk, dev_to_vhdl(rootdev),"root");
		ASSERT(rc == GRAPH_SUCCESS); rc = rc;
		dev = blocktochar(rootdev);
		if (dev != NODEV) {
			rc = hwgraph_edge_add(hwrdisk, dev, "root");
			ASSERT(rc == GRAPH_SUCCESS); rc = rc;	
			hwgraph_vertex_unref(dev);
		}
		if (dksc_partition_to_canonical_special(devnm, EDGE_LBL_VOLUME_HEADER)) {
			dev = hwgraph_path_to_dev(devnm);
			if (dev != NODEV){
#ifdef DEBUG
				printf("/hw/rdisk/%s alias for %s\n", EDGE_LBL_VOLUME_HEADER, devnm);
#endif /* DEBUG */
				rc = hwgraph_edge_add(hwrdisk, dev, EDGE_LBL_VOLUME_HEADER);
				ASSERT(rc == GRAPH_SUCCESS); rc = rc;
				hwgraph_vertex_unref(dev);
			}
		}
		dev_to_name(rootdev, devnm, MAXDEVNAME);
		/*
		 * Add usr alias only if / is on 0. If it is not 0
		 * then we are not a standard disk.  Also setup the
		 * the usr alias if we're on partition 1 and we're
		 * booting off the miniroot.
		 */
		if ((!miniroot && partitiontoother(devnm, 0, 6)) ||
		    (miniroot && partitiontoother(devnm, 1, 6))) {
			dev = hwgraph_path_to_dev(devnm);
			if (dev != NODEV){
#ifdef DEBUG
				printf("/hw/disk/usr is alias for %s\n",devnm);
#endif /* DEBUG */
				rc = hwgraph_edge_add(hwdisk, dev, "usr");
				ASSERT(rc == GRAPH_SUCCESS); rc = rc;
				hwgraph_vertex_unref(dev);
				dev = blocktochar(dev);
				if (dev != NODEV) {
					rc = hwgraph_edge_add(hwrdisk, dev, "usr");
					ASSERT(rc == GRAPH_SUCCESS); rc = rc;	
					hwgraph_vertex_unref(dev);
				}
			}
		}

	}

	if (didsomething) {
		printf("root on %s ; ", dev_to_name(rootdev, devnm, MAXDEVNAME));
		if (bootswapfile == NULL)
			printf("no boot swap file\n");
		else {
			printf("boot swap file on %s", bootswapfile);
			if (swplo)
				printf(" swplo %d", swplo);
			if (nswap)
				printf(" nswap %d", nswap);
			printf("\n");
		}
	}
}

/*
 * devavail - make sure dev is configured and is openable
 */
int
devavail(dev_t dev)
{
	/* Need code for both bdev and cdev as there are times where we only
	 * have one, not the other
	 */
	struct bdevsw *my_bdevsw = get_bdevsw(dev);
	if (my_bdevsw) {
		if (!bdvalid(my_bdevsw))
			return(0);
		if (!bdstatic(my_bdevsw))
			return(0);
		if (bdopen(my_bdevsw, &dev, FREAD|FWRITE, OTYP_LYR, sys_cred)
		    != 0)
			return(0);
		bdclose(my_bdevsw, dev, FREAD|FWRITE, OTYP_LYR, sys_cred);
	} else {
		struct cdevsw *my_cdevsw = get_cdevsw(dev);
		if (!cdvalid(my_cdevsw))
			return(0);
		if (!cdstatic(my_cdevsw))
			return(0);
		if (cdopen(my_cdevsw, &dev, FREAD|FWRITE, OTYP_LYR, sys_cred)
		    != 0)
			return(0);
		cdclose(my_cdevsw, dev, FREAD|FWRITE, OTYP_LYR, sys_cred);
	}
	return(1);
}

static void
tunetableinit(void)
{
	struct tunetable *tp;
	extern struct tunetable	tunetable[];

	/*
	 * init tune - the sanity function for paging will call 
	 * tune_sanity for us and verify all these numbers
	 */
	tune.t_gpgslo = gpgslo;
	tune.t_gpgshi = gpgshi;
	tune.t_gpgsmsk = gpgsmsk;
	tune.t_maxsc = maxsc;
	tune.t_maxfc = maxfc;
	tune.t_maxdc = maxdc;
	tune.t_bdflushr = bdflushr;
	tune.t_minarmem = minarmem;
	tune.t_minasmem = minasmem;
	tune.t_maxlkmem = maxlkmem;
	tune.t_tlbdrop = tlbdrop;
	tune.t_rsshogfrac = rsshogfrac;
	tune.t_rsshogslop = rsshogslop;
	tune.t_dwcluster = dwcluster;
	tune.t_autoup = autoup;

	/* loop-through tunetables */
	for (tp = &tunetable[0]; *(tp->t_name) != NULL ; tp++) {
		void (*sanity)(int);
		sanity = (void (*)(int))tp->t_sanity;
		if (sanity)
			(*sanity)(0);
	}

	/* init var */
	v.v_buf = nbuf;
	v.v_proc = nproc;
	v.v_maxdmasz = maxdmasz;
	v.v_dquot = ndquot;
	v.v_maxpmem = maxpmem;
}

/*
 * Setup the maxpglst value.  Once set, the value should not change
 * during the current incarnation of the system.
 */
static void
initmaxpglst(pfn_t basemem)
{
	register int npgs;

	if (maxpglst == 0) {
		npgs = basemem/512;
		npgs = MAX(npgs, 100);
		/*
		 * The ``cost'' of tlbsync()s rises with the
		 * number of cpus, so increase maxpglst.
		 */
		npgs += (numcpus - 1) * 16;
		maxpglst = MIN(npgs, MAXPGLST);
	} else if (maxpglst > MAXPGLST)
		maxpglst = MAXPGLST;
}

static void
globaldatast_init()
{
	/*
	 * Allocate space for the buffer structures.
	 */
	int tmp_nbuf = v.v_buf;

	ASSERT(v.v_buf && v.v_hbuf);

	global_buf_table = NULL;

	/*
	 * Check if 512 byte mapping per buffer plus the buffer
	 * array is greater than 7/8 ths of physmem. If so, scale 
	 * down nbufs.
	 */
#ifndef DEBUG_BUFTRACE
	while (btoc(v.v_buf * (512 + sizeof(buf_t))) > (physmem * 7/8))
#else
	while (btoc(v.v_buf * (512 + sizeof(buf_t) + sizeof(ktrace_t)
		+ (sizeof(ktrace_entry_t) * BUF_TRACE_SIZE))) > (physmem * 1/4))
#endif
		v.v_buf /= 2;

	while (global_buf_table == NULL) {
		global_buf_table = (buf_t *)kmem_zalloc(v.v_buf * sizeof(buf_t),
					VM_DIRECT|VM_PHYSCONTIG|VM_NOSLEEP);
		/*
		 * On O200 and O2000s since physical memory can be 
		 * discontiguous, we might not be able to allocate the
		 * global_buf_table, therefore we need to scale down the
		 * nbufs value.
		 */
		if (global_buf_table == NULL)
			v.v_buf /= 2;
	}
	nbuf = v.v_buf;

	if (tmp_nbuf != v.v_buf) {
		cmn_err_tag(1775, CE_WARN, "Could not allocate %d nbufs allocated %d nbufs",
			tmp_nbuf, v.v_buf);
		cmn_err_tag(1776, CE_WARN, "Please reconfigure nbuf tunable and reboot the system");
	}

	global_buf_hash = (hbuf_t *)kmem_zalloc(v.v_hbuf * sizeof(hbuf_t),
					VM_DIRECT|VM_PHYSCONTIG|VM_NOSLEEP);
	global_dev_hash = (dhbuf_t *)kmem_zalloc(v.v_hbuf * sizeof(dhbuf_t),
					VM_DIRECT|VM_PHYSCONTIG|VM_NOSLEEP);
	ASSERT((global_buf_table != NULL) && (global_buf_hash != NULL) &&
	       (global_dev_hash != NULL));
}
