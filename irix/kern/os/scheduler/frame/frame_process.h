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

#ifndef __FRAME_THREAD_H__
#define __FRAME_THREAD_H__

#include <sys/uthread.h>

/*
 * The Global Data Structure View:
 *
 *                                          Minor Frames
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/ frs_minor_t   frs_prod_t
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 *                  |      |    |--|              |   |   [*]<--[ ]<--[ ]<--
 *                  --------    |  |              -----    \
 *                              |--|                        \
 *                frs_major_t   |  | etc.                    --> uthread*
 *                              |--|
 *                              
 */

#include "frame_state.h"

/*
 * FRS Thread ID defines and MACROS
 */

typedef struct frs_threadid {
	pid_t		pid;
	uthreadid_t	utid;
} frs_threadid_t;

#define frs_tid_to_threadid(tid, threadid)				\
	if ((tid) & 0x80000000) {					\
		(threadid)->pid = current_pid();			\
		(threadid)->utid = (uthreadid_t) ((tid) & ~0x80000000);	\
	} else {							\
		(threadid)->pid = (tid);				\
		(threadid)->utid = UT_ID_NULL;				\
	}

#define frs_threadid_to_tid(threadid)			\
	(((threadid)->utid == UT_ID_NULL) ? (threadid)->pid : (threadid)->utid)

#define frs_threadid_clear(threadid)			\
	(threadid)->pid = -1;				\
	(threadid)->utid = UT_ID_NULL;

#define frs_threadid_valid(threadid)			\
	((threadid)->pid != -1 ? 1 : 0)

/*
 * FRS Thread lock primatives
 */
#if defined(IP30)
#define frs_thread_lock(ut)	mutex_spinlock_spl(&(ut)->ut_frslock, splprof)
#else
#define frs_thread_lock(ut)	mutex_spinlock(&(ut)->ut_frslock)
#endif

#define frs_thread_unlock(p, s)	mutex_spinunlock(&(ut)->ut_frslock, (s))

/*
 * FRS Thread Protos
 */

struct frs_procd;

extern int frs_thread_linkfrs(uthread_t*, struct frs_fsched*, int*);
extern void frs_thread_unlinkfrs(uthread_t*, struct frs_procd*);
extern int frs_thread_join(uthread_t*);
extern int frs_thread_yield(uthread_t*);
extern int frs_thread_controller_access(uthread_t*);
extern int frs_thread_access(uthread_t*);
extern void frs_thread_controller_unset(uthread_t*);
extern void frs_thread_controller_linkme(struct frs_fsched*);
extern void frs_thread_controller_unlinkme(struct frs_fsched*);
extern uthread_t* frs_thread_controller_getthread(struct frs_fsched*);
extern struct frs_fsched* frs_thread_controller_getfrs(frs_threadid_t *, int*);
extern struct frs_fsched* frs_thread_getfrs(void);
extern int frs_thread_starter_test(uthread_t*);
extern uthread_t* frs_thread_ref_acquire(frs_threadid_t *);
extern void frs_thread_ref_release(uthread_t*);
extern void frs_thread_sendsig(frs_threadid_t *, int);
#endif /* __FRAME_THREAD_H__ */
