/*
 * ldramtest.c
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
/* Architecturally dependent DRAM address/data test:
 *  	- covers 0-16MB on IP30.
 *
 * NOTE: for SR, CPUCTRL0 was switched to HEART_MODE because that's
 *       where the ECC bits are set.
 * 	 low_memtest is called with ldram as a diag cmd.
 *	 low_memtest calls low_dram_test in ldram_diag.s.
 *       what's left to do:
 *		- remove pflag code?		XXX should be ECC diag?
 */
#ident	"ide/godzilla/mem/ldramtest.c:  $Revision: 1.18 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/RACER/IP30addrs.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

/* extern declarations */
extern int low_dram_test(int, __psint_t);
extern int _low_dram_test(int, __psint_t);
extern void	sync_cpu(void);

/* Architecturally dependent DRAM address/data test:
 *	- covers 0-16MB on IP30
 */

int
low_memtest(int argc, char *argv[])
{
	extern unsigned int _hpc3_write1;
	char		*command;
	int		error = 0;
	int		pflag = 0;
	k_machreg_t	old_sr;
	heartreg_t tmp_hr_buf[2] = {HEART_MODE, D_EOLIST};
	heartreg_t tmp_hr_save[2];

	/* parse command */
	command = *argv;
	while ((--argc > 0) && !error) {
		argv++;
		if ((*argv)[0] == '-') {
			switch ((*argv)[1]) {
#ifndef IP30 		/* XXX some sort of ECC diag here? */
			case 'p':		/* check parity bit */
				if ((*argv)[2] != '\0')
					error++;
				else
					pflag = 1;
				break;
#endif
			default:
				error++;
				break;
			}
		} else
			error++;
	}

	if (error) {
		msg_printf(ERR, "usage: %s \n", command);
		return error;
	}

	msg_printf(VRB, "Low DRAM (0-16MB) address/data bit Test\n");

	flush_cache();

        /* save the content of the control register */
	_hr_save_regs(tmp_hr_buf, tmp_hr_save);

        /* enable parity/ECC and error reporting */
        /* speedracer CPU control register is on HEART */
        /*  enable ECC and ECC error reporting (bit 15 and 3) */
        /*  set the memory forced write (??) bit 21 */
/* XXX how much fo this stuff is already done?  at this point */
	d_errors += pio_reg_mod_64(HEART_MODE,
			HM_MEM_ERE | HM_GLOBAL_ECC_EN , SET_MODE);

	old_sr = get_sr();  /* must be changed for IP30 ? XXX */
	error = low_dram_test(pflag, *Reportlevel);
	set_sr(old_sr);

        /* restore the content of the control register */
	_hr_rest_regs(tmp_hr_buf, tmp_hr_save);

	busy(0);		/* restore the LED */

	if (error)
		sum_error("low DRAM (0-16MB) address/data bit");
	else
		okydoky();

	return error;
}

/*  IP30 has a 16MB standalone memory map, but the minimum memory config
 * is much higher (32 or 64MB).  Use a temp buffer @ 16-24MB.
 * We do this in chunks, and only do the very low-level test on
 * the ide chunk.
 *
 * ASSUMPTIONS:
 *	- ide uses between 8MB and 14MB only (6MB).
 *	- 16MB-22MB is free (there is some headroom here).
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
/*
 * Name:        low_dram_test
 * Description: top level test, calling different types of tests
 *			for different chunks of memory.
 * Input:       pflag (N/A) & _Reportlevel
 * Output:      Returns total number of errors.
 */
int
low_dram_test(int pflag, __psint_t _Reportlevel)
{
	void *scratch = (void *)PHYS_TO_K0(IP30_SCRATCH_MEM);
	int i, rc;

	/* XXX use IDE_MASTER if switching cpu is allowed */
	extern int master_cpuid;

	HEART_PIU_K1PTR->h_mode &= ~HM_GP_FLAG(0);
	if (cpuid() != master_cpuid) {
		flush_cache();
		sync_cpu();
		return 0;
	}

	/* Test scratch area.  Will see double coverage with memtest, but
	 * we have an ordering problem if we don't retest it.  Size should
	 * match the max lens size or max ide size (currently 6MB).
	 */
	msg_printf(VRB, "Testing 16-22MB (scratch area)...\n");
	rc = dram(PHYS_TO_K1(IP30_SCRATCH_MEM), 0x600000, pflag);

	/*  Do the hard part of testing memory where ide is loaded.  Handles
	 * own bcopy()'s so it can jump back-n-forth.
	 */
	msg_printf(VRB, "Testing 8-14MB (ide location)...\n");
	rc += _low_dram_test(pflag, _Reportlevel);
	HEART_PIU_K1PTR->h_mode |= HM_GP_FLAG(0);

	set_SR(get_SR()|SR_BEV);	/* for test @ 0 */

	for (i=0; addrs[i]; i++) {
		void *k0_addrs = (void *)K1_TO_K0(addrs[i]);
		msg_printf(VRB, "Testing 0x%x...\n", addrs[i]);
		bcopy(k0_addrs,scratch,lens[i]);
		rc += dram(addrs[i],lens[i],pflag);	/* dram->flush_cache */
		bcopy(scratch,k0_addrs,lens[i]);
		flush_cache();
	}

	HEART_PIU_K1PTR->h_mode &= ~HM_GP_FLAG(0);
	set_SR(get_SR() & ~SR_BEV);

	return rc;
}
