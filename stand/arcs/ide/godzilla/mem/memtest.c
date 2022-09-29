/*
 * memtest.c - Driver module for the generic test.
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
 *  The memsetup call parses the incoming arguments and assigns a default
 *  address if no range is specified Generic_l is the actual test code.
 * 
 * NOTE: 
 *	 memtest2 (here) was renamed memtest (the original memtest does not 
 *		exist anymore)
 */
#ident "ide/godzilla/mem/memtest.c:  $Revision: 1.28 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "libsk.h"
#include "libsc.h"
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Name:        memory_test
 * Description: Tests On board RAM (above Low RAM (first 16MB in IP26)
 * Input:       [test number] [rangestring]  (if no number, then addruniq 
 *		 and walking bit are run; if no range is given then the 
 *		 whole memory >16MB is tested)              
 *		NOTE: the range affects only the "low-range". 
 * Output:      Returns 0 if no error, -1 if error
 * Error Handling: the memerr msg is displayed (from memutil)
 * Side Effects: modifies the contents of On board RAM                       
 * Remarks:	was called memtest2 for IP22, IP26 etc....
 * Debug Status: debugged for  IP22, IP26 etc.. not debugged for IP30
 */
int
memory_test(int argc, char *argv[])
{
	unsigned long count;
	heartreg_t tmp_hr_buf[2] = {HEART_MODE, D_EOLIST}; 
	heartreg_t tmp_hr_save[2];
	int error = 0;
	struct range range;
	jmp_buf	faultbuf;
	int  memtestnum;
	__psunsigned_t end;

	d_errors = 0;	/* initialize diag error counter */

	msg_printf(VRB, "From shortest to longest:\n");
	msg_printf(VRB, "--AddrUniq=0, Kh=1, FastTest=2, MarchY=3, MarchX=4,\n");
	msg_printf(VRB, "--Mats=5, Butterfly=6, AltAccess=7, March=8, WalkingBit=9\n");
	msg_printf(VRB, "--no argument = Addruniq & WalkingBit\n");
	memtestnum = atoi(argv[1]);
	if (memtestnum > MAX_MEMTESTNUM) 
		{
		printf("Incorrect selection of tests, try again\n");
		error = -1;
		return (1);
		}

	/* to prevent write-back since ide is cached */
	flush_cache();

	/* argv[0] is memtest; argv[1] is test number; argv[2]=rangestring */
	/* NOTE: range string is used only on range */
	/* mem_setup derives the values of range */
	if (mem_setup(argc-1, argv[0], argc == 3 ? argv[2] : "", &range) != 0)
		  return(-1);
	
	count = range.ra_count*range.ra_size;

	if (setjmp(faultbuf)) {
		show_fault();
		return -1;
	}

	nofault = faultbuf;

	/* save all readable/writable H registers that are written to */ 
        /* (at least the control register) */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save); 

	/* enable parity/ECC and error reporting */
	/* speedracer memory control register is on HEART */
        /*  enable ECC and ECC error reporting (bit 15 and 3) */
        /*  do NOT set the memory forced write bit 21: */
	/*	one is not trying to correct memory errors */
        d_errors += pio_reg_mod_64(HEART_MODE, 
				HM_MEM_ERE | HM_GLOBAL_ECC_EN , SET_MODE);

	/* Note the -1 due to Mips code; the boundary condition is different */
	msg_printf(DBG,"count+range.ra_base=%x\n", range.ra_base+count);

	/* without any argument, run AddrUniq and WalkingBit tests */
	if (argc == 1) {
		msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count); 
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		msg_printf(VRB, "Walking Bit Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		goto done;
	}

	switch (memtestnum) {
	case 0:
		msg_printf(VRB,"Address Uniqueness Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		break;
	case 1:
		msg_printf(VRB,"Kh Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Kh_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 2:
		msg_printf(VRB,"Fast memory Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (Fastmemtest_l(range.ra_base,(range.ra_base + count - 0x1), 
			memerr))
			error = -1;
		break;
	case 3:
		msg_printf(VRB,"March Y Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchY(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 4:
		msg_printf(VRB,"March X Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (marchX(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 5:
		msg_printf(VRB,"Mats Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Mats_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 6:
		msg_printf(VRB,"Butterfly Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (Butterfly_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 7:
		msg_printf(VRB,"Altaccess Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Altaccess_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 8:
		msg_printf(VRB,"March Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!March_l(range.ra_base,(range.ra_base + count - 0x1)))
			error = -1;
		break;
	case 9:
		msg_printf(VRB,"Walking Bit Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
		break;

	/* the MovI test takes a long time, so the code is here, */
	/*  but it won't be run (too high a test number) */
	case 10:
		msg_printf(VRB,"Movi Test, Base: 0x%x, Size: 0x%x bytes\n",
			range.ra_base, count);
		if (!Movi_l(range.ra_base, (range.ra_base + count - 0x1 )))
			error = -1;
		break;
	case 11:
		msg_printf(VRB,"Patterns Test, Base: 0x%x, "
			"Size: 0x%x bytes\n", range.ra_base, count);
		if (Patterns_test_l(range.ra_base,(range.ra_base + count - 0x1), 
			memerr))
			error = -1;
		break;
	default:
		printf("Incorrect selection of tests, try again\n");
		error = -1;
		break;
	}

done:
        /* restore the content of the modified heart registers */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	nofault=0;

	/* top level function must report the pass/fail status (was okydoky)*/
	if (error) d_errors = 1;
#ifdef NOTNOW
        REPORT_PASS_OR_FAIL(d_errors, "Memory ", D_FRU_MEM);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_MEM_0000], d_errors );

	/* the FRU number for memory failures is not enough. */
	/* the SIMM/BX numbers are given by getbadsimm/memutil.c */
}
