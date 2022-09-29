#ifndef _MTX_H_
#define	_MTX_H_

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

/*
 * Mutex visible synchronization operations
 */

/* Global names
 */
#define lock_bootstrap		PFX_NAME(lock_bootstrap)
#define libc_locks_init		PFX_NAME(libc_locks_init)
#define mtx_bootstrap		PFX_NAME(mtx_bootstrap)
#define mtx_giveinheritance	PFX_NAME(mtx_giveinheritance)
#define lock_tryenter		PFX_NAME(lock_tryenter)
#define sl_lock			PFX_NAME(sl_lock)
#define lock_leave		PFX_NAME(lock_leave)
#define lock_held		PFX_NAME(lock_held)
#define mtx_get_owner		PFX_NAME(mtx_get_owner)
#define libc_fork_prepare	PFX_NAME(libc_fork_prepare)
#define libc_fork_parent	PFX_NAME(libc_fork_parent)
#define libc_fork_child		PFX_NAME(libc_fork_child)
#define pt_usync_cntl		PFX_NAME(pt_usync_cntl)


#include "q.h"
#include "mtxattr.h"

/*
 * Simple lock data
 */
typedef struct slock {
	ulong_t	sl_lockwd;
} slock_t;


/*
 * Mutex data
 */
typedef struct mtx {
	union {
		/* PTHREAD_PROCESS_PRIVATE mutex data members */
		struct {
			slock_t		prv_slock;	/* simple lock */
			q_t		prv_waitq;	/* waiters */
			struct pt	*prv_owner;	/* owner */
		} un_pprivate;

		/* PTHREAD_PROCESS_SHARED mutex data members */
		struct {
			struct pt	*shr_thread;	/* owning thread */
			pid_t		shr_pid;	/* owning process */
			unsigned int	shr_waiters;	/* waiters flag */
		} un_pshared;
	} mtx_un;
	struct mtx	*mtx_inhprev;	/* inheritance */
	struct mtx	*mtx_inhnext;	/*    queue    */
	int		mtx_lckcount;	/* lock count */
	struct mtxattr	mtx_attr;	/* mutex attributes */
} mtx_t;


/*
 * Process Private union member defines
 */
#define	mtx_slock	mtx_un.un_pprivate.prv_slock
#define	mtx_waitq	mtx_un.un_pprivate.prv_waitq
#define	mtx_owner	mtx_un.un_pprivate.prv_owner

/* Locking note:
 * mtx_waitq is protected by the mutex interlock.
 * mtx_inh* are protected by the owning pthread's lock.
 */

/*
 * Process Shared union member defines
 */
#define	mtx_thread	mtx_un.un_pshared.shr_thread
#define	mtx_pid		mtx_un.un_pshared.shr_pid
#define	mtx_waiters	mtx_un.un_pshared.shr_waiters

struct pt;
extern void	mtx_giveinheritance(struct mtx *, schedpri_t);

#define MTX_INHERITPRI(pt)				\
	(pt)->pt_minhnext == (mtx_t *)(pt) ?		\
		PX_PRIO_MIN :			\
		(pt)->pt_minhnext->mtx_attr.ma_priority


extern void	mtx_bootstrap(void);
extern int	libc_locks_init(void);
extern void	libc_fork_prepare(void);
extern void	libc_fork_parent(void);
extern void	libc_fork_child(void);
#define lock_init(sl)	(sl)->sl_lockwd = 0;

/* Simple lock sp/mp switches
 */
extern void	lock_bootstrap(void);
extern int	lock_tryenter(slock_t *);
extern void	(*sl_lock)(slock_t *);
extern void	lock_leave(slock_t *);
extern int	lock_held(slock_t *);

#define lock_enter	(*sl_lock)

extern struct pt	*mtx_get_owner(struct mtx *);

#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700)
#define SYNCHRONIZE()	__synchronize()
#else
#define	SYNCHRONIZE()
#endif

/*
 * Underlying blocking mechanisms
 * for pshared mutexes and condition variables
 */
extern int pt_usync_cntl(int, void *, int);

#endif /* !_MTX_H_ */
