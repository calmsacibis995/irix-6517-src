/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sed:sed0.c	1.10"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sed/RCS/sed0.c,v 1.14 1998/10/28 20:44:23 danc Exp $"



#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <limits.h>
#include "sed.h"
FILE	*fin;
FILE    *fcode[12];
struct savestr_s lastre;
struct savestr_s address_string;
char    sseof;
char	*recur;
char    *reend;
char    *hend;
union reptr     *ptrend;
int     eflag;
char    linebuf[LBSIZE+1];
int     gflag;
int     nlno;
#define	LOCMAXNFILES	12
char    fname[LOCMAXNFILES][NAME_MAX+1];
int     nfiles;
union reptr ptrspace[PTRSIZE];
union reptr *rep;
char    *cp;
char    respace[RESIZE];
struct label ltab[LABSIZE];
struct label    *lab;
struct label    *labend;
int     depth;
int     eargc;
char    **eargv;
union reptr     **cmpend[DEPTH];
char    *badp;
char    bad;
char    compfl;
int	xpg4command = 0;

#define CCEOF	22

struct label    *labtab = ltab;

char    CGMES[]		= "sed: command garbled: %s\n";
char    TMMES[]		= "Too much text: %s\n";
char    LTL[]  		= "Label too long: %s\n";
char    AD0MES[]	= "No addresses allowed: %s\n";
char    AD1MES[]	= "Only one address allowed: %s\n";
char	TOOBIG[]	= "Suffix too large - 512 max: %s\n";
char	*bracket = NULL;

main(argc, argv)
char    *argv[];
{
	int	c;
	extern char *optarg;
	extern int optind;
	extern int opterr;
	extern int optopt;
	int	error = 0;

	initsavestr(&lastre);
	initsavestr(&address_string);

	badp = &bad;
	aptr = abuf;
	lab = labtab + 1;       /* 0 reserved for end-pointer */
	rep = ptrspace;
	rep->r1.ad1lno = LNO_NONE;
	rep->r1.ad2lno = LNO_NONE;
	rep->r1.re1lno = LNO_NONE;
	recur = respace;
	lbend = &linebuf[LBSIZE];
	hend = &holdsp[LBSIZE];
	lcomend = &genbuf[71];
	ptrend = &ptrspace[PTRSIZE];
	reend = &respace[RESIZE-1];
	labend = &labtab[LABSIZE];
	lnum = 0;
	pending = 0;
	depth = 0;
	spend = linebuf;
	hspend = holdsp;	/* Avoid "bus error" under "H" cmd. */
	fcode[0] = stdout;
	nfiles = 1;

	if(argc == 1)
		exit(0);

	{
		char	*ep;

		ep = getenv("_XPG");
		if (ep != NULL)
			xpg4command = (atoi(ep) > 0);
	}

	opterr = 0;
	while ((c = getopt(argc,argv,"nf:e:g")) != EOF)
		switch (c) {

		case 'n':
			nflag++;
			continue;

		case 'f':
			if((fin = fopen(optarg, "r")) == NULL) {
				fprintf(stderr, "Cannot open pattern-file: %s\n", optarg);
				exit(2);
			}

			fcomp();
			fclose(fin);
			continue;

		case 'e':
			eflag++;
			eargc = 1;
			eargv = (&optarg) - 1;
			fcomp();
			eflag = 0;
			continue;

		case 'g':
			gflag++;
			continue;

		case '?':
		default:
			switch (optopt) {
			case 'f':
				fprintf( stderr, "-f requires a script file argument\n" );
				break;

			case 'g':
				fprintf( stderr, "-e requires a script argument\n" );
				break;
				
			default:
				fprintf(stderr, "Unknown flag: %c\n", optopt);
				break;
			}
			exit(2);
		}


	if (compfl == 0) {
		/*
		 * if no -e or -f option, parse first non-option argument
		 * as if it were preceded by -e.
		 */
		eargc = argc - optind;
		eargv = argv + optind - 1;
		eflag++;
		fcomp();
		optind++;
		eflag = 0;
	}

	if(depth) {
		fprintf(stderr, "Too many {'s");
		exit(2);
	}

	labtab->address = rep;

	dechain();

	if (optind >= argc) {
		eargc = 0;
		if (execute((char *)NULL))
			error = 1;
	} else {
		for (; optind < argc; optind++) {
			eargc = (argc - optind) - 1;
			if (execute(argv[optind]))
				error = 1;
		}
	}
	fclose(stdout);
	exit(error);
	/* NOTREACHED */
}

fcomp()
{

	register char   *rp, *tp;
	struct savestr_s op;
	char *sp;
	union reptr     *pt, *pt1;
	int     i, ii;
	struct label    *lpt;
	int	address_result;
	

	compfl = 1;
	initsavestr(&op);
	savestr(&op,lastre.s);

	if(rline(linebuf) < 0)  return;
	if(*linebuf == '#') {
		if(linebuf[1] == 'n')
			nflag = 1;
	}
	else {
		cp = linebuf;
		goto comploop;
	}


	for(;;) {
		if(rline(linebuf) < 0)  break;

		cp = linebuf;

comploop:
		while(*cp == ' ' || *cp == '\t')	cp++;
		if(*cp == '\0' || *cp == '#')	 continue;
		if(*cp == ';') {
			cp++;
			goto comploop;
		}

		sp = cp;
		if (address(&rep->r1.ad1,&rep->r1.ad1lno,NULL)) {
			fprintf(stderr, CGMES, linebuf);
			exit(2);
		}
		if (rep->r1.ad1lno == LNO_EMPTY) {
			if (op.length == 0) {
				fprintf(stderr, "First RE may not be null\n");
				exit(2);
			}
			(void) address(&rep->r1.ad1,&rep->r1.ad1lno,op.s);
		} else if (rep->r1.ad1lno == LNO_RE)
			savestr(&op,address_string.s);

		if(*cp == ',' || *cp == ';') {
			cp++;
			sp = cp;
			if (address(&rep->r1.ad2,&rep->r1.ad2lno,NULL)) {
				fprintf(stderr, CGMES, linebuf);
				exit(2);
			}
			if (rep->r1.ad2lno == LNO_EMPTY) {
				if (op.length == 0) {
					fprintf(stderr, "First RE may not be null\n");
					exit(2);
				}
				(void) address(&rep->r1.ad2,&rep->r1.ad2lno,op.s);
			} else if (rep->r1.ad2lno == LNO_RE)
				savestr(&op,address_string.s);
		} else
			rep->r1.ad2lno = LNO_NONE;

		while(*cp == ' ' || *cp == '\t')	cp++;

swit:
		switch(*cp++) {

			default:
				fprintf(stderr, "Unrecognized command: %s\n", linebuf);
				exit(2);

			case '!':
				rep->r1.negfl = 1;
				goto swit;

			case '{':
				rep->r1.command = BCOM;
				rep->r1.negfl = !(rep->r1.negfl);
				cmpend[depth++] = &rep->r2.lb1;
				rep->r2.lb1 = NULL;
				if(++rep >= ptrend) {
					fprintf(stderr, "Too many commands: %s\n", linebuf);
					exit(2);
				}
				if(*cp == '\0') continue;

				goto comploop;

			case '}':
				if(rep->r1.ad1lno != LNO_NONE) {
					fprintf(stderr, AD0MES, linebuf);
					exit(2);
				}

				if(--depth < 0) {	/* { */
					fprintf(stderr, "Too many }'s\n");
					exit(2);
				}
				*cmpend[depth] = rep;

				rep->r3.p = recur; /* XXX */
				continue;

			case '=':
				rep->r1.command = EQCOM;
				if(rep->r1.ad2lno != LNO_NONE) {
					fprintf(stderr, AD1MES, linebuf);
					exit(2);
				}
				break;

			case ':':
				if(rep->r1.ad1lno != LNO_NONE) {
					fprintf(stderr, AD0MES, linebuf);
					exit(2);
				}

				while(*cp++ == ' ');
				cp--;


				tp = lab->asc;
				while((*tp++ = *cp++))
					if(tp > &(lab->asc[MAXLABLEN])) {
						fprintf(stderr, LTL, linebuf);
						exit(2);
					}

				if(lpt = search(lab)) {
					if(lpt->address) {
						fprintf(stderr, "Duplicate labels: %s\n", linebuf);
						exit(2);
					}
				} else {
					lab->chain = 0;
					lpt = lab;
					if(++lab >= labend) {
						fprintf(stderr, "Too many labels: %s\n", linebuf);
						exit(2);
					}
				}
				lpt->address = rep;
				rep->r3.p = recur; /* XXX */

				continue;

			case 'a':
				rep->r1.command = ACOM;
				if(rep->r1.ad2lno != LNO_NONE) {
					fprintf(stderr, AD1MES, linebuf);
					exit(2);
				}
				if(*cp == '\\') cp++;
				if(*cp++ != '\n') {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				rep->r3.p = recur;
				recur = text(rep->r3.p, &respace[RESIZE]);
				break;
			case 'c':
				rep->r1.command = CCOM;
				if(*cp == '\\') cp++;
				if(*cp++ != ('\n')) {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				rep->r3.p = recur;
				recur = text(rep->r3.p, &respace[RESIZE]);
				break;
			case 'i':
				rep->r1.command = ICOM;
				if(rep->r1.ad2lno != LNO_NONE) {
					fprintf(stderr, AD1MES, linebuf);
					exit(2);
				}
				if(*cp == '\\') cp++;
				if(*cp++ != ('\n')) {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				rep->r3.p = recur;
				recur = text(rep->r3.p, &respace[RESIZE]);
				break;

			case 'g':
				rep->r1.command = GCOM;
				break;

			case 'G':
				rep->r1.command = CGCOM;
				break;

			case 'h':
				rep->r1.command = HCOM;
				break;

			case 'H':
				rep->r1.command = CHCOM;
				break;

			case 't':
				rep->r1.command = TCOM;
				goto jtcommon;

			case 'b':
				rep->r1.command = BCOM;
jtcommon:
				rep->r2.lb1 = NULL;
				while(*cp++ == ' ');
				cp--;

				if(*cp == '\0') {
					if(pt = labtab->chain) {
						while(pt1 = pt->r2.lb1)
							pt = pt1;
						pt->r2.lb1 = rep;
					} else
						labtab->chain = rep;
					break;
				}
				tp = lab->asc;
				while((*tp++ = *cp++))
					if(tp > &(lab->asc[MAXLABLEN])) {
						fprintf(stderr, LTL, linebuf);
						exit(2);
					}
				cp--;

				if(lpt = search(lab)) {
					if(lpt->address) {
						rep->r2.lb1 = lpt->address;
					} else {
						pt = lpt->chain;
						while(pt1 = pt->r2.lb1)
							pt = pt1;
						pt->r2.lb1 = rep;
					}
				} else {
					lab->chain = rep;
					lab->address = 0;
					if(++lab >= labend) {
						fprintf(stderr, "Too many labels: %s\n", linebuf);
						exit(2);
					}
				}
				break;

			case 'n':
				rep->r1.command = NCOM;
				break;

			case 'N':
				rep->r1.command = CNCOM;
				break;

			case 'p':
				rep->r1.command = PCOM;
				break;

			case 'P':
				rep->r1.command = CPCOM;
				break;

			case 'r':
				rep->r1.command = RCOM;
				if(rep->r1.ad2lno != LNO_NONE) {
					fprintf(stderr, AD1MES, linebuf);
					exit(2);
				}
				if(*cp++ != ' ') {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				rep->r3.p = recur;
				recur = text(rep->r3.p, &respace[RESIZE]);
				break;

			case 'd':
				rep->r1.command = DCOM;
				break;

			case 'D':
				rep->r1.command = CDCOM;
				rep->r2.lb1 = ptrspace;
				break;

			case 'q':
				rep->r1.command = QCOM;
				if(rep->r1.ad2lno != LNO_NONE) {
					fprintf(stderr, AD1MES, linebuf);
					exit(2);
				}
				break;

			case 'l':
				rep->r1.command = LCOM;
				break;

			case 's':
				rep->r1.command = SCOM;
				if (scansavestr(&address_string,&cp,0)) {
					fprintf(stderr,CGMES,linebuf);
					exit(2);
				}
				if (address_string.length == 0) {
					if(op.length == 0) {
						fprintf(stderr,
							"First RE may not be null\n");
						exit(2);
					}
					savestr(&address_string,
						op.s);
				} else {
					savestr(&op,
						address_string.s);
				}
				rep->r1.re1lno = LNO_RE;
				if (regcomp(&rep->r1.re1,
					    address_string.s, 0)) {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				rep->r1.rhs = recur;
				if(compsub(&rep->r1.re1,
					   rep->r1.rhs,&recur)) {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}

				if(*cp == 'g') {
					cp++;
					rep->r1.gfl = 999;
				} else if(gflag)
					rep->r1.gfl = 999;

				if(*cp >= '1' && *cp <= '9')
					{i = *cp - '0';
					cp++;
					while(1)
						{ii = *cp;
						if(ii < '0' || ii > '9') break;
						i = i*10 + ii - '0';
						if(i > 512)
							{fprintf(stderr, TOOBIG, linebuf);
							exit(2);
							}
						cp++;
						}
					rep->r1.gfl = i;
					}

				if(*cp == 'p') {
					cp++;
					rep->r1.pfl = 1;
				}

				if(*cp == 'P') {
					cp++;
					rep->r1.pfl = 2;
				}

				if(*cp == 'w') {
					cp++;
					if(*cp++ !=  ' ') {
						fprintf(stderr, CGMES, linebuf);
						exit(2);
					}
					if(nfiles >= 10) {
						fprintf(stderr, "Too many files in w commands\n");
						exit(2);
					}

					text(fname[nfiles], 
						&fname[LOCMAXNFILES][0]);
					for(i = nfiles - 1; i >= 0; i--)
						if(cmp(fname[nfiles],fname[i]) == 0) {
							rep->r1.fcode = fcode[i];
							goto done;
						}
					if((rep->r1.fcode = fopen(fname[nfiles], "w")) == NULL) {
						fprintf(stderr, "cannot open %s\n", fname[nfiles]);
						exit(2);
					}
					fcode[nfiles++] = rep->r1.fcode;
				}
				break;

			case 'w':
				rep->r1.command = WCOM;
				if(*cp++ != ' ') {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				if(nfiles >= 10){
					fprintf(stderr, "Too many files in w commands\n");
					exit(2);
				}

				text(fname[nfiles], &fname[LOCMAXNFILES][0]);
				for(i = nfiles - 1; i >= 0; i--)
					if(cmp(fname[nfiles], fname[i]) == 0) {
						rep->r1.fcode = fcode[i];
						goto done;
					}

				if((rep->r1.fcode = fopen(fname[nfiles], "w")) == NULL) {
					fprintf(stderr, "Cannot create %s\n", fname[nfiles]);
					exit(2);
				}
				fcode[nfiles++] = rep->r1.fcode;
				break;

			case 'x':
				rep->r1.command = XCOM;
				break;

			case 'y':
				rep->r1.command = YCOM;
				sseof = *cp++;
				rep->r3.p = recur;
				if (ycomp(rep->r3.p,&recur)) {
					fprintf(stderr, CGMES, linebuf);
					exit(2);
				}
				break;

		}
done:
		if(++rep >= ptrend) {
			fprintf(stderr, "Too many commands, last: %s\n", linebuf);
			exit(2);
		}

		rep->r1.ad1lno = LNO_NONE;
		rep->r1.ad2lno = LNO_NONE;
		rep->r1.re1lno = LNO_NONE;

		if(*cp++ != '\0') {
			if(cp[-1] == ';')
				goto comploop;
			fprintf(stderr, CGMES, linebuf);
			exit(2);
		}

	}
	rep->r1.command = 0;
	lastre = op;
}
int	compsub(regex_t *expbuf,char *rhsbuf,char **rp)
{
	register char   *p, *q;

	p = rhsbuf;
	q = cp;
	for(;;) {
		if(p > reend) {
			fprintf(stderr, TMMES, linebuf);
			exit(2);
		}
		if((*p = *q++) == '\\') {
			p++;
			if(p > reend) {
				fprintf(stderr, TMMES, linebuf);
				exit(2);
			}
			*p = *q++;
			if(*p > expbuf->re_nsub + '0' && *p <= '9')
				return(1);
			p++;
			continue;
		}
		if(*p == sseof) {
			*p++ = '\0';
			cp = q;
			*rp = p;
			return(0);
		}
		if(*p++ == '\0')
			return(1);
	}
}

rline(lbuf)
char    *lbuf;
{
	register char   *p, *q;
	register	t;
	static char     *saveq;

	p = lbuf - 1;

	if(eflag) {
		if(eflag > 0) {
			eflag = -1;
			if(eargc-- <= 0)
				exit(2);
			q = *++eargv;
			while(*++p = *q++) {
				if(*p == '\\') {
					if((*++p = *q++) == '\0') {
						saveq = 0;
						return(-1);
					} else
						continue;
				}
				if(*p == '\n') {
					*p = '\0';
					saveq = q;
					return(1);
				}
			}
			saveq = 0;
			return(1);
		}
		if((q = saveq) == 0)    return(-1);

		while(*++p = *q++) {
			if(*p == '\\') {
				if((*++p = *q++) == '0') {
					saveq = 0;
					return(-1);
				} else
					continue;
			}
			if(*p == '\n') {
				*p = '\0';
				saveq = q;
				return(1);
			}
		}
		saveq = 0;
		return(1);
	}

	while((t = getc(fin)) != EOF) {
		*++p = t;
		if(*p == '\\') {
			t = getc(fin);
			*++p = t;
		}
		else if(*p == '\n') {
			*p = '\0';
			return(1);
		}
	}
	return(-1);
}


int	address(regex_t *rep,int *replno,char *s)
{
	char   *rcp;
	char *base_rcp;
	char *scp;
	long    lno;
	int	result = 0;

	if (s != NULL)
		rcp = s;
	else
		rcp = cp;
	base_rcp = rcp;
	if(*rcp == '$') {
		*replno = LNO_LAST;
		rcp++;
		goto update_and_return;
	}
	if (*rcp == '/' || *rcp == '\\' ) {
		if (scansavestr(&address_string,&rcp,1))
			return(1);
		if (address_string.length == 0) {
			*replno = LNO_EMPTY;
			goto update_and_return;
		}
		*replno = LNO_RE;
		result = regcomp(rep,
				 address_string.s,
				 REG_NOSUB);
		goto update_and_return;
	}

	lno = 0;

	while(*rcp >= '0' && *rcp <= '9')
		lno = lno*10 + *rcp++ - '0';

	if(rcp > base_rcp) {
		*replno = nlno;
		tlno[nlno++] = lno;
		if(nlno >= NLINES) {
			fprintf(stderr, "Too many line numbers\n");
			exit(2);
		}
	} else
		*replno = LNO_NONE;

update_and_return:
	if (s == NULL) 
		cp = rcp;
	else if (*rcp != 0)
		return(1);
	if (result != 0)
		return(1);
	return(0);
}
cmp(a, b)
char    *a,*b;
{
	register char   *ra, *rb;

	ra = a - 1;
	rb = b - 1;

	while(*++ra == *++rb)
		if(*ra == '\0') return(0);
	return(1);
}

char    *text(textbuf, maxptr)
char    *textbuf;
char	*maxptr;
{
	register char   *p, *q;

	p = textbuf;
	q = cp;
	for(;;) {

		if((*p = *q++) == '\\')
			*p = *q++;
		if(*p == '\0') {
			cp = --q;
			return(++p);
		}
		if((p+1) >= maxptr)
			break;
		p++;
	}
}


struct label    *search(ptr)
struct label    *ptr;
{
	struct label    *rp;

	rp = labtab;
	while(rp < ptr) {
		if(cmp(rp->asc, ptr->asc) == 0)
			return(rp);
		rp++;
	}

	return(0);
}


dechain()
{
	struct label    *lptr;
	union reptr     *rptr, *trptr;

	for(lptr = labtab; lptr < lab; lptr++) {

		if(lptr->address == 0) {
			fprintf(stderr, "Undefined label: %s\n", lptr->asc);
			exit(2);
		}

		if(lptr->chain) {
			rptr = lptr->chain;
			while(trptr = rptr->r2.lb1) {
				rptr->r2.lb1 = lptr->address;
				rptr = trptr;
			}
			rptr->r2.lb1 = lptr->address;
		}
	}
}

int ycomp(char *expbuf,char **nep)
{
	register char   c; 
	register char *ep, *tsp;
	register int i;
	char    *sp;

	ep = expbuf;
	if(ep + 0377 > reend) {
		fprintf(stderr, TMMES, linebuf);
		exit(2);
	}
	sp = cp;
	for(tsp = cp; *tsp != sseof; tsp++) {
		if(*tsp == '\\')
			tsp++;
		if(*tsp == '\n' || *tsp == '\0')
			return(1);
	}
	tsp++;

	while((c = *sp++) != sseof) {
		c &= 0377;
		if(c == '\\' && *sp == 'n') {
			sp++;
			c = '\n';
		}
		if((ep[c] = *tsp++) == '\\' && *tsp == 'n') {
			ep[c] = '\n';
			tsp++;
		}
		if(ep[c] == sseof || ep[c] == '\0')
			return(1);
	}
	if(*tsp != sseof)
		return(1);
	cp = ++tsp;

	for(i = 0; i < 0400; i++)
		if(ep[i] == 0)
			ep[i] = i;

	*nep = (ep + 0400);
	return(0);
}


void 
initsavestr(struct savestr_s *sp)
{
	sp->maxlength = 0;
	sp->length = 0;
	sp->s = NULL;
	bracket = NULL;	/* Initialize bracket expression open position */
}


void 
freesavestr(struct savestr_s *sp)
{
	if (sp->s != NULL)
		free((void *) sp->s);
	initsavestr(sp);
}


int
savestrn(struct savestr_s *sp, char *s,int n)
{
	int	len;
	char	*ns;

	if (s == NULL)
		s = "";
	len = strlen(s);
	if (n >= 0 &&
	    len > n)
		len = n;
	if (len > sp->maxlength ||
	    sp->maxlength == 0) {
		sp->maxlength = len;
		if (sp->maxlength < 10)
			sp->maxlength = 10;
		ns = (char *) malloc(sp->maxlength + 1);
		if (ns == NULL) {
			fprintf(stderr,"Cannot allocate %d bytes of working storage\n",len + 1);
			exit(1);
		}
		if (sp->s != NULL)
			free((void *) sp->s);
		sp->s = ns;
	}
	sp->length = len;
	if (len > 0)
		strncpy(sp->s,s,len);
	sp->s[len] = 0;
	return(0);
}


int
scansavestr(struct savestr_s *sp, char **cpp,int do_escape)
{
	char	*scp;
	int	did_escape = 0;
	char	ch;
	int	i;
	int	saw_escape = 0;
	char	*np;

	sseof = *(*cpp)++;
	if (do_escape &&
	    sseof == '\\') {
		sseof = *(*cpp)++;
		if (sseof == 0)
			return(1);
		did_escape = 1;
	}
	scp = (*cpp);
	while ((ch = *(*cpp)) != 0) {
		if (!bracket && ch == '[' && sseof != ch)
			bracket = (char *)*cpp;	/* '[' position */
		if (ch == '\\') {
			(*cpp)++;
			ch = *(*cpp);
			if (ch == 0)
				break;
			if (ch == sseof ||
			    ch == 'n')
				saw_escape = 1;
		} else if (!bracket && (ch == sseof)) {
			savestrn(&address_string,scp,(*cpp) - scp);
			(*cpp)++;
			if (saw_escape) {
				for (scp = address_string.s, np = scp;
				     (ch = (*np)) != 0;
				     scp++, np++) {
					if (ch == '\\') {
						np++;
						ch = *np;
						if (ch == sseof) {
							NULL;
						} else if (ch == 'n') {
							ch = '\n';
						} else {
							*scp++ = '\\';
						}
					}
					*scp = ch;
				}
				*scp = 0;
				address_string.length = scp - address_string.s;
			}
			return(0);
		}
		if (bracket && ((*cpp - 1) != bracket) && 
			 (ch == ']'))
			bracket = 0;
		(*cpp)++;
	}
	return(1);
}
