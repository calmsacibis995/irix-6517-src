%{
/*
 * Yacc spec for unifdef's #if expression parser.
 */
#include <stdio.h>
#include "unifdef.h"

static struct expr *result;	/* parse result expression */
%}

%union {
	struct expr	*exp;
	struct symbol	*sym;
	long		val;
	enum operator	op;
}

%type	<exp>	expr identarg ident

%token		DEFINED
%token	<sym>	IDENT
%token	<val>	NUMBER

%right		','
%left		'?' ':'
%left		OR		/* || */
%left		AND		/* && */
%left		'|'
%left		'^'
%left		'&'
%left	<op>	EQOP		/* ==, != */
%left	<op>	RELOP		/* <, <=, >, >= */
%left	<op>	SHOP		/* <<, >> */
%left		'+' '-'
%left	<op>	MULOP		/* *, /, % */
%right	<op>	UNOP		/* !, ~, (-) */

%%

result:
	expr			{ result = $1; }
;

expr:
	expr ',' expr		{ destroyexpr($1); $$ = $3; }
|	expr '?' expr ':' expr	{ $$ = ternary(EIFELSE, $1, $3, $5); }
|	expr OR expr		{ $$ = binary(EOR, $1, $3); }
|	expr AND expr		{ $$ = binary(EAND, $1, $3); }
|	expr '|' expr		{ $$ = binary(EBITOR, $1, $3); }
|	expr '^' expr		{ $$ = binary(EBITXOR, $1, $3); }
|	expr '&' expr		{ $$ = binary(EBITAND, $1, $3); }
|	expr EQOP expr		{ $$ = binary($2, $1, $3); }
|	expr RELOP expr		{ $$ = binary($2, $1, $3); }
|	expr SHOP expr		{ $$ = binary($2, $1, $3); }
|	expr '+' expr		{ $$ = binary(EADD, $1, $3); }
|	expr '-' expr		{ $$ = binary(ESUB, $1, $3); }
|	expr MULOP expr		{ $$ = binary($2, $1, $3); }
|	UNOP expr		{ $$ = unary($1, $2); }
|	'-' expr %prec UNOP	{ $$ = unary(ENEG, $2); }
|	DEFINED identarg	{ $$ = $2; $$->e_op = EDEFINED; }
|	'(' expr ')'		{ $$ = $2; $$->e_paren = 1; }
|	ident
|	NUMBER			{ $$ = expr(ENUMBER, 0); $$->e_val = $1; }
;

identarg:
	ident
|	'(' ident ')'		{ $$ = $2; $$->e_paren = 1; }
;

ident:
	IDENT			{ $$ = expr(EIDENT, 0); $$->e_sym = $1; }
;

%%

struct expr *
parseexpr(char *buf)
{
	extern char *nextcp;	/* ptr to next input char */

	if (buf)
		nextcp = buf;
	if (yyparse() != 0) {
		destroyexpr(result);
		result = 0;
	}
	return result;
}

void
yyerror(const char *message)
{
	extern char *infilename;
	extern int lineno;

	fprintf(stderr, "%s: ", progname);
	if (infilename)
		fprintf(stderr, "%s, ", infilename);
	fprintf(stderr, "%d: %s.\n", lineno, message);
	status++;
}

static struct expr *
expr(enum operator op, int arity)
{
	struct expr *exp;
	extern int expanding;

	exp = new(struct expr);
	exp->e_op = op;
	exp->e_arity = arity;
	exp->e_paren = 0;
	exp->e_reduced = expanding;
	return exp;
}

static struct expr *
ternary(enum operator op, struct expr *cond, struct expr *true,
	struct expr *false)
{
	struct expr *exp;

	exp = expr(op, 3);
	exp->e_cond = cond;
	exp->e_true = true;
	exp->e_false = false;
	return exp;
}

static struct expr *
binary(enum operator op, struct expr *left, struct expr *right)
{
	struct expr *exp;

	exp = expr(op, 2);
	exp->e_left = left;
	exp->e_right = right;
	return exp;
}

static struct expr *
unary(enum operator op, struct expr *kid)
{
	struct expr *exp;

	exp = expr(op, 1);
	exp->e_kid = kid;
	return exp;
}
