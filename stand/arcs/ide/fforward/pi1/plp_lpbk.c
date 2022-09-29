/* Parallel (PI1) loopback test.
 */
#ident	"$Revision: 1.19 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "setjmp.h"
#include "fault.h"
#include "saioctl.h"
#include "uif.h"
#include "plp.h"
#include "libsc.h"
#include "libsk.h"

extern void plp_dbginfo(void);

/* 
 * plp_lpbk - test the parallel port output dma interface
 * (Single descriptor)
 * a printer is required 
 */
#define PBUS_DMASTART           0x08 /* set pbus-ctrl to write dma & start */
#define PBUS_INT_PEND           0x01 /* interrupt pending on reads */
#define PI1_DMA_DIR             0x40 /* 0=write */
#define PI1_DMA_START           0x80 /* 1=start DMA */
#define PI1_BLK_MODE            0x10 /* 1=blk mode */
#define PI1_RICOH_MODE          0x08 /* 1=ricoh mode */
#define PI1_FIFOEM_INT          0x08 /* 1=interrupt active */
#define PI1_DMA_ABORT           0x02 /* 1=reset everything */

#define RESTORE_CHGVOLTBITS(X)	*dev_control=(X)
#define	SAVE_CHGVOLTBITS(X)	(X)=*dev_control


	/* PBUS registers */
volatile unsigned int *pbus_cfgpio = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6));
volatile unsigned int *pbus_cfgdma = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(7));
volatile unsigned int *pbus_control = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(7));
volatile unsigned int *pubs_nbdp = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_DP(7));
volatile unsigned int *pbus_cbp = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_BP(7));

	/* External registers */
volatile unsigned char *dev_control = (unsigned char *)
	PHYS_TO_K1(PAR_CTRL_ADDR);
volatile unsigned char *dev_status = (unsigned char *)
	PHYS_TO_K1(PAR_SR_ADDR);
volatile unsigned char *dev_dma_control = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_DMA_CONTROL+3);
volatile unsigned char *dev_int_stat = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_INT_STAT+3);
volatile unsigned char *dev_mask_stat = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_INT_MASK+3);
volatile unsigned char *dev_tval1    = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_TIMER1+3);
volatile unsigned char *dev_tval2 = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_TIMER2+3);
volatile unsigned char *dev_tval3 = (unsigned char *)
	PHYS_TO_K1(HPC3_PAR_TIMER3+3);


	/*  IOC DMA register */
volatile unsigned char *dev_ioc_dmasel = (unsigned char *)
                PHYS_TO_K1(0x1fbd986b);


int 
plp_lpbk (int argc, char **argv)
{
    jmp_buf faultbuf;
    unsigned char ch;
    int	dma_xfer;
    __uint32_t cbp;
    k_machreg_t oldsr = get_SR();
    int timeout;
    int bcount;
    unsigned char *data_buffer = 0;
    struct md *mem_desc = 0;
    int error_count = 0;
	int saved_chgvolt_bits;
/*  Save the voltage margining values... */

	SAVE_CHGVOLTBITS(saved_chgvolt_bits);	
    if (argc > 1) {
        msg_printf (SUM, "usage: %s \n", argv[0]);
		RESTORE_CHGVOLTBITS(saved_chgvolt_bits);
        return (-1);
    }

    msg_printf (VRB, "Parallel port loopback test\n");
    *dev_ioc_dmasel = 0x7;    /*  Added since the test assumed prom sets this register */
    *pbus_cfgdma = 0x80108a4;     /* set d4,d5 to max for now */

    *dev_dma_control = PI1_RICOH_MODE|PI1_DMA_ABORT; /* reset PI1's dma */
    us_delay (MS1*100);

    /* reset parallel port logic on HPC3
    */
    /* enable and reset the printer 
    */
    *dev_control = 0x0b;
    us_delay (MS1*100);
    *dev_control = 0x0f;
    us_delay (MS1*100);
   
    /* get buffer and memory descriptor that lie within a page
     */
    data_buffer = (unsigned char *) align_malloc (LRWCHARS + 3, 0x1000);
    if (data_buffer == NULL) {
	msg_printf (ERR, "Failed to allocate data buffer\n");
	RESTORE_CHGVOLTBITS(saved_chgvolt_bits);
	return (FALSE);
    }

    mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
    if (mem_desc == NULL) {
	if (data_buffer)
	    align_free (data_buffer);
	msg_printf (ERR, "Failed to allocate memory descriptor\n");
	RESTORE_CHGVOLTBITS(saved_chgvolt_bits);
	return (FALSE);
    }

    for (ch = LFIRST_CHAR, bcount = 0; bcount <= 0xff; ch++, bcount++)
	data_buffer[bcount] = ch;

    /* original values were:tds=0x7f, tsw=0x70, tdh=0x21 */
    *dev_tval1=0x27;	/* Tds+Tsw+Tdh- Data valid time for PLP writes */
    *dev_tval2=0x13;	/* Tds- data setup to strobe assertion */
    *dev_tval3=0x10;	/* Tsw- strobe pulse width */
    *K1_LIO_2_MASK_ADDR &= 0; /* mask parallel port */
    *K1_LIO_0_MASK_ADDR |= LIO_CENTR_MASK; /* mask parallel port */
    *K1_LIO_0_ISR_ADDR = 0x0; 	/* clear interrupt */
    *K1_LIO_1_ISR_ADDR = 0x0; 	/* clear interrupt */
    *K1_LIO_2_ISR_ADDR = 0x0; 	/* clear interrupt */
    dma_xfer = *pbus_control;   /* clear interrupt */

    /* execute dma transfers on word, half, and byte boundaries
     */
	if (setjmp (faultbuf)) {
            if ((_exc_save == EXCEPT_NORM) &&
                ((_cause_save & CAUSE_EXCMASK) == EXC_INT) &&
                (_cause_save & CAUSE_IP3) &&
                (*K1_LIO_0_ISR_ADDR & LIO_CENTR) &&
                (*dev_int_stat & PI1_FIFOEM_INT)) { /* any interrupts pending */
	    } 
	    else {
		error_count++;
		msg_printf (ERR,
		    "Phantom interrupt during parallel port write test\n");
		show_fault ();
		plp_dbginfo();
	    }   /* if */

	    /* printer interrupt clears on read, stop dma */

	    *pbus_control = 0x00000120;		/* Stop pbus dma */
	    *dev_dma_control |= PI1_DMA_ABORT;	/* reset PI1's dma */
	    us_delay (MS1*100);
    	    /* disable further interrupts */
	    *K1_LIO_0_ISR_ADDR &= ~LIO_CENTR; 	/* clear printer interrupt */

	} else {
	    /* enable printer to interrupt at dma completion */
            set_SR(SR_IBIT3 | SR_IEC | (oldsr & ~SR_IMASK));
            nofault = faultbuf;
	    *dev_mask_stat=0xf4; /* PI1 interrupt mask for fifoempty */

	    cbp = KDM_TO_MCPHYS(data_buffer);
	    bcount = LRWCHARS;
	    msg_printf(VRB, "Writing %d bytes\n",bcount);

	    mem_desc->bc = 0x80000000 | bcount;	/* Writing */
            mem_desc->cbp = cbp;		/* eox hi */
	    mem_desc->nbdp = 0;
	    /* start the printer dma */
	    *pbus_cbp = 0xbad00bad;	/* set to unreasonable value */
	    *pubs_nbdp = KDM_TO_MCPHYS(mem_desc);

	   *pbus_control = 0x00000130; 		/* Start pbus dma write */
	   *dev_dma_control = 0x98;		/* set dma direction/start  */

	   for (timeout=0; timeout < PRINTER_DELAY; ++timeout)
		us_delay (MS1);
	   /* printer dma did not complete in time */
	   error_count++;

    	   *dev_control = 0x0f;
	   us_delay (MS1*100);
	   *dev_dma_control &= ~PI1_DMA_START; /* stop dma */
	   us_delay (MS1*100);
	   *dev_dma_control |= (PI1_DMA_ABORT & ~PI1_DMA_DIR);	/* reset PI1's dma */
	   us_delay (MS1*100);

	   *pbus_control = 0x00000120;		/* Stop pbus dma */
	   msg_printf (ERR, "Time out waiting for write DMA to complete\n");
	   plp_dbginfo();

	  *dev_control = 0x0b;
	  us_delay (MS1*100);
	}

	bzero (data_buffer, LRWCHARS + 3);
        *K1_LIO_1_MASK_ADDR = LIO_HPC3_MASK; /* mask hpc3 interrupt */
	if (setjmp (faultbuf)) {
	    if ((_exc_save == EXCEPT_NORM) && 
	        ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
	        (_cause_save & CAUSE_IP4) &&
	        (*K1_LIO_1_ISR_ADDR & LIO_HPC3) &&
		(*pbus_control & PBUS_CTRL_INT_PEND)) {  /* if was reading */
	    } 
	    else {
		error_count++;
		msg_printf (ERR,
		    "Phantom interrupt during parallel port write test\n");
		show_fault ();
		plp_dbginfo();
	    }   /* if */

	    *K1_LIO_1_ISR_ADDR &= ~LIO_HPC3; 	/* clear printer interrupt */
	    /* printer interrupt clears on read, stop dma */
	    *pbus_control = 0x00000120;		/* Stop pbus dma */
	    *dev_dma_control |= PI1_DMA_ABORT;	/* reset PI1's dma */

	} else {
	    /* enable printer to interrupt at dma completion */
	    set_SR(SR_IBIT4 | SR_IEC | (oldsr & ~SR_IMASK));
	    nofault = faultbuf;

	    cbp = KDM_TO_MCPHYS(data_buffer);
	    bcount = LRWCHARS;
	    msg_printf(VRB, "Reading %d bytes back\n",bcount);

	    mem_desc->bc = 0xA0000000 | bcount;	/* Reading */
            mem_desc->cbp = KDM_TO_MCPHYS(cbp); /* eox hi */
	    mem_desc->nbdp = 0;
	    /* start the printer dma */
	    *pbus_cbp = 0xbad00bad;	/* set to unreasonable value */
	    *pubs_nbdp = KDM_TO_MCPHYS(mem_desc);


      	    *pbus_control = 0x00000134;                 /* Start pbus dma */
            /* next:set dma direction/start  */
      	    *dev_control = 0x0b;
      	    *dev_control = 0x09;
      	    *dev_dma_control = 0xd8;
      	    *dev_control = 0x0d;

	    for (timeout=0; timeout < PRINTER_DELAY; ++timeout)
		us_delay (MS1);

	    /* printer dma did not complete in time */
	    error_count++;
	    *dev_dma_control &= ~PI1_DMA_START; /* stop dma */
	    *dev_dma_control |= (PI1_DMA_ABORT & ~PI1_DMA_DIR);	/* reset PI1's dma */

	    *pbus_control = 0x00000120;		/* Stop pbus dma */
	    *dev_control = 0x0f;

	    msg_printf (ERR, "Time out waiting for read DMA to complete\n");
	    plp_dbginfo();
	}   /* if */

    /* resets mode to centronix-write so that when the status is monitored
     * in print1 or print2 we are in the correct state.
     */
    *dev_dma_control = 0x02;
    /* disable further interrupts */
    nofault = 0;
    set_SR(oldsr);

    msg_printf(VRB, "Comparing %d bytes of data read...",GETDESC(*pbus_cbp)-mem_desc->cbp);
    dma_xfer = 0;
    for (bcount=0,ch=LFIRST_CHAR;bcount <= 0xff; ch++, bcount++) {
       if (data_buffer[bcount] != ch) {
	     dma_xfer = 1;
             msg_printf(DBG, "Read data: 0x%02x, Expected data: 0x%02x, count: 0x%03x\n",data_buffer[bcount],ch,bcount);
             error_count++;
       } /* if of mismatched data */
    } /* for */
    if (dma_xfer) msg_printf(ERR, "\n\nComparision Failed\n");
    else msg_printf(SUM, "Passed\n");

    if (data_buffer)
	align_free (data_buffer);
    if (mem_desc)
	align_free (mem_desc);

    if (!error_count)
	okydoky();
	RESTORE_CHGVOLTBITS(saved_chgvolt_bits);
    return (error_count);
}   /* plp_write */
