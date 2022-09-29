#ifndef _VP_H_
#define	_VP_H_

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
#define sched_bootstrap		PFX_NAME(sched_bootstrap)
#define sched_block		PFX_NAME(sched_block)
#define sched_dispatch		PFX_NAME(sched_dispatch)
#define sched_resume		PFX_NAME(sched_resume)
#define sched_add_vp		PFX_NAME(sched_add_vp)
#define vp_fixup_initial	PFX_NAME(vp_fixup_initial)
#define vp_create		PFX_NAME(vp_create)
#define vp_exit			PFX_NAME(vp_exit)
#define vp_yield		PFX_NAME(vp_yield)
#define vp_setpri		PFX_NAME(vp_setpri)
#define sched_proc_count	PFX_NAME(sched_proc_count)
#define sched_rr_quantum	PFX_NAME(sched_rr_quantum)
#define sched_fifo_quantum	PFX_NAME(sched_fifo_quantum)
#define sched_mask		PFX_NAME(sched_mask)
#define sched_spins		PFX_NAME(sched_spins)
#define pt_readyq		PFX_NAME(pt_readyq)
#define vp_fork_child		PFX_NAME(vp_fork_child)

#define vp_active		__vp_active	/* Special for libc */


#include <sched.h>
#include <signal.h>
#include <sys/types.h>

#include "mtx.h"
#include "q.h"
#include "sys.h"

struct pt;


/* Vp run states
 */
typedef enum {
	VP_IDLE = 1,	/* ready to run a pthread */
	VP_EXEC		/* running pthread */
} vp_states_t;


/* Virtual processor context
 */
typedef struct vp {
	q_t		vp_queue;	/* vp queue membership */

	vp_states_t	vp_state;	/* run state */ 
	struct pt	*vp_pt;		/* pthread run by vp */
	schedpri_t	vp_priority;	/* current pt priority */
	struct {
		__uint32_t	stopping : 1;	/* resched in progress */
		__uint32_t	vyield : 1;	/* voluntary yield */
		__uint32_t	resume : 1;	/* resume instead of idling */
	} vp_bits;

	void		*vp_stk;	/* stack used by pt_exit and vp_idle */

	pid_t		vp_pid;		/* pid for this vp */
	__uint32_t	vp_occlude;	/* block signals (incl. preemption) */
	struct vp	*vp_spinwaitq;	/* spinlock queue */
} vp_t;

#define vp_stopping	vp_bits.stopping
#define vp_vyield	vp_bits.vyield
#define vp_resume	vp_bits.resume

/*
 * Type of pthread switch requested.
 */
typedef enum {
	SCHED_NEW = 1,		/* thread exited - want a new one */
	SCHED_READY,		/* thread at a wait point - want first ready */
	SCHED_PREEMPTING	/* preemption check - want higher pri thread */
} sched_thread_types_t;

/*
 * Type of pthread switch that happened.
 */
typedef enum {
	SCHED_RESUME_SAME = 0,	/* restart context */
	SCHED_RESUME_NEW = 1,	/* start new context (result of longjmp(0)) */
	SCHED_RESUME_SWAP,	/* change contexts */
	SCHED_RESUME_LATER	/* block for reschedule */
} sched_resume_t;

/*
 * Type of vp to wakeup in vp_wakeup_idle().
 */
typedef enum {
	VP_WAKEUP_ANY = 1,	/* any vp will do */
	VP_WAKEUP_CACHED	/* only wake a vp that is associated with pt */
} vp_wakeup_t;

/*
 * Flag values for vp_setpri().
 */
#define	SETPRI_NOPREEMPT	0x0001

extern void		sched_bootstrap(void);
extern int		sched_block(sched_thread_types_t);
extern void		sched_dispatch(struct pt *);
extern int		sched_resume(struct pt *);
extern void		sched_add_vp(void);

extern void		vp_fixup_initial(struct pt *);
extern int		vp_create(struct pt *, int);
extern void		vp_exit(void);
extern void		vp_yield(int);
extern void		vp_setpri(struct pt *, schedpri_t, int);
extern void		vp_fork_child(void);

extern int		vp_active;
extern int		sched_proc_count;
extern unsigned short	sched_rr_quantum;
extern unsigned short	sched_fifo_quantum;
extern k_sigset_t	sched_mask;
extern int		sched_spins;
extern q_t		pt_readyq;

/* Return quantum for scheduling policy.
 */
#define SCHED_POLICY_QUANTUM(pol)	((pol == SCHED_FIFO)		\
						? sched_fifo_quantum	\
						: sched_rr_quantum)

#define	sched_enter()					\
	MACRO_BEGIN					\
		VP_SIGMASK = sched_mask;		\
		VP->vp_occlude++;			\
	MACRO_END

#define	sched_leave()					\
	MACRO_BEGIN					\
		ASSERT(sched_entered());		\
		if (--VP->vp_occlude == 0)		\
			VP_SIGMASK = PT->pt_ksigmask;	\
	MACRO_END

#define sched_entered()	(VP->vp_occlude)

/* Adjust vp population due to blocked or bound vps.
 * Note that !sched_entered() is assumed because this is in the syscall path.
 */
#define VP_ADD_ACTIVE()	add_then_test32((uint_t *)&vp_active, 1u)
#define VP_SUB_ACTIVE()	add_then_test32((uint_t *)&vp_active, -1u)

#define VP_RESERVE()	if (VP_SUB_ACTIVE() == 0) { sched_add_vp(); }
#define VP_RELEASE()	((void)VP_ADD_ACTIVE())

#define VP_YIELD(cond)	\
	{ int _retry;	\
	for (_retry = 0; (cond); _retry++) { vp_yield(_retry); } } 


#endif /* !_VP_H_ */
