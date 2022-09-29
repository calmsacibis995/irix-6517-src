/*
 * x_regs.c : 
 *	Xbow Register read/write Tests
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
 
/* NOTE: partially run on sable, but the default values (in d_xbow.h) 
 *	need to be adjusted. XXX
 * Only the crossbow registers are tested. 
 * The link registers are read to give system status.
 */
#ident "ide/IP30/xbow/x_regs.c:  $Revision: 1.6 $"

/*
 * x_regs.c - Xbow Register read/write Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_xbow.h"
#include "sys/xtalk/xbow.h"

extern const short ArgLength;
extern const short ArgLengthData;

Xbow_Regs	gz_xbow_regs_ep[] = {

/*
 * crossbow registers
 */
{"XBOW_WID_ID",			XBOW_WID_ID,		GZ_READ_ONLY,
  XBOW_WID_ID_MASK},

{"XBOW_WID_STAT",		XBOW_WID_STAT,		GZ_READ_ONLY,
  XBOW_WID_STAT_MASK},

{"XBOW_WID_ERR_UPPER",		XBOW_WID_ERR_UPPER,	GZ_READ_ONLY,
  XBOW_WID_ERR_UPPER_MASK},

{"XBOW_WID_ERR_LOWER",		XBOW_WID_ERR_LOWER,	GZ_READ_ONLY,
  XBOW_WID_ERR_LOWER_MASK},

{"XBOW_WID_CONTROL",		XBOW_WID_CONTROL,	GZ_READ_WRITE,
  XBOW_WID_CONTROL_MASK},

{"XBOW_WID_REQ_TO",		XBOW_WID_REQ_TO,	GZ_READ_WRITE,
  XBOW_WID_REQ_TO_MASK},

{"XBOW_WID_INT_UPPER",		XBOW_WID_INT_UPPER,	GZ_READ_WRITE,
  XBOW_WID_INT_UPPER_MASK},

{"XBOW_WID_INT_LOWER",		XBOW_WID_INT_LOWER,	GZ_READ_WRITE,
  XBOW_WID_INT_LOWER_MASK},

/* writing to this register clears it, regardless of the written value */
{"XBOW_WID_ERR_CMDWORD",	XBOW_WID_ERR_CMDWORD,	GZ_READ_ONLY,
  XBOW_WID_ERR_CMDWORD_MASK},

{"XBOW_WID_LLP",		XBOW_WID_LLP,		GZ_READ_WRITE,
  XBOW_WID_LLP_MASK},

/* XBOW_WID_STAT_CLR is not tested as reading it clears it */

{"XBOW_WID_ARB_RELOAD",		XBOW_WID_ARB_RELOAD,	GZ_READ_WRITE,
  XBOW_WID_ARB_RELOAD_MASK},

/* 2 perf counters, according to released specs 9.0 */
{"XBOW_WID_PERF_CTR_A",		XBOW_WID_PERF_CTR_A,	GZ_READ_ONLY,
  XBOW_WID_PERF_CTR_A_MASK},

{"XBOW_WID_PERF_CTR_B",		XBOW_WID_PERF_CTR_B,	GZ_READ_ONLY,
  XBOW_WID_PERF_CTR_B_MASK},

{"XBOW_WID_NIC",		XBOW_WID_NIC,		GZ_READ_ONLY,
  XBOW_WID_NIC_MASK},

{"XB_LINK_IBUF_FLUSH(XBOW_PORT_8)", XB_LINK_IBUF_FLUSH(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_8)",	XB_LINK_CTRL(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_8)",	XB_LINK_STATUS(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_8)", XB_LINK_AUX_STATUS(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_8)", XB_LINK_ARB_UPPER(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_8)", XB_LINK_ARB_LOWER(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},

/* do not test XB_LINK_STATUS_CLR as it has side effects (reading clears */
/*	XB_LINK_STATUS and XB_LINK_AUX_STATUS) */

/* do not test XB_LINK_RESET as it has side effects */
/*	(writing resets the Xbow LLP's) */

/* link 9 */
{"XB_LINK_IBUF_FLUSH(XBOW_PORT_9)", XB_LINK_IBUF_FLUSH(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_9)",	XB_LINK_CTRL(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_9)",	XB_LINK_STATUS(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_9)", XB_LINK_AUX_STATUS(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_9)", XB_LINK_ARB_UPPER(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_9)", XB_LINK_ARB_LOWER(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},

/* link f */
{"XB_LINK_IBUF_FLUSH(XBOW_PORT_F)", XB_LINK_IBUF_FLUSH(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_F)",	XB_LINK_CTRL(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_F)",	XB_LINK_STATUS(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_F)", XB_LINK_AUX_STATUS(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_F)", XB_LINK_ARB_UPPER(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_F)", XB_LINK_ARB_LOWER(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},
{"", -1, -1, -1}
};


/*
 * Name:	_xb_regs_write.c
 * Description:	Performs Write/Reads on the Xbow Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_xb_regs_write(Xbow_Regs *regs_ptr, long long data)
{					
    msg_printf(DBG, "Write location (64) = 0x%x\n", XBOW_BASE+(regs_ptr->address));
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    XB_REG_WR_32(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_xb_regs_read
 * Description:	Performs Reads on the Xbow Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_xb_regs_read(Xbow_Regs *regs_ptr)
{
    xbowreg_t	saved_reg_val;
					
    /* Save the current value */
    XB_REG_RD_32(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (64) 0x%x = 0x%x\n", XBOW_BASE+(regs_ptr->address), saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	xb_regs.c
 * Description:	tests registers in the Xbow
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_xbow_poke -l 1 -d 0x0 #exception
 * ip30_xbow_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_xbow_poke(__uint32_t argc, char **argv)
{
	Xbow_Regs	*regs_ptr = gz_xbow_regs_ep;
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
					msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
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
					msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
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
		msg_printf(INFO, "Usage: ip30_xbow_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {
	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "Xbow Register Write\n");

	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _xb_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_xbow_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_xbow_peek -l 2
*/

bool_t
ip30_xbow_peek(__uint32_t argc, char **argv)
{
	Xbow_Regs	*regs_ptr = gz_xbow_regs_ep;
	short bad_arg = 0;
        long long loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "Xbow Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_xbow_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_xbow_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_xbow_peek -l<loopcount>\n");
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
		_xb_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_xbow_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

void xbow_ind_peek(int offset, long long regAddr64, int loop) {

   Xbow_Regs *xbow_regs_ptr = gz_xbow_regs_ep;
   long mask;
   short i;
   xbowreg_t	readValue;
   bool_t found = 0;

   regAddr64 = offset;
   while (  (!found) && (xbow_regs_ptr->address != -1)  ) {
      if (xbow_regs_ptr->address == regAddr64) found = 1;
      xbow_regs_ptr++;
   }
   if (found) xbow_regs_ptr--;
   mask = xbow_regs_ptr->mask;
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",XBOW_BASE+xbow_regs_ptr->address,loop,offset,mask);
   if (   (xbow_regs_ptr->mode == GZ_READ_ONLY) || (xbow_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      XB_REG_RD_32(regAddr64, mask, readValue);
      msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void xbow_ind_poke(int offset, long long regAddr64, int loop, long long data) {

   Xbow_Regs *xbow_regs_ptr = gz_xbow_regs_ep;
   long mask;
   short i;
   xbowreg_t	readValue;
   bool_t found = 0;

   regAddr64 = offset;
   while (  (!found) && (xbow_regs_ptr->address != -1)  ) {
      if (xbow_regs_ptr->address == regAddr64) found = 1;
      xbow_regs_ptr++;
   }
   if (found) xbow_regs_ptr--;
   mask = xbow_regs_ptr->mask;
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",XBOW_BASE+xbow_regs_ptr->address,loop,offset,mask);
   if (xbow_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably read only) see documentation\n");
      return;
   }  

   for (i = 1;i<=loop;i++) {
      XB_REG_WR_32(regAddr64, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr64);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
