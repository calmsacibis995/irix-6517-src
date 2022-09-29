#include "uif.h"
#include "sv.h"
#include "ide_msg.h"


#define CLUT_SIZE_P0	0x1000  /* size of output CLUT on PO board */
#define CLUT_SIZE_P1	0x2000  /* size of output CLUT on P1 board */

static void report_clut_error(int, unsigned long, unsigned long);

/*
 *  test of the video output color look-up table (data)
 */
int voclut_test()
{
    int errorcount = 0, totalerrs = 0;
    int offset, clut_size;
    unsigned long readdata, writedata;
    volatile unsigned long *clut_ptr;

    msg_printf(VRB, "\nTesting Video Output Color Look-Up Tables ...\n");

    /* 
     * Check ASIC (SVC1) rev ID to see if we have a P0 or P1 video board
     * because the look-up table doubled in size after P0.  Right now we
     * assume that a board is a P0 if it is not a P1.  This must be
     * corrected if there are future revisions.
     */
    if (svc1_revid == SV_REV_P1)
	clut_size = CLUT_SIZE_P1;
    else
	clut_size = CLUT_SIZE_P0;

    /* Load up the color look-up table. */
    writedata = 0x555aaa;
    msg_printf(DBG, "  Pass #1 - Constant test value: 0x%06x\n", writedata);
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	*clut_ptr++ = writedata;
	delayWritedata(GIO_BUS_DELAY);	/* dummy for GIO bus */
    }

    /* Read back and test for correct values. */
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	readdata = (*clut_ptr++ & 0xffffff);
        if (readdata != writedata) {
	    report_clut_error(offset, writedata, readdata);
	    errorcount++;
	}
    }
    msg_printf(DBG, "  Errors detected: %d\n", errorcount);
    totalerrs += errorcount;

    /* Load up the color look-up table. */
    errorcount = 0;
    writedata = 0xaaa555;
    msg_printf(DBG, "  Pass #2 - Constant test value: 0x%06x\n", writedata);
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	*clut_ptr++ = writedata;
	delayWritedata(GIO_BUS_DELAY);	/* dummy for GIO bus */
    }

    /* Read back and test for correct values. */
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	readdata = (*clut_ptr++ & 0xffffff);
        if (readdata != writedata) {
	    report_clut_error(offset, writedata, readdata);
	    errorcount++;
	}
    }
    msg_printf(DBG, "  Errors detected: %d\n", errorcount);
    totalerrs += errorcount;

    /* Load up the color look-up table. */
    errorcount = 0;
    msg_printf(DBG, "  Pass #3 - Variable test value\n", writedata);
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	*clut_ptr++ = offset;
	delayWritedata(GIO_BUS_DELAY);	/* dummy for GIO bus */
    }

    /* Read back and test for correct values. */
    clut_ptr = (volatile unsigned long *)&theSVboard[SV_CLUT_BASE];
    for (offset = 0; offset < clut_size; offset += 4) {
	readdata = (*clut_ptr++ & 0xffffff);
        if (readdata != offset) {
	    report_clut_error(offset, offset, readdata);
	    errorcount++;
	}
    }
    msg_printf(DBG, "  Errors detected: %d\n", errorcount);
    totalerrs += errorcount;

    if (totalerrs) {
	sum_error("video output CLUT");
	return -1;
    } else {
	okydoky();
	return 0;
    }
}


static void
report_clut_error(int offset, unsigned long expected, unsigned long actual)
{
    msg_printf(ERR, "Error reading Color Look-Up Table at offset: %04d\n",
								    offset);
    msg_printf(VRB, "  expected: 0x%08x, actual: 0x%08x\n", expected, actual);
}
