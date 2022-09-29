#ifdef _LIBMUTEX_ABI

#ifdef __STDC__
        #pragma weak acquire_lock = _acquire_lock
        #pragma weak init_lock = _init_lock
        #pragma weak release_lock = _release_lock
        #pragma weak stat_lock = _stat_lock
        #pragma weak spin_lock = _spin_lock
#endif

#define init_lock       _init_lock
#define acquire_lock    _acquire_lock
#define release_lock    _release_lock
#define stat_lock       _stat_lock
#define spin_lock       _spin_lock


#include "abi_mutex.h"

/* ABI mutex functions */
/* ARGSUSED */
int
init_lock(abilock_t *addr)
{
        return 0;
}

/* ARGSUSED */
int
acquire_lock(abilock_t *addr)
{
        return 0;
}

/* ARGSUSED */
int
release_lock(abilock_t *addr)
{
        return 0;
}

/* ARGSUSED */
int
stat_lock(abilock_t *addr)
{
        return 0;
}

/* ARGSUSED */
void
spin_lock(abilock_t *addr)
{
        return;
}

#else /* _LIBMUTEX_ABI */

/* Nothing here! - all routines in libc */
void
__mutex_dummy(void){}

#endif /* _LIBMUTEX_ABI */
