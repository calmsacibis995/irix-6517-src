static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/dkstat/RCS/dkstat.c,v 1.13 1999/02/12 03:08:20 kenmcd Exp $";

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <curses.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pcp/pmapi.h>
#include <pcp/pmapi_dev.h>

#include "spindle.h"

extern char *optarg;
extern int optind;
extern int pmDebug;

static char *usage = "dkstat [options] [interval [repeats]]\n"
"Reporting options:\n"
"    [-h host]                  : report statistics for remote host\n"
"    [-L]                       : connect to localhost (by-pass pmcd)\n"
"    [-f {r|t|s}]               : report style: row, table or sorted\n"
"    [-r] [-w]                  : reads (-r), writes (-w), both (-rw) or total\n"
"    [-c]                       : continuous display using curses\n"
"    [interval [repeats]]       : interval duration (5 sec) and repeats (inf)\n"
"\n"
"Device Selection:\n"
"    [-a]                       : select all controllers/drives (default)\n"
"    [-i devspec[,devspec ...]] : list of inclusions (all others excluded)\n"
"    [-x devspec[,devspec ...]] : list of exclusions (all others included)\n"
"    [-n ndisks]                : for -fs, only report most busy ndisks (22)\n"
"\n"
"Device selection (devspec) syntax: XXXN[dM[lU]]\n"
"    where XXX is controller name (e.g. dks), N is controller number,\n"
"    d is a literal 'd', M is drive number (1-15), l is a literal 'ell',\n"
"    and U is a logical unit number (0-7).\n"
"    If the drive (and logical unit number) is not given, all drives on\n"
"    the controller are matched.\n";

/* arg flags */
static int errflag=0, Iflag=0, cflag=0;
static int fflag='r', rflag=0, wflag=0;
static int Lflag = 0;
static char *hflag = NULL;
static int	debug;

#define INCLUDE_DEVSPEC 0
#define EXCLUDE_DEVSPEC 1

static int aflag=0;
static char *iflag=NULL, *xflag=NULL;

static int nflag=22;
static int interval=5, repeats=0, iter=0;
static int ndrives;
static spindle *stats, *prev;
static int *inclusion_list;

static __uint32_t delta_u32(__uint32_t, __uint32_t);
static void print_rate(int r, int w, int width, float r_rate, float w_rate);
static void print_space(int r, int w, int width);
static void parse_devspec(int exclude, char *flagspec);
static double tv_sub(struct timeval *a, struct timeval *b);
static int sort_cmp(spindle **a, spindle **b);
static int dname_cmp(char **a, char **b);
static int cname_cmp(controller *a, controller *b);
static int prnt(const char *, ...);
static void interrupt(void);

int
main(int argc, char *argv[])
{
    int c, i, j, con, drive, max_unit;
    int err;
    int	max_lun;
    int	max_wname;
    int	todo;
    int head_freq;
    char *pp;
    char *cmd = argv[0];
    char sbuf[64];
    char hname[MAXHOSTNAMELEN];
    int nheadings = 0;
    char **headings;
    struct hostent *host;
    char *fullhostname;
    int pcp_context;
    int is_local;
    int (*cmp)() = sort_cmp;
    int (*dcmp)() = dname_cmp;
    int (*ctlcmp)() = cname_cmp;
    int reconnecting = 0;
    spindle **ptrlist;
    int ncontrollers;
    controller *controllers;
    double duration;
    struct timeval this_time, prev_time;

    signal(SIGINT, interrupt);
    fclose(stdin); /* without this, dkstat hangs on keyboard input */

    while ((c = getopt(argc, argv, "D:n:h:cf:rwai:x:L?")) != EOF) {
        switch (c) {
	case 'D':	/* debug */
	    pmDebug = atoi(optarg);
	    break;
	case 'L':
	    Lflag = 1;
	    break;
        case 'h': /* host */
            hflag = strdup(optarg);
            break;
        case 'n':
            nflag = atoi(optarg);
            break;
        case 'a':
            aflag=1;
            break;
        case 'c':
            cflag=1;
            break;
        case 'r':
            rflag=1;
            break;
        case 'w':
            wflag=1;
            break;
        case 'f':
            fflag = *optarg;
            break;
        case 'i':
            iflag = strdup(optarg);
            break;
        case 'x':
            xflag = strdup(optarg);
            break;
        case '?':
        default:
            errflag++;
            break;
        }
    }

    if (errflag || optind > argc) {
USAGE:  
        (void)fprintf(stderr, "Usage: %s", usage);
        exit (1);
    }

    /* optional interval */
    if (optind < argc)
        sscanf(argv[optind++], "%d", &interval);

    /* optional repeats */
    if (optind < argc)
        sscanf(argv[optind++], "%d", &repeats);

    for (; optind < argc; optind++)
        (void)fprintf(stderr, "%s: extra arg '%s' ignored\n", cmd, argv[optind]);

    if (iflag && xflag || iflag && aflag || xflag && aflag) {
        fprintf(stderr, "%s: flags -i -x and -a are mutually exclusive\n", cmd);
        goto USAGE;
    }

    if (hflag && Lflag) {
	fprintf(stderr, "%s: Error: -h and -L are mutually exclusive\n", cmd);
	goto USAGE;
    }

    if (iflag == NULL && xflag == NULL)
        /* default */
        aflag=1;

    if (fflag != 'r' && fflag != 't' && fflag != 's') {
        fprintf(stderr,
        "%s: use -ft = table format, -fr = row format, -fs = sorted format\n", cmd);
        goto USAGE;
    }

    /*
     * pmcd connection
     */
    if (hflag == NULL) {
	gethostname(hname, sizeof(hname));
	hflag = strdup(hname);
	is_local = 1;
    }
    else
	is_local = 0;
    
    if ((host = gethostbyname(hflag)) == NULL) {
	fprintf(stderr, "%s: Error: host \"%s\": %s\n", cmd, hflag, hstrerror(h_errno));
	exit(1);
    }
    fullhostname = strdup(host->h_name);

    if (Lflag) {
	if ((pcp_context = pmNewContext(PM_CONTEXT_LOCAL, fullhostname)) < 0) {
	    fprintf(stderr, "%s: Error: could not connect to local host \"%s\" : %s\n",
		cmd, fullhostname, pmErrStr(pcp_context));
	    exit(1);
	}
    }
    else {
	__pmSetAuthClient();	/* we are an authorized PCP client */
	if ((pcp_context = pmNewContext(PM_CONTEXT_HOST, fullhostname)) < 0) {
	    if (is_local) { /* try pmcd by-pass */
		if ((pcp_context = pmNewContext(PM_CONTEXT_LOCAL, fullhostname)) < 0) {
		    fprintf(stderr, "%s: Error: could not connect to local host \"%s\" : %s\n",
			cmd, fullhostname, pmErrStr(pcp_context));
		    exit(1);
		}
	    }
	    else {
		fprintf(stderr, "%s: Error: could not connect to pmcd on host \"%s\" : %s\n",
		    cmd, fullhostname, pmErrStr(pcp_context));
		exit(1);
	    }
	}
    }

    if ((err = spindle_stats_init(&stats)) <= 0) {
	fprintf(stderr, "%s: Error: %s\n", cmd, pmErrStr(err));
	exit(1);
    }
    ndrives = err;
    if (nflag > ndrives)
        nflag = ndrives;
	
    inclusion_list = (int *)malloc(ndrives * sizeof(int));
    for (i=0; i < ndrives; i++) {
	/* include all drives by default */
	inclusion_list[i] = 1;
    }

    if ((err = spindle_stats(&stats, &this_time)) < 0) {
	fprintf(stderr, "%s: Error fetching disk stats: %s\n", cmd, pmErrStr(err));
	exit(1);
    }

    controllers = controller_info(&ncontrollers);
    qsort(controllers, ncontrollers, sizeof(controller), ctlcmp);

    if (fflag == 's' || fflag == 't') {
	/*
	 * -fs report style sorts by disk usage
	 * -ft needs to sort by drive name
	 */
	if ((ptrlist = (spindle **)malloc(ndrives * sizeof(spindle *))) == NULL) {
	    fprintf(stderr, "%s: malloc failed: %s\n", cmd, pmErrStr(-errno));
	    exit(1);
	}
	for (i=0; i < ndrives; i++)
	    ptrlist[i] = &stats[i];
	if (fflag == 't' && ndrives > 1)
	    qsort(ptrlist, ndrives, sizeof(spindle *), dcmp);
    }

    /*
     * parse the inclusions or exclusions
     */
    if (aflag)
        /* default is to include all drives on all controllers */
        ; /* do nothing */

    if (iflag)
        parse_devspec(INCLUDE_DEVSPEC, iflag);

    if (xflag)
        parse_devspec(EXCLUDE_DEVSPEC, xflag);
	
    if ((err = spindle_stats_profile(inclusion_list)) < 0) {
	fprintf(stderr, "%s: Error setting disk instance profile: %s\n", cmd, pmErrStr(err));
	exit(1);
    }


    if (rflag && wflag)
	max_wname = 9;
    else
	max_wname = 3;

    if (fflag == 't') {
	/* table headings */
	headings = (char **)malloc(ndrives * sizeof(char *));
	nheadings = 0;
    }

    for (drive=0; drive < ndrives; drive++) {
	spindle *d = &stats[drive];
	if (!inclusion_list[drive])
	    continue;
	if (d->wname > max_wname)
	    max_wname = d->wname;

	if (fflag == 't') {
	    for (i=0; i < nheadings; i++)
		if (strcmp(headings[i], d->unit) == 0)
		    break;
	    if (i == nheadings)
		headings[nheadings++] = d->unit;
	}
    }

    switch (fflag) {
    case 'r':
        head_freq = 23;
        break;
    case 't':
	qsort(headings, nheadings, sizeof(char *), dcmp);
        head_freq = 22 / ncontrollers;
        if (head_freq == 0)
            head_freq = 1;
        break;
    case 's':
	if (ndrives <= nflag) {
	    head_freq = 22 / ndrives;
	    if (head_freq == 0)
		head_freq = 1;
	}
	else
	    head_freq = 1;
	break;
    default:
        head_freq = 1;
        break;
    }

    if (cflag) {
	initscr();
	clear();
	refresh();
	cflag++;
    }

    for (iter=(-1); repeats==0 || iter < repeats; iter++) {

	if (reconnecting > 0) {
	    if ((err = pmReconnectContext(pcp_context)) < 0) {
		/* metrics source is still unavailable */
		goto NEXT;
	    }
	    else {
		/*
		 * Reconnected successfully, but need headings
		 * and another fetch before reporting values
		 */
		reconnecting--;
	    }
	}

        if ((err = spindle_stats(&stats, &this_time)) < 0) {
	    if (cflag)
		move(0,0);
	    if (err == PM_ERR_IPC) {
		prnt("Fetch failed to \"%s\". Reconnecting ...\n", fullhostname, pmErrStr(err));
		if (cflag)
		    clrtobot();
		reconnecting = 2;
		goto NEXT;
	    }
	    else {
		prnt("%s: Error: Fetch failed to host \"%s\" : %s.", cmd, fullhostname, pmErrStr(err));
		interrupt(); /* exit */
	    }
	}

        if (cflag || (iter >= 0 && (reconnecting == 1 || iter % head_freq == 0))) {
            /*
             * heading
             */
	    if (cflag)
		move(0,0);
            prnt("# %s ", hflag);

            if (rflag && wflag)
                prnt("disk read/write rate, ");
            else if (rflag == 0 && wflag == 0)
                prnt("total I/O rate, ");
            else if (rflag)
                prnt("disk read rate, ");
            else
                prnt("disk write rate, ");

            prnt("interval: %d sec, %s", interval, ctime(&this_time.tv_sec));

	    if (cflag)
		move(1, 0);

            switch (fflag) {
            case 's':
                /* sorted format heading */
                if (iter >= 0) {
		    prnt("Disk(s)        Read/s  Write/s    %%Busy   MeanST");
		}
		prnt("\n");
                break;

            case 'r':
                /* row format heading */
		for (drive=0; drive < ndrives; drive++) {
		    spindle *d = &stats[drive];
		    if (inclusion_list[drive]) {
			if (rflag == 0 && wflag == 0 || rflag && wflag == 0 || rflag == 0 && wflag)
			    /* read | write | total */
			    prnt("%-*s ", d->wname, d->dname);
			else
			if (rflag && wflag)
			    /* both XXXr+XXXw */
			    prnt("%*s ", max_wname, d->dname);
		    }
                }
		prnt("\n");
                break;

            case 't':
                /* table format heading */
                prnt("Disk(s)    ");
		for (i=0; i < nheadings; i++) {
                    if (rflag == 0 && wflag == 0 || rflag && wflag == 0 || rflag == 0 && wflag) {
                        /* read | write | total */
                        prnt("%4s", headings[i]);
                    }
                    else
		    if (rflag && wflag) {
                        /* both XXXr+XXXw */
                        prnt("%*s ", max_wname, headings[i]);
                    }
                }
		prnt("\n");
                break;

            default:
                exit(99);
            }
        }

        if (iter < 0) {
            goto NEXT;
	}

	if (reconnecting > 0) {
	    reconnecting--;
            goto NEXT;
	}

        duration = tv_sub(&this_time, &prev_time);

	/*
	 * Compute deltas, and store them back in prev
	 * (we need current stats to use as prev on the next iteration)
	 */
	for (drive=0; drive < ndrives; drive++) {
	    spindle *a = &stats[drive];
	    spindle *b = &prev[drive];

	    b->reads = delta_u32(a->reads, b->reads);
	    b->writes = delta_u32(a->writes, b->writes);
	    b->breads = delta_u32(a->breads, b->breads);
	    b->bwrites = delta_u32(a->bwrites, b->bwrites);
	    if (fflag == 's') {
		ptrlist[drive] = b;
		b->active = delta_u32(a->active, b->active);
		b->response = delta_u32(a->response, b->response);
	    }
	}

	if (cflag)
	    move(2, 0);

        switch(fflag) {
        case 's':
            /* sorted format */
            if (ndrives > 1)
                qsort(ptrlist, ndrives, sizeof(spindle *), cmp);

            for (i=0; i < nflag; i++) {
		spindle *d = ptrlist[i];
		float active = (float)d->active;
                if (inclusion_list[d->drivenumber]) {
                    if (active > (float)d->response)
                        active = (float)d->response;

                    active /= 1000.0; /* active milliseconds -> seconds */

                    prnt("%-12s %8.1f %8.1f %8.1f %8.1f",
                    /* driveName, Read/s, Write/s */
                    d->dname,
		    (float)d->reads/duration,
                    (float)d->writes / duration,

                    /* Mean percent busy during interval */
                    100.0 * active / duration,

                    /* Mean Service Time in milliseconds */
                    ((d->reads + d->writes) == 0) ? 0.0 :
                    (1000.0 * active / (float)(d->reads + d->writes)));

		    prnt("\n");
                }
            }
            break;

        case 'r':
            /* row format */
	    for (drive=0; drive < ndrives; drive++) {
		spindle *d = &prev[drive];
		if (inclusion_list[drive]) {
		    print_rate(rflag, wflag, (rflag && wflag) ? max_wname+1 : d->wname,
		    (float)d->reads / duration, (float)d->writes / duration);
                }
            }
            prnt("\n");
            break;
	
	case 't':
	    /* table format */
	    for (i=0; i < ncontrollers; i++) {
		controller *c = &controllers[i];
		prnt("%-12s", c->dname);
		for (j=0; j < nheadings; j++) {
		    int k;
		    for (k=0; k < c->ndrives; k++) {
			spindle *d = &prev[c->drives[k]];
			if (strcmp(d->unit, headings[j]) == 0) {
			    print_rate(rflag, wflag, (rflag && wflag) ? max_wname+1 : 3,
				(float)d->writes/duration, (float)d->reads/duration);
			    break;
			}
		    }
		    if (k == c->ndrives) {
			/* leave a gap */
			print_space(rflag, wflag, (rflag && wflag) ? max_wname+1 : 3);
		    }
		}
		prnt("\n");
	    }
	    break;

        default:
            exit(99);
	}

NEXT:;
        prev_time = this_time; /* struct cpy */
	prev = stats;

	if (cflag) {
	    move(0, 0);
	    refresh();
	}

        if (repeats == 0 || iter < repeats-1)
            sleep(interval);

    } /* for */

    exit(0);
}

static int
dname_cmp(char **ap, char **bp)
{
    int a = atoi(*ap);
    int b = atoi(*bp);

    return a - b;
}

static int
cname_cmp(controller *ap, controller *bp)
{
    int a = 0;
    int b = 0;
    char *p;

    for (p=ap->dname; *p != NULL && !isdigit(*p); p++)
	; /**/
    if (isdigit(*p))
	a = atoi(p);
    for (p=bp->dname; *p != NULL && !isdigit(*p); p++)
	; /**/
    if (isdigit(*p))
	b = atoi(p);

    return a - b;
}

static int
sort_cmp(spindle **ap, spindle **bp)
{
    spindle *a = *ap, *b = *bp;
    int a_r = a->reads, b_r = b->reads;
    int a_w = a->writes, b_w = b->writes;

    if (rflag && wflag == 0)
        return (b_r - a_r);

    if (rflag == 0 && wflag)
        return (b_w - a_w);

    return ((b_r + b_w) - (a_r + a_w));
}


static void
print_space(int r, int w, int width)
{
    if (r && w)
	prnt("%9s ", "- "); /* both, fixed width: XXXr+XXXw */
    else
    if (width > 0) /* width of column heading */
	prnt("%*s ", width, "-");
}

static void
print_rate(int r, int w, int width, float r_rate, float w_rate)
{
    static char buf[64];
    if (r && w) {
        /* both, fixed width: XXXr+XXXw */
        if (r_rate > 0.0 && w_rate > 0.0) {
            sprintf(buf, "%.0fr+%.0fw", r_rate, w_rate);
        }
        else
	if (r_rate > 0.0)
	    sprintf(buf, "%3.0fr", r_rate);
        else
	if (w_rate > 0.0)
	    sprintf(buf, "%3.0fr", w_rate);
        else
	    strcpy(buf, "0 ");
	prnt("%9s ", buf);
    }
    else
    if (r == 0 && w == 0) {
        /* total, in the width of column heading */
        if (width > 0)
            prnt("%*.0f ", width, r_rate + w_rate);
    }
    else
    if (r) {
        /* reads, in the width of column heading */
        if (width > 0)
            prnt("%*.0f ", width, r_rate);
    }
    else
    if (w) {
        /* writes, in the width of column heading */
        if (width > 0)
            prnt("%*.0f ", width, w_rate);
    }
}

static void
parse_devspec(int exclude, char *flagspec)
{
    static char *sep = ",";
    char *spec, *p;
    char *devspec;
    register int con, drive;
    int is_controller;

    if (flagspec == NULL || strlen(flagspec) == 0)
        return;
    spec = strdup(flagspec);

    for (drive=0; drive < ndrives; drive++) {
	/*
	 * start condition:
	 *  include all (and exclude some)
	 *  or exclude all (and include some)
	 */
	inclusion_list[drive] = exclude ? 1 : 0;
    }

    devspec = strtok(spec, sep);
    while (devspec) {
	for (drive=0; drive < ndrives; drive++) {
	    spindle *d = &stats[drive];
	    if (debug)
		fprintf(stderr, "select %s : %s ?\n", devspec, d->dname);
	    if (strncmp(devspec, d->dname, strlen(devspec)) == 0)
		inclusion_list[drive] = exclude ? 0 : 1;
	}
        devspec = strtok(NULL, sep);
    }
    free(spec);
}

/* return real seconds for (a - b) */
static double
tv_sub(struct timeval *a, struct timeval *b)
{
    return a->tv_sec - b->tv_sec +
        ((double)(a->tv_usec - b->tv_usec))/1000000;
}

static __uint32_t
delta_u32(__uint32_t a, __uint32_t b)
{
    if (a >= b) 
	return a - b;
    return 0xffffffff - b + a;
}

static
int prnt(const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    return cflag ? vwprintw(stdscr, (char *)fmt, arg) : vfprintf(stdout, fmt, arg);
}

static void
interrupt(void)
{
    if (cflag == 2)
	endwin();
    exit(0);
}

