/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.5 $"

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

#include <sys/types.h>
#include <unistd.h>
#include "sh.h"
#include "sh.wconst.h"
/*
 * C Shell
 */

static void	p2dig(long l);

void
psecs(int64_t l)
{
	register int64_t i;

	i = l / 3600;
	if(i) {
	    shprintf("%lld:", i);
	    i = l % 3600;
	    p2dig((long)(i / 60));
	    goto minsec;
	}
	i = l;
	shprintf("%lld", i / 60);
minsec:
	i %= 60;
	shprintf(":");
	p2dig((long)i);
}

static void
p2dig(long i)
{

	shprintf("%ld%ld", i / 10, i % 10);
}
 
char linbuf[128];
char *linp = linbuf;

/*
 * putbyte() send a byte to SHOUT.  No interpretation is done
 * except an un-QUOTE'd control character, which is displayed 
 * as ^x.
 */
void
putbyte(int c)
{

	if( !(c & QUOTE) && (c == 0177 || c < ' ' && c != '\t' && c != '\n')) {
	    putbyte('^');
	    if(c == 0177)
		c = '?';
	    else
		c |= 'A' - 1;
	}
	c &= TRIM;
	*linp++ = c;
	if(c == '\n' || linp >= &linbuf[sizeof(linbuf) - 1 - MB_CUR_MAX])
	    flush();
}

/*
 * wputchar(tc) does what putbyte(c) do for a byte c.
 * Note that putbyte(c) just send the byte c (provided c is not
 * a control character) as it is, while putchar(tc) may expand the
 * character tc to some byte sequence that represents the character
 * in mbchar form.
 */
void
wputchar(wchar_t tc)
{
	register int n;

	if(isascii(tc & TRIM)) {
	    putbyte((int)tc);
	    return;
	}
	n = wctomb(linp, tc & TRIM);
	if(n < 0)
	    return;				/* bad mbchar */
	linp += n;
	if(linp >= &linbuf[sizeof(linbuf) - 1 - MB_CUR_MAX])
	    flush(); 
}

void
draino(void)
{
	linp = linbuf;
}

void
flush(void)
{
	register int unit;
#ifdef TIOCLGET
	int lmode;
#endif
	static int interrupted = 0;

	if(linp == linbuf)
	    return;

	if (interrupted) {
		interrupted = 0;
		linp = linbuf;	/* prevent recursion as error calls flush */
		error(NULL);
	}
	interrupted = 1;

	if(haderr)
	    unit = didfds? 2 : SHDIAG;
	else
	    unit = didfds? 1 : SHOUT;

#ifdef TIOCLGET
	if( !didfds
	    && !ioctl(unit, TIOCLGET, (char *)&lmode)
	    && lmode&LFLUSHO) {
		lmode = LFLUSHO;
		(void)ioctl(unit, TIOCLBIC, (char *)&lmode);
		(void)write(unit, "\n", 1);
	}
#endif
	(void)write(unit, linbuf, linp - linbuf);
	linp = linbuf;
	interrupted = 0;
}

/*
 * Should not be needed.
 */
void
write_string(char *s)
{
	register int unit;

	/*
	 * First let's make it sure to flush out things.
	 */
	flush();

	if(haderr)
	    unit = didfds? 2 : SHDIAG;
	else
	    unit = didfds? 1 : SHOUT;
	(void)write(unit, s, strlen(s));
}
