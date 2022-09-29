#include "cosmo_hw.h"
#include "cosmo_ide.h"

cosmo_idt(int argc, char **argv)
{
	char *self = "cosmo_idt";
	volatile unsigned *d_ctl;
	volatile IDT_FIFO_REGS *idtfifo;
	unsigned d_ctl_value;
	unsigned dummy, i, idtfifo_errcnt = 0, save_errcnt = 0;

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	d_ctl = (unsigned *)(cosmo_regs.jpeg_dma_ctrl);
	idtfifo = (IDT_FIFO_REGS *)cosmo_regs.jpeg_idt_fifo;
	d_ctl_value = *d_ctl;
	msg_printf(DBG, "%s: dma control==0x%x, idtfifo==0x%x\n",
					self, d_ctl, idtfifo);
	msg_printf(DBG, "%s: dma control register==0x%x\n",
						self, d_ctl_value);
	msg_printf(DBG, "%s: default FIFO A->B almost empty==0x%x\n",
				self, idtfifo->idt_fifo_ab_almost_empty&0x1ff);
	msg_printf(DBG, "%s: default FIFO A->B almost full==0x%x\n",
				self, idtfifo->idt_fifo_ab_almost_full&0x1ff);
	msg_printf(DBG, "%s: default FIFO B->A almost empty==0x%x\n",
				self, idtfifo->idt_fifo_ba_almost_empty&0x1ff);
	msg_printf(DBG, "%s: default FIFO B->A almost full==0x%x\n",
				self, idtfifo->idt_fifo_ba_almost_full&0x1ff);
	save_errcnt = 0;
	msg_printf(VRB,"%s: ab almost empty, full range of values test\n", self);
	for ( i=0; i<0x200; i++ ) {
		idtfifo->idt_fifo_ab_almost_empty = i;
		dummy = idtfifo->idt_fifo_ab_almost_empty;
		if ( dummy != i ) {
			msg_printf(ERR, "%s: idt_fifo_ab_almost_empty",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						i, dummy);
			idtfifo_errcnt++;
		}
	}
	msg_printf(INFO,"%s: ab almost empty, walking 1's test\n", self);
	for ( i=0; i<9; i++ ) {
		idtfifo->idt_fifo_ab_almost_empty = 1<<i;
		dummy = idtfifo->idt_fifo_ab_almost_empty;
		if ( dummy != (1<<i) ) {
			msg_printf(ERR, "%s: idt_fifo_ab_almost_empty",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						(1<<i), dummy);
			idtfifo_errcnt++;
		}
	}
	if ( save_errcnt != idtfifo_errcnt ) {
		msg_printf(VRB, "%s: ab almost empty data test failed\n",
								self);
	} else {
		msg_printf(VRB, "%s: ab almost empty data test passed\n",
								self);
	}
	save_errcnt = idtfifo_errcnt;
	msg_printf(INFO,"%s: ab almost full, full range of values test\n", self);
	for ( i=0; i<0x200; i++ ) {
		idtfifo->idt_fifo_ab_almost_full = i;
		dummy = idtfifo->idt_fifo_ab_almost_full;
		if ( dummy != i ) {
			msg_printf(ERR, "%s: idt_fifo_ab_almost_full",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						i, dummy);
			idtfifo_errcnt++;
		}
	}
	msg_printf(INFO,"%s: ab almost full, walking 1's test\n", self);
	for ( i=0; i<9; i++ ) {
		idtfifo->idt_fifo_ab_almost_full = 1<<i;
		dummy = idtfifo->idt_fifo_ab_almost_full;
		if ( dummy != (1<<i) ) {
			msg_printf(ERR, "%s: idt_fifo_ab_almost_full",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						(1<<i), dummy);
			idtfifo_errcnt++;
		}
	}
	if ( save_errcnt != idtfifo_errcnt ) {
		msg_printf(VRB, "%s: ab almost full data test failed\n",
								self);
	} else {
		msg_printf(VRB, "%s: ab almost full data test passed\n",
								self);
	}
	save_errcnt = idtfifo_errcnt;
	msg_printf(INFO,"%s: ba almost empty, full range of values test\n", self);
	for ( i=0; i<0x200; i++ ) {
		idtfifo->idt_fifo_ba_almost_empty = i;
		dummy = idtfifo->idt_fifo_ba_almost_empty;
		if ( dummy != i ) {
			msg_printf(ERR, "%s: idt_fifo_ba_almost_empty",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						i, dummy);
			idtfifo_errcnt++;
		}
	}
	msg_printf(INFO,"%s: ba almost empty, walking 1's test\n", self);
	for ( i=0; i<9; i++ ) {
		idtfifo->idt_fifo_ba_almost_empty = 1<<i;
		dummy = idtfifo->idt_fifo_ba_almost_empty;
		if ( dummy != (1<<i) ) {
			msg_printf(ERR, "%s: idt_fifo_ba_almost_empty",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						(1<<i), dummy);
			idtfifo_errcnt++;
		}
	}
	if ( save_errcnt != idtfifo_errcnt ) {
		msg_printf(VRB, "%s: ba almost empty data test failed\n",
								self);
	} else {
		msg_printf(VRB, "%s: ba almost empty data test passed\n",
								self);
	}
	save_errcnt = idtfifo_errcnt;
	msg_printf(INFO,"%s: ba almost full, full range of values test\n", self);
	for ( i=0; i<0x200; i++ ) {
		idtfifo->idt_fifo_ba_almost_full = i;
		dummy = idtfifo->idt_fifo_ba_almost_full;
		if ( dummy != i ) {
			msg_printf(ERR, "%s: idt_fifo_ba_almost_full",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						i, dummy);
			idtfifo_errcnt++;
		}
	}
	msg_printf(INFO,"%s: ba almost full, walking 1's test\n", self);
	for ( i=0; i<9; i++ ) {
		idtfifo->idt_fifo_ba_almost_full = 1<<i;
		dummy = idtfifo->idt_fifo_ba_almost_full;
		if ( dummy != (1<<i) ) {
			msg_printf(ERR, "%s: idt_fifo_ba_almost_full",
									self);
			msg_printf(ERR, " expected 0x%x, got 0x%x\n",
						(1<<i), dummy);
			idtfifo_errcnt++;
		}
	}
	if ( save_errcnt != idtfifo_errcnt ) {
		msg_printf(VRB, "%s: ba almost full data test failed\n",
								self);
	} else {
		msg_printf(VRB, "%s: ba almost full data test passed\n",
								self);
	}
	save_errcnt = idtfifo_errcnt;
	if ( idtfifo_errcnt ) {
		msg_printf(SUM, "%s: IDT FIFO test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: IDT FIFO test passed\n", self);
		return(0);
	}
}
