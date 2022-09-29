/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * PIDL compiler.
 */
#include <bstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "analyze.h"
#include "debug.h"
#include "generate.h"
#include "getopt.h"
#include "heap.h"
#include "pidl.h"
#include "scope.h"
#include "treenode.h"

/*
 * Import variables.
 */
extern int	yydebug;
extern int	rpclmode;

/*
 * Globals and exported variables.
 */
Scope		*innermost;
Scope		strings;
Scope		imports;
unsigned short	lastfid;
TreeNode	*parsetree;
int		status;
FILE		*ptfile;
char		ptname[] = "/tmp/pidl.XXXXXX";
FILE		*bbfile;

/*
 * Local functions and data.
 */
static void	compile(char *);
static FILE	*openoutput(char *, char *, char *);
static void	closeoutput(FILE **, char *);

static Scope	pragmas;
static char	cname[_POSIX_NAME_MAX];
static char	hname[_POSIX_NAME_MAX];

main(int argc, char **argv)
{
	int opt;

	exc_progname = argv[0];
	initscope(&pragmas, "pragmas", 0, 0);

	while ((opt = getopt(argc, argv, "I:T:dp:x")) != EOF) {
		switch (opt) {
		  case 'I':
			addimportdir(optarg);
			break;
		  case 'T':
			bbfile = fopen(optarg, "w");
			if (bbfile == 0)
				exc_perror(errno, "can't open %s", optarg);
			break;
		  case 'd':
			yydebug++;
			break;
		  case 'p':
			(void) addpragma(optarg);
			break;
		  case 'x':
			rpclmode = 1;
			break;
		  default:
			fprintf(stderr,
			    "usage: %s [-I dir] [-p pragma] [-x] [name ...]\n",
				exc_progname);
			exit(-1);
		}
	}
	argc -= optind, argv += optind;

	(void) sigset(SIGHUP, terminate);
	(void) sigset(SIGINT, terminate);
	(void) sigset(SIGTERM, terminate);
	if (argc == 0)
		compile("-");
	else {
		while (--argc >= 0)
			compile(*argv++);
	}
	terminate(0);
	/* NOTREACHED */
}

static void
compile(char *name)
{
	FILE *file;
	char *base;
	int namlen, rpcl, save;
	Symbol *modsym;
	Scope globals;
	Context context;

	/*
	 * Open the named input file, or use stdin.
	 */
	if (!strcmp(name, "-")) {
		file = stdin;
		name = base = 0;
		namlen = rpcl = 0;
	} else {
		file = fopen(name, "r");
		if (file == 0) {
			exc_perror(errno, "can't open %s", name);
			status++;
			return;
		}
		namlen = strlen(name);
		if (!strncmp(name + namlen - 2, ".x", 2)) {
			save = rpclmode;
			rpcl = rpclmode = 1;
			base = basename(name, ".x");
		} else {
			rpcl = 0;
			base = basename(name, ".pi");
		}
		base = strdup(base);
	}

	/*
	 * Initialize scopes, types, and the lexical scanner's input stack.
	 */
	yyfilename = name;
	initscope(&strings, "strings", 0, 0);
	initscope(&imports, "imports", 0, 0);
	initscope(&globals, base, 0, 0);
	installtypes(&globals);
	innermost = &globals;
	modsym = enter(&imports, name, namlen, hashname(name,namlen), stNil, 0);
	pushinput(file, name, namlen);

	/*
	 * Parse and analyze.  PIDL allows forward field references, so two
	 * passes are necessary.
	 */
	parsetree = 0;
	if (yyparse() == 0 && !status) {
		bzero(&context, sizeof context);
		context.cx_modsym = modsym;
		context.cx_scope = &globals;
		context.cx_lookfun = find;
		if (analyze(parsetree, &context)) {
			/*
			 * Open output files and generate code.
			 */
			if (name)
				cfile = openoutput(base, ".c", cname);
			else
				cfile = stdout;
			if (name && (context.cx_flags & cxHeaderFile))
				hfile = openoutput(base, ".h", hname);
			else
				hfile = 0;
			if (cfile) {
				generate(parsetree, &context, cname);
				if (cfile != stdout)
					closeoutput(&cfile, cname);
			}
			if (hfile)
				closeoutput(&hfile, hname);
		}
	}

	/*
	 * Clean up pass-thru file and free all dynamic store.
	 */
	if (ptfile) {
		(void) fclose(ptfile);
		(void) unlink(ptname);
		ptfile = 0;
	}
	destroytree(parsetree);
	freescope(&globals);
	freescope(&imports);
	freescope(&strings);
	delete(base);

	/*
	 * Reset per-file global variables.
	 */
	if (rpcl)
		rpclmode = save;
	lastfid = 0;
}

/*
 * Suffixed output file open/create.
 */
static FILE *
openoutput(char *base, char *suffix, char *name)
{
	FILE *file;

	(void) nsprintf(name, _POSIX_NAME_MAX, "%s%s", base, suffix);
	file = fopen(name, "w");
	if (file == 0) {
		exc_perror(errno, "can't create %s", name);
		status++;
		return 0;
	}
	return file;
}

static void
closeoutput(FILE **filep, char *name)
{
	if (fclose(*filep) == EOF) {
		exc_perror(errno, "can't write to %s", name);
		status++;
		terminate(0);
	}
	*filep = 0;
}

Pragma *
addpragma(char *args)
{
	char *prot, *name, *value;
	int len;
	hash_t hash;
	Symbol *sym;
	Pragma *p;

	prot = args;
	name = strchr(prot, '.');
	if (name == 0) {
		yyerror("missing '.' after protocol in pragma %s", args);
		return 0;
	}
	*name++ = '\0';
	value = strchr(name, '=');
	if (value) {
		*value++ = '\0';
		value = strdup(value);
	}
	len = strlen(prot);
	hash = hashname(prot, len);
	sym = lookup(&pragmas, prot, len, hash);
	if (sym == 0) {
		sym = enter(&pragmas, prot, len, hash, stNil, sfSaveName);
		sym->s_pragmas = 0;
	}
	for (p = sym->s_pragmas; p; p = p->p_next) {
		if (!strcmp(p->p_name, name)) {
			delete(p->p_value);
			p->p_value = value;
			return p;
		}
	}
	p = new(Pragma);
	p->p_refcnt = 1;
	p->p_sym = sym;
	p->p_name = strdup(name);
	p->p_value = value;
	p->p_next = sym->s_pragmas;
	sym->s_pragmas = p;
	return p;
}

char *
haspragma(Symbol *sym, char *name)
{
	Symbol *psym;
	Pragma *p;

	psym = lookup(&pragmas, sym->s_name, sym->s_namlen, sym->s_hash);
	if (psym == 0)
		return 0;
	for (p = psym->s_pragmas; p; p = p->p_next) {
		if (!strcmp(p->p_name, name))
			return p->p_value;
	}
	return 0;
}

void
deletepragma(Pragma *p)
{
	Pragma **qp, *q;

	for (qp = &p->p_sym->s_pragmas; (q = *qp) != 0; qp = &q->p_next) {
		if (q == p) {
			if (--p->p_refcnt != 0)
				return;
			*qp = p->p_next;
			delete(p->p_name);
			delete(p->p_value);
			delete(p);
			return;
		}
	}
}

char *
basename(char *name, char *suffix)
{
	char *p;
	static char base[_POSIX_NAME_MAX];

	if (*name == '\0')
		return ".";
	if ((p = strrchr(name, '/')) == 0)
		(void) strcpy(base, name);
	else {
		do {
			if (p[1] != '\0') {
				(void) strcpy(base, p + 1);
				break;
			}
			*p = '\0';
		} while ((p = strrchr(name, '/')) != 0);
	}
	p = base + strlen(base) - strlen(suffix);
	if (!strcmp(p, suffix))
		*p = '\0';
	return base;
}

char *
dirname(char *name)
{
	char *slash;
	static char dir[_POSIX_PATH_MAX];

	if (name == 0 || (slash = strrchr(name, '/')) == 0)
		return ".";
	slash = strcpy(dir, name) + (slash - name);
	do {
		*slash = '\0';
		if (slash[1] != '\0')
			break;
	} while ((slash = strrchr(dir, '/')) != 0);
	return dir;
}

void
terminate(int signo)
{
	if (ptfile)
		(void) unlink(ptname);
	if (status) {
		if (cfile && cfile != stdout)
			(void) unlink(cname);
		if (hfile)
			(void) unlink(hname);
	}
	exit(signo ? signo : status);
}
