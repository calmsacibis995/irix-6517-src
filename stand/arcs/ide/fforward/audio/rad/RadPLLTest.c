/*********************************************************************
 *                                                                   *
 *  Title :  RadCSpaceTest					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  03.08.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  								     *
 *      Test RAD Frequency lock and range detectors.                 *
 *      The test will check for if the output of the mplls are       * 
 *      locked to their input, it will also check for the            *
 *      if the input and output is overrunned or underruned          *
 *                                                                   *
 *      Test Options:                                                *
 *         -aes (Check the AES MPLL) default                         *
 *         -adat (Check the ADAT MPLL)                               *
 *         -i <interval>  (PCI Reporting Interval)                   *
 *         -s (Sensitivity for input vs. output clock)               *
 *         -d Total acceptable delay for lock
 *         -range <PLL_RANGE> Reference pll range                    *
 *         -r Reset RAD after finished                               *
 *         -v Turn on verbose mode                                   *
 *                                                                   *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/

#include "ide_msg.h"
#include "uif.h"
#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"
#include "libsc.h"

#include <sys/rad.h>
#include "rad_util.h"

static char* adatString = "MPLL0";
static char* aesString = "MPLL1";

int RadPLLTest(int argc, char** argv)
{
    
    int i;

    uint_t value, readback;
    int adat=0, aes=0, verbose=0;
    int locked=0;
    char*  pllString;
    uint_t mask;

    uint_t pciInterval = 0xffffff; /* ~2 ms. */
    uint_t pllRange = 0x3000;      /* 48 kHz x 256 */
    uint_t freqDiff = 0;           /* Sensitivity of 2 bits */
    uint_t totalDelay = 50;        /* 50 milliseconds */
    
    unsigned long time, initTime;
    /*
     * Parse command line arguments
     */
    
    for(i=1; i<argc; i++) {
	if(!strcmp(argv[i],"-adat")) 
	    adat = 1;
	if(!strcmp(argv[i],"-aes"))
	    aes = 1;
	if(!strcmp(argv[i],"-v"))
	    verbose = 1;
	if(!strcmp(argv[i],"-s"))
	    freqDiff = 1;
	if(!strcmp(argv[i],"-d"))
	    totalDelay = atoi(argv[++i]);
	if(!strcmp(argv[i],"-range"))
	   pllRange = atoi(argv[++i]);
	if(!strcmp(argv[i],"-i"))
	    pciInterval = atoi(argv[++i]);
	   
    }

    
    /*
     * Initialize RAD
     */

    if(RadSetup() == -1) {
	msg_printf(SUM,"Rad Initialization failed aborting test.\n");
	return -1;
    }
    

    /* 
     *  Setup the MPLL lock control register
     */
    SETBITS(value,0xFFFFFF,pciInterval,0); /* PCI Interval Bits 15:0 */
    SETBITS(value,0x1,freqDiff,30);        /* Freq. diff sensitivity */
    
    /*
     * TODO: Add to test parameters for range detection.
     */
    if(aes) {
	RAD_WRITE(RAD_MPLL1_LOCK_CONTROL,value);
	mask = 0x20;            /* mask for readback */
	pllString = aesString;
    }
    else {
	RAD_WRITE(RAD_MPLL0_LOCK_CONTROL,value);
	mask = 0x4;             /* mask for readback */
	pllString = adatString; 
    }
    
    /* 
     * Check for frequency lock and range, usually this would be done
     * through status DMA.  
     */


    time = initTime = RadGetRelativeTime();
    
    while(totalDelay > (time - initTime)) {
	readback = RAD_READ(RAD_CHIP_STATUS1);
	time = RadGetRelativeTime();
	if(!(readback & mask)) { /* check for frequency lock */
	    locked = 1;
	    break;
	}
    }

	if(locked) 
	    msg_printf(SUM,"%s locked after %d milliseconds\n",
		       pllString,time-initTime);
	else 
	    msg_printf(SUM,"%s failed to lock after %d milliseconds\n",
		       pllString,totalDelay);

	return (!locked);
}
