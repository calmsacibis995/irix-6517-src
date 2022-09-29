/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.7 $"


/*=============================================================================
 *
 *	File Name: i_tagcmp.c
 *
 *	Diag name: i_tagcmp
 *
 *	Description: This diag tests the comparator at the I-cache tag for
 *		     hit and miss detection.
 *			1. Invalidate the entire cache.
 *			2. For each tag, set the ptag field with the values
 *			   which will cause a cache hit for the Kseg0 address
 *			   of 0x80002000 to 0x9fffffff. The values used are
 *			   a walking one or a walking zero pattern. This will
 *			   ensure only one bit location is tested at the
 *			   comparator.
 *			   For example, 0x000002, 0x000004, 0x000008, ...
 *			   0x01ffffe, 0x01ffffd etc.
 *			3. The cache op Hit Invalidate is used to check for
 *			   cache hit and miss situations.
 *			
 *===========================================================================*/


#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];
#define ADDR1	0xa0000000 - config_info.dcache_size
#define TAG_PTAG_SHIFT          0x00000008

void i_tagcmp_error(__psunsigned_t, uint, __psunsigned_t);

i_tagcmp()
{
	register __psunsigned_t addr;
	register u_int i;
	register u_int j;
	register u_int taglo;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_tagcmp) \n");
	msg_printf(VRB, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/*
	 * Do the comparator test to each cache line. (K0BASE + i) is the 
	 * addresses used to access the cache lines. "j" is the walking
	 * one pattern which is stored into the ptag field of the tag.
	 * "j" starts with the value of icache_size which should be 8k for
	 * aftershock. The following addresses will map to the same cache
	 * line location.
	 *	0x80000000, 0x80002000, 0x80004000, ... 0x9fffe000.
	 *	0x80000020, 0x80002020, 0x80004020, ... 0x9fffe020.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K0BASE + i;
		for(j = config_info.icache_size; j < 0x20000000; j = j << 1) {

			/* Store values into the ptag for addresses 0x80002xxx,
			 * 0x80004xxx, 0x80008xxx, ... with a dirty exclusive
			 * state. This creates a walking one pattern in the
			 * ptag field of the tag.
			 */
			taglo = (uint)(K0_TO_PHYS(addr | j) >> 12) << TAG_PTAG_SHIFT;
			msg_printf(DBG, "Walking 1 pattern 0x%x\n", taglo);

			IndxStorTag_I(addr, taglo | PState_Clean_Exclusive);

			/* Use 0x80000xxx address to compare with the ptag
			 * to generate a miss.
			 */
			HitInv_I(addr, 1, config_info.iline_size);
			if(ChkITag_CS(addr, 1, PState_Clean_Exclusive, config_info.iline_size)) {
				/*Eprintf("I-cache tag comparator did not detect a miss.\n");*/
				err_msg( ITAGCMP_ERR1, cpu_loc);
				i_tagcmp_error(addr, taglo >> TAG_PTAG_SHIFT,
						K0_TO_PHYS(addr) >> 12);
				retval = FAIL;
			}

			HitInv_I(addr | j, 1, config_info.iline_size);
			if(ChkITag_CS(addr, 1, PState_Invalid, config_info.iline_size)) {
				/*Eprintf("I-cache tag comparator did not detect a hit.\n");*/
				err_msg( ITAGCMP_ERR2, cpu_loc);
				i_tagcmp_error(addr, taglo >> TAG_PTAG_SHIFT,
						K0_TO_PHYS(addr | j) >> 12);
				retval = FAIL;
			}

		}
	}

	/*
	 * Repeat the above test but use a walking zero pattern.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = ADDR1 + i;
		for(j = config_info.icache_size; j < 0x20000000; j = j << 1) {

			/* Store values into the ptag for addresses 0x9fffcxxx,
			 * 0x9fffaxxx, 0x9fff6xxx, ... with a dirty exclusive
			 * state. This creates a walking zero pattern in the
			 * ptag field of the tag.
			 */
			taglo = (uint)(K0_TO_PHYS(addr & ~j) >> 12) << TAG_PTAG_SHIFT;
			msg_printf(DBG, "Walking 0 pattern 0x%x\n", taglo);
			IndxStorTag_I(addr, taglo | PState_Clean_Exclusive);

			HitInv_I(addr, 1, config_info.iline_size);
			if(ChkITag_CS(addr, 1, PState_Clean_Exclusive, config_info.iline_size)) {
				/*Eprintf("I-cache tag comparator did not detect a miss.\n");*/
				err_msg( ITAGCMP_ERR3, cpu_loc);
				i_tagcmp_error(addr, taglo >> TAG_PTAG_SHIFT,
						K0_TO_PHYS(addr) >> 12);
				retval = FAIL;
			}

			HitInv_I(addr & ~j, 1, config_info.iline_size);
			if(ChkITag_CS(addr, 1, PState_Invalid, config_info.iline_size)) {
				/*Eprintf("I-cache tag comparator did not detect a hit.\n");*/
				err_msg( ITAGCMP_ERR4, cpu_loc);
				i_tagcmp_error(addr, taglo >> TAG_PTAG_SHIFT,
						K0_TO_PHYS(addr & ~j) >> 12);
				retval = FAIL;
			}

		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction TAG RAM comparitor test\n");
	return(retval);
}


void i_tagcmp_error(__psunsigned_t tag_index_addr, uint ptag, __psunsigned_t pfn)
{
	msg_printf( ERR, "Tag ram address: 0x%08x\n", tag_index_addr);
	msg_printf( ERR, "PTag field of tag: 0x%06x comparing with PFN: 0x%06x.\n",
			ptag, pfn);
}

