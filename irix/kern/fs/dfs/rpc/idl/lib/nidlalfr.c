/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: nidlalfr.c,v 65.5 1998/03/23 17:25:20 gwehrman Exp $";
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
 * $Log: nidlalfr.c,v $
 * Revision 65.5  1998/03/23 17:25:20  gwehrman
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
 * Revision 65.2  1997/10/20  19:15:41  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.12.2  1996/02/17  23:58:13  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:52:38  marty]
 *
 * Revision 1.1.12.1  1995/12/07  22:31:18  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:08 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:16:19  root]
 * 
 * Revision 1.1.2.1  1995/11/01  14:22:21  bfc
 * 	idl cleanup
 * 	[1995/11/01  14:21:17  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/11/01  14:12:58  bfc]
 * 
 * Revision 1.1.8.1  1994/07/26  17:21:09  rico
 * 	Create rpc_ss_mem_alloc for exc_handling/mutex locking bug
 * 	in rpc_ss_allocate
 * 	[1994/07/26  17:15:56  rico]
 * 
 * Revision 1.1.6.2  1993/07/07  20:10:19  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:39:01  ganni]
 * 
 * $EndLog$
 */
/*
** 25-Mar-92 harrow     Use idl_size_t which matches size_t argument to malloc().
*/
/*
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
**
**  NAME:
**
**      nidlalfr.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      rpc_ss_allocate, rpc_ss_free and helper thread routines
**
**  VERSION: DCE 1.0
**
*/

#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef PERFMON
#include <dce/idl_log.h>
#endif

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_allocate                                                         */
/*                                                                            */
/******************************************************************************/
idl_void_p_t IDL_ALLOC_ENTRY rpc_ss_allocate
#ifdef IDL_PROTOTYPES
(
    idl_size_t size
)
#else
( size )
    idl_size_t size;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;
    rpc_void_p_t                 p_new_node;
    error_status_t               status;

#ifdef PERFMON
    RPC_SS_ALLOCATE_N;
#endif

    rpc_ss_get_support_ptrs( &p_support_ptrs );
    /* OT 13600 */
    if (p_support_ptrs == NULL)
	 return NULL;
    
    RPC_SS_THREADS_MUTEX_LOCK(&(p_support_ptrs->mutex));
    p_new_node = (rpc_void_p_t)rpc_sm_mem_alloc( p_support_ptrs->p_mem_h, size, &status );
    RPC_SS_THREADS_MUTEX_UNLOCK(&(p_support_ptrs->mutex));

    if (status == rpc_s_no_memory) RAISE( rpc_x_no_memory );

#ifdef PERFMON
    RPC_SS_ALLOCATE_X;
#endif

    return(p_new_node);

}

/******************************************************************************/
/*                                                                            */
/*    rpc_ss_free                                                             */
/*                                                                            */
/******************************************************************************/
void IDL_ALLOC_ENTRY rpc_ss_free
#ifdef IDL_PROTOTYPES
(
    idl_void_p_t node_to_free
)
#else
(node_to_free)
    idl_void_p_t node_to_free;
#endif
{
    rpc_ss_thread_support_ptrs_t *p_support_ptrs;

#ifdef PERFMON
    RPC_SS_FREE_N;
#endif

    rpc_ss_get_support_ptrs( &p_support_ptrs );
    /* OT 13600 */
    if (p_support_ptrs == NULL)
	 return;
    
    RPC_SS_THREADS_MUTEX_LOCK(&(p_support_ptrs->mutex));
    if (p_support_ptrs->p_mem_h->node_table)
        /*
         * Must unregister node or a subsequent alloc could get same addr and
         * nodetbl mgmt would think it was an alias to storage's former life.
         */
        rpc_ss_unregister_node(p_support_ptrs->p_mem_h->node_table,
                               (byte_p_t)node_to_free);
    rpc_ss_mem_release(p_support_ptrs->p_mem_h, (byte_p_t)node_to_free, ndr_true);
    RPC_SS_THREADS_MUTEX_UNLOCK(&(p_support_ptrs->mutex));

#ifdef PERFMON
    RPC_SS_FREE_X;
#endif

}
