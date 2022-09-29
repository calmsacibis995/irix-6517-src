 /* ioc3_regs.c : 
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

#ident "ide/IP30/ioc3/ioc3_regs_ep.c:  $Revision: 1.3 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"
#include "d_ioc3.h"

/*
 * Forward References: all in d_prototypes.h 
 */
extern const short ArgLength;
extern const short ArgLengthData;

/* ioc3 registers */
ioc3_Registers	gz_ioc3_regs_ep[] = {
{"IOC3REGS_ID",			IOC3REGS_ID,		GZ_READ_ONLY,
IOC3REGS_ID_MASK,		IOC3REGS_ID_DEFAULT},

{"IOC3REGS_SCR",		IOC3REGS_SCR,		GZ_READ_ONLY,
IOC3REGS_SCR_MASK,		IOC3REGS_SCR_DEFAULT},

{"IOC3REGS_REV",		IOC3REGS_REV,		GZ_READ_ONLY,
IOC3REGS_REV_MASK,		IOC3REGS_REV_DEFAULT},

{"IOC3REGS_LAT",		IOC3REGS_LAT,		GZ_READ_ONLY,
IOC3REGS_LAT_MASK,		IOC3REGS_LAT_DEFAULT},

{"IOC3REGS_ADDR",		IOC3REGS_ADDR,		GZ_READ_ONLY,
IOC3REGS_ADDR_MASK,		IOC3REGS_ADDR_DEFAULT},

{"IOC3REGS_ADDR_L",		IOC3REGS_ADDR_L,	GZ_READ_ONLY,
IOC3REGS_ADDR_L_MASK,		IOC3REGS_ADDR_L_DEFAULT},

{"IOC3REGS_ADDR_H",		IOC3REGS_ADDR_H,	GZ_READ_ONLY,
IOC3REGS_ADDR_H_MASK,		IOC3REGS_ADDR_H_DEFAULT},

{"IOC3REGS_IR",			IOC3REGS_IR,		GZ_READ_ONLY,
IOC3REGS_IR_MASK,		IOC3REGS_IR_DEFAULT},

{"IOC3REGS_IES",		IOC3REGS_IES,		GZ_READ_ONLY,
IOC3REGS_IES_MASK,		IOC3REGS_IES_DEFAULT},

{"IOC3REGS_IEC",		IOC3REGS_IEC,		GZ_READ_ONLY,
IOC3REGS_IEC_MASK,		IOC3REGS_IEC_DEFAULT},

{"IOC3REGS_CR",			IOC3REGS_CR,		GZ_READ_ONLY,
IOC3REGS_CR_MASK,		IOC3REGS_CR_DEFAULT},
	
{"IOC3REGS_OUT",			IOC3REGS_OUT,		GZ_READ_ONLY,
IOC3REGS_OUT_MASK,			IOC3REGS_OUT_DEFAULT},

{"IOC3REGS_MCR",			IOC3REGS_MCR,			GZ_READ_ONLY,
IOC3REGS_MCR_MASK,			IOC3REGS_MCR_DEFAULT},

{"IOC3REGS_S",				IOC3REGS_S,		GZ_READ_ONLY,
IOC3REGS_S_MASK,			IOC3REGS_S_DEFAULT},

{"IOC3REGS_C",				IOC3REGS_C,		GZ_READ_ONLY,
IOC3REGS_C_MASK,			IOC3REGS_C_DEFAULT},

{"IOC3REGS_GPDR",			IOC3REGS_GPDR,			GZ_READ_ONLY,
IOC3REGS_GPDR_MASK,			IOC3REGS_GPDR_DEFAULT},

{"IOC3REGS_0",				IOC3REGS_0,		GZ_READ_ONLY,
IOC3REGS_0_MASK,			IOC3REGS_0_DEFAULT},

{"IOC3REGS_PPBR_H_A",			IOC3REGS_PPBR_H_A,		GZ_READ_WRITE,
IOC3REGS_PPBR_H_A_MASK,			IOC3REGS_PPBR_H_A_DEFAULT},

{"IOC3REGS_PPBR_L_A",			IOC3REGS_PPBR_L_A,		GZ_READ_WRITE,
IOC3REGS_PPBR_L_A_MASK,			IOC3REGS_PPBR_L_A_DEFAULT},

{"IOC3REGS_PPCR_A",			IOC3REGS_PPCR_A,		GZ_READ_WRITE,
IOC3REGS_PPCR_A_MASK,			IOC3REGS_PPCR_A_DEFAULT},

{"IOC3REGS_PPCR",			IOC3REGS_PPCR,			GZ_READ_ONLY,
IOC3REGS_PPCR_MASK,			IOC3REGS_PPCR_DEFAULT},

{"IOC3REGS_PPBR_H_B",			IOC3REGS_PPBR_H_B,		GZ_READ_WRITE,
IOC3REGS_PPBR_H_B_MASK,			IOC3REGS_PPBR_H_B_DEFAULT},

{"IOC3REGS_PPBR_L_B",			IOC3REGS_PPBR_L_B,		GZ_READ_WRITE,
IOC3REGS_PPBR_L_B_MASK,			IOC3REGS_PPBR_L_B_DEFAULT},

{"IOC3REGS_PPCR_B",			IOC3REGS_PPCR_B,		GZ_READ_WRITE,
IOC3REGS_PPCR_B_MASK,			IOC3REGS_PPCR_B_DEFAULT},

{"IOC3REGS_KM_CSR",			IOC3REGS_KM_CSR,		GZ_READ_ONLY,
IOC3REGS_KM_CSR_MASK,			IOC3REGS_KM_CSR_DEFAULT},

{"IOC3REGS_K_RD",			IOC3REGS_K_RD,			GZ_READ_ONLY,
IOC3REGS_K_RD_MASK,			IOC3REGS_K_RD_DEFAULT},

{"IOC3REGS_M_RD",			IOC3REGS_M_RD,			GZ_READ_ONLY,
IOC3REGS_M_RD_MASK,			IOC3REGS_M_RD_DEFAULT},

{"IOC3REGS_K_WD",			IOC3REGS_K_WD,			GZ_READ_ONLY,
IOC3REGS_K_WD_MASK,			IOC3REGS_K_WD_DEFAULT},

{"IOC3REGS_M_WD",			IOC3REGS_M_WD,			GZ_READ_ONLY,
IOC3REGS_M_WD_MASK,			IOC3REGS_M_WD_DEFAULT},

{"IOC3REGS_SBBR_H",			IOC3REGS_SBBR_H,		GZ_READ_WRITE,
IOC3REGS_SBBR_H_MASK,			IOC3REGS_SBBR_H_DEFAULT},

{"IOC3REGS_SBBR_L",			IOC3REGS_SBBR_L,		GZ_READ_WRITE,
IOC3REGS_SBBR_L_MASK,			IOC3REGS_SBBR_L_DEFAULT},

{"IOC3REGS_SSCR_A",			IOC3REGS_SSCR_A,		GZ_READ_ONLY,
IOC3REGS_SSCR_A_MASK,			IOC3REGS_SSCR_A_DEFAULT},

{"IOC3REGS_STPIR_A",			IOC3REGS_STPIR_A,		GZ_READ_ONLY,
IOC3REGS_STPIR_A_MASK,			IOC3REGS_STPIR_A_DEFAULT},

{"IOC3REGS_STCIR_A",			IOC3REGS_STCIR_A,		GZ_READ_ONLY,
IOC3REGS_STCIR_A_MASK,			IOC3REGS_STCIR_A_DEFAULT},

{"IOC3REGS_SRPIR_A",			IOC3REGS_SRPIR_A,		GZ_READ_WRITE,
IOC3REGS_SRPIR_A_MASK,			IOC3REGS_SRPIR_A_DEFAULT},

{"IOC3REGS_SRCIR_A",			IOC3REGS_SRCIR_A,		GZ_READ_WRITE,
IOC3REGS_SRCIR_A_MASK,			IOC3REGS_SRCIR_A_DEFAULT},

{"IOC3REGS_SRTR_A",			IOC3REGS_SRTR_A,		GZ_READ_WRITE,
IOC3REGS_SRTR_A_MASK,			IOC3REGS_SRTR_A_DEFAULT},

{"IOC3REGS_SHADOW_A",			IOC3REGS_SHADOW_A,		GZ_READ_ONLY,
IOC3REGS_SHADOW_A_MASK,			IOC3REGS_SHADOW_A_DEFAULT},

{"IOC3REGS_SSCR_B",			IOC3REGS_SSCR_B,		GZ_READ_ONLY,
IOC3REGS_SSCR_B_MASK,			IOC3REGS_SSCR_B_DEFAULT},

{"IOC3REGS_STPIR_B",			IOC3REGS_STPIR_B,		GZ_READ_ONLY,
IOC3REGS_STPIR_B_MASK,			IOC3REGS_STPIR_B_DEFAULT},

{"IOC3REGS_STCIR_B",			IOC3REGS_STCIR_B,		GZ_READ_ONLY,
IOC3REGS_STCIR_B_MASK,			IOC3REGS_STCIR_B_DEFAULT},

{"IOC3REGS_SRPIR_B",			IOC3REGS_SRPIR_B,		GZ_READ_WRITE,
IOC3REGS_SRPIR_B_MASK,			IOC3REGS_SRPIR_B_DEFAULT},

{"IOC3REGS_SRCIR_B",			IOC3REGS_SRCIR_B,		GZ_READ_WRITE,
IOC3REGS_SRCIR_B_MASK,			IOC3REGS_SRCIR_B_DEFAULT},

{"IOC3REGS_SRTR_B",			IOC3REGS_SRTR_B,		GZ_READ_WRITE,
IOC3REGS_SRTR_B_MASK,			IOC3REGS_SRTR_B_DEFAULT},

{"IOC3REGS_SHADOW_B",			IOC3REGS_SHADOW_B,		GZ_READ_ONLY,
IOC3REGS_SHADOW_B_MASK,			IOC3REGS_SHADOW_B_DEFAULT},

{"IOC3REGS_REG_OFF",			IOC3REGS_REG_OFF,		GZ_READ_ONLY,
IOC3REGS_REG_OFF_MASK,			IOC3REGS_REG_OFF_DEFAULT},

{"IOC3REGS_RAM_OFF",			IOC3REGS_RAM_OFF,		GZ_READ_ONLY,
IOC3REGS_RAM_OFF_MASK,			IOC3REGS_RAM_OFF_DEFAULT},

/*write causes hang on read*/
{"IOC3REGS_EMCR",			IOC3REGS_EMCR,			GZ_READ_ONLY,
IOC3REGS_EMCR_MASK,			IOC3REGS_EMCR_DEFAULT},

{"IOC3REGS_EISR",			IOC3REGS_EISR,			GZ_READ_ONLY,
IOC3REGS_EISR_MASK,			IOC3REGS_EISR_DEFAULT},

{"IOC3REGS_EIER",			IOC3REGS_EIER,			GZ_READ_ONLY,
IOC3REGS_EIER_MASK,			IOC3REGS_EIER_DEFAULT},

{"IOC3REGS_ERCSR",			IOC3REGS_ERCSR,			GZ_READ_ONLY,
IOC3REGS_ERCSR_MASK,			IOC3REGS_ERCSR_DEFAULT},

{"IOC3REGS_ERBR_H",			IOC3REGS_ERBR_H,		GZ_READ_WRITE,
IOC3REGS_ERBR_H_MASK,			IOC3REGS_ERBR_H_DEFAULT},

{"IOC3REGS_ERBR_L",			IOC3REGS_ERBR_L,		GZ_READ_WRITE,
IOC3REGS_ERBR_L_MASK,			IOC3REGS_ERBR_L_DEFAULT},

{"IOC3REGS_ERBAR",			IOC3REGS_ERBAR,			GZ_READ_WRITE,
IOC3REGS_ERBAR_MASK,			IOC3REGS_ERBAR_DEFAULT},

{"IOC3REGS_ERCIR",			IOC3REGS_ERCIR,			GZ_READ_WRITE,
IOC3REGS_ERCIR_MASK,			IOC3REGS_ERCIR_DEFAULT},

{"IOC3REGS_ERPIR",			IOC3REGS_ERPIR,			GZ_READ_ONLY,
IOC3REGS_ERPIR_MASK,			IOC3REGS_ERPIR_DEFAULT},

{"IOC3REGS_ERTR",			IOC3REGS_ERTR,			GZ_READ_WRITE,
IOC3REGS_ERTR_MASK,			IOC3REGS_ERTR_DEFAULT},

{"IOC3REGS_ETCSR",			IOC3REGS_ETCSR,			GZ_READ_ONLY,
IOC3REGS_ETCSR_MASK,			IOC3REGS_ETCSR_DEFAULT},

{"IOC3REGS_ERSR",			IOC3REGS_ERSR,			GZ_READ_ONLY,
IOC3REGS_ERSR_MASK,			IOC3REGS_ERSR_DEFAULT},

{"IOC3REGS_ETCDC",			IOC3REGS_ETCDC,			GZ_READ_WRITE,
IOC3REGS_ETCDC_MASK,			IOC3REGS_ETCDC_DEFAULT},

{"IOC3REGS_ETBR_H",			IOC3REGS_ETBR_H,		GZ_READ_WRITE,
IOC3REGS_ETBR_H_MASK,			IOC3REGS_ETBR_H_DEFAULT},

{"IOC3REGS_ETBR_L",			IOC3REGS_ETBR_L,		GZ_READ_WRITE,
IOC3REGS_ETBR_L_MASK,			IOC3REGS_ETBR_L_DEFAULT},

{"IOC3REGS_ETCIR",			IOC3REGS_ETCIR,			GZ_READ_WRITE,
IOC3REGS_ETCIR_MASK,			IOC3REGS_ETCIR_DEFAULT},

{"IOC3REGS_ETPIR",			IOC3REGS_ETPIR,			GZ_READ_WRITE,
IOC3REGS_ETPIR_MASK,			IOC3REGS_ETPIR_DEFAULT},

{"IOC3REGS_EBIR",			IOC3REGS_EBIR,			GZ_READ_WRITE,
IOC3REGS_EBIR_MASK,			IOC3REGS_EBIR_DEFAULT},

{"IOC3REGS_EMAR_H",			IOC3REGS_EMAR_H,		GZ_READ_WRITE,
IOC3REGS_EMAR_H_MASK,			IOC3REGS_EMAR_H_DEFAULT},

{"IOC3REGS_EMAR_L",			IOC3REGS_EMAR_L,		GZ_READ_WRITE,
IOC3REGS_EMAR_L_MASK,			IOC3REGS_EMAR_L_DEFAULT},

{"IOC3REGS_EHAR_H",			IOC3REGS_EHAR_H,		GZ_READ_WRITE,
IOC3REGS_EHAR_H_MASK,			IOC3REGS_EHAR_H_DEFAULT},

{"IOC3REGS_EHAR_L",			IOC3REGS_EHAR_L,		GZ_READ_WRITE,
IOC3REGS_EHAR_L_MASK,			IOC3REGS_EHAR_L_DEFAULT},

{"IOC3REGS_MICR",			IOC3REGS_MICR,			GZ_READ_ONLY,
IOC3REGS_MICR_MASK,			IOC3REGS_MICR_DEFAULT},

{"IOC3REGS_MIDR",			IOC3REGS_MIDR,			GZ_READ_ONLY,
IOC3REGS_MIDR_MASK,			IOC3REGS_MIDR_DEFAULT},

{"IOC3REGS_INT_OUT_P",			IOC3REGS_INT_OUT_P,		GZ_READ_ONLY,
IOC3REGS_INT_OUT_P_MASK,		IOC3REGS_INT_OUT_P_DEFAULT},

{"IOC3REGS_SSCR_A_P",			IOC3REGS_SSCR_A_P,		GZ_READ_ONLY,
IOC3REGS_SSCR_A_P_MASK,			IOC3REGS_SSCR_A_P_DEFAULT},

{"IOC3REGS_STPIR_A_P",			IOC3REGS_STPIR_A_P,		GZ_READ_WRITE,
IOC3REGS_STPIR_A_P_MASK,		IOC3REGS_STPIR_A_P_DEFAULT},

{"IOC3REGS_STCIR_A_P",			IOC3REGS_STCIR_A_P,		GZ_READ_WRITE,
IOC3REGS_STCIR_A_P_MASK,		IOC3REGS_STCIR_A_P_DEFAULT},

{"IOC3REGS_SRPIR_A_P",			IOC3REGS_SRPIR_A_P,		GZ_READ_ONLY,
IOC3REGS_SRPIR_A_P_MASK,		IOC3REGS_SRPIR_A_P_DEFAULT},

{"IOC3REGS_SRCIR_A_P",			IOC3REGS_SRCIR_A_P,		GZ_READ_ONLY,
IOC3REGS_SRCIR_A_P_MASK,		IOC3REGS_SRCIR_A_P_DEFAULT},

{"IOC3REGS_SRTR_A_P",			IOC3REGS_SRTR_A_P,		GZ_READ_ONLY,
IOC3REGS_SRTR_A_P_MASK,			IOC3REGS_SRTR_A_P_DEFAULT},

{"IOC3REGS_SHADOW_A_P",			IOC3REGS_SHADOW_A_P,		GZ_READ_ONLY,
IOC3REGS_SHADOW_A_P_MASK,		IOC3REGS_SHADOW_A_P_DEFAULT},

{"IOC3REGS_SSCR_B_P",			IOC3REGS_SSCR_B_P,		GZ_READ_ONLY,
IOC3REGS_SSCR_B_P_MASK,			IOC3REGS_SSCR_B_P_DEFAULT},

{"IOC3REGS_STPIR_B_P",			IOC3REGS_STPIR_B_P,		GZ_READ_WRITE,
IOC3REGS_STPIR_B_P_MASK,		IOC3REGS_STPIR_B_P_DEFAULT},

{"IOC3REGS_STCIR_B_P",			IOC3REGS_STCIR_B_P,		GZ_READ_WRITE,
IOC3REGS_STCIR_B_P_MASK,		IOC3REGS_STCIR_B_P_DEFAULT},

{"IOC3REGS_SRPIR_B_P",			IOC3REGS_SRPIR_B_P,		GZ_READ_ONLY,
IOC3REGS_SRPIR_B_P_MASK,		IOC3REGS_SRPIR_B_P_DEFAULT},

{"IOC3REGS_SRCIR_B_P",			IOC3REGS_SRCIR_B_P,		GZ_READ_ONLY,
IOC3REGS_SRCIR_B_P_MASK,		IOC3REGS_SRCIR_B_P_DEFAULT},

{"IOC3REGS_SRTR_B_P",			IOC3REGS_SRTR_B_P,		GZ_READ_ONLY,
IOC3REGS_SRTR_B_P_MASK,			IOC3REGS_SRTR_B_P_DEFAULT},

{"IOC3REGS_SHADOW_B_P",			IOC3REGS_SHADOW_B_P,		GZ_READ_ONLY,
IOC3REGS_SHADOW_B_P_MASK,		IOC3REGS_SHADOW_B_P_DEFAULT},

{"", -1, -1, -1 }
};



/*
 * Name:	_ioc3_regs_write.c
 * Description:	Performs Write/Reads on the ioc3 Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_ioc3_regs_write(ioc3_Registers *regs_ptr, int data)
{					
    msg_printf(DBG, "Write location (32) = 0x%x\n", regs_ptr->address);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    /* Write the test value */
    PIO_REG_WR_32(regs_ptr->address, regs_ptr->mask, data);

    return(0);
}

/*
 * Name:	_ioc3_regs_read
 * Description:	Performs Reads on the ioc3 Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */

bool_t
_ioc3_regs_read(ioc3_Registers *regs_ptr)
{
    ioc3registers_t	saved_reg_val;
					
    /* Save the current value */
    PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, saved_reg_val);
    msg_printf(INFO,"Read Addr (32) 0x%x = 0x%x\n", regs_ptr->address, saved_reg_val);
    msg_printf(DBG, "Mask = 0x%x\n", regs_ptr->mask);
    return(0);
}

/*
 * Name:	ioc3_regs.c
 * Description:	tests registers in the ioc3
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_ioc3_poke -l 1 -d 0x0 #exception
 * ip30_ioc3_poke -l 1 -d 0xFFFFFFFF 
 */

bool_t
ip30_ioc3_poke(__uint32_t argc, char **argv)
{
	ioc3_Registers	*regs_ptr = gz_ioc3_regs_ep;
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
					msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
					return 1;	
				}

				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(data));
		    			argc--; argv++;
					/*if converted string is not an legal hex number*/
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
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
					msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
					return 1;	
				}
	         		if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
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
		msg_printf(INFO, "Usage: ip30_ioc3_poke -l<loopcount> -d <data>\n");
		return 1;
	}
	
	for (i = 1;i<=loop;i++) {

	   /* give register info if it was not given in default checking */
    	   msg_printf(INFO, "IOC3 Register Write\n");
	   while (regs_ptr->name[0] != NULL) {
	      	if (regs_ptr->mode == GZ_READ_WRITE) {
		   _ioc3_regs_write(regs_ptr, data);
	       	}
	       	regs_ptr++;
	   }
		regs_ptr = gz_ioc3_regs_ep;
		msg_printf(INFO, "\n");
	}
	   
	return 0;
}

/*
 * ip30_ioc3_peek -l 1
*/

bool_t
ip30_ioc3_peek(__uint32_t argc, char **argv)
{
	ioc3_Registers	*regs_ptr = gz_ioc3_regs_ep;
	short bad_arg = 0;
        int loop;
	short i;
	char *errStr;

	loop = 1;

	/* give register info if it was not given in default checking */
    	msg_printf(INFO, "IOC3 Register Read\n");

	/* get the args */
   	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {            
  			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_ioc3_peek -l<loopcount>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_ioc3_peek -l<loopcount>\n");
					return 1;	
				}
	         		
				if (argv[0][2]=='\0') {
		    			errStr = atob(&argv[1][0], &(loop));
		    			argc--; argv++;
					if (errStr[0] != '\0') {
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_ioc3_peek -l<loopcount>\n");
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
		_ioc3_regs_read(regs_ptr); 
	       	regs_ptr++;
	   }
		regs_ptr = gz_ioc3_regs_ep;
		msg_printf(INFO, "\n");
	}

	return 0;
}

/*ioc3 registers require direct write to address*/
void ioc3_ind_peek(int offset, int regAddr32, int loop) {

   ioc3_Registers *ioc3_regs_ptr = gz_ioc3_regs_ep;
   int mask;
   short i;
   ioc3registers_t	readValue;
   bool_t found = 0;

   msg_printf(DBG,"Inside ioc3_ind_peek\n");

   regAddr32 = offset;
   while (  (!found) && (ioc3_regs_ptr->address != -1)  ) {
      if (ioc3_regs_ptr->address == regAddr32) found = 1;
      ioc3_regs_ptr++;
   }
   if (found) ioc3_regs_ptr--;
   mask = ioc3_regs_ptr->mask;
   msg_printf(DBG,"ioc3_regs_ptr->address = %x;\n", ioc3_regs_ptr->address);
   msg_printf(DBG,"ioc3_regs_ptr->mode = %x;\n", ioc3_regs_ptr->mode);
   msg_printf(DBG,"ioc3_regs_ptr->mask = %x;\n", ioc3_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (   (ioc3_regs_ptr->mode == GZ_READ_ONLY) || (ioc3_regs_ptr->mode == GZ_READ_WRITE) ) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_RD_32(regAddr32, mask, readValue);
      msg_printf(INFO,"Read Back Value (32) = 0x%x\n", readValue);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}

void ioc3_ind_poke(int offset, int regAddr32, int loop, int data) {

   ioc3_Registers *ioc3_regs_ptr = gz_ioc3_regs_ep;
   int mask;
   short i;
   ioc3registers_t	readValue;
   bool_t found = 0;

  msg_printf(DBG,"Inside ioc3_ind_poke\n");

   regAddr32 = offset;
   while (  (!found) && (ioc3_regs_ptr->address != -1)  ) {
      if (ioc3_regs_ptr->address == regAddr32) found = 1;
      ioc3_regs_ptr++;
   }
   if (found) ioc3_regs_ptr--;
   mask = ioc3_regs_ptr->mask;
   msg_printf(DBG,"ioc3_regs_ptr->address = %x;\n", ioc3_regs_ptr->address);
   msg_printf(DBG,"ioc3_regs_ptr->mode = %x;\n", ioc3_regs_ptr->mode);
   msg_printf(DBG,"ioc3_regs_ptr->mask = %x;\n", ioc3_regs_ptr->mask);
   msg_printf(DBG,"regAddr32 = %x; loop = %x; offset = %x; mask = %x\n",regAddr32,loop,offset,mask);  
   if (ioc3_regs_ptr->mode == GZ_READ_WRITE) {}
   else {
      msg_printf(INFO,"Incorrect register address (probably no access) see documentation\n");
      return;
   }

   for (i = 1;i<=loop;i++) {
      PIO_REG_WR_32(regAddr32, mask, data);
      msg_printf(INFO,"Register write address = 0x%x\n", regAddr32);
      msg_printf(DBG, "Mask = 0x%x\n", mask);
   }
}
