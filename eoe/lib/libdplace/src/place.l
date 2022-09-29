D	[0-9]
X	[0-9a-fA-F]
O	[0-7]
%{
#include <stdlib.h>
#include <string.h>
#include "place_defs.h"
#include "y.tab.h"
static int
yywrap(void)
{ 
  return 1;
}
void *__dplace_malloc_check(size_t);
static char *dplace_filename;
extern int __dplace_fileio;
extern char *__dplace_command;
#ifdef input
#undef input
#endif /*input*/
#define input() (__dplace_fileio==1?(((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar):((yytchar=yysptr>yysbuf?U(*--yysptr):*__dplace_command++)==10?(yylineno++,yytchar):yytchar))

%}

%%
{D}+		|
0{O}+		|
0(x|X){X}+	{ yylval.x = strtoll(yytext,(char **)NULL,0);
		return(NUMBER);}
#[^\n]*	return COMMENT;
memories	return NODES;
memory	return NODE;
in	return IN;
cube	{ yylval.x = cube; return TOPOLOGY_TYPE; }
cluster	{ yylval.x = cluster; return TOPOLOGY_TYPE; }
none	{ yylval.x = none; return TOPOLOGY_TYPE; }
physical	{ yylval.x = physical; return TOPOLOGY_TYPE; }
near	return NEAR;
range	return RANGE;
place	return PLACE;
threads	return THREADS;
thread	return THREAD;
run	return RUN;
move	return MOVE;
link	return LINK;
on	return ON;
off	return OFF;
toggle	return TOGGLE;
to	return TO;
pid	return PID;
with	return WITH;
global  return GLOBAL;
policy	return POLICY;
stack	return STACK;
data	return DATA;
text	return TEXT;
migration	return MIGRATION;
migrate	return MIGRATE;
mode	return MODE;
verbose	return VERBOSE;
distribute	return DISTRIBUTE;
block	return BLOCK;
cyclic	return CYCLIC;
across	return ACROSS;
using	return USING;
cpu	return CPU;
level	return LEVEL;
(k|K)	return K;
(m|M)	return M;
"%"	return PERCENT;
topology	return TOPOLOGY;
pagesize	return PAGESIZE;
pagewait	return PAGEWAIT;
placement	return PLACEMENT;
"$"[a-zA-Z0-9_]+	{
			char *c;
			char ctmp[256];
			strncpy(ctmp, yytext +1 , yyleng - 1);
			ctmp[yyleng-1] = '\0';
			c = getenv(ctmp);
			if( c ){ 
				char *s;
				yylval.x = strtoll(c,&s,0);
				if( c == s ){
					yylval.s = c;
					return STRING;
				} else
				return(NUMBER);
			}
			fprintf(stderr,"Warning: %s is not defined. Using zero.\n",ctmp);
			yylval.x = 0;
			return NUMBER;
			}
\"[a-zA-Z0-9_/]+\"	{ char *ctmp = __dplace_malloc_check(yyleng-1);
				strncpy(ctmp, yytext+1, yyleng -2);
				ctmp[yyleng-2] = '\0';
				yylval.s = ctmp;
				return STRING;
			}
"/"[a-zA-Z][a-zA-Z0-9.\/_+-]*	{ dplace_filename = (char*)
			__dplace_malloc_check(1+strlen(yytext));
        		strcpy(dplace_filename,yytext)	;
			yylval.s = dplace_filename;
        		return(FILENAME)	;}
[ \t]			;
\\[ \t]*\n		;
\n	|
.	return(yytext[0]);
%%

