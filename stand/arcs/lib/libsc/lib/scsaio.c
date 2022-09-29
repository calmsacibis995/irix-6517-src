/* High level saio code, that belongs in libsc.
 */

#ident "$Revision: 1.45 $"


#include <saioctl.h>
#include <parser.h>
#include <arcs/hinv.h>
#include <arcs/spb.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <arcs/time.h>
#include <setjmp.h>
#include <stdarg.h>
#include <libsc.h>
#include <libsc_internal.h>

extern void _errputs(char*);

int Debug, Verbose, _udpcksum;
jmp_buf restart_buf;

/* __close_fds(): close all open fds.  Called by the EnterInteractiveMode()
 * backend before calling _exit().
 */
void
__close_fds(void)
{
	register ULONG fd;

	for (fd = 0; fd < ARCS_FOPEN_MAX; fd++)
		Close(fd);
}

static int notfirst;	/* not initialized so that init resets */

void
getack_andexit(void)
{
	int c;

	if (SPB->DebugBlock)
	    debug("quiet");
	/* this is the same message used for kernel panics; consistency is
		sometimes a good thing... */
	printf("\n[Press reset or ENTER to restart.]");
	while((c=getchar()) != '\n' && c != '\r')
		; /* spin, so user can see msg; especially important on
		   * graphics tube, where an exit clears
		   * back to the initial screen. */
	notfirst = 0;
	EnterInteractiveMode();
}

/*VARARGS1*/
void
panic(char *msg, ...)
{
	va_list ap;

	/*CONSTCOND*/
	va_start(ap, msg);

	if(notfirst++) {
		if(notfirst == 2)	/* sometimes printf panics also... */
			_errputs("\r\nDOUBLE PANIC\n");
		for(;;);
		/*NOTREACHED*/
	}
	_errputs("\r\nPANIC: ");
	vprintf(msg, ap);
	putchar('\n');
	getack_andexit();
}

/*
 * isgraphic -- determine if fd refers to a graphics device
 */
int
isgraphic(ULONG fd)
{
	int arg;

	ioctl(fd,TIOCISGRAPHIC,(long)&arg);
	return(arg);
}

/* used in places like sash main command loop to close all open
 * file descriptors other than the console ones.  Otherwise one
 * can easily run out of descriptors after a few interrupts.
 */
void
close_noncons(void)
{
	register ULONG fd;

	for(fd=0; fd < ARCS_FOPEN_MAX; fd++)
		if (!isatty(fd))
			Close(fd);
}

int month_days[12] = {
	31,	/* January */
	28,	/* February */
	31,	/* March */
	30,	/* April */
	31,	/* May */
	30,	/* June */
	31,	/* July */
	31,	/* August */
	30,	/* September */
	31,	/* October */
	30,	/* November */
	31	/* December */
};

unsigned long
get_tod(void)
{
    unsigned long secs;
    TIMEINFO *time;
    uint i;

    time = GetTime();
    secs = time->Seconds;

    /*
     * Sum up seconds from beginning of year
     */
    secs += time->Minutes * SECMIN;
    secs += time->Hour * SECHOUR;
    if ( time->Day > 0 ) {
	    secs += (time->Day-1) * SECDAY;
    }
    if ( time->Month > 0 ) {
	    for (i = 0; i < time->Month-1; i++) {
		    secs += (ulong)month_days[i] * SECDAY;
	    }
    }
    secs += (ulong)time->Year * SECYR;
    return(secs);
}


/*
 * Return the cpuid of this processor
 */
int
getcpuid(void)
{
	static int gotcpuid;
	static int cpuid;
	COMPONENT *c;

	if (!gotcpuid) {
		if ((c = GetChild(NULL)) == NULL) {
			if ((cpuid = sn0_getcpuid()) != -1) {
				gotcpuid = 1;
				return cpuid;
			}

			return -1;
		}
		if (c->Class != SystemClass)
			return -1;
		for (c = GetChild(c); c; c = GetPeer(c))
			if ((c->Class == ProcessorClass) && (c->Type == CPU)) {
			    cpuid = (int)c->Key;
			    gotcpuid = 1;
			    break;
			}
		if (!gotcpuid)
			return -1;
	}

	return cpuid;
}

/*  Print the current offset.  This way error printers don't have to deal
 * with the ugly seek interface.
 */
void
prcuroff(ULONG fd)
{
	LARGE off;

	off.hi = 0;
	off.lo = 0;
	Seek(fd,&off,SeekRelative);

	printf("offset=0x%x 0x%x\n",off.hi,off.lo);
}
