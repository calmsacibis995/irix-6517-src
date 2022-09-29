/*  
 * ioc3_regs.c : 
 *	ioc3 Register read/write Tests
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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

#ident "ide/godzilla/ioc3/ioc3_regs.c:  $Revision: 1.10 $"

/*
 * ioc3_regs.c - ioc3 Register read/write Tests for configuration space
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_ioc3.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

#define EMCR_ARB_DIAG_IDLE      0x00200000
#define EMCR_RST                0x80000000

/*
 * Forward References: all in d_prototypes.h 
 */
extern void test(void);

/*::
 * Global Variables
 */
bool_t		exit_on_ioc3_error = 0;
__uint32_t	ioc3_error = 0;

ioc3_Registers	gd_ioc3_regs[] = {
{"IOC3REGS_ID",			IOC3REGS_ID,		GZ_READ_ONLY,
  IOC3REGS_ID_MASK,		IOC3REGS_ID_DEFAULT},

{"IOC3REGS_REV",		IOC3REGS_REV,		GZ_READ_ONLY,
  IOC3REGS_REV_MASK,		IOC3REGS_REV_DEFAULT},

/*Reads to gpdr return state of pins so do not write
{"IOC3REGS_GPDR",		IOC3REGS_GPDR,		GZ_READ_WRITE,
  IOC3REGS_GPDR_MASK,		IOC3REGS_GPDR_DEFAULT},
*/

{"IOC3REGS_PPBR_H_A",		IOC3REGS_PPBR_H_A,		GZ_READ_WRITE,
  IOC3REGS_PPBR_H_A_MASK,	IOC3REGS_PPBR_H_A_DEFAULT},

{"IOC3REGS_PPBR_H_B",		IOC3REGS_PPBR_H_B,		GZ_READ_WRITE,
  IOC3REGS_PPBR_H_B_MASK,	IOC3REGS_PPBR_H_B_DEFAULT},

{"IOC3REGS_PPBR_L_A",		IOC3REGS_PPBR_L_A,		GZ_READ_WRITE,
  IOC3REGS_PPBR_L_A_MASK,	IOC3REGS_PPBR_L_A_DEFAULT},

{"IOC3REGS_PPBR_L_B",		IOC3REGS_PPBR_L_B,		GZ_READ_WRITE,
  IOC3REGS_PPBR_L_B_MASK,	IOC3REGS_PPBR_L_B_DEFAULT},

{"IOC3REGS_PPCR_A",		IOC3REGS_PPCR_A,		GZ_READ_WRITE,
  IOC3REGS_PPCR_A_MASK,		IOC3REGS_PPCR_A_DEFAULT},

{"IOC3REGS_PPCR_B",		IOC3REGS_PPCR_B,		GZ_READ_WRITE,
  IOC3REGS_PPCR_B_MASK,		IOC3REGS_PPCR_B_DEFAULT},

{"IOC3REGS_SBBR_H",		IOC3REGS_SBBR_H,		GZ_READ_WRITE,
  IOC3REGS_SBBR_H_MASK,		IOC3REGS_SBBR_H_DEFAULT},

{"IOC3REGS_SBBR_L",		IOC3REGS_SBBR_L,		GZ_READ_WRITE,
  IOC3REGS_SBBR_L_MASK,		IOC3REGS_SBBR_L_DEFAULT},

{"IOC3REGS_SRCIR_A",		IOC3REGS_SRCIR_A,		GZ_READ_WRITE,
  IOC3REGS_SRCIR_A_MASK,	IOC3REGS_SRCIR_A_DEFAULT},

{"IOC3REGS_SRCIR_B",		IOC3REGS_SRCIR_B,		GZ_READ_WRITE,
 IOC3REGS_SRCIR_B_MASK,		IOC3REGS_SRCIR_B_DEFAULT},

{"IOC3REGS_STCIR_A",		IOC3REGS_STCIR_A,		GZ_READ_WRITE,
  IOC3REGS_STCIR_A_MASK,	IOC3REGS_STCIR_A_DEFAULT},

{"IOC3REGS_STCIR_B",		IOC3REGS_STCIR_B,		GZ_READ_WRITE,
 IOC3REGS_STCIR_B_MASK,		IOC3REGS_STCIR_B_DEFAULT},

{"IOC3REGS_SRPIR_A",		IOC3REGS_SRPIR_A,		GZ_READ_WRITE,
  IOC3REGS_SRPIR_A_MASK,	IOC3REGS_SRPIR_A_DEFAULT},

{"IOC3REGS_SRPIR_B",		IOC3REGS_SRPIR_B,		GZ_READ_WRITE,
 IOC3REGS_SRPIR_B_MASK,		IOC3REGS_SRPIR_B_DEFAULT},

{"IOC3REGS_STPIR_A",		IOC3REGS_STPIR_A,		GZ_READ_WRITE,
  IOC3REGS_STPIR_A_MASK,	IOC3REGS_STPIR_A_DEFAULT},

{"IOC3REGS_STPIR_B",		IOC3REGS_STPIR_B,		GZ_READ_WRITE,
 IOC3REGS_STPIR_B_MASK,		IOC3REGS_STPIR_B_DEFAULT},

{"IOC3REGS_SRTR_A",		IOC3REGS_SRTR_A,		GZ_READ_WRITE,
  IOC3REGS_SRTR_A_MASK,		IOC3REGS_SRTR_A_DEFAULT},

{"IOC3REGS_SRTR_B",		IOC3REGS_SRTR_B,		GZ_READ_WRITE,
 IOC3REGS_SRTR_B_MASK,		IOC3REGS_SRTR_B_DEFAULT},

{"IOC3REGS_EBIR",		IOC3REGS_EBIR,	GZ_READ_WRITE,
  IOC3REGS_EBIR_MASK,		IOC3REGS_EBIR_DEFAULT},

{"IOC3REGS_ETCDC",		IOC3REGS_ETCDC,	GZ_READ_WRITE,
  IOC3REGS_ETCDC_MASK,		IOC3REGS_ETCDC_DEFAULT},

{"IOC3REGS_ERBR_H",		IOC3REGS_ERBR_H,		GZ_READ_WRITE,
  IOC3REGS_ERBR_H_MASK,		IOC3REGS_ERBR_H_DEFAULT},

{"IOC3REGS_ERBR_L",		IOC3REGS_ERBR_L,		GZ_READ_WRITE,
  IOC3REGS_ERBR_L_MASK,		IOC3REGS_ERBR_L_DEFAULT},

{"IOC3REGS_ERBAR",		IOC3REGS_ERBAR,		GZ_READ_WRITE,
  IOC3REGS_ERBAR_MASK,		IOC3REGS_ERBAR_DEFAULT},

{"IOC3REGS_ERCIR",		IOC3REGS_ERCIR,	GZ_READ_WRITE,
  IOC3REGS_ERCIR_MASK,		IOC3REGS_ERCIR_DEFAULT},

{"IOC3REGS_ERBAR",		IOC3REGS_ERBAR,	GZ_READ_WRITE,
  IOC3REGS_ERBAR_MASK,		IOC3REGS_ERBAR_DEFAULT},

{"IOC3REGS_ETBR_H",		IOC3REGS_ETBR_H,		GZ_READ_WRITE,
  IOC3REGS_ETBR_H_MASK,		IOC3REGS_ETBR_H_DEFAULT},

{"IOC3REGS_ETBR_L",		IOC3REGS_ETBR_L,		GZ_READ_WRITE,
  IOC3REGS_ETBR_L_MASK,		IOC3REGS_ETBR_L_DEFAULT},

{"IOC3REGS_ETPIR",		IOC3REGS_ETPIR,		GZ_READ_WRITE,
  IOC3REGS_ETPIR_MASK,		IOC3REGS_ETPIR_DEFAULT},

{"IOC3REGS_ETCIR",		IOC3REGS_ETCIR,		GZ_READ_WRITE,
  IOC3REGS_ETCIR_MASK,		IOC3REGS_ETCIR_DEFAULT},

{"IOC3REGS_ERTR",		IOC3REGS_ERTR,		GZ_READ_WRITE,
  IOC3REGS_ERTR_MASK,		IOC3REGS_ERTR_DEFAULT},

{"IOC3REGS_EMAR_H",		IOC3REGS_EMAR_H,		GZ_READ_WRITE,
  IOC3REGS_EMAR_H_MASK,		IOC3REGS_EMAR_H_DEFAULT},

{"IOC3REGS_EMAR_L",		IOC3REGS_EMAR_L,		GZ_READ_WRITE,
  IOC3REGS_EMAR_L_MASK,		IOC3REGS_EMAR_L_DEFAULT},

{"IOC3REGS_EHAR_H",		IOC3REGS_EHAR_H,		GZ_READ_WRITE,
  IOC3REGS_EHAR_H_MASK,		IOC3REGS_EHAR_H_DEFAULT},

{"IOC3REGS_EHAR_L",		IOC3REGS_EHAR_L,		GZ_READ_WRITE,
  IOC3REGS_EHAR_L_MASK,		IOC3REGS_EHAR_L_DEFAULT},

{"", -1, -1, -1, -1}

};

/*
 * Name:	ioc3_regs.c
 * Description:	tests registers in the ioc3
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
ioc3_regs(__uint32_t argc, char **argv)
{
	ioc3_Registers	*regs_ptr = gd_ioc3_regs;
	bool_t		check_defaults;
	ioc3registers_t r_stat;

	msg_printf(INFO,"IOC3 Register Test\n");

	ioc3_error = 0; /* init : it is incremented here, 
				not in the called subroutines */
	/*test
	test();
	*/

        /* Before doing test, reset ioc3 enet so that the enet is in the
        ** known state. */

        PIO_REG_WR_32(IOC3REGS_EMCR, 0x80000000, EMCR_RST);
        PIO_REG_RD_32(IOC3REGS_EMCR, EMCR_ARB_DIAG_IDLE, r_stat);
        while ( r_stat == 0) {
                DELAY(10);
                PIO_REG_RD_32(IOC3REGS_EMCR, EMCR_ARB_DIAG_IDLE, r_stat);
        }
        PIO_REG_WR_32(IOC3REGS_EMCR, 0xFFFFFFFF, 0x0);

	while (regs_ptr->name[0] != NULL) {

	   switch (regs_ptr->mode) {
	      case  GZ_READ_ONLY:
	        if (_ioc3_regs_check_default(regs_ptr)) {
	        	msg_printf( DBG,"\n");
			ioc3_error++;
	  		if (exit_on_ioc3_error) goto _error;
	  	        msg_printf( DBG,"\n");
		}
		break;
	      case  GZ_READ_WRITE: 
		if (_ioc3_regs_read_write(regs_ptr)) {
		    ioc3_error++;
	   	    if (exit_on_ioc3_error) goto _error;
		}
		break;
	      case  GZ_WRITE_ONLY:
		break;
	      case  GZ_NO_ACCESS:
		break;
	   }
	   regs_ptr++;
	}
	/*
	if (_special_ioc3_cases()) { 
		ioc3_error++;
	}
	*/

_error:
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(ioc3_error, "IOC3 Read-Write Register", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_IOC3_0001], ioc3_error );
}

/*
 * Name:	_ioc3_regs_check_default.c
 * Description:	Tests the default values of ioc3 Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_ioc3_regs_check_default(ioc3_Registers *regs_ptr)
{
	ioc3registers_t	reg_val;

	PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, reg_val);

	if (reg_val != (regs_ptr->def & regs_ptr->mask)) {
	   msg_printf(INFO, "ERROR Default:    %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(regs_ptr->def & regs_ptr->mask), (reg_val & regs_ptr->mask));
	   if (exit_on_ioc3_error) {
		return(1);
	   }
	}
        else {
	   msg_printf(DBG, "Configuration Register Test Passed\n");
	   msg_printf(DBG, "\t%s (0x%x) mask= 0x%x; default OK= 0x%x; ",
			regs_ptr->name, regs_ptr->address, regs_ptr->mask,
			(regs_ptr->def & regs_ptr->mask));
	}
	
	return(0);
}

/*
 * Name:	_ioc3_regs_read_write.c
 * Description:	Performs Write/Reads on the ioc3 Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_ioc3_regs_read_write(ioc3_Registers *regs_ptr)
{
	ioc3registers_t	saved_reg_val, write_val, read_val;
	ioc3registers_t	pattern[IOC3REGS_REGS_PATTERN_MAX] = {
					0x5a5a5a5a, 0xa5a5a5a5,
					0xcccccccc, 0x33333333,
					0xf0f0f0f0, 0x0f0f0f0f};
	char 		pattern_index;
					

    /* Save the current value */
    PIO_REG_RD_32(regs_ptr->address, (ioc3registers_t)~0x0, saved_reg_val);

    for (pattern_index=0; pattern_index<HR_REGS_PATTERN_MAX; pattern_index++) {
	write_val = pattern[pattern_index];

	/* Write the test value */
	PIO_REG_WR_32(regs_ptr->address, regs_ptr->mask, write_val);

	/* Read back the register */
	PIO_REG_RD_32(regs_ptr->address, regs_ptr->mask, read_val);

	/* Compare the expected and received values */
	if ((write_val & regs_ptr->mask) != read_val) {
	   msg_printf(INFO, "\nERROR Write/Read: %s; Addr = 0x%x; Mask = 0x%x; \
		\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(write_val&regs_ptr->mask), read_val);
	   /* Restore the register with old value */
	   PIO_REG_WR_32(regs_ptr->address, (ioc3registers_t)~0x0, saved_reg_val);
	   return(1);	/* exit on first error */
	}
	else if (pattern_index == HR_REGS_PATTERN_MAX -1) {
		msg_printf(DBG, "Write/Read Test OK.\n");
	}
    } /* pattern_index loop */

    /* Restore the register with old value */
    PIO_REG_WR_32(regs_ptr->address, (ioc3registers_t)~0x0, saved_reg_val);

    return(0);
}

/*
 * Name:	_special_ioc3_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_special_ioc3_cases()
{
	return (0);
 
}

void test(void)
{

#define IOC3REGS_MEM_BASE IOC3_PCI_DEVIO_BASE         
  ioc3registers_t	address, write_val, read_val, readvalue;
  long mask = 0xFFFFFFFF;
  write_val = 0xFFFFFFFF;
       
	/*Read only*/
	address = IOC3REGS_ID+4; 
	msg_printf(INFO, "\n IOC3 addr  reg 4");
	PIO_REG_RD_32(address, mask ,read_val);
	msg_printf(INFO, "\naddr: ; Addr = 0x%x; Mask = 0x%x; \
	\n\t\t write = 0x%x; read = 0x%x\n", 
	address, mask, write_val, read_val);	

	/*Read/Write/Read*/
	msg_printf(INFO, "\n Status CMD reg");
	address = IOC3REGS_ID+0x4;
	PIO_REG_RD_32(address, mask ,read_val);
	msg_printf(INFO, "\naddr: ; First: Addr = 0x%x; Mask = 0x%x; \
	\n\t\t write = 0x%x; read = 0x%x\n", 
	address, mask, write_val, read_val);
	PIO_REG_WR_32(address, mask ,0x6);
	PIO_REG_RD_32(address, mask ,read_val);
	msg_printf(INFO, "\naddr: ; Second: Addr = 0x%x; Mask = 0x%x; \
	\n\t\t write = 0x%x; read = 0x%x\n", 
	address, mask, 0x6, read_val);

	msg_printf(INFO, "\n Memory space ID 0");
	address = IOC3REGS_MEM_BASE+0x0;
	PIO_REG_RD_32(address, mask ,read_val);
	msg_printf(INFO, "\naddr: ; First: Addr = 0x%x; Mask = 0x%x; \
	\n\t\t write = 0x%x; read = 0x%x\n", 
	address, mask, write_val, read_val);
	PIO_REG_WR_32(address, mask ,write_val);
	PIO_REG_RD_32(address, mask ,read_val);
	msg_printf(INFO, "\naddr: ; Second: Addr = 0x%x; Mask = 0x%x; \
	\n\t\t write = 0x%x; read = 0x%x\n", 
	address, mask, write_val, read_val);

}
