/*
 * Unifdef input processing routines.
 */
#include <stdio.h>
#include <string.h>
#include "unifdef.h"
#include "unif_scan.h"

FILE	*infile;	/* input file pointer */
char	*infilename;	/* input file pathname or null if stdin */
int	inspace;	/* space left in input buffer */
int	lineno;		/* line number in input file */

#define	LINESIZE	8192
#define	MAXIFDEPTH	10

enum keytype { IF, IFDEF, IFNDEF, ELIF, ELSE, ENDIF, SENTINEL };

struct ifstate {
	enum symtype	type;		/* type of controlling expression */
	enum keytype	key;		/* keyword for current section */
	int		endifs;		/* number of don't-care #endif's */
	int		defines;	/* number of defines for current section */
};

static struct ifstate	ifstack[MAXIFDEPTH];
static struct ifstate	*ifsp = &ifstack[-1];
static int		falselevel;

static struct keyword {
	char		*name;
	enum keytype	type;
} keywords[] = {
	{ "if",		IF },
	{ "ifdef",	IFDEF },
	{ "ifndef",	IFNDEF },
	{ "elif",	ELIF },
	{ "else",	ELSE },
	{ "endif",	ENDIF },
	{ 0,		SENTINEL }
};

void
initkeywords()
{
	struct keyword *kw;

	for (kw = keywords; kw->name; kw++)
		install(kw->name, SRESERVED, (long) kw->type, 0);
}

static void	putstr(char *);
static void	doifexpr(enum keytype, char *, enum symtype, struct expr *);
static void	doifdef(enum keytype, char *, enum symtype);
static void	doelif(char *, struct expr *);
static void	doelse(char *);
static void	doendif(char *);

/*
 * Variation on fgets that does end-of-line backslash handling as in cpp.
 */
int
getline(char *buf, int len, FILE *file)
{
	char *bp;
	int lastc, c;

	bp = buf;
	lastc = 0;
	while (--len > 0) {
		c = getc(file);
		if (c == EOF)
			break;
		if (c == '\n') {
			lineno++;
			if (lastc != '\\') {
				*bp++ = c;
				break;
			}
		}
		*bp++ = lastc = c;
	}

	*bp = '\0';
	if (bp == buf && ferror(file))
		return -1;
	return bp - buf;
}

void
process(FILE *file, char *name)
{
	int len, toktype;
	char line[LINESIZE];
	enum keytype keytype;
	tokval_t tokval;
	struct expr *exp;

	infile = file;
	infilename = name;
	while ((len = getline(line, sizeof line, file)) > 0) {
		inspace = sizeof line - len;
		if (line[0] != '#'
		    || scantoken(&line[1], &tokval) != IDENT
		    || tokval.sym->type != SRESERVED) {
			putstr(line);
			continue;
		}
		keytype = (enum keytype) tokval.sym->value;
		switch (keytype) {
		  case IF:
			exp = parseexpr(0);
			doifexpr(IF, line, evalexpr(exp), exp);
			destroyexpr(exp);
			break;
		  case IFDEF:
		  case IFNDEF:
			toktype = scantoken(0, &tokval);
			if (toktype != IDENT)
				doifdef(keytype, line, SDONTCARE);
			else {
				doifdef(keytype, line, tokval.sym->type);
				dropsym(tokval.sym);
			}
			break;
		  case ELIF:
			exp = parseexpr(0);
			doelif(line, exp);
			destroyexpr(exp);
			break;
		  case ELSE:
			doelse(line);
			break;
		  case ENDIF:
			doendif(line);
			break;
		  default:
			putstr(line);
		}
	}
	if (len < 0)
		Perror(name);
	else if (ifsp != &ifstack[-1])
		yyerror("missing #endif");
	ifsp = &ifstack[-1];
	falselevel = 0;
	infile = 0;
	infilename = 0;
	lineno = 0;
}

static void
putstr(char *s)
{
	extern FILE *outfile;
	extern char *outfilename;

	if (falselevel == 0 && fputs(s, outfile) == EOF) {
		Perror(outfilename);
		terminate();
	}
}

static char	*sprintp;
static void	sprintexpr(struct expr *);
static void	doif(enum keytype, char *, enum symtype);

static void
doifexpr(enum keytype key, char *line, enum symtype type, struct expr *exp)
{
	char *blanks;
	extern int zflag;
	extern char *blankptr;
	extern int blanklen;

	if (!zflag && type == SDEFINED
	    && exp->e_op == ENUMBER && !exp->e_reduced) {
		type = SDONTCARE;
	}
	switch (type) {
	  case SDONTCARE:
		blanks = strncpy(&line[LINESIZE-blanklen], blankptr, blanklen);
		sprintp = line;
		if (key == IF && exp->e_op == EDEFINED) {
			sprintp += sprintf(sprintp, "#ifdef %s",
					   exp->e_sym->name);
		} else if (key == IF && exp->e_op == ENOT
			   && exp->e_kid->e_op == EDEFINED) {
			sprintp += sprintf(sprintp, "#ifndef %s",
					   exp->e_kid->e_sym->name);
		} else {
			sprintp += sprintf(sprintp, "#%sif ",
					   (key == ELIF) ? "el" : "");
			sprintexpr(exp);
		}
		if (sprintp < blanks)
			(void) sprintf(sprintp, "%.*s", blanklen, blanks);
		else
			(void) strcpy(sprintp, "\n");
		break;
	  case SDEFINED:
		if (exp->e_val == 0)
			type = SUNDEFINED;
	}
	doif(key, line, type);
}

static char *opname[] = {
	"?:",			/* EIFELSE */
	"||",			/* EOR */
	"&&",			/* EAND */
	"|",			/* EBITOR */
	"^",			/* EBITXOR */
	"&",			/* EBITAND */
	"==", "!=",		/* EEQ, ENE */
	"<", "<=", ">", ">=",	/* ELT, ELE, EGT, EGE */
	"<<", ">>",		/* ELSH, ERSH */
	"+", "-",		/* EADD, ESUB */
	"*", "/", "%",		/* EMUL, EDIV, EMOD */
	"!", "~", "-"		/* ENOT, EBITNOT, ENEG */
};

static void
sprintexpr(struct expr *exp)
{
	if (exp->e_paren && exp->e_op != EDEFINED)
		sprintp += sprintf(sprintp, "(");
	switch (exp->e_arity) {
	  case 0:
		switch (exp->e_op) {
		  case EDEFINED:
			sprintp += sprintf(sprintp, "defined%c",
					   exp->e_paren ? '(' : ' ');	/* ) */
			/* FALL THROUGH */
		  case EIDENT:
			sprintp += sprintf(sprintp, "%s", exp->e_sym->name);
			break;
		  case ENUMBER:
			sprintp += sprintf(sprintp, "%ld", exp->e_val);
		}
		break;
	  case 1:
		sprintp += sprintf(sprintp, "%s", opname[(int)exp->e_op]);
		sprintexpr(exp->e_kid);
		break;
	  case 2:
		sprintexpr(exp->e_left);
		sprintp += sprintf(sprintp, " %s ", opname[(int)exp->e_op]);
		sprintexpr(exp->e_right);
		break;
	  case 3:
		sprintexpr(exp->e_cond);
		sprintp += sprintf(sprintp, " ? ");
		sprintexpr(exp->e_true);
		sprintp += sprintf(sprintp, " : ");
		sprintexpr(exp->e_false);
	}
	if (exp->e_paren)
		sprintp += sprintf(sprintp, ")");
}

static void
doifdef(enum keytype key, char *line, enum symtype type)
{
	if (type == SRESERVED)
		type = SDONTCARE;
	if (key == IFNDEF && type != SDONTCARE) {
		if (type == SDEFINED)
			type = SUNDEFINED;
		else
			type = SDEFINED;
	}
	doif(key, line, type);
}

static void
doif(enum keytype key, char *line, enum symtype type)
{
	if (++ifsp >= &ifstack[MAXIFDEPTH]) {
		yyerror("too much #if nesting");
		terminate();
	}
	ifsp->type = type;
	ifsp->key = key;
	switch (type) {
	  case SDONTCARE:
		ifsp->endifs++;
		putstr(line);
		break;
	  case SUNDEFINED:
		falselevel++;
		break;
	  case SDEFINED:
		++ifsp->defines;
	}
}

static void
doelif(char *line, struct expr *exp)
{
	enum symtype type;
	enum keytype key;

	if (ifsp == &ifstack[-1]) {
		yyerror("dangling #elif");
		return;
	}
	if (ifsp->key == ELSE) {
		yyerror("#elif after #else");
		return;
	}
	type = evalexpr(exp);
	key = ELIF;
	switch (ifsp->type) {
	  case SDONTCARE:
		switch (type) {
		  case SDONTCARE:
			--ifsp->endifs;
			break;
		  case SDEFINED:
		  case SUNDEFINED:
			putstr("#else\n");
			key = SENTINEL;
		}
		break;
	  case SDEFINED:
		switch (type) {
		  case SDONTCARE:
			if (ifsp->type == SENTINEL)
				ifsp->endifs++;
			key = IF;
			break;
		  case SDEFINED:
			if (exp->e_val != 0)
				type = SUNDEFINED;
		}
		break;
	  case SUNDEFINED:
		switch (type) {
		  case SDONTCARE:
			if (ifsp->type == SENTINEL)
				ifsp->endifs++;
			key = IF;
		}
		--falselevel;
	}
	--ifsp;
	doifexpr(key, line, type, exp);
}

static void
doelse(char *line)
{
	if (ifsp == &ifstack[-1]) {
		yyerror("dangling #else");
		return;
	}
	if (ifsp->key == ELSE) {
		yyerror("duplicate #else");
		return;
	}
	switch (ifsp->type) {
	  case SDONTCARE:
		ansify(line, LINESIZE);
		putstr(line);
		break;
	  case SDEFINED:
		ifsp->type = SUNDEFINED;
		falselevel++;
		break;
	  case SUNDEFINED:
		if(ifsp->defines == 0) {
			ifsp->type = SDEFINED;
			--falselevel;
		}
	}
	ifsp->key = ELSE;
}

static void
doendif(char *line)
{
	int endifs;

	if (ifsp == &ifstack[-1]) {
		yyerror("dangling #endif");
		return;
	}
	switch (ifsp->type) {
	  case SUNDEFINED:
		--falselevel;
	}
	endifs = ifsp->endifs;
	if (endifs > 0) {
		ansify(line, LINESIZE);
		do {
			putstr(line);
		} while (--endifs > 0);
		ifsp->endifs = 0;
	}
	ifsp->defines = 0;
	--ifsp;
}
