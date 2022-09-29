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
 *  CID/DRAM Diagnostics
 *  $Revision: 1.3 $
 */

#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <uif.h>

extern int f_planes;
extern int f_xywin;
extern int f_rgb;
extern Rexchip *REX;

int test_dram(int argc, char **argv)
{
    int x, y;
    int i, j, k;
    int quad;
    int got;
    int totalerrors = 0;
    int rgb_tmp;
    int planes_tmp;
    int yres;
 
    if (argc > 1)
	yres = 816;
    else
	yres = 768;
#if 0
    int ysize;
    ysize = yres/16;
#endif

    msg_printf(VRB, "Starting CID/DRAM tests...\n");
    rgb_tmp = f_rgb;
    f_rgb = 0;
    planes_tmp = f_planes;
    _planes(CIDPLANES);
    for (i = 0; i < 4; i++) {
	msg_printf(VRB, "Filling DRAM with %#01x...\n", i);
	_color(i);
	_block(0, 0, 1023, yres-1);
	while ((REX->p1.set.configmode) & CHIPBUSY);
	for (x = 0; x < 1024; x += 4)
	    for (y = 0; y < yres; y++) {
		REX->set.xstarti = x;
		REX->set.ystarti = y;
		REX->set.command = REX_LO_SRC | QUADMODE | f_rgb | REX_LDPIXEL;
		quad = REX->go.rwaux1;
		quad = REX->set.rwaux1;
		for (k = 0; k < 4; k++) {
		    got = (quad >> 8*k) & 0xff;
		    if (got != i) {
			msg_printf(ERR, "Error reading from DRAM pixel location %#04d %#03d, returned: %#02x, expected: %#02x\n", x+k, y, got, i);
			totalerrors++;
		    }
		}
	    }
    }
    f_rgb = rgb_tmp;
    _planes(planes_tmp);
    if (totalerrors) {
	msg_printf(SUM,"Total errors in CID/DRAM test: %d\n", totalerrors);
	return -1;
    }
    else {
	okydoky();
	return 0;
    }
}
