/***************************************************************************
 * pmval - performance metrics value dumper
 ***************************************************************************
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: pmval.c,v 2.70 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>		/* getopt, malloc, qsort and friends */
#include <sys/time.h>		/* timeval and friends */
#include <limits.h>		/* CLK_TCK */
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>		/* pow */

#include "pmapi.h"
#include "impl.h"
#include "pmapi_dev.h"

#if defined(IRIX6_5)
#include <optional_sym.h>
#endif

/***************************************************************************
 * constants
 ***************************************************************************/

#define FALSE	0
#define TRUE	1

#define	START	-1
#define STANDBY	0
#define FORW	1
#define BACK	2
#define MOVING	3
#define ENDLOG	4

static char usage[] =
    "Usage: %s [options] metricname\n\n"
    "Options:\n"
    "  -A align      align sample times on natural boundaries\n"
    "  -a archive    metrics source is a PCP log archive\n"
    "  -d            delay, pause between updates for archive replay\n"
    "  -g            start in GUI mode with new time control\n"
    "  -h host       metrics source is PMCD on host\n"
    "  -i instance   metric instance or list of instances - elements in an\n"
    "                instance list are separated by commas or whitespace\n"
    "  -n pmnsfile   use an alternative PMNS\n"
    "  -O offset     initial offset into the time window\n"
    "  -p port       port name for connection to existing time control\n"
    "  -r            output raw counter values\n"
    "  -S starttime  start of the time window\n"
    "  -s samples    terminate after this many samples\n"
    "  -T endtime    end of the time window\n"
    "  -t interval   sample interval [default 1 second]\n"
    "  -w width      set the width of each column of output\n"
    "  -Z timezone   set reporting timezone\n"
    "  -z            set reporting timezone to local time of metrics source\n";


/***************************************************************************
 * type definitions
 ***************************************************************************/

/* instance id - instance name association */
typedef struct {
    int  id;
    char *name;
} InstPair;

/* full description of a performance metric */
typedef struct {
    /* external (printable) description */
    char	*host;		/* name of host */
    char	*metric;	/* name of metric */
    int		iall;		/* all instances */
    int		inum;		/* number of instances */
    char	**inames;	/* list of instance names */
    /* internal description */
    int		handle;		/* context handle */
    pmID	pmid;		/* metric identifier */
    pmDesc	desc;		/* metric description */
    float	scale;		/* conversion factor for rate */
    int		*iids;		/* list of instance ids */
    /* internal-external association */
    InstPair	*ipairs;	/* sorted array of id-name */
} Context;


/***************************************************************************
 * Globals
 ***************************************************************************/

static char		*archive = NULL;
static pmLogLabel	label;
static char		*pmnsfile = PM_NS_DEFAULT;
static char		*rpt_tz = NULL;
static char		*rpt_tz_label = NULL;
static int		pauseFlag = 0;
static int		raw = 0;
static int		ahtype = PM_CONTEXT_HOST;	/* archive or host? */
static int		amode = PM_MODE_INTERP;		/* archive scan mode */
static char		local[] = "localhost";
static int		gui = 0;
static int		state = START;
static char		*control_port = NULL;
static int		control_fd;
static int		lastdeltaunits;	/* from -t or pmtime.vcrmode */
static int		lastdelta;	/* from -t or pmtime.delta */
static int		rawarchive = 0;
static pmTime		pmtime;
static struct timeval	last = {INT_MAX, 999999};	/* end time for log */

/***************************************************************************
 * timing functions
 ***************************************************************************/

/* add timevals */
static struct timeval
tadd(struct timeval t1, struct timeval t2)
{
    t1.tv_sec += t2.tv_sec;
    t1.tv_usec += t2.tv_usec;
    if (t1.tv_usec > 1000000) {
	(t1.tv_sec)++;
	t1.tv_usec -= 1000000;
    }
    return t1;
}


/* subtract timevals */
static struct timeval
tsub(struct timeval t1, struct timeval t2)
{
    t1.tv_usec -= t2.tv_usec;
    if (t1.tv_usec < 0) {
	t1.tv_usec += 1000000;
	t1.tv_sec--;
    }
    t1.tv_sec -= t2.tv_sec;
    return t1;
}

/* first timeval has reached second timeval to within 1 tick */
static int
reached(struct timeval t1, struct timeval t2)
{
    static struct timeval	tick = { 0, 0 };
    
    if (tick.tv_usec == 0) 
	/* one trip, usec per tick */
	tick.tv_usec = 1000000 / CLK_TCK;

    t1 = tadd(t1, tick);

    return (t1.tv_sec > t2.tv_sec) ||
           (t1.tv_sec == t2.tv_sec && t1.tv_usec >= t2.tv_usec);
    
}


/* convert timeval to ticks
    - positive time only
    - accurate to 1 tick */
static long
toticks(struct timeval t)
{
    static int	ticks_per_sec = 0;
    long	ticks;

    if (ticks_per_sec == 0)
	ticks_per_sec = CLK_TCK;

    ticks = ticks_per_sec * t.tv_sec + ticks_per_sec * t.tv_usec/1000000;

    if (ticks > 0)
	return ticks;
    else
	return 1L;
}

/* convert timeval to seconds */
static double
tosec(struct timeval t)
{
    return t.tv_sec + (t.tv_usec / 1000000.0);
}

/* sleep until given timeval */
static void
sleeptill(struct timeval sched)
{
    struct timeval curr;	/* current time */
    struct timeval delay;	/* interval to sleep */

    for (;;) {		/* loop to catch early wakeup by sginap */
        gettimeofday(&curr, NULL);
        if (reached(curr,sched)) return;
        delay = tsub(sched, curr);
        sginap(toticks(delay));
    }
}


/***************************************************************************
 * processing fetched values
 ***************************************************************************/

/* Compare two InstPair's on their id fields.
   - This function is passed as an argument to qsort,
     hence the ugly casts. */
static int	/* -1 less, 0 equal, 1 greater */
compair(const void *pair1, const void *pair2)
{
    if (((InstPair *)pair1)->id < ((InstPair *)pair2)->id) return -1;
    if (((InstPair *)pair1)->id < ((InstPair *)pair2)->id) return 1;
    return 0;
}


/* Does the Context have names for all instances in the pmValueSet? */
static int		/* 1 yes, 0 no */
chkinsts(Context *x, pmValueSet *vs)
{
    int      i, j;

    if (x->desc.indom == PM_INDOM_NULL)
	return 1;

    for (i = 0; i < vs->numval; i++) {
	for (j = 0; j < x->inum; j++) {
	    if (vs->vlist[i].inst == x->ipairs[j].id)
		break;
	}
	if (j == x->inum)
	    return 0;
    }
    return 1;
}


/***************************************************************************
 * interface to performance metrics API
 ***************************************************************************/

/* Fill in current instances into given Context.
   Instances sorted by instance identifier.  */
static void
initinsts(Context *x)
{
    int      *ip;
    char     **np;
    InstPair *pp;
    int      n;
    int      e;
    int      i;

    if (x->desc.indom == PM_INDOM_NULL)
	x->inum = 0;
    else {

	/* fill in instance ids for given profile */
	if (! x->iall) {
	    n = x->inum;
	    np = x->inames;
	    ip = (int *)malloc(n * sizeof(int));
	    if (ip == NULL) {
		__pmNoMem("pmval.ip", n * sizeof(int), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    x->iids = ip;
	    for (i = 0; i < n; i++) {
		if (ahtype == PM_CONTEXT_ARCHIVE)
		    e = pmLookupInDomArchive(x->desc.indom, *np);
		else
		    e = pmLookupInDom(x->desc.indom, *np);
		if (e < 0) {
            	    printf("%s: instance %s not available\n", pmProgname, *np);
            	    exit(EXIT_FAILURE);
		}
		*ip = e;
		np++;  ip++;
	    }
	    ip = x->iids;
	    np = x->inames;
	    if ((e = pmAddProfile(x->desc.indom, x->inum, x->iids)) < 0) {
		fprintf(stderr, "%s: pmAddProfile: %s\n", pmProgname, pmErrStr(e));
		exit(EXIT_FAILURE);
	    }
	}

	/* find all available instances */
	else {
	    if (ahtype == PM_CONTEXT_ARCHIVE)
		n = pmGetInDomArchive(x->desc.indom, &ip, &np);
	    else
		n = pmGetInDom(x->desc.indom, &ip, &np);
	    if (n < 0) {
                fprintf(stderr, "%s: pmGetInDom: %s\n", pmProgname, pmErrStr(n));
                exit(EXIT_FAILURE);
	    }
            x->inum = n;
	    x->iids = ip;
	    x->inames = np;
	}

	/* build InstPair list and sort */
	pp = (InstPair *)malloc(n * sizeof(InstPair));
	if (pp == NULL) {
	    __pmNoMem("pmval.pp", n * sizeof(InstPair), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	x->ipairs = pp;
	for (i = 0; i < n; i++) {
	    pp->id = *ip;
	    pp->name = *np;
	    ip++;  np++; pp++;
	}
	qsort(x->ipairs, (size_t)n, sizeof(InstPair), compair);
    }
}


/* Initialize API and fill in internal description for given Context. */
static void
initapi(Context *x)
{
    int e;

    x->handle = pmWhichContext();

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((e = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: pmLoadNameSpace: %s\n", pmProgname, pmErrStr(e));
	    exit(EXIT_FAILURE);
	}
    }

    if ((e = pmLookupName(1, &(x->metric), &(x->pmid))) < 0) {
        fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmProgname, x->metric, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if ((e = pmLookupDesc(x->pmid, &(x->desc))) < 0) {
        fprintf(stderr, "%s: pmLookupDesc: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if (x->desc.sem == PM_SEM_COUNTER) {
	if (x->desc.units.dimTime == 0)
	    x->scale = 1.0;
	else {
	    if (x->desc.units.scaleTime > PM_TIME_SEC)
		x->scale = pow(60, (PM_TIME_SEC - x->desc.units.scaleTime));
	    else
		x->scale = pow(1000, (PM_TIME_SEC - x->desc.units.scaleTime));
	}
    }
}

static void
ack_tctl(struct timeval *now)
{
    int		sts;

    /* let pmtime control know we are done */
    if ((sts = pmTimeSendAck(now)) < 0) {
	if (sts == -EPIPE)
	    fprintf(stderr, "\n%s: Time Controller has exited, goodbye\n",
		pmProgname);
	else
	    fprintf(stderr, "\n%s: pmTimeSendAck: %s\n", pmProgname, pmErrStr(sts));
	exit(EXIT_FAILURE);
    }
}


/* Fetch metric values. */
static int
getvals(Context *x,		/* in - full pm description */
        pmResult **vs)		/* alloc - pm values */
{
    pmResult	*r;
    int		e;
    int		i;

    if (rawarchive) {
	/*
	 * for -U mode, read until we find either a pmResult with the
	 * pmid we are after, or a mark record
	 */
	for ( ; ; ) {
	    e = pmFetchArchive(&r);
	    if (e < 0)
		break;

	    if (r->numpmid == 0) {
		if (gui || archive != NULL)
		    __pmPrintStamp(stdout, &r->timestamp);
		printf("  Archive logging suspended\n");
		return -1;
	    }

	    for (i = 0; i < r->numpmid; i++) {
		if (r->vset[i]->pmid == x->pmid)
		    break;
	    }
	    if (i != r->numpmid)
		break;
	    pmFreeResult(r);
	}
    }
    else {
	e = pmFetch(1, &(x->pmid), &r);
	i = 0;
    }

    if (e < 0) {
	if (e == PM_ERR_EOL && gui) {
	    ack_tctl(&last);
	    if (state != ENDLOG) {
		printf("\n[Time Control] End of Archive ...\n");
		state = ENDLOG;
	    }
	    return -1;
	}
	if (rawarchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(e));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if (gui)
	ack_tctl(&r->timestamp);

    if ((double)r->timestamp.tv_sec + (double)r->timestamp.tv_usec/1000000 >
	(double)last.tv_sec + (double)last.tv_usec/1000000) {
	return -2;
    }

    if (r->vset[i]->numval == 0) {
	if (gui || archive != NULL) {
	    __pmPrintStamp(stdout, &r->timestamp);
	    printf("  ");
	}
	printf("No values available\n");
	return -1;
    }
    else if (r->vset[i]->numval < 0) {
	if (rawarchive)
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	else
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	return -1;
    }

    *vs = r;
    qsort(&r->vset[i]->vlist[i],
          (size_t)r->vset[i]->numval,
          sizeof(pmValue),
          compair);

    return i;
}


/***************************************************************************
 * output
 ***************************************************************************/

/* How many print positions required for value of given type? */
static int
howide(int type)
{
    switch (type) {
    case PM_TYPE_32: return(11);
    case PM_TYPE_U32: return(11);
    case PM_TYPE_64: return(21);
    case PM_TYPE_U64: return(21);
    case PM_TYPE_FLOAT: return(13);
    case PM_TYPE_DOUBLE: return(21);
    case PM_TYPE_STRING: return(21);
    case PM_TYPE_AGGREGATE: return(21);
    default:
	fprintf(stderr, "pmval: unknown performance metric value type\n");
	exit(EXIT_FAILURE);
    }
    /*NOTREACHED*/
}

/*
 * Get Extended Time Base interval and Units from a timeval
 */
#define SECS_IN_24_DAYS 2073600.0

static void
getXTBintervalFromTimeval(int *ival, int *mode, struct timeval *tval)
{
    double tmp_ival = tval->tv_sec + tval->tv_usec / 1000000.0;

    if (tmp_ival > SECS_IN_24_DAYS) {
	*ival = (int)tmp_ival;
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_SEC);
    }
    else {
	*ival = (int)(tmp_ival * 1000.0);
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_MSEC);
    }
}

/*
 * Get the interval in seconds
 */
static double
getXTBinSeconds(int *ival, int *mode)
{
    double rval = 0.0;

    switch(PM_XTB_GET(*mode)) {
    case PM_TIME_NSEC:
	rval = *ival / 1000000000.0;
	break;
    case PM_TIME_USEC:
	rval = *ival / 1000000.0;
	break;
    case PM_TIME_SEC:
	rval = (double)*ival;
	break;
    case PM_TIME_MIN:
	rval = *ival * 60;
	break;
    case PM_TIME_HOUR:
	rval = *ival * 3600;
	break;
    case PM_TIME_MSEC:
    default:
	/* default (not XTB) is milliseconds */
	rval = *ival / 1000.0;
	break;
    }
    
    return rval;
}


/* Print parameter values as output header. */
static void
printhdr(Context *x, long smpls, struct timeval delta, struct timeval first)
{
    pmUnits		units;
    time_t		t;
    char		tbfr[26];
    const char		*u;

    /* metric name */
    printf("metric:    %s\n", x->metric);

    /* live host */
    if (archive == NULL)
	printf("host:      %s\n", x->host);

    /* archive */
    else {
	printf("archive:   %s\n", archive);
	printf("host:      %s\n", label.ll_hostname);
	t = (time_t) first.tv_sec;
	printf("start:     %s", pmCtime(&t, tbfr));
	if (last.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime(&last.tv_sec, tbfr));
    }

    /* semantics */
    printf("semantics: ");
    switch (x->desc.sem) {
    case PM_SEM_COUNTER:
	printf("cumulative counter");
	if (! raw) printf(" (converting to rate)");
	break;
    case PM_SEM_INSTANT:
        printf("instantaneous value");
	break;
    case PM_SEM_DISCRETE:
        printf("discrete instantaneous value");
	break;
    default:
        printf("unknown");
    }
    putchar('\n');

    /* units */
    units = x->desc.units;
    u = pmUnitsStr(&units);
    printf("units:     %s", *u == '\0' ? "none" : u);
    if ((! raw) && (x->desc.sem == PM_SEM_COUNTER)) {
	printf(" (converting to ");
	if (units.dimTime == 0) units.scaleTime = PM_TIME_SEC;
	units.dimTime--;
	if ((units.dimSpace == 0) && (units.dimTime == 0) && (units.dimCount == 0))
	    printf("time utilization)");
	else {
	    u = pmUnitsStr(&units);
	    printf("%s)", *u == '\0' ? "none" : u);
	}
    }
    putchar('\n');

    /* sample count */
    if (smpls == 0) printf("samples:   all\n");
    else printf("samples:   %i\n", smpls);
    if (smpls != 1 && (ahtype != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP)) {
	printf("interval:  %1.2f sec\n", tosec(delta));
	getXTBintervalFromTimeval(&lastdelta, &lastdeltaunits, &delta);
    }
}

/* Print instance identifier names as column labels. */
static void
printlabels(Context *x, int cols)
{
    int		n = x->inum;
    InstPair	*pairs = x->ipairs;
    int		i;
    static int	style = -1;

    if (style == -1) {
	InstPair	*ip = pairs;
	style = 0;
	for (i = 0; i < n; i++) {
	    if (strlen(ip->name) > cols) {
		style = 2;		/* too wide */
		break;
	    }
	    if (strlen(ip->name) > cols-3)
		style = 1;		/* wide enough to change shift */
	    ip++;
	}
	if (style == 2) {
	    ip = pairs;
	    for (i = 0; i < n; i++) {
		printf("full label for instance[%d]: %s\n", i, ip->name);
		ip++;
	    }
	}
    }

    putchar('\n');
    for (i = 0; i < n; i++) {
	if ((gui || archive != NULL) && i == 0)
	    printf("            ");
	if (raw || (x->desc.sem != PM_SEM_COUNTER) || style != 0)
	    printf("%*.*s ", cols, cols, pairs->name);
	else
	    /* shift left by 3 places for decimal points in rate */
	    printf("%*.*s    ", cols-3, cols-3, pairs->name);
	pairs++;
    }
    if (n > 0) putchar('\n');
}

void
printreal(double v, int minwidth)
{
    char	*fmt;

    /*
     *   <--  minwidth -->
     *   xxxxxxxxxxxxxxxxx
     *                   !	no value
     *           x.xxxE-xx	< 0.1
     *              0.0___	0
     *              x.xxxx	0.1 ... 0.9999
     *              x.xxx_	1 ... 9.999
     *		   xx.xx__	10 ... 99.99
     *            xxx.x___	100 ... 999.9
     *           xxxx.____	1000 ... 9999
     *           x.xxxE+xx	> 9999
     */

    if (v < 0.0)
	printf("%*s", minwidth, "!");
    else {
	if (v == 0) {
	    fmt = "%*.0f.0   ";
	    minwidth -= 5;
	}
	else if (v < 0.1 || v > 9999)
	    fmt = "%*.3E";
	else if (v <= 0.9999)
	    fmt = "%*.4f";
	else if (v <= 9.999) {
	    fmt = "%*.3f ";
	    minwidth -= 1;
	}
	else if (v <= 99.99) {
	    fmt = "%*.2f  ";
	    minwidth -= 2;
	}
	else if (v <= 999.9) {
	    fmt = "%*.1f   ";
	    minwidth -= 3;
	}
	else {
	    fmt = "%*.0f.    ";
	    minwidth -= 5;
	}
	printf(fmt, minwidth, v);

    }
}

/* Print performance metric values */
static void
printvals(Context *x, pmValueSet *vset, int cols)
{
    int 	i, j;
    pmAtomValue	av;
    int		doreal = 0;

    if (x->desc.type == PM_TYPE_FLOAT || x->desc.type == PM_TYPE_DOUBLE)
	doreal = 1;

    /* null instance domain */
    if (x->desc.indom == PM_INDOM_NULL) {
	if (vset->numval == 1) {
	    if (doreal) {
		pmExtractValue(vset->valfmt, &vset->vlist[0], x->desc.type, &av, PM_TYPE_DOUBLE);
		printreal(av.d, cols);
	    }
	    else
		pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[0], cols);

	}
	else
	    printf("%*s", cols, "?");
	putchar('\n');
    }

    /* non-null instance domain */
    else {
	for (i = 0; i < x->inum; i++) {
	    for (j = 0; j < vset->numval; j++) {
		if (vset->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (j < vset->numval) {
		if (doreal) {
		    pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    printreal(av.d, cols);
		}
		else
		    pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[j], cols);
	    }
	    else
		printf("%*s", cols, "?");
	    putchar(' ');
	}
	putchar('\n');

	for (j = 0; j < vset->numval; j++) {
	    for (i = 0; i < x->inum; i++) {
		if (vset->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (x->iall == 1 && i == x->inum) {
		printf("Warning: value=");
		if (doreal) {
		    pmExtractValue(vset->valfmt, &vset->vlist[j], x->desc.type, &av, PM_TYPE_DOUBLE);
		    printreal(av.d, 1);
		}
		else
		    pmPrintValue(stdout, vset->valfmt, x->desc.type, &vset->vlist[j], 1);
		printf(", but instance=%d is unknown\n", vset->vlist[j].inst);
	    }
	}
    }
}


/* print single performance metric rate value */
static void
printrate(int     valfmt,	/* from pmValueSet */
          int     type,		/* from pmDesc */
          pmValue *val1,	/* current value */
          pmValue *val2,	/* previous value */
	  double  delta,	/* time difference between samples */
          int     minwidth)	/* output is at least this wide */
{
    pmAtomValue a, b;
    double	v;
    static int	dowrap = -1;

    pmExtractValue(valfmt, val1, type, &a, PM_TYPE_DOUBLE);
    pmExtractValue(valfmt, val2, type, &b, PM_TYPE_DOUBLE);
    v = a.d - b.d;
    if (v < 0.0) {
	if (dowrap == -1) {
	    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	    if (getenv("PCP_COUNTER_WRAP") == NULL)
		dowrap = 0;
	    else
		dowrap = 1;
	}
	if (dowrap) {
	    switch (type) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    v += (double)UINT_MAX+1;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    v += (double)ULONGLONG_MAX+1;
		    break;
	    }
	}
    }
    v /= delta;
    printreal(v, minwidth);
}

/* Print performance metric rates */
static void
printrates(Context *x,
	   pmValueSet *vset1, struct timeval stamp1,	/* current values */
	   pmValueSet *vset2, struct timeval stamp2,	/* previous values */
	   int cols)
{
    int     i, j;
    double  delta;

    /* compute delta from timestamps and convert units */
    delta = x->scale * (tosec(stamp1) - tosec(stamp2));

    /* null instance domain */
    if (x->desc.indom == PM_INDOM_NULL) {
	if ((vset1->numval == 1) && (vset2->numval == 1))
	    printrate(vset1->valfmt, x->desc.type, &vset1->vlist[0], &vset2->vlist[0], delta, cols);
	else
	    printf("%*s", cols, "?");
	putchar('\n');
    }

    /* non-null instance domain */
    else {
	for (i = 0; i < x->inum; i++) {
	    for (j = 0; j < vset1->numval; j++) {
		if (vset1->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if ((j < vset1->numval) && (j < vset2->numval) &&
		(vset1->vlist[j].inst == vset2->vlist[j].inst))
		printrate(vset1->valfmt, x->desc.type, &vset1->vlist[j], &vset2->vlist[j], delta, cols);
	    else
		printf("%*s", cols, "?");
	    putchar(' ');
	}
	putchar('\n');

	for (j = 0; j < vset1->numval; j++) {
	    for (i = 0; i < x->inum; i++) {
		if (vset1->vlist[j].inst == x->ipairs[i].id)
		    break;
	    }
	    if (x->iall == 1 && i == x->inum && j < vset2->numval &&
		vset1->vlist[j].inst == vset2->vlist[j].inst) {
		printf("Warning: value=");
		printrate(vset1->valfmt, x->desc.type, &vset1->vlist[j], &vset2->vlist[j], delta, 1);
		printf(", but instance=%d is unknown\n", vset1->vlist[j].inst);
	    }
	}
    }
}


/***************************************************************************
 * command line processing
 ***************************************************************************/

#define WHITESPACE ", \t\n"

static int
isany(char *p, char *set)
{
    if (p != NULL && *p) {
	while (*set) {
	    if (*p == *set)
		return 1;
	    set++;
	}
    }
    return 0;
}

/*
 * like strtok, but smarter
 */
static char *
getinstance(char *p)
{
    static char	*save;
    char	quot;
    char	*q;
    char	*start;

    if (p == NULL)
	q = save;
    else
	q = p;
    
    while (isany(q, WHITESPACE))
	q++;

    if (*q == '\0')
	return NULL;
    else if (*q == '"' || *q == '\'') {
	quot = *q;
	start = ++q;

	while (*q && *q != quot)
	    q++;
	if (*q == quot)
	    *q++ = '\0';
    }
    else {
	start = q;
	while (*q && !isany(q, WHITESPACE))
	    q++;
    }
    if (*q)
	*q++ = '\0';
    save = q;

    return start;
}

/* extract command line arguments - exits on error */
static void
getargs(int		argc,		/* in - command line argument count */
        char		*argv[],	/* in - argument strings */
        Context		*cntxt,		/* out - full pm description */
        struct timeval	*posn,		/* out - first sample time */
        struct timeval	*delta,		/* out - sample interval */
        long		*smpls,		/* out - number of samples */
	int		*cols)		/* out - output column width */
{
    extern char	*optarg;
    extern int	optind;
    extern int	errno;
    int		c;

    char        *subopt;

    long	d;
    int		errflag = 0;
    int         i;
    int		src = 0;

    char	*host = local;
    int		sts;
    char        *endnum;

    char	    *Sflag = NULL;		/* argument of -S flag */
    char	    *Tflag = NULL;		/* argument of -T flag */
    char	    *Aflag = NULL;		/* argument of -A flag */
    char	    *Oflag = NULL;		/* argument of -O flag */
    int		    zflag = 0;			/* for -z */
    char 	    *tz = NULL;		/* for -Z timezone */
    int		    tzh;			/* initial timezone handle */
    struct timeval  logStart;
    struct timeval  first;
    pmMetricSpec   *msp;
    char	    *msg;
    struct stat	statbuf;

    __pmSetAuthClient();

    /* fill in default values */
    cntxt->iall = 1;
    cntxt->inum = 0;
    cntxt->inames = NULL;
    delta->tv_sec = 1;
    delta->tv_usec = 0;
    *smpls = 0;
    *cols = 0;

    /* extract command-line arguments */
    while ((c = getopt(argc, argv, "A:a:D:dgh:i:n:O:p:rs:S:t:T:U:w:zZ:?")) != EOF) {
	switch (c) {

	case 'A':		/* sample alignment */
	    Aflag = optarg;
	    break;

	case 'a':		/* interpolate archive */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    archive = optarg;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'd':
	    pauseFlag = 1;
	    break;

	case 'g':
	    gui = 1;
	    break;

	case 'h':		/* host name */
	    if (++src > 1) {
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
		errflag++;
	    }
	    cntxt->host = host = optarg;
	    break;

	case 'i':		/* instance names */
	    cntxt->iall = 0;
	    i = cntxt->inum;
	    subopt = getinstance(optarg);
	    while (subopt != NULL) {
		i++;
		cntxt->inames =
		    (char **)realloc(cntxt->inames, i * (sizeof (char *)));
		if (cntxt->inames == NULL) {
		    __pmNoMem("pmval.ip", i * sizeof(char *), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		*(cntxt->inames + i - 1) = subopt;
		subopt = getinstance(NULL);
	    }
	    cntxt->inum = i;
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':		/* sample offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* port for slave of existing pmtime master */
	    if ((sts = stat(optarg, &statbuf)) < 0) {
		fprintf(stderr, "%s: Error: can not access time control port \"%s\": %s\n",
		    pmProgname, optarg, pmErrStr(-errno));
		errflag++;
	    }
	    else if ((statbuf.st_mode & S_IFSOCK) != S_IFSOCK) {
		fprintf(stderr, "%s: Error: time control port \"%s\" is not a socket\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		control_port = optarg;
	    break;

	case 'r':		/* raw */
	   raw = 1;
	   break;

	case 's':		/* sample count */
	    d = (int)strtol(optarg, &endnum, 10);
	    if (Tflag) {
		fprintf(stderr, "%s: at most one of -E and -T allowed\n", pmProgname);
		errflag++;
	    }
	    else if (*endnum != '\0' || d < 0.0) {
		fprintf(stderr, "%s: -s requires +ve numeric argument\n", pmProgname);
		errflag++;
	   }
	   else *smpls = d;
	   break;

	case 'S':		/* start run time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, delta, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	    break;

	case 'T':		/* run time */
	    if (*smpls != 0) {
		fprintf(stderr, "%s: at most one of -T and -s allowed\n", pmProgname);
		errflag++;
	    }
	    Tflag = optarg;
	    break;

	case 'U':		/* non-interpolated archive (undocumented) */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a, -h and -U allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    amode = PM_MODE_FORW;
	    archive = optarg;
	    rawarchive = 1;
	    break;

	case 'w':		/* output column width */
	    errno = 0;
	    d = atol(optarg);
	    if (errno || d < 1) errflag++;
	    else *cols = d;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	    fprintf(stderr, usage, pmProgname);
	    exit(EXIT_FAILURE);
	    /* NOTREACHED */

	default:
	    errflag++;
	}
    }

    /* parse uniform metric spec */
    if (optind >= argc) {
	fprintf(stderr, "Error: no metricname specified\n\n");
	errflag++;
    }
    else if (optind < argc-1) {
	fprintf(stderr, "Error: pmval can only process one metricname at a time\n\n");
	errflag++;
    }
    else {
	if (ahtype == PM_CONTEXT_HOST) {
	    if (pmParseMetricSpec(argv[optind], 0, host, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
	else {		/* must be archive */
	    if (pmParseMetricSpec(argv[optind], 1, archive, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    if (msp->isarch) {
	archive = msp->source;
	ahtype = PM_CONTEXT_ARCHIVE;
    }

    if (ahtype == PM_CONTEXT_ARCHIVE) {
	if (gui == 1 && control_port != NULL) {
	    fprintf(stderr, "%s: -g cannot be used with -p\n", pmProgname);
	    errflag++;
	}
	if (gui == 1 && pauseFlag) {
	    fprintf(stderr, "%s: -g cannot be used with -d\n", pmProgname);
	    errflag++;
	}
    }
    else {
	if (pauseFlag) {
	    fprintf(stderr, "%s: -d can only be used with -a\n", pmProgname);
	    errflag++;
	}
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    cntxt->metric = msp->metric;
    if (msp->ninst > 0) {
	cntxt->inum = msp->ninst;
	cntxt->iall = (cntxt->inum == 0);
	cntxt->inames = &msp->inst[0];
    }

    /* open connection to host */
    if (msp->isarch == 0) {
	if ((sts = pmNewContext(PM_CONTEXT_HOST, msp->source)) < 0) {
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, msp->source, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	cntxt->host = msp->source;
	gettimeofday(&logStart, NULL);
    }

    /* open connection to archive */
    else {
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, msp->source)) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, msp->source, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	logStart = label.ll_start;
	if ((sts = pmGetArchiveEnd(&last)) < 0) {
	    fprintf(stderr, "%s: Cannot determine end of archive: %s",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	if (ahtype == PM_CONTEXT_ARCHIVE) {
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	    rpt_tz_label = label.ll_hostname;
	}
	else {
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
	    rpt_tz_label = host;
	}
	pmWhichZone(&rpt_tz);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
	pmWhichZone(&rpt_tz);
    }
    else printf("\n");

    if (pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
			   &logStart, &last,
			   &first, &last, posn, &msg) < 0) {
	fprintf(stderr, msg);
	exit(EXIT_FAILURE);
    }

    if (!gui &&
	*smpls == 0 && last.tv_sec != INT_MAX && amode != PM_MODE_FORW)
	*smpls = (long)((tosec(last) - tosec(*posn)) / tosec(*delta) + 1);

    if (gui || control_port != NULL) {
	/* set up pmtime control */
	int		mode;

	if (msp->isarch)
	    mode = PM_TCTL_MODE_ARCHIVE;
	else
	    mode = PM_TCTL_MODE_HOST;

	pmtime.showdialog = 0;	/* don't expose dialog yet */

	if (gui) {
	    mode |= PM_TCTL_MODE_NEWMASTER;
	    control_port = mktemp("/usr/tmp/vcr.XXXXXX");
	}
	else
	    mode |= PM_TCTL_MODE_MASTER;

	getXTBintervalFromTimeval(&pmtime.delta, &pmtime.vcrmode, delta);

	if (msp->isarch) {
	    pmtime.position = *posn;
	    pmtime.start = first;
	    pmtime.finish = last;
	}
	else
	    gettimeofday(&pmtime.position, NULL);
	if (rpt_tz == NULL) {
#if defined(IRIX6_5)
            if (_MIPS_SYMBOL_PRESENT(__pmTimezone))
                rpt_tz = __pmTimezone();
            else
                rpt_tz = getenv("TZ");
#else
            rpt_tz = __pmTimezone();
#endif
	    if (msp->isarch) {
		if ((sts = pmNewZone(rpt_tz)) < 0) {
		    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
			pmProgname, rpt_tz, pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
	    }
	}
	strncpy(pmtime.tz, rpt_tz, sizeof(pmtime.tz));
	if (rpt_tz_label == NULL)
	    rpt_tz_label = "localhost";
	strncpy(pmtime.tzlabel, rpt_tz_label, sizeof(pmtime.tzlabel));
	if ((control_fd = pmTimeConnect(mode, control_port, &pmtime)) < 0) {
	    fprintf(stderr, "%s: pmTimeConnect: %s\n", pmProgname, pmErrStr(control_fd));
	    exit(EXIT_FAILURE);
	}
	gui = 1;		/* means using pmtime control from here on */
    }
    else if (msp->isarch) {
	/* archive, and no pmtime control, go it alone */
	int tmp_ival;
	int tmp_mode;
	getXTBintervalFromTimeval(&tmp_ival, &tmp_mode, delta);
	tmp_mode = (tmp_mode & 0xffff0000) | (amode & __PM_MODE_MASK);
	if ((sts = pmSetMode(tmp_mode, posn, tmp_ival)) < 0) {
	    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }
}

/***************************************************************************
 * main
 ***************************************************************************/
int
main(int argc, char *argv[])
{
    struct timeval  delta;		/* sample interval */
    long	    smpls;		/* number of samples */
    int             cols;		/* width of output column */
    struct timeval  now;		/* current task start time */
    struct timeval  sched;		/* next task scheduled time */
    Context	    cntxt;		/* performance metric description */
    pmResult	    *rslt1;		/* current values */
    pmResult	    *rslt2;		/* previous values */
    char		*p;
    int		    sts;
    int		    first = 1;		/* need first sample */
    int		    forever;
    int			cmd;
    int			fetch;
    int			idx1;
    int			idx2;
    int			no_values = 0;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }


    getargs(argc, argv, &cntxt, &now, &delta, &smpls, &cols);
    forever = (smpls == 0 || gui);
    initapi(&cntxt);
    initinsts(&cntxt);

    if (gui)
	/*
	 * seems safe enough at this point to expose the Time Control
	 * dialog
	 */
	pmTimeShowDialog(1);

    if (cols <= 0) cols = howide(cntxt.desc.type);
    printhdr(&cntxt, smpls, delta, now);

    /* wait till time for first sample */
    if (archive == NULL )
	sleeptill(now);

    /* main loop fetching and printing sample values */
    while (forever || (smpls-- > 0)) {

	if (gui) {
	    for ( ; ; ) {
		fetch = 0;
		cmd = pmTimeRecv(&pmtime);
		if (cmd < 0) {
		    fprintf(stderr, "\n%s: Time Control dialog has terminated: %s\n",
			pmProgname, pmErrStr(cmd));
		    fprintf(stderr, "Sorry.\n");
            	    exit(EXIT_FAILURE);
		}

		switch (cmd) {
		    case PM_TCTL_SET:
			if (state == ENDLOG)
			    state = STANDBY;
			else if (state == FORW)
			    state = START;
			break;

		    case PM_TCTL_STEP:
			if (pmtime.delta < 0) {
			    if (state != BACK) {
				printf("\n[Time Control] Rewind/Reverse ...\n");
				state = BACK;
			    }
			}
			else if (state != FORW && state != ENDLOG) {
			    if (ahtype == PM_CONTEXT_ARCHIVE) {
				if (state != STANDBY)
				    printf("\n[Time Control] Repositioned in archive ...\n");
			    }
			    else {
				printf("\n[Time Control] Resume ...\n");
			    }
			    if (pmtime.delta != lastdelta ||
			        PM_XTB_GET(pmtime.vcrmode) != PM_XTB_GET(lastdeltaunits))
				printf("new interval:  %1.2f sec\n", 
					getXTBinSeconds(&pmtime.delta, &pmtime.vcrmode));

			    if (ahtype == PM_CONTEXT_ARCHIVE) {
				int setmode = PM_MODE_INTERP | (pmtime.vcrmode & 0xffff0000);
				if ((sts = pmSetMode(setmode, &pmtime.position, pmtime.delta)) < 0) {
				    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
				    exit(EXIT_FAILURE);
				}
			    }
			    lastdeltaunits = pmtime.vcrmode & 0xffff0000;
			    lastdelta = pmtime.delta;
			    state = FORW;
			    first = 1;
			}
		    
			if (state == BACK || state == ENDLOG) {
			    /*
			     * for EOL and reverse travel, no pmFetch,
			     * so ack here
			     */
			    ack_tctl(&pmtime.position);
			    break;
			}
			fetch = 1;
			break;

		    case PM_TCTL_TZ:
			if ((sts = pmNewZone(pmtime.tz)) < 0) {
			    fprintf(stderr, "%s: Warning: cannot set timezone to \"%s\": %s\n",
				pmProgname, pmtime.tz, pmErrStr(sts));
			}
			break;

		    case PM_TCTL_VCRMODE:
			/* something has changed ... suppress reporting */
			if ((pmtime.vcrmode & __PM_MODE_MASK) == PM_TCTL_VCRMODE_DRAG)
			    state = MOVING;
			else if (state != MOVING)
			    state = STANDBY;
			break;

		    /*
		     * safely and silently ignore these
		     */
		    case PM_TCTL_SHOWDIALOG:
			break;

		    case PM_TCTL_SKIP:
		    case PM_TCTL_BOUNDS:
		    case PM_TCTL_ACK:
			break;

		    default:
			printf("pmTimeRecv: cmd %d?\n", cmd);
			break;
		}
		if (fetch)
		    break;
	    }
	}

	if (first) {
	    if ((idx2 = getvals(&cntxt, &rslt2)) >= 0) {
		/* first-time success */
		first = 0;
		if (cntxt.desc.indom != PM_INDOM_NULL)
		    printlabels(&cntxt, cols);
		if (raw || (cntxt.desc.sem != PM_SEM_COUNTER)) {
		    if (gui || archive != NULL)
			__pmPrintStamp(stdout, &rslt2->timestamp);
		    printvals(&cntxt, rslt2->vset[idx2], cols);
		    continue;
		}
		else if (no_values) {
		    if (gui || archive != NULL) {
			__pmPrintStamp(stdout, &rslt2->timestamp);
			printf("  ");
		    }
		    printf("No values available\n");
		}
		no_values = 0;
		if (gui)
		    /* pmtime controls timing */
		    continue;
	    }
	    else if (idx2 == -2)
		/* out the end of the window */
		break;
	    else
		no_values = 1;
	}

	/* wait till time for sample */
	if (pauseFlag) {
	    sginap(toticks(delta));
	}
	else if (archive == NULL) {
	    sched = tadd(now,delta);
	    now = sched;
	    sleeptill(sched);
	}

	if (first)
	    /* keep trying */
	    continue;

	/* next sample */
	if ((idx1 = getvals(&cntxt, &rslt1)) == -2)
		/* out the end of the window */
		break;
	else if (idx1 < 0) {
	    first = 1;
	    continue;
	}

	/* refresh instance names */
	if (cntxt.iall && ! chkinsts(&cntxt, rslt1->vset[idx1])) {
	    free(cntxt.iids);
	    if (cntxt.iall)
		free(cntxt.inames);
	    free(cntxt.ipairs);
	    initinsts(&cntxt);
	    printlabels(&cntxt, cols);
	}

	/* print values */
	if (gui || archive != NULL)
	    __pmPrintStamp(stdout, &rslt1->timestamp);
	if (raw || (cntxt.desc.sem != PM_SEM_COUNTER))
	    printvals(&cntxt, rslt1->vset[idx1], cols);
	else
	    printrates(&cntxt, rslt1->vset[idx1], rslt1->timestamp,
		       rslt2->vset[idx2], rslt2->timestamp, cols);

	/* discard previous and save current result */
	pmFreeResult(rslt2);
	rslt2 = rslt1;
	idx2 = idx1;
    }

    exit(first == 0);
    /*NOTREACHED*/
}
