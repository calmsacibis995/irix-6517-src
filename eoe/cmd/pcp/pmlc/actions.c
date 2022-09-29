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

#ident "$Id: actions.c,v 2.27 1999/05/11 00:28:03 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "pmapi_dev.h"
#include "./pmlc.h"

#if defined(IRIX6_5)
#include <optional_sym.h>
#endif

/* for the pmlogger/PMCD we currently have a connection to */
static int	logger_fd = -1;			/* file desc pmlogger */
static char	*lasthost = NULL;		/* host that logger_ctx is for */
static int	src_ctx = -1;		/* context for logged host's PMCD*/
static char	*srchost = NULL;	/* host that logged_ctx is for */

static time_t	tmp;		/* for pmCtime */

/* PCP 1.x pmlogger returns one of these when a status request is made */
typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_tz[40];      /* $TZ at collection host */
    char	ls_tzlogger[40]; /* $TZ at pmlogger */
} __pmLoggerStatus_v1;

int
ConnectPMCD(void)
{
    int			sts;
    __pmPDU		*pb;
    __pmIPC		*ipc;

    if (src_ctx >= 0)
	return src_ctx;

    if ((sts = __pmFdLookupIPC(logger_fd, &ipc)) < 0) {
	fprintf(stderr, "Logger connection badly initialised.\n");
	return sts;
    }

    if (ipc->version >= LOG_PDU_VERSION2) {
	__pmLoggerStatus	*lsp;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	     fprintf(stderr, "pmlc: sending version 2 status request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_STATUS)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if ((sts = __pmDecodeLogStatus(pb, &lsp)) < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    goto done;
	} 
	srchost = strdup(lsp->ls_fqdn);
    }
    else {
	int			subtype, vlen;
	__pmLoggerStatus_v1	*lsp_v1;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 1 datax status request\n");
#endif
	if ((sts = __pmSendDataX(logger_fd, PDU_BINARY, PMLC_PDU_STATUS_REQ,
				0, NULL)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return sts;
	}
	if ((sts = __pmDecodeDataX(pb, PDU_BINARY, &subtype,
			      &vlen, (void **)&lsp_v1)) < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    goto done;
	}
	if (subtype != PMLC_PDU_STATUS || vlen != sizeof(__pmLoggerStatus_v1)) {
	    fprintf(stderr, "Incorrect data from pmlogger (version mismatch?)\n"
#ifdef PCP_DEBUG
		"subtype=%d vlen=%d (expected %d)\n",
		subtype, vlen, sizeof(__pmLoggerStatus_v1)
#endif
		);
	    sts = PM_ERR_IPC;
	    goto done;
	}
	srchost = strdup(lsp_v1->ls_hostname);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, srchost)) < 0) {
	/* no PMCD connection, we can't do anything, give up */
	fprintf(stderr, "Error trying to connect to PMCD on %s: %s\n",
		srchost, pmErrStr(sts));
    }
    else
        src_ctx = sts;

done:
    __pmUnpinPDUBuf(pb);
    return sts;
}

int
ConnectLogger(char *hostname, int *pid, int *port)
{
    int		sts;

    if (lasthost != NULL) {
	free(lasthost);
	lasthost = NULL;
    }
    if (logger_fd != -1) {
	__pmNoMoreInput(logger_fd);
	__pmResetIPC(logger_fd);
	close(logger_fd);
	sleep(1);
    }

    if (src_ctx != -1) {
	if ((sts = pmDestroyContext(src_ctx)) < 0)
		fprintf(stderr, "Error deleting PMCD connection to %s: %s\n",
			srchost, pmErrStr(sts));
	src_ctx = -1;
    }
    if (srchost != NULL) {
	free(srchost);
	srchost = NULL;
    }

    if ((sts = __pmConnectLogger(hostname, pid, port)) < 0) {
	logger_fd = -1;
	return sts;
    }
    else {
	logger_fd = sts;
	if ((lasthost = strdup(hostname)) == NULL) {
	    __pmNoMem("Error copying host name", strlen(hostname), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	return 0;
    }
}

void
ShowLoggers(char *hostname)
{
    int		i, n;
    int		ctx;
    int		primary = -1;		/* ports[] index for primary logger */
    int		pport;			/* port for primary logger */
    __pmLogPort	*ports;

    if ((n = __pmIsLocalhost(hostname)) == 0) {
	/* remote, need PMCD's help for __pmLogFindPort */
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, hostname)) < 0) {
	    fprintf(stderr, "Error trying to connect to PMCD on %s: %s\n",
		hostname, pmErrStr(ctx));
	    return;
	}
    }
    else
	ctx = -1;

    if ((n = __pmLogFindPort(hostname, PM_LOG_ALL_PIDS, &ports)) < 0) {
	fprintf(stderr, "Error finding pmloggers on %s: ",
		hostname);
	if (still_connected(n))
	    fprintf(stderr, "%s\n", pmErrStr(n));
    }
    else if (n == 0)
	    printf("No pmloggers running on %s\n", hostname);
    else {
	/* find the position of the primary logger */
	for (i = 0; i < n; i++) {
	    if (ports[i].pid == PM_LOG_PRIMARY_PID) {
		pport = ports[i].port;
		break;
	    }
	}
	for (i = 0; i < n; i++) {
	    if (ports[i].port == pport) {
		primary = i;
		break;
	    }
	}
	printf("The following pmloggers are running on %s:\n    ", hostname);
	/* print any primary logger first, with its pid alias in parentheses) */
	if (primary != -1) {
	    printf("primary");
	    printf(" (%d)", ports[primary].pid);
	}
	/* now print everything except the primary logger */
	for (i = 0; i < n; i++) {
	    if (i != primary &&
		ports[i].pid != PM_LOG_PRIMARY_PID) {
		    printf(" %d", ports[i].pid);
	    }
	}
	putchar('\n');

	/* Note: Don't free ports, it's storage is managed by __pmLogFindPort() */
    }

    if (ctx >= 0)
	pmDestroyContext(ctx);
}

static void
PrintState(int state)
{
    static char	*units[] = {"msec", "sec ", "min ", "hour"};
    static int	factor[] = {1000, 60, 60, 24};
    int		nfactors = sizeof(factor) / sizeof(factor[0]);
    int		i, j, is_on;
    int		delta = PMLC_GET_DELTA(state);
    float	t = delta;
    int		frac;

    fputs(PMLC_GET_MAND(state) ? "mand " : "adv  ", stdout);
    is_on = PMLC_GET_ON(state);
    fputs(is_on ? "on  " : "off ", stdout);
    if (PMLC_GET_INLOG(state))
	fputs(PMLC_GET_AVAIL(state) ? "   " : "na ", stdout);
    else
	fputs("nl ", stdout);

    /* don't display time unless logging on */
    if (!is_on) {
	fputs("            ", stdout);
	return;
    }

    if (delta == 0) {
	fputs("        once", stdout);
	return;
    }

    for (i = 0; i < nfactors; i++) {
	if (t < factor[i])
	    break;
	t /= factor[i];
    }
    if (i >= nfactors)
	i = nfactors - 1;
    
    frac = (int) ((t - (int)t) * 1000);	/* get 3 decimal places */
    if (frac % 10)
	j = 3;
    else
	if (frac % 100)
	    j = 2;
	else
	    if (frac % 1000)
		j = 1;
	    else
		j = 0;
    fprintf(stdout, "%*.*f %s", 7 - j, j, t, units[i]);
    return;
}


/* this pmResult is built during parsing of each pmlc statement.
 * the metrics and indoms likewise
 */
extern pmResult	*logreq;
extern metric_t	*metric;
extern int	n_metrics;
extern indom_t	*indom;
extern int	n_indoms;

void
Query(void)
{
    int		i, j, k, inst;
    metric_t	*mp;
    pmValueSet	*vsp;
    pmResult	*res;

    if (!connected())
	return;

    if ((i = __pmControlLog(logger_fd, logreq, PM_LOG_ENQUIRE, 0, 0, &res)) < 0) {
	fprintf(stderr, "Error: ");
	if (still_connected(i))
	    fprintf(stderr, "%s\n", pmErrStr(i));
	return;
    }
    if (res == NULL) {
	fprintf(stderr, "Error: NULL result from __pmControlLog\n");
	return;
    }

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	vsp = res->vset[i];
	if (mp->pmid != vsp->pmid) {
	    fprintf(stderr, "GAK! %s not found in returned result\n", mp->name);
	    return;
	}
	puts(mp->name);
	if (vsp->numval < 0) {
	    fputs("    ", stdout);
	    puts(pmErrStr(vsp->numval));
	}
	else if (mp->indom == -1) {
	    fputs("    ", stdout);
	    PrintState(vsp->vlist[0].value.lval);
	    putchar('\n');
	}
	else if (mp->status.has_insts)
	    for (j = 0; j < mp->n_insts; j++) {
		inst = mp->inst[j];
		for (k = 0; k < vsp->numval; k++)
		    if (inst == vsp->vlist[k].inst)
			break;
		if (k >= vsp->numval) {
		    printf("                  [%d] (not found)\n", inst);
		    continue;
		}
		fputs("    ", stdout);
		PrintState(vsp->vlist[k].value.lval);
		for (k = 0; k < indom[mp->indom].n_insts; k++)
		    if (inst == indom[mp->indom].inst[k])
			break;
		printf(" [%d or \"%s\"]\n", inst,
		       (k < indom[mp->indom].n_insts) ? indom[mp->indom].name[k] : "???");
	    }
	else {
	    if (vsp->numval <= 0)
		puts("    (no instances)");
	    else
		for (j = 0; j < vsp->numval; j++) {
		    fputs("    ", stdout);
		    PrintState(vsp->vlist[j].value.lval);
		    inst = vsp->vlist[j].inst;
		    for (k = 0; k < indom[mp->indom].n_insts; k++)
			if (inst == indom[mp->indom].inst[k])
			    break;
		    printf(" [%d or \"%s\"]\n",
			   inst,
			   (k < indom[mp->indom].n_insts) ? indom[mp->indom].name[k] : "???");
		}
	}
	putchar('\n');
    }
    pmFreeResult(res);
}

void LogCtl(int control, int state, int delta)
{
    int		i;
    metric_t	*mp;
    pmValueSet	*vsp;
    pmResult	*res;
    int		newstate, newdelta;	/* from pmlogger */
    int		expstate = 0;		/* expected from pmlogger */
    int		expdelta;
    int		firsterr = 1;
    unsigned	statemask;
    static char	*heading = "Warning: unable to change logging state for:";

    if (!connected())
	return;

    i = __pmControlLog(logger_fd, logreq, control, state, delta, &res);
    if (i < 0 && i != PM_ERR_GENERIC) {
	fprintf(stderr, "Error: ");
	if (still_connected(i))
	    fprintf(stderr, "%s\n", pmErrStr(i));
	return;
    }
    if (res == NULL) {
	fprintf(stderr, "Error: NULL result from __pmControlLog\n");
	return;
    }

    /* Set up the state that we expect pmlogger to return.  The encoding for
     * control and state passed to __pmControlLog differs from that returned
     * in the result.
     */
    statemask = 0;
    if (state != PM_LOG_MAYBE) {
	PMLC_SET_MAND(expstate, (control == PM_LOG_MANDATORY) ? 1 : 0);
	PMLC_SET_ON(expstate, (state == PM_LOG_ON) ? 1 : 0);
	PMLC_SET_MAND(statemask, 1);
	PMLC_SET_ON(statemask, 1);
    }
    else {
	/* only mandatory+maybe is allowed by parser, which should return
	 * advisory+off OR advisory+on from pmlogger
	 */
	PMLC_SET_MAND(expstate, 0);
	PMLC_SET_MAND(statemask, 1);
	/* don't set ON bit in statemask; ignore returned on/off status */
    }

    expdelta = PMLC_GET_ON(expstate) ? delta : 0;

    for (i = 0, mp = metric; i < n_metrics; i++, mp++) {
	int	j, k, inst;
	int	hadinstmsg;
	char	*name;

	vsp = res->vset[i];
	if (mp->pmid != vsp->pmid) {
	    fprintf(stderr, "GAK! %s not found in returned result\n", mp->name);
	    return;
	}
	if (vsp->numval < 0) {
	    if (firsterr) {
		firsterr = 0;
		puts(heading);
	    }
	    printf("%s:%s\n", mp->name, pmErrStr(vsp->numval));
	    continue;
	}

	if (mp->indom == -1) {
	    newstate = PMLC_GET_STATE(vsp->vlist[0].value.lval) & statemask;
	    newdelta = PMLC_GET_DELTA(vsp->vlist[0].value.lval);
	    if (expstate != newstate || expdelta != newdelta) {
		if (firsterr) {
		    firsterr = 0;
		    puts(heading);
		}
		printf("%s\n\n", mp->name);
	    }
	}
	else if (mp->status.has_insts) {
	    long	val;

	    hadinstmsg = 0;
	    for (j = 0; j < mp->n_insts; j++) {
		inst = mp->inst[j];
		/* find inst name via mp->indom */
		for (k = 0; k < indom[mp->indom].n_insts; k++)
		    if (inst == indom[mp->indom].inst[k])
			break;
		if (k < indom[mp->indom].n_insts)
		    name = indom[mp->indom].name[k];
		else
		    name = "???";
		/* look for inst in pmValueSet from pmlogger */
		for (k = 0; k < vsp->numval; k++)
		    if (inst == vsp->vlist[k].inst)
			break;
		if (k >= vsp->numval) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s\n", mp->name);
		    }
		    printf("    [%d or \"%s\"] instance not found\n", inst, name);
		    continue;
		}
		val = vsp->vlist[k].value.lval;
		newstate = (int)PMLC_GET_STATE(val) & statemask;
		newdelta = PMLC_GET_DELTA(val);
		if (expstate != newstate || expdelta != newdelta) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s instance(s):\n", mp->name);
		    }
		    printf("    [%d or \"%s\"]\n", inst, name);
		}
	    }
	    if (hadinstmsg)
		putchar('\n');
	}
	else {
	    hadinstmsg = 0;
	    for (j = 0; j < vsp->numval; j++) {
		newstate = PMLC_GET_STATE(vsp->vlist[j].value.lval) & statemask;
		newdelta = PMLC_GET_DELTA(vsp->vlist[j].value.lval);
		if (expstate != newstate || expdelta != newdelta) {
		    if (firsterr) {
			firsterr = 0;
			puts(heading);
		    }
		    if (!hadinstmsg) {
			hadinstmsg = 1;
			printf("%s instance(s):\n", mp->name);
		    }
		    inst = vsp->vlist[j].inst;
		    for (k = 0; k < indom[mp->indom].n_insts; k++)
			if (inst == indom[mp->indom].inst[k])
			    break;
		    if (k < indom[mp->indom].n_insts)
			name = indom[mp->indom].name[k];
		    else
			name = "???";
		    printf("    [%d or \"%s\"]\n", inst, name);
		}
	    }
	    if (hadinstmsg)
		putchar('\n');
	}
    }
    pmFreeResult(res);
}

/*
 * Used to flag timezone changes.
 * These changes are only relevant to Status() so they are made here.
 */
int	tzchange = 0;

#define TZBUFSZ	30			/* for pmCtime buffers */

void Status(int pid, int primary)
{
    static int		localtz = -1;
    static char 	*ltzstr = "";	/* pmNewZone doesn't like null pointers */
    char		*str;
    __pmLoggerStatus	*lsp;
    __pmLoggerStatus_v1	*lsp_v1;
    static char		localzone[] = "local"; 
    static char		*zonename = localzone;
    char		*tzlogger;
    __pmTimeval		*start;
    __pmTimeval		*last;
    __pmTimeval		*timenow;
    char		*hostname;
    int			state;
    int			vol;
    long long		size;
    char		startbuf[TZBUFSZ];
    char		lastbuf[TZBUFSZ];
    char		timenowbuf[TZBUFSZ];
    int			sts;
    __pmPDU		*pb;
    __pmIPC		*ipc;

    if (!connected())
	return;

    if (__pmFdLookupIPC(logger_fd, &ipc) < 0) {
	fprintf(stderr, "Logger connection badly initialised.\n");
	return;
    }

    if (ipc->version >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 status request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_STATUS)) < 0) {
	    fprintf(stderr, "Error sending status request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if ((sts = __pmDecodeLogStatus(pb, &lsp)) < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	tzlogger = lsp->ls_tzlogger;
	start = &lsp->ls_start;
	last = &lsp->ls_last;
	timenow = &lsp->ls_timenow;
	hostname = lsp->ls_hostname;
	state = lsp->ls_state;
	vol = lsp->ls_vol;
	size = lsp->ls_size;
    }
    else {
	int	subtype, vlen;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 1 datax status request\n");
#endif
	if ((sts = __pmSendDataX(logger_fd, PDU_BINARY, PMLC_PDU_STATUS_REQ,
			    0, NULL)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) <= 0) {
	    if (sts == 0)
		/* end of file! */
		sts = PM_ERR_IPC;
	    fprintf(stderr, "Error receiving response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if ((sts = __pmDecodeDataX(pb, PDU_BINARY, &subtype,
			      &vlen, (void **)&lsp_v1)) < 0) {
	    fprintf(stderr, "Error decoding response from pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
	if (subtype != PMLC_PDU_STATUS || vlen != sizeof(__pmLoggerStatus_v1)) {
	    fprintf(stderr, "Incorrect data from pmlogger (version mismatch?)\n"
#ifdef PCP_DEBUG
		    "subtype=%d vlen=%d (expected %d)\n",
		    subtype, vlen, sizeof(__pmLoggerStatus_v1)
#endif
		    );
	    goto done;
	}
	tzlogger = lsp_v1->ls_tzlogger;
	start = &lsp_v1->ls_start;
	last = &lsp_v1->ls_last;
	timenow = &lsp_v1->ls_timenow;
	hostname = lsp_v1->ls_hostname;
	state = lsp_v1->ls_state;
	vol = lsp_v1->ls_vol;
	size = lsp_v1->ls_size;
    }

    if (tzchange) {
	switch (tztype) {
	    case TZ_LOCAL:
		if (localtz == -1) {
#if defined(IRIX6_5)
                    if (_MIPS_SYMBOL_PRESENT(__pmTimezone))
                        str = __pmTimezone();
                    else
                        str = getenv("TZ");
#else
                    str = __pmTimezone();
#endif
		    if (str != NULL)
			ltzstr = str;
		    localtz = pmNewZone(ltzstr);
		    /* (exits if it fails) */
		}
		else
		    pmUseZone(localtz);
		zonename = localzone;
		break;

	    case TZ_LOGGER:
		pmNewZone(tzlogger);		/* but keep me! */
		zonename = "pmlogger";
		break;

	    case TZ_OTHER:
		pmNewZone(tz);
		zonename = tz;
		break;
	}
    }
    tmp = start->tv_sec;
    pmCtime(&tmp, startbuf);
    startbuf[strlen(startbuf)-1] = '\0'; /* zap the '\n' at the end */
    tmp = last->tv_sec;
    pmCtime(&tmp, lastbuf);
    lastbuf[strlen(lastbuf)-1] = '\0';
    tmp = timenow->tv_sec;
    pmCtime(&tmp, timenowbuf);
    timenowbuf[strlen(timenowbuf)-1] = '\0';
    printf("pmlogger ");
    if (primary)
	printf("[primary]");
    else
	printf("[%d]", pid);
    printf(" on host %s is logging metrics from host %s\n",
	lasthost, hostname);
    if (ipc->version >= LOG_PDU_VERSION2)
	printf("PMCD host        %s\n", lsp->ls_fqdn);
    if (state == PM_LOG_STATE_NEW) {
	puts("logging hasn't started yet");
	goto done;
    }
    printf("log started      %s (times in %s time)\n"
	   "last log entry   %s\n"
	   "current time     %s\n"
	   "log volume       %d\n"
	   "log size         %lld\n",
	   startbuf, zonename, lastbuf, timenowbuf, vol, size);

done:
    __pmUnpinPDUBuf(pb);
    return;

}

void
Sync(void)
{
    int			sts;
    __pmPDU		*pb;
    __pmIPC		*ipc;

    if (!connected())
	return;

    if (__pmFdLookupIPC(logger_fd, &ipc) < 0) {
	fprintf(stderr, "Logger connection badly initialised.\n");
	return;
    }

    if (ipc->version >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 sync request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_SYNC)) < 0) {
	    fprintf(stderr, "Error sending sync request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 1 datax sync request\n");
#endif
	if ((sts = __pmSendDataX(logger_fd, PDU_BINARY, PMLC_PDU_SYNC,
			    0, NULL)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }
    if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) != PDU_ERROR) {
	if (sts == 0)
	    /* end of file! */
	    sts = PM_ERR_IPC;
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    __pmDecodeError(pb, PDU_BINARY, &sts);
    if (sts < 0) {
	fprintf(stderr, "Error decoding response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }

    __pmUnpinPDUBuf(pb);
    return;
}

void
NewVolume(void)
{
    int			sts;
    __pmPDU		*pb;
    __pmIPC		*ipc;

    if (!connected())
	return;

    if (__pmFdLookupIPC(logger_fd, &ipc) < 0) {
	fprintf(stderr, "Logger connection badly initialised.\n");
	return;
    }

    if (ipc->version >= LOG_PDU_VERSION2) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 2 newvol request\n");
#endif
	if ((sts = __pmSendLogRequest(logger_fd, LOG_REQUEST_NEWVOLUME)) < 0) {
	    fprintf(stderr, "Error sending newvolume request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU)
	    fprintf(stderr, "pmlc: sending version 1 datax newvol request\n");
#endif
	if ((sts = __pmSendDataX(logger_fd, PDU_BINARY, PMLC_PDU_NEWVOLUME,
			    0, NULL)) < 0) {
	    fprintf(stderr, "Error sending request to pmlogger: ");
	    if (still_connected(sts))
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    return;
	}
    }
    if ((sts = __pmGetPDU(logger_fd, PDU_BINARY, TIMEOUT_NEVER, &pb)) != PDU_ERROR) {
	if (sts == 0)
	    /* end of file! */
	    sts = PM_ERR_IPC;
	fprintf(stderr, "Error receiving response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    __pmDecodeError(pb, PDU_BINARY, &sts);
    if (sts < 0) {
	fprintf(stderr, "Error decoding response from pmlogger: ");
	if (still_connected(sts))
	    fprintf(stderr, "%s\n", pmErrStr(sts));
	return;
    }
    else
	fprintf(stderr, "New log volume %d\n", sts);

    __pmUnpinPDUBuf(pb);
    return;
}

int
connected(void)
{
    if (logger_fd == -1) {
	yyerror("Not connected to any pmlogger instance");
	return 0;
    }
    else
	return 1;
}

int
still_connected(int sts)
{
    if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) {
	fprintf(stderr, "Lost connection to the pmlogger instance\n");
	if (logger_fd != -1) {
	    __pmNoMoreInput(logger_fd);
	    __pmResetIPC(logger_fd);
	    close(logger_fd);
	    logger_fd = -1;
	}
	return 0;
    }

    return 1;
}
