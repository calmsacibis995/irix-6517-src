
/*
 * mc3/mem_ecc.c
 *
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

#ident "$Revision: 1.27 $"

/*
 *      mem_ecc
 *
 *      Initialize memory through uncached space
 *      and read them back through cached space.  
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "pattern.h"
#include "memstr.h"
#include "exp_parser.h"
#include "mc3_reg.h"
#include "libsk.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "uif.h"
#include "prototypes.h"


#define MASK		0xffffffffffffffff
#define NUMPATTERNS 20

jmp_buf ecc_buf;
extern uint mem_slots;
extern long long vir2phys(__psunsigned_t);

int ecc_test(struct memstr *, int, unsigned long long, unsigned long long,
		unsigned long long, int);

int
mem_ecc(void)
{
    unsigned long long memsize, i;
    __psunsigned_t himem, paddr;
    int first_time;
    unsigned int fail = 0;
    struct memstr addr;
    register int inc = 4;
    int val;
    __psunsigned_t osr;

    msg_printf(SUM, "Memory ECC test\n");
    if (quick_mode()) {
	  msg_printf(SUM, "\nRunning in quick mode...\n");
        inc = QUICK_INC_ECC;
    }

    mem_slots = mc3slots();
    mem_err_clear(mem_slots); /* clear error regs */
    memtest_init();
    if (val = setjmp(ecc_buf)) {
        if (val == 1){
                msg_printf(ERR, "Unexpected exception during memory test\n");
                show_fault();
        }
	mem_err_log();
   	mem_err_show();
	/* everest_error_show(); */
	fail = 1;
	goto SLOTFAILED;
    }
    else {
	date_stamp();
	set_nofault(ecc_buf);
	memsize = readconfig_reg(RDCONFIG_MEMSIZE,0);
	first_time = 1;
    /* since this test is doing cached-data-accesses,
     * turn off DE bit so that ECC errors will not be automatically corrected
     */
    msg_printf(VRB, "Turning off ECC correction\n");
    osr = get_sr();
    set_sr(osr & ~SR_DE);

	/* Enable the bouncing cursor via the R4k timer interrupt
	 * every four seconds
	 */
	InstallHandler( INT_Level5, TimerHandler);
	EnableTimer(4);

#if _MIPS_SIM == _ABI64
	paddr = PHYS_TO_MEM(PHYS_CHECK_LO);
	addr.lomem = paddr;
	himem = PHYS_TO_MEM(memsize);
	addr.himem = himem;
	addr.mask = (__psunsigned_t)MASK;
	msg_printf(VRB, "lo:%llx, hi:%llx, p: %llx, inc: %d\n",
			 addr.lomem, addr.himem, paddr, inc);
	fail |= ecc_test(&addr, inc, paddr, i, memsize, first_time);
#else
	for (i = 0; i < memsize; i += DIRECT_MAP) {
		if (i < (unsigned long long)PHYS_CHECK_LO)
			paddr = (__psunsigned_t)PHYS_CHECK_LO;
		else
			paddr = (__psunsigned_t)i;
		addr.lomem = map_addr(paddr, DIRECT_MAP, UNCACHED);
		msg_printf(VRB, "paddr = 0x%x  lomem = 0x%x\n", 
			paddr, addr.lomem);
		if (memsize < (i + (unsigned long long)DIRECT_MAP)) {
    		   himem = (addr.lomem + (memsize - (unsigned long long)paddr));
    		   addr.himem = himem;
		}
		else {
		   himem = (addr.lomem +
			(DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
		   addr.himem = himem;
	   	   /* first_time = 0; */
		}
	        msg_printf(VRB,"i = 0x%llx  himem = 0x%x  inc = 0x%x\n", i, addr.himem, inc);
    		addr.mask = MASK;

        	fail |= ecc_test(&addr, inc, paddr, i, memsize, first_time);
		if (memsize >= (i + (unsigned long long)DIRECT_MAP)) 
			first_time =  0;
	}
#endif

	/* Put back the orginal fault handler */
	RemoveHandler( INT_Level5);
	StopTimer();
    } /* else... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory ECC test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    msg_printf(INFO, "Restoring ECC correction\n");
    set_sr(osr);    /* restore old setting of Status register */
    clear_nofault();
    date_stamp();
    return (fail);
}

int
ecc_test(struct memstr *memptr, int inc, unsigned long long physaddr, 
	unsigned long long base, unsigned long long memsize, int first_time)
{
        register __psunsigned_t lomem = memptr->lomem;
        register __psunsigned_t himem = memptr->himem;
        register uint mask = (uint)(memptr->mask);
        register __psunsigned_t pmem = lomem;
        register __psunsigned_t pmemhi = himem;
        register int fail = 0;
        register int k;
	int i, slot;
        register int data;
        uint badbits = 0;
        unsigned long long badaddr, hi, ptr;

    unsigned int addressForCheckBits[] = {
        0x0,            /* data = 0x0 creates check bit pattern of 0x0 */
        0x1d,           /* data = 0x1d creates check pattern of 0xff */
        0x500000,       /* data = 0x500000 creates check pattern of 0x55 */
        0xa0000000,     /* data = a0000000 creates check bit pattern of aa */
        0x1,            /* data = 1 creates check bit pattern of 13 */
        0x2,            /* data = 2 creates check bit pattern of 23 */
        0x4,            /* data = 4 creates check bit pattern of 43 */
        0x8,            /* data = 8 creates check bit pattern of 83 */
        0x1000,         /* data = 1000 creates check bit pattern of 61 */
        0x2000,         /* data = 2000 creates check bit pattern of 62 */
        0x4000,         /* data = 4000 creates check bit pattern of 64 */
        0x8000,         /* data = 8000 creates check bit pattern of 68 */
        0x7,            /* data = 7 creates check bit pattern of 73 */
        0xb,            /* data = b creates check bit pattern of b3 */
        0xd,            /* data = d creates check bit pattern of d3 */
        0xe,            /* data = e creates check bit pattern of e3 */
        0x86,           /* data = 86 creates check bit pattern of 67 */
        0x8002,         /* data = 8002 creates check bit pattern of 6b */
        0x1040,         /* data = 1040 creates check bit pattern of 6d */
        0xe000          /* data = e000 creates check bit pattern of 6e */
        };

    /* toggle check bits by writing data patterns which cause the check bits
     * to cycle through the following patterns:
     *
     * 0, ff, 55, aa,
     * 83, 43, 23, 13 (walk 1 on check byte1),
     * 68, 64, 62, 61 (walk 1 on check byte0)
     * 73, b3, d3, e3 (walk 0 on check byte1),
     * 67, 6b, 6d, 6e (walk 0 on check byte0)
     *
     * The check bits are determined by the following:
     *
     *
     *  ECC matrix formation
     *
     * assign       Row7 = 64'hff28 0ff0 8888 0928;     Check bit 7
     * assign       Row6 = 64'hfa24 000f 4444 ff24;     Check bit 6
     * assign       Row5 = 64'h0b22 ff00 2222 fa32;     Check bit 5
     * assign       Row4 = 64'h0931 f0ff 1111 0b21;     Check bit 4
     * assign       Row3 = 64'h84d0 8888 ff0f 8c50;     Check bit 3
     * assign       Row2 = 64'h4c9f 4444 00ff 44d0;     Check bit 2
     * assign       Row1 = 64'h24ff 2222 f000 249f;     Check bit 1
     * assign       Row0 = 64'h1450 1111 0ff0 14ff;     Check bit 0
     *
     * f   e   d   c   b   a   9   8   7   6   5    4    3    2    1    0
     * 84218421842184218421842184218421842184218421 8421 8421 8421 8421 8421 B
     * xxxxxxxx  x x       xxxxxxxx    x   x   x    x         x  x   x  x    7
     * xxxxx x   x  x              xxxx x   x   x    x   xxxx xxxx   x   x   6
     *     x xx  x   x xxxxxxxx          x   x   x    x  xxxx x x    xx   x  5
     *     x  x  xx   xxxxx    xxxxxxxx   x   x   x    x      x xx   x     x 4
     * x    x  xx x    x   x   x   x   xxxxxxxx     xxxx x    xx    x x      3
     *  x  xx  x  xxxxx x   x   x   x          xxxx xxxx  x    x   xx x      2
     *   x  x  xxxxxxxx  x   x   x   x xxxx                x   x   x  x xxxx 1
     *    x x   x x       x   x   x   x    xxxxxxxx         x  x   xxxx xxxx 0
     *
     *
     * Running the genECCaddrs program generated the following output for us:
     *
     * data = 0 creates cbitpat 0
     * data = 1d creates cbitpat ff
     * data = 500000 creates cbitpat 55
     * data = a0000000 creates cbitpat aa
     * data = 1 creates cbitpat 13
     * data = 2 creates cbitpat 23
     * data = 4 creates cbitpat 43
     * data = 8 creates cbitpat 83
     * data = 1000 creates cbitpat 61
     * data = 2000 creates cbitpat 62
     * data = 4000 creates cbitpat 64
     * data = 8000 creates cbitpat 68
     * data = 7 creates cbitpat 73
     * data = b creates cbitpat b3
     * data = d creates cbitpat d3
     * data = e creates cbitpat e3
     * data = 86 creates cbitpat 67
     * data = 8002 creates cbitpat 6b
     * data = 1040 creates cbitpat 6d
     * data = e000 creates cbitpat 6e
     */

    msg_printf(INFO, "Writing patterns to toggle check bits\n");
    for (i = 0, ptr = lomem; i < NUMPATTERNS; i++, ptr += 4)
    {
msg_printf(DBG,"ptr is 0x%x, lomem is 0x%x\n", ptr, lomem);
msg_printf(DBG,"addrfor cbits is 0x%x\n", addressForCheckBits[i]);
        *(uint *)ptr = addressForCheckBits[i];
    }

        msg_printf(INFO, "\nWriting uncached from ");
#if _MIPS_SIM == _ABI64
        msg_printf(INFO, "0x%llx to 0x%llx\n", lomem, himem);
#else
        msg_printf(INFO, "0x%llx to 0x%llx\n",
                physaddr, physaddr + (himem-lomem));
#endif

        while( pmem < pmemhi ) {
                data = (uint)pmem;
                *(uint *)pmem = data & mask;
/*		wcheck_mem(pmem);
*/
                pmem += inc;
        }
        msg_printf(INFO, "Read uncached\n");

        pmem = lomem;
        msg_printf(VRB,"pmem for verify starts at 0x%x\n", pmem);
        while( pmem < pmemhi) {
                data = (uint)pmem;
/*                if ((pmem % (__psunsigned_t)0x10000) == 0)
			msg_printf(VRB,"addr 0x%x data 0x%x\r", pmem, data);
*/
                if( (k = *(uint *)pmem & mask) != (data & mask)) {
			badbits = k ^ (data & mask);
			msg_printf(ERR, "Read uncached:");
			badaddr = pmem;
			decode_address(K1_TO_PHYS(badaddr), badbits, SW_WORD, (int *)&slot);
			err_msg(MEM_ECC_ERR, &slot, badaddr, data & mask, k);
			fail ++;
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
                }
                pmem += inc;
        }
#ifndef DEBUGECC
#ifdef IP19
	/* set up uncached space */
	lomem = map_addr((__psunsigned_t)physaddr, DIRECT_MAP, CACHED);
	msg_printf(VRB, "physaddr = 0x%x  lomem = 0x%x\n", physaddr, lomem);
        if (memsize < (base + (unsigned long long)DIRECT_MAP)) {
                   hi = (lomem + (memsize - physaddr));
                   himem = hi;
        }
        else {
                   hi = (lomem +
                        (DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
                   himem = hi;
        }
	        msg_printf(VRB,"himem = 0x%x\n", himem);
#endif

        msg_printf(INFO, "Read cached\n");
	mask = 0x1fffffff; /* ignore the leading a0... for now */

        pmem = lomem;
        msg_printf(VRB,"pmem for verify starts at 0x%x\n", pmem);
        while( pmem < pmemhi) {
                data = (uint)pmem;
/*                if ((pmem % (__psunsigned_t)0x10000) == 0)
			msg_printf(VRB,"addr 0x%x data 0x%x\r", pmem, data);
*/
                if( (k = *(uint *)pmem & mask) != (data & mask)) {
			badbits = k ^ (data & mask);
			msg_printf(ERR, "Read cached:");
			badaddr = pmem;
			decode_address(K1_TO_PHYS(badaddr), badbits, SW_WORD, (int *)&slot);
			err_msg(MEM_ECC_ERR, &slot, badaddr, data & mask, k);
			fail ++;
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
                }
                pmem += inc;
        }
#endif /* DEBUGECC */
        return(fail);

} /* ecc_test */
