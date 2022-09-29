/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: alfrsupp.c,v 65.5 1998/03/23 17:25:23 gwehrman Exp $";
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
 * $Log: alfrsupp.c,v $
 * Revision 65.5  1998/03/23 17:25:23  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:01  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:10  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:15:30  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.12.2  1996/02/18  18:52:45  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:05:54  marty]
 *
 * Revision 1.1.12.1  1995/12/07  22:23:44  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:13:38  root]
 * 
 * Revision 1.1.14.1  1996/10/14  17:56:32  mak
 * 	 OT 13597  ptrs are not freed when per-thread context destroyed
 * 	[1996/10/14  17:56:14  mak]
 *
 * Revision 1.1.8.1  1994/08/09  15:32:09  rico
 * 	Added volatile to the argument IDL_ms_t * in the function definition of
 * 	rpc_ss_mts_client_estab_alloc().
 * 	[1994/08/09  15:29:21  rico]
 * 
 * Revision 1.1.6.2  1993/07/07  20:04:21  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:11  ganni]
 * 
 * $EndLog$
 */
/*
** 27-Oct-92 hinxman    OT5712 - memory leak with per-thread context on client
**                      side
** 25-Mar-92 harrow     Add rpc_ss_client_free() and make use of idl_size_t
**                      which matches size_t argument to malloc().
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
**
**  NAME:
**
**      alfrsupp.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Support routines for rpc_ss_allocate, rpc_ss_free
**
**  VERSION: DCE 1.0
*/

#include <dce/rpc.h>
#include <dce/stubbase.h>
#ifdef MIA
#include <dce/idlddefs.h>
#endif
#include <lsysdep.h>

/*
1. Explanation of the per-thread context
   -------------------------------------

    The per-thread context enables a group of threads to share the same stub
memory management mechanism.

    Per-thread context is built from an rpc_ss_thread_indirection_t
    structure, which consists of
        indirection     (pointer to a rpc_ss_thread_support_ptrs_t)
        free_referents  (indicates whether the referents of indirection must
                            be freed when the thread terminates)
    and a rpc_ss_thread_support_ptrs_t structure consisting of
        mutex       (to prevent threads in the group accessing the mechanism
                        simultaneously)
        p_mem_h     (pointer to the memory handle used if the rpc_ss_allocate/
                        rpc_ss_free machinery is in use)
        p_allocate  (pointer to the memory allocator the group is using)
        p_free      (pointer to memory release function compatible with
                        p_allocate)
    The context information registered with Pthreads is the address of a
rpc_ss_thread_indirection_t structure and is denoted by
helper_thread_indirection_pointer.

1.1 Use of the context in a server thread
    -------------------------------------

    The memory management mechanism is set up by a server stub if the
operation includes pointers in its parameters, or the ACF [enable_allocate]
attribute has been used. It is set up by the routine rpc_ss_create_support_ptrs
(eenodtbl.c). p_mem_h points into the server stub stack. Memory attached to
this handle is released before the server stub returns control to the RPC
runtime. After this memory has been released, rpc_ss_destroy_support_pointers
(eenodtbl.c) destroys the mutex in the rpc_ss_thread_support_ptrs_t structure,
releases the rpc_ss_thread_indirection_t structure and explicitly unregisters
the context with Pthreads. The rpc_ss_thread_support_ptrs_t structure the
indirection pointer was pointing at is on the server stub's stack, and so will
be released automatically when control returns to the RPC runtime.
    The routines rpc_ss_allocate, rpc_ss_free, which are the normal machinery
used by manager application code, for allocating and releasing memory, need to
access the memory handle set up in the server stub. They do this by calling
rpc_ss_get_support_ptrs (eenodtbl.c) to obtain the value of the indirection
pointer.


1.2 Use of the context in manager helper threads
    --------------------------------------------

    A manager helper thread is spawned by manager application code, and so
belongs to the same group as the server thread from which the manager
application code was entered. All the manager helper threads in a group need
to access the same mutex and memory handle. The application achieves this by
calling rpc_ss_get_thread_handle (alfrsupp.c) to get the value of the
indirection pointer for the group from an existing thread.
    A new thread uses rpc_ss_set_thread_handle (alfrsupp.c) to create a new cell
containing this value and register the address of this new cell as the context
for this thread.
    When a manager helper thread teminates, Pthreads invokes the destructor,
rpc_ss_destroy_thread_ctx (alfrsupp.c) for the context. This frees the
rpc_ss_thread_indirection_t structure, but takes no other action, because the
data structure pointed at by the indirection pointer is pointed at by at least
one other cell, the context for the server stub's thread.


1.3 Use of the context with rpc_ss_enable_allocate, rpc_ss_disable_allocate
    -----------------------------------------------------------------------

    This is usage on the client side, i.e. when client code that is not part
of a manager is being executed. rpc_ss_enable_allocate (alfrsupp.c) is called
by the client to establish an environment in which rpc_ss_allocate, rpc_ss_free
can be used.
    rpc_ss_enable_allocate creates a memory handle on the heap. It then creates
a rpc_ss_thread_support_ptrs_t structure on the heap which includes a pointer
to the memory handle. Then it calls rpc_ss_create_support_ptrs to complete the
required data structures.
    When the client no longer needs this machinery it calls
rpc_ss_disable_allocate (alfrsupp.c). This releases any memory owned by the
memory handle, releases the rpc_ss_thread_support_ptrs_t structure and the
rpc_ss_thread_indirection_t structure and unregisters the context with Pthreads.


1.4 Other client side uses of the context
    -------------------------------------

    These arise when rpc_ss_enable_allocate has not been called and one of the
following events occurs:
i)      A client stub for an operation whose [out] parameters include a pointer
    is executed. In the OSF compiler this results in a call to the routine
    rpc_ss_client_establish_alloc (alfrsupp.c).
ii)     Application code calls rpc_ss_set_client_alloc_free (alfrsupp.c).
iii)    Application code calls rpc_ss_swap_client_alloc_free (alfrsupp.c).
    In any of these cases, all the data structures established by
rpc_ss_enable_allocate have been created, although their fields may have
different values. If a thread in which one of these events has occurred
terminates, the same set of memory releases that would be performed by
rpc_ss_disable_allocate occurs.

*/

#ifdef STUBS_USE_PTHREADS
typedef void (*destructor_t)
(
#ifdef IDL_PROTOTYPES
    rpc_ss_threads_dest_arg_t arg
#endif
);
#else
    typedef cma_t_destructor destructor_t;
#endif

/******************************************************************************/
/*                                                                            */
/*    Set up CMA machinery required by rpc_ss_allocate, rpc_ss_free           */
/*                                                                            */
/******************************************************************************/
#ifndef VMS
    ndr_boolean rpc_ss_allocate_is_set_up = ndr_false;
#endif

static RPC_SS_THREADS_ONCE_T allocate_once = RPC_SS_THREADS_ONCE_INIT;

RPC_SS_THREADS_KEY_T rpc_ss_thread_supp_key;

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_destroy_thread_ctx                                               */
/*    Destroy client per thread context.                                      */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_destroy_thread_ctx
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_indirection_t *thread_indirection_ptr
)
#else
( thread_indirection_ptr )
    rpc_ss_thread_indirection_t *thread_indirection_ptr;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;

    if (thread_indirection_ptr != NULL)
    {
        if (thread_indirection_ptr->free_referents)
        {
            p_thread_support_ptrs = thread_indirection_ptr->indirection;

            /* Release any memory owned by the memory handle */
            rpc_ss_mem_free( p_thread_support_ptrs->p_mem_h );

            /*
             *  Free the objects it points at.
             *  Must cast because instance_of
             *  (rpc_ss_thread_support_ptrs_t).p_mem_h
             *  is of type rpc_mem_handle, which is a pointer to volatile,
             *  and free() doesn't take a pointer to volatile.
             */
            free( (idl_void_p_t)p_thread_support_ptrs->p_mem_h );
            RPC_SS_THREADS_MUTEX_DELETE( &(p_thread_support_ptrs->mutex) );

            /* Free the structure */
            free( p_thread_support_ptrs );
        }

        /* Free the indirection storage */
        free( thread_indirection_ptr );

        /* And destroy the context - this is required for Kernel RPC */
        RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key, NULL );
    }
}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_thread_ctx_destructor                                            */
/*    Destroy per thread context at thread termination                        */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_thread_ctx_destructor
#ifdef IDL_PROTOTYPES
(
    rpc_ss_threads_dest_arg_t arg
)
#else
( arg )
    rpc_ss_threads_dest_arg_t arg;
#endif
{
    rpc_ss_thread_indirection_t *thread_indirection_ptr =
        (rpc_ss_thread_indirection_t *) arg;

    rpc_ss_destroy_thread_ctx( thread_indirection_ptr );
}

static void rpc_ss_init_allocate(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    /* Key for thread local storage for tree management */
    RPC_SS_THREADS_KEY_CREATE( &rpc_ss_thread_supp_key,
                    (destructor_t)rpc_ss_thread_ctx_destructor );
}

void rpc_ss_init_allocate_once(
#ifdef IDL_PROTOTYPES
    void
#endif
)
{
    RPC_SS_THREADS_INIT;
    RPC_SS_THREADS_ONCE( &allocate_once, rpc_ss_init_allocate );
#ifndef VMS
    rpc_ss_allocate_is_set_up = ndr_true;
#endif
}

/******************************************************************************/
/*                                                                            */
/*    Do we currently have thread context data?                               */
/*    If not, create local storage with malloc() and free() as the            */
/*      allocate and free routines                                            */
/*                                                                            */
/******************************************************************************/
static void rpc_ss_client_get_thread_ctx
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_support_ptrs_t **p_p_support_ptrs
)
#else
( p_p_support_ptrs)
    rpc_ss_thread_support_ptrs_t **p_p_support_ptrs;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;
    rpc_ss_thread_indirection_t *thread_indirection_ptr;

#ifndef MEMORY_NOT_WRITTEN_SERIALLY
    if ( ! rpc_ss_allocate_is_set_up )
#endif
        rpc_ss_init_allocate_once();

    p_support_ptrs = (rpc_ss_thread_support_ptrs_t *)rpc_ss_get_thread_handle();
    if (p_support_ptrs == NULL)
    {
        /* We have no context. Make one with the fields we need */
        p_support_ptrs = (rpc_ss_thread_support_ptrs_t *)
                                   malloc(sizeof(rpc_ss_thread_support_ptrs_t));
        if (p_support_ptrs == NULL)
        {
            RAISE( rpc_x_no_memory );
        }

        p_support_ptrs->p_mem_h = (rpc_ss_mem_handle *)
                            malloc(sizeof(rpc_ss_mem_handle));
        if (p_support_ptrs->p_mem_h == NULL)
        {
            RAISE( rpc_x_no_memory );
        }
        p_support_ptrs->p_mem_h->memory = NULL;
        p_support_ptrs->p_mem_h->node_table = NULL;

        RPC_SS_THREADS_MUTEX_CREATE (&(p_support_ptrs->mutex));

        p_support_ptrs->p_allocate = (idl_void_p_t (*)(
#       ifdef IDL_PROTOTYPES
            idl_size_t size
#       endif
            ))malloc;

        p_support_ptrs->p_free = (void (*)(
#       ifdef IDL_PROTOTYPES
            idl_void_p_t ptr
#       endif
            ))free;

        thread_indirection_ptr = (rpc_ss_thread_indirection_t *)
                        malloc(sizeof(rpc_ss_thread_indirection_t));
        if (thread_indirection_ptr == NULL)
        {
            RAISE( rpc_x_no_memory );
        }
        thread_indirection_ptr->indirection = p_support_ptrs;
        thread_indirection_ptr->free_referents = idl_true;
        RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key,
                                           thread_indirection_ptr );
    }
    *p_p_support_ptrs = p_support_ptrs;
}

/******************************************************************************/
/*                                                                            */
/*    Do we currently have thread context data?                               */
/*    If not, create local storage with malloc() and free() as the            */
/*      allocate and free routines                                            */
/*    Copy pointers to allocate and free routines to local storage            */
/*                                                                            */
/******************************************************************************/
void rpc_ss_client_establish_alloc
#ifdef IDL_PROTOTYPES
(
    rpc_ss_marsh_state_t *p_unmar_params
)
#else
( p_unmar_params )
    rpc_ss_marsh_state_t *p_unmar_params;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

    rpc_ss_client_get_thread_ctx( &p_support_ptrs );
    p_unmar_params->p_allocate = p_support_ptrs->p_allocate;
    p_unmar_params->p_free = p_support_ptrs->p_free;
}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_get_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
rpc_ss_thread_handle_t rpc_ss_get_thread_handle
#ifdef IDL_PROTOTYPES
( void )
#else
()
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;
    rpc_ss_thread_indirection_t *thread_indirection_ptr;

    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &thread_indirection_ptr );
    if (thread_indirection_ptr == NULL) return(NULL);
    else return (rpc_ss_thread_handle_t)(thread_indirection_ptr->indirection);
}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_set_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
void rpc_ss_set_thread_handle
#ifdef IDL_PROTOTYPES
(
    rpc_ss_thread_handle_t thread_handle
)
#else
( thread_handle )
    rpc_ss_thread_handle_t thread_handle;
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
	      rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;
	      
	      p_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
	      /* Release any memory owned by the memory handle */
	      rpc_ss_mem_free( p_thread_support_ptrs->p_mem_h );
	      
	      /*
	       *  Free the objects it points at.
	       *  Must cast because instance_of
	       *  (rpc_ss_thread_support_ptrs_t).p_mem_h
	       *  is of type rpc_mem_handle, which is a pointer to volatile,
	       *  and free() doesn't take a pointer to volatile.
	       */
	      free( (idl_void_p_t)p_thread_support_ptrs->p_mem_h );
	      RPC_SS_THREADS_MUTEX_DELETE( &(p_thread_support_ptrs->mutex) );
	      
	      /* Free the structure */
	      free( p_thread_support_ptrs );
	 }
	 /* End of OT 13597 */
        free( helper_thread_indirection_ptr );
    }

    /* Now create the new context */
    helper_thread_indirection_ptr = (rpc_ss_thread_indirection_t *)
                            malloc(sizeof(rpc_ss_thread_indirection_t));
    if (helper_thread_indirection_ptr == NULL)
    {
        RAISE( rpc_x_no_memory );
    }
    helper_thread_indirection_ptr->indirection =
                                  (rpc_ss_thread_support_ptrs_t *)thread_handle;
    helper_thread_indirection_ptr->free_referents = idl_false;
    RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key,
                                           helper_thread_indirection_ptr );
}

/******************************************************************************/
/*                                                                            */
/*    Create thread context with references to named alloc and free rtns      */
/*                                                                            */
/******************************************************************************/
void rpc_ss_set_client_alloc_free
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t (*p_allocate)(
        idl_size_t size
    ),
    void (*p_free)(
        idl_void_p_t ptr
    )
)
#else
( p_allocate, p_free )
    idl_void_p_t (*p_allocate)();
    void (*p_free)();
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

    rpc_ss_client_get_thread_ctx( &p_support_ptrs );
    p_support_ptrs->p_allocate = p_allocate;
    p_support_ptrs->p_free = p_free;
}

/******************************************************************************/
/*                                                                            */
/*    Get the existing allocate, free routines and replace them with new ones */
/*                                                                            */
/******************************************************************************/
void rpc_ss_swap_client_alloc_free
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t (*p_allocate)(
        idl_size_t size
    ),
    void (*p_free)(
        idl_void_p_t ptr
    ),
    idl_void_p_t (**p_p_old_allocate)(
        idl_size_t size
    ),
    void (**p_p_old_free)(
        idl_void_p_t ptr
    )
)
#else
( p_allocate, p_free, p_p_old_allocate, p_p_old_free )
    idl_void_p_t (*p_allocate)();
    void         (*p_free)();
    idl_void_p_t (**p_p_old_allocate)();
    void         (**p_p_old_free)();
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

    rpc_ss_client_get_thread_ctx( &p_support_ptrs );
    *p_p_old_allocate = p_support_ptrs->p_allocate;
    *p_p_old_free = p_support_ptrs->p_free;
    p_support_ptrs->p_allocate = p_allocate;
    p_support_ptrs->p_free = p_free;
}

/****************************************************************************/
/* rpc_ss_client_free                                                       */
/*                                                                          */
/* Free the specified memory using the free routine from the current memory */
/* management environment.  This routine provides a simple interface to     */
/* free memory returned from an RPC call.                                   */
/*                                                                          */
/****************************************************************************/
void rpc_ss_client_free
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t p_mem
)
#else
( p_mem )
    idl_void_p_t p_mem;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

    /* Get free routine address */
    rpc_ss_client_get_thread_ctx( &p_support_ptrs );
    /* Invoke free with the specified memory */
    (*p_support_ptrs->p_free)( p_mem );
}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_enable_allocate                                                  */
/*    Create environment for rpc_ss_allocate to be used                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_enable_allocate
#ifdef IDL_PROTOTYPES
( void )
#else
()
#endif
{
    rpc_ss_mem_handle *p_mem_handle;
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;

    /* Make sure there is a thread context key */
#ifndef MEMORY_NOT_WRITTEN_SERIALLY
    if ( ! rpc_ss_allocate_is_set_up )
#endif
        rpc_ss_init_allocate_once();

    /* Set up the parts of the required data structure */
    p_mem_handle = (rpc_ss_mem_handle *)malloc(sizeof(rpc_ss_mem_handle));
    if (p_mem_handle == NULL)
    {
        RAISE( rpc_x_no_memory );
    }
    p_mem_handle->memory = NULL;
    p_mem_handle->node_table = NULL;
    p_thread_support_ptrs = (rpc_ss_thread_support_ptrs_t *)
                                   malloc(sizeof(rpc_ss_thread_support_ptrs_t));
    if (p_thread_support_ptrs == NULL)
    {
        RAISE( rpc_x_no_memory );
    }

    /* Complete the data structure and associate it with the key */
    /* This will make rpc_ss_allocate, rpc_ss_free the allocate/free pair */
    rpc_ss_build_indirection_struct( p_thread_support_ptrs, p_mem_handle,
                                     idl_true );
}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_disable_allocate                                                 */
/*    Destroy environment created by rpc_ss_enable_allocate                   */
/*                                                                            */
/******************************************************************************/
void rpc_ss_disable_allocate
#ifdef IDL_PROTOTYPES
( void )
#else
()
#endif
{
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;

    /* Get the thread support pointers structure */
    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );

    rpc_ss_destroy_thread_ctx( helper_thread_indirection_ptr );
}

#ifdef MIA
/******************************************************************************/
/*                                                                            */
/*    MTS version of                                                          */
/*                                                                            */
/*    Do we currently have thread context data?                               */
/*    If not, create local storage with malloc() and free() as the            */
/*      allocate and free routines                                            */
/*    Copy pointers to allocate and free routines to local storage            */
/*                                                                            */
/******************************************************************************/
void rpc_ss_mts_client_estab_alloc
#ifdef IDL_PROTOTYPES
(
    volatile IDL_ms_t * IDL_msp
)
#else
( IDL_msp )
    volatile IDL_ms_t * IDL_msp;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

#ifdef PERFMON
    RPC_SS_CLIENT_ESTABLISH_ALLOC_N;
#endif

    rpc_ss_client_get_thread_ctx( &p_support_ptrs );
    IDL_msp->IDL_p_allocate = p_support_ptrs->p_allocate;
    IDL_msp->IDL_p_free = p_support_ptrs->p_free;

#ifdef PERFMON
    RPC_SS_CLIENT_ESTABLISH_ALLOC_X;
#endif

}
#endif
