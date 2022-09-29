/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)pr:pr.c	1.12"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/pr/RCS/pr.c,v 1.15 1998/04/17 01:48:20 bitbug Exp $"

/*
 *	PR command (print files in pages and columns, with headings)
 *	2+head+2+page[56]+5
 */

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

#define ESC	'\033'
#define LENGTH	66
#define LINEW	72
#define NUMW	5
#define MARGIN	10
#define DEFTAB	8
#define NFILES	10

#define STDINNAME()	nulls
#define PROMPT()	putc('\7', stderr) /* BEL */
#define NOFILE	nulls
#define TABS(N,C)	if ((N = intopt(argv, &C)) < 0) N = DEFTAB
#define ETABS	(Inpos % Etabn)
#define ITABS	(Itabn > 0 && Nspace >= (nc = Itabn - Outpos % Itabn))
#define NSEPC	'\t'
#define done()	if (Ttyout) chmod(Ttyout, Mode)

#define FORMAT	"%b %e %H:%M %Y"	/* ---date time format--- 
	b -- abbreviated month name 
	e -- day of month
	H -- Hour (24 hour version)
	M -- Minute
	Y -- Year in the form ccyy */

typedef char CHAR;
typedef int ANY;
typedef unsigned UNS;
typedef struct { 
	FILE *f_f; 
	char *f_name; 
	int f_nextc; 
} FILS;
typedef struct {int fold; int skip; int eof; } foldinf;

#define EMPTY	14	/* length of " -- empty file" */

typedef struct err { 
	struct err *e_nextp; 
	char *e_mess; 
} ERR;

ERR *Err = NULL, *Lasterr = (ERR *)&Err;

ANY *getspace();

FILS *Files;
FILE *mustopen();

int Mode;
int Multi = 0, Nfiles = 0, Error = 0;
void onintr();

void foldpage(), foldbuf(), unget();

char nulls[] = "";
char *ttyname(), *Ttyout, obuf[BUFSIZ];

static char	time_buf[50];	/* array to hold the time and date */

extern	void	exit();
extern	void	_exit();
extern	char	*malloc();
extern	time_t	time();

/*
 * Used to incorporate 'fold'.  Note that short cuts around multibyte
 * char support were taken.  The next pass should probably attempt
 * to pick up generic SVR4 pr(1).
 */
#define GETC		getc
#define UNGETC		ungetc
#define PUTCHAR		putchar
#define iswprint	isprint
#define SCRWIDTH(c)	((c) ? 1 : 0)


struct	prtext {
	int     flag;
	char    *cmsg;		/* catalog */
	char    *dmsg;		/* default */
};

char	cmd_label[] = "UX:pr";

/*
 * error messages
 */
struct prtext prerr[] = {
	{ SGINL_NOSYSERR,	_SGI_DMMX_illoption,
		"illegal option -- %c"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_pr_optmcolumn,
		"only one of either -m or -column allowed" },
	{ SGINL_NOSYSERR,	_SGI_DMMX_pr_optacolumn,
		"-a valid only with -column"		},
};

#define	ERR_option		0
#define	ERR_optmcolumn		1
#define	ERR_optacolumn		2

#define	PR_MAX_ERR		2

/*
 * some msg prints
 */
static void
error(nbr, arg1, arg2)
int nbr;
{
	register struct prtext *ep;

	ep = prerr + nbr;
	_sgi_nl_error(ep->flag, cmd_label,
	    gettxt(ep->cmsg, ep->dmsg),
	    arg1,
	    arg2);
}

errprint() /* print accumulated error reports */
{
	fflush(stdout);
	for ( ; Err != NULL; Err = Err->e_nextp) {
		fprintf(stderr, "%s\n", Err->e_mess);
	}
	done();
}

struct prtext dieerr[] = {
	{ SGINL_NOSYSERR,	_SGI_DMMX_pr_toomanyfiles,
		"too many files"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_pr_diewidth,
		"width too small"			},
	{ SGINL_SYSERR,		_SGI_DMMX_CannotOpen,
		"Cannot open %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_pr_diepagebuf,
		"page-buffer overflow"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_outofmem,
		"Out of memory"				},
};

#define	DIE_2MFILES	0
#define	DIE_WIDTH	1
#define	DIE_open	2
#define	DIE_pagebuf	3
#define	DIE_nomem	4

static void
die(nbr, arg)
int nbr;
{
	register struct prtext *ep;

	Error++;
	errprint();
	ep = dieerr + nbr;
	_sgi_nl_error(ep->flag, cmd_label,
	    gettxt(ep->cmsg, ep->dmsg), arg);
	exit(1);
}

/*
 * usage print
 */
struct prtext prusage[] = {
	{ SGINL_USAGE,		_SGI_DMMX_pr_usage1,
		"pr [-column [-wwidth] [-a]] [-ect] [-ict] [-drtfp] [+page] [-nsk]"	},
	{ SGINL_USAGESPC,	_SGI_DMMX_pr_usage2,
		"   [-ooffset] [-llength] [-sseparator] [-h header] [file ...]"		},
	{ SGINL_USAGESPC,	_SGI_DMMX_pr_usage3,
		"   [-m      [-wwidth]]      [-ect] [-ict] [-drtfp] [+page] [-nsk]"	},
	{ SGINL_USAGESPC,	_SGI_DMMX_pr_usage4,
		"   [-ooffset] [-llength] [-sseperator] [-h header] file1 file2 ..."	},
	{ 0, 0 },
};

void
usage()
{
	register struct prtext *tp;

	for(tp = prusage; tp->cmsg; tp++)
	    _sgi_nl_usage(tp->flag, cmd_label, gettxt(tp->cmsg, tp->dmsg));
	exit(1);
}

void
fixtty()
{
	struct stat sbuf;

	setbuf(stdout, obuf);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) signal(SIGINT, onintr);
	if (Ttyout= ttyname(fileno(stdout))) {		/* is stdout a tty? */
		stat(Ttyout, &sbuf);
		Mode = sbuf.st_mode&0777;		/* save permissions */
		chmod(Ttyout, 0600);
	}
	return;
}

char *GETDATE() /* return date file was last modified */
{
	static char *now = NULL;
	static struct stat64 sbuf, nbuf;

	if (Nfiles > 1 || Files->f_name == nulls) {
		if (now == NULL) {
			time(&nbuf.st_mtime);
			cftime(time_buf, gettxt(":723",FORMAT), &nbuf.st_mtime);
			now = time_buf;
		}
		return (now);
	} else {
		stat64(Files->f_name, &sbuf);
		cftime(time_buf, gettxt(":723",FORMAT), &sbuf.st_mtime);
		return (time_buf);
	}
}

/*
 * main entry
 */
int
main(argc, argv)
int argc;
char *argv[];
{
	FILS fstr[NFILES];
	int nfdone = 0;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	Files = fstr;
	for (argc = findopt(argc, argv); argc > 0; --argc, ++argv)
		if (Multi == 'm') {
			if (Nfiles >= NFILES - 1)
				die(DIE_2MFILES);
			if (mustopen(*argv, &Files[Nfiles++]) == NULL)
				++nfdone; /* suppress printing */
		} 
		else {
			if (print(*argv))
				fclose(Files->f_f);
			++nfdone;
		}
	if (!nfdone) /* no files named, use stdin */
		print(NOFILE); /* on GCOS, use current file, if any */
	errprint(); /* print accumulated error reports */
	exit(Error);	/*NOTREACHED*/
}


long Lnumb = 0;
FILE *Ttyin = stdin;
int Dblspace = 1, Fpage = 1, Formfeed = 0,
Length = LENGTH, Linew = 0, Offset = 0, Ncols = 1, Pause = 0, Sepc = 0,
Colw, Plength, Margin = MARGIN, Numw, Nsepc = NSEPC, Report = 1,
Etabn = 0, Etabc = '\t', Itabn = 0, Itabc = '\t', fold = 0,
foldcol = 0, alleof = 0, Tflag = 0;
int OptOver = 0;
char *Head = NULL;
char *mac_label = NULL;
int   label_truncated;
CHAR *Buffer = NULL, *Bufend, *Bufptr;
UNS Buflen;
typedef struct { 
	CHAR *c_ptr, *c_ptr0; 
	long c_lno; 
	int c_skip;
} *COLP;
COLP Colpts;
foldinf *Fcol;
/* findopt() returns eargc modified to be    */
/* the number of explicitly supplied         */
/* filenames, including '-', the explicit    */
/* request to use stdin.  eargc == 0 implies */
/* that no filenames were supplied and       */
/* stdin should be used.		     */
findopt(argc, argv) char *argv[];
{
	char **eargv = argv;
	int eargc = 0, c;
	int mflg = 0, aflg = 0;
	int i;
	fixtty();
	while (--argc > 0) {
		argv++;
		if (!OptOver) {
		switch (c = **argv) {
		case '-':
			if ((c = *++*argv) == '\0') break;
			if (c == '-') {
				OptOver = 1;
				continue;
			}
		case '+':
			do {
				if (isdigit(c)) {
					--*argv;
					Ncols = atoix(argv);
				}
				else
					switch (c) {
					case '+': 
						if ((Fpage = atoix(argv)) < 1)
							Fpage = 1;
						continue;
					case 'd': 
						Dblspace = 2;
						continue;
					case 'e': 
						TABS(Etabn, Etabc);
						continue;
					case 'F':
					case 'f': 
						++Formfeed;
						continue;
					case 'M':
						if (--argc > 0)
							mac_label = argv[1];
						continue;
					case 'h': 
						if (--argc > 0)
							Head = argv[1];
						continue;
					case 'i': 
						TABS(Itabn, Itabc);
						continue;
#if 0
					case 'L':
						fold++;
						continue;
#endif
					case 'l': 
						if ((*argv)[1] == '\0') {
							if (--argc <= 0)
								usage();
							(*(++argv))--;
						}
						Length = atoix(argv);
						continue;
					case 'a': 
						aflg++;
						Multi = c;
						continue;
					case 'm': 
						mflg++;
						Multi = c;
						continue;
					case 'o': 
						if ((*argv)[1] == '\0') {
							if (--argc <= 0)
								usage();
							(*(++argv))--;
						}
						Offset = atoix(argv);
						continue;
					case 'p': 
						++Pause;
						continue;
					case 'r': 
						Report = 0;
						continue;
					case 's': 
						if ((Sepc = (*argv)[1]) != '\0')
							++*argv;
						else
							Sepc = '\t';
						continue;
					case 't': 
						Tflag++;
						Margin = 0;
						continue;
					case 'w': 
						if ((*argv)[1] == '\0') {
							if (--argc <= 0)
								usage();
							(*(++argv))--;
						}
						Linew = atoix(argv);
						continue;
					case 'n': 
						++Lnumb;
						if ((Numw = intopt(argv,&Nsepc)) <= 0)
							Numw = NUMW;
						continue;
					default :
						error(ERR_option, (char)c);
						usage();
					}
				/* Advance over options    */
				/* until null is found.    */
				/* Allows clumping options */
				/* as in "pr -pm f1 f2".   */
			} while ((c = *++*argv) != '\0');

			if (Head == argv[1]) ++argv;
			if (mac_label != NULL && mac_label == argv[1]) {
				if (strlen (mac_label) > 60) {
					label_truncated = 1;
					mac_label = strdup (argv[1]);
					if (mac_label == (char *) NULL) {
						fprintf (stderr,
							 "out of memory\n");
						exit(1);
					}
					mac_label[60] = '\0';
				}
				++argv;
			}
			continue;
		}
		}
		*eargv++ = *argv;
		++eargc;	/* count the filenames */
	}
#ifdef notdef
	if (mflg && (Ncols > 1)) {
		error(ERR_optmcolumn);
		usage();
	}
#endif
	if (aflg && (Ncols < 2)) {
		error(ERR_optacolumn);
		usage();
	}
	if (Ncols == 1 && fold)
		Multi = 'm';
	if (Length == 0) Length = LENGTH;
	if (Length <= Margin) { Margin = 0; Tflag++; }
	Plength = Length - Margin/2;
	if (Multi == 'm') Ncols = eargc;
	switch (Ncols) {
	case 0:
		Ncols = 1;
	case 1:
		break;
	default:
		if (Etabn == 0) /* respect explicit tab specification */
			Etabn = DEFTAB;
		if (Itabn == 0)
			Itabn = DEFTAB;
	}
	if ((Fcol = (foldinf *)malloc(sizeof(foldinf) * Ncols)) == NULL) {
		die(DIE_nomem);
	}
	for ( i=0; i<Ncols; i++)
		Fcol[i].fold = Fcol[i].skip = 0;
	if (Linew == 0) Linew = Sepc == 0 ? LINEW : 512;
	if (Lnumb) {
		int numw;

		if (Nsepc == '\t') {
			if(Itabn == 0)
				numw = Numw + DEFTAB - (Numw % DEFTAB);
			else
				numw = Numw + Itabn - (Numw % Itabn);
		} else {
			numw = Numw + ((isprint(Nsepc)) ? 1 : 0);
		}
		Linew -= (Multi == 'm') ? numw : numw * Ncols;
	}
	if ((Colw = (Linew - Ncols + 1)/Ncols) < 1)
		die(DIE_WIDTH);
	if (Ncols != 1 && Multi == 0) {
		Buflen = ((UNS)(Plength/Dblspace + 1))*(Linew+1)*sizeof(CHAR);
		Buffer = (CHAR *)getspace(Buflen);
		Bufptr = Bufend = &Buffer[Buflen];
		Colpts = (COLP)getspace((UNS)((Ncols+1)*sizeof(*Colpts)));
	}
	/* is stdin not a tty? */
	if (Ttyout && (Pause || Formfeed) && !ttyname(fileno(stdin)))
		Ttyin = fopen("/dev/tty", "r");
	return (eargc);
}

intopt(argv, optp) char *argv[]; 
int *optp;
{
	int c;

	if ((c = (*argv)[1]) != '\0' && !isdigit(c)) { 
		*optp = c; 
		++*argv; 
	}
	return ((c = atoix(argv)) != 0 ? c : -1);
}

int Page, C = '\0', Nspace, Inpos;

print(name) char *name;
{
	static int notfirst = 0;
	char *date = NULL, *head = NULL;
	int c;

	if (Multi != 'm' && mustopen(name, &Files[0]) == NULL) return (0);
	if (Multi == 'm' && Nfiles == 0 && mustopen(name, &Files[0]) == NULL)
		die(DIE_open, "<stdin>");
	if (Buffer) ungetc(Files->f_nextc, Files->f_f);
	if (Lnumb) Lnumb = 1;
	for (Page = 0; ; putpage()) {
		if (C == EOF && !(fold && Buffer)) break;
		if (Buffer) nexbuf();
		Inpos = 0;
		if (get(0) == EOF) break;
		fflush(stdout);
		if (++Page >= Fpage) {
			if (Ttyout && (Pause || Formfeed && !notfirst++)) {
				PROMPT(); /* prompt with bell and pause */
				while ((c = getc(Ttyin)) != EOF && c != '\n') ;
			}
			if (Margin == 0) continue;
			if (date == NULL) date = GETDATE();
			if (head == NULL) head = Head != NULL ? Head :
			Nfiles < 2 ? Files->f_name : nulls;
			printf("\n\n");
			Nspace = Offset;
			putspace();
			if (mac_label != (char *) NULL) {
				printf (label_truncated ?
					"    *** TRUNCATED ***\n    %s\n\n" :
					"\n    %s\n\n", mac_label);
			}
			else
				printf(gettxt(_SGI_DMMX_pr_head,
					      "%s  %s Page %d\n\n\n"),
				       date, head, Page);
		}
	}
	C = '\0';
	return (1);
}

int Outpos, Lcolpos, Pcolpos, Line;

putpage()
{
	register int colno;

	if (fold) {
		foldpage();
		return;
	}

	for (Line = Margin/2; ; get(0)) {
		for (Nspace = Offset, colno = 0, Outpos = 0; C != '\f'; ) {
			if (Lnumb && C != EOF && ((colno == 0 && Multi == 'm') || Multi != 'm')) {
				if (Page >= Fpage) {
					putspace();
					printf("%*ld%c", Numw, Buffer ?
					    Colpts[colno].c_lno++ : Lnumb, Nsepc);
				}
				++Lnumb;
			}
			for (Lcolpos = 0, Pcolpos = 0;
			    C != '\n' && C != '\f' && C != EOF; get(colno)) {
#ifdef TRUNCATE_AT_WIDTH
			        if (Lcolpos < Linew)
#endif
					put(C);
			}
			if (C == EOF || ++colno == Ncols ||
			    C == '\n' && get(colno) == EOF) break;
			if (Sepc) PUTCHAR(Sepc);
			else if ((Nspace += Colw - Lcolpos + 1) < 1) Nspace = 1;
		}
		if (C == EOF) {
			if (Margin != 0) break;
			if (colno != 0) put('\n');
			else if (Formfeed == 0 && Ncols == 1 && !Tflag) {
				put('\f');
				put('\n');
			}
			return;
		}
		if (C == '\f') break;
		put('\n');
		if (Dblspace == 2 && Line < Plength) put('\n');
		if (Line >= Plength) break;
	}
	if (mac_label != (char *) NULL) {
		while (Line < Length - 2)
			put ('\n');
		printf (label_truncated ?
			"    %s\n    *** TRUNCATED ***\n" :
			"    %s\n\n", mac_label);
	}
	else if (Formfeed) put('\f');
	else while (Line < Length) put('\n');
}


void
foldpage()
{
	register int colno;
	CHAR keep;
	int i;
	static int  sl;

	for (Line = Margin/2; ; get(0)) {
		for (Nspace = Offset, colno = 0, Outpos = 0; C != '\f'; ) {
			if (Lnumb && Multi == 'm' && foldcol) {
				if (!Fcol[colno].skip) {
					unget(colno);
					putspace();
					if (!colno) {
					    for(i=0;i<=Numw;i++) (void) printf(" ");
					    (void) printf("%C",Nsepc);
					    Outpos += Numw;
					    if (iswprint(Nsepc))
						Outpos += SCRWIDTH(Nsepc);
					    else if (Nsepc == '\t')
						Outpos = Outpos + DEFTAB
						    - (Outpos % DEFTAB);
					}
					for(i=0;i<=Colw;i++) (void) printf(" ");
					Outpos += Colw;
					(void) PUTCHAR(Sepc);
					if (++colno == Ncols) break;
					(void) get(colno);
					continue;
				}
				else if (!colno) Lnumb = sl;
			}

			if (Lnumb && (C != EOF)
			&& ((colno == 0 && Multi == 'm') || Multi != 'm')) {
				if (Page >= Fpage) {
					putspace();
					if ((foldcol &&
					Fcol[colno].skip && Multi!='a') ||
					(Fcol[0].fold && Multi == 'a') ||
					(Buffer && Colpts[colno].c_skip)) {
						for (i=0; i<Numw;i++) (void) printf(" ");
						(void) printf("%C",Nsepc);
						if (Buffer) {
							Colpts[colno].c_lno++;
							Colpts[colno].c_skip = 0;
						}
					}
					else
					    (void) printf("%*ld%C", Numw,
						Buffer ?
						    Colpts[colno].c_lno++
						    : Lnumb, Nsepc);
					Outpos += Numw;
					if (iswprint(Nsepc))
					    Outpos += SCRWIDTH(Nsepc);
					else if (Nsepc == '\t')
					    Outpos = Outpos + DEFTAB
						    - (Outpos % DEFTAB);
				}
				sl = Lnumb++;
			}
			for (Lcolpos = 0, Pcolpos = 0;
				C != '\n' && C != '\f' && C != EOF;
							get(colno))
					if (put(C)) {
					    unget(colno);
					    Fcol[(Multi=='a')?0:colno].fold = 1;
					    break;
					}
					else if (Multi == 'a') {
					    Fcol[0].fold = 0;
					}
			if (Buffer) {
				alleof = 1;
				for (i=0; i<Ncols; i++)
					if (!Fcol[i].eof) alleof = 0;
				if (alleof || ++colno == Ncols)
					break;
			} else if (C == EOF || ++colno == Ncols)
				break;
			keep = C;
			(void) get(colno);
			if (keep == '\n' && C == EOF)
				break;
			if (Sepc) (void) PUTCHAR(Sepc);
			else if ((Nspace += Colw - Lcolpos + 1) < 1) Nspace = 1;
		}
		foldcol = 0;
		if (Lnumb && Multi != 'a') {
			for (i=0; i<Ncols; i++) {
				if (Fcol[i].skip = Fcol[i].fold)
					foldcol++;
				Fcol[i].fold = 0;
			}
		}
		if (C == EOF) {
			if (Margin != 0) break;
			if (colno != 0) (void) put('\n');
			return;
		}
		if (C == '\f') break;
		(void) put('\n');
		(void) fflush(stdout);
		if (Dblspace == 2 && Line < Plength) (void) put('\n');
		if (Line >= Plength) break;
	}
	if (Formfeed) (void) put('\f');
	else while (Line < Length) (void) put('\n');
}

nexbuf()
{
	register CHAR *s = Buffer;
	register COLP p = Colpts;
	int j, c, bline = 0;

	if (fold) {
		foldbuf();
		return;
	}

	for ( ; ; ) {
		p->c_ptr0 = p->c_ptr = s;
		if (p == &Colpts[Ncols]) return;
		(p++)->c_lno = Lnumb + bline;
		for (j = (Length - Margin)/Dblspace; --j >= 0; ++bline)
			for (Inpos = 0; ; ) {
				if ((c = getc(Files->f_f)) == EOF) {
					for (*s = EOF; p <= &Colpts[Ncols]; ++p)
						p->c_ptr0 = p->c_ptr = s;
					balance(bline);
					return;
				}
				if (isprint(c)) ++Inpos;
				if (Inpos <= Colw || c == '\n') {
					*s = c;
					if (++s >= Bufend)
						die(DIE_pagebuf);
				}
				if (c == '\n') break;
				switch (c) {
				case '\b': 
					if (Inpos == 0) --s;
				case ESC:  
					if (Inpos > 0) --Inpos;
				}
			}
	}
}

void
foldbuf()
{
	int num, i, colno=0;
	int size = Buflen;
	CHAR *s, *d;
	register COLP p=Colpts;

	for (i =0; i< Ncols; i++)
		Fcol[i].eof = 0;
	d = Buffer;
	if (Bufptr != Bufend) {
		s = Bufptr;
		while (s < Bufend) *d++ = *s++;
		size -= (Bufend - Bufptr);
	}
	Bufptr = Buffer;
	p->c_ptr0 = p->c_ptr = Buffer;
	if (p->c_lno == 0) {
		p->c_lno = Lnumb;
		p->c_skip = 0; 
	}
	else {
		p->c_lno = Colpts[Ncols-1].c_lno;
		if (p->c_skip = Colpts[Ncols].c_skip)
			p->c_lno--;
	}
	if ((num = Fread(d, 1, (size_t)size, Files->f_f)) != size) {
		for (*(d+num) = (CHAR)EOF; (++p)<= &Colpts[Ncols]; )
			p->c_ptr0 = p->c_ptr = (d+num);
		balance(0);
		return;
	}
	i = (Length - Margin) / Dblspace;
	do {
		(void) readbuf(&Bufptr, i, p++);
	} while (++colno < Ncols);
}

int
Fread(ptr, size, nitems, stream)
CHAR *ptr;
size_t size;
size_t nitems;
FILE *stream;
{
	register CHAR *p = ptr, *pe = ptr + size * nitems;

	while (p < pe && (*p = GETC(stream)) != (CHAR)EOF) p++;
	return (p - ptr);
}

balance(bline) /* line balancing for last page */
{
	CHAR *s = Buffer;
	register COLP p = Colpts;
	int colno = 0, j, c, l;
	int lines;

	if (!fold) {
		c = bline % Ncols;
		l = (bline + Ncols - 1)/Ncols;
		bline = 0;
		do {
			for (j = 0; j < l; ++j)
				while (*s++ != '\n') ;
			(++p)->c_lno = Lnumb + (bline += l);
			p->c_ptr0 = p->c_ptr = s;
			if (++colno == c) --l;
		} while (colno < Ncols - 1);
	} else {
		lines = readbuf(&s, 0, 0);
		l = (lines + Ncols - 1)/Ncols;
		if (l > ((Length - Margin) / Dblspace)) {
			l = (Length - Margin) / Dblspace;
			c = Ncols;
		} else
			c = lines % Ncols;
		s = Buffer;
		do {
			(void) readbuf(&s, l, p++);
			if (++colno == c) --l;
		} while (colno < Ncols);
		Bufptr = s;
	}
}

int
readbuf(s, lincol, p)
CHAR **s;
int lincol;
COLP p;
{
	int lines = 0;
	int chars = 0, width;
	int nls = 0, move;
	int skip = 0;
	int decr = 0;

	width = (Ncols == 1) ? Linew : Colw;
	while (**s != (CHAR)EOF) {
		switch (**s) {
			case L'\n': lines++; nls++; chars=0; skip = 0;
				  break;
			case L'\b':
			case ESC: if (chars) chars--;
				   break;
			case L'\t':
				move = Itabn - ((chars + Itabn) % Itabn);
				move = (move < width-chars) ? move : width-chars;
				chars += move;
			default:
				chars += (isprint(**s) != 0);
		}
		if (chars > width) {
			lines++; skip++; decr++; chars = 0; 
		}
		if (lincol && lines == lincol) {
			(p+1)->c_lno = p->c_lno + nls;
			(++p)->c_skip = skip;
			if (**s == L'\n') (*s)++;
			p->c_ptr0 = p->c_ptr = (CHAR *)*s;
			return(lines);
		}
		if (decr) decr = 0;
		else (*s)++;
	}
	return(lines);
}

get(colno)
{
	static int peekc = 0;
	register COLP p;
	register FILS *q;
	register int c;

	if (peekc)
	{ 
		peekc = 0; 
		c = Etabc; 
	}
	else if (Buffer) {
		p = &Colpts[colno];
		if (p->c_ptr >= (p+1)->c_ptr0) c = EOF;
		else if ((c = *p->c_ptr) != EOF) ++p->c_ptr;
		if (fold && c == (CHAR)EOF) Fcol[colno].eof = 1;
	} else if ((c = 
	    (q = &Files[Multi == 'a' ? 0 : colno])->f_nextc) == EOF) {
		for (q = &Files[Nfiles]; --q >= Files && q->f_nextc == EOF; ) ;
		if (q >= Files) c = '\n';
	} else
		q->f_nextc = getc(q->f_f);
	if (Etabn != 0 && c == Etabc) {
		++Inpos;
		peekc = ETABS;
		c = ' ';
	} else if (isprint(c))
		++Inpos;
	else
		switch (c) {
		case '\b':
		case ESC:
			if (Inpos > 0) --Inpos;
			break;
		case '\f':
			if (Ncols == 1) break;
			c = '\n';
		case '\n':
		case '\r':
			Inpos = 0;
		}
	return (C = c);
}

put(c)
{
	int move = 0;
	int width = Colw;
	int sp=Lcolpos;

	if (fold && Ncols == 1) width = Linew;
	switch (c) {
	case ' ':
		if((!fold && Ncols < 2) || (Lcolpos < Colw)) {
			++Nspace;
			++Lcolpos;
		}
		goto rettab;
	case '\t':
		if(Itabn == 0)
			break;
		if(Lcolpos < Colw) {
			move = Itabn - ((Lcolpos + Itabn) % Itabn);
			move = (move < Colw-Lcolpos) ? move : Colw-Lcolpos;
			Nspace += move;
			Lcolpos += move;
		}
rettab:
		if (fold && sp == Lcolpos)
		if (Lcolpos >= width)
				return(1);
		return(0);	
	case '\b':
		if (Lcolpos == 0) return(1);
		if (Nspace > 0) { 
			--Nspace; 
			--Lcolpos; 
			return(0); 
		}
		if (Lcolpos > Pcolpos) { 
			--Lcolpos; 
			return(0); 
		}
	case ESC:
		move = -1;
		break;
	case '\n':
		++Line;
	case '\r':
	case '\f':
		Pcolpos = 0; 
		Lcolpos = 0; 
		Nspace = 0; 
		Outpos = 0;
	default:
		move = (isprint(c) != 0);
	}
	if (Page < Fpage) return(0);
	if (Lcolpos > 0 || move > 0) Lcolpos += move;
	putspace();
#ifdef old
	if (Ncols < 2 || Lcolpos <= Colw) {
		putchar(c);
		Outpos += move;
		Pcolpos = Lcolpos;
	}
#endif
	if ((!fold && Ncols < 2) || (Lcolpos <= width)) {
		(void) PUTCHAR(c);
		Outpos += move;
		Pcolpos = Lcolpos;
	}
	else if (Lcolpos - move < width) {
		Nspace += (width - (Lcolpos - move));
		putspace();
	}
	if (fold && Lcolpos > width)
		return(1);

	return(0);
}

putspace()
{
	int nc;

	for ( ; Nspace > 0; Outpos += nc, Nspace -= nc)
		if (ITABS && !fold && nc >= 2)
			putchar(Itabc);
		else {
			nc = 1;
			putchar(' ');
		}
}

void
unget(colno)
int colno;
{
	if (Buffer) {
		if (*(Colpts[colno].c_ptr-1) != L'\t')
			--(Colpts[colno].c_ptr);
		if (Colpts[colno].c_lno) Colpts[colno].c_lno--;
	}
	else {
		if ((Multi == 'm' && colno == 0) || Multi != 'm')
			if (Lnumb && !foldcol) Lnumb--;
		colno = (Multi == 'a') ? 0 : colno;
		(void) UNGETC(Files[colno].f_nextc, Files[colno].f_f);
		Files[colno].f_nextc = C;
	}
}

atoix(p) register char **p;
{
	register int n = 0, c;

	while (isdigit(c = *++*p)) n = 10*n + c - '0';
	--*p;
	return (n);
}


/* Defer message about failure to open file to prevent messing up
   alignment of page with tear perforations or form markers.
   Treat empty file as special case and report as diagnostic.
*/
FILE *mustopen(s, f)
char *s; 
register FILS *f;
{
	char msgbuf[BUFSIZ];

	if (*s == '\0') {
		f->f_name = STDINNAME();
		f->f_f = stdin;
	} else if ((f->f_f = fopen(f->f_name = s, "r")) == NULL) {
		_sgi_sfmtmsg(msgbuf, 0, cmd_label, MM_ERROR,
		    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"),
		    f->f_name);
		s = strcpy((char *)getspace((UNS)(strlen(msgbuf) + 1)), msgbuf);
	}
	if (f->f_f != NULL) {
		if ((f->f_nextc = getc(f->f_f)) != EOF || Multi == 'm')
			return (f->f_f);
		_sgi_sfmtmsg(msgbuf, 0, cmd_label, MM_ERROR,
		    gettxt(_SGI_DMMX_is_empty, "file '%s' is empty"),
		    f->f_name);
		s = strcpy((char *)getspace((UNS)(strlen(msgbuf) + 1)), msgbuf);
		fclose(f->f_f);
	}
	Error = 1;
	if (Report)
		if (Ttyout) { /* accumulate error reports */
			Lasterr = Lasterr->e_nextp = (ERR *)getspace((UNS)sizeof(ERR));
			Lasterr->e_nextp = NULL;
			Lasterr->e_mess = s;
		} 
		else { /* ok to print error report now */
			fprintf(stderr, "%s", s);
		}
	return ((FILE *)NULL);
}

ANY *getspace(n) UNS n;
{
	ANY *t;

	if ((t = (ANY *)malloc(n)) == NULL)
		die(DIE_nomem);
	return (t);
}

void
onintr()
{
	++Error;
	errprint();
	_exit(1);
}

