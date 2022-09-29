/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: mgmt.c,v 65.7 1998/04/01 14:17:04 gwehrman Exp $";
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
 * $Log: mgmt.c,v $
 * Revision 65.7  1998/04/01 14:17:04  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Turned off 'pointless comparison of unsigned integer with zero'
 * 	errors.
 *
 * Revision 65.6  1998/03/23  17:29:07  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/02/26 17:05:42  lmc
 * Added prototyping and casting.
 *
 * Revision 65.4  1998/01/07  17:21:19  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:22  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:03  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.508.1  1996/06/04  21:56:11  arvind
 * 	u2u merge: Add rpc_mgmt_inq_svr_princ_name_tgt() and
 * 	inq_princ_name_tgt()
 * 	[1996/05/06  20:32 UTC  burati  /main/DCE_1.2/2]
 *
 * 	merge u2u work
 * 	[1996/04/29  21:11 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	u2u work off Mothra, before 1.2 branch is available
 * 	[1996/04/29  20:26 UTC  burati  /main/HPDCE02/mb_mothra8/1]
 *
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:34 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:15 UTC  lmm  /main/HPDCE02/1]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:19 UTC  lmm  /main/lmm_rpc_alloc_fail_detect/1]
 *
 * Revision 1.1.506.2  1996/02/18  00:04:43  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:52  marty]
 * 
 * Revision 1.1.506.1  1995/12/08  00:21:00  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:34 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 	[1995/12/07  23:59:44  root]
 * 
 * Revision 1.1.504.2  1994/07/15  18:19:28  tom
 * 	Bug 9933 - Since we declare mgmt.idl with the fault_status
 * 	attribute, we need to check return status for cancels and
 * 	re-raise them.
 * 	[1994/07/14  21:29:10  tom]
 * 
 * Revision 1.1.504.1  1994/01/21  22:38:06  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:23  cbrooks]
 * 
 * Revision 1.1.4.6  1993/03/31  22:13:51  markar
 * 	     OT CR 6212 fix: add support for setting server-side com timeouts
 * 	[1993/03/31  20:57:07  markar]
 * 
 * Revision 1.1.4.5  1993/01/03  23:52:57  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:09:05  bbelch]
 * 
 * Revision 1.1.4.4  1992/12/23  21:12:03  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:40:50  zeliff]
 * 
 * Revision 1.1.4.3  1992/10/07  20:05:19  markar
 * 	  CR 5462 fix: Fixed comment in rpc_mgmt_set_com_timeout()
 * 	[1992/10/07  17:56:12  markar]
 * 
 * Revision 1.1.4.2  1992/10/02  15:09:00  markar
 * 	  CR 3895: fixed (rpc_mgmt_)is_server_listening return codes.
 * 	[1992/09/22  16:10:43  markar]
 * 
 * Revision 1.1.2.2  1992/05/01  16:38:04  rsalz
 * 	  1-apr-92 ko        fix rpc_mgmt_is_server_listening to set
 * 	                     return status when binding handle is NULL
 * 	 26-mar-92 nm        - Deal with error return in inq_princ_name().
 * 	                     - Fix dealing with error return in inq_stats().
 * 	  6-mar-92 wh        DCE 1.0.1 merge.
 * 	 27-jan-92 mishkin   fix rpc__mgmt_authorization_check to set return
 * 	                     status more cleanly.
 * 	[1992/05/01  16:29:48  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:58  devrcs
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
**      mgmt.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of the Management Component of the RPC Runtime Sytem.
**  This module contains both Local management functions (those which
**  only execute locally) and Local/Remote management functions (those
**  which can execute either locally or on a remote server). The class
**  into which each function falls is identified in its description.
**  For each Local/Remote function there is a corresponding routine that
**  provides the remote (manager) implementation of its functionality.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime  */
#include <com.h>        /* Common communications services           */
#include <comp.h>       /* Private communications services          */
#include <mgmtp.h>      /* Private management services              */
#include <dce/mgmt.h>   /* Remote RPC Management Interface          */


/*
 * Authorization function to use to check remote access. 
 */

INTERNAL rpc_mgmt_authorization_fn_t authorization_fn = NULL;

/*
 * Default server comm timeout value.
 */
INTERNAL unsigned32 server_com_timeout;

/*
 * Size of buffer used when asking for remote server's principal name
 */

#define MAX_SERVER_PRINC_NAME_LEN 500

/*
 * Size of buffer used when asking for remote server's TGT data
 */

#define MAX_SERVER_TGT_LEN 1024


/*
 * Forward definitions of network manager entry points (implementation
 * of mgmt.idl).
 */

INTERNAL void inq_if_ids _DCE_PROTOTYPE_ ((    
        rpc_binding_handle_t     /*binding_h*/,
        rpc_if_id_vector_p_t    * /*if_id_vector*/,
        unsigned32              * /*status*/
    ));

INTERNAL void inq_stats _DCE_PROTOTYPE_ ((            
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32              * /*count*/,
        unsigned32              statistics[],
        unsigned32              * /*status*/
    ));

INTERNAL boolean32 is_server_listening _DCE_PROTOTYPE_ ((            
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32              * /*status*/
    ));


INTERNAL void inq_princ_name _DCE_PROTOTYPE_ ((            
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32               /*authn_proto*/,
        unsigned32               /*princ_name_size*/,
        idl_char                princ_name[],
        unsigned32              * /*status*/
    ));

INTERNAL void inq_princ_name_tgt _DCE_PROTOTYPE_ ((            
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32               /*authn_proto*/,
        unsigned32               /*princ_name_size*/,
        unsigned32               /*max_tgt_size*/,
        idl_char                princ_name[],
        unsigned32              * /*tgt_length*/,
        idl_char                * /*tgt_data*/,
        unsigned32              * /*status*/
    ));



#ifdef SGIMIPS
INTERNAL idl_void_p_t my_allocate _DCE_PROTOTYPE_ ((
        idl_size_t/*size*/
    ));
#else
INTERNAL idl_void_p_t my_allocate _DCE_PROTOTYPE_ ((
        unsigned long  /*size*/
    ));
#endif /* SGIMIPS */

INTERNAL void my_free _DCE_PROTOTYPE_ ((
        idl_void_p_t  /*ptr*/
    ));

INTERNAL void remote_binding_validate _DCE_PROTOTYPE_ ((
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32              * /*status*/
    ));



/*
**++
**
**  ROUTINE NAME:       rpc__mgmt_init
**
**  SCOPE:              PRIVATE - declared in cominit.c
**
**  DESCRIPTION:
**      
**  Initialize the management component. Register the remote management
**  interface for the RPC runtime.
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
**  FUNCTION VALUE:
**
**      returns rpc_s_ok if everything went well, otherwise returns
**      an error status
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE unsigned32 rpc__mgmt_init(void)

{
    unsigned32              status;
    /*
     * Manager EPV for implementation of mgmt.idl.
     */
    static mgmt_v1_0_epv_t mgmt_v1_0_mgr_epv =
    {
        inq_if_ids,
        inq_stats,
        is_server_listening,
        rpc__mgmt_stop_server_lsn_mgr,
        inq_princ_name,
        inq_princ_name_tgt
    };

    

    /*
     * register the remote management interface with the runtime
     * as an internal interface
     */
    rpc__server_register_if_int
        ((rpc_if_handle_t) mgmt_v1_0_s_ifspec, NULL, 
        (rpc_mgr_epv_t) &mgmt_v1_0_mgr_epv, true, &status);
                                   
    authorization_fn = NULL;

    server_com_timeout = rpc_c_binding_default_timeout;

    return (status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_com_timeout
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that inquires what the timeout
**  value is in a binding.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be read.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      timeout         The relative timeout value used when making a
**                      connection to the location specified in the
**                      binding rep.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

#ifdef SGIMIPS
PUBLIC void rpc_mgmt_inq_com_timeout (
rpc_binding_handle_t    binding_handle,
unsigned32              *timeout,
unsigned32              *status)
#else
PUBLIC void rpc_mgmt_inq_com_timeout (binding_handle, timeout, status)
        
rpc_binding_handle_t    binding_handle;
unsigned32              *timeout;
unsigned32              *status;
#endif

{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_handle;
    

    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    *timeout = binding_rep->timeout;
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_server_com_timeout
**
**  SCOPE:              PUBLIC - declared in rpcpvt.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that returns the default server-side
**  com timeout setting.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**                          RPC Protocol ID in binding handle was invalid.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     unsigned32, the current com timeout setting
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC unsigned32 rpc_mgmt_inq_server_com_timeout (void)

{
    RPC_VERIFY_INIT ();
    
    return (server_com_timeout);
}


/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_if_ids
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local/Remote management function that obtains a vector of
**  interface identifications listing the interfaces registered with the
**  RPC runtime. If a server has not registered any interfaces this routine
**  will return an rpc_s_no_interfaces status code and a NULL if_id_vector.
**  The application is responsible for calling rpc_if_id_vector_free to
**  release the memory used by the vector.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      if_id_vector    A vector of the if id's registered for this server
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PRIVATE void rpc_mgmt_inq_if_ids 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_if_id_vector_p_t    *if_id_vector,
    unsigned32              *status
)
#else
(binding_h, if_id_vector, status)
rpc_binding_handle_t    binding_h;
rpc_if_id_vector_p_t    *if_id_vector;
unsigned32              *status;
#endif
{
#ifdef SGIMIPS
    idl_void_p_t            (*old_allocate) _DCE_PROTOTYPE_ ((idl_size_t));
    idl_void_p_t            (*tmp_allocate) _DCE_PROTOTYPE_ ((idl_size_t));
#else
    idl_void_p_t            (*old_allocate) _DCE_PROTOTYPE_ ((unsigned long));
    idl_void_p_t            (*tmp_allocate) _DCE_PROTOTYPE_ ((unsigned long));
#endif /* SGIMIPS */
    void                    (*old_free) _DCE_PROTOTYPE_ ((idl_void_p_t));
    void                    (*tmp_free) _DCE_PROTOTYPE_ ((idl_void_p_t));

    RPC_VERIFY_INIT ();
    
    /*
     * if this is a local request, just do it locally
     */
    if (binding_h == NULL)
    {
        rpc__if_mgmt_inq_if_ids (if_id_vector, status);
    }
    else
    {
        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
            return;

        /*
         * force the stubs to use malloc and free (because the caller is going
         * to have to free the if id vector using rpc_if_id_vector_free() later
         */
        rpc_ss_swap_client_alloc_free
            (my_allocate, my_free, &old_allocate, &old_free);

        /*
         * call the corresponding remote routine to get an if id vector
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_inq_if_ids)
            (binding_h, if_id_vector, status);

        if (*status == rpc_s_call_cancelled)
            pthread_cancel(pthread_self());
        
        /*
         * restore the memory allocation scheme in effect before we got here
         */
        rpc_ss_swap_client_alloc_free
            (old_allocate, old_free, &tmp_allocate, &tmp_free);
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_stats
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local/Remote management function that obtains statistics
**  about the specified server from the RPC runtime. Each element in the
**  returned argument contains an integer value which can be indexed using
**  the defined statistics constants.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      stats           An vector of statistics values for this server.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_mgmt_inq_stats 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_stats_vector_p_t    *statistics,
    unsigned32              *status
)
#else
(binding_h, statistics, status)
rpc_binding_handle_t    binding_h;
rpc_stats_vector_p_t    *statistics;
unsigned32              *status;
#endif
{
    unsigned32              i;

    RPC_VERIFY_INIT ();

    /*
     * Allocate a stats vector large enough to hold all the
     * statistics we know about and set the vector count to match
     * the size allocated.
     */
    RPC_MEM_ALLOC (*statistics, rpc_stats_vector_p_t, 
                   sizeof (rpc_stats_vector_t)
                   + (sizeof ((*statistics)->stats) *
                      (rpc_c_stats_array_max_size - 1)),
                   RPC_C_MEM_STATS_VECTOR,
                   RPC_C_MEM_WAITOK);
    if (*statistics == NULL){
	*status = rpc_s_no_memory;
	return;
    }
    (*statistics)->count = rpc_c_stats_array_max_size;
    
    /*  
     * If this is a local request, just do it locally.
     */
    if (binding_h == NULL)
    {
        /*
         * Clear the output array and query each protocol service
         * for its information. Sum the results in the output array.
         */
        memset (&(*statistics)->stats[0], 0, ((*statistics)->count * sizeof (unsigned32)));
        for (i = 0; i < RPC_C_PROTOCOL_ID_MAX; i++)
        {
            if (RPC_PROTOCOL_INQ_SUPPORTED (i))
            {
                (*statistics)->stats[rpc_c_stats_calls_in] +=
                (*rpc_g_protocol_id[i].mgmt_epv->mgmt_inq_calls_rcvd)();

                (*statistics)->stats[rpc_c_stats_calls_out] +=
                (*rpc_g_protocol_id[i].mgmt_epv->mgmt_inq_calls_sent)(); 

                (*statistics)->stats[rpc_c_stats_pkts_in] +=
                (*rpc_g_protocol_id[i].mgmt_epv->mgmt_inq_pkts_rcvd)();

                (*statistics)->stats[rpc_c_stats_pkts_out] +=
                (*rpc_g_protocol_id[i].mgmt_epv->mgmt_inq_pkts_sent)(); 
            }
        }
        *status = rpc_s_ok;
    }
    else
    {
    
        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
            return;

        /*
         * Call the corresponding remote routine to get remote stats.
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_inq_stats) (binding_h, 
                                                &(*statistics)->count, 
                                                &(*statistics)->stats[0],
                                                status);

        if (*status == rpc_s_call_cancelled)
            pthread_cancel(pthread_self());
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_stats_vector_free
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will free the statistics vector memory allocated by
**  and returned by rpc_mgmt_inq_stats.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:    
**
**      stats           A pointer to a pointer to the stats vector.
**                      The contents will be NULL on output.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

PUBLIC void rpc_mgmt_stats_vector_free 
#ifdef _DCE_PROTO_
(
    rpc_stats_vector_p_t    *statistics,
    unsigned32              *status
)
#else
(statistics, status)
rpc_stats_vector_p_t    *statistics;
unsigned32              *status;
#endif
{
    RPC_MEM_FREE (*statistics, RPC_C_MEM_STATS_VECTOR);
    *statistics = NULL;
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_is_server_listening
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local/Remote management function that determines if the
**  specified server is listening for remote procedure calls.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      true - if server is listening
**      false - if server is not listening :-)
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 rpc_mgmt_is_server_listening 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{


    RPC_VERIFY_INIT ();
    
    /*
     * if this is a local request, just do it locally
     */
    if (binding_h == NULL)
    {
        *status = rpc_s_ok;
        return (rpc__server_is_listening());
    }
    else
    {
        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
            return (false);

        /*
         * call the corresponding remote routine
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_is_server_listening) (binding_h, status);

        if (*status == rpc_s_call_cancelled)
            pthread_cancel(pthread_self());

        return (*status == rpc_s_ok ? true : false);
     }
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_cancel_timeout
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that sets the amount of time the
**  RPC runtime is to wait for a server to acknowledge a cancel before
**  orphaning the call. The application should specify to either wait
**  forever or to wait the length of the time specified in seconds. If the
**  value of seconds is 0 the remote procedure call is orphaned as soon as
**  a cancel is received by the server and control returns immediately to 
**  the client application. The default is to wait forever for the call to
**  complete.
**
**  The value for the cancel timeout applies to all remote procedure calls
**  made in the current thread. A multi-threaded client that wishes to change
**  the default timeout value must call this routine in each thread of
**  execution.
**
**  INPUTS:
**
**      seconds         The number of seconds to wait for an acknowledgement.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_no_memory
**          rpc_s_coding_error
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

PUBLIC void rpc_mgmt_set_cancel_timeout 
#ifdef _DCE_PROTO_
(
    signed32                seconds,
    unsigned32              *status
)
#else
(seconds, status)
signed32                seconds;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    /*
     * set the cancel timeout value in the per-thread context block
     * for this thread
     */
    RPC_SET_CANCEL_TIMEOUT (seconds, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_call_timeout
**
**  SCOPE:              PUBLIC - (SHOULD BE) declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that sets the amount of time the
**  RPC runtime is to wait for a server to complete a call.  A timeout of
**  0 means no max call execution time is imposed (this is the default).
**
**  The value for the call timeout applies to all remote procedure calls
**  made using the specified binding handle.
**
**  This function currently is NOT a documented API operation (i.e. it
**  is not in rpc.idl).  At least initially, only the ncadg_ protocols
**  support the use of this timeout. This function is necessary for
**  "kernel" RPC support since kernels don't support a cancel mechanism.
**  User space applications can build an equivalent mechanism using
**  cancels.  This code is not conditionally compiled for the kernel
**  so that we can test it in user space.
**
**  INPUTS:
**
**      binding         The binding handle to use.
**      seconds         The number of seconds to wait for call completion.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_no_memory
**          rpc_s_coding_error
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_mgmt_set_call_timeout _DCE_PROTOTYPE_ ((
        rpc_binding_handle_t     /*binding_h*/,
        unsigned32               /*seconds*/,
        unsigned32              * /*status*/
    ));

PUBLIC void rpc_mgmt_set_call_timeout 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              seconds,
    unsigned32              *status
)
#else
(binding_h, seconds, status)
rpc_binding_handle_t    binding_h;
unsigned32              seconds;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t binding_rep = (rpc_binding_rep_p_t) binding_h; 

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    binding_rep->call_timeout_time = RPC_CLOCK_SEC(seconds);
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_com_timeout
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that sets the RPC timeout for a
**  binding. The timeout value is a metric indicating the relative amount
**  of time retries to contact the server should be made. The value 10
**  indicates an unbounded wait. A zero value indicates no wait. Values 1-5
**  favor fast reponse time over correctness in determining whether the server
**  is alive. Values 6-9 favor correctness over response time. The RPC
**  Protocol Service identified by the RPC Protocol ID in the Binding Rep
**  will be notified that the Binding Rep has changed.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be modified.
**
**      timeout         The relative timeout value to be used when making
**                      a connection to the location in the binding rep.
**
**                      0   -   rpc_c_binding_min_timeout
**                      5   -   rpc_c_binding_default_timeout
**                      9   -   rpc-c_binding_max_timeout
**                      10  -   rpc_c_binding_infinite_timeout
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
**          rpc_s_invalid_timeout
**                          Timeout value is not in the range -1 to 10
**          rpc_s_coding_error
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

PUBLIC void rpc_mgmt_set_com_timeout 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              timeout,
    unsigned32              *status
)
#else
(binding_h, timeout, status)
rpc_binding_handle_t    binding_h;
unsigned32              timeout;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;
    
    /*
     * see if the timeout value is valid
     */
#ifdef SGIMIPS
/*
 * The compiler generates the warning "pointless comparison of unsigned
 * integer with zero" if rpc_c_binding_min_timeout is less than zero.
 * This code cannot be modified becaue rpc_c_binding_min_timeout may be
 * changed to a value greater than zero.
 */
#pragma set woff 1183
#endif
    if (timeout < rpc_c_binding_min_timeout ||
        timeout > rpc_c_binding_max_timeout)
    {
        if (timeout != rpc_c_binding_infinite_timeout)
        {
            *status = rpc_s_invalid_timeout;
            return;
        }
    }
#ifdef SGIMIPS
#pragma reset woff 1183
#endif
         
    /*
     * copy the new timeout value into the binding rep
     */
    binding_rep->timeout = timeout;

    /*
     * notify the protocol service that the binding has changed
     */
#ifdef FOO

Note: there should be a dispatching routine in com...

    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_changed) (binding_rep, status);

#else

    *status = rpc_s_ok;

#endif
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_server_com_timeout
**
**  SCOPE:              PUBLIC - declared in rpcpvt.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that sets a default RPC timeout for
**  all calls handled by a server.  The timeout value is a metric indicating 
**  the relative amount of time retries to contact the client should be made. 
**  The value 10 indicates an unbounded wait. A zero value indicates no wait. 
**  Values 1-5 favor fast reponse time over correctness in determining whether 
**  the client is alive. Values 6-9 favor correctness over response time. 
**
**  INPUTS:
**
**      timeout         The relative timeout value to be used by all calls
**                      run on this server.
**
**                      0   -   rpc_c_binding_min_timeout
**                      5   -   rpc_c_binding_default_timeout
**                      9   -   rpc-c_binding_max_timeout
**                      10  -   rpc_c_binding_infinite_timeout
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_timeout
**                          Timeout value is not in the range -1 to 10
**          rpc_s_coding_error
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

PUBLIC void rpc_mgmt_set_server_com_timeout 
#ifdef _DCE_PROTO_
(
 unsigned32              timeout,
 unsigned32 * status
)
#else
(timeout, status)
unsigned32              timeout;
unsigned32              *status;
#endif
{

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    /*
     * see if the timeout value is valid
     */
#ifdef SGIMIPS
/*
 * The compiler generates the warning "pointless comparison of unsigned
 * integer with zero" if rpc_c_binding_min_timeout is less than zero.
 * This code cannot be modified becaue rpc_c_binding_min_timeout may be
 * changed to a value greater than zero.
 */
#pragma set woff 1183
#endif
    if (timeout < rpc_c_binding_min_timeout ||
        timeout > rpc_c_binding_max_timeout)
    {
        if (timeout != rpc_c_binding_infinite_timeout)
        {
            *status = rpc_s_invalid_timeout;
            return;
        }
    }
#ifdef SGIMIPS
#pragma reset woff 1183
#endif

    server_com_timeout = timeout;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_server_stack_size
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local management function that sets the value that the
**  RPC runtime is to use in specifying the the thread stack size when
**  creating call threads. This value will be applied to all threads
**  created for the server.
**
**  INPUTS:
**
**      thread_stack_size   The value to be used by the RPC runtime for
**                      specifying the stack size when creating threads.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_coding_error
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

PUBLIC void rpc_mgmt_set_server_stack_size 
#ifdef _DCE_PROTO_
(
    unsigned32              thread_stack_size,
    unsigned32              *status
)
#else
(thread_stack_size, status)
unsigned32              thread_stack_size;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

/* !!! the 2nd one is due to a CMA pthreads bug and should go away */
#if !defined(_POSIX_THREAD_ATTR_STACKSIZE) && !defined(_POSIX_PTHREAD_ATTR_STACKSIZE)
    *status = rpc_s_not_supported;
#else
#  ifndef PTHREAD_EXC
    if (pthread_attr_setstacksize
        (&rpc_g_server_pthread_attr, thread_stack_size) == -1)
    {
        *status = rpc_s_invalid_arg;
        return;
    }

    *status = rpc_s_ok;
#  else

    pthread_attr_setstacksize
        (&rpc_g_server_pthread_attr, thread_stack_size);

    *status = rpc_s_ok;
#  endif
#endif
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_stop_server_listening
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local/Remote management function that directs a server to
**  stop listening for remote procedure calls. On receipt of a stop listening
**  request the RPC runtime stops accepting new remote procedure calls for all
**  registered interfaces. Executing calls are allowed to complete, including
**  callbacks. After alls executing calls complete the rpc_server_listen()
**  routine returns to the caller.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_mgmt_stop_server_listening 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{

    RPC_VERIFY_INIT ();
    
    /*
     * if this is a local request, just do it locally
     */
    if (binding_h == NULL)
    {
        rpc__server_stop_listening (status);
    }
    else
    {

        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
            return;

        /*
         * call the corresponding remote routine
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_stop_server_listening) (binding_h, status);

        if (*status == rpc_s_call_cancelled)
            pthread_cancel(pthread_self());
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_server_princ_name
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This is a Local/Remote management function that directs a server to
**  
**  
**  
**  
**  
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_mgmt_inq_server_princ_name 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              authn_protocol,
    unsigned_char_p_t       *server_princ_name,
    unsigned32              *status
)
#else
(binding_h, authn_protocol, server_princ_name, status)
rpc_binding_handle_t    binding_h;
unsigned32              authn_protocol;
unsigned_char_p_t       *server_princ_name;
unsigned32              *status;
#endif
{
    unsigned32          dce_rpc_authn_protocol;


    RPC_VERIFY_INIT ();

    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, status);

    dce_rpc_authn_protocol = 
        rpc_g_authn_protocol_id[authn_protocol].dce_rpc_authn_protocol_id;

    RPC_MEM_ALLOC (
        *server_princ_name,
        unsigned_char_p_t,
        MAX_SERVER_PRINC_NAME_LEN,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
    if (*server_princ_name == NULL){
	*status = rpc_s_no_memory;
	return;
    }
    
    /*
     * if this is a local request, just do it locally
     */
    if (binding_h == NULL)
    {
        rpc__auth_inq_my_princ_name 
            (dce_rpc_authn_protocol, MAX_SERVER_PRINC_NAME_LEN, 
             *server_princ_name, status);
    }
    else
    {
        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
        {
            RPC_MEM_FREE (*server_princ_name, RPC_C_MEM_STRING);
            return;
        }

        /*
         * call the corresponding remote routine
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_inq_princ_name)
            (binding_h, 
             dce_rpc_authn_protocol,
             MAX_SERVER_PRINC_NAME_LEN, 
             *server_princ_name,
             status);

        if (*status != rpc_s_ok)
        {
            RPC_MEM_FREE (*server_princ_name, RPC_C_MEM_STRING);
            if (*status == rpc_s_call_cancelled)
                pthread_cancel(pthread_self());
            return;
        }
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_set_authorization_fn
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  A server application calls the rpc_mgmt_set_authorization_fn routine
**  to specify an authorization function to control remote access to
**  the server's remote management routines.
**
**  INPUTS:
**
**      authorization_fn
**                      Authorization function to be used
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

PUBLIC void rpc_mgmt_set_authorization_fn 
#ifdef _DCE_PROTO_
(
    rpc_mgmt_authorization_fn_t authorization_fn_arg,
    unsigned32              *status
)
#else
(authorization_fn_arg, status)
rpc_mgmt_authorization_fn_t authorization_fn_arg;
unsigned32              *status;
#endif
{
    RPC_VERIFY_INIT ();
    authorization_fn = authorization_fn_arg;
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       inq_if_ids
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  This is the manager routine that provides remote access to the
**  rpc_mgmt_inq_if_ids function.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      if_id_vector    A vector of the if id's registered for this server
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_no_memory
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

INTERNAL void inq_if_ids 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_if_id_vector_p_t    *if_id_vector,
    unsigned32              *status
)
#else
(binding_h, if_id_vector, status)
rpc_binding_handle_t    binding_h;
rpc_if_id_vector_p_t    *if_id_vector;
unsigned32              *status;
#endif
{
    rpc_if_id_vector_p_t    local_if_id_vector;
    unsigned32              index;
    unsigned32              temp_status;
    

    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_inq_if_ids, 
                               true, status))
    {
        *if_id_vector = NULL;
        return;
    }

    /*
     * call the corresponding local routine to get a local if id vector
     */
    rpc_mgmt_inq_if_ids (NULL, &local_if_id_vector, status);

    if (*status != rpc_s_ok)
    {
        *if_id_vector = NULL;
        return;
    }

    /*
     * allocate memory to hold the output argument so that it can be
     * freed by the stubs when we're done
     */
    *if_id_vector = (rpc_if_id_vector_p_t)
        rpc_ss_allocate (((sizeof local_if_id_vector->count) +
            (local_if_id_vector->count * sizeof (rpc_if_id_p_t))));

    if (*if_id_vector == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * set the count field in the output vector
     */
    (*if_id_vector)->count = local_if_id_vector->count;
    
    /*
     * walk the local vector and for each element create a copy in the
     * output vector
     */
    for (index = 0; index < local_if_id_vector->count; index++)
    {
        (*if_id_vector)->if_id[index] = (rpc_if_id_p_t)
            rpc_ss_allocate (sizeof (rpc_if_id_t));
            
        if ((*if_id_vector)->if_id[index] == NULL)
        {
            /*
             * if we can't create a copy of any element, free the local
             * vector, free all existing elements in the output vector,
             * and free the output vector itself
             */
            rpc_if_id_vector_free (&local_if_id_vector, &temp_status);

            while (index > 0)
            {
                rpc_ss_free ((char *) (*if_id_vector)->if_id[--index]);
            }
            
            rpc_ss_free ((char *) *if_id_vector);
            
            *if_id_vector = NULL;
            *status = rpc_s_no_memory;
            return;
        }

        /*
         * Copy the the entry in the local vector to the output vector.
         */
        (*if_id_vector)->if_id[index]->uuid = 
            local_if_id_vector->if_id[index]->uuid;
        (*if_id_vector)->if_id[index]->vers_major = 
            local_if_id_vector->if_id[index]->vers_major;
        (*if_id_vector)->if_id[index]->vers_minor = 
            local_if_id_vector->if_id[index]->vers_minor;
    }

    /*
     * free the local vector
     */
    rpc_if_id_vector_free (&local_if_id_vector, &temp_status);

    /*
     * return the output vector
     */
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc__mgmt_stop_server_lsn_mgr
**
**  SCOPE:              PRIVATE - declared in mgmtp.h
**
**  DESCRIPTION:
**      
**  This is the manager routine that provides remote access to the
**  rpc_mgmt_stop_server_listening function.  This routine is PRIVATE
**  instead of INTERNAL so it can be used by the RRPC i/f as well.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

PRIVATE void rpc__mgmt_stop_server_lsn_mgr 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{
    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_stop_server_listen, 
                               false, status))
    {
        return;
    }

    rpc_mgmt_stop_server_listening (NULL, status);
}

/*
**++
**
**  ROUTINE NAME:       inq_stats
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  This is the manager routine that provides remote access to the
**  rpc_mgmt_inq_stats function.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     
**
**      count           The maximum size of the array on input and
**                      the actual size of the array on output.
**
**  OUTPUTS:
**
**      stats           An array of statistics values for this server.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

INTERNAL void inq_stats 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *count,
    unsigned32              statistics[],
    unsigned32              *status
)
#else
(binding_h, count, statistics, status)
rpc_binding_handle_t    binding_h;
unsigned32              *count;
unsigned32              statistics[];
unsigned32              *status;
#endif
{
    rpc_stats_vector_p_t        stats_vector;
    unsigned32                  temp_status;
    unsigned32                  i;

    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_inq_stats, 
                               true, status))
    {
        *count = 0;
        return;
    }

    /*
     * Call the corresponding local routine to get the stats array
     */
    rpc_mgmt_inq_stats (NULL, &stats_vector, status);
    if (*status != rpc_s_ok)
    {
        *count = 0;
        return;
    }
     
    *count = stats_vector->count;

    for (i = 0; i < *count; i++)
    {
        statistics[i] = stats_vector->stats[i];
    }
    rpc_mgmt_stats_vector_free (&stats_vector, &temp_status);
}

/*
**++
**
**  ROUTINE NAME:       is_server_listening
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  This is the manager routine that returns true if it is ever executed to
**  indicate that the server is listening for remote calls.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      true - if server is listening
**      false - if server is not listening :-)
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean32 is_server_listening 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{
    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_is_server_listen, 
                               true, status))
    {
        return (false);     /* Sort of pointless, since we're answering anyway */
    }

    /*
     * Cogito ergo sum.
     */
    *status = rpc_s_ok;
    return (true);
}

/*
**++
**
**  ROUTINE NAME:       inq_princ_name
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  This is a manager routine that provides a remote caller with the
**  principal name (really one of the principal names) for a server.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**      authn_proto     The *wire* authentication protocol ID we're
**                      interested in
**
**      princ_name_size The max size of princ_name
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      princ_name      Server's principal name
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

INTERNAL void inq_princ_name 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              authn_proto,
    unsigned32              princ_name_size,
    idl_char                princ_name[],
    unsigned32              *status
)
#else
(binding_h, authn_proto, princ_name_size, princ_name, status)
rpc_binding_handle_t    binding_h;
unsigned32              authn_proto;
unsigned32              princ_name_size;
idl_char                princ_name[];
unsigned32              *status;
#endif
{
    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_inq_princ_name,
                               true, status))
    {
        princ_name[0] = '\0';
        return;
    }

    rpc__auth_inq_my_princ_name 
        (authn_proto, princ_name_size, (unsigned_char_p_t) princ_name, status);

    if (*status != rpc_s_ok) 
    {
        princ_name[0] = '\0';
    }
}


/*
**++
**
**  ROUTINE NAME:       rpc__mgmt_authorization_check
**
**  SCOPE:              PRIVATE - declared in mgmtp.h
**
**  DESCRIPTION:
**      
**  Routine to check whether a management operation is allowed.  This
**  routine is PRIVATE instead of INTERNAL so it can be used by the RRPC
**  i/f as well.
**
**  INPUTS:
**
**      binding_h       RPC binding handle
**
**      op              Management operation in question
**
**      deflt           What to return in there's no authorization function set 
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            
**
**      status          A value indicating the status of the routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean
**
**      return          Whether operation is allowed
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean32 rpc__mgmt_authorization_check 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              op,
    boolean32               deflt,
    unsigned32              *status
)
#else
(binding_h, op, deflt, status)
rpc_binding_handle_t    binding_h;
unsigned32              op;
boolean32               deflt;
unsigned32              *status;
#endif
{
    if (authorization_fn == NULL)
    {
        *status = deflt ? rpc_s_ok : rpc_s_mgmt_op_disallowed;
        return (deflt);
    }
    else
    {
        if ((*authorization_fn) (binding_h, op, status))
        {
            *status = rpc_s_ok;     /* Be consistent */
            return (true);
        }
        else
        {
            *status = rpc_s_mgmt_op_disallowed;
            return (false);
        }
    }
}

/*
**++
**
**  ROUTINE NAME:       my_allocate
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Wrapper around RPC_MEM_ALLOC to use in call to
**  rpc_ss_swap_client_alloc_free.
**
**  INPUTS:
**
**      size            number of bytes to allocate
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     idl_void_p_t
**
**      return          pointer to allocated storage
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL idl_void_p_t my_allocate 
#ifdef _DCE_PROTO_
(
#ifdef SGIMIPS
idl_size_t		    size
#else
    unsigned long           size
#endif /* SGIMIPS */
)
#else
(size)
#ifdef SGIMIPS
idl_size_t		size;
#else
unsigned long           size;
#endif /* SGIMIPS */
#endif
{
    idl_void_p_t             ptr;


    RPC_MEM_ALLOC (
        ptr,
        idl_void_p_t,
        size,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    return (ptr);
}

/*
**++
**
**  ROUTINE NAME:       my_free
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Wrapper around RPC_MEM_FREE to use in call to
**  rpc_ss_swap_client_alloc_free.
**
**  INPUTS:
**
**      ptr             storage to free
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

INTERNAL void my_free 
#ifdef _DCE_PROTO_
(
    idl_void_p_t            ptr
)
#else
(ptr)
idl_void_p_t            ptr;
#endif
{
    RPC_MEM_FREE (ptr, RPC_C_MEM_STRING);
}


/*
**++
**
**  ROUTINE NAME:       remote_binding_validate
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Function to make sure a binding is sensible to use as a parameter to
**  one of the local/remote mgmt calls.  "Sensible" means (a) it's a
**  client binding, and (b) it has at least one of an object UUID or an
**  endpoint (so the call has a reasonable chance of making it to a real
**  server process).
**
**  INPUTS:
**
**      binding_h       RPC binding handle
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
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

INTERNAL void remote_binding_validate 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t binding_rep = (rpc_binding_rep_p_t) binding_h;


    RPC_BINDING_VALIDATE_CLIENT (binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    if ((! binding_rep->addr_has_endpoint) && UUID_IS_NIL (&binding_rep->obj, status))
    {
        *status = rpc_s_binding_incomplete;
        return;
    }

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_svr_princ_name_tgt
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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
PUBLIC void rpc_mgmt_inq_svr_princ_name_tgt
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              authn_protocol,
    unsigned_char_p_t       *server_princ_name,
    unsigned32              *tgt_length,
    unsigned_char_p_t       *tgt_data,
    unsigned32              *status
)
#else
(binding_h, authn_protocol, server_princ_name, tgt_length, tgt_data, status)
rpc_binding_handle_t    binding_h;
unsigned32              authn_protocol;
unsigned_char_p_t       *server_princ_name;
unsigned32              *tgt_length;
unsigned_char_p_t       *tgt_data;
unsigned32              *status;
#endif
{
    unsigned32          dce_rpc_authn_protocol;

    RPC_VERIFY_INIT ();

    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, status);

    dce_rpc_authn_protocol = 
        rpc_g_authn_protocol_id[authn_protocol].dce_rpc_authn_protocol_id;

    RPC_MEM_ALLOC (
        *server_princ_name,
        unsigned_char_p_t,
        MAX_SERVER_PRINC_NAME_LEN,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
#ifndef HPDCE_ALLOC_FAIL_DETECT
    if (*server_princ_name == NULL){
	*status = rpc_s_no_memory;
	return;
    }
#endif /* HPDCE_ALLOC_FAIL_DETECT */

    RPC_MEM_ALLOC (
        *tgt_data,
        unsigned_char_p_t,
        MAX_SERVER_TGT_LEN,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
#ifndef HPDCE_ALLOC_FAIL_DETECT
    if (*tgt_data == NULL){
	*status = rpc_s_no_memory;
	return;
    }
#endif /* HPDCE_ALLOC_FAIL_DETECT */

    /*
     * if this is a local request, just do it locally
     */
    if (binding_h == NULL)
    {
        rpc__auth_inq_my_princ_name_tgt( dce_rpc_authn_protocol,
            MAX_SERVER_PRINC_NAME_LEN, MAX_SERVER_TGT_LEN, *server_princ_name,
            tgt_length, *tgt_data, status);
    }
    else
    {
        remote_binding_validate(binding_h, status);
        if (*status != rpc_s_ok)
        {
            RPC_MEM_FREE (*server_princ_name, RPC_C_MEM_STRING);
            return;
        }

        /*
         * call the corresponding remote routine
         */
        (*mgmt_v1_0_c_epv.rpc__mgmt_inq_svr_name_tgt)
            (binding_h, 
             dce_rpc_authn_protocol,
             MAX_SERVER_PRINC_NAME_LEN,
             MAX_SERVER_TGT_LEN,
             *server_princ_name,
             tgt_length,
             *tgt_data,
             status);

        if (*status != rpc_s_ok)
        {
            RPC_MEM_FREE (*server_princ_name, RPC_C_MEM_STRING);
            if (*status == rpc_s_call_cancelled)
                pthread_cancel(pthread_self());
            return;
        }
    }
}


/*
**++
**
**  ROUTINE NAME:       inq_princ_name_tgt
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  This is a manager routine that provides a remote caller with the
**  principal name (really one of the principal names) and tgt for a server.
**
**  INPUTS:
**
**      binding_h       The binding handle for this remote call.
**
**      authn_proto     The *wire* authentication protocol ID we're
**                      interested in
**
**      princ_name_size The max size of princ_name
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      princ_name      Server's principal name
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

INTERNAL void inq_princ_name_tgt
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              authn_proto,
    unsigned32              princ_name_size,
    unsigned32              max_tgt_size,
    idl_char                princ_name[],
    unsigned32              *tgt_length,
    idl_char                *tgt_data,
    unsigned32              *status
)
#else
(binding_h, authn_proto, princ_name_size, max_tgt_size, princ_name, tgt_length,
 tgt_data, status)
rpc_binding_handle_t    binding_h;
unsigned32              authn_proto;
unsigned32              princ_name_size;
unsigned32              max_tgt_size;
idl_char                princ_name[];
unsigned32              *tgt_length;
idl_char                *tgt_data;
unsigned32              *status;
#endif
{
    if (! rpc__mgmt_authorization_check (binding_h, rpc_c_mgmt_inq_princ_name,
                               true, status))
    {
        princ_name[0] = '\0';
        *tgt_length=0;
        return;
    }

    rpc__auth_inq_my_princ_name_tgt (authn_proto, princ_name_size,
        max_tgt_size, (unsigned_char_p_t) princ_name, tgt_length, tgt_data,
        status);

    if (*status != rpc_s_ok) 
    {
        princ_name[0] = '\0';
        *tgt_length=0;
    }
}
