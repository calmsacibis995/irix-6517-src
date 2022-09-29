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
#ifndef __TK_H__
#define __TK_H__

#ident "$Id: tkm.h,v 1.35 1997/02/19 22:31:17 cp Exp $"

#ifndef	CELL
#error included by non-CELL configuration
#endif

#include "sgidefs.h"
#include "stdarg.h"
#ifndef _KERNEL
#include "stdio.h"
#else
#include "ksys/object.h"
#endif

/*
 * Public interface for token management - both client and server side.
 *
 * Naming convention:
 *	tkc_ client calls
 *	tks_ server calls
 *
 * Each token controlled object has a set of up to 10 tokens associated with it.
 * The state for the client set of tokens is maintained in the tkc_state_t
 * data structure. This structure is allocated by the client using
 * the TKC_DECL macro.
 * Most operations take token sets (tk_set_t) and disposition sets (tk_disp_t)
 * thus permitting operations on a number of individual tokens (classes)
 * at once.
 */

/*
 * Token sets & levels and manipulation macros
 */
#define TK_MAXTOKENS	10	/* max tokens per object */
#define TK_READ		1	/* shared read */
#define TK_WRITE	2	/* exclusive write */
#define TK_SWRITE	4	/* shared write */
#define TK_LWIDTH	3	/* 3 bits */
typedef __uint32_t tk_set_t;

/* TK_MAKE is the basic macro for constructing
 * token sets. It can be used for both static/runtime construction.
 * Token sets can be constructed using:
 * TK_ADD_SET - for static / runtime initialization
 * TK_SUB_SET - for static / runtime initialization
 * TK_COMBINE_SET for runtime use
 * TK_DIFF_SET for runtime use
 *
 * One may assume that token sets are assignable.
 */
#define TK_MAKE(c,l)		((l) << ((c) * TK_LWIDTH))
#define TK_ADD_SET(t, t2)	((t) | (t2))
#define TK_SUB_SET(t, t2)	((t) & ~(t2))
#define TK_NULLSET		(0)
#define TK_IS_IN_SET(t, t2)	((t) & (t2))
#define TK_INTERSECT_SET(t, t2)	((t) & (t2))
#define TK_COMBINE_SET(t1, t2)	((t1) |= (t2))
#define	TK_DIFF_SET(t, t2)	((t) &= ~(t2))
#define TK_GET_CLASS(t, c)	(((t) >> ((c) * TK_LWIDTH)) & 0x7)
#define TK_GET_LEVEL(t, c)	(((t) >> ((c) * TK_LWIDTH)) & 0x7)

/*
 * all tokens all levels - useful when calling tkc_returning and
 * want simply to 'send them all back'
 */
#define TK_SET_ALL		0x3fffffff

/*
 * token singleton set
 */
typedef __uint32_t tk_singleton_t;

#define TK_MAKE_SINGLETON(c,l)	(l | ((c) << TK_LWIDTH) | 0x80000000)
/*
 * various flag values
 */
#define TK_WAIT			1	/* tkc_returning */

/*
 * disposition of tokens - used as the 'why' in (*tkc_return), (*tks_recall),
 * tks_return(), tkc_recall().
 * There are 3 reasons a token can be returned: CLIENT_INITIATED,
 * SERVER_CONDITIONAL, and SERVER_MANDATORY.
 * These must be constructed using the macros below.
 * In general, tk_disp_t's are passed from the token module out to the
 * various callouts and need to be passed to the server via an ipc/rpc.
 * In a few cases, a disposition set needs to be constructed - in many
 * of these cases, simply passing TK_DISP_CLIENT_ALL works well since
 * the token module doesn't mind having extra disposition bits on.
 */
typedef __uint32_t tk_disp_t;

/*
 * reasons for 'why' a token is being returned. These values should
 * be used in the macros below to construct a tk_disp_t
 */
#define TK_SERVER_CONDITIONAL	0x1	/* server asking for token */
#define TK_SERVER_MANDATORY	0x2	/* server demanding token */
#define TK_CLIENT_INITIATED	0x4	/* client initiated
					 * return (via tkc_recall()),
					 * return (via tkc_returning()),
					 * return (via *(tkc_obtain))
					 * return (via tkc_obtaining())
					 */
#define TK_MAKE_DISP(c,l)	((l) << ((c) * TK_LWIDTH))
#define TK_ADD_DISP(t, t2)	((t) | (t2))
#define TK_SUB_DISP(t, t2)	((t) & ~(t2))

#define TK_DISP_COND_ALL	01111111111
#define TK_DISP_MAND_ALL	02222222222
#define TK_DISP_CLIENT_ALL	04444444444

/*
 * these macros tell whether a disposition set has any of a particular kind
 */
#define TK_DISP_ANY_CONDITIONAL(td)	((td) & TK_DISP_COND_ALL)
#define TK_DISP_ANY_MANDATORY(td)	((td) & TK_DISP_MAND_ALL)
#define TK_DISP_ANY_CLIENT(td)		((td) & TK_DISP_CLIENT_ALL)

/*
 * Client object state - this data structure is present in each object
 * It is totally opaque to the application and consists of 4 pointers
 * plus 1 32 bit quantity per token in the set.
 */
typedef void *tkc_state_t;

/*
 * Token Server state
 */
typedef void *tks_state_t;

/*
 * Application interface state - contains information that is
 * likely to be constant for many objects within the same application.
 * It includes the callback routines, etc.
 */
typedef struct {
	/*
	 * (*tkc_obtain) - called when the token client module needs a token
	 *	set that it doesn't already have.
	 *
	 *	The tokens obtained is inferred from
	 *	taking the 'toobtain' set and subtracting the 'refused' set. 
	 *	It is the responsibility of the callout to set the return
	 *	token sets ('refused') no matter what.
	 */
	void (*tkc_obtain)(
		void *,		/* client object pointer */
		tk_set_t,	/* tokens to be obtained */
		tk_set_t,	/* tokens to be returned */
		tk_disp_t,	/* disposition of tokens to be returned */
		tk_set_t *);	/* tokens refused */
	/*
	 * (*tkc_return) - called when the token client module wants to return
	 *	a token set
	 * The final argument gives the disposition of each of the
	 * tokens in the 2 sets. The sets don't overlap.
	 * For 2nd token set (don't know), the disposition will never be CLIENT
	 *
	 * The client object manager MUST call tkc_return on all tokens
	 * in the first set (to be returned)
	 *
	 * For tokens with the CLIENT disposition, tkc_returned() may only be
	 * called after a successful RPC to the server.
	 * For the other cases, tkc_returned may be called immediately after
	 * the one-way msg to the server is sent.
	 *
	 * Note that this callout may be called many times in response to
	 * a single tkc_recall() call.
	 */
	void (*tkc_return)(
		tkc_state_t,	/* client token data */
		void *,		/* client object pointer */
		tk_set_t,	/* tokens to be returned */
		tk_set_t,	/* tokens client doesn't know about */
		tk_disp_t);	/* disposition of tokens */
} tkc_ifstate_t;

/*
 * Macro for declaring the appropriate sized client token state struct
 */
#if (_MIPS_SZPTR == 32)
#define TKC_DECL(name, n)	tkc_state_t name[n + 4]
#elif (_MIPS_SZPTR == 64)
#define TKC_DECL(name, n)	tkc_state_t name[(n+1)/2 + 4]
#else
BOMB!!
#endif

/*
 * tkc_init - one time client side initialization
 */
extern void tkc_init(void);

/*
 * tkc_create - create client side of a token managed object
 */
extern void tkc_create(
	char *,			/* name (up to TK_NAME_SZ-1 chars) */
	tkc_state_t *,		/* object state to set up */
	void *,			/* client object pointer */
	tkc_ifstate_t *,	/* interface state */
	int,			/* # of tokens in set */
	tk_set_t,		/* tokens to mark as already present */
	tk_set_t,		/* tokens to mark as held (subset of previous
				 * set) */
	void *);		/* tag for tracing */

extern void tkc_create_local(
	char *,			/* name (up to TK_NAME_SZ-1 chars) */
	tkc_state_t *,		/* object state to set up */
	tks_state_t *,		/* object server pointer */
	int,			/* # of tokens in set */
	tk_set_t,		/* tokens to mark as already present */
	tk_set_t,		/* tokens to mark as held (subset of previous
				 * set) */
	void *);		/* tag for tracing */


/*
 * tkc_destroy - stop managing an object.
 * It is assumed that NO tokens are owned/cached and NO requests for any
 * more tokens can come in.
 */
extern void tkc_destroy(
	tkc_state_t *);		/* object to be destroyed */

/*
 * tks_free - reset state and destroy object.
 */
extern void tkc_free(
	tkc_state_t *);		/* object to be freed */

/*
 * tks_copy - copy state.
 */
extern void tkc_copy(
	tkc_state_t *,		/* source object */
	tkc_state_t *);		/* destination object */

/*
 * tkc_destroy_local - stop managing a local object
 * Any cached tokens are flushed back to the (local) server.
 * It is assumed that NO tokens are owned and NO requests for any more tokens
 * can come in
 */
extern void tkc_destroy_local(
	tkc_state_t *);		/* object to be destroyed */


/*
 * tkc_acquire - get a token(s)
 * tkc_acquire1 - acquire a single token - FAST path for cached/owned tokens
 * The tokens are obtained from the server if they are not present
 * on the client cell and held. While held, they cannot be forceably recalled.
 *
 * tkc_acquire1 returns 0 if token was acquired, !0 otherwise
 */
extern void tkc_acquire(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens requested */
	tk_set_t *);		/* tokens acquired */

extern int tkc_acquire1(
	tkc_state_t *,		/* object containing tokens */
	tk_singleton_t);	/* single token to acquire */

/*
 * tkc_obtaining - guts of tkc_acquire - a set of tokens wanted is passed
 * in and 4 sets are passed back. Tokens requested but not in any set
 * may be assumed to have already been obtained. Since classes must be acquired
 * in ascending order, the 'get-later' set will include those classes of the
 * initial set which haven't yet been looked at.
 *
 * Once the tokens have been obtained/returned - tkc_obtained must be called
 * to inform the client module that the tokens have been dealt with.
 * The disposition argument to tkc_obtained should be the same as the
 * disposition passed back in tkc_obtaining.
 */
extern void tkc_obtaining(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens requested */
	tk_set_t *,		/* tokens to obtain */
	tk_set_t *,		/* tokens to return */
	tk_disp_t *,		/* disposition of tokens to return */
	tk_set_t *,		/* tokens that were refused */
	tk_set_t *);		/* get later please! */

extern void tkc_obtained(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens which have been obtained */
	tk_set_t,		/* tokens server refused to give */
	tk_disp_t);		/* disposition of return tokens refused */

/*
 * tkc_release - release a held token
 */
extern void tkc_release(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t);		/* tokens to release */

extern void tkc_release1(
	tkc_state_t *,		/* object containing tokens */
	tk_singleton_t);

/*
 * tkc_hold - hold a token. No attempt is made to acquire the token if
 * it is not on the client. The set of successfully held tokens is returned.
 */
extern tk_set_t tkc_hold(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t);		/* tokens requested */

/*
 * tkc_recall - respond to a server request to return a token.
 *
 * It can also be used by the client to optimize recall transactions - if
 * the client knows that soon one will be acquiring a token that would likely
 * require a recall - one can plan ahead and force the recall early.
 *
 * This call is basically asynchronous - if the token is in a state where
 *	it can't be sent back - it is marked for future handling. Thus
 *	it is perfectly legal to call tkc_recall on a token which the caller
 *	has currently held. When the caller finally calls tkc_release, the
 *	token will be sent back.
 */
extern void tkc_recall(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens to return */
	tk_disp_t);		/* client or server initiated?? */

/*
 * tkc_returning - return a set of tokens by pushing them back to the server.
 * If last parameter is TK_WAIT then any tokens that are held, in the
 *	process of being recalled will be waited for - otherwise
 *	tokens in these states are ignored. It's important not to have
 *	any of the tokens passed in the return set 'held' - if one passes
 *	TK_WAIT then one will deadlock.
 * This can improve performance if the client knows that it won't be needing
 * the token for a long time and its likely that someone else will
 *
 * This is also used to remove any client held tokens before destroying
 * the object. For this call, the 'return' set may contain multiple
 * levels per class and classes that the client doesn't currently have.
 * This call is basically synchronous - all the results are known when the
 * call returns.
 * The set returned in the OUT parameter MUST be sent to tkc_returned().
 * The 'why' parameter is of course always filled in with CLIENT_INITIATED
 * but is useful when wishing to combine sets with other sets.
 */
extern void tkc_returning(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens to return */
	tk_set_t *,		/* OUT:tokens ok to return */
	tk_disp_t *,		/* OUT: disposition of ok tokens */
	int);			/* flags: wait for tokens in transit */

/*
 * tkc_returned - return a set of tokens
 * Used by the client to disposition a set of tokens. It must call this
 * in 1 of 2 cases:
 * 1) to return tokens required by a call to (*tkc_return)()
 *	In this case, it can call tkc_returned before or after sending
 *	the one-way message to the server which contains the tokens
 * 2) to return tokens initiated via tkc_returning()
 *	In this case, tkc_returned must be called only after the server
 *	has acknowledged the return (via an RPC)
 * NOTE: this routine updates client state only - it does NOT perform any
 * callouts - to 'return' tokens, one must use tkc_recall or tkc_returning.
 */
extern void tkc_returned(
	tkc_state_t *,		/* object containing tokens */
	tk_set_t,		/* tokens to return */
	tk_set_t);		/* tokens refused to be returned */

/*
 * tk_print_tk_set - print token set
 */
#ifndef _KERNEL
extern void tk_print_tk_set(tk_set_t, FILE *, char *, ...);
#else
extern void tk_print_tk_set(tk_set_t, char *, ...);
#endif

/*
 * tkc_print - print info about client token object
 */
#ifndef _KERNEL
extern void tkc_print(tkc_state_t *, FILE *, char *, ...);
#else
extern void tkc_print(tkc_state_t *, char *, ...);
#endif

/*
 * Token Server
 */
/*
 * client handle - token system can handle any unique value
 */
typedef int tks_ch_t;

/*
 * Application interface state - contains information that is
 * likely to be constant for many objects within the same application.
 * It includes the callback routines, etc.
 */
typedef struct {
	/*
	 * tks_recall - called when the server wishes to have a token recalled.
	 * The disposition will be either SERVER_MANDATORY or SERVER_CONDITIONAL
	 * for each token.
	 */
	void (*tks_recall)(
		void *,		/* server object pointer */
		tks_ch_t,	/* client handle */
		tk_set_t,	/* tokens to be recalled */
		tk_disp_t);	/* disposition */
	/*
	 * tks_recalled - called when a recall operation is complete
	 * Note that in the returned tk_set_t's that level doesn't matter
	 * just the class
	 */
	void (*tks_recalled)(
		void *,		/* server object pointer */
		tk_set_t,	/* tokens that were recalled */
		tk_set_t);	/* tokens that weren't recalled */
	/*
	 * tks_idle - called when a class goes idle. This is only
	 * called if the class was tagged as wanting idle notification
	 * via tks_notify_idle
	 * Note: there is no locking during this call - object manager
	 * must provide some synchronization.
	 */
	void (*tks_idle)(
		void *,		/* server object pointer */
		tk_set_t);	/* set that went idle */
		
} tks_ifstate_t;

#if (_MIPS_SZPTR == 32)
#define TKS_DECL(name, n)	tks_state_t name[n + 6]
#elif (_MIPS_SZPTR == 64)
#define TKS_DECL(name, n)	tks_state_t name[(n+1)/2 + 5]
#else
BOMB!!
#endif

/*
 * tks_init - one time server side initialization
 */
extern void tks_init(void);

/*
 * tks_create - create server side of a token managed object
 */
extern void tks_create(
	char *,			/* name (up to TK_NAME_SZ-1 chars) */
	tks_state_t *,		/* object state to set up */
	void *,			/* server object pointer */
	tks_ifstate_t *,	/* interface state */
	int,			/* # of tokens in set */
	void *);		/* tag for tracing */

/*
 * tks_destroy - stop managing an object.
 * It is assumed that NO requests for any more tokens can come in.
 */
extern void tks_destroy(
	tks_state_t *);		/* object to be destroyed */

/*
 * tks_free - reset state and destroy object.
 */
extern void tks_free(
	tks_state_t *);		/* object to be freed */

/*
 * tks_obtain - obtain a set of tokens on behalf of a client.
 * Does not return until all tokens are either granted, or the client
 * already has them.
 *
 * Currently the refused set will always be 0.
 */
extern void tks_obtain(
	tks_state_t *,		/* object containing token */
	tks_ch_t,		/* handle for client making request */
	tk_set_t,		/* tokens wanted */
	tk_set_t *,		/* tokens granted */
	tk_set_t *,		/* tokens refused */
	tk_set_t *);		/* tokens already held by client */

/*
 * tks_return - return a set of tokens
 * The last argument encodes per token why each is being returned. The reason
 * is either TK_SERVER_RECALL, TK_SERVER_REVOKE, or TK_CLIENT_INITIATED
 * XXX currently the 'refused' set is not supported for applications.
 */
extern void tks_return(
	tks_state_t *,		/* object containing token */
	tks_ch_t,		/* client handle */
	tk_set_t,		/* tokens to be returned */
	tk_set_t,		/* tokens refused to be returned */
	tk_set_t,		/* tokens unknown to client */
	tk_disp_t);		/* disposition for each token */

/*
 * tks_recall - retrieve tokens - all tokens for the specified class(es)
 * are requested to be sent home upon last use.
 * This will invoke (*tks_recall)() for each client that has the token
 * granted AT THE TIME tks_recall() was called.
 * The client MUST respond and call tks_return() with 3 sets of tokens:
 * 	1) set to be returned
 *	2) set refused to be returned
 *	3) set client didn't think it had
 * The third set can result from message races
 * Once all classes have been recalled - the (*tks_recalled)() function is
 * called
 *
 * Note that only 1 tks_recall is permitted to be outstanding for a given
 * tks_state_t. Also, the tk_set_t simply specifies which classes are to
 * be recalled - the level doesn't matter.
 *
 * Sequence:
 *	tks_recall (app originated) ->
 *	(*tks_recall) (server callout function) ->
 *	RPC to client ->
 *	tkc_recall ->
 *	(*tkc_return) (client callout function) ->
 *	RPC back to server ->
 *	tks_return ...
 *	(*tks_recalled) when all RPCs complete
 */
extern void tks_recall(
	tks_state_t *,		/* object containing token */
	tk_set_t,		/* tokens to be recalled */
	tk_set_t *);		/* tokens refused, null for async callout */

/*
 * tks_iterate - call specified function for each client cell that
 * has the the specified token
 * The callout is called once per client.
 * Progress can we altered by various return codes from the callout.
 * Returns last returned value from callout.
 */
typedef enum {
	TKS_STOP, TKS_CONTINUE, TKS_RETRY
} tks_iter_t;

/* flag values */
#define TKS_STABLE	0x0001	/* caller asserts that token set it stable
				 * no in-progress obtains or recalls
				 * This is a debugging flag only
				 */


extern tks_iter_t tks_iterate(
	tks_state_t *,		/* object containing token */
	tk_set_t,		/* tokens to search for */
	int,			/* flag */
	tks_iter_t (*)(
		void *,		/* server object pointer */
		tks_ch_t,	/* client handle */
		tk_set_t,	/* tokens client owns */
		va_list),	/* args */
	...);			/* args */

/*
 * tks_cleancell - For the passed in cell, clear any grants it has
 *	corresponding to the passed in tk_set_t.
 *	This routine handles 'returning' the token if it has an outstanding
 *	recall, thus the (*recalled) callout or (*obtain) could
 *	be called as the result of this call.
 *
 * The result is:
 *	If the token was the subject of a tks_recall,
 *	the resultant (*recalled)() callout will have
 *	the token(s) in the 'weren't recalled' list.
 *
 *	If the token was waiting for a recall due to an obtain pending
 *	for a conflicting level, the obtain will be failed.
 *
 *	If the token was simply granted to the client w/ no other activity
 *	the token is assumed returned safely.
 */

/* flag values */
#define TKS_CHECK	0x0001	/* don't do anything - just return token
				 * set client has
				 */
extern void tks_cleancell(
	tks_state_t *,		/* object containing token */
	tks_ch_t,		/* client handle */
	tk_set_t,		/* tokens to cleanup */
	tk_set_t *,		/* tokens cleaned */
	int);			/* flags */

/*
 * tks_notify_idle
 *	Mark a set of classes to be notified when they go idle (no grants).
 * When a class goes idle the (*tks_idle)() callout will be called
 */
extern void tks_notify_idle(
	tks_state_t *,		/* object containing token */
	tk_set_t);		/* tokens to notify on idle */

/*
 * tks_state
 *	Call out to the supplied function with the sets of tokens that
 * are either held or being currently revoked/recalled.
 */
extern void tks_state(
	tks_state_t *,		/* object containing tokens */
	tks_ch_t,		/* client we are interested in */
	void (*)(
		void *,		/* server object pointer */
		tks_ch_t,	/* client handle */
		tk_set_t,	/* unrecalled tokens client owns */
		tk_set_t));	/* tokens being recalled or revoked */

#ifdef _KERNEL
/*
 * tks_bag - return total state of the token service for
 * the associated object. An object data is filles with data
 * encapsulating the token state of all tokens and the current clients.
 * The token state is marked as undergoing migration and further token
 * activity is not expected.
 * It is assumed that the object is in a quiesced state when this
 * is called so that the state is frozen. This routine is used during
 * token server migration.
 */
extern void tks_bag(
	tks_state_t *,		/* object to be migrated */
	obj_bag_t);		/* object bag to use for migrated data */

/*
 * tks_unbag - re-establish token server state for an object from
 * bagged data.
 */
extern int tks_unbag(
	tks_state_t *,		/* object to be migrated */
	obj_bag_t);		/* pointer to bag with migrated data */

/*
 * tks_isclient - determine if a cell is a client of an object
 */
extern int tks_isclient(
	tks_state_t *,		/* object of interest */
	tks_ch_t);		/* cell */
#endif

/*
 * tks_print - print info about a token object
 */
#ifndef _KERNEL
extern void tks_print(tks_state_t *, FILE *, char *, ...);
#else
extern void tks_print(tks_state_t *, char *, ...);
#endif

/*
 * Common routines/data structures
 */
/*
 * Metering info - dynamically allocated per token client or server if
 * metering is requested
 */
#define TK_NAME_SZ	16
typedef struct tk_meter_s {
	struct tk_meter_s  *tm_link;	/* forward meter link */
	void		   *tm_h;	/* pointer to token handle */
	void		   *tm_tag;	/* user-set tag for tracing */
	char		    tm_name[TK_NAME_SZ]; /* name! */
	char		    tm_which;	/* client or server */
	union {
	 struct tkc_class_data {
		unsigned int tm_acquire;   /* # acquire attempts
					    * via tkc_obtaining or via tkc_hold
					    */
		unsigned int tm_acq_cached;/* # of acquires where token
					    * was immediately avail
					    * either cached or already owned
					    */
		unsigned int tm_acq_conflict;/* # of acquires where token
					      * level conflicted with one
					      * cell already had
					      */
		unsigned int tm_acq_outbound;/* # of acquires where token
					      * is in process of being sent
					      * back due to recall, return
					      * conflicting level, etc.
					      */
		unsigned int tm_recall;	/* # tkc_recall */
		unsigned int tm_return; /* # tkc_returns */
	 } tm_cclass_data[TK_MAXTOKENS];
	 struct tks_class_data {
		char *tm_trackcrecall;	/* bitmap - track conditional recalls */
		char *tm_trackmrecall;	/* bitmap - track mandatory recalls */
		unsigned int tm_obtain;	/* # calls to tks_obtain */
		unsigned int tm_return; /* # tks_returns */
		unsigned int tm_revrace;/* # races where client says 'eh'
					 * and we think client still has token
					 * (SERVER_MANDATORY)
					 */
		unsigned int tm_mrecall;/* # of mandatory recalls */
		unsigned int tm_revracecur; /* current number of revraces for
					     * this particular revoke
					     * Used to debug infinite loops
					     */
		unsigned int tm_recrace;/* # races where client says 'eh'
					 * and we think client still has token
					 * RECALL
					 */
		unsigned int tm_recracecur; /* current number of recraces for
					     * this particular recall
					     * Used to debug infinite loops
					     */
	 } tm_sclass_data[TK_MAXTOKENS];
	} tm_data;
} tk_meter_t;

#define TK_METER_CLIENT	1
#define TK_METER_SERVER	2

/* turn on metering for client */
extern tk_meter_t *tkc_meteron(
	tkc_state_t *,
	char *,		/* name */
	void *);	/* tag */

/* retrieve meter struct */
extern tk_meter_t *tkc_meter(tkc_state_t *);

/* turn off metering */
extern void tkc_meteroff(tkc_state_t *);

/* turn on metering for server */
extern tk_meter_t *tks_meteron(
	tks_state_t *,
	char *,		/* name */
	void *);	/* tag */

/* retrieve meter struct */
extern tk_meter_t *tks_meter(tks_state_t *);

/* turn off metering */
extern void tks_meteroff(tks_state_t *);

/*
 * tk_printlog - dump log entries
 * FILE * - where to dump
 * int - dump last 'n' entries - (-1) for all
 * int - how verbose (from selections below)
 * char * - criteria (NULL to print everything)
 */
#define TK_LOG_TS	0x1 /* add time stamps and obj pointer */
#define TK_LOG_STATE	0x2 /* print state of all tokens after each op */
#define TK_LOG_ALL	0xf /* print everything */
#ifndef _KERNEL
extern void tk_printlog(FILE *, int, int, char *);
#else
extern void tk_printlog(int, int, char *);
#endif

/*
 * tuneables
 */
/* if set to 1 then tracing will print as it gets entries */
extern int __tk_tracenow;

/* tracing buffer size */
extern unsigned int __tk_tracemax;

/* default metering */
extern int __tk_defaultmeter;

/* # of metering hash bucket */
extern unsigned int __tk_meterhash;

#endif
