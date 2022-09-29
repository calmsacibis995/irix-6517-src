/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * SN0.c
 *	Support for  SN0 machines
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/xtalk/xwidget.h>
#include "sn1_private.h"
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>


/*
 * get_nasid() returns the physical node id number of the caller.
 */
nasid_t
get_nasid(void)
{
	ni_status_rev_id_u_t ni_sts;

	ni_sts.ni_status_rev_id_regval = LOCAL_HUB_L(NI_STATUS_REV_ID);
	
	return ni_sts.ni_status_rev_id_fld_s.nsri_node_id;
}

int
get_slice(void)
{
	return LOCAL_HUB_L(PI_CPU_NUM);
}

hubreg_t
get_hub_chiprev(nasid_t nasid)
{
	ni_status_rev_id_u_t ni_sts;

	ni_sts.ni_status_rev_id_regval = REMOTE_HUB_L(nasid, NI_STATUS_REV_ID);
	
	return ni_sts.ni_status_rev_id_fld_s.nsri_chiprevision;
}


void
verify_snchip_rev(void)
{
	int min_hub_rev, hub_chip_rev;
	int i;
	nasid_t nasid;

	for (i = 0; i < maxnodes; i++) {	
		nasid = COMPACT_TO_NASID_NODEID(i);
		hub_chip_rev = get_hub_chiprev(nasid);

		if ((hub_chip_rev < min_hub_rev) || (i == 0))
		    min_hub_rev = hub_chip_rev;
	}
	
	if (min_hub_rev < 0) {
		cmn_err(CE_CONT, "NOTICE: Running with obscure snac chips\n");
	}
	
}

