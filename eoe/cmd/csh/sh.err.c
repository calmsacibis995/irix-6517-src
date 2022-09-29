/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.10 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley Software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <sgi_nl.h>
#include <pfmt.h>
#include "sh.h"
#include "sh.wconst.h"

/*
 * C Shell
 */
bool	errspl;			/* Argument to error was spliced by seterr2 */

wchar_t	*onev[2] = { S_1, 0 };

/*
 * error() with errstr as an argument.
 * Some callers set errstr to something that looks like a printf format,
 * and since there aren't any printfargs through this interface, that
 * won't work right.  So pass %s and errstr instead.
 */
void
error_errstr(void)
{
	error("%s", errstr);
}

/*
 * Print error string s with optional argument arg.
 * This routine always resets or exits.  The flag haderr
 * is set so the routine who catches the unwind can propogate
 * it if they want.
 *
 * Note that any open files at the point of error will eventually
 * be closed in the routine process in sh.c which is the only
 * place error unwinds are ever caught.
 */
void
error(char *s, ...)
{
	va_list ap;
	register wchar_t **v;
	register char *ep;
	char errbuf[CSHBUFSIZ];

	/*
	 * Must flush before we print as we wish output before the error
	 * to go on (some form of) standard output, while output after
	 * goes on (some form of) diagnostic output.
	 * If didfds then output will go to 1/2 else to FSHOUT/FSHDIAG.
	 * See flush in sh.print.c.
	 */
	flush();
	haderr = 1;			/* Now to diagnostic output */
	timflg = 0;			/* This isn't otherwise reset */
	if(v = pargv)
	    pargv = 0, blkfree(v);
	if(v = gargv)
	    gargv = 0, blkfree(v);

	/*
	 * A zero arguments causes no printing, else print
	 * an error diagnostic here.
	 */
	if(s) {
	    va_start(ap, s);
	    _sgi_dofmt(errbuf, 0, cmd_label, MM_ERROR, s, ap);
	    shprintf("%s\n", errbuf);
	    va_end(ap);
	}
	didfds = 0;			/* Forget about 0,1,2 */
	if((ep = errstr) && errspl)
	    xfree(ep);
	errspl = 0;

	/*
	 * Go away if -e or we are a child shell
	 */
	if(exiterr || child)
	    done(1);

	/*
	 * Reset the state of the input.
	 * This buffered seek to end of file will also
	 * clear the while/foreach stack.
	 */
	btoeof();

	setq(S_status, onev, &shvhed);
	if(tpgrp > 0)
	    (void) tcsetpgrp(FSHTTY, tpgrp);
	reset();
}

/*
 * Perror is the shells version of perror which should otherwise
 * never be called.
 */
void
Perror(wchar_t *s)
{
	register int oerrno = errno;
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];
	char varbuf[CSHBUFSIZ * MB_LEN_MAX];

	/*
	 * Perror uses unit 2, thus if we didn't set up the fd's
	 * we must set up unit 2 now else the diagnostic will disappear
	 */
	if(!didfds) {
	    (void)dcopy(SHDIAG, 2);
	    errno = oerrno;
	}
	_sgi_sfmtmsg(chbuf, 0, cmd_label, MM_ERROR,
	    "%s - %s",
	    tstostr(varbuf, s, 0),
	    strerror(oserror()));
	haderr = 1;	/* so it goes to diag descriptor! */
	shprintf("%s\n", chbuf);
	error(NULL);			/* To exit or unwind */
	/*NOTREACHED*/
}

void
bferr(char *cp)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	flush();
	haderr = 1;
	error("%s - %s", (__psint_t)tstostr(chbuf, bname, NOFLAG), (__psint_t)cp);
}

/*
 * The parser and scanner set up errors for later by calling seterr,
 * which sets the variable err as a side effect; later to be tested,
 * e.g. in process.
 */
void
seterr(char *s)
{
	if( !errstr)
	    errstr = s, errspl = 0;
}

/*
 * Set err to a splice of cp and dp,
 * to be freed later in error()
 */
void
seterr2(wchar_t *cp, char *dp)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	if(errstr)
	    return;
	tstostr(chbuf, cp, NOFLAG);
	errstr = xalloc(strlen(chbuf) + strlen(dp) + 1);
	strcpy(errstr, chbuf);
	strcat(errstr, dp);
	errspl++;			/* Remember to xfree(err). */
}

/*
 * Set errstr to a splice of cp
 * with a string form of character d
 * string may contain %s to place mbchar d into
 */
void
seterrc(char *cp, wchar_t d)
{
	register char *s;
	register int x;
	char chbuf[MB_LEN_MAX + 16]; 

	d &= TRIM;
	(void)wctomb(NOSTR, d);		/* reset to initial shift status */
	x = wctomb(chbuf, d);		/* convert char */
	if(x < 0)
	    strcpy(chbuf, "(ill char)");
	else
	    chbuf[x] = '\0';
	errstr = xalloc(strlen(cp) + MB_LEN_MAX + 16);
	for(s = errstr; *cp;) {
	    if((cp[0] == '%') && (cp[1] == 's')) {
		strcpy(s, chbuf);
		s += strlen(chbuf);
		cp += 2;
		continue;
	    }
	    *s++ = *cp++;
	}
	*s = '\0';
	errspl++;			/* Remember to xfree(errstr). */
}

/*
 * print only an error string
 * no call of error();
 */
void
showstr(int sev, char *s, void *arg)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];
	size_t sl;
	/* arg is always NULL for cases where there is no string arg */
	if(arg && (sl=strlen(arg)) > 512) {
		shprintf("csh: string to showstr too long (%d)\n", sl);
		strncpy(chbuf,arg,512);
		chbuf[512] = 0;
		shprintf("%s\n", chbuf);
	}
	_sgi_sfmtmsg(chbuf, 0, cmd_label, sev, s, arg);
	shprintf("%s\n", chbuf);
}

/*
 * some error prints
 */
void
err_fntruncated(char *s)
{
	showstr(MM_ERROR,
	    gettxt(_SGI_DMMX_csh_fntrunc, "Filename '%s' truncated !"),
	    (void *)s);
}

void
err_line2long(void)
{
	error(gettxt(_SGI_DMMX_csh_line2long, "Line too long"));
}

void
err_nomatch(void)
{
	error(gettxt(_SGI_DMMX_csh_nomatch, "No match"));
}

void
err_divby0(void)
{
	error(gettxt(_SGI_DMMX_csh_divby0, "Divide by 0"));
}

void
err_modby0(void)
{
	error(gettxt(_SGI_DMMX_csh_modby0, "Mod by 0"));
}

void
err_notlogin(void)
{
	error(gettxt(_SGI_DMMX_csh_notlogin, "Not login shell"));
}

void
syntaxerr(void)
{
	error(gettxt(_SGI_DMMX_csh_syntaxerr, "Syntax error"));
}

void
err_word2long(void)
{
	error(gettxt(_SGI_DMMX_csh_word2long, "Word too long"));
}

void
err_arg2long(void)
{
	error(gettxt(_SGI_DMMX_csh_arg2long, "Arguments too long"));
}

void
err_missing(wchar_t wc)
{
	char chbuf[MB_LEN_MAX + 1];

	error(gettxt(_SGI_DMMX_csh_miss, "Missing %s"),
	    tctomb(chbuf, wc & TRIM));
}

void
err_unknflag(wchar_t wc)
{
	char wbuf[MB_LEN_MAX + 1];

	showstr(MM_ERROR,
	    gettxt(_SGI_DMMX_csh_badflag, "Unknown flag: -%s"),
	    tctomb(wbuf, wc & TRIM));
}

void
err_unmatched(wchar_t wc)
{
	char chbuf[MB_LEN_MAX + 1];

	error(gettxt(_SGI_DMMX_csh_unmatched, "Unmatched %s"),
	    tctomb(chbuf, wc & TRIM));
}

void
err_nomem(void)
{
	error(gettxt(_SGI_DMMX_csh_nomem, "No memory"));
}

void
err_toomany(wchar_t wc)
{
	char chbuf[MB_LEN_MAX + 1];

	error(gettxt(_SGI_DMMX_csh_toomany, "Too many %s's"),
	    tctomb(chbuf, wc & TRIM));
}

void
err_usage(char *s)
{
	error(gettxt(_SGI_DMMX_csh_usage, "Usage: %s"), (__psint_t)s);
}

void
err_experr(void)
{
	bferr(gettxt(_SGI_DMMX_csh_expsyntax, "Expression syntax"));
}

void
err_notfromtty(void)
{
	bferr(gettxt(_SGI_DMMX_csh_notfromtty, "Can't from terminal"));
}

void
err_notinwf(void)
{
	bferr(gettxt(_SGI_DMMX_csh_notinwf, "Not in while/foreach"));
}
