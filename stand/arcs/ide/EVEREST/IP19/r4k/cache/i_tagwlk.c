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

#ident "$Revision: 1.4 $"


/*=============================================================================
 *
 *	File Name: i_tagwlk.c
 *
 *	Diag name: pitagwlk
 *
 *	Description: This diag checks the data integrity of the primary 
 *		     instruction cache tag ram path using a walking ones
 *		     and zeros pattern.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

pitagwlk()
{
	register u_int actual;
	register u_int expected;
	register u_int address;
	register u_int tmp;
	register u_int i;
	register u_int datamask;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (pitagwlk) \n");
	msg_printf(VRB, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* Use location zero of the tag rams
	 */
	address= 0xa0000000;
	
	/* Mask off undefined bits plus parity bit */
	datamask= 0xffffff00;

	/* Try sliding one */
	for( i=1, expected =0x100; i <= 24; i ++, expected <<= 1) {
		IndxStorTag_I(address, expected);
		actual = IndxLoadTag_I(address);
		if( (actual & datamask) != expected) {
			/*Eprintf("I-cache tag ram data line error.\n"); */
			err_msg( ITAGWLK_ERR1, cpu_loc);
			msg_printf( ERR, "Failed sliding one test at 0x%08x\n",
					address);
			msg_printf( ERR, "Expected: 0x%08x, Actual: 0x%08x\n",
					expected, actual);
			retval = FAIL;
		}	
	}
	/* Try sliding zero */
	for( i=1, expected =0x100; i <= 24; i ++, expected <<= 1) {
		tmp= (datamask & (~expected) );

		/* Load TAG Location */
		IndxStorTag_I(address, tmp);
		actual = (datamask & IndxLoadTag_I(address));
		if( actual != tmp) {
			/*Eprintf("I-cache tag ram data line error.\n");*/
			err_msg( ITAGWLK_ERR1, cpu_loc);
			msg_printf( ERR, "Failed sliding zero test at 0x%08x\n",
					address);
			msg_printf( ERR, "Expected: 0x%08x, Actual: 0x%08x\n",
					tmp, actual);
			retval = FAIL;
		}	
	}

	msg_printf(INFO, "Completed primary instruction TAG RAM data line test\n");
	return (retval);
}

