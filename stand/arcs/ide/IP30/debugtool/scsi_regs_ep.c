 /* scsi_regs.c : 
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

#ident "ide/IP30/scsi/scsi_regs.c:  $Revision: 1.4 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"
#include "d_scsiregs.h"
/*
 * Forward References: all in d_prototypes.h 
 */

enum {Eightbits, Sixteenbits, ThirtyTwobits, SixtyFourbits};
extern const short ArgLength;
extern const short ArgLengthData;

/* scsi registers */
scsi_Registers	gz_scsi_regs_ep[] = {

{"SCSI_VENDOR_ID",			SCSI_VENDOR_ID,			GZ_READ_ONLY,
SCSI_VENDOR_ID_MASK,			SCSI_VENDOR_ID_DEFAULT,		Sixteenbits},

{"SCSI_DEVICE_ID",			SCSI_DEVICE_ID,			GZ_READ_ONLY,
SCSI_DEVICE_ID_MASK,			SCSI_DEVICE_ID_DEFAULT, 	Sixteenbits},

{"SCSI_COMMAND",                        SCSI_COMMAND,           GZ_READ_ONLY,
SCSI_COMMAND_MASK,                      SCSI_COMMAND_DEFAULT,         Sixteenbits},

{"SCSI_STATUS",                         SCSI_STATUS,            GZ_READ_ONLY,
SCSI_STATUS_MASK,                       SCSI_STATUS_DEFAULT,         Sixteenbits},

{"SCSI_REVISION_ID",			SCSI_REVISION_ID,		GZ_READ_ONLY,
SCSI_REVISION_ID_MASK,			SCSI_REVISION_ID_DEFAULT, Eightbits},

{"SCSI_CLASS_CODE",			SCSI_CLASS_CODE,		GZ_READ_ONLY,
SCSI_CLASS_CODE_MASK,			SCSI_CLASS_CODE_DEFAULT, Eightbits},

{"SCSI_CACHE_LINE_SIZE",                SCSI_CACHE_LINE_SIZE,           GZ_READ_ONLY,
SCSI_CACHE_LINE_SIZE_MASK,              SCSI_CACHE_LINE_SIZE_DEFAULT,   Eightbits},

{"SCSI_LATENCY_TIMER",                  SCSI_LATENCY_TIMER,             GZ_READ_ONLY,
SCSI_LATENCY_TIMER_MASK,                SCSI_LATENCY_TIMER_DEFAULT,   Eightbits},

{"SCSI_HEADER_TYPE",                    SCSI_HEADER_TYPE,               GZ_READ_ONLY,
SCSI_HEADER_TYPE_MASK,                  SCSI_HEADER_TYPE_DEFAULT,   Eightbits},

{"SCSI_IO_BASE_ADDRESS",		SCSI_IO_BASE_ADDRESS,		GZ_READ_ONLY,
SCSI_IO_BASE_ADDRESS_MASK,		SCSI_IO_BASE_ADDRESS_DEFAULT, ThirtyTwobits},

{"SCSI_MEMORY_BASE_ADDRESS",		SCSI_MEMORY_BASE_ADDRESS,	GZ_READ_ONLY,
SCSI_MEMORY_BASE_ADDRESS_MASK,		SCSI_MEMORY_BASE_ADDRESS_DEFAULT, ThirtyTwobits},

{"SCSI_ROM_BASE_ADDRESS",               SCSI_ROM_BASE_ADDRESS,          GZ_READ_ONLY,
SCSI_ROM_BASE_ADDRESS_MASK,             SCSI_ROM_BASE_ADDRESS_DEFAULT, ThirtyTwobits},

{"SCSI_INTR_LINE",                      SCSI_INTR_LINE,         GZ_READ_ONLY,
SCSI_INTR_LINE_MASK,                    SCSI_INTR_LINE_DEFAULT,   Eightbits},

{"SCSI_INTR_PIN",                       SCSI_INTR_PIN,          GZ_READ_ONLY,
SCSI_INTR_PIN_MASK,                     SCSI_INTR_PIN_DEFAULT,   Eightbits},

{"SCSI_MIN_GRANT",                      SCSI_MIN_GRANT,         GZ_READ_ONLY,
SCSI_MIN_GRANT_MASK,                    SCSI_MIN_GRANT_DEFAULT,   Eightbits},
/*hang after write
{"SCSI_MAX_LATENCY",                    SCSI_MAX_LATENCY,               GZ_READ_ONLY,
SCSI_MAX_LATENCY_MASK,                  SCSI_MAX_LATENCY_DEFAULT,   Eightbits},
*/
/*SCSI 0 DEVICE IO SPACE REGISTERS*/
{"SCSI_SEMAPHORE",                      SCSI_SEMAPHORE,                 GZ_READ_WRITE,
SCSI_SEMAPHORE_MASK,                    SCSI_SEMAPHORE_DEFAULT, Sixteenbits},

/*SCSI 1 DEVICE IO SPACE REGISTERS*/
{"SCSI1_SEMAPHORE",                     SCSI1_SEMAPHORE,                GZ_READ_WRITE,
SCSI1_SEMAPHORE_MASK,                   SCSI1_SEMAPHORE_DEFAULT, Sixteenbits},

{"", -1, -1, -1 }
};

extern void initSCSI(void);
extern void restoreSCSI(void);

bool_t
_scsi_regs_read(scsi_Registers *regs_ptr)
{
	scsiregisters_t	reg_val;
	unsigned char mask_char;
	unsigned char reg_val_char;
	unsigned short mask_short;
	unsigned short reg_val_short;

	switch (regs_ptr->size) {
		case Eightbits:
			mask_char = regs_ptr->mask;
     			PIO_REG_RD_8(regs_ptr->address, mask_char, reg_val_char);
    			msg_printf(INFO,"Read Addr (8) 0x%x = 0x%x\n", regs_ptr->address,reg_val_char );
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
		break;
		case Sixteenbits:
			mask_short = regs_ptr->mask;
		       	PIO_REG_RD_16(regs_ptr->address, mask_short, reg_val_short);
    			msg_printf(INFO,"Read Addr (16) 0x%x = 0x%x\n", regs_ptr->address,reg_val_short );
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
		break;
		case ThirtyTwobits:
		       	PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, reg_val);
    			msg_printf(INFO,"Read Addr (32) 0x%x = 0x%x\n", regs_ptr->address,reg_val );
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}
	
	return(0);
}

/*
 * Name:	_scsi_regs_read_write.c
 * Description:	Performs Write/Reads on the scsi Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_scsi_regs_write(scsi_Registers *regs_ptr, int data)
{
	scsiregisters_t	write_val, read_val;
	unsigned char mask_char;
	unsigned char read_val_char;
	unsigned short mask_short;
	unsigned short read_val_short;
	unsigned char saved_reg_val_char;
	unsigned short saved_reg_val_short;
	unsigned char write_val_char;
	unsigned short write_val_short;	

	switch (regs_ptr->size) {
		case Eightbits:
			mask_char = regs_ptr->mask;
			write_val_char = data;
			msg_printf(DBG, "Write location (8) = 0x%x\n", regs_ptr->address);
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
			PIO_REG_WR_8(regs_ptr->address, mask_char, write_val_char);
		break;
		case Sixteenbits:
			mask_short = regs_ptr->mask;
			write_val_short = data;
			msg_printf(DBG, "Write location (16) = 0x%x\n", regs_ptr->address);
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
			PIO_REG_WR_16(regs_ptr->address, mask_short, write_val_short);
		break;
		case ThirtyTwobits:
			msg_printf(DBG, "Write location (32) = 0x%x\n", regs_ptr->address);
    			msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    			PIO_REG_WR_32(regs_ptr->address, regs_ptr->mask, data);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}

    return(0);
}

/*
 * Name:	scsi_regs.c
 * Description:	tests registers in the scsi
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_scsi_poke -l 1 -d 0x0 #exception
 * ip30_scsi_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_scsi_poke(__uint32_t argc, char **argv)
{
	scsi_Registers	*regs_ptr = gz_scsi_regs_ep;
	int data;
   	short bad_arg = 0; 
        int loop;
	short i;
	char *errStr;
	bool_t dataArgDefined = 0;

	loop = 1;
	data = 0xFFFFFFFF;
	initSCSI();

   	/* get the args */
   	argc--; argv++;

   	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {    
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
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
					msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
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
		msg_printf(INFO, "Usage: ip30_scsi_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "SCSI Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _scsi_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_scsi_regs_ep;
		msg_printf(INFO, "\n");
	}
	restoreSCSI();   
	return 0;
}

/*
 * ip30_scsi_peek -l 1
*/

bool_t
ip30_scsi_peek(__uint32_t argc, char **argv)
{
	scsi_Registers	*regs_ptr = gz_scsi_regs_ep;
	short bad_arg = 0;
        int loop;
	short i;
	char *errStr;

	loop = 1;
	initSCSI();

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "SCSI Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_scsi_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_scsi_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_scsi_peek -l<loopcount>\n");
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
		_scsi_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_scsi_regs_ep;
		msg_printf(INFO, "\n");
	}

	restoreSCSI();
	return 0;
}

/*scsi registers require direct write to address*/
void scsi_ind_peek(int offset, int regAddr32, int loop) {

   scsi_Registers *scsi_regs_ptr = gz_scsi_regs_ep;
   int mask;
   short i;
   int	readValue;
   bool_t found = 0;
   unsigned char reg_val_char;
   short reg_val_short;

   msg_printf(DBG,"Inside scsi_ind_peek\n");

   regAddr32 = offset;
   while (  (!found) && (scsi_regs_ptr->address != -1)  ) {
      if (scsi_regs_ptr->address == regAddr32) found = 1;
      scsi_regs_ptr++;
   }
   if (found) scsi_regs_ptr--;
   mask = scsi_regs_ptr->mask;
   msg_printf(DBG,"scsi_regs_ptr->address = %x;\n", scsi_regs_ptr->address);
   msg_printf(DBG,"scsi_regs_ptr->mode = %x;\n", scsi_regs_ptr->mode);
   msg_printf(DBG,"scsi_regs_ptr->mask = %x;\n", scsi_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   msg_printf(DBG,"scsi_regs_ptr->size = %x;\n", scsi_regs_ptr->size);
   if (   (scsi_regs_ptr->mode == GZ_READ_ONLY) || (scsi_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
	switch (scsi_regs_ptr->size) {
		case Eightbits:
     			PIO_REG_RD_8(regAddr32, mask, reg_val_char);
    			msg_printf(INFO,"Read Addr (8) 0x%x = 0x%x\n", regAddr32,reg_val_char );
    			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		case Sixteenbits:
		       	PIO_REG_RD_16(regAddr32, mask, reg_val_short);
    			msg_printf(INFO,"Read Addr (16) 0x%x = 0x%x\n", regAddr32,reg_val_short );
    			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		case ThirtyTwobits:
      			PIO_REG_RD_32(regAddr32, mask, readValue);
      			msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}
   }
}

void scsi_ind_poke(int offset, int regAddr32, int loop, int data) {

   scsi_Registers *scsi_regs_ptr = gz_scsi_regs_ep;
   int mask;
   short i;
   scsiregisters_t	readValue;
   bool_t found = 0;
   unsigned char data8;
   short data16;

  msg_printf(DBG,"Inside scsi_ind_poke\n");

   regAddr32 = offset;
   while (  (!found) && (scsi_regs_ptr->address != -1)  ) {
      if (scsi_regs_ptr->address == regAddr32) found = 1;
      scsi_regs_ptr++;
   }
   if (found) scsi_regs_ptr--;
   mask = scsi_regs_ptr->mask;
   msg_printf(DBG,"scsi_regs_ptr->address = %x;\n", scsi_regs_ptr->address);
   msg_printf(DBG,"scsi_regs_ptr->mode = %x;\n", scsi_regs_ptr->mode);
   msg_printf(DBG,"scsi_regs_ptr->mask = %x;\n", scsi_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (scsi_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
	switch (scsi_regs_ptr->size) {
		case Eightbits:
      			PIO_REG_WR_8(regAddr32, mask, data8);
      			msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		case Sixteenbits:
      			PIO_REG_WR_16(regAddr32, mask, data16);
      			msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		case ThirtyTwobits:
      			PIO_REG_WR_32(regAddr32, mask, data);
      			msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      			msg_printf(DBG, "Mask = 0x%x\n", mask);
		break;
		default:
		msg_printf(INFO, "SW error\n");
	}
   }
}
