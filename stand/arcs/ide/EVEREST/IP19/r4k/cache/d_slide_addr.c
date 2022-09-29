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
 *	File Name: d_slide_addr.c
 *
 *	Diag name: d_slide_addr
 *
 *	Description: This diag tests the address lines to the primary data
 *		     cache. Each address line to the data cache is toggled
 *		     once individually by sliding a one and then a zero
 *		     across the address lines. 
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include "everr_hints.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

d_slide_addr()
{
	register u_int actual;
	register u_int expected;
	register u_char *addr;
	register u_int i;
	register u_int tmp;
	register u_int addr_mask;
	register u_int byte_mask = 0x000000ff;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_slide_addr) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* Slide a one across the D-cache address lines */
	for(i = 1, expected = 1; i < config_info.dcache_size; i *= 2, expected += 1 ) {
		addr = (u_char *)(K0BASE + i);
		*addr = (u_char) expected;
#ifdef DEBUG
		msg_printf( DBG, "Location 0x%x  expected %x\n", addr, expected);
#endif /* DEBUG */
	}

	/* Read back the expected */
	for( i = 1, expected = 1; i < config_info.dcache_size; i *= 2, expected += 1 ) {
		addr = (u_char *)(K0BASE + i);
		actual = byte_mask & (u_int) *addr;
		if( actual != expected) {
			/*Eprintf("D-cache data ram address lines failed sliding one test.\n"); */
			err_msg(DSLIDEADR_ERR1, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				addr, expected, actual, expected ^ actual);
			retval = FAIL;
		}	
	}

	/* Slide a zero across the D-cache address lines
	*/
	addr_mask = config_info.dcache_size - 1;
	for(i = 1, expected = 1; i < config_info.dcache_size; i *= 2, expected += 1 ) {
		addr = (u_char *)(K0BASE + (addr_mask & ~i));
		*addr = (u_char) ~expected;
#ifdef DEBUG
		msg_printf( DBG, "Location 0x%x  expected %x\n", addr, expected);
#endif /* DEBUG */
	}

	/* Read back the expected */
	for( i = 1, expected = 1; i < config_info.dcache_size; i *= 2, expected += 1 ) {
		addr = (u_char *)(K0BASE + (addr_mask & ~i));
		actual = byte_mask & (u_int) *addr;
		tmp = byte_mask & ~expected;
		if( actual != tmp) {
			/*Eprintf("D-cache data ram address lines failed sliding zero test.\n"); */
			err_msg(DSLIDEADR_ERR2, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				addr, tmp, actual, tmp ^ actual);
			retval = FAIL;
		}	
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data RAM address line test\n");
	return(retval);
}

