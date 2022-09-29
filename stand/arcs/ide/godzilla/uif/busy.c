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

#ident "$Revision: 1.12 $"

#include <ide_msg.h>
#include <sys/cpu.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

extern int failflag;
extern short msg_where;		/* indicates destination of output */

#define SETLED(X)	ip30_set_cpuled(X)
#define SPINLED(X)	((X) & 0x01 ? 1 : 0)	/* green/off */
#define FAILED(X)	((X) ? 2 : 1)		/* amber/green */

void
busy(int on)
{
	static unsigned int spincount = 0;
	static char *spinchar[] = {"\r-", "\r\\", "\r|", "\r/"};

	_scandevs();
	spincount++;

	if (on)
		SETLED(SPINLED(spincount));
	else {
		SETLED(FAILED(failflag));

		if (!(msg_where & PRW_BUF))
			msg_printf(NO_REPORT, "\r");
	}

	if (!(msg_where & PRW_BUF) && on)
		msg_printf(NO_REPORT, spinchar[spincount & 0x3]);
}

int
wait(int argc, char *argv[])
{
	char line[80];

	if (!(msg_where & PRW_BUF)) {   /* if "buffering" isn't on ... */
		if (argc > 1)
			printf("%s -", argv[1]);
		printf(" press <Enter> to continue\n");
		gets(line);
	}
	return(0);
}
