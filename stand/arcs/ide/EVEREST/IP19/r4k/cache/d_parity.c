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
 *	File Name: d_parity.c
 *
 *	Diag Name: d_parity()
 *
 *	Description: This diag tests the parity bit generation of the
 *		     D-cache data ram.
 *			1. Invalidate the entire D-cache.
 *			2. Set the DE bit in the status regiter to avoid
 *			   cache parity exception.
 *			3. Initialize the D-cache with zero so that the
 *			   parity bits are all initialized to zero.
 *			4. For each byte within within each double word,
 *			   shift a stream of one's from right to left.
 *			   After every bit shift, check if the parity bit
 *			   for that byte is in the correct state.
 *			5. Initialize the D-cache with 0x01010101 so that the
 *			   parity bits are all initialized to one.
 *			6. Repeat 4. with a steam of zeros.
 *	
 *===========================================================================*/

#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

#define DWORD_ADDR_MASK	0xfffffff8
#define BYTE_LANE_MASK	0x00000007

void d_parity_err(uint, u_char *, uint, uint, uint, uint);

d_parity()
{
	register u_char *last_addr;
	register u_char *ptr;
	register u_int byte_lane;
	register u_int parity;
	register u_int i;
	register u_int bits;
	register u_int pbit;
	register u_int ecc_reg;
	u_int retval;
	u_int last_pbit;
	u_char *first_addr;
	__psunsigned_t osr;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_parity) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	osr = GetSR();
	/* Set the DE bit in the Status register so that the 
	 * D-cache data ram parity error will not cause exception.
	 */
	SetSR(osr | SR_DE);

	first_addr = (u_char *)K0BASE;
	last_addr = (u_char *)(K0BASE + config_info.dcache_size - 1);

	/* 
	 * Initialize the D-cache to zero so that the parity ram is also
	 * initialized to zero.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 4) {
		*(u_int *)ptr = 0;
	}

	/*
	 * shift a series of ones into the D-cache data ram to check the
	 * generation of the parity bits.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr++ ) {
		byte_lane = (u_int)ptr & BYTE_LANE_MASK;
		parity = (1 << (byte_lane ^ 7) );
#ifdef DEBUG
		msg_printf( DBG, "byte lane: %x parity %x\n", byte_lane, parity);
#endif
		for(i=0, bits=0, pbit=0; i < 9; i++) {
			*ptr = bits;
			IndxLoadTag_D((u_int)ptr & DWORD_ADDR_MASK);
			ecc_reg = ~GetECC() & 0xff;
			msg_printf( DBG, "addr: %x data: %x ecc_reg: %x pbit: %x\n", ptr, bits,
				ecc_reg, pbit);
			if(ecc_reg != pbit) {
				d_parity_err(1,ptr, bits, byte_lane, pbit, ecc_reg);
				retval = FAIL;
			}
			bits = bits * 2 + 1;
			pbit = pbit ^ parity;
		}
	}


	/* 
	 * Fill the D-cache with 0x1111 so that the parity ram is initialized
	 * to all ones.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 4) {
		*(u_int *)ptr = 0x01010101;
	}


	/*
	 * shift a series of zero into the D-cache data ram to check the
	 * generation of the parity bits.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr++ ) {
		if( ((u_int)ptr & BYTE_LANE_MASK) == 0) {
			last_pbit= 0;
			pbit = 0x0;
		}
		byte_lane = (u_int)ptr & BYTE_LANE_MASK;
		parity = (1 << (byte_lane ^ 7) );
		msg_printf( DBG, "byte lane: %x parity %x\n", byte_lane, parity);

		/* Set up expected parity, begins with all ones from above*/
		/*tmp = (pbit >> (byte_lane +1)) ; */
		pbit= (parity ^ pbit) | last_pbit;

		for( i=0, bits=0; i < 9; i++) {
			*ptr = ~(u_char)bits;
			IndxLoadTag_D((u_int)ptr & DWORD_ADDR_MASK);
			ecc_reg = GetECC() & 0xff;

			msg_printf( DBG, "addr: %x data: %x ecc_reg: %x pbit: %x\n", ptr, ~bits,
				ecc_reg, pbit);
			if(ecc_reg != pbit) {
				d_parity_err(2,ptr, ~bits, byte_lane, pbit, ecc_reg);
				retval = FAIL;
			}
			bits = bits * 2 + 1;
			last_pbit = pbit | last_pbit;
			pbit = pbit ^ parity;
		}
	}

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data parity generation test\n");
	return(retval);
}


void d_parity_err(uint errcode, u_char *byte_addr, uint data, uint byte_lane, uint expected_par, uint actual_par)
{
	/*Eprintf("D-cache parity generation error %x\n", errcode);*/
	err_msg( DPAR_ERR1, cpu_loc);
	msg_printf( ERR, "error %x\n", errcode);
	msg_printf( ERR, "Cache byte address: 0x%08x data:0x%02x\n", byte_addr, (u_char)data);
	msg_printf( ERR, "Parity bit position: 0x%02x\n", byte_lane);
	msg_printf( ERR, "Expected parity: 0x%02x Actual parity:0x%02x.\n",
				expected_par, actual_par);
}

