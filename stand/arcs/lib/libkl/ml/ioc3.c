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

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/ioc3.h>
#include <promgraph.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/nvram.h>

__psunsigned_t 
get_ioc3_base(nasid_t nasid)
{
	lboard_t *lboard;
	klioc3_t *ioc3_compt;
	volatile __psunsigned_t ioc3_base;
	__uint64_t  pci_key;
	
	if (nasid == INVALID_NASID) 
	    nasid = get_nasid();

	lboard = (lboard_t *)KL_CONFIG_INFO(nasid);

	while (lboard) {
		if ((lboard = find_lboard(lboard, KLTYPE_BASEIO)) == NULL) {
			/* Set LED here */
			printf("get_ioc3_base: find lboard failed\n");
			break;
		}

		if (lboard->brd_flags & LOCAL_MASTER_IO6) {
		    if ((ioc3_compt = (klioc3_t *)
		     	find_component(lboard, NULL, KLSTRUCT_IOC3)) &&
				(IS_CONSOLE_IOC3(ioc3_compt)))
			break;
		}

#if 0
if (ioc3_compt)
printf("get_ioc3_base: ioc3 on widget 0x%x not a master. skipping ...\n",
        ioc3_compt->ioc3_info.widid);
else
printf("get_ioc3_base: ioc3 not found on slot 0x%x\n", lboard->brd_slot) ;
#endif

		lboard = KLCF_NEXT(lboard);

	}

	if (!lboard) {
		printf("get_ioc3_base: find lboard failed\n");
		return (__psunsigned_t)NULL;
	}

	pci_key = MK_SN0_KEY(nasid, ioc3_compt->ioc3_info.widid,
			       ioc3_compt->ioc3_info.physid);
		     
	ioc3_base = GET_PCIBASE_FROM_KEY(pci_key);
	return ioc3_base;
}

__psunsigned_t
kl_ioc3uart_base(nasid_t nasid)
{
	__psunsigned_t ioc3_base;
	__psunsigned_t ioc3uart_base;

	if (ioc3_base = get_ioc3_base(nasid)) {
		ioc3uart_base = ioc3_base + IOC3_SIO_BASE ;
		return ioc3uart_base;
	}
	return NULL;
}


__psunsigned_t
kl_ioc3nvram_base(nasid_t nasid)
{
	__psunsigned_t ioc3_base;
	__psunsigned_t nvram_base;
    
	if (ioc3_base = get_ioc3_base(nasid)) {
		nvram_base = ioc3_base + IOC3_NVRAM_OFFSET ;
		if (check_nvram(nvram_base, NIC_UNKNOWN))
		    return nvram_base ;
	}
	return NULL;
}

__psunsigned_t
getnv_base_lb(lboard_t *l)
{
        klinfo_t        *k ;

	if (k = (klinfo_t *)find_component(l, NULL, KLSTRUCT_IOC3))
                return ((NODE_SWIN_BASE(l->brd_nasid, k->widid)) |
                          (BRIDGE_DEVIO(k->physid)) | IOC3_NVRAM_OFFSET) ;
	return NULL ;

}


