/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: rpctimer.c,v 65.8 1998/03/23 17:29:46 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpctimer.c,v $
 * Revision 65.8  1998/03/23 17:29:46  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.7  1998/03/09 19:31:49  lmc
 * Remove setting of process name because kernel threads are now sthreads
 * instead of uthreads and thus don't have a process name - only a kthread
 * name.
 *
 * Revision 65.5  1998/01/07  17:21:28  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.4  1997/12/16  17:23:26  lmc
 * Removed prototypes that were no longer correct and/or existed in a .h file now.
 *
 * Revision 65.3  1997/11/06  19:57:31  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/27  19:20:53  jdoak
 * 6.4 + 1.2.2 code merge
 *
 * Revision 65.1  1997/10/20  19:17:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.815.1  1996/05/10  13:12:42  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/4  1996/04/24  14:14 UTC  bissen
 * 	Add return to timer_init()
 *
 * 	HP revision /main/DCE_1.2/3  1996/04/23  19:43 UTC  bissen
 * 	Add some __hpux defines
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:14 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:43 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/6]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:04 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:35 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/6]
 *
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 *
 * 	Added the TCB mutex check, but turned it off.
 * 	[1995/03/29  13:59 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/3]
 *
 * 	Enabled itimer as default.
 * 	[1995/02/17  17:45 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/2]
 *
 * 	Registered rpc__timer_itimer_stop() with _cma_atexec_hp().
 * 	[1995/02/16  19:51 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:42 UTC  lmm  /main/HPDCE02/5]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:02 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:16 UTC  lmm  /main/HPDCE02/4]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:20 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted CHFtsthe fix for CHFts14267 and misc. itimer fixes.
 * 	[1995/02/07  20:56 UTC  tatsu_s  /main/HPDCE02/3]
 *
 * 	Turn itimer off if become multi-threaded.
 * 	[1995/02/03  19:15 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b1/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:17 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Fixed KRPC.
 * 	[1995/01/31  14:49 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/5]
 *
 * 	Fixed disable_itimer logic.
 * 	[1995/01/28  02:37 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/4]
 *
 * 	Changed the signature of rpc__timer_itimer_enable().
 * 	Cleaned up the signal handler installation.
 * 	[1995/01/27  22:22 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/3]
 *
 * 	Cleaned itimer up.
 * 	[1995/01/25  20:38 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/2]
 *
 * 	Added VTALRM timer.
 * 	[1995/01/19  19:15 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:19 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:25 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 *
 * Revision 1.1.811.2  1994/05/19  21:15:07  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:44:49  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:28:10  hopkins]
 * 
 * Revision 1.1.811.1  1994/01/21  22:39:18  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  22:00:03  cbrooks]
 * 
 * Revision 1.1.80.1  1993/09/15  15:40:00  damon
 * 	Synched with ODE 2.1 based build
 * 	[1993/09/15  15:32:11  damon]
 * 
 * Revision 1.1.8.2  1993/09/14  16:51:44  tatsu_s
 * 	Bug 8103 - Added the call to pthread_detach() after
 * 	pthread_join() to reclaime the storage.
 * 	Added the variables, timer_task_running & timer_task_was_running,
 * 	to keep track of the timer thread's state aross the fork.
 * 	[1993/09/13  17:10:37  tatsu_s]
 * 
 * Revision 1.1.6.3  1993/08/20  18:38:22  ganni
 * 	Bug 8463 - change prev== NULL to prev = NULL.
 * 	[1993/08/20  18:37:56  ganni]
 * 
 * Revision 1.1.6.2  1993/08/03  16:25:20  tom
 * 	Bug 7654 - Set the prev pointer in rpc__timer_clear.
 * 	[1993/08/03  16:24:57  tom]
 * 
 * Revision 1.1.4.4  1993/01/03  23:55:07  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:12:41  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  21:16:35  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:44:22  zeliff]
 * 
 * Revision 1.1.4.2  1992/12/22  22:25:39  sommerfeld
 * 	Add fix to previous "gotcha" (from sandhya at IBM)
 * 	[1992/12/15  20:44:25  sommerfeld]
 * 
 * 	Add warning about potential "gotcha" if rpc__clock_update isn't
 * 	called frequently.
 * 	[1992/09/02  15:24:59  sommerfeld]
 * 
 * 	Timer reorganization: don't wake up unless we have work to do.
 * 	[1992/09/01  22:57:05  sommerfeld]
 * 
 * Revision 1.1  1992/01/19  03:08:49  devrcs
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
**  NAME
**
**      rpctimer.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of types/constants internal to RPC facility and common
**  to all RPC components.
**
**
*/

/* !!! CMA Hack !!! */
#include <signal.h>
typedef	struct sigaction itimer_sigaction_t;
#include <commonp.h>
#include <dce/assert.h>
#include <com.h>


/* ========================================================================= */

EXTERNAL rpc_clock_t rpc_g_clock_curr;
EXTERNAL boolean rpc_g_is_single_threaded;
GLOBAL boolean32 rpc_g_long_sleep;

INTERNAL rpc_timer_p_t running_list;
INTERNAL rpc_mutex_t   rpc_g_timer_mutex;
INTERNAL rpc_cond_t    rpc_g_timer_cond;
INTERNAL rpc_clock_t   rpc_timer_high_trigger;
INTERNAL rpc_clock_t   rpc_timer_cur_trigger;
INTERNAL rpc_timer_p_t rpc_timer_head, rpc_timer_tail;
INTERNAL unsigned32    stop_timer;
INTERNAL pthread_t timer_task;
INTERNAL boolean timer_task_running = false;
INTERNAL boolean timer_task_was_running = false;
/*
 * Disable itimer as default. (no official support)
 */
#ifndef _KERNEL
#define	INITIAL_DISABLE_ITIMER_STATE	true
#else
#define	INITIAL_DISABLE_ITIMER_STATE	true
#endif  /* _KERNEL */
INTERNAL int rpc_itimer_signal = SIGVTALRM;
INTERNAL int rpc_itimer_type = ITIMER_VIRTUAL;
INTERNAL volatile boolean is_itimer_running = false;
INTERNAL volatile boolean is_itimer_enabled = false;
INTERNAL volatile boolean disable_itimer = INITIAL_DISABLE_ITIMER_STATE;
#ifndef _KERNEL
INTERNAL struct itimerval last_itimer;
INTERNAL itimer_sigaction_t itimer_act;
#endif  /* _KERNEL */

#if defined(CMA_INCLUDE) && defined(__hpux)
extern int cma__hp_self_tcb_locked(void);
extern int cma__hp_kernel_locked(void);
#endif  /* defined(CMA_INCLUDE) */

INTERNAL void timer_loop _DCE_PROTOTYPE_((void));


INTERNAL void rpc__timer_set_int _DCE_PROTOTYPE_ ((
        rpc_timer_p_t            /*t*/,
        rpc_timer_proc_p_t       /*proc*/,
        pointer_t                /*parg*/,
        rpc_clock_t              /*freq*/
    ));

INTERNAL rpc_clock_t rpc__timer_callout_int _DCE_PROTOTYPE_((void));
#ifndef _KERNEL
INTERNAL boolean32 rpc__timer_itimer_install _DCE_PROTOTYPE_((void));

INTERNAL void rpc__timer_itimer_handler _DCE_PROTOTYPE_ ((
	int			/*sig*/,
	int			/*code*/,
	struct sigcontext	* /*scp*/
    ));
#endif  /* _KERNEL */

/*
 * Mutex lock macros 
 */

#define RPC_TIMER_LOCK_INIT(junk)   RPC_MUTEX_INIT (rpc_g_timer_mutex)
#define RPC_TIMER_LOCK(junk)        RPC_MUTEX_LOCK (rpc_g_timer_mutex)
#define RPC_TIMER_TRY_LOCK(bp)      RPC_MUTEX_TRY_LOCK(rpc_g_timer_mutex,(bp))
#define RPC_TIMER_UNLOCK(junk)      RPC_MUTEX_UNLOCK (rpc_g_timer_mutex)
#define RPC_TIMER_LOCK_ASSERT(junk) RPC_MUTEX_LOCK_ASSERT(rpc_g_timer_mutex)
#define RPC_TIMER_COND_INIT(junk)   RPC_COND_INIT (rpc_g_timer_cond, rpc_g_timer_mutex)

/* ========================================================================= */


/*
 * T I M E R _ L O O P
 *
 * Periodically get the new clock time and process any ready timer events.
 */

#ifndef NO_RPC_TIMER_THREAD

INTERNAL void timer_loop(void)
{
    RPC_TIMER_LOCK(0);

    while (!stop_timer)
    {
        rpc_clock_t next;
        struct timespec next_ts;
        rpc_clock_t max_step;
        
	/*
	 * It would be real nice if we could figure out a way to get
	 * the system time global variable mapped read-only into our
	 * address space to avoid the !@#$% gettimeofday syscall overhead.
	 */
        rpc__clock_update();
        next = rpc__timer_callout_int();

        /*
         * wake up at least once every 50 seconds, so we don't confuse
         * the underlying rpc__clock_update() code.
         */
        max_step = RPC_CLOCK_SEC(50);   
        if (next == 0 || next > max_step)
            next = max_step;
	rpc_g_long_sleep = ( next > RPC_CLOCK_SEC(1) );

        if (next > 10)
        {
            RPC_DBG_PRINTF(rpc_e_dbg_timer, 5,
                ("(timer_loop) next event in %d seconds\n", next/RPC_C_CLOCK_HZ));
        }
        rpc_timer_cur_trigger = rpc_g_clock_curr + next;
        rpc__clock_timespec (rpc_timer_cur_trigger, &next_ts);
        RPC_COND_TIMED_WAIT(rpc_g_timer_cond, rpc_g_timer_mutex, &next_ts);
    }

    RPC_TIMER_UNLOCK(0);
}

#endif /* NO_RPC_TIMER_THREAD */

INTERNAL void rpc__timer_prod
#ifdef _DCE_PROTO_
(
        rpc_clock_t trigger
)
#else
(trigger)
    rpc_clock_t trigger;
#endif
{
                    
    RPC_DBG_PRINTF(rpc_e_dbg_timer, 5, (
        "(rpc__timer_prod) timer backup; old %d, new %d\n",
        rpc_timer_cur_trigger, trigger
        ));
    /* 
     * forestall "double pokes", which appear to
     * happen occasionally.
     */
    rpc_timer_cur_trigger = trigger;
    RPC_COND_SIGNAL(rpc_g_timer_cond, rpc_g_timer_mutex);
    
}


/*
 **  R P C _ _ T I M E R _ I N I T
 **
 **  Initialize the timer package.
 **/

PRIVATE unsigned32 rpc__timer_init(void)
{    
    RPC_TIMER_LOCK_INIT (0);
    RPC_TIMER_COND_INIT (0);
    
    running_list            = NULL;
    rpc_timer_high_trigger  = 0;
    rpc_timer_head          = NULL;
    rpc_timer_tail          = NULL;
    rpc_g_clock_curr        = 0;
    rpc_g_long_sleep	    = 0;
    timer_task_running      = false;
    timer_task_was_running  = false;
    disable_itimer	    = INITIAL_DISABLE_ITIMER_STATE;
    is_itimer_running       = false;
    is_itimer_enabled	    = false;
#ifndef _KERNEL
    timerclear(&last_itimer.it_value);
    timerclear(&last_itimer.it_interval);
    (void)sigemptyset(&itimer_act.sa_mask);
    itimer_act.sa_handler = rpc__timer_itimer_handler;
    itimer_act.sa_flags = 0;
#endif  /* _KERNEL */
    
    /*
     * Initialize the current time...
     */
    if (rpc_g_is_single_threaded)
    {
#ifndef _KERNEL
	char *cp = getenv("RPC_ITIMER_SIGNAL");

	if (cp != NULL)
	{
	    if (strcmp(cp, "SIGALRM") == 0)
	    {
		rpc_itimer_signal = SIGALRM;
		rpc_itimer_type = ITIMER_REAL;
		disable_itimer = false;
	    }
	    else if (strcmp(cp, "SIGVTALRM") == 0)
	    {
		rpc_itimer_signal = SIGVTALRM;
		rpc_itimer_type = ITIMER_VIRTUAL;
		disable_itimer = false;
	    }
	    else
		disable_itimer = true;
	}
	disable_itimer = true;
	if (! disable_itimer)
	{
	    if (! rpc__timer_itimer_install())
	    {
		if (rpc_itimer_signal == SIGALRM)
		{
		    /*
		     * Fallback to the virtual timer.
		     * Nobody should be using it since CMA will use it.
		     */
		    rpc_itimer_signal = SIGVTALRM;
		    rpc_itimer_type = ITIMER_VIRTUAL;

		    if (! rpc__timer_itimer_install())
			disable_itimer = true;
		}
		else
		    disable_itimer = true;
	    }
	}
	if (disable_itimer)
	{
            RPC_DBG_PRINTF(rpc_e_dbg_timer, 5, (
		"(rpc__timer_init) itimer disabled\n"));
	}
#endif  /* _KERNEL */
	rpc_g_long_sleep = true;
	rpc__clock_update();
    }
    else
	if (rpc__timer_start() == rpc_s_no_memory)
	    return(rpc_s_no_memory);

    return(rpc_s_ok);
}

/*
 **  R P C _ _ T I M E R _ S T A R T
 **
 **  Start the timer thread.
 **/
PRIVATE unsigned32 rpc__timer_start(void)
{
#ifndef NO_RPC_TIMER_THREAD
    if (timer_task_running)
	return(rpc_s_ok);

    (void)rpc__timer_itimer_stop();

    RPC_TIMER_LOCK(0);
    if (! timer_task_running)
    {
	rpc__clock_update();

	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_start) Starting the timer thread...\n"));

	stop_timer              = 0;
	timer_task_running      = true;
	TRY {
	pthread_create (
	    &timer_task,                            /* new thread    */
	    pthread_attr_default,                   /* attributes    */
	    (pthread_startroutine_t) timer_loop,    /* start routine */
	    (pthread_addr_t) NULL);                 /* arguments     */
	} CATCH_ALL {
	    timer_task_running = false;
            RPC_TIMER_UNLOCK(0);
	    return(rpc_s_no_memory);
	} ENDTRY
    }
    RPC_TIMER_UNLOCK(0);
#endif /* NO_RPC_TIMER_THREAD */
	return(rpc_s_ok);
}

#ifdef ATFORK_SUPPORTED
/*
 **  R P C _ _ T I M E R _ F O R K _ H A N D L E R 
 **
 **  Handle timer related fork issues.
 **/ 

PRIVATE void rpc__timer_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{  
    unsigned32 st;
    itimer_sigaction_t cact;
    struct itimerval vtimer;

#ifndef NO_RPC_TIMER_THREAD
    switch ((int)stage)
    {
        case RPC_C_PREFORK:
            RPC_TIMER_LOCK(0);
            timer_task_was_running = false;
            if (timer_task_running)
            {
                stop_timer = 1;
                rpc__timer_prod(0);
                RPC_TIMER_UNLOCK(0);
                pthread_join(timer_task, (pthread_addr_t *) &st);
                RPC_TIMER_LOCK(0);
                pthread_detach(&timer_task);
                timer_task_was_running = true;
                timer_task_running = false;
            }
            break;   

        case RPC_C_POSTFORK_PARENT:
            if (timer_task_was_running)
            {
                timer_task_was_running = false;
                timer_task_running = true;
                stop_timer = 0;
		TRY {
                pthread_create (
                    &timer_task,                          /* new thread    */
                    pthread_attr_default,                 /* attributes    */
                    (pthread_startroutine_t) timer_loop,  /* start routine */
                    (pthread_addr_t) NULL);               /* arguments     */
		} CATCH_ALL {
		/*
		 * rpc_m_call_failed
		 * "%s failed: %s"
		 */
		RPC_DCE_SVC_PRINTF (( 
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), 
		    rpc_svc_threads, 
		    svc_c_sev_fatal | svc_c_action_abort, 
		    rpc_m_call_failed, 
		    "rpc__timer_fork_handler",
		    "pthread_create failure")); 
		} ENDTRY
            }
            RPC_TIMER_UNLOCK(0);
            break;

        case RPC_C_POSTFORK_CHILD:  
	    if (sigaction(rpc_itimer_signal, NULL, &cact) != -1)
	    {
		if (cact.sa_handler == itimer_act.sa_handler)
		{
		    cact.sa_handler = SIG_DFL;
		    cact.sa_flags = 0;
		    (void)sigemptyset(&cact.sa_mask);
		    (void)sigaction(rpc_itimer_signal, &cact, NULL);

		    timerclear(&vtimer.it_value);
		    timerclear(&vtimer.it_interval);
		    (void)setitimer(rpc_itimer_type, &vtimer, &last_itimer);
		}
	    }
	    is_itimer_running = false;
	    is_itimer_enabled = false;
            timer_task_was_running = false;
            timer_task_running = false;
            stop_timer = 0;
            RPC_TIMER_UNLOCK(0);
            break;
    }
#endif /* NO_RPC_TIMER_THREAD */
}   
#endif /* ATFORK_SUPPORTED */


/*
** R P C _ _ T I M E R _ S E T 
** 
** Lock timer lock, add timer to timer callout queue, and unlock timer lock.
** 
*/

PRIVATE void rpc__timer_set 
#ifdef _DCE_PROTO_
(
    rpc_timer_p_t           t,
    rpc_timer_proc_p_t      proc,
    pointer_t               parg,
    rpc_clock_t             freq
)
#else
(t, proc, parg, freq)
rpc_timer_p_t           t;
rpc_timer_proc_p_t      proc;
pointer_t               parg;
rpc_clock_t             freq;
#endif
{    
    RPC_TIMER_LOCK (0);
    if (! timer_task_running)
	rpc__clock_update();
    rpc__timer_set_int (t, proc, parg, freq);
    RPC_TIMER_UNLOCK (0);
}

/*
**  R P C _ _ T I M E R _ S E T _ I N T
**
**  Insert a routine to be called from the rpc_timer periodic callout queue.
**  The entry address, a single argument to be passed when the routine is
**  called,  together with the frequency with which the routine is to be 
**  called are inserted into a rpc_timer_t structure provided by the 
**  caller and pointed to by 't', which is placed in the queue.
*/


INTERNAL void rpc__timer_set_int 
#ifdef _DCE_PROTO_
(
    rpc_timer_p_t           t,
    rpc_timer_proc_p_t      proc,
    pointer_t               parg,
    rpc_clock_t             freq
)
#else
(t, proc, parg, freq)
rpc_timer_p_t           t;
rpc_timer_proc_p_t      proc;
pointer_t               parg;
rpc_clock_t             freq;
#endif
{    
    rpc_timer_p_t       list_ptr, prev_ptr;
    
    /*
     * Insert the routine calling address, the argument to be passed and
     * the frequency into the rpc_timer_t structure.
     */       
    
    t->proc = proc;
    t->parg = parg;
    t->frequency = freq;
    t->trigger = freq + rpc_g_clock_curr;

    /*
     * Insert the rpc_timer_t structure sent by the caller into the
     * rpc_timer queue, by order of the frequency with which it is to be
     * called.
     */
    if (rpc_timer_head != NULL && t->trigger >= rpc_timer_high_trigger)
    {            
        rpc_timer_tail->next = t;
        rpc_timer_tail = t;
        t->next = 0;
        rpc_timer_high_trigger = t->trigger;
        return;
    }          
    
    /*
     * Handle the case in which the element gets inserted somewhere
     * into the middle of the list.
     */                             
    
    prev_ptr = NULL;
    for (list_ptr = rpc_timer_head; list_ptr; list_ptr = list_ptr->next )
    {
        if (t->trigger < list_ptr->trigger)
        {
            if (list_ptr == rpc_timer_head)
            {         
                t->next = rpc_timer_head;
                rpc_timer_head = t;
                /*
                 * If the next time the timer thread will wake up is
                 * in the future, prod it.
                 */
                if (rpc_timer_cur_trigger > t->trigger) 
                    rpc__timer_prod(t->trigger);
            }
            else
            {
                prev_ptr->next = t;
                t->next = list_ptr;
            }
            return;
        }
        prev_ptr = list_ptr;
    }  
    
    /*
     * The only way to get this far is when the head pointer is NULL.  Either
     * we just started up, or possibly there's been no RPC activity and all the
     * monitors have removed themselves.
     */    
    assert (rpc_timer_head == NULL);
    
    rpc_timer_head = rpc_timer_tail = t;
    t->next = NULL;
    rpc_timer_high_trigger = t->trigger;
    if (rpc_timer_cur_trigger > t->trigger) 
        rpc__timer_prod(t->trigger);
}

/*
 **  R P C _ _ T I M E R _ A D J U S T
 **
 **  Search the rpc_timer queue for the rpc_timer_t structure pointed to by
 **   't'.  Then modify the 'frequency' attribute.
 **/

PRIVATE void rpc__timer_adjust 
#ifdef _DCE_PROTO_
(
    rpc_timer_p_t           t,
    rpc_clock_t             frequency           
)
#else
(t, frequency)
rpc_timer_p_t           t;
rpc_clock_t             frequency;           
#endif
{   
    rpc_timer_p_t       ptr;
    
    /*
     * First see if the monitor being adjusted is currently running.
     * If so, just reset the frequency field, when the monitor is rescheduled
     * it will be queued in the proper place.
     */                             
    
    RPC_TIMER_LOCK (0);
    for (ptr = running_list; ptr != NULL; ptr = ptr->next)
    {
        if (t == ptr)
        {
            t->frequency = frequency;
            RPC_TIMER_UNLOCK (0);
            return;
        }
    }
    
    RPC_TIMER_UNLOCK (0);
    /*
     * Otherwise, we need to remove the monitor from its current position
     * in the queue, adjust the frequency, and then replace it in the correct
     * position.
     */
    
    rpc__timer_clear (t);
    t->frequency = frequency;
    rpc__timer_set (t, t->proc, t->parg, t->frequency);
}

/*
 ** R P C _ _ T I M E R _ C L E A R
 **
 **  Remove an rpc_timer_t structure from the rpc_timer scheduling queue.
 **/

PRIVATE void rpc__timer_clear 
#ifdef _DCE_PROTO_
(
    rpc_timer_p_t           t
)
#else
(t)
rpc_timer_p_t           t;
#endif
{  
    rpc_timer_p_t       list_ptr, prev = NULL;
    
    RPC_TIMER_LOCK (0);
    
    /*
     * First see if the monitor being cleared is currently running.
     * If so, remove it from the running list.
     */  
    
    for (list_ptr = running_list; list_ptr; prev = list_ptr, list_ptr = list_ptr->next)
    {
        if (list_ptr == t)                  
        {
            if (prev == NULL)
                running_list = running_list->next;
            else
                prev->next = list_ptr->next;
            RPC_TIMER_UNLOCK (0);
            return;
        }
    }
    
    /*
     * Next, see if the specified timer element is in the list.
     */
    
    prev = NULL;
    for (list_ptr = rpc_timer_head; list_ptr;
        prev = list_ptr, list_ptr = list_ptr->next)
    {
        if (list_ptr == t)
        {                   
            /*
             * If this element was at the head of the list...
             */
            if (t == rpc_timer_head)
            {
                /*
                 * !!! perhaps need to inform timer thread that next event
                 * is a little further off?
                 */
                rpc_timer_head = t->next;
            }
            else
            {      
                /*
                 * Unlink element from list.  If the element was at the end
                 * of the list, reset the tail pointer and high trigger value.
                 */
                prev->next = t->next;
                if (t->next == NULL)
                {
                    rpc_timer_tail = prev;
                    rpc_timer_high_trigger = prev->trigger;
                }
            }
            RPC_TIMER_UNLOCK (0);
            return;
        }                 
    }   
    
    RPC_TIMER_UNLOCK (0);
}

/*
**  R P C _ _ T I M E R _ C A L L O U T
**
**  Make a pass through the rpc_timer_t periodic callout queue.
**  Make a call to any routines which are ready to run at this time.
**  The mutex, RPC_TIMER_LOCK, is LOCKED while this routine is running
**  and while any routines which may be calld are running.
**/

PRIVATE void rpc__timer_callout (void)
{
    rpc_clock_t next;
#ifndef SGIMIPS			/*  Not used */
    rpc_clock_t	max_step = RPC_CLOCK_SEC(50);
#endif
    int oc;

    if (timer_task_running)
	return;

    RPC_TIMER_LOCK (0);
    if (! timer_task_running)
    {
        rpc__clock_update();

	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_callout) executing the timer chain...\n"));

	/*
	 * We need to disable general cancelability for the event of a timer
	 * routine throwing a cancel to us.
	 */
	oc = pthread_setcancel(CANCEL_OFF);

	next = rpc__timer_callout_int();

	/*
	 * Restore the original cancelability state.
	 */
	pthread_setcancel(oc);

	rpc_g_long_sleep = ((next > RPC_CLOCK_SEC(1)) || (next == 0));

	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_callout) next event in %d seconds\n",
	      next/RPC_C_CLOCK_HZ));
    }
    RPC_TIMER_UNLOCK (0);

    return;
}

INTERNAL rpc_clock_t rpc__timer_callout_int (void)
{                              
    unsigned32      ret_val;
    rpc_timer_p_t   ptr, prev;

#if 0    
    RPC_TIMER_LOCK (0);
#endif

    RPC_TIMER_LOCK_ASSERT(0);
    
    /*
     * Go through list, running routines which have trigger counts which
     * have expired.
     */
    
    while (rpc_timer_head && rpc_timer_head->trigger <= rpc_g_clock_curr)
    { 
        ptr = rpc_timer_head;
        rpc_timer_head = rpc_timer_head->next;
        ptr->next = running_list;
        running_list = ptr;

        RPC_TIMER_UNLOCK (0);
        (*ptr->proc) (ptr->parg);
        RPC_TIMER_LOCK (0);
        
        /* 
         * We need to handle two situations here depending on whether
         * the monitor routine has deleted itself from the timer queue.
         * If so, we'll know this from the fact that it will be gone
         * from the running list.  If not, we need to update the head
         * pointer here, and reschedule the current monitor.
         */                                
        
        if (ptr == running_list)
        {
            running_list = running_list->next;
            rpc__timer_set_int (ptr, ptr->proc, ptr->parg, ptr->frequency);
        }
        else
        {
            for (prev = running_list; prev; prev = prev->next)
            {
                if (prev->next == ptr)
                {
                    prev->next = ptr->next;

                    rpc__timer_set_int 
                        (ptr, ptr->proc, ptr->parg, ptr->frequency);

                }
            } 
        }
    }
    
    
    ret_val = (rpc_timer_head == NULL) ? 0
        : (rpc_timer_head->trigger - rpc_g_clock_curr);
    
    RPC_TIMER_LOCK_ASSERT(0);    
    return (ret_val);
} 


#ifndef _KERNEL
/*
**  R P C _ _ T I M E R _ I T I M E R _ I N S T A L L
**
**  Install rpc__timer_itimer_handler() for an appropriate signal.
**
**  Due to the lack of the mutex, this can be safely called by
**  rpc__timer_init() only.
**/
INTERNAL boolean32 rpc__timer_itimer_install (void)
{
    itimer_sigaction_t cact;
    sigset_t itimer_signal;
    sigset_t oset;

    if (disable_itimer)
	return (false);

    /*
     * Mask our signal.
     */
    if (sigemptyset(&itimer_signal) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigemptyset",
	     errno ));
	return (false);
    }
    if (sigaddset(&itimer_signal, rpc_itimer_signal) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigaddset",
	     errno ));
	return (false);
    }

    /*
     * Mask everything.
     */
    if (sigfillset(&itimer_act.sa_mask) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigfillset",
	     errno ));
	return (false);
    }
    if (sigdelset(&itimer_act.sa_mask, SIGKILL) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigdelset",
	     errno ));
	return (false);
    }
    if (sigdelset(&itimer_act.sa_mask, SIGSTOP) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigdelset",
	     errno ));
	return (false);
    }
    if (sigdelset(&itimer_act.sa_mask, SIGCONT) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigdelset",
	     errno ));
	return (false);
    }
    if (sigdelset(&itimer_act.sa_mask, SIGTRAP) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigdelset",
	     errno ));
	return (false);
    }

    /*
     * Install signal handler
     */
    if (sigprocmask(SIG_BLOCK, &itimer_signal, &oset) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigprocmask",
	     errno ));
	return (false);
    }
    if (sigaction(rpc_itimer_signal, &itimer_act, &cact) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigaction",
	     errno ));

	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	return (false);
    }

    if (cact.sa_handler != SIG_DFL)
    {
	/*
	 * Someone is already using this signal.
	 */
	goto cleanup;
    }

    /*
     * Restore mask without our signal.
     */
    if (sigdelset(&oset, rpc_itimer_signal) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigdelset",
	     errno ));
	goto cleanup;
    }
    if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_fatal | svc_c_action_abort,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigprocmask",
	     errno ));
    }

#if defined(CMA_INCLUDE) && defined(__hpux)
    _cma_atexec_hp((void (*)())rpc__timer_itimer_stop, NULL);
#endif  /* defined(CMA_INCLUDE) */

    RPC_DBG_PRINTF
	(rpc_e_dbg_timer, 5,
	 ("(rpc__timer_itimer_install) installed the itimer (%d).\n",
	  rpc_itimer_signal));

    return (true);

cleanup:
    if (sigaction(rpc_itimer_signal, &cact, NULL) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF (
	    (DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	     rpc_svc_general,
	     svc_c_sev_warning | svc_c_action_none,
	     rpc_m_call_failed_errno,
	     "(rpc__timer_itimer_install) sigaction",
	     errno ));
    }
    (void)sigprocmask(SIG_SETMASK, &oset, NULL);
    return (false);
}

/*
**  R P C _ _ T I M E R _ I T I M E R _ H A N D L E R
**
**  Make a pass through the rpc_timer_t periodic callout queue.
**/
INTERNAL void rpc__timer_itimer_handler
#ifdef _DCE_PROTO_
(
    int	sig,
    int	code,
    struct sigcontext *scp
)
#else
(sig, code, scp)
    int	sig;
    int	code;
    struct sigcontext *scp;
#endif
{
    struct itimerval vtimer;
    boolean gotit;
    rpc_clock_t next;
    rpc_clock_t	max_step = RPC_CLOCK_SEC(50);
    int oc;

#if defined(CMA_INCLUDE) && defined(__hpux)
    /*
     * If we are in CMA kernel or TCB is locked, just return.
     */
    if (cma__hp_kernel_locked() || cma__hp_self_tcb_locked())
	return;
#endif  /* defined(CMA_INCLUDE) */

    if (!RPC_IS_SINGLE_THREADED(0) && sig == rpc_itimer_signal)
    {
	/*
	 * We are no longer single-threaded.
	 *
	 * Do not make any call which can end up in CMA
	 * (including a debug output)!
	 * It will cause the CMA assertion failure.
	 */
	timerclear(&vtimer.it_value);
	timerclear(&vtimer.it_interval);
	disable_itimer = true;
	is_itimer_running = false;
	(void)setitimer(rpc_itimer_type, &vtimer, NULL);
	return;
    }

    RPC_DBG_PRINTF(rpc_e_dbg_timer, 7, ("(rpc__timer_itimer_handler)\n"));

#ifdef ATFORK_SUPPORTED
    if (disable_itimer || sig != rpc_itimer_signal
	|| timer_task_running || rpc__fork_is_in_progress())
	return;
#else
    if (disable_itimer || sig != rpc_itimer_signal
	|| timer_task_running)
	return;
#endif /* ATFORK_SUPPORTED */

    if (getitimer(rpc_itimer_type, &vtimer) == -1)
	return;

    RPC_TRY_LOCK(&gotit);
    if (!gotit)
	goto retry_later;

    RPC_TIMER_TRY_LOCK(&gotit);
    RPC_UNLOCK(0);
    if (!gotit)
	goto retry_later;

    if (! timer_task_running)
    {
        rpc__clock_update();

	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_itimer_handler) executing the timer chain...\n"));

	/*
	 * We need to disable general cancelability for the event of a timer
	 * routine throwing a cancel to us.
	 */
	oc = pthread_setcancel(CANCEL_OFF);

	next = rpc__timer_callout_int();

	/*
	 * Restore the original cancelability state.
	 */
	pthread_setcancel(oc);

	rpc_g_long_sleep = ((next > RPC_CLOCK_SEC(1)) || (next == 0));
	if (next > max_step)
	    next = max_step;

	if (next > 10)
	{
	    RPC_DBG_PRINTF
		(rpc_e_dbg_timer, 5,
		 ("(rpc__timer_itimer_handler) next event in %d seconds\n",
		  next/RPC_C_CLOCK_HZ));
	}
    }

    if (next != 0)
    {
	vtimer.it_value.tv_sec  = (next / RPC_C_CLOCK_HZ);
	vtimer.it_value.tv_usec =
	    (next % RPC_C_CLOCK_HZ) * (1000000L/RPC_C_CLOCK_HZ);
	/*
	 * If retry is necessary, reload 1 rpc clock tick.
	 */
	vtimer.it_interval.tv_sec  = 0;
	vtimer.it_interval.tv_usec = (1000000L/RPC_C_CLOCK_HZ);
    }
    else
    {
	/*
	 * Nothing to run, so shut ourself down.
	 */
	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_itimer_handler) Shuting down the itimer...\n"));

	timerclear(&vtimer.it_value);
	timerclear(&vtimer.it_interval);
	is_itimer_running = false;
    }

    if (setitimer(rpc_itimer_type, &vtimer, NULL) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	rpc_svc_general,
	svc_c_sev_warning | svc_c_action_none,
	rpc_m_call_failed_errno,
	"(rpc__timer_itimer_handler) setitimer",
	errno ));
    }
    else
    {
	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 7,
	     ("(rpc__timer_itimer_handler) Adjusted the itimer %d.%d %d.%d\n",
	      vtimer.it_value.tv_sec, vtimer.it_value.tv_usec,
	      vtimer.it_interval.tv_sec, vtimer.it_interval.tv_usec));
    }

    RPC_TIMER_UNLOCK(0);
    return;

retry_later:
    RPC_DBG_PRINTF
        (rpc_e_dbg_timer, 7,
	 ("(rpc__timer_itimer_handler) retry later.\n"));
    /*
     * The next timeout should be 1 rpc clock tick set by
     * rpc__timer_itimer_start() or this function.
     */
    return;
}
#endif  /* _KERNEL */

/*
**  R P C _ _ T I M E R _ I T I M E R _ S T A R T
**
** Start the itimer if enabled.
**/
PRIVATE void rpc__timer_itimer_start(void)
{
#ifndef	_KERNEL
    itimer_sigaction_t cact;
    struct itimerval vtimer;
    sigset_t signal_mask;

    if (disable_itimer || !rpc_g_is_single_threaded ||
	is_itimer_enabled == false || timer_task_running)
	return;

    /*
     * If the signal handler is changed (probably CMA installed it),
     * don't start the itimer.
     * Note: We check it without lock...
     */
    if (sigaction(rpc_itimer_signal, NULL, &cact) == -1)
	return;

    if (cact.sa_handler != itimer_act.sa_handler)
    {
	disable_itimer = true;
	return;
    }

    RPC_TIMER_LOCK(0);

#ifdef	MAX_DEBUG
    if (sigprocmask(SIG_BLOCK, NULL, &signal_mask) != -1)
    {
	switch (sigismember(&signal_mask, rpc_itimer_signal))
	{
	case 1:
	    RPC_DBG_PRINTF
	        (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_itimer_start) Unblocking signal (%d)...\n",
		  rpc_itimer_signal));
	    if (sigdelset(&signal_mask, rpc_itimer_signal) == -1)
	    {
		/*
		 * rpc_m_call_failed_errno
		 * "%s failed, errno = %d"
		 */
		RPC_DCE_SVC_PRINTF ((
		    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
		rpc_svc_general,
		svc_c_sev_warning | svc_c_action_none,
		rpc_m_call_failed_errno,
		"(rpc__timer_itimer_start) sigdelset",
		errno ));
	    }
	    if (sigprocmask(SIG_SETMASK, &signal_mask, NULL) == -1)
	    {
		/*
		 * rpc_m_call_failed_errno
		 * "%s failed, errno = %d"
		 */
		RPC_DCE_SVC_PRINTF ((
		    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
		rpc_svc_general,
		svc_c_sev_warning | svc_c_action_none,
		rpc_m_call_failed_errno,
		"(rpc__timer_itimer_start) sigprocmask",
		errno ));
	    }
	    break;
	case 0:
	    RPC_DBG_PRINTF
	        (rpc_e_dbg_timer, 5,
		 ("(rpc__timer_itimer_start) signal (%d) is unblocked\n",
		  rpc_itimer_signal));
	    break;
	default:
	    /*
	     * rpc_m_call_failed_errno
	     * "%s failed, errno = %d"
	     */
	    RPC_DCE_SVC_PRINTF ((
		DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_warning | svc_c_action_none,
	    rpc_m_call_failed_errno,
	    "(rpc__timer_itimer_start) sigismember",
	    errno ));
	    break;
	}
    }
    else
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	rpc_svc_general,
	svc_c_sev_warning | svc_c_action_none,
	rpc_m_call_failed_errno,
	"(rpc__timer_itimer_start) sigprocmask",
	errno ));
    }
#endif	/* MAX_DEBUG */

    is_itimer_running = true;

    vtimer.it_value.tv_sec     = 1;	/* be aggressive */
    vtimer.it_value.tv_usec    = 0;

    if (timerisset(&last_itimer.it_value) &&
	timercmp(&last_itimer.it_value, &vtimer.it_value, <))
    {
	vtimer.it_value = last_itimer.it_value;
    }

    /*
     * If retry is necessary, reload 1 rpc clock tick.
     */
    vtimer.it_interval.tv_sec  = 0;
    vtimer.it_interval.tv_usec = (1000000L/RPC_C_CLOCK_HZ);

    if (setitimer(rpc_itimer_type, &vtimer, NULL) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	rpc_svc_general,
	svc_c_sev_warning | svc_c_action_none,
	rpc_m_call_failed_errno,
	"(rpc__timer_itimer_start) setitimer",
	errno ));
    }
    else
    {
	RPC_DBG_PRINTF
	    (rpc_e_dbg_timer, 5,
	     ("(rpc__timer_itimer_start) Started the itimer %d.%d %d.%d\n",
	      vtimer.it_value.tv_sec, vtimer.it_value.tv_usec,
	      vtimer.it_interval.tv_sec, vtimer.it_interval.tv_usec));
    }

    RPC_TIMER_UNLOCK(0);
#endif  /* _KERNEL */
    return;
}

/*
**  R P C _ _ T I M E R _ I T I M E R _ S T O P
**
** Stop the itimer.
**/
PRIVATE boolean32 rpc__timer_itimer_stop(void)
{
    boolean was_running = false;
#ifndef _KERNEL
    itimer_sigaction_t cact;
    struct itimerval vtimer;

    /*
     * If the signal handler is changed (probably CMA installed it),
     * don't stop the itimer.
     * Note: We check it without lock...
     */
    if (sigaction(rpc_itimer_signal, NULL, &cact) == -1)
	return (was_running);

    if (cact.sa_handler != itimer_act.sa_handler)
    {
	disable_itimer = true;
	return (was_running);
    }

    if (is_itimer_running == true)
    {
	RPC_TIMER_LOCK(0);

	if (is_itimer_running == true)
	{
	    timerclear(&vtimer.it_value);
	    timerclear(&vtimer.it_interval);

	    if (setitimer(rpc_itimer_type, &vtimer, &last_itimer) == -1)
	    {
		/*
		 * rpc_m_call_failed_errno
		 * "%s failed, errno = %d"
		 */
		RPC_DCE_SVC_PRINTF ((
		    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
		rpc_svc_general,
		svc_c_sev_warning | svc_c_action_none,
		rpc_m_call_failed_errno,
		"(rpc__timer_itimer_stop) setitimer",
		errno ));
	    }
	    else
	    {
		RPC_DBG_PRINTF
		    (rpc_e_dbg_timer, 5,
		     ("(rpc__timer_itimer_stop) Stopped the itimer %d.%d %d.%d\n",
		      last_itimer.it_value.tv_sec,
		      last_itimer.it_value.tv_usec,
		      last_itimer.it_interval.tv_sec,
		      last_itimer.it_interval.tv_usec));
	    }

	    is_itimer_running = false;
	    was_running = true;
	}

	RPC_TIMER_UNLOCK(0);
    }
#endif  /* _KERNEL */
    return (was_running);
}

/*
**  R P C _ _ T I M E R _ I T I M E R _ E N A B L E
**
** Enable the itimer, but doesn't start it.
**/
PRIVATE boolean32 rpc__timer_itimer_enable(void)
{
    if (disable_itimer)
	return(false);
#ifndef _KERNEL
    if (is_itimer_enabled == false)
    {
	RPC_TIMER_LOCK(0);
	if (is_itimer_enabled == false)
	{
	    RPC_DBG_PRINTF
		(rpc_e_dbg_timer, 5,
		 ("(rpc__timer_itimer_enable) Enabling the itimer...\n"));
	    is_itimer_enabled = true;
	}
	RPC_TIMER_UNLOCK(0);
    }
#endif  /* _KERNEL */
    return (is_itimer_enabled);
}


/*
 **  R P C _ _ T I M E R _ U P D A T E _ C L O C K
 **
 **  Forcefully update the current rpcclock's tick count.
 **/
PRIVATE void rpc__timer_update_clock (void)
{
    RPC_TIMER_LOCK (0);
    rpc__clock_update();
    RPC_TIMER_UNLOCK (0);
    return;
}
