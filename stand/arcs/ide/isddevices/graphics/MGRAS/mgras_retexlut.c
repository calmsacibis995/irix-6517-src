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
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif

extern mgras_hw *mgbase;
extern __uint32_t numTRAMs;

__uint32_t re_texlut_array[64] = {
0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f,
0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f, 0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f,
0x30313233, 0x34353637, 0x38393a3b, 0x3c3d3e3f, 0x40414243, 0x44454647, 0x48494a4b, 0x4c4d4e4f,
0x50515253, 0x54555657, 0x58595a5b, 0x5c5d5e5f, 0x60616263, 0x64656667, 0x68696a6b, 0x6c6d6e6f,
0x70717273, 0x74757677, 0x78797a7b, 0x7c7d7e7f, 0x80818283, 0x84858687, 0x88898a8b, 0x8c8d8e8f,
0x90919293, 0x94959697, 0x98999a9b, 0x9c9d9e9f, 0xa0a1a2a3, 0xa4a5a6a7, 0xa8a9aaab, 0xacadaeaf,
0xb0b1b2b3, 0xb4b5b6b7, 0xb8b9babb, 0xbcbdbebf, 0xc0c1c2c3, 0xc4c5c6c7, 0xc8c9cacb, 0xcccdcecf,
0xd0d1d2d3, 0xd4d5d6d7, 0xd8d9dadb, 0xdcdddedf, 0xe0e1e2e3, 0xe4e5e6e7, 0xe8e9eaeb, 0xecedeeef,
0xf0f1f2f3, 0xf4f5f6f7, 0xf8f9fafb, 0xfcfdfeff
};


__uint32_t _retexlut();


__uint32_t _retexlut()
{
__uint32_t rssnum=4, cfifo_val, cfifo_status, height, scanWidth, cmd, pix_cmd;
__int32_t i, j, k, s, t;
__uint32_t *data, word_width;

data = re_texlut_array;

        cfifo_val = 32;


WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00004000);
hq_HQ_RE_WrReg_0(0x161,0x00000000,0x00000300);
hq_HQ_RE_WrReg_0(0x16a,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40000);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40040);
}
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x188,0x00000000,0x00800800);
hq_HQ_RE_WrReg_0(0x189,0x00000000,0x00800800);
hq_HQ_RE_WrReg_0(0x1a7,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x186,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x182,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x110,0x00000000,0x00005000);
hq_HQ_RE_WrReg_0(0x112,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x113,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x114,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x144,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x145,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x116,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x115,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x147,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x148,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x149,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14a,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14b,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14c,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14d,0x00000000,0x000004ff);
hq_HQ_RE_WrReg_0(0x14e,0x00000000,0x000003ff);
hq_HQ_RE_WrReg_0(0x14f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x156,0x00000000,0xffffffff);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x157,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00040000);
hq_HQ_RE_WrReg_0(0x15a,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x15b,0x00000000,0x000f0000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x0);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40);
}
hq_HQ_RE_WrReg_0(0x160,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x161,0x00000000,0x0c004300);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000001);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x58840000);
hq_HQ_RE_WrReg_0(0x166,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x167,0x00000000,0x0000ffff);
hq_HQ_RE_WrReg_0(0x168,0x00000000,0x0ffffff1);
hq_HQ_RE_WrReg_0(0x169,0x00000000,0x00000007);
hq_HQ_RE_WrReg_0(0x16a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x16d,0x00000000,0x000c8240);
if (numRE4s == 2) {
	hq_HQ_RE_WrReg_0(0x16e,0x00000000,0x0000025e);
}
else {
	hq_HQ_RE_WrReg_0(0x16e,0x00000000,0x0000031e);
}
hq_HQ_RE_WrReg_0(0x187,0x00000000,0x0000010f);
hq_HQ_RE_WrReg_0(0x162,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x163,0x00000000,0xffffffff);
hq_HQ_RE_WrReg_0(0x164,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x268,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x50000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004111);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x40000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004111);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004191);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004111);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x50000000);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004191);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00004111);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0xb0000000);

for (k = 0; k < 2; k++) {
   for (i = 0xff, j = 0; i >= 0x3; i-=4, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
   }
}

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x15c,0x00000000,0xb0000080);

for (i = 0; i < 32; i++) {
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x00000100);
}

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40000);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40040);
}
hq_HQ_RE_WrReg_0(0x1a0,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18b,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40400);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40440);
}
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40480);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x404c0);
}
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00034020);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000440);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000010);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00000090);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00100010);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00100010);
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

        height = 0x10;
        scanWidth = 0x10;
	word_width = scanWidth/4;

#if 0
        _send_data(height, scanWidth, TE1_LUT_TEST);
#endif

pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, scanWidth);

for (t = 0; t < height; t++) {

       HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);

       for (s = 0; s < word_width; s+=2) {
            if ((s % 0x20) == 0) {
                   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);                   if (cfifo_status == CFIFO_TIME_OUT) {
                                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");                                return (-1);
                   }
             }

             HQ3_PIO_WR64(CFIFO1_ADDR, *(data), *(data +1), ~0x0);
	     data++; data++;
        }
}

_mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);

hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40480);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x404c0);
}
hq_HQ_RE_WrReg_0(0x142,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x143,0x00000000,0x00000000);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x40480);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x404c0);
}
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x90000000);

for (i = 0x33, j = 0; i <= 0xff; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}
	
for (i = 0, j = 0; i <= 0x32; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}

hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x90000100);

for (i = 0x66, j = 0; i <= 0xff; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}

for (i = 0x0, j = 0; i <= 0x65; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}

hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x90000200);

for (i = 0x99, j = 0; i <= 0xff; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}

for (i = 0, j = 0; i <= 0x98; i++, j++) {
	if (j == 32) {
		WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        	if (cfifo_status == CFIFO_TIME_OUT) {
                	msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                	return (-1);
        	}
		j = 0;
	}
		
	hq_HQ_RE_WrReg_0(0x15d,0x00000000,i);
}


WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }


if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x4e480);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x4e4c0);
}
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x80000054);
hq_HQ_RE_WrReg_0(0x165,0x00000000,0x5990c9c3);
hq_HQ_RE_WrReg_0(0x161,0x00000000,0x0c004308);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x20c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x47417f80,0x47407f80);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x01000000);
hq_HQ_RE_WrReg_1(0x200,0x47417f80,0x47417f80);
hq_HQ_RE_WrReg_1(0x202,0x47408000,0x47417f80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00011984);
if (numTRAMs == 1) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x4e401);
} else if (numTRAMs == 4) {
    hq_HQ_RE_WrReg_0(0x111,0x00000000,0x4e441);
}
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x009997ff);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00fffe66);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00004444);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00020010);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xfffffffe,0x00008000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x000001fe,0xff804000);
hq_HQ_RE_WrReg_2(0x282,0x000001fe,0xff804000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0xfffffffe,0x00008000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x00000001,0xffff8000);
hq_HQ_RE_WrReg_2(0x2c2,0xfffffffe,0x00008000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000001);
hq_HQ_RE_WrReg_1(0x20c,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x47407f80,0x47407f80);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x01000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x47407f80,0x47408000);
hq_HQ_RE_WrReg_1(0x202,0x47417f80,0x47417f80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xfffffffe,0x00008000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x00000000,0xffffc000);
hq_HQ_RE_WrReg_2(0x282,0x000001fe,0xff804000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0x00000001,0xffff8000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c2,0xfffffffe,0x00008000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x413,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x160,0x00000000,0x00000000);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_0(0x161,0x00000000,0x00006300);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x000000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000000dd);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x010000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000001dd);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x020000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000002dd);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x100000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000100dd);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x110000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000101dd);
hq_HQ_RE_WrReg_0(0x15c,0x00000000,0x120000d0);
hq_HQ_RE_WrReg_0(0x15d,0x00000000,0x000102dd);

return (0);

}
