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
 *	File Name: i_tagparity.c
 *
 *	Diag name: i_tagparity
 *
 *	Description: This diag tests the functionality of the parity bit in
 *		     the primary I-cache tag. For each tag, the parity bit
 *		     is tested to respond to each bit change in the tag.
 *			1. Invalidate the I-cache.
 *			2. For each tag, shift a stream of one's and zero's
 *			   into the tag to check if the parity bit change
 *			   state accordingly.
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

void i_tagparity_err(__psunsigned_t, uint, uint, uint, uint);

#define TAGPARITY_MASK 1
#define TAG_PTAG_SHIFT          0x00000008

i_tagparity()
{
	register __psunsigned_t addr;
	register u_int data;
	register u_int i;
	register u_int ln;
	register u_int pbit;
	register u_int bits;
	register u_int taglo;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_tagparity) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	for(ln = 0; ln < config_info.icache_size; ln += config_info.iline_size) {
		addr = PHYS_TO_K0(ln);
		for(i=0, bits = 0, pbit = 0; i < 24; i++) {

			/* I-cache tag Valid bit is mapped to Taglo bit 7 */
			data = bits << (TAG_PTAG_SHIFT + 1);

			IndxStorTag_I(addr, data);
			msg_printf( DBG, "Tag location 0x%x data 0x%x\n", addr, data);
			msg_printf( DBG, "parity bit %x\n", pbit);
			taglo = IndxLoadTag_I(addr);

			if((taglo & TAGPARITY_MASK) != pbit) {
				i_tagparity_err(addr, data, taglo,
						taglo & TAGPARITY_MASK, pbit);
				retval = FAIL;
			}
			bits = (bits * 2 + 1);
			pbit = pbit ^ TAGPARITY_MASK;
		}

		for(i=0, bits = 0, pbit = TAGPARITY_MASK; i < 24; i++) {
			/* I-cache tag V bit is map at Taglo bit 7 */
			data = ~bits << (TAG_PTAG_SHIFT + 1);
			IndxStorTag_I(addr, data);
			msg_printf( DBG, "Inverse: Tag location 0x%x data 0x%x\n",
				 addr, data);
			taglo = IndxLoadTag_I(addr);

			if((taglo & TAGPARITY_MASK) != pbit) {
				i_tagparity_err(addr, data, taglo,
						taglo & TAGPARITY_MASK, pbit);
				retval = FAIL;
			}
			bits = (bits * 2 + 1);
			pbit = pbit ^ TAGPARITY_MASK;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction TAG RAM parity test\n");
	return(retval);
}


void
i_tagparity_err(__psunsigned_t tag_index, uint tag_data, uint taglo, uint actual_par, uint expected_par)
{
	/*Eprintf("I-cache tag ram parity bit error.\n");*/
	err_msg( ITAGPAR_ERR1, cpu_loc);
	msg_printf( ERR, "Tag ram address: 0x%08x expected content: 0x%08x\n",
			tag_index, tag_data);
	msg_printf( ERR, "Taglo: 0x%08x expected parity: 0x%x actual parity: 0x%x\n",
			taglo, expected_par, actual_par);
}

