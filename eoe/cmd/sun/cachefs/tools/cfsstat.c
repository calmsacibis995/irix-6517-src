/*
 * The origin of this is nfsstat.

#ifndef lint
static char sccsid[] = 	"@(#)nfsstat.c	1.2 88/05/17 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

/*
 * cfsstat: Cache File System statistics
 */

#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>
#include <curses.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/sysmp.h>
#include <cachefs/cachefs_fs.h>

#  include <sys/immu.h>

static int	Interval = 1;		/* seconds for below */
static int	Cflag = 0;		/* Continuous stat display */
static int	fflag = 0;		/* display fileheader cache statistics */
static int	bflag = 0;		/* display back FS operation statistics */
static int	gotstats = 0;
static struct cachefs_stats cfs_stats;
static void disp(void);

getstats(void)
{
	gotstats++;
	if (sysmp(MP_SAGET, MPSA_CFSSTAT, &cfs_stats, sizeof(cfs_stats)) == -1) {
		fprintf(stderr,"can't read CacheFS stats\n");
		exit(1);
	}

}

putstats(void)
{
	if (sysmp(MP_CLEARCFSSTAT) == -1) {
		fprintf(stderr,"can't clear CacheFS stats\n");
		exit(1);
	}
}


cfs_fileheader_print(int zflag)
{
	int i;
	int hits;
	int misses;

	if (cfs_stats.cs_fileheaders.cf_reads != 0) {
		hits = (int)(((double)cfs_stats.cs_fileheaders.cf_hits /
			(double)cfs_stats.cs_fileheaders.cf_reads) * 100.0);
		misses = (int)(((double)cfs_stats.cs_fileheaders.cf_misses /
			(double)cfs_stats.cs_fileheaders.cf_reads) * 100.0);
	} else {
		hits = misses = 0;
	}
	fprintf(stdout, "\nCacheFS:\n");
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s\n",
	    "hits(%)", "misses(%)", "reads", "writes");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            hits,
            misses,
            cfs_stats.cs_fileheaders.cf_reads,
            cfs_stats.cs_fileheaders.cf_writes);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s\n",
	    "cache enter", "cache remove", "LRU enter", "LRU remove");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_fileheaders.cf_cacheenters,
            cfs_stats.cs_fileheaders.cf_cacheremoves,
            cfs_stats.cs_fileheaders.cf_lruenters,
            cfs_stats.cs_fileheaders.cf_lruremoves);
	fprintf(stdout,
	    "%-13s%-13s\n",
	    "purges", "releases");
	fprintf(stdout,
	    "%-13d%-13d\n",
            cfs_stats.cs_fileheaders.cf_purges,
            cfs_stats.cs_fileheaders.cf_releases);
	if (zflag) {
		bzero(&cfs_stats, sizeof cfs_stats);
	}
}

cfs_print(int zflag)
{
	int i;

	fprintf(stdout, "\nCacheFS:\n");
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s%-13s\n",
	    "inval", "nocache", "reclaim", "dnlchit", "shorthit", "shortmiss");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_inval,
            cfs_stats.cs_nocache,
            cfs_stats.cs_reclaims,
            cfs_stats.cs_dnlchit,
            cfs_stats.cs_shorthit,
            cfs_stats.cs_shortmiss);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "lookuphit", "lookupstale", "lookupmiss", "nocachelook", "lookups");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_lookuphit,
            cfs_stats.cs_lookupstale,
            cfs_stats.cs_lookupmiss,
            cfs_stats.cs_nocachelookup,
            cfs_stats.cs_lookups);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "readhit", "readmiss", "reads", "nocachereads", "readerrors");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_readhit,
            cfs_stats.cs_readmiss,
            cfs_stats.cs_reads,
            cfs_stats.cs_nocachereads,
            cfs_stats.cs_readerrors);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "writes", "nocachewrite", "writeerrors", "asyncreqs", "replacements");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_writes,
            cfs_stats.cs_nocachewrites,
            cfs_stats.cs_writeerrors,
            cfs_stats.cs_asyncreqs,
			cfs_stats.cs_replacements);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "rdirhit", "rdirmiss", "readdirs", "short rdlink", "long rdlink");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_readdirhit,
            cfs_stats.cs_readdirmiss,
            cfs_stats.cs_readdirs,
            cfs_stats.cs_shortrdlnk,
            cfs_stats.cs_longrdlnk);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "vnops", "vfsops", "backvnops", "backvfsops", "getbackvp");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_vnops,
            cfs_stats.cs_vfsops,
            cfs_stats.cs_backvnops,
            cfs_stats.cs_backvfsops,
			cfs_stats.cs_getbackvp);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s%-13s\n",
	    "objchecks", "back checks", "objinits", "objmods", "objinvals",
		"objexprires");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_objchecks,
			cfs_stats.cs_backchecks,
			cfs_stats.cs_objinits,
			cfs_stats.cs_objmods,
			cfs_stats.cs_objinvals,
			cfs_stats.cs_objexpires);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s\n",
	    "newcnodes", "makecnode", "nocnode", "cnodehit");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_newcnodes,
            cfs_stats.cs_makecnode,
            cfs_stats.cs_nocnode,
            cfs_stats.cs_cnodehit);
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s%-13s\n",
	    "cnrestart", "cnlookagain", "cntoss", "reclaim race", "destroy race");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d%-13d\n",
            cfs_stats.cs_cnoderestart,
            cfs_stats.cs_cnodelookagain,
            cfs_stats.cs_cnodetoss,
            cfs_stats.cs_race.cr_reclaim,
			cfs_stats.cs_race.cr_destroy);
	if (zflag) {
		bzero(&cfs_stats, sizeof cfs_stats);
	}
}

usage(void)
{
	fprintf(stderr, "cfsstat [-Cz] [interval]\n");
	exit(1);
}

main(int argc, char **argv)
{
	int	zflag = 0;		/* zero stats after printing */
	int 	c;

	opterr = 0;
	while ((c = getopt(argc, argv, "Cfbz")) != EOF)
		switch (c) {
		case 'f':
			fflag++;
			break;
		case 'b':
			bflag++;
			break;
		case 'C':
			Cflag++;
			break;
		case 'z':
			if (getuid()) {
				fprintf(stderr,
				    "Must be root for z flag\n");
				exit(1);
			}
			zflag++;
			break;
		default:
			usage();
		}
	if (optind < argc) {
		Interval = atoi(argv[optind]);
		if (Interval > 0)
		  optind++;
	}


	if (optind < argc)
		usage();

	if (Cflag)
		disp();
	getstats();
	if (fflag)
		cfs_fileheader_print(zflag);
	else
		cfs_print(zflag);
	if (zflag) {
		putstats();
	}
	exit(0);
}

/*----------------------------------------------------------------------*/
/* Curses mode specific to SGI */

const	int	BOS = 23;	/* Index of last line on screen */

static void
quit(int stat)
{
	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	exit(stat);
}

static void
fail(char *fmt, ...)
{
	va_list args;

	if (Cflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	va_start(args, fmt);
	fprintf(stderr, "smtstat: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}


static int Cfirst = 1;
static enum cmode {NORM, ZERO, DELTA} cmode;
static char hostname[MAXHOSTNAMELEN+1];

const	char	ctrl_l = 'L' & 037;

#define LABEL_LEN 14
#define NUM_LEN	  12
#define P(lab,fmt,val) \
	{ \
	    if (Cfirst) { move(y,x); printw("%-.*s", LABEL_LEN, lab); } \
	    move(y,x); \
	    printw(fmt,(val)); \
	    y++; \
	}
#define PL(lab) \
	{ \
	    if (Cfirst) { move(y,x); printw("%-s",lab); } \
	    y++; \
	}

static struct cachefs_stats zcfs_stat, dcfs_stat;

/* Scale a value if necessary */
static int
DSP(int value)
{
	if (cmode == DELTA) {
		value += (Interval-1); /* round UP */
		value /= Interval;
	}
	return (value);
}

static void
initstats(int initall)
{
	if (initall) {
		bzero(&dcfs_stat, sizeof(dcfs_stat));
	}
	bzero(&zcfs_stat, sizeof(zcfs_stat));
}

static void
zerostats(void)
{
	zcfs_stat = dcfs_stat;
}

static void
_print_fileheader_stats(register int y, char *tmb,
	struct cachefs_stats *cfsp, struct cachefs_stats *cfss)
{
	register int i, j;
	register int x = 0;
	register int *req, *zreq;
	register int tot;
	int reads = cfss->cs_fileheaders.cf_reads - cfsp->cs_fileheaders.cf_reads;
	int hits = (int)(((double)(cfss->cs_fileheaders.cf_hits -
		cfsp->cs_fileheaders.cf_hits) /
		(double)(cfss->cs_fileheaders.cf_reads -
		cfsp->cs_fileheaders.cf_reads)) * 100.0);
	int misses = (int)(((double)(cfss->cs_fileheaders.cf_misses -
		cfsp->cs_fileheaders.cf_misses) /
		(double)(cfss->cs_fileheaders.cf_reads -
		cfsp->cs_fileheaders.cf_reads)) * 100.0);

	if (reads != 0) {
		hits = (int)(((double)(cfss->cs_fileheaders.cf_hits -
			cfsp->cs_fileheaders.cf_hits) / (double)reads) * 100.0);
		misses = (int)(((double)(cfss->cs_fileheaders.cf_misses -
			cfsp->cs_fileheaders.cf_misses) / (double)reads) * 100.0);
	} else {
		hits = misses = 0;
	}

	move(0,0);
	printw("CacheFS  %.26s    %s",hostname,tmb);y++;

	PL("hits(%)      misses(%)    reads        writes");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d",
			DSP(hits),
			DSP(misses),
			DSP(cfss->cs_fileheaders.cf_reads - cfsp->cs_fileheaders.cf_reads),
			DSP(cfss->cs_fileheaders.cf_writes - cfsp->cs_fileheaders.cf_writes));
	y++;
	PL("cache enter  cache remove LRU enter    LRU remove");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d",
			DSP(cfss->cs_fileheaders.cf_cacheenters - cfsp->cs_fileheaders.cf_cacheenters),
			DSP(cfss->cs_fileheaders.cf_cacheremoves - cfsp->cs_fileheaders.cf_cacheremoves),
			DSP(cfss->cs_fileheaders.cf_lruenters - cfsp->cs_fileheaders.cf_lruenters),
			DSP(cfss->cs_fileheaders.cf_lruremoves - cfsp->cs_fileheaders.cf_lruremoves));
	y++;
	PL("purges       releases");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d",
			DSP(cfss->cs_fileheaders.cf_purges - cfsp->cs_fileheaders.cf_purges),
			DSP(cfss->cs_fileheaders.cf_releases - cfsp->cs_fileheaders.cf_releases));
	y++;

}

static void
_printstats(register int y, char *tmb,
	struct cachefs_stats *cfsp, struct cachefs_stats *cfss)
{
	register int i, j;
	register int x = 0;
	register int *req, *zreq;
	register int tot;


	move(0,0);
	printw("CacheFS  %.26s    %s",hostname,tmb);y++;

	PL("inval        nocache      reclaim      dnlchit      shorthit     shortmiss");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_inval - cfsp->cs_inval),
            		DSP(cfss->cs_nocache - cfsp->cs_nocache),
            		DSP(cfss->cs_reclaims - cfsp->cs_reclaims),
            		DSP(cfss->cs_dnlchit - cfsp->cs_dnlchit),
            		DSP(cfss->cs_shorthit - cfsp->cs_shorthit),
            		DSP(cfss->cs_shortmiss - cfsp->cs_shortmiss));
	y++;
	PL("lookuphit    lookupstale  lookupmiss   nocachelook  lookups");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_lookuphit - cfsp->cs_lookuphit),
            		DSP(cfss->cs_lookupstale - cfsp->cs_lookupstale),
            		DSP(cfss->cs_lookupmiss - cfsp->cs_lookupmiss),
            		DSP(cfss->cs_nocachelookup - cfsp->cs_nocachelookup),
            		DSP(cfss->cs_lookups - cfsp->cs_lookups));
	y++;
	PL("readhit      readmiss     reads        nocachereads readerrors");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_readhit - cfsp->cs_readhit),
            		DSP(cfss->cs_readmiss - cfsp->cs_readmiss),
            		DSP(cfss->cs_reads - cfsp->cs_reads),
            		DSP(cfss->cs_nocachereads - cfsp->cs_nocachereads),
            		DSP(cfss->cs_readerrors - cfsp->cs_readerrors));
	y++;
	PL("writes       nocachewrite writeerrors  asyncreqs    replacements");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_writes - cfsp->cs_writes),
            		DSP(cfss->cs_nocachewrites - cfsp->cs_nocachewrites),
            		DSP(cfss->cs_writeerrors - cfsp->cs_writeerrors),
            		DSP(cfss->cs_asyncreqs - cfsp->cs_asyncreqs),
            		DSP(cfss->cs_replacements - cfsp->cs_replacements));
	y++;
	PL("rdirhit      rdirmiss     readdirs     short rdlink long rdlink");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_readdirhit - cfsp->cs_readdirhit),
            		DSP(cfss->cs_readdirmiss - cfsp->cs_readdirmiss),
            		DSP(cfss->cs_readdirs - cfsp->cs_readdirs),
            		DSP(cfss->cs_shortrdlnk - cfsp->cs_shortrdlnk),
            		DSP(cfss->cs_longrdlnk - cfsp->cs_longrdlnk));
	y++;
	PL("vnops        vfsops       back vnops   back vfsops  getbackvp");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_vnops - cfsp->cs_vnops),
            		DSP(cfss->cs_vfsops - cfsp->cs_vfsops),
            		DSP(cfss->cs_backvnops - cfsp->cs_backvnops),
            		DSP(cfss->cs_backvfsops - cfsp->cs_backvfsops),
            		DSP(cfss->cs_getbackvp - cfsp->cs_getbackvp));
	y++;
	PL("objchecks    back checks  objinits     objmods      objinvals    objexpires");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_objchecks - cfsp->cs_objchecks),
					DSP(cfss->cs_backchecks - cfsp->cs_backchecks),
					DSP(cfss->cs_objinits - cfsp->cs_objinits),
					DSP(cfss->cs_objmods - cfsp->cs_objmods),
					DSP(cfss->cs_objinvals - cfsp->cs_objinvals),
					DSP(cfss->cs_objexpires - cfsp->cs_objexpires));
	y++;
	PL("newcnodes    makecnode    nocnode      cnodehit");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_newcnodes - cfsp->cs_newcnodes),
            		DSP(cfss->cs_makecnode - cfsp->cs_makecnode),
            		DSP(cfss->cs_nocnode - cfsp->cs_nocnode),
            		DSP(cfss->cs_cnodehit - cfsp->cs_cnodehit));
	y++;
	PL("cnrestart    cnlookagain  cntoss       reclaim race destroy race");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d%-13d",
            		DSP(cfss->cs_cnoderestart - cfsp->cs_cnoderestart),
            		DSP(cfss->cs_cnodelookagain - cfsp->cs_cnodelookagain),
            		DSP(cfss->cs_cnodetoss - cfsp->cs_cnodetoss),
            		DSP(cfss->cs_race.cr_reclaim - cfsp->cs_race.cr_reclaim),
            		DSP(cfss->cs_race.cr_destroy - cfsp->cs_race.cr_destroy));
	y++;
	if (bflag) {
		PL("Back Vnode Ops:");
		move(y,x);
		PL("lku fsy rdl acc get set clo opn rdd cre rem lnk ren mkd rmd sym frl rdv wrv");
		move(y,x);
		if (gotstats > 1)
			printw("%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d%-4d",
				DSP(cfss->cs_backops.cb_look - cfsp->cs_backops.cb_look),
				DSP(cfss->cs_backops.cb_fsy - cfsp->cs_backops.cb_fsy),
				DSP(cfss->cs_backops.cb_rdl - cfsp->cs_backops.cb_rdl),
				DSP(cfss->cs_backops.cb_acc - cfsp->cs_backops.cb_acc),
				DSP(cfss->cs_backops.cb_get - cfsp->cs_backops.cb_get),
				DSP(cfss->cs_backops.cb_set - cfsp->cs_backops.cb_set),
				DSP(cfss->cs_backops.cb_clo - cfsp->cs_backops.cb_clo),
				DSP(cfss->cs_backops.cb_opn - cfsp->cs_backops.cb_opn),
				DSP(cfss->cs_backops.cb_rdd - cfsp->cs_backops.cb_rdd),
				DSP(cfss->cs_backops.cb_cre - cfsp->cs_backops.cb_cre),
				DSP(cfss->cs_backops.cb_rem - cfsp->cs_backops.cb_rem),
				DSP(cfss->cs_backops.cb_lnk - cfsp->cs_backops.cb_lnk),
				DSP(cfss->cs_backops.cb_ren - cfsp->cs_backops.cb_ren),
				DSP(cfss->cs_backops.cb_mkd - cfsp->cs_backops.cb_mkd),
				DSP(cfss->cs_backops.cb_rmd - cfsp->cs_backops.cb_rmd),
				DSP(cfss->cs_backops.cb_sym - cfsp->cs_backops.cb_sym),
				DSP(cfss->cs_backops.cb_frl - cfsp->cs_backops.cb_frl),
				DSP(cfss->cs_backops.cb_rdv - cfsp->cs_backops.cb_rdv),
				DSP(cfss->cs_backops.cb_wrv - cfsp->cs_backops.cb_wrv));
		y++;
	}

}
static void
printstats(register int y, char *tmb)
{
	register struct cachefs_stats *cfsp;

	if (cmode == DELTA) {
		cfsp = &dcfs_stat;
	} else {
		cfsp = &zcfs_stat;
	}
	if (fflag)
		_print_fileheader_stats(y, tmb, cfsp, &cfs_stats);
	else
		_printstats(y, tmb, cfsp, &cfs_stats);
	dcfs_stat = cfs_stats;
}

/*
 * Print a description of the station.
 */
static void
disp(void)
{
	time_t now;
	char tmb[26];
	struct timeval wait;
	fd_set rmask;
	struct termio  tb;
	int c, n;
	int intrchar;   /* user's interrupt character */
	int suspchar;   /* job control character */
	static char dtitle[] = "D: Delta/second";
	static char rtitle[] = "R: Normal mode";
	static char ztitle0[] = "Z: Delta -- %b %d %T";
	static char ztitle[]  = "Z: Delta -- MMM DD HH:MM:SS";

	cmode = DELTA;

	(void) gethostname(hostname, sizeof(hostname));
	initstats(1);

	initscr();
	raw();
	noecho();
	keypad(stdscr, TRUE);
	leaveok(stdscr, FALSE);
	move(0, 0);

	(void) ioctl(0, TCGETA, &tb);
	intrchar = tb.c_cc[VINTR];
	suspchar = tb.c_cc[VSUSP];
	FD_ZERO(&rmask);

	while (1) {
		if (Cfirst)
			clear();
		getstats();
		now = time(0);
		cftime(tmb,"%b %e %T", &now);

		printstats(1, tmb);

		if (Cfirst) {
			standout();
			move(BOS, 0);
			printw(
"CacheFS                                                                DZR:mode");
			standend();
			switch (cmode) {
			case DELTA:
				move(0, 80-sizeof(dtitle));
				printw(dtitle);
				break;
			case ZERO:
				move(0, 80-sizeof(ztitle));
				printw(ztitle);
				break;
			case NORM:
				move(0, 80-sizeof(rtitle));
				printw(rtitle);
				break;
			}
		}
		move(0, 0);
		refresh();
		Cfirst = 0;

		FD_SET(0, &rmask);
		wait.tv_sec = Interval;
		wait.tv_usec = 0;
		n = select(1, &rmask, NULL, NULL, &wait);
		if (n < 0) {
			fail("select: %s", strerror(errno));
			break;
		} else if (n == 1) {
			c = getch();
			if (c == intrchar || c == 'q' || c == 'Q') {
				quit(0);
				break;
			} else if (c == suspchar) {
				reset_shell_mode();
				kill(getpid(), SIGTSTP);
				reset_prog_mode();
			} else if (c == 'z' || c == 'Z') {
				now = time(0);
				cftime(ztitle,ztitle0,&now);
				zerostats();
				cmode = ZERO;
			} else if (c == 'r' || c == 'R') {
				initstats(0);
				cmode = NORM;
			} else if (c == 'd' || c == 'D') {
				cmode = DELTA;
			}
			Cfirst = 1;
		}
	}
}
