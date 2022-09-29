%{
/*	Copyright (c) 1984 AT&T	*/
%}
%{
/*	  All Rights Reserved  	*/
%}

%{
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
%}
%{
/*	The copyright notice above does not evidence any   	*/
%}
%{
/*	actual or intended publication of such source code.	*/
%}

%{
/* #ident	"@(#)cron:att2.l	1.2" */
#ident	"$Revision: 1.2 $"
%}

%{
# define TIME 257
# define NOW 258
# define NOON 259
# define MIDNIGHT 260
# define MINUTE 261
# define HOUR 262
# define DAY 263
# define WEEK 264
# define MONTH 265
# define YEAR 266
# define UNIT 267
# define SUFF 268
# define AM 269
# define PM 270
# define ZULU 271
# define NEXT 272
# define NUMB 273
# define COLON 274
# define COMMA 275
# define PLUS 276
# define TZ_NAME 277
# define UTC 278
# define UNKNOWN 279

#define	LL(t,v)	return(yylval = v, t)
#undef getc
#define	getc(x)	(*argp?tolower(*argp++):EOF)
#undef feof
#define	feof(x)	(*argp?0:1)
extern	int yylval;
char	*argp = "";
%}
%%
[ \t\n]+		;
"jan"|"january"		{ LL(MONTH, 0); }
"feb"|"february"	{ LL(MONTH, 1); }
"mar"|"march"		{ LL(MONTH, 2); }
"apr"|"april"		{ LL(MONTH, 3); }
"may"			{ LL(MONTH, 4); }
"jun"|"june"		{ LL(MONTH, 5); }
"jul"|"july"		{ LL(MONTH, 6); }
"aug"|"august"		{ LL(MONTH, 7); }
"sep"|"september"	{ LL(MONTH, 8); }
"oct"|"october"		{ LL(MONTH, 9); }
"nov"|"november"	{ LL(MONTH, 10); }
"dec"|"december"	{ LL(MONTH, 11); }
"sun"|"sunday"		{ LL(DAY, 0); }
"mon"|"monday"		{ LL(DAY, 1); }
"tue"|"tuesday"		{ LL(DAY, 2); }
"wed"|"wednesday"	{ LL(DAY, 3); }
"thu"|"thursday"	{ LL(DAY, 4); }
"fri"|"friday"		{ LL(DAY, 5); }
"sat"|"saturday"	{ LL(DAY, 6); }
"today"			{ LL(DAY, 7); }
"tomorrow"		{ LL(DAY, 8); }
"noon"			{ LL(TIME, NOON); }
"midnight"		{ LL(TIME, MIDNIGHT); }
"now"			{ LL(TIME, NOW); }
"am"			{ LL(SUFF, AM); }
"pm"			{ LL(SUFF, PM); }
"zulu"			{ LL(SUFF, ZULU); }
"utc"			{ LL(TZ_NAME, UTC); }		
"next"			{ LL(NEXT, 1); }
"min"s?|"minute"s?		{ LL(UNIT, MINUTE); }
"hour"s?		{ LL(UNIT, HOUR); }
"day"s?			{ LL(UNIT, DAY); }
"week"s?		{ LL(UNIT, WEEK); }
"month"s?		{ LL(UNIT, MONTH); }
"year"s?		{ LL(UNIT, YEAR); }
[0-9]			{ yylval = yytext[0] - '0'; return(NUMB); }
[:]			{ LL(COLON, 0); }
[,]			{ LL(COMMA, 0); }
[+]			{ LL(PLUS, 0); }
.			{ LL(UNKNOWN, 0); }
%%
