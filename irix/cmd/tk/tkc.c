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
#ident "$Id: tkc.c,v 1.43 1997/09/05 15:55:04 mostek Exp $"

#include "tk_private.h"

/*
 * Each token controlled object has a set of up to 10 tokens associated with it.
 * The state for entire set of tokens is maintined in the tkci_cstate_t
 * data structure. This structure is allocated by the client using
 * the TKC_DECL macro.
 * Any operation may act on any or all of the tokens for a particular object -
 * the interface deals exclusivly with tk_set_t's.
 */
static void tkci_obtainwake(tkci_cstate_t *, int);
static void tkci_obtainwait(tkci_cstate_t *, int, int *);
#if TK_DEBUG
static int tkci_nobtainwaiting(tkci_cstate_t *, int);
#endif
static void tkci_scwake(tkci_cstate_t *, int);
static void tkci_scwait(tkci_cstate_t *, int, int *);
static void tkci_badstate(char *, tkci_cstate_t *, int);
static void __tkc_obtaining(tkc_state_t *, tk_set_t, tk_set_t *,
			tk_set_t *, tk_disp_t *, tk_set_t *, tk_set_t *,
			int, void *);
static void __tkc_obtained(tkc_state_t *, tk_set_t, tk_set_t,
			tk_disp_t, int, void *);

/*
 * table lookup for extlev downgrading. Basically WRITE and anything else
 *	goes back to WRITE
 */
static unsigned char downext[] = {
	0,	/* No Token */
	1,	/* READ -> READ */
	2,	/* WRITE -> WRITE */
	2,	/* READ|WRITE -> WRITE */
	4,	/* SWRITE -> SWRITE */
	5,	/* SWRITE|READ -> ILLEGAL */
	2,	/* SWRITE|WRITE -> WRITE */
	7	/* READ|WRITE|SWRITE -> ILLEGAL */
};

/*
 * table lookup for which obtain bit to use. This defines the priority of
 * tokens if multiple obtains are outstanding. If we want WRITE then we get
 * that in preference, then SWRITE then READ.
 */
static unsigned char obtaintab[] = {
	0,
	1,	/* READ -> READ */
	2,	/* WRITE -> WRITE */
	2,	/* READ|WRITE -> WRITE */
	4,	/* SWRITE -> SWRITE */
	4,	/* SWRITE|READ -> SWRITE */
	2,	/* SWRITE|WRITE -> WRITE */
	2	/* SWRITE|WRITE|READ -> WRITE */
};

/*
 * macro to change states. Automatically calls scwake if the we should
 * call it when transitioning out of a state (based on waketab)
 */
#define NEWSTATE(ci,class,tstate,ns)  \
		{ int wk = waketab[(tstate)->cl_state]; \
		  (tstate)->cl_state = ns; \
		  if (wk && (tstate)->cl_scwait) tkci_scwake(ci, class); \
		}

static unsigned char waketab[] = {
	0,	/* 0 */
	0,	/* IDLE */
	0,	/* OWNED */
	0,	/* CACHED */
	1,	/* REVOKING */
	0,	/* OBTAINING */
	0,	/* ALREADY */
	1,	/* WILLRETURN */
	1,	/* RETURNING */
	1,	/* WILLOBTAIN */
	0,	/* BORROWED */
	0,	/* BORROWING */
	0,	/* REJECTED */
	1,	/* BREVOKING */
	1,	/* SREVOKING */
	0,	/* SOBTAINING */
	0,	/* COBTAINING */
	0,	/* BOBTAINING */
	1,	/* SWILLOBTAIN */
	1,	/* CREVOKING */
	0,	/* CRESTORE */
	0,	/* SRESTORE */
	0	/* BRESTORE */
};

static void tkc_local_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
static void tkc_local_return(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);

tkc_ifstate_t tkc_local_iface = {
	tkc_local_obtain,
	tkc_local_return
};

/*
 * testing/asserting state conditions
 */
#ifdef TK_DEBUG
#define CHECK(t) tkci_checkstate(t)

static void
tkci_checkstate(tkci_clstate_t *t)
{
	switch(t->cl_state) {
	case TK_CL_IDLE:
		/*
		 * IDLE - extlev null; no hold count;
		 *	noone waiting to obtain; possible revoke request.
		 */
		TK_ASSERT(t->cl_extlev == 0);
		TK_ASSERT(t->cl_hold == 0);
		TK_ASSERT(t->cl_obtain == 0);
		break;
	case TK_CL_OWNED:
	case TK_CL_BORROWED:
		/*
		 * OWNED - a valid extlev; a hold count; no revokes;
		 * and the obtain mask shouldn't contain any bits that are
		 * compatible with what we've already got
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ) && \
			  t->cl_extlev != 0);
		TK_ASSERT(t->cl_hold > 0);
		TK_ASSERT(t->cl_mrecall == 0);
		TK_ASSERT((t->cl_obtain & t->cl_extlev) == 0);
		break;
	case TK_CL_CACHED:
		/*
		 * CACHED - a valid singleton extlev (only one bit set);
		 * no hold count; noone waiting to obtain;
		 * noone wanting to revoke
		 */
		TK_ASSERT((t->cl_extlev & t->cl_extlev - 1) == 0);
		TK_ASSERT(t->cl_hold == 0);
		TK_ASSERT(t->cl_mrecall == 0);
		TK_ASSERT(t->cl_obtain == 0);
		break;
	case TK_CL_OBTAINING:
	case TK_CL_SOBTAINING:
	case TK_CL_COBTAINING:
	case TK_CL_BOBTAINING:
		/*
		 * OBTAINING - hold count > 0; valid extlev; potential revoke;
		 * obtain mask may be anything
		 * SOBTAINING - same.
		 * COBTAINING - same.
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ) && \
			  t->cl_extlev != 0);
		TK_ASSERT(t->cl_hold > 0);
		break;
	case TK_CL_RETURNING:
	case TK_CL_CREVOKING:
		/*
		 * RETURNING - hold count == 0; extlev singleton
		 * CREVOKING - 
		 */
		TK_ASSERT(t->cl_hold == 0);
		TK_ASSERT((t->cl_extlev & t->cl_extlev - 1) == 0);
		break;
	case TK_CL_WILLRETURN:
		/*
		 * WILLRETURN - a valid extlev; a hold count;
		 * and the obtain mask can be anything
		 * Unlike OSF - we permit mandatory recalls -
		 * to make sure that conditional recalls
		 * always succed
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ) && \
			  t->cl_extlev != 0);
		TK_ASSERT(t->cl_hold > 0);
		break;
	case TK_CL_REVOKING:
	case TK_CL_SREVOKING:
		/*
		 * REVOKING - a valid extlev; a hold count; no revokes;
		 * and the obtain mask can be anything
		 * SREVOKING - same except send a SERVER_MANDATORY msg
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ) && \
			  t->cl_extlev != 0);
		TK_ASSERT(t->cl_hold > 0);
		TK_ASSERT(t->cl_mrecall == 0);
		break;
	case TK_CL_BREVOKING:
		/*
		 * BREVOKING - same as REVOKING except that
		 * a) we should send a SERVER_CONDITIONAL msg
		 * b) an have a 0 hold count since we can transition from
		 * 	the CREVOKING state to here in tkci_creturn
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ) && \
			  t->cl_extlev != 0);
		TK_ASSERT(t->cl_mrecall == 0);
		break;
	case TK_CL_WILLOBTAIN:
	case TK_CL_SWILLOBTAIN:
		/*
		 * WILLOBTAIN - a valid singleton extlev (only one bit set);
		 * no hold count;
		 */
		TK_ASSERT(((t->cl_extlev & t->cl_extlev - 1) == 0) || \
					t->cl_extlev == 0);
		TK_ASSERT(t->cl_hold == 0);
		break;
	case TK_CL_REJECTED:
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ));
		TK_ASSERT(t->cl_hold > 0);
		TK_ASSERT((t->cl_obtain & t->cl_extlev) == 0);
		break;
	case TK_CL_BORROWING:
		/*
		 * BORROWING - similar to OBTAINING: hold count > 0;
		 * valid extlev;
		 * obtain mask may be anything
		 * XXX hard to say what the token paper means by
		 * 'extended level may show any non-null set of
		 * of compatible levels'. Does this mean it can be 0??
		 * For REJECTED we think so...
		 */
		TK_ASSERT(t->cl_extlev != (TK_SWRITE|TK_READ) && \
			  t->cl_extlev != (TK_SWRITE|TK_WRITE|TK_READ));
		TK_ASSERT(t->cl_hold > 0);
		TK_ASSERT(t->cl_mrecall == 0); \
		TK_ASSERT((t->cl_obtain & t->cl_extlev) == 0);
		break;
	default:
		__tk_fatal("unknown state t 0x%x state %s\n",
				t, __tkci_prstate(*t));
			/* NOTREACHED */
	}
}
#else /* TK_DEBUG */
#define CHECK(t)
#endif

/* obtain list manipulation */
static tk_list_t obtainhead;
static tklock_t obtainlock;
/* status change list */
static tk_list_t schead;
static tklock_t sclock;

/*
 * tkc_init - one time client side initialization
 */
void
tkc_init(void)
{
	__tk_lock_init();
	obtainhead.tk_next = obtainhead.tk_prev = &obtainhead;
	__tk_create_lock(&obtainlock);
	schead.tk_next = schead.tk_prev = &schead;
	__tk_create_lock(&sclock);
#if TK_TRC
	__tk_trace_init();
#endif
#if TK_METER
	__tk_meter_init();
#endif
#ifndef _KERNEL
	__tk_ts_init();
#endif
}

/*
 * tkc_local_obtain - get a token from a local token server
 */
static void
tkc_local_obtain(
	void		*tserver,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	whyreturn,
	tk_set_t	*refused)
{
	tk_set_t granted, already;

	/*
	 * XXX since we don't have the client tkc pointer, we can't
	 * really figure out in a generic way, which cell we're on.
	 * This is easy with an underlying OS, so we simply don't
	 * don't do this correctly for user level ..
	 */
	if (to_be_returned != TK_NULLSET)
		tks_return(tserver, TK_GETCH(NULL), to_be_returned,
			   TK_NULLSET, TK_NULLSET, whyreturn);
	tks_obtain(tserver, TK_GETCH(NULL), to_be_obtained, &granted,
		   refused, &already);
	TK_ASSERT(already == TK_NULLSET);
}

/*
 * tkc_local_return - return a token to a local token server
 *
 * For client initiated returns we must tell server first (emulating
 *	an RPC).
 * For server initiated returns we must tell the client first since
 *	once we tell the server, the entire token struct could be
 *	torn down
 */
static void
tkc_local_return(
	tkc_state_t tclient,
	void *tserver,
	tk_set_t tstoret,
	tk_set_t teh,
	tk_disp_t why)
{
	tk_set_t clientrev;

	/*
	 * compute the subset of tstoret that are to be
	 * dispositioned as client initiated return
	 */
	clientrev = why & TK_DISP_CLIENT_ALL;
	clientrev |= (clientrev >> 1) | (clientrev >> 2);
	clientrev &= tstoret;
	if (clientrev != TK_NULLSET) {
		tks_return(tserver, TK_GETCH(NULL),
				clientrev, TK_NULLSET, TK_NULLSET, why);
		tkc_returned(tclient, clientrev, TK_NULLSET);
		/* all rest are server initiated */
		TK_DIFF_SET(tstoret, clientrev);
	}
	if (tstoret != TK_NULLSET)
		tkc_returned(tclient, tstoret, TK_NULLSET);
	tks_return(tserver, TK_GETCH(NULL),
			tstoret, TK_NULLSET, teh, why);
}

/*
 * tkc_create_local - create a token managed object with local server
 */
void
tkc_create_local(
	char *name,		/* name for tracing/metering */
	tkc_state_t *cs,	/* object state to set up */
	tks_state_t *ss,	/* object server pointer */
	int ntokens,		/* # of tokens in set */
	tk_set_t tsetcached,	/* tokens to mark as present */
	tk_set_t tset,		/* tokens to mark as present and held */
	void *tag)		/* tag for tracing */
{
	tkc_create(name, cs, ss, &tkc_local_iface, ntokens, tsetcached,
					tset, tag);
}

/*
 * tkc_create - create a token managed object
 */
/* ARGSUSED */
void
tkc_create(
	char *name,		/* name for tracing/metering */
	tkc_state_t *cs,	/* object state to set up */
	void *o,		/* client object pointer */
	tkc_ifstate_t *is,	/* interface state */
	int ntokens,		/* # of tokens in set */
	tk_set_t tsetcached,	/* tokens to mark as present */
	tk_set_t tset,		/* tokens to mark as present and held */
	void *tag)		/* tag for tracing */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts, tsh;
	int i, class, level;
	tkci_clstate_t *tstate;
	TK_LOCKDECL;

	/*
	 * try to catch races by setting ntokens to 0, and only when
	 * everything is set up setting it to the 'real' number
	 */
	ci->tkci_ntokens = 0;
	ci->tkci_if = is;
	ci->tkci_obj = o;
	ci->tkci_flags = 0;
	ci->tkci_ref = 0;
#if _KERNEL
	ci->tkci_origcell = cellid();
#endif
	for (i = 0; i < ntokens; i++) {
		ci->tkci_state[i].cl_word = 0;
		ci->tkci_state[i].cl_state = TK_CL_IDLE;
	}
	__tk_create_lock(&ci->tkci_lock);

#if TK_METER
	tm = NULL;
	if (__tk_defaultmeter)
		tm = tkc_meteron(cs, name, tag);
#endif

	/* eveything wanted to be held must be in first set */
	ASSERT((tsetcached & tset) == tset);
	TK_LOCK(ci->tkci_lock);
	for (ts = tsetcached, tsh = tset, class = 0;
			ts;
			ts = ts >> TK_LWIDTH, tsh = tsh >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ntokens);
		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];
		tstate->cl_extlev = level;
		if (tsh & TK_SET_MASK) {
			/* wants it held */
			tstate->cl_state = TK_CL_OWNED;
			tstate->cl_hold = 1;
		} else {
			/* just cached please */
			tstate->cl_state = TK_CL_CACHED;
		}
	}
	TK_UNLOCK(ci->tkci_lock);

	ci->tkci_ntokens = ntokens;

	TK_TRACE(TKC_CREATE, __return_address, TK_GETCH(ci->tkci_obj),
			tsetcached, tset, TK_NULLSET, ci, TK_METER_VAR);

}

/*
 * tkc_destroy - stop managing an object.
 * It is assumed that NO tokens are held and that NO requests for any
 * more tokens can come in.
 */
void
tkc_destroy(tkc_state_t *cs)		/* object to be destroyed */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	int i;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */

	TKC_METER_VAR_INIT(tm, cs);

	ASSERT(ci->tkci_if != &tkc_local_iface);
	TK_TRACE(TKC_DESTROY, __return_address, TK_GETCH(ci->tkci_obj),
		TK_NULLSET, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	for (i = 0; i < ci->tkci_ntokens; i++) {
		tstate = &ci->tkci_state[i];
		if (tstate->cl_state != TK_CL_IDLE)
			__tk_fatal("tkc_destroy - not idle state cs 0x%x class %d state %s\n",
				cs, i, __tkci_prstate(*tstate));
			/* NOTREACHED */
	}

#if TK_METER
	tkc_meteroff(cs);
#endif
	ci->tkci_if = NULL;
	ci->tkci_obj = NULL;
	__tk_destroy_lock(&ci->tkci_lock);
}

/*
 * tkc_destroy_local - stop managing a local object.
 * It is assumed that NO tokens are held and that NO requests for any
 * more tokens can come in.
 * Cached tokens are returned to server.
 */
void
tkc_destroy_local(tkc_state_t *cs)		/* object to be destroyed */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	int class;
	tk_set_t toret = TK_NULLSET;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */

	TKC_METER_VAR_INIT(tm, cs);

	ASSERT(ci->tkci_if == &tkc_local_iface);
	TK_TRACE(TKC_DESTROY, __return_address, TK_GETCH(ci->tkci_obj),
		TK_NULLSET, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	for (class = 0; class < ci->tkci_ntokens; class++) {
		tstate = &ci->tkci_state[class];
		ASSERT(tstate->cl_obtain == 0);
		if (tstate->cl_state == TK_CL_CACHED) {
			ASSERT(downext[tstate->cl_extlev]);
			TK_COMBINE_SET(toret, TK_MAKE(class,
						downext[tstate->cl_extlev]));
		} else if (tstate->cl_state != TK_CL_IDLE) {
			__tk_fatal("tkc_destroy_local - not idle/cached state cs 0x%x class %d state %s\n",
				cs, class, __tkci_prstate(*tstate));
			/* NOTREACHED */
		}
	}
	if (toret)
		tks_return(ci->tkci_obj, TK_GETCH(NULL),
				toret, TK_NULLSET, TK_NULLSET,
				TK_DISP_CLIENT_ALL);
#if TK_METER
	tkc_meteroff(cs);
#endif
	ci->tkci_if = NULL;
	ci->tkci_obj = NULL;
	__tk_destroy_lock(&ci->tkci_lock);
}

/*
 * tkc_acquire1 - a fast path acquire
 * Returns 0 if got token, -1 if token refused, errno on out of band error.
 */
int
tkc_acquire1(tkc_state_t *cs, tk_singleton_t tsing)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	auto tk_set_t gotten;
	TK_METER_DECL /* decls 'tm' */
	int class = (tsing >> TK_LWIDTH) & 0xF;
	int level = tsing & TK_SET_MASK;
	TK_LOCKDECL;

	TK_ASSERT(tsing & 0x80000000);/* !passed in a tk_set_t */
	TKC_METER_VAR_INIT(tm, cs);
	TKC_INC_METER(tm, class, acquire);
	TK_ASSERT((level & level - 1) == 0);

	TK_LOCK(ci->tkci_lock);

	tstate = &ci->tkci_state[class];
	CHECK(tstate);
	if ((level & tstate->cl_extlev) || (tstate->cl_extlev == TK_WRITE)) {
		if ((tstate->cl_state == TK_CL_CACHED) ||
		    (tstate->cl_state == TK_CL_OWNED)) {
			/*
			 * asking for the level we own OR
			 * we have the write token - its always
			 * ok to get a token if we have the write token
			 * level as no-one else can have a conflicting token
			 * XXX do we need to turn off obtain bit for the
			 * OWNED case as in tkc_obtaining??
			 */
			tstate->cl_hold++;
			tstate->cl_state = TK_CL_OWNED;
			tstate->cl_extlev |= level;
			TKC_INC_METER(tm, class, acq_cached);
			TK_TRACE(TKC_FACQUIRE, __return_address,
				TK_GETCH(ci->tkci_obj),
				TK_MAKE(class, level), TK_NULLSET,
				TK_NULLSET, ci, TK_METER_VAR);
			TK_UNLOCK(ci->tkci_lock);
			return 0;
		}
	}
	TK_UNLOCK(ci->tkci_lock);
	tkc_acquire(cs, TK_MAKE(class, level), &gotten);
	return gotten == TK_NULLSET ? 1 : 0;
}

void
tkc_release1(tkc_state_t *cs, tk_singleton_t tsing)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	int class;
#if TK_DEBUG || TK_TRC
	int level;
#endif
	TK_LOCKDECL;
#if TK_DEBUG || TK_TRC
	level = tsing & TK_SET_MASK;
#endif

	TK_ASSERT(tsing & 0x80000000);/* !passed in a tk_set_t */
	TKC_METER_VAR_INIT(tm, cs);
	TK_ASSERT((level & level - 1) == 0);

	TK_LOCK(ci->tkci_lock);

	class = (tsing >> TK_LWIDTH) & 0xF;
	tstate = &ci->tkci_state[class];
	CHECK(tstate);
	if (tstate->cl_state == TK_CL_OWNED && !tstate->cl_obtain) {
		TK_ASSERT((tsing & TK_SET_MASK) & tstate->cl_extlev);
		if (--tstate->cl_hold == 0) {
			/* turn off appropriate extlev bits.  */
			tstate->cl_extlev = downext[tstate->cl_extlev];
			tstate->cl_state = TK_CL_CACHED;
		}
		TK_TRACE(TKC_FRELEASE, __return_address, TK_GETCH(ci->tkci_obj),
			TK_MAKE(class, level), TK_NULLSET,
			TK_NULLSET, ci, TK_METER_VAR);
		TK_UNLOCK(ci->tkci_lock);
	} else {
		TK_UNLOCK(ci->tkci_lock);
		tkc_release(cs, TK_MAKE(class, tsing & TK_SET_MASK));
	}
}

/*
 * tkc_obtaining - get a token(s)
 * The tokens are obtained from the server if they are not present
 * on the client cell and held. While held, they cannot be recalled.
 * Note that ltoret is only updated when ltoobtain is so we don't
 *	have to check it.
 */
void
tkc_obtaining(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens requested */
	tk_set_t *tstoobtain,	/* tokens to obtain */
	tk_set_t *tstoret,	/* tokens to return */
	tk_disp_t *dofret,	/* disposition of tokens to return */
	tk_set_t *tsrefused,	/* tokens that were refused */
	tk_set_t *tslater)	/* get later please! */
{
	__tkc_obtaining(cs, tset, tstoobtain, tstoret, dofret,
		tsrefused, tslater, 1, (void *)__return_address);

}

/* ARGSUSED */
static void
__tkc_obtaining(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens requested */
	tk_set_t *tstoobtain,	/* tokens to obtain */
	tk_set_t *tstoret,	/* tokens to return */
	tk_disp_t *dofret,	/* disposition of tokens to return */
	tk_set_t *tsrefused,	/* tokens that were refused */
	tk_set_t *tslater,	/* get later please! */
	int dotrace,
	void *ra)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	tk_set_t lrefused, ltoobtain, ltoret;
	tk_disp_t ldofret;
	int level, class;
	int setobtain;
	TK_LOCKDECL;

	lrefused = TK_NULLSET;
	ltoobtain = TK_NULLSET;
	ltoret = TK_NULLSET;
	ldofret = TK_NULLSET;
	TKC_METER_VAR_INIT(tm, cs);

	TK_ASSERT((tset & 0x80000000) == 0);/* !passed in a tk_singleton_t */
	TK_LOCK(ci->tkci_lock);
	if (dotrace)
		TK_TRACE(TKC_SOBTAINING, ra, TK_GETCH(ci->tkci_obj), tset,
				TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	lrefused = TK_NULLSET;
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);
		TKC_INC_METER(tm, class, acquire);
		setobtain = 0;

tryagain:
		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		switch (tstate->cl_state) {
		case TK_CL_IDLE:
			/*
			 * we can get here with the setobtain bit on
			 * if a return was refused - which will place us
			 * in the cached state. Then, some other thread could
			 * go ahead and return the token, finally the thread
			 * sleeping below in RETURNING wakes up and lands here.
			 */
			setobtain = 0;
			tstate->cl_hold++;
			tstate->cl_extlev = level;
			tstate->cl_state = TK_CL_OBTAINING;

			/* update list of tokens to obtain/return */
			TK_COMBINE_SET(ltoobtain, TK_MAKE(class, level));
			break;
		case TK_CL_CACHED:
			/*
			 * we already have the token - if at a compatible
			 * level, bump hold count and return
			 *
			 * We can get here with the setobtain bit on in
			 * rare instances - if we got into a state
			 * where we held the WR token and wanted the
			 * RD and SWR tokens, both RD & SWR would
			 * be set in the obtain mask. We would then turn
			 * off the SWR bit and let that get obtained.
			 * Then we would turn off the RD bit and wake up the
			 * thread that set the RD bit. Lets assume that it
			 * doesn't run for a while. Meanwhile the thread that
			 * got the SWR token could release it - that would
			 * place the token into the CACHED state.
			 */
			if (ltoobtain != TK_NULLSET)
				goto done;
			setobtain = 0;
			if (level == tstate->cl_extlev ||
			    (tstate->cl_extlev == TK_WRITE)) {
				/*
				 * asking for the level we own OR
				 * we have the write token - its always
				 * ok to get a token if we have the write token
				 * level as no-one else can have a conflicting
				 * token
				 */
				tstate->cl_hold++;
				tstate->cl_state = TK_CL_OWNED;
				tstate->cl_extlev |= level;
				TKC_INC_METER(tm, class, acq_cached);
			} else {
				int oextlev;
				/* wants a conflicting token */
				tstate->cl_hold++;
				tstate->cl_state = TK_CL_OBTAINING;
				oextlev = tstate->cl_extlev;
				tstate->cl_extlev = level;
				TKC_INC_METER(tm, class, acq_conflict);

				/*
				 * update list of tokens to obtain/return
				 */
				TK_COMBINE_SET(ltoret, TK_MAKE(class, downext[oextlev]));
				TK_COMBINE_SET(ldofret,
					TK_MAKE(class, TK_CLIENT_INITIATED));
				TK_COMBINE_SET(ltoobtain, TK_MAKE(class, level));
			}
			break;
		case TK_CL_OWNED:
		case TK_CL_BORROWED:
			if (ltoobtain != TK_NULLSET)
				goto done;
			/*
			 * we already have the token held - if at a compatible
			 * level, bump hold count and return
			 */
			if ((level & tstate->cl_extlev) ||
			    (tstate->cl_extlev == TK_WRITE)) {
				/*
				 * asking for the level we own OR
				 * got the write token - its always
				 * ok to get a token if we have the write token
				 * level as no-one else can have a conflicting
				 * token
				 */
				tstate->cl_hold++;
				tstate->cl_extlev |= level;
				tstate->cl_obtain &= ~level;
				setobtain = 0;
				TKC_INC_METER(tm, class, acq_cached);
			} else {
				/*
				 * wants a conflicting token 
				 * This is equivalent to the client
				 * calling tkc_recall.
				 */
				TKC_INC_METER(tm, class, acq_conflict);
				if (tstate->cl_state == TK_CL_BORROWED)
					tstate->cl_state = TK_CL_BREVOKING;
				else
					tstate->cl_state = TK_CL_REVOKING;
				if ((tstate->cl_obtain & level) == 0) {
					setobtain = 1;
					tstate->cl_obtain |= level;
				}
				CHECK(tstate);

				tkci_scwait(ci, class, TK_LOCKVARADDR);
				/* something has changed - try again */
				TK_LOCK(ci->tkci_lock);
				goto tryagain;
			}
			break;
		case TK_CL_BORROWING:
		case TK_CL_OBTAINING:
		case TK_CL_SOBTAINING:
		case TK_CL_COBTAINING:
			/*
			 * Should we wait here or go ahead and accumulate
			 * everything to do? if and when we support
			 * class acquisition ordering it will be important
			 * to wait here. It also seems to minimize
			 * deadlock situations where we have a token
			 * (because we have already held some) and now
			 * are waiting for one
			 */
			if (ltoobtain != TK_NULLSET)
				goto done;

			if ((level & tstate->cl_extlev) ||
			    (tstate->cl_extlev == TK_WRITE)) {
				/*
				 * asking for the level we own OR
				 * got the write token - its always
				 * ok to get a token if we have the write token
				 * level as no-one else can have a conflicting
				 * token
				 */
				tstate->cl_hold++;
				tstate->cl_extlev |= level;
				/*
				 * turn off obtain bit if it is on - this
				 * could be READ - we might have had
				 * both WRITE and READ in the obtain mask
				 * and went after the WRITE token, leaving
				 * the READ set in obtain. Since WRITE
				 * implies READ, we certainly don't need to
				 * 'get' it
				 */
				tstate->cl_obtain &= ~level;
				setobtain = 0;

				/* wait for token to arrive */
				tkci_obtainwait(ci, class, TK_LOCKVARADDR);
				TK_LOCK(ci->tkci_lock);

				/*
				 * unless we got into the REJECTED state
				 * we should have the token so we can progress
				 * to the next one
				 */
				if (tstate->cl_state != TK_CL_REJECTED) {
					TK_ASSERT(
					  tstate->cl_state == TK_CL_OWNED ||
					  tstate->cl_state == TK_CL_BORROWED ||
					  tstate->cl_state == TK_CL_WILLRETURN ||
					  tstate->cl_state == TK_CL_SREVOKING ||
					  tstate->cl_state == TK_CL_BREVOKING ||
					  tstate->cl_state == TK_CL_REVOKING);
					break;
				}

				/*
				 * The obtain was rejected
				 * undo our hold
				 * Note that if the hold count goes to zero
				 * we always need to wake up folks
				 * To clear a rejection all waiters must
				 * be finished and the hold count get to 0
				 */
				TK_COMBINE_SET(lrefused, TK_MAKE(class, level));
				if (--tstate->cl_hold > 0)
					break;

				if (tstate->cl_obtain) {
					int toobtain;
					/* turn off appropriate extlev bits.  */
					/* XXXcheck out */
					tstate->cl_extlev = downext[tstate->cl_extlev];
					toobtain = obtaintab[tstate->cl_obtain];
					TK_ASSERT(toobtain);
					tstate->cl_obtain &= ~toobtain;
					tstate->cl_state = TK_CL_WILLOBTAIN;
				} else {
					tstate->cl_extlev = 0;
					tstate->cl_state = TK_CL_IDLE;
					TK_ASSERT(tkci_nobtainwaiting(ci, class) == 0);
				}
				if (tstate->cl_scwait)
					tkci_scwake(ci, class);
			} else {
				/* wants a conflicting token! */
				if ((tstate->cl_obtain & level) == 0) {
					setobtain = 1;
					tstate->cl_obtain |= level;
				}
				TKC_INC_METER(tm, class, acq_conflict);
				tkci_scwait(ci, class, TK_LOCKVARADDR);
				/* something has changed - try again */
				TK_LOCK(ci->tkci_lock);
				goto tryagain;
			}
			break;
		case TK_CL_WILLOBTAIN:
		case TK_CL_SWILLOBTAIN:
			{
			/*
			 * This state permits us to proceed to get
			 * a level - note that the obtain mask may
			 * have been set to more than one level - the thread
			 * that sets the WILLOBTAIN state will turn off only
			 * one bit 
			 * Only the thread that turned on the obtain bit can
			 * use this state. Consider:
			 * An initial state:
			 *	we hold SWR
			 *	we want WRITE (this is set in the obtain mask
			 *		and the state set to REVOKING).
			 * We then release SWR which starts a revoke sequence
			 *	and changes the state to RETURNING.
			 * The return completes (having sent a one-way
			 *	message to the server), and the state changes to
			 *	WILLOBTAIN (and the WR bit is turned off in
			 *	obtain mask).
			 * Another thread comes in wanting SWR - if this thread
			 * gets the WILLOBTAIN state (rather than the thread
			 * that set the WR bit), it will send an obtain message
			 * to the server. This obtain message can overtake the
			 * return message at the server which will confuse the
			 * server into returning an 'already' indication
			 * followed by the server removing the fact that
			 * we own the token!
			 *
			 * We are only permitted in this state if we set
			 * and obtain bit AND that bit has been turned off.
			 */
			int oextlev;
			if (ltoobtain != TK_NULLSET)
				goto done;
			if (setobtain == 0 ||
			    (tstate->cl_obtain & level)) {
				tkci_scwait(ci, class, TK_LOCKVARADDR);
				/* something has changed - try again */
				TK_LOCK(ci->tkci_lock);
				goto tryagain;
			} else {
				tstate->cl_hold++;
				oextlev = tstate->cl_extlev;
				tstate->cl_extlev = level;
				setobtain = 0;
				/*
				 * If we're in the SWILLOBTAIN state then
				 * the server has sent a recall for the
				 * token we currently have (and will be
				 * returning). We must of course return
				 * it with a TK_SERVER_MANDATORY message -
				 * something a standard returned token
				 * can't do. So we do a (*tkc_return)
				 * callout
				 */
				if (tstate->cl_state == TK_CL_SWILLOBTAIN) {
					TK_ASSERT(oextlev != level);
					TK_COMBINE_SET(ltoret,
						TK_MAKE(class, oextlev));
					TK_COMBINE_SET(ldofret,
						TK_MAKE(class, TK_SERVER_MANDATORY));
				} else if (oextlev != level) {
					/*
					 * we have to return a token if it
					 * conflicts with the one we want
					 */
					TK_COMBINE_SET(ltoret,
						TK_MAKE(class, oextlev));
					TK_COMBINE_SET(ldofret,
						TK_MAKE(class, TK_CLIENT_INITIATED));
				}
				/*
				 * This wakeup only for folks just above us
				 * who went to sleep in WILLOBTAIN
				 */
				NEWSTATE(ci, class, tstate, TK_CL_OBTAINING);

				TK_COMBINE_SET(ltoobtain, TK_MAKE(class, level));
			}
			break;
			}
		case TK_CL_REJECTED:
		case TK_CL_REVOKING:
		case TK_CL_BREVOKING:
		case TK_CL_SREVOKING:
		case TK_CL_CREVOKING:
		case TK_CL_WILLRETURN:
		case TK_CL_RETURNING:
			/*
			 * In all these state we can't grant the token -
			 * we must wait until the hold count goes to zero
			 */
			if (ltoobtain != TK_NULLSET)
				goto done;

			TKC_INC_METER(tm, class, acq_outbound);
			if ((tstate->cl_obtain & level) == 0) {
				setobtain = 1;
				tstate->cl_obtain |= level;
			}

			tkci_scwait(ci, class, TK_LOCKVARADDR);
			/* something has changed - try again */
			TK_LOCK(ci->tkci_lock);
			goto tryagain;
			
		default:
			tkci_badstate("tkc_obtaining", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
		TK_ASSERT(setobtain == 0);
	}
done:
	if (dotrace) {
		TK_TRACE(TKC_EOBTAINING, 0, TK_GETCH(ci->tkci_obj), ltoobtain,
				ts << (TK_LWIDTH * class), lrefused, ci,
				TK_METER_VAR);
		TK_TRACE(TKC_EOBTAINING2, 0, TK_GETCH(ci->tkci_obj), ltoret,
				TK_NULLSET, ldofret,
				ci, TK_METER_VAR);
	}
	TK_UNLOCK(ci->tkci_lock);
	
	*tstoobtain = ltoobtain;
	*tstoret = ltoret;
	*tsrefused = lrefused;
	*tslater = ts << (TK_LWIDTH * class);
	*dofret = ldofret;
}

/*
 * called to initiate obtain/return requests
 */
void
tkc_obtained(
	tkc_state_t *cs,
	tk_set_t tsobtained,
	tk_set_t tsrefused,
	tk_disp_t dofref)
{
	__tkc_obtained(cs, tsobtained, tsrefused, dofref,
				1, (void *)__return_address);

}

/* ARGSUSED */
static void
__tkc_obtained(
	tkc_state_t *cs,
	tk_set_t tsobtained,
	tk_set_t tsrefused,
	tk_disp_t dofref,
	int dotrace,
	void *ra)
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	tk_set_t ts;
	int class, level, disp;
	TK_METER_DECL /* decls 'tm' */
	TK_LOCKDECL;

	TKC_METER_VAR_INIT(tm, cs);

	TK_ASSERT((tsrefused & tsobtained) == 0);

	TK_LOCK(ci->tkci_lock);
	if (dotrace)
		TK_TRACE(TKC_SOBTAINED, ra, TK_GETCH(ci->tkci_obj), \
				tsobtained, tsrefused, TK_NULLSET,
				ci, TK_METER_VAR);

	/*
	 * Deal with successfully obtained tokens
	 */
	for (ts = tsobtained, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		switch (tstate->cl_state) {
		case TK_CL_SOBTAINING:
			/*
			 * we got the token but it must be returned as soon
			 * as possible (server wants it)
			 */
			tstate->cl_state = TK_CL_SREVOKING;
			break;
		case TK_CL_COBTAINING:
			/*
			 * we got the token but it must be returned as soon
			 * as possible (client requested it)
			 */
			tstate->cl_state = TK_CL_REVOKING;
			break;
		case TK_CL_BOBTAINING:
			/*
			 * we got the token but it must be returned as soon
			 * as possible (server recalled it)
			 */
			tstate->cl_state = TK_CL_BREVOKING;
			break;
		case TK_CL_BORROWING:
			tstate->cl_obtain &= ~level;
			tstate->cl_state = TK_CL_BORROWED;
			break;
		case TK_CL_OBTAINING:
			tstate->cl_obtain &= ~level;
			tstate->cl_state = TK_CL_OWNED;
			break;
		default:
			tkci_badstate("tkc_obtained", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
		/* XXX would be nice to only call wake if we knew 
		 * someone was waiting
		 */
		tkci_obtainwake(ci, class);
	}

	/*
	 * Deal with refused tokens
	 * In the case of (*tkc_obtain) refusing to return a token,
	 * the refuse set can have multiple levels on.
	 */
	for (ts = tsrefused, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		/*
		 * If we have a disposition for this token, then
		 * we must have been trying to return it.
		 */
		disp = TK_GET_LEVEL(dofref, class);
		if (disp) {
			TK_ASSERT((disp & disp - 1) == 0);
			/*
			 * since we were returning this, we shouldn't 'have'
			 * it
			 */
			TK_ASSERT((level & downext[tstate->cl_extlev]) == 0);
			switch (tstate->cl_state) {
			case TK_CL_COBTAINING:
			case TK_CL_OBTAINING:
				if (--tstate->cl_hold == 0) {
				} else {
					/* put into restore state based
					 * on disposition
					 */
					tstate->cl_extlev = level;
					if (disp == TK_CLIENT_INITIATED) {
						tstate->cl_state = TK_CL_CRESTORE;
					} else if (disp == TK_SERVER_MANDATORY) {
						tstate->cl_state = TK_CL_SRESTORE;
					} else {
						TK_ASSERT(disp == TK_SERVER_CONDITIONAL);
						tstate->cl_state = TK_CL_BRESTORE;
					}
					tkci_obtainwake(ci, class);
				}
			default:
				tkci_badstate("tkc_obtained3", ci, class);
				/* NOTREACHED */
			}
			CHECK(tstate);
			continue;
		}

		/*
		 * standard! refuse of obtain
		 * Seems like we might be able to get into the COBTAINING
		 * or BOBTAINING states since it's legal for the client
		 * to call tkc_recall() on a token being obtained.
		 * We shouldn't be able to get SOBTAINING - we don't
		 * have the token so how can the server ask for it back?
		 * XXX actually for now we can't get into BORROWING nor
		 * BOBTAINING.
		 */
		TK_ASSERT((level & level - 1) == 0);
		switch (tstate->cl_state) {
		case TK_CL_COBTAINING:
			/*
			 * client wanted to revoke token but we refused
			 * to get it. Sounds like a standoff..
			 * Ignore revoke.
			 */
			 /* FALLTHROUGH */
		case TK_CL_BORROWING:
		case TK_CL_OBTAINING:
			if (--tstate->cl_hold == 0) {
				if (tstate->cl_obtain) {
					int toobtain;
					/* turn off appropriate extlev bits.  */
					/* XXXcheckout */
					tstate->cl_extlev = downext[tstate->cl_extlev];
					toobtain = obtaintab[tstate->cl_obtain];
					TK_ASSERT(toobtain);
					tstate->cl_obtain &= ~toobtain;
					tstate->cl_state = TK_CL_WILLOBTAIN;
					if (tstate->cl_scwait)
						tkci_scwake(ci, class);
				} else {
					tstate->cl_extlev = 0;
					tstate->cl_state = TK_CL_IDLE;
					TK_ASSERT(tkci_nobtainwaiting(ci, class) == 0);
				}
			} else {
				/*
				 * XX we clearly need to handle either
				 * extlev or obtain mask here - in the
				 * OBTAINING or RETURNING state we are
				 * permitted to have the obtain mask
				 * overlap the extlev, but not in the
				 * REJECTED state
				 */
				tstate->cl_extlev = 0;
				tstate->cl_state = TK_CL_REJECTED;
				tkci_obtainwake(ci, class);
			}
			break;
		default:
			tkci_badstate("tkc_obtained2", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
	}
	if (dotrace)
		TK_TRACE(TKC_EOBTAINED, 0, TK_GETCH(ci->tkci_obj), TK_NULLSET,
				TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);
}

/*
 * tkc_acquire - conditionally acquire.
 */
void
tkc_acquire(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens requested */
	tk_set_t *got)		/* tokens gotten */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t tstoret, tswanted, tstoobtain, obtained;
	tk_set_t tsrefused, tslater, gotten;
	tk_disp_t dofret;

	TKC_METER_VAR_INIT(tm, cs);
	TK_TRACE(TKC_SCACQUIRE, __return_address, TK_GETCH(ci->tkci_obj),
				tset, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	tswanted = tset;
	gotten = TK_NULLSET;
	while (tswanted) {
		__tkc_obtaining(cs, tswanted, &tstoobtain,
			&tstoret, &dofret, &tsrefused, &tslater, 1,
			(void *)__return_address);
		gotten |= tswanted & ~(tstoobtain | tsrefused | tslater);

		if ((tstoobtain | tstoret) != 0) {
			/*
			 * Refusal to return is really just for
			 * error handling - if the server goes down
			 * then (*tkc_obtain)() may well refuse all
			 * returns and obtains.
			 */
			ci->tkci_if->tkc_obtain(ci->tkci_obj, tstoobtain,
				tstoret, dofret, &tsrefused);
			TK_ASSERT((tstoobtain & tsrefused) == tsrefused);
#if TK_DEBUG
			/*
			 * if any toreturn tokens were refused, then the obtain
			 * token that prompted the return must also have
			 * been refused
			 */
			if (tsrefused & tstoret) {
				tk_set_t ovlp, tsob;
				int class, oblevel;
				for (   ovlp = (tsrefused & tstoret),
					tsob = tstoobtain, class = 0;
					ovlp;
					ovlp = ovlp >> TK_LWIDTH,
					tsob = tsob >> TK_LWIDTH,
					class++) {
					if ((ovlp & TK_SET_MASK) == 0)
						continue;
					oblevel = tsob & TK_SET_MASK;
					/* must have wanted something */
					TK_ASSERT(oblevel);
					/* what we wanted must also be refused */
					TK_ASSERT(oblevel & TK_GET_LEVEL(tsrefused, class));
				}
			}
#endif
			obtained = tstoobtain & ~tsrefused;
			/* 
			 * it's fine to pass more disposition info
			 * than is needed.
			 */
			__tkc_obtained(cs, obtained, tsrefused,
				dofret, 1, (void *)__return_address);
			gotten |= obtained;
		}

		tswanted = tslater;
	}

	TK_TRACE(TKC_ECACQUIRE, 0, TK_GETCH(ci->tkci_obj), gotten,
				TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	*got = gotten;
}

/*
 * tkc_release - release a held token
 */
void
tkc_release(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset)		/* tokens to release */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	tk_set_t tstoret = TK_NULLSET;
	tk_disp_t dofret = TK_NULLSET;
	/* REFERENCED (level) */
	int level;
	int class;
	TK_LOCKDECL;

	TK_ASSERT((tset & 0x80000000) == 0);/* !passed in a tk_singleton_t */
	TKC_METER_VAR_INIT(tm, cs);
	TK_LOCK(ci->tkci_lock);
	TK_TRACE(TKC_SRELEASE, __return_address, TK_GETCH(ci->tkci_obj),
				tset, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];
		TK_ASSERT(tstate->cl_hold > 0);

		CHECK(tstate);
		switch (tstate->cl_state) {
		case TK_CL_BORROWED:
		case TK_CL_OWNED:
			TK_ASSERT(level & tstate->cl_extlev);
			if (--tstate->cl_hold == 0) {
				/* turn off appropriate extlev bits.  */
				tstate->cl_extlev = downext[tstate->cl_extlev];

				if (tstate->cl_state == TK_CL_BORROWED) {
					tstate->cl_state = TK_CL_RETURNING;
					TK_COMBINE_SET(tstoret,
						TK_MAKE(class, tstate->cl_extlev));
					TK_COMBINE_SET(dofret,
						TK_MAKE(class, TK_SERVER_CONDITIONAL));
					if (tstate->cl_scwait)
						tkci_scwake(ci, class);
				} else if (tstate->cl_obtain) {
					int toobtain;
					toobtain = obtaintab[tstate->cl_obtain];
					TK_ASSERT(toobtain);
					tstate->cl_obtain &= ~toobtain;
					tstate->cl_state = TK_CL_WILLOBTAIN;
					if (tstate->cl_scwait)
						tkci_scwake(ci, class);
				} else {
					tstate->cl_state = TK_CL_CACHED;
				}
			}
			break;
		case TK_CL_REVOKING:
		case TK_CL_SREVOKING:
		case TK_CL_BREVOKING:
			/*
			 * token wants to be revoked - if hold count goes to
			 * zero, call (*tkc_return)() callout.
			 */
			TK_ASSERT(level & tstate->cl_extlev);
			if (--tstate->cl_hold == 0) {
				/* turn off appropriate extlev bits. */
				tstate->cl_extlev = downext[tstate->cl_extlev];
				/*
				 * XXX OSF seems to permit transition to
				 * the WILLOBTAIN state if the obtain
				 * mask is set. This seems to cause some
				 * problems - extlev is still set so a thread
				 * trying to get a conflicting token will
				 * attempt to return it (in tkc_acquire)
				 * as well as our sending it back
				 * (via tkc_return)
				 */
				/*
				 * the token level to revoke is the extlev
				 * not what we're returning - we could
				 * be returning a READ token but have
				 * the WRITE or SWRITE token cached
				 */
				TK_COMBINE_SET(tstoret,
					TK_MAKE(class, tstate->cl_extlev));
				if (tstate->cl_state == TK_CL_BREVOKING) {
					TK_COMBINE_SET(dofret,
						TK_MAKE(class, TK_SERVER_CONDITIONAL));
				} else if (tstate->cl_state == TK_CL_SREVOKING) {
					TK_COMBINE_SET(dofret,
						TK_MAKE(class, TK_SERVER_MANDATORY));
				} else {
					TK_COMBINE_SET(dofret,
						TK_MAKE(class, TK_CLIENT_INITIATED));
				}
				NEWSTATE(ci, class, tstate, TK_CL_RETURNING);
			}
			break;
		case TK_CL_WILLRETURN:
			/*
			 * token wants to be returned - if hold count goes to
			 * zero, wake returner
			 * We go to a CREVOKING state rather than, like OSF
			 * to the RETURNING state. We want RETURNING to mean
			 * that the return message has gone out. Instead,
			 * depending on which request kicks us out of
			 * WILLRETURN we go into one of the 3 REVOKING states
			 * This lets the thread waiting in tkc_returning
			 * decide whether it needs to send a message.
			 */
			TK_ASSERT(level & tstate->cl_extlev);
			if (--tstate->cl_hold == 0) {
				/* turn off appropriate extlev bits. */
				tstate->cl_extlev = downext[tstate->cl_extlev];
				NEWSTATE(ci, class, tstate, TK_CL_CREVOKING);
			}
			break;
		default:
			tkci_badstate("tkc_release", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
	}
	TK_TRACE(TKC_ERELEASE, 0, TK_GETCH(ci->tkci_obj), tstoret, TK_NULLSET,
						dofret, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);
	if (tstoret) {
		TK_TRACE(TKC_RETURN_CALL, __return_address,
				TK_GETCH(ci->tkci_obj),
				tstoret, TK_NULLSET, dofret, ci, TK_METER_VAR);
		ci->tkci_if->tkc_return(ci, ci->tkci_obj, tstoret,
				TK_NULLSET, dofret);
	}
	return;
}

/*
 * tkc_hold - hold a token. No attempt is made to acquire the token if
 * it is not on the client. The set of successfully held tokens is returned.
 */
tk_set_t
tkc_hold(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset)		/* tokens requested */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts, gts = TK_NULLSET;
	int level, class;
	TK_LOCKDECL;

	TK_ASSERT((tset & 0x80000000) == 0);/* !passed in a tk_singleton_t */
	TKC_METER_VAR_INIT(tm, cs);
	TK_LOCK(ci->tkci_lock);
	TK_TRACE(TKC_SHOLD, __return_address, TK_GETCH(ci->tkci_obj), tset,
				TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
tryagain:
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];

		CHECK(tstate);
		TKC_INC_METER(tm, class, acquire);
		switch (tstate->cl_state) {
		case TK_CL_CACHED:
			/*
			 * we already have the token - if at a compatible
			 * level, bump hold count and return
			 */
			if (level == tstate->cl_extlev ||
			    (tstate->cl_extlev == TK_WRITE)) {
				/*
				 * asking for the level we own OR
				 * got the write token - its always
				 * ok to get a token if we have the write token
				 * level as no-one else can have a conflicting
				 * token
				 */
				tstate->cl_hold++;
				tstate->cl_state = TK_CL_OWNED;
				tstate->cl_extlev |= level;
				TKC_INC_METER(tm, class, acq_cached);
				TK_COMBINE_SET(gts, TK_MAKE(class, level));
			}
			break;
		case TK_CL_OWNED:
		case TK_CL_BORROWED:
			/*
			 * we already have the token held - if at a compatible
			 * level, bump hold count and return
			 */
			if ((level & tstate->cl_extlev) ||
			    (tstate->cl_extlev == TK_WRITE)) {
				/*
				 * asking for the level we own OR
				 * got the write token - its always
				 * ok to get a token if we have the write token
				 * level as no-one else can have a conflicting
				 * token
				 */
				tstate->cl_hold++;
				tstate->cl_extlev |= level;
				TK_COMBINE_SET(gts, TK_MAKE(class, level));
				TKC_INC_METER(tm, class, acq_cached);
			}
			break;
		case TK_CL_BORROWING:
		case TK_CL_OBTAINING:
		case TK_CL_COBTAINING:
		case TK_CL_BOBTAINING:
			if ((level & tstate->cl_extlev) ||
			    (tstate->cl_extlev == TK_WRITE)) {
				tkci_obtainwait(ci, class, TK_LOCKVARADDR);
				TK_LOCK(ci->tkci_lock);
				goto tryagain;
			}
			break;
		default:
			/*
			 * these are all states for which we won't grant a 
			 * token
			 */
#ifdef NEVER
			if (tstate->cl_state != TK_CL_IDLE)
				printf("tkc_hold - token 0x%x not granted because in state %s\n",
					cs, __tkci_prstate(*tstate));
#endif
			break;
		}
		CHECK(tstate);
	}
	TK_TRACE(TKC_EHOLD, 0, TK_GETCH(ci->tkci_obj), gts, TK_NULLSET,
		TK_NULLSET, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);
	return gts;
}

/* ARGSUSED */
static void
tkci_mreturn(
	tkci_cstate_t *ci,	/* object containing tokens */
	tk_meter_t *tm,
	int class,
	int level,
	tk_set_t *tstoret,		/* tokens to return */
	tk_set_t *teh,
	tk_disp_t *dofret)
{
	tkci_clstate_t *tstate;

	TKC_INC_METER(tm, class, recall);
	tstate = &ci->tkci_state[class];

	CHECK(tstate);
	switch (tstate->cl_state) {
	case TK_CL_IDLE:
		/*
		 * we can get here if the client spontaneously sent
		 * back a token while the server was revoking it
		 * but before we got the revoke message
		 */
		TK_COMBINE_SET(*teh, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofret, TK_MAKE(class, TK_SERVER_MANDATORY));
		break;
	case TK_CL_CACHED:
		TK_ASSERT((level & downext[tstate->cl_extlev]));
		tstate->cl_state = TK_CL_RETURNING;
		TK_COMBINE_SET(*tstoret, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofret, TK_MAKE(class, TK_SERVER_MANDATORY));
		break;
	case TK_CL_OWNED:
		TK_ASSERT((level & downext[tstate->cl_extlev]));
		tstate->cl_state = TK_CL_SREVOKING;
		break;
	case TK_CL_OBTAINING:
		/*
		 * If we are in the obtaining state it is because the
		 * client has voluntarily returned the token after
		 * the revoke started, and then tried to reacquire
		 * it. At this point the obtain will be stuck at the
		 * server waiting for the response to this revoke
		 * so we say that we don't know where the token is
		 */
		TK_COMBINE_SET(*teh, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofret, TK_MAKE(class, TK_SERVER_MANDATORY));
		break;
	case TK_CL_COBTAINING:
		/*
		 * client wants to return it and now so does
		 * the server - server wins
		 */
		TK_ASSERT(level & downext[tstate->cl_extlev]);
		tstate->cl_state = TK_CL_SOBTAINING;
		break;
	case TK_CL_RETURNING:
		TK_COMBINE_SET(*teh, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofret, TK_MAKE(class, TK_SERVER_MANDATORY));
		break;
	case TK_CL_REVOKING:
		/*
		 * client wants to return token - so do we - no
		 * need for 2 messages, change to server style
		 * No need to use NEWSTATE - don't need to wake anyone
		 */
		tstate->cl_state = TK_CL_SREVOKING;
		break;
	case TK_CL_WILLRETURN:
		TK_ASSERT(level & downext[tstate->cl_extlev]);
		NEWSTATE(ci, class, tstate, TK_CL_SREVOKING);
		break;
	case TK_CL_WILLOBTAIN:
		/*
		 * In this state, the thread that set this
		 * state will ALWAYS be the one to return the current token,
		 * There are a few possiblities:
		 * 1) wait for status chg - but tkc_recall
		 *	isn't supposed to sleep
		 * 2) return an 'eh' indication - this could
		 *	cause storms if the thread that owns the
		 *	WILLOBTAIN state doesn't run for a while
		 * 3) somehow force the WILLOBTAIN thread to
		 *	place this token into the SREVOKING state.
		 * 3) of course is the right answer.
		 */
		if (level & downext[tstate->cl_extlev])
			tstate->cl_state = TK_CL_SWILLOBTAIN;
		else {
			TK_COMBINE_SET(*teh, TK_MAKE(class, level));
			TK_COMBINE_SET(*dofret, TK_MAKE(class, TK_SERVER_MANDATORY));
		}
		break;
	case TK_CL_BREVOKING:
	case TK_CL_BOBTAINING:
	case TK_CL_BORROWED:
	case TK_CL_BORROWING:
		/* implies a conditional recall - but server can't both
		 * conditionally and mandatorily ask use for token!
		 */
	default:
		tkci_badstate("tkci_mreturn", ci, class);
		/* NOTREACHED */
	}
	CHECK(tstate);
}

/*
 * handle recalls initiated by client - in general these are just hints
 */
/* ARGSUSED */
static void
tkci_recall_client(
	tkci_cstate_t *ci,	/* object containing tokens */
	tk_meter_t *tm,
	int class,
	int level,
	tk_set_t *tset,		/* tokens to return */
	tk_disp_t *dofset)
{
	tkci_clstate_t *tstate;

	tstate = &ci->tkci_state[class];
	CHECK(tstate);

	/* if we don't have it, ignore revoke 'request' */
	if (!(level & downext[tstate->cl_extlev]))
		return;

	switch (tstate->cl_state) {
	case TK_CL_CACHED:
		TKC_INC_METER(tm, class, recall);
		tstate->cl_state = TK_CL_RETURNING;
		TK_COMBINE_SET(*tset, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofset, TK_MAKE(class, TK_CLIENT_INITIATED));
		break;
	case TK_CL_BORROWED:
		TKC_INC_METER(tm, class, recall);
		tstate->cl_state = TK_CL_BREVOKING;
		break;
	case TK_CL_OWNED:
		TKC_INC_METER(tm, class, recall);
		tstate->cl_state = TK_CL_REVOKING;
		break;
	case TK_CL_OBTAINING:
		TKC_INC_METER(tm, class, recall);
		tstate->cl_state = TK_CL_COBTAINING;
		break;
	case TK_CL_BORROWING:
		TKC_INC_METER(tm, class, recall);
		tstate->cl_state = TK_CL_BOBTAINING;
		break;
	case TK_CL_SOBTAINING:
	case TK_CL_COBTAINING:
	case TK_CL_RETURNING:
	case TK_CL_REVOKING:
	case TK_CL_BREVOKING:
	case TK_CL_SREVOKING:
	case TK_CL_WILLRETURN:
		/* seems harmless enough */
		break;
	case TK_CL_WILLOBTAIN:
#ifdef NEVER
		/*
		 * We may have already revoked and returned
		 * this token, and just be permitting someone
		 * else to obtain a token.
		 * We don't set the mrecall bit - at this point there
		 * is NO way that the server could believe we have
		 * a token.
		 */
		break;
#endif
	default:
		tkci_badstate("tkci_recall_client", ci, class);
		/* NOTREACHED */
	}
	CHECK(tstate);
}

/*
 * tkci_creturn - called in response to a server initiated conditional
 * recall message
 */
/* ARGSUSED */
static void
tkci_creturn(
	tkci_cstate_t *ci,	/* object containing tokens */
	tk_meter_t *tm,
	int class,
	int level,
	tk_set_t *tstoret,	/* tokens to recall */
	tk_set_t *teh,
	tk_disp_t *dofsets)
{
	tkci_clstate_t *tstate;

	tstate = &ci->tkci_state[class];
	CHECK(tstate);

	/*
	 * For recall, level doesn't matter. If we don't think
	 * we have it at all, call it unknown and add it to the 'eh' list.
	 */
	level = downext[tstate->cl_extlev];
	if (level == 0) {
		TK_COMBINE_SET(*teh, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofsets, TK_MAKE(class, TK_SERVER_CONDITIONAL));
		return;
	}

	switch (tstate->cl_state) {
	case TK_CL_CACHED:
		tstate->cl_state = TK_CL_RETURNING;
		TK_COMBINE_SET(*tstoret, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofsets, TK_MAKE(class, TK_SERVER_CONDITIONAL));
		break;
	case TK_CL_OWNED:
		tstate->cl_state = TK_CL_BORROWED;
		break;
	case TK_CL_OBTAINING:
	case TK_CL_RETURNING:
		/*
		 * to avoid a deadlock we need to let this
		 * recall complete w/o waiting for the client
		 * to 'finish' the return.
		 * If, in fact the server still thinks we have
		 * the token, it'll send us another recall request
		 */
		TK_COMBINE_SET(*teh, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofsets, TK_MAKE(class, TK_SERVER_CONDITIONAL));
		break;
	case TK_CL_WILLRETURN:
		/*
		 * WILLRETURN is only entered from a client
		 * initiated return via tkc_returning.
		 * Since the server wants the token - its better
		 * to let the return message be the one that
		 * will satisfy the server rather than a
		 * client initiated one.
		 */
		NEWSTATE(ci, class, tstate, TK_CL_BREVOKING);
		break;
	case TK_CL_REVOKING:
		/* no need to wake anyone thus no NEWSTATE */
		tstate->cl_state = TK_CL_BREVOKING;
		break;
	case TK_CL_CREVOKING:
		/*
		 * the hold count is zero and the thread that set
		 * WILLRETURN in tkc_returning will wake and
		 * realize that we've have already returned the
		 * token.
		 */
		TK_COMBINE_SET(*tstoret, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofsets, TK_MAKE(class, TK_SERVER_CONDITIONAL));
		NEWSTATE(ci, class, tstate, TK_CL_RETURNING);
		break;
	case TK_CL_BORROWED:
	case TK_CL_BORROWING:
	case TK_CL_BOBTAINING:
	case TK_CL_BREVOKING:
		/* this would imply 2 recalls! */
	case TK_CL_SREVOKING:
		/* implies both a mandatory and conditional recall */
	default:
		tkci_badstate("tkci_creturn", ci, class);
		/* NOTREACHED */
	}
	CHECK(tstate);
}

/*
 * tkc_recall - called when:
 * 1) a client knows that its likely that another client would be acquiring
 *	a token that would likely cause a recall. This can be a good
 *	performance win (why == TK_CLIENT_INITIATED)
 * 2) in response to a mandatory server request (why == TK_SERVER_MANDATORY)
 * 3) in response to a conditional server request (TK_SERVER_CONDITIONAL)
 * 
 * the 'why' argument encodes the reason per token.
 *
 * Since a CLIENT_INITIATED revoke is just a 'hint' if we don't think
 * we have the token, we just ignore it.
 * Server initiated recalls we MUST answer at some time - in fact, no
 * more acquires on the token will be permitted until we answer.
 */
void
tkc_recall(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens to return */
	tk_disp_t why)		/* server or client initiated per token */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	tk_set_t tstoret = TK_NULLSET, eh = TK_NULLSET;
	tk_disp_t ds, dofret = TK_NULLSET;
	int disp, level, class;
	TK_LOCKDECL;

	TKC_METER_VAR_INIT(tm, cs);
	TK_LOCK(ci->tkci_lock);
	TK_TRACE(TKC_SRECALL, __return_address, TK_GETCH(ci->tkci_obj),
				tset, TK_NULLSET, why, ci, TK_METER_VAR);

	for (ts = tset, ds = why, class = 0; ts;
			ts = ts >> TK_LWIDTH, ds = ds >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		disp = ds & TK_SET_MASK;
		TK_ASSERT(disp);
		TK_ASSERT((disp & disp - 1) == 0);

		if (disp == TK_SERVER_CONDITIONAL)
			tkci_creturn(ci, TK_METER_VAR, class, level, &tstoret,
						&eh, &dofret);
		else if (disp == TK_SERVER_MANDATORY)
			tkci_mreturn(ci, TK_METER_VAR, class, level,
						&tstoret, &eh, &dofret);
		else
			tkci_recall_client(ci, TK_METER_VAR, class, level,
						&tstoret, &dofret);
	}
	TK_TRACE(TKC_ERECALL, 0, TK_GETCH(ci->tkci_obj), tstoret, eh,
				dofret, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);

	if (tstoret || eh) {
		TK_TRACE(TKC_RETURN_CALL, __return_address,
				TK_GETCH(ci->tkci_obj),
				tstoret, eh, dofret, ci, TK_METER_VAR);
		ci->tkci_if->tkc_return(ci, ci->tkci_obj, tstoret, eh, dofret);
	}
}

/*
 * tkc_returned - return a set of tokens
 * Used by the client to disposition a set of tokens. It must call this
 * in these cases:
 * 1) to return tokens required by a call to (*tkc_return)
 * 2) to return tokens initiated via tkc_returning()
 *
 * Note that it is ILLEGAL to refuse to return a token that is server-initiated.
 */
void
tkc_returned(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens to return */
	tk_set_t refused)	/* tokens refused */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	/* REFERENCED (level) */
	int level;
	int class;
	TK_LOCKDECL;

	TKC_METER_VAR_INIT(tm, cs);
	TK_LOCK(ci->tkci_lock);
	TK_TRACE(TKC_SRETURNED, __return_address, TK_GETCH(ci->tkci_obj),
				tset, refused, TK_NULLSET, ci, TK_METER_VAR);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		TKC_INC_METER(tm, class, return);
		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		switch (tstate->cl_state) {
		case TK_CL_RETURNING:
			TK_ASSERT((tstate->cl_extlev & level) != 0);
			if (tstate->cl_obtain) {
				/*
				 * There can be multiple bits on - including
				 * the one for the level we're returning. This
				 * can happen if say T1 has token for READ,
				 * T2 wants it for WRITE (which set the
				 * state to REVOKING and set WRITE in the
				 * obtain mask); then T3 wants it for READ -
				 * this will cause READ to get placed into
				 * the obtain mask.
				 */
				int toobtain;
				toobtain = obtaintab[tstate->cl_obtain];
				TK_ASSERT(toobtain);
				tstate->cl_obtain &= ~toobtain;
				tstate->cl_mrecall = 0;
				tstate->cl_extlev = 0;
				NEWSTATE(ci, class, tstate, TK_CL_WILLOBTAIN);
			} else {
				tstate->cl_mrecall = 0;
				tstate->cl_extlev = 0;
				NEWSTATE(ci, class, tstate, TK_CL_IDLE);
			}
			break;
		case TK_CL_OBTAINING:
			/*
			 * another thread might have just returned a token while
			 * obtaining a conflicting token
			 * This can happen if the server recalled a token
			 * that was in the WILLOBTAIN state (which
			 * set the state to SWILLOBTAIN). When we change
			 * the state to OBTAINING we send back the old
			 * token (via a (*tkc_return)() callout). This
			 * callout will send back the token and call us
			 *
			 * Note that cl_extlev could contain our token if
			 * for example we are returning RD and another
			 * thread is OBTAINING WR and a third thread goes
			 * for RD - the extlev will show WR|RD,
			 * which has nothing to do with our RD.
			 */
			TK_ASSERT((level & downext[tstate->cl_extlev]) == 0);
			break;
		default:
			tkci_badstate("tkc_returned", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
	}
	for (ts = refused, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < ci->tkci_ntokens);

		TK_ASSERT((level & level - 1) == 0);
		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		switch (tstate->cl_state) {
		case TK_CL_RETURNING:
			TK_ASSERT((tstate->cl_extlev & level) != 0);
			if (tstate->cl_obtain) {
				/*
				 * There can be multiple bits on - including
				 * the one for the level we're returning. This
				 * can happen if say T1 has token for READ,
				 * T2 wants it for WRITE (which set the
				 * state to REVOKING and set WRITE in the
				 * obtain mask); then T3 wants it for READ -
				 * this will cause READ to get placed into
				 * the obtain mask.
				 */
				int toobtain;
				toobtain = obtaintab[tstate->cl_obtain];
				TK_ASSERT(toobtain);
				tstate->cl_obtain &= ~toobtain;
				if (tstate->cl_extlev & toobtain) {
					/* what we've got and what they want
					 * are the same - call it CACHED
					 */
					NEWSTATE(ci, class, tstate, TK_CL_CACHED);
				} else {
					NEWSTATE(ci, class, tstate, TK_CL_WILLOBTAIN);
				}
			} else {
				NEWSTATE(ci, class, tstate, TK_CL_CACHED);
			}
			break;
		case TK_CL_BORROWING:
		case TK_CL_OBTAINING:
		default:
			tkci_badstate("tkc_return-refused", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
	}
	TK_TRACE(TKC_ERETURNED, 0, TK_GETCH(ci->tkci_obj),
			TK_NULLSET, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);
}

/*
 * tkc_returning - return a set of tokens
 * Used by the client to push tokens back to the server
 * We are particularly forgiving in what can be passed in tset -
 * it is very reasonable to turn on all 3 levels. We will return the
 * one we have ..
 *
 * If wait_to_resolve is TRUE then we may well sleep - since things can
 * change while we sleep, we basically must re-start the loop if we do.
 * So, we only return if we get through all of tset w/o sleeping
 *
 * Note that we use the WILLRETURN state - this basically means that no
 * other acquires may be done (they will PANIC)
 */
void
tkc_returning(
	tkc_state_t *cs,	/* object containing tokens */
	tk_set_t tset,		/* tokens to return */
	tk_set_t *ok,		/* OUT:tokens that are ok to return */
	tk_disp_t *okwhy,	/* OUT:disposition of ok tokens */
	int wait_to_resolve)	/* if TRUE then wait for tokens in process */
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	int level, class;
	TK_LOCKDECL;

	TKC_METER_VAR_INIT(tm, cs);
	*ok = TK_NULLSET;
	*okwhy = TK_NULLSET;

	TK_LOCK(ci->tkci_lock);
	TK_TRACE(TKC_SRETURNING, __return_address, TK_GETCH(ci->tkci_obj),
				tset, TK_NULLSET, TK_NULLSET, ci, TK_METER_VAR);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
tryagain:
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		/* heck, they can even pass total garbage! */
		if (class >= ci->tkci_ntokens)
			break;

		tstate = &ci->tkci_state[class];
		CHECK(tstate);
		if ((downext[tstate->cl_extlev] & level) == 0)
			/* we don't have it */
			continue;

		if (tstate->cl_state == TK_CL_CACHED) {
			tstate->cl_state = TK_CL_RETURNING;
			CHECK(tstate);
			TK_COMBINE_SET(*ok, TK_MAKE(class, downext[tstate->cl_extlev]));
			TK_COMBINE_SET(*okwhy, TK_MAKE(class, TK_CLIENT_INITIATED));
			continue;
		}
		if (!wait_to_resolve) {
			/* thats all */
			continue;
		}

		switch (tstate->cl_state) {
		case TK_CL_IDLE:
			/* ignore */
			break;
		case TK_CL_SREVOKING:
		case TK_CL_BREVOKING:
		case TK_CL_RETURNING:
			tkci_scwait(ci, class, TK_LOCKVARADDR);
			TK_LOCK(ci->tkci_lock);
			goto tryagain;
		case TK_CL_BORROWED:
			tstate->cl_state = TK_CL_BREVOKING;
			tkci_scwait(ci, class, TK_LOCKVARADDR);
			TK_LOCK(ci->tkci_lock);
			goto tryagain;
		case TK_CL_OWNED:
		case TK_CL_REVOKING:
			/*
			 * OWNED is changed to WILLRETURN to force no
			 * more grants.
			 * REVOKING is changed to WILLRETURN to avoid
			 * an extra message - the caller has signified
			 * their desire to send a message
			 */
			tstate->cl_state = TK_CL_WILLRETURN;
			CHECK(tstate);

			tkci_scwait(ci, class, TK_LOCKVARADDR);
			TK_LOCK(ci->tkci_lock);
			if (tstate->cl_state == TK_CL_CREVOKING) {
				TK_COMBINE_SET(*ok, TK_MAKE(class, downext[tstate->cl_extlev]));
				TK_COMBINE_SET(*okwhy, TK_MAKE(class, TK_CLIENT_INITIATED));
				NEWSTATE(ci, class, tstate, TK_CL_RETURNING);
				break;
			} else {
				goto tryagain;
			}
		case TK_CL_COBTAINING:
		case TK_CL_SOBTAINING:
		case TK_CL_OBTAINING:
		case TK_CL_BORROWING:
			/*
			 * XXX does this really get woken all the time even
			 * though we don't set a hold count??
			 */
			tkci_obtainwait(ci, class, TK_LOCKVARADDR);
			TK_LOCK(ci->tkci_lock);
			goto tryagain;
		case TK_CL_WILLOBTAIN:
		case TK_CL_SWILLOBTAIN:
		case TK_CL_REJECTED:
			tkci_scwait(ci, class, TK_LOCKVARADDR);
			/* something has changed - try again */
			TK_LOCK(ci->tkci_lock);
			goto tryagain;
		default:
			tkci_badstate("tkc_returning", ci, class);
			/* NOTREACHED */
		}
		CHECK(tstate);
	}
	TK_TRACE(TKC_ERETURNING, 0, TK_GETCH(ci->tkci_obj), *ok, TK_NULLSET,
				TK_NULLSET, ci, TK_METER_VAR);
	TK_UNLOCK(ci->tkci_lock);
}

/*
 * Clear state and destroy.
 */
void
tkc_free(
	tkc_state_t	*cs)	/* object to be freed */
{
	tkci_cstate_t	*ci = (tkci_cstate_t *)cs;
	int		i;
	for (i = 0; i < ci->tkci_ntokens; i++) {
		ci->tkci_state[i].cl_word = 0;
		ci->tkci_state[i].cl_state = TK_CL_IDLE;
	}
	if (ci->tkci_if == &tkc_local_iface)
		tkc_destroy_local(cs);
	else
		tkc_destroy(cs);
}


/*
 * Copy token state.
 */
void
tkc_copy(
	tkc_state_t	*src_cs,
	tkc_state_t	*dst_cs)
{
	tkci_cstate_t	*src_ci = (tkci_cstate_t *)src_cs;
	tkci_cstate_t	*dst_ci = (tkci_cstate_t *)dst_cs;
	int		i;
	for (i = 0; i < src_ci->tkci_ntokens; i++) {
		dst_ci->tkci_state[i].cl_word = src_ci->tkci_state[i].cl_word;
	}
}

#ifdef _KERNEL
#define OUTPUT		qprintf(
void
tkc_print(tkc_state_t *cs, char *fmt, ...)

#else

#define OUTPUT		fprintf(f,
void
tkc_print(tkc_state_t *cs, FILE *f, char *fmt, ...)
#endif
{
	tkci_cstate_t *ci = (tkci_cstate_t *)cs;
	tkci_clstate_t tstate;
	int i;
	char buf[512], *p;
	va_list ap;

	p = buf;
	if (fmt) {
		va_start(ap, fmt);
		vsprintf(p, fmt, ap);
		p += strlen(p);
	}
	p = __tk_sprintf(p, "obj 0x%x ref %d ntokens %d origcell %d\n", ci->tkci_obj,
			ci->tkci_ref,
			ci->tkci_ntokens,
#if (_MIPS_SZPTR == 64)
			ci->tkci_origcell
#else
			0
#endif
			);
	for (i = 0; i < ci->tkci_ntokens; i++) {
		tstate = ci->tkci_state[i];
		p = __tk_sprintf(p, "\tclass %d:level %s hold %d state %s mrecall %s obtain %s\n",
				i,
				__tk_levtab[tstate.cl_extlev],
				tstate.cl_hold,
				__tkci_prstate(tstate),
				__tk_levtab[tstate.cl_mrecall],
				__tk_levtab[tstate.cl_obtain]);
	}
	OUTPUT buf);

#if TK_METER
	if (ci->tkci_flags & TKC_METER) {
		tk_meter_t *tm = tkc_meter(cs);
		OUTPUT " meter @0x%p tag 0x%lx name \"%s\"\n",
				tm, tm->tm_tag, tm->tm_name);
		for (i = 0; i < ci->tkci_ntokens; i++) {
			OUTPUT  " Cl:%d acq %d cached %d conflict %d outbound %d\n",	
				i,
				TKC_GET_METER(tm, i, acquire),
				TKC_GET_METER(tm, i, acq_cached),
				TKC_GET_METER(tm, i, acq_conflict),
				TKC_GET_METER(tm, i, acq_outbound));
			OUTPUT  "      recall %d return %d\n",	
				TKC_GET_METER(tm, i, recall),
				TKC_GET_METER(tm, i, return));
		}

	}
#endif
}

static char *cstates[] = {
	"BAD-0",
	"IDLE",
	"OWNED",
	"CACHED",
	"REVOKING",
	"OBTAINING",
	"ALREADY",
	"WILLRETURN",
	"RETURNING",
	"WILLOBTAIN",
	"BORROWED",
	"BORROWING",
	"REJECTED",
	"BREVOKING",
	"SREVOKING",
	"SOBTAINING",
	"COBTAINING",
	"BOBTAINING",
	"SWILLOBTAIN",
	"CREVOKING",
	"CRESTORE",
	"SRESTORE",
	"BRESTORE",
};

char *
__tkci_prstate(tkci_clstate_t s)
{
	return (cstates[s.cl_state]);
}

static void
tkci_badstate(char *s, tkci_cstate_t *ci, int class)
{
	tkci_clstate_t tstate;

	tstate = ci->tkci_state[class];
	__tk_fatal("%s - bad state %s for class %d - ci 0x%x\n",
				s, __tkci_prstate(tstate), class, ci);
	/* NOTREACHED */
}

/*
 * tkci_obtainwait - wait until a token is obtained - there is already a
 * client that has done the appropriate RPC
 * The token object lock is held across until we sleep.
 */
/* ARGSUSED */
static void
tkci_obtainwait(tkci_cstate_t *ci, int class, int *l)
{
	tk_list_t *tl;
	TK_LOCKDECL;

	TK_LOCK(obtainlock);
	for (tl = obtainhead.tk_next; tl != &obtainhead; tl = tl->tk_next) {
		if (tl->tk_tag1 == (void *)ci && tl->tk_tag2 == class)
			break;
	}
	if (tl != &obtainhead) {
		/* someone already waiting - queue up on that sema */

		TK_ASSERT(tl->tk_nwait > 0);
		tl->tk_nwait++;
		TK_UNLOCK(obtainlock);
	} else {
		/* we can unlock the obtainlock across the malloc, etc
		 * since we are protected via the token object lock.
		 * this means that noone can attempt to wake us.
		 */
		TK_UNLOCK(obtainlock);

		/* XXX don't malloc with lock held */
		tl = TK_NODEMALLOC(sizeof(*tl));
		tl->tk_tag1 = (void *)ci;
		tl->tk_tag2 = class;
		tl->tk_nwait = 1;
		TK_CREATE_SYNC(tl->tk_wait);

		/* link onto global class wait list */
		TK_LOCK(obtainlock);
		tl->tk_next = obtainhead.tk_next;
		tl->tk_prev = &obtainhead;
		obtainhead.tk_next->tk_prev = tl;
		obtainhead.tk_next = tl;
		TK_UNLOCK(obtainlock);
	}

	TK_UNLOCKVAR(ci->tkci_lock, *l);

	/* wait */
	TK_SYNC(tl->tk_wait);

	TK_LOCK(obtainlock);
	TK_ASSERT(tl->tk_nwait > 0);
	if (--tl->tk_nwait == 0) {
		TK_DESTROY_SYNC(tl->tk_wait);
		TK_NODEFREE(tl);
	}
	TK_UNLOCK(obtainlock);
}

/*
 * tkci_obtainwake - wake all threads waiting for an obtain
 */
static void
tkci_obtainwake(tkci_cstate_t *ci, int class)
{
	int i;
	tk_list_t *tl;
	TK_LOCKDECL

	TK_LOCK(obtainlock);
	for (tl = obtainhead.tk_next; tl != &obtainhead; tl = tl->tk_next) {
		if (tl->tk_tag1 == (void *)ci && tl->tk_tag2 == class)
			break;
	}
	if (tl == &obtainhead) {
		TK_UNLOCK(obtainlock);
		return;
	}

	/* remove from list */
	tl->tk_next->tk_prev = tl->tk_prev;
	tl->tk_prev->tk_next = tl->tk_next;

	TK_ASSERT(tl->tk_nwait > 0);
	/* wake them all up */
	for (i = 0; i < tl->tk_nwait; i++)
		TK_WAKE(tl->tk_wait);
	TK_UNLOCK(obtainlock);
}

#if TK_DEBUG
static int
tkci_nobtainwaiting(tkci_cstate_t *ci, int class)
{
	int i;
	tk_list_t *tl;
	TK_LOCKDECL

	TK_LOCK(obtainlock);
	for (tl = obtainhead.tk_next; tl != &obtainhead; tl = tl->tk_next) {
		if (tl->tk_tag1 == (void *)ci && tl->tk_tag2 == class)
			break;
	}
	if (tl == &obtainhead) {
		TK_UNLOCK(obtainlock);
		return 0;
	}

	i = tl->tk_nwait;
	TK_UNLOCK(obtainlock);
	return i;
}
#endif

/*
 * tkci_scwait - wait for status change
 * The token object lock is held across until we sleep.
 */
/* ARGSUSED */
static void
tkci_scwait(tkci_cstate_t *ci, int class, int *l)
{
	tk_list_t *tl;
	TK_LOCKDECL
	tkci_clstate_t *tstate;

	tstate = &ci->tkci_state[class];
	if (tstate->cl_scwait == 1) {
		/* someone already waiting - queue up on that sema */
		TK_LOCK(sclock);
		for (tl = schead.tk_next; tl != &schead; tl = tl->tk_next) {
			if (tl->tk_tag1 == (void *)ci && tl->tk_tag2 == class)
				break;
		}
		TK_ASSERT(tl != &schead);
		TK_ASSERT(tl->tk_nwait > 0);
		tl->tk_nwait++;
		TK_UNLOCK(sclock);
	} else {
		/* XXX don't malloc with lock held */
		tl = TK_NODEMALLOC(sizeof(*tl));
		tl->tk_tag1 = (void *)ci;
		tl->tk_tag2 = class;
		tl->tk_nwait = 1;
		TK_CREATE_SYNC(tl->tk_wait);

		/* link onto global status change wait list */
		TK_LOCK(sclock);
		tl->tk_next = schead.tk_next;
		tl->tk_prev = &schead;
		schead.tk_next->tk_prev = tl;
		schead.tk_next = tl;
		TK_UNLOCK(sclock);
		tstate->cl_scwait = 1;
	}

	TK_UNLOCKVAR(ci->tkci_lock, *l);

	/* wait */
	TK_SYNC(tl->tk_wait);

	TK_LOCK(sclock);
	TK_ASSERT(tl->tk_nwait > 0);
	if (--tl->tk_nwait == 0) {
		TK_DESTROY_SYNC(tl->tk_wait);
		TK_NODEFREE(tl);
	}
	TK_UNLOCK(sclock);
}

/*
 * tkci_scwake - wake all threads waiting for a status change
 * Note that token lock must be held across entire operation
 */
static void
tkci_scwake(tkci_cstate_t *ci, int class)
{
	tk_list_t *tl;
	int i;
	TK_LOCKDECL;

	TK_ASSERT(ci->tkci_state[class].cl_scwait);
	TK_LOCK(sclock);
	for (tl = schead.tk_next; tl != &schead; tl = tl->tk_next) {
		if (tl->tk_tag1 == (void *)ci && tl->tk_tag2 == class)
			break;
	}
	TK_ASSERT(tl != &schead);

	/* remove from list */
	tl->tk_next->tk_prev = tl->tk_prev;
	tl->tk_prev->tk_next = tl->tk_next;

	TK_ASSERT(tl->tk_nwait > 0);
	/* wake them all up */
	for (i = 0; i < tl->tk_nwait; i++)
		TK_WAKE(tl->tk_wait);
	TK_UNLOCK(sclock);

	/* noone waiting */
	ci->tkci_state[class].cl_scwait = 0;
}
