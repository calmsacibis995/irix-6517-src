
/*
 * path/cache_stress.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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
 * cache_stress -- 
 *
 * Second cache stress test. CPU0 write/read to one word in every page
 * through 0x80000000 space.
 */

#include "pattern.h"
#include <sys/cpu.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "ip19.h"
#include "cache.h"
#include "prototypes.h"

static __psunsigned_t  cpu_loc[2];

struct addr_range addr_range;
#define LSHIFT	16

extern void mem_init(__psunsigned_t, __psunsigned_t, uint, uint);
extern uint ml_hammer_scache(uint, __psunsigned_t);
extern uint ml_h_sc_end();
extern uint ml_hammer_pdcache(uint, __psunsigned_t);
extern uint ml_h_pdc_end();


/* Second Data Cache */
cache_stress()
{
	register unsigned int i, j;
	int fail = 0;
	register int data;
	register __psunsigned_t mem_address;
	register int tmp;
	register int shiftit = set_shift();

	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (cache_stress) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	setconfig_info();

	msg_printf(VRB, "Clear memory\n");	/* clear memory */
	mem_init(addr_range.lo, addr_range.hi, M_I_PATTERN, 0);

	/* write/read one location in every 64k */
	msg_printf(VRB, "Write/read one location in every 64k\n");
	for (j = 0; j < (1<<LSHIFT); j++) {
		if ((j % 100) == 0)
			msg_printf(VRB, "Loop %d\r", j);
#ifdef NEVER
		for (i = 0x10; i <= 0x07f; i++) {
#endif
		for (i=(addr_range.lo>>LSHIFT);i<(addr_range.hi>>LSHIFT);i++) {

			/* set up memory address */

			mem_address = i << LSHIFT;
			tmp = j+i;
			while (tmp > 0xfff)
				tmp -= 0xfff;
			mem_address |= ((tmp) << shiftit);
			c_wwrite(mem_address, (uint)mem_address);
		}
#ifdef NEVER
		for (i = 0x10; i <= 0x07f; i++) {
#endif
		for (i=(addr_range.lo>>LSHIFT);i<(addr_range.hi>>LSHIFT);i++) {

			/* set up memory address */

			mem_address = i << LSHIFT;
			tmp = j+i;
			while (tmp > 0xfff)
				tmp -= 0xfff;
			mem_address |= ((tmp) << shiftit);
			if ((data = c_wread(mem_address)) != mem_address) {
				fail = 1;
				err_msg(CSTRESS_ERR, cpu_loc, mem_address, mem_address, data);
			}
		}
	}
	ide_invalidate_caches();
	msg_printf(VRB, "\n");
	msg_printf(INFO, "Completed stress test for caches\n");
		
	return(fail);
}


hammer_pdcache()
{
	__psunsigned_t fnstart = (__psunsigned_t)ml_hammer_pdcache;
	__psunsigned_t fnend = (__psunsigned_t)ml_h_pdc_end;
	__psunsigned_t iline_start, iline_end;
	int fail = 0;
	__psunsigned_t  osr = get_sr();
	uint uints5[5];

        setconfig_info();
	msg_printf(INFO, " (hammer_pdcache) \n");

	msg_printf(DBG, "fstart 0x%x, fend 0x%x\n",fnstart, fnend);

	set_sr (osr & ~SR_DE);
	msg_printf(DBG, "SR set to 0x%x\n", get_sr());

	/* rnd start down to line-addr */
	iline_start = (fnstart & ~(PIL_SIZE-1));
	iline_end = ((fnend & ~(PIL_SIZE-1)) + PIL_SIZE);

	msg_printf(DBG, "istart 0x%x, iend 0x%x\n", iline_start, iline_end);

	setup_icache(iline_start, iline_end);
	msg_printf(DBG, "After setup_icache\n");

	fail = ml_hammer_pdcache(50000, (__psunsigned_t)uints5);
	if (fail) 
		err_msg(CSTRESS_PDERR, cpu_loc, uints5[0], uints5[1], uints5[2]);

	msg_printf(DBG, "End addr 0x%x, iterations 0x%x\n", uints5[3], uints5[4]);
	ide_invalidate_caches();
	set_sr(osr);

	msg_printf(INFO, "Completed primary D-cache stress test\n");
	return(fail);

} /* hammer_pdcache */


hammer_scache()
{
	__psunsigned_t fnstart = (__psunsigned_t)ml_hammer_scache;
	__psunsigned_t fnend = (__psunsigned_t)ml_h_sc_end;
	__psunsigned_t iline_start, iline_end;
	int fail = 0;
	__psunsigned_t  osr = get_sr();
	uint uints5[5];

    	setconfig_info();
	msg_printf(INFO, " (hammer_scache) \n");

	msg_printf(DBG, "fstart 0x%x fend 0x%x\n", fnstart, fnend);

	set_sr (osr & ~SR_DE);
	msg_printf(DBG, "SR set to 0x%x\n", get_sr());

	/* rnd start down to line-addr */
	iline_start = (fnstart & ~(PIL_SIZE-1));
	iline_end = ((fnend & ~(PIL_SIZE-1)) + PIL_SIZE);

	msg_printf(DBG, "istart 0x%x, iend 0x%x\n", iline_start, iline_end);

	setup_icache(iline_start, iline_end);
	msg_printf(DBG, "After setup_icache\n");

	fail = ml_hammer_scache(500, (__psunsigned_t)uints5);
	if (fail)
		err_msg(CSTRESS_SERR, cpu_loc, uints5[0], uints5[1], uints5[2]);

	msg_printf(DBG, "End addr 0x%x, iterations 0x%x\n", uints5[3], uints5[4]);
	ide_invalidate_caches();
	set_sr(osr);

	msg_printf(INFO, "Completed secondary cache stress test\n");
	return(fail);

} /* hammer_scache */

