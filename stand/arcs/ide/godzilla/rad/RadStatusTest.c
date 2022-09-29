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
 *	rad_stat -s -v							     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/
#include "ide_msg.h"
#include "uif.h"
#include "pause.h"

#include <libsc.h>
#include <libsk.h>
#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"
#include "d_frus.h"
#include "uif.h"
#include "d_godzilla.h"

#include "d_rad.h"
#include "d_rad_util.h"
#include "d_prototypes.h"

static  __uint64_t dmabuf[512/2];  /* Storage place for status dma */

int RadStatusTest(int argc, char** argv)
{
    int spin=0, verbose=0, temp2;
    int delay = 0, bad_arg;
    volatile uint_t temp;
    int result=0, i;
    volatile uint_t uitmp=0;
    __uint64_t *Tptr;
    int * Tptr2;
    int status=0;

   if (IsOctaneLx())
       return 100;

   RadConfigBase = RAD_PCI_CFG_BASE; /*default*/
 	RadMemoryBase = RAD_PCI_DEVIO_BASE; /*default*/

	/*check args*/
	if (argc > 1) {
		if ( !(strcmp(argv[1], "-s0")) || !(strcmp(argv[1], "-s1")) 
			|| !(strcmp(argv[1], "-s2")) || !(strcmp(argv[1], "-s3"))
		   
			) {}
		else {
			msg_printf(INFO, "PCI Slot argument required\n");
			msg_printf(INFO, "Usage: rad_dma -s[0..2]\n");
			return 1; /*non fatal fail*/
		}
	}
	
	/* get the args */
   argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
   	switch (argv[0][1]) {
			case 's':
				switch (argv[0][2]) {
					case '0':
						RadConfigBase = SHOE_BRIDGE_BASE | 0x020000;
						RadMemoryBase = SHOE_BRIDGE_BASE | 0x200000;
					break;
					
					case '1':
						RadConfigBase = SHOE_BRIDGE_BASE | 0x021000;
						RadMemoryBase = SHOE_BRIDGE_BASE | 0x400000;
					break;
					
					case '2':
						RadConfigBase = SHOE_BRIDGE_BASE | 0x022000;
						RadMemoryBase = SHOE_BRIDGE_BASE | 0x600000;
					break;
					
					case '3':
						RadConfigBase = SHOE_BRIDGE_BASE | 0x023000;
						RadMemoryBase = SHOE_BRIDGE_BASE | 0x700000;
					break;
					
					default:
					 	return 1;
				}	
			break;
			default: 
			bad_arg++; break;
		}
	    argc--; argv++;
	}	

   msg_printf(INFO, "Testing RAD DMA at CONF Addr 0x%x, MEM Addr 0x%x\n", RadConfigBase, RadMemoryBase);
   /* switched off
	argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	if(argv[0][1] == 's')
	    spin = 1;
	else if(argv[0][1] == 'v')
	    verbose = 1;
	argc--; argv++;
    }
	*/
	
	 /*Take this out is -v switched on*/
	 verbose = 1;
	 spin = 0;
    
	 /* 
     * Initialize status dma buffer to 0 __psint_t
     */

	 /* inherited makes last byte 0x00 or 0x80*/
    Tptr =(__uint64_t *) (((__uint64_t)dmabuf & ~127) + 128);
    for(i=0; i<64; i++) 
        Tptr[i] = 0x0;
    Tptr2 = (int *)Tptr;

    /*flushbus();sr remove*/ 
    /*
     * Initialize RAD registers
     */
    
    msg_printf(DBG,"Initializing RAD \n");
 
    result = RadSetup();
    if(result == -1) {
		msg_printf(DBG,"Problem setting up RAD, exiting diagnostic.\n");
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
    
    status = RadSetupDMA(RAD_STATUS,Tptr2,2,spin);
    if(status == -1) {
		msg_printf(DBG,"Problems setting up status registers, exiting\n");
		return -1;
    }
    uitmp = 0;
    SETBITS(uitmp,0x7FFF,0x4,8);	/* bits 17:2 - timer value */
    SETBITS(uitmp,0x1,spin,1);		/* bit 1 - retrigger, 1 = enable */
    SETBITS(uitmp,0x1,1,0);		/* bit 0 - status DMA, 1 = enable */
    RAD_WRITE(RAD_STATUS_TIMER,uitmp);
	
    msg_printf(DBG,"Enabling Status DMA\n");

    if(spin) {
		msg_printf(DBG,"Status DMA: Press space bar to stop dma\n");
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
    FlushAllCaches();	  

    temp = RAD_READ(RAD_PCI_CONTROL_STATUS);
    msg_printf(DBG,"DMA done\n");

    for(i=0;i<32;i++) msg_printf(DBG,"i= 0x%x, Tptr2= 0x%x\n", i, Tptr2[i]);

    for(i=0;i<16;i++) {
		if(Tptr2[i] == (0x55AA0000+i)) {		
	    	if(verbose)
				msg_printf(DBG,"Word %d: OK\n",i);
		}
		else {
	    	if(verbose) {
				msg_printf(DBG,"Word %d: FAIL\n",i); 
	   	}
	    	result++;
    	}
    }

    if(Tptr2[31] ==  0x80000000) {
		if(verbose)
	    	msg_printf(DBG,"Word 31: OK\n");
    	} else {
		if(verbose) 
	    	msg_printf(DBG,"Word 31: FAIL\n");
	    	result++;
    }
   
/*
    msg_printf(INFO,"RAD Status DMA Test: ");
    if(result) {
		msg_printf(INFO,"FAIL\n");
		msg_printf(INFO,"Number of failures: %d \n",result);
    }
    else msg_printf(INFO,"OK\n");
 
    return result;
*/ 
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(result, "RAD Status DMA", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RAD_0003], result ); 
}
	    
