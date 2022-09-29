%{
/*
 * Lex spec for unifdef's #if expression scanner.
 */
#include <stdlib.h>
#include "unifdef.h"
#include "unif_scan.h"

char	*nextcp;		/* next char in input string */
int	expanding;		/* true if expanding a macro */
char	*blankptr;		/* pointer to leading comments/space */
int	blanklen;		/* number of comment/space chars */

#define	MAXPUSHBACK	4096

static int	literal;	/* to shut off macro expansion */
static char	pushbuf[MAXPUSHBACK];
static char	*pushbp = &pushbuf[MAXPUSHBACK];

static void	overflow();
extern int	getline(char *, int, FILE *);

#define	pushback(c) \
	((void)(pushbp > &pushbuf[0] ? (void)(*--pushbp = (c)) : overflow()))

#undef	input
#define	input() \
	(pushbp < &pushbuf[MAXPUSHBACK] ? *pushbp++ : *nextcp++)

#undef	unput
#define	unput(c) \
	(pushbp < &pushbuf[MAXPUSHBACK] ? pushback(c) : (void)(*--nextcp = (c)))
%}

%Start	token incom

BEGINCOM	"/*"
ENDCOM		"*/"

%%
	/*
	 * Witchcraft.
	 */
	BEGIN token;
	expanding = (pushbp < &pushbuf[MAXPUSHBACK]);
	blankptr = nextcp;
	blanklen = 0;

<incom>{BEGINCOM}		{ yyerror("nested comment"); blanklen += 2;}
<incom>{ENDCOM}			{ BEGIN token; blanklen += 2; }
<incom>\n			{ extern FILE *infile;
				  extern int inspace;
				  inspace -= getline(nextcp, inspace, infile);
				  blanklen++; }
<incom>.			{ blanklen++; }
<token>{BEGINCOM}		{ BEGIN incom; blanklen += 2; }
<token>{ENDCOM}			{ yyerror("extra comment terminator");
				  blanklen += 2; }
<token>\n			{ blanklen++; }
<token>\\\n			{ blanklen += 2; }
<token>[ \t]+			{ blanklen += yyleng; }

<token>defined			{ literal = 1; return DEFINED; }
<token>[A-Za-z_][A-Za-z_0-9]*	{ yylval.sym = lookup(yytext);
				  if (!literal && yylval.sym->defn) {
					int len = strlen(yylval.sym->defn);
					if (len > pushbp - &pushbuf[0])
						overflow();
					pushbp -= len;
					bcopy(yylval.sym->defn, pushbp, len);
					return yylex();
				  }
				  literal = 0;
				  return IDENT; }
<token>[0-9]+			{ yylval.val = strtol(yytext, 0, 0);
				  return NUMBER; }
<token>0[Xx][0-9A-Fa-f]+	{ yylval.val = strtol(yytext, 0, 16);
				  return NUMBER; }

<token>[,?:]			{ return yytext[0]; }
<token>"||"			{ return OR; }
<token>"&&"			{ return AND; }
<token>"=="			{ yylval.op = EEQ; return EQOP; }
<token>"!="			{ yylval.op = ENE; return EQOP; }
<token>"<"			{ yylval.op = ELT; return RELOP; }
<token>"<="			{ yylval.op = ELE; return RELOP; }
<token>">"			{ yylval.op = EGT; return RELOP; }
<token>">="			{ yylval.op = EGE; return RELOP; }
<token>[|^&]			{ return yytext[0]; }
<token>"<<"			{ yylval.op = ELSH; return SHOP; }
<token>">>"			{ yylval.op = ERSH; return SHOP; }
<token>[+-]			{ return yytext[0]; }
<token>"*"			{ yylval.op = EMUL; return MULOP; }
<token>"/"			{ yylval.op = EDIV; return MULOP; }
<token>"%"			{ yylval.op = EMOD; return MULOP; }
<token>"!"			{ yylval.op = ENOT; return UNOP; }
<token>"~"			{ yylval.op = EBITNOT; return UNOP; }
<token>[()]			{ return yytext[0]; }

<token>.			{ yyerror("illegal character"); blanklen++; }

%%

int
scantoken(char *buf, tokval_t *value)
{
	int type;

	if (buf)
		nextcp = buf;
	literal = 1;
	type = yylex();
	literal = 0;
	if (value)
		*value = yylval;
	return type;
}

void
ansify(char *line, int linesize)
{
	int len, off;
	char *cp;

	if (scantoken(0, 0) == 0)
		return;
	len = strlen(line);
	if (line[len-1] == '\n')
		--len;
	if (len + 3 + 4 >= linesize)
		return;
	cp = nextcp - yyleng;
	off = cp - line;
	bcopy(&line[off], &line[off+3], len - off);
	bcopy("/* ", &line[off], 3);
	bcopy(" */\n", &line[len+3], 4+1);
}

static void
overflow()
{
	yyerror("too much pushback");
	terminate();
}

static int
yywrap()
{
	return 1;
}
