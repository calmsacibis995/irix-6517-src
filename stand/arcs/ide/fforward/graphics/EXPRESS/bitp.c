/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Bitplanes test
 */

#define GR2_BITP	1

#include "ide_msg.h"
#include "sys/gr2hw.h"
#include "sys/cpu.h"
#include "sys/types.h"
#include "sys/sbd.h"
#include "diag_glob.h"
#include "diagcmds.h"
#include "gr2_loc.h"
#include "gr3_loc.h"
#include "vb2_loc.h"
#include "ru1_loc.h"
#include "gu1_loc.h"
#include "gr2.h"
#include "dma.h"
#include "uif.h"
#include "libsc.h"

/* #define MFG_USED 1 */

#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define NUMXPIXELS	(XMAXSCREEN+1)
#define NUMYPIXELS	(YMAXSCREEN+1)
#define NUMTESTS	9
#define ALLMASK	0x01ff

/* RE3 READSRC bits selection */
#define BITPLANE	0
#define AUXPLANE	1
#define ZPLANE		2
#define CIDPLANE	3

extern struct gr2_hw *base;

int totalerrs = 0;
static void usage_err(char *);

#define GFX_TO_HOST 	0
#define HOST_TO_GFX	1

extern int GBL_VMAVers;
extern int GBL_VMBVers;
extern int GBL_VMCVers;
extern int GBL_GRVers;
extern int GBL_ZbufferVers;

static int gr2_chkpixels(unsigned int);
static int gr2_chkoffset(void);
static int gr2_chkauxoffset(void);
static int gr2_chkauxpixels(unsigned int);
static int gr2_chkcidpixels(unsigned int);
static int gr2_chkzplanes(int, int, int, int, unsigned int);
static int gr2_chkzoffset(void);
static void report_aux_loc(int, int, unsigned int, unsigned int);
static void gr2_reportz_loc(int, int, unsigned int, unsigned int);
static void report_pix_loc(int, int, unsigned int, unsigned int);
static void report_cid_loc(int, int, unsigned int, unsigned int);

 /*
 * Test of 8 or 24 bitplanes
 */
int
gr2_bitp(int argc, char **argv)
{
    register int errs;
    totalerrs = 0;

    if (argc > 2)
    {
	usage_err(argv[0]);
	return -1;
    }
    gr2_boardvers();
    if (GBL_VMAVers == 3 & GBL_VMBVers == 3 & GBL_VMCVers == 3) {
      msg_printf(ERR,"VM2 boards not installed\n");
      return(-1);
      }

    if (argc == 2)
    {
	switch (*argv[1])
	{
	    case '?':
		printf("Bitplane Tests:\n");
		printf("    0 - Test bitplanes with 0x0000000\n");
		printf("    1 - Test bitplanes with 0xfffffff\n");
		printf("    2 - Test bitplanes with 0x5a5a5a5a\n");
		printf("    3 - Test bitplanes with 0xa5a5a5a5\n");
		printf("    4 - Bitplane incremental data test\n");
		printf("    a - Test auxiliary planes option only\n");
		printf("    c - Test cidplane only\n");
		printf("    z - Test z-buffer only\n");
		return 0;
	    case '0':
		msg_printf(VRB,"Bitplane test - writing 0x0 ...\n");
		totalerrs = gr2_chkpixels(0L);
		break;
	    case '1':
		msg_printf(VRB,"Bitplane test - writing 0xff ...\n");
		totalerrs = gr2_chkpixels(0xfffffff);
		break;
	    case '2':
		msg_printf(VRB,"Bitplane test - writing 0x5a ...\n");
		totalerrs = gr2_chkpixels(0x5a5a5a5a);
		break;
	    case '3':
		msg_printf(VRB,"Bitplane test - writing 0xa5 ...\n");
		totalerrs = gr2_chkpixels(0xa5a5a5a5);
		break;
	    case '4':
		msg_printf(VRB,"Bitplane test - writing incremental data...\n");
		totalerrs = gr2_chkoffset();
		break;
	    case 'a':
		    msg_printf(VRB,"Auxiliary plane test - writing incremental data.....\n");
		    errs = gr2_chkauxoffset();

		    msg_printf(VRB,"Auxiliary plane test - writing 0x5a ...\n");
		    errs = gr2_chkauxpixels(0x5a);
		    totalerrs += errs;
		    msg_printf(VRB,"Auxiliary plane test - writing 0xa5 ...\n");
		    errs = gr2_chkauxpixels(0xa5);
		    totalerrs += errs;
		break;
	    case 'c':
		    msg_printf(VRB,"Cidplane test - writing 0x5 ...\n");
		    errs = gr2_chkcidpixels(0x55555555);
		    totalerrs += errs;
		    msg_printf(VRB,"Cidplane test - writing 0xa ...\n");
		    errs = gr2_chkcidpixels(0xaaaaaaaa);
		    totalerrs += errs;
		break;
	    case 'z':
		msg_printf(VRB,"Z-buffer test ...\n");

		msg_printf(VRB,"Z-buffer test - writing 0x00 ...\n");
		errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0x000000);
		totalerrs += errs;
		msg_printf(VRB,"Z-buffer test - writing 0xff ...\n");
		errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xffffff);
		totalerrs += errs;
		msg_printf(VRB,"Z-buffer test - writing 0x5a ...\n");
		errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0x5a5a5a5a);
		totalerrs += errs;
		msg_printf(VRB,"Z-buffer test - writing 0xa5 ...\n");
		errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xa5a5a5a5);
		totalerrs += errs;
		msg_printf(VRB,"Z-buffer test - writing incremental data...\n");
		errs = gr2_chkzoffset();
		totalerrs += errs;
		break;
	    default:
		usage_err(argv[0]);
		return -1;
	}
    }
    else
    {
	msg_printf(VRB,"Bitplane test - writing incremental data ...\n");
	errs =gr2_chkoffset();
	totalerrs += errs;

	msg_printf(VRB,"Bitplane test - writing 0xff ...\n");
	errs =gr2_chkpixels(0xffffffff);
	totalerrs += errs;

	msg_printf(VRB,"Bitplane test - writing 0x5a ...\n");
	errs =gr2_chkpixels(0x5a5a5a5a);
	totalerrs += errs;

	msg_printf(VRB,"Bitplane test - writing 0xa5 ...\n");
	errs =gr2_chkpixels(0xa5a5a5a5);
	totalerrs += errs;

	msg_printf(VRB,"Bitplane test - writing 0x00 ...\n");
	errs = gr2_chkpixels(0L);
	totalerrs += errs;

	msg_printf(VRB,"Cidplane test - writing 0x5 ...\n");
        errs = gr2_chkcidpixels(0x55555555);
        totalerrs += errs;

	msg_printf(VRB,"Cidplane test - writing 0xa ...\n");
        errs = gr2_chkcidpixels(0xaaaaaaaa);
        totalerrs += errs;

	msg_printf(VRB,"Auxiliary plane test - writing incremental data.....\n");
	errs = gr2_chkauxoffset();

	msg_printf(VRB,"Auxiliary plane test - writing 0x5a ...\n");
        errs = gr2_chkauxpixels(0x5a);
        totalerrs += errs;

	msg_printf(VRB,"Auxiliary plane test - writing 0xa5 ...\n");
        errs = gr2_chkauxpixels(0xa5);
        totalerrs += errs;
    }

    if (totalerrs)
    {
	    sum_error("bitplanes");

	return -1;
    }
    else
    {
	okydoky();
	return 0;
    }
}

/*
 * Test  z-buffer
 */
int
gr2_zb(int argc, char **argv)
{
    register int errs;

    totalerrs = 0;
    if (argc > 2)
    {
	printf("usage: %s [0|1|2|3|4|?]\n", *argv);
	return -1;
    }
    gr2_boardvers();
    if ( 	((GBL_GRVers < 4) && (GBL_ZbufferVers == 3)) || 
		((GBL_GRVers >=4) && (GBL_ZbufferVers == 0))
	) {
      msg_printf(VRB,"Z-buffer board not installed\n");
      return(0);
      }
    if (argc == 2)
    {
	switch (*argv[1])
	{
	    case '?':
		printf("z_buffer Tests:\n");
		printf("    0 - Test z-buffer with 0x0000000\n");
		printf("    1 - Test z-buffer with 0xfffffff\n");
		printf("    2 - Test z-buffer with 0x5555555\n");
		printf("    3 - Test z-buffer with 0xaaaaaaa\n");
		printf("    4 - z-buffer incremental data test\n");
	    case '0':
		msg_printf(VRB,"z-buffer test - writing 0x00 ...\n");
		totalerrs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0);
		break;
	    case '1':
		msg_printf(VRB,"z-buffer test - writing 0xff ...\n");
		totalerrs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xffffff);
		break;
	    case '2':
		msg_printf(VRB,"z-buffer test - writing 0x55...\n");
		totalerrs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0x555555);
		break;
	    case '3':
		msg_printf(VRB,"z-buffer test - writing 0xaa...\n");
		totalerrs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xaaaaaa);
		break;
	    case '4':
		msg_printf(VRB,"z-buffer test - writing incremental data...\n");
		totalerrs = gr2_chkzoffset();
		break;
	    default:
      		msg_printf(ERR,"Z-buffer board not installed\n");
		return -1;
	}
    }
    else
    {
	msg_printf(VRB,"z-buffer test - writing incremental data...\n");
	errs =gr2_chkzoffset();
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0x00 ...\n");
	errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0x55 ...\n");
	errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0x555555);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0xaa ...\n");
	errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xaaaaaa);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0xff ...\n");
	errs = gr2_chkzplanes(0,0,XMAXSCREEN,YMAXSCREEN,0xffffff);
	totalerrs += errs;

    }

    if (totalerrs)
    {
	    sum_error("z-buffer");

	return -1;
    }
    else
    {
	okydoky();
	return 0;
    }
}


int
gr2_zb_small(int argc, char **argv)
{
    register int errs;

    totalerrs = 0;
    if (argc > 2)
    {
	printf("usage: %s [0|1|2|3|4|?]\n", *argv);
	return -1;
    }
    gr2_boardvers();
    if ( 	((GBL_GRVers < 4) && (GBL_ZbufferVers == 3)) || 
		((GBL_GRVers >=4) && (GBL_ZbufferVers == 0))
	) 
      {
      	msg_printf(VRB,"Z-buffer board not installed\n");
      	return(0);
      }
    if (argc == 2)
    {
	switch (*argv[1])
	{
	    case '?':
		printf("z_buffer Tests:\n");
		printf("    0 - Test z-buffer with 0x0000000\n");
		printf("    1 - Test z-buffer with 0xfffffff\n");
		printf("    2 - Test z-buffer with 0x5555555\n");
		printf("    3 - Test z-buffer with 0xaaaaaaa\n");
		printf("    4 - z-buffer incremental data test\n");
	    case '0':
		msg_printf(VRB,"z-buffer test - writing 0x00 ...\n");
		totalerrs = gr2_chkzplanes(0,0,10,3,0);
		break;
	    case '1':
		msg_printf(VRB,"z-buffer test - writing 0xff ...\n");
		totalerrs = gr2_chkzplanes(0,0,10,3,0xffffff);
		break;
	    case '2':
		msg_printf(VRB,"z-buffer test - writing 0x55...\n");
		totalerrs = gr2_chkzplanes(0,0,10,3,0x555555);
		break;
	    case '3':
		msg_printf(VRB,"z-buffer test - writing 0xaa...\n");
		totalerrs = gr2_chkzplanes(0,0,10,3,0xaaaaaa);
		break;
	    case '4':
		msg_printf(VRB,"z-buffer test - writing incremental data...\n");
		totalerrs = gr2_chkzoffset();
		break;
	    default:
      		msg_printf(ERR,"Z-buffer board not installed\n");
		return -1;
	}
    }
    else
    {
	msg_printf(VRB,"z-buffer test - writing incremental data...\n");
	errs =gr2_chkzoffset();
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0x00 ...\n");
	errs = gr2_chkzplanes(0,0,10,3,0);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0x55 ...\n");
	errs = gr2_chkzplanes(0,0,10,3,0x555555);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0xaa ...\n");
	errs = gr2_chkzplanes(0,0,10,3,0xaaaaaa);
	totalerrs += errs;

	msg_printf(VRB,"z-buffer test - writing 0xff ...\n");
	errs = gr2_chkzplanes(0,0,10,3,0xffffff);
	totalerrs += errs;

    }

    if (totalerrs)
    {
	    sum_error("z-buffer");

	return -1;
    }
    else
    {
	okydoky();
	return 0;
    }
}


static void
usage_err(char *func_name)
{
	printf("usage: %s [0|1|2|3|4|z|c|a|?]\n", func_name);
}


/*
 * Write the specified values to full pixel planes.
 * Verify all pixel locations.  
 */
int 
gr2_chkpixels(unsigned int value)
{
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask = 0;
    int 	errors = 0;
    unsigned int actual, expected;

    if (GBL_VMAVers != 3) pixmask = 0xff;
    if (GBL_VMBVers != 3) pixmask |= 0xff00;
    if (GBL_VMCVers != 3) pixmask |= 0xff0000;

    /*************************
     *  Write to bitplanes   *
     *************************/
    count = NUMXPIXELS;
    x = 0;
    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	for (i = 0; i < count; i++)
	{
	    buf1[i] = value & pixmask;
	}

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    /*************************
     *  Read from bitplanes  *
     *************************/
    count = NUMXPIXELS;
    x = 0;

    base->fifo[DIAG_READSOURCE] = BITPLANE; /* make sure reading from bit-plane */
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);
	/* check for test values in host buffer */

	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask;
	    expected = value & pixmask;

	    if (actual != expected) {
		report_pix_loc(i,y,actual,expected);
		errors++;
	    }

	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    if (errors) {
	sum_error("CPU/bitplanes test"); 
    }

    return errors;
}


/*
 * Write the specified values to the z-buffer planes.
 * Verify all pixel locations
 */
int
gr2_chkzplanes(int x1, int y1, int x2, int y2, unsigned int value)
{
    int i,j, count, errors = 0;
    unsigned int  actual, expected;

    msg_printf(DBG,"Z-buffer test: write and read 0x%x\n",value);
    count = (x2 - x1 + 1);
    for (i=0; i < count; i++) {
      buf1[i] = value; }
    msg_printf(DBG,"DMA write to z-buffer\n");

    base->fifo[DIAG_ZDRAW] = 1;
    for (i=y1; i < y2 ; i++) {
      if (pix_dma_zb(VDMA_HTOG, x1, i, x2, i, buf1) < 0)
       return(-1);
      if (!(i%100)) busy(1);
       }
     busy(0);
    base->fifo[DIAG_ZDRAW] = 0;
  /* read back and check value */

    for (i=0; i < count; i++) {
      buf2[i] = 0; }
    msg_printf(DBG,"DMA read from z-buffer\n");
    base->fifo[DIAG_READSOURCE] = ZPLANE;
    for (i=y1; i < y2; i++) {
      if (pix_dma_zb(VDMA_GTOH, x1, i, x2, i, buf2) < 0)
      return(-1); 
      for (j = x1; j < count; j++) {
        actual = *(int *)(K0_TO_K1(&buf2[j])); 
         expected = value ;

         if (actual != expected) {
	    gr2_reportz_loc(j,i,actual,expected);
	     errors++;
	    }
      }
      if (!(i % 100)) busy(1);
     }
     busy(0);
    base->fifo[DIAG_READSOURCE] = BITPLANE;
    return errors;
}


/*
 * Write the specified values to auxiliary  pixel planes.
 * Verify all pixel locations.  
 */
int
gr2_chkauxpixels(unsigned int value)
{
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask = 0xf;
    int 	errors = 0;
    unsigned int actual, expected;

    msg_printf(DBG,"CPU to auxiliary planes DMA test ...\n");


    /***********************************
     *  Write to auxiliary bitplanes   *
     ***********************************/
    base->fifo[DIAG_AUXPLANE] = 1; /* make sure writing to auxiliary-plane */
    count = NUMXPIXELS;
    x = 0;
    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	for (i = 0; i < count; i++)
	{
	    buf1[i] = value & pixmask;
	}

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;
	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_AUXPLANE] = 0; /* reset to bit-plane */
    /***********************************
     *  Read from auxiliary bitplanes  *
     ***********************************/
    count = NUMXPIXELS;
    x = 0;

    base->fifo[DIAG_READSOURCE] = AUXPLANE; /* make sure reading from bit-plane */
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);

	/* check for test values in host buffer */

	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask;
	    expected = value & pixmask;

	    if (actual != expected) {
		report_aux_loc(i,y,actual,expected);
		errors++;
	    }

	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_READSOURCE] = BITPLANE; /* make sure reading from bit-plane */
    if (errors) {
	sum_error("CPU/auxiliary planes test"); 
    }

    return errors;
}


/*
 * Write the specified values to cid planes.
 * Verify all pixel locations.  
 */
int
gr2_chkcidpixels(unsigned int value)
{
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask = 0xf;
    int 	errors = 0;
    unsigned int actual, expected;

    msg_printf(DBG,"CPU to cid planes DMA test ...\n");


    /***********************************
     *  Write to cid bitplanes   *
     ***********************************/
    base->fifo[DIAG_CIDPLANE] = (value & 0xf00) | 0xf000; /* make sure writing to  cid-plane */
    count = NUMXPIXELS;
    x = 0;
    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	for (i = 0; i < count; i++)
	{
	    buf1[i] = value & pixmask;
    	    base->fifo[DIAG_CIDPLANE] = (value & 0xf00) | 0xf000; /* make sure writing to  cid-plane */
	}

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_CIDPLANE] = 0;  /* change back to bit-plane */
    /***********************************
     *  Read from cid bitplanes  *
     ***********************************/
    count = NUMXPIXELS;
    x = 0;

    base->fifo[DIAG_READSOURCE] = CIDPLANE; /* make sure reading from bit-plane */
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);
	/* check for test values in host buffer */

	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask ;
	    expected = value  & pixmask;
	    if (actual != expected) {
		report_cid_loc(i,y,actual,expected);
		errors++;
	    }

	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_READSOURCE] = BITPLANE; /* make sure reading from bit-plane */
    if (errors) {
	sum_error("CPU/cid planes test"); 
    }

    return errors;
}
/*
 Write incremental data to pixel planes and verify 
 */
int
gr2_chkoffset(void)
{
    register 	testval;
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask = 0;
    int 	errors = 0;
    unsigned int actual, expected;

    if (GBL_VMAVers != 3) pixmask = 0xff;
    if (GBL_VMBVers != 3) pixmask |= 0xff00;
    if (GBL_VMCVers != 3) pixmask |= 0xff0000;
    /*************************
     *  Write to bitplanes   *
     *************************/
    count = NUMXPIXELS;
    x = 0;

    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	testval = y * count;
	for (i = 0; i < count; i++)
	{
	    buf1[i] = testval | (testval << 8) | (testval << 16) & pixmask;
	    testval++;
	}

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;
	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    /*************************
     *  Read from bitplanes  *
     *************************/
    count = NUMXPIXELS;
    x = 0;
    base->fifo[DIAG_READSOURCE] = BITPLANE; /* make sure reading from bit-plane */
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);
	/* check for test values in host buffer */
	testval = y * count;
	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask;
	    expected = (testval | (testval << 8) | (testval << 16)) & pixmask;

	    if (actual != expected) {
		report_pix_loc(i,y,actual,expected);
		errors++;
	    }

	    testval++;
	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    if (errors) {
	sum_error("CPU/bitplanes Test"); 
    }

    return errors;
}

/*
 * View the z-buffer as a bunch of 4K pages and write the page offset into
 * the bitplanes
 */
int
gr2_chkzoffset(void)
{
    register 	testval;
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask;
    int 	errors = 0;
    unsigned int actual, expected;


    /*************************
     *  Write to z-buffer    *
     *************************/
    count = NUMXPIXELS;
    x = 0;
    base->fifo[DIAG_ZDRAW] = 1;
    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	testval = y * count;
	for (i = 0; i < count; i++)
	{
	    buf1[i] = testval | (testval << 8) | (testval << 16);
	    testval++;
	}

        if (pix_dma_zb(VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;
	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_ZDRAW] = 0;

    /*************************
     *  Read from z_buffer   *
     *************************/
    count = NUMXPIXELS;
    x = 0;

    base->fifo[DIAG_READSOURCE] = ZPLANE;
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma_zb(VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);
	/* check for test values in host buffer */
	testval = y * count;
	pixmask = 0xffffff;

	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask;
	    expected = (testval | (testval << 8) | (testval << 16)) & pixmask;

	    if (actual != expected) {
		gr2_reportz_loc(i,y,actual,expected);
		errors++;
	    }

	    testval++;
	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    base->fifo[DIAG_READSOURCE] = BITPLANE;
    if (errors) {
	sum_error("CPU/z-buffer DMA"); 
    }

    return errors;
}

/*
 Write incremental data to auxiliary planes and verify 
 */
int
gr2_chkauxoffset(void)
{
    register 	testval;
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask = 0xf;
    int 	errors = 0;
    unsigned int actual, expected;

    /*************************
     *  Write to auxplanes   *
     *************************/
    base->fifo[DIAG_AUXPLANE] = 1; /* make sure writing to auxiliary-plane */
    count = NUMXPIXELS;
    x = 0;

    for (y=0; y<NUMYPIXELS; y++)
    {
	/* Set up test values in host buffer */
	for (i = 0, testval = 0; i < count; i++, testval++)
	    buf1[i] = testval & pixmask;

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf1) < 0)
	    return(-1) ;
	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    /*************************
     *  Read from bitplanes  *
     *************************/
    count = NUMXPIXELS;
    x = 0;
    base->fifo[DIAG_READSOURCE] = AUXPLANE; /* make sure reading from bit-plane */
    for (y=0; y<NUMYPIXELS; y++)
    {
	for (i = 0; i < count; i++) {
	    buf2[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf2) < 0) 
	    return(-1);
	/* check for test values in host buffer */
	testval = 0;
	for (i = 0; i < count; i++) {
	    actual = *(int *)(K0_TO_K1(&buf2[i])) & pixmask;
	    expected = testval & pixmask;

	    if (actual != expected) {
		report_aux_loc(i,y,actual,expected);
		errors++;
	    }

	    testval++;
	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    if (errors) {
	sum_error("CPU/auxiliary planes Test"); 
    }

    return errors;
}


/*
 * Report that an error occured in the pixel bitplanes
 */
void
report_pix_loc(int x,int y, unsigned int pix, unsigned int value)
{
	register unsigned bank, slot;

	/* figure out what video chip is bad */

#if IP22
	unsigned int row, col;
	row = (y >> 1) & 0xfffffffc | ( ( ( x / 5 ) >> 6) & 0x0003);
        col = ( (y & 0x0001) << 8 ) | ( ( y & 0x0006) << 5) |
          ( ( x / 5) & 0x003f);
#endif
        bank = (x % 5) & 0x00007;
	slot = 0;

        msg_printf(ERR,"Error in bitplanes at pixel (%4d,%4d)\n    expected: %#lx, actual: %#lx\n",
   	       x, y, value, pix);

	switch (GBL_GRVers) {
		case 0:
		case 1:
		case 2:
		case 3:
		/* GR2 board */
		if ( (value&0x0f) != (pix&0x0f) ) 	/* low nibble */
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2]); 

		if ( (value&0xf0) != (pix&0xf0) )  /* high nibble */
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2+1]); 
		if ( (value&0xf00) != (pix&0xf00) ) {
	  		slot = 1;
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2]); 
		}
		if ( (value&0xf000) != (pix&0xf000) ) {
	  		slot = 1;
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2+1]); 
		}
		if ( (value&0xf0000) != (pix&0xf0000) ) {
	  		slot = 2;
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2]); 
		}
		if ( (value&0xf00000) != (pix&0xf00000) ) {
	  		slot = 2;
 	  		print_VM2_loc(slot,VM2_VRAM_U[bank*2+1]); 
		}
		break;

		case 4:
		case 9:
		case 10:
		case 11:

	/* GR3 board */
#if IP22
		if (is_indyelan()) {
		   if ( (value&0xff) != (pix&0xff) ) 	  /* bit 0 - 7 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank]); 
		   if ( (value&0xff00) != (pix&0xff00) )  /* bit 8 - 15 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank+5]); 
		   if ( (value&0xff0000) != (pix&0xff0000) ) /* bit 15 - 23 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank+10]); 
		}
		else
#endif
		{
		   if ( (value&0xff) != (pix&0xff) ) 	  /* bit 0 - 7 */
 	  		print_VB2_loc(VB2_VRAM_U[bank]); 
		   if ( (value&0xff00) != (pix&0xff00) )   /* bit 8 - 15 */
 	  		print_VB2_loc(VB2_VRAM_U[bank+5]); 
		   if ( (value&0xff0000) != (pix&0xff0000) ) /* bit 15 - 23 */
 	  		print_VB2_loc(VB2_VRAM_U[bank+10]); 
		}
		break;

		case 5:
		case 6:
		case 7:
		case 8:
		default:

	      if ( (y % 2)) {
	      /* Ultra board even line go to VB2 */
#if IP22
		if(is_indyelan()) {
		   if ( (value&0xff) != (pix&0xff) ) 	  /* bit 0 - 7 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank]); 
		   if ( (value&0xff00) != (pix&0xff00) )   /* bit 8 - 15 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank+5]); 
		   if ( (value&0xff0000) != (pix&0xff0000) ) /* bit 15 - 23 */
 	  		print_VB2_loc(VB2_VRAM_U_INDY[bank+10]); 
		}
		else
#endif
		{
		   if ( (value&0xff) != (pix&0xff) ) 	  /* bit 0 - 7 */
 	  		print_VB2_loc(VB2_VRAM_U[bank]); 
		   if ( (value&0xff00) != (pix&0xff00) )   /* bit 8 - 15 */
 	  		print_VB2_loc(VB2_VRAM_U[bank+5]); 
		   if ( (value&0xff0000) != (pix&0xff0000) ) /* bit 15 - 23 */
 	  		print_VB2_loc(VB2_VRAM_U[bank+10]); 
			}
	}
	else {
		if ( (value&0xff) != (pix&0xff) ) 	  /* bit 0 - 7 */
 	  		print_RU1_loc(RU1_VRAM_U[bank]); 
		if ( (value&0xff00) != (pix&0xff00) ) 	  /* bit 8 - 15 */
 	  		print_RU1_loc(RU1_VRAM_U[bank+5]); 
		if ( (value&0xff0000) != (pix&0xff0000) ) /* bit 15 - 23 */
 	  		print_RU1_loc(RU1_VRAM_U[bank+10]); 
	}
	 break;
	}
}

/*
 * Report that an error occured in the auxiliary planes
 * "value" is what the pixel should be
 * "pix" is what pixel really is
 */
void
report_aux_loc(int x, int y, unsigned int pix, unsigned int value)
{
	register unsigned bank;

        msg_printf(ERR,"Error in auxliary planes at pixel (%4d,%4d)\n    expected: %#lx, actual: %#lx\n",
   	       x, y, value, pix);

	/* figure out what video chip is bad */

        bank = (x % 5) & 0x00007; 
	switch (GBL_GRVers) {
		case 0:
		case 1:
		case 2:
		case 3:
			print_GR2_loc(AUX_VRAM_U[bank]);
			break;
		case 4:
		case 9:
		case 10:
		case 11:
#if IP22
			if(is_indyelan())
				print_VB2_loc(VB2_AUX_VRAM_U_INDY[bank]); 
	    		else
#endif
				print_VB2_loc(VB2_AUX_VRAM_U[bank]); 
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		default:
			if ( (y % 2)) {
#if IP22
				if(is_indyelan())
				    print_VB2_loc(VB2_AUX_VRAM_U_INDY[bank]); 
				else
#endif
				    print_VB2_loc(VB2_AUX_VRAM_U[bank]); 
			}
	    		else 
		   	    print_RU1_loc(RU1_AUX_VRAM_U[bank]); 
			break;
	}

}  

/*
 * Report that an error occured in the auxiliary planes
 * "value" is what the pixel should be
 * "pix" is what pixel really is
 */
void
report_cid_loc(int x, int y, unsigned int pix, unsigned int value)
{
	register unsigned bank;

        msg_printf(ERR,"Error in cid planes at pixel (%4d,%4d)\n    expected: %#lx, actual: %#lx\n",
   	       x, y, value, pix);

	/* figure out what video chip is bad */

/*        bank = ((x % 5) & 0x7)  + ((x/5) & 0x1) * 5 ; */
        bank = ((x % 5) & 0x7);
	switch (GBL_GRVers) {
		case 0:
		case 1:
		case 2:
		case 3:
 	  		print_GR2_loc(CID_DRAM_U[bank]); 
			break;
		case 4:
		case 9:
		case 10:
		case 11:
#if IP22
			if(is_indyelan())
				print_VB2_loc(VB2_CID_VRAM_U_INDY[bank]); 
			else
#endif
				print_VB2_loc(VB2_CID_VRAM_U[bank]); 
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		default:
	    		if ( (y % 2)) {
#if IP22
				if(is_indyelan())
				    print_VB2_loc(VB2_CID_VRAM_U_INDY[bank]); 
				else
#endif
				    print_VB2_loc(VB2_CID_VRAM_U[bank]); 
	    		}
	    		else 
				print_RU1_loc(RU1_CID_VRAM_U[bank]); 
			break;
	}
}

/*
 * Report that an error occured in the wid or z-buffer bitplanes
 */
void
gr2_reportz_loc(int x,int y, unsigned int zactual, unsigned int zexpected)
{
    unsigned int row, col, bank, dram_select, cold5;


        msg_printf(ERR,"Error in z-buffer at pixel (%4d,%4d)\n    expected: %#lx, actual: %#lx\n",
   	       x, y, zexpected, zactual);

	/* figure out which dram is bad */

	row = (y & 0x1f8) << 2 | ( ( ( cold5 = x / 5 ) >> 6) & 0x3);
        col = ( (y & 0x1) << 7 ) | ( ( y & 0x6) << 4) |
          (  cold5  & 0x3e) >> 1;
        bank = ((x % 5) & 0x7)  + (cold5 & 0x1) * 5 ;
	dram_select = (y & 0x100) >> 8;
   msg_printf(DBG,"row= 0x%x, colum = 0x%x, bank = 0x%x, dram_select= 0x%x\n",row, col,bank,dram_select);

	switch (GBL_GRVers) {
		case 0:
		case 1:
		case 2:
		case 3:
		if ( (zactual & 0xffff) != (zexpected & 0xffff) ) 	/* low 16-bit */
 	  		print_ZB4_loc(ZB4_DRAM_U[bank*4+dram_select*2]); 


		if ( (zactual & 0xff0000) != (zexpected & 0xff0000) ) 	/* high 16-bit */
 	 		 print_ZB4_loc(ZB4_DRAM_U[bank*4+dram_select*2+1]); 
		break;

		case 4:
		case 9:
		case 10:
		case 11:
		if ( (zactual & 0xffff) != (zexpected & 0xffff) ) {	/* low 16-bit */
#if IP22
		   if(is_indyelan())
 	  		print_GR3_loc(GR3_ZB4_DRAM_U_INDY[bank*2]); 
		   else
#endif
 	  		print_GR3_loc(GR3_ZB4_DRAM_U[bank*2]); 
		}
		if ( (zactual & 0xff0000) != (zexpected & 0xff0000) ) { 	/* high 16-bit */
#if IP22
		   if(is_indyelan())
 	 		 print_GR3_loc(GR3_ZB4_DRAM_U_INDY[bank*2+1]); 
		   else
#endif
 	 		 print_GR3_loc(GR3_ZB4_DRAM_U[bank*2+1]); 
		}
		break;
		case 5:
		case 6:
		case 7:
		case 8:
		default:
	    	if ( !(y % 2)) { 
			/* low 16-bit */
			if ( (zactual & 0xffff) != (zexpected & 0xffff) ) 	
 	  			print_GU1_loc(GU1_ZB4_DRAM_U[bank*2]); 


			/* high 16-bit */
			if ( (zactual & 0xff0000) != (zexpected & 0xff0000) ) 	
 	 	 		print_GU1_loc(GU1_ZB4_DRAM_U[bank*2+1]); 
	     	}
		else {
			/* low 16-bit */
			if ( (zactual & 0xffff) != (zexpected & 0xffff) ) 	
 	  			print_RU1_loc(RU1_ZB4_DRAM_U[bank*2]); 

 			/* high 16-bit */
			if ( (zactual & 0xff0000) != (zexpected & 0xff0000) )
 	 		 	print_RU1_loc(RU1_ZB4_DRAM_U[bank*2+1]); 
	     	}
		break;
	}
}


/*
 *  gr2_bptest - bitplane (pixels) read/write tool
 */
/*ARGSUSED*/
int
gr2_bptest(int argc, char **argv)
{
#ifdef MFG_USED 

#define BOTH  0
#define READ 1 
#define WRITE 2
    int x1, x2, y1, y2, option;
    int value;
    int i,j, errors,count;
    unsigned int  actual, expected;
    if (argc < 5)
    {
	printf("usage: %s x1, y1, x2, y2, value (r/w)\n", argv[0]);
	return -1;
    }
    else
    {
	atob(argv[1], &x1);
	atob(argv[3], &x2);
	atob(argv[2], &y1);
	atob(argv[4], &y2);
    }
    if (argc > 5)
	atob(argv[5], &value);
    else
	value = 0;
    if (argc > 6)
       atob(argv[6], &option);
    else option = BOTH;
    count = (x2 - x1 + 1);
    for (i=0; i < count; i++) {
      buf1[i] = value; }
    errors = 0;
    if (option == BOTH | option == WRITE ) {
      msg_printf(DBG,"Bit-plane write data 0x%x\n",value);
      for (i=y1; i <= y2 ; i++) {
        if (pix_dma(base, VDMA_HTOG, x1, i, x2, i, buf1) < 0) {
	  msg_printf(DBG,"bptest DMA write failed...\n");
          return(-1);
	}
        if (!(i%100)) busy(1);
       }
     busy(0);
    }
  /* read back and check value */

    if (option == BOTH | option == READ) {
      for (i=0; i < count; i++) {
        buf2[i] = 0; }
      msg_printf(DBG,"Bit-plane read data 0x%x\n",value);
      for (i=y1; i <= y2; i++) {
        if (pix_dma(base, VDMA_GTOH, x1, i, x2, i, buf2) < 0) {
          msg_printf(DBG,"bptest DMA read failed...\n");
          return(-1); 
	}

  /* compare data */
      if (option == BOTH | option == READ) {
        msg_printf(DBG,"Bit-plane compare data 0x%x\n",value);
        for (j = 0; j < count; j++) {
          actual = *(int *)(K0_TO_K1(&buf2[j])); 
          expected = value ;
	  msg_printf(DBG,"pixel(%d,%d),		 expected = 0x%x, actual = 0x%x\n",
	   j+x1,i,expected,actual); 

          if (actual != expected) {
	    msg_printf(DBG,"pixel(%d,%d),	 expected = 0x%x, actual = 0x%x\n",
	     x1+j,i,expected,actual); 
             report_pix_loc(j+x1,i,actual,expected);
	     errors++;

	    }
         }
	}
      }
      if (!(i % 100)) busy(1);
     }
     busy(0);
  if (errors) msg_printf(VRB,"bptest failed....\n");
  else 
    msg_printf(VRB,"bptest test completed....\n");
  return errors;
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif /* MFG_USED */
}

/*
 *  gr2_zbtest - z-buffer (pixels) read tool
 */
/*ARGSUSED*/
int
gr2_zbtest(int argc, char **argv)
{
#ifdef MFG_USED
    int x1, x2, y1, y2;
    int  errors;
    int value;
    if (GBL_ZbufferVers == 3 ) {
      msg_printf(VRB,"Z-buffer board not installed\n");
      return(0);
      }
    if (argc < 5)
    {
	printf("usage: %s x1, y1, x2, y2, value\n", argv[0]);
	return -1;
    }
    else
    {
	atob(argv[1], &x1);
	atob(argv[3], &x2);
	atob(argv[2], &y1);
	atob(argv[4], &y2);
    }
    if (argc > 5)
	atob(argv[5], &value);
    else
	value = 0;
    errors = gr2_chkzplanes(x1,y1,x2,y2,value);
    if (errors > 0) msg_printf(VRB,"Z-buffer test failed\n");
    else msg_printf(VRB,"Z-buffer test passed\n");
    return errors;
#else /* MFG_USED */
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);  
return 0;
#endif
}
