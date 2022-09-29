/*
 * xbow_arb.c : 
 *	Xbow Arbitration utilities (for the xtalk tests)
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
 * xbow_arb.c - Xbow Arbitration utilities (for the xtalk tests)
 *
 *	functions to be called by tests:
 *		xbow_perf_mon_set: to set what's to be monitored
 *		xbow_arb_set: to set the xbow arbitration
 *		xbow_perf_results: to get the performance results
 */
#ident "ide/godzilla/x_test/xbow_arb.c:  $Revision: 1.3 $"

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
bool_t set_link_monitor_perf(struct test_args *targ_p, struct proc_args *parg_p);
bool_t set_link_arb(struct test_args *targ_p, struct proc_args *parg_p);


/*
 * Name:	xbow_perf_mon_set
 * Description:	sets up the xbow performance monitor
 *			for ALL links
 * Input:	test parameters in test_args struc must be defined.
 *		 in particular: mon_link_id_a, mon_link_id_b
 *			ALL perf_mon_mode[8], perf_mon_buf_level[8]
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     the setting of mon_link_id_a, mon_link_id_b may need
 *		 to be separated from setting ALL perf_mon_mode[8], 
 *		 perf_mon_buf_level[8], as we may want to vary 
 *		 which are monitored more often than "what's monitored".
 * Debug Status: compiles, simulated with test_xbow_arg, not emulated
 */
bool_t
xbow_perf_mon_set(struct test_args *targ_p, struct proc_args *parg_p)
{

	/*
	 * link loop 
	 */
	/* set the mode in the link ctrl reg */
	for (parg_p->index = XBOW_PORT_8; parg_p->index <= XBOW_PORT_F; 
					parg_p->index++) {
		msg_printf(DBG,"xbow_perf_mon_set... setting link %x\n", 
				parg_p->index);
		if (set_link_monitor_perf(targ_p, parg_p)) 
			return(1); /* exit at the first error */
	}

	/*
	 * select which link will be monitored 
	 */
	/* acceptable values are 0x8...0xf */
	if (((targ_p->mon_link_id_a > MON_LINK_ID_MAX) ||
		(targ_p->mon_link_id_a < MON_LINK_ID_MIN)) ||
	   ((targ_p->mon_link_id_b > MON_LINK_ID_MAX) ||
		(targ_p->mon_link_id_b < MON_LINK_ID_MIN)))
		{
		msg_printf(ERR," *** valid values for the id are 0x%x..0x%x (0x%x, 0x%x)\n",
			MON_LINK_ID_MIN, MON_LINK_ID_MAX, 
			targ_p->mon_link_id_a, targ_p->mon_link_id_b);
		return(1);
	}
	msg_printf(INFO,"xbow_perf_mon_set... monitoring only links %x & %x\n",
			targ_p->mon_link_id_a, targ_p->mon_link_id_b);
	parg_p->xbow_wid_perf_ctr_a = (targ_p->mon_link_id_a 
			& (MAX_XBOW_PORTS - 1)) << LINK_PERF_MON_SEL_SHIFT;
	parg_p->xbow_wid_perf_ctr_b = (targ_p->mon_link_id_b 
			& (MAX_XBOW_PORTS - 1)) << LINK_PERF_MON_SEL_SHIFT;
	/* write to xbow arb registers */
	/* NOTE: only 22:20 bits are affected by write */
	XB_REG_WR_32(XBOW_WID_PERF_CTR_A, parg_p->x_mask,
			parg_p->xbow_wid_perf_ctr_a);
	XB_REG_WR_32(XBOW_WID_PERF_CTR_B, parg_p->x_mask,
			parg_p->xbow_wid_perf_ctr_b);

	return(0); /* function must return a value */
}

/*
 * Name:	set_link_monitor_perf
 * Description:	sets up the performance monitor for one link
 * Input:	perf_monitor_mode (0-3), perf_mon_buf_level (0-7 
 *			only if perf_monitor_mode==3)
 * Output:	none
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated with test_xbow_arg, not emulated
 */
bool_t
set_link_monitor_perf(struct test_args *targ_p, struct proc_args *parg_p)
{
	/* acceptable values are 0x0...0x3 for perf_mon_mode */
	/* acceptable values are 0x0...0x4 for perf_mon_buf_level */
	if (targ_p->perf_mon_mode[parg_p->index-XBOW_PORT_8] > 
						PERF_MON_MODE_MAX) {
                msg_printf(ERR," *** valid values for perf_mon_mode are 0x%x..0x%x (0x%x) \nparg_p->index-XBOW_PORT_8=%x\n",
                        PERF_MON_MODE_MIN, PERF_MON_MODE_MAX,
			targ_p->perf_mon_mode[parg_p->index-XBOW_PORT_8],
			parg_p->index-XBOW_PORT_8);
                return(1);
        }
	if (targ_p->perf_mon_buf_level[parg_p->index-XBOW_PORT_8] > 
						PERF_MON_BUF_LEVEL_MAX) {
                msg_printf(ERR," *** valid values for perf_mon_buf_level are 0x%x..0x%x (0x%x)\n",
                        PERF_MON_BUF_LEVEL_MIN, PERF_MON_BUF_LEVEL_MAX,
			targ_p->perf_mon_buf_level[parg_p->index-XBOW_PORT_8]);
                return(1);
	}
		

	msg_printf(DBG,"set_link_monitor_perf...parg_p->index = %x  \n\ttarg_p->perf_mon_mode[parg_p->index-XBOW_PORT_8] = %x\n", parg_p->index, targ_p->perf_mon_mode[parg_p->index-XBOW_PORT_8]);

	/* read the Link(x) control register */
	XB_REG_RD_32(XB_LINK_CTRL(parg_p->index), parg_p->x_mask, 
						parg_p->xb_link_ctrl);

	/* modify it with perf_monitor_mode select */
	parg_p->xb_link_ctrl &=	~XB_CTRL_PERF_CTR_MODE_MSK;
	parg_p->xb_link_ctrl |= 
		((targ_p->perf_mon_mode[parg_p->index-XBOW_PORT_8] 
		<< PERF_MON_MODE_SEL_SHIFT) 
			& XB_CTRL_PERF_CTR_MODE_MSK);
	
	/* modify it with perf_mon_buf_level if applicable */
	if (targ_p->perf_mon_mode[parg_p->index-XBOW_PORT_8] 
		== MONITOR_INPUT_PACKET_BUFFER_LEVEL) {
		parg_p->xb_link_ctrl &= ~XB_CTRL_IBUF_LEVEL_MSK;
		parg_p->xb_link_ctrl |= 
			((targ_p->perf_mon_buf_level[parg_p->index-XBOW_PORT_8] 
			<< INPUT_PACKET_BUF_LEV_SHIFT) & XB_CTRL_IBUF_LEVEL_MSK);
	}
	/* flag a non-zero input level with non-matching mode */
	else if (targ_p->perf_mon_buf_level[parg_p->index-XBOW_PORT_8] != 0) {
		msg_printf(ERR,"\t\t*** perf_mon_buf_level = 0x%x and perf_monitor_mode != 0x%x\n", 
			targ_p->perf_mon_buf_level[parg_p->index-XBOW_PORT_8], 
			MONITOR_INPUT_PACKET_BUFFER_LEVEL);
		return(1);
	}

	/* acceptable values are 0x0...0x3 for perf_mon_mode */
	/* write it back to the xbow register */
	XB_REG_WR_32(XB_LINK_CTRL(parg_p->index), parg_p->x_mask, 
						parg_p->xb_link_ctrl);
	return(0);	
}

/*
 * Name:	xbow_arb_set
 * Description:	sets up the arbitration in xbow (2 regs)
 * Input:	all links GBR and RR counts, plus GBR reload interval
 *		(see Remarks)
 * Output:	none
 * Error Handling: 
 * Side Effects: 
 * Remarks:     GBR[i][i] must be 0x1f, RR[i][i] must be 0
 *		GBR[i][j] can differ from GBR[j][i]
 *		valid values are 0..7 for RR, 0..31 for GBR
 * Debug Status: compiles, simulated with test_xbow_arg, not emulated
 */
bool_t
xbow_arb_set(struct test_args *targ_p, struct proc_args *parg_p) 
{
	msg_printf(DBG," in xbow_arb_set \n");

	/* verify the GBR/RR arrays before doing anything */
	/* acceptable values are 0x0..0x7 for RR */
	/* acceptable values are 0x0..0x31 for GBR */
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    for (parg_p->j = 0; parg_p->j < 8; parg_p->j ++) {
		if ((targ_p->gbr_count[parg_p->i][parg_p->j] > GBR_COUNT_MAX) ||
			(targ_p->rr_count[parg_p->i][parg_p->j] > RR_COUNT_MAX)){
			msg_printf(ERR,"invalid values for GBR/RR counts:\n\t\tGBR within 0x%x..0x%x (0x%x), RR within 0x%x..0x%x (0x%x)\n",
			GBR_COUNT_MIN, GBR_COUNT_MAX, 
			targ_p->gbr_count[parg_p->i][parg_p->j], 
			RR_COUNT_MIN, RR_COUNT_MAX,
			targ_p->rr_count[parg_p->i][parg_p->j]); 
		return(1);
	        }
	    }
	}
	/* the values in the diag (i=j) are meaningless */
	/*   but must be programmed to 0x1f (spec 9.0) */
	for (parg_p->i = 0; parg_p->i < 8; parg_p->i ++) {
	    if (targ_p->gbr_count[parg_p->i][parg_p->i] != LOOPBACK_GBR ) {
		msg_printf(ERR,"invalid value for GBR[%x][%x] (%x): must be %x\n",
			parg_p->i, parg_p->i, 
			targ_p->gbr_count[parg_p->i][parg_p->i], LOOPBACK_GBR);
		return(1);
	    }
	    if (targ_p->rr_count[parg_p->i][parg_p->i] != LOOPBACK_RR ) {
		msg_printf(ERR,"invalid value for RR[%x][%x] (%x): must be %x\n",
			parg_p->i, parg_p->i, 
			targ_p->rr_count[parg_p->i][parg_p->i], LOOPBACK_RR);
		return(1);
	    }
	}

			
	/* load the new reload interval if different from current one */
	XB_REG_RD_32(XBOW_WID_ARB_RELOAD, parg_p->x_mask,
						parg_p->xbow_wid_arb_reload);
	if (targ_p->arb_reload_interval != parg_p->xbow_wid_arb_reload) {
		parg_p->xbow_wid_arb_reload = targ_p->arb_reload_interval
						& XBOW_WID_ARB_RELOAD_INT;
		XB_REG_WR_32(XBOW_WID_ARB_RELOAD, parg_p->x_mask,
						parg_p->xbow_wid_arb_reload);
		}

	/* modify the arbitration registers for each link */
	for (parg_p->link = 0; parg_p->link < MAX_XBOW_PORTS; parg_p->link++) {
		msg_printf(DBG,"xbow_arb_set... setting link %x\n", 
				parg_p->link + XBOW_PORT_8);
		if (set_link_arb(targ_p, parg_p)) 
			return(1); /* exit at the first error */
	}

	return(0); /* function must return a value */
}

/*
 * Name:	set_link_arb
 * Description:	sets up the arbitration registers for one link
 * Input:	gbr_count (0-31), rr_count (0-7) for all other 7 links
 *			parg_p->link= link in question
 * Output:	none
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated with test_xbow_arg, not emulated
 */
bool_t
set_link_arb(struct test_args *targ_p, struct proc_args *parg_p)
{
	/* modify the arbitration registers only if the new values differ */
	for (parg_p->index = XBOW_PORT_8; parg_p->index <= XBOW_PORT_F; 
					parg_p->index++) {
	    /* find out which register to read */
	    if (XBOW_ARB_IS_UPPER(parg_p->index)) {
		XB_REG_RD_32(XB_LINK_ARB_UPPER(parg_p->link+XBOW_PORT_8), 
						parg_p->x_mask,
						parg_p->xb_link_arb);
	    }
	    else {
		XB_REG_RD_32(XB_LINK_ARB_LOWER(parg_p->link+XBOW_PORT_8), 
						parg_p->x_mask,
						parg_p->xb_link_arb);
	    }
	    /* only if new values differ from current ones */
	    if ((((targ_p->gbr_count[parg_p->link][parg_p->index-XBOW_PORT_8] 
		!= parg_p->xb_link_arb >> XB_ARB_GBR_SHFT(parg_p->index) 
			& XB_ARB_GBR_MSK))) 
	       || (((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] 
		!= parg_p->xb_link_arb >> XB_ARB_RR_SHFT(parg_p->index) 
			& XB_ARB_RR_MSK)))) {
	
		parg_p->xb_link_arb &= ~(XB_ARB_GBR_MSK 
			<< XB_ARB_GBR_SHFT(parg_p->index));
		parg_p->xb_link_arb |= 
		    (targ_p->gbr_count[parg_p->link][parg_p->index-XBOW_PORT_8] 
			& XB_ARB_GBR_MSK) 
			<< XB_ARB_GBR_SHFT(parg_p->index);
		parg_p->xb_link_arb &= ~(XB_ARB_RR_MSK 
			<< XB_ARB_RR_SHFT(parg_p->index));
#if 0	/* to investigate how/why ffffffffwas tacked on ... */
		if(parg_p->link==0x4) {
		msg_printf(INFO,"parg_p->link=%x parg_p->index-XBOW_PORT_8=%x XB_ARB_RR_MSK=%x XB_ARB_RR_SHFT(parg_p->index)=%x\n",parg_p->link,parg_p->index-XBOW_PORT_8,XB_ARB_RR_MSK,XB_ARB_RR_SHFT(parg_p->index));
		msg_printf(INFO,"targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] & XB_ARB_RR_MSK=%x\n",targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] & XB_ARB_RR_MSK);
		msg_printf(INFO,"((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] & XB_ARB_RR_MSK) << XB_ARB_RR_SHFT(parg_p->index))=%x\n",((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8]
                        & XB_ARB_RR_MSK)
                        << XB_ARB_RR_SHFT(parg_p->index)));
		msg_printf(INFO,"(xbowreg_t)((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] & XB_ARB_RR_MSK) << XB_ARB_RR_SHFT(parg_p->index))=%x\n",(xbowreg_t)((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8]
                        & XB_ARB_RR_MSK)
                        << XB_ARB_RR_SHFT(parg_p->index)));
		msg_printf(INFO,"truncated  ((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] & XB_ARB_RR_MSK) << XB_ARB_RR_SHFT(parg_p->index))=%x\n",0xffffffff&((__uint64_t)((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8]
                        & XB_ARB_RR_MSK)
                        << XB_ARB_RR_SHFT(parg_p->index))));
		}
#endif
		parg_p->xb_link_arb |= (xbowreg_t)
		    0x00000000ffffffff&((__uint64_t)
		    ((targ_p->rr_count[parg_p->link][parg_p->index-XBOW_PORT_8] 
			& XB_ARB_RR_MSK) << XB_ARB_RR_SHFT(parg_p->index)));
		/* write the data back */
		if (XBOW_ARB_IS_UPPER(parg_p->index)) {
		    XB_REG_WR_32(XB_LINK_ARB_UPPER(parg_p->link+XBOW_PORT_8), 
					parg_p->x_mask, parg_p->xb_link_arb);
	    	}
		else {
		    XB_REG_WR_32(XB_LINK_ARB_LOWER(parg_p->link+XBOW_PORT_8), 
					parg_p->x_mask, parg_p->xb_link_arb);
	        }

	    }

	}

	return(0); /* function must return a value */
}

/*
 * Name:	xbow_perf_results
 * Description:	returns the performance results
 * Input:	none
 * Output:	values in targ struct
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
xbow_perf_results(void)
{
	/* the next 6 lines of code need to be removed when */
	/*  the targ & parg struc are defined in calling program */
	struct test_args targ;
	struct proc_args parg;
	struct test_args *targ_p;
	struct proc_args *parg_p;
	targ_p = &targ;	/* set the pointers */
	parg_p = &parg;

	/* read the performance counters into the targ struct */
	XB_REG_RD_32(XBOW_WID_PERF_CTR_A, parg_p->x_mask,
			targ_p->xbow_wid_perf_ctr_a);
	XB_REG_RD_32(XBOW_WID_PERF_CTR_B, parg_p->x_mask,
			targ_p->xbow_wid_perf_ctr_b);

	/* print the results */
	msg_printf(DBG,"xbow_perf_results... %x for link %x\n", 
			targ_p->xbow_wid_perf_ctr_a, targ_p->mon_link_id_a);
	msg_printf(DBG,"xbow_perf_results... %x for link %x\n", 
			targ_p->xbow_wid_perf_ctr_b, targ_p->mon_link_id_b);
	
	return(0); /* function must return a value */
}
