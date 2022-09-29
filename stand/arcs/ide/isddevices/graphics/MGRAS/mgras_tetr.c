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
#include "uif.h"
#include "libsc.h"
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

extern __uint32_t check_re_revision(__uint32_t);


/**************************************************************************
 *                                                                        *
 *  read_TERev								  *
 *                                                                        *
 *  	Read the TE1 revision register of 1 TE1                           *
 *                                                                        *
 **************************************************************************/

read_TERev(__uint32_t rssnum)
{
	__uint32_t status= 0, errors = 0, version, id, expected_vs, count = 0;

	expected_vs = 1;

	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
			rssnum, TWELVEBIT_MASK);

	HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(TEVERSION))), 
			TWENTYNINEBIT_MASK, status, CSIM_RSS_DEFAULT);

	version = status & TE_VERSION_BITS;
	id = status & TE_ID_BITS;		
	id >>= 25; 
	count = status & TE_COUNT_BITS;
	count >>= 23;

#ifdef MGRAS_DIAG_SIM
	version = 0;
	id = rssnum;
#endif

	msg_printf(DBG,"RSS-%d, TE Version Reg is 0x%x\n", rssnum, status);

	if (version > expected_vs) {
		msg_printf(ERR,"RSS-%d, TE Version is 0x%x, exp 0x0 or 0x1\n",
				rssnum, version);
		msg_printf(SUM,"TE Version is bits [3..0] of the RE-TE bus.\n");
		errors++;
	}
	if (id != rssnum) {
		msg_printf(ERR,"RSS-%d, TE ID is 0x%x, expected 0x%x\n",
			rssnum, id, rssnum);
		msg_printf(SUM,"TE ID is bits [26..25] of the RE-TE bus.\n"); 
		errors++;
	}
	if (count != (numRE4s - 1) ) {
		msg_printf(ERR,"RSS-%d, TE Count is 0x%x, expected 0x%x\n",
			rssnum, count , (numRE4s -1));
		msg_printf(SUM,"TE Count is bits [24..23] of the RE-TE bus.\n");
		errors++;
	}
	return (errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_TERev -r[optional RSS_Number]	-l[optional loopcount]	 	  *
 *                                                                        *
 *  	Read the TE1 revision register of all or specified TE1s           *
 *									  * 
 * 	RSS_Number is optional and if specified, only the TE1 in that	  *
 *	RSS will be tested. If no RSS_Number is specified, all TEs will   *
 * 	be checked.							  *
 *                                                                        *
 **************************************************************************/

mgras_TERev(__int32_t argc, char ** argv)
{
	__uint32_t errors = 0, status, rssnum, rflag = 0, bad_arg=0;
	__uint32_t regnum = 0, gflag = 0, tot_errs = 0;
	__int32_t loopcount = 1, i, j;

	NUMBER_OF_RE4S();

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test...\n");
		return (0);
	}

	/* initialize to test everything */
        rssstart = 0; rssend = numRE4s;

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
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

        if ((bad_arg) || (argc)) {
		msg_printf(SUM, 
		   "Usage: -r[optional RSS number] -l[optional loopcount]\n");
		return (0);
	}

	if (re_checkargs(rssnum, regnum, rflag, gflag))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
          for (j = (rssend-1); j >= 0; j--) {
		if (check_re_revision(j)) {
	  	   	errors =+ read_TERev(j);
			if (errors) {
				msg_printf(ERR, "TE-%d REV test FAILED\n", j);
				REPORT_PASS_OR_FAIL_NR((&TE_err[TE1_REV_TEST]), errors);
			}
			else {
				msg_printf(VRB, "TE-%d REV test PASSED\n", j);
				REPORT_PASS_OR_FAIL_NR((&TE_err[TE1_REV_TEST]), errors);
			}
            		tot_errs += errors;

            		errors = 0;
            		msg_printf(DBG, "j=%d\n", j);
		}
		else {
			msg_printf(SUM, "This revision of RE-%d cannot read TE registers.\n", j);
			msg_printf(SUM, "Skipping this test.\n");
		}
          }
        }

	/* reset config register */
        HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);

  	RETURN_STATUS(tot_errs);
}

#if 0
/* the following read and write tests are now obsolete. Replaced by
 * Write/Read RSSReg() routines in mgras_re4.c
 */
/**************************************************************************
 *                                                                        *
 *  mgras_WriteTEReg -r[RSS #] -g[regnum] -d[data] -m[mask] -p -e[data2]  *
 *                                                                        *
 *      Write the specified reg						  *
 *                                                                        *
 *      -p = do a double register write. -p is optional                   *
 *      -e = the upper 32 bits of data in a 64-bit (double) reg write. It *
 *              only is used if the -p switch is used.                    *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_WriteTEReg(int argc, char ** argv)
{
        __uint32_t data, regnum, rssnum, mask, status, errors = 0;
	__uint32_t rflag = 0, gflag = 0, dflag = 0, mflag = 0, bad_arg = 0;
	__uint32_t dbl = 0, eflag = 0, data2 = 0xdeadcafe;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();

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
			case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
			case 'd':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&data);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&data);
                                dflag++; break;
			case 'e':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&data2);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&data2);
                                eflag++; break;
			case 'p':
				dbl = 1; break;
			case 'm':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&mask);
                                mflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
        rflag = 1; gflag = 1, mflag = 1; dflag = 1;
        rssnum = 0; regnum = 0x80; /* 0x182, SW_U = 0x80 */
        mask = TWENTYSIXBIT_MASK, data = 0x44332211;
	dbl = 1; eflag = 1; data2 = 0x11223344;
#endif
        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
            "Usage: -r[RSS #] -g[regnum] -d[data] -m[mask]\n");
		return (0);
	}

	if ((rflag == 0) || (gflag == 0) || (dflag == 0) || (mflag == 0)) {
                msg_printf(SUM,"You must specify RSS#, reg#, data, and mask\n");                return (0);
        }

	if ((dbl) && (eflag == 0)) {
                msg_printf(SUM,"You must specify the 2nd data with the -e flag since you specified -p\n");
                return (0);
        }

	if (re_checkargs(rssnum, regnum, rflag, gflag))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	   rdwr_RSSReg(rssnum, regnum, data, mask, WRITE, 0, dbl,data2,2,0);
	}

	return(0);
}

/* the following read and write tests are now obsolete. Replaced by
 * Write/Read RSSReg() routines in mgras_re4.c
 */
/**************************************************************************
 *                                                                        *
 *  mgras_ReadTEReg -r[RSS #] -g[regnum] -m[mask] -p                      *
 *                                                                        *
 *      Read the specified reg                                            *
 *                                                                        *
 *	-p = double reg read						  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_ReadTEReg(int argc, char **argv)
{
        __uint32_t dbl = 0, regnum, rssnum, mask, bad_arg = 0;
	__uint32_t status, rflag = 0, gflag = 0, mflag = 0, errors=0;
	__int32_t loopcount = 1;

	NUMBER_OF_RE4S();

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
			case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
			case 'm':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&mask);
                                mflag++; break;
			case 'p':
				dbl = 1; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					   "Error. Loop must be > or = to 0\n");
					return (0);
				}
				break;
                        default: bad_arg++; break;
                }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
        rflag = 1; gflag = 1, mflag = 1;
        rssnum = 0; regnum = SW_U; /* 0x80 */
        mask = TWENTYEIGHTBIT_MASK;
#endif


        if ((bad_arg) || (argc)) {
           msg_printf(SUM,
            "Usage: -r[RSS #] -g[regnum] -m[mask]\n");
		return (0);
	}

	if ((rflag == 0) || (gflag == 0) || (mflag == 0)) {
                msg_printf(SUM,"You must specify RSS#, reg#, and mask\n");                return (0);
        }

	if (re_checkargs(rssnum, regnum, rflag, gflag))
		return (0);

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	 rdwr_RSSReg(rssnum, regnum, CSIM_RSS_DEFAULT, mask, READ, 0,dbl,0,0,0);
	}

	return(0);
	
}

/* NOT USED. WE ARE USING mgras_RE4RdWrRegs WITH A -T SWITCH INSTEAD */
/**************************************************************************
 *                                                                        *
 *  mgras_TERdWrRegs -r[optional RSS_Number] -g[optional regnum] 	  *
 *			-m[optional mask] -x [1 | 0]		  	  *
 *                                                                        *
 *  	Write/Read marching 1/0 to readable regs               		  *
 *                                                                        *
 * 	RSS_Number is optional and if specified, only the TE1 in that	  *
 *	RSS will be tested. If no RSS_Number is specified, all TEs will   *
 * 	be checked.							  *
 *									  *
 * 	regnum is also optional and specifies which register is to be	  *
 * 	tested. regnum must appear with a RSS_Number, but RSS_Number can  *
 *	be specified without regnum - in that case, all TE registers 	  *
 *	associated with the specified RSS_Number RSS will be tested. 	  *
 *									  *
 *	mask specifies the bit mask for the register			  *
 *									  *
 * 	-x = stop_on_err, =1 is true					  *
 **************************************************************************/

mgras_TERdWrRegs(__int32_t argc, char ** argv)
{
	__uint32_t errors=0, mask, rssnum, regnum, status;
	__uint32_t i, j, stop_on_err = 1, is_re4 = 0;
	__uint32_t rflag = 0, gflag = 0, mflag = 0, bad_arg = 0; 
	__int32_t loopcount = 1;
	char regname[30];

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test...\n");
		return (0);
	}

	NUMBER_OF_RE4S();
	
	/* initialize to test everything */
        rssstart = 0; rssend = numRE4s;

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
                        case 'g':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&regnum);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&regnum);
                                gflag++; break;
                        case 'm':
				if (argv[0][2]=='\0') {
                                	atob(&argv[1][0], (int*)&mask);
        				argc--; argv++;
				}
				else
                                	atob(&argv[0][2], (int*)&mask);
                                mflag++; break;
			case 'l':
				if (argv[0][2]=='\0') { /* has white space */
					atob(&argv[1][0], (int*)&loopcount);
					argc--; argv++;
				}
				else { /* no white space */
					atob(&argv[0][2], (int*)&loopcount);
				}
				if (loopcount < 0) {
					msg_printf(SUM, 
					   "Error. Loop must be > or = to 0\n");
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

        if ((bad_arg) || (argc)) {
	   msg_printf(SUM, 
	     "Usage: -r[optional RSS#] -g[optional reg#] -m[optional mask] -x[1|0] (optional stop-on-error, default = 1)\n\n");
		return (0);
	}

	if (re_checkargs(rssnum, regnum, rflag, gflag))
		return (0);

	if (gflag ^ mflag) {
	  msg_printf(SUM, "Register and mask go together.\n");
 	  msg_printf(SUM, "If you specify one, you must specify the other.\n");
	  return (0);
	}

#ifdef MGRAS_DIAG_SIM
	stop_on_err = 0;
#endif

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
	  for (rssnum = rssstart; rssnum < rssend; rssnum++) {
	   	if (gflag) { /* test only 1 reg per rss */
        	   for (i = 0; i < 120; i++)
		   {
                	if (te_rwreg[i].regNum == regnum) {
                       		strcpy(regname, te_rwreg[i].str);
				break;
			}
                   }
		   errors += test_RSSRdWrReg(rssnum,regname, regnum, mask, 
				stop_on_err, is_re4);
		   REPORT_PASS_OR_FAIL(
	   		(&TE_err[TE1_RDWRREGS_TEST +rssnum*LAST_TE_ERRCODE]),
			errors);
	   	}
	   	else { /* test all regs per rss */
	      	   j = 0;
	      	   while (te_rwreg[j].regNum != 0x999) {
	   	       	errors += test_RSSRdWrReg(i,
			  te_rwreg[j].str,te_rwreg[j].regNum, te_rwreg[j].mask,
					stop_on_err, is_re4);
		       	j++;
			if (errors && stop_on_err) {
			  REPORT_PASS_OR_FAIL(
	   		   (&TE_err[TE1_RDWRREGS_TEST +rssnum*LAST_TE_ERRCODE]),
				errors);
			}
	      	   }
	   	}
	     	REPORT_PASS_OR_FAIL_NR(
	   	   (&TE_err[TE1_RDWRREGS_TEST +rssnum*LAST_TE_ERRCODE]),errors);
	  }
	}
	RETURN_STATUS(errors);
}	
#endif
