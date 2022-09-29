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

/*
 *  XMAP Diagnostics
 *  $Revision: 1.10 $
 */

#include "sys/ng1hw.h"
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include "sys/ng1.h"
#include "sys/xmap9.h"
#include "ide_msg.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern struct ng1_timing_info *ng1_timing[];
extern struct ng1_info ng1_ginfo[];
extern Rex3chip *REX;
extern int REXindex;

#ifndef PROM

/*
 * wxmap9() - write XMAP9 register
 *
 *	This is a tool to allow writing of arbitrary values to the various
 *	XMAP9 registers from the IDE command line.
 */
int ng1wxmap9(int argc, char **argv)
{
	int reg;		/* XMAP9 register offset */
	int data;	  	/* XMAP9 register data */

	if(!ng1checkboard())
		return -1;

	if (argc != 3) {
		printf("usage: %s XMAP9_reg data\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &reg);
	atohex(argv[2], &data);

	msg_printf(DBG,"Writing XMAP9 reg %d with %#08x\n", reg, data);
	xmap9SetReg(REX, (reg << DCB_CRS_SHIFT), data);
	return 0;
}


/*
 * rxmap9() - read XMAP9 register
 *
 *	This is a tool to allow reading of values from the various
 *	XMAP9 registers from the IDE command line.
 */
int ng1rxmap9(int argc, char **argv)
{
	int chip;	/* which xmap, 0 or 1 */
	int reg;	/* XMAP9 register index */
	int data;   	/* XMAP9 register data */

	if(!ng1checkboard())
		return -1;

	if (argc < 3) {
		printf("usage: %s XMAP9_reg 0|1\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &reg);
	atohex(argv[2], &chip);

	xmap9GetReg (REX, (reg << DCB_CRS_SHIFT), chip, data);
	msg_printf(DBG,"Reading XMAP9-%d reg %d returns %#02x\n", chip, reg, data);
	return 0;
}


#endif /* !PROM */

/*
 * Test xmap9 (non-mode) registers 
 */

int test_xmap9_register(unsigned int reg, unsigned int pattern, int chip)
{
	unsigned int got;

	xmap9SetReg (REX, reg, pattern);
	xmap9GetReg (REX, reg, chip, got);

	if (got != pattern) {
		msg_printf(ERR, "Error reading from XMAP9(%d):U%d reg %d returned: %#04x, expected: %#04x\n", chip,(chip & 1) ? 504 : 500, reg, got, pattern);
		return 1;
	}
	return 0;
}

/*
 * Test xmap9 mode registers.
 * Note : vc2 video timing function must be enabled.
 */

int test_xmap9_modes(unsigned int pattern, int chip)
{
	int i;
	unsigned int got;
	int errors = 0;

	for (i = 0; i < 32; i++) {
		xmap9SetModeReg (REX, i, pattern,
				 ng1_timing[REXindex]->cfreq);
		xmap9FIFOWait (REX);
		xmap9GetMode (REX, i, chip, got);
		if (got != pattern) {
			errors++;
			msg_printf(ERR, "Error reading from XMAP9(%d) mode reg %d returned: %#08x, expected: %#08x\n", chip, i, got, pattern);
		}
	}
	return errors;
}

int ng1xmap9test(void)
{
	unsigned int pattern;
	int errors = 0;
	int i;

	if(!ng1checkboard())
		return -1;

#if !PROM
	msg_printf(VRB, "Starting XMAP9 tests...\n");
#endif

	for (i = 0;i < 2; i++) 		/* text xmaps individually */
	for (pattern = 0; pattern <= 0xffffff; pattern += 0x555555) {
		errors += test_xmap9_register(XM9_CRS_MODE_REG_INDEX,
							pattern & 0xff,i);
		errors += test_xmap9_modes(pattern, i);
		errors += test_xmap9_register(XM9_CRS_CURS_CMAP_MSB,
							pattern & 0xff,i);
		errors += test_xmap9_register(XM9_CRS_PUP_CMAP_MSB,
							pattern & 0xff,i);
		errors += test_xmap9_register(XM9_CRS_CONFIG, pattern & 0xf7,i);
	}
	Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);
	if (!errors) {
		msg_printf(INFO, "All of the XMAP9 register r/w tests have passed.\n");
		return 0;
	} else {
		msg_printf(ERR, "Total of %d errors detected in XMAP9 register r/w tests.\n", errors);
		return -1;
	}
}
