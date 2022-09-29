/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1991, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  SRAM Diagnostics
 *  $Revision: 1.5 $
 */

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include "sys/ng1hw.h"
#include "sys/vc2.h"
#include "sys/ng1.h"
#include "sys/ng1_cmap.h"
#include "sys/xmap9.h"
#include "ide_msg.h"
#include "libsk.h"
#include "libsc.h"
#include "local_ng1.h"
#include "uif.h"

extern struct ng1_info ng1_ginfo[];
extern Rex3chip *REX;
extern int REXindex;

#define VC2_RAMWORDS VC2_EXTERNAL_RAM_NBYTES/2

int ng1_test_sram(int argc, char **argv)
{
	unsigned int i, j, a;
	unsigned int expect;
	int errors = 0;

	msg_printf(VRB, "Starting VC2 SRAM tests...\n");

	vc2_off();

	for (expect = 0; expect <= 0xffff; expect += 0x5555) {
		msg_printf(VRB, "Filling SRAM with %#04x...\n", expect);
		vc2SetupRamAddr(REX, 0x0);
		for (a = 0; a < VC2_RAMWORDS; a++) {
			vc2SetRam( REX, expect);
		}
		vc2SetupRamAddr( REX, 0x0);
		for (a = 0; a < VC2_RAMWORDS; a++) {
			vc2GetRam( REX, j);
			if (expect != j) {
				msg_printf(ERR, "Error reading from SRAM address %#04x, returned: %#04x, expected: %#04x\n", a, j, expect);
				errors++;
			}
		}
	}

	msg_printf(VRB, "SRAM address uniqueness test...\n");
	vc2SetupRamAddr( REX, 0x0);
	for (a = 0; a < VC2_RAMWORDS; a++) {
		vc2SetRam( REX, a);
	}
	vc2SetupRamAddr( REX, 0x0);
	for (a = 0; a < VC2_RAMWORDS; a++) {
		vc2GetRam( REX, j);
		if (a != j) {
			msg_printf(ERR, "Error reading from SRAM address %#04x, returned: %#04x, expected: %#04x\n", a, j, a);
			errors++;
		}
	}

        Ng1RegisterInit( REX, &ng1_ginfo[REXindex]);

	if (!errors) {
		msg_printf(INFO, "All of the VC2 SRAM r/w tests have passed.\n");
		return 0;
	} else {
		msg_printf(ERR, "Total of %d errors detected in VC2 SRAM r/w tests.\n", errors);
		return -1;
	}
}


