#ifndef _RWL_H_
#define	_RWL_H_

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

#include "q.h"
#include "mtx.h"
#include "rwlattr.h"

/*
 * Read-write lock data
 */
typedef struct rwl {
	union {
		/* PTHREAD_PROCESS_PRIVATE rwlock data members */
		struct {
			slock_t		prv_slock;	/* simple lock */
			q_t		prv_rdwaitq;	/* read wait queue */
			q_t		prv_wrwaitq;	/* write wait queue */
			int		prv_busybit;	/* waiter indicator */
		} un_rwlprivate;

		/* PTHREAD_PROCESS_SHARED rwlock data members */
		struct {
			mtx_t		shr_mtx;	/* mutex for sync */
			int		shr_rwaiters;	/* read waiters */
			unsigned int	shr_wwaiters;	/* write waiters */
		} un_rwlshared;
	} rwl_un;
	int		rwl_rdcount;	/* read count */
	struct rwlattr	rwl_attr;	/* read-write lock attributes */
} rwl_t;


/*
 * Process Private union member defines
 */
#define rwl_slock	rwl_un.un_rwlprivate.prv_slock
#define rwl_rdwaitq	rwl_un.un_rwlprivate.prv_rdwaitq
#define rwl_wrwaitq	rwl_un.un_rwlprivate.prv_wrwaitq
#define rwl_busybit	rwl_un.un_rwlprivate.prv_busybit

/*
 * Process Shared union member defines
 */
#define rwl_mtx		rwl_un.un_rwlshared.shr_mtx
#define rwl_rwaiters	rwl_un.un_rwlshared.shr_rwaiters
#define	rwl_wwaiters	rwl_un.un_rwlshared.shr_wwaiters

#endif /* !_RWL_H_ */
