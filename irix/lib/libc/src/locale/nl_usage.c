/* @[$] nl_usage.c 1.0 frank@ceres.esd.sgi.com Oct 28 1992
 * print error messages with variable number of arguments
 *
 * this function depends on catalog 'uxsgicore'
 *	_SGI_MMX_Usage and _SGI_MMX_usagespc is used
 * and on the catalog used by fmtmsg()
 */

#include	<synonyms.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<fmtmsg.h>
#include	<string.h>	/* prototype for strlen() */
#include	<nl_types.h>	/* prototype for gettxt() */
#include	<sgi_nl.h>
#include	<msgs/uxsgicore.h>

#define	LMSGBUF	1024		/* this size seems to be enough */

/*
 * print usage messages with variable number of arguments
 * calls _sgi_ffmtmsg() to write to stderr
 */
void
_sgi_nl_usage(int flag, char *label, char *format, ...)
{
	va_list ap;
	char lmsgbuf[LMSGBUF];

	sprintf(lmsgbuf, "%s ",
	    flag?
		gettxt(_SGI_MMX_Usage, "Usage:")
	    :
		gettxt(_SGI_MMX_usagespc, "      "));
	va_start(ap, format);
	vsprintf(lmsgbuf + strlen(lmsgbuf), format, ap);
	va_end(ap);
	_sgi_ffmtmsg(stderr, 0, label, MM_INFO, "%s", lmsgbuf);
}

