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
#include <libsc.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "ide_msg.h"

extern __uint32_t global_startx;
extern __uint32_t global_starty;
extern void _mgras_disp_rdram_error();

/**************************************************************************
 *                                                                        *
 *  mgras_logicop()							  *
 *  Draw points to exercise the logic ops in the pp1.			  *
 *									  *
 **************************************************************************/

int
mgras_logicop(int argc, char ** argv)
{
	__uint32_t cmd, i, numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0, ci_en = 1;
	__uint32_t stop_on_err, num_logicops = 16, color0, color1, color2;
	__uint32_t do_crc=0, rflag = 0, bad_arg = 0, oldErrors = 0;
	__uint32_t cfifo_status, local_nocompare;
	float xstart, ystart;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;
	ppfillmodeu	ppfillmode = {0};

	/*****************************************************************
	 * We read back the data as if it were 8888-abgr, but we write it
 	 * in CI-12 mode. So here are the conversions:
	 *
	 * BRGB	RGBR GBRG
	 * 0000 0011 1111
	 * 8889	9900 0111	CI	B G R
	 * --------------	--	------
	 *	0001 1111	1f	88cccc
	 *	0010 0000	20	440000
  	 *	0011 1111	3f	cccccc
	 *	0100 0000	40	002200
	 *	0101 1111	5f	88eecc
	 *	0110 0000	60	442200
	 *	0111 1111	7f	cceecc
	 * 1111 1000 0000	f80	331133
	 * 1111 1001 1111	f9f	bbddff
	 * 1111 1010 0000	fa0	771133
	 * 1111 1011 1111	fbf	ffddff
	 * 1111 1100 0000	fc0	333333
	 * 1111 1101 1111	fdf	bbffff
	 * 1111 1110 0000	fe0	773333
	 * 1111 1111 1111	fff	ffffff
	 *			0	000000
	 *****************************************************************/

	__uint32_t logicop_array[48] = {
	0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 0x88eecc, 
	0x0, 0x0, 0xcccccc, 0xcccccc, 0x0, 0x0, 0xcccccc, 0xcccccc, 0x333333, 0x333333, 0xffffff, 0xffffff, 0x333333, 0x333333, 0xffffff, 0xffffff, 
	0x0, 0x88cccc, 0x440000,0xcccccc, 0x2200, 0x88eecc, 0x442200, 0xcceecc, 0x331133, 0xbbddff, 0x771133, 0xffddff, 0x333333, 0xbbffff, 0x773333, 0xffffff};
	

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
#endif

	/* initialize to test everything */
	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;
	pp_drb = PP_1_RSS_DRB_AB; /* default */

	/* pp_drb can be set with -b switch */
	GET_RSS_NUM();

	local_nocompare = nocompare;
	color0 = 0;
	color1 = 0x3f;
	color2 = 0x5f;

	reReadMode = DMA_PIO;
	rssnum = 0;
	stop_on_err = 1;
	numCols = 16; numRows = 3;  /* stay on 1 page in fb memory */

	/* this write to green makes logicop pass after a tex test */
	WRITE_RSS_REG(rssnum, RE_GREEN, 0x0, ~0x0);

	   _re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
		DMA_PP1, 1);

	   _load_aa_tables();

	   xstart = ystart = 0.0;

#if 0
	   /* draw 3 rows of color0 (=0) pixels */
	   for (ystart = 0.0; y < 3.0; y++) 
	   	for (xstart = 0.0; x < num_logicops; x++)
		   _mgras_pt(xstart,ystart,0.0,0.0,0.0,0.0,0, 0, ci_en, color0);
#endif

	   /* draw the pixels */
	   for (i = 0; i < num_logicops; i++) {
		WAIT_FOR_CFIFO(cfifo_status, 32, CFIFO_TIMEOUT_VAL);
                if (cfifo_status == CFIFO_TIME_OUT) {
                       msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       return (-1);
                }

	   	ppfillmode.bits.DitherEnab = 0;
	   	ppfillmode.bits.DitherEnabA = 0;
	   	ppfillmode.bits.LogicOpEnab = 1;
	   	ppfillmode.bits.PixType = PP_FILLMODE_CI12;
	   	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA;
	   	ppfillmode.bits.CIclamp = 1;

	   	ppfillmode.bits.LogicOP = PP_FILLMODE_LO_SRC;

	   	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,
			(0x1800+(PP_FILLMODE &0x1ff)),4);
	   	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
           	HQ3_PIO_WR(CFIFO1_ADDR, ppfillmode.val, ~0x0);

	   	_mgras_pt(i, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, ci_en, color2);
	   	_mgras_pt(i, 2.0, 0.0, 0.0, 0.0, 0.0, 0, 0, ci_en, color2);

		WAIT_FOR_CFIFO(cfifo_status, 32, CFIFO_TIMEOUT_VAL);
                if (cfifo_status == CFIFO_TIME_OUT) {
                       msg_printf(ERR,"HQ3 CFIFO IS FULL...EXITING\n");
                       return (-1);
                }

	   	ppfillmode.bits.DitherEnab = 0;
	   	ppfillmode.bits.DitherEnabA = 0;
	   	ppfillmode.bits.LogicOpEnab = 1;
	   	ppfillmode.bits.PixType = PP_FILLMODE_CI12;
	   	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA;
	   	ppfillmode.bits.CIclamp = 1;

		ppfillmode.bits.LogicOP = i;

	   	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,
			(0x1800+(PP_FILLMODE &0x1ff)),4);
	   	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
           	HQ3_PIO_WR(CFIFO1_ADDR, ppfillmode.val, ~0x0);

	   	_mgras_pt(i, 1.0, 0.0, 0.0, 0.0, 0.0, 0, 0, ci_en, color1);
	   	_mgras_pt(i, 2.0, 0.0, 0.0, 0.0, 0.0, 0, 0, ci_en, color1);

	   } /* for loop */

	   if (write_only == 0) {
	   /* don't do a compare here since the default is to compare 32 bits
	    * but for this test we don't want to compare any alpha bits
	    */
	   nocompare = 1;
	   errors += _readback(rssnum,numRows,numCols, logicop_array,reReadMode,
		0, rssDmaPar, hq3DmaPar, logicop_array_checksum & 0xffffffff, 
		LOGICOP_CRC_INDEX, do_crc, 0, 0, 0, 0);
	   nocompare = 0;

	   /* Do the compare here */
	   if (local_nocompare == 0) {
	     for (i = 0; i < (numRows*numCols); i++) {
		COMPARE("PP Logic OP", ((int)DMA_WRTile),(*(DMA_WRTile)),
			(*(DMA_RDTile)),0xffffff, errors);
		if (errors != oldErrors) {
			msg_printf(VRB, "Read addr: 0x%x\n", DMA_RDTile);
			_xy_to_rdram((i % numCols), (i/numCols), 0, 0, 0, 0, &cmd, &cmd,0x240);
		}
		oldErrors = errors;
		DMA_CompareTile++;
		DMA_RDTile++;
		DMA_WRTile++;
	     }
	     _mgras_disp_rdram_error();
	   }

	   if (errors & stop_on_err) {
	      if (rflag == 0) rssnum = 0;
	      REPORT_PASS_OR_FAIL(
		(&PP_err[PP1_LOGICOP_TEST+ rssnum*LAST_PP_ERRCODE]), errors);
	   }

	   } /* write_only == 0 */
	
	REPORT_PASS_OR_FAIL( (&PP_err[PP1_LOGICOP_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_dither()							  *
 *  Draw lines to exercise the dither unit.				  *
 *  Draw 2 lines, 1 with dither, 1 without.				  *
 *									  *
 **************************************************************************/

__uint32_t
mgras_dither(int argc, char ** argv)
{
	__uint32_t cmd, numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0;
	__uint32_t do_crc = 0, stop_on_err, rflag = 0, bad_arg = 0;
	float xstart, ystart;
	Vertex max_pt, min_pt;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;
	ppfillmodeu	ppfillmode = {0};

	__uint32_t dither_array[32] = {
	0x0, 0x9000009, 0x1a00001a, 0x2a00002a, 0x3c00003c, 0x4d00004d, 0x5e00005e, 0x6e00006e, 0x80000080, 0x90000090, 0xa20000a2, 0xb20000b2, 0xc40000c4, 0xd40000d4, 0xe60000e6, 0x0, 
	0x0, 0x8000008, 0x19000019, 0x2a00002a, 0x3b00003b, 0x4c00004c, 0x5d00005d, 0x6e00006e, 0x80000080, 0x91000091, 0xa20000a2, 0xb30000b3, 0xc40000c4, 0xd50000d5, 0xe60000e6, 0x0};

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
        numRE4s = 1;
#endif
	/* initialize to test everything */
	rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
	rssnum = 0;
	stop_on_err = 1;
	numCols = 16; numRows = 2;  /* stay on 1 page in fb memory */

	   _re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
		DMA_PP1, 1);

	   xstart = ystart = 0.0;

	   /* draw the lines */

		max_pt.coords.x = 15.0; max_pt.coords.y = 0.0;
        	max_pt.coords.z = 0.0;  max_pt.coords.w = 1.0;

        	min_pt.coords.x = 0.0;  min_pt.coords.y = 0.0;
        	min_pt.coords.z = 0.0;  min_pt.coords.w = 1.0;

        	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
        	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
        	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
        	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

        	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
        	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
        	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
        	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 0.0;

		/* with dither enabled */
		ppfillmode.bits.DitherEnab = 1;
                ppfillmode.bits.DitherEnabA = 1;
		ppfillmode.bits.ZEnab = 0;
        	ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
        	ppfillmode.bits.DrawBuffer = 0x1;

		cmd = BUILD_CONTROL_COMMAND(0,0,0,0,
			(0x1800 + (PP_FILLMODE & 0x1ff)),4);        
		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0);

		_mg0_Line(&min_pt, &max_pt, 0,0, 0, 0, 0, 0, 0,rssDmaPar);

		/* no dither */
		ppfillmode.bits.DitherEnab = 0;
                ppfillmode.bits.DitherEnabA = 0;

		cmd = BUILD_CONTROL_COMMAND(0,0,0,0,
			(0x1800 + (PP_FILLMODE & 0x1ff)),4);        
		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0);

		max_pt.coords.y = 1.0; min_pt.coords.y = 1.0;

		_mg0_Line(&min_pt, &max_pt, 0,0, 0, 0, 0, 0, 0,rssDmaPar);

		msg_printf(DBG, "do_crc in pp1func: %d\n", do_crc);

	   errors += _readback(rssnum,numRows,numCols, dither_array,reReadMode,
		0, rssDmaPar, hq3DmaPar, 0, 
		DITHER_CRC_INDEX, do_crc, 0, 0, 0, 0);

	   if (errors & stop_on_err) {
	      if (rflag == 0) rssnum = 0;
	      REPORT_PASS_OR_FAIL(
		(&PP_err[PP1_DITHER_TEST+ rssnum*LAST_PP_ERRCODE]), errors);
	   }
	
#if 0
	/* reset ranges to test all RSS's together if user has not specified
           a particular RSS to test */
        if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {                rssnum = 3; rssend = 5;
        }
#endif

	REPORT_PASS_OR_FAIL( (&PP_err[PP1_DITHER_TEST]), errors);
}

__uint32_t 
mgras_pp_dac_sig()
{     
	__uint32_t error, shift, loop, index, xend, yend;
        __uint32_t patrn;
	float r,g,b,a;
	__uint32_t pat_array[4] = {0xff, 0x0, 0x55, 0xaa};
	input   *ExpSign;
	input   *RcvSign;

#ifndef MGRAS_DIAG_SIM
        ExpSign = &Signature;
        RcvSign = &HWSignature;
#endif

	a = 1.0;
	r = g = b = 0.0;

	xend = 1344; yend = 1024;
#ifdef MGRAS_DIAG_SIM
	xend = 50; yend = 50;
#endif
	
	_mgras_rss_init(CONFIG_RE4_ALL);
	_draw_setup(DMA_PP1, 0);

	/* do 1-component tests, looping through b, g, and r */
        error = 0;
        for (loop = 0, shift = 0; loop<3; loop++, shift +=8) {
                for (index=0; index < 8; index++) {
			r = (0.0039215686 * (1<<index)) * (loop == 2);
			g = (0.0039215686 * (1<<index)) * (loop == 1);
			b = (0.0039215686 * (1<<index)) * (loop == 0);
#ifndef MGRAS_DIAG_SIM
                        patrn =  1 << index;
                        patrn <<= shift;
                        msg_printf(DBG, "Drawing patrn  0x%x\n" ,patrn);
                        ResetDacCrc() ;
#else
			fprintf(stderr, "i: %d, r: %f, g: %f, b: %f\n", index, r,g, b);	
#endif
                        _mg0_block(PP_FILLMODE_DBUFA,r,g,b,a,0, 0, xend, yend);
#ifndef MGRAS_DIAG_SIM
                        GetSignature() ;
                        GetComputeSig(CmapAll, patrn);
                        error = CrcCompare() ;
                        CONTINUITY_CHECK((&PP_err[PP1_PP_DAC_TEST]), CmapAll, patrn, error);
#endif
                }
        }

	/* loop through bits with r=g=b */
	r = g = b = 0.0;
        /* BroadCast RGB */
        for (index=0; index<8; index++) {
		r = g = b = 0.0039215686 * (1 << index);
#ifndef MGRAS_DIAG_SIM
                patrn =  1 << index;
                patrn = (patrn << 16) | (patrn << 8) | patrn ;
                msg_printf(DBG, "Drawing patrn  0x%x\n" ,patrn);
                ResetDacCrc() ;
#else
		fprintf(stderr, "i: %d, r: %f, g: %f, b: %f\n", index, r,g, b);	
#endif
                _mg0_block(PP_FILLMODE_DBUFA,r,g,b,a,0, 0, xend, yend);
#ifndef MGRAS_DIAG_SIM
                GetSignature() ;
                GetComputeSig(CmapAll, patrn);
                error = CrcCompare() ;
                CONTINUITY_CHECK((&PP_err[PP1_PP_DAC_TEST]), CmapAll, patrn, error);
#endif
        }
	
	/* loop through additional patterns where r=g=b */
	r = g = b = 0.0;
        /* BroadCast RGB */
        for (index=0; index < 4; index++) {
		r = g = b = 0.0039215686 * pat_array[index];
#ifndef MGRAS_DIAG_SIM
                patrn = pat_array[index];
                patrn = (patrn << 16) | (patrn << 8) | patrn ;
                msg_printf(DBG, "Drawing patrn  0x%x\n" ,patrn);
                ResetDacCrc() ;
#else
		fprintf(stderr, "i: %d, r: %f, g: %f, b: %f\n", index, r,g, b);	
#endif
                _mg0_block(PP_FILLMODE_DBUFA,r,g,b,a,0, 0, xend, yend);
#ifndef MGRAS_DIAG_SIM
                GetSignature() ;
                GetComputeSig(CmapAll, patrn);
                error = CrcCompare() ;
                CONTINUITY_CHECK((&PP_err[PP1_PP_DAC_TEST]), CmapAll, patrn, error);
#endif
        }
	return(error);
}
