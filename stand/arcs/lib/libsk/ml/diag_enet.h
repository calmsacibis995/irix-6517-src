/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* 
 *   File:  "$Id: diag_enet.h,v 1.3 1996/08/15 02:11:28 csatt Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 */

#ident  "$Revision: 1.3 $"

#define IOC3_LOOP           0
#define PHY_LOOP            1
#define TWISTER_LOOP        2
#define EXTERNAL_LOOP       3
#define EXTERNAL_LOOP_10    4
#define EXTERNAL_LOOP_100   5
#define XTALK_STRESS        6

/* Prototypes */
int diag_enet_all(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_ssram(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_phy_reg(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_phy_read(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, 
		       int reg,  __uint32_t *val);
int diag_enet_phy_write(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, 
			int reg,  u_short val);
int diag_enet_phy_or(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, 
		     int reg,  u_short val);
int diag_enet_phy_and(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, 
		      int reg,  u_short val);
int diag_enet_phy_reset(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_tx_clk(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_loop(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, int type);
int diag_enet_nic(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
int diag_enet_dump_regs(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
