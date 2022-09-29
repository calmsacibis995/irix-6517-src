/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 * @(#)last.c	5.3 (Berkeley) 5/15/86
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * last
 */

#define _BSD_SIGNALS

#include <sys/types.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <utmpx.h>
#include <bstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <locale.h>

#if 0
#define _XWTMP_FILE "./xwtmp"
#define WTMP_FILE "./wtmp"
#endif

#define NMAX	sizeof(((struct utmp *)0)->ut_name)
#define LMAX	sizeof(((struct utmp *)0)->ut_line)
#define HMAX	21
/* in wide format, print more name */
#define XNMAX	(NMAX+HMAX+1)
#define	SECDAY	(24*60*60)

#define MIN(a,b)	(a < b ? a : b)
#define	plineq(a)	(!strncmp(a,ut_line,MIN(LMAX, strlen(a))))
#define	lineq(a,b)	(!strncmp(a,b,LMAX))
#define	nameq(a,b)	(!strncmp(a,b,NMAX))

/*
 * BSD puts a ~ in line for special messages, but ATT doesn't.
 * So in want() we translate from ATT to BSD...
 */
#define BOOTMSG		"system boot"
#define RUNLEVEL	"run-level"
#define OTIME		"old time"
#define NTIME		"new time"

#define MAXTTYS 1000

char	**firstarg;
int	nameargs;
int	all = 0;			/* 1=show even LOGIN, getty, etc */
int want(char *ut_line, char *ut_name, int ut_type);

static	char	ttnames[MAXTTYS][LMAX+1];
static	long	logouts[MAXTTYS];

typedef struct {
    int fd;		/* descriptor of wtmp file */
    int tot;		/* initially, total number of entries in the file */
    int num;		/* current number of entries remaining to be
			 *  processed in the buffer */
    int block;		/* current index of block into file */
    size_t bufsize;	/* size in bytes of the wtmp array */ 
    size_t entsize;	/* size in bytes of an wtmp entry */
} fileinfo;
static	fileinfo xwi, wi;
static	struct utmpx xbuf[128];
static	struct utmp buf[128];

/*
 * -W option displays ut_host field untruncated on a separate line
 * (and suppresses the default truncated 'from' field), matching the
 * behavior of the -W opt to "w".
 */
int wide_from = 0;

char	*ctime(), *strspl();
void		onintr();
static void	last_usage(void);

/*
 * The file is treated as 1 or more blocks of the size of the buffer.
 * Since the records are increasing in time and "last" prints
 * in reverse chronological order, the last block is read first.
 * The last block may have less entries than a full block.
 * When the block is consumed, getmore() reads the previous block.
 */

static void
initfile(fileinfo *w, void *buffer)
{
	struct stat stb;

	if (fstat(w->fd, &stb) < 0) {
		perror("can't stat file");
		exit(1);
	}
#if 0
	if ((stb.st_size % w->entsize) != 0)
		fprintf(stderr, "nonintegral size for ?\n");
#endif
	w->tot = stb.st_size / w->entsize;

	w->block = (((w->tot * w->entsize) + w->bufsize - 1)/ w->bufsize) - 1;
	if (w->block < 1) {
		w->num = w->tot;
	} else {
		w->num = w->tot % (w->bufsize/w->entsize);
		if (w->num == 0)
			w->num = (w->bufsize/w->entsize);
		if (lseek(w->fd, w->block * w->bufsize, 0) < 0) {
			perror("lseek");
			exit(1);
		}
	}
	if (read(w->fd, buffer, (w->num * w->entsize)) < 0) {
		perror("read");
		exit(1);
	}
}

static int
getmore(fileinfo *i, void *buffer)
{
	if (i->tot <= 0 || --i->block < 0)
		return 0;
	if (lseek(i->fd, i->block * i->bufsize, 0) < 0) {
		perror("lseek");
		return 0;
	}
	if (read(i->fd, buffer, i->bufsize) < 0) {
		perror("read");
		return 0;
	}
	i->num = i->bufsize / i->entsize;
	return 1;
}

static struct utmp *
fetchboth(char **hostp)
{
	struct utmp *b;

	if (wi.num < 1 && !getmore(&wi, buf))
		return NULL;
	*hostp = NULL;
	b = &buf[--wi.num];
	wi.tot--;

	if (xwi.tot > 0) {
		while (1) {
			if (xwi.num < 1 && !getmore(&xwi, xbuf))
				return b;
			/*
			 * The xwtmp entries should be written in the same
			 * order as wtmp entries. (A race can happen between
			 * rlogind and ftpd, for example.) In case they do 
			 * get out of order, skip xwtmp entries that are 
			 * more recent than the current wtmp entry to try
			 * to get back in synch.
			 * e.g.,
			 *		--time-->
			 *  wtmp	123456
			 *  xwtmp	 A  B
			 * If files are ordered, 2 = A and 5 = B
			 * If 5 != B, when processing 4, this loop 
			 * will skip B.
			 */
			if (b->ut_time >= xbuf[xwi.num-1].ut_xtime)
				break;
			xwi.num--;
			xwi.tot--;
		}
		if (b->ut_time == xbuf[xwi.num-1].ut_xtime &&
		    !strncmp(b->ut_line, xbuf[xwi.num-1].ut_line,
			     MIN(sizeof(b->ut_line), sizeof(xbuf[xwi.num-1].ut_line))) &&
		    !strncmp(b->ut_user, xbuf[xwi.num-1].ut_user,
			     MIN(sizeof(b->ut_user), sizeof(xbuf[xwi.num-1].ut_user)))) {
			*hostp = xbuf[--xwi.num].ut_host;
			xwi.tot--;
		}
	}
	return b;
}

static struct utmpx *
fetchx(char **hostp)
{
	if (xwi.num < 1 && !getmore(&xwi, xbuf))
		return NULL;
	xwi.tot--;
	*hostp = xbuf[--xwi.num].ut_host;
	return &xbuf[xwi.num];
}

int
main(int argc, char **argv)
{
	char *wtmpfile = NULL;
	register int i, c;
	long otime;
	int print;
	int extended = 0;
	char *crmsg = NULL;
	long outrec = 0;
	long maxrec = LONG_MAX;
	char *ct, *hostp;
	int done = 0;
 
	setlocale(LC_ALL, "");
	while ((c = getopt(argc, argv, "f:xaW0123456789")) != EOF)
		switch (c) {
		case 'f':
			wtmpfile = optarg;
			break;
		case 'x':
			extended = 1;
			break;
		case 'a':
			all = 1;
			break;
		case 'W':
			wide_from = 1;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			/* this doesn't really follow the arg convention
			 * we assume that this one is always at the end
			 * of the option
			 */
			maxrec = atoi(argv[optind-1]+1);
			break;
		default:
			fprintf(stderr, "last:illegal option '%c':\n", c);
			last_usage();
			exit(1);
			/* NOTREACHED */
		}
	
	firstarg = &argv[optind];
	nameargs = 0;
	for ( ; optind < argc; optind++) {
		char *av = argv[optind];
		char *tc;
		/*
		 * nameargs - either user's or tty names - difficulty
		 * is to differentiate between tty names and user names..
		 * "Hello Ms q23..."
		 * So:
		 * 1) if it begins with 'tty' leave it alone
		 * 2) if has no numbers leave it alone
		 * 3) if getpwnam on it returns true leave it alone
		 * 4) else add a 'tty' to it.
		 */
		nameargs++;
		if (*av == 't' && *(av+1) == 't' && *(av+2) == 'y')
			continue;
		for (tc = av; *tc; tc++) {
			if (isdigit((int)*tc))
				break;
		}
		if (!*tc)
			continue; /* no digits */
		/* has some numbers - but could be a userid.. */
		if (getpwnam(av))
			continue;
		argv[optind] = strspl("tty", av);
	}

	xwi.bufsize = sizeof(xbuf);
	xwi.entsize = sizeof(xbuf[0]);
	wi.bufsize = sizeof(buf);
	wi.entsize = sizeof(buf[0]);

	if (extended) {
		if (wtmpfile == NULL) 
			wtmpfile = WTMPX_FILE;
		xwi.fd = open(wtmpfile, O_RDONLY);
		if (xwi.fd < 0) {
			perror(wtmpfile);
			exit(1);
		}
		time(&xbuf[0].ut_xtime);
		initfile(&xwi, xbuf);
	} else {
		if (wtmpfile == NULL) {
			xwi.fd = open(WTMPX_FILE, O_RDONLY);
			if (xwi.fd < 0) {
				if (errno != ENOENT) {
					perror(WTMPX_FILE);
					exit(1);
				}
			} else {
				time(&xbuf[0].ut_xtime);
				initfile(&xwi, xbuf);
			}
			wtmpfile = WTMP_FILE;
		}
		wi.fd = open(wtmpfile, O_RDONLY);
		if (wi.fd < 0) {
			perror(wtmpfile);
			exit(1);
		}
		time(&buf[0].ut_time);
		initfile(&wi, buf);
	}


	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGINT, onintr);
		(void) signal(SIGQUIT, onintr);
	}

	while (! done) {
		    static char *ut_line;
		    static char *ut_name;
		    static time_t ut_time;
		    static short ut_type;
		    if (extended) {
			struct utmpx *utx;
			if ((utx = fetchx(&hostp)) == NULL) {
				done = 1;
				break;
			}
			ut_line = utx->ut_line;
			ut_name = utx->ut_name;
			ut_time = utx->ut_xtime;
			ut_type = utx->ut_type;
		    } else {
			struct utmp *ut;
			if ((ut = fetchboth(&hostp)) == NULL) {
				done = 1;
				break;
			}
			ut_line = ut->ut_line;
			ut_name = ut->ut_name;
			ut_time = ut->ut_time;
			ut_type = ut->ut_type;
		    }
		    print = want(ut_line, ut_name, ut_type);
		    if (print) {
			ct = ctime(&ut_time);
			if (!wide_from) {	/* all fields on same line */
			    printf("%-*.*s %-*.*s %-*.*s %10.10s %5.5s ",
				NMAX, NMAX, ut_name,
				LMAX, LMAX, *ut_line == '!' ? "" : ut_line,
				HMAX, HMAX, hostp ? hostp : "",
				ct, 11+ct);
			} else {    /* remotehost on next line, untruncated */
			    printf("%-*.*s %-*.*s %10.10s %5.5s ",
				XNMAX, XNMAX, ut_name,
				LMAX, LMAX, *ut_line == '!' ? "" : ut_line,
				ct, 11+ct);
			}
		    }
		    for (i = 0; i < MAXTTYS; i++) {
			    if (ttnames[i][0] == 0) {
				    strncpy(ttnames[i], ut_line,
					sizeof(ttnames[0]));
				    otime = logouts[i];
				    logouts[i] = ut_time;
				    break;
			    }
			    if (lineq(ut_line, ttnames[i])) {
				    otime = logouts[i];
				    logouts[i] = ut_time;
				    break;
			    }
		    }
		    if (print) {
			    /*
			     * ATT init states have null names but non-null
			     * ut_lines
			     * uadmin is the only line that won't
			     * get cleaned up on a std reboot - we
			     * don't want to print a 'crash' for it..
			     */
			    if (lineq(ut_line, "~") ||
				lineq(ut_line, "!") ||
				nameq(ut_name, ""))
				    printf("\n");
			    else if (otime == 0)
				    printf("  still logged in\n");
			    else {
				    long delta;
				    if (otime < 0) {
					    otime = -otime;
					    printf("- %s", crmsg);
				    } else
					    printf("- %5.5s",
						ctime(&otime)+11);
				    delta = otime - ut_time;
				    if (delta < SECDAY)
					printf("  (%5.5s)\n",
					    asctime(gmtime(&delta))+11);
				    else
					printf(" (%ld+%5.5s)\n",
					    delta / SECDAY,
					    asctime(gmtime(&delta))+11);
			    }
			    if (wide_from && hostp && hostp[0])
				printf("%*.s %-s\n", XNMAX, " ", hostp);

			    fflush(stdout);
			    if (++outrec >= maxrec)
				    exit(0);
		    }

		    /*
		     * if run level was changed right before the reboot,
		     * the system was shutdown; else it had to be a crash.
		     */
		    if (ut_type == RUN_LVL) {
			    crmsg = "down ";
		    }

		    /*
		     * this is basically a boot message
		     * Everything except the final 'uadmin' entry should
		     * be cleaned up during a normal shutdown so we
		     * really never see the name 'shutdown' and always
		     * assume that if any entries (except uadmin) haven't
		     * been closed out that it was due to a crash...
		     */
		    if (lineq(ut_line, "~")) {
			    for (i = 0; i < MAXTTYS; i++)
				    logouts[i] = -ut_time;
			    if (nameq(ut_name, "shutdown"))
				    crmsg = "down ";
			    else
				    crmsg = "crash";
		    }
	}
	ct = ctime(extended ? &xbuf[0].ut_xtime : &buf[0].ut_time);
	printf("\n%swtmp begins %10.10s %5.5s \n", 
		extended ? "x" : "", ct, ct + 11);
	exit(0);
	/*NOTREACHED*/
}

void
onintr(signo)
	int signo;
{
	char *ct;

	if (signo == SIGQUIT)
		(void) signal(SIGQUIT, onintr);
	ct = ctime(&buf->ut_time);
	printf("\ninterrupted %10.10s %5.5s \n", ct, ct + 11);
	fflush(stdout);
	if (signo == SIGINT)
		exit(1);
}

int
want(char *ut_line, char *ut_name, int ut_type)
{
	register char **av;
	register int ac;

	/* convert special names to BSD style names */
	if (plineq(BOOTMSG) && ut_name[0] == '\0') {
		strcpy(ut_name, "reboot");
		ut_line[0] = '\0';
		strcpy(ut_line, "~");
	}
	if (all && ut_name[0] == '\0') {
		/* catch ATT init messages */
		if (plineq(RUNLEVEL) || plineq(NTIME) || plineq(OTIME))
			return 1;
		
	}
	if (ut_name[0] == 0)
		return (0);
	/* try to handle ATT entries that don't have ut_lines.. */
	if (*ut_line == '\0') {
		/*
		 * by copying into 'line' we can compute duration of these
		 * init-spawned events
		 * uugetty, getty are strange - they may or may not have 'lines'
		 * and they never put end records in (they change their
		 * name to LOGIN).
	         * uadmin is the only line that won't
	         * get cleaned up on a std reboot - we
	         * don't want to print a 'crash' for it..
		 */
		if (nameq(ut_name, "getty") ||
		    nameq(ut_name, "uadmin") ||
		    nameq(ut_name, "uugetty")) {
		    strcpy(ut_line, "!"); /* mark as don't print times */
		    return all; /* if we want all, return != 0 */
		}
		strncpy(ut_line, ut_name, LMAX);
		/* XXX assume NMAX < LMAX */
		ut_line[NMAX] = '\0';
		/* never want dead processes */
		if (ut_type == DEAD_PROCESS) {
			/* convention is that 'dead' entries have null names */
			*ut_name = '\0';
			return 0;
		}
		if (!all && ut_type == INIT_PROCESS)
			return 0;
	} else {
		/*
		 * Things like rlogin, LOGIN, etc don't null out dead
		 * records - we don't really want to report them twice
		 * But we do like to use their times to report duration.
		 */
		if (ut_type == DEAD_PROCESS) {
			/* convention is that 'dead' entries have null names */
			*ut_name = '\0';
			return 0;
		}
		if (!all) {
			/*
			 * by default don't print things spawned by init -
			 * this includes getty, uugetty, etc.,
			 * or login initiators
			 */
			if (ut_type == INIT_PROCESS || ut_type == LOGIN_PROCESS)
				   return 0;
		}
	}
	if (nameargs == 0)
		return (1);
	av = firstarg;
	for (ac = 0; ac < nameargs; ac++, av++) {
		if (av[0][0] == '-')
			continue;
		if (nameq(*av, ut_name) || lineq(ut_line, *av))
			return (1);
	}
	return (0);
}

char *
strspl(left, right)
	char *left, *right;
{
	char *res = (char *)malloc(strlen(left)+strlen(right)+1);

	strcpy(res, left);
	strcat(res, right);
	return (res);
}


static char lu_msg[] = "[-#] [-a] [-x] [-f FILE] [-W] [name ...] [tty ...]";

static void
last_usage(void)
{
	fprintf(stderr, "Usage:last %s\n", lu_msg);
}
