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
 *	enetlib.c - ethernet library routines. Could be put into a lib  *
 *			directory someday.				*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>

jmp_buf  enetlib_buf;


int
test_enet(int arggc, char** argvv, int (*test_it)())
{
    	int slot, adap, atype, val;
	int fail = 0;
	uint adap_arr;

    	slot = 0;
    	adap = 0;

    	/*
    	 * if bad command line, exit
    	 */
    	if (io4_select (TRUE, arggc, argvv))
		return(1);

	adap_arr = adap_slots(IO4_ADAP_EPC);
	/* setup error handling */
	for (slot = EV_MAX_SLOTS; slot > 0; slot--)
           if (board_type(slot) == EVTYPE_IO4)
                for (adap = 1; adap < IO4_MAX_PADAPS; adap++)
                        if ((atype = adap_type(slot,adap)) == IO4_ADAP_EPC) {
                                setup_err_intr(slot, adap);
                                io_err_clear(slot, adap_arr);
                                msg_printf(DBG,"loc 1\n");
                        }
	
	if (val = setjmp(enetlib_buf)) {
                if (val == 1){
                        msg_printf(SUM,
                          "Unexpected exception during ethernet test\n");
                        show_fault();
                }
                io_err_log(slot, adap_arr) ;
                io_err_show(slot, adap_arr);
                fail++;
                goto SLOTFAILED;
        }
        else {

        set_nofault(enetlib_buf);

        adap_arr = adap_slots(IO4_ADAP_EPC);
	
    	/*
    	 * iterate through all io boards in the system
    	 */
    	for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

	   /*
	    * cheat - if slot was given on command line, use it
	    */
	   if (io4_tslot)
	    	slot = io4_tslot;

	   if (board_type(slot) == EVTYPE_IO4) {
	      /*
	       * iterate through all io adapters in the system
	       */
	      for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {

		/*
		 * cheat - if adapter was given on command line, use it
		 */
		if (io4_tadap)
		    adap = io4_tadap;

		/*
		 * get the current adapter type
		 */
		atype = adap_type(slot, adap);

		if (atype == IO4_ADAP_EPC)
		    fail += (*test_it)(slot, adap);
		else if (io4_tadap) {
		    msg_printf(ERR, "ERROR: %s - slot %x, adap %x was %x, expected %x\n",
			 argvv[0], slot, adap, atype, IO4_ADAP_EPC);
		    fail++;
		}
	    
		/*
		 * if adap was given on command line, we are done
		 */
		if (io4_tadap)
		    break;
	    } /* for adap */
	
	    /*
	     * if slot was given on command line, we are done
	     */
	    if (io4_tslot)
		break;
	} /* if board_type */
    } /* for slot */
    } /* if setjmp... else */

            /* Post processing */
        for (slot = EV_MAX_SLOTS; slot > 0; slot--)
           if (board_type(slot) == EVTYPE_IO4)
                if (check_ioereg(slot, adap_arr)){
                     report_check_error(fail);
                     /* msg_printf(SUM,"WARNING: ");
                     if (!fail)
                        msg_printf(SUM,"Test passed, but got an ");
                     msg_printf(SUM,"Unexpected ERROR in a register\n");
                     msg_printf(SUM,"Run test again at report level 3 ");
                     msg_printf(SUM,"to see the error. It's printed before ");
                     msg_printf(SUM,"this message.\n");
                        /* longjmp(scsilib_buf, 2); */
                }


SLOTFAILED:
        for (slot = EV_MAX_SLOTS; slot > 0; slot--)
                if (board_type(slot) == EVTYPE_IO4) {
                        io_err_clear(slot, adap_arr);
		}

    	cpu_err_clear();
        clear_nofault();
        clear_IP();


    return (fail);
}
