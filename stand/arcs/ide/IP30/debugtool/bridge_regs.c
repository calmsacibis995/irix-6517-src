/*
 * br_regs.c
 *	
 *	bridge internal register tests
 *
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

#ident "ide/bridge/br_regs.c: $Revision: 1.6 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/xtalk/xwidget.h"  /* for widget identification fields */
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

Bridge_Regs	gz_bridge_regs_ep[] = {
{"BRIDGE_WID_ID",		BRIDGE_WID_ID,
  BRIDGE_WID_ID_MASK,		GZ_READ_ONLY},

{"BRIDGE_WID_STAT",		BRIDGE_WID_STAT,
  BRIDGE_WID_STAT_MASK,		GZ_READ_ONLY},

/* was GZ_READ_WRITE with single W/R pattern (0x55aa55aa), but is too */
/*  sensitive to the pattern written to it: made it GZ_READ_ONLY */
{"BRIDGE_WID_CONTROL",		BRIDGE_WID_CONTROL,
  BRIDGE_WID_CONTROL_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_REQ_TIMEOUT",	BRIDGE_WID_REQ_TIMEOUT,
  BRIDGE_WID_REQ_TIMEOUT_MASK,	GZ_READ_WRITE},

/* XXX is this really safe ?? */
{"BRIDGE_WID_LLP",		BRIDGE_WID_LLP,
  BRIDGE_WID_LLP_MASK,		GZ_READ_WRITE},

/* When read, this register will return a 0x00 after all previous transfers to 
the Bridge have completed. Not tested */
/* X at power-up, hence not tested */
{"BRIDGE_WID_RESP_UPPER",	BRIDGE_WID_RESP_UPPER,
  BRIDGE_WID_RESP_UPPER_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_RESP_LOWER",	BRIDGE_WID_RESP_LOWER,
  BRIDGE_WID_RESP_LOWER_MASK,	GZ_READ_ONLY},

{"BRIDGE_WID_TST_PIN_CTRL",	BRIDGE_WID_TST_PIN_CTRL,
  BRIDGE_WID_TST_PIN_CTRL_MASK,	GZ_READ_WRITE},

{"BRIDGE_DIR_MAP",		BRIDGE_DIR_MAP,
  BRIDGE_DIR_MAP_MASK,		GZ_READ_WRITE},

{"BRIDGE_ARB",			BRIDGE_ARB,
  BRIDGE_ARB_MASK,		GZ_READ_ONLY}, /*changed causes latter registers to hang sr*/

/* do a read-only test on the BRIDGE_NIC because most fields are counter */
/*  which start to count down at the time of the write. */
{"BRIDGE_NIC",			BRIDGE_NIC,
  BRIDGE_NIC_MASK,		GZ_READ_ONLY},

{"BRIDGE_PCI_BUS_TIMEOUT",	BRIDGE_PCI_BUS_TIMEOUT,
  BRIDGE_PCI_BUS_TIMEOUT_MASK,	GZ_READ_WRITE}, 

{"BRIDGE_PCI_CFG",		BRIDGE_PCI_CFG,
  BRIDGE_PCI_CFG_MASK,		GZ_READ_WRITE},

{"BRIDGE_PCI_ERR_UPPER",	BRIDGE_PCI_ERR_UPPER,
  BRIDGE_PCI_ERR_UPPER_MASK,	GZ_READ_ONLY},

{"BRIDGE_PCI_ERR_LOWER",	BRIDGE_PCI_ERR_LOWER,
  BRIDGE_PCI_ERR_LOWER_MASK,	GZ_READ_ONLY},

{"BRIDGE_INT_STATUS",		BRIDGE_INT_STATUS,
  BRIDGE_INT_STATUS_MASK,	GZ_READ_ONLY},

{"BRIDGE_INT_ENABLE",		BRIDGE_INT_ENABLE,
  BRIDGE_INT_ENABLE_MASK,	GZ_READ_WRITE},

/* write-only !*/

{"BRIDGE_INT_MODE",		BRIDGE_INT_MODE,
  BRIDGE_INT_MODE_MASK,		GZ_READ_WRITE},

{"BRIDGE_INT_DEVICE",		BRIDGE_INT_DEVICE,
  BRIDGE_INT_DEVICE_MASK,	GZ_READ_WRITE},

/* X at power-up, not tested */

{"BRIDGE_INT_ADDR(0)",		BRIDGE_INT_ADDR(0),
  BRIDGE_INT_ADDR_MASK(0),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(1)",		BRIDGE_INT_ADDR(1),
  BRIDGE_INT_ADDR_MASK(1),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(2)",		BRIDGE_INT_ADDR(2),
  BRIDGE_INT_ADDR_MASK(2),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(3)",		BRIDGE_INT_ADDR(3),
  BRIDGE_INT_ADDR_MASK(3),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(4)",		BRIDGE_INT_ADDR(4),
  BRIDGE_INT_ADDR_MASK(4),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(5)",		BRIDGE_INT_ADDR(5),
  BRIDGE_INT_ADDR_MASK(5),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(6)",		BRIDGE_INT_ADDR(6),
  BRIDGE_INT_ADDR_MASK(6),	GZ_READ_WRITE},

{"BRIDGE_INT_ADDR(7)",		BRIDGE_INT_ADDR(7),
  BRIDGE_INT_ADDR_MASK(7),	GZ_READ_WRITE},

/*causes exception in scsi register tests sr
{"BRIDGE_DEVICE(0)",		BRIDGE_DEVICE(0),
  BRIDGE_DEVICE_MASK(0),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(1)",		BRIDGE_DEVICE(1),
  BRIDGE_DEVICE_MASK(1),	GZ_READ_WRITE},

out: IP30 xtalk port 15 will always be a baseio bridge with
IOC3 using slots 2 and 4. So ide should not mess with slot 2 and 4 
{"BRIDGE_DEVICE(2)",		BRIDGE_DEVICE(2),
  BRIDGE_DEVICE_MASK(2),	GZ_READ_WRITE},

causes rad device reg tests to PANIC
{"BRIDGE_DEVICE(3)",		BRIDGE_DEVICE(3),
  BRIDGE_DEVICE_MASK(3),	GZ_READ_WRITE},

out: IP30 xtalk port 15 will always be a baseio bridge with
IOC3 using slots 2 and 4. So ide should not mess with slot 2 and 4 
{"BRIDGE_DEVICE(4)",		BRIDGE_DEVICE(4),
  BRIDGE_DEVICE_MASK(4),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(5)",		BRIDGE_DEVICE(5),
  BRIDGE_DEVICE_MASK(5),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(6)",		BRIDGE_DEVICE(6),
  BRIDGE_DEVICE_MASK(6),	GZ_READ_WRITE},

{"BRIDGE_DEVICE(7)",		BRIDGE_DEVICE(7),
  BRIDGE_DEVICE_MASK(7),	GZ_READ_WRITE},
*/

{"BRIDGE_WR_REQ_BUF(0)",	BRIDGE_WR_REQ_BUF(0),
  BRIDGE_WR_REQ_BUF_MASK(0),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(1)",	BRIDGE_WR_REQ_BUF(1),
  BRIDGE_WR_REQ_BUF_MASK(1),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(2)",	BRIDGE_WR_REQ_BUF(2),
  BRIDGE_WR_REQ_BUF_MASK(2),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(3)",	BRIDGE_WR_REQ_BUF(3),
  BRIDGE_WR_REQ_BUF_MASK(3),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(4)",	BRIDGE_WR_REQ_BUF(4),
  BRIDGE_WR_REQ_BUF_MASK(4),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(5)",	BRIDGE_WR_REQ_BUF(5),
  BRIDGE_WR_REQ_BUF_MASK(5),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(6)",	BRIDGE_WR_REQ_BUF(6),
  BRIDGE_WR_REQ_BUF_MASK(6),	GZ_READ_ONLY},

{"BRIDGE_WR_REQ_BUF(7)",	BRIDGE_WR_REQ_BUF(7),
  BRIDGE_WR_REQ_BUF_MASK(7),	GZ_READ_ONLY},

{"", -1, -1, -1 }
};

/*
 * Name:	_br_regs_write.c
 * Description:	Performs Write/Reads on the Bridge Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_br_regs_write(Bridge_Regs *regs_ptr, long long data)
{					
    msg_printf(DBG, "Write location (64) = 0x%x\n", BRIDGE_BASE+(regs_ptr->address));
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    BR_REG_WR_32(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_br_regs_read
 * Description:	Performs Reads on the Bridge Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */


bool_t
_br_regs_read(Bridge_Regs *regs_ptr)
{
    bridgereg_t	saved_reg_val;
					
    /* Save the current value */
    BR_REG_RD_32(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (64) 0x%x = 0x%x\n", BRIDGE_BASE+(regs_ptr->address), saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	br_regs.c
 * Description:	tests registers in the Bridge as a group
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_bridge_poke -l 1 -d 0x0 #exception
 * ip30_bridge_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_bridge_poke(__uint32_t argc, char **argv)
{
	Bridge_Regs	*regs_ptr = gz_bridge_regs_ep;
	long long data;
   	short bad_arg = 0; 
  	long long loop;
	short i;
	char *errStr;
	bool_t dataArgDefined = 0;

	loop = 1;
	data = 0xFFFFFFFFFFFFFFFF;


  	/* get the args */
  	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {    
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
						return 1;
					}
				} 
				else {
		    			atob_L(&argv[0][2], &(data));
	          		}
				 	dataArgDefined = 1;
			break;
			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
						return 1;
					}
	          		} else {
		    			atob_L(&argv[0][2], &(loop));
	         		}
			break;
			default: 
               			bad_arg++; break;
		}
	    argc--; argv++;
	}

	if (!dataArgDefined) {
		msg_printf(INFO,"Arg -d is required\n");
		msg_printf(INFO, "Usage: ip30_bridge_poke -l<loopcount> -d <data>\n");
		return 1;
	}

	for (i = 1;i<=loop;i++) {
           /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "Bridge Register Write\n");
	   	while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _br_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_bridge_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_bridge_peek -l 1
*/

bool_t
ip30_bridge_peek(__uint32_t argc, char **argv)
{
	Bridge_Regs	*regs_ptr = gz_bridge_regs_ep;
	short bad_arg = 0;
  	long long loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
  	msg_printf(INFO, "Bridge Register Read\n");

	/* get the args */
  	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_bridge_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_bridge_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_bridge_peek -l<loopcount>\n");
						return 1;
					}
	          		} else {
		    			atob_L(&argv[0][2], &(loop));
	         		}
			break;
	  		default: 
            			 bad_arg++; break;
	    	}
	    		argc--; argv++;
	}
	
	for (i = 1;i<=loop;i++) {
	   while (regs_ptr->name[0] != NULL) {
		_br_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_bridge_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*heart registers require direct write to address*/
void bridge_ind_peek(int offset, long long regAddr64, int loop) {

   Bridge_Regs *bridge_regs_ptr = gz_bridge_regs_ep;
   long mask;
   short i;
   bridgereg_t	readValue;
   bool_t found = 0;

   regAddr64 = offset;
   /*find register in table*/
   while (  (!found) && (bridge_regs_ptr->address != -1)  ) {
      if (bridge_regs_ptr->address == regAddr64) found = 1;
      bridge_regs_ptr++;
   }
   if (found) bridge_regs_ptr--;
   mask = bridge_regs_ptr->mask;
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",BRIDGE_BASE+bridge_regs_ptr->address,loop,offset,mask);
   /*do operation if mode is correct*/
   if (   (bridge_regs_ptr->mode == GZ_READ_ONLY) || (bridge_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }  

   for (i = 1;i<=loop;i++) {
      BR_REG_RD_32(regAddr64, mask, readValue);
      msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void
bridge_ind_poke(int offset, long long regAddr64, int loop, long long data)
{
   Bridge_Regs *bridge_regs_ptr = gz_bridge_regs_ep;
   long mask;
   short i;
   bridgereg_t	readValue;
   bool_t found = 0;

   regAddr64 = offset;
   while (  (!found) && (bridge_regs_ptr->address != -1)  ) {
      if (bridge_regs_ptr->address == regAddr64) found = 1;
      bridge_regs_ptr++;
   }
   if (found) bridge_regs_ptr--;
   mask = bridge_regs_ptr->mask;
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",BRIDGE_BASE+bridge_regs_ptr->address,loop,offset,mask);
   if (bridge_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably read only) see documentation\n");
      return;
   }  

   for (i = 1;i<=loop;i++) {
      BR_REG_WR_32(regAddr64, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr64);
      msg_printf(DBG, "Mask = 0x%x\n", mask);  
   }
}
