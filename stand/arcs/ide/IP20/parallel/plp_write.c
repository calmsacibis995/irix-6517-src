#ident	"IP12diags/parallel/plp_write.c:  $Revision: 1.9 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "setjmp.h"
#include "fault.h"
#include "saioctl.h"
#include "uif.h"
#include "plp.h"
#include <libsc.h>
#include <libsk.h>

/* 
 * plp_write - test the parallel port output dma interface
 *
 * a printer is required 
 */

int 
plp_write (argc, argv)
int argc;
char **argv;
{
    jmp_buf faultbuf;
    unsigned char ch;
    int	dma_xfer;
    unsigned int cbp;
    unsigned int oldsr = get_SR();
    int timeout;
    int bcount;
    unsigned char *data_buffer = 0;
    struct md *mem_desc = 0;
    int error_count = 0;

    volatile unsigned int *p_control = (unsigned int *)
	PHYS_TO_K1(PAR_CTRL_ADDR);
    volatile unsigned char *p_status = (unsigned char *)
	PHYS_TO_K1(PAR_SR_ADDR);
    volatile unsigned int *p_nbdp = (unsigned int *)
	PHYS_TO_K1(PAR_NBDP_ADDR);
    volatile unsigned int *p_cbp = (unsigned int *)
	PHYS_TO_K1(PAR_CBP_ADDR);
    volatile unsigned int *p_bc = (unsigned int *)
	PHYS_TO_K1(PAR_BC_ADDR);

    msg_printf (VRB, "Parallel port write test\n");

    if ((argc != 2) || strcmp (argv[1], "-r")) {
	/* reset parallel port logic on HPC
	 */
	*p_control = PAR_CTRL_RESET;
	*p_control = 0;

	/* enable and reset the printer 
	 */
	*p_status = 0;
	for (timeout = 0; timeout < PRINTER_RESET_DELAY; timeout++)
	    us_delay (MS1);
	*p_status = PRINTER_RESET;

	/* wait 1 sec for printer to reset
	 */
	us_delay (MS1 * 1000 * 1);
    } 

    if (!(*p_status & PRINTER_FAULT) || !(*p_status & PRINTER_ONLINE)) {
	msg_printf (ERR, "Printer fault and/or offline\n");
	msg_printf (DBG, "Status: 0x%02x\n", *p_status);
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
    data_buffer[bcount++] = '\r';
    data_buffer[bcount++] = '\n';

    /* execute dma transfers on word, half, and byte boundaries
     */
    for (dma_xfer = 0; dma_xfer < 4; dma_xfer++) {
	if (setjmp (faultbuf)) {
	    if ((_exc_save == EXCEPT_NORM) && 
	        ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
	        (_cause_save & CAUSE_IP3) && 
	        (*K1_LIO_0_ISR_ADDR & LIO_CENTR) && 
	        (*p_control & PAR_CTRL_INT)) {

		if (*p_control & PAR_CTRL_STRTDMA) {
		    error_count++;
		    msg_printf (ERR, "Bad status in printer control ");
		    msg_printf (ERR, "after interrupt: 0x%08x\n", *p_control);
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

	    /* clear printer interrupt, stop dma */
	    *p_control &= ~PAR_CTRL_STRTDMA;

	    if (*p_bc != 0) {
		error_count++;
		msg_printf (ERR, "Not all bytes transfered ");
		msg_printf (ERR, "(%d of %d)\n", bcount - *p_bc,  bcount);
	    }

	    cbp = mem_desc->cbp + mem_desc->bc - *p_bc;	/* expected end cbp */
	    if (cbp != *p_cbp) {
		error_count++;
		msg_printf (ERR, "CBP not updated properly ");
		msg_printf (ERR, "(%x should be %x)\n", *p_cbp, cbp);
	    }

	} else {
	    
	    /* enable printer to interrupt at dma completion */
	    set_SR(SR_IBIT3 | SR_IEC | (oldsr & ~SR_IMASK));
	    *K1_LIO_0_MASK_ADDR = LIO_CENTR_MASK;
	    *p_control = PAR_CTRL_INT;
	    nofault = faultbuf;

	    cbp = (unsigned int)&data_buffer[dma_xfer];
	    bcount = NCHARS - dma_xfer;

	    mem_desc->bc = bcount;
	    mem_desc->cbp = 0x80000000 | KDM_TO_PHYS(cbp);
	    mem_desc->nbdp = 0;

	    /* start the printer dma */
	    *p_bc = 0xbad00bad;		/* set to unreasonable value */
	    *p_cbp = 0xbad00bad;	/* set to unreasonable value */
	    *p_nbdp = KDM_TO_PHYS(mem_desc);

	    *p_control = PAR_CTRL_DIAG | PAR_CTRL_MEMTOPP;
	    *p_control |= PAR_CTRL_SOFTACK;
	    *p_control &= ~PAR_CTRL_SOFTACK;	/* generate softack */
	    *p_control |= PAR_CTRL_STRTDMA;	/* start dma */

	    for (timeout=0; timeout < PRINTER_DELAY; ++timeout)
		us_delay (MS1);

	    /* printer dma did not complete in time */
	    error_count++;
	    *p_control &= ~PAR_CTRL_STRTDMA;	/* stop dma */
	    msg_printf (ERR, "Time out waiting for printer DMA to complete\n");
	    plp_dbginfo();
	}   /* if */
    }   /* for */

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

int
plp_dbginfo(void)
{
    msg_printf (DBG, "Parallel Port Registers\n");
    msg_printf (DBG, "Control 0x%08x, Status 0x%02x\n", 
	*(unsigned int *)PHYS_TO_K1(PAR_CTRL_ADDR),
	*(unsigned char *)PHYS_TO_K1(PAR_SR_ADDR));
    msg_printf (DBG, "BC 0x%08x, NBDP 0x%08x, CBP 0x%08x\n", 
	*(unsigned int *)PHYS_TO_K1(PAR_BC_ADDR),
	*(unsigned int *)PHYS_TO_K1(PAR_NBDP_ADDR),
	*(unsigned int *)PHYS_TO_K1(PAR_CBP_ADDR));
}
