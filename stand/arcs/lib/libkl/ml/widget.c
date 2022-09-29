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

#ident  "$Revision: 1.3 $"

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/xbow.h>
#include <promgraph.h>


int
io_get_widgetid(nasid_t nasid, int widget)
{
	int wid_id;
    	jmp_buf		fault_buf;
	void		*prev_fault;

	extern void hubii_prb_cleanup(nasid_t, int);

	if (setfault(fault_buf, &prev_fault)) {
		printf("Reading link %X (addr 0x%lx) failed\n", 
			widget, (SN0_WIDGET_BASE(nasid, widget) + WIDGET_ID));
		hubii_prb_cleanup(nasid, widget);
		restorefault(prev_fault);
		return -1;
	}

	wid_id = *(volatile __uint32_t *)(SN0_WIDGET_BASE(nasid, widget) 
					     + WIDGET_ID);
	restorefault(prev_fault);

	return wid_id;
}
