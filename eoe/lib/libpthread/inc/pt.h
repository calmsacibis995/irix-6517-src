#ifndef _PT_H_
#define _PT_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* Global names
 */
#define pt_bootstrap		PFX_NAME(pt_bootstrap)
#define pt_dequeue		PFX_NAME(pt_dequeue)
#define pt_create_bound		PFX_NAME(pt_create_bound)
#define pt_ref			PFX_NAME(pt_ref)
#define pt_unref		PFX_NAME(pt_unref)
#define pt_foreach		PFX_NAME(pt_foreach)
#define pt_fork_child		PFX_NAME(pt_fork_child)


#include <sys/types.h>
#include <signal.h>

#include "mtx.h"
#include "q.h"

#define PT_STACK_DEFAULT	0x20000		/* 128k */
#define PT_STACK_MIN		0x4000		/*  16k */


/* Pthread run states
 *
 *                    CREATE
 *                       |
 *                       V
 *        +<--------- DISPATCH <--------+
 *        |              |              ^
 *        V              V              |
 *      READY --------> EXEC --------> WAIT
 *        ^              |  \
 *        |              V   \
 *        +<-------------+    +------> DEAD
 */
typedef enum {
	PT_DEAD = 0,	/* pthread has exited */
	PT_DISPATCH,	/* being scheduled */
	PT_READY,	/* ready to run */
	PT_EXEC,	/* running on a vp */
	PT_MWAIT,	/* waiting on a mutex */
	PT_JWAIT,	/* waiting in a join */
	PT_CVWAIT,	/* waiting on a condition variable */
	PT_CVTWAIT,	/* waiting on a condition variable with timeout */
	PT_RLWAIT,	/* waiting on a read-write lock read */
	PT_WLWAIT,	/* waiting on a read-write lock write */
	PT_SWAIT	/* waiting on a semaphore */
} pt_states_t;

/* Pthread wait synchronization structure
 */
typedef struct ptsync {
	slock_t		sync_slock;		/* simple lock */
	q_t		sync_waitq1;		/* first wait queue */
	q_t		sync_waitq2;		/* second wait queue */
						/* only used by rwlocks */
} pt_sync_t;	


/* Pthread context
 */
typedef struct pt {
	q_t		pt_queue;		/* pthread queue membership */
	slock_t		pt_lock;		/* pthread state lock */

	pt_states_t	pt_state;		/* run state */
	struct vp	*pt_vp;			/* vp running pthread */

	__uint32_t	pt_label;		/* gen num & ref count */
	__uint32_t	pt_nid;			/* kernel handle */

	schedpri_t	pt_priority;		/* currently running priority */
	schedpri_t	pt_schedpriority;	/* pthread's native priority */

	schedpolicy_t	pt_policy;
	unsigned short	pt_resched;		/* timeslice to be loaded */

	union {		/* boolean flags */
		__uint32_t	flags;
		struct {
			__uint32_t	detached : 1;
			__uint32_t	glued : 1;
			__uint32_t	system : 1;
			__uint32_t	cncl_enabled : 1;
			__uint32_t	cncl_deferred : 1;
			__uint32_t	cncl_pending : 1;
			__uint32_t	signalled : 1;
			__uint32_t	cancelled : 1;
			__uint32_t	noceiling : 1;

		} bitfields;
	} pt_bits;

	volatile __uint32_t	pt_occupied;
	volatile __uint32_t	pt_blocked;	/* count of blocks */

	void		**pt_data;		/* thread specific data */
	struct __pthread_cncl_hdlr
			*pt_cncl_first;		/* cancellation handlers */
	q_t		pt_join_q;		/* threads joining this one */

	mtx_t		*pt_minhprev;		/* pt might inherit priority */
	mtx_t		*pt_minhnext;		/* mutexes owned through */

	union {
		void	*exit_status;		/* thread exit value */
		void	*join_status;		/* joined thread exit value */
	} pt_value;
	int		pt_wait;		/* result of a wait */
	pt_sync_t	*pt_sync;		/* wait synchronization */

	int		pt_errno;		/* thread errno */
	k_sigset_t	pt_ksigmask;		/* pthread signal mask */
	sigset_t	pt_sigpending;		/* pending signals */
	sigset_t	*pt_sigwait;		/* pointer to sigwait set */

	void		*pt_stk;		/* stack */
	jmp_buf		pt_context;		/* machine context */
} pt_t;


extern void	pt_bootstrap(void);
extern pt_t 	*pt_dequeue(q_t *);
extern int	pt_create_bound(pt_t **, void *(*)(void *), void *);
extern void	pt_fork_child(void);

/*
 * Get pt_t from pthread_t and maintain reference counts.
 */
extern pt_t	*pt_ref(__uint32_t /* pthread_t */);
extern void	pt_unref(pt_t *);

/*
 * Call func for all active pthreads with arg.
 */
extern void	pt_foreach(void (*func)(pt_t *, void *), void *arg);

/*
 * Pthread queue management
 */
#define pt_q_insert_head(q, pt)		Q_INSERT_HEAD(q, pt, pt_t, pt_priority)
#define pt_q_insert_tail(q, pt)		Q_INSERT_TAIL(q, pt, pt_t, pt_priority)

/*
 * Pthread wait synchronization
 */
#define pt_sync_slock		pt_sync->sync_slock
#define pt_sync_waitq1		pt_sync->sync_waitq1
#define pt_sync_waitq2		pt_sync->sync_waitq2

/*
 * Pthread flags
 */
#define pt_flags		pt_bits.flags
#define pt_detached		pt_bits.bitfields.detached
#define pt_glued		pt_bits.bitfields.glued
#define pt_system		pt_bits.bitfields.system
#define pt_cncl_enabled		pt_bits.bitfields.cncl_enabled
#define pt_cncl_deferred	pt_bits.bitfields.cncl_deferred
#define pt_cncl_pending		pt_bits.bitfields.cncl_pending
#define pt_signalled		pt_bits.bitfields.signalled
#define pt_cancelled		pt_bits.bitfields.cancelled
#define pt_noceiling		pt_bits.bitfields.noceiling


/*
 * The 32 bit fields are allocated msb first.  Flags allow bit twiddling
 * and masks.
 */
#define	PT_DETACHED	(1<<31)	/* join state */
#define	PT_GLUED	(1<<30)	/* temporary bond to VP */
#define	PT_SYSTEM	(1<<29)	/* system scope (bound) */
#define	PT_CNCLENABLED	(1<<28)	/* cancellation is enabled */
#define	PT_CNCLDEFERRED	(1<<27)	/* cancellation is deferred (not async) */
#define	PT_CNCLPENDING	(1<<26)	/* cancellation request received */
#define	PT_SIGNALLED	(1<<25)	/* signal should be acted upon */
#define	PT_CANCELLED	(1<<24)	/* cancellation should be acted upon */
#define	PT_NOCEILING	(1<<23)	/* ignore priority ceiling rules */

#define PT_INTERRUPTS		(PT_CANCELLED | PT_SIGNALLED)
#define PT_BOUND		(PT_GLUED | PT_SYSTEM)

#define pt_is_interrupted(pt)	((pt)->pt_flags & PT_INTERRUPTS)
#define pt_is_bound(pt)		((pt)->pt_flags & PT_BOUND)


#define PT_MAKE_ID(gen, index)	((gen) | (index))
#define PT_GEN_BITS(label)	(label & 0xFFFF0000)
#define PT_REF_BITS(label)	(label & 0x0000FFFF)
#define PT_INDEX(pthread_t)	(pthread_t & 0x0000FFFF)

#endif /* !_PT_H_ */
