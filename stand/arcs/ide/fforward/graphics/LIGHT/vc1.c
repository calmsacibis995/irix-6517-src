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
 *  VC1 Diagnostics
 *  $Revision: 1.13 $
 */

#include <math.h>
#include <sys/lg1hw.h>
#include <ide_msg.h>
#include "light.h"
#include <libsc.h>
#include <uif.h>

#ifndef PROM
#include "xlogo32"
#include "bob"
#endif

#ifndef NULL
#define NULL 0
#endif

extern Rexchip *REX;
extern struct lg1_info lg1_ginfo;

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

#define min(a, b)	(a > b ? b : a)
#define max(a, b)	(a < b ? b : a)

extern int f_rgb;    

int vc1_length(int cmd, int offset)
{
    if (cmd == VC1_XMAP_MODE)
	    return 16;
    switch (offset) {
    case VID_EP:
    case CUR_EP:
    case DID_EP:
    case DID_END_EP:
	    return 16;
    case CUR_XL:
    case CUR_LY:
	    return 11;
    case CUR_YL:
	    return 12;
    case CUR_MODE:
    case CUR_BX:
    case DID_HOR_DIV:
    case BLKOUT_EVEN:
    case BLKOUT_ODD:
    case AUXVIDEO_MAP:
	    return 8;
    case DID_HOR_MOD:
	    return 3;
    }
    printf("vc1_length: unknown register\n");
}

#ifndef PROM
void vc1_off(void)
{
	int state;

	REX->p1.go.configsel = VC1_SYS_CTRL;
	VC1GetByte(state);

	REX->p1.go.configsel = VC1_SYS_CTRL;
	VC1SetByte(state & ~VC1_VC1);
}

void vc1_on(void)
{
	int state;

	REX->p1.go.configsel = VC1_SYS_CTRL;
	VC1GetByte(state);

	REX->p1.go.configsel = VC1_SYS_CTRL;
	VC1SetByte(state | VC1_VC1);
}

/*
 * wvc1() - write VC1 register
 *
 *	This is a tool to allow writing of arbitrary values to the various
 *	VC1 registers from the IDE command line.
 */
int wvc1(int argc, char **argv)
{
    int tmp;
    int cmd;		/* command for configsel */
    int offset;		/* VC1 register offset */
    int data;	  	/* VC1 register data */

    if (argc < 4) {
	printf("usage: %s CMD VC1_reg_offset data\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &cmd);
    atohex(argv[2], &offset);
    atohex(argv[3], &data);

    if (vc1_length(cmd, offset) > 8) {
	VC1SetAddr(offset, cmd);
	VC1SetWord(data);
    } else {
	VC1SetAddr(offset+1, cmd);
	VC1GetByte(tmp);
	VC1SetAddr(offset, cmd);
	VC1SetByte(data);
	VC1SetByte(tmp);
    }
    printf("Writing VC1 element %d offset %#04x with %#04x\n", cmd, offset, data);
    return 0;
}


/*
 * rvc1() - read VC1 register
 *
 *	This is a tool to allow reading of values from the various
 *	VC1 registers from the IDE command line.
 */
int rvc1(int argc, char **argv)
{
    int cmd;		/* command for configsel */
    int offset;		/* VC1 register offset */
    int data;   	/* VC1 register data */

    if (argc < 3) {
	printf("usage: %s CMD VC1_reg_offset\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &cmd);
    atohex(argv[2], &offset);

    data = 0;
    VC1SetAddr(offset, cmd);
    VC1GetByte(data);
    printf("Reading VC1 element %d offset %#04x returns %#02x\n", cmd, offset, data);
    return 0;
}
#endif /* !PROM */


int test_vc1_register(short cmd, unsigned short offset, unsigned int pattern)
{
    unsigned int expect;
    unsigned int got;
    unsigned int low;
    unsigned int high;
    int size;

    expect = pattern;
    size = vc1_length(cmd, offset);
    expect &= (0x1 << size) - 1;
    VC1SetAddr(offset, cmd);
    VC1SetWord(pattern);

    if (cmd == VC1_CURSOR_CTRL && offset == CUR_XL) {
	VC1SetAddr(CUR_YL, cmd);
	VC1SetWord(pattern);
    }

    high = low = 0;
    VC1SetAddr(offset, cmd);
    VC1GetByte(high);
    VC1GetByte(low);

    if (size > 8)
	got = (high << 8) | low;
    else
	got = high;
    if (got != expect) {
	msg_printf(ERR, "Error reading from VC1 element %d offset %#04x, returned: %#04x, expected: %#04x\n", cmd, offset, got, expect);
	return 1;
    }
    return 0;
}

#ifndef PROM
int test_vc1(void)
{
    unsigned int pattern;
    int errors = 0;
    int i, fix;

    msg_printf(VRB, "Starting VC1 tests...\n");

    REX->p1.go.configsel = VC1_SYS_CTRL;
    VC1GetByte(fix);
    if (fix & VC1_VC1)
	do {
	    VC1SetAddr(VID_LC, VC1_VID_TIMING);
	    VC1GetWord(fix);
	} while (fix > 16);
    REX->p1.go.configsel = VC1_SYS_CTRL;
    VC1SetByte(VC1_INTERRUPT | VC1_VTG);

    for (i = 0; i < 0x4; i++) {
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
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_2_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_3_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_4_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_5_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_6_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_7_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_8_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_9_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_A_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_B_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_C_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_D_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_E_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_F_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_10_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_11_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_12_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_13_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_14_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_15_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_16_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_17_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_18_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_19_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1A_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1B_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1C_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1D_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1E_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1F_E, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_0_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_2_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_3_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_4_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_5_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_6_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_7_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_8_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_9_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_A_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_B_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_C_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_D_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_E_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_F_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_10_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_11_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_12_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_13_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_14_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_15_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_16_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_17_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_18_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_19_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1A_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1B_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1C_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1D_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1E_O, pattern);
	errors += test_vc1_register(VC1_XMAP_MODE, MODE_REG_1F_O, pattern);
    }
    if (!errors) {
	okydoky();
	Lg1RegisterInit(REX, &lg1_ginfo);
	return 0;
    } else {
	msg_printf(ERR, "Total of %d errors detected in VC1 register r/w tests.\n", errors);
	Lg1RegisterInit(REX, &lg1_ginfo);
	return -1;
    }
}


int fix(void)
{
    int sysctl;
    
    VC1SetAddr(0x0, 6); /* system control */
    sysctl = REX->p1.go.rwvc1;
    sysctl = REX->p1.set.rwvc1;
    VC1SetByte(((sysctl | 0x02) & 0xfb));
    VC1SetByte(sysctl);
}


int vc1cursor(int argc, char **argv)
{
    int x, y;

    if (argc < 3) {
	printf("usage: %s x y\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x);
    atob(argv[2], &y);
    
    VC1SetAddr(CUR_XL, VC1_CURSOR_CTRL);
    VC1SetWord(x+140);
    VC1SetWord(y+39);
    return 0;
}    

int wsysctl(int argc, char **argv)
{
    int data;

    if (argc < 2) {
        printf("usage: %s data\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &data);
    REX->p1.go.configsel = VC1_SYS_CTRL;
    VC1SetByte(data);
    return 0;
}

void rsysctl(int argc, char **argv)
{
    int data;

    REX->p1.go.configsel = VC1_SYS_CTRL;
    VC1GetByte(data);
    printf("VC1 SYS_CTL register contains %#02x\n", data);
}

int cursormode(int argc, char **argv)
{
    int bx;
    int mode;

    if (argc < 2) {
	printf("usage: %s mode\n", argv[0]);
	return -1;
    }
    atob(argv[1], &mode);
    VC1SetAddr(CUR_BX, VC1_CURSOR_CTRL);
    VC1GetByte(bx);

    if (mode == CUR_MODE_NORMAL) {
	VC1SetAddr(CUR_EP, VC1_CURSOR_CTRL);
	VC1SetWord(0x3000);
	VC1SetAddr(CUR_MODE, VC1_CURSOR_CTRL);
	VC1SetByte(mode);
	VC1SetByte(32);
    } else if (mode == CUR_MODE_SPECIAL) {
	VC1SetAddr(CUR_EP, VC1_CURSOR_CTRL);
	VC1SetWord(0x2000); /* CUR_EP */
	VC1SetAddr(CUR_MODE, VC1_CURSOR_CTRL);
	VC1SetByte(mode);
	VC1SetByte(bob_width/8);
	VC1SetWord(bob_height);
    } else {
	VC1SetAddr(CUR_MODE, VC1_CURSOR_CTRL);
	VC1SetByte(mode);
	VC1SetByte(bx);
    }	
    return 0;
}
#endif /* !PROM */
