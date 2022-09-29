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
/* 
 * pdsdwlk- Test the data path from memory through the secondary cache and
 * and to the Primary Data Cache.
 */
#ifdef TOAD

#include "bup.h"
#include "mach_ops.h"
#endif
#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];
uint pdsdwlk(__psunsigned_t, uint);

extern void IndxWBInv_SD(__psunsigned_t, uint, uint);

/* Defined by the compiler 'end' */
extern int error_data[4];
extern int end;

extern int scacherrx;
dsd_wlk()
{
	u_int retval;
	__psunsigned_t first_addr;
	__psunsigned_t osr;

	first_addr = PHYS_TO_K0(0x800000);
	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (dsd_wlk) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	osr = GetSR();
	if( scacherrx == NO)
		SetSR(osr | 0x10000);
	else
		SetSR(osr & 0xfffeffff);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* Do walking ones test */
	msg_printf( DBG, "Walking one\n");
	retval = pdsdwlk(first_addr, 0);
	/*while(startc < endc) { 
		startc += 0x10;
	}*/

	msg_printf( DBG, "Walking zero\n");
	retval = pdsdwlk(first_addr, 1);
	/* Do waling zero's test */
/*	while(startc < endc) { 
		startc += 0x10;
	}
*/

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary/secondary data path test\n");

#ifdef	TOAD
	BupHappy();
#else
	return(retval);
#endif
}

/*
 * This routine does the following
 *  1. Tag memory with a background pattern
 *  2. Invalidate the primary/secondary caches
 *  3. Load the data caches from the tagged memory (Clean Exclusive)
 *  4. Store new data into these locations (Dirty exclusive) 
 *  5. Unlock cpu 1
 *  6. check the data after cpu1 unlocks this code
 */
uint pdsdwlk(__psunsigned_t phypge, uint walking0)
{
	u_int actual[4];
	register u_int expected;
	register i;
	u_int j, retval=PASS;
	register u_int tmp;
	register u_int *adrptr;


	/* Memory address to use */
	adrptr = (u_int *)K0_TO_K1(phypge);

	/* Write walking ones pattern to memory, Kseg1 accesses */
	for( i=1, expected =1; i <= 32; i ++, expected <<= 1) {
		/* Set up K0 page pointer */
		adrptr = (u_int *)K0_TO_K1(phypge);

		/* walking zero pattern ? */
		if (walking0) {
			/* Load memory */
			*adrptr++ = ~expected;
			*adrptr++ = ~expected;
			*adrptr++ = ~expected;
			*adrptr++ = ~expected;
		}
		else {
			*adrptr++ = expected;
			*adrptr++ = expected;
			*adrptr++ = expected;
			*adrptr++ = expected;
		}
		

		/* Set primary and secondary to invalid state */
		/*InvCaches_all(); */
		HitInv_SD( phypge, 1, config_info.sidline_size);

		
		/* Set up K0 page pointer */
		adrptr = (u_int *)phypge;

		/*
	 	 * Load primary from the secondary using  mapped 
	 	 * cached accesses. Note, cache miss should cause a block access
		 * into the primary/secondary. 
	 	 */
		actual[0] = *adrptr++;
		actual[1] = *adrptr++;
		actual[2] = *adrptr++;
		actual[3] = *adrptr++;
		
		if (walking0) 
			tmp = ~expected;
		else
			tmp = expected;

		/* Set up K0 page pointer */
		adrptr = (u_int *)phypge;
		for(j=0; j < 4; j++) {
			if (tmp != actual[j]) {
				/*Eprintf("Data Path Error from Memory->Secondary->Primary Data\n");*/
				err_msg( DSDWLK_ERR1, cpu_loc);
				msg_printf( ERR, "Address %x, expected %x, actual %x, Xor %x\n",
					adrptr, tmp, actual[j], (expected ^ actual[j]) );
				retval = FAIL;
		
			}
			adrptr++;
			msg_printf( DBG, "expected %x actual %x\n", tmp, actual[j]);
		}
	}

	/*
	 * Now lets check the path out of the primary to the secondary into
	 * memory.
	 */


	/* Use a different pattern, reverse walking ones pattern to pcache */
	for( i=1, expected =0x80000000; i <= 32; i ++, expected >>= 1) {
		/* Set up virtual address */
		adrptr = (u_int *)phypge;

		/* Walking zero pattern */
		if (walking0)  {
			/* dirty pcache with new pattern */
			*adrptr ++ = ~expected;
			*adrptr ++ = ~expected;
			*adrptr ++ = ~expected;
			*adrptr ++ = ~expected;
		}
		else {
			*adrptr ++ = expected;
			*adrptr ++ = expected;
			*adrptr ++ = expected;
			*adrptr ++ = expected;
		}

		/*
	 	* Writeback Pcache to secondary and invalidate primary cache 
	 	* flush all 32 patterns 
	 	*/
		IndxWBInv_D( phypge, (32 *4)/config_info.dline_size,  config_info.dline_size);

		/* Now flush the secondary to memory and check the results */
		IndxWBInv_SD(phypge, ((32 * 4) /config_info.sidline_size),  config_info.sidline_size);

		/* Memory address were data was flushed to */
		adrptr = (u_int *)phypge;

		actual[0] = *adrptr++;
		actual[1] = *adrptr++;
		actual[2] = *adrptr++;
		actual[3] = *adrptr++;

		if (walking0)
			tmp = ~expected;
		else
			tmp = expected;

		/* Set up virtual address */
		adrptr = (u_int *)phypge;
		for(j=0; j < 4; j++) {
			if (tmp != actual[j]) {
				/*Eprintf("Data Path Error from Primary->Secondary->Memory\n");*/
				err_msg( DSDWLK_ERR2, cpu_loc);
				msg_printf( ERR, "Address %x, Expected %x, Actual %x, Xor %x\n",
				adrptr, tmp, actual[j], (expected ^ actual[j]) );
				retval = FAIL;
			}
			msg_printf( DBG, "expected %x actual %x\n", tmp, actual[j]);
			adrptr++;
		}
	}
	return(retval);
}

		
#ifdef TOAD
static printf()
{
	bupSad();
}
static BupHappy()
{
}
#endif
