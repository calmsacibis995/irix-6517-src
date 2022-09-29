/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: eenodtbl.c,v 65.5 1998/03/23 17:25:21 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: eenodtbl.c,v $
 * Revision 65.5  1998/03/23 17:25:21  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:00  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:09  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:15:33  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.12.2  1996/02/18  18:53:32  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:06:23  marty]
 *
 * Revision 1.1.12.1  1995/12/07  22:26:00  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:06 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:14:25  root]
 * 
 * Revision 1.1.2.1  1995/11/01  14:22:18  bfc
 * 	idl cleanup
 * 	[1995/11/01  14:21:11  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/11/01  14:12:12  bfc]
 * 
 * Revision 1.1.8.1  1993/10/12  14:51:57  hinxman
 * 	OT 9083 Don't deallocate support pointers data structure if it doesn't exist
 * 	[1993/10/12  14:51:29  hinxman]
 * 
 * Revision 1.1.6.2  1993/07/07  20:06:07  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:36:20  ganni]
 * 
 * $EndLog$
 */
/*
    27-Oct-1992 A.I.Hinxman    OT5712 - memory leak with per-thread context on
                               client side
    29-Sep-1992 A.I.Hinxman     Add rpc_sm_... procedures
    08-Jun-1992 A.I.Hinxman     Fix memory leak in rpc_ss_create_support_ptrs
*/
/*
**
**  Copyright (c) 1990, 1992 by
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
**      eenodtbl.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Multi-threading support for callee nodes
**
**  VERSION: DCE 1.0
**
*/

#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

/******************************************************************************/
/*                                                                            */
/*    Fill in a support pointers structure and create an indirection          */
/*      for this thread                                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_build_indirection_struct
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs,
    rpc_ss_mem_handle *p_mem_handle,
    idl_boolean free_referents
)
#else
(p_thread_support_ptrs,p_mem_handle,free_referents)
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;
    rpc_ss_mem_handle *p_mem_handle;
    idl_boolean free_referents;
#endif
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;

    /* If a context exists, destroy it */
    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    if ( helper_thread_indirection_ptr != NULL )
    {
	 /* OT 13597 */
	 if (helper_thread_indirection_ptr->free_referents)
	 {
	      rpc_ss_thread_support_ptrs_t *old_thread_support_ptrs;
	      
	      old_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
	      
	      /* Release any memory owned by the memory handle */
	      rpc_ss_mem_free( old_thread_support_ptrs->p_mem_h );
	      
	      /*
	       *  Free the objects it points at.
	       *  Must cast because instance_of
	       *  (rpc_ss_thread_support_ptrs_t).p_mem_h
	       *  is of type rpc_mem_handle, which is a pointer to volatile,
	       *  and free() doesn't take a pointer to volatile.
	       */
	      free( (idl_void_p_t)old_thread_support_ptrs->p_mem_h );
	      RPC_SS_THREADS_MUTEX_DELETE( &(old_thread_support_ptrs->mutex) );
	      
	      /* Free the structure */
	      free( old_thread_support_ptrs );
	 } /* End of OT 13597 */
	 
        free( helper_thread_indirection_ptr );
    }

    RPC_SS_THREADS_MUTEX_CREATE(&(p_thread_support_ptrs->mutex));
    p_thread_support_ptrs->p_mem_h = p_mem_handle;
    p_thread_support_ptrs->p_allocate = rpc_ss_allocate;
    p_thread_support_ptrs->p_free = rpc_ss_free;

    helper_thread_indirection_ptr = (rpc_ss_thread_indirection_t *)
                            malloc(sizeof(rpc_ss_thread_indirection_t));
    helper_thread_indirection_ptr->indirection = p_thread_support_ptrs;
    helper_thread_indirection_ptr->free_referents = free_referents;
    RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key,
                                       helper_thread_indirection_ptr );
}

/******************************************************************************/
/*                                                                            */
/*    Create a support pointers structure for this thread                     */
/*      Only called from server stub                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_create_support_ptrs
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs,
    rpc_ss_mem_handle *p_mem_handle
)
#else
(p_thread_support_ptrs,p_mem_handle)
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;
    rpc_ss_mem_handle *p_mem_handle;
#endif
{
    rpc_ss_build_indirection_struct(p_thread_support_ptrs, p_mem_handle,
                                    idl_false);
}

/******************************************************************************/
/*                                                                            */
/*    Return the address of the support pointers structure for this thread    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_get_support_ptrs
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_support_ptrs_t **p_p_thread_support_ptrs
)
#else
(p_p_thread_support_ptrs )
    rpc_ss_thread_support_ptrs_t **p_p_thread_support_ptrs;
#endif
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;

    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    *p_p_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
}

/******************************************************************************/
/*                                                                            */
/*    Destroy the support pointers structure for this thread                  */
/*      Only called from server stub                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_destroy_support_ptrs(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;

    /* Destroy the mutex that the context points at */
    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    if (helper_thread_indirection_ptr == NULL)
        return;
    p_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
    RPC_SS_THREADS_MUTEX_DELETE( &(p_thread_support_ptrs->mutex) );
    /* There is no need to delete the support_ptrs structure as it is
        on the server stub stack */

    /* free the ptr context storage */
    free( helper_thread_indirection_ptr );

    /* And destroy the context - this is required for Kernel RPC */
    RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key, NULL );
}

/*
 *  The rpc_sm_... routines have the same functionality as the corresponding
 *  rpc_ss_... routines but have an extra, error_status_t, parameter, which
 *  is used instead of exceptions for error reporting.
 */

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_allocate                                                         */
/*                                                                            */
/******************************************************************************/
idl_void_p_t rpc_sm_allocate
#ifdef IDL_PROTOTYPES
(
    idl_size_t size,
    error_status_t *p_st
)
#else
( size, p_st )
    idl_size_t size;
    error_status_t *p_st;
#endif
{
    idl_void_p_t result;

    *p_st = error_status_ok;
    TRY
        result = rpc_ss_allocate(size);
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
    return result;
}

/****************************************************************************/
/*                                                                          */
/* rpc_sm_client_free                                                       */
/*                                                                          */
/****************************************************************************/
void rpc_sm_client_free
#ifdef IDL_PROTOTYPES
(
    rpc_void_p_t p_mem,
    error_status_t *p_st
)
#else
( p_mem, p_st )
    rpc_void_p_t p_mem;
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    rpc_ss_client_free(p_mem);
}

/******************************************************************************/
/*                                                                            */
/* Function to be called by user to release unusable context_handle           */
/*                                                                            */
/******************************************************************************/
void rpc_sm_destroy_client_context
#ifdef IDL_PROTOTYPES
(
    rpc_ss_context_t *p_unusable_context_handle,
    error_status_t *p_st
)
#else
( p_unusable_context_handle, p_st )
    rpc_ss_context_t *p_unusable_context_handle;
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    TRY
        rpc_ss_destroy_client_context(p_unusable_context_handle);
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_disable_allocate                                                 */
/*                                                                            */
/******************************************************************************/
void rpc_sm_disable_allocate
#ifdef IDL_PROTOTYPES
(error_status_t *p_st )
#else
(p_st)
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    rpc_ss_disable_allocate();
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_enable_allocate                                                  */
/*                                                                            */
/******************************************************************************/
void rpc_sm_enable_allocate
#ifdef IDL_PROTOTYPES
(error_status_t *p_st)
#else
(p_st)
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    TRY
        rpc_ss_enable_allocate();
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_free                                                             */
/*                                                                            */
/******************************************************************************/
void rpc_sm_free
#ifdef IDL_PROTOTYPES
(
    rpc_void_p_t node_to_free,
    error_status_t *p_st
)
#else
(node_to_free, p_st)
    rpc_void_p_t node_to_free;
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    rpc_ss_free(node_to_free);
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_get_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
rpc_ss_thread_handle_t rpc_sm_get_thread_handle
#ifdef IDL_PROTOTYPES
( error_status_t *p_st )
#else
(p_st)
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    return rpc_ss_get_thread_handle();
}

/******************************************************************************/
/*                                                                            */
/*    Create thread context with references to named alloc and free rtns      */
/*                                                                            */
/******************************************************************************/
void rpc_sm_set_client_alloc_free
#ifdef IDL_PROTOTYPES
(
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate)(
        idl_size_t size
    ),
    void (IDL_ALLOC_ENTRY *p_free)(
        rpc_void_p_t ptr
    ),
    error_status_t *p_st
)
#else
( p_allocate, p_free, p_st )
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate)();
    void (IDL_ALLOC_ENTRY *p_free)();
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    TRY
        rpc_ss_set_client_alloc_free(p_allocate, p_free);
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_set_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
void rpc_sm_set_thread_handle
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_handle_t thread_handle,
    error_status_t *p_st
)
#else
( thread_handle, p_st )
    rpc_ss_thread_handle_t thread_handle;
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    TRY
        rpc_ss_set_thread_handle(thread_handle);
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    Get the existing allocate, free routines and replace them with new ones */
/*                                                                            */
/******************************************************************************/
void rpc_sm_swap_client_alloc_free
#ifdef IDL_PROTOTYPES
(
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate)(
        idl_size_t size
    ),
    void (IDL_ALLOC_ENTRY *p_free)(
        rpc_void_p_t ptr
    ),
    rpc_void_p_t (IDL_ALLOC_ENTRY **p_p_old_allocate)(
        idl_size_t size
    ),
    void (IDL_ALLOC_ENTRY **p_p_old_free)(
        rpc_void_p_t ptr
    ),
    error_status_t *p_st
)
#else
( p_allocate, p_free, p_p_old_allocate, p_p_old_free, p_st )
    rpc_void_p_t (IDL_ALLOC_ENTRY *p_allocate)();
    void         (IDL_ALLOC_ENTRY *p_free)();
    rpc_void_p_t (IDL_ALLOC_ENTRY **p_p_old_allocate)();
    void         (IDL_ALLOC_ENTRY **p_p_old_free)();
    error_status_t *p_st;
#endif
{
    *p_st = error_status_ok;
    TRY
        rpc_ss_swap_client_alloc_free(p_allocate, p_free, p_p_old_allocate,
                                                                p_p_old_free);
    CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    ENDTRY
}
