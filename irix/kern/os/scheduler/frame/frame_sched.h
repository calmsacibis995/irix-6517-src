/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_FRAME_SCHED_H__
#define __SYS_FRAME_SCHED_H__

#include <sys/frs.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/atomic_ops.h>
#include <sys/idbgentry.h>
#include <sys/groupintr.h>

extern void resetcounter(uint);
extern void acktmoclock(void);

/*
 * Frame Scheduler Definitions
 */ 

/*
 * The Basic Idea:
 *
 *         Start of Major Frame             End of Major Frame
 *         |                                |
 *         |                                | 
 *         | Minor 0    Minor 1    Minor 2  |                 
 *  -------|----------*----------*----------|-----> (time)
 *         ^     ^    ^     ^    ^     ^    ^
 *         |     |    |     |    |     |    |
 *       (INTR)  |  (INTR)  |  (INTR)  |   (INTR)
 *             -----      -----      -----
 *             | - |      | - |      | - |
 *             | - |      | - |      | - |
 *            QUEUE 0    QUEUE 1    QUEUE 2
 *
 */


/*
 * The Global Data Structure View: (Basic Frame Scheduler)
 *
 *                                              Minor Frames
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/  frs_minor_t     frs_prod_t
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 * *frs_fsched_t*   |      |    |--|              |   |   [ ]<--[ ]<--[ ]<--
 *                  --------    |  |              -----
 *                              |--|
 *                frs_major_t   |  | etc.
 *                              |--|
 *                              
 */

/*
 * Forward declaration of the frame scheduler object
 */
struct frs_fsched;

/*
 * Frame Scheduler States
 */
typedef enum {
	FRS_STATE_ANY,
	FRS_STATE_CREATED,
	FRS_STATE_STARTING,
	FRS_STATE_SYNCINTR,
	FRS_STATE_FIRSTINTR,
	FRS_STATE_DISPATCH,
	FRS_STATE_EXEC,
	FRS_STATE_EXEC_IDLE,
	FRS_STATE_TERMINATION
  } frs_state_t;


/*
 * Frame Scheduler Descriptor
 */

typedef struct frs_fsched {
        intrgroup_t*	intrgroup;	/* MUST be first: interrupt group */
	uint_t		intrmask;	/* Interrupt source mask */
	uint_t		flags;   	/* Type of frame scheduler */

        semabarrier_t*  references;     /* Frs object reference count */
        
	lock_t		state_lock;	/* State lock */
	frs_state_t	state;		/* Current frs state */
	frs_state_t	prev_state;	/* Previous frs state */        

	uthread_t*	control_thread; /* Master thread for this frs */
	frs_threadid_t	control_id;	/* The id of the master thread */
	cpuid_t		cpu;		/* Cpu for this frs */
        
	syncdesc_t*	sdesc;		/* Shared synchronization descriptor */
	sigdesc_t*      sigdesc;	/* Shared signal descriptor */

	syncbar_t*      sb_intr_main;
	syncbar_t*      sb_intr_clear;
	syncbar_t*      sb_intr_first;

	sema_t          term_sema;      /* termination semaphore */

        frs_major_t*	major;		/* Major frame descriptor */

} frs_fsched_t;

#define FRS_FLAGS_MASTER	0x001
#define FRS_FLAGS_THREADED	0x002
#define FRS_FLAGS_ARMED		0x004

#if defined(IP30)
#define frs_fsched_statelock(frs) \
	mutex_spinlock_spl(&(frs)->state_lock, splprof)
#else
#define frs_fsched_statelock(frs) \
	mutex_spinlock(&(frs)->state_lock)
#endif

#define frs_fsched_stateunlock(frs, s) \
	mutex_spinunlock(&(frs)->state_lock, (s))

#define frs_fsched_nested_statelock(frs)		\
{							\
	ASSERT(issplhi(getsr()) || issplprof(getsr())); \
	nested_spinlock(&(frs)->state_lock);		\
}

#define frs_fsched_nested_stateunlock(frs) \
	nested_spinunlock(&(frs)->state_lock)

#define frs_fsched_sllock(frs) \
	mutex_spinlock(&(frs)->sdesc->sl_lock)

#define frs_fsched_slunlock(frs, s) \
	mutex_spinunlock(&(frs)->sdesc->sl_lock, (s))

#define frs_isthreaded(frs)	((frs)->flags & FRS_FLAGS_THREADED)
#define frs_unthreaded(frs)	((frs)->flags &= ~FRS_FLAGS_THREADED)
#define frs_setthreaded(frs)	((frs)->flags |= FRS_FLAGS_THREADED)

#define _isMasterFrs(frs) ((frs)->flags & FRS_FLAGS_MASTER)
#define _setMasterFrs(frs) ((frs)->flags |= FRS_FLAGS_MASTER)
#define _unMasterFrs(frs) ((frs)->flags &= ~FRS_FLAGS_MASTER)
#define _setSlaveFrs(frs) _unMasterFrs(frs)

#define frs_get_cpumask(frs)  ((frs)->sdesc->cpumask)

#define frs_get_rmode(frs)    ((frs)->sdesc->rdesc.rmode)
#define frs_get_tmode(frs)    ((frs)->sdesc->rdesc.tmode)
#define frs_get_mcerr(frs)    ((frs)->sdesc->rdesc.maxcerr)
#define frs_get_xtime(frs)    ((frs)->sdesc->rdesc.xtime)

#define frs_get_dsync(frs)    ((frs)->sdesc->destroy_sync)

#define frs_set_rmode(frs, v)    ((frs)->sdesc->rdesc.rmode = (v))
#define frs_set_tmode(frs, v)    ((frs)->sdesc->rdesc.tmode = (v))
#define frs_set_mcerr(frs, v)    ((frs)->sdesc->rdesc.maxcerr = (v))
#define frs_set_xtime(frs, v)    ((frs)->sdesc->rdesc.xtime = (v))

/*
 * Support for user drivers
 */

typedef struct frs_drvent {
	void (*frs_func_set)(intrgroup_t*);
	void (*frs_func_clear)(void);
} frs_drvent_t;

#define FRS_MAXDRVENTS 16 

typedef struct frs_drvtable {
	frs_drvent_t frs_drvent[FRS_MAXDRVENTS];
	lock_t lock;
} frs_drvtable_t;

extern frs_drvtable_t frs_drvtable;


/*
 * Intr source accessors
 */

#define         _isIntTimerIntrSource(intr_source) \
                           ( ((intr_source) == FRS_INTRSOURCE_CPUTIMER) || \
			     ((intr_source) == FRS_INTRSOURCE_CCTIMER) )

#define         _isIntTimerIdescSource(idesc) \
                           ( ((idesc)->source == FRS_INTRSOURCE_CPUTIMER) || \
			     ((idesc)->source == FRS_INTRSOURCE_CCTIMER) )

#define         frs_get_intrgroup(frs) \
                           ((frs)->intrgroup)

/*
 * Internal Error Codes -- Must be power of 2, up to sizeof(uint) bits
 */
  
#define FRS_ERROR_OVERRUN          1
#define FRS_ERROR_UNDERRUN         2
#define FRS_ERROR_SYNCTERM         4
#define FRS_ERROR_FENCE            8

#define FRS_SIGNAL_UNDERRUN        SIGUSR1
#define FRS_SIGNAL_OVERRUN         SIGUSR2
#define FRS_SIGNAL_UNFRAMESCHED    SIGRTMIN
#define FRS_SIGNAL_ISEQUENCE       SIGRTMIN+1
#define FRS_SIGNAL_DEQUEUE         0
  
#define FRS_RTSLOW_RATE            0xf0000000
#define FRS_CC10MS                 476191    

#define MAX_GRAPH_PIPES            3

extern frs_fsched_t* frs_fsched_create(frs_fsched_info_t*,
				       frs_intr_info_t*, int*);
extern void frs_fsched_destroy(frs_fsched_t*);
extern int frs_fsched_enqueue(frs_fsched_t*, frs_threadid_t*, int, int);
extern int frs_fsched_join(frs_fsched_t*, long*);
extern int frs_fsched_start(frs_fsched_t*);
extern void frs_fsched_step(frs_fsched_t*);
extern int frs_fsched_yield(frs_fsched_t*, long*);
extern int frs_fsched_getqueuelen(frs_fsched_t*, int, long*);
extern int frs_fsched_readqueue(frs_fsched_t*, int, pid_t*, int);
extern int frs_fsched_premove(frs_fsched_t*, int, frs_threadid_t*);
extern int frs_fsched_pinsert(frs_fsched_t*, int, tid_t, frs_threadid_t*, int);
extern int frs_fsched_resume(frs_fsched_t*);
extern int frs_fsched_stop(frs_fsched_t*);
extern int frs_fsched_setrecovery(frs_fsched_t*, frs_recv_info_t*);
extern int frs_fsched_getrecovery(frs_fsched_t*, frs_recv_info_t*);
extern int frs_fsched_set_signals(frs_fsched_t*, frs_signal_info_t*);
extern int frs_fsched_get_signals(frs_fsched_t*, frs_signal_info_t*);
extern int frs_fsched_getoverruns(frs_fsched_t*, int, tid_t tid,
                                  frs_overrun_info_t*);
extern int frs_fsched_sdesc_spaceavail(frs_fsched_t*);
extern int frs_fsched_sdesc_insertmaster(frs_fsched_t*);
extern int frs_fsched_sdesc_insertslave(frs_fsched_t*);
extern void frs_fsched_sdesc_clear_slentry(frs_fsched_t*);
extern void  frs_fsched_sdesc_clear_masterentry(frs_fsched_t*);
extern int frs_fsched_sdesc_settermination(frs_fsched_t*);
extern int frs_fsched_sdesc_terminating(frs_fsched_t*);
extern void frs_fsched_sdesc_broadcasttermination(frs_fsched_t*);

extern void frs_fsched_isequence_error(frs_fsched_t*);

extern void frs_fsched_incrref(frs_fsched_t*);
extern void frs_fsched_decrref(frs_fsched_t*);
extern void frs_fsched_waitref(frs_fsched_t*);

extern void frs_fsched_eventintr(void);

extern int frs_fsched_state_verify_canintr(frs_fsched_t*);
extern int frs_fsched_state_verify_canyield(frs_fsched_t*);

#endif /*  __SYS_FRAME_SCHED_H__ */
