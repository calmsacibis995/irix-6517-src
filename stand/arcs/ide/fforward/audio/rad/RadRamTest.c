/*********************************************************************
 *                                                                   *
 *  Title :  RadRamTest   					     *
 *  Author:  Jimmy Acosta					     *
 *  Date  :  03.18.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  	                                                             *
 *    This program tests reading and writing into RAD's RAM.  It     *
 *    will verify that the saturation and sign extension is properly *
 *    working.                                                       *
 *    Options:                                                       *
 *             -v (verbose mode)                                     *
 *	       -r (Reset RAD after operation)			     *
 *                                                                   *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/
#include "ide_msg.h"
#include "uif.h"

#include "sys/types.h"
#include <sgidefs.h>
#include "sys/sbd.h"
#include "libsc.h"

#include "rad_util.h"
#include <sys/rad.h>

#define RAD_POSITIVE_SAT 0x800000
#define RAD_NEGATIVE_SAT 0x7FFFFF


#define RAD_NUMBER_ADAT_SUBCODE    24
#define RAD_NUMBER_AES_SUBCODE1    32
#define RAD_NUMBER_AES_SUBCODE2    4

#define RAD_NUMBER_DATA            256
 

typedef enum { NONE, 
	       POSITIVE_SATURATION, 
	       NEGATIVE_SATURATION,
	       SIGN_EXTENSION } RadTestType;


static int verbose=0;
int RadRWVPattern(uint_t , uint_t , int ); 

int RadRamTest(int argc, char** argv)
{
    int status = 0, globStatus=0; 
    int  i;
    int reset = 0, value = 0;
    int pattern = 0x55AA00;
    
    int positiveSaturation = 0x01AABB00;
    int negativeSaturation = 0x81AABB00;
    int signExtension = 0x80AA00;
    /*
     * Check for command line Options
     */
    
    verbose = 0;
    for(i=1; i<argc; i++) {
	if(!strcmp("-r",argv[i]))
	    reset = 1;
	if(!strcmp("-v",argv[i]))
	    verbose = 1;
	if(!strcmp("-p",argv[i]))
	    pattern = atoi(argv[++i]);
    }

    /* 
     * Initialize and verify RAD configuration registers.
     */

    msg_printf(DBG,"RAD Configuration \n");
    status = RadSetup();
    if(status == -1) {
	msg_printf(SUM,"Problems setting up RAD, exiting diagnostic\n");
	return -1;
    }
    
    /* 
     * Set RAD into RAM test mode
     */
    msg_printf(DBG,"Set RAD into RAM test mode\n");
    SETBITS(value,0x1,1,5);
    RAD_WRITE(RAD_MISC_CONTROL,value);

    /*
     * We will test all RAD DMA areas: DMA RAM, Tx Subcode RAM, Tx Data RAM,
     * Rx Subcode RAM, Rx Data RAM.  For the data RAM test saturation and 
     * sign extension.
     */
    
    /* Start out with Descriptor RAM */
    
    msg_printf(DBG,"Testing Descriptor RAM\n");
    status = RadRWVPattern(RAD_PCI_LOADR_D0,
			   RAD_NUMBER_DESCRIPTORS,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD DMA Descriptor Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;

    /* Test RAD DMA Working registers */

    status = RadRWVPattern(RAD_PCI_LOADR_ADAT_RX,
			   RAD_NUMBER_WORK_REGISTERS,			   
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD DMA Working Registers  R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;

    /* Test Subcode Ram */

    status = RadRWVPattern(RAD_ADAT_SUBCODE_TXA_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD ADAT Subcode Tx A Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
   
    globStatus |= status;
    status = 0;
    
    status = RadRWVPattern(RAD_ADAT_SUBCODE_TXB_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);


    if(verbose) {
	msg_printf(SUM,"RAD ADAT Subcode Tx B Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }

    globStatus |= status;
    status = 0;

    /* 
     * AES Subcode has a break in the address due to 
     * size requirements in the standard.
     */

    status = RadRWVPattern(RAD_AES_SUBCODE_TXA_LU0,
			   RAD_NUMBER_AES_SUBCODE1,
			   pattern);


    globStatus |= status;
    status = 0;

    status = RadRWVPattern(RAD_AES_SUBCODE_TXA_RV2,
			   RAD_NUMBER_AES_SUBCODE2,
			   pattern);
    globStatus |= status;
    status = 0;

    if(verbose) {
	msg_printf(SUM,"RAD AES Subcode Tx A R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    


    status = RadRWVPattern(RAD_AES_SUBCODE_TXB_LU0,
			   RAD_NUMBER_AES_SUBCODE1,
			   pattern);

    globStatus |= status;
    status = 0;

    status = RadRWVPattern(RAD_AES_SUBCODE_TXB_RV2,
			   RAD_NUMBER_AES_SUBCODE2,
			   pattern);

    globStatus |= status;
    status = 0;

    if(verbose) {
	msg_printf(SUM,"AES Subcode Tx B Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    status = RadRWVPattern(RAD_ADAT_SUBCODE_RXA_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD ADAT Subcode Rx A Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;
    
    status = RadRWVPattern(RAD_ADAT_SUBCODE_RXB_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);


    if(verbose) {
	msg_printf(SUM,"RAD ADAT Subcode Rx B Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }

    globStatus |= status;
    status = 0;

    /* 
     * AES Subcode has a break in the address due to 
     * size requirements in the standard.
     */

    status = RadRWVPattern(RAD_AES_SUBCODE_RXA_LU0,
			   RAD_NUMBER_AES_SUBCODE1,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD AES Subcode Rx A R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;

    status = RadRWVPattern(RAD_AES_SUBCODE_RXB_RV2,
			   RAD_NUMBER_AES_SUBCODE2,
			   pattern);
    if(verbose) {
	msg_printf(SUM,"RAD AES Subcode Rx A R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;


    status = RadRWVPattern(RAD_AES_SUBCODE_RXB_LU0,
			   RAD_NUMBER_AES_SUBCODE1,
			   pattern);

    globStatus |= status;
    status = 0;

    status = RadRWVPattern(RAD_AES_SUBCODE_RXB_RV2,
			   RAD_NUMBER_AES_SUBCODE2,
			   pattern);

    globStatus |= status;
    status = 0;

    if(verbose) {
	msg_printf(SUM,"AES Subcode Rx B Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    

    /*
     * Test Data Tx DMA:
     */

    status = RadRWVPattern(RAD_ADAT_TX_DATA,
			   RAD_NUMBER_DATA,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD Tx Data Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;


    /* 
     * Test Rx Data Ram 
     */

    status = RadRWVPattern(RAD_ADAT_RX_DATA,
			   RAD_NUMBER_DATA,
			   pattern);

    if(verbose) {
	msg_printf(SUM,"RAD Rx Data Ram R/W test: ");
	if(status)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;
    

    /*
     * Get RAD out of RAD test mode
     */

    msg_printf(DBG,"Set RAD out of RAM test mode\n");
    SETBITS(value,0x1,0,5);
    RAD_WRITE(RAD_MISC_CONTROL,value);

    /*
     * Report Exit code
     */
    
    msg_printf(SUM,"RAM RAM Test: ");

    if(globStatus)
	    msg_printf(SUM,"FAIL\n");
	else 
	    msg_printf(SUM,"OK\n");

    return globStatus;

}


int 
RadRWVPattern(uint_t baseRegister, 
	      uint_t numberOfRegisters, 
	      int basePattern)
{
    int status=0, i;
    uint_t readback=0;
    uint_t readbackPattern;

    for(i=0; i<numberOfRegisters; i++) 
	RAD_WRITE(baseRegister+i*4,basePattern+i);

    for(i=0; i<numberOfRegisters; i++) {
	readback = RAD_READ(baseRegister+i*4);
	readbackPattern = basePattern + i;
	    
	if(readback != readbackPattern && verbose) {
	    msg_printf(SUM,"Error reading back value from address %x:\n",
		       baseRegister+i*4);
	    msg_printf(SUM,"Read Value %x Actual pattern: %x\n",readback,
		       readbackPattern);
	    status = 1;
	}	    
    }
    return status;
    
}
