/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Memory configuration and management routines.  Used to determine
 * both the size and configuration of memory.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/mapped_kernel.h>
#include <sys/pfdat.h>
#include <sys/debug.h>
#include <sys/SN/agent.h>
#include <sys/SN/klconfig.h>
#include "sn_private.h"
#include <ksys/cacheops.h>
#include <sys/kthread.h>
#include <sys/rt.h>
#include <sys/SN/nvram.h>

#if defined (SN0)
#include <sys/SN/SN0/IP27.h>
#endif

#if defined (SN1)
#include <sys/SN/SN1/IP33.h>
#endif

extern int numnodes;		/* Number of nodes in system */
extern char *kend;		/* Pointer to end of kernel */
extern int memory_iscalias(paddr_t);
extern unchar get_nvreg(uint offset);

short slot_lastfilled_cache[MAX_COMPACT_NODES];
unsigned short slot_psize_cache[MAX_COMPACT_NODES][MAX_MEM_SLOTS];
pfn_t page_ordinal[MAX_COMPACT_NODES][MAX_MEM_SLOTS];
volatile int poison_page_waitcnt[MAXCPUS];

static pfn_t slot_psize_compute(cnodeid_t node, int slot);
static void poison_page_inval_range(__psunsigned_t , int ,int *);

/*
 * Perform basic memory configuration.
 *
 * XXX - This would be a good place to set up protection appropriately.
 *	It may be necessary to set up memory interrupts and counters too.
 */
void
mem_init(void)
{
	extern  mutex_t probe_sema;

	/* initialize monitor lock for "badaddr" routines */
	mutex_init(&probe_sema, MUTEX_DEFAULT, "probe");
}


/**************************************************************************
 * The following routines describe the configuration of memory on systems
 * which support discontiguous physical memory (such as SN0).  The memory
 * is divided up into contiguous regions called slots, and a group of
 * zero or more slots are associated with each node.  Logically, a slot
 * can be thought of as equivalent to a bank on the Hub board.
 *
 * Currently, the discontiguous memory support assumes that each slot 
 * contains at least 32 MBytes of memory.
 */

/*
 * Return the size (in pages) of all memory installed.  This includes
 * the memory occupied or disabled.
 */
/*ARGSUSED*/
pfn_t
szmem(pfn_t fpage, pfn_t maxpmem)
{
	int node, slot;		/* Index variables */
	int numslots;		/* Number of slots in current node */
	pfn_t num_pages = 0;	/* Number of pages found */
	pfn_t slot_psize;
	extern pfn_t low_maxclick;

	ASSERT_ALWAYS(numnodes > 0);
	for (node = 0; node < numnodes; node++) {
		numslots = node_getnumslots(node);
		for (slot = 0; slot < numslots; slot++) {
			page_ordinal[node][slot] = num_pages;
			slot_psize = slot_psize_compute(node, slot);
			num_pages += slot_psize;	
			if (!low_maxclick)
			    low_maxclick = slot_psize;

			/* Initialize the slot psize cache */
			ASSERT(btoc(SLOT_SIZE) <
				1<<(8*sizeof(unsigned short)));
			ASSERT(slot_psize <= SLOT_SIZE);
			slot_psize_cache[node][slot] =
					(unsigned short) slot_psize;
			if (slot_psize)
				slot_lastfilled_cache[node] = slot;
		}	
	}

	/*
	 * If maxpmem is nonzero, restrict the size of memory, otherwise, use
	 * all of physical memory
	 */
	if (maxpmem)
		return MIN(num_pages, maxpmem);
	else
		return num_pages;
}


/*
 * Mark the pfdats of questionable pages as unusable.  This might
 * be done is a particular page of physical memory has a bad ram
 * cell, and we don't want to shut off the entire bank of memory.
 */
/* ARGSUSED */
pfn_t
setupbadmem(pfd_t *pfd, pfn_t first, pfn_t last)
{
	return 0;
}

/*****************************************************************************
 *                       SN0   Configuration Setup                         *
 *****************************************************************************/

int nasid_shift = NASID_SHFT;
int node_size_bits = NODE_SIZE_BITS;
__uint64_t nasid_bitmask = NASID_BITMASK;

int slot_shift = SLOT_SHIFT;
__uint64_t slot_bitmask = SLOT_BITMASK;

#ifdef SABLE
int slots_per_node = 1;
#else
#ifdef SN0
int slots_per_node = MD_MEM_BANKS * 4;
#else
int slots_per_node = MD_MEM_BANKS;
#endif
#endif


/*****************************************************************************
 *                      Cache Modes & Poisonn Bits                           *
 *****************************************************************************/

/*
 * Mark the given pde as cachable.
 */
void
pg_setcachable(pde_t *p)
{
	pg_setcohrnt_exlwr(p);
}


/*
 * Return the coherency bits used when a page is cachable.
 */
unsigned int
pte_cachebits(void)
{
	return PG_COHRNT_EXLWR;
}

/*
 * Set or clear the poison state for a range of addresses
 */
void
poison_state_alter_range(__psunsigned_t start, int len, int poison)
{
	int state;
	__uint64_t vec_ptr;
	hubreg_t elo;
	void *oldpin;
	char *p;
	int   _len;
	__psunsigned_t _start;
	__psunsigned_t addr;

/* physical to kseg0 - excl*/
#define      PHYS_TO_K0_EXL(x)       ((x) | K0BASE_EXL)


	ASSERT(SCACHE_ALIGNED(start));
	ASSERT(SCACHE_ALIGNED(len));

	_start = start;
	start  = TO_PHYS(start);

	oldpin = rt_pin_thread();


	if (poison) {
		/*
		 * read cacheline excl
		 */
		for (p = (char *) PHYS_TO_K0_EXL(start) , _len = len ;
		     _len; _len -= SCACHE_LINESIZE, p += SCACHE_LINESIZE) 
			*(volatile int *)(p);

		/*
		 * flush cache & inval
		 */
		p = (char *) PHYS_TO_K0_EXL(start);
		__dcache_wb_inval(p,len);
	}

#define	MAX_POISON_RETRY 15

	/*
	 * set directory state via backdoor
	 */
	for (addr = start , _len = len ;
	     _len; _len -= SCACHE_LINESIZE, addr += SCACHE_LINESIZE) {
		if (poison) {
			int i;

			for (i = 0 ; i < MAX_POISON_RETRY ; i++) {
				set_dir_state((paddr_t)addr,MD_DIR_POISONED);
				get_dir_ent((paddr_t)addr,&state,&vec_ptr,&elo);
				if (state == MD_DIR_POISONED)
					break;
			}

			ASSERT_ALWAYS(state == MD_DIR_POISONED);
		} else {
			get_dir_ent((paddr_t)addr, &state, &vec_ptr, &elo);
			ASSERT_ALWAYS(state == MD_DIR_POISONED);
			set_dir_state((paddr_t)addr, MD_DIR_UNOWNED);
		}
	}	

	if (poison) {
		int i;
		int my_id = getcpuid();

        	for (i = 0 ; i < maxcpus ; i++) {
                	if (pdaindr[i].CpuId == -1)
                        	continue;

			atomicAddInt(&poison_page_waitcnt[my_id],1);
                	cpuaction(i,(cpuacfunc_t)poison_page_inval_range,
				  A_NOW,_start,len,
				  &poison_page_waitcnt[my_id]);
        	}

        	while (poison_page_waitcnt[my_id] > 0)
                	;
	}

	rt_unpin_thread(oldpin);
}

static void 
poison_page_inval_range(__psunsigned_t start, int len,int *waitcnt)
{
	__dcache_wb_inval((void *)start, len);
	atomicAddInt(waitcnt,-1);
}

/*
 * Clear the poison bits for a page
 */
/*ARGSUSED*/
void
poison_state_clear(pfn_t pfn)
{
	poison_state_alter_range(ctob(pfn), NBPC, 0);
        return;
}

/*
 * Set the poison bits for a page
 */
/*ARGSUSED*/
void
poison_state_set(pfn_t pfn)
{
	poison_state_alter_range(ctob(pfn), NBPC, 1);
        return;
}


/*****************************************************************************
 *           System Physical Memory Parameters /  Access Methods             *
 *****************************************************************************/

/*
 * Return the first valid pfn in the system.  This is mildly
 * bogus, but we keep it around for backward compatibility.
 */
pfn_t
pmem_getfirstclick(void)
{
	nasid_t	nasid;

	nasid = get_lowest_nasid();

	return (btoc(NODE_OFFSET(nasid)));
}

/*****************************************************************************
 *           Per Node Physical Memory Parameters /  Access Methods           *
 *****************************************************************************/

/*
 * Return the page frame number of the first free page of memory
 * on a given node.  The PROM may allocate data structures on the
 * first couple of pages of the first slot of each node.  If this
 * is the case, getfirstfree(node) > getslotstart(node, 0).  The
 * meaning of getfirstfree() for node 0 is somewhat vague; it
 * will probably return the first page above kend.
 */
pfn_t
node_getfirstfree(cnodeid_t node)
{
	__psunsigned_t firstfree;

	firstfree = get_freemem_start(node);

	return (pfn_t)btoc(firstfree);
}

/*
 * Return the page frame number of the last free page of memory
 * on a given node.
 */
pfn_t 
node_getmaxclick(cnodeid_t node)
{
	pfn_t	slot_psize;
	int 	slot;

	/* Start at the top slot.
	 * When we find a slot with memory in it, that's the winner.
	 */
	for (slot = (node_getnumslots(node) - 1); slot >= 0; slot--) {
		if (slot_psize = slot_getsize(node, slot)) {
			/* Return the basepfn + the slot size, minus 1. */
			return slot_getbasepfn(node, slot) + slot_psize - 1;
		}
	}

	/*
	 * If there's no memory on the node, return 0.
	 * - This is likely to cause problems.
	 */
#if defined(DEBUG) || defined(SABLE)
	dprintf("node_getmaxclick: No memory on node %d", node);
#endif
	return (pfn_t)0;
}


/*****************************************************************************
 *                 Discontiguous Physical Memory Management                  *
 *****************************************************************************/

/*
 * Return highest slot filled
 */
int
node_getlastslot(cnodeid_t node)
{
	return (int) slot_lastfilled_cache[node];
}

/*
 * Return the number of pages of memory provided by the given
 * slot on the specified node.
 */
pfn_t
slot_getsize(cnodeid_t node, int slot)
{
	return (pfn_t) slot_psize_cache[node][slot];
}

/*ARGSUSED*/
static
pfn_t
slot_psize_compute(cnodeid_t node, int slot)
{
	nasid_t	nasid;
	lboard_t *brd;
	klmembnk_t *banks;
	__int64_t size;
	int szslot, swapval;
	unchar swap_all_banks;

#ifdef SABLE
	if (fake_prom) {
		return (btoct(SLOT_MIN_MEM_SIZE));
	}
#endif

	/* Get out nasid */
	nasid = COMPACT_TO_NASID_NODEID(node);

	/* Find the node board */
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IP27);

	if (!brd) {
#if defined(DEBUG) || defined(SABLE)
		printf("slot_getsize: Couldn't find node board.  Ignoring memory.");
#endif
		return 0;
	}

	/* Get the memory bank structure */
	banks = (klmembnk_t *)find_first_component(brd, KLSTRUCT_MEMBNK);

	if (!banks) {
#if defined(DEBUG) || defined(SABLE)
		dprintf("slot_getsize: Couldn't find memory banks.  Ignoring memory.");
#endif
		return 0;
	}

	/* Size in _Megabytes_ */

#ifdef SN0
	/*
	 * if the IO6prom has the capability to swap all the memory banks
	 * (all newer versions do), then klconfig will be in "locational"
	 * format.  See comment in translate_hub_mcreg(), and PV 669589.
	 * However, the kernel is interested in the "software" bank order,
	 * which corresponds to the actual CPU-visible memory layout.
	 */
	szslot = slot/4;  /* adjust for holes in 256MB DIMMs */
	swap_all_banks = get_nvreg(NVOFF_SWAP_ALL_BANKS);

	if (swap_all_banks == 'y') {
	    /* klconfig is in locational format:  we must translate */
	    swapval = banks->membnk_dimm_select / 2;
	} else {
	    /* klconfig is in software format:  no translation needed */
	    swapval = 0;
	}
#else
	/* 
	 * SN1's klconfig is always in locational format.
	 */
	szslot = slot;
	swapval = banks->membnk_dimm_select;

#endif
	
	size = (__int64_t)banks->membnk_bnksz[szslot ^ swapval];


#ifdef SN0
/* hack for 128 dimm banks */

	if(size <= 128 )
	{
		if(slot%4 == 0) {
		/* size in bytes */
		size <<= 20;
		/* Convert the size to pages */
		return btoc(size); }
		else
			return 0;
	}
	else
	{
		size /= 4;
		/* size in bytes */
		size <<= 20;
		/* Convert the size to pages */
		return btoc(size); 
	}
}
#else
	/* size in bytes */
	size <<= 20;
	/* Convert the size to pages */
	return btoc(size); 
	

#endif

int
memory_present(paddr_t paddr)
{
	return !CKPHYS(paddr);
}

#if defined  (SN0)

int
check_dir_access(paddr_t paddr)
{
	__uint64_t *dir_addr;
	int premium;
	md_sdir_low_t mdslow;
	md_pdir_low_t mdplow;
	nasid_t nasid = NASID_GET(paddr);

	dir_addr = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	premium = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);

	if (premium) {
		mdplow.pd_lo_val = *(dir_addr);
		return mdplow.pde_lo_fmt.pde_lo_ax;
	} else {
		mdslow.sd_lo_val = *(dir_addr);
		return mdslow.sde_lo_fmt.sde_lo_ax;
	}
}
	


int 
memory_read_accessible(paddr_t pa)
{
	__psunsigned_t prot;
	nasid_t node = NASID_GET(pa);
	int rgn = get_region(cnodeid());

	if (memory_iscalias(pa))  {
		node = get_nasid();
		pa = TO_NODE(node, TO_NODE_ADDRSPACE(pa));
	}

	if (node != get_nasid()) {
		prot = *(__psunsigned_t *)BDPRT_ENTRY(pa, rgn);
		prot = ((prot & MD_PPROT_MASK) >> MD_PPROT_SHFT);
		if ((prot == MD_PROT_RW) || (prot == MD_PROT_RO))
		    return 1;
	}
	else {
		if (check_dir_access(pa))
		    return 1;
	}
	return 0;
}

int
page_read_accessible(pfn_t pfn)
{
	int off;

	for (off = 0; off < NBPP; off += MD_PAGE_SIZE)
		if (!memory_read_accessible(ctob(pfn) + off))
			return 0;

	return 1;
}

int 
memory_write_accessible(paddr_t pa)
{
	__psunsigned_t prot;
	nasid_t node = NASID_GET(pa);
	int rgn = get_region(cnodeid());

	if (memory_iscalias(pa))  {
		node = get_nasid();
		pa = TO_NODE(node, TO_NODE_ADDRSPACE(pa));
	}

	if (node != get_nasid()) {
		prot = *(__psunsigned_t *)BDPRT_ENTRY(pa, rgn);
		prot = ((prot & MD_PPROT_MASK) >> MD_PPROT_SHFT);
		if (prot == MD_PROT_RW) 
		    return 1;
	}
	else {
		if (check_dir_access(pa))
		    return 1;
	}
	
	return 0;
}

int
page_write_accessible(pfn_t pfn)
{
	int off;

	for (off = 0; off < NBPP; off += MD_PAGE_SIZE)
		if (!memory_write_accessible(ctob(pfn) + off))
			return 0;

	return 1;
}

/*
 *  NOTE: NOT ATMOIC. Could cause inconsistent directory states. 
 *  	  Discretion advised.
 */

void
set_dir_access(paddr_t paddr, int acc)
{
	__uint64_t *dir_addr;
	int premium;
	md_sdir_low_t mdslow;
	md_pdir_low_t mdplow;
	nasid_t nasid = NASID_GET(paddr);

	dir_addr = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	premium = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);

	if (premium) {
		mdplow.pd_lo_val = *(dir_addr);
		mdplow.pde_lo_fmt.pde_lo_ax = acc;
		*(dir_addr) = mdplow.pd_lo_val;
	} else {
		mdslow.sd_lo_val = *(dir_addr);
		mdslow.sde_lo_fmt.sde_lo_ax = acc;
		*(dir_addr) = mdslow.sd_lo_val;
	}
}




/*
 *  NOTE: NOT ATMOIC. Could cause inconsistent directory states. 
 *  	  Discretion advised.
 */

int
memory_set_access(paddr_t paddr, int rgn, int access)
{
	__uint64_t prot;
	int len;

	prot = *(__psunsigned_t *)BDPRT_ENTRY(paddr, rgn);

	prot &= ~MD_PPROT_MASK;
	prot |= ((access << MD_PPROT_SHFT) & MD_PPROT_MASK);
	
	*(__psunsigned_t *)BDPRT_ENTRY(paddr, rgn) = prot;

	if (NASID_GET(paddr) == get_nasid()) {
		for (len = 0; len < 4096; len += SCACHE_LINESIZE)
		    set_dir_access(paddr+len, (access)? 1 : 0);
		/* change access in dir entries as well */
	}
	return 0;
}

#endif /* SN0 */

/*
 * Allow read/write access to a page by clearing protection bits.
 * Used by crash dump code.
 */
void
page_allow_access(pfn_t pfn)
{
	paddr_t paddr = ctob(pfn), eaddr;

	for (eaddr = paddr + NBPP; paddr < eaddr; paddr += SCACHE_LINESIZE)
		memory_set_access(paddr, get_region(cnodeid()), MD_PROT_RW);
}


/* Defined in error_dump.c */
extern char *dir_state_str[];
extern char *dir_owner_str[];

char *
decode_prot(int prot)
{
	switch(prot) {

		case MD_PROT_RW:
			return "RW";
		case MD_PROT_RO:
			return "RO";
		case MD_PROT_NO:
			return "No access";
		default:
			return "Bad";
	}
}

#if defined (SN0)
void
show_dir_state(paddr_t paddr, void (*prfunc)(char *, ...))
{
	int state;
	__uint64_t vec_ptr;
	hubreg_t elo;
	int r, mr;
	__psunsigned_t *dirprotp;

	get_dir_ent(paddr, &state, &vec_ptr, &elo);

	(*prfunc)("addr 0x%x %s ", paddr, dir_state_str[state]);
	switch (state) {
	case MD_DIR_SHARED:
		(*prfunc)("vec<0x%x>\n", vec_ptr);
		break;
	case MD_DIR_BUSY_SHARED:
	case MD_DIR_BUSY_EXCL:
	case MD_DIR_WAIT:
	case MD_DIR_EXCLUSIVE:
		(*prfunc)("Owner nasid 0x%x/%s\n", 
			vec_ptr >> 2, dir_owner_str[vec_ptr & 0x3]);
		break;
	default:
		(*prfunc)("\n");
		break;
	}

	if (REMOTE_HUB_L(NASID_GET(paddr), MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM)
		mr = MAX_PREMIUM_REGIONS;
	else
		mr = MAX_NONPREMIUM_REGIONS;

	for (r = 0; r < mr; r++) {
		dirprotp = (__psunsigned_t *)BDPRT_ENTRY(paddr, r);
		if (((*dirprotp & MD_PPROT_MASK) >> MD_PPROT_SHFT) != MD_PROT_NO) {
			if (is_fine_dirmode()) 
				(*prfunc)("NASID %d: (0x%x) Access: %s\n",
					(r>>NASID_TO_FINEREG_SHFT), dirprotp, 
					decode_prot((*dirprotp & MD_PPROT_MASK) >> MD_PPROT_SHFT));
			else
				(*prfunc)("NASID %d-%d: (0x%x) Access: %s\n",
					(r<<NASID_TO_COARSEREG_SHFT), ((r+1)<<NASID_TO_COARSEREG_SHFT)-1,
					dirprotp, decode_prot((*dirprotp & MD_PPROT_MASK) >> MD_PPROT_SHFT));
		}
	}
}


int
mdir_pstate(int chk_state, __psunsigned_t addr,
	    __uint32_t size, void (*prfunc)(char *, ...))
{
    __uint32_t off;
    md_pdir_low_t elo;
    hubreg_t elo1;
    __uint64_t vec_ptr;
    paddr_t phys;
    int state, owner, slice, i;

    phys = TO_PHYS(addr);
    for (off = 0; off < size; off += 4096, phys += 4096) {
	for (i = 0; i < 4096; i += CACHE_SLINE_SIZE) {
	    get_dir_ent(phys, &state, &vec_ptr, &elo1);
	    elo.pd_lo_val = elo1;
	    if ((chk_state != -1) && (chk_state != state)) 
		continue;

	    slice = elo.pde_lo_fmt.pde_lo_ptr & 0x3;
	    owner = elo.pde_lo_fmt.pde_lo_ptr >> 2;

	    (*prfunc)("0x%02x   0x%08x   0x%02x/%s,  %s  (0x%lx)\n",
		   NASID_GET(phys), (K0_TO_NODE_OFFSET(phys + i)),
		   owner, dir_owner_str[slice], dir_state_str[state],
		   elo.pd_lo_val);
	}
    }
    
    return 0;
}

	


int
mdir_sstate(int chk_state, __psunsigned_t addr, 
	    __uint32_t size, void (*prfunc)(char *, ...))
{
    __uint32_t off;
    md_sdir_low_t elo;
    hubreg_t elo1;
    __uint64_t vec_ptr;
    paddr_t phys;
    int state, owner, slice;
    int i;

    if (chk_state > 7) chk_state = -1;

    phys = TO_PHYS(addr);
    for (off = 0; off < size; off += 4096, phys += 4096) {
	for (i = 0; i < 4096; i += CACHE_SLINE_SIZE) {
    	    get_dir_ent(phys, &state, &vec_ptr, &elo1);
	    elo.sd_lo_val = elo1;
	    if ((chk_state != -1) && (chk_state != state)) 
		continue;

	    slice = elo.sde_lo_fmt.sde_lo_ptr & 0x3;
	    owner = elo.sde_lo_fmt.sde_lo_ptr >> 2;

	    (*prfunc)("0x%02x   0x%08x   0x%02x/%s,  %s  (0x%lx)\n",
		   NASID_GET(phys), (K0_TO_NODE_OFFSET(phys + i)),
		   owner, dir_owner_str[slice], dir_state_str[state],
		   elo.sd_lo_val);
	}
    }
    return 0;
}

void
check_dir_state(nasid_t nasid, int in_state, void (*prfunc)(char *, ...))
{
    hubreg_t mem_cfg;
    int premium;
    int size, i;
    __psunsigned_t base;
    int chk_state = in_state;
    int holed;

    chk_state = -1;

    mem_cfg = REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG);
    premium = mem_cfg & MMC_DIR_PREMIUM;

    (*prfunc)("Node   ___Off___    __Owner__      __State__   (DirLo)\n");
    
    for (i = 0; i < MD_MEM_BANKS; i++) {
	    size = MD_SIZE_MBYTES((mem_cfg & MMC_BANK_MASK(i))
				  >> MMC_BANK_SHFT(i));
            holed = ( size == 256 ) ? 1 : 0;
	    size <<= 20;
	    base = TO_NODE(nasid, i * MD_BANK_SIZE);
	    if (premium) {
              if(!holed)
              {
		if (mdir_pstate(chk_state, base, size, prfunc))
		    break;
	      }
	      else
              {
		if (mdir_pstate(chk_state, base, size/4 , prfunc))
		    break;
		if (mdir_pstate(chk_state, base + (128 << 20), size/4 , prfunc))
		    break;
		if (mdir_pstate(chk_state, base + (256 << 20), size/4 , prfunc))
		    break;
		if (mdir_pstate(chk_state, base + (384 << 20), size/4 , prfunc))
		    break;
	      }
	    }
	    else {
              if(!holed)
	      {
		if (mdir_sstate(chk_state, base, size, prfunc))
		    break;
              }
	      else
	      {
		if (mdir_sstate(chk_state, base, size/4 , prfunc))
		    break;
		if (mdir_sstate(chk_state, base + (128 << 20), size/4 , prfunc))
		    break;
		if (mdir_sstate(chk_state, base + (256 << 20), size/4 , prfunc))
		    break;
		if (mdir_sstate(chk_state, base + (384 << 20), size/4 , prfunc))
		    break;
	      }
	    }
    }
}



void
set_dir_state(paddr_t paddr, int state)
{
	__uint64_t *dir_addr;
	int premium;
	md_sdir_low_t mdslow;
	md_pdir_low_t mdplow;
	nasid_t nasid = NASID_GET(paddr);
#if DEBUG
	int		tstate; 
	__uint64_t     	towner;
	hubreg_t	telo;

	get_dir_ent(paddr, &tstate, &towner, &telo);
#endif

	dir_addr = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	premium = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);

	if (premium) {
		mdplow.pd_lo_val = *(dir_addr);
		mdplow.pde_lo_fmt.pde_lo_state = (state);
		mdplow.pde_lo_fmt.pde_lo_ste = (state ? 0 : 1);
		*(dir_addr) = mdplow.pd_lo_val;
	} else {
		mdslow.sd_lo_val = *(dir_addr);
		mdslow.sde_lo_fmt.sde_lo_state = state;
		*(dir_addr) = mdslow.sd_lo_val;
	}
}


void 
set_dir_owner(paddr_t paddr, int owner)
{
	__uint64_t *dir_addr;
	int premium;
	md_sdir_low_t mdslow;
	md_pdir_low_t mdplow;
	nasid_t nasid = NASID_GET(paddr);

	dir_addr = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	premium = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);	

	if (premium) {
		mdplow.pd_lo_val = *(dir_addr);
		if (owner == -1) owner = ~mdplow.pde_lo_fmt.pde_lo_ptr;
		mdplow.pde_lo_fmt.pde_lo_ptr = owner;
		*(dir_addr) = mdplow.pd_lo_val;
	} else {
		mdslow.sd_lo_val = *(dir_addr);
		if (owner == -1) owner = ~mdslow.sde_lo_fmt.sde_lo_ptr;
		mdslow.sde_lo_fmt.sde_lo_state = owner;
		*(dir_addr) = mdslow.sd_lo_val;
	}
}


void
get_dir_ent(paddr_t paddr, int *state, __uint64_t *owner, hubreg_t *elo)
{
	__uint64_t *dir_addr_lo, *dir_addr_hi;
	int premium;
	md_sdir_low_t mdslow;
	md_sdir_high_t mdshigh;
	md_pdir_low_t mdplow;
	md_pdir_high_t mdphigh;
	nasid_t nasid = NASID_GET(paddr);
	dir_addr_lo = (__uint64_t *)BDDIR_ENTRY_LO(paddr);
	dir_addr_hi = (__uint64_t *)BDDIR_ENTRY_HI(paddr);
	
	premium = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) & MMC_DIR_PREMIUM);
	if (premium) {
		mdplow.pd_lo_val = *(dir_addr_lo);
		mdphigh.pd_hi_val= *(dir_addr_hi);


		/*****************************************************
		 * This code only works for SHARED FINE - <= 128CPUS *
		 ****************************************************/

		if (mdplow.pde_lo_fmt.pde_lo_ste) {
		    *owner = mdphigh.pd_hi_fmt.pd_hi_bvec | 
			(mdplow.pds_lo_fmt.pds_lo_bvec << 38);
		    *state = MD_DIR_SHARED; /* Shared Fine */
		} else { 
		    *state = mdplow.pde_lo_fmt.pde_lo_state;
		    *owner = mdplow.pde_lo_fmt.pde_lo_ptr;
		}
		*(elo) = mdplow.pd_lo_val;
	} else {
		mdslow.sd_lo_val = *(dir_addr_lo);
		mdshigh.sd_hi_val= *(dir_addr_hi);
		*state = mdslow.sde_lo_fmt.sde_lo_state;
		*(elo) = mdslow.sd_lo_val;
		if (*state == MD_DIR_SHARED) {
		        *owner = (mdslow.sds_lo_fmt.sds_lo_bvec << 11) | 
			         mdshigh.sd_hi_fmt.sd_hi_bvec;
		} else {
		        *owner = mdslow.sde_lo_fmt.sde_lo_ptr;
		}
	}
}

#endif /* SN0 */

memory_iscalias(paddr_t pa)
{
#if defined (SN0)
	hubreg_t calias_reg, calias_size;

	calias_reg = LOCAL_HUB_L(PI_CALIAS_SIZE);
	calias_size = calias_reg ? (0x1000 << (calias_reg - 1)) : 0;

	if (TO_NODE_ADDRSPACE(pa) < calias_size)
	    return 1;
#endif
	return 0;
}	



int
page_iscalias(pfn_t pfn)
{

	return memory_iscalias(ctob(pfn));
}


int
page_local_calias(pfn_t pfn)
{
#if defined (SN0)
	hubreg_t calias_reg, calias_size;

	calias_reg = LOCAL_HUB_L(PI_CALIAS_SIZE);
	calias_size = calias_reg ? (0x1000 << (calias_reg - 1)) : 0;

	if (ctob(pfn) < calias_size)
	    return 1;
#endif
	return 0;
}

/*
 * Check on the node where pfd is resident, if the fetchop cache entry 
 * is in the page pointed to by pfdat, and flush if it is. 
 * return 1 on success, 0 on failure. 
 * At this time, there is no reason for this routine to return failure. 
 */
#define	FETCHOP_FLUSH_OFFS	(4 << 3) 

#if defined (SN0)
int
fetchop_flush(struct pfdat *pfd)
{
	nasid_t nasid = 	pfdattonasid(pfd);
	__uint64_t		fop_cache_stat;
	__uint64_t		fop_page_addr;
	volatile __uint64_t	fop_cache_addr;

	fop_cache_stat = REMOTE_HUB_L(nasid, MD_FANDOP_CAC_STAT);

	if (fop_cache_stat & MFC_VALID_MASK){
		
		/* FANDOP cache is valid. Check if the cached address is
		 * in the page passed in. 
		 * MD_FANDOP_CAC_STAT register provides node local address. 
		 * Before comparing with page address generated via pfn,
		 * update fop_cache_addr to be true physical address.
		 */
		fop_cache_addr = fop_cache_stat & 
					(MFC_ADDR_MASK << MFC_ADDR_SHFT);
		fop_cache_addr = TO_NODE(nasid, fop_cache_addr);

		fop_page_addr = ctob(pfdattopfn(pfd));

		/*
	 	 * If cached address is in the same page as pfd,
		 * flush it.
		 */
		if ((fop_cache_addr >= fop_page_addr) && 
		    (fop_cache_addr < (fop_page_addr + NBPP)) ) { 
		
			/*
			 * Get MSPEC address for the cached address, and
			 * read from the FLUSH offset. 
			 */
			fop_cache_addr = TO_NODE_MSPEC(nasid, fop_cache_addr);
			fop_cache_addr += FETCHOP_FLUSH_OFFS;

			/* Read the flush offset */
			*(volatile __uint64_t *)fop_cache_addr; 
		}
#ifdef DEBUG
		/*
		 * Read it back and make sure it's flushed. 
		 * Note that the following check is valid ONLY if
		 * the page is not shared. Otherwise, the line could
		 * get reloaded into the HUB after it was purged &
		 * before the check is made.
		 */
		if (pfd->pf_use == 1) {
			fop_cache_stat = REMOTE_HUB_L(nasid, MD_FANDOP_CAC_STAT);

			if (fop_cache_stat & MFC_VALID_MASK) {
	
				fop_cache_addr = fop_cache_stat & 
					    	(MFC_ADDR_MASK << MFC_ADDR_SHFT);
				fop_cache_addr = TO_NODE(nasid, fop_cache_addr);
	
				ASSERT ( (fop_cache_addr < fop_page_addr) || 
				 	(fop_cache_addr >= (fop_page_addr + NBPP)));
			}
		}
#endif /* DEBUG */
	}
	
	return 1;

}
#endif
