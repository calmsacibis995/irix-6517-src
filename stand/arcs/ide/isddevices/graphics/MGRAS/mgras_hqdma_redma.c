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
 *  DMA Diagnostics.
 *  	o Host <-> RE transfer test
 *  	o Host <-> GE transfer test
 *  	o RE <-> RE transfer test
 *
 *  $Revision: 1.114 $
 */

#include <sys/types.h>
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include <sys/sbd.h>
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "mgras_diag.h"
#include "ide_msg.h"
#include "mgras_dma.h"

#define	DMA_DEBUG	0
/*
 * The following constant makes sure that the tests
 * use the general purpose scratch buffer rather than
 * a dedicated buffer for storing translation table entries
 */
#define trans_table mgras_scratch_buf

uchar_t _mgras_vsync = 0;
__uint32_t _mgras_rtdma_enabled = 0;
__uint32_t dmawrite_only = 0;
__uint32_t dmaread_only = 0;
__uint32_t nocompare;
__uint32_t trampg_start = 0;
__uint32_t tram_mem_pat = 0;
__uint32_t r_to_l = 0;
__uint32_t set_xstart = 0;
__uint32_t set_ystart = 0;
__uint32_t set_dma = 0;
__uint32_t tram_wr_flag = 0;
__uint32_t tram_wr_data;
__uint32_t which_rss_for_tram_rd = 0;
__uint32_t pp_wr_flag = 0;
__uint32_t pp_wr_data;
__uint32_t pp_dma_pat = PP_WALK_0_1_TEST; /* initialize */
__uint32_t pp_dma_mode = PP_RGB12; 
__uint32_t pp_drb = 0;
__uint32_t global_stop_on_err = 1;
__uint32_t PIXEL_TILE_SIZE;

RSS_DMA_Params	mgras_RssDmaParams;
HQ3_DMA_Params	mgras_Hq3DmaParams;

/* this is the data which will be dma'd to both rdram and tram */
__uint32_t DMA_WRLine[24] = { 	0x55555500, /* a0-pixel 0 */
				0x55555504, /* b0-pixel 0 */
				0xaaaaaa00, /* a0-pixel 1 */
				0xaaaaaa04, /* b0-pixel 1 */
			      	0xcccccc08, /* a1-pixel 0 */
				0xcccccc0c, /* b1-pixel 0 */
				0xffffff08, /* a1-pixel 1 */
				0xffffff0c, /* b1-pixel 1 */
			      	0x40414210, /* a2-pixel 0 */
				0x40414214, /* b2-pixel 0 */
				0x48494a10, /* a2-pixel 1 */
				0x48494a14, /* b2-pixel 1 */
			      0x55555500, 0x55555504, 0xaaaaaa00, 0xaaaaaa04,
			      0xcccccc08, 0xcccccc0c, 0xffffff08, 0xffffff0c,
			      0x40414210, 0x40414214, 0x48494a10, 0x48494a14
};

/* a DMA-DMA read of rdram or tram  will read back exactly the same as 
 * DMA_WRLine */

/* a DMA-PIO read of rdram will read data back in the following form when 
 * written by using DMA_WRLine[]
 */
__uint32_t DMA_RDRAM_PIORead_RDLine[24]= {
				0x00028a28, 0x00002aa8,/* a0-0: 0x55555500 */
				0x00005145, 0x00001545,/* a0-1: 0xaaaaaa00 */
				0x0002f02d, 0x00000cc0,/* a1-0: 0xcccccc08 */
				0x0002fb6d, 0x00003fed,/* a1-1: 0xffffff08 */
			      	0x00000428, 0x000000a1,/* a2-0: 0x40414210 */
				0x00005428, 0x000004a1,/* a2-1: 0x48494a10 */
				0x00028a28, 0x00002aa8,/* a0-0: 0x55555500 */
				0x00005145, 0x00001545,/* a0-1: 0xaaaaaa00 */
				0x0002f02d, 0x00000cc0,/* a1-0: 0xcccccc08 */
				0x0002fb6d, 0x00003fed,/* a1-1: 0xffffff08 */
			      	0x00000428, 0x000000a1,/* a2-0: 0x40414210 */
				0x00005428, 0x000004a1};/* a2-1:0x48494a10 */

ushort_t DMA_WRLine16[24] = {0x5500, 0x5504, 0xaa00, 0xaa04,
				0xcc00, 0xcc04, 0xff00, 0xff04,
				0x4243, 0x4647, 0x4a4b, 0x4e4f,
				0x5500, 0x5504, 0xaa00, 0xaa04,
				0xcc00, 0xcc04, 0xff00, 0xff04,
				0x4243, 0x4647, 0x4a4b, 0x4e4f};


__uint32_t DMA_RDData[1];
__uint32_t *DMA_WRTile_notaligned;
ushort_t DMA_WRTile16[256];
__uint32_t *DMA_RDTile_notaligned;
__uint32_t DMA_CompareTile_notaligned[1];
__uint32_t DMA_WRTile_ZERO_notaligned[1];
ushort_t DMA_WRTile16_ZERO[1];

__uint32_t *DMA_RDTile;
__uint32_t *DMA_WRTile;
__uint32_t *DMA_CompareTile;
__uint32_t *DMA_WRTile_ZERO;

__uint32_t dma_overwrite_flag = 0x1;
__uint32_t YMAX = 1024;
__uint32_t XMAX = 1344;

rdram_error_info	_mg_rdram_error_info[12];
__uint32_t		_mg_rdram_error_index = 0;

void
_mgras_disp_rdram_error(void)
{
    rdram_error_info	*p = &(_mg_rdram_error_info[0]);
    __uint32_t		i;

    if (!_mg_rdram_error_index) return;
    msg_printf(SUM, "FAILURE INFO:\n");
    for (i = 0; i < _mg_rdram_error_index; i++, p++) {
	msg_printf(SUM, "\
	RSS %d; \tPP %d; \tRDRAM %d; \t\tRDRAM_ADDR 0x%x;\n\
	X %d; \tY %d; \tEXP 0x%x; \tRCV 0x%x\n\n",
	p->rssnum, p->ppnum, p->rdnum, p->rd_addr, p->x, p->y, 
	(__scunsigned_t)(p->exp&0xffffffff),
	(__scunsigned_t)(p->rcv&0xffffffff));
    }
}

void
_mgras_store_rdram_error_info(__uint32_t rssnum, __uint32_t ppnum,
	__uint32_t rdnum, __uint32_t rd_addr, __uint32_t x, __uint32_t y,
	__uint32_t exp, __uint32_t rcv)
{
    rdram_error_info	*p, *tmp;
    __uint32_t		index = _mg_rdram_error_index;
    __uint32_t		i, store = 1;


    if (index) {
    	tmp = &(_mg_rdram_error_info[0]);
	for (i = 0; i < index; i++) {
	    if ( (tmp->rssnum == rssnum) &&
		 (tmp->ppnum  == ppnum)  &&
		 (tmp->rdnum  == rdnum) ) {
			store = 0;
			return;
	    }
	    tmp++;
	}
    }

    if (store) {
    	p = &(_mg_rdram_error_info[index]);
	p->rssnum = rssnum;
	p->ppnum = ppnum;
	p->rdnum = rdnum;
	p->rd_addr = rd_addr;
	p->x = x;
	p->y = y;
	p->exp = exp;
	p->rcv = rcv;
	_mg_rdram_error_index++;
    } 
}

__uint32_t
mgras_hq_redma_PP1(int argc, char **argv)
{
   __uint32_t rssnum, status, errIndex, errors=0, stop_on_err = 1;
   __uint32_t bad_arg = 0, col_cnt, row_cnt, sflag = 0;
   __uint32_t hqWriteFlag, reWriteFlag, hqReadFlag, reReadFlag;
   __uint32_t hqWriteMode, reWriteMode, hqReadMode, reReadMode;
   __uint32_t i, no_overwrite = 0, yend, xend, allpats = 0;
   char buf_str[16];
   __paddr_t	pgMask = ~0x0;

   pgMask = PAGE_MASK(pgMask);

   _mg_rdram_error_index = 0;
   for (i = 0; i < 12; i++) {
	_mg_rdram_error_info[i].rssnum = 0xff;
	_mg_rdram_error_info[i].ppnum = 0xff;
	_mg_rdram_error_info[i].rdnum = 0xff;
   }
   nocompare = dmawrite_only = dmaread_only = r_to_l = 0;
   dma_overwrite_flag = 1;
   set_dma = set_xstart = set_ystart = 0;
   pp_wr_flag = 0; 
   pp_dma_pat = PP_WALK_0_1_TEST; /* initialize */
   pp_dma_mode = PP_RGB12;
   global_stop_on_err = 1;
   YMAX = 1024; XMAX = 1344;

#if 0
        /* Enable broadcast writes to all PP1s */
        mgras_xmapSetAddr(mgbase, 0x0) ;
        mgras_xmapSetPP1Select(mgbase, 0x01000000);

        /* Select Second byte of the config reg :- for AutoInc */
        mgras_xmapSetAddr(mgbase, 0x1) ;  /* Do NOT REMOVE THIS */
        mgras_xmapSetConfig(mgbase, 0x0);

#endif

   hqWriteFlag = reWriteFlag = hqReadFlag = reReadFlag = 0;
   hqWriteMode = reWriteMode = hqReadMode = reReadMode = ~0x0;

   /* default */
   PIXEL_TILE_SIZE = 1344; /* for rdram */
   col_cnt = PIXEL_TILE_SIZE;
   row_cnt = 256;

   NUMBER_OF_RE4S();
   if (numRE4s == 0 || errors) {
	msg_printf(ERR, "RE4Count is %d, allowed values are 1-%d\n",
				numRE4s, MAXRSSNUM);
	REPORT_PASS_OR_FAIL((&RE_err[RE4_STATUS_REG_TEST]), errors);
   }

   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'e':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&hqWriteMode);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&hqWriteMode);
			}
			hqWriteFlag++;
			break;
		case 'w':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&reWriteMode);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&reWriteMode);
			}
			reWriteFlag++;
			break;
		case 'd':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&hqReadMode);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&hqReadMode);
			}
			hqReadFlag++;
			break;
		case 'r':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&reReadMode);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&reReadMode);
			}
			reReadFlag++;
			break;
		case 's':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&rssnum);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&rssnum);
			}
			sflag++;
			break;
		case 'x':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&stop_on_err);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&stop_on_err);
			}
			global_stop_on_err = stop_on_err;
			break;
		case 'y':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&row_cnt);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&row_cnt);
			}
			break;
		case 'z':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&col_cnt);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&col_cnt);
			}
			PIXEL_TILE_SIZE = col_cnt;
			break;
		case 'm':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&set_xstart);
				argc--; argv++;
				atob(&argv[1][0], (int*)&set_ystart);
				argc--; argv++;
			} else {
				msg_printf(SUM, "Need a space after -%c\n",
                                        argv[0][1]);
                                   bad_arg++;
			}
			set_dma = 1;
			break;
		case 'u':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&pp_wr_data);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&pp_wr_data);
			}
			pp_wr_flag = 1;
			break;
		case 'p':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&pp_dma_pat);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&pp_dma_pat);
			}
			break;
		case '8': pp_dma_mode = PP_RGBA8888; 
			break;
		case 'b':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&pp_drb);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&pp_drb);
			}
			break;
		case 'i': YMAX = 1488; XMAX = 2112;
			break;
		case 't': dmawrite_only = 1; break;
		case 'a': dmaread_only = 1; break;
		case 'c': nocompare = 1; break;
		case 'f': dma_overwrite_flag  = 0; break;
		default: bad_arg++; break;
	}
	argc--; argv++;
   }

   if ( bad_arg || argc ) {
	   PRINT_DMA_PP_USAGE();
	 return(0);
   }

	rssstart = 0; rssend = numRE4s;

	/* check if RSS is valid */
	if (re_checkargs(rssnum, 0, sflag, 0)) 
		return (0);
		
   msg_printf(DBG, "in top-level dma_mode = 0x%x\n", pp_dma_mode);

   switch (pp_drb) {
	case 0: strcpy(buf_str, "Z Buffer"); break;
	case 0x1c0: strcpy(buf_str, "Overlay Buffer"); break;
	case 0x240: strcpy(buf_str, "Color Buffer AB"); break;
	case 0x320: strcpy(buf_str, "Color Buffer CD"); break;
	case 0x0e0: strcpy(buf_str, "ACC36/AUX Buffer"); break;
	default: strcpy(buf_str, "Undefined"); break;
   }

   msg_printf(VRB, "%s DMA Test\n", buf_str);
   msg_printf(DBG, "(starting page address: 0x%x)\n",pp_drb);

   if (hqReadFlag || hqWriteFlag || reWriteFlag || reReadFlag) {
	msg_printf(DBG, "hqWriteFlag: %d, reWriteFlag: %d, hqReadFlag: %d, reReadFlag: %d\n", hqWriteFlag, reWriteFlag, hqReadFlag, reReadFlag);

	/* user specified modes */
	if (hqWriteMode == DMA_OP) {
	   if (reWriteMode == DMA_BURST) {
		if (hqReadMode == DMA_OP && reReadMode == DMA_BURST) {
		   errIndex = DMA_HQ_RE_PP1_DMA_BURST_TEST;
		} else if (hqReadMode == PIO_OP && reReadMode == DMA_PIO) {
		   errIndex = DMA_HQ_RE_PP1_PIO_PIO_TEST;
		} else if (hqReadMode == PIO_OP && reReadMode == INDIRECT_ADDR) {
		   errIndex = DMA_HQ_RE_PP1_PIO_IND_TEST;
		} else {
			errIndex = ERR_INDEX_ILLEGAL;
		}
	   } else {
		errIndex = ERR_INDEX_ILLEGAL;
	   }
	} else if (hqWriteMode == PIO_OP) {
	   if (hqReadMode == DMA_OP) {
		if (reWriteMode == DMA_BURST && reReadMode == DMA_BURST) {
		   errIndex = DMA_HQ_RE_PP1_BURST_BURST_TEST;
		} else if (reWriteMode == DMA_PIO && reReadMode == DMA_BURST) {
		   errIndex = DMA_HQ_RE_PP1_PIO_BURST_TEST;
		} else if (reWriteMode == INDIRECT_ADDR && reReadMode == DMA_BURST) {
		   errIndex = DMA_HQ_RE_PP1_IND_BURST_TEST;
		} else {
			errIndex = ERR_INDEX_ILLEGAL;
		}
	   } else if (hqReadMode == PIO_OP) {
		if (reReadMode == DMA_PIO && reWriteMode == DMA_BURST) {
            		errIndex = DMA_HQPIO_REDMA_PIO_BURST_TEST;
        	} else if (reReadMode==INDIRECT_ADDR && reWriteMode==DMA_BURST){
            		errIndex = DMA_HQPIO_REDMA_IND_BURST_TEST;
        	} else if (reReadMode == DMA_PIO && reWriteMode == DMA_PIO) {
        	    	errIndex = DMA_HQPIO_REDMA_PIO_PIO_TEST;
        	} else if (reReadMode==INDIRECT_ADDR && reWriteMode==DMA_PIO) {
            		errIndex = DMA_HQPIO_REDMA_IND_PIO_TEST;
        	} else if (reReadMode==DMA_PIO && reWriteMode==INDIRECT_ADDR) {
            		errIndex = DMA_HQPIO_REDMA_PIO_IND_TEST;
        	} else if (reReadMode==INDIRECT_ADDR && reWriteMode==INDIRECT_ADDR) {
            		errIndex = DMA_HQPIO_REDMA_IND_IND_TEST;
		}
	   } else {
		errIndex = ERR_INDEX_ILLEGAL;
	   }

	} else {
		errIndex = ERR_INDEX_ILLEGAL;
	}

	if (errIndex == ERR_INDEX_ILLEGAL) {
	   msg_printf(SUM, "Illegal read/write mode combination.\n");
	   PRINT_DMA_PP_USAGE();
	   return(0);
	} else { /* user specified mode */
	   if (pp_dma_pat== PP_ALL_PATTERNS)
		allpats = 1;
	   msg_printf(DBG, "3:pat: %d, allpats=%d\n", pp_dma_pat, allpats);
	   if ((hqWriteMode == PIO_OP) && (hqReadMode == PIO_OP)) {
		if ((pp_dma_pat == PP_PATTERN_TEST) || (pp_dma_pat== PP_ALL_PATTERNS)) {
		   if (allpats) {
			msg_printf(VRB, "Walking 0/1 pattern");
			pp_dma_pat = PP_WALK_0_1_TEST;
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);
		
			msg_printf(VRB, "Address-in-Address pattern");
			pp_dma_pat = PP_PAGE_ADDR_TEST;
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);
		
			msg_printf(VRB, "All F's/All 0's pattern");
			pp_dma_pat = PP_FF00_TEST;
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);
		   }
			/* run 6 patterns through memory */
			pp_dma_pat = PP_PATTERN_TEST_0;
			msg_printf(VRB, "Pattern pass 0");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_1;
			msg_printf(VRB, "Pattern pass 1");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_2;
			msg_printf(VRB, "Pattern pass 2");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_3;
			msg_printf(VRB, "Pattern pass 3");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_4;
			msg_printf(VRB, "Pattern pass 4");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_5;
			msg_printf(VRB, "Pattern pass 5");
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);
		}
		else { /* user chose 1 specific pattern */
			errors += _mgras_hqpio_redma(reReadMode, reWriteMode, 
				row_cnt, col_cnt, stop_on_err);
		}
	   }
	   else { /* has dma burst in read or write */
		if (set_dma) {
			xend = set_xstart + col_cnt;
			yend = set_ystart + row_cnt;
		}
		else {
			xend = XMAX; yend = YMAX;
		}
		
		if ((pp_dma_pat == PP_PATTERN_TEST) || (pp_dma_pat== PP_ALL_PATTERNS)) {
		   if (allpats) {
			msg_printf(VRB, "Walking 0/1 pattern");
			pp_dma_pat = PP_WALK_0_1_TEST;
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);
		
			msg_printf(VRB, "Address-in-Address pattern");
			pp_dma_pat = PP_PAGE_ADDR_TEST;
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);
		
			msg_printf(VRB, "All F's/All 0's pattern");
			pp_dma_pat = PP_FF00_TEST;
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);
		   }
			/* run 6 patterns through memory */
			pp_dma_pat = PP_PATTERN_TEST_0;
			msg_printf(VRB, "Pattern pass 0");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_1;
			msg_printf(VRB, "Pattern pass 1");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_2;
			msg_printf(VRB, "Pattern pass 2");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_3;
			msg_printf(VRB, "Pattern pass 3");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_4;
			msg_printf(VRB, "Pattern pass 4");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);

			pp_dma_pat = PP_PATTERN_TEST_5;
			msg_printf(VRB, "Pattern pass 5");
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);
	 	}
		else { /* user chose 1 specific pattern */
	   		errors += _mgras_hq_redma(DMA_PP1, hqWriteMode, reWriteMode, 
			   hqReadMode,reReadMode,PAT_NORM,row_cnt,col_cnt,
			   yend, xend, 0, 1, stop_on_err);
		}
	   }
	   msg_printf(DBG, "errIndex: %d, errors: %d\n",
		errIndex, errors);

	   /* display the rdram error info */
	   _mgras_disp_rdram_error();

	   REPORT_PASS_OR_FAIL((&DMA_err[errIndex]), errors);
	}
   } else {
	/* no mode is specified. Do all of them */
	errors += _mgras_hq_redma(DMA_PP1, DMA_OP, DMA_BURST, DMA_OP, DMA_BURST,
		PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 0, 1, 
		stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_DMA_BURST_TEST]), errors);
	}

	errors += _mgras_hq_redma(DMA_PP1, DMA_OP, DMA_BURST, PIO_OP, DMA_PIO,
		PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 0, 1, 
		stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_PIO_PIO_TEST]), errors);
	}

	errors += _mgras_hq_redma(DMA_PP1, DMA_OP, DMA_BURST, PIO_OP, 
		INDIRECT_ADDR, PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 
		0,1, stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_PIO_IND_TEST]), errors);
	}

	errors += _mgras_hq_redma(DMA_PP1, PIO_OP, DMA_BURST, DMA_OP, DMA_BURST
		, PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 0, 1, 
		stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_BURST_BURST_TEST]), errors);
	}

	errors += _mgras_hq_redma(DMA_PP1, PIO_OP, DMA_PIO, DMA_OP, DMA_BURST,
		PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 0, 1, 
		stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_PIO_BURST_TEST]), errors);
	}

	errors += _mgras_hq_redma(DMA_PP1, PIO_OP, INDIRECT_ADDR, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt, col_cnt, YMAX, XMAX, 0,
		 1, stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_IND_BURST_TEST]), errors);
	}

	errors += _mgras_hqpio_redma(DMA_PIO, DMA_BURST, row_cnt, 
		col_cnt, stop_on_err);
	if (errors) {
        REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQPIO_REDMA_PIO_BURST_TEST]), errors);
	}

        errors += _mgras_hqpio_redma(INDIRECT_ADDR, DMA_BURST, row_cnt, 
		col_cnt, stop_on_err);
	if (errors) {
        REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQPIO_REDMA_IND_BURST_TEST]), errors);
	}

        errors += _mgras_hqpio_redma(DMA_PIO, DMA_PIO, row_cnt, col_cnt,
		 stop_on_err);
	if (errors) {
        REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQPIO_REDMA_PIO_PIO_TEST]), errors);
	}

        errors += _mgras_hqpio_redma(INDIRECT_ADDR, DMA_PIO,row_cnt, 
		col_cnt, stop_on_err);
	if (errors) {
        REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQPIO_REDMA_IND_PIO_TEST]), errors);
	}

        errors += _mgras_hqpio_redma(DMA_PIO, INDIRECT_ADDR, row_cnt, 
		col_cnt, stop_on_err);
	if (errors) {
          REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQPIO_REDMA_PIO_IND_TEST]), errors);
	}
   }

	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_PP1_PIO_PIO_TEST]), errors);
}


#ifndef IP30
#ifdef _STANDALONE
#define GFX_HACK_NOT_FIXED
#endif

#ifdef GFX_HACK_NOT_FIXED
extern __int32_t	_mgras_run_ide;
#endif
#endif

__uint32_t
mgras_hq_redma_TRAM(int argc, char **argv)
{
   __uint32_t rssnum, status, errIndex, errors=0;
   __uint32_t bad_arg = 0, pg_end, row_cnt, col_cnt, sflag = 0;
   __uint32_t hqWriteFlag = 0, stop_on_err = 1;
   __uint32_t hqWriteMode = ~0x0, dmapio_read = 0;
   __uint32_t destDevice = DMA_TRAM;
   __uint32_t hqReadMode = ~0x0, reReadMode = ~0x0;
   __uint32_t allpats = 0;
   ULONG      fd;
   long       rc; 
   char *	console;

   r_to_l = set_xstart = tram_mem_pat = trampg_start = nocompare = dmawrite_only = dmaread_only = 0;
   tram_wr_flag = 0;
   global_stop_on_err = 1;
   num_tram_comps = 4; /* this test sends 4444 data */
   which_rss_for_tram_rd = 0; /* default */

   allpats = 0;

#ifdef GFX_HACK_NOT_FIXED
   console = getenv("console");
   if (console && ((strcmp(console, "g")==0) || (strcmp(console, "G")==0))) {
   	_mgras_run_ide = 0x1;
   	rc = Open("video()", 1, &fd);
   	if (rc) {
   		msg_printf(DBG, "Could not open video... rc = %d\n", rc);
   	} else {
		msg_printf(DBG, "Opening the video from console -d\n");
   	}
   	Close(fd);
   	_mgras_run_ide = 0x0;
   }
#endif

   NUMBER_OF_RE4S();
   if (numRE4s == 0 || errors) {
	msg_printf(ERR, "RE4Count is %d, allowed values are 1-%d\n",
				numRE4s, MAXRSSNUM);
	REPORT_PASS_OR_FAIL((&RE_err[RE4_STATUS_REG_TEST]), errors);
   }
   NUMBER_OF_TRAMS();
   if (numTRAMs == 0) {
	msg_printf(VRB, "No TRAMs detected. Skipping TRAM tests\n");
	return(0);
   }

   /* default */	
   pg_end = 256;
   if (numTRAMs == 1) {
	col_cnt = 64; row_cnt = 4096;
   }
   else  if (numTRAMs == 4) {
	if (_mgras_rtdma_enabled == 0) 
	  {
		col_cnt = 128; row_cnt = 4096;
	  }
	else {
		col_cnt = 64; row_cnt = 4096;
	}
   }

   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'e':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&hqWriteMode);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&hqWriteMode);
			}
			hqWriteFlag++;
			break;
		case 'r':
			destDevice = DMA_TRAM_REV;
			break;
		case 'n':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&rssnum);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&rssnum);
			}
   			which_rss_for_tram_rd = rssnum;
			sflag++;
			break;
		case 'x':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&stop_on_err);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&stop_on_err);
			}
			global_stop_on_err = stop_on_err;
			break;
		case 's':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&col_cnt);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&col_cnt);
			}
			break;
		case 'a':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&trampg_start);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&trampg_start);
			}
			break;
		case 'g':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&pg_end);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&pg_end);
			}
			break;
		case 't':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&row_cnt);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&row_cnt);
			}
			break;
		case 'm':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&tram_mem_pat);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&tram_mem_pat);
			}
			break;
		case 'y':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&set_xstart);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&set_xstart);
			}
			break;
		case 'f':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&tram_wr_data);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&tram_wr_data);
			}
			tram_wr_flag = 1;
			break;
		case 'p': dmapio_read = 1; break;
		case 'w': dmawrite_only = 1; break;
		case 'd': dmaread_only = 1; break;
		case 'c': nocompare = 1; break;
		case 'i': r_to_l = 1; break;
		default: bad_arg++; break;
	}
	argc--; argv++;
   }

   if ( bad_arg || argc ) {
	    	PRINT_DMA_TRAM_USAGE();
	 	return(0);
   }

	dma_overwrite_flag = 0;

	rssstart = 0; rssend = numRE4s;

	/* check if RSS is valid */
	if (re_checkargs(rssnum, 0, sflag, 0)) 
		return (0);
	
   if (destDevice == DMA_TRAM_REV)
	fpusetup();
	
   msg_printf(VRB, "\nRSS-%d, ", which_rss_for_tram_rd);
   if (numTRAMs == 1) 
	msg_printf(VRB, "Testing 1 TRAM.\n");
   else if (numTRAMs == 4)
	msg_printf(VRB, "Testing 4 TRAMs.\n");

   if (hqWriteFlag) {
	/* user specified modes */
	if (hqWriteMode == DMA_OP) {
	    errIndex = DMA_HQ_RE_TRAM_DMA_TEST;
	} else if (hqWriteMode == PIO_OP) {
	    errIndex = DMA_HQ_RE_TRAM_PIO_TEST;
	} else {
	    msg_printf(SUM, "Illegal read/write mode combination.\n");
	    PRINT_DMA_TRAM_USAGE();
#if 0
		 Usage: -e[HQ Write Mode]\n\
		 HQ_write\n\
		 --------\n\
		 DMA_OP (0)\n\
		 PIO_OP (1)\n");
#endif
	    return(0);
	}
	msg_printf(DBG, "destDevice: %d, hqWriteMode: %d, s: %d, t: %d\n",
			destDevice, hqWriteMode, col_cnt, row_cnt);

	if (dmapio_read) {
	   errIndex = DMA_HQ_RE_TRAM_BURSTWR_DMAPIORD_TEST;
	   hqReadMode = PIO_OP;
	   reReadMode = DMA_PIO;
	} else {
	   hqReadMode = DMA_OP;
	   reReadMode = DMA_BURST;
	}
	msg_printf(DBG, "pat: %d\n", tram_mem_pat);
	if (tram_mem_pat== TRAM_ALL_PATTERNS)
		allpats = 1;
	if ((tram_mem_pat == TRAM_PATTERN_TEST) || (tram_mem_pat== TRAM_ALL_PATTERNS)) {
	if (allpats) {
		msg_printf(VRB, "Walking 0/1 pattern");
		tram_mem_pat = TRAM_WALK_0_1_TEST;
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);
		
		msg_printf(VRB, "Address-in-Address pattern");
		tram_mem_pat = TRAM_PAGE_ADDR_TEST;
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);
		
		msg_printf(VRB, "All F's/All 0's pattern");
		tram_mem_pat = TRAM_FF00_TEST;
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);
	}
		/* run 6 patterns through memory */
		tram_mem_pat = TRAM_PATTERN_TEST_0;
		msg_printf(VRB, "Pattern pass 0");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_1;
		msg_printf(VRB, "Pattern pass 1");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_2;
		msg_printf(VRB, "Pattern pass 2");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_3;
		msg_printf(VRB, "Pattern pass 3");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_4;
		msg_printf(VRB, "Pattern pass 4");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_5;
		msg_printf(VRB, "Pattern pass 5");
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);
	}
	else {
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, hqReadMode,
		reReadMode, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, 
		stop_on_err);
	}
#if 0
	else { /* burst read */
	   errors += _mgras_hq_redma(destDevice, hqWriteMode, DMA_BURST, DMA_OP,
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 1, 0, 1,
	 	stop_on_err);
	}
#endif
	/* turn off for now */
	if (*Reportlevel >= VRB) {
	REPORT_PASS_OR_FAIL((&DMA_err[errIndex]), errors);
	}
   } else { /* user did not specify hqWriteFlag */
	if (tram_mem_pat == TRAM_PATTERN_TEST) {	
		tram_mem_pat = TRAM_PATTERN_TEST_0;
		msg_printf(VRB, "Pattern pass 0");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_1;
		msg_printf(VRB, "Pattern pass 1");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_2;
		msg_printf(VRB, "Pattern pass 2");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_3;
		msg_printf(VRB, "Pattern pass 3");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_4;
		msg_printf(VRB, "Pattern pass 4");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_5;
		msg_printf(VRB, "Pattern pass 5");
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);
	}
	else
	/* no mode is specified. Do all of them */
	errors += _mgras_hq_redma(destDevice, DMA_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM, row_cnt,col_cnt,1, pg_end, 0, 1, stop_on_err);
	if (errors) {
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_TRAM_DMA_TEST]), errors);
	}
	/* now do HQ-PIO, RE-BURST */
	if (tram_mem_pat == TRAM_PATTERN_TEST) {	

		tram_mem_pat = TRAM_PATTERN_TEST_0;
		msg_printf(VRB, "Pattern pass 0");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_1;
		msg_printf(VRB, "Pattern pass 1");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_2;
		msg_printf(VRB, "Pattern pass 2");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_3;
		msg_printf(VRB, "Pattern pass 3");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_4;
		msg_printf(VRB, "Pattern pass 4");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);

		tram_mem_pat = TRAM_PATTERN_TEST_5;
		msg_printf(VRB, "Pattern pass 5");
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);
	}
	else
	errors += _mgras_hq_redma(destDevice, PIO_OP, DMA_BURST, DMA_OP, 
		DMA_BURST, PAT_NORM,row_cnt,col_cnt, 1, pg_end, 0, 1, stop_on_err);
	REPORT_PASS_OR_FAIL((&DMA_err[DMA_HQ_RE_TRAM_PIO_TEST]), errors);


   }

   return errors;
}

extern __uint32_t _mgras_rttexture_setup(uchar_t in_format,uchar_t out_format, 
			__uint32_t d_len, __uint32_t *in_buf);

void mgras_rtdma_enable (  int argc, char **argv )
{
#if HQ4
    msg_printf(DBG, "Entering mgras_rtdma_enable...\n");

    /* get the args */

    if (argc < 2) {
        msg_printf(SUM, " mg_rtdma <0= disable, 1=enable> \n");
        return ;
    }
    atobu(argv[1], &_mgras_rtdma_enabled);

	if (_mgras_rtdma_enabled > 0) {
		msg_printf(SUM, " real-time dma enabled \n");
		_mgras_vsync = 0x3; /* turn on before the first data and turn-off after the
					the last data */
		_mgras_rtdma_enabled = 1; 

#if 0
			{
		        tram_cntrlu  tram_cntrl = {0};
        		tram_cntrl.bits.refresh = 8;        
        		tram_cntrl.bits.ref_count = 1;   

        		WRITE_RSS_REG(4, CONFIG, 4, ~0x0);
        		WRITE_RSS_REG(4, TRAM_CNTRL, tram_cntrl.val, ~0x0);
        		WRITE_RSS_REG(4, XYWIN, 0x0, 0xffffffff);
			}
#endif
		  }
	else {
		msg_printf(SUM, "real-time dma disabled \n");
		WRITE_RSS_REG(0, TL_VIDCNTRL, 0, ~0x0); 
		DELAY(5000);
		_mgras_vsync = 0;
		_mgras_rtdma_enabled = 0; 
	     }
#endif

}

/***********************************************************************/
/*
 * HQ3 <-> RE DMA test
 * Input:
 *	destDevice   = PP1 or TRAM or TRAM_REV
 *	hqWriteMode  = DMA_OP or PIO_OP
 *      reWriteMode  = DMA_BURST or DMA_PIO or INDIRECT_ADDR
 *	hqReadMode   = DMA_OP or PIO_OP
 *      reReadMode   = DMA_BURST or DMA_PIO or INDIRECT_ADDR
 */
__uint32_t
_mgras_hq_redma(__uint32_t destDevice, __uint32_t hqWriteMode, 
	__uint32_t reWriteMode, __uint32_t hqReadMode, __uint32_t reReadMode,
	__uint32_t pattern, __uint32_t row_cnt, __uint32_t col_cnt, 
	__uint32_t y_end, __uint32_t x_end, __uint32_t db_enable, 
	__uint32_t do_readback, __uint32_t stop_on_err)
{
   RSS_DMA_Params	*rssDmaPar = &mgras_RssDmaParams;
   HQ3_DMA_Params	*hq3DmaPar = &mgras_Hq3DmaParams;
   __uint32_t ystart, i, x, y, errors = 0, yend, xend;
   __uint32_t first_sp_width, numRows,numCols,pixComponents,pixComponentSize;
   __uint32_t xstart, *wrPageAligned, *rdPageAligned, *wrZeroPageAligned;
   __uint32_t rdram_dma_read_config;
   static char *led_on[] =  { NULL, "1" };
   static char *led_off[] = { NULL, "0" };
   __paddr_t	pgMask = ~0x0;

   pgMask = PAGE_MASK(pgMask);

   /* XXX: We need this for HQ PIO dma */
   FormatterMode(0x201);

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

   DMA_WRTile_ZERO = (__uint32_t *)DMA_WRTile_ZERO_notaligned;
   msg_printf(DBG, "DMA_WRTile_ZERO 0x%x\n", DMA_WRTile_ZERO);

   rdPageAligned = (__uint32_t *) 
	((((__paddr_t)DMA_RDTile_notaligned) + 0x1000) & pgMask);
   msg_printf(DBG, "rdPageAligned 0x%x\n", rdPageAligned);

   wrPageAligned = (__uint32_t *) 
	((((__paddr_t)DMA_WRTile_notaligned) + 0x1000) & pgMask);
   msg_printf(DBG, "wrPageAligned 0x%x\n", wrPageAligned);

#if 0
   wrZeroPageAligned = (__uint32_t *) 
	((((__paddr_t)DMA_WRTile_ZERO_notaligned) + 0x1000) & pgMask);
   msg_printf(DBG, "wrZeroPageAligned 0x%x\n", wrZeroPageAligned);
#endif


#if 0
   rssnum = CONFIG_RE4_ALL;
#endif

   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), CONFIG_RE4_ALL, TWELVEBIT_MASK);


   msg_printf(DBG, "Start _mgras_hq_redma");


   _mgras_hq_re_dma_init();

   GFX_HQHOSTCALLBACK();

   numRows = row_cnt;
   numCols = col_cnt;

   yend = y_end;
   xend = x_end;
   xstart = 0;
   ystart = 0;

   switch (destDevice) {
	case DMA_PP1: 
		if (pp_dma_mode == PP_RGBA8888) {
			first_sp_width = (numCols * 4); /* number of bytes */
			pixComponents = 4;
			pixComponentSize = 1;
		}
		else {
			first_sp_width = (numCols * 6); /* number of bytes */
			pixComponents = 3;
			pixComponentSize = 2;
		}
		xstart = set_xstart;
		ystart = set_ystart;
		break;
	case DMA_TRAM:
	if (_mgras_rtdma_enabled == 0) {
		if (numTRAMs == 1) 
			PIXEL_TILE_SIZE = 128; /* 2 dmas, each 128 pages */
		else if (numTRAMs == 4)
			PIXEL_TILE_SIZE = 64; /* 4 dmas, each 64 pages */
        	first_sp_width = (numCols * 2); /* # of bytes per row */
	}else {
		PIXEL_TILE_SIZE = 	64; /* 4 dmas, each 64 pages */
        	first_sp_width = (numCols * 4); /* # of bytes per row */
	}
		pixComponents = 4;
		xstart = trampg_start;
		if (numTRAMs == 1)
			pixComponentSize = 1;
		else if (numTRAMs == 4)
			pixComponentSize = 1;
		if (db_enable && (numTRAMs == 1)) {
			/* 1 component, 8-bit, L data for double buf, 1 TRAM */
			first_sp_width = numCols;
			pixComponents = 1;
		}
		break;
	case DMA_TRAM_REV: 
		PIXEL_TILE_SIZE = 1; /* 1 texel */
   		numRows	= 1;
   		numCols	= 1;
		first_sp_width = 8; /* read 64-bits of data */
		pixComponents = 4;
		pixComponentSize = 2; /* 12-bits per component */
		yend = 1; xend = 1;
		break;
   }

   msg_printf(DBG, "_mgras_hq_redma: destDevice 0x%x; first_sp_width 0x%x\n",
			destDevice, first_sp_width);

   rssDmaPar->numRows			= numRows;
   rssDmaPar->numCols			= numCols;
   rssDmaPar->pixComponents		= pixComponents;
   rssDmaPar->pixComponentSize		= pixComponentSize;
   rssDmaPar->x				= 0;
   rssDmaPar->y				= 0;
   rssDmaPar->beginSkipBytes		= 0;
   rssDmaPar->strideSkipBytes		= 0;
   rssDmaPar->xfrDevice			= destDevice;
   rssDmaPar->rss_clrmsk_msb_pp1	= ~0x0;
   rssDmaPar->rss_clrmsk_lsba_pp1	= ~0x0;
   rssDmaPar->rss_clrmsk_lsbb_pp1	= ~0x0;
   rssDmaPar->checksum_only 		= 0;

   hq3DmaPar->page_size			= page_4k;
   hq3DmaPar->physpages_per_impage 	= 16;
   hq3DmaPar->phys_base 		= 0xdeadbee0;
   hq3DmaPar->pool_id 			= DMA_GL_POOL;
   hq3DmaPar->IM_pg_width		= first_sp_width;
   hq3DmaPar->IM_row_offset		= 0;
   hq3DmaPar->IM_row_start_addr		= 0;
   hq3DmaPar->IM_line_cnt		= numRows;
   hq3DmaPar->IM_first_sp_width		= first_sp_width;
   hq3DmaPar->IM_DMA_ctrl		= 
	( (0x1  << 0) |        		/* Run flag */
          (DMA_GL_POOL  << 1) |         /* Pool - GL */
          (0x1  << 4) |        		/* DMA dest (0 -> cp) */
          (0x1  << 5) |        		/* Scan packed (1 -> yes) */
          (0x0  << 6) |        		/* Decimate (1 -> yes) */
          (0x0  << 7) );        	/* Start unconditionally */


#if 0
   /* for ide we have  a max dma byte size of 5120 words */
   if ((numRows * first_sp_width) > 20480) {
	msg_printf(SUM, "The region specified has > 20k bytes of data. The max allowable bytes of data is 16k. Skipping this test! \n");
	return (0);
   }
#endif

   for (i = 0; i <4096; i++) 
	*(rdPageAligned +i) = 0xdeadbeef;

   HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, i);
   i |= 0x7c00;
   HQ3_PIO_WR(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, i);

#if 0
   tram_cntrl.bits.refresh = 8;         /* 8us refresh */
   tram_cntrl.bits.ref_count = 1;       /* 1 page refreshed per refresh */
#endif

	msg_printf(DBG, "xstart: %d, xend:%d, ", xstart, xend);
	msg_printf(DBG, "ystart: %d, yend:%d, ", ystart, yend);

   rssDmaPar->x = 0; rssDmaPar->y = ystart;
   _mgras_setupTile(rssDmaPar, wrPageAligned, pattern, rdPageAligned);

   for (y = ystart; y < yend; y+=numRows) {

   	/* for multi-RSS frame buffer read, set the config register to 
   	 * CONFIG_RE4_ALL | ((last row #)-1) % numRE4s)
	 */
   	rdram_dma_read_config = CONFIG_RE4_ALL | (((y + numRows)-1) % numRE4s);
	msg_printf(DBG, "rdram_dma_read_config: %d, y: %d, numRows: %d\n", rdram_dma_read_config, y, numRows);

	if (destDevice == DMA_PP1)
		msg_printf(VRB, ".");

	setled(2, led_on);

	rssDmaPar->x = 0; rssDmaPar->y = y;


	for (x = xstart ; x < xend; x+= PIXEL_TILE_SIZE) {

		msg_printf(DBG, "x: %d\n", x);

		/* DMA a 1-high by PIXEL_TILE_SIZE-wide area */
		rssDmaPar->x = x;

		if ((destDevice == DMA_TRAM)  || (destDevice == DMA_TRAM_REV)) {
		/* For 1 tram, we want to do a 64x4096 texture which will
		 * cover 128 pages and then change the page location to 128
		 * and do the 2nd dma. For 4 trams, we want to do a  128x4096
		 * texture which will cover 64 pages so we will do this 4 times.
		 */
		   if (destDevice == DMA_TRAM) {
			msg_printf(VRB, ".");
			msg_printf(DBG, "Testing page #%d of %d\n", x,xend-1);
		   }

	errors += _mgras_DMA_TETRSetup(rssDmaPar,DMA_WRITE, db_enable);
	}


	if (dmaread_only == 0) {
	  if (_mgras_rtdma_enabled == 0) {/*do the graphics DMA Write*/
	     setled(2, led_off);
	     if ((destDevice == DMA_TRAM) || (destDevice == DMA_PP1)) {
		msg_printf(DBG, "DMA_WRITE.\n");
  	  	if (hqWriteMode == DMA_OP) {
	    	    FormatterMode((HQ_FC_CANONICAL & ~HQ_FC_INVALIDATE_TRAILING));
	    	    errors += _mgras_hqdma_redma_rw(rssDmaPar, hq3DmaPar, reWriteMode, 0x0, wrPageAligned, 0, db_enable);
		    FormatterMode(HQ_FC_CANONICAL);
		} else { /* hqWriteMode == PIO_OP */
		    errors += _mgras_hqpio_redma_write(rssDmaPar, reWriteMode, wrPageAligned, db_enable);
		 }
		}
		  if (errors) goto _error;
             } 
#if HQ4 
	    else if (_mgras_rtdma_enabled == 1) {  /*do the Real Time DMA */
			{
			__uint32_t bytesPerPixel, scanWidth;
    			WRITE_RSS_REG(0, TL_VIDCNTRL, 1, ~0x0);
    			DELAY(5000);

        		bytesPerPixel = 4; 
        		scanWidth = bytesPerPixel * rssDmaPar->numCols;
			msg_printf(DBG, " scanWidth 0x%x \n", scanWidth);
			
	if (_mgras_rttexture_setup(TEX_ABGR8_IN, TEX_RGBA8_OUT, (scanWidth*rssDmaPar->numRows), wrPageAligned) ) {
		_mgras_rtdma_enabled = 0;
		errors = 1; goto _error;
	}
			}
    		    } /* (_mgras_rtdma_enabled == 1) */ 
#endif
		} /* (dmaread_only == 0) */

		if (_mgras_rtdma_enabled == 1) {
	        WRITE_RSS_REG(0, TL_VIDCNTRL, 0, ~0x0);
       		 DELAY(5000);
		}


		if (dmawrite_only == 0 ) {
		     if (do_readback) {
		        setled(2, led_on);
		   	msg_printf(DBG, "DMA_READ BACK.\n");

			if ((destDevice == DMA_TRAM) || (destDevice == DMA_TRAM_REV)) {
			/* specify specific RSS for multi-RSS tram read */
			   if (numRE4s > 1) {
				msg_printf(DBG, "Setting config to %d\n",
					which_rss_for_tram_rd);
   				HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(CONFIG))),					 which_rss_for_tram_rd, TWELVEBIT_MASK);
			   }
			}
			else if (destDevice == DMA_PP1) {
				/* set config  for reads */
				HQ3_PIO_WR_RE((RSS_BASE+(HQ_RSS_SPACE(CONFIG))),
					rdram_dma_read_config, TWELVEBIT_MASK);
			}

		   	if (hqReadMode == DMA_OP) {
			    FormatterMode(0x201);
		            errors +=  _mgras_hqdma_redma_read_compare(rssDmaPar, 
			          	hq3DmaPar, reReadMode, rdPageAligned, db_enable);
		  	    FormatterMode(HQ_FC_CANONICAL);
		   	} else { /* hqReadMode == PIO_OP */
		            errors += _mgras_hqpio_redma_read_compare(rssDmaPar,
					reReadMode, DMA_RDTile, db_enable);
		   	}
		     } /* do_readback */
#if 0
			{
			__uint32_t bytesPerPixel, scanWidth;

        		bytesPerPixel = 4; 
        		scanWidth = bytesPerPixel * rssDmaPar->numCols;
	for ( i = 0 ; i < (rssDmaPar->numRows*rssDmaPar->numCols) ; i++)
	   msg_printf(SUM, " i %d exp 0x%x rcv 0x%x \n", i, *(wrPageAligned+i) , *(rdPageAligned+i) );
			}
#endif
		     if (errors && stop_on_err) goto _error;
		}


		if (dma_overwrite_flag) {
#if 0
		   /* write 0's in the FB region */
		   if (hqWriteMode == DMA_OP) {
   			FormatterMode( 0x201);
		    	errors += _mgras_hqdma_redma_rw(rssDmaPar, hq3DmaPar, 
				reWriteMode, 0x0, wrZeroPageAligned, 
				0, db_enable);
		  	FormatterMode(HQ_FC_CANONICAL);
		   } else { /* hqWriteMode == PIO_OP */
		    	errors += _mgras_hqpio_redma_write(rssDmaPar, reWriteMode, db_enable);
		   }
		   if (errors) goto _error;
#endif
		}

		/* reset to broadcast to all RSs */
   		HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),CONFIG_RE4_ALL,TWELVEBIT_MASK);

        } /* for x */
   } /* for y */
   msg_printf(VRB, "\n");

#if 0
	/* reset ranges to test all RSS's together if user has not specified
           a particular RSS to test */
        if ((sflag==0) && (rssnum == (rssend-1)) && (rssnum) && !(rssnum== 4)) {                rssnum = 3; rssend = 5;
        }

   } /* rssnum loop */
#endif

_error:

   _mgras_hq_re_dma_end();

   msg_printf(DBG, "End _mgras_hq_redma\n");


   return (errors);
}

__uint32_t
_mgras_hqdma_redma_rw(RSS_DMA_Params *rssDmaPar, HQ3_DMA_Params *hq3DmaPar, 
	__uint32_t reWriteMode, __uint32_t reReadMode, __uint32_t *data, 
	__uint32_t rwFlag, __uint32_t db_enable)
{
   IL_SIM_MEMORY_DECL()
   __uint32_t dmaCtrl;
   __uint32_t errors = 0;

   msg_printf(DBG, "Begin _mgras_hqdma_redma_rw");

   if (rwFlag) { /* read mode */
   	msg_printf(DBG, "Begin accessing memory");

   	/* Set up the Simulated Memory */
   	IL_SETUP_SIM_MEMORY(hq3DmaPar, rssDmaPar);

   	msg_printf(DBG, "End  accessing memory");

	/* Setup HAG to accept the data */
   	dmaCtrl = hq3DmaPar->IM_DMA_ctrl | (0x1 << 3); /* Set the direction */
	errors += _mgras_dma_hag_setup(hq3DmaPar, dmaCtrl, data);

	if ((rssDmaPar->xfrDevice == DMA_TRAM) ||
		(rssDmaPar->xfrDevice == DMA_TRAM_REV))
		errors += _mgras_DMA_TexReadSetup(rssDmaPar, reReadMode, DMA_READ, 
			1, 0, db_enable);
	else
	/* Trigger the DMA from RSS */
	_mgras_dma_re_setup(rssDmaPar, reReadMode, DMA_READ, db_enable);

	/* Send synchronization token and wait */
	errors += _mgras_hq_re_dma_WaitForDone(DMA_READ, reReadMode);

	/* Read data from the simulated memory */
	IL_READ_DATA_FROM_SIM_MEMORY();
   } else { /* write mode */

   	msg_printf(DBG, "Begin accessing memory");

   	/* Set up the Simulated Memory */
   	IL_SETUP_SIM_MEMORY(hq3DmaPar, rssDmaPar);

   	msg_printf(DBG, "End  accessing memory");

   	/* Write data into the simulated memory */
   	IL_WRITE_DATA_TO_SIM_MEMORY(hq3DmaPar, rssDmaPar, data);

#if DMA_DEBUG
	fprintf(stderr, "rssDmaPar->numRows: %d, Cols: %d\n",
		rssDmaPar->numRows, rssDmaPar->numCols);
#endif

	/* Setup the RSS to accept the data */
   	_mgras_dma_re_setup(rssDmaPar, reWriteMode, DMA_WRITE, db_enable);


	/* Trigger the DMA from HQ */
   	dmaCtrl = hq3DmaPar->IM_DMA_ctrl;
	errors += _mgras_dma_hag_setup(hq3DmaPar, dmaCtrl, data);


	/* Send synchronization token and wait */
	errors += _mgras_hq_re_dma_WaitForDone(DMA_WRITE, reWriteMode);
   }

   /* Destroy the pool */
   IL_DESTROY_SIM_MEMORY();

   msg_printf(DBG, "End   _mgras_hqdma_redma_rw");

   return(errors);
}


__uint32_t
_mgras_dma_hag_setup(HQ3_DMA_Params *hq3DmaPar, __uint32_t dmaCtrl, __uint32_t *data)
{
   __uint32_t cmd;
   __uint32_t pages, i, _14bitbuf;
   __uint32_t remainBits, data_lo, data_hi;
   __uint32_t data_hi_tbl_base, gtlbMode;
   __uint64_t mask64 = ~0x0, phys, physPage;
   __uint32_t gio_config, cfifo_status, errors=0;
   __uint32_t actual = 0xdeadbeef;
   unsigned int bits;


#if 0
   /* Setup the translation table */
   trans_table[0] = (K1_TO_PHYS(data)) >> 12; /* XXX page size = 2^12 */
   trans_table[1] = (K1_TO_PHYS(data+1024)) >> 12; /* XXX page size = 2^12 */
   trans_table[2] = (K1_TO_PHYS(data+2048)) >> 12; /* XXX page size = 2^12 */
   trans_table[3] = (K1_TO_PHYS(data+3072)) >> 12; /* XXX page size = 2^12 */
   trans_table[4] = (K1_TO_PHYS(data+4096)) >> 12; /* XXX page size = 2^12 */
   trans_table[5] = (K1_TO_PHYS(data+5120)) >> 12; /* XXX page size = 2^12 */
#endif

   pages = 505;
 
#if !HQ4
   bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) <<
			mgras_baseindex(mgbase);

   if ((_gio64_arb & bits) != bits) {
       msg_printf(ERR,"GIO64_ARB not set correctly!\n");
       return 1;
   }

   pages = 505;
   if ((_gio64_arb & bits) != bits) {
       msg_printf(ERR,"GIO64_ARB not set correctly!\n");
       return 1;
   }
   for (i = 0; i < pages; i++) {
   	trans_table[i] = (K1_TO_PHYS(data+(i*1024))) >> 12; 
   }
#else
   pages = 505;
   for (i = 0; i < pages; i++) {
       trans_table[i] = (K1_TO_PHYS(data+(i*1024))) >> 12;
   }
   /* store the 14 bits graphics DMA mode B register */
   phys = K1_TO_PHYS(data);
   physPage = phys >> 12;
   remainBits = HQ4_DMAB_ADDX(physPage >> 22);
   HQ3_PIO_WR(GRAPHICS_DMA_MODE_B, remainBits, ~0x0);
   HQ3_PIO_WR(GRAPHICS_DMA_MODE_A, (HEART_ID << 16), ~0x0);
#endif /* !HQ4 */
	

   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
   if (cfifo_status == CFIFO_TIME_OUT) {
	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
	return (-1);
   }

   msg_printf(DBG, "Address of trans_table is 0x%x\n", &(trans_table[0]));
   msg_printf(DBG, "trans_table[0] is 0x%x\n", (trans_table[0]));
   msg_printf(DBG, "trans_table[1] is 0x%x\n", (trans_table[1]));
   msg_printf(DBG, "trans_table[2] is 0x%x\n", (trans_table[2]));
   msg_printf(DBG, "trans_table[3] is 0x%x\n", (trans_table[3]));
   msg_printf(DBG, "trans_table[4] is 0x%x\n", (trans_table[4]));
   msg_printf(DBG, "trans_table[5] is 0x%x\n", (trans_table[5]));

   HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, ~0x0, ~0x0);

   /* Invalidate TLB entries */
   HQ3_PIO_WR(TLB_VALIDS_ADDR, 0x0, ~0x0);

   /* Tell the Gfx to handle TLB misses */
   HQ3_PIO_RD(GIO_CONFIG_ADDR, ~0x0, gio_config);
   gio_config |= HQ_GIOCFG_HQ_UPDATES_TLB_BIT;
   HQ3_PIO_WR(GIO_CONFIG_ADDR, gio_config, ~0x0);

#if 0
   /* Clear the hag state flags */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + HAG_STATE_FLAGS), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
#endif

#if HQ4
   /* Set up the table base address */
   phys = K1_TO_PHYS(&(trans_table[0]));
   data_lo = (__uint32_t)(phys); /* the lower 32 bits */
   data_hi = (__uint32_t)((__uint64_t)phys >> 32);
   data_hi_tbl_base = data_hi & 0xff; /* the upper 8 bits */

   msg_printf(DBG, "Setting up the table base and tlb mode registers...\n");
   msg_printf(DBG, "phys 0x%x; data_lo 0x%x; data_hi_tbl_base 0x%x\n",
	phys, data_lo, data_hi_tbl_base);
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + GL_TABLE_BASE_ADDR), 8);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR64(CFIFO1_ADDR, data_hi_tbl_base, data_lo, mask64);

   /* store the remaining 8 bits and the compat mode in graphics TLB mode */
   gtlbMode = ((data_hi >> 8) & 0xff) << 24;
   gtlbMode |= (1 << 22);
   gtlbMode |= (HEART_ID << 16);
   msg_printf(DBG, "data_hi 0x%x; gtlbMode 0x%x\n", data_hi, gtlbMode);
   HQ3_PIO_WR(GRAPHICS_TLB_MODE, gtlbMode, ~0x0);
#else
   /* Set up the table base address */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + GL_TABLE_BASE_ADDR), 8);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, (K1_TO_PHYS(&(trans_table[0]))), ~0x0);
#endif /* HQ4 */
   
   /* Set up the page size */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DMA_PG_SIZE), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);
   
   /* Set up the page range */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + DMA_PG_NO_RANGE_GL), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x2, ~0x0);
   
   /* Set up the page range */
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + MAX_GL_TBL_PTR_OFFSET), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, pages, ~0x0);
   
   /* IDE might keep the data in caches, HQ can not DMA from caches */
   flush_cache();

   msg_printf(DBG, "Start _mgras_dma_hag_setup");

msg_printf(DBG,
        " in dma: hq3DmaPar->IM_pg_width: 0x%x, hq3DmaPar->IM_line_cnt: 0x%x, addr: 0x%x\n",
                hq3DmaPar->IM_pg_width, hq3DmaPar->IM_line_cnt, hq3DmaPar);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_PG_LIST1), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x80000000, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_PG_WIDTH), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_pg_width, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_ROW_OFFSET), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_row_offset, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_ROW_START_ADDR), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_row_start_addr, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_LINE_CNT), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_line_cnt, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_FIRST_SP_WIDTH), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_first_sp_width, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_LAST_SP_WIDTH), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, hq3DmaPar->IM_first_sp_width, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_Y_DEC_FACTOR), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + HAG_STATE_REMAINDER), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, 0x0, ~0x0);

   /* IDE might keep the data in caches, HQ can not DMA from caches */
   flush_cache();

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (HAG_CTX_CMD_BASE + IM_DMA_CNTL_STAT), 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, dmaCtrl, ~0x0);

   /* IDE might keep the data in caches, HQ can not DMA from caches */
   flush_cache();


	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);    
       msg_printf(DBG, "HQ3 Flag set reg: 0x%x\n", actual);

   msg_printf(DBG, "End _mgras_dma_hag_setup");
   
   return (errors);
}

__uint32_t
_mgras_hqdma_redma_read_compare(
	RSS_DMA_Params *rssDmaPar, 
	HQ3_DMA_Params *hq3DmaPar, 
	__uint32_t reReadMode, 
	__uint32_t *data, 
	__uint32_t db_enable)
{
   __uint32_t errors = 0;

   msg_printf(DBG, "Begin _mgras_hqdma_redma_read_compare\n");


   /* Read the data */
   errors += _mgras_hqdma_redma_rw(rssDmaPar, hq3DmaPar, ~0x0, reReadMode, data, 1, db_enable);
   if (!errors) {
   	/* Compare the data */
	if (nocompare == 0) {
	   if (rssDmaPar->xfrDevice == DMA_TRAM)
   	   errors += _mgras_hqpio_redma_compare(rssDmaPar,DMA_WRTile,data,db_enable);
	   else
   	   errors += _mgras_hqpio_redma_compare(rssDmaPar,DMA_WRTile,data,db_enable);
	}
   }

   msg_printf(DBG, "End   _mgras_hqdma_redma_read_compare\n");

   return(errors);
}

/*
 * Setup the texture system for a DMA read - taken from gfx/lib/MG0/mg0_pixel.c
 */
__uint32_t
_mgras_DMA_TexReadSetup(
	RSS_DMA_Params *rssDmaPar, 
	__uint32_t dmakind, 
	__uint32_t readOrWrite, 
	__uint32_t do_it, 
	__uint32_t fb_linelength,
	__uint32_t db_enable)
{
	__uint32_t cmd, xend, yend, maxs, maxt, texmode2_bits, cfifo_status;
	double scale;
	long logssize, logtsize, fs, ft;
	float tmp0, tmp1;
	__int64_t longnum;
	__uint32_t ph, ymax_val;
	iru          ir = {0};
	txscaleu        txscale = {0};
	xfrmodeu     xfrMode = {0};
	xfrcontrolu  xfrCtrl = {0};
	texmode1u	texmode1 = {0};
	__uint32_t	bytesPerPixel, scanWidth;

	ph = 1;

	xend = rssDmaPar->numCols - 1;
	yend = rssDmaPar->numRows - 1;
	
	maxs = xend;
	maxt = yend;
	logssize = 1; while(maxs >>= 1) logssize++;
	logtsize = 1; while(maxt >>= 1) logtsize++;

	fs = 19 - logssize;
    	ft = 19 - logtsize;

    	if (fs > 7) fs = 7;
    	else if (fs < 0) fs = 0;
    	if (ft > 7) ft = 7;
    	else if (ft < 0) ft = 0;

	/* Add this magic number so that all exponents will have the same
 	 * exponent and a different mantissa during the conversion from
	 * float to hex. Then we can ignore the exponent bits.
	 */
#if 0
	if (ph)
	magic_num = 45056.5; 
	else
	magic_num = 49151.5; 
#endif

	msg_printf(DBG, "Begin _mgras_DMA_TexReadSetup \n");

#if HQ4
	if (_mgras_rtdma_enabled == 1 )
	bytesPerPixel = 4;
	else
	bytesPerPixel = 2; /* TRAM is always 2 bytes per pixel 4-4-4-4 */

	scanWidth = bytesPerPixel * rssDmaPar->numCols;

	msg_printf(DBG, "_mgras_DMA_TexReadSetup: scanWidth 0x%x\n",
		scanWidth);

	cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, REIF_SCAN_WIDTH, 4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: scanWidth 0x%x\n",
		scanWidth);
#endif

	/* hq reif setup: 0x2 = dest is host */
   	cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, REIF_DMA_TYPE, 4);
   	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   	HQ3_PIO_WR(CFIFO1_ADDR, 0x2, ~0x0); 

#if 0	
	magic_num2 = 262143.0; /* 2^18 -1, scaling factor */
#endif
   	xfrCtrl.bits.XFR_DMAStart = xfrCtrl.bits.XFR_PIOStart = 0;
   	xfrCtrl.bits.XFR_Device=((rssDmaPar->xfrDevice == DMA_PP1) ? 0x0 : 0x1);
   	xfrCtrl.bits.XFR_Direction = ((readOrWrite == DMA_WRITE) ? 0x0 : 0x1);

        cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (0x1800 + XFRCONTROL), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, xfrCtrl.val, ~0x0);

	if (_mgras_DMA_TETRSetup(rssDmaPar, DMA_READ, db_enable) != 0)
		return -1;

    	WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
    	}

	/* this is for textured drawing. the only diff from tetr setup is 
	 * TexEnvMode is now MODULATE  */
	if (do_it == 0) { 
		texmode1.bits.TexEn = 1;             /* enable */
   		texmode1.bits.TexEnvMode = 0;        /* MODULATE */
		if (rssDmaPar->xfrDevice == DMA_TRAM_REV)
   		   texmode1.bits.NumTram = 2;  /* 4 */
		else
   		   texmode1.bits.NumTram =numTRAMs/2; /* from NUMBER_OF_TRAMS */
   		texmode1.bits.TexReadSelect = 0;     /* location 0 */
   		texmode1.bits.TexLUTmode = 4;        /* RGBA -> RGBA */
   		texmode1.bits.TexLUTbypass = 0;      /* bypass alpha LUT */
   		texmode1.bits.BlendUnitCount = 4;    /* 5 pairs */
   		texmode1.bits.IAMode = 0;            /* not enabled */
		texmode1.bits.TexComp = TEXMODE_4COMP;

		cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (0x1800 + TEXMODE1), 4);
   		HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   		HQ3_PIO_WR(CFIFO1_ADDR, texmode1.val, ~0x0);
	}

	/* First setup resample area in TE and begin iteration */
	
	switch (numRE4s) {
		case 1: ymax_val = yend; break;
		case 2: ymax_val = 2*yend; msg_printf(DBG, "2 RSS tex\n"); break;
	}

	if ((numRE4s == 2)  && (which_rss_for_tram_rd == 1)) { /* add 1 to ymax, ymin, ymid */
		__glMgrRE4Position_diag(TRI_YMID, 49153.0, 49153.0, 1);
		__glMgrRE4Position_diag(TRI_X2, ((float)xend) + 49153.5,
		(ymax_val+49154.0),1);
	} else {
		__glMgrRE4Position_diag(TRI_YMID, 49152.0, 49152.0, 1);
		__glMgrRE4Position_diag(TRI_X2, ((float)xend) + 49153.5,
		(ymax_val+49153.0),1);
	}

    	__glMgrRE4Position_diag(TRI_X0, 49152.5, ((float)xend) + 49153.5,1); 
    	__glMgrRE4TriSlope_diag(TRI_DXDY0_HI, 0);
    	__glMgrRE4TriSlope_diag(TRI_DXDY1_HI, 0);

	texmode2_bits = 0; /* warp enable not on */
    	__glMgrRE4Texture_diag(texmode2_bits, DWIY_U | 0x200, 0.0);
    	__glMgrRE4Texture_diag(texmode2_bits, DWIX_U | 0x200, 0.0);
	if (do_it) {
	   	if (numRE4s == 2) { /* halve the T increment */
			msg_printf(DBG, "2 RSS tex inc\n");
    			__glMgrRE4Texture_diag(texmode2_bits,DTWE_U | 0x200,(double)16777152.0);
   			__glMgrRE4Texture_diag(texmode2_bits,DTWY_U | 0x200,(double)16777152.0);
	   	}
	   	else {
    			__glMgrRE4Texture_diag(texmode2_bits,DTWE_U | 0x200,(double)33554304.0);
   			__glMgrRE4Texture_diag(texmode2_bits,DTWY_U | 0x200,(double)33554304.0);
	   	}
    		__glMgrRE4Texture_diag(texmode2_bits, DSWY_U | 0x200, 0.0);
    		__glMgrRE4Texture_diag(texmode2_bits,DSWX_U | 0x200,(double)33554304.0);
    		__glMgrRE4Texture_diag(texmode2_bits, DTWX_U | 0x200, 0.0);
    		__glMgrRE4Texture_diag(texmode2_bits, DSWE_U | 0x200, 0.0);
	}
	else {
		switch (fb_linelength) {
			case 32: scale = 2.0; break;
			case 64: scale = 1.0; break;
			case 128: scale = 0.5; break;
		}
				
    		__glMgrRE4Texture_diag(texmode2_bits,DSWE_U | 0x200,
			(double)33554304.0 * scale);
   		__glMgrRE4Texture_diag(texmode2_bits, DTWE_U | 0x200, 0.0);
    		__glMgrRE4Texture_diag(texmode2_bits,DSWX_U | 0x200,
			(double)33554304.0 * scale);
    		__glMgrRE4Texture_diag(texmode2_bits, DTWX_U | 0x200, 
			(double)33554304.0 * scale);
    		__glMgrRE4Texture_diag(texmode2_bits, DSWY_U | 0x200, 
			(double)33554304.0 * scale);
    		__glMgrRE4Texture_diag(texmode2_bits,DTWY_U | 0x200,
			(double)33554304.0 * scale);
	}
    	__glMgrRE4Texture_diag(texmode2_bits, DWIE_U | 0x200, 0.0);
    	__glMgrRE4Texture_diag(texmode2_bits, SW_U | 0x200, 
		(double)(0.1) * 33554304.0);
    	__glMgrRE4Texture_diag(texmode2_bits, TW_U | 0x200,
		(double)(0.0) * 33554304.0);
    	__glMgrRE4Texture_diag(texmode2_bits, WI_U | 0x200, 262143.0);

    	WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    	if (cfifo_status == CFIFO_TIME_OUT) {
		msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
		return (-1);
    	}

	if (ph) {
	txscale.bits.fs = fs;
        txscale.bits.ft = ft;
	}
	else {
	txscale.bits.fs = 0x7;
	txscale.bits.ft = 0x7;
	}

	if (do_it == 0) {
        	cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, (0x1800 + TXSCALE), 4);
        	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        	HQ3_PIO_WR(CFIFO1_ADDR, txscale.val, ~0x0);
	}	
	else if (do_it) {
	ir.bits.Setup = 0x0;
	if (rssDmaPar->xfrDevice == DMA_TRAM_REV)
		ir.bits.Opcode = IR_OP_GL_POINT;
	else
		ir.bits.Opcode = IR_OP_AREA_LTOR;

        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + XFRSIZE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, ((rssDmaPar->numRows << 16)+rssDmaPar->numCols),
		 ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: XFRSIZE 0x%x\n",
		((rssDmaPar->numRows << 16)+rssDmaPar->numCols));
	
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + XFRCOUNTERS), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, ((rssDmaPar->numRows << 16)+rssDmaPar->numCols),
		 ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: XFRSIZE 0x%x\n",
		((rssDmaPar->numRows << 16)+rssDmaPar->numCols));
	
   	xfrMode.bits.StrideSkipBytes = rssDmaPar->strideSkipBytes;
   	xfrMode.bits.BeginSkipBytes = rssDmaPar->beginSkipBytes;
   	xfrMode.bits.RGBorder = 0; /* ABGR */
	if (db_enable && (numTRAMs == 1))
   		xfrMode.bits.PixelFormat = XFRMODE_L1;
	else
   		xfrMode.bits.PixelFormat = XFRMODE_RGBA4;

	/* default PixelCompType = 0 -- 1byte, unsigned */
   	if (rssDmaPar->xfrDevice == DMA_TRAM) {
	   if (db_enable && (numTRAMs == 1))
        	xfrMode.bits.PixelCompType = XFRMODE_1U; /* 4,4 */
	   else {
		if (_mgras_rtdma_enabled == 0)	
        	xfrMode.bits.PixelCompType = XFRMODE_4444U; /* 4,4,4,4 */
		else
		xfrMode.bits.PixelCompType = XFRMODE_1U ;
		}
	}
	else if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
	   if (numTRAMs == 1)	{
        	xfrMode.bits.PixelCompType =  XFRMODE_2U; /* 2bytes,unsign */
   		xfrMode.bits.PixelFormat = XFRMODE_RGBA4;
	   }
	   else if (numTRAMs == 4) {
        	xfrMode.bits.PixelCompType =  XFRMODE_2U; /* 2bytes,unsign */
   		xfrMode.bits.PixelFormat = XFRMODE_RGBA4;
	   }
	}
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + XFRMODE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, xfrMode.val, ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: XFRMODE 0x%x\n",
		xfrMode.val);
	
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + XFRSIZE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, ((rssDmaPar->numRows << 16)+rssDmaPar->numCols),
		 ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: XFRSIZE 0x%x\n",
		((rssDmaPar->numRows << 16)+rssDmaPar->numCols));

   	xfrCtrl.bits.XFR_DMAStart = ((dmakind == DMA_BURST) ? 0x1 : 0x0);
   	xfrCtrl.bits.XFR_PIOStart = ((dmakind == DMA_BURST) ? 0x0 : 0x1);
   	xfrCtrl.bits.XFR_Device=((rssDmaPar->xfrDevice == DMA_PP1) ? 0x0 : 0x1);
   	xfrCtrl.bits.XFR_Direction = ((readOrWrite == DMA_WRITE) ? 0x0 : 0x1);

	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + IR), 4);
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
	HQ3_PIO_WR(CFIFO1_ADDR, ir.val, ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: IR 0x%x\n",
		ir.val);

	/* Actually start resampling in the TE/TRAM, but the RE does not do
	 * anything yet
 	 */

        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1c00 + TXSCALE), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, txscale.val, ~0x0);
	
        cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + XFRCONTROL), 4);
        HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
        HQ3_PIO_WR(CFIFO1_ADDR, xfrCtrl.val, ~0x0);
	msg_printf(DBG, "_mgras_DMA_TexReadSetup: XFRCONTROL 0x%x\n",
		xfrCtrl.val);


	} /* do_it */

	msg_printf(DBG, "END _mgras_DMA_TexReadSetup \n");
	return 0;
}
