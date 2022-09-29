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
 *   File:  "$Id: d_enet.h,v 1.3 1996/09/20 19:09:25 eunjoo Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 */

#ident  "$Revision: 1.3 $"

#define IOC3_LOOP           0
#define PHY_LOOP            1
#define EXTERNAL_LOOP       2
#define EXTERNAL_LOOP_10    3
#define EXTERNAL_LOOP_100   4
#define XTALK_STRESS        5

/* Prototypes */

int enet_ssram(__psunsigned_t bridge_baseAddr,int npci);
int enet_phy_reg(void);
int enet_phy_read(int reg,  __uint32_t *val);
int enet_phy_write(int reg,  u_short val);
int enet_phy_or(int reg,  u_short val);
int enet_phy_and(int reg,  u_short val);
int enet_phy_reset(void);
int enet_tx_clk(void);
int enet_loop(int type);
int enet_nic(void);
int enet_dump_regs(void);

