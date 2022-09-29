/*--------------------------------------------------------------------*/
/*                                                                    */
/* Copyright 1992-1998 Silicon Graphics, Inc.                         */
/* All Rights Reserved.                                               */
/*                                                                    */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics    */
/* Inc.; the contents of this file may not be disclosed to third      */
/* parties, copied or duplicated in any form, in whole or in part,    */
/* without the prior written permission of Silicon Graphics, Inc.     */
/*                                                                    */
/* RESTRICTED RIGHTS LEGEND:                                          */
/* Use, duplication or disclosure by the Government is subject to     */
/* restrictions as set forth in subdivision (c)(1)(ii) of the Rights  */
/* in Technical Data and Computer Software clause at                  */
/* DFARS 252.227-7013, and/or in similar or successor clauses in      */
/* the FAR, DOD or NASA FAR Supplement.  Unpublished - rights         */
/* reserved under the Copyright Laws of the United States.            */
/*                                                                    */
/*--------------------------------------------------------------------*/

%{

#include  <lexyacc.h>

extern char *sqlstr;

#undef  input
#undef  unput
#define input()   (*sqlstr++)
#define unput(c)  (*--sqlstr = c) 

extern YYSTYPE yylval;
void  yyerror(char *);
extern FILE *yyout;
extern char *cmd;
extern errorexit();


%}

sqstring		'(\\.|[^'\\]+)*'
dqstring		\"(\\.|[^\"\\]+)*\"
signednum		\-[0-9]+|\-[0-9]+"."[0-9]*|\-"."[0-9]*

%%
[ \t\r]					;

[0-9]+|[0-9]+"."[0-9]*|"."[0-9]*	{
					    yylval.string=strdup(yytext);
					    return INTNUM;
					}

{signednum}				{
					    yylval.string=strdup(yytext);
					    return INTNUM;
					}

{sqstring}				{
					    yylval.string=strdup(yytext+1);
					    yylval.string[yyleng-2] = '\0';
					    return STRING;
					}

{dqstring}				{
					    yylval.string=strdup(yytext+1);
					    yylval.string[yyleng-2] = '\0';
					    return STRING;
					}

[Nn][Uu][Ll][Ll]			{
					    yylval.string=strdup(yytext);
					    return NOVAL;
					}

'[^'\n]*$				{
					    yyerror("Unterminated String");
					}

[,;.()\*]                               { return yytext[0]; }

.					{ yyerror("Error"); }
%%

void yyerror(char *s) {
    errorexit(cmd, 0, 7, "Parse error at : %s", sqlstr);
}
