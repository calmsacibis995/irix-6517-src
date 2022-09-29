/*
 * pragmatics.c - inference engine pragmatics analysis
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
 * 
 * 
 * The analysis of how to organize the fetching of metrics (pragmatics),
 * and any other parts of the inference engine sensitive to details of
 * the PMAPI access are kept in this source file.
 */

#ident "$Id: pragmatics.c,v 1.4 1999/08/16 23:13:49 kenmcd Exp $"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "pmapi.h"
#include "impl.h"
#include "dstruct.h"
#include "eval.h"
#include "pragmatics.h"

int	dfltConn;	/* default context type */

/* for initialization of pmUnits struct */
pmUnits noUnits    = {0, 0, 0, 0, 0, 0};
pmUnits countUnits = {0, 0, 1, 0, 0, 0};

char *
findsource(char *host)
{
    static char	buf[MAXPATHLEN+MAXHOSTNAMELEN+30];

    if (archives) {
	Archive	*a = archives;
	while (a) {	/* find archive for host */
	    if (strcmp(host, a->hname) == 0)
		break;
	    a = a->next;
	}
	if (a)
	    sprintf(buf, "archive %s (host %s)", a->fname, host);
	else
	    sprintf(buf, "host %s in unknown archive!", host);
    }
    else
	sprintf(buf, "host %s", host);

    return buf;
}

/***********************************************************************
 * PMAPI context creation & destruction
 ***********************************************************************/

int	/* > 0: context handle,  -1: retry later */
newContext(char *host)
{
    Archive	*a;
    int		sts = -1;

    if (!licensed && archives) {
	fprintf(stderr, "Error: %s unlicensed - cannot create an archive "
			"context without a valid PCP\n"
			"       Collector or Monitor license.", pmProgname);
	exit(1);
    }
    if (archives) {
	a = archives;
	while (a) {	/* find archive for host */
	    if (strcmp(host, a->hname) == 0)
		break;
	    a = a->next;
	}
	if (a) {	/* archive found */
	    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, a->fname)) < 0) {
		fprintf(stderr, "%s: cannot open archive %s\n",
			pmProgname, a->fname);
		fprintf(stderr, "pmNewContext: %s\n", pmErrStr(sts));
		exit(1);
	    }
	}
	else {		/* no archive for host */
	    fprintf(stderr, "%s: no archive for host %s\n", pmProgname, host);
	    exit(1);
	}
    }
    else if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	if (host_state_changed(host, STATE_FAILINIT) == 1) {
	    if (sts == -ECONNREFUSED)
		fprintf(stderr, "%s: warning - pmcd "
			"on host %s does not respond\n",
			pmProgname, host);
	    else if (sts == PM_ERR_PERMISSION)
		fprintf(stderr, "%s: warning - host %s does not "
			"permit delivery of metrics to the local host\n",
			pmProgname, host);
	    else if (sts == PM_ERR_CONNLIMIT)
		fprintf(stderr, "%s: warning - pmcd "
			"on host %s has exceeded its connection limit\n",
			pmProgname, host);
	    else
		fprintf(stderr, "%s: warning - host %s is unreachable\n", 
			pmProgname, host);
	}
	sts = -1;
    }
    return sts;
}


/***********************************************************************
 * instance profile
 ***********************************************************************/

/* equality (not symmetric) of instance names */
static int
eqinst(char *i1, char *i2)
{
    int		n1 = (int) strlen(i1);
    int		n2 = (int) strlen(i2);

    /* test equality of first word */
    if ((strncmp(i1, i2, n1) == 0) && ((n1 == n2) || isspace(i2[n1])))
	return 1;

    do {	/* skip over first word */
	i2++;
	n2--;
	if (n2 < n1)
	    return 0;
    } while (! isspace(*i2));

    do {	/* skip over spaces */
	i2++;
	n2--;
	if (n2 < n1)
	    return 0;
    } while (isspace(*i2));

    /* test equality of second word */
    if ((strncmp(i1, i2, n1) == 0) && ((n1 == n2) || isspace(i2[n1])))
	return 1;
    return 0;
}


/***********************************************************************
 * task queue
 ***********************************************************************/

/* find Task for new rule */
static Task *
findTask(RealTime delta)
{
    Task	*t, *u;
    int		n = 0;

    t = taskq;
    if (t) {
	while (t->next) {	/* find last task in queue */
	    t = t->next;
	    n++;
	}

	/* last task in queue has same delta */
	if (t->delta == delta)
	    return t;
    }

    u = newTask(delta, n);	/* create new Task */
    if (t) {
	t->next = u;
	u->prev = t;
    }
    else
	taskq = u;
    return u;
}


/***********************************************************************
 * wait list
 ***********************************************************************/

/* put Metric onto wait list */
void
waitMetric(Metric *m)
{
    Host *h = m->host;

    m->next = h->waits;
    m->prev = NULL;
    if (h->waits) h->waits->prev = m;
    h->waits = m;
}

/* remove Metric from wait list */
void
unwaitMetric(Metric *m)
{
    if (m->prev) m->prev->next = m->next;
    else m->host->waits = m->next;
    if (m->next) m->next->prev = m->prev;
}


/***********************************************************************
 * fetch list
 ***********************************************************************/

/* find Host for Metric */
static Host *
findHost(Task *t, Metric *m)
{
    Host	*h;

    h = t->hosts;
    while (h) {			/* look for existing host */
	if (h->name == m->hname)
	    return h;
	h = h->next;
    }

    h = newHost(t, m->hname);	/* add new host */
    if (t->hosts) {
	h->next = t->hosts;
	t->hosts->prev = h;
    }
    t->hosts = h;
    return h;
}

/* helper function for Extended Time Base */
static void
getDoubleAsXTB(double *realtime, int *ival, int *mode)
{
#define SECS_IN_24_DAYS 2073600.0

    if (*realtime > SECS_IN_24_DAYS) {
        *ival = (int)*realtime;
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_SEC);
    }
    else {
	*ival = (int)(*realtime * 1000.0);
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_MSEC);
    }
}


/* find Fetch bundle for Metric */
static Fetch *
findFetch(Host *h, Metric *m)
{
    Fetch	    *f;
    int		    sts;
    int		    i;
    int		    n;
    pmID	    pmid = m->desc.pmid;
    pmID	    *p;
    struct timeval  tv;

    /* find existing Fetch bundle */
    f = h->fetches;

    /* create new Fetch bundle */
    if (! f) {
	f = newFetch(h);
	if ((sts = newContext(symName(h->name))) < 0) {
	    free(f);
	    h->down = 1;
	    return NULL;
	}
	if (archives) {
	    int tmp_ival;
	    int tmp_mode = PM_MODE_INTERP;
	    getDoubleAsXTB(&h->task->delta, &tmp_ival, &tmp_mode);

	    tv.tv_sec = (time_t)start;
	    tv.tv_usec = (int)((start - tv.tv_sec) * 1000000.0);
	    if ((sts = pmSetMode(tmp_mode, &tv, tmp_ival)) < 0) {
		fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname,
			pmErrStr(sts));
		exit(1);
	    }
	}
	f->handle = sts;
	f->next = NULL;
	f->prev = NULL;
	h->fetches = f;
    }

    /* look for existing pmid */
    p = f->pmids;
    n = f->npmids;
    for (i = 0; i < n; i++) {
	if (*p == pmid) break;
	p++;
    }

    /* add new pmid */
    if (i == n) {
	p = f->pmids;
	p = ralloc(p, (n+1) * sizeof(pmID));
	p[n] = pmid;
	f->npmids = n + 1;
	f->pmids = p;
    }

    return f;
}


/* find Profile for Metric */
static Profile *
findProfile(Fetch *f, Metric *m)
{
    Profile	*p;
    int		sts;

    /* find existing Profile */
    p = f->profiles;
    while (p) {
	if (p->indom == m->desc.indom) {
	    m->next = p->metrics;
	    if (p->metrics) p->metrics->prev = m;
	    p->metrics = m;
	    break;
	}
	p = p->next;
    }

    /* create new Profile */
    if (p == NULL) {
	m->next = NULL;
	p = newProfile(f, m->desc.indom);
	p->next = f->profiles;
	if (f->profiles) f->profiles->prev = p;
	f->profiles = p;
	p->metrics = m;
    }

    /* add instances required by Metric to Profile */
    if ((sts = pmUseContext(f->handle)) < 0) {
	fprintf(stderr, "%s: pmUseContext failed: %s\n", pmProgname,
		pmErrStr(sts));
	exit(1);
    }
    if (m->specinst == 0)
	sts = pmAddProfile(p->indom, 0, (int *)0);
    else
	sts = pmAddProfile(p->indom, m->m_idom, m->iids);

    if (sts < 0) {
	fprintf(stderr, "%s: pmAddProfile failed: %s\n", pmProgname,
		pmErrStr(sts));
	exit(1);
    }

    m->profile = p;
    return p;
}


/* organize fetch bundling for given expression */
static void
bundle(Task *t, Expr *x)
{
    Metric	*m;
    Host	*h;
    int		i;

    if (x->op == CND_FETCH) {
	m = x->metrics;
	for (i = 0; i < x->hdom; i++) {
	    h = findHost(t, m);
	    m->host = h;
	    if (m->conv)	/* initialized Metric */
		bundleMetric(h, m);
	    else		/* uninitialized Metric */
		waitMetric(m);
	    m++;
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    fprintf(stderr, "bundle: task 0x%p expr ...\n", t);
	    __dumpExpr(1, x);
	    m = x->metrics;
	    for (i = 0; i < x->hdom; i++) {
		__dumpMetric(2, m);
		m++;
	    }
	}
#endif
    }
    else {
	if (x->arg1) {
	    bundle(t, x->arg1);
	    if (x->arg2)
		bundle(t, x->arg2);
	}
    }
}


/***********************************************************************
 * secret agent mode support
 ***********************************************************************/

/* send pmDescriptor for the given Expr as a binary PDU */
static void
sendDesc(Expr *x, pmValueSet *vset)
{
    pmDesc	d;

    d.pmid = vset->pmid;
    d.indom = PM_INDOM_NULL;
    if (x->sem != SEM_UNKNOWN) {
	d.type = PM_TYPE_DOUBLE;
	d.sem = x->sem;
	d.units = x->units;
    }
    else {
	d.type = PM_TYPE_NOSUPPORT;
	d.sem = PM_SEM_INSTANT;
	d.units = noUnits;
    }
    __pmSendDesc(STDOUT_FILENO, PDU_BINARY, &d);
}


/***********************************************************************
 * exported functions
 ***********************************************************************/

/* initialize access to archive */
int
initArchive(Archive *a)
{
    pmLogLabel	    label;
    struct timeval  tv;
    int		    sts;
    int		    handle;
    int		    n;
    Archive	    *b;

    /* setup temorary context for the archive */
    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, a->fname)) < 0) {
	fprintf(stderr, "%s: cannot open archive %s\n"
		"pmNewContext failed: %s\n",
		pmProgname, a->fname, pmErrStr(sts));
	return 0;
    }
    handle = sts;

    /* get the goodies from archive label */
    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: cannot read label from archive %s\n"
			"pmGetArchiveLabel failed: %s\n", 
			pmProgname, a->fname, pmErrStr(sts));
	pmDestroyContext(handle);
	return 0;
    }
    n = (int) strlen(label.ll_hostname);
    a->hname = (char *) alloc(n + 1);
    strcpy(a->hname, label.ll_hostname);
    a->first = realize(label.ll_start);
    if ((sts = pmGetArchiveEnd(&tv)) < 0) {
	fprintf(stderr, "%s: archive %s is corrupted\n"
		"pmGetArchiveEnd failed: %s\n",
		pmProgname, a->fname, pmErrStr(sts));
	pmDestroyContext(handle);
	return 0;
    }
    a->last = realize(tv);

    /* check for duplicate host */
    b = archives;
    while (b) {
	if (strcmp(a->hname, b->hname) == 0) {
	    fprintf(stderr, "%s: archive %s ignored - archive %s is already open "
		    "for host %s\n", pmProgname, a->fname, b->fname, a->hname);
	    pmDestroyContext(handle);
	    return 0;
	}
    }

    /* put archive record on the archives list */
    a->next = archives;
    archives = a;

    /* update first and last available data points */
    if (a->first < first)
	first = a->first;
    if (a->last > last)
	last = a->last;

    pmDestroyContext(handle);
    return 1;
}


/* initialize timezone */
void
zoneInit(void)
{
    int		sts;
    int		handle = -1;
    Archive	*a;

    if (timeZone) {			/* TZ from timezone string */
	if ((sts = pmNewZone(timeZone)) < 0)
	    fprintf(stderr, "%s: cannot set timezone to %s\n"
		    "pmNewZone failed: %s\n", pmProgname, timeZone,
		    pmErrStr(sts));
    }
    else if (! archives && hostZone) {	/* TZ from live host */
	if ((handle = pmNewContext(PM_CONTEXT_HOST, dfltHost)) < 0)
	    fprintf(stderr, "%s: cannot set timezone from %s\n"
		    "pmNewContext failed: %s\n", pmProgname,
		    findsource(dfltHost), pmErrStr(handle));
	else if ((sts = pmNewContextZone()) < 0)
	    fprintf(stderr, "%s: cannot set timezone from %s\n"
		    "pmNewContextZone failed: %s\n", pmProgname,
		    findsource(dfltHost), pmErrStr(sts));
	else
	    fprintf(stdout, "%s: timezone set to local timezone of host %s\n",
		    pmProgname, dfltHost);
	if (handle >= 0)
	    pmDestroyContext(handle);
    }
    else if (hostZone) {		/* TZ from an archive */
	a = archives;
	while (a) {
	    if (strcmp(dfltHost, a->hname) == 0)
		break;
	    a = a->next;
	}
	if (! a)
	    fprintf(stderr, "%s: no archive supplied for host %s\n",
		    pmProgname, dfltHost);
	else if ((handle = pmNewContext(PM_CONTEXT_ARCHIVE, a->fname)) < 0)
	    fprintf(stderr, "%s: cannot set timezone from %s\npmNewContext failed: %s\n",
		    pmProgname, findsource(dfltHost), pmErrStr(handle));
	else if ((sts = pmNewContextZone()) < 0)
	    fprintf(stderr, "%s: cannot set timezone from %s\n"
		    "pmNewContextZone failed: %s\n",
		    pmProgname, findsource(dfltHost), pmErrStr(sts));
	else
	    fprintf(stdout, "%s: timezone set to local timezone of host %s\n",
		    pmProgname, dfltHost);
	if (handle >= 0)
	    pmDestroyContext(handle);
    }
}


/* convert to canonical units */
pmUnits
canon(pmUnits in)
{
    static pmUnits	out;

    out = in;
    out.scaleSpace = PM_SPACE_BYTE;
    out.scaleTime = PM_TIME_SEC;
    out.scaleCount = 0;
    return out;
}


/* scale factor to canonical pmUnits */
double
scale(pmUnits in)
{
    double	f;

    /* scale space to Mbyte */
    f = pow(1024, in.dimSpace * (in.scaleSpace - PM_SPACE_BYTE));

    /* scale time to seconds  */
    if (in.scaleTime > PM_TIME_SEC)
	f *= pow(60, in.dimTime * (in.scaleTime - PM_TIME_SEC));
    else
	f *= pow(1000, in.dimTime * (in.scaleTime - PM_TIME_SEC));

    /* scale events to millions of events */
    f *= pow(10, in.dimCount * in.scaleCount);

    return f;
}


/* initialize Metric */
int      /* 1: ok, 0: try again later, -1: fail */
initMetric(Metric *m)
{
    char	*hname = symName(m->hname);
    char	*mname = symName(m->mname);
    char	**inames;
    int		*iids;
    int		handle;
    int		ret = 1;
    int		sts;
    int		i, j;

    /* set up temporary context */
    if ((handle = newContext(hname)) < 0)
	return 0;

    host_state_changed(hname, STATE_RECONN);

    if ((sts = pmLookupName(1, &mname, &m->desc.pmid)) < 0) {
	fprintf(stderr, "%s: metric %s not in namespace for %s\n"
		"pmLookupName failed: %s\n",
		pmProgname, mname, findsource(hname), pmErrStr(sts));
	ret = 0;
	goto end;
    }

    /* fill in performance metric descriptor */
    if ((sts = pmLookupDesc(m->desc.pmid, &m->desc)) < 0) {
	fprintf(stderr, "%s: metric %s not currently available from %s\n"
		"pmLookupDesc failed: %s\n",
		pmProgname, mname, findsource(hname), pmErrStr(sts));
	ret = 0;
	goto end;
    }

    if (m->desc.type == PM_TYPE_STRING ||
	m->desc.type == PM_TYPE_AGGREGATE ||
	m->desc.type == PM_TYPE_AGGREGATE_STATIC ||
	m->desc.type == PM_TYPE_UNKNOWN) {
	fprintf(stderr, "%s: metric %s has non-numeric type\n", pmProgname, mname);
	ret = -1;
    }
    else if (m->desc.indom == PM_INDOM_NULL) {
	if (m->specinst != 0) {
	    fprintf(stderr, "%s: metric %s has no instances\n", pmProgname, mname);
	    ret = -1;
	}
	else
	    m->m_idom = 1;
    }
    else {
	/* metric has instances, get full instance profile */
	if (archives) {
	    if ((sts = pmGetInDomArchive(m->desc.indom, &iids, &inames)) < 0) {
		fprintf(stderr, "Metric %s from %s - instance domain not "
			"available in archive\npmGetInDomArchive failed: %s\n",
			mname, findsource(hname), pmErrStr(sts));
		ret = -1;
	    }
	}
	else if ((sts = pmGetInDom(m->desc.indom, &iids, &inames)) < 0) {
	    fprintf(stderr, "Instance domain for metric %s from %s not (currently) available\n"
		    "pmGetIndom failed: %s\n", mname, findsource(hname), pmErrStr(sts));
	    ret = 0;
	}

	if (ret == 1) {	/* got instance profile */
	    if (m->specinst == 0) {
		/* all instances */
		m->iids = iids;
		m->m_idom = sts;
		m->inames = alloc(m->m_idom*sizeof(char *));
		for (i = 0; i < m->m_idom; i++) {
		    m->inames[i] = sdup(inames[i]);
		}
	    }
	    else {
		/* selected instances only */
		m->m_idom = 0;
		for (i = 0; i < m->specinst; i++) {
		    /* look for first matching instance name */
		    for (j = 0; j < sts; j++) {
			if (eqinst(m->inames[i], inames[j])) {
			    m->iids[i] = iids[j];
			    m->m_idom++;
			    break;
			}
		    }
		    if (j == sts) {
			__pmNotifyErr(LOG_ERR, "metric %s from %s does not "
				"(currently) have instance \"%s\"\n",
				mname, findsource(hname), m->inames[i]);
			m->iids[i] = PM_IN_NULL;
			ret = 0;
		    }
		}
		free(iids);

		/* 
		 * if specinst != m_idom, then some not found ... move these
		 * to the end of the list
		 */
		for (j = m->specinst-1; j >= 0; j--) {
		    if (m->iids[j] != PM_IN_NULL)
			break;
		}
		for (i = 0; i < j; i++) {
		    if (m->iids[i] == PM_IN_NULL) {
			/* need to swap */
			char	*tp;
			tp = m->inames[i];
			m->inames[i] = m->inames[j];
			m->iids[i] = m->iids[j];
			m->inames[j] = tp;
			m->iids[j] = PM_IN_NULL;
			j--;
		    }
		}
	    }

#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		int	numinst;
		fprintf(stderr, "initMetric: %s from %s: instance domain specinst=%d\n",
			mname, hname, m->specinst);
		if (m->m_idom < 1) fprintf(stderr, "  %d instances!\n", m->m_idom);
		numinst =  m->specinst == 0 ? m->m_idom : m->specinst;
		for (i = 0; i < numinst; i++) {
		    fprintf(stderr, "  indom[%d]", i);
		    if (m->iids[i] == PM_IN_NULL) 
			fprintf(stderr, " ?missing");
		    else
			fprintf(stderr, " %d", m->iids[i]);
		    fprintf(stderr, " \"%s\"\n", m->inames[i]);
		}
	    }
#endif
	    free(inames);
	}
    }

    if (ret == 1) {
	/* compute conversion factor into canonical units
	   - non-zero conversion factor flags initialized metric */
	m->conv = scale(m->desc.units);

	/* automatic rate computation */
	if (m->desc.sem == PM_SEM_COUNTER) {
	    m->vals = (double *) ralloc(m->vals, m->m_idom * sizeof(double));
	    for (j = 0; j < m->m_idom; j++)
		m->vals[j] = 0;
	}
    }

end:
    /* destroy temporary context */
    pmDestroyContext(handle);

    /* retry not meaningful for archives */
    if (archives && (ret == 0))
	ret = -1;

    return ret;
}


/* reinitialize Metric - only for live host */
int      /* 1: ok, 0: try again later, -1: fail */
reinitMetric(Metric *m)
{
    char	*hname = symName(m->hname);
    char	*mname = symName(m->mname);
    char	**inames;
    int		*iids;
    int		handle;
    int		ret = 1;
    int		sts;
    int		i, j;

    /* set up temporary context */
    if ((handle = newContext(hname)) < 0)
	return 0;

    host_state_changed(hname, STATE_RECONN);

    if ((sts = pmLookupName(1, &mname, &m->desc.pmid)) < 0) {
	ret = 0;
	goto end;
    }

    /* fill in performance metric descriptor */
    if ((sts = pmLookupDesc(m->desc.pmid, &m->desc)) < 0) {
	ret = 0;
        goto end;
    }

    if (m->desc.type == PM_TYPE_STRING ||
	m->desc.type == PM_TYPE_AGGREGATE ||
	m->desc.type == PM_TYPE_AGGREGATE_STATIC ||
	m->desc.type == PM_TYPE_UNKNOWN) {
	fprintf(stderr, "%s: metric %s has non-numeric type\n", pmProgname, mname);
	ret = -1;
    }
    else if (m->desc.indom == PM_INDOM_NULL) {
	if (m->specinst != 0) {
	    fprintf(stderr, "%s: metric %s has no instances\n", pmProgname, mname);
	    ret = -1;
	}
	else
	    m->m_idom = 1;
    }
    else {
	if ((sts = pmGetInDom(m->desc.indom, &iids, &inames)) < 0) { /* full profile */
	    ret = 0;
	}
	else {
	    if (m->specinst == 0) {
		/* all instances */
		m->iids = iids;
		m->m_idom = sts;
		m->inames = alloc(m->m_idom*sizeof(char *));
		for (i = 0; i < m->m_idom; i++) {
		    m->inames[i] = sdup(inames[i]);
		}
	    }
	    else {
		/* explicit instance profile */
		m->m_idom = 0;
		for (i = 0; i < m->specinst; i++) {
		    /* look for first matching instance name */
		    for (j = 0; j < sts; j++) {
			if (eqinst(m->inames[i], inames[j])) {
			    m->iids[i] = iids[j];
			    m->m_idom++;
			    break;
			}
		    }
		    if (j == sts) {
			m->iids[i] = PM_IN_NULL;
			ret = 0;
		    }
		}
		free(iids);

		/* 
		 * if specinst != m_idom, then some not found ... move these
		 * to the end of the list
		 */
		for (j = m->specinst-1; j >= 0; j--) {
		    if (m->iids[j] != PM_IN_NULL)
			break;
		}
		for (i = 0; i < j; i++) {
		    if (m->iids[i] == PM_IN_NULL) {
			/* need to swap */
			char	*tp;
			tp = m->inames[i];
			m->inames[i] = m->inames[j];
			m->iids[i] = m->iids[j];
			m->inames[j] = tp;
			m->iids[j] = PM_IN_NULL;
			j--;
		    }
		}
	    }

#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		int	numinst;
		fprintf(stderr, "reinitMetric: %s from %s: instance domain specinst=%d\n",
			mname, hname, m->specinst);
		if (m->m_idom < 1) fprintf(stderr, "  %d instances!\n", m->m_idom);
		if (m->specinst == 0) numinst = m->m_idom;
		else numinst = m->specinst;
		for (i = 0; i < numinst; i++) {
		    fprintf(stderr, "  indom[%d]", i);
		    if (m->iids[i] == PM_IN_NULL) 
			fprintf(stderr, " ?missing");
		    else
			fprintf(stderr, " %d", m->iids[i]);
		    fprintf(stderr, " \"%s\"\n", m->inames[i]);
		}
	    }
#endif
	    free(inames);
	}
    }

    if (ret == 1) {
	/* compute conversion factor into canonical units
	   - non-zero conversion factor flags initialized metric */
	m->conv = scale(m->desc.units);

	/* automatic rate computation */
	if (m->desc.sem == PM_SEM_COUNTER) {
	    m->vals = (double *) ralloc(m->vals, m->m_idom * sizeof(double));
	    for (j = 0; j < m->m_idom; j++)
		m->vals[j] = 0;
	}
    }

    if (ret >= 0) {
	/*
	 * re-shape, starting here are working up the expression until
	 * we reach the top of the tree or the designated metrics
	 * associated with the node are not the same
	 */
	Expr	*x = m->expr;
	while (x) {
	    /*
	     * only re-shape expressions that may have set values
	     */
	    if (x->op == CND_FETCH ||
		x->op == CND_NEG || x->op == CND_ADD || x->op == CND_SUB ||
		x->op == CND_MUL || x->op == CND_DIV ||
		x->op == CND_SUM_HOST || x->op == CND_SUM_INST ||
		x->op == CND_SUM_TIME ||
		x->op == CND_AVG_HOST || x->op == CND_AVG_INST ||
		x->op == CND_AVG_TIME ||
		x->op == CND_MAX_HOST || x->op == CND_MAX_INST ||
		x->op == CND_MAX_TIME ||
		x->op == CND_MIN_HOST || x->op == CND_MIN_INST ||
		x->op == CND_MIN_TIME ||
		x->op == CND_EQ || x->op == CND_NEQ ||
		x->op == CND_LT || x->op == CND_LTE ||
		x->op == CND_GT || x->op == CND_GTE ||
		x->op == CND_NOT || x->op == CND_AND || x->op == CND_OR ||
		x->op == CND_RISE || x->op == CND_FALL ||
		x->op == CND_MATCH || x->op == CND_NOMATCH) {
		instFetchExpr(x);
		findEval(x);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1) {
		    fprintf(stderr, "reinitMetric: re-shaped ...\n");
		    dumpExpr(x);
		}
#endif
	    }
	    if (x->parent) {
		x = x->parent;
		if (x->metrics == m)
		    continue;
	    }
	    break;
	}
    }

end:
    /* destroy temporary context */
    pmDestroyContext(handle);

    return ret;
}


/* put initialised Metric onto fetch list */
void
bundleMetric(Host *h, Metric *m)
{
    Fetch	*f = findFetch(h, m);
    if (f == NULL) {
	/*
	 * creating new fetch bundle and pmNewContext failed ...
	 * not much choice here
	 */
	waitMetric(m);
    }
    else
	/* usual case */
	findProfile(findFetch(h, m), m);
}


/* reconnect attempt to host */
int
reconnect(Host *h)
{
    Fetch	*f;

    f = h->fetches;
    while (f) {
	if (pmReconnectContext(f->handle) < 0)
	    return 0;
	f = f->next;
    }
    return 1;
}


/* pragmatics analysis */
void
pragmatics(Symbol rule, RealTime delta)
{
    Expr	*x = symValue(rule);
    Task	*t;

    if (x->op != NOP) {
	t = findTask(delta);
	bundle(t, x);
	t->nrules++;
	t->rules = (Symbol *) ralloc(t->rules, t->nrules * sizeof(Symbol));
	t->rules[t->nrules-1] = symCopy(rule);
	perf->eval_expected += (float)1/delta;
    }
}

/*
 * find all expressions for a host that has just been marked "down"
 * and invalidate them
 */
static void
mark_all(Host *hdown)
{
    Task	*t;
    Symbol	*s;
    Metric	*m;
    Expr	*x;
    int		i;

    for (t = taskq; t != NULL; t = t->next) {
	s = t->rules;
	for (i = 0; i < t->nrules; i++, s++) {
	    x = (Expr *)symValue(*s);
	    for (m = x->metrics; m != NULL; m = m->next) {
		if (m->host == hdown)
		    clobber(x);
	    }
	}
    }
}

/* execute fetches for given Task */
void
taskFetch(Task *t)
{
    Host	*h;
    Fetch	*f;
    Profile	*p;
    Metric	*m;
    pmResult	*r;
    pmValueSet	**v;
    int		i;
    int		sts;

    /* do all fetches, quick as you can */
    h = t->hosts;
    while (h) {
	f = h->fetches;
	while (f) {
	    if (f->result) pmFreeResult(f->result);
	    if (! h->down) {
		pmUseContext(f->handle);
		if ((sts = pmFetch(f->npmids, f->pmids, &f->result)) < 0) {
		    if (! archives) {
			__pmNotifyErr(LOG_ERR, "pmFetch from %s failed: %s\n",
				symName(f->host->name), pmErrStr(sts));
			host_state_changed(symName(f->host->name), STATE_LOSTCONN);
			h->down = 1;
			mark_all(h);
		    }
		    f->result = NULL;
		}
	    }
	    else
		f->result = NULL;
	    f = f->next;
	}
	h = h->next;
    }

    /* sort and distribute pmValueSets to requesting Metrics */
    h = t->hosts;
    while (h) {
	if (! h->down) {
	    f = h->fetches;
	    while (f && (r = f->result)) {
		/* sort all vlists in result r */
		v = r->vset;
		for (i = 0; i < r->numpmid; i++) {
		    if ((*v)->numval > 0) {
			qsort((*v)->vlist, (size_t)(*v)->numval,
			      sizeof(pmValue), compair);
		    }
		    v++;
		}

		/* distribute pmValueSets to Metrics */
		p = f->profiles;
		while (p) {
		    m = p->metrics;
		    while (m) {
			for (i = 0; i < r->numpmid; i++) {
			    if (m->desc.pmid == r->vset[i]->pmid) {
				if (r->vset[i]->numval > 0) {
				    m->vset = r->vset[i];
				    m->stamp = realize(r->timestamp);
				}
				break;
			    }
			}
			m = m->next;
		    }
		    p = p->next;
		}
		f = f->next;
	    }
	}
	h = h->next;
    }
}


/* send pmDescriptors for all expressions in given task */
void
sendDescs(Task *task)
{
    Symbol	*s;
    int		i;

    s = task->rules;
    for (i = 0; i < task->nrules; i++) {
	sendDesc(symValue(*s), task->rslt->vset[i]);
	s++;
    }
}


/* convert Expr value to pmValueSet value */
void
fillVSet(Expr *x, pmValueSet *vset)
{
    if (x->valid > 0) {
	if (finite(*((double *)x->ring))) {	/* copy value */
	    vset->numval = 1;
	    memcpy(&vset->vlist[0].value.pval->vbuf, x->ring, sizeof(double));
	}
	else	/* value not representable */
	    vset->numval = 0;
    }
    else {	/* value not available */
	vset->numval = PM_ERR_VALUE;
    }
}
