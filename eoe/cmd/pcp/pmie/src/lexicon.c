/***********************************************************************
 * lexicon.c - lexical scanner
 *
 * There is enough lookahead to enable use of '/' as both an arithmetic
 * and units operator.  Nested macro expansion is supported using a stack
 * of input contexts (see definition of LexIn in file syntax.y).
 ***********************************************************************
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: lexicon.c,v 1.1 1999/04/28 10:06:17 kenmcd Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "dstruct.h"
#include "syntax.h"
#include "grammar.h"
#include "lexicon.h"
#include "pragmatics.h"


/***********************************************************************
 * type definitions
 ***********************************************************************/

/* for table of lexical items */
typedef struct {
    char        *key;
    int         token;
} LexEntry1;

/* for another table of lexical items */
typedef struct {
    char        *key;
    int         token;
    int         scale;
} LexEntry2;


/***********************************************************************
 * constants
 ***********************************************************************/

static LexEntry1 domtab[] = {
        {"host",   HOST_DOM},
        {"inst",   INST_DOM},
        {"sample", TIME_DOM},
        {NULL,   0}
};

static LexEntry1 quantab[] = {
        {"sum",   SUM_AGGR},
        {"avg",   AVG_AGGR},
        {"max",   MAX_AGGR},
        {"min",   MIN_AGGR},
        {"count", COUNT_AGGR},
	{"all",   ALL_QUANT},
	{"some",  SOME_QUANT},
	{"%",     PCNT_QUANT},
        {NULL,    0}
};

static LexEntry1 optab[] = {
	{"true",    	TRU},
	{"false",   	FALS},
	{"rate",    	RATE},
	{"shell",   	SHELL},
	{"alarm",   	ALARM},
	{"syslog",  	SYSLOG},
	{"print",   	PRINT},
	{"rising",  	RISE},
	{"falling", 	FALL},
	{"match_inst",	MATCH},
	{"nomatch_inst",NOMATCH},
        {NULL,      	0}
};

static LexEntry2 unitab[] = {
	{"byte",	SPACE_UNIT, PM_SPACE_BYTE},
	{"Kbyte",	SPACE_UNIT, PM_SPACE_KBYTE},
	{"Mbyte",	SPACE_UNIT, PM_SPACE_MBYTE},
	{"Gbyte",	SPACE_UNIT, PM_SPACE_GBYTE},
	{"Tbyte",	SPACE_UNIT, PM_SPACE_TBYTE},
	{"nsec",	TIME_UNIT,  PM_TIME_NSEC},
	{"usec",	TIME_UNIT,  PM_TIME_USEC},
	{"msec",	TIME_UNIT,  PM_TIME_MSEC},
	{"sec",	TIME_UNIT,  PM_TIME_SEC},
	{"min",	TIME_UNIT,  PM_TIME_MIN},
	{"hour",	TIME_UNIT,  PM_TIME_HOUR},
	{"count",	EVENT_UNIT, 0},
	{"Kcount",	EVENT_UNIT, 3},
	{"Mcount",	EVENT_UNIT, 6},
	{"Gcount",	EVENT_UNIT, 9},
	{"Tcount",	EVENT_UNIT, 12},
        {"nanosec",	TIME_UNIT,  PM_TIME_NSEC},
        {"nanosecond",	TIME_UNIT,  PM_TIME_NSEC},
        {"microsec",	TIME_UNIT,  PM_TIME_USEC},
        {"microsecond",	TIME_UNIT,  PM_TIME_USEC},
        {"millisec",	TIME_UNIT,  PM_TIME_MSEC},
        {"millisecond",	TIME_UNIT,  PM_TIME_MSEC},
	{"second",	TIME_UNIT,  PM_TIME_SEC},
	{"minute",	TIME_UNIT,  PM_TIME_MIN},
        {NULL,     0,          0}
};



/***********************************************************************
 * variables
 ***********************************************************************/

       LexIn    *lin;                   /* current input context */
static char	*token;			/* current token buffer */



/***********************************************************************
 * local functions
 ***********************************************************************/

/* unwind input context */
static void
unwind(void)
{
    LexIn    *tmp;

    if (lin->name) {
	free(lin->name);
	if (! lin->macro)
	    fclose(lin->stream);
    }
    tmp = lin;
    lin = lin->prev;
    free(tmp);
}


/* next input character */
static int
nextc(void)
{
    int      c = '\0';

    if (lin) {
	if (lin->lookin != lin->lookout) {
	    c = lin->look[lin->lookout++];
	    if (lin->lookout >= LEX_MAX + 2)
		lin->lookout = 0;
	}
	else {
	    if (lin->macro)
		c = *lin->macro++;
	    else {
		c = getc(lin->stream);
		if (c == EOF)
		    c = '\0';
	    }
	    if (c == '\0') {
		unwind();
		return nextc();
	    }
	    lin->cno++;
	    if (c == '\n') {
		lin->lno++;
		lin->cno = 0;
	    }
	}
#if PCP_DEBUG && PCP_DESPERATE
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "nextc() -> \'%c\'\n", c);
#endif
	return c;
    }
    return EOF;
}


/* new file input context */
static int
inFile(char *name)
{
    FILE    *f;
    LexIn   *t;

    t = (LexIn *) zalloc(sizeof(LexIn));

    if (name == NULL)
	f = stdin;
    else {
	if ((f = fopen(name, "r")) == NULL) {
	    fprintf(stderr, "%s: cannot open config file %s\n", pmProgname, name);
	    free(t);
	    return 0;
	}
	t->name = (char *) alloc(strlen(name) + 1);
	strcpy(t->name, name);
    }

    t->stream = f;
    t->lno = 1;
    t->cno = 0;
    t->prev = lin;
    lin = t;
    return 1;
}


/* dereference macro */
static int		/* 0 -> error, 1 -> string, 2 -> truth, 3 -> number */
varDeref(char *name)
{
    Symbol  s;
    Expr    *x;
    LexIn   *t;

    /* lookup macro name */
    if ((s = symLookup(&vars, name)) == NULL) {
	fprintf(stderr, "undefined macro name $%s\n", name);
	return 0;
    }
    x = symValue(s);

    /* string macro */
    if (x->sem == SEM_CHAR) {
	t = (LexIn *) zalloc(sizeof(LexIn));
	t->prev = lin;
	lin = t;
	lin->name = (char *) alloc(strlen(name) + 1);
	strcpy(lin->name, name);
	lin->macro = (char *) x->ring;
	lin->lno = 1;
	lin->cno = 0;
	return 1;
    }

    /* truth valued macro */
    if (x->sem == SEM_TRUTH) {
	yylval.x = x;
	return 2;
    }

    /* numeric valued macro */
    yylval.x = x;
    return 3;
}


/* push character into lookahead queue */
static void
prevc(int c)
{
    if (lin) {
	lin->look[lin->lookin++] = c;
	if (lin->lookin >= LEX_MAX + 2)
	    lin->lookin = 0;
    }
}


/* push  string into lookahead queue */
static void
prevs(char *s)
{
    while (*s != '\0') {
	lin->look[lin->lookin++] = *s++;
	if (lin->lookin >= LEX_MAX + 2)
	    lin->lookin = 0;
    }
}

/* get IDENT after '$' ... match rules for IDENT in main scanner */
static int
get_ident(char *namebuf)
{
    int		c;
    int		d = 0;
    int		i;
    char	*namebuf_start = namebuf;

    c = nextc();
    if (c == '\'') {
	d = c;
	c = nextc();
    }
    if (!isalpha(c)) {
	fprintf(stderr, "macro name must begin with an alphabetic (not '%c')\n", c);
	lexSync();
	return 0;
    }
    i = 0;
    do {
	if (c == '\\') c = nextc();
	*namebuf++ = c;
	i++;
	c = nextc();
    } while (i < LEX_MAX && c != EOF &&
	     (isalpha(c) || isdigit(c) || c == '_' || (d && c != d)));
    
    if (i == LEX_MAX) {
	namebuf_start[20] = '\0';
	fprintf(stderr, "macro name too long: $%s...\n", namebuf_start);
	lexSync();
	return 0;
    }
    if (c == EOF) {
	*namebuf = '\0';
	fprintf(stderr, "unexpected end of file in macro name: $%s\n", namebuf_start);
	lexSync();
	return 0;
    }

    if (!d)
	prevc(c);

    *namebuf = '\0';

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "get_ident() -> macro name \"%s\"\n", namebuf_start);
#endif

    return 1;
}


/***********************************************************************
 * exported functions
 ***********************************************************************/

/* initialize scan of new input stream */
int
lexInit(char *fname)
{
    lin = NULL;
    if (! inFile(fname))
	return 0;
    token = (char *) alloc(LEX_MAX + 1);
    return 1;
}


/* finalize scan of input stream */
void
lexFinal(void)
{
    free(token);
}


/* not end of input stream? */
int lexMore(void)
{
    return (lin != NULL);
}


/* discard input to next ';' or EOF */
void
lexSync(void)
{
    int	    c;

    do
	c = nextc();
    while ((c != ';') && (c != EOF));
    prevc(c);
}


/* scanner main function */
int
yylex(void)
{
    int		c, d;			/* current character */
    static int	ahead = 0;		/* lookahead token */
    int		behind = 0;		/* lookbehind token */
    LexEntry1	*lt1;			/* scans through lexbuf1 */
    LexEntry2	*lt2;			/* scans through lexbuf2 */
    char	*p, *q;
    int		i;
    char	nbuf[LEX_MAX+1];	/* for getting macro name */

    /* token from previous invocation */
    if (ahead) {
	c = ahead;
	ahead = 0;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "yylex() -> TOKEN (ahead) \'%c\'\n", c);
#endif
	return c;
    }

    /* scan token from input */
    c = nextc();
    while (c != EOF) {

	/* skip blanks */
	while (isspace(c))
	    c = nextc();

	/* scan C style comment */
	if (c == '/') {
	    if ((d = nextc()) != '*')
		prevc(d);
	    else {
		c = nextc();
		while (c != EOF) {
		    d = nextc();
		    if ((c == '*') && (d == '/')) {
			c = nextc();
			break;
		    }
		    c = d;
		}
		continue;
	    }
	}

	/* scan C++ style comment */
	if (c == '/') {
	    if ((d = nextc()) != '/')
		prevc(d);
	    else {
		do c = nextc();
		while (( c != '\n') && (c != EOF));
		continue;
	    }
	}

	/* scan alphanumeric */
	if (isalpha(c) || (c == '\'') || (c == '%')) {
	    d = c;
	    if (d == '\'') c = nextc();
	    i = 0;
	    do {
		if (c == '$') {
		    /* macro embedded in identifier */
		    int		sts;
		    if (!get_ident(nbuf))
			return EOF;
		    sts = varDeref(nbuf);
		    if (sts == 0) {
			/* error reporting in varDeref() */
			lexSync();
			return EOF;
		    }
		    else if (sts != 1) {
			synerr();
			fprintf(stderr, "macro $%s not string valued as expected\n", nbuf);
			lexSync();
			return EOF;
		    }
		    c = nextc();
		}
		else {
		    if (c == '\\') c = nextc();
		    token[i++] = c;
		    c = nextc();
		}
	    } while ((isalpha(c) || isdigit(c) ||
		      c == '.' || c == '_' || c == '\\' || c == '$' ||
		      (d == '\'' && c != d)) &&
		     (i < LEX_MAX));
	    token[i] = '\0';
	    if (d == '\'') c = nextc();

	    /* recognize units keyword */
	    if (d != '\'') {
		lt2 = &unitab[0];
		if (i > 0 && token[i-1] == 's')
		    i--;
		do {
		    if (strlen(lt2->key) == i &&
			strncmp(token, lt2->key, i) == 0) {

			/* if looking ahead after '/', return UNIT_SLASH */
			if (behind == '/') {
			    prevs(&token[0]);
			    prevc(c);
#if PCP_DEBUG
			    if (pmDebug & DBG_TRACE_APPL0)
				fprintf(stderr, "yylex() -> OPERATOR \"/\"\n");
#endif
			    return UNIT_SLASH;
			}
			prevc(c);

			yylval.u = noUnits;
			switch (lt2->token) {
			case SPACE_UNIT:
			    yylval.u.dimSpace = 1;
			    yylval.u.scaleSpace = lt2->scale;
			    break;
			case TIME_UNIT:
			    yylval.u.dimTime = 1;
			    yylval.u.scaleTime = lt2->scale;
			    break;
			case EVENT_UNIT:
			    yylval.u.dimCount = 1;
			    yylval.u.scaleCount = lt2->scale;
			    break;
			}
#if PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    fprintf(stderr, "yylex() -> UNITS \"%s\"\n", pmUnitsStr(&yylval.u));
#endif
			return lt2->token;
		    }
		    lt2++;
		} while (lt2->key);
	    }

	    /* if looking ahead, return previous token */
	    if (behind) {
		prevs(&token[0]);
		prevc(c);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> TOKEN (behind) \'%c\'\n", behind);
#endif
		return behind;
	    }
	    prevc(c);

	    /* recognize aggregation and quantification */
	    if (d != '\'') {
		if ((p = strchr(token, '_')) != NULL) {
		    *p = '\0';
		    lt1 = &quantab[0];
		    do {
			if (strcmp(&token[0], lt1->key) == 0) {
			    c = lt1->token;
			    q = p + 1;
			    lt1 = &domtab[0];
			    do {
				if (strcmp(q, lt1->key) == 0) {
				    ahead = lt1->token;
#if PCP_DEBUG
				    if (pmDebug & DBG_TRACE_APPL0)
					fprintf(stderr, "yylex() -> OPERATOR \"%s\'\n", token);
#endif
				    return c;
				}
				lt1++;
			    } while (lt1->key);
			    break;
			}
			lt1++;
		    } while (lt1->key);
		    *p = '_';
		}

		/* recognize other reserved word */
		lt1 = &optab[0];
		do {
		    if (strcmp(&token[0], lt1->key) == 0) {
#if PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    fprintf(stderr, "yylex() -> RESERVED-WORD \"%s\"\n", token);
#endif
			return lt1->token;
		    }
		    lt1++;
		} while (lt1->key);
	    }

	    /* recognize identifier */
	    yylval.s = (char *) alloc(strlen(&token[0]) + 1);
	    strcpy(yylval.s, &token[0]);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> IDENT \"%s\"\n", token);
#endif
	    return IDENT;
	}

	/* if looking ahead, return preceding token */
	if (behind) {
	    prevc(c);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> TOKEN (behind) \'%c\'\n", behind);
#endif
	    return behind;
	}

	/* special case for .[0-9]... number without leading [0-9] */
	if (c == '.') {
	    c = nextc();
	    if (isdigit(c)) {
		/* note prevc() implements a FIFO, not a stack! */
		prevc('.');	/* push back period */
		prevc(c);	/* push back digit */
		c = '0';	/* start with fake leading zero */
	    }
	    else
		prevc(c);	/* not a digit after period, push back */
	}

	/* scan NUMBER */
	if (isdigit(c)) {
	    int	flote = 0;
	    i = 0;
	    do {
		token[i++] = c;
		c = nextc();
		if ((flote == 0) && (c == '.') && (i < LEX_MAX)) {
		    c = nextc();
		    if (c == '.')
			prevc(c);	/* INTERVAL token */
		    else {
			flote = 1;
			token[i++] = '.';
		    }
		}
		if ((flote <= 1) && (i < LEX_MAX) && ((c == 'e') || (c == 'E'))) {
		    flote = 2;
		    token[i++] = c;
		    c = nextc();
		}
		if ((flote <= 2) && (c == '-') && (i < LEX_MAX)) {
		    flote = 3;
		    token[i++] = c;
		    c = nextc();
		}
	    } while (isdigit(c) && (i < LEX_MAX));
            prevc(c);
            token[i] = '\0';
	    yylval.d = strtod(&token[0], NULL);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> NUMBER %g\n", yylval.d);
#endif
	    return NUMBER;
	}

	/* scan string */
	if (c == '"') {
	    yylval.s = NULL;
	    i = 0;
	    c = nextc();
	    while ((c != '"') && (c != EOF) && (i < LEX_MAX)) {

		/* escape character */
		if (c == '\\') {
		    c = nextc();
		    switch (c) {
		    case 'n':
			c = '\n';
			break;
		    case 't':
			c = '\t';
			break;
		    case 'v':
			c = '\v';
			break;
		    case 'b':
			c = '\b';
			break;
		    case 'r':
			c = '\r';
			break;
		    case 'f':
			c = '\f';
			break;
		    case 'a':
			c = '\a';
			break;
		    }
		}

		/* macro embedded in string */
		else if (c == '$') {
		    int		sts;
		    if (!get_ident(nbuf))
			return EOF;
		    sts = varDeref(nbuf);
		    if (sts == 0) {
			/* error reporting in varDeref() */
			lexSync();
			return EOF;
		    }
		    else if (sts != 1) {
			synerr();
			fprintf(stderr, "macro $%s not string valued as expected\n", nbuf);
			lexSync();
			return EOF;
		    }
		    c = nextc();
		}

		/* add character to string */
		yylval.s = (char *) ralloc(yylval.s, i+2);
		yylval.s[i++] = c;
		c = nextc();
	    }
	    if (i == 0) {
		/* special case for null string */
		yylval.s = (char *) ralloc(yylval.s, 1);
	    }
	    yylval.s[i++] = '\0';
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> STRING \"%s\"\n", yylval.s);
#endif
	    return STRING;
	}

	/* scan operator */
	switch (c) {
	case ';':
            do
                d = nextc();
            while (isspace(d));
            if (d == '.') {
		while (lin)
		    unwind();
            }
            else
                prevc(d);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> END-OF-RULE\n");
#endif
            return EOF;

	case '$':
	    if (!get_ident(nbuf))
		return EOF;
	    switch (varDeref(nbuf)) {
	    case 0:
		lexSync();
		return EOF;
	    case 1:
		c = nextc();
		continue;
	    case 2:
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> (boolean) macro $%s\n", nbuf);
#endif
		return VAR;
	    case 3:
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> (numeric) macro $%s\n", nbuf);
#endif
		return VAR;
	    }
	case '/':
	    behind = c;
	    c = nextc();
	    continue;
	case '-':
	    if ((d = nextc()) == '>') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"->\"\n");
#endif
		return ARROW;
	    }
	    prevc(d);
	    break;
	case '=':
	    if ((d = nextc()) == '=') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"==\"\n");
#endif
		return EQ_REL;
	    }
	    prevc(d);
	    break;
	case '!':
	    if ((d = nextc()) == '=') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"!=\"\n");
#endif
		return NEQ_REL;
	    }
	    prevc(d);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> OPERATOR \"!\"\n");
#endif
	    return NOT;
	case '<':
	    if ((d = nextc()) == '=') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"<=\"\n");
#endif
		return LEQ_REL;
	    }
	    prevc(d);
	    break;
	case '>':
	    if ((d = nextc()) == '=') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \">=\"\n");
#endif
		return GEQ_REL;
	    }
	    prevc(d);
	    break;
	case '&':
	    if ((d = nextc()) == '&') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"&&\"\n");
#endif
		return AND;
	    }
	    prevc(d);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> OPERATOR \"&\"\n");
#endif
	    return SEQ;
	case '|':
	    if ((d = nextc()) == '|') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"||\"\n");
#endif
		return OR;
	    }
	    prevc(d);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "yylex() -> OPERATOR \"|\"\n");
#endif
	    return ALT;
	case '.':
	    if ((d = nextc()) == '.') {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "yylex() -> OPERATOR \"..\"\n");
#endif
		return INTERVAL;
	    }
	    prevc(d);
	    break;

	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    if (c == EOF)
		fprintf(stderr, "yylex() -> EOF\n");
	    else
		fprintf(stderr, "yylex() -> TOKEN \'%c\' (0x%x)\n", c, c & 0xff);
#endif
	return c;
    }

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "yylex() -> EOF\n");
#endif
    return EOF;
}



