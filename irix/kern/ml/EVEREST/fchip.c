/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/reg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/vmecc.h>



#define FCHIP_TLB_FLUSH_BASE		FCHIP_TLB_FLUSH
#define FCHIP_TLB_FLUSH_MASK		FCHIP_TLB_FLUSH+4

#define FCHIP_TLB_FLUSH_ALL_BASE	0
#define FCHIP_TLB_FLUSH_ALL_MASK	0x1FFFFF
#define FCHIP_TLB_FLUSH_A32_BASE	0x040000	/* 80000000 */
#define FCHIP_TLB_FLUSH_A32_MASK	0x1BFFFF
#define FCHIP_TLB_FLUSH_A24_BASE	0x07F800	/* FFXXXXXX */
#define FCHIP_TLB_FLUSH_A24_MASK	0x1807FF
#define FCHIP_TLB_FLUSH_SCSI_BASE	0x040000
#define FCHIP_TLB_FLUSH_SCSI_MASK	0x1BFFFF


void
fchip_tlbflushall(__psunsigned_t swin)
{
	/* if F tlb lookup collides with invalidate request, invalidate request is dropped */
	/* manual invalidations (clearing the valid bits via PIO) are reliably arbitrated tho */
	EV_SET_REG(swin + FCHIP_TLB_IO0, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO1, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO2, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO3, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO4, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO5, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO6, 0);
	EV_SET_REG(swin + FCHIP_TLB_IO7, 0);
}

void
fchip_tlbflush(dmamap_t* dmamap)
{
	__psunsigned_t swin;

	if (dmamap->dma_type == DMA_VMEA24) {
		swin = vmecc[vmecc_xtoiadap(dmamap->dma_adap)].ioadap.swin;
		EV_SET_REG(swin + FCHIP_TLB_FLUSH_BASE,
		  (evreg_t)(((evreg_t)FCHIP_TLB_FLUSH_A24_BASE << 32) | 
				      FCHIP_TLB_FLUSH_A24_MASK));
	} else if (dmamap->dma_type == DMA_VMEA32) {
		swin = vmecc[vmecc_xtoiadap(dmamap->dma_adap)].ioadap.swin;
		EV_SET_REG(swin + FCHIP_TLB_FLUSH_BASE,
		  (evreg_t)(((evreg_t)FCHIP_TLB_FLUSH_A32_BASE << 32) | 
				      FCHIP_TLB_FLUSH_A32_MASK));
#ifdef SCSI0
	} else if (dmamap->dma_type == DMA_SCSI) {
		swin = scsi[dmamap->dma_adap].ioadap.swin;
		EV_SET_REG(swin + FCHIP_TLB_FLUSH_BASE,
				  FCHIP_TLB_FLUSH_SCSI_BASE);
		EV_SET_REG(swin + FCHIP_TLB_FLUSH,
				  FCHIP_TLB_FLUSH_SCSI_MASK);
#endif
	}
}


void
fchip_intr(eframe_t *ep, void *arg)
{
	/* always panic */
	cmn_err(CE_NOTE, "Fchip Error Interrupt\n");
	everest_error_handler(ep, arg);
}

void
fchip_init(__psunsigned_t swin)
{
	ulong	mastid;
	ulong	fchip_rev;

	/* No Initialization if there is No FCI Master */
	evintr_connect((evreg_t *)(swin + FCHIP_INTR_MAP),
			EVINTR_LEVEL_FCHIP_ERROR,
			SPLMAX,
			EVINTR_DEST_FCHIP_ERROR,
			fchip_intr,
			(void *)(__psint_t)EVINTR_LEVEL_FCHIP_ERROR);

	/* reset FCI */
	EV_SET_REG(swin + FCHIP_SW_FCI_RESET, 0);

	/* init f-chip tlb */
	EV_SET_REG(swin + FCHIP_TLB_BASE, 0);
	fchip_tlbflushall(swin);

	/* Clear the Fchip error registers */
	EV_GET_REG(swin + FCHIP_ERROR_CLEAR);

	mastid = EV_GET_REG(swin+FCHIP_MASTER_ID);
	if (mastid == IO4_ADAP_FCG) {
		fchip_rev = EV_GET_REG(swin + FCHIP_VERSION_NUMBER);
		if (fchip_rev == 1) {
			/* Don't enable cmd overlap on rev 1 F chips */
			EV_SET_REG(swin+FCHIP_MODE, FCHIP_MODE_ORDER_READ_RESP);
		}
		else {
			/* Enable cmd overlap, set DMA read buffer to 4 cache lines,
			 * and set prefetch count to 4.
			 */
			EV_SET_REG(swin+FCHIP_MODE, FCHIP_MODE_ORDER_READ_RESP
				|FCHIP_MODE_CMD_OVERLAP|FCHIP_MODE_DMA_RD_4LINES | (0x4 << 3));
		}
	} else { /* IO4_ADAP_VMECC || IO4_ADAP_HIPPI (IO4_ADAP_HIP1X will override mode later) */
		EV_SET_REG(swin+FCHIP_MODE, 
			FCHIP_MODE_CMD_OVERLAP|FCHIP_MODE_ORDER_READ_RESP);
	}

	if ((mastid == IO4_ADAP_FCG) || (mastid == IO4_ADAP_VMECC) ||
	    (mastid == IO4_ADAP_HIPPI) || (mastid == IO4_ADAP_HIP1A) || (mastid == IO4_ADAP_HIP1B))
	    EV_SET_REG(swin+FCHIP_INTR_RESET_MASK, 1); /* Enable Error intrs */

}

int
pio_mapfix_fci(piomap_t *pmap)
{
	int 		 slot, padap;
	evioacfg_t	*evioa;
	int		 window;


	/* convert adapter # to slot and phys adapter on IO4 */
	slot = pmap->pio_adap / IO4_MAX_PADAPS;
	padap = pmap->pio_adap % IO4_MAX_PADAPS;

	/* sanity check */
	if( (slot > EV_MAX_SLOTS) || (padap == 0) )
		return 1;

	/* check the everest config array */
	evioa = &EVCFGINFO->ecfg_board[slot].eb_io.eb_ioas[padap];

	/* see if an Fchip is present */
	switch( evioa->ioa_type ) {
	case IO4_ADAP_FCHIP:
	case IO4_ADAP_FCG:
	case IO4_ADAP_HIP1A:
	case IO4_ADAP_HIP1B:
	case IO4_ADAP_VMECC:
	case IO4_ADAP_HIPPI:
		break;
	default:
		return 1;
	}

	/* check address ranges */
	if( (pmap->pio_iopaddr + pmap->pio_size) > SWIN_SIZE )
		return 1;

	/* We have an fchip, set up the piomap */
	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;

	pmap->pio_flag = PIOMAP_FIXED;
	pmap->pio_vaddr = pmap->pio_iopaddr + (caddr_t)SWIN_BASE(window, padap);

	return 0;
}
