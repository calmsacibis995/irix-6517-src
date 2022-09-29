/*
 * lkhtest.c - Architecturally dependent DRAM address/data/parity test
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
 *
 * NOTE: for IP30,
 *           what needs to be done for IP30:  XXX
		- low_kh_test ??? 
 */
#ident	"ide/godzilla/mem/lkhtest.c:  $Revision: 1.19 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/RACER/IP30addrs.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

extern int low_kh_test(int, __psint_t);
extern int _low_kh_test(int, __psint_t);
extern void sync_cpu(void);

/* Architecturally dependent DRAM address/data/parity test:
 *	- covers 0-16MB on IP30.
 */
/*ARGSUSED*/
int
khlow_drv(int argc, char *argv[])
{
	int		error = 0;
	int		pflag = 0;
	k_machreg_t	old_sr;
	heartreg_t tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t tmp_hr_save[2];

	msg_printf(VRB, "Low KH Test\n");

	/* save all readable/writable H registers that are written to */
        /* (at least the control register) */
        _hr_save_regs(tmp_hr_buf, tmp_hr_save);

	/* enable parity/ECC and error reporting */
	/* speedracer memory control register is on HEART */
	/*  enable ECC and ECC error reporting (bit 15 and 3) */
	/*  set the memory forced write (XXX) bit 21 */
	d_errors += pio_reg_mod_64(HEART_MODE,
		HM_MEM_ERE | HM_GLOBAL_ECC_EN | HM_MEM_FORCE_WR, SET_MODE);

	old_sr = get_sr();
	error = low_kh_test(pflag,*Reportlevel);
	set_sr(old_sr);

	/* restore the content of the modified heart registers */
        _hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	busy(0);		/* restore the LED */

	if (error)
		sum_error("low KH test");
	else
		okydoky();

	return error;
}

/*  IP30 has a 16MB standalone memory map.  To avoid needing 32MB to complete
 * ldram test we do this in chunks, and only do the very low-level test on
 * the ide chunk.
 *
 * ASSUMPTIONS:
 *	- ide uses between 8MB and 14MB only (6MB).
 *	- 16MB-22MB is free.
 */
static __psunsigned_t addrs[] = {
	PHYS_TO_K1(0x00000000), PHYS_TO_K1_RAM(SYSTEM_MEMORY_ALIAS_SIZE),
	PHYS_TO_K1_RAM(0x00400000), PHYS_TO_K1_RAM(0x00e00000),
	0
};
static int lens[] = {
	SYSTEM_MEMORY_ALIAS_SIZE, 0x400000-SYSTEM_MEMORY_ALIAS_SIZE,
	0x400000, 0x200000
};
int
low_kh_test(int pflag, __psint_t _Reportlevel)
{
	void *scratch = (void *)PHYS_TO_K1_RAM(IP30_SCRATCH_MEM);
	void set_BEV(void *);
	int i,rc = 0;

	/* XXX use IDE_MASTER if switching cpu is allowed */
	extern int master_cpuid;

	HEART_PIU_K1PTR->h_mode &= ~HM_GP_FLAG(0);
	if (cpuid() != master_cpuid) {
		flush_cache();
		sync_cpu();
		return 0;
	}

	/* scratch area is tested elsewhere on IP30! */

	/*  Do the hard part of testing memory where ide is loaded.  Handles
	 * own bcopy()'s so it can jump back-n-forth.
	 */
	msg_printf(VRB,"Testing 8-14MB (ide location)\n");
	rc += _low_kh_test(pflag,_Reportlevel);
	HEART_PIU_K1PTR->h_mode |= HM_GP_FLAG(0);

	set_SR(get_SR() | SR_BEV);

	scratch = (void *)PHYS_TO_K1_RAM(0x01000000);

	for (i=0; addrs[i]; i++) {
		msg_printf(VRB, "Testing 0x%x...\n", addrs[i]);
		msg_printf(DBG," bcopy to scratch area %016x size = %08x\n",
			   scratch,lens[i]);
		bcopy((void *)addrs[i],scratch,lens[i]);
		if (Kh_l(addrs[i], addrs[i]+lens[i]-1) == 0)
			rc++;
		if (Altaccess_l(addrs[i], addrs[i]+lens[i]-1) == 0)
			rc++;
		bcopy(scratch,(void *)addrs[i],lens[i]);
	}

	HEART_PIU_K1PTR->h_mode &= ~HM_GP_FLAG(0);
	set_SR(get_SR() & ~SR_BEV);

	return rc;
}
