/*
 * hb_sav_rst.c
 *
 * 	Saves and Restores the Heart and Bridge Registers
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

#ident "ide/godzilla/heart_bridge/hb_sav_rst.c: $Revision: 1.14 $"

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
 * Forward References : in d_prototypes.h
 */

/*
 * Global Variables
 */
/* only the R/W (W without special effect) registers are listed */
heartreg_t	gz_hr_def_list[] = {HEART_WID_CONTROL, HEART_WID_REQ_TIMEOUT, 
		HEART_WID_LLP, HEART_WID_ERR_MASK, HEART_MODE, HEART_SDRAM_MODE,
		HEART_MEM_REF, HEART_MEM_REQ_ARB, HEART_FC_MODE, 
		HEART_FC_TIMER_LIMIT, HEART_FC_ADDR(0), HEART_FC_ADDR(1), 
		HEART_FC_CR_CNT(0), HEART_FC_CR_CNT(1), 
		HEART_MLAN_CLK_DIV, HEART_MLAN_CTL, 
		HEART_IMR(0), HEART_IMR(1), HEART_IMR(2), HEART_IMR(3),
		HEART_COMPARE,
		D_EOLIST};

/*
 * Name:	_hr_save_regs
 * Description:	saves a set of Heart registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, emulated, 
 */
void
_hr_save_regs(heartreg_t *p_regs, heartreg_t *p_save)
{
	heartreg_t	mask = ~0x0;

	msg_printf(DBG, "Heart Save Registers\n");

	if (p_regs == NULL) {
	   /* need to save all registers */
	   p_regs = gz_hr_def_list;
	   p_save = (heartreg_t *)malloc(sizeof(gz_hr_def_list));
	}

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   PIO_REG_RD_64(*p_regs, mask, *p_save);
	   p_regs++; 
	   p_save++;
	}
}

/*
 * Name:	_br_save_regs
 * Description:	saves a set of Bridge registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, emulated, 
 */
void
_br_save_regs(bridgereg_t *p_regs, bridgereg_t *p_save)
{
	bridgereg_t	mask = ~0x0;

	msg_printf(DBG, "Bridge Save Registers\n");

	if (p_regs == NULL)
		return;

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   BR_REG_RD_32((*p_regs), mask, *p_save);
	   p_regs++; 
	   p_save++;
	}
}

/*
 * Name:	_hr_rest_regs
 * Description:	restores a set of Heart registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, emulated, 
 */
void
_hr_rest_regs(heartreg_t *p_regs, heartreg_t *p_save)
{
	heartreg_t	mask = ~0x0;

	msg_printf(DBG, "Heart Restore Registers\n");

	if (p_regs == NULL) {
	   /* need to restore all registers */
	   p_regs = gz_hr_def_list;
	   free(p_save);
	}

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   PIO_REG_WR_64(*p_regs, mask, *p_save);
	   p_regs++; 
	   p_save++;
	}
}

/*
 * Name:	_br_rest_regs
 * Description:	restores a set of Bridge registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, emulated, 
 */
void
_br_rest_regs(bridgereg_t *p_regs, bridgereg_t *p_save)
{
	bridgereg_t	mask = ~0x0;

	msg_printf(DBG, "Heart & Bridge Restore Registers... Restore bridge registers\n");

	if (p_regs == NULL)
	   return;

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   BR_REG_WR_32((*p_regs), mask, *p_save);
	   p_regs++; 
	   p_save++;
	}
}
