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

/*
 *	sdtagecc test	
 *	Checks the data integrity of the Secondary data tag ram path using a
 *	walking ones/zeros pattern.
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

/* TAG ECC codes for a sliding 1 across the tag bits 7-31 */
/* The first entry corresponds to bit 7, .i.e 0x45 is the */
/* ECC pattern generated when only bit 7 is turned on.	  */
/* Done this way for simulation since generating the ECC  */
/* on the fly takes to long.				  */
static	u_int sdtagecc[] = {

		0x45, 0x29, 0x51, 0x13, 0x49, 0x25,	/* ECC for bits 7-12 */
		0x7,  0x16, 0x26, 0x46, 0xd,  0xe,	/* ECC for bits 13-18 */
		0x1c, 0x4c, 0x31, 0x32, 0x38, 0x70,	/* ECC for bits 19-24 */
		0x61, 0x62, 0x64, 0x68, 0xb,  0x15,	/* ECC for bits 25-30 */
		0x23, 					/* ECC for bit 31 */ 

		/* Sliding zero pattern; TAG ECC  codes for bits 7-31*/
		0x5c, 0x30, 0x48, 0xa,  0x50, 0x3c,	/* Bits 7-12 */
		0x1e, 0xf,  0x3f, 0x5f, 0x14, 0x17,	/* Bits 13-18 */
		0x5,  0x55, 0x28, 0x2b, 0x21, 0x69,	/* Bits 19-24 */
		0x78, 0x7b, 0x7d, 0x71, 0x12, 0xc,	/* bits 25-30 */ 
		0x3a,					/* bit 31 */
		};
/***********************************************/
/***********  START of TEST CODE  **************/
/***********************************************/

sd_tagecc()


{
	register u_int actual;
	register u_int expected;
	register __psunsigned_t address;
	register u_int tmp;
	register u_int 	i;
	register u_int datamask;
	register __psunsigned_t  lastaddr;
	u_int ecc, retval;
	extern u_int scacherrx;
	__psunsigned_t osr;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_tagecc) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	lastaddr = 0xa0000000 + config_info.sidcache_size;

	/* Use location zero of the tag rams, use a Kseg1 address to
	 * avoid tlb misses
	 */
	for( address= 0xa0000000; address < lastaddr; address += config_info.sidline_size) {
		/* Mask off undefined bits plus parity bit */
		datamask= 0x7f;

		/* Try sliding one */
		for( i=0, expected =0x80; i <= 24; i ++, expected <<= 1) {


			IndxStorTag_SD(address, expected);
			ecc = sdtagecc[i];
#ifndef	TOAD
			msg_printf( DBG, "Tag Location 0x%x  data %x\n", address, expected);
#endif	/* TOAD */

			actual = IndxLoadTag_SD(address);
			if( (actual & datamask) != ecc) {
#ifdef TOAD
				bupSad();
#else

				/*Eprintf("Secondary Data TAG RAM Path Error\n");*/
				err_msg( SDTAGECC_ERR1, cpu_loc);
				msg_printf( ERR, "TAG RAM Location 0x%x\n", address);
				msg_printf( ERR, "Expected 0x%x  Actual= 0x%x  XOR= 0x%x\n",
				    expected, actual, (expected ^ actual) );
#endif /* TOAD */
				retval = FAIL;
			}
		}
		/* Try sliding zero */
		for( i=25, expected =0x80; i <= 49; i ++, expected <<= 1) {

			tmp= (0xffffff80 & (~expected) );

			ecc = sdtagecc[i];
			/* Load TAG Location */
			IndxStorTag_SD(address, tmp);

#ifndef TOAD
			msg_printf( DBG, "Tag Location 0x%x  data %x\n", address, expected);
#endif  /* TOAD */

			actual = (datamask & IndxLoadTag_SD(address));
			if( actual != ecc) {
#ifdef TOAD
				bupSad();
#else

				/*Eprintf("Secondary Data TAG RAM Path Error\n");*/
				err_msg( SDTAGECC_ERR2, cpu_loc);
				msg_printf( ERR, "TAG RAM Location 0x%x\n", address);
				msg_printf( ERR, "Expected 0x%x  Actual= 0x%x  XOR= 0x%x\n",
				    tmp, actual, (tmp ^ actual) );
#endif	/* TOAD */
				retval = FAIL;
			}
		}
	}
	msg_printf( DBG, "address %x\n, lastaddr %x\n", address, lastaddr);
	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary TAG ECC test\n");
	return(retval);
}
