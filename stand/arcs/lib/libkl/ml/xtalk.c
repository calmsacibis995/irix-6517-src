/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.7 $"


#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/xbow.h>
#include <sys/SN/kldiag.h>
#include <promgraph.h>
#include <sys/PCI/bridge.h>

#define IS_BRIDGE_INITTED(b,id)	((b) && ((b->b_wid_control & 0xf) == id))

void
check_and_do_widget_inits(volatile __psunsigned_t xbow_base, int diag_mode)
{
	int 				link = 0;
	int				wid_type = 0, wid_id;
	volatile __psunsigned_t		wid_base;
	bridge_t			*b ;

	while ((link = xbow_get_active_link(xbow_base, link)) >= 0) {
		if (xbow_check_link_hub(xbow_base, link) == 0) {
			wid_base = 
			    SN0_WIDGET_BASE(NASID_GET(xbow_base), link);
			wid_id = io_get_widgetid(NASID_GET(xbow_base), link);
			if (wid_id == -1) {
			    wid_type = -1;
		        }
			else {
			    wid_type = XWIDGET_PART_NUM(wid_id);
		        }
		}
		else
		    wid_type = HUB_WIDGET_PART_NUM;

		if (wid_type == BRIDGE_WIDGET_PART_NUM) {
			b = (bridge_t *)wid_base ;
			/*
			 * bridge_init really gets called only
		 	 * for widgets 8 thro e. Since the default
			 * value of this reg is 0xf, it wont
			 * get called for 0xf. There is no other
			 * bit to differentiate this.
			 */
			if (!IS_BRIDGE_INITTED(b,link)) {
				bridge_init(wid_base, diag_mode) ;
			}
		}
		link++;
	}
}

int
xtalk_init(nasid_t nasid, int diag_mode, int flag)
{
	volatile __psunsigned_t hub_base;
	hubii_ilcsr_t	ii_csr;
	volatile __psunsigned_t 	wid_base ;
	reg32_t		wid_id ;
	jmp_buf		fault_buf;
	void		*old_buf;
	int		rc = 0;

	if (setfault(fault_buf, &old_buf)) {
		printf("*** nasid %d slot %d xtalk_init resulted in an "
			"exception!\n", nasid, 
			SLOTNUM_GETSLOT(get_node_slotid(nasid)));
		restorefault(old_buf);
		return -1;
	}

	hub_base = (__psunsigned_t)REMOTE_HUB(nasid, 0);

	ii_csr.icsr_reg_value = LD(hub_base + IIO_LLP_CSR);
	if (((ii_csr.icsr_fields_s.icsr_lnk_stat & LNK_STAT_WORKING) == 0)
	    || 	(ii_csr.icsr_fields_s.icsr_llp_en == 0)) {
	    restorefault(old_buf);
	    return -1;
	}
	
	wid_base =  SN0_WIDGET_BASE(NASID_GET(hub_base), 0);

	wid_id = LD32(wid_base + WIDGET_ID);

	switch (XWIDGET_PART_NUM(wid_id)) {
	case BRIDGE_WIDGET_PART_NUM:
		/*
		 * If the bridge is directly connected to the hub
		 * the we can use any widget number that we want,
		 * we try and avoid 0 as it has problems with error handling
		 */
		 
		wid_base =  SN0_WIDGET_BASE(NASID_GET(hub_base), 8);
                if (diag_bridgeSanity(diag_mode, wid_base) != KLDIAG_PASSED) {
			moduleid_t	module;
			int		slot;

			get_module_slot(wid_base, &module, &slot);
			printf("diag_bridgeSanity: /hw/module/%d/slot/io%d: FAILED\n",
				module, slot);
		}
		bridge_init(wid_base, diag_mode);
		break;

	case XBOW_WIDGET_PART_NUM:
		xbow_init(wid_base, diag_mode);
		/*
		 * For a headless XBOW - all cpus connected to this disabled -
		 * the widgets do not get initted as there is no cpu to
		 * run discover_xbow_console and hence bridge_init.
		 * A heady (tm) xbow would have called get local console
	 	 * and hence bridge_init.
		 */
		if (flag) 	/* called from init_headless */
			check_and_do_widget_inits(wid_base, diag_mode) ;
		break;
		
	default:
		break;
	}

	restorefault(old_buf);

	return 0;
}
