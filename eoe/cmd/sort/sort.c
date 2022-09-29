/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	 "@(#)sort:sort.c	1.22.1.3"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <values.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <sys/euc.h>
#include <getwidth.h>

#define SIZE	256		/* size of character conversion table */
#define	N	16
#define	C	20
#define NF	10
#define MTHRESH	 8 /* threshhold for doing median of 3 qksort selection */
#define TREEZ	32 /* no less than N and best if power of 2 */

/*
 * Memory administration
 *
 * Using a lot of memory is great when sorting a lot of data.
 * Using a megabyte to sort the output of `who' loses big.
 * MAXMEM, MINMEM and DEFMEM define the absolute maximum,
 * minimum and default memory requirements.  Administrators
 * can override any or all of these via defines at compile time.
 * Users can override the amount allocated (within the limits
 * of MAXMEM and MINMEM) on the command line.
 *
 * For PDP-11s, memory is limited by the maximum unsigned number, 2^16-1.
 * Administrators can override this too.
 * Arguments to core getting routines must be unsigned.
 * Unsigned long not supported on 11s.
 *
 * SGI:we use an eighth of phys mem as the default max.
 * If they specify -y with no following arg we up it to 1/2 phys mem
 * We permit them to specify up to MAXMEM with the -y option..
 * For sanity everything is kept <2Gb
 */

#ifndef	MAXMEM
#ifdef pdp11
#define	MAXMEM ((1L << 16)-1)
#define	MAXUNSIGNED ((1L << 16)-1)
#else 
#define	MAXMEM	((unsigned)2048*1024*1024)	/* 2 Gigabyte maximum */
#endif
#endif
#define MAXMEMMB	(MAXMEM / (1024*1024))	/* MAXMEM in Mb */
unsigned long maxmem;
unsigned long physmemmb;

#ifndef	MINMEM
#define	MINMEM	  16384	/* 16K minimum */
#endif

#ifndef	DEFMEM
#ifdef sgi
/* brk is expensive - we can improve performance by getting more by default -
 * but we obviously don't want to waste memory - so we compromise and bump
 * up the default a bit
 */
#define DEFMEM	(64*1024)
#else
#define	DEFMEM	  32768	/* Same as old sort */
#endif
#endif


#define ASC 	0
#define NUM	1
#define MON	2

#define	blank(c) ((c)==' ' || (c)=='\t')

FILE	*os;
char	*dirtry[] = {"/var/tmp", "/tmp", NULL};
char	**dirs;
char	file1[100];
char	*file = file1;
char	*filep;
#define NAMEOHD 12 /* sizeof("/stm00000aa") */
int	nfiles;
int	*lspace;
int	*maxbrk;
unsigned tryfor;
unsigned alloc;
char bufin[BUFSIZ], bufout[BUFSIZ];	/* Use setbuf's to avoid malloc calls.
					** malloc seems to get heartburn
					** when brk returns storage.
					*/
int	maxrec;
/* cmpa uses strcmp(), cmp_coll uses strcoll() */
/* compare points to the appropriate function, depending on the locale */
int	cmp(), cmpa(), cmp_coll();
int	(*compare)();

unsigned char	*eol();
void	term();
int 	mflg;
int	nway;
int	cflg;
int	uflg;
#ifdef	sgi
int	iflag;
#endif
char	*outfil;
int unsafeout;	/*kludge to assure -m -o works*/
unsigned char	*tabchar;
int 	eargc;
char	**eargv;
struct btree {
    char *rp;
    int  rn;
} tree[TREEZ], *treep[TREEZ];
int	blkcnt[TREEZ];
char	**blkcur[TREEZ];
long	wasfirst = 0, notfirst = 0;
int	bonus;

/* make the compiler happy. --lguo */
void key_index(char *);
void sort(void);
void msort(char **, char **);
void insert(struct btree **, int);
void cline(char *, char *);
void merge(int, int);
void wline(char *);
void checksort(void);
void disorder(char *, char *);
void newfile(void);
void oldfile(void);
void safeoutfil(void);
void cant(char *);
void copyproto(void);
void initree(void);
void qksort(char **, char **);
void month_init(void);
void rderror(char *);
void wterror(int);

unsigned char zero[256];

unsigned char	fold[256];

unsigned char nofold[256] = {
	0000,0001,0002,0003,0004,0005,0006,0007,
	0010,0011,0012,0013,0014,0015,0016,0017,
	0020,0021,0022,0023,0024,0025,0026,0027,
	0030,0031,0032,0033,0034,0035,0036,0037,
	0040,0041,0042,0043,0044,0045,0046,0047,
	0050,0051,0052,0053,0054,0055,0056,0057,
	0060,0061,0062,0063,0064,0065,0066,0067,
	0070,0071,0072,0073,0074,0075,0076,0077,
	0100,0101,0102,0103,0104,0105,0106,0107,
	0110,0111,0112,0113,0114,0115,0116,0117,
	0120,0121,0122,0123,0124,0125,0126,0127,
	0130,0131,0132,0133,0134,0135,0136,0137,
	0140,0141,0142,0143,0144,0145,0146,0147,
	0150,0151,0152,0153,0154,0155,0156,0157,
	0160,0161,0162,0163,0164,0165,0166,0167,
	0170,0171,0172,0173,0174,0175,0176,0177,
	0200,0201,0202,0203,0204,0205,0206,0207,
	0210,0211,0212,0213,0214,0215,0216,0217,
	0220,0221,0222,0223,0224,0225,0226,0227,
	0230,0231,0232,0233,0234,0235,0236,0237,
	0240,0241,0242,0243,0244,0245,0246,0247,
	0250,0251,0252,0253,0254,0255,0256,0257,
	0260,0261,0262,0263,0264,0265,0266,0267,
	0270,0271,0272,0273,0274,0275,0276,0277,
	0300,0301,0302,0303,0304,0305,0306,0307,
	0310,0311,0312,0313,0314,0315,0316,0317,
	0320,0321,0322,0323,0324,0325,0326,0327,
	0330,0331,0332,0333,0334,0335,0336,0337,
	0340,0341,0342,0343,0344,0345,0346,0347,
	0350,0351,0352,0353,0354,0355,0356,0357,
	0360,0361,0362,0363,0364,0365,0366,0367,
	0370,0371,0372,0373,0374,0375,0376,0377
};

unsigned char	nonprint[256]; 

unsigned char	dict[256]; 

struct	field {
	unsigned char *code;
	unsigned char *ignore;
	int fcmp;
	int rflg;
	int bflg[2];
	int m[2];
	int n[2];
	int kflg;
}	*fields;
struct field proto = {
	nofold,
	zero,
	ASC,
	1,
	0,0,
	0,-1,
	0,0
};
int	nfields;
int 	error = 1;
char	*setfil();

char	*months[12] = {
	"   ", "   ", "   ", "   ", "   ", "   ",
	"   ", "   ", "   ", "   ", "   ", "   "};

struct	tm	ct = {
	0, 0, 1, 0, 86, 0, 0};

static const char USE[] = ":8:Incorrect usage\n";
static const char nonl[] = ":566:Missing NEWLINE added at end of file\n";
static const char badcreate[] = ":148:Cannot create %s: %s\n";
static const char badchmod[] = ":840:Cannot change mode for file %s: %s\n";

unsigned long physmem_size();

eucwidth_t	wp;
int	tbcw;

#define MERGE	0
#define SORT	1

main(argc, argv)
char **argv;
{
    register a;
    char *arg;
    struct field *p, *q;
    int i, nf;
    long ulimit();
    char *sbrk();
    int index, flg=0;
    
    setlocale(LC_ALL, "");

    /* Set the comparision function based on the locale */
    compare = ( __libc_attr._collate_res._coll_as_cmp ? cmpa : cmp_coll );

    (void)setcat("uxcore.abi");
    (void)setlabel("UX:sort");

    getwidth(&wp);

    /* close any file descriptors that may have been */
    /* left open -- we may need them all		*/
    for (i = 3; i < getdtablehi(); i++)
      (void) close(i);
    
    physmemmb = physmem_size();
    maxmem = (physmemmb / 8) * (1024*1024);

    fields = (struct field *)malloc(NF*sizeof(struct field));
    nf = NF;
    copyproto();
    initree();
    eargv = argv;
    tryfor = DEFMEM;
    while (--argc > 0) {
	if(**++argv == '-'){
	    if(argv[0][1] == '-')
	      break;
	    arg = *argv;
more:	    switch(*++arg) {
	      case '\0':
		if(arg[-1] == '-')
		  eargv[eargc++] = "-";
		break;
		
	      case 'o':
		if (*(arg+1) != '\0')
		  outfil = arg + 1;
		else
		  if(--argc > 0)
		    outfil = *++argv;
		  else {
		      pfmt(stderr, MM_ERROR,
			   ":567:Cannot identify output file\n");
		      exit(1);
		  }
		break;
		
	      case 'T':
		if (*(arg+1) != '\0') 
                        dirtry[0] = arg +1;
		else {
		  if (--argc > 0) {
		    if ((strlen(*++argv) + NAMEOHD) > sizeof(file1)) {
			pfmt(stderr, MM_ERROR, ":568:Path name too long: %s\n",
			     *argv);
			exit(1);
		    }
		    else dirtry[0] = *argv;
		  }
		}
		break;
		
	      case 'y':
		if (*++arg)
		  if(isdigit(*arg)){
		      /* SGI: if they specify a value
		       * set maximum to that
		       */
		      tryfor = number(&arg) * 1024;
		      if (tryfor > MAXMEM)
			tryfor = MAXMEM;
		      if(tryfor > maxmem)
			maxmem = tryfor;
		  } else {
		      pfmt(stderr, MM_ERROR, USE);
		      exit(1);
		  }
		else{
		    --arg;
		    maxmem = (physmemmb / 2) * (1024*1024);
		    tryfor = maxmem;
		}
		break;

	      case 't':
		if(*++arg == '\0' && argc < 2){
		    pfmt(stderr, MM_ERROR, ":1021:need field separator\n");
		    exit(1);
		}
		if(*arg == '\0'){
		    argc--;
		    arg = *++argv;
		}
		if (ISASCII(*arg)) {
		    tabchar = (unsigned char *)arg;
		    if(*tabchar == 0) arg--;
		    tbcw = 1;
		} else {
		    if (ISSET2(*arg))
		      tbcw = wp._eucw2 + 1;
		    else if (ISSET3(*arg))
		      tbcw = wp._eucw3 + 1;
		    else
		      tbcw = wp._eucw1;
		    tabchar = (unsigned char *)arg;
		    arg += (tbcw - 1);
		}
		break;
		
	      case 'z':
		if ( *++arg && isdigit(*arg))
		  maxrec = number(&arg);
		else if(argc < 2 || !isdigit(**++argv)){
		    pfmt(stderr, MM_ERROR, USE);
		    exit(1);
		}
		else{
		    argc--;
		    maxrec = number(argv);
		}
		break;
		
	      case 'k':
		flg=1;
		if(++nfields >= nf){
		    if((fields = (struct field *)
			realloc(fields, (nf + NF) * sizeof(struct field))) == NULL) {
			pfmt(stderr, MM_ERROR, ":569:Too many keys\n");
			exit(1);
		    }
		    nf += NF;
		}
		copyproto();
		if(*++arg == '\0'){
		    if(argc < 2 || !isdigit(**++argv)){
			pfmt(stderr, MM_ERROR, ":1020:Need key definition\n");
			exit(3);
		    }
		    else{
			argc--;
			key_index(*argv);
		    }
		}
		else{
		    key_index(arg);
		}
		break;

	      default:
		if ((index = field(++*argv, nfields>0, 0)) == 0)
		  break;
		else {
			arg = *argv + index - 1;
			goto more;
		}
	    }
	} else if (**argv == '+') {
	    if(++nfields >= nf) {
		if((fields = (struct field *)
		    realloc(fields, (nf + NF) * sizeof(struct field))) == NULL) {
		    pfmt(stderr, MM_ERROR, ":569:Too many keys\n");
		    exit(1);
		}
		nf += NF;
	    }
	    copyproto();
	    field(++*argv, 0, 0);
	} else
	  eargv[eargc++] = *argv;
    }
	q = &fields[0];
	for(a=1; a<=nfields; a++) {
		p = &fields[a];
		if(p->code != proto.code) continue;
		if(p->ignore != proto.ignore) continue;
		if(p->fcmp != proto.fcmp) continue;
		if(p->rflg != proto.rflg) continue;
		if(p->bflg[0] != proto.bflg[0]) continue;
		if(p->bflg[1] != proto.bflg[1]) continue;
		p->code = q->code;
		p->ignore = q->ignore;
		p->fcmp = q->fcmp;
		p->rflg = q->rflg;
		if ( flg )  /* only copy -b to bflg[1] when it's -k */
		  p->bflg[0] = p->bflg[1] = q->bflg[0];
		else
		  p->bflg[0] = q->bflg[0];
	}
	if(eargc == 0)
		eargv[eargc++] = "-";
	if(cflg && eargc>1) {
		pfmt(stderr, MM_ERROR, ":570:Can check only 1 file\n");
		exit(1);
	}

	safeoutfil();

	lspace = (int *) sbrk(0);
	maxbrk = (int *) ulimit(3,0L);
	if (!mflg && !cflg)
		if ((alloc=grow_core(tryfor,(unsigned) 0)) == 0) {
			pfmt(stderr, MM_ERROR, ":571:Out of memory before sort: %s\n",
				strerror(errno));
			exit(1);
		}

	a = -1;
	for(dirs=dirtry; *dirs; dirs++) {
		(void) sprintf(filep=file1, "%s/stm%.5uaa", *dirs, getpid());
		while (*filep)
			filep++;
		filep -= 2;
		if ( (a=creat(file, 0600)) >=0)
			break;
	}
	if(a < 0) {
		pfmt(stderr, MM_ERROR, ":572:Cannot locate temporary file: %s\n",
			strerror(errno));
		exit(1);
	}
	(void) close(a);
	(void) unlink(file);
	if (sigset(SIGHUP, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGHUP, term);
	if (sigset(SIGINT, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGINT, term);
	(void) sigset(SIGPIPE, term);
	if (sigset(SIGTERM, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGTERM, term);
	nfiles = eargc;
	if(!mflg && !cflg) {
		sort();
		if (ferror(stdin))
			rderror(NULL);
		(void) fclose(stdin);
	}

	if (maxrec == 0)  maxrec = 1024;
	alloc = (N + 1) * maxrec + N * BUFSIZ;
	for (nway = N; nway >= 2; --nway) {
		if (alloc < (maxbrk - lspace) * sizeof(int *))
			break;
		alloc -= maxrec + BUFSIZ;
	}
	if (nway < 2 || brk((char *)lspace + alloc) != 0) {
		pfmt(stderr, MM_ERROR, ":573:Out of memory before merge: %s\n",
			strerror(errno));
		term();
	}

	if (cflg)   checksort();

	wasfirst = notfirst = 0;
	a = mflg || cflg ? 0 : eargc;
	if ((i = nfiles - a) > nway) {	/* Do leftovers early */
		if ((i %= (nway - 1)) == 0)
			i = nway - 1;
		if (i != 1)  {
			newfile();
			setbuf(os, bufout);
			merge(a, a+i);
			a += i;
		}
	}
	for(; a+nway<nfiles || unsafeout&&a<eargc; a=i) {
		i = a+nway;
		if(i>=nfiles)
			i = nfiles;
		newfile();
		setbuf(os, bufout);
		merge(a, i);
	}
	if(a != nfiles) {
		oldfile();
		setbuf(os, bufout);
		merge(a, nfiles);
	}
	error = 0;
	term();
	/*NOTREACHED*/
}

/* This routine will change the -k option to +pos, -pos format */
void key_index(original)
char *original;
{
    char *field1 = NULL, *field2 = NULL, *character = NULL, *option;
    char *start = NULL, *end = NULL;
    char *final;
    char *temp;
    char *dot = NULL, *temp2 = NULL;

    if ((final = (char *)malloc((strlen(original) + 1) * sizeof(char))) 
	== NULL){
	pfmt(stderr, MM_ERROR, ":571:Out of memory before sort: %s\n", 
	     strerror(errno));
	exit(3);
    }

    if ((temp = (char *)malloc((strlen(original) + 1) * sizeof(char))) 
	== NULL){
	pfmt(stderr, MM_ERROR, ":571:Out of memory before sort: %s\n", 
	     strerror(errno));
	exit(3);
    }

    if ((option = (char *)malloc((strlen(original) + 1) * sizeof(char))) 
	== NULL){
	pfmt(stderr, MM_ERROR, ":571:Out of memory before sort: %s\n", 
	     strerror(errno));
	exit(3);
    }

    start = strtok(original, ",");
    end = strtok(NULL, " ");

    dot = strchr(start, '.');
    if (temp2 = strpbrk(start, "bdfinr"))
      strcpy(option, temp2);
    field1 = strtok(start, ".bdfinr");
    sprintf(final, "%d", atoi(field1) - 1);
    if(dot) {
	character = strtok(NULL, "bdfinr");
	if (character == NULL) {
		pfmt(stderr, MM_ERROR, USE);
		exit(1);
	}
	if (*character != '0') {
	    sprintf(temp, ".%d", atoi(character) - 1);
	    strcat(final, temp);
	    if(temp2)
	      strcat(final, option);
	} else if (temp2)
	  strcat(final, option);
    } else if (temp2)
      strcat(final, option);

    field(final, 0, 1);
    
    if (end) {
	dot = strchr(end, '.');
	if (temp2 = strpbrk(end, "bdfinr"))
	  strcpy(option, temp2);
	field2 = strtok(end, ".bdfinr");
	sprintf(final, "%d", atoi(field2) - 1);
	if(dot) {
	    character = strtok(NULL, "bdfinr");
	    if (character == NULL) {
		pfmt(stderr, MM_ERROR, USE);
		exit(1);
	    }
	    if (*character != '0') {
		sprintf(temp, ".%d", atoi(character) - 1);
		strcat(final, temp);
		if (temp2)
		  strcat(final, option);
	    } else if(temp2)
	      strcat(final, option);
	} else if(temp2)
	  strcat(final, option);
	
	field(final, 1, 1);
    }
}    

void sort()
{
	register char *cp;
	register char **lp;
	FILE *iop;
	char *keep, *ekeep, **mp, **lmp, **ep;
	int n;
	int done, i, first;
	char *f;

	/*
	** Records are read in from the front of the buffer area.
	** Pointers to the records are allocated from the back of the buffer.
	** If a partially read record exhausts the buffer, it is saved and
	** then copied to the start of the buffer for processing with the
	** next coreload.
	*/
	first = 1;
	done = 0;
	keep = 0;
	i = 0;
	ep = (char **) (((char *) lspace) + alloc);
	if ((f=setfil(i++)) == NULL) /* open first file */
		iop = stdin;
	else if ((iop=fopen(f,"r")) == NULL)
		cant(f);
	setbuf(iop,bufin);
	do {
		lp = ep - 1;
		cp = (char *) lspace;
		*lp-- = cp;
		if (keep != 0) /* move record from previous coreload */
			for(; keep < ekeep; *cp++ = *keep++);
#ifndef pdp11
		while ((char *)lp - cp > 1) {
			if (fgets(cp,(char *) lp - cp, iop) == NULL)
#else
		while ((char *)lp > cp + 1 ) {
			n = (char *)lp -cp;
			if(n < 0)
				n = MAXINT;
			if (fgets(cp,n,iop) == NULL)
#endif
				n = 0;
			else
#ifdef	sgi
				if (iflag)
				    /*
				     * fgets() reads null, but
				     * strlen(), cmp() stop there
				     */
				    n = compactlen(cp, (char*)lp);
				else {
				    /* null detector */
				    register char *tcp;
				    n = strlen(cp);
				    tcp = cp + n;
				    /* if not at end of this coreload && ... ;
				     * if we saw feof, then this is a
				     * file whose last line doesn't end in a
				     * newline, so we simply fall through to the
				     * code below that adds one. */
				    if (tcp!=(char*)lp-1 && *(tcp-1)!='\n' && !feof(iop)) {
					pfmt(stderr, MM_ERROR, "uxsgicore:1:NULL discovered in input (use -i option?)\n");
					term();
				    }
				}
#else
				n = strlen(cp);
#endif
			if (n == 0) {
				if (ferror(iop))
					rderror(f);

				if (keep != 0 )
					/* The kept record was at
					   the EOF.  Let the code
					   below handle it.       */;
				else
				if (i < eargc) {
					(void) fclose(iop);
					if ((f=setfil(i++)) == NULL)
						iop = stdin;
					else if ((iop=fopen(f,"r")) == NULL )
						cant(f);
					setbuf(iop,bufin);
					continue;
				}
				else {
					done++;
					break;
				}
			}
			cp += n - 1;
			if ( *cp == '\n') {
				cp += 2;
				if ( cp - *(lp+1) > maxrec )
					maxrec = cp - *(lp+1);
				*lp-- = cp;
				keep = 0;
			}
			else
			if ( cp + 2 < (char *) lp ) {
				/* the last record of the input */
				/* file is missing a NEWLINE    */
				if(f == NULL)
					pfmt(stderr, MM_WARNING, nonl);
				else
					pfmt(stderr, MM_WARNING, ":574:Missing NEWLINE added at end of input file %s\n", f);
				*++cp = '\n';
				*++cp = '\0';
				*lp-- = ++cp;
				keep = 0;
			}
			else {  /* the buffer is full */
				keep = *(lp+1);
				ekeep = ++cp;
			}

#ifdef pdp11
			if ((char *)lp <= cp + 2 && first == 1) {
#else
			if ((char *)lp - cp <= 2 && first == 1) {
#endif
				/* full buffer */
				tryfor = alloc;
				tryfor = grow_core(tryfor,alloc);
				if (tryfor == 0)
					/* could not grow */
					first = 0;
				else { /* move pointers */
					lmp = ep + 
					   (tryfor/sizeof(char **) - 1);
					for ( mp = ep - 1; mp > lp;)
						*lmp-- = *mp--;
					ep += tryfor/sizeof(char **);
					lp += tryfor/sizeof(char **);
					alloc += tryfor;
				}
			}
		}
		if (keep != 0 && *(lp+1) == (char *) lspace) {
			pfmt(stderr, MM_ERROR, ":575:Record too large\n");
			term();
		}
		first = 0;
		lp += 2;
		if(done == 0 || nfiles != eargc)
			newfile();
		else
			oldfile();
		setbuf(os, bufout);
		msort(lp, ep);
		if (ferror(os))
			wterror(SORT);
		(void) fclose(os);
	} while(done == 0);
}

#ifdef	sgi
/*
 * 'first' points to an 'fgets'ed buffer which is an array of characters
 * terminated by the 2 characters \n\0 OR one which is terminated by a \0
 * at 'last'.  fgets() does not report the number of chars which makes it
 * a minor trick to manipulate this buffer if there are nulls in it.
 *
 *      1) pull together any ranges separated by one or more '\0's
 *      2) return the length of the resulting string
 *	3) don't be too much worse in time than strlen for the normal case
 */
compactlen(first, last)
register char *first, *last;
{
        register char *cp;
        register char *ocp = 0;
        register int count = 0;

        for ( cp = first; *cp != '\n' && cp < last; ) {
                if ( *cp == '\0' ) {
                        if ( ocp == 0 )
                                ocp = cp;
                        while ( *cp == '\0' && cp < last )
                                cp++;
                        while ( *cp != '\0' && cp < last ) {
                                *ocp = *cp++;
                                count++;
				if ( *ocp++ == '\n' ) {
					*ocp = '\0';
					return count;
				}
                        }
                } else {
                        cp++;
                        count++;
                }
        }
        if ( *cp == '\n' )
                ++count;
        return count;
}
#endif

void msort(a, b)
char **a, **b;
{
	register struct btree **tp;
	register int i, j, n;
	char *save;

	i = (b - a);
	if (i < 1)
		return;
	else if (i == 1) {
		wline(*a);
		return;
	}
	else if (i >= TREEZ)
		n = TREEZ; /* number of blocks of records */
	else n = i;

	/* break into n sorted subgroups of approximately equal size */
	tp = &(treep[0]);
	j = 0;
	do {
		(*tp++)->rn = j;
		b = a + (blkcnt[j] = i / n);
		qksort(a, b);
		blkcur[j] = a = b;
		i -= blkcnt[j++];
	} while (--n > 0);
	n = j;

	/* make a sorted binary tree using the first record in each group */
	for (i = 0; i < n;) {
		(*--tp)->rp = *(--blkcur[--j]);
		insert(tp, ++i);
	}
	wasfirst = notfirst = 0;
	bonus = cmpsave(n);


	j = uflg;
	tp = &(treep[0]);
	while (n > 0)  {
		wline((*tp)->rp);
		if (j) save = (*tp)->rp;

		/* Get another record and insert.  Bypass repeats if uflg */

		do {
			i = (*tp)->rn;
			if (j) while((blkcnt[i] > 1) &&
					(**(blkcur[i]-1) == '\0')) {
				--blkcnt[i];
				--blkcur[i];
			}
			if (--blkcnt[i] > 0) {
				(*tp)->rp = *(--blkcur[i]);
				insert(tp, n);
			} else {
				if (--n <= 0) break;
				bonus = cmpsave(n);
				tp++;
			}
		} while (j && (*compare)((*tp)->rp, save) == 0);
	}
}


/* Insert the element at tp[0] into its proper place in the array of size n */
/* Pretty much Algorith B from 6.2.1 of Knuth, Sorting and Searching */
/* Special case for data that appears to be in correct order */

void insert(tp, n)
struct btree **tp;
int n;
{
    register struct btree **lop, **hip, **midp;
    register int c;
    struct btree *hold;

    midp = lop = tp;
    hip = lop++ + (n - 1);
    if ((wasfirst > notfirst) && (n > 2) &&
	((*compare)((*tp)->rp, (*lop)->rp) >= 0)) {
	wasfirst += bonus;
	return;
    }
    while ((c = hip - lop) >= 0) { /* leave midp at the one tp is in front of */
	midp = lop + c / 2;
	if ((c = (*compare)((*tp)->rp, (*midp)->rp)) == 0) break; /* match */
	if (c < 0) lop = ++midp;   /* c < 0 => tp > midp */
	else       hip = midp - 1; /* c > 0 => tp < midp */
    }
    c = midp - tp;
    if (--c > 0) { /* number of moves to get tp just before midp */
	hip = tp;
	lop = hip++;
	hold = *lop;
	do *lop++ = *hip++; while (--c > 0);
	*lop = hold;
	notfirst++;
    } else wasfirst += bonus;
}


void merge(a, b)
int a,b;
{
	FILE *tfile[N];
	char *buffer = (char *) lspace;
	register int nf;		/* number of merge files */
	register struct btree **tp;
	register int i, j;
	char	*f;
	char	*save, *iobuf;

	save = (char *) lspace + (nway * maxrec);
	iobuf = save + maxrec;
	tp = &(treep[0]);
	for (nf=0, i=a; i < b; i++)  {
		f = setfil(i);
		if (f == 0)
			tfile[nf] = stdin;
		else if ((tfile[nf] = fopen(f, "r")) == NULL)
			cant(f);
		(*tp)->rp = buffer + (nf * maxrec);
		(*tp)->rn = nf;
		setbuf(tfile[nf], iobuf);
		iobuf += BUFSIZ;
		if (rline(tfile[nf], (*tp)->rp)==0) {
			nf++;
			tp++;
		} else {
			if(ferror(tfile[nf]))
				rderror(f);
			(void) fclose(tfile[nf]);
		}
	}


	/* make a sorted btree from the first record of each file */
	for (--tp, i = 1; i++ < nf;) insert(--tp, i);

	bonus = cmpsave(nf);
	tp = &(treep[0]);
	j = uflg;
	while (nf > 0) {
		wline((*tp)->rp);
		if (j) cline(save, (*tp)->rp);

		/* Get another record and insert.  Bypass repeats if uflg */

		do {
			i = (*tp)->rn;
			if (rline(tfile[i], (*tp)->rp)) {
				if (ferror(tfile[i]))
					rderror(setfil(i+a));
				(void) fclose(tfile[i]);
				if (--nf <= 0) break;
				++tp;
				bonus = cmpsave(nf);
			} else insert(tp, nf);
		} while (j && (*compare)((*tp)->rp, save) == 0 );
	}


	for (i=a; i < b; i++) {
		if (i >= eargc)
			(void) unlink(setfil(i));
	}
	if (ferror(os))
		wterror(MERGE);
	(void) fclose(os);
}

void cline(tp, fp)
register char *tp, *fp;
{
	while ((*tp++ = *fp++) != '\n');
}

rline(iop, s)
register FILE *iop;
register char *s;
{
	register int n;

	if (fgets(s,maxrec,iop) == NULL )
		n = 0;
	else
		n = strlen(s);
	if ( n == 0 )
		return(1);
	s += n - 1;
	if ( *s == '\n' )
		return(0);
	else
	if ( n < maxrec - 1) {
		pfmt(stderr, MM_WARNING, nonl);
		*++s = '\n';
		return(0);
	}
	else {
		pfmt(stderr, MM_ERROR, ":469:Line too long\n");
		term();
		/*NOTREACHED*/
	}
	return 0;
}

void wline(s)
char *s;
{
	(void) fputs(s,os);
	if(ferror(os))
		wterror(SORT);

}

void checksort()
{
	char *f;
	char *lines[2];
	register int i, j, r;
	register char **s;
	register FILE *iop;

	s = &(lines[0]);
	f = setfil(0);
	if (f == 0)
		iop = stdin;
	else if ((iop = fopen(f, "r")) == NULL)
		cant(f);
	setbuf(iop, bufin);

	i = 0;   j = 1;
	s[0] = (char *) lspace;
	s[1] = s[0] + maxrec;
	if ( rline(iop, s[0]) ) {
		if (ferror(iop)) {
			rderror(f);
		}
		(void) fclose(iop);
		exit(0);
	}
	while ( !rline(iop, s[j]) )  {
		r = (*compare)(s[i], s[j]);
		if (r < 0)
			disorder(":576:disorder: %s\n", s[j]);
		if (r == 0 && uflg)
			disorder(":577:non-unique: %s\n", s[j]);
		r = i;  i = j; j = r;
	}
	if (ferror(iop))
		rderror(f);
	(void) fclose(iop);
	exit(0);
}


void disorder(s, t)
char *s, *t;
{
	register char *u;
	for(u=t; *u!='\n';u++) ;
	*u = 0;
	pfmt(stderr, MM_INFO, s, t, 0);
	term();
}

void newfile()
{
	register char *f;

	f = setfil(nfiles);
	if((os=fopen(f, "w")) == NULL) {
		pfmt(stderr, MM_ERROR, badcreate, f, strerror(errno));
		term();
	}
	if (chmod(f, S_IRUSR|S_IWUSR) == -1) {
		pfmt(stderr, MM_ERROR, badchmod, f, strerror(errno));
		term();
	}
	nfiles++;
}

char *
setfil(i)
register int i;
{
	if(i < eargc)
		if(eargv[i][0] == '-' && eargv[i][1] == '\0')
			return(0);
		else
			return(eargv[i]);
	i -= eargc;
	filep[0] = i/26 + 'a';
	filep[1] = i%26 + 'a';
	return(file);
}

void oldfile()
{
	if(outfil) {
		if((os=fopen(outfil, "w")) == NULL) {
			pfmt(stderr, MM_ERROR, badcreate, outfil, strerror(errno));
			term();
		}
	} else
		os = stdout;
}

void safeoutfil()
{
	register int i;
	struct stat64 ostat, istat;

	if(!mflg||outfil==0)
		return;
	if(stat64(outfil, &ostat)==-1)
		return;
	if ((i = eargc - N) < 0) i = 0;	/*-N is suff., not nec. */
	for (; i < eargc; i++) {
		if(stat64(eargv[i], &istat)==-1)
			continue;
		if(ostat.st_dev==istat.st_dev&&
		   ostat.st_ino==istat.st_ino)
			unsafeout++;
	}
}

void cant(f)
char *f;
{
	pfmt(stderr, MM_ERROR, ":4:Cannot open %s: %s\n", f, strerror(errno));
	term();
}

void
term()
{
	register i;

	if(nfiles == eargc)
		nfiles++;
	for(i=eargc; i<=nfiles; i++) {	/*<= in case of interrupt*/
		(void) unlink(setfil(i));	/*with nfiles not updated*/
	}
	exit(error);
}

cmp(i, j)
char *i, *j;
{
	register unsigned char *pa, *pb;
	register unsigned char *ignore;
	register int sa;
	int sb;
	unsigned char *skip();
	unsigned char *code;
	int a, b;
	int k;
	unsigned char *la, *lb;
	unsigned char *ipa, *ipb, *jpa, *jpb;
	struct field *fp;
	unsigned char ca[2], cb[2];

	for(k = nfields>0; k<=nfields; k++) {
		fp = &fields[k];
		pa = (unsigned char *)i;
		pb = (unsigned char *)j;
		if(k) {
			la = skip(pa, fp, 1);
			pa = skip(pa, fp, 0);
			lb = skip(pb, fp, 1);
			pb = skip(pb, fp, 0);
		} else {
			la = eol(pa);
			lb = eol(pb);
		}

		/*
		 * ignore leading blanks for NUMeric and MONth
		 * sorting. Arguably, one should ignore leading
		 * blanks anytime fp->bflg[0] is set but this
		 * changes the arcane sysV behaviour of leading
		 * blanks be signifacantif no fields were specified
		 * even if -b was given
		 * NOTE that this does change the behaviour
		 * that blanks are significant for numeric compares
		 * which is in violation of what the man page/BSD says
		 * but stock ATT
		 */
		if (fp->fcmp == NUM || fp->fcmp == MON) {
			while (blank(*pa))
				pa++;
			while (blank(*pb))
				pb++;
		}

		if(fp->fcmp==NUM) {
			sa = sb = fp->rflg;
			if(*pa == '-') {
				pa++;
				sa = -sa;
			}
			if(*pb == '-') {
				pb++;
				sb = -sb;
			}
			for(ipa = pa; ipa<la&&isdigit(*ipa); ipa++) ;
			for(ipb = pb; ipb<lb&&isdigit(*ipb); ipb++) ;
			jpa = ipa;
			jpb = ipb;
			a = 0;
			if(sa==sb)
				while(ipa > pa && ipb > pb)
					if(b = *--ipb - *--ipa)
						a = b;
			while(ipa > pa)
				if(*--ipa != '0')
					return(-sa);
			while(ipb > pb)
				if(*--ipb != '0')
					return(sb);
			if(a) return(a*sa);
			if(*(pa=jpa) == '.')
				pa++;
			if(*(pb=jpb) == '.')
				pb++;
			if(sa==sb)
				while(pa<la && isdigit(*pa)
				   && pb<lb && isdigit(*pb))
					if(a = *pb++ - *pa++)
						return(a*sa);
			while(pa<la && isdigit(*pa))
				if(*pa++ != '0')
					return(-sa);
			while(pb<lb && isdigit(*pb))
				if(*pb++ != '0')
					return(sb);
			continue;
		} else if(fp->fcmp==MON)  {
			sa = fp->rflg*(month(pb)-month(pa));
			if(sa)
				return(sa);
			else
				continue;
		}
		code = fp->code;
		ignore = fp->ignore;
loop: 
		while(*pa && ignore[*pa])
			pa++;
		while(*pb && ignore[*pb])
			pb++;
		if(pa>=la || *pa=='\n')
			if(pb<lb && *pb!='\n')
				return(fp->rflg);
			else continue;
		if(pb>=lb || *pb=='\n')
			return(-fp->rflg);

		/* WARNING: it is incorrect to compare
		 * two keys character by character with strcoll (bug 556434).
		 * But, since we do it, strcoll() will always return 0
		 * if those two caracters are identical, so we can
		 * optimize it. */
		ca[0] = code[*pa++];
		cb[0] = code[*pb++];

		if ((sa = cb[0] - ca[0]) == 0)
		    goto loop;

		if ( __libc_attr._collate_res._coll_as_cmp )
		    return(sa*fp->rflg);

		ca[1] = cb[1] = '\0';

		if((sa = strcoll((const char*)cb, (const char*)ca )) == 0)
			goto loop;
		return(sa*fp->rflg);
	}
	if(uflg)
		return(0);
	return( __libc_attr._collate_res._coll_as_cmp ? cmpa(i, j) : cmp_coll(i,j) );
}

cmpa(pa, pb)
register char *pa, *pb;
{
	while((unsigned char)*pa == (unsigned char)*pb++)
		if((unsigned char)*pa++ == '\n')
			return(0);
	return(
		(unsigned char)*pa == '\n' ? fields[0].rflg:
		(unsigned char)*--pb == '\n' ?-fields[0].rflg:
		(unsigned char)*pb > (unsigned char)*pa   ? fields[0].rflg:
		-fields[0].rflg
	);
}

cmp_coll(pa, pb)
register char *pa, *pb;
{
	static unsigned char ca[2], cb[2];

	while((unsigned char)*pa == (unsigned char)*pb++)
		if((unsigned char)*pa++ == '\n')
			return(0);

	if ((unsigned char)*pa == '\n')
		return(fields[0].rflg);
	else if ((unsigned char)*--pb == '\n')
		return(-fields[0].rflg);
	else{
		ca[0] = *pa;
		cb[0] = *pb;
		ca[1] = cb[1] = '\0';
		if (strcoll((const char *)cb, (const char *)ca ) > 0 )
			return(fields[0].rflg);
		else
			return(-fields[0].rflg);
	}
}


unsigned char *
skip(p, fp, j)
struct field *fp;
register unsigned char *p;
{
	register i;
	register unsigned char *tbc;
	int contflag;
	int ew;
	int sw;
	int sswidth;
	int k;

	if( (i=fp->m[j]) < 0)
		return(eol(p));
	if ( j && fp->kflg )
		i++;
	if (tabchar && *tabchar) {
		while (--i >= 0) {
			do {
				contflag = 0;
				tbc = tabchar;
				if (ISASCII(*p)) {
					ew = 1;
				} else {
					if (ISSET2(*p))
						ew = wp._eucw2 + 1;
					else if (ISSET3(*p))
						ew = wp._eucw3 + 1;
					else
						ew = wp._eucw1;
				}
				if (*p != *tbc)
					if(*p != '\n') {
						p += ew;
						contflag = 1;
					} else goto ret;
				else {
					for (k=1; k < tbcw; k++)
						if (*++p != *++tbc) {
							contflag = 1;
							p += (ew - k);
							break;
						}
				}
			} while (contflag);
			if (i > 0 || j == 0)
				p++;
			if (j == 1)
				p -= (tbcw -1);
		}
	} else  {
		while (--i >= 0) {
			while(blank(*p))
				p++;
			while(!blank(*p))
				if(*p != '\n')
					p++;
				else goto ret;
		}
		if ( fp->kflg && blank(*p)) 
			p++;
	}
	if(fp->bflg[j]) {
		if (j == 1 && fp->m[j] > 0)
			p++;
		while(blank(*p))
			p++;
	}
	i = fp->n[j];
	while((i > 0) && (*p != '\n')) {
		if (ISASCII(*p)) {
			p++;
			i--;
		} else {
			if (ISSET2(*p)) {
				ew = wp._eucw2;
				sw = wp._scrw2;
				sswidth = 1;
			} else if (ISSET3(*p)) {
				ew = wp._eucw3;
				sw = wp._scrw3;
				sswidth = 1;
			} else {
				ew = wp._eucw1;
				sw = wp._scrw1;
				sswidth = 0;
			}
			if ((i - sw) >= 0) {
				p += (ew + sswidth);
				i -= sw;
			} else if (ew == sw) {
				p += (i + sswidth);
				break;
			} else {
				while(*p != '\n')
					p++;
				break;
			}
		}
	}
ret:
	return(p);
}

unsigned char *
eol(p)
register unsigned char *p;
{
	while(*p != '\n') p++;
	return(p);
}

void copyproto()
{
	register i;
	register int *p, *q;

	p = (int *)&proto;
	q = (int *)&fields[nfields];
	for(i=0; i<sizeof(proto)/sizeof(*p); i++)
		*q++ = *p++;
}

void initree()
{
	register struct btree **tpp, *tp;
	register int i;

	for (tp = &(tree[0]), tpp = &(treep[0]), i = TREEZ; --i >= 0;)
	    *tpp++ = tp++;
}

cmpsave(n)
register int n;
{
	register int award;

	if (n < 2) return (0);
	for (n++, award = 0; (n >>= 1) > 0; award++);
	return (award);
}

field(s, k, flg)
char *s;
int k;
int flg;
{
	register struct field *p;
	register d;
	register i;
	register index = 0;

	p = &fields[nfields];
	p->kflg=flg;
	d = 0;
	for(; *s!=0; s++, index++) {
		switch(*s) {
		      case '\0':
			return 0;

		      case 'b':
			p->bflg[k]++;
			break;

		      case 'c':
			cflg = 1;
			break;
		    
		      case 'm':
			mflg = 1;
			break;
		
		      case 'u':
			uflg = 1;
			break;

		      case 'd':
			for(i=0; i<256; i++)
			  dict[i] = (!isalnum(i));
			dict['\t'] = dict[' '] = dict['\n'] = 0;
			p->ignore = dict;
			break;

		      case 'f':
			for(i=0; i<SIZE; i++)
			  fold[i] = (islower(i)? _toupper(i) : i);
			p->code = fold;
			break;

		      case 'i':
			for(i=0; i<256; i++)
			  nonprint[i] =  (isprint(i)  == 0);
			p->ignore = nonprint;
#ifdef	sgi
			iflag++;
#endif
			break;

		      case 'M':
			for(i=0; i<SIZE; i++)
			  fold[i] = (islower(i)? _toupper(i) : i);
			month_init();
			p->fcmp = MON;
			p->bflg[0]++;
			break;
			
		      case 'n':
			p->fcmp = NUM;
			p->bflg[0]++;
			break;
			
		      case 'r':
			p->rflg = -1;
			continue;
			
		      case '.':
			if(p->m[k] == -1)	/* -m.n with m missing */
			  p->m[k] = 0;
			d = &fields[0].n[0]-&fields[0].m[0];
			if (*++s == '\0') {
				--s;
				p->m[k+d] = 0;
				continue;
			}
			
		      default:
			if (isdigit(*s))
			  p->m[k+d] = number(&s);
			else {
				/*pfmt(stderr, MM_ERROR, USE);
				exit(1);*/
				return index;
			}
		}
		compare = cmp;
	}

	/* never reach here. Just keep the compiler happy */
	return 0;
}

number(ppa)
register char **ppa;
{
	register int n;
	register char *pa;

	pa = *ppa;
	n = 0;
	while(isdigit(*pa)) {
		n = n*10 + *pa - '0';
		*ppa = pa++;
	}
	return(n);
}

#define qsexc(p,q) t= *p;*p= *q;*q=t
#define qstexc(p,q,r) t= *p;*p= *r;*r= *q;*q=t

void qksort(a, l)
char **a, **l;
{
	register char **i, **j;
	register char **lp, **hp;
	char **k;
	int c, delta;
	char *t;
	unsigned n;


start:
	if((n=l-a) <= 1)
		return;

	n /= 2;
	if (n >= MTHRESH) {
		lp = a + n;
		i = lp - 1;
		j = lp + 1;
		delta = 0;
		c = (*compare)(*lp, *i);
		if (c < 0) --delta;
		else if (c > 0) ++delta;
		c = (*compare)(*lp, *j);
		if (c < 0) --delta;
		else if (c > 0) ++delta;
		if ((delta /= 2) && (c = (*compare)(*i, *j)))
		    if (c > 0) n -= delta;
		    else       n += delta;
	}
	hp = lp = a+n;
	i = a;
	j = l-1;


	for(;;) {
		if(i < lp) {
			if((c = (*compare)(*i, *lp)) == 0) {
				--lp;
				qsexc(i, lp);
				continue;
			}
			if(c < 0) {
				++i;
				continue;
			}
		}

loop:
		if(j > hp) {
			if((c = (*compare)(*hp, *j)) == 0) {
				++hp;
				qsexc(hp, j);
				goto loop;
			}
			if(c > 0) {
				if(i == lp) {
					++hp;
					qstexc(i, hp, j);
					i = ++lp;
					goto loop;
				}
				qsexc(i, j);
				--j;
				++i;
				continue;
			}
			--j;
			goto loop;
		}


		if(i == lp) {
			if(uflg)
				for(k=lp; k<hp;) **k++ = '\0';
			if(lp-a >= l-hp) {
				qksort(hp+1, l);
				l = lp;
			} else {
				qksort(a, lp);
				a = hp+1;
			}
			goto start;
		}


		--lp;
		qstexc(j, lp, i);
		j = --hp;
	}
}


void month_init()
{
	char	time_buf[20];
	int	i, len;

	for(i=0; i<12; i++)
		{
		ct.tm_mon = i;
		ascftime(time_buf, "%b", &ct);
		if ((len = strlen(time_buf)) > 3)
			memcpy(months[i], time_buf, 3);
		else
			memcpy(months[i], time_buf, len+1);
		}
}


month(s)
char *s;
{
	register unsigned char *t, *u;
	register i;
	register unsigned char *f = fold ;

	for(i=0; i<12; i++) {
		for(t=(unsigned char *)s, u=(unsigned char *)months[i]; f[*t++]==f[*u++]; )
			if(*u==0)
				return(i);
	}
	return(-1);
}

void rderror(s)
char *s;
{
	if (s)
		pfmt(stderr, MM_ERROR, ":341:Read error in %s: %s\n", s,
			strerror(errno));
	else
		pfmt(stderr, MM_ERROR, ":578:Read error on stdin: %s\n",
			strerror(errno));
	term();
}

void wterror(s)
int s;
{
	switch(s){
	case SORT:
		pfmt(stderr, MM_ERROR, ":579:Write error while sorting: %s\n",
			strerror(errno));
		break;
	case MERGE:
		pfmt(stderr, MM_ERROR, ":580:Write error while merging: %s\n",
			strerror(errno));
		break;
	}
	term();
}

grow_core(size,cursize)
	unsigned size, cursize;
{
	unsigned newsize;
	/*The variable below and its associated code was written so this would work on */
	/*pdp11s.  It works on the vax & 3b20 also. */
	u_long longnewsize;

	longnewsize = (u_long) size + (u_long) cursize;
	if (longnewsize < MINMEM)
		longnewsize = MINMEM;
	else
	if (longnewsize > maxmem)
		longnewsize = maxmem;
	newsize = (unsigned) longnewsize;
	for (; ((char *)lspace+newsize) <= (char *)lspace; newsize >>= 1);
	if (longnewsize > (u_long) (maxbrk - lspace) * (u_long) sizeof(int *))
		newsize = (maxbrk - lspace) * sizeof(int *);
	if (newsize <= cursize)
		return(0);
	if ( brk((char *) lspace + newsize) != 0)
		return(0);
	return(newsize - cursize);
}


#include <invent.h>

unsigned long
physmem_size()
{
	inventory_t	*inv;

	while ( inv = getinvent() )
		if ( (inv->inv_class == INV_MEMORY) &&
		     (inv->inv_type == INV_MAIN_MB) ) {
			if (inv->inv_state > MAXMEMMB)
				return MAXMEMMB;
			else {
				if (inv->inv_state)
					return inv->inv_state;
				break;
			}
		}
	/* defensive programming */
	return 16;
}
