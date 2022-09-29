#ident	"IP20diags/parallel/plp_lpbk.c:  $Revision: 1.5 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "setjmp.h"
#include "saioctl.h"
#include "uif.h"
#include "fault.h"
#include "plp.h"
#include <arcs/types.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>

static jmp_buf catch;
static int catch_intr();

extern int error_count;
extern unsigned int *p_control;
extern int master;

/* 
 * plp_lpbk - Parallel port loopback test 
 *
 * This test program require a loopback board connected to parallel port
 * It first write 256 bytes data to the loopback board, read the data 
 * back and check to see if data is correct or not
 */
int 
plp_lpbk (argc, argv)
int argc;
char **argv;
{
    SIGNALHANDLER prev_handler;

    volatile unsigned char *p_status= (unsigned char *) PHYS_TO_K1(PAR_SR_ADDR);

    extern unsigned char *cbp;
    jmp_buf faultbuf;
    int ac = argc;
    unsigned char ch;
    int bcount;
    struct md *mem_desc = 0;

    if (argc > 1) {
	printf ("usage: %s \n", argv[0]);
	return (-1);
    }

    
    msg_printf (VRB, "Parallel port loopback test\n");

    /* disable all interrupts */
    *K1_LIO_0_MASK_ADDR = 0 ;
    *K1_LIO_1_MASK_ADDR = 0 ;

    /* reset parallel port logic on HPC
     */
    *p_control =  PAR_CTRL_RESET;
    *p_control = 0;
    *p_status = PRINTER_PRT;   
    *p_status = PRINTER_RESET | PRINTER_PRT;

    /* get buffers and memory descriptor that lie within a page
     */
    cbp = (unsigned char *) align_malloc (LRWCHARS + 3, 0x1000);
    if (cbp == NULL) {
	msg_printf (ERR, "Failed to allocate output data buffer\n");
	return (-1);
    }
    mem_desc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
    if (mem_desc == NULL) {
	align_free (cbp);
	msg_printf (ERR, "Failed to allocate memory descriptor\n");
	return (-1);
    }

	    bzero (cbp, LRWCHARS + 3);

    /*
     * return on ^C
     */
    prev_handler = Signal(SIGINT, (void(*)())catch_intr);
    if (setjmp (catch)) {
	Signal(SIGINT, prev_handler);
	bzero(catch, sizeof(jmp_buf));
	goto plpl_done;
    }   /* if */

    if (!setjmp (faultbuf)) {
	nofault = faultbuf;
	    /* do write first, then read */
	    for (ch = LFIRST_CHAR, bcount = 0; bcount <= 0xff; ch++, bcount++)
		cbp[bcount] = ch;

	    msg_printf(DBG,"p_control: 0x%8x\n",*p_control);
	    msg_printf(DBG, "Status: 0x%02x before write\n", *p_status);
	    plpl_dma_start (mem_desc, PAR_CTRL_MEMTOPP, cbp, bcount);
	    if (plpl_dma_wait ())
		goto plpl_done;
	/* Check all status are set or not? */
	    if ((*p_status & PRINTER_STATUS_MASK) != PRINTER_STATUS_MASK)  {
	    msg_printf(DBG, "Status: 0x%02x after write\n", *p_status);
	    msg_printf(ERR, "Printer status not clear\n");
	    error_count++;
	    goto plpl_done;
	    }
      msg_printf(DBG, "Writing data: Finished data writting\n");

	    bzero (cbp, LRWCHARS + 3);


	msg_printf(DBG, "Reading Data: About to start dma\n");
    	*p_control =  PAR_CTRL_RESET;
    	*p_control = 0;
    	*p_status = 0;
    	*p_status = ~PRINTER_PRT | PRINTER_RESET;
	    msg_printf(DBG, "Status before read: 0x%02x\n", *p_status);
	    msg_printf(DBG,"Read, after reset,p_control: 0x%8x\n",*p_control);
	    plpl_dma_start (mem_desc, PAR_CTRL_PPTOMEM, cbp, LRWCHARS);
	    if (plpl_dma_wait ()) {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		msg_printf (VRB, "%d bytes requested, %d remaining\n",
		    mem_desc->bc, bytes);
		goto plpl_done;
	    }
	    else {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		((char *)mem_desc->cbp)[bytes] = 0;
		msg_printf (DBG, "Received: 0x%sn", mem_desc->cbp);
	/* Check all status are zero or not? */
	    if(*p_status & PRINTER_STATUS_MASK)  {
	    msg_printf(DBG, "Status: 0x%02x\n", *p_status);
	    msg_printf(ERR, "Printer status not set\n");
	    error_count++;
	    goto plpl_done;
	    }
	    *p_status = 0;
	/* Compare data */
	    for (bcount=0,ch=LFIRST_CHAR;bcount <= 0xff; ch++, bcount++) {
		if (cbp[bcount] != ch) {
		 msg_printf(ERR, "Read data: 0x%02x, Expected data: 0x%02x, count: 0x%03x\n",cbp[bcount],ch,bcount);
		error_count++;
		} /* if of mismatched data */
	    } /* for */ 
	    goto plpl_done;
	  }
    } else {
	msg_printf (ERR, "Unexpected interrupt during parallel test\n");
	error_count++;
	show_fault ();
	plp_dbginfo();
    }

plpl_done:

    if (cbp)
	align_free (cbp);
    if (mem_desc)
	align_free (mem_desc);
    Signal(SIGINT, prev_handler);
/* Reset the hardware before exit */
    *p_control =  PAR_CTRL_RESET;
    *p_control = 0;
    if (!error_count)
	okydoky();
    return error_count;
} /* plp_lpbk */


int
plpl_dma_start (struct md *mem_desc, unsigned dir, unsigned char *cbp,
		unsigned bcount)
{
    *p_control = PAR_CTRL_INT;

    /* create the memory descriptor 
     */
    mem_desc->bc = bcount ? bcount : LRWCHARS;
    mem_desc->cbp = 0x80000000 | KDM_TO_PHYS((unsigned)cbp);
    mem_desc->nbdp = 0;

    /* start the printer dma 
     */
    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR) = 0xbad00bad;
    *(volatile unsigned int *)PHYS_TO_K1(PAR_CBP_ADDR) = 0xbad00bad;
    *(volatile unsigned int *)PHYS_TO_K1(PAR_NBDP_ADDR)=KDM_TO_PHYS(mem_desc);
    *p_control |= PAR_CTRL_STB  | PAR_CTRL_IGNACK | dir;
	    msg_printf(DBG,"p_control before dma: 0x%8x\n",*p_control);
    *p_control |= PAR_CTRL_STRTDMA;
}

int
plpl_dma_wait ()
{
    int timeout;
    for (timeout=0; timeout < PRINTER_DELAY; ++timeout) {
	if (*p_control & PAR_CTRL_INT)
	    return 0;
	us_delay (MS1);
    }
    /* printer dma did not complete in time */
    error_count++;
    if (*p_control & PAR_CTRL_PPTOMEM) {
	*p_control |= PAR_CTRL_FLUSH;
	us_delay(100);
	*p_control &= ~(PAR_CTRL_FLUSH | PAR_CTRL_STRTDMA);
    } 
    else 
	*p_control &= ~PAR_CTRL_STRTDMA;
    msg_printf (ERR, "%s timed out waiting for %s to complete\n",
	master ? "Master" : "Slave",
	*p_control & PAR_CTRL_PPTOMEM ? "read" : "write");
    plp_dbginfo();
    return 1;
}

static int catch_intr ()
{
    if (*p_control & PAR_CTRL_PPTOMEM) {
    	*p_control |= PAR_CTRL_FLUSH;
    	us_delay(100);
        *p_control &= ~(PAR_CTRL_FLUSH | PAR_CTRL_STRTDMA);
    } 
    else 
	*p_control &= ~PAR_CTRL_STRTDMA;
    longjmp (catch, 1);
    /* doesn't return */
}

/* 
	Voltage is changed thro software by writing particular values
	to parallel port registers

	RESET	PRT	Voltage  	Parameter passed

	0	1 	4.8V		0
	1	1 	5.00V		1
	1	0 	5.25V		2
*/


int 
change_voltage(int argc, char *argv[])
{

    volatile unsigned char *p_status= (unsigned char *) PHYS_TO_K1(PAR_SR_ADDR);
    /*
    volatile unsigned int *p_control = (unsigned int*) PHYS_TO_K1(PAR_CTRL_ADDR);
*/

    if ( argc < 2 ) return 1;

    /* reset parallel port logic on HPC
     */
    *p_control =  PAR_CTRL_RESET;
    *p_control = 0;

    switch(atoi(argv[1])) {
     
     case 0: *p_status = PRINTER_PRT;  break ; 
     case 1: *p_status = PRINTER_RESET | PRINTER_PRT;  break ; 
     case 2: *p_status = PRINTER_RESET ;  break ; 
     default: break ;
    }

    return 0;
}
