/*
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


%{
#ident "$Id: gram.y,v 2.11 1998/09/09 17:53:58 kenmcd Exp $"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"

int		mystate = GLOBAL;
int		mynumb;
char		emess[160];
__pmHashCtl	pm_hash;
task_t		*tasklist;
fetchctl_t	*fetchroot;

static char	*name;
static pmID	pmid;
static int	sts;
static int	numinst;
static int	*intlist;
static char	**extlist;
static task_t	*tp;
static fetchctl_t	*fp;
static int	numvalid;
static int	warn = 1;

extern char	yytext[];
extern int	yylineno;
extern int	errno;

extern struct timeval	delta;		/* default logging interval */

typedef struct _hl {
    struct _hl	*hl_next;
    char	*hl_name;
    int		hl_line;
} hostlist_t;

static hostlist_t	*hl_root = NULL;
static hostlist_t	*hl_last = NULL;
static hostlist_t	*hlp;
static hostlist_t	*prevhlp;
static int		opmask = 0;
static int		specmask = 0;
static int		allow;
static int		state = 0;

%}

%term	LSQB
	RSQB
	COMMA
	LBRACE
	RBRACE
	COLON
	SEMICOLON

	LOG
	MANDATORY ADVISORY
	ON OFF MAYBE
	EVERY ONCE DEFAULT
	MSEC SECOND MINUTE HOUR

	ACCESS ENQUIRE ALLOW DISALLOW ALL EXCEPT

	NAME
	IPSPEC
	NUMBER
	STRING
%%

config	: specopt accessopt
	;

specopt	: spec
	| /* nothing */
	;

spec	: stmt
	| spec stmt
	;

stmt    : dowhat somemetrics				{
			mystate = GLOBAL;
			if (numvalid) {
			    PMLC_SET_MAYBE(tp->t_state, 0);	/* clear req */
			    tp->t_next = tasklist;
			    tasklist = tp;
			    tp->t_fetch = fetchroot;
			    for (fp = fetchroot; fp != NULL; fp = fp->f_next)
				/* link fetchctl back to task */
				fp->f_aux = (void *)tp;


			    if (PMLC_GET_ON(state))
				tp->t_afid = __pmAFregister(&tp->t_delta, (void *)tp, log_callback);
			}
			else
			    free(tp);

			fetchroot = NULL;
			state = 0;
		    }

dowhat	: logopt action		{
			tp = (task_t *)calloc(1, sizeof(task_t));
			if (tp == NULL) {
			    sprintf(emess, "malloc failed: %s", strerror(errno));
			    yyerror(emess);
			    /*NOTREACHED*/
			}
			if (mynumb == LOG_DELTA_ONCE) {
			    tp->t_delta.tv_sec = 0;
			    tp->t_delta.tv_usec = 0;
			}
			else if (mynumb == LOG_DELTA_DEFAULT) {
			    tp->t_delta = delta;
			}
			else if (PMLC_GET_ON(state)) {
			    if (mynumb < 0) {
sprintf(emess, "Logging delta (%d msec) must be positive", mynumb);
				yyerror(emess);
				/*NOTREACHED*/
			    }
			    else if (mynumb > PMLC_MAX_DELTA) {
sprintf(emess, "Logging delta (%d msec) cannot be bigger than %d msec", mynumb, PMLC_MAX_DELTA);
				yyerror(emess);
				/*NOTREACHED*/
			    }
			    tp->t_delta.tv_sec = mynumb / 1000;
			    tp->t_delta.tv_usec = 1000 * (mynumb % 1000);
			}
			tp->t_state =  state;
		    }
	;

logopt	: LOG 
	| /* nothing */
	;

action	: cntrl ON frequency		{ PMLC_SET_ON(state, 1); }
	| cntrl OFF			{ PMLC_SET_ON(state, 0); }
	| MANDATORY MAYBE		{
		    PMLC_SET_MAND(state, 0);
		    PMLC_SET_ON(state, 0);
		    PMLC_SET_MAYBE(state, 1);
		}
	;

cntrl	: MANDATORY			{ PMLC_SET_MAND(state, 1); }
	| ADVISORY			{ PMLC_SET_MAND(state, 0); }
	;

frequency	: everyopt NUMBER timeunits	{ mynumb *= $3; }
		| ONCE				{ mynumb = LOG_DELTA_ONCE; }
		| DEFAULT			{ mynumb = LOG_DELTA_DEFAULT; }
		;

everyopt	: EVERY
		| /* nothing */
		;

timeunits	: MSEC		{ $$ = 1; }
		| SECOND	{ $$ = 1000; }
		| MINUTE	{ $$ = 60000; }
		| HOUR		{ $$ = 3600000; }
		;

somemetrics	: LBRACE { numvalid = 0; mystate = INSPEC; } metriclist RBRACE
		| metricspec
		;

metriclist	: metricspec
		| metriclist metricspec
		| metriclist COMMA metricspec
		;

metricspec	: NAME { name = strdup(yytext); } optinst	{
			if (name == NULL) {
			    sprintf(emess, "malloc failed: %s", strerror(errno));
			    yyerror(emess);
			    /*NOTREACHED*/
			}
			sts = pmTraversePMNS(name, dometric);
			if (sts < 0) {
    sprintf(emess, "Problem with lookup for metric \"%s\" ... logging not activated", name);
    yywarn(emess);
    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
			    free(name);
			}
			freeinst(&numinst, intlist, extlist);
		    }
		;

optinst		: LSQB instancelist RSQB
		| /* nothing */
		;

instancelist	: instance
		| instance instancelist
		| instance COMMA instancelist
		;

instance	: NAME			{ buildinst(&numinst, &intlist, &extlist, -1, yytext); }
		| NUMBER		{ buildinst(&numinst, &intlist, &extlist, mynumb, NULL); }
		| STRING		{ buildinst(&numinst, &intlist, &extlist, -1, yytext); }
		;

accessopt	: LSQB ACCESS RSQB ctllist
		| /* nothing */
		;

ctllist		: ctl
		| ctl ctllist
		;
		
ctl		: allow hostlist COLON operation SEMICOLON	{
			    prevhlp = NULL;
			    for (hlp = hl_root; hlp != NULL; hlp = hlp->hl_next) {
				if (prevhlp != NULL) {
				    free(prevhlp->hl_name);
				    free(prevhlp);
				}
				sts = __pmAccAddHost(hlp->hl_name, specmask, opmask, 0);
				if (sts < 0) {
				    fprintf(stderr, "error was on line %d\n", hlp->hl_line);
				    YYABORT;
				}
				prevhlp = hlp;
			    }
			    if (prevhlp != NULL) {
				free(prevhlp->hl_name);
				free(prevhlp);
			    }
			    opmask = 0;
			    specmask = 0;
			    hl_root = hl_last = NULL;
			}
		;

allow		: ALLOW			{ allow = 1; }
		| DISALLOW		{ allow = 0; }

hostlist	: host
		| host COMMA hostlist
		;

host		: hostspec		{
			hlp = (hostlist_t *)malloc(sts = sizeof(hostlist_t));
			if (hlp == NULL) {
			    __pmNoMem("adding new host", sts, PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			if (hl_last != NULL) {
			    hl_last->hl_next = hlp;
			    hl_last = hlp;
			}
			else
			    hl_root = hl_last = hlp;
			hlp->hl_next = NULL;
			hlp->hl_name = strdup(yytext);
			hlp->hl_line = yylineno;
		    }
		;

hostspec	: IPSPEC
		| NAME
		;

operation	: operlist		{
			specmask = opmask;
			if (allow)
			    opmask = ~opmask;
		    }
		| ALL			{
			specmask = OP_ALL;
			if (allow)
			    opmask = OP_NONE;
			else
			    opmask = OP_ALL;
		    }
		| ALL EXCEPT operlist	{
			specmask = OP_ALL;
			if (!allow)
			    opmask = ~opmask;
		    }
		;

operlist	: op
		| op COMMA operlist
		;

op		: ADVISORY		{ opmask |= OP_LOG_ADV; }
		| MANDATORY		{ opmask |= OP_LOG_MAND; }
		| ENQUIRE		{ opmask |= OP_LOG_ENQ; }
		;

%%

/*
 * Assumed calling context ...
 *	tp		the correct task for the requested metric
 *	numinst		number of instances associated with this request
 *	extlist[]	external instance names if numinst > 0
 *	intlist[]	internal instance identifier if numinst > 0 and
 *			corresponding extlist[] entry is NULL
 */

void
dometric(const char *name)
{
    int		sts;
    int		inst;
    int		i;
    int		j;
    int		skip;
    pmID	pmid;
    pmDesc	*dp;
    optreq_t	*rqp;
    extern char	*chk_emess[];

    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL)
	goto nomem;

    /* Cast away const, pmLookupName should never modify name */
    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0 || pmid == PM_ID_NULL) {
	sprintf(emess, "Metric \"%s\" is unknown ... not logged", name);
	goto defer;
    }

    if ((sts = pmLookupDesc(pmid, dp)) < 0) {
	sprintf(emess, "Description unavailable for metric \"%s\" ... not logged", name);
	goto defer;
    }

    tp->t_numpmid++;
    tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, tp->t_numpmid * sizeof(pmID));
    if (tp->t_pmidlist == NULL)
	goto nomem;
    tp->t_pmidlist[tp->t_numpmid-1] = pmid;
    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL)
	goto nomem;
    rqp->r_desc = dp;
    rqp->r_numinst = numinst;
    skip = 0;
    if (numinst) {
	/*
	 * malloc here, and keep ... gets buried in optFetch data structures
	 */
	rqp->r_instlist = (int *)malloc(numinst * sizeof(rqp->r_instlist[0]));
	if (rqp->r_instlist == NULL)
	    goto nomem;
	j = 0;
	for (i = 0; i < numinst; i++) {
	    if (extlist[i] != NULL) {
		sts = pmLookupInDom(dp->indom, extlist[i]);
		if (sts < 0) {
		    sprintf(emess, "Instance \"%s\" is not defined for the metric \"%s\"", extlist[i], name);
		    yywarn(emess);
		    rqp->r_numinst--;
		    continue;
		}
		inst = sts;
	    }
	    else {
		char	*p;
		sts = pmNameInDom(dp->indom, intlist[i], &p);
		if (sts < 0) {
		    sprintf(emess, "Instance \"%d\" is not defined for the metric \"%s\"", intlist[i], name);
		    yywarn(emess);
		    rqp->r_numinst--;
		    continue;
		}
		free(p);
		inst = intlist[i];
	    }
	    if ((sts = chk_one(tp, pmid, inst)) < 0) {
		sprintf(emess, "Incompatible request for metric \"%s\" and instance \"%s\"", name, extlist[i]);
		yywarn(emess);
		fprintf(stderr, "%s\n", chk_emess[-sts]);
		rqp->r_numinst--;
	    }
	    else
		rqp->r_instlist[j++] = inst;
	}
	if (rqp->r_numinst == 0)
	    skip = 1;
    }
    else {
	if ((sts = chk_all(tp, pmid)) < 0) {
	    sprintf(emess, "Incompatible request for metric \"%s\"", name);
	    yywarn(emess);
	    skip = 1;
	}
    }

    if (!skip) {
	if ((sts = __pmOptFetchAdd(&fetchroot, rqp)) < 0) {
	    sprintf(emess, "__pmOptFetchAdd failed for metric \"%s\" ... logging not activated", name);
	    goto snarf;
	}

	if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0) {
	    sprintf(emess, "__pmHashAdd failed for metric \"%s\" ... logging not activated", name);
	    goto snarf;
	}
	numvalid++;
    }
    else {
	free(dp);
	free(rqp);
    }

    return;

defer:
    /* TODO EXCEPTION PCP 2.0
     * The idea here is that we will sort all logging request into "good" and
     * "bad" (like pmie) ... the "bad" ones are "deferred" and at some point
     * later pmlogger would (periodically) revisit the "deferred" ones and see
     * if they can be added to the "good" set.
     */
    if (warn) {
	yywarn(emess);
	fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    }
    if (dp != NULL)
	free(dp);
    return;

nomem:
    sprintf(emess, "malloc failed: %s", strerror(errno));
    yyerror(emess);
    /*NOTREACHED*/

snarf:
    yywarn(emess);
    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    free(dp);
    return;
}
