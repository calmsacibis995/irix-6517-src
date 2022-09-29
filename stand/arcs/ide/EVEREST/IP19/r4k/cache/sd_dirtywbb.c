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
 *	This test verifies the block (4 words) write mode in data cache.
 *	It writes to K0 (0x80020000) cached space, causing the cache dirty.
 *	Then it replace the cache line by reading 0x80022000, different cache
 *	line with same offset.  This causes the data in 0x80020000 wrtie back 
 *	to memory which now has the same data as in 0x80020000.  Multiple
 *	cache lines are tested back to back.  Byte transactions are tested.
 *
 *	5/17/90: Gus Wang wrote the original version.
 *	8/1/91:  Richard Frias modified for secondary cache.
 */

#ifdef	TOAD
#include "bup.h"
#include "mach_ops.h"
#endif	/* TOAD */

#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

/* Set address to beyond 8 M */
#define TSTADR 0x800000
#define ALIASADR (TSTADR + config_info.sidcache_size)
#define LINES (800 * config_info.sidline_size)


/* 'end' defined by the compiler as the end of your code */
extern u_int end;

sd_dirtywbb()
{
	register volatile unsigned char data;
	register volatile unsigned char *addr;
	uint retval;

	retval = PASS;
	msg_printf( INFO, " (sd_dirtywbb) \n");
	getcpu_loc(cpu_loc);
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);
	setconfig_info();

	/* init K1 uncached space */
	for (addr = (u_char *)PHYS_TO_K1(TSTADR);addr< (u_char *)PHYS_TO_K1(TSTADR + LINES);addr++) {
		*addr = 0x5a;
	}

	/* write to K0 cached space with unique data */
	for (addr = (u_char *)PHYS_TO_K0(TSTADR),data=0;
	    addr<(u_char *)PHYS_TO_K0(TSTADR + LINES);addr++,data += 0x1) {
		*addr = data;
	}

	/* read back data through uncached space, it should not write through */
	for (addr = (u_char *)PHYS_TO_K1(TSTADR);addr<(u_char *)PHYS_TO_K1(TSTADR + LINES);addr++) {
		if (*addr != 0x5a) {
			/*Eprintf("Unexpected Cache write through to memory on store byte\n");*/
			err_msg( SDIRTYWBB_ERR1, cpu_loc);
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %2x, actual = %2x, XOR %2x\n",
				0x5a, *addr, *addr ^ 0x5a );
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}

	/* init memory before reading it */
	for (addr = (u_char *)PHYS_TO_K1(TSTADR);
	    addr<(u_char *)PHYS_TO_K1(TSTADR + LINES);addr++) {
		*addr = 0xaa;
	}
	/* replace cache line, 4 words each */
	for(addr = (u_char *)PHYS_TO_K0(ALIASADR); addr < (u_char *)PHYS_TO_K0(ALIASADR + LINES); ) {
		data = *addr;
		addr = (u_char *)(((u_int)addr) + config_info.sidline_size);
	}
	/* read back data through uncached space, it should write back to memory */
	for (addr = (u_char *)PHYS_TO_K1(TSTADR),data=0;
	    addr<(u_char *)PHYS_TO_K1(TSTADR + LINES);addr++,data += 0x1) {
		if (*addr != data) {
			/*Eprintf("Data read replaced a dirty line in Secondary\n");*/
			err_msg( SDIRTYWBB_ERR2, cpu_loc);
			msg_printf( ERR, "Dirty line not written back to memory\n");
			msg_printf( ERR, "addr = %x\n", addr);
			msg_printf( ERR, "expected = %2x, actual = %2x, XOR %2x\n",
				data, *addr, data ^ *addr );
			msg_printf( ERR, "Seconday TAG %x\n", IndxLoadTag_SD((__psunsigned_t)addr) );
			retval = FAIL;
		}
	}
	msg_printf(INFO, "Completed secondary dirty writeback byte test\n");
	return(retval);

}
