/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgslive.c,v 65.8 1998/04/01 14:17:01 gwehrman Exp $";
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
 * $Log: dgslive.c,v $
 * Revision 65.8  1998/04/01 14:17:01  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed the volatile qualifier from the definition of rpc_mutex_t.
 *
 * Revision 65.7  1998/03/23  17:30:40  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/03/09 19:31:47  lmc
 * Remove setting of process name because kernel threads are now sthreads
 * instead of uthreads and thus don't have a process name - only a kthread
 * name.
 *
 * Revision 65.5  1998/01/07  17:21:41  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.4  1997/12/16  17:23:23  lmc
 * Removed prototypes that were no longer correct and/or existed in a .h file now.
 *
 * Revision 65.3  1997/11/06  19:57:42  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:58  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.522.2  1996/02/18  00:04:04  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:29  marty]
 *
 * Revision 1.1.522.1  1995/12/08  00:20:18  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/07/14  19:06 UTC  tatsu_s
 * 	Submitted the fix for CHFts15608.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b4/2  1995/07/11  16:20 UTC  tatsu_s
 * 	Added more volatile.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.local_rpc.b4/1  1995/07/06  21:04 UTC  tatsu_s
 * 	Added volatile.
 * 	Fixed the frequency of the moniter thread.
 * 	Don't stop the moniter thread if executing rundown.
 * 
 * 	HP revision /main/HPDCE02/2  1995/05/25  21:41 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:01 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 	[1995/12/07  23:59:17  root]
 * 
 * Revision 1.1.520.2  1994/07/15  16:56:23  tatsu_s
 * 	Fix OT10589: Reversed the execution order of fork_handlers.
 * 	[1994/07/09  02:45:24  tatsu_s]
 * 
 * Revision 1.1.520.1  1994/01/21  22:37:20  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:35  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  23:24:49  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:14  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:49:37  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:59  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/07  19:46:57  markar
 * 	 06-may-92 markar   packet rationing mods
 * 	[1992/05/07  15:35:58  markar]
 * 
 * Revision 1.1  1992/01/19  03:10:25  devrcs
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
**      dgslive.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for monitoring liveness of clients.
**
**
*/

#include <dg.h>
#include <dgsct.h>
#include <dgslive.h>

#include <dce/convc.h>
#include <dce/conv.h>

/* ========================================================================= */

INTERNAL void network_monitor_liveness    _DCE_PROTOTYPE_((void));

/* ========================================================================= */

/*
 * Number of seconds before declaring a monitored client dead.
 */

#define LIVE_TIMEOUT_INTERVAL   120

/*
 * The client table is a hash table with seperate chaining, used by the
 * server runtime to keep track of client processes which it has been
 * asked to monitor. 
 *
 * This table is protected by the global lock.
 */

#define CLIENT_TABLE_SIZE 29    /* must be prime */

INTERNAL rpc_dg_client_rep_p_t client_table[CLIENT_TABLE_SIZE];
         
#define CLIENT_HASH_PROBE(cas_uuid, st) \
    (rpc__dg_uuid_hash(cas_uuid) % CLIENT_TABLE_SIZE)

/*
 * static variables associated with running a client monitoring thread.
 * 
 * All are protected by the monitor_mutex lock.
 */

#ifdef SGIMIPS
/*
 * The only reason to make the mutex volitile would be if the mutex state
 * is changed while in a try block.  In that case the mutex state change
 * is lost in the event of a exception.  But in the event of an exception
 * bad things are going to happen regardless with a mutex whose state
 * has been changed inside the TRY block.  The compiler is much happier
 * if the mutex is not a volitale.  This is important for the kernel
 * builds.
 */
INTERNAL rpc_mutex_t    monitor_mutex;
#else
INTERNAL volatile rpc_mutex_t    monitor_mutex;
#endif
INTERNAL rpc_cond_t     monitor_cond;
INTERNAL pthread_t  monitor_task;
INTERNAL volatile boolean    monitor_running = false;
INTERNAL volatile boolean    monitor_was_running = false;
INTERNAL volatile boolean    stop_monitor = false;
INTERNAL volatile boolean    executing_rundown = false;
INTERNAL volatile unsigned32 active_monitors = 0;

/* ========================================================================= */

/*
 * F I N D _ C L I E N T 
 *
 * Utility routine for looking up a client handle, by UUID, in the
 * global client_rep table.
 */

INTERNAL rpc_dg_client_rep_p_t find_client _DCE_PROTOTYPE_((
        uuid_p_t /*cas_uuid*/
    ));

INTERNAL rpc_dg_client_rep_p_t find_client
#ifdef _DCE_PROTO_
(
    uuid_p_t cas_uuid
)
#else
(cas_uuid)
uuid_p_t cas_uuid;
#endif
{
    rpc_dg_client_rep_p_t client;
    unsigned16 probe;
    unsigned32 st;
                        
    probe = CLIENT_HASH_PROBE(cas_uuid, &st);
    client = client_table[probe];

    while (client != NULL) 
    {
        if (uuid_equal(cas_uuid, &client->cas_uuid, &st))
            return(client);
        client = client->next;
    }
    return(NULL);
}


/*
 * R P C _ _ D G _ N E T W O R K _ M O N
 *
 * This routine is called, via the network listener service, by a server
 * stub which needs to maintain context for a particular client.  The
 * client handle is provided, and in the event that the connection to
 * the client is lost, that handle will be presented to the rundown routine
 * specified. 
 * 
 * The actual client rep structure is created during the call to
 * binding_inq_client and is stored in a global table at that time.  When
 * successful, this routine merely associates a rundown function pointer
 * with the appropriate client rep structure in the table.
 */

PRIVATE void rpc__dg_network_mon
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t binding_r,
    rpc_client_handle_t client_h,
    rpc_network_rundown_fn_t rundown,
    unsigned32 *st
)
#else
(binding_r, client_h, rundown, st)
rpc_binding_rep_p_t binding_r;
rpc_client_handle_t client_h;
rpc_network_rundown_fn_t rundown;
unsigned32 *st;
#endif
{            
    rpc_dg_client_rep_p_t ptr, client = (rpc_dg_client_rep_p_t) client_h;
    unsigned16 probe;
    uuid_p_t cas_uuid = (uuid_p_t) &client->cas_uuid;

    RPC_MUTEX_LOCK(monitor_mutex);
   
    /*
     * Hash into the client rep table based on the handle's UUID.
     * Scan the chain to find the client handle.
     */
                  
    probe = CLIENT_HASH_PROBE(cas_uuid, st);
    ptr = client_table[probe]; 

    while (ptr != NULL)
    {
        if (ptr == client)
            break;
        ptr = ptr->next;
    }

    /*           
     * If the handle passed in is not in the table, it must be bogus.
     * Also, make sure that we are not already monitoring this client,
     * indicated by a non-NULL rundown routine pointer.
     */

    if (ptr == NULL || ptr->rundown != NULL)
    {    
        *st = -1;         /* !!! Need a real error value */
        RPC_MUTEX_UNLOCK(monitor_mutex);
        return;
    }

    /*
     * (Re)initialize the table entry, and bump the count of active monitors.
     */

    client->rundown  = rundown;
    client->last_update = rpc__clock_stamp();
    active_monitors++;

    /*
     * Last, make sure that the monitor timer routine is running. 
     */

    if (! monitor_running)
    {
        monitor_running = true;
	TRY {
        pthread_create(&monitor_task, pthread_attr_default, 
            (pthread_startroutine_t) network_monitor_liveness, 
            (pthread_addr_t) NULL);  
	} CATCH_ALL {
	    monitor_running = false;
	    *st = rpc_s_no_memory;
	    RPC_MUTEX_UNLOCK(monitor_mutex);
	    return;
	} ENDTRY
    }                         

    *st = rpc_s_ok;
    RPC_MUTEX_UNLOCK(monitor_mutex);
}


/*
 * R P C _ _ D G _ N E T W O R K _ S T O P _ M O N
 *
 * This routine is called, via the network listener service, by a server stub
 * when it wishes to discontinue maintaining context for a particular client. 
 * The client will no longer be monitored if the rundown function pointer
 * is set to NULL.  The actual client handle structure is maintained, with
 * reference from the SCTE, to avoid doing another callback if the client 
 * needs to be monitored again.
 */
 
PRIVATE void rpc__dg_network_stop_mon
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t binding_r,
    rpc_client_handle_t client_h,
    unsigned32 *st
)
#else
(binding_r, client_h, st)
rpc_binding_rep_p_t binding_r;
rpc_client_handle_t client_h;
unsigned32 *st;
#endif
{
    rpc_dg_client_rep_p_t client = (rpc_dg_client_rep_p_t) client_h;
    rpc_dg_client_rep_p_t ptr;
    uuid_p_t cas_uuid = &client->cas_uuid;
    unsigned16 probe;
                 
    RPC_MUTEX_LOCK(monitor_mutex);

    /*
     * Hash into the client rep table based on the client handle's UUID.
     */
  
    probe = CLIENT_HASH_PROBE(cas_uuid, st);
    ptr = client_table[probe];
       
    /*
     * Scan down the hash chain, looking for the reference to the client
     * handle
     */
                   
    while (ptr != NULL) {
        if (ptr == client)
        {   
            /*
             * To stop monitoring a client handle requires only that 
             * the rundown function pointer be set to NULL.
             */           
         
            if (client->rundown != NULL)
            {
                client->rundown = NULL;
                active_monitors--;
            }
            RPC_MUTEX_UNLOCK(monitor_mutex);
            *st = rpc_s_ok;
            return;
        }
        ptr = ptr->next;
    }

    *st = -1;               /* !!! attempt to remove unmonitored client */
    RPC_MUTEX_UNLOCK(monitor_mutex);
}


/*
 * N E T W O R K _ M O N I T O R _ L I V E N E S S 
 *
 * This routine runs as the base routine of a thread; it periodically
 * checks for lost client connections.  We can't run this routine from
 * the timer queue (and thread) because it calls out to the application
 * (stub) rundown routines and we can't tie up the timer while we do
 * that.
 */

INTERNAL void network_monitor_liveness(void)
{
    rpc_dg_client_rep_p_t client;
    unsigned32 i;
    struct timespec next_ts;
#ifdef KERNEL
    extern int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
        struct timespec *);

#endif /* KERNEL */


    RPC_DBG_PRINTF(rpc_e_dbg_conv_thread, 1, 
                   ("(network_monitor_liveness) starting up...\n"));

    RPC_MUTEX_LOCK(monitor_mutex);

    while (stop_monitor == false)
    {
        /*
         * Awake every 60 seconds.
         */
        rpc__clock_timespec(rpc__clock_stamp()+RPC_CLOCK_SEC(60), &next_ts);

        RPC_COND_TIMED_WAIT(monitor_cond, monitor_mutex, &next_ts);
        if (stop_monitor == true)
	    goto shutdown;

        for (i = 0; i < CLIENT_TABLE_SIZE; i++)
        {                                     
            client = client_table[i];
    
            while (client != NULL && active_monitors != 0)
            {      
                if (client->rundown != NULL &&
                    rpc__clock_aged(client->last_update, 
                                    RPC_CLOCK_SEC(LIVE_TIMEOUT_INTERVAL)))
                {                 
                    /*
                     * If the timer has expired, call the rundown routine.
                     * Stop monitoring the client handle by setting its rundown
                     * routine pointer to NULL.
                     */
    
                    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                        ("(network_monitor_liveness_timer) Calling rundown function\n"));
                            
		    executing_rundown = true;
                    RPC_MUTEX_UNLOCK(monitor_mutex);
                    (*client->rundown)((rpc_client_handle_t)client);
                    RPC_MUTEX_LOCK(monitor_mutex);
		    executing_rundown = false;

                    /*
                     * The monitor is no longer active.
                     */
                    client->rundown = NULL;
                    active_monitors--;
#ifndef	_KERNEL
		    /*
		     * While we were executing the rundown function and opened
		     * the mutex, the fork handler might try to stop us.
		     */
		    if (stop_monitor == true)
			goto shutdown;
#endif	/* _KERNEL */
                }
                client = client->next;
            }

            if (active_monitors == 0)
            {
#ifndef _KERNEL
                /*
                 * While we were executing the rundown function and opened the
                 * mutex, the fork handler might try to stop us.
                 */
                if (stop_monitor == true)
                    break;
                /*
                 * Nothing left to monitor, so terminate the thread.
                 */
                pthread_detach(&monitor_task);
                monitor_running = false;
                RPC_DBG_PRINTF(rpc_e_dbg_conv_thread, 1, 
                    ("(network_monitor_liveness) shutting down (no active)...\n"));
                RPC_MUTEX_UNLOCK(monitor_mutex);
                return;
#else
                /*
                 * Nothing left to monitor, so just go back to sleep.
                 */
                break;
#endif
            }
        }
    }
shutdown:
    RPC_DBG_PRINTF(rpc_e_dbg_conv_thread, 1, 
                   ("(network_monitor_liveness) shutting down...\n"));

    RPC_MUTEX_UNLOCK(monitor_mutex);
}


/*
 * R P C _ _ D G _ C O N V C _ I N D Y 
 *
 * Server manager routine for monitoring the liveness of clients.
 */

PRIVATE void rpc__dg_convc_indy
#ifdef _DCE_PROTO_
(
    uuid_t *cas_uuid
)
#else
(cas_uuid)
uuid_t *cas_uuid;
#endif
{
    rpc_dg_client_rep_p_t client;
                
    RPC_MUTEX_LOCK(monitor_mutex);

    client = find_client(cas_uuid);

    if (client != NULL)
    {
        client->last_update = rpc__clock_stamp();
    }
    RPC_MUTEX_UNLOCK(monitor_mutex);
}


/*
 * R P C _ _ D G _ B I N D I N G _ I N Q _ C L I E N T
 *
 * Inquire what client address space a binding handle refers to.
 */

PRIVATE void rpc__dg_binding_inq_client
#ifdef _DCE_PROTO_
(
    rpc_binding_rep_p_t binding_r,
    rpc_client_handle_t *client_h,
    unsigned32 *st
)
#else
(binding_r, client_h, st)
rpc_binding_rep_p_t binding_r;
rpc_client_handle_t *client_h;
unsigned32 *st;
#endif
{       
    rpc_dg_binding_server_p_t shand = (rpc_dg_binding_server_p_t) binding_r;
    rpc_dg_scall_p_t scall = shand->scall;
    rpc_binding_handle_t h;
    uuid_t cas_uuid;
    rpc_dg_client_rep_p_t client;
    unsigned32 temp_seq, tst;
                              
    *st = rpc_s_ok;

    /*
     * Lock down and make sure we're in an OK state.
     */

    RPC_LOCK(0);
    RPC_DG_CALL_LOCK(&scall->c);
                      
    if (scall->c.state == rpc_e_dg_cs_orphan)
    {
        *st = rpc_s_call_orphaned;
        RPC_DG_CALL_UNLOCK(&scall->c);
        RPC_UNLOCK(0);
        return;
    }
    
    /*
     * See if there is already a client handle associated with the scte
     * associated with this server binding handle.  If there is, just
     * return it.
     */

    if (scall->scte->client != NULL)
    {
        *client_h = (rpc_client_handle_t) scall->scte->client;
        RPC_DG_CALL_UNLOCK(&scall->c);
        RPC_UNLOCK(0);
        return;
    }

    /*
     * No client handle.  We need to do a call back to obtain a UUID
     * uniquely identifying this particular instance of the client.
     */

    h = rpc__dg_sct_make_way_binding(scall->scte, st);

    RPC_DG_CALL_UNLOCK(&scall->c);
    RPC_UNLOCK(0);

    if (h == NULL)
    {
        return;
    }

    RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
        ("(binding_inq_client) Doing whats-your-proc-id callback\n"));

    TRY
    {
        (*conv_v3_0_c_epv.conv_who_are_you2)
            (h, &scall->c.call_actid, rpc_g_dg_server_boot_time, 
            &temp_seq, &cas_uuid, st);
    }
    CATCH_ALL
    {
        *st = rpc_s_who_are_you_failed;
    }
    ENDTRY

    rpc_binding_free(&h, &tst);

    if (*st != rpc_s_ok)
        return;

    /*
     * Check to see if the UUID returned has already been built into
     * a client handle associated with another scte.  Since we have no
     * way of mapping actids to processes, we can't know that two actid
     * are in the same address space until we get the same address space
     * UUID from both.  In this case it is necessary to use the same
     * client handle for both actids.
     */
             
    RPC_LOCK(0);          
    RPC_DG_CALL_LOCK(&scall->c);

    if (scall->c.state == rpc_e_dg_cs_orphan)
    {
        *st = rpc_s_call_orphaned;
        RPC_DG_CALL_UNLOCK(&scall->c);
        RPC_UNLOCK(0);                                     
        return;
    }
    
    RPC_MUTEX_LOCK(monitor_mutex);

    client = find_client(&cas_uuid);

    if (client != NULL)
    {   
        client->refcnt++;
        scall->scte->client = client;
    }
    else
    {
        /*
         * If not, alloc up a client handle structure and thread
         * it onto the table.
         */

        unsigned16 probe;

        probe = CLIENT_HASH_PROBE(&cas_uuid, st);

        RPC_MEM_ALLOC(client, rpc_dg_client_rep_p_t, sizeof *client, 
            RPC_C_MEM_DG_CLIENT_REP, RPC_C_MEM_NOWAIT);
	if (client == NULL){
	    RPC_MUTEX_UNLOCK(monitor_mutex);
	    RPC_DG_CALL_UNLOCK(&scall->c);
	    RPC_UNLOCK(0);                                     
	    *st = rpc_s_no_memory;
	    return;
	}

        client->next = client_table[probe];
        client->rundown = NULL;
        client->last_update = 0;
        client->cas_uuid = cas_uuid;

        client_table[probe] = client;
        scall->scte->client = client;
        client->refcnt = 2;
    }  

    RPC_MUTEX_UNLOCK(monitor_mutex);
    RPC_DG_CALL_UNLOCK(&scall->c);
    RPC_UNLOCK(0);                                     
    
    *client_h = (rpc_client_handle_t) client; 
}
  

/*
 * R P C _ _ D G _ M O N I T O R _  I N I T
 *
 * This routine performs any initializations required for the network
 * listener service maintain/monitor functions.
 */

PRIVATE void rpc__dg_monitor_init(void)
{               

    /*
     * Initialize the count of handles currently being monitored.
     */

    active_monitors = 0;
    monitor_running = false;
    monitor_was_running = false;
    stop_monitor = false;
    executing_rundown = false;
    RPC_MUTEX_INIT(monitor_mutex);
    RPC_COND_INIT(monitor_cond, monitor_mutex);
}

#ifdef ATFORK_SUPPORTED
/*
 * R P C _ _ D G _ M O N I T O R _ F O R K 
 *
 * Handle fork related processing for this module.
 */

PRIVATE void rpc__dg_monitor_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{                           
    unsigned32 i;
    unsigned32 st;

    switch ((int)stage)
    {
    case RPC_C_PREFORK:
        RPC_MUTEX_LOCK(monitor_mutex);
        monitor_was_running = false;

	/*
	 * If the monitor timer routine is executing the rundown function, we
	 * can't safely stop it because of the possible mutex deadlock. So, we
	 * let it run across the fork, hoping no bad side effect...
	 * Since we have the monitor_mutex locked, it can't modify our state.
	 */
	if (monitor_running && !executing_rundown)
        {
            stop_monitor = true;
            RPC_COND_SIGNAL(monitor_cond, monitor_mutex);
            RPC_MUTEX_UNLOCK(monitor_mutex);
            pthread_join (monitor_task, (pthread_addr_t *) &st);
            RPC_MUTEX_LOCK(monitor_mutex);
            pthread_detach(&monitor_task);
            monitor_running = false;
            /*
             * The monitor thread may have nothing to do.
             */
            if (active_monitors != 0)
                monitor_was_running = true;
            stop_monitor = false;
        }
        break;
    case RPC_C_POSTFORK_PARENT:
        if (monitor_was_running) 
        {
            monitor_was_running = false;
            monitor_running = true;
            stop_monitor = false;
	    executing_rundown = false;
	    TRY {
            pthread_create(&monitor_task, pthread_attr_default, 
                           (pthread_startroutine_t) network_monitor_liveness, 
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
		    "rpc__dg_monitor_fork", 
		    "pthread_create failure")); 
	    } ENDTRY
        }
        RPC_MUTEX_UNLOCK(monitor_mutex);
        break;
    case RPC_C_POSTFORK_CHILD:  
        monitor_was_running = false;
        monitor_running = false;
        stop_monitor = false;
	executing_rundown = false;

        /*
         * Initialize the count of handles currently being monitored.
         */
        
        active_monitors = 0;
        for (i = 0; i < CLIENT_TABLE_SIZE; i++)
            client_table[i] = NULL;

        RPC_MUTEX_UNLOCK(monitor_mutex);
        break;
    }
}
#endif /* ATFORK_SUPPORTED */
  
/*
 * R P C _ _ D G _ C L I E N T _ F R E E
 *
 * This routine frees the memory associated with a client handle (created 
 * for the purpose of monitoring client liveness).  It is called by the 
 * the RPC_DG_CLIENT_RELEASE macro when the last scte which refers to this
 * client handle is freed.  The client handle is also removed from the 
 * client table.
 */

PRIVATE void rpc__dg_client_free
#ifdef _DCE_PROTO_
(
    rpc_client_handle_t client_h
)
#else
(client_h)
rpc_client_handle_t client_h;
#endif
{
    unsigned16 probe;
    rpc_dg_client_rep_p_t client = (rpc_dg_client_rep_p_t) client_h;
    rpc_dg_client_rep_p_t ptr, prev = NULL;

    RPC_MUTEX_LOCK(monitor_mutex);
             
    /*
     * Hash into the client rep table based on the client handle's UUID.
     */
  
    probe = CLIENT_HASH_PROBE(&client->cas_uuid, &st);
    ptr = client_table[probe];
       
    /*
     * Scan down the hash chain, looking for the reference to the client
     * handle
     */
                   
    while (ptr != NULL) 
    {
        if (ptr == client)
        {   
            if (prev == NULL)
                client_table[probe] = ptr->next;
            else
                prev->next = ptr->next;

            RPC_MEM_FREE(client, RPC_C_MEM_DG_CLIENT_REP);

            RPC_DBG_PRINTF(rpc_e_dbg_general, 3, 
                ("(client_free) Freeing client handle\n"));
    
            RPC_MUTEX_UNLOCK(monitor_mutex);
            return;
        }          

        prev = ptr;
        ptr = ptr->next;
    }
    RPC_MUTEX_UNLOCK(monitor_mutex);
}
