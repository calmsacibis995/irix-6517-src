/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)awk:main.c	2.15"

#define DEBUG
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <sys/euc.h>
#include <getwidth.h>
eucwidth_t WW;

#define	CMDCLASS	"UX:"	/* Command classification */

/* SGI CHANGE: I presume we want the locale stuff */
#include <locale.h>

#include "awk.h"
#include "y.tab.h"

char	*version;
/* _XPG Saved command line variables settings for post BEGIN processing. */
char	*Argv[ARG_MAX];	
int	Argc = 0;
int	_XPG;

int	dbg	= 0;
uchar	*cmdname;	/* gets argv[0] for error messages */
extern	FILE	*yyin;	/* lex input file */
uchar	*lexprog;	/* points to program argument if it exists */
extern	int errorflag;	/* non-zero if any syntax errors; set by yyerror */
int	compile_time = 2;	/* for error printing: */
				/* 2 = cmdline, 1 = compile, 0 = running */

uchar	*pfile[20];	/* program filenames from -f's */
int	npfile = 0;	/* number of filenames */
int	curpfile = 0;	/* current filename */

extern const char badopen[];
extern const char outofspace[];

char *getenv();

main(argc, argv, envp)
	int argc;
	uchar *argv[], *envp[];
{
	uchar *fs = NULL;
	char *xpgstr;
	char label[MAXLABEL+1];	/* Space for the catalogue label */
	extern void fpecatch();

	(void)setlocale(LC_ALL, "");
	{getwidth(&WW); WW._eucw2++; WW._eucw3++;}
	cmdname = ((cmdname = (uchar*) strrchr ((char*) argv[0], '/')) ?
		++cmdname : argv[0]);
	(void)strcpy(label, CMDCLASS);
	(void)strncat(label, (char*) cmdname, (MAXLABEL - sizeof(CMDCLASS) - 1));
	(void)setcat("uxawk");
	(void)setlabel(label);
	version = (char*) gettxt(":31", "version Oct 11, 1989");
 	if (argc == 1) {
		pfmt(stderr, MM_ERROR, ":32:Incorrect usage\n");
		pfmt(stderr, MM_ACTION,
			":33:Usage: %s [-f programfile | 'program'] [-Ffieldsep] [-v var=value] [files]\n",
			cmdname);
		exit(1);
	}
	signal(SIGFPE, fpecatch);
	yyin = NULL;
	syminit();
	while (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0') {
		if (strcmp((char*) argv[1], "--") == 0) {	/* explicit end of args */
			argc--;
			argv++;
			break;
		}
		switch (argv[1][1]) {
		case 'f':	/* next argument is program filename */
			argc--;
			argv++;
			if (argc <= 1)
				error(MM_ERROR, ":34:No program filename");
			pfile[npfile++] = argv[1];
			break;
		case 'F':	/* set field separator */
			if (argv[1][2] != 0) {	/* arg is -Fsomething */
				if (argv[1][2] == 't' && argv[1][3] == 0)	/* wart: t=>\t */
					fs = (uchar *) "\t";
				else if (argv[1][2] != 0)
					fs = &argv[1][2];
			} else {		/* arg is -F something */
				argc--; argv++;
				if (argc > 1 && argv[1][0] == 't' && argv[1][1] == 0)	/* wart: t=>\t */
					fs = (uchar *) "\t";
				else if (argc > 1 && argv[1][0] != 0)
					fs = &argv[1][0];
			}
			if (fs == NULL || *fs == '\0')
				error(MM_WARNING, ":35:Field separator FS is empty");
			break;
		case 'v':	/* -v a=1 to be done NOW.  one -v for each */
			if (argv[1][2] == '\0' && --argc > 1) {
			    if (isclvar((++argv)[1]))
				setclvar(argv[1]);
			    else
				error(MM_WARNING, ":41:Invalid variable setting '%s'", argv[1]);
			} else
				error(MM_ERROR, ":40:No variable specified after -v switch");
			break;
		case 'd':
			dbg = atoi(&argv[1][2]);
			if (dbg == 0)
				dbg = 1;
			pfmt(stdout, (MM_INFO | MM_NOGET), "%s %s\n",
				cmdname, version);
			break;
		default:
			pfmt(stderr, MM_WARNING,
				":36:Unknown option %s ignored\n", argv[1]);
			break;
		}
		argc--;
		argv++;
	}
	/* argv[1] is now the first argument */
	if (npfile == 0) {	/* no -f; first argument is program */
		if (argc <= 1)
			error(MM_ERROR, ":37:No program given");
		dprintf( ("program = |%s|\n", argv[1]) );
		lexprog = argv[1];
		argc--;
		argv++;
	}
	compile_time = 1;
	argv[0] = cmdname;	/* put prog name at front of arglist */
	dprintf( ("argc=%d, argv[0]=%s\n", argc, argv[0]) );

	/* SGI HACK: Lots of programs assume that nawk performs the
	 * leading set of variable initializations before it starts
	 * parsing the program (probably because the old nawk did this).
	 * The Awk book implies this behavior is wrong, but the book 
	 * is kind of ambiguous about exactly when the first file is
	 * opened, and a number of our scripts assume that the initialization
	 * has occurred before the BEGIN clause is executed.
	 */

	if (xpgstr = getenv("_XPG"))
		_XPG = atoi(xpgstr);

	while (argc > 1) {
		if (!isclvar(argv[1]))
			break;
		if(_XPG == 1) {
			if((Argv[Argc] = malloc(strlen(argv[1])+1)) == NULL)
				error(MM_ERROR, outofspace, "save command args");
			strcpy(Argv[Argc++],argv[1]);
		} else  setclvar(argv[1]);
		argc--;
		argv++;
	}
	argv[0] = cmdname;

	arginit(argc, argv);
	envinit(envp);
	yyparse();
	if (fs)
		*FS = tostring(qstring(fs, '\0'));
	dprintf( ("errorflag=%d\n", errorflag) );
	if (errorflag == 0) {
		compile_time = 0;
		run(winner);
	} else
		bracecheck();
	exit(errorflag);
}

pgetc()		/* get program character */
{
	int c;

	for (;;) {
		if (yyin == NULL) {
			if (curpfile >= npfile)
				return EOF;
			if ((yyin = fopen((char *) pfile[curpfile], "r")) == NULL)
				error(MM_ERROR, badopen,
					pfile[curpfile], strerror(errno));
		}
		if ((c = getc(yyin)) != EOF)
			return c;
		yyin = NULL;
		curpfile++;
	}
}
