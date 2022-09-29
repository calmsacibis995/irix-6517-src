/* Parallel (PI1) write tests
 */
#ident	"$Revision: 1.13 $"

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
 * plp_mwrite - test the parallel port output dma interface
 * (Multiple descriptors)
 * a printer is required 
 */

int 
plp_mwrite (int argc, char **argv)
{
    jmp_buf faultbuf;
    unsigned char ch;
    int	dma_xfer;
    __int32_t cbp;
    k_machreg_t oldsr = get_SR();
    int timeout;
    int bcount;
    unsigned char *data_buffer = 0;
    struct md *mem_desc = 0,*head_desc=0,*link_desc;
    int error_count = 0;
    int counter;
    __psunsigned_t desc_addr[5];

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
    *pbus_cfgdma = 0x80108a4;     /* set d4,d5 to max for now */

    if ((argc == 2) && !(strcmp (argv[1], "-r"))) {
	/* reset parallel port logic on HPC3
	 */

	/* enable and reset the printer 
	 */
	*dev_control = 0x0b;
#ifdef DEBUG
printf("RESETING\n");
#endif
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

    for (counter=0;counter!=4;counter++) {
       mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
       if (mem_desc == NULL) {
	   if (data_buffer)
	       align_free (data_buffer);
	   msg_printf (ERR, "Failed to allocate memory descriptor\n");
	   return (FALSE);
       }
       else {
	   if (!head_desc)
		head_desc = (struct md *)(mem_desc);
	   else
		link_desc->nbdp = KDM_TO_MCPHYS(mem_desc);
	   link_desc = (struct md *)(mem_desc);
	   desc_addr[counter] = (__psunsigned_t)mem_desc;
       }
    }
    mem_desc->nbdp = 0;
    mem_desc = head_desc;


#ifdef DEBUG
printf("Cheking link:\n");
for (counter=0;counter!=4;counter++){
	 printf("mem_desc=0x%x mem_desc->nbdp=0x%x\n",
		mem_desc,mem_desc->nbdp);
	 mem_desc= (struct md *)mem_desc->nbdp;
}
mem_desc=head_desc;
#endif

    for (ch = FIRST_CHAR, bcount = 0; ch <= LAST_CHAR; ch++, bcount++)
	data_buffer[bcount] = ch;
    data_buffer[bcount++] = 0xd; /* same as '\r' */
    data_buffer[bcount++] = 0x0a;/* same as '\n' */

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
		plp_dbginfo();
	    } 
	    else {
		error_count++;
		msg_printf (ERR,
		    "Phantom interrupt during parallel port write test\n");
		show_fault ();
		plp_dbginfo();
	    }   /* if */

	    /* printer interrupt clears on read, stop dma */
	    *pbus_control = 0x00000120; 		/* Stop pbus dma */
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

	    cbp = KDM_TO_MCPHYS(&data_buffer[dma_xfer]);
	    bcount = NCHARS - dma_xfer;

	    for (counter=0;counter!=4;counter++) {
		if (counter==3){
	    	  mem_desc->bc = 0x80000000 | bcount; /* eox hi */
	    	  mem_desc->nbdp = 0;
		}else{
	    	  mem_desc->bc = bcount; 
      		  mem_desc->nbdp = KDM_TO_MCPHYS(desc_addr[counter+1]);
		}

            	mem_desc->cbp = cbp; 
	    	mem_desc = (struct md *)desc_addr[counter+1];
	    }
	    mem_desc = head_desc;
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
}   /* plp_mwrite */
