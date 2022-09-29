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
 *  DAC Diagnostics
 *  $Revision: 1.9 $
 */

#include <sys/lg1hw.h>
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/lg1.h>
#include "ide_msg.h"
#include <uif.h>
#include <libsc.h>

extern Rexchip *REX;
extern struct lg1_info lg1_ginfo;
#define sync (lg1_ginfo.monitortype != 6)

#define btSet(addr, val) \
    REX->p1.set.configsel = addr; \
    REX->p1.set.rwdac = val; \
    REX->p1.go.rwdac = val;

#define btGet(addr, val) \
    REX->p1.set.configsel = addr; \
    val = REX->p1.go.rwdac; \
    val = REX->p1.set.rwdac;


#ifndef PROM
int wbt(int argc, char **argv)
{
    int ctrl, data;

    if (argc < 3) {
	printf("usage: %s element data\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &ctrl);
    atohex(argv[2], &data);

    printf("Writing Bt479 element %#01x with %#02x\n", ctrl, data);
    btSet(ctrl, data);
    return 0;
}


int rbt(int argc, char **argv)
{
    int ctrl, data;

    if (argc < 2) {
	printf("usage: %s element\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &ctrl);
    btGet(ctrl, data);
    printf("Reading Bt479 element %#01x returns %#02x\n", ctrl, data);
    return 0;
}

#endif /* !PROM */
int test_palette(int mapno, int data)
{
    int i;
    unsigned char r[256], g[256], b[256];
    int totalerrors = 0;
    volatile int dummy;

    if (lg1_ginfo.boardrev < 2) {                   /* LG1 */
	    mapno = (mapno << 4) | 0xf;
	    btSet(WRITE_ADDR, 0x82);
    } else {                                        /* LG2 */
	    mapno = (mapno << 6) | sync | 0x2;
    }
    btSet(CONTROL, mapno);
    btSet(WRITE_ADDR, 0x00);
    REX->p1.set.configsel = PALETTE_RAM;
    for (i = 0; i < 256; i++) {
	REX->p1.set.rwdac = data;
	REX->p1.go.rwdac = data;
	REX->p1.set.rwdac = data;
	REX->p1.go.rwdac = data;
	REX->p1.set.rwdac = data;
	REX->p1.go.rwdac = data;
    }

    if (lg1_ginfo.boardrev < 2)  {	   		/* LG1 */
	    btSet(WRITE_ADDR, 0x82);
    }
    btSet(CONTROL, mapno);
    btSet(READ_ADDR, 0x00);
    REX->p1.set.configsel = PALETTE_RAM;
    dummy = REX->p1.go.rwdac;
    for (i = 0; i < 256; i++) {
	r[i] = REX->p1.set.rwdac;
	r[i] = REX->p1.go.rwdac;
	g[i] = REX->p1.set.rwdac;
	g[i] = REX->p1.go.rwdac;
	b[i] = REX->p1.set.rwdac;
	b[i] = REX->p1.go.rwdac;
    }
    for (i = 0; i < 256; i++) {
	if (r[i] != data) {
	    msg_printf(ERR, "Error reading from (red) DAC address 0x00%02x, returned: %#02x, expected: %#02x\n", i, r[i], data);
	    totalerrors++;
	}
	if (g[i] != data) {
	    msg_printf(ERR, "Error reading from (green) DAC address 0x00%02x, returned: %#02x, expected: %#02x\n", i, g[i], data);
	    totalerrors++;
	}
	if (b[i] != data) {
	    msg_printf(ERR, "Error reading from (blue) DAC address 0x00%02x, returned: %#02x, expected: %#02x\n", i, b[i], data);
	    totalerrors++;
	}
    }
    return totalerrors;
}

#ifndef PROM
int btdactest()
{
    int i;
    int totalerrors = 0;
    int mapno;

    msg_printf(VRB, "Starting DAC tests ...\n");
    for (i = 0; i < 0x100; i += 0x55) {
	msg_printf(VRB, "Filling palette ram with %#02x...\n", i);
	for (mapno = 0; mapno < 4; mapno++)
	    totalerrors += test_palette(mapno, i);
    }
    if (totalerrors) {
	msg_printf(SUM,"Total errors in DAC test: %d\n", totalerrors);
	return -1;
    }
    else {
	okydoky();
	return 0;
    }
}
#endif /* !PROM */
