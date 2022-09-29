 /**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
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
#include <uif.h>
#include <libsc.h>
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"


__uint32_t ppstart;
__uint32_t ppend;
__uint32_t rdstart;
__uint32_t rdend;
__uint32_t re_racstart;
__uint32_t re_racend;
__uint32_t pp_racstart;
__uint32_t pp_racend;

/**************************************************************************
 *                                                                        *
 *  test_RACReg 							  *
 *                                                                        *
 *  	Write/Read a RAC_CONTROL reg. Reset the register when done	  *
 * 	Tests the RE4 RAC_CONTROL regs, and the 3 RDRAM-RAC_CONTROL regs. *
 *	The PP's RAC_CONTROL reg which interfaces with the RE4 is tested  *
 *	via the dcb.
 *                                                                        *
 **************************************************************************/

test_RACReg(__uint32_t rssnum, __uint32_t addr,__uint32_t mask,__uint32_t reset_bit, __uint32_t stop_on_err)
{
	__uint32_t errors = 0, reg_size, status, data, actual, i, no_reset_mask;

	no_reset_mask = mask ^ (1 << reset_bit); /* don't write the reset bit */

	msg_printf(DBG, "Testing RSS-%d, RAC_CONTOL 0x%x\n", rssnum, addr);

	HQ3_PIO_WR_RE((RSS_BASE+ (HQ_RSS_SPACE(CONFIG))), 
			rssnum, TWELVEBIT_MASK);

	/* figure out how big the register is */
        status = mask;
        reg_size = 0;
        while (status) {
                status >>= 1;
                reg_size++;
        }

	msg_printf(DBG, "Walk 1\n");

	/* Walk 1 */
        for (i = 0; i < reg_size; i++) {
	   WRITE_RE_INDIRECT_REG(rssnum, addr, walk_1_0_32_rss[i], mask);
	   READ_RE_INDIRECT_REG(rssnum, addr, mask, walk_1_0_32_rss[i]);
           COMPARE("RAC_CONTROL", RSS_BASE + 
                       HQ_RSS_SPACE(DEVICE_DATA), walk_1_0_32_rss[i], actual, mask, errors);
	   if (errors && stop_on_err) {
		data = 0x222000a4; /* set other default values anyway */
		WRITE_RE_INDIRECT_REG(rssnum, addr, data, mask);
		return (errors);
	   }
        }

	msg_printf(DBG, "Walk 0\n");

        /* Walk 0 */
        for (i = 0; i < reg_size; i++)
        {
	   WRITE_RE_INDIRECT_REG(rssnum, addr, ~walk_1_0_32_rss[i], mask);
	   READ_RE_INDIRECT_REG(rssnum, addr, mask, ~walk_1_0_32_rss[i]);
           COMPARE("RAC_CONTROL", RSS_BASE +
                     HQ_RSS_SPACE(DEVICE_DATA), ~walk_1_0_32_rss[i], actual, mask, errors);
	   if (errors && stop_on_err) {
		data = 0x222000a4; /* set other default values anyway */
		WRITE_RE_INDIRECT_REG(rssnum, addr, data, mask);
		return (errors);
	   }
        }

	/* restore default values via a reset */
	data = 0x222000a4; /* set other default values anyway */
	WRITE_RE_INDIRECT_REG(rssnum, addr, data, mask);
	
        return (errors);
}

/**************************************************************************
 *                                                                        *
 *  rac_checkargs							  *
 *                                                                        *
 *  	Make sure that the user specified args are within range. Set the  *
 *	start and end range for re, pp, rac_control ranges 		  *
 *                                                                        *
 **************************************************************************/

rac_checkargs(__uint32_t rssnum, __uint32_t ppnum, __uint32_t rac_addr, __uint32_t rflag, __uint32_t pflag, __uint32_t aflag)
{
	__uint32_t errors = 0;

	if (rflag ) {
	   if ((rssnum == 0) || ((rssnum >= 1) && (rssnum < numRE4s)))
	   {
		rssstart = rssnum;
		rssend = rssstart+1;
	   }
	   else {
		msg_printf(SUM,
			"The RSS number must be >= 0 and < %d\n", numRE4s);
		errors++;
	   }		
	}

	if (pflag) {
	   if ((ppnum == 0) || ((ppnum >= 1) && (ppnum < PP_PER_RSS))) {
		ppstart = ppnum;
		ppend = ppstart+1;
	   }
	   else {
		msg_printf(SUM, 
			"The PP number must be >= 0 and < %d\n", PP_PER_RSS);
		errors++;
	   }
	}

	if (aflag) {
	   if ((rac_addr == 0x40000000) || (rac_addr == 0x50000000) || 
	     (rac_addr == 0x8000000)) {
		if ((rac_addr == 0x40000000) || (rac_addr == 0x50000000)) {
			re_racstart = rac_addr >> 28; /* get 4 or 5 */
			re_racend = re_racstart+1;
		}
		else {
			pp_racstart = rac_addr >> 24; /* just get 0,1, or 2 */
			pp_racend = pp_racstart+1;
		}
	   }
	   else {
		msg_printf(SUM, "The RAC_CONTROL register address must be: \n");
		msg_printf(SUM, 
		"0x40000000, 0x50000000, or 0x8000000\n");
		errors++;
	   }
	}

	return (errors);
}

/* mgras_PPRACReg is now an obsolete function since PP IMP RAC registers are 
 * now write-only */

/**************************************************************************
 *                                                                        *
 *  mgras_PPRACReg -r[RSS #] -p[PP #] -a[RAC addr] -x[1 | 0] 		  *
 *	RSS, RAC_addr, and PP are all optional arguments		  *
 *                                                                        *
 *  	Write/Read  RAC_CONTROL regs in the PP. The PP's RAC which 	  *
 *	interfaces to the RE4 is tested via the dcb.			  *
 *                                                                        *
 *	-x controls stop_on_err. Default is 1, stop_on_err = TRUE	  *
 *                                                                        *
 *	Examples:							  *
 *									  *
 *	'mgras_PPRACReg'						  *
 *		- test all RAC_CONTROL regs	                  	  *
 *                                                                        *
 *	'mgras_PPRACReg -a0x[addr]' 					  *
 *		- test this RAC in all RSS's 	  	  		  *
 *                                                                        *
 *	'mgras_PPRACReg -p1' 						  *
 *		- test all RACs in PP#1 in all RSS's 	  		  *
 *                                                                        *
 *	'mgras_PPRACReg -p1 -a0x[addr]' 				  *
 *		- test this RAC in PP#1 in all RSS  			  *
 *                                                                        *
 *	'mgras_PPRACReg -r1' 						  *
 *		- test all RAC_CONTROL regs in RSS #1	  		  *
 *                                                                        *
 *	'mgras_PPRACReg -r1 -a0x[8|9|a]000000' 				  *
 *		- test all PPs in RSS #1     				  *
 *                                                                        *
 *	'mgras_PPRACReg -r1 -p1' 					  *
 *		- test all RAC_CONTROL regs in PP#1, RSS#1 		  *
 *                                                                        *
 *	'mgras_PPRACReg -r1 -a0x[8|9|a]000000 -p1' 			  *
 *		- test PP#1 RAC in RSS #1	  			  *
 *                                                                        *
 **************************************************************************/

mgras_PPRACReg(int argc, char ** argv)
{
	__uint32_t status, rssnum, ppnum, rac_addr, errors = 0;
	__uint32_t bad_arg = 0, stop_on_err = 1;
	__uint32_t rflag = 0, aflag = 0, pflag = 0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n");
                                        /*status & RE4COUNT_BITS);*/
                REPORT_PASS_OR_FAIL((&PP_err[PP1_RACREG_TEST]), errors);
        }
#endif
	/* initialize start and end variables to test everything */
	rssstart = 0; ppstart = 0; pp_racstart = PP_RAC_CONTROL0;
	rssend = numRE4s; ppend = PP_PER_RSS; pp_racend = PP_RAC_CONTROL2;

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rac_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rac_addr); 
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
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

	if ((bad_arg) || (argc)) {
		msg_printf(SUM,
  "Usage: -r[optional RSS #] -p[optional PP #] -a[optional RAC addr] -x[1|0] (optional stop-on-error, default =1)\n");
		return (0);
	}

	if (rac_checkargs(rssnum, ppnum, rac_addr, rflag, pflag, aflag))
		return (0);
	
	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	  for (rssnum = rssstart; rssnum < rssend; rssnum++) {
	     for (ppnum = ppstart; ppnum < ppend; ppnum++) {
	   	for (rac_addr = pp_racstart; rac_addr <= pp_racend; rac_addr++)
		{
		   errors += test_RACReg(rssnum,(ppnum << 28)| (rac_addr << 24),
				THIRTYTWOBIT_MASK, 5, stop_on_err);
		   if (errors && stop_on_err)
			REPORT_PASS_OR_FAIL((&PP_err[rssnum*2*LAST_PP_ERRCODE + 
			   ppnum*LAST_PP_ERRCODE + PP1_RACREG_TEST]), errors);
		}
	     }
	     REPORT_PASS_OR_FAIL_NR((&PP_err[rssnum*2*LAST_PP_ERRCODE + 
	   	ppnum*LAST_PP_ERRCODE + PP1_RACREG_TEST]), errors);
	  }
	} /* loopcount */

	RETURN_STATUS(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_RERACReg -r[optional RSS #] -a[optional RAC addr] 		  *
 *		-l[optional loopcount]  -x[optional stopon error]  	  *
 *                                                                        *
 *  	Write/Read  RE4 RDRAM RAC_CONTROL regs. 			  *
 *                                                                        *
 *	Examples:							  *
 *									  *
 *	'mgras_RERACReg'						  *
 *		- test all RAC_CONTROL regs	                  	  *
 *                                                                        *
 *	'mgras_RERACReg -a0x[addr]' 					  *
 *		- test this RAC in all RSS's 	  	  		  *
 *                                                                        *
 *	'mgras_RERACReg -r1' 						  *
 *		- test all RAC_CONTROL regs in RSS #1	  		  *
 *                                                                        *
 *	'mgras_RERACReg -r1 -a0x[addr]' 				  *
 *		- test this RAC_CONTROL reg in RSS #1	  		  *
 *                                                                        *
 *                                                                        *
 **************************************************************************/

int
mgras_RERACReg(int argc, char ** argv)
{
	__uint32_t status, rssnum, ppnum = 0, rac_addr, errors = 0;
	__uint32_t bad_arg = 0, stop_on_err = 1;
	__uint32_t rflag = 0, aflag = 0, pflag = 0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
                REPORT_PASS_OR_FAIL((&RE_err[RE4_RAC_REG_TEST]), errors);
        }
#endif
	/* initialize start and end variables to test everything */
	rssstart = 0; rssend = numRE4s;
	ppstart =  0; ppend = PP_PER_RSS;
	re_racstart = RE_RAC_CONTROL0; re_racend = RE_RAC_CONTROL1;

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rac_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rac_addr); 
				aflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
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

	if ((bad_arg) || (argc)) {
		msg_printf(SUM,
  "Usage: -r[optional RSS #] -a[optional RAC addr] -l[optional loopcount] -x[1|0] (optional stop-on-error, default=1)\n");
		return (0);
	}

	if (rac_checkargs(rssnum, ppnum, rac_addr, rflag, pflag, aflag))
		return (0);
	
	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	  for (rssnum = rssstart; rssnum < rssend; rssnum++) {
	   for (rac_addr = re_racstart; rac_addr <= re_racend; rac_addr++) {
	      errors += test_RACReg(rssnum,(rac_addr<<28),TWENTYTWOBIT_MASK,
			18, stop_on_err);
	      if (errors & stop_on_err)
		REPORT_PASS_OR_FAIL((&RE_err[RE4_RAC_REG_TEST + 
					rssnum*LAST_RE_ERRCODE]), errors);
	   }
	   REPORT_PASS_OR_FAIL_NR((&RE_err[RE4_RAC_REG_TEST + 
			rssnum*LAST_RE_ERRCODE]), errors);

	  }
	}

	RETURN_STATUS(errors);
}
#if 0
/**************************************************************************
 *                                                                        *
 *  rdwr_RACReg [RSS #] [RAC addr] [optional PP #]			  *
 *                                                                        *
 *  	Read the specified RAC_CONTROL reg. The PP's RAC which 		  *
 *	interfaces to the RE4 is tested via the dcb.			  *
 *                                                                        *
 **************************************************************************/

__uint32_t
rdwr_RACReg(__uint32_t rssnum, __uint32_t rac_addr, __uint32_t ppnum, __uint32_t data, __uint32_t access)
{
	__uint32_t actual, mask, reset_bit;

	   switch (rac_addr) {
		case 0x8000000: reset_bit = 5; mask = THIRTYONEBIT_MASK; break;
		case 0x40000000:
		case 0x50000000: reset_bit = 18; mask = TWENTYNINEBIT_MASK;
				break;
		default: reset_bit = 0x99; break;
	   }
	   if (reset_bit != 0x99) {
		HQ3_PIO_WR_RE(RSS_BASE+ HQ_RSS_SPACE(CONFIG),
			rssnum, TWELVEBIT_MASK);

		if (reset_bit == 18)
		/* if this is true, user is talking to re, so set ppnum = 0 */
			ppnum = 0; 
   		if (access == WRITE) {
		   msg_printf(DBG, 
			   "Writing RSS-%d, RAC_CONTROL 0x%x, data: 0x%x\n",
				rssnum, rac_addr, data);
		   WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | rac_addr, 
							data, mask);
#ifdef MGRAS_DIAG_SIM
		   if ((rac_addr == 0x40000000) || (rac_addr == 0x50000000)) {
		    READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | rac_addr, mask,
				CSIM_RSS_DEFAULT);
		   }
		   else {	
		   	WRITE_RE_INDIRECT_REG(rssnum, (ppnum << 28) | 0x8000000,
							data + 1, mask);
		   }
#endif
		}
		else if (access == READ) {
		   if ((rac_addr != 0x40000000) && (rac_addr != 0x50000000)) {
			msg_printf(SUM, 
				"RAC_CONTROL register 0x%x is not readable\n",
				rac_addr);	
		   }
		   else {	
		    READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | rac_addr, mask,
				CSIM_RSS_DEFAULT);
       		    msg_printf(SUM,
			 "RSS-%d RAC_CONTROL reg 0x%x, contents: 0x%x\n",
					rssnum, rac_addr, actual);
		   }
		}
	   }
	   else {
	     	msg_printf(SUM, 
		   "Illegal RAC_CONTROL register address: 0x%x\n", rac_addr);
		return (0);
	   }
	   return(0);
}

/**************************************************************************
 *                                                                        *
 *  mgras_ReadRACReg -r[RSS #] -a[RAC addr] -p[optional PP #]		  *
 *                                                                        *
 *  	Read the specified RAC_CONTROL reg. The PP's RAC which 		  *
 *	interfaces to the RE4 is tested via the dcb.			  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_ReadRACReg(int argc, char ** argv)
{
	__uint32_t rssnum, rac_addr, ppnum, status;
	__uint32_t bad_arg = 0, errors = 0, rflag = 0, aflag = 0, pflag = 0;
	__int32_t loopcount = 1;
	
	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (-1);
        }
#endif
	/* initialize */
	ppnum = 0;
	
	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rac_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rac_addr); 
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
			default: bad_arg++; break;
		}
		argc--; argv++;
	}
#ifdef MGRAS_DIAG_SIM
	rssnum = 0; ppnum = 0; rac_addr = 0x40000000;
	rflag = 1; pflag = 1; aflag = 1;
#endif

	if ((bad_arg) || (argc)) {
 		msg_printf(SUM, 
		"Usage: -r[RSS #] -a[RAC addr] -p[optional PP # (default=0)] -l[optional loopcount]\n");
		return (0);
	}
	if ((rflag == 0) || (aflag == 0)) {
		msg_printf(SUM, 
			"You must specify an RSS# and a RAC_CONTROL address\n");
		return (0);
	}

	if (rac_checkargs(rssnum, ppnum, rac_addr, rflag, pflag, aflag))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
		rdwr_RACReg(rssnum, rac_addr, ppnum, 0, READ);
	}
	return(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_WriteRACReg -r[RSS #] -a[RAC addr] -d[data] -p[optional PP #]	  *
 *                                                                        *
 *  	Write the specified RAC_CONTROL reg. The PP's RAC which 	  *
 *	interfaces to the RE4 is tested via the dcb.			  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_WriteRACReg(int argc, char ** argv)
{
	__uint32_t bad_arg = 0, rssnum, rac_addr, ppnum, status;
	__uint32_t errors = 0, data, rflag = 0, aflag = 0, pflag = 0, dflag = 0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (-1);
        }
#endif
	/* initialize */
	ppnum = 0;
	
	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rac_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rac_addr); 
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'd':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&data);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&data);
				dflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
			default: bad_arg++; break;
		}
		argc--; argv++;
	}

#ifdef MGRAS_DIAG_SIM
	rssnum = 0; ppnum = 0; rac_addr = 0x8000000; data = 0x11223344;
	rflag = 1; pflag = 1; aflag = 1; dflag = 1;
#endif
	if ((bad_arg) || (argc)) {
 		msg_printf(SUM, 
"Usage: -r[RSS #] -a[RAC addr] -d[data] -p[optional PP # (default =0)] -l[optional loopcount]\n");
		return (0);
	}

	if ((rflag == 0) || (aflag == 0) || (dflag == 0)) {
		msg_printf(SUM,
		"You must specify a RSS#, a RAC_CONTROL address, and data\n");
		return (0);
	}

	if (rac_checkargs(rssnum, ppnum, rac_addr, rflag, pflag, aflag))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
		rdwr_RACReg(rssnum, rac_addr, ppnum, data, WRITE);
	}

	return(errors);
}
#endif
/**************************************************************************
 *                                                                        *
 *  rdram_checkargs							  *
 *                                                                        *
 *  	Make sure that the user specified args are within range. Set the  *
 *	start and end range for re, pp, rdram register ranges 		  *
 *                                                                        *
 **************************************************************************/

int
rdram_checkargs(
	__uint32_t rssnum, 
	__uint32_t rdram_addr, 
	__uint32_t ppnum,  
	__uint32_t rflag, 
	__uint32_t pflag, 
	__uint32_t aflag, 
	__uint32_t rdend_addr,
	__uint32_t is_reg)
{
	__uint32_t errors = 0;

	/* is_reg ==1 ==> register, == 0 ==> memory */

	if (rflag) {
	   if ((rssnum == 0) || ((rssnum >= 1) && (rssnum < numRE4s)))
	   {
		rssstart = rssnum;
		rssend = rssstart+1;
	   }
	   else {
		msg_printf(SUM, "The RSS number must be >= 0 and < %d\n",
					numRE4s);
		errors++;
	   }		
	}

	if (pflag) {
	   if ((ppnum == 0) || ((ppnum >= 1) && (ppnum < PP_PER_RSS))) {
		ppstart = ppnum;
		ppend = ppstart+1;
	   }
	   else {
		msg_printf(SUM, "The PP number must be >= 0 and < %d\n",
			PP_PER_RSS);
		errors++;
	   }
	}

	if (aflag) {
	   if (is_reg) { /* rdram base is a register address */
	      if (((rdram_addr & 0x3000000) == 0x0) || 
			((rdram_addr & 0x3000000) == 0x1000000) || 
			((rdram_addr & 0x3000000) == 0x2000000) ||
			((rdram_addr & 0x3000000) == 0x3000000) ) {
			rdstart = rdram_addr >> 24; /* just get 0,1, or 2 */
			rdend = rdstart+1;
	      }
	      else {
		msg_printf(SUM,"The RDRAM register address must begin with: \n");
		msg_printf(SUM, "0x0, 0x1000000, 0x2000000, or 0x3000000\n");
		errors++;
	      }
	   }
	   else { /* rdram addr is a memory base address */
		rdstart = ((rdram_addr & 0x7000000) >> 24);
		rdend = ((rdend_addr & 0x7000000) >> 24);

		/* check the memory range */
		if (rdend_addr < rdram_addr) {
		   msg_printf(SUM,
			"Ending address is less than the starting address.\n");
		   errors++;
		}
		if ((rdend_addr<RDRAM_MEM_MIN)|| (rdram_addr<RDRAM_MEM_MIN) ||
		     (rdend_addr>RDRAM_MEM_MAX)|| (rdram_addr>RDRAM_MEM_MAX))
		{ 
		   msg_printf(SUM, 
			"The specified address is out of range.\n");
		   msg_printf(SUM, "Addresses must be between 0x%x and 0x%x\n",
			RDRAM_MEM_MIN, RDRAM_MEM_MAX);
		   errors++;
		}
	   }
	}

	return (errors);
}

/**************************************************************************
 *                                                                        *
 *  rdwr_RDRAMReg 							  *
 *                                                                        *
 *  	Read or write the specified RDRAM reg. 				  *
 *                                                                        *
 **************************************************************************/

__uint32_t
rdwr_RDRAMReg(__uint32_t rssnum, __uint32_t rdram_base, __uint32_t ppnum, __uint32_t data, __uint32_t access)
{
	__uint32_t actual;

	HQ3_PIO_WR_RE(RSS_BASE+ HQ_RSS_SPACE(CONFIG), rssnum, TWELVEBIT_MASK);

   	if (access == READ) {
          	READ_RE_INDIRECT_REG(rssnum, (ppnum << 28) | (rdram_base << 24),
		 	THIRTYTWOBIT_MASK, CSIM_RSS_DEFAULT);
	   	msg_printf(DBG,
			"Reading RSS-%d, PP-%d RDRAM reg 0x%x, contents: 0x%x\n"
				,rssnum, ppnum , rdram_base, actual);
	}
	else if (access == WRITE) {
		msg_printf(DBG, 
		   "Writing RSS-%d, PP-%d, RDRAM reg 0x%x, data: 0x%x\n",
			rssnum, ppnum , rdram_base, data);
		WRITE_RE_INDIRECT_REG(rssnum,(ppnum << 28) | (rdram_base << 24),
			data, THIRTYTWOBIT_MASK);
	}
	return(0);
}

/**************************************************************************
 *                                                                        *
 *  mgras_ReadRDRAMReg -r[optional RSS #] -p[optional PP #] 		  *
 *	-a[optional RDRAM reg base addr] -l[optional loopcount]		  *
 *                                                                        *
 *  	Read the specified RDRAM reg. 					  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_ReadRDRAMReg(__uint32_t argc, char ** argv)
{
	__uint32_t rssnum, rdram_base, rdram_addr, ppnum, status;
	__uint32_t errors = 0, bad_arg = 0;
	__uint32_t rflag = 0, aflag = 0, pflag = 0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (1);
        }
#endif
	/* initialize to test everything */
	rssnum = ppnum = rdram_base = rdram_addr = 0;
	rssstart = 0; ppstart = 0; rdstart = 0;
	rssend = numRE4s; ppend = PP_PER_RSS; rdend = RDRAM2_REG;

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rdram_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rdram_addr); 
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
			default: bad_arg++; break;
		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
 		msg_printf(SUM, 
	   "Usage: -r[optional RSS #] -p[optional PP#] -a[optional RDRAM base addr] -l[optional loopcount]\n");
		return (0);
	}

	if (rdram_checkargs(rssnum, rdram_addr,ppnum,rflag,pflag,aflag,0,REG))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

#if 0
	msg_printf(DBG, "rssnum:%d, ppnum:%d, rdram_addr:%x, rssstart: %d, rssend:%d, ppstart: %d, ppend: %d, rdstart: %x, rdend: %x, loop: %d\n", rssnum, ppnum, rdram_addr, rssstart, rssend, ppstart, ppend, rdstart, rdend, loopcount);
#endif
	while (--loopcount) {
	  /* no error checking */
	  for (rssnum = rssstart; rssnum < rssend; rssnum++)
	     for (ppnum = ppstart; ppnum < ppend; ppnum++)
		for (rdram_base = rdstart; rdram_base < rdend; rdram_base++)
			rdwr_RDRAMReg(rssnum, rdram_base, ppnum, 0, READ);
	}

	return(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_WriteRDRAMReg -r[RSS #] -p[PP #] -a[RDRAM addr] -d[data]	  *
 *			-l[optional loopcount]				  *
 *                                                                        *
 *  	Read the specified RDRAM reg. 					  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_WriteRDRAMReg(__uint32_t argc, char ** argv)
{
	__uint32_t rssnum, rdram_base, rdram_addr, ppnum, status, data;
	__uint32_t errors = 0, bad_arg = 0;
	__uint32_t rflag = 0, aflag = 0, pflag = 0, dflag = 0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (1);
        }
#endif
	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rdram_addr); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rdram_addr); 
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'd':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&data);
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&data);
				dflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
			default: bad_arg++; break;
		}
		argc--; argv++;
	}

#ifdef MGRAS_DIAG_SIM
	rssnum = 0; ppnum = 0; rdram_addr = 0x2000000; data = 0x11223344;
	rflag = 1; pflag = 1; aflag = 1; dflag = 1;
#endif
	if ((bad_arg) || (argc)) {
 		msg_printf(SUM, 
"Usage: -r[RSS #] -p[ PP#] -a[RDRAM addr] -d[data] -l[optional loopcount]\n");
		return (0);
	}
	if ((rflag == 0) || (aflag == 0) || (pflag == 0) || (dflag == 0)) {
		msg_printf(SUM, 
		"You must specify RSS#, PP#, RDRAM reg addr, and data\n");
		return (0);
	}

	if (rdram_checkargs(rssnum,rdram_addr,ppnum,rflag, pflag,aflag,0,REG))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	rdram_base = (rdram_addr >> 24); /* gets 0, 1, or 2 */
	while (--loopcount) {
		rdwr_RDRAMReg(rssnum, rdram_base, ppnum, data, WRITE);
	}

	return(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_RDRAM_Databus 						  *
 *	-r[optional RSS #] 						  *
 *	-p[optional PP #]		  				  *
 *	-a[optional RDRAM addr base], specifies 1 RDRAM to test. 	  *
 *	-x[1 | 0]  -- stop_on_err, default is 1				  *
 *	-l[optional loopcount]						  *
 *                                                                        *
 *  	Test the RDRAM Data bus					  	  *
 *                                                                        *
 *	Walk a '1' and a '0' on the data bus. Write to addr[0]		  *
 *	and verify the written data.					  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_RDRAM_Databus(__uint32_t argc, char ** argv)
{
	__uint32_t i, j, k, actual, rssnum, ppnum, rdram_base, rdram_end,status;
	__uint32_t errors = 0, bad_arg = 0, cnt, stop_on_err = 1;
	__uint32_t rflag = 0, aflag = 0, pflag = 0, rd_dbus_size = 18;
	__int32_t loopcount = 1;
	ppfillmodeu  ppfillMode = {0};
	
	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
                REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_DATABUS_TEST]), errors);
        }
#endif
	/* initialize values to test everything */
	rssstart = 0; ppstart = 0; rdstart = RDRAM0_MEM;
	rssend = numRE4s; ppend = PP_PER_RSS; rdend = RDRAM2_MEM;
	rdram_base = (RDRAM0_MEM << 24); rdram_end = RDRAM_MEM_MAX;
	rssnum = 0;

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rdram_base); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rdram_base); 
				rdram_end = rdram_base + MEM_HIGH;
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
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

	if ((bad_arg) || (argc)) {
 	   msg_printf(SUM, 
 "Usage: -r[optional RSS #] -p[optional PP #] -a[optional RDRAM addr base] -l[optional loopcount] -x[1|0] (optional stop-on-error, default = 1)\n");
		return (0);
	}	

	if (rdram_checkargs(rssnum,rdram_base,ppnum,rflag,pflag, aflag,
		rdram_end, MEM))
		return (0);

	/* For walking bits on the addr bus, the LSB is always 0 since we
   	 * are always reading 2 bytes at a time. Therefore we do the walk
	 * on the most significant n-1 bits, where n is the addr bus size.
 	 * n is 21 bits currently [20:0]
	 */

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {

	  /* Walk 1 on data bus, write all to addr[0] */
	  for (i = rssstart; i < rssend; i++) {
	   HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(CONFIG), i, TWELVEBIT_MASK);

	   /* set the PP fillmode to use 12-12-12 */
	   ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
	   ppfillMode.bits.BufSize  = 1;
	   ppfillMode.bits.DrawBuffer  = 1;
	   WRITE_RSS_REG(i, (PP_FILLMODE& 0x1ff), ppfillMode.val, ~0x0);

	   for (j = ppstart; j < ppend; j++) {
		for (k = rdstart; k <= rdend; k++) {
		   msg_printf(DBG,"Walking 1 on RSS-%d, PP-%d, RDRAM 0x%x\n",
			i, j, (k << 24));
		   for (cnt = 0; cnt < rd_dbus_size; cnt++) {
		   	WRITE_RE_INDIRECT_REG(i, (j << 28) | (k << 24),
			 walk_1_0_32_rss[cnt], EIGHTEENBIT_MASK);
		   	READ_RE_INDIRECT_REG(i, (j << 28) | (k << 24),
			 EIGHTEENBIT_MASK, walk_1_0_32_rss[cnt]);
		   	COMPARE("mgras_RDRAM_Databus", (j << 28) | (k << 24),
			   walk_1_0_32_rss[cnt], actual, EIGHTEENBIT_MASK, errors);

			if (errors) {
			   msg_printf(VRB,
                           "RSS-%d, PP-%d, rdram-%d, wrote: 0x%x, read: 0x%x\n",
                                i, j, (k % RDRAM0_MEM), walk_1_0_32_rss[cnt], 
				actual);

			   REPORT_PASS_OR_FAIL_NR((&RDRAM_err[RDRAM_DATABUS_TEST
			   + i*6*LAST_RDRAM_ERRCODE + j*3*LAST_RDRAM_ERRCODE +
			   (k % RDRAM0_MEM)*LAST_RDRAM_ERRCODE]), errors);

			   if (stop_on_err) {
				RETURN_STATUS(errors);
			   }
			}	
		   }
		}
	   }
	   if (errors == 0) {
		msg_printf(DBG, "RSS-%d RDRAM Databus Test: PASSED\n", i);
	   }
	  }

	  /* Walk 0 on data bus, write all to addr[0] */
	  for (i = rssstart; i < rssend; i++) {
	   HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(CONFIG), i, TWELVEBIT_MASK);

	   /* set the PP fillmode to use 12-12-12 */
	   ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
	   ppfillMode.bits.BufSize  = 1;
	   ppfillMode.bits.DrawBuffer  = 1;
	   WRITE_RSS_REG(i, (PP_FILLMODE& 0x1ff), ppfillMode.val, ~0x0);

	   for (j = ppstart; j < ppend; j++) {
		for (k = rdstart; k <= rdend; k++) {
		   msg_printf(DBG,"Walking 0 on RSS-%d, PP-%d, RDRAM 0x%x\n",
			i, j, (k << 24));
		   for (cnt = 0; cnt < rd_dbus_size; cnt++) {
		   	WRITE_RE_INDIRECT_REG(i, (j << 28) | (k << 24),
			   ~walk_1_0_32_rss[cnt],EIGHTEENBIT_MASK);
		   	READ_RE_INDIRECT_REG(i, (j << 28) | (k << 24),
				EIGHTEENBIT_MASK, ~walk_1_0_32_rss[cnt]);
		   	COMPARE("mgras_RDRAM_Databus", (j << 28) | (k << 24),
			   ~walk_1_0_32_rss[cnt], actual, EIGHTEENBIT_MASK, errors);

		   	if (errors) {
			   msg_printf(VRB,
                           "RSS-%d, PP-%d, rdram-%d, wrote: 0x%x, read: 0x%x\n",
                                i, j, (k % RDRAM0_MEM), walk_1_0_32_rss[cnt], 
				actual);

			   REPORT_PASS_OR_FAIL_NR((&RDRAM_err[RDRAM_DATABUS_TEST
			   + i*6*LAST_RDRAM_ERRCODE + j*3*LAST_RDRAM_ERRCODE +
			   (k % RDRAM0_MEM)*LAST_RDRAM_ERRCODE]), errors);
		
			   if (stop_on_err) {
				RETURN_STATUS(errors);
			   }
			}
		   }
		}
	   }
	   if (errors == 0) {
		msg_printf(DBG, "RSS-%d RDRAM Databus Test: PASSED\n", i);
	   }
	  }
	
	} /* loopcount */

#if 0
   	REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_DATABUS_TEST]), errors);
#endif
	if (errors == 0) {
		msg_printf(VRB, "RDRAM Databus Test: PASSED\n");
		return (0);
	}
	else {
		msg_printf(VRB, "RDRAM Databus Test: FAILED\n");
		return (1);
	}
}

/**************************************************************************
 *                                                                        *
 *  mgras_RDRAM_Addrbus -r[optional RSS #] -p[optional PP #]		  *
 *			 -a[opt RDRAM addr base] -x[1|0] 		  *
 *			-l[optional loopcount]				  *
 *                                                                        *
 *  	Test the RDRAM address bus					  *
 *                                                                        *
 *	Walk a '1' and a '0' on the address bus. Write ~address in address*
 *	and verify the written data.					  *
 *                                                                        *
 * 	-x controls stop_on_err						  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_RDRAM_Addrbus(__uint32_t argc, char ** argv)
{
	__uint32_t i, j, k, actual, rssnum, ppnum, rdram_base, status;
	__uint32_t errors = 0, bad_arg = 0, cnt, rdram_end, stop_on_err = 1;
	__uint32_t rflag = 0, aflag = 0, pflag = 0, rd_abus_size = 20;
	__int32_t loopcount = 1;
	ppfillmodeu  ppfillMode = {0};
	
	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_ADDRBUS_TEST]), errors);
        }
#endif
	/* initialize values to test everything */
	rssstart = 0; ppstart = 0; rdstart = RDRAM0_MEM;
	rssend = numRE4s; ppend = PP_PER_RSS; rdend = RDRAM2_MEM;
	rdram_base = (RDRAM0_MEM << 24); rdram_end = RDRAM_MEM_MAX;
	rssnum = 0;

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'r': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rssnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rssnum); 
				rflag++; break;
			case 'a': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&rdram_base); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&rdram_base); 
				rdram_end = rdram_base + MEM_HIGH;
				aflag++; break;
			case 'p': 
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int*)&ppnum); 
					argc--; argv++;
				}
				else
					atob(&argv[0][2], (int*)&ppnum); 
				pflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else {  /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					  "Error. Loop must be > or = to 0\n");
					return (0);
				} break;
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

	if ((bad_arg) || (argc)) {
 	   msg_printf(SUM, 
 "Usage: -r[optional RSS #] -p[optional PP #] -a[optional RDRAM addr base] -l[optional loopcount] -x[1|0] (optional stop-on-error, default =1)\n");
		return (0);
	}	

	if (rdram_checkargs(rssnum,rdram_base,ppnum,rflag,pflag, aflag,
		rdram_end, MEM))
		return (0);

	/* For walking bits on the addr bus, the LSB is always 0 since we
   	 * are always reading 2 bytes at a time. Therefore we do the walk
	 * on the most significant n-1 bits, where n is the addr bus size.
 	 * n is 21 bits currently [20:0]
	 */

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {

	/* Walk 1, starting with 0x1, write ~addr to addr */
	  for (i = rssstart; i < rssend; i++) {
	   HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(CONFIG), i, TWELVEBIT_MASK);

	   /* set the PP fillmode to use 12-12-12 */
	   ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
	   ppfillMode.bits.BufSize  = 1;
	   ppfillMode.bits.DrawBuffer  = 1;
	   WRITE_RSS_REG(i, (PP_FILLMODE& 0x1ff), ppfillMode.val, ~0x0);

	   for (j = ppstart; j < ppend; j++) {
		for (k = rdstart; k <= rdend; k++) {
		   msg_printf(DBG,"Walking 0 on RSS-%d, PP-%d, RDRAM 0x%x\n",
			i, j, (k << 24));

		  /* a 1 in the LSB of the address will be treated the same as
		   * a 0 because we access 2 bytes at a time, but test it anyway
		   */ 
		   for (cnt = 0; cnt <= rd_abus_size; cnt++) {
		   	WRITE_RE_INDIRECT_REG(i, (j << 28) | (k << 24) | 
			 walk_1_0_32_rss[cnt],~walk_1_0_32_rss[cnt], EIGHTEENBIT_MASK);
		   	READ_RE_INDIRECT_REG(i, (j << 28) | (k << 24) |
			 walk_1_0_32_rss[cnt],EIGHTEENBIT_MASK, ~walk_1_0_32_rss[cnt]);
		   	COMPARE("mgras_RDRAM_Addrbus", (j << 28) | (k << 24)|
			   walk_1_0_32_rss[cnt], ~walk_1_0_32_rss[cnt], actual, 
				EIGHTEENBIT_MASK, errors);
		   	if (errors) {
			   msg_printf(ERR,
                           "RSS-%d, PP-%d, rdram-%d, wrote: 0x%x, read: 0x%x\n",
                                i, j, (k % RDRAM0_MEM), ~walk_1_0_32_rss[cnt], 
				actual);

			   REPORT_PASS_OR_FAIL_NR((&RDRAM_err[RDRAM_ADDRBUS_TEST
			   + i*6*LAST_RDRAM_ERRCODE + j*3*LAST_RDRAM_ERRCODE +
			   (k % RDRAM0_MEM)*LAST_RDRAM_ERRCODE]), errors);

			   if (stop_on_err) {
				RETURN_STATUS(errors);
			   }
			}
		   }
		}
	   }
	   if (errors == 0) {
		msg_printf(DBG, "RSS-%d RDRAM Addrbus Test: PASSED\n", i);
	   }
	  }

	  /* Walk 0, starting with LSByte= (0xe), write addr to addr */
	  for (i = rssstart; i < rssend; i++) {
	   HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(CONFIG), i, TWELVEBIT_MASK);

	   /* set the PP fillmode to use 12-12-12 */
	   ppfillMode.bits.PixType = PP_FILLMODE_RGB121212;
	   ppfillMode.bits.BufSize  = 1;
	   ppfillMode.bits.DrawBuffer  = 1;
	   WRITE_RSS_REG(i, (PP_FILLMODE& 0x1ff), ppfillMode.val, ~0x0);

		  /* a 1 in the LSB of the address will be treated the same as
		   * a 0 because we access 2 bytes at a time, but test it anyway
		   */ 
	   for (j = ppstart; j < ppend; j++) {
		for (k = rdstart; k <= rdend; k++) {
		   msg_printf(DBG,"Walking 1 on RSS-%d, PP-%d, RDRAM 0x%x\n",
			i, j, (k << 24));
		   for (cnt = 0; cnt <= rd_abus_size; cnt++) {
		   	WRITE_RE_INDIRECT_REG(i, (j << 28) | (k << 24) | 
			   (~walk_1_0_32_rss[cnt] & EIGHTEENBIT_MASK), 
				walk_1_0_32_rss[cnt], EIGHTEENBIT_MASK);
		   	READ_RE_INDIRECT_REG(i, (j << 28) | (k << 24) |
				(~walk_1_0_32_rss[cnt] & EIGHTEENBIT_MASK), 
				EIGHTEENBIT_MASK, walk_1_0_32_rss[cnt]);
		   	COMPARE("mgras_RDRAM_Addrbus", (j << 28) | (k << 24)|
			   (~walk_1_0_32_rss[cnt] & EIGHTEENBIT_MASK),
				walk_1_0_32_rss[cnt], actual, EIGHTEENBIT_MASK, errors);

		   	if (errors) {
			   msg_printf(VRB,
                           "RSS-%d, PP-%d, rdram-%d, wrote: 0x%x, read: 0x%x\n",
                                i, j, (k % RDRAM0_MEM), walk_1_0_32_rss[cnt], 
				actual);

		           REPORT_PASS_OR_FAIL_NR((&RDRAM_err[RDRAM_ADDRBUS_TEST
			    + i*6*LAST_RDRAM_ERRCODE + j*3*LAST_RDRAM_ERRCODE +
			   (k % RDRAM0_MEM)*LAST_RDRAM_ERRCODE]), errors);

			   if (stop_on_err) {
				RETURN_STATUS(errors);
			   }
			}
		   }
		}
	   }
	   if (errors == 0) {
		msg_printf(DBG, "RSS-%d RDRAM Addrbus Test: PASSED\n", i);
	   }
	  }
	} /* loopcount */

#if 0
   	REPORT_PASS_OR_FAIL((&RDRAM_err[RDRAM_ADDRBUS_TEST]), errors);
#endif
	if (errors == 0) {
		msg_printf(VRB, "RDRAM Addrbus Test: PASSED\n");
		return (0);
	}
	else {
		msg_printf(VRB, "RDRAM Addrbus Test: FAILED\n");
		return (1);
	}
}

/**************************************************************************
 *                                                                        *
 *  mgras_ReadRDRAM_PIO -r[RSS #] -p[PP #] -a[RDRAM addr range] 	  *
 *			-l[optional loopcount]				  *
 *                                                                        *
 *  	Read the RDRAM contents at the specified address		  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_ReadRDRAM_PIO(__uint32_t argc, char ** argv)
{
	__uint32_t actual, i, j, end, rssnum, ppnum, status;
	__uint32_t errors = 0, bad_arg = 0, rdstart_addr, rdend_addr;
	__uint32_t rflag = 0, aflag = 0, pflag = 0;
	__int32_t loopcount = 1;
	struct range range;

	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (1);
        }
#endif
	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	   switch (argv[0][1]) {
		case 'r': 
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&rssnum); 
				argc--; argv++;
			}
			else
				atob(&argv[0][2], (int*)&rssnum); 
			rflag++; break;
		case 'a': 
			if (argv[0][2]=='\0') {

		     	/* we're reading 2 bytes at a time, so use a size of 16 
		      	 * We're really using 9-bit bytes but doesn't matter
		      	 */

		     		if (!(parse_range(&argv[1][0], sizeof(ushort_t),					&range)))
		     		{
			   		msg_printf(SUM, 
					   "Syntax error in address range\n");	
			   		bad_arg++; 
		     		}
				argc--; argv++;
			}
			else {
		     		if (!(parse_range(&argv[0][2], sizeof(ushort_t),					&range)))
		     		{
			   		msg_printf(SUM, 
					   "Syntax error in address range\n");	
			   		bad_arg++; 
		     		}
			}
		     	aflag++; break;
		case 'p': 
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&ppnum); 
				argc--; argv++;
			}
			else
				atob(&argv[0][2], (int*)&ppnum); 
			pflag++; break;
		case 'l':
			if (argv[0][2]=='\0') { /* has white space */
				atob(&argv[1][0], (int*)&loopcount);
				argc--; argv++;
			}
			else {  /* no white space */
				atob(&argv[0][2], (int*)&loopcount);
			}
			if (loopcount < 0) {
				msg_printf(SUM, 
				  "Error. Loop must be > or = to 0\n");
				return (0);
			} break;
		default: bad_arg++; break;
	   }
	   argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
 	   msg_printf(SUM, 
    "Usage: -r[RSS #] -p[PP #] -a[RDRAM addr range] -l[optional loopcount]\n");
		return (0);
	}	

	rdstart_addr = range.ra_base;
	rdend_addr = range.ra_base + (range.ra_size*range.ra_count);

#ifdef MGRAS_DIAG_SIM
	rflag = 1; aflag = 1; pflag = 1; rssnum = 0; ppnum = 1; 
	rdstart_addr = 0x41ffff0; rdend_addr = 0x5000004;
#endif
	if ((rflag == 0) || (aflag == 0) || (pflag == 0)) { 
		msg_printf(SUM, 
		   "You must specify RSS#, PP#, and RDRAM memory address\n");
		return (0);
	}
	if (rdram_checkargs(rssnum, rdstart_addr, ppnum, rflag, 
		pflag, aflag, rdend_addr, MEM))
		return (0);

	/* read 2 bytes (9-bit) for each mem access */
	msg_printf(DBG, "Reading RSS-%d, PP-%d, RDRAM addresses 0x%x to 0x%x\n",
			rssnum, ppnum, rdstart_addr, rdend_addr);
	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(CONFIG),rssnum,
		TWELVEBIT_MASK);

	while (--loopcount) {
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

		   /* write to 1 rdram at a time, so this for loop covers
		    * the offset within 1 rdram */

		   for (j =(rdstart_addr & TWENTYFOURBIT_MASK); j<= end; j += 2)
		   {
			READ_RE_INDIRECT_REG(rssnum, ((ppnum << 28) + 
			   (i << 24) + j), EIGHTEENBIT_MASK, CSIM_RSS_DEFAULT);
			msg_printf(VRB, "Addr: 0x%x, Read: 0x%x\n",
			 ((ppnum << 28) +(i << 24) + j), actual);
		   }
		}
		rdstart_addr = (rdstart_addr & 0x7000000) + 0x1000000;
	   }
	}
	return(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_WriteRDRAM_PIO -r[RSS #] -p[PP #] -a[RDRAM addr range] -d[data] *
 *			-l[optional loopcount]				  *
 *                                                                        *
 *  	Write the RDRAM contents at the specified address with data	  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_WriteRDRAM_PIO(__uint32_t argc, char ** argv)
{
	__uint32_t i, j, rssnum, ppnum, status, rdend_addr, end;
	__uint32_t errors = 0, bad_arg = 0, data, rdstart_addr;
	__uint32_t rflag = 0, aflag = 0, pflag =0, dflag = 0;
	__int32_t loopcount = 1;
	struct range range;
	
	NUMBER_OF_RE4S();
#if 0
	if (errors) {
                msg_printf(ERR, "RE4Count is %d, allowed values are 1-4\n",
                                        status & RE4COUNT_BITS);
		return (1);
        }
#endif
	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	   switch (argv[0][1]) {
		case 'r': 
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&rssnum); 
				argc--; argv++;
			}
			else
				atob(&argv[0][2], (int*)&rssnum); 
			rflag++; break;
		case 'a': 
			if (argv[0][2]=='\0') {

		     /* we're reading 2 bytes at a time, so use a size of 16. 
		      * We're really using 9-bit bytes but that doesn't matter
		      */

		     		if (!(parse_range(&argv[1][0], sizeof(ushort_t),					&range)))
		     		{
			   		msg_printf(SUM, 
					   "Syntax error in address range\n");	
			   		bad_arg++; 
		     		}
				argc--; argv++;
			}
			else {
		     		if (!(parse_range(&argv[0][2], sizeof(ushort_t),					&range)))
		     		{
			   		msg_printf(SUM, 
					   "Syntax error in address range\n");	
			   		bad_arg++; 
		     		}
			}
		     	aflag++; break;
		case 'p': 
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&ppnum); 
				argc--; argv++;
			}
			else
				atob(&argv[0][2], (int*)&ppnum); 
			pflag++; break;
		case 'd':
			if (argv[0][2]=='\0') {
			/* data - required */
				atob(&argv[1][0], (int*)&data);
				argc--; argv++;
			}
			else
				atob(&argv[0][2], (int*)&data);
			dflag++; break;
		case 'l':
			if (argv[0][2]=='\0') { /* has white space */
				atob(&argv[1][0], (int*)&loopcount);
				argc--; argv++;
			}
			else {  /* no white space */
				atob(&argv[0][2], (int*)&loopcount);
			}
			if (loopcount < 0) {
				msg_printf(SUM, 
				  "Error. Loop must be > or = to 0\n");
				return (0);
			} break;
		default: bad_arg++; break;
	   }
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
 	   msg_printf(SUM, 
"Usage: -r[RSS#] -p[PP#] -a[RDRAM addr range] -d[data] -l[optional loopcount]\n");
		return (0);
	}	

	rdstart_addr = (__uint32_t)range.ra_base;
	rdend_addr = (__uint32_t)range.ra_base + (range.ra_size*range.ra_count);

#ifdef MGRAS_DIAG_SIM
	rflag = 1; aflag = 1; pflag = 1; dflag = 1; rssnum = 0; ppnum = 1; 
	rdstart_addr = 0x51ffff0; rdend_addr = 0x6000004; data = 0x33114422;

 	/* also checked the following:
	 * rdstart_addr = 0x51ffffc; rdend_addr = 0x6000004; -- cross rdram
	 * rdstart_addr = 0x51ffffc; rdend_addr = 0x51ffffe; -- normal
	 * rdstart_addr = 0x5200000; rdend_addr = 0x6000006; -- invalid start
	 * rdstart_addr = 0x51ffff0; rdend_addr = 0x5500000; -- invalid end
	 */
#endif
	if ((rflag == 0) || (aflag == 0) || (pflag == 0) || (dflag == 0)) {
		msg_printf(SUM, 
		"You must specify RSS#, PP#, RDRAM mem addr range, and data\n");
		return (0);
	}
	if (rdram_checkargs(rssnum, rdstart_addr, ppnum, rflag, 
		pflag, aflag, rdend_addr, MEM))
		return (0);

	/* write 2 bytes (9-bit) for each mem access */
	msg_printf(DBG, 
	   "Writing RSS-%d, PP-%d, RDRAM addresses 0x%x to 0x%x, data:0x%x\n",
			rssnum, ppnum, rdstart_addr, rdend_addr, data);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(CONFIG))),rssnum, TWELVEBIT_MASK);

	while (--loopcount) {
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

		   /* write to 1 rdram at a time, so this for loop covers
		    * the offset within 1 rdram */

		   for (j =(rdstart_addr & TWENTYFOURBIT_MASK); j<= end; j += 2)
		   {
			WRITE_RE_INDIRECT_REG(rssnum, ((ppnum << 28) + 
				(i << 24) + j), data, EIGHTEENBIT_MASK);
#ifdef MGRAS_DIAG_SIM
			data++;
#endif
		   }
		}
		rdstart_addr = (rdstart_addr & 0x7000000) + 0x1000000;
	   }
	}
	return(errors);
}
