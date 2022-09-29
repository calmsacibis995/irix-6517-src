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
 *      Test RAD configuration space registers.  Check reset state   * 
 *      of every register and report if they are correct.            *
 *                                                                   *
 *      Test Options:                                                *
 *         -s (Check device ID register only in spin mode)           *
 *         -v Verbose mode                                           *
 *								     *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/

#include "ide_msg.h"
#include "uif.h"
#include "sys/types.h" 
#include <sgidefs.h>
#include "sys/sbd.h"

#include "sys/PCI/bridge.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"

#include "d_rad.h"
#include "d_rad_util.h"

#define RAD_CFG_ID_VAL      0x000510A9

#define RAD_CFG_ID_MASK      0xFFFFFFFF  
#define RAD_CFG_LATENCY_MASK 0xFFFFC000
 
int RadVerifyConfig(uint_t ,uint_t ,uint_t ,uint_t ,char* , int );

__uint32_t RadConfigBase;
__uint32_t RadMemoryBase; 

/*run all rad tests on all cards*/
int RunAllCardRadTests () {
	short err;
	char *s1[] = {"dummy", "-s1"}; /*build cmd arg string. No arg check required*/
   char *s2[] = {"dummy", "-s2"};
	char *s3[] = {"dummy", "-s3"};

    if (IsOctaneLx())
        return 100;
	
	msg_printf(INFO, "Running All Rad Tests On All Cards\n");
	
	
   msg_printf(INFO, "Testing Radical Card 1\n");
	err = RadCSpaceTest(2, (char **)s1);
	if (!err) err = RadRamTest(2, (char **)s1);
	if (!err) err = RadStatusTest(2, (char **)s1);
	if (!err) msg_printf(INFO, "Card 1 Tests Passed\n");
	else msg_printf(INFO, "Card 1 Test Failed\n");

   msg_printf(INFO, "Testing Radical Card 2\n");
	if (!err) err =  RadCSpaceTest(2, (char **)s2);
	if (!err) err = RadRamTest(2, (char **)s2);
	if (!err) err = RadStatusTest(2, (char **)s2);
	if (!err) msg_printf(INFO, "Card 2 Tests Passed\n");
	else msg_printf(INFO, "Card 2 Test Failed\n");
   
	msg_printf(INFO, "Testing Radical Card 3\n");
	if (!err) err =  RadCSpaceTest(2, (char **)s3);
	if (!err) err = RadRamTest(2, (char **)s3);
	if (!err) err = RadStatusTest(2, (char **)s3);
	if (!err) msg_printf(INFO, "Card 3 Tests Passed\n");
	else msg_printf(INFO, "Card 3 Test Failed\n");
	
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(err, "Rad All Card Tests", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RAD_0000], err ); 
}

int RunAllRadTests (int argc, char** argv) {
	short err;

    if (IsOctaneLx())
        return 100;

   msg_printf(INFO, "Running All Rad Tests\n");
	/*check args*/
	if (argc > 1) {
		if ( !(strcmp(argv[1], "-s0")) || !(strcmp(argv[1], "-s1")) 
			  || !(strcmp(argv[1], "-s2")) || !(strcmp(argv[1], "-s3"))
		   ) {}
		else {
			msg_printf(INFO, "PCI Slot argument required\n");
			msg_printf(INFO, "Usage: rad_all -s[0..3]\n");
			return 1; /*non fatal fail*/
		}
	}
	err = RadCSpaceTest(argc, argv);
	if (!err) err = RadRamTest(argc, argv);
	if (!err) err = RadStatusTest(argc, argv);

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(err, "Rad All Tests", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RAD_0000], err ); 
}

int RunAllShoeboxTests (int argc, char** argv) {
	short err;
	char *str1[] = {"dummy", "13"}; /*build cmd arg string. No arg check required*/

   msg_printf(INFO, "Running All PCI Shoe Box Tests\n");
	err = br_regs(2, (char **)str1);
	if (!err) err = br_intr(2, (char **)str1);
	if (!err) err = br_ram(2, (char **)str1);
	if (!err) err = br_err(2, (char **)str1);
	
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(err, "Shoebox All Tests", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0008], err ); 
}

/*
rad_regs test on board rad
rad_regs -s0 test on shoe box in pci slot 0 change to slots
1..3 for shipping version
*/
int RadCSpaceTest(int argc, char** argv)
{
    
   int spin=0, bad_arg;
   int status=0, globStatus=0;
   uint_t reg;
   u_int mask,flags;
	short radRevision;
 	
    if (IsOctaneLx())
        return 100;

	RadConfigBase = RAD_PCI_CFG_BASE; /*default*/
 	RadMemoryBase = RAD_PCI_DEVIO_BASE; /*default*/
	radRevision = 0xC0;

	/*check args*/
	if (argc > 1) {
		if ( !(strcmp(argv[1], "-s0")) || !(strcmp(argv[1], "-s1")) 
			  || !(strcmp(argv[1], "-s2")) || !(strcmp(argv[1], "-s3"))
		   ) {}
		else {
			msg_printf(INFO, "PCI Slot argument required\n");
			msg_printf(INFO, "Usage: rad_regs -s[0..2]\n");
			return 1; /*non fatal fail*/
		}
	}
	
	/* get the args */
   argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
   	switch (argv[0][1]) {
			case 's':
				radRevision = 0xD0;
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

    msg_printf(INFO, "Testing RAD Registers at CONF Addr 0x%x, MEM Addr 0x%x\n", RadConfigBase, RadMemoryBase);

    /* Check Device ID register */
    reg = RAD_CFG_READ(RAD_CFG_ID);
    if (reg != RAD_CFG_ID_VAL) {
		msg_printf(INFO, "RAD Configuration Register Test Failed\n");
		msg_printf(INFO, "RAD Configuration ID = 0x%x\n", reg);
		globStatus = -1;
    }

     /* Check revison register */
    if (!globStatus) {
		reg = RAD_CFG_READ(RAD_CFG_REV);
		if (reg != radRevision) {
	   	globStatus = -1;
	   	msg_printf(INFO, "RAD Revision Register Test Failed\n");
	   	msg_printf(INFO, "RAD Revision = 0x%x\n", reg);
    	}
    }

#ifdef NOTNOW
    REPORT_PASS_OR_FAIL(globStatus? 1: 0, "Rad Configuration Space", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_RAD_0001], (globStatus ? 1 : 0) ); 
}
