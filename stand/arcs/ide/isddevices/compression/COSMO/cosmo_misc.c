#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <fault.h>
#include "cosmo_hw.h"
#include "cosmo_ide.h"
#include <libsk.h>
#include <libsc.h>

JPEG_Regs cosmo_regs;		/* cosmo register k1 adresses */
unsigned cosmo_err_count;


void
cosmo_board_reset(void)
{
	char *self = "cosmo_board_reset";
	volatile long *reset_addr = cosmo_regs.jpeg_brd_reset;
	volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;

	*reset_addr = 0;
	us_delay(0x2000);
	/* reset/release the resets for FIFO|CL560|IIC */
	cosmo_dma_reset(CL560_RST_N);
	cosmo_dma_reset(IIC_RST_N);
	cosmo_dma_reset(FIFO_RST_N);
	/* reset the vfc */
	vfc_regs->vfc_rev = 0;
	msg_printf(VRB, "%s: cosmo board reset\n", self);
}


cosmo_dma_reset(unsigned reset_flags)
{
	char *self = "cosmo_dma_reset";
	volatile long *dma_ctrl = cosmo_regs.jpeg_dma_ctrl;
	long old_ctrl, new_ctrl;

	old_ctrl = *dma_ctrl;
	/* clear the reset_flags first (reset) */
	new_ctrl = (old_ctrl & ~(reset_flags&ALL_DMA_RESETS));
	*dma_ctrl = new_ctrl;
	/* now add the to the new value (un-reset) */
	new_ctrl = old_ctrl | reset_flags;
	*dma_ctrl = new_ctrl;
}

extern unsigned *fb_dma_buffer;
extern unsigned fb_dma_bufferx[];
cosmo_set_reg_addrs(unsigned cosmo_base)
{
	cosmo_regs.jpeg_id_reg = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_BOARD_ID_OFFSET));
	cosmo_regs.jpeg_brd_reset = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_BOARD_RESET_OFFSET));
	cosmo_regs.jpeg_cl560 = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_560_REGS_OFFSET));
	cosmo_regs.jpeg_swap_cl560 = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_560_SWAP_REGS_OFFSET));
	cosmo_regs.jpeg_timestamp = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_TIMESTAMP_OFFSET));
	cosmo_regs.jpeg_cc_tcr = 
	      (long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_NEXT_CC_TCR_ADDR_OFFSET));
	cosmo_regs.jpeg_ucc_tcr = 
	     (long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_NEXT_UCC_TCR_ADDR_OFFSET));
	cosmo_regs.jpeg_dma_ctrl = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_DMA_CTRL_OFFSET));
	cosmo_regs.jpeg_dma_int = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_DMA_INTERRUPT_OFFSET));
	cosmo_regs.jpeg_dma_int_mask = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_DMA_INT_MASK_OFFSET));
	cosmo_regs.jpeg_dma_cc_reg = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_DMA_CC_REG_OFFSET));
	cosmo_regs.jpeg_dma_ucc_reg = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_DMA_UCC_REG_OFFSET));
	cosmo_regs.jpeg_vfc = 
		(VFC_REGS *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_VFC_REGS_OFFSET));
	cosmo_regs.jpeg_idt_fifo = 
	       (IDT_FIFO_REGS *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_IDT_FIFO_OFFSET));
	cosmo_regs.jpeg_8584_SAA7 = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_SAA7_CTL_OFFSET));
	cosmo_regs.jpeg_uic_pio = 
		(long *)(JPEG_VIRT_ADDR(cosmo_base+JPEG_UIC_PIO_OFFSET));
	cosmo_regs.jpeg_reserve1 = NULL;
	cosmo_regs.jpeg_reserve2 = NULL;
	cosmo_regs.jpeg_reserve3 = NULL;
	cosmo_regs.jpeg_reserve4 = NULL;
	fb_dma_buffer = (unsigned *)(((int)fb_dma_bufferx+0x1000) & 0xfffff000);
}

unsigned cosmo_slot = 0xffffffff;
cosmo_probe(int argc, char **argv)
{
	char *self = "cosmo_probe";

	if ( cosmo_probe_for_it() == -1 ) {
		msg_printf(JUST_DOIT, "%s: No COSMO board in system!\n", self);
		return(-1);
	} else {
		msg_printf(JUST_DOIT, "%s: COSMO board exists in slot %d\n",
							self, cosmo_slot);
		return(0);
	}

}

cosmo_probe_for_it()
{
	char *self = "cosmo_probe_for_it";
	jmp_buf	faultbuf;
	unsigned *cosmo_base;
	unsigned id;

	cosmo_base = (unsigned int *)PHYS_TO_K1(JPEG0_GIO_IDENTR);
	if ( setjmp(faultbuf) ) {
		msg_printf(VRB, "%s: Cosmo (slot 0 at 0x%x) not found\n",
				self, cosmo_base);
	} else {
		nofault = faultbuf;
		id = *cosmo_base;
		nofault = 0;
		if ( (id & 0xff) == COSMO_GIO_ID ) {
			msg_printf(INFO, "%s: slot 0 (at 0x%x), ident==0x%x\n",
						self, cosmo_base, id);
			cosmo_set_reg_addrs((unsigned)cosmo_base);
			cosmo_slot = 0;
			return(0);
		}
	}
	cosmo_base = (unsigned int *)PHYS_TO_K1(JPEG1_GIO_IDENTR);
	if ( setjmp(faultbuf) ) {
		msg_printf(VRB, "%s: Cosmo (slot 1 at 0x%x) not found\n",
				self, cosmo_base);
	} else {
		nofault = faultbuf;
		id = *cosmo_base;
		nofault = 0;
		if ( (id & 0xff) == COSMO_GIO_ID ) {
			msg_printf(INFO, "%s: slot 1 (at 0x%x), ident==0x%x\n",
						self, cosmo_base, id);
			cosmo_set_reg_addrs((unsigned)cosmo_base);
			cosmo_slot = 1;
			return(0);
		}
	}
	cosmo_base = (unsigned int *)PHYS_TO_K1(JPEGGFX_GIO_IDENTR);
	if ( setjmp(faultbuf) ) {
		msg_printf(VRB, "%s: Cosmo (slot 1 at 0x%x) not found\n",
				self, cosmo_base);
	} else {
		nofault = faultbuf;
		id = *cosmo_base;
		nofault = 0;
		if ( (id & 0xff) == COSMO_GIO_ID ) {
			msg_printf(INFO, "%s: slot 1 (at 0x%x), ident==0x%x\n",
						self, cosmo_base, id);
			cosmo_set_reg_addrs((unsigned)cosmo_base);
			cosmo_slot = 1;
			return(0);
		}
	}
	msg_printf(JUST_DOIT, "%s: No Cosmo Board in System!\n", self, id);
	return(-1);
	
}
cosmo_set_vfc_mode(unsigned vfc_mode)
{
	char *self = "cosmo_set_vfc_mode";
        volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;

	msg_printf(DBG, "%s: old vfc_mode==0x%x, ", self,
					(vfc_regs->vfc_mode&0xffff));
	vfc_regs->vfc_mode = vfc_mode & 0xffff;
	msg_printf(DBG, "new vfc_mode==0x%x (0x%x)\n",
			(vfc_regs->vfc_mode&0xffff), (vfc_mode & 0xffff));
}

cosmo_carob_reset(unsigned vfc_flags)
{
	char *self = "cosmo_carob_reset";
        volatile VFC_REGS *vfc_regs = cosmo_regs.jpeg_vfc;
	long old_rev, new_rev;

	old_rev = vfc_regs->vfc_rev;
	new_rev = (old_rev | vfc_flags);
	vfc_regs->vfc_rev = new_rev;
	new_rev = (old_rev & ~vfc_flags);
	vfc_regs->vfc_rev = new_rev;
	msg_printf(DBG, "%s: new vfc_mode==0x%x (0x%x)\n", self,
			(vfc_regs->vfc_rev&0xffff), (vfc_flags & 0xffff));
}

int
cosmo_test_bits(volatile unsigned *reg_address, unsigned num_bits,
		unsigned bits_to_test)
{
	char *self = "test_bits";
	unsigned i, tbit, rbit, errors = 0;

	msg_printf(DBG, "%s: reg_addr==0x%x,num_bits==0x%x,reg_bits==0x%x\n",
				self, reg_address, num_bits, bits_to_test);
	for ( i=0; i<num_bits; i++ ) {
		tbit = (1<<i);
		if ( !(tbit & bits_to_test) ) {
			continue;
		}
		*reg_address = tbit;
		rbit = *reg_address;
		rbit &= bits_to_test;
		if ( rbit != tbit ) {
			msg_printf(ERR, "%s: bit %d failed\n", self, i);
			msg_printf(ERR, "\texpected 0x%x, got 0x%x\n",
								tbit, rbit);
			errors++;
		} else {
			msg_printf(DBG, "%s: bit %d passed\n", self, i);
			msg_printf(DBG, "\texpected 0x%x, got 0x%x\n",
								tbit, rbit);
		}
	}
	return(errors);
}

void
cosmo_comp_print_err(char *cself, unsigned offset,
		   unsigned cexpected, unsigned cgot)
{
	msg_printf(ERR,
		  "%s: offset 0x%x(0x%x) ,expected==0x%x, got 0x%x\n",
		  cself, offset, (offset*4), cexpected, cgot);
#ifdef notdef
	debug(0);
#endif /* notdef */
}
