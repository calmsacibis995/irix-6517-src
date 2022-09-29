/*
 * blk.c - 
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
 */
/* NOTE: for IP30, CPUCTRL0 was switched to HEART_MODE because that's
 *       where the ECC bits are set.
 *       what's left to do:  XXX
 *              - delete all non-IP30 code 
 */
#ident "ide/godzilla/mem/blk.c: $Revision 1.1$"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <libsc.h>
#include <libsk.h>
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

#ifdef IP26
#define _scache_linesize	TCC_LINESIZE
#else
extern int _scache_linesize;
#endif

#define INT_EN 0x2000

/***************************************************************************/
/*  Driver module for the blk test.                                         */
/*  The memsetup call parses the incoming arguments and assigns a default  */
/*  address if no range is specified                                       */
/***************************************************************************/



int
blk_test(int argc, char *argv[])
{
	heartreg_t tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t tmp_hr_save[2];
	int		error = 0;
	struct range	range;
	jmp_buf         faultbuf;

	msg_printf(DBG,"Blk test (memsize=%x)\n",cpu_get_memsize());

	/* to prevent write-back since ide is cached */
	flush_cache();

	if  (mem_setup(argc,argv[0],argv[1],&range) != 0) {
		error=-1;
	}
	else {
		msg_printf(VRB, "BLK Test, Base: 0x%x, Size: 0x%x bytes\n",
			   range.ra_base, range.ra_size*range.ra_count);

		if (setjmp(faultbuf)) {
			/*                busy(0);  */
			show_fault();
			return FALSE;
		}
		nofault = faultbuf;

		/* save the content of the control register */
	        _hr_save_regs(tmp_hr_buf, tmp_hr_save);

        	/* enable parity/ECC and error reporting */
        	/* speedracer CPU control register is on HEART */
         	/*  enable ECC and ECC error reporting (bit 15 and 3) */
        	/*  set the memory forced write (??) bit 21 */
		d_errors += pio_reg_mod_64(HEART_MODE,
		     HM_MEM_ERE | HM_GLOBAL_ECC_EN | HM_MEM_FORCE_WR, SET_MODE);
		/* the control register is on MC */
		/* *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = */
		/* (cpuctrl0 | CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) & */
		/*    ~CPUCTRL0_R4K_CHK_PAR_N;	*/

		busy(1);

		blkwrt((u_int *)range.ra_base,
		       (u_int *)range.ra_base+range.ra_count,
		       _scache_linesize,0xaaaaaaaa);

		busy(0);

		blkwrt((u_int *)range.ra_base,
                       (u_int *)range.ra_base+range.ra_count,
		       _scache_linesize,0x55555555);

		busy(1);

		blkrd((u_int *)range.ra_base,
		      (u_int *)range.ra_base+range.ra_count,
		      _scache_linesize);

		busy(0);

        	/* restore the content of the control register */
	        _hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	}
	/* End of if-then for the memsetup condition */

	if (error) {
		sum_error("CPU local memory");
		nofault=0;
		return -1;
	}
	else {
		okydoky();
		nofault=0;
		return 0;
	}
}
