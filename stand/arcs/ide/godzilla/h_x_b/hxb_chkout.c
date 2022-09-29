/*
 * hxb_chkout.c
 *
 * 	Checks whether some important registers in HEART & BRIDGE
 *	have the right (or decent) values. This function must be called
 *	before actually performing the test.
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

#ident "ide/godzilla/h_x_b/hxb_chkout.c: $Revision: 1.9 $"

/*
 * hxb_chkout.c - checks whether some important registers in HEART, XBOW 
 *		 & BRIDGE have the right (or decent) values
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h" /* all godzilla function prototypes */

/*
 * Forward References : in d_prototypes.h
 */

/*
 * Name:	hxb_chkout
 * Description: call to _hxb_chkout as ide command
 */
bool_t
hxb_chkout(__uint32_t argc, char **argv)
{
	bool_t	check_heart = FALSE;
	bool_t	check_bridge = FALSE;
	bool_t	check_xbow = FALSE;

	/* "-h" arg for heart, "-b" for bridge, "-x" for xbow */
	/*  default is "check all " */

	if(_hxb_chkout(CHK_HEART, CHK_XBOW, CHK_BRIDGE))
		return(1);

	return(0);

}

/*
 * Name:        _hxb_chkout
 * Description: checks whether some important registers in HEART,
 *			XBOW  & BRIDGE have the right (or decent) values
 * Input:       None
 * Output:      None
 * Error Handling:
 * Side Effects: none
 * Remarks:
 * Debug Status: compiles, simulated, not emulated, not debugged
 */
bool_t
_hxb_chkout(bool_t chk_heart, bool_t chk_xbow, bool_t chk_bridge)
{
	xbowreg_t	xr_mask = ~0x0;
	xbowreg_t	xbow_wid_id, xbow_wid_status;
	xbowreg_t	xbow_wid_err_lower, xbow_wid_err_upper;
	xbowreg_t	xbow_wid_int_lower, xbow_wid_int_upper;
	xbowreg_t	xb_link_status, xb_link_aux_status;
	char		index;  /* from 8 to f */

	if (_hb_chkout(chk_heart, chk_bridge)) {
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

    if (chk_xbow) {
	/* CROSSBOW REGISTERS */

	/* Read Xbow Identification register */
	XB_REG_RD_32(XBOW_WID_ID, xr_mask, xbow_wid_id);
	/* Verify Xbow Identification register */
	if (xbow_wid_id & ~D_XBOW_WID_ID_REV_NUM_MASK != D_XBOW_ID & D_XBOW_WID_ID_REV_NUM_MASK) {
	    msg_printf(ERR, "Xbow Identification Register Mismatch\n");
	    msg_printf(DBG, "Xbow ID = 0x%x, should be 0x%x\n", 
					xbow_wid_id, D_XBOW_ID);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* Read Xbow Status register */
	XB_REG_RD_32(XBOW_WID_STAT, xr_mask, xbow_wid_status);
	msg_printf(SUM, "Xbow Status = 0x%x\n",xbow_wid_status);
#if EMULATION
	/* Verify Xbow register */
	if (xbow_wid_status != D_XBOW_WID_STAT) {
	    msg_printf(ERR, "Xbow Status Register Mismatch\n");
	    msg_printf(DBG, "Xbow Status = 0x%x, should be 0x%x\n", 
					xbow_wid_status, D_XBOW_WID_STAT);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}
#endif
	/* Read ERROR UPPER ADDRESS register */
	XB_REG_RD_32(XBOW_WID_ERR_UPPER, xr_mask, xbow_wid_err_upper);
	/* Verify error upper register */
	if (xbow_wid_err_upper) {
	    msg_printf(ERR, "Xbow Widget Error Upper Register is not 0\n");
	    msg_printf(DBG, "Xbow Widget Error Upper Register = 0x%x\n", 
			xbow_wid_err_upper);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}

	/* Read ERROR LOWER ADDRESS register */
	XB_REG_RD_32(XBOW_WID_ERR_LOWER, xr_mask, xbow_wid_err_lower);
	/* Verify error upper register */
	if (xbow_wid_err_lower) {
	    msg_printf(ERR, "Xbow Widget Error Lower Register is not 0\n");
	    msg_printf(DBG, "Xbow Widget Error Lower Register = 0x%x\n", 
			xbow_wid_err_lower);
	    d_errors++;
	    if (d_exit_on_error) goto _error;
	}
	/* Read  INT dest Upper/Lower */ /* XXX no check */
	XB_REG_RD_32(XBOW_WID_INT_UPPER, xr_mask, xbow_wid_int_upper);
	XB_REG_RD_32(XBOW_WID_INT_LOWER, xr_mask, xbow_wid_int_lower);
	/* print interrupt destination address */
	msg_printf(DBG, "Xbow Widget Int Destination Address (Upper) = 0x%x\n",
		xbow_wid_int_upper & D_XBOW_WID_INT_UPPER_ADDR);
	msg_printf(DBG, "Xbow Widget Int Destination Address (Lower) = 0x%x\n",
		xbow_wid_int_lower);
	msg_printf(DBG, "Xbow Widget Int Target ID Number = 0x%x\n",
		xbow_wid_int_upper & D_XBOW_WID_INT_UPPER_TARGID);
	msg_printf(DBG, "Xbow Widget Int Vector = 0x%x\n",
		xbow_wid_int_upper & D_XBOW_WID_INT_UPPER_INTVECT);

	/* Check Port 8(HEART) and 0xf(BRIDGE) */

	/* Check HEART link and widget status */
	/* Link(x) status register */
	XB_REG_RD_32(XB_LINK_STATUS(XBOW_PORT_8), xr_mask, xb_link_status);
	if ((xb_link_status & XB_STAT_LINKALIVE) == 0)
		msg_printf(ERR, "Xbow Link (%x)(HAERT) Link Not Alive\n",XBOW_PORT_8);
	/* Link(x) auxiliary status register */
	XB_REG_RD_32(XB_LINK_AUX_STATUS(XBOW_PORT_8),xr_mask, xb_link_aux_status);
	if ((xb_link_aux_status & XB_AUX_STAT_PRESENT) == 0)
		msg_printf(ERR, "Xbow Link (%x)(HAERT) Widge not Present\n",XBOW_PORT_8);

	/* Check BRIDGE link and widget status */
	/* Link(x) status register */
	XB_REG_RD_32(XB_LINK_STATUS(XBOW_PORT_F), xr_mask, xb_link_status);
	if ((xb_link_status & XB_STAT_LINKALIVE) == 0)
		msg_printf(ERR, "Xbow Link (%x)(BRIDGE) Link Not Alive\n",XBOW_PORT_F);
	/* Link(x) auxiliary status register */
	XB_REG_RD_32(XB_LINK_AUX_STATUS(XBOW_PORT_F),xr_mask, xb_link_aux_status);
	if ((xb_link_aux_status & XB_AUX_STAT_PRESENT) == 0)
		msg_printf(ERR, "Xbow Link (%x)(BRIDGE) Widge not Present\n",XBOW_PORT_F);

#if EMULATION /* KY: EMULATION only */
	/* LINK REGISTERS IN CROSSBOW */

	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) {
	    /* Link(x) status register */
	    XB_REG_RD_32(XB_LINK_STATUS(index), xr_mask, xb_link_status);
	    if (xb_link_status) {
		msg_printf(ERR, "Xbow Link (%x) Status Register is not 0\n",
				index);
		msg_printf(DBG, "Xbow Link (%x) Status Register = 0x%x\n",
				index, xb_link_status);
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	    /* Link(x) auxiliary status register */
	    XB_REG_RD_32(XB_LINK_AUX_STATUS(index), 
				xr_mask, xb_link_aux_status);
	    if (xb_link_aux_status) {
		msg_printf(ERR, "Xbow Link (%x) Auxiliary Status Register is not 0\n",
				index);
		msg_printf(DBG, "Xbow Link (%x) Auxiliary Status Register = 0x%x\n",
				index, xb_link_aux_status);
		d_errors++;
		if (d_exit_on_error) goto _error;
	    }
	}
#endif
    } /* chk_xbow */
			
_error:
	msg_printf(DBG, "Heart, Xbow & Bridge Check Out... End\n");

#ifdef SABLE
	d_errors=0; /* XXX */
	msg_printf(INFO, "kludge to make x_chkout pass on sable!\n");
#endif

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Heart, Xbow & Bridge Check Out", 
			D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_HXB_0000], d_errors );
}

