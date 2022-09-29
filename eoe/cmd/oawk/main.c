/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)awk:main.c	1.5" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/oawk/RCS/main.c,v 1.6 1993/10/14 06:37:31 jfk Exp $"
#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "sys/signal.h"
#include "awk.def"
#include "awk.h"
#define TOLOWER(c)	(isupper(c) ? tolower(c) : c) /* ugh!!! */

int	dbg	= 0;
int	svflg	= 0;
int	rstflg	= 0;
int	svargc;
char	**svargv, **xargv;
extern FILE	*yyin;	/* lex input file */
char	*lexprog;	/* points to program argument if it exists */
extern	errorflag;	/* non-zero if any syntax errors; set by yyerror */

int filefd, symnum, ansfd;
char *filelist;
extern int maxsym, errno;

#ifdef sgi
void
error_handler(int x)
{
	if (errorflag) {
		printf("awk: too many errors; cannot continue.\n");
		exit(errorflag);
	} else {
		printf("awk: unexpected exception.  Dumping core.\n");
		abort();
	}
}
#endif
	
main(argc, argv) int argc; char *argv[]; {
	if (argc == 1) {
usage:
		fprintf(stderr,
			"awk: Usage: awk [-Fc] [-f source | 'cmds'] [parameters] [files]\n");
		exit(2);
		}
	syminit();
	while (argc > 1) {
		argc--;
		argv++;
		/* this nonsense is because gcos argument handling */
		/* folds -F into -f.  accordingly, one checks the next
		/* character after f to see if it's -f file or -Fx.
		*/
		if (argv[0][0] == '-' &&
#ifdef	sgi
			       argv[0][1] == 'f' &&
#else
			       TOLOWER(argv[0][1]) == 'f' &&
#endif
			       argv[0][2] == '\0') {
			if (argc == 1) {
				goto usage;
			}
			yyin = fopen(argv[1], "r");
			if (yyin == NULL)
				error(FATAL, "can't open %s", argv[1]);
			argc--;
			argv++;
			break;
		} else if (argv[0][0] == '-' &&
#ifdef	sgi
				      argv[0][1] == 'F'
#else
				      TOLOWER(argv[0][1]) == 'f'
#endif
				      ) {	/* set field sep */
			if (argv[0][2] == 't')	/* special case for tab */
				**FS = '\t';
			else
				**FS = argv[0][2];
			continue;
		} else if (argv[0][0] != '-') {
			dprintf("cmds=|%s|\n", argv[0], NULL, NULL);
			yyin = NULL;
			lexprog = argv[0];
			argv[0] = argv[-1];	/* need this space */
			break;
		} else if (strcmp("-d", argv[0])==0) {
			dbg = 1;
		}
		else if(strcmp("-S", argv[0]) == 0) {
			svflg = 1;
		}
		else if(strncmp("-R", argv[0], 2) == 0) {
			if(thaw(argv[0] + 2) == 0)
				rstflg = 1;
			else {
				fprintf(stderr, "not restored\n");
				exit(1);
			}
		}
	}
	if (argc <= 1) {
		argv[0][0] = '-';
		argv[0][1] = '\0';
		argc++;
		argv--;
	}
	svargc = --argc;
	svargv = ++argv;
	dprintf("svargc=%d svargv[0]=%s\n", svargc, svargv[0], NULL);
	*FILENAME = *svargv;	/* initial file name */

	if(rstflg == 0) 
#ifdef sgi
        {
		/*
		 * If we get unexpected syntax errors during parsing,
		 * the parser can go off track and self-destruct.  To
		 * avoid this possibility, we add an error handler which
		 * checks to see if a syntax error occurs and prints a 
		 * vaguely friendly message if one does.
		 */
		signal(SIGSEGV, error_handler);
		signal(SIGBUS, error_handler);
		yyparse();
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
	}
#else
		yyparse();
#endif
	dprintf("errorflag=%d\n", errorflag, NULL, NULL);
	if (errorflag)
		exit(errorflag);
	if(svflg) {
		svflg = 0;
		if(freeze("awk.out") != 0)
			fprintf(stderr, "not saved\n");
		exit(0);
	}
	run(winner);
	exit(errorflag);
}


yywrap()
{
	return(1);
}
