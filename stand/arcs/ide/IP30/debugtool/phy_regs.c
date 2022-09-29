 /* phy_regs.c : 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "ide/IP30/phy/phy_regs.c:  $Revision: 1.3 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"
#include "phy.h"

/*
 * Forward References: all in d_prototypes.h 
 */
extern const short ArgLength;
extern const short ArgLengthData;

/* phy registers */
phy_Registers	gz_phy_regs_ep[] = {

{"PHY_CONTROL",         	PHY_CONTROL,            	GZ_READ_ONLY,
PHY_CONTROL_MASK,       	PHY_CONTROL_DEFAULT},

{"PHY_STATUS",         		PHY_STATUS,            		GZ_READ_ONLY,
PHY_STATUS_MASK,       		PHY_STATUS_DEFAULT},

{"PHY_ID1",         		PHY_ID1,            		GZ_READ_ONLY,
PHY_ID1_MASK,       		PHY_ID1_DEFAULT},

{"PHY_ID2",         		PHY_ID2,            		GZ_READ_ONLY,
PHY_ID2_MASK,       		PHY_ID2_DEFAULT},

{"PHY_AUTO_NEG_ADDl",         	PHY_AUTO_NEG_ADD,            	GZ_READ_ONLY,
PHY_AUTO_NEG_ADD_MASK,	 	PHY_AUTO_NEG_ADD_DEFAULT},

{"PHY_AUTO_NEG_LINK_PTNR",	PHY_AUTO_NEG_LINK_PTNR,         GZ_READ_ONLY,
PHY_AUTO_NEG_LINK_PTNR_MASK, 	PHY_AUTO_NEG_LINK_PTNR_DEFAULT},

{"PHY_AUTO_NEG_EXP",         	PHY_AUTO_NEG_EXP,            	GZ_READ_ONLY,
PHY_AUTO_NEG_EXP_MASK,       	PHY_AUTO_NEG_EXP_DEFAULT},

{"PHY_EXT_CONTROL",         	PHY_EXT_CONTROL,            	GZ_READ_ONLY,
PHY_EXT_CONTROL_MASK,       	PHY_EXT_CONTROL_DEFAULT},

{"PHY_QUICKPOLL_STATUS",        PHY_QUICKPOLL_STATUS,           GZ_READ_ONLY,
PHY_QUICKPOLL_STATUS_MASK,      PHY_QUICKPOLL_STATUS_DEFAULT},

{"PHY_10BASET",         	PHY_10BASET,            	GZ_READ_WRITE,
PHY_10BASET_MASK,       	PHY_10BASET_DEFAULT},

{"PHY_EXT_CONTROL2",         	PHY_EXT_CONTROL2,            	GZ_READ_ONLY,
PHY_EXT_CONTROL2_MASK,       	PHY_EXT_CONTROL2_DEFAULT},

{"", -1, -1, -1 }
};


__uint32_t ReadPHYRegister(__uint32_t registerOffset, __uint32_t mask) { 
	__uint32_t tmp;

	/*wait for busy to clear*/
	PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	while ((tmp&PHY_BUSY)) {
		PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	}

	PIO_REG_WR_32(IOC3REG_MICR, ~0x0, PHY_ADDR|registerOffset|PHY_READ);
	
	/*wait for busy to clear*/
	PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	while ((tmp&PHY_BUSY)) {
		PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	}
	
	/*read data*/
	PIO_REG_RD_32(IOC3REG_MIDR, mask, tmp);
	return tmp;
}

void WritePHYRegister(__uint32_t registerOffset, __uint32_t data, __uint32_t mask ) {
	__uint32_t tmp;

	/*wait for busy to clear*/
	PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	while ((tmp&PHY_BUSY)) {
		PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	}
	
	/*write data*/
	PIO_REG_WR_32(IOC3REG_MIDW, mask, data);
	PIO_REG_WR_32(IOC3REG_MICR, ~0x0, PHY_ADDR|registerOffset|PHY_WRITE);
	
	/*wait for busy to clear*/
	PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	while ((tmp&PHY_BUSY)) {
		PIO_REG_RD_32(IOC3REG_MICR, ~0x0, tmp);
	}
}

/*
 * Name:	_phy_regs_write.c
 * Description:	Performs Write/Reads on the phy Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_phy_regs_write(phy_Registers *regs_ptr, __uint32_t data)
{					
    msg_printf(DBG, "Write location (32) = 0x%x\n", regs_ptr->address);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    WritePHYRegister(regs_ptr->address, regs_ptr->mask, data);
    return(0);
}

/*
 * Name:	_phy_regs_read
 * Description:	Performs Reads on the phy Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_phy_regs_read(phy_Registers *regs_ptr)
{
    phyregisters_t	saved_reg_val;
					
    /* Save the current value */
    saved_reg_val = ReadPHYRegister(regs_ptr->address, regs_ptr->mask);
    msg_printf(INFO,"Read Addr (32) 0x%x = 0x%x\n", regs_ptr->address, saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	phy_regs.c
 * Description:	tests registers in the phy
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_phy_poke -l 1 -d 0x0 #exception
 * ip30_phy_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_phy_poke(__uint32_t argc, char **argv)
{
	phy_Registers	*regs_ptr = gz_phy_regs_ep;
	int data;
   	short bad_arg = 0; 
        int loop;
	short i;
	char *errStr;
	bool_t dataArgDefined = 0;

	loop = 1;
	data = 0xFFFFFFFF;

   	/* get the args */
   	argc--; argv++;

   	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {    
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
						return 1;
					}
				} 
				else {
		    			atob(&argv[0][2], &(data));
	          		}
				 	dataArgDefined = 1;
			break;
			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
						return 1;
					}
	          		} else {
		    			atob(&argv[0][2], &(loop));
	         		}
			break;
			default: 
               			bad_arg++; break;
		}
	    argc--; argv++;
	}

	if (!dataArgDefined) {
		msg_printf(INFO,"Arg -d is required\n");
		msg_printf(INFO, "Usage: ip30_phy_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "PHY Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _phy_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_phy_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_phy_peek -l 1
*/

bool_t
ip30_phy_peek(__uint32_t argc, char **argv)
{
	phy_Registers	*regs_ptr = gz_phy_regs_ep;
	short bad_arg = 0;
        int loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "PHY Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_phy_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_phy_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_phy_peek -l<loopcount>\n");
						return 1;
					}
	          		} else {
		    			atob(&argv[0][2], &(loop));
	         		}
			break;
	  		default: 
            			 bad_arg++; break;
	    	}
	    		argc--; argv++;
	}
	
	for (i = 1;i<=loop;i++) {
	   while (regs_ptr->name[0] != NULL) {
		_phy_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_phy_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*phy registers require direct write to address*/
void phy_ind_peek(__uint32_t offset, __uint32_t regAddr32, __uint32_t loop) {

   phy_Registers *phy_regs_ptr = gz_phy_regs_ep;
   __uint32_t mask;
   short i;
   phyregisters_t	readValue;
   bool_t found = 0;

   msg_printf(DBG,"Inside phy_ind_peek\n");

   regAddr32 = offset;
   while (  (!found) && (phy_regs_ptr->address != -1)  ) {
      if (phy_regs_ptr->address == regAddr32) found = 1;
      phy_regs_ptr++;
   }
   if (found) phy_regs_ptr--;
   mask = phy_regs_ptr->mask;
   msg_printf(DBG,"phy_regs_ptr->address = %x;\n", phy_regs_ptr->address);
   msg_printf(DBG,"phy_regs_ptr->mode = %x;\n", phy_regs_ptr->mode);
   msg_printf(DBG,"phy_regs_ptr->mask = %x;\n", phy_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (   (phy_regs_ptr->mode == GZ_READ_ONLY) || (phy_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      readValue = ReadPHYRegister(regAddr32, mask);
      msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void phy_ind_poke(__uint32_t offset, __uint32_t regAddr32, __uint32_t loop, __uint32_t data) {

   phy_Registers *phy_regs_ptr = gz_phy_regs_ep;
   __uint32_t mask;
   short i;
   phyregisters_t	readValue;
   bool_t found = 0;

  msg_printf(DBG,"Inside phy_ind_poke\n");

   regAddr32 = offset;
   while (  (!found) && (phy_regs_ptr->address != -1)  ) {
      if (phy_regs_ptr->address == regAddr32) found = 1;
      phy_regs_ptr++;
   }
   if (found) phy_regs_ptr--;
   mask = phy_regs_ptr->mask;
   msg_printf(DBG,"phy_regs_ptr->address = %x;\n", phy_regs_ptr->address);
   msg_printf(DBG,"phy_regs_ptr->mode = %x;\n", phy_regs_ptr->mode);
   msg_printf(DBG,"phy_regs_ptr->mask = %x;\n", phy_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (phy_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      WritePHYRegister(regAddr32, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
