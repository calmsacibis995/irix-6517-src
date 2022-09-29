/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cmp:cmp.c	1.4.1.8"

/***************************************************************************
 * Command: cmp
 * Inheritable Privileges: P_DACREAD,P_MACREAD
 *       Fixed Privileges: None
 * Notes: compares the difference of two files
 *
 ***************************************************************************/

/*
 *	compare two files
*/

#include	<stdio.h>
#include	<ctype.h>
#include	<locale.h>
#include	<pfmt.h>
#include	<errno.h>
#include	<string.h>

FILE	*file1,*file2;

char	*arg;

int	eflg;
int	lflg = 1;

long	line = 1;
long	chr = 0;
long long skip1 = 0;
long long skip2 = 0;

long long otoi(char *);
void usage(int);
void subset(char *name);
void barg(char *name);
void earg(char *name);

/*
 * Procedure:     main
 *
 * Restrictions:
                 setlocale: 	none
                 fopen: 	none
		 pfmt:		none
 */
main(argc, argv)
char **argv;
{
	register c1, c2;
	extern int optind;
	int opt;


	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:cmp");

	while ((opt = getopt(argc, argv, "ls")) != EOF){
		switch(opt){
		case 's':
			if (lflg != 1)
				usage(1);
			lflg--;
			continue;
		case 'l':
			if (lflg != 1)
				usage(1);
			lflg++;
			continue;
		default:
			usage(0);
		}
	}
	argc -= optind;
	if(argc < 2 || argc > 4)
		usage(1);
	argv += optind;


	if(strcmp(argv[0], "-") == 0)
		file1 = stdin;
	else if((file1 = fopen(argv[0], "r")) == NULL) {
		barg(argv[0]);
	}
	if(strcmp(argv[1], "-") == 0)
		file2 = stdin;
	else if((file2 = fopen(argv[1], "r")) == NULL) {
	        barg(argv[1]);
	}

	if (argc>2)
		skip1 = otoi(argv[2]);
	if (argc>3)
		skip2 = otoi(argv[3]);
	while (skip1) {
		if ((c1 = getc(file1)) == EOF) {
			arg = argv[1];
			earg(argv[1]);
		}
		skip1--;
	}
	while (skip2) {
		if ((c2 = getc(file2)) == EOF) {
			arg = argv[2];
			earg(argv[2]);
		}
		skip2--;
	}
	while(1) {
		chr++;
		c1 = getc(file1);
		c2 = getc(file2);
		if(c1 == c2) {
			if (c1 == '\n')
				line++;
			if(c1 == EOF) {
				if(eflg)
					exit(1);
				exit(0);
			}
			continue;
		}
		if(lflg == 0)
			exit(1);
		if(c1 == EOF)
			subset(argv[0]);
		if(c2 == EOF)
			subset(argv[1]);
		if(lflg == 1) {
			pfmt(stdout, MM_NOSTD,
				":28:%s %s differ: char %ld, line %ld\n",
				argv[0], argv[1], chr, line);
			exit(1);
		}
		eflg = 1;
		printf("%6ld %3o %3o\n", chr, c1, c2);
	}
}

/*
 * Procedure:     otoi
 *
 * Restrictions:
                 isdigit: 	none
*/
long long
otoi(char *s)
{
	long long v;
	int base;

	v = 0;
	base = 10;
	if (*s == '0')
		base = 8;
	while(isdigit(*s))
		v = v*base + *s++ - '0';
	return(v);
}

/*
 * Procedure:     usage
 *
 * Restrictions:
                 pfmt: 	none
*/
void 
usage(int complain)
{
	if (complain)
		pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
	pfmt(stderr, MM_ACTION, ":892:Usage: cmp [-l] [-s] filename1 filename2 [skip1] [skip2]\n");
	exit(2);
}

/*
 * Procedure:     barg
 *
 * Restrictions:
                 pfmt:		none
                 strerror: 	none
*/
void
subset(char *name)
{
    fprintf(stderr, "cmp:  EOF  on  %s\n", name);
    exit(1);
}


/*
 * Procedure:     barg
 *
 * Restrictions:
                 pfmt:		none
                 strerror: 	none
*/
void 
barg(char *name)
{
	if (lflg)
		pfmt(stderr, MM_ERROR, ":4:Cannot open %s: %s\n", name,
			strerror(errno));
	exit(2);
}

/*
 * Procedure:     earg
 *
 * Restrictions:
                 pfmt: 	none
*/
void
earg(char *name)
{
	(void)setlabel("cmp");
	pfmt(stderr, MM_INFO, ":30: EOF  on  %s.\n", name);
	exit(1);
}
