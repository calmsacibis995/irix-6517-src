#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include "uif.h"

/* size of fifo */
#define FIFO_ENTRY_COUNT	128
#define FIFO_ENTRY_SIZE		sizeof(long)
#define FIFO_SIZE		(FIFO_ENTRY_COUNT * FIFO_ENTRY_SIZE)

/* high water mark */
#define FIFO_HW_MARK		50

/* low water mark */
#define FIFO_LW_MARK		3

int
tcc_fifo_test() {
    int errcount = 1;		/* default to error */
    volatile unsigned long *fifo_ctrl = (unsigned long*)PHYS_TO_K1(TCC_FIFO);
    volatile unsigned long *intr_reg = (unsigned long*)PHYS_TO_K1(TCC_INTR);
    volatile unsigned long *fifo = NULL;
    volatile unsigned long *fifo_access;
    unsigned long fifo_ctrl_orig, fifo_ctrl_temp;
    int entry;
    long bitwalk, prevbitwalk;

    msg_printf(VRB, "TCC FIFO test\n");

    /* save fifo control register */
    fifo_ctrl_orig = *fifo_ctrl;

    /* enable fifo input */
    *fifo_ctrl = fifo_ctrl_orig | FIFO_INPUT_EN | FIFO_OUTPUT_EN |
      (FIFO_HW_MARK << FIFO_HW_SHIFT);

    do {

	/* check the current level of the fifo */
	if ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT) {
    	    msg_printf(DBG, "FIFO isn't empty\n");
	    break;
	}

	/* allocate fifo memory */
	if ((fifo = (unsigned long*)malloc(FIFO_SIZE)) == NULL) {
	    msg_printf(DBG, "FIFO memory allocation failed\n");
	    break;
	}


	/* TEST #1 - write out 64 entries consecutively into the FIFO */
	/*           make sure they are removed and verify that the */
	/*           entries are correct */
	msg_printf(DBG, " test 1 - write FIFO at consecutive addresses\n");

	/* add entries to the fifo */
	fifo_access = (unsigned long *)K1_TO_KG(fifo);
	bitwalk = 1L;
	for (entry = 0; entry < 64; entry++) {
	    *fifo_access = bitwalk;
	    bitwalk <<= 1;
	    fifo_access++;
	}

	/* delay */
	us_delay(5000);

	/* check to see that the fifo has emptied */
	if ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT) {
    	    msg_printf(DBG, "FIFO still contains %d entries, should be empty\n",
	      ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT));
	    break;
	}

	/* verify the contents of the fifo */
	fifo_access = (unsigned long *)K1_TO_KG(fifo);
	bitwalk = 1L;
	for (entry = 0; entry < 64; entry++) {
	    if (*fifo_access != bitwalk) {
		msg_printf(DBG, "FIFO entries invalid\n");
		break;
	    }
	    bitwalk <<= 1;
	    fifo_access++;
	    if (entry < 16)
		break;
	}



	/* TEST #2 - write 64 entries to the first address and verify */
	/*           that the last entry is valid and all entries have */
	/*           been removed from the fifo */
	msg_printf(DBG, " test 2 - write FIFO at singular address\n");

	/* add entries to the fifo */
	fifo_access = (unsigned long *)K1_TO_KG(fifo);
	bitwalk = 1L;
	for (entry = 0; entry < 64; entry++) {
	    *fifo_access = bitwalk;
	    prevbitwalk = bitwalk;
	    bitwalk <<= 1;
	}

	/* delay */
	us_delay(5000);

	/* check to see that the fifo has emptied */
	if ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT) {
    	    msg_printf(DBG, "FIFO still contains %d entries, should be empty\n",
	      ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT));
	    break;
	}

	/* check the last entry */
	if (*fifo_access != prevbitwalk) {
  	    msg_printf(DBG, "FIFO entries invalid\n");
	    break;
	}


	/* TEST 3 - test low water trigger */
	msg_printf(DBG, " test 3 - low water trigger\n");

	/* turn off FIFO output so data can be placed into the FIFO plus */
	/* set the low water marker */
	fifo_ctrl_temp = *fifo_ctrl | (FIFO_LW_MARK << FIFO_LW_SHIFT);
	fifo_ctrl_temp &= (~ FIFO_OUTPUT_EN);
	*fifo_ctrl = fifo_ctrl_temp;

	/* add entries to the fifo */
	fifo_access = (unsigned long *)K1_TO_KG(fifo);
	bitwalk = 1L;
	for (entry = 0; entry < 16; entry++) {
	    *fifo_access = bitwalk;
	    bitwalk <<= 1;
	    fifo_access++;
	}

	/* clear out any possible FIFO LW interrupts */
	*intr_reg |= INTR_FIFO_LW;

	/* check to see that the fifo has 16 entries */
	if (((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT) != 16) {
    	    msg_printf(DBG, "FIFO entries missing\n");
	    break;
	}

	/* enable FIFO output */
	*fifo_ctrl |= FIFO_OUTPUT_EN;

	/* delay */
	us_delay(5000);

	/* check if FIFO emptied */
	if ((*fifo_ctrl & FIFO_LEVEL)>>FIFO_LEVEL_SHIFT) {
    	    msg_printf(DBG, "FIFO didn't empty\n");
	    break;
	}

	/* check to see if FIFO low water intr triggered */
	if (!(*intr_reg | INTR_FIFO_LW)) {
    	    msg_printf(DBG, "FIFO low water intr didn't trigger\n");
	    break;
	}


	/* TEST COMPLETE - if this point is reached, all the tests passed */
	msg_printf(DBG, " test complete\n");
	errcount = 0;

    } while(0);



    /* Wait for fifo to drain.
     */
    while ((*fifo_ctrl & FIFO_LEVEL))
	us_delay(5);

    /* restore fifo control register */
    *fifo_ctrl = fifo_ctrl_orig;

    /* clear memory */
    if (fifo)
	free((void*)fifo);
	
    /* write out passed/failed msg */
    if (errcount)
	msg_printf(VRB, "TCC FIFO test failed\n");
    else
	msg_printf(VRB, "TCC FIFO test passed\n");

    return errcount;
}

