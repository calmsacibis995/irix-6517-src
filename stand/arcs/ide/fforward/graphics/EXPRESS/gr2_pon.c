/**************************************************************************
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
 * gr2 power-on graphics test
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/gr2_if.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "dma.h"
#include "gr2.h"
#include "uif.h"
#include <libsc.h>


/*ARGSUSED1*/
static int
chk_Dac_regs(
	register struct gr2_hw  *base,  /* board base address in K1 seg */
	register struct gr2_bt457_dac *dac) /* DAC base addr in K1 seg */
{
	register int i,j, errs=0;
	unsigned int tmp;

	/* Read READMASK REG. data and save it */
	dac->addr = GR2_DAC_READMASK;
	tmp = dac->cmd2;
	for (i = 0; i<= 0xff; i += 0x55) {
		dac->addr = GR2_DAC_READMASK;
		dac->cmd2 = i;	
		dac->addr = GR2_DAC_READMASK;
		if (dac->cmd2 != i)  
			errs++;
	}
	/* Restore READMASK data */
	dac->addr = GR2_DAC_READMASK;
	dac->cmd2 = tmp;

	/* Read BLINKMASK REG. data and save it */
	dac->addr = GR2_DAC_BLINKMASK;
	tmp = dac->cmd2;
	for (i = 0; i<= 0xff; i += 0x55) {
		dac->addr = GR2_DAC_BLINKMASK;
		dac->cmd2 = i;	
		dac->addr = GR2_DAC_BLINKMASK;
		if (dac->cmd2 != i)  
			errs++;
	}
	/* Restore BLINKMASK data */
	dac->addr = GR2_DAC_BLINKMASK;
	dac->cmd2 = tmp;


	/* Read CMD REGISTER data and save it */
	dac->addr = GR2_DAC_CMD;
	tmp = dac->cmd2;
	for (i = 0; i<= 0xff; i += 0x55) {
		dac->addr = GR2_DAC_CMD;
		dac->cmd2 = i;	
		dac->addr = GR2_DAC_CMD;
		if (dac->cmd2 != i)  
			errs++;
	}
	/* Restore CMD REG. data */
	dac->addr = GR2_DAC_CMD;
	dac->cmd2 = tmp;

	/* Read TEST REGISTER (4-bit only) data and save it */
	dac->addr = GR2_DAC_TEST;
	tmp = dac->cmd2;
	for (i = 0; i<= 0xf; i += 0x5) {
		dac->addr = GR2_DAC_TEST;
		dac->cmd2 = i;	
		dac->addr = GR2_DAC_TEST;
		if (dac->cmd2 != i )  
			errs++;
	}
	/* Restore TEST REG. data */
	dac->addr = GR2_DAC_TEST;
	dac->cmd2 = tmp;



	/* check Identity gamma ramp */

	for (i=0; i < GR2_DAC_NGAMMAENT; i++) {
		dac->addr = i;
		tmp = dac->paltram; /*save data */
		for (j = 0; j <= 0xff; j += 0x55) {
			/* write data */
			dac->addr = i;
		 	dac->paltram = j;
			/* read back and compare */
			dac->addr = i;
	  		if (dac->paltram != j)
				errs++;
		}

		dac->addr = i;
		dac->paltram = tmp; /* restore initialization data */
	}

	/* check overlay color palette initialization */
	for (i=0; i < GR2_DAC_NOVLMAPENT; i++) {
		dac->addr = i;
		tmp = dac->ovrlay; /*save data */
		for (j = 0; j <= 0xff; j += 0x55) {
			dac->addr = i;
		 	dac->ovrlay = j;

			dac->addr = i;
	  		if (dac->ovrlay != j)
				errs++;
		}

		dac->addr = i;
		dac->ovrlay = tmp; /* restore data */
	}

	return errs;
}

chk_dac(
	register struct gr2_hw  *base)  /* board base address in K1 seg */
{
	int errs;

	errs = 0;

	msg_printf(DBG,"GR2: Dac test ...\n");

	if (chk_Dac_regs(base,&base->reddac) !=0) 
		errs++;

	if (chk_Dac_regs(base,&base->grndac) !=0) 
		errs++;

	if (chk_Dac_regs(base,&base->bluedac) !=0) 
		errs++;

	if (errs == 0) msg_printf(VRB,"GR2: Dac test completed with no errors\n");
	else msg_printf(VRB,"GR2: Dac test failed\n");

	return errs;
}
#define GFX_TO_HOST	0
#define HOST_TO_GFX	1

#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define NUMXPIXELS	(XMAXSCREEN+1)
#define NUMYPIXELS	(YMAXSCREEN+1)
#define NPIXELS		NUMXPIXELS/5

long
chk_dma(
register struct gr2_hw  *base  /* board base address in K1 seg */)
{
    register 	i;
    int 	count;
    int 	x,y;
    int 	pixmask;
    int 	errors = 0;
    unsigned int actual, expected;
    int *buf;

    msg_printf(DBG,"GR2:  CPU to bitplanes DMA test ...\n");

    /* allocate dma buffer */
    buf = (int *)dmabuf_malloc(NPIXELS*sizeof(int));
    pixmask = 0xff;

    /*************************
     *  Write to bitplanes   *
     *************************/
    count = NPIXELS;
    x = 0;

    for (y=0; y<NUMYPIXELS; y+=NUMYPIXELS-1)
    {
	/* Set up test values in host buffer */
	for (i = 0; i < count; i++)
	    buf[i] = i & pixmask;

        if (pix_dma(base, VDMA_HTOG, x, y, x+count-1, y, buf) < 0)
	    return(-1) ;
    }

    /*************************
     *  Read from bitplanes  *
     *************************/
    x = 0;
    for (y=0; y<NUMYPIXELS; y+=NUMYPIXELS-1)
    {
	for (i = 0; i < count; i++) {
	    buf[i] = 0;
	}

	if (pix_dma(base, VDMA_GTOH, x, y, x+count-1, y, buf) < 0) 
	    return(-1);
	/* check for test values in host buffer */
	for (i = 0; i < count; i++) {
	    actual = *(int *)(&buf[i]) & pixmask;
	    expected = i & pixmask;

	    if (actual != expected)  
		errors++;
	}

    }
    dmabuf_free(buf); /* free dma buffer */
    if (errors) 
	msg_printf(VRB,"GR2: CPU/bitplanes DMA test failed...\n"); 
    else msg_printf(VRB,"GR2: CPU/bitplanes DMA test completed with no errors\n");

    return(errors);
}

#if IP20
#define VRSTAT	(volatile unchar *)PHYS_TO_K1(VRSTAT_ADDR)
#elif IP22 || IP26 || IP28
#define VRSTAT	(volatile u_int *)PHYS_TO_K1(HPC3_EXT_IO_ADDR)
#define VRSTAT_MASK	EXTIO_SG_STAT_0
#define VRSTAT_MASK_S1	EXTIO_S0_STAT_0
#endif

static unsigned char initcolors[8][3] = {

	/* R     G     B */
	0x00, 0x00, 0x00,               /* BLACK */
	0xff, 0x00, 0x00,               /* RED */
	0x00, 0xff, 0x00,               /* GREEN */
	0xff, 0xff, 0x00,               /* YELLOW */
	0x00, 0x00, 0xff,               /* BLUE */
	0xff, 0x00, 0xff,               /* MAGENTA */
	0x00, 0xff, 0xff,               /* CYAN */
	0xff, 0xff, 0xff,               /* WHITE */
};
unsigned char rdbuf[8][5];
chk_xmap_regs(
register struct gr2_hw  *base  /* board base address in K1 seg */)
{
	int i, errs = 0;
	char tmp1,tmp2,tmp3,tmp4,tmp5;
	int data;

#ifdef VRSTAT_MASK_S1
	/* need to figure out slot # from base address */
	unsigned int vrstat_mask;

	if (base == (struct gr2_hw *)PHYS_TO_K1(GR2_BASE1_PHYS))
		vrstat_mask = VRSTAT_MASK_S1;		/* slot 1 */
	else
		vrstat_mask = VRSTAT_MASK;		/* slot 0 */
#else
#define vrstat_mask VRSTAT_MASK
#endif

	msg_printf(DBG,"GR2:  XMAP test ...\n");

	/* Wait for XMAP external FIFO to be empty */
	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY);

	while (1)
		if (!(*VRSTAT & vrstat_mask))
			break;
	/* Wait until the vertical blanking starts, then update the XMAP5 */
	while (1)
        	if (*VRSTAT & vrstat_mask)
			break;

	/* check xmap address line */

	for (i=0; i< 0x4 ; i++) {

		data = i * 0x55;
		/* write to addr line */
		base->xmapall.addrlo = data;
		base->xmapall.addrhi = data & 0x1f;

		/* read into buffer */
		rdbuf[0][i] = base->xmap[0].addrlo;
		rdbuf[1][i] = base->xmap[0].addrhi;
	}

	/* compare data */

	for (i=0; i< 0x4; i++) {
		data = i * 0x55;
		if (rdbuf[0][i] != data) 
			errs++;
		if (rdbuf[1][i] != (data & 0x1f)) 
			errs++;
	}		

	/*
	 * Check mode registers
	 */
	
	/* save the original mode reg. contenr */
	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;
	tmp1 = base->xmap[0].mode; /* save init. data */

	/* Wait for XMAP external FIFO to be empty */
	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY);

	while (1)
		if (!(*VRSTAT & vrstat_mask))
			break;
	/* Wait until the vertical blanking starts, then update the XMAP5 */
	while (1)
        	if (*VRSTAT & vrstat_mask)
			break;

	/* write pattern and read into buffer*/
	for (i = 0; i < 4; i++) {

		/* write patterns */
		base->xmapall.addrlo = 0;
	        base->xmapall.addrhi = 0;
		base->xmapall.mode = (i * 0x55);

		/* read into buffer */
		base->xmapall.addrlo = 0;
	        base->xmapall.addrhi = 0;
		rdbuf[0][i] = base->xmap[0].mode; 
	}

	/* restore data */
	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;
	base->xmapall.mode = tmp1; 

	/* check data */
	for (i = 0; i < 4; i++) 
		if (rdbuf[0][i] != i * 0x55) 
			errs++;

	/*
	 * Check misc. register
	 */

	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;

	/* Wait for XMAP external FIFO to be empty */
	while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY);

	while (1)
		if (!(*VRSTAT & vrstat_mask))
			break;
	/* Wait until the vertical blanking starts, then update the XMAP5 */
	while (1)
        	if (*VRSTAT & vrstat_mask)
			break;

	/* save init. data */
	tmp1= base->xmap[0].misc;
	tmp2= base->xmap[0].misc;
	tmp3= base->xmap[0].misc;
	tmp4= base->xmap[0].misc;
	tmp5= base->xmap[0].misc;

	for (i = 0; i < 4; i++) {

		/* write pattern */

		data = i * 0x55;
		base->xmapall.addrlo = 0;
		base->xmapall.addrhi = 0;
		base->xmapall.misc = data;
		base->xmapall.misc = data;
		base->xmapall.misc = data;
		base->xmapall.misc = data & 0xf;
		base->xmapall.misc = data & 0x1f;

		/* Wait for XMAP external FIFO to be empty */
		while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY);
		while (1)
			if (!(*VRSTAT & vrstat_mask))
				break;
		/* Wait until the vertical blanking starts, then update the XMAP5 */
		while (1)
        		if (*VRSTAT & vrstat_mask)
				break;

		/* read into buffer */

		base->xmapall.addrlo = 0;
		base->xmapall.addrhi = 0;

		rdbuf[i][0] = base->xmap[0].misc;
		rdbuf[i][1] = base->xmap[0].misc;
		rdbuf[i][2] = base->xmap[0].misc;
		rdbuf[i][3] = base->xmap[0].misc;
		rdbuf[i][4] = base->xmap[0].misc;

	}

	/* restore data */

	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;
	base->xmapall.misc = tmp1;
	base->xmapall.misc = tmp2;
	base->xmapall.misc = tmp3;
	base->xmapall.misc = tmp4;
	base->xmapall.misc = tmp5;


	/* verify data */
	for (i = 0; i < 4; i++) {
		data = i * 0x55;
		if (rdbuf[i][0] != data) 
			errs++;

		if (rdbuf[i][1] != data) 
			errs++;

		if (rdbuf[i][2] != data) 
			errs++;
		if (rdbuf[i][3] != (data &0xf)) 
			errs++;
		if (rdbuf[i][4] != (data &0x1f)) 
			errs++;
	}
	
	/* check color map */

	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;

	/* write initialization colors first */
	for (i=0; i<8; i++) {
		base->xmapall.clut = initcolors[i][0]; 
		base->xmapall.clut = initcolors[i][1];
		base->xmapall.clut = initcolors[i][2];
	}

	/* read colors  back  into buffer*/

	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;
	for (i=0; i<8; i++) {
		/* Wait for XMAP external FIFO to be empty */
		while (base->xmap[0].fifostatus & GR2_FIFO_EMPTY);

		while (1)
			if (!(*VRSTAT & vrstat_mask))
				break;
		while (1) 
        		if (*VRSTAT & vrstat_mask)
				break;
		rdbuf[i][0] = (base->xmap[0]).clut;
		rdbuf[i][1] = (base->xmap[0]).clut;
		rdbuf[i][2] = (base->xmap[0]).clut;
	}

	/* compare data */

	for (i=0; i<8; i++) {
		if(rdbuf[i][0] !=  initcolors[i][0]) 
			errs++;
		if(rdbuf[i][1] !=  initcolors[i][1]) 
			errs++;
		if(rdbuf[i][2] !=  initcolors[i][2]) 
			errs++;
	}

	if (errs == 0) msg_printf(VRB,"GR2: XMAP test completed with no errors\n");
	else msg_printf(VRB,"GR2: XMAP test failed\n");
	return errs;
}

#define	VSRAM_START	0x7900 /* Starting address of unused VSRAM */
#define VSRAM_END	0x8000 /* End of VC1 SRAM address + 1 */
	/**********************************************************************
	 * Test VC1
	 *********************************************************************/
chk_vc1(
register struct gr2_hw  *base  /* board base address in K1 seg */) 
{
	int errors,i, offset;
	unsigned int save;
	msg_printf(DBG,"GR2:  VC1 test ...\n");
	errors = 0;
		
		
		  
	for (offset = VSRAM_START; offset < VSRAM_END; offset+=2) {
		base->vc1.addrhi = offset >> 8;
		base->vc1.addrlo = offset & 0x00ff;
		save = base->vc1.sram; /*save the data */

		for (i = 0; i <= 0xffff; i += 0x5555) {
		/* write first */
			base->vc1.addrhi = offset >> 8;
			base->vc1.addrlo = offset & 0x00ff;
			base->vc1.sram = i;
	
		/* read back and compare */
			base->vc1.addrhi = offset >> 8;
			base->vc1.addrlo = offset & 0x00ff;
			if (base->vc1.sram != i)  
				errors++;
		}
		base->vc1.addrhi = offset >> 8;
		base->vc1.addrlo = offset & 0x00ff;
		base->vc1.sram = save; /*restore the data */
	}

	if (errors == 0) msg_printf(VRB,"GR2: VC1 test completed with no errors\n");
	else msg_printf(VRB,"GR2: VC1 test failed\n");
	return errors;

}

gr2_pon(struct gr2_hw *base) 
{
	int errors;
	
	errors = 0;
	errors += chk_dac(base);
	errors += chk_vc1(base); 

#ifdef IP22
	if(!is_indyelan())
#endif
	{
		/* Do not run this test on IndyElan. Will fix later */
		errors += chk_xmap_regs(base); 
	}

	return(errors);
}
