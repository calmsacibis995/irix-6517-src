/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: cominit.c,v 65.7 1998/03/23 17:29:26 gwehrman Exp $";
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
 * $Log: cominit.c,v $
 * Revision 65.7  1998/03/23 17:29:26  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.6  1998/02/26 19:04:56  lmc
 * Changes for sockets using behaviors.  Prototype and cast changes.
 *
 * Revision 65.4  1998/01/07  17:21:23  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:25  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:47  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.99.1  1996/05/10  13:10:03  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:13 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:41 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	merge exit handling fix
 * 	[1995/10/20  21:55 UTC  lmm  /main/HPDCE02/12]
 *
 * 	temporarily disable DG exit handling for C++
 * 	[1995/10/20  21:44 UTC  lmm  /main/HPDCE02/lmm_disable_atexit/1]
 *
 * 	Submitted the fix for CHFts15608.
 * 	[1995/07/14  19:06 UTC  tatsu_s  /main/HPDCE02/11]
 *
 * 	Do nothing in the exit handler, if RPC runtime is not initialized.
 * 	[1995/07/06  20:58 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b4/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/10]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:00 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:14 UTC  lmm  /main/HPDCE02/9]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:18 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted the local rpc security bypass.
 * 	[1995/03/06  17:19 UTC  tatsu_s  /main/HPDCE02/8]
 *
 * 	Fixed KRPC.
 * 	[1995/03/01  19:33 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/2]
 *
 * 	Added the backdoor to disable the single-thread client.
 * 	[1995/02/16  19:51 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b1/1]
 *
 * 	Submitted CHFtsthe fix for CHFts14267 and misc. itimer fixes.
 * 	[1995/02/07  20:56 UTC  tatsu_s  /main/HPDCE02/7]
 *
 * 	Disabled itimer in atfork & atexit handlers.
 * 	[1995/02/02  19:40 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b1/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/6]
 *
 * 	Added rpc__local_fork_handler() and rpc__exit_handler().
 * 	[1995/01/26  20:36 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Merging Local RPC mods
 * 	[1995/01/16  19:14 UTC  markar  /main/HPDCE02/5]
 *
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/01/05  18:31 UTC  markar  /main/HPDCE02/markar_local_rpc/1]
 *
 * 	Fixed RPC_PREFERRED_PROTSEQ=<invalid protseq>.
 * 	[1994/12/12  21:42 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:17 UTC  tatsu_s  /main/HPDCE02/3]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/4]
 *
 * 	Fixed the initialization of rpc_g_preferred_protseq_id.
 * 	[1994/12/07  14:33 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/3]
 *
 * 	RPC_PREFERRED_PROTSEQ: Initial version.
 * 	[1994/12/05  18:53 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:25 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/1]
 *
 * 	Merged from Hpdce02_01
 * 	[1994/08/17  20:41 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Bug 11358 - Translate status to message on call_failed errors.
 * 	[1994/08/05  19:06:48  tom]
 * 	 *
 *
 * 	Merged mothra up to dce 1.1.
 * 	[1994/08/03  16:35 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	Fixed pointer bug in 1.1.9.2.
 * 	[1993/11/10  18:52:27  markar]
 * 	 *
 * 	Added support for RPC_SUPPORTED_NETADDRS environment variable
 * 	[1993/11/09  18:56:57  markar]
 * 	 *
 *
 * 	Loading drop DCE1_0_3b931005
 * 	[1993/10/05  17:59:13  tatsu_s  1.1.8.5]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  18:59 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:30 UTC  jrr  /main/HPDCE02/gaz_dce_instr/jrr_1.2_mothra/1]
 *
 * 	rearrange placement of DMS init handler and fork-handlers
 * 	[1995/08/24  20:28 UTC  gaz  /main/HPDCE02/gaz_dce_instr/5]
 *
 * 	Ensure that DMS is NOT built in the Kernel RPC
 * 	[1995/08/17  04:19 UTC  gaz  /main/HPDCE02/gaz_dce_instr/4]
 *
 * 	Merge out RPC atfork() fix
 * 	[1995/07/17  20:41 UTC  gaz  /main/HPDCE02/gaz_dce_instr/3]
 *
 * 	Submitted the fix for CHFts15608.
 * 	[1995/07/14  19:06 UTC  tatsu_s  /main/HPDCE02/11]
 *
 * 	Do nothing in the exit handler, if RPC runtime is not initialized.
 * 	[1995/07/06  20:58 UTC  tatsu_s  /main/HPDCE02/tatsu_s.local_rpc.b4/1]
 *
 * 	Merge out memory allocation failure detection logic
 * 	[1995/06/16  15:04 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/10]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25
 *
 * Revision 1.1.95.5  1994/08/05  19:49:48  tom
 * 	Bug 11358 - Translate status to message on call_failed errors.
 * 	[1994/08/05  19:06:48  tom]
 * 
 * Revision 1.1.95.4  1994/07/15  16:56:11  tatsu_s
 * 	Fix OT10589: Reversed the execution order of fork_handlers.
 * 	[1994/07/09  02:42:20  tatsu_s]
 * 
 * Revision 1.1.95.3  1994/06/21  21:53:48  hopkins
 * 	More serviceability
 * 	[1994/06/08  21:30:16  hopkins]
 * 
 * Revision 1.1.95.2  1994/05/19  21:14:33  hopkins
 * 	More serviceability.
 * 	[1994/05/18  21:25:01  hopkins]
 * 
 * 	Merge with DCE1_1.
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:18:56  hopkins]
 * 
 * Revision 1.1.95.1  1994/01/21  22:35:23  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:09  cbrooks]
 * 
 * Revision 1.1.93.2  1993/09/28  21:58:06  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  19:36:53  weisman]
 * 
 * Revision 1.1.93.1  1993/09/15  15:39:46  damon
 * 	Synched with ODE 2.1 based build
 * 	[1993/09/15  15:32:00  damon]
 * 
 * Revision 1.1.10.2  1993/09/14  16:48:35  tatsu_s
 * 	Bug 8103 - Moved call to rpc__nlsn_fork_handler() into
 * 	comnet.c::rpc__network_fork_handler().
 * 	Added support for rpc_e_dbg_inherit.
 * 	[1993/09/13  17:08:25  tatsu_s]
 * 
 * Revision 1.1.4.8  1993/01/08  21:47:05  weisman
 * 	Backed down to 1.1.4.3 and included new OSF copyright.
 * 	[1993/01/07  21:32:30  weisman]
 * 
 * Revision 1.1.4.3  1992/10/14  19:50:16  weisman
 * 		Added ifndef _KERNEL around pthread_attr_setstacksize.
 * 	[1992/10/14  19:49:49  weisman]
 * 
 * Revision 1.1.4.2  1992/10/13  20:54:30  weisman
 * 	 21-aug-92 wh            Initialize rpc_g_default_pthread_attr.
 * 	[1992/10/13  20:47:10  weisman]
 * 
 * Revision 1.1.2.6  1992/05/21  13:03:44  mishkin
 * 	Put getenv() stuff under #ifdef DEBUG
 * 	[1992/05/20  19:28:35  mishkin]
 * 
 * 	Merge with (really discard) previous getenv() hacking for protseqs.
 * 	[1992/05/18  18:47:25  mishkin]
 * 
 * 	- Add support for RPC_SUPPORT_PROTSEQS.
 * 	- Misc reorg and cleanup.
 * 	[1992/05/18  18:17:44  mishkin]
 * 
 * Revision 1.1.2.5  1992/05/18  14:02:44  rsalz
 * 	Put getenv inside NO_GETENV test.
 * 	[1992/05/18  14:02:23  rsalz]
 * 
 * Revision 1.1.2.4  1992/05/17  17:51:36  rsalz
 * 	Fix declaration errors in DaveW's getenv hack; also use atoi, not atol.
 * 	[1992/05/17  17:51:15  rsalz]
 * 
 * Revision 1.1.2.3  1992/05/15  23:35:12  weisman
 * 	Added support for environment var RPC_PROTSEQ.
 * 	[1992/05/15  23:34:52  weisman]
 * 
 * Revision 1.1.2.2  1992/05/01  17:02:04  rsalz
 * 	 30-mar-92 af            assert(sizeof(unsigned long)==sizeof(unsigned8))
 * 	                         in the function init_once().
 * 	 23-mar-92 wh            Put #ifdef RRPC around prototype of rrpc_init.
 * 	  5-mar-92 wh            DCE 1.0.1 merge.
 * 	 27-jan-92 ko            No longer die when a NAF init routine
 * 	                         returns a non-ok status. Just mark it as
 * 	                         unsupported.
 * 	[1992/05/01  16:49:43  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:09:46  devrcs
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
**      cominit.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Initialization Service routines.
**
**
*/

#include <commonp.h>    /* Common internals for RPC Runtime System      */
#include <com.h>        /* Externals for Common Services component      */
#include <comp.h>       /* Internals for Common Services component      */
#include <comcthd.h>    /* Externals for call thread services component */
#include <cominit.h>    /* Externals for Initialization sub-component   */
#include <cominitp.h>   /*                                              */
#include <comnaf.h>     /* Internals for NAF Extension services         */
#include <mgmtp.h>      /* Private management services                  */


#define SUPPORTED true
#define UNSUPPORTED false

/*
 * 64K bytes should be sufficient as that is what DCE Security
 * uses for its own threads.
 */
#define DEFAULT_STACK_SIZE 64000


#ifdef RRPC
PRIVATE unsigned32 rpc__rrpc_init _DCE_PROTOTYPE_ ((void));
#endif

INTERNAL boolean supported_naf _DCE_PROTOTYPE_ ((
        rpc_naf_id_elt_p_t               /*naf*/
    ));
    
INTERNAL boolean supported_interface _DCE_PROTOTYPE_ ((
        rpc_naf_id_t                    /*naf*/,
        rpc_network_if_id_t             /*network_if*/,
        rpc_network_protocol_id_t        /*network_protocol*/
    ));

INTERNAL boolean protocol_is_compatible _DCE_PROTOTYPE_ ((
        rpc_protocol_id_elt_p_t          /*rpc_protocol*/
    ));

INTERNAL void init_once _DCE_PROTOTYPE_ ((void));

INTERNAL void thread_context_destructor _DCE_PROTOTYPE_ ((
        rpc_thread_context_p_t   /*ctx_value*/
    ));

extern rpc_socket_error_t rpc_route_socket_init (void);

/*
 * The structure that defines the one-time initialization code. This
 * structure will be associated with init_once.
 */
INTERNAL pthread_once_t init_once_block = pthread_once_init;

/*
 * The value that indicates whether or not the RPC runtime is currently
 * being initialized.
 */
INTERNAL boolean        init_in_progress = false;

/*
 * The id of the thread that is executing (has executed) the RPC runtime 
 * initialization code.
 */
GLOBAL   pthread_t      init_thread;


/*
 * Don't do the getenv() stuff if not built with DEBUG #define'd
 */

#if !defined(DEBUG) && !defined(NO_GETENV)
#  define NO_GETENV
#endif

#ifndef NO_GETENV

INTERNAL void init_getenv_protseqs _DCE_PROTOTYPE_ ((void));

INTERNAL void init_getenv_debug _DCE_PROTOTYPE_ ((void));

INTERNAL void init_getenv_port_restriction _DCE_PROTOTYPE_ ((void));


#endif


/*
**++
**
**  ROUTINE NAME:       rpc__init
**
**  SCOPE:              PRIVATE - declared in cominit.h
**
**  DESCRIPTION:
**      
**  rpc__init() is called to intialize the runtime.  It can safely be
**  called at any time, though it is typically called only be
**  RPC_VERIFY_INIT() in order to minimize overhead.  Upon return from
**  this routine, the runtime will be initialized.
**
**  Prevent rpc__init() (actually pthread_once() of init_once()) recursive
**  call deadlocks by the thread that is actually performing the
**  initialization (see init_once()).
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE void rpc__init(void)

{
    pthread_t       current_thread;
    

    if (init_in_progress)
    {
        current_thread = pthread_self();
        
        if (pthread_equal(init_thread, current_thread))
        {
            /*
             * We're the thread in the middle of initialization (init_once).
             * Assume it knows what it's doing and prevent a deadlock.
             */
            return;
        }
    }

    pthread_once (&init_once_block, (pthread_initroutine_t) init_once);
}



/*
**++
**
**  ROUTINE NAME:       init_once
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  init_once() is invoked only once, by either the RPC runtime or,
**  (in the case of shared images), the host operating system. It performs
**  the basic initialization functions for the all of the RPC Runtime
**  Services (by calling the initialization routines in other components).
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:
**
**  The random number generator is seeded with a psuedo-random value (the
**  system time).
**
**--
**/

INTERNAL void init_once(void)

{
    rpc_naf_id_elt_p_t      naf;
    rpc_protocol_id_elt_p_t rpc_protocol;
    rpc_protseq_id_elt_p_t  rpc_protseq;
    rpc_authn_protocol_id_elt_p_t auth_protocol;
    unsigned32              ctr;
    unsigned32              status;


    /*
     *  Assert that the size of unsigned long is the same as the size of
     *  a pointer.  This assumption may not be true for all machines.
     *
     *  If you are getting this assertion, you will need to change the
     *  macros RPC_CN_ALIGN_PTR(ptr, boundary) in cnp.h and
     *  RPC_DG_ALIGN_8(p) in dg.h.  The pointer should be cast to
     *  the correct scalar data type inorder to make the bitwise-AND
     *  work.  The bitwise operators do not allow pointers for
     *  operands.  The following assert will also have to be changed
     *  to use the new scalar data type.
     */
     assert(sizeof(unsigned long) == sizeof(unsigned8 *));

#ifndef _KERNEL
    if (getenv ("RPC_DISABLE_SINGLE_THREAD") == NULL)
	rpc_g_is_single_threaded = RPC_IS_SINGLE_THREADED(0);
    else
	rpc_g_is_single_threaded = false;
#else
    rpc_g_is_single_threaded = RPC_IS_SINGLE_THREADED(0);
#endif	/* _KERNEL */

#ifdef APOLLO_GLOBAL_LIBRARY
    apollo_global_lib_init();
#endif

    /*
     * Initialize the performance logging service.
     */
    RPC_LOG_INITIALIZE;

#ifdef DCE_RPC_SVC
/*
 * Remove this ifdef when SVC is implemented
 * in the kernel.
 */
# if !defined(KERNEL) && !defined(_KERNEL)
    rpc__svc_init();
# endif
#endif
#ifndef NO_GETENV
    init_getenv_debug ();
#endif

    /*
     * These first two operations (and their order) are critical to
     * creating a deadlock safe environment for the initialization
     * processing.  This code (in conjunction with rpc__init()) allows
     * a thread that is executing init_once() to call other runtime
     * operations that would normally want to ensure that the runtime
     * is initialized prior to executing (which would typically recursively
     * call pthread_once for this block and deadlock).
     * 
     * While this capability now allows the initializing thread to not
     * deadlock, it also allows it to *potentially* attempt some operation
     * that require initialization that hasn't yet been performed.
     * Unfortunately, this can't be avoided; be warned, be careful, don't
     * do anything crazy and all will be fine.
     *
     * One example of where this type of processing is occuring is in
     * the nca_dg initialization processing - the dg init code calls
     * rpc_server_register_if.
     */
    init_thread = pthread_self();
    init_in_progress = true;

    /*
     * Register our fork handler, if such a service is supported.
     */
#ifdef ATFORK_SUPPORTED
    ATFORK((void *)rpc__fork_handler);
#endif

    /*
     * Initialize the global mutex variable.
     */
    RPC_LOCK_INIT (0);

    /*
     * Initialize the global lookaside list mutex.
     */
    RPC_LIST_MUTEX_INIT (0);

    /*
     * Initialize the global binding handle condition variable.
     */
    RPC_BINDING_COND_INIT (0);

    /*
     * create the per-thread context key
     */
    pthread_keycreate (&rpc_g_thread_context_key, 
            (void (*) _DCE_PROTOTYPE_((pointer_t))) thread_context_destructor);

    /*
     * Initialize the timer service.
     */
    if (rpc__timer_init() == rpc_s_no_memory){
	RPC_DCE_SVC_PRINTF (( 
	    DCE_SVC(RPC__SVC_HANDLE, "%s"), 
	    rpc_svc_threads, 
	    svc_c_sev_fatal | svc_c_action_abort, 
	    rpc_m_call_failed, 
	    "init_once",
	    "rpc__timer_init failure"));
    }

    /*
     * Initialize the interface service.
     */
    rpc__if_init (&status);    
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__if_init",
	    error_text ));
    }

    /*
     * Initialize the object service.
     */
    rpc__obj_init (&status);    
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__obj_init",
	    error_text ));
    }

    /*
     * Initialize the cthread service (this doesn't do too much
     * so it doesn't matter if this process never becomes a server).
     */
    rpc__cthread_init (&status);    
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__cthread_init",
	    error_text ));
    }

    /*
     * go through the NAF id table and check each Network Address Family
     * to see if it is supported on this system
     */
    for (ctr = 0; ctr < RPC_C_NAF_ID_MAX; ctr++)
    {
        naf = (rpc_naf_id_elt_p_t) &(rpc_g_naf_id[ctr]);

        if (supported_naf (naf))
        {
            /*
             * if there is no pointer to the init routine for this NAF
             * it must be a shared image, so load it
             */
            if (naf->naf_init == NULL)
            {
                naf->naf_init = rpc__load_naf (naf, &status);
            }

            /*
             * check it again - shouldn't be NULL now, but if it is
             * we'll leave it that way (unsupported)
             */
            if (naf->naf_init)
            {
                (*naf->naf_init) (&(naf->epv), &status);
                if (status != rpc_s_ok)
                {
                    /*
                     * If the NAF couldn't be intialized make it unsupported.
                     */
                    naf->naf_init = NULL;
                }
            }
        }
        else
        {
            /*
             * network family not supported
             */
            naf->naf_init = NULL;
        }
    }
    
    /*
     * go through the RPC protocol id table and check each protocol to
     * see if it is supported on this system
     */
    for (ctr = 0; ctr < RPC_C_PROTOCOL_ID_MAX; ctr++)
    {
        if (protocol_is_compatible (rpc_protocol =
            (rpc_protocol_id_elt_p_t) &(rpc_g_protocol_id[ctr])))
        {
            /*
             * if there is no pointer to the init routine for this protocol
             * it must be a shared image, so load it
             */
            if ((rpc_protocol->prot_init) == NULL)
            {
                rpc_protocol->prot_init =
                    rpc__load_prot (rpc_protocol, &status);
            }

            /*
             * check it again - shouldn't be NULL now, but if it is
             * we'll leave it that way (unsupported)
             */
            if (rpc_protocol->prot_init)
            {
                (*rpc_protocol->prot_init)
                    (&(rpc_protocol->call_epv), &(rpc_protocol->mgmt_epv),
                    &(rpc_protocol->binding_epv), &(rpc_protocol->network_epv),
                    &(rpc_protocol->prot_fork_handler), &status);
                if (status != rpc_s_ok)
                {
                    dce_error_string_t error_text;
                    int temp_status;

                    dce_error_inq_text(status, error_text, &temp_status);

		    /*
		     * rpc_m_call_failed
		     * "%s failed: %s"
		     */
		    RPC_DCE_SVC_PRINTF ((
		        DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
		        rpc_svc_general,
		        svc_c_sev_fatal | svc_c_action_abort,
		        rpc_m_call_failed,
		        "prot_init",
		        error_text ));
                }
            }
        }
        else
        {
            /*
             * protocol family not supported
             */
            rpc_protocol->prot_init = NULL;
        }
    }
    
    /*
     * go through the RPC protocol sequence table and check each protocol
     * sequence to see if it is supported on this system
     */
    for (ctr = 0; ctr < RPC_C_PROTSEQ_ID_MAX; ctr++)
    {
        rpc_protseq = (rpc_protseq_id_elt_p_t) &(rpc_g_protseq_id[ctr]);

        rpc_protocol = (rpc_protocol_id_elt_p_t)
            &(rpc_g_protocol_id[RPC_PROTSEQ_INQ_PROT_ID(ctr)]);

        naf = (rpc_naf_id_elt_p_t)
            &(rpc_g_naf_id[RPC_PROTSEQ_INQ_NAF_ID(ctr)]);

        if (rpc_protocol->prot_init != NULL 
            && naf->naf_init != NULL
            && supported_interface (rpc_protseq->naf_id,
                rpc_protseq->network_if_id, rpc_protseq->network_protocol_id))
        {
            rpc_protseq->supported = SUPPORTED;
        }
        else
        {
            rpc_protseq->supported = UNSUPPORTED;
        }
    }

#ifndef NO_GETENV

    /*
     * See if a list of protocol sequences to use was specified in the
     * RPC_SUPPORTED_PROTSEQS enviroment variable and if one was, process
     * them appropriately.  Note well that this call must follow the above
     * loop.  (See comments in init_getenv_protseqs.)
     */

    init_getenv_protseqs ();

#endif                                  /* NO_GETENV */

    /*
     * Initialize the auth info cache.
     */
    rpc__auth_info_cache_init (&status);    
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__auth_info_cache_init",
	    error_text ));
    }

    /*
     * go through the authentication protocol id table and check each protocol to
     * see if it is supported on this system
     */
    for (ctr = 0; ctr < RPC_C_AUTHN_PROTOCOL_ID_MAX; ctr++)
    {
        auth_protocol = &rpc_g_authn_protocol_id[ctr];

        /*
         * if there is no pointer to the init routine for this protocol
         * it must be a shared image, so load it
         */
        if ((auth_protocol->auth_init) == NULL)
        {
            auth_protocol->auth_init =
                rpc__load_auth (auth_protocol, &status);
        }

        /*
         * check it again - shouldn't be NULL now, but if it is
         * we'll leave it that way (unsupported)
         */
        if (auth_protocol->auth_init)
        {
            (*auth_protocol->auth_init)
                (&(auth_protocol->epv),
                (&(auth_protocol->rpc_prot_epv_tbl)),
                &status);
            if (status != rpc_s_ok) return;
        }
    }

    /*
     * make calls to initialize other parts of the RPC runtime
     */
    
#ifndef PTHREAD_EXC
    if (pthread_attr_create(&rpc_g_server_pthread_attr) == -1)
    {
#if defined(SGIMIPS) && defined(KERNEL)
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed" (no errno)
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_errno,
	    "pthread_attr_create"));
#else
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_errno,
	    "pthread_attr_create",
	    errno ));
#endif
    }
#else
    TRY 
        pthread_attr_create(&rpc_g_server_pthread_attr);
    CATCH_ALL
	/*
	 * rpc_m_call_failed_no_status
	 * "%s failed"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_no_status,
	    "pthread_attr_create" ));
    ENDTRY
#endif


#ifndef _KERNEL

    /*
     * We explicitly set the stack size for the threads that
     * the runtime uses internally.  This size needs to accomodate
     * deeply nested calls to support authenticated RPC.
     */
#ifndef PTHREAD_EXC
    if (pthread_attr_create(&rpc_g_default_pthread_attr) == -1)
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_errno,
	    "pthread_attr_create",
	    errno ));
    }
    if (pthread_attr_setstacksize(&rpc_g_default_pthread_attr,
                                  DEFAULT_STACK_SIZE) == -1) 
    {
	/*
	 * rpc_m_call_failed_errno
	 * "%s failed, errno = %d"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_errno,
	    "pthread_attr_setstacksize",
	    errno ));
    }

#else  /* PTHREAD_EXC */
    TRY 
        pthread_attr_create(&rpc_g_default_pthread_attr);
    CATCH_ALL
	/*
	 * rpc_m_call_failed_no_status
	 * "%s failed"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_no_status,
	    "pthread_attr_create" ));
    ENDTRY

    TRY 
        pthread_attr_setstacksize(&rpc_g_default_pthread_attr,
                                  DEFAULT_STACK_SIZE);
    CATCH_ALL
	/*
	 * rpc_m_call_failed_no_status
	 * "%s failed"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%d"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed_no_status,
	    "pthread_attr_setstacksize" ));
    ENDTRY
#endif    /* not PTHREAD_EXC */

#endif /* not _KERNEL */

    rpc__network_init (&status);    
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__network_init",
	    error_text ));
    }

    status = rpc__mgmt_init();
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__mgmt_init",
	    error_text ));
    }

#ifdef RRPC
    status = rpc__rrpc_init();
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_fatal | svc_c_action_abort,
	    rpc_m_call_failed,
	    "rpc__rrpc_init",
	    error_text ));
    }
#endif

    /*
     * initialize (seed) the random number generator using the current
     * system time
     */
    RPC_RANDOM_INIT(time ((int) NULL));

#ifndef NO_GETENV

    /* 
     * See if there are any protocol sequences which should only bind to 
     * certain ranges of network endpoints.
     */

    init_getenv_port_restriction ();

#endif                                  /* ! NO_GETENV */

#if defined(SGIMIPS) && defined(_KERNEL)
    rpc_route_socket_init();
#endif

    init_in_progress = false;
    rpc_g_initialized = true;
}    

/*
**++
**
**  ROUTINE NAME:       supported_naf
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Called by rpc__init to determine whether a specified Network Family 
**  is supported on the local host operating system.  It makes this
**  determination by trying to create a "socket" under the specified
**  network family.
**
**  INPUTS:
**
**      naf             The network family whose existence is to be tested.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      Returns true if the network family is supported.
**      Otherwise returns false.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean supported_naf 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_elt_p_t      naf
)
#else
(naf)
rpc_naf_id_elt_p_t      naf;
#endif
{
    rpc_socket_t            socket;
    rpc_socket_error_t      socket_error;
    
    if (naf->naf_id == 0)
        return (false);
    
    socket_error = rpc__socket_open_basic
        (naf->naf_id, naf->network_if_id, 0, &socket);

    if (! RPC_SOCKET_IS_ERR (socket_error))
    {
        rpc__socket_close (socket);
        return (true);
    }

    return (false);
}

/*
**++
**
**  ROUTINE NAME:       supported_interface
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Called by rpc__init to determine whether the Network Protocol ID/
**  Network Interface Type ID combination will work on the local host 
**  operating system (for the specified Network Family).
**
**  INPUTS:
**
**      naf             The network family to be tested.
**
**      network_if      The network interface to be tested.
**
**      network_protocol The network protocol to be tested.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      Returns true if the combination is valid.
**      Otherwise returns false.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean supported_interface 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_t            naf,
    rpc_network_if_id_t     network_if,
    rpc_network_protocol_id_t network_protocol
)
#else
(naf, network_if, network_protocol)
rpc_naf_id_t            naf;
rpc_network_if_id_t     network_if;
rpc_network_protocol_id_t network_protocol;
#endif
{
    rpc_socket_t            socket;
    rpc_socket_error_t      socket_error;

    socket_error = rpc__socket_open_basic
        (naf, network_if, network_protocol, &socket);

    if (! RPC_SOCKET_IS_ERR (socket_error))
    {
        (void) rpc__socket_close (socket);
        return (true);
    }

    return (false);
}

/*
**++
**
**  ROUTINE NAME:       protocol_is_compatible
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Called by rpc__init to determine whether the specified RPC protocol
**  is compatible with at least one of the Network Families previously
**  found to be supported under the local operating system.
**
**  INPUTS:
**
**      rpc_protocol        The RPC protocol to be tested.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      Returns true if the protocol is compatible.
**      Otherwise returns false.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean protocol_is_compatible 
#ifdef _DCE_PROTO_
(
    rpc_protocol_id_elt_p_t rpc_protocol
)
#else
(rpc_protocol)
rpc_protocol_id_elt_p_t rpc_protocol;
#endif
{
    unsigned32              i;
    rpc_protseq_id_elt_p_t  rpc_protseq;
    
    
    for (i = 0; i < RPC_C_PROTSEQ_ID_MAX; i++)
    {
        rpc_protseq = (rpc_protseq_id_elt_p_t) &(rpc_g_protseq_id[i]);

        if ((rpc_protseq->rpc_protocol_id == rpc_protocol->rpc_protocol_id) &&
            (RPC_NAF_INQ_SUPPORTED (RPC_PROTSEQ_INQ_NAF_ID (i))))
        {
            return (true);
        }
    }

    return (false);
}


/*
**++
**
**  ROUTINE NAME:       thread_context_destructor
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Called by the threads mechanism when a context has been
**  associated with a particular key. For RPC this routine will be
**  used to free the context associated with the
**  rpc_g_thread_context_key key. The context being freed is a
**  rpc_thread_context_t structure.
**
**  INPUTS:
**
**      ctx_value       pointer to an rpc_thread_context_t
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void thread_context_destructor 
#ifdef _DCE_PROTO_
(
    rpc_thread_context_p_t      ctx_value
)
#else
(ctx_value)
rpc_thread_context_p_t      ctx_value;
#endif
{
    RPC_MEM_FREE (ctx_value, RPC_C_MEM_THREAD_CONTEXT);
}

#ifndef NO_GETENV

/*
**++
**
**  ROUTINE NAME:       init_getenv_debug
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Read the RPC_DEBUG enviroment variable to set some debug switches.
**  
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       
**
**      Sets RPC debugging switches.
**
**--
**/

INTERNAL void init_getenv_debug (void)
{
    unsigned32  status;
    char        *env_name;

    env_name = (char *) getenv ("RPC_DEBUG");
    if (env_name == NULL)
    {
        return;
    }

    rpc__dbg_set_switches (env_name, &status);
    if (status != rpc_s_ok)
    {
        dce_error_string_t error_text;
        int temp_status;

        dce_error_inq_text(status, error_text, &temp_status);

	/*
	 * rpc_m_call_failed
	 * "%s failed: %s"
	 */
	RPC_DCE_SVC_PRINTF ((
	    DCE_SVC(RPC__SVC_HANDLE, "%s%x"),
	    rpc_svc_general,
	    svc_c_sev_error,
	    rpc_m_call_failed,
	    "init_getenv_debug",
	    error_text ));
    }
}


/*
**++
**
**  ROUTINE NAME:       init_getenv_protseqs
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
#ifndef HPDCE_RPC_DG_PREF
**  Read the RPC_PREFERRED_PROTSEQ enviroment variable to determine
**  whether we have the preferred protocol sequence we'll use.
#endif
**  Read the RPC_SUPPORTED_PROTSEQS enviroment variable to determine
**  whether we're restricting the protocol sequence we'll use.  The value
**  of the environment variable is a ":"-separated list of protocol
**  sequence ID strings.  Update the protseq table to eliminate any not
**  specified in the environment variable.
**
**  Depends on protocol sequence table having been set up so that we
**  can convert protocol sequence strings to ID via rpc__network_pseq_id_from_pseq().
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:   
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void init_getenv_protseqs (void)

{
    unsigned16          i, j;
    unsigned32          status;
    char                *env_name, *s;
    unsigned_char_t     protseq_string[RPC_C_PROTSEQ_MAX], *sp;
    rpc_protseq_id_t    protseq_id;
    rpc_protseq_id_t    protseq_id_vec[RPC_C_PROTSEQ_ID_MAX];
    unsigned16          protseq_count;


    /*
     * See if environment variable exists.  If it doesn't, we're done now.
     */

    env_name = (char *) getenv ("RPC_SUPPORTED_PROTSEQS");
    if (env_name == NULL)
    {
        return;
    }

    /*
     * Parse the value of the environment variable into a list of protocol
     * sequence IDs.
     */

    protseq_count = 0;
    s = env_name;
    sp = protseq_string;

    do
    {
        if (*s == '\0' || *s == ':')
        {
            *sp = '\0';
            protseq_id = rpc__network_pseq_id_from_pseq (protseq_string, &status);
            if (status == rpc_s_ok)
            {
                protseq_id_vec[protseq_count++] = protseq_id;
            }
            sp = protseq_string;
        }
        else
        {
            *sp++ = *s;
        }
    } while (*s++ != '\0' && protseq_count < RPC_C_PROTSEQ_ID_MAX);

    /*
     * If no valid protocol sequence was specified, we're done now.
     */

    if (protseq_count == 0)
    {   
        return;
    }

    /*
     * Loop through all the supported protocol sequences.  Make any of them
     * that aren't in our list become UNsupported.
     */

    for (j = 0; j < RPC_C_PROTSEQ_ID_MAX; j++)
    {
        rpc_protseq_id_elt_p_t  protseq = (rpc_protseq_id_elt_p_t) &(rpc_g_protseq_id[j]);

        if (protseq->supported)
        {
            for (i = 0; 
                 i < protseq_count && protseq_id_vec[i] != protseq->rpc_protseq_id; 
                 i++)
                ;
    
            if (i >= protseq_count)
            {
                protseq->supported = UNSUPPORTED;
            }
        }
    }
}


/*
**++
**
**  ROUTINE NAME:       init_getenv_port_restriction
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Read the RPC_RESTRICTED_PORTS environment variable to determine whether
**  we're restricting the ports for a (set of) protocol sequence.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:   
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void init_getenv_port_restriction (void)
{

    unsigned_char_p_t   env_name;
    unsigned32          status;

    /*
     * See if environment variable exists.  If it doesn't, we're done now.
     */

    env_name = (unsigned_char_p_t) getenv ("RPC_RESTRICTED_PORTS");
    if (env_name != NULL)
    {
        rpc__set_port_restriction_from_string (env_name, &status);
    }
}                                       /* init_getenv_protseqs */


#endif  /* ifndef NO_GETENV */

/*
**++
**
**  ROUTINE NAME:       rpc__set_port_restriction_from_string
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**      
**  Parses an input string containing a port restriction list for one or
**  more protocol sequences.
**
**  Input grammar:
** 
**     <entry> [COLON <entry>]*
**     
**     <entry> : <protseq_name> LEFT-BRACKET <ranges> RIGHT-BRACKET
**    
**     <ranges>: <range> [COMMA <range>]*
**     
**     <range> : <endpoint-low> HYPHEN <endpoint-high>
**     
**     Example:
**     
**          ncacn_ip_tcp[5000-5110,5500-5521]:ncadg_ip_udp[6500-7000]
**     
**
**  INPUTS:             
**
**      input_string    String as described above.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**      status          rpc_s_ok
**                      rpc_s_invalid_rpc_protseq
**
**  IMPLICIT INPUTS:   
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/


PRIVATE void rpc__set_port_restriction_from_string
#ifdef _DCE_PROTO_
(
 unsigned_char_p_t  input_string,
 unsigned32         *status
)
#else
(input_string, status)
unsigned_char_p_t  input_string;
unsigned32         *status;
#endif
{
    unsigned_char_p_t       buf = NULL;
    unsigned_char_p_t       p;
    unsigned32              max_ranges;
    unsigned_char_p_t       * from_vec = NULL;
    unsigned_char_p_t       * to_vec = NULL;
    unsigned_char_p_t       protseq_name;
    rpc_protseq_id_t        protseq_id;
    unsigned32              range_index;
    boolean                 found;
    boolean                 more_protseqs;
    boolean                 more_ranges;

    CODING_ERROR (status);

    /* 
     * We're going to do some serious gnawing on the input string, so 
     * make a copy.
     */
    
    buf = rpc__stralloc (input_string);
    if (buf == NULL){
	*status = rpc_s_no_memory;
	goto cleanup_and_return;
    }

    /* 
     * rpc__naf_set_port_restriction takes two arrays of pointers to 
     * strings (low port & high port).  Make a pass through the input text 
     * to find how large these vectors must be.  Allocate the vectors.
     */

    max_ranges = 0;
    p = buf;

    while (*p)
    {
        if (*p == '-')
            max_ranges ++;
        p++;
    }                                   /* while *p */

    if (max_ranges == 0)
    {
        *status = rpc_s_invalid_rpc_protseq;
        goto cleanup_and_return;
    }

    RPC_MEM_ALLOC 
        (from_vec,
         unsigned_char_p_t *,
#ifdef SGIMIPS
         max_ranges * (int)sizeof (* from_vec),
#else
         max_ranges * sizeof (* from_vec),
#endif
         RPC_C_MEM_STRING,
         RPC_C_MEM_WAITOK);
    if (from_vec == NULL){
	*status = rpc_s_no_memory;
	goto cleanup_and_return;
    }

    RPC_MEM_ALLOC 
        (to_vec,
         unsigned_char_p_t *,
#ifdef SGIMIPS
         max_ranges * (int)sizeof (* to_vec),
#else
         max_ranges * sizeof (* to_vec),
#endif
         RPC_C_MEM_STRING,
         RPC_C_MEM_WAITOK);
    if (to_vec == NULL){
	*status = rpc_s_no_memory;
	goto cleanup_and_return;
    }

    /* 
     * Loop over protseqs in input string.
     */
    
    p = buf;

    protseq_name = p;
    more_protseqs = true;

    while (more_protseqs)
    {
        
        /* 
         * Loop to find end of protseq.
         */

        found = false;

        while (*p && !found)
        {
            if (*p == '[')
            {
                *p = '\0';
                found = true;
            }
            p++;
        }                               /* while *p */
        
        if (!found)
        {
            *status = rpc_s_invalid_rpc_protseq;
            goto cleanup_and_return;
        }

        protseq_id = 
            rpc__network_pseq_id_from_pseq (protseq_name, status);

        if (*status != rpc_s_ok)
            goto cleanup_and_return;

        /* 
         * Loop over ranges for this protseq.
         */

        more_ranges = true;
        range_index = 0;

        while (more_ranges)
        {
            /* 
             * Grab low port and null terminate.
             */

            from_vec [range_index] = p;

            /* 
             * Scan for end of low range.
             */

            found = false;
            while (*p && !found)
            {
                if (*p == '-')
                {
                    *p = '\0';
                    found = true;
                }
                p++;
            }                           /* while *p */

            if (!found)
            {
                *status = rpc_s_invalid_rpc_protseq;
                goto cleanup_and_return;
            }

            to_vec [range_index] = p;

            /* 
             * Scan for end of high range.
             */
            
            found = false;
            while (*p && !found)
            {
                if (*p == ']')
                {
                    *p = '\0';
                    more_ranges = false;
                    found = true;
                }

                else if (*p == ',')
                {
                    *p = '\0';
                    found = true;
                }
                p++;
            }                           /* while *p */
            
            if (!found)
            {
                *status = rpc_s_invalid_rpc_protseq;
                goto cleanup_and_return;
            }

            range_index ++;

        }                               /* while more ranges */

        /* 
         * Have everything for this protocol sequence.  
         */

        rpc__naf_set_port_restriction
            (protseq_id,
             range_index,
             from_vec,      
             to_vec,
             status);

        if (*status != rpc_s_ok)
            goto cleanup_and_return;
                 
        /* 
         * At this point we're at the physical end of the string, or at the 
         * beginning of the next protseq.
         */
        
        if (*p == ':')
        {
            more_protseqs = true;       
            p++;
            protseq_name = p;
        }
        else if (*p == '\0')
            more_protseqs = false;
        else
        {
            *status = rpc_s_invalid_rpc_protseq;
            goto cleanup_and_return;
        }
    }                                   /* while more_protseqs */
                   
    *status = rpc_s_ok;
    
cleanup_and_return:

    if (buf != NULL)
        rpc_string_free (&buf, status);

    if (from_vec != NULL)
        RPC_MEM_FREE (from_vec, RPC_C_MEM_STRING);

    if (to_vec != NULL)
        RPC_MEM_FREE (to_vec, RPC_C_MEM_STRING);
}


#ifdef ATFORK_SUPPORTED

/*
**++
**
**  ROUTINE NAME:       rpc__fork_handler
**
**  SCOPE:              PRIVATE - declared in cominit.h
**
**  DESCRIPTION:
**      
**  This routine is called prior to, and immediately after, forking
**  the process's address space.  The input argument specifies which
**  stage of the fork we're currently in.
**
**  INPUTS:             
**
**        stage         indicates the stage in the fork operation
**                      (prefork | postfork_parent | postfork_child)
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/       
PRIVATE void rpc__fork_handler
#ifdef _DCE_PROTO_
(
  rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{   
    unsigned32 ctr;
    rpc_protocol_id_elt_p_t rpc_protocol;
    boolean32 itimer_was_running = false;

    /*
     * Pre-fork handlers are called in reverse order of init_once().
     * Post-fork handlers are called in same order.
     *
     * First, take care of stage-independent operations, such as 
     * calling into handlers that differentiate among the stages
     * themselves.
     *
     * Next, take care of any stage-specific activities for this
     * module..
     */

    switch ((int)stage)
    {
    case RPC_C_PREFORK:
	/*
	 * First, stop the itimer so that we won't be interrupted
	 * while executing atfork handlers.
	 */
	itimer_was_running = rpc__timer_itimer_stop();
        rpc__network_fork_handler(stage);
        /* each auth protocol */
        /* auth_info_cache */
        /*
         * go through the RPC protocol id table and call each protocol's
         * fork handler.
         */
        for (ctr = 0; ctr < RPC_C_PROTOCOL_ID_MAX; ctr++)
        {   
            rpc_protocol = (rpc_protocol_id_elt_p_t) &(rpc_g_protocol_id[ctr]);
            if (rpc_protocol->prot_fork_handler != NULL)
            {
                (*rpc_protocol->prot_fork_handler)(stage);
            }
        }
        /* each NAF */
        /* cthread */
        rpc__obj_fork_handler(stage);
        rpc__if_fork_handler(stage);
        rpc__timer_fork_handler(stage);
        rpc__list_fork_handler(stage);
        break;
    case RPC_C_POSTFORK_CHILD:  
        /*
         * Reset any debug switches.
         */       
#ifdef DEBUG
        if (!RPC_DBG(rpc_es_dbg_inherit, 1))
        {
            for (ctr = 0; ctr < RPC_C_DBG_SWITCHES; ctr++)
                rpc_g_dbg_switches[ctr] = 0; 
        }
#endif            
        /*
         * We want the rpc__init code to run in the new invokation.
         */
        rpc_g_initialized = false;
        /*b_z_e_r_o((char *)&init_once_block, sizeof(pthread_once_t));*/
        memset((char *)&init_once_block, 0, sizeof(pthread_once_t));
        
        /*
         * Increment the global fork count.  For more info on the use
         * of this variable, see comp.c.
         */
        rpc_g_fork_count++;
        
        /* fall through */
    case RPC_C_POSTFORK_PARENT:
        rpc__list_fork_handler(stage);
        rpc__timer_fork_handler(stage);
        rpc__if_fork_handler(stage);
        rpc__obj_fork_handler(stage);
        /* cthread */
        /* each NAF */
        /*
         * go through the RPC protocol id table and call each protocol's
         * fork handler.
         */
        for (ctr = 0; ctr < RPC_C_PROTOCOL_ID_MAX; ctr++)
        {   
            rpc_protocol = (rpc_protocol_id_elt_p_t) &(rpc_g_protocol_id[ctr]);
            if (rpc_protocol->prot_fork_handler != NULL)
            {
                (*rpc_protocol->prot_fork_handler)(stage);
            }
        }
        /* auth_info_cache */
        /* each auth protocol */
        rpc__network_fork_handler(stage);
        break;
    }  
    if (stage == RPC_C_POSTFORK_PARENT && itimer_was_running)
	rpc__timer_itimer_start();
}
#endif


