/* @[$] sgi_ffmtmsg.c 1.0 Nov 4 1992 frank@ceres.esd.sgi.com
 * print formatted messages to stream
 */

#include	<stdarg.h>
#include	<stdio.h>
#include	<sgi_nl.h>

#define	LMSGBUF		1024

/*
 * print messages with variable number of arguments to stream
 * return: like fprintf()
 */
int
_sgi_ffmtmsg(FILE *f, int class, char *label, int sev, char *fmt, ...)
{
	
	va_list ap;
	char lmsgbuf[LMSGBUF];

	va_start(ap, fmt);
	(void)_sgi_dofmt(lmsgbuf, class, label, sev, fmt, ap)
	va_end(ap);
	return(fprintf(f, "%s\n", lmsgbuf));
}

