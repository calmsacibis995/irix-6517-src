/*
 * hb_status.c
 *
 * 	Displays some important registers in HEART & BRIDGE
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

#ident "ide/godzilla/heart_bridge/hb_status.c:  $Revision: 1.13 $"

/*
 * hb_status.c - Displays some important registers in HEART & BRIDGE
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h" /* all function prototypes */

/*
 * Name:	hb_status
 * Input:       whether to print the status for heart or bridge (-h -b flags)
 * Description: call to _hb_status as ide command
 */
bool_t
hb_status(__uint32_t argc, char **argv)
{
	bool_t	heart_status = FALSE;
	bool_t	bridge_status = FALSE;

	/* "-h" arg for heart, "-b" for bridge */
	/*  default is "reset both " */
	if (argc == 1) {
		heart_status = TRUE;
		bridge_status = TRUE;
	}
	else  {
		argc--; argv++;
		if (argc && argv[0][0]=='-' && argv[0][1]=='h')
			heart_status = TRUE;
		else if (argc && argv[0][0]=='-' && argv[0][1]=='b')
			bridge_status = TRUE;
		if (argc != 0) {
			argc--; argv++;
			if (argc && argv[0][0]=='-' && argv[0][1]=='b')
				bridge_status = TRUE;
			else if (argc && argv[0][0]=='-' && argv[0][1]=='h')
				heart_status = TRUE;
		}
	}

	if (_hb_status(heart_status, bridge_status))
		return(1);
	return(0);
}

/*
 * Name:        _hb_status
 * Description: Displays some important registers in HEART & BRIDGE
 * Input:       whether to print the status for heart or bridge
 * Output:      None
 * Error Handling:
 * Side Effects: none
 * Remarks:
 * Debug Status: compiles, simulated, emulated, 
 */
bool_t
_hb_status(bool_t heart_stat, bool_t bridge_stat)
{
	heartreg_t	hr_mask = ~0x0;
	bridgereg_t	br_mask = ~0x0;
	heartreg_t	hr_regs[20];
	bridgereg_t	br_regs[10];


	if (heart_stat) {
	    msg_printf(DBG, "Display Heart Registers... \n");
	    /*
	     * Read Heart Registers :
	     *  1. Widget Identification Register
	     *  2. Widget Status Register
	     *  3. Widget Error Upper Address Register
	     *  4. Widget Error Lower Address Register
	     *  5. Widget Request Time-Out Value Regsiter
	     *  6. Widget Error Command Word Register
	     *  7. Flow Control Timer 0
	     *  8. Flow Control Timer 1
	     *  9. Status Register
	     * 10. Processor Bus Error Address Register
	     * 11. Processor Bus Error Misc. Register
	     * 12. Memory Address or Data Error Register
	     * 13. Bad Memory Data Register
	     * 14. PIU Access Error Register
	     * 15. Interrupt Status Register
	     * 16. Exception Cause Register
	     * 17. Processor ID Register
	     * 18. Widget Error Mask Register
	     * 19. Widget Control Register
	     * 20. Widget Error Type Register
	     */
	    PIO_REG_RD_64(HEART_WID_ID, hr_mask, hr_regs[0]);
	    PIO_REG_RD_64(HEART_WID_STAT, hr_mask, hr_regs[1]);
	    PIO_REG_RD_64(HEART_WID_ERR_UPPER, hr_mask, hr_regs[2]);
	    PIO_REG_RD_64(HEART_WID_ERR_LOWER, hr_mask, hr_regs[3]);
	    PIO_REG_RD_64(HEART_WID_REQ_TIMEOUT, hr_mask, hr_regs[4]);
	    PIO_REG_RD_64(HEART_WID_ERR_CMDWORD, hr_mask, hr_regs[5]);
	    PIO_REG_RD_64(HEART_FC_TIMER(0), hr_mask, hr_regs[6]);
	    PIO_REG_RD_64(HEART_FC_TIMER(1), hr_mask, hr_regs[7]);
	    PIO_REG_RD_64(HEART_STATUS, hr_mask, hr_regs[8]);
	    PIO_REG_RD_64(HEART_BERR_ADDR, hr_mask, hr_regs[9]);
	    PIO_REG_RD_64(HEART_BERR_MISC, hr_mask, hr_regs[10]);
	    PIO_REG_RD_64(HEART_MEMERR_ADDR, hr_mask, hr_regs[11]);
	    PIO_REG_RD_64(HEART_MEMERR_DATA, hr_mask, hr_regs[12]);
	    PIO_REG_RD_64(HEART_PIUR_ACC_ERR, hr_mask, hr_regs[13]);
	    PIO_REG_RD_64(HEART_ISR, hr_mask, hr_regs[14]);
	    PIO_REG_RD_64(HEART_CAUSE, hr_mask, hr_regs[15]);
	    PIO_REG_RD_64(HEART_PRID, hr_mask, hr_regs[16]);
	    PIO_REG_RD_64(HEART_WID_ERR_MASK, hr_mask, hr_regs[17]);
	    PIO_REG_RD_64(HEART_WID_CONTROL, hr_mask, hr_regs[18]);
	    PIO_REG_RD_64(HEART_WID_ERR_TYPE, hr_mask, hr_regs[19]);
	    msg_printf(SUM,
	   "HEART_WID_ID = 0x%x;\tHEART_WID_STAT = 0x%x;\nHEART_WID_ERR_MASK = 0x%x;\tHEART_WID_CONTROL = 0x%x;\nHEART_WID_ERR_UPPER = 0x%x;\tHEART_WID_ERR_LOWER = 0x%x;\nHEART_WID_ERR_TYPE = 0x%x;\nHEART_WID_REQ_TIMEOUT = 0x%x;\tHEART_WID_ERR_CMDWORD = 0x%x;\nHEART_FC0_TIMER = 0x%x;\tHEART_FC1_TIMER = 0x%x;\nHEART_STATUS = 0x%x;\tHEART_BERR_ADDR = 0x%x;\nHEART_BERR_MISC = 0x%x;\tHEART_MEM_ERR = 0x%x;\nHEART_BAD_MEM = 0x%x;\tHEART_PIU_ERR = 0x%x;\nHEART_ISR = 0x%x;\tHEART_CAUSE = 0x%x;\nHEART_PRID = 0x%x;\n\n", 
	    	hr_regs[0], hr_regs[1], hr_regs[17], hr_regs[18], 
		hr_regs[2], hr_regs[3], hr_regs[19],
		hr_regs[4], hr_regs[5], hr_regs[6], hr_regs[7], hr_regs[8], 
		hr_regs[9], hr_regs[10], hr_regs[11], hr_regs[12], hr_regs[13], 
		hr_regs[14], hr_regs[15], hr_regs[16]);
	} /* heart_stat */

	if (bridge_stat) {
	    msg_printf(DBG, "Display Bridge Registers... \n");
	    /* 
	     * Read Bridge Registers :
	     *  1. Identification register
	     *  2. Status register
	     *  3. INT_STATUS register
	     *  4. ERROR UPPER ADDRESS register
	     *  5. ERROR LOWER ADDRESS register
	     *  6. Control Register
	     *  7. Request Time-Out Value register
	     *  8. Bridge Interrupt enable register
	     *  9. widget error cmd word register
	     *  10. auxiliary widget error cmd word register
	     */
	    BR_REG_RD_32(BRIDGE_WID_ID, br_mask, br_regs[0]);
	    BR_REG_RD_32(BRIDGE_WID_STAT, br_mask, br_regs[1]);
	    BR_REG_RD_32(BRIDGE_INT_STATUS, br_mask, br_regs[2]);
	    BR_REG_RD_32(BRIDGE_WID_ERR_UPPER, br_mask, br_regs[3]);
	    BR_REG_RD_32(BRIDGE_WID_ERR_LOWER, br_mask, br_regs[4]);
	    BR_REG_RD_32(BRIDGE_WID_CONTROL, br_mask, br_regs[5]);
	    BR_REG_RD_32(BRIDGE_WID_REQ_TIMEOUT, br_mask, br_regs[6]);
	    BR_REG_RD_32(BRIDGE_INT_ENABLE, br_mask, br_regs[7]);
	    BR_REG_RD_32(BRIDGE_WID_ERR_CMDWORD, br_mask, br_regs[8]);
	    BR_REG_RD_32(BRIDGE_WID_AUX_ERR, br_mask, br_regs[9]);
	    msg_printf(SUM,
		"BRIDGE_WID_ID = 0x%x;\tBRIDGE_WID_STAT = 0x%x;\nBRIDGE_WID_ERR_UPPER = 0x%x;\tBRIDGE_WID_ERR_LOWER = 0x%x;\nBRIDGE_WID_CONTROL = 0x%x;\tBRIDGE_WID_REQ_TIMEOUT = 0x%x;\nBRIDGE_INT_STATUS = 0x%x;\tBRIDGE_INT_ENABLE = 0x%x\nBRIDGE_WID_ERR_CMDWORD = 0x%x;\tBRIDGE_WID_AUX_ERR = 0x%x;\n\n",
		br_regs[0], br_regs[1], br_regs[3], 
		br_regs[4], br_regs[5], br_regs[6], br_regs[2], br_regs[7],
		br_regs[8], br_regs[9]);
	} /* bridge_stat */

	return(0);
}
