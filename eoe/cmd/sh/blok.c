/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:blok.c	1.9.6.1"
/*
 *	UNIX shell
 */
#include	<memory.h>
#include	"defs.h"


/*
 *	storage allocator
 *	(circular first fit strategy)
 */

#define BUSY 01
#define busy(x)	(Rcheat((x)->ag.word) & BUSY)

unsigned	brkincr = BRKINCR;
static struct blk *blokp;			/*current search pointer*/
static struct blk *bloktop;		/* top of arena (last blok) */

static unsigned char	*brkbegin, *aligned_brkbegin;
unsigned char		*setbrk();
void	addblok();

#ifdef __STDC__
void *
#else
char *
#endif
alloc(nbytes)
	size_t nbytes;
{
	register unsigned rbytes = round(nbytes+ALIGN, ALIGN);

	for (;;)
	{
		int	c = 0;
		register struct blk *p = blokp;
		register struct blk *q;

		do
		{
			if (!busy(p))
			{
				while (!busy(q = p->ag.word))
					p->ag.word = q->ag.word;
				if ((char *)q - (char *)p >= rbytes)
				{
					blokp = (struct blk *)((char *)p + rbytes);
					if (q > blokp)
						blokp->ag.word = p->ag.word;
					p->ag.word = (struct blk *)(Rcheat(blokp) | BUSY);
					return((char *)(p + 1));
				}
			}
			q = p;
			p = (struct blk *)(Rcheat(p->ag.word) & ~BUSY);
		} while (p > q || (c++) == 0);
		addblok(rbytes);
	}
}

void
addblok(reqd)
	unsigned reqd;
{
	if (stakbot == 0)
	{
		brkbegin = setbrk(3 * BRKINCR);
		aligned_brkbegin = (unsigned char *) round(brkbegin, ALIGN);
		bloktop = (struct blk *) aligned_brkbegin;
	}

	if (stakbas != staktop)
	{
		register unsigned char *rndstak;
		register struct blk *blokstak;

		/*
		 * Round up at least one byte so
		 * staktop doesn't get clobbered.
		 */

		rndstak = (unsigned char *)round(staktop+1, ALIGN);
		blokstak = (struct blk *)(stakbas) - 1;
		blokstak->ag.word = stakbsy;
		stakbsy = blokstak;
		bloktop->ag.word = (struct blk *)(Rcheat(rndstak) | BUSY);
		bloktop = (struct blk *)(rndstak);
	}
	reqd += brkincr;
	reqd &= ~(brkincr - 1);
	blokp = bloktop;
	bloktop = bloktop->ag.word = (struct blk *)(Rcheat(bloktop) + reqd);
	bloktop->ag.word = (struct blk *) ((long) aligned_brkbegin | BUSY);
	{
		register unsigned char *stakadr = (unsigned char *)(bloktop + 2);

		int length = staktop - stakbot;
		if(length)
			(void)memcpy(stakadr, stakbot, length);
		stakbas = stakbot = stakadr;
		staktop = stakbot + length;
	}
}

void
free(ap)
	void *ap;
{
	register struct blk *p;

	p = (struct blk *)ap;
	if (p && p < bloktop && p > (struct blk *)aligned_brkbegin)
	{
#ifdef DEBUG
		chkbptr(p);
#endif
		--p;
		p->ag.word = (struct blk *)(Rcheat(p->ag.word) & ~BUSY);
	}


}

/*
 * realloc() is needed by __gtxt(). Normally not used.
 */
void *
realloc(oldp, nbytes)
void *oldp;
size_t nbytes;
{
	void *newp;

	newp = alloc((int)nbytes);
	if(newp){
		(void)memcpy(newp, oldp, nbytes);
		free(oldp);
	}
	return(newp);
}
#ifdef DEBUG

chkbptr(ptr)
	struct blk *ptr;
{
	int	exf = 0;
	register struct blk *p = (struct blk *)aligned_brkbegin;
	register struct blk *q;
	int	us = 0, un = 0;

	for (;;)
	{
		q = (struct blk *)(Rcheat(p->ag.word) & ~BUSY);

		if (p+1 == ptr)
			exf++;

		if (q < (struct blk *)aligned_brkbegin || q > bloktop)
			abort(/* 3 */);

		if (p == bloktop)
			break;

		if (busy(p))
			us += q - p;
		else
			un += q - p;

		if (p >= q)
			abort(/* 4 */);

		p = q;
	}
	if (exf == 0)
		abort(/* 1 */);
}


chkmem()
{
	register struct blk *p = (struct blk *)aligned_brkbegin;
	register struct blk *q;
	int	us = 0, un = 0;

	for (;;)
	{
		q = (struct blk *)(Rcheat(p->ag.word) & ~BUSY);

		if (q < (struct blk *)aligned_brkbegin || q > bloktop)
			abort(/* 3 */);

		if (p == bloktop)
			break;

		if (busy(p))
			us += q - p;
		else
			un += q - p;

		if (p >= q)
			abort(/* 4 */);

		p = q;
	}

	prs("un/used/avail ");
	prn(un);
	blank();
	prn(us);
	blank();
	prn((char *)bloktop - aligned_brkbegin - (un + us));
	newline();

}

#endif /* DEBUG */
