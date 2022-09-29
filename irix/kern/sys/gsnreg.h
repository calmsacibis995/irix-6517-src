/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/********************************************************************************
 *										*
 * sys/gsnreg.h - Stub for GSN register definitions
 *										*
 ********************************************************************************/

#ifndef __SYS_GSNREG_H__
#define __SYS_GSNREG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/xtalk/xtalk.h"
#include "sys/xtalk/xwidget.h"

/*
 * Crosstalk hardware values.
 */
#define	GSN_A_WIDGET_PART_NUM	0xc104	/* Primary GSN board   */
#define	GSN_B_WIDGET_PART_NUM	0xc105	/* Auxiliary GSN board */
#define	GSN_WIDGET_MFGR_NUM	0x49
/*
 * The NIC register is accessible in this location in each XTALK port 
 * for reasons of compliance with the Crosstalk standard. However,
 * there is actually only one NIC register on SHAC. A value written to 
 * either address 0x058 or address 0x158 will be seen at both addresses when 
 * they are read. Although the NIC register may be accessed from both XTALK 
 * ports,  doing so simultaneously will result in reading garbage. 
 */
#define GSN_L_WIDGET_NIC	0x058 /* Local Number In a Can register */
#define GSN_R_WIDGET_NIC	0x158 /* Remote Number In a Can register */


/*
 * The following three one-bit registers may be used to implement Petersen's
 * algorithm for mutual exclusion between a single host process and the FWP 
 * firmware (or second host on a 2 board configuration). This could be 
 * necessary in order to synchronize access to registers or SSRAM. These 
 * registers have no effect on the hardware whatsoever. 
 *
 *
 * Petersen's algorithm 
 *
 * Host lock routine: HOST_WANTS_LOCK = 1; FAVOR_FWP_LOCK = 1; 
 *		      while (FWP_WANTS_LOCK && FAVOR_FWP_LOCK); 
 *
 * FWP lock routine:  FWP_WANTS_LOCK = 1; FAVOR_FWP_LOCK = 0; 
 *			      while (HOST_WANTS_LOCK && !FAVOR_FWP_LOCK); 
 *
 * Host unlock routine: HOST_WANTS_LOCK = 0; 
 *
 * FWP unlock routine FWP_WANTS_LOCK = 0; 
 */
#define GSN_HOST_WANTS_LOCK	0xA20 /* One of three one-bit lock registers */
#define GSN_FWP_WANTS_LOCK	0xA28 /* One of three one-bit lock registers */
#define GSN_FAVOR_FWP_LOCK	0xA30 /* One of three one-bit lock registers */

#endif
