/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgclive.c,v 65.7 1998/03/23 17:30:42 gwehrman Exp $";
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
 * $Log: dgclive.c,v $
 * Revision 65.7  1998/03/23 17:30:42  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/03/09 19:31:44  lmc
 * Remove setting of process name because kernel threads are now sthreads
 * instead of uthreads and thus don't have a process name - only a kthread
 * name.
 *
 * Revision 65.5  1998/01/07  17:21:42  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.4  1997/12/16  17:23:12  lmc
 * Removed prototypes that were no longer correct and/or existed in a .h file now.
 *
 * Revision 65.3  1997/11/06  19:57:42  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:55  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.512.1  1996/05/10  13:11:27  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:13 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:42 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the fix for CHFts15608.
 * 	[1995/07/14  19:06 UTC  tatsu_s  /main/HPDCE02/5]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:01 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:32 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the fix for CHFts15608.
 * 	[1995/07/14  19:06 UTC  tatsu_s  /main/HPDCE02/5]
 *
 * 	Added volatile.
 * 	Fixed the frequency of INDY.
 * 	[1995/07/06  21:00 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b4/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/4]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:01 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 *
 * 	fix typo
 * 	[1995/04/03  13:20 UTC  mothra  /main/HPDCE02/3]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:14 UTC  lmm  /main/HPDCE02/2]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:19 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:18 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:25 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 *
 * Revision 1.1.508.4  1994/07/15  16:56:19  tatsu_s
 * 	Fix OT10589: Added rpc__dg_maintain_fork_handler().
 * 	Fix OT11153: Changed svc_c_action_abort to svc_c_action_exit_bad.
 * 	[1994/07/09  02:44:18  tatsu_s]
 * 
 * Revision 1.1.508.3  1994/06/21  21:53:55  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:31:18  hopkins]
 * 
 * Revision 1.1.508.2  1994/05/19  20:11:23  tom
 * 	Bug 6224 - Check return value of uuid_create.
 * 	[1994/05/18  21:22:58  tom]
 * 
 * Revision 1.1.508.1  1994/01/21  22:36:40  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:56:52  cbrooks]
 * 
 * Revision 1.1.4.4  1993/01/03  23:23:54  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:04:42  bbelch]
 * 
 * Revision 1.1.4.3  1992/12/23  20:47:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:36:28  zeliff]
 * 
 * Revision 1.1.4.2  1992/10/02  20:16:42  markar
 * 	  CR 5350: Make convc_indy calls on unauthenticated binding.
 * 	[1992/10/01  20:17:06  markar]
 * 
 * Revision 1.1.2.2  1992/05/07  17:59:01  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:27:51  markar]
 * 
 * Revision 1.1  1992/01/19  03:07:22  devrcs
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
**      dgclive.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for maintaining liveness of clients.
**
**
*/

#include <dg.h>
#include <dgglob.h>

#include <dce/conv.h>
#include <dce/convc.h>
#ifdef KERNEL
#include <commonp.h>
#endif /* KERNEL */

/* =============================================================================== */

/*
 * Define a linked list element which the client runtime can use to keep
 * track of which servers it needs to maintain liveness with.  
 *
 * Here's the list elements...
 */
typedef struct maint_elt_t
{
    struct maint_elt_t   *next;       /* -> to next entry in list.       */
    rpc_binding_rep_p_t  shand;       /* -> to server binding handle     */
    unsigned8            refcnt;      /* -> # of entries for this server */
} maint_elt_t, *maint_elt_p_t;

/*
 * And here's the head of the list...
 */
INTERNAL volatile maint_elt_p_t maint_head;

/*
 * And here's the mutex that protects the list...
 */
INTERNAL rpc_mutex_t   rpc_g_maint_mutex;
INTERNAL rpc_cond_t    maintain_cond;

INTERNAL volatile boolean maintain_thread_running = false;
INTERNAL volatile boolean maintain_thread_was_running = false;
INTERNAL volatile boolean stop_maintain_thread = false;

INTERNAL pthread_t  maintain_task;
  
/*
 * Mutex lock macros 
 */

#define RPC_MAINT_LOCK_INIT()   RPC_MUTEX_INIT (rpc_g_maint_mutex)
#define RPC_MAINT_LOCK()        RPC_MUTEX_LOCK (rpc_g_maint_mutex)
#define RPC_MAINT_UNLOCK()      RPC_MUTEX_UNLOCK (rpc_g_maint_mutex)

/* ========================================================================= */

INTERNAL void network_maintain_liveness _DCE_PROTOTYPE_((void));

/* ========================================================================= */

/*
 * R P C _ _ D G _ N E T W O R K _ M A I N T
 *
 * This function is called, via the network listener service, by a client
 * stub which needs to maintain context with a server.  A copy of the
 * binding handle is made and entered into a list associated with a timer
 * monitor.  This monitor will then periodically send an identifier to
 * the server to assure it that this client is still alive.
 */

PRIVATE void rpc__dg_network_maint
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t binding_r,
    unsigned32 *st
)
#else
(binding_r, st)
rpc_binding_rep_p_t binding_r;
unsigned32 *st;
#endif
{               
    volatile maint_elt_p_t maint;
    volatile rpc_dg_binding_client_p_t chand =
	(rpc_dg_binding_client_p_t) binding_r;
  
    *st = rpc_s_ok;

    /*
     * Start the timer thread if not running yet.
     * We start it here (before taking the list's mutex) because we take the
     * global lock. This happens only once 'cause we can't go back to
     * single-threaded...
     */
    if (rpc_g_is_single_threaded)
    {
	RPC_LOCK(0);
	if ((*st =rpc__timer_start()) == rpc_s_no_memory){
	    RPC_UNLOCK(0);
	    return;
	}
	rpc_g_is_single_threaded = false;
	RPC_UNLOCK(0);
    }

    RPC_MAINT_LOCK();

    /*
     * First, we need to traverse the list of maintained contexts to see
     * if this server is already on it.  If we find a matching  address,
     * we can just return.
     */
                                            
    for (maint = maint_head; maint != NULL; maint = maint->next)
    {
        if (rpc__naf_addr_compare(maint->shand->rpc_addr, binding_r->rpc_addr, st))
        {          
            /*
             * If we find a matching element, store a pointer to it in the
             * client binding handle (so we don't have to do these compares
             * when maint_stop gets called) and note the reference.
             */

            chand->maint_binding = maint->shand;
            maint->refcnt++;
            RPC_MAINT_UNLOCK();
            return;       
        }
    }
      
    /*
     * Need to make a new entry in the maintain list.  Alloc up a
     * list element.
     */

    RPC_MEM_ALLOC(maint, maint_elt_p_t, sizeof *maint, 
            RPC_C_MEM_DG_MAINT, RPC_C_MEM_NOWAIT);
    if (maint == NULL){
	*st = rpc_s_no_memory;
	RPC_MAINT_UNLOCK();
	return;
    }

    /*
     * Make our own copy of the binding handle (so we have a handle to 
     * send INDY's to this server).  Reference the new binding in the
     * client handle and note the reference.
     */
  
    rpc_binding_copy((rpc_binding_handle_t) chand, 
                     (rpc_binding_handle_t *) &maint->shand, st);
    chand->maint_binding = maint->shand;
    maint->refcnt = 1;

    /*
     * and thread it onto the head of the list.
     */                                    

    maint->next = maint_head;
    maint_head = maint;
        
    /*
     * If the binding handle had any authentication info associated with
     * it, free it up now.   We don't want the convc_indy() calls using
     * authentication.
     */
    rpc_binding_set_auth_info((rpc_binding_handle_t) maint->shand, NULL, 
                       rpc_c_protect_level_none, rpc_c_authn_none, NULL, 
                       rpc_c_authz_none, st);
        
    /*
     * Finally, check to make sure the 'context maintainer' thread has
     * been started, and if not, start it up.
     */

    if (! maintain_thread_running)
    {     
        maintain_thread_running = true;
	TRY {
        pthread_create(&maintain_task, pthread_attr_default, 
            (pthread_startroutine_t) network_maintain_liveness, 
            (pthread_addr_t) NULL);  
	} CATCH_ALL {
	    maintain_thread_running = false;

	    chand->maint_binding = NULL;
	    maint_head = maint->next;
	    rpc_binding_free((rpc_binding_handle_t *)&maint->shand, st);
	    RPC_MEM_FREE(maint, RPC_C_MEM_DG_MAINT);
	    *st = rpc_s_no_memory;
	    RPC_MAINT_UNLOCK();
	    return;
	} ENDTRY
	
    }

    RPC_MAINT_UNLOCK();
}

/*
 * R P C _ _ D G _ N E T W O R K _ S T O P _ M A I N T
 *
 * This routine is called, via the network listener service, by a client stub
 * when it wishes to discontinue maintaining context with a server.  
 */

PRIVATE void rpc__dg_network_stop_maint
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t binding_r,
    unsigned32 *st
)
#else
(binding_r, st)
rpc_binding_rep_p_t binding_r;
unsigned32 *st;
#endif
{
    maint_elt_p_t maint, prev = NULL;
    rpc_dg_binding_client_p_t chand = (rpc_dg_binding_client_p_t) binding_r;
                                   
    RPC_MAINT_LOCK();

    /*
     * Search through the list for the element which references this
     * binding handle.                                         
     */

    for (maint = maint_head; maint != NULL; maint = maint->next)
    {     
        if (chand->maint_binding == maint->shand)               
        {                    
            /*
             * Remove the reference from the binding handle, and decrement 
             * the reference count in the list element.  If the count
             * falls to 0, remove the element from the list.
             */
            
            chand->maint_binding = NULL;
            if (--maint->refcnt == 0) 
            {
                if (prev == NULL)
                    maint_head = maint->next;
                else                              
                    prev->next = maint->next;
                rpc_binding_free((rpc_binding_handle_t *)&maint->shand, st);
                RPC_MEM_FREE(maint, RPC_C_MEM_DG_MAINT);
            }          
            *st = rpc_s_ok;
            RPC_MAINT_UNLOCK();
            return;
        }
        prev = maint;
    }
                         
    RPC_MAINT_UNLOCK();
    *st = -1;           /*!!! didn't find it, need real error value here */
}

/*
 * N E T W O R K _ M A I N T A I N _ L I V E N E S S
 *
 * Base routine for the thread which periodically sends out the address
 * space UUID of this process.  This UUID uniquely identifies this
 * particular instance of this particular client process for use in
 * maintaing context with servers.
 */

INTERNAL void network_maintain_liveness(void)
{
    maint_elt_p_t ptr;
    struct timespec next_ts;

    RPC_DBG_PRINTF(rpc_e_dbg_conv_thread, 1, 
                   ("(network_maintain_liveness) starting up...\n"));

    RPC_MAINT_LOCK();

    while (stop_maintain_thread == false)
    {
        /*
         * Send out INDYs every 20 seconds.
         */
        rpc__clock_timespec(rpc__clock_stamp()+RPC_CLOCK_SEC(20), &next_ts);

        RPC_COND_TIMED_WAIT(maintain_cond, rpc_g_maint_mutex, &next_ts);
        if (stop_maintain_thread == true)
            break;

        for (ptr = maint_head; ptr != NULL; ptr = ptr->next)
        {                
            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                ("(maintain_liveness_timer) Doing convc_indy call\n"));
                              
            (*convc_v1_0_c_epv.convc_indy)((handle_t) ptr->shand, 
                                                   &rpc_g_dg_my_cas_uuid);
        }                
                  
        /*
         * See if the list is empty...
         */
        
#ifndef _KERNEL                                 
        if (maint_head == NULL)
        {
            /*
             * Nothing left to do, so terminate the thread.
             */
            pthread_detach(&maintain_task);
            maintain_thread_running = false;
            break;
        }
#endif
    }
    RPC_DBG_PRINTF(rpc_e_dbg_conv_thread, 1, 
                   ("(network_maintain_liveness) shutting down%s...\n",
                    (maint_head == NULL)?" (no active)":""));

    RPC_MAINT_UNLOCK();
}

/*
 * R P C _ _ D G _ M A I N T A I N _ I N I T
 *
 * This routine performs any initializations required for the network
 * listener service maintain/monitor functions.  Note that way2 liveness
 * callbacks are handled by the conversation manager interface; we do
 * not need to register the interfaces because the runtime intercepts
 * and handles way2 callbacks as part of listener thread request
 * processing.
 */
   
PRIVATE void rpc__dg_maintain_init(void)
{               
    unsigned32 st;

    /*
     * Gen the address space UUID that we will send to servers
     * to indicate that we're still alive.
     */

    uuid_create(&rpc_g_dg_my_cas_uuid, &st); 
    if (st != rpc_s_ok)
    {
	/*
	 * rpc_m_cant_create_uuid
	 * "(%s) Can't create UUID"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_exit_bad,
	    rpc_m_cant_create_uuid,
	    "rpc__dg_maintain_init" ));
    }

    /*
     * Initialize a private mutex.
     */

    RPC_MAINT_LOCK_INIT(); 
    RPC_COND_INIT(maintain_cond, rpc_g_maint_mutex);

    maint_head = NULL;
    maintain_thread_running = false;
    maintain_thread_was_running = false;
    stop_maintain_thread = false;
}

#ifdef ATFORK_SUPPORTED
/*
 * R P C _ _ D G _ M A I N T A I N _ F O R K _ H A N D L E R
 *
 * Handle fork related processing for this module.
 */

PRIVATE void rpc__dg_maintain_fork_handler
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

    switch ((int)stage)
    {
    case RPC_C_PREFORK:
        RPC_MAINT_LOCK();
        maintain_thread_was_running = false;
        
        if (maintain_thread_running) 
        {
            stop_maintain_thread = true;
            RPC_COND_SIGNAL(maintain_cond, rpc_g_maint_mutex);
            RPC_MAINT_UNLOCK();
            pthread_join (maintain_task, (pthread_addr_t *) &st);
            RPC_MAINT_LOCK();
            pthread_detach(&maintain_task);
            maintain_thread_running = false;
            maintain_thread_was_running = true;
        }
        break;
    case RPC_C_POSTFORK_PARENT:
        if (maintain_thread_was_running) 
        {
            maintain_thread_was_running = false;
            maintain_thread_running = true;
            stop_maintain_thread = false;
	    TRY {
            pthread_create(&maintain_task, pthread_attr_default, 
                           (pthread_startroutine_t) network_maintain_liveness, 
                           (pthread_addr_t) NULL);  
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
		    "rpc__dg_maintain_fork_handler", 
		    "pthread_create failure")); 
	    } ENDTRY
        }
        RPC_MAINT_UNLOCK();
        break;
    case RPC_C_POSTFORK_CHILD:  
        maintain_thread_was_running = false;
        maintain_thread_running = false;
        stop_maintain_thread = false;

        /*
         * Clear out the list... we should free resources...
         */ 
        maint_head = NULL;

        RPC_MAINT_UNLOCK();
        break;
    }
}
#endif /* ATFORK_SUPPORTED */
