/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)env.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <errno.h>

main(argc, argv)
	int argc;
	char **argv;
{
	extern char **environ;
	extern int errno, optind;
	register char **ep, *p;
	register char *cp;
	char *cleanenv[1];
	int ch;

	if ((cp = strrchr(argv[0], '/')) != NULL)
		cp++;
	else
		cp = argv[0];

	/* if "printenv" then do this */
		
	if (!strcmp(cp, "printenv")) {
		register char **ep;
		register int len;

		/* if printenv with no args then print all environment variables */

		if (argc < 2) {
			for (ep = environ; *ep; ep++)
				(void) printf("%s\n",*ep);
			exit(0);
		}

		/* Given arg. print out the value of the envronment variable */

		len = strlen(*++argv);
		for (ep = environ; *ep; ep++)
			if (!strncmp(*ep, *argv, len)) {
				cp = *ep + len;
				if (!*cp || *cp == '=') {
					(void) printf("%s\n",*cp ? cp + 1 : cp);
					exit(0);
				}
			}
		exit(1);
	}

	argv++;
	argc--;
	if(argc && (strcmp(*argv, "-i") == 0 || strcmp(*argv, "-") == 0)){
		environ = cleanenv;
		cleanenv[0] = NULL;
		argv++;
		argc--;
	}

	if(argc && strcmp(*argv, "--") == 0){
		argv++;
		argc--;
	}

	if (argc && strcmp(*argv,"-?") == 0) {
		(void)fprintf(stderr, "usage: env [-] [name=value ...] [command]\n");
                exit(1);
	}

	for (; (*argv != NULL) && (p = strchr(*argv, '=')); ++argv)
		(void)putenv(*argv);
	if (*argv) {
		execvp(*argv, argv);
		(void)fprintf(stderr, "env: %s: %s\n", *argv,
		    strerror(errno));
		if(errno == ENOENT) exit(127);
		if(errno == EACCES) exit(126);
	}
	for (ep = environ; *ep; ep++)
		(void)printf("%s\n", *ep);
	exit(0);
}
