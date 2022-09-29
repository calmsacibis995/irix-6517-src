#ident	"IP22diags/hpc/hpc3_fifo.c: $Revision: 1.12 $"

/* 
 * hpc3_fifo.c - test HPC3 fifo ram
 */

#include "uif.h"
#include "hpctest.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

static int scsi0_fifo(unsigned int *);
static int scsi1_fifo(unsigned int *), pbus_fifo(unsigned int *);

struct fifo {
    char *name;
    int (*fifotest)(unsigned int *);
};
static struct fifo fifos [] = {
    {"scsi0", scsi0_fifo},
    {"scsi1", scsi1_fifo},
    {"pbus", pbus_fifo},
};

static int numfifos = sizeof(fifos) / sizeof (struct fifo);

int
hpc3_fifo (int hpc_select)
{
    int error_count = 0;
    int hpc, i;
    unsigned int *hpcbase;

    msg_printf (VRB, "HPC3 fifo test\n");

    for (hpc = 0; hpc < MAXHPC3; ++hpc) {
	if ((hpc_select != MAXHPC3) && ( hpc != hpc_select))
	    continue;
	if (hpc_probe(hpc)) {
	    hpcbase = hpc_base(hpc);
	    for (i = 0; i < numfifos; ++i) {
		    msg_printf (VRB, "Testing %s fifo for HPC3#%x\n", fifos[i].name,hpc);
		    error_count += (*fifos[i].fifotest)(hpcbase);
	    }
	}
	else if (hpc == 0)
	    msg_printf (ERR, "HPC 0 not responding\n");
    }

    if (error_count)
	sum_error ("HPC3 fifo test");
    else
	okydoky ();

    return error_count;
}

/*
 * Diagnostic tests for each HPC fifo
 */

unsigned int pats[] = {
    0xaaaaaaaa, ~0xaaaaaaaa,
    0xcccccccc, ~0xcccccccc,
    0xf0f0f0f0, ~0xf0f0f0f0,
    0xff00ff00, ~0xff00ff00,
    0xffff0000, ~0xffff0000,
    0xffffffff, ~0xffffffff
};
#define NWORDPATS (sizeof(pats)/sizeof(unsigned long))

/*
 * SCSI fifo
 */
void
scsififo_write0(unsigned int *hpcbase, int offset, unsigned int upper,
		unsigned int lower)
{
    volatile unsigned int *sfifo = (unsigned int *)((char *)hpcbase + S_FIFO0 + offset*8);

    if (upper)
    	*sfifo = upper;
    else {
	sfifo++;
	*sfifo = lower;
    }
}

void
scsififo_read0(unsigned int *hpcbase, int offset, unsigned int *pupper,
	       unsigned int *plower,int i)
{
    volatile unsigned int *sfifo = (unsigned int *)((char *)hpcbase + S_FIFO0 + offset*8);

    if (i)
    	*pupper = *sfifo;
    else {
    	sfifo++;
	*plower = *sfifo;
    }
}

static int
scsi0_fifo(unsigned int *hpcbase)
{
    int error_count = 0;
    unsigned int upper, lower;
    int fifoaddr;
    int flag=0;

	msg_printf (VRB, "scsi0 FIFO upper half test...");
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
	   scsififo_write0(hpcbase, fifoaddr, pats[fifoaddr%12], 0); /* upper half */

	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    scsififo_read0(hpcbase, fifoaddr, &upper, &lower, 1);
	    if (upper != pats[fifoaddr%12]) {
		msg_printf (VRB, "SCSI-0 fifo error at offset %d\n", fifoaddr);
		    msg_printf (VRB,
		    "Expected 0x%x, Received 0x%x\n",pats[fifoaddr%12], 
		    upper);
		error_count++;
		flag++;
	     }
	}

	if (flag)
		msg_printf (VRB, "scsi0 FIFO upper half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");
	flag=0;

	msg_printf (VRB, "scsi0 FIFO lower half test...");
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
	    scsififo_write0(hpcbase, fifoaddr, 0, pats[fifoaddr%12]); /* lower half */

	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    scsififo_read0(hpcbase, fifoaddr, &upper, &lower, 0);
	    if (lower != pats[fifoaddr%12]) {
		msg_printf (VRB, "SCSI-0 fifo error at offset %d\n", fifoaddr);
		msg_printf (VRB,
		    "Expected 0x%x, Received 0x%x\n",pats[fifoaddr%12], 
		    lower);
		error_count++;
		flag++;
	    }
	}
	if (flag)
		msg_printf (VRB, "scsi0 FIFO lower half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");

    return error_count;
}


void
scsififo_write1(unsigned int *hpcbase, int offset, unsigned int upper,
		unsigned int lower)
{
    volatile unsigned int *sfifo = (unsigned int *)((char *)hpcbase + S_FIFO1 + (offset*8));

    if (upper)
        *sfifo = upper;
    else {
        sfifo++;
        *sfifo = lower;
    }

}

void
scsififo_read1(unsigned int *hpcbase, int offset, unsigned int *pupper,
	       unsigned int *plower,int i)
{
    volatile unsigned int *sfifo = (unsigned int *)((char *)hpcbase + S_FIFO1 + (offset*8));

    if (i)
        *pupper = *sfifo;
    else {
        sfifo++;
        *plower = *sfifo;
    }


}

static int
scsi1_fifo(unsigned int *hpcbase)
{
    int error_count = 0;
    unsigned int upper, lower;
    int fifoaddr;
    int flag=0;

	msg_printf (VRB, "scsi1 FIFO upper half test...");
        for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
            scsififo_write1(hpcbase, fifoaddr, pats[fifoaddr%12], 0);

        for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
            /* read pattern from fifo and verify
             */
            scsififo_read1(hpcbase, fifoaddr, &upper, &lower, 1);
	    if (upper != pats[fifoaddr%12]) {
		    msg_printf (VRB, "SCSI-1 fifo error at offset %d\n", 
				fifoaddr);
		    msg_printf (VRB,
		    "Expected 0x%x, Received 0x%x\n",pats[fifoaddr%12], 
		    upper);
		error_count++;
		flag++;
	    }
	}
	if (flag)
		msg_printf (VRB, "scsi1 FIFO upper half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");
	flag=0;

	msg_printf (VRB, "scsi1 FIFO lower half test...");
        for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
            scsififo_write1(hpcbase, fifoaddr, 0, pats[fifoaddr%12]);

        for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
            /* read pattern from fifo and verify
             */
            scsififo_read1(hpcbase, fifoaddr, &upper, &lower, 0);
		if (lower != pats[fifoaddr%12]) {
		    msg_printf (VRB, "SCSI-1 fifo error at offset %d\n", 
				fifoaddr);
		    msg_printf (VRB,
		    "Expected 0x%x, Received 0x%x\n",pats[fifoaddr%12], 
		    lower);
		error_count++;
		flag++;
	    }
	}
	if (flag)
		msg_printf (VRB, "scsi1 FIFO lower half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");

    return error_count;
}


/*
 * Pbus fifo
 */
void
pbusfifo_write(unsigned int *hpcbase, int offset, unsigned int upper,
	       unsigned int lower)
{
    volatile unsigned int *pfifo = (unsigned int *)((char *)hpcbase + P_FIFO + (offset*16));

    if (upper) 
        *pfifo = upper;
    else {
         pfifo++;
        *pfifo = lower;
    }
}

static unsigned int *
pbusfifo_read(unsigned int *hpcbase, int offset, unsigned int *pupper,
	      unsigned int *plower, int i)
{
    volatile unsigned int *pfifo = (unsigned int *)((char *)hpcbase + P_FIFO + (offset*8));

    if (i) {	/* Test upper half */
        *pupper = *pfifo;
	*plower = (unsigned int)((__psunsigned_t)pfifo & 0xffffffff);
    } else {
        pfifo++;
        *plower = *pfifo;
        *pupper = (unsigned int)((__psunsigned_t)pfifo & 0xffffffff);
    }
    return (unsigned int *)pfifo;
}

static int
pbus_fifo(unsigned int *hpcbase)
{
    int error_count = 0;
    unsigned int upper, lower;
    int fifoaddr;
    int flag=0;

	msg_printf (VRB, "PBUS FIFO upper half test...");
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
	    pbusfifo_write(hpcbase, fifoaddr, pats[fifoaddr%12], 0);

	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    pbusfifo_read(hpcbase, fifoaddr, &upper, &lower, 1);
            if (upper != pats[fifoaddr%12]) {
                    msg_printf (VRB, "PBUS fifo error at 0x%x, offset %d\n", 
			lower,fifoaddr);
                    msg_printf (VRB,
                        "Expected 0x%x, Received 0x%x\n",pats[fifoaddr%12],
                    upper);
                error_count++;
		flag++;
	    }
	}
	if (flag)
		msg_printf (VRB, "PBUS FIFO upper half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");
	flag=0;

	msg_printf (VRB, "PBUS FIFO lower half test...");
	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) 
	    pbusfifo_write(hpcbase, fifoaddr, 0, pats[fifoaddr%12]);

	for (fifoaddr = 0; fifoaddr < FIFOLEN; fifoaddr++) {
	    /* read pattern from fifo and verify
	     */
	    (void)pbusfifo_read(hpcbase, fifoaddr, &upper, &lower, 0);
            if (lower != pats[fifoaddr%12]) {
                    msg_printf (VRB, "PBUS fifo error at 0x%x, offset %d\n", 
			upper,fifoaddr);
                    msg_printf (VRB, "Expected 0x%x, Received 0x%x\n",
			pats[fifoaddr%12], lower);
                error_count++;
		flag++;
	    }
	}
	if (flag)
		msg_printf (VRB, "PBUS FIFO lower half test FAILED.\n\n");
	else
		msg_printf (VRB, "passed.\n");

    return error_count;
}
