/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

%{
#ident	"@(#)make:gram.y	1.22.1.1"

#include "defs"
#include <pfmt.h>
#define NLEFTS	200
#define INMAX	10000
%}

%term NAME SHELLINE START COLON DOUBLECOLON EQUAL A_STRING VERSION

%union
{
	SHBLOCK	 yshblock;
	DEPBLOCK ydepblock;
	NAMEBLOCK ynameblock;
	CHARSTAR ycharstring;
}

%type	<yshblock>	SHELLINE, shlist, shellist
%type	<ynameblock>	NAME, namelist, name
%type	<ydepblock>	deplist, dlist
%type	<ycharstring>	A_STRING

%%

%{
extern	NAMEBLOCK mainname;	/* from main.c */
extern	desc_start;		/* from main.c */
int	firstrule = YES;
#define TURNON(a)	(Mflags |= (a))
void	setvar();
DEPBLOCK 	pp;
NAMEBLOCK	leftp;
LINEBLOCK 	lp, lpp;
FSTATIC SHBLOCK prevshp;
FSTATIC NAMEBLOCK *lefts;
FSTATIC DEPBLOCK prevdep;
FSTATIC int	nlefts, szlefts,
		sepc;
char yyfilename[PATH_MAX];
static int nextlin();
static void fstack(char *, FILE **, int *, int);
static int GETC();
static int retsh(CHARSTAR);
%}


file:
	| file comline
	;

comline:  START
	| START macrodef
	| START namelist deplist shellist
	{
		if ( !(mainname || IS_ON(INTRULE)) &&
		     ((lefts[0]->namep[0] != DOT) ||
		      ANY(lefts[0]->namep, SLASH)))
			mainname = lefts[0];
	        while( --nlefts >= 0) {
			leftp = lefts[nlefts];
			if ( !(leftp->septype) )
				leftp->septype = sepc;
			else if (leftp->septype != sepc)
				yprintf(stderr,
					":158:inconsistent rules lines for `%s' (bu36)\n",
						leftp->namep);
			else if (sepc==ALLDEPS &&
				 *(leftp->namep)!=DOT &&
				 $4) {
				for (lp = leftp->linep;
				     lp->nextline;
				     lp = lp->nextline)
					if (lp->shp)
						yprintf(stderr, 
							":159:multiple make lines for %s (bu37)\n",
								leftp->namep);
			}
	
			lp = ALLOC(lineblock);
			lp->nextline = NULL;
			lp->depp = $3;
			lp->shp = $4;

			if (STREQ(leftp->namep, ".SUFFIXES") && $3==0)
				leftp->linep = 0;
			else if (STREQ(leftp->namep, ".MAKEOPTS") && $3==0)
				leftp->linep = 0;
			else if (STREQ(leftp->namep, ".POSIX")) {
				if($3 != 0 || $4 != 0)
					fatal1(":168:special target `%s:' illegal prerequisites or commands",leftp->namep);
				if(firstrule != POSIX)
					yprintf(stderr,":167:special target `%s:' not first rule: ignoring\n", leftp->namep);
				leftp->linep = 0;
			} else if ( !(leftp->linep) )
				leftp->linep = lp;
			else {
				for (lpp = leftp->linep;
				     lpp->nextline;
				     lpp = lpp->nextline)
					;
				if (sepc==ALLDEPS && 
						leftp->namep[0]==DOT &&
		      				!ANY(leftp->namep, SLASH))
					lpp->shp = 0;
				lpp->nextline = lp;
			}
		}
		free(lefts);
		lefts = NULL;
	}
	| error
	;

macrodef: NAME EQUAL A_STRING
	{
		setvar((CHARSTAR) $1, $3);
	}
	;

name:	NAME
	{
		if( !($$ = SRCHNAME((CHARSTAR) $1)) )
			$$ = makename((CHARSTAR) $1);
	}
	| NAME VERSION
	{
		if( !($$ = SRCHNAME((CHARSTAR) $1)) )
			$$ = makename((CHARSTAR) $1);
	}
	;

namelist: name
	{
                if ((lefts = (NAMEBLOCK *)malloc(NLEFTS * sizeof(NAMEBLOCK)))
                                == NULL) {
			fatal(":161:Too many lefts");
                }
                szlefts = NLEFTS;
                lefts[0] = $1;
                nlefts = 1;
	}
	| namelist name
	{
		lefts[nlefts++] = $2;
                if(nlefts >= szlefts) {
                        if ((lefts = (NAMEBLOCK *)realloc(lefts,
                                        (szlefts + NLEFTS) * sizeof(NAMEBLOCK)))                                        == NULL) {
				fatal(":161:Too many lefts");
                        }
                        szlefts += NLEFTS;
                }
	}
	;

dlist:  sepchar
	{
		prevdep = 0;
		$$ = 0;
	}
	| dlist name
	{
		pp = ALLOC(depblock);
		pp->nextdep = NULL;
		pp->depname = $2;
		if ( !prevdep )
			$$ = pp;
		else
			prevdep->nextdep = pp;
		prevdep = pp;
	}
	;

deplist:	
	{
		yprintf(stderr, _SGI_MMX_make_sep ":Must be a separator (: or ::) for rules (bu39)\n");
		$$ = 0;
	}
	| dlist
	;

sepchar:  COLON
	{
		sepc = ALLDEPS;
	}
	| DOUBLECOLON
	{
		sepc = SOMEDEPS;
	}
	;


shlist:	SHELLINE
	{
		$$ = $1;
		prevshp = $1;
	}
	| shlist SHELLINE
	{
		$$ = $1;
		prevshp->nextsh = $2;
		prevshp = $2;
	}
	;

shellist:
	{
		$$ = 0;
	}
	| shlist
	{
		$$ = $1;
	}
	;

%%

/*	@(#)make:gram.y	1.6 of 5/9/86	*/

#include <ctype.h>

CHARSTAR zznextc;	/* zero if need another line; otherwise points to next char */
int yylineno;
static char inmacro = NO;

static char *word;
static int word_count;
void
init_lex()
{
	word_count = 128;
	 word = ck_malloc(word_count);
}

int
yylex()
{
	register CHARSTAR lq, p, q;
	CHARSTAR pword;
	int indollar = 0;

	if ( !zznextc )
		return( nextlin() );

	while ( isspace(*zznextc) )
		++zznextc;

	if ( inmacro ) {
		inmacro = NO;
		yylval.ycharstring = copys(zznextc);
		zznextc = 0;
		return(A_STRING);
	}

	if (*zznextc == CNULL)
		return( nextlin() );

	if (*zznextc == KOLON) {
		if(*++zznextc == KOLON)	{
			++zznextc;
			return(DOUBLECOLON);
		} else
			return(COLON);
	}

	if (*zznextc == EQUALS) {
		inmacro = YES;
		++zznextc;
		return(EQUAL);
	}

	if (*zznextc == SKOLON)
		return( retsh(zznextc) );

	q = pword = word;

	p = zznextc;

	lq = word + word_count;
	while ( !( funny[(unsigned char) *p] & TERMINAL) ) {
again:
		if (q == lq) {
			if ((word = realloc(word, word_count + 128)) == NULL)
				fatal1(":140:couldn't malloc len=%d", word_count + 128);
			/* recompute pointers - realloc may change */
			pword = word;
			q = word + word_count;
			word_count += 128;
			lq = word + word_count;
		}
		*q++ = *p++;
	}
	/*
	 * Permit $$(@:.c=.o)
	 */
	if (*p == DOLLAR) {
		indollar++;
		goto again;
	}
	if (indollar) {
		if (*p == KOLON || *p == EQUALS)
			goto again;
		else if (*p == RPAREN)
			indollar = 0;
	}

	if (p != zznextc) {
		*q = CNULL;
		yylval.ycharstring = copys(pword);
		if (*p == RCURLY) {
			zznextc = p+1;
			return(VERSION);
		}
		if (*p == LCURLY)
			p++;
		zznextc = p;
		if(desc_start == YES && firstrule == YES) {
			desc_start = firstrule = NO;
			/* capture this here so that we know as
			 * soon as possible that we're in POSIX
			 * mode
			 */
			if(STREQ(".POSIX",yylval.ycharstring)){
				TURNON(POSIX);
				posix_env();
				firstrule = POSIX;
			}
			/* Not POSIX - convert MAKEFLAGS back */
			else	convtmflgs();	
		}
		return(NAME);
	} else {
		yprintf(stderr, _SGI_MMX_make_badc ":bad character %c (octal %o)\n",
				*zznextc,*zznextc);
		fatal(NULL);
	}
	return(0);	/* never executed */
}


static int
retsh(q)
register CHARSTAR q;
{
	extern CHARSTAR *linesptr;
	register CHARSTAR p;
	SHBLOCK sp = ALLOC(shblock);

	for (p = q + 1; *p==BLANK || *p==TAB; ++p)
		;
	sp->nextsh = 0;
	sp->shbp = ( !fin ? p : copys(p) );
	yylval.yshblock = sp;
	zznextc = 0;
/*
 *	The following if-else "thing" eats up newlines within
 *	shell blocks.
 */
	if ( !fin ) {
		if (linesptr[0])
			while (linesptr[1] && STREQ(linesptr[1], "\n")) {
				yylineno++;
				linesptr++;
			}
	} else {
		register int c;
		while ((c = GETC()) == NEWLINE)
			yylineno++;
		if (c != EOF)
			(void)ungetc(c, fin);
	}
	return(SHELLINE);
}

static int morefiles;

static int
nextlin()
{
	extern CHARSTAR *linesptr;
	static char yytext[INMAX*10];
	static CHARSTAR yytextl = yytext+INMAX*10;
	register char c;
	register CHARSTAR p, t;
	/* templin needs to be large enough to hold entire dependency line
	 * with full macros expanded as does yytext
	 */
	char 	templin[INMAX*10], lastch;
	CHARSTAR text, lastchp, subst();
	int	sindex();
	int 	incom, kc, nflg, poundflg, poundfirst;

again:	incom = 0;
	zznextc = 0;
	poundflg = 0;
	poundfirst = 0;

	if ( !fin ) {
		if ( !(text = *linesptr++) )
			return(0);
		++yylineno;
		(void)copstr(yytext, text);
	} else {
		yytext[0] = CNULL;
		for (p = yytext; ; ++p) {
			if(p > yytextl)
				fatal(":164:line too long");
			if ((kc = GETC()) == EOF) {
				*p = CNULL;
				return(0);
			}
                        else if(kc == '!' && poundfirst && morefiles == 0
                                && yylineno == 0 && p == yytext+1) {
                                /* special #! 'pick-a-make' */
                                p = yytext;
                                while (kc = GETC()) {
                                        if (kc == NEWLINE) {
                                                yylineno++;
                                                break;
                                        }
                                        if (kc == EOF)
                                                break;
                                        *p++ = kc;
                                }
                                *p = CNULL;
                                donmake(yytext);
                                /* if returns then failed - ignore line */
                                goto again;
			}
			else if ((kc == SKOLON) ||
				 (kc == TAB && p == yytext))
				++incom;
			else if (p[-1] == BACKSLASH && kc == POUND && !incom && yytext[0] != TAB) {
				/* permit \# to work */
				p--;
			} else if (kc == POUND && !incom && yytext[0] != TAB) {
				poundflg++;
				kc = CNULL;
				/* identify if first line is #! */
				if (p == yytext)
					poundfirst++;
			}
			else if (kc == NEWLINE)	{
				++yylineno;
				/*
				 * ATT has additional '|| poundflg' but
				 * this breaks the ability to comment out
				 * continuation lines. I hope this isn't
				 * a POSIX feature!
				 */
				if (p == yytext || p[-1] != BACKSLASH)
					break;
				if(incom || yytext[0] == TAB)
					*p++ = NEWLINE;
				else
					p[-1] = BLANK;
				nflg = YES;
				while (kc = GETC()) {
					if ( !( kc == TAB ||
					        kc == BLANK ||
					        kc == NEWLINE) )
						break;
					if (incom || yytext[0] == TAB) {
						/*
						 * do not put TABS into shell
						 * command lines
						 */
						if (nflg && kc == TAB) {
							nflg = NO;
							continue;
						}
						if (kc == NEWLINE)
							nflg = YES;
						*p++ = kc;
					}
					if (kc == NEWLINE)
						++yylineno;
				}
				if(kc == EOF) {
					*p = CNULL;
					return(0);
				}
			}
			*p = kc;
		}
		*p = CNULL;
		text = yytext;
	}
	
	if ((c = text[0]) == TAB)  
		return( retsh(text) );
/*
 *	DO include FILES HERE.
 */
	if ( !sindex(text, "include") && 
	     (text[7] == BLANK || text[7] == TAB)) {
		CHARSTAR pfile;

		for (p = &text[8]; *p != CNULL; p++)
			if(*p != TAB &&
			   *p != BLANK)
				break;
		pfile = p;
		for (;	*p != CNULL	&&
			*p != NEWLINE	&&
			*p != TAB	&&
			*p != BLANK; p++)
			;
		if (*p != CNULL)
			*p = CNULL;

/*
 *	Start using new file.
 */
		(void)subst(pfile, templin, 0);
		fstack(templin, &fin, &yylineno, 1);
		goto again;
	}

	if ( !sindex(text, "sinclude") && 
	     (text[8] == BLANK || text[8] == TAB)) {
		CHARSTAR pfile;

		for (p = &text[9]; *p != CNULL; p++)
			if(*p != TAB &&
			   *p != BLANK)
				break;
		pfile = p;
		for (;	*p != CNULL	&&
			*p != NEWLINE	&&
			*p != TAB	&&
			*p != BLANK; p++)
			;
		if (*p != CNULL)
			*p = CNULL;

/*
 *	Start using new file.
 */
		(void)subst(pfile, templin, 0);
		fstack(templin, &fin, &yylineno, 0);
		goto again;
	}
	if (isalpha(c) || isdigit(c) || c==BLANK || c==DOT || c=='_')
		for (p = text+1; *p!=CNULL; p++)
			if (*p == KOLON || *p == EQUALS)
				break;

/* substitute for macros on dependency line up to the semicolon if any */
	if (p && *p != EQUALS) {
		for (t = yytext ; *t!=CNULL && *t!=SKOLON ; ++t)
			;

		lastchp = t;
		lastch = *t;
		*t = CNULL;

		(void)subst(yytext, templin, 0);

		if (lastch) {
			for (t = templin ; *t ; ++t)
				;
			*t = lastch;
			while ( *++t = *++lastchp )
				;
		}

		p = templin;
		t = yytext;
		while ( *t++ = *p++ )
			;
	}

	if ( !poundflg || yytext[0] != CNULL)	{
		zznextc = text;
		return(START);
	} else
		goto again;
}


#include "stdio.h"


static struct sfiles {
	CHARSTAR sfname;
	FILE *sfilep;
	int syylno;
} sfiles[MAX_INC];


/* GETC() automatically unravels stacked include files.  During
 *	`include' file processing, when a new file is encountered,
 *	fstack will stack the FILE pointer argument. Subsequent
 *	calls to GETC with the new FILE pointer will get characters
 *	from the new file. When an EOF is encountered, GETC will
 *	check to see if the file pointer has been stacked. If so,
 *	a character from the previous file will be returned.
 */
static int
GETC()
{
	register int c;

	while ((c = getc(fin)) == EOF && morefiles) {
		(void)fclose(fin);
		yylineno = sfiles[--morefiles].syylno;
		fin = sfiles[morefiles].sfilep;
                strcpy(yyfilename, sfiles[morefiles].sfname);
		free(sfiles[morefiles].sfname);
                if(IS_ON(DBUG) || IS_ON(DBUG2))
                        pfmt(stdout, MM_NOSTD,
				_SGI_MMX_make_backi ":Back to file: \"%s\"\n",
				yyfilename);
	}
	return(c);
}


/* fstack(stfname,ream,lno) is used to stack an old file pointer
 *	before the new file is assigned to the same variable. Also
 *	stacked are the file name and the old current lineno,
 *	generally, yylineno. 
 */
static	void
fstack(newname, oldfp, oldlno, force)
register char *newname;
register FILE **oldfp;
register int *oldlno;
int force;
{
	FILE *newfp;			/* new file descriptor */
	char *strcat();

	/* if unable to open file */
	if ( !(newfp = fopen(newname, "r")) ) {
		/* try sccs get of file */
		if ( !get(newname, 0, 0) ) {
			if (force)
				fatal1(":165:cannot get %s for including",
					newname);
			else {
#ifdef MKDEBUG
				if (IS_ON(DBUG))
					printf(
				"cannot get include file (skipped) %s\n",
						newname);
				return;
			}
#endif
		}

		if ( !(newfp = fopen(newname, "r")) )
			if (force)
				fatal1(":166:cannot open %s for including",
					newname);
			else {
#ifdef MKDEBUG
				if (IS_ON(DBUG))
					printf(
				"cannot open include file (skipped) %s\n",
						newname);
				return;
#endif
			}
	}

#ifdef MKDEBUG
	if (IS_ON(DBUG) || IS_ON(DBUG2))
		pfmt(stdout, MM_NOSTD,
			_SGI_MMX_make_inc ":Include file: \"%s\"\n",newname);
#endif
/*
 *	Stack the new file name, the old file pointer and the
 *	old yylineno;
 */
	
	if(morefiles >=  MAX_INC)
		fatal1(_SGI_MMX_make_tooinc ":include files nested more than %d deep", MAX_INC-1);
	sfiles[morefiles].sfname = ck_malloc(strlen(yyfilename) + 1);
	(void)strcpy(sfiles[morefiles].sfname, yyfilename);
	sfiles[morefiles].sfilep = *oldfp;
	sfiles[morefiles++].syylno = *oldlno;
	yylineno = 0;
	*oldfp = newfp;				/* set the file pointer */
	(void)strcpy(yyfilename, newname);
}

