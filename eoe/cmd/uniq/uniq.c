/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uniq:uniq.c	1.4.1.2"
/*
 * Deal with duplicated lines in a file
 */

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <locale.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <string.h>
#include <errno.h>
#include <sys/euc.h>
#include <getwidth.h>

int	fields = 0;
int	letters = 0;
int	linec;
char	mode = 0;
int	uniq;
char	*skip();

eucwidth_t	wp;

main(argc, argv)
int argc;
char *argv[];
{
	static char b1[2048], b2[2048];
	FILE *temp = NULL;
	int keep_looping = 1;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel("UX:uniq");

	getwidth(&wp);
	wp._eucw2++;
	wp._eucw3++;

	while(argc > 1 && keep_looping) {
		if (*argv[1] == '-') {
			switch(argv[1][1]) {
			    case 'c':
			    case 'd':
			    case 'u':
				    mode = argv[1][1];
				    break;
			    case 'f':
				    if (argv[2] && isdigit(argv[2][0])) {
					    fields = atoi(argv[2]);
				    } else {
					    _sgi_nl_error(SGINL_NOSYSERR,
							  "UX:uniq",
							  gettxt(_SGI_MMX_uniq_missing_f_arg, "Missing argument to -f option"));
					    _sgi_nl_usage(SGINL_USAGE,
							  "UX:uniq",
							  gettxt(_SGI_MMX_uniq_usage, "uniq [-c|-d|-u] [-f fields] [-s char] [input_file [output_file]]"));
					    exit(1);
				    }
				    argc--;
				    argv++;
				    break;
			    case 's':
				    if (argv[2] && isdigit(argv[2][0])) {
					    letters = atoi(argv[2]);
				    } else {
					    _sgi_nl_error(SGINL_NOSYSERR,
							  "UX:uniq",
							  gettxt(_SGI_MMX_uniq_missing_s_arg, "Missing argument to -s option"));
					    _sgi_nl_usage(SGINL_USAGE,
							  "UX:uniq",
							  gettxt(_SGI_MMX_uniq_usage, "uniq [-c|-d|-u] [-f fields] [-s char] [input_file [output_file]]"));
					    exit(1);
				    }
				    argc--;
				    argv++;
				    break;
			    case '-':
				    keep_looping = 0;
				    break;
			    case 0:	/* '-' alone */
				    temp = stdin;
				    break;
			    default:
				    if (isdigit(argv[1][1])) {
					    fields = atoi(&argv[1][1]);
				    } else {
					    _sgi_nl_error(SGINL_NOSYSERR,
							  "UX:uniq",
							  gettxt(_SGI_MMX_illoption, "Illegal option -- %c"),
							  argv[1][1]);
					    _sgi_nl_usage(SGINL_USAGE,
							  "UX:uniq",
							  gettxt(_SGI_MMX_uniq_usage, "uniq [-c|-d|-u] [-f fields] [-s char] [input_file [output_file]]"));
					    exit(1);
				    }
			}
			argc--;
			argv++;
		} else if (*argv[1] == '+') {
			letters = atoi(&argv[1][1]);
			argc--;
			argv++;
		} else {
			break;
		}
	}

	if (!temp && argc > 1) {
		if ((temp = fopen(argv[1], "r")) == NULL) {
			_sgi_nl_error(SGINL_SYSERR2, "UX:uniq",
				      gettxt(_SGI_MMX_CannotOpen,
					     "Cannot open %s"),
				      argv[1]);
			exit(1);
		} else {
			fclose(temp);
			freopen(argv[1], "r", stdin);
		}
		argc--;
		argv++;
	}

	if (argc > 1 && freopen(argv[1], "w", stdout) == NULL) {
		_sgi_nl_error(SGINL_SYSERR2, "UX:uniq",
			      gettxt(_SGI_MMX_CannotCreat,
				     "Cannot create %s"),
			      argv[1]);
		exit(1);
	}

	if(gline(b1))
		exit(0);
	for(;;) {
		linec++;
		if(gline(b2)) {
			pline(b1);
			exit(0);
		}
		if(!equal(b1, b2)) {
			pline(b1);
			linec = 0;
			do {
				linec++;
				if(gline(b1)) {
					pline(b2);
					exit(0);
				}
			} while(equal(b1, b2));
			pline(b2);
			linec = 0;
		}
	}
}

gline(buf)
register char buf[];
{
	register c;

	while((c = getchar()) != '\n') {
		if(c == EOF)
			return(1);
		*buf++ = c;
	}
	*buf = 0;
	return(0);
}

pline(buf)
register char buf[];
{

	switch(mode) {

	case 'u':
		if(uniq) {
			uniq = 0;
			return;
		}
		break;

	case 'd':
		if(uniq) break;
		return;

	case 'c':
		printf("%4d ", linec);
	}
	uniq = 0;
	fputs(buf, stdout);
	putchar('\n');
}

equal(b1, b2)
register char b1[], b2[];
{
	register char c;

	b1 = skip(b1);
	b2 = skip(b2);
	while((c = *b1++) != 0)
		if(c != *b2++) return(0);
	if(*b2 != 0)
		return(0);
	uniq++;
	return(1);
}

char *
skip(s)
register char *s;
{
	register nf, nl;
	register int i, j;

	nf = nl = 0;
	while(nf++ < fields) {
		while(*s == ' ' || *s == '\t')
			s++;
		while( !(*s == ' ' || *s == '\t' || *s == 0) ) 
			s++;
	}
	while(nl++ < letters && *s != 0) 
	{
		if (ISASCII(*s))
			s++;
		else {
			if (ISSET2(*s)) {
				i = wp._eucw2;
				j = wp._scrw2;
			} else if (ISSET3(*s)) {
				i = wp._eucw3;
				j = wp._scrw3;
			} else {
				i = wp._eucw1;
				j = wp._scrw1;
			}

			nl += (--j);

			if (nl <= letters)
				s += i;
		}
	}
	return(s);
}
