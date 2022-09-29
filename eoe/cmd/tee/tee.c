/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tee:tee.c	1.6.2.2"
/*
 * tee-- pipe fitting
 */

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int openf[20] = { 1 };
int n = 1;
int errflg;
int aflag;

char in[BUFSIZ];

main(argc,argv)
char **argv;
{
	int register w;
	extern int optind;
	int c;
	struct stat64 buf;
	int exitcode = 0;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:tee");

	while ((c = getopt(argc, argv, "ai")) != EOF)
		switch(c) {
			case 'a':
				aflag++;
				break;
			case 'i':
				signal(SIGINT, SIG_IGN);
				break;
			case '?':
				errflg++;
		}
	if (errflg) {
		pfmt(stderr, MM_ACTION,
			":582:Usage: tee [ -i ] [ -a ] [file ] ...\n");
		exit(2);
	}
	argc -= optind;
	argv = &argv[optind];
	while(argc-->0) {
		if((openf[n++] = open(argv[0],O_WRONLY|O_CREAT|
			(aflag?O_APPEND:O_TRUNC), 0666)) == -1 ) {
			/* Why two different error messages?  I don't
			 * know, but I'm not going to change it. */
			if (errno == EACCES || errno == EISDIR) {
				pfmt(stderr, MM_ERROR, ":5:Cannot access %s: %s\n",
					argv[0], strerror(errno));
			} else {
				pfmt(stderr, MM_ERROR, ":4:Cannot open %s: %s\n",
					argv[0], strerror(errno));
			}
			n--;
			exitcode = 1;
		}
		argv++;
	}
	w = 0;
	for(;;) {
		w = read(0, in, BUFSIZ);
		if (w > 0)
			for(c=0;c<n;c++)
				write(openf[c], in, w);
		else
			break;
	}
	exit(exitcode);
}

