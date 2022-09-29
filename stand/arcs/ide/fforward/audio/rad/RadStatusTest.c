/*********************************************************************
 *                                                                   *
 *  Title :  RadStatusTest					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  03.18.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  								     *
 *      Test Status DMA transfer.  RadStatusTest will write a        *
 *      predetermined pattern to the working registers and later     *
 *      confirm their values in main memory after status dma         *
 *      occurred.                                                    *
 *                                                                   *
 *      Test Options:                                                *
 *         -s <delay> delay based in PCI clock rate values           *
 *         -v (Print in verbose mode)                                *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/
#include "ide_msg.h"
#include "uif.h"
#include "pause.h"

#include <libsc.h>
#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"

#include <sys/rad.h>
#include "rad_util.h"

static int dmabuf[512];  /* Storage place for status dma */

int RadStatusTest(int argc, char** argv)
{
    int spin=0, verbose=0;
    int delay = 0;
    volatile uint_t temp;
    int result=0, i;
    volatile uint_t uitmp=0;
    int *Tptr;
    int status=0;
    /*
     * Check for command line Options
     */
    

    msg_printf(DBG,"Checking command line arguments\n");

    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	if(argv[0][1] == 's')
	    spin = 1;
	else if(argv[0][1] == 'v')
	    verbose = 1;
	argc--; argv++;
    }

    /* 
     * Initialize status dma buffer to 0
     */

    Tptr =(int *) ((int)dmabuf & ~127) + 128;
    for(i=0; i<64; i++) 
        Tptr[i] = 0x0;

    flushbus();
    /*
     * Initialize RAD registers
     */
    
    msg_printf(DBG,"Initializing RAD \n");

    result = RadSetup();
    if(result == -1) {
	msg_printf(SUM,"Problem setting up RAD, exiting diagnostic.\n");
	return -1;
    }

    /* 
     * Write a pattern to the working DMA registers, so that we can cross
     * check them when they are dma'd by status dma.
     */
    

    msg_printf(DBG,"Doing PIO to command line registers \n");

    for(i=0; i<16; i++) 
	RAD_WRITE(RAD_PCI_LOADR_ADAT_RX+(i*4), 0x55AA0000+i);

    /*
     * Setup status dma, including value for the delay timer and 
     * start it.
     */

    msg_printf(DBG,"Setting up status dma working registers\n");
    
    status = RadSetupDMA(RAD_STATUS,Tptr,2,spin);
    if(status == -1) {
	msg_printf(SUM,"Problems setting up status registers, exiting\n");
	return -1;
    }
   uitmp = 0;
    SETBITS(uitmp,0x7FFF,0x4,8);	/* bits 17:2 - timer value */
    SETBITS(uitmp,0x1,spin,1);		/* bit 1 - retrigger, 1 = enable */
    SETBITS(uitmp,0x1,1,0);		/* bit 0 - status DMA, 1 = enable */
    RAD_WRITE(RAD_STATUS_TIMER,uitmp);
	

    msg_printf(DBG,"Enabling Status DMA\n");

    if(spin) {
	msg_printf(SUM,"Status DMA: Press space bar to stop dma\n");
	pause(0," ",NULL);
	uitmp = RAD_READ(RAD_STATUS_TIMER);
	SETBITS(uitmp,0x1,0,0); /* Bit 0: Stop status dma */
	RAD_WRITE(RAD_STATUS_TIMER,uitmp);
    }    
    else {
	/*
	 * Wait for a second to make sure that status dma
	 * has been finished, more than enough time since the timer
	 * was set to ~2ms delay for the transfer.
	 */

	pause(1,NULL,NULL);


	temp = RAD_READ(RAD_STATUS_TIMER);
	i=0;
	while(temp & 0x1 ) {
	  pause(1,NULL,NULL);
	  temp = RAD_READ(RAD_STATUS_TIMER);
	  if(i++ == 5)
	    break;
	}

    }
    invalidate_caches();	  

    temp = RAD_READ(RAD_PCI_CONTROL_STATUS);


    for(i=0;i<16;i++) {
	if(Tptr[i] == (0x55AA0000+i)) {
	    if(verbose)
		msg_printf(SUM,"Word %d: OK\n",i);
	}
	else {
	    if(verbose) {
		msg_printf(SUM,"Word %d: FAIL\n",i); 
	    }
	    result++;
	}
    }

    if(Tptr[31] ==  0x80000000) {
	if(verbose)
	    msg_printf(SUM,"Word 31: OK\n");
    } else {
	if(verbose) 
	    msg_printf(SUM,"Word 31: FAIL\n");
	    result++;
    }
    
    msg_printf(SUM,"RAD Status DMA Test: ");
    if(result) {
	msg_printf(SUM,"FAIL\n");
	msg_printf(SUM,"Number of failures: %d \n",result);
    }
    else
	msg_printf(SUM,"OK\n");
    
    return result;
       
}
	    
