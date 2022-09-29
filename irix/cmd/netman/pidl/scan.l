%{
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Lex specification for PIDL.
 */
#include <bstring.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "expr.h"
#include "heap.h"
#include "macros.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"
#include "x.tab.h"

extern int	yychar;		/* imported from the yacc parser */
extern int	indecl;		/* true if we're in a declarator */

char		*yyfilename;	/* exported to pidl_tree.c */
int		rpclmode;	/* exported to main() in pidl.c */

#define	LINESIZE	512
#define	PUSHSIZE	128

typedef struct inputstack {
	char			*i_name;
	FILE			*i_file;
	unsigned short		i_rpcl;
	unsigned short		i_lineno;
	char			i_linebuf[LINESIZE];
	int			i_count;
	char			*i_nextcp;
	char			i_pushbuf[PUSHSIZE];
	char			*i_pushbp;
	struct inputstack	*i_down;
} InputStack;

static InputStack *instack;

static int	nextline(InputStack *);
#ifdef sun
static int	overflow();
#else
static void	overflow();
#endif
static void	pushback(char *, int);
static int	scanimport();
static int	scanaddress(char *, Address *);

#undef input
#define	input()		((instack->i_pushbp < instack->i_pushbuf+PUSHSIZE) ? \
			 *instack->i_pushbp++ : ((--instack->i_count >= 0) ? \
			 *instack->i_nextcp++ : nextline(instack)))
#undef unput
#ifdef sun
#define	unput(c)	((instack->i_pushbp > instack->i_pushbuf) ? \
			 (*--instack->i_pushbp = (c)) : overflow())
#else
#define	unput(c)	((instack->i_pushbp > instack->i_pushbuf) ? \
			 (void)(*--instack->i_pushbp = (c)) : overflow())
#endif

#define	RPCL_KEYWORD(t)	if (instack->i_rpcl) return t; goto name
%}

%Start	Comm1 Comm2 PassThru Token

BEGINCOMM1	"/*"
ENDCOMM1	"*/"
BEGINCOMM2	"//"
ENDCOMM2	\n
BEGINPASS	"%{"
ENDPASS		"%}"

%%
	/*
	 * Witchcraft.
	 */
	BEGIN Token;

<Comm1>{BEGINCOMM1}		{ yyerror("nested comment"); }
<Comm1>{ENDCOMM1}		{ BEGIN Token; }
<Comm1>\n			{ }
<Comm1>.			{ }
<Comm2>{ENDCOMM2}		{ BEGIN Token; }
<Comm2>.			{ }
<Token>{BEGINCOMM1}		{ BEGIN Comm1; }
<Token>{BEGINCOMM2}		{ BEGIN Comm2; }
<Token>{ENDCOMM1}		{ yyerror("extra comment terminator"); }
<Token>[ \t\n]+			{ }

<Token>^%pragma[ \t].*		{ addpragma(&yytext[8]); }
<Token>^{BEGINPASS}		{ if (instack->i_down == 0) {
					if (ptfile == 0) {
						ptfile = fdopen(mkstemp(ptname),
								"w+");
					}
					if (ptfile == 0) {
						yyerror(
					    "can't create temporary file %s",
							ptname);
					} else {
						fprintf(ptfile,
							"\n#line %d \"%s\"\n",
							yylineno, yyfilename ?
							yyfilename : "");
					}
				  }
				  BEGIN PassThru;
				}
<Token>^{ENDPASS}		{ yyerror("extra pass-through terminator"); }

<PassThru>^{BEGINPASS}		{ yyerror("nested pass-through"); }
<PassThru>^{ENDPASS}		{ BEGIN Token; }
<PassThru>\n			{ if (instack->i_down == 0)
					putc(yytext[0], ptfile); }
<PassThru>.+			{ if (instack->i_down == 0)
					fputs(yytext, ptfile); }

<Token>bitset			{ yylval.op = opBitSet; return ENUM; }
<Token>const			{ return CONST; }
<Token>define			{ return DEFINE; }
<Token>enum			{ yylval.op = opEnum; return ENUM; }
<Token>expect			{ return EXPECT; }
<Token>export			{ return EXPORT; }
<Token>import			{ if (scanimport()) return IMPORT; }
<Token>protocol			{ return PROTOCOL; }
<Token>struct			{ return STRUCT; }
<Token>typedef			{ return TYPEDEF; }
<Token>if			{ return IF; }
<Token>else			{ return ELSE; }
<Token>switch			{ return SWITCH; }
<Token>case			{ return CASE; }
<Token>default			{ return DEFAULT; }
<Token>footer			{ return FOOTER; }
<Token>nest			{ return NEST; }
<Token>sizeof			{ return SIZEOF; }
<Token>union			{ RPCL_KEYWORD(UNION); }
<Token>program			{ RPCL_KEYWORD(PROGRAM); }
<Token>version			{ RPCL_KEYWORD(VERSION); }

<Token>big-endian		{ yylval.val = drBigEndian; return DREP; }
<Token>little-endian		{ yylval.val = drLittleEndian; return DREP; }
<Token>XDR			{ yylval.val = drXDR; return DREP; }
<Token>ASN\.1			{ yylval.val = drASN1; return DREP; }

<Token>-v+			{ yylval.val = yyleng - 1; return LEVEL; }

<Token>[A-Za-z_][A-Za-z_0-9]*	{ hash_t hash;
				  Symbol *sym;

				name:
				  hash = hashname(yytext, yyleng);
				  sym = find(innermost, yytext, yyleng, hash);
				  if (sym == 0) {
					yylval.sym =
					    enter(&strings, yytext, yyleng,
						  hash, stNil, sfSaveName);
				  } else {
					yylval.sym = sym;
					if (sym->s_flags & sfTypeName) {
						if (!instack->i_rpcl)
							return TYPE;
						if (!indecl)
							return RPCLTYPE;
				  	}
				  }
				  return NAME;
				}
<Token>[0-9]+			{ yylval.val = strtol(yytext, 0, 0);
				  return NUMBER; }
<Token>0[Xx][0-9A-Fa-f]+	{ yylval.val = strtoul(yytext, 0, 16);
				  return NUMBER; }
<Token>\"[^\n"]*		{ if (yytext[yyleng-1] == '\\')
					yymore();
				  else {
					int c, len;
					Expr *ex;

					yytext[yyleng++] = c = input();
					if (c == '\n')
						yyerror("unterminated string");
					if (scanaddress(yytext+1, &yylval.addr))
						return ADDRESS;
					ex = ex_string(yytext+1, yyleng-2);
					len = ex->ex_str.s_len;
					yylval.sym =
					    enter(&strings, yytext+1, len,
						  hashname(yytext+1, len),
						  stNil, sfSaveName);
					ex_destroy(ex);
					return STRING;
				  }
				}

<Token>[{}[\];,=?:]		{ return yytext[0]; }
<Token>"||"			{ return OR; }
<Token>"&&"			{ return AND; }
<Token>"=="			{ yylval.op = opEQ; return EQOP; }
<Token>"!="			{ yylval.op = opNE; return EQOP; }
<Token>[<>]			{ return yytext[0]; }
<Token>"<="			{ yylval.op = opLE; return RELOP; }
<Token>">="			{ yylval.op = opGE; return RELOP; }
<Token>[|^&]			{ return yytext[0]; }
<Token>"<<"			{ yylval.op = opLsh; return SHOP; }
<Token>">>"			{ yylval.op = opRsh; return SHOP; }
<Token>[+\-*]			{ return yytext[0]; }
<Token>"/"			{ yylval.op = opDiv; return DIVOP; }
<Token>"%"			{ yylval.op = opMod; return DIVOP; }
<Token>"!"			{ yylval.op = opNot; return UNOP; }
<Token>"~"			{ yylval.op = opBitNot; return UNOP; }
<Token>\$ds			{ yylval.op = opDataStream; return MACHREG; }
<Token>\$ps			{ yylval.op = opProtoStack; return MACHREG; }
<Token>[.()$]			{ return yytext[0]; }

<Token>.			{ yyerror("illegal character"); }

%%

static char *severities[] = { "Warning", "Error", "Internal" };

void
vferror(enum severity sev, char *filename, int lineno, char *format, va_list ap)
{
	InputStack *ip;
	int dot, col, tab;
	char *bp;

	fprintf(stderr, "%s: %s: ", exc_progname, severities[sev]);
	if (filename)
		fprintf(stderr, "%s, ", filename);
	if (lineno != 0)
		fprintf(stderr, "%d: ", lineno);
	vfprintf(stderr, format, ap);
	fputs(".\n", stderr);
	ip = instack;
	if (ip) {	/* XXX if (ip == 0) open & read till line */
		fputs(ip->i_linebuf, stderr);
		if (ip->i_nextcp[ip->i_count-1] != '\n')
			putc('\n', stderr);
		dot = ip->i_nextcp - (ip->i_pushbuf + PUSHSIZE - ip->i_pushbp)
		      - yyleng - ip->i_linebuf;
		for (bp = ip->i_linebuf, col = 0; --dot >= 0; bp++, col++) {
			if (*bp == '\t') {
				for (tab = 8 - col % 8; --tab > 0; col++)
					putc('.', stderr);
			}
			putc('.', stderr);
		}
		fputs("^\n", stderr);
	}
	if (sev != WARNING && ++status >= 10) {
		fprintf(stderr, "%s: sorry, too many errors.\n", exc_progname);
		terminate(0);
	}
}

void
yyerror(char *format, ...)
{
	va_list ap;

	if (!strcmp(format, "syntax error")) {
		if (yychar == 0)
			format = "unexpected end of file";
		else if (yychar == '$') {
			if (instack->i_down)
				format = "unexpected end of imported module";
			else
				format = "illegal character";
		}
	}
	va_start(ap, format);
	vferror(ERROR, yyfilename, yylineno, format, ap);
	va_end(ap);
}

void
pushinput(FILE *file, char *name, int namlen)
{
	InputStack *ip;

	ip = new(InputStack);
	ip->i_name = yyfilename = name;
	ip->i_file = file;
	ip->i_rpcl = (instack == 0) ? rpclmode : !strcmp(name+namlen-3, ".x");
	ip->i_lineno = yylineno = 0;
	ip->i_count = 0;
	ip->i_pushbp = ip->i_pushbuf + PUSHSIZE;
	ip->i_down = instack;
	instack = ip;
}

typedef struct importdir {
	char		 *id_name;
	struct importdir *id_next;
} ImportDir;

static ImportDir *importdirs;
static ImportDir **lastimport = &importdirs;

void
addimportdir(char *name)
{
	ImportDir *id;

	id = new(ImportDir);
	id->id_name = name;
	id->id_next = 0;
	*lastimport = id;
	lastimport = &id->id_next;
}

static FILE *
findimport(char *name, int namlen, char *path, int *pathlen)
{
	char newname[_POSIX_PATH_MAX];
	int absolute;
	FILE *file;
	ImportDir *id;

	if (strncmp(&name[namlen-3], ".pi", 3)) {
		namlen = nsprintf(newname, _POSIX_PATH_MAX, "%s.pi", name);
		name = newname;
	}
	absolute = (name[0] == '/');
	if (!absolute) {
		*pathlen = nsprintf(path, _POSIX_PATH_MAX, "%s/%s",
				    dirname(instack->i_name), name);
	} else {
		(void) strcpy(path, name);
		*pathlen = namlen;
	}
	file = fopen(path, "r");
	if (file)
		return file;
	if (!absolute) {
		for (id = importdirs; id; id = id->id_next) {
			*pathlen = nsprintf(path, _POSIX_PATH_MAX, "%s/%s",
					    id->id_name, name);
			file = fopen(path, "r");
			if (file)
				return file;
		}
	}
	return 0;
}

static void
pushback(char *s, int len)
{
	InputStack *ip;

	ip = instack;
	if (ip == 0)
		return;
	ip->i_pushbp -= len;
	if (ip->i_pushbp < ip->i_pushbuf)
		overflow();
	bcopy(s, ip->i_pushbp, len);
}

static void
endimport()
{
	static char import[] = "import ";

	if (yylex() == ',')
		pushback(import, constrlen(import));
	else
		pushback(yytext, yyleng);
	unput('$');	/* balance { */
	unput('}');
}

static int
scanimport()
{
	int token, pathlen;
	Symbol *sym;
	char path[_POSIX_PATH_MAX];
	FILE *file;
	hash_t hash;

	token = yylex();
	if (token != NAME && token != STRING) {
		yyerror("name expected after import");
		return 0;
	}
	sym = yylval.sym;
	file = findimport(sym->s_name, sym->s_namlen, path, &pathlen);
	if (file == 0) {
		yyerror("can't find import module %s", sym->s_name);
		return 0;
	}
	hash = hashname(path, pathlen);
	sym = lookup(&imports, path, pathlen, hash);
	if (sym == 0) {
		sym = enter(&imports, path, pathlen, hash, stNil, sfSaveName);
		pushinput(file, sym->s_name, pathlen);
	} else {
		fclose(file);
		endimport();
	}
	unput('{');		/* balance } */
	yylval.sym = sym;
	return 1;
}

static int
yywrap()
{
	InputStack *ip;

	ip = instack;
	fclose(ip->i_file);
	instack = ip->i_down;
	delete(ip);
	if ((ip = instack) == 0)
		return 1;
	yyfilename = ip->i_name;
	yylineno = ip->i_lineno;
	endimport();
	return 0;
}

static int
scanaddress(char *s, Address *a)
{
	int n, v[5];
	unsigned long b;
	char *p;
	unsigned char *vp;

	n = 0;
	for (;;) {
		b = strtoul(s, &p, 16);
		if (p == s || b > 255)
			return 0;
		v[n++] = b;
		if (*p == '\0' || *p == '"')
			break;
		if (*p != ':' || n == sizeof a->a_vec)
			return 0;
		s = p + 1;
	}
	vp = &a->a_vec[sizeof a->a_vec];
	while (--n >= 0)
		*--vp = v[n];
	while (vp > &a->a_vec[0])
		*--vp = 0;
	return 1;
}

static int
nextline(InputStack *ip)
{
	char *bp;
	int lastc, count, c;

	if (ip == 0)
		return 0;
	bp = ip->i_nextcp = ip->i_linebuf;
	lastc = 0;
	count = sizeof ip->i_linebuf;
	while (--count > 0) {
		c = getc(ip->i_file);
		if (c == EOF)
			break;
		if (c == '\n') {
			yylineno = ++ip->i_lineno;
			if (lastc == '\\') {
				--bp, count++;
				continue;
			}
			*bp++ = c;
			break;
		}
		*bp++ = lastc = c;
	}

	*bp = '\0';
	ip->i_count = bp - ip->i_linebuf;
	if (ip->i_count == 0)
		return 0;
	return input();
}

#ifdef sun
static int
#else
static void
#endif
overflow()
{
	yyerror("too much input pushback");
	terminate(0);
}
