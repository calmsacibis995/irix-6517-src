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
#ifndef __TK_PRIVATE_H__
#define __TK_PRIVATE_H__

#ident "$Id: tk_private.h,v 1.34 1997/07/23 01:33:37 beck Exp $"

#ifdef _KERNEL
#include <stdarg.h>
#include <string.h>
#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/idbgentry.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/timers.h"
#include "sys/systm.h"
#include "ksys/cell/tkm.h"
#include "ksys/cell.h"
typedef lock_t tklock_t;
typedef sema_t tksync_t;
typedef mutex_t tkmutex_t;
#ifdef DEBUG
#define TK_DEBUG 1
#define TK_METER 1
#define TK_TRC	 1
#endif
#ifdef TK_DEBUG
#define TK_ASSERT(EX) ASSERT(EX)
#else
#define TK_ASSERT(EX) ((void)0)
#endif

#else /* !_KERNEL */

#include <stdarg.h>
#include "tkm.h"
#include "ulocks.h"
#include "assert.h"
#include "sys/types.h"
#include "unistd.h"
#include "string.h"
#include "bstring.h"
#include "mutex.h"
#include "stdlib.h"
#include "syncv.h"
#include "signal.h"
#include "sys/procset.h"
typedef ulock_t tklock_t;
typedef usema_t tksync_t;
typedef usema_t tkmutex_t;
#ifdef TK_DEBUG
#define TK_ASSERT(EX) assert(EX)
#else
#define TK_ASSERT(EX) ((void)0)
#endif
/* kernel only asserts */
#define ASSERT(EX) ((void)0)

#endif /* _KERNEL */
/*
 * common/internal routine prototypes
 */
#ifdef _KERNEL
#define TK_LOCKDECL		int __tk_s;
#define TK_LOCKVARADDR		&__tk_s
#define TK_LOCK(l)		__tk_s = mutex_spinlock(&l)
#define TK_UNLOCK(l)		mutex_spinunlock(&l, __tk_s)
#define TK_UNLOCKVAR(l,s)	mutex_spinunlock(&l, s)
#define TK_WAITRECALL(si)	{ \
					sv_wait(&recall_sv, PZERO, \
						&(si)->tksi_lock,  __tk_s); \
					TK_LOCK((si)->tksi_lock); \
				}
#define TK_WAKERECALL(si)	sv_broadcast(&recall_sv);
#define TK_MALLOC(n)		kern_malloc(n)
#define TK_FREE(v)		kern_free(v)
#define TK_NODEMALLOC(n)	kern_malloc(n)
#define TK_NODEFREE(v)		kern_free(v)
#define TK_DECL_SYNC		tksync_t
#define TK_CREATE_SYNC(v)	initnsema(&v, 0, "tksy")
#define TK_DESTROY_SYNC(v)	freesema(&v)
#define TK_SYNC(v)		psema(&v, PZERO)
#define TK_WAKE(v)		vsema(&v)
#define TK_CREATE_MUTEX(x)	mutex_init(&x, MUTEX_DEFAULT, "tkcl")
#define TK_DESTROY_MUTEX(x)	mutex_destroy(&x)
#define TK_MUTEX_GET(x)		mutex_lock(&x, PZERO)
#define TK_MUTEX_PUT(x)		mutex_unlock(&x)
#define TK_GETCH(x)		((tks_ch_t)cellid())
#define TK_ATOMIC_ADD(x,y)	atomicAddUint(x, y)
#define TK_DELAY(x)		delay(x)
#else
extern usptr_t *__tkus;
#define TK_LOCKDECL
#define TK_LOCKVARADDR	0
#define TK_LOCK(l)		ussetlock(l)
#define TK_UNLOCK(l)		usunsetlock(l)
#define TK_UNLOCKVAR(l,s)	usunsetlock(l)
#define TK_MALLOC(n)		malloc(n)
#define TK_FREE(v)		free(v)
#define TK_NODEMALLOC(n)	malloc(n)
#define TK_NODEFREE(v)		free(v)
#define TK_DECL_SYNC		tksync_t *
#define TK_CREATE_SYNC(v)	v = usnewsema(__tkus, 0)
#define TK_DESTROY_SYNC(v)	usfreesema(v, __tkus)
#define TK_SYNC(v)		uspsema(v)
#define TK_WAKE(v)		usvsema(v)
#define TK_CREATE_MUTEX(x)	x = usnewsema(__tkus, 1)
#define TK_DESTROY_MUTEX(x)	usfreesema(x, __tkus)
#define TK_MUTEX_GET(x)		uspsema(x)
#define TK_MUTEX_PUT(x)		usvsema(x)
extern tks_ch_t getch_t(void *);
#define TK_GETCH(x)		getch_t(x)
#define TK_ATOMIC_ADD(x,y)	test_then_add32(x, y)
#define TK_DELAY(x)		sginap(x)
#endif

/*
 * Implementation of tokens
 *
 * Naming conventions:
 *	tkci_ client implementation
 *	tksi_ server implementation
 */
#define TK_SET_MASK	0x7

/*
 * tkci_state definitions
 * cl_state -> current state for class
 * cl_mrecall -> level(s) being recalled (mandatory).
 *		This may come in before the
 *		the token is actually held by anyone. This is only
 *		on the behalf of a server initiated mandatory recall.
 * cl_obtain -> level(s) being obtained - the thread that sets this bit
 *		is the one that will get the token
 * cl_extlev -> extended level - holds all levels that are currently outstanding
 * cl_hold ->hold count for all tokens for a given class. Most operations
 *	     cannot occur until hold count == 0
 */
typedef union {
	struct cl {
		unsigned int	ccl_unused:1,
				ccl_scwait:1,	/* waiting for status change */
				ccl_state:5,	/* state machine */
				ccl_mrecall:3,	/* level(s) being m-recalled */
				ccl_obtain:3,	/* level(s) being obtained */
				ccl_extlev:3,	/* extended level */
				ccl_hold:16;	/* hold count */
	} cl_un;
	unsigned int		cl_word;
} tkci_clstate_t;
#define cl_scwait	cl_un.ccl_scwait
#define cl_state	cl_un.ccl_state
#define cl_mrecall	cl_un.ccl_mrecall
#define cl_obtain	cl_un.ccl_obtain
#define cl_extlev	cl_un.ccl_extlev
#define cl_hold		cl_un.ccl_hold

/*
 * states
 */
/*
 * IDLE - we have no tokens for the given class, are not in the
 * process of returning any nor obtaining any.
 */
#define TK_CL_IDLE	1

/*
 * OWNED - the token is held and there is no reason to return it even
 * when its hold count goes to 0
 */
#define TK_CL_OWNED	2

/*
 * CACHED - we own a token at a specific level but it is not currently held
 */
#define TK_CL_CACHED	3

/*
 * REVOKING - the token is held but has been recalld (mandatory). No further
 * acquires will be granted and the token will be returned when the hold
 * count goes to zero.
 * The REVOKING state is always the result of a client-initiated recall.
 * Either via tkc_recall() or due to a thread attempting to obtain a
 * conflicting token (and the server asking for the token back).
 */
#define TK_CL_REVOKING	4

/*
 * OBTAINING - in the process of obtaining the token. The thread that sets
 * this state is the one that will receive the token.
 * This may embody the RETURNING state also - we could be in the process
 * of returning a token to obtain a conflicting one.
 */
#define TK_CL_OBTAINING	5

/*
 * ALREADY - an obtain was refused because the token server believed
 * we already had the token. The assumption is that a tkc_gotten is pending
 * that will resolve this situation
 */
#define TK_CL_ALREADY	6

/*
 * WILLRETURN - a token is held that some thread is waiting to return.
 * No further acquires will be granted and the waiting thread will be
 * woken when the hold count goes to 0
 */
#define TK_CL_WILLRETURN 7

/*
 * RETURNING - in the process of returning a token. The token is not held
 */
#define TK_CL_RETURNING 8

/*
 * WILLOBTAIN - the thread that is waiting to obtain a specific token level
 * may now proceed.
 */
#define TK_CL_WILLOBTAIN 9

/*
 * BORROWED - like OWNED except that when the hold count goes to 0
 * we will automatically return the token. Further acquires are permitted
 * This only applies to conditionally recalled tokens. All the 'B' states are
 * various states a conditionally recalled token can go through. In the end,
 * a (*tkc_return)() will be called with the SERVER_CONDITIONAL reason.
 */
#define TK_CL_BORROWED	10

/*
 * BORROWING - like OBTAINING except that the server is owed a recall
 * message as soon as the hold count goes to zero. Note that further grants
 * are permitted unlike the *REVOKING states.
 */
#define TK_CL_BORROWING	11

/*
 * REJECTED - a token obtain was refused. Stay in this state until the
 * hold count goes to zero.
 */
#define TK_CL_REJECTED	12

/*
 * BREVOKING - like REVOKING except that when the hold count goes to
 *	0, send a SERVER_CONDITIONAL recall msg
 */
#define TK_CL_BREVOKING 13

/*
 * SREVOKING - like REVOKING except that when the hold count goes to
 *	0, send a SERVER_MANDATORY recall msg
 */
#define TK_CL_SREVOKING 14

/*
 * SOBTAINING - like OBTAINING except that once the token arrives
 *	it should transition into the SREVOKING state so that it gets
 *	sent back pronto w/ no more grants being given.
 */
#define TK_CL_SOBTAINING 15

/*
 * COBTAINING - like OBTAINING except that once the token arrives
 *	it should transition into the REVOKING state so that it gets
 *	sent back pronto w/ no more grants being given.
 */
#define TK_CL_COBTAINING 16

/*
 * BOBTAINING - like BORROWING except that once the token arrives
 *	it should transition into the BREVOKING state so that it gets
 *	sent back pronto w/ no more grants being given.
 */
#define TK_CL_BOBTAINING 17

/*
 * SWILLOBTAIN - like WILLOBTAIN except that when returning tokens,
 * use the SERVER_REVOKE return msg type
 */
#define TK_CL_SWILLOBTAIN 18

/*
 * CREVOKING - used to transition between WILLRETURN to the
 * thread that set teh WILLRETURN state. This means that the client should
 * send the token back with the CLIENT_INITIATED type.
 */
#define TK_CL_CREVOKING	19

/*
 * CRESTORE - a to-be-returned token was refused in tkc_obtained.
 * The client initiated the return
 */
#define TK_CL_CRESTORE 20

/*
 * SRESTORE - a to-be-returned token was refused in tkc_obtained.
 * A SERVER_MANDATORY recall initiated the return.
 */
#define TK_CL_SRESTORE 21

/*
 * BRESTORE - a to-be-returned token was refused in tkc_obtained.
 * A SERVER_CONDITIONAL recall initiated the return.
 */
#define TK_CL_BRESTORE 22

/*
 * extended level - a mask of levels in use, always comaptible. Remember
 * that the token manager doesn't maintain coherency for tokens on a given
 * cell - multiple threads on a cell can all get a write token.
 * The extended level also keeps the state of which token level we have
 * when a token is cached.
 * Valid values for the extended level are:
 * 0		- no token cached
 * READ		- a read token cached (and perhaps held)
 * WRITE	- a write token cached (and perhaps held)
 * SWRITE	- a shared write token cached (and perhaps held)
 * READ|WRITE	- a write token cached and either/both a read/write held
 * READ|SWRITE	- a shared write token cached and either/both a read/swrite held
 * Note that a multi-valued state implies that the token is in fact being
 * 	used.
 * For ASSERT purposes - all values are legal except 0, 6, 7
 */
/*
 * client state - this structure overlays the tkc_state_t array that
 * the user defines.
 */
typedef struct {
	tkc_ifstate_t *tkci_if;	/* interface */
	void *tkci_obj;		/* client object pointer */
	ushort_t tkci_ref;		/* reference count */
	uchar_t tkci_ntokens;	/* # of tokens in set */
	uchar_t tkci_flags;
#if (_MIPS_SZPTR == 64)
	/* must pad so tksi_state is on double boundary */
	cell_t tkci_origcell;	/* cell tkc created on (debug) */
#endif
	tklock_t tkci_lock;	/* client lock */
	tkci_clstate_t tkci_state[TK_MAXTOKENS]; /* state word per token */
} tkci_cstate_t;

/* values for flags */
#define TKC_NOHIST	0x1
#define TKC_METER	0x2	/* provide metering info */
#define TKC_MIGRATING	0x4	/* client state being migrated */

/*
 * Server side implementation
 */
/*
 * state word per token class
 *
 * If the class word is locked all further obtains will block
 * The current bit distribution implies the following limits:
 * Max client cells (== max grants and max outstanding requests)
 *	1024 = 10 bits
 */
typedef struct {
	unsigned int	svr_grants:10,	/* # grants */
			svr_noteidle:1,	/* notify on idle */
			svr_backoff:2,	/* backoff for retransmits */
			svr_revinp:1,	/* class revoke msgs in progress */
			svr_revoke:1,	/* this class being revoked */
			svr_recall:1,	/* this class being recalled */
			svr_rwait:1,	/* waiting for revoke */
			svr_lock:1,	/* class is locked */
			svr_lockw:1,	/* lock is waited for */
			svr_level:3,	/* level granted */
			svr_noutreqs:10;/* # outstanding initiated requests
					 * XX currently only revokes
					 */
} tksi_svrstate_t;

/*
 * server state - this structure overlays the tks_state_t array that
 * the user defines.
 */
typedef struct {
	tks_ifstate_t *tksi_if;	/* interface */
	void *tksi_obj;		/* server object pointer */
	ushort_t tksi_nrecalls;	/* # of outstanding classes X clients */
	uchar_t tksi_ntokens;	/* # of tokens in set */
	uchar_t tksi_flags;
	union {
		struct {
			ushort_t __tksi_origcell;/* cell tks was created on (debug) */
			ushort_t __tksi_ref;	/* ref count */
		} __ref;
		uint_t __tksi_refint;		/* use this for atomic adds */
	} tksi_un;
	tklock_t tksi_lock;	/* lock */
	char *tksi_gbits;	/* grant bitmaps */
	/* NOTE: tksi_state must be on double-word boundary for 64 bit */
	tksi_svrstate_t tksi_state[TK_MAXTOKENS]; /* state word per token */
} tksi_sstate_t;
#define tksi_origcell	tksi_un.__ref.__tksi_origcell
#define tksi_ref	tksi_un.__ref.__tksi_ref
#define tksi_addref	tksi_un.__tksi_refint

/* values for flags */
#define TKS_NOHIST	0x01
#define TKS_INRECALL	0x02
#define TKS_METER	0x04	/* provide metering info */
#define TKS_SYNCRECALL	0x08	/* tks_returned callout not wanted */

/* max backoff value */
#define TK_MAXBACKOFF	3

/*
 * Token migration synchronization:
 */

/* Object service states. */
typedef enum {
	TKM_INSERVICE,		/* unused */
	TKM_ARRESTING,
	TKM_ARRESTED
} tk_mig_state_t;

/*
 * Structure allocated only during object migration.
 * It tracks progress of object quiescing.
 */
typedef struct {
	tk_set_t	tkmi_existence;
	tk_set_t	tkmi_service;
	tk_mig_state_t	tkmi_migration_state;
	cell_t		tkmi_to_cell;
} tkmi_state_t;

/*
 * generic list management.
 * Server must keep the following lists:
 *	classwait - wait for a class to go unlocked
 *		TAG - tksi_sstate_t, class
 *	obtaining - which client should get woken to acquire token.
 *		TAG - tksi_sstate_t, class
 * Client must keep the following lists:
 *	obtaining - which clients are waiting for a token.
 *		TAG - tkci_cstate_t, class
 * XXX need fancy algorithm here
 */
typedef struct tk_list_s {
	void *tk_tag1;
	int tk_tag2;
	int tk_nwait;
	TK_DECL_SYNC tk_wait;
	tklock_t tk_lock;
	struct tk_list_s *tk_next;
	struct tk_list_s *tk_prev;
} tk_list_t;

extern void __tk_lock_init(void);
extern void __tk_destroy_lock(tklock_t *);
extern void __tk_create_lock(tklock_t *);

/* other routines */
extern void __tk_fatal(char *, ...);
extern char *__tk_levtab[];
extern char *__tkci_prstate(tkci_clstate_t);
extern void __tk_meter_init(void);
extern void tk_backoff(int);
extern char *__tksi_prbits(char *, char *, bitnum_t, char *, char *);

/*
 * Tracing
 */
#define TK_MAXTRBM 4	/* 32 cells (needs to be an int multiple) */
typedef struct tktrace {
	ushort_t tkt_op;
	ushort_t tkt_ntokens;
	tks_ch_t tkt_chandle;
	uint64_t tkt_id;		/* calling id */
	unsigned long long tkt_ts;	/* timestamp */
	tk_set_t tkt_act;	/* set we're acting on */
	tk_set_t tkt_act2;	/* set we're acting on */
	tk_set_t tkt_act3;	/* set we're acting on */
	void *tkt_callpc;
	void *tkt_tk;		/* ptr to token */
	void *tkt_tag;		/* user-set tag */
	char tkt_name[TK_NAME_SZ];
	union {
		struct cldata {
			tkci_clstate_t tkt_state[TK_MAXTOKENS];
			void *tkt_obj;
		} tkt_cl;
		struct sdata {
			tksi_svrstate_t tkt_state[TK_MAXTOKENS];
			void *tkt_obj;
			/* XX - these need to be on 32 bit boundary */
			char tkt_out_grants[TK_MAXTOKENS][TK_MAXTRBM];
			char tkt_out_crecalls[TK_MAXTOKENS][TK_MAXTRBM];
			char tkt_out_mrecalls[TK_MAXTOKENS][TK_MAXTRBM];
			ushort_t tkt_nrecalls;
		} tkt_s;
	} tkt_un;
} tktrace_t;

/* client ops */
#define TKC_SACQUIRE	1
#define TKC_EACQUIRE	2
#define TKC_SCACQUIRE	3
#define TKC_ECACQUIRE	4
#define TKC_SRELEASE	5
#define TKC_ERELEASE	6
#define TKC_SHOLD	7
#define TKC_EHOLD	8
#define TKC_SRECALL	9		/* tkc_recall */
#define TKC_ERECALL	10
#define TKC_SRETURNED	11
#define TKC_ERETURNED	12
#define TKC_SOBTAIN	13		/* tkc_obtain callout */
#define TKC_EOBTAIN	14		/* tkc_obtain callout */
#define TKC_SRETURNING	15
#define TKC_ERETURNING	16
#define TKC_CREATE	17
#define TKC_DESTROY	18
#define TKC_SOBTAINING	19
#define TKC_EOBTAINING	20
#define TKC_SOBTAINED	21
#define TKC_EOBTAINED	22
#define TKC_EOBTAINING2	23
#define TKC_RETURN_CALL	24		/* tkc_return callout */
#define TKC_FACQUIRE	25		/* tkc_cacquire1 */
#define TKC_FRELEASE	26		/* tkc_release1 */

/* server ops */
#define TKS_OP		0x1000		/* all server ops have this or'd in */
#define TKS_SOBTAIN	(1 | TKS_OP)
#define TKS_EOBTAIN	(2 | TKS_OP)
#define TKS_SRETURN	(3 | TKS_OP)
#define TKS_ERETURN	(4 | TKS_OP)
#define TKS_SRECALL	(6 | TKS_OP)
#define TKS_ERECALL	(7 | TKS_OP)
#define TKS_RECALL_CALL	(8 | TKS_OP)	/* callout to recall handler */
#define TKS_RECALLED	(9 | TKS_OP)	/* callout to recall handler */
#define TKS_SRETURN2	(10 | TKS_OP)
#define TKS_CREATE	(11 | TKS_OP)
#define TKS_DESTROY	(12 | TKS_OP)
#define TKS_ITERATE	(13 | TKS_OP)
#define TKS_ITERATE_CALL (14 | TKS_OP)
#define TKS_SCLEAN	(15 | TKS_OP)
#define TKS_ECLEAN	(16 | TKS_OP)
#define TKS_NOTIFY_IDLE (17 | TKS_OP)
#define TKS_IDLE_CALL	(18 | TKS_OP)
#define TKS_SCOBTAIN	(19 | TKS_OP)
#define TKS_ECOBTAIN	(20 | TKS_OP)

extern int __tk_trace(int, void *, tks_ch_t, tk_set_t, tk_set_t, tk_set_t,
					void *, tk_meter_t *);
extern void __tk_trace_init(void);
extern uint_t __tk_tracemax;
extern int __tksi_goffset[];
#define __tksi_goffset_intr __tksi_goffset

#if TK_TRC
#define TK_TRACE(a,b,c,d,e,f,g,h)	__tk_tracemax && __tk_trace(a,(void *)b,c,d,e,f,g,h)
#else
#define TK_TRACE(a,b,c,d,e,f,g,h)	((void)0)
#endif

/*
 * metering macros
 */
#if TK_METER
#define TKC_INC_METER(t,c,w)	((t) && (t)->tm_data.tm_cclass_data[c].tm_##w++)
#define TKC_GET_METER(t,c,w)	((t) ? (t)->tm_data.tm_cclass_data[c].tm_##w : 0)
#define TKC_FIELD_METER(t,c,w)	((t)->tm_data.tm_cclass_data[c].tm_##w)
#define TKS_INC_METER(t,c,w)	((t) && (t)->tm_data.tm_sclass_data[c].tm_##w++)
#define TKS_GET_METER(t,c,w)	((t) ? (t)->tm_data.tm_sclass_data[c].tm_##w : 0)
#define TKS_FIELD_METER(t,c,w)	((t)->tm_data.tm_sclass_data[c].tm_##w)
#define TK_METER_DECL		tk_meter_t *tm;
#define TK_METER_VAR		tm
#define TKS_METER_VAR_INIT(t,x)	t = tks_meter((tks_state_t *)x)
#define TKC_METER_VAR_INIT(t,x)	t = tkc_meter((tkc_state_t *)x)

#else

#define TKC_INC_METER(t,c,w)
#define TKC_GET_METER(t,c,w)	0
#define TKS_INC_METER(t,c,w)
#define TKS_GET_METER(t,c,w)	0
#define TK_METER_DECL
#define TK_METER_VAR		0
#define TKS_METER_VAR_INIT(t,x)
#define TKC_METER_VAR_INIT(t,x)
#endif

#ifndef _KERNEL
extern volatile unsigned long long *__tk_ts_init(void);
extern unsigned long long __tk_ccdelta(unsigned long long);
extern volatile unsigned long long *__tkts;
#endif

/* alas the kernel sprintf doesn't return # of chars printed so we must have
 * one of our own
 * This function returns a pointer to the NULL char at the end of the string
 */
extern char *__tk_sprintf(char *, char *, ...);

#endif
