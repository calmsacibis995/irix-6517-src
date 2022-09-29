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

#include <sys/types.h>
#include <sgidefs.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "ide_msg.h"

int
_tex_detail(void)
{
__uint32_t cfifo_val, cfifo_status, height, scanWidth, cmd;

cfifo_val = 32;

_verif_setup();

hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00040000);
hq_HQ_RE_WrReg_0(0x188,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x189,0x00000000,0x00fff4cc);
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004422);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000780);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000080);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000000a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00800100);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00800100);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x200;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x80;
        scanWidth = 0x200;
        _send_data(height, scanWidth, TEX_DETAIL_TEST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00040000);
hq_HQ_RE_WrReg_0(0x192,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x0000000e);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x0000000c);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x194,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x194,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00002001);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000847e4);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00040000);
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004e22);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000110);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000018);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000660);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000040);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000040);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000000a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00400040);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00400040);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x80;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x40;
        scanWidth = 0x80;
        _send_data(height, scanWidth, TEX_DETAIL_TEST | 0x100);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00080040);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00046666);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000110);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000018);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000947e4);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00fffe66);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40409);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40449);
}
hq_HQ_RE_WrReg_1(0x204,0x4741420f,0x474026e1);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x0087a8d2);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x01000000);
hq_HQ_RE_WrReg_1(0x200,0x47414217,0x4741420f);
hq_HQ_RE_WrReg_1(0x202,0x4740bd00,0x4741420f);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000007,0xa68dda4d);
hq_HQ_RE_WrReg_2(0x2ca,0x00000002,0xd52416dd);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x0028cdb5);
hq_HQ_RE_WrReg_2(0x280,0x000004ca,0x5487ca2c);
hq_HQ_RE_WrReg_2(0x282,0x000004ca,0x54884737);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x12df976c);
hq_HQ_RE_WrReg_2(0x2c4,0xfffffffb,0x2e8de676);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x0000000c,0x77fff3d7);
hq_HQ_RE_WrReg_2(0x2c2,0x00000002,0xd52416dd);
hq_HQ_RE_WrReg_2(0x286,0x00000002,0x8cdb5852);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);
hq_HQ_RE_WrReg_1(0x204,0x474026e1,0x474026e1);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xff78572e);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x01000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x4740bce9,0x4740bd00);
hq_HQ_RE_WrReg_1(0x202,0x4741d81f,0x4741420f);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000007,0xa6934d36);
hq_HQ_RE_WrReg_2(0x2ca,0x00000002,0xd52416dd);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x0028cdb5);
hq_HQ_RE_WrReg_2(0x280,0x00000249,0x84408579);
hq_HQ_RE_WrReg_2(0x282,0x000004ca,0x54884737);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x12df976c);
hq_HQ_RE_WrReg_2(0x2c4,0x00000004,0xd16ca6a0);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x00000002,0xd526a695);
hq_HQ_RE_WrReg_2(0x2c2,0x00000002,0xd52416dd);
hq_HQ_RE_WrReg_2(0x286,0x00000002,0x8cdb5852);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);

	return 0;
}
