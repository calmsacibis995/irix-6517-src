/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 * io4_select -  io4 slot/adapter select command line parser		*
 *									*
 * Parses the command line for slot and adapter numbers, if given.	*
 * Returns 0 if good slot/adapter numbers given, !0 if the format was	*
 * bad, the slot or adapter number given was out of range, or the slot	*
 * given did not have an IO4 installed. If the slot/adapter numbers are *
 * all right, returns the slot number in io4_tslot and the adapter in	*
 * io4_tadap.								*
 *									*
 ************************************************************************/

#include	<sys/types.h>
#include	<sys/sbd.h>
#include	<sys/EVEREST/everest.h>
#include	<sys/EVEREST/io4.h>
#include	<sys/EVEREST/evconfig.h>
#include	<sys/EVEREST/addrs.h>
#include	<uif.h>
#include	<ide_msg.h>

#define	IO4_BADFORMAT	"ERROR: %s - bad command line format\n",argv[0]
/* should be EV_MAX_SLOTS - 1, not 15, once token pasting works */
#define	IO4_BADSLOT	"ERROR: %s - bad slot # %x, range is 1 - 15\n",argv[0],io4_tslot 
/* should be EV_MAX_PADAPS - 1, not 7, once token pasting works */
#define	IO4_BADADAP	"ERROR: %s - bad adapter # %x, range is 1 - 7\n",argv[0],io4_tadap
#define	IO4_NOT_INSTALL	"ERROR: %s - no io4 installed in slot %x\n",argv[0],io4_tslot

int io4_tslot, io4_tadap;

int
io4_select( int do_adap, int argc, char** argv)
{
    int retval;

    retval = 0;
    io4_tslot = 0;
    io4_tadap = 0;

    /*
     * if command line arguments given
     */
    if (argc > 1) {

	/*
	 * always parse for slot, parse for adapter if do_adap is true
	 * print error message if the format is garbled
	 */
	if ((atob(argv[1], &io4_tslot) == 0) || (do_adap && (argc < 3)) ||
	    (atob(argv[2], &io4_tadap) == 0))
	    /*((argc > 2) && do_adap && (atob(argv[2], &io4_tadap) == 0)))*/
	{ 
	    msg_printf(ERR, IO4_BADFORMAT);
	    retval++;
	    goto EXIT;
	}

	/*
	 * is the slot number in range?
	 */
	if ((io4_tslot <= 0) || (io4_tslot > (EV_MAX_SLOTS - 1))) {
	    msg_printf(ERR, IO4_BADSLOT);
	    retval++;
	    goto EXIT;
	}

	/*
	 * if selecting for adapter, was the adapter in range?
	 */
	if (do_adap && ((io4_tadap <= 0) || (io4_tadap > (IO4_MAX_PADAPS-1)))){
	    msg_printf(ERR, IO4_BADADAP);
	    retval++;
	    goto EXIT;
	}

	/*
	 * was there an io4 board in the  selected slot?
	 */
	if (EVCFGINFO->ecfg_board[(io4_tslot)].eb_type != EVTYPE_IO4) {
	    msg_printf(ERR, IO4_NOT_INSTALL);
	    retval++;
	}
    }

EXIT:
    return (retval);
}


/*
 * Return 
 *	0 if no adapter of the required type is found in any IO4s
 *	1 if an adapter is found and the slot and anum have proper values
 *	This Routine should be called until it returns 0 
 */
static int start_slot=EV_MAX_SLOTS;
static int start_adap=1;
int
io4_search(int atype, int *io4slot, int *anum)
{
    int		slot, adap;

    if (atype){
        for(slot=start_slot; slot > 0; slot--) {
	    if (board_type(slot) == EVTYPE_IO4) {
	        for (adap=start_adap; adap < IO4_MAX_PADAPS; adap++) {
		    if (adap_type(slot, adap) == atype){
		        start_slot = slot;
		        start_adap = adap+1;
		        *io4slot = slot;
		        *anum    = adap;
			msg_printf(DBG,
			"io4_search: found atype 0x%x at slot 0x%x, adap %d\n",
			atype, slot, adap);
		        return 1;
		    }		/* if adap type */
		}		/* for adap */

		/*
		 * if we tested all the adapters on this slot, set up for
		 * the next IO4 board
		 */
		start_adap = 1;
	    }			/* if board type */
	}			/* for slot */
    } 				/* if atype */

    start_slot = EV_MAX_SLOTS;
    start_adap = 1;
    return(0);
}

/*
 * adapter test dispatcher for adapters on IO4 boards
 *
 * the test function should pass the command line stuff through, specify the
 * adapter type (IO4_ADAP_XXXXXX) in  "atype" and give the address for the
 * single-adapter test function in "test_func"
 *
 * saves duplicating the logic again in each test.  Thanks to Tom Shou for
 * the original idea in the scsi stuff - we had been inlining all this sort
 * of code
 */
int
test_adapter(int argc, char **argv, int atype, int (*test_func) (int, int))
{
    
    int slot, adap, retval, t_ret;


    retval = TEST_SKIPPED;
    t_ret = 0;
    slot = 0;
    adap = 0;

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);

    /*
     * reset the io4 search parameters
     */
    io4_search(0, &slot, &adap);

    /*
     * loop while valid "atype" adapters may be found
     */
    while (io4_search(atype, &slot, &adap)) {

	/*
	 * if any io4 OK or we are on the selected one
	 */
	if (!io4_tslot || (io4_tslot == slot)) {

	    /*
	     * if any adapter OK or if we are on the selected one
	     */
	    if (!io4_tadap || (io4_tadap == adap)) {

		/*
		 * run the test function
		 */
		t_ret = (*test_func)(slot, adap);

		/*
		 * bump the error count by the appropriate measure
		 */
		if (retval == TEST_SKIPPED)
		    retval = t_ret;
		else if (t_ret != TEST_SKIPPED)
		    retval += t_ret;

	    }	/* if adapter */
	}	/* if io4 */
    }		/* while valid "atype" chips to be found */

    /*
     * reset the io4 search parameters again (cleanup just in case)
     */
    io4_search(0, &slot, &adap);

    return (retval);
}
