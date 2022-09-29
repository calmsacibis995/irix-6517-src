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
#include "sh.proc.h"
#include "sh.wconst.h"

/*
 * C shell
 */

#define IGNORE	1	/* in ignore, it means to ignore value, just parse */
#define NOGLOB	2	/* in ignore, it means not to globone */

#define	ADDOP	1
#define	MULOP	2
#define	EQOP	4
#define	RELOP	8
#define	RESTOP	16
#define	ANYOP	31

#define	EQEQ	1
#define	GTR	2
#define	LSS	4
#define	NOTEQ	6
#define EQMATCH 7
#define NOTEQMATCH 8

static int	egetn(wchar_t *);
#ifdef EDEBUG
static void	etracc(wchar_t *, wchar_t *, wchar_t ***);
static void	etraci(wchar_t *, int, wchar_t ***);
#endif
static void	evalav(wchar_t **);
static int	exp1(wchar_t ***, bool);
static int	exp2(wchar_t ***, bool);
static int	exp2a(wchar_t ***, bool);
static int	exp2b(wchar_t ***, bool);
static int	exp2c(wchar_t ***, bool);
static wchar_t	*exp3(wchar_t ***, bool);
static wchar_t	*exp3a(wchar_t ***, bool);
static wchar_t	*exp4(wchar_t ***, bool);
static wchar_t	*exp5(wchar_t ***, bool);
static wchar_t	*exp6(wchar_t ***, bool);
static int	isa(wchar_t *, int);

int
exp(wchar_t ***vp)
{
#ifdef TRACE
	tprintf("TRACE- exp()\n");
#endif
	return(exp0(vp, 0));
}

int
exp0(wchar_t ***vp, bool ignore)
{
	register int p1 = exp1(vp, ignore);
#ifdef TRACE
	tprintf("TRACE- exp0()\n");
#endif
	
#ifdef EDEBUG
	etraci("exp0 p1", p1, vp);
#endif
	if(**vp && eq(**vp, S_BARBAR)) {
	    register int p2;

	    (*vp)++;
	    p2 = exp0(vp, (ignore & IGNORE) || p1);
#ifdef EDEBUG
	    etraci("exp0 p2", p2, vp);
#endif
	    return(p1 || p2);
	}
	return(p1);
}

static int
exp1(wchar_t ***vp, bool ignore)
{
	register int p1 = exp2(vp, ignore);

#ifdef TRACE
	tprintf("TRACE- exp1()\n");
#endif
#ifdef EDEBUG
	etraci("exp1 p1", p1, vp);
#endif
	if(**vp && eq(**vp, S_ANDAND)) {
	    register int p2;

	    (*vp)++;
	    p2 = exp1(vp, (ignore & IGNORE) || !p1);
#ifdef EDEBUG
	    etraci("exp1 p2", p2, vp);
#endif
	    return(p1 && p2);
	}
	return(p1);
}

static int
exp2(wchar_t ***vp, bool ignore)
{
	register int p1 = exp2a(vp, ignore);

#ifdef TRACE
	tprintf("TRACE- exp2()\n");
#endif
#ifdef EDEBUG
	etraci("exp3 p1", p1, vp);
#endif
	if(**vp && eq(**vp, S_BAR)) {
	    register int p2;

	    (*vp)++;
	    p2 = exp2(vp, ignore);
#ifdef EDEBUG
	    etraci("exp3 p2", p2, vp);
#endif
	    return(p1 | p2);
	}
	return(p1);
}

static int
exp2a(wchar_t ***vp, bool ignore)
{
	register int p1 = exp2b(vp, ignore);

#ifdef TRACE
	tprintf("TRACE- exp2a()\n");
#endif
#ifdef EDEBUG
	etraci("exp2a p1", p1, vp);
#endif
	if(**vp && eq(**vp, S_HAT /*"^" */)) {
	    register int p2;

	    (*vp)++;
	    p2 = exp2a(vp, ignore);
#ifdef EDEBUG
	    etraci("exp2a p2", p2, vp);
#endif
	    return(p1 ^ p2);
	}
	return(p1);
}

static int
exp2b(wchar_t ***vp, bool ignore)
{
	register int p1 = exp2c(vp, ignore);

#ifdef TRACE
	tprintf("TRACE- exp2b()\n");
#endif
#ifdef EDEBUG
	etraci("exp2b p1", p1, vp);
#endif
	if (**vp && eq(**vp, S_AND /*"&"*/)) {
		register int p2;

		(*vp)++;
		p2 = exp2b(vp, ignore);
#ifdef EDEBUG
		etraci("exp2b p2", p2, vp);
#endif
		return (p1 & p2);
	}
	return (p1);
}

static int
exp2c(wchar_t ***vp, bool ignore)
{
	register wchar_t *p1 = exp3(vp, ignore);
	register wchar_t *p2;
	register int i;

#ifdef TRACE
	tprintf("TRACE- exp2c()\n");
#endif
#ifdef EDEBUG
	etracc("exp2c p1", p1, vp);
#endif
	if (i = isa(**vp, EQOP)) {
		(*vp)++;
		if (i == EQMATCH || i == NOTEQMATCH)
			ignore |= NOGLOB;
		p2 = exp3(vp, ignore);
#ifdef EDEBUG
		etracc("exp2c p2", p2, vp);
#endif
		if (!(ignore&IGNORE)) switch (i) {

		case EQEQ:
			i = eq(p1, p2);
			break;

		case NOTEQ:
			i = !eq(p1, p2);
			break;

		case EQMATCH:
			i = Gmatch(p1, p2);
			break;

		case NOTEQMATCH:
			i = !Gmatch(p1, p2);
			break;
		}
		xfree(p1), xfree(p2);
		return (i);
	}
	i = egetn(p1);
	xfree(p1);
	return (i);
}

static wchar_t *
exp3(wchar_t ***vp, bool ignore)
{
	register wchar_t *p1, *p2;
	register int i;

#ifdef TRACE
	tprintf("TRACE- exp3()\n");
#endif
	p1 = exp3a(vp, ignore);
#ifdef EDEBUG
	etracc("exp3 p1", p1, vp);
#endif
	if (i = isa(**vp, RELOP)) {
		(*vp)++;
		if (**vp && eq(**vp, S_EQ /*"=" */))
			i |= 1, (*vp)++;
		p2 = exp3(vp, ignore);
#ifdef EDEBUG
		etracc("exp3 p2", p2, vp);
#endif
		if (!(ignore&IGNORE)) switch (i) {

		case GTR:
			i = egetn(p1) > egetn(p2);
			break;

		case GTR|1:
			i = egetn(p1) >= egetn(p2);
			break;

		case LSS:
			i = egetn(p1) < egetn(p2);
			break;

		case LSS|1:
			i = egetn(p1) <= egetn(p2);
			break;
		}
		xfree(p1), xfree(p2);
		return (putn(i));
	}
	return (p1);
}

static wchar_t *
exp3a(wchar_t ***vp, bool ignore)
{
	register wchar_t *p1, *p2, *op;
	register int i;

#ifdef TRACE
	tprintf("TRACE- exp3a()\n");
#endif
	p1 = exp4(vp, ignore);
#ifdef EDEBUG
	etracc("exp3a p1", p1, vp);
#endif
	op = **vp;
	if (op && (op[0] == '<' || op[0] == '>') && op[0] == op[1]) {
		(*vp)++;
		p2 = exp3a(vp, ignore);
#ifdef EDEBUG
		etracc("exp3a p2", p2, vp);
#endif
		if (op[0] == '<')
			i = egetn(p1) << egetn(p2);
		else
			i = egetn(p1) >> egetn(p2);
		xfree(p1), xfree(p2);
		return (putn(i));
	}
	return (p1);
}

static wchar_t *
exp4(wchar_t ***vp, bool ignore)
{
	register wchar_t *p1, *p2;
	register int i = 0;

#ifdef TRACE
	tprintf("TRACE- exp4()\n");
#endif
	p1 = exp5(vp, ignore);
#ifdef EDEBUG
	etracc("exp4 p1", p1, vp);
#endif
	if (isa(**vp, ADDOP)) {
		register wchar_t *op = *(*vp)++;

		p2 = exp4(vp, ignore);
#ifdef EDEBUG
		etracc("exp4 p2", p2, vp);
#endif
		if( !(ignore & IGNORE))
		switch (op[0]) {
		case '+':
			i = egetn(p1) + egetn(p2);
			break;

		case '-':
			i = egetn(p1) - egetn(p2);
			break;
		}
		xfree(p1), xfree(p2);
		return (putn(i));
	}
	return (p1);
}

static wchar_t *
exp5(wchar_t ***vp, bool ignore)
{
	register wchar_t *p1, *p2;
	register int i = 0;

#ifdef TRACE
	tprintf("TRACE- exp5()\n");
#endif
	p1 = exp6(vp, ignore);
#ifdef EDEBUG
	etracc("exp5 p1", p1, vp);
#endif
	if (isa(**vp, MULOP)) {
		register wchar_t *op = *(*vp)++;

		p2 = exp5(vp, ignore);
#ifdef EDEBUG
		etracc("exp5 p2", p2, vp);
#endif
		if (!(ignore&IGNORE)) switch (op[0]) {

		case '*':
			i = egetn(p1) * egetn(p2);
			break;

		case '/':
			i = egetn(p2);
			if (i == 0)
				err_divby0();
			i = egetn(p1) / i;
			break;

		case '%':
			i = egetn(p2);
			if (i == 0)
				err_modby0();
			i = egetn(p1) % i;
			break;
		}
		xfree(p1), xfree(p2);
		return (putn(i));
	}
	return (p1);
}

static wchar_t *
exp6(wchar_t ***vp, bool ignore)
{
	int ccode, i;
	register wchar_t *cp, *dp, *ep;

#ifdef TRACE
	tprintf("TRACE- exp6()\n");
#endif
	if (**vp == 0)
		err_experr();
	if (eq(**vp, S_EXAS /* "!" */)) {
		(*vp)++;
		cp = exp6(vp, ignore);
#ifdef EDEBUG
		etracc("exp6 ! cp", cp, vp);
#endif
		i = egetn(cp);
		xfree(cp);
		return (putn(!i));
	}
	if (eq(**vp, S_TIL /*"~" */)) {
		(*vp)++;
		cp = exp6(vp, ignore);
#ifdef EDEBUG
		etracc("exp6 ~ cp", cp, vp);
#endif
		i = egetn(cp);
		xfree(cp);
		return (putn(~i));
	}
	if (eq(**vp, S_LPAR /*"(" */)) {
		(*vp)++;
		ccode = exp0(vp, ignore);
#ifdef EDEBUG
		etraci("exp6 () ccode", ccode, vp);
#endif
		if (*vp == 0 || **vp == 0 || ***vp != ')')
			err_experr();
		(*vp)++;
		return (putn(ccode));
	}
	if (eq(**vp, S_LBRA)) {
		register wchar_t **v;
		struct command faket;
		wchar_t *fakecom[2];

		faket.t_dtyp = TCOM;
		faket.t_dflg = 0;
		faket.t_dcar = faket.t_dcdr = faket.t_dspr = (struct command *)0;
		faket.t_dcom = fakecom;
		fakecom[0] = S_BRAPPPBRA /*"{ ... }" */;
		fakecom[1] = NOSTR;
		(*vp)++;
		v = *vp;
		for (;;) {
			if (!**vp)
				err_missing('}');
			if (eq(*(*vp)++, S_RBRA /*"}" */))
				break;
		}
		if (ignore&IGNORE)
			return (S_ /*""*/);
		psavejob();
		if (pfork(&faket, -1) == 0) {
			*--(*vp) = 0;
			evalav(v);
			exitstat();
		}
		pwait();
		prestjob();
#ifdef EDEBUG
		etraci("exp6 {} status", egetn(value("status")), vp);
#endif
		return (putn(egetn(value(S_status /*"status" */)) == 0));
	}
	if (isa(**vp, ANYOP))
		return (S_ /*""*/);
	cp = *(*vp)++;
	if (*cp == '-' && any(cp[1], S_erwxfdzocbpugkstl /*"erwxfdzo" */)) {
		struct stat stb;
		int (*statfunc)() = stat_;

		if (cp[2] != '\0')
			bferr(gettxt(_SGI_DMMX_csh_mfinqu, "Malformed file inquiry"));

		/*
		 * Detect missing file names by checking for operator
		 * in the file name position.  However, if an operator
		 * name appears there, we must make sure that there's
		 * no file by that name (e.g., "/") before announcing
		 * an error.  Even this check isn't quite right, since
		 * it doesn't take globbing into account.
		 *
		 * If however we do glob it, there are problems with filenames
		 * like the char '?' and '*'. We would have to check if
		 * globbed files exist (if they are unambiguous) or if
		 * files with these meta-character names exist. Without
		 * taking care of globbing we are compatible with sys V and
		 * sun.
		 */
		if (isa(**vp, ANYOP) && statfunc(**vp, &stb))
			bferr(gettxt(_SGI_DMMX_csh_missfilnam,
			    "Missing file name"));
		dp = *(*vp)++;

		if (ignore&IGNORE)
			return (S_ /*""*/);
		ep = globone(dp);
		switch (cp[1]) {

		case 'r':
			i = !access_(ep, R_OK);
			break;

		case 'w':
			i = !access_(ep, W_OK);
			break;

		case 'x':
			i = !access_(ep, X_OK);
			break;

		case 't':
			i = isatty(egetn(ep));
			break;

		default:
			if (cp[1] == 'l')
				statfunc = lstat_;
			if (statfunc(ep, &stb)) {
				xfree(ep);
				return (S_0 /*"0"*/);
			}
			switch (cp[1]) {

			case 'f':
				i = (stb.st_mode & S_IFMT) == S_IFREG;
				break;

			case 'd':
				i = (stb.st_mode & S_IFMT) == S_IFDIR;
				break;

			case 'z':
				i = stb.st_size == 0;
				break;

			case 'e':
				i = 1;
				break;

			case 'o':
				i = stb.st_uid == uid;
				break;

			case 'l':
				i = (stb.st_mode & S_IFMT) == S_IFLNK;
				break;

			case 'c':
				i = (stb.st_mode & S_IFMT) == S_IFCHR;
				break;

			case 'b':
				i = (stb.st_mode & S_IFMT) == S_IFBLK;
				break;

			case 'p':
				i = (stb.st_mode & S_IFMT) == S_IFIFO;
				break;

			case 'u':
				i = (int)(stb.st_mode & S_ISUID);
				break;

			case 'g':
				i = (int)(stb.st_mode & S_ISGID);
				break;

			case 'k':
				i = (int)(stb.st_mode & S_ISVTX);
				break;

			case 's':
				i = stb.st_size != 0;
				break;
			}
		}
#ifdef EDEBUG
		etraci("exp6 -? i", i, vp);
#endif
		xfree(ep);
		return (putn(i));
	}
#ifdef EDEBUG
	etracc("exp6 default", cp, vp);
#endif
	return (ignore&NOGLOB ? savestr(cp) : globone(cp));
}

static void
evalav(wchar_t **v)
{
	register struct wordent *hp;
	register struct wordent *wdp;
	register struct wordent *new;
	struct command *t;
	struct wordent paraml;
	
#ifdef TRACE
	tprintf("TRACE- evalav()\n");
#endif
	wdp = hp = &paraml;
	set(S_status, S_0);
	hp->prev = hp->next = hp;
	hp->word = S_;
	while(*v) {
	    new = (struct wordent *)salloc(1, sizeof(struct wordent));
	    new->prev = wdp;
	    new->next = hp;
	    wdp->next = new;
	    wdp = new;
	    wdp->word = savestr(*v++);
	}
	hp->prev = wdp;
	alias(&paraml);
	t = syntax(paraml.next, &paraml, 0);
	if(errstr)
	    error_errstr();
	execute(t, -1);
	freelex(&paraml), freesyn(t);
}

static int
isa(wchar_t *cp, int what)
{

#ifdef TRACE
	tprintf("TRACE- isa()\n");
#endif
	if (cp == 0)
		return ((what & RESTOP) != 0);
	if (cp[1] == 0) {
		if (what & ADDOP && (*cp == '+' || *cp == '-'))
			return (1);
		if (what & MULOP && (*cp == '*' || *cp == '/' || *cp == '%'))
			return (1);
		if (what & RESTOP && (*cp == '(' || *cp == ')' || *cp == '!' ||
				      *cp == '~' || *cp == '^' || *cp == '"'))
			return (1);
	} else if (cp[2] == 0) {
		if (what & RESTOP) {
			if (cp[0] == '|' && cp[1] == '&')
				return (1);
			if (cp[0] == '<' && cp[1] == '<')
				return (1);
			if (cp[0] == '>' && cp[1] == '>')
				return (1);
		}
		if (what & EQOP) {
			if (cp[0] == '=') {
				if (cp[1] == '=')
					return (EQEQ);
				if (cp[1] == '~')
					return (EQMATCH);
			} else if (cp[0] == '!') {
				if (cp[1] == '=')
					return (NOTEQ);
				if (cp[1] == '~')
					return (NOTEQMATCH);
			}
		}
	}
	if (what & RELOP) {
		if (*cp == '<')
			return (LSS);
		if (*cp == '>')
			return (GTR);
	}
	return (0);
}

static int
egetn(wchar_t *cp)
{
#ifdef TRACE
	tprintf("TRACE- egetn()\n");
#endif
	if(*cp && (*cp != '-') && !digit(*cp))
	    err_experr();
	return(getn(cp));
}

/* Phew! */

#ifdef EDEBUG
static void
etraci(wchar_t *str, int i, wchar_t ***vp)
{
	shprintf("%s=%d\t", str, i);	/*XXXX wrooooong */
	blkpr(*vp);
	shprintf("\n");
}

static void
etracc(wchar_t *str, wchar_t *cp, wchar_t ***vp)
{
	shprintf("%s=%s\t", str, cp);	/*XXXX wrooooong */
	blkpr(*vp);
	shprintf("\n");
}
#endif
