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
 *  $Revision: 1.3 $
 */

#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <uif.h>

extern Rexchip *REX;

#define VC1SetByte(data) \
    REX->p1.set.rwvc1 = data; \
    REX->p1.go.rwvc1 = data

#define VC1GetByte(data) \
    data = REX->p1.go.rwvc1; \
    data = REX->p1.set.rwvc1

#define VC1SetWord(data) \
	    VC1SetByte((data) >> 8 & 0xff); \
	    VC1SetByte((data) & 0xff)

#define VC1GetWord(data) { \
	    unsigned vc1_get_word_tmp; \
	    VC1GetByte(vc1_get_word_tmp); \
	    VC1GetByte(data); \
	    data = ((vc1_get_word_tmp << 8) & 0xff00) | (data & 0xff); \
}

#define VC1SetAddr(addr, configaddr) \
	    REX->p1.go.configsel = VC1_ADDR_HIGH; \
	    VC1SetByte((addr) >> 8 & 0xff); \
	    REX->p1.go.configsel = VC1_ADDR_LOW; \
	    VC1SetByte((addr) & 0xff); \
	    REX->p1.go.configsel = configaddr

int lg1_test_sram(int argc, char **argv)
{
	unsigned long i, j, a;
	unsigned long expect;
	int errors = 0;

	msg_printf(VRB, "Starting VC1 SRAM tests...\n");

	vc1_off();

	for (i = 0; i < 4; i++) {
		expect = 0x5555 * i;
		msg_printf(VRB, "Filling SRAM with %#04x...\n", expect);
		VC1SetAddr(0x0, VC1_EXT_MEMORY);
		for (a = 0; a < 0x1000; a++) {
			VC1SetWord(expect);
		}
		VC1SetAddr(0x0, VC1_EXT_MEMORY);
		for (a = 0; a < 0x1000; a++) {
			VC1GetWord(j);
			if (expect != j) {
				msg_printf(ERR, "Error reading from SRAM address %#04x, returned: %#04x, expected: %#04x\n", a, j, expect);
				errors++;
			}
		}
	}

	msg_printf(VRB, "SRAM address uniqueness test...\n");
	VC1SetAddr(0x0, VC1_EXT_MEMORY);
	for (a = 0; a < 0x1000; a++) {
		VC1SetWord(a);
	}
	VC1SetAddr(0x0, VC1_EXT_MEMORY);
	for (a = 0; a < 0x1000; a++) {
		VC1GetWord(j);
		if (a != j) {
			msg_printf(ERR, "Error reading from SRAM address %#04x, returned: %#04x, expected: %#04x\n", a, j, a);
			errors++;
		}
	}

	vc1_on();
	if (!errors) {
		okydoky();
		return 0;
	} else {
		msg_printf(ERR, "Total of %d errors detected in VC1 SRAM r/w tests.\n", errors);
		return -1;
	}
}


