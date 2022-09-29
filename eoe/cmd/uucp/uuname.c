/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.6 $"

#include "uucp.h"
 
/*
 * returns a list of remote systems accessible to uucico
 * option:
 *	-l	-> returns only the local system name.
 *	-c	-> print remote systems accessible to cu
 */
void
main(argc,argv, envp)
int argc;
char **argv, **envp;
{
	int c, lflg = 0, cflg = 0;
	char s[BUFSIZ], prev[BUFSIZ], name[BUFSIZ];

	while ( (c = getopt(argc, argv, "lc")) != EOF )
		switch(c) {
		case 'c':
			cflg++;
			break;
		case 'l':
			lflg++;
			break;
		default:
			(void) fprintf(stderr, "usage: uuname [-l] [-c]\n");
			exit(1);
		}
 
	if (lflg) {
		if ( cflg )
			(void) fprintf(stderr,
			"uuname: -l overrides -c ... -c option ignored\n");
		uucpname(name);

		puts(name);
		exit(0);
	}
 
	if ( cflg )
		setservice("cu");
	else
		setservice("uucico");

	if ( sysaccess(EACCESS_SYSTEMS) != 0 ) {
		(void)fprintf(stderr,
			"uuname: cannot access Systems files\n");
		exit(1);
	}

	while ( getsysline(s, sizeof(s)) ) {
		if((s[0] == '#') || (s[0] == ' ') || (s[0] == '\t') || 
		    (s[0] == '\n'))
			continue;
		(void) sscanf(s, "%s", name);
		if (EQUALS(name, prev))
		    continue;
		puts(name);
		(void) strcpy(prev, name);
	}
	exit(0);
}

/* small, private copies of assert(), logent(), */
/* cleanup() so we can use routines in sysfiles.c */

/* ARGSUSED */
void assert(s1, s2, i1, file, line)
char *s1, *s2, *file; int i1, line;
{	
	(void)fprintf(stderr, "uuname: %s %s\n", s2, s1);
}

/* ARGSUSED */
void logent(char * text, char * status) {}

void cleanup(code) 	{ exit(code);	}
