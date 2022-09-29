/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/pio.h>
#include <sys/dmamap.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/map.h>
#include <sys/vmereg.h>
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/univ_vmestr.h>

#if UNUSED
/*
 * This file contains SN0-dependent implementation for PIO and DMA
 * mapping.  The machine-independent counterpart is in io/iomap.c.
 */


/*
 * Piomap_t and dmamap_t should be machine-independent.  However, for 
 * historical reasons, they contains machine-dependent fields.  Ideally,
 * such fields should be replaced by a private structure pointed by
 * bushandle.  It is not done, as some of these fields are used by
 * the drivers on ealier systems, and we may have to maintain 
 * compatibility!!
 *
 */

/******************************************************************
 *                            dispatchers
 ******************************************************************/
int
pio_geth(piomap_t *piomap, int bus, int adap, int subtype, iopaddr_t addr, 
	int size)
{
	switch (bus) {
	case ADAP_VME:
		return unvme_pio_geth(piomap, bus, adap, subtype, addr, size);
	default:
		/* NOTE: No PIO handling routines for PCI bus */
		ASSERT(0);
		return RC_ERROR;
	}
}

int
dma_geth(dmamap_t* dmamap, int bus, int adap, int type, int npages, int ps, 
	int flags)
{
	switch(bus){

	case ADAP_PCI:
		return pci_dma_geth(dmamap, bus, adap, type, npages, ps, flags);

	case ADAP_VME:
		switch(type){
		case DMA_A24VME:
		case DMA_A32VME:
		/* case DMA_A64VME: Not defined yet */
			return unvme_dma_geth(dmamap, bus, adap, type, 
							npages, ps, flags);

		default: 
			ASSERT(0);
			return RC_ERROR;
		}

	default:
		ASSERT(0);
		return RC_ERROR;
	}
}
#endif /* UNUSED */

/*ARGSUSED*/
int
pio_geth(piomap_t *piomap, int bus, int adap, int subtype, iopaddr_t addr, 
	int size)
{
	return 0;
}

/*ARGSUSED*/
int
dma_geth(dmamap_t* dmamap, int bus, int adap, int type, int npages, int ps, 
	int flags)
{
	return 0;
}

/*
 * SN0 Specific Initializations needed for piomap
 */
int
piomap_init()
{
	return 0;
}

int
dmamap_init()
{
	return 0;
}
