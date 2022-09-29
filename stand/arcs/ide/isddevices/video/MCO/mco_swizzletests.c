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
 *  MCO Input Swizzle Tests, Output Swizzle Tests, Line FIFO Tests, and
 *  Field Memory Tests
 *
 *  $Revision: 1.11 $
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include <math.h>
#include "sys/mgrashw.h"
#include "ide_msg.h"
#include "mco_diag.h"

extern mgras_hw *mgbase;
#ifdef IP30
extern void sr_mco_set_dcbctrl(mgras_hw *base, int crs);
#endif 	/* IP30 */
extern void mco_set_dcbctrl(mgras_hw *base, int crs);

#ifdef CHRIS
int mco_precomp[6][8][8] = {
	{{0x0F0, 0x09F, 0x00D, 0x0AE, 0x0DF, 0x017, 0x085, 0x000},
	 {0x06B, 0x063, 0x0E3, 0x09F, 0x03E, 0x07C, 0x02C, 0x0F7},
	 {0x052, 0x0FF, 0x067, 0x03E, 0x0BE, 0x047, 0x0ED, 0x0D6},
	 {0x039, 0x02A, 0x0B7, 0x080, 0x039, 0x02E, 0x061, 0x0C2},
	 {0x006, 0x0C3, 0x1A5, 0x039, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}},

	{{0x0A3, 0x020, 0x0F6, 0x0FD, 0x01A, 0x0DB, 0x02A, 0x0AA},
	 {0x09D, 0x0CF, 0x025, 0x0D1, 0x00A, 0x047, 0x0BC, 0x0C5},
	 {0x01B, 0x091, 0x0B0, 0x08A, 0x0DB, 0x051, 0x076, 0x08D},
	 {0x042, 0x0C8, 0x0F2, 0x09F, 0x0B7, 0x0AF, 0x033, 0x03C},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x006, 0x0C3, 0x1A5, 0x039, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}},

	{{0x037, 0x030, 0x09C, 0x08A, 0x045, 0x0C6, 0x024, 0x025},
	 {0x04A, 0x0EF, 0x03A, 0x0D2, 0x064, 0x0FE, 0x008, 0x0FB},
	 {0x047, 0x052, 0x010, 0x065, 0x0C6, 0x0D3, 0x09D, 0x06B},
	 {0x0F4, 0x0E1, 0x094, 0x04E, 0x01E, 0x02F, 0x017, 0x009},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x006, 0x0C3, 0x1A5, 0x039, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}},

	{{0x037, 0x096, 0x01B, 0x0F1, 0x094, 0x0B1, 0x053, 0x0E2},
	 {0x062, 0x07D, 0x037, 0x00C, 0x0A0, 0x058, 0x07C, 0x0C8},
	 {0x07E, 0x033, 0x099, 0x077, 0x0EA, 0x063, 0x0A1, 0x058},
	 {0x05A, 0x078, 0x051, 0x09C, 0x00D, 0x0A1, 0x09C, 0x0E0},
	 {0x13D, 0x08D, 0x04A, 0x1CD, 0x000, 0x000, 0x000, 0x000},
	 {0x075, 0x186, 0x1DA, 0x124, 0x000, 0x000, 0x000, 0x000},
	 {0x1BA, 0x121, 0x176, 0x0CA, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}},

	{{0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}},

	{{0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA},
	 {0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA},
	 {0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA},
	 {0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA, 0x0A0, 0x0CA},
	 {0x1EC, 0x0BF, 0x136, 0x0F2, 0x000, 0x000, 0x000, 0x000},
	 {0x1EC, 0x0BF, 0x136, 0x0F2, 0x000, 0x000, 0x000, 0x000},
	 {0x1EC, 0x0BF, 0x136, 0x0F2, 0x000, 0x000, 0x000, 0x000},
	 {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000}}
};	
#endif /*  CHRIS */

int mco_precomp[8][8][8] = {
	{{0x00D, 0x098, 0x0B0, 0x0E1, 0x025, 0x07F, 0x0BE, 0x085},
	 {0x09F, 0x01D, 0x04C, 0x09D, 0x08F, 0x015, 0x02B, 0x068},
	 {0x0CA, 0x0E8, 0x09D, 0x0F2, 0x017, 0x030, 0x042, 0x014},
	 {0x0AB, 0x0CD, 0x0D4, 0x0E3, 0x053, 0x02F, 0x0DE, 0x0E4},
	 {0x1F0, 0x049, 0x0E2, 0x1EA, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* Solid Black */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 0 -d 1600 1200 */
	{{0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0, 0x0D0},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x000, 0x000, 0x000, 0x000}},

	/* Solid Red */
	/* mg0_clear_color -c 256 -r 255 -g 0 -b 0 -a 0 -d 1600 1200 */
	{{0x031, 0x031, 0x031, 0x031, 0x031, 0x031, 0x031, 0x031},
	 {0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E},
	 {0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E},
	 {0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E, 0x03E},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* Solid Green */
	/* mg0_clear_color -c 256 -r 0 -g 255 -b 0 -a 0 -d 1600 1200 */
	{{0x013, 0x013, 0x013, 0x013, 0x013, 0x013, 0x013, 0x013},
	 {0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D},
	 {0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D},
	 {0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D, 0x07D},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* Solid Blue */
	/* mg0_clear_color -c 256 -r 0 -g 0 -b 255 -a 0 -d 1600 1200 */
	{{0x057, 0x057, 0x057, 0x057, 0x057, 0x057, 0x057, 0x057},
	 {0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B},
	 {0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B},
	 {0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B, 0x08B},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x1FD, 0x1FD, 0x1FD, 0x1FD, 0x000, 0x000, 0x000, 0x000},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* Solid White */
	/* mg0_clear_color -c 256 -r 255 -g 255 -b 255 -a 0 -d 1600 1200 */
	{{0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075},
	 {0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8},
	 {0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8},
	 {0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8, 0x0C8},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x009, 0x009, 0x009, 0x009, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* R=0xAA, G=0xAA, B=0xAA */
	/* mg0_clear_color -c 256 -r 170 -g 170 -b 170 -a 0 -d 1600 1200 */
	{{0x02B, 0x010, 0x02B, 0x010, 0x02B, 0x010, 0x02B, 0x010},
	 {0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075},
	 {0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075},
	 {0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075, 0x075},
	 {0x141, 0x0BB, 0x15E, 0x0BB, 0x000, 0x000, 0x000, 0x000},
	 {0x141, 0x0BB, 0x15E, 0x0BB, 0x000, 0x000, 0x000, 0x000},
	 {0x141, 0x0BB, 0x15E, 0x0BB, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}},

	/* R=0x55, G=0x55, B=0x55 */
	/* mg0_clear_color -c 256 -r 85 -g 85 -b 85 -a 0 -d 1600 1200 */
	{{0x015, 0x0b7, 0x015, 0x0b7, 0x015, 0x0b7, 0x015, 0x0b7},
	 {0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D},
	 {0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D},
	 {0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D, 0x06D},
	 {0x1D4, 0x00E, 0x17D, 0x00E, 0x000, 0x000, 0x000, 0x000},
	 {0x1D4, 0x00E, 0x17D, 0x00E, 0x000, 0x000, 0x000, 0x000},
	 {0x1D4, 0x00E, 0x17D, 0x00E, 0x000, 0x000, 0x000, 0x000},
	 {0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x000, 0x000, 0x000, 0x000}}
};	

int *mco_four_screen_precomp(int screenpat) {
   return((int *)&(mco_precomp[screenpat-1]));
}

static struct VOFtable mcoVOFmodelist[] = {
	{"vga",		(OS_VGA | IS_VGA) },
	{"mini",	(OS_MINI | IS_MINI) },
	{"field",	(OS_FIELD | IS_FIELD) },
	{"",	0},
};

#define TEST_RED 	3
#define TEST_BLUE 	4
#define TEST_GREEN	5

/*
 * mco_OSChecksumTest() --
 */

int 
_mco_chksum(int mco_mode,	/* mco_mode can be one of: 	*/
				/* 	(OS_VGA|IS_VGA),	*/
				/*	(OS_MINI|IS_MINI), or	*/
				/*	(OS_FIELD|IS_FIELD) */
	    int *cmpvals, 	/* Pointer to array of expected chksum values */
	    int testnum	) 	/* test pattern is to be checked */
{
    int i,j,k,l;
    unsigned int retvals[8][8];
    int err;
    int osref[4] = {592, 49, 48, 593};
#ifdef IP30
    int mode_mask, mask;

    mask = 0;
    mode_mask = 0xC0;

    /* We want both IS and OS, so set mask and mode_mask accordingly */
    mask |= 0x7;	/* OS */
    mode_mask &= ~0x40;	/* OS */

    mask |= (0x7 << 3);	/* IS */
    mode_mask &= ~0x80;	/* IS */

#endif	/* IP30 */

    err = 0;

    /* Reset the DIAG state machine */
#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */
    MCO_SETINDEX(mgbase, MCO_CONTROL1, 0);

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 3);
    MCO_IO_WRITE(mgbase, (0 | (mco_mode & 0x38) | mode_mask));
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 3);
    MCO_IO_WRITE(mgbase, (0 | (mco_mode & 0x38)));
#endif	/* IP30 */

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */
    MCO_SETINDEX(mgbase, 0xB0, 0);   /* XXX Where did this address come from? */

    j = 0;
    do {
#ifdef IP30
	sr_mco_set_dcbctrl(mgbase, 3);
#else	/* IP30 */
	mco_set_dcbctrl(mgbase, 3);
#endif	/* IP30 */
	MCO_IO_READ(mgbase, i);
	j++;
	us_delay(25000);	/* 25 milliseconds */
    } while ((j < 20) && ((i & 0xF7) != 0xF7));

    if ((i & 0xF0) != 0xF0) {
	msg_printf(VRB,"ERROR: mco_chksum: OS never preset? (%d)\n", i);
	err = 1;
    }

    if ((i & 0x07) != 0x07) {
	msg_printf(VRB,"ERROR: mco_chksum: IS never preset? (%d)\n", i);
	err = 1;
    }

    /* Release the reset and wait for a response */

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */
    MCO_SETINDEX(mgbase, MCO_CONTROL1, 0);

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 3);
    MCO_IO_WRITE(mgbase, ((1+8 | (mco_mode & 0x38) | mode_mask)));
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 3);
    MCO_IO_WRITE(mgbase, (1+8 | (mco_mode & 0x38)));
#endif	/* IP30 */

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */
    MCO_SETINDEX(mgbase, 0xB0, 0);

    j = 0;
    do {
#ifdef IP30
	sr_mco_set_dcbctrl(mgbase, 3);
#else	/* IP30 */
	mco_set_dcbctrl(mgbase, 3);
#endif	/* IP30 */
	MCO_IO_READ(mgbase, i);
	j++;
	us_delay(25000);	/* 25 milliseconds */
    } while ((j < 30) && (i & 0xF7));

#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 3);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 3);
#endif	/* IP30 */
    MCO_IO_READ(mgbase, i);

    if (i & 0xF0) {
	msg_printf(VRB,"ERROR: mco_chksum:  OS never cleared? (%d)\n", i);
	err = 1;
    }

    if (i & 0x07) {
	msg_printf(VRB,"ERROR: IS never cleared? (%d)\n", i);
	err = 1;
    }

    for (i=0; i<8; i++) {
	for (j=0; j<8; j++) {
	    retvals[j][i] = 0;
	}
    }

    /* Now try to read 8*8 = 64 bits... */
    for (j=0; j<72; j++) {

	/* Strobe the shift register... */
#ifdef IP30
	sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
	mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */
	MCO_SETINDEX(mgbase, MCO_CONTROL1, 0);

#ifdef IP30
	sr_mco_set_dcbctrl(mgbase, 3);
	MCO_IO_WRITE(mgbase, ((3+24 & mask) | (mco_mode & 0x38) | mode_mask));
#else	/* IP30 */
	mco_set_dcbctrl(mgbase, 3);
	MCO_IO_WRITE(mgbase, (3+24 | (mco_mode & 0x38)));
#endif	/* IP30 */

#ifdef IP30
	sr_mco_set_dcbctrl(mgbase, 3);
	MCO_IO_WRITE(mgbase, ((1+8 & mask) | (mco_mode & 0x38) | mode_mask));
#else	/* IP30 */
	mco_set_dcbctrl(mgbase, 3);
	MCO_IO_WRITE(mgbase, (1+8 | (mco_mode & 0x38)));
#endif	/* IP30 */

	/* Read the result... */

	mco_set_dcbctrl(mgbase, 2);
	MCO_SETINDEX(mgbase, 0xB0, 0);

	mco_set_dcbctrl(mgbase, 3);
	MCO_IO_READ(mgbase, i);

	if (j < 64) {
	    k = 7 - (j % 8);

	    /* Output Swizzles... */
	    retvals[0][(int)(j/8)] |= (i & 16) ? (1 << k) : 0;
	    retvals[1][(int)(j/8)] |= (i & 32) ? (1 << k) : 0;
	    retvals[2][(int)(j/8)] |= (i & 64) ? (1 << k) : 0;
	    retvals[3][(int)(j/8)] |= (i & 128) ? (1 << k) : 0;
	}

	if (j < 36) {
	    l = 8 - (j % 9);

	    /* Input Swizzles... */
	    retvals[4][(int)(j/9)] |= (i & 1) ? (1 << l) : 0; /* RED? */
	    retvals[5][(int)(j/9)] |= (i & 2) ? (1 << l) : 0; /* GRE? */
	    retvals[6][(int)(j/9)] |= (i & 4) ? (1 << l) : 0; /* BLU? */
	    retvals[7][(int)(j/9)] |= (i & 8) ? (1 << l) : 0; /* DCB? */
	}
	if (j > 64) {
	    if (i & 0xF7) {
	         msg_printf(VRB,"ERROR: More than 64 bits?  Transmission error? (%d, %d)\n", j, i);
	         err = 1;
	    }
	}
    }

    if (testnum == 99) {
	msg_printf(VRB,"----- Output Swizzle -----\n");
	msg_printf(VRB,"               0,1  2,3  4,5  6,7\n");
	for (i=7; i>=0; i--) {
	    switch (i) {
		case 7: msg_printf(VRB,"Upper Left A:  "); break;
		case 6: msg_printf(VRB,"Upper Left B:  "); break;
		case 5: msg_printf(VRB,"Upper Right A: "); break;
		case 4: msg_printf(VRB,"Upper Right B: "); break;
		case 3: msg_printf(VRB,"Lower Left A:  "); break;
		case 2: msg_printf(VRB,"Lower Left B:  "); break;
		case 1: msg_printf(VRB,"Lower Right A: "); break;
		case 0: msg_printf(VRB,"Lower Right B: "); break;
	    }

	    for (j=0; j<4; j++) {
	        msg_printf(VRB,"0x%02X ", (int)(retvals[j][i]));
	    }
	    msg_printf(VRB,"\n");
	}
	msg_printf(VRB,"\n");
	msg_printf(VRB,"----- Input Swizzle -----\n");
	msg_printf(VRB,"Red   Green Blue  Unused\n");
	for (i=7; i>=0; i--) {
	    for (j=4; j<8; j++) {
	        msg_printf(VRB,"0x%03X ", (int)(retvals[j][i]));
	    }
	    msg_printf(VRB,"\n");
	}
    }
    else {
	for (i=0; i<8; i++) {
	    for (j=4; j<7; j++) {
		if (retvals[j][i] != *(cmpvals + j*8 + i)) {
		    msg_printf(VRB,"\n");
		    switch (j) {
			case 4: 
				msg_printf(VRB,"Input Swizzle Red (U16): "); 
				break;
			case 5: 
				msg_printf(VRB,"Input Swizzle Green (U31): "); 
				break;
			case 6: 
				msg_printf(VRB,"Input Swizzle Blue (U583): "); 
				break;
		    }
		    msg_printf(VRB,"Error: Exp 0x%2X  Rcv 0x%2X Diff 0x%2X", 
			*(cmpvals + j*8 + i), retvals[j][i], 
			(*(cmpvals+ j*8 + i)^retvals[j][i]));
		    err = 2;
		}
	    }
	    if (err != 2) {
		for (j=0; j<4; j++) {
		    if (retvals[j][i] != *(cmpvals + j*8 + i)) {
			msg_printf(VRB,"\n");
			switch (i) {
			    case 7: msg_printf(VRB,"Upper Left A: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U45");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U36");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U34");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 6: msg_printf(VRB,"Upper Left B: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U596");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U52");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U598");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 5: msg_printf(VRB,"Upper Right A: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U37");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U35");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U42");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 4: msg_printf(VRB,"Upper Right B: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U595");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U44");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U589");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 3: msg_printf(VRB,"Lower Left A: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U587");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U579");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U581");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 2: msg_printf(VRB,"Lower Left B: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U53");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U597");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U51");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 1: msg_printf(VRB,"Lower Right A: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U578");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U580");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U590");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			    case 0: msg_printf(VRB,"Lower Right B: ");
				if (testnum == TEST_RED) {
					msg_printf(VRB,"FLDMEM RED U54");
				}
				if (testnum == TEST_GREEN) {
					msg_printf(VRB,"FLDMEM GRE U588");
				}
				if (testnum == TEST_BLUE) {
					msg_printf(VRB,"FLDMEM BLU U43");
				}
				msg_printf(VRB," OS U%d, bits %d,%d ", osref[j], 2*j, 2*j+1); 
				break;
			}
			msg_printf(VRB,"Error: Exp 0x%2X  Rcv 0x%2X Diff 0x%2X", 
			*(cmpvals + j*8 + i), retvals[j][i], 
			(*(cmpvals + j*8 + i)^retvals[j][i]));
			err = 1;
		    }
		}
	    }
	}
	for (i=0; i<8; i++) {
	    for (j=0; j<8; j++) {
		msg_printf(DBG, "cmpvals[%d][%d] = 0x%3X\tretvals[%d][%d] =x 0x%3X\n",
		j, i, *(cmpvals + j*8 + i), j, i, (int)(retvals[j][i]));
	    }
	}
    }

    if (err) {
	msg_printf(VRB,"\n");
    }

    return(err);
}

/* mco_OSChksumTest -- 
 *	Requirements:  This test assumes the MCO VOF is set up already
 * 	    and that the test pattern is written into the MGRAS framebuffer
 *
 *	usage: mco_OSChksumTest mode testpattern#
 *	
 *	where mode is one of: vga mini field
 *	and testpattern# is a number from 1 to 6 
 *
 *	Test description:    This test reads the Output Swizzle chksums
 *		and compares them with the pre-computed OS checksum values.
 *		If the values don't match, it attempts to determine likely
 *		failing chips.
 */


int 
mco_OSChksumTest(int argc, char **argv)
{
	 __uint32_t mco_mode;
	int VOFmodefound = 0;
	int i, j, err=0;
	char VOFmodename[80];
	char tmpstr[80], badstr[80];
	int pattern = 0;

	if (argc != 3) {
	    msg_printf(SUM, "usage: %s mode patrn_number\n", argv[0]);
		return -1;
	}

        argv++; argc--;
	sprintf(tmpstr,"%s", argv[0]);
	for (j=0; (!VOFmodefound && mcoVOFmodelist[j].name[0]); j++) {
	    if(strcmp(mcoVOFmodelist[j].name, tmpstr) == 0) {
		sprintf(VOFmodename,"%s", tmpstr);
		mco_mode = mcoVOFmodelist[j].TimingToken;
		msg_printf(DBG,"mco_OSChksumTest: MCO mode: %s\tValue = 0x%x\n", VOFmodename, mco_mode);
		VOFmodefound++;
	    }
	}
	if (!VOFmodefound) {
	    sprintf(badstr,"%s", tmpstr);
	    msg_printf(SUM,"\"%s\" is not a valid MCO mode\n", badstr);

	    msg_printf(VRB,"Supported MCO modes are:\n");
	    for ( i=0; mcoVOFmodelist[i].name[0]; i++) {
		msg_printf(VRB,"\t%s\n", mcoVOFmodelist[i].name);
	    }
	    return -1;
	}
	else {
	    msg_printf(DBG,"Testing MCO mode: %s  val = 0x%x\n",
	    VOFmodename, mco_mode);
	}

        argv++; argc--;
        if (argc != 1) {
                return -1;
	}

	pattern = atoi(argv[0]);
	msg_printf(VRB,"MCO OS Checksum Test:  Testing MCO mode: %s  testpattern = %d\n",
	    VOFmodename, pattern);

	if (pattern == 99) {
	    err = _mco_chksum(mco_mode, mco_four_screen_precomp(0), pattern);
	}
	else {
	    err = _mco_chksum(mco_mode, mco_four_screen_precomp(pattern), pattern);
	}
	if (err) {
		msg_printf(VRB,"mco_oschksumtest FAILED\n");
	}
	else {
		msg_printf(VRB,"mco_oschksumtest PASSED\n");
	}

	return(err);
}
	
#ifndef IP30
/* 
 * mco_ChkCompareBitTest() -- 
 *		     
 *	Level 0: This routine reads the Test Result bits from the 
 *	Input Swizzle FPGA and checks the results.
 *	
 *	This routine assumes the following setup has been done
 *	before running this test:
 *	    a. An acceptable pattern has been loaded into the Impact (MardiGras)
 *	       frame buffer.
 *	    2. The Impact graphics boardset is running a valid video display
 *             format (ie, the Impact pixel clock is running)
 *
 */
__uint32_t
_mco_ChkCompareBitTest(int bitlevel)
{
    uchar_t rdval, result, expectedresult;
    __uint32_t errors=0;
    char levelstring[8];

    /* Initialize the dcb interface for MCO Index Register. */
    mco_set_dcbctrl(mgbase, 2);

    MCO_SETINDEX(mgbase, 0, MCO_ISOS_TESTREG);

    MCO_IO_READ(mgbase, rdval);

    result = rdval & MCO_SWIZZLE_RESULT_MASK;
    if (bitlevel) {
	expectedresult = MCO_SWIZZLE_RESULT_MASK;
	sprintf(levelstring, "low");
    }
    else {
	expectedresult = 0;
	sprintf(levelstring, "high");
    }
    msg_printf(DBG,"mco_ChkCompareBitTest: Check Compare bits = 0x%x (exp 0x%x)\t(bits=%d)\n", rdval, expectedresult, bitlevel);
    if (result != expectedresult) {
	switch (bitlevel) {
	    case 1:
		if (~result & MCO_RIS_RESULT) {
		    msg_printf(ERR,"MCO Red Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (~result & MCO_GIS_RESULT) {
		    msg_printf(ERR,"MCO Green Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (~result & MCO_BIS_RESULT) {
		    msg_printf(ERR,"MCO Blue Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n");
		    errors++;
		}
		if (~result & MCO_OS1_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 1 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n");
		    errors++;
		}
		if (~result & MCO_OS2_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 2 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n");
		    errors++;
		}
		if (~result & MCO_OS3_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 3 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n");
		    errors++;
		}
		if (~result & MCO_OS4_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 4 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n");
		    errors++;
		}
		break;
	    case 0:
		if (result & MCO_RIS_RESULT) {
		    msg_printf(ERR,"MCO Red Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_GIS_RESULT) {
		    msg_printf(ERR,"MCO Green Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_BIS_RESULT) {
		    msg_printf(ERR,"MCO Blue Input Swizzle Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_OS1_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 1 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_OS2_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 2 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_OS3_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 3 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		if (result & MCO_OS4_RESULT) {
		    msg_printf(ERR,"MCO Output Swizzle 4 Check Compare Bit stuck %s (Probe Pin 12 on UYYY)\n", levelstring);
		    errors++;
		}
		break;
	}
    }
    return(errors);
}

/*ARGSUSED(ide command format)*/
int
mco_ChkCompareBitTest(int argc, char **argv)
{
    __uint32_t errors = 0;
    char ucodefilename[80];
    int level;

    /* Load special fpga program to set Check Compare Bits High */
#ifdef NO_NETWORK
    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/chkcmpbittest1.rbf");
#else
    sprintf(ucodefilename,"bootp()chkcmpbittest1.rbf");
#endif /* NO_NETWORK */
#ifdef IP30
    errors = _sr_mco_initfpga(ucodefilename);
#else	/* IP30 */
    errors = _mco_initfpga(ucodefilename);
#endif	/* IP30 */
    if (errors) {
	msg_printf(VRB,"ERROR: Couldn't load fpga file %s\n", ucodefilename);
	return errors;
    }
    level = 1;
    errors = _mco_ChkCompareBitTest(level); 
    if (errors) {
	msg_printf(VRB,"MCO Check Compare Bit Test 1 failed\n");
    }
    else {
	msg_printf(VRB,"MCO Check Compare Bit Test 1 passed\n");
    }

    /* Load special fpga program to set Check Compare Bits Low */
#ifdef NO_NETWORK
    sprintf(ucodefilename,"dksc(0,1,0)/usr/stand/chkcmpbittest0.rbf");
#else
    sprintf(ucodefilename,"bootp()chkcmpbittest0.rbf");
#endif /* NO_NETWORK */
#ifdef IP30
    errors = _sr_mco_initfpga(ucodefilename);
#else	/* IP30 */
    errors = _mco_initfpga(ucodefilename);
#endif	/* IP30 */
    if (errors) {
	msg_printf(ERR,"ERROR: Couldn't load fpga file %s\n", ucodefilename);
	return errors;
    }
    level = 0;
    errors += _mco_ChkCompareBitTest(level); 
    if (errors) {
	msg_printf(ERR,"MCO Check Compare Bit Test 0 failed\n");
    }
    else {
	msg_printf(VRB,"MCO Check Compare Bit Test 0 passed\n");
    }

    REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_ISOS_READREG_TEST]), errors);

}
#endif	/* IP30 */
