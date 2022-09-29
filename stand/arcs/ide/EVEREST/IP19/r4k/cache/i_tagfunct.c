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

#ident "$Revision: 1.6 $"


/*=============================================================================
 *
 *	File Name: i_tagfunct.c
 *
 *	Diag name: i_tagfunct
 *
 *	Description: This diag tests the functionality of the instruction
 *		     cache tag. Kseg0 addresses are used to load the cache 
 *		     from memory. This will test if the cache is functional
 *		     on the cachable memory space. 
 *		     After each 8k segment of memory is loaded into the cache.
 *		     The ptag and the cache state field are 
 *		     checked to see if they are holding expected values.
 *		     Virtual addresses 0x80000000, 0x80002000, 0x80004000,
 *		     0x80008000, ..., 0x90000000 are used as the base 
 *		     address of each 8k page which is mapped to the cache. 
 *		     The ptag and cache state of each cache line are checked 
 *		     against the expected value.
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
#define TAG_PTAG_SHIFT          0x00000008
#define TAG_PSTATE_MASK         0x000000c0


i_tagfunct()
{
	extern int end;
	register __psunsigned_t addr;
	register __psunsigned_t baseaddr;
	register u_int i;
	register u_int temp;
	register u_int taglo;
	register u_int j;
	__psunsigned_t last_loc, first_loc, osr;
	extern u_int scacherrx;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_tagfunct) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	first_loc = PHYS_TO_K1(0x800000);
	last_loc = PHYS_TO_K1(first_loc + 0x1000000);

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);

	last_loc = last_loc - config_info.iline_size;
	msg_printf( DBG, "last location %x\n", last_loc);
	/*
	 * Use a walking one offset on the K0BASE address to obtain the
	 * base address of an 8k page which mapped into the 8k cache.
	 */
	for(baseaddr = K1_TO_K0(first_loc), j = config_info.icache_size; baseaddr < K1_TO_K0(last_loc); j = j * 2, baseaddr = K1_TO_K0(first_loc) + j){

		/* Invalidate the cache before each test case.
	 	*/
		ide_invalidate_caches();

		/* For each cache line, check the ptag and the cache state
		 * field against the expected value.
		 */
		for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
			addr = baseaddr + i;
			Fill_Icache(addr, 1, config_info.iline_size);
			taglo = IndxLoadTag_I(addr);
			temp = (uint)(K0_TO_PHYS(addr) >> 12);
			/*
			 * If the ptag field does not contain the expected
			 * pfn, the tag is not functional.
			 */
			if ( (taglo >> TAG_PTAG_SHIFT) != temp) {
				/*Eprintf("I-cache tag functional error.\n");*/
				err_msg( ITAGFUNC_ERR1, cpu_loc);
				msg_printf( ERR, "PTag field does not contain correct tag bits.\n");
				msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
				msg_printf( ERR, "Expected PTag: 0x%06x\n", temp);
				msg_printf( ERR, "Actual PTag: 0x%06x\n", taglo >> TAG_PTAG_SHIFT);
				retval = FAIL;
			}

			/*
			 * Check cache state. Without a secondary cache, the
			 * cach can only have two states: invalid and clean
			 * exclusive.
			 */
			if((taglo & TAG_PSTATE_MASK)!= PState_Clean_Exclusive){
				/*Eprintf("I-cache tag functional error.\n");*/
				err_msg( ITAGFUNC_ERR2, cpu_loc);
				msg_printf( ERR, "Cache state not correct.\n");
				msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
				msg_printf( ERR, "Expected cache state: 0x%08x\n",
						PState_Clean_Exclusive);
				msg_printf( ERR, "Actual cache state: 0x%08x\n", 
						taglo & TAG_PSTATE_MASK);
				retval = FAIL;
			}
			
		}
	}

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction TAG functionality test\n");
	return(retval);
}

