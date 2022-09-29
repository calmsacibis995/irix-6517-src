#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/lg1hw.h"
#include "sys/gfx.h"
#include "sys/lg1.h"
#include "uif.h"
#include "dma.h"

/* external variables */
extern Rexchip *REX;


/* external utility routines */
extern void basic_dma_setup();
extern int dma_error_handler();
extern void rex_clear();
extern void rex_setclt();

/* Since we are testing stop and restart DMA, I would like to turn the
 * snooping off here.  Results need to be verified visually. */
int
lg1_cxtsw_dma(argc, argv)
int argc;
char **argv;
{
	int x, y;
	int linewidth = 512;
	int linecount = 16;
	int i,j;
	unsigned int u;
	unsigned char *wbuf; 
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
	struct light_cx cxsave; 
 
   	msg_printf (DBG, "Virtual DMA context switching test\n");

	x = y = 100;

	basic_DMA_setup(0);


	/* Initialize color map for index 0 to 7 */
	rex_setclt();

	rex_clear();
	vc1_on();

	/*
	 * Initialize data buffer  
	 */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_wphys); 

	for (i=0; i<linecount; i++) 
		for (j=0; j<linewidth; j++) 
	    		*wbuf++ = i % 8;

        flush_cache();

    	REX->set.xstarti = x; 
	REX->set.xendi = x + linewidth - 1;
    	REX->set.ystarti = y;     
	REX->set.yendi = y + (linecount - 1);
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADR, u = vdma_wphys); 
	msg_printf (DBG, "ctxsw: wrote %x to DMA_MEMADR\n", u);

	writemcreg(DMA_SIZE, u = (linecount << 16) | linewidth);
	msg_printf (DBG, "ctxsw: wrote %x to DMA_SIZE\n", u);

	writemcreg(DMA_STRIDE, u = (1 << 16));
	msg_printf (DBG, "ctxsw: wrote %x to DMA_STRIDE\n", u);

	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync disabled, DMA read */

       	writemcreg(DMA_MODE, u = 0x50);
	msg_printf (DBG, "ctxsw: wrote %x to DMA_MODE\n", u);


	/* Start DMA */
	writemcreg(DMA_GIO_ADRS, u = (uint)&REX->go.rwaux1);
	msg_printf (DBG, "ctxsw: wrote %x to DMA_GIO_ADRS\n", u);

	wbflush();	/* flushbus */

/* ??? We might want to fine tune this number */
#define TIME_TO_STOP_DMA	1

	i = 0;
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		/* stop the DMA */
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA, 0);

			wbflush();	/* flushbus */
	msg_printf (DBG, "ctxsw: wrote 0 to DMA_STDMA\n", u);
			break;
		}
		i++;
		DELAY(1);
	}


	/* Make sure DMA is stopped */

	msg_printf (DBG, "ctxsw: waiting for dma to stop\n");
	vdma_wait();
	
	/* SAVE the current context */
	/* Save all required VDMA registers, even we know some of
	   them won't be changed here. */

	msg_printf (DBG, "ctxsw: saving current context\n");

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
	
	/* Save all REX1 registers */
	REXWAIT(REX);

	cxsave.current_xstart = REX->set.xstart.word;
	cxsave.current_ystart = REX->set.ystart.word;
	cxsave.current_colorred = REX->set.colorredf.word;

	REX->p1.set.togglectxt    = 0;


	REXWAIT(REX);
	cxsave.next_xstart = REX->set.xstart.word;
	cxsave.next_ystart = REX->set.ystart.word;
	cxsave.next_colorred = REX->set.colorredf.word;
	cxsave.next_xend = REX->set.xendf.word;
	cxsave.next_yend = REX->set.yendf.word;
	cxsave.next_xsave = REX->set.xsave;
	cxsave.next_command = REX->set.command;
	 
	/* Start another context. */
	wbuf = (unsigned char *) PHYS_TO_K0(vdma_rphys); 

	for (i=0; i<linecount; i++) 
		for (j=0; j<linewidth; j++) 
	    		*wbuf++ = j % 8;
        flush_cache();

	y = 380;
    	REX->set.xstarti = x; 
	REX->set.xendi = x + linewidth - 1;
    	REX->set.ystarti = y;     
	REX->set.yendi = y + (linecount - 1);
    	REX->go.command = ( 0);     /* NO-OP */

    	REX->set.command = ( REX_LO_SRC | COLORAUX | BLOCK | XYCONTINUE | QUADMODE | REX_DRAW);

	msg_printf (DBG, "ctxsw: start dma of new context\n");
	/* DMA read - from memory to gfx */
	writemcreg(DMA_MEMADR, vdma_rphys); 
	writemcreg(DMA_SIZE, (linecount << 16) | linewidth);
	writemcreg(DMA_STRIDE, (1 << 16));

#if 0
	/*### We might want to change some of the mode setting here !! */
	/*### ie., from cache-flushing to snooping mode */
	/* Setup DMA mode: long burst, no snooping, incrementing addressing,
           fill mode disabled, sync disabled, DMA read */
#endif
       	writemcreg(DMA_MODE, 0x50);

	/* Start DMA */
	writemcreg(DMA_GIO_ADRS, (uint)&REX->go.rwaux1);

	i = 0;
	while (*(uint *)(PHYS_TO_K1(DMA_RUN)) & 0x40) {
		/* stop the DMA */
		if (i == TIME_TO_STOP_DMA) {
			writemcreg(DMA_STDMA, 0);
			break;
		}
		i++;
		DELAY(1);
	}
	msg_printf (DBG, "ctxsw: wait for dma complete\n");

	/* Make sure the 2nd DMA is stopped */
	vdma_wait();

	/* RESTORE the previous(1st) context */

	/* restore REX1 context */
	REXWAIT(REX);

	REX->set.xstart.word = cxsave.next_xstart;
	REX->set.ystart.word = cxsave.next_ystart;
	REX->set.xendf.word = cxsave.next_xend;
	REX->set.yendf.word = cxsave.next_yend;
	REX->go.command = REX_NOP;

	REXWAIT(REX);

	REX->set.xstart.word = cxsave.current_xstart;
	REX->set.ystart.word = cxsave.current_ystart;
	REX->set.colorredf.word = cxsave.current_colorred;
	REX->set.xsave = cxsave.next_xsave;; 
	REXWAIT(REX);

	REX->p1.set.togglectxt    = 0;

	REX->set.xstart.word = cxsave.next_xstart;
	REX->set.ystart.word = cxsave.next_ystart;
	REX->set.colorredf.word = cxsave.next_colorred;
	REX->set.xsave = cxsave.next_xsave;
	REX->set.command = cxsave.next_command;
	
	msg_printf (DBG, "ctxsw: restoring original context\n");

	/* restore DMA registers */
	writemcreg(DMA_MEMADR, dmasave.maddr);
	writemcreg(DMA_SIZE, dmasave.size);
	writemcreg(DMA_STRIDE, dmasave.stride);
	writemcreg(DMA_COUNT, dmasave.count);
	writemcreg(DMA_MODE, dmasave.mode);
	writemcreg(DMA_GIO_MASK, dmasave.mask);
	writemcreg(DMA_GIO_SUB, dmasave.subst);
	writemcreg(DMA_CTL, dmasave.ctl);
	writemcreg(DMA_CAUSE, dmasave.cause);

	msg_printf (DBG, "ctxsw: restarting original context\n");

	/* restart DMA */
	writemcreg(DMA_GIO_ADRS, dmasave.gaddr);

	/* Poll until DMA is stopped */

	msg_printf (DBG, "ctxsw: waiting for dma complete\n");
	vdma_wait();

	if (dma_error_handler()) {
		sum_error ("DMA context switching test");
		return -1;
	}
	else
            okydoky ();

	return 0;
}
