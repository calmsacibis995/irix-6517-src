/*
 * memutil.c -
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * Utilities for memory tests:
 *	- memtestcommon_usage
 *	- mem_setup
 *	- getbadsimm
 *	- fillmemW
 *	- blkrd
 *	- blkwrt
 *	- *err_string
 *	- memerr
 *
 * NOTE: for IP30, the following is left to do  XXX
 *		- define the memory map for IP30 , and get rid of all 
 *						non-IP30 simm maps XXX
 *		- remove one of lowrange/hirange (mem_setup),
 *			which has implications on several other files.
 */
#ident "ide/godzilla/mem/memutil.c:  $Revision: 1.13 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <fault.h>
#include <libsc.h>
#include <libsk.h>
#include "d_mem.h"


#define WRD0    0
#define WRD1    0x4
#define WRD2    0x8
#define WRD3    0xc

static void
memtestcommon_usage(char *progname)
{
	msg_printf(ERR, "usage: %s [beginaddr:endaddr|beginaddr#size]\n",
		   progname);
}


/*
 * Name:	mem_setup
 * Description: derives the lowrange and hirange struct from cpu_get_memsize()
 *		and machine architecture.
 * Output:	lowrange struct
 * Error Handling: returns -1 if error, 0 if not
 * Remarks:	uses two ranges because Ip22/26 used to have a segmented memory
 *		for IP30, I think we could just use one
 * calling sequence:	from shift_dram, memtest, dram2_diagnostics, 
 *		dram_diagnostics, blk_test 
 */
int
mem_setup(int setupargc, char *testname, char *rangestring,
	  struct range *lowrange)
{
	unsigned int count;

	msg_printf(DBG,"mem_setup: %s %s\n", testname, rangestring);
  
	/* IP30: PROM_STACK PHYS_TO_K1_RAM(0x1000000)
	 *  	 and PHYS_TO_K1_RAM = PHYS_TO_K1((x)+SYSTEM_MEMORY_BASE
	 *  	 where SYSTEM_MEMORY_BASE = 0x20000000
	 * 	 so PROM_STACK is in K1 space
	 */
	lowrange->ra_base = PROM_STACK;
  
	/* in IP30, PHYS_RAMBASE = SYSTEM_MEMORY_BASE */
	/*	    count does not need to be uint64 */
	msg_printf(DBG,"mem_setup: memsize = %x\n",cpu_get_memsize());
	count = (unsigned int)(cpu_get_memsize() -
		 (KDM_TO_PHYS(lowrange->ra_base) - PHYS_RAMBASE));
	/* IP30: count ~ cpu_get_memsize() - PROM_STACK */ 
	lowrange->ra_size = sizeof(u_int);
	lowrange->ra_count = count / lowrange->ra_size;

	msg_printf(DBG,"Default: base=%x size=%x count=%x\n",
		   lowrange->ra_base, lowrange->ra_size,
		   lowrange->ra_count);

	if (setupargc > 2) {
		msg_printf(VRB,"Incorrect argument list\n");
		memtestcommon_usage(testname);
		return -1;
	}

	/* if a range was given when calling memtest: */
	/* parse_range parses the string and returns the result in lowrange */
	if (setupargc == 2)
		parse_range(rangestring, lowrange->ra_size, lowrange);

	/* print the resulting lowrange */
	msg_printf(DBG,"Updated: base=%x count=%x size=%x\n",
		   lowrange->ra_base, lowrange->ra_count,
		   lowrange->ra_size);

	return(0);
}

/* BX chips on IP30: 
 *		MEM_DATA0-17 <-> BX1
 *		MEM_DATA18-31 <-> BX2
 *		MEM_DATA32-45 <-> BX3
 *		MEM_DATA46-63 <-> BX4
 */
static int
bad_bx(__psunsigned_t bad_bits)
{
	/* analyze the bad bits XXX the first bad BX is returned */
	if (bad_bits & BX1) return (1);
	else if (bad_bits & BX2) return (2);
	else if (bad_bits & BX3) return (3);
	else if (bad_bits & BX4) return (4);
	else return (0);
}

/* Finds the bad sim based on the failing address and data bits
 *	also add information on what BX may be wrong
 */
void
getbadsimm(caddr_t fail_addr, __psunsigned_t bad_bits)
{
	__psunsigned_t	badbnk;

	/* compute the bad bank and the physical address */
        badbnk = ip30_addr_to_bank( (__uint32_t)KDM_TO_PHYS(fail_addr) );
	fail_addr = (caddr_t) KDM_TO_PHYS(fail_addr);

	msg_printf(DBG,"Physical addr: %x, Bank: %x, Bad bits: 0x%x\n", 
		fail_addr, badbnk, bad_bits);

	/* use printf instead of msg_printf so that the msg is never masked */
	printf("** Check SIMM S%d ",
		badbnk * 2 + (((ulong)fail_addr & 0x8) >> 3) + 1);
	if(bad_bx(bad_bits)) printf("or mux BX%d\n", bad_bx(bad_bits));
	else printf("\n");
}


/*
 *			f i l l m e m W _ l( )
 *
 * fillmemW- fills memory from the starting address to ending address with the
 * data pattern supplied. Or if the user chooses, writes are done from ending
 * address to the starting address. Writes are done in multiples of 8 to reduce
 * the overhead. The disassembled code takes 12 cycles for the 8 writes versus
 * 32 cycles if done 1 word at a time.
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	pat1   = pattern 1
 *	direction = 0= write first to last, 1= write last to first
 */

#define REVERSE 1
#define FORWARD 0

void
fillmemW(__psunsigned_t *begadr, __psunsigned_t *endadr, __psunsigned_t pat1,
	 __psunsigned_t direct)
{
	/* This code is run in cached mode so a switch statement is not
	 * used because it will pop the code into uncached space since the
	 * code was linked in K1 space.
	 */
	if (direct == 0) {
		/*
	 	 * Do the writes in multiples of 8 for speed
	 	 */

		msg_printf(DBG,"%x %x %x .. %x\n", begadr, (begadr + 1), (begadr + 2), (begadr + 7));
		while (begadr <= (endadr -  8)) {
			*begadr = pat1;
			*(begadr + 1) = pat1;
			*(begadr + 2 ) = pat1;
			*(begadr + 3 ) = pat1;
			*(begadr + 4 ) = pat1;
			*(begadr + 5 ) = pat1;
			*(begadr + 6 ) = pat1;
			*(begadr + 7 ) = pat1;
			begadr += 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;begadr <= endadr; ) {
			*begadr++ = pat1;
		}
	}
	else {
		/*
	 	 * Do the writes in multiples of 8 for speed, from last-> first 
	 	 */
		for( ;endadr >= ( begadr -  8 * sizeof(__psunsigned_t) ); ) {
			*endadr = pat1;
			*(endadr - 1 ) = pat1;
			*(endadr - 2 ) = pat1;
			*(endadr - 3 ) = pat1;
			*(endadr - 4 ) = pat1;
			*(endadr - 5 ) = pat1;
			*(endadr - 6 ) = pat1;
			*(endadr - 7 ) = pat1;
			endadr -= 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;endadr >= begadr; ) {
			*endadr-- = pat1;
		}
	}
}


/*
 *			b l k r d ( )
 *
 * blkrd - reads memory in block mode,i.e the reads are in K0 space. The cache 
 * block size is passed into this routine so that the appropriate
 * if statement can be executed. The reads are done in groups of 128 bytes.
 * Left over reads are done in a slower loop. The below C code causes sequencial
 * reads (lw reg, offset(reg) assembly code).
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	blksize= cache block size
 */

void
blkrd(u_int *begadr, u_int *endadr, u_int blksize)
{
	volatile u_int tmpbuf;

	/*
	 * A switch statement isn't used because the code is linked in
	 * k1 space. This code is run in k0 space (cached) a switch statement
	 * will pop us into k1 space, thus slowing the code down.
	 */
	msg_printf(VRB,"blkrd(0x%x,0x%x)\n",begadr,endadr);
	if( blksize == 16) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 100);) {
			tmpbuf = *begadr;
			tmpbuf = *(begadr + 8);
			tmpbuf = *(begadr + 12);
			tmpbuf = *(begadr + 16);
			tmpbuf = *(begadr + 20);
			tmpbuf = *(begadr + 24);
			tmpbuf = *(begadr + 28);
			tmpbuf = *(begadr + 32);
			tmpbuf = *(begadr + 36);
			tmpbuf = *(begadr + 40);
			tmpbuf = *(begadr + 44);
			tmpbuf = *(begadr + 48);
			tmpbuf = *(begadr + 52);
			tmpbuf = *(begadr + 56);
			tmpbuf = *(begadr + 60);
			tmpbuf = *(begadr + 64);
			tmpbuf = *(begadr + 68);
			tmpbuf = *(begadr + 72);
			tmpbuf = *(begadr + 76);
			tmpbuf = *(begadr + 80);
			tmpbuf = *(begadr + 84);
			tmpbuf = *(begadr + 88);
			tmpbuf = *(begadr + 92);
			tmpbuf = *(begadr + 96);
			begadr += 100;
		}  /* end for */
		/*
                 * Finish up remaining reads
                 */
		for ( ;begadr < endadr; ) {
			tmpbuf = *begadr++;
		}

	}
	else {
		if( blksize == 32) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  128);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 8);
				tmpbuf = *(begadr + 16);
				tmpbuf = *(begadr + 24);
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 40);
				tmpbuf = *(begadr + 48);
				tmpbuf = *(begadr + 56);
				tmpbuf = *(begadr + 64);
				tmpbuf = *(begadr + 72);
				tmpbuf = *(begadr + 80);
				tmpbuf = *(begadr + 88);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 104);
				tmpbuf = *(begadr + 112);
				tmpbuf = *(begadr + 120);
				begadr += 128;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}

		}
		else if( blksize == 64) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  160);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 16);
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 48);
				tmpbuf = *(begadr + 80);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 112);
				tmpbuf = *(begadr + 128);
				tmpbuf = *(begadr + 144);
				begadr += 160;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}
		}
		else if( blksize == 128) {
			/* Do the reads 
	 	 	 */
			for( ;begadr <= (endadr -  160);) {
				tmpbuf = *begadr;
				tmpbuf = *(begadr + 32);
				tmpbuf = *(begadr + 64);
				tmpbuf = *(begadr + 96);
				tmpbuf = *(begadr + 128);
				begadr += 160;
			}
			/* Finish up remaining reads
	 	 	 */
			for ( ;begadr < endadr; ) {
				tmpbuf = *begadr++;
			}
		}
		else {
			printf("Unknown blocksize\n");
		}
	}
}


/*
 *  Block writes in the same fashion as blkrd(), with the
 * additional parameter of the pattern to be written
 *
 * inputs:
 *      begadr = starting address
 *      endadr = ending address
 *      blksize= cache block size
 *      pattern = specified pattern
 */

void
blkwrt(register u_int *begadr, register u_int *endadr, register u_int blksize, 
       register u_int pattern)
{
	msg_printf(VRB,"blkwrt(0x%x,0x%x)\n",begadr,endadr);
	if (blksize == 16) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 100);) {
			*begadr = pattern;
			*(begadr + 8) = pattern;
			*(begadr + 12) = pattern;
			*(begadr + 16) = pattern;
			*(begadr + 20) = pattern;
			*(begadr + 24) = pattern;
			*(begadr + 28) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 36) = pattern;
			*(begadr + 40) = pattern;
			*(begadr + 44) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 52) = pattern;
			*(begadr + 56) = pattern;
			*(begadr + 60) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 68) = pattern;
			*(begadr + 72) = pattern;
			*(begadr + 76) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 84) = pattern;
			*(begadr + 88) = pattern;
			*(begadr + 92) = pattern;
			*(begadr + 96) = pattern;
			begadr += 100;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}

	}  /* end if */
	else if (blksize == 32) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr - 124);) {
			*begadr = pattern;
			*(begadr + 8) = pattern;
			*(begadr + 16) = pattern;
			*(begadr + 24) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 40) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 56) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 72) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 88) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 104) = pattern;
			*(begadr + 112) = pattern;
			*(begadr + 120) = pattern;
			begadr += 124;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}

	}  /* end if */

	else if (blksize == 64) {
		/*
                 * Do the writes
                 */
		for( ;begadr < (endadr -  144);) {
			*begadr = pattern;

			*(begadr + 16) = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 48) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 80) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 112) = pattern;
			*(begadr + 128) = pattern;
			begadr += 144;
		} /* end for  */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		}  /* end for */
	}  /* end if */
	else if (blksize == 128) {
		/*
                 * Do the writes
                 */
		for( ;begadr <= (endadr -  160);) {

			*begadr = pattern;
			*(begadr + 32) = pattern;
			*(begadr + 64) = pattern;
			*(begadr + 96) = pattern;
			*(begadr + 128) = pattern;
			begadr += 160;
		}  /* end for */
		/*
                 * Finish up remaining writes
                 */
		for ( ;begadr < endadr; ) {
			*begadr++ = pattern;
		} /* end for */
	}  /* end if */
	else {
		printf("Unknown blocksize\n");
	}

}

static char *err_string[] = {
	"Address: 0x%x, Expected: 0x%02x, Actual: 0x%02x, XOR: 0x%02x\n",
	"Address: 0x%x, Expected: 0x%04x, Actual: 0x%04x, XOR: 0x%04x\n",
	"Address: 0x%x, Expected: 0x%08x, Actual: 0x%08x, XOR: 0x%08x\n",
	"Address: 0x%x, Expected: 0x%016x, Actual: 0x%016x, XOR: 0x%016x\n",
};

void
memerr(caddr_t badaddr, __psunsigned_t expected, __psunsigned_t actual, int size)
{
	msg_printf(ERR, err_string[ size>=8 ? 3: size>>1],
	    badaddr, expected, actual, expected ^ actual);
	getbadsimm( badaddr, (expected ^ actual) );
}
