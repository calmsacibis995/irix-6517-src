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

#ident  "$Revision: 1.6 $"

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/xbow.h>


int diag_xbowSanity(int, __psunsigned_t);


/* initialize a xbow ASIC for SN0 discover */
void
xbow_init(__psunsigned_t xbow_base, int diag_mode)
{
	int hub_link = hub_xbow_link(NASID_GET(xbow_base));
	int	wid_type = 0, wid_id;
	int link = 0;
	volatile __psunsigned_t	wid_base;

	if (xbow_get_master_hub(xbow_base) != hub_link) {
		return;
	}

	if (diag_xbowSanity(diag_mode, xbow_base) != KLDIAG_PASSED) {
		moduleid_t	module;
		char		buf[8];
		nasid_t		nasid = NASID_GET(xbow_base);

		ip27log_getenv(nasid, IP27LOG_MODULE_KEY, buf, "0", 0);
		module = atoi(buf);

		get_slotname(get_node_slotid(nasid), buf);

		printf("diag_xbowSanity: /hw/module/%d/slot/%s/node/xbow: FAILED\n",
			module, buf);
	}
	
        /* Read the status reg to clear errors, pending values, timeouts */
        *(volatile xbowreg_t *)(xbow_base + XBOW_WID_STAT_CLR);

	*(volatile xbowreg_t *) (xbow_base + XBOW_WID_INT_LOWER) = 0;
	*(volatile xbowreg_t *) (xbow_base + XBOW_WID_INT_UPPER) = 0;

	*(volatile xbowreg_t *) 
	    (xbow_base + XBOW_WID_REQ_TO) = XBOW_PKT_TIMEOUT_TICKS ;
	
	*(volatile xbowreg_t *) (xbow_base + XBOW_WID_ERR_CMDWORD) = 0;
	*(volatile xbowreg_t *) (xbow_base + XBOW_WID_CONTROL) = 0x80;
	*(volatile xbowreg_t *) (xbow_base + XBOW_WID_LLP) = XBOW_LLP_CFG;

	while ((link = xbow_get_active_link(xbow_base, link)) >= 0) {
		if (xbow_check_link_hub(xbow_base, link) == 0) {
			wid_base = 
			    SN0_WIDGET_BASE(NASID_GET(xbow_base), link);
			wid_id = io_get_widgetid(NASID_GET(xbow_base), link);
			if (wid_id != -1) 
			    wid_type = XWIDGET_PART_NUM(wid_id);
			else
			    wid_type = -1;
		}
		else
		    wid_type = HUB_WIDGET_PART_NUM;

		switch (wid_type) {
		case BRIDGE_WIDGET_PART_NUM:
			xbow_link_init(xbow_base, link);
			break;
		default:
			break;
		}
		xbow_set_link_credits(xbow_base, link, wid_type);
		link++;
	}
}

