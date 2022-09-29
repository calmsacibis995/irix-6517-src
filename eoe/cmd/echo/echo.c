/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)echo:echo.c	1.3.1.1"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

main(argc, argv)
int argc;
char **argv;
{
	register char	*cp;
	register int	i, wd;
	int	j;
	int fnewline = 1;
	char *xpg;                 /* "on": use xpg standard for -n optin */

	if(--argc == 0) {
		putchar('\n');
		goto done;
	} else if (!strcmp(argv[1], "-n")) {
	    if( (xpg = getenv("_XPG")) != NULL && atoi(xpg) > 0) {
	        fputs("-n ", stdout);
	    }
	    else{
	        fnewline = 0;
	    }
	    ++argv;
	    --argc;
	}
	for(i = 1; i <= argc; i++) {
		for(cp = argv[i]; *cp; cp++) {
			if(*cp == '\\')
			switch(*++cp) {
				case 'b':
					putchar('\b');
					continue;

				case 'c':
#ifdef sgi
					fnewline = 0;
					continue;
#else
					exit(0);
#endif

				case 'f':
					putchar('\f');
					continue;

				case 'n':
					putchar('\n');
					continue;

				case 'r':
					putchar('\r');
					continue;

				case 't':
					putchar('\t');
					continue;

				case 'v':
					putchar('\v');
					continue;

				case '\\':
					putchar('\\');
					continue;
				case '0':
					j = wd = 0;
					while ((*++cp >= '0' && *cp <= '7') && j++ < 3) {
						wd <<= 3;
						wd |= (*cp - '0');
					}
					putchar(wd);
					--cp;
					continue;

				default:
					cp--;
				}
			putchar(*cp);
		}
#ifdef sgi
		if (i == argc) {
			if (fnewline)
				putchar ('\n');
		} else {
			putchar(' ');
		}
#else
		putchar(((i == argc) && fnewline)? '\n': ' ');
#endif
	}
done:
	/* done so we can report errors over nfsv2, and writes to a file on
	 * a full filesystem. */
	if(fclose(stdout) == EOF)
		fprintf(stderr, "echo: %s\n", strerror(errno));
	exit(0);
}
