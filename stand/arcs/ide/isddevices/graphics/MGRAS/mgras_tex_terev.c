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

int _tex_terev(void)
{
__uint32_t cfifo_val, cfifo_status, height, scanWidth, cmd;

        cfifo_val = 32;

	_verif_setup();

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004e04);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000100);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000660);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000040);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000040);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400078);
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

   /* use file MGRAS_te_rev_array_0 */
        height = 0x40;
        scanWidth = 0x80;
        _send_data(height, scanWidth, TEX_TEREV_TEST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40810);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40850);
}
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00080040);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e0);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x182,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x188,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x189,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00006666);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00001551);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400078);
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

   /* use file MGRAS_te_rev_array_1 */
        height = 0x20;
        scanWidth = 0x40;
        _send_data(height, scanWidth, TEX_TEREV_TEST | 0x100);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00002442);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400078);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00100010);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00100010);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x20;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000005);

   /* use file MGRAS_te_rev_array_2 */
        height = 0x10;
        scanWidth = 0x20;
        _send_data(height, scanWidth, TEX_TEREV_TEST | 0x200);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000002);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00003333);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400078);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00080008);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00080008);
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

   /* use file MGRAS_te_rev_array_3 */
        height = 0x8;
        scanWidth = 0x10;
        _send_data(height, scanWidth, TEX_TEREV_TEST | 0x300);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000001);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00004224);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400078);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00040004);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00040004);
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

   /* use file MGRAS_te_rev_array_4 */
        height = 0x4;
        scanWidth = 0x8;
        _send_data(height, scanWidth, TEX_TEREV_TEST | 0x400);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x193,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40810);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40850);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000941e0);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000909e0);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000919e0);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40812);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40852);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000918a0);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x000918a0);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000fff);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00fff000);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40813);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40853);
}
hq_HQ_RE_WrReg_1(0x204,0x4741013e,0x4740f7dd);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x02c53f54);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xfeb84dce);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x0824c8e0);
hq_HQ_RE_WrReg_1(0x200,0x47410323,0x4741005e);
hq_HQ_RE_WrReg_1(0x202,0x4740f34a,0x47410daf);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xfffffffc,0x48bc9d92);
hq_HQ_RE_WrReg_2(0x2ca,0xfffffff7,0xcde5cf7f);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00c4b273);
hq_HQ_RE_WrReg_2(0x280,0x000001e8,0x3fefb89c);
hq_HQ_RE_WrReg_2(0x282,0x000001a5,0x09315a2b);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x0c871688);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe1,0xe857f3fe);
hq_HQ_RE_WrReg_2(0x2c6,0x00000006,0x67136b36);
hq_HQ_RE_WrReg_2(0x288,0xfffffff6,0x6562e6d5);
hq_HQ_RE_WrReg_2(0x2c0,0x00000056,0x8fb4c198);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe4,0x98ab8ddb);
hq_HQ_RE_WrReg_2(0x286,0x00000029,0x1afe8a7b);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);
hq_HQ_RE_WrReg_1(0x204,0x4740f7dd,0x4740c14e);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x001a368d);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x208,0x00000000,0x0824c8e0);
hq_HQ_RE_WrReg_2(0x20a,0xffffffff,0xfeb8557c);
hq_HQ_RE_WrReg_1(0x200,0x4740f157,0x4740f34a);
hq_HQ_RE_WrReg_1(0x202,0x47413c97,0x4741013e);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0xfffffffc,0x49589c07);
hq_HQ_RE_WrReg_2(0x2ca,0xfffffff7,0xce0d8b0b);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00c4aeba);
hq_HQ_RE_WrReg_2(0x280,0xffffffbc,0x17c9e71c);
hq_HQ_RE_WrReg_2(0x282,0x000001af,0x880d5d8d);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x0b8b31e9);
hq_HQ_RE_WrReg_2(0x2c4,0x0000001e,0x17a0816d);
hq_HQ_RE_WrReg_2(0x2c6,0xfffffff9,0x98e53930);
hq_HQ_RE_WrReg_2(0x288,0x00000009,0x9aa82293);
hq_HQ_RE_WrReg_2(0x2c0,0xfffffffc,0x49589c07);
hq_HQ_RE_WrReg_2(0x2c2,0xfffffff7,0xce0d8b0b);
hq_HQ_RE_WrReg_2(0x286,0x0000000c,0x4aeba5a6);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);

return(0);
}

