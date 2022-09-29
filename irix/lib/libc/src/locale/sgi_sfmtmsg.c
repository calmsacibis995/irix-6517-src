/* @[$] sgi_sfmtmsg.c 1.0 Nov 4 1992 frank@ceres.esd.sgi.com
 * print formatted messages to buffer
 */

#include	<stdarg.h>
#include	<stdio.h>
#include	<sgi_nl.h>

/*
 * print messages with variable number of arguments to buffer
 * return: char ptr to terminating \0 of buf
 *         0 if buf=0
 */
char *
_sgi_sfmtmsg(char *buf, int class, char *label, int sev, char *fmt, ...)
{
	register char *s;
	va_list	ap;

	va_start(ap, fmt);
	s = _sgi_dofmt(buf, class, label, sev, fmt, ap);
	va_end(ap);
	return(s);
}

