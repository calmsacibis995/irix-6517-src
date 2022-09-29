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
#ident "$Id: tks.c,v 1.54 1997/09/25 20:22:26 henseler Exp $"

#include "tk_private.h"

/*
 * Token server implementation
 *
 * 1) currently tks_obtain waits until the token is obtained then returns.
 *	this requires alot of threads potentially - should we change it to
 *	stash away the response message info and return and have another
 *	callout when we actually get it?
 * 2) currently, tks_obtain always will get all tokens - there is
 *	no communication between tks_cleancell and tks_obtain to
 *	return immediately with a 'refused' set.
 */
/* list manipulation */
static void tksi_listwait(tk_list_t *, tksi_sstate_t *, int, int *);
static void tksi_listwake(tk_list_t *, tksi_sstate_t *, int);
static tk_list_t classhead; /* class list */
static tk_list_t revokehead; /* revoke list */
static bitlen_t tksi_maxcnum;	/* max number for client number */
static sv_t	recall_sv;
int __tksi_goffset[TK_MAXTOKENS]; /* offset table for grant bitmaps */

#if !_KERNEL
/* client handle manipulation */
static int tksi_getcnum(tks_ch_t);
static tks_ch_t tksi_getch(int);
typedef struct tksi_chlist_s {
	tks_ch_t handle;
	int cnum;
	struct tksi_chlist_s *next;
} tksi_chlist_t;
static tksi_chlist_t chhead;
static tklock_t chlock;

#else /* _KERNEL */

/* for kernel we assume that cells are small integers */
#define tksi_getcnum(x)	x
#define tksi_getch(x) x

#endif /* _KERNEL */

static void tksi_mrecallall(tksi_sstate_t *, tks_ch_t, int, int);

/*
 * Some (artificial) limits of the implementation
 * MAXCNUM should really be a tuneable..
 */
#define MAXCNUM	MAX_CELLS	/* max value of client handle num (== cells) */

/* bitmap allocation */
static char *tksi_allocbm(int);
static void tksi_freebm(char *);
static void tksi_initbm(void);
static void tksi_zerobm(char *, int);
#define     tksi_sizebm(ntokens)	((ntokens) * ((int)tksi_maxcnum / 8))

/*
 * Barriers to synchronize recovery threads with ongoing tks_iterates.
 * If a tks_iterate is in progress for a given cell, the mrlock is held in
 * MR_ACCESS mode. The recovery thread holds the mrlock in update mode
 * to wait for previous tks_iterates to complete.
 */ 
mrlock_t	tks_iterate_barrier[MAXCNUM];

/*
 * Per cell initialization
 */
void
tks_init(void)
{
	int i;

	__tk_lock_init();
	revokehead.tk_next = revokehead.tk_prev = &revokehead;
	__tk_create_lock(&revokehead.tk_lock);

	/*
	 * must be a multiple of 32 for bitmap to align correctly
	 */
	tksi_maxcnum = MAXCNUM;
	while (tksi_maxcnum & 0x1f)
		tksi_maxcnum++;

	for (i = 0; i < MAXCNUM; i++)
		mrlock_init(&tks_iterate_barrier[i], MRLOCK_DEFAULT, 
						"iterate_barrier", i);

	classhead.tk_next = classhead.tk_prev = &classhead;
	__tk_create_lock(&classhead.tk_lock);
	/* init grant bitmap offset table */
	for (i = 0; i < TK_MAXTOKENS; i++) {
		__tksi_goffset[i] = i * ((int)tksi_maxcnum / 8);
	}
	tksi_initbm();

#if !_KERNEL
	chhead.next = NULL;
	__tk_create_lock(&chlock);
#endif
#if TK_TRC
	__tk_trace_init();
#endif
#if TK_METER
	__tk_meter_init();
#endif
#ifndef _KERNEL
	__tk_ts_init();
#endif
	sv_init(&recall_sv, SV_DEFAULT, "tks_recall");
}

/*
 * tks_create - create a token managed object
 */
/* ARGSUSED */
void
tks_create(
	char *name,
	tks_state_t *ss,	/* object state to set up */
	void *o,		/* server object pointer */
	tks_ifstate_t *is,	/* interface state */
	int ntokens,		/* # of tokens in set */
	void *tag)		/* tag for tracing */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	TK_METER_DECL /* decls 'tm' */

	si->tksi_if = is;
	si->tksi_obj = o;
	si->tksi_ntokens = ntokens;
	si->tksi_flags = 0;
	si->tksi_nrecalls = 0;
	si->tksi_ref = 0;
#if _KERNEL
	si->tksi_origcell = cellid();
#endif

	bzero(&si->tksi_state[0], ntokens * (int)sizeof(tksi_svrstate_t));
	si->tksi_gbits = tksi_allocbm(ntokens);
	__tk_create_lock(&si->tksi_lock);

#if TK_METER
	tm = NULL;
	if (__tk_defaultmeter)
		tm = tks_meteron(ss, name, tag);
#endif
	TK_TRACE(TKS_CREATE, __return_address, NULL,
			TK_NULLSET, TK_NULLSET, TK_NULLSET, si, TK_METER_VAR);

}

/*
 * tks_destroy - stop managing an object.
 * It is assumed that NO tokens are held and that NO requests for any
 * more tokens can come in.
 *
 * We do have to synchronize with tks_return so that we permit a thread
 * in tks_return to complete before we tear everything down. Note
 * however that it is illegal to call tks_return AFTER tks_destroy -
 * but a thread could be in tks_return.
 */
static int __tk_destroywaits;

void
tks_destroy(
	tks_state_t *ss)	/* object to be destroyed */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	int i;

	TKS_METER_VAR_INIT(tm, ss);
	TK_ASSERT((si->tksi_flags & TKS_INRECALL) == 0);
	TK_TRACE(TKS_DESTROY, __return_address, NULL, TK_NULLSET, TK_NULLSET,
				TK_NULLSET, si, TK_METER_VAR);
	while (si->tksi_ref) {
		__tk_destroywaits++;
		TK_DELAY(1);
	}

	si->tksi_if = NULL;
	si->tksi_obj = NULL;
	tstate = &si->tksi_state[0];
        for (i = 0; i < si->tksi_ntokens; i++, tstate++) {
		TK_ASSERT(tstate->svr_noutreqs == 0);
		TK_ASSERT(tstate->svr_lock == 0);
		TK_ASSERT(tstate->svr_lockw == 0);
		TK_ASSERT(tstate->svr_rwait == 0);
		TK_ASSERT(tstate->svr_grants == 0);
		TK_ASSERT(tstate->svr_revoke == 0);
		TK_ASSERT(tstate->svr_recall == 0);
		TK_ASSERT(bftstclr(si->tksi_gbits + __tksi_goffset[i], \
			0, tksi_maxcnum) == tksi_maxcnum);
#if TK_METER
		if (tm) {
			char *rp;
			rp = TKS_GET_METER(tm, i, trackcrecall);
			if (rp) {
				TK_ASSERT(bftstclr(rp, \
					0, tksi_maxcnum) == tksi_maxcnum);
				tksi_freebm(rp);
			}
			rp = TKS_GET_METER(tm, i, trackmrecall);
			if (rp) {
				TK_ASSERT(bftstclr(rp, \
					0, tksi_maxcnum) == tksi_maxcnum);
				tksi_freebm(rp);
			}
		}
#endif
	}
#if TK_METER
	tks_meteroff(ss);
#endif
	tksi_freebm(si->tksi_gbits);
	si->tksi_ntokens = 0;
	__tk_destroy_lock(&si->tksi_lock);
}

/*
 * tks_obtain - obtain a set of tokens on behalf of a client.
 * Does not return until all tokens are either granted, or the client
 * already has them.
 */
void
tks_obtain(
	tks_state_t *ss,	/* object containing token */
	tks_ch_t h,		/* handle for client making request */
	tk_set_t tw,		/* tokens wanted */
	tk_set_t *tg,		/* tokens granted */
	tk_set_t *tr,		/* tokens refused */
	tk_set_t *th)		/* tokens already held by client */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	bitnum_t cnum = (bitnum_t)tksi_getcnum(h);
	char *bp;
	int level, class;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	TK_TRACE(TKS_SOBTAIN, __return_address, h, tw, TK_NULLSET, TK_NULLSET,
					si, TK_METER_VAR);
	*tg = TK_NULLSET;
	*th = TK_NULLSET;
	for (ts = tw, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;

		TKS_INC_METER(tm, class, obtain);
		TK_ASSERT((level & level - 1) == 0);
		tstate = &si->tksi_state[class];
		bp = si->tksi_gbits + __tksi_goffset[class];
		TK_LOCK(si->tksi_lock);
		/*
		 * the svr_lock serializes the basic server operations -
		 * making sure that an operation (such as obtain) completes
		 * before we start the next operation. Note that any operation
		 * may require numerous messages and responses to complete
		 */
		while (tstate->svr_lock) {
			tksi_listwait(&classhead, si, class, TK_LOCKVARADDR);
			TK_LOCK(si->tksi_lock);
			TK_ASSERT(tstate->svr_recall == 0);
		}
		TK_ASSERT(tstate->svr_recall == 0);

		TK_ASSERT((tstate->svr_level & tstate->svr_level - 1) == 0);
		if ((level & tstate->svr_level) && level != TK_WRITE) {
			/* wants level that is already outstanding
			 * fine - if we believe client already
			 * has token add to already-set else
			 * to granted-set
			 */
				TK_ASSERT(tstate->svr_grants > 0);
			if (btst(bp, cnum)) {
				*th |= TK_MAKE(class, level);
			} else {
				bset(bp, cnum);
				tstate->svr_grants++;
				*tg |= TK_MAKE(class, level);
			}
		} else if (tstate->svr_level == 0) {
			/* token not given out yet */
			TK_ASSERT(bftstclr(bp, 0, tksi_maxcnum) == tksi_maxcnum);
			TK_ASSERT(tstate->svr_grants == 0);
			bset(bp, cnum);
			tstate->svr_grants++;
			tstate->svr_level = level;
			*tg |= TK_MAKE(class, level);
		} else {
			/*
			 * wants conflicting level
			 * locking the class keeps anyone else from doing
			 * an obtain on this class.
			 */
			tstate->svr_lock = 1;
			tstate->svr_revoke = 1;
			tstate->svr_revinp = 1;
			tstate->svr_backoff = 0;
#if TK_METER
			if (tm)
				TKS_FIELD_METER(tm, class, revracecur) = 0;
#endif
			TK_UNLOCK(si->tksi_lock);

			/* callback recall function */
			tksi_mrecallall(si, h, class, tstate->svr_level);

			TK_LOCK(si->tksi_lock);
			TK_ASSERT(tstate->svr_recall == 0);
			TK_ASSERT(tstate->svr_revoke == 0);
			TK_ASSERT(tstate->svr_noutreqs == 0);

			TK_ASSERT(tstate->svr_grants == 0);
			TK_ASSERT(bftstclr(bp, 0, tksi_maxcnum) == tksi_maxcnum);
			/* give out token */
			bset(bp, cnum);
			tstate->svr_grants++;
			tstate->svr_level = level;
			*tg |= TK_MAKE(class, level);
			TK_ASSERT(tstate->svr_lock);

			tstate->svr_lock = 0;
			if (tstate->svr_lockw)
				tksi_listwake(&classhead, si, class);
			/* XXX once we wake how do we make sure that
			 * obtainer gets its message before someone else
			 * comes in and starts a revoke sequence??
			 */
		}
		TK_UNLOCK(si->tksi_lock);
	}
	if (tr)
		*tr = TK_SUB_SET(tw, TK_ADD_SET(*tg, *th));
	else {
		TK_ASSERT(TK_SUB_SET(tw, TK_ADD_SET(*tg, *th)) == TK_NULLSET);
	}
	TK_TRACE(TKS_EOBTAIN, 0, h, *tg, *th, TK_NULLSET, si, TK_METER_VAR);
	return;
}

/*
 * tks_obtain_conditional - obtain a set of tokens on behalf of a client.
 * Does not return until all mandatory tokens are either granted, or the client
 * already has them.  Also attempts to get conditional tokens, but not very hard.
 */
void
tks_obtain_conditional(
	tks_state_t *ss,	/* object containing token */
	tks_ch_t h,		/* handle for client making request */
	tk_set_t tw,		/* mandatory tokens */
	tk_set_t tc,		/* conditional tokens */
	tk_set_t *tg,		/* tokens granted */
	tk_set_t *tr,		/* tokens refused */
	tk_set_t *th)		/* tokens already held by client */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	bitnum_t cnum = (bitnum_t)tksi_getcnum(h);
	char *bp;
	int level, class;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	TK_TRACE(TKS_SCOBTAIN, __return_address, h, tw, tc, TK_NULLSET,
					si, TK_METER_VAR);
	*tg = TK_NULLSET;
	*th = TK_NULLSET;

	tks_obtain(ss, h, tw, tg, tr, th);

	/*
	 * Now try for conditional tokens.  Only grant those that can be done
	 * immediately and only if we got all the mandatory ones.
	 */
	if (tw != *tg) {
		if (tr) 
			*tr = TK_SUB_SET(tw, TK_ADD_SET(*tg, *th));
		else {
			TK_ASSERT(0);
		}
		TK_TRACE(TKS_ECOBTAIN, 0, h, *tg, *th, TK_NULLSET, si, TK_METER_VAR);
		return;
	}
	for (ts = tc, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;

		TKS_INC_METER(tm, class, obtain);
		TK_ASSERT((level & level - 1) == 0);
		tstate = &si->tksi_state[class];
		bp = si->tksi_gbits + __tksi_goffset[class];
		TK_LOCK(si->tksi_lock);
		/*
		 * the svr_lock serializes the basic server operations -
		 * making sure that an operation (such as obtain) completes
		 * before we start the next operation. Note that any operation
		 * may require numerous messages and responses to complete
		 */
		while (tstate->svr_lock) {
			tksi_listwait(&classhead, si, class, TK_LOCKVARADDR);
			TK_LOCK(si->tksi_lock);
			TK_ASSERT(tstate->svr_recall == 0);
		}
		TK_ASSERT(tstate->svr_recall == 0);

		TK_ASSERT((tstate->svr_level & tstate->svr_level - 1) == 0);
		if ((level & tstate->svr_level) && level != TK_WRITE) {
			/* wants level that is already outstanding
			 * fine - if we believe client already
			 * has token add to already-set else
			 * to granted-set
			 */
				TK_ASSERT(tstate->svr_grants > 0);
			if (btst(bp, cnum)) {
				*th |= TK_MAKE(class, level);
			} else {
				bset(bp, cnum);
				tstate->svr_grants++;
				*tg |= TK_MAKE(class, level);
			}
		} else if (tstate->svr_level == 0) {
			/* token not given out yet */
			TK_ASSERT(bftstclr(bp, 0, tksi_maxcnum) == tksi_maxcnum);
			TK_ASSERT(tstate->svr_grants == 0);
			bset(bp, cnum);
			tstate->svr_grants++;
			tstate->svr_level = level;
			*tg |= TK_MAKE(class, level);
		}
		TK_UNLOCK(si->tksi_lock);
	}
	if (tr)
		*tr = TK_SUB_SET(TK_ADD_SET(tw, tc), TK_ADD_SET(*tg, *th));
	else {
		TK_ASSERT(TK_SUB_SET(TK_ADD_SET(tw, tc), TK_ADD_SET(*tg, *th)) == TK_NULLSET);
	}
	TK_TRACE(TKS_ECOBTAIN, 0, h, *tg, *th, TK_NULLSET, si, TK_METER_VAR);
	return;
}

/*
 * tks_is_idle - return 1 if the token server is completely idle, else 
 *	return 0.
 */
int
tks_is_idle(
	tks_state_t *ss)	
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	int i;
	int is_idle = 1;

	if ((si->tksi_flags & TKS_INRECALL) != 0)
		is_idle = 0;
	if (si->tksi_ref) 
		is_idle = 0;

	tstate = &si->tksi_state[0];
        for (i = 0; i < si->tksi_ntokens; i++, tstate++) {
		if (tstate->svr_noutreqs != 0)
			is_idle = 0;
		if (tstate->svr_lock != 0)
			is_idle = 0;
		if (tstate->svr_lockw != 0)
			is_idle = 0;
		if (tstate->svr_rwait != 0)
			is_idle = 0;
		if (tstate->svr_grants != 0)
			is_idle = 0;
		if (tstate->svr_revoke != 0)
			is_idle = 0;
		if (tstate->svr_recall != 0)
			is_idle = 0;
	}

	return is_idle;
}

/*
 * handle 'eh' tokens from a server initiated mandatory recall
 * These tokens may be the result of a race where the client is
 * in the process of returning the token and we went off and tried to
 * recall (mandatory) it.
 */
static int
tksi_sr(
	tksi_sstate_t *si,	/* object containing token */
	bitnum_t cnum,
	int class,
	int level,
	tk_set_t *tset,
	tk_disp_t *dofset,
	int refused)		/* token refused to be returned */
{
	tksi_svrstate_t *tstate;
	char *bp;
	int bk = 0;
	TK_METER_DECL /* decls 'tm' */

	TKS_METER_VAR_INIT(tm, si);

	tstate = &si->tksi_state[class];

	TK_ASSERT(!(tstate->svr_recall));

	TK_ASSERT(tstate->svr_revoke);
	TK_ASSERT(tstate->svr_noutreqs > 0);

	/*
	 * if we still think client has token, send another revoke
	 * message
	 */
	bp = si->tksi_gbits + __tksi_goffset[class];
	if (btst(bp, cnum) && !refused) {
		TK_COMBINE_SET(*tset, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofset, TK_MAKE(class, TK_SERVER_MANDATORY));
		if (bk < tstate->svr_backoff)
			bk = tstate->svr_backoff;
		if (tstate->svr_backoff < TK_MAXBACKOFF)
			tstate->svr_backoff++;
		TKS_INC_METER(tm, class, revrace);
		TKS_INC_METER(tm, class, revracecur);
#if TK_DEBUG && !_KERNEL
		if (TKS_GET_METER(tm, class, revracecur) > (10 * tstate->svr_noutreqs)) {
			/* dump log */
			kill(getpid(), SIGUSR2);
			fprintf(stderr, "%d:revracecur > 10!\n",
				getpid());
			/* stop us all! */
			sigsend(P_PGID, P_MYID, SIGSTOP);
		}
#else
		if ((TKS_GET_METER(tm, class, revracecur) % 10) == 0)
			cmn_err(CE_WARN, "token server 0x%x revracecur %d\n",
				si, TKS_GET_METER(tm, class, revracecur));
#endif
	} else {
		/* client returned it! great. OR refused */
#if TK_METER
		if (tm) {
			char *rp = TKS_GET_METER(tm, class, trackmrecall);
			TK_ASSERT(rp);
			TK_ASSERT(btst(rp, cnum));
			bclr(rp, cnum);
		}
#endif
		TK_ASSERT(tstate->svr_noutreqs > 0);
		tstate->svr_noutreqs--;
		/*
		 * we don't need complicated flags as for recall since
		 * mrecallall is waiting for a single token..
		 */
		if (!tstate->svr_revinp && tstate->svr_noutreqs == 0) {
			TK_ASSERT(refused || bftstclr(bp, 0, tksi_maxcnum) == tksi_maxcnum);
			TK_ASSERT(tstate->svr_level == 0);
			if (tstate->svr_rwait)
				tksi_listwake(&revokehead, si, class);
		}
	}
	return bk;
}

/*
 * tksi_rr - called by tks_return
 * Check to see it token are ones a recall is pending on and if so checks
 * to see if all recalled classes are complete and if so calls the
 * (*tks_recalled)() callout
 * XXX locking - is this necessary/sufficient??
 */
static int
tksi_rr(
	tksi_sstate_t *si,	/* object containing token */
	bitnum_t cnum,
	int class,
	int level,
	tk_set_t *torec,
	tk_disp_t *dofrec,
	int *done,
	int refused)		/* true if token was refused to be returned */
{
	tksi_svrstate_t *tstate;
	char *bp;
	int bk = 0;
	TK_METER_DECL /* decls 'tm' */

	TKS_METER_VAR_INIT(tm, si);

	/*
	 * handle 'unknown' tokens - these are most likely races.
	 * If in fact the client has already returned the token
	 * all is well, otherwise we need to resend the recall msg
	 */
	tstate = &si->tksi_state[class];
	TK_ASSERT(tstate->svr_revoke == 0);
	TK_ASSERT(si->tksi_nrecalls > 0);

	/*
	 * if we still think client has token, send another revoke
	 * message
	 */
	bp = si->tksi_gbits + __tksi_goffset[class];
	if (btst(bp, cnum) && !refused) {
		TK_COMBINE_SET(*torec, TK_MAKE(class, level));
		TK_COMBINE_SET(*dofrec, TK_MAKE(class, TK_SERVER_CONDITIONAL));
		/*
		 * XXX this isn't the greatest - using a per
		 * token backoff since it really doesn't make much
		 * sense when there are multiple tokens
		 */
		bk = tstate->svr_backoff;
		if (tstate->svr_backoff < TK_MAXBACKOFF)
			tstate->svr_backoff++;
		TKS_INC_METER(tm, class, recrace);
		TKS_INC_METER(tm, class, recracecur);
#if TK_DEBUG
#if !_KERNEL
		if (TKS_GET_METER(tm, class, recracecur) > 10) {
			/* dump log */
			kill(getpid(), SIGUSR2);
			fprintf(stderr, "%d:recracecur > 10!\n",
				getpid());
			/* stop us all! */
			sigsend(P_PGID, P_MYID, SIGSTOP);
		}
#else
		if ((TKS_GET_METER(tm, class, recracecur) % 10) == 0)
			cmn_err(CE_WARN, "token server 0x%x recrace %d\n",
				si, TKS_GET_METER(tm, class, recracecur));
#endif
#endif
	} else {
		/* client returned it! great. OR refused */
		TK_ASSERT(si->tksi_nrecalls > 0);
		if (--si->tksi_nrecalls == 0)
			*done = 1;

#if TK_METER
		if (tm) {
			char *rp = TKS_GET_METER(tm, class, trackcrecall);
			TK_ASSERT(rp);
			TK_ASSERT(btst(rp, cnum));
			bclr(rp, cnum);
		}
#endif
	}
	return bk;
}

/*
 * tks_return - return a set of tokens.
 * This is called when:
 * 1) responding to a server initiated conditional recall (tks_recall)
 * 2) responding to a server initiated mandatory recall (tks_obtain)
 * 3) processing a client initiated return
 *
 * Note that return operations are always permitted - whether the class is
 * locked or not.
 *
 * Note that a client initiated return always must have a token and
 * that the token must always still be valid (have a grant). This is
 * since if a return shows up at the client, and the client has already
 * started to return the token, an 'eh' response will be generated - so
 * the only token that could actually satisfy the return would be the
 * client one.
 *
 * Races - any one of the tokens returned here could be the last outstanding
 * token, and could allow another thread to legitimitely call tks_destroy.
 * We need some synchronization so that we can continue through this routine
 * and acquire/release locks etc w/o accessing freed memory. Note that
 * it is illegal to call tks_destroy if there are in fact any outstanding
 * tokens so the race is fairly simple to close with a reference count.
 *
 * Note - only TK_SERVER_CONDITIONAL tokens are permitted in the tref set
 *	unless we are processing an OOB error.
 */
/* ARGSUSED */
void
tks_return(
	tks_state_t *ss,	/* object containing token */
	tks_ch_t h,		/* client handle */
	tk_set_t tr,		/* tokens to be returned */
	tk_set_t tref,		/* tokens refused */
	tk_set_t teh,		/* tokens client didn't think it had */
	tk_disp_t why)		/* why returning */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts;
	tk_disp_t ds;
	bitnum_t cnum = (bitnum_t)tksi_getcnum(h);
	char *bp;
	tk_set_t torecall, tocall;
	tk_disp_t dofrecall, dofref;
	int i, bk, disp, level, class;
	int recalldone;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	TK_TRACE(TKS_SRETURN, __return_address, h, tr, tref, teh,
						si, TK_METER_VAR);
	TK_TRACE(TKS_SRETURN2, 0, h, TK_NULLSET, TK_NULLSET, why,
						si, TK_METER_VAR);
	TK_ATOMIC_ADD(&si->tksi_addref, 1);
	dofref = why;
	recalldone = 0;
	tocall = TK_NULLSET;
	for (ts = tr, class = 0, ds = why; ts;
			ts = ts >> TK_LWIDTH, ds = ds >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;

		TKS_INC_METER(tm, class, return);
		/*
		 * level could be up to 2 levels - could be returning
		 * SWRITE|READ and getting WRITE
		 */
		TK_ASSERT(level != 6 && level != 7 && level != 0);
		tstate = &si->tksi_state[class];
		bp = si->tksi_gbits + __tksi_goffset[class];
		disp = ds & TK_SET_MASK;
		TK_ASSERT(disp);
		TK_ASSERT((disp & disp - 1) == 0);

		TK_LOCK(si->tksi_lock);
		TK_ASSERT(disp != TK_SERVER_CONDITIONAL || tstate->svr_recall);
		TK_ASSERT(!(tstate->svr_recall && tstate->svr_revoke));

		/* make sure consistent revoke state */
		TK_ASSERT(!tstate->svr_rwait || tstate->svr_revoke);
		TK_ASSERT(disp != TK_SERVER_MANDATORY || tstate->svr_revoke);

		/*
		 * XXX is this ok for recall? the token client is sure
		 * to set the level to be what it really has but in general
		 * recall doesn't really talk about levels ..
		 */
		TK_ASSERT((level & tstate->svr_level) == level);
		TK_ASSERT(btst(bp, cnum));
		TK_ASSERT(tstate->svr_grants > 0);
		bclr(bp, cnum);
		if (disp == TK_SERVER_MANDATORY) {
#if TK_METER
			if (tm) {
				char *rp = TKS_GET_METER(tm, class, trackmrecall);
				TK_ASSERT(rp);
				TK_ASSERT(btst(rp, cnum));
				bclr(rp, cnum);
			}
#endif
			TK_ASSERT(tstate->svr_noutreqs > 0);
			tstate->svr_noutreqs--;
		} else if (disp == TK_SERVER_CONDITIONAL) {
			if (--si->tksi_nrecalls == 0)
				recalldone = 1;

#if TK_METER
			if (tm) {
				char *rp = TKS_GET_METER(tm, class, trackcrecall);
				TK_ASSERT(rp);
				TK_ASSERT(btst(rp, cnum));
				bclr(rp, cnum);
			}
#endif
		}
		if (--tstate->svr_grants == 0) {
			TK_ASSERT(bftstclr(bp, 0, tksi_maxcnum) == tksi_maxcnum);
			tstate->svr_level = 0;
			if (tstate->svr_noteidle) {
				tstate->svr_noteidle = 0;
				TK_COMBINE_SET(tocall, TK_MAKE(class, TK_READ));
			}

			/*
			 * we don't need complicated flags
			 * as for recall since
			 * mrecallall is waiting for a single token..
			 */
			if (disp == TK_SERVER_MANDATORY) {
				/*
				 * With positive Ack protocol - a client
				 * initiated return can't possibly be
				 * the last msg in
				 */

				if (!tstate->svr_revinp &&
				     tstate->svr_noutreqs == 0) {
					if (tstate->svr_rwait)
						tksi_listwake(&revokehead, si,
								class);
				}
			}
		} else {
			TK_ASSERT(tstate->svr_level != TK_WRITE);
		}
		TK_UNLOCK(si->tksi_lock);
	}

	/*
	 * now take care of 'unknown' tokens
	 */
	torecall = TK_NULLSET;
	dofrecall = TK_NULLSET;
	TK_LOCK(si->tksi_lock);
	for (ts = teh, class = 0, ds = why; ts;
			ts = ts >> TK_LWIDTH, ds = ds >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		disp = ds & TK_SET_MASK;
		TK_ASSERT(disp);
		TK_ASSERT((disp & disp - 1) == 0);
		if (disp == TK_SERVER_MANDATORY)
			bk = tksi_sr(si, cnum, class, level,
					&torecall, &dofrecall, 0);
		else if (disp == TK_SERVER_CONDITIONAL)
			bk = tksi_rr(si, cnum, class, level,
					&torecall, &dofrecall, &recalldone, 0);
		else
			TK_ASSERT(0);
	}
	TK_UNLOCK(si->tksi_lock);

	/*
	 * the result of going through the 'eh' tokens will be some to
	 * re-transmit or perhaps a sign that we are done
	 */
	if (torecall) {
		if (bk)
			tk_backoff(bk);
		TK_TRACE(TKS_RECALL_CALL, 0, h, torecall, TK_NULLSET,
					dofrecall, si, TK_METER_VAR);
		si->tksi_if->tks_recall(si->tksi_obj, h, torecall, dofrecall);
	}

	/*
	 * go through refused tokens.
	 */
	TK_LOCK(si->tksi_lock);
	for (ts = tref, class = 0, ds = dofref; ts;
			ts = ts >> TK_LWIDTH, ds = ds >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;
		disp = ds & TK_SET_MASK;
		TK_ASSERT(disp);
		TK_ASSERT((disp & disp - 1) == 0);
		if (disp == TK_SERVER_MANDATORY)
			bk = tksi_sr(si, cnum, class, level,
					NULL, NULL, 1);
		else if (disp == TK_SERVER_CONDITIONAL)
			bk = tksi_rr(si, cnum, class, level,
					NULL, NULL,
					&recalldone, 1);
		else
			TK_ASSERT(0);
	}
	TK_UNLOCK(si->tksi_lock);

	/*
	 * If idle callout needs to happen do so
	 */
	if (tocall) {
		TK_TRACE(TKS_IDLE_CALL, 0, 0, tocall, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);
		si->tksi_if->tks_idle(si->tksi_obj, tocall);
	}

	/*
	 * no longer need reference - if recalledone is set then there
	 * is no legitimate way that tks_destroy could have been called
	 * And if recalldone is not set, we NEVER touch 'si' again.
	 */
	TK_ASSERT(si->tksi_ref > 0);
	TK_ATOMIC_ADD(&si->tksi_addref, -1);

	/*
	 * we call (*tks_recalled) only when all recalled tokens have been
	 * dispositioned. We don't require a single call to do all that
	 */
	if (recalldone) {
		tk_set_t tdone = TK_NULLSET;
		tk_set_t tndone = TK_NULLSET;

		if (si->tksi_flags & TKS_SYNCRECALL) {
			TK_WAKERECALL(si);
			return;
		}
		tstate = &si->tksi_state[0];
		TK_LOCK(si->tksi_lock);
		for (i = 0; i < si->tksi_ntokens; i++, tstate++) {
			if (tstate->svr_recall) {
				if (tstate->svr_grants == 0)
					TK_COMBINE_SET(tdone, TK_MAKE(i, TK_READ));
				else
					TK_COMBINE_SET(tndone, TK_MAKE(i, TK_READ));
				tstate->svr_recall = 0;
				tstate->svr_lock = 0;
				if (tstate->svr_lockw)
					tksi_listwake(&classhead, si, i);
			}
#if TK_METER
			if (tm && TKS_FIELD_METER(tm, i, trackcrecall))
				TK_ASSERT( \
					bftstclr(TKS_FIELD_METER(tm,\
						i, trackcrecall), 0, \
						tksi_maxcnum) == tksi_maxcnum);
#endif
		}
		TK_ASSERT((tdone | tndone) != TK_NULLSET);

		si->tksi_flags &= ~(TKS_INRECALL);
		TK_UNLOCK(si->tksi_lock);
		TK_TRACE(TKS_RECALLED, 0, 0, tdone, tndone,
					TK_NULLSET, si, TK_METER_VAR);
		si->tksi_if->tks_recalled(si->tksi_obj, tdone, tndone);
		/* WARNING - entire tks may be torn down when we return! */
	}
}

/*
 * tks_recall - recall a set of tokens
 * This is used when the server object manager wishes to shut down
 * an object.
 * Note that NO locking is done - any client who knows
 * how can still acquire tokens.
 * This routine calls out to the (*tks_recall) callout for each
 * client that has the token AT THE TIME tks_recall() is called.
 * It then returns.
 * When a response from all clients arrive, the (*tks_recalled) callout
 * is called with the set of tokens that has been successfully recalled,
 * as well as the set which weren't successfully recalled.
 * The token sets passed to tks_recalled() are up to date - i.e.
 * if an obtain for a particular class came in during the recall operation,
 * the 'refused' set will contain that class..
 */
void
tks_recall(
	tks_state_t *ss,	/* object containing token */
	tk_set_t tset,		/* tokens to be recalled */
	tk_set_t *refused)	/* Tokens refused, if null recall is async */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts, tr;
	tk_disp_t dofrec;
	char *bp, *cp;
	int didrecalls = 0, class;
	tks_ch_t client;
	bitnum_t b, remain, bv;
	bitnum_t lastb = tksi_maxcnum;
	char *mp[TK_MAXTOKENS];
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	TK_LOCK(si->tksi_lock);
	TK_TRACE(TKS_SRECALL, __return_address, 0, tset, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);

	/* XXX later - just queue them */
	TK_ASSERT((si->tksi_flags & (TKS_INRECALL)) == 0);
	si->tksi_flags |= TKS_INRECALL;
	if (refused)
		si->tksi_flags |= TKS_SYNCRECALL;
	TK_ASSERT(si->tksi_nrecalls == 0);
	TK_UNLOCK(si->tksi_lock);

	/*
	 * go through and tag each token to be recalled - we must do
	 * this before actually doing any callouts since a callout
	 * could end up calling tks_return which must know
	 * whether all the tokens to be recalled are finished
	 * We keep track of each client that needs a message
	 * to speed things up in the real pass
	 *
	 * cp tracks all clients that need a msg
	 * mp tracks clients per token
	 */
	bzero(mp, sizeof(mp));
	cp = tksi_allocbm(1);

	TK_LOCK(si->tksi_lock);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((ts & TK_SET_MASK) == 0)
			continue;
		TK_ASSERT(class < si->tksi_ntokens);

		tstate = &si->tksi_state[class];
		TK_ASSERT(tstate->svr_lock == 0);
		TK_ASSERT(tstate->svr_lockw == 0);
		TK_ASSERT(tstate->svr_revoke == 0);

		if (tstate->svr_grants == 0)
			/* easy! */
			continue;

		/* alloc a bitmap for this class to track all clients */
		mp[class] = tksi_allocbm(1);

#if TK_METER
		if (tm)
			TKS_FIELD_METER(tm, class, recracecur) = 0;
#endif
		tstate->svr_recall = 1;
		tstate->svr_backoff = 0;
		bp = si->tksi_gbits + __tksi_goffset[class];
		/*
		 * walk through grant bitmap for this class
		 */
		b = 0;
		remain = lastb;
		while (b < lastb) {
			bv = bftstclr(bp, b, remain);
			b += bv;
			remain -= bv;

			/* b now points to set bit unless we're at end */
			if (b >= lastb)
				break;
			si->tksi_nrecalls++;
			tstate->svr_lock = 1;

			/* mark this client as needing a msg */
			bset(mp[class], b);
			bset(cp, b);

			/* advance past one we just did */
			remain--;
			b++;
		}
	}
	/*
	 * if there are any recalls going out then tks_return will be
	 * the one that ends up calling tks_recalled 
	 */
	if (si->tksi_nrecalls)
		didrecalls = 1;
	TK_UNLOCK(si->tksi_lock);

	/*
	 * Now that we've unlocked the token, returns, etc. can come in
	 * But we will send a msg to them regardless ...
	 */

	/*
	 * walk through client bitmap
	 */
	b = 0;
	remain = lastb;
	while (b < lastb) {
		bv = bftstclr(cp, b, remain);
		b += bv;
		remain -= bv;

		/* b now points to set bit unless we're at end */
		if (b >= lastb)
			break;

		tr = TK_NULLSET;
		dofrec = TK_NULLSET;
		client = tksi_getch((int)b);

		/*
		 * go through class bitmaps to see if this client should
		 * get a message
		 */
		for (class = 0; class < TK_MAXTOKENS; class++) {
			if (mp[class] == 0)
				continue;
			if (!btst(mp[class], b))
				continue;
#if TK_METER
			if (tm) {
				/*
				 * for debugging - track who we've sent
				 * messages to
				 */
				char *rp;
				rp = TKS_GET_METER(tm, class, trackcrecall);
				if (rp == 0) {
					rp = tksi_allocbm(1);
					TKS_FIELD_METER(tm, class, trackcrecall) = rp;
				}
				TK_ASSERT(!btst(rp, b));
				/*
				 * alas, we need to lock since a previous
				 * recall-callout could be in tksi_rr
				 * turning off a different bit.
				 */
				TK_LOCK(si->tksi_lock);
				bset(rp, b);
				TK_UNLOCK(si->tksi_lock);
			}
#endif

			TK_COMBINE_SET(tr,
				TK_MAKE(class, TK_GET_LEVEL(tset, class)));
			TK_COMBINE_SET(dofrec,
				TK_MAKE(class, TK_SERVER_CONDITIONAL));
		}
		TK_ASSERT(tr);

		TK_TRACE(TKS_RECALL_CALL, 0, client, tr, TK_NULLSET,
					dofrec, si, TK_METER_VAR);
		/*
		 * note that this could be the last recall to be sent
		 * out and could immediately turn around and call
		 * tks_return which would then call the (*tks_recalled)
		 * function, thus effectively completing the recall
		 */
		si->tksi_if->tks_recall(si->tksi_obj, client, tr, dofrec);

		/* advance past one we just did */
		remain--;
		b++;
	}
	/*
	 * NOTE: unless didrecalls is 0, the recall might already
	 * have completed and all our memory de-allocated. So from here on
	 * out NO touching si...(and therefore no tracing unless !didrecalls)
	 */
	for (class = 0; class < TK_MAXTOKENS; class++)
		if (mp[class])
			tksi_freebm(mp[class]);
	tksi_freebm(cp);

	if (!didrecalls) {
		/* easy! */
		tk_set_t tdone = tset;
		TK_TRACE(TKS_ERECALL, 0, 0, TK_NULLSET, TK_NULLSET, TK_NULLSET,
					si, TK_METER_VAR);

		TK_LOCK(si->tksi_lock);
		si->tksi_flags &= ~TKS_INRECALL;
		if (refused)
			si->tksi_flags &= ~TKS_SYNCRECALL;
		TK_UNLOCK(si->tksi_lock);

		TK_ASSERT(si->tksi_nrecalls == 0);
		if (refused) {
			*refused = TK_NULLSET;
			return;
		}
		TK_TRACE(TKS_RECALLED, 0, 0, tdone, TK_NULLSET,
				TK_NULLSET, si, TK_METER_VAR);
		si->tksi_if->tks_recalled(si->tksi_obj, tdone, TK_NULLSET);
	} else if (refused) {
		tk_set_t tndone = TK_NULLSET;
		int	i;

		TK_LOCK(si->tksi_lock);
		while (si->tksi_nrecalls > 0) {
			TK_WAITRECALL(si);
		}
		tstate = &si->tksi_state[0];
		for (i = 0; i < si->tksi_ntokens; i++, tstate++) {
			if (tstate->svr_recall) {
				if (tstate->svr_grants != 0)
					TK_COMBINE_SET(tndone,
							TK_MAKE(i, TK_READ));
				tstate->svr_recall = 0;
				tstate->svr_lock = 0;
				if (tstate->svr_lockw)
					tksi_listwake(&classhead, si, i);
			}
		}
		si->tksi_flags &= ~(TKS_INRECALL|TKS_SYNCRECALL);
		TK_UNLOCK(si->tksi_lock);
		*refused = tndone;
		/* return */
	}
}

/*
 * tks_iterate - call specified callout for each client that has
 * any of the specified tokens
 *
 * Unlike recall, we're not quite as concerned about atomic snapshots.
 * We do, in an atomic fashion, record all clients that have one of the passed
 * token classes. But then we releases the lock, so its possible that
 * the client has dropped the token or is in the process of dropping the token.
 */
tks_iter_t
tks_iterate(
	tks_state_t *ss,	/* object containing token */
	tk_set_t tset,		/* tokens to search for */
	int flag,		/* flag */
	tks_iter_t (*func)(
		void *obj,
		tks_ch_t h,	/* client handle */
		tk_set_t tset,	/* tokens client owns */
		va_list ap),	/* args */
	...)			/* args */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts, tr;
	char *bp, *cp;
	int restart, class, stillclients;
	tks_iter_t rv;
	tks_ch_t h;
	bitnum_t b, remain, bv;
	bitnum_t lastb = tksi_maxcnum;
	int stable = flag & TKS_STABLE;
	va_list ap;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	va_start(ap, func);

	TK_LOCK(si->tksi_lock);
	TK_TRACE(TKS_ITERATE, __return_address, 0, tset, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);

	if (stable)
		TK_ASSERT(si->tksi_nrecalls == 0);
	TK_UNLOCK(si->tksi_lock);

	/*
	 * go through and tag each token to be iterated on
	 * cp tracks all clients that need a msg
	 */
	cp = tksi_allocbm(1);

	TK_LOCK(si->tksi_lock);
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((ts & TK_SET_MASK) == 0)
			continue;

		tstate = &si->tksi_state[class];
		if (stable) {
			TK_ASSERT(tstate->svr_lock == 0);
			TK_ASSERT(tstate->svr_lockw == 0);
			TK_ASSERT(tstate->svr_revoke == 0);
		}

		if (tstate->svr_grants == 0)
			/* easy! */
			continue;

		bp = si->tksi_gbits + __tksi_goffset[class];
		/*
		 * walk through grant bitmap for this class
		 */
		b = 0;
		remain = lastb;
		while (b < lastb) {
			bv = bftstclr(bp, b, remain);
			b += bv;
			remain -= bv;

			/* b now points to set bit unless we're at end */
			if (b >= lastb)
				break;

			/* mark this client as needing a msg */
			bset(cp, b);

			/* advance past one we just did */
			remain--;
			b++;
		}
	}
	TK_UNLOCK(si->tksi_lock);

	/*
	 * walk through client bitmap
	 */
	restart = 1;
	rv = TKS_CONTINUE;
	stillclients = 1;
	while (rv != TKS_STOP && stillclients) {
		if (restart) {
			b = 0;
			remain = lastb;
			restart = 0;
			stillclients = 0;
		}
		bv = bftstclr(cp, b, remain);
		b += bv;
		remain -= bv;

		/* b now points to set bit unless we're at end */
		if (b >= lastb) {
			restart = 1;
			continue;
		}

		h = tksi_getch((int)b);
		stillclients = 1;

		/*
		 * go through class bitmaps to see if this client should
		 * get a message
		 */
		tr = TK_NULLSET;
		mrlock(&tks_iterate_barrier[h], MR_ACCESS, PZERO);
		for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
			if ((ts & TK_SET_MASK) == 0)
				continue;
			if (!btst(si->tksi_gbits + __tksi_goffset[class], b))
				continue;

			TK_COMBINE_SET(tr, TK_MAKE(class, ts & TK_SET_MASK));
		}

		TK_TRACE(TKS_ITERATE_CALL, 0, h, tr, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);

		
		rv = (*func)(si->tksi_obj, h, tr, ap);
		mrunlock(&tks_iterate_barrier[h]);
		if (rv != TKS_RETRY) {
			/* client is done */
			bclr(cp, b);
		}

		/* advance past one we just did */
		remain--;
		b++;
		if (b >= lastb)
			restart = 1;
	}
	tksi_freebm(cp);

	return rv;
}

/*
 * tks_cleancell - For the passed in cell, clear any grants it has
 *	corresponding to the passed in tk_set_t.
 *	This routine handles 'returning' the token if it has an outstanding
 *	recall, thus the (*recalled) callout or (*obtain) could
 *	be called as the result of this call.
 *
 * Note: it is assumed that the selected cell is down and that NO more
 * 	messages from that cell will be coming in and that any
 *	attempts to send a message will result in an immediate error.
 *
 * Note: there are no interlocks between this and tks_destroy.
 *
 * For this call, it is permissible to pass multiple levels per class
 */
void
tks_cleancell(
	tks_state_t *ss,	/* object containing token */
	tks_ch_t h,		/* handle for client to clean */
	tk_set_t tw,		/* tokens to clean */
	tk_set_t *tc,		/* tokens cleaned */
	int flags)		/* flags */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tk_set_t ts, tstoret, tstoref;
	tk_disp_t dofts;
	bitnum_t cnum = (bitnum_t)tksi_getcnum(h);
	char *bp;
	int level, class;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	TK_TRACE(TKS_SCLEAN, __return_address, h, tw, TK_NULLSET, TK_NULLSET,
					si, TK_METER_VAR);

	dofts = TK_NULLSET;
	tstoret = TK_NULLSET;
	tstoref = TK_NULLSET;
	TK_LOCK(si->tksi_lock);
	for (ts = tw, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((level = ts & TK_SET_MASK) == 0)
			continue;

		tstate = &si->tksi_state[class];
		bp = si->tksi_gbits + __tksi_goffset[class];

		if (!btst(bp, cnum)) {
			/* client doesn't have it! */
			continue;
		}

		if (!(level & tstate->svr_level)) {
			/* client has it but not at level requested */
			continue;
		}

		TK_ASSERT(tstate->svr_level != 0);
		TK_ASSERT((tstate->svr_level & tstate->svr_level - 1) == 0);

		if (flags & TKS_CHECK) {
			TK_COMBINE_SET(tstoret, TK_MAKE(class, tstate->svr_level));
		} else if (tstate->svr_recall) {
			/*
			 * recalls always involve ALL grantees so
			 * we know that it's reasonable to call tks_return()
			 * We set the tokens as 'refused' to be recalled
			 * XXX must sync with an inprogress recall...
			 */
			TK_COMBINE_SET(dofts, TK_MAKE(class, TK_SERVER_CONDITIONAL));
			TK_COMBINE_SET(tstoref, TK_MAKE(class, tstate->svr_level));
		} else if (tstate->svr_revoke) {
			/*
			 * XXX sync with inprogress revoke - really
			 * need to make sure svr_revinp is 0 before
			 * proceeding
			 */
			TK_ASSERT(tstate->svr_revinp == 0);
			TK_COMBINE_SET(dofts, TK_MAKE(class, TK_SERVER_MANDATORY));
			TK_COMBINE_SET(tstoref, TK_MAKE(class, tstate->svr_level));
		} else {
			/*
			 * not being revoked or recalled - return as if
			 * client asked us to
			 */
			TK_COMBINE_SET(dofts, TK_MAKE(class, TK_CLIENT_INITIATED));
			TK_COMBINE_SET(tstoret, TK_MAKE(class, tstate->svr_level));
		}
	}
	TK_UNLOCK(si->tksi_lock);
	if ((tstoret || tstoref) && !(flags & TKS_CHECK))
		tks_return(ss, h, tstoret, tstoref, TK_NULLSET, dofts);

	TK_TRACE(TKS_ECLEAN, 0, h, tstoret, tstoref, dofts,
					si, TK_METER_VAR);
	*tc = TK_ADD_SET(tstoret, tstoref);
	return;
}

/*
 * tks_notify_idle
 * Set up to call (*tks_idle) if grant count goes to zero
 */
void
tks_notify_idle(
	tks_state_t *ss,	/* object containing token */
	tk_set_t tset)		/* tokens (classes) to be notified */
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	int class;
	tk_set_t ts, tocall;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, ss);
	tocall = TK_NULLSET;

	TK_LOCK(si->tksi_lock);
	TK_TRACE(TKS_NOTIFY_IDLE, __return_address, 0, tset, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);

	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((ts & TK_SET_MASK) == 0)
			continue;

		tstate = &si->tksi_state[class];
		if (tstate->svr_grants == 0) {
			TK_ASSERT(tstate->svr_level == 0);
			TK_COMBINE_SET(tocall, TK_MAKE(class, TK_READ));
		} else {
			tstate->svr_noteidle = 1;
		}
	}
	TK_UNLOCK(si->tksi_lock);

	/* callout now for any that are already idle */
	if (tocall) {
		TK_TRACE(TKS_IDLE_CALL, 0, 0, tocall, TK_NULLSET,
					TK_NULLSET, si, TK_METER_VAR);
		si->tksi_if->tks_idle(si->tksi_obj, tocall);
	}
}

void
tks_state(
	tks_state_t	*ss,	/* object containing tokens */
	tks_ch_t	client, /* client we are interested in */
	void		(*func)(
				void		*obj,
				tks_ch_t	client,
				tk_set_t	grants,
				tk_set_t	recalls))
{
	tksi_sstate_t	*si;
	tksi_svrstate_t	*tstate;
	int		class;
	tk_set_t	granted;
	tk_set_t	recalled;
	char		needunlock[TK_MAXTOKENS];
	char		*bitmap;
	TK_LOCKDECL;

	si = (tksi_sstate_t *)ss;
	granted = TK_NULLSET;
	recalled = TK_NULLSET;
	TK_LOCK(si->tksi_lock);
	for (class = 0; class < si->tksi_ntokens; class++) {
		tstate = &si->tksi_state[class];
		if (tstate->svr_grants == 0) {
			needunlock[class] = 0;
			TK_ASSERT(tstate->svr_level == 0);
			continue;
		}
		bitmap = si->tksi_gbits + __tksi_goffset[class];
		if (!btst(bitmap, client)) {
			/* Client doesn't have it */
			needunlock[class] = 0;
			continue;
		}
		ASSERT(tstate->svr_lock == 0);
		tstate->svr_lock = 1;
		needunlock[class] = 1;
		if (tstate->svr_revoke || tstate->svr_recall)
			TK_COMBINE_SET(recalled,
					TK_MAKE(class, tstate->svr_level));
		else
			TK_COMBINE_SET(granted,
					TK_MAKE(class, tstate->svr_level));
	}
	TK_UNLOCK(si->tksi_lock);
	if (granted == TK_NULLSET && recalled == TK_NULLSET)
		return;
	(*func)(si->tksi_obj, client, granted, recalled);
	TK_LOCK(si->tksi_lock);
	for (class = 0; class < si->tksi_ntokens; class++) {
		if (needunlock[class]) {
			tstate = &si->tksi_state[class];
			tstate->svr_lock = 0;
			if (tstate->svr_lockw)
				tksi_listwake(&classhead, si, class);
		}
	}
	TK_UNLOCK(si->tksi_lock);
}

/*
 * tks_bag - return total state of the token service for
 * the associated object. An object bag is supplied. This is loadedwith
 * with the token state of all tokens and the current clients.
 * It is assumed that the object is in a quiesced state when this
 * is called so that the state is frozen. This routine is used during
 * token server migration.
 */
void
tks_bag(
	tks_state_t	*ss,		/* object to be migrated */
	obj_bag_t	bag)		/* object bag to use */
{
	tksi_sstate_t	*si = (tksi_sstate_t *)ss;
	int		ntokens = si->tksi_ntokens;
	
	obj_bag_put(bag, OBJ_TAG_TKS_NTOKENS,
		    &si->tksi_ntokens, sizeof(si->tksi_ntokens)); 
	obj_bag_put(bag, OBJ_TAG_TKS_GBITS,
		    si->tksi_gbits, tksi_sizebm(ntokens)); 
	obj_bag_put(bag, OBJ_TAG_TKS_STATE,
		    &si->tksi_state[0], ntokens * sizeof(tksi_svrstate_t));

}

void
tks_free(
	tks_state_t	*ss)	/* object to free */
{
	tksi_sstate_t	*si = (tksi_sstate_t *)ss;
	int		ntokens = si->tksi_ntokens;

	bzero(&si->tksi_state[0], ntokens * (int)sizeof(tksi_svrstate_t));
	tksi_zerobm(si->tksi_gbits, ntokens);
	tks_destroy(ss);
}

/*
 * tks_unbag - re-establish token server state for an object from
 * bagged data.
 */
int
tks_unbag(
	tks_state_t	*ss,		/* object to be migrated */
	obj_bag_t	bag)		/* bag containing bits */
{
	tksi_sstate_t	*si = (tksi_sstate_t *)ss;
	uchar_t		ntokens;
	int		error;

	obj_bag_get_here(bag, OBJ_TAG_TKS_NTOKENS,
			 &ntokens, sizeof(ntokens), error);
	if (error)
		return error;
	ASSERT(ntokens == si->tksi_ntokens);

	obj_bag_get_here(bag, OBJ_TAG_TKS_GBITS,
			 si->tksi_gbits, tksi_sizebm(ntokens), error);
	if (error)
		return error;

	obj_bag_get_here(bag, OBJ_TAG_TKS_STATE,
			 &si->tksi_state[0], ntokens * sizeof(tksi_svrstate_t),
			 error);
	return error;
}

/*
 * tks_isclient - return whether a cell is a client of an object.
 * This is determined from the grant map for the existence token.
 * It is assumed that the existence token is token 0.
 */
int
tks_isclient(
        tks_state_t	*ss,		/* object of interest */
	tks_ch_t	cell)
{
	tksi_sstate_t	*si = (tksi_sstate_t *)ss;
	return btst(si->tksi_gbits, cell);
}

/*
 * Server utilities
 */

/*
 * tksi_listwait - wait until list goes unlocked
 * calles with 'si' lock held.
 */
/* ARGSUSED */
static void
tksi_listwait(tk_list_t *head, tksi_sstate_t *si, int class, int *l)
{
	tksi_svrstate_t *tstate;
	tk_list_t *tl;
	TK_LOCKDECL;

	tstate = &si->tksi_state[class];
	if ((head == &revokehead && tstate->svr_rwait == 1) ||
	    (head != &revokehead && tstate->svr_lockw == 1)) {
		/* someone already waiting - queue up on that sema */
		TK_LOCK(head->tk_lock);
		for (tl = head->tk_next; tl != head; tl = tl->tk_next) {
			if (tl->tk_tag1 == (void *)si && tl->tk_tag2 == class)
				break;
		}
		TK_ASSERT(tl != head);

		TK_ASSERT(tl->tk_nwait > 0);
		tl->tk_nwait++;
		TK_UNLOCK(head->tk_lock);
	} else {
		if (head == &revokehead)
			tstate->svr_rwait = 1;
		else
			tstate->svr_lockw = 1;
		/* XXX don't malloc with lock held */
		tl = TK_NODEMALLOC(sizeof(*tl));
		tl->tk_tag1 = (void *)si;
		tl->tk_tag2 = class;
		tl->tk_nwait = 1;
		TK_CREATE_SYNC(tl->tk_wait);
		/* link onto global class wait list */
		TK_LOCK(head->tk_lock);
		tl->tk_next = head->tk_next;
		tl->tk_prev = head;
		head->tk_next->tk_prev = tl;
		head->tk_next = tl;
		TK_UNLOCK(head->tk_lock);
	}

	TK_UNLOCKVAR(si->tksi_lock, *l);

	/* wait */
	TK_SYNC(tl->tk_wait);

	TK_LOCK(head->tk_lock);
	TK_ASSERT(tl->tk_nwait > 0);
	tl->tk_nwait--;
	if (tl->tk_nwait == 0) {
		TK_DESTROY_SYNC(tl->tk_wait);
		TK_NODEFREE(tl);
	}
	TK_UNLOCK(head->tk_lock);
}

/*
 * tksi_listwake - wake threads waiting for list lock
 */
static void
tksi_listwake(tk_list_t *head, tksi_sstate_t *si, int class)
{
	tksi_svrstate_t *tstate;
	tk_list_t *tl;
	int i;
	TK_LOCKDECL;

	tstate = &si->tksi_state[class];
#ifdef TK_DEBUG
	if (head == &revokehead)
		TK_ASSERT(tstate->svr_rwait == 1);
	else
		TK_ASSERT(tstate->svr_lockw == 1);
#endif
	TK_LOCK(head->tk_lock);
	for (tl = head->tk_next; tl != head; tl = tl->tk_next) {
		if (tl->tk_tag1 == (void *)si && tl->tk_tag2 == class)
			break;
	}
	TK_ASSERT(tl != head);

	TK_ASSERT(tl->tk_nwait > 0);
	/* remove from list */
	tl->tk_next->tk_prev = tl->tk_prev;
	tl->tk_prev->tk_next = tl->tk_next;
	i = tl->tk_nwait;
	if (head == &revokehead)
		tstate->svr_rwait = 0;
	else
		tstate->svr_lockw = 0;
	while (i--)
		TK_WAKE(tl->tk_wait);
	TK_UNLOCK(head->tk_lock);
}

#if !_KERNEL
/*
 * convert from client handle to client number and back
 */
static int
tksi_getcnum(tks_ch_t h)
{
	static int acnum = 0;
	tksi_chlist_t *p;
	TK_LOCKDECL;

	TK_LOCK(chlock);
	for (p = chhead.next; p; p = p->next) {
		if (p->handle == h)
			break;
	}

	if (p == NULL) {
		/* alloc a new one */
		p = TK_NODEMALLOC(sizeof(*p));
		p->cnum = acnum++;
		p->handle = h;
		p->next = chhead.next;
		chhead.next = p;
	}
	TK_UNLOCK(chlock);
	TK_ASSERT(p->cnum < MAXCNUM);
	return p->cnum;
}

static tks_ch_t
tksi_getch(int cnum)
{
	tksi_chlist_t *p;
	TK_LOCKDECL;

	TK_LOCK(chlock);
	for (p = chhead.next; p; p = p->next) {
		if (p->cnum == cnum)
			break;
	}
	TK_UNLOCK(chlock);
	TK_ASSERT(p);

	return p->handle;
}
#endif /* !_KERNEL */

/*
 * tksi_mrecallall - loop through all clients that have the token and
 * perform the mandatory recall callback
 * We don't need any locks since there is a bitmap per class, and class
 * is locked so no more grants can occur and we walk through the map just once
 */
/* ARGSUSED */
static void
tksi_mrecallall(tksi_sstate_t *si, tks_ch_t h, int class, int level)
{
	tk_set_t ts = TK_MAKE(class, level);
	tk_disp_t dofset = TK_MAKE(class, TK_SERVER_MANDATORY);
	tksi_svrstate_t *tstate;
	TK_METER_DECL /* decls 'tm' */
	tks_ch_t th;
	char *bp;
	bitnum_t b, remain, bv;
	bitnum_t lastb = tksi_maxcnum;
	TK_LOCKDECL;

	TKS_METER_VAR_INIT(tm, si);
	tstate = &si->tksi_state[class];
	bp = si->tksi_gbits + __tksi_goffset[class];
	b = 0;
	remain = lastb;
	TK_ASSERT(tstate->svr_noutreqs == 0);
	TK_ASSERT(tstate->svr_revinp == 1);
	while (b < lastb) {
		bv = bftstclr(bp, b, remain);
		b += bv;
		/* b now points to set bit unless we're at end */
		if (b >= lastb)
			break;
		th = tksi_getch((int)b);
		/*
		 * with the positive acknowledge protocol,
		 * there should be NO way that a revoke needs to
		 * go to the client that originated the request.
		 * We can now assert that this doesn't happen - the only
		 * way it will is if a client violates the return/revoke
		 * protocol.
		 */
		TK_ASSERT(th != h);

		/* XXX atomic op should work just fine */
		TK_LOCK(si->tksi_lock);
		tstate->svr_noutreqs++;
		TK_UNLOCK(si->tksi_lock);
#if TK_METER
		if (tm) {
			/*
			 * for debugging - track who we've sent
			 * messages to
			 */
			char *rp;
			rp = TKS_GET_METER(tm, class, trackmrecall);
			if (rp == 0) {
				rp = tksi_allocbm(1);
				TKS_FIELD_METER(tm, class, trackmrecall) = rp;
			}
			TK_ASSERT(!btst(rp, b));
			/*
			 * alas, we need to lock since a previous
			 * revoke-callout could be in tksi_sr
			 * turning off a different bit.
			 */
			TK_LOCK(si->tksi_lock);
			bset(rp, b);
			TK_UNLOCK(si->tksi_lock);
		}
#endif
		TKS_INC_METER(tm, class, mrecall);

		TK_TRACE(TKS_RECALL_CALL, 0, th, ts, TK_NULLSET,
					dofset, si, TK_METER_VAR);
		si->tksi_if->tks_recall(si->tksi_obj, th, ts, dofset);

		remain -= bv;
		/* advance past one we just did */
		remain--;
		b++;
	}
	TK_LOCK(si->tksi_lock);
	TK_ASSERT(tstate->svr_revinp);
	tstate->svr_revinp = 0; /* let wakeups come in */

	while (tstate->svr_noutreqs > 0) {
		tksi_listwait(&revokehead, si, class, TK_LOCKVARADDR);
		TK_LOCK(si->tksi_lock);
	}
#if TK_METER
	if (tm && TKS_FIELD_METER(tm, class, trackmrecall))
		TK_ASSERT(bftstclr(TKS_FIELD_METER(tm, class, trackmrecall), \
					0, tksi_maxcnum) == tksi_maxcnum);
#endif
	TK_ASSERT(tstate->svr_grants == 0);
	TK_ASSERT(tstate->svr_revoke);
	tstate->svr_revoke = 0;
	tstate->svr_backoff = 0;
	/* wake up potential folks in tksi_return */
	if (tstate->svr_rwait)
		tksi_listwake(&revokehead, si, class);
	TK_UNLOCK(si->tksi_lock);
	return;
}

#ifdef _KERNEL
#define OUTPUT		qprintf(
void
tks_print(tks_state_t *ss, char *fmt, ...)

#else

#define OUTPUT		fprintf(f,
void
tks_print(tks_state_t *ss, FILE *f, char *fmt, ...)
#endif
{
	tksi_sstate_t *si = (tksi_sstate_t *)ss;
	tksi_svrstate_t *tstate;
	int i;
	va_list ap;
	char buf[512], *p;

	p = buf;
	if (fmt) {
		va_start(ap, fmt);
		vsprintf(p, fmt, ap);
		p += strlen(p);
	}
	p = __tk_sprintf(p, "obj 0x%x nrecalls %d ntokens %d flags 0x%x ref %d origcell %d\n", si->tksi_obj,
			si->tksi_nrecalls, si->tksi_ntokens, si->tksi_flags,
			si->tksi_ref, si->tksi_origcell);
	OUTPUT buf);
	for (i = 0; i < si->tksi_ntokens; i++) {
		tstate = &si->tksi_state[i];
		p = buf;
#if _KERNEL
		p = __tk_sprintf(p, "\tclass %w32d:level %s rwait %w32d idle %w32d lock %w32d lockw %w32d grants %w32d\n",
#else
		p = __tk_sprintf(p, "\tclass %d:level %s rwait %d idle %d lock %d lockw %d grants %d\n",
#endif
				i,
				__tk_levtab[tstate->svr_level],
				tstate->svr_rwait,
				tstate->svr_noteidle,
				tstate->svr_lock,
				tstate->svr_lockw,
				tstate->svr_grants);
		p = __tk_sprintf(p, "\t grants@0x%x crecall %d mrecall %d revinp %d bckoff %d noutreqs %d\n",
				si->tksi_gbits,
				tstate->svr_recall,
				tstate->svr_revoke,
				tstate->svr_revinp,
				tstate->svr_backoff,
				tstate->svr_noutreqs);
		p = __tksi_prbits(p, si->tksi_gbits + __tksi_goffset_intr[i],
				tksi_maxcnum, "\t grants at cells:", "\n");
		OUTPUT buf);
	}
#if TK_METER
	if (si->tksi_flags & TKS_METER) {
		tk_meter_t *tm = tks_meter(ss);
		OUTPUT " meter @0x%p tag 0x%lx name \"%s\"\n",
				tm, tm->tm_tag, tm->tm_name);
		for (i = 0; i < si->tksi_ntokens; i++) {
			char *rp;
			p = buf;
			p = __tk_sprintf(p, " Cl:%d obtain %d return %d revrace %d mrecall %d\n",
				i,
				TKS_GET_METER(tm, i, obtain),
				TKS_GET_METER(tm, i, return),
				TKS_GET_METER(tm, i, revrace),
				TKS_GET_METER(tm, i, mrecall));
			p = __tk_sprintf(p, "   recrace %d\n",
				TKS_GET_METER(tm, i, recrace));
				
			if ((rp = TKS_GET_METER(tm, i, trackcrecall)))
				p = __tksi_prbits(p, rp,
					tksi_maxcnum, "\t conditional recalls:", "\n");
			if ((rp = TKS_GET_METER(tm, i, trackmrecall)))
				p = __tksi_prbits(p, rp,
					tksi_maxcnum, "\t mandatory recalls:", "\n");
			OUTPUT buf);
		}

	}
#endif
}

char *
__tksi_prbits(char *p, char *bp, bitnum_t lastb, char *pref, char *suff)
{
	int havebits;
	bitnum_t b, remain, bv;

	havebits = 0;
	b = 0;
	remain = lastb;
	while (b < lastb) {
		bv = bftstclr(bp, b, remain);
		b += bv;
		/* b now points to set bit unless we're at end */
		if (b >= lastb)
			break;
		if (havebits == 0) {
			p = __tk_sprintf(p, pref);
			havebits = 1;
		}
		p = __tk_sprintf(p, "%d ", tksi_getch((int)b));
		remain -= bv;
		/* advance past one we just did */
		remain--;
		b++;
	}
	if (havebits && suff)
		p = __tk_sprintf(p, suff);
	return p;
}

/*
 * tksi_allocbm - allocate bitmap for tksi_maxcnum clients
 */
static char *
tksi_allocbm(int nmaps)
{
	char *bp;
	bp = TK_NODEMALLOC(tksi_sizebm(nmaps));
	bzero(bp, tksi_sizebm(nmaps));
	return bp;
}

static void
tksi_freebm(char *bp)
{
	TK_NODEFREE(bp);
}

static void
tksi_zerobm(char *bp, int nmaps)
{
	bzero(bp, tksi_sizebm(nmaps)); 
}

static void
tksi_initbm(void)
{
}

/*
 * tks_make_dispall:
 * Create a disposition for all tokens in tset. The disposition is 
 * passed in disp.
 */
void
tks_make_dispall(tk_set_t tset, int disp, tk_disp_t *tk_disp)
{
	tk_set_t ts;
	tk_disp_t ds; /* For speed do it locally and then assign to tk_disp */
	int	class;

	
	ds = TK_NULLSET;
	for (ts = tset, class = 0; ts; ts = ts >> TK_LWIDTH, class++) {
		if ((ts & TK_SET_MASK) == 0)
			continue;
		ds = TK_ADD_DISP(ds, TK_MAKE_DISP(class, disp));
	}	
	*tk_disp = ds;
}

/*
 * tks_iterate_sync:
 * Wait for any pending tks_iterates to complete for a given cell.
 * Called by the recovery thread.
 */
void
tks_iterate_wait(cell_t cell)
{
	mrlock(&tks_iterate_barrier[cell], MR_UPDATE, PZERO);
	mrunlock(&tks_iterate_barrier[cell]);
}	
