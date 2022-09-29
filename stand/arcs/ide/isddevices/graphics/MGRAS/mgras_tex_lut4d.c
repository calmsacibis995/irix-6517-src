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
_tex_lut4d(void)
{
__uint32_t i,cfifo_val, cfifo_status, height, scanWidth, cmd;

cfifo_val = 32;

_verif_setup();

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004046);
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
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000350);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400081);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00080020);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00080020);
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

        height = 0x8;
        scanWidth = 0x100;
        _send_data(height, scanWidth, TEX_LUT4D_TEST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000101);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000350);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400081);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00080020);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00080020);
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

        height = 0x8;
        scanWidth = 0x100;
        _send_data(height, scanWidth, TEX_LUT4D_TEST | 0x100);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000102);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000350);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400081);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00080020);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00080020);
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

        height = 0x8;
        scanWidth = 0x100;
        _send_data(height, scanWidth, TEX_LUT4D_TEST | 0x200);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000103);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000350);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000020);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400081);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00080020);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00080020);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }


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

        height = 0x8;
        scanWidth = 0x100;
        _send_data(height, scanWidth, TEX_LUT4D_TEST | 0x300);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10818);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10858);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x182,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00020008);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00103535);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d0);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000100);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10818);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10858);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x008141c8);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10818);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10858);
}
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10819);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x10859);
}
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d0);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d0);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d0);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d2);
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d2);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

if (numRE4s == 2) {
	hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00800100);
	hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00800100);
}
else if (numRE4s == 1) {
	hq_HQ_RE_WrReg_0(0x153,0x00000000,0x01000100);
	hq_HQ_RE_WrReg_0(0x158,0x00000000,0x01000100);
}
hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d3);
hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00005000);
hq_HQ_RE_WrReg_0(0x112,0x00000000,0x0000000c);
hq_HQ_RE_WrReg_0(0x115,0x00000000,0x00ff0000);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x473fff80,0x473fff80);
hq_HQ_RE_WrReg_1(0x200,0x473fff80,0x4740ff80);
hq_HQ_RE_WrReg_1(0x202,0x00000000,0x4740ff80);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x445,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004026);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00400080);

/* Initializing the HQ for scanlines */
FormatterMode(0x201);

scanWidth = 0x400;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

if (numRE4s == 2) {
	/* we need to do 2 writes at this point, 1 for even lines and 1 for
	 * odd lines.
	 */
	for (i = 0; i < 2; i++) {

		hq_HQ_RE_WrReg_0(0x112,0x00000000,i);
		hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00800100);
        	hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00800100);
		hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000045);

		/* use file MGRAS_lut4d_array_4 */
        	height = 0x100;
        	scanWidth = 0x400;
        	_send_data(height, scanWidth, TEX_LUT4D_TEST | 0x400|(i << 12));

	}
	
	hq_HQ_RE_WrReg_0(0x112,0x00000000,0x00000004);
	hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d2);
	hq_HQ_RE_WrReg_0(0x115,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00005000);
	hq_HQ_RE_WrReg_0(0x161,0x00000000,0x0c004203);
	hq_HQ_RE_WrReg_0(0x160,0x00000000,0x00000000);
}
else if (numRE4s == 1) {
	hq_HQ_RE_WrReg_2(0x102,0x5555,0x00000045);

	/* use file MGRAS_lut4d_array_4 */
        height = 0x100;
        scanWidth = 0x400;
        _send_data(height, scanWidth, TEX_LUT4D_TEST | 0x400 );

	hq_HQ_RE_WrReg_0(0x112,0x00000000,0x00000004);
	hq_HQ_RE_WrReg_0(0x18a,0x00000000,0x040048d2);
	hq_HQ_RE_WrReg_0(0x115,0x00000000,0x00000000);
	hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00005000);
	hq_HQ_RE_WrReg_0(0x161,0x00000000,0x0c004203);
	hq_HQ_RE_WrReg_0(0x160,0x00000000,0x00000000);
}

	return 0;
}
