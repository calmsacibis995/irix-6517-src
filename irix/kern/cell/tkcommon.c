/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: tkcommon.c,v 1.36 1997/07/23 01:34:07 beck Exp $"

#include "stdarg.h"
#include "tk_private.h"
#ifdef _KERNEL
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/kthread.h"
#include "sys/systm.h"
#else
#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "time.h"
#endif

/*
 * XXX should be tuneables ...
 */
uint_t __tk_tracemax = 10000;
int __tk_tracenow = 0; /* if set to 1 then print as we go */
int __tk_defaultmeter = 1;
uint_t __tk_meterhash = 512-1;

uint_t __tk_ndelays;

#ifndef _KERNEL
usptr_t *__tkus;
#define READCYCLE(x) (cycleis32 ? *(unsigned long *)(x) : *(x))
static int cycleis32 = 0;
static unsigned int cycleval;
#endif

void
__tk_lock_init(void)
{
#ifndef _KERNEL
	static char *aname = NULL;
	if (aname == NULL) {
		aname = tempnam(NULL, "tk");
	}
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITSIZE, 1024*1024);
	if ((__tkus = usinit(aname)) == NULL)
		abort();
#endif
}

void
__tk_create_lock(tklock_t *l)
{
#ifdef _KERNEL
	spinlock_init(l, "tk");
#else
	*l = usnewlock(__tkus);
#endif
}

void
__tk_destroy_lock(tklock_t *l)
{
#ifdef _KERNEL
	spinlock_destroy(l);
#else
	usfreelock(*l, __tkus);
#endif
}

void
__tk_fatal(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

#ifdef _KERNEL
	icmn_err(CE_PANIC, fmt, args);
	/* NOTREACHED */
#else
	vfprintf(stderr, fmt, args);
	abort();
#endif
	va_end(args);
}

/*
 * internal sprintf since kernel sprintf doesn't return # chars printed
 */
char *
__tk_sprintf(char *buf, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	return buf + strlen(buf);
}

char *__tk_levtab[] = {
	"NULL",
	"RD",
	"WR",
	"WR|RD",
	"SWR",
	"SWR|RD",
	"SWR|WR",
	"SWR|WR|RD",
};

char *
tk_sprint_tk_set(tk_set_t tset, char *buf)
{
	tk_set_t ts;
	int didprint = 0, level, class;
	char *p;

	p = buf;
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		if (didprint == 0) {
			didprint = 1;
		}
		p = __tk_sprintf(p, "<%d,%s>", class, __tk_levtab[level]);
	}
	if (didprint == 0)
		p = __tk_sprintf(p, "<null>");
	return p;
}

char *__tk_disptab[] = {
	"NULL",
	"CREC",
	"MREC",
	"ILL",
	"CLI",
	"ILL",
	"ILL",
	"ILL",
};

char *
tk_sprint_tk_disp(tk_disp_t why, char *buf)
{
	tk_disp_t ds;
	int didprint = 0, level, class;
	char *p;

	p = buf;
	for (ds = why, class = 0; ds; ds = ds >> TK_LWIDTH, class++) {
		if ((level = ds & TK_SET_MASK) == 0)
			continue;
		if (didprint == 0) {
			didprint = 1;
		}
		p = __tk_sprintf(p, "<%d,%s>", class, __tk_disptab[level]);
	}
	if (didprint == 0)
		p = __tk_sprintf(p, "<null>");
	return p;
}

#ifdef _KERNEL
#define OUTPUT		qprintf(
void
tk_print_tk_set(tk_set_t tset, char *fmt, ...)

#else

#define OUTPUT		fprintf(f,
void
tk_print_tk_set(tk_set_t tset, FILE *f, char *fmt, ...)
#endif
{
	char buf[128], *p;
	va_list ap;

	p = buf;
	if (fmt) {
		va_start(ap, fmt);
		vsprintf(p, fmt, ap);
		p += strlen(p);
	}
	p = tk_sprint_tk_set(tset, p);
	p = __tk_sprintf(p, "\n");
	OUTPUT buf);
}

/*
 * basic backoff
 */
void
tk_backoff(int bk)
{
	struct timespec ts;
	
	if (bk > 0) {
		/* REFERENCED (rv) */
		int rv;
		long nsec;
		nsec =  (1 << (bk * 5)) * 1000;
		ts.tv_sec = (int)(nsec / (1000 * 1000 * 1000));
		ts.tv_nsec =  nsec % (1000 * 1000 * 1000);
#if _KERNEL
		nano_delay(&ts);
#else
		rv = nanosleep(&ts, NULL);
		TK_ASSERT(rv == 0);
#endif
		__tk_ndelays++;
	}
}

#if TK_TRC
/*
 * Tracing
 */
static void tk_printentry(tktrace_t *, int);

static volatile int tkt_initted;
static tklock_t tkt_lock;
static uint tkt_pos;
static tktrace_t *tkt;
static char *tkt_cops[] = {
	"NULL",
	"C-SACQUIRE",
	"C-EACQUIRE",
	"C-SCACQUIRE",
	"C-ECACQUIRE",
	"C-SRELEASE",
	"C-ERELEASE",
	"C-SHOLD",
	"C-EHOLD",
	"C-SRECALL",
	"C-ERECALL",
	"C-SRETURNED",
	"C-ERETURNED",
	"C-SOBTAIN-CALLOUT",
	"C-EOBTAIN-CALLOUT",
	"C-SRETURNING",
	"C-ERETURNING",
	"C-CREATE",
	"C-DESTROY",
	"C-SOBTAINING",
	"C-EOBTAINING",
	"C-SOBTAINED",
	"C-EOBTAINED",
	"C-EOBTAINING2",
	"C-RETURN-CALLOUT",
	"C-CACQUIRE1",
	"C-RELEASE1",
};
static char *tkt_sops[] = {
	"NULL",
	"S-SOBTAIN",
	"S-EOBTAIN",
	"S-SRETURN",
	"S-ERETURN",
	"???",
	"S-SRECALL",
	"S-ERECALL",
	"S-RECALL-CALLOUT",
	"S-RECALLED-CALLOUT",
	"S-SRETURN2",
	"S-CREATE",
	"S-DESTROY",
	"S-ITERATE",
	"S-ITERATE-CALLOUT",
	"S-SCLEAN",
	"S-ECLEAN",
	"S-NOTIFY-IDLE",
	"S-IDLE-CALLOUT",
	"S-SCOBTAIN",
	"S_ESCOBTAIN",
};

/*
 * Note that tracing is kept global over all cells to make the
 * tracing more effective. This means that each cell will attempt
 * to initialize this
 */
void
__tk_trace_init(void)
{
#if _KERNEL
	int rv;

	rv = test_and_set_int((int *)&tkt_initted, 2); /* returns old value */
	if (rv == 2) {
		/* someone else initializing - wait */
		while (tkt_initted == 2)
			;
		return;
	} else if (rv == 1) {
		tkt_initted = 1;
		return;
	}
#else
	if (tkt_initted)
		return;
#endif
	__tk_create_lock(&tkt_lock);
	tkt = TK_MALLOC(__tk_tracemax * sizeof(*tkt));
	bzero(tkt, (int)__tk_tracemax * (int)sizeof(*tkt));
	tkt_initted = 1;
}

/*
 * matching routines for trace log dumping
 * A match criteria consists of:
 * {letter}[cs]{|&}[{value}[,{values},]...]{;}[{letter}...]
 *
 * Examples:
 *	dump all client records for host 2 - hc&2;
 *	dump all client/server records for id 400 - p&400;
 *	dump all records to tokens named dshm or vshm - n&dshm,vshm;
 *	dump all clients for host 1 and all server records for id 10 -
 *				hc|1;ps|10;
 * default is - *cs|; i.e. all client and server records.
 * Note that '*' really only means anything for OR terms.
 *
 * 'c' and 's' are special - they always modify everything else
 * just 'c' or 's' implies not the other. specifying neither implies both.
 */
/* fields supported for matching */
#define TK_TRACE_PID	0	/* 'p' */
#define TK_TRACE_HOST	1	/* 'h' */
#define TK_TRACE_NAME	2	/* 'n' */
#define TK_TRACE_TAG	3	/* 't' */
#define TK_NTRACE_TYPES 4
#define TK_TRACE_ANY	TK_NTRACE_TYPES

/*
 * if the PID, HOST, TAG, or NAME values are NULL then there is no criteria
 * against those types.
 */
#define TK_TRACE_AND	0
#define TK_TRACE_OR	1
static int trace_client[2], trace_server[2];	/* per AND/OR */
static char *trace_and_terms[TK_NTRACE_TYPES];
static char *trace_or_terms[TK_NTRACE_TYPES];
#define IS_CL(t)	(!((t)->tkt_op & TKS_OP))
#define IS_SVR(t)	((t)->tkt_op & TKS_OP)

#define SET_CL(type,field)	(trace_client[type] |= (1 << (field)))
#define WANT_CL(type,field)	(trace_client[type] & (1 << (field)))
#define SET_SVR(type,field)	(trace_server[type] |= (1 << (field)))
#define WANT_SVR(type,field)	(trace_server[type] & (1 << (field)))

static int
setmatch(char *criteria)
{
	char *c, *cp;
	char *sep;
	int i, found = 0;
	int curcrit, crittype, wantclient, wantserver;

	bzero(trace_and_terms, sizeof(trace_and_terms));
	bzero(trace_or_terms, sizeof(trace_or_terms));
	bzero(trace_client, sizeof(trace_client));
	bzero(trace_server, sizeof(trace_server));

	cp = criteria;
	sep = "&|";
	curcrit = -1;
	while (cp && ((c = strpbrk(cp, sep)) != NULL)) {
		if (*c == '&' || *c == '|') {
			wantserver = wantclient = 0;
			if (*c == '&')
				crittype = TK_TRACE_AND;
			else
				crittype = TK_TRACE_OR;
			*c++ = '\0';
			/* dealing with action letter */
			if (curcrit != -1)
				return 0;
			sep = ";";
			switch (*cp) {
			case 'p': curcrit = TK_TRACE_PID; break;
			case 'h': curcrit = TK_TRACE_HOST; break;
			case 'n': curcrit = TK_TRACE_NAME; break;
			case 't': curcrit = TK_TRACE_TAG; break;
			case '*': curcrit = TK_TRACE_ANY; break;
			default:
				return 0;
			}
			while (*++cp) {
				if (*cp == 's') wantserver = 1;
				if (*cp == 'c') wantclient = 1;
			}
			if (wantserver == 0 && wantclient == 0)
				wantserver = wantclient = 1;
			if (curcrit != TK_TRACE_ANY) {
				if (wantclient)
					SET_CL(crittype, curcrit);
				if (wantserver)
					SET_SVR(crittype, curcrit);
			} else {
				for (i = 0; i < TK_NTRACE_TYPES; i++) {
					if (wantclient)
						SET_CL(crittype, i);
					if (wantserver)
						SET_SVR(crittype, i);
				}
			}
		} else {
			/* dealing with values */
			if (curcrit < 0)
				return 0;
			*c++ = '\0';
			if (crittype == TK_TRACE_AND)
				trace_and_terms[curcrit] = cp;
			else
				trace_or_terms[curcrit]= cp;
			curcrit = -1;
			sep = "&|";
			found++;
		}
		cp = c;
	}
	if (!found) {
		/* nothing worked/NULL - set to default */
		for (i = 0; i < TK_NTRACE_TYPES; i++) {
			SET_CL(TK_TRACE_OR, i);
			SET_SVR(TK_TRACE_OR, i);
		}
	}
	/* all parsed ok */
	return 1;
}

static int
lookfornum(char *cp, long value)
{
	char *c;
	char *lasts;
	long val;

	lasts = NULL;
	while ((c = strtok_r(cp, ",", &lasts)) != NULL) {
		cp = NULL;
#if _KERNEL
		val = atoi(c);
#else
		val = strtol(c, NULL, 0);
#endif
		if (val == value)
			return 1;
	}
	return 0;
}

static int
match(tktrace_t *t)
{
	char *cp;

	/* do 'AND' terms first */
	if (cp = trace_and_terms[TK_TRACE_HOST]) {
		/* have a term - see if matches */
		if ((IS_CL(t) && !WANT_CL(TK_TRACE_AND, TK_TRACE_HOST)) ||
		    (IS_SVR(t) && !WANT_SVR(TK_TRACE_AND, TK_TRACE_HOST)) ||
		    (!lookfornum(cp, (long)t->tkt_chandle)))
			goto or;
	}
	if (cp = trace_and_terms[TK_TRACE_PID]) {
		/* have a term - see if matches */
		if ((IS_CL(t) && !WANT_CL(TK_TRACE_AND, TK_TRACE_PID)) ||
		    (IS_SVR(t) && !WANT_SVR(TK_TRACE_AND, TK_TRACE_PID)) ||
		    (!lookfornum(cp, (long)t->tkt_id)))
			goto or;
	}
	if (cp = trace_and_terms[TK_TRACE_TAG]) {
		/* have a term - see if matches */
		if ((IS_CL(t) && !WANT_CL(TK_TRACE_AND, TK_TRACE_TAG)) ||
		    (IS_SVR(t) && !WANT_SVR(TK_TRACE_AND, TK_TRACE_TAG)) ||
		    (!lookfornum(cp, (long)t->tkt_tag)))
			goto or;
	}

	/* passed 'and' terms */
	return 1;

	/*
	 * now check 'OR' terms
	 */
or:
	cp = trace_or_terms[TK_TRACE_HOST];
	if (((IS_CL(t) && WANT_CL(TK_TRACE_OR, TK_TRACE_HOST)) ||
	    (IS_SVR(t) && WANT_SVR(TK_TRACE_OR, TK_TRACE_HOST))) &&
	    (!cp || (lookfornum(cp, (long)t->tkt_chandle))))
		return 1;
	cp = trace_or_terms[TK_TRACE_PID];

	if (((IS_CL(t) && WANT_CL(TK_TRACE_OR, TK_TRACE_PID)) ||
	    (IS_SVR(t) && WANT_SVR(TK_TRACE_OR, TK_TRACE_PID))) &&
	    (!cp || (lookfornum(cp, (long)t->tkt_id))))
			return 1;

	cp = trace_or_terms[TK_TRACE_TAG];
	if (((IS_CL(t) && WANT_CL(TK_TRACE_OR, TK_TRACE_TAG)) ||
	    (IS_SVR(t) && WANT_SVR(TK_TRACE_OR, TK_TRACE_TAG))) &&
	    (!cp || (lookfornum(cp, (long)t->tkt_tag))))
		return 1;

	/* didn't pass anything */
	return 0;
}

int
__tk_trace(int op,
	void *callpc,
	tks_ch_t h,
	tk_set_t act,
	tk_set_t act2,
	tk_set_t act3,
	void *s,
	tk_meter_t *tm)
{
	tksi_sstate_t *sstate;
	tkci_cstate_t *cstate;
	tktrace_t *t;
	TK_LOCKDECL;

	if (op & TKS_OP) {
		sstate = (tksi_sstate_t *)s;
		ASSERT(sstate->tksi_origcell == cellid());
		if (sstate->tksi_flags & TKS_NOHIST)
			return 0;
	} else {
		cstate = (tkci_cstate_t *)s;
		ASSERT(cstate->tkci_origcell == cellid());
		if (cstate->tkci_flags & TKC_NOHIST)
			return 0;
	}
	TK_LOCK(tkt_lock);
	t = tkt + tkt_pos;
	tkt_pos = (tkt_pos + 1) % __tk_tracemax;
	TK_UNLOCK(tkt_lock);

	t->tkt_op = op;
	t->tkt_act = act;
	t->tkt_callpc = callpc;
	t->tkt_act = act;
	t->tkt_act2 = act2;
	t->tkt_act3 = act3;
	t->tkt_chandle = h;
	t->tkt_tk = s;
#ifdef _KERNEL
	t->tkt_id = kthread_getid();
	t->tkt_ts = (unsigned int)get_timestamp();
#else
	t->tkt_id = (uint64_t)get_pid();
	t->tkt_ts = READCYCLE(__tkts);
#endif
	if (tm) {
		t->tkt_tag = tm->tm_tag;
		bcopy(tm->tm_name, t->tkt_name, TK_NAME_SZ);
		t->tkt_name[TK_NAME_SZ-1] = '\0';
	} else {
		t->tkt_tag = 0;
		t->tkt_name[0] = '\0';
	}

	if (op & TKS_OP) {
		int i, j;
		char *bp;
		struct sdata *sd = &t->tkt_un.tkt_s;

		t->tkt_ntokens = sstate->tksi_ntokens;
		bcopy(sstate->tksi_state, sd->tkt_state,
			sizeof(sstate->tksi_state));
		sd->tkt_obj = sstate->tksi_obj;
		sd->tkt_nrecalls = sstate->tksi_nrecalls;
		bzero(sd->tkt_out_grants,
				TK_MAXTRBM * t->tkt_ntokens);
		bzero(sd->tkt_out_mrecalls,
				TK_MAXTRBM * t->tkt_ntokens);
		bzero(sd->tkt_out_crecalls,
				TK_MAXTRBM * t->tkt_ntokens);
		for (i = 0; i < t->tkt_ntokens; i++) {
			if (sd->tkt_state[i].svr_grants == 0)
				continue;
			bp = sstate->tksi_gbits + __tksi_goffset[i];
			for (j = 0; j < TK_MAXTRBM; j++)
				sd->tkt_out_grants[i][j] = *bp++;
		}
		/* save outstanding mrecalls */
		for (i = 0; i < t->tkt_ntokens; i++) {
			if (sd->tkt_state[i].svr_revoke == 0)
				continue;
			bp = TKS_GET_METER(tm, i, trackmrecall);
			if (bp == 0)
				continue;
			
			for (j = 0; j < TK_MAXTRBM; j++)
				sd->tkt_out_mrecalls[i][j] = *bp++;
		}
		/* save outstanding recalls */
		for (i = 0; i < t->tkt_ntokens; i++) {
			if (sd->tkt_state[i].svr_recall == 0)
				continue;
			bp = TKS_GET_METER(tm, i, trackcrecall);
			if (bp == 0)
				continue;
			
			for (j = 0; j < TK_MAXTRBM; j++)
				sd->tkt_out_crecalls[i][j] = *bp++;
		}
	} else {
		t->tkt_ntokens = cstate->tkci_ntokens;
		bcopy(cstate->tkci_state, t->tkt_un.tkt_cl.tkt_state,
			sizeof(cstate->tkci_state));
		t->tkt_un.tkt_cl.tkt_obj = cstate->tkci_obj;
	}
	if (__tk_tracenow)
		tk_printentry(t, TK_LOG_ALL);
	return 0;
}

static void
tk_printserverlogentry(tktrace_t *t, char *buf, int flag)
{
	tksi_svrstate_t tstate;
	int disp, i, op, prbits;
	char *tag, *p, buf2[128];

	if ((op = t->tkt_op) == 0) {
		*buf = '\0';
		return;
	}
	p = buf;
	tk_sprint_tk_set(t->tkt_act, buf2);
	switch(op) {
	default: tag = "set"; break;
	}
	p = __tk_sprintf(p, "%s:%s:%s",
			tkt_sops[op & ~TKS_OP],
			tag,
			buf2);

	/* 
	 * second set
	 */
	tag = NULL;
	switch(op) {
	case TKS_ECOBTAIN:
	case TKS_EOBTAIN: tag = "already"; break;
	case TKS_SOBTAIN: tag = "to-return"; break;
	case TKS_SCOBTAIN: tag = "conditional"; break;
	case TKS_RECALLED: tag = "not-done"; break;
	case TKS_SRETURN: tag = "refused"; break;
	}
	if (tag || t->tkt_act2) {
		tk_sprint_tk_set(t->tkt_act2, buf2);
		if (!tag)
			tag = "set";
		p = __tk_sprintf(p, " %s:%s", tag, buf2);
	}

	/*
	 * 3rd set
	 */
	tag = NULL;
	disp = 0;
	switch(op) {
	case TKS_SRETURN: tag = "eh?"; break;
	case TKS_SRETURN2:
	case TKS_ECLEAN:
	case TKS_RECALL_CALL: tag = "disp"; disp = 1; break;
	}
	if (tag || t->tkt_act3) {
		if (disp)
			tk_sprint_tk_disp(t->tkt_act3, buf2);
		else
			tk_sprint_tk_set(t->tkt_act3, buf2);
		if (!tag)
			tag = "set";
		p = __tk_sprintf(p, " %s:%s", tag, buf2);
	}

	p = __tk_sprintf(p, " nrecalls %d\n",
			t->tkt_un.tkt_s.tkt_nrecalls);

	p = __tk_sprintf(p, "    cell %d \"%s\"(0x%p) tag 0x%lx\n",
			t->tkt_chandle,
			t->tkt_name,
			t->tkt_tk,
			t->tkt_tag);

	if (flag & TK_LOG_TS) {
		p = __tk_sprintf(p, "    id %lld obj 0x%p ntk %d",
			t->tkt_id,
			t->tkt_un.tkt_s.tkt_obj,
			t->tkt_ntokens);
#ifdef _KERNEL
		p = __tk_sprintf(p, " ts 0x%x", t->tkt_ts);
#else
		p = __tk_sprintf(p, " ts %llduS delta %llduS",
				(t->tkt_ts * cycleval) / (1000 * 1000),
				__tk_ccdelta(t->tkt_ts));
#endif
		if (t->tkt_callpc)
			p = __tk_sprintf(p, " pc 0x%x\n", t->tkt_callpc);
		else
			p = __tk_sprintf(p, "\n");
	}

	if (flag & TK_LOG_STATE) {
		for (i = 0; i < t->tkt_ntokens; i++) {
			prbits = 0;
			tstate = t->tkt_un.tkt_s.tkt_state[i];
#if _KERNEL
			p = __tk_sprintf(p, "\t<%d,%s> grants %w32d rwait %w32d lock %w32d lockw %w32d noutreqs %w32d",
#else
			p = __tk_sprintf(p, "\t<%d,%s> grants %d rwait %d lock %d lockw %d noutreqs %d",
#endif
					i,
					__tk_levtab[tstate.svr_level],
					tstate.svr_grants,
					tstate.svr_rwait,
					tstate.svr_lock,
					tstate.svr_lockw,
					tstate.svr_noutreqs);
			if (tstate.svr_backoff) {
#if _KERNEL
				p = __tk_sprintf(p, " bk %w32d",
#else
				p = __tk_sprintf(p, " bk %d",
#endif
					tstate.svr_backoff);
			}
			p = __tk_sprintf(p, "%s%s%s%s\n",
				tstate.svr_recall ? " CRECALL" : "",
				tstate.svr_revoke ? " MRECALL" : "",
				tstate.svr_revinp ? " MRECALL-IN-PROG" : "",
				tstate.svr_noteidle ? " NOTEIDLE" : "");

			if (tstate.svr_grants) {
				p = __tksi_prbits(p,
					&t->tkt_un.tkt_s.tkt_out_grants[i][0],
					TK_MAXTRBM*8, "\tgrants@:", " ");
				prbits = 1;
			}
			if (tstate.svr_recall) {
				if (prbits == 0) {
					prbits = 1;
					p = __tk_sprintf(p, "\t");
				}
				p = __tksi_prbits(p,
					&t->tkt_un.tkt_s.tkt_out_crecalls[i][0],
					TK_MAXTRBM*8, "crecalls@:", " ");
			}
			if (tstate.svr_revoke) {
				if (prbits == 0) {
					prbits = 1;
					p = __tk_sprintf(p, "\t");
				}
				p = __tksi_prbits(p,
					&t->tkt_un.tkt_s.tkt_out_mrecalls[i][0],
					TK_MAXTRBM*8, "mrecalls@:", NULL);
			}
			if (prbits)
				p = __tk_sprintf(p, "\n");
		}
	}
}

static void
tk_printclientlogentry(tktrace_t *t, char *buf, int flag)
{
	tkci_clstate_t tstate;
	int disp, i, op;
	char *tag;
	char *p, buf2[128];

	*buf = '\0';
	if ((op = t->tkt_op) == 0)
		return;
	if (flag == 0 && t->tkt_act == TK_NULLSET) {
		/* ignore boring trace records */
		switch (op) {
		case TKC_ERELEASE:
		case TKC_ERETURNED:
			return;
		}
	}

	/*
	 * decode first set - always a token set
	 */
	p = buf;
	tk_sprint_tk_set(t->tkt_act, buf2);
	switch (op) {
	case TKC_SOBTAINED:
	case TKC_EHOLD: tag = "obtained"; break;
	case TKC_ERELEASE:
	case TKC_ERETURNED:
	case TKC_ERECALL:
	case TKC_SRECALL:
	case TKC_RETURN_CALL: tag = "to-return"; break;
	case TKC_EOBTAINING: tag = "to-obtain"; break;
	case TKC_EOBTAINING2: tag = "to-return"; break;
	case TKC_ERETURNING: tag = "ok"; break;
	default: tag = "set"; break;
	}
	p = __tk_sprintf(p, "%s:%s:%s",
			tkt_cops[op],
			tag,
			buf2);
	/*
	 * second set
	 */
	tag = NULL;
	switch (op) {
	case TKC_ERECALL:
	case TKC_RETURN_CALL: tag = "eh?"; break;
	case TKC_SOBTAINED:
	case TKC_SRETURNED: tag = "refused"; break;
	case TKC_EOBTAINING: tag = "get-later"; break;
	}
	if (tag || t->tkt_act2) {
		tk_sprint_tk_set(t->tkt_act2, buf2);
		if (!tag)
			tag = "set";
		p = __tk_sprintf(p, " %s:%s", tag, buf2);
	}

	/*
	 * decode 3rd set - sometimes a disposition
	 */
	tag = NULL;
	disp = 0;
	switch (op) {
	case TKC_EOBTAINING: tag = "refused"; break;
	case TKC_EOBTAINING2:
	case TKC_RETURN_CALL:
	case TKC_ERELEASE:
	case TKC_SRECALL:
	case TKC_ERECALL: tag = "disp"; disp = 1; break;
	}
	if (tag || t->tkt_act3) {
		if (disp)
			tk_sprint_tk_disp(t->tkt_act3, buf2);
		else
			tk_sprint_tk_set(t->tkt_act3, buf2);
		if (!tag)
			tag = "set";
		p = __tk_sprintf(p, " %s:%s", tag, buf2);
	}

	p = __tk_sprintf(p, "\n");
		
	p = __tk_sprintf(p, "    cell %d \"%s\"(0x%p) tag 0x%lx\n",
			t->tkt_chandle,
			t->tkt_name,
			t->tkt_tk,
			t->tkt_tag);
		
	if (flag & TK_LOG_TS) {
		p = __tk_sprintf(p, "    id %lld obj 0x%p ntk %d ",
				t->tkt_id,
				t->tkt_un.tkt_cl.tkt_obj,
				t->tkt_ntokens);
#ifdef _KERNEL
		p = __tk_sprintf(p, "ts 0x%x", t->tkt_ts);
#else
		p = __tk_sprintf(p, "ts %llduS delta %llduS",
				(t->tkt_ts * cycleval) / (1000 * 1000),
				__tk_ccdelta(t->tkt_ts));
#endif
		if (t->tkt_callpc)
			p = __tk_sprintf(p, " pc 0x%x\n", t->tkt_callpc);
		else
			p = __tk_sprintf(p, "\n");
	}

	if (flag & TK_LOG_STATE) {
		for (i = 0; i < t->tkt_ntokens; i++) {
			tstate = t->tkt_un.tkt_cl.tkt_state[i];
			if (tstate.cl_state == TK_CL_IDLE &&
			    tstate.cl_mrecall == 0 &&
			    tstate.cl_scwait == 0 &&
			    tstate.cl_obtain == 0)
				continue;
			p = __tk_sprintf(p, "\t<%d,%s> hold %d state %s mrecall %s obtain %s wait %s\n",
					i,
					__tk_levtab[tstate.cl_extlev],
					tstate.cl_hold,
					__tkci_prstate(tstate),
					__tk_levtab[tstate.cl_mrecall],
					__tk_levtab[tstate.cl_obtain],
					tstate.cl_scwait ? "YES" : "NO"
					);
		}
	}
}

static void
tk_printentry(tktrace_t *t, int flag)
{
	char buf[1024];
	if (t->tkt_op & TKS_OP)
		tk_printserverlogentry(t, buf, flag);
	else
		tk_printclientlogentry(t, buf, flag);
	printf(buf);
}

#ifdef _KERNEL
#define OUTPUT		qprintf(
/* ARGSUSED */
void
tk_printlog(int n, int flag, char *criteria)

#else

#define OUTPUT		fprintf(f,
void
tk_printlog(FILE *f, int n, int flag, char *criteria)
#endif
{
	int i, start1, n1, n2;
	tktrace_t *t;
	char buf[1024];

	if (n == -1L) {
		n = __tk_tracemax;
	} else {
		n = __tk_tracemax > n ? n : __tk_tracemax;
	}

	if (n > tkt_pos) {
		n1 = n - tkt_pos;
		start1 = __tk_tracemax - n1;
		n2 = tkt_pos;
	} else {
		n1 = n;
		start1 = tkt_pos - n;
		n2 = 0;
	}

	if (!setmatch(criteria)) {
		OUTPUT "Bad criteria:%s\n", criteria);
		return;
	}
#if NEVER
	if (criteria) {
		for (i = 0; i < TK_NTRACE_TYPES; i++)
			OUTPUT "Type %d AND criteria:<%s>\n", i, trace_and_terms[i]);
		for (i = 0; i < TK_NTRACE_TYPES; i++)
			OUTPUT "Type %d OR criteria:<%s>\n", i, trace_or_terms[i]);
	}
#endif

	for (i = start1; i < (start1 + n1); i++) {
		t = &tkt[i];
		if (!t)
			break;
		if (!match(t)) continue;
		if (t->tkt_op & TKS_OP)
			tk_printserverlogentry(t, buf, flag);
		else
			tk_printclientlogentry(t, buf, flag);
		OUTPUT buf);
	}
	for (i = 0; i < n2; i++) {
		t = &tkt[i];
		if (!match(t)) continue;
		if (t->tkt_op & TKS_OP)
			tk_printserverlogentry(t, buf, flag);
		else
			tk_printclientlogentry(t, buf, flag);
		OUTPUT buf);
	}
}

#else /* !TK_TRC */
#ifdef _KERNEL
#define OUTPUT		qprintf(
/* ARGSUSED */
void
tk_printlog(int n, int flag, char *criteria)

#else

#define OUTPUT		fprintf(f,
/* ARGSUSED */
void
tk_printlog(FILE *f, int n, int flag, char *criteria)
#endif
{
}
#endif /* TK_TRC */

#if TK_METER

#define TKMBUCKET_NUM(a) ((int)((__psint_t)(a) & __tk_meterhash))
#define TKMHASH(i)	tkmeterbuckets[i]
#define TKMETER_LOCK(i)	tkmeter_locks[i]
static tk_meter_t **tkmeterbuckets;
static tklock_t *tkmeter_locks;
static int tkm_initted;

void
__tk_meter_init(void)
{
	int i;

	if (tkm_initted)
		return;
	tkm_initted = 1;
	tkmeter_locks = TK_NODEMALLOC((__tk_meterhash+1) * sizeof(tklock_t *));
	for (i = 0; i < (__tk_meterhash+1); i++) {
		__tk_create_lock(&tkmeter_locks[i]);
	}
	tkmeterbuckets = TK_NODEMALLOC((__tk_meterhash+1) * sizeof(tk_meter_t *));
	bzero(tkmeterbuckets, (int)(__tk_meterhash+1) * (int)sizeof(tk_meter_t *));
}

/*
 * client side metering
 */
tk_meter_t *
tkc_meteron(tkc_state_t *cs, char *name, void *tag)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tk_meter_t *tm, *tms;
	tk_meter_t **tmp;
	int i;
	TK_LOCKDECL;

	if (ci->tkci_flags & TKC_METER)
		return tkc_meter(cs);

	tm = TK_NODEMALLOC(sizeof(*tm));
	bzero(tm, (int)sizeof(*tm));

	bcopy(name, tm->tm_name, TK_NAME_SZ);
	tm->tm_name[TK_NAME_SZ-1] = '\0';
	tm->tm_h = cs;
	tm->tm_which = TK_METER_CLIENT;
	tm->tm_tag = tag;

	i = TKMBUCKET_NUM(ci);
	tmp = &TKMHASH(i);
	TK_LOCK(TKMETER_LOCK(i));

	while (tms = *tmp) {
		if (tms->tm_h == (void *)cs) {
			/* dup - free up */
			TK_UNLOCK(TKMETER_LOCK(i));
#if _KERNEL
			cmn_err(CE_WARN,
#else
			fprintf(stderr,
#endif
				"tkcmeter dup @ 0x%x for tkc 0x%x\n", tms, cs);
			TK_NODEFREE(tm);
			goto out;
		}
		tmp = &tms->tm_link;
	}
	*tmp = tm;
	TK_UNLOCK(TKMETER_LOCK(i));

out:
	TK_LOCK(ci->tkci_lock);
	ci->tkci_flags |= TKC_METER;
	TK_UNLOCK(ci->tkci_lock);

	return tm;
}

void
tkc_meteroff(tkc_state_t *cs)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tk_meter_t *tm;
	tk_meter_t **tmp;
	int i;
	TK_LOCKDECL;

	if (!(ci->tkci_flags & TKC_METER))
		return;
	ci->tkci_flags &= ~TKC_METER;
	i = TKMBUCKET_NUM(ci);
	tmp = &TKMHASH(i);
	TK_LOCK(TKMETER_LOCK(i));
	TK_ASSERT(*tmp);

	while (tm = *tmp) {
		if (tm->tm_h == (void *)ci) {
			TK_ASSERT(tm->tm_which == TK_METER_CLIENT);
			*tmp = tm->tm_link;
			TK_UNLOCK(TKMETER_LOCK(i));
			TK_NODEFREE(tm);
			return;
		} 
		tmp = &tm->tm_link;
	}
	TK_UNLOCK(TKMETER_LOCK(i));
}

/*
 * return client meter struct
 */
tk_meter_t *
tkc_meter(tkc_state_t *cs)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tk_meter_t *tm;
	int i;
	TK_LOCKDECL;

	if ((ci->tkci_flags & TKC_METER) == 0)
		return NULL;

	ASSERT(ci->tkci_origcell == cellid());
	i = TKMBUCKET_NUM(ci);
	TK_LOCK(TKMETER_LOCK(i));

	for (tm = TKMHASH(i); tm; tm = tm->tm_link)
		if (tm->tm_h == (void *)ci)
			break;
	TK_UNLOCK(TKMETER_LOCK(i));
#if _KERNEL
	if (!tm) {
		qprintf("no meter for cs 0x%x bucket %d hash 0x%x\n",
			cs, i, TKMHASH(i));
		debug("ring");
	}
#endif
	return tm;
}

/*
 * server side metering
 */
tk_meter_t *
tks_meteron(tks_state_t *ss, char *name, void *tag)
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tk_meter_t *tm, *tms;
	tk_meter_t **tmp;
	int i;
	TK_LOCKDECL;

	if (si->tksi_flags & TKS_METER)
		return tks_meter(ss);

	tm = TK_NODEMALLOC(sizeof(*tm));
	bzero(tm, (int)sizeof(*tm));

	bcopy(name, tm->tm_name, TK_NAME_SZ);
	tm->tm_name[TK_NAME_SZ-1] = '\0';
	tm->tm_h = ss;
	tm->tm_which = TK_METER_SERVER;
	tm->tm_tag = tag;

	i = TKMBUCKET_NUM(si);
	tmp = &TKMHASH(i);
	TK_LOCK(TKMETER_LOCK(i));

	while (tms = *tmp) {
		if (tms->tm_h == (void *)ss) {
			/* dup - free up */
			TK_UNLOCK(TKMETER_LOCK(i));
#if _KERNEL
			cmn_err(CE_WARN,
#else
			fprintf(stderr,
#endif
				"tksmeter dup @ 0x%x for tks 0x%x\n", tms, ss);
			TK_NODEFREE(tm);
			goto out;
		}
		tmp = &tms->tm_link;
	}
	*tmp = tm;
	TK_UNLOCK(TKMETER_LOCK(i));

out:
	TK_LOCK(si->tksi_lock);
	si->tksi_flags |= TKS_METER;
	TK_UNLOCK(si->tksi_lock);

	return tm;
}

void
tks_meteroff(tks_state_t *ss)
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tk_meter_t *tm;
	tk_meter_t **tmp;
	int i;
	TK_LOCKDECL;

	if (!(si->tksi_flags & TKS_METER))
		return;
	si->tksi_flags &= ~TKS_METER;
	i = TKMBUCKET_NUM(si);
	tmp = &TKMHASH(i);
	TK_LOCK(TKMETER_LOCK(i));
	TK_ASSERT(*tmp);

	while (tm = *tmp) {
		if (tm->tm_h == (void *)ss) {
			TK_ASSERT(tm->tm_which == TK_METER_SERVER);
			*tmp = tm->tm_link;
			TK_UNLOCK(TKMETER_LOCK(i));
			TK_NODEFREE(tm);
			return;
		} 
		tmp = &tm->tm_link;
	}
	TK_UNLOCK(TKMETER_LOCK(i));
}

tk_meter_t *
tks_meter(tks_state_t *ss)
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tk_meter_t *tm;
	int i;
	TK_LOCKDECL;

	if ((si->tksi_flags & TKS_METER) == 0)
		return NULL;

	ASSERT(si->tksi_origcell == cellid());
	i = TKMBUCKET_NUM(si);
	TK_LOCK(TKMETER_LOCK(i));

	for (tm = TKMHASH(i); tm; tm = tm->tm_link)
		if (tm->tm_h == (void *)si)
			break;
	TK_UNLOCK(TKMETER_LOCK(i));
#if _KERNEL
	if (!tm) {
		qprintf("no meter for ss 0x%x bucket %d hash 0x%x\n",
			ss, i, TKMHASH(i));
		debug("ring");
	}
#endif
	return tm;
}

#else /* !TK_METER */

/* ARGSUSED */
tk_meter_t *tkc_meteron(tkc_state_t *cs, char *name, void *tag) { return NULL; }
/* ARGSUSED */
void tkc_meteroff(tkc_state_t *cs) {}
/* ARGSUSED */
tk_meter_t *tkc_meter(tkc_state_t *cs) { return NULL; }
/* ARGSUSED */
tk_meter_t *tks_meteron(tks_state_t *ss, char *name, void *tag) { return NULL; }
/* ARGSUSED */
void tks_meteroff(tks_state_t *ss) {}
/* ARGSUSED */
tk_meter_t *tks_meter(tks_state_t *ss) { return NULL; }

#endif /* TK_METER */

#ifndef _KERNEL
#include "sys/types.h"
#include "sys/syssgi.h"
#include "fcntl.h"
#include "sys/mman.h"

volatile unsigned long long *__tkts;
static int fd = -1;
static unsigned long long lastts;

volatile unsigned long long *
__tk_ts_init(void)
{
	__psunsigned_t phys_addr, raddr;
	int poffmask;

	if (fd >= 0)
		return __tkts;
	poffmask = getpagesize() - 1;
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	raddr = phys_addr & ~poffmask;
	fd = open("/dev/mmem", O_RDONLY);
	__tkts = (volatile unsigned long long *)mmap(0, poffmask, PROT_READ,
			MAP_PRIVATE, fd, (off_t)raddr);
	__tkts = (volatile unsigned long long *)
		((__psunsigned_t)__tkts + (phys_addr & poffmask));
	/* HACK! */
	if ((unsigned long long)__tkts & 0xf) cycleis32 = 1;

	lastts = READCYCLE(__tkts);
	return __tkts;
}

unsigned long long
__tk_ccdelta(unsigned long long cc)
{
	unsigned long long t = cc - lastts;
	lastts = cc;
	return (t * cycleval) / (1000 * 1000);
}
#endif

#ifndef _KERNEL
/*
 * user space implementation of sync variables
 * 1) requires hlding mutex while calling sv_broadcast
 * 2) mimics kernel - does not reacquire the mutex on exit from sv_wait.
 */
void
sv_create(sv_t *sync)
{
	sync->waiters = 0;
	TK_CREATE_SYNC(sync->wait);
}

void
sv_wait(sv_t *sync, ulock_t l)
{
	sync->waiters++;
	TK_ASSERT(sync->waiters > 0);
	usunsetlock(l);
	uspsema(sync->wait);
}

/*
 * XXX always called with mutex locked
 */
int
sv_broadcast(sv_t *sync)
{
	int rv;

	if ((rv = sync->waiters) == 0)
		return 0;
	while (sync->waiters) {
		usvsema(sync->wait);
		sync->waiters--;
	}
	return rv;
}
#endif
