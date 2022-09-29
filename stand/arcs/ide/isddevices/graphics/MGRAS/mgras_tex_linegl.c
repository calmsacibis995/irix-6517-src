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
_tex_linegl(void)
{
__uint32_t cfifo_val, cfifo_status, height, scanWidth, cmd;

	cfifo_val = 32;

	_verif_setup();

	hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004e22);
	hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000770);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000080);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000080);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000000a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00800080);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00800080);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x100;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x80;
        scanWidth = 0x100;
        _send_data(height, scanWidth, TEX_LINEGL_TEST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00100080);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00007777);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000009);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00001661);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000040);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000040);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

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
        _send_data(height, scanWidth, TEX_LINEGL_TEST | 0x100);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000009);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x0000010c);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000d);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00002552);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000000a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00200020);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00200020);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x40;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x20;
        scanWidth = 0x40;
        _send_data(height, scanWidth, TEX_LINEGL_TEST | 0x200);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x0000010c);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000d);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e4);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00fffe66);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40409);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40449);
}
hq_HQ_RE_WrReg_0(0x110,0x00000000,0x10005000);
hq_HQ_RE_WrReg_0(0x113,0x00000000,0x000001ff);
hq_HQ_RE_WrReg_0(0x114,0x00000000,0x000001ff);
hq_HQ_RE_WrReg_0(0x113,0x00000000,0x00fa0103);
hq_HQ_RE_WrReg_0(0x114,0x00000000,0x00fa0103);
hq_HQ_RE_WrReg_1(0x20c,0x4740db80,0x47412380);
hq_HQ_RE_WrReg_1(0x20e,0x47412380,0x4740db80);
hq_HQ_RE_WrReg_1(0x210,0x01000000,0x03000000);
hq_HQ_RE_WrReg_0(0x012,0x00000000,0x01800000);
hq_HQ_RE_WrReg_2(0x2c0,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffc0,0x00100000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x00000ebf,0xfc500000);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x282,0x0000213f,0xf7b00000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x2c6,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x2ca,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x060,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x062,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x064,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x066,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000003);
hq_HQ_RE_WrReg_1(0x20c,0x47412380,0x47412380);
hq_HQ_RE_WrReg_1(0x20e,0x4740db80,0x4740db80);
hq_HQ_RE_WrReg_1(0x210,0x03000000,0x03000000);
hq_HQ_RE_WrReg_0(0x012,0x00000000,0x00800000);
hq_HQ_RE_WrReg_2(0x2c0,0xffffffc0,0x00100000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffc0,0x00100000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x000020ff,0xf7c00000);
hq_HQ_RE_WrReg_2(0x282,0x000020ff,0xf7c00000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x2c6,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x2ca,0x0000003f,0xfff00000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

	hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
	hq_HQ_RE_WrReg_0(0x060,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x062,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x064,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x066,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
	hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
	hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000003);

	return 0;
}

