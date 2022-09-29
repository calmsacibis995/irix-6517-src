/*
 * pci_mem.c
 *	
 *	PCI Memory Space Tests
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

#ident "$Revision: 1.5 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/vdma.h"
#include "sys/time.h"
#include "sys/immu.h"
#include "sys/PCI/ioc3.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"
#include "d_bridge.h"
#include "d_pci.h"

/*
 * Forward References 
 */
bool_t 	pci_mem();

/*
 * Global Variables 
 */
IOC3_Regs	gz_ioc3_regs[] = {
#if PROBLEM_FIXED
{"IOC3_EMAR_H", 	IOC3_EMAR_H, 	IOC3_EMAR_H_MASK},
{"IOC3_MIDR", 		IOC3_MIDR, 	IOC3_MIDR_MASK},
{"IOC3_PPCR_A", 	IOC3_PPCR_A, 	IOC3_PPCR_A_MASK},
{"IOC3_PPCR_B", 	IOC3_PPCR_B, 	IOC3_PPCR_B_MASK},
#else
{"IOC3_INT_OUT", 	IOC3_INT_OUT, 	IOC3_INT_OUT_MASK},
{"IOC3_PPBR_H_A", 	IOC3_PPBR_H_A, 	IOC3_PPBR_H_A_MASK},
{"IOC3_PPBR_H_B", 	IOC3_PPBR_H_B, 	IOC3_PPBR_H_B_MASK},
{"IOC3_SBBR_H", 	IOC3_SBBR_H, 	IOC3_SBBR_H_MASK},
{"IOC3_SBBR_L", 	IOC3_SBBR_L, 	IOC3_SBBR_L_MASK},
{"IOC3_SRCIR_A", 	IOC3_SRCIR_A, 	IOC3_SRCIR_A_MASK},
{"IOC3_SRCIR_B", 	IOC3_SRCIR_B, 	IOC3_SRCIR_B_MASK},
{"IOC3_SRPIR_A", 	IOC3_SRPIR_A, 	IOC3_SRPIR_A_MASK},
{"IOC3_SRPIR_B", 	IOC3_SRPIR_B, 	IOC3_SRPIR_B_MASK},
{"IOC3_STPIR_A", 	IOC3_STPIR_A, 	IOC3_STPIR_A_MASK},
{"IOC3_STPIR_B", 	IOC3_STPIR_B, 	IOC3_STPIR_B_MASK},
{"IOC3_STCIR_A", 	IOC3_STCIR_A, 	IOC3_STCIR_A_MASK},
{"IOC3_STCIR_B", 	IOC3_STCIR_B, 	IOC3_STCIR_B_MASK},
{"IOC3_SRTR_A", 	IOC3_SRTR_A, 	IOC3_SRTR_A_MASK},
{"IOC3_SRTR_B", 	IOC3_SRTR_B, 	IOC3_SRTR_B_MASK},
{"IOC3_ERBR_H", 	IOC3_ERBR_H, 	IOC3_ERBR_H_MASK},
{"IOC3_ERBR_L", 	IOC3_ERBR_L, 	IOC3_ERBR_L_MASK},
{"IOC3_ERPIR", 		IOC3_ERPIR, 	IOC3_ERPIR_MASK},
{"IOC3_ERCIR", 		IOC3_ERCIR, 	IOC3_ERCIR_MASK},
{"IOC3_ERBAR", 		IOC3_ERBAR, 	IOC3_ERBAR_MASK},
{"IOC3_ETBR_H",		IOC3_ETBR_H, 	IOC3_ETBR_H_MASK},
{"IOC3_ETBR_L",		IOC3_ETBR_L, 	IOC3_ETBR_L_MASK},
{"IOC3_ETPIR", 		IOC3_ETPIR, 	IOC3_ETPIR_MASK},
{"IOC3_ETCIR", 		IOC3_ETCIR, 	IOC3_ETCIR_MASK},
{"IOC3_ERTR", 		IOC3_ERTR, 	IOC3_ERTR_MASK},
{"IOC3_EBIR", 		IOC3_EBIR, 	IOC3_EBIR_MASK},
{"IOC3_EMAR_L", 	IOC3_EMAR_L, 	IOC3_EMAR_L_MASK},
{"IOC3_EHAR_H", 	IOC3_EHAR_H, 	IOC3_EHAR_H_MASK},
{"IOC3_EHAR_L", 	IOC3_EHAR_L, 	IOC3_EHAR_L_MASK},
#endif
{"", -1, -1}
};

/*
 * Name:        pci_mem
 * Description: Tests the PCI Memory Read/Write
 * Input:       None
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks: Uses IOC3
 * Debug Status: compiles, simulated, not emulated, debugged
 */
bool_t
pci_mem()
{
	ioc3reg_t	exp, rcv, cur_reg, cur_mask, reg_saved;
	IOC3_Regs	*regs_ptr = gz_ioc3_regs;
	__uint32_t	i, ioc3_dev_num = IOC3_PCI_SLOT;
	bridgereg_t	ioc3_mem;

	msg_printf(DBG,"PCI Memory Space Read/Write Test... Begin\n");

	msg_printf(SUM, "The following registers are NOT tested\n\
		IOC3_EMAR_H, IOC3_MIDR, IOC3_PPCR_A & IOC3_PPCR_B\n");

	/* get the pointer to config space */
	ioc3_mem = BRIDGE_DEVIO(ioc3_dev_num);
	msg_printf(DBG, "ioc3_mem 0x%x\n", ioc3_mem);

	while (regs_ptr->name[0] != NULL) {
	    /* compute the device address first */
	    cur_reg  = ioc3_mem + regs_ptr->offset;
	    cur_mask = regs_ptr->mask;

	    /* read the register and save it */
	    PIO_REG_RD_32(cur_reg, cur_mask, reg_saved);
	    msg_printf(DBG, "Reg:%s; Add:0x%x; InitVal:0x%x\n",
		regs_ptr->name, cur_reg, reg_saved);

	    /* perform write-read-compare */
	    for (i = 0; i < 32; i++) {
		exp = (1 << i) & cur_mask;
		PIO_REG_WR_32(cur_reg, cur_mask, exp);
		PIO_REG_RD_32(cur_reg, cur_mask, rcv);
		if (exp != rcv) {
		    msg_printf(ERR, "PCI Memory Space Error in %s\n\
		    exp 0x%x rcv 0x%x\n", regs_ptr->name, exp, rcv);
		    d_errors++;
		}
		msg_printf(DBG, "Reg:%s; exp 0x%x; rcv 0x%x\n",
			regs_ptr->name, exp, rcv);

		exp = ~exp & cur_mask;
		PIO_REG_WR_32(cur_reg, cur_mask, exp);
		PIO_REG_RD_32(cur_reg, cur_mask, rcv);
		if (exp != rcv) {
		    msg_printf(ERR, "PCI Memory Space Error in %s\n\
		    exp 0x%x rcv 0x%x\n", regs_ptr->name, exp, rcv);
		    d_errors++;
		}
		msg_printf(DBG, "Reg:%s; exp 0x%x; rcv 0x%x\n",
			regs_ptr->name, exp, rcv);
		if (d_errors) break;
	    }

	    /* restore the register */
	    PIO_REG_WR_32(cur_reg, cur_mask, reg_saved);

	    msg_printf(SUM, "Tested %s\n", regs_ptr->name);

	    regs_ptr++;
	}
	
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "PCI Memory Space", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0003], d_errors );
}
