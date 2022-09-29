/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.9 $"

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
 * frank@ceres.esd.sgi.com
 *
 * This module provides with system/library function substitutes
 * for wchar_t datatype.
 */
#ifdef DBG
# include <stdio.h>	/* For <assert.h> needs stderr defined. */
#else/*!DBG*/
# define NDEBUG		/* Disable assert(). */
#endif/*!DBG*/

#include <sys/param.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>
#include <widec.h>
#include <limits.h>
#include <pfmt.h>
#include <unistd.h>
#include <fcntl.h>
#include "sh.h"

static void	illwchar(char *);
static int	mbstrlen(char *, int *);
static int	wcstrlen(wchar_t *, int *);

/*
 * illegal character in mbstr
 */
void
illmbchar(char *s)
{
	showstr(MM_ERROR,
	    gettxt(_SGI_DMMX_csh_illchar, "Illegal character in '%s' !"),
	    s);
}

/*
 * illegal character in wchar
 */
/*ARGSUSED*/
static void
illwchar(char *s)
{
	showstr(MM_ERROR,
	    "Illegal wide character, internal error",
	    0);
}

/*
 * bad environment variable
 */
void
illenvvar(char *s)
{
	showstr(MM_ERROR,
	    gettxt(_SGI_DMMX_csh_illenvvar, "Env variable '%s' ignored"),
	    s);
}

/*
 * strlen of multibyte string
 * in units of wchar_t inclusive \0
 * returns 1 in case of illegal mbchar
 * n ist at least 1 for the '\0'
 */
static int
mbstrlen(char *s, int *np)
{
	register int x;
	wchar_t wc;

	*np = 0;
	do {
	    *np += 1;
	    if((x = mbtowc(&wc, s, MB_CUR_MAX)) < 0)
		return(1);
	    s += x;
	} while(wc);
	return(0);
}

/*
 * strlen of wchar_t string
 * in units of char inclusive \0
 * returns 0 in case of illegal wchar
 * n ist at least 1 for the '\0'
 */
static int
wcstrlen(wchar_t *s, int *np)
{
	register int x;
	register wchar_t wc;
	register int ret = 0;
	char chbuf[MB_LEN_MAX];

	*np = 0;
	do {
	    wc = *s & TRIM;
	    if((x = wctomb(chbuf, wc)) < 0)
		ret = 1;
	    else
		*np += x;
	    s++;
	} while(wc);
	return(ret);
}

/*
 * convert a wide character to mbstring
 */
char *
tctomb(char *s, wchar_t wc)
{
	register int x;

	x = wctomb(s, wc);
	if(x < 0)
	    x = 0;
	s[x] = 0;
	return(s);
}

/*
 * strtots(to, from)
 *
 * Convert a char string 'from' into a wchar_t buffer 'to'.
 * 'to' is assumed to have the enough size to hold the conversion result.
 * When 'to' is NOSTR, strtots() attempts to allocate a space
 * automatically using xalloc().  It is caller's responsibility to
 * free the space allocated in this way, by calling XFREE(ptr).
 * In either case, strtots() returns the pointer to the conversion
 * result (i.e. 'to', if 'to' wasn't NOSTR, or the allocated space.).
 * When a conversion failed, *flag is set to 1.
 */
wchar_t	*
strtots(wchar_t *to, char *from, int *flag)
{
	register char *s = from;
	register wchar_t *d = to;
	int n;
	int myflag;

	if( !flag)
	    flag = &myflag;			/* if no flag */
	if( ! to) {
	    if(*flag = mbstrlen(from, &n))
		illmbchar(from);
	    to = wcalloc(n + 1);		/* allocate memory */
	    mbstowcs(to, from, n);		/* convert */
	    return(to);
	}
	*flag = 0;
	do {
	    if((n = mbtowc(d, s, MB_CUR_MAX)) < 0) {
		illmbchar(from);
		*d = '\0';			/* truncate wcstr */
		*flag = 1;			/* illegal char */
		break;
	    }
	    s += n;
	} while(*d++);
	return(to);
}

/*
 * tstostr(to, from)
 *
 * Convert a wchar string 'from' into a char string buffer 'to'.
 * 'to' is assumed to have the enough size to hold the conversion result.
 * When 'to' is NOSTR, tstostr() attempts to allocate a space
 * automatically using malloc(). It is caller's responsibility to
 * free the space allocated in this way, by calling XFREE(ptr).
 * In either case, tstostr() returns the pointer to the conversion
 * result (i.e. 'to', if 'to' wasn't NOSTR, or the allocated space.).
 * When a conversion or allocation failed, *flag is set to 1.
 */
char *
tstostr(char *to, wchar_t *from, int *flag)
{
	register char *d;
	register wchar_t wc;
	int n;
	int myflag;

	if( !flag)
	    flag = &myflag;			/* if no flag */
	if( ! to) {
	    if(*flag = wcstrlen(from, &n))
		illwchar(0);
	    to = xalloc(n + 1);			/* allocate space */
	}
	*flag = 0;
	d = to;
	do {
	    wc = *from & TRIM;
	    if((n = wctomb(d, wc)) < 0) {
		illwchar(0);
		*flag = 1;			/* ignore illegal wchar */
	    } else
		d += n;
	    from++;
	} while(wc);
	return(to);
}

/*
 * compare two strings in different representation
 */
int
cmpmbwc(wchar_t *wcs, char *mbs, char **p)
{
	register int n;
	register int x;
	wchar_t wc;

	for(x = 0; *wcs && *mbs; wcs++, x++) {
	    n = mbtowc(&wc, mbs, MB_CUR_MAX);
	    if(n < 0)
		return(n);			/* bad mbchar */
	    mbs += n;
	    if(wc != (*wcs & TRIM))
		break;				/* unequal position */
	}
	*p = mbs;				/* save mbs ptr */
	return(x);
}

/*
 * copy wchar_t string and return end of dst string
 * ret: ptr to \0
 */
wchar_t *
wscpyend(wchar_t *d, wchar_t *s)
{
	while(*d++ = *s++);
	return(--d);
}

/*
 * Additional misc functions
 */

/*
 * Calculate the display width of a string.
 */
int
tswidth(wchar_t *cp)
{
	return(wslen(cp));
/*DDDD
	wchar_t	tc;
	int	w=0;

	while(tc=*ts++) w+=csetcol(wcsetno((wchar_t)tc));
	return w;
*/
}

/*
 * Two getenv() substitute functions. They differ in the type of arguments.
 * Both returns the pointer to an allocated space where the env var's values
 * is stored.  This space can be freed after saving the value.
 * There is an arbitary limitation on the number of chars of a env var name.
 */
#define	LONGEST_ENVVARNAME	256		/* Too big? */

wchar_t *
getenvs_(char *name)
{
	register char *val;
	register wchar_t *p;
	int cflag;
	
	val = getenv(name);
	if( !val)
	    return(NOSTR);
	p = strtots(NOSTR, val, &cflag);
	if(cflag) {
	    illenvvar(name);
	    xfree(p);
	    return(NOSTR);
	}
	return(p);
}

wchar_t *
getenv_(wchar_t *name_)
{
	register char *s;
	char name[LONGEST_ENVVARNAME * MB_LEN_MAX];

	s = tstostr(name, name_, NOFLAG);
	return(getenvs_(s));
}

/*
 * Followings are the system call interface for wchar_t strings
 */

/*
 * creat() and open() replacement.
 */
int 
creat_(wchar_t *name, int mode)
{
	char chbuf[MB_MAXPATH];

	wcstombs(chbuf, name, MB_MAXPATH);
	return(creat(chbuf, mode));
}

int 
open_(wchar_t *path, int flags)
{
	char chbuf[MB_MAXPATH];

	wcstombs(chbuf, path, MB_MAXPATH);
	return(open(chbuf, flags));
}

/*
 * read() and write() reaplacement.
 */
int
read_(int fd, wchar_t *buf, int n)
{
	register wchar_t *d;
	register char *s;
	register int inb;
	register int x;
	register int nb;
	register int tord;
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	d = buf;
#ifdef TRACE
	tprintf("Entering read_(fd=%d, buf=0x%x, n=%d);\n", d, buf, n);
#endif
	tord = n;
	inb = 0;
	for(;;) {
	    /*
	     * First we try to read n bytes, because the minimal
	     * encoding is 1 mbchar to 1 wchar. That's better than
	     * reading only 1 byte at a time.
	     */
	    if((nb = read(fd, chbuf + inb, tord)) < 0)
		return(-1);			/* read error */
	    inb += nb;
	    for(s = chbuf; inb >= MB_CUR_MAX;) {
		x = mbtowc(d, s, MB_CUR_MAX);
		if(x < 0) {
		    s[1] = 0;
		    goto illegalchar;		/* bad char = end of read */
		}
		if( !x)
		    x++;			/* \0 is a char here */
		s += x;
		inb -= x;
		d++;
	    }
	    if( !inb)
		return(d - buf);		/* EOF and all converted */
	    /*
	     * Now the # of nb bytes is < MB_CUR_MAX, but
	     * if # of wchars left > MB_CUR_MAX restart
	     * successive read.
	     */
	    if(inb && (s != chbuf))
		bcopy((void *)s, (void *)chbuf, inb);
	    if(nb == tord) {
		tord = n - (d - buf);		/* wchars left */
		if(tord >= MB_CUR_MAX) {
		    tord -= inb;
		    continue;			/* restart succ. read */
		}
		tord = 1;
	    } else
		tord = 0;

	    /*
	     * Now we have less than MB_CUR_MAX bytes left
	     * in the buffer, and (d - buf) wchars to read.
	     * Read them bytewise.
	     */
	    do {
		for(s = chbuf; inb > 0;) {
		    x = mbtowc(d, s, inb);	/* try to convert */
		    if(x >= 0) {
			if( !x)
			    x++;		/* \0 is a char here */
			d++; s += x; inb -= x;	/* rem. bytes form a mbchar */
			continue;
		    }
		    if( !tord || (inb >= MB_CUR_MAX)) {
			s[1] = 0;
			goto illegalchar;	/* bad mbchar = end of read */
		    }
		    /*
		     * Now there is an incomplete mbchar,
		     * try to read the next byte of it.
		     */
		    if((x = read(fd, chbuf + inb, 1)) < 0)
			return(-1);		/* read error */
		    if(x != 1)
			goto illegalchar;	/* bad mbchar = end of read */
		    inb++;
		}
		if(((d - buf) >= n) || !tord)
		    break;			/* n reached */
		if((inb = read(fd, chbuf, 1)) < 0)
		    return(-1);			/* read error */
	    } while(inb);
	    return(d - buf);			/* nwchars read */
	}
illegalchar:
	illmbchar(s);
	if(d > buf)
	    d--;				/* store \n in last char */
	*d++ = '\n';
	return(d - buf);			/* bad mbchar = truncate */
}

/*
 * write a block of n wchar_t to file
 *
 * Problem is JIS:
 *	wctomb() is called between two block write()s. That means,
 *	the shift status of wctomb() will be destroyed.
 */
int
write_(int fd, wchar_t *buf, int n)
{
	register char *d;
	register wchar_t *s;
	register int x;
	register wchar_t wc;
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

#ifdef	TRACE
	tprintf("Entering write_(fd=%d, buf=0x%x, n=%d);\n", fd, buf, n);
#endif
	assert((n * MB_CUR_MAX) < sizeof(chbuf));

	/*
	 * Convert to mbchar string.
	 * NULL is treated as normal char here.
	 */
	for(d = chbuf, s = buf; n--;) {
	    wc = *s++ & TRIM;
	    x = wctomb(d, wc);			/* convert to mbchar */
	    if(x < 0)
		return(-1);			/* bad char = read error */
	    d += x;
	}
	return(write(fd, chbuf, d - chbuf));
}

int
stat_(wchar_t *path, struct stat *buf)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, path, NOFLAG);
	return(stat(chbuf, buf));
}

int
lstat_(wchar_t *path, struct stat *buf)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, path, NOFLAG);
	return(lstat(chbuf, buf));
}

int
access_(wchar_t *path, int mode)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, path, NOFLAG);
	return(access(chbuf, mode));
}

int
chdir_(wchar_t *path)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, path, NOFLAG);
	return(chdir(chbuf));
}

wchar_t *
getwd_(wchar_t *path)
{
	register wchar_t *cp;
	int cflag;
	char chbuf[MB_MAXPATH];
	
	if( !getwd(chbuf))
	    return(0);
	cp = strtots(path, chbuf, &cflag);
	if(cflag)
	    return(0);
	return(cp);
}

int
unlink_(wchar_t *path)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, path, NOFLAG);
	return(unlink(chbuf));
}

DIR *
opendir_(wchar_t *dirname)
{
	char chbuf[MB_MAXPATH];

	tstostr(chbuf, dirname, NOFLAG);
	return(opendir(chbuf));
}

int 
gethostname_(wchar_t *name, int namelen)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	assert(namelen < CSHBUFSIZ);
	if(gethostname(chbuf, sizeof(chbuf)))
	    return(-1);
	if(mbstowcs(name, chbuf, namelen) == (size_t)-1)
	    return(-1);
	return(0);
}

int 
readlink_(wchar_t *path, wchar_t *buf, int bufsiz)
				/* Size of buf in terms of # of wchar_ts. */
{
	int i;
	char chpath[MAXPATHLEN + 1];
	char chbuf[MB_MAXPATH];

	tstostr(chpath, path, NOFLAG);
	i = readlink(chpath, chbuf, sizeof(chbuf));
	if(i < 0)
	    return(-1);
	chbuf[i] = '\0';		/* readlink() doesn't put '\0' */

	i = mbstowcs(buf, chbuf, bufsiz);
	if(i < 0)
	    return(-1);
	return(i);			/* # of wchars excl. '\0' */
}

double
atof_(wchar_t *str)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	tstostr(chbuf, str, NOFLAG);
	return(atof(chbuf));
}

int 
atoi_(wchar_t *str)
{
	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

	tstostr(chbuf, str, NOFLAG);
	return(atoi(chbuf));
}
