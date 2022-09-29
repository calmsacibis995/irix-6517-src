#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/gr2hw.h"
#include "libsk.h"
#include "sys/gr2_if.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern struct gr2_hw *base;

/* external utility routines */
extern int dma_error_handler();

#define NUM_SHORT_WORDS	256
#define VSRAM_START     0x7000 

/* Since we are testing stop and restart DMA, I would like to turn the
 * snooping off here. */
int
gr2_cxtsw_dma(argc, argv)
int argc;
char **argv;
{
	int error_count = 0;
	int i,j;
	int vdma_modes = VDMA_SYNC | VDMA_LBURST | VDMA_INCA;
	unsigned char *wbuf, *rbuf; 
	unsigned char save_buf[NUM_SHORT_WORDS*2];
	struct dma_descriptor {
		unsigned int	maddr;
		unsigned int	size;
		unsigned int	stride;
		unsigned int	gaddr;
		unsigned int	mode;
		unsigned int	count;
		unsigned int	mask;
		unsigned int	subst;
		unsigned int	cause;
		unsigned int	ctl;
	} dmasave;
	unsigned char vc1hiaddr, vc1loaddr;
 

   	msg_printf (VRB, "Virtual DMA context switching test\n");

	basic_DMA_setup(0);	/* No snooping */

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 

	/* 128 long words */
	for (i=0; i<NUM_SHORT_WORDS*2; i++) {
   		*wbuf = i & 0xff;
		save_buf[i] = *wbuf++;
	}

	/* writeback-invalidate data cache */
	flush_cache();

	/* set DMA_SYNC source to 0 to synchronize the transfer with vertical
	   retrace. */
	base->hq.dmasync = 0;

        base->vc1.addrhi = VSRAM_START >> 8;
        base->vc1.addrlo = VSRAM_START & 0xff;

	/* DMA read - from memory to vc1 */
	writemcreg(DMA_MEMADR, i = vdma_wphys); 
msg_printf (DBG,"Wrote %x to MEMADR\n", i);
	writemcreg(DMA_STRIDE, i = 1 << 16); 
msg_printf (DBG,"Wrote %x to STRIDE\n", i);
	writemcreg(DMA_SIZE, i = (1 << 16) | (NUM_SHORT_WORDS*2));
msg_printf (DBG,"Wrote %x to SIZE\n", i);

	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync disabled, DMA read */
       	writemcreg(DMA_MODE, i = vdma_modes);
msg_printf (DBG,"Wrote %x to MODE\n", i);

	/* Start DMA */
msg_printf (DBG,"Writing %x to GIO_ADRS\n", 
			i= KDM_TO_PHYS((uint)&base->vc1.sram));
	writemcreg(DMA_GIO_ADRS, i);
	/*
	 * Wait for DMA to begin
	 */
	while ( ! (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)) 
		;

/* ??? We might want to fine tune this number */
#define TIME_TO_STOP_DMA	5

	i = 0;
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		/* stop the DMA */
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA, 0);
msg_printf (DBG,"Wrote 0 to STDMA\n");
			break;
		}
		i++;
		DELAY(1);
	}
	/* Make sure DMA is stopped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)
			;
	/* SAVE the current context */
	/* Save all required VDMA registers, even we know some of
	   them won't be changed here. */
	dmasave.maddr = *(volatile uint *)(PHYS_TO_K1(DMA_MEMADR));
	dmasave.size = *(volatile uint *) (PHYS_TO_K1(DMA_SIZE));
	dmasave.stride = *(volatile uint *) (PHYS_TO_K1(DMA_STRIDE));
	dmasave.count = *(volatile uint *) (PHYS_TO_K1(DMA_COUNT));
	dmasave.gaddr = *(volatile uint *) (PHYS_TO_K1(DMA_GIO_ADR));
	dmasave.mode = *(volatile uint *) (PHYS_TO_K1(DMA_MODE));
	dmasave.mask = *(volatile uint *) (PHYS_TO_K1(DMA_GIO_MASK));
	dmasave.subst = *(volatile uint *) (PHYS_TO_K1(DMA_GIO_SUB));
	dmasave.cause = *(volatile uint *) (PHYS_TO_K1(DMA_CAUSE));
	dmasave.ctl = *(volatile uint *) (PHYS_TO_K1(DMA_CTL));

msg_printf (DBG,"Saved context\n");
msg_printf (DBG,"MEMADR %x SIZE %x STRIDE %x COUNT %x\n",
	dmasave.maddr, dmasave.size, dmasave.stride, dmasave.count);
msg_printf (DBG,"GADDR %x MODE %x CAUSE %x CTL %x\n",
	dmasave.gaddr, dmasave.mode, dmasave.cause, dmasave.ctl);
	
	/* Save the next VC1 address to sram */
	vc1hiaddr = base->vc1.addrhi;
	vc1loaddr = base->vc1.addrlo;
	
	/* Start 2nd context. */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys+0x1000); 

	for (i=0; i<NUM_SHORT_WORDS; i++) {
   		*wbuf++ = i; 
   		*wbuf++ = i; 
	}

	/* writeback-invalidate data cache */
	flush_cache();

        base->vc1.addrhi = (VSRAM_START+NUM_SHORT_WORDS*2) >> 8;
        base->vc1.addrlo = (VSRAM_START+NUM_SHORT_WORDS*2) & 0xff;

	/* DMA read - from memory to vc1 */
	writemcreg(DMA_MEMADR, i = (vdma_wphys+0x1000)); 
msg_printf (DBG,"Wrote %x to MEMADR\n", i);
	writemcreg(DMA_STRIDE, i = 1 << 16); 
msg_printf (DBG,"Wrote %x to STRIDE\n", i);
	writemcreg(DMA_SIZE, i = (1 << 16) | (NUM_SHORT_WORDS*2));
msg_printf (DBG,"Wrote %x to SIZE\n", i);

#if 0
	/*### We might want to change some of the mode setting here !! */
	/*### ie., from cache-flushing to snooping mode */
	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync disabled, DMA read */
       	writemcreg (*(uint *)(PHYS_TO_K1(DMA_MODE, vdma_modes);
#endif

	/* Start DMA */
	writemcreg(DMA_GIO_ADRS, i = KDM_TO_PHYS((uint) &base->vc1.sram ));
	/*
	 * Wait for DMA to begin
	 */
	while ( ! (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)) 
		;
msg_printf (DBG,"Wrote %x to GIO_ADRS\n", i);

	i = 0;
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		/* stop the DMA */
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA, 0);
msg_printf (DBG,"Wrote 0 to STDMA\n");
			break;
		}
		i++;
		DELAY(1);
	}

	/* Make sure the 2nd DMA is stopped */
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40)

	/* writeback-invalidate data cache */
        pdcache_wb_inval(PHYS_TO_K0(vdma_wphys), NUM_SHORT_WORDS*2);

	/* RESTORE the previous(1st) context */
	/* restore DMA registers */
	writemcreg(DMA_MEMADR, i =  dmasave.maddr);
msg_printf (DBG,"Wrote %x to MEMADR\n", i);
	writemcreg(DMA_SIZE, i = dmasave.size);
msg_printf (DBG,"Wrote %x to SIZE\n", i);
	writemcreg(DMA_STRIDE, i = dmasave.stride);
msg_printf (DBG,"Wrote %x to STRIDE\n", i);
	writemcreg(DMA_COUNT, i = dmasave.count);
msg_printf (DBG,"Wrote %x to COUNT\n", i);
	writemcreg(DMA_MODE, i = dmasave.mode);
msg_printf (DBG,"Wrote %x to MODE\n", i);
	writemcreg(DMA_GIO_MASK, i = dmasave.mask);
msg_printf (DBG,"Wrote %x to MASK\n", i);
	writemcreg(DMA_GIO_SUB, i = dmasave.subst);
msg_printf (DBG,"Wrote %x to SUB\n", i);
	writemcreg(DMA_CTL, i = dmasave.ctl);
msg_printf (DBG,"Wrote %x to CTL\n", i);
	writemcreg(DMA_CAUSE, i = dmasave.cause);
msg_printf (DBG,"Wrote %x to CAUSE\n", i);

	/* restart DMA at the saved address */
	base->vc1.addrhi = vc1hiaddr;
	base->vc1.addrlo = vc1loaddr;

msg_printf (DBG,"Writing %x to GIO_ADRS\n", i = dmasave.gaddr);
	writemcreg(DMA_GIO_ADRS, i);

	/* Wait for dma to stop */
	vdma_wait();

	if (dma_error_handler()) {
		msg_printf(ERR, "VC1 SRAM context switching failed\n");
		return -1;
	}

	/* Read back comparsion */
	rbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys); 

        base->vc1.addrhi = (VSRAM_START) >> 8;
        base->vc1.addrlo = (VSRAM_START) & 0xff;

	/* DMA write - from vc1 to memory */
	writemcreg(DMA_MEMADR, i = vdma_rphys); 
msg_printf (DBG,"Wrote %x to MEMADR\n", i);
	writemcreg(DMA_STRIDE, i = 1 << 16); 
msg_printf (DBG,"Wrote %x to STRIDE\n", i);
	writemcreg(DMA_SIZE, i = (1 << 16) | (NUM_SHORT_WORDS*2));
msg_printf (DBG,"Wrote %x to SIZE\n", i);

       	writemcreg(DMA_MODE, i = vdma_modes | VDMA_GTOH);
msg_printf (DBG,"Wrote %x to MODE\n", i);

	/* Start DMA */
msg_printf (DBG,"Writing %x to GIO_ADRS\n", 
			i= KDM_TO_PHYS((uint)&base->vc1.sram));
	writemcreg(DMA_GIO_ADRS, i);

	/* Wait for dma to stop */
	vdma_wait();

	if (dma_error_handler()) {
		msg_printf (ERR, "VC1 SRAM readback DMA failed\n");
		return -1;
	}
	
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys);
	for (i=0; i < NUM_SHORT_WORDS*2; i++) {
		if (*rbuf != save_buf[i]) {
	        	error_count++;
	        	msg_printf(ERR,
	           	"Address: 0x%08x, Expected: 0x%08x, Actual: 0x%08x\n",
		           	(unsigned int) wbuf, save_buf[i], *rbuf);
		}
            	wbuf++; rbuf++;
	}
		
	if (error_count)
		sum_error("VDMA context switching test");
	else
            okydoky ();

	return (error_count);
}
