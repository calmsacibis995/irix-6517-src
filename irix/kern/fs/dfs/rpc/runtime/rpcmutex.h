/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcmutex.h,v $
 * Revision 65.3  1998/02/24 15:26:16  lmc
 * Removed volatile from function declarations.  It wasn't necessary and
 * generated warnings during compiles.
 *
 * Revision 65.2  1998/02/23  22:19:44  gwehrman
 * Added debugging information for mutexes and condition variables.
 * The added debuging is only present when compiled with DEBUG.
 *
 * Revision 65.1  1997/10/20 19:17:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.350.2  1996/02/18  22:56:55  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:55  marty]
 *
 * Revision 1.1.350.1  1995/12/08  00:22:11  root
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
 * 	[1995/12/08  00:00:29  root]
 * 
 * Revision 1.1.348.3  1994/06/21  21:54:18  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:34:15  hopkins]
 * 
 * Revision 1.1.348.2  1994/05/19  21:15:03  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:44:38  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:29:01  hopkins]
 * 
 * Revision 1.1.348.1  1994/01/21  22:39:15  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:59  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:55:01  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:12:29  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:16:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:44:11  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:11:32  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _RPCMUTEX_H
#define _RPCMUTEX_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      rpcmutex.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  A veneer over CMA (or anything else for that matter) to assist with
**  mutex and condition variable support for the runtime.  This provides
**  some isolation from the underlying mechanisms to allow us to (more)
**  easily slip in alternate facilities, and more importantly add features
**  that we'd like to have in the areas of correctness and debugging
**  of code using these locks as well as statistics gathering.  Note
**  that this package contains a "condition variable" veneer as well,
**  since condition variables and mutexes are tightly integrated (i.e.
**  condition variables have an associated mutex).
**
**  This package provides the following PRIVATE data types and operations:
**  
**      rpc_mutex_t m;
**      rpc_cond_t c;
**  
**      void RPC_MUTEX_INIT(m)
**      void RPC_MUTEX_DELETE(m)
**      void RPC_MUTEX_LOCK(m)
**      void RPC_MUTEX_TRY_LOCK(m,bp)
**      void RPC_MUTEX_UNLOCK(m)
**      void RPC_MUTEX_LOCK_ASSERT(m)
**      void RPC_MUTEX_UNLOCK_ASSERT(m)
**
**      void RPC_COND_INIT(c,m)
**      void RPC_COND_DELETE(c,m)
**      void RPC_COND_WAIT(c,m)
**      void RPC_COND_TIMED_WAIT(c,m,t)
**      void RPC_COND_SIGNAL(c,m)
**
**  This code is controlled by debug switch "rpc_e_dbg_mutex".  At levels 1
**  or higher, statistics gathering is enabled.  At levels 5 or higher,
**  assertion checking, ownership tracking, deadlock detection, etc. are
**  enabled.
**
**
*/

/*
 * The rpc_mutex_t data type.  
 *
 * If debugging isn't configured, this is just a unadorned mutex, else we adorn
 * the struct with goodies to enable assertion checking, deadlock detection
 * and statistics gathering.
 */

typedef struct rpc_mutex_stats_t 
{
    unsigned32 busy;            /* total lock requests when already locked */
    unsigned32 lock;            /* total locks */
    unsigned32 try_lock;        /* total try_locks */
    unsigned32 unlock;          /* total unlocks */
    unsigned32 init;            /* total inits */
    unsigned32 delete;          /* total deletes */
    unsigned32 lock_assert;     /* total lock_asserts */
    unsigned32 unlock_assert;
} rpc_mutex_stats_t, *rpc_mutex_stats_p_t;


typedef struct rpc_mutex_t 
{
    pthread_mutex_t m;          /* the unadorned mutex lock */
#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)
    unsigned is_locked:1;       /* T=> locked */
    pthread_t owner;            /* current owner if locked */
    char *locker_file;          /* last locker */
    int locker_line;            /* last locker */
    rpc_mutex_stats_t stats;
#endif /* RPC_MUTEX_DEBUG OR RPC_MUTEX_STATS */
} rpc_mutex_t, *rpc_mutex_p_t;


/*
 * The rpc_cond_t (condition variable) data type.
 */

typedef struct rpc_cond_stats_t
{
    unsigned32 init;            /* total inits */
    unsigned32 delete;          /* total deletes */
    unsigned32 wait;            /* total waits */
    unsigned32 signals;         /* total signals + broadcasts */
} rpc_cond_stats_t, *rpc_cond_stats_p_t;

typedef struct rpc_cond_t
{
    pthread_cond_t c;           /* the unadorned condition variable */
    rpc_mutex_p_t mp;           /* the cv's associated mutex */
    rpc_cond_stats_t stats;
} rpc_cond_t, *rpc_cond_p_t;



/*
 * Some relatively efficient generic mutex operations that are controllable
 * at run time as well as compile time.  The "real" support routines
 * can be found in rpcmutex.c .
 */


/*
 * R P C _ M U T E X _ I N I T
 *
 * We always need to call the support routine so that the stats, etc
 * get initialized.
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_INIT(mutex)  \
    { \
        if (! rpc__mutex_init(&(mutex))) \
        { \
	    RPC_DCE_SVC_PRINTF (( \
		DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		rpc_svc_mutex, \
		svc_c_sev_fatal | svc_c_action_abort, \
		rpc_m_call_failed_no_status, \
		"RPC_MUTEX_INIT/rpc__mutex_init" )); \
        } \
	else \
	{ \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("mutex %x initialized\n", &(mutex))); \
	} \
    }
#else
#  define RPC_MUTEX_INIT(mutex) \
    { \
        RPC_LOG_MUTEX_INIT_NTR; \
        pthread_mutex_init(&(mutex).m, pthread_mutexattr_default); \
        RPC_LOG_MUTEX_INIT_XIT; \
    }
#endif /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ D E L E T E
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)
#  define RPC_MUTEX_DELETE(mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_delete(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_DELETE/rpc__mutex_delete" )); \
            } \
	    else \
	    { \
		RPC_DBG_PRINTF ( \
		    rpc_e_dbg_mutex, \
		    8, \
		    ("mutex %x deleted\n", &(mutex))); \
	    } \
        } else { \
            pthread_mutex_destroy(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_DELETE(mutex) \
    { \
        RPC_LOG_MUTEX_DELETE_NTR; \
        pthread_mutex_destroy(&(mutex).m); \
        RPC_LOG_MUTEX_DELETE_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_LOCK(mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_lock(&(mutex), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_LOCK/rpc__mutex_lock" )); \
            } \
	    else \
	    { \
		RPC_DBG_PRINTF ( \
		    rpc_e_dbg_mutex, \
		    8, \
		    ("mutex %x locked\n", &(mutex))); \
	    } \
        } else { \
            RPC_LOG_MUTEX_LOCK_NTR; \
            pthread_mutex_lock(&(mutex).m); \
            RPC_LOG_MUTEX_LOCK_XIT; \
        } \
    }
#else
#  define RPC_MUTEX_LOCK(mutex) \
    { \
        RPC_LOG_MUTEX_LOCK_NTR; \
        pthread_mutex_lock(&(mutex).m); \
        RPC_LOG_MUTEX_LOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ T R Y _ L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_TRY_LOCK(mutex,bp)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_try_lock(&(mutex), (bp), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_TRY_LOCK/rpc__mutex_try_lock" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    8, \
                    ("mutex %x trylocked\n", &(mutex))); \
            } \
        } else { \
            *(bp) = pthread_mutex_trylock(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_TRY_LOCK(mutex,bp) \
    { \
        RPC_LOG_MUTEX_TRY_LOCK_NTR; \
        *(bp) = pthread_mutex_trylock(&(mutex).m); \
        RPC_LOG_MUTEX_TRY_LOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ U N L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_UNLOCK(mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_unlock(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_UNLOCK/rpc__mutex_unlock" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    8, \
                    ("mutex %x unlocked\n", &(mutex))); \
            } \
        } else { \
            pthread_mutex_unlock(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_UNLOCK(mutex) \
    { \
        RPC_LOG_MUTEX_UNLOCK_NTR; \
        pthread_mutex_unlock(&(mutex).m); \
        RPC_LOG_MUTEX_UNLOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ L O C K _ A S S E R T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_LOCK_ASSERT(mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_lock_assert(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_LOCK_ASSERT/rpc__mutex_lock_assert" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    9, \
                    ("mutex %x lock_asserted\n", &(mutex))); \
            } \
        } \
    }
#else
#  define RPC_MUTEX_LOCK_ASSERT(mutex)      { }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ U N L O C K _ A S S E R T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_UNLOCK_ASSERT(mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_unlock_assert(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_UNLOCK_ASSERT/rpc__mutex_unlock_assert" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    9, \
                    ("mutex %x unlock_asserted\n", &(mutex))); \
            } \
        } \
    }
#else
#  define RPC_MUTEX_UNLOCK_ASSERT(mutex)    { }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ C O N D _ I N I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_INIT(cond,mutex)  \
    { \
        if (! rpc__cond_init(&(cond), &(mutex))) \
        { \
	    RPC_DCE_SVC_PRINTF (( \
		DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		rpc_svc_mutex, \
		svc_c_sev_fatal | svc_c_action_abort, \
		rpc_m_call_failed_no_status, \
		"RPC_COND_INIT/rpc__cond_init" )); \
        } \
	else \
	{ \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("cond: %x, mutex: %x initialized\n", &(cond), &(mutex))); \
	} \
    }
#else
#  define RPC_COND_INIT(cond,mutex) \
    { \
        RPC_LOG_COND_INIT_NTR; \
        pthread_cond_init(&(cond).c, pthread_condattr_default); \
        RPC_LOG_COND_INIT_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 *  R P C _ C O N D _ D E L E T E
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_DELETE(cond,mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_delete(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_DELETE/rpc__cond_delete" )); \
            } \
	    else \
	    { \
		RPC_DBG_PRINTF ( \
		    rpc_e_dbg_mutex, \
		    8, \
		    ("cond: %x, mutex: %x deleted\n", &(cond), &(mutex))); \
	    } \
        } else { \
            pthread_cond_destroy(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_DELETE(cond,mutex) \
    { \
      RPC_LOG_COND_DELETE_NTR; \
      pthread_cond_destroy(&(cond).c); \
      RPC_LOG_COND_DELETE_XIT; \
  }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 *  R P C _ C O N D _ W A I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_WAIT(cond,mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("cond: %x, mutex: %x waiting\n", &(cond), &(mutex))); \
            if (! rpc__cond_wait(&(cond), &(mutex), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_WAIT/rpc__cond_wait" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    8, \
                    ("cond: %x, mutex: %x signaled\n", &(cond), &(mutex))); \
            } \
        } else { \
            pthread_cond_wait(&(cond).c, &(mutex).m); \
        } \
    }
#else
#  define RPC_COND_WAIT(cond,mutex) \
    { \
        RPC_LOG_COND_WAIT_NTR; \
        pthread_cond_wait(&(cond).c, &(mutex).m); \
        RPC_LOG_COND_WAIT_XIT; \
    }
#endif


/*
 *  R P C _ C O N D _ T I M E D _ W A I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_TIMED_WAIT(cond,mutex,time)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("cond: %x, mutex: %x waiting %d\n", &(cond), &(mutex), (time))); \
            if (! rpc__cond_timed_wait(&(cond), &(mutex), (time), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_TIMED_WAIT/rpc__cond_timed_wait" )); \
            } \
            else \
            { \
                RPC_DBG_PRINTF ( \
                    rpc_e_dbg_mutex, \
                    8, \
                    ("cond: %x, mutex: %x signaled\n", &(cond), &(mutex))); \
            } \
        } else { \
            pthread_cond_timedwait(&(cond).c, &(mutex).m, (time)); \
        } \
    }
#else
#  define RPC_COND_TIMED_WAIT(cond,mutex,time) \
    { \
        RPC_LOG_COND_TIMED_WAIT_NTR; \
        pthread_cond_timedwait(&(cond).c, &(mutex).m, (time)); \
        RPC_LOG_COND_TIMED_WAIT_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ _ C O N D _ S I G N A L
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_SIGNAL(cond,mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("cond: %x, mutex: %x signaling\n", &(cond), &(mutex))); \
            if (! rpc__cond_signal(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_SIGNAL/rpc__cond_signal" )); \
            } \
        } else { \
            pthread_cond_signal(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_SIGNAL(cond,mutex) \
    { \
        RPC_LOG_COND_SIGNAL_NTR; \
        pthread_cond_signal(&(cond).c); \
        RPC_LOG_COND_SIGNAL_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ _ C O N D _ B R O A D C A S T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_BROADCAST(cond,mutex)  \
    { \
        if (RPC_DBG2(rpc_e_dbg_mutex, 1)) \
        { \
	    RPC_DBG_PRINTF ( \
		rpc_e_dbg_mutex, \
		8, \
		("cond: %x, mutex: %x broadcasting\n", &(cond), &(mutex))); \
            if (! rpc__cond_broadcast(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_BROADCAST/rpc__cond_broadcast" )); \
            } \
        } else { \
            pthread_cond_broadcast(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_BROADCAST(cond,mutex) \
    { \
        RPC_LOG_COND_BROADCAST_NTR; \
        pthread_cond_broadcast(&(cond).c); \
        RPC_LOG_COND_BROADCAST_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/* ===================================================================== */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

/*
 * Prototypes for the support routines.
 */

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif /* _DCE_PROTOTYPE_ */

boolean rpc__mutex_init _DCE_PROTOTYPE_(( rpc_mutex_p_t  /*mp*/ ));

boolean rpc__mutex_delete _DCE_PROTOTYPE_(( rpc_mutex_p_t  /*mp*/ ));

boolean rpc__mutex_lock _DCE_PROTOTYPE_((
        rpc_mutex_p_t  /*mp*/,
        char * /*file*/,
        int  /*line*/
    ));

boolean rpc__mutex_try_lock _DCE_PROTOTYPE_((
        rpc_mutex_p_t  /*mp*/,
        boolean * /*bp*/,
        char * /*file*/,
        int  /*line*/
    ));

boolean rpc__mutex_unlock _DCE_PROTOTYPE_(( rpc_mutex_p_t  /*mp*/ ));

boolean rpc__mutex_lock_assert _DCE_PROTOTYPE_(( rpc_mutex_p_t  /*mp*/));

boolean rpc__mutex_unlock_assert _DCE_PROTOTYPE_(( rpc_mutex_p_t  /*mp*/));


boolean rpc__cond_init _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    ));

boolean rpc__cond_delete _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    ));

boolean rpc__cond_wait _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        volatile rpc_mutex_p_t  /*mp*/,
#ifdef SGIMIPS
        char * /*file*/,
#else
        volatile char * /*file*/,
#endif
        volatile int  /*line*/
    ));

boolean rpc__cond_timed_wait _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        volatile rpc_mutex_p_t  /*mp*/,
        struct timespec * /*wtime*/,
#ifdef SGIMIPS
        char * /*file*/,
#else
        volatile char * /*file*/,
#endif
        volatile int  /*line*/
    ));

boolean rpc__cond_signal _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    ));

boolean rpc__cond_broadcast _DCE_PROTOTYPE_((
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    ));

#else 

void rpc__mutex_none _DCE_PROTOTYPE_((void));
#endif /* defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS) */

/* ===================================================================== */

#endif /* RCPMUTEX_H */
