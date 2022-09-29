/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *  Graphics DMA Diagnostics
 */
#include "ide_msg.h"
#include "sys/gr2hw.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "diag_glob.h"
#include "diagcmds.h"
#include "dma.dat"
#include "gr2.h"
#include "dma.h"
#include "libsk.h"
#include "uif.h"

static int gr2_dma_cpu_ge7(int);
extern struct gr2_hw *base;


#define GIO_MYST_ADDR	0x1f06a07c

#define NUMYPIXELS	1024
#define NUMXPIXELS	1280
#define GDMA_BUFOFF	GE_STATUS
#define GDMA_BUFSIZ	NUMXPIXELS
#define GDMA_TESTVAL	0xCAFEBABE
#define GDMA_BADVAL	0xDEADBEEF

#define GFX_TO_HOST	0
#define HOST_TO_GFX	1

#define XLEN 21
#define YLEN 21

#define XINIT		11
#define YINIT		11

extern int GBL_VMAVers;
extern int GBL_VMBVers;
extern int GBL_VMCVers;

#define GR_TYPE int
GR_TYPE buf1[GDMA_BUFSIZ];
GR_TYPE buf2[GDMA_BUFSIZ];
extern char dirstr[][20];



/***************************************************************************
*   Routine: ge7dma
*
*   Purpose: checks DMA between HOST and GE7 shared RAM.
*
*   History: 03/14/91 : Original Version. Vimal Parikh
*
*   Notes:
*
****************************************************************************/
int
gr2_ge7dma()
{
    msg_printf(VRB,"GE7 DMA test ...\n");

    if (gr2_dma_cpu_ge7(HOST_TO_GFX) < 0) 
       return(-1);

    if (gr2_dma_cpu_ge7(GFX_TO_HOST) < 0)
       return(-1);

    return 0;
}


/***************************************************************************
*   Routine: dma_cpu_ge7
*
*   Purpose: checks DMA between HOST and GE7 shared RAM.
*
*   History: 03/14/91 : Original Version. Vimal Parikh
*
*   Notes:
*
****************************************************************************/
static int
gr2_dma_cpu_ge7(int direction)
{
    int errors = 0;
    register int i;
    int tmpi;
    long dadir;
    int addr;
    int found;

    msg_printf(VRB,"%s DMA test ...\n", dirstr[direction]);

    /*
     * Set up test values in microcode data ram
     * (numbers in sequence in source buffer)
     * (known bad values in target buffer)
     */
    addr = GDMA_BUFOFF + MAXCONS;
    for (i = 0; i < GDMA_BUFSIZ; i++) {
	tmpi = (direction == (HOST_TO_GFX)) ? GDMA_BADVAL : i;
	base->shram[addr++] = tmpi;
    }

    /*
     * Set up test values in host buffer
     * (same values as above)
     */
    for (i = 0; i < GDMA_BUFSIZ; i++)
    {
	tmpi = (direction == (HOST_TO_GFX)) ? i : GDMA_BADVAL;
	buf1[i] = tmpi;
    }


    /*
     * Send down the dma test token with 2 arguments: direction and count.
     * This sets up HQ2 with DMA.
     */
     msg_printf(DBG,"About to send graphic command\n");
    base->hq.fin1 = 0;
    base->fifo[DIAG_SHRAMDMA] = direction;
    base->fifo[DIAG_DATA] = GDMA_BUFSIZ;
    base->fifo[DIAG_DATA] = GDMA_BUFOFF + MAXCONS;
    base->hq.fin1 = 0;

    /*
     * Now set up the MC with DMA.
     * Load channel reg with pointer to the decriptor array
     */
    dadir = (direction == (HOST_TO_GFX)) ? VDMA_HTOG : VDMA_GTOH;
    mk_dmada((char *)buf1, (1 << 16) | sizeof(buf1), 
			dadir, (__psunsigned_t)&base->hq.gedma);

    if (do_dma() < 0) return(-1);

    i = 20;
    while (i) {

        if ((base->hq.version & 0x4)== 0) {
            DELAY(1000);
        } else break;

	i--;
    }

    if ( i <= 0 ) {
            msg_printf(DBG, "Waiting for finish flag 1 in %s ...\n",dirstr[direction]);
            msg_printf(ERR, "GE7 not responding.\n");
            return(-1);
    }

    /* now check results */
    addr = GDMA_BUFOFF + MAXCONS;

    for (i = 0; i < GDMA_BUFSIZ; i++)
    {
	found = (direction == (HOST_TO_GFX)) ? base->shram[addr] 
				 : *(int *)(K0_TO_K1(&buf1[i]));

	if (found != i) {
	    msg_printf(ERR,"%s at offset %d.\n", dirstr[direction], addr);
	    msg_printf(ERR, "expected: %#x, actual: %#x\n", i, found);
	    errors--;
	}
	++addr;
    }

    if (errors < 0) {
	sum_error(dirstr[direction]);
    }
    return (errors);
}

/***************************************************************************
*   Routine: shramre3dma
*
*   Purpose: checks DMA between Shared RAM and RE3.
*
*   History: 03/14/91 : Original Version. Vimal Parikh
*
*   Notes: Do re3dma test before this test. 
*
****************************************************************************/

int
gr2_shramre3dma()
{
    register	i, addr;
    register	x, y;
    int 	pixmask, count;
    int 	x1,y1,x2,y2;
    int 	errors = 0;
    unsigned long actual, expected;

    msg_printf(VRB,"SHRAM to bitplanes DMA test ...\n");

    /* check bit-plane configuration first */
    gr2_boardvers();

    if (GBL_VMAVers != 3) pixmask = 0xff;
    if (GBL_VMBVers != 3) pixmask |= 0xff00;
    if (GBL_VMCVers != 3) pixmask |= 0xff0000;


    x1 = 100;
    y1 = 100;
    x2 = 131;
    y2 = 131;
    count = (x2 - x1 + 1) * (y2 - y1 + 1);

    /*******************************
     *  DMA from shared RAM to RE3 *
     ******************************/
    /* fill the source shared data ram with test values */
    addr = GE_STATUS + MAXCONS;
    for(i=1; i<=count; i++) 
	base->shram[addr++] = i;

    /*
     * Send down the dma test token with 2 arguments: direction and count.
     * This sets up HQ2 with DMA.
     */

    base->hq.fin1 = 0;
    base->fifo[DIAG_SHRAMREDMA] = HOST_TO_GFX;
    base->fifo[DIAG_DATA] = x1;
    base->fifo[DIAG_DATA] = y1;
    base->fifo[DIAG_DATA] = x2;
    base->fifo[DIAG_DATA] = y2;


    i = 100;
    while (i) {
        if ((base->hq.version & 0x4)== 0) {
            DELAY(500);
        } else break;

        i--;
    }
    if ( i <= 0 ) {
        msg_printf(DBG, "Waiting for finish flag 1 in shram ---> re3 .....\n");
        msg_printf(ERR, "GE7 not responding.\n");
        msg_printf(DBG,"hq_version_reg = 0X%x.\n",base->hq.version);
        return(-1);
    }


    base->hq.fin1 = 0;

    /* now check the results by reading data directly from RE3 to HOST */
    if (pix_dma(base, VDMA_GTOH, x1, y1, x2, y2, buf1) < 0)
	return(-1);
	
    i = 0;
    for(y=y1; y<=y2; y++) {
	for(x=x1; x<=x2; x++) {	
	    if ((buf1[i] & pixmask) != ((i+1) & pixmask)) {
		msg_printf(ERR,"RE3 to HOST DMA at (%d,%d).\n", x, y);
		msg_printf(ERR, "expected: %#x, actual: %#x\n", (i+1)&pixmask,
							    buf1[i]&pixmask);
		errors--;
	    }
	    i++;
	}
    }

    if (errors < 0) return(errors);
		
    /*******************************
     *  DMA from RE3 to SHRAM to  *
     ******************************/
    /* clear the shared ram target area */
    addr = GE_STATUS + MAXCONS;
    for(i=1; i<=count; i++) 
	base->shram[addr++] = GDMA_BADVAL;

    /*
     * Send down the dma test token.
     * This sets up HQ2 with DMA.
     */

    base->hq.fin1 = 0;

    base->fifo[DIAG_SHRAMREDMA] = GFX_TO_HOST;
    base->fifo[DIAG_DATA] = x1;
    base->fifo[DIAG_DATA] = y1;
    base->fifo[DIAG_DATA] = x2;
    base->fifo[DIAG_DATA] = y2;

    i = 20;
    while (i) {
       if ((base->hq.version & 0x4)== 0) {
          DELAY(1000);
       } else break;

       i--;
    }

    if ( i <= 0 ) {
       msg_printf(DBG, "Waiting for finish flag 1 in re3 ---> shram .....\n");
       msg_printf(ERR, "GE7 not responding.\n");
       msg_printf(DBG,"hq_version_reg = 0X%x.\n",base->hq.version);
       return(-1);
    }

    base->hq.fin1 = 0;


    /* now check the results by reading data directly from shram */
    i = 0;
    addr = GE_STATUS + MAXCONS;
    for(y=y1; y<=y2; y++) {
	for(x=x1; x<=x2; x++) {	
	    expected = (i+1) & pixmask;
	    actual = base->shram[addr] & pixmask;
	    if (expected != actual) {
		msg_printf(ERR,"RE3 to SHRAM DMA at (%d,%d).\n", x, y);
		msg_printf(ERR, "expected: %#x, actual: %#x\n", expected,
							    actual);
		errors--;
	    }
	    i++;
	    addr++;
	}
    }



    /* Fill buf1 with test values */
    for(i=1; i<=count; i++)
       buf1[i- 1] = i;

    /* DMA write from HOST to RE */
    base->hq.fin1 = 0;
    if (pix_dma(base, VDMA_HTOG, x1, y1, x2, y2, buf1) < 0)
       return(-1);


    /* Fill in buf1 with bad values */
    for(i=1; i<=count; i++)
       buf1[i- 1] = GDMA_BADVAL;

    /* now check the results by reading data directly from RE3 to HOST*/
    base->hq.fin1 = 0;
    if (pix_dma(base, VDMA_GTOH, x1, y1, x2, y2, buf1) < 0)
       return(-1);

    i = 0;

    for(y=y1; y<=y2; y++) {
       for(x=x1; x<=x2; x++) {
          if ((buf1[i]& pixmask) != ((i+1) & pixmask)) {
             msg_printf(ERR,"RE3 to HOST DMA at (%d,%d).\n", x, y);
             msg_printf(ERR, "expected: %#x, actual: %#x\n", (i+1) & pixmask, buf1[i] & pixmask);
             errors--;
          }
          i++;
       }
    }


    return(errors);
}

    
/***************************************************************************
*   Routine: re3dma
*
*   Purpose: checks DMA between HOST and RE3.
*
*   History: 03/14/91 : Original Version. Vimal Parikh
*
*   Notes:
*
****************************************************************************/

int
gr2_re3dma()
{
    register 	testval;
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask;
    int 	errors = 0;
    volatile int mystery;
    unsigned int actual, expected;

    /* check bit-plane configuration first */
    gr2_boardvers();

    if (GBL_VMAVers != 3) pixmask = 0xff;
    if (GBL_VMBVers != 3) pixmask |= 0xff00;
    if (GBL_VMCVers != 3) pixmask |= 0xff0000;


     msg_printf(VRB,"CPU to bitplanes DMA test ...\n");
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
	    buf1[i] = (testval | (testval << 9)| (testval << 17)) & pixmask ;
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
	    expected = (testval | (testval << 9)| (testval << 17)) & pixmask ;

	    if (actual != expected) {
                mystery = *(volatile uint *)PHYS_TO_K1(GIO_MYST_ADDR);
		msg_printf(ERR,"CPU to RE3 DMA at (%d,%d).\n", i, y);
		msg_printf(ERR, "expected: %#x, actual: %#x\n", expected, 
								actual);
		errors--;
	    }

	    testval++;
	}

	if ( !(y % 100) )
	    busy(1);
    }
    busy(0);

    return errors;
}


/***************************************************************************
*   Routine: pix_dma_zb
*
*   Purpose: Writes pixel data to z-buffer.
*
*   History: 03/14/91 : Original Version. Vimal Parikh
*
*   Notes: dir == VDMA_HTOG or VDMA_GTOH
*
****************************************************************************/

int
pix_dma_zb(int dir, int x1, int y1, int x2, int y2, GR_TYPE *buf)
{
    int	count;
    int	direction;

    /* send command to GE7 */
    /****
    msg_printf(DBG,"Making descriptor array for CPU to bitplanes transfer.\n");
    ****/
    direction = (dir == (VDMA_HTOG)) ? HOST_TO_GFX : GFX_TO_HOST;

    count = (x2 - x1 + 1) * (int)sizeof(GR_TYPE) |  ((y2 - y1 + 1) << 16);

    mk_dmada((char *)buf, count, dir, (__psunsigned_t)&base->hq.gedma);

    /* Send down the dma CPU/Bitplanes test token */
    base->hq.fin1 = 0;
    base->fifo[DIAG_REDMA_ZB] = direction;
    base->fifo[DIAG_DATA] = x1;
    base->fifo[DIAG_DATA] = y1;
    base->fifo[DIAG_DATA] = x2;
    base->fifo[DIAG_DATA] = y2;

    if (do_dma() < 0) return(-1);
    DELAY(9000);
    if ((base->hq.version & 0x4) == 0) {
        msg_printf(DBG, "Waiting for finish flag 1 in %s ...\n",dirstr[direction]);
	msg_printf(ERR, "GE7 not responding.\n");
	return(-1);
    }
    base->hq.fin1 = 0;
    return(0);
}

int
gr2_stridedma()
{
    register	i, j, addr;
    int 	count;
    int 	x1,y1,x2,y2,x0,y0;
    int 	x1s,y1s,x2s,y2s;


    x1 = 10;
    y1 = 10;
    x2 = x1 + XLEN - 1;
    y2 = y1 + YLEN - 1;
    count = XLEN * YLEN;

    x1s = 100;
    y1s = 100;
    x0 = 200;
    y0 = 200;

    /* fill the source shared data ram with test values */
    addr = GE_STATUS + MAXCONS;
    for(i=0; i<count; i++)
	base->shram[addr++] = buf3[i];

    base->hq.fin1 = 0;

    base->fifo[DIAG_SHRAMREDMA] = HOST_TO_GFX;
    base->fifo[DIAG_DATA] = x1;
    base->fifo[DIAG_DATA] = y1;
    base->fifo[DIAG_DATA] = x2;
    base->fifo[DIAG_DATA] = y2;

    i = 200;
    while (i) {
       if ((base->hq.version & 0x4)== 0) {
          DELAY(1000);
       } else break;

       i--;
    }

    if ( i <= 0 ) {
       msg_printf(DBG, "Waiting for finish flag 1 in shram ---> re3 .....\n");
       msg_printf(ERR, "GE7 not responding.\n");
       msg_printf(DBG,"hq_version_reg = 0X%x.\n",base->hq.version);
       return(-1);
    }


    for (i =01; i < 6; i++) {

       base->hq.fin1 = 0;

       x2s = x0 + (i * 175);
       y2s = y0 + (i * 150);

       /*
       ** setup dmaregs
       */
       addr = MAXCONS + DMAREGS;

       base->shram[addr++] = MAXCONS + ((YLEN - YINIT) * XLEN) + XLEN - XINIT;
       base->shram[addr++] = x2s - x1s;
       base->shram[addr++] = XINIT - 1;
       base->shram[addr++] = MAXCONS + ((YLEN - YINIT) * XLEN); 
       base->shram[addr++] = XLEN - 1;
       base->shram[addr++] = y2s - y1s;
       base->shram[addr++] = YINIT - 1;
       base->shram[addr++] = MAXCONS + XLEN - XINIT;
       base->shram[addr++] = YLEN - 1;
       base->shram[addr++] = MAXCONS;

       /***********
       base->shram[addr++] = MAXCONS;
       base->shram[addr++] = x2s - x1s;
       base->shram[addr++] = XLEN - 1;
       base->shram[addr++] = MAXCONS;
       base->shram[addr++] = XLEN - 1;
       base->shram[addr++] = y2s - y1s;
       base->shram[addr++] = YLEN -1;
       base->shram[addr++] = MAXCONS;
       base->shram[addr++] = YLEN - 1;
       base->shram[addr++] = MAXCONS;
       ***********/
					   

       /*
       *  STRIDE_DMA tokens
       */
       base->fifo[DIAG_STRIDEDMA] = 0;
       base->fifo[DIAG_DATA] = x1s;
       base->fifo[DIAG_DATA] = y1s;
       base->fifo[DIAG_DATA] = x2s - x1s + 1;
       base->fifo[DIAG_DATA] = y2s - y1s + 1;
       
       j = 100;
       while (j) {
          if ((base->hq.version & 0x4)== 0) {
             DELAY(1000);
          } else break;

          j--;
       }

       if ( j <= 0 ) {
          msg_printf(DBG, "Waiting for finish flag 1 in STRIDE_DMA.....\n");
          msg_printf(ERR, "GE7 not responding.\n");
          msg_printf(DBG,"hq_version_reg = 0X%x.\n",base->hq.version);
          return(-1);
       }


    }

    return(0);



}
    


static unsigned short attr[] = {
	0x1122, 0x3344,
	0x5566, 0x7788,
	0x9900, 0x9988,
	0x7766, 0x5544,
	0x3322, 0x1100,
	0xabcd, 0xbcde,
	0xcdef, 0xfedc,
	0xedcb, 0xdcba
};

static unsigned short cmds[] = {
	0xf1, 0xe2,
	0xd3, 0xc4,
	0xb5, DIAG_RDVTX
};

static unsigned short vector = DIAG_RDVTX;

int
gr2_ctxsw()
{
    register	i, addr;
    int 	found;
    int		err=0;


    msg_printf(VRB,"CTX_SWITCH DMA test ...\n");

    /* init finish flag to 4 */
    base->hq.fin1 = 4;


    /* set shram loc to dump context */
    base->shram[MAXCONS + CTXSHRAM] = MAXCONS + CTXDUMP;

    /* Set context attributes */
    base->fifo[DIAG_SETCTX] = 0;

    /* sending attr for ctx 0 */
    base->fifo[DIAG_DATA] = attr[0];
    base->fifo[DIAG_DATA] = attr[1];
    base->fifo[DIAG_DATA] = attr[2];
    base->fifo[DIAG_DATA] = attr[3];
    base->fifo[DIAG_DATA] = attr[4];
    base->fifo[DIAG_DATA] = attr[5];
    base->fifo[DIAG_DATA] = attr[6];
    base->fifo[DIAG_DATA] = attr[7];
    base->fifo[DIAG_DATA] = attr[8];
    base->fifo[DIAG_DATA] = attr[9];
    base->fifo[DIAG_DATA] = attr[10];
    base->fifo[DIAG_DATA] = attr[11];
    base->fifo[DIAG_DATA] = attr[12];
    base->fifo[DIAG_DATA] = attr[13];
    base->fifo[DIAG_DATA] = attr[14];
    base->fifo[DIAG_DATA] = attr[15];

    /* sending cmdstore for ctx 0 */
    base->fifo[cmds[0]] = cmds[0];
    base->fifo[cmds[1]] = cmds[1];
    base->fifo[cmds[2]] = cmds[2];
    base->fifo[cmds[3]] = cmds[3];
    base->fifo[cmds[4]] = cmds[4];
    base->fifo[cmds[5]] = cmds[5];

    /* vector of ctx 0  = RDVTX */
    base->fifo[vector] = vector;		/* end of setctx parameters */

    /* pars for rdvtx cmd (V2I asserts USEV) */
    base->fifo[DIAG_V2I]= 0x777;
    base->fifo[DIAG_CTXSW]= 4;
    base->fifo[DIAG_V2I] = 0x456;

    /* ctx switching during fetch + back to back ctxsw */
    base->fifo[DIAG_CTXSW]= 4;

    DELAY(900000);

    addr = MAXCONS + CTXDUMP;

    for (i = 0; i < 16; i++) {
	if (base->shram[addr++] != attr[i]) { 
        	msg_printf(ERR,"Attr_store[%d] of CTX0 = 0X%x. expected = 0x%x\n",i, base->shram[addr++],attr[i]);
		err++;
	}
    }

    for (i = 0; i < 6; i++) {
	if (base->shram[addr++] != cmds[i]) {
        	msg_printf(ERR,"Cmds_store[%d] of CTX0 = 0X%x. expected = 0x%x\n",i, base->shram[addr++],cmds[i]);
		err++;
	}
    }
    if ( (found = base->shram[addr++]) != 0x360037) {
      	msg_printf(ERR,"Cmdreg    of CTX0 = 0X%x. expected = 0x%x\n",(found & 0x1ff), 0x37);
     	 msg_printf(ERR,"Vecreg    of CTX0 = 0X%x. expected = 0x%x\n",((found >> 16) & 0x1ff), 0x36);
      err++;
    }

    if ( (found = base->shram[addr++]) != 0x3f800000) {
    	msg_printf(ERR,"Datareg   of CTX0 = 0X%x. expected = 0x%x\n",found, 0x3f800000);
       	err++;
    }

    if ( (found = base->shram[addr++]) != 0xbeef) {
      	msg_printf(ERR,"Act_attr  of CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xbeef);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0xbabe) {
      	msg_printf(ERR,"Cur_attr  of CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xbabe);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0xdade) {
      	msg_printf(ERR,"flag_reg  of CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xdade);
      	err++;
    }


    if ( ((found = base->shram[addr++]) & 0xffffff00)  != (0x2505ff & 0xffffff00)) {
      	msg_printf(ERR,"hq2pc_reg of CTX0 = 0X%x. expected = 0x%x\n",(found & 0x1fff), 0x5ff);
      	msg_printf(ERR,"cmdptr    of CTX0 = 0X%x. expected = 0x%x\n",((found >> 16) & 0x7), 0x5);
      	msg_printf(ERR,"vtxctr    of CTX0 = 0X%x. expected = 0x%x\n",((found >> 20) & 0x7), 0x2);
      	msg_printf(ERR,"fifostate of CTX0 = 0X%x. expected = 0x%x\n",((found >> 24) & 0x3), 0x0);
      	err++;
     }

    if ( (found = base->shram[addr++]) != 0x202000) {
      	msg_printf(ERR,"ucode_reg of CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffffff), 0x202000);
      	msg_printf(ERR,"finishflg of CTX0 = 0X%x. expected = 0x%x\n",((found >> 24) & 0x3), 0x0);
      	err++;
     }

    /* Read back the restored context AFTER context switching */
    addr = MAXCONS + CTXDUMP + 256;

    for (i = 0; i < 16; i++) {
	if(base->shram[addr++] != attr[i]) { 
        	msg_printf(ERR,"Attr_store[%d] of CTX0 = 0X%x. expected = 0x%x\n",i, base->shram[addr++],attr[i]);
		err++;
	}
    }

    for (i = 0; i < 6; i++) {
	if (base->shram[addr++] != cmds[i]) {
        	msg_printf(ERR,"Cmds_store[%d] of Restored CTX0 = 0X%x. expected = 0x%x\n",i, base->shram[addr++],cmds[i]);
		err++;
	}
    }

    if ( (found = base->shram[addr++]) != 0x360037) {
      	msg_printf(ERR,"Cmdreg    of Restored CTX0 = 0X%x. expected = 0x%x\n",(found & 0x1ff), 0x37);
      	msg_printf(ERR,"Vecreg    of Restored CTX0 = 0X%x. expected = 0x%x\n",((found >> 16) & 0x1ff), 0x36);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0x44eee000) {
    	msg_printf(ERR,"Datareg   of Restored CTX0 = 0X%x. expected = 0x%x\n",found, 0x44eee000);
       	err++;
    }

    if ( (found = base->shram[addr++]) != 0xbeef) {
      	msg_printf(ERR,"Act_attr  of Restored CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xbeef);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0xbabe) {
      	msg_printf(ERR,"Cur_attr  of Restored CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xbabe);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0xdade) {
      	msg_printf(ERR,"flag_reg  of Restored CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffff), 0xdade);
      	err++;
    }


    if ( ((found = base->shram[addr++])) & (0xffffff00 != 0x12505ec & 0xffffff00)) {
      
      	msg_printf(ERR,"vtxctr    of Restored CTX0 = 0X%x. expected = 0x%x\n",((found >> 20) & 0x7), 0x2);
      	msg_printf(ERR,"fifostate of Restored CTX0 = 0X%x. expected = 0x%x\n",((found >> 24) & 0x3), 0x0);
      	err++;
    }

    if ( (found = base->shram[addr++]) != 0x600000) {
      	msg_printf(ERR,"ucode_reg of Restored CTX0 = 0X%x. expected = 0x%x\n",(found & 0xffffff), 0x600000);
      	msg_printf(ERR,"finishflg of Restored CTX0 = 0X%x. expected = 0x%x\n",((found >> 24) & 0x3), 0x0);
      	err++;
    }


    /* grep vertex */
    addr = MAXCONS + DIAG_XLOC;

    if ((found = base->shram[addr++]) != 0x777) {
	msg_printf(ERR," 1ST vertexes: X_VTX = 0X%x. expected= 0x%x\n",found, 0x777);
      	err++;
    }
    if ((found = base->shram[addr++]) != 0x456) {
      	msg_printf(ERR," 1ST vertexes: Y_VTX = 0X%x. expected= 0x%x\n",found, 0x456);
      	err++;
    }

    if ((found = base->shram[addr++]) != 0) {
      	msg_printf(ERR," 1ST vertexes: Z_VTX = 0X%x. expected= 0x%x\n",found, 0);
      	err++;
    }
    if ((found = base->shram[addr++]) != 1) {
      	msg_printf(ERR," 1ST vertexes: W_VTX = 0X%x. expected= 0x%x\n",found, 1);
      	err++;
    }



    /* pars for rdvtx cmd */
    base->fifo[DIAG_V2I]= 0x888;
    base->fifo[DIAG_V2I]= 0x999;
    time_delay(500);

    /* grep vertex */
    addr = MAXCONS + DIAG_XLOC;
    if ((found = base->shram[addr++]) != 0x888) {
    	msg_printf(ERR," 2ND vertexes: X_VTX = 0X%x. expected= 0x%x\n",found, 0x888);
      	err++;
      }
    if ((found = base->shram[addr++]) != 0x999) {
      	msg_printf(ERR," 2ND vertexes: Y_VTX = 0X%x. expected= 0x%x\n",found, 0x999);
      	err++;
      }

    if ((found = base->shram[addr++]) != 0) {
      	msg_printf(ERR," 2ND vertexes: Z_VTX = 0X%x. expected= 0x%x\n",found, 0);
      	err++;
      }
    if ((found = base->shram[addr++]) != 1) {
      	msg_printf(ERR," 1ST vertexes: W_VTX = 0X%x. expected= 0x%x\n",found, 1);
      	err++;
      }
  return(err);
}
