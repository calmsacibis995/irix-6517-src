/*
 * test_xbow_arb.c : 
 *	Testing of the Xbow Arbitration utilities (for the xtalk tests)
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

/*
 * test_xbow_arb.c - Test the Xbow Arbitration utilities (for the xtalk tests)
 *
 *	functions to be tested:
 *		xbow_perf_mon_set: to set what's to be monitored
 *		xbow_arb_set: to set the xbow arbitration
 *		xbow_perf_results: to get the performance results
 */
#ident "ide/godzilla/x_test/test_xbow_arb.c:  $Revision: 1.2 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/xtalk/xbow.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_x_test.h"

/* forward declarations */
bool_t xbow_perf_mon_set (struct test_args *targ_p, struct proc_args *parg_p);
bool_t xbow_arb_set (struct test_args *targ_p, struct proc_args *parg_p);

/*
 * Name:	test_xbow_perf_mon_set
 * Description:	tests the xbow performance monitor
 * 		This program varies the following inputs:
 *		test_args struc: perf_monitor_mode (0-3), 
 *			perf_monitor_buf_level (0-7 
 *                      only if perf_monitor_mode==3)
 *			mon_link_id_a, mon_link_id_b		
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
test_xbow_perf_mon_set(void)
{
	struct test_args targ;
	struct proc_args parg;
	struct test_args *targ_p;
	struct proc_args *parg_p;
	
	msg_printf(DBG," in test_xbow_perf_mon_set \n");

	targ_p = &targ;
	parg_p = &parg;

	parg_p->errors = 0; /* init */
	parg_p->x_mask = ~0x0; /* init, unchanged */

	targ_p->mon_link_id_a = 0x8; /* init */
	targ_p->mon_link_id_b = 0x8; /* init */

	/* 
	 * test of perf_mon_mode & perf_mon_buf_level values
 	 */

	/* manual test */
	msg_printf(DBG,"test_xbow_perf_mon_set: manual test\n");
	/*  vary these and watch the link ctrl reg change */
	targ_p->perf_mon_mode[0]=1;	
	targ_p->perf_mon_mode[1]=1;	
	targ_p->perf_mon_mode[2]=1;	
	targ_p->perf_mon_mode[3]=1;	
	targ_p->perf_mon_mode[4]=1;	
	targ_p->perf_mon_mode[5]=1;	
	targ_p->perf_mon_mode[6]=1;	
	targ_p->perf_mon_mode[7]=1;	
	targ_p->perf_mon_buf_level[0]=0;
	targ_p->perf_mon_buf_level[1]=0;
	targ_p->perf_mon_buf_level[2]=0;
	targ_p->perf_mon_buf_level[3]=0;
	targ_p->perf_mon_buf_level[4]=0;
	targ_p->perf_mon_buf_level[5]=0;
	targ_p->perf_mon_buf_level[6]=0;
	targ_p->perf_mon_buf_level[7]=0;
	if(xbow_perf_mon_set (targ_p, parg_p)) {
		parg_p->errors ++;
	}
	XB_REG_RD_32(XB_LINK_CTRL(XBOW_PORT_8), parg_p->x_mask,
			parg_p->xb_test);
	msg_printf(INFO,"perf_mon_mode=%x perf_mon_buf_level=%x ctrl_reg=%x\n\n",
			targ_p->perf_mon_mode[0], 
			targ_p->perf_mon_buf_level[0],
			parg_p->xb_test);

	/* automated test */
	/* define init values for the arrays */
	for (parg_p->i = 0; parg_p->i <8; parg_p->i++) {
		targ_p->perf_mon_mode[parg_p->i] = 0;
		targ_p->perf_mon_buf_level[parg_p->i] = 0;
	}
	/* link loop */
	for (parg_p->i = 0; parg_p->i <8; parg_p->i++) {
	  /* perf_mon_mode loop: valid values are 0..3  */
	  for (targ_p->perf_mon_mode[parg_p->i] = 0; 
		targ_p->perf_mon_mode[parg_p->i] < 0x04; 
		targ_p->perf_mon_mode[parg_p->i] ++) {
	    /* perf_mon_buf_level loops: valid values are 0..4  */
	    for (targ_p->perf_mon_buf_level[parg_p->i] = 0; 
		targ_p->perf_mon_buf_level[parg_p->i] <0x05; 
		targ_p->perf_mon_buf_level[parg_p->i]++) {
		/* msg_printf(DBG,"ALL: i=%x j=%x perf_mon_mode=%x perf_mon_buf_level=%x \n",
			parg_p->i, parg_p->i, 
			targ_p->perf_mon_mode[parg_p->i], 
			targ_p->perf_mon_buf_level[parg_p->i]);
			parg_p->errors ++;
	        msg_printf(INFO,"111111111          perf_mon_mode[parg_p->i]=0x%x\n",targ_p->perf_mon_mode[parg_p->i]);
		*/
		if ((targ_p->perf_mon_mode[parg_p->i] == MONITOR_INPUT_PACKET_BUFFER_LEVEL) ||
		    ((targ_p->perf_mon_mode[parg_p->i] != MONITOR_INPUT_PACKET_BUFFER_LEVEL) &&
		     targ_p->perf_mon_buf_level[parg_p->i] == 0)) {
		    if(xbow_perf_mon_set (targ_p, parg_p)) {
				parg_p->errors ++;
		    }
		    /* print the control register */
		    XB_REG_RD_32(XB_LINK_CTRL(parg_p->i+XBOW_PORT_8), parg_p->x_mask,
                                                parg_p->xb_test);
		    msg_printf(INFO,"i=%x perf_mon_mode=%x perf_mon_buf_level=%x ctrl_reg=%x\n",
				parg_p->i, 
				targ_p->perf_mon_mode[parg_p->i], 
				targ_p->perf_mon_buf_level[parg_p->i],
				parg_p->xb_test);
		}
	    }
	  }
	  targ_p->perf_mon_mode[parg_p->i] = 0; /* reset value */
	  targ_p->perf_mon_buf_level[parg_p->i] = 0; /* reset value */
	}
	msg_printf(INFO,"test of perf_mon_mode & perf_mon_buf_level: errors = %d\n\n", parg_p->errors);

	/* 
	 * test of mon_link_id_a, mon_link_id_b 
	 * (acceptable values are 0x8...0xf) 
  	 */

	parg_p->errors = 0; /* reset */
	/* mon_link_id_a: */
	for (targ_p->mon_link_id_a = 0x8; targ_p->mon_link_id_a <= 0xf; targ_p->mon_link_id_a ++) {
	    if(xbow_perf_mon_set (targ_p, parg_p)) {
		msg_printf(INFO,"error in xbow_perf_mon_set\n");
		parg_p->errors ++;
	    }
	    XB_REG_RD_32(XBOW_WID_PERF_CTR_A, parg_p->x_mask,
                        parg_p->xb_test);
	    msg_printf(INFO,"targ_p->mon_link_id_a = 0x%x, XBOW_WID_PERF_CTR_A = 0x%x\n",
			targ_p->mon_link_id_a, parg_p->xb_test >> LINK_PERF_MON_SEL_SHIFT);
	}
	/* generate an error */
	targ_p->mon_link_id_a = MON_LINK_ID_MIN - 1;
	if(xbow_perf_mon_set (targ_p, parg_p)) {
		msg_printf(INFO,"error is normal\n");
	}
	/* mon_link_id_b: */
	targ_p->mon_link_id_a = MON_LINK_ID_MIN; /* reset the value */
	for (targ_p->mon_link_id_b = 0x8; targ_p->mon_link_id_b <= 0xf; targ_p->mon_link_id_b ++) {
	    if(xbow_perf_mon_set (targ_p, parg_p)) {
		msg_printf(INFO,"error in xbow_perf_mon_set\n");
		parg_p->errors ++;
	    }
	    XB_REG_RD_32(XBOW_WID_PERF_CTR_B, parg_p->x_mask,
                        parg_p->xb_test);
	    msg_printf(INFO,"targ_p->mon_link_id_b = 0x%x, XBOW_WID_PERF_CTR_B = 0x%x\n",
			targ_p->mon_link_id_b, parg_p->xb_test >> LINK_PERF_MON_SEL_SHIFT);
	}
	/* generate an error */
	targ_p->mon_link_id_b = MON_LINK_ID_MIN - 1;
	if(xbow_perf_mon_set (targ_p, parg_p)) {
		msg_printf(INFO,"error is normal\n");
	}
	msg_printf(INFO,"test of mon_link_id_a & mon_link_id_b: errors = %d\n", parg_p->errors);

	return(0); /* function must return a value */
}

/*
 * Name:	test_xbow_arb_set
 * Description:	tests the xbow aribitration set-up
 * 		This program varies the following inputs:
 *		test_args struc: all links GBR and RR counts, 
 *			plus GBR reload interval
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
test_xbow_arb_set(void)
{
	struct test_args targ;
	struct proc_args parg;
	struct test_args *targ_p;
	struct proc_args *parg_p;
	
	msg_printf(DBG," in test_xbow_arb_set \n");

	targ_p = &targ;
	parg_p = &parg;

	parg_p->errors = 0; /* init */
	parg_p->x_mask = ~0x0; /* init, unchanged */

	/* manual test */

	/* define the arrays */
	msg_printf(DBG,"test_xbow_arb_set: manual test\n");
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    for (parg_p->j = 0; parg_p->j < 8; parg_p->j ++) {
		/* modify these values */
		targ_p->gbr_count[parg_p->i][parg_p->j] = 4*parg_p->i; /* vary! */
		targ_p->rr_count[parg_p->i][parg_p->j] = parg_p->i; /* vary! */
	    }
	}
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    targ_p->gbr_count[parg_p->i][parg_p->i] = LOOPBACK_GBR; 
	    targ_p->rr_count[parg_p->i][parg_p->i] = LOOPBACK_RR; 
	}
	/* print the arrays */
	msg_printf(DBG,"GBR counts:\n");
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    msg_printf(DBG,"%8x %8x %8x %8x ",
			targ_p->gbr_count[parg_p->i][0],
			targ_p->gbr_count[parg_p->i][1],
			targ_p->gbr_count[parg_p->i][2],
			targ_p->gbr_count[parg_p->i][3]);
	    msg_printf(DBG,"%8x %8x %8x %8x\n",
			targ_p->gbr_count[parg_p->i][4],
			targ_p->gbr_count[parg_p->i][5],
			targ_p->gbr_count[parg_p->i][6],
			targ_p->gbr_count[parg_p->i][7]);
	}
	msg_printf(DBG,"RR counts:\n");
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    msg_printf(DBG,"%8x %8x %8x %8x ",
			targ_p->rr_count[parg_p->i][0],
			targ_p->rr_count[parg_p->i][1],
			targ_p->rr_count[parg_p->i][2],
			targ_p->rr_count[parg_p->i][3]);
	    msg_printf(DBG,"%8x %8x %8x %8x\n",
			targ_p->rr_count[parg_p->i][4],
			targ_p->rr_count[parg_p->i][5],
			targ_p->rr_count[parg_p->i][6],
			targ_p->rr_count[parg_p->i][7]);
	}
	msg_printf(DBG,"\n");
	/* make call to function to test */
	if(xbow_arb_set (targ_p, parg_p)) {
		parg_p->errors ++;
	}
	/* print the results */
	/* links 8..b in upper, c..f in lower */
	for (parg_p->i = XBOW_PORT_8; parg_p->i <= XBOW_PORT_F; parg_p->i ++) {
	    XB_REG_RD_32(XB_LINK_ARB_UPPER(parg_p->i), parg_p->x_mask, parg_p->xb_test);
	    XB_REG_RD_32(XB_LINK_ARB_LOWER(parg_p->i), parg_p->x_mask, parg_p->xb_test_2);
	    msg_printf(INFO,"%08x%08x\n", parg_p->xb_test_2, parg_p->xb_test);
	}

	return(0); /* function must return a value */

}
	
