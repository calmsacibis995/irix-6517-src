/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)paste:paste.c	1.4.1.2"	*/
#ident	"$Revision: 1.8 $"
#
/* paste: concatenate corresponding lines of each file in parallel. Release 1.4 */
/*	(-s option: serial concatenation like old (127's) paste command */
# include <stdio.h>	/* make :  cc paste.c  */
# include <sys/euc.h>
# include <getwidth.h>
# include <locale.h>
# include <pfmt.h>
# include <string.h>
# include <errno.h>
# include <unistd.h>

# define MAXOPNF 12  	/* maximal no. of open files (not with -s option) */
# define MAXLINE BUFSIZ  	/* maximal line length */
#define RUB  '\177'
	char del[MAXLINE] = {"\t"};
  
eucwidth_t	wp;
  
static const char badopen[] = ":3:Cannot open %s: %s\n";
static int exitcode = 0;

main(argc, argv)
int argc;
char ** argv;
{
	int i, j, k, eofcount, nfiles, maxline, glue;
	int delcount = { 1 } ;
	int onefile  = { 0 } ;
	register int c ;
	char outbuf[MAXLINE], l, t;
	register char *p;
	FILE *inptr[MAXOPNF];

 	int delw;
	int mdelc;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:paste");

	getwidth(&wp);
	wp._eucw2++;
	wp._eucw3++;

	maxline = MAXLINE -2;
 
	while((c = getopt(argc, argv, ":sd:")) != EOF)
	  switch(c){
		case 's': 
		  onefile++;
		  break;
		case 'd': 
		  delcount = move(optarg, &del[0]);
		  break;
		case ':':
		  pfmt(stderr, MM_ERROR, ":51:no delimiters\n");
		  usage();		  
		case '?':
		  pfmt(stderr, MM_ERROR,
		       "uxlibc:1:Illegal option -- %c\n", c);
		  usage();
	  }/* end options */

	argc -= optind;
	argv = &argv[optind];
 
	if ( ! onefile) {	/* not -s option: parallel line merging */
		nfiles = 0;
		for (i = 0; argc >0 && i < MAXOPNF; i++) {
			if (!strcmp(argv[i], "-")) {
				inptr[nfiles] = stdin;
			} else inptr[nfiles] = fopen(argv[i], "r");
			if (inptr[nfiles] == NULL) {
				diag(badopen, argv[i], strerror(errno));
				exit(1);
			}
			else nfiles++;
			argc--;
		}
		if (argc > 0) diag(":52:Too many files - limit %d\n", MAXOPNF);
  
		do {
			p = &outbuf[0];
			eofcount = 0;
			j = k = 0;
			for (i = 0; i < nfiles; i++) {
				while((c = getc(inptr[i])) != '\n' && c != EOF)   {
					if (++j <= maxline) *p++ = c ;
					else {
					diag(":53:Line too long\n");
					}
				}
				if ( (l = del[k]) != RUB) *p++ = l;
				if (NOTASCII(l)) {
					if(ISSET2(l))
						delw = wp._eucw2;
					else if (ISSET3(l))
						delw = wp._eucw3;
					else
						delw = wp._eucw1;
					mdelc = delw;
					while (--mdelc)
						*p++ = del[++k];
				}
				k = (k + 1) % delcount;
				if( c == EOF) eofcount++;
			}
			if (l != RUB) {
				if (NOTASCII(l))
					p -= (--delw);
				*--p = '\n';
			} else
				*p = '\n';
			*++p = 0;
			if (eofcount < nfiles) fputs(outbuf, stdout);
		}while (eofcount < nfiles);
  
	} else {	/* -s option: serial file pasting (old 127 paste command) */
		k = 0;
		t = 0;
		for (i = 0; i < argc; i++) {
			p = &outbuf[0];
			glue = 0;
			j = 0;
			if (argv[i][0] == '-') {
				inptr[0] = stdin;
			} else inptr[0] = fopen(argv[i], "r");
			if (inptr[0] == NULL) {
				diag(badopen, argv[i], strerror(errno));
				continue;
			}
	  
			while((c = getc(inptr[0])) != EOF)   {
				if (j >= maxline) {
					t = *--p;
					*++p = 0;
					fputs(outbuf, stdout);
					p = &outbuf[0];
					j = 0;
				}
				if (glue) {
					glue = 0;
					l = del[k];
					if (l != RUB) {
						*p++ = l ;
						t = l ;
						j++;
						if (NOTASCII(l)) {
							if (ISSET2(l))
								delw = wp._eucw2;
							else if (ISSET3(l))
								delw = wp._eucw3;
							else
								delw = wp._eucw1;
							mdelc = delw;
							while (--mdelc) {
								*p++ = del[++k];
								j++;
							}
						}
					}
					k = (k + 1) % delcount;
				}
				if(c != '\n') {
					*p++ = c;
					t = c;
					j++;
				} else glue++;
			}
			if (t != '\n') {
				*p++ = '\n';
				j++;
			}
			if (j > 0) {
				*p = 0;
				fputs(outbuf, stdout);
			}
		}
	}
	exit(exitcode);
}

diag(s,a1, a2)
char *s,*a1, *a2;
{
	pfmt(stderr, MM_ERROR, s, a1, a2);
	exitcode = 1;
}
  
move(from, to)
char *from, *to;
{
int c, i;
	i = 0;
	do {
		c = *from++;
		i++;
		if (c != '\\') *to++ = c;
		else { c = *from++;
			switch (c) {
				case '0' : *to++ = RUB;
						break;
				case 't' : *to++ = '\t';
						break;
				case 'n' : *to++ = '\n';
						break;
				default  : *to++ = c;
						break;
			}
		}
	} while (c) ;
return(--i);
}


usage()
{
	pfmt(stderr, MM_ACTION,
		":54:Usage: paste [-s] [-d<delimiterstring>] file1 file2 ...\n");
	exit(1);
}
