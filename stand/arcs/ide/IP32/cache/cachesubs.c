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

#ident "$Revision: 1.3 $"


#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

static uint read_word(enum mem_space, uint);

/* returns -1 if called with invalid state */
set_tag(which, state, phys_addr)
  int which;	/* PRIMARYD, PRIMARYI, or SECONDARY */
  enum c_states state;
  uint phys_addr;	/* < MAX_PHYS_MEM */
{
	struct tag_regs tags;
	uint value;

	read_tag(which, phys_addr, &tags);

	/* clear old state bits */
	if (which == SECONDARY)
		tags.tag_lo &= ~SSTATEMASK;
	else
		tags.tag_lo &= ~PSTATEMASK;

	switch(state) {
		case INVALIDL:
			break;	/* above clearing did the trick */
		case CLEAN_EXCL:
			if (which == SECONDARY) {
				tags.tag_lo |= SCLEANEXCL;
			}
			else {
				/* PCLEANEXCL sets V bit in P I cache */
				tags.tag_lo |= PCLEANEXCL;
			}
			break;
		case DIRTY_EXCL:
		case DIRTY_EXCL_NWB:
			if (which == PRIMARYI) {
				return(-1);
			}
			if (which == SECONDARY) {
				tags.tag_lo |= SDIRTYEXCL;
			}
			else {
				tags.tag_lo |= PDIRTYEXCL;
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


/*
 * flush2ndline(physaddress)
 * Flush 2ndary cache-line at specified address to memory.  To provide
 * calling routine with a sanity-check, return the value of the CH bit
 * in the diagnostic portion of the status register (set if the cache-flush
 * instruction 'hit' secondary).  This routine is non-destructive; i.e.
 * the contents and state of the flushed secondary line is unchanged.
 */
flush2ndline(physaddr)
  uint physaddr;	/* flush line in second-level cache which contains
			 * this physical address */
{
	struct tag_regs oldPtag, oldStag;
	uint caddr = PHYS_TO_K0(physaddr);

	/* hit writeback cache instruction on secondary will try to
	 * get newer data from the primary cache if it is dirty unless
	 * we temporarily mark it invalid. */
	read_tag(PRIMARYD, physaddr, &oldPtag);	/* save real tags */
	set_tag(PRIMARYD, INVALIDL, physaddr);

	/* secondary must be DE to flush */
	read_tag(SECONDARY, physaddr, &oldStag);
	set_tag(SECONDARY, DIRTY_EXCL, physaddr);
	
	if (!sd_HWB(caddr))		/* instr didn't hit secondary?? */
		return(MISSED_2NDARY);

	/* restore previous states */
	write_tag(PRIMARYD, physaddr, &oldPtag);
	write_tag(SECONDARY, physaddr, &oldStag);

	return(0);
}


/* given a physical address, read_word converts it to its equivalent
 * in the specified kernel space and reads the value there.
 *
 *	accessmode - k0seg, k1seg, or k2seg.
 *	phys_addr - read from this address.
 */
static uint
read_word(enum mem_space accessmode, uint phys_addr)
{
	uint vaddr;
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
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = phys_addr+K2BASE;
			break;
	}

#ifdef R4K_UNCACHED_BUG
	uval = *(uint *)vaddr;
	if (accessmode == k1seg) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
	return (uval);
#else
	return (*(uint *)vaddr);
#endif

} /* end fn read_word */


/* given a physical address, write_word converts it to its equivalent
 * in the specified kernel space and stores value there.
 */
write_word(accessmode, phys_addr, value)
  enum mem_space accessmode;	/* k0seg, k1seg, or k2seg */
  uint phys_addr;		/* write to this address */
  uint value;
{
	uint vaddr;
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
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = phys_addr+K2BASE;
			break;
	}
	*(uint *)vaddr = value;

#ifdef R4K_UNCACHED_BUG
	if (accessmode == k1seg) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
#endif

} /* end fn write_word */

get_tag_info(which, physaddr, tag_info)
  int which;		/* fetch tags from this cache */
  uint physaddr;	/* from the line containing this physical address */
  tag_info_t *tag_info;	/* and store the state and addr here */
{
	struct tag_regs tags;	/* taghi and taglo regs */

	if (which != PRIMARYD && which != PRIMARYI && which != SECONDARY) {
		return(-2);
	}

	read_tag(which, physaddr, &tags);

	if (which == SECONDARY) {
		tag_info->state = ((tags.tag_lo & SSTATEMASK) >> SSTATE_RROLL);
		tag_info->vindex = ((tags.tag_lo&SVINDEXMASK) << SVINDEXLROLL);
		tag_info->physaddr = (tags.tag_lo&SADDRMASK) << TAGADDRLROLL;
	} else { /* PRIMARYI or PRIMARYD */
		tag_info->state = ((tags.tag_lo & PSTATEMASK) >> PSTATE_RROLL);
		/* cache paddr tops at bit 35.  we want bit 31 at top of word */
		tag_info->physaddr = (tags.tag_lo&PADDRMASK) << TAGADDRLROLL;
	}

	return(0);
} /* get_tag_info */


flushall_tlb()
{
	int i, n_tlb_entries, tmp;
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	if (!r5000)
		n_tlb_entries = R10K_NTLBENTRIES;
	else
		n_tlb_entries = R4K_NTLBENTRIES;

	i=0;
	do {
		invaltlb(i);
		i++;
	} while ( i != n_tlb_entries );
}
