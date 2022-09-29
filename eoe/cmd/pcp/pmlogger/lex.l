/*
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

#ident "$Id: lex.l,v 2.3 1997/12/12 05:01:26 markgw Exp $"

%{

#include "gram.h"
#include "pmapi.h"
#include "impl.h"
#include "./logger.h"

#undef input
#define input() ((yytchar=fgetc(fconfig))=='\n'?(yylineno++,yytchar):(yytchar==EOF?0:yytchar))
#undef unput
#define unput(c) {yytchar=(c);if(yytchar=='\n')yylineno--;ungetc(yytchar, fconfig);}

extern int	mystate;
extern int	mynumb;

static int
ctx(int type)
{
    if (mystate == GLOBAL)
	return type;
    return NAME;
}

%}

%%
"["	{ return LSQB; }
"]"	{ return RSQB; }
","	{ return COMMA; }
"{"	{ return LBRACE; }
"}"	{ return RBRACE; }
":"	{ return COLON; }
";"	{ return SEMICOLON; }

milliseconds?	{ return ctx(MSEC); }
mandatory	{ return ctx(MANDATORY); }
advisory	{ return ctx(ADVISORY); }
disallow	{ return ctx(DISALLOW); }
minutes?	{ return ctx(MINUTE); }
seconds?	{ return ctx(SECOND); }
default		{ return ctx(DEFAULT); }
enquire		{ return ctx(ENQUIRE); }
access		{ return ctx(ACCESS); }
except		{ return ctx(EXCEPT); }
allow		{ return ctx(ALLOW); }
every		{ return ctx(EVERY); }
maybe		{ return ctx(MAYBE); }
hours?		{ return ctx(HOUR); }
msecs?		{ return ctx(MSEC); }
mins?		{ return ctx(MINUTE); }
once		{ return ctx(ONCE); }
secs?		{ return ctx(SECOND); }
log		{ return ctx(LOG); }
all		{ return ctx(ALL); }
off		{ return ctx(OFF); }
on		{ return ctx(ON); }

[A-Za-z][A-Za-z0-9_.]* { return NAME; }

\"[^\"\n][^\"\n]*\"	{ return STRING; }

[0-9]+			{ mynumb = atol(yytext); return NUMBER; }

[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+	{ return IPSPEC; }
[0-9]+\.[0-9]+\.[0-9]+\.\*	{ return IPSPEC; }
[0-9]+\.[0-9]+\.\*	{ return IPSPEC; }
[0-9]+\.\*		{ return IPSPEC; }
\*			{ return IPSPEC; }

\#.*\n			{ }

[ \t\n]+		{ }

.			{
			    sprintf(emess, "Unexpected character '%c'", yytext[0]);
			    yyerror(emess);
			    /*NOTREACHED*/
			}
%%
