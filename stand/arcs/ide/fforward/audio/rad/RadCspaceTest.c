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

#include <sys/rad.h>
#include "rad_util.h"
#include <sys/mc.h>

#define RAD_CFG_ID_VAL      0x000510A9


#define RAD_CFG_ID_MASK      0xFFFFFFFF
#define RAD_CFG_LATENCY_MASK 0xFFFFC000
#define RAD_CFG_BASE_MASK    0xFF00
 

int RadVerifyConfig(uint_t ,uint_t ,uint_t ,uint_t ,char* , int );


int RadCSpaceTest(int argc, char** argv)
{
    
    int spin=0;
    int verbose=0;
    int status=0, globStatus=0;
    uint_t reg;
    extern unsigned int _gio64_arb;

    u_int gio64_arb_reg, gio64_arb_reg_sav;
    u_int mask,flags;

    /*
     * Check for command line options.
     */

    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	if(argv[0][1] == 's')
	    spin = 1;
	else if(argv[0][1] == 'v')
	    verbose = 1;
	argc--; argv++;
    }

    
    if(spin) {
	msg_printf(SUM,"Entering spin mode\n");
	while(1) 
	    reg = RAD_CFG_READ(RAD_CFG_ID);
    }

    /* Check Device ID register */
    status = RadVerifyConfig(RAD_CFG_ID,RAD_CFG_ID_VAL,
			     RAD_CFG_ID_VAL,
			     RAD_CFG_ID_MASK,
			     "Device ID",
			     verbose);
    globStatus |= status;
    status=0;


    /* Class Latency Register */
    status = RadVerifyConfig(RAD_CFG_LATENCY,1,
			     0xffffffff,
			     RAD_CFG_LATENCY_MASK,
			     "Latency Timer",
			     verbose);
    globStatus |= status;
    status=0;

    /* Memory Base Register */
    status = RadVerifyConfig(RAD_CFG_MEMORY_BASE,1,
			     PCI_MEM_BASE,
			     RAD_CFG_BASE_MASK,
			     "Memory Base",
			     verbose);

    globStatus |= status;
    status=0;

    /*
     * Print RAD class and rev number 
     */

    if(verbose) {
	reg = RAD_CFG_READ(RAD_CFG_CLASS);
	msg_printf(SUM,"RAD Class Code:\t%x\n",reg>>8);
	msg_printf(SUM,"RAD Board Id:\t%x\n",(reg & 0xf0)>>4);
	msg_printf(SUM,"RAD Chip Rev:\t%x\n",reg & 0xf);
    }

    msg_printf(SUM,"Rad Configuration Space Test:");
    if(globStatus)
	msg_printf(SUM," FAIL\n");
    else
	msg_printf(SUM," OK\n");
    
    return globStatus;

}


int RadVerifyConfig(uint_t addr, 
		    uint_t value,
		    uint_t readWrite,
		    uint_t mask,
		    char* description,
		    int verbose)
{

    int status = 0;
    uint_t reg;

    if(readWrite) {
	RAD_CFG_WRITE(addr,value);
    }

    reg = RAD_CFG_READ(addr);
    reg &= mask;
    value &= mask;

    if(reg != value) {
	status = 1;
	msg_printf(SUM,"readback = %x \t value = %x\n",reg,value);    
      }

	
    
    if(verbose) {
	msg_printf(SUM,"RAD %s register:",description); 
	if(status)
	    msg_printf(SUM," FAIL\n");
	else 
	    msg_printf(SUM," OK\n");
    }

    return status;
}
