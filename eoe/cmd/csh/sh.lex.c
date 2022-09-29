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

#include <sys/ioctl.h>
#include <unistd.h>
#include <pfmt.h>
#include "sh.h"
#include "sh.proc.h"
#include "sh.wconst.h"

/*
 * C shell
 */

/*
 * These lexical routines read input and form lists of words.
 * There is some involved processing here, because of the complications
 * of input buffering, and especially because of history substitution.
 */

/*
 * Peekc is a peek characer for getC, peekread for readc.
 * There is a subtlety here in many places... history routines
 * will read ahead and then insert stuff into the input stream.
 * If they push back a character then they must push it behind
 * the text substituted by the history substitution.  On the other
 * hand in several places we need 2 peek characters.  To make this
 * all work, the history routines read with getC, and make use both
 * of ungetC and unreadc.  The key observation is that the state
 * of getC at the call of a history reference is such that calls
 * to getC from the history routines will always yield calls of
 * readc, unless this peeking is involved.  That is to say that during
 * getexcl the variables lap, exclp, and exclnxt are all zero.
 *
 * Getdol invokes history substitution, hence the extra peek, peekd,
 * which it can ungetD to be before history substitutions.
 */
wchar_t peekc, peekd;
wchar_t peekread;

wchar_t *exclp;			/* (Tail of) current word from ! subst */
struct	wordent *exclnxt;	/* The rest of the ! subst words */
int	exclc;			/* Count of remainig words in ! subst */
wchar_t *alvecp;		/* "Globp" for alias resubstitution */

/*
 * Lex returns to its caller not only a wordlist (as a "var" parameter)
 * but also whether a history substitution occurred.  This is used in
 * the main (process) routine to determine whether to echo, and also
 * when called by the alias routine to determine whether to keep the
 * argument list.
 */
bool	hadhist;

wchar_t getCtmp;

#define getC(f)		((getCtmp = peekc) ? (peekc = 0, getCtmp) : getC1(f))
#define	ungetC(c)	(peekc = (c))
#define	ungetD(c)	(peekd = (c))

static void		bfree(void);
static wchar_t		bgetc(void);
static struct wordent	*dosub(wchar_t, struct wordent *, bool);
static struct Hist	*findev(wchar_t *, bool);
static wchar_t		getC1(int);
static void		getdol(void);
static void		getexcl(wchar_t);
static struct wordent	*gethent(wchar_t);
static int		getsel(int *, int *, int);
static struct wordent	*getsub(struct wordent *);
static int		matchs(wchar_t *, wchar_t *);
static void		noev(wchar_t *);
static void		setexclp(wchar_t *);
static wchar_t		*subword(wchar_t *, wchar_t, bool *);
static wchar_t		*word(void);

int
lex(struct wordent *hp)
{
	register struct wordent *wdp;
	register struct wordent *new;
	register wchar_t c;

#ifdef TRACE
	tprintf("TRACE- lex()\n");
#endif
	lineloc = btell();
	hp->next = hp->prev = hp;
	hp->word = S_;
	alvecp = 0, hadhist = 0;
	do {
	    c = readc(0);
	} while(issp(c));
	if(c == HISTSUB && intty)
	    getexcl(c);				/* ^lef^rit */
	else
	    unreadc(c);
	wdp = hp;
	/*
	 * The following loop is written so that the links needed
	 * by freelex will be ready and rarin to go even if it is
	 * interrupted.
	 */
	do {
	    new = (struct wordent *)salloc(1, sizeof(struct wordent));
	    new->word = 0;
	    new->prev = wdp;
	    new->next = hp;
	    wdp->next = new;
	    wdp = new;
	    wdp->word = word();
	} while(wdp->word[0] != '\n');
#ifdef TRACE
	tprintf("Exiting lex()\n");
#endif
	hp->prev = wdp;
	return(hadhist);
}

void
prlex(struct wordent *sp0)
{
	register struct wordent *sp = sp0->next;

#ifdef TRACE
	tprintf("TRACE- prlex()\n");
#endif
	for(;;) {
	    shprintf("%t", sp->word);
	    sp = sp->next;
	    if(sp == sp0)
		break;
	    if(sp->word[0] != '\n')
		wputchar(' ');
	}
}

void
copylex(struct wordent *hp, struct wordent *fp)
{
	register struct wordent *wdp;
	register struct wordent *new;

#ifdef TRACE
	tprintf("TRACE- copylex()\n");
#endif
	wdp = hp;
	fp = fp->next;
	do {
	    new = (struct wordent *)salloc(1, sizeof(struct wordent));
	    new->prev = wdp;
	    new->next = hp;
	    wdp->next = new;
	    wdp = new;
	    wdp->word = savestr(fp->word);
	    fp = fp->next;
	} while(wdp->word[0] != '\n');
	hp->prev = wdp;
}

void
freelex(struct wordent *vp)
{
	register struct wordent *fp;

#ifdef TRACE
	tprintf("TRACE- freelex()\n");
#endif
	while((fp = vp->next) != vp) {
	    vp->next = fp->next;
	    XFREE(fp->word)
	    XFREE( (char *)fp)
	}
	vp->prev = vp;
}

static wchar_t *
word(void)
{
	register wchar_t c, c1;
	register wchar_t *wp;
	register int i;
	register bool dolflg;
	wchar_t wbuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- word()\n");
#endif
	wp = wbuf;
	i = CSHBUFSIZ - 4;
loop:
	do {
	    c = getC(DOALL);			/* issp is a macro */
	} while(issp(c));
	if(cmlook(c, _META | _ESC)) {
	    switch(c) {
		case '&':
		case '|':
		case '<':
		case '>':
		    *wp++ = c;
		    c1 = getC(DOALL);
		    if(c1 == c)
			*wp++ = c1;
		    else
			ungetC(c1);
		    goto ret;
		case '#':
		    if(intty)
			break;
		    c = 0;
		    do {
			c1 = c;
			c = getC(0);
		    } while(c != '\n');
		    if(c1 == '\\')
			goto loop;
		case ';':
		case '(':
		case ')':
		case '\n':
		    *wp++ = c;
		    goto ret;
		case '\\':
		    c = getC(0);
		    if(c == '\n') {
			if(onelflg == 1)
			    onelflg = 2;
			goto loop;
		    }
		    if(c != HIST)
			*wp++ = '\\', --i;
		    c |= QUOTE;
		}
	}
	c1 = 0;
	dolflg = DOALL;
	for(;;) {
	    if(c1) {
		if(c == c1) {
		    c1 = 0;
		    dolflg = DOALL;
		} else if (c == '\\') {
		    c = getC(0);
		    if(c == HIST)
			c |= QUOTE;
		    else {
			if(c == '\n')
				/*
				if (c1 == '`')
					c = ' ';
				else
				*/
			    c |= QUOTE;
			ungetC(c);
			c = '\\';
		    }
		} else if (c == '\n') {
		    ungetC(c);
		    seterrc(gettxt(_SGI_DMMX_csh_unmatched, "Unmatched %s"),
			c1);
		    break;
		}
	    } else if(( !(c & QUOTE) && cmlook(c, _META | _Q | _Q1 | _ESC))
			|| isauxsp(c)) {
		if(c == '\\') {
		    c = getC(0);
		    if(c == '\n') {
			if(onelflg == 1)
			    onelflg = 2;
			break;
		    }
		    if(c != HIST)
			*wp++ = '\\', --i;
		    c |= QUOTE;
		} else if(cmlook(c, _Q | _Q1)) {	/* '"` */
		    c1 = c;
		    dolflg = (c == '"')? DOALL : DOEXCL;
		} else if(c != '#' || !intty) {
		    ungetC(c);
		    break;
		}
	    }
	    if(--i > 0) {
		*wp++ = c;
		c = getC(dolflg);
	    } else {
		seterr(gettxt(_SGI_DMMX_csh_word2long, "Word too long"));
		wp = &wbuf[1];
		break;
	    }
	}
ret:
	*wp = 0;
#ifdef TRACE
	tprintf("word() returning:%t\n", wbuf);
#endif
	return(savestr(wbuf));
}

static wchar_t
getC1(int flag)
{
	register wchar_t c;

top:
	if(c = peekc) {
	    peekc = 0;
	    return(c);
	}
	if(lap) {
	    if( !(c = *lap++))
		lap = 0;
	    else {
		if(cmlook(c, _META | _Q | _Q1) || isauxsp(c))
		    c |= QUOTE;
		return(c);
	    }
	}
	if(c = peekd) {
	    peekd = 0;
	    return(c);
	}
	if(exclp) {
	    if(c = *exclp++)
		return(c);
	    if(exclnxt && --exclc >= 0) {
		exclnxt = exclnxt->next;
		setexclp(exclnxt->word);
		return(' ');
	    }
	    exclp = 0;
	    exclnxt = 0;
	}
	if(exclnxt) {
	    exclnxt = exclnxt->next;
	    if(--exclc < 0)
		exclnxt = 0;
	    else
		setexclp(exclnxt->word);
	    goto top;
	}
	c = readc(0);
	if(c == '$' && (flag & DODOL)) {
	    getdol();
	    goto top;
	}
	if(c == HIST && (flag & DOEXCL)) {
	    getexcl(0);
	    goto top;
	}
	return(c);
}

/*
 * 'name' buffer size is
 *
 * ${?XXXXXXXXXXXXXXXXX[ZZZZZZZZZZZZZZZZ]:gY}\0
 *    <   MAXVARLEN   > <  MAXVARLEN   >   ^
 *                                         |
 *                                       htrqxe
 */
static void
getdol(void)
{
	register wchar_t *np, *ep;
	register wchar_t c, sc;
	bool special = 0;
	wchar_t name[MAXVARLEN*2 + 12];

#ifdef TRACE
	tprintf("TRACE- getdol()\n");
#endif
	np = name, *np++ = '$';
	c = sc = getC(DOEXCL);
	if(isspnl(c)) {
	    ungetD(c);
	    ungetC('$' | QUOTE);
	    return;
	}
	if(c == '{')
	    *np++ = c, c = getC(DOEXCL);
	if(c == '#' || c == '?')
	    special++, *np++ = c, c = getC(DOEXCL);
	*np++ = c;

	switch(c) {
	    case '<':
	    case '$':
	    case '*':
		if(special)
		    goto vsyn;
		goto ret;
	    case '\n':
		ungetD(c);
		np--;
		goto vsyn;
	    default:
		ep = &name[MAXVARLEN + 4];
		if(digit(c)) {			/* let $?0 pass for now */
		    while(digit(c = getC(DOEXCL))) {
			if(np < ep)
			    *np++ = c;
		    }
		} else if(letter(c)) {
		    while(letter(c = getC(DOEXCL)) || digit(c)) {
			if(np < ep)
			    *np++ = c;
		    }
		} else
		    goto vsyn;
	}
	if(c == '[') {
	    *np++ = c;
	    ep = &name[MAXVARLEN*2 + 4];
	    do {
		c = getC(DOEXCL);
		if(c == '\n') {
		    ungetD(c);
		    np--;
		    goto vsyn;
		}
		if(np >= ep)
		    goto vsyn;
		*np++ = c;
	    } while(c != ']');
	    c = getC(DOEXCL);
	}
	if(c == ':') {
	    *np++ = c, c = getC(DOEXCL);
	    if(c == 'g')
		*np++ = c, c = getC(DOEXCL);
	    *np++ = c;
	    if( !any(c, S_htrqxe))
		goto vsyn;
	} else
	    ungetD(c);
	if(sc == '{') {
	    c = getC(DOEXCL);
	    if(c != '}') {
		ungetC(c);
		goto vsyn;
	    }
	    *np++ = c;
	}
ret:
	*np = 0;
	addla(name);
	return;

vsyn:
	seterr(gettxt(_SGI_DMMX_csh_varsyntax, "Variable syntax"));
	goto ret;
}

void
addla(wchar_t *cp)
{
	register wchar_t *p;
	wchar_t buf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- addla()\n");
#endif
	if(lap) {
	    if((wslen(cp) + wslen(lap)) >= (CSHBUFSIZ - 4)) {
		seterr(gettxt(_SGI_DMMX_csh_expfull,
		    "Expansion buffer overflow"));
		return;
	    }
	    wscpy(buf, lap);
	}
	p = wscpyend(labuf, cp);
	if(lap)
	    wscpy(p, buf);
	lap = labuf;
}

wchar_t	lhsb[32];
wchar_t	slhs[32];
wchar_t	rhsb[64];
int	quesarg;

static void
getexcl(wchar_t sc)
{
	register struct wordent *hp, *ip;
	register wchar_t c;
	int left, right, dol;

#ifdef TRACE
	tprintf("TRACE- getexcl()\n");
#endif
	if( !sc) {
	    sc = getC(0);
	    if(sc != '{') {
		ungetC(sc);
		sc = 0;
	    }
	}
	quesarg = -1;
	lastev = eventno;
	hp = gethent(sc);
	if( !hp)
	    return;
	hadhist = 1;
	dol = 0;
	if(hp == alhistp) {
	    for(ip = hp->next->next; ip != alhistt; ip = ip->next)
		dol++;
	} else {
	    for(ip = hp->next->next; ip != hp->prev; ip = ip->next)
		dol++;
	}
	left = 0, right = dol;
	if(sc == HISTSUB) {
	    ungetC('s'), unreadc(HISTSUB), c = ':';
	    goto subst;
	}
	c = getC(0);
	if (! (c == ':' || c == '^' || c == '$' || c == '*' ||
	       c == '-' || c == '%' ))
		goto subst;
	left = right = -1;
	if(c == ':') {
	    c = getC(0);
	    unreadc(c);
	    if(letter(c) || c == '&') {
		c = ':';
		left = 0, right = dol;
		goto subst;
	    }
	} else
	    ungetC(c);
	if( !getsel(&left, &right, dol))
	    return;
	c = getC(0);
	if(c == '*')
	    ungetC(c), c = '-';
	if(c == '-') {
	    if( !getsel(&left, &right, dol))
		return;
	    c = getC(0);
	}
subst:
	exclc = right - left + 1;
	while (--left >= 0)
		hp = hp->next;
	if (sc == HISTSUB || c == ':') {
		do {
			hp = getsub(hp);
			c = getC(0);
		} while (c == ':');
	}
	unreadc(c);
	if (sc == '{') {
		c = getC(0);
		if (c != '}')
			seterr(gettxt(_SGI_DMMX_csh_badbang, "Bad ! form"));
	}
	exclnxt = hp;
}

static struct wordent *
getsub(struct wordent *en)
{
	register wchar_t *cp;
	register wchar_t c;
	register wchar_t sc;
	register wchar_t delim;
	bool global = 0;
	wchar_t orhsb[(sizeof rhsb)/(sizeof rhsb[0])];

#ifdef TRACE
	tprintf("TRACE- getsub()\n");
#endif
	exclnxt = 0;
	sc = c = getC(0);
	if (c == 'g')
		global++, c = getC(0);
	switch (c) {

	case 'p':
		justpr++;
		goto ret;

	case 'x':
	case 'q':
		global++;
		/* fall into ... */

	case 'h':
	case 'r':
	case 't':
	case 'e':
		break;

	case '&':
		if (slhs[0] == 0) {
			seterr(gettxt(_SGI_DMMX_csh_noprevs, "No prev sub"));
			goto ret;
		}
		wscpy(lhsb, slhs);
		break;

/*
	case '~':
		if (lhsb[0] == 0)
			goto badlhs;
		break;
*/

	case 's':
		delim = getC(0);
		if (alnum(delim) || isspnl(delim)){
			unreadc(delim);
bads:
			lhsb[0] = 0;
			seterr(gettxt(_SGI_DMMX_csh_badsub, "Bad substitute"));
			goto ret;
		}
		cp = lhsb;
		for (;;) {
			c = getC(0);
			if (c == '\n') {
				unreadc(c);
				break;
			}
			if (c == delim)
				break;
			if (cp > &lhsb[(sizeof lhsb)/(sizeof lhsb[0]) - 2])
				goto bads;
			if (c == '\\') {
				c = getC(0);
				if (c != delim && c != '\\')
					*cp++ = '\\';
			}
			*cp++ = c;
		}
		if (cp != lhsb)
			*cp++ = 0;
		else if (lhsb[0] == 0) {
/*badlhs:*/
			seterr(gettxt(_SGI_DMMX_csh_noplhs, "No prev lhs"));
			goto ret;
		}
		cp = rhsb;
		wscpy(orhsb, cp);
		for (;;) {
			c = getC(0);
			if (c == '\n') {
				unreadc(c);
				break;
			}
			if (c == delim)
				break;
/*
			if (c == '~') {
				if (&cp[wslen(orhsb)] 
				> &rhsb[(sizeof rhsb)/(sizeof rhsb[0]) - 2])
					goto toorhs;
				wscpy(cp, orhsb);
				cp = strend(cp);
				continue;
			}
*/
			if (cp > &rhsb[(sizeof rhsb)/(sizeof rhsb[0]) - 2]) {
/*toorhs:*/
				seterr(gettxt(_SGI_DMMX_csh_rhs2long,
				    "Rhs too long"));
				goto ret;
			}
			if (c == '\\') {
				c = getC(0);
				if (c != delim /* && c != '~' */)
					*cp++ = '\\';
			}
			*cp++ = c;
		}
		*cp++ = 0;
		break;

	default:
		if (c == '\n')
			unreadc(c);
		seterrc(gettxt(_SGI_DMMX_csh_badbangmod,
		    "%s: Bad ! modifier"), c);
		goto ret;
	}
	wscpy(slhs, lhsb);
	if (exclc)
		en = dosub(sc, en, global);
ret:
	return (en);
}

static struct wordent *
dosub(wchar_t sc, struct wordent *en, bool global)
{
	register struct wordent *wdp;
	register int i = exclc;
	register struct wordent *new;
	struct wordent *hp;
	bool didsub = 0;
	struct wordent lex;

#ifdef TRACE
	tprintf("TRACE- dosub()\n");
#endif
	hp = &lex;
	wdp = hp;
	while (--i >= 0) {
	    new = (struct wordent *)salloc(1, sizeof(struct wordent));
	    new->prev = wdp;
	    new->next = hp;
	    wdp->next = new;
	    wdp = new;
	    en = en->next;
	    wdp->word = (global || !didsub)?
		    subword(en->word, sc, &didsub) : savestr(en->word);
	}
	if( !didsub)
	    seterr(gettxt(_SGI_DMMX_csh_modfailed, "Modifier failed"));
	hp->prev = wdp;
	return(&enthist(-1000, &lex, 0)->Hlex);
}

static wchar_t *
subword(wchar_t *cp, wchar_t type, bool *adid)
{
	wchar_t wbuf[CSHBUFSIZ];
	register wchar_t *wp, *mp, *np;
	register int i;

#ifdef TRACE
	tprintf("TRACE- subword()\n");
#endif
	switch (type) {

	case 'r':
	case 'e':
	case 'h':
	case 't':
	case 'q':
	case 'x':
		wp = domod(cp, type);
		if (wp == 0)
			return (savestr(cp));
		*adid = 1;
		return (wp);

	default:
		wp = wbuf;
		i = CSHBUFSIZ - 4;
		for (mp = cp; *mp; mp++)
			if (matchs(mp, lhsb)) {
				for (np = cp; np < mp;)
					*wp++ = *np++, --i;
				for (np = rhsb; *np; np++) switch (*np) {

				case '\\':
					if (np[1] == '&')
						np++;
					/* fall into ... */

				default:
					if (--i < 0)
						goto ovflo;
					*wp++ = *np;
					continue;

				case '&':
					i -= wslen(lhsb);
					if (i < 0)
						goto ovflo;
					*wp = 0;
					wscat(wp, lhsb);
					wp = strend(wp);
					continue;
				}
				mp += wslen(lhsb);
				i -= wslen(mp);
				if (i < 0) {
ovflo:
					seterr(gettxt(_SGI_DMMX_csh_sbufov,
					    "Substitution buffer overflow"));
					return(S_);
				}
				*wp = 0;
				wscat(wp, mp);
				*adid = 1;
				return (savestr(wbuf));
			}
		return (savestr(cp));
	}
}

wchar_t *
domod(wchar_t *cp, wchar_t type)
{
	register wchar_t *wp, *xp;
	register wchar_t c;

#ifdef TRACE
	tprintf("TRACE- domod()\n");
#endif
	switch (type) {

	case 'x':
	case 'q':
		wp = savestr(cp);
		for (xp = wp; c = *xp; xp++)
			if (!issp(c) || type == 'q')
				*xp |= QUOTE;
		return (wp);

	case 'h':
	case 't':
		if (!any('/', cp))
			return (type == 't' ? savestr(cp) : 0);
		wp = strend(cp);
		while (*--wp != '/')
			continue;
		if (type == 'h')
			xp = savestr(cp), xp[wp - cp] = 0;
		else
			xp = savestr(wp + 1);
		return (xp);

	case 'e':
	case 'r':
		wp = strend(cp);
		for (wp--; wp >= cp && *wp != '/'; wp--)
			if (*wp == '.') {
				if (type == 'e')
					xp = savestr(wp + 1);
				else
					xp = savestr(cp), xp[wp - cp] = 0;
				return (xp);
			}
		return (savestr(type == 'e' ? S_ /*""*/ : cp));
	}
	return (0);
}

static int
matchs(wchar_t *str, wchar_t *pat)
{
#ifdef TRACE
	tprintf("TRACE- matchs()\n");
#endif
	while(*str && *pat && *str == *pat)
	    str++, pat++;
	return(*pat == 0);
}

static int
getsel(int *al, int *ar, int dol)
{
	register wchar_t c = getC(0);
	register int i;
	bool first = *al < 0;

#ifdef TRACE
	tprintf("TRACE- getsel()\n");
#endif
	switch (c) {

	case '%':
		if (quesarg == -1)
			goto bad;
		if (*al < 0)
			*al = quesarg;
		*ar = quesarg;
		break;

	case '-':
		if (*al < 0) {
			*al = 0;
			*ar = dol - 1;
			unreadc(c);
		}
		return (1);

	case '^':
		if (*al < 0)
			*al = 1;
		*ar = 1;
		break;

	case '$':
		if (*al < 0)
			*al = dol;
		*ar = dol;
		break;

	case '*':
		if (*al < 0)
			*al = 1;
		*ar = dol;
		if (*ar < *al) {
			*ar = 0;
			*al = 1;
			return (1);
		}
		break;

	default:
		if (digit(c)) {
			i = 0;
			while (digit(c)) {
				i = i * 10 + c - '0';
				c = getC(0);
			}
			if (i < 0)
				i = dol + 1;
			if (*al < 0)
				*al = i;
			*ar = i;
		} else
			if (*al < 0)
				*al = 0, *ar = dol;
			else
				*ar = dol - 1;
		unreadc(c);
		break;
	}
	if (first) {
		c = getC(0);
		unreadc(c);
		/* if (any(c, "-$*")) */	/* char -> wchar_t */
		if (c == '-' || c == '$' || c == '*')
			return (1);
	}
	if (*al > *ar || *ar > dol) {
bad:
		seterr(gettxt(_SGI_DMMX_csh_badbangsel, "Bad ! arg selector"));
		return (0);
	}
	return (1);

}

static struct wordent *
gethent(wchar_t sc)
{
	register struct Hist *hp;
	register wchar_t *np;
	register wchar_t c;
	int event;
	bool back = 0;

#ifdef TRACE
	tprintf("TRACE- gethent()\n");
#endif
	c = (sc == HISTSUB)? HIST : getC(0);
	if (c == HIST) {
		if (alhistp)
			return (alhistp);
		event = eventno;
		goto skip;
	}
	switch (c) {

	case ':':
	case '^':
	case '$':
	case '*':
	case '%':
		ungetC(c);
		if (lastev == eventno && alhistp)
			return (alhistp);
		event = lastev;
		break;

	case '-':
		back = 1;
		c = getC(0);
		goto number;

	case '#':			/* !# is command being typed in (mrh) */
		return(&paraml);

	default:
		/*if (any(c, "(=~")) {*/
		if (c == '(' || c == '=' || c == '~') {
			unreadc(c);
			ungetC(HIST);
			return (0);
		}
		if (digit(c))
			goto number;
		np = lhsb;
		/* while (!any(c, ": \t\\\n}")) { */
		while (! (c == ':' || c == '\\' || isspnl(c) || c == '}')) {
			if (np < &lhsb[(sizeof lhsb)/(sizeof lhsb[0]) - 2])
				*np++ = c;
			c = getC(0);
		}
		unreadc(c);
		if (np == lhsb) {
			ungetC(HIST);
			return (0);
		}
		*np++ = 0;
		hp = findev(lhsb, 0);
		if (hp)
			lastev = hp->Hnum;
		return (&hp->Hlex);

	case '?':
		np = lhsb;
		for (;;) {
			c = getC(0);
			if (c == '\n') {
				unreadc(c);
				break;
			}
			if (c == '?')
				break;
			if (np < &lhsb[(sizeof lhsb)/(sizeof lhsb[0]) - 2])
				*np++ = c;
		}
		if (np == lhsb) {
			if (lhsb[0] == 0) {
				seterr(gettxt(_SGI_DMMX_csh_nopsrch,
				    "No prev search"));
				return (0);
			}
		} else
			*np++ = 0;
		hp = findev(lhsb, 1);
		if (hp)
			lastev = hp->Hnum;
		return (&hp->Hlex);

	number:
		event = 0;
		while (digit(c)) {
			event = event * 10 + c - '0';
			c = getC(0);
		}
		if (back)
			event = eventno + (alhistp == 0) - (event ? event : 0);
		unreadc(c);
		break;
	}
skip:
	for (hp = Histlist.Hnext; hp; hp = hp->Hnext)
		if (hp->Hnum == event) {
			hp->Href = eventno;
			lastev = hp->Hnum;
			return (&hp->Hlex);
		}
	np = putn(event);
	noev(np);
	return (0);
}

static struct Hist *
findev(wchar_t *cp, bool anyarg)
{
	register struct Hist *hp;

#ifdef TRACE
	tprintf("TRACE- findev(%t)\n", cp);
#endif
	for (hp = Histlist.Hnext; hp; hp = hp->Hnext) {
		register wchar_t *dp;
		register wchar_t *p, *q;
		register struct wordent *lp = hp->Hlex.next;
		int argno = 0;

		if (lp->word[0] == '\n')
			continue;
		if (!anyarg) {
			p = cp;
			q = lp->word;
			do
				if (!*p)
					return (hp);
			while (*p++ == *q++);
			continue;
		}
		do {
			for (dp = lp->word; *dp; dp++) {
				p = cp;
				q = dp;
				do
					if (!*p) {
						quesarg = argno;
						return (hp);
					}
				while (*p++ == *q++);
			}
			lp = lp->next;
			argno++;
		} while (lp->word[0] != '\n');
	}
	noev(cp);
	return (0);
}

static void
noev(wchar_t *cp)
{
#ifdef TRACE
	tprintf("TRACE- noev(%t)\n", cp);
#endif
	seterr2(cp, gettxt(_SGI_DMMX_csh_evtnotfnd, ": Event not found"));
}

static void
setexclp(wchar_t *cp)
{
#ifdef TRACE
	tprintf("TRACE- setexclp()\n");
#endif
	if (cp && cp[0] == '\n')
		return;
	exclp = cp;
}

void
unreadc(wchar_t c)
{
	peekread = c;
}

wchar_t
readc(bool wanteof)
{
	register wchar_t c;
	static sincereal;

	if(c = peekread) {
	    peekread = 0;
	    return(c);
	}
top:
	if (alvecp) {
		if (c = *alvecp++)
			return (c);
		if (*alvec) {
			alvecp = *alvec++;
			return (' ');
		}
	}
	if (alvec) {
		if (alvecp = *alvec) {
			alvec++;
			goto top;
		}
		/* Infinite source! */
		return ('\n');
	}
	if (evalp) {
		if (c = *evalp++)
			return (c);
		if (*evalvec) {
			evalp = *evalvec++;
			return (' ');
		}
		evalp = 0;
	}
	if (evalvec) {
		if (evalvec ==  (wchar_t **)1) {
			doneinp = 1;
			reset();
		}
		if (evalp = *evalvec) {
			evalvec++;
			goto top;
		}
		evalvec =  (wchar_t **)1;
		return ('\n');
	}
	do {
		if (arginp ==  (wchar_t *) 1 || onelflg == 1) {
			if (wanteof)
				return (-1);
			exitstat();
		}
		if (arginp) {
			if ((c = *arginp++) == 0) {
				arginp =  (wchar_t *) 1;
				return ('\n');
			}
			return (c);
		}
reread:
		c = bgetc();
		if (c < 0) {
			struct termio tty;

			if (wanteof)
				return (-1);
			if (ioctl(SHIN, TCGETA, (char *)&tty) == 0 &&
			    (tty.c_lflag & ICANON))
			   {
				/* was 'short' for FILEC */
				int ctpgrp;

				if (++sincereal > 25)
					goto oops;
				if (tpgrp != -1 &&
				    (ctpgrp = tcgetpgrp(FSHTTY)) != -1 &&
				    tpgrp != ctpgrp) {
					(void) tcsetpgrp(FSHTTY, tpgrp);
					(void) killpg(ctpgrp, SIGHUP);
shprintf("Reset tty pgrp from %d to %d\n", ctpgrp, tpgrp);
					goto reread;
				}
				if(adrof(S_ignoreeof)) {
				    shprintf("\n");
				    showstr(MM_INFO,
					loginsh?
					    gettxt(_SGI_DMMX_csh_uselogout,
					    "Use \"logout\" to logout")
					:
					    gettxt(_SGI_DMMX_csh_useexit,
					    "Use \"exit\" to leave csh"),
					0);
				    reset();
				}
				if (chkstop == 0) 
					panystop(1);
			}
oops:
			doneinp = 1;
			reset();
		}
		sincereal = 0;
		if (c == '\n' && onelflg)
			onelflg--;
	} while (c == 0);
	return (c);
}

static wchar_t
bgetc(void)
{
	register int buf, off;
	register wchar_t wc;
	register int x;
	register wchar_t **nfbuf;

	wchar_t ttyline[CSHBUFSIZ];
	register int numleft = 0, roomleft;

#ifdef TELL
	if(cantell) {
	    if(fseekp < fbobp || fseekp > feobp) {
		fbobp = feobp = fseekp;
		(void)lseek(SHIN, fseekp, SEEK_SET);
	    }
	    if(fseekp == feobp) {
		fbobp = feobp;
		do {
		    x = read_(SHIN, fbuf[0], CSHBUFSIZ);
		} while(x < 0 && errno == EINTR);
		if(x <= 0)
		    return(-1);
		feobp += x;
	    }
	    wc = fbuf[0][fseekp - fbobp];
	    fseekp++;
	    return(wc);
	}
#endif
again:
	buf = (int) fseekp / CSHBUFSIZ;
	if(buf >= fblocks) {
	    nfbuf = (wchar_t **)salloc(fblocks + 2, sizeof(wchar_t **));
	    if(fbuf) {
		(void)blkcpy(nfbuf, fbuf);
		xfree(fbuf);
	    }
	    fbuf = nfbuf;
	    fbuf[fblocks] = wcalloc(CSHBUFSIZ);
	    fblocks++;
	    goto again;
	}
	if(fseekp >= feobp) {
	    buf = (int) feobp / CSHBUFSIZ;
	    off = (int) feobp % CSHBUFSIZ;
	    roomleft = CSHBUFSIZ - off;
	    for(;;) {
		if(filec && intty) {
		    x = numleft? numleft : tenex(ttyline, CSHBUFSIZ);
		    if(x > roomleft) {
			/* start with fresh buffer */
			feobp = fseekp = fblocks * CSHBUFSIZ;
			numleft = x;
			goto again;
		    }
		    if (x > 0)
			copy(fbuf[buf] + off, ttyline, x * sizeof(wchar_t));
		    numleft = 0;
		} else
		    x = read_(SHIN, fbuf[buf] + off, roomleft);
		if (x >= 0)
		    break;
		if (errno == EWOULDBLOCK) {
		    int off = 0;

		    (void) ioctl(SHIN, FIONBIO,  (char *)&off);
		} else if (errno != EINTR)
		    break;
	    }
	    if(x <= 0)
		return (-1);
	    feobp += x;
	    if(filec && !intty)
		goto again;
	}
	wc = fbuf[buf][(int) fseekp % CSHBUFSIZ];
	fseekp++;
	return(wc);
}

static void
bfree(void)
{
	register int sb, i;

#ifdef TELL
	if (cantell)
		return;
#endif
	if (whyles)
		return;
	sb = (int) (fseekp - 1) / CSHBUFSIZ;
	if (sb > 0) {
		for (i = 0; i < sb; i++)
			xfree(fbuf[i]);
		(void) blkcpy(fbuf, &fbuf[sb]);
		fseekp -= CSHBUFSIZ * sb;
		feobp -= CSHBUFSIZ * sb;
		fblocks -= sb;
	}
}

void
bseek(off_t l)
{
	register struct whyle *wp;

	fseekp = l;
#ifdef TELL
	if (!cantell) {
#endif
		if (!whyles)
			return;
		for (wp = whyles; wp->w_next; wp = wp->w_next)
			continue;
		if (wp->w_start > l)
			l = wp->w_start;
#ifdef TELL
	}
#endif
}

/* any similarity to bell telephone is purely accidental */
#ifndef btell
off_t
btell(void)
{
	return (fseekp);
}
#endif

void
btoeof(void)
{
	(void) lseek(SHIN, 0, SEEK_END);
	fseekp = feobp;
	wfree();
	bfree();
}

#ifdef TELL
void
settell(void)
{
	cantell = 0;
	if (arginp || onelflg || intty)
		return;
	if (lseek(SHIN, 0, SEEK_CUR) < 0 || errno == ESPIPE)
		return;
	fbuf =  (wchar_t **)salloc(2, sizeof(wchar_t **));
	fblocks = 1;
	fbuf[0] = wcalloc(CSHBUFSIZ);
	fseekp = fbobp = feobp = lseek(SHIN, 0, SEEK_CUR);
	cantell = 1;
}
#endif
