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
 *	File Name: i_tagaddr.c
 *
 *	Diag name: pdtagadr
 *
 *	Description: This diag tests the address lines to the primary
 *		     instruction cache tag ram by sliding a one and then a 
 *		     zero one the address lines. This test assumes that the
 *		     taglo register is in good working condition.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

pitagadr()
{
	register u_int actual;
	register u_int data;
	register u_int tmp_data;
	register u_int datamask;
	register u_int addr;
	register u_int i;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (pitagadr) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* Mask off cache state bits, undefined bits, and parity bit */
	datamask= 0xffffff00;

	data = 0x100;		/* ignore undefined bits and parity bit */
	for( i = config_info.iline_size ; i < config_info.icache_size; i *= 2 ) {
		addr = 0xa0000000 + i;
		IndxStorTag_I(addr, data);
		msg_printf( DBG, "Tag Location 0x%x  data %x\n", addr, data);
		data = data * 2;
	}

	/* Read back the data */
	data = 0x100;
	for( i = config_info.iline_size; i < config_info.icache_size; i *= 2 ) {
		addr = 0xa0000000 + i;
		actual = (datamask & IndxLoadTag_I(addr));
		if( actual != data) {
			/*Eprintf("I-cache tag ram address line error.\n");*/
			err_msg( ITAGADR_ERR1, cpu_loc);
			msg_printf( ERR, "Failed sliding one test at 0x%08x\n",
					addr);
			msg_printf( ERR, "Expected: 0x%08x Actual 0x%08x.\n",
					data, actual);
			retval = FAIL;
		}	
		data = data * 2;
	}

	data = 0x100;
	for( i = config_info.iline_size ; i < config_info.icache_size; i *= 2 ) {
		addr = 0xa0000000 + ((config_info.icache_size - 4) & ~i);
		tmp_data = ~data & datamask;
		IndxStorTag_I(addr, tmp_data);
		msg_printf( DBG, "Tag Location 0x%x  data %x\n", addr, tmp_data);
		data = data * 2;
	}

	/* Read back the data */
	data = 0x100;
	for( i = config_info.iline_size; i < config_info.icache_size; i *= 2 ) {
		addr = 0xa0000000 + ((config_info.icache_size - 4) & ~i);
		tmp_data = ~data & datamask;
		actual = (datamask & IndxLoadTag_I(addr));
		if( actual != tmp_data) {
			/*Eprintf("I-cache tag ram address line error.\n");*/
			err_msg( ITAGADR_ERR1, cpu_loc);
			msg_printf( ERR, "Failed sliding zero test at 0x%08x\n",
					addr);
			msg_printf( ERR, "Expected: 0x%08x Actual 0x%08x.\n",
					tmp_data, actual);
			retval = FAIL;
		}	
		data = data * 2;
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction TAG RAM address line test\n");
	return(retval);
}

