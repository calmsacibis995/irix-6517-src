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

#ident "$Id: lex.l,v 2.5 1997/09/12 01:08:03 markgw Exp $"

%{

#undef input
#undef unput
#undef yywrap

#include "gram.h"
#include "pmapi.h"
#include "impl.h"
#include "./pmlc.h"

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
"@"	{ return AT; }

milliseconds?	{ return ctx(MSEC); }
mandatory	{ return ctx(MANDATORY); }
advisory	{ return ctx(ADVISORY); }
timezone	{ return ctx(TIMEZONE); }
loggers?	{ return ctx(LOGGER); }
minutes?	{ return ctx(MINUTE); }
seconds?	{ return ctx(SECOND); }
connect		{ return ctx(CONNECT); }
primary		{ return ctx(PRIMARY); }
status		{ return ctx(STATUS); }
volume		{ return ctx(VOLUME); }
flush		{ return ctx(SYNC); }
every		{ return ctx(EVERY); }
local		{ return ctx(LOCAL); }
maybe		{ return ctx(MAYBE); }
query		{ return ctx(QUERY); }
hours?		{ return ctx(HOUR); }
msecs?		{ return ctx(MSEC); }
mins?		{ return ctx(MINUTE); }
secs?		{ return ctx(SECOND); }
help		{ return ctx(HELP); }
once		{ return ctx(ONCE); }
port		{ return ctx(PORT); }
quit		{ return ctx(QUIT); }
show		{ return ctx(SHOW); }
sync		{ return ctx(SYNC); }
log		{ return ctx(LOG); }
new		{ return ctx(NEW); }
off		{ return ctx(OFF); }
on		{ return ctx(ON); }
at		{ return ctx(AT); }
h		{ return ctx(HELP); }
q		{ return ctx(QUIT); }
\?		{ return ctx(HELP); }

[A-Za-z][A-Za-z0-9_.]* { return NAME; }

[A-Za-z][A-Za-z0-9_.-]* { return HOSTNAME; }

\"[^\"\n][^\"\n]*\"	{ return STRING; }

[0-9]+			{ mynumb = atol(yytext); return NUMBER; }

\#.*\n			{ }

\n			{ if (!(mystate & INSPECLIST))
			    return EOL;
			}

[ \t]+			{ }

.			{
			    sprintf(emess, "Unexpected character '%c'", yytext[0]);
			    yyerror(emess);
			    /*NOTREACHED*/
			}
%%
