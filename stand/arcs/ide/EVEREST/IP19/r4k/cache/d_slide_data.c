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
 *	File Name: d_slide_data.c
 *
 *	Diag name: d_slide_data
 *
 *	Description: This diag tests the data lines to the primary data cache.
 *		     A sliding one and a sliding zero data pattern is written
 *		     into the first location of the D-cache to check if each
 *		     data line can be toggled individually.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];


d_slide_data()
{
	register u_int actual;
	register u_int expected;
	register __psunsigned_t address;
	register u_int tmp;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_slide_data) \n");
	msg_printf(VRB, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* Use cache location zero (Kseg0 address) to avoid tlb misses
	 */
	address= K0BASE;
	
	/* sliding one across the D-cache data lines */
	for( expected = 1; expected != 0; expected <<= 1) {
		*(uint *)address = expected;
#ifdef DEBUG
		msg_printf( DBG, "Location 0x%x  data %x\n", address, expected);
#endif /* DEBUG */
		actual = *(uint *)address;
		if(actual != expected) {
			/*Eprintf("D-cache data ram data lines failed walking one test.\n");*/
			err_msg( DSLIDEDATA_ERR1, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				address, expected, actual, expected ^ actual);
			retval = FAIL;
		}	
	}

	/* sliding zero across the D-cache data lines */
	for( expected = 1; expected != 0; expected <<= 1) {
		tmp = ~expected;
		*(uint *)address = tmp;
#ifdef DEBUG
		msg_printf( DBG, "Location 0x%x  data %x\n", address, tmp);
#endif /* DEBUG */
		actual = *(uint *)address;
		if(actual != tmp) {
			/*Eprintf("D-cache data ram data lines failed walking zero test.\n");*/
			err_msg( DSLIDEDATA_ERR2, cpu_loc);
			msg_printf( ERR, "Addr: 0x%08x Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				address, tmp, actual, tmp ^ actual);
			retval = FAIL;
		}	
	}

	msg_printf(INFO, "Completed primary data RAM data line test\n");
	return(retval);
}

