/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __BIST_H__
#define __BIST_H__

#ident "$Revision: 1.13 $"

#include "net.h"

int		lbist_hub_execute(nasid_t nasid);
int		lbist_hub_results(nasid_t nasid, int diag_mode);

int		abist_hub_execute(nasid_t nasid);
int		abist_hub_results(nasid_t nasid, int diag_mode);

int 		get_hub_bist_status(int slot_num);
void 		set_hub_bist_status(int slot_num, int bist_status);

int		lbist_router_execute(net_vec_t vec);
int		lbist_router_results(net_vec_t vec, int diag_mode);

int		abist_router_execute(net_vec_t vec);
int		abist_router_results(net_vec_t vec, int diag_mode);

#define BIST_OK			0
#define BIST_IOERROR		-1	/* Error accessing device register */
#define BIST_TIMEOUT		-2	/* Timed out accessing device reg. */
#define BIST_FAIL		-3	/* BIST ran and failed		   */

/* defines to track bist progress */
#define HUB_NO_BIST_RAN 	0x0	/* no bist ops ran */ 
#define HUB_LBIST_RAN           0x1	/* lbist ran value */  
#define HUB_ABIST_RAN           0x3	/* abist ran value */ 

/* defines to navigate bits in the elsc nvram byte */
#define BIST_SLOT_SHIFT(slot_num)	(int) ((slot_num - 1) * 2)	
#define BIST_SLOT_MASK(slot_num)	(char) (0x3 << ((slot_num - 1) * 2)) 

/* defines for hub revisions */
#define HUB2			2
#define HUB2_1			3

/* defines for bist results */
#define STATUS_BIST_RAN		2
#define HUB2_LBIST_RESULTS	0x0000000062d3fe5c  /* MISR for LBIST */
#define HUB2_LBIST_ALT		0x0ca50fb762d3fe5c  /* MISR for LBIST */
#define HUB2_1_LBIST_RESULTS	0xffffffff943762ff  /* MISR for LBIST */
#define HUB2_ABIST_RESULTS	0x600		    /* result reg for ABIST */			
#define LOCAL_BIST		0

#endif /* __BIST_H__ */
