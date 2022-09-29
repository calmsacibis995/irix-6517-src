/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.6 $"

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
#include <pfmt.h>
#include <msgs/uxsgicsh.h>
#include "sh.h"
#include "sh.wconst.h"

/*
 * C Shell
 */

static void		asx(wchar_t *, int, wchar_t *);
static void		balance(struct varent *, int, int);
static int		chkpath(wchar_t **, wchar_t **);
static void		exportpath(wchar_t **);
static wchar_t		*getinx(wchar_t *, int *);
static struct varent	*getvx(wchar_t *, int);
static struct varent	*madrof(wchar_t *, struct varent *);
static wchar_t		*operate(wchar_t, wchar_t *, wchar_t *);
static void		putn1(int);
static void		unsetv1(struct varent *);
static wchar_t		*xset(wchar_t *, wchar_t ***);

/*
 * variable setting
 */
void
setq(wchar_t *name, wchar_t **vec, struct varent *p)
{
	register struct varent *c;
	register int f;

#ifdef TRACE
	tprintf("TRACE- setq(%t)\n", name);
#endif
	f = 0;			/* tree hangs off the header's left link */
	while(c = p->v_link[f]) {
	    if( !(f = wscmp(name, c->v_name))) {
		blkfree(c->vec);
		goto found;
	    }
	    p = c;
	    f = f > 0;
	}
	c = (struct varent *)salloc(1, sizeof(struct varent));
	p->v_link[f] = c;
	c->v_name = savestr(name);
	c->v_bal = 0;
	c->v_left = c->v_right = 0;
	c->v_parent = p;
	balance(p, f, 0);
found:
	c->vec = vec;
	trim(vec);
}

void
set1(wchar_t *var, wchar_t **vec, struct varent *head)
{
	register wchar_t **oldv = vec;

#ifdef TRACE
	tprintf("TRACE- set1(%t)\n", var);
#endif
	gflag = 0;
	tglob(oldv);
	if(gflag) {
	    vec = glob(oldv);
	    if( !vec) {
		err_nomatch();
		blkfree(oldv);
		return;
	    }
	    blkfree(oldv);
	    gargv = 0;
	}
	setq(var, vec, head);
}

/*
 * The caller is responsible for putting value in a safe place
 */
void
set(wchar_t *var, wchar_t *val)
{
	register wchar_t **vec;

#ifdef TRACE
	tprintf("TRACE- set()\n");
#endif
	vec = (wchar_t **)salloc(2, sizeof(wchar_t **));
	vec[0] = onlyread(val)? savestr(val) : val;
	vec[1] = 0;
	set1(var, vec, &shvhed);
}

void
doset(wchar_t **v)
{
	register wchar_t *p;
	wchar_t *vp, op;
	wchar_t **vecp;
	bool hadsub;
	int subscr;

#ifdef TRACE
	tprintf("TRACE- doset()\n");
#endif
	v++;				/* 'set' already checked */
	p = *v++;
	if( !p) {
	    prvars();
	    return;
	}
	do {
	    hadsub = 0;
	    for(vp = p; alnum(*p); p++)
		continue;
	    if(vp == p || !letter(*vp))
		goto setsyn;
	    if(*p == '[') {
		hadsub++;
		p = getinx(p, &subscr);
	    }
	    if(op = *p) {
		*p++ = 0;
		if(*p == 0 && *v && **v == '(')
		    p = *v++;
	    } else if(*v && eq(*v, S_EQ)) {
		op = '=', v++;
		if(*v)
		    p = *v++;
	    }
	    if(op && op != '=')
setsyn:
		syntaxerr();
	    if(eq(p, S_LPAR)) {
		register wchar_t **e = v;

		if(hadsub)
		    goto setsyn;
		for(;;) {
		    if( !*e)
			err_missing(')');
		    if(**e == ')')
			break;
		    e++;
		}
		p = *e;
		*e = 0;
		vecp = saveblk(v);
		set1(vp, vecp, &shvhed);
		*e = p;
		v = e + 1;
	    } else if(hadsub) {
#ifdef TRACE
		tprintf("TRACE - doset calling asx(%t)\n", p);
#endif /*TRACE*/
		asx(vp, subscr, savestr(p));
	    } else {
#ifdef TRACE
		tprintf("TRACE - doset calling set\n");
#endif /*TRACE*/
		set(vp, savestr(p));
	    }

	    if(eq(vp, S_path)) {
		exportpath(adrof(S_path)->vec);
		dohash();
	    } else if(eq(vp, S_histchars)) {
		register wchar_t *p = value(S_histchars);
		HIST = *p++;
		HISTSUB = *p;
	    } else if(eq(vp, S_user))
		setenv(S_USER, value(vp));
	    else if(eq(vp, S_term))
		setenv(S_TERM, value(vp));
	    else if(eq(vp, S_home))
		setenv(S_HOME, value(vp));
	    else if(eq(vp, S_filec))
		filec = 1;
	} while(p = *v++);
}

static wchar_t *
getinx(wchar_t *cp, int *ip)
{

#ifdef TRACE
	tprintf("TRACE- getinx()\n");
#endif
	*ip = 0;
	*cp++ = 0;
	while (*cp && digit(*cp))
		*ip = *ip * 10 + *cp++ - '0';
	if (*cp++ != ']')
		bferr(gettxt(_SGI_DMMX_csh_subserr, "Subscript error"));
	return (cp);
}

static void
asx(wchar_t *vp, int subscr, wchar_t *p)
{
	register struct varent *v = getvx(vp, subscr);
	wchar_t *tmp;

#ifdef TRACE
	tprintf("TRACE- asx()\n");
#endif
	/*
	 * Order is important here.  It's possible that we are using
	 * the vector element in the right hand side.  So, we perform
	 * the operation, store the result in a temporary, then
	 * make the assignment from the temporary to the vector element.
	 */
	tmp = globone(p);
	xfree(v->vec[subscr - 1]);
	v->vec[subscr - 1] = tmp;
}

static struct varent *
getvx(wchar_t *vp, int subscr)
{
	register struct varent *v = adrof(vp);

#ifdef TRACE
	tprintf("TRACE- getvx()\n");
#endif
	if (v == 0)
		udvar(vp);
	if (subscr < 1 || subscr > blklen(v->vec))
		bferr(gettxt(_SGI_DMMX_csh_subsout, "Subscript out of range"));
	return (v);
}

wchar_t plusplus[2] = { '1', 0 };

void
dolet(wchar_t **v)
{
	register wchar_t *p;
	wchar_t *vp, c, op;
	bool hadsub;
	int subscr;

	v++;
	p = *v++;
	if (p == 0) {
		prvars();
		return;
	}
	do {
		hadsub = 0;
		for (vp = p; alnum(*p); p++)
			continue;
		if (vp == p || !letter(*vp))
			goto letsyn;
		if (*p == '[') {
			hadsub++;
			p = getinx(p, &subscr);
		}
		if (*p == 0 && *v)
			p = *v++;
		if (op = *p)
			*p++ = 0;
		else
			goto letsyn;
		vp = savestr(vp);
		if (op == '=') {
			c = '=';
			p = xset(p, &v);
		} else {
			c = *p++;
			/* if (any(c, "+-")) { */
			if (c == '+' || c == '-') {
				if (c != op || *p)
					goto letsyn;
				p = plusplus;
			} else {
				/*if (any(op, "<>")) {*/
				if (op == '<' || op == '>') {
					if (c != op)
						goto letsyn;
					c = *p++;
letsyn:
					syntaxerr();
				}
				if (c != '=')
					goto letsyn;
				p = xset(p, &v);
			}
		}
		if (op == '=')
			if (hadsub)
				asx(vp, subscr, p);
			else
				set(vp, p);
		else
			if (hadsub)
				/* avoid bug in vax CC */
				{
					struct varent *gv = getvx(vp, subscr);

					asx(vp, subscr, operate(op, gv->vec[subscr - 1], p));
				}
			else
				set(vp, operate(op, value(vp), p));
		if (eq(vp, S_path/*"path"*/)) {
			exportpath(adrof(S_path)->vec);
			dohash();
		}
		XFREE(vp)
		if (c != '=')
			XFREE(p)
	} while (p = *v++);
}

static wchar_t *
xset(wchar_t *cp, wchar_t ***vp)
{
	register wchar_t *dp;

#ifdef TRACE
	tprintf("TRACE- xset()\n");
#endif
	if (*cp) {
		dp = savestr(cp);
		--(*vp);
		xfree(**vp);
		**vp = dp;
	}
	return (putn(exp(vp)));
}

static wchar_t *
operate(wchar_t op, wchar_t *vp, wchar_t *p)
{
	wchar_t opr[2];
	wchar_t *vec[5];
	register wchar_t **v = vec;
	wchar_t **vecp = v;
	register int i;

	if (op != '=') {
		if (*vp)
			*v++ = vp;
		opr[0] = op;
		opr[1] = 0;
		*v++ = opr;
		if (op == '<' || op == '>')
			*v++ = opr;
	}
	*v++ = p;
	*v++ = 0;
	i = exp(&vecp);
	if (*vecp)
		err_experr();
	return (putn(i));
}

static wchar_t *putp;
 
wchar_t *
putn(int n)
{
	static wchar_t number[15];

#ifdef TRACE
	tprintf("TRACE- putn()\n");
#endif
	putp = number;
	if (n < 0) {
		n = -n;
		*putp++ = '-';
	}
	if (n == -2147483648) {
		*putp++ = '2';
		n = 147483648;
	}
	putn1(n);
	*putp = 0;
	return (savestr(number));
}

static void
putn1(int n)
{
#ifdef TRACE
	tprintf("TRACE- putn1()\n");
#endif
	if (n > 9)
		putn1(n / 10);
	*putp++ = n % 10 + '0';
}

int
getn(wchar_t *cp)
{
	register int n;
	int sign;

#ifdef TRACE
	tprintf("TRACE- getn()\n");
#endif
	sign = 0;
	if (cp[0] == '+' && cp[1])
		cp++;
	if (*cp == '-') {
		sign++;
		cp++;
		if (!digit(*cp))
			goto badnum;
	}
	n = 0;
	while (digit(*cp))
		n = n * 10 + *cp++ - '0';
	if (*cp)
		goto badnum;
	return (sign ? -n : n);
badnum:
	bferr(gettxt(_SGI_DMMX_csh_badnbr, "Badly formed number"));
	return (0);
}

wchar_t *
value1(wchar_t *var, struct varent *head)
{
	register struct varent *vp;

#ifdef TRACE
	tprintf("TRACE- value1()\n");
#endif
	vp = adrof1(var, head);
	return((vp == 0 || vp->vec[0] == 0)? S_ : vp->vec[0]);
}

static struct varent *
madrof(wchar_t *pat, struct varent *vp)
{
	register struct varent *vp1;

#ifdef TRACE
	tprintf("TRACE- madrof()\n");
#endif
	for(; vp; vp = vp->v_right) {
	    if(vp->v_left && (vp1 = madrof(pat, vp->v_left)))
		return(vp1);
	    if(Gmatch(vp->v_name, pat))
		return(vp);
	}
	return(vp);
}

struct varent *
adrof1(wchar_t *name, struct varent *v)
{
	register struct varent *c;
	register int f;

#ifdef TRACE
	tprintf("TRACE- adrof1(%t)\n", name);
#endif
	f = 0;
	while(c = v->v_link[f]) {
	    if( !(f = wscmp(name, c->v_name)))
		break;				/* found */
	    v = c;
	    f = f > 0;
	}
	return(c);
}

void
unset(wchar_t **v)
{

#ifdef TRACE
	tprintf("TRACE- unset()\n");
#endif
	unset1(v, &shvhed);
	if( !adrof(S_histchars)) {
	    HIST = '!';
	    HISTSUB = '^';
	}
	if( !adrof(S_filec))
	    filec = 0;
}

void
unset1(wchar_t *v[], struct varent *head)
{
	register struct varent *vp;
	register int cnt;

#ifdef TRACE
	tprintf("TRACE- unset1()\n");
#endif
	while (*++v) {
		cnt = 0;
		while (vp = madrof(*v, head->v_left))
			unsetv1(vp), cnt++;
		if (cnt == 0)
			setname(*v);
	}
}

void
unsetv(wchar_t *var)
{
	register struct varent *vp;

#ifdef TRACE
	tprintf("TRACE- unsetv()\n");
#endif
	if ((vp = adrof1(var, &shvhed)) == 0)
		udvar(var);
	unsetv1(vp);
}

static void
unsetv1(struct varent *p)
{
	register struct varent *c, *pp;
	register f;

#ifdef TRACE
	tprintf("TRACE- unsetv1()\n");
#endif
	/*
	 * Free associated memory first to avoid complications.
	 */
	blkfree(p->vec);
	XFREE(p->v_name);
	/*
	 * If p is missing one child, then we can move the other
	 * into where p is.  Otherwise, we find the predecessor
	 * of p, which is guaranteed to have no right child, copy
	 * it into p, and move it's left child into it.
	 */
	if (p->v_right == 0)
		c = p->v_left;
	else if (p->v_left == 0)
		c = p->v_right;
	else {
		for (c = p->v_left; c->v_right; c = c->v_right)
			;
		p->v_name = c->v_name;
		p->vec = c->vec;
		p = c;
		c = p->v_left;
	}
	/*
	 * Move c into where p is.
	 */
	pp = p->v_parent;
	f = pp->v_right == p;
	if (pp->v_link[f] = c)
		c->v_parent = pp;
	/*
	 * Free the deleted node, and rebalance.
	 */
	XFREE( (wchar_t *)p);
	balance(pp, f, 1);
}

void
setNS(wchar_t *cp)
{
#ifdef TRACE
	tprintf("TRACE- setNS()\n");
#endif
	set(cp, S_/*""*/);
}

void
shift(wchar_t **v)
{
	register struct varent *argv;
	register wchar_t *name;

#ifdef TRACE
	tprintf("TRACE- shift()\n");
#endif
	v++;
	name = *v;
	if (name == 0)
		name = S_argv/*"argv"*/;
	else
		(void) strip(name);
	argv = adrof(name);
	if (argv == 0)
		udvar(name);
	if (argv->vec[0] == 0)
		bferr(gettxt(_SGI_DMMX_csh_nomorw, "No more words"));
	lshift(argv->vec, 1);
}

static int
chkpath(wchar_t **oval, wchar_t **val)
{
	register wchar_t **s;

	if(oval == val)
	    return(0);				/* first search */
	for(; oval < val; oval++) {
	    if(eq(*oval, *val)) {
		/*
		 * component prev. assembled
		 */
		xfree(*val);
		for(s = val + 1; *val;)		/* remove component */
		    *val++ = *s++;
		return(1);			/* found */
	    }
	}
	return(0);
}

static void
exportpath(wchar_t **val)
{
	register wchar_t **ov = val;
	register wchar_t *p;
	wchar_t exppath[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- exportpath()\n");
#endif
	if(val) {
	    p = exppath;
	    for(p = exppath; *val;) {
		if(chkpath(ov, val))
		    continue;
		if(wslen(*val) + (p - exppath) + 2 > CSHBUFSIZ) {
		    showstr(MM_WARNING,
			gettxt(_SGI_DMMX_csh_ridpat,
			    "Ridiculously long PATH truncated"),
			0);
		    break;
		}
		p = wscpyend(p, *val++);
		if(*val == 0 || eq(*val, S_RPAR))
		    break;
		p = wscpyend(p, S_COLON);
	    }
	}
	setenv(S_PATH, exppath);
}

/*
 * macros to do single rotations on node p
 */
#define rright(p) (\
	t = (p)->v_left,\
	(t)->v_parent = (p)->v_parent,\
	((p)->v_left = t->v_right) ? (t->v_right->v_parent = (p)) : 0,\
	(t->v_right = (p))->v_parent = t,\
	(p) = t)

#define rleft(p) (\
	t = (p)->v_right,\
	(t)->v_parent = (p)->v_parent,\
	((p)->v_right = t->v_left) ? (t->v_left->v_parent = (p)) : 0,\
	(t->v_left = (p))->v_parent = t,\
	(p) = t)

/*
 * Rebalance a tree, starting at p and up.
 * F == 0 means we've come from p's left child.
 * D == 1 means we've just done a delete, otherwise an insert.
 */
static void
balance(struct varent *p, int f, int d)
{
	register struct varent *pp;
	register struct varent *t;		/* used by the rotate macros */
	register ff;

#ifdef TRACE
	tprintf("TRACE- balance()\n");
#endif
	/*
	 * Ok, from here on, p is the node we're operating on;
	 * pp is it's parent; f is the branch of p from which we have come;
	 * ff is the branch of pp which is p.
	 */
	for (; pp = p->v_parent; p = pp, f = ff) {
		ff = pp->v_right == p;
		if (f ^ d) {		/* right heavy */
			switch (p->v_bal) {
			case -1:		/* was left heavy */
				p->v_bal = 0;
				break;
			case 0:			/* was balanced */
				p->v_bal = 1;
				break;
			case 1:			/* was already right heavy */
				switch (p->v_right->v_bal) {
				case 1:			/* sigle rotate */
					pp->v_link[ff] = rleft(p);
					p->v_left->v_bal = 0;
					p->v_bal = 0;
					break;
				case 0:			/* single rotate */
					pp->v_link[ff] = rleft(p);
					p->v_left->v_bal = 1;
					p->v_bal = -1;
					break;
				case -1:		/* double rotate */
					rright(p->v_right);
					pp->v_link[ff] = rleft(p);
					p->v_left->v_bal =
						p->v_bal < 1 ? 0 : -1;
					p->v_right->v_bal =
						p->v_bal > -1 ? 0 : 1;
					p->v_bal = 0;
					break;
				}
				break;
			}
		} else {		/* left heavy */
			switch (p->v_bal) {
			case 1:			/* was right heavy */
				p->v_bal = 0;
				break;
			case 0:			/* was balanced */
				p->v_bal = -1;
				break;
			case -1:		/* was already left heavy */
				switch (p->v_left->v_bal) {
				case -1:		/* single rotate */
					pp->v_link[ff] = rright(p);
					p->v_right->v_bal = 0;
					p->v_bal = 0;
					break;
				case 0:			/* signle rotate */
					pp->v_link[ff] = rright(p);
					p->v_right->v_bal = -1;
					p->v_bal = 1;
					break;
				case 1:			/* double rotate */
					rleft(p->v_left);
					pp->v_link[ff] = rright(p);
					p->v_left->v_bal =
						p->v_bal < 1 ? 0 : -1;
					p->v_right->v_bal =
						p->v_bal > -1 ? 0 : 1;
					p->v_bal = 0;
					break;
				}
				break;
			}
		}
		/*
		 * If from insert, then we terminate when p is balanced.
		 * If from delete, then we terminate when p is unbalanced.
		 */
		if ((p->v_bal == 0) ^ d)
			break;
	}
}

void
plist(struct varent *p)
{
	register struct varent *c;
	register len;

#ifdef TRACE
	tprintf("TRACE- plist()\n");
#endif
	if(setintr)
	    (void)sigsetmask(sigblock(0) & ~ sigmask(SIGINT));
	for(;;) {
	    while(p->v_left)
		p = p->v_left;
	x:
		if( !p->v_parent)		/* is it the header? */
		    return;
		len = blklen(p->vec);
		shprintf("%t", p->v_name);
		wputchar('\t');
		if(len != 1)
		    wputchar('(');
		blkpr(p->vec);
		if(len != 1)
		    wputchar(')');
		wputchar('\n');
		if(p->v_right) {
		    p = p->v_right;
		    continue;
		}
		do {
		    c = p;
		    p = p->v_parent;
		} while (p->v_right == c);
	goto x;
	}
}
