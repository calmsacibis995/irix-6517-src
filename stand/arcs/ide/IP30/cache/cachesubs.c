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

#ident "$Revision: 1.11 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

jmp_buf cache_fault_buf;		/* fault buffer for all cache tests */

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
 * write_icache() -  Writes an instruction cache entry.
 */
void
write_icache_line(void *addr, int way, unsigned long data)
{
	__uint32_t dat[2];
        tag_regs_t tag;
	uint ecc = 0x0;
	uint *ptr;
	ulong i;

	data &= 0xfffffffff;		/* 36-bits */

	/* Fill I cacheline with the passed in data (includes pre-decode bits).
	 */
	for (ptr = (uint *)addr, i=(PIL_SIZE/4); i ;ptr++,i--) {
		/* make up some decode bits for 32:36 */
		uint *ucptr = (uint *)PHYS_TO_K1(KDM_TO_PHYS(ptr));
		dat[0] = data & 0xffffffff;
		dat[1] = (data>>32) & 0x0f;
		ecc = calc_r10k_parity(data);
		_write_cache_data(CACH_PI,(__psunsigned_t)ptr|way,dat,ecc);
	}

	/* Create tag */
	tag.tag_hi = (uint)(((__psunsigned_t)addr&PTAG1)>>TAGHI_PI_PTAG1_SHIFT);
	tag.tag_lo = (uint)(((__psunsigned_t)addr&PTAG0)>>TAGLO_PI_PTAG0_SHIFT);
	
	/* set parity bit? */
	if (calc_r10k_parity((__uint64_t)tag.tag_hi << 32 | tag.tag_lo))
	       tag.tag_lo |= TAGLO_PI_TP;

	/* Rest of tag has seperate parity */
	tag.tag_lo |= TAGLO_PI_LRU | TAGLO_PI_PSTATE | TAGLO_PI_SP;

	_write_tag(CACH_PI, PHYS_TO_K0(addr)|way, (void*)&tag);
}

/* read_icache: Read one word of i$ data
 */
unsigned long
read_icache_word(void *addr, int way)
{
        uint tag[2];

	_read_cache_data(CACH_PI, (ulong)addr|way, tag);

	return (((unsigned long)tag[1])<<32) | tag[0];
}

/* which    - fetch tags from this cache
 * physaddr - from the line containing this physical address
 * tag_info - and store the state and addr here
 */
get_tag_info(int which, __psunsigned_t physaddr, int way, tag_info_t *tag_info)
{
	struct tag_regs tags;	/* taghi and taglo regs */

	if (which != PRIMARYD && which != PRIMARYI && which != SECONDARY) {
		return(-2);
	}

	if (way)
		physaddr |= 1;

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
}


void
flushall_tlb(void)
{
	int i = 0;

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
	/* not quite right, but no where near as ugly as above */
	volatile uint *p = (volatile uint *)PHYS_TO_K0(physaddr);

	if (!sd_HWBINV(p))
		return(MISSED_2NDARY);

	*p;		/* read cacheline -- may have lost dirty bit... */

	return(0);
}
