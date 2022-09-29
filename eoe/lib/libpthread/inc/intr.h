#ifndef _INTR_H_
#define _INTR_H_

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* Global names
 */
#define intr_bootstrap		PFX_NAME(intr_bootstrap)
#define intr_notify		PFX_NAME(intr_notify)
#define intr_check		PFX_NAME(intr_check)
#define intr_destroy_sync	PFX_NAME(intr_destroy_sync)

#include "sys.h"
#include "pt.h"

#include <mutex.h>

typedef enum {
	INTR_CANCEL = 1,		/* called from pthread_cancel() */
	INTR_SIGNAL			/* called from pthread_kill() */
} intr_reasons_t;

struct pt;

extern void 	intr_bootstrap(void);
extern void 	intr_notify(struct pt *, intr_reasons_t);
extern int 	intr_check(int);


/* Postpone/resume destruction of any wait lock (condition variable or mutex).
 */
extern unsigned long	intr_destroy_sync;
#define intr_destroy_disable()	(void)test_then_add(&intr_destroy_sync, 1L);
#define intr_destroy_enable()	(void)test_then_add(&intr_destroy_sync, -1uL);
#define intr_destroy_wait()	intr_destroy_sync


#define PT_CNCL_THRESHOLD	0x10000		/* upper 16 bit counter */
#define PT_CNCLPOINT(pt)	(pt->pt_blocked  >= PT_CNCL_THRESHOLD)
#define PT_BLOCKED(pt)		(pt->pt_blocked & (0x10000 - 1))

/* Act on any pending interrupts; on return we are in the scheduler
 * with the pthread lock held.
 */
#define PT_INTR_PENDING(self, intr)			\
	for (;;) {					\
		sched_enter();				\
		lock_enter(&(self)->pt_lock);		\
		if (!((self)->pt_flags & (intr))) {	\
			break;				\
		}					\
		lock_leave(&(self)->pt_lock);		\
		sched_leave();				\
							\
		(void)intr_check(intr);			\
	}


#endif /* !_INTR_H_ */
