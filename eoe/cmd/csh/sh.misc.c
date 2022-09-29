/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.7 $"

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

#include <stdlib.h>
#include <unistd.h>
#include <rpcsvc/ypclnt.h>
#include "sh.h"
#include "sh.wconst.h"

extern char end[];

static wchar_t	**blkcat(wchar_t **, wchar_t **);
static char	**blkcat_(char **, char **);
static char	**blkcpy_(char **, char **);
static wchar_t	**blkend(wchar_t **);
static char	**blkend_(char **);
static int	blklen_(char **);
static int	renum(int, int);

/*
 * C Shell
 */

int
any(int c, wchar_t *s)
{
	if( !s)
	    return(0);			/* no string */
	while(*s)
	    if(*s++ == c)
		return(1);
	return(0);
}

int
onlyread(wchar_t *cp)
{
	return((char *)cp < end);
}

/*
 * free memory
 */
void
xfree(void *cp)
{
#ifdef	TRACE
	tprintf("TRACE - xfree() 0x%x\n", cp);
#endif
	if(((char *)cp >= end) && ((char *)cp < (char *)&cp))
	    free(cp);
}

/*
 * allocate memory and save a wchar_t string
 */
wchar_t *
savestr(wchar_t *s)
{
	register wchar_t *p;

	if( !s)
	    s = S_;
	p = wcalloc(wslen(s) + 1);
	wscpy(p, s);
	return(p);
}

/*
 * allocate memory in units of byte
 */
void *
xalloc(unsigned int n)
{
	register void *p;

#ifdef	ATRACE
	tprintf("TRACE - xalloc() size = %d\n", n);
#endif
	if(p = (char *)malloc(n))
	    return(p);			/* allocated */
	child++;
	err_nomem();			/* no return */
	/*NOTREACHED*/
}

/*
 * allocate memory in units of size
 */
void *
salloc(unsigned int n, unsigned int size)
{
	register void *p;

#ifdef	ATRACE
	tprintf("TRACE - salloc() n=%d, size=%d\n", n, size);
#endif
	if(p = (void *)malloc(n * size)) {
	    bzero(p, n * size);
	    return(p);			/* allocated */
	}
	child++;
	err_nomem();			/* no return */
	/*NOTREACHED*/
}

/*
 * allocate memory in units of wchar_t
 */
wchar_t *
wcalloc(unsigned int n)
{
	register wchar_t *p;

#ifdef	ATRACE
	tprintf("TRACE - wcalloc() size = %d\n", n);
#endif
	if(p = (wchar_t *)malloc(n * sizeof(wchar_t)))
	    return(p);
	child++;
	err_nomem();			/* no return */
	/*NOTREACHED*/
}

static wchar_t **
blkend(wchar_t **up)
{
	while(*up)
	    up++;
	return(up);
}
 
void
blkpr(wchar_t **av)
{
	for(; *av;) {
	    shprintf("%t", *av++);
	    if( !*av)
		break;
	    shprintf(" ");
	}
}

int
blklen(wchar_t **av)
{
	register int n = 0;

	while(*av++)
	    n++;
	return(n);
}

wchar_t **
blkcpy(wchar_t **oav, wchar_t **bv)
{
	register wchar_t **av = oav;

	while(*av++ = *bv++);
	return(oav);
}

static wchar_t **
blkcat(wchar_t **up, wchar_t **vp)
{
	(void)blkcpy(blkend(up), vp);
	return(up);
}

void
blkfree(wchar_t **av0)
{
	register wchar_t **av = av0;

	if( !av)
	    return;
	for(; *av; av++)
	    XFREE(*av)
	XFREE((wchar_t *)av0)
}

wchar_t **
saveblk(wchar_t **v)
{
	register wchar_t **newv;
	register wchar_t **onewv;

	onewv = newv = (wchar_t **)salloc(blklen(v) + 1, sizeof(wchar_t **));
	while(*v)
	    *newv++ = savestr(*v++);
	*newv = 0;
	return(onewv);
}

wchar_t *
strspl(wchar_t *cp, wchar_t *dp)
{
	register wchar_t *ep;
	register wchar_t *p;

	ep = wcalloc(wslen(cp) + wslen(dp) + 1);
	p = wscpyend(ep, cp);
	wscpy(p, dp);
	return(ep);
}

wchar_t **
blkspl(wchar_t **up, wchar_t **vp)
{
	register wchar_t **wp;

	wp = (wchar_t **)salloc(blklen(up) + blklen(vp) + 1,
	    sizeof(wchar_t **));
	(void)blkcpy(wp, up);
	return(blkcat(wp, vp));
}

wchar_t
lastchr(wchar_t *cp)
{
	if( ! *cp)
	    return(0);
	while(cp[1])
	    cp++;
	return(*cp);
}

/*
 * This routine is called after an error to close up
 * any units which may have been left open accidentally.
 * Since the yellow pages keeps sockets open, close them first.
 */
void
closem(void)
{
	register int f;
#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
	char name[256];
#endif
	int hifile;

#if 0 /* Follow tcsh example: yp auto-unbinds if new pid */
	getdomainname(name, sizeof(name));
	yp_unbind(name);
#endif
	hifile = getdtablehi();
	for(f = 0; f < hifile; f++)
	    if((f != SHIN)
		&& (f != SHOUT)
		&& (f != SHDIAG)
		&& (f != OLDSTD)
		&& (f != FSHTTY)
		&& (f != FDIN))
		    (void) close(f);		/* close fildes */
}

void
donefds(void)
{
	(void)close(0);
	(void)close(1);
	(void)close(2);
	didfds = 0;
}

/*
 * descriptor ops
 */
static int
renum(int i, int j)
{
	register int k = dup(i);

	if(k < 0)
	    return(-1);
	if (j == -1 && k > 2)
	    return(k);
	if (k != j) {
	    j = renum(k, j);
	    (void)close(k);
	    return(j);
	}
	return(k);
}

int
dcopy(int i, int j)
{
	if(i == j || i < 0 || j < 0 && i > 2)
	    return(i);
	if(j >= 0) {
	    (void)dup2(i, j);
	    return(j);
	}
	(void)close(j);
	return(renum(i, j));
}

/*
 * Move descriptor i to j.
 * If j is -1 then we just want to get i to a safe place,
 * i.e. to a unit > 2.  This also happens in dcopy.
 */
int
dmove(int i, int j)
{
	if(i == j || i < 0)
	    return(i);
	if(j >= 0) {
	    (void) dup2(i, j);
	    return(j);
	}
	j = dcopy(i, j);
	if(j != i)
	    (void)close(i);
	return(j);
}

/*
 * Left shift a command argument list, discarding
 * the first c arguments.  Used in "shift" commands
 * as well as by commands like "repeat".
 */
void
lshift(wchar_t **v, int c)
{
	register wchar_t **u = v;

	while(*u && (--c >= 0))
	    xfree(*u++);
	(void)blkcpy(v, u);
}

int
number(wchar_t *cp)
{
	if(*cp == '-') {
	    cp++;
	    if( !digit(*cp++))
		return(0);
	}
/*XXXX digit() */
	while(*cp && digit(*cp))
	    cp++;
	return(*cp == 0);
}

wchar_t **
copyblk(wchar_t **v)
{
	register wchar_t **nv;

	nv = (wchar_t **)salloc((unsigned)(blklen(v) + 1), sizeof(wchar_t **));
	return(blkcpy(nv, v));
}

wchar_t *
strend(wchar_t *cp)
{
	while(*cp++);
	return(cp - 1);
}

wchar_t *
strip(wchar_t *cp)
{
	register wchar_t *dp = cp;

	while(*dp)
	    *dp++ &= TRIM;
	return(cp);
}

void
udvar(wchar_t *name)
{
	setname(name);
	bferr(gettxt(_SGI_DMMX_csh_undefvar, "Undefined variable"));
}

int
prefix(wchar_t *sub, wchar_t *str)
{
	for(;;) {
	    if( ! *sub)
		return(1);
	    if( ! *str)
		return(0);
	    if(*sub++ != *str++)
		return(0);
	}
}

/*
 * blk*_ routines
 */
static char **
blkend_(char **up)
{
	while(*up)
	     up++;
	return(up);
}
 
static int
blklen_(char **av)
{
	register int i = 0;

	while(*av++)
	    i++;
	return(i);
}

static char **
blkcpy_(char **oav, char **bv)
{
	register char **av = oav;

	while(*av++ = *bv++);
	return(oav);
}

static char **
blkcat_(char **up, char **vp)
{
	(void)blkcpy_(blkend_(up), vp);
	return(up);
}

char **
blkspl_(char **up, char **vp)
{
	register char **wp;

	wp = (char **)salloc((unsigned)(blklen_(up) + blklen_(vp) + 1),
	    sizeof(char **));
	(void)blkcpy_(wp, up);
	return(blkcat_(wp, vp));
}

/*ARGSUSED*/
int 
wcsetno(wchar_t wc1, wchar_t wc2)
{
	return(1);
}
