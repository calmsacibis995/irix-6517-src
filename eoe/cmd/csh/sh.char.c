/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.2 $"

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

#define	SIZCMAP		128	/* size of the cmap */

unsigned short _cmap[SIZCMAP] = {
/*	nul		soh		stx		etx	*/
	0,		0,		0,		0,

/*	eot		enq		ack		bel	*/
	0,		0,		0,		0,

/*	bs		ht		nl		vt	*/
	0,		_SP|_META,	_NL|_META,	0,

/*	np		cr		so		si	*/
	0,		0,		0,		0,

/*	dle		dc1		dc2		dc3	*/
	0,		0,		0,		0,

/*	dc4		nak		syn		etb	*/
	0,		0,		0,		0,

/*	can		em		sub		esc	*/
	0,		0,		0,		0,

/*	fs		gs		rs		us	*/
	0,		0,		0,		0,

/*	sp		!		"		#	*/
	_SP|_META,	0,		_Q,		_META,

/*	$		%		&		'	*/
	_DOL,		0,		_META,		_Q,

/*	(		)		*		+	*/
	_META,		_META,		_GLOB,		0,

/*	,		-		.		/	*/
	0,		0,		0,		0,

/*	0		1		2		3	*/
	0,		0,		0,		0,

/*	4		5		6		7	*/
	0,		0,		0,		0,

/*	8		9		:		;	*/
	0,		0,		0,		_META,

/*	<		=		>		?	*/
	_META,		0,		_META,		_GLOB,

/*	@		A		B		C	*/
	0,		0,		0,		0,

/*	D		E		F		G	*/
	0,		0,		0,		0,

/*	H		I		J		K	*/
	0,		0,		0,		0,

/*	L		M		N		O	*/
	0,		0,		0,		0,

/*	P		Q		R		S	*/
	0,		0,		0,		0,

/*	T		U		V		W	*/
	0,		0,		0,		0,

/*	X		Y		Z		[	*/
	0,		0,		0,		_GLOB,

/*	\		]		^		_	*/
	_ESC,		0,		0,		0,

/*	`		a		b		c	*/
	_Q1|_GLOB,	0,		0,		0,

/*	d		e		f		g	*/
	0,		0,		0,		0,

/*	h		i		j		k	*/
	0,		0,		0,		0,

/*	l		m		n		o	*/
	0,		0,		0,		0,

/*	p		q		r		s	*/
	0,		0,		0,		0,

/*	t		u		v		w	*/
	0,		0,		0,		0,

/*	x		y		z		{	*/
	0,		0,		0,		_GLOB,

/*	|		}		~		del	*/
	_META,		0,		0,		0,
};

/*
 * look up a char in cmap
 */
int
cmlook(wchar_t c, int x)
{
	c &= TRIM;
	if(c >= SIZCMAP)
	    return(0);
	return(_cmap[c] & x);
}

/*
 * check for letter
 */
int
letter(wchar_t c)
{
	c &= TRIM;
	return(iswalpha(c)
	    || (c == '_')
	    || isphonogram(c)
	    || isideogram(c));
}

/*
 * check for alpha-numeric
 */
int
alnum(wchar_t c)
{
	c &= TRIM;
	return(iswalnum(c)
	    || (c == '_')
	    || isphonogram(c)
	    || isideogram(c));
}
