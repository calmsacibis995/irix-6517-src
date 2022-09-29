/* Parallel (PI1) write test.
 */
#ident	"$Revision: 1.14 $"

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

/* 
 * plp_write - test the parallel port output dma interface
 * (Single descriptor)
 * a printer is required 
 */

int 
plp_write (int argc, char **argv)
{
    jmp_buf faultbuf;
    unsigned char ch;
    int	dma_xfer;
    __int32_t cbp;
    k_machreg_t oldsr = get_SR();
    int timeout;
    int bcount;
    unsigned char *data_buffer = 0;
    struct md *mem_desc = 0;
    int error_count = 0;


	/* PBUS registers */
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

    msg_printf (VRB, "Parallel port write test\n");

    /* Set up strobes */
    *pbus_cfgdma = 0x80108a4;	/* set d4,d5 to max for now */

    if ((argc == 2) && !(strcmp (argv[1], "-r"))) {
	/* reset parallel port logic on HPC3
	 */
	/* enable and reset the printer 
	 */
	*dev_control = 0x0b;
	for (timeout = 0; timeout < PRINTER_RESET_DELAY; timeout++)
	    us_delay (MS1);
	*dev_control = 0x0f;

	/* wait 1 sec for printer to reset
	 */
	us_delay (MS1 * 1000 * 1);
    } 

#ifdef DEBUG
printf("stat=0x%x 0x%x \n",*dev_status,dev_status);
#endif
    if (!(*dev_status & PRINTER_FAULT) || !(*dev_status & PRINTER_ONLINE)) {
	msg_printf (ERR, "Printer fault and/or offline\n");
	msg_printf (DBG, "Status: 0x%02x\n", *dev_status);
	return (FALSE);
    }   /* if */

    /* get buffer and memory descriptor that lie within a page
     */
    data_buffer = (unsigned char *) align_malloc (NCHARS + 3, 0x1000);
    if (data_buffer == NULL) {
	msg_printf (ERR, "Failed to allocate data buffer\n");
	return (FALSE);
    }
    mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
    if (mem_desc == NULL) {
	if (data_buffer)
	    align_free (data_buffer);
	msg_printf (ERR, "Failed to allocate memory descriptor\n");
	return (FALSE);
    }


    for (ch = FIRST_CHAR, bcount = 0; ch <= LAST_CHAR; ch++, bcount++)
	data_buffer[bcount] = ch;
    data_buffer[bcount++] = 0xd; /* same as '\r' */
    data_buffer[bcount++] = 0x0a;/* same as '\n' */

    /* Set up strobes */
    /* original values were:tds=0x7f, tsw=0x70, tdh=0x21 */
    *dev_tval1=0x27;	/* Tds+Tsw+Tdh- Data valid time for PLP writes */
    *dev_tval2=0x13;	/* Tds- data setup to strobe assertion */
    *dev_tval3=0x10;	/* Tsw- strobe pulse width */
    *K1_LIO_2_MASK_ADDR &= 0; /* mask parallel port */
    *K1_LIO_0_MASK_ADDR |= LIO_CENTR_MASK; /* mask parallel port */


    /* execute dma transfers on word, half, and byte boundaries
     */
    for (dma_xfer = 0; dma_xfer < 4; dma_xfer++) {
	if (setjmp (faultbuf)) {
	    if ((_exc_save == EXCEPT_NORM) && 
	        ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
	        (_cause_save & CAUSE_IP3) && 
	        (*K1_LIO_0_ISR_ADDR & LIO_CENTR) && 
	        (*dev_int_stat & PI1_FIFOEM_INT)) { /* any interrupts pending */

		if (*dev_dma_control & PI1_DMA_START) { /*dma still hi?*/
		    error_count++;
		    msg_printf (ERR, "Bad status in printer control ");
		    msg_printf (ERR, "after interrupt: 0x%08x\n", *dev_dma_control);
		}   /* if */
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
	    *K1_LIO_0_ISR_ADDR &= ~LIO_CENTR; 	/* clear printer interrupt */

	    if (GETDESC(*pbus_cbp) != (mem_desc->cbp+(mem_desc->bc&0xff))) {
		int byte=GETDESC(*pbus_cbp)-mem_desc->cbp;
		error_count++;
		msg_printf (ERR, "Not all bytes transfered ");
		msg_printf (ERR, "(%d of %d)\n",byte ,  bcount);
	    }
	} else {
	    /* enable printer to interrupt at dma completion */
	    set_SR(SR_IBIT3 | SR_IEC | (oldsr & ~SR_IMASK));
	    *dev_mask_stat=0xf4; /* PI1 interrupt mask for fifoempty */
	    nofault = faultbuf;

	    cbp = KDM_TO_MCPHYS(data_buffer + dma_xfer);
	    bcount = NCHARS - dma_xfer;

	    mem_desc->bc = 0x80000000 | bcount;
            mem_desc->cbp = KDM_TO_MCPHYS(cbp); /* eox hi */
	    mem_desc->nbdp = 0;
#ifdef DEBUG
printf("Before DMA: mem_desc->bc=0x%x mem_desc->cbp=0x%x, mem_desc->nbdp=0x%x \n",mem_desc->bc,mem_desc->cbp,mem_desc->nbdp);

printf("_0_ISR_ADDR=0x%x\n",*K1_LIO_0_ISR_ADDR);
#endif
	    /* start the printer dma */
	    *pbus_cbp = 0xbad00bad;	/* set to unreasonable value */
	    *pubs_nbdp = KDM_TO_MCPHYS(mem_desc);

	    *pbus_control = 0x00000130; 		/* Start pbus dma */
	    *dev_dma_control = 0x90;		/* set dma direction/start  */

	    for (timeout=0; timeout < PRINTER_DELAY; ++timeout){
		us_delay (MS1);
	    }
#ifdef DEBUG
printf("After DMA: pbus_cbp=0x%x, pbus_control=0x%x , pubs_nbdp=0x%x\n",*pbus_cbp,*pbus_control,*pubs_nbdp);
printf("PI1: stat=0x%x dma_control=0x%x , dev_int_stat=0x%x\n",*dev_status,*dev_dma_control,*dev_int_stat);
printf("_0_ISR_ADDR=0x%x\n",*K1_LIO_0_ISR_ADDR);
#endif

	    /* printer dma did not complete in time */
	    error_count++;
	    *dev_dma_control &= ~PI1_DMA_START; /* stop dma */
	    *dev_dma_control |= PI1_DMA_ABORT;	/* reset PI1's dma */

	    *pbus_control = 0x00000120;		/* Stop pbus dma */

	    msg_printf (ERR, "Time out waiting for printer DMA to complete\n");
	    plp_dbginfo();
	}   /* if */
    }   

    /* disable further interrupts */
    nofault = 0;
    set_SR(oldsr);
    *K1_LIO_0_MASK_ADDR = 0x00;

    if (data_buffer)
	align_free (data_buffer);
    if (mem_desc)
	align_free (mem_desc);

    if (!error_count)
	okydoky();
    return (error_count);
}   /* plp_write */

void
plp_dbginfo(void)
{
    msg_printf (DBG, "Parallel Port Registers\n");
    msg_printf (DBG, "Control 0x%08x, Status 0x%02x\n", 
	*(unsigned char *)PHYS_TO_K1(PAR_CTRL_ADDR),
	*(unsigned char *)PHYS_TO_K1(PAR_SR_ADDR));
    msg_printf (DBG, "NBDP 0x%08x, CBP 0x%08x\n", 
	*(unsigned int *)PHYS_TO_K1(PAR_NBDP_ADDR),
	*(unsigned int *)PHYS_TO_K1(PAR_CBP_ADDR));
}
