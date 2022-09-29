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
#include <assert.h>
#include <alloca.h>
#include <fcntl.h>
#include "sh.h"
#include "sh.wconst.h"

/*
 * C shell
 */

/*
 * These routines perform variable substitution and quoting via ' and ".
 * To this point these constructs have been preserved in the divided
 * input words.  Here we expand variables and turn quoting via ' and " into
 * QUOTE bits on characters (which prevent further interpretation).
 * If the `:q' modifier was applied during history expansion, then
 * some QUOTEing may have occurred already, so we dont "trim()" here.
 */

wchar_t	Dpeekc, Dpeekrd;		/* Peeks for DgetC and Dreadc */
wchar_t	*Dcp, **Dvp;			/* Input vector for Dreadc */

#define	DEOF	-1
#define	unDgetC(c)	(Dpeekc = (c))
#define QUOTES		(_Q|_Q1|_ESC)	/* \ ' " ` */

/*
 * The following variables give the information about the current
 * $ expansion, recording the current word position, the remaining
 * words within this expansion, the count of remaining words, and the
 * information about any : modifier which is being applied.
 */
wchar_t	*dolp;			/* Remaining chars from this word */
wchar_t	**dolnxt;		/* Further words */
int	dolcnt;			/* Count of further words */
wchar_t	dolmod;			/* : modifier character */
int	dolmcnt;		/* :gx -> 10000, else 1 */

static void	Dfix2(wchar_t **);
static wchar_t	DgetC(int);
static void	Dgetdol(void);
static wchar_t	Dredc(void);
static void	Dtestq(wchar_t);
static int	Dword(void);
static void	setDolp(wchar_t *);
static void	unDredc(wchar_t);

/*
 * print ambiguous messages
 */
void
ambiguous(void)
{
	bferr(gettxt(_SGI_DMMX_csh_Ambiguous, "Ambiguous"));
}

static wchar_t
Dredc(void)
{
	register wchar_t c;

	if(c = Dpeekrd) {
	    Dpeekrd = 0;
	    return(c);
	}
	if(Dcp && (c = *Dcp++))
	    return(c);
	if( ! *Dvp) {
	    Dcp = 0;
	    return (DEOF);
	}
	Dcp = *Dvp++;
	return(' ');
}

static void
unDredc(wchar_t c)
{
	Dpeekrd = c;
}

/*
 * Subroutine to do actual fixing after state initialization.
 */
static void
Dfix2(wchar_t **v)
{
	wchar_t **agargv;
	agargv = (wchar_t **)alloca((gavsiz+4)*sizeof(*agargv));

#ifdef TRACE
	tprintf("TRACE- Dfix2()\n");
#endif
	ginit(agargv);			/* Initialize glob's area pointers */
	Dvp = v; Dcp = S_;		/* Setup input vector for Dreadc */
	unDgetC(0); unDredc(0);		/* Clear out any old peeks (at error) */
	dolp = 0; dolcnt = 0;		/* Clear out residual $ expands (...) */
	while(Dword());
	gargv = copyblk(gargv);
}

/*
 * Fix up the $ expansions and quotations in the
 * argument list to command t.
 */
void
Dfix(struct command *t)
{
	register wchar_t **pp;
	register wchar_t *p;

#ifdef TRACE
	tprintf("TRACE- Dfix()\n");
#endif
	if(noexec)
	    return;
	/*
	 * Note that t_dcom isn't trimmed
	 * thus !...:q's aren't lost
	 */
	for(pp = t->t_dcom; p = *pp++;) {
	    while(*p) {
		if(cmlook(*p++, _DOL | QUOTES)) {	/* $, \, ', ", ` */
		    Dfix2(t->t_dcom);			/* found one */
		    blkfree(t->t_dcom);
		    t->t_dcom = gargv;
		    gargv = 0;
		    return;
		}
	    }
	}
}

/*
 * $ substitute one word, for i/o redirection
 */
wchar_t *
Dfix1(wchar_t *cp)
{
	wchar_t *Dv[2];

#ifdef TRACE
	tprintf("TRACE- Dfix1(%t)\n", cp);
#endif
	if(noexec)
	    return(0);
	Dv[0] = cp; Dv[1] = NOSTR;
	Dfix2(Dv);
	if(gargc != 1) {
	    setname(cp);
	    ambiguous();
	}
	cp = savestr(gargv[0]);
	blkfree(gargv), gargv = 0;
	return(cp);
}

/*
 * Get a word.  This routine is analogous to the routine
 * word() in sh.lex.c for the main lexical input.  One difference
 * here is that we don't get a newline to terminate our expansion.
 * Rather, DgetC will return a DEOF when we hit the end-of-input.
 */
static int
Dword(void)
{
	register wchar_t c, c1;
	register wchar_t *wp;
	register int i = CSHBUFSIZ - 4;
	register bool dolflg;
	bool sofar = 0;
	wchar_t wbuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- Dword()\n");
#endif
	wp = wbuf;
loop:
	c = DgetC(DODOL);
	switch(c) {
	    case DEOF:
deof:
		if( !sofar) {
		    return(0);
		}
		unDredc(c);		/* finish this word */
	    case '\n':
		*wp = 0;
		goto ret;
	    case ' ':
	    case '\t':
		goto loop;
	    case '`':
		*wp++ = c, --i;		/* preserve ` quotations, done later */
	    case '\'':
	    case '"':
		/*
		 * Note that DgetC never returns a QUOTES character
		 * from an expansion, so only true input quotes will
		 * get us here or out.
		 */
		c1 = c;
		dolflg = (c1 == '"')? DODOL : 0;
		for(;;) {
		    c = DgetC(dolflg);
		    if(c == c1)
			break;
		    if(c == '\n' || c == DEOF)
			err_unmatched(c1);
		    if(c == ('\n' | QUOTE))
			--wp, ++i;
		    if(--i <= 0)
			goto toochars;

		    switch(c1) {
			case '"':
			    /*
			     * Leave any `s alone for later.
			     * Other chars are all quoted, thus `...`
			     * can tell it was within "...".
			     */
			    *wp++ = (c == '`')? '`' : (wchar_t)(c | QUOTE);
			    break;
			case '\'':
			    /*
			     * Prevent all further interpretation
			     */
			    *wp++ = (wchar_t)(c | QUOTE);
			    break;
			case '`':
			    /*
			     * Leave all text alone for later
			     */
			    *wp++ = c;
			    break;
			}
		}
		if(c1 == '`')
		    *wp++ = '`', --i;
		goto pack;			/* continue the word */
	    case '\\':
		c = DgetC(0);			/* No $ subst! */
		if(c == '\n' || c == DEOF)
		    goto loop;
		c |= QUOTE;
		break;
	    default :
		if(isauxsp(c))
		    goto loop;
	}
	unDgetC(c);
pack:
	sofar = 1;

	/*
	 * pack up more characters in this word
	 */
	for(;;) {
	    c = DgetC(DODOL);
	    if(c == '\\') {
		c = DgetC(0);
		if(c == DEOF)
		    goto deof;
		if(c == '\n')
		    c = ' ';
		else
		    c |= QUOTE;
	    }
	    if(c == DEOF)
		goto deof;
	    if(( !(c & QUOTE) && cmlook(c, _SP|_NL|_Q|_Q1)) || isauxsp(c)) {
		unDgetC(c);
		if(cmlook(c, QUOTES))
		    goto loop;
		*wp++ = 0;
		goto ret;
	    }
	    if(--i <= 0)
toochars:
		err_word2long();
	    *wp++ = c;
	}
ret:
	Gcat(S_, wbuf);
	return(1);
}

/*
 * Get a character, performing $ substitution unless flag is 0.
 * Any QUOTES character which is returned from a $ expansion is
 * QUOTEd so that it will not be recognized above.
 */
static wchar_t
DgetC(int flag)
{
	register wchar_t c;

top:
	if(c = Dpeekc) {
	    Dpeekc = 0;
	    return(c);
	}
	if(lap) {
	    c = *lap++;
	    if( !c) {
		lap = 0;
		goto top;
	    }
quotspec:
	    if(cmlook(c, QUOTES))
		return((wchar_t)(c | QUOTE));
	    return(c);
	}
	if(dolp) {
	    if(c = *dolp++)
		goto quotspec;
	    if(dolcnt > 0) {
		setDolp(*dolnxt++);
		--dolcnt;
		return(' ');
	    }
	    dolp = 0;
	}
	if(dolcnt > 0) {
	    setDolp(*dolnxt++);
	    --dolcnt;
	    goto top;
	}
	c = Dredc();
	if(c == '$' && flag) {
	    Dgetdol();
	    goto top;
	}
	return(c);
}

wchar_t	*nulvec[] = { 0 };
struct	varent nulargv = { nulvec, S_argv, 0 };

/*
 * Handle the multi-tudinous $ expansion forms.
 * Ugh.
 */
static void
Dgetdol(void)
{
	register wchar_t *np;
	register struct varent *vp;
	register wchar_t c, sc;
	int subscr = 0, lwb = 1, upb = 0;
	bool dimen = 0, bitset = 0;
	register int i;
	wchar_t name[MAXVARLEN + 4];
	wchar_t wbuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- Dgetdol()\n");
#endif
	dolmod = dolmcnt = 0;
	c = sc = DgetC(0);
	if(c == '{')
	    c = DgetC(0);			/* sc is { to take } later */
	if((c & TRIM) == '#')
	    dimen++, c = DgetC(0);		/* $# takes dimension */
	else
	    if(c == '?')
		bitset++, c = DgetC(0);		/* $? tests existence */

	switch(c) {
	    case '$':
		if(dimen || bitset)
		    goto syntax;		/* No $?$, $#$ */
		setDolp(doldol);
		goto eatbrac;

	    case '<' | QUOTE:
		if(dimen || bitset)
		    goto syntax;		/* No $?<, $#< */
		for(np = wbuf; read_(OLDSTD, np, 1) == 1; np++) {
		    if(np >= &wbuf[CSHBUFSIZ-1]) {
			setname(S_DOLLESS);
			err_line2long();	/* no return */
		    }
		    if(*np <= 0 || *np == '\n')
			break;
		}
		*np = 0;

		/*
		 * KLUDGE: dolmod is set here because it will
		 * cause setDolp to call domod and thus to copy wbuf.
		 * Otherwise setDolp would use it directly. If we saved
		 * it ourselves, no one would know when to free it.
		 * The actual function of the 'q' causes filename
		 * expansion not to be done on the interpolated value.
		 */
		dolmod = 'q';
		dolmcnt = 10000;
		setDolp(wbuf);
		goto eatbrac;

	    case DEOF:
	    case '\n':
		goto syntax;

	    case '*':
		wscpy(name, S_argv);
		vp = adrof(S_argv);
		subscr = -1;			/* Prevent eating [...] */
		break;

	    default:
		np = name;
		if(digit(c)) {
		    if (dimen)
			goto syntax;		/* No $#1, e.g. */
		    subscr = 0;
		    do {
			subscr = (subscr * 10) + (c - '0');
			c = DgetC(0);
		    } while(digit(c));
		    unDredc(c);
		    if(subscr < 0)
			goto oob;
		    if( !subscr) {
			if(bitset) {
			    dolp = file? S_1 : S_0;
			    goto eatbrac;
			}
			if( !file)
			    error(gettxt(_SGI_DMMX_csh_no0file,
				"No file for $0"));
			setDolp(file);
			goto eatbrac;
		    }
		    if(bitset)
			goto syntax;
		    vp = adrof(S_argv);
		    if( !vp) {
			vp = &nulargv;
			goto eatmod;
		    }
		    break;
		}
		if( !alnum(c))
		    goto syntax;
		for(;;) {
		    *np++ = c;
		    c = DgetC(0);
		    if( !alnum(c))
			break;
		    if(np >= &name[MAXVARLEN])
syntax:
			error(gettxt(_SGI_DMMX_csh_varsyntax,
			    "Variable syntax"));
		}
		*np++ = 0;
		unDredc(c);
		vp = adrof(name);
	}
	/*
	 * getenv() to getenv_(),
	 * because 'name''s type is now wchar_t *
	 */
	if(bitset) {
	    np = getenv_(name);
	    dolp = (vp || np)? S_1 : S_0;
	    xfree(np);
	    goto eatbrac;
	}
	if( !vp) {
	    if(np = getenv_(name)) {
		addla(np);
		xfree(np);
		goto eatbrac;
	    }
	    udvar(name);
	}
	c = DgetC(0);
	upb = blklen(vp->vec);
	if( !dimen && !subscr && (c == '[')) {
	    for(np = name;;) {
		c = DgetC(DODOL);	/* Allow $ expand within [ ] */
		if(c == ']')
		    break;
		if(c == '\n' || c == DEOF)
		    goto syntax;
		if(np >= &name[MAXVARLEN])
		    goto syntax;
		*np++ = c;
	    }
	    *np = 0, np = name;
	    if(dolp || dolcnt)		/* $ exp must end before ] */
		goto syntax;
	    if( !*np)
		goto syntax;
	    if(digit(*np)) {
		i = 0;
		while(digit(*np))
		    i = i * 10 + *np++ - '0';
		if((i < 0 || i > upb) && (*np != '-') && (*np != '*')) {
oob:
		    setname(vp->v_name);
		    error(gettxt(_SGI_DMMX_csh_subsout,
			"Subscript out of range"));
		}
		lwb = i;
		if( !*np)
		    upb = lwb, np = S_AST;
	    }
	    if(*np == '*')
		np++;
	    else {
		if(*np != '-')
		    goto syntax;
	    	else {
		    register int i = upb;

		    np++;
		    if(digit(*np)) {
			i = 0;
			while(digit(*np))
			    i = i * 10 + *np++ - '0';
			if(i < 0 || i > upb)
			    goto oob;
		    }
		    if(i < lwb)
			upb = lwb - 1;
		    else
			upb = i;
	        }
	    }
	    if( !lwb) {
		if(upb)
		    goto oob;
		upb = -1;
	    }
	    if(*np)
		goto syntax;
	} else {
	    if(subscr > 0) {
		if(subscr > upb)
		    lwb = 1, upb = 0;
		else
		    lwb = upb = subscr;
	    }
	    unDredc(c);
	}
	if(dimen) {
	    wchar_t *cp = putn(upb - lwb + 1);

	    addla(cp);
	    xfree(cp);
	} else {
eatmod:
	    c = DgetC(0);
	    if(c == ':') {
		c = DgetC(0), dolmcnt = 1;
		if(c == 'g')
		    c = DgetC(0), dolmcnt = 10000;
		if( !any(c, S_htrqxe))
		    error(gettxt(_SGI_DMMX_csh_badmodin, "Bad : mod in $"));
		dolmod = c;
		if(c == 'q')
		    dolmcnt = 10000;
	    } else
		unDredc(c);
	    dolnxt = &vp->vec[lwb - 1];
	    dolcnt = upb - lwb + 1;
	}
eatbrac:
	if(sc == '{') {
	    c = Dredc();
	    if(c != '}')
		goto syntax;
	}
}

static void
setDolp(wchar_t *cp)
{
	register wchar_t *dp;

#ifdef TRACE
	tprintf("TRACE- setDolp()\n");
#endif
	if( !dolmod || !dolmcnt) {
	    dolp = cp;
	    return;
	}
	dp = domod(cp, dolmod);
	if(dp) {
	    dolmcnt--;
	    addla(dp);
	    xfree(dp);
	} else
	    addla(cp);
	dolp = S_;
}

static void
Dtestq(wchar_t c)
{
	if(cmlook(c, QUOTES))
	    gflag = 1;
}

/*
 * Form a shell temporary file (in unit 0) from the words
 * of the shell input up to a line the same as "term".
 * Unit 0 should have been closed before this call.
 */
void
heredoc(wchar_t *term)
{
	register wchar_t *lbp, *obp, *mbp;
	register wchar_t c;
	wchar_t **vp;
	int ocnt, lcnt, mcnt;
	bool quoted;
	wchar_t *Dv[2];
	wchar_t obuf[CSHBUFSIZ];
	wchar_t lbuf[CSHBUFSIZ];
	wchar_t mbuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- heredoc()\n");
#endif
	if(creat_(shtemp, 0600) < 0)
	    Perror(shtemp);
	(void)close(0);
	if(open_(shtemp, O_RDWR) < 0) {
	    int oerrno = errno;

	    (void)unlink_(shtemp);
	    errno = oerrno;
	    Perror(shtemp);
	}
	(void)unlink_(shtemp);			/* 0 0 inode! */
	Dv[0] = term; Dv[1] = NOSTR; gflag = 0;
	trim(Dv); rscan(Dv, Dtestq); quoted = gflag;
	ocnt = CSHBUFSIZ; obp = obuf;

	for(;;) {
	    /*
	     * Read up a line
	     */
	    lbp = lbuf; lcnt = CSHBUFSIZ - 4;
	    for(;;) {
		c = readc(1);		/* 1 -> Want EOF returns */
		if(c < 0) {
		    setname(term);
		    bferr(gettxt(_SGI_DMMX_csh_notermin,
			    "<< terminator not found"));
		}
		if(c == '\n')
		    break;
		if(c &= TRIM) {
		    *lbp++ = c;
		    if (--lcnt < 0) {
			setname(S_LESLES);
			err_line2long();
		    } 
		}
	    }
	    *lbp = 0;

	    /*
	     * Compare to terminator -- before expansion
	     */
	    if(eq(lbuf, term)) {
		(void)write_(0, obuf, CSHBUFSIZ - ocnt);
		(void)lseek(0, 0, SEEK_SET);
		return;
	    }

	    /*
	     * If term was quoted or -n just pass it on
	     */
	    if(quoted || noexec) {
		*lbp++ = '\n'; *lbp = 0;
		for(lbp = lbuf; c = *lbp++;) {
		    *obp++ = c;
		    if(--ocnt == 0) {
			(void)write_(0, obuf, CSHBUFSIZ);
			obp = obuf; ocnt = CSHBUFSIZ;
		    }
		}
		continue;
	    }

	    /*
	     * Term wasn't quoted so variable and then command
	     * expand the input line
	     */
	    Dcp = lbuf; Dvp = Dv + 1; mbp = mbuf; mcnt = CSHBUFSIZ - 4;
	    for(;;) {
		c = DgetC(DODOL);
		if(c == DEOF)
		    break;
		if( !(c &= TRIM))
		    continue;
		/*
		 * \ quotes \ $ ` here
		 */
		if(c =='\\') {
		    c = DgetC(0);
		    if((c == '$') || (c == '\\') || (c == '`'))
			c |= QUOTE;
		    else
			unDgetC((wchar_t)(c | QUOTE)), c = '\\';
		}
		*mbp++ = c;
		if(--mcnt == 0) {
		    setname(S_LESLES);
		    bferr(gettxt(_SGI_DMMX_csh_lineov, "Line overflow"));
		}
	    }
	    *mbp++ = 0;

	    /*
	     * If any ` in line do command substitution
	     */
	    mbp = mbuf;
	    if(any('`', mbp)) {
		/*
		 * 1 arg to dobackp causes substitution to be literal.
		 * Words are broken only at newlines so that all blanks
		 * and tabs are preserved.  Blank lines (null words)
		 * are not discarded.
		 */
		vp = dobackp(mbuf, 1);
	    } else
		/*
		 * Setup trivial vector similar to return of dobackp
		 */
		Dv[0] = mbp, Dv[1] = NOSTR, vp = Dv;

	    /*
	     * Resurrect the words from the command substitution
	     * each separated by a newline.  Note that the last
	     * newline of a command substitution will have been
	     * discarded, but we put a newline after the last word
	     * because this represents the newline after the last
	     * input line!
	     */
	    for(; *vp; vp++) {
		for(mbp = *vp; *mbp; mbp++) {
		    *obp++ = *mbp & TRIM;
		    if(--ocnt == 0) {
			(void)write_(0, obuf, CSHBUFSIZ);
			obp = obuf; ocnt = CSHBUFSIZ;
		    }
		}
		*obp++ = '\n';
		if(--ocnt == 0) {
		    (void)write_(0, obuf, CSHBUFSIZ);
		    obp = obuf; ocnt = CSHBUFSIZ;
		}
	    }
	    if(pargv)
		blkfree(pargv), pargv = 0;
	}
}
