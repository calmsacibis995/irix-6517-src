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

/*
 *  DMA Diagnostics.
 *  	o Host <-> RE transfer test
 *  	o Host <-> GE transfer test
 *  	o RE <-> RE transfer test
 *
 *  $Revision: 1.120 $
 */

#include "sys/types.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "mgras_diag.h"
#include "ide_msg.h"
#include "mgras_dma.h"

extern __uint32_t which_rss_for_tram_rd;

#define DMA_DEBUG		0
#define DMA_DEBUG2		0

extern __uint32_t _mgras_rtdma_enabled;
extern uchar_t    _mgras_vsync ;

__uint64_t read_checksum;
__uint64_t read_checksum_even;
__uint64_t read_checksum_odd ;

/* walk 1, 0 on 16 bits */
static __uint32_t tram_patrn[] = {
    0x00010002, 0x00040008, 0x00100020, 0x00400080,
    0x01000200, 0x04000800, 0x10002000, 0x40008000,
    0xfffefffd, 0xfffbfff7, 0xffefffdf, 0xffbfff7f,
    0xfefffdff, 0xfbfff7ff, 0xefffdfff, 0xbfff7fff
};


#define NO_LONGER_USED 1

__uint32_t
_mgras_hqpio_redma(__uint32_t reReadMode, __uint32_t reWriteMode, __uint32_t row_cnt, __uint32_t col_cnt, __uint32_t stop_on_err)
{
   RSS_DMA_Params	*rssDmaPar = &mgras_RssDmaParams;
   __uint32_t xstart, xend, yend,ystart, x, y, errors = 0;
   __paddr_t		pgMask = ~0x0;

   FormatterMode(0x201);

   pgMask = PAGE_MASK(pgMask);

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

   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),4, TWELVEBIT_MASK);

   msg_printf(DBG, "Start _mgras_hqpio_redma\n");
   switch (reReadMode) {
     case DMA_PIO:
		msg_printf(DBG, "RE Read Mode is DMA_PIO\n");
		break;
     case INDIRECT_ADDR:
		msg_printf(DBG, "RE Read Mode is INDIRECT_ADDR\n");
		break;
   }

   switch (reWriteMode) {
     case DMA_BURST:
		msg_printf(DBG, "RE Write Mode is DMA_BURST\n");
		break;
     case DMA_PIO:
		msg_printf(DBG, "RE Write Mode is DMA_PIO\n");
		break;
     case INDIRECT_ADDR:
		msg_printf(DBG, "RE Write Mode is INDIRECT_ADDR\n");
		break;
   }

   ystart  =  set_ystart;
   xstart  =  set_xstart;
   xend = XMAX;
   yend = YMAX;
   if (set_dma) { /* user has set dma size */
	xend = set_xstart + col_cnt;
	yend = set_ystart + row_cnt;
   }

   _mgras_hq_re_dma_init();

	msg_printf(DBG, "in _mgras_hqpio_redma pp_dma_mode: %x\n", pp_dma_mode);

      rssDmaPar->numRows		= row_cnt;
      rssDmaPar->numCols		= col_cnt;
      if (pp_dma_mode == PP_RGBA8888) {
      		rssDmaPar->pixComponents	= 4;
      		rssDmaPar->pixComponentSize	= 1;
      }
      else {
      		rssDmaPar->pixComponents	= 3;
      		rssDmaPar->pixComponentSize	= 2;
      }
      rssDmaPar->beginSkipBytes		= 0;
      rssDmaPar->strideSkipBytes	= 0;
      rssDmaPar->xfrDevice		= DMA_PP1;
      rssDmaPar->rss_clrmsk_msb_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsba_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsbb_pp1	= ~0x0;

	msg_printf(DBG, "xstart: %d, xend %d, ystart: %d, yend: %d\n",
		xstart, xend, ystart, yend);

      for (y = ystart; y < yend; y+= row_cnt) {

	msg_printf(VRB, ".");

	rssDmaPar->x = 0; rssDmaPar->y = y;

	_mgras_setupTile(rssDmaPar, DMA_WRTile, PAT_NORM, DMA_RDTile);

	for (x = xstart; x < xend; x+= PIXEL_TILE_SIZE) {

		/* DMA a 1-high by PIXEL_TILE_SIZE-wide area */

		rssDmaPar->x = x;

		if (dmaread_only == 0 ) {
   		errors += _mgras_hqpio_redma_write(rssDmaPar, reWriteMode, DMA_WRTile, 0);
		if (errors && stop_on_err) goto _error;
		}

		if (dmawrite_only == 0) {
   		errors += _mgras_hqpio_redma_read_compare(rssDmaPar, reReadMode, DMA_RDTile, 0);
		if (errors && stop_on_err) goto _error;
		}
		if (dma_overwrite_flag) {
#if 0
		    /* write 0's in the FB/TRAM region */
		    errors += _mgras_hqpio_redma_write(rssDmaPar, reWriteMode, DMA_WRTile_ZERO, 0);
		    if (errors && stop_on_err) goto _error;
#endif
		}
   	} /* for x */
      } /* for y */
      msg_printf(VRB, "\n");

_error:
   _mgras_hq_re_dma_end();

   msg_printf(DBG, "End _mgras_hqpio_redma\n");
   dmawrite_only = dmaread_only = 0;
   return (errors);
}

/* Reads back pixel locations for 1 tile (192 pixels) via indirect reads.
 * This will perform 2* PIXEL_TILE_SIZE (384) reads.
 */
__uint32_t
_mgras_read_indirect(
	RSS_DMA_Params 	*rssDmaPar, 
	__uint32_t 	*expected_data)
{
	__uint32_t subtile, exp, mask, rssnum;
	__uint32_t ppnum, rdram, addr, page, actual, errors = 0;

	/* 
	 * Each rdram holds 32 pixels per scan line, 2 pixels per repeating
 	 * 12-pixel subtile. In a 192 pixel tile, this repeated 16 times.
	 * Since each pixel requires 2 reads, we get 4 reads per 12-pixel
	 * chunk per rdram for a total of 64 reads per rdram.
	 */

	/* tell which RSS to read from */
	rssnum = rssDmaPar->y % numRE4s;
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), rssnum, 
		THIRTEENBIT_MASK);

	/* reset drb pointers for the read */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(RSS_DRBPTRS_PP1))), 0, 
		EIGHTEENBIT_MASK);

	

	page = (rssDmaPar->x / PIXEL_TILE_SIZE) + (7 * (rssDmaPar->y / 16));
	/* 7 is from 1344/192 */

/* #undef PIXEL_SUBTILE_SIZE */
/* #define PIXEL_SUBTILE_SIZE 4 */

	/* addr = 0 since we always start reading at the start of a page */
	for (ppnum = 0; ppnum < PP_PER_RSS; ppnum++) 
	   for (rdram = 0; rdram < RDRAM_PER_PP; rdram++) 
		for (subtile = 0; subtile < PIXEL_TILE_SIZE/PIXEL_SUBTILE_SIZE;	 subtile++) 
		{

		   /* read the 2 pixels in each subtile */
	 	   for (addr = 0; addr <= 6; addr+=2) 
		   {
			exp = *(expected_data + (addr/2)+(4*rdram));
			if (((addr % 4) == 0) && (ppnum))  /* data for PP-B */
				exp += 0x10000;

	   		READ_RE_INDIRECT_REG(rssnum, 
				((ppnum << 28) | 
				((RDRAM0_MEM + rdram) << 24) | 
				(8*subtile) | 
				((rssDmaPar->y % 16) * 128) | 
					/* +(8*16) per y-line within a page*/
				addr | 
				(page << 11) | 
				(PP_1_RSS_DRB_AB << 11)), 
				mask, exp);

			 msg_printf(DBG, "Ind Addr = 0x%x, Read = 0x%x, Exp: 0x%x\n", 
				((ppnum << 28) |
                                ((RDRAM0_MEM + rdram) << 24) |
                                (8*subtile) |
                                ((rssDmaPar->y % 16) * 128) |
                                        /* +(8*16) per y-line within a page*/
                                addr |
                                (page << 11) |
                                (PP_1_RSS_DRB_AB << 11)), actual, exp);

			if ((actual & EIGHTEENBIT_MASK) != exp)
			{
			   msg_printf(ERR, 
			      "ERROR: Addr: 0x%x, Read: 0x%x, Expected: 0x%x\n",
                                (ppnum << 28) | ((RDRAM0_MEM+rdram)<<24) |
                                (8*subtile) | ((rssDmaPar->y % 16) * 128) | 
				(page << 11) | addr | (PP_1_RSS_DRB_AB << 11),actual, 
				exp);
			   msg_printf(VRB, 
				"192 pixel block started at x: %d, y: %d, ",
					rssDmaPar->x, rssDmaPar->y);
			   msg_printf(VRB, "\t==> RSS-%d, PP-%d, RDRAM-%d\n",
				rssnum, ppnum, rdram);
			   errors++;
#if DMA_DEBUG
			   fprintf(stderr, 
				"Addr: 0x%x, Read: 0x%x, Expected: 0x%x\n",
                                (ppnum << 28) | ((RDRAM0_MEM+rdram)<<24) |
                                (8*subtile) | ((rssDmaPar->y % 16)* 128) | 
				(page << 11) | addr | (PP_1_RSS_DRB_AB << 11),actual, 
				exp);
			   fprintf(stderr, 
				"192 pixel block started at x: %d, y: %d\n",
					rssDmaPar->x, rssDmaPar->y);
#endif
			   return (errors);
			}
		   }
		}

	return (errors);
}

/* Writes the same data as the dma writes but uses the indirect addressing mode.
 * Writes a 192-pixel area, 1 pixel high, 192 pixels wide. 
 */
__uint32_t
_mgras_write_indirect(
	RSS_DMA_Params 	*rssDmaPar, 
	__uint32_t 	*data)
{
	__uint32_t subtile, write_data, mask, rssnum;
	__uint32_t ppnum, rdram, addr, page, errors = 0;
	__uint32_t cfifo_status, cfifo_count;

	/* 
	 * Each rdram holds 32 pixels per scan line, 2 pixels per repeating
 	 * 12-pixel subtile. In a 192 pixel tile, this is repeated 16 times.
	 * Since each pixel requires 2 writes, we get 4 writes per 12-pixel
	 * chunk per rdram for a total of 64 writes per rdram.
	 */

	/* tell which RSS to write to */
	rssnum = rssDmaPar->y % numRE4s;
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))), rssnum, 
		THIRTEENBIT_MASK);

	/* reset drb pointers for the write */
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(RSS_DRBPTRS_PP1))), 0, 
		EIGHTEENBIT_MASK);

	mask = EIGHTEENBIT_MASK;

	page = (rssDmaPar->x / PIXEL_TILE_SIZE) + (7 * (rssDmaPar->y / 16));
	/* 7 is from 1344/192 */

	cfifo_count = 0;
	/* addr = 0 since we always start writing at the start of a page */
	for (ppnum = 0; ppnum < PP_PER_RSS; ppnum++) 
	   for (rdram = 0; rdram < RDRAM_PER_PP; rdram++) 
		for (subtile = 0; subtile < PIXEL_TILE_SIZE/PIXEL_SUBTILE_SIZE;				 subtile++) 

		   /* write the 2 pixels in each subtile */
	 	   for (addr = 0; addr <= 6; addr+=2) 
		   {
			write_data = *(data + (addr/2)+(4*rdram));
			if (((addr % 4) == 0) && (ppnum))  /* data for PP-B */
				write_data += 0x10000;
			
			msg_printf(DBG, "pp: %x, rd:%x, subtile:%x, y-line:%x, addr: %x, page: %x, drb: %x\n",
			(ppnum << 28), (RDRAM0_MEM + rdram) << 24, (8*subtile), (rssDmaPar->y % 16) * 128, addr, (page << 11), (PP_1_RSS_DRB_AB << 11));
	   		WRITE_RE_INDIRECT_REG(rssnum, 
				((ppnum << 28) | 
				((RDRAM0_MEM + rdram) << 24) | 
				(8*subtile) | 	/* +8 per subtile in x */
				((rssDmaPar->y % 16) * 128) | 
					/* +(8*16) per y-line within a page */
				addr | 
				(page << 11) | 
				(PP_1_RSS_DRB_AB << 11)), 
				write_data, mask);
			cfifo_count ++;
			if ((cfifo_count & 0xff) == 0x1f) {
    			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    			   if (cfifo_status == CFIFO_TIME_OUT) {
			   	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
				errors++;
			   	return (errors);
    			   }
			}

		   }
	return errors;
}

void
_mgras_hq_re_dma_init(void)
{
   msg_printf(DBG, "Begin _mgras_hq_re_dma_init\n");

#if 0
   /* GE Initialization */
   GE_INITIALIZE();

   /* Clear the CD Flag bit, so we'll know when we're done */
   HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, 0x0, ~0x0);

   START_GIO_ARBITRATION(100);

   SET_REBUS_SYNC();	/* Simulator-specific */
#endif

   msg_printf(DBG, "End   _mgras_hq_re_dma_init\n");
}

/* sets up the 192 pixel (or 256 texel) DMA_WRTile array with the 12 pixel 
 * repeating pattern 
 * we want to write. DMA_WRLine is the 12-pixel array. data is a ptr to 
 * DMA_WRTile. Also sets up the CompareTile for comparing write/read data.
 */
void
_mgras_setupTile(
	RSS_DMA_Params *rssDmaPar,
	__uint32_t *data, 
	__uint32_t pattern,
	__uint32_t *readdata)
{
	__uint32_t i, repeat_num, pat_size, tram_pat_size;
	__uint32_t pp_walk10_size, pp_max_size, tram_max_size;

	repeat_num = 24;
#if 0
	switch ((rssDmaPar->y / numRE4s) % 3) {
		case 0: start = 0; break;
		case 1: start = 4; break;
		case 2: start = 8; break;
		default: start = 99; break;
	}
#endif
	
	pp_max_size = 517120;
	tram_max_size = 517120;
	pat_size = (sizeof(patrn)/sizeof(patrn[0]));
	tram_pat_size = (sizeof(tram_patrn)/sizeof(tram_patrn[0]));
	pp_walk10_size = (sizeof(walk_1_0_32_rss)/sizeof(walk_1_0_32_rss[0]));

#if 0
   	for (i = 0; i < 4096; i++ )  {
		*(DMA_WRTile_ZERO + i) = 0x0;
	}

   	for (i = 0; i < 256; i++ ) 
		DMA_WRTile16_ZERO[i] = 0x0;
#endif

	   msg_printf(DBG, "PIXEL_TILE_SIZE: %d, numRows: %d\n",
		PIXEL_TILE_SIZE, rssDmaPar->numRows);

	for (i = 0; i < pp_max_size; i++) {
		*(data + i) = 0xdeadbeef;
#if 0
		*(compare_data + i) = 0xdeadbeef;
#endif
	}

	/* clear the read buffer */
	bzero(readdata, sizeof(readdata));

	if (rssDmaPar->xfrDevice == DMA_PP1) {

	   /****************************************************************
	    *
	    * PP_WALK_0_1_TEST:
	    *	Walk 0 on 32 bits, then walk 1 on 32 bits. 
	    *
	    * PP_PAGE_ADDR_TEST:
	    *	Write page_num, ~page_num, page_num, ~page_num. 
	    *
	    * PP_PATTERN_TEST:
	    *	Uses patrn (mgras_diag.h) as the repeating pattern of data
	    * 	to write into successive texels. Does 1 pass through memory.
	    *
	    * PP_FF00_TEST:
	    *	Writes 0xffff0000 in each word.
	    *
	    ****************************************************************/

	   msg_printf(DBG, "setupT: pp_dma_pat= 0x%x\n", pp_dma_pat);

	   if (pp_wr_flag)  { /* user specified pattern */
		msg_printf(DBG, "setupT: user spec data\n");
		for (i = 0; i < pp_max_size; i++) 
			*(data + i) = pp_wr_data;
	   }
	   else 
		
	   switch (pp_dma_pat) {
	   	case PP_WALK_0_1_TEST:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i)= walk_1_0_32_rss[i %pp_walk10_size];
			}
			break;			

	   	case PP_PAGE_ADDR_TEST:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
			   if (pattern == PAT_NORM) {
				*(data + i) = ((i*2) << 16) | ((~((i*2) + 1))&0xffff);
			   }
			   else if (pattern == PAT_INV) {
				*(data + i) = (~(i*2) << 16) | (((i*2) + 1)&0xffff);
			   }
			}
			break;			

	   	case PP_PATTERN_TEST_0:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[i % pat_size];
			}
			break;			

	   	case PP_PATTERN_TEST_1:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[(i+1) % pat_size];
			}
			break;			

	   	case PP_PATTERN_TEST_2:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[(i+2) % pat_size];
			}
			break;			

	   	case PP_PATTERN_TEST_3:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[(i+3) % pat_size];
			}
			break;			

	   	case PP_PATTERN_TEST_4:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[(i+4) % pat_size];
			}
			break;			

	   	case PP_PATTERN_TEST_5:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = patrn[(i+5) % pat_size];
			}
			break;			

	   	case PP_FF00_TEST:
			msg_printf(DBG, "setupT: pp_dma_pat case: 0x%x\n", pp_dma_pat);
			for (i = 0; i < pp_max_size; i++) {
				*(data + i) = 0xffff0000;
			}
			break;			

		default: break;
	   }

	   for (i = 0; i < 10; i++) 
		msg_printf(DBG, "setupT: data[%d]: 0x%x, ", i, *(data +i));

	}
	else { /* tram */

	   /****************************************************************
	    *
	    * TRAM_WALK_0_1_TEST:
	    *	Walk 0 on 16 bits, then walk 1 on 16 bits. Each texel gets
	    *	a different pattern, so this repeats every 32 texels.
	    *
	    * TRAM_PAGE_ADDR_TEST:
	    *	Write page_num, ~page_num, page_num, ~page_num. If this test
	    *	is run on a 1x1 texture, this will write to every page in
	    *	256 writes.
	    *
	    * TRAM_PATTERN_TEST:
	    *	Uses patrn (mgras_diag.h) as the repeating pattern of data
	    * 	to write into successive texels. Does 1 pass through memory.
	    *	Since patrn is 32-bit patterns, texel pairs will have the same	
	    *	data.
	    *
	    * TRAM_FF00_TEST:
	    *	Writes 0xffff then 0x0 in successive texels.
	    *
	    ****************************************************************/

	   if (tram_wr_flag)  { /* user specified pattern */
		for (i = 0; i < tram_max_size; i++) 
			*(data + i) = tram_wr_data;
	   }
	   else 
		
	   switch (tram_mem_pat) {
	   	case TRAM_WALK_0_1_TEST:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i)= tram_patrn[i % tram_pat_size];
			}
			break;			

	   	case TRAM_PAGE_ADDR_TEST:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = ((i*2) << 16) | ((~((i*2) + 1))&0xffff);
			}
			break;			

	   	case TRAM_PATTERN_TEST_0:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[i % pat_size];
			}
			break;			

	   	case TRAM_PATTERN_TEST_1:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[(i+1) % pat_size];
			}
			break;			

	   	case TRAM_PATTERN_TEST_2:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[(i+2) % pat_size];
			}
			break;			

	   	case TRAM_PATTERN_TEST_3:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[(i+3) % pat_size];
			}
			break;			

	   	case TRAM_PATTERN_TEST_4:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[(i+4) % pat_size];
			}
			break;			

	   	case TRAM_PATTERN_TEST_5:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = patrn[(i+5) % pat_size];
			}
			break;			

	   	case TRAM_FF00_TEST:
			for (i = 0; i < tram_max_size; i++) {
				*(data + i) = 0xffff0000;
			}
			break;			

		case TRAM_TEX_LINE_PAT:
			for (i = 0; i < tram_max_size; i++) {
			   if (pattern == PAT_NORM) {
				*(data + i) = DMA_WRLine[i % repeat_num];
			   }
			   else if (pattern == PAT_INV) {
				*(data + i) = ~DMA_WRLine[i % repeat_num];
			   }
			}
			break;			
		
		default: break;
	   }

	} /* tram */

#if 0
	/* set up the compare_data array */
	if ((rssDmaPar->xfrDevice == DMA_PP1) && (reReadMode == INDIRECT_ADDR)){
	   for (i = 0; i < PIXEL_TILE_SIZE*rssDmaPar->numRows; i++) 
		   if (pattern == PAT_NORM)
		   	*(compare_data+i) = DMA_RDRAM_PIORead_RDLine[(start+i)%repeat_num];
		   else if (pattern == PAT_INV)	
		   	*(compare_data+i) =~DMA_RDRAM_PIORead_RDLine[(start+i)%repeat_num];
	}
	else if (rssDmaPar->xfrDevice == DMA_PP1) 
	{ /* DMA-DMA to rdram */
	   if (pp_wr_flag) {
	      for (i = 0; i < max_size; i++) 
		   	*(compare_data + i) = pp_wr_data;
	   }
	   else {
#if 0
	   for (i = 0; i < PIXEL_TILE_SIZE*rssDmaPar->numRows; i++) 
		   if (pattern == PAT_NORM)
		   	*(compare_data+i) = DMA_WRLine[(start+i)%repeat_num];
		   else if (pattern == PAT_INV)	
		   	*(compare_data+i) = ~DMA_WRLine[(start+i)%repeat_num];
#else
	   /* mostly blue, with max i = 0xc00 */
	   for (i = 0; i < max_size; i++) 
	  	*(compare_data + i) = (i << 24) | (i <<8) | 0xff00ff;
#endif
	   } /* not user specified data */
	}
#endif
	/* see above for the tram compare data array */

	for (i = 0; i < 10; i++) 
		msg_printf(DBG, "setupT: data[%d]: 0x%x, ", i, *(data +i));

	msg_printf(DBG, "\n");
		
		
}

		
/*
 * Sets up the TE1 and TR1 registers for DMAs 
 */
__uint32_t
_mgras_DMA_TETRSetup(RSS_DMA_Params *rssDmaPar, __uint32_t readOrWrite, __uint32_t db_enable)
{
   __uint32_t 	level, pagenum, ssize, tsize, wbuffer, rbuffer, cfifo_status;
   texmode1u	texmode1 = {0};
   tl_modeu 	tl_mode = {0};
   tl_specu	tl_spec = {0};
   tl_mipmapu	tl_mipmap = {0};
   txsizeu	txsize = {0};
   texmode2u	texmode2 = {0};
   txmipmapu	txmipmap = {0};
   mddma_cntrlu	mddma_cntrl = {0};
   txclampsizeu txclampsize = {0};

   pagenum = rssDmaPar->x;

#if 0
	pagenum = 0;
#endif

   msg_printf(DBG, "Begin _mgras_DMA_TETRSetup\n");

   /* used for double buffering to make sure tl_wbuffer and texrbuffer are 
    *	mutually exclusive.
    *
    * db_enable is encoded in 2 bits so that the lsb is always 1 and the msb 
    * indicates what value rbuffer should have. 
    */

   if (db_enable) {
	if (readOrWrite == DMA_WRITE) {
   		rbuffer = (db_enable & 0x2) >> 1;
		wbuffer = (~rbuffer & 0x1);
	}
	else if (readOrWrite == DMA_READ) {/* swap rbuffer, wbuffer for reads */
   		rbuffer = ~((db_enable & 0x2) >> 1);
		wbuffer = (~rbuffer & 0x1);
	}
   }
   else {
   	rbuffer = 0; wbuffer = 1;
   }
	msg_printf(DBG, "setup:rbuffer: 0x%x, wbuf: 0x%x\n", rbuffer&0x1, 
		wbuffer&0x1);
	msg_printf(DBG, "setup:db_enable: %d\n", db_enable);

   if (rssDmaPar->xfrDevice == DMA_TRAM) {
	if (db_enable & (numTRAMs == 1)) {
   		texmode1.bits.TexComp = TEXMODE_1COMP;	/* L */
   		texmode1.bits.TexEnvMode = TEXMODE_TEV_MODULATE;
   		texmode1.bits.TexLUTmode = TEXMODE_I_RGB; /* I -> RGB */
   		tl_mode.bits.tltexcomp = 0; /* L */
	}
	else {
   		texmode1.bits.TexComp = TEXMODE_4COMP; 	/* RGBA */
   		texmode1.bits.TexEnvMode = TEXMODE_TEV_DECAL;
   		texmode1.bits.TexLUTmode = TEXMODE_RGBA_RGBA; /* RGBA -> RGBA */
   		tl_mode.bits.tltexcomp = 3; /* RGBA */
	}
   }
   else if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
	if (numTRAMs == 1)
   		texmode1.bits.TexComp = TEXMODE_4COMP; 	/* 4 component */
	else	
   		texmode1.bits.TexComp = TEXMODE_4COMP; 	/* 4 component */
   	rbuffer = 0; /* set to 1 so we can verify that readbacks work */
	wbuffer = 1; /* no-op */
   }
   texmode1.bits.TexEn = 1; 		/* enable */
   if (rssDmaPar->xfrDevice == DMA_TRAM_REV)
   	texmode1.bits.NumTram = 2; 	/* 4 trams */
   else
   	texmode1.bits.NumTram = numTRAMs/2; 	/* from NUMBER_OF_TRAMS() */
   texmode1.bits.TexReadSelect = 0;	/* location 0 */
   texmode1.bits.TexLUTbypass = 0;	/* bypass alpha LUT */
   texmode1.bits.BlendUnitCount = 4;	/* 5 pairs */
   texmode1.bits.IAMode = 0;		/* not enabled */

#if 0
   tram_cntrl.bits.refresh = 1;		/* 1us refresh */
   tram_cntrl.bits.ref_count = 1;	/* 1 page refreshed per refresh */
#endif

   tl_mode.bits.tloneden = 0;		/* 1D load not enabled */
   tl_mode.bits.tlwsel = 0;		/* used for 1,2 components only */
   tl_mode.bits.tlmmgen_en = 0;		/* mipmap load sequencing not enabled */
   tl_mode.bits.tldb_en = db_enable & 1;/* 2 bank enabled double buffering  */
   tl_mode.bits.tlsoutbrd_en = 0;	/* store borders at outer edge */
   tl_mode.bits.tlsoutrep_en = 0;	/* border replication not enabled */
   tl_mode.bits.tlsinbrd_en = 0;	/* enable use of border data at s 
						boundary between grp1, grp2 */
   tl_mode.bits.tltbrd_en = 0;		/* border stored on top */
#if 0
   tl_mode.bits.tlctx_write = 0;	/* ??? */
   tl_mode.bits.tlsround_en = 0;	/* ??? */
#endif

   tl_spec.bits.lod = 0;		/* LOD level being dma'd */


   if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
     	if (numTRAMs == 1)
   		tl_mode.bits.tlcompdepth = 2; 	/* 12-bit components */
     	else if (numTRAMs == 4)
   		tl_mode.bits.tlcompdepth = 2; 	/* 12-bit components */
   	tl_spec.bits.tlssize = 0;		/* S-size 2^0 = 1 texel */
   	tl_spec.bits.tltsize = 0;		/* T-size 2^0 = 1 texel */
   	txsize.bits.it_ssize = 0; 	
   	txsize.bits.it_tsize = 0; 
   	txsize.bits.us_ssize = 0;
   	txsize.bits.us_tsize = 0;
   }
   else {
	if (db_enable && (numTRAMs == 1))
   		tl_mode.bits.tlcompdepth = 1; 	/* 8-bit components */
     	else {
	   if (numTRAMs == 1)
   		tl_mode.bits.tlcompdepth = 0; 	/* 4-bit components */
     	   else if (numTRAMs == 4) {
			if (_mgras_rtdma_enabled == 0)
   			tl_mode.bits.tlcompdepth = 0; 	/* 4-bit components */
			else
   			tl_mode.bits.tlcompdepth = 1; 	/* 8-bit components */
		}
	}

	ssize = ide_log(rssDmaPar->numCols);
	tsize = ide_log(rssDmaPar->numRows);	

   	tl_spec.bits.tlssize = ssize;		/* S-size 2^6 = 64*/
   	tl_spec.bits.tltsize = tsize;		/* T-size 2^0 = 1 */
   	txsize.bits.it_ssize = ssize; 	/* 2^6 = 64 */
   	txsize.bits.it_tsize = tsize; 	/* 2^0 = 1 */
   	txsize.bits.us_ssize = ssize;	/* 2^6 = 64 */
   	txsize.bits.us_tsize = tsize;	/* 2^0 = 1 */
	txclampsize.bits.maxaddrsi = rssDmaPar->numCols;
	txclampsize.bits.maxaddrti = rssDmaPar->numRows;
   	mddma_cntrl.bits.rint_bits = 0; /* ssize; */
   	mddma_cntrl.bits.gint_bits = 0; /* tsize;*/
   	mddma_cntrl.bits.bint_bits = 0;
   	mddma_cntrl.bits.gint_bits = 0;
   	mddma_cntrl.bits.mdlut_en = 0;
   }

   tl_spec.bits.tlmaxlod = 0;		/* max LOD automatically generated */

   tl_mipmap.bits.tlmipmap = pagenum;		/* dram page base address */
   tl_mipmap.bits.l_i= 0;		/* left half in grp 1 if == 1 */

   txsize.bits.detail_mn = 0; 		/* 64:1 */
   txsize.bits.us_rsize = 0; 		
   

   if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
   	texmode2.bits.compdepth = TEXMODE2_12BIT; 	/* 12-bit component */
   }
   else {
	if (db_enable && (numTRAMs == 1))
  	 	texmode2.bits.compdepth = 1; 	/* 8-bit component */
	else {
		if (_mgras_rtdma_enabled == 0)
  	 	   texmode2.bits.compdepth = 0; 	/* 4-bit component */
		else
  	 	   texmode2.bits.compdepth = 1; 	/* 8-bit component */
 	      }
   }

   texmode2.bits.db_enable = db_enable & 1;	/* enable db textures */
   texmode2.bits.sacp = 0; 		/* enable s-clamp at unit square */
   texmode2.bits.tacp = 0; 		/* enable t-clamp at unit square */
   if (readOrWrite == DMA_READ) {
   	texmode2.bits.bc_enable = 1;
   	texmode2.bits.mag = 0;
   	texmode2.bits.min = 0;
   	texmode2.bits.clamp_gl5 = 1;
   }
   else { /* writes */
   	texmode2.bits.bc_enable = 0;
   	texmode2.bits.mag = 1;
   	texmode2.bits.min = 1;
   	texmode2.bits.clamp_gl5 = 0;
   }
   texmode2.bits.mag_detail = 0;
   texmode2.bits.mm_enable = 0;
   if ((rssDmaPar->xfrDevice == DMA_TRAM_REV) && (readOrWrite == DMA_READ)) {
	texmode2.bits.tram_read_reg = 1; /* read the tram revision reg */
   	texmode2.bits.compdepth = TEXMODE2_12BIT;
   }

   txmipmap.bits.l_i = 0;
   txmipmap.bits.tmipmap = pagenum;

   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
   if (cfifo_status == CFIFO_TIME_OUT) {
	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
	return (-1);
   }

   msg_printf(DBG, "TEXMODE1 0x%x \n", texmode1.val);
   msg_printf(DBG, "TEXMODE2 0x%x \n", texmode2.val);
   msg_printf(DBG, "MDDMA_CNTRL 0x%x \n", mddma_cntrl.val);

   WRITE_RSS_REG(0, TEXMODE1, texmode1.val, ~0x0);
   WRITE_RSS_REG(0, TEXMODE2, texmode2.val, ~0x0);
   WRITE_RSS_REG(0, MDDMA_CNTRL, mddma_cntrl.val, ~0x0);
   WRITE_RSS_REG(0, 0x18f, 0, ~0x0);

   if (_mgras_rtdma_enabled == 1) {
       if (readOrWrite == DMA_READ) {
	 WRITE_RSS_REG(0, TL_MODE, (tl_mode.val | 0x4000), ~0x0); /*bottom-up*/
	msg_printf(DBG, "TL_MODE 0x%x \n", (tl_mode.val|0x4000) );
	}
       else {
	 WRITE_RSS_REG(0, TL_MODE, (tl_mode.val | 0x4000 | (0x30000)), ~0x0); /* bottom-up , RGBA8 -> RGBA8 */
	msg_printf(DBG, "TL_MODE 0x%x \n", (tl_mode.val|0x4000|(0x30000)) );
	}
   }
   else {
	WRITE_RSS_REG(0, TL_MODE, (tl_mode.val | 0x4000), ~0x0); /* bottom-up */
	msg_printf(DBG, "TL_MODE 0x%x \n", (tl_mode.val|0x4000)) ;
	}


   msg_printf(DBG, "TL_SPEC 0x%x \n", tl_spec.val);
   WRITE_RSS_REG(0, TL_SPEC, tl_spec.val, ~0x0);

   if (readOrWrite == DMA_READ) {
#if HQ4
	if (_mgras_rtdma_enabled == 1) {
	 	WRITE_RSS_REG(0, TL_VIDCNTRL, 0, ~0x0); DELAY(5000);	
	}
#endif

   	WRITE_RSS_REG(0, TXTILE, 0, ~0x0);
   	WRITE_RSS_REG(0, TXBCOLOR_RG, 0, ~0x0);
   	WRITE_RSS_REG(0, TXBCOLOR_BA, 0, ~0x0);
#if 0
   WRITE_RSS_REG(0, TRAM_CNTRL, tram_cntrl.val, ~0x0);
#endif
   	WRITE_RSS_REG(0, MAX_PIXEL_RG, 0, ~0x0);
   	WRITE_RSS_REG(0, MAX_PIXEL_BA, 0, ~0x0);
   	WRITE_RSS_REG(0, TXDETAIL, 0, ~0x0);
   	WRITE_RSS_REG(0, TXADDR, 0x0, ~0x0);
   	WRITE_RSS_REG(0, DETAILSCALE, 0, ~0x0);
   	WRITE_RSS_REG(0, TXLOD, 0, ~0x0);
   	WRITE_RSS_REG(0, TXCLAMPSIZE, txclampsize.val, ~0x0);
   	msg_printf(DBG, " DMA_READ: TXCLAMPSIZE 0x%x \n", txclampsize.val);

   }

  
   	msg_printf(DBG, "TEXRBUFFER 0x%x \n", (rbuffer & 0x1)  );
   WRITE_RSS_REG(0, TEXRBUFFER, (rbuffer & 0x1), ~0x0);
   	msg_printf(DBG, "TL_WBUFFER 0x%x \n", wbuffer );
   WRITE_RSS_REG(0, TL_WBUFFER, wbuffer, ~0x0);

   if (readOrWrite == DMA_WRITE) {
   	WRITE_RSS_REG(0, TL_BASE, 0, ~0x0);
   	WRITE_RSS_REG(0, TL_ADDR, 0, ~0x0);
   	WRITE_RSS_REG(0, TL_BORDER, 0, ~0x0);
   	WRITE_RSS_REG(0, TL_S_SIZE, rssDmaPar->numCols, ~0x0);
   	WRITE_RSS_REG(0, TL_T_SIZE, rssDmaPar->numRows, ~0x0);
   	msg_printf(DBG, " DMA_WRITE: TL_S_SIZE 0x%x \n", rssDmaPar->numCols);
   	msg_printf(DBG, " DMA_WRITE: TL_T_SIZE 0x%x \n", rssDmaPar->numRows);
   	WRITE_RSS_REG(0, TL_ADDR, 0, ~0x0);
   	WRITE_RSS_REG(0, TL_MIPMAP, tl_mipmap.val, ~0x0);
   	msg_printf(DBG, " DMA_WRITE: TL_MIPMAP 0x%x \n", tl_mipmap.val);

	/* for normal setup, start at 0,0. */
   	WRITE_RSS_REG(0, TL_S_LEFT, 0, ~0x0);
   	WRITE_RSS_REG(0, TL_T_BOTTOM, 0, ~0x0);
#if HQ4 
	if (_mgras_rtdma_enabled == 1) {
   	/*	
	WRITE_RSS_REG(0, TL_VIDABORT, 1, ~0x0);
	DELAY(5000);	
	*/
	
	}
#endif
   }


   if (readOrWrite == DMA_READ) {
   	WRITE_RSS_REG(0, TXSIZE, txsize.val, ~0x0);
   	msg_printf(DBG, " DMA_READ: TXSIZE 0x%x \n", txsize.val);
   	WRITE_RSS_REG(0, TXADDR, 0, ~0x0);
   	WRITE_RSS_REG(0, TXMIPMAP, txmipmap.val, ~0x0);
   	msg_printf(DBG, " DMA_READ: TXMIPMAP 0x%x \n", txmipmap.val);
   	WRITE_RSS_REG(0, TXADDR, 0, ~0x0);
   	WRITE_RSS_REG(0, TXBORDER, 0, ~0x0);
   	WRITE_RSS_REG(0, TXBASE, 0, ~0x0);
   }


   msg_printf(DBG, "End _mgras_DMA_TETRSetup\n");

   return 0;
}

/*
 * Sets up the RSS registers to perform DMA BURST or DMA PIO operation
 */
__uint32_t
_mgras_dma_re_setup (RSS_DMA_Params *rssDmaPar, __uint32_t dmakind, __uint32_t readOrWrite, __uint32_t db_enable)
{
   __uint32_t bytesPerPixel, scanWidth, pixCmdPP1, reif_dmaType, cmd, xfrsize, cfifo_status;
   fillmodeu 	fillMode = {0};
   xfrmodeu  	xfrMode = {0};
   ppfillmodeu 	ppfillMode = {0};
   iru 		ir = {0};
   xfrcontrolu	xfrCtrl = {0};
   xfrcontrolu	xfrCtrl_nogo = {0};
   ppdrbsizeu	drbsize = {0};
   __uint32_t actual, time;

   msg_printf(DBG, "Begin _mgras_dma_re_setup\n");

#if 0
   READ_RSS_REG(0x0, STATUS, status_data, 0xffffffff);
#endif

   /* Default is pixComponents=4, pixComponentSize=1  ==> 4 bytes per*/

   msg_printf(DBG, "in setup: pp_dma_mode: %x\n", pp_dma_mode);

   bytesPerPixel = rssDmaPar->pixComponents * rssDmaPar->pixComponentSize;

   if (rssDmaPar->xfrDevice == DMA_PP1) {
	if (pp_dma_mode == PP_RGBA8888)
   	   bytesPerPixel = rssDmaPar->pixComponents * rssDmaPar->pixComponentSize;
	else
   	   bytesPerPixel = 6; /* for 121212 for the HQ3 */
   }

   else if (rssDmaPar->xfrDevice == DMA_TRAM) {
	if (db_enable && (numTRAMs == 1)) /*  L data */
   		bytesPerPixel = 1; /* 8 ==> 1 bytes/per */
	else {
   		/* use 4,4,4,4 for 1 and 4 TRAMs */
   		bytesPerPixel >>= 1; /* 4444 ==> 2 bytes/per */
		if (_mgras_rtdma_enabled == 1) { 
		bytesPerPixel = 
			rssDmaPar->pixComponents * rssDmaPar->pixComponentSize;
		}
	     }
   }

   scanWidth = bytesPerPixel * rssDmaPar->numCols;
   
   fillMode.bits.BlockType  = ((dmakind == DMA_BURST) ? 0x4 : 0x2);
   fillMode.bits.BlockType |= ((readOrWrite == DMA_WRITE) ? 0x1: 0x0);

   xfrMode.bits.StrideSkipBytes = rssDmaPar->strideSkipBytes;
   xfrMode.bits.BeginSkipBytes = rssDmaPar->beginSkipBytes;
   if ((rssDmaPar->xfrDevice == DMA_TRAM) && db_enable && (numTRAMs == 1))
   	xfrMode.bits.PixelFormat = XFRMODE_L1;
   else
   	xfrMode.bits.PixelFormat = XFRMODE_RGBA4;

   if ((rssDmaPar->xfrDevice == DMA_PP1) && (pp_dma_mode == PP_RGB12))
   	xfrMode.bits.PixelFormat = XFRMODE_RGB3;

   xfrMode.bits.RGBorder = 0; /* ABGR */

   /* default PixelCompType = 0 -- 1byte, unsigned */
   if (rssDmaPar->xfrDevice == DMA_TRAM) {
	if (db_enable && (numTRAMs == 1))
                xfrMode.bits.PixelCompType = XFRMODE_1U; /* 4,4 */
	else {
		if (_mgras_rtdma_enabled == 0)
                xfrMode.bits.PixelCompType = XFRMODE_4444U; /* 4,4,4,4 */
		else
                xfrMode.bits.PixelCompType = XFRMODE_1U; /* 8,8,8,8 */
	     }
   }
   else if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
           if (numTRAMs == 1) {
                xfrMode.bits.PixelCompType =  XFRMODE_2U; /* 2bytes,unsign */  
              	xfrMode.bits.PixelFormat = XFRMODE_RGBA4;
	   }
           else if (numTRAMs == 4) {
                xfrMode.bits.PixelCompType =  XFRMODE_2U; /* 2bytes,unsign */ 
               	xfrMode.bits.PixelFormat = XFRMODE_RGBA4;
	   }
   }
   else if (rssDmaPar->xfrDevice == DMA_PP1) {
	if (pp_dma_mode == PP_RGBA8888) {
   	 	ppfillMode.bits.PixType = PP_FILLMODE_RGBA8888; /* RGBA 32bit */
   	}
   	else {
   		ppfillMode.bits.PixType = PP_FILLMODE_RGB121212; /* RGB 36bit */
        	xfrMode.bits.PixelCompType =  XFRMODE_2U; /* 2bytes,unsign */ 
   	}
   }
   ppfillMode.bits.BufCount = 0;
   ppfillMode.bits.BufSize  = 1;
   ppfillMode.bits.LogicOP = 3; /* src */
   ppfillMode.bits.DrawBuffer  = 1;

   ir.bits.Setup = 1;
   ir.bits.Opcode = 0x8; /* BLOCK MODE */

   xfrCtrl.bits.XFR_DMAStart = ((dmakind == DMA_BURST) ? 0x1 : 0x0);
   xfrCtrl.bits.XFR_PIOStart = ((dmakind == DMA_BURST) ? 0x0 : 0x1);
   xfrCtrl.bits.XFR_Device   = ((rssDmaPar->xfrDevice == DMA_PP1) ? 0x0 : 0x1);
   xfrCtrl.bits.XFR_Direction = ((readOrWrite == DMA_WRITE) ? 0x0 : 0x1);

   xfrCtrl_nogo.bits.XFR_Device = xfrCtrl.bits.XFR_Device;
   xfrCtrl_nogo.bits.XFR_Direction = xfrCtrl.bits.XFR_Direction;

   pixCmdPP1 = ((readOrWrite == DMA_WRITE) ? 0x3 : 0x2);

   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
   if (cfifo_status == CFIFO_TIME_OUT) {
	msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
	return (1);
   } 

#if 0
   time = 500; /* MGRAS_DMA_TIMEOUT_COUNT; */
   do {
	READ_RSS_REG(4, STATUS, 0, ~0x0);
	time--;
   } while ( ((actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) && time);
#endif

   if ((rssDmaPar->xfrDevice == DMA_TRAM) && (readOrWrite == DMA_WRITE)) {
	/* don't do those writes */
   } else {
   	WRITE_RSS_REG(0x0, XFRMASKLOW, 0xffffffff, 0xffffffff);
   	WRITE_RSS_REG(0x0, XFRMASKHIGH, 0xffffffff, 0xffffffff);
   	WRITE_RSS_REG(0x0, FILLMODE, fillMode.val, 0xffffffff);
   }

   xfrsize = ((rssDmaPar->numRows << 16) + rssDmaPar->numCols);

   WRITE_RSS_REG(0x0, XFRSIZE, xfrsize, 0xffffffff);
   WRITE_RSS_REG(0x0, XFRCOUNTERS, xfrsize, 0xffffffff);
   WRITE_RSS_REG(0x0, XFRMODE, xfrMode.val, 0xffffffff);

   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, REIF_SCAN_WIDTH, 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, scanWidth, ~0x0);

   reif_dmaType = ((readOrWrite == DMA_WRITE) ? 0x4 : 0x2);
   cmd = BUILD_CONTROL_COMMAND(0, 0, 1, 0, REIF_DMA_TYPE, 4);
   HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0);
   HQ3_PIO_WR(CFIFO1_ADDR, reif_dmaType, ~0x0); 

   if (rssDmaPar->xfrDevice == DMA_PP1) {
   	WRITE_RSS_REG(0x0, BLOCK_XYSTARTI, ((rssDmaPar->x << 16) + rssDmaPar->y), 0xffffffff);
   	WRITE_RSS_REG(0x0, BLOCK_XYENDI, (((rssDmaPar->x + rssDmaPar->numCols - 1) << 16) + 
		(rssDmaPar->y + rssDmaPar->numRows - 1)), 0xffffffff);

	msg_printf(DBG, "BLOCK_XYENDI: 0x%x, rss-x:0x%x, rss-y:0x%x, rows:%d\n",
		(((rssDmaPar->x + rssDmaPar->numCols - 1) << 16) +
                (rssDmaPar->y + rssDmaPar->numRows - 1)), rssDmaPar->x, 
		rssDmaPar->y, rssDmaPar->numRows);
   }
   else if ((rssDmaPar->xfrDevice == DMA_TRAM) || 
		(rssDmaPar->xfrDevice == DMA_TRAM_REV)) {
	if (readOrWrite == DMA_READ) {
   	WRITE_RSS_REG(0x0, BLOCK_XYSTARTI, 0x0, 0xffffffff);
   	WRITE_RSS_REG(0x0, BLOCK_XYENDI, (((rssDmaPar->numCols - 1) << 16) +
		(rssDmaPar->numRows - 1)), 0xffffffff);
	}
   }

   if (rssDmaPar->xfrDevice == DMA_PP1) {
	if (XMAX == 1344)
		drbsize.bits.DRB_Xtiles36 = 7; /* for 1344 screen*/
	else if (XMAX == 2112)
		drbsize.bits.DRB_Xtiles36 = 11; /* for all of memory */
	else {
		drbsize.bits.DRB_Xtiles36 = XMAX/192;
		if ((drbsize.bits.DRB_Xtiles36 * 192) < XMAX) {
			drbsize.bits.DRB_Xtiles36 += 1;
		}
	}
	drbsize.bits.DRB_Xtiles9 = XMAX/768;
	if ((drbsize.bits.DRB_Xtiles9 * 768) < XMAX) {
		drbsize.bits.DRB_Xtiles9 += 1;
	}
	/* 3 = 1 rss, 2 = 2 rss, 0 = 4 rss */
	drbsize.bits.DMA_line_count_mask= 4 - numRE4s; 
	/* 0 = 1 rss, 1 = 2 rss, 3 = 4 rss */
	drbsize.bits.DRB_RSS = numRE4s - 1; 

   	WRITE_RSS_REG(0x0, RSS_PIXCMD_PP1, pixCmdPP1, 0xffffffff);
   	WRITE_RSS_REG(0x0, RSS_FILLCMD_PP1, ppfillMode.val, 0xffffffff);
#if 0
	if (pp_dma_mode == PP_RGBA8888) {
   		WRITE_RSS_REG(0x0,RSS_DRBPTRS_PP1, PP_1_RSS_DRB_AB, 0xffffffff);
	}
	else {
   		WRITE_RSS_REG(0x0, RSS_DRBPTRS_PP1, 0x0, 0xffffffff);
	}
#endif
   	WRITE_RSS_REG(0x0, RSS_DRBPTRS_PP1, pp_drb, 0xffffffff);
   	WRITE_RSS_REG(0x0, RSS_DRBSIZE_PP1, drbsize.val, 0xffffffff);
   	WRITE_RSS_REG(0x0, RSS_COLORMASK_MSB_PP1, rssDmaPar->rss_clrmsk_msb_pp1, 0xffffffff);
   	WRITE_RSS_REG(0x0, RSS_COLORMASK_LSBA_PP1, rssDmaPar->rss_clrmsk_lsba_pp1, 0xffffffff);
   	WRITE_RSS_REG(0x0, RSS_COLORMASK_LSBB_PP1, rssDmaPar->rss_clrmsk_lsbb_pp1, 0xffffffff);
   }

   if ((rssDmaPar->xfrDevice == DMA_TRAM) && (readOrWrite == DMA_WRITE)) {
        /* don't do those writes */
   }
   else {
   /* need this write! */
   WRITE_RSS_REG(0x0, XYWIN, 0x0, 0xffffffff);
   }

	/* debug info */
	HQ3_PIO_RD(SCAN_WIDTH_OFF, ~0x0, actual);
	msg_printf(DBG, "SCAN_WIDTH_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(RE_DMA_MODES_OFF, ~0x0, actual);
	msg_printf(DBG, "RE_DMA_MODES_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(RE_DMA_CNTRL_OFF, ~0x0, actual);
	msg_printf(DBG, "RE_DMA_CNTRL_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(RE_DMA_TYPE_OFF, ~0x0, actual);
	msg_printf(DBG, "RE_DMA_TYPE_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(LINE_COUNT_OFF, ~0x0, actual);
	msg_printf(DBG, "LINE_COUNT_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_PG_WIDTH_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_PG_WIDTH_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_ROW_OFFSET_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_ROW_OFFSET_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_ROW_START_ADDR_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_ROW_START_ADDR_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_LINE_CNT_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_LINE_CNT_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_FIRST_SPAN_WIDTH_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_FIRST_SPAN_WIDTH_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_LAST_SPAN_WIDTH_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_LAST_SPAN_WIDTH_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_Y_DEC_FACTOR_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_Y_DEC_FACTOR_OFF: 0x%x\n", actual);
	HQ3_PIO_RD(IM_DMA_CTRL_OFF, ~0x0, actual);
	msg_printf(DBG, "IM_DMA_CTRL_OFF: 0x%x\n", actual);
	
   msg_printf(DBG, "Before kicking off the DMA control register\n");

   flush_cache();

#if 1
   time = 500; /* MGRAS_DMA_TIMEOUT_COUNT; */
   do {
	READ_RSS_REG(4, STATUS, 0, ~0x0);
	time--;
   } while ( ((actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) && time);

   if (((actual & (RE4BUSY_BITS | RE4PP1BUSY_BITS)) != 0) && (time ==0)) {
      if (readOrWrite == DMA_READ) {
           msg_printf(ERR, "RE4 STATUS register is not idle during DMA setup. status reg = 0x%x\n", actual);
      }
      else { /* It's okay to be busy during a write, though this test probably 
	      * will fail somewhere down the line.
	      */
           msg_printf(VRB, "WARNING: RE4 STATUS register is not idle during DMA setup, status reg = 0x%x\n", actual);
      }
   }
#endif
	
    if ((rssDmaPar->xfrDevice == DMA_PP1) ||
	((rssDmaPar->xfrDevice == DMA_TRAM || rssDmaPar->xfrDevice == DMA_TRAM_REV) &&
	 (readOrWrite == DMA_READ))) {
      	/* Warn the RSS of direction (Bug workaround) */
  	 WRITE_RSS_REG(0x0, XFRCONTROL, xfrCtrl_nogo.val, 0xffffffff);
    }

#if 1

	/* write is okay with IR before XFRCONTROL */
   	if (rssDmaPar->xfrDevice == DMA_PP1) {
      		HQ3_PIO_WR_RE_EX(RSS_BASE + HQ_RSS_SPACE(IR_ALIAS), ir.val, 
			0xffffffff);
   	}

      	/* DMA kicks off after this write */
	if (rssDmaPar->xfrDevice == DMA_PP1) {
  	 	WRITE_RSS_REG(0x0, XFRCONTROL, xfrCtrl.val, 0xffffffff);
	}
	else {
	    WRITE_RSS_REG_DBL(0x0, XFRCONTROL, 0x00005555, xfrCtrl.val, ~0x0);
	}

#endif

   flush_cache();

   msg_printf(DBG, "End   _mgras_DMA_RESetup\n");

   return(0);
}

/*
 * Program RSS if nexessary. Send data to PP1/TRAM.
 */
__uint32_t
_mgras_hqpio_redma_write(RSS_DMA_Params *rssDmaPar, __uint32_t reWriteMode, __uint32_t *data, __uint32_t db_enable)
{
   __uint32_t nWords, nWordsPerRow, i, j, errors = 0, inc;
   __uint32_t status, pix_cmd, data_high, data_low, cfifo_status;

   msg_printf(DBG, "Begin _mgras_hqpio_redma_write\n");

   if (rssDmaPar->xfrDevice == DMA_PP1) {
	if (pp_dma_mode == PP_RGBA8888) {
   		nWords = (rssDmaPar->numCols * rssDmaPar->numRows);
		inc = 2;
	}
	else {
		/* 12bit data has 1.5x nWords */
   		nWords = (3*(rssDmaPar->numCols * rssDmaPar->numRows))/2;
		inc = 2;
	}
   }
   else if (rssDmaPar->xfrDevice == DMA_TRAM) {
	if (db_enable && (numTRAMs == 1)) { /* L data */
		inc = 2;
		nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/4;
	}
	else {
		inc = 2;
		if (numTRAMs == 1) /* 4444 data */
			nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/2;
		else if (numTRAMs == 4)
			nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/2;
	}
   }
   else if (rssDmaPar->xfrDevice == DMA_TRAM_REV) {
		if (numTRAMs == 1)
   			nWords = (rssDmaPar->numCols * rssDmaPar->numRows);
		else if (numTRAMs == 4)
   			nWords = (rssDmaPar->numCols * rssDmaPar->numRows)*2;
   }

#if DMA_DEBUG
	fprintf(stderr, "nWords is %d, numTRAMs is %d\n", nWords, numTRAMs);
#endif

   switch (reWriteMode) {
      case DMA_BURST:
		/* Setup the RSS for DMA_BURST_WRITE mode */
   		_mgras_dma_re_setup(rssDmaPar, reWriteMode,DMA_WRITE,db_enable);

		nWordsPerRow = nWords/(rssDmaPar->numRows);
		/* Stuffs data into CFIFO so RE DMA controller can take it */
   		pix_cmd = BUILD_PIXEL_COMMAND(0, 0, 0, 0x2, (nWordsPerRow<< 2));
			/* byte count = nWords << 2 */

   		for (j = 0; j < rssDmaPar->numRows; j++) {
   		   HQ3_PIO_WR(CFIFO1_ADDR, pix_cmd, ~0x0);
   		   for (i = 0; i < nWordsPerRow; i+=inc) {
      		 	data_high = *(data+i + (j * nWordsPerRow));
      		   	data_low  = *(data+i+1 + (j * nWordsPerRow));
      		   	HQ3_PIO_WR64((CFIFO1_ADDR), data_high, data_low, ~0x0);
#if 1 
		   	if ((i & 0xff) == 0x40) {
    			   WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL);
    			   if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
				return (-1);
    			   }
		   	}
#endif

#if 0
	fprintf(stderr, "%d: data_high: 0x%x, data_low: 0x%x\n", 
		i, data_high, data_low);
#endif
   		   }
		}

		/* Send synchronization token and wait */
   		errors += _mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_BURST);
		break;
      case DMA_PIO:
		/* Setup the RSS for DMA_PIO_WRITE mode */
   		_mgras_dma_re_setup(rssDmaPar, reWriteMode,DMA_WRITE,db_enable);

		/* Stuffs the data into CHAR regs so that RE DMA controller can take it */
		msg_printf(DBG, "data addr: 0x%x\n", data);
   		for (i = 0; i < nWords; i+=2) {
      		   data_high = *(data+i);
      		   data_low  = *(data+i+1);
#if 1
		   if ((i & 0xff) == 0x40) {
    			WAIT_FOR_CFIFO(cfifo_status, CFIFO_COUNT, CFIFO_TIMEOUT_VAL); /* XXX: Tunable parameters */
    			if (cfifo_status == CFIFO_TIME_OUT) {
				msg_printf(ERR, "HQ3 CFIFO IS FULL...EXITING\n");
				return (-1);
    			}
		   }
#endif
		   HQ3_PIO_WR_RE((RSS_BASE + (CHAR_H << 2)), data_high, ~0x0);
		   HQ3_PIO_WR_RE_EX((RSS_BASE + (CHAR_L << 2)), data_low, ~0x0);
   		}

		/* Send synchronization token and wait */
		errors += _mgras_hq_re_dma_WaitForDone(DMA_WRITE, DMA_PIO);
		break;
      case INDIRECT_ADDR:
		errors += _mgras_write_indirect(rssDmaPar, DMA_RDRAM_PIORead_RDLine);
		break;

   }

   msg_printf(DBG, "End   _mgras_hqpio_redma_write\n");

   return(errors);
}

/*
 * Program RSS if nexessary. Receive data from PP1/TRAM.
 */
__uint32_t
_mgras_hqpio_redma_read_compare(RSS_DMA_Params *rssDmaPar, __uint32_t reReadMode, __uint32_t *data, __uint32_t db_enable)
{
   __uint32_t errors = 0, actual, timer;
   __uint32_t data_high, data_low, addr_hi, addr_lo;
   __uint32_t destDevice = rssDmaPar->xfrDevice;
   register nWords, i;
#if DMA_DEBUG2
	FILE *fp;	

	fp = fopen("data2.out", "a+");
#endif

   msg_printf(DBG, "Begin _mgras_hqpio_redma_read_compare\n");

   nWords = (rssDmaPar->numCols * rssDmaPar->numRows);

   if (destDevice == DMA_PP1) {
	if (pp_dma_mode == PP_RGBA8888)
   		nWords = (rssDmaPar->numCols * rssDmaPar->numRows);
	else
   		nWords = (3*(rssDmaPar->numCols * rssDmaPar->numRows))/2;
   }
   if (destDevice == DMA_TRAM)  {
	if (db_enable && (numTRAMs == 1)) 
		nWords >>= 2;
	else
		nWords >>= 1;
   }

#if DMA_DEBUG
	fprintf(stdout, "rssDmaPar->numCols: %d, rssDmaPar->numRows: %d\n",
		rssDmaPar->numCols, rssDmaPar->numRows);
#endif
   switch (reReadMode) {
      case DMA_PIO:
		if ((rssDmaPar->xfrDevice == DMA_TRAM) ||
                (rssDmaPar->xfrDevice == DMA_TRAM_REV))
                	errors += _mgras_DMA_TexReadSetup(rssDmaPar, reReadMode, DMA_READ,
                           1, 0, db_enable);
        	else
			/* Setup the RSS for DMA_PIO_READ mode */
   			_mgras_dma_re_setup(rssDmaPar, reReadMode, DMA_READ, 
				db_enable);

#if 0
		/* Clear the read buffer */
   		bzero(DMA_RDData, sizeof(DMA_RDData));
#endif

		/* make sure the re cmd fifo is empty before reading so 
		 * we are sure the dma commands have been sent down.
		 */
		actual = 0x0;
		timer = 0xffff;
		while (((actual & 0x100) != 0x100) && timer) {
		   HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(STATUS))), 
			TWELVEBIT_MASK, actual, NO_PRINT_OUT);
		   timer--;
		}

		if ((timer == 0) && ((actual & 0x100) != 0x100)) {
			msg_printf(ERR, "DMA Timed Out: Waiting for the RE4 CmdFIFO to go empty before reading DMA-PIO data\n");
			return (1);
		}

		/* Read the data */
#if DMA_DEBUG
		fprintf(stderr, "\ny: %d, x:%d\n", rssDmaPar->y, rssDmaPar->x);
#endif
		addr_hi = ((RSS_BASE + (HQ_RSS_SPACE(CHAR_H))) + 0x1000);
		addr_lo = (RSS_BASE + (HQ_RSS_SPACE(CHAR_L)));
   		for (i = 0; i < nWords; i+=2) {
		   HQ3_PIO_RD_RE_NOSTATUS(addr_hi,~0x0,data_high, NO_PRINT_OUT);
		   HQ3_PIO_RD_RE_NOSTATUS(addr_lo,~0x0, data_low, NO_PRINT_OUT);
        	   *(data+i) = data_high;
        	   *(data+i+1) = data_low;
#if 0
		   if ((i & 0xfff) == 0x100) {
			busy(busy_bit);
			if (busy_bit)
				busy_bit = 0;		
			else
				busy_bit = 1;
		   }
#endif
#if DMA_DEBUG
		   fprintf(stderr, "%d: data_high = %x ; data_low = %x\n",
			i, data_high, data_low);
#endif
#if DMA_DEBUG2
		   fprintf(fp, "0x%x, 0x%x, ", data_high, data_low);
#endif
   		}

#if DMA_DEBUG2
		fprintf(fp, "\n");
		if (rssDmaPar->y == 20)
		   fprintf(fp, "\n -------------------------------------\n");
		fclose(fp);
#endif
		if (nocompare == 0) {
		   /* Compare the data */
		   if (destDevice == DMA_TRAM)
		      errors += _mgras_hqpio_redma_compare(rssDmaPar, 
				DMA_WRTile, DMA_RDTile, db_enable);
		   else
		      errors += _mgras_hqpio_redma_compare(rssDmaPar, 
				DMA_WRTile, DMA_RDTile, db_enable);
		}
		_mgras_hq_re_dma_WaitForDone(DMA_READ, DMA_PIO);
		break;
      case INDIRECT_ADDR:
		errors += _mgras_read_indirect(rssDmaPar, 
					DMA_RDRAM_PIORead_RDLine);
		break;
   }

   msg_printf(DBG, "End   _mgras_hqpio_redma_read_compare\n");

   return(errors);
}

__uint32_t
_mgras_hq_re_dma_WaitForDone(__uint32_t is_read, __uint32_t is_dmapio)
{
   __uint32_t 	level, cd_flag_set, status, actual;
   xfrcontrolu  xfrctrl = {0};

/**********************************************************************
   if (headless_dmawrite) {
	
   if (dmapio) {
	check 1st 5 bits of (hq_sync, hif, cfifo, cd , reif
	check rss_idle
   }
   else
   if (dma_burst) {
	check cd_sync
   	if (dma_burst_write) {
		check reif_dma_active
		check rss_idle
   	}
   	if (dma_burs_read) {
		check bits 9, 12 in busy (mstr_dma, hag_dma)
		check rss_idle
   	}
   }
**********************************************************************/
   msg_printf(DBG, "Start   _mgras_hq_re_dma_WaitForDone\n");

   if (is_dmapio == DMA_PIO) {
        HQ3_POLL_DMA (0x0, (HQ_SYNC_BUSY | HIF_BUSY | CFIFO_BUSY | CD_BUSY |
		REIF_BUSY), BUSY_DMA_ADDR, status);
        if (status == DMA_TIMED_OUT) {
	   if (is_read)
		msg_printf(VRB, "DMA Read...\n");
	   else
		msg_printf(VRB, "DMA Write...\n");
           msg_printf(VRB, "DMA Timed out in busy bits\n");
           HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
           HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
           HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
           return(status);
        }
        msg_printf(DBG, "................Polling for RSS_IDLE\n");
        HQ3_POLL_DMA((1 << 1), (1 << 1), STATUS_ADDR, status); 
        if (status == DMA_TIMED_OUT) {
	   if (is_read)
		msg_printf(VRB, "DMA Read...\n");
	   else
		msg_printf(VRB, "DMA Write...\n");
	   msg_printf(VRB, "DMA Timed out polling for HQ RSS_IDLE bit\n");
	   HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
	   HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
   	   return(status);
        }
   }
   else if (is_dmapio == DMA_BURST) { /* is dma burst */

#if 1
      /* set and check cd flag */
      cd_flag_set = BUILD_CONTROL_COMMAND(0,0,1, 0, HQ_CD_ADDR_CD_FLAG_SET, 0 );

      /* Resynchronize - wait for completion of the dma */
      HQ3_PIO_WR(CFIFO1_ADDR, cd_flag_set, ~0x0);

      /* Poll till dma is finished */
      HQ3_POLL_DMA(HQ_FLAG_CD_FLAG_BIT, HQ_FLAG_CD_FLAG_BIT, FLAG_SET_PRIV_ADDR,	 status);
      if (status == DMA_TIMED_OUT) {
	   if (is_read)
		msg_printf(VRB, "DMA Read...\n");
	   else
		msg_printf(VRB, "DMA Write...\n");
        msg_printf(VRB, "DMA Timed out in cd flag bit\n");
        HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
        msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
        HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
        msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
        HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
        msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
	/* reset the cd flag */
	HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, (HQ_FLAG_CD_FLAG_BIT),(HQ_FLAG_CD_FLAG_BIT));
        HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
        msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
        return(status);
      }

	/* reset the cd flag */
	HQ3_PIO_WR(FLAG_CLR_PRIV_ADDR, (HQ_FLAG_CD_FLAG_BIT),(HQ_FLAG_CD_FLAG_BIT));
        HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
        msg_printf(DBG, "HQ3 Flag set reg: 0x%x\n", actual);
#endif

      if (is_read == DMA_READ) { /* dma burst read */

        HQ3_POLL_DMA (0x0, (HQ_BUSY_HAG_DMA_BIT | HQ_BUSY_MSTR_DMA_BIT), 
		BUSY_DMA_ADDR, status);
        if (status == DMA_TIMED_OUT) {
	   msg_printf(VRB, "DMA Read...\n");
           msg_printf(VRB, "DMA Timed out in busy hag bit\n");
           HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
           HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
           HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
           HQ3_PIO_RD(0x45000, ~0x0, actual);
           msg_printf(VRB, "window reg: 0x%x\n", actual);
           return(status);
	}

        msg_printf(DBG, "................Polling for RSS_IDLE\n");
        HQ3_POLL_DMA((1 << 1), (1 << 1), STATUS_ADDR, status); 
        if (status == DMA_TIMED_OUT) {
	   msg_printf(VRB, "DMA Read...\n");
	   msg_printf(VRB, "DMA Timed out polling for HQ RSS_IDLE bit\n");
	   HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
	   HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
   	   return(status);
        }
      }
      else { /* dma burst write */

        HQ3_POLL_DMA (0x0, HQ_BUSY_REIF_DMA_ACTV_BIT, BUSY_DMA_ADDR, status);
        if (status == DMA_TIMED_OUT) {
	   msg_printf(VRB, "DMA Write...\n");
           msg_printf(VRB, "DMA Timed out in reif_dma_actv bit\n");
           HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
           HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
           HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
           msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
           return(status);
        }

        msg_printf(DBG, "................Polling for RSS_IDLE\n");
        HQ3_POLL_DMA((1 << 1), (1 << 1), STATUS_ADDR, status); 
        if (status == DMA_TIMED_OUT) {
	   msg_printf(VRB, "DMA Write...\n");
	   msg_printf(VRB, "DMA Timed out polling for HQ RSS_IDLE bit\n");
	   HQ3_PIO_RD(STATUS_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 status reg: 0x%x\n", actual);
	   HQ3_PIO_RD(BUSY_DMA_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 busy reg: 0x%x\n", actual);
	   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, ~0x0, actual);
	   msg_printf(VRB, "HQ3 Flag set reg: 0x%x\n", actual);
   	   return(status);
        }

      } /* dma burst write */
   } /* dma bursts */


   msg_printf(DBG, "................Polling Done\n");

#if !HQ4
   /* reset the XFR-direction to be default = 0, write */
   xfrctrl.bits.XFR_DMAStart = 0;
   xfrctrl.bits.XFR_PIOStart = 0;
   xfrctrl.bits.XFR_Device   = 0; /* pp */
   xfrctrl.bits.XFR_Direction = 0; /* write */
   WRITE_RSS_REG(0, XFRCONTROL, xfrctrl.val, ~0x0);
#endif

   msg_printf(DBG, "End   _mgras_hq_re_dma_WaitForDone\n");
   return(status);
}


void
_mgras_hq_re_dma_end(void)
{
   CLEAR_REBUS_SYNC();
   STOP_GIO_ARBITRATION();
}

#define DMA_COMPARE(str, addr, exp, rcv, mask)\
        if ((exp & mask)  != (rcv & mask) ) {\
                msg_printf(DBG, "ERROR***: %s failed, exp: %x, rcv: %x, diff: %x\n" ,str, (exp & mask) , (rcv & mask), (exp ^ rcv) & mask );\
                errors++;\
        }

#define DMA_COMPARE_TRAM(str, addr, exp, rcv, mask)\
        if ((exp & mask)  != (rcv & mask) ) {\
                msg_printf(ERR, "ERROR***: %s failed, TRAM-REBus exp: %x, rcv: %x, diff: %x\n" ,str, (exp & mask) , (rcv & mask), (exp ^ rcv) & mask );\
                errors++;\
        }

/*
 * Convert component bit errors to which fb-bit is it (12-bit, 3-comp mode )
 * e.g. green bit 0 == framebuffer bit 33 
 */
void
comp_bits_to_fb_bits(__uint32_t component, __uint32_t err_bits)
{
	__uint32_t i, j, offset;

	switch (component) {
		case 1: offset = 0; break; /* blue */
		case 2: offset = 2; break; /* green */
		case 3: offset = 1; break; /* red */
	}
	msg_printf(DBG, "(Corresponds to Frame Buffer bit(s): ");

	msg_printf(DBG, "component: %d, err_bits: 0x%x, offset: %d\n", component, err_bits, offset);
	for (i = err_bits, j=35; i > 0; i >>= 1, j-=3) {
		if (i & 0x1) 
			msg_printf(DBG, "%d ", (j - offset));
	}
	msg_printf(DBG, ") \n\n");
}


/*
 * Data comparison function
 */
__uint32_t
_mgras_hqpio_redma_compare(RSS_DMA_Params *rssDmaPar, __uint32_t *expected_data, __uint32_t *read_data, __uint32_t db_enable)
{
   __uint32_t nWords, i, y, x, destDevice;
   __uint32_t *wrData, *rdData, bad_tram;
   __uint32_t errors = 0, tram_rev_high, tram_rev_low, led=0;
   __uint32_t oldErrors = 0, tmp, component, half_word_relative;
   __uint32_t drb_ptr, words_per_tileline, mask, diff, half_word_num;
   __uint32_t x_relative, y_relative, high_half_err,low_half_err, first_pix_err;
   __uint32_t tram_words_per_page;
   __uint64_t checksum_simple;
   char comp_str[8];
   static char *led_on[] =  { NULL, "1" };
   static char *led_off[] = { NULL, "0" };

#if DMA_DEBUG
   FILE *fp, *fp2;
#endif

	mask = ~0x0;

	drb_ptr = pp_drb;

	destDevice = rssDmaPar->xfrDevice;

	if ((destDevice == DMA_PP1) && (pp_dma_mode == PP_RGB12))
		mask = 0xfff0fff0;

	if (destDevice == DMA_PP1) {
	   if (pp_dma_mode == PP_RGBA8888) {
   		nWords = (rssDmaPar->numCols * rssDmaPar->numRows);
		words_per_tileline = rssDmaPar->numCols; /* 1 word/pixel */
		}
	   else {
   		nWords = (3*(rssDmaPar->numCols * rssDmaPar->numRows))/2;
		words_per_tileline = (3*rssDmaPar->numCols)/2; /* 1.5 words per pixel */
	   }
	}
	else if (destDevice == DMA_TRAM) {
	   if (db_enable && (numTRAMs == 1)) {
		nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/4;
	   }
	   else {	
		if (numTRAMs == 1) /* 4444 data */
			nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/2;
		else if (numTRAMs == 4)
			nWords = (rssDmaPar->numCols * rssDmaPar->numRows)/2;
	   }
	}
	else if (destDevice == DMA_TRAM_REV) {
   		nWords = 2; /* always read 48 bits */
	}

   	wrData = expected_data;
   	rdData = read_data;

	msg_printf(DBG, "destDevice 0x%x; expected_data 0x%x; read_data 0x%x\n",
		destDevice, expected_data, read_data);

#if DMA_DEBUG
   fp = fopen("data.out", "a+");
   fp2 = fopen("data2.out", "a+");
   fprintf(fp2, "\n");
#endif
   if ((destDevice == DMA_PP1) || (destDevice == DMA_TRAM)) {

	/* for debugging */
	msg_printf(6, "word 0: 0x%x\n", *(rdData));

	/* checksum currently assumes 32-bit rgba data in framebuffer */
	/* Uses the "sum" algorithm */
	if (rssDmaPar->checksum_only == 1) {
		read_checksum = checksum_simple = 0;
		msg_printf(6, "Debug: nWords = 0x%x\n", nWords);

#if (defined(IP22) || defined(IP28))
   		for (i = 0; i < nWords; i++) {
		   if (read_checksum & 0x1) 
				read_checksum = (read_checksum >>1) + 0x8000;
		   read_checksum += (((__uint32_t)*rdData) & 0xffffffff);
		   checksum_simple += (((__uint32_t)*rdData) & 0xffffffff);
		   rdData++;
		}
#else
		read_checksum_even = read_checksum_odd = 0;
		for (y = 0; y < rssDmaPar->numRows; y++) {
		   if (y%2) { /* odd line */
   		   	for (x = 0; x < rssDmaPar->numCols; x++) {
		   	   if (read_checksum_odd & 0x1) 
				read_checksum_odd = (read_checksum_odd >>1) + 
					0x8000;
		   	   read_checksum_odd += 
				(((__uint32_t)*rdData) & 0xffffffff);
		   	   rdData++;
			}
		   }
		   else {	/* even lines */
   		   	for (x = 0; x < rssDmaPar->numCols; x++) {
		   	   if (read_checksum_even & 0x1) 
				read_checksum_even = (read_checksum_even >>1) + 
					0x8000;
		   	   read_checksum_even += 
				(((__uint32_t)*rdData) & 0xffffffff);
		   	   rdData++;
			}
		   }
		}
		msg_printf(6, "Debug: checksum_even = 0x%llx\n", 
			read_checksum_even);
		msg_printf(6, "Debug: checksum_odd = 0x%llx\n", 
			read_checksum_odd);
#endif
		msg_printf(6, "Debug: checksum = 0x%llx\n", read_checksum);
		msg_printf(6, "Debug: simple checksum = 0x%llx\n", checksum_simple);
		return (0);
	}

   	for (i = 0; i < nWords; i++) {
#if DMA_DEBUG
      	   fprintf(fp, "x: %d, y: %d, i: %d, expData = %x ; rdData = %x\n", 
			rssDmaPar->x, rssDmaPar->y, i, *wrData, *rdData);
      	   fprintf(stdout, "x: %d, y: %d, i: %d, expData = %x ; rdData = %x\n", 
			rssDmaPar->x, rssDmaPar->y, i, *wrData, *rdData);
      	   fprintf(fp2, "%x, ", *rdData);
	   j++;
	   if (j == rssDmaPar->numCols) {
   		fprintf(fp2, "\n");
		j = 0;
	   }
#endif
	   if ((i % 0x10000) == 0) {
		led ? setled(2,led_on) : setled(2,led_off);
		led = ~led & 0x1;
  	   }		
      	   DMA_COMPARE("Compare", wrData, (*(wrData)), (*(rdData)), mask);
      	   if (errors != oldErrors) {
		msg_printf(DBG,"Data word # %d of %d, Read buffer addr: 0x%x\n",
			i, nWords-1, rdData);
#if DMA_DEBUG
	      	data = *(wrData);
	      	rss_num = data & 0x3;
	      	pp1_num = (data & 0x4) >> 2;
	      	imp_num = (data & 0x18) >> 3;
		if (destDevice == DMA_PP1) {
	   		fprintf(stderr,"RSS #->%d;PP1 #->%d;IMP #->%d\n",
				rss_num, pp1_num, imp_num);	
		   	fprintf("Screen X-coordinate: %d, Y-coordinate: %d\n",
                                rssDmaPar->x + (i % rssDmaPar->numCols),
				rssDmaPar->y + (i / rssDmaPar->numCols));
		}
		else if (destDevice == DMA_TRAM) {
		   fprintf(stderr, "ERROR***: RSS #%3d; TRAM page #%3d\n",
                        which_rss_for_tram_rd, rssDmaPar->x);
		   fprintf(stderr, "Word #: %d\n", i);
		}
#endif
		if (destDevice == DMA_PP1) {
		   high_half_err = low_half_err = first_pix_err = 0;
		   msg_printf(DBG, "drb_ptr: %d, mode: 0x%x\n",drb_ptr,pp_dma_mode);
	   	   if (pp_dma_mode == PP_RGBA8888) {
			x_relative = i % rssDmaPar->numCols;
			y_relative = i / rssDmaPar->numCols;
		   }
		   else if (pp_dma_mode == PP_RGB12) {
			diff = ((*(wrData)) ^ (*(rdData))) & 0xfff0fff0;	
			if (diff & 0xfff00000) 
				high_half_err = 1;
			if (diff & 0xfff0)
				low_half_err = 1;
			half_word_num = (i * 2) + low_half_err;
			x_relative = (half_word_num/3) % rssDmaPar->numCols;
			y_relative = (half_word_num/3) / rssDmaPar->numCols;

			/* 2016 words, 4032 half words per 1344 pix */
			half_word_relative = 2*(i % words_per_tileline) + 
							low_half_err; 
			msg_printf(DBG, "c: 0x%x, h: 0x%x, x: 0x%x, i: 0x%x\n",
				component, half_word_relative, x_relative, i);
			if (x_relative == 0) {
			   if (i == 0) 
				component = half_word_relative + 1;
			   else
				component = (half_word_relative % (i*2)) + 1;
			}
			else
				component = (half_word_relative % (x_relative * 3)) + 1;
			if (component == 1)
				strcpy(comp_str, "blue");
			else if (component == 2)
				strcpy(comp_str, "green");
			else if (component == 3)
				strcpy(comp_str, "red");
			if (high_half_err && low_half_err && ((i % 3) == 1)) {
			   first_pix_err = 1;

			   /* report the low-half error here */
			   msg_printf(DBG,
			   "12-bits per component, error in %s bits: 0x%x\n",
				   comp_str, (diff & 0xfff0) >> 4);	
			   comp_bits_to_fb_bits(component,(diff & 0xfff0) >> 4);
			}
			else { /* error is in 1 pixel only */
			   if (low_half_err) {
			   	msg_printf(DBG,
			   "12-bits per component, error in %s bits: 0x%x\n",
				   comp_str, (diff & 0xfff0) >> 4);	
			   comp_bits_to_fb_bits(component,(diff & 0xfff0) >> 4);
			   }
			   if (high_half_err) {
				if ((component -1) == 1)
					strcpy(comp_str, "blue");
				else if ((component -1) == 2)
					strcpy(comp_str, "green");
				else if ((component -1) == 3)
					strcpy(comp_str, "red");
				msg_printf(DBG, 
			   "12-bits per component, error in %s bits: 0x%x\n",
			      comp_str, (diff & 0xfff00000) >> 20);
			   comp_bits_to_fb_bits(component-1,
							(diff&0xfff00000)>>20);
			   }
			}
		   }

		   _xy_to_rdram(
			rssDmaPar->x + (x_relative),
			rssDmaPar->y + (y_relative),
			0, 0, 0, *wrData, &tmp, rdData, drb_ptr);

		   /* report the 2nd pixel error if both high/low halves err */
		   if (first_pix_err) {
			if ((component -1) == 1)
				strcpy(comp_str, "blue");
			else if ((component -1) == 2)
				strcpy(comp_str, "green");
			else if ((component -1) == 3)
				strcpy(comp_str, "red");
			msg_printf(DBG, 
			   "The first pixel in this word also had an error.\n");
			msg_printf(DBG, 
			   "12-bits per component, error in %s bits: 0x%x\n",
			      comp_str, (diff & 0xfff00000) >> 20);	
			comp_bits_to_fb_bits(component, (diff&0xfff00000)>>20);

		   	_xy_to_rdram(
				rssDmaPar->x + (x_relative - 1),
				rssDmaPar->y + (y_relative),
				0, 0, 0, *wrData, &tmp, rdData, drb_ptr);
		   }
		}
		else if (destDevice == DMA_TRAM) {
		   msg_printf(ERR, "Error in RSS-%d\n", which_rss_for_tram_rd);
		   if (numTRAMs == 1) {
			tram_words_per_page = 1024; /* 64x32 per page */
		   	/* s, t assumes 16-bit 4444 texels */
	   	   	msg_printf(ERR, "Error in TRAM #0\n");
		   }
		   else if (numTRAMs == 4) {
			/* assumes 4444 4-bit texture data */
			tram_words_per_page = 4096; /* 128x64 page */
			bad_tram = 0;
			diff = ((*(wrData)) ^ (*(rdData)));	
			if (diff & 0xf000f000) 
				bad_tram |= 8;
			if (diff & 0x0f000f00)
				bad_tram |= 4;
			if (diff & 0x00f000f0)
				bad_tram |= 2;
			if (diff & 0x000f000f)
				bad_tram |= 1;
					
			if (bad_tram & 0x1)
				msg_printf(ERR, "Error in TRAM #0 (of 0-3)\n");
			if ((bad_tram & 0x2) && (num_tram_comps > 1))
				msg_printf(ERR, "Error in TRAM #1 (of 0-3)\n");
			if ((bad_tram & 0x4) && (num_tram_comps > 2))
				msg_printf(ERR, "Error in TRAM #2 (of 0-3)\n");
			if ((bad_tram & 0x8) && (num_tram_comps == 4))
				msg_printf(ERR, "Error in TRAM #3 (of 0-3)\n");
		   } /* 4 trams */
	   	   msg_printf(ERR, "TRAM page: %3d; Word: %d, starting s of texel pair:%d, t:%d\n",
			rssDmaPar->x + (i/tram_words_per_page), 
			i % tram_words_per_page,
			((i % tram_words_per_page)%(rssDmaPar->numCols/2))*2,
                       	(i % tram_words_per_page) / (rssDmaPar->numCols/2));
		}
	   	if (global_stop_on_err)
			break; /* only report the 1st error encountered */
	   }
      	   oldErrors = errors;
      	   wrData++; 
      	   rdData++;
      	}
#if DMA_DEBUG
	fclose(fp);
	fclose(fp2);
#endif
   } else {
     /* TRAM_REV */
	for (i = 0; i < nWords; i++) {
		msg_printf(VRB, "TRAM Revision register (word %d): 0x%x\n",
			i, *(rdData));
		if (i == 0) 
			tram_rev_high = *(rdData);
		else
			tram_rev_low = *(rdData);
	rdData++;
	}

	/* TRAM revision is 12-bits but the system returns a 16-bit quantity,
	 * where nibbles 3,2,1 are the 12-bits of the revision register and
	 * nibble 0 is just a replica of nibble 3.
 	 */
	if (numTRAMs == 1) {
		DMA_COMPARE_TRAM("TRAM Revision Low Word", 0, 0x400, tram_rev_low, ~0x0);
		DMA_COMPARE_TRAM("TRAM Revision High Word", 1, 0x0, tram_rev_high, ~0x0);
	}
	else if (numTRAMs == 4) {
		DMA_COMPARE_TRAM("TRAM Revision Low Word",0,0xc000400,tram_rev_low,~0x0);
		DMA_COMPARE_TRAM("TRAM Revision High Word",1, 0x1c011401, tram_rev_high,
			 ~0x0);
	}
   }

   return (errors);
}
/***********************************************************************/

#ifdef MFG_USED
__uint32_t
mg_dmapio_write(void)
{
   RSS_DMA_Params	*rssDmaPar = &mgras_RssDmaParams;
   __uint32_t reWriteMode;
   __uint32_t stop_on_err;
   __uint32_t x, y, rssnum;
   int errors = 0;

   reWriteMode = DMA_PIO;
   stop_on_err = 0;

   PIXEL_TILE_SIZE = 6; /* for rdram */

   rssnum = 4;
   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),rssnum, TWELVEBIT_MASK);

#if 0
   _mgras_rss_init(rssnum);
#endif

   msg_printf(DBG, "Start mg_dmapio_write\n");

   _mgras_hq_re_dma_init();

      rssDmaPar->numRows		= 1;
      rssDmaPar->numCols		= PIXEL_TILE_SIZE;
      rssDmaPar->pixComponents		= 4;
      rssDmaPar->pixComponentSize	= 1;
      rssDmaPar->beginSkipBytes		= 0;
      rssDmaPar->strideSkipBytes	= 0;
      rssDmaPar->xfrDevice		= DMA_PP1;
      rssDmaPar->rss_clrmsk_msb_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsba_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsbb_pp1	= ~0x0;

      for (y = 0; y < 1; y++) {

	rssDmaPar->x = 0; rssDmaPar->y = y;

	_mgras_setupTile(rssDmaPar, DMA_WRTile, PAT_NORM, DMA_RDTile);

	for (x = 0; x < 1; x+= PIXEL_TILE_SIZE) {

		/* DMA a 1-high by PIXEL_TILE_SIZE-wide area */

		rssDmaPar->x = x;

   		errors += _mgras_hqpio_redma_write(rssDmaPar, reWriteMode, DMA_WRTile, 0);
		if (errors && stop_on_err) goto _error;

   	} /* for x */
      } /* for y */

_error:
   _mgras_hq_re_dma_end();

   msg_printf(DBG, "End mg_dmapio_write\n");
   return (errors);
}

__uint32_t
mg_dmapio_read(void)
{
   RSS_DMA_Params	*rssDmaPar = &mgras_RssDmaParams;
   __uint32_t reReadMode;
   __uint32_t stop_on_err;
   __uint32_t x, y, rssnum;
   int errors = 0;

   reReadMode  = DMA_PIO;
   stop_on_err = 0;

   rssnum = 4;
   HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(CONFIG))),rssnum, TWELVEBIT_MASK);

#if 0
   _mgras_rss_init(rssnum);
#endif
   PIXEL_TILE_SIZE = 6; /* for rdram */

   msg_printf(DBG, "Start mg_dmapio_read\n");

   _mgras_hq_re_dma_init();

      rssDmaPar->numRows		= 1;
      rssDmaPar->numCols		= PIXEL_TILE_SIZE;
      rssDmaPar->pixComponents		= 4;
      rssDmaPar->pixComponentSize	= 1;
      rssDmaPar->beginSkipBytes		= 0;
      rssDmaPar->strideSkipBytes	= 0;
      rssDmaPar->xfrDevice		= DMA_PP1;
      rssDmaPar->rss_clrmsk_msb_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsba_pp1	= ~0x0;
      rssDmaPar->rss_clrmsk_lsbb_pp1	= ~0x0;

      for (y = 0; y < 1; y++) {

	rssDmaPar->x = 0; rssDmaPar->y = y;

	_mgras_setupTile(rssDmaPar, DMA_WRTile, PAT_NORM, DMA_RDTile);

	for (x = 0; x < 1; x+= PIXEL_TILE_SIZE) {

		/* DMA a 1-high by PIXEL_TILE_SIZE-wide area */

		rssDmaPar->x = x;

   		errors += _mgras_hqpio_redma_read_compare(rssDmaPar, reReadMode, DMA_RDTile, 0);
		if (errors && stop_on_err) goto _error;
   	} /* for x */
      } /* for y */

_error:
   _mgras_hq_re_dma_end();

   msg_printf(DBG, "End mg_dmapio_read\n");
   return (errors);
}
#endif /* MFG_USED */
