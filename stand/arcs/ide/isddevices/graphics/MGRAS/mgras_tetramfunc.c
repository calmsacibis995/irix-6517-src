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
#include "libsc.h"
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "mgras_tetr.h"
#include "ide_msg.h"
#include "mgras_tex_poly.h"

#if 0
__uint64_t TEX_1D_CHECKSUM;
__uint64_t TEX_3D_CHECKSUM;
__uint64_t TEX_LOAD_CHECKSUM;
__uint64_t TEX_SCISTRI_CHECKSUM;
__uint64_t TEX_LINEGL_CHECKSUM;
__uint64_t TEX_PERSP_CHECKSUM;
__uint64_t TEX_MAG_CHECKSUM;
__uint64_t TEX_DETAIL_CHECKSUM;	
__uint64_t TEX_BORDERTALL_CHECKSUM;
__uint64_t TEX_BORDERWIDE_CHECKSUM;
__uint64_t TEX_LUT4D_CHECKSUM;	
__uint64_t TEX_LUT4D_CHECKSUM_EVEN;	
__uint64_t TEX_LUT4D_CHECKSUM_ODD;	
__uint64_t TEX_MDDMA_CHECKSUM;
__uint64_t TEX_MDDMA_CHECKSUM_EVEN;
__uint64_t TEX_MDDMA_CHECKSUM_ODD;
__uint64_t TEX_RETEXLUT_CHECKSUM;
#endif

__uint64_t TEX_1D_CHECKSUM =	0x1dfdfdddcull;
__uint64_t TEX_3D_CHECKSUM =	0x350c19042ull;
__uint64_t TEX_LOAD_CHECKSUM =	0x171a1b18; /* 0x5676ad6a */
__uint64_t TEX_SCISTRI_CHECKSUM =	0x3545d6458ull;
__uint64_t TEX_LINEGL_CHECKSUM =	0x17275726eull;
__uint64_t TEX_PERSP_CHECKSUM =	0xb1723170ull;
__uint64_t TEX_MAG_CHECKSUM =	0xc7d8e7d6;
__uint64_t TEX_DETAIL_CHECKSUM =	0x9e53d58c; /* 0x80037144 */
__uint64_t TEX_BORDERTALL_CHECKSUM =	0xec761b4e;
__uint64_t TEX_BORDERWIDE_CHECKSUM =	0x3d2c4b5eeull;
__uint64_t TEX_LUT4D_CHECKSUM =	0x1769ffa6dull;
__uint64_t TEX_LUT4D_CHECKSUM_EVEN =	0x10e2e30e0ull;
__uint64_t TEX_LUT4D_CHECKSUM_ODD =	0x1701caf18ull;
__uint64_t TEX_MDDMA_CHECKSUM =	0x10b2c4b29ull;
__uint64_t TEX_MDDMA_CHECKSUM_EVEN =	0x3293a4935ull;
__uint64_t TEX_MDDMA_CHECKSUM_ODD =	0x248894805ull;
__uint64_t TEX_RETEXLUT_CHECKSUM =	0x10b2c4b29ull;

extern __uint32_t check_re_revision(__uint32_t);
extern __uint32_t _retexlut(void);
extern void mg_restore_pps();
extern __uint32_t _mg_dac_crc_rss_pp(int, int, int, int, int, int, int, int);
extern __uint32_t write_only;
extern __uint64_t read_checksum;
extern __uint64_t read_checksum_even;
extern __uint64_t read_checksum_odd;

/**************************************************************************
 *                                                                        *
 *  mg_terev()							  	  *
 *  Draws a blah blah to differentiate rev B and rev D TE chips	  	  *
 *									  *
 **************************************************************************/

int
mg_terev(void)
{
	__uint32_t do_crc=0, errors = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;


        /* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	/* initialize to test everything */

        rssnum = 4;
        reReadMode = DMA_BURST;
        numCols = 76; numRows = 75;
	xstart = 242; ystart = 194;

        _draw_setup(DMA_PP1, 1);

        errors = _tex_terev();

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

	rssDmaPar->checksum_only = 1;

	/* readback will return the value for te_rev */
	errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
               	reReadMode, db_enable, rssDmaPar, hq3DmaPar, 0, 0, do_crc, 
		xstart, ystart, TE_REV_D_CHECKSUM, 0);

	msg_printf(DBG, "mg_terev: te_rev = %d\n", te_rev);
	switch (te_rev) {
		case TE_REV_B:
			TEX_1D_CHECKSUM = 	0x1dfdfdddcull;
			TEX_3D_CHECKSUM =	0x350c19042ull;
			TEX_LOAD_CHECKSUM =	0xc180c0; 	/* 0x1afc1b3a */
			TEX_SCISTRI_CHECKSUM =	0x1d2e1d0c;
			TEX_LINEGL_CHECKSUM =	0x17275726eull;
			TEX_PERSP_CHECKSUM =	0x83c48342;
			TEX_MAG_CHECKSUM =	0x192d412d0ull;
			TEX_DETAIL_CHECKSUM =	0x1690cc61aull;
			TEX_BORDERTALL_CHECKSUM =	0xec761b4e;
			TEX_BORDERWIDE_CHECKSUM =	0x3d2c4b5eeull;
			TEX_LUT4D_CHECKSUM =	0x1769ffa6dull;/* 0x1668589a6 */
			TEX_MDDMA_CHECKSUM =	0x10b2c4b29ull;/* 0x1419b1f67 */
			TEX_RETEXLUT_CHECKSUM =	0x10b2c4b29ull;
			break;
		case TE_REV_D:
			TEX_1D_CHECKSUM =	0x1dfdfdddcull;
			TEX_3D_CHECKSUM =	0x350c19042ull;
			TEX_LOAD_CHECKSUM =	0x171a1b18; /* 0x5676ad6a */
			TEX_SCISTRI_CHECKSUM =	0x3545d6458ull;
			TEX_LINEGL_CHECKSUM =	0x17275726eull;
			TEX_PERSP_CHECKSUM =	0xb1723170ull;
			TEX_MAG_CHECKSUM =	0xc7d8e7d6;
			TEX_DETAIL_CHECKSUM =	0x9e53d58c; /* 0x80037144 */
			TEX_BORDERTALL_CHECKSUM =	0xec761b4e;
			TEX_BORDERWIDE_CHECKSUM =	0x3d2c4b5eeull;
			TEX_LUT4D_CHECKSUM =	0x1769ffa6dull;
			TEX_MDDMA_CHECKSUM =	0x10b2c4b29ull;
			TEX_RETEXLUT_CHECKSUM =	0x10b2c4b29ull;
			break;
		default:
			msg_printf(ERR, "Cannot determine TE revision. Make sure that the TE chip on the RA and RB board have the same revision.\n");
			break;
	}

	return 0;
}


/**************************************************************************
 *                                                                        *
 *  mg_tex_poly()							  *
 *									  *
 * Draw geometry_opt43 in ide.						  *
 **************************************************************************/

__uint32_t
mgras_tex_poly(int argc, char ** argv)
{
	__uint32_t numCols, numRows, reReadMode, rssnum, errors = 0;
	__uint32_t stop_on_err = 1, bad_arg = 0, tex_en, rflag=0;
	__uint32_t do_crc = 0, db_enable = 1;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
#endif

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test...\n");
		return (0);
	}

	/* initialize to test everything */
	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	rssnum = 4;
	reReadMode = DMA_PIO;
	numCols = 40; numRows = 40;
	tex_en = 1;

	GET_RSS_NUM();

#if HQ4
	te_rev = TE_REV_D;
#else
	if (te_rev == TE_REV_UNDETERMINED)
		mg_terev();
#endif

	_draw_setup(DMA_PP1, 1);

	errors += _mg_tex_poly(tex_en);

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_PP1, 0);

	if (tex_en) {
	   errors += _readback(rssnum,numRows,numCols,tex_poly_array,reReadMode,
		db_enable, rssDmaPar, hq3DmaPar, 
		tex_poly_array_checksum & 0xffffffff,
		 TE1_TEX_POLY_CRC_INDEX, do_crc, 0, 0, 0, 0);
	}
	
	REPORT_PASS_OR_FAIL(
		(&TE_err[TE1_TEX_POLY_TEST]), errors);
}

__uint32_t
mgras_notex_poly(int argc, char ** argv)
{
	__uint32_t numCols, numRows, reReadMode, rssnum, errors = 0;
	__uint32_t stop_on_err = 1, bad_arg = 0, tex_en, rflag=0;
	__uint32_t do_crc = 0, db_enable = 1, x, y;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;
	__uint32_t notex_poly_array[1600];

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
#endif

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test...\n");
		return (0);
	}

	/* initialize to test everything */
	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	rssnum = 4;
	reReadMode = DMA_PIO;
	numCols = 40; numRows = 40;
	tex_en = 0;

	GET_RSS_NUM();

#if HQ4
	te_rev = TE_REV_D;
#else
	if (te_rev == TE_REV_UNDETERMINED)
		mg_terev();
#endif

	_draw_setup(DMA_PP1, 1);

	errors += _mg_tex_poly(tex_en);

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_PP1, 0);

	   /* rgba = all red, others = 0 */
	   for (y = 0; y < numRows; y++) {
		for (x = 0; x < numCols; x++) {
			notex_poly_array[x + y*numCols] = 0xff;
		}
	   }

	   errors += _readback(rssnum,numRows,numCols,notex_poly_array,
		reReadMode, db_enable, rssDmaPar, hq3DmaPar, 
		notex_poly_array_checksum & 0xffffffff,
		 TE1_NO_TEX_POLY_CRC_INDEX, do_crc, 0, 0, 0, 0);

	REPORT_PASS_OR_FAIL(
		(&TE_err[TE1_NO_TEX_POLY_TEST]), errors);
}

/* _mg_tex_poly  
 * 
 * Taken from geometry_opt43 in james2 script.
 *
 */

int
_mg_tex_poly(__uint32_t tex_en)
{

__uint32_t cfifo_val, cfifo_status, count, scanWidth, cmd, pix_cmd;
__uint32_t llx, lly, urx, ury, red, green, blue, alpha, do_compare;
__uint32_t actual;

	cfifo_val = 32;

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

/* Initializing the HQ for 64 byte scanlines */
FormatterMode(0x201);

scanWidth = 0x40;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

/* Begin__glMgrInitRGB */
WRITE_RSS_REG(rssnum, 0x110, 0x00004000, ~0x0);
WRITE_RSS_REG(rssnum, 0x161, 0x00000200, ~0x0);
WRITE_RSS_REG(rssnum, 0x16a, 0x00000000, ~0x0);
/* End__glMgrInitRGB */
WRITE_RSS_REG(rssnum, 0x110, 0x00005000, ~0x0);
if (numTRAMs == 1) { 
	WRITE_RSS_REG(rssnum, 0x111, 0x00040408, ~0x0);
} else if (numTRAMs == 4) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040448, ~0x0);
}
WRITE_RSS_REG(rssnum, 0x112, 0x00000004, ~0x0);
WRITE_RSS_REG(rssnum, 0x113, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x114, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x115, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x142, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x143, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x144, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x145, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x116, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x147, 0x000004ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x148, 0x000003ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x149, 0x000004ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14a, 0x000003ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14b, 0x000004ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14c, 0x000003ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14d, 0x000004ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14e, 0x000003ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x14f, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x156, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x157, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x159, 0x00040000, ~0x0);
WRITE_RSS_REG(rssnum, 0x15a, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x15b, 0x000f0000, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x192, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x182, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x184, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x185, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x187, 0x00000101, ~0x0);
WRITE_RSS_REG(rssnum, 0x188, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x189, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x18a, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x18b, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x194, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x160, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x161, 0x0c004203, ~0x0);
WRITE_RSS_REG(rssnum, 0x162, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x163, 0xffffffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x164, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x165, 0x80000001, ~0x0);
WRITE_RSS_REG(rssnum, 0x165, 0x58840000, ~0x0);
WRITE_RSS_REG(rssnum, 0x166, 0x00000007, ~0x0);
WRITE_RSS_REG(rssnum, 0x167, 0x0000ffff, ~0x0);
WRITE_RSS_REG(rssnum, 0x168, 0x0ffffff1, ~0x0);
WRITE_RSS_REG(rssnum, 0x169, 0x00000007, ~0x0);
WRITE_RSS_REG(rssnum, 0x16a, 0x00000000, ~0x0);
#if 0
WRITE_RSS_REG(rssnum, 0x16d, 0x000c8240, ~0x0);
WRITE_RSS_REG(rssnum, 0x16e, 0x0000031e, ~0x0);
#endif
WRITE_RSS_REG_DBL(rssnum, 0x268, 0x00000000, 0x00000000, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15c, 0x40000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x03084111, ~0x0);
WRITE_RSS_REG(rssnum, 0x15c, 0x50000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00004111, ~0x0);
WRITE_RSS_REG(rssnum, 0x15c, 0xb0000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000fb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000f7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000f3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ef, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000eb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000e7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000e3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000df, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000db, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000d7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000d3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000cf, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000cb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000c7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000c3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000bf, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000bb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000b7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000b3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000af, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ab, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000a7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000a3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000009f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000009b, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15d, 0x00000097, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000093, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000008f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000008b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000087, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000083, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000007f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000007b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000077, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000073, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000006f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000006b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000067, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000063, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000005f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000005b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000057, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000053, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000004f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000004b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000047, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000043, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000003f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000003b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000037, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000033, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000002f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000002b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000027, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000023, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000001f, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15d, 0x0000001b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000017, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000013, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000000f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000000b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000007, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000003, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ff, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000fb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000f7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000f3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ef, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000eb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000e7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000e3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000df, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000db, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000d7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000d3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000cf, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000cb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000c7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000c3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000bf, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000bb, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000b7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000b3, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000af, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000ab, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000a7, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x000000a3, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15d, 0x0000009f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000009b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000097, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000093, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000008f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000008b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000087, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000083, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000007f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000007b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000077, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000073, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000006f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000006b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000067, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000063, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000005f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000005b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000057, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000053, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000004f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000004b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000047, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000043, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000003f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000003b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000037, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000033, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000002f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000002b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000027, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15d, 0x00000023, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000001f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000001b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000017, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000013, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000000f, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x0000000b, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000007, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000003, ~0x0);
WRITE_RSS_REG(rssnum, 0x15c, 0xb0000080, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x15d, 0x00000100, ~0x0);
if (numTRAMs == 1) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040408, ~0x0);
} else if (numTRAMs == 4) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040448, ~0x0);
}
WRITE_RSS_REG(rssnum, 0x1a1, 0x00004022, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a7, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a3, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a4, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a3, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a5, 0x00000001, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a2, 0x00000550, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a9, 0x00000020, ~0x0);
WRITE_RSS_REG(rssnum, 0x1aa, 0x00000020, ~0x0);
WRITE_RSS_REG(rssnum, 0x1ab, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x1ac, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x159, 0x000000a0, ~0x0);
WRITE_RSS_REG(rssnum, 0x153, 0x00200020, ~0x0);
WRITE_RSS_REG(rssnum, 0x158, 0x00200020, ~0x0);
#if 0
WRITE_RSS_REG(rssnum, 0x11f, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x11f, 0x00000001, ~0x0);
WRITE_RSS_REG(rssnum, 0x11f, 0x00000002, ~0x0);
#endif
	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG_DBL(rssnum, 0x102, 0x00005555, 0x00000005, ~0x0);

count = 0x40;
pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, count);
HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff88fff4, 0xffffffff, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xffffffff, 0xffffffff, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xfffffff6, 0xffd9ffb8, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff9fff7d, 0xff69ff67, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff68ff68, 0xff6bff73, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7bff88, 0xffafffe0, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xfffeffff, 0xffffffff, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xfffeffe6, 0xffa8ff3d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff16ff32, 0xff8affd0, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xfffdffff, 0xfffffffe, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xffd8ffb0, 0xff9aff80, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6fff61, 0xff5cff5b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff5aff5b, 0xff5bff64, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff69ff67, 0xff64ff66, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7cffc0, 0xfff5ffe2, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff9dff62, 0xff26ff11, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff13ff1a, 0xff34ff51, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff50ffb6, 0xfffdffc1, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xffa1ff8f, 0xff7fff6d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6cff69, 0xff62ff5d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff5fff5e, 0xff5cff5e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff56ff49, 0xff3aff2f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff26ff24, 0xff49ff5e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff52ff30, 0xff16ff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0fff13, 0xff1aff53, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3bff27, 0xff54ff45, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff43ff5b, 0xff73ff72, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff79ff6e, 0xff60ff5f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff5fff5d, 0xff5eff5e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff46ff28, 0xff1cff1c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff1a, 0xff19ff33, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3bff1b, 0xff0fff0d, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0fff10, 0xff16ff3f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3dff34, 0xff20ff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff13ff1a, 0xff65ff72, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7aff7d, 0xff74ff75, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7dff6c, 0xff5bff5c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3cff12, 0xff14ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff24ff1b, 0xff1dff32, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2cff17, 0xff1fff10, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0fff10, 0xff10ff2a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3aff36, 0xff37ff1b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff11, 0xff5bff74, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff88ff8c, 0xff81ff82, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff85ff7f, 0xff6fff5d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff34ff0e, 0xff13ff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff20ff19, 0xff1cff30, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff11, 0xff1aff12, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff10, 0xff0fff24, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff30ff33, 0xff39ff1f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff13, 0xff5fff72, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff96ff96, 0xff7bff73, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7dff83, 0xff78ff6a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3fff0e, 0xff12ff23, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff22ff19, 0xff1cff2f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff11, 0xff10ff0f, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0f, 0xff0fff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2cff2f, 0xff30ff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff45, 0xff68ff74, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff8bff6c, 0xff62ff61, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff68ff75, 0xff6bff6c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff67ff25, 0xff12ff27, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2bff18, 0xff21ff2c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff14ff11, 0xff10ff0e, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0d, 0xff0eff1a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff26ff29, 0xff2bff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2cff69, 0xff6aff62, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff54ff61, 0xff63ff58, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff53ff4a, 0xff50ff66, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff68ff55, 0xff15ff29, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff32ff18, 0xff29ff26, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0fff0f, 0xff0eff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0c, 0xff0cff19, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff22ff21, 0xff26ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff4fff64, 0xff58ff49, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff67ff7d, 0xff6dff68, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6fff6c, 0xff50ff53, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff63ff63, 0xff25ff2a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2bff19, 0xff31ff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0eff0e, 0xff0eff0e, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0bff12, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff20ff1d, 0xff1eff1b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff64ff6d, 0xff66ff5b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6cff61, 0xff54ff58, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff58ff65, 0xff63ff70, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6fff68, 0xff32ff24, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff20ff1a, 0xff36ff18, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0e, 0xff0fff0e, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0c, 0xff0cff0e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1bff1b, 0xff16ff17, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff75ff95, 0xff92ff78, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff68ffac, 0xff9dff97, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff8dff6b, 0xff7dff8b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff89ff78, 0xff50ff2a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff19ff1b, 0xff35ff12, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0e, 0xff10ff0f, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0b, 0xff0cff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff1a, 0xff12ff49, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff92ffb3, 0xffb7ffa1, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff90ffa3, 0xffbfffac, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff7fff80, 0xff8dff91, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff8eff7d, 0xff66ff52, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff15ff1a, 0xff2cff10, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0eff0e, 0xff0eff0e, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0c, 0xff0bff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff15ff1a, 0xff15ff68, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff92ffb0, 0xffa6ff81, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff75ff87, 0xffa5ff97, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6bff60, 0xff61ff73, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff83ff7b, 0xff64ff40, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff1a, 0xff20ff10, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0eff0d, 0xff0dff0d, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0a, 0xff0bff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff15ff1b, 0xff15ff40, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff98ffab, 0xff7bff4c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff38ff44, 0xff7eff81, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff46ff37, 0xff39ff4f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff6cff7c, 0xff5dff2c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff19, 0xff18ff0e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0d, 0xff0cff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0a, 0xff0aff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff13ff1b, 0xff15ff31, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff8cff69, 0xff39ff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff2d, 0xff66ff6a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff41ff23, 0xff1dff26, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3cff63, 0xff4bff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff19ff16, 0xff14ff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0cff0d, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0a, 0xff0aff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff10ff1c, 0xff16ff27, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff67ff51, 0xff37ff2c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff28ff3d, 0xff5bff63, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff4dff3a, 0xff2dff33, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3cff40, 0xff2bff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff27ff16, 0xff10ff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0bff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff08, 0xff09ff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff1d, 0xff20ff2b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff48ff5c, 0xff54ff43, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff37ff50, 0xff65ff69, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff63ff5a, 0xff4bff35, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff30ff2e, 0xff21ff1b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff10ff16, 0xff0eff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0aff0c, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff07, 0xff08ff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff1a, 0xff18ff32, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3bff5e, 0xff73ff56, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff51ff3e, 0xff5dff63, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff59ff4f, 0xff3eff35, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2eff28, 0xff1dff15, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff14ff17, 0xff0cff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0bff0a, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff09, 0xff09ff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff16, 0xff16ff23, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff30ff4f, 0xff5eff4f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff45ff43, 0xff41ff56, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff4cff3d, 0xff35ff34, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2aff1e, 0xff15ff19, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff19ff17, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0aff0a, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff09, 0xff09ff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff11, 0xff1aff1b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff26ff42, 0xff58ff4c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff46ff4b, 0xff3dff47, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff46ff33, 0xff38ff32, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2aff1c, 0xff18ff1b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1eff13, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0bff0a, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff08, 0xff08ff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0b, 0xff1eff13, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff2b, 0xff47ff4c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff47ff4c, 0xff45ff38, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff3cff33, 0xff33ff2e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff23ff19, 0xff18ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1dff0f, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0c, 0xff0bff0a, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff09, 0xff09ff08, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff09, 0xff19ff15, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff15ff21, 0xff32ff2d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff38ff47, 0xff3cff28, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff33ff32, 0xff24ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1bff18, 0xff16ff22, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1bff0c, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0bff0a, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff09, 0xff09ff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff08, 0xff0eff1c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff11ff11, 0xff1fff23, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1bff2d, 0xff2cff24, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1eff19, 0xff1eff1d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1bff18, 0xff18ff22, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff14ff0d, 0xff0eff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0bff0b, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0a, 0xff09ff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff08, 0xff0aff18, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff11, 0xff19ff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff19ff2c, 0xff29ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff1c, 0xff1bff1f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff17, 0xff21ff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0dff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0eff0b, 0xff0aff0b, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0a, 0xff09ff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff09, 0xff09ff0f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1fff18, 0xff1cff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff17ff29, 0xff31ff1e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff18ff1a, 0xff1fff20, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff22, 0xff24ff14, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0d, 0xff0dff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0aff0a, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0a, 0xff0aff09, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff08ff09, 0xff09ff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff27, 0xff25ff28, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1cff1e, 0xff2bff24, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff24ff24, 0xff23ff21, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff28ff28, 0xff17ff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0c, 0xff0cff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0c, 0xff0cff0b, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0a, 0xff0aff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0a, 0xff0aff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff22, 0xff2dff2e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff23ff1a, 0xff1eff26, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff29ff25, 0xff26ff2b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff2bff1b, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0bff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0d, 0xff10ff0c, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0a, 0xff0dff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff09, 0xff0aff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0e, 0xff26ff38, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff30ff1f, 0xff20ff25, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff29ff2e, 0xff2fff29, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff1aff0d, 0xff0cff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0bff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0b, 0xff0dff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0b, 0xff10ff12, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0a, 0xff0bff0a, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0fff25, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff33ff24, 0xff1dff28, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff35ff2d, 0xff24ff15, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0bff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0cff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0cff0d, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff09ff0b, 0xff0cff0e, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0aff0c, 0xff0bff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0bff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff17ff22, 0xff1dff13, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff19ff12, 0xff0eff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0b, 0xff0bff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0d, 0xff0cff0b, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0b, 0xff0bff0d, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0bff0d, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0cff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0fff0e, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff10, 0xff12ff0f, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0eff0e, 0xff0dff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0d, 0xff0cff0c, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0dff0d, 0xff0dff0d, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xff0cff0c, 0xff0cff0d, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

#if 0
WRITE_RSS_REG(rssnum, 0x11f, 0x00000003, ~0x0);
#endif
_mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);

if (tex_en == 0) { /* no texture */
	tri_dith_en = 0;
	llx = lly = 0;
	urx = ury = 40;
	red = 100;
	blue = green = alpha = 0;
	do_compare = 0;
	_mg0_rect(llx,lly,urx,ury,red,green, blue, alpha, do_compare, 1);
	tri_dith_en = 1;
}
else { /* do tex poly */
if (numTRAMs == 1) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040408, ~0x0);
} else if (numTRAMs == 4) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040448, ~0x0);
}
WRITE_RSS_REG(rssnum, 0x18c, 0x00040020, ~0x0);
WRITE_RSS_REG(rssnum, 0x180, 0x000141c4, ~0x0);
WRITE_RSS_REG(rssnum, 0x186, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x182, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x184, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x185, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x188, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x189, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x181, 0x00005555, ~0x0);
WRITE_RSS_REG(rssnum, 0x183, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x190, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x191, 0x00000100, ~0x0);
WRITE_RSS_REG(rssnum, 0x190, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x193, 0x00000001, ~0x0);
WRITE_RSS_REG(rssnum, 0x18b, 0x00000000, ~0x0);
if (numTRAMs == 1) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040408, ~0x0);
} else if (numTRAMs == 4) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040448, ~0x0);
}
WRITE_RSS_REG(rssnum, 0x180, 0x000141c4, ~0x0);
WRITE_RSS_REG(rssnum, 0x183, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x180, 0x000141c4, ~0x0);
WRITE_RSS_REG(rssnum, 0x180, 0x000141c4, ~0x0);
WRITE_RSS_REG(rssnum, 0x180, 0x00014084, ~0x0);
WRITE_RSS_REG(rssnum, 0x180, 0x00014084, ~0x0);
WRITE_RSS_REG(rssnum, 0x184, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x185, 0x00fffe66, ~0x0);
WRITE_RSS_REG(rssnum, 0x142, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x143, 0x00000000, ~0x0);
if (numTRAMs == 1) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040409, ~0x0);
} else if (numTRAMs == 4) {
	WRITE_RSS_REG(rssnum, 0x111, 0x00040449, ~0x0);
}

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

/* BeginTriangle */
#if 0
WRITE_RSS_REG(rssnum, 0x04, 0x47411380, ~0x0);
WRITE_RSS_REG(rssnum, 0x05, 0x4740eb80, ~0x0);
#endif
WRITE_RSS_REG(rssnum, 0x04, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x05, 0x473fff80, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x206, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x208, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x20a, 0x00000000, 0x01000000, ~0x0);
#if 0
WRITE_RSS_REG(rssnum, 0x00, 0x47411380, ~0x0);
WRITE_RSS_REG(rssnum, 0x01, 0x47411380, ~0x0);
WRITE_RSS_REG(rssnum, 0x02, 0x4740ec00, ~0x0);
WRITE_RSS_REG(rssnum, 0x03, 0x47411380, ~0x0);
#endif
WRITE_RSS_REG(rssnum, 0x00, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x01, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x02, 0x47000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x03, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x5c, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5d, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5e, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5f, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x60, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x61, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x62, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x63, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x264, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x65, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x66, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x67, 0x00000000, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}

WRITE_RSS_REG_DBL(rssnum, 0x2c8, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2ca, 0xffffffe0, 0x00080000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x28a, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x280, 0x0000086f, 0xfde40000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x282, 0x0000086f, 0xfde40000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x284, 0x00000000, 0x3ffff000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c4, 0xffffffe0, 0x00080000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c6, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x288, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c0, 0x0000001f, 0xfff80000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c2, 0xffffffe0, 0x00080000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x286, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x08c, 0x0000003f, ~0x0);
READ_RSS_REG(0x0, STATUS, 0, ~0x0);
msg_printf(DBG, "status 0x%x\n", actual);

HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x1, ~0x0);
/* EndTriangle */
/* BeginTriangle */

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
	}
#if 0
WRITE_RSS_REG(rssnum, 0x04, 0x4740eb80, ~0x0);
WRITE_RSS_REG(rssnum, 0x05, 0x4740eb80, ~0x0);
#endif
WRITE_RSS_REG(rssnum, 0x04, 0x473fff80, ~0x0);
WRITE_RSS_REG(rssnum, 0x05, 0x473fff80, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x206, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x208, 0x00000000, 0x01000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x20a, 0x00000000, 0x00000000, ~0x0);
#if 0
WRITE_RSS_REG(rssnum, 0x00, 0x4740eb80, ~0x0);
WRITE_RSS_REG(rssnum, 0x01, 0x4740ec00, ~0x0);
WRITE_RSS_REG(rssnum, 0x02, 0x47411380, ~0x0);
WRITE_RSS_REG(rssnum, 0x03, 0x47411380, ~0x0);
#endif
WRITE_RSS_REG(rssnum, 0x00, 0x473fff80, ~0x0);
WRITE_RSS_REG(rssnum, 0x01, 0x47400000, ~0x0);
WRITE_RSS_REG(rssnum, 0x02, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x03, 0x47402780, ~0x0);
WRITE_RSS_REG(rssnum, 0x5c, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5d, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5e, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x5f, 0x00fff000, ~0x0);
WRITE_RSS_REG(rssnum, 0x60, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x61, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x62, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x63, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x64, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x65, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x66, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x67, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c8, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2ca, 0xffffffe0, 0x00080000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x28a, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x280, 0x0000038f, 0xff1c0000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x282, 0x0000086f, 0xfde40000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x284, 0x00000000, 0x3ffff000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c4, 0x0000001f, 0xfff80000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c6, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x288, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c0, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x2c2, 0xffffffe0, 0x00080000, ~0x0);
WRITE_RSS_REG_DBL(rssnum, 0x286, 0x00000000, 0x00000000, ~0x0);
WRITE_RSS_REG(rssnum, 0x08c, 0x0000003f, ~0x0);
READ_RSS_REG(0x0, STATUS, 0, ~0x0);
msg_printf(DBG, "status 0x%x\n", actual);
HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x0, ~0x0);
} /* of do tex poly */
return 0;
}

int
_ldrsmpl_poly(void)
{
__uint32_t cfifo_val, cfifo_status, count, scanWidth, cmd, pix_cmd;

        cfifo_val = 32;

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

/* Initializing the HQ for 64 byte scanlines */
FormatterMode(0x201);

scanWidth = 0x8;
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_SCAN_WIDTH, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

/* dma write */
cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, REIF_DMA_TYPE, 4);
HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
HQ3_PIO_WR(CFIFO1_ADDR, 0x4, ~0x0);

if (numTRAMs == 1) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040480, ~0x0);
} else if (numTRAMs == 4) {
        WRITE_RSS_REG(rssnum, 0x111, 0x000404c0, ~0x0);
};

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004e20);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000230);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00000090);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00040008);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00040008);
#if 0
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);
#endif
	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

/* setup rbuffer, wbuffer */
WRITE_RSS_REG(rssnum, 0x18b, 0x0, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a0, 0x1, ~0x0);

WRITE_RSS_REG_DBL(rssnum, 0x102, 0x00005555, 0x00000005, ~0x0);


count = 0x8;
pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, count);
HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0x55555555, 0x55555555, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0x55555555, 0x55555555, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0x55555555, 0x55555555, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0x55555555, 0x55555555, ~0x0);

#if 0
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
#endif
_mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);

WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

/* setup rbuffer, wbuffer */
WRITE_RSS_REG(rssnum, 0x18b, 0x1, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a0, 0x0, ~0x0);

hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00002323);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00010004);
hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00000008);
if (numTRAMs == 1) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040401, ~0x0);
} else if (numTRAMs == 4) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040441, ~0x0);
};

hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00010194);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x473fff80,0x473fff80);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xfd800000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x4744ff80,0x4744fe40);
hq_HQ_RE_WrReg_1(0x202,0x473fff80,0x4741ff80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_2(0x280,0x00009fef,0xd8040000);
hq_HQ_RE_WrReg_2(0x282,0x00003fef,0xf0040000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x0000001f,0xfff80000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x1, ~0x0);

hq_HQ_RE_WrReg_1(0x204,0x4741ff80,0x473fff80);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xfd800000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x4744fe40,0x4744ff80);
hq_HQ_RE_WrReg_1(0x202,0x473fff80,0x4741ff80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x00009fcf,0xd80c0000);
hq_HQ_RE_WrReg_2(0x282,0x00003fef,0xf0040000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0xffffffc0,0x00100000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x1, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

if (numTRAMs == 1) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040480, ~0x0);
} else if (numTRAMs == 4) {
        WRITE_RSS_REG(rssnum, 0x111, 0x000404c0, ~0x0);
};

hq_HQ_RE_WrReg_0(0x1a1,0x00000000,0x00004e20);
hq_HQ_RE_WrReg_0(0x1a2,0x00000000,0x00000230);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a4,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a3,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a5,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1a9,0x00000000,0x00000008);
hq_HQ_RE_WrReg_0(0x1aa,0x00000000,0x00000004);
hq_HQ_RE_WrReg_0(0x1ab,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x1ac,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x159,0x00000000,0x00000090);
hq_HQ_RE_WrReg_0(0x153,0x00000000,0x00040008);
hq_HQ_RE_WrReg_0(0x158,0x00000000,0x00040008);
#if 0
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000001);
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000002);
#endif

WRITE_RSS_REG_DBL(rssnum, 0x102, 0x00005555, 0x00000005, ~0x0);


count = 0x8;
pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 2, count);
HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xaaaaaaaa, 0xaaaaaaaa, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xaaaaaaaa, 0xaaaaaaaa, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xaaaaaaaa, 0xaaaaaaaa, ~0x0);

HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
HQ3_PIO_WR64(CFIFO1_ADDR, 0xaaaaaaaa, 0xaaaaaaaa, ~0x0);

#if 0
hq_HQ_RE_WrReg_0(0x11f,0x00000000,0x00000003);
#endif

_mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);

/* setup rbuffer, wbuffer */
WRITE_RSS_REG(rssnum, 0x18b, 0x0, ~0x0);
WRITE_RSS_REG(rssnum, 0x1a0, 0x1, ~0x0);

hq_HQ_RE_WrReg_0(0x181,0x00000000,0x00002323);
hq_HQ_RE_WrReg_0(0x183,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x18c,0x00000000,0x00010004);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

if (numTRAMs == 1) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040401, ~0x0);
} else if (numTRAMs == 4) {
        WRITE_RSS_REG(rssnum, 0x111, 0x00040441, ~0x0);
};

hq_HQ_RE_WrReg_0(0x180,0x00000000,0x00010194);
hq_HQ_RE_WrReg_0(0x184,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x185,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x190,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x191,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x204,0x4741ff80,0x4741ff80);
hq_HQ_RE_WrReg_2(0x206,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x208,0xffffffff,0xfd800000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x4744ff80,0x4744fe40);
hq_HQ_RE_WrReg_1(0x202,0x473fff80,0x4743ff80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x00009fef,0xd8040000);
hq_HQ_RE_WrReg_2(0x282,0x00003fef,0xf0040000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0x0000001f,0xfff80000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x1, ~0x0);

	WAIT_FOR_CFIFO(cfifo_status, cfifo_val, CFIFO_TIMEOUT_VAL);
        if (cfifo_status == CFIFO_TIME_OUT) {
                msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                return (-1);
        }

hq_HQ_RE_WrReg_1(0x204,0x4743ff80,0x4741ff80);
hq_HQ_RE_WrReg_2(0x206,0xffffffff,0xfd800000);
hq_HQ_RE_WrReg_2(0x208,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x20a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x200,0x4744fe40,0x4744ff80);
hq_HQ_RE_WrReg_1(0x202,0x473fff80,0x4743ff80);
hq_HQ_RE_WrReg_1(0x25c,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x25e,0x00fff000,0x00fff000);
hq_HQ_RE_WrReg_1(0x260,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x262,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x264,0x00000000,0x00000000);
hq_HQ_RE_WrReg_1(0x266,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c8,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2ca,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x28a,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x280,0x00009fcf,0xd80c0000);
hq_HQ_RE_WrReg_2(0x282,0x00003fef,0xf0040000);
hq_HQ_RE_WrReg_2(0x284,0x00000000,0x3ffff000);
hq_HQ_RE_WrReg_2(0x2c4,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x2c6,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x288,0x00000000,0x00000000);
hq_HQ_RE_WrReg_2(0x2c0,0xffffffc0,0x00100000);
hq_HQ_RE_WrReg_2(0x2c2,0xffffffe0,0x00080000);
hq_HQ_RE_WrReg_2(0x286,0x00000000,0x00000000);
hq_HQ_RE_WrReg_0(0x08c,0x00000000,0x0000003f);
HQ3_PIO_WR_RE_EX((RSS_BASE + HQ_RSS_SPACE(0x13)), 0x1, ~0x0);

	return 0;
}


#ifndef MGRAS_DIAG_SIM
__uint32_t
mg_tex_3d(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();

		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */
        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 100; numRows = 100;
		xstart = ystart = 206;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_3d();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors += _readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_TEX3D_CRC_INDEX, do_crc, xstart, 
			ystart,TEX_3D_CHECKSUM, 0);

        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_TEX3D_TEST]), errors);

}


__uint32_t
mg_tex_1d(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 324; numRows = 382;
		xstart = 94; ystart = 65;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_1d();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors += _readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_TEX1D_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_1D_CHECKSUM, 0);
        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_TEX1D_TEST]), errors);

}

__uint32_t
mg_tex_load(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 4) {
                msg_printf(SUM, "This test requires a 4-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 98; numRows = 128;
		xstart = 196; ystart = 207;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_load();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors += _readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_LOAD_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_LOAD_CHECKSUM, 0);

        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_LOAD_TEST]), errors);
}

__uint32_t
mg_tex_scistri(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 100; numRows = 100;
		xstart = 195; ystart = 195;

        	_draw_setup(DMA_PP1, 1);
        	errors = _tex_scistri();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_SCISTRI_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_SCISTRI_CHECKSUM, 0);

        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_SCISTRI_TEST]), errors);
}

__uint32_t
mg_tex_linegl(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 72; numRows = 72;
		xstart = 220; ystart = 220;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_linegl();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_LINEGL_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_LINEGL_CHECKSUM, 0);
        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_LINEGL_TEST]), errors);
}

__uint32_t
mg_tex_persp(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 124; numRows = 98;
		xstart = 194; ystart = 194;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_persp();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_PERSP_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_PERSP_CHECKSUM, 0);
        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_PERSP_TEST]), errors);

}

__uint32_t
mg_tex_mag(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 224; numRows = 159;
		xstart = 144; ystart = 208;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_mag();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_MAG_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_MAG_CHECKSUM, 0);

        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_MAG_TEST]), errors);
}

__uint32_t
mg_tex_detail(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 436; numRows = 284;
		xstart = 39; ystart = 39;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_detail();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, ILLEGAL_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_DETAIL_CHECKSUM, 0);

        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_DETAIL_TEST]), errors);
}


__uint32_t
mg_tex_bordertall(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 98; numRows = 291;
		xstart = 196; ystart = 221;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_border_8Khigh();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols,DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_BORDERTALL_CRC_INDEX, do_crc, 
			xstart, ystart, TEX_BORDERTALL_CHECKSUM, 0);
        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_8KHIGH_TEST]), errors);
}

__uint32_t
mg_tex_borderwide(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 1) {
                msg_printf(SUM, "This test requires a 1-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 292; numRows = 98;
		xstart = 220; ystart = 196;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_8Kwide();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
		errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_BORDERWIDE_CRC_INDEX, do_crc, 
			xstart, ystart, TEX_BORDERWIDE_CHECKSUM, 0);
        }

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_8KWIDE_TEST]), errors);
}

__uint32_t
mg_tex_lut4d(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 4) {
                msg_printf(SUM, "This test requires a 4-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();

#if HQ4
		te_rev = TE_REV_D;
#else
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();
#endif

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 256; numRows = 256;
		xstart = 0; ystart = 0;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_lut4d();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
#if (defined(IP22) || defined(IP28))
		errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_LUT4D_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_LUT4D_CHECKSUM, 0);
#else
		errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_LUT4D_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_LUT4D_CHECKSUM_EVEN,TEX_LUT4D_CHECKSUM_ODD);
#endif

        }

	/* reset config register */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);

        REPORT_PASS_OR_FAIL(
                (&TE_err[TEX_LUT4D_TEST]), errors);
}

__uint32_t
mg_tex_mddma(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif
        if (numTRAMs != 4) {
                msg_printf(SUM, "This test requires a 4-TRAM system.\n");
		return (0);
        }
        else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();

#if HQ4
		te_rev = TE_REV_D;
#else
		if (te_rev == TE_REV_UNDETERMINED)
			mg_terev();
#endif

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 256; numRows = 256;
		xstart = 0; ystart = 0;

        	_draw_setup(DMA_PP1, 1);

        	errors = _tex_mddma();

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar,
                        DMA_PP1, 0);	

		rssDmaPar->checksum_only = 1;
#if (defined(IP22) || defined(IP28))
		errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_MDDMA_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_MDDMA_CHECKSUM, 0);
#else
		errors +=_readback(rssnum,numRows,numCols, DMA_RDTile,
                	reReadMode, db_enable, rssDmaPar, hq3DmaPar,
                	0 & 0xffffffff, TEX_MDDMA_CRC_INDEX, do_crc, xstart, 
			ystart, TEX_MDDMA_CHECKSUM_EVEN,TEX_MDDMA_CHECKSUM_ODD);
#endif

        }

	/* reset config register */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);

        REPORT_PASS_OR_FAIL((&TE_err[TEX_MDDMA_TEST]), errors);
}
#endif


__uint32_t
mg_retexlut(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t xstart, ystart;	
 	__uint32_t stop_on_err = 1, db_enable = 0;
	__uint32_t numCols, numRows, reReadMode;
        RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
        HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

        NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
        numTRAMs = 1;
#endif

	if (check_re_revision(0)) {
           if (numTRAMs != 4) {
                msg_printf(SUM, "This test requires a 4-TRAM system.\n");
		return (0);
           }
           else {
        	/* initialize to test everything */
        	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

        	GET_RSS_NUM();

		/* initialize to test everything */

        	rssnum = 4;
        	reReadMode = DMA_BURST;
        	numCols = 256; numRows = 256;
		xstart = 0; ystart = 0;

        	_draw_setup(DMA_PP1, 1);

#ifdef MFG_USED
		mgras_VC3ClearCursor();
#endif

		/* draw the picture */
        	errors = _retexlut();
           }
	}
	else {
		msg_printf(SUM, "This revision of the RE cannot perform a texture LUT operation.\n");
		msg_printf(SUM, "Skipping this test.\n");
		return (0);
	}

	/* reset config register */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);

        return (0);
	
/* doesn't work yet */
#if 0

	if ((write_only == 0) && (nocompare == 0)) { /* do compare */
	   errors += _mg_dac_crc_rss_pp(0, 0,0x78,0x19b,0x1c3,0x358,0x19,0x1cb);
	   if ((stop_on_err == 1) && (errors)) {
		/* reset config register */
		HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
			CONFIG_RE4_ALL, TWELVEBIT_MASK);

		mg_restore_pps();
        	REPORT_PASS_OR_FAIL((&TE_err[TE1_LUT_TEST]), errors);
	   }

	   errors += _mg_dac_crc_rss_pp(0, 1, 0xaf,0x302,0x158,0x35e,0x67,0xfa);
	   if ((stop_on_err == 1) && (errors)) {
		/* reset config register */
		HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
			CONFIG_RE4_ALL, TWELVEBIT_MASK);

		mg_restore_pps();
        	REPORT_PASS_OR_FAIL((&TE_err[TE1_LUT_TEST]), errors);
	   }

	   if (numRE4s == 2) {
	   	errors += _mg_dac_crc_rss_pp(1, 0, 0, 0, 0, 0x78, 0x19b, 0x1c3);
	   	if ((stop_on_err == 1) && (errors)) {
			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();
        		REPORT_PASS_OR_FAIL((&TE_err[TE1_LUT_TEST]), errors);
		}

	   	errors += _mg_dac_crc_rss_pp(1, 1, 0, 0, 0, 0xaf, 0x302, 0x158);
	   	if ((stop_on_err == 1) && (errors)) {
			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), 
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();
        		REPORT_PASS_OR_FAIL((&TE_err[TE1_LUT_TEST]), errors);
	   	}
	   }

	   mg_restore_pps();
	}

        REPORT_PASS_OR_FAIL((&TE_err[TE1_LUT_TEST]), errors);
#endif
}
