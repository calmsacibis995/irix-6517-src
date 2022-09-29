#ident	"lib/libsc/lib/pause.c:  $Revision: 1.9 $"

/*
 *  pause.c - wait for input characters or interrupt
 */

#include <pause.h>
#include <setjmp.h>
#include <libsc.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <arcs/time.h>
#include <arcs/signal.h>

static jmp_buf jb;

static void pause_handler (void);

/*
 * pause - pause for the given number of seconds, waiting for either
 * 	an input character, or an interrupt.  expedite is a string of
 *	the input characters that will terminate waiting.  interrupt
 *	is a string of the input characters that will terminate waiting
 *	as if an interrupt were received.  If secs is zero, then 
 *	the pause will not timeout.  
 *
 *	pause returns 
 *		0 for interrupt characters,
 *		1 for expedite characters
 *		2 for timeout
 */
int
pause (int secs, char *expedite, char *interrupt)
{
    ULONG inittime = GetRelativeTime();
    SIGNALHANDLER old_handler;
    int forever = (secs == 0);
    long rv = P_TIMEOUT;
    ULONG nread;
    char inchar;

    old_handler = Signal (SIGINT, (SIGNALHANDLER)pause_handler);

    if (setjmp (jb)) {
	rv = P_INTERRUPT;
	goto timeout;
    }

    while (forever || (inittime + (ULONG)secs > GetRelativeTime())) {
	if (ESUCCESS == GetReadStatus(StandardIn)) {
	    if (ESUCCESS == Read (StandardIn, &inchar, 1, &nread)) {
		char *cp = expedite;
		while (cp && *cp)
		    if (inchar == *cp++) {
			rv = P_EXPEDITE;
			goto timeout;
		    }
		cp = interrupt;
		while (cp && *cp)
		    if (inchar == *cp++) {
			rv = P_INTERRUPT;
			goto timeout;
		    }
	    }
	}
    }

timeout:

    Signal (SIGINT, old_handler);
    return (int)rv;
}

static void 
pause_handler (void)
{
    longjmp (jb, 1);
}
