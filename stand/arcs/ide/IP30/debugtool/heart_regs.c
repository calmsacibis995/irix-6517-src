/*
 * hr_regs.c : 
 *	Heart Register read/write Tests
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

#ident "ide/IP30/heart/hr_regs.c:  $Revision: 1.6 $"

/*
 * hr_regs.c - Heart Reister read/write Tests
 */
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

/* heart registers */
Heart_Regs	gz_heart_regs_ep[] = {
{"HEART_WID_ID",		HEART_WID_ID,
  HEART_WID_ID_MASK,		GZ_READ_ONLY },

{"HEART_WID_STAT",		HEART_WID_STAT,
  HEART_WID_STAT_MASK,		GZ_READ_ONLY },

{"HEART_WID_ERR_UPPER",		HEART_WID_ERR_UPPER,
  HEART_WID_ERR_UPPER_MASK,	GZ_READ_ONLY },

{"HEART_WID_ERR_LOWER",		HEART_WID_ERR_LOWER,
  HEART_WID_ERR_LOWER_MASK,	GZ_READ_ONLY },

{"HEART_WID_REQ_TIMEOUT",	HEART_WID_REQ_TIMEOUT,
  HEART_WID_REQ_TO_MASK,	GZ_READ_WRITE },

/* the register is R/W but writing to it only clears the register */
{"HEART_WID_ERR_CMDWORD",	HEART_WID_ERR_CMDWORD,
  HEART_WID_ERR_CMDWORD_MASK,	GZ_READ_ONLY },

/* should this be written to?? XXX emulation exception: Data Bus error. */
{"HEART_WID_LLP",		HEART_WID_LLP,
  HEART_WID_LLP_MASK,		GZ_READ_ONLY },

{"HEART_WID_ERR_TYPE",		HEART_WID_ERR_TYPE,
  HEART_WID_ERR_TYPE_MASK,	GZ_READ_ONLY },

{"HEART_WID_ERR_MASK",		HEART_WID_ERR_MASK,
  HEART_WID_ERR_MASK_MASK,	GZ_READ_WRITE },

{"HEART_MODE",			HEART_MODE,
  HEART_MODE_MASK,		GZ_READ_ONLY },

{"HEART_WID_PIO_ERR_UPPER",	HEART_WID_PIO_ERR_UPPER,
  HEART_WID_PIO_ERR_UPPER_MASK,	GZ_READ_ONLY },

{"HEART_WID_PIO_ERR_LOWER",	HEART_WID_PIO_ERR_LOWER,
  HEART_WID_PIO_ERR_LOWER_MASK,	GZ_READ_ONLY },

{"HEART_WID_PIO_RTO_ADDR",	HEART_WID_PIO_RTO_ADDR,
  HEART_WID_PIO_RTO_ADDR_MASK,	GZ_READ_ONLY },

{"HEART_SDRAM_MODE",		HEART_SDRAM_MODE,
  HEART_SDRAM_MODE_MASK,	GZ_READ_ONLY },

{"HEART_MEM_REF",		HEART_MEM_REF,
  HEART_MEM_REF_MASK,		GZ_READ_ONLY },

{"HEART_MEM_ARB",		HEART_MEM_REQ_ARB,
  HEART_MEM_ARB_MASK,		GZ_READ_ONLY },

/* the other mem config reg can be modified as they are unused (sable only) XXX */
#ifdef SABLE
{"HEART_MEMCFG1",		HEART_MEMCFG(1),
  HEART_MEMCFG1_MASK,		GZ_READ_WRITE },

{"HEART_MEMCFG2",		HEART_MEMCFG(2),
  HEART_MEMCFG2_MASK,		GZ_READ_WRITE },

{"HEART_MEMCFG3",		HEART_MEMCFG(3),
  HEART_MEMCFG3_MASK,		GZ_READ_WRITE },
#endif

{"HEART_FC_MODE",		HEART_FC_MODE,
  HEART_FC_MODE_MASK,		GZ_READ_ONLY },

{"HEART_FC_LIMIT",		HEART_FC_TIMER_LIMIT,
  HEART_FC_LIMIT_MASK,		GZ_READ_ONLY },

{"HEART_FC0_ADDR",		HEART_FC_ADDR(0),
  HEART_FC0_ADDR_MASK,		GZ_READ_WRITE },

{"HEART_FC1_ADDR",		HEART_FC_ADDR(1),
  HEART_FC1_ADDR_MASK,		GZ_READ_WRITE },

{"HEART_FC0_COUNT",		HEART_FC_CR_CNT(0),
  HEART_FC0_COUNT_MASK,		GZ_READ_ONLY },

{"HEART_FC1_COUNT",		HEART_FC_CR_CNT(1),
  HEART_FC1_COUNT_MASK,		GZ_READ_ONLY },

{"HEART_FC0_TIMER",		HEART_FC_TIMER(0),
  HEART_FC0_TIMER_MASK,		GZ_READ_ONLY },

{"HEART_FC1_TIMER",		HEART_FC_TIMER(1),
  HEART_FC1_TIMER_MASK,		GZ_READ_ONLY },

{"HEART_STATUS",		HEART_STATUS,
  HEART_STATUS_MASK,		GZ_READ_ONLY },

{"HEART_BERR_ADDR",		HEART_BERR_ADDR,
  HEART_BERR_ADDR_MASK,		GZ_READ_ONLY },

{"HEART_BERR_MISC",		HEART_BERR_MISC,
  HEART_BERR_MISC_MASK,		GZ_READ_ONLY },

{"HEART_MEM_ERR",		HEART_MEMERR_ADDR,
  HEART_MEM_ERR_MASK,		GZ_READ_ONLY },

{"HEART_BAD_MEM",		HEART_MEMERR_DATA,
  HEART_BAD_MEM_MASK,		GZ_READ_ONLY },

{"HEART_PIU_ERR",		HEART_PIUR_ACC_ERR,
  HEART_PIU_ERR_MASK,		GZ_READ_ONLY },

{"HEART_MLAN_CLK_DIV",		HEART_MLAN_CLK_DIV,
  HEART_MLAN_CLK_DIV_MASK,	GZ_READ_WRITE },

/* theoretically WRITABLE but does not retain value: made RO */
{"HEART_MLAN_CTL",		HEART_MLAN_CTL,
  HEART_MLAN_CTL_MASK,		GZ_READ_ONLY },

{"HEART_IMR(0)",		HEART_IMR(0),
  HEART_IMR0_MASK,		GZ_READ_WRITE },

{"HEART_IMR(1)",		HEART_IMR(1),
  HEART_IMR1_MASK,		GZ_READ_WRITE },

{"HEART_IMR(2)",		HEART_IMR(2),
  HEART_IMR2_MASK,		GZ_READ_WRITE },

{"HEART_IMR(3)",		HEART_IMR(3),
  HEART_IMR3_MASK,		GZ_READ_WRITE },

{"HEART_CAUSE",			HEART_CAUSE,
  HEART_CAUSE_MASK,		GZ_READ_ONLY },

{"HEART_COMPARE",		HEART_COMPARE,
  HEART_COMPARE_MASK,		GZ_READ_WRITE },

{"", -1, -1, -1 }
};



/*
 * Name:	_hr_regs_write.c
 * Description:	Performs Write/Reads on the Heart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_hr_regs_write(Heart_Regs *regs_ptr, long long data)
{					
    msg_printf(DBG, "Write location (64) = 0x%x\n", regs_ptr->address);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    PIO_REG_WR_64(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_hr_regs_read
 * Description:	Performs Reads on the Heart Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_hr_regs_read(Heart_Regs *regs_ptr)
{
    heartreg_t	saved_reg_val;
					
    /* Save the current value */
    PIO_REG_RD_64(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (64) 0x%x = 0x%x\n", regs_ptr->address, saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	hr_regs.c
 * Description:	tests registers in the Heart
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_heart_poke -l 1 -d 0x0 #exception
 * ip30_heart_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_heart_poke(__uint32_t argc, char **argv)
{
	Heart_Regs	*regs_ptr = gz_heart_regs_ep;
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
					msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
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
					msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
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
		msg_printf(INFO, "Usage: ip30_heart_poke -l<loopcount> -d <data>\n");
		return 1;
	}

	msg_printf(DBG, "loop = %x\n", loop);
	if (data == 0)  {
	   msg_printf(INFO, "Cannot write 0x0 to heart registers\n");
	   return 0;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "Heart Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _hr_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_heart_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_heart_peek -l 1
*/

bool_t
ip30_heart_peek(__uint32_t argc, char **argv)
{
	Heart_Regs	*regs_ptr = gz_heart_regs_ep;
	short bad_arg = 0;
        long long loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "Heart Register Read\n");

	/* get the args */
   	argc--; argv++;

  	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_heart_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_heart_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob_L(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_heart_peek -l<loopcount>\n");
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
		_hr_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_heart_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*heart registers require direct write to address*/
void heart_ind_peek(int offset, long long regAddr64, int loop) {

   Heart_Regs *heart_regs_ptr = gz_heart_regs_ep;
   long mask;
   short i;
   heartreg_t	readValue;
   bool_t found = 0;

   msg_printf(DBG,"Inside heart_ind_peek\n");

   regAddr64 = offset;
   while (  (!found) && (heart_regs_ptr->address != -1)  ) {
      if (heart_regs_ptr->address == regAddr64) found = 1;
      heart_regs_ptr++;
   }
   if (found) heart_regs_ptr--;
   mask = heart_regs_ptr->mask;
   msg_printf(DBG,"heart_regs_ptr->address = %x;\n", heart_regs_ptr->address);
   msg_printf(DBG,"heart_regs_ptr->mode = %x;\n", heart_regs_ptr->mode);
   msg_printf(DBG,"heart_regs_ptr->mask = %x;\n", heart_regs_ptr->mask);
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",regAddr64,loop,offset,mask);  
   if (   (heart_regs_ptr->mode == GZ_READ_ONLY) || (heart_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_RD_64(regAddr64, mask, readValue);
      msg_printf(INFO,"Read Back Value (64) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void heart_ind_poke(int offset, long long regAddr64, int loop, long long data) {

   Heart_Regs *heart_regs_ptr = gz_heart_regs_ep;
   long mask;
   short i;
   heartreg_t	readValue;
   bool_t found = 0;

  msg_printf(DBG,"Inside heart_ind_poke\n");

   regAddr64 = offset;
   while (  (!found) && (heart_regs_ptr->address != -1)  ) {
      if (heart_regs_ptr->address == regAddr64) found = 1;
      heart_regs_ptr++;
   }
   if (found) heart_regs_ptr--;
   mask = heart_regs_ptr->mask;
   msg_printf(DBG,"heart_regs_ptr->address = %x;\n", heart_regs_ptr->address);
   msg_printf(DBG,"heart_regs_ptr->mode = %x;\n", heart_regs_ptr->mode);
   msg_printf(DBG,"heart_regs_ptr->mask = %x;\n", heart_regs_ptr->mask);
   msg_printf(DBG,"regAddr64 = %x; loop = %x; offset = %x; mask = %x\n",regAddr64,loop,offset,mask);  
   if (heart_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_WR_64(regAddr64, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr64);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
