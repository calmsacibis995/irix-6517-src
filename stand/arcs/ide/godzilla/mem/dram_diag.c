/*
 * dram_diag.c - architecturally dependent DRAM address/data/parity test
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
 * algorithms: associate with each 4-bits in the DRAM module is a
 *	unique 18-23 bits address. this test writes the address/address
 *	complement of each bit into itself, 4 bits at a time, then
 *	reads back and verifies. 
 *	this test guarantees any 2 bits within a DRAM module
 *	will take on different values some time during the test*
 *
 *	* we don't walk a one across each 4 bits group since it's covered by
 *	  the walking bit test
 *
 * NOTE: for IP30, CPUCTRL0 was switched to HEART_MODE because that's
 *       where the ECC bits are set.
 * 	 The -p option (parity) was removed.
 */
#ident	"ide/godzilla/mem/dram_diag.c:  $Revision: 1.12 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "setjmp.h"
#include "libsc.h"
#include "libsk.h"
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"


/* architecturally dependent DRAM address/data/parity test */	
int 
dram_diagnostics(int argc, char *argv[])
{
	char			*command = *argv;
	int			error = 0;
	int			pflag = 0;
	struct range		range;
	char			*rstr = argv[1];

	flush_cache();

	if (mem_setup(argc,command,rstr,&range) != 0) {
		error = 1;
	}

	if (error) {
		msg_printf(ERR, "Usage: %s [RANGE]\n", command);
		return error;
	}

	msg_printf(VRB,"DRAM address/data bit test\n");

	error = dram(range.ra_base, range.ra_count, pflag);

	if (error)
		sum_error("DRAM address/data bit");
	else
		okydoky();

	return error;
}

int
dram(__psunsigned_t _addrlo, __psunsigned_t count, int pflag)
{
	__uint64_t addrhi_offset;
	unsigned int one, zero, value, tmp;
	unsigned int *addrhi = (unsigned int *)(_addrlo+count);
	unsigned int *addrlo = (unsigned int *)_addrlo;
	int complement, bit, error=0, busy_count=0;
	volatile unsigned int *addr;
	jmp_buf faultbuf;
	heartreg_t  tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t  tmp_hr_save[2];

        /* save the content of the control/mode register */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

        /* enable parity/ECC and error reporting */
        /* speedracer CPU control register is on HEART */
        /*  enable ECC and ECC error reporting (bit 15 and 3) */
        /*  set the memory forced write (??) bit 21 */
	d_errors += pio_reg_mod_64(HEART_MODE,
		HM_MEM_ERE | HM_GLOBAL_ECC_EN , SET_MODE);

	addrhi_offset = KDM_TO_PHYS((__psunsigned_t)addrhi - PHYS_RAMBASE);

	busy(0);
	for (complement = 0; complement <= 1; complement++) {
		if (pflag) {
			one = complement ? 0x00000000 : 0x01010101;
			zero = complement ? 0x01010101 : 0x00000000;
		} else {
			one = complement ? 0x00000000 : 0xffffffff;
			zero = complement ? 0xffffffff : 0x00000000;
	  	}

	     	/*
		 * since we are using 32 bits wide SIMMS and each bank has 4
		 * SIMMS, address lines to the SIMMs start at bit 4 
		 */
	  	for (bit = 0x10; bit < addrhi_offset; bit <<= 1) {
	     		for (addr = addrlo; addr < addrhi; addr++) {
				*addr = (((__psint_t)addr & bit) ? one : zero);
				if (!busy_count--) {
					busy_count = BUSY_COUNT;
					busy(1);
				}
			}

			/* read and verify */
			nofault = faultbuf;
			addr = addrlo;

			/*
			 * this setup allows the test to continue after an
			 * exception
			 */
			if (setjmp(faultbuf)) {
				show_fault();
				nofault = faultbuf;
				error++;
				addr++;
			}
			else {
				for (; addr < addrhi; addr++) {
					value = (((__psint_t)addr & bit) ? one : zero);
					if ((tmp=*addr) != value) {
						error++;
						memerr((caddr_t)addr, value,
							tmp, sizeof(u_int));
					}

					if (!busy_count--) {
						busy_count = BUSY_COUNT;
						busy(1);
					}
				}
			}
		}
	}

	nofault = 0;
	
        /* restore the content of the control register */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	busy(0);

	return error;
}
