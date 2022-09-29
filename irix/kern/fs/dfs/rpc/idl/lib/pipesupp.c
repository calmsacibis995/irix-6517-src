/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: pipesupp.c,v 65.4 1998/03/23 17:24:45 gwehrman Exp $";
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
 * $Log: pipesupp.c,v $
 * Revision 65.4  1998/03/23 17:24:45  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:19:50  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:55:58  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/17  23:58:18  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:52:41  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:31:34  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:16:25  root]
 * 
 * Revision 1.1.6.1  1994/08/12  20:57:26  tom
 * 	Bug 10411 - Remove unsigned from pipe_number, next_{in,out}_pipe
 * 	  arguments for rpc_ss_initialize_callee_pipe and
 * 	  rpc_ss_mts_init_callee_pipe.
 * 	[1994/08/12  20:57:06  tom]
 * 
 * Revision 1.1.4.2  1993/07/07  20:10:31  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:39:10  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1990, 1991, 1992 by
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
**  NAME:
**
**      pipesupp.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Type independent routines to support pipes
**
**  MODIFICATION HISTORY:
**
**  13-Sep-91 rico          Change #include ordering
**  09-May-91 tbl           NIDL_PROTOTYPES => IDL_PROTOTYPES
**  07-May-91 tbl           Include lsysdep.h.
**  08-Apr-91 dineen        (stub => rpc_ss_ee)_pipe_state_t
**  12-feb-91 labossiere    ifdef stdio.h for kernel
**  28-Aug-90 A.I.Hinxman   Changed API
**  06-Mar-90 wen           status_ok=>error_status_ok
**  06-Feb-90 A.I.Hinxman   Original version.
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

#ifdef MIA
#include <dce/idlddefs.h>
#endif

void rpc_ss_initialize_callee_pipe
#ifdef IDL_PROTOTYPES
(
    long pipe_index,    /* Index of pipe in set of pipes in the
                            operation's parameter list */
    long next_in_pipe,     /* Index of next [in] pipe to process */
    long next_out_pipe,     /* Index of next [out] pipe to process */
    long *p_current_pipe,    /* Ptr to index num and dirn of curr active pipe */
    rpc_mp_t *p_mp,         /* Ptr to marshalling pointer */
    rpc_op_t *p_op,     /* Ptr to offset pointer */
    ndr_format_t src_drep,   /* Sender's data representation */
    rpc_iovector_elt_t *p_rcvd_data, /* Addr of received data descriptor */
    rpc_ss_mem_handle *p_mem_h,    /* Ptr to stub memory allocation handle */
    rpc_call_handle_t call_h,
    rpc_ss_ee_pipe_state_t **p_p_pipe_state,    /* Addr of ptr to pipe state block */
    error_status_t *st
)
#else
( pipe_index, next_in_pipe, next_out_pipe, p_current_pipe, p_mp, p_op, src_drep,
  p_rcvd_data, p_mem_h, call_h, p_p_pipe_state, st )
    long pipe_index;    /* Index of pipe in set of pipes in the
                            operation's parameter list */
    long next_in_pipe;     /* Index of next [in] pipe to process */
    long next_out_pipe;     /* Index of next [out] pipe to process */
    long *p_current_pipe;
    rpc_mp_t *p_mp;         /* Ptr to marshalling pointer */
    rpc_op_t *p_op;     /* Ptr to offset pointer */
    ndr_format_t src_drep;   /* Sender's data representation */
    rpc_iovector_elt_t *p_rcvd_data; /* Addr of received data descriptor */
    rpc_ss_mem_handle *p_mem_h;    /* Ptr to stub memory allocation handle */
    rpc_call_handle_t call_h;
    rpc_ss_ee_pipe_state_t **p_p_pipe_state;    /* Addr of ptr to pipe state block */
    error_status_t *st;
#endif
{
    rpc_ss_ee_pipe_state_t *p_pipe_state;

#ifdef PERFMON
    RPC_SS_INITIALIZE_CALLEE_PIPE_N;
#endif

    p_pipe_state = (rpc_ss_ee_pipe_state_t *)rpc_ss_mem_alloc(
                                    p_mem_h, sizeof(rpc_ss_ee_pipe_state_t));
    if (p_pipe_state == NULL)
    {
        RAISE(rpc_x_no_memory);
        return;
    }
    p_pipe_state->pipe_drained = ndr_false;
    p_pipe_state->pipe_filled = ndr_false;
    p_pipe_state->pipe_number = pipe_index;
    p_pipe_state->next_in_pipe = next_in_pipe;
    p_pipe_state->next_out_pipe = next_out_pipe;
    p_pipe_state->p_current_pipe = p_current_pipe;
    p_pipe_state->left_in_wire_array = 0;
    p_pipe_state->p_mp = p_mp;
    p_pipe_state->p_op = p_op;
    p_pipe_state->src_drep = src_drep;
    p_pipe_state->p_rcvd_data = p_rcvd_data;
    p_pipe_state->p_mem_h = p_mem_h;
    p_pipe_state->call_h = call_h;
    p_pipe_state->p_st = st;
    *p_p_pipe_state = p_pipe_state;
    *st = error_status_ok;

#ifdef PERFMON
    RPC_SS_INITIALIZE_CALLEE_PIPE_X;
#endif

}

#ifdef MIA

void rpc_ss_mts_init_callee_pipe
#ifdef IDL_PROTOTYPES
(
    long pipe_index,    /* Index of pipe in set of pipes in the
                            operation's parameter list */
    long next_in_pipe,     /* Index of next [in] pipe to process */
    long next_out_pipe,     /* Index of next [out] pipe to process */
    long *p_current_pipe,    /* Ptr to index num and dirn of curr active pipe */
    struct IDL_ms_t *IDL_msp,       /* Pointer to interpreter state block */
    unsigned long IDL_base_type_offset,  /* Offset of pipe base type definition
                                            in type vector */
    rpc_ss_mts_ee_pipe_state_t **p_p_pipe_state
                                           /* Addr of ptr to pipe state block */
)
#else
( pipe_index, next_in_pipe, next_out_pipe, p_current_pipe, IDL_msp,
  IDL_base_type_offset, p_p_pipe_state )
    long pipe_index;    /* Index of pipe in set of pipes in the
                            operation's parameter list */
    long next_in_pipe;     /* Index of next [in] pipe to process */
    long next_out_pipe;     /* Index of next [out] pipe to process */
    long *p_current_pipe;
    struct IDL_ms_t *IDL_msp;
    unsigned long IDL_base_type_offset;
    rpc_ss_mts_ee_pipe_state_t **p_p_pipe_state;
#endif
{
    rpc_ss_mts_ee_pipe_state_t *p_pipe_state;

#ifdef PERFMON
    RPC_SS_INITIALIZE_CALLEE_PIPE_N;
#endif

    p_pipe_state = (rpc_ss_mts_ee_pipe_state_t *)
                                rpc_ss_mem_alloc(&IDL_msp->IDL_mem_handle,
                                sizeof(rpc_ss_mts_ee_pipe_state_t));
    if (p_pipe_state == NULL)
    {
        RAISE(rpc_x_no_memory);
        return;
    }
    p_pipe_state->pipe_drained = ndr_false;
    p_pipe_state->pipe_filled = ndr_false;
    p_pipe_state->pipe_number = pipe_index;
    p_pipe_state->next_in_pipe = next_in_pipe;
    p_pipe_state->next_out_pipe = next_out_pipe;
    p_pipe_state->p_current_pipe = p_current_pipe;
    p_pipe_state->left_in_wire_array = 0;
    p_pipe_state->IDL_msp = IDL_msp;
    p_pipe_state->IDL_base_type_offset = IDL_base_type_offset;
    *p_p_pipe_state = p_pipe_state;

#ifdef PERFMON
    RPC_SS_INITIALIZE_CALLEE_PIPE_X;
#endif

}
#endif
