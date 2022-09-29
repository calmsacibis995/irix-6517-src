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
 *	This test verifies the Hit Writeback Cache operation.
 *	It verifies that the data can be written back from the Secondary
 *	or in the case where the primary data is more current that the
 *	data is written from the Primary to memory. Also checked is the
 *	fact that the cache lines are not invalidated as with the Hit
 *	Writeback Invalidate Cache Op. Instead in checks that the lines
 * 	is set to the clean exclusive state.
 *
 *	7/17/91: Richard Frias wrote the original version.
 */

#ifdef	TOAD
#include "bup.h"
#include "types.h"
#include "mach_ops.h"

#else
#include "sys/types.h"

#endif	/* TOAD */
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

/* 'end' defined by the compiler as the end of your code */
extern u_int end;

/* Set address to beyond 8 M */
#define TSTADR 0x800000
#define ALIASADR (TSTADR + config_info.sidcache_size)


#define ADDR1	TSTADR	

/* Address that will overwrite addr1 in the primary cache*/
#define ADDR2	(TSTADR + config_info.dcache_size)

#define	K0ADDR1	PHYS_TO_K0(ADDR1)

#define	K1ADDR1	PHYS_TO_K1(ADDR1)

#define	K0ADDR2	PHYS_TO_K0(ADDR2)
#define	K1ADDR2	PHYS_TO_K1(ADDR2)

#define SC_LINES_UT 100

extern int error_data[4];
extern int scache_linesize;
static u_int lastadr;


sd_hitwb()
{
	register volatile u_int data;
	register volatile u_int *addr;
	register u_int tmp;
	register u_int dlines;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_hitwb) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* Set global for subroutine */
	scache_linesize= config_info.sidline_size;

	/* determine the number of primary lines under test */
	dlines = (config_info.sidline_size/config_info.dline_size) * SC_LINES_UT;
	
	/* Compute the end address based on the line size*/
	lastadr = (config_info.sidline_size * SC_LINES_UT);

	/* init K1 (memory)  uncached space */
	for (addr = (u_int *)K1ADDR1;addr < (u_int *)(K1ADDR1+ lastadr);addr++) {
		*addr = 0x5aa50f0f;
	}

	/*
	 * write to K0 cached space with unique data. IMPLIED READ of cache
	 * block before write, must be owner before we can dirty the line 
	 */
	data = 0;
	for (addr = (u_int *)K0ADDR1; addr < (u_int *)(K0ADDR1+ lastadr);
			addr++,data += 0x11111111) {

		/* Dirty cache line */
		*addr = data;
	}

	/* Verify memory has the background data, not a write through cache*/
	for (addr = (u_int *)K1ADDR1; addr < (u_int *)(K1ADDR1+ lastadr); addr++) {
		if (*addr != 0x5aa50f0f) {
			/*Eprintf("Unexpected Cache write through to memory\n");*/
			err_msg( SDHITWB_ERR1, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR %x\n",
				0x5aa50f0f, *addr, *addr ^ 0x5aa50f0f );
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}

	/* init some memory that will alias the cache lines already in the cache */
	for (addr = (u_int *)K1ADDR2; addr < (u_int *)(K1ADDR2+ lastadr); addr++) {
		*addr = 0xdeadbeef;
	}

	/* This read will overwrite the dirty primary cache line. In doing so
	 * forces a writeback of this dirty line to the secondary.
	 */
	for(addr = (u_int *)K0ADDR2; addr < (u_int *)(K0ADDR2 + lastadr); ) {
		data = *addr++;
	}

	/* At this point, memory, primary, and secondary all have different
	 * data patterns. Do the writeback and make sure the data written
	 * back comes from the secondary cache.
	 */
	HitWB_SD(K0ADDR1, SC_LINES_UT, config_info.sidline_size );

	/* Check the secondary tags for a dirty excl state. Note this cache
	 * op does not change the state in the primary or secondary.
	 */
	if (tmp= ChkSdTag( ADDR1,SC_LINES_UT,SCState_Dirty_Exclusive, K0ADDR1) ) {
		/*Eprintf("SCache error during Primary Cache dirty line writeback to Scache\n");*/
		err_msg( SDHITWB_ERR2, cpu_loc);
                scacherr( tmp);
                retval = FAIL;
	}  

	/* Check memory to ensure that data was written back to memory 
	 * from the secondary cache correctly.
 	 */
	data = 0;
	for (addr = (u_int *)K1ADDR1; addr < (u_int *)(K1ADDR1 + lastadr);
			addr++,data += 0x11111111) {
		if (*addr != data) {
			/*Eprintf("Data not written back from Scache to Memory\n");*/
			err_msg( SDHITWB_ERR3, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR %x\n",
				data, *addr, *addr ^ data);
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}


	/* Test that Hit Writeback on the secondary takes data
	 * from the primary, if the data is more current (W bit set).
	 */ 

	/* init K1 (memory)  uncached space */
	for (addr = (u_int *)K1ADDR1;addr < (u_int *)(K1ADDR1+ lastadr);addr++) {
		*addr = 0x5aa50f0f;
	}

	/*
	 * write to K0 cached space with unique data. IMPLIED READ of cache
	 * block before write, must be owner before we can dirty the line 
	 */
	data = 0;
	for (addr = (u_int *)K0ADDR1; addr < (u_int *)(K0ADDR1+ lastadr);
			addr++,data += 0x11111111) {

		/* Dirty cache line */
		*addr = data;
	}

	/* Verify memory still has the background data, not a write through cache */
	for (addr = (u_int *)K1ADDR1; addr < (u_int *)(K1ADDR1 + lastadr); addr++) {
		if (*addr != 0x5aa50f0f) {
			/*Eprintf("Unexpected Cache write through to memory1\n");*/
			err_msg( SDHITWB_ERR4, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR %x\n",
				0x5aa50f0f, *addr, *addr ^ 0x5aa50f0f );
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}

	/* Secondary has the 0x5aa50f0f pattern so make memory different
	 * so we can tell if the secondary is written back to memory
	 */
	for (addr = (u_int *)K1ADDR1;addr < (u_int *)(K1ADDR1+ lastadr);addr++) {
		*addr = 0xdeadbeef;
	}

	/*
	 * At this point, memory, primary, and secondary all have different
	 * data patterns. Do the writeback and make sure the data written
	 * back comes from the primary since the dirty data has not been
	 * written back to the secondary cache.
	 */
	HitWB_SD(K0ADDR1, SC_LINES_UT, config_info.sidline_size );

	/* Check the secondary tags for a dirty exclusive state */
	if (tmp= ChkSdTag( ADDR1, SC_LINES_UT, SCState_Dirty_Exclusive, K0ADDR1) ) {
		/*Eprintf("SCache error during Hit Writeback on dirty line \n");*/
		err_msg( SDHITWB_ERR5, cpu_loc);
                scacherr( tmp);
		retval = FAIL;
	}  

	/* Check the primary tags for an dirty state, above cache op should
	 * not have invalidated the primary.
	 */
	if (tmp= ChkPTag( ADDR1, dlines, PState_Dirty_Exclusive, config_info.dline_size) ) {
		/*Eprintf("Error in Primary Cache TAG after a hitwb cacheop on the SCache\n");*/
		err_msg( SDHITWB_ERR6, cpu_loc);
		msg_printf( ERR, "addr %x\n", ADDR1);
		msg_printf( ERR, "Expected cache state: Dirty Exclusive\n");
		msg_printf( ERR, "Primary Data TAG %x\n", IndxLoadTag_D(PHYS_TO_K1(ADDR1)));
		retval = FAIL;
	}  
	/* 
	 * Check memory to ensure that data was written back to memory 
	 * from the primary cache correctly.
 	 */
	data = 0;
	for (addr = (u_int *)K1ADDR1; addr < (u_int *)(K1ADDR1 + lastadr);
			addr++,data += 0x11111111) {
		if (*addr != data) {
			/*Eprintf("Data not written back from Pcache to Memory\n");*/
			err_msg( SDHITWB_ERR7, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR %x\n",
				data, *addr, *addr ^ data);
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			msg_printf( ERR, "Primary Data TAG %x\n", IndxLoadTag_D((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}
	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary hit writeback test\n");
	return(retval);
}
