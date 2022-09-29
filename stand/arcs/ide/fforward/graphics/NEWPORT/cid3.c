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
 *  $Revision: 1.15 $
 */

#include "sys/ng1hw.h"
#include "sys/xmap9.h"
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include "sys/ng1.h"
#include <sys/param.h>
#include "ide_msg.h"
#include "gl/rex3macros.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern Rex3chip *REX;
extern int REXindex;

/*extern __psint_t *Reportlevel;*/
extern int dm1;
extern struct ng1_info ng1_ginfo[];

static void cid_checkbits(unsigned int expect, unsigned int got, int bank_no);
int ng1_testdram(char *testname, int draw_dm1, int mask);

int ng1cid(int argc, char **argv)
{
	int wmask;
	int errors_detected;

	if(!ng1checkboard())
		return -1;

	REX3WAIT (REX);
	wmask = REX->set.wrmask;

	msg_printf(VRB, "Starting CID planes test...\n");
	ng1_writemask (0x33);
	errors_detected = ng1_testdram("CID", DM1_CIDPLANES | DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 |
	    DM1_COLORCOMPARE | DM1_RWPACKED | DM1_ENPREFETCH | DM1_LO_SRC, 3);

	if (!errors_detected) {
		msg_printf(VRB, "Starting PUP planes test...\n");
		ng1_writemask (0xcc);
		errors_detected = ng1_testdram("PUP", DM1_PUPPLANES | DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 |
	    	DM1_COLORCOMPARE | DM1_RWPACKED | DM1_ENPREFETCH | DM1_LO_SRC, 3);
		}

	if (!errors_detected) {
		if (ng1_ginfo[REXindex].bitplanes == 24) {
			msg_printf(VRB, "Starting OLAY planes test...\n");
			ng1_writemask (0xffff00);
			ng1_testdram("OLAY", DM1_OLAYPLANES | DM1_DRAWDEPTH8 |
			DM1_COLORCOMPARE |DM1_HOSTDEPTH8 | DM1_RWPACKED |
			DM1_LO_SRC , 0xff);
			}	
		ng1_writemask (wmask);
	 	}
}

int ng1_testdram(char *testname, int draw_dm1, int mask)
{
	int x, y;
	int i, j, k;
	int quad;
	int got;
	int totalerrors = 0;
	/*int ysize = (NG1_YSIZE / 16);  never used*/
	int tmp_dm1;
	int color;
	int bank_no=0;
	int stop_at_50;

	stop_at_50 = console_is_gfx();

	msg_printf(VRB, "Testing %s planes .....\n", testname);		

	tmp_dm1 = dm1;
	dm1 = draw_dm1; 

/*
 * XXX Add more tests - walking ones, galloping ones etc
 */
	color = 0x55;
	for (i = 3; i >= 0 && totalerrors < 50; i--) {
		color = 0x55 * i;
		color &= mask;

		/* On newport board there is no DRAM. We use VRAM as DRAM. */
		msg_printf(VRB, "Filling VRAM with %#01x...\n", color);
		ng1_color(color);
		ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);

                REX->set.drawmode1 = dm1;
                REX->set.drawmode0 = DM0_READ | DM0_BLOCK |
                                                DM0_DOSETUP | DM0_COLORHOST;
		REX3WAIT(REX);
		/* Dummy read for Prefetch */
		REX->go.hostrw0;
		for (y = 0; y < NG1_YSIZE; y++) { 
			for (x = 0; x < NG1_XSIZE; x += 4) {
				REX->set.xystarti = (x << 16) | y;
                                REX->set.xyendi = ((x+3) << 16) | y;
				quad = REX->go.hostrw0;
                                for (k = 0; k < 4; k++) {
                                    got = quad & mask;
                                    if (got != color) {
					if(*Reportlevel > 4)	
						bank_no=check_bank(x+3-k, y);
                                        msg_printf(VRB, "Error reading from VRAM (%s) location %#04d %#03d, returned: %#02x, expected: %#02x\n",
                                            testname, x+3-k, y, got, color);
                                            totalerrors++;
					if(*Reportlevel > 4) {
                                		print_bank(bank_no);
						if (!strcmp("OLAY", testname))
							cid_checkbits(color, got, bank_no);
						else
                                			print_chip(1,bank_no); /* Always lower chip for CID & PUP */
                        		}
                                        totalerrors++;
					if (stop_at_50 && totalerrors > 50) {
						y=NG1_YSIZE;
						x=NG1_XSIZE;
						}	
				    }
				    quad = quad >> 8;
				}
				if(!(y % 200))
					busy(1);
			}
		}
	}

	color = 0;
	/* Walking 1 test */
	for (i = 0; i < 3 && totalerrors < 50; i++) {
		color = 0x1 << i;
		color &= mask;
		msg_printf(VRB, "Filling VRAM with %#01x...\n", color);
		ng1_color(color);
		ng1_block(0, 0, NG1_XSIZE-1, NG1_YSIZE-1, 0, 0);

                REX->set.drawmode1 = dm1;
                REX->set.drawmode0 = DM0_READ | DM0_BLOCK |
                                                DM0_DOSETUP | DM0_COLORHOST;
		REX3WAIT(REX);
		/* Dummy read for Prefetch */
		REX->go.hostrw0;
		for (y = 0; y < NG1_YSIZE; y++) { 
			for (x = 0; x < NG1_XSIZE; x += 4) {
				REX->set.xystarti = (x << 16) | y;
                                REX->set.xyendi = ((x+3) << 16) | y;
				quad = REX->go.hostrw0;

                                for (k = 0; k < 4; k++) {
                                    got = quad & mask;
                                    if (got != color) {
					if(*Reportlevel > 4)
						bank_no=check_bank(x+3-k, y);
                                        msg_printf(VRB, "Error reading from VRAM (%s) location %#04d %#03d, returned: %#02x, expected: %#02x\n",
                                            testname, x+3-k, y, got, color);
                                            totalerrors++;
					if(*Reportlevel > 4) {
                                                print_bank(bank_no);
						if (!strcmp("OLAY", testname))
							cid_checkbits(color, got, bank_no);
						else
							print_chip(1,bank_no); /* Always lower chip for CID & PUP */

					}
                                        totalerrors++;
					if (stop_at_50 && totalerrors > 50) {
						y=NG1_YSIZE;
						x=NG1_XSIZE;
						}	
				    }
				    quad = quad >> 8;
				}
				if(!(y % 200))
					busy(1);
			}
		}
	}
	busy(0);
	dm1 = tmp_dm1;

	if (totalerrors) {
		msg_printf(SUM,"Total errors in %s planes test: %d\n", 
				testname, totalerrors);
		return -1;
	}
	else {
		msg_printf(INFO, "All of the %s planes test have passed.\n", testname);
		return 0;
	}
}

static void
cid_checkbits(unsigned int expect, unsigned int got, int bank_no)
{
	int i;
	unsigned int result;
	int chip_no=0;

	result = expect ^ got;
	for(i=0; i<8; i++) {
		if((result >> i) & 0x0001) {
			if (i <= 3)
				chip_no = 2;
			else if ((i > 3) && (i <= 7)) 
				chip_no = 3;
			print_chip(chip_no, bank_no);
		}
	}
}
