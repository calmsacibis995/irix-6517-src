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

#ident "$Revision: 1.5 $"


/*=============================================================================
 *
 *	File Name: d_tagfunct.c
 *
 *	Diag name: d_tagfunct
 *
 *	Description: This diag tests the functionality of the data cache tag.
 *		     Kseg0 addresses are used to load the cache from memory. 
 *		     The ptag and the cache state field are checked to see if
 *		     they are holding expected values. Virtual addresses
 *		     0x80000000, 0x80002000, 0x80004000, 0x80008000, ...
 *		     0x90000000 are used as the baseaddress of an 8k
 *		     page which is mapped to the cache. The ptag and state
 *		     of each cache line are checked against the expected
 *		     value.
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


d_tagfunct()
{
	extern int end;
	volatile __psunsigned_t addr;
	register __psunsigned_t baseaddr;
	register u_int i;
	volatile __psunsigned_t temp;
	register u_int taglo;
	register u_int j;
	u_int cache_state, retval;
	__psunsigned_t first_loc, last_loc;
	extern u_int scacherrx;
	__psunsigned_t osr;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_tagfunct) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	first_loc = PHYS_TO_K1(0x800000);
	last_loc = PHYS_TO_K1(0x1000000);

	msg_printf( DBG, "first 0x%8x, last 0x%8x\n", first_loc, last_loc);

	cache_state = PState_Clean_Exclusive;

	/*
	 * Use a walking one offset on the K0BASE address to obtain the
	 * base address of an 8k page which mapped into the cache.
	 */
	for(baseaddr = K1_TO_K0(first_loc), j = config_info.dcache_size; baseaddr < K1_TO_K0(last_loc);
					j = j * 2, baseaddr = K1_TO_K0(first_loc) + j){

		/* Invalidate the cache before each test case.
	 	*/
		ide_invalidate_caches();

		/* For each cache line, check the ptag and the cache state
		 * field against the expected value.
		 */
		msg_printf( DBG, "baseaddr %x\n", baseaddr);
		for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
			addr = baseaddr + i;
			temp = *(u_int *)addr;
			taglo = IndxLoadTag_D(addr);
			temp = K0_TO_PHYS(addr) >> 12;
			/*
			 * If the ptag field does not contain the expected
			 * pfn, the tag is not functional.
			 */
			if ((taglo >> TAG_PTAG_SHIFT) != temp) {
				/*Eprintf("D-cache tag functional error.\n");*/
				err_msg( DTAGFUNCT_ERR1, cpu_loc);
				msg_printf( ERR, "PTag field does not contain correct tag bits.\n");
				msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
				msg_printf( ERR, "Expected PTag: 0x%06x\n", temp);
				msg_printf( ERR, "Actual PTag: 0x%06x\n", taglo >> TAG_PTAG_SHIFT);
				msg_printf( ERR, "TAGLO Register %x\n", taglo);
				msg_printf( ERR, "Re-read DTAG %x\n", IndxLoadTag_D(addr));
				msg_printf( ERR, "STAG %x\n", IndxLoadTag_SD(addr));
				retval = FAIL;
			}

			/*
			 * Check cache state. Without a secondary cache, the
			 * cach can only have two states: invalid and dirty
			 * exclusive.
			 */
			if((taglo & TAG_PSTATE_MASK)!= cache_state ){
				/*Eprintf("D-cache tag functional error.\n");*/
				err_msg( DTAGFUNCT_ERR2, cpu_loc);
				msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
				msg_printf( ERR, "Expected cache state: 0x%08x\n",
						cache_state);
				msg_printf( ERR, "Actual cache state: 0x%08x\n", 
						taglo & TAG_PSTATE_MASK);
				msg_printf( ERR, "TAGLO Register %x\n", taglo);
				msg_printf( ERR, "Re-read DTAG %x\n", IndxLoadTag_D(addr));
				msg_printf( ERR, "STAG %x\n", IndxLoadTag_SD(addr));
				retval = FAIL;
			}
			
		}
	}

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data TAG functionality test\n");
	return(retval);
}

