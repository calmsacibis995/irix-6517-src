/**************************************************************************
 *									  *
 * Copyright (C) 1997 Silicon Graphics, Inc.				  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <sync.h>
#include <pthread.h>
#include <semaphore_internal.h>
#include <priv_extern.h>

/*
 * Semaphore reference counting macros
 */

#define SEM_REF_GRAB(s)		(void) __add_and_fetch(&(s)->sem_refs, 1)
#define SEM_REF_DROP(s)		(void) __add_and_fetch(&(s)->sem_refs, -1)
#define SEM_REF_PENDING(s)	((s)->sem_refs > 0 ? 1 : 0)

/*
 * Semaphore Trace MACROS
 */

#define SEM_TRACE_ENABLED(sp) \
	(sp->sem_flags & (SEM_FLAGS_METER|SEM_FLAGS_DEBUG|SEM_FLAGS_HISTORY))

#define SEM_HISTORY(sp, cmd, arg) \
  if ((((ushdr_t*)sp->sem_usptr)->u_histon & _USSEMAHIST) && \
      (sp->sem_flags & SEM_FLAGS_HISTORY)) \
    hlog(sp, cmd, get_pid(), arg, (char *) __return_address, sp->sem_value);

/*
 * Semaphore Meter MACROS and Commands
 */

#define SM_BLOCK	sem_xtrace->sem_waiters
#define SM_WAIT		sem_xtrace->sem_waits
#define SM_POST		sem_xtrace->sem_posts
#define SM_NOWAITERS	sem_xtrace->sem_posthits
#define SM_HIT		sem_xtrace->sem_waithits

#define SEM_METER(sp, cmd, val)	 	 		\
{	 		 	 	 		\
	if (sp->sem_flags & SEM_FLAGS_METER)		\
		(void) __add_and_fetch(&sp->cmd, val);	\
} 

/*
 * Semaphore Trace MACROS and Commands
 */

#define SD_LAST  1
#define SD_OWNER 2

#define SEM_DEBUG(sp, cmd)						\
{									\
	sem_trace_t *xt;						\
	int cmd2 = cmd;	/* compiler WAR */				\
									\
	if (sp->sem_flags & SEM_FLAGS_DEBUG) {				\
		xt = sp->sem_xtrace;					\
		spin_lock((spinlock_t*) &xt->__st_l);			\
		if (cmd2 & SD_LAST) {					\
			xt->sem_last_pid = get_pid();			\
			xt->sem_last_tid = pthread_self();		\
			xt->sem_last_pc = (char *) __return_address;	\
		}							\
		if (cmd2 & SD_OWNER) {					\
			xt->sem_owner_pid = get_pid();			\
			xt->sem_owner_tid = pthread_self();		\
			xt->sem_owner_pc = (char *) __return_address;	\
		}							\
		spin_unlock((spinlock_t*) &xt->__st_l);			\
	}								\
}

#endif /* __DEBUG_H__ */
