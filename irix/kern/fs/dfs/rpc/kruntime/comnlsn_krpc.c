/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comnlsn_krpc.c,v 65.10 1998/05/08 18:59:31 lmc Exp $";
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
 * $Log: comnlsn_krpc.c,v $
 * Revision 65.10  1998/05/08 18:59:31  lmc
 * Changed the names of the debugging commands so that all rpc
 * commands begin with rpc.
 *
 * Revision 65.8  1998/04/01  14:16:50  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed unreachable statement.
 *
 * Revision 65.7  1998/03/24  16:21:54  lmc
 * Fixed missing comment characters lost when the ident lines were
 * ifdef'd for the kernel.  Also now calls rpc_icrash_init() for
 * initializing symbols, and the makefile changed to build rpc_icrash.c
 * instead of dfs_icrash.c.
 *
 * Revision 65.6  1998/03/23  17:26:26  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/03/22 02:51:49  lmc
 * These changes make krpc work for at least the simple case.  Threads
 * are now sthreads, but for the select code to work we needed a
 * uthread structure.  This is dummied up in the listener code.
 * Calls are added to add idbg functions for dfs.  The mapping of
 * names to routines can be found here.
 *
 * Revision 65.3  1998/01/07  17:20:48  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:42  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.37.2  1996/02/18  00:00:38  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:45  marty]
 *
 * Revision 1.1.37.1  1995/12/08  00:15:01  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:15  root]
 * 
 * Revision 1.1.35.2  1994/05/19  21:14:14  hopkins
 * 	Serviceability work
 * 	[1994/05/19  02:18:11  hopkins]
 * 
 * 	Serviceability:
 * 	  Change code using RPC_E_DBG_LTHREAD so that
 * 	  rpc_es_* form is used instead of rpc_e_* form,
 * 	  and use RPC_DBG_GPRINTF instead of RPC_DBG_PRINTF
 * 	  so that a valid subcomponent gets passed to the
 * 	  SVC debug macro.
 * 	[1994/05/18  21:33:35  hopkins]
 * 
 * Revision 1.1.35.1  1994/01/21  22:31:58  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:20  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:36:12  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:20  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  19:38:48  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:04  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:16:11  devrcs
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
**      comnlsn_osc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**      Network Listener Service internal operations.
**      This file provides the "less portable" portion of the PRIVATE
**      Network Listener internal interface (see comnet.c) and should
**      ONLY be called by the PRIVATE Network Listener Operations.
**
**      Different implementations may choose to provide an alternate
**      implementation of the listener thread processing by providing
**      a reimplementation of the operations in this file.  The portable
**      Network Listener Services operations (for the public and private
**      API) is in comnet.c
**
**      This particular implementation is "tuned" for:
**          a) the use of a pthread for listener processing
**          b) a rpc_socket_t is a UNIX File Descriptor
**          c) BSD UNIX select()
**
**
*/

#include <commonp.h>
#include <com.h>
#include <comp.h>
#include <comnetp.h>
#ifdef SGIMIPS
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif

/*
 * The lstate_generation structure is used to notify / synchronize
 * the listener thread private state with the listener thread global state.
 */

INTERNAL pthread_once_t comnlsn_init_once_block = pthread_once_init;

INTERNAL struct {
    rpc_mutex_t m;
    unsigned32  cnt;
} lstate_generation; 

INTERNAL pthread_t                  listener_thread;
INTERNAL boolean                    listener_thread_running;

INTERNAL void lthread _DCE_PROTOTYPE_ ((
        rpc_listener_state_p_t lstate
    ));

INTERNAL void update_local_lstate _DCE_PROTOTYPE_((rpc_listener_state_p_t /* lstate*/)) ;

INTERNAL void comnlsn_init_once _DCE_PROTOTYPE_ ((void));

#ifdef SGIMIPS
void idbg_lstate(__psint_t);
void idbg_dfs_cct(__psint_t);
void idbg_dfs_ccall(__psint_t);
void idbg_dfs_ccall(__psint_t);
void idbg_pr_rpc_dg_recvq_elt(__psint_t);
void idbg_dfs_scall(__psint_t);
void idbg_pr_scall_ent(__psint_t); 
void idbg_pr_call_entry(__psint_t);
void idbg_helper(__psint_t);
void idbg_krpc_help(__psint_t);
void idbg_helper_queues(__psint_t);
void rpc_icrash_init(void);
#endif


INTERNAL void comnlsn_init_once(void)
{
    /*
     * Init the listener state generation identifier.
     */

    RPC_MUTEX_INIT(lstate_generation.m);
    lstate_generation.cnt = 0;
}


/*
 * R P C _ _ N L S N _ A C T I V A T E _ D E S C
 *
 * Arrange for the listener to handle packets on the new descriptor.
 * This routine is also responsible for starting up a listener
 * thread if once doesn't already exist.
 */

PRIVATE void rpc__nlsn_activate_desc (
      rpc_listener_state_p_t  lstate,
      unsigned32              idx,
      unsigned32              *status
)
{
#ifdef SGIMIPS
    idbg_addfunc("rpc_lstate", idbg_lstate);
    idbg_addfunc("rpc_ccall", idbg_dfs_ccall);
    idbg_addfunc("rpc_cct", idbg_dfs_cct);
    idbg_addfunc("rpc_rqe", idbg_pr_rpc_dg_recvq_elt);
    idbg_addfunc("rpc_scall", idbg_pr_scall_ent);
    idbg_addfunc("rpc_sct", idbg_dfs_scall);
    idbg_addfunc("rpc_call", idbg_pr_call_entry);
    idbg_addfunc("rpc_helper", idbg_helper);
    idbg_addfunc("rpc_helper_queues", idbg_helper_queues);
    idbg_addfunc("rpc_help", idbg_krpc_help);

    /*  Pull in symbols for icrash */
    rpc_icrash_init();
#endif

    RPC_MUTEX_LOCK_ASSERT (lstate->mutex);

    pthread_once (&comnlsn_init_once_block, comnlsn_init_once);

    lstate->socks[idx].is_active = true;
                                          
    /*
     * Bump the listener state generation number so that the listener
     * will (eventually) detect that something has changed.
     */

    RPC_MUTEX_LOCK(lstate_generation.m);

    lstate_generation.cnt++;

    RPC_MUTEX_UNLOCK(lstate_generation.m);

    /*
     * Start the listener thread.
     */

    if (listener_thread_running == false)
    {
        listener_thread_running = true;
        pthread_create (
            &listener_thread,                   /* new thread    */
            pthread_attr_default,               /* attributes    */
            (pthread_startroutine_t) lthread,   /* start routine */
            (pthread_addr_t) lstate);           /* arguments     */
    }

    *status = rpc_s_ok;
}


/*
 * R P C _ _ N L S N _ D E A C T I V A T E _ D E S C
 *
 * Mark that the specified descriptor is no longer "live" -- i.e., that
 * events on it should NOT be processed.  Note, the socket must continue
 * to be listened on to ensure that subsequent received data is drained.
 */

PRIVATE void rpc__nlsn_deactivate_desc 
( 
    rpc_listener_state_p_t  lstate,
    unsigned32              idx,
    unsigned32              *status
)
{
    RPC_MUTEX_LOCK_ASSERT (lstate->mutex);

    lstate->socks[idx].is_active = false;

    /*
     * If the listener thread is not running, there's nothing more to do.
     */

    if (! listener_thread_running)
    {
        return;
    }

    /*
     * If we're the listener thread, then just update the listener state
     * ourselves.  If we're not, then get the listener thread to pick
     * up the new state (by bumping the generation count).  Note that
     * in the latter case, we synchronize with the listener thread.  The
     * point of this exercise is that when we return from this function,
     * we want to make SURE that the listener thread will NOT try to
     * select() on the FD we're trying to remove.  (Some callers of this
     * function close the FD and some other [perhaps not RPC runtime]
     * thread might call open() or socket() and get that FD.  We need
     * to make sure that the listener thread never uses this reincarnated
     * FD.)
     */

    if (pthread_equal (pthread_self(), listener_thread))
    {
        update_local_lstate(lstate);
    }
    else 
    {
        lstate->reload_pending = true;

        /*
         * Bump the listener state generation number so that the listener
         * will (eventually) detect that something has changed.
         */

        RPC_MUTEX_LOCK(lstate_generation.m);

        lstate_generation.cnt++;

        RPC_MUTEX_UNLOCK(lstate_generation.m);

        /*
         * Wait for the update to take effect.
         */
        while (lstate->reload_pending)
        {
            RPC_COND_WAIT (lstate->cond, lstate->mutex);
        }
    }

    *status = rpc_s_ok;
}

/*
 * Things STRICTLY private to the listener *thread*.
 */

/*
 * A structure that captures the listener's information about a single socket.
 */

typedef struct
{
    rpc_prot_network_epv_p_t    network_epv;
    pointer_t                   priv_info;
    unsigned                    is_active: 1;   /* T => events should NOT be discarded */
} local_sock_t, *local_sock_p_t;

typedef struct {
        unsigned32          generation;
        int                 num_desc;
        /*
         * The following per descriptor information isn't maintained
         * as a single array of structs because we want to use the desc array
         * directly as an arg to select.
         */
        rpc_socket_sel_t    desc[RPC_C_SERVER_MAX_SOCKETS];
        local_sock_t        desc_info[RPC_C_SERVER_MAX_SOCKETS];
	/*  The uthread structure found here exists for the sole purpose of
		allowing us to use select code.  We must have a uthread
		structure for the socket wakeup (poll_wakeup), but since
		this is all it is used for, we can make a dummy structure
		for this and all dfs kernel threads can be sthreads - just
		like xfs, nfs..etc.  */
        uthread_t	    dummy_ut;	
} local_lstate_t, *local_lstate_p_t;

INTERNAL local_lstate_t     local_lstate;
INTERNAL rpc_socket_sel_t   fndsoc[RPC_C_SERVER_MAX_SOCKETS];
INTERNAL struct timeval     sec3_tv = {3, 0};


/*
 * U P D A T E _ L O C A L _ L S T A T E
 *
 * Update the listener thread's private state.
 */

INTERNAL void update_local_lstate( rpc_listener_state_p_t  lstate)
{
    int i, nd;

    RPC_MUTEX_LOCK_ASSERT(lstate->mutex);

    RPC_MUTEX_LOCK(lstate_generation.m);

    if (local_lstate.generation != lstate_generation.cnt)
    {
        nd = 0;
        for (i = 0; i < lstate->high_water; i++)
        {
            rpc_listener_sock_p_t lsock = &lstate->socks[i];

            if (lsock->busy)
            {
                local_lstate.desc[nd].so = lsock->desc;
#if !defined ( _AIX)
                local_lstate.desc[nd].events = POLLNORM; /* currently only do reads */
#else
                local_lstate.desc[nd].events = POLLIN; /* currently only do reads */
#endif
                local_lstate.desc_info[nd].network_epv = lsock->network_epv;
                local_lstate.desc_info[nd].priv_info = lsock->priv_info;
                local_lstate.desc_info[nd].is_active = lsock->is_active;
                nd++;
            }
        }
        local_lstate.num_desc = nd;
        local_lstate.generation = lstate_generation.cnt;

        /*
         * Let everyone who's waiting on our completion of the state update
         * know we're done.
         */
        lstate->reload_pending = false;
        RPC_COND_BROADCAST (lstate->cond, lstate->mutex);
    }

    RPC_MUTEX_UNLOCK(lstate_generation.m);
}

/*
 * L T H R E A D
 *
 * Server listen thread loop.
 */

INTERNAL void lthread ( rpc_listener_state_p_t lstate)
{
    unsigned32          status;
    int                 nd, fs, n_found;
    rpc_socket_error_t  error;
    kthread_t 		*kt;
    uthread_t		*ut;

    /*
     * Ensure we get our initial state.
     */

    local_lstate.generation = -1;
    ut = &local_lstate.dummy_ut;
    kt = UT_TO_KT(ut);
    sv_init(&kt->k_timewait, SV_DEFAULT, "ktwait");
    init_spinlock(&ut->ut_pollrotorlock, "pollrotor", 0);
    
    /*
     * Loop waiting for incoming packets.
     */

    while (true)
    {
        /*
         * Update our local state as required.  We take a peek
         * at the generation count without a lock for performance reasons
         * (eventually we'll notice the change and do things properly).
         */

        if (local_lstate.generation != lstate_generation.cnt)
        {
            RPC_MUTEX_LOCK(lstate->mutex);
            update_local_lstate(lstate);
            RPC_MUTEX_UNLOCK(lstate->mutex);
        }

        /*
         * Wait for packets.  Since there really isn't a way for other
         * threads wanting to add/remove descriptors to notify us
         * (without having to do something with signals or do something
         * special with the in-kernel select), just do a timed
         * select to ensure that we ocassionally check our state.
         * The state doesn't change very often and the responsiveness
         * to such an event isn't too critical, so don't wake up
         * too frequently.
         */

        error = rpc__socket_select(local_lstate.num_desc, local_lstate.desc,
                        &n_found, fndsoc, &sec3_tv, &local_lstate.dummy_ut);

        if (RPC_SOCKET_IS_ERR(error))
        {
            RPC_DBG_GPRINTF(
                ("lthread: select error - %d\n", RPC_SOCKET_ETOI(error)));
            continue;
        }

#define RPC_ES_DBG_LTHREAD   (rpc_es_dbg_last_switch - 1)

        if (RPC_DBG(RPC_ES_DBG_LTHREAD, 5))
        {
            if (RPC_DBG(RPC_ES_DBG_LTHREAD, 7))
            {
                RPC_DBG_GPRINTF(("lthread: select found %d sockets\n", n_found));
            }
            else if (RPC_DBG(RPC_ES_DBG_LTHREAD, 6) && n_found > 0)
            {
                RPC_DBG_GPRINTF(("lthread: select found %d sockets\n", n_found));
            }
            else if (RPC_DBG(RPC_ES_DBG_LTHREAD, 5) && n_found > 1)
            {
                RPC_DBG_GPRINTF(("lthread: select found %d sockets\n", n_found));
            }
        }

        /*
         * Process any descriptors that were active.
         */

        for (fs = 0; fs < n_found; fs++)
        {
            rpc_socket_t    desc = fndsoc[fs].so;

            for (nd = 0; nd < local_lstate.num_desc; nd++)
            {
                if (local_lstate.desc[nd].so == desc)
                {
                    local_sock_t    *di = &local_lstate.desc_info[nd];

                    (di->network_epv->network_select_disp) (
                                desc,
                                di->priv_info,
                                di->is_active,
                                &status
                            );
                    if (status != rpc_s_ok)
                    {
                        RPC_DBG_GPRINTF((
                            "(_lthread) select dispatch failed: desc=%d status=%d\n",
                            desc, status));
                    }
                    break;
                }
            }
        }
    }

#ifndef SGIMIPS
    /* unreachable */
    RPC_DBG_GPRINTF(("(lthread) shutting down...\n"));
#endif
}    
