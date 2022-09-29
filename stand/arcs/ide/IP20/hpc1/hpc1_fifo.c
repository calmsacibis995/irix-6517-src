#ident	"IP12diags/hpc/hpc1_fifo.c: $Revision: 1.3 $"

/* 
 * hpc1_fifo.c - test HPC1 fifo ram
 */

#include "sys/cpu.h"
#include "sys/sbd.h"
#include "uif.h"
#include "hpc1.h"
#include <libsk.h>

extern int xmit_fifo(unsigned int), recv_fifo(unsigned int);
extern int scsi_fifo(unsigned int), par_fifo(unsigned int);
extern int hpc_probe(int);
extern unsigned int *hpc_base(int);

struct fifo {
    char *name;
    int (*fifotest)(unsigned int);
} fifos [] = {
    {"xmit", xmit_fifo},
    {"recv", recv_fifo},
    {"scsi", scsi_fifo},
    {"parallel", par_fifo},
};

static int numfifos = sizeof(fifos) / sizeof (struct fifo);

int
hpc1_fifo ()
{
    int error_count = 0;
    int hpc, i;
    unsigned int hpcbase;

    msg_printf (VRB, "HPC1 fifo test\n");

    for (hpc = 0; hpc < MAXHPCS; ++hpc) {
	if (hpc_probe(hpc)) {
	    hpcbase = (unsigned int)hpc_base(hpc);
	    for (i = 0; i < numfifos; ++i) {
		    msg_printf (VRB, "Testing %s fifo\n", fifos[i].name);
		    error_count += (*fifos[i].fifotest)(hpcbase);
	    }
	}
	else if (hpc == 0)
	    msg_printf (ERR, "HPC 0 not responding\n");
    }

    if (error_count)
	sum_error ("HPC1 fifo test");
    else
	okydoky ();

    return error_count;
}

/*
 * Diagnostic tests for each HPC fifo
 */

unsigned long pats[] = {
    0xaaaaaaaa, ~0xaaaaaaaa,
    0xcccccccc, ~0xcccccccc,
    0xf0f0f0f0, ~0xf0f0f0f0,
    0xff00ff00, ~0xff00ff00,
    0xffff0000, ~0xffff0000,
    0xffffffff, ~0xffffffff
};
#define NWORDPATS (sizeof(pats)/sizeof(unsigned long))

/*
 * Ethernet transmitter fifo
 */

xmitfifo_write(
unsigned int hpcbase, int offset, unsigned int upper, unsigned int lower
)
{
    volatile unsigned int *xpntr = (unsigned int *)(hpcbase + ENET_X_PNTR);
    volatile unsigned int *xfifo = (unsigned int *)(hpcbase + ENET_X_FIFO);

    *xpntr = ((upper & 0xff) << X_FLAG_SHIFT) | 
	    (offset << X_TWP_SHIFT) | (offset << X_TRP_SHIFT); 	wbflush();
    *xfifo = lower;						wbflush();
}

xmitfifo_read(
unsigned int hpcbase, int offset, unsigned int *pupper, unsigned int *plower
)
{
    volatile unsigned int *xpntr = (unsigned int *)(hpcbase + ENET_X_PNTR);
    volatile unsigned int *xfifo = (unsigned int *)(hpcbase + ENET_X_FIFO);

    /* XXX wbflush is because HPC needs a little turnaround time
     * to read back from the fifo
     */
    *xpntr = (offset << X_TRP_SHIFT) | (offset << X_TWP_SHIFT);	wbflush();
    *plower = *xfifo;						wbflush();
    *pupper = *xpntr >> X_FLAG_SHIFT;				wbflush();
}

int 
xmit_fifo(unsigned int hpcbase)
{
    int error_count = 0;
    int patstart;
    int wordidx;
    unsigned int upper, lower;
    int fifoaddr;

    /* disable the seeq chip transmitter so fifo doesn't get
     * changed while we test it.
     */
    *(volatile unsigned char *)(hpcbase+SEEQ_ENET_TX_ADDR) = 0;

    for (patstart = 0; patstart < NWORDPATS; patstart++) {
	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* write pattern to fifo
	     */
	    xmitfifo_write(hpcbase, fifoaddr, pats[wordidx] & 0xff,
		pats[wordidx]);
	    wordidx = ++wordidx % NWORDPATS;
	}

	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    xmitfifo_read(hpcbase, fifoaddr, &upper, &lower);
	    if (((upper & 0xff) != (pats[wordidx] & 0xff))
		    || (lower != pats[wordidx])) {
		msg_printf (VRB, 
		    "Ethernet transmitter fifo error at offset %d\n", fifoaddr);
		if ((upper & 0xff) != (pats[wordidx] & 0xff))
		    msg_printf (VRB, 
		    "Expected 0x%2x, Received 0x%2x\n", pats[wordidx] & 0xff,
		    upper & 0xff);
		if (lower != pats[wordidx])
		    msg_printf (VRB,
		    "Expected 0x%8x, Received 0x%8x\n", pats[wordidx], lower);
		error_count++;
	    }
	    wordidx = ++wordidx % NWORDPATS;
	}
    }

    return error_count;
}

/*
 * Ethernet receiver fifo
 */

recvfifo_write(unsigned int hpcbase, int offset, unsigned int word)
{
    volatile unsigned int *rpntr = (unsigned int *)(hpcbase + ENET_R_PNTR);
    volatile unsigned int *rfifo = (unsigned int *)(hpcbase + ENET_R_FIFO);

    *rpntr = R_RDIR_MP | (offset << R_RWP_SHIFT); 	wbflush();
    *rfifo = word;					wbflush();
}

recvfifo_read(unsigned int hpcbase, int offset, unsigned int *pword)
{
    volatile unsigned int *rpntr = (unsigned int *)(hpcbase + ENET_R_PNTR);
    volatile unsigned int *rfifo = (unsigned int *)(hpcbase + ENET_R_FIFO);

    *rpntr = offset << R_RRP_SHIFT;			wbflush();
    *pword = *rfifo;					wbflush();
}

int 
recv_fifo(unsigned int hpcbase)
{
    int error_count = 0;
    int patstart;
    int wordidx;
    unsigned int word;
    int fifoaddr;

    /* disable the seeq chip receiver so fifo doesn't get
     * changed while we test it.
     */
    *(volatile unsigned char *)(hpcbase+SEEQ_ENET_RX_ADDR) = 0;

    for (patstart = 0; patstart < NWORDPATS; patstart++) {
	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* write pattern to fifo
	     */
	    recvfifo_write(hpcbase, fifoaddr, pats[wordidx]);
	    wordidx = ++wordidx % NWORDPATS;
	}

	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    recvfifo_read(hpcbase, fifoaddr, &word);
	    if (word != pats[wordidx]) {
		msg_printf (VRB,
		    "Ethernet receiver fifo error at offset %d\n", fifoaddr);
		msg_printf (VRB,
		    "Expected 0x%8x, Received 0x%8x\n", pats[wordidx], word);
		error_count++;
	    }
	    wordidx = ++wordidx % NWORDPATS;
	}
    }

    return error_count;
}

/*
 * SCSI fifo
 */

scsififo_write(
unsigned int hpcbase, int offset, unsigned int upper, unsigned int lower
)
{
    volatile unsigned int *spntr = (unsigned int *)(hpcbase + S_PNTR);
    volatile unsigned int *sfifo = (unsigned int *)(hpcbase + S_FIFO);
    volatile unsigned int *sctrl = (unsigned int *)(hpcbase + S_CTRL);

    *sctrl = 0;
    *spntr = ((upper & 0xf) << FLAG_SHIFT) | (offset << MEMP_SHIFT);
    *sfifo = lower;
}

scsififo_read(
unsigned int hpcbase, int offset, unsigned int *pupper, unsigned int *plower
)
{
    volatile unsigned int *spntr = (unsigned int *)(hpcbase + S_PNTR);
    volatile unsigned int *sfifo = (unsigned int *)(hpcbase + S_FIFO);
    volatile unsigned int *sctrl = (unsigned int *)(hpcbase + S_CTRL);

    *sctrl = SCSI_TO_MEM;
    *spntr = offset << MEMP_SHIFT;
    *plower = *sfifo;
    *pupper = *spntr >> FLAG_SHIFT;
}

int
scsi_fifo(unsigned int hpcbase)
{
    int error_count = 0;
    int patstart;
    int wordidx;
    unsigned int upper, lower;
    int fifoaddr;

    for (patstart = 0; patstart < NWORDPATS; patstart++) {
	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* write pattern to fifo
	     */
	    scsififo_write(hpcbase, fifoaddr, pats[wordidx] & 0xff,
		pats[wordidx]);
	    wordidx = ++wordidx % NWORDPATS;
	}

	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    scsififo_read(hpcbase, fifoaddr, &upper, &lower);
	    if (((upper & 0xf) != (pats[wordidx] & 0xf))
		    || (lower != pats[wordidx])) {
		msg_printf (VRB, "SCSI fifo error at offset %d\n", fifoaddr);
		if ((upper & 0xf) != (pats[wordidx] & 0xf))
		    msg_printf (VRB,
		    "Expected 0x%2x, Received 0x%2x\n", pats[wordidx] & 0xf,
		    upper & 0xf);
		if (lower != pats[wordidx])
		    msg_printf (VRB,
		    "Expected 0x%8x, Received 0x%8x\n", pats[wordidx], lower);
		error_count++;
	    }
	    wordidx = ++wordidx % NWORDPATS;
	}
    }

    return error_count;
}

/*
 * Parallel port fifo
 */

parfifo_write(
unsigned int hpcbase, int offset, unsigned int upper, unsigned int lower
)
{
    volatile unsigned int *ppntr = (unsigned int *)(hpcbase + P_PNTR);
    volatile unsigned int *pfifo = (unsigned int *)(hpcbase + P_FIFO);
    volatile unsigned int *pctrl = (unsigned int *)(hpcbase + P_CTRL);

    *pctrl = 0;
    *ppntr = ((upper & 0xf) << FLAG_SHIFT) | (offset << MEMP_SHIFT);
    *pfifo = lower;
}

parfifo_read(
unsigned int hpcbase, int offset, unsigned int *pupper, unsigned int *plower
)
{
    volatile unsigned int *ppntr = (unsigned int *)(hpcbase + P_PNTR);
    volatile unsigned int *pfifo = (unsigned int *)(hpcbase + P_FIFO);
    volatile unsigned int *pctrl = (unsigned int *)(hpcbase + P_CTRL);

    *pctrl = PAR_CTRL_PPTOMEM;
    *ppntr = offset << MEMP_SHIFT;
    *plower = *pfifo;
    *pupper = *ppntr >> FLAG_SHIFT;
}

int
par_fifo(unsigned int hpcbase)
{
    int error_count = 0;
    int patstart;
    int wordidx;
    unsigned int upper, lower;
    int fifoaddr;

    for (patstart = 0; patstart < NWORDPATS; patstart++) {
	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* write pattern to fifo
	     */
	    parfifo_write(hpcbase, fifoaddr, pats[wordidx] & 0xff,
		pats[wordidx]);
	    wordidx = ++wordidx % NWORDPATS;
	}

	wordidx = patstart;
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    parfifo_read(hpcbase, fifoaddr, &upper, &lower);
	    if (((upper & 0xf) != (pats[wordidx] & 0xf))
		    || (lower != pats[wordidx])) {
		msg_printf (VRB,
		    "Parallel port fifo error at offset %d\n", fifoaddr);
		if ((upper & 0xf) != (pats[wordidx] & 0xf))
		    msg_printf (VRB,
		    "Expected 0x%2x, Received 0x%2x\n", pats[wordidx] & 0xf,
		    upper & 0xf);
		if (lower != pats[wordidx])
		    msg_printf (VRB,
		    "Expected 0x%8x, Received 0x%8x\n", pats[wordidx], lower);
		error_count++;
	    }
	    wordidx = ++wordidx % NWORDPATS;
	}
    }

    return error_count;
}
