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
 *	File Name: i_slide_data.c
 *
 *	Diag name: i_slide_data()
 *
 *	Description: This diag checks the data lines to the I-cache data ram
 *		     by sliding a one and zero bit across the bus.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

i_slide_data()
{
	register u_int actual;
	register u_int expected;
	register __psunsigned_t k1addr;
	register __psunsigned_t k0addr;
	volatile u_int tmp;
	volatile u_int tmp1;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_slide_data) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* Get the first free location at 8k boundary for cache mapping.
	 */
	k1addr= PHYS_TO_K1(0x800000);
	k0addr= K1_TO_K0(k1addr);
	msg_printf( DBG, "k1addr %x, k0addr %x\n", k1addr, k0addr);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* sliding one across the I-cache data lines */
	for( expected = 1; expected != 0; expected <<= 1) {
		*(uint *)k1addr= expected;

		invalidate_scache(config_info.sidcache_size, config_info.sidline_size);
		invalidate_dcache(config_info.dcache_size, config_info.dline_size);
		tmp = *(uint *)k0addr;

		Fill_Icache(k0addr, 1, config_info.iline_size);
		tmp= IndxLoadTag_I( k1addr);
		msg_printf( DBG, "after fill addr %x  itag %x dtag %x\n", k1addr, tmp,
			IndxLoadTag_D(k1addr) );
#ifdef DEBUG
		tmp= IndxLoadTag_SD( k1addr);
		msg_printf( DBG, " addr %x  sdtag %x\n", k1addr, tmp);
#endif /* DEBUG */

		*(uint *)k1addr= 0x007cafe;	/* clear the location for the HitWB */

		/* Write back Icache to memory or Scache if one exists */
		/* Note this does not set the secondary cache state */
		HitWB_I(k0addr, 1, config_info.iline_size);

		/* Since the above doesn't set the state to dirty/clean in the
	 	* secondary we have to dirty it so that the Icache data comes
	 	* from the secondary. Otherwise we will cause a miss on the hitwb
	 	*/
		CreateDE_SD(k0addr, 4, config_info.sidline_size);
		tmp= IndxLoadTag_SD( k0addr);
		msg_printf( DBG, " addr %x  sdtag %x\n", k0addr, tmp);
		invalidate_dcache(config_info.dcache_size, config_info.dline_size);
		actual = *(uint *)k0addr;

		if(actual != expected) {
			HitWB_SD( k0addr, 1, config_info.sidline_size);
			/*Eprintf("I-cache data ram data lines failed sliding one test.\n");*/
			err_msg( ISLIDED_ERR1, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				k0addr, expected, actual,
				expected ^ actual);
			msg_printf( ERR, "PITAG  %x\n", IndxLoadTag_I(k1addr) );
			msg_printf( ERR, "PDTAG  %x\n", IndxLoadTag_D(k1addr) );
			msg_printf( ERR, "STAG  %x\n", IndxLoadTag_SD(k1addr) );
			retval = FAIL;
		}	
	}

	invalidate_scache(config_info.sidcache_size, config_info.sidline_size);
	invalidate_dcache(config_info.dcache_size, config_info.dline_size);

	/* sliding zero across the I-cache data lines */
	for( expected = 1; expected != 0; expected <<= 1) {
		tmp = ~expected;
		*(uint *)k1addr = tmp;

		/* This read loads the secondary */
		invalidate_scache(config_info.sidcache_size, config_info.sidline_size);
		invalidate_dcache(config_info.dcache_size, config_info.dline_size);
		tmp1 = *(uint *)k0addr;
		Fill_Icache(k0addr, 1, config_info.iline_size);
		*(uint *)k1addr = 0x007cafe;

		HitWB_I(k0addr, 1, config_info.iline_size);

		/* Since the above doesn't set the state to dirty/clean in the
	 	* secondary we have to dirty it so that the Icache data comes
	 	* from the secondary. Otherwise we will cause a miss on the hitwb
	 	*/
		CreateDE_SD(k0addr, 1, config_info.sidline_size);
		tmp1 = IndxLoadTag_SD( k0addr);
		msg_printf( DBG, " addr %x  sdtag %x\n", k0addr, tmp);
		actual = *(uint *)k0addr;
		if(actual != tmp) {
			/*Eprintf("I-cache data ram data lines failed sliding zero test.\n");*/
			err_msg( ISLIDED_ERR2, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				k0addr, tmp, actual, tmp ^ actual);
			msg_printf( ERR, "PTAG  %x\n", IndxLoadTag_I(k1addr) );
			msg_printf( ERR, "STAG  %x\n", IndxLoadTag_SD(k1addr) );
			retval = FAIL;
		}	
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction data RAM data line test\n");
	return(retval);
}

