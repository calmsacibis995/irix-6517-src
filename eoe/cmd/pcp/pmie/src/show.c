/***********************************************************************
 * show.c - display expressions and their values
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

#ident "$Id: show.c,v 1.2 1999/05/25 10:52:35 kenmcd Exp $"

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "show.h"
#include "impl.h"
#include "dstruct.h"
#include "lexicon.h"
#include "pragmatics.h"
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif

/***********************************************************************
 * local declarations
 ***********************************************************************/

static struct {
    int		op;
    char	*str;
} opstr[] = {
	{ RULE,	"->" },
	{ CND_FETCH,	"<fetch node>" },
	{ CND_DELAY,	"<delay node>" },
	{ CND_RATE,	"rate" },
	{ CND_NEG,	"-" },
	{ CND_ADD,	"+" },
	{ CND_SUB,	"-" },
	{ CND_MUL,	"*" },
	{ CND_DIV,	"/" },
/* aggregation */
	{ CND_SUM_HOST,	"sum_host" },
	{ CND_SUM_INST,	"sum_inst" },
	{ CND_SUM_TIME,	"sum_sample" },
	{ CND_AVG_HOST,	"avg_host" },
	{ CND_AVG_INST,	"avg_inst" },
	{ CND_AVG_TIME,	"avg_sample" },
	{ CND_MAX_HOST,	"max_host" },
	{ CND_MAX_INST,	"max_inst" },
	{ CND_MAX_TIME,	"max_sample" },
	{ CND_MIN_HOST,	"min_host" },
	{ CND_MIN_INST,	"min_inst" },
	{ CND_MIN_TIME,	"min_sample" },
/* relational */
	{ CND_EQ,	"==" },
	{ CND_NEQ,	"!=" },
	{ CND_LT,	"<" },
	{ CND_LTE,	"<=" },
	{ CND_GT,	">" },
	{ CND_GTE,	">=" },
/* boolean */
	{ CND_NOT,	"!" },
	{ CND_RISE,	"rising" },
	{ CND_FALL,	"falling" },
	{ CND_AND,	"&&" },
	{ CND_OR,	"||" },
	{ CND_MATCH,	"match_inst" },
	{ CND_NOMATCH,	"nomatch_inst" },
/* quantification */
	{ CND_ALL_HOST,	"all_host" },
	{ CND_ALL_INST,	"all_inst" },
	{ CND_ALL_TIME,	"all_sample" },
	{ CND_SOME_HOST,	"some_host" },
	{ CND_SOME_INST,	"some_inst" },
	{ CND_SOME_TIME,	"some_sample" },
	{ CND_PCNT_HOST,	"pcnt_host" },
	{ CND_PCNT_INST,	"pcnt_inst" },
	{ CND_PCNT_TIME,	"pcnt_sample" },
	{ CND_COUNT_HOST,	"count_host" },
	{ CND_COUNT_INST,	"count_inst" },
	{ CND_COUNT_TIME,	"count_sample" },
	{ ACT_SEQ,	"&" },
	{ ACT_ALT,	"|" },
	{ ACT_SHELL,	"shell" },
	{ ACT_ALARM,	"alarm" },
	{ ACT_SYSLOG,	"syslog" },
	{ ACT_PRINT,	"print" },
	{ ACT_ARG,	"<action arg node>" },
	{ NOP,		"<nop node>" },
	{ OP_VAR,	"<op_var node>" },
};

static numopstr = sizeof(opstr) / sizeof(opstr[0]);

/***********************************************************************
 * local utility functions
 ***********************************************************************/

/* Concatenate string1 to existing string2 whose original length is given. */
static size_t	 /* new length of *string2 */
concat(char *string1, size_t pos, char **string2)
{
    size_t	slen;
    size_t	tlen;
    char	*cat;
    char	*dog;

    if ((slen = strlen(string1)) == 0)
	return pos;
    tlen = pos + slen;
    cat = (char *) ralloc(*string2, tlen + 1);
    dog = cat + pos;
    strcpy(dog, string1);
    dog += slen;
    *dog = '\0';

    *string2 = cat;
    return tlen;
}


/***********************************************************************
 * host and instance names
 ***********************************************************************/

/* Return host and instance name for nth value in expression *x */
static int
lookupHostInst(Expr *x, int nth, char **host, char **inst)
{
    Metric	*m;
    int		mi;
    int		sts = 0;
#if PCP_DEBUG
    static Expr	*lastx = NULL;
    int		dbg_dump;
#endif

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	if (x != lastx) {
	    fprintf(stderr, "lookupHostInst(x=0x%p, nth=%d, ...)\n", x, nth);
	    lastx = x;
	    dbg_dump = 1;
	}
	else
	    dbg_dump = 0;
    }
#endif

    /* check for no host and instance available e.g. constant expression */
    if ((x->e_idom <= 0 && x->hdom <= 0) || ! x->metrics) {
	*host = NULL;
	*inst = NULL;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "lookupHostInst(x=0x%p, nth=%d, ...) -> %%h and %%i undefined\n", x, nth);
	}
#endif
	return sts;
    }

    /* find Metric containing the nth instance */
    mi = 0;
    for (;;) {
	m = &x->metrics[mi];
#if PCP_DEBUG
	if ((pmDebug & DBG_TRACE_APPL2) && dbg_dump) {
	    fprintf(stderr, "lookupHostInst: metrics[%d]\n", mi);
	    dumpMetric(m);
	}
#endif
	if (nth < m->m_idom)
	    break;
	if (m->m_idom > 0)
	    nth -= m->m_idom;
	mi++;
    }

    /* host and instance names */
    *host = symName(m->hname);
    sts++;
    if (x->e_idom > 0 && m->inames) {
	*inst = m->inames[nth];
	sts++;
    }
    else
	*inst = NULL;

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "lookupHostInst(x=0x%p, nth=%d, ...) -> sts=%d %%h=%s %%i=%s\n",
	    x, nth, sts, *host, *inst == NULL ? "undefined" : *inst);
    }
#endif

    return sts;
}


/***********************************************************************
 * expression value
 ***********************************************************************/

#define	TRUTH_SPACE 7

static size_t
showTruth(Expr *x, int nth, size_t length, char **string)
{
    int		smpl;
    size_t	tlen;
    char	*cat;
    char	*dog;
    int		val;

    tlen = length + (x->nsmpls * TRUTH_SPACE);
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    for (smpl = 0; smpl < x->nsmpls; smpl++) {
	if (smpl > 0) {
	    strcpy(dog, " ");
	    dog += 1;
	}

	if (x->valid == 0) {
	    strncpy(dog, "?", TRUTH_SPACE);
	    dog++;
	    continue;
	}

	val = *((char *)x->smpls[smpl].ptr + nth);
	if (val == FALSE) {
	    strncpy(dog, "false", TRUTH_SPACE);
	    dog += 5;
	}
	else if (val == TRUE) {
	    strncpy(dog, "true", TRUTH_SPACE);
	    dog += 4;
	}
	else if (val == DUNNO) {
	    strncpy(dog, "?", TRUTH_SPACE);
	    dog++;
	}
	else {
	    sprintf(dog, "0x%02x?", val & 0xff);
	    dog += 5;
	}
    }
    *dog = '\0';

    *string = cat;
    return dog - cat;
}


static size_t
showString(Expr *x, size_t length, char **string)
{
    size_t	tlen;
    char	*cat;
    char	*dog;

    tlen = length + (x->nvals - 1) + 2;
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    *dog++ = '"';
    strcpy(dog, (char *)x->smpls[0].ptr);
    dog += x->nvals - 1;
    *dog++ = '"';
    *dog = '\0';

    *string = cat;
    return tlen;
}

#define	DBL_SPACE 24

static size_t
showNum(Expr *x, int nth, size_t length, char **string)
{
    int		smpl;
    size_t	tlen;
    char	*cat;
    char	*dog;
    char	*fmt;
    double	v;
    double	abs_v;
    int		sts;

    tlen = length + (x->nsmpls * DBL_SPACE);
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    for (smpl = 0; smpl < x->nsmpls; smpl++) {
	if (smpl > 0) {
	    strcpy(dog, " ");
	    dog += 1;
	}
	if (x->valid <= smpl || isnand(*((double *)x->smpls[smpl].ptr + nth))) {
	    strcpy(dog, "?");
	    dog += 1;
	}
	else {
	    v = *((double *)x->smpls[smpl].ptr+nth);
	    if (v == (int)v)
		sts = sprintf(dog, "%d", (int)v);
	    else {
		abs_v = v < 0 ? -v : v;
		if (abs_v < 0.5)
		    fmt = "%g";
		else if (abs_v < 5)
		    fmt = "%.2f";
		else if (abs_v < 50)
		    fmt = "%.1f";
		else
		    fmt = "%.0f";
		sts = sprintf(dog, fmt, v);
	    }
	    if (sts > 0)
		dog += sts;
	    else {
		strcpy(dog, "!");
		dog += 1;
	    }
	}
    }
    *dog = '\0';

    *string = cat;
    return dog - cat;
}

static char *
showConst(Expr *x)
{
    char	*string = NULL;
    size_t	length = 0;
    int		i;
    int		first = 1;

    /* construct string representation */
    if (x->nvals > 0) {
	for (i = 0; i < x->tspan; i++) {
	    if (first) 
		first = 0;
	    else
		length = concat(" ", length, &string);
	    if (x->sem == SEM_TRUTH)
		length = showTruth(x, i, length, &string);
	    else if (x->sem == SEM_CHAR)
		length = showString(x, length, &string);
	    else
		length = showNum(x, i, length, &string);
	}
    }
    return string;
}



/***********************************************************************
 * expression syntax
 ***********************************************************************/

static void
showSyn(FILE *f, Expr *x)
{
    char	*s;
    char	*c;
    Metric	*m;
    char	**n;
    int		i;

    /* constant */
    if (x->op >= NOP) {
	s = showConst(x);
	if (s) {
	    c = s;
	    while(isspace(*c))
		c++;
	    fputs(c, f);
	    free(s);
	}
    }

    /* fetch expression (perhaps with delay) */
    else if ((x->op == CND_FETCH) || (x->op == CND_DELAY)) {
	m = x->metrics;
	fprintf(f, "%s", symName(m->mname));
	for (i = 0; i < x->hdom; i++) {
	    fprintf(f, " :%s", symName(m->hname));
	    m++;
	}
	m = x->metrics;
	if (m->inames) {
	    n = m->inames;
	    for (i = 0; i < m->m_idom; i++) {
		fprintf(f, " #%s", *n);
		n++;
	    }
	}
	if (x->op == CND_FETCH) {
	    if (x->tdom == 1) fprintf(f, " @0");
	    else fprintf(f, " @0..%d", x->tdom - 1);
	}
	else {
	    if (x->tdom == x->arg1->tdom - 1) fprintf(f, " @%d", x->tdom);
	    else fprintf(f, " @%d..%d", x->tdom, x->tdom + x->arg1->tdom - 1);
	}
    }

    /* binary operator */
    else if (x->arg1 && x->arg2) {
	if (x->op >= ACT_SHELL && x->op < ACT_ARG) {
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    showSyn(f, x->arg2);
	    fputc(' ', f);
	    showSyn(f, x->arg1);
	}
	else if (x->op >= CND_PCNT_HOST && x->op <= CND_PCNT_TIME) {
	    showSyn(f, x->arg2);
	    fputc(' ', f);
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    if (x->arg1->op >= NOP || x->arg1->op <= CND_DELAY)
		showSyn(f, x->arg1);
	    else {
		fputc('(', f);
		showSyn(f, x->arg1);
		fputc(')', f);
	    }
	}
	else {
	    if (x->arg1->op >= NOP || x->arg1->op <= CND_DELAY)
		showSyn(f, x->arg1);
	    else {
		fputc('(', f);
		showSyn(f, x->arg1);
		fputc(')', f);
	    }
	    fputc(' ', f);
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    if (x->arg2->op >= NOP || x->arg2->op <= CND_DELAY)
		showSyn(f, x->arg2);
	    else {
		fputc('(', f);
		showSyn(f, x->arg2);
		fputc(')', f);
	    }
	}
    }

    /* unary operator */
    else {
	fputs(opStrings(x->op), f);
	fputc(' ', f);
	if (x->arg1->op >= ACT_ARG || x->arg1->op <= CND_DELAY)
	    showSyn(f, x->arg1);
	else {
	    fputc('(', f);
	    showSyn(f, x->arg1);
	    fputc(')', f);
	}
    }
}

/*
 * recursive descent to find a conjunct from the root of
 * the expression that has associated metrics (not constants)
 */
static Expr *
findMetrics(Expr *y)
{
    Expr	*z;

    if (y == NULL) return NULL;
    if (y->metrics) return y;		/* success */

    /* give up if not a conjunct */
    if (y->op != CND_AND) return NULL;

    /* recurse left and then right */
    z = findMetrics(y->arg1);
    if (z != NULL) return z;
    return findMetrics(y->arg2);
}

/***********************************************************************
 * satisfying bindings and values
 ***********************************************************************/

/* Find sub-expression that reveals host and instance bindings
   that satisfy the given expression *x. */
static Expr *
findBindings(Expr *x)
{
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "call findBindings(x=0x%p)\n", x);
    }
#endif

    if (x->metrics == NULL) {
	/*
	 * this Expr node has no metrics (involves only constants)
	 * ... try and find a conjunct at the top level that has
	 * associated metrics
	 */
	Expr	*y = findMetrics(x->arg1);
	if (y != NULL) x = y;
    }
    while (x->metrics && (x->e_idom <= 0 || x->hdom <= 0)) {
	if (x->arg1 && x->metrics == x->arg1->metrics)
	    x = x->arg1;
	else if (x->arg2)
	    x = x->arg2;
	else
	    break;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "findBindings: try x=0x%p\n", x);
	}
#endif
    }
    return x;
}


/* Find sub-expression that reveals the values that satisfy the
   given expression *x. */
static Expr *
findValues(Expr *x)
{
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "call findValues(x=0x%p)\n", x);
    }
#endif
    while (x->sem == SEM_TRUTH && x->metrics) {
	if (x->metrics == x->arg1->metrics)
	    x = x->arg1;
	else
	    x = x->arg2;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "findValues: try x=0x%p\n", x);
	}
#endif
    }
    return x;
}


/***********************************************************************
 * format string
 ***********************************************************************/

/* Locate next %h, %i or %v in format string. */
static int	/* 0 -> not found, 1 -> host, 2 -> inst, 3 -> value */
findFormat(char *format, char **pos)
{
    for (;;) {
	if (*format == '\0')
	    return 0;
	if (*format == '%') {
	    switch (*(format + 1)) {
	    case 'h':
		*pos = format;
		return 1;
	    case 'i':
		*pos = format;
		return 2;
	    case 'v':
		*pos = format;
		return 3;
	    }
	}
	format++;
    }

}


/***********************************************************************
 * exported functions
 ***********************************************************************/

void
showSyntax(FILE *f, Symbol s)
{
    char *name = symName(s);
    Expr *x = symValue(s);

    fprintf(f, "%s =\n", name);
    showSyn(f, x);
    fprintf(f, ";\n\n");
}


void
showSubsyntax(FILE *f, Symbol s)
{
    char *name = symName(s);
    Expr *x = symValue(s);
    Expr *x1;
    Expr *x2;

    fprintf(f, "%s (subexpression for %s, %s and %s bindings) =\n",
	   name, "%h", "%i", "%v");
    x1 = findBindings(x);
    x2 = findValues(x1);
    showSyn(f, x2);
    fprintf(f, "\n\n");
}


/* Print value of expression */
void
showValue(FILE *f, Expr *x)
{
    char    *string = NULL;

    string = showConst(x);
    if (string) {
	fputs(string, f);
	free(string);
    }
    else
	fputs("?", f);
}


/* Print value of expression together with any host and instance bindings */
void
showAnnotatedValue(FILE *f, Expr *x)
{
    char    *string = NULL;
    size_t  length = 0;
    char    *host;
    char    *inst;
    int	    i;

    /* no annotation possible */
    if ((x->e_idom <= 0 && x->hdom <= 0) ||
	 x->sem == SEM_CHAR ||
	 x->metrics == NULL ||
	 x->valid == 0) {
	showValue(f, x);
	return;
    }

    /* construct string representation */
    for (i = 0; i < x->tspan; i++) {
	length = concat("\n    ", length, &string);
	lookupHostInst(x, i, &host, &inst);
	length = concat(host, length,  &string);
	if (inst) {
	    length = concat(": [", length,  &string);
	    length = concat(inst, length,  &string);
	    length = concat("] ", length,  &string);
	}
	else
	    length = concat(": ", length,  &string);
	if (x->sem == SEM_TRUTH)
	    length = showTruth(x, i, length, &string);
	else	/* numeric value */
	    length = showNum(x, i, length, &string);
    }

    /* print string representation */
    if (string) {
	fputs(string, f);
	free(string);
    }
}


void
showTime(FILE *f, RealTime rt)
{
    time_t t = (time_t)rt;
    char   bfr[26];

    pmCtime(&t, bfr);
    bfr[24] = '\0';
    fprintf(f, "%s", bfr);
}


void
showSatisfyingValue(FILE *f, Expr *x)
{
    char    *string = NULL;
    size_t  length = 0;
    char    *host;
    char    *inst;
    int	    i;
    Expr    *x1;
    Expr    *x2;

    /* no satisfying values possible */
    if (x->metrics == NULL || x->valid == 0) {
	showValue(f, x);
	return;
    }

    if (x->sem != SEM_TRUTH) {
	showAnnotatedValue(f, x);
	return;
    }

    x1 = findBindings(x);
    x2 = findValues(x1);

    /* construct string representation */
    for (i = 0; i < x1->tspan; i++) {
	if (x1->valid &&
	    (x1->sem == SEM_TRUTH && *((char *)x1->smpls[0].ptr + i) == TRUE)) {
	    length = concat("\n    ", length, &string);
	    lookupHostInst(x1, i, &host, &inst);
	    length = concat(host, length,  &string);
	    if (inst) {
		length = concat(": [", length,  &string);
		length = concat(inst, length,  &string);
		length = concat("] ", length,  &string);
	    }
	    else
		length = concat(": ", length,  &string);
	    if (x2->sem == SEM_TRUTH)
		length = showTruth(x2, i, length, &string);
	    else	/* numeric value */
		length = showNum(x2, i, length, &string);
	}
    }

    /* print string representation */
    if (string) {
	fputs(string, f);
	free(string);
    }
}


/* Instantiate format string for each satisfying binding and value
   of the current rule.
   WARNING: This is not thread safe, it dinks with the format string. */
size_t	/* new length of string */
formatSatisfyingValue(char *format, size_t length, char **string)
{
    char    *host;
    char    *inst;
    char    *first;
    char    *prev;
    char    *next;
    int     i;
    Expr    *x1;
    Expr    *x2;
    int	    sts1;
    int	    sts2;

    /* no formatting present? */
    if ((sts1 = findFormat(format, &first)) == 0)
	return concat(format, length, string);

    x1 = findBindings(curr);
    x2 = findValues(x1);

    for (i = 0; i < x1->tspan; i++) {
	if (x1->valid && 
	    (x1->sem == SEM_TRUTH && *((char *)x1->smpls[0].ptr + i) == TRUE)) {
	    prev = format;
	    next = first;
	    sts2 = sts1;
	    lookupHostInst(x1, i, &host, &inst);
	    do {
		*next = '\0';
		length = concat(prev, length, string);
		*next = '%';

		switch (sts2) {
		case 1:
		    if (host)
			length = concat(host, length, string);
		    else
			length = concat("??? unknown %h", length, string);
		    break;
		case 2:
		    if (inst)
			length = concat(inst, length, string);
		    else
			length = concat("??? unknown %i", length, string);
		    break;
		case 3:
		    if (x2->sem == SEM_TRUTH)
			length = showTruth(x2, i, length, string);
		    else	/* numeric value */
			length = showNum(x2, i, length, string);
		    break;
		}
		prev = next + 2;
	    } while (sts2 = findFormat(prev, &next));
	    length = concat(prev, length, string);
	}
    }

    return length;
}

char *
opStrings(int op)
{
    int		i;
    /*
     * sizing of "eh" is a bit tricky ...
     * XXXXXXXXX is the number of digits in the largest possible value
     * for "op", to handle the default "<unknown op %d>" case, but also
     * "eh" must be long enough to accommodate the longest string from
     * opstr[i].str ... currently "<action arg node>"
     */
    static char	*eh = "<unknown op XXXXXXXXX>";

    for (i = 0; i < numopstr; i++) {
	if (opstr[i].op == op)
	    break;
    }

    if (i < numopstr)
	return opstr[i].str;
    else {
	sprintf(eh, "<unknown op %d>", op);
	return eh;
    }
}
