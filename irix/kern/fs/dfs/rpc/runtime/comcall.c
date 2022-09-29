/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comcall.c,v 65.5 1998/03/23 17:29:11 gwehrman Exp $";
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
 * $Log: comcall.c,v $
 * Revision 65.5  1998/03/23 17:29:11  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/26 19:04:53  lmc
 * Changes for sockets using behaviors.  Prototype and cast changes.
 *
 * Revision 65.3  1998/01/07  17:21:19  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:22  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.43.1  1996/05/10  13:09:38  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:12 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:41 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/5]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  18:59 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:30 UTC  jrr  /main/HPDCE02/gaz_dce_instr/jrr_1.2_mothra/1]
 *
 * 	fix server bytes-in counting logic
 * 	[1995/09/15  20:12 UTC  gaz  /main/HPDCE02/gaz_dce_instr/4]
 *
 * 	Ensure that DMS is NOT built in the Kernel RPC
 * 	[1995/08/17  04:18 UTC  gaz  /main/HPDCE02/gaz_dce_instr/3]
 *
 * 	correct DMS include filename
 * 	[1995/07/15  20:22 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
 *
 * 	count user bytes for the DMS
 * 	[1995/07/05  17:00 UTC  gaz  /main/HPDCE02/gaz_dce_instr/1]
 *
 * 	Merge allocation failure detection cleanup work
 * 	[1995/05/25  21:40 UTC  lmm  /main/HPDCE02/5]
 *
 * 	allocation failure detection cleanup
 * 	[1995/05/25  21:00 UTC  lmm  /main/HPDCE02/lmm_alloc_fail_clnup/1]
 *
 * 	Fixed DG awaiting_ack_timestamp.
 * 	[1995/04/19  17:07 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:13 UTC  lmm  /main/HPDCE02/3]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:18 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Added itimer.
 * 	[1995/01/25  20:33 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:17 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 *
 * Revision 1.1.39.1  1994/01/21  22:35:08  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:54:58  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  22:59:54  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:22  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:19:23  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:02  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:12:14  devrcs
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
**      comcall.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of the Call Services for the Common Communication
**  Services component. These routines are called by the stub to
**  perform the actual communications for an RPC. A call handle
**  is created, and is subsequently used to dispatch to the appropriate
**  communications protocol service.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime  */
#include <com.h>        /* Common communications services           */
#include <comprot.h>    /* Common protocol services                 */
#include <comnaf.h>     /* Common network address family services   */
#include <comp.h>       /* Private communications services          */

/*
**++
**
**  ROUTINE NAME:       rpc_call_start
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Begin a Remote Procedure Call. This is the first in a sequence of calls
**  by the client stub. It returns the information needed to marshal input
**  arguments. This routine is intended for use by the client stub only.
**
**  INPUTS:
**
**      binding_h       The binding handle which identifies the protocol
**                      stack to which the RPC is being made.
**
**      flags           The options to be applied to this RPC. One of these
**                      options must be set. They may not be ORed together.
**
**                      rpc_c_call_non_idempotent
**                      rpc_c_call_idempotent
**                      rpc_c_call_maybe
**                      rpc_c_call_brdcst
**
**      ifspec_h        The interface specification handle containing the
**                      interface UUID and version in which the RPC is
**                      contained. Also contains client stub supported
**                      transfer syntaxes.
**
**      opnum           The operation number in the interface the RPC
**                      corresponds to.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      xfer_syntax     The negotiated transfer syntax. The stub will use
**                      this to marshal the input arguments and unmarshal
**                      the output arguments of the RPC.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
**          rpc_s_coding_error
**          any of the RPC Protocol Service status codes
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      call_h          The handle which uniquely identifies this RPC.
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_call_start 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              flags,
    rpc_if_handle_t         ifspec_h,
    unsigned32              opnum,
    rpc_call_handle_t       *call_handle,
    rpc_transfer_syntax_t   *xfer_syntax,
    unsigned32              *status
)
#else
(binding_h, flags, ifspec_h, opnum, call_handle, xfer_syntax, status)
rpc_binding_handle_t    binding_h;
unsigned32              flags;
rpc_if_handle_t         ifspec_h;
unsigned32              opnum;
rpc_call_handle_t       *call_handle;
rpc_transfer_syntax_t   *xfer_syntax;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_call_rep_p_t        call_rep;
    boolean32		    itimer_was_running = false;

    RPC_LOG_CALL_START_NTR;
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();


    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
    {
        *call_handle = NULL;
        return;              
    }
    RPC_IF_VALIDATE((rpc_if_rep_p_t) ifspec_h, status);
    if (*status != rpc_s_ok)
    {
        *call_handle = NULL;
        return;
    }

    /*
     * Update rpc_g_is_single_threaded.
     */
    if (rpc_g_is_single_threaded)
    {
	RPC_LOCK(0);

	if (!(rpc_g_is_single_threaded = RPC_IS_SINGLE_THREADED(0)))
	{
	    if ((*status = rpc__timer_start()) == rpc_s_no_memory)
	    {
		RPC_UNLOCK(0);
		*call_handle = NULL;
		return;
	    }
	}
	else
	{
	    itimer_was_running = rpc__timer_itimer_stop();
	    rpc__clock_update();
	}

	RPC_UNLOCK(0);
    }

    /*
     * dispatch to the appropriate protocol service to get a call handle
     */
    call_rep = (*rpc_g_protocol_id[binding_rep->protocol_id].call_epv
        ->call_start)
            (binding_rep, flags, (rpc_if_rep_p_t) ifspec_h,
            opnum, xfer_syntax, status);

    if (*status == rpc_s_ok)
    {
        *call_handle = (rpc_call_handle_t) call_rep;

        /*
         * initialize the common part of the call handle
         */
        call_rep->protocol_id = binding_rep->protocol_id;
    }
    else if (rpc_g_is_single_threaded && itimer_was_running)
	rpc__timer_itimer_start();

    RPC_LOG_CALL_START_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_transmit
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Transmit a vector of marshaled arguments to the remote thread. Use the
**  call handle as the identifier of the RPC being performed. This routine
**  is intended for use by the client or server stub only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**      call_args       The marshaled RPC arguments being transmitted to the
**                      remote thread.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_coding_error
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_transmit 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    rpc_iovector_p_t        call_args,
    unsigned32              *status
)
#else
(call_h, call_args, status)
rpc_call_handle_t       call_h;
rpc_iovector_p_t        call_args;
unsigned32              *status;
#endif
{ 
    RPC_LOG_CALL_TRANSMIT_NTR;
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_transmit)
            ((rpc_call_rep_p_t) call_h, call_args, status);

    RPC_LOG_CALL_TRANSMIT_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_transceive
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Transmit a vector of marshaled arguments to the remote thread. Use the
**  call handle as the identifier of the RPC being performed. Block until
**  the first buffer of marshaled output arguments has been received. This
**  routine is intended for use by the client or server stub only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**      in_call_args    The marshaled RPC input arguments being transmitted
**                      to the remote thread.
**
**      out_call_args   The marshaled RPC output arguments received from the
**                      remote thread.
**
**      remote_ndr_fmt  The Network Data Representation format of the remote
**                      machine. This is used by the stub to unmarshal
**                      arguments encoded using NDR as the trasnfer syntax.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_transceive 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    rpc_iovector_p_t        in_call_args,
    rpc_iovector_elt_t      *out_call_args,
    ndr_format_t            *remote_ndr_fmt,
    unsigned32              *status
)
#else
(call_h, in_call_args, out_call_args, remote_ndr_fmt, status)
rpc_call_handle_t       call_h;
rpc_iovector_p_t        in_call_args;
rpc_iovector_elt_t      *out_call_args;
ndr_format_t            *remote_ndr_fmt;
unsigned32              *status;
#endif
{ 
    RPC_LOG_CALL_TRANSCEIVE_NTR;
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_transceive)
            ((rpc_call_rep_p_t) call_h, in_call_args,
            out_call_args, remote_ndr_fmt, status);

    RPC_LOG_CALL_TRANSCEIVE_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_receive
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Return a buffer of marshaled arguments from the remote thread. This
**  routine is intended for use by the client or server stub only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      call_args       The marshaled RPC arguments received from the
**                      remote thread.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_receive 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    rpc_iovector_elt_t      *call_args,
    unsigned32              *status
)
#else
(call_h, call_args, status)
rpc_call_handle_t       call_h;
rpc_iovector_elt_t      *call_args;
unsigned32              *status;
#endif
{
    RPC_LOG_CALL_RECEIVE_NTR;
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_receive)
            ((rpc_call_rep_p_t) call_h, call_args, status);

    RPC_LOG_CALL_RECEIVE_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_block_until_free
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will block until all marshaled RPC output arguments have
**  been transmitted and acknowledged. It is provided for use by the server
**  stub when the marshaled arguments are contained in buffers which are on
**  the stack.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_block_until_free 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    unsigned32              *status
)
#else
(call_h, status)
rpc_call_handle_t       call_h;
unsigned32              *status;
#endif
{ 
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_block_until_free) ((rpc_call_rep_p_t) call_h, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_cancel
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Forward a cancel to the remote RPC thread by the call handle
**  provided. This routine is intended for use by the client stub only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_cancel 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    unsigned32              *status
)
#else
(call_h, status)
rpc_call_handle_t       call_h;
unsigned32              *status;
#endif
{ 
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_cancel) ((rpc_call_rep_p_t) call_h, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_end
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  End a Remote Procedure Call. This is the last in a sequence of calls by
**  the client or server stub. This routine is intended for use by the
**  client stub only.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_coding_error
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_end 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       *call_h,
    unsigned32              *status
)
#else
(call_h, status)
rpc_call_handle_t       *call_h;
unsigned32              *status;
#endif
{ 
    RPC_LOG_CALL_END_NTR;
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (*call_h))->protocol_id].call_epv
        ->call_end) ((rpc_call_rep_p_t *) call_h, status);

    if (rpc_g_is_single_threaded)
	rpc__timer_itimer_start();
    RPC_LOG_CALL_END_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc_call_transmit_fault
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Forward an exception to the remote RPC thread identified by the call
**  handle. This routine is intended for use by the client or server stub
**  only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**      call_fault_info The marshaled fault information.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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

PUBLIC void rpc_call_transmit_fault 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    rpc_iovector_p_t        call_fault_info,
    unsigned32              *status
)
#else
(call_h, call_fault_info, status)
rpc_call_handle_t       call_h;
rpc_iovector_p_t        call_fault_info;
unsigned32              *status;
#endif
{ 
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_transmit_fault)
            ((rpc_call_rep_p_t) call_h, call_fault_info, status);
} 

/*
**++
**
**  ROUTINE NAME:       rpc_call_receive_fault
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Return a buffer of marshaled fault information from the remote thread. This
**  routine is intended for use by the client or server stub only.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      fault_info      The marshaled fault information received from the
**                      remote thread.
**
**      remote_ndr_fmt  The Network Data Representation format of the remote
**                      machine. This is used by the stub to unmarshal
**                      arguments encoded using NDR as the trasnfer syntax.
**
**
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
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
PUBLIC void rpc_call_receive_fault (
rpc_call_handle_t       call_h,
rpc_iovector_elt_t      *fault_info,
ndr_format_t            *remote_ndr_fmt,
unsigned32              *status)
#else
PUBLIC void rpc_call_receive_fault (call_h, fault_info, remote_ndr_fmt, status)

rpc_call_handle_t       call_h;
rpc_iovector_elt_t      *fault_info;
ndr_format_t            *remote_ndr_fmt;
unsigned32              *status;
#endif
        
{
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    (*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
        ->call_receive_fault)
            ((rpc_call_rep_p_t) call_h, fault_info, remote_ndr_fmt, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc_call_did_mgr_execute
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Return a boolean indicating whether the manager routine for the
**  RPC identified by the call handle has begun executing.
**
**  INPUTS:
**
**      call_h          The call handle which uniquely identifies this RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          any of the RPC Protocol Service status codes
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**                      true if the manager routine has begun
**                      executing, false otherwise.
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 rpc_call_did_mgr_execute 
#ifdef _DCE_PROTO_
(
    rpc_call_handle_t       call_h,
    unsigned32              *status
)
#else
(call_h, status)
rpc_call_handle_t       call_h;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    
    /*
     * dispatch to the appropriate protocol service
     */
    return ((*rpc_g_protocol_id[((rpc_call_rep_p_t) (call_h))->protocol_id].call_epv
             ->call_did_mgr_execute)
            ((rpc_call_rep_p_t) call_h, status));
}
