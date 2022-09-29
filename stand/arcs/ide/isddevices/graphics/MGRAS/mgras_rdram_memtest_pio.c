/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sgidefs.h>
#include <libsc.h>
#include <libsk.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"


/**************************************************************************
 *                                                                        *
 *  mgras_RDRAM_unique							  *
 *                                                                        *
 *	Makes sure that the system can talk to each RDRAM uniquely        *
 *                                                                        *
 * Write to the 2nd octbyte on page 0 of each RDRAM as follows and        *
 * verify the writes.							  *	
 *                                                                        *
 *	RDRAM0-PP0 = 0x10 << RSS# (=10, 20,  40,  80)			  *
 *	RDRAM1-PP0 = 0x30 << RSS# (=30, 60,  d0, 180)			  *
 *	RDRAM2-PP0 = 0x70 << RSS# (=70, e0, 1d0, 380)			  *
 *	RDRAM0-PP1 = 0x1f << RSS# (=1f, 3e,  7d,  f8)			  *
 *    	RDRAM1-PP1 = 0x3f << RSS# (=3f, 7e,  fd, 1f8)                     *
 *	RDRAM2-PP1 = 0x7f << RSS# (=7f, fe, 1fc, 3f8)			  *
 *                                                                        *
 **************************************************************************/

mgras_RDRAM_unique()
{
	__uint32_t errors = 0, actual, status, rssnum, ppnum, rd, expected;
	__uint32_t base, i, mask;
	ppfillmodeu  ppfillMode = {0};

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
		msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
			status & RE4COUNT_BITS);
		REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_UNIQUE_TEST]), errors);
        }
#endif
	mask = EIGHTEENBIT_MASK;

	/* Write all rss, pp0/1, rd0/1/2 */
	for (rssnum = 0; rssnum < numRE4s; rssnum++) {
	  HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum, 
		EIGHTEENBIT_MASK);

	  /* set the PP fillmode to use 12-12-12 */
           ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
           ppfillMode.bits.BufSize  = 1;
           ppfillMode.bits.DrawBuffer  = 1;
           WRITE_RSS_REG(rssnum, (PP_FILLMODE& 0x1ff), ppfillMode.val, ~0x0);

	   for (ppnum = 0; ppnum < PP_PER_RSS; ppnum++) {
		for (rd = 0; rd < RDRAM_PER_PP; rd++) {
			base = 1;
			for (i = 1; i <= (rd+1); i++)
				base *= 2;
			base--; /* get 1, 3, or 7 */
			expected = (((base << 4) | (0xf * ppnum)) << rssnum);
	   		WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) |
				((RDRAM0_MEM+rd) << 24) |
				 (0x0), expected, mask);
#if 0
			fprintf(stdout, "wrote: 0x%x\n", expected);
#endif
		}
	   }
	}		

	/* Read all rss, pp0/1, rd0/1/2 */
	for (rssnum = 0; rssnum < numRE4s; rssnum++) {
	  HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG), rssnum, 				TWELVEBIT_MASK);

	   for (ppnum = 0; ppnum < PP_PER_RSS; ppnum++) {
		for (rd = 0; rd < RDRAM_PER_PP; rd++) {
			base = 1;
			for (i = 1; i <= (rd+1); i++)
				base *= 2;
			base--; /* get 1, 3, or 7 */
			expected = (((base << 4) | (0xf * ppnum)) << rssnum);
			READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | 
				((RDRAM0_MEM+rd)<<24) | (0x0), mask,
				expected);
#if 0
				fprintf(stdout, "read: 0x%x, wrote: 0x%x\n", 
					actual, expected);
#endif
			if (actual != expected) {
			   msg_printf(ERR, 
			   "RSS-%d, PP-%d, rdram-%d, wrote: 0x%x, read: 0x%x\n",
				rssnum, ppnum, rd, expected, actual);
		   	   errors++;
	   		   REPORT_PASS_OR_FAIL_NR((&RDRAM_err[RDRAM_UNIQUE_TEST
			   + 	rssnum*6*LAST_RDRAM_ERRCODE + 
				ppnum*3*LAST_RDRAM_ERRCODE +
				(rd % RDRAM0_MEM)*LAST_RDRAM_ERRCODE]), errors);
			}
		}
	   }
	}

	if (errors == 0) {
                msg_printf(VRB, "RDRAM Unique Test: PASSED\n");
                return (0);
        }
        else {
                msg_printf(VRB, "RDRAM Unique Test: FAILED\n");
                return (1);
        }

}

/**************************************************************************
 *
 * ram_walkingbit
 *
 *			Walkingbit test 1/0 (algorithm from mti code)
 *
 *	Checks the data path(word) and basic control using one location.
 *	This tests using a walking ones and zeros pattern. Since we write
 *	in 18-bit chunks, this test will make 36 passes through memory.
 *
 *
 **************************************************************************/

int
ram_walkingbit( __uint32_t rssnum, __uint32_t ppnum, __uint32_t start_addr, 
	__uint32_t end_addr, __uint32_t allrdram, __uint32_t inc,
	__uint32_t stop_on_err, __uint32_t broadcast, __uint32_t mask,
	__uint32_t numbits)
{
	__uint32_t  j=0, errors = 0, i, addr, actual, cfifo_count, cfifo_status;
	__uint32_t ctr;

	msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x Walking Bit test\n",
		rssnum, ppnum, start_addr, end_addr);

	/* Try walking one */
	for (i = 0; i < numbits; i++) {
	   msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x walking 0x%x\n",
		rssnum, ppnum, start_addr, end_addr, walk_1_0_32_rss[i]);
	   cfifo_count = 0; ctr = 0;
	   if (allrdram == 0) { /* test all rdram on this pp */
	      	for (addr = ((start_addr & 0xffffff) | broadcast); 
			addr <= ((end_addr & 0xffffff) | broadcast); 
			addr += inc) 
		{
		   	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28)|addr,
				walk_1_0_32_rss[i], mask);
			cfifo_count++; ctr++;
			if ((cfifo_count & 0xff) == 64) {
			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			   if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                          	 return (-1);
                           }
                	}
			ALIVE_BAR(ctr, WALKTIME);
	     	}
	   }
	   else {
	      for (addr = start_addr; addr <= end_addr; addr += inc) {
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         walk_1_0_32_rss[i], mask);
		cfifo_count++; ctr++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
		ALIVE_BAR(ctr, WALKTIME);
	      }
	   }
	   
	   cfifo_count = 0; ctr = 0;
	   for (addr = start_addr; addr <= end_addr; addr += inc)
	   {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, walk_1_0_32_rss[i]);
		RE_IND_COMPARE("Walking 1/0", (ppnum << 28) | addr, walk_1_0_32_rss[i], 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(DBG, "RAM Walking Bit Test: FAILED\n");
			return (errors);
		}
		cfifo_count++; ctr++;
		ALIVE_BAR(ctr, WALKTIME);
	   }
	}

	/* Try walking zero */
	for (i = 0; i < numbits; i++) {
	   msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x walking 0x%x\n",
		rssnum, ppnum, start_addr, end_addr, walk_1_0_32_rss[i]);
	   cfifo_count = 0; ctr = 0;
	   if (allrdram == 0) { /* test all rdram on this pp */
	      	for (addr = ((start_addr & 0xffffff) | broadcast); 
			addr <= ((end_addr & 0xffffff) | broadcast); 
			addr += inc) 
		{
		   	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28)|addr,
				~walk_1_0_32_rss[i], mask);
			cfifo_count++; ctr++;
			if ((cfifo_count & 0xff) == 64) {
			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			   if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                          	 return (-1);
                           }
                	}
			ALIVE_BAR(ctr, WALKTIME);
	     	}
	   }
	   else
	   for (addr = start_addr; addr <= end_addr; addr+=inc) {
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         ~walk_1_0_32_rss[i], mask);
		cfifo_count++; ctr++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
		ALIVE_BAR(ctr, WALKTIME);
	   }

	   cfifo_count = 0; ctr = 0;
	   for (addr = start_addr; addr <= end_addr; addr+=inc)
	   {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~walk_1_0_32_rss[i]);
		RE_IND_COMPARE("Walking 1/0", (ppnum << 28) | addr, ~walk_1_0_32_rss[i], 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(VRB, "RAM Walking Bit Test: FAILED\n");
			return (errors);
		}	
		cfifo_count++; ctr++;
		ALIVE_BAR(ctr, WALKTIME);
	   }
	}

	if (errors) {
		msg_printf(VRB, "RAM Walking 1/0 Test: FAILED\n");
	}
	else
		msg_printf(VRB, "RAM Walking 1/0 Test: PASSED\n");

	return (errors);
}

/**************************************************************************
 *
 * ram_addru
 *
 *			Address in Address test
 *
 *	Writes addr[i] = i and addr[i] = ~i, for a total of 2 passes through
 * 	memory.
 *
 *
 **************************************************************************/

int
ram_addru( __uint32_t rssnum, __uint32_t ppnum, __uint32_t start_addr, 
	__uint32_t end_addr, __uint32_t allrdram, __uint32_t inc,
	__uint32_t stop_on_err, __uint32_t broadcast, __uint32_t mask,
	__uint32_t numbits)
{
	__uint32_t  errors = 0, addr, actual, cfifo_count, cfifo_status;

	/* addr[i] = i */
	msg_printf(DBG,
	   "RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x Address Unique\n",
		rssnum, ppnum, start_addr, end_addr);

	cfifo_count = 0;
	for (addr = start_addr; addr <= end_addr; addr += inc) {
        	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, addr, 
			mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
		DELAY(10);
	}
	   
	for (addr = start_addr; addr <= end_addr; addr += inc)
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, addr);
		RE_IND_COMPARE("Address Unique", (ppnum << 28) | addr, addr, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(VRB, "RAM Address Unique Test: FAILED\n");
			return (errors);
		}	
	}

	/* addr[i] = ~i */
	msg_printf(DBG, 
	   "RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x Inverse Address Unique\n",
		rssnum, ppnum, start_addr, end_addr);

	cfifo_count = 0;
	for (addr = start_addr; addr <= end_addr; addr += inc) {
        	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, ~addr, 
			mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}
	   
	for (addr = start_addr; addr <= end_addr; addr += inc)
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~addr);
		RE_IND_COMPARE("Address Unique", (ppnum << 28) | addr, ~addr, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(VRB, "RAM Address Unique Test: FAILED\n");
			return (errors);
		}	
	}
	if (errors) {
		msg_printf(VRB, "RAM Address Unique Test: FAILED\n");
	}
	else
		msg_printf(VRB, "RAM Address Unique Test: PASSED\n");

	return (errors);
}

/**************************************************************************
 *
 * ram_pattern
 *
 *	Write a set of patterns to memory and then cycle the patterns through
 *	memory. 		
 *
 *	e.g. For a set of 3 patterns, pat[0:2],
 *	     Pass 1:
 *	 	addr[i] = pat[0];
 *	   	addr[i + 1] = pat[1];
 *		addr[i + 2] = pat[2];
 *		addr[i + 3] = pat[0];
 *		addr[i + 4] = pat[1];
 *		etc...
 *	     Pass 2:
 *	 	addr[i] = pat[1];
 *	   	addr[i + 1] = pat[2];
 *		addr[i + 2] = pat[0];
 *	     Pass 3:
 *	 	addr[i] = pat[2];
 *	   	addr[i + 1] = pat[0];
 *		addr[i + 2] = pat[1];
 * 
 *	The test makes n passes through memory, where n is the number of
 * 	patterns.
 *
 **************************************************************************/

int
ram_pattern( __uint32_t rssnum, __uint32_t ppnum, __uint32_t start_addr, 
	__uint32_t end_addr, __uint32_t allrdram, __uint32_t inc,
	__uint32_t stop_on_err, __uint32_t broadcast, __uint32_t mask,
	__uint32_t numbits)
{
	__uint32_t  cfifo_status, cfifo_count, errors = 0, i, j, addr, actual, pat;

	/* do sizeof(patrn)/2 passes through memory since patrn is doubled */
	for (i = 0; i < (sizeof(patrn)/sizeof(patrn[0]))/2; i++) {

	    msg_printf(DBG,
		"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x Pattern Pass %d\n",
		rssnum, ppnum, start_addr, end_addr, i);

	   cfifo_count = 0;
	   /* write 1 pass through memory */
	   for (addr=start_addr, j=0; addr< end_addr; addr+= inc,j++)
	   {
	      	pat = ((j % (sizeof(patrn)/sizeof(patrn[0]))) + i) % 
			((sizeof(patrn)/sizeof(patrn[0]))); /* which pat */
              	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         patrn[pat], mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	   }
	   
	   /* read 1 pass through memory */
	   for (addr=start_addr, j=0; addr< end_addr; addr+= inc, j++)
	   {
	      	pat = ((j % (sizeof(patrn)/sizeof(patrn[0]))) + i) % 
			((sizeof(patrn)/sizeof(patrn[0]))); /* which pat */
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, patrn[pat]);
		RE_IND_COMPARE("Pattern", (ppnum << 28) | addr, patrn[pat],
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(VRB, "RAM Pattern Test: FAILED\n");
			return (errors); 
		}
	   }
	}
	if (errors) {
		msg_printf(VRB, "RAM Pattern Test: FAILED\n");
	}
	else
		msg_printf(VRB, "RAM Pattern Test: PASSED\n");
	return (errors);
}

/**************************************************************************
 *
 * ram_kh
 *
 *  Knaizuk Hartmann Memory Test
 *
 *  This algorithm is used to perform a fast but non-exhaustive memory test.
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
 *
 *  Algorithm:
 *	P1 P2 ==> 0
 *	P0 ==> 1
 *	check P1=0
 *	P1==>1
 *	check P2=0
 *	check P0 P1 = 1
 *	P0==>0
 *	check P0=0
 *	P2==>1
 *	check P2 = 1
 *
 **************************************************************************/

int ram_kh(
	__uint32_t rssnum,
	__uint32_t ppnum,
	__uint32_t start_addr, 
	__uint32_t end_addr, 
	__uint32_t allrdram,
	__uint32_t inc,
	__uint32_t stop_on_err,
	__uint32_t broadcast,
	__uint32_t mask,
	__uint32_t numbits)
{
	__uint32_t  errors = 0, addr, actual, cfifo_count, cfifo_status;

	msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x KH Test\n",
		rssnum, ppnum, start_addr, end_addr);

	/* 
         * Set partitions 1 and 2 to 0's.
         */
	cfifo_count = 0;
	msg_printf(DBG,"...P1 P2 ==> 0\n");
	for (addr  = start_addr + inc; addr <= end_addr; 
							addr += 3*inc) 
	{
           WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         0, mask);
	   if ((addr + inc) <= end_addr) {
           	WRITE_RE_INDIRECT_REG(rssnum,(ppnum<<28)|(addr + inc),
                         0, mask);
	   }
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/*
         * Set partition 0 to ones.
         */
	cfifo_count = 0;
	msg_printf(DBG,"...P0 ==> 1\n");
	for (addr  = start_addr; addr <= end_addr; addr += 3*inc) {
        	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         ~0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

        /*
         * Verify partition 1 is still 0's.
         */
	msg_printf(DBG,"...check P1=0\n");
	for (addr  = start_addr + inc; addr <= end_addr; 
							addr += 3*inc) 
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("KH Test", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		}	
	}

	/*
         * Set partition 1 to ones.
         */
	cfifo_count = 0;
	msg_printf(DBG,"...P1==>1\n");
	for (addr  = start_addr + inc; addr <= end_addr; 
							addr += 3*inc) 
	{
           	WRITE_RE_INDIRECT_REG(rssnum,(ppnum<<28)|(addr),
                         ~0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/*
         * Verify that partition 2 is zeroes.
         */
	msg_printf(DBG,"...check P2=0\n");
	for (addr  = start_addr + 2*inc; addr <= end_addr; 
							addr += 3*inc) 
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("KH Test", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		}	
	}

	/*
         * Verify that partitions 0 and 1 are still ones.
         */
	msg_printf(DBG,"...check P0 P1 = 1\n");
	for (addr  = start_addr; addr <= end_addr; addr += 3*inc) 
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~0);
		RE_IND_COMPARE("KH Test", (ppnum << 28) | addr, ~0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		}	

		if ((addr + inc) <= end_addr ) {
		   READ_RE_INDIRECT_REG(rssnum,(ppnum<<28)|(addr+inc),
			mask, ~0);
		   RE_IND_COMPARE("KH Test",(ppnum << 28) | (addr + inc), ~0,
				actual, mask, errors);
		   if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		   }	
		}
	}

	/*
         * Set partition 0 to zeroes.
         */
	cfifo_count = 0;
	msg_printf(DBG,"...P0==>0\n");
	for (addr  = start_addr; addr <= end_addr; addr += 3*inc) {
           	WRITE_RE_INDIRECT_REG(rssnum,(ppnum<<28)|(addr),
                         0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/*
         * Check partition 0 for zeroes.
         */
	msg_printf(DBG,"...check P0=0\n");
	for (addr  = start_addr; addr <= end_addr; addr += 3*inc) 
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("KH Test", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		}	
	}
	
	/*
         * Set partition 2 to ones.
         */
	cfifo_count = 0;
	msg_printf(DBG,"...P2==>1\n");
	for (addr  = start_addr + 2*inc; addr <= end_addr; 
						addr += 3*inc) {
           	WRITE_RE_INDIRECT_REG(rssnum,(ppnum<<28)|(addr),
                         ~0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/*
         * Check partition 2 for ones.
         */
	msg_printf(DBG,"...check P2 = 1\n");	
	for (addr  = start_addr+ 2*inc; addr <= end_addr; 
						addr += 3*inc) 
	{
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~0);
		RE_IND_COMPARE("KH Test", (ppnum << 28) | addr, ~0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM KH Test: FAILED\n");
			return (errors);
		}	
	}
		
	if (errors) {
		msg_printf(ERR, "RAM KH Test: FAILED\n");
	}
	else
		msg_printf(SUM, "RAM KH Test: PASSED\n");
	return (errors);
}

/**************************************************************************
 *
 * ram_marchx - March X algorithm
 *
 * Described in van de Goor's book, "Testing Semiconductor Memories" and
 * has the following flow:
 *
 * (w0), u(r0,w1), d(r1,w0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and inversion coupling faults
 *
 **************************************************************************/

int ram_marchx(
	__uint32_t rssnum,
	__uint32_t ppnum,
	__uint32_t start_addr, 
	__uint32_t end_addr, 
	__uint32_t allrdram,
	__uint32_t inc,
	__uint32_t stop_on_err,
	__uint32_t broadcast,
	__uint32_t mask,
	__uint32_t numbits)
{

	__uint32_t  errors = 0, addr, actual, cfifo_count, cfifo_status;

	msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x March X Test\n",
		rssnum, ppnum, start_addr, end_addr);

	/* from lomem to himem, write 0 */
	cfifo_count = 0;
        msg_printf(DBG, "Write 0's\n");
	if (allrdram == 0) { /* test all rdram on this pp */
	      	for (addr = ((start_addr & 0xffffff) | broadcast); 
			addr <= ((end_addr & 0xffffff) | broadcast); 
			addr += inc) 
		{
		   	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28)|addr,
				0, mask);
			cfifo_count++;
			if ((cfifo_count & 0xff) == 64) {
			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			   if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	       	 return (-1);
                	    }
               		}
	     	}
	}
	else {
	   for (addr = start_addr; addr <= end_addr; addr += inc) {
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	   }
	}

	/* from lomem to himem read and verify 0's, then write 1's */
        msg_printf(DBG, "Verify 0's and then write 1's\n");
	cfifo_count = 0;
	for (addr = start_addr; addr <= end_addr; addr += inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("March X", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March X Test: FAILED\n");
			return (errors);
		}	
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         ~0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/* Reset the end_addr since the user may have entered an unaligned end
 	 * address, or we may not reach the specified end_addr due to
	 * skipping 2-byte locations if inc > 1.
	 */
	end_addr = addr - inc; 

	/* from himem to lomem read and verify 1's, then write 0 */
	cfifo_count = 0;
        msg_printf(DBG, "Verify 1's and then write 0\n");
	for (addr = end_addr; addr >= start_addr; 
						addr -= inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~0);
		RE_IND_COMPARE("March X", (ppnum << 28) | addr, ~0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March X Test: FAILED\n");
			return (errors);
		}	
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/* from lomem to himem, verify 0 */
        msg_printf(DBG, "Verify 0's\n");
	for (addr = start_addr; addr <= end_addr; addr += inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("March X", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March X Test: FAILED\n");
			return (errors);
		}	
	}
	if (errors) {
		msg_printf(ERR, "RAM March X Test: FAILED\n");
	}
	else
		msg_printf(SUM, "RAM March X Test: PASSED\n");

	return (errors);
}

/**************************************************************************
 *
 * ram_marchy - March Y Test
 *
 * Described in van de Goor's book, "Testing Semiconductor Memories" and
 * has the following flow:
 *
 * (w0), u(r0,w1,r1), d(r1,w0,r0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and linked transition faults
 *
 **************************************************************************/

int ram_marchy(
	__uint32_t rssnum,
	__uint32_t ppnum,
	__uint32_t start_addr, 
	__uint32_t end_addr, 
	__uint32_t allrdram,
	__uint32_t inc,
	__uint32_t stop_on_err,
	__uint32_t broadcast,
	__uint32_t mask,
	__uint32_t numbits)
{

	__uint32_t  errors = 0, addr, actual, cfifo_count, cfifo_status;

	msg_printf(DBG,"RSS-%d, PP-%d, RAM Addr: 0x%x:0x%x March Y Test\n",
		rssnum, ppnum, start_addr, end_addr);

	/* from lomem to himem, write 0 */
	cfifo_count = 0;
        msg_printf(DBG, "Write 0's\n");
	if (allrdram == 0) { /* test all rdram on this pp */
	      	for (addr = ((start_addr & 0xffffff) | broadcast); 
			addr <= ((end_addr & 0xffffff) | broadcast); 
			addr += inc) 
		{
		   	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28)|addr,
				0, mask);
			cfifo_count++;
			if ((cfifo_count & 0xff) == 64) {
			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
			   if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	       	 return (-1);
                	    }
               		}
	     	}
	}
	else {
	   for (addr = start_addr; addr < end_addr; addr += inc) {
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         0, mask);
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	   }
	}

	/* from lomem to himem read and verify 0's, write 1's, read 1's */
        msg_printf(DBG, "Verify 0's, write 1's, then read 1's\n");
	cfifo_count = 0;
	for (addr = start_addr; addr <= end_addr; addr += inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("March Y", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March Y Test: FAILED\n");
			return (errors);
		}	
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         ~0, mask);
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~0);
		RE_IND_COMPARE("March Y", (ppnum << 28) | addr, ~0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March Y Test: FAILED\n");
			return (errors);
		}	
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/* Reset the end_addr since the user may have entered an unaligned end
 	 * address, or we may not reach the specified end_addr due to
	 * skipping 2-byte locations if inc > 1.
	 */
	end_addr = addr - inc; 

	/* from himem to lomem read and verify 1's, then write 0, read 0 */
	cfifo_count = 0;
        msg_printf(DBG, "Verify 1's and then write 0, then read 0\n");
	for (addr = end_addr; addr >= start_addr; 
						addr -= inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, ~0);
		RE_IND_COMPARE("March Y", (ppnum << 28) | addr, ~0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March Y Test: FAILED\n");
			return (errors);
		}	
                WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr,
                         0, mask);
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("March Y", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March Y Test: FAILED\n");
			return (errors);
		}	
		cfifo_count++;
		if ((cfifo_count & 0xff) == 64) {
		   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
		   if (cfifo_status == CFIFO_TIME_OUT) {
			msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       	 return (-1);
                    }
               	}
	}

	/* from lomem to himem, verify 0 */
        msg_printf(DBG, "Verify 0's\n");
	for (addr = start_addr; addr <= end_addr; addr += inc) {
		READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | addr, 
			mask, 0);
		RE_IND_COMPARE("March Y", (ppnum << 28) | addr, 0, 
				actual, mask, errors);
		if (errors && stop_on_err) {
			msg_printf(ERR, "RAM March Y Test: FAILED\n");
			return (errors);
		}	
	}
	if (errors) {
		msg_printf(ERR, "RAM March Y Test: FAILED\n");
	}
	else
		msg_printf(SUM, "RAM March Y Test: PASSED\n");
	return (errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_RDRAM_PIO_memtest						  *
 *                                                                        *
 *	-t [test#]							  *
 *	-r [optional RSS #]						  *
 *	-p [optional PP #]						  *
 *	-a [optional RDRAM memory range]				  *
 *		If the range spans rdrams, we *skip* memory offsets	  *
 *		greater than MEM_HIGH since they don't physcially exist.  *
 *	-i [optional increment]    Test every nth byte chunk 		  *
 *		default is 2.						  *
 *	-x [0 | 1]  -- stop on error, default is 1 = true		  *
 *                                                                        *
 *  	check the memory routine's parameters				  *
 *                                                                        *
 **************************************************************************/

int
mgras_RDRAM_PIO_memtest(int argc, char ** argv)
{
	__uint32_t status, rssnum, ppnum, testnum, errors = 0;
	__uint32_t rflag = 0, tflag = 0, pflag = 0, aflag = 0, bad_arg = 0;	
	__uint32_t inc, rdstart_addr, rdend_addr, end, i, start;
	__uint32_t stop_on_err = 1, rdstart_save;
	__int32_t loopcount = 1;
	struct range range;
	int (*memtest)(__uint32_t,__uint32_t,__uint32_t,__uint32_t,__uint32_t,
		       __uint32_t,__uint32_t,__uint32_t,__uint32_t,__uint32_t);
	ppfillmodeu     ppfillmode = {0};

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
		msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
			status & RE4COUNT_BITS);
		REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_PIO_MEM_TEST]), errors);
        }
#endif
	/* initialize to test everything */
	rssstart = ppstart = 0; rdstart = RDRAM0_MEM;
	rssend = numRE4s; ppend = PP_PER_RSS; rdend = RDRAM2_MEM;
	rdstart_addr = (RDRAM0_MEM << 24); rdend_addr = RDRAM_MEM_MAX;

	inc = 2; /* default is to test every 2-byte location */

	if (argc == 1)
		bad_arg++;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
           switch (argv[0][1]) {
                case 'r':
			if (argv[0][2]=='\0') {
                        	/* RSS select  - optional */
                        	atob(&argv[1][0], (int*)&rssnum);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&rssnum);
                        rflag++; break;
                case 'a':
			if (argv[0][2]=='\0') {
			   if (!(parse_range(&argv[1][0], sizeof(ushort_t),
				&range)))
			   {
				msg_printf(SUM, 
					"Syntax error in address range\n");
				bad_arg++;
			   }
			   argc--; argv++;
			}
			else {
			   if (!(parse_range(&argv[0][2], sizeof(ushort_t),
				&range)))
			   {
				msg_printf(SUM, 
					"Syntax error in address range\n");
				bad_arg++;
			   }
			}
			rdstart_addr = range.ra_base;
			rdend_addr = range.ra_base + 
				(range.ra_size*range.ra_count);
                     	aflag++; break;
                case 'p':
			if (argv[0][2]=='\0') {
                        	/* PP select  - optional */
                        	atob(&argv[1][0], (int*)&ppnum);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&ppnum);
                        pflag++; break;
                case 'i':
			if (argv[0][2]=='\0') {
                        	/* quick mode count */
                        	atob(&argv[1][0], (int*)&inc);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&inc);
			break;
                case 't':
			if (argv[0][2]=='\0') {
                        	/* test number- required */
                        	atob(&argv[1][0], (int*)&testnum);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&testnum);
                        tflag++; break;
		case 'l':
			if (argv[0][2]=='\0') { /* has white space */
				atob(&argv[1][0], (int*)&loopcount); 
				argc--; argv++; 
			}
			else {  /* no white space */
				atob(&argv[0][2], (int*)&loopcount);
			}
			if (loopcount < 0) {
			   msg_printf(SUM, "Error. Loop must be > or = to 0\n");
			   return (0);
			}
			break;	
                case 'x':
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], (int*)&stop_on_err);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&stop_on_err);
                        break;
                default: bad_arg++; break;
           }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
	tflag = 1;
	bad_arg = 0;
#endif

        if ((bad_arg) || (argc) || (tflag == 0)) {
	   msg_printf(SUM, 
		"Usage: -t[test#] -r[optional RSS#]\n");
	   msg_printf(SUM,"\t-p[optional PP #] -a[optional RDRAM addr range]\n");
	   msg_printf(SUM,"\t-i[optional increment]  Test every nth byte, default is 2\n"); 
	   msg_printf(SUM,"\t-x[0 | 1]  -- stop on error, default is 1=true\n");
	   msg_printf(SUM, 
	"\nTest #: WalkingBit=0,AddrUniq=1,Pattern=2,Kh=3,MarchX=4,MarchY=5\n");
	   return (0);
        }

        if (rdram_checkargs(rssnum, rdstart_addr, ppnum, rflag, pflag, aflag, 
		rdend_addr, MEM))
                return (0);

	switch (testnum) {
		case 0: memtest = &ram_walkingbit; break;
		case 1: memtest = &ram_addru; break;
		case 2: memtest = &ram_pattern; break;
		case 3: memtest = &ram_kh; break;
		case 4: memtest = &ram_marchx; break;
		case 5: memtest = &ram_marchy; break;
		default: bad_arg++; break;
	}
#ifdef MGRAS_DIAG_SIM

	rssend = 1; ppend = 2; 
	memtest = ram_pattern;
	bad_arg = 0;
	aflag = 1; 
	inc = 0x18000;
	stop_on_err = 1;

	/* the following variations have also been simulated:
  	 *		aflag = 1; -- to test the allrdram variable in each test
	 *		memtest = ram_marchx;
	 *		memtest = ram_marchy;
	 *		memtest = ram_walkingbit;
	 *		memtest = ram_addru;
	 *		memtest = ram_kh;
	 */
#endif
		
        if ((bad_arg) || (argc)) {
	   msg_printf(SUM,"Unrecognized test number: %d\n", testnum);
	   msg_printf(SUM, 
	"\nTests: WalkingBit=0,AddrUniq=1,Pattern=2,Kh=3,MarchX=4,MarchY=5\n");
	   return (0);
        }

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	/* set up needed registers */
        ppfillmode.bits.DitherEnab = 0;
        ppfillmode.bits.DitherEnabA = 0;
        ppfillmode.bits.ZEnab = 0;
        ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */
	ppfillmode.bits.PixType = PP_FILLMODE_RGB121212;

	while (--loopcount) {
#ifdef MGRAS_DIAG_SIM
	for (x = 0; x < 6; x++) {
	   switch (x) {
		case 0: memtest = ram_walkingbit; break;
		case 1: memtest = ram_addru; break;
		case 2: memtest = ram_pattern; break;
		case 3: memtest = ram_kh; break;
		case 4: memtest = ram_marchx; break;
		case 5: memtest = ram_marchy; break;
	   }
	msg_printf(DBG, "memtest = %d\n", memtest);
#endif

	   /* errors returned by each individual memory test */
	   for (rssnum = rssstart; rssnum < rssend; rssnum++) {
		HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG),
			rssnum, EIGHTEENBIT_MASK);

		/* set up the data to be rgb 12-12-12 */
		WRITE_RSS_REG(rssnum, (PP_FILLMODE& 0x1ff), ppfillmode.val,
			0xffffffff);

	      rdstart_save = rdstart_addr;
	      for (ppnum = ppstart; ppnum < ppend; ppnum++) {
		rdstart_addr = rdstart_save;
           	for (i = rdstart; i <= rdend; i++) {
                  if ((rdstart_addr & TWENTYFOURBIT_MASK) <= MEM_HIGH)
                  {
                     if (((rdend_addr & TWENTYFOURBIT_MASK) > MEM_HIGH) ||
                                (i < rdend))
                     {
                        end = MEM_HIGH;
                     }
                      else
                        end = (rdend_addr & TWENTYFOURBIT_MASK);

                     /* write to 1 rdram at a time */

		     start = (ppnum << 28) + (i << 24) + 
			(rdstart_addr & TWENTYFOURBIT_MASK);
		     msg_printf(DBG,
			"Testing RSS-%d, PP-%d, RDRAM from addr 0x%x to 0x%x\n",
			rssnum, ppnum, start, ((ppnum << 28)+(i << 24) +end));
	     	     errors += (*memtest)(rssnum, ppnum, start,
			(ppnum << 28) + (i << 24) + end,  aflag, 
			inc, stop_on_err, RD_MEM_BROADCAST_WR, 
			EIGHTEENBIT_MASK, HALFWORD);
		     if (errors && stop_on_err) {
			REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_PIO_MEM_TEST +
				ppnum*3*LAST_RDRAM_ERRCODE +
				rssnum*6*LAST_RDRAM_ERRCODE]), errors);	
		     }
                  }
                  rdstart_addr = (rdstart_addr & 0x7000000) + 0x1000000;
		  msg_printf(DBG, "rdstart new: 0x%x, i:%d\n", rdstart_addr, i);
           	} /* rdstart */
	      } /* ppnum */
		if (errors == 0) {
			msg_printf(VRB, 
			"RSS-%d, RDRAM PIO Memory Test: PASSED\n", rssnum);
		}
		else {
			msg_printf(VRB, 
			"RSS-%d, RDRAM PIO Memory Test: FAILED\n", rssnum);
		}
	   } /* rssnum */
#ifdef MGRAS_DIAG_SIM
	  } /* for all testnums */
#endif
        } /* loopcount */

	RETURN_STATUS(errors);
}	
