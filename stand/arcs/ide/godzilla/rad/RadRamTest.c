/*********************************************************************
 *                                                                   *
 *  Tite :  RadRamTest   					     *
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
#include "sys/PCI/bridge.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"

#include "d_rad_util.h"
#include "d_rad.h"

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
   int  i, bad_arg;
   int value = 0;
   int pattern = 0x55AA00;
    
   int positiveSaturation = 0x01AABB00;
   int negativeSaturation = 0x81AABB00;
   int signExtension = 0x80AA00;

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
			msg_printf(INFO, "Usage: rad_ram -s[0..2]\n");
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

   msg_printf(INFO, "Testing RAD RAM at CONF Addr 0x%x, MEM Addr 0x%x\n", RadConfigBase, RadMemoryBase);
   
	/*turned off to match arg passing in IDE
	verbose = 0;
    
	for(i=1; i<argc; i++) {
		if(!strcmp("-r",argv[i]))
		if(!strcmp("-v",argv[i]))
	    	verbose = 1;
		if(!strcmp("-p",argv[i]))
	    	pattern = atoi(argv[++i]);
    }
	 */

	 /*remove these settings if -v -p turned on*/
	 verbose = 1;
	 pattern = 0;

   /* 
    * Initialize and verify RAD configuration registers.
    */

   msg_printf(DBG,"RAD Configuration \n");
   /* causes exception while RAD_WRITE, READ OK*/
   status = RadSetup();
   if(status == -1) {
		msg_printf(DBG,"Problems setting up RAD, exiting diagnostic\n");
		return -1;
    }
  
   /* 
    * Set RAD into RAM test mode
    */
	msg_printf(DBG,"Set RAD into RAM test mode\n");
   SETBITS(value,0x1,1,5);
   RAD_WRITE(RAD_MISC_CONTROL,value);
   /*PIO_REG_WR_32(PCI_MEM_BASE+RAD_MISC_CONTROL, mask32, value);*/

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
		msg_printf(DBG,"RAD DMA Descriptor Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
   }	    
    
   globStatus |= status;
   status = 0;

   /* Test RAD DMA Working registers */

   status = RadRWVPattern(RAD_PCI_LOADR_ADAT_RX,
			   RAD_NUMBER_WORK_REGISTERS,			   
			   pattern);

   if(verbose) {
		msg_printf(DBG,"RAD DMA Working Registers  R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
   }	    
    
   globStatus |= status;
   status = 0;

   /* Test Subcode Ram */

   status = RadRWVPattern(RAD_ADAT_SUBCODE_TXA_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);

   if(verbose) {
		msg_printf(DBG,"RAD ADAT Subcode Tx A Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
   }	    
   
   globStatus |= status;
   status = 0;
    
   status = RadRWVPattern(RAD_ADAT_SUBCODE_TXB_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);

   if(verbose) {
	msg_printf(DBG,"RAD ADAT Subcode Tx B Ram R/W test: ");
	if(status)
	    msg_printf(DBG,"FAIL\n");
	else 
	    msg_printf(DBG,"OK\n");
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
	msg_printf(DBG,"RAD AES Subcode Tx A R/W test: ");
	if(status)
	    msg_printf(DBG,"FAIL\n");
	else 
	    msg_printf(DBG,"OK\n");
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
	msg_printf(DBG,"AES Subcode Tx B Ram R/W test: ");
	if(status)
	    msg_printf(DBG,"FAIL\n");
	else 
	    msg_printf(DBG,"OK\n");
    }	    
   
	status = RadRWVPattern(RAD_ADAT_SUBCODE_RXA_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);

   if(verbose) {
		msg_printf(DBG,"RAD ADAT Subcode Rx A Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
    }	    
    
   globStatus |= status;
   status = 0;
    
   status = RadRWVPattern(RAD_ADAT_SUBCODE_RXB_U0_0,
			   RAD_NUMBER_ADAT_SUBCODE,
			   pattern);


   if(verbose) {
		msg_printf(DBG,"RAD ADAT Subcode Rx B Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
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
		msg_printf(DBG,"RAD AES Subcode Rx A R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;

    status = RadRWVPattern(RAD_AES_SUBCODE_RXB_RV2,
			   RAD_NUMBER_AES_SUBCODE2,
			   pattern);
    
	 if(verbose) {
		msg_printf(DBG,"RAD AES Subcode Rx A R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
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
		msg_printf(DBG,"AES Subcode Rx B Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
    }	    

    /*
     * Test Data Tx DMA:
     */

    status = RadRWVPattern(RAD_ADAT_TX_DATA,
			   RAD_NUMBER_DATA,
			   pattern);

    if(verbose) {
		msg_printf(DBG,"RAD Tx Data Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
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
		msg_printf(DBG,"RAD Rx Data Ram R/W test: ");
		if(status)
	    	msg_printf(DBG,"FAIL\n");
		else 
	    	msg_printf(DBG,"OK\n");
    }	    
    
    globStatus |= status;
    status = 0;
    

    /*
     * Get RAD out of RAD test mode
     */

    msg_printf(DBG,"Set RAD out of RAM test mode\n");
    SETBITS(value,0x1,0,5);
    RAD_WRITE(RAD_MISC_CONTROL,value);
    /*PIO_REG_WR_32(PCI_MEM_BASE+RAD_MISC_CONTROL, mask32, value);*/
    /*
     * Report Exit code
     */

#ifdef NOTNOW
    REPORT_PASS_OR_FAIL(globStatus? 1: 0, "RAD RAM", D_FRU_IP30);
#endif
    REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RAD_0002], (globStatus ? 1 : 0) ); 
}


 
RadRWVPattern(uint_t baseRegister, 
	      uint_t numberOfRegisters, 
	      int basePattern)
{
    int status=0, i;
    uint_t readback=0;
    uint_t readbackPattern;

    for(i=0; i<numberOfRegisters; i++) RAD_WRITE(baseRegister+i*4,basePattern+i);

    for(i=0; i<numberOfRegisters; i++) {

		readback = RAD_READ(baseRegister+i*4);

		readbackPattern = basePattern + i;
	   
	 	msg_printf(DBG,"Addr: %x Read Value %x Actual pattern: %x\n",baseRegister+i*4,  readback,
		       readbackPattern);
	
		if(readback != readbackPattern && verbose) {
	  		msg_printf(ERR,"Read Value %x Actual pattern: %x\n",readback,
	      readbackPattern);
	  		status = 1;
		}	    
    
	 }
    return status;
    
}
