/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: rpcmutex.c,v 65.6 1998/03/23 17:28:11 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcmutex.c,v $
 * Revision 65.6  1998/03/23 17:28:11  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/02/26 16:53:04  lmc
 * Change to a prototype from volatile to non-volatile.  #ifdef'd around
 * a variable that isn't used.
 *
 * Revision 65.4  1998/01/07  17:21:04  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:05  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:11  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.349.2  1996/02/18  00:05:36  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:57:23  marty]
 *
 * Revision 1.1.349.1  1995/12/08  00:22:09  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:35 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/07/14  19:06 UTC  tatsu_s
 * 	Submitted the fix for CHFts15608.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b4/3  1995/07/11  16:26 UTC  tatsu_s
 * 	Added more volatile.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b4/2  1995/07/10  20:32 UTC  tatsu_s
 * 	Added volatile.
 * 
 * 	HP revision /main/tatsu_s.local_rpc.b4/1  1995/07/06  21:11 UTC  tatsu_s
 * 	Use the serviceability macro.
 * 	[1995/12/08  00:00:28  root]
 * 
 * Revision 1.1.347.2  1994/05/19  21:15:02  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:44:16  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:28:48  hopkins]
 * 
 * Revision 1.1.347.1  1994/01/21  22:39:14  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:58  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:54:58  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:12:25  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:16:16  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:44:07  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:48  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      rpcmutex.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  The support routines for rpcmutex.h abstraction.  These should NOT
**  be called directly; use the macros in rpcmutex.h .
**
**
*/

#include <commonp.h>

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

/*
 * All of the routines return true on success, false on failure.
 *
 * There are races present on the modifications of the package wide
 * statistics as well as the per lock "busy", "try_lock" and "*_assert"
 * statistics, but we don't really care.  We don't want to burden these
 * "informative" statistics with some other mutex.  We're only providing
 * these stats so we can track gross trends.
 */


/*
 * !!! Since CMA pthreads doesn't provide a "null" handle, create our own.
 */
PRIVATE pthread_t rpc_g_null_thread_handle;
#define NULL_THREAD rpc_g_null_thread_handle

#define IS_MY_THREAD(t) pthread_equal((t), my_thread)

/*
 * Some package wide statistics.
 */

INTERNAL rpc_mutex_stats_t mutex_stats = {0};
INTERNAL rpc_cond_stats_t cond_stats = {0};


/*
 * R P C _ _ M U T E X _ I N I T
 */

PRIVATE boolean rpc__mutex_init
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp
)
#else
(mp) 
rpc_mutex_p_t mp;
#endif
{
    mp->stats.busy = 0;
    mp->stats.lock = 0;
    mp->stats.try_lock = 0;
    mp->stats.unlock = 0;
    mp->stats.init = 1;
    mp->stats.delete = 0;
    mp->stats.lock_assert = 0;
    mp->stats.unlock_assert = 0;
    mp->is_locked = false;
    mp->owner = NULL_THREAD;
    mp->locker_file = "never_locked";
    mp->locker_line = 0;
    pthread_mutex_init(&mp->m, pthread_mutexattr_default);
    mutex_stats.init++;
    return(true);
}


/*
 * R P C _ _ M U T E X _ D E L E T E
 */

PRIVATE boolean rpc__mutex_delete
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp
)
#else
(mp) 
rpc_mutex_p_t mp;
#endif
{
    mp->stats.delete++;
    mutex_stats.delete++;
    pthread_mutex_destroy(&mp->m);
    return(true);
}


/*
 * R P C _ _ M U T E X _ L O C K
 */

PRIVATE boolean rpc__mutex_lock
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp,
    char *file,
    int line
)
#else
(mp, file, line) 
rpc_mutex_p_t mp;
char *file;
int line;
#endif
{
    pthread_t my_thread;
    boolean is_locked = mp->is_locked;
    boolean dbg;

    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    if (dbg) 
    {
        my_thread = pthread_self();
        if (is_locked && IS_MY_THREAD(mp->owner))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_lock) deadlock with self at %s/%d (previous lock at %s/%d)\n",
                file, line, mp->locker_file, mp->locker_line));
            return(false);
        }
    }
    pthread_mutex_lock(&mp->m);
    mp->is_locked = true; 
    if (dbg)
    {
        mp->owner = my_thread;
        mp->locker_file = file;
        mp->locker_line = line;
    }
    if (is_locked)
    {
        mp->stats.busy++;
        mutex_stats.busy++;
    }
    mp->stats.lock++;
    mutex_stats.lock++;
    return(true);
}


/*
 * R P C _ _ M U T E X _ T R Y _ L O C K
 */

PRIVATE boolean rpc__mutex_try_lock
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp,
    boolean *bp,
    char *file,
    int line
)
#else
(mp, bp, file, line) 
rpc_mutex_p_t mp;
boolean *bp;
char *file;
int line;
#endif
{
    pthread_t my_thread;
    boolean is_locked = mp->is_locked;
    boolean dbg;

    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    if (dbg) 
    {
        my_thread = pthread_self();
        if (is_locked && IS_MY_THREAD(mp->owner))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_try_lock) deadlock with self at %s/%d (previous lock at %s/%d)\n",
                file, line, mp->locker_file, mp->locker_line));
            return(false);
        }
    }
    *bp = pthread_mutex_trylock(&mp->m);
    if (*bp)
    {
        mp->is_locked = true;
        if (dbg)
        {
            mp->owner = my_thread;
            mp->locker_file = file;
            mp->locker_line = line;
        }
    }
    else
    {
        mp->stats.busy++;
        mutex_stats.busy++;
    }
    mp->stats.try_lock++;
    mutex_stats.try_lock++;
    return(true);
}


/*
 * R P C _ _ M U T E X _ U N L O C K
 */

PRIVATE boolean rpc__mutex_unlock
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp
)
#else
(mp) 
rpc_mutex_p_t mp;
#endif
{
    pthread_t my_thread;
    boolean is_locked = mp->is_locked;
    boolean dbg;

    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    if (dbg) 
    {
        if (! is_locked)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_unlock) not locked\n"));
            return(false);
        }
        my_thread = pthread_self();
        if (!IS_MY_THREAD(mp->owner))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_unlock) not owner (owner at %s/%d)\n",
                mp->locker_file, mp->locker_line));
            return(false);
        }
        mp->owner = NULL_THREAD;
    }
    mp->stats.unlock++;
    mutex_stats.unlock++;
    mp->is_locked = false;
    pthread_mutex_unlock(&mp->m);
    return(true);
}


/*
 * R P C _ _ M U T E X _ L O C K _ A S S E R T
 *
 * assert that we are the owner of the lock.
 */

PRIVATE boolean rpc__mutex_lock_assert
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp
)
#else
(mp) 
rpc_mutex_p_t mp;
#endif
{
    pthread_t my_thread;
    boolean is_locked = mp->is_locked;
    boolean dbg;

    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    mp->stats.lock_assert++;
    mutex_stats.lock_assert++;
    if (dbg)
    {
        if (! is_locked)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_lock_assert) not locked\n"));
            return(false);
        }
        my_thread = pthread_self();
        if (!IS_MY_THREAD(mp->owner))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
	       ("(rpc__mutex_lock_assert) not owner\n"));
            return(false);
        }
    }
    return(true);
}


/*
 * R P C _ _ M U T E X _ U N L O C K _ A S S E R T
 *
 * assert that we are not the owner of the lock.
 */

PRIVATE boolean rpc__mutex_unlock_assert
#ifdef _DCE_PROTO_
(
    rpc_mutex_p_t mp
)
#else
(mp) 
rpc_mutex_p_t mp;
#endif
{
    pthread_t my_thread;
    boolean is_locked = mp->is_locked;
    boolean dbg;

    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    mp->stats.unlock_assert++;
    mutex_stats.unlock_assert++;
    if (dbg)
    {
        if (! is_locked)
            return(true);
        my_thread = pthread_self();
        if (IS_MY_THREAD(mp->owner))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__mutex_unlock_assert) owner\n"));
            return(false);
        }
    }
    return(true);
}


/*
 * R P C _ _ C O N D _ I N I T
 *
 * The "mp" is the mutex that is associated with the cv.
 */

boolean rpc__cond_init
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    rpc_mutex_p_t mp
)
#else
(cp, mp)
rpc_cond_p_t cp;
rpc_mutex_p_t mp;
#endif
{
    cp->stats.init = 1;
    cp->stats.delete = 0;
    cp->stats.wait = 0;
    cp->stats.signals = 0;
    cp->mp = mp;
    pthread_cond_init(&cp->c, pthread_condattr_default);
    cond_stats.init++;
    return(true);
}


/*
 * R P C _ _ C O N D _ D E L E T E
 */

boolean rpc__cond_delete
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    rpc_mutex_p_t mp
)
#else
(cp, mp)
rpc_cond_p_t cp;
rpc_mutex_p_t mp;
#endif
{
    cp->stats.delete++;
    cond_stats.delete++;
    pthread_cond_destroy(&cp->c);
    return(true);
}


/*
 *  R P C _ _ C O N D _ W A I T
 *
 * The mutex is automatically released and reacquired by the wait.
 */

boolean rpc__cond_wait
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    volatile rpc_mutex_p_t mp,
#ifdef SGIMIPS
    char *file,			/*  mp should be volatile, but file is
					not changed. */
#else
    volatile char *file,
#endif
    volatile int line
)
#else
(cp, mp, file, line)
rpc_cond_p_t cp;
volatile rpc_mutex_p_t mp;
volatile char *file;
volatile int line;
#endif
{
    volatile pthread_t my_thread;
    volatile boolean dbg;

    cp->stats.wait++;
    cond_stats.wait++;
    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    if (dbg)
    {
        if (! rpc__mutex_lock_assert(mp))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__cond_wait) mutex usage error\n"));
            return(false);
        }
        if (cp->mp != mp)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__cond_wait) incorrect mutex\n"));
            return(false);
        }
        my_thread = pthread_self();
        mp->owner = NULL_THREAD;
    }
    mp->is_locked = false;
    TRY
        pthread_cond_wait(&cp->c, &mp->m);
    CATCH_ALL
        mp->is_locked = true;
        if (dbg)
        {
            mp->owner = my_thread;
            mp->locker_file = file;
            mp->locker_line = line;
        }
        RERAISE;
    ENDTRY
    mp->is_locked = true;
    if (dbg)
    {
        mp->owner = my_thread;
        mp->locker_file = file;
        mp->locker_line = line;
    }
    return(true);
}


/*
 *  R P C _ _ C O N D _ T I M E D _ W A I T
 *
 * The mutex is automatically released and reacquired by the wait.
 */

boolean rpc__cond_timed_wait
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    volatile rpc_mutex_p_t mp,
    struct timespec *wtime,
#ifdef SGIMIPS
    char *file,		/* file is used but not changed. */
#else
    volatile char *file,
#endif
    volatile int line
)
#else
(cp, mp, wtime, file, line)
rpc_cond_p_t cp;
volatile rpc_mutex_p_t mp;
struct timespec *wtime;
volatile char *file;
volatile int line;
#endif
{
    volatile pthread_t my_thread;
    volatile boolean dbg;
#ifdef KERNEL
    extern int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
	struct timespec *);
#endif /* KERNEL */

    cp->stats.wait++;
    cond_stats.wait++;
    dbg = RPC_DBG2(rpc_e_dbg_mutex, 5);
    if (dbg)
    {
        if (! rpc__mutex_lock_assert(mp))
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__cond_wait) mutex usage error\n"));
            return(false);
        }
        if (cp->mp != mp)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_mutex, 1,
		("(rpc__cond_wait) incorrect mutex\n"));
            return(false);
        }
        my_thread = pthread_self();
        mp->owner = NULL_THREAD;
    }
    mp->is_locked = false;
    TRY
        pthread_cond_timedwait(&cp->c, &mp->m, wtime);
    CATCH_ALL
        mp->is_locked = true;
        if (dbg)
        {
            mp->owner = my_thread;
            mp->locker_file = file;
            mp->locker_line = line;
        }
        RERAISE;
    ENDTRY
    mp->is_locked = true;
    if (dbg)
    {
        mp->owner = my_thread;
        mp->locker_file = file;
        mp->locker_line = line;
    }
    return(true);
}


/*
 *  R P C _ _ C O N D _ S I G N A L
 *
 * It's not clear if it's legal to signal w/o holding the lock
 * (in the runtime's context);  CMA clearly doesn't require it.
 */

boolean rpc__cond_signal
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    rpc_mutex_p_t mp
)
#else
(cp, mp)
rpc_cond_p_t cp;
rpc_mutex_p_t mp;
#endif
{
    cp->stats.signals++;
    cond_stats.signals++;
    pthread_cond_signal(&cp->c);
    return(true);
}


/*
 *  R P C _ _ C O N D _ B R O A D C A S T
 *
 * It's not clear if it's legal to broadcast w/o holding the lock
 * (in the runtime's context);  CMA clearly doesn't require it.
 */

boolean rpc__cond_broadcast
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    rpc_mutex_p_t mp
)
#else
(cp, mp)
rpc_cond_p_t cp;
rpc_mutex_p_t mp;
#endif
{
    cp->stats.signals++;
    cond_stats.signals++;
    pthread_cond_broadcast(&cp->c);
    return(true);
}


#else
INTERNAL void rpc__mutex_none (void)
{
}
#endif /* defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS) */
