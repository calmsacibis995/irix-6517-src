/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)join:join.c	1.5.1.2"
/*	join F1 F2 on stuff */

#include	<stdio.h>
#include	<locale.h>
#include	<sys/euc.h>
#include	<getwidth.h>
#include	<pfmt.h>
#include	<errno.h>
#include	<string.h>

#define F1 0
#define F2 1
#define	NFLD	500	/* max field per line */
#define comp() cmp(ppi[F1][j1],ppi[F2][j2])
#define putfield(string) if(*string == NULL) (void) fputs(null, stdout); \
			else (void) fputs(string, stdout)

FILE *f[2];
char buf[2][BUFSIZ];	/*input lines */
char *ppi[2][NFLD];	/* pointers to fields in lines */
int	j1	= 1;	/* join of this field of file 1 */
int	j2	= 1;	/* join of this field of file 2 */
int	olist[2*NFLD];	/* output these fields */
int	olistf[2*NFLD];	/* from these files */
int	no;	/* number of entries in olist */
int	sep1	= ' ';	/* default field separator */
int	sep2	= '\t';
char null[BUFSIZ];
int	aflg;
int     vflg;
char	*msep1	= " ";
char	*msep2	= "\t";
int	sepwidth = 1;
eucwidth_t	wp;

static const char badopen[] = ":3:Cannot open %s: %s\n";

main(argc, argv)
char *argv[];
{
	int i;
	int n1, n2;
	long long top2, bot2;
	long long ftell64();
	int c;
	extern char *optarg;
	extern int optind;
	char *comma_p;
	int field_buf;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:join");

	getwidth(&wp);
	wp._eucw2++;
	wp._eucw3++;

	while ((c = getopt(argc, argv, "a:v:e:o:t:1:2:j:")) != EOF) {
		switch (c) {
		case 'a': 
			switch(optarg[0]) {
			case '1':
				aflg |= 1;
				break;
			case '2':
				aflg |= 2;
				break;
			default:
				aflg |= 3;
			}
			break;
		case 'v': 
			/* will make use of aflg here */
			vflg = 1;
			switch(optarg[0]) {
			case '1':
				aflg |= 1;
				break;
			case '2':
				aflg |= 2;
				break;
			default:
				aflg |= 3;
			}
			break;
		case 'e':
			strcpy(null, optarg);
			break;
		case 't':
			msep1 = msep2 = optarg;
			if (NOTASCII(*msep1)) {
				if (ISSET2(*msep1))
					sepwidth = wp._eucw2;
				else
				if (ISSET3(*msep1))
					sepwidth = wp._eucw3;
				else
					sepwidth = wp._eucw1;
			}
			break;
		case 'o':
			/* 
			 * For the compatability of non-xpg4 features,
			 * we have read in all the arguments separated 
			 * by spaces for this option.
			 */

			for (no = 0; no < 2*NFLD && optarg; no++) {
				if (optarg[0] == '1' && 
				    optarg[1] == '.') {
					optarg += 2;
					if ((comma_p = strchr(optarg, ','))) {
						*comma_p = '\0';
						olist[no] = atoi(optarg);
						olistf[no] = F1;
						optarg = comma_p + 1;
					}
					else if ((comma_p = 
						  strchr(optarg, ' '))) {
						*comma_p = '\0';
						olist[no] = atoi(optarg);
						olistf[no] = F1;
						optarg = comma_p + 1;
					}
					else if (isdigit(argv[optind][0]) &&
						 argv[optind][1] == '.') {
						olist[no] = atoi(optarg);
						olistf[no] = F1;
						optarg = argv[optind++];
					}
					else {
						olist[no] = atoi(optarg);
						olistf[no] = F1;
						optarg = NULL;
					}
					/* Check for invalid specification */
					if (olist[no] <1) goto usage;
				}
				else if (optarg[0] == '2' && 
				    optarg[1] == '.') {
					optarg += 2;
					if ((comma_p = strchr(optarg, ','))) {
						*comma_p = '\0';
						olist[no] = atoi(optarg);
						olistf[no] = F2;
						optarg = comma_p + 1;
					}
					else if ((comma_p = 
						  strchr(optarg, ' '))) {
						*comma_p = '\0';
						olist[no] = atoi(optarg);
						olistf[no] = F2;
						optarg = comma_p + 1;
					}
					else if (isdigit(argv[optind][0]) &&
						 argv[optind][1] == '.') {
						olist[no] = atoi(optarg);
						olistf[no] = F2;
						optarg = argv[optind++];
					}
					else {
						olist[no] = atoi(optarg);
						olistf[no] = F2;
						optarg = NULL;
					}
					/* Invalid specification */
					if (olist[no] <1) goto usage;
				}
				else {
					break;
				}
			}
			if ( no == 0 )
				goto usage;
			break;
		case '1':
			j1 = atoi(optarg);
			break;
		case '2':
			j2 = atoi(optarg);
			break;
		case 'j':
			if (strcmp(argv[optind - 2], "-j") == 0) {
				/* -j case */
				field_buf = atoi(optarg);
				if (field_buf > 2) {
					j2 = j1 = field_buf;
				}
				else {
					j1 = field_buf;
				}
			}
			else if (strcmp(argv[optind - 1], "-j1") == 0) {
				/* -j1 case */
				j1 = atoi(argv[optind++]);
			}
			else if (strcmp(argv[optind - 1], "-j2") == 0) {
				/* -j2 case */
				j2 = atoi(argv[optind++]);
			}
			else {
				goto usage;
			}
			break;
		}
	}

	for (i = 0; i < no; i++)
		olist[i]--;	/* 0 origin */

	/* final check on command line input */
	if ((argc - optind) != 2){ 
usage:
		pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		pfmt(stderr, MM_ACTION, ":21:Usage: join [-an] [-e s] [-jn m] [-tc] [-o list] file1 file2\n");
		exit(1);
	}

	j1--;
	j2--;	/* everyone else believes in 0 origin */

	/* file1 */
	if (argv[optind][0] == '-') {
		f[F1] = stdin;
		optind++;
	}
	else if ((f[F1] = fopen(argv[optind], "r")) == NULL) {
		error(badopen, argv[optind], strerror(errno));
		
	}
	else {
		optind++;
	}

	/* file2 */
	if (argv[optind][0] == '-') {
		f[F2] = stdin;
	}
	else if ((f[F2] = fopen(argv[optind], "r")) == NULL) {
		error(badopen, argv[optind], strerror(errno));
	}

#define get1() n1=input(F1)
#define get2() n2=input(F2)
	get1();
	bot2 = ftell64(f[F2]);
	get2();
	while(n1>0 && n2>0 || aflg!=0 && n1+n2>0) {
		if(n1>0 && n2>0 && comp()>0 || n1==0) {
			if(aflg&2) output(0, n2);
			bot2 = ftell64(f[F2]);
			get2();
		} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
			if(aflg&1) output(n1, 0);
			get1();
		} else {
			/*(n1>0 && n2>0 && comp()==0 */ 
			while(n2>0 && comp()==0) {
				if (!vflg) {
					output(n1, n2);
				}
				top2 = ftell64(f[F2]);
				get2();
			}
			(void) fseek64(f[F2], bot2, 0);
			get2();
			get1();
			if (n1 && n1 <= j1 || n2 && n2 <= j2)
				/* ignore lines whose # fields < -j number */
				continue;
			for(;;) {
				if(n1>0 && n2>0 && comp()==0) {
					if (!vflg) {
						output(n1, n2);
					}
					get2();
				} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
					(void) fseek64(f[F2], bot2, 0);
					get2();
					get1();
				} else /*(n1>0 && n2>0 && comp()>0 || n1==0)*/{
					(void) fseek64(f[F2], top2, 0);
					bot2 = top2;
					get2();
					break;
				}
			}
		}
	}
	return(0);
}

input(n)		/* get input line and split into fields */
{
	register int i, c;
	char *bp;
	char **pp;

	register int sepc;
	register int mltwidth = 1;
	int sepflag = 1;

	bp = buf[n];
	pp = ppi[n];
	if (fgets(bp, BUFSIZ, f[n]) == NULL)
		return(0);
	i = 0;
	do {
		i++;
		if (*msep1 == ' ')
			while ((c = *bp) == *msep1 || c == *msep2)
				bp++;	/* skip blanks */
		else
			c = *bp;
		*pp++ = bp;	/* record beginning */
			/* fails badly if string doesn't have \n at end */
		while ((c = *bp) != '\n' && c != '\0') {
			if ((c == *msep1) || (c == *msep2)) {
				if (c == *msep1) {
					for (sepc = 0; sepc < sepwidth ; sepc++) {
						if (*bp != msep1[sepc]) {
							sepflag = 0;
							break;
						}
						++bp;
					}
				}
				else if (c == *msep2) {
					break;
				}
				if (sepflag) {
					bp -= sepwidth;
					break;
				} else {
					sepflag = 1;
					bp -= sepc;
				}
			}
			if (NOTASCII(c)) {
				if (ISSET2(c))
					mltwidth = wp._eucw2;
				else
				if (ISSET3(c))
					mltwidth = wp._eucw3;
				else
					mltwidth = wp._eucw1;
			} else
				mltwidth = 1;
			bp += mltwidth;
		}
		*bp++ = '\0';
		bp += (sepwidth-1);
	} while (c != '\n' && c != '\0');

	*pp = 0;
	return(i);
}

output(on1, on2)	/* print items from olist */
int on1, on2;
{
	int i;
	int sepc;

	if (no <= 0) {	/* default case */
		if (on1)
			putfield(ppi[F1][j1]);
		else
			putfield(ppi[F2][j2]);
		for (i = 0; i < on1; i++)
			if (i != j1) {
				for (sepc = 0; sepc < sepwidth ; sepc++)
					putchar(msep1[sepc]);
				putfield(ppi[F1][i]);
			}
		for (i = 0; i < on2; i++)
			if (i != j2) {
				for (sepc = 0; sepc < sepwidth ; sepc++)
					putchar(msep1[sepc]);
				putfield(ppi[F2][i]);
			}
		(void) putchar('\n');
	} else {
		for (i = 0; i < no; i++) {
			if(olistf[i]==F1 && on1<=olist[i] ||
			   olistf[i]==F2 && on2<=olist[i])
				(void) fputs(null, stdout);
			else
				putfield(ppi[olistf[i]][olist[i]]);
			if (i < no - 1)
				for (sepc = 0; sepc < sepwidth ; sepc++)
					putchar(msep1[sepc]);
			else
				(void) putchar('\n');
		}
	}
}

/*VARARGS*/
error(s1, s2, s3, s4, s5)
char *s1;
{
	(void) pfmt(stderr, MM_ERROR, s1, s2, s3, s4, s5);
	exit(1);
}

cmp(s1, s2)
char *s1, *s2;
{
	if (s1 == NULL) {
		if (s2 == NULL)
			return(0);
		else
			return(-1);
	} else if (s2 == NULL)
		return(1);
	return(strcmp(s1, s2));
}
