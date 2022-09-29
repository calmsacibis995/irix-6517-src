/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)grep:grep.c	1.22.1.3"
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
/*
 * grep -- print lines matching (or not matching) a pattern
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 *
 * Internationalization by
 *	frank@ceres.esd.sgi.com
 */

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <locale.h>
#include <fmtmsg.h>
#include <wctype.h>
#include <regex.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

int	nfile;
int	retval;

#define	RET_ERR	1
#define	SBSIZE		BUFSIZ
#define	LINESIZE	(SBSIZE * MB_LEN_MAX)
#define	LBSIZE	LINESIZE

char	linbuf[LINESIZE];		/* line read buffer */
char	pattern[LINESIZE];		/* pattern */
char	ibuf[LINESIZE];			/* input patterns */
char    tempbuf[LINESIZE];

int	nflag, bflag, lflag, cflag;
int	vflag, sflag, iflag, hflag;
int	Cflag, errflg;
int qflag, Eflag, Fflag, eflag, fflag, xflag;
int reflag;

int	cantopen = 0;

/* Multiple expression/file(-e/-f) argument list */
#define RE_TEXT 0
#define RE_FILE 1

struct  re_list {
    char *re_addr;      /* expression/file name  pointer */
	regex_t	*re_exp;	/* compiled RE */
    struct re_list *re_next ;
};

/* re_list header */
struct {
    struct re_list *re_head;
    struct re_list *re_tail;
} relst;

char	cmd_label[] = "UX:grep";

/* externals */

extern int	optind;
extern char	*optarg;

static void
#ifdef __STDC__
re_args(char *addr , int type);
#else
re_args();
#endif

static void
#ifdef __STDC__
add_re(char *addr);
#else
add_re(addr);
#endif

static void
#ifdef __STDC__
add_fre(char *addr);
#else
add_re(addr);
#endif

/*
 * some error prints
 */
static void
err_nomem()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_outofmem, "Out of memory"));
}

static void
err_char(s)
char *s;
{
	if(Cflag)
	    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		gettxt(_SGI_DMMX_grep_illchar, "Illegal character in: %s"), s);
}

static void
err_open(s)
char *s;
{
	_sgi_nl_error(SGINL_SYSERR, cmd_label,
	    gettxt(_SGI_DMMX_CannotOpen, "Cannot open %s"), s);
}

#define REG_MSGLEN  36
static void
regerr(re, re_err, preg)
char *re;
int	re_err;
regex_t	*preg;
{

	char *f;
	char msg[REG_MSGLEN];
	if( !re)
	    f = "(standard input)";
	else
		f = re;
	(void)regerror(re_err, preg, msg, REG_MSGLEN);

	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_regexp_str, "regular expression: %s"), f);
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label, "%s", msg);
	exit(2);
}

/*
 * print line according to flags
 * return 0 if continue
 * return 1 if stop
 */
static int
glist(f, fp, s, lnum)
char *f;
FILE *fp;
char *s;
long long lnum;
{

	if( !f)
	    f = "(standard input)";
	if(lflag) {
	    fprintf(stdout, "%s\n", f);
	    return(1);
	}
	if (nfile > 1 && !hflag)
	    fprintf(stdout, "%s:", f);			/* print filename */

	if(bflag)
#ifdef _XFS_OK
	    fprintf(stdout, "%lld:", ftell64(fp) / 512);	/* print block nbr */
#else
	    fprintf(stdout, "%ld:", ftell(fp) / 512);	/* print block nbr */
#endif

	if(nflag)
	    fprintf(stdout, "%lld:", lnum);		/* print line number */

	fprintf(stdout, "%s", s);			/* print line */
	return(0);
}

/*
 * execute one file
 */
static int
execute(file)
register char *file;
{
	register FILE *fp;
	register long long lnum = 0;
	long long tln = 0;
	regmatch_t  pmatch[1];
	int err_ret, match=0;
	struct re_list	*re_ptr;
	regex_t	*preg;
	char	*fptr;
	int illcflg = 0;
	int	lines = 0;
	int	re_flag = 0;
	
	if(file) {
	    if( !(fp = fopen(file, "r"))) {
			if( !sflag)
			    err_open(file);
			retval = RET_ERR;			/* error */
			cantopen++;
			goto prntln;			/* for -c option */
	    }
	} else
	    fp = stdin;

	fptr = (char *)fgets(linbuf, sizeof(linbuf), fp);
	if(!fptr)
		goto filelengthzero;
	do {
		if (fptr)
		    lnum++;
		else
			linbuf[0] = 0;

		re_ptr = relst.re_head;
		while (re_ptr) {
			if (!*re_ptr->re_addr) 	/* NULL string ? */
				err_ret = 0;	/* null string matches */
			else {
				preg = re_ptr->re_exp;
			    err_ret=regexec(preg, linbuf, (size_t)1, pmatch, re_flag);
			}

			/* Abort if there is an error */
			if (err_ret  && err_ret != REG_NOMATCH)
					regerr(file, err_ret, preg);

			if (err_ret == 0) {
				if (xflag) {	/* an exact match? */
					int     llen = strlen(linbuf);

					if (llen > 0 && linbuf[llen-1] == '\n')
						llen--;
					if ((pmatch[0].rm_so == 0) &&
					    (pmatch[0].rm_eo == llen))
						break;  /* match */
					else
						err_ret = REG_NOMATCH;
				} else {
					break;	/* Report once per line */
				}
			}
			re_ptr = re_ptr->re_next;
		}

		if ((err_ret == 0) ^ vflag) {
			match++;
			if (qflag)
				break;
			if(cflag)
			    tln++;		/* # of pattern lines */
			else if(glist(file, fp, linbuf, lnum))
			    break;		/* no continue */
		}
	} while (fptr = (char *)fgets(linbuf, sizeof(linbuf), fp));

filelengthzero:
	if(illcflg && !sflag) {
	    _sgi_ffmtmsg(stderr, 0, cmd_label, MM_INFO,
		gettxt(_SGI_DMMX_grep_illchar, "Illegal character(s) in: %s"),
		file);
	}
	(void)fclose(fp);
prntln:
	if(cflag) {
	    if((nfile > 1) && !hflag && file)
		fprintf(stdout, "%s:", file);
	    fprintf(stdout, "%lld\n", tln);
	}
	return (match?0:1);
}

/*
 * main entry
 */
int
main(argc, argv)
register argc;
char **argv;
{
	register int c;
	register int ep;
	int n, fmatch;
	char	*ptr;
	struct re_list *cur_re;
	regex_t	tpreg;	/* data structure for regex */
	char    *bstr;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	nflag = bflag = lflag = cflag =
	vflag = sflag = iflag = hflag =
	qflag = eflag = fflag = Eflag = xflag =
	Cflag = errflg = 0;
	reflag = 0;

	retval = 1;
	while((c=getopt(argc, argv, "qxhblcnsviyCEFe:f:")) != -1)
		switch(c) {
		case 'h':
			hflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'b':
			bflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'l':
			lflag++;
			break;
		case 'y':
		case 'i':
			iflag++;
			reflag |= REG_ICASE;
			break;
		case 'C' :
			Cflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 'e':
			eflag++;
			re_args(optarg, RE_TEXT);
			break;
		case 'f':
			fflag++;
			re_args(optarg, RE_FILE);
			break;
		case 'x':
			xflag++;
			break;
		case 'E' :
			Eflag++;
			reflag |= REG_EXTENDED;
			break;
		case 'F' :
			Fflag++;
			reflag |= REG_NOSPEC;
			break;
		case '?':
			errflg++;
			break;
		default :
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
			    gettxt(_SGI_DMMX_illoption, "illegal option -- %c"),
			    c);
		}

	if(errflg || ((optind >= argc) && !eflag && !fflag) ||
				((optind > argc) && (eflag || fflag)) ||
				(Eflag && Fflag)) {
	    _sgi_nl_usage(SGINL_USAGE, cmd_label,
		    gettxt(_SGI_DMMX_grep_usage,
			"grep [-E|-F][-c|-l|-q][-xhbnsviyC] -e pattern_list [-f pattern_file] [file ...]"));
	    exit(2);
	}


	if (!eflag && !fflag) {
		argv = &argv[optind];
		argc -= optind;

		re_args(argv[0], RE_TEXT);
	} else {
		argv = &argv[optind-1];
		argc -= optind - 1;
	}

	nfile = argc - 1;
	reflag |= REG_NEWLINE;

	do {
		++argv;				/* Point to the input file name */
		cur_re = relst.re_head;
	
		while (cur_re) {
			if (!cur_re->re_exp) {
				if (!*cur_re->re_addr) {
					cur_re = cur_re->re_next;
					continue;	/* null string is always matched */
				}

				ptr = strcpy(ibuf, cur_re->re_addr);
				(void)strcpy(pattern, ptr);

				if (!Fflag) {
					/* word boundary search */

					while(bstr = strstr(pattern, "\\<")) {
						char tmpbuf[LBSIZE];
						char    wbow[] = "[[:<:]]"; /* Map to regcomp() syntax */
						register int    i;

						strcpy(tmpbuf,pattern);
						i = bstr - pattern;
						strcpy(&tmpbuf[i], wbow);
						strcat(tmpbuf,&pattern[i+2]);
						strcpy(pattern,tmpbuf);
					}
					while(bstr = strstr(pattern, "\\>")) {
						char tmpbuf[LBSIZE];
						char	weow[] = "[[:>:]]";	/* Map to regcomp() syntax */
						register int    i;

						strcpy(tmpbuf,pattern);
						i = bstr - pattern;
						strcpy(&tmpbuf[i], weow);
						strcat(tmpbuf,&pattern[i+2]);
						strcpy(pattern,tmpbuf);
					}
				}

				ep = regcomp(&tpreg, pattern, reflag);
				if(ep)
				    regerr(*argv, ep, &tpreg);
				if ((cur_re->re_exp = (regex_t *)malloc(sizeof(regex_t))) == NULL) {
					err_nomem();
					exit(2);
				}
				memcpy(cur_re->re_exp,&tpreg,sizeof(regex_t));
			}
			cur_re = cur_re->re_next;
		}

		if(!nfile)
		    fmatch = execute((char *)NULL);
		else
			fmatch = execute(*argv);
		argc--;
		if (qflag && !fmatch)
			break;			/* A select input is enough */
		retval &= fmatch;
	} while (argc > 1);
	if(qflag)
		return(fmatch);
	else
		return(cantopen? 2: retval);
}

/*
 * function: re_args
 */

static void
#ifdef __STDC__
re_args(char *addr , int type)
#else
re_args(addr, type)
char *addr ;
int type ;
#endif
{
	char	*p = addr;
	char	*q;
	struct re_list *re ;
	register	int c, len;
	static FILE *fp;

	if (type == RE_TEXT) {
		if (!*p) {	/* Argument is null string ? */
			add_re(p);
			return;
		}

		while (p && *p) {
			add_re(p);

			/* -e ? */
			while ((c = (int)*p & 0xFF) != '\0') {
				if (c == '\n') {
					*p++ = '\0';
					break;
				}
				p++;
			}
		}
	} else { /* -f ? */
		if ((fp = fopen(addr, "r")) == NULL) {
			err_open(addr);
			exit(2) ;
		}

		do { 

			p = q = tempbuf;
			len = 0;
			while ((c = getc(fp)) != EOF) {
				if (c == '\n'|| len == (LINESIZE -1)) {
					*p = '\0';
					break ;
				}
				*p++ = c;
				len++;
			}
			if (c == EOF) {
				(void) fclose(fp);
				if (p != tempbuf) {
					add_fre(q);			
				}
			} else
				add_fre(q);			

		} while (c != EOF);
	}

	return ;
}

static void
#ifdef __STDC__
add_re(char *addr)
#else
add_re(addr)
#endif
{
	struct re_list *re ;

	if ((re = (struct re_list *)malloc(sizeof(struct re_list))) == NULL) {
		err_nomem();
		exit(2);
	}

	re->re_addr = addr;
	re->re_exp = NULL;
	re->re_next = NULL;

	if (relst.re_head == NULL)
		relst.re_head = re ;
	else
		relst.re_tail->re_next = re;
	relst.re_tail = re ;
	return;
} 

static void
#ifdef __STDC__
add_fre(char *addr)
#else
add_re(addr)
#endif
{
	char	*newre;

	if ((newre = (char *)malloc(strlen(addr) + 1)) == NULL) {
		err_nomem();
		exit(2);
	}
	(void)strcpy(newre, addr);
	add_re(newre);

	return;
} 
