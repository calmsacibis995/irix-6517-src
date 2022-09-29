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

#ident "$Revision: 1.7 $"


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

#define DEBUG
#define WORD_ADDR_MASK 0xfffffffc

/* Use the compliment of the address as data */
#define COMPLIMENT 1

/* write data from low to high */
#define FORWARD 0

void dumpmem(__psunsigned_t, __psunsigned_t);


/* performs an address in address test on the secondary data cache */
sd_aina(first_addr, last_addr)
register int first_addr;
register int last_addr;

{
	register __psunsigned_t i, tmp;
	register u_int j;
	register __psunsigned_t k1addr;
	extern u_int scacherrx;
	uint retval;
	__psunsigned_t osr;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_aina) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	first_addr = 0x82800000;
	last_addr = first_addr + config_info.sidcache_size;

	/* get cache sizes and invalidate caches */
	setconfig_info();

	msg_printf( DBG, "Write memory with compilment\n");
	/* TAG MEMORY with the compliment of the address */
	filltagW_l((u_int *)K0_TO_K1(first_addr), (u_int *)K0_TO_K1(last_addr), COMPLIMENT, FORWARD);
#ifdef DEBUG
	dumpmem( K0_TO_K1(first_addr), K0_TO_K1(first_addr + 0x40) );
#endif 

	msg_printf( DBG, "Dirty Cache\n");
	/* DIRTY SCACHE */
	for(i= first_addr; i < last_addr ;  ) {
		*(u_int *)i = ~((uint)K0_TO_K1(i)) ;
		i = i + config_info.sidline_size;
	}

#ifdef DEBUG
	msg_printf( DBG, "Write memory with deadbeef\n");
#endif 
	/* Load memory with a background pattern */
	fillmemW( K0_TO_K1(first_addr), K0_TO_K1(last_addr + config_info.sidcache_size), 0xdeadbeef, FORWARD);
#ifdef DEBUG
	dumpmem( K0_TO_K1(first_addr), K0_TO_K1(first_addr+ 0x40) );
#endif 

	msg_printf( DBG, "force write back\n");
	/* Write the address as data now, force back the first pattern */
	filltagW_l((u_int *)(first_addr + config_info.sidcache_size), (u_int *)(last_addr + config_info.sidcache_size), 0, FORWARD);

#ifdef DEBUG
	dumpmem( K0_TO_K1(first_addr + config_info.sidcache_size), K0_TO_K1(first_addr+ config_info.sidcache_size + 0x40) );
	dumpmem( K0_TO_K1(first_addr), K0_TO_K1(first_addr+ 0x40) );
#endif 

	msg_printf( DBG, "5\n");
	/* check the data in memory */
	k1addr = K0_TO_K1(last_addr);
	for(i= K0_TO_K1(first_addr); i < k1addr; i += 4) {
		tmp = *(u_int *)i;
		if( ~i != tmp) {
			
			/*Eprintf("Secondary Memory Error 1\n");*/
			err_msg( SDAINA_ERR1, cpu_loc);
			msg_printf( ERR, "Address %08x\n", i);
			msg_printf( ERR, "expected %08x, actual %08x, XOR %08x\n",
				~i, tmp, (~i ^ tmp) );
			retval = FAIL;
		}
	}

	/* now check the second pattern in the secondary cache */
	/* Force the writeback */
	msg_printf( DBG, "Check the second writeback operation.\n");
	fillmemW( first_addr, last_addr, 0x0, FORWARD);

#ifdef DEBUG
	dumpmem( K0_TO_K1(first_addr +config_info.sidcache_size), K0_TO_K1(first_addr+ config_info.sidcache_size + 0x40) );
	dumpmem( K0_TO_K1(first_addr ), K0_TO_K1(first_addr) );
#endif 

	/* check the data in memory */
	k1addr = K0_TO_K1(last_addr + config_info.sidcache_size);
	j= first_addr + config_info.sidcache_size;
	for(i= K0_TO_K1(first_addr + config_info.sidcache_size); i < k1addr; i += 4, j += 4) {
		tmp = *(u_int *)i;
		if( tmp != j ) {
			
			/*Eprintf("Secondary Memory Error pattern 2\n");*/
			err_msg( SDAINA_ERR2, cpu_loc);
			msg_printf( ERR, "Address %08x\n", i);
			msg_printf( ERR, "expected %08x, actual %08x, XOR %08x\n",
				j, tmp, (j ^ tmp) );
			retval = FAIL;
		}
	}

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary data ram test\n");
	return(retval);
}

#ifdef DEBUG
void dumpmem(__psunsigned_t start, __psunsigned_t end)
{
	msg_printf( DBG, "start %x, end %x\n", start, end);
	for(; start < end; start += 0x10) {
		msg_printf( DBG, "%x ", *(u_int *)start);
		msg_printf( DBG, "%x ", *(u_int *)(start + 4));
		msg_printf( DBG, "%x ", *(u_int *)(start + 8));
		msg_printf( DBG, "%x \n", *(u_int *)(start + 0xc));
	}
}
#endif 
