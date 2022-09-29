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
 *  REX Diagnostics
 *  $Revision: 1.12 $
 */

#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <libsc.h>
#include <uif.h>

Rexchip *REX = (Rexchip*)PHYS_TO_K1(0x1F3F0000);

#ifndef PROM
int lg1_setboard(int argc, char **argv)
{
        extern Rexchip *rex[];
        int i;

        if (argc < 2) {
                printf("usage: %s 0|1\n", argv[0]);
                return -1;
        }
        atob(argv[1], &i);
        if ( i < 0 || i > 1 ) {
                msg_printf(ERR, "Illegal LG1 board number %d\n",i);
                return 1;
        }
        if (Lg1Probe(rex[i])) {
                REX = rex[i];
                msg_printf(VRB, "LG1 board %d found.\n", i);
                return 1;
        } else {
                msg_printf(VRB, "LG1 board %d not found.\n", i);
                return 0;
        }
}

void rex_clear(void)
{
    while ((REX->p1.set.configmode) & (BFIFO|CHIPBUSY));
    REX->p1.set.xywin = 0x08000800;
    REX->p1.set.configmode = 0x1f << 13 | 0x1f << 18 | 1 << 31;
    REX->p1.set.aux2 = PIXELPLANES;
    REX->p1.set.togglectxt = 0;
    REX->set.aux1 = 0;
    REX->set.rwmask = 0x00ff;
    _color(0);
    _block(0, 0, 1023, 767);
}

/*
 * wrex() - load REX register
 *
 *	This is a tool to allow writing of arbitrary values to the various
 *	REX registers from the IDE command line.
 */
int wrex(int argc, char **argv)
{
    int reg;		/* REX register offset */
    int val;		/* REX register data */
    int loop = 0;

    if (argc < 3) {
	printf("usage: %s [-f] REX_reg_offset data\n", argv[0]);
	return -1;
    }
    argv++;
    if ((*argv)[0] == '-') {
	loop = 1;
	argv++;
    }
    atohex((*argv), &reg);
    argv++;
    atohex((*argv), &val);

    reg += (int)REX;

    printf("Writing location %#08lx with %#08lx\n", reg, val);

    *(int *)reg = val;
    while (loop)
	*(int *)reg = val;
    return 0;
}


/*
 * rrex() - read REX register
 *
 *	This is a tool to allow reading of values from the various
 *	REX registers from the IDE command line.
 */
int rrex(int argc, char **argv)
{
    int reg;				/* REX register offset */
    volatile int tmp;
    int loop = 0;

    if (argc < 2) {
	printf("usage: %s [-f] REX_reg_offset\n", argv[0]);
	return -1;
    }
    argv++;
    if ((*argv)[0] == '-') {
	loop = 1;
	argv++;
    }
    atohex((*argv), &reg);

	reg += (int)REX;

    /* Dummy write before reading aux2 */
    REX->set.aweight = 0;

    printf("Reading from %#08lx returns %#08x\n", reg, *(int *)reg);
    while (loop)
	tmp = *(int *)reg;
    return 0;
}
#endif /* !PROM */


/*
 * REX Register Tests
 *
 *      The following group of tests verifies that the correct data is
 *      being written to the specified register.
 */

int test_rex_register(volatile unsigned long *wreg,
		      volatile unsigned long *rreg, char *name,
		      unsigned long data, unsigned long mask, int shift)
{
    unsigned long expect;
    unsigned long read;

    *wreg = data;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    expect = (data & mask) << shift;
    read = *rreg;
    if (read != expect) {
	msg_printf(ERR, "Error reading from REX register %s, returned: %#08x, expected: %#08x\n", name, read, expect);
	return 1;
    }
    return 0;
}


int test_rex_xstate(unsigned long data)
{
    unsigned long read;

    REX->set.xstate = data;
    REX->go.xstart.word = 0;
    REX->go.xstart.word = 0;
    REX->go.xstart.word = 0;
    read = REX->set.xstate;
    if (read != data) {
	msg_printf(ERR, "Error reading from REX register XSTATE, returned: %#08x, expected: %#08x\n", read, data);
	return 1;
    } else
	return 0;
}

#ifndef PROM
int test_rex_minorslope(unsigned long data)
{
    unsigned long expect;
    unsigned long ms;
    
    REX->set.minorslope.word = data;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    expect = data & 0x8001ffff;
    if (expect & 0x80000000) {
	expect &= 0xfffeffff;
	expect = ~expect + 1;
    }
    expect &= 0x0001ffff;
    ms = REX->set.minorslope.word;
    if (ms != expect) {
	msg_printf(ERR, "Error reading from REX register MINORSLOPE, returned: %#08x, expected: %#08x\n", ms, expect);
	return 1;
    }
    return 0;
}


int test_rex_slopecolor(volatile unsigned long *reg, char *name, unsigned long data)
{
    unsigned long expect;

    *reg = data;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    REX->go.command = REX_NOP;
    expect = data & 0x801fffff;
    if (expect & 0x80000000) {
	expect &= 0xffefffff;
	expect = ~expect + 1;
    }
    expect = ((expect & 0x00080000) << 1) | (expect & 0x001fffff);
    if (*reg != expect) {
	msg_printf(ERR, "Error reading from REX register %s, returned: %#08x, expected: %#08x\n", name, *reg, expect);
	return 1;
    }
    return 0;
}


int test_rex(void)
{
    unsigned long pattern;
    int errors = 0;
    int i;

    msg_printf(VRB, "Starting REX tests...\n");
    
    for (i = 0; i < 4; i++) {
	pattern = i*0x55555555;
	errors += test_rex_register(&REX->set.aux1, &REX->set.aux1,
				    "AUX1", pattern, 0x000003ff, 0);
	errors += test_rex_xstate(pattern);

	errors += test_rex_register(&REX->set.xstarti, &REX->set.xstart.word,
				    "XSTARTI", pattern, 0x00000fff, 15);
	errors += test_rex_register(&REX->set.xstartf.word, &REX->set.xstart.word,
				    "XSTARTF", pattern, 0x007fffff, 4);
	errors += test_rex_register(&REX->set.xstart.word, &REX->set.xstart.word,
				    "XSTART", pattern, 0x07ffffff, 0);
	errors += test_rex_register(&REX->set.xendf.word, &REX->set.xendf.word,
				    "XENDF", pattern, 0x007ff800, 0);
	errors += test_rex_register(&REX->set.ystarti, &REX->set.ystart.word,
				    "YSTARTI", pattern, 0x00000fff, 15);
	errors += test_rex_register(&REX->set.ystartf.word, &REX->set.ystart.word,
				    "YSTARTF", pattern, 0x007fffff, 4);
	errors += test_rex_register(&REX->set.ystart.word, &REX->set.ystart.word,
				    "YSTART", pattern, 0x07ffffff, 0);
	errors += test_rex_register(&REX->set.yendf.word, &REX->set.yendf.word,
				    "YENDF", pattern, 0x007ff800, 0);
	errors += test_rex_register(&REX->set.xsave, &REX->set.xsave,
				    "XSAVE", pattern, 0x00000fff, 0);
	errors += test_rex_minorslope(pattern);
	errors += test_rex_register(&REX->set.xymove, &REX->set.xymove,
				    "XYMOVE", pattern, 0x07ff07ff, 0);
	errors += test_rex_register(&REX->set.colorredi, &REX->set.colorredf.word,
				    "COLORREDI", pattern, 0x000000ff, 11);
	errors += test_rex_register(&REX->set.colorredf.word, &REX->set.colorredf.word,
				    "COLORRED", pattern, 0x000fffff, 0);
	errors += test_rex_register(&REX->set.colorgreeni, &REX->set.colorgreenf.word,
				    "COLORGREENI", pattern, 0x000000ff, 11);
	errors += test_rex_register(&REX->set.colorgreenf.word,
				    &REX->set.colorgreenf.word,
				    "COLORGREEN", pattern, 0x000fffff, 0);
	errors += test_rex_register(&REX->set.colorbluei, &REX->set.colorbluef.word,
				    "COLORBLUEI", pattern, 0x000000ff, 11);
	errors += test_rex_register(&REX->set.colorbluef.word,
				    &REX->set.colorbluef.word,
				    "COLORBLUE", pattern, 0x000fffff, 0);
	errors += test_rex_slopecolor(&REX->set.slopered.word,
				      "SLOPERED", pattern);
	errors += test_rex_slopecolor(&REX->set.slopegreen.word,
				      "SLOPEGREEN", pattern);
	errors += test_rex_slopecolor(&REX->set.slopeblue.word,
				      "SLOPEBLUE", pattern);
	errors += test_rex_register(&REX->set.colorback, &REX->set.colorback,
				    "COLORBACK", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->set.zpattern, &REX->set.zpattern,
				    "ZPATTERN", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.lspattern, &REX->set.lspattern,
				    "LSPATTERN", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.lsmode, &REX->set.lsmode,
				    "LSMODE", pattern, 0x000fffff, 0);
	errors += test_rex_register(&REX->set.aweight, &REX->set.aweight,
				    "AWEIGHT", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.rwaux1, &REX->set.rwaux1,
				    "RWAUX1", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.rwaux2, &REX->set.rwaux2,
				    "RWAUX2", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.rwmask, &REX->set.rwmask,
				    "RWMASK", pattern, 0x0000ffff, 0);
	errors += test_rex_register(&REX->set.smask1x, &REX->set.smask1x,
				    "SMASK1X", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->set.smask1y, &REX->set.smask1y,
				    "SMASK1Y", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->set.xendi, &REX->set.xendf.word,
				    "XENDI", pattern, 0x00000fff, 11);
	errors += test_rex_register(&REX->set.yendi, &REX->set.yendf.word,
				    "YENDI", pattern, 0x00000fff, 11);
	errors += test_rex_register(&REX->p1.set.smask2x,
				    &REX->p1.set.smask2x,
				    "SMASK2X", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.smask2y,
				    &REX->p1.set.smask2y,
				    "SMASK2Y", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.smask3x,
				    &REX->p1.set.smask3x,
				    "SMASK3X", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.smask3y,
				    &REX->p1.set.smask3y,
				    "SMASK3Y", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.smask4x,
				    &REX->p1.set.smask4x,
				    "SMASK4X", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.smask4y,
				    &REX->p1.set.smask4y,
				    "SMASK4Y", pattern, 0x03ff03ff, 0);
	errors += test_rex_register(&REX->p1.set.aux2,
				    &REX->p1.set.aux2,
				    "AUX2", pattern, 0x7fffffff, 0);
	errors += test_rex_register(&REX->p1.set.wclock,
				    &REX->p1.set.wclock,
				    "WCLOCK", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.rwdac,
				    &REX->p1.set.rwdac,
				    "RWDAC", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.configsel,
				    &REX->p1.set.configsel,
				    "CONFIGSEL", pattern, 0x00000007, 0);
	errors += test_rex_register(&REX->p1.set.rwvc1,
				    &REX->p1.set.rwvc1,
				    "RWVC1", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.xywin,
				    &REX->p1.set.xywin,
				    "XYWIN", pattern, 0x0fff0fff, 0);
    }
    if (!errors) {
	okydoky();
	rex_clear();
	return 0;
    } else {
	msg_printf(ERR, "Total of %d errors detected in REX register r/w tests.\n", errors);
	rex_clear();
	return -1;
    }
}
#endif /* !PROM */


#ifdef PROM
/* Preliminary power on test */

int lg1_pon(void)
{
    unsigned long pattern;
    int errors = 0;
    int i;
    
    /* REX test */
    for (i = 1; i < 3; i++) {
	pattern = i*0x55555555;
	errors += test_rex_register(&REX->p1.set.rwvc1, &REX->p1.set.rwvc1,
				    "RWVC1", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.configsel,
				    &REX->p1.set.configsel,
				    "CONFIGSEL", pattern, 0x00000007, 0);
	errors += test_rex_register(&REX->p1.set.rwdac,
				    &REX->p1.set.rwdac,
				    "RWDAC", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.wclock,
				    &REX->p1.set.wclock,
				    "WCLOCK", pattern, 0x000000ff, 0);
	errors += test_rex_register(&REX->p1.set.xywin,
				    &REX->p1.set.xywin,
				    "XYWIN", pattern, 0x0fff0fff, 0);
	errors += test_rex_register(&REX->set.rwaux2, &REX->set.rwaux2,
				    "RWAUX2", pattern, 0xffffffff, 0);
	errors += test_rex_register(&REX->set.aux1, &REX->set.aux1,
				    "AUX1", pattern, 0x000003ff, 0);
	errors += test_rex_xstate(pattern);
	errors += test_rex_register(&REX->set.xstarti, &REX->set.xstart.word,
				    "XSTARTI", pattern, 0x00000fff, 15);
	errors += test_rex_register(&REX->set.ystarti, &REX->set.ystart.word,
				    "YSTARTI", pattern, 0x00000fff, 15);
	errors += test_rex_register(&REX->set.xendi, &REX->set.xendf.word,
				    "XENDI", pattern, 0x00000fff, 11);
	errors += test_rex_register(&REX->set.yendi, &REX->set.yendf.word,
				    "YENDI", pattern, 0x00000fff, 11);
	errors += test_rex_register(&REX->set.colorredi, &REX->set.colorredf.word,
				    "COLORREDI", pattern, 0x000000ff, 11);
	errors += test_rex_register(&REX->set.zpattern, &REX->set.zpattern,
				    "ZPATTERN", pattern, 0xffffffff, 0);
    }
    if (errors) {
	msg_printf(VRB, "REX power on test failed.\n");
	return -1;
    }

    /* VC1 */
    for (i = 1; i < 3; i++) {
	pattern = i*0x5555;
	errors += test_vc1_register(VC1_VID_TIMING, VID_EP, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_EP, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_XL, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_YL, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_MODE, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_BX, pattern);
	errors += test_vc1_register(VC1_CURSOR_CTRL, CUR_LY, pattern);
	errors += test_vc1_register(VC1_DID_CTRL, DID_EP, pattern);
	errors += test_vc1_register(VC1_DID_CTRL, DID_END_EP, pattern);
	errors += test_vc1_register(VC1_DID_CTRL, DID_HOR_DIV, pattern);
	errors += test_vc1_register(VC1_DID_CTRL, DID_HOR_MOD, pattern);
	errors += test_vc1_register(VC1_XMAP_CTRL, BLKOUT_EVEN, pattern);
	errors += test_vc1_register(VC1_XMAP_CTRL, BLKOUT_ODD, pattern);
	errors += test_vc1_register(VC1_XMAP_CTRL, AUXVIDEO_MAP, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_0_E, pattern);
    }
    if (errors) {
	msg_printf(VRB, "VC1 power on test failed.\n");
	return -1;
    }

    /* Brooktree */
    for (i = 1; i < 3; i++) {
	pattern = i*0x55;
	errors += test_palette(0, pattern);
    }
    if (errors) {
	msg_printf(VRB, "Bt479 power on test failed.\n");
	return -1;
    }
    return 0;
}
#endif /* PROM */
