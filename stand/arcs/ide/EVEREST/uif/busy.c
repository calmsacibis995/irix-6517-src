#ident	"ide/EVEREST/uif/busy.c:  $Revision: 1.2 $"

#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "ide_msg.h"

extern int failflag;
extern short msg_where;		/* indicates destination of output */
extern void _set_cc_leds(int);

#define CCLED_MASK	0x3f	/* cycle through all 6 leds */

/*
 *  busy() - This function is called to "blink" characters on the screen
 *  during long tests.  Each time it is called with a 1 passed to it, one
 *  "busy" character is overwritten by the other.  The function _scandevs()
 *  is also called so that the test may be interrupted.  When this function
 *  is called with 0, the last character is erased.
 *
 *  By popular demand, this function has been modified to also blink the
 *  failure LED.  This provides an additional sign of life that is
 *  particularly useful during long graphics tests run from the graphics
 *  console.
 */
void
busy(int on)
{
	static unsigned int spincount = 0;
	static char *spinchar[] = {"\r-", "\r\\", "\r|", "\r/"};

	_scandevs();
	spincount++;

	/*
	 * XXXXX: eventually this must be modified for Everest to only
	 * flash the LEDs when operating in noncoherent mode!
	 */
	if (on) {
		_set_cc_leds(spincount & CCLED_MASK);
	} else {
		_set_cc_leds(failflag);
		if (!(msg_where & PRW_BUF))
			msg_printf(NO_REPORT, "\r");
	}

	if (!(msg_where & PRW_BUF) && on)
		msg_printf(NO_REPORT, spinchar[spincount & 0x3]);
}

/*
 *  pbusy() - Same as busy() but without mucking with the LED
 *
 *  Called by tests that check cache parity because changing
 *  the console LED can clear the DBG2 latched parity error
 *  addresses.
 */
void pbusy(on)
int on;
{
    extern short msg_where;
    static int spinchar = 1;
    int doprint = !(msg_where & PRW_BUF);

    if (doprint) {
	if (on) {
	    spinchar ^= 1;
	    if (spinchar)
		msg_printf(NO_REPORT,"\r*");
	    else
		msg_printf(NO_REPORT,"\r+");
	} else
	    msg_printf(NO_REPORT,"\r");
    }
    _scandevs();
}

void
wait(int argc, char *argv[])
{
	char line[80];

	if (!(msg_where & PRW_BUF)) {   /* if "buffering" isn't on ... */
		if (argc > 1)
			printf("%s -", argv[1]);
		printf(" press < Enter > to continue...\n");
		gets(line);
	}
}
