#ident	"ide/EVEREST/uif/status.c:  $Revision: 1.2 $"

/*
 * IDE universal status messages and notifier functions.
 */

#include "libsc.h"
#include "saioctl.h"
#include "uif.h"

#define MAX_CCUART_LEDVAL	63

void okydoky(void);
void _set_cc_leds(int);

/*
 * Command to turn LED on and off
 */
void
sickled(argc,argv)
int argc;
char *argv[];
{
	int on = -1;
	char *usage = "Usage: led n (n is 0..63)\n";

	if ( argc != 2 ) {
		msg_printf(SUM, usage);
		return;
	}

	atob(argv[1],&on);
	if (on < 0 || on > MAX_CCUART_LEDVAL) {
		msg_printf(SUM, usage);
		return;
	}

	_set_cc_leds(on);
}

/*
 * Print out error summary of diagnostics test
 */
void
sum_error (char *test_name)
{
#ifdef PROM
	msg_printf (ERR, "\r\t\t\t\t\t\t\t\t- ERROR -\n");
#endif
	msg_printf (ERR, "\rHardware failure detected in %s test\n", test_name);
}   /* sum_error */

/*
 *  Print out commonly used successful-completion message
 */
void okydoky(void)
{
    msg_printf(VRB, "Test completed with no errors.\n");
}

#ifdef notdef
gfx_exit()
{
    if (console_is_gfx())
    {
	/*
	 *  Now we need to reintialize the graphics console, but first
	 *  we need do some voodoo to convince the ioctl that graphics
	 *  really need initializing.
	 */
	ioctl(1, TIOCREOPEN, 0);
    }
    buffoff();
}
#endif
