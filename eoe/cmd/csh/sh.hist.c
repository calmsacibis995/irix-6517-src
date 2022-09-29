/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.3 $"

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

#include "sh.h"
#include "sh.wconst.h"

static void	dohist1(struct Hist *, int *, int, int);
static void	hfree(struct Hist *);
static void	phist(struct Hist *, int);

/*
 * C shell
 */

void
savehist(struct wordent *sp)
{
	register struct Hist *hp, *np;
	register int histlen = 0;
	register wchar_t *p;
	register wchar_t *cp;

#ifdef TRACE
	tprintf("TRACE- savehist()\n");
#endif
	/*
	 * throw away null lines
	 */
	if(sp->next->word[0] == '\n')
	    return;
	cp = value(S_history);
	if(*cp) {
	    p = cp;
	    while(*p) {
		if( !digit(*p)) {
		    histlen = 0;
		    break;
		}
		histlen = histlen * 10 + *p++ - '0';
	    }
	}
	for(hp = &Histlist; np = hp->Hnext;) {
	    if(np->Href < 0 || eventno - np->Href >= histlen || !histlen)
		hp->Hnext = np->Hnext, hfree(np);
	    else
		hp = np;
	}
	(void)enthist(++eventno, sp, 1);
}

struct Hist *
enthist(int event, struct wordent *lp, bool docopy)
{
	register struct Hist *np;

#ifdef TRACE
	tprintf("TRACE- enthist()\n");
#endif
	np = (struct Hist *)salloc(1, sizeof(struct Hist));
	np->Hnum = np->Href = event;
	if(docopy)
	    copylex(&np->Hlex, lp);
	else {
	    np->Hlex.next = lp->next;
	    lp->next->prev = &np->Hlex;
	    np->Hlex.prev = lp->prev;
	    lp->prev->next = &np->Hlex;
	}
	np->Hnext = Histlist.Hnext;
	Histlist.Hnext = np;
	return(np);
}

static void
hfree(struct Hist *hp)
{
#ifdef TRACE
	tprintf("TRACE- hfree()\n");
#endif

	freelex(&hp->Hlex);
	xfree(hp);
}

void
dohist(wchar_t **vp)
{
	register wchar_t *vp2;
	register int rflg = 0;
	register int hflg = 0;
	int n;

#ifdef TRACE
	tprintf("TRACE- dohist()\n");
#endif
	if( !(n = getn(value(S_history))))
	    return;
	if(setintr)
	    (void)sigsetmask(sigblock(0) & ~sigmask(SIGINT));
	while(*++vp && **vp == '-') {
	    vp2 = *vp;
	    while(*++vp2) {
		switch (*vp2) {
		    case 'h':
			hflg++;
			break;
		    case 'r':
			rflg++;
			break;
		    case '-':	/* ignore multiple '-'s */
			break;
		    default:
			err_unknflag(*vp2);
			err_usage("history [-rh] [# number of events]");
		}
	    }
	}
	if(*vp)
	    n = getn(*vp);
	dohist1(Histlist.Hnext, &n, rflg, hflg);
}

static void
phist(struct Hist *hp, int hflg)
{
#ifdef TRACE
	tprintf("TRACE- phist()\n");
#endif
	if( !hflg)
	    shprintf("%6d\t", hp->Hnum);
	prlex(&hp->Hlex);
}

static void
dohist1(struct Hist *hp, int *np, int rflg, int hflg)
{
	bool print = (*np) > 0;

#ifdef TRACE
	tprintf("TRACE- dohist1()\n");
#endif
	while(hp) {
	    (*np)--;
	    hp->Href++;
	    if( !rflg) {
		dohist1(hp->Hnext, np, rflg, hflg);
		if(print)
		    phist(hp, hflg);
		return;
	    }
	    if(*np >= 0)
		phist(hp, hflg);
	    hp = hp->Hnext;
	}
}
