
/*
 * mc3/memtests.c
 *
 *
 * Copyright 1991, 1992, Silicon Graphics, Inc.
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

#ident "$Revision: 1.24 $"


#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <values.h>
#include "pattern.h"
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "uif.h"
#include "memstr.h"
#include "tlb.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <prototypes.h>

extern jmp_buf mem_buf;

extern long long vir2phys(__psunsigned_t);
extern unsigned int pow(unsigned int, unsigned int);
extern int wfail_mem(__psunsigned_t, __psunsigned_t, __psunsigned_t, int);
extern __psunsigned_t WriteLoop14(__psunsigned_t, __psunsigned_t, uint, uint, uint);
extern int chk_memereg(int);
extern void dwcheck_mem(__psunsigned_t);
extern int dwfail_mem(__psunsigned_t, uint, uint, int);

int 	walking_1(struct memstr *, register int, unsigned int, enum bitsense);
int 	bwalking_addr(struct memstr *, int);
int 	addr_pattern(struct memstr *, int, enum bitsense);
int 	read_wr(struct memstr *, unsigned int);
int 	marchX(struct memstr *, int);
int 	marchY(struct memstr *, int);
int 	dblmarchY(struct memstr *, int);
int 	bread_wr(struct memstr *, unsigned int, unsigned long long);
int 	Kh_l(__psunsigned_t, __psunsigned_t);
int 	Kh_dbl(__psunsigned_t, __psunsigned_t);
static int dwd_3bit(__psunsigned_t, __psunsigned_t, u_int, u_int, u_int, u_int, u_int);
static int threebit(u_int, u_int, u_int, __psunsigned_t, __psunsigned_t, u_int);
unsigned int mem_slots;

/*
 *****************************************************************************
 * note that most of the routines that call walking_1 set lomem==himem,
 * expecting to test one word only: the termination condition of the 
 * for loop below MUST be <= */

int
walking_1(struct memstr *memptr, register int bitcount, unsigned int inc, 
	  enum bitsense sense)
{
	register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
	register unsigned int mask = (uint)(memptr->mask);
    	register unsigned int testmask;
    	register unsigned int walkmask = 1;
	int failed;
	register unsigned int resettestmask;
	register __psunsigned_t ptr;
	register int count = bitcount;
	unsigned int invertORmask;

	if (sense == BIT_INVERT)
	   msg_printf(INFO,"\nWalking 0 from ");
	else
	   msg_printf(INFO,"\nWalking 1 from ");
	msg_printf(INFO, "0x%x to 0x%x inc 0x%x\n", lomem, himem, inc);
		
	invertORmask = pow(2, count-1); /* 2 to the (count-1)th */
	invertORmask --; /* 2^(count-1) - 1 */
	msg_printf(DBG, "invertORmask is 0x%x\n", invertORmask);
	failed = 0;
    	while (--bitcount >= 0) {
	    register unsigned int value;

	    testmask = (sense == BIT_INVERT ? ~walkmask : walkmask);
	    msg_printf(INFO,"Walking pattern 0x%x\n", testmask);
	    /* the termination-check must be <= to include himem address */
	    /* write to memory */
	    msg_printf(INFO,"Writing pattern to memory... ");
	    for (ptr = lomem; ptr < himem; ptr += inc) {
	    	testmask &= mask;
	    	*(uint *)ptr = testmask;
/*		wcheck_mem(ptr);	*/

		if (resettestmask = (sense == BIT_INVERT ? 
			testmask <= ((0xfffffffe << (count-1))|invertORmask) :
			testmask >= (1 << (count -1)))) {
#if DEBUG8
			msg_printf(VRB,"testmask is 0x%x ", testmask);
			msg_printf(VRB,"sense is ");
			if (sense == BIT_INVERT) {
			  	msg_printf(VRB,"bit-invert\n");
			}
			else {
				msg_printf(VRB,"bit-true\n");
				msg_printf(VRB,
				 "1 << (count -1) is 0x%x\n", 1 << (count -1));
			}
	  		msg_printf(VRB,"and count is %d\n", count );
#endif
			testmask = 1;
	    		testmask =(sense == BIT_INVERT ? ~testmask : testmask);
		}
		else {
		 	if (sense == BIT_INVERT)
				testmask = (testmask <<= 1) | 0x1;
			else
		    		testmask <<= 1;
		}
	    };
	    /* read back from memory */
	    msg_printf(INFO, "Verifying data\n");
		/* reset testmask for verifying step */
	    testmask = (sense == BIT_INVERT ? ~walkmask : walkmask);
	    for (ptr = lomem; ptr < himem; ptr += inc) {
	    	value = *(uint *)ptr;
	    	value &= mask;
	    	if (value != testmask)  {
			failed += wfail_mem(ptr, testmask, value, MEMWLK_ERR);
	    	}
		/* if (testmask >= (1 << (count -1))) { */
		if (resettestmask = (sense == BIT_INVERT ? 
			testmask <= ((0xfffffffe << (count-1))|invertORmask)  :
			testmask >= (1 << (count -1)))) {
			testmask = 1;
	    		testmask =(sense == BIT_INVERT ? ~testmask : testmask);
		}
		else {
                        if (sense == BIT_INVERT)
                                testmask = (testmask <<= 1) | 0x1;
                        else
                                testmask <<= 1;
                }
	    }
	    walkmask <<= 1;
	}
	msg_printf(INFO, "\n");
	return(failed);

} /* walking_1 */


/* walking_addr checks for shorts and open address lines.
 * Address lines that are >= the most significant address
 * lines of the limits are not tested.  The test is conducted
 * by doing byte reads/writes from lomem up to himem.
 */
int
walking_addr(struct memstr *memptr, int start)
{
    register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
    register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
    unsigned int mask = (uint)(memptr->mask);
    register unsigned int k;
    register __psunsigned_t pmem, refmem;
    unsigned int testline;
    int fail = 0;

    msg_printf(DBG,"\nWalking Address test from 0x%x to 0x%x\n",lomem,himem);

    refmem = lomem;
    for (testline = start; (lomem|testline) < himem; testline <<= 1) {
	if (testline >= K0SEG) break;
	*(int *)refmem = 0x55;
	pmem = lomem | testline;
	if( pmem == refmem )
		continue;
	*(int *)pmem = 0xaa;
	k = *(int *)refmem; 
#ifdef DEBUG8
printf("Writing 0x55 to 0x%x and 0xaa to 0x%x\r", (int)refmem,(int)pmem );
#endif
	if ( (k  & mask) != 0x55 ) {
	    printf("\nError: Wrote 0x55 to 0x%x then wrote 0xaa to 0x%x; ",
	        (__psunsigned_t)refmem,(__psunsigned_t)pmem);
	    printf("read back 0x%x from 0x%x\n",k,(__psunsigned_t)refmem);
	    printf("\t\tAddress line 0x%x might be bad\n",testline);
	    fail = 1;
	}
    }
    return(fail);
} /* fn walking_addr */
	
int
bwalking_addr(struct memstr *memptr, int start)
{
    register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
    register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
    register unsigned int mask = (uint)(memptr->mask);
    register unsigned int k;
    register __psunsigned_t pmem, refmem;
    register __psunsigned_t testline;
    register int fail = 0;
    int slot;
    __psunsigned_t badaddr;

    msg_printf(INFO,"\nWalking Address test from 0x%x to 0x%x\n",
                lomem, himem);

    refmem = lomem;
    for (testline = (__psunsigned_t)start; ((lomem | testline) < himem); testline <<= 1) {
#if _MIPS_SIM != _ABI64
	if (testline >= K0SEG)
		break;
#endif
	*(char *)refmem = 0x55;
	pmem = lomem | testline;
	if( pmem == refmem )
		continue;
	*(char *)pmem = 0xaa;
	msg_printf(VRB,"Writing 0x%x to 0x%x and 0x%x to 0x%x\n", 
		*(char *)refmem, refmem, 
		*(char *)pmem, pmem);
/*	if ((pmem % 0x80) == 0)
        	if (cpu_intr_pending()) {
			   badaddr = vir2phys(pmem);
                           decode_address(badaddr,0, SW_WORD, &slot);
                           err_msg(MC3_INTR_ERR, &slot, badaddr);
			    if (chk_memereg(mem_slots)) {
				check_memereg(mem_slots);
				mem_err_clear(mem_slots);
			    }
		}
*/
	k = *(char *)refmem; 
	if ( (k & mask) != 0x55 ) {
		fail = 1;
		badaddr = K1_TO_PHYS(refmem);
		decode_address(K1_TO_PHYS(badaddr), 0, SW_WORD, &slot);
	    	err_msg(MEMADDR_ERR, &slot, badaddr, 0x55, k);
	    	err_msg(MEMADDR_HINT, &slot, testline);
		if (chk_memereg(mem_slots)) {
		    check_memereg(mem_slots);
		    mem_err_clear(mem_slots);
		}
	    return (fail);
	}
    }
    return(fail);
} /* fn bwalking_addr */
	
int
addr_pattern(struct memstr *memptr, int inc, enum bitsense sense)
/* 
 * Address in address algorithm 
 *
 * sense =  address pattern or inv_pattern 
 * physaddr = starting physical addr of the section of memory to test
 */
{
	register __psunsigned_t lomem = memptr->lomem;
	register __psunsigned_t himem = memptr->himem;
	register __psunsigned_t mask = memptr->mask;
	register __psunsigned_t pmem = lomem;
	register __psunsigned_t pmemhi = himem;
	int fail = 0;
	register __psunsigned_t k, data;
	register int incc;

	incc= inc;

	if (sense == BIT_INVERT) {
		msg_printf(INFO, "\nWriting inverted address pattern from ");
	}
	else {
	        msg_printf(INFO, "\nWriting address pattern from ");
	}
	msg_printf(INFO, "0x%x to 0x%x\n", lomem, himem);

	while( pmem < pmemhi ) {
		data = (sense == BIT_INVERT ? ~(uint)pmem : (uint)pmem);
		data &= mask;
		*(uint *)pmem = data;
		pmem += incc;
	}

	if (sense == BIT_INVERT)
		msg_printf(INFO, "Read back & verify invert addr pattern\n");
	else
		msg_printf(INFO, "Read back & verify addr pattern\n");

	pmem = lomem;
	msg_printf(DBG,"pmem for verify starts at 0x%x\n", pmem);
	while( pmem < pmemhi) {
		data = (sense == BIT_INVERT ? ~(uint)pmem : (uint)pmem);
		data &= mask;
		if( (k = *(uint *)pmem & mask) != data ) {
		    fail += wfail_mem(pmem, data & mask, k, ADDRU_ERR);
		}
		pmem += incc;
	}
	return(fail);

} /* addr_pattern */


int
read_wr(struct memstr *memptr, unsigned int inc)
{
	register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
	register uint mask = (uint)(memptr->mask);
	register __psunsigned_t data;
	register __psunsigned_t ptr, hiptr;
	register int fail = 0;
	register unsigned int incc;


	incc = inc;

	msg_printf(INFO, "\nWrite 0's from 0x%x to 0x%x\n", lomem, himem);

	for (ptr = lomem; ptr < himem; ptr += incc ) {
		*(uint *)ptr = 0;
	}

	/* from lomem to himem read and verify 0's, then write 1's */
	msg_printf(INFO, "Verify 0's, write 1's from 0x%x to 0x%x\n",
                lomem, himem);
	for (ptr = lomem ; ptr < himem; ptr += incc){
		if ((data = *(uint *)ptr & mask) != 0) {
			fail += wfail_mem(ptr, 0, data, MEMWRRD_ERR);
		}
		*(uint *)ptr = -1;
	}
	hiptr = ptr - incc;

	/* from himem to lomem read and verify 1's, then write 0x5a5a5a5a  */
	msg_printf(INFO,"Verify 1's, write 5a5a5a5a from 0x%x to 0x%x\n",
            himem, lomem);

	/* decrement 1 since ptr should be himem-1 */
	for (ptr = hiptr ; ptr >= lomem; ptr -= incc) {
		if ((data = *(uint *)ptr & mask) != mask) {
		    fail += wfail_mem(ptr, mask, data, MEMWRRD_ERR);
		}
		*(uint *)ptr = 0x5a5a5a5a;
	}
		
	/* from lomem to himem read 0x5a5a5a5a,then write 0xa5a5a5a5 */
	msg_printf(INFO,"Verify 5a5a5a5a, write a5a5a5a5 from 0x%x to 0x%x\n",
             lomem, himem);
	for (ptr = lomem ; ptr < himem; ptr += incc) {
		if ((data = *(uint *)ptr & mask) != (0x5a5a5a5a & mask)) {
		    fail += wfail_mem(ptr,0x5a5a5a5a & mask,data, MEMWRRD_ERR);
		}
		*(uint *)ptr = 0xa5a5a5a5;
	}
	hiptr = ptr - incc;
		
	/* from himem to lomem read  and verify 0xa5a5a5a5, */
	msg_printf(INFO,"Verify a5a5a5a5 from 0x%x to 0x%x\n", 
            himem, lomem);
	for (ptr = hiptr ; ptr >= lomem; ptr -= incc)
		if ((data = *(uint *)ptr & mask) != (0xa5a5a5a5 & mask))  {
		    fail += wfail_mem(ptr, 0xa5a5a5a5 & mask,data,MEMWRRD_ERR);
		}
	return(fail);

} /* fn read_wr */


int
bread_wr(struct memstr *memptr, unsigned int incc, unsigned long long physaddr)
{
	register __psunsigned_t lomem = (__psunsigned_t) memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t )memptr->himem;
	register unsigned int mask = (uint)(memptr->mask);
	register unsigned char data;
	register __psunsigned_t ptr, hiptr;
	register unsigned int inc;
	int fail = 0;

	inc= incc;
        msg_printf(INFO, "\nByte write 0's from 0x%x to 0x%x\n",
                physaddr, physaddr + (himem-lomem));

	for (ptr = lomem; ptr < himem; ptr += inc ) {
		*(char *)ptr = 0;
	}


	/* from lomem to himem read and verify 0's, then write 1's */
        msg_printf(INFO, "Verify 0's, write 1's from 0x%x to 0x%x\n",
                physaddr, physaddr + (himem-lomem));
	for (ptr = lomem ; ptr < himem; ptr += inc){
		if ((data = *(char *)ptr & mask) != 0) {
		    fail += bfail_mem(ptr, 0, data, MEMBWRRD_ERR);
		}
		*(char *)ptr = 0xff;
	}
	hiptr= ptr - inc;

	/* from lomem to himem read and verify 1's, then write 0x5a5a5a5a */
	msg_printf(INFO,"Verify 1's, write 5a from 0x%x to 0x%x\n",
                physaddr + (himem-lomem), physaddr);
	for (ptr = hiptr ; ptr >= lomem; ptr -= inc){
		if ((data = *(char *)ptr & mask) != (0xff & mask)) {
		    fail += bfail_mem(ptr, 0xff & mask, data, MEMBWRRD_ERR);
		}
		*(char *)ptr = 0x5a;
	}
		
	/* from lomem to himem read and verify 0x5a5a5a5a,
	 * then write 0xa5a5a5a5 */
	/* himem = (unsigned char *)memptr->himem; */
	msg_printf(INFO,"Verify 5a, write a5 from 0x%x to 0x%x\n", 
                physaddr, physaddr + (himem-lomem));
	for (ptr = lomem ; ptr < himem; ptr += inc) {
		if ((data = *(char *)ptr & mask) != (0x5a & mask)) 
		    fail += bfail_mem(ptr, 0x5a & mask, data, MEMBWRRD_ERR);
		*(char *)ptr = 0xa5;
	}
	hiptr = ptr - inc;
		
	/* from lomem to himem read and verify 0xa5a5a5a5 */
	msg_printf(INFO,"Verify a5 from 0x%x to 0x%x\n", 
                physaddr + (himem-lomem), physaddr);
	for (ptr = hiptr ; ptr >= lomem; ptr -= inc)
		if ((data = *(char *)ptr & mask) != (0xa5 & mask))  
		    fail += bfail_mem(ptr, 0xa5 & mask, data, MEMBWRRD_ERR);

	return(fail);

} /* fn bread_wr */
		

/* Described in van de Goor's book, "Testing Semiconductor Memories" and 
 * has the following flow:
 * 
 * (w0), u(r0,w1), d(r1,w0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and inversion coupling faults
 */
int
marchX(struct memstr *memptr, int inc)
{
	register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
	register uint mask = (uint)(memptr->mask);
	register uint data;
	register int fail = 0;
 	__psunsigned_t ptr, hiptr;
	register unsigned incc;


	incc = inc;

	msg_printf(INFO, "\nMarchX Test from 0x%x to 0x%x\n",
		lomem, himem);

 	/* from lomem to himem, write 0 */
	msg_printf(INFO, "Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr+=incc) {
		*(uint *)ptr = 0;
	}

	/* from lomem to himem read and verify 0's, then write 1's */
	msg_printf(INFO, "Verify 0's and then write 1's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc){
		if ((data = *(uint *)ptr & mask) != 0) {
			fail += wfail_mem(ptr, 0, data, MARCHX_ERR);
		}
		*(uint *)ptr = -1;
	}
	hiptr = ptr - incc;

	/* from himem to lomem read and verify 1's, then write 0 */
	msg_printf(INFO, "Verify 1's and then write 0\n");
	for (ptr = hiptr ; ptr >= lomem; ptr-=incc) {
		if ((data = *(uint *)ptr & mask) != mask) {
		    fail += wfail_mem(ptr, mask, data, MARCHX_ERR);
		}
		*(uint *)ptr = 0x0;
	}
		
	/* from lomem to himem, verify 0 */
	/* himem = memptr->himem; */
	msg_printf(INFO, "Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if ((data = *(uint *)ptr & mask) != 0x0) {
		    fail += wfail_mem(ptr, 0, data, MARCHX_ERR);
		}
	}
	return (fail);
} /* marchX */


/* MarchY
 * Described in van de Goor's book, "Testing Semiconductor Memories" and 
 *
 * (w0), u(r0,w1,r1), d(r1,w0,r0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and linked transition faults
 */

marchY(struct memstr *memptr, int inc)
{
	register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
	register unsigned int mask = (uint)(memptr->mask);
	register uint data;
	register int fail = 0;
	__psunsigned_t ptr, hiptr;
	register int incc;

	incc = inc;
	msg_printf(INFO,"\nMarchY test from 0x%x to 0x%x\n",
		lomem, himem);

	/* from lomem to himem, write 0 */
	msg_printf(INFO,"Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr+=incc) {
		*(uint *)ptr = 0;
	}
	/* from lomem to himem read and verify 0's, write 1's, read 1's */
	msg_printf(INFO,"Verify 0's, write 1's, then verify 1's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if ((data = *(uint *)ptr & mask) != 0x0) {
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
		*(uint *)ptr = -1;
		if ((data = *(uint *)ptr & mask) != mask) {
			wfail_mem(ptr, mask, data, MARCHY_ERR);
		}
	}
	hiptr = ptr - incc;

	/* from himem to lomem read and verify 1's, write 0's, read 0's */
	msg_printf(INFO,"Verify 1's and then write 0's\n");
	for (ptr = hiptr ; ptr >= lomem; ptr-=incc) {
		if ((data = *(uint *)ptr & mask) != mask) {
			fail += wfail_mem(ptr, mask, data, MARCHY_ERR);
		}
		*(uint *)ptr = 0;
		if ((data = *(uint *)ptr & mask) != 0x0) {
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
	}
		
	/* himem = memptr->himem; */
	msg_printf(INFO,"Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr+=incc) {
		if ((data = *(uint *)ptr & mask) != 0x0) {
			fail += wfail_mem(ptr, 0, data, MARCHY_ERR);
		}
	}
	return (fail);
} /* marchY */

 
/* double word MarchY
 * Described in van de Goor's book, "Testing Semiconductor Memories" and 
 *
 * (w0), u(r0,w1,r1), d(r1,w0,r0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and linked transition faults
 */
int
dblmarchY(struct memstr *memptr, int inc)
{
	register unsigned long long mask = memptr->mask;
	register long long data;
	register int fail = 0;
	register __psunsigned_t lomem = (__psunsigned_t)memptr->lomem;
	register __psunsigned_t himem = (__psunsigned_t)memptr->himem;
	__psunsigned_t ptr;
	__psunsigned_t hiptr;

	msg_printf(INFO,"\nDouble Word MarchY test from 0x%x to 0x%x\n",
		lomem, himem);

	/* from lomem to himem, write 0 */
	msg_printf(INFO,"Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr+=inc) {
		*(long long *)ptr = 0;
/*		dwcheck_mem(ptr);	*/
	}
	/* from lomem to himem read and verify 0's, write 1's, read 1's */
	msg_printf(INFO,"Verify 0's, write 1's, then verify 1's\n");
	for (ptr = lomem ; ptr < himem; ptr+=inc) {
		if ((data = *(long long *)ptr & mask) != 0) {
			fail += dwfail_mem(ptr, 0, data, DBLMARCHY_ERR);
		}
		*(long long *)ptr = -1;
/*		dwcheck_mem(ptr);	*/
		if ((data = *(long long *)ptr & mask) != mask) {
			fail += dwfail_mem(ptr, mask, data, DBLMARCHY_ERR);
		}
		hiptr = ptr;
	}

	/* from himem to lomem read and verify 1's, write 0's, read 0's */
	/* himem--; */
	msg_printf(INFO,"Verify 1's and then write 0's\n");
	for (ptr = hiptr ; ptr >= lomem; ptr-=inc) {
		if ((data = *(long long *)ptr & mask) != mask) {
			fail += dwfail_mem(ptr, mask, data, DBLMARCHY_ERR);
		}
		*(long long *)ptr = 0;
/*		dwcheck_mem(ptr);	*/
		if ((data = *(long long *)ptr & mask) != 0) {
			fail += dwfail_mem(ptr, 0, data, DBLMARCHY_ERR);
		}
	}
		
	/* himem = memptr->himem; */
	msg_printf(INFO,"Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr+=inc) {
		if ((data = *(long long *)ptr & mask) != 0) {
			fail += dwfail_mem(ptr, 0, data, DBLMARCHY_ERR);
		}
	}
	return (fail);
} /* dblmarch_y */

/* mem_init() either sets memory via uncached, unmapped accesses (K1SEG)
 * to the value specified by the 'pattern' parm (M_I_PATTERN mode), an
 * ascending value incremented by 4 per word, with starting value
 * specified by 'pattern' (M_I_ASCENDING) or the one's complement
 * of the ascending value (M_I_ASCENDING_INV).
 */
#if 0
uint
mem_init(uint first_addr, uint last_addr, enum fill_mode mode, uint pattern,
		unsigned long long physaddr)
  /* mode: M_I_PATTERN or M_I_ASCENDING */
  /* pattern: value to fill (M_I_PATTERN) or begin with (M_I_ASCENDING) */
{
    uint count = 0;             /* return # bytes filled */
    int Cached = 0;
    register uint *firstp, *lastp;
    uint *ptr;

#if defined(R4K_UNCACHED_BUG)
    invalidate_dcache(PD_SIZE, PDL_SIZE);
    invalidate_scache(SID_SIZE, SIDL_SIZE);
#endif  R4K_UNCACHED_BUG */

    if ((mode < M_I_PATTERN) || (mode > M_I_ALT_PATTERN)) {
        msg_printf(VRB,"mem_init: illegal mode %d\n",mode);
        return((uint)-1);
    }

    if ((first_addr > MAX_PHYS_MEM) || (last_addr > MAX_PHYS_MEM)) {
        msg_printf(VRB,"mem_init: illegal %s addr 0x%x\n",
        ((first_addr > MAX_PHYS_MEM) ? "starting": "ending"),
        ((first_addr > MAX_PHYS_MEM) ? first_addr : last_addr));
        return((uint)-1);
    }

    /* access via uncached and unmapped segment */
    firstp = (uint *)PHYS_TO_MEM(first_addr);
    lastp = (uint *)PHYS_TO_MEM(last_addr);

    switch(mode) {

    case M_I_PATTERN:   /* write memory with user defined pattern */
        msg_printf(VRB,"\tWrite address pattern 0x%x to 0x%x\r",
                pattern,(uint)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
/*	    if (((uint) ptr % 0x80) == 0)
        	if (cpu_intr_pending())
                	if (!continue_on_error());longjmp(mem_buf, 2);
*/
            count += sizeof(uint);
        }
        break;

    case M_I_ALT_PATTERN:  /* write mem w/ alternating-not user-defined pat */
        msg_printf(VRB,
                "\tWrite alternating-one's comp addr pat 0x%x to 0x%x\r",
                pattern,(uint)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
/*	    if (((uint) ptr % 0x80) == 0)
        	if (cpu_intr_pending())
                	if (!continue_on_error());longjmp(mem_buf, 2);
*/
            pattern = ~pattern;
            count += sizeof(uint);
        }
        break;

    case M_I_ASCENDING:         /* ascending-value fill mode */
        msg_printf(VRB,
                "\tWrite ascending-value pat (start-val 0x%x) from 0x%x\r",
                pattern, (uint)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
/*	    if (((uint) ptr % 0x80) == 0)
        	if (cpu_intr_pending())
                	if (!continue_on_error());longjmp(mem_buf, 2);
*/
            pattern += sizeof(uint);
            count += sizeof(uint);
        }
        break;

    case M_I_ASCENDING_INV:       /* inverted ascending-value fill mode */
        msg_printf(VRB,
            "\tWrite inverse ascending-value pat (start-val 0x%x) from 0x%x\r",
                ~pattern, (uint)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = ~pattern;
/*	    if (((uint) ptr % 0x80) == 0)
        	if (cpu_intr_pending())
                	if (!continue_on_error());longjmp(mem_buf, 2);
*/
            pattern += sizeof(uint);
            count += sizeof(uint);
        }
        break;

    }

#if (SANITY_CHECK == 2)
    msg_printf(VRB,"\nmem_init exits, 0x%x bytes initialized\n",count);
#endif
    return(count);

} /* fn mem_init */
#endif

/*
 *  Knaizuk Hartmann Memory Test
 *
 *  This algorithm is used to perform a fast but non-ehaustive memory test.
 *  It will test a memory subsystem for stuck-at faults in both the address
 *  lines as well as the data locations.  Wired or memory array behavior and
 *  non creative decoder design are assummed.  It makes only 4n memory accesses
 *  where n is the number of locations to be tested.  This alogorithm trades
 *  completeness for speed. No free lunch around here.  Hence this algorithm
 *  is not able to isolate pattern sensitivity or intermittent errors.  C'est
 *  la vie. This algorithm is excellent when a quick memory test is needed.
 *
 *  The algorithm breaks up the memory to be tested into 3 partitions.  Partion
 *  0 consists of memory locations 0, 3, 6, ... partition 1 consists of
 *  memory locations 1,4,7,...  partition 2 consists oflocations 2,5,8...
 *  The partitions are filled with either an all ones pattern or an all
 *  zeroes pattern.  By varying the order in which the partitions are filled
 *  and then checked, this algorithm manages to check all combinations
 *  of possible stuck at faults.  If you don't believe me, you can find
 *  a rigorous mathematical proof (set notation and every thing) in
 *  a correspondence called "An Optimal Algorithm for Testing Stuck-at Faults
 *  in Random Access Memories" in the November 1977 issue of IEEE Transactions
 *  on Computing, volume C-26 #11 page 1141.
 *  Inputs:  
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *	tstparms = Address of test parameter struct, see struct definition
 */

/*  Log of Changes from original frias version  */
/*  Changed the includes to contain sbd.h	*/
/*  Blank defined ONERRCHK LOOPCHK RESETFLAGS	*/
/*  Substituted TRUE for PASS FALSE for FAIL	*/ 
/*  Removed tstparms-> calls                    */
/*  Remove mem_err variable condition           */
/*  Removed ONERRCKH LOOPCHK calls              */
/*  Changed printf statements to msg_printf     */

#define   WORD_ADDR_MASK          0xfffffffc
void khmemerr(__psunsigned_t, unsigned long long, unsigned long long);

int
Kh_l(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register uint *first_addr, *last_addr;
	register uint last_value_read;
	register uint local_expected_data;
	register uint *ptr;
	register int error;

	msg_printf( DBG, "Kh_l( first %x, last %x, error func %x)\n",
		first_address, last_address);
	/* make sure the addresses are on word boundary
	*/
	if( first_address & 3) {
		first_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Starting address word aligned, 0x%x\n", first_address);
	}
	last_address -= 0x400;
	if( last_address & 3) {
		last_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n", last_address);
	}

	/* Debug message */
	msg_printf(VRB,"Kh_l( %x, %x)\n", first_address, last_address);
	error = 0;
	first_addr = (uint *)first_address;
	last_addr = (uint *)last_address;
	msg_printf(DBG, "first_addr %x, last_addr %x\n", first_addr, last_addr);

	/*
	 * Set partitions 1 and 2 to 0's.
	 */
	msg_printf(VRB,"...P1 P2 ==> 0\n");
	local_expected_data = 0;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=2) {
		*ptr++ = local_expected_data;
		if (ptr <= last_addr) {
			*ptr = local_expected_data;
		}
	}

	/*
	 * Set partition 0 to ones.
	 */
	msg_printf(VRB,"...P0 ==> 1\n");
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P1=0\n");

	/*
	 * Verify partition 1 is still 0's.
	 */
	local_expected_data = 0;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
	}

	msg_printf(VRB,"...P1==>1\n");

	/*
	 * Set partition 1 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P2=0\n");


	/*
	 * Verify that partition 2 is zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
	}

	msg_printf(VRB,"...check P0 P1 = 1\n");
	/*
	 * Verify that partitions 0 and 1 are still ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr+=2) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
		if( ++ptr <= last_addr && (last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
	}

	msg_printf(VRB,"...P0==>0\n");
	/*
	 * Set partition 0 to zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P0=0\n");
	/*
	 * Check partition 0 for zeroes.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
	}

	msg_printf(VRB,"...P2==>1\n");
	/*
	 * Set partition 2 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}


	msg_printf(VRB,"...check P2 = 1\n");
	/*
	 * Check partition 2 for onees.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;

			khmemerr((__psunsigned_t)ptr,(long long)local_expected_data, (long long)last_value_read);
		}
	}

	msg_printf(VRB,"...done\n");
	if (error == 0) { 
		return(PASS);
	} else {
	return(FAIL);
	}
}

void
khmemerr(__psunsigned_t badaddr, unsigned long long expected, unsigned long long actual)
{
	int slot;

	decode_address(K1_TO_PHYS(badaddr), actual ^ expected, SW_WORD, &slot);
	err_msg(MEM_KH_ERR, &slot, badaddr, expected, actual);
	if (chk_memereg(mem_slots)) {
	    check_memereg(mem_slots);
	    mem_err_clear(mem_slots);
	}
/*	date_stamp();	*/
}
#define   DBLWD_ADDR_MASK          0xfffffff8

int
Kh_dbl(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register long long *first_addr, *last_addr;
	register long long last_value_read;
	register long long local_expected_data;
	register long long *ptr;
	register int error;

	/* make sure the addresses are on word boundary
	*/
	if( first_address & 7) {
		first_address &= DBLWD_ADDR_MASK;
		msg_printf(VRB," Starting address word aligned, 0x%x\n", first_address);
	}
	last_address -= 0x800;
	if( last_address & 7) {
		last_address &= DBLWD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n", last_address);
	}

	/* Debug message */
	msg_printf(DBG,"Kh_dbl(%x, %x)\n", first_address, last_address);
	error = 0;
	first_addr = (long long *)first_address;
	last_addr = (long long *)last_address;
	msg_printf(DBG, "first_addr %x, last_addr %x\n", first_addr, last_addr);

	/*
	 * Set partitions 1 and 2 to 0's.
	 */
	msg_printf(VRB,"...P1 P2 ==> 0\n");
	local_expected_data = 0;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=2) {
		*ptr++ = local_expected_data;
		if (ptr <= last_addr) {
			*ptr = local_expected_data;
		}
	}

	/*
	 * Set partition 0 to ones.
	 */
	msg_printf(VRB,"...P0 ==> 1\n");
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P1=0\n");

	/*
	 * Verify partition 1 is still 0's.
	 */
	local_expected_data = 0;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
	}

	msg_printf(VRB,"...P1==>1\n");

	/*
	 * Set partition 1 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+1; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P2=0\n");


	/*
	 * Verify that partition 2 is zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
	}

	msg_printf(VRB,"...check P0 P1 = 1\n");
	/*
	 * Verify that partitions 0 and 1 are still ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr+=2) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
		if( ++ptr <= last_addr && (last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
	}

	msg_printf(VRB,"...P0==>0\n");
	/*
	 * Set partition 0 to zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}

	msg_printf(VRB,"...check P0=0\n");
	/*
	 * Check partition 0 for zeroes.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
	}

	msg_printf(VRB,"...P2==>1\n");
	/*
	 * Set partition 2 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		*ptr = local_expected_data;
	}


	msg_printf(VRB,"...check P2 = 1\n");
	/*
	 * Check partition 2 for onees.
	 */
	local_expected_data = -1;
	for (ptr = first_addr+2; ptr <= last_addr; ptr+=3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;

			khmemerr((__psunsigned_t)ptr,local_expected_data, last_value_read);
		}
	}

	msg_printf(VRB,"...done\n");
	if (error == 0) { 
		return(PASS);
	} else {
	return(FAIL);
	}
}

/*
 *		t h r e e b i t ()
 *
 * The three bit test checks memory for pattern sensitive faults.
 *
 *
 *  Inputs:  
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *	tstparms = Address of test parameter struct, see struct definition
 */



/*
 * Define data pattern. Note, every three data patterns
 * constitues a new pass of the test.
 */
static	u_int tbl[] = {
		0xb6db6db6, 0xdb6db6db, 0x6db6db6d,
		0x24924924, 0x92492492, 0x49249249,
		0x6db6db6d, 0xb6db6db6, 0xdb6db6db,
		0x49249249, 0x24924924, 0x92492492,
		0xbd6bd6bd, 0x6bd6bd6b, 0xd6bd6bd6,
		0x92492492, 0x49249249, 0x24924924,
		};

static u_int fail;
u_int
three_l(__psunsigned_t first, __psunsigned_t last)
{
	u_int i;
	u_int j;

	j = fail = 0;
	last = last - 0x10;
	for(i=0; i < 18; i += 3) {
		j++;
		msg_printf( VRB, "starting test pass %x\n", j);
		dwd_3bit(first, last, tbl[i], tbl[i+1], tbl[i+2], 0, 0);
	}
	return fail;
}

static int
dwd_3bit(__psunsigned_t begadr, __psunsigned_t endadr, uint pat1, uint pat2, uint pat3, uint mask, uint errlimit)
{
	u_int	r_code;
	u_int	rtn_errors= 0;
	u_int	max_errors;

	rtn_errors = 0;
	max_errors = errlimit;

	/* Ensure word aligned */
	begadr &= (__psunsigned_t) ~(sizeof(long) -1);
	endadr &= (__psunsigned_t) ~(sizeof(long) -1);

	/* Do a sanity check */
	if (endadr <= begadr)
		return(rtn_errors);

	/*if (setjmp(buffer) )
		printf("Hello\n");
	*/	

	r_code = threebit(pat1, pat2, pat3, begadr, endadr, mask);
	return 0;
}


/* The actual tests. The variables are declared this way so  the compiler
 * gives us machine registers.
 */
static int threebit(u_int pat1, u_int pat2, u_int pat3, __psunsigned_t wrkp, __psunsigned_t endptr, u_int mask)
{
	register u_int pattern1= pat1;
	register u_int pattern2= pat2;
	register u_int pattern3= pat3;
	register __psunsigned_t wrkptr= wrkp;
	register u_int received;
	register __psunsigned_t nearlydone = endptr -14;
	register u_int received2;
	register u_int received3;
	__psunsigned_t begptr ;
	u_int pattern4 = pattern1;

	begptr = wrkptr;

	do {
		wrkptr= begptr;
		msg_printf( DBG,"  3-Bit test running  ");
		msg_printf( DBG,"wrkptr = %x\n", wrkptr);

		/* Fill most of memory up */
		wrkptr = WriteLoop14(wrkptr, nearlydone, pattern1, pattern2, pattern3);

		/* Finish up the last locations */
		while (1) {
			if (wrkptr > endptr)
				break;
			*(uint *)wrkptr++ = pattern1;			
			if (wrkptr > endptr)
				break;
			*(uint *)wrkptr++ = pattern2;			
			if (wrkptr > endptr)
				break;
			*(uint *)wrkptr++ = pattern3;			
		}


		/* Now start the read back */
		msg_printf(VRB, "    starting read back \n");

		wrkptr = begptr;
	
		while (wrkptr < nearlydone ) {
			received = *(uint *)wrkptr++;		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *(uint *)wrkptr++;		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *(uint *)wrkptr++;		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;

			received = *(uint *)wrkptr++;		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *(uint *)wrkptr++;		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *(uint *)wrkptr++;		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;
		}

		msg_printf( DBG,"    begin slow reads %x\n", wrkptr);
		/* 
		 * Finish up the last reads here
		 */
		while (1) {
			if (wrkptr > endptr)
				break;
			received = *(uint *)wrkptr++;		/* READ */
			if (received != pattern1)
				goto bit3_err1;

bit3_ret1:
			if (wrkptr > endptr)
				break;
			received = *(uint *)wrkptr++;		/* READ */
			if (received != pattern2)
				goto bit3_err2;
bit3_ret2:
			if (wrkptr > endptr)
				break;
			received = *(uint *)wrkptr++;		/* READ */
			if (received != pattern3)
				goto bit3_err3;
bit3_ret3:
			continue;
		}
		/* Rotate patterns */
		received = pattern1 ^ mask;
		pattern1 = pattern2 ^ mask;
		pattern2 = pattern3 ^ mask;
		pattern3 = received ^ mask;
	} while (pattern1 != pattern4);

	return(0);

/***************************************/


bit3_err1:
	if ((received ) != (pattern1 ) ) {
		msg_printf(ERR, "Test Error 1\n");
		msg_printf(ERR, "Failing address %x\n", wrkptr-1);
		msg_printf(ERR, "expected %x, actual %x, Xor %x\n", 
			pattern1, received, (pattern1 ^ received) );
		if (check_memereg(mem_slots)) mem_err_clear(mem_slots);
		fail ++;
	}
	goto bit3_ret1;

bit3_err2:
	if ((received ) != (pattern2 ) )  {
		msg_printf( ERR, "Test Error 2\n");
		msg_printf( ERR, "Failing address %x\n", wrkptr-1);
		msg_printf( ERR, "expected %x, actual %x, Xor %x\n", 
			pattern2, received, (pattern2 ^ received) );
		if (check_memereg(mem_slots)) mem_err_clear(mem_slots);
		fail ++;
	}
	goto bit3_ret2;

bit3_err3:
	if ((received ) != (pattern3 ) )  {
		msg_printf( ERR, "Test Error 3\n");
		msg_printf( ERR, "Failing address %x\n", wrkptr-1);
		msg_printf( ERR, "expected %x, actual %x, Xor %x\n", 
			pattern3, received, (pattern3 ^ received) );
		if (check_memereg(mem_slots)) mem_err_clear(mem_slots);
		fail ++;
	}
	goto bit3_ret3;
			
}


