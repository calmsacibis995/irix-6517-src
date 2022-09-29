/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Recursive descent filter expression parser.
 */
#include <bstring.h>
#include <ctype.h>
#include "expr.h"
#include "heap.h"
#include "macros.h"
#include "protocol.h"
#include "scope.h"
#include "snooper.h"
#include "strings.h"

enum tokencode {
	BAD = -1,
	END,
	ARG,
	COMMA,
	OR,
	AND,
	BITOR,
	BITXOR,
	BITAND,
	EQ, NE,
	LE, LT, GE, GT,
	LSH, RSH,
	PLUS, MINUS,
	MUL, DIV, MOD,
	NOT, BITNOT,
	LB, RB, DOT,
	NAME, NUMBER, STRING, QUOTEDSTRING,
	LP, RP
};

static int	maclevel;	/* macro call nesting level */
static Snooper	*snoop;		/* invariant arg to recursive parser */
static ExprError exprerr;	/* invariant error report arg/result */

typedef struct parsedata ParseData;

struct parsedata {
	char		*firstc;	/* base of input buffer */
	char		*nextc;		/* points to next input char */
	char		*token;		/* last token recognized */
	ExprSource	*src;		/* parser input information */
	long		numval;		/* last number scanned */
	Protocol	*proto;		/* protocol for path lookup */
	Protocol	*lastproto;	/* last protocol in path lookup */
	Scope		*instruct;	/* open structure scope or null */
	ParseData	*caller;	/* macro calling context */
	struct string	*argv;		/* macro call actual args */
};

static enum tokencode	token(ParseData *);
static char		*match(enum tokencode, ParseData *);

/*
 * Note how input is zapped on first error, by setting pd->nextc to point
 * at an empty string.
 */
static void
error(char *message, ParseData *pd)
{
	if (exprerr.err_message)
		return;
	exprerr.err_message = message;
	exprerr.err_token = pd->token;
	if (pd->src)
		exprerr.err_source = *pd->src;
	pd->nextc = "";
}

/*
 * Forward prototype declaration of expression string parser.
 */
static Expr *parse(struct string *, ExprSource *, Protocol *, struct string *,
		   ParseData *);

/*
 * Public interface to the parser which takes a terminated string pointed
 * at by an ExprSource descriptor.
 */
Expr *
ex_parse(ExprSource *src, Snooper *sn, Protocol *pr, ExprError *err)
{
	struct string s;
	Expr *ex;

	snoop = sn;
	bzero(&exprerr, sizeof exprerr);
	s.s_ptr = src->src_buf;
	s.s_len = strlen(src->src_buf);
	ex = parse(&s, src, pr, 0, 0);
	*err = exprerr;
	return ex;
}

/*
 * Forward prototype declarations for most of the grammar.
 */
static Expr *commaexpr(ParseData *);
static Expr *orexpr(ParseData *);
static Expr *andexpr(ParseData *);
static Expr *bitorexpr(ParseData *);
static Expr *bitxorexpr(ParseData *);
static Expr *bitandexpr(ParseData *);
static Expr *eqexpr(ParseData *);
static Expr *relexpr(ParseData *);
static Expr *shiftexpr(ParseData *);
static Expr *addexpr(ParseData *);
static Expr *mulexpr(ParseData *);
static Expr *unaryexpr(ParseData *);
static Expr *memberexpr(ParseData *);
static Expr *primaryexpr(ParseData *);
static void correct(Expr *, int, ParseData *);

/*
 * Parse the expression in sp using src for diagnostics, pr as the raw
 * protocol, argv for macro actuals, and caller for macro calls.  If the
 * user failed to supply an operator between two operands, try to add an
 * appropriate operator.
 */
static Expr *
parse(struct string *sp, ExprSource *src, Protocol *pr, struct string *argv,
      ParseData *caller)
{
	ParseData pd;
	char *end, save;
	Expr *ex;

	pd.firstc = pd.nextc = sp->s_ptr;
	pd.src = holdExprSource(src);
	pd.proto = pd.lastproto = pr;
	pd.instruct = 0;
	pd.caller = caller;
	pd.argv = argv;
	end = sp->s_ptr + sp->s_len;
	save = *end;
	*end = '\0';

	for (;;) {
		ex = commaexpr(&pd);
		if (exprerr.err_message) {
			ex_destroy(ex);
			ex = 0;
			break;
		}
		if (*pd.nextc == '\0')
			break;
		correct(ex, 0, &pd);
		ex_destroy(ex);
	}
	dropExprSource(pd.src);
	*end = save;
	return ex;
}

static int
boolean(enum exop op)
{
	switch (op) {
	  case EXOP_OR:		/* logical, equality, relational, inverse */
	  case EXOP_AND:
	  case EXOP_EQ:
	  case EXOP_NE:
	  case EXOP_LT:
	  case EXOP_LE:
	  case EXOP_GT:
	  case EXOP_GE:
	  case EXOP_NOT:
		return 1;
	}
	return 0;
}

static void
correct(Expr *ex, int pos, ParseData *pd)
{
	char *from, *buf, *to;
	int len, off, rem;
	enum exop op;
	static char *lastbuf;

	/*
	 * Compute offset of missing operator in pd and allocate new space
	 * for the corrected version.  Allow at most 4 characters (" == " or
	 * " && " depending on left context and white space).
	 */
	from = pd->firstc;
	len = strlen(from);
	off = pd->nextc - from;
	rem = len - off;
	len += 4;
	buf = vnew(len + 1, char);

	/*
	 * Check for a binary operator with a protocol path on the right.
	 * Non-boolean protocol expressions are corrected to become left
	 * operands of equality ops.
	 */
	op = ex->ex_op;
	if (ex->ex_arity == EX_BINARY) {
		switch (ex->ex_right->ex_op) {
		  case EXOP_PROTOCOL:
		  case EXOP_FIELD:
			ex = ex->ex_right;
			op = ex->ex_op;
		}
	}
	while (op == EXOP_PROTOCOL) {
		ex = ex->ex_member;
		if (ex == 0) {
			op = EXOP_EQ;	/* boolean protocol path */
			break;
		}
		op = ex->ex_op;
	}

	/*
	 * Now copy the old string to the new, inserting the correction at
	 * the offset where parsing stopped prematurely.  Add white space
	 * only where it might be needed to terminate macro argument lists.
	 */
	to = buf;
	bcopy(from, to, off);
	from += off, to += off;
	if (!isspace(from[-1]))
		*to++ = ' ';
	if (boolean(op))
		bcopy("&&", to, 2);
	else
		bcopy("==", to, 2);
	to += 2;
	if (!isspace(*from))
		*to++ = ' ';
	bcopy(from, to, rem);
	to += rem;
	*to = '\0';

	/*
	 * Delete any old correction and restore pd for another parse.
	 * We delete the old source late in order to avoid overlapping
	 * copies from freed store, above.
	 */
	if (pd->firstc == lastbuf)
		delete(lastbuf);
	pd->firstc = lastbuf = buf;	/* XXX leaky */
	pd->nextc = buf + pos;
	if (pd->src)
		pd->src->src_buf = buf;
	pd->lastproto = pd->proto;
	pd->instruct = 0;
}

static Expr *
binary(enum exop bop, Expr *lex, Expr *rex, char *token, ExprSource *src)
{
	Expr *ex;

	if (lex == 0 || rex == 0) {
		ex_destroy(lex);
		ex_destroy(rex);
		return 0;
	}
	ex = expr(bop, EX_BINARY, token);
	ex_holdsrc(ex, src);
	ex->ex_left = lex;
	ex->ex_right = rex;
	if (ex_eval(ex, &exprerr) == ET_ERROR) {
		ex_destroy(ex);
		return 0;
	}
	return ex;
}

static Expr *
unary(enum exop uop, Expr *kex, char *token, ExprSource *src)
{
	Expr *ex;

	if (kex == 0)
		return 0;
	ex = expr(uop, EX_UNARY, token);
	ex_holdsrc(ex, src);
	ex->ex_kid = kex;
	if (ex_eval(ex, &exprerr) == ET_ERROR) {
		ex_destroy(ex);
		return 0;
	}
	return ex;
}

static Expr *
nullary(enum exop op, char *token, ExprSource *src)
{
	Expr *ex;

	ex = expr(op, EX_NULLARY, token);
	ex_holdsrc(ex, src);
	return ex;
}

static Expr *
commaexpr(ParseData *pd)
{
	Expr *ex;

	ex = orexpr(pd);
	while (ex && match(COMMA, pd)) {
		ex_destroy(ex);
		ex = orexpr(pd);
	}
	return ex;
}

static Expr *
orexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = andexpr(pd);
	while (ex && (t = match(OR, pd)) != 0)
		ex = binary(EXOP_OR, ex, andexpr(pd), t, pd->src);
	return ex;
}

static Expr *
andexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = bitorexpr(pd);
	while (ex && (t = match(AND, pd)) != 0)
		ex = binary(EXOP_AND, ex, bitorexpr(pd), t, pd->src);
	return ex;
}

static Expr *
bitorexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = bitxorexpr(pd);
	while (ex && (t = match(BITOR, pd)) != 0)
		ex = binary(EXOP_BITOR, ex, bitxorexpr(pd), t, pd->src);
	return ex;
}

static Expr *
bitxorexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = bitandexpr(pd);
	while (ex && (t = match(BITXOR, pd)) != 0)
		ex = binary(EXOP_BITXOR, ex, bitandexpr(pd), t, pd->src);
	return ex;
}

static Expr *
bitandexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = eqexpr(pd);
	while (ex && (t = match(BITAND, pd)) != 0)
		ex = binary(EXOP_BITAND, ex, eqexpr(pd), t, pd->src);
	return ex;
}

static Expr *
eqexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = relexpr(pd);
	while (ex)
		if (t = match(EQ, pd))
			ex = binary(EXOP_EQ, ex, relexpr(pd), t, pd->src);
		else if (t = match(NE, pd))
			ex = binary(EXOP_NE, ex, relexpr(pd), t, pd->src);
		else
			break;
	return ex;
}

static Expr *
relexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = shiftexpr(pd);
	while (ex)
		if (t = match(LT, pd))
			ex = binary(EXOP_LT, ex, shiftexpr(pd), t, pd->src);
		else if (t = match(LE, pd))
			ex = binary(EXOP_LE, ex, shiftexpr(pd), t, pd->src);
		else if (t = match(GT, pd))
			ex = binary(EXOP_GT, ex, shiftexpr(pd), t, pd->src);
		else if (t = match(GE, pd))
			ex = binary(EXOP_GE, ex, shiftexpr(pd), t, pd->src);
		else
			break;
	return ex;
}

static Expr *
shiftexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = addexpr(pd);
	while (ex)
		if (t = match(LSH, pd))
			ex = binary(EXOP_LSH, ex, addexpr(pd), t, pd->src);
		else if (t = match(RSH, pd))
			ex = binary(EXOP_RSH, ex, addexpr(pd), t, pd->src);
		else
			break;
	return ex;
}

static Expr *
addexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = mulexpr(pd);
	while (ex)
		if (t = match(PLUS, pd))
			ex = binary(EXOP_ADD, ex, mulexpr(pd), t, pd->src);
		else if (t = match(MINUS, pd))
			ex = binary(EXOP_SUB, ex, mulexpr(pd), t, pd->src);
		else
			break;
	return ex;
}

static Expr *
mulexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	ex = unaryexpr(pd);
	while (ex)
		if (t = match(MUL, pd))
			ex = binary(EXOP_MUL, ex, unaryexpr(pd), t, pd->src);
		else if (t = match(DIV, pd))
			ex = binary(EXOP_DIV, ex, unaryexpr(pd), t, pd->src);
		else if (t = match(MOD, pd))
			ex = binary(EXOP_MOD, ex, unaryexpr(pd), t, pd->src);
		else
			break;
	return ex;
}

static Expr *
unaryexpr(ParseData *pd)
{
	char *t;
	Expr *ex;

	if (t = match(NOT, pd))
		ex = unary(EXOP_NOT, unaryexpr(pd), t, pd->src);
	else if (t = match(BITNOT, pd))
		ex = unary(EXOP_BITNOT, unaryexpr(pd), t, pd->src);
	else if (t = match(MINUS, pd))
		ex = unary(EXOP_NEG, unaryexpr(pd), t, pd->src);
	else
		ex = memberexpr(pd);
	return ex;
}

static Expr *
memberexpr(ParseData *pd)
{
	Expr *ex, *rex, *lex;
	char *t;
	int pos;
	Protocol *save;

	ex = primaryexpr(pd);
	while (ex) {
		if (t = match(LB, pd)) {
			/*
			 * Left side must be an array.
			 */
			if (!ex_typematch(ex, EXOP_ARRAY, &pd->instruct)) {
				ex_error(ex, "not an array", &exprerr);
				ex_destroy(ex);
				return 0;
			}

			/*
			 * Correct missing operator errors in the bracketed
			 * index expression.
			 */
			pos = pd->nextc - pd->firstc;
			while ((rex = commaexpr(pd)) != 0 && !match(RB, pd)) {
				if (*pd->nextc == '\0') {	/* [ */
					error("missing ']'", pd);
					ex_destroy(rex);
					ex_destroy(ex);
					return 0;
				}
				correct(ex, pos, pd);
			}

			/*
			 * Right side must be an integer expression.
			 */
			if (!ex_typematch(rex, EXOP_NUMBER, &pd->instruct)) {
				ex_error(rex, "illegal array index", &exprerr);
				ex_destroy(rex);
				ex_destroy(ex);
				return 0;
			}

			ex = binary(EXOP_ARRAY, ex, rex, t, pd->src);
		} else if (t = match(DOT, pd)) {
			/*
			 * Left side must be a struct or protocol path.  Find
			 * the lowest struct or protocol node.
			 */
			lex = ex_typematch(ex, EXOP_STRUCT, &pd->instruct);
			if (lex == 0) {
				ex_error(ex, "illegal left side of '.'",
					 &exprerr);
				ex_destroy(ex);
				return 0;
			}

			/*
			 * If the left side is a protocol path, the right side
			 * either replaces ex or connects to lex->ex_member.
			 */
			if (lex->ex_op == EXOP_PROTOCOL) {
				save = pd->proto;
				pd->proto = lex->ex_prsym->sym_proto;
				rex = memberexpr(pd);
				pd->proto = save;
				if (rex == 0) {
					ex_destroy(ex);
					return 0;
				}
				switch (rex->ex_op) {
				  case EXOP_NUMBER:	/* protocol constant */
				  case EXOP_ADDRESS:
				  case EXOP_STRING:
					ex_destroy(ex);
					ex = rex;
					break;
				  default:
					lex->ex_member = rex;
				}
				continue;
			}

			/*
			 * Parse the member expression on the right.  Require
			 * that it be a field or a field aggregate.
			 */
			rex = memberexpr(pd);
			if (rex == 0) {
				ex_destroy(ex);
				return 0;
			}
			switch (rex->ex_op) {
			  case EXOP_ARRAY:
			  case EXOP_STRUCT:
			  case EXOP_FIELD:
				break;
			  default:
				ex_error(rex, "illegal right side of '.'",
					 &exprerr);
				ex_destroy(rex);
				ex_destroy(ex);
				return 0;
			}
			ex = binary(EXOP_STRUCT, ex, rex, t, pd->src);
		} else {
			/*
			 * Mismatch: return to lower-precedence parsers.
			 */
			break;
		}
	}
	pd->instruct = 0;
	return ex;
}

static Expr *pathexpr(char *, int, ParseData *);
static Expr *strexpr(char *, int, enum tokencode, ParseData *);

static Expr *
primaryexpr(ParseData *pd)
{
	enum tokencode tc;
	int len, pos;
	Expr *ex;

	tc = token(pd);
	len = pd->nextc - pd->token;
	switch (tc) {
	  case ARG:
		if (maclevel == 0) {
			error("formal argument outside macro", pd);
			return 0;
		}
		ex = parse(&pd->argv[pd->numval-1], pd->caller->src,
			   pd->proto, pd->caller->argv, pd->caller->caller);
		break;
	  case NAME:
		ex = pathexpr(pd->token, len, pd);
		break;
	  case NUMBER:
		ex = nullary(EXOP_NUMBER, pd->token, pd->src);
		ex->ex_val = pd->numval;
		break;
	  case STRING:
		ex = strexpr(pd->token, len, tc, pd);
		break;
	  case QUOTEDSTRING:
		ex = strexpr(pd->token + 1, len - 2, tc, pd);
		break;
	  case LP:
		pos = pd->nextc - pd->firstc;
		while ((ex = commaexpr(pd)) != 0 && !match(RP, pd)) {
			if (*pd->nextc == '\0') {	/* ( */
				error("missing ')'", pd);
				ex_destroy(ex);
				return 0;
			}
			correct(ex, pos, pd);
		}
		break;
	  default:
		if (tc == END)
			error("incomplete expression", pd);
		else if (tc != BAD)
			error("missing operand", pd);
		return 0;
	}
	return ex;
}

/*
 * Built-in functions and protocols.
 */
enum bintype { BIN_DEFINE, BIN_UNDEF, BIN_FETCH, BIN_STRING, BIN_SNOOP };

struct builtin {
	char		*name;		/* builtin name */
	int		namlen;		/* and name length */
	enum bintype	type;		/* type code */
	int		nargs;		/* number of arguments */
};
#define	BIN(name, type, nargs) \
	{ name, constrlen(name), type, nargs }

static struct builtin builtins[] = {
	BIN("define",	BIN_DEFINE,	2),
	BIN("undef",	BIN_UNDEF,	1),
	BIN("fetch",	BIN_FETCH,	2),
	BIN("string",	BIN_STRING,	2),
	BIN("snoop",	BIN_SNOOP,	0),
	BIN("SNOOP",	BIN_SNOOP,	0),
	0,
};

static struct builtin *binfind(char *, int);
static Expr *binexpr(char *, struct builtin *, Protocol *, ParseData *);
static Expr *callexpr(char *, struct funcdef *, Protocol *, ParseData *);
static Expr *macexpr(char *, struct macrodef *, Protocol *, ParseData *);

static Expr *
pathexpr(char *path, int pathlen, ParseData *pd)
{
	Protocol *pr;
	Expr *ex, *rex;
	char *name;
	int namlen;
	Symbol *sym;

	pr = pd->proto;
nextcomponent:
	name = path;
	while (pathlen != 0 && *path != '.')
		--pathlen, path++;
	namlen = path - name;

	/*
	 * If we're in a structure scope, look for the named struct member.
	 * Otherwise try pr's scope, then pd->lastproto's.
	 */
	if (pd->instruct) {
		sym = sc_lookupsym(pd->instruct, name, namlen);
		pd->instruct = 0;
		if (sym == 0) {
			error("unknown member", pd);
			return 0;
		}
	} else {
		sym = pr_lookupsym(pr, name, namlen);
		if (sym == 0 && pr != pd->lastproto) {
			pr = pd->lastproto;
			sym = pr_lookupsym(pr, name, namlen);
		}
	}

	/*
	 * If we couldn't find name in a scope, check for a degenerate or
	 * reflexive path component like "ether" where pr is &ether_proto.
	 * If that fails, try a builtin.  If name isn't known, make name
	 * and the rest of the path into a string expression.
	 */
	if (sym == 0) {
		struct builtin *bin;

		pr = pd->proto;
		if (findprotobyname(name, namlen) == pr) {
			if (pathlen != 0) {
				--pathlen, path++;
				goto nextcomponent;
			}
			ex = nullary(EXOP_NUMBER, "1", 0);
			ex->ex_val = 1;
			return ex;
		}
		bin = binfind(name, namlen);
		if (bin == 0)
			return strexpr(name, namlen + pathlen, STRING, pd);
		ex = binexpr(name, bin, pr, pd);
		if (ex == 0)
			return 0;
	} else {
		/*
		 * Build an expression node for sym, collecting arguments
		 * from pd if necessary.
		 */
		switch (sym->sym_type) {
		  case SYM_ADDRESS:
			ex = nullary(EXOP_ADDRESS, name, pd->src);
			ex->ex_addr = sym->sym_addr;
			break;
		  case SYM_NUMBER:
			ex = nullary(EXOP_NUMBER, name, pd->src);
			ex->ex_val = sym->sym_val;
			break;
		  case SYM_PROTOCOL:
			ex = nullary(EXOP_PROTOCOL, name, pd->src);
			ex->ex_prsym = sym;
			ex->ex_member = 0;
			break;
		  case SYM_FIELD:
			ex = nullary(EXOP_FIELD, name, pd->src);
			ex->ex_field = sym->sym_field;
			break;
		  case SYM_MACRO:
			ex = macexpr(name, &sym->sym_def, pr, pd);
			if (ex == 0)
				return 0;
			break;
		  case SYM_FUNCTION:
			ex = callexpr(name, &sym->sym_func, pr, pd);
			if (ex == 0)
				return 0;
			break;
		}
	}

	/*
	 * Update pd->nextc if there are path components left, and update
	 * pd->lastproto if ex is a protocol node.
	 */
	if (pathlen != 0)
		pd->nextc = path;
	rex = ex;
	while (ex->ex_op == EXOP_PROTOCOL) {
		pr = ex->ex_prsym->sym_proto;
		ex = ex->ex_member;
		if (ex == 0)
			break;
	}
	pd->lastproto = pr;
	return rex;
}

static Expr *
strexpr(char *ptr, int len, enum tokencode tc, ParseData *pd)
{
	char save;
	Expr *ex;

	save = ptr[len];
	ptr[len] = '\0';
	ex = pr_resolve(pd->lastproto, ptr, len, snoop);
	if (ex == 0 && pd->lastproto != pd->proto)
		ex = pr_resolve(pd->proto, ptr, len, snoop);
	ptr[len] = save;
	if (ex) {
		ex_holdsrc(ex, pd->src);
	} else if (tc == QUOTEDSTRING) {
		ex = ex_string(ptr, len);
		ex_holdsrc(ex, pd->src);
	} else {
		pd->token = ptr;
		error("unresolved string", pd);
	}
	return ex;
}

static struct builtin *
binfind(char *name, int namlen)
{
	struct builtin *bin;

	for (bin = builtins; bin->name; bin++) {
		if (bin->namlen == namlen && !bcmp(bin->name, name, namlen))
			return bin;
	}
	return 0;
}

static int getargs(int, struct string *, ParseData *);
static int getshellstyleargs(int, struct string *, ParseData *);
static int getcppstyleargs(int, struct string *, ParseData *);
static int parseunsigned(struct string *, Protocol *, ParseData *, unsigned *);

static Expr *
binexpr(char *name, struct builtin *bin, Protocol *pr, ParseData *pd)
{
	int argc;
	struct string argv[MAXMACROARGS];
	Expr *ex;
	static Symbol snoopsym;

	argc = getargs(bin->nargs, argv, pd);
	if (argc < 0)
		return 0;
	switch (bin->type) {
	  case BIN_DEFINE:
		pr_addmacro(pr, argv[0].s_ptr, argv[0].s_len,
				argv[1].s_ptr, argv[1].s_len, pd->src);
		ex = nullary(EXOP_NUMBER, name, pd->src);
		ex->ex_val = 1;
		break;
	  case BIN_UNDEF:
		pr_deletesym(pr, argv[0].s_ptr, argv[0].s_len);
		ex = nullary(EXOP_NUMBER, name, pd->src);
		ex->ex_val = 1;
		break;
	  case BIN_FETCH:
	  case BIN_STRING:
		ex = nullary(EXOP_FETCH, name, pd->src);
		if (!parseunsigned(&argv[0], pr, pd, &ex->ex_size)
		    || !parseunsigned(&argv[1], pr, pd, &ex->ex_off)) {
			ex_destroy(ex);
			ex = 0;
		} else if (bin->type == BIN_STRING) {
			ex->ex_type = EXOP_STRING;
		} else {
			switch (ex->ex_size) {
			  case sizeof(char):
			  case sizeof(short):
			  case sizeof(long):
				ex->ex_type = EXOP_NUMBER;
				break;
			  default:
				if (ex->ex_size <= sizeof ex->ex_addr)
					ex->ex_type = EXOP_ADDRESS;
				else
					ex->ex_type = EXOP_STRING;
			}
		}
		break;
	  case BIN_SNOOP:
		ex = nullary(EXOP_PROTOCOL, name, pd->src);
		if (snoopsym.sym_name == 0) {
			snoopsym.sym_name = snoop_proto.pr_name;
			snoopsym.sym_namlen = snoop_proto.pr_namlen;
			snoopsym.sym_type = SYM_PROTOCOL;
			snoopsym.sym_proto = &snoop_proto;
		}
		ex->ex_prsym = &snoopsym;
		ex->ex_member = 0;
	}
	return ex;
}

static Expr *
callexpr(char *name, struct funcdef *fd, Protocol *pr, ParseData *pd)
{
	int argc, n;
	struct string argv[MAXCALLARGS];
	Expr *ex, *arg;

	argc = getargs(fd->fd_nargs, argv, pd);
	if (argc < 0)
		return 0;
	ex = nullary(EXOP_CALL, name, pd->src);
	ex->ex_func = fd->fd_func;
	for (n = 0; n < argc; n++) {
		arg = parse(&argv[n], pd->src, pr, pd->argv, pd->caller);
		if (arg == 0) {
			while (--n >= 0)
				ex_destroy(ex->ex_args[n]);
			ex_destroy(ex);
			return 0;
		}
		ex->ex_args[n] = arg;
	}
	return ex;
}

static Expr *
macexpr(char *name, struct macrodef *md, Protocol *pr, ParseData *pd)
{
	int argc;
	struct string argv[MAXMACROARGS];
	Expr *ex;

	if (maclevel >= 20) {
		error("too much macro nesting", pd);
		return 0;
	}
	argc = getargs(md->md_nargs, argv, pd);
	if (argc < 0)
		return 0;
	maclevel++;
	ex = parse(&md->md_string, md->md_src, pr, argv, pd);
	--maclevel;
	return ex;
}

static int
getargs(int nargs, struct string *argv, ParseData *pd)
{
	int argc;

	argc = 0;
	if (nargs != 0) {
		if (isspace(*pd->nextc))
			argc = getshellstyleargs(nargs, argv, pd);
		else if (match(LP, pd) && !match(RP, pd))
			argc = getcppstyleargs(nargs, argv, pd);
		if (argc < 0)
			return argc;
	}
	if (nargs > 0 && argc != nargs) {
		error("too few arguments", pd);
		return -1;
	}
	return argc;
}

static int
getshellstyleargs(int nargs, struct string *argv, ParseData *pd)
{
	int argc;
	char *t, c;
	struct string *ap;

	t = pd->nextc;
	for (argc = 0; argc < nargs; argc++) {
		while (isspace(c = *t))
			t++;
		if (c == '\0')
			break;
		pd->token = t;
		ap = &argv[argc];
		ap->s_ptr = t;
		while (!isspace(c = *t) && c != '\0')
			t++;
		ap->s_len = t - ap->s_ptr;
	}
	pd->nextc = t;
	return argc;
}

static int
getcppstyleargs(int nargs, struct string *argv, ParseData *pd)
{
	int argc;
	enum tokencode tc;

	argc = 0;
	do {
		char *arg;
		int parens;

		if (argc >= nargs) {
			error("too many arguments", pd);
			return -1;
		}
		arg = pd->nextc;
		parens = 0;
		while ((tc = token(pd)) != END) {
			if (tc == BAD)
				return -1;
			if (tc == LP) {
				parens++;
			} else if (tc == COMMA) {
				if (parens == 0)
					break;
			} else if (tc == RP) {
				if (parens == 0)
					break;
				--parens;
			}
		}
		argv[argc].s_ptr = arg;
		argv[argc].s_len = pd->token - arg;
		argc++;
	} while (tc == COMMA);
	if (tc != RP) {
		error("missing ')'", pd);
		return -1;
	}
	return argc;
}

static int
parseunsigned(struct string *sp, Protocol *pr, ParseData *pd, unsigned *ip)
{
	Expr *ex;
	int ok;

	ex = parse(sp, pd->src, pr, pd->argv, pd->caller);
	if (ex == 0)
		return 0;
	if (ex_eval(ex, &exprerr) == ET_SIMPLE && ex->ex_op == EXOP_NUMBER) {
		*ip = ex->ex_val;
		ok = 1;
	} else {
		ex_error(ex, "constant integer expression required", &exprerr);
		ok = 0;
	}
	ex_destroy(ex);
	return ok;
}

static enum tokencode
token(ParseData *pd)
{
	char *t;
	char c;
	enum tokencode tc;

	t = pd->nextc;
	while (isspace(c = *t))
		t++;
	pd->token = t;

	if (isalpha(c) || c == '_') {
		int funny;

		tc = NAME;
		do {
			c = *++t;
			funny = (c == ':' || c == '-' || c == '@');
			if (funny)
				tc = STRING;
		} while (isalnum(c) || c == '_' || c == '.' || funny);
		pd->nextc = t;
		if (tc == NAME) {
			int len;
#define	iskeyword(s) \
	(len == constrlen(s) && !strncmp(pd->token, s, len))

			len = t - pd->token;
			if (iskeyword("and"))
				return AND;
			if (iskeyword("or"))
				return OR;
			if (iskeyword("not"))
				return NOT;
#undef	iskeyword
		}
		return tc;
	}

	if (isdigit(c) || c == ':') {
		pd->numval = strtoul(t, &pd->nextc, 0);
		t = pd->nextc;
		c = *t;
		if (isalpha(c) || c == '.' || c == ':' || c == '-') {
			do {
				c = *++t;
			} while (isalnum(c) || c == '.' || c == ':' || c== '-');
			pd->nextc = t;
			return STRING;
		}
		return NUMBER;
	}

	if (c == '"' || c == '\'') {
		char qc = c;

		while ((c = *++t) != qc)
			if (c == '\0') {
				error("unterminated string", pd);
				pd->nextc = t;
				return BAD;
			}
		pd->nextc = t + 1;
		return QUOTEDSTRING;
	}

#define	ifnext(c, y, n)	(t[1] == (c) ? t++, (y) : (n))
	switch (c) {
	  case '\0':
		pd->nextc = t;
		return END;		/* NB: early return */
	  case '$':
		t++;
		pd->numval = strtol(t, &pd->nextc, 10);
		if (pd->nextc == t
		    || pd->numval <= 0 || MAXMACROARGS < pd->numval) {
			error("illegal formal argument", pd);
			return BAD;
		}
		return ARG;		/* NB: early return */
	  case ',':
		tc = COMMA;
		break;
	  case '|':
		tc = ifnext(c, OR, BITOR);
		break;
	  case '&':
		tc = ifnext(c, AND, BITAND);
		break;
	  case '=':
		tc = ifnext(c, EQ, EQ);
		break;
	  case '!':
		tc = ifnext('=', NE, NOT);
		break;
	  case '<':
		tc = ifnext(c, LSH, ifnext('=', LE, LT));
		break;
	  case '>':
		tc = ifnext(c, RSH, ifnext('=', GE, GT));
		break;
	  case '^':
		tc = BITXOR;
		break;
	  case '+':
		tc = PLUS;
		break;
	  case '-':
		tc = MINUS;
		break;
	  case '*':
		tc = MUL;
		break;
	  case '/':
		tc = DIV;
		break;
	  case '%':
		tc = MOD;
		break;
	  case '~':
		tc = BITNOT;
		break;
	  case '[':
		tc = LB;
		break;
	  case ']':
		tc = RB;
		break;
	  case '.':
		tc = DOT;
		break;
	  case '(':
		tc = LP;
		break;
	  case ')':
		tc = RP;
		break;
	  default:
		error("illegal character", pd);
		tc = BAD;
	}
#undef	ifnext

	pd->nextc = t + 1;
	return tc;
}

/*
 * Match tc against the next input token.
 */
static char *
match(enum tokencode tc, ParseData *pd)
{
	enum tokencode ntc;

	ntc = token(pd);
	if (ntc == tc)
		return pd->token;
	if (ntc != BAD && ntc != END)
		pd->nextc = pd->token;
	return 0;
}
