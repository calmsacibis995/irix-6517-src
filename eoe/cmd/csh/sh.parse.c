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

/*
 * C shell
 */
char	s_toomany[] = "Too many %s's";

static void		asyn0(struct wordent *, struct wordent *);
static void		asyn3(struct wordent *, struct wordent *);
static void		asyntax(struct wordent *, struct wordent *);
static struct wordent	*freenod(struct wordent *, struct wordent *);
static struct command	*syn0(struct wordent *, struct wordent *, int);
static struct command	*syn1(struct wordent *, struct wordent *, int);
static struct command	*syn1a(struct wordent *, struct wordent *, int);
static struct command	*syn1b(struct wordent *, struct wordent *, int);
static struct command	*syn2(struct wordent *, struct wordent *, int);
static struct command	*syn3(struct wordent *, struct wordent *, int);

/*
 * Perform aliasing on the word list lex
 * Do a (very rudimentary) parse to separate into commands.
 * If word 0 of a command has an alias, do it.
 * Repeat a maximum of 100 times.
 */
void
alias(struct wordent *lex)
{
	volatile int aleft = 101;
	volatile jmp_buf osetexit;

#ifdef TRACE
	tprintf("TRACE- alias()\n");
#endif
	getexit(osetexit);
	setexit();
	if(haderr) {
	    resexit(osetexit);
	    reset();
	}
	if(--aleft == 0)
	    error(gettxt(_SGI_DMMX_csh_alloop, "Alias loop"));
	asyntax(lex->next, lex);
	resexit(osetexit);
}

static void
asyntax(struct wordent *p1, struct wordent *p2)
{
	register wchar_t wc;

#ifdef TRACE
	tprintf("TRACE- asyntax()\n");
#endif
	while(p1 != p2) {
	    wc = p1->word[0];
	    if(wc == ';' || wc == '&' || wc == '\n')
		p1 = p1->next;
	    else {
		asyn0(p1, p2);
		return;
	    }
	}
}

static void
asyn0(struct wordent *p1, struct wordent *p2)
{
	register struct wordent *p;
	register int l = 0;

#ifdef TRACE
	tprintf("TRACE- asyn0()\n");
#endif
	for(p = p1; p != p2; p = p->next)
	    switch(p->word[0]) {
		case '(':
		    l++;
		    continue;
		case ')':
		    l--;
		    if(l < 0)
			err_toomany(')');
		    continue;
		case '>':
		    if(p->next != p2 && eq(p->next->word, S_AND))
			p = p->next;
		    continue;
		case '&':
		case '|':
		case ';':
		case '\n':
		    if(l)
			continue;
		    asyn3(p1, p);
		    asyntax(p->next, p2);
		    return;
	    }
	if( !l)
	    asyn3(p1, p2);
}

static void
asyn3(struct wordent *p1, struct wordent *p2)
{
	register struct varent *ap;
	struct wordent alout;
	register bool redid;

#ifdef TRACE
	tprintf("TRACE- asyn3()\n");
#endif
	if (p1 == p2)
		return;
	if (p1->word[0] == '(') {
		for (p2 = p2->prev; p2->word[0] != ')'; p2 = p2->prev)
			if (p2 == p1)
				return;
		if (p2 == p1->next)
			return;
		asyn0(p1->next, p2);
		return;
	}
	ap = adrof1(p1->word, &aliases);
	if (ap == 0)
		return;
	alhistp = p1->prev;
	alhistt = p2;
	alvec = ap->vec;
	redid = lex(&alout);
	alhistp = alhistt = 0;
	alvec = 0;
	if (errstr) {
		freelex(&alout);
		error_errstr();
	}
	if (p1->word[0] && eq(p1->word, alout.next->word)) {
		wchar_t *cp = alout.next->word;

		alout.next->word = strspl(S_TOPBIT /*"\200"*/, cp);
		XFREE(cp)
	}
	p1 = freenod(p1, redid ? p2 : p1->next);
	if (alout.next != &alout) {
		p1->next->prev = alout.prev->prev;
		alout.prev->prev->next = p1->next;
		alout.next->prev = p1;
		p1->next = alout.next;
		XFREE(alout.prev->word)
		XFREE( (wchar_t *)alout.prev)
	}
	reset();		/* throw! */
}

static struct wordent *
freenod(struct wordent *p1, struct wordent *p2)
{
	register struct wordent *retp = p1->prev;

#ifdef TRACE
	tprintf("TRACE- freenod()\n");
#endif
	while (p1 != p2) {
		XFREE(p1->word)
		p1 = p1->next;
		XFREE( (wchar_t *)p1->prev)
	}
	retp->next = p2;
	p2->prev = retp;
	return (retp);
}

#define	PHERE	1
#define	PIN	2
#define	POUT	4
#define	PDIAG	8

/*
 * syntax
 *	empty
 *	syn0
 */
struct command *
syntax(struct wordent *p1, struct wordent *p2, int flags)
{
	register wchar_t wc;

#ifdef TRACE
	tprintf("TRACE- syntax()\n");
#endif
	while(p1 != p2) {
	    wc = p1->word[0];
	    if(wc == ';' || wc == '&' || wc == '\n')
		p1 = p1->next;
	    else
		return(syn0(p1, p2, flags));
	}
	return(0);
}

/*
 * syn0
 *	syn1
 *	syn1 & syntax
 */
static struct command *
syn0(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p;
	register struct command *t, *t1;
	int l;

#ifdef TRACE
	tprintf("TRACE- syn0()\n");
#endif
	l = 0;
	for(p = p1; p != p2; p = p->next) {
	    switch(p->word[0]) {
		case '(':
			l++;
			continue;
		case ')':
		    l--;
		    if (l < 0)
			seterrc(gettxt(_SGI_DMMX_csh_toomany, s_toomany), ')');
		    continue;
		case '|':
			if (p->word[1] == '|')
				continue;
			/* fall into ... */
		case '>':
			if (p->next != p2 && eq(p->next->word, S_AND /*"&"*/))
				p = p->next;
			continue;
		case '&':
			if (l != 0)
				break;
			if (p->word[1] == '&')
				continue;
			t1 = syn1(p1, p, flags);
			if (t1->t_dtyp == TLST ||
    			    t1->t_dtyp == TAND ||
    			    t1->t_dtyp == TOR) {
				t = (struct command *)salloc(1,
					sizeof(struct command));
				t->t_dtyp = TPAR;
				t->t_dflg = FAND|FINT;
				t->t_dspr = t1;
				t1 = t;
			} else
				t1->t_dflg |= FAND|FINT;
			t = (struct command *)salloc(1, sizeof(struct command));
			t->t_dtyp = TLST;
			t->t_dflg = 0;
			t->t_dcar = t1;
			t->t_dcdr = syntax(p, p2, flags);
			return(t);
		}
	}
	if( !l)
	    return(syn1(p1, p2, flags));
	seterrc(gettxt(_SGI_DMMX_csh_toomany, s_toomany), '(');
	return(0);
}

/*
 * syn1
 *	syn1a
 *	syn1a ; syntax
 */
static struct command *
syn1(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p;
	register struct command *t;
	int l;

#ifdef TRACE
	tprintf("TRACE- syn1()\n");
#endif
	l = 0;
	for(p = p1; p != p2; p = p->next) {
	    switch(p->word[0]) {
		case '(':
		    l++;
		    continue;
		case ')':
		    l--;
		    continue;
		case ';':
		case '\n':
		    if(l)
			break;
		    t = (struct command *)salloc(1, sizeof(struct command));
		    t->t_dtyp = TLST;
		    t->t_dflg = 0;
		    t->t_dcar = syn1a(p1, p, flags);
		    t->t_dcdr = syntax(p->next, p2, flags);
		    if( ! t->t_dcdr) {
			t->t_dcdr = t->t_dcar;
			t->t_dcar = 0;
		    }
		    return(t);
	    }
	}
	return(syn1a(p1, p2, flags));
}

/*
 * syn1a
 *	syn1b
 *	syn1b || syn1a
 */
static struct command *
syn1a(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p;
	register struct command *t;
	register int l = 0;

#ifdef TRACE
	tprintf("TRACE- syn1a()\n");
#endif
	for (p = p1; p != p2; p = p->next)
		switch (p->word[0]) {

		case '(':
			l++;
			continue;

		case ')':
			l--;
			continue;

		case '|':
			if (p->word[1] != '|')
				continue;
			if (l == 0) {
				t = (struct command *)salloc(1,
					sizeof(struct command));
				t->t_dtyp = TOR;
				t->t_dcar = syn1b(p1, p, flags);
				t->t_dcdr = syn1a(p->next, p2, flags);
				t->t_dflg = 0;
				return (t);
			}
			continue;
		}
	return (syn1b(p1, p2, flags));
}

/*
 * syn1b
 *	syn2
 *	syn2 && syn1b
 */
static struct command *
syn1b(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p;
	register struct command *t;
	register int l = 0;

#ifdef TRACE
	tprintf("TRACE- syn1b()\n");
#endif
	l = 0;
	for (p = p1; p != p2; p = p->next)
		switch (p->word[0]) {

		case '(':
			l++;
			continue;

		case ')':
			l--;
			continue;

		case '&':
			if (p->word[1] == '&' && l == 0) {
				t = (struct command *)salloc(1,
					sizeof(struct command));
				t->t_dtyp = TAND;
				t->t_dcar = syn2(p1, p, flags);
				t->t_dcdr = syn1b(p->next, p2, flags);
				t->t_dflg = 0;
				return (t);
			}
			continue;
		}
	return (syn2(p1, p2, flags));
}

/*
 * syn2
 *	syn3
 *	syn3 | syn2
 *	syn3 |& syn2
 */
static struct command *
syn2(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p, *pn;
	register struct command *t;
	register int l = 0;
	int f;

#ifdef TRACE
	tprintf("TRACE- syn2()\n");
#endif
	for (p = p1; p != p2; p = p->next)
		switch (p->word[0]) {

		case '(':
			l++;
			continue;

		case ')':
			l--;
			continue;

		case '|':
			if (l != 0)
				continue;
			t = (struct command *)salloc(1, sizeof(struct command));
			f = flags | POUT;
			pn = p->next;
			if (pn != p2 && pn->word[0] == '&') {
				f |= PDIAG;
				t->t_dflg |= FDIAG;
			}
			t->t_dtyp = TFIL;
			t->t_dcar = syn3(p1, p, f);
			if (pn != p2 && pn->word[0] == '&')
				p = pn;
			t->t_dcdr = syn2(p->next, p2, flags | PIN);
			return (t);
		}
	return (syn3(p1, p2, flags));
}

/*
 * syn3
 *	( syn0 ) [ < in  ] [ > out ]
 *	word word* [ < in ] [ > out ]
 *	KEYWORD ( word* ) word* [ < in ] [ > out ]
 *
 *	KEYWORD = (@ exit foreach if set switch test while)
 */
static struct command *
syn3(struct wordent *p1, struct wordent *p2, int flags)
{
	register struct wordent *p;
	struct wordent *lp, *rp;
	register struct command *t;
	register int l;
	wchar_t **av;
	wchar_t c;
	int n;
	bool specp = 0;

#ifdef TRACE
	tprintf("TRACE- syn3()\n");
#endif
	if(p1 != p2) {
	    p = p1;
again:
	    switch(srchx(p->word)) {
		case ZELSE:
		    p = p->next;
		    if(p != p2)
			goto again;
		    break;
		case ZEXIT:
		case ZFOREACH:
		case ZIF:
		case ZLET:
		case ZSET:
		case ZSWITCH:
		case ZWHILE:
		    specp = 1;
		    break;
	    }
	}
	n = 0;
	l = 0;
	for(p = p1; p != p2; p = p->next) {
	    switch (p->word[0]) {
		case '(':
		    if (specp)
			n++;
		    l++;
		    continue;
		case ')':
		    if (specp)
			n++;
		    l--;
		    continue;
		case '>':
		case '<':
		    if (l != 0) {
			if (specp)
				n++;
			continue;
		    }
		    if (p->next == p2)
			continue;
		    if (any(p->next->word[0], S_RELPAR))
			continue;
		    n--;
		    continue;
		default:
		    if (!specp && l)
			continue;
		    n++;
		    continue;
		}
	}
	if(n < 0)
	    n = 0;

	t = (struct command *)salloc(1, sizeof(struct command));
	av = (wchar_t **)salloc(n + 2, sizeof(wchar_t **));
	t->t_dcom = av;
	t->t_dflg = 0;
	n = 0;
	if (p2->word[0] == ')')
		t->t_dflg = FPAR;
	lp = 0;
	rp = 0;
	l = 0;
	for(p = p1; p != p2; p = p->next) {
	    c = p->word[0];
	    switch(c) {
		case '(':
		    if( !l) {
			if(lp && !specp)
			    seterr(gettxt(_SGI_DMMX_csh_badpllb,
				"Badly placed ("));
			lp = p->next;
		    }
		    l++;
		    goto savep;
		case ')':
		    l--;
		    if( !l)
			rp = p;
		    goto savep;
		case '>':
		    if (l != 0)
			goto savep;
		    if (p->word[1] == '>')
			t->t_dflg |= FCAT;
		    if (p->next != p2 && eq(p->next->word, S_AND /*"&"*/)) {
			t->t_dflg |= FDIAG, p = p->next;
			if (flags & (POUT|PDIAG))
				goto badout;
		    }
		    if (p->next != p2 && eq(p->next->word, S_EXAS /*"!"*/))
			t->t_dflg |= FANY, p = p->next;
		    if (p->next == p2) {
missfile:
			seterr(gettxt(_SGI_DMMX_csh_missnamred,
			    "Missing name for redirect"));
			continue;
		    }
		    p = p->next;
		    if (any(p->word[0], S_RELPAR))
			goto missfile;
		    if ((flags & POUT) && (flags & PDIAG) == 0 || t->t_drit)
badout:
			seterr(gettxt(_SGI_DMMX_csh_ambored,
			    "Ambiguous output redirect"));
		    else
			t->t_drit = savestr(p->word);
		    continue;
		case '<':
			if (l != 0)
				goto savep;
			if (p->word[1] == '<')
				t->t_dflg |= FHERE;
			if (p->next == p2)
				goto missfile;
			p = p->next;
			if (any(p->word[0], S_RELPAR))
				goto missfile;
			if ((flags & PHERE) && (t->t_dflg & FHERE))
				seterr(gettxt(_SGI_DMMX_csh_cantred,
				    "Can't << within ()'s"));
			else if ((flags & PIN) || t->t_dlef)
				seterr(gettxt(_SGI_DMMX_csh_ambired,
				    "Ambiguous input redirect"));
			else
				t->t_dlef = savestr(p->word);
			continue;
savep:
			if (!specp)
				continue;
		default:
		    if(l && !specp)
			continue;
		    if( !errstr)
			av[n] = savestr(p->word);
		    n++;
		    continue;
		}
	}
	av[n] = 0;
	if(lp && !specp) {
	    if(n)
		seterr(gettxt(_SGI_DMMX_csh_badplbb, "Badly placed ()'s"));
	    t->t_dtyp = TPAR;
	    t->t_dspr = syn0(lp, rp, PHERE);
	} else {
	    if( !n)
		seterr(gettxt(_SGI_DMMX_csh_inv0cmd, "Invalid null command"));
	    t->t_dtyp = TCOM;
	}
	return(t);
}

void
freesyn(struct command *t)
{
	register wchar_t **v;

#ifdef TRACE
	tprintf("TRACE- freesyn()\n");
#endif
	if (t == 0)
		return;
	switch (t->t_dtyp) {

	case TCOM:
		for (v = t->t_dcom; *v; v++)
			XFREE(*v)
		XFREE( (wchar_t *)t->t_dcom)
		goto lr;

	case TPAR:
		freesyn(t->t_dspr);
		/* fall into ... */

lr:
		XFREE(t->t_dlef)
		XFREE(t->t_drit)
		break;

	case TAND:
	case TOR:
	case TFIL:
	case TLST:
		freesyn(t->t_dcar), freesyn(t->t_dcdr);
		break;
	}
	XFREE( (wchar_t *)t)
}
