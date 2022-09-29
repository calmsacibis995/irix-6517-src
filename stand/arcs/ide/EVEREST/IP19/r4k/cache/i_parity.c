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
 *	File Name: i_parity.c
 *
 *	Diag Name: i_parity()
 *
 *	Description: This diag tests the parity bit generation of the
 *		     I-cache data ram.
 *	
 *===========================================================================*/

#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>

#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];


#define DWORD_ADDR_MASK	0xfffffff8
#define WORD_ADDR_MASK	0xfffffffc
#define BYTE_LANE_MASK	0x00000007

void i_parity_err(uint, u_char *, uint, uint, uint, uint);

i_parity()
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
	msg_printf( INFO, " (i_parity) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	osr = GetSR();
	/* Set the DE bit in the Status register so that the 
	 * I-cache data ram parity error will not cause exception.
	 */
	SetSR(osr | SR_DE);

	first_addr = (u_char *)PHYS_TO_K1(0x800000);
	last_addr = (u_char *)((u_int)first_addr + config_info.icache_size - 1);
	first_addr= (u_char *)K1_TO_K0(first_addr);
	last_addr = (u_char *)K1_TO_K0(last_addr);

	/*
	 * Initialize the memory buffer with zeros and then fill the 
	 * I-cache.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 4) {
		* (u_int *)ptr = 0;
	}

	Fill_Icache(K1_TO_K0(first_addr), config_info.icache_size / config_info.iline_size, config_info.iline_size);

	/*
	 * shift a series of ones into the I-cache data ram to check the
	 * generation of the parity bits.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr++ ) {
		byte_lane = (u_int)ptr & BYTE_LANE_MASK;
		parity = (1 << (byte_lane ^ 7) );
		msg_printf( DBG, "byte lane: %x parity %x\n", byte_lane, parity);
		for(i=0, bits=0, pbit=0; i < 9; i++) {
			*ptr = bits;
			Fill_Icache((u_int)ptr & WORD_ADDR_MASK, 1, config_info.iline_size);
			IndxLoadTag_I((u_int)ptr & DWORD_ADDR_MASK);
			ecc_reg = ~GetECC() & 0xff;

			msg_printf( DBG, "addr: %x data: %x ecc_reg: %x pbit: %x\n", ptr, bits,
				ecc_reg, pbit);
			if(ecc_reg != pbit) {
				i_parity_err(1, ptr, bits, parity, pbit, ecc_reg);
				retval = FAIL;
			}
			bits = bits * 2 + 1;
			pbit = pbit ^ parity;
		}
	}

	/* 
	 * Fill the I-cache with 0x1111 so that the parity ram is initialized
	 * to all ones.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 4) {
		*(u_int *)ptr = 0x01010101;
		Fill_Icache((__psunsigned_t)ptr, 1, config_info.iline_size);
	}

	/*
	 * shift a series of zero into the I-cache data ram to check the
	 * generation of the parity bits.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr++ ) {
                if( ((u_int)ptr & BYTE_LANE_MASK) == 0) {
                        last_pbit= 0;
                        pbit = 0x0;
                }

		byte_lane = (u_int)ptr & BYTE_LANE_MASK;
		parity = (1 << (byte_lane ^ 7) );

		/* Set up expected parity, begins with all ones from above*/
		/*pbit = 0xff;
		tmp = (pbit >> (byte_lane +1)) ;
		pbit = tmp << (byte_lane +1); */
		pbit= (parity ^ pbit) | last_pbit;
		for(i=0, bits=0; i < 9; i++) {
			*ptr = ~(u_char)bits;
			Fill_Icache((u_int)ptr & WORD_ADDR_MASK, 1, config_info.iline_size);
			IndxLoadTag_I((u_int)ptr & DWORD_ADDR_MASK);
			ecc_reg = GetECC() & 0xff;

			msg_printf( DBG, "addr: %x data: %x ecc_reg: %x pbit: %x\n", ptr, ~bits,
				ecc_reg, pbit);
			if(ecc_reg != pbit) {
				i_parity_err(2, ptr, ~bits, parity, pbit, ecc_reg);
				retval = FAIL;
			}
			bits = bits * 2 + 1;
			last_pbit = pbit | last_pbit;
			pbit = pbit ^ parity;
		}
	}

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction parity generation test\n");
	return(retval);
}


void
i_parity_err(uint errcode, u_char *byte_addr, uint data, uint byte_lane, uint expected_par, uint actual_par)
{
	/*Eprintf("I-cache parity generation error %x\n", errcode);*/
	err_msg( IPAR_ERR1, cpu_loc);
	msg_printf( ERR, "error %x\n", errcode);
	msg_printf( ERR, "Cache byte address: 0x%08x data:0x%02x\n", byte_addr, (u_char)data);
	msg_printf( ERR, "Parity bit position: 0x%02x\n", byte_lane);
	msg_printf( ERR, "Expected parity: 0x%02x Actual parity:0x%02x.\n",
				expected_par, actual_par);
}

