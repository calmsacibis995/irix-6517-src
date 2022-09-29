/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ctxerrtl.c,v 65.4 1998/03/23 17:25:37 gwehrman Exp $";
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
 * $Log: ctxerrtl.c,v $
 * Revision 65.4  1998/03/23 17:25:37  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:20:04  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:13  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  18:53:02  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:06:08  marty]
 *
 * Revision 1.1.8.1  1995/12/07  22:24:33  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:06 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:13:54  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:49:00  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:35:51  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:56:57  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:21  bfc]
 * 
 * Revision 1.1.4.2  1993/07/07  20:05:00  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:37  ganni]
 * 
 * $EndLog$
 */
/*
**  @OSF_COPYRIGHT@
**
**  Copyright (c) 1990 by
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
**      ctxerrtl.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Runtime support for caller context
**
**  MODIFICATION HISTORY:
**
**  13-Sep-91 Annicchiarico Change #include ordering
**  09-Jul-91 J.Harrow      Make to_wire() header match stubbase.h (add volatile).
**  26-Jun-91 A.I.Hinxman   Remove [out_of_line] routines for ndr_context_handle
**  20-May-91 hinxman   Un/marshall context handles by routine
**  10-May-91 tbl           Fix _to_wire() header.
**                          Cast status args from volatile where necessary.
**  09-May-91 tbl           Fix rpc_ss_er_ctx_from_wire() header.
**  07-May-91 tbl           Include lsysdep.h.
**  22-Apr-91 A.I.Hinxman   Add rpc_ss_destroy_client_context
**  02-Apr-91 Annicchiarico Put in context handle debugging support
**  04-Mar-91 A.I.Hinxman   Remove redundant "result" local
**  12-feb-91 labossiere    ifdef stdio.h for kernel
**  20-Jul-90 A.I.Hinxman   maintain liveness on copied handle
**  04-Jul-90 wen           wire_to_ctx=>ctx_from_wire
**  24-Apr-90 A.I.Hinxman   Major rework
**  06-Mar-90 wen           status_ok=>error_status_ok
**  23-Feb-90 A.I.Hinxman   Original version.
**
*/

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef DEBUGCTX
#  include <stdio.h>
#endif

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

/* On the caller side a context handle is a pointer into the stub's data space.
    The object pointed to contains a 0 attributes word, a UUID and a handle_t */

/*****************************************************************************/
/*                                                                           */
/* Uses  malloc  to create a  context_element_t  containing the              */
/* wire rep of the context handle and the binding handle                     */
/*  of the association. Makes the caller's context handle point at           */
/* the created object. Starts liveness maintenance                           */
/*                                                                           */
/*****************************************************************************/
static void rpc_ss_create_caller_context
#ifdef IDL_PROTOTYPES
(
    ndr_context_handle *p_wire_context,
              /* Pointer to the wire representation of the context_handle */
    handle_t caller_handle,    /* Binding handle */
    rpc_ss_context_t *p_caller_context, /* Pointer to caller's context handle */
    error_status_t *p_st
)
#else
( p_wire_context, caller_handle, p_caller_context,p_st )
    ndr_context_handle *p_wire_context;
    handle_t caller_handle;    /* Handle on which the call was made */
    rpc_ss_context_t *p_caller_context;/* Pointer to caller's context handle */
    error_status_t *p_st;
#endif
{
    rpc_ss_caller_context_element_t *p_created_context;

#ifdef PERFMON
    RPC_SS_CREATE_CALLER_CONTEXT_N;
#endif

    p_created_context = (rpc_ss_caller_context_element_t *)
                       malloc(sizeof(rpc_ss_caller_context_element_t));
    if (p_created_context == NULL)
    {

#ifdef PERFMON
    RPC_SS_CREATE_CALLER_CONTEXT_X;
#endif

        RAISE( rpc_x_no_memory );
        return;
    }

    p_created_context->context_on_wire.context_handle_attributes
        = p_wire_context->context_handle_attributes;
    memcpy(
            (char *)&p_created_context->context_on_wire.context_handle_uuid,
            (char *)&p_wire_context->context_handle_uuid,
                        sizeof(uuid_t));

    rpc_binding_copy(caller_handle, &p_created_context->using_handle, p_st);
    if (*p_st != error_status_ok) return;
    *p_caller_context = (rpc_ss_context_t)p_created_context;
    rpc_network_maintain_liveness(p_created_context->using_handle, p_st);
#ifdef PERFMON
    RPC_SS_CREATE_CALLER_CONTEXT_X;
#endif

}

/******************************************************************************/
/*                                                                            */
/*    Convert wire form of context handle to local form and update the stub's */
/*    internal tables                                                         */
/*                                                                            */
/******************************************************************************/
void rpc_ss_er_ctx_from_wire
#ifdef IDL_PROTOTYPES
(
    ndr_context_handle      *p_wire_context,
    rpc_ss_context_t        *p_caller_context, /* Pointer to application context */
    handle_t                caller_handle,     /* Binding handle */
    ndr_boolean             in_out,            /* TRUE for [in,out], FALSE for [out] */
    volatile error_status_t *p_st
)
#else
(p_wire_context, p_caller_context, caller_handle, in_out, p_st)
    ndr_context_handle *p_wire_context;
    rpc_ss_context_t   *p_caller_context;
    handle_t           caller_handle;
    ndr_boolean        in_out;
    error_status_t     *p_st;
#endif
{
#ifdef DEBUGCTX
    debug_context_uuid(&p_wire_context->context_handle_uuid, "N");
#endif

#ifdef PERFMON
    RPC_SS_ER_CTX_FROM_WIRE_N;
#endif

    if (in_out) {
        if (
            uuid_is_nil(
                &p_wire_context->context_handle_uuid, (error_status_t *)p_st
            )
        ) {
            /* Context is now NIL */
            if (*p_caller_context != NULL) {
                /* If it wasn't NIL previously, stop monitoring it */
                rpc_network_stop_maintaining(
                    caller_handle, (error_status_t *)p_st
                );
                rpc_binding_free(
                     &((rpc_ss_caller_context_element_t *)*p_caller_context)
                        ->using_handle, (error_status_t *)p_st
                );
                /* Now release it */
                free((byte_p_t)*p_caller_context);
                *p_caller_context = NULL;

#ifdef PERFMON
                RPC_SS_ER_CTX_FROM_WIRE_X;
#endif

                return;
            }
        }
        else {  /* Returned context is not NIL */
            if (*p_caller_context != NULL) {
                /* And it wasn't NIL before the call */
                if (
                    ! uuid_equal(
                        &p_wire_context->context_handle_uuid,
                        &((rpc_ss_caller_context_element_t *)*p_caller_context)
                            ->context_on_wire.context_handle_uuid,
                        (error_status_t *)p_st
                    )
                )
                    RAISE( rpc_x_ss_context_damaged );
            }
            else {
                /* This is a new context */
                rpc_ss_create_caller_context(
                    p_wire_context, caller_handle,
                    p_caller_context, (error_status_t *)p_st
                );
            }
        }
    }
    else {  /* Handling an OUT parameter */
        if (
            uuid_is_nil(
                &p_wire_context->context_handle_uuid, (error_status_t *)p_st
            )
        )
            *p_caller_context = NULL; /* A NIL context was returned */
        else
            rpc_ss_create_caller_context(
                p_wire_context, caller_handle,
                p_caller_context, (error_status_t *)p_st
            );
    }

#ifdef PERFMON
    RPC_SS_ER_CTX_FROM_WIRE_X;
#endif

    return;
}

/******************************************************************************/
/*                                                                            */
/*    This routine converts a caller's context handle to wire format          */
/*    This routine is only called for an IN or IN OUT  context_t   parameter  */
/*                                                                            */
/******************************************************************************/
void rpc_ss_er_ctx_to_wire
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t   caller_context,  /* The context handle the caller is using */
    ndr_context_handle *p_wire_context, /* Where to put data to be marshalled */
    handle_t           assoc_handle,    /* Handle on which the call will be made */
    ndr_boolean        in_out,          /* TRUE for [in,out] param, FALSE for [in] */
    volatile error_status_t     *p_st
)
#else
(caller_context, p_wire_context, assoc_handle, in_out, p_st)
    rpc_ss_context_t   caller_context;
    ndr_context_handle *p_wire_context;
    handle_t           assoc_handle;
    ndr_boolean        in_out;
    volatile error_status_t     *p_st;
#endif
{

#ifdef PERFMON
    RPC_SS_ER_CTX_TO_WIRE_N;
#endif

    *p_st = error_status_ok;
    if (caller_context != NULL)
    {
        memcpy(
         (char *)p_wire_context,
         (char *)
          &((rpc_ss_caller_context_element_t *)caller_context)->context_on_wire,
             sizeof(ndr_context_handle) );
    }
    else
    {
        if ( in_out )
        {
            /* No active context. Send callee a NIL UUID */
            p_wire_context->context_handle_attributes = 0;
            uuid_create_nil(&p_wire_context->context_handle_uuid,(unsigned32*)p_st);
        }
        else
        {
            RAISE( rpc_x_ss_in_null_context );
        }
    }

#ifdef DEBUGCTX
    debug_context_uuid(&p_wire_context->context_handle_uuid, "L");
#endif

#ifdef PERFMON
    RPC_SS_ER_CTX_TO_WIRE_X;
#endif

}

#ifdef DEBUGCTX
static ndr_boolean debug_file_open = ndr_false;
static char *debug_file = "ctxer.dmp";
static FILE *debug_fid;

static int debug_context_uuid(uuid_p, prefix)
    unsigned char *uuid_p;
    char *prefix;
{
    int j;
    unsigned long k;

    if (!debug_file_open)
    {
        debug_fid = fopen(debug_file, "w");
        debug_file_open = ndr_true;
    }

    fprintf(debug_fid, prefix);
    for (j=0; j<sizeof(uuid_t); j++)
    {
        k = *uuid_p++;
        fprintf(debug_fid, " %02x", k);
    }
    fprintf(debug_fid, "\n");
}
#endif

/******************************************************************************/
/*                                                                            */
/* Function to be called by user to release unusable context_handle           */
/*                                                                            */
/******************************************************************************/
void rpc_ss_destroy_client_context
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t *p_unusable_context_handle
)
#else
( p_unusable_context_handle )
    rpc_ss_context_t *p_unusable_context_handle;
#endif
{

#ifdef PERFMON
    RPC_SS_DESTROY_CLIENT_CONTEXT_N;
#endif

    free( *p_unusable_context_handle );
    *p_unusable_context_handle = NULL;

#ifdef PERFMON
    RPC_SS_DESTROY_CLIENT_CONTEXT_X;
#endif

}

