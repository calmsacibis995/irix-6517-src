/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.4 $"

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

#include <unistd.h>
#include "sh.h"
#include "sh.wconst.h"

static void pdeltat(struct timeval *, struct timeval *);
static void tvadd(struct timeval *, struct timeval *);

/*
 * C Shell - routines handling process timing and niceing
 */

void
settimes(void)
{
	struct rusage ruch;

#ifdef TRACE
	tprintf("TRACE- settimes()\n");
#endif
	(void) gettimeofday(&time0, (struct timezone *)0);
	(void) getrusage(RUSAGE_SELF, &ru0);
	(void) getrusage(RUSAGE_CHILDREN, &ruch);
	ruadd(&ru0, &ruch);
}

/*
 * dotime is only called if it is truly a builtin function and not a
 * prefix to another command
 */
void
dotime(void)
{
	struct timeval timedol;
	struct rusage ru1, ruch;

#ifdef TRACE
	tprintf("TRACE- dotime()\n");
#endif
	(void) getrusage(RUSAGE_SELF, &ru1);
	(void) getrusage(RUSAGE_CHILDREN, &ruch);
	ruadd(&ru1, &ruch);
	(void) gettimeofday(&timedol, (struct timezone *)0);
	prusage(&ru0, &ru1, &timedol, &time0);
}

/*
 * donice is only called when it's on the line by itself or with a +- value
 */
void
donice(wchar_t **v)
{
	register wchar_t *cp;
	register int nval;

#ifdef TRACE
	tprintf("TRACE- donice()\n");
#endif
	v++, cp = *v++;
	if (cp == 0)
		nval = 4;
	/* else if (*v == 0 && any(cp[0], "+-")) */
	else if (*v == 0 && (cp[0] == '+' || cp[0] == '-'))
		nval = getn(cp);
	(void) setpriority(PRIO_PROCESS, 0, nval);
}

void
ruadd(struct rusage *ru, struct rusage *ru2)
{
	register long *lp, *lp2;
	register int cnt;

#ifdef TRACE
	tprintf("TRACE- ruadd()\n");
#endif
	tvadd(&ru->ru_utime, &ru2->ru_utime);
	tvadd(&ru->ru_stime, &ru2->ru_stime);
	if (ru2->ru_maxrss > ru->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	cnt = &ru->ru_last - &ru->ru_first + 1;
	lp = &ru->ru_first; lp2 = &ru2->ru_first;
	do
		*lp++ += *lp2++;
	while (--cnt > 0);
}

void
prusage(struct rusage *r0, struct rusage *r1,
	struct timeval *e, struct timeval *b)
{
#define	pgtok(p)	(((p) * pgsize) / 1024)
	static int	pgsize;

	register time_t t =
	    (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	    (r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	    (r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	    (r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
	register wchar_t *cp;
	register int i;
	register struct varent *vp = adrof(S_time/*"time"*/);
	int ms =
	    (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

#ifdef TRACE
	tprintf("TRACE- prusage()\n");
#endif
	if (pgsize == 0)
		pgsize = getpagesize();

	cp = S_USAGEFORMAT /*"%Uu %Ss %E %P %X+%Dk %I+%Oio %Fpf+%Ww"*/;
	if (vp && vp->vec[0] && vp->vec[1])
		cp = vp->vec[1];
	for (; *cp; cp++)
	if (*cp != '%')
		wputchar(*cp);
	else if (cp[1]) switch(*++cp) {

	case 'U':
		pdeltat(&r1->ru_utime, &r0->ru_utime);
		break;

	case 'S':
		pdeltat(&r1->ru_stime, &r0->ru_stime);
		break;

	case 'E':
		psecs(ms / 100LL);
		break;

	case 'P':
		shprintf("%d%%", (int) (t*100 / ((ms ? ms : 1))));
		break;

	case 'W':
		i = r1->ru_nswap - r0->ru_nswap;
		shprintf("%d", i);
		break;

	case 'X':
		shprintf("%d", t == 0 ? 0 :
		    pgtok((r1->ru_ixrss-r0->ru_ixrss)/t));
		break;

	case 'D':
		shprintf("%d", t == 0 ? 0 :
		    pgtok((r1->ru_idrss+r1->ru_isrss-
		    	(r0->ru_idrss+r0->ru_isrss))/t));
		break;

	case 'K':
		shprintf("%d", t == 0 ? 0 :
		    pgtok(((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) -
			(r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t));
		break;

	case 'M':
		shprintf("%d", r1->ru_maxrss);
		break;

	case 'F':
		shprintf("%d", r1->ru_majflt-r0->ru_majflt);
		break;

	case 'R':
		shprintf("%d", r1->ru_minflt-r0->ru_minflt);
		break;

	case 'I':
		shprintf("%d", r1->ru_inblock-r0->ru_inblock);
		break;

	case 'O':
		shprintf("%d", r1->ru_oublock-r0->ru_oublock);
		break;
	}
	wputchar('\n');
#undef	pgtok
}

static void
pdeltat(struct timeval *t1, struct timeval *t0)
{
	struct timeval td;

#ifdef TRACE
	tprintf("TRACE- pdeltat()\n");
#endif
	tvsub(&td, t1, t0);
	shprintf("%d.%01d", td.tv_sec, td.tv_usec/100000);
}

static void
tvadd(struct timeval *tsum, struct timeval *t0)
{

#ifdef TRACE
	tprintf("TRACE- tvadd()\n");
#endif
	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
}

void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

#ifdef TRACE
	tprintf("TRACE- tvsub()\n");
#endif
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}
