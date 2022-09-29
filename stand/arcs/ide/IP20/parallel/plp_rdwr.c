#ident	"IP12diags/parallel/plp_rdwr.c:  $Revision: 1.5 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include <fault.h>
#include "setjmp.h"
#include "saioctl.h"
#include "uif.h"
#include "plp.h"
#include <arcs/types.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>

/* 
 * plp_rdwr - test the parallel port input/output dma interface
 *
 * an IP12 running the same diagnostic must be connected to the parallel port
 * the master sends and the slave receives, then they switch.  start
 * the slave end test slightly before the master test for good results
 */

volatile unsigned int *p_control = (unsigned int *) PHYS_TO_K1(PAR_CTRL_ADDR);
static jmp_buf catch;
static int catch_intr ();

int error_count = 0;
unsigned char *cbp = 0;
int master = 1;

int 
plp_rdwr (argc, argv)
int argc;
char **argv;
{
    SIGNALHANDLER			prev_handler;
    jmp_buf faultbuf;
    int ac = argc;
    unsigned char ch;
    int bcount;
    struct md *mem_desc = 0;

    if (argc > 2) {
	printf ("usage: %s [ -m | -s ]\n", argv[0]);
	return (-1);
    }

    while (--argc) {
	++argv;
	if (!strcmp (*argv, "-m"))
	    master = 1;
	else if (!strcmp (*argv, "-s"))
	    master = 0;
    }

    msg_printf (VRB, "Parallel port read/write test\n");

    /* reset parallel port logic on HPC
     */
    *p_control = PAR_CTRL_RESET;
    *p_control = 0;

    /* get buffers and memory descriptor that lie within a page
     */
    cbp = (unsigned char *) align_malloc (RWCHARS + 3, 0x1000);
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

    /*
     * return on ^C
     */
    prev_handler = Signal (SIGINT, (void (*)()) catch_intr);
    if (setjmp (catch)) {
	Signal (SIGINT, prev_handler);
	bzero (catch, sizeof(jmp_buf));
	goto plp_done;
    }   /* if */

    if (!setjmp (faultbuf)) {
	nofault = faultbuf;
	if (master) {
	    /* do write, then read
	     */
	    for (ch = FIRST_CHAR, bcount = 0; ch <= LAST_CHAR; ch++, bcount++)
		cbp[bcount] = ch;

	    plp_dma_start (mem_desc, PAR_CTRL_MEMTOPP, cbp, bcount);
	    if (plp_dma_wait ())
		goto plp_done;

	    bzero (cbp, RWCHARS + 3);

	    plp_dma_start (mem_desc, PAR_CTRL_PPTOMEM, cbp, RWCHARS);
	    if (plp_dma_wait ()) {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		msg_printf (VRB, "%d bytes requested, %d remaining\n",
		    mem_desc->bc, bytes);
		goto plp_done;
	    }
	    else {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		((char *)mem_desc->cbp)[bytes] = 0;
		msg_printf (VRB, "Received: %s\n", mem_desc->cbp);
	    }
	} else {
	    /* do read, then write
	     */
	    bzero (cbp, RWCHARS + 3);

	    plp_dma_start (mem_desc, PAR_CTRL_PPTOMEM, cbp, RWCHARS);
	    if (plp_dma_wait ()) {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		msg_printf (VRB, "%d bytes requested, %d remaining\n",
		    mem_desc->bc, bytes);
		goto plp_done;
	    }
	    else {
		int bytes = mem_desc->bc - 
		    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR);
		((char *)mem_desc->cbp)[bytes] = 0;
		msg_printf (VRB, "Received: %s\n", mem_desc->cbp);
	    }

	    for (ch = FIRST_CHAR, bcount = 0; ch <= LAST_CHAR; ch++, bcount++)
		cbp[bcount] = ch;

	    plp_dma_start (mem_desc, PAR_CTRL_MEMTOPP, cbp, bcount);
	    plp_dma_wait ();
	}
    } else {
	msg_printf (ERR, "Unexpected interrupt during parallel test\n");
	error_count++;
	show_fault ();
	plp_dbginfo();
    }

plp_done:

    if (cbp)
	align_free (cbp);
    if (mem_desc)
	align_free (mem_desc);
    Signal (SIGINT, prev_handler);

    if (!error_count)
	okydoky();
    return error_count;
} /* plp_rdwr */


int
plp_dma_start (struct md *mem_desc, unsigned dir, unsigned char *cbp,
	       unsigned bcount)
{
    *p_control = PAR_CTRL_INT;

    /* create the memory descriptor 
     */
    mem_desc->bc = bcount ? bcount : RWCHARS;
    mem_desc->cbp = 0x80000000 | KDM_TO_PHYS((unsigned)cbp);
    mem_desc->nbdp = 0;

    /* start the printer dma 
     */
    *(volatile unsigned int *)PHYS_TO_K1(PAR_BC_ADDR) = 0xbad00bad;
    *(volatile unsigned int *)PHYS_TO_K1(PAR_CBP_ADDR) = 0xbad00bad;
    *(volatile unsigned int *)PHYS_TO_K1(PAR_NBDP_ADDR)=KDM_TO_PHYS(mem_desc);
    *p_control = PAR_CTRL_DIAG | PAR_CTRL_IGNACK | dir;
    *p_control |= PAR_CTRL_STRTDMA;
}

int
plp_dma_wait(void)
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
