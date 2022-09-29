/***********************************************************************
 * eval.c - task scheduling and expression evaluation
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

#ident "$Id: eval.c,v 1.2 1999/08/16 23:13:49 kenmcd Exp $"

#include <limits.h>
#include <sys/syslog.h>
#include "dstruct.h"
#include "eval.h"
#include "fun.h"
#include "pragmatics.h"
#include "show.h"

/***********************************************************************
 * scheduling
 ***********************************************************************/

/* enter Task into task queue */
static void
enque(Task *t)
{
    Task	*q;
    RealTime	tt;
    RealTime	qt;

    q = taskq;
    if (q == NULL) {
	taskq = t;
	t->next = NULL;
	t->prev = NULL;
    }
    else {
	tt = (t->retry && t->retry < t->eval) ? t->retry : t->eval;
	while (q) {
	    qt = (q->retry && q->retry < q->eval) ? q->retry : q->eval;
	    if (tt <= qt) {
		t->next = q;
		t->prev = q->prev;
		if (q->prev) q->prev->next = t;
		else taskq = t;
		q->prev = t;
		break;
	    }
	    if (q->next == NULL) {
		q->next = t;
		t->next = NULL;
		t->prev = q;
		break;
	    }
	    q = q->next;
	}
    }
}


/***********************************************************************
 * reconnect
 ***********************************************************************/

/* any hosts down or unavailable metrics in this task? */
static int
waiting(Task *t)
{
    Host        *h;

    h = t->hosts;
    while (h) {
	if (h->down || h->waits)
	    return 1;
	h = h->next;
    }
    return 0;
}

/*
 * state values
 *	STATE_INIT
 *	STATE_FAILINIT
 *	STATE_RECONN
 *	STATE_LOSTCONN
 */

typedef struct hstate {
    struct hstate	*next;
    char		*name;
    int			state;
} hstate_t;

static hstate_t	*host_map = NULL;
    
int
host_state_changed(char *host, int state)
{
    hstate_t	*hsp;

    for (hsp = host_map; hsp != NULL; hsp = hsp->next) {
	if (strcmp(host, hsp->name) == 0)
	    break;
    }

    if (hsp == NULL) {
	hsp = (hstate_t *)alloc(sizeof(*hsp));
	hsp->next = host_map;
	hsp->name = sdup(host);
	hsp->state = STATE_INIT;
	host_map = hsp;
    }

    if (state == hsp->state) return 0;

    if (state == STATE_FAILINIT)
	__pmNotifyErr(LOG_INFO, "Cannot connect to pmcd on host %s\n", host);
    else if (state == STATE_RECONN && hsp->state != STATE_INIT)
	__pmNotifyErr(LOG_INFO, "Re-established connection to pmcd on host %s\n", host);
    else if (state == STATE_LOSTCONN)
	__pmNotifyErr(LOG_INFO, "Lost connection to pmcd on host %s\n", host);

    hsp->state = state;
    return 1;
}

/* try to reconnect to hosts and initialize missing metrics */
static void
enable(Task *t)
{
    Host	*h;
    Metric	*m;
    Metric	**p;

    h = t->hosts;
    while (h) {

	/* reconnect to host */
	if (h->down) {
	    if (reconnect(h)) {
		h->down = 0;
		host_state_changed(symName(h->name), STATE_RECONN);
	    }
	}

	/* reinitialize waiting Metrics */
	if ((! h->down) && (h->waits)) {
	    p = &h->waits;
	    m = *p;
	    while (m) {
		switch (reinitMetric(m)) {
		case 1:
		    *p = m->next;
		    unwaitMetric(m);
		    bundleMetric(h,m);
		    break;
		case 0:
		    p = &m->next;
		    break;
		case -1:
		    *p = m->next;
		    m->next = h->duds;
		    h->duds = m;
		    break;
		}
		m = *p;
	    }
	}

	h = h->next;
    }
}


/***********************************************************************
 * evaluation
 ***********************************************************************/

/* evaluate Task */
static void
eval(Task *task)
{
    Symbol	*s;
    pmValueSet  *vset;
    int		i;

    /* fetch metrics */
    taskFetch(task);

    /* evaluate rule expressions */
    s = task->rules;
    for (i = 0; i < task->nrules; i++) {
	curr = symValue(*s);
	if (curr->op < NOP) {
	    (curr->eval)(curr);
	    perf->eval_actual++;
	}
	s++;
    }

    if (verbose) {

	/* send binary values */
	if (agent) {
	    int	sts;
	    s = task->rules;
	    for (i = 0; i < task->nrules; i++) {
		vset = task->rslt->vset[i];
		fillVSet(symValue(*s), vset);
		s++;
	    }
	    __pmOverrideLastFd(PDU_OVERRIDE2);
	    sts = __pmSendResult(1, PDU_BINARY, task->rslt);
	    if (sts < 0) {
		fprintf(stderr, "Error: __pmSendResult to summary agent failed: %s\n", pmErrStr(sts));
		exit(0);
	    }

	}

        /* send values to applet */
        else if (applet) {
            s = task->rules;
            for (i = 0; i < task->nrules; i++) {
                showValue(stdout, symValue(*s));
                putchar(' ');
                s++;
            }
            putchar('\n');
        }

	/* print values in ASCII */
	else {
	    s = task->rules;
	    for (i = 0; i < task->nrules; i++) {
		printf("%s", symName(*s));
		if (archives) {
		    printf(" (");
		    showTime(stdout, now);
		    putchar(')');
		}
		printf(": ");
		switch (verbose) {
		case 1:
		    showValue(stdout, symValue(*s));
		    break;
		case 2:
		    showAnnotatedValue(stdout, symValue(*s));
		    break;
		case 3:
		    showSatisfyingValue(stdout, symValue(*s));
		    break;
		}
		putchar('\n');
		s++;
	    }
	    putchar('\n');
	}
    }
}


/* Mark expression as having invalid values */
void
clobber(Expr *x)
{
    int	    i;
    Truth   *t;
    double  *d;

    if (x->op < NOP) {
	if (x->arg1)
	    clobber(x->arg1);
	if (x->arg2)
	    clobber(x->arg2);
	x->valid = 0;
	if (x->sem <= SEM_NUM) {
	    d = (double *) x->ring;
	    for (i = 0; i < x->nvals; i++)
		*d++ = nan;
	}
	else if (x->sem == SEM_TRUTH) {
	    t = (Truth *) x->ring;
	    for (i = 0; i < x->nvals; i++)
		*t++ = DUNNO;
	}
    }
}


/***********************************************************************
 * exported functions
 ***********************************************************************/

/* fill in appropriate evaluator function for given Expr */
void
findEval(Expr *x)
{
    int		arity = 0;
    Metric	*m;

    /* 
     * arity values constructed from bit masks
     *	1	arg1 has tspan 1, and must always have one metric value
     *	2	arg2 has tspan 1, and must always have one metric value
     */
    if (x->arg1 && x->arg1->tspan == 1) {
	for (m = x->arg1->metrics; m; m = m->next) {
	    if (m->desc.indom == PM_INDOM_NULL) continue;
	    if (m->specinst == 0) break;
	}
	if (m == NULL) arity |= 1;
    }
    if (x->arg2 && x->arg2->tspan == 1) {
	for (m = x->arg2->metrics; m; m = m->next) {
	    if (m->desc.indom == PM_INDOM_NULL) continue;
	    if (m->specinst == 0) break;
	}
	if (m == NULL) arity |= 2;
    }

    switch (x->op) {

    case NOP:
    case OP_VAR:
	x->eval = NULL;

    case RULE:
	x->eval = rule;
	break;

    case CND_FETCH:
	if (x->metrics->desc.indom == PM_INDOM_NULL ||
	    x->metrics->conv == 0)
	    x->eval = cndFetch_1;
	else if (x->metrics->specinst == 0)
	    x->eval = cndFetch_all;
	else
	    x->eval = cndFetch_n;
	break;

    case CND_SUM_HOST:
	x->eval = cndSum_host;
	break;

    case CND_SUM_INST:
	x->eval = cndSum_inst;
	break;

    case CND_SUM_TIME:
	x->eval = cndSum_time;
	break;

    case CND_AVG_HOST:
	x->eval = cndAvg_host;
	break;

    case CND_AVG_INST:
	x->eval = cndAvg_inst;
	break;

    case CND_AVG_TIME:
	x->eval = cndAvg_time;
	break;

    case CND_MAX_HOST:
	x->eval = cndMax_host;
	break;

    case CND_MAX_INST:
	x->eval = cndMax_inst;
	break;

    case CND_MAX_TIME:
	x->eval = cndMax_time;
	break;

    case CND_MIN_HOST:
	x->eval = cndMin_host;
	break;

    case CND_MIN_INST:
	x->eval = cndMin_inst;
	break;

    case CND_MIN_TIME:
	x->eval = cndMin_time;
	break;

    case CND_ALL_HOST:
	x->eval = cndAll_host;
	break;

    case CND_ALL_INST:
	x->eval = cndAll_inst;
	break;

    case CND_ALL_TIME:
	x->eval = cndAll_time;
	break;

    case CND_SOME_HOST:
	x->eval = cndSome_host;
	break;

    case CND_SOME_INST:
	x->eval = cndSome_inst;
	break;

    case CND_SOME_TIME:
	x->eval = cndSome_time;
	break;

    case CND_PCNT_HOST:
	x->eval = cndPcnt_host;
	break;

    case CND_PCNT_INST:
	x->eval = cndPcnt_inst;
	break;

    case CND_PCNT_TIME:
	x->eval = cndPcnt_time;
	break;

    case CND_COUNT_HOST:
	x->eval = cndCount_host;
	break;

    case CND_COUNT_INST:
	x->eval = cndCount_inst;
	break;

    case CND_COUNT_TIME:
	x->eval = cndCount_time;
	break;

    case ACT_SEQ:
	x->eval = actAnd;
	break;

    case ACT_ALT:
	x->eval = actOr;
	break;

    case ACT_SHELL:
	x->eval = actShell;
	break;

    case ACT_ALARM:
	x->eval = actAlarm;
	break;

    case ACT_SYSLOG:
	x->eval = actSyslog;
	break;

    case ACT_PRINT:
	x->eval = actPrint;
	break;

    case ACT_ARG:
	x->eval = actArg;
	break;

    case CND_DELAY:
	if (arity & 1)
	    x->eval = cndDelay_1;
	else
	    x->eval = cndDelay_n;
	break;

    case CND_RATE:
	if (arity & 1)
	    x->eval = cndRate_1;
	else
	    x->eval = cndRate_n;
	break;

    case CND_NEG:
	if (arity & 1)
	    x->eval = cndNeg_1;
	else
	    x->eval = cndNeg_n;
	break;

    case CND_NOT:
	if (arity & 1)
	    x->eval = cndNot_1;
	else
	    x->eval = cndNot_n;
	break;

    case CND_RISE:
	if (arity & 1)
	    x->eval = cndRise_1;
	else
	    x->eval = cndRise_n;
	break;

    case CND_FALL:
	if (arity & 1)
	    x->eval = cndFall_1;
	else
	    x->eval = cndFall_n;
	break;

    case CND_ADD:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndAdd_1_1;
	    else
		x->eval = cndAdd_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndAdd_n_1;
	    else
		x->eval = cndAdd_n_n;
	}
	break;

    case CND_SUB:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndSub_1_1;
	    else
		x->eval = cndSub_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndSub_n_1;
	    else
		x->eval = cndSub_n_n;
	}
	break;

    case CND_MUL:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndMul_1_1;
	    else
		x->eval = cndMul_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndMul_n_1;
	    else
		x->eval = cndMul_n_n;
	}
	break;

    case CND_DIV:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndDiv_1_1;
	    else
		x->eval = cndDiv_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndDiv_n_1;
	    else
		x->eval = cndDiv_n_n;
	}
	break;

    case CND_EQ:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndEq_1_1;
	    else
		x->eval = cndEq_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndEq_n_1;
	    else
		x->eval = cndEq_n_n;
	}
	break;

    case CND_NEQ:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndNeq_1_1;
	    else
		x->eval = cndNeq_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndNeq_n_1;
	    else
		x->eval = cndNeq_n_n;
	}
	break;

    case CND_LT:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndLt_1_1;
	    else
		x->eval = cndLt_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndLt_n_1;
	    else
		x->eval = cndLt_n_n;
	}
	break;

    case CND_LTE:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndLte_1_1;
	    else
		x->eval = cndLte_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndLte_n_1;
	    else
		x->eval = cndLte_n_n;
	}
	break;

    case CND_GT:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndGt_1_1;
	    else
		x->eval = cndGt_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndGt_n_1;
	    else
		x->eval = cndGt_n_n;
	}
	break;

    case CND_GTE:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndGte_1_1;
	    else
		x->eval = cndGte_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndGte_n_1;
	    else
		x->eval = cndGte_n_n;
	}
	break;

    case CND_AND:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndAnd_1_1;
	    else
		x->eval = cndAnd_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndAnd_n_1;
	    else
		x->eval = cndAnd_n_n;
	}
	break;

    case CND_OR:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndOr_1_1;
	    else
		x->eval = cndOr_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndOr_n_1;
	    else
		x->eval = cndOr_n_n;
	}
	break;

    case CND_MATCH:
    case CND_NOMATCH:
	x->eval = cndMatch_inst;
	break;

    default:
	__pmNotifyErr(LOG_ERR, "findEval: internal error: bad op (%d)\n", x->op);
	dumpExpr(x);
	exit(1);
    }

    /* patch in fake actions for archive mode */
    if ((archives) && (x->op >= ACT_SHELL) && (x->op <= ACT_PRINT)) {
	x->eval = actFake;
    }
}


/* run evaluator */
void
run(void)
{
    Task	*t;

    /* empty task queue */
    if (taskq == NULL)
	return;

    /* initialize task scheduling */
    t = taskq;
    while (t) {
	t->eval = t->epoch = start;
	t->retry = 0;
	t->tick = 0;
	t = t->next;
    }

    /* evaluate and reschedule */
    t = taskq;
    for (;;) {
	if (t->retry && t->retry < t->eval) {
	    now = t->retry;
	    if (now > stop)
		break;
	    sleepTight(t->retry);
	    enable(t);
	    t->retry = waiting(t) ? now + RETRY : 0;
	}
	else {
	    now = t->eval;
	    if (now > stop)
		break;
	    reflectTime(t->delta);
	    sleepTight(t->eval);
	    eval(t);
	    t->tick++;
	    t->eval = t->epoch + t->tick * t->delta;
	    if ((! t->retry) && waiting(t))
		t->retry = now + RETRY;
	}
	taskq = t->next;
	if (taskq) taskq->prev = NULL;
	enque(t);
	t = taskq;
    }
    __pmNotifyErr(LOG_INFO, "evaluator exiting\n");
}


/* invalidate all expressions being evaluated
   i.e. mark values as unknown */
void
invalidate(void)
{
    Task    *t;
    Expr    *x;
    Symbol  *s;
    int	    i;

    t = taskq;
    while (t) {
	s = t->rules;
	for (i = 0; i < t->nrules; i++) {
	    x = symValue(*s);
	    clobber(x);
	    s++;
	}
	t = t->next;
    }
}
