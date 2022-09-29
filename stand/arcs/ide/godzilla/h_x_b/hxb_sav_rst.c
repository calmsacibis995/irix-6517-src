/*
 * hxb_sav_rst.c
 *
 * 	Saves and Restores the Heart, Xbow, and Bridge Registers
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
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_heart.h"
#include "d_bridge.h"
#include "d_xbow.h"
#include "d_prototypes.h"

/*
 * Forward References 
 */
void 	_xr_save_regs(xbowreg_t *save, xbowreg_t *buf); 	/* XBOW */
void 	_xr_rest_regs(xbowreg_t *save, xbowreg_t *buf);
void	_hr_save_regs(heartreg_t *save, heartreg_t *buf);	/* HEART */
void	_hr_rest_regs(heartreg_t *save, heartreg_t *buf);
void	_br_save_regs(bridgereg_t *save, bridgereg_t *buf);	/* BRIDGE */
void	_br_rest_regs(bridgereg_t *save, bridgereg_t *buf);

/*
 * Global Variables
 */
/* only the R/W (W without special effect) registers are listed */
xbowreg_t	gz_xr_def_list[] = {XBOW_WID_CONTROL, XBOW_WID_REQ_TO, 
		XBOW_WID_LLP, /* XXX would that jeopardize the B link ? */
		XBOW_WID_NIC,
		/* skip widget 8 (heart) */
		XB_LINK_CTRL(XBOW_PORT_9), 
		XB_LINK_ARB_UPPER(XBOW_PORT_9), XB_LINK_ARB_LOWER(XBOW_PORT_9),
		XB_LINK_CTRL(XBOW_PORT_A), 
		XB_LINK_ARB_UPPER(XBOW_PORT_A), XB_LINK_ARB_LOWER(XBOW_PORT_A),
		XB_LINK_CTRL(XBOW_PORT_B), 
		XB_LINK_ARB_UPPER(XBOW_PORT_B), XB_LINK_ARB_LOWER(XBOW_PORT_B),
		XB_LINK_CTRL(XBOW_PORT_C), 
		XB_LINK_ARB_UPPER(XBOW_PORT_C), XB_LINK_ARB_LOWER(XBOW_PORT_C),
		XB_LINK_CTRL(XBOW_PORT_D), 
		XB_LINK_ARB_UPPER(XBOW_PORT_D), XB_LINK_ARB_LOWER(XBOW_PORT_D),
		XB_LINK_CTRL(XBOW_PORT_E), 
		XB_LINK_ARB_UPPER(XBOW_PORT_E), XB_LINK_ARB_LOWER(XBOW_PORT_E),
		/* skip widget f (bridge) */
		D_EOLIST};

/*
 * Name:	_xr_save_regs
 * Description:	saves a set of Xbow registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
void
_xr_save_regs(xbowreg_t *p_regs, xbowreg_t *p_save)
{
	xbowreg_t	mask = ~0x0;

	msg_printf(DBG, "Xbow Save Registers\n");

	if (p_regs == NULL) {
	   /* need to save all registers */
	   p_regs = gz_xr_def_list;
	   p_save = (xbowreg_t *)malloc(sizeof(gz_xr_def_list));
	}

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   XB_REG_RD_32(*p_regs, mask, *p_save);
	   p_regs++; 
	   p_save++;
	}

	msg_printf(DBG, "Xbow Save done\n");
}

/*
 * Name:	_xr_rest_regs
 * Description:	restores a set of Xbow registers
 * Input:	pointer to a set of register addresses
 * Output:	None
 * Error Handling:
 * Side Effects: 
 * Remarks:	assumes that the mask is ~0x0
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
void
_xr_rest_regs(xbowreg_t *p_regs, xbowreg_t *p_save)
{
	xbowreg_t	mask = ~0x0;

	msg_printf(DBG, "Xbow Restore Registers\n");

	if (p_regs == NULL) {
	   /* need to restore all registers */
	   p_regs = gz_xr_def_list;
	   free(p_save);
	}

	/* read registers and save them */
	while (*p_regs != D_EOLIST) {
	   XB_REG_WR_32(*p_regs, mask, *p_save);
	   p_regs++; 
	   p_save++;
	}

	msg_printf(DBG, "Xbow Restore done\n");
}
