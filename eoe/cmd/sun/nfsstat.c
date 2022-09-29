#ifndef lint
static char sccsid[] = 	"@(#)nfsstat.c	1.2 88/05/17 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

/*
 * nfsstat: Network File System statistics
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
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/signal.h>
#include <sys/sysmp.h>

#  include <sys/immu.h>

/*
 * client side rpc statistics
 * NOTE: These data structures must match the kernel ones in nfs_stat.h!
 */
struct rcstat {
        int	rccalls;
        int	rcbadcalls;
        int	rcretrans;
        int	rcbadxids;
        int	rctimeouts;
        int	rcwaits;
        int	rcnewcreds;
	int     rcbadverfs;
} rcstat;

/*
 * client side nfs statistics
 */
struct clstat {
        int	nclsleeps;              /* client handle waits */
        int	nclgets;                /* client handle gets */
        int	ncalls;                 /* client requests */
        int	nbadcalls;              /* rpc failures */
        int	reqs[32];               /* count of each request */
} clstat, clstat3;

/*
 * Server side rpc statistics
 */
struct rsstat {
        int	rscalls;
        int	rsbadcalls;
        int	rsnullrecv;
        int	rsbadlen;
        int	rsxdrcall;
        int	rsduphits;	/* duplicate request cache hits */
        int	rsdupage;	/* average age of recycled cache entries */
} rsstat;

/*
 * server side nfs statistics
 */
struct svstat {
        int     ncalls;         /* number of calls received */
        int     nbadcalls;      /* calls that failed */
        int     reqs[32];       /* count for each request */
} svstat, svstat3;

static int	ccode = 1;	/* SGI doesn't separate these two */
static int 	scode = 1; 	/* Identify server and client code present */

static int	Interval = 1;		/* seconds for below */
static int	Cflag = 0;		/* Continuous stat display */
static int	gotstats = 0;
static void disp(int);

static void     getstats(void);
static void     putstats(void);
static void     cr_print(int);
static void     sr_print(int);
static void     cn_print(int);
static void     cn_print3(int);
static void     sn_print(int);
static void     sn_print3(int);
static void     req_print(u_int *, u_int, char **, int);
static void     usage(void);

main(argc, argv)
	char *argv[];
{
	int	cflag = 0;		/* client stats */
	int	sflag = 0;		/* server stats */
	int	nflag = 0;		/* nfs stats */
	int	rflag = 0;		/* rpc stats */
	int	zflag = 0;		/* zero stats after printing */
	int 	c;

	opterr = 0;
	while ((c = getopt(argc, argv, "Ccnrsz")) != EOF)
		switch (c) {
		case 'C':
			Cflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'r':
			rflag++;
			break;
		case 's':
			sflag++;
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
		disp(sflag ? 1 : 0);
	getstats();
	if (sflag && (!scode)) {
		fprintf(stderr,"nfsstat: kernel is not configured with the server nfs and rpc code.\n");
	}
	if ((sflag || (!sflag && !cflag)) && scode) {
		if (rflag || (!rflag && !nflag)) {
			sr_print(zflag);
		}
		if (nflag || (!rflag && !nflag)) {
			sn_print(zflag);
			sn_print3(zflag);
		}
	}
	if (cflag && (!ccode)) {
		fprintf(stderr,"nfsstat: kernel is not configured with the client nfs and rpc code.\n");
	}
	if ((cflag || (!sflag && !cflag)) && ccode) {
		if (rflag || (!rflag && !nflag)) {
			cr_print(zflag);
		}
		if (nflag || (!rflag && !nflag)) {
			cn_print(zflag);
			cn_print3(zflag);
		}
	}
	if (zflag) {
		putstats();
	}
	exit(0);
}

static void
getstats()
{
	gotstats++;
	if (ccode) {
		if (sysmp(MP_SAGET, MPSA_RCSTAT, &rcstat, sizeof(rcstat)) 
								== -1) {
			fprintf(stderr,"can't read rcstat\n");
			exit(1);
		}
		if (sysmp(MP_SAGET, MPSA_CLSTAT, &clstat, sizeof(clstat)) 
								== -1) {
			fprintf(stderr,"can't read clstat\n");
			exit(1);
		}
		if (sysmp(MP_SAGET, MPSA_CLSTAT3, &clstat3, sizeof(clstat)) 
								== -1) {
			fprintf(stderr,"can't read clstat3\n");
			exit(1);
		}
	}

	if (scode) {
		if (sysmp(MP_SAGET, MPSA_RSSTAT, &rsstat, sizeof(rsstat)) 
								== -1) {
			fprintf(stderr,"can't read rsstat\n");
			exit(1);
		}
		if (sysmp(MP_SAGET, MPSA_SVSTAT, &svstat, sizeof(svstat)) 
								== -1) {
			fprintf(stderr,"can't read svstat\n");
			exit(1);
		}
		if (sysmp(MP_SAGET, MPSA_SVSTAT3, &svstat3, sizeof(svstat)) 
								== -1) {
			fprintf(stderr,"can't read svstat3\n");
			exit(1);
		}
	}
}

static void
putstats()
{
	if (sysmp(MP_CLEARNFSSTAT) == -1) {
		fprintf(stderr,"can't clear nfsstat\n");
		exit(1);
	}
}

static void
cr_print(zflag)
	int zflag;
{
	fprintf(stdout, "\nClient RPC:\n");
	fprintf(stdout,
	 "calls      badcalls   retrans    badxid     timeout    wait       newcred	badversions\n");
	fprintf(stdout,
	    "%-11d%-11d%-11d%-11d%-11d%-11d%-11d%-11d\n",
	    rcstat.rccalls,
            rcstat.rcbadcalls,
            rcstat.rcretrans,
            rcstat.rcbadxids,
            rcstat.rctimeouts,
            rcstat.rcwaits,
            rcstat.rcnewcreds,
	    rcstat.rcbadverfs);
	if (zflag) {
		bzero(&rcstat, sizeof rcstat);
	}
}

static void
sr_print(zflag)
	int zflag;
{
	fprintf(stdout, "\nServer RPC:\n");
	fprintf(stdout,
	    "calls      badcalls   nullrecv   badlen     xdrcall    duphits    dupage\n");
	fprintf(stdout,
	    "%-11d%-11d%-11d%-11d%-11d%-11d%-8.2f\n",
           rsstat.rscalls,
           rsstat.rsbadcalls,
           rsstat.rsnullrecv,
           rsstat.rsbadlen,
           rsstat.rsxdrcall,
           rsstat.rsduphits,
           rsstat.rsdupage/(HZ*1.0));
	if (zflag) {
		bzero(&rsstat, sizeof rsstat);
	}
}

#define RFS_NPROC       18
char *nfsstr[RFS_NPROC] = {
	"null",
	"getattr",
	"setattr",
	"root",
	"lookup",
	"readlink",
	"read",
	"wrcache",
	"write",
	"create",
	"remove",
	"rename",
	"link",
	"symlink",
	"mkdir",
	"rmdir",
	"readdir",
	"fsstat" };

#define	RFS3_NPROC	22
char *nfs3str[RFS3_NPROC] = {
	"null",
	"getattr",
	"setattr",
	"lookup",
	"access",
	"readlink",
	"read",
	"write",
	"create",
	"mkdir",
	"symlink",
	"mknod",
	"remove",
	"rmdir",
	"rename",
	"link",
	"readdir",
	"readdir+",
	"fsstat",
	"fsinfo",
	"pathconf",
	"commit"
};

static void
cn_print(zflag)
	int zflag;
{
	int i;

	fprintf(stdout, "\nClient NFS V2:\n");
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s\n",
	    "calls", "badcalls", "nclget", "nclsleep");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            clstat.ncalls,
            clstat.nbadcalls,
            clstat.nclgets,
            clstat.nclsleeps);
	req_print((int *)clstat.reqs, clstat.ncalls, &nfsstr[0], RFS_NPROC);
	if (zflag) {
		bzero(&clstat, sizeof clstat);
	}
}

static void
cn_print3(zflag)
	int zflag;
{
	int i;

	fprintf(stdout, "\nClient NFS V3:\n");
	fprintf(stdout,
	    "%-13s%-13s%-13s%-13s\n",
	    "calls", "badcalls", "nclget", "nclsleep");
	fprintf(stdout,
	    "%-13d%-13d%-13d%-13d\n",
            clstat3.ncalls,
            clstat3.nbadcalls,
            clstat3.nclgets,
            clstat3.nclsleeps);
	req_print((int *)clstat3.reqs, clstat3.ncalls, &nfs3str[0], RFS3_NPROC);
	if (zflag) {
		bzero(&clstat3, sizeof clstat);
	}
}

static void
sn_print3(zflag)
	int zflag;
{
	fprintf(stdout, "\nServer NFS V3:\n");
	fprintf(stdout, "%-13s%-13s\n", "calls", "badcalls");
	fprintf(stdout, "%-13d%-13d\n", svstat3.ncalls, svstat3.nbadcalls);
	req_print((int *)svstat3.reqs, svstat3.ncalls, &nfs3str[0], RFS3_NPROC);
	if (zflag) {
		bzero(&svstat3, sizeof svstat);
	}
}

static void
sn_print(zflag)
	int zflag;
{
	fprintf(stdout, "\nServer NFS V2:\n");
	fprintf(stdout, "%-13s%-13s\n", "calls", "badcalls");
	fprintf(stdout, "%-13d%-13d\n", svstat.ncalls, svstat.nbadcalls);
	req_print((int *)svstat.reqs, svstat.ncalls, &nfsstr[0], RFS_NPROC);
	if (zflag) {
		bzero(&svstat, sizeof svstat);
	}
}

static void
req_print(req, tot, str, len)
	int	*req;
	int	tot;
	char	**str;
	int	len;
{
	int	i, j, p;
	char	fixlen[128];

	for (i=0; i<=len / 6; i++) {
		for (j=i*6; j<min(i*6+6, len); j++) {
			fprintf(stdout, "%-13s", str[j]);
		}
		fprintf(stdout, "\n");
		for (j=i*6; j<min(i*6+6, len); j++) {
			if (tot) {
				p = ((double)req[j]*100)/tot;
			} else {
				p = 0;
			}
			sprintf(fixlen, "%d %2d%% ", req[j], p);
			fprintf(stdout, "%-13s", fixlen);
		}
		fprintf(stdout, "\n");
	}
}

static void
usage()
{
	fprintf(stderr, "nfsstat [-Ccnrsz] [interval]\n");
	exit(1);
}

min(a,b)
	int a,b;
{
	if (a<b) {
		return(a);
	}
	return(b);
}

/*----------------------------------------------------------------------*/
/* Curses mode specific to SGI */

extern int printw(const char *, ...);
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

static struct rsstat zrsstat, drsstat;
static struct svstat zsvstat, dsvstat;
static struct svstat zsvstat3, dsvstat3;

static void
initserver(int initall)
{
	if (initall) {
		bzero(&drsstat, sizeof(drsstat));
		bzero(&dsvstat, sizeof(dsvstat));
		bzero(&dsvstat3, sizeof(dsvstat3));
	}
	bzero(&zrsstat, sizeof(zrsstat));
	bzero(&zsvstat, sizeof(zsvstat));
	bzero(&zsvstat3, sizeof(zsvstat3));
}

static void
zeroserver()
{
	zrsstat = drsstat;
	zsvstat = dsvstat;
	zsvstat3 = dsvstat3;
}

/* Scale a value if necessary */
static int
DSP(value)
     int value;
{
	if (cmode == DELTA) {
	  value += (Interval-1); /* round UP */
	  value /= Interval;
	}
	return (value);
}

static void
spr_server3(int cmd, register int y, char *tmb)
{
	register int i, j;
	register int x = 0;
	register int *req, *zreq;
	register int tot;
	register struct rsstat *rsp;
	register struct svstat *svp;

	if (cmode == DELTA) {
		rsp = &drsstat;
		svp = &dsvstat3;
	} else {
		rsp = &zrsstat;
		svp = &zsvstat3;
	}

	move(0,0);
	printw(" %d: Server   %.26s    %s",cmd,hostname,tmb); y++;
	PL("Server RPC:");
	PL("calls      badcalls   nullrecv   badlen     xdrcall    duphits    dupage");
	move(y,x);
	if (gotstats > 1)
		printw("%-11d%-11d%-11d%-11d%-11d%-11d%-8.2f",
           		DSP(rsstat.rscalls - rsp->rscalls),
           		DSP(rsstat.rsbadcalls - rsp->rsbadcalls),
           		DSP(rsstat.rsnullrecv - rsp->rsnullrecv),
           		DSP(rsstat.rsbadlen - rsp->rsbadlen),
           		DSP(rsstat.rsxdrcall - rsp->rsxdrcall),
           		DSP(rsstat.rsduphits - rsp->rsduphits),
           		rsstat.rsdupage/(HZ*1.0));
	y++;
	y++;	/* skip a blank line */


	PL("Server NFS V3:");
	PL("calls        badcalls");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d",
			DSP(svstat3.ncalls - svp->ncalls),
			DSP(svstat3.nbadcalls - svp->nbadcalls));
	y++;

	req = (int *)svstat3.reqs;
	zreq = (int *)(svp->reqs);
	tot = svstat3.ncalls - svp->ncalls;
	for (i = 0; i <= RFS3_NPROC / 6; i++) {
		if (Cfirst) {
			move(y,x);
			for (j = i*6; j < min(i*6+6, RFS3_NPROC); j++) {
				printw("%-13s", nfs3str[j]);
			}
		}
		y++;

		move(y,x);
		for (j = i*6; j < min(i*6+6, RFS3_NPROC); j++) {
			char	fixlen[128];
			int	curreq = req[j] - zreq[j];
			int	p;
			
			if (gotstats <= 1)
				break;
			if (tot) {
				p = ((double)curreq*100)/tot;
			} else {
				p = 0;
			}
			sprintf(fixlen, "%d %2d%% ", DSP(curreq), p);
			printw("%-13s", fixlen);
		}
		y++;
	}

	drsstat = rsstat;
	dsvstat3 = svstat3;
}

static void
spr_server(int cmd, register int y, char *tmb)
{
	register int i, j;
	register int x = 0;
	register int *req, *zreq;
	register int tot;
	register struct rsstat *rsp;
	register struct svstat *svp;

	if (cmode == DELTA) {
		rsp = &drsstat;
		svp = &dsvstat;
	} else {
		rsp = &zrsstat;
		svp = &zsvstat;
	}

	move(0,0);
	printw(" %d: Server   %.26s    %s",cmd,hostname,tmb); y++;
	PL("Server RPC:");
	PL("calls      badcalls   nullrecv   badlen     xdrcall    duphits    dupage");
	move(y,x);
	if (gotstats > 1)
		printw("%-11d%-11d%-11d%-11d%-11d%-11d%-8.2f",
           		DSP(rsstat.rscalls - rsp->rscalls),
           		DSP(rsstat.rsbadcalls - rsp->rsbadcalls),
           		DSP(rsstat.rsnullrecv - rsp->rsnullrecv),
           		DSP(rsstat.rsbadlen - rsp->rsbadlen),
           		DSP(rsstat.rsxdrcall - rsp->rsxdrcall),
           		DSP(rsstat.rsduphits - rsp->rsduphits),
           		rsstat.rsdupage/(HZ*1.0));
	y++;
	y++;	/* skip a blank line */


	PL("Server NFS V2:");
	PL("calls        badcalls");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d",
			DSP(svstat.ncalls - svp->ncalls),
			DSP(svstat.nbadcalls - svp->nbadcalls));
	y++;

	req = (int *)svstat.reqs;
	zreq = (int *)(svp->reqs);
	tot = svstat.ncalls - svp->ncalls;
	for (i = 0; i <= RFS_NPROC / 6; i++) {
		if (Cfirst) {
			move(y,x);
			for (j = i*6; j < min(i*6+6, RFS_NPROC); j++) {
				printw("%-13s", nfsstr[j]);
			}
		}
		y++;

		move(y,x);
		for (j = i*6; j < min(i*6+6, RFS_NPROC); j++) {
			char	fixlen[128];
			int	curreq = req[j] - zreq[j];
			int	p;
			
			if (gotstats <= 1)
				break;
			if (tot) {
				p = ((double)curreq*100)/tot;
			} else {
				p = 0;
			}
			sprintf(fixlen, "%d %2d%% ", DSP(curreq), p);
			printw("%-13s", fixlen);
		}
		y++;
	}

	drsstat = rsstat;
	dsvstat = svstat;
}

static struct rcstat zrcstat, drcstat;
static struct clstat zclstat, dclstat;
static struct clstat zclstat3, dclstat3;
static void
initclient(int initall)
{
	if (initall) {
		bzero(&drcstat, sizeof(drcstat));
		bzero(&dclstat, sizeof(dclstat));
		bzero(&dclstat3, sizeof(dclstat3));
	}
	bzero(&zrcstat, sizeof(zrcstat));
	bzero(&zclstat, sizeof(zclstat));
	bzero(&zclstat3, sizeof(zclstat3));
}

static void
zeroclient()
{
	zrcstat = drcstat;
	zclstat = dclstat;
	zclstat3 = dclstat3;
}

static void
_spr_client(int cmd, register int y, char *tmb, char *legend,
	struct rcstat *rcp, struct clstat *clp, struct clstat *cls,
	char **str, int strcnt)
{
	register int i, j;
	register int x = 0;
	register int *req, *zreq;
	register int tot;


	move(0,0);
	printw(" %d: Client   %.26s    %s",cmd,hostname,tmb);y++;
	PL("Client RPC:");
	PL("calls      badcalls   retrans    badxid     timeout    wait       newcred");
	move(y,x);
	if (gotstats > 1)
		printw("%-11d%-11d%-11d%-11d%-11d%-11d%-11d\n",
			DSP(rcstat.rccalls - rcp->rccalls),
			DSP(rcstat.rcbadcalls - rcp->rcbadcalls),
			DSP(rcstat.rcretrans - rcp->rcretrans),
			DSP(rcstat.rcbadxids - rcp->rcbadxids),
			DSP(rcstat.rctimeouts - rcp->rctimeouts),
			DSP(rcstat.rcwaits - rcp->rcwaits),
			DSP(rcstat.rcnewcreds - rcp->rcnewcreds));
	y++;
	y++;	/* skip a blank line */

	PL(legend);
	PL("calls        badcalls     nclget       nclsleep");
	move(y,x);
	if (gotstats > 1)
		printw("%-13d%-13d%-13d%-13d",
            		DSP(cls->ncalls - clp->ncalls),
            		DSP(cls->nbadcalls - clp->nbadcalls),
            		DSP(cls->nclgets - clp->nclgets),
            		DSP(cls->nclsleeps - clp->nclsleeps));
	y++;

	req = (int *)cls->reqs;
	zreq = (int *)(clp->reqs);
	tot = cls->ncalls - clp->ncalls;
	for (i = 0; i <= strcnt / 6; i++) {
		if (Cfirst) {
			move(y,x);
			for (j = i*6; j < min(i*6+6, strcnt); j++) {
				printw("%-13s", str[j]);
			}
		}
		y++;

		move(y,x);
		for (j = i*6; j < min(i*6+6, strcnt); j++) {
			char	fixlen[128];
			int	curreq = req[j] - zreq[j];
			int	p;

			if (gotstats <= 1)
				break;
			if (tot) {
				p = ((double)curreq*100)/tot;
			} else {
				p = 0;
			}
			sprintf(fixlen, "%d %2d%% ", DSP(curreq), p);
			printw("%-13s", fixlen);
		}
		y++;
	}

}
static void
spr_client(int cmd, register int y, char *tmb)
{
	register struct rcstat *rcp;
	register struct clstat *clp;

	if (cmode == DELTA) {
		rcp = &drcstat;
		clp = &dclstat;
	} else {
		rcp = &zrcstat;
		clp = &zclstat;
	}
	_spr_client(cmd, y, tmb, "Client NFS:", rcp, clp,
		&clstat, &nfsstr[0], RFS_NPROC);
	drcstat = rcstat;
	dclstat = clstat;
}
static void
spr_client3(int cmd, register int y, char *tmb)
{
	register struct rcstat *rcp;
	register struct clstat *clp;

	if (cmode == DELTA) {
		rcp = &drcstat;
		clp = &dclstat3;
	} else {
		rcp = &zrcstat;
		clp = &zclstat3;
	}
	_spr_client(cmd, y, tmb, "Client NFS3:", rcp, clp,
		&clstat3, &nfs3str[0], RFS3_NPROC);
	drcstat = rcstat;
	dclstat3 = clstat3;
}

#define NUMFUNCS 4
static void (*sprfuncs[NUMFUNCS])(int, int, char *) = {
    spr_client,
    spr_server,
    spr_client3,
    spr_server3,
};
static void (*sprinitfuncs[NUMFUNCS])(int) = {
    initclient,
    initserver,
    initclient,
    initserver,
};
static void (*sprzerofuncs[NUMFUNCS])() = {
    zeroclient,
    zeroserver,
    zeroclient,
    zeroserver,
};


const char min_cmd_num = '1';
const char max_cmd_num = '1' + NUMFUNCS - 1;

/*
 * Print a description of the station.
 */
static void
disp(int scmd)
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
	for (n = 0; n < NUMFUNCS; n++)
		sprinitfuncs[n](1);

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

		sprfuncs[scmd](scmd+1, 1, tmb);

		if (Cfirst) {
			standout();
			move(BOS, 0);
			printw(
"1: Client[V2]  2: Server[V2]  3: Client[V3]  4: Server[V3]             DZR:mode");
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
				for (n = 0; n < NUMFUNCS; n++)
					sprzerofuncs[n]();
				cmode = ZERO;
			} else if (c == 'r' || c == 'R') {
				for (n = 0; n < NUMFUNCS; n++)
					sprinitfuncs[n](0);
				cmode = NORM;
			} else if (c == 'd' || c == 'D') {
				cmode = DELTA;
			} else if ((c >= min_cmd_num) && (c <= max_cmd_num)) {
				scmd = c - '1';
			}
			Cfirst = 1;
		}
	}
}
