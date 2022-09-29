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
 *	This test verifies the block  write/read mode in data cache.
 *	It writes to K0 (0x80020000) cached space, causing the cache dirty.
 *	Then it replace the cache line by reading 0x80022000, different cache
 *	line with same offset.  This causes the data in primary data cache
 *	to be written back to the secondary. The address 0x80020000 is reread
 *	and compared. Should be a cache hit in the secondary.
 *
 *	5/17/90: Gus Wang wrote the original version.
 *	7/16/91: Richard frias modified for secondary cache.
 */
#ifdef TOAD

#include "bup.h"
#include "mach_ops.h"
#endif /* TOAD */

#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern uint noSCache(void);


/* defined by compiler as the end of our code */
extern int end;

/* Set address to beyond 8 M */
#define TSTADR	0x800000

/* addr1 used to be 0x20000 */
#define SIZE 0x20

d_refill()
{
	register volatile u_int data;
	register volatile u_int *addr;
	uint retval;

	/* Check for a secondary cache */
	if( noSCache() ) {
		msg_printf(SUM, " No secondary cache detected\n");
		return(SKIP);
	}

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (d_refill) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);
	msg_printf( DBG, "d_refill(test addr = 0x%08x)\n", TSTADR);

	ide_invalidate_caches();

	/* init K1 uncached space */
	for (addr = (u_int *)PHYS_TO_K1(TSTADR);
		addr < (u_int *)PHYS_TO_K1(TSTADR + SIZE); addr++) {

		*addr = 0x5aa50f0f;
	}

	/* write to K0 cached space with unique data */
	/* IMPLIED READ of cache block before write, must be owner first */
	for (addr = (u_int *)PHYS_TO_K0(TSTADR),data=0;
	    addr < (u_int *)PHYS_TO_K0(TSTADR + SIZE); addr++, data += 0x11111111) {

		/* Dirty cache line */
		*addr = data;
	}

	/* read back data through uncached space, it should not write through */
	for (addr = (u_int *)PHYS_TO_K1(TSTADR); addr<(u_int *)PHYS_TO_K1(TSTADR + SIZE);addr++) {
		if (*addr != 0x5aa50f0f) {
			/*Eprintf("Unexpected Cache write through to memory\n");*/
			err_msg( DREFILL_ERR1, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR %x\n",
				0x5aa50f0f, *addr,  *addr ^ 0x5aa50f0f );
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}

	/* init memory before reading it */
	for (addr = (u_int *)PHYS_TO_K1(TSTADR);
	    addr<(u_int *)PHYS_TO_K1(TSTADR + SIZE);addr++) {
		*addr = 0xdeadbeef;
	}
	/* replace primary cache line, 4 words each */
	data = *(u_int *)PHYS_TO_K0(TSTADR + 0x2000);
	data = *(u_int *)PHYS_TO_K0(TSTADR + 0x2010);

	/* read back data through cached space, it should read from secondary
	 * cache not memory. REFILL
 	 */
	for (addr = (u_int *)PHYS_TO_K0(TSTADR),data=0;
	    addr<(u_int *)PHYS_TO_K0(TSTADR + SIZE);addr++,data += 0x11111111) {
		if (*addr != data)  {
			/*Eprintf("Secondary Cache miss, expected a cache hit\n");*/
			err_msg( DREFILL_ERR2, cpu_loc);
			msg_printf( ERR, "addr = %x \n", addr);
			msg_printf( ERR, "expected = %x, actual = %x, XOR = %x\n",
				data, *addr, data ^ *addr );
			msg_printf( ERR, "Data in memory = 0xdeadbeef"); 
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}
	ide_invalidate_caches();

	msg_printf(INFO, "Completed primary data refill from secondary cache test\n");
#ifdef	TOAD
	BupHappy();
#else
	return(retval);
#endif	/* TOAD */
}
