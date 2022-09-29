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

#ident "$Revision: 1.2 $"

#include <ide_msg.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

extern int failflag;
extern short msg_where;		/* indicates destination of output */

#if IP22 || IP26
#define SETLED(X)	ip22_set_cpuled(X)
#define SPINLED(X)	((X) & 0x01 ? Green : Black)
#define FAILED(X)	((X) ? Red : Green)
#define Black	0
#define Green	1
#define Amber	2
#define Red	3
#endif

#if IP20
#define SETLED(X)	ip20_set_sickled(X)
#define SPINLED(X)	((X) & 0x01)
#define FAILED(X)	(X)
#endif

/* this is actually duplicated in status.c */
#if IP30 /* XXX NOTE: empty function defined in stand/arcs/lib/libsk/ml/IP30.c */
#define SETLED(X)	ip30_set_cpuled(X)
#define SPINLED(X)	((X) & 0x01)	/* XXX */
#define FAILED(X)	(X)		/* XXX */
#endif /* IP30 */

#if IP32 
#define SETLED(X)	ip32_set_cpuled(X)
#define SPINLED(X)	((X) & 0x01 ? Green : Black)
#define FAILED(X)	((X) ? Red : Green)
#define Black	0x30
#define Red	0x20
#define Amber	0x00
#define Green	0x10
void ip32_set_cpuled(uint);
#endif


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
