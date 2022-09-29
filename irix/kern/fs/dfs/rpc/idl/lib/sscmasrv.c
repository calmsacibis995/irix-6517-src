/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: sscmasrv.c,v 65.4 1998/03/23 17:25:34 gwehrman Exp $";
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
 * $Log: sscmasrv.c,v $
 * Revision 65.4  1998/03/23 17:25:34  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:04  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:12  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:43  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/17  23:58:42  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:52:51  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:32:43  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:08 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:16:49  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:49:46  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:36:29  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:58:21  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:26:10  bfc]
 * 
 * Revision 1.1.4.2  1993/07/07  20:11:23  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:39:47  ganni]
 * 
 * $EndLog$
 */
/*
**  @OSF_COPYRIGHT@
**
**  Copyright (c) 1990,1991,1993 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**
**  NAME:
**
**      sscmasrv.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      CMA machinery used by IDL server stub
**
**  VERSION: DCE 1.0
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>
#include <stdio.h>

#ifdef MIA
#include <dce/idlddefs.h>
#endif

#ifdef PERFMON
#include <dce/idl_log.h>
#endif
/******************************************************************************/
/*                                                                            */
/*    Set up CMA machinery required by server and client                      */
/*                                                                            */
/******************************************************************************/

#ifndef VMS
    ndr_boolean rpc_ss_server_is_set_up = ndr_false;
#endif

void rpc_ss_init_server_once(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{

#ifdef PERFMON
    RPC_SS_INIT_SERVER_ONCE_N;
#endif

    RPC_SS_THREADS_INIT;
    rpc_ss_init_client_once();
    rpc_ss_init_allocate_once();
#ifndef VMS
    rpc_ss_server_is_set_up = ndr_true;
#endif

#ifdef PERFMON
    RPC_SS_INIT_SERVER_ONCE_X;
#endif

}


/******************************************************************************/
/*                                                                            */
/*   Map an exception into a fault code and send a fault packet               */
/*  Old version - no user exceptions                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_send_server_exception
#ifdef IDL_PROTOTYPES
(
    rpc_call_handle_t h,
    EXCEPTION *e
)
#else
( h, e )
    rpc_call_handle_t h;
    EXCEPTION *e;
#endif
{
    ndr_ulong_int mapped_code;
    ndr_ulong_int fault_buff;
    rpc_mp_t mp;
    rpc_iovector_t iovec;
    error_status_t st;

#ifdef PERFMON
    RPC_SS_SEND_SERVER_EXCEPTION_N;
#endif

    iovec.num_elt = 1;
    iovec.elt[0].buff_dealloc = NULL;
    iovec.elt[0].flags = rpc_c_iovector_elt_reused;
    iovec.elt[0].buff_addr = (byte_p_t)&fault_buff;
    iovec.elt[0].buff_len = 4;
    iovec.elt[0].data_addr = (byte_p_t)&fault_buff;
    iovec.elt[0].data_len = 4;

    if ( RPC_SS_EXC_MATCHES( e, &rpc_x_invalid_tag ) )
        mapped_code = nca_s_fault_invalid_tag;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_invalid_bound ) )
        mapped_code = nca_s_fault_invalid_bound;
    else if ( RPC_SS_EXC_MATCHES( e, &RPC_SS_THREADS_X_CANCELLED ) )
        mapped_code = nca_s_fault_cancel;
#ifndef _KERNEL
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_fltdiv ) )
        mapped_code = nca_s_fault_fp_div_zero;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_fltovf ) )
        mapped_code = nca_s_fault_fp_overflow;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_aritherr ) )
        mapped_code = nca_s_fault_fp_error;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_fltund ) )
        mapped_code = nca_s_fault_fp_underflow;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_illaddr ) )
        mapped_code = nca_s_fault_addr_error;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_illinstr ) )
        mapped_code = nca_s_fault_ill_inst;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_intdiv ) )
        mapped_code = nca_s_fault_int_div_by_zero;
    else if ( RPC_SS_EXC_MATCHES( e, &exc_e_intovf ) )
        mapped_code = nca_s_fault_int_overflow;
#endif
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_no_memory ) )
        mapped_code = nca_s_fault_remote_no_memory;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_context_mismatch ) )
        mapped_code = nca_s_fault_context_mismatch;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_empty ) )
        mapped_code = nca_s_fault_pipe_empty;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_closed ) )
        mapped_code = nca_s_fault_pipe_closed;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_order ) )
        mapped_code = nca_s_fault_pipe_order;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_discipline_error ) )
        mapped_code = nca_s_fault_pipe_discipline;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_comm_error ) )
        mapped_code = nca_s_fault_pipe_comm_error;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_pipe_memory ) )
        mapped_code = nca_s_fault_pipe_memory;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_ss_remote_comm_failure ) )
        mapped_code = nca_s_fault_remote_comm_failure;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_object_not_found ) )
        mapped_code = nca_s_fault_object_not_found;
    else if ( RPC_SS_EXC_MATCHES( e, &rpc_x_no_client_stub ) )
        mapped_code = nca_s_fault_no_client_stub;
    else
        mapped_code = nca_s_fault_unspec;

    rpc_init_mp(mp, &fault_buff);
    rpc_marshall_ulong_int(mp, mapped_code);
    rpc_call_transmit_fault( h, &iovec, &st );

#ifdef PERFMON
    RPC_SS_SEND_SERVER_EXCEPTION_X;
#endif

}

#ifdef MIA
/******************************************************************************/
/*                                                                            */
/*   Map an exception into a fault code and send a fault packet               */
/*  New version - user exceptions                                             */
/*                                                                            */
/******************************************************************************/
void rpc_ss_send_server_exception_2
#ifdef IDL_PROTOTYPES
(
    rpc_call_handle_t h,
    EXCEPTION *e,
    idl_long_int num_user_exceptions,
    EXCEPTION *user_exception_pointers[],
    IDL_msp_t IDL_msp
)
#else
( h, e, num_user_exceptions, user_exception_pointers, IDL_msp )
    rpc_call_handle_t h;
    EXCEPTION *e;
    idl_long_int num_user_exceptions;
    EXCEPTION *user_exception_pointers[];
    IDL_msp_t IDL_msp;
#endif
{
    ndr_ulong_int mapped_code;
    ndr_ulong_int fault_buff[2];
    rpc_iovector_t iovec;
    error_status_t st;
    ndr_long_int i;
    rpc_mp_t mp;

    for (i=0; i<num_user_exceptions; i++)
    {
        if (RPC_SS_EXC_MATCHES(e, user_exception_pointers[i]))
        {
            mapped_code = nca_s_fault_user_defined;
            rpc_init_mp(mp, fault_buff);
            rpc_marshall_ulong_int(mp, mapped_code);
            rpc_advance_mp(mp, 4);
            rpc_marshall_ulong_int(mp, i);
            iovec.num_elt = 1;
            iovec.elt[0].buff_dealloc = NULL;
            iovec.elt[0].flags = rpc_c_iovector_elt_reused;
            iovec.elt[0].buff_addr = (byte_p_t)fault_buff;
            iovec.elt[0].buff_len = 8;
            iovec.elt[0].data_addr = (byte_p_t)fault_buff;
            iovec.elt[0].data_len = 8;
            rpc_call_transmit_fault( h, &iovec, &st );
            return;
        }
    }

    /* Exception did not match any user defined exception.
        Call the old (system exception) code */
    rpc_ss_send_server_exception( h, e );
}
#endif
