/***********************************************************************
 * syntax.h - inference rule language parser
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

/* $Id: syntax.h,v 1.1 1999/04/28 10:06:17 kenmcd Exp $ */

#ifndef SYNTAX_H
#define SYNTAX_H


/***********************************************************************
 * ONLY FOR USE BY: lexicon.c grammar.y
 ***********************************************************************/

typedef struct {
        int     n;              /* number of elements */
        char    **ss;           /* dynamically allocated array */
} StringArray;

typedef struct {
        int     t1;             /* start of interval */
        int     t2;             /* end of interval */
} Interval;

/* parser stack entry */
typedef union {
    int                  i;
    char                *s;
    double               d;
    StringArray          sa;
    Interval             t;
    pmUnits              u;
    Metric              *m;
    Expr                *x;
    Symbol              *z;
} YYSTYPE;

extern YYSTYPE yylval;

/* error reporting */
extern int      errs;                   /* error count */
void yyerror(char *);
void synerr(void);
void synwarn(void);

/* parser actions */
Symbol statement(char *, Expr *);
Expr *ruleExpr(Expr *, Expr *);
Expr *relExpr(int, Expr *, Expr *);
Expr *binaryExpr(int, Expr *, Expr *);
Expr *unaryExpr(int, Expr *);
Expr *domainExpr(int, int, Expr *);
Expr *percentExpr(double, int, Expr *);
Expr *numMergeExpr(int, Expr *);
Expr *boolMergeExpr(int, Expr *);
Expr *fetchExpr(char *, StringArray, StringArray, Interval);
Expr *numConst(double, pmUnits);
Expr *strConst(char *);
Expr *boolConst(Truth);
Expr *numVar(Expr *);
Expr *boolVar(Expr *);
Expr *actExpr(int, Expr *, Expr *);
Expr *actArgExpr(Expr *, Expr *);
Expr *actArgList(Expr *, char *);

/* parse tree */
extern Symbol parse;



/***********************************************************************
 * public
 ***********************************************************************/

/* Initialization to be called at the start of new input file. */
int synInit(char *);

/* parse single statement */
Symbol syntax(void);

#endif /* SYNTAX_H */

