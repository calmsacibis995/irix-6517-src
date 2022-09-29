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
 *	sdtagadr test	
 *	Checks the address integrity to the Primary Data TAG rams by using
 *	a walking address.
 *
 */

#ifdef	TOAD
#include "bup.h"
#include "types.h"

#else
#include "sys/types.h"
 
#endif	/* TOAD */
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern int error_data[4];

/* Address table for tag test */
/* sliding address or walking one addr */

struct tagadr {
	u_int addr;
	u_int data;
	};

static struct tagadr 	sdtagadr_tbl[] = {
			0xa0000080, 0x11400,
		  	0xa0000100, 0x21400,
		  	0xa0000200, 0x31400,
		  	0xa0000400, 0x41400,
		  	0xa0000800, 0x51400,
		  	0xa0001000, 0x61400,
		  	0xa0002000, 0x71400,
		  	0xa0004000, 0x81400,
		  	0xa0008000, 0x91400,
		  	0xa0010000, 0xa1400,
		  	0xa0020000, 0xb1400,
			0xa0000000, 0x1400,
			0,0
			};

static sdtagadr(void);

/**********************************************/
/***********  START of TEST CODE  *************/
/**********************************************/

sd_tagaddr()
{
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_tagaddr) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	if( sdtagadr() ) {
#ifdef	TOAD
		bupSad();
#endif	/* TOAD */
		retval = FAIL;
	}
	else 
#ifdef	TOAD
		BupHappy();
#endif	/* TOAD */
	
	msg_printf(INFO, "Completed secondary TAG address test\n");
	return(retval);
	
}


static sdtagadr(void)
{
	register u_int error;
	register u_int actual;
	register u_int datamask;
	register struct	tagadr *ptr;
	extern u_int scacherrx;
	__psunsigned_t osr;

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);


	ptr = sdtagadr_tbl;

	/* Mask off undefined bits plus parity bit */
	datamask= 0xffffff80;

	for( ; ptr->addr; ptr ++) {

		IndxStorTag_SD(ptr->addr, ptr->data);
#ifndef	TOAD
		msg_printf( DBG, "Tag Location 0x%x  data %x\n", 
			ptr->addr, ptr->data);
#endif	/* TOAD */

	}

	ptr = sdtagadr_tbl;

	/* Read back the data */
	for( ; ptr->addr; ptr ++) {
		error= 0;

		actual = (datamask & IndxLoadTag_SD(ptr->addr));
		if( actual != ptr->data) {

#ifdef	TOAD
			bupSad();
#else
			/*Eprintf("Secondary Data TAG Address Error\n");*/
			err_msg( SDTAGADR_ERR1, cpu_loc);
			msg_printf( ERR, "TAG RAM Location 0x%x\n", ptr->addr);
			msg_printf( ERR, "Expected 0x%x  Actual= 0x%x  XOR= 0x%x\n",
				ptr->data, actual, (ptr->data ^ actual) );
#endif	/* TOAD */
			error= 1;
			SetSR(osr);
			return(error);
		}	
	}
	SetSR(osr);
	return(PASS);
}
