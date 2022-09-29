/*
 * hxb_status.c
 *
 * 	Displays some important registers in HEART, XBOW & BRIDGE
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

#ident "ide/godzilla/h_x_b/hxb_status.c: $Revision: 1.3 $"

/*
 * hxb_status.c - Displays some important registers in HEART, XBOW & BRIDGE
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

/*
 * Name:        hxb_status
 * Description: Displays some important registers in HEART, XBOW & BRIDGE
 * Input:       None
 * Output:      None
 * Error Handling:
 * Side Effects: none
 * Remarks:	XXX implement input flags, just like hb_status
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
hxb_status()
{
	xbowreg_t	xr_mask = ~0x0;
	xbowreg_t	xr_regs[20];
	/* currently 6 registers/link that can be read w/o side effects */ 
	/*  I picked 10 to be safe */
	xbowreg_t	xr_link_regs[10]; 
	char		index;  /* from 8 to f */

	msg_printf(DBG, "Display Heart/Xbow/Bridge Registers... Begin\n");

	/* use existing routine: i.e. Heart and Bridge first: */
	if (_hb_status(HEART_STAT, BRIDGE_STAT)) return(1);

	/*
	 * Read Xbow Registers :
 	 * NOTE: do not read XBOW_WID_STAT_CLR as it would clear the xbow status
	 */
	XB_REG_RD_32(XBOW_WID_ID, xr_mask, xr_regs[0]);
	XB_REG_RD_32(XBOW_WID_STAT, xr_mask, xr_regs[1]);
	XB_REG_RD_32(XBOW_WID_ERR_UPPER, xr_mask, xr_regs[2]);
	XB_REG_RD_32(XBOW_WID_ERR_LOWER, xr_mask, xr_regs[3]);
	XB_REG_RD_32(XBOW_WID_CONTROL, xr_mask, xr_regs[4]);
	XB_REG_RD_32(XBOW_WID_REQ_TO, xr_mask, xr_regs[5]);
	XB_REG_RD_32(XBOW_WID_INT_UPPER, xr_mask, xr_regs[6]);
	XB_REG_RD_32(XBOW_WID_INT_LOWER, xr_mask, xr_regs[7]);
	XB_REG_RD_32(XBOW_WID_ERR_CMDWORD, xr_mask, xr_regs[8]);
	XB_REG_RD_32(XBOW_WID_LLP, xr_mask, xr_regs[9]);
	XB_REG_RD_32(XBOW_WID_ARB_RELOAD, xr_mask, xr_regs[10]);
	XB_REG_RD_32(XBOW_WID_PERF_CTR_A, xr_mask, xr_regs[11]);
	XB_REG_RD_32(XBOW_WID_PERF_CTR_B, xr_mask, xr_regs[12]);
	XB_REG_RD_32(XBOW_WID_NIC, xr_mask, xr_regs[13]);
	msg_printf(SUM,
	   "XBOW_WID_ID = 0x%08x;\tXBOW_WID_STAT = 0x%08x; \nXBOW_WID_ERR_UPPER = 0x%08x;\tXBOW_WID_ERR_LOWER = 0x%08x;\nXBOW_WID_CONTROL = 0x%08x;\tXBOW_WID_REQ_TO = 0x%08x; \nXBOW_WID_INT_UPPER = 0x%08x;\tXBOW_WID_INT_LOWER = 0x%08x; \nXBOW_WID_ERR_CMDWORD = 0x%08x;\nXBOW_WID_LLP = 0x%08x;\tXBOW_WID_ARB_RELOAD = 0x%08x; \nXBOW_WID_PERF_CTR_A = 0x%08x;\tXBOW_WID_PERF_CTR_B = 0x%08x; \nXBOW_WID_NIC = 0x%08x; \n\n", 
	    xr_regs[0], xr_regs[1], xr_regs[2], xr_regs[3], 
	    xr_regs[4], xr_regs[5], xr_regs[6], xr_regs[7], 
	    xr_regs[8], xr_regs[9], xr_regs[10], xr_regs[11], 
	    xr_regs[12], xr_regs[13]); 

	/*
	 * Read the Link registers in Xbow:
 	 */
	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) {
		XB_REG_RD_32(XB_LINK_IBUF_FLUSH(index), xr_mask, 
				xr_link_regs[0]);
		XB_REG_RD_32(XB_LINK_CTRL(index), xr_mask, 
				xr_link_regs[1]);
		XB_REG_RD_32(XB_LINK_STATUS(index), xr_mask, 
				xr_link_regs[2]);
		XB_REG_RD_32(XB_LINK_ARB_UPPER(index), xr_mask, 
				xr_link_regs[3]);
		XB_REG_RD_32(XB_LINK_ARB_LOWER(index), xr_mask, 
				xr_link_regs[4]);
		/* reading XB_LINK_STATUS_CLR clears the status register */
		/* Also, XB_LINK_RESET is write-only */
		XB_REG_RD_32(XB_LINK_AUX_STATUS(index), xr_mask, 
				xr_link_regs[5]);
		msg_printf(SUM,"XBOW LINK %x:\n\tXB_LINK_IBUF_FLUSH = 0x%08x;\n\tXB_LINK_CTRL = 0x%08x;\tXB_LINK_STATUS = 0x%08x;\n\tXB_LINK_ARB_UPPER = 0x%08x;\tXB_LINK_ARB_LOWER = 0x%08x;\n\tXB_LINK_AUX_STATUS = 0x%08x;\n", index,
			xr_link_regs[0], xr_link_regs[1],
			xr_link_regs[2], xr_link_regs[3],
			xr_link_regs[4], xr_link_regs[5]);
	}

	msg_printf(DBG, "Display Heart/Xbow/Bridge Registers... End\n");

	return(0);
}
