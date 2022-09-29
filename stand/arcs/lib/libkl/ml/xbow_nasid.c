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


#include <sys/mips_addrspace.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/xbow.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <libkl.h>
#include <setjmp.h>


nasid_t
xbow_get_link_nasid(__psunsigned_t xbow_base, int link)
{
	nasid_t pnasid;
	nasid_t nasid = NASID_GET(xbow_base);
	hubreg_t nsri;
	jmp_buf		fault_buf;	
	void	       *old_buf;

	hubii_setup_bigwin(nasid, link, 0);

	db_printf("nasid %x Reading peer hub nasid: 0x%lx\n", nasid,
	       NODE_BWIN_BASE(nasid, 0) +
	       (PS_UINT_CAST(1) << SWIN_SIZE_BITS) + NI_STATUS_REV_ID);

	if (setfault(fault_buf, &old_buf)) {
		restorefault(old_buf);		    
		    pnasid = INVALID_NASID;
	}
	else {
		/*
		 * XX fixme XX fix the way we get to the big win address.
		 */ 
		nsri = *(HUBREG_CAST (NODE_BWIN_BASE(nasid, 0) +
				      (PS_UINT_CAST(1)<<SWIN_SIZE_BITS) +
				      NI_STATUS_REV_ID));
		restorefault(old_buf);		    
		pnasid = (nsri & NSRI_NODEID_MASK)>>NSRI_NODEID_SHFT;
	}
	return pnasid;
}

