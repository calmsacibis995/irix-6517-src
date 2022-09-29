
/***********************************************************************\
*	File:		io4_pioerr.c   					*
*									*
*	Invalid PIO write test to check the IBus error register and	*
*	IO4 to CPU interrupt propogation				*
*									*
\***********************************************************************/

#ident "arcs/ide/EVEREST/io4/io4_pioerr.c $Revision: 1.6 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/addrs.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>

#ifdef TFP
#include <prototypes.h>
#endif

#include <io4_tdefs.h>

#define IAID_TESTP	"io4_pioerr Passed\n"
#define IAID_TESTF	"FAILED: io4_errpio\n"

static int io4_check_errpio(int);

/*
 * io4_pioerr produces an io4 pio error by attepting to access an io adapter
 * (0) that is never installed. Checks the io4-to-cpu error path.
 */
int
io4_pioerr (int argc, char** argv)
{
    int retval, slot;
    jmp_buf	iaid_buf;

    retval = 0;
    if (io4_select(FALSE, argc, argv))
	return(1);

    msg_printf(INFO, "\nio4_pioerr starting. . .\n");

    /*
     * clear any old fault handlers, timers, etc
     */
    clear_nofault();

    if (setjmp(iaid_buf))
    {
	msg_printf(ERR, "ERROR: io4_pioerr - unexpected interrupt\n");
    }
    else
    {
	nofault = iaid_buf;

	/*
	 * if slot specifed on command line, test just that slot
	 */
	if (io4_tslot)
	    return (io4_check_errpio(io4_tslot));

	/*
	 * iterate through all io adapters in the system
	 */
	for (slot = EV_MAX_SLOTS; slot > 0; slot--) {
	    if (board_type(slot) == EVTYPE_IO4)
		retval |= io4_check_errpio(slot);
	}
    }

    /*
     * clear fault handlers again
     */
    clear_nofault();

    nofault = 0;

    msg_printf(INFO, "%s", (retval == 0) ? IAID_TESTP : IAID_TESTF);

    return(retval);
	
}

/*
 * Function : io4_check_errpio
 * Description :
 *	Do some invalid PIO operation, and check the error bits which
 *	get set. Do the invalid PIO again, and see if the sticky bit 
 *	gets set.
 */
static int
io4_check_errpio(int slot)
{
    int         ilvl, retval; 
    unsigned    int_level;
    unsigned	window;
#ifndef TFP
    caddr_t	swin;
    unsigned	ebus_error; 
#else
    __psunsigned_t swin;
    __uint64_t ebus_error;
#endif

#define	exp_error 	(IO4_EBUSERROR_BADIOA | IO4_EBUSERROR_STICKY)

    retval = 0;
    window = io4_window(slot);

#ifndef  TFP
    swin = (caddr_t)SWIN_BASE(window, 0);
#else
    swin = (__psunsigned_t)SWIN_BASE(window, 0);
#endif

    save_io4config(slot);

    /*
     * kluge - just reseting the existing window location
     */
    setup_err_intr(slot, 0);

    EV_SET_LOCAL(swin, 0);	/* NON_EXISTANT_IOA	*/

    /* This should set the sticky bit 	*/
    EV_SET_LOCAL(swin, 0);	

    ilvl = EVINTR_LEVEL_IO4_ERROR;
    int_level = (unsigned int)EV_GET_LOCAL(EV_HPIL);

    ebus_error = EV_GET_CONFIG(slot, IO4_CONF_EBUSERROR);

    if (ebus_error != exp_error){
	msg_printf(ERR,
		"ERROR: io4_pioerr - ebus err # was %llx, expected %llx\n",
		(long long) ebus_error, exp_error);
	retval++;
    }
    if (cpu_intr_pending() == 0){
	msg_printf(ERR, "ERROR: io4_pioerr - no ebus error interrupt\n");
	retval++;
    }
    if (int_level  != ilvl) {
	msg_printf(ERR,
		"ERROR: io4_pioerr - ebus interrupt was %x, expected %x\n",
		int_level, ilvl);
	retval++;
    }

    /*
     * dump the error stuff if faulure of any sort
     */
    if (retval)
    {
	io_err_log(slot, 0);
	io_err_show(slot, 0);
    }

    io_err_clear(slot, 0);

    restore_io4config(slot);

    return(retval);

#undef 	exp_error
}

