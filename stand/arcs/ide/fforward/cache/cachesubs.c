/*
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.10 $"


#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

static uint read_word(enum mem_space, __psunsigned_t);
void write_word(enum mem_space accessmode, __psunsigned_t phys_addr, uint value);

/* returns -1 if called with invalid state
 *	which is PRIMARYD, PRIMARYI, or SECONDARY
 *	phys_addr < MAX_PHYS_MEM
 */
set_tag(int which, enum c_states state, __psunsigned_t phys_addr)
{
	struct tag_regs tags;
	uint value;

	read_tag(which, phys_addr, &tags);

	/* clear old state bits */
	switch(which) {
 	        case SECONDARY:
#if R4000
 	                tags.tag_lo &= ~SSTATEMASK;	           
#elif R10000
 	                tags.tag_lo &= ~TAGLO_SC_SSTATE;	           
#endif
			break;
		case PRIMARYD:
#if R4000
			tags.tag_lo &= ~PSTATEMASK;
#elif R10000
			tags.tag_lo &= ~TAGLO_PD_PSTATE;
#endif
			break;
		case PRIMARYI:
#if R4000
			tags.tag_lo &= ~PSTATEMASK;
#elif R10000
			tags.tag_lo &= ~TAGLO_PI_PSTATE;
#endif
			break;
	}

	switch(state) {
		case INVALIDL:
			break;	/* above clearing did the trick */
		case CLEAN_EXCL:
			if (which == SECONDARY) {
#if R4000
				tags.tag_lo |= SCLEANEXCL;
#elif R10000
				tags.tag_lo |= TAGLO_SC_SSTATE_CLEANEXCL;
#endif
			}
			else {
#if R4000
				/* PCLEANEXCL sets V bit in P I cache */
				tags.tag_lo |= PCLEANEXCL;
#elif R10000
				/* does nothing for P I cache */
				tags.tag_lo |= TAGLO_PD_PSTATE_CLEANEXCL;
#endif
			}
			break;
		case DIRTY_EXCL:
		case DIRTY_EXCL_NWB:
			if (which == PRIMARYI) {
				return(-1);
			}
			if (which == SECONDARY) {
#if R4000
				tags.tag_lo |= SDIRTYEXCL;
#elif R10000
				tags.tag_lo |= TAGLO_SC_SSTATE_DIRTYEXCL;
#endif
			}
			else {
#if R4000
				tags.tag_lo |= PDIRTYEXCL;
#elif R10000
				tags.tag_lo |= TAGLO_PD_PSTATE_DIRTYEXCL;
#endif
			}
			break;
	}
	write_tag(which, phys_addr, &tags);

	/* if primary data cache and need dirty exclusive state, we must
	 * set the writeback bit */
	if ( (state == DIRTY_EXCL) && (which == PRIMARYD) ) {
		/* read then write it--this sets 'W' bit w/o changing value */
		value = read_word(k0seg, phys_addr);
		write_word(k0seg, phys_addr, value);
	}
	return(0);
} /* fn set_tag */

/* given a physical address, read_word converts it to its equivalent
 * in the specified kernel space and reads the value there.
 *
 *	accessmode - k0seg, k1seg, or k2seg.
 *	phys_addr - read from this address.
 */
static uint
read_word(enum mem_space accessmode, __psunsigned_t phys_addr)
{
	uint *vaddr;
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;
	uint uval;

	/* if reading through k1seg, must invalidate line to
	 * force R4K to read from memory, not cache, due to bug */
	if (accessmode == k1seg) {
		TagZero.tag_lo = 0;
		read_tag(PRIMARYI, phys_addr, &PITagSave);
		read_tag(PRIMARYD, phys_addr, &PDTagSave);
		read_tag(SECONDARY, phys_addr, &STagSave);
		write_tag(PRIMARYI, phys_addr, &TagZero);
		write_tag(PRIMARYD, phys_addr, &TagZero);
		write_tag(SECONDARY, phys_addr, &TagZero);
	}
#endif

	switch(accessmode) {
	case k0seg:
			vaddr = (uint *)PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = (uint *)PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = (uint *)(phys_addr+K2BASE);
			break;
	}

#ifdef R4K_UNCACHED_BUG
	uval = *vaddr;
	if (accessmode == k1seg) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
	return (uval);
#else
	return (*vaddr);
#endif

} /* end fn read_word */


/* given a physical address, write_word converts it to its equivalent
 * in the specified kernel space and stores value there.
 *
 *	accessmode - k0seg, k1seg, or k2seg
 *	phys_addr  - write to this address
 */
void
write_word(enum mem_space accessmode, __psunsigned_t phys_addr, uint value)
{
	uint *vaddr;
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;

	/* if writing through k1seg, must invalidate line to
	 * force R4K to write to memory, not cache, due to bug */
	if (accessmode == k1seg) {
		TagZero.tag_lo = 0;
		read_tag(PRIMARYI, phys_addr, &PITagSave);
		read_tag(PRIMARYD, phys_addr, &PDTagSave);
		read_tag(SECONDARY, phys_addr, &STagSave);
		write_tag(PRIMARYI, phys_addr, &TagZero);
		write_tag(PRIMARYD, phys_addr, &TagZero);
		write_tag(SECONDARY, phys_addr, &TagZero);
	}
#endif

	switch(accessmode) {
	case k0seg:
			vaddr = (uint *)PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = (uint *)PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = (uint *)(phys_addr+K2BASE);
			break;
	}
	*vaddr = value;

#ifdef R4K_UNCACHED_BUG
	if (accessmode == k1seg) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
#endif

} /* end fn write_word */

#ifdef R10000

/* Routines needed For compatibility with R4k tests */

/*
 * calc_r10k_parity -- even parity over 64 bits.
 */
int
calc_r10k_parity(__uint64_t value)
{
	int num = 0;
	int i;

	for (i=0 ; i < 64 ; i++) {
		if ( ((value >> i) & 1 ) == 1)
			num++;
	}
  
	/* even parity, so if odd number of 1, parity bit is 1 */
	return(num & 1);
}

/*
 * write_icache_- simple routine that writes an 
 *                instruction cache entry with
 */
static void
write_icache(__psunsigned_t addr)
{
	__uint32_t dat[2];
        tag_regs_t tag;
	uint ecc = 0x0;
	uint *ptr;
	ulong i;
	int way;

	/* Hack to test 2nd way. */
	if ((KDM_TO_PHYS(addr)&(PI_SIZE-1)) >= (PI_SIZE/2))
		way = 1;
	else
		way = 0;

	/* Fill I cacheline with data, making up decode bits from nibble
	 * of data pattern.
	 */
	for (ptr = (uint *)addr, i=(PIL_SIZE/4); i ;ptr++,i--) {
		/* make up some decode bits for 32:36 */
		uint *ucptr = (uint *)PHYS_TO_K1(KDM_TO_PHYS(ptr));
		dat[0] = ucptr[0];
		dat[1] = dat[0]&0x0f;
		ecc = calc_r10k_parity((((__uint64_t)dat[1]) << 32) | dat[0]);
		_write_cache_data(CACH_PI,(ulong)ptr|way,dat,ecc);
	}

	/* Create tag */
	tag.tag_hi = (uint) ((addr & PTAG1) >> TAGHI_PI_PTAG1_SHIFT);
	tag.tag_lo = (uint) ((addr & PTAG0) >> TAGLO_PI_PTAG0_SHIFT);
	
	/* set parity bit? */
	if (calc_r10k_parity((__uint64_t)tag.tag_hi << 32 | tag.tag_lo))
	       tag.tag_lo |= TAGLO_PI_TP;

	/* Rest of tag has seperate parity */
	tag.tag_lo |= TAGLO_PI_LRU | TAGLO_PI_PSTATE | TAGLO_PI_SP;

	_write_tag(CACH_PI, PHYS_TO_K0(addr)|way, (void*)&tag);
}

/*
 * fill_ipline: emulates the R4k fill_ipline routine
 *              which simply uses the R4k Fill CacheOp for PI
 *
 */
void
fill_ipline(volatile uint *addr)
{
        write_icache((__psunsigned_t)addr);
}

/*
 * write_ipline: emulates the R4k write_ipline routine
 *               which simply uses the R4k Hit Write Back CacheOp for PI
 *
 */
void
write_ipline(volatile uint* addr)
{
        uint tag[2];
	ulong way;
	int i;

	/* Hack to test 2nd way. */
	if ((KDM_TO_PHYS(addr)&(PI_SIZE-1)) >= (PI_SIZE/2))
		way = 1;
	else
		way = 0;
	
	/* write back to the cache */
	i = (int)(PIL_SIZE / sizeof(uint));
	while (i--) {
	       /* emulate it by just writting the data */
	       _read_cache_data(CACH_PI, (ulong)addr|way, tag);
	       *addr = tag[0];		/* drop decode bits */
	       addr++;
        }
}

#endif          /* R10k cache tests only */

#if R4000	/* R4k cache tests  only */

/* which    - fetch tags from this cache
 * physaddr - from the line containing this physical address
 * tag_info - and store the state and addr here
 */
get_tag_info(int which, __psunsigned_t physaddr, tag_info_t *tag_info)
{
	struct tag_regs tags;	/* taghi and taglo regs */

	if (which != PRIMARYD && which != PRIMARYI && which != SECONDARY) {
		return(-2);
	}

	read_tag(which, physaddr, &tags);

	if (which == SECONDARY) {
		tag_info->state = ((tags.tag_lo & SSTATEMASK) >> SSTATE_RROLL);
		tag_info->vindex = ((tags.tag_lo&SVINDEXMASK) << SVINDEXLROLL);
		tag_info->physaddr = (caddr_t)
			(((__psint_t)tags.tag_lo&SADDRMASK) << TAGADDRLROLL);
	} else { /* PRIMARYI or PRIMARYD */
		tag_info->state = ((tags.tag_lo & PSTATEMASK) >> PSTATE_RROLL);
		/* cache paddr tops at bit 35.  we want bit 31 at top of word */
		tag_info->physaddr = (caddr_t)
			(((__psint_t)tags.tag_lo&PADDRMASK) << TAGADDRLROLL);
	}

	return(0);
} /* get_tag_info */

#elif R10000

/* which    - fetch tags from this cache
 * physaddr - from the line containing this physical address
 * tag_info - and store the state and addr here
 */
get_tag_info(int which, __psunsigned_t physaddr, tag_info_t *tag_info)
{
	struct tag_regs tags;	/* taghi and taglo regs */

	if (which != PRIMARYD && which != PRIMARYI && which != SECONDARY) {
		return(-2);
	}

	read_tag(which, physaddr, &tags);

	if (which == SECONDARY) {
		tag_info->state = ((tags.tag_lo & TAGLO_SC_SSTATE) >> 
				   TAGLO_SC_SSTATE_SHIFT);
		tag_info->vindex = ((tags.tag_lo & TAGLO_SC_VINDEX) >> 
				    TAGLO_SC_VINDEX_SHIFT);
		tag_info->physaddr = (caddr_t)
		   (   (((__psint_t)tags.tag_lo & TAGLO_SC_STAG0) >> TAGLO_SC_STAG0_SHIFT)
		     | (((__psint_t)tags.tag_hi & TAGHI_SC_STAG1) << TAGHI_SC_STAG1_SHIFT));
	} else if (which == PRIMARYD) {
		tag_info->state = ((tags.tag_lo & TAGLO_PD_PSTATE) >> 
				   TAGLO_PD_PSTATE_SHIFT);
		tag_info->physaddr = (caddr_t)
		    (  (((__psint_t)tags.tag_lo & TAGLO_PD_PTAG0) >> TAGLO_PD_PTAG0_SHIFT)
		     | (((__psint_t)tags.tag_hi & TAGHI_PD_PTAG1) << TAGHI_PD_PTAG1_SHIFT));
	} else if (which == PRIMARYI) {
		tag_info->state = ((tags.tag_lo & TAGLO_PI_PSTATE) >> 
				   TAGLO_PI_PSTATE_SHIFT);
		tag_info->physaddr = (caddr_t)
		    (  (((__psint_t)tags.tag_lo & TAGLO_PI_PTAG0) >> TAGLO_PI_PTAG0_SHIFT_2)
		     | (((__psint_t)tags.tag_hi & TAGHI_PI_PTAG1) << TAGHI_PI_PTAG1_SHIFT));
	}

	return(0);
} /* get_tag_info */

#endif 	/* R10k cache tests  only */

#if R4000 || R10000	/* R4k & R10k cache tests */

void
flushall_tlb(void)
{
	int i;

	i=0;
	do {
		invaltlb(i);
		i++;
	} while ( i != NTLBENTRIES );
}

/*
 * flush2ndline(physaddress)
 * Flush 2ndary cache-line at specified address to memory.  To provide
 * calling routine with a sanity-check, return the value of the CH bit
 * in the diagnostic portion of the status register (set if the cache-flush
 * instruction 'hit' secondary).  This routine is non-destructive; i.e.
 * the contents and state of the flushed secondary line is unchanged.
 */
flush2ndline(__psunsigned_t physaddr)
{
#ifdef R4000
	struct tag_regs oldPtag, oldStag;
	__psunsigned_t caddr = PHYS_TO_K0(physaddr);

	/* hit writeback cache instruction on secondary will try to
	 * get newer data from the primary cache if it is dirty unless
	 * we temporarily mark it invalid. */
	read_tag(PRIMARYD, physaddr, &oldPtag);	/* save real tags */
	set_tag(PRIMARYD, INVALIDL, physaddr);

	/* secondary must be DE to flush */
	read_tag(SECONDARY, physaddr, &oldStag);
	set_tag(SECONDARY, DIRTY_EXCL, physaddr);

	if (!sd_HWB((void *)caddr))	/* instr didn't hit secondary?? */
		return(MISSED_2NDARY);

	/* restore previous states */
	write_tag(PRIMARYD, physaddr, &oldPtag);
	write_tag(SECONDARY, physaddr, &oldStag);
#elif R10000
	/* not quite right, but no where near as ugly as above */
	volatile uint *p = (volatile uint *)PHYS_TO_K0(physaddr);

	if (!sd_HWBINV(p))
		return(MISSED_2NDARY);

	*p;		/* read cacheline -- may have lost dirty bit... */
#endif

	return(0);
}

#endif /* R4k & R10k cache tests */
