/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: pthread_sys.c,v 65.9 1998/04/01 14:16:49 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif

/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: pthread_sys.c,v $
 * Revision 65.9  1998/04/01 14:16:49  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added forward prototype declaration for pthread_get_interval().
 *
 * Revision 65.8  1998/03/24  16:21:53  lmc
 * Fixed missing comment characters lost when the ident lines were
 * ifdef'd for the kernel.  Also now calls rpc_icrash_init() for
 * initializing symbols, and the makefile changed to build rpc_icrash.c
 * instead of dfs_icrash.c.
 *
 * Revision 65.7  1998/03/23  17:26:43  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/03/09 19:15:58  lmc
 * Added uthread_t * to rpc__soo_select call.  This passes around a pointer
 * to a dummy uthread structure which is used for the pollrotor lock, etc.
 * Threads are now sthreads, so there is no process name to set.
 *
 * Revision 65.2  1997/11/06  19:56:50  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:34  jdoak
 * Initial revision
 *
 * Revision 64.7  1997/08/26  22:01:04  bdr
 * High load average fix.  Make sure we are setting the PLTWAIT flag when ever
 * we call things like sv_wait and sv_wait_sig to prevent us from getting counted
 * in the nrun load average when we are sleeping.
 *
 * Revision 64.5  1997/05/06  23:11:20  bdr
 * Clear SNWAKE flag in pthread_base.  We do this so that we don't
 * get included in calculating nrun.  This is to fix DFS kernel threads
 * causing the machines load average to be very high.  We still will
 * not get any signals because we have SNOSIG set.
 *
 * Revision 64.4  1997/02/27  18:17:54  lmc
 * MICROTIME changed to use nanotime() as this is documented as the
 * preferable call from the kernel.
 *
 * Revision 64.1  1997/02/14  19:45:32  dce
 * *** empty log message ***
 *
 * Revision 1.5  1996/06/15  03:00:53  brat
 * Changed references to curproc to access curprocp directly instead of via uarea
 * to make it work even for kernel threads which have no u area.
 *
 * Revision 1.4  1996/05/23  18:05:05  vrk
 * Cleanup. Now the parent process's nice value is bumped to enable the
 * child to be scheduled before it. Earlier, it was found that parent
 * was getting scheduled repeatedly - resulting in a temporary hang
 * situation.
 *
 * Revision 1.3  1996/04/25  23:38:48  vrk
 * In pthread_create, the parent process will now call qswtch() to
 * relinquish cpu. PREEMPT() call was not doing the work bcos the same
 * process was getting scheduled continuosly; causing a hang.
 *
 * Revision 1.2  1996/04/15  19:22:15  vrk
 * Old changes and some(lot) of debug statements. The debug printfs will
 * be cleaned later.
 *
 * Revision 1.1  1995/09/28  21:50:13  dcebuild
 * Initial revision
 *
 * Revision 1.1.78.4  1994/06/10  20:54:12  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:56  devsrc]
 *
 * Revision 1.1.78.3  1994/05/19  21:14:10  hopkins
 * 	Serviceability work
 * 	[1994/05/19  02:17:22  hopkins]
 * 
 * 	Serviceability:
 * 	  Change refs to rpc_e_dbg_last_switch to
 * 	  rpc_e_dbg_threads.
 * 	[1994/05/18  21:33:02  hopkins]
 * 
 * Revision 1.1.78.2  1994/02/02  21:48:54  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:00  cbrooks]
 * 
 * Revision 1.1.78.1  1994/01/21  22:31:25  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:33:02  cbrooks]
 * 
 * Revision 1.1.6.1  1993/10/15  21:57:00  kinney
 * 	pthread_create(): inherit command name in new process
 * 	[1993/10/15  21:48:24  kinney]
 * 
 * Revision 1.1.4.2  1993/06/10  19:24:48  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:04:35  kissel]
 * 
 * 	Increase kernel stack size in child in pthread_create()
 * 	[1993/01/15  15:35:47  toml]
 * 
 * 	Change pthread_create for HPUX so that only one process is created.
 * 	[1992/10/27  19:37:01  kissel]
 * 
 * 	06/16/92   tmm  Created from COSL drop.
 * 	[1992/06/18  18:30:48  tmm]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989, 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**  NAME:
**
**      pthread.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  See pthread.h.  Implements pthreads interface for 4.4 bsd kernels.
**
*/

#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <commonp.h>
#include <sys/ksignal.h>
#include <sys/wait.h>
#include <dce/ker/exc_handling.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>       /* need for u.u_procp.c_cptr */
#include <os/proc/pproc_private.h>       
#include <sys/vnode.h>
#include <sys/kthread.h>
#include <sys/par.h>
#include <sys/sat.h>
#include <ksys/sthread.h>
#include <ksys/fdt.h>

extern int	setpgid(pid_t, pid_t);
extern void	kpth_save_stack_space(pthread_startroutine_t, int);

#ifdef	PRIVATE
#undef	PRIVATE
#endif
#define	PRIVATE

/* ================================================================= */

EXCEPTION exc_e_alerted;

/* ================================================================= */

pthread_attr_t          pthread_attr_default;
pthread_mutexattr_t     pthread_mutexattr_default;
pthread_condattr_t      pthread_condattr_default;

/* ================================================================= */

#define PTHREADS_INIT \
    if (pthreads__initd == 0) \
        pthread__init();

INTERNAL short pthreads__initd = 0;

/* ----------------------------------------------------------------- */

/*
 * "once" stuff...
 */

#define PTHREAD_ONCE_INITD          0x01
#define PTHREAD_ONCE_INPROGRESS     0x02
INTERNAL pthread_mutex_t once_m;
INTERNAL pthread_cond_t once_cv;


/* ----------------------------------------------------------------- */

INTERNAL pthread_mutex_t create_m;
INTERNAL pthread_mutex_t spawn_m;
INTERNAL pthread_cond_t spawn_cv;

typedef struct {
    pthread_startroutine_t start_routine;
    pthread_addr_t arg;
    pid_t pid;
    pthread_t new_thread;
    int state;
} pthread_spawn_control_t;

#define PTH_SPAWN_STATE_DONE            0
#define PTH_SPAWN_STATE_INITIATED       1
#define PTH_SPAWN_STATE_ACTIVE          2
#define PTH_SPAWN_STATE_STARTED         3

INTERNAL pthread_spawn_control_t spawn_control = { 0 };

/* ----------------------------------------------------------------- */

/*
 * exception package grunge...
 */

INTERNAL pthread_key_t pthread_exc_ctxt_key;
INTERNAL pthread_mutex_t ctxt_m;

/* ----------------------------------------------------------------- */


/* ----------------------------------------------------------------- */
/*
 * Per-thread context stuff.
 */

/*
 * By default, PTHREAD_DESTRUCTOR_USEFUL is not defined since the
 * destructor is only used internally by pthreads during thread exit
 * processing...  in the kernel, this pthreads isn't wired-in to the
 * thread exit path, so it never gets a chance to run anyway, so why
 * use the storage...
 */

typedef struct pthread_key_ctx_t
{
    pthread_key_t           key;

#ifdef PTHREAD_DESTRUCTOR_USEFUL
    pthread_destructor_t    destructor;
#endif

} pthread_key_ctx_t;

typedef struct pthread_thread_ctx_chain_t
{
    struct pthread_thread_ctx_t *forw;
    struct pthread_thread_ctx_t *back;
} pthread_thread_ctx_chain_t;

typedef struct pthread_thread_ctx_t
{
    pthread_thread_ctx_chain_t  c;
    pthread_t                   thread;
    pthread_key_t               key;
    pthread_addr_t              context_value;
} pthread_thread_ctx_t;

/*
 * A hash table for the thread contexts.
 * Tune the hash list size as necessary.
 *
 * If you're willing to add something to the OS's standard
 * per thread data structures, all of this lookup stuff is
 * unnecessary (as well as the set / get operations becoming
 * pretty cheap).
 */

#define THREAD_CTX_TABLE_SIZE   37
INTERNAL pthread_thread_ctx_chain_t   thread_ctx[THREAD_CTX_TABLE_SIZE];

/*
 * A fixed size list of the different keys that can exist.
 */

#define KEY_CTX_TABLE_SIZE      4
INTERNAL pthread_key_ctx_t      key_ctx[KEY_CTX_TABLE_SIZE];

/*
 * Convert the thread ID into a index into the table.
 */
#define thread_hash(thread) \
    ((unsigned long)(((unsigned long) thread) % THREAD_CTX_TABLE_SIZE))


/* =============================================================== */

INTERNAL void pthread__init _DCE_PROTOTYPE_((void));

INTERNAL void pthread_exc_ctxt_free _DCE_PROTOTYPE_((
        pthread_addr_t  /*context_value*/
    ));

INTERNAL void pthread__delay_wakeup _DCE_PROTOTYPE_(( caddr_t  /*arg*/));


INTERNAL void pthread__ctx_init _DCE_PROTOTYPE_((void));

INTERNAL pthread_thread_ctx_t *pthread__thread_ctx_get _DCE_PROTOTYPE_((
        pthread_key_t                /*key*/
    ));

INTERNAL pthread_thread_ctx_t *pthread__thread_ctx_lookup _DCE_PROTOTYPE_((
        pthread_key_t                /*key*/
    ));

INTERNAL void pthread__thread_ctx_free _DCE_PROTOTYPE_((
        pthread_thread_ctx_t            * /*ctx*/
    ));

INTERNAL void pthread__delay_init _DCE_PROTOTYPE_((void));

#if defined(SGIMIPS) && defined(_KERNEL)
PRIVATE int pthread_get_interval _DCE_PROTOTYPE_((
	struct timespec *abs_ts,
	struct timespec *interval_ts
    ));
#endif


/* =============================================================== */
/*
 * Package initialization
 */
 
INTERNAL void pthread__init(void)
{
    if (pthreads__initd == 1)
        return;

    pthreads__initd = 1;
    pthread__ctx_init();
    /*
     * do this after the __initd = 1 or loop forever :-)
     */

    pthread_mutex_init (&once_m, pthread_mutexattr_default);
    pthread_cond_init (&once_cv, pthread_condattr_default);
    pthread_mutex_init (&create_m, pthread_mutexattr_default);
    pthread_mutex_init (&spawn_m, pthread_mutexattr_default);
    pthread_cond_init (&spawn_cv, pthread_condattr_default);
    pthread_keycreate(&pthread_exc_ctxt_key, pthread_exc_ctxt_free);

    /*
     * Initialize exception package defined exceptions (easier to
     * do as part of pthreads initialization than some exc specific
     * init).
     */
    EXCEPTION_INIT(exc_e_alerted);

    /*
     * Init other goodies...
     */
    pthread__delay_init();
}
   
/* =============================================================== */

/*
 * "once" operations.
 */

PRIVATE int pthread_once
#ifdef _DCE_PROTO_
(
    pthread_once_t              *once_block,
    pthread_initroutine_t       init_routine
)
#else
(once_block, init_routine)
pthread_once_t              *once_block;
pthread_initroutine_t       init_routine;
#endif
{
    int waiting;

    PTHREADS_INIT;

    if (*once_block & PTHREAD_ONCE_INITD)
        return (0);

    /*
     * This test should really be:
     *      if one init is inprogress
     *          wait until the inprogress one completes
     *          return
     * Since this condition is very unlikely (at least initially)
     * this hack will at least protect us.
     */
    pthread_mutex_lock (&once_m);
    waiting = 0;
    while (*once_block & PTHREAD_ONCE_INPROGRESS) {
	waiting = 1;
	pthread_cond_wait (&once_cv, &once_m);
    }

    if (waiting) {
	pthread_mutex_unlock (&once_m);
	return(0);
    }

    *once_block |= PTHREAD_ONCE_INPROGRESS;
    pthread_mutex_unlock (&once_m);

    (*init_routine) ();

    pthread_mutex_lock (&once_m);
    *once_block &= ~PTHREAD_ONCE_INPROGRESS;
    *once_block |= PTHREAD_ONCE_INITD;
    pthread_cond_broadcast (&once_cv);
    pthread_mutex_unlock (&once_m);

    return (0);
}

/* =============================================================== */

/*
 * Generic Thread operations.
 */

PRIVATE void pthread_base()
{
    pthread_startroutine_t start_routine;
    pthread_addr_t arg;
    struct proc *curp = curprocp;
    register int s;
    uthread_t       *ut = curuthread;


    /*
     * Get the start function and arguments
     */
    pthread_mutex_lock(&spawn_m);
    start_routine = spawn_control.start_routine;
    arg = spawn_control.arg;

    spawn_control.new_thread = pthread_self();
    spawn_control.state = PTH_SPAWN_STATE_STARTED;
    
    pthread_cond_broadcast(&spawn_cv); 
    pthread_mutex_unlock(&spawn_m);

    /*
     * Close parent's files.
     */
    fdt_closeall();

    timerclear(&curp->p_cru.ru_utime);
    timerclear(&curp->p_cru.ru_stime);

    /*
     * Set the ps-visible arg list to something
     * sensible.
     */
    bcopy("DCEpool", curp->p_psargs, 8);
    bcopy("DCEpool", curp->p_comm, 8);

    /* Discard parents signal handlers */
#if 0
    s = p_siglock(curp);
    /* SNOSIG no longer exists in 6.5.  There isn't a one for one
	replacement.  Deleting it for now.  Maybe we need to do the
	sigemptyset now instead. 
     */
    curp->p_flag |= SNOSIG;            /* Don't receive any new signals. */
#endif
#ifdef NOTDEF
    sigemptyset(&curp->p_sig);	       /* And clear all pending signals. */
#endif

    curp->p_flag &= ~(STRC | SPROF);
    ut_flagclr(ut, UT_OWEUPC);
    UT_TO_KT(ut)->k_flags &= ~KT_NWAKE;
    ut->ut_polldat = 0;

#if 0
    p_sigunlock(curp, s);
#endif

    sigqfree(&ut->ut_sigpend.s_sigqueue);

    setpgid(0, 0);


    /*
     * The TRY / CATCH is setup to make it so threads created internally
     * (don't have to keep freeing their per thread exception context
     * when we know that they won't exit out from underneath us...).
     * This just lessens the cost of of TRY / CATCH for call executor
     * threads (at the cost of some stack space).
     */
    TRY {
        (*start_routine)(arg);
    } CATCH_ALL {
        RPC_DBG_GPRINTF(("(pthread_create) thread unwound...\n"));
    } ENDTRY

    exit(CLD_EXITED, 0);
}


PRIVATE int pthread_create
#ifdef _DCE_PROTO_
(
    pthread_t                   *new_thread,
    pthread_attr_t              attr,
    pthread_startroutine_t      start_routine,
    pthread_addr_t              arg
)
#else
(new_thread, attr, start_routine, arg)
pthread_t                   *new_thread;
pthread_attr_t              attr;
pthread_startroutine_t      start_routine;
pthread_addr_t              arg;
#endif
{
    register int    s;
    pid_t pid;
    pid_t child_pid;
    PTHREADS_INIT
    
    if (s = sthread_create("DCEpool", 0, 2*KTHREAD_DEF_STACKSZ,
		0, 174, KT_PS, (st_func_t *)start_routine, 
		arg, 0, 0, 0)) {
            RPC_DBG_GPRINTF(("pthread_create: sthread_create() failed with %d\n",
		s));
    }
	
    return (0);
}

PRIVATE int pthread_detach
#ifdef _DCE_PROTO_
(
    pthread_t                   *thread
)
#else
(thread)
pthread_t                   *thread;
#endif
{
    PTHREADS_INIT

    return (0);
    /* NO OP */
}


PRIVATE pthread_t pthread_self(void)
{
    PTHREADS_INIT

    return((pthread_t) kthread_getid());
}

PRIVATE void pthread_yield(void)
{
    PTHREADS_INIT

}

PRIVATE int pthread_attr_create
#ifdef _DCE_PROTO_
(
    pthread_attr_t *attr
)
#else
(attr)
pthread_attr_t *attr;
#endif
{
    PTHREADS_INIT

    *attr = pthread_attr_default;
    return (0);
}

PRIVATE int pthread_attr_destroy
#ifdef _DCE_PROTO_
(
    pthread_attr_t *attr
)
#else
(attr)
pthread_attr_t *attr;
#endif
{
    PTHREADS_INIT
    return (0);

    /* NO-OP */
}

/* =============================================================== */
/*
 * Per-thread context operations.
 *
 * This implementation is counting on the fact that there will be only
 * a small number of keys (e.g. one for stubs, one for exceptions).
 * Unfortunately, there may be numerous threads with active contexts
 * for these keys.
 *
 * There is a simply indexed small "Key Context" table that contains
 * the global per key information (i.e. the destructor).  Essentially,
 * this table is only used when a key is created (to assign a key value) and
 * when key's destructor function needs to be invoked (at thread exit).
 *
 * All the real work involves a thread_id hashed doubly linked "Thread
 * Context" table.  There is one Thread Context table entry for each
 * "active" key.
 * 
 * A context value of NULL has special meaning indicating that there
 * is no context value for the the key (see below)...
 */

INTERNAL void pthread__ctx_init(void)
{
    int     i;

    for (i = 0; i < KEY_CTX_TABLE_SIZE; i++)
        key_ctx[i].key = 0;

    for (i = 0; i < THREAD_CTX_TABLE_SIZE; i++)
        thread_ctx[i].forw = 
            thread_ctx[i].back = (pthread_thread_ctx_t *) &thread_ctx[i];
    pthread_mutex_init(&ctxt_m, pthread_mutexattr_default);
}

/*
 * Lookup (create if necessary) the thread context for this key.
 */

INTERNAL pthread_thread_ctx_t *pthread__thread_ctx_get
(pthread_key_t   key)
{
    pthread_thread_ctx_t    *tctx;
    pthread_t               thread;
    unsigned long	    probe;
    pthread_thread_ctx_t    *tctx_head;

    ASSERT((key >= 1 && key <= KEY_CTX_TABLE_SIZE));

    pthread_mutex_lock(&ctxt_m);
    thread = pthread_self();
    probe = thread_hash(thread);
    tctx_head = (pthread_thread_ctx_t *) &thread_ctx[probe];

    /*
     * Search for the thread specific key context using the
     * thread to compute a hash.
     */

    for (tctx = tctx_head->c.forw; tctx != tctx_head; tctx = tctx->c.forw)
    {
        if (pthread_equal(tctx->thread, thread) && tctx->key == key)
        {
	    pthread_mutex_unlock(&ctxt_m);
            return (tctx);
        }
    }

    /*
     * Not found; Create and initialize a thread context descriptor.
     * Insert at the head of the doubly linked list.
     */

    RPC_MEM_ALLOC(tctx, pthread_thread_ctx_t *, sizeof *tctx, RPC_C_MEM_PTHREAD_CTX, 
        RPC_C_MEM_WAITOK);
    if (tctx == NULL)
        DIE("(pthread__thread_ctx_get)");

    tctx->thread    = thread;
    tctx->key       = key;
    tctx->context_value = NULL;

    tctx->c.forw    = tctx_head->c.forw;
    tctx->c.back    = tctx_head;
    tctx_head->c.forw->c.back   = tctx;
    tctx_head->c.forw           = tctx;

    pthread_mutex_unlock(&ctxt_m);

    return (tctx);
}


/*
 * Lookup the thread context for this key.
 */

INTERNAL pthread_thread_ctx_t *pthread__thread_ctx_lookup
#ifdef _DCE_PROTO_
(
    pthread_key_t               key
)
#else
(key)
pthread_key_t               key;
#endif
{
    pthread_thread_ctx_t    *tctx;
    pthread_t               thread;
    unsigned long           probe;
    pthread_thread_ctx_t    *tctx_head;

    ASSERT((key >= 1 && key <= KEY_CTX_TABLE_SIZE));

    thread = pthread_self();
    probe = thread_hash(thread);
    tctx_head = (pthread_thread_ctx_t *) &thread_ctx[probe];

    /*
     * Search for the thread specific key context using the
     * thread to compute a hash.
     */

    pthread_mutex_lock(&ctxt_m);
    for (tctx = tctx_head->c.forw; tctx != tctx_head; tctx = tctx->c.forw)
    {
        if (pthread_equal(tctx->thread, thread) && tctx->key == key)
        {
	    pthread_mutex_unlock(&ctxt_m);
            return (tctx);
        }
    }
    pthread_mutex_unlock(&ctxt_m);

    return (NULL);
}


/*
 * Free the thread context for this key.
 */

INTERNAL void pthread__thread_ctx_free
#ifdef _DCE_PROTO_
(
    pthread_thread_ctx_t            *tctx
)
#else
(tctx)
pthread_thread_ctx_t            *tctx;
#endif
{
    pthread_mutex_lock(&ctxt_m);
    tctx->c.back->c.forw = tctx->c.forw;
    tctx->c.forw->c.back = tctx->c.back;
    pthread_mutex_unlock(&ctxt_m);

    RPC_MEM_FREE(tctx, RPC_C_MEM_PTHREAD_CTX);
}

/* ------------------------------------------------------------------ */

#ifdef SGIMIPS
PRIVATE int pthread_keycreate(
pthread_key_t               *key,
pthread_destructor_t        destructor)
#else
PRIVATE int pthread_keycreate(key, destructor)
pthread_key_t               *key;
pthread_destructor_t        destructor;
#endif
{
    pthread_key_ctx_t   *kctx;
    pthread_key_t   k;

    PTHREADS_INIT

    /*
     * Create and initialize a key context descriptor.
     * key '0' is a special "unused" key.
     * returned keys range from 1..KEY_CTX_TABLE_SIZE
     */

    pthread_mutex_lock(&ctxt_m);
    for (k = 0, kctx = &key_ctx[k]; k < KEY_CTX_TABLE_SIZE; k++, kctx++)
        if (kctx->key == 0)
        {
            kctx->key = *key = k + 1;
#ifdef PTHREAD_DESTRUCTOR_USEFUL
            kctx->destructor = destructor;
#endif
	    pthread_mutex_unlock(&ctxt_m);
            return (0);
        }
    pthread_mutex_unlock(&ctxt_m);

    DIE("pthread_key_create");
    return (0);
}


#ifdef SGIMIPS
PRIVATE int pthread_setspecific(
pthread_key_t               key,
pthread_addr_t              context_value)
#else
PRIVATE int pthread_setspecific(key, context_value)
pthread_key_t               key;
pthread_addr_t              context_value;
#endif
{
    pthread_thread_ctx_t   *tctx;

    /*
     * Lookup (create if necessary) the thread's ctx block for this key.
     */

    tctx = pthread__thread_ctx_get(key);

    /*
     * If a new context is NULL, we can free the internal per-thread 
     * context storage.
     *
     * Note: this is NOT a property of the pthreads spec!  However,
     * we need some way to determine that the per-thread context
     * block can be freed since we aren't wired into thread exit processing.
     * There are places in the runtime and libnidl that set the value to
     * NULL so this will happen.
     */

    if (context_value == NULL)
    {
        pthread__thread_ctx_free(tctx);
        return (0);
    }

    /*
     * Save the new context value.
     */

    tctx->context_value = context_value;

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_getspecific(
pthread_key_t               key,
pthread_addr_t              *context_value)
#else
PRIVATE int pthread_getspecific(key, context_value)
pthread_key_t               key;
pthread_addr_t              *context_value;
#endif
{
    pthread_thread_ctx_t   *tctx;

    /*
     * ensure that a getspecific does NOT create a internal
     * per-thread context block.
     */
    tctx = pthread__thread_ctx_lookup(key);

    if (tctx == NULL)
        *context_value = NULL;
    else
        *context_value = tctx->context_value;

    return (0);
}

/* =============================================================== */
/*
 * Generic Mutex operations.
 */

#ifdef SGIMIPS
PRIVATE int pthread_mutex_init(
pthread_mutex_t             *new_mutex,
pthread_mutexattr_t         attr)
#else
PRIVATE int pthread_mutex_init(new_mutex, attr)
pthread_mutex_t             *new_mutex;
pthread_mutexattr_t         attr;
#endif
{
    PTHREADS_INIT

    mutex_init(new_mutex, MUTEX_DEFAULT, NULL);

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_mutex_destroy(
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_mutex_destroy(mutex)
pthread_mutex_t             *mutex;
#endif
{
    mutex_destroy(mutex);

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_mutex_lock(
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_mutex_lock(mutex)
pthread_mutex_t             *mutex;
#endif
{
    mutex_lock(mutex, (PZERO | PLTWAIT));
    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_mutex_trylock(
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_mutex_trylock(mutex)
pthread_mutex_t             *mutex;
#endif
{
    return (mutex_trylock(mutex));
}

#ifdef SGIMIPS
PRIVATE int pthread_mutex_unlock(
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_mutex_unlock(mutex)
pthread_mutex_t             *mutex;
#endif
{
    mutex_unlock(mutex);

    return (0);
}

/* =============================================================== */

/*
 * Generic Condition Variable operations.
 */

#ifdef SGIMIPS
PRIVATE int pthread_cond_init(
pthread_cond_t              *new_condition,
pthread_condattr_t          attr)
#else
PRIVATE int pthread_cond_init(new_condition, attr)
pthread_cond_t              *new_condition;
pthread_condattr_t          attr;
#endif
{
    PTHREADS_INIT

    sv_init(new_condition, SV_DEFAULT, NULL);

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_cond_destroy(
pthread_cond_t              *condition)
#else
PRIVATE int pthread_cond_destroy(condition)
pthread_cond_t              *condition;
#endif
{
    
    sv_destroy(condition);

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_cond_signal(
pthread_cond_t              *condition)
#else
PRIVATE int pthread_cond_signal(condition)
pthread_cond_t              *condition;
#endif
{

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_signal(0x%8x) called", (unsigned) condition);
#endif
    sv_signal(condition);

    return (0);
}

#ifdef SGIMIPS
PRIVATE int pthread_cond_broadcast(
pthread_cond_t              *condition)
#else
PRIVATE int pthread_cond_broadcast(condition)
pthread_cond_t              *condition;
#endif
{

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_broadcast(0x%8x) called", (unsigned) condition);
#endif
    sv_broadcast(condition);

    return (0);
}

/*
 * This routine gets a little tricky if cancel are supported since it
 * would have to tie in with cancels, signals and exception handlers.
 * Fortunately, this is not the case... In the kernel, we don't support
 * cancels the cancel mechanism so a cond wait blocks in a non-signallable.
 */

#define PTHREAD_COND_SLEEP_PRI              ((PZERO))

#ifdef SGIMIPS
PRIVATE int pthread_cond_wait(
pthread_cond_t              *condition,
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_cond_wait(condition, mutex)
pthread_cond_t              *condition;
pthread_mutex_t             *mutex;
#endif
{
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_wait(0x%8x) called", (unsigned) condition);
#endif
    sv_wait(condition, (PTHREAD_COND_SLEEP_PRI | PLTWAIT), mutex, 0);
    mutex_lock( mutex, (PTHREAD_COND_SLEEP_PRI | PLTWAIT));
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_wait(0x%8x) returning", (unsigned) condition);
#endif
    return (0);
}

/*
 * This routine is called when you need to make an interruptible sleep.
 */
#ifdef SGIMIPS
PRIVATE int pthread_cond_wait_intr_np(
pthread_cond_t              *condition,
pthread_mutex_t             *mutex)
#else
PRIVATE int pthread_cond_wait_intr_np(condition, mutex)
pthread_cond_t              *condition;
pthread_mutex_t             *mutex;
#endif
{
int rval;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_wait_intr_np(0x%8x) called", (unsigned) condition);
#endif
    rval = sv_wait_sig(condition, (PTHREAD_COND_SLEEP_PRI | PLTWAIT), mutex, 0);
    mutex_lock(mutex, (PTHREAD_COND_SLEEP_PRI | PLTWAIT));
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_wait_intr_np(0x%8x) returning", (unsigned) condition);
#endif
    if(rval == 0)
    	return (0);
    else
	return (-1);
}


PRIVATE int pthread_cond_timedwait
#ifdef _DCE_PROTO_
(
    pthread_cond_t              *condition,
    pthread_mutex_t             *mutex,
    struct timespec             *abs_time
)
#else
(condition, mutex, abs_time)
pthread_cond_t              *condition;
pthread_mutex_t             *mutex;
struct timespec             *abs_time;
#endif
{
    timespec_t		atv;		/* time to wait  */
    timespec_t		remaining;	/*remaining time if woke early */
    int			left;
    extern time_t	lbolt;
    time_t 		diff = 0;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_timedwait(0x%8x) called", (unsigned) condition);
#endif
#if defined(SGIMIPS) && defined(_KERNEL)
    /* sv_timedwait takes an interval instead of an absolute time. */
    pthread_get_interval(abs_time, &atv);
#else
    atv.tv_sec = abs_time->tv_sec;
    atv.tv_nsec = abs_time->tv_nsec;
#endif

    sv_timedwait(condition, PLTWAIT, mutex, 0, 0, &atv, &remaining);

    mutex_lock(mutex, (PTHREAD_COND_SLEEP_PRI | PLTWAIT));
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "pthread_cond_timedwait(0x%8x) returning", (unsigned) condition);
#endif
    left = remaining.tv_sec || remaining.tv_nsec;
    if(left > 0)
	return 0;
    else
	return -1;
}


/* =============================================================== */
/*
 * pthread cancel stuff...
 */

PRIVATE int pthread_cancel
#ifdef _DCE_PROTO_
(
    pthread_t                   thread
)
#else
(thread)
pthread_t                   thread;
#endif
{
    PTHREADS_INIT

    return (0);
    /* NO OP; no cancel behavour support in the kernel */
}

PRIVATE int pthread_setasynccancel
#ifdef _DCE_PROTO_
(
    int mode
)
#else
(mode)
int mode;
#endif
{
    PTHREADS_INIT
    return (0);

    /* NO OP; no cancel behavour support in the kernel */
}

PRIVATE int pthread_setcancel
#ifdef _DCE_PROTO_
(
    int mode
)
#else
(mode)
int mode;
#endif
{
    PTHREADS_INIT
 
    return (0);

    /* NO OP; no cancel behavour support in the kernel */
}


PRIVATE void pthread_testcancel(void)
{
    /*
     * Any code that depended on this working has to be re-coded.
     */
    DIE("(pthread_testcancel)");
}

/* =============================================================== */
/*
 * Support for the exception package (see exc.h for more info).
 */

/*
 * Raise the indicated exception (unwind to the innermost handler).
 */
PRIVATE void pthread_exc_raise
#ifdef _DCE_PROTO_
(
    volatile EXCEPTION *e
)
#else
(e)
volatile EXCEPTION *e;
#endif
{
    pthread_exc_ctxt_t **_exc_ctxt_head = NULL;

    pthread_getspecific(pthread_exc_ctxt_key, (pthread_addr_t *)(&_exc_ctxt_head));

    /*
     * Ensure that attempts to unwind past the outermost handler
     * (a programming error) are detected in an obvous fashion.
     */
    if (_exc_ctxt_head == NULL || *_exc_ctxt_head == NULL)
        DIE("attempt to unwind above top cleanup handler; fix your application");

    /*
     * Can't raise 0 (typically indicates unitialized exception).
     */
    if (e->kind != _exc_kind_status || e->match.value == 0)
        DIE("unitialized exception");

    /*
     * stash the exception in the exc_ctxt that we're unwinding to
     * so they know what caused the unwind.
     */
    (*_exc_ctxt_head)->exc = *e;

    /*
     * Unwind to the thread's innermost exception context.
     *
     * Note kernel longjmp doesn't seem to take a "value" arg
     * (they always seem to induce a setjmp return value of '1').
     * We specify one anyway, just to be clear about this.
     */
    dce_exc_longjmp((k_machreg_t *)&(*_exc_ctxt_head)->jmpbuf, 1);
}

/*
 * Get a pointer to the per-thread exc context storage
 * and initialize the prev pointer to its current value.
 * If the per-thread exc context hasn't yet been allocated,
 * now's the time to do it.
 */

PRIVATE void pthread_exc_setup
#ifdef _DCE_PROTO_
(
    volatile pthread_exc_ctxt_t ***_exc_ctxt_head,
    pthread_exc_ctxt_t **prev_exc_ctxt
)
#else
(_exc_ctxt_head, prev_exc_ctxt)
volatile pthread_exc_ctxt_t ***_exc_ctxt_head;
pthread_exc_ctxt_t **prev_exc_ctxt;
#endif
{
    pthread_getspecific(pthread_exc_ctxt_key, (pthread_addr_t *)(_exc_ctxt_head));
    if (*_exc_ctxt_head == NULL) {
        RPC_MEM_ALLOC(*_exc_ctxt_head, volatile pthread_exc_ctxt_t **, sizeof *_exc_ctxt_head, 
            RPC_C_MEM_JMPBUF_HEAD, RPC_C_MEM_WAITOK);
        if (*_exc_ctxt_head == NULL)
            DIE("(pthread_exc_setup)");
        **_exc_ctxt_head = NULL;
        pthread_setspecific(pthread_exc_ctxt_key, *_exc_ctxt_head);
    }

    *prev_exc_ctxt = (pthread_exc_ctxt_t *)**_exc_ctxt_head;
}

/*
 * Destroy the per-thread exception state context; i.e.
 * free the per-thread storage and cause the per-thread
 * key context storage to be freed.
 */

PRIVATE void pthread_exc_destroy
#ifdef _DCE_PROTO_
(
    volatile pthread_exc_ctxt_t **_exc_ctxt_head
)
#else
(_exc_ctxt_head)
volatile pthread_exc_ctxt_t **_exc_ctxt_head;
#endif
{
    RPC_MEM_FREE(_exc_ctxt_head, RPC_C_MEM_JMPBUF_HEAD);
    pthread_setspecific(pthread_exc_ctxt_key, NULL);
}

INTERNAL void pthread_exc_ctxt_free
#ifdef _DCE_PROTO_
(
    pthread_addr_t context_value
)
#else
(context_value)
pthread_addr_t context_value;
#endif
{
    RPC_MEM_FREE(context_value, RPC_C_MEM_JMPBUF_HEAD);
}

/* =============================================================== */
/*
 * pthread_*_np() stuff...
 */

#if defined(SGIMIPS) && defined(_KERNEL)
PRIVATE int pthread_get_interval
#ifdef _DCE_PROTO_
(
    struct timespec *abs_ts,
    struct timespec *interval_ts
)
#else
(delta_ts, abs_ts)
struct timespec *abs_ts;
struct timespec *interval_ts;
#endif
{
    int s;
    struct timespec abs_timestruc;

    PTHREADS_INIT
    /*
     * Compute an interval timespec from an absolute 
     */

    nanotime(&abs_timestruc);
    if (abs_timestruc.tv_sec > abs_ts->tv_sec) {
	interval_ts->tv_sec=0;
    } else {
        interval_ts->tv_sec = abs_ts->tv_sec - abs_timestruc.tv_sec;
    }

    interval_ts->tv_nsec = abs_ts->tv_nsec - abs_timestruc.tv_nsec;
    if (interval_ts->tv_nsec <= 0)
    {
        interval_ts->tv_nsec += 1000000000;
        interval_ts->tv_sec--;
    }

    return (0);
}
#endif

PRIVATE int pthread_get_expiration_np
#ifdef _DCE_PROTO_
(
    struct timespec *delta_ts,
    struct timespec *abs_ts
)
#else
(delta_ts, abs_ts)
struct timespec *delta_ts;
struct timespec *abs_ts;
#endif
{
    int s;
    struct timespec abs_timestruc;

    PTHREADS_INIT

    /*
     * Compute an absolute timespec
     */
    nanotime(abs_ts);

    abs_ts->tv_nsec += delta_ts->tv_nsec;
    if (abs_ts->tv_nsec >= 1000000000)
    {
        abs_ts->tv_nsec -= 1000000000;
        abs_ts->tv_sec++;
    }
    abs_ts->tv_sec += delta_ts->tv_sec;

    return (0);
}


/*
 * This implementation of pthread_delay_np() is skewed towards
 * a small number of threads concurrently delaying.  Threads
 * block on a per-thread cv (so that cuncurrent delayers aren't
 * prematurely awakened by other waiter's time expirations).
 * Since real per-thread storage is rather inefficient with
 * this implementation, we just use a small lightweight
 * table to achieve the same effect (a.k.a. hack).
 */

typedef struct {
    pthread_t   thread;
    pthread_cond_t cv;          /* cv to delay on */
} pthread__delay_t;

#ifndef PTHREAD_DELAY_TABLE_SIZE
#  define PTHREAD_DELAY_TABLE_SIZE    5
#endif

INTERNAL pthread__delay_t delay_tbl[PTHREAD_DELAY_TABLE_SIZE];
INTERNAL pthread_mutex_t delay_m;

INTERNAL void pthread__delay_init(void)
{
    pthread__delay_t        *dtbl;

    pthread_mutex_init(&delay_m, pthread_mutexattr_default);

    for (dtbl = delay_tbl; 
        dtbl < &delay_tbl[PTHREAD_DELAY_TABLE_SIZE]; dtbl++)
    {
        dtbl->thread = NULL;
        pthread_cond_init(&dtbl->cv, pthread_condattr_default);
    }
}

PRIVATE int pthread_delay_np
#ifdef _DCE_PROTO_
(
    struct timespec *interval
)
#else
(interval)
struct timespec *interval;
#endif
{
    pthread__delay_t        *dtbl;
    struct timespec         abs_ts;     /* absolute timespec */

    PTHREADS_INIT

    pthread_mutex_lock(&delay_m);

    /*
     * Locate a delay table entry to use.
     */
    for (dtbl = delay_tbl;
        dtbl < &delay_tbl[PTHREAD_DELAY_TABLE_SIZE]; dtbl++)
    {
        if (dtbl->thread == NULL)
        {
            dtbl->thread = pthread_self();
            break;
        }
    }
    if (dtbl >= &delay_tbl[PTHREAD_DELAY_TABLE_SIZE])
    {
        DIE("delay table exhausted");
        /* 
         * make the table bigger, have "cheap" per-thread delay
         * storage, ...  vendors choice...
         */
    }

    /*
     * Convert to a useable absolute time.
     */
    pthread_get_expiration_np(interval, &abs_ts);

    /*
     * Block until the absolute time is reached...
     */
    while (pthread_cond_timedwait(&dtbl->cv, &delay_m, &abs_ts) == 0)
        ;

    /*
     * Free the table entry.
     */
    dtbl->thread = NULL;

    pthread_mutex_unlock(&delay_m);
    return (0);
}
