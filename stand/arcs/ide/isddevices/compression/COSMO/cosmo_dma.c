#include <sys/types.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/mc.h>
#include "cosmo_hw.h"
#include "cosmo_struct.h"
#include "cosmo_ide.h"
#include <libsc.h>

TCR_t	*ucc_tcrs = NULL;

static void build_tcr(unsigned *buf,unsigned nbytes);

unsigned initdma_called = 0;
void
initdma(unsigned slot)
{
	char *self = "initdma";
	extern unsigned int _gio64_arb;
	volatile long  *dma_ctrl = cosmo_regs.jpeg_dma_ctrl;

	/* turn on MC DMA, make device master */
	if ( initdma_called ) {
		msg_printf(DBG, "%s: init dma for slot %d NOT DONE\n",
								self, slot);
		return;
	}
	initdma_called++;
	msg_printf(DBG, "%s: init dma for slot %d\n", self, slot);
	if(slot == 0 ){	
		_gio64_arb |= (GIO64_ARB_EXP0_MST |
				  GIO64_ARB_GRX_MST | GIO64_ARB_EXP0_RT);
		writemcreg(GIO64_ARB,_gio64_arb);
	} else {
		_gio64_arb |= (GIO64_ARB_EXP1_MST |
				  GIO64_ARB_GRX_MST | GIO64_ARB_EXP1_RT);
		writemcreg(GIO64_ARB,_gio64_arb);
	}
	*dma_ctrl = 0;
	*dma_ctrl = (TIMESTAMP_RST_N|FIFO_RST_N| CL560_RST_N|IIC_RST_N);
}

int
ucc_dma(unsigned *dma_buffer, unsigned dma_size)
{
	char *self = "ucc_dma";
	int i, looper;
	volatile long  *dma_ctrl = cosmo_regs.jpeg_dma_ucc_reg;
	volatile long  *dma_int_mask = cosmo_regs.jpeg_dma_int_mask;
	volatile long  *local_tcr = cosmo_regs.jpeg_ucc_tcr;
	long dummy;
	extern unsigned cosmo_slot;

	msg_printf(DBG,"%s: buffer==0x%x, size==0x%x\n", self,
						dma_buffer, dma_size);
	msg_printf(DBG,"%s: dma_ctrl==0x%x,dma_int_mask==0x%x,local_tcr==0x%x\n",
			self, dma_ctrl, dma_int_mask, local_tcr);
	build_tcr(dma_buffer, (dma_size*sizeof(int)));
	initdma(cosmo_slot);
	msg_printf(DBG,"%s: dma_int_mask==0x%x,tcr==0x%x,dma_ctrl==0x%x\n",
				self, dma_int_mask, local_tcr, dma_ctrl);
	msg_printf(DBG,"%s: dma_int_mask==0x%x,tcr==0x%x,dma_ctrl==0x%x\n",
				self, *dma_int_mask, *local_tcr, *dma_ctrl);
	*dma_int_mask = 0;	/* want no interrupt */
	*local_tcr = (long )K1_TO_PHYS((unsigned)ucc_tcrs);
	msg_printf(DBG,"%s: dma_int_mask==0x%x,tcr==0x%x,dma_ctrl==0x%x\n",
				self, *dma_int_mask, *local_tcr, *dma_ctrl);
	*dma_ctrl = (UCC_DIR|UCC_RUN);
	looper = 0x2000000;
	while ( ((dummy = *dma_ctrl) & UCC_RUN) && --looper ) {
		if ( !(looper % 0x1000000) ) {
			msg_printf(JUST_DOIT, "%s dma_ctrl==0x%x (0x%x)\n",
					self, dummy, looper);
		}	
	}
	if ( *dma_ctrl & UCC_RUN ) {
		msg_printf(ERR,"%s: DMA did not complete!!! (0x%x,0x%x)\n",
						self, looper, *dma_ctrl);
		cosmo_err_count++;
		return(-1);
	} else {
		msg_printf(DBG,"%s: DMA complete (0x%x,0x%x)\n",
						self, looper, *dma_ctrl);
		return(0);
	}
}

/*
 * build_tcr - build chain of tcr for specified buffer.  Make sure
 *	       correct physical pages are used going across page 
 *	       boundaries.
 */

static void
build_tcr(unsigned *buf,unsigned nbytes)
{
	char *self = "build_tcr";
	int i = 0;
	unsigned 	*dma_physaddr;
	unsigned    offset;
	unsigned tcr_bcount;
	int     len;
	long    dmalen;
	TCR_t *curr_tcr, *nxt_tcr;
  
	msg_printf(DBG,"%s: buf==0x%x, nbytes==0x%x\n", self, buf, nbytes);
	if ( ucc_tcrs == NULL ) {
		/* This is double word alignment, is that enough? */
		ucc_tcrs = (TCR_t *)align_malloc(sizeof(TCR_t)*NUM_UCC_TCRS,4);
	}
	nxt_tcr = curr_tcr = ucc_tcrs;
	nxt_tcr++;
	msg_printf(DBG,"%s: curr_tcr==0x%x, nxt_tcr==0x%x, nbytes==0x%x\n",
					self, curr_tcr, nxt_tcr, nbytes);
	while ( nbytes > 0 ) {
		offset = poff(buf);
		curr_tcr->tcr_paddr =
				(paddr_t)K1_TO_PHYS((unsigned)buf);
		if ( nbytes > UCC_TCR_SIZE ) {
			tcr_bcount = UCC_TCR_SIZE;
		} else {
			tcr_bcount = nbytes;
		}
		if ( (offset+tcr_bcount) > NBPC ) {
			tcr_bcount = (NBPC - offset);
		}
		curr_tcr->tcr_bcnt = tcr_bcount;
		nbytes -= tcr_bcount;
		buf += (tcr_bcount/sizeof(int));
		if ( nbytes <= 0 ) {
			curr_tcr->tcr_bcnt |= JPEG_END_DMA_TCR;
			curr_tcr->next_tcr = NULL;
		} else {
			curr_tcr->next_tcr =
			     (paddr_t)K1_TO_PHYS((unsigned)nxt_tcr);
		}
#ifdef notdef
		msg_printf(DBG,"%s: tcr_paddr==0x%x,bcnt==0x%x,nxt_tcr==0x%x\n",
				self, curr_tcr->tcr_paddr,
				curr_tcr->tcr_bcnt,  curr_tcr->next_tcr);
#endif
		curr_tcr = nxt_tcr;
		nxt_tcr++;
#ifdef notdef
		msg_printf(DBG,"%s: curr_tcr==0x%x, nxt_tcr==0x%x\n",
					self, curr_tcr, nxt_tcr);
#endif
	}
	i = 0;
	do {
		msg_printf(DBG,"%s: TCR[%d/0x%x] 0x%x for 0x%x, next == 0x%x\n",
				self, i, &ucc_tcrs[i], ucc_tcrs[i].tcr_paddr,
				ucc_tcrs[i].tcr_bcnt, ucc_tcrs[i].next_tcr);
	} while (!(ucc_tcrs[i++].tcr_bcnt & JPEG_END_DMA_TCR));
}
