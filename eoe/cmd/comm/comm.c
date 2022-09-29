/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)comm:comm.c	1.3.1.1"
/*
**	process common lines of two files
*/

#include	<stdio.h>
#include	<locale.h>
#include	<pfmt.h>
#include	<errno.h>
#include	<string.h>

#define	LB	4096 /* previously 256 */

int	one;
int	two;
int	three;

char	*ldr[3];

FILE	*ib1;
FILE	*ib2;
FILE	*openfil();

main(argc,argv)
char **argv;
{
	int	l = 1;
	char	lb1[LB],lb2[LB];

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:comm");

	ldr[0] = "";
	ldr[1] = "\t";
	ldr[2] = "\t\t";
	while (argc > 1)  {
		if(*argv[1] != '-' || argv[1][1] == 0)
			break;
		while(*++argv[1]) {
			switch(*argv[1]) {
			case '1':
				one = 1;
				break;

			case '2':
				two = 1;
				break;

			case '3':
				three = 1;
				break;

			case '-':
				argv++;
				argc--;
				goto Break;

			default:
				pfmt(stderr, MM_ERROR,
					"uxlibc:1:Illegal option -- %c\n",
					*argv[1]);
				usage(0);
			}
		}
		argv++;
		argc--;
	}
 Break:

	if(argc < 3)
		usage(1);
	if (one) {
		ldr[1][0] = '\0';
		ldr[2][l--] = '\0';
	}
	if (two)
		ldr[2][l] = '\0';
	ib1 = openfil(argv[1]);
	ib2 = openfil(argv[2]);
	if(rd(ib1,lb1) < 0) {
		if(rd(ib2,lb2) < 0)
			exit(0);
		copy(ib2,lb2,2);
	}
	if(rd(ib2,lb2) < 0)
		copy(ib1, lb1, 1);
	while(1) {
		switch(compare(lb1,lb2)) {
			case 0:
				wr(lb1,3);
				if(rd(ib1,lb1) < 0) {
					if(rd(ib2,lb2) < 0)
						exit(0);
					copy(ib2,lb2,2);
				}
				if(rd(ib2,lb2) < 0)
					copy(ib1, lb1, 1);
				continue;

			case 1:
				wr(lb1,1);
				if(rd(ib1,lb1) < 0)
					copy(ib2, lb2, 2);
				continue;

			case 2:
				wr(lb2,2);
				if(rd(ib2,lb2) < 0)
					copy(ib1, lb1, 1);
				continue;
		}
	}
}

rd(file,buf)
FILE *file;
char *buf;
{

	register int i, j;
	i = j = 0;
	while((j = getc(file)) != EOF) {
		*buf = j;
		if(*buf == '\n' || i > LB-2) {
			*buf = '\0';
			return(0);
		}
		i++;
		buf++;
	}
	return(-1);
}

wr(str,n)
char *str;
{
	switch(n) {
		case 1:
			if(one)
				return;
			break;

		case 2:
			if(two)
				return;
			break;

		case 3:
			if(three)
				return;
	}
	printf("%s%s\n",ldr[n-1],str);
}

copy(ibuf,lbuf,n)
FILE *ibuf;
char *lbuf;
{
	do {
		wr(lbuf,n);
	} while(rd(ibuf,lbuf) >= 0);

	exit(0);
}

compare(a,b)
char *a,*b;
{
	register char *ra,*rb;

	ra = --a;
	rb = --b;
	while(*++ra == *++rb)
		if(*ra == '\0')
			return(0);
	if(*ra < *rb)
		return(1);
	return(2);
}
FILE *openfil(s)
char *s;
{
	FILE *b;
	if(s[0]=='-' && s[1]==0)
		b = stdin;
	else if((b=fopen(s,"r")) == NULL) {
		pfmt(stderr, MM_ERROR, ":3:Cannot open %s: %s\n",
			s, strerror (errno));
		exit(2);
	}
	return(b);
}

usage(complain)
int complain;
{
	if (complain)
		pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
	pfmt(stderr, MM_ACTION, ":4:Usage: comm [ - [123] ] file1 file2\n");
	exit(2);
}
