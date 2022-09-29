/*
 * Copyright (c) 1980, 1987 Regents of the University of California.
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
 *
 * Internationalized by frank@ceres.esd.sgi.com
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)head.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <fmtmsg.h>
#include <stdarg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

/* locals */

char	cmd_label[] = "UX:head";

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */
int
main(argc, argv)
int argc;
char **argv;
{
	register int ch, cnt;
	int firsttime, linecnt = 10;
	int exit_val;
	extern char *optarg;
	extern int optind,opterr,optopt;
	int gotnum = 0;
	int c;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	opterr = 0;
	while ((c = getopt(argc, argv, "n:")) != EOF)
	{
		switch (c) {
			case 'n':
				if(isdigit(*optarg) && !gotnum)
				{
					if((linecnt = atoi(optarg)) < 0)
						goto usage;
					else
					{	++gotnum;
						break;
					}
				}
				else	goto usage;
				break;
			case '?':
				if(!isdigit(optopt))
				{
					_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
					gettxt(_SGI_DMMX_illoption, "illegal option -- %c"),
					optopt);
					goto usage;
				}
				if(gotnum)
					goto usage;
				if(optind == 2)
				{
					if((linecnt = atoi(&argv[optind-1][1])) < 0)
						goto usage;
					else
					{	++gotnum;
						break;
					}
				}
				break;
		}
	}
	if(optind > 1) 
	{
		--optind;
		argc -= optind;
		argv += optind;
	}
	exit_val = 0;
	for(firsttime = 1, --argc, ++argv;; firsttime = 0) {
	    if( !*argv) {
		if(!firsttime)
		    exit(exit_val);
	    } else {
		if( !freopen(*argv, "r", stdin)) {
		    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			gettxt(_SGI_DMMX_CannotRead,
			    "Cannot read from %s"),
			*argv);
		    exit_val = 1;
		    ++argv;
		    continue;
		}
		if(argc > 1) {
		    if( !firsttime)
			putchar('\n');
		    printf("==> %s <==\n", *argv);
		}
		++argv;
	    }
	    for(cnt = linecnt; cnt; --cnt) {
		while((ch = getchar()) != EOF)
		    if (putchar(ch) == '\n')
			break;
		if(ch == EOF)
			break;
	    }
	}
	/*NOTREACHED*/
usage:
	_sgi_nl_usage(SGINL_USAGE, cmd_label,
		gettxt(_SGI_DMMX_usage_head,
		"head [-number|-n number] [file ...]"));
	exit(1);
}
