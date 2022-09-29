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

#ident "$Id: context.c,v 2.28 1999/08/17 04:26:04 kenmcd Exp $"

#include <sys/param.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

#define PM_CONTEXT_UNDEF	-1	/* current context is undefined */

static __pmContext	*contexts = NULL;		/* array of contexts */
static int		contexts_len = 0;		/* number of contexts */
static int		curcontext = PM_CONTEXT_UNDEF;	/* current context */

static int	n_backoff = 0;
static int	def_backoff[] = {5, 10, 20, 40, 80};
static int	*backoff = NULL;

extern int 	__pmConnectPMCD(const char *);
extern int 	__pmConnectLocal(void);
extern int	errno;

static void
waitawhile(__pmPMCDCtl *ctl)
{
    /*
     * after failure, compute delay before trying again ...
     */
    if (n_backoff == 0) {
	char	*q;
	/* first time ... try for PMCD_RECONNECT_TIMEOUT from env */
	if ((q = getenv("PMCD_RECONNECT_TIMEOUT")) != NULL) {
	    char	*pend;
	    char	*p;
	    int		val;

	    for (p = q; *p != '\0'; ) {
		val = (int)strtol(p, &pend, 10);
		if (val <= 0 || (*pend != ',' && *pend != '\0')) {
		    __pmNotifyErr(LOG_WARNING,
				 "pmReconnectContext: ignored bad PMCD_RECONNECT_TIMEOUT = '%s'\n",
				 q);
		    n_backoff = 0;
		    if (backoff != NULL)
			free(backoff);
		    break;
		}
		if ((backoff = (int *)realloc(backoff, (n_backoff+1) * sizeof(backoff[0]))) == NULL) {
		    __pmNoMem("pmReconnectContext", (n_backoff+1) * sizeof(backoff[0]), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
		backoff[n_backoff++] = val;
		if (*pend == '\0')
		    break;
		p = &pend[1];
	    }
	}
	if (n_backoff == 0) {
	    /* use default */
	    n_backoff = 5;
	    backoff = def_backoff;
	}
    }
    if (ctl->pc_timeout == 0)
	ctl->pc_timeout = 1;
    else if (ctl->pc_timeout < n_backoff)
	ctl->pc_timeout++;
    ctl->pc_again = time(NULL) + backoff[ctl->pc_timeout-1];
}

__pmContext *
__pmHandleToPtr(int handle)
{
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE)
	return NULL;
    else
	return &contexts[handle];
}

int
pmWhichContext(void)
{
    /*
     * return curcontext, provided it is defined
     */
    int		sts;

    if (curcontext > PM_CONTEXT_UNDEF)
	sts = curcontext;
    else
	sts = PM_ERR_NOCONTEXT;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmWhichContext() -> %d, cur=%d\n",
	    sts, curcontext);
#endif
    return sts;
}


int
pmNewContext(int type, const char *name)
{
    __pmContext	*new = NULL;
    __pmContext	*list;
    int		i;
    int		sts;
    int		old_curcontext = curcontext;
    int		old_contexts_len = contexts_len;

    /* See if we can reuse a free context */
    for (i = 0; i < contexts_len; i++) {
	if (contexts[i].c_type == PM_CONTEXT_FREE) {
	    curcontext = i;
	    new = &contexts[curcontext];
	    goto INIT_CONTEXT;
	}
    }

#ifdef MALLOC_AUDIT
/* contexts are persistent, and no memory leak here */
#include "no-malloc-audit.h"
#endif

    /* Create a new one */
    if (contexts == NULL)
	list = (__pmContext *)malloc(sizeof(__pmContext));
    else
	list = (__pmContext *)realloc((void *)contexts, (1+contexts_len) * sizeof(__pmContext));

#ifdef MALLOC_AUDIT
/*
 * but other code may contain memory leaks ...
 * note { } 'cause prototypes in the include file
 */
{
#include "malloc-audit.h"
}
#endif

    if (list == NULL) {
	/* fail : nothing changed */
	sts = -errno;
	goto FAILED;
    }

    contexts = list;
    curcontext = contexts_len++;
    new = &contexts[curcontext];

INIT_CONTEXT:
    /*
     * Set up the default state
     */
    memset(new, 0, sizeof(__pmContext));
    new->c_type = type;
    if ((new->c_instprof = (__pmProfile *)malloc(sizeof(__pmProfile))) == NULL) {
	/*
	 * fail : nothing changed -- actually list is changed, but restoring
	 * contexts_len should make it ok next time through
	 */
	sts = -errno;
	goto FAILED;
    }
    memset(new->c_instprof, 0, sizeof(__pmProfile));
    new->c_instprof->state = PM_PROFILE_INCLUDE;	/* default global state */
    new->c_sent = 0;
						/* profile not sent */
    new->c_origin.tv_sec = new->c_origin.tv_usec = 0;	/* default time */

    if (type == PM_CONTEXT_HOST) {
	for (i = 0; i < contexts_len; i++) {
	    if (i == curcontext)
		continue;
	    if (contexts[i].c_type == PM_CONTEXT_HOST &&
		strcmp(name, contexts[i].c_pmcd->pc_name) == 0) {
		new->c_pmcd = contexts[i].c_pmcd;
		/*new->c_pduinfo = contexts[i].c_pduinfo;*/
	    }
	}
	if (new->c_pmcd == NULL) {
	    /*
	     * Try to establish the connection.
	     * If this fails, restore the original current context
	     * and return an error.
	     */
	    if ((sts = __pmConnectPMCD(name)) < 0)
		goto FAILED;

	    if ((new->c_pmcd = (__pmPMCDCtl *)malloc(sizeof(__pmPMCDCtl))) == NULL) {
		__pmNoMoreInput(sts);
		close(sts);
		sts = -errno;
		goto FAILED;
	    }
	    if ((new->c_pmcd->pc_name = strdup(name)) == NULL) {
		__pmNoMoreInput(sts);
		close(sts);
		free(new->c_pmcd);
		sts = -errno;
		goto FAILED;
	    }
	    new->c_pmcd->pc_refcnt = 0;
	    new->c_pmcd->pc_fd = sts;
	    new->c_pmcd->pc_timeout = 0;
	}
	new->c_pmcd->pc_refcnt++;
    }
    else if (type == PM_CONTEXT_LOCAL) {
	if ((sts = __pmCheckObjectStyle()) != 0)
	    goto FAILED;
	if ((sts = __pmConnectLocal()) != 0)
	    goto FAILED;
    }
    else if (type == PM_CONTEXT_ARCHIVE) {
	if ((new->c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -errno;
	    goto FAILED;
	}
	new->c_archctl->ac_log = NULL;
	for (i = 0; i < contexts_len; i++) {
	    if (i == curcontext)
		continue;
	    if (contexts[i].c_type == PM_CONTEXT_ARCHIVE &&
		strcmp(name, contexts[i].c_archctl->ac_log->l_name) == 0) {
		new->c_archctl->ac_log = contexts[i].c_archctl->ac_log;
	    }
	}
	if (new->c_archctl->ac_log == NULL) {
	    if ((new->c_archctl->ac_log = (__pmLogCtl *)malloc(sizeof(__pmLogCtl))) == NULL) {
		free(new->c_archctl);
		sts = -errno;
		goto FAILED;
	    }
	    if ((sts = __pmLogOpen(name, new)) < 0) {
		free(new->c_archctl->ac_log);
		free(new->c_archctl);
		goto FAILED;
	    }
        }
	else {
	    /* archive already open, set default starting state as per __pmLogOpen */
	    new->c_origin.tv_sec = (__int32_t)new->c_archctl->ac_log->l_label.ill_start.tv_sec;
	    new->c_origin.tv_usec = (__int32_t)new->c_archctl->ac_log->l_label.ill_start.tv_usec;
	    new->c_mode = (new->c_mode & 0xffff0000) | PM_MODE_FORW;
	}

	/* start after header + label record + trailer */
	new->c_archctl->ac_offset = sizeof(__pmLogLabel) + 2*sizeof(int);
	new->c_archctl->ac_vol = new->c_archctl->ac_log->l_curvol;
	new->c_archctl->ac_serial = 0;		/* not serial access, yet */
	new->c_archctl->ac_pmid_hc.nodes = 0;	/* empty hash list */
	new->c_archctl->ac_pmid_hc.hsize = 0;

	new->c_archctl->ac_log->l_refcnt++;
    }
    else {
	/* bad type */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "pmNewContext(%d, %s): illegal type\n",
		    type, name);
	}
#endif
	return PM_ERR_NOCONTEXT;
    }

    /* return the handle to the new (current) context */
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmNewContext(%d, %s) -> %d\n", type, name, curcontext);
	__pmDumpContext(stderr, curcontext, PM_INDOM_NULL);
    }
#endif
    return curcontext;

FAILED:
    if (new != NULL) {
	new->c_type = PM_CONTEXT_FREE;
	if (new->c_instprof != NULL)
	    free(new->c_instprof);
    }
    curcontext = old_curcontext;
    contexts_len = old_contexts_len;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewContext(%d, %s) -> %d, curcontext=%d\n",
	    type, name, sts, curcontext);
#endif
    return sts;
}


int
pmReconnectContext(int handle)
{
    __pmContext	*ctxp;
    __pmPMCDCtl	*ctl;
    int		sts;

    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

    ctxp = &contexts[handle];
    ctl = ctxp->c_pmcd;
    if (ctxp->c_type == PM_CONTEXT_HOST) {
	if (ctl->pc_timeout && time(NULL) < ctl->pc_again) {
	    /* too soon to try again */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "pmReconnectContext(%d) -> %d, too soon (need wait another %d secs)\n",
		handle, -ETIMEDOUT, ctl->pc_again - time(NULL));
#endif
	    return -ETIMEDOUT;
	}

	if (ctl->pc_fd >= 0) {
	    /* don't care if this fails */
	    __pmNoMoreInput(ctl->pc_fd);
	    close(ctl->pc_fd);
	    ctl->pc_fd = -1;
	}

	if ((sts = __pmConnectPMCD(ctl->pc_name)) >= 0) {
	    ctl->pc_fd = sts;
	    ctxp->c_sent = 0;
	}

	if (sts < 0) {
	    waitawhile(ctl);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), failed (wait %d secs before next attempt)\n",
		    handle, ctl->pc_again - time(NULL));
#endif
	    return -ETIMEDOUT;
	}
	else {
	    int		i;
	    /*
	     * mark profile as not sent for all contexts sharing this
	     * socket
	     */
	    for (i = 0; i < contexts_len; i++) {
		if (contexts[i].c_type != PM_CONTEXT_FREE && contexts[i].c_pmcd == ctl) {
		    contexts[i].c_sent = 0;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), done\n", handle);
#endif
	    ctl->pc_timeout = 0;
	}
    }
    else {
	/*
	 * assume PM_CONTEXT_ARCHIVE or PM_CONTEXT_LOCAL reconnect,
	 * this is a NOP in either case.
	 */
	;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, handle);
#endif
    return handle;
}

int
pmDupContext(void)
{
    int			sts;
    int			old, new;
    __pmContext		*newcon, *oldcon;
    __pmInDomProfile	*q, *p, *p_end;
    __pmProfile		*save;

    if ((old = pmWhichContext()) < 0) {
	sts = old;
	goto done;
    }
    oldcon = &contexts[old];
    if (oldcon->c_type == PM_CONTEXT_HOST)
	new = pmNewContext(oldcon->c_type, oldcon->c_pmcd->pc_name);
    else if (oldcon->c_type == PM_CONTEXT_LOCAL)
	new = pmNewContext(oldcon->c_type, NULL);
    else
	/* assume PM_CONTEXT_ARCHIVE */
	new = pmNewContext(oldcon->c_type, oldcon->c_archctl->ac_log->l_name);
    if (new < 0) {
	/* failed to connect or out of memory */
	sts = new;
	goto done;
    }
    oldcon = &contexts[old];	/* contexts[] may have been relocated */
    newcon = &contexts[new];
    save = newcon->c_instprof;	/* need this later */
    *newcon = *oldcon;		/* struct copy */
    newcon->c_instprof = save;	/* restore saved instprof from pmNewContext */

    /* clone the per-domain profiles (if any) */
    if (oldcon->c_instprof->profile_len > 0) {
	newcon->c_instprof->profile = (__pmInDomProfile *)malloc(
	    oldcon->c_instprof->profile_len * sizeof(__pmInDomProfile));
	if (newcon->c_instprof->profile == NULL) {
	    sts = -errno;
	    goto done;
	}
	memcpy(newcon->c_instprof->profile, oldcon->c_instprof->profile,
	    oldcon->c_instprof->profile_len * sizeof(__pmInDomProfile));
	p = oldcon->c_instprof->profile;
	p_end = p + oldcon->c_instprof->profile_len;
	q = newcon->c_instprof->profile;
	for (; p < p_end; p++, q++) {
	    if (p->instances) {
		q->instances = (int *)malloc(p->instances_len * sizeof(int));
		if (q->instances == NULL) {
		    sts = -errno;
		    goto done;
		}
		memcpy(q->instances, p->instances,
		    p->instances_len * sizeof(int));
	    }
	}
    }

    /*
     * The call to pmNewContext (above) should have connected to the pmcd.
     * Make sure the new profile will be sent before the next fetch.
     */
    newcon->c_sent = 0;

    /* clone the archive control struct, if any */
    if (oldcon->c_archctl != NULL) {
	if ((newcon->c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -errno;
	    goto done;
	}
	*newcon->c_archctl = *oldcon->c_archctl;	/* struct assignment */
    }

    sts = new;

done:
    /* return an error code, or the handle for the new context */
    if (sts < 0 && new >= 0)
	contexts[new].c_type = PM_CONTEXT_FREE;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmDupContext() -> %d\n", sts);
	if (sts >= 0)
	    __pmDumpContext(stderr, sts, PM_INDOM_NULL);
    }
#endif
    return sts;
}

int
pmUseContext(int handle)
{
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmUseContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseContext(%d) -> 0\n", handle);
#endif
    curcontext = handle;
    return 0;
}

int
pmDestroyContext(int handle)
{
    __pmContext		*ctxp;
    struct linger       dolinger = {0, 1};

    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmDestroyContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

    ctxp = &contexts[handle];
    if (ctxp->c_pmcd != NULL) {
	if (--ctxp->c_pmcd->pc_refcnt == 0) {
	    if (ctxp->c_pmcd->pc_fd >= 0) {
		/* before close, unsent data should be flushed */
		setsockopt(ctxp->c_pmcd->pc_fd, SOL_SOCKET,
		    SO_LINGER, (char *) &dolinger, sizeof(dolinger));
		__pmNoMoreInput(ctxp->c_pmcd->pc_fd);
		close(ctxp->c_pmcd->pc_fd);
	    }
	    free(ctxp->c_pmcd->pc_name);
	    free(ctxp->c_pmcd);
	}
    }
    if (ctxp->c_archctl != NULL) {
	if (--ctxp->c_archctl->ac_log->l_refcnt == 0) {
	    __pmLogClose(ctxp->c_archctl->ac_log);
	    free(ctxp->c_archctl->ac_log);
	}
	free(ctxp->c_archctl);
    }
    ctxp->c_type = PM_CONTEXT_FREE;

    if (handle == curcontext)
	/* we have no choice */
	curcontext = PM_CONTEXT_UNDEF;

    __pmFreeProfile(ctxp->c_instprof);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmDestroyContext(%d) -> 0, curcontext=%d\n",
		handle, curcontext);
#endif

    return 0;
}

static char *_mode[] = { "LIVE", "INTERP", "FORW", "BACK" };

/*
 * dump context(s); context == -1 for all contexts, indom == PM_INDOM_NULL
 * for all instance domains.
 */
void
__pmDumpContext(FILE *f, int context, pmInDom indom)
{
    int			i;
    __pmContext		*con;

    fprintf(f, "Dump Contexts: current context = %d\n", curcontext);
    if (curcontext < 0)
	return;

    if (indom != PM_INDOM_NULL) {
	fprintf(f, "Dump restricted to indom=%d [%s]\n", 
	        indom, pmInDomStr(indom));
    }

    for (con=contexts, i=0; i < contexts_len; i++, con++) {
	if (context == -1 || context == i) {
	    fprintf(f, "Context[%d]", i);
	    if (con->c_type == PM_CONTEXT_HOST) {
		fprintf(f, " host %s:", con->c_pmcd->pc_name);
		fprintf(f, " pmcd=%s profile=%s fd=%d refcnt=%d",
		    (con->c_pmcd->pc_fd < 0) ? "NOT CONNECTED" : "CONNECTED",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_pmcd->pc_fd,
		    con->c_pmcd->pc_refcnt);
	    }
	    else if (con->c_type == PM_CONTEXT_LOCAL) {
		fprintf(f, " standalone:");
		fprintf(f, " profile=%s\n",
		    con->c_sent ? "SENT" : "NOT_SENT");
	    }
	    else {
		fprintf(f, " log %s:", con->c_archctl->ac_log->l_name);
		fprintf(f, " mode=%s", _mode[con->c_mode & __PM_MODE_MASK]);
		fprintf(f, " profile=%s tifd=%d mdfd=%d mfd=%d\nrefcnt=%d vol=%d",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_archctl->ac_log->l_tifp == NULL ? -1 : fileno(con->c_archctl->ac_log->l_tifp),
		    fileno(con->c_archctl->ac_log->l_mdfp),
		    fileno(con->c_archctl->ac_log->l_mfp),
		    con->c_archctl->ac_log->l_refcnt,
		    con->c_archctl->ac_log->l_curvol);
		fprintf(f, " offset=%ld (vol=%d) serial=%d",
		    (long)con->c_archctl->ac_offset,
		    con->c_archctl->ac_vol,
		    con->c_archctl->ac_serial);
	    }
	    if (con->c_type != PM_CONTEXT_LOCAL) {
		fprintf(f, " origin=%d.%06d",
		    con->c_origin.tv_sec, con->c_origin.tv_usec);
		fprintf(f, " delta=%d\n", con->c_delta);
	    }
	    __pmDumpProfile(f, indom, con->c_instprof);
	}
    }
}
