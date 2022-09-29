/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
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


__uint32_t _tex_load(void)
{
__uint32_t cfifo_val, cfifo_status, height, scanWidth, cmd;

cfifo_val = 32;

_verif_setup();

hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000001);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000841f4);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00005b02);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000750);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000022);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000082);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00820022);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00820022);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x44;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x82;
        scanWidth = 0x44;
        _send_data(height, scanWidth, TEX_LOAD_TEST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00040080);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000841f0);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00007575);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000009);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00001641);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000012);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000042);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00420012);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00420012);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x24;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x42;
        scanWidth = 0x24;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x100);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000009);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x0000010c);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00002532);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000022);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x0022000a);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x0022000a);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x14;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x22;
        scanWidth = 0x14;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x200);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x0000010c);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00003423);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000012);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00120006);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00120006);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0xc;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x12;
        scanWidth = 0xc;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x300);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000849f0);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val,CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000859f0);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40408);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40448);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000859f0);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085970);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x009997ff);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00fffe66);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40409);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40449);
}
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40409);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40449);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085970);
hq_HQ_RE_WrReg_1(0x204,0x47413869,0x4740dd16);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x007fd43e);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xfe44917c);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x010e3c28);
hq_HQ_RE_WrReg_1(0x200,0x4740ebe0,0x4740eb7c);
hq_HQ_RE_WrReg_1(0x202,0x4740c4ca,0x47414f2d);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xfffffff2,0x8f328840);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe2,0xa429f5b7);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xffd2dad2);
hq_HQ_RE_WrReg_2(0x280,0x0000040a,0x02389b31);
hq_HQ_RE_WrReg_2(0x282,0x00001077,0xa0c8c355);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x400c9141);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffec,0x60f8002c);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val,CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x2c6,0x00000010,0xf30e4042);
hq_HQ_RE_WrReg_2(0x288,0x00000001,0xa103669e);
hq_HQ_RE_WrReg_2(0x2c0,0x00000006,0x2e3a8813);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffd1,0xb11bb575);
hq_HQ_RE_WrReg_2(0x286,0xfffffffb,0x8ca9b008);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);
hq_HQ_RE_WrReg_1(0x204,0x4740dd16,0x4740ce07);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00a93815);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x010e3c28);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xfe449944);
hq_HQ_RE_WrReg_1(0x200,0x4740c4a0,0x4740c4ca);
hq_HQ_RE_WrReg_1(0x202,0x4741249c,0x47413869);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xfffffff3,0xd791ac17);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe2,0xa4675b98);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xffd2db30);
hq_HQ_RE_WrReg_2(0x280,0xffffffeb,0x04e86d49);
hq_HQ_RE_WrReg_2(0x282,0x00001058,0x72c22ae0);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3fdc9f35);
hq_HQ_RE_WrReg_2(0x2c4,0x00000012,0x67f9280b);
hq_HQ_RE_WrReg_2(0x2c6,0xffffffef,0x0cb367c1);
hq_HQ_RE_WrReg_2(0x288,0xfffffffe,0x5ef69b85);
hq_HQ_RE_WrReg_2(0x2c0,0xfffffff3,0xd791ac17);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe2,0xa4675b98);
hq_HQ_RE_WrReg_2(0x286,0xfffffffd,0x2db2fd3f);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40409);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40449);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085970);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00005b26);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000440);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000012);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000012);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400080);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00120012);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00120012);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x48;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x12;
        scanWidth = 0x48;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x400);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40819);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40859);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00020010);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085974);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00004444);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000101);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00001331);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400080);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x000a000a);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x000a000a);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x28;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0xa;
        scanWidth = 0x28;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x500);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00002222);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val,CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400080);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00060006);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00060006);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x18;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x6;
        scanWidth = 0x18;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x600);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00003113);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400080);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00040004);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00040004);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x10;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x4;
        scanWidth = 0x10;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x700);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000003);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000001);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40819);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40859);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085974);
hq_HQ_RE_WrReg_1(0x204,0x47410516,0x4740fc63);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x004062cf);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xfe44a6a6);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x03bb5f88);
hq_HQ_RE_WrReg_1(0x200,0x4741052d,0x4741047a);
hq_HQ_RE_WrReg_1(0x202,0x4740eaf8,0x4741145a);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xffffffef,0x011b5764);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe6,0x3230a240);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xfec29165);
hq_HQ_RE_WrReg_2(0x280,0x00000267,0xe860bd34);
hq_HQ_RE_WrReg_2(0x282,0x00000267,0xa3078d6d);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3fa0177c);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffee,0x049717a7);
hq_HQ_RE_WrReg_2(0x2c6,0x0000000e,0xe64973a4);
hq_HQ_RE_WrReg_2(0x288,0x0000000b,0x74a2a200);
hq_HQ_RE_WrReg_2(0x2c0,0x00000000,0xfc843fbd);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffd7,0x4be72e9c);
hq_HQ_RE_WrReg_2(0x286,0xffffffe0,0xb473a15f);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val,CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);
hq_HQ_RE_WrReg_1(0x204,0x4740fc63,0x4740f3e3);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x0108060c);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x03bb5f88);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xfe447878);
hq_HQ_RE_WrReg_1(0x200,0x4740eabd,0x4740eaf8);
hq_HQ_RE_WrReg_1(0x202,0x47410a71,0x47410516);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xffffffef,0x01030afc);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe6,0x307fd055);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xfec27c98);
hq_HQ_RE_WrReg_2(0x280,0xffffff95,0x6e732ab1);
hq_HQ_RE_WrReg_2(0x282,0x00000267,0xf8e950ed);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3fa437fa);
hq_HQ_RE_WrReg_2(0x2c4,0x00000011,0xfb9b2861);
hq_HQ_RE_WrReg_2(0x2c6,0xfffffff1,0x1a063a88);
hq_HQ_RE_WrReg_2(0x288,0xfffffff4,0x8b9aa13f);
hq_HQ_RE_WrReg_2(0x2c0,0x00000000,0xfc9e335c);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffd7,0x4a860adc);
hq_HQ_RE_WrReg_2(0x286,0xffffffe0,0xb3641df4);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000001);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40899);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x408d9);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085974);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status,cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00005b2a);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000350);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000022);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x000a0022);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x000a0022);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x44;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0xa;
        scanWidth = 0x44;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x800);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40489);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x404c9);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00040008);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085974);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00003535);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000b);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00001241);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000012);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00060012);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00060012);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x24;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x6;
        scanWidth = 0x24;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0x900);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00002132);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x0004000a);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x0004000a);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x14;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x4;
        scanWidth = 0x14;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0xa00);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00003023);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000006);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000100a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00030006);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00030006);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0xc;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x3;
        scanWidth = 0xc;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0xb00);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000004);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00004014);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x000000a0);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00030004);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00030004);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x8;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

        height = 0x3;
        scanWidth = 0x8;
        _send_data(height, scanWidth, TEX_LOAD_TEST | 0xc00);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x0000000a);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40489);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x404c9);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00085974);
hq_HQ_RE_WrReg_1(0x204,0x47410163,0x4740fc4a);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xfe4488c2);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x005cf98a);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xf879d218);
hq_HQ_RE_WrReg_1(0x200,0x47410b9c,0x47410c9b);
hq_HQ_RE_WrReg_1(0x202,0x47410ea4,0x47410f7a);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xffffffe9,0x1077b4f2);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffed,0x47fdd9c7);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xff29bcc9);
hq_HQ_RE_WrReg_2(0x280,0x00000459,0xf8ac5bc2);
hq_HQ_RE_WrReg_2(0x282,0x00000117,0x9f05fcde);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3fcf754a);
hq_HQ_RE_WrReg_2(0x2c4,0x00000018,0x45c5f449);
hq_HQ_RE_WrReg_2(0x2c6,0xfffffff5,0x31b640a5);
hq_HQ_RE_WrReg_2(0x288,0xfffffff8,0x44ffa136);
hq_HQ_RE_WrReg_2(0x2c0,0xffffffb8,0x84ebcc5e);
hq_HQ_RE_WrReg_2(0x2c2,0x00000002,0xe491587e);
hq_HQ_RE_WrReg_2(0x286,0x00000002,0x11cd48e8);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x4740fc4a,0x4740f0cf);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xfe44a50a);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xf879d218);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00d70f97);
hq_HQ_RE_WrReg_1(0x200,0x474110e2,0x47410ea4);
hq_HQ_RE_WrReg_1(0x202,0x4740eb70,0x47410163);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xffffffe9,0x0ffa3273);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x2ca,0xffffffed,0x49d1b35c);
hq_HQ_RE_WrReg_2(0x28a,0xffffffff,0xff29d1b4);
hq_HQ_RE_WrReg_2(0x280,0x00000379,0xedd31c52);
hq_HQ_RE_WrReg_2(0x282,0xffffffe6,0x59d2ee7f);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x322936c3);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe7,0xba9180a7);
hq_HQ_RE_WrReg_2(0x2c6,0x0000000a,0xcdec0fd5);
hq_HQ_RE_WrReg_2(0x288,0x00000007,0xbabd58fc);
hq_HQ_RE_WrReg_2(0x2c0,0xffffffd0,0xca8bb31a);
hq_HQ_RE_WrReg_2(0x2c2,0xfffffff8,0x17bdc331);
hq_HQ_RE_WrReg_2(0x286,0xfffffffa,0x57d8976d);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);

return(0);
}

