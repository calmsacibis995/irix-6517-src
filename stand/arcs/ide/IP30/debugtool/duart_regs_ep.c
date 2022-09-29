 /* duart_regs.c : 
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

#ident "ide/IP30/duart/duart_regs.c:  $Revision: 1.3 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"

/*
 * Forward References: all in d_prototypes.h 
 */
 extern const short ArgLength;
 extern const short ArgLengthData;

/* duart registers */
duart_Registers	gz_duart_regs_ep[] = {
{"DU_SIO_PP_DCR",               DU_SIO_PP_DCR,          	GZ_READ_WRITE,
  DU_SIO_PP_DCR_MASK,           DU_SIO_PP_DCR_DEFAULT},

{"DU_SIO_UB_SCRPAD",            DU_SIO_UB_SCRPAD,               GZ_READ_WRITE,
DU_SIO_UB_SCRPAD_MASK,          DU_SIO_UB_SCRPAD_DEFAULT},

{"DU_SIO_UA_SCRPAD",            DU_SIO_UA_SCRPAD,               GZ_READ_WRITE,
DU_SIO_UA_SCRPAD_MASK,          DU_SIO_UA_SCRPAD_DEFAULT},

{"DU_SIO_UARTC",		DU_SIO_UARTC,		GZ_READ_ONLY,
DU_SIO_UARTC_MASK,		DU_SIO_UARTC_DEFAULT},

{"DU_SIO_KBDCG",		DU_SIO_KBDCG,		GZ_READ_ONLY,
DU_SIO_KBDCG_MASK,		DU_SIO_KBDCG_DEFAULT},

{"DU_SIO_PP_DATA",		DU_SIO_PP_DATA,		GZ_READ_ONLY,
DU_SIO_PP_DATA_MASK,		DU_SIO_PP_DATA_DEFAULT},

{"DU_SIO_PP_DSR",		DU_SIO_PP_DSR,		GZ_READ_ONLY,
DU_SIO_PP_DSR_MASK,		DU_SIO_PP_DSR_DEFAULT},

{"DU_SIO_PP_FIFA",		DU_SIO_PP_FIFA,		GZ_READ_ONLY,
DU_SIO_PP_FIFA_MASK,		DU_SIO_PP_FIFA_DEFAULT},

{"DU_SIO_PP_CFGB",		DU_SIO_PP_CFGB,		GZ_READ_ONLY,
DU_SIO_PP_CFGB_MASK,		DU_SIO_PP_CFGB_DEFAULT},

{"DU_SIO_PP_ECR",		DU_SIO_PP_ECR,		GZ_READ_ONLY,
DU_SIO_PP_ECR_MASK,		DU_SIO_PP_ECR_DEFAULT},

{"DU_SIO_RTCAD",		DU_SIO_RTCAD,		GZ_READ_ONLY,
DU_SIO_RTCAD_MASK,		DU_SIO_RTCAD_DEFAULT},

{"DU_SIO_RTCDAT",		DU_SIO_RTCDAT,		GZ_READ_ONLY,
DU_SIO_RTCDAT_MASK,		DU_SIO_RTCDAT_DEFAULT},

{"DU_SIO_UB_THOLD",		DU_SIO_UB_THOLD,		GZ_READ_ONLY,
DU_SIO_UB_THOLD_MASK,		DU_SIO_UB_THOLD_DEFAULT},

{"DU_SIO_UB_RHOLD",		DU_SIO_UB_RHOLD,		GZ_READ_ONLY,
DU_SIO_UB_RHOLD_MASK,		DU_SIO_UB_RHOLD_DEFAULT},

{"DU_SIO_UB_DIV_LSB",		DU_SIO_UB_DIV_LSB,		GZ_READ_ONLY,
DU_SIO_UB_DIV_LSB_MASK,		DU_SIO_UB_DIV_LSB_DEFAULT},

{"DU_SIO_UB_DIV_MSB",		DU_SIO_UB_DIV_MSB,		GZ_READ_ONLY,
DU_SIO_UB_DIV_MSB_MASK,		DU_SIO_UB_DIV_MSB_DEFAULT},

{"DU_SIO_UB_IENB",		DU_SIO_UB_IENB,			GZ_READ_ONLY,
DU_SIO_UB_IENB_MASK,		DU_SIO_UB_IENB_DEFAULT},

{"DU_SIO_UB_IIDENT",		DU_SIO_UB_IIDENT,		GZ_READ_ONLY,
DU_SIO_UB_IIDENT_MASK,		DU_SIO_UB_IIDENT_DEFAULT},

{"DU_SIO_UB_FIFOC",		DU_SIO_UB_FIFOC,		GZ_READ_ONLY,
DU_SIO_UB_FIFOC_MASK,		DU_SIO_UB_FIFOC_DEFAULT},

{"DU_SIO_UB_LINEC",		DU_SIO_UB_LINEC,		GZ_READ_ONLY,
DU_SIO_UB_LINEC_MASK,		DU_SIO_UB_LINEC_DEFAULT},

{"DU_SIO_UB_MODEMC",		DU_SIO_UB_MODEMC,		GZ_READ_ONLY,
DU_SIO_UB_MODEMC_MASK,		DU_SIO_UB_MODEMC_DEFAULT},

{"DU_SIO_UB_LINES",		DU_SIO_UB_LINES,		GZ_READ_ONLY,
DU_SIO_UB_LINES_MASK,		DU_SIO_UB_LINES_DEFAULT},

{"DU_SIO_UB_MODEMS",		DU_SIO_UB_MODEMS,		GZ_READ_ONLY,
DU_SIO_UB_MODEMS_MASK,		DU_SIO_UB_MODEMS_DEFAULT},

{"DU_SIO_UA_THOLD",		DU_SIO_UA_THOLD,		GZ_READ_ONLY,
DU_SIO_UA_THOLD_MASK,		DU_SIO_UA_THOLD_DEFAULT},

{"DU_SIO_UA_RHOLD",		DU_SIO_UA_RHOLD,		GZ_READ_ONLY,
DU_SIO_UA_RHOLD_MASK,		DU_SIO_UA_RHOLD_DEFAULT},

{"DU_SIO_UA_DIV_LSB",		DU_SIO_UA_DIV_LSB,		GZ_READ_ONLY,
DU_SIO_UA_DIV_LSB_MASK,		DU_SIO_UA_DIV_LSB_DEFAULT},

{"DU_SIO_UA_DIV_MSB",		DU_SIO_UA_DIV_MSB,		GZ_READ_ONLY,
DU_SIO_UA_DIV_MSB_MASK,		DU_SIO_UA_DIV_MSB_DEFAULT},

{"DU_SIO_UA_IENB",		DU_SIO_UA_IENB,			GZ_READ_ONLY,
DU_SIO_UA_IENB_MASK,		DU_SIO_UA_IENB_DEFAULT},

{"DU_SIO_UA_IIDENT",		DU_SIO_UA_IIDENT,		GZ_READ_ONLY,
DU_SIO_UA_IIDENT_MASK,		DU_SIO_UA_IIDENT_DEFAULT},

{"DU_SIO_UA_FIFOC",		DU_SIO_UA_FIFOC,		GZ_READ_ONLY,
DU_SIO_UA_FIFOC_MASK,		DU_SIO_UA_FIFOC_DEFAULT},

{"DU_SIO_UA_LINEC",		DU_SIO_UA_LINEC,		GZ_READ_ONLY,
DU_SIO_UA_LINEC_MASK,		DU_SIO_UA_LINEC_DEFAULT},

{"DU_SIO_UA_MODEMC",		DU_SIO_UA_MODEMC,		GZ_READ_ONLY,
DU_SIO_UA_MODEMC_MASK,		DU_SIO_UA_MODEMC_DEFAULT},

{"DU_SIO_UA_LINES",		DU_SIO_UA_LINES,		GZ_READ_ONLY,
DU_SIO_UA_LINES_MASK,		DU_SIO_UA_LINES_DEFAULT},

{"DU_SIO_UA_MODEMS",		DU_SIO_UA_MODEMS,		GZ_READ_ONLY,
DU_SIO_UA_MODEMS_MASK,		DU_SIO_UA_MODEMS_DEFAULT},

{"", -1, -1, -1 }
};



/*
 * Name:	_duart_regs_write.c
 * Description:	Performs Write/Reads on the duart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_duart_regs_write(duart_Registers *regs_ptr, unsigned char data)
{					
    msg_printf(DBG, "Write location (8) = 0x%x\n", regs_ptr->address);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    PIO_REG_WR_8(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_duart_regs_read
 * Description:	Performs Reads on the duart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_duart_regs_read(duart_Registers *regs_ptr)
{
    duartregisters_t	saved_reg_val;
					
    /* Save the current value */
    PIO_REG_RD_8(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (8) 0x%x = 0x%x\n", regs_ptr->address, saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	duart_regs.c
 * Description:	tests registers in the duart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_duart_poke -l 1 -d 0x0 #exception
 * ip30_duart_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_duart_poke(__uint32_t argc, char **argv)
{
	duart_Registers	*regs_ptr = gz_duart_regs_ep;
	int data;
   	short bad_arg = 0; 
        int loop;
	short i;
	char *errStr;
	bool_t dataArgDefined = 0;

	loop = 1;
	data = 0xFF;

   	/* get the args */
   	argc--; argv++;

   	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {    
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
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
					msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
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
		msg_printf(INFO, "Usage: ip30_duart_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "Duart Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _duart_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_duart_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_duart_peek -l 1
*/

bool_t
ip30_duart_peek(__uint32_t argc, char **argv)
{
	duart_Registers	*regs_ptr = gz_duart_regs_ep;
	short bad_arg = 0;
        int loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "Duart Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_duart_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_duart_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_duart_peek -l<loopcount>\n");
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
		_duart_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_duart_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*duart registers require direct write to address*/
void duart_ind_peek(int offset, int regAddr32, int loop) {

   duart_Registers *duart_regs_ptr = gz_duart_regs_ep;
   unsigned char mask;
   short i;
   duartregisters_t	readValue;
   bool_t found = 0;

   msg_printf(DBG,"Inside duart_ind_peek\n");

   regAddr32 = offset;
   while (  (!found) && (duart_regs_ptr->address != -1)  ) {
      if (duart_regs_ptr->address == regAddr32) found = 1;
      duart_regs_ptr++;
   }
   if (found) duart_regs_ptr--;
   mask = duart_regs_ptr->mask;
   msg_printf(DBG,"duart_regs_ptr->address = %x;\n", duart_regs_ptr->address);
   msg_printf(DBG,"duart_regs_ptr->mode = %x;\n", duart_regs_ptr->mode);
   msg_printf(DBG,"duart_regs_ptr->mask = %x;\n", duart_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (   (duart_regs_ptr->mode == GZ_READ_ONLY) || (duart_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_RD_8(regAddr32, mask, readValue);
      msg_printf(INFO,"Read Back Value (8) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void duart_ind_poke(int offset, int regAddr32, int loop, unsigned char data) {

   duart_Registers *duart_regs_ptr = gz_duart_regs_ep;
   unsigned char mask;
   short i;
   duartregisters_t	readValue;
   bool_t found = 0;

  msg_printf(DBG,"Inside duart_ind_poke\n");

   regAddr32 = offset;
   while (  (!found) && (duart_regs_ptr->address != -1)  ) {
      if (duart_regs_ptr->address == regAddr32) found = 1;
      duart_regs_ptr++;
   }
   if (found) duart_regs_ptr--;
   mask = duart_regs_ptr->mask;
   msg_printf(DBG,"duart_regs_ptr->address = %x;\n", duart_regs_ptr->address);
   msg_printf(DBG,"duart_regs_ptr->mode = %x;\n", duart_regs_ptr->mode);
   msg_printf(DBG,"duart_regs_ptr->mask = %x;\n", duart_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (duart_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_WR_8(regAddr32, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
