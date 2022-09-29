#include "uif.h"
#include <libsc.h>
#include "sv.h"
#include "ide_msg.h"

#define FIFO_SIZE		0x40000		/* 256K */


/*
 * Freeze fifo 1 (do a "frame grab").  This allows writing of fifo 3
 * without overwriting fifo 1.
 */
void
freezefifo1(void)
{
    /* set CLOCK MODE reg to reset fifo write pointer */
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_GO);

    /* freeze fifo 1 */
    writeParallelData(SV_FUNC_CTL, SV_FIFO3_WRITE);
    delayWritedata(300);  /* wait for fifo1 to freeze */
}


/*
 * Read fifo 1 contents.  This requires the frame to be frozen,
 * and then copied through the fifos.
 */
int
readfifo1(long *array)
{
    int i;
    volatile register long *DmaData;

    /* freeze fifo 1 */
    writeParallelData(SV_FUNC_CTL, SV_FIFO3_WRITE);
    delayWritedata(300);  /* wait for fifo1 to freeze */

    /* set Bus Control register to send fifo1 contents to GIO bus */
    writeParallelData(SV_BUS_CTL, SV_BUS_READ_FIFO1);

    /*
     * Set CLOCK MODE reg to reset the fifo read pointer.  This allows
     * us to read the contents of fifo 1 from the host.
     */
    writeParallelData(SV_CLOCK_MODE, SV_FIFO1_READ_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_FIFO1_READ_GO);

    /* Do the appropriate bus steering stuff. */
    writeParallelData(SV_DMACTL, SV_FIFO2GIO_SETUP1);
    writeParallelData(SV_DMACTL, SV_FIFO2GIO_SETUP2);
    
    DmaData = (volatile long *)&theSVboard[SV_DMADAT];

    /* do some dummy reads to get things started */
    msg_printf(DBG, "  dummy readback started ...\n");
    for (i = 0; i < 130; i++) {
	array[i] = (*DmaData & 0xffffff);
    }

    /* Reset the fifo read pointer again. */
    writeParallelData(SV_CLOCK_MODE, SV_FIFO1_READ_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_FIFO1_READ_GO);

    /* OK, now we're really ready */
    msg_printf(DBG, "  fifo1 readback started ...\n");
    for (i = 0; i < FIFO_SIZE; i++) {
	array[i] = (*DmaData & 0xffffff);
    }
    msg_printf(DBG, "  fifo1 readback finished\n");

    return (i * sizeof(long));
}

/*
 * Write data to fifos 1 & 3
 */
int
writefifos(long *array)
{
    int i, writes;
    volatile register long *DmaData;

    /* disable Frame Grab to allow writing to fifos 1 & 3 */
    writeParallelData(SV_FUNC_CTL, SV_FUNC_CTL_INIT);
    delayWritedata(300);  /* XXX - need this? */

    /* set Bus Control register to send GIO contents to fifos 1 & 3 */
    writeParallelData(SV_BUS_CTL, SV_BUS_WRITE_FIFOS);

    /* set CLOCK MODE reg to update contents of fifos 1 & 3 from host */
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_GO);

    /* set DMACTL reg to still-frame fifo1 out and reset ASIC fifo */
    writeParallelData(SV_DMACTL, SV_SF_FIFO1_SETUP1);
    writeParallelData(SV_DMACTL, SV_SF_FIFO1_SETUP2);

    msg_printf(DBG, "  fifo1 write started ...\n");
    DmaData = (volatile long *)&theSVboard[SV_DMADAT];

    for (i = 0; i < FIFO_SIZE; i++) {
	*DmaData = array[i];
	delayWritedata(2);
    }
    writes = i * sizeof (long);
    msg_printf(DBG, "  fifo1 write finished\n");

    /*
     * Set CLOCK MODE reg to reset fifo write pointer.  This is
     * necessary to flush an internal write buffer, completing
     * the fifo write operation.
     */
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_FIFOS_WRITE_GO);

    return (writes);
}



/*
 * error reporting routine
 */
static void
report_fifo_error(int offset, unsigned long expected, unsigned long actual)
{
    msg_printf(ERR, "Error reading FIFO #1 at offset: %06d\n", offset);
    msg_printf(VRB, "  expected: 0x%06x, actual: 0x%06x\n", expected, actual);
}


/*
 * data integrity test of fifo 1
 */
int
vfifo1data(long testvalue)
{
    int i, errors = 0;
    long testdata[FIFO_SIZE];

    testvalue &= 0xffffff;  /* mask off meaningless bits */
    for (i = 0; i < FIFO_SIZE; i++)
	testdata[i] = testvalue;

    writefifos(testdata);

    /* clear testdata buffer so we know for sure when it is overwritten */
    for (i = 0; i < FIFO_SIZE; i++)
	testdata[i] = 0xDEADBEEF;

    readfifo1(testdata);

    /* step thru the testdata buffer and check all of the values */
    for (i = 0; i < FIFO_SIZE; i++) {
	if (svc1_revid == SV_REV_P1) {
	    /* swizzling done for P1 boards */
	    testdata[i] = ((testdata[i] & 0xFF) << 16) |
			  (testdata[i] & 0xff00) |
			  ((testdata[i] & 0xff0000) >> 16);
	}
	if (i > 255 && (testdata[i]) != testvalue ) {
	    report_fifo_error(i, testvalue, testdata[i]);
	    errors++;
	}
    }

    return errors;
}


/*
 * address uniqueness test of fifo 1
 */
int
vfifo1addr(void)
{
    int i, errors = 0;
    long testdata[FIFO_SIZE];

    for (i = 0; i < FIFO_SIZE; i++)
	testdata[i] = i;

    writefifos(testdata);

    /* clear testdata buffer so we know for sure when it is overwritten */
    for (i = 0; i < FIFO_SIZE; i++)
	testdata[i] = 0xDEADBEEF;

    readfifo1(testdata);

    /* step thru the testdata buffer and check all of the values */
    for (i = 0; i < FIFO_SIZE; i++) {
	if (svc1_revid == SV_REV_P1) {
	    /* swizzling done for P1 boards */
	    testdata[i] = ((testdata[i] & 0xFF) << 16) |
			  (testdata[i] & 0xff00) |
			  ((testdata[i] & 0xff0000) >> 16);
	}
	if (i > 255 && (testdata[i]) != i ) {
	    report_fifo_error(i, i, testdata[i]);
	    errors++;
	}
    }

    return errors;
}


/*
 * Read / Write test of fifo 1
 */
int
vfifo1test()
{
    int i, errors = 0;
    unsigned long writedata;

    msg_printf(VRB, "\nTesting Video Input Frame FIFO (FIFO #1) ...\n");

    writedata = 0x555aaa;
    msg_printf(DBG, "  Pass #1 - Constant test value: 0x%06x\n", writedata);
    errors += vfifo1data(writedata);

    writedata = 0xaaa555;
    msg_printf(DBG, "  Pass #2 - Constant test value: 0x%06x\n", writedata);
    errors += vfifo1data(writedata);

    msg_printf(DBG, "  Pass #3 - Variable test value\n");
    errors = vfifo1addr();

    if (errors) {
	sum_error("Video Input Frame FIFO (FIFO #1)");
	msg_printf(DBG, "  Total errors detected: %d\n", errors);
	return -1;
    } else {
	okydoky();
	return 0;
    }
}


/*
 * read fifo 1 - this was a handy routine when the video board was
 *               being debug.
 */
int
vfifo1read()
{
    int i;
    long testdata[FIFO_SIZE];

    for (i = 0; i < FIFO_SIZE; i++)
	testdata[i] = 0xDEADBEEF;

    readfifo1(testdata);

    for (i = 0; i < FIFO_SIZE; i++) {
	if (svc1_revid == SV_REV_P1) {
	    /* swizzling done for P1 boards */
	    testdata[i] = ((testdata[i] & 0xff) << 16) |
			  (testdata[i] & 0xff00) |
			  ((testdata[i] & 0xff0000) >> 16);
	}
	printf("FIFO1[%06d] = 0x%06x\n", i, testdata[i]);
    }
}

