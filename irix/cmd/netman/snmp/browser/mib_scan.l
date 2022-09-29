%{
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SMI Lexical Analyzer Generator
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#undef YYLMAX
#include "y.tab.h"
extern int lineNumber;
static void eatComment(void);
#define YYLMAX  30000
%}

%%

"OBJECT IDENTIFIER"	{ yylval.integer = OBJECTID;
			  return OBJECTID;
			}
"::="			return CCEQUALS;
"{"			return LBRACE;
"}"			return RBRACE;
"("			return LPAREN;
")"			return RPAREN;
","			return COMMA;
".."			return DOTDOT;
"OBJECT-TYPE"		return OBJECT_TYPE;
"SYNTAX"		return SYNTAX;
"SEQUENCE"		return SEQUENCE;
"SIZE"                  return SIZE;
"OF"			return OF;
"INDEX"			return INDEX;
"INTEGER"		{ yylval.integer = INTEGER;
                          return INTEGER;
			}
"OCTET STRING"		{ yylval.integer = OCTET_STRING;
			  return OCTET_STRING;
			}
"DisplayString"         { yylval.integer = DISPLAYSTRING;
			  return DISPLAYSTRING;
			}
"NetworkAddress"	{ yylval.integer = IPADDRESS;
			  return IPADDRESS;
			}
"IpAddress"             { yylval.integer = IPADDRESS;
                          return IPADDRESS;
                        }
"Counter"		{ yylval.integer = COUNTER;
			  return COUNTER;
			}
"Gauge"			{ yylval.integer = GAUGE;
			  return GAUGE;
			}
"TimeTicks"		{ yylval.integer = TIMETICKS;
			  return TIMETICKS;
			}
"ACCESS"		{ yylval.integer = ACCESS;
			  return ACCESS;
			}
"read-only"		{ yylval.integer = READ_ONLY;
			  return READ_ONLY;
			}
"read-write"		{ yylval.integer = READ_WRITE;
			  return READ_WRITE;
			}
"write-only"		{ yylval.integer = WRITE_ONLY;
			  return WRITE_ONLY;
			}
"not-accessible"	{ yylval.integer = NOT_ACCESSIBLE;
			  return NOT_ACCESSIBLE;
			}
"STATUS"		return STATUS;
"mandatory"		return MANDATORY;
"optional"		return OPTIONAL;
"obsolete"		return OBSOLETE;
"DEFINITIONS"           return DEFINITIONS;
"BEGIN"                 return BEGIN1;
"DEFVAL"		return DEFVAL;
"deprecated"            return DEPRECATED;
"IMPORTS"               ;  /* ignore */
"END"			;  /* ignore END */
"DESCRIPTION"           return DESCRIPTION;
[a-z][A-Za-z0-9\-/]*	{ yylval.string = strdup(yytext);
				return NAME;	
			}
[A-Z][A-Za-z0-9\-]*	{ yylval.string = strdup(yytext);
				return ID;
			}
[-]*[0-9]+			{ sscanf(yytext, "%d", &yylval.integer);
				return NUMBER;
			}
\"[^\"]*[\"\n]          { yylval.string = strdup(yytext);
				return QSTRING; 
			}
"--"			eatComment();
[ \t]*			/* eat white space */;
"\n"			lineNumber++;

%%

static void
eatComment(void)
{
    while (input() != '\n')
	;
    unput('\n');
}
