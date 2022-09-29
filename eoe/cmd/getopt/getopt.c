/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.6 $"

#include	<stdio.h>
#include	<stdlib.h>
#include	<locale.h>
#include	<pfmt.h>

extern char *strcat();
extern char *strchr();
char *chkrm(char *os, int l);

main(argc, argv)
int argc;
char **argv;
{
	extern	int optind;
	extern	char *optarg;
	register int	c;
	int	errflg = 0;
	char	tmpstr[4];
	char	*outstr;
	char	*goarg;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore");
	(void)setlabel("UX:getopt");

	if(argc < 2) {
		pfmt(stderr, MM_ERROR, ":1:Incorrect usage\n");
		pfmt(stderr, MM_ACTION, ":306:Usage: getopt legal-args $*\n");
		exit(2);
	}

	outstr = malloc(1);
	goarg = argv[1];
	argv[1] = argv[0];
	argv++;
	argc--;
	outstr[0] = '\0';

	while((c=getopt(argc, argv, goarg)) != EOF) {
		if(c=='?') {
			errflg++;
			continue;
		}

		tmpstr[0] = '-';
		tmpstr[1] = c;
		tmpstr[2] = ' ';
		tmpstr[3] = '\0';

		outstr = chkrm(outstr, 4);
		strcat(outstr, tmpstr);

		if(*(strchr(goarg, c)+1) == ':') {
			outstr = chkrm(outstr, strlen(optarg) + 1 + 1);
			strcat(outstr, optarg);
			strcat(outstr, " ");
		}
	}

	if(errflg) {
		exit(2);
	}

	outstr = chkrm(outstr, 4);
	strcat(outstr, "-- ");
	while(optind < argc) {
		outstr = chkrm(outstr, strlen(argv[optind]) + 1 + 1);
		strcat(outstr, argv[optind++]);
		strcat(outstr, " ");
	}

	(void) printf("%s\n", outstr);
	exit(0);	/*NOTREACHED*/
}

char *
chkrm(os, l)
char *os;
int l;
{
	static nchars = 0, osize = 0;

	nchars += l;
	if (nchars >= osize) {
		os = realloc(os, 10000 + nchars);
		osize = 10000 + nchars;
	}
	return(os);
}
