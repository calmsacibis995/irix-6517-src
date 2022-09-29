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
#include <sys/cpu.h>
#include <sgidefs.h>
#include <libsc.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include <sys/mgras.h>
#include "parser.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "mgras_dma.h"
#include "mgras_color_z.h"
#include "mgras_z.h"
#include "ide_msg.h"
#include "mgras_alphablend.h"

/* block characters */
static ushort_t M[8] = {0x82,0x82,0x82,0x82,0x92,0xaa,0xc6,0x82};
static ushort_t A[8] = {0x82,0x82,0x82,0xfe,0x82,0x44,0x28,0x10};
static ushort_t R[8] = {0x82,0x84,0x88,0x90,0xfc,0x82,0x82,0xfc};
static ushort_t D[8] = {0xf8,0x84,0x82,0x82,0x82,0x82,0x84,0xf8};
static ushort_t I[8] = {0xfe,0x10,0x10,0x10,0x10,0x10,0x10,0xfe};
static ushort_t G[8] = {0xfe,0x82,0x82,0x9e,0x80,0x80,0x80,0xfe};
static ushort_t S[8] = {0xfc,0x02,0x02,0x02,0x7c,0x80,0x80,0x7e};

extern int ram_walkingbit();
extern int ram_addru(); 
extern int ram_pattern();
extern int ram_kh();
extern int ram_marchx();
extern int ram_marchy();
extern int ram_pattern();
void _mgras_disp_rdram_error(void);
extern __uint32_t _mg_rdram_error_index;
extern __uint32_t check_re_revision(__uint32_t);
extern void mg_restore_pps(void);
extern __uint32_t _mg_dac_crc_rss_pp(int, int, int, int, int, int, int, int);
extern __uint64_t read_checksum;
extern __uint64_t read_checksum_even;
extern __uint64_t read_checksum_odd;
#ifdef MFG_USED
extern void mgras_VC3ClearCursor(void);
#endif

__uint32_t green_texlut = 0;
__uint32_t blue_texlut = 0;
__uint32_t alpha_texlut = 0;
__uint32_t write_only =0; /* for write_only option in drawing tests */
__uint32_t global_dbuf;
__uint32_t te_rev;
__uint32_t global_starty = 0;
__uint32_t global_startx = 0;


/**************************************************************************
 *                                                                        *
 *  mgras_REInternalRAM							  *
 *                                                                        *
 *	-t [test#]							  *
 *	-r [optional RSS #]						  *
 *	-l [optional loopcount]						  *
 *	-a [specify a specific internal ram to test]- default is test all *
 * 		1= ADDRESS_RAM_LO and HI				  *
 *		2= TEX_LUT						  *	
 *		3= POLYSTIP_RAM						  *
 *		4= AA_TABLE						  *
 *		5= ZFIFO_A and B					  *
 *                                                                        *
 **************************************************************************/

int
mgras_REInternalRAM(int argc, char ** argv)
{
	__uint32_t status, rssnum, testnum, errors = 0;
	__uint32_t rflag = 0, tflag = 0, bad_arg = 0, ram_to_test = 0;	
	__uint32_t stop_on_err = 1;
	__int32_t loopcount = 1;
	int (*memtest)(__uint32_t,__uint32_t,__uint32_t,__uint32_t,__uint32_t,
		       __uint32_t,__uint32_t,__uint32_t,__uint32_t,__uint32_t);
	char *testname = "";

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	if ((numRE4s < 1) || (numRE4s > 2)) {
		msg_printf(ERR, "RE4Count is %d, allowed values are 1-2\n",
			numRE4s);
		REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST]), errors);
        }

	/* initialize to test everything */
	rssstart = 0; 
	rssend = numRE4s; 

	if (argc == 1)
		bad_arg++;

	/* get the args */
        argc--; argv++;
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
           switch (argv[0][1]) {
                case 'r':
			if (argv[0][2]=='\0') {
                        	/* RSS select  - optional */
                        	atob(&argv[1][0], (int*)&rssnum);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&rssnum);
                        rflag++; break;
                case 't':
			if (argv[0][2]=='\0') {
                        	/* test number- required */
                        	atob(&argv[1][0], (int*)&testnum);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&testnum);
                        tflag++; break;
		case 'l':
			if (argv[0][2]=='\0') { /* has white space */
				atob(&argv[1][0], (int*)&loopcount); 
				argc--; argv++; 
			}
			else {  /* no white space */
				atob(&argv[0][2], (int*)&loopcount);
			}
			if (loopcount < 0) {
			   msg_printf(SUM, "Error. Loop must be > or = to 0\n");
			   return (0);
			}
			break;	
                case 'x':
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], (int*)&stop_on_err);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&stop_on_err);
                        break;
                case 'a':
			if (argv[0][2]=='\0') {
                        	atob(&argv[1][0], (int*)&ram_to_test);
				argc--; argv++; 
			}
			else 
                        	atob(&argv[0][2], (int*)&ram_to_test);
                        break;
                default: bad_arg++; break;
           }
                argc--; argv++;
        }

#ifdef MGRAS_DIAG_SIM
	tflag = 1;
	bad_arg = 0;
#endif

        if ((bad_arg) || (argc) || (tflag == 0)) {
	   msg_printf(SUM, 
		"Usage: -t[test#] -r[optional RSS#] -l[optional loopcount] -a[optional ram to test] -x[1|0] (optional stop-on-error, default =1)\n");
	   msg_printf(SUM, 
	"\nTests: WalkingBit=0,AddrUniq=1,Pattern=2,Kh=3,MarchX=4,MarchY=5\n");
	   msg_printf(SUM, 
	"\nRAMs: Address FIFO=1,TexLUT=2,Polygon Stipple=3,AA=4,ZFIFO=5\n");
	   return (0);
        }

        if (re_checkargs(rssnum, 0, rflag, 0))
                return (0);

	switch (testnum) {
		case 0:
			memtest = ram_walkingbit;
			testname= "Walking Bit";
			break;
		case 1:
			memtest = ram_addru;
			testname= "Address Uniq";
			break;
		case 2:
			memtest = ram_pattern;
			testname= "Pattern";
			break;
		case 3:
			memtest = ram_kh;
			testname= "Kh";
			break;
		case 4:
			memtest = ram_marchx;
			testname= "March X";
			break;
		case 5:
			memtest = ram_marchy;
			testname= "March Y";
			break;
		default:
			bad_arg++;
			break;
	}

	msg_printf(VRB,"RE InternalRAM %s Test\n",testname);
#ifdef MGRAS_DIAG_SIM

	rssend = 1; 
	memtest = ram_pattern;
	bad_arg = 0;
	stop_on_err = 1;

	/* the following variations have also been simulated:
  	 *		aflag = 1; -- to test the allram variable in each test
	 *		memtest = ram_marchx;
	 *		memtest = ram_marchy;
	 *		memtest = ram_walkingbit;
	 *		memtest = ram_addru;
	 *		memtest = ram_kh;
	 */
#endif
		
        if ((bad_arg) || (argc)) {
	   msg_printf(SUM,"Unrecognized test number: %d\n", testnum);
	   msg_printf(SUM, 
	"\nTests: WalkingBit=0,AddrUniq=1,Pattern=2,Kh=3,MarchX=4,MarchY=5\n");
	   return (0);
        }

	if (loopcount)
		loopcount++; /* bump it up since we decrement it next */

	while (--loopcount) {
#ifdef MGRAS_DIAG_SIM
	for (x = 0; x < 1; x++) {
	   switch (x) {
		case 0: memtest = ram_walkingbit; break;
		case 1: memtest = ram_addru; break;
		case 2: memtest = ram_pattern; break;
		case 3: memtest = ram_kh; break;
		case 4: memtest = ram_marchx; break;
		case 5: memtest = ram_marchy; break;
	   }
	msg_printf(DBG, "memtest = %d\n", memtest);
#endif

	   /* errors returned by each individual memory test */
	   for (rssnum = rssstart; rssnum < rssend; rssnum++) {
		HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(CONFIG),
			rssnum, EIGHTEENBIT_MASK);

		/* poly stipple enable gives access to polygon stipple ram */
		HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE(FILLMODE),
			0x1, EIGHTEENBIT_MASK);

#if 0
		WRITE_RE_INDIRECT_REG(rssnum, ADDRESS_RAM_LO, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, ADDRESS_RAM_HI, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, TEX_LUT, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, POLYSTIP_RAM, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, AA_TABLE, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, ZFIFO_A, 0xbaadcafe, 0xffffffff);
		WRITE_RE_INDIRECT_REG(rssnum, ZFIFO_B, 0xbaadcafe, 0xffffffff);

		READ_RE_INDIRECT_REG(rssnum, ADDRESS_RAM_LO, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, ADDRESS_RAM_HI, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, TEX_LUT, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, POLYSTIP_RAM, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, AA_TABLE, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, ZFIFO_A, 0xffffffff, 0xbaadcafe);
		READ_RE_INDIRECT_REG(rssnum, ZFIFO_B, 0xffffffff, 0xbaadcafe);
#endif

		if ((ram_to_test == 0) || (ram_to_test == 1)) {

		msg_printf(VRB, "Testing Address FIFO.\n");

		/* test address ram, 8x36 split into 8x22, 8x14 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   ADDRESS_RAM_LO, ADDRESS_RAM_LO + 7, 1 /* do broadcast */, 
		   1 /* test every location */, stop_on_err, ADDRESS_RAM_LO, 
		   TWENTYTWOBIT_MASK, 22);
		if (errors) {
			REPORT_PASS_OR_FAIL_NR((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
			if (stop_on_err) {
				RETURN_STATUS(errors);
			}
		}

	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   ADDRESS_RAM_HI, ADDRESS_RAM_HI + 7, 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ADDRESS_RAM_HI,
		   FOURTEENBIT_MASK, 14);
		if (errors) {
			REPORT_PASS_OR_FAIL_NR((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
			if (stop_on_err) {
				RETURN_STATUS(errors);
			}
		}
		}

		if ((ram_to_test == 0) || (ram_to_test == 5)) {

		msg_printf(VRB, "Testing Z-FIFO.\n");
		/* Test the z-fifo, 8x48, read pipe A 8x24 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   ZFIFO_A, ZFIFO_A + 7, 1 /* do broadcast */, 
		   1 /* test every location */, stop_on_err, ZFIFO_A, 
		   TWENTYFOURBIT_MASK, 24);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}

		/* Test the z-fifo, pipe B, 8x24 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   ZFIFO_B, ZFIFO_B + 7, 1 /* do broadcast */, 
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   TWENTYFOURBIT_MASK, 24);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		}

		if ((ram_to_test == 0) || (ram_to_test == 4)) {

		msg_printf(VRB, "Testing AA Table.\n");
		/* Test the aa ram 1 (angle correction), 32x9 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   AA_TABLE | 0x80, (AA_TABLE | 0x80) + 31, 1/* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   NINEBIT_MASK, 9);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}

		/* Test the aa ram 0 (coverage), 128x8 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   AA_TABLE, AA_TABLE + 0x80, 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   EIGHTBIT_MASK, 8);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		}

		if ((ram_to_test == 0) || (ram_to_test == 3)) {

		msg_printf(VRB, "Testing Polygon Stipple RAM.\n");
		/* Test the polygon stipple ram, 32x32 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   POLYSTIP_RAM, POLYSTIP_RAM + 31, 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   THIRTYTWOBIT_MASK, 32);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		}

		if ((ram_to_test == 0) || (ram_to_test == 2)) {

		/* Skip luts unless RE is at least rev C (=2) */
		/* or if user explicitly asks for luts */
		if ((ram_to_test == 2) || (check_re_revision(rssnum))){

		msg_printf(VRB, "Testing Red Texture LUT.\n");
		/* Test the texture lut RED 256x8 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   TEX_LUT, TEX_LUT + 255, 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   EIGHTBIT_MASK, 8);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}

		/* The green lut is read back in bits [11:8]. The 
		   RE_IND_COMPARE macro takes care of it.
		 */
		green_texlut = 1;
		msg_printf(VRB, "Testing Green Texture LUT.\n");
		/* Test the texture lut GREEN 256x8 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   (TEX_LUT + 256), (TEX_LUT + 256 + 255), 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   SIXTEENBIT_MASK, 8);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		green_texlut = 0;

		/* The blue/alpha lut is read back in bits [31:23]. The
		   RE_IND_COMPARE macro takes care of it. [31:28] and [27:24]
		   are replicated so that [27:24] is blue and [31:28] is alpha.
		   We only check [27:24] in this call, while bits [31:28] are
		   checked in the alpha-lut call below.
		 */
		blue_texlut = 1;
		msg_printf(VRB, "Testing Blue/Alpha Texture LUT.\n");
		/* Test the texture lut BLUE, ALPHA 256x8 */
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   (TEX_LUT + 512), (TEX_LUT + 512 + 255), 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   TWENTYFOURBIT_MASK, 8);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		blue_texlut = 0;

		/* The alpha lut is really the same as the blue lut, but
		 * we test again and only check bits [31:27]. */
		alpha_texlut = 1;
	     	errors += (*memtest)(rssnum, 0 /* ppnum is no op here */ ,
		   (TEX_LUT + 512), (TEX_LUT + 512 + 255), 1 /* do broadcast */,
		   1 /* test every location */, stop_on_err, ZFIFO_B, 
		   THIRTYTWOBIT_MASK, 8);
		if (errors) {
			REPORT_PASS_OR_FAIL((&RE_err[RE4_INTERNALRAM_TEST +
				rssnum*LAST_RE_ERRCODE]),
				errors);	
		}
		alpha_texlut = 0;

		} /* ram == 2 */
		} /* test luts , rams == 0 or 2 */

	   } /* rssnum */
#ifdef MGRAS_DIAG_SIM
	  } /* for all testnums */
#endif
        } /* loopcount */


	if (errors == 0) {
		msg_printf(VRB, "     RE InternalRAM %s Test passed.\n",
			testname);
		return (0);
	}
	else {
		msg_printf(ERR, "**** RE InternalRAM %s Test failed.\n",
			testname);
		return (-1);
	}
}

#define HQ_PIO_RD 1

/* common setup steps for drawing */
__uint32_t
_draw_setup(__uint32_t destDevice, __uint32_t do_clear)
{
	__uint32_t colormasklsba, cmd;
	fillmodeu	fillmode = {0};
	ppfillmodeu	ppfillmode = {0};
	ppzmodeu	ppzmode = {0};
	texmode1u	texmode1 = {0};
	__paddr_t	pgMask = ~0x0;
	__uint32_t xend = XMAX;
	__uint32_t yend = YMAX;
	__uint32_t drb_ptr;
	
	drb_ptr = mg_tinfo->DRBptrs.main;

	pgMask = PAGE_MASK(pgMask);

	_mg_rdram_error_index = 0;

	fpusetup();

	/* initialize memory chunk */
   DMA_WRTile_notaligned = (__uint32_t *)get_chunk(2*DMABUFSIZE);
   DMA_RDTile_notaligned = DMA_WRTile_notaligned + 517120;
   if (DMA_WRTile_notaligned == 0) {
        msg_printf(ERR,"Could not allocate %d bytes.\n",2*DMABUFSIZE);
        return 1;
   }

	DMA_RDTile = (__uint32_t *) 
		((((__paddr_t)DMA_RDTile_notaligned) + 0x1000) & pgMask);
   	msg_printf(DBG, "DMA_RDTile 0x%x\n", DMA_RDTile);

   	DMA_WRTile = (__uint32_t *) 
		((((__paddr_t)DMA_WRTile_notaligned) + 0x1000) & pgMask);
   	msg_printf(DBG, "DMA_WRTile 0x%x\n", DMA_WRTile);

   	DMA_CompareTile = (__uint32_t *)DMA_CompareTile_notaligned;
   	msg_printf(DBG, "DMA_CompareTile 0x%x\n", DMA_CompareTile);

	/* if do_clear ==2, then use the indirect mode to write to the PP1 */
	
	/* set up needed registers */
	fillmode.bits.ZiterEN = 1;
	fillmode.bits.ShadeEN = 1;
	fillmode.bits.RoundEN = 1;
	
    	ppfillmode.bits.DitherEnab = 1;
    	ppfillmode.bits.DitherEnabA = 1;
    	ppfillmode.bits.ZEnab = 0;
    	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
    	ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */

	ppzmode.bits.Zfunc = PP_ZF_ALWAYS; 
	
	texmode1.bits.TexEn = 0;
	texmode1.bits.BlendUnitCount = TEXMODE1_5PAIR;

	/* green bit hack for now */
	if (is_old_pp)
		colormasklsba = 0xfffbffff;
	else
		colormasklsba = 0xffffffff;

	if (destDevice == DMA_PP1) {
	   if (do_clear == 2) /* write via re4's indirect addressing mode */
	   {
		if (pp_drb == PP_1_RSS_DRB_OVERLAY) {
		   WRITE_RE_INDIRECT_REG(4, (PP_DRBPOINTERS | DEVICE_PP1_AB), 
			PP_1_RSS_DRB_OVERLAY, THIRTYTWOBIT_MASK);
		}
		else {
		   WRITE_RE_INDIRECT_REG(4, (PP_DRBPOINTERS | DEVICE_PP1_AB), 
			(drb_ptr & 0x3ff), THIRTYTWOBIT_MASK);
		}

		WRITE_RE_INDIRECT_REG(4, (PP_ZMODE | DEVICE_PP1_AB), 
			(ppzmode.val | 0x0ffffff0), THIRTYTWOBIT_MASK);

		WRITE_RE_INDIRECT_REG(4, (PP_COLORMASKLSBA | DEVICE_PP1_AB), 
			colormasklsba, THIRTYTWOBIT_MASK);
	   } 
	   else /* write the normal way */
	   {
		if (pp_drb == PP_1_RSS_DRB_OVERLAY) {
			HQ3_PIO_WR_RE(RSS_BASE + 
				HQ_RSS_SPACE((PP_DRBPOINTERS & 0x1ff)), 
				PP_1_RSS_DRB_OVERLAY, 0xffffffff);
		}
		else {
			HQ3_PIO_WR_RE(RSS_BASE + 
				HQ_RSS_SPACE((PP_DRBPOINTERS & 0x1ff)), 
				(drb_ptr & 0x3ff), 0xffffffff);
		}

		HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_ZMODE& 0x1ff)), 
			(ppzmode.val | 0x0ffffff0), 0xffffffff);

		HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE((PP_COLORMASKLSBA& 0x1ff)),
			 colormasklsba, 0xffffffff);
	   }

	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(FILLMODE),fillmode.val, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(XFRMASKLOW),0xffffffff, 0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE(XFRMASKHIGH),0xffffffff,0xffffffff);
	HQ3_PIO_WR_RE(RSS_BASE +HQ_RSS_SPACE(TEXMODE1),texmode1.val,0xffffffff);

	cmd = BUILD_CONTROL_COMMAND(0,0,0, 0, (0x1800+(PP_PIXCMD & 0x1ff)), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, PP_PIXCMD_DRAW, ~0x0);

	} /* DMA_PP1 */

	if (do_clear) {
	   _mg0_block(PP_FILLMODE_DBUFA, 0.0, 0.0, 0.0, 0.0, 0, 0, xend, yend);
	}

	if (destDevice == DMA_PP1) {
	   if (do_clear == 2) 
	   {
		WRITE_RE_INDIRECT_REG(4, (PP_FILLMODE | DEVICE_PP1_AB), 
			ppfillmode.val, THIRTYTWOBIT_MASK);
	   } 
	   else 
	   {
		HQ3_PIO_WR_RE(RSS_BASE + HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)), ppfillmode.val, 0xffffffff);
	   }
	}

	 WRITE_RSS_REG(4, XYWIN, 0x0, 0xffffffff);
	 return(0);
}

/* common setup steps for the re4 functionality tests */
void
_re4func_setup(
	__uint32_t rssnum, 
	__uint32_t numCols,
	__uint32_t numRows,
	RSS_DMA_Params *rssDmaPar,
	HQ3_DMA_Params *hq3DmaPar,
	__uint32_t destDevice,
	__uint32_t do_clear)
{
	__uint32_t first_sp_width;

	rssDmaPar->x = 0; /* always start at 0 */
	first_sp_width = numCols * 4;

	/* setup for the dma readback of the numCols x numRows area */
	rssDmaPar->numRows                   = numRows;
   	rssDmaPar->numCols                   = numCols;
   	rssDmaPar->pixComponents             = 4;
   	rssDmaPar->pixComponentSize          = 1; /* 1 byte per component */
   	rssDmaPar->x                         = global_startx;
   	rssDmaPar->y                         = global_starty;
   	rssDmaPar->beginSkipBytes            = 0;
   	rssDmaPar->strideSkipBytes           = 0;
   	rssDmaPar->xfrDevice                 = DMA_PP1;
  	rssDmaPar->rss_clrmsk_msb_pp1        = ~0x0;
   	rssDmaPar->rss_clrmsk_lsba_pp1       = ~0x0;
   	rssDmaPar->rss_clrmsk_lsbb_pp1       = ~0x0;
   	rssDmaPar->checksum_only 	     = 0;

   	hq3DmaPar->page_size                 = page_4k;
   	hq3DmaPar->physpages_per_impage      = 16;
   	hq3DmaPar->phys_base                 = 0xdeadbee0;
   	hq3DmaPar->pool_id                   = DMA_GL_POOL;
   	hq3DmaPar->IM_pg_width               = first_sp_width;
   	hq3DmaPar->IM_row_offset             = 0;
   	hq3DmaPar->IM_row_start_addr         = 0;
   	hq3DmaPar->IM_line_cnt               = numRows;
   	hq3DmaPar->IM_first_sp_width         = first_sp_width;
   	hq3DmaPar->IM_DMA_ctrl               =
        ( (0x1  << 0) |                 /* Run flag */
          (DMA_GL_POOL  << 1) |         /* Pool - GL */
          (0x1  << 4) |                 /* DMA dest (0 -> cp) */
          (0x1  << 5) |                 /* Scan packed (1 -> yes) */
          (0x0  << 6) |                 /* Decimate (1 -> yes) */
          (0x0  << 7) );                /* Start unconditionally */

	_mgras_hq_re_dma_init();

   	GFX_HQHOSTCALLBACK();

	/* clear regs */
	/* _mgras_rss_init(rssnum); */

	_draw_setup(destDevice, do_clear);
}

__uint32_t
_readback(
	__uint32_t rssnum,
	__uint32_t numRows,
	__uint32_t numCols, 
	__uint32_t *data,
	__uint32_t reReadMode,
	__uint32_t db_enable,
	RSS_DMA_Params *rssDmaPar,
	HQ3_DMA_Params *hq3DmaPar,
	__uint32_t pattern,
	__uint32_t exp_crc_index,
	__uint32_t do_crc,
	__uint32_t dma_xstart,
	__uint32_t dma_ystart,
	__uint64_t target_checksum,       /* even checksum for IP30 */
	__uint64_t target_checksum_odd)   /* not used by IP22/IP28 */
{
	__uint32_t x, y, cmd, errors = 0, time, actual, first_sp_width;
	__uint32_t rdram_dma_read_config;

	msg_printf(DBG, " rssnum-%d rows-%d cols-%d data-%x readmode-%d db-%d rssdmapar-%x hqpar-%x pat-%d crc-%d do_crc-%d x-%d y-%d targ-%llx \n",
	rssnum, numRows, numCols, data, reReadMode, db_enable, rssDmaPar, hq3DmaPar, pattern, exp_crc_index, do_crc, dma_xstart, dma_ystart, target_checksum);
	msg_printf(DBG, 
	"exp_crc_index: %d, do_crc %d, dma_xstart %d, dma_ystart %d\n", 
		exp_crc_index, do_crc,dma_xstart,dma_ystart);

	if (write_only == 1) {
		msg_printf(DBG, "Skipping readback.\n");
		return (0);
	}

	pp_dma_mode = PP_RGBA8888;
	XMAX = 1344; YMAX = 1024;

	rssDmaPar->numRows = numRows;	
	rssDmaPar->numCols = numCols;	
	rssDmaPar->y = dma_ystart;
	rssDmaPar->x = dma_xstart;

	first_sp_width = numCols * 4;

	hq3DmaPar->page_size                 = page_4k;
        hq3DmaPar->physpages_per_impage      = 16;
        hq3DmaPar->phys_base                 = 0xdeadbee0;
        hq3DmaPar->pool_id                   = DMA_GL_POOL;
        hq3DmaPar->IM_pg_width               = first_sp_width;
        hq3DmaPar->IM_row_offset             = 0;
        hq3DmaPar->IM_row_start_addr         = 0;
        hq3DmaPar->IM_line_cnt               = numRows;
        hq3DmaPar->IM_first_sp_width         = first_sp_width;
        hq3DmaPar->IM_DMA_ctrl               =
        ( (0x1  << 0) |                 /* Run flag */
          (DMA_GL_POOL  << 1) |         /* Pool - GL */
          (0x1  << 4) |                 /* DMA dest (0 -> cp) */
          (0x1  << 5) |                 /* Scan packed (1 -> yes) */
          (0x0  << 6) |                 /* Decimate (1 -> yes) */
          (0x0  << 7) );                /* Start unconditionally */

	msg_printf(DBG, "pp_drb in readback: 0x%x\n", pp_drb);

	msg_printf(DBG, 
	"hq3DmaPar->IM_pg_width: 0x%x, hq3DmaPar->IM_line_cnt: 0x%x, addr:0x%x\n",
		hq3DmaPar->IM_pg_width, hq3DmaPar->IM_line_cnt, hq3DmaPar);
	msg_printf(DBG, "DMA_WRTile addr: 0x%x\n", DMA_WRTile);
	msg_printf(DBG, "data addr: 0x%x\n", data);
	/* setup the compare array */
	for (y = 0; y < numRows; y++)
		for (x = 0; x < numCols; x++) {
			DMA_WRTile[x + numCols*y] = *data;
			data++;
		}
#ifndef MGRAS_DIAG_SIM	

	time = 500; /* MGRAS_DMA_TIMEOUT_COUNT; */
   	do {
        	READ_RSS_REG(0, STATUS, 0, ~0x0);
        	time--;
   	} while ( ((actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) && time);

	if (((actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) && (time ==0)) {
	   msg_printf(ERR, "RE4 STATUS register is not idle: 0x%x\n",
			actual);
	   msg_printf(VRB, "Continuing anyway, data read may not be valid\n");
	}

	/* for multi-RSS frame buffer read, set the config register to
         * CONFIG_RE4_ALL | ((last row #)-1) % numRE4s)
         */
        rdram_dma_read_config=CONFIG_RE4_ALL | (((rssDmaPar->y + rssDmaPar->numRows)-1)%numRE4s);
        msg_printf(DBG,"rdram_dma_read_config: %d, y: %d, numRows: %d\n", rdram_dma_read_config,
		rssDmaPar->y, rssDmaPar->numRows);

	/* set config  for reads */
	HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(CONFIG))), rdram_dma_read_config, TWELVEBIT_MASK);

	if (reReadMode == DMA_PIO) {
	   errors += _mgras_hqpio_redma_read_compare(rssDmaPar, reReadMode, 
		DMA_RDTile, db_enable);
	}
	else if (reReadMode == DMA_BURST) {
	   FormatterMode(0x201);
	   errors +=  _mgras_hqdma_redma_read_compare(rssDmaPar,
                        hq3DmaPar, DMA_BURST, DMA_RDTile, db_enable);
	   FormatterMode(HQ_FC_CANONICAL);
	}

	/* display the error info */
        _mgras_disp_rdram_error();

	if (rssDmaPar->checksum_only == 1) {
	   if (target_checksum == TE_REV_D_CHECKSUM) { /* the terev test */
		switch (read_checksum) {
			case TE_REV_D_CHECKSUM: te_rev = TE_REV_D; break;
			case TE_REV_B_1TRAM_CHECKSUM: 
				if (numTRAMs== 1)
					te_rev = TE_REV_B;
				else
			   	   msg_printf(ERR, "Got a rev B checksum for a 1-TRAM system, but the diags thinks there are %d trams.\n", numTRAMs);
				break;
			case TE_REV_B_4TRAM_CHECKSUM:
				if (numTRAMs == 4)
					te_rev = TE_REV_B;
				else
			   	   msg_printf(ERR, "Got a rev B checksum for a 4-TRAM system, but the diags thinks there are %d trams.\n", numTRAMs);
				break;
			default: 
			   te_rev = TE_REV_UNDETERMINED; 

			   msg_printf(ERR, "te_rev checksum, got: 0x%llx, expected 0x%llx or 0x%x or 0x%llx\n", read_checksum, TE_REV_D_CHECKSUM, TE_REV_B_1TRAM_CHECKSUM, TE_REV_B_4TRAM_CHECKSUM);
			   msg_printf(SUM, "The TE revision is unknown or the system has TE chips with different revisions\n");

			   break;
		}
		msg_printf(DBG, "te_rev checksum: 0x%llx, te_rev: %d\n", 
			read_checksum, te_rev);
	   }
	   else {
#if (defined(IP22) || defined(IP28))
		if (read_checksum != target_checksum) {
			msg_printf(ERR,
			"Checksum error: exp = 0x%llx, rcv = 0x%llx\n",
			target_checksum, read_checksum);
			errors++;
		}
#else
		if (read_checksum_even != target_checksum) {
			if (numRE4s == 2) {
				msg_printf(ERR, "RSS-0 checksum error\n");
			}
			msg_printf(ERR,
			"Checksum error: exp = 0x%llx, rcv = 0x%llx\n",
			target_checksum, read_checksum_even);
			errors++;
		}
		if (read_checksum_odd != target_checksum_odd) {
			if (numRE4s == 2) {
				msg_printf(ERR, "RSS-1 checksum error\n");
			}
			msg_printf(ERR,
			"Checksum error: exp = 0x%llx, rcv = 0x%llx\n",
			target_checksum_odd, read_checksum_odd);
			errors++;
		}
#endif
	   }
	}

	/* do_crc == 1, ==> do the crc_check
	 * do_crc == 0, ==> skip the crc_check
	 */
	msg_printf(DBG, "readback: do_crc: %d\n", do_crc);
	if (do_crc == 1) {
		msg_printf(DBG, "trying to do check crc!\n");
		errors += _check_crc_sig(pattern, exp_crc_index);
	}

#endif
	/* reset the pixcmd reg to draw -- the dma sets it to dma */
	cmd = BUILD_CONTROL_COMMAND(0,0,0, 0, (0x1800+(PP_PIXCMD & 0x1ff)), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, PP_PIXCMD_DRAW, ~0x0);

	return(errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_color_tri()							  *
 *                                                                        *
 * Test the color and alpha iterators.					  *
 *                                                                        *
 * 	Draw red, green, and blue triangles with different color values   *
 *	at each pixel.							  *
 *	Draw a red triangle, then a green one over it with different      *
 *	alpha values.							  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_color_tri(int argc, char ** argv)
{
	__uint32_t cmd, source, dest, numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0, do_crc=0;
	__uint32_t depthkludge = 0, stop_on_err = 1, rflag = 0, bad_arg = 0;
	Vertex max_pt, mid_pt, min_pt;
	ppfillmodeu     ppfillmode = {0};
	ppblendfactoru	ppblendfactor = {0};
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;

	NUMBER_OF_RE4S();

#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	reReadMode = DMA_PIO;
        numCols = 24; numRows = 16; /* stay on 1 page */

	GET_RSS_NUM();

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, DMA_PP1,
		1);

	/* common x,y,z */
	max_pt.coords.x = 0.0; 		
	max_pt.coords.y = (float)(numRows-1);		
	max_pt.coords.z = 0.0;		
	max_pt.coords.w = 1.0;		

	mid_pt.coords.x = (float)(numCols-1); 	
	mid_pt.coords.y = (float)((numRows/2)-1);	
	mid_pt.coords.z = 0.0;
	mid_pt.coords.w = 1.0;

	min_pt.coords.x = 0.0; 	min_pt.coords.y = 0.0;	
	min_pt.coords.z = 0.0;	 min_pt.coords.w = 1.0;

	/* Red triangle */
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 0.5;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge);

	errors += _readback(rssnum, numRows, numCols, red_tri, reReadMode,
		0, rssDmaPar, hq3DmaPar, (red_tri_checksum & 0xffffffff),
		RED_TRI_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors) {
	   msg_printf(ERR, "Color Triangle Test: Red Triangle failed\n");
	   if (stop_on_err) {
	 	if (rflag == 0) rssnum = 0;
	 	   REPORT_PASS_OR_FAIL((&RE_err[RE4_COLOR_TRI_TEST]), errors);
	   }
	}
	else {
	   msg_printf(VRB, "Color Triangle Test: Red Triangle passed\n");
	}
		

	/* Green triangle */
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 0.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 0.0;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 0.5;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 1.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	_mg0_block(PP_FILLMODE_DBUFA, 0.2, 0.4, 0.8, 0.1, 0, 0, XEND, YEND); 
	_mg0_block(PP_FILLMODE_DBUFA, 0.0, 0.0, 0.0, 0.0, 0, 0, XEND, YEND); 
	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge);

	errors += _readback(rssnum, numRows, numCols, green_tri, reReadMode,
		0, rssDmaPar, hq3DmaPar, green_tri_checksum & 0xffffffff, 
		GRN_TRI_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors) {
	   msg_printf(ERR, "Color Triangle Test: Green Triangle failed\n");
	   if (stop_on_err) {
	 	if (rflag == 0) rssnum = 0;
	 	   REPORT_PASS_OR_FAIL((&RE_err[RE4_COLOR_TRI_TEST]), errors);
	   }
	}
	else {
	   msg_printf(VRB, "Color Triangle Test: Green Triangle passed\n");
	}
		
	/* Blue triangle */
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 0.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.5;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 0.0;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 1.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	_mg0_block(PP_FILLMODE_DBUFA, 0.2, 0.4, 0.8, 0.1, 0, 0, XEND, YEND); 
	_mg0_block(PP_FILLMODE_DBUFA, 0.0, 0.0, 0.0, 0.0, 0, 0, XEND, YEND); 
	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge);

	errors += _readback(rssnum, numRows, numCols, blue_tri, reReadMode,
		0, rssDmaPar, hq3DmaPar, blue_tri_checksum & 0xffffffff,
		BLU_TRI_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors) {
	   msg_printf(ERR, "Color Triangle Test: Blue Triangle failed\n");
	   if (stop_on_err) {
	 	if (rflag == 0) rssnum = 0;
	 	   REPORT_PASS_OR_FAIL((&RE_err[RE4_COLOR_TRI_TEST]), errors);
	   }
	}
	else {
	   msg_printf(VRB, "Color Triangle Test: Blue Triangle passed\n");
	}
		
	/* Alpha iterator - draw a red tri, then a green one */
	_mg0_block(PP_FILLMODE_DBUFA, 0.2, 0.4, 0.8, 0.1, 0, 0, XEND, YEND); 
	_mg0_block(PP_FILLMODE_DBUFA, 0.0, 0.0, 0.0, 0.0, 0, 0, XEND, YEND); 

	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 0.25;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 1.0;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 0.25;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 1.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 0.25;

	max_pt.coords.z = 0.0;
	mid_pt.coords.z = 0.0;
	min_pt.coords.z = 0.0;

	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge); /* red tri */
	source = PP_SBF_SA; dest = PP_DBF_MDA;
	ppblendfactor.bits.BlendSfactor = source; /* source alpha */
	ppblendfactor.bits.BlendDfactor = dest; /* 1-dest alpha */

	cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);
    	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    	HQ3_PIO_WR(CFIFO1_ADDR, (ppblendfactor.val | 0x80000000), ~0x0);

	cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_BLENDFACTOR & 0x1ff)),4);
    	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    	HQ3_PIO_WR(CFIFO1_ADDR, 0x7880c8c4, ~0x0);

    	ppfillmode.bits.DitherEnab = 1;
    	ppfillmode.bits.DitherEnabA = 1;
    	ppfillmode.bits.ZEnab = 0;
    	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
    	ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */
	ppfillmode.bits.BlendEnab = 1; /* turn on blending */

	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + (PP_FILLMODE & 0x1ff)),4);
    	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    	HQ3_PIO_WR(CFIFO1_ADDR, (ppfillmode.val | 0x0c000000), ~0x0);

	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 0.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 1.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 0.0;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 0.0;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 1.0;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 0.5;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 1.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge);

	errors += _readback(rssnum, numRows, numCols, alpha_tri, reReadMode,
		0, rssDmaPar, hq3DmaPar,alpha_tri_checksum & 0xffffffff,
		ALP_TRI_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors) {
	   msg_printf(ERR, "Color Triangle Test: Alpha Triangle failed\n");
	   if (stop_on_err) {
	 	if (rflag == 0) rssnum = 0;
	 	   REPORT_PASS_OR_FAIL((&RE_err[RE4_COLOR_TRI_TEST]), errors);
	   }
	}
	else {
	   msg_printf(VRB, "Color Triangle Test: Alpha Triangle passed\n");
	}
#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif
		

	REPORT_PASS_OR_FAIL((&RE_err[RE4_COLOR_TRI_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 *  _mgras_z_tri()							  *
 *                                                                        *
 * Test the z iterators.						  *
 *                                                                        *
 *	Same as the verif/REPP/fragments/zfunc tests.                     *
 *									  *
 *	this is to test the 8 depthfunc  values				  *
 * draw_scene1 generates the same result for the following depthfunc 	  *
 * pairs:								  *
 *  GT_NEVER,GT_EQUAL							  *
 *  GT_LESS,GT_LEQUAL,							  *
 *  GT_GREATER,GT_GEQUAL,						  *
 *  GT_NOTEQUAL,GT_ALWAYS.						  *
 *									  *
 *  draw scene2 is written with both triangles having equal Z values.	  *
 *  Therefore it will generate differnt results for the pairs above. Thus *
 *  all 8 functions are uniquely tested.				  *
 *									  *
 **************************************************************************/

__uint32_t
_mgras_z_tri(__uint32_t scene_num, __uint32_t rflag, __uint32_t stop_on_err, __uint32_t do_crc)
{
	__uint32_t cmd, numCols, numRows, reReadMode;
	__uint32_t exp_crc_index, crc_pat, rssnum, errors = 0, mode;
	Vertex max_pt_r, mid_pt_r, min_pt_r, max_pt_g, mid_pt_g, min_pt_g;
	Vertex max_pt_b, mid_pt_b, min_pt_b;
	__uint32_t depthkludge_r, depthkludge_g, depthkludge_b; /* for varied z triangles */
	__uint32_t scene;
	__uint32_t *tri;
	ppfillmodeu     ppfillmode = {0};
	ppzmodeu     ppzmode = {0};
	fillmodeu     fillmode = {0};
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;
	__uint32_t depthfunclist[8] = {
		PP_ZF_NEVER, 	/* 0 */
		PP_ZF_LESS,  	/* 1 */
		PP_ZF_EQUAL,	/* 2 */
		PP_ZF_LEQUAL,	/* 3 */
		PP_ZF_GREATER,	/* 4 */
		PP_ZF_NOTEQUAL,	/* 5 */
		PP_ZF_GEQUAL,	/* 6 */
		PP_ZF_ALWAYS};	/* 7 */

	reReadMode = DMA_PIO;
        numCols = 16; numRows = 16; /* stay on 1 page */

	depthkludge_r = depthkludge_g = depthkludge_b = 1; 

#ifdef MGRAS_DIAG_SIM
	rssstart = 0; rssend = 1;
#endif

	rssnum = 1; /* a no-op since _re4func_setup ignores this */
	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, DMA_PP1,
		1);

	/* set up needed registers */
	fillmode.bits.ZiterEN = 1;
	fillmode.bits.ShadeEN = 1;
	fillmode.bits.RoundEN = 1;
	
    	ppfillmode.bits.DitherEnab = 1;
    	ppfillmode.bits.DitherEnabA = 1;
    	ppfillmode.bits.ZEnab = 0;
    	ppfillmode.bits.PixType = 0x2; /* 32-bit rgba */
    	ppfillmode.bits.DrawBuffer = 0x1; /* buffer A */

	/* Red triangle */
	max_pt_r.coords.x = 0.0; 	max_pt_r.coords.y = (float)(numRows);	
	max_pt_r.coords.z = 16.0;	max_pt_r.coords.w = 1.0;		

	mid_pt_r.coords.x = (float)(numCols); 	
	mid_pt_r.coords.y = (float)((numRows/2));	
	mid_pt_r.coords.z = 16.0; 
	mid_pt_r.coords.w = 1.0;

	min_pt_r.coords.x = 0.0; 	min_pt_r.coords.y = 0.0;	
	min_pt_r.coords.z = 16.0; 	min_pt_r.coords.w = 1.0;

	max_pt_r.color.r = max_pt_r.colors[__GL_FRONTFACE].r = 1.0;
	max_pt_r.color.g = max_pt_r.colors[__GL_FRONTFACE].g = 0.0;
	max_pt_r.color.b = max_pt_r.colors[__GL_FRONTFACE].b = 0.0;
	max_pt_r.color.a = max_pt_r.colors[__GL_FRONTFACE].a = 1.0;

	mid_pt_r.color.r = mid_pt_r.colors[__GL_FRONTFACE].r = 1.0;
	mid_pt_r.color.g = mid_pt_r.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt_r.color.b = mid_pt_r.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt_r.color.a = mid_pt_r.colors[__GL_FRONTFACE].a = 1.0;

	min_pt_r.color.r = min_pt_r.colors[__GL_FRONTFACE].r = 1.0;
	min_pt_r.color.g = min_pt_r.colors[__GL_FRONTFACE].g = 0.0;
	min_pt_r.color.b = min_pt_r.colors[__GL_FRONTFACE].b = 0.0;
	min_pt_r.color.a = min_pt_r.colors[__GL_FRONTFACE].a = 1.0;

	/* set up the green triangle */
	max_pt_g.coords.x = (float)(numCols); 
	max_pt_g.coords.y = (float)(numRows);		
	max_pt_g.coords.z = 0.0;		
	max_pt_g.coords.w = 1.0;		

	mid_pt_g.coords.x = 0.0; 
	mid_pt_g.coords.y = (float)((numRows/2));	
	mid_pt_g.coords.z = 32.0; 	
	mid_pt_g.coords.w = 1.0;

	min_pt_g.coords.x = (float)(numCols); 	min_pt_g.coords.y = 0.0;
	min_pt_g.coords.z = 0.0; 			min_pt_g.coords.w = 1.0;

	max_pt_g.color.g = max_pt_g.colors[__GL_FRONTFACE].g = 1.0;
	max_pt_g.color.r = max_pt_g.colors[__GL_FRONTFACE].r = 0.0;
	max_pt_g.color.b = max_pt_g.colors[__GL_FRONTFACE].b = 0.0;
	max_pt_g.color.a = max_pt_g.colors[__GL_FRONTFACE].a = 0.0;

	mid_pt_g.color.g = mid_pt_g.colors[__GL_FRONTFACE].g = 1.0;
	mid_pt_g.color.r = mid_pt_g.colors[__GL_FRONTFACE].r = 0.0;
	mid_pt_g.color.b = mid_pt_g.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt_g.color.a = mid_pt_g.colors[__GL_FRONTFACE].a = 0.0;

	min_pt_g.color.g = min_pt_g.colors[__GL_FRONTFACE].g = 1.0;
	min_pt_g.color.r = min_pt_g.colors[__GL_FRONTFACE].r = 0.0;
	min_pt_g.color.b = min_pt_g.colors[__GL_FRONTFACE].b = 0.0;
	min_pt_g.color.a = min_pt_g.colors[__GL_FRONTFACE].a = 0.0;

	/* set up the black triangle */
	max_pt_b.coords.x = (float)(numCols); 
	max_pt_b.coords.y = (float)(numRows);		
	max_pt_b.coords.z = 16.0;		
	max_pt_b.coords.w = 1.0;		

	mid_pt_b.coords.x = 0.0; 
	mid_pt_b.coords.y = (float)((numRows/2));	
	mid_pt_b.coords.z = 16.0; 	
	mid_pt_b.coords.w = 1.0;

	min_pt_b.coords.x = (float)(numCols); 	min_pt_b.coords.y = 0.0;
	min_pt_b.coords.z = 16.0; 			min_pt_b.coords.w = 1.0;

	max_pt_b.color.g = max_pt_b.colors[__GL_FRONTFACE].g = 0.0;
	max_pt_b.color.r = max_pt_b.colors[__GL_FRONTFACE].r = 0.0;
	max_pt_b.color.b = max_pt_b.colors[__GL_FRONTFACE].b = 0.0;
	max_pt_b.color.a = max_pt_b.colors[__GL_FRONTFACE].a = 0.0;

	mid_pt_b.color.g = mid_pt_b.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt_b.color.r = mid_pt_b.colors[__GL_FRONTFACE].r = 0.0;
	mid_pt_b.color.b = mid_pt_b.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt_b.color.a = mid_pt_b.colors[__GL_FRONTFACE].a = 0.0;

	min_pt_b.color.g = min_pt_b.colors[__GL_FRONTFACE].g = 0.0;
	min_pt_b.color.r = min_pt_b.colors[__GL_FRONTFACE].r = 0.0;
	min_pt_b.color.b = min_pt_b.colors[__GL_FRONTFACE].b = 0.0;
	min_pt_b.color.a = min_pt_b.colors[__GL_FRONTFACE].a = 0.0;

	/* scene1 has different z values. scene2 has common z values */
	for (scene = scene_num; scene <= scene_num;  scene++) {
	   if (scene == 2) {
		min_pt_g.coords.z = mid_pt_g.coords.z =max_pt_g.coords.z =16.0;
		min_pt_r.coords.z = mid_pt_r.coords.z =max_pt_r.coords.z =16.0;
		min_pt_b.coords.z = mid_pt_b.coords.z =max_pt_b.coords.z =32.0;
	   }
	   for (mode = 0; mode < 8; mode++) {
		/* turn on z-enab since clear turns it off */
		fillmode.bits.ZiterEN = 1;
		cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
    		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    		HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

		ppfillmode.bits.ZEnab = 1;
		cmd=BUILD_CONTROL_COMMAND(0,0,0,0,
			(0x1800 + (PP_FILLMODE & 0x1ff)), 4);
    		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    		HQ3_PIO_WR(CFIFO1_ADDR, ppfillmode.val, ~0x0);

		/* set the mode for the red triangle */
		ppzmode.bits.Zfunc = PP_ZF_ALWAYS; 
		cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_ZMODE&0x1ff)),4);
    		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    		HQ3_PIO_WR(CFIFO1_ADDR,(ppzmode.val| 0x0ffffff0),~0x0);

		/* draw the black triangle */
		_mg0_FillTriangle(&max_pt_b, &mid_pt_b,&min_pt_b,depthkludge_b);

		/* draw the red triangle */
		_mg0_FillTriangle(&max_pt_r, &mid_pt_r,&min_pt_r,depthkludge_r);

		/* set the depthfunc */
		ppzmode.bits.Zfunc = depthfunclist[mode];
		cmd =BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800+(PP_ZMODE&0x1ff)),4);
    		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    		HQ3_PIO_WR(CFIFO1_ADDR,(ppzmode.val| 0x0ffffff0),~0x0);

		/* draw the green triangle */
		_mg0_FillTriangle(&max_pt_g, &mid_pt_g,&min_pt_g,depthkludge_g);

#if 0
		fprintf(stderr, "-----------------------------------------\n");
		fprintf(stderr, "scene: %d , mode: %d\n", scene, mode);
#endif
		switch (scene) {
		   case 1:
			switch (mode) {
				case 0: tri = s1t0; 
					exp_crc_index = ZTRI_S1T0_CRC_INDEX;
					crc_pat = (__uint32_t)s1t0_checksum; break;
				case 1: tri = s1t1; 
					exp_crc_index = ZTRI_S1T1_CRC_INDEX;
					crc_pat = (__uint32_t)s1t1_checksum; break;
				case 2: tri = s1t0; 
					exp_crc_index = ZTRI_S1T2_CRC_INDEX;
					crc_pat = (__uint32_t)s1t0_checksum; break;
				case 3: tri = s1t1; 
					exp_crc_index = ZTRI_S1T3_CRC_INDEX;
					crc_pat = (__uint32_t)s1t1_checksum; break;
				case 4: tri = s1t4; 
					exp_crc_index = ZTRI_S1T4_CRC_INDEX;
					crc_pat = (__uint32_t) s1t4_checksum; break;
				case 5: tri = s1t5; 
					exp_crc_index = ZTRI_S1T5_CRC_INDEX;
					crc_pat = (__uint32_t)s1t5_checksum; break;
				case 6: tri = s1t4; 
					exp_crc_index = ZTRI_S1T6_CRC_INDEX;
					crc_pat = (__uint32_t) s1t4_checksum; break;
				case 7: tri = s1t5; 
					exp_crc_index = ZTRI_S1T7_CRC_INDEX;
					crc_pat = (__uint32_t) s1t5_checksum; break;
			};
			break;
		   case 2:
			switch (mode) {
				case 0: tri = s1t0; 
					exp_crc_index = ZTRI_S2T0_CRC_INDEX;
					crc_pat = (__uint32_t) s1t0_checksum; break;
				case 1: tri = s2t1;
					exp_crc_index = ZTRI_S2T1_CRC_INDEX;
					crc_pat = (__uint32_t) s2t1_checksum; break;
				case 2: tri = s2t2; 
					exp_crc_index = ZTRI_S2T2_CRC_INDEX;
					crc_pat = (__uint32_t) s2t2_checksum; break;
				case 3: tri = s1t5; 
					exp_crc_index = ZTRI_S2T3_CRC_INDEX;
					crc_pat = (__uint32_t) s1t5_checksum; break;
				case 4: tri = s1t0; 
					exp_crc_index = ZTRI_S2T4_CRC_INDEX;
					crc_pat = (__uint32_t) s1t0_checksum; break;
				case 5: tri = s2t1;
					exp_crc_index = ZTRI_S2T5_CRC_INDEX;
					crc_pat = (__uint32_t) s2t1_checksum; break;
				case 6: tri = s2t2; 
					exp_crc_index = ZTRI_S2T6_CRC_INDEX;
					crc_pat = (__uint32_t) s2t2_checksum; break;
				case 7: tri = s1t5; 
					exp_crc_index = ZTRI_S2T7_CRC_INDEX;
					crc_pat = (__uint32_t) s1t5_checksum; break;
			};
			break;
		}

		errors += _readback(rssnum, numRows, numCols, tri, reReadMode,
			0, rssDmaPar, hq3DmaPar, crc_pat, exp_crc_index, do_crc,
			0, 0, 0, 0);

		if (errors) {
		   msg_printf(ERR,"Z Triangle Test: Triangle %d of 15 failed\n",
			mode + (scene-1)*8);
		   if (stop_on_err) {
			if (rflag == 0) rssnum = 0;
	 		REPORT_PASS_OR_FAIL((&RE_err[RE4_Z_TRI_TEST]), errors);
		   }
		}
		else {
		   msg_printf(DBG,"Z Triangle Test: Triangle %d of 15 passed\n",
			mode + (scene-1)*8);
		}
		
		_mg0_block(PP_FILLMODE_DBUFA,0.0,0.0,0.0,0.0, 0, 0, XEND, YEND);

	   } /* mode */
	} /* scene */

	REPORT_PASS_OR_FAIL((&RE_err[RE4_Z_TRI_TEST]), errors);
}

__uint32_t
mgras_z_tri(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t stop_on_err = 1;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	errors =_mgras_z_tri(1, rflag, stop_on_err, do_crc);
	if ((errors == 0) || (stop_on_err == 0))
		errors += _mgras_z_tri(2, rflag, stop_on_err, do_crc);

	REPORT_PASS_OR_FAIL((&RE_err[RE4_Z_TRI_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_points()							  *
 *  Draw rgb, aa-rgb, and x points.                                       *
 *									  * 
 *  Will take a -i switch to test writing the PP1 via indirect mode	  *
 *									  *
 **************************************************************************/


__uint32_t
mgras_points(int argc, char ** argv)
{
	__uint32_t numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0;
	__uint32_t stop_on_err, aa_enable, x_enable, rflag = 0, bad_arg = 0;
	__uint32_t do_crc=0, do_indirect = 1; /* 1 = false, 2 = true */
	float red, green, blue, alpha, xstart, ystart;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;
	__uint32_t points_array[30] = {
		0xff8080ff, 0x0, 0x0, 0x0, 0x0, 0x0, 
		0x0, 0x0, 0x7e33ff7f, 0x33ff80, 0x0, 0x0, 
		0x0, 0x0, 0x7f33ff80, 0x33ff7f, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0xffff3f7f, 0x0};

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	do_indirect = 2;
#endif
	/* initialize to test everything together */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
	rssnum = 0;
	stop_on_err = 1;
	numCols = 6; numRows = 4;  /* stay on 1 page in fb memory */

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, DMA_PP1,
		do_indirect);

	red = 1.0; green = 0.5; blue = 0.5; alpha = 1.0;
	aa_enable = 0; x_enable = 0;
	xstart = ystart = 0.0;

	/* normal point */
	_mgras_pt(xstart, ystart, red, green, blue, alpha, aa_enable, 
		x_enable, 0, 0);

	/* aa point */
	red = 0.5; green = 1.0; blue = 0.2; alpha = 1.0;
	xstart = 2.0; ystart = 2.0; aa_enable = 1;
	_mgras_pt(xstart, ystart, red, green, blue, alpha, aa_enable, 
		x_enable, 0, 0);

	/* x-point */
	red = 0.5; green = 0.25; blue = 1.0; alpha = 1.0;
	xstart = 4.0; ystart = 3.0; aa_enable = 0; x_enable = 1;
	_mgras_pt(xstart, ystart, red, green, blue, alpha, aa_enable, 
		x_enable, 0, 0);

	errors += _readback(rssnum, numRows, numCols, points_array, reReadMode,
		0, rssDmaPar, hq3DmaPar, points_array_checksum & 0xffffffff,
		POINTS_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors & stop_on_err) {
		if (rflag == 0) rssnum = 0;
		REPORT_PASS_OR_FAIL((&RE_err[RE4_POINTS_TEST]), errors);
	}
		
#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif

	REPORT_PASS_OR_FAIL((&RE_err[RE4_POINTS_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_lines()							  *
 *                                                                        *
 * Test the line functionality.						  *
 *                                                                        *
 *									  *
 **************************************************************************/

__uint32_t
mgras_lines(int argc, char ** argv)
{
	__uint32_t numCols, numRows, reReadMode, bad_arg = 0;
	__uint32_t rssnum, errors = 0, stip_en, aa_en, x_en;
	__uint32_t do_crc=0, stop_on_err=1, lspat_val, rflag = 0;
	Vertex max_pt, min_pt;
	lscrlu		lscrl = {0};
	
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params       *hq3DmaPar = &mgras_Hq3DmaParams;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	do_crc = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
	rssnum = 0;
        numCols = 14; numRows = 17; /* stay on 1 page */
	stop_on_err = 1;

	stip_en = aa_en = x_en = 0;

	lscrl.bits.LineStipCount = 0;
	lscrl.bits.LineStipRepeat = 0; 
	lscrl.bits.LineStipLength = 0;
	lspat_val = 0;
	
	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, DMA_PP1,
		1);

	max_pt.coords.x = 4.0; max_pt.coords.y = 16.0;	
	max_pt.coords.z = 0.0;	max_pt.coords.w = 1.0;		

	min_pt.coords.x = 0.0; 	min_pt.coords.y = 0.0;	
	min_pt.coords.z = 0.0; 	min_pt.coords.w = 1.0;

	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 1.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	/* rgb line */
	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val,aa_en,x_en,0,
		0, rssDmaPar);

	/* rgb aa line */
	aa_en = 1;
	max_pt.coords.x = 9.0; max_pt.coords.y = 16.0;	
	min_pt.coords.x = 5.0;	min_pt.coords.y = 0.0;		
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 0.0;
	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 1.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 1.0;

	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val,aa_en,x_en,0,
		0, rssDmaPar);

	/* stippled, aliased line */

	stip_en = 1; aa_en = 0;
	lspat_val = 0xa9876543;
	lscrl.bits.LineStipLength = 0xf; /* go forever */
	lscrl.bits.LineStipRepeat = 0;	/* 0 = repeat just 1 time */
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 1.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;

	/* actually draw 2 lines */
	max_pt.coords.x = 5.0; max_pt.coords.y = 5.0;	
	min_pt.coords.x = 0.0;	min_pt.coords.y = 5.0;		
	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val,aa_en,x_en,0,
		0, rssDmaPar);
	
	max_pt.coords.x = 10.0; max_pt.coords.y = 5.0;	
	min_pt.coords.x = 5.0;	min_pt.coords.y = 5.0;		
	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val,aa_en,x_en,0,
		0, rssDmaPar);

	/* draw an x-line */
	x_en = 1; stip_en = 0;
	max_pt.coords.x = 12.0; max_pt.coords.y = 10.0;	
	min_pt.coords.x = 12.0;	min_pt.coords.y = 0.0;		
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.5;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.5;
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 0.5;
	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 0.5;
	_mg0_Line(&min_pt, &max_pt, stip_en, lscrl.val, lspat_val,aa_en,x_en,0,
		0, rssDmaPar);

	errors += _readback(rssnum, numRows, numCols, lines_array, reReadMode,
		0, rssDmaPar, hq3DmaPar, lines_array_checksum & 0xffffffff,
		LINES_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors & stop_on_err) {
		if (rflag == 0) rssnum = 0;
		REPORT_PASS_OR_FAIL((&RE_err[RE4_LINES_TEST]), errors);
	}
		
#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif

	REPORT_PASS_OR_FAIL((&RE_err[RE4_LINES_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 *  mgras_stip_tri()							  *
 *                                                                        *
 * Draw a stippled triangle.						  *
 * The stipple pattern is written to the stipple ram.                     *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_stip_tri(int argc, char ** argv)
{
	__uint32_t i, cmd, numCols, numRows, reReadMode, depthkludge = 0;
	__uint32_t rssnum, errors = 0;
	__uint32_t do_crc=0, stop_on_err = 1, rflag = 0, bad_arg = 0;
	Vertex max_pt, mid_pt, min_pt;
	fillmodeu     fillmode = {0};
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
        numCols = 24; numRows = 16; /* stay on 1 page */

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, DMA_PP1,
		1);

	/* common x,y,z */
	max_pt.coords.x = 0.0; 		
	max_pt.coords.y = (float)(numRows-1);		
	max_pt.coords.z = 0.0;		
	max_pt.coords.w = 1.0;		

	mid_pt.coords.x = (float)(numCols-1); 	
	mid_pt.coords.y = (float)((numRows/2)-1);	
	mid_pt.coords.z = 0.0;
	mid_pt.coords.w = 1.0;

	min_pt.coords.x = 0.0; 	min_pt.coords.y = 0.0;	
	min_pt.coords.z = 0.0;	 min_pt.coords.w = 1.0;

	/* Red triangle */
	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 0.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 0.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	mid_pt.color.r = mid_pt.colors[__GL_FRONTFACE].r = 1.0;
	mid_pt.color.g = mid_pt.colors[__GL_FRONTFACE].g = 0.0;
	mid_pt.color.b = mid_pt.colors[__GL_FRONTFACE].b = 0.0;
	mid_pt.color.a = mid_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 1.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 0.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 0.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

	/* set up needed registers */
        fillmode.bits.ZiterEN = 1;
        fillmode.bits.ShadeEN = 1;
        fillmode.bits.RoundEN = 1;
	fillmode.bits.PolyStipEn = 1;	
	cmd = BUILD_CONTROL_COMMAND(0,0,0,0,(0x1800 + FILLMODE), 4);
    	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
    	HQ3_PIO_WR(CFIFO1_ADDR, fillmode.val, ~0x0);

	/* Write 16 pairs of 32-bit words for the half tone stipple pattern */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_ADDR))), POLYSTIP_RAM,
                THIRTYTWOBIT_MASK);
	for (i = 0; i < 16; i++)
	{
	   /* write 16 pairs of 32-bit words for the half tone */
	   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_DATA))), 0xaaaaaaaa, 
		THIRTYTWOBIT_MASK);	
	   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_DATA))), 0x55555555, 
		THIRTYTWOBIT_MASK);	
	}
	
	_mg0_FillTriangle(&max_pt, &mid_pt, &min_pt, depthkludge);

	errors += _readback(rssnum, numRows, numCols, stip_tri, reReadMode,
		0, rssDmaPar, hq3DmaPar, stip_tri_checksum & 0xffffffff,
		POLYSTIP_CRC_INDEX, do_crc, 0, 0, 0, 0);

	if (errors & stop_on_err) {
	 if (rflag == 0) rssnum = 0;
	 REPORT_PASS_OR_FAIL( (&RE_err[RE4_POLYSTIP_TEST]), errors);
	}

#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif

	REPORT_PASS_OR_FAIL( (&RE_err[RE4_POLYSTIP_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 * mgras_block()							  *
 *                                                                        *
 * Draw a block and read it back.					  *
 *                                                                        *
 **************************************************************************/


__uint32_t
mgras_block(int argc, char ** argv)
{
	__uint32_t x, y, numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0;
	__uint32_t do_crc=0, stop_on_err = 1, rflag = 0, bad_arg = 0;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;
	__uint32_t block_array[384];

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
        numCols = 24; numRows = 16; /* stay on 1 page */

	for (y = 0; y < numRows; y++) {
	  	for (x = 0; x < numCols; x+=2) {
		   if (y % 2) { /* odd rows */
			block_array[x + numCols*y] = 0xff7f7f7f;
			block_array[x+1 + numCols*y] = 0xff808080;
		   }
		   else { /* even rows */
			block_array[x + numCols*y] = 0xff808080;
			block_array[x+1 + numCols*y] = 0xff7f7f7f;
		   }
	   	}
	}

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_PP1, 1);

#if 1
		_mg0_block(PP_FILLMODE_DBUFA, 0.5, 0.5, 0.5, 1.0, 0, 0, 
			numCols-1, numRows-1);
#else
		dith_off = 1;
		_mg0_block(PP_FILLMODE_DBUFA, 1.0, 0, 0, 0, 0, 0, 
			1279, 1023);

#endif
		errors+= _readback(rssnum,numRows,numCols,block_array,
			reReadMode, 0, rssDmaPar, hq3DmaPar, 
			block_array_checksum & 0xffffffff, XBLK_CRC_INDEX, 
			do_crc, 0, 0, 0, 0);

		if (errors & stop_on_err) {
	 	if (rflag == 0) rssnum = 0;
	 	REPORT_PASS_OR_FAIL( (&RE_err[RE4_XBLK_TEST]), errors);
		}

#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif

	REPORT_PASS_OR_FAIL( (&RE_err[RE4_XBLK_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 * mgras_chars()							  *
 *                                                                        *
 * Draw block characters						  *
 *                                                                        *
 **************************************************************************/

__uint32_t
mgras_chars(int argc, char ** argv)
{
	__uint32_t numCols, numRows, reReadMode;
	__uint32_t rssnum, errors = 0;
	__uint32_t stop_on_err = 1, xstart, ystart, xend, yend;
	__uint32_t do_crc=0, rflag = 0, bad_arg = 0;
	float red, green, blue, alpha;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	reReadMode = DMA_PIO;
        numCols = 40; numRows = 16; /* stay on 1 page */

		_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_PP1, 1);

		xstart = 0; xend = 7; 
		ystart = 0; yend = 7;
		red = 1.0; green = 0.5; blue = 0.25; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&G, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 8; xend = 15; 
		red = 0.5; green = 0.25; blue = 1.0; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&R, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 16; xend = 23;
		red = 0.25; green = 1.0; blue = 0.5; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&A, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 24; xend = 31;
		red = 1.0; green = 0.25; blue = 0.5; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&S, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 0; xend = 7;
		ystart = 8; yend = 15;
		red = 0.25; green = 0.5; blue = 1.0; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&M, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 8; xend = 15; 
		red = 0.5; green = 1.0; blue = 0.25; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&A, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 16; xend = 23;
		red = 0.5; green = 0.5; blue = 0.25; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&R, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 24; xend = 31;
		red = 0.0; green = 0.5; blue = 0.25; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&D, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		xstart = 32; xend = 39; 
		red = 1.0; green = 0.5; blue = 0.0; alpha = 1.0;
		_draw_char(rssnum, (ushort_t*)&I, xstart, ystart, xend, yend, red, green, 
			blue, alpha);

		errors+= _readback(rssnum,numRows,numCols,char_array,reReadMode,
			0,rssDmaPar,hq3DmaPar,char_array_checksum & 0xffffffff,
			CHARS_CRC_INDEX, do_crc, 0, 0, 0, 0);

		if (errors & stop_on_err) {
			if (rflag == 0) rssnum = 0;
	 		REPORT_PASS_OR_FAIL((&RE_err[RE4_CHARS_TEST]), errors);
		}
#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif

	REPORT_PASS_OR_FAIL( (&RE_err[RE4_CHARS_TEST]), errors);
}

/**************************************************************************
 *                                                                        *
 * _mgras_tex_line() -- see mgras_tex_line(), mgras_notex_line() after    *
 * this routine.							  *
 *                                                                        *
 * Draw line (1 x numCols rectangle) and write data to the trams. Draw    *
 * 2 lines, one with texturing enabled and one without texturing but      *
 * data in the tram to make sure no data "leaks" out from the tram.	  *
 * The line data and the tram data are complements of each other.	  *
 *                                                                        *
 **************************************************************************/

__uint32_t
_mgras_tex_line(__uint32_t tex_en, __uint32_t numCols, __uint32_t rflag, __uint32_t stop_on_err, __uint32_t do_crc)
{
	__uint32_t i, numRows, reReadMode;
	__uint32_t rssnum, errors = 0;
	Vertex max_pt, min_pt;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;
	__uint32_t notex_line[128];

	/* Our line is 8888 data, but the texture is 4444 so the texture data
 	 * gets doubled so a texel of 0xb5eb becomes 0xbb55eebb in the frame
	 * buffer.
	 */
	__uint32_t  tex_line[64] = {
	0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33, 0xbbffbbee, 0xbbddeeff, 0xbbffbbee, 0xbbddeebb, 0xbb77bb66, 0xbb55eeff, 0xbb77bb66, 0xbb55eebb, 0xaaaaaaaa, 0xaaaaffff, 0xaaaaaaaa, 0xaaaaffbb, 0x55555555, 0x5555ffff, 0x55555555, 0x5555ffbb, 0x33333333, 0x3333ff77, 0x33333333, 0x3333ff33, 0x0, 0xff77, 0x0, 0xff33};

	__uint32_t telod_128[128] = {
0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x6688dd99, 0xbbffbbee, 0xbbeeddff, 0xbbddeeff, 0xbbeeddff, 0xbbffbbee, 0xbbeedddd, 0xbbddeebb, 0xbbaadd99, 0xbb77bb66, 0xbb66ddbb, 0xbb55eeff, 0xbb66ddbb, 0xbb77bb66, 0xbb66dd99, 0xbb55eebb, 0xbb88ccbb, 0xaaaaaaaa, 0xaaaadddd, 0xaaaaffff, 0xaaaadddd, 0xaaaaaaaa, 0xaaaaddbb, 0xaaaaffbb, 0x8888aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x22228822, 0x0, 0x8844, 0xff77, 0x8844, 0x0, 0x8822, 0xff33, 0x5555dd77};

	__uint32_t telod_128_4tram[128] = {
0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955,
0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff, 0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33, 0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33,
0x5d80dd91, 0xbbffbbee, 0xbbeed5f7, 0xbbddeeff, 0xbbeed5f7, 0xbbffbbee, 0xbbeed5d5, 0xbbddeebb, 0xbbaad591, 0xbb77bb66, 0xbb66d5b3, 0xbb55eeff, 0xbb66d5b3, 0xbb77bb66, 0xbb66d591, 0xbb55eebb, 0xb380ccb3, 0xaaaaaaaa, 0xaaaad5d5, 0xaaaaffff,
0xaaaad5d5, 0xaaaaaaaa, 0xaaaad5b3, 0xaaaaffbb, 0x8080aa88, 0x55555555, 0x5555aaaa, 0x5555ffff, 0x5555aaaa, 0x55555555, 0x5555aa88, 0x5555ffbb, 0x44449977, 0x33333333, 0x33339955, 0x3333ff77, 0x33339955, 0x33333333, 0x33339933, 0x3333ff33,
0x19198019, 0x0, 0x803b, 0xff77, 0x803b, 0x0, 0x8019, 0xff33, 0x5555d56e};

	__uint32_t telod_32[32] = {
0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333, 0x33333333, 0x0, 0x0, 0xbbffbbee, 0xbbffbbee, 0xbb77bb66, 0xbb77bb66, 0xaaaaaaaa, 0xaaaaaaaa, 0x55555555, 0x55555555, 0x33333333,
0x33333333, 0x0, 0x0};

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test.\n");
		return (0);
	}

	rssnum = 4;
	reReadMode = DMA_PIO;
        numRows = 1; /* stay on 1 page */

	for (i = 0; i < numCols; i++) {
		notex_line[i] = 0xffffffff;
	}

	max_pt.coords.x = numCols; max_pt.coords.y = 0.0;	
	max_pt.coords.z = 0.0;	max_pt.coords.w = 1.0;		

	min_pt.coords.x = 0.0; 	min_pt.coords.y = 0.0;	
	min_pt.coords.z = 0.0; 	min_pt.coords.w = 1.0;

	max_pt.texture.x = 1.0;
        max_pt.texture.y = max_pt.coords.y;
        max_pt.texture.z = max_pt.coords.z;
        max_pt.texture.w = max_pt.coords.w;

	min_pt.texture.x = 0.0;
        min_pt.texture.y = min_pt.coords.y;
        min_pt.texture.z = min_pt.coords.z;
        min_pt.texture.w = min_pt.coords.w;

	max_pt.color.r = max_pt.colors[__GL_FRONTFACE].r = 1.0;
	max_pt.color.g = max_pt.colors[__GL_FRONTFACE].g = 1.0;
	max_pt.color.b = max_pt.colors[__GL_FRONTFACE].b = 1.0;
	max_pt.color.a = max_pt.colors[__GL_FRONTFACE].a = 1.0;

	min_pt.color.r = min_pt.colors[__GL_FRONTFACE].r = 1.0;
	min_pt.color.g = min_pt.colors[__GL_FRONTFACE].g = 1.0;
	min_pt.color.b = min_pt.colors[__GL_FRONTFACE].b = 1.0;
	min_pt.color.a = min_pt.colors[__GL_FRONTFACE].a = 1.0;

#ifndef MGRAS_DIAG_SIM
	   _re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_TRAM, 0);

	   /* mods to the default */
	   rssDmaPar->xfrDevice = DMA_TRAM; 
	   hq3DmaPar->IM_pg_width = numCols * 2;
   	   hq3DmaPar->IM_first_sp_width = numCols * 2;

	   set_xstart = tram_mem_pat = trampg_start = 0;
	   dmawrite_only = dmaread_only = tram_wr_flag = 0;
	   dma_overwrite_flag = 0;

	   tram_mem_pat = TRAM_TEX_LINE_PAT;
	   /* readback also sets up needed regs */
	   errors += _mgras_hq_redma(DMA_TRAM, PIO_OP, DMA_BURST, PIO_OP, 
		DMA_PIO, PAT_INV, 1, 64, 1, 1, 0, 1, 0);
#endif

	   _re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
			DMA_PP1, 1);

	   /* additional set up */
	   if (tex_en) {
		WRITE_RSS_REG(rssnum, TXCLAMPSIZE, 0x40080, ~0x0);
		WRITE_RSS_REG(rssnum, TXBASE, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXSIZE, 0x0606, ~0x0);
		WRITE_RSS_REG(rssnum, TXADDR, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXMIPMAP, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXADDR, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXBORDER, 0x4, ~0x0);
		WRITE_RSS_REG(rssnum, TEXRBUFFER, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXLOD, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TEXMODE2, 0x14080, ~0x0);
		WRITE_RSS_REG(rssnum, TXBCOLOR_RG, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXBCOLOR_BA, 0xfffe66, ~0x0);
		WRITE_RSS_REG(rssnum, TXENV_RG, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TXENV_B, 0, ~0x0);
		WRITE_RSS_REG(rssnum, TEXMODE1, 0x40819, ~0x0);
	   }
	   
	   _mg0_Line(&min_pt, &max_pt, 0, 0, 0, 0, 0, tex_en, 0, rssDmaPar);

	   switch (tex_en) {
	   	case 64: 
		   switch (numCols) {
			case 32:
	   			errors += _readback(rssnum,numRows,numCols,
				   telod_32,reReadMode,0, rssDmaPar, hq3DmaPar,
					telod_32_checksum & 0xffffffff, 
					ILLEGAL_CRC_INDEX, do_crc, 0, 0, 0, 0);
				break;
			case 64:
	   			errors += _readback(rssnum,numRows,numCols,
				   tex_line,reReadMode,0, rssDmaPar, hq3DmaPar,
					tex_line_checksum & 0xffffffff, 
					ILLEGAL_CRC_INDEX, do_crc, 0, 0, 0, 0);
				break;
			case 128:
				NUMBER_OF_TRAMS();
#ifdef MGRAS_DIAG_SIM
				numTRAMs = 1;
#ifdef TRAM_4
				numTRAMs = 4;
#endif
#endif
				if (numTRAMs == 1) {
	   			   errors += _readback(rssnum,numRows,numCols,
				    telod_128,reReadMode,0,rssDmaPar,hq3DmaPar,
					telod_128_checksum & 0xffffffff,
					ILLEGAL_CRC_INDEX, do_crc, 0, 0, 0, 0);
				} 
				else if (numTRAMs == 4) {
	   			   errors += _readback(rssnum,numRows,numCols,
				    	telod_128_4tram, reReadMode, 0,
					rssDmaPar,hq3DmaPar, 
					telod_128_4tram_checksum & 0xffffffff,
					ILLEGAL_CRC_INDEX, do_crc, 0, 0, 0, 0);
				} 
				break;
		   };
		   break;
	   	case 0:
	   		errors += _readback(rssnum,numRows,numCols,notex_line,
				reReadMode, 0, rssDmaPar, hq3DmaPar,
				notex_line_checksum & 0xffffffff,
				NOTEX_LINE_CRC_INDEX, do_crc, 0,0,0, 0);
			break;
		default:
			errors++; break;
	   };

	   if (errors & stop_on_err) {
		rssnum++;
		goto ERR_LABEL;
	   }

#if 0
	/* reset ranges to test all RSS's together if user has not specified
	   a particular RSS to test */
	if ((rflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {
		rssnum = 3; rssend = 5;
	}
#endif


ERR_LABEL:
	rssnum--;
	if (rflag == 0) rssnum = 0;
	switch (tex_en) {
	   case 64: 
		switch (numCols) {
		   case 32:
			REPORT_PASS_OR_FAIL( (&TE_err[TE1_LOD_TEST]), errors);
			/*NOTREACHED*/
			break;
		   case 64:
			REPORT_PASS_OR_FAIL( (&RE_err[RE4_TEX_TEST]), errors);
			/*NOTREACHED*/
			break;
		   case 128:
			REPORT_PASS_OR_FAIL( (&TE_err[TE1_LOD_TEST]), errors);
			/*NOTREACHED*/
			break;
	 	};
		break;
	   case 0:
	 	REPORT_PASS_OR_FAIL( (&RE_err[RE4_NOTEX_TEST]), errors);
		/*NOTREACHED*/
		break;
	};

	return (errors);
}

__uint32_t
mg_tex_line(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t stop_on_err = 1;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	numTRAMs = 1;
#endif

	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test.\n");
	}
	else {
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	errors = _mgras_tex_line(64, 64, rflag, stop_on_err, do_crc);
	}

	return(errors);
}

__uint32_t
mg_notex_line(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t stop_on_err = 1;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
	numTRAMs = 1;
#endif
	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test.\n");
	}
	else {
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	errors = _mgras_tex_line(0, 64, rflag, stop_on_err, do_crc);
	}

	return(errors);
}

__uint32_t
mgras_telod(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t stop_on_err = 1;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test.\n");
	}
	else {
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	errors += _mgras_tex_line(64, 128, rflag, stop_on_err, do_crc);

	if ((errors == 0) || (stop_on_err == 0))
		errors += _mgras_tex_line(64, 32, rflag, stop_on_err, do_crc);

	}

	return(errors);
}

__uint32_t
mgras_detail_tex(int argc, char ** argv)
{
	__uint32_t do_crc=0, errors = 0, rflag = 0, bad_arg = 0, rssnum;
	__uint32_t stop_on_err = 1;

	NUMBER_OF_RE4S();
#ifdef MGRAS_DIAG_SIM
	numRE4s = 1;
#endif
	if (numTRAMs == 0) {
		msg_printf(SUM, "No TRAMs detected, skipping this test.\n");
	}
	else {
	/* initialize to test everything */
        rssstart = CONFIG_RE4_ALL; rssend = rssstart + 1;

	GET_RSS_NUM();

	errors = _mgras_tex_line(64, 32, rflag, stop_on_err, do_crc);
	}

	return (errors);
}

/*
 * Sets up registers to draw to the overlay register 
 *
 */
__uint32_t
mgras_turn_overlay_on(void)
{
	
	ppfillmodeu	ppfillmode = {0};

    	ppfillmode.bits.PixType = PP_FILLMODE_CI8;
    	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DOVERA_OR_B;
    	ppfillmode.bits.BufSize = PP_FILLMODE_FULL_SIZE;
    	ppfillmode.bits.ReadBuffer = PP_FILLMODE_ROVERA_OR_B;

	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)),
		 ppfillmode.val, 0xffffffff);

	pp_drb = PP_1_RSS_DRB_OVERLAY; /* same for 1 or 2 RSS */
	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE((PP_DRBPOINTERS & 0x1ff)),
		 pp_drb, 0xffffffff);

	global_dbuf = PP_FILLMODE_DOVERA_OR_B; /* 0x41 */

	return 0;
}

/*
 * Sets up registers to display the overlay buffer on the screen
 *
 */
__uint32_t
mg_enabdisp_overlay(void)
{
	__uint32_t i;

	/* Init the Olay mode register file (MGRAS_XMAP_OVERLAY_MODE) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
        for (i = 0; i < 8; i++) {
                HQ3_PIO_WR(MGRAS_XMAP_OVERLAY_MODE_OFF, 0x1, ~0x0);
        }
	return 0;
}


/*
 * Sets up registers to disable displaying the overlay buffer
 *
 */
__uint32_t
mg_disabdisp_overlay(void)
{
	__uint32_t i;

	/* Init the Olay mode register file (MGRAS_XMAP_OVERLAY_MODE) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
        for (i = 0; i < 8; i++) {
                HQ3_PIO_WR(MGRAS_XMAP_OVERLAY_MODE_OFF, 0x0, ~0x0);
        }
	return 0;
}


/*
 * Turn off overlay drawing and go back to main mode drawing
 *
 */
__uint32_t
mgras_turn_overlay_off(void)
{
	ppfillmodeu	ppfillmode = {0};

    	ppfillmode.bits.PixType = PP_FILLMODE_RGBA8888;
    	ppfillmode.bits.DrawBuffer = PP_FILLMODE_DBUFA;
    	ppfillmode.bits.BufSize = PP_FILLMODE_FULL_SIZE;
    	ppfillmode.bits.ReadBuffer = PP_FILLMODE_RBUFA;

	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE((PP_FILLMODE & 0x1ff)),
		 ppfillmode.val, 0xffffffff);

	pp_drb = PP_1_RSS_DRB_AB; /* same for 1 or 2 RSS */
	HQ3_PIO_WR_RE(RSS_BASE+HQ_RSS_SPACE((PP_DRBPOINTERS & 0x1ff)),
		 pp_drb, 0xffffffff);

	global_dbuf = PP_FILLMODE_DBUFA; /* 0x1 */

	return 0;
}

/*
 * Taken from the verif alphablend test but only tests 5 modes.
 * Assumptions: mg0_clear_color has been run before this test
 * 		Uses mg_crc_test to verify correctness.
 *
 */
__uint32_t
mg_alphablend(int argc, char ** argv)
{
	__uint32_t rssnum, rflag = 0, do_crc = 0, stop_on_err = 1, bad_arg = 0;
	__uint32_t xstart, ystart, reReadMode, numCols, numRows;
	__uint32_t db_enable=0, errors = 0;
	RSS_DMA_Params  *rssDmaPar = &mgras_RssDmaParams;
 	HQ3_DMA_Params  *hq3DmaPar = &mgras_Hq3DmaParams;

	GET_RSS_NUM();	

	/* initialize to test everything */
	rssnum = 4;
	reReadMode = DMA_PIO;
	numCols = 10; numRows = 90;
	xstart = 20; ystart = 20;
	_mg_rdram_error_index = 0;
	global_startx = xstart;
	global_starty = ystart;

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
		DMA_PP1, 1);

	/* assumes mg0_clear_color has been run before this to clear sceen */
	errors += _mg_alphablend();

	/* use mg_crc_test in the scripts to check the crc checksum */

	_re4func_setup(rssnum, numCols, numRows, rssDmaPar, hq3DmaPar, 
		DMA_PP1, 0);

	errors +=_readback(rssnum,numRows,numCols, alphablend_array,reReadMode, 
		db_enable, rssDmaPar, hq3DmaPar, 0 & 0xffffffff, 
		0 ,do_crc, xstart, ystart, 0, 0);

	/* reset to what everybody else wants */
	global_startx = 0;
	global_starty = 0;

	REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), errors);

/* doesn't work yet ... */
#if 0
	/* check the CRC here */

#ifdef MFG_USED
	mgras_VC3ClearCursor();
#endif

	if ((write_only == 0) && (nocompare == 0)) { /* do compare */
		errors += _mg_dac_crc_rss_pp(0, 0,0x167, 0x3b2, 0x3a0, 0x1d2, 
				0x1d, 0x2d6);
		if ((stop_on_err == 1) && (errors)) {

			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();

			REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), 
				errors);
		}

		errors += _mg_dac_crc_rss_pp(0, 1,0x254, 0x370, 0x266, 0xf8, 
			0x235, 0xc4);
		if ((stop_on_err == 1) && (errors)) {

			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();

			REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), 
				errors);
		}

		if (numRE4s == 2) {
		   errors += _mg_dac_crc_rss_pp(1, 0,0,0,0,0x167, 0x3b2, 0x3a0);
		   if ((stop_on_err == 1) && (errors)) {

			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();

			REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), 
				errors);
		   }

		   errors += _mg_dac_crc_rss_pp(1, 1,0,0,0,0x254, 0x370, 0x266);
		   if ((stop_on_err == 1) && (errors)) {

			/* reset config register */
			HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),
				CONFIG_RE4_ALL, TWELVEBIT_MASK);

			mg_restore_pps();

			REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), 
				errors);
		   }
		}
	}
	REPORT_PASS_OR_FAIL((&RE_err[RE4_ALPHA_BLEND_TEST]), errors);
#endif
}		
