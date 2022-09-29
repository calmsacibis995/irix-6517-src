/* Copyright (C) 1989-1992 Silicon Graphics, Inc. All rights reserved.  */

#ifndef _SYS_KSYNCH_H
#define _SYS_KSYNCH_H

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.7 $"

#include <sys/sema.h>

typedef struct {
	char *lk_name;		/* lock name */
	int  lk_flags;		/* flags */
	long lk_pad[2];		/* for future expansion */
} lkinfo_t;

typedef sema_t sleep_t;		/* the sleep lock */
typedef mrlock_t rwlock_t;	/* the sleep lock */

#define LOCK_ALLOC 	lock_alloc
#define LOCK_DEALLOC 	lock_dealloc
#define LOCK_INIT 	lock_init
#define LOCK_DESTROY 	lock_destroy
#define LOCK		ddi_lock
#define UNLOCK		ddi_unlock
#define TRYLOCK		trylock

#define SLEEP_ALLOC	sleep_alloc
#define SLEEP_INIT	sleep_init
#define SLEEP_DEALLOC	sleep_dealloc
#define SLEEP_LOCK	sleep_lock
#define SLEEP_LOCK_SIG	sleep_lock_sig
#define SLEEP_TRYLOCK	sleep_trylock
#define SLEEP_UNLOCK	sleep_unlock
#define SLEEP_LOCKAVAIL sleep_lockavail

#define MUTEX_ALLOC	ddi_mutex_alloc
#define MUTEX_INIT	mutex_init
#define MUTEX_DEALLOC	mutex_dealloc
#define MUTEX_DESTROY	mutex_destroy

#define MUTEX_LOCK	ddi_mutex_lock
#define MUTEX_TRYLOCK	mutex_trylock
#define MUTEX_UNLOCK	mutex_unlock

#define MUTEX_OWNED	mutex_owned
#define MUTEX_MINE	mutex_mine

#define SV_ALLOC	sv_alloc
#define SV_INIT		sv_init
#define SV_DEALLOC	sv_dealloc
#define SV_SIGNAL	sv_signal
#define SV_BROADCAST	sv_broadcast
#define SV_WAIT		ddi_sv_wait
#define SV_WAIT_SIG	ddi_sv_wait_sig

#define RW_ALLOC	rw_alloc
#define RW_INIT		mrinit
#define RW_DEALLOC	rw_dealloc
#define RW_DESTROY	mrfree
#define RW_RDLOCK	rw_rdlock
#define RW_TRYRDLOCK	rw_tryrdlock
#define RW_WRLOCK	rw_wrlock
#define RW_TRYWRLOCK	rw_trywrlock
#define RW_UNLOCK	mrunlock

extern pl_t plbase;
extern pl_t plbase;
extern pl_t pltimeout;
extern pl_t pldisk;
extern pl_t plstr;
extern pl_t plhi;

extern mutex_t	*ddi_mutex_alloc(int, int, char *);
extern void	ddi_mutex_lock(mutex_t *, int );

extern lock_t 	*lock_alloc(uchar_t, pl_t, lkinfo_t *, int);
extern void 	lock_init(lock_t *, uchar_t, pl_t, lkinfo_t *);
extern void 	lock_dealloc(lock_t *);
extern void 	lock_destroy(lock_t *);
extern int 	ddi_lock(lock_t *, pl_t);
extern void	ddi_unlock(lock_t *, int);
extern int	trylock(lock_t *, pl_t);

extern sleep_t  *sleep_alloc(int, lkinfo_t *, int);
extern void	sleep_dealloc(sleep_t *);
extern void	sleep_lock(sleep_t *, int);
extern boolean_t sleep_lock_sig(sleep_t *, int);
extern boolean_t sleep_trylock(sleep_t *);
extern void	sleep_unlock(sleep_t *);
extern boolean_t sleep_lockavail(sleep_t *);

extern void	ddi_sv_wait(sv_t *, void *, int);
extern int	ddi_sv_wait_sig(sv_t *, void *, int);

#define INVPL 	-1


#ifdef __cplusplus
}
#endif

#endif /* _SYS_KSYNCH_H */
