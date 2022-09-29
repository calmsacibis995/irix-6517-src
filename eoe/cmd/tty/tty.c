/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)tty:tty.c	1.3"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/tty/RCS/tty.c,v 1.4 1992/04/24 10:26:02 wtk Exp $"

/*
** Type tty name
*/

#include	<stdio.h>
#include	<sys/stermio.h>
#include	<locale.h>
#include	<pfmt.h>

char	*ttyname();

extern int	optind;
int		lflg;
int		sflg;

main(argc, argv)
char **argv;
{
	register char *p;
	register int	i;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxue.abi");
	(void)setlabel("UX:tty");

	while((i = getopt(argc, argv, "ls")) != EOF)
		switch(i) {
		case 'l':
			lflg = 1;
			break;
		case 's':
			sflg = 1;
			break;
		case '?':
			pfmt(stderr, MM_ACTION, ":1:Usage: tty [-l] [-s]\n");
			exit(2);
		}
	p = ttyname(0);
	if(!sflg){
		if (p)
			puts(p);
		else
			pfmt(stdout, MM_NOSTD, ":2:not a tty\n");
	}
	if(lflg) {
		if((i = ioctl(0, STWLINE, 0)) == -1)
			pfmt(stdout, MM_NOSTD, 
				":3:not on an active synchronous line\n");
		else
			pfmt(stdout, MM_NOSTD, ":4:synchronous line %d\n", i);
	}
	exit(p? 0: 1);
}
