/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: allocate.c,v 65.6 1998/04/09 20:23:45 lmc Exp $";
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
 * $Log: allocate.c,v $
 * Revision 65.6  1998/04/09 20:23:45  lmc
 * DEBUG_MEM is turned off for SGIMIPS so that kernel builds work.  This
 * looks like an attempt to make debugging set via an environment variable,
 * but it is compiled into the code.  If DEBUG_MEM=0, then it adds the lines
 * for the if statement, but the if is always 0.
 *
 * Revision 65.5  1998/03/23  17:25:51  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/03/03 15:56:12  lmc
 * Put a #ifndef KERNEL around some DEBUG setup.  It was using getenv()
 * which we can't use in the kernel.
 *
 * Revision 65.3  1998/01/07  17:20:08  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:56:16  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:15:30  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/18  18:52:48  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  18:05:57  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:23:53  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/dat_xidl2/1  1995/11/17  17:05 UTC  dat
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/12/07  21:13:41  root]
 * 
 * Revision 1.1.2.1  1995/10/23  01:48:52  bfc
 * 	oct 95 idl drop
 * 	[1995/10/22  23:35:40  bfc]
 * 
 * 	may 95 idl drop
 * 	[1995/10/22  22:56:33  bfc]
 * 
 * 	DCE for DEC OSF/1: populate from OSF DCE R1.1
 * 	[1995/10/21  18:25:07  bfc]
 * 
 * Revision 1.1.6.1  1994/07/26  17:21:06  rico
 * 	Create rpc_ss_mem_alloc for exc_handling/mutex locking bug
 * 	in rpc_ss_allocate
 * 	[1994/07/26  17:15:33  rico]
 * 
 * Revision 1.1.4.2  1993/07/07  20:04:26  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:15  ganni]
 * 
 * $EndLog$
 */
/*
**  @OSF_COPYRIGHT@
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
**      allocate.c
**
**  FACILITY:
**
**      IDL Stub Support Routines
**
**  ABSTRACT:
**
**  Stub memory allocation and free routines to keep track of all allocated
**  memory so that it can readily be freed
**
**  VERSION: DCE 1.0
**
*/

#include <dce/idlddefs.h>
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifndef _KERNEL
#ifdef DEBUG
#   include <stdio.h>
#define DEBUG_MEM getenv("DEBUGMEM")
#endif
#else
#define DEBUG_MEM 0
#endif
#ifdef SGIMIPS
#undef DEBUG_MEM
#endif


#ifdef PERFMON
#include <dce/idl_log.h>
#endif


/*
 * The following is the header information at the beginning of all
 * memory returned
 */
typedef struct header
{
    rpc_ss_mem_handle *head;    /* only valid for the first entry in the list */
    struct header *next;
    struct header *prev;
} header;

/*
 * Leave room in the buffer allocated before the data address for a pointer
 * to the buffer address (for freeing) and a node number (for the convenience
 * of server stubs)
 */
typedef struct
{
    header **buffer_address;
    ndr_ulong_int node_number;
} ptr_and_long;

/*
 * If the data address is 8-byte aligned and the gap is a multiple of 8-bytes
 * in size then the gap is maximally aligned, too
 */
#define GAP ((sizeof(ptr_and_long) + 7) & ~7)

byte_p_t rpc_ss_mem_alloc
#ifdef IDL_PROTOTYPES
(
    rpc_ss_mem_handle *handle,
    unsigned bytes
)
#else
(handle, bytes)
    rpc_ss_mem_handle *handle;
    unsigned bytes;
#endif
{
    /*
     * In addition to the memory requested by the user, allocate space for
     * the header, gap, and 8-byte alignment
     */
    byte_p_t data_addr;
    header *new;

#ifdef PERFMON
    RPC_SS_MEM_ALLOC_N;
#endif

    new = (header *)malloc(sizeof(header) + GAP + 7 + bytes);

#ifdef DEBUG_MEM		/* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
       printf("Allocated %d bytes at %lx\n", sizeof(header) + GAP + 7 + bytes,
           new);
#endif

    if (new == NULL) RAISE( rpc_x_no_memory );

    if (handle->memory) ((header *)(handle->memory))->prev = new;

    new->head = handle;
    new->prev = NULL;
    new->next = (header *)handle->memory;
    handle->memory = (rpc_void_p_t)new;

    /*
     * Skip the header, and gap and align to an 8-byte boundary
     */
    data_addr = (byte_p_t)
        (((char *)new - (char *)0 + sizeof(header) + GAP + 7) & ~7);

    /*
     * Stash away the buffer address for _release and _dealloc
     */
    *((header **)((char *)data_addr - GAP)) = new;

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
    printf("Returning %lx\n", data_addr);
#endif

#ifdef PERFMON
    RPC_SS_MEM_ALLOC_X;
#endif

    return data_addr;
}

/*
 * Any changes to rpc_sm_mem_alloc should be parralleled in rpc_ss_mem_alloc
 *    Changed to avoid leaving mutex locked in rpc_ss_allocate
 *    6-15-94 Simcoe
 */

byte_p_t rpc_sm_mem_alloc
#ifdef IDL_PROTOTYPES
(
    rpc_ss_mem_handle *handle,
    unsigned bytes,
    error_status_t *st
)
#else
(handle, bytes, st)
    rpc_sm_mem_handle *handle;
    unsigned bytes;
    error_status_t *st;
#endif
{
    /*
     * In addition to the memory requested by the user, allocate space for
     * the header, gap, and 8-byte alignment
     */
    byte_p_t data_addr;
    header *new;

#ifdef PERFMON
    RPC_SM_MEM_ALLOC_N;
#endif

    new = (header *)malloc(sizeof(header) + GAP + 7 + bytes);

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
       printf("Allocated %d bytes at %lx\n", sizeof(header) + GAP + 7 + bytes,
           new);
#endif

    if (new == NULL)
    {
      *st = rpc_s_no_memory;
      return(NULL);
    }
    else *st = error_status_ok;

    if (handle->memory) ((header *)(handle->memory))->prev = new;

    new->head = handle;
    new->prev = NULL;
    new->next = (header *)handle->memory;
    handle->memory = (rpc_void_p_t)new;

    /*
     * Skip the header, and gap and align to an 8-byte boundary
     */
    data_addr = (byte_p_t)
        (((char *)new - (char *)0 + sizeof(header) + GAP + 7) & ~7);

    /*
     * Stash away the buffer address for _release and _dealloc
     */
    *((header **)((char *)data_addr - GAP)) = new;

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
    printf("Returning %lx\n", data_addr);
#endif

#ifdef PERFMON
    RPC_SM_MEM_ALLOC_X;
#endif

    return data_addr;
}

void rpc_ss_mem_free
#ifdef IDL_PROTOTYPES
(
    rpc_ss_mem_handle *handle
)
#else
(handle)
    rpc_ss_mem_handle *handle;
#endif
{
    header *tmp;

#ifdef PERFMON
    RPC_SS_MEM_FREE_N;
#endif

    while (handle->memory)
    {
        tmp = (header *)handle->memory;
        handle->memory = (rpc_void_p_t)((header *)handle->memory)->next;

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
	if (DEBUG_MEM)
        printf("Freeing %lx\n", tmp);
#endif

        free((byte_p_t)tmp);
    }

#ifdef PERFMON
    RPC_SS_MEM_FREE_X;
#endif

}

void rpc_ss_mem_release
#ifdef IDL_PROTOTYPES
(
    rpc_ss_mem_handle *handle,
    byte_p_t data_addr,
    int freeit
)
#else
(handle, data_addr, freeit)
    rpc_ss_mem_handle *handle;
    byte_p_t data_addr;
    int freeit;
#endif
{
    header *this = *(header **)((char *)data_addr - GAP);

#ifdef PERFMON
    RPC_SS_MEM_RELEASE_N;
#endif

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
       printf("Releasing %lx\n", this);
#endif

    if (this->next) this->next->prev = this->prev;
    if (this->prev) this->prev->next = this->next;
    else
    {
        /*
         * We've deleted the first element of the list
         */
        handle->memory = (rpc_void_p_t)this->next;
        if (this->next) this->next->head = handle;
    }
    if (freeit) free((byte_p_t)this);

#ifdef PERFMON
    RPC_SS_MEM_RELEASE_X;
#endif

}

#ifdef MIA
void rpc_ss_mem_item_free
#ifdef IDL_PROTOTYPES
(
    rpc_ss_mem_handle *handle,
    byte_p_t data_addr
)
#else
(handle, data_addr)
    rpc_ss_mem_handle *handle;
    byte_p_t data_addr;
#endif
{
    header *this = *(header **)((char *)data_addr - GAP);

#ifdef PERFMON
    RPC_SS_MEM_ITEM_FREE_N;
#endif

#ifdef DEBUG_MEM                /* SGIMIPS - printf not defined in kernel */
    if (DEBUG_MEM)
       printf("Releasing %lx\n", this);
#endif

    if (this->next) this->next->prev = this->prev;
    if (this->prev) this->prev->next = this->next;
    else
    {
        /*
         * We've deleted the first element of the list
         */
        handle->memory = (rpc_void_p_t)this->next;
        if (this->next) this->next->head = handle;
    }
    free((byte_p_t)this);

#ifdef PERFMON
    RPC_SS_MEM_ITEM_FREE_X;
#endif

}
#endif

void rpc_ss_mem_dealloc
#ifdef IDL_PROTOTYPES
(
    byte_p_t data_addr
)
#else
(data_addr)
    byte_p_t data_addr;
#endif
{
    header *tmp = *((header **)((char *)data_addr - GAP));

#ifdef PERFMON
    RPC_SS_MEM_DEALLOC_N;
#endif

    /*
     * find first element of list to get to handle
     */
    while (tmp->prev) tmp = tmp->prev;

    rpc_ss_mem_release(tmp->head, data_addr, 1);

#ifdef PERFMON
    RPC_SS_MEM_DEALLOC_X;
#endif

}


#if 0
void traverse_list(rpc_ss_mem_handle handle)
{
    printf("List contains:");
    while (handle)
    {
        printf(" %d", handle);
        handle = ((header *)handle)->next;
    }
    printf(" (done)\n");
}

void main()
{
    char buf[100];
    byte_p_t tmp, *buff_addr;
    rpc_ss_mem_handle handle = NULL;

    do
    {
        printf("q/a bytes/f addr/d addr:");
        gets(buf);
        if (*buf == 'q')
        {
            rpc_ss_mem_free(&handle);
            exit();
        }
        if (*buf == 'a')
            if ((tmp = rpc_ss_mem_alloc(&handle, atoi(buf+2))) == NULL)
                printf("\tCouldn't get memory\n");
                else printf("\tGot %d\n", tmp);
        if (*buf == 'f')
            rpc_ss_mem_release(&handle, (byte_p_t)atoi(buf+2), 1);
        if (*buf == 'd')
            rpc_ss_mem_dealloc((byte_p_t)atoi(buf+2));
        traverse_list(handle);
    } while (*buf != 'q');
}
#endif
