/***********************************************************************
 * dstruct.c - central data structures and associated operations
 ***********************************************************************
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

#ident "$Id: dstruct.c,v 1.2 1999/08/16 23:13:49 kenmcd Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <values.h>
#include <string.h>
#include "dstruct.h"
#include "symbol.h"
#include "pragmatics.h"
#include "fun.h"
#include "eval.h"
#include "show.h"
#include "impl.h"
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif

/***********************************************************************
 * constants
 ***********************************************************************/

char	localHost[MAXHOSTNAMELEN+1];	/* "official" name of localhost */
double	nan;				/* not-a-number run time initialized */


/***********************************************************************
 * user supplied parameters
 ***********************************************************************/

char		*pmnsfile = PM_NS_DEFAULT;	/* alternate name space */
Archive		*archives = NULL;		/* list of open archives */
RealTime	first = DBL_MAX;		/* archive starting point */
RealTime	last = 0.0;			/* archive end point */
char		*dfltHost;			/* default host */
RealTime	dfltDelta = DELTA_DFLT;		/* default sample interval */
char		*startFlag = NULL;		/* start time specified? */
char		*stopFlag = NULL;		/* end time specified? */
char		*alignFlag = NULL;		/* align time specified? */
char		*offsetFlag = NULL;		/* offset time specified? */
RealTime	runTime;			/* run time interval */
int		hostZone = 0;			/* timezone from host? */
char		*timeZone = NULL;		/* timezone from command line */
int		verbose = 0;			/* verbosity 0, 1 or 2 */
int		interactive = 0;		/* interactive mode, -d */
int		isdaemon = 0;			/* run as a daemon */
int		agent = 0;			/* secret agent mode? */
int		applet = 0;			/* applet mode? */
int		dowrap = 0;			/* counter wrap? default no */
int		licensed = 0;			/* pmie licensed? default no */
pmiestats_t	*perf;				/* live performance data */
pmiestats_t	instrument;			/* used if no mmap (archive) */


/***********************************************************************
 * this is where the action is
 ***********************************************************************/

Task		*taskq = NULL;		/* evaluator task queue */
Expr		*curr;			/* current executing rule expression */

SymbolTable	hosts;			/* currently known hosts */
SymbolTable	metrics;		/* currently known metrics */
SymbolTable	rules;			/* currently known rules */
SymbolTable	vars;			/* currently known variables */


/***********************************************************************
 * time
 ***********************************************************************/

RealTime	now;			/* current time */
RealTime	start;			/* start evaluation time */
RealTime	stop;			/* stop evaluation time */

Symbol		symDelta;		/* current sample interval */
Symbol		symMinute;		/* minutes after the hour 0..59 */
Symbol		symHour;		/* hours since midnight 0..23 */
Symbol		symDay;			/* day of the month 1..31 */
Symbol		symMonth;		/* month of the year 1..12 */
Symbol		symYear;		/* year 1996.. */
Symbol		symWeekday;		/* days since Sunday 0..6 */

static double	delta;			/* current sample interval */
static double	second;			/* seconds after the minute 0..59 */
static double	minute;			/* minutes after the hour 0..59 */
static double	hour;			/* hours since midnight 0..23 */
static double	day;			/* day of the month 1..31 */
static double	month;			/* month of the year 1..12 */
static double	year;			/* year 1996.. */
static double	weekday;		/* days since Sunday 0..6 */


/* return real time */
RealTime
getReal(void)
{
    struct timeval t;

    gettimeofday(&t, NULL);
    return realize(t);
}


/* update time variables to reflect current time */
void
reflectTime(RealTime d)
{
    static time_t   then = 0;			/* previous time */
    int		    skip = now - then;
    struct tm	    tm;

    then = (time_t)now;

    /* sample interval */
    delta = d;

    /* try short path for current time */
    if (skip >= 0 && skip < 24 * 60 * 60) {
	second += skip;
	if (second < 60)
	    return;
	skip = (int)(second / 60);
	second -= (double)(60 * skip);
	minute += (double)skip;
	if (minute < 60)
	    return;
	skip = (int)(minute / 60);
	minute -= (double)(60 * skip);
	hour += (double)skip;
	if (hour < 24)
	    return;
    }

    /* long path for current time */
    pmLocaltime(&then, &tm);
    second = (double) tm.tm_sec;
    minute = (double) tm.tm_min;
    hour = (double) tm.tm_hour;
    day = (double) tm.tm_mday;
    month = (double) tm.tm_mon;
		    /* tm_year is years since 1900, so this is Y2K safe */
    year = (double) tm.tm_year + 1900;
    weekday = (double) tm.tm_wday;
}


/* sleep until given RealTime */
void
sleepTight(RealTime sched)
{
    RealTime	curr;	/* current time */
    long	delay;	/* interval to sleep */
    int		status;

    /* harvest terminated children */
    while (waitpid(-1, &status, WNOHANG) > (pid_t)0);

    if (archives)
	return;

    for (;;) {		/* loop to catch early wakeup from sginap */
	curr = getReal();
	delay = CLK_TCK * (long)(sched - curr);
	if (delay < 1) return;
	sginap(delay);
    }
}


/* convert RealTime to timeval */
void
unrealize(RealTime rt, struct timeval *tv)
{
    tv->tv_sec = (time_t)rt;
    tv->tv_usec = (int)(1000000 * (rt - tv->tv_sec));
}


/***********************************************************************
 * ring buffer management
 ***********************************************************************/

void
newRingBfr(Expr *x)
{
    size_t  sz;
    char    *p;
    int     i;

    sz = ((x->sem == SEM_TRUTH) || (x->sem == SEM_CHAR)) ?
	    sizeof(char) * x->tspan :
	    sizeof(double) * x->tspan;
    if (x->ring) free(x->ring);
    x->ring = alloc(x->nsmpls * sz);
    p = (char *) x->ring;
    for (i = 0; i < x->nsmpls; i++) {
	x->smpls[i].ptr = (void *) p;
	p += sz;
    }
}


void
newStringBfr(Expr *x, size_t length, char *bfr)
{
    if (x->e_idom != (int)length) {
	x->e_idom = (int)length;
	x->tspan = (int)length;
	x->nvals = (int)length;
    }
    if (x->ring)
	free(x->ring);
    x->ring = bfr;
    x->smpls[0].ptr = (void *) bfr;
}


/* Rotate ring buffer - safe to call only if x->nsmpls > 1 */
void
rotate(Expr *x)
{
    int     n = x->nsmpls-1;
    Sample *q = &x->smpls[n];
    Sample *p = q - 1;
    void   *t = q->ptr;
    int	    i;

    for (i = n; i > 0; i--)
	*q-- = *p--;
    x->smpls[0].ptr = t;
}


/***********************************************************************
 * memory allocation
 ***********************************************************************/

void *
alloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL) {
	__pmNoMem("pmie.alloc", size, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    return p;
}


void *
zalloc(size_t size)
{
    void *p;

    if ((p = calloc(1, size)) == NULL) {
	__pmNoMem("pmie.zalloc", size, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    return p;
}


void *
aalloc(size_t align, size_t size)
{
    void *p;

    if ((p = memalign(align, size)) == NULL) {
	__pmNoMem("pmie.aalloc", size, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    return p;
}


void *
ralloc(void *p, size_t size)
{
    void *q;

    if ((q = realloc(p, size)) == NULL) {
	__pmNoMem("pmie.ralloc", size, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    return q;
}

char *
sdup(char *p)
{
    char *q;

    if ((q = strdup(p)) == NULL) {
	__pmNoMem("pmie.sdup", strlen(p), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    return q;
}


Expr *
newExpr(int op, Expr *arg1, Expr *arg2,
	int hdom, int idom, int tdom, int nsmpls,
	int sem)
{
    Expr *x;
    Expr *arg;

    x = (Expr *) zalloc(sizeof(Expr) + (nsmpls - 1) * sizeof(Sample));
    x->op = op;
    if (arg1) {
	x->arg1 = arg1;
	arg1->parent = x;
    }
    if (arg2) {
	x->arg2 = arg2;
	arg2->parent = x;
    }
    x->hdom = hdom;
    x->e_idom = idom;
    x->tdom = tdom;
    x->nsmpls = nsmpls;
    x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
    x->nvals = x->tspan * nsmpls;
    if (arg1) {
	arg = primary(arg1, arg2);
	x->metrics = arg->metrics;
    }
    x->units = noUnits;
    x->sem = sem;
    return x;
}


Profile *
newProfile(Fetch *owner, pmInDom indom)
{
    Profile *p = (Profile *) zalloc(sizeof(Profile));
    p->indom = indom;
    p->fetch = owner;
    return p;
}


Fetch *
newFetch(Host *owner)
{
    Fetch *f = (Fetch *) zalloc(sizeof(Fetch));
    f->host = owner;
    return f;
}


Host *
newHost(Task *owner, Symbol name)
{
    Host *h = (Host *) zalloc(sizeof(Host));

    if (!licensed && !archives) {
	static char	*first_name = NULL;
	if (first_name == NULL)
	    first_name = sdup(symName(name));
	else if (strcmp(symName(name), first_name) != 0) {
	    fprintf(stderr, "Error: %s unlicensed - cannot create multiple host contexts without a valid\n"
			    "       PCP Collector or Monitor license\n"
			    "       (hosts \"%s\" and \"%s\")\n",
			    pmProgname, first_name, symName(name));
	    exit(1);
	}
    }
    h->name = symCopy(name);
    h->task = owner;
    return h;
}


Task *
newTask(RealTime delta, int nth)
{
    Task *t = (Task *) zalloc(sizeof(Task));
    t->nth = nth;
    t->delta = delta;
    return t;
}

/* translate new metric name to internal pmid for agent mode */
static pmID
agentId(char *name)
{
    int		sts;
    pmID	pmid;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	fprintf(stderr, "%s: agentId: metric %s not found in namespace: %s\n",
		pmProgname, name, pmErrStr(sts));
	exit(1);
    }
    return pmid;
}


void
newResult(Task *t)
{
    pmResult	 *rslt;
    Symbol	 *sym;
    pmValueSet	 *vset;
    pmValueBlock *vblk;
    int		 i;
    int		 len;

    /* allocate pmResult */
    rslt = (pmResult *) zalloc(sizeof(pmResult) + (t->nrules - 1) * sizeof(pmValueSet *));
    rslt->numpmid = t->nrules;

    /* allocate pmValueSet's */
    sym = t->rules;
    for (i = 0; i < t->nrules; i++) {
	vset = (pmValueSet *)alloc(sizeof(pmValueSet));
	vset->pmid = agentId(symName(*sym));
	vset->numval = 0;
	vset->valfmt = PM_VAL_DPTR;
	vset->vlist[0].inst = PM_IN_NULL;
	len = PM_VAL_HDR_SIZE + sizeof(double);
	vblk = (pmValueBlock *)zalloc(len);
	vblk->vlen = len;
	vblk->vtype = PM_TYPE_DOUBLE;
	vset->vlist[0].value.pval = vblk;
	rslt->vset[i] = vset;
	sym++;
    }

    t->rslt = rslt;
}


/***********************************************************************
 * memory deallocation
 *
 * IMPORTANT: These functions free the argument structure plus any
 *            structures it owns below it in the expression tree.
 ***********************************************************************/

void
freeTask(Task *t)
{
    if ((t->hosts == NULL) && (t->rules == NULL)) {
	if (t->next) t->next->prev = t->prev;
	if (t->prev) t->prev->next = t->next;
	else taskq = t->next;
	free(t);
   }
}


void
freeHost(Host *h)
{
    if ((h->fetches == NULL) && (h->waits == NULL)) {
	if (h->next) h->next->prev = h->prev;
	if (h->prev) h->prev->next = h->next;
	else {
	    h->task->hosts = h->next;
	    freeTask(h->task);
	}
	symFree(h->name);
	free(h);
    }
}


void
freeFetch(Fetch *f)
{
    if (f->profiles == NULL) {
	if (f->next) f->next->prev = f->prev;
	if (f->prev) f->prev->next = f->next;
	else {
	    f->host->fetches = f->next;
	    freeHost(f->host);
	}
	pmDestroyContext(f->handle);
	if (f->result) pmFreeResult(f->result);
	if (f->pmids) free(f->pmids);
	free(f);
    }
}


void
FreeProfile(Profile *p)
{
    if (p->metrics == NULL) {
	if (p->next) p->next->prev = p->prev;
	if (p->prev) p->prev->next = p->next;
	else {
	    p->fetch->profiles = p->next;
	    freeFetch(p->fetch);
	}
	free(p);
    }
}


void
freeMetric(Metric *m)
{
    int		numinst;

    /* Metric is on fetch list */
    if (m->profile) {
	if (m->prev) {
	    m->prev->next = m->next;
	    if (m->next) m->next->prev = m->prev;
	}
	else {
	    m->host->waits = m->next;
	    if (m->next) m->next->prev = NULL;
	}
	if (m->host) freeHost(m->host);
    }

    symFree(m->hname);
    numinst =  m->specinst == 0 ? m->m_idom : m->specinst;
    if (numinst > 0 && m->inames) {
	int	i;
	for (i = 0; i < numinst; i++) {
	    if (m->inames[i] != NULL) free(m->inames[i]);
	}
	free(m->inames);
    }
    if (numinst && m->iids) free(m->iids);
    if (m->vals) free(m->vals);
    free(m);
}


void
freeExpr(Expr *x)
{
    Metric	*m;
    int		i;

    if (x) {
	if (x->arg1 && x->arg1->parent == x)
	    freeExpr(x->arg1);
	if (x->arg2 && x->arg2->parent == x)
	    freeExpr(x->arg2);
	if (x->metrics && x->op == CND_FETCH) {
	    if (x->hdom < 0) {
		/*
		 * no trips through the loop to call freeMetric(),
		 * so free partially allocated structure
		 */
		free(x->metrics);
	    }
	    else {
		for (m = x->metrics, i = 0; i < x->hdom; m++, i++)
		    freeMetric(m);
	    }
	}
	if (x->ring) free(x->ring);
	free(x);
    }
}


/***********************************************************************
 * comparison functions (for use by qsort)
 ***********************************************************************/

/* Compare two instance identifiers.
   - This function is passed as an argument to qsort, hence the casts. */
int	/* -1 less, 0 equal, 1 greater */
compid(const void *i1, const void *i2)
{
    if (*(int *)i1 < *(int *)i2) return -1;
    if (*(int *)i1 > *(int *)i2) return 1;
    return 0;
}


/* Compare two pmValue's on their inst fields
   - This function is passed as an argument to qsort, hence the casts. */
int	/* -1 less, 0 equal, 1 greater */
compair(const void *pmv1, const void *pmv2)
{
    if (((pmValue *)pmv1)->inst < ((pmValue *)pmv2)->inst) return -1;
    if (((pmValue *)pmv1)->inst > ((pmValue *)pmv2)->inst) return 1;
    return 0;
}


/***********************************************************************
 * Expr manipulation
 ***********************************************************************/

/* Decide primary argument for inheritance of Expr attributes */
Expr *
primary(Expr *arg1, Expr *arg2)
{
    if (arg2 == NULL || arg1->nvals > 1)
	return arg1;
    if (arg2->nvals > 1)
	return arg2;
    if (arg1->metrics &&
	(arg1->hdom != -1 || arg1->e_idom != -1 || arg1->tdom != -1))
	return arg1;
    if (arg2->metrics &&
	(arg2->hdom != -1 || arg2->e_idom != -1 || arg2->tdom != -1))
	return arg2;
    return arg1;
}


/* change number of samples allocated in ring buffer */
void
changeSmpls(Expr **p, int nsmpls)
{
    Expr   *x = *p;
    Metric *m;
    int    i;

    if (nsmpls == x->nsmpls) return;
    *p = x = (Expr *) ralloc(x, sizeof(Expr) + (nsmpls - 1) * sizeof(Sample));
    x->nsmpls = nsmpls;
    x->nvals = x->tspan * nsmpls;
    x->valid = 0;
    if (x->op == CND_FETCH) {
	m = x->metrics;
	for (i = 0; i < x->hdom; i++) {
	    m->expr = x;
	    m++;
	}
    }
    newRingBfr(x);
}


/* propagate instance domain, semantics and units from
   argument expressions to parents */
static void
instExpr(Expr *x)
{
    int	    up = 0;
    Expr    *arg1 = x->arg1;
    Expr    *arg2 = x->arg2;
    Expr    *arg = primary(arg1, arg2);
    pmUnits u;

    /* semantics and units */
    if (x->sem == SEM_UNKNOWN) {

	/* unary expression */
	if (arg2 == NULL) {
	    up = 1;
	    x->sem = arg1->sem;
	    x->units = arg1->units;
	}

	/* bianry expression with known args */
	else if ((arg1->sem != SEM_UNKNOWN) &&
		 (arg2->sem != SEM_UNKNOWN)) {
	    up = 1;
	    x->sem = arg->sem;
	    if (x->op == CND_MUL) {
		u.dimSpace = arg1->units.dimSpace + arg2->units.dimSpace;
		u.dimTime = arg1->units.dimTime + arg2->units.dimTime;
		u.dimCount = arg1->units.dimCount + arg2->units.dimCount;
	    }
	    else if (x->op == CND_DIV) {
		u.dimSpace = arg1->units.dimSpace - arg2->units.dimSpace;
		u.dimTime = arg1->units.dimTime - arg2->units.dimTime;
		u.dimCount = arg1->units.dimCount - arg2->units.dimCount;
	    }
	    else
		u = arg->units;
	    x->units = u;
	}

	/* binary expression with unknown arg */
	else
	    return;
    }

    /* instance domain */
    if ((x->e_idom != -1) && (x->e_idom != arg->e_idom)) {
	up = 1;
	x->e_idom = arg->e_idom;
	x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
	x->nvals = x->tspan * x->nsmpls;
	x->valid = 0;
	newRingBfr(x);
    }

    if (up && x->parent)
	instExpr(x->parent);
}


/* propagate instance domain, semantics and units from given
   fetch expression to its parents */
void
instFetchExpr(Expr *x)
{
    Metric  *m;
    int     ninst;
    int	    up = 0;
    int     i;

    /* update semantics and units */
    if (x->sem == SEM_UNKNOWN) {
	m = x->metrics;
	for (i = 0; i < x->hdom; i++) {
	    if (m->desc.sem != SEM_UNKNOWN) {
		if (m->desc.sem == PM_SEM_COUNTER) {
		    x->sem = PM_SEM_INSTANT;
		    x->units = canon(m->desc.units);
		    x->units.dimTime--;
		}
		else {
		    x->sem = m->desc.sem;
		    x->units = canon(m->desc.units);
		}
		up = 1;
		break;
	    }
	}
    }

    /*
     * update number of instances ... need to be careful because may be more
     * than one host, and instances may not be fully available ...
     *   m_idom < 0 =>	no idea how many instances there might be (cannot
     *			contact pmcd, unknown metric, can't get indom, ...)
     *   m_idom == 0 =>	no values, but otherwise OK
     *   m_idom > 0 =>	have this many values (and hence instances)
     */
    m = x->metrics;
    ninst = -1;
    for (i = 0; i < x->hdom; i++) {
	m->offset = ninst;
	if (m->m_idom >= 0) {
	    if (ninst == -1)
		ninst = m->m_idom;
	    else
		ninst += m->m_idom;
	}
	m++;
    }
    if (x->e_idom != ninst) {
	x->e_idom = ninst;
	x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
	x->nvals = x->nsmpls * x->tspan;
	x->valid = 0;
	newRingBfr(x);
	up = 1;
    }
    if (up && x->parent)
	instExpr(x->parent);
}


/***********************************************************************
 * compulsory initialization
 ***********************************************************************/

void dstructInit(void)
{
    Expr   *x;
    double zero = 0.0;

    /* not-a-number initialization */
    nan = zero / zero;

    /* initialize default host */
    gethostname(localHost, MAXHOSTNAMELEN);
    localHost[MAXHOSTNAMELEN-1] = '\0';
    dfltHost = localHost;

    /* set up symbol tables */
    symSetTable(&hosts);
    symSetTable(&metrics);
    symSetTable(&rules);
    symSetTable(&vars);

    /* set yp inter-sample interval (delta) symbol */
    symDelta = symIntern(&vars, "delta");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &delta;
    x->valid = 1;
    symValue(symDelta) = x;

    /* set up time symbols */
    symMinute = symIntern(&vars, "minute");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &minute;
    x->valid = 1;
    symValue(symMinute) = x;
    symHour = symIntern(&vars, "hour");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &hour;
    x->valid = 1;
    symValue(symHour) = x;
    symDay = symIntern(&vars, "day");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &day;
    x->valid = 1;
    symValue(symDay) = x;
    symMonth = symIntern(&vars, "month");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &month;
    x->valid = 1;
    symValue(symMonth) = x;
    symYear = symIntern(&vars, "year");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &year;
    x->valid = 1;
    symValue(symYear) = x;
    symWeekday = symIntern(&vars, "day_of_week");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUM);
    x->smpls[0].ptr = &weekday;
    x->valid = 1;
    symValue(symWeekday) = x;
}


/* get ready to run evaluator */
void
agentInit(void)
{
    Task	*t;
    int		sts;

    /* Set up local name space for agent */
    /* Only load PMNS if it's default and hence not already loaded */
    if (pmnsfile == PM_NS_DEFAULT && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: agentInit: cannot load metric namespace: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* allocate pmResult's and send pmDescs for secret agent mode */
    t = taskq;
    while (t) {
	newResult(t);
	sendDescs(t);
	t = t->next;
    }
}

/*
 * useful for diagnostics and with dbx
 */

static struct {
    void	(*addr)(Expr *);
    char	*name;
} fn_map[] = {
    { actAlarm,		"actAlarm" },
    { actAnd,		"actAnd" },
    { actArg,		"actArg" },
    { actFake,		"actFake" },
    { actOr,		"actOr" },
    { actPrint,		"actPrint" },
    { actShell,		"actShell" },
    { actSyslog,	"actSyslog" },
    { cndAdd_1_1,	"cndAdd_1_1" },
    { cndAdd_1_n,	"cndAdd_1_n" },
    { cndAdd_n_1,	"cndAdd_n_1" },
    { cndAdd_n_n,	"cndAdd_n_n" },
    { cndAll_host,	"cndAll_host" },
    { cndAll_inst,	"cndAll_inst" },
    { cndAll_time,	"cndAll_time" },
    { cndAnd_1_1,	"cndAnd_1_1" },
    { cndAnd_1_n,	"cndAnd_1_n" },
    { cndAnd_n_1,	"cndAnd_n_1" },
    { cndAnd_n_n,	"cndAnd_n_n" },
    { cndAvg_host,	"cndAvg_host" },
    { cndAvg_inst,	"cndAvg_inst" },
    { cndAvg_time,	"cndAvg_time" },
    { cndCount_host,	"cndCount_host" },
    { cndCount_inst,	"cndCount_inst" },
    { cndCount_time,	"cndCount_time" },
    { cndDelay_1,	"cndDelay_1" },
    { cndDelay_n,	"cndDelay_n" },
    { cndDiv_1_1,	"cndDiv_1_1" },
    { cndDiv_1_n,	"cndDiv_1_n" },
    { cndDiv_n_1,	"cndDiv_n_1" },
    { cndDiv_n_n,	"cndDiv_n_n" },
    { cndEq_1_1,	"cndEq_1_1" },
    { cndEq_1_n,	"cndEq_1_n" },
    { cndEq_n_1,	"cndEq_n_1" },
    { cndEq_n_n,	"cndEq_n_n" },
    { cndFall_1,	"cndFall_1" },
    { cndFall_n,	"cndFall_n" },
    { cndFetch_1,	"cndFetch_1" },
    { cndFetch_all,	"cndFetch_all" },
    { cndFetch_n,	"cndFetch_n" },
    { cndGt_1_1,	"cndGt_1_1" },
    { cndGt_1_n,	"cndGt_1_n" },
    { cndGt_n_1,	"cndGt_n_1" },
    { cndGt_n_n,	"cndGt_n_n" },
    { cndGte_1_1,	"cndGte_1_1" },
    { cndGte_1_n,	"cndGte_1_n" },
    { cndGte_n_1,	"cndGte_n_1" },
    { cndGte_n_n,	"cndGte_n_n" },
    { cndLt_1_1,	"cndLt_1_1" },
    { cndLt_1_n,	"cndLt_1_n" },
    { cndLt_n_1,	"cndLt_n_1" },
    { cndLt_n_n,	"cndLt_n_n" },
    { cndLte_1_1,	"cndLte_1_1" },
    { cndLte_1_n,	"cndLte_1_n" },
    { cndLte_n_1,	"cndLte_n_1" },
    { cndLte_n_n,	"cndLte_n_n" },
    { cndMax_host,	"cndMax_host" },
    { cndMax_inst,	"cndMax_inst" },
    { cndMax_time,	"cndMax_time" },
    { cndMin_host,	"cndMin_host" },
    { cndMin_inst,	"cndMin_inst" },
    { cndMin_time,	"cndMin_time" },
    { cndMul_1_1,	"cndMul_1_1" },
    { cndMul_1_n,	"cndMul_1_n" },
    { cndMul_n_1,	"cndMul_n_1" },
    { cndMul_n_n,	"cndMul_n_n" },
    { cndNeg_1,		"cndNeg_1" },
    { cndNeg_n,		"cndNeg_n" },
    { cndNeq_1_1,	"cndNeq_1_1" },
    { cndNeq_1_n,	"cndNeq_1_n" },
    { cndNeq_n_1,	"cndNeq_n_1" },
    { cndNeq_n_n,	"cndNeq_n_n" },
    { cndNot_1,		"cndNot_1" },
    { cndNot_n,		"cndNot_n" },
    { cndOr_1_1,	"cndOr_1_1" },
    { cndOr_1_n,	"cndOr_1_n" },
    { cndOr_n_1,	"cndOr_n_1" },
    { cndOr_n_n,	"cndOr_n_n" },
    { cndPcnt_host,	"cndPcnt_host" },
    { cndPcnt_inst,	"cndPcnt_inst" },
    { cndPcnt_time,	"cndPcnt_time" },
    { cndRate_1,	"cndRate_1" },
    { cndRate_n,	"cndRate_n" },
    { cndRise_1,	"cndRise_1" },
    { cndRise_n,	"cndRise_n" },
    { cndSome_host,	"cndSome_host" },
    { cndSome_inst,	"cndSome_inst" },
    { cndSome_time,	"cndSome_time" },
    { cndSub_1_1,	"cndSub_1_1" },
    { cndSub_1_n,	"cndSub_1_n" },
    { cndSub_n_1,	"cndSub_n_1" },
    { cndSub_n_n,	"cndSub_n_n" },
    { cndSum_host,	"cndSum_host" },
    { cndSum_inst,	"cndSum_inst" },
    { cndSum_time,	"cndSum_time" },
    { rule,		"rule" },
    { NULL,		NULL },
};

static struct {
    int			val;
    char		*name;
} sem_map[] = {
    { SEM_UNKNOWN,	"UNKNOWN" },
    { SEM_NUM,		"NUM" },
    { SEM_TRUTH,	"TRUTH" },
    { SEM_CHAR,		"CHAR" },
    { PM_SEM_COUNTER,	"COUNTER" },
    { PM_SEM_INSTANT,	"INSTANT" },
    { PM_SEM_DISCRETE,	"DISCRETE" },
    { 0,		NULL },
};

void
__dumpExpr(int level, Expr *x)
{
    int		i;
    int		j;
    int		k;

    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "Expr dump @ 0x%p\n", x);
    if (x == NULL) return;
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  op=%d (%s) arg1=0x%p arg2=0x%p parent=0x%p\n",
	x->op, opStrings(x->op), x->arg1, x->arg2, x->parent);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  eval=");
    for (j = 0; fn_map[j].addr; j++) {
	if (x->eval == fn_map[j].addr) {
	    fprintf(stderr, "%s", fn_map[j].name);
	    break;
	}
    }
    if (fn_map[j].addr == NULL)
	fprintf(stderr, "0x%p()", x->eval);
    fprintf(stderr, " metrics=0x%p ring=0x%p\n", x->metrics, x->ring);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  valid=%d cardinality[H,I,T]=[%d,%d,%d] tspan=%d\n",
	x->valid, x->hdom, x->e_idom, x->tdom, x->tspan);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  nsmpls=%d nvals=%d sem=", x->nsmpls, x->nvals);
    for (j = 0; sem_map[j].name; j++) {
	if (x->sem == sem_map[j].val) {
	    fprintf(stderr, "%s", sem_map[j].name);
	    break;
	}
    }
    if (sem_map[j].name == NULL)
	fprintf(stderr, "%d", x->sem);
    fprintf(stderr, " units=%s\n", pmUnitsStr(&x->units));
    if (x->valid > 0 &&
	(x->sem == SEM_TRUTH || x->sem == SEM_CHAR || x->sem == SEM_NUM ||
	 x->sem == PM_SEM_COUNTER || x->sem == PM_SEM_INSTANT ||
	 x->sem == PM_SEM_DISCRETE)) {
	for (j = 0; j < x->nsmpls; j++) {
	    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
	    fprintf(stderr, "  smpls[%d].ptr 0x%p ", j, x->smpls[j].ptr);
	    for (k = 0; k < x->tspan; k++) {
		if (x->tspan > 1) {
		    if (k > 0)
			fprintf(stderr, ", ");
		    fprintf(stderr, "{%d} ", k);
		}
		if (x->sem == SEM_TRUTH) {
		    char 	c = *((char *)x->smpls[j].ptr+k);
		    if ((int)c == TRUE)
			fprintf(stderr, "true");
		    else if ((int)c == FALSE)
			fprintf(stderr, "false");
		    else if ((int)c == DUNNO)
			fprintf(stderr, "unknown");
		    else
			fprintf(stderr, "bogus (0x%x)", c & 0xff);
		}
		else if (x->sem == SEM_CHAR) {
		    char 	c = *((char *)x->smpls[j].ptr+k);
		    if (isprint(c))
			fprintf(stderr, "'%c'", c);
		    else
			fprintf(stderr, "0x%x", c & 0xff);
		}
		else {
		    double	v = *((double *)x->smpls[j].ptr+k);
		    if (isnand(v))
			fputc('?', stderr);
		    else
			fprintf(stderr, "%g", v);
		}
	    }
	    fputc('\n', stderr);
	}
    }
}

void
__dumpMetric(int level, Metric *m)
{
    int		i;
    int		j;
    int		numinst;

    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "Metric dump @ 0x%p\n", m);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "expr=0x%p profile=0x%p host=0x%p next=0x%p prev=0x%p\n",
	m->expr, m->profile, m->host, m->next, m->prev);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "metric=%s host=%s conv=%g specinst=%d m_indom=%d\n",
	symName(m->mname), symName(m->hname), m->conv, m->specinst, m->m_idom);
    if (m->desc.indom != PM_INDOM_NULL) {
	numinst =  m->specinst == 0 ? m->m_idom : m->specinst;
	for (j = 0; j < numinst; j++) {
	    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
	    fprintf(stderr, "[%d] iid=", j);
	    if (m->iids[j] == PM_IN_NULL) 
		fprintf(stderr, "?missing");
	    else
		fprintf(stderr, "%d", m->iids[j]);
	    fprintf(stderr, " iname=\"%s\"\n", m->inames[j]);
	}
    }
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fputc('\n', stderr);

#if 0
    pmDesc          desc;       /* pmAPI metric description */
    RealTime	    stamp;	/* time stamp for current values */
    pmValueSet	    *vset;	/* current values */
    RealTime	    stomp;	/* previous time stamp for rate calculation */
    double	    *vals;	/* vector of values for rate computation */
    int		    offset;	/* offset within sample in expr ring buffer */
... Metric;
#endif

}


void
__dumpTree(int level, Expr *x)
{
    __dumpExpr(level, x);
    if (x->arg1 != NULL) __dumpTree(level+1, x->arg1);
    if (x->arg2 != NULL) __dumpTree(level+1, x->arg2);
}

void
dumpTree(Expr *x)
{
    __dumpTree(0, x);
}

void
dumpRules(void)
{
    Task	*t;
    Symbol	*s;
    int		i;

    for (t = taskq; t != NULL; t = t->next) {
	s = t->rules;
	for (i = 0; i < t->nrules; i++, s++) {
	    fprintf(stderr, "\nRule: %s\n", symName(*s));
	    dumpTree((Expr *)symValue(*s));
	}
    }
}

void
dumpExpr(Expr *x)
{
    __dumpExpr(0, x);
}

void
dumpMetric(Metric *m)
{
    __dumpMetric(0, m);
}
