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

/*
 *	sdtagwlk test	
 *	Checks the data integrity of the Secondary data tag ram path using a
 *	walking ones/zeros pattern.
 *
 */

#ifdef TOAD

#include "bup.h"
#include "types.h"

#else
#include "sys/types.h"
#endif

#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern int error_data[4];

int sdtagwlk(void);

/**********************************************/
/***********  START of TEST CODE  *************/
/**********************************************/

sd_tagwlk()
{
	u_int retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_tagwlk) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	if( sdtagwlk() )
		retval = FAIL;
	
	msg_printf(INFO, "Completed secondary TAG data path test\n");
	return(retval);
	
}


int sdtagwlk(void)
{
	register u_int actual;
	register u_int expected;
	register u_int address;
	register u_int tmp;
	register u_int 	i;
	register u_int datamask;
	extern u_int scacherrx;
	uint retval=PASS;
	__psunsigned_t osr;

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);


	/* Use location zero of the tag rams, use a Kseg1 address to
	 * avoid tlb misses
	 */
	address= 0xa0000000;
	
	/* Mask off undefined bits plus parity bit */
	datamask= 0xffffff80;

	/* Try sliding one */
	for( i=1, expected =0x80; i <= 24; i ++, expected <<= 1) {


		IndxStorTag_SD(address, expected);
		msg_printf( DBG, "Tag Location 0x%x  data %x\n", address, expected);

		actual = IndxLoadTag_SD(address);
		if( (actual & datamask) != expected) {

			/*Eprintf("Secondary Data TAG RAM Path Error\n");*/
			err_msg( SDTAGWLK_ERR1, cpu_loc);
			msg_printf( ERR, "on sliding one pattern\n");
			msg_printf( ERR, "TAG RAM Location 0x%x\n", address);
			msg_printf( ERR, "Expected 0x%x  Actual= 0x%x  XOR= 0x%x\n",
				expected, actual, (expected ^ actual) );
			retval = FAIL;
		}	
	}
	/* Try sliding zero */
	for( i=1, expected =0x80; i <= 24; i ++, expected <<= 1) {

		tmp= (datamask & (~expected) );

		/* Load TAG Location */
		IndxStorTag_SD(address, tmp);

		msg_printf( DBG, "Tag Location 0x%x  data %x\n", address, expected);

		actual = (datamask & IndxLoadTag_SD(address));
		if( actual != tmp) {

			/*Eprintf("Secondary Data TAG RAM Path Error\n");*/
			err_msg( SDTAGWLK_ERR1, cpu_loc);
			msg_printf( ERR, "on sliding zero pattern\n");
			msg_printf( ERR, "TAG RAM Location 0x%x\n", address);
			msg_printf( ERR, "Expected 0x%x  Actual= 0x%x  XOR= 0x%x\n",
				tmp, actual, (tmp ^ actual) );

			retval = FAIL;
		}	
	}
	SetSR(osr);
	return(retval);
}
