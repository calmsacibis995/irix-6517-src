/* @[$] nl_error.c 1.0 frank@ceres.esd.sgi.com Oct 27 1992
 * print error messages with variable number of arguments
 *
 * this function depends only on catalog used by fmtmsg()
 */

#include	<stdarg.h>
#include	<stdio.h>
#include	<fmtmsg.h>
#include	<errno.h>
#include	<sgi_nl.h>
#include	<string.h>
#include 	<limits.h>

	/*
   	 * We set the size of the buffer we format into to be
	 * twice the size of the maximum pathname.  This 
	 * sizing is based on the assumption that our callers
	 * might pass us something a little over PATH_MAX
	 * in length (see BUG #169708). Of course, this
	 * size buffer can overflow just like the old size
	 * (1024) did. With the current interface we can't
	 * know how big to make this. _sgi_dofmt should be
	 * modified to accept the buffer size. But until then
	 * we continue the hack.
	 */
#define	LMSGBUF	(2*PATH_MAX)

extern  char *_sgi_donfmt(char *, ssize_t, int, char *, int, char *, va_list);

/*
 * print error messages with variable number of arguments
 * to stderr
 */
void
_sgi_nl_error(int flag, char *label, char *format, ...)
{
	va_list ap;
	char lmsgbuf[LMSGBUF];
	int error = errno;
	char *err_msg=NULL;

	va_start(ap, format);
	/* Now calls _sgi_donfmt the bounds check version of _sgi_dofmt() */
	(void)_sgi_donfmt(lmsgbuf, LMSGBUF, 0, label, MM_ERROR, format, ap);
	va_end(ap);

	/*
	 * if flag add system error message
	 */
	if(flag == 2) {
	    /* Make sure that adding the system error message would'nt
	       overflow the lmsgbuf buffer */
	    if ((err_msg = strerror(error)) != NULL)
	     if(strlen(lmsgbuf) + strlen(err_msg) < (LMSGBUF-3)) {
		   /* Changed the check to use -3 instead of -1 as " - " is 3 bytes!! */
   	           sprintf(lmsgbuf + strlen(lmsgbuf), " - %s", err_msg);
	    }
	}
	(void)fprintf(stderr, "%s\n", lmsgbuf);
	if(flag == 1) {
	    (void)_sgi_ffmtmsg(stderr, 0, label, MM_REASON,
		"%s", strerror(error));
	}
}
