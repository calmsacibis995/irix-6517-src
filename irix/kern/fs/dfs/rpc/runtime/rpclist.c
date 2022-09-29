/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: rpclist.c,v 65.6 1998/03/24 16:00:12 lmc Exp $";
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
 * $Log: rpclist.c,v $
 * Revision 65.6  1998/03/24 16:00:12  lmc
 * Fix some of the #ifdef's for SGI_RPC_DEBUG.  Also typecast a few
 * variables to fix warnings.
 *
 * Revision 65.5  1998/03/23  17:30:26  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:38  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:39  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/27  19:47:06  jdoak
 * 6.4 + 1.2.2 merge
 *
 * Revision 65.1  1997/10/20  19:17:10  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.844.2  1996/02/18  00:05:30  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:57:20  marty]
 *
 * Revision 1.1.844.1  1995/12/08  00:21:57  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:35 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1995/05/25  21:41 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:02 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:37 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 	[1995/12/08  00:00:23  root]
 * 
 * Revision 1.1.5.8  1993/10/25  17:28:25  tatsu_s
 * 	Removed the assertions, {next,last} == NULL, from
 * 	rpc__list_element_free(), instead added the memory cleanup.
 * 	[1993/10/25  15:38:34  tatsu_s]
 * 
 * 	Nullify the next and last pointers in rpc__list_element_alloc().
 * 	Added the assertions, {next,last} == NULL, in
 * 	rpc__list_element_free().
 * 	[1993/10/22  13:29:21  tatsu_s]
 * 
 * Revision 1.1.5.7  1993/10/08  16:32:29  tatsu_s
 * 	Removed U_STACK_TRACE().
 * 	[1993/10/07  20:21:45  tatsu_s]
 * 
 * Revision 1.1.5.6  1993/09/20  18:01:45  tatsu_s
 * 	KK merged upto DCE1_0_3b03.
 * 
 * 	Added the message print to the double-free check.
 * 	[1993/08/27  18:12:58  tatsu_s]
 * 
 * Revision 1.1.7.4  1993/09/17  17:21:37  tatsu_s
 * 	Loading drop DCE1_0_3b930917
 * 	Revision 1.1.5.5  1993/09/01  16:52:23  tatsu_s
 * 	Fixed the double mutex lock in rpc__list_element_free().
 * 	[1993/09/01  16:49:59  tatsu_s]
 * 
 * Revision 1.1.6.2  1993/09/08  14:55:04  damon
 * 	Synched with ODE 2.3 based build
 * 	[1993/09/08  14:46:44  damon]
 * 
 * Revision 1.1.842.2  1994/06/21  21:54:15  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:33:43  hopkins]
 * 
 * Revision 1.1.842.1  1994/01/21  22:39:06  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:38  cbrooks]
 * 
 * Revision 1.1.6.1  1993/09/07  17:32:40  tom
 * 	Bug 8036 - If alloc_rtn in rpc__list_element_alloc() raises an exception,
 * 	we DIE so we don't return null to callers that don't expect it.
 * 	[1993/09/07  17:32:24  tom]
 * 
 * Revision 1.1.4.3  1993/01/03  23:54:45  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:12:01  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  21:15:42  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:43:44  zeliff]
 * 
 * Revision 1.1.2.2  1992/06/23  21:47:40  sekhar
 * 	22-jun-92    wh Rearranged the locking structure with list_element_alloc
 * 	                     so that we reacquire the mutex everytime we retry the
 * 	                     malloc operation.
 * 	[1992/06/23  21:05:57  sekhar]
 * 
 * Revision 1.1  1992/01/19  03:06:46  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      rpclist.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  This module contains routines to maintain lookaside list descriptors.
**
**
*/

#include <commonp.h>

GLOBAL rpc_lookaside_rcb_t rpc_g_lookaside_rcb =
{
    RPC_C_LOOKASIDE_RES,
    0,
    RPC_C_LOOKASIDE_RES_MAX_WAIT,
    RPC_C_LOOKASIDE_RES_WAIT,
};

/*
**++
**
**  ROUTINE NAME:       rpc__list_desc_init
**
**  SCOPE:              PRIVATE - declared in rpclist.h
**
**  DESCRIPTION:
**
**  Initializes a lookaside list descriptor.  The maximum size and element
**  size are set as part of this operation.
**
**  INPUTS:
**
**      list_desc       List descriptor which is to be initialized.
**
**      max_size        The maximum length of the lookaside list.
**
**      element_size    The size of each list element.
**
**      element_type    The type of each list element (from rpcmem.h).
**
**      alloc_rtn       The element specific routine to be
**                      called when allocating from heap. If this is
**                      NULL no routine will be called.
**
**      free_rtn        The element specific alloc routine to be
**                      called when freeing to heap. If this is NULL
**                      no routine will be called.
**
**      mutex           The list specific mutex used to protect the
**                      integrity of the list. If the NULL is
**                      provided the global lookaside list mutex and
**                      condition variable will be used.It is used when 
**                      blocking on or signalling condition
**                      variables. Note that the neither 
**                      rpc__list_element_alloc or _free ever
**                      explicitly acquires or releases this mutex.
**                      This must be done by the caller.
**
**      cond            The list specific condition variable
**                      associated with the above mutex. Valid iff
**                      mutex is not NULL.
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

PRIVATE void rpc__list_desc_init 
#ifdef _DCE_PROTO_
(
    rpc_list_desc_p_t               list_desc,
    unsigned32                      max_size,
    unsigned32                      element_size,
    unsigned32                      element_type,
    rpc_list_element_alloc_fn_t     alloc_rtn,
    rpc_list_element_free_fn_t      free_rtn,
    rpc_mutex_p_t                   mutex,
    rpc_cond_p_t                    cond
)
#else
(list_desc, max_size, element_size, element_type, alloc_rtn, free_rtn, mutex, cond) 
rpc_list_desc_p_t               list_desc;
unsigned32                      max_size;
unsigned32                      element_size;
unsigned32                      element_type;
rpc_list_element_alloc_fn_t     alloc_rtn;
rpc_list_element_free_fn_t      free_rtn;
rpc_mutex_p_t                   mutex;
rpc_cond_p_t                    cond;
#endif
{
    list_desc->max_size = max_size;
    list_desc->cur_size = 0;
    list_desc->element_size = element_size;
    list_desc->element_type = element_type;
    list_desc->alloc_rtn = alloc_rtn;
    list_desc->free_rtn = free_rtn;
    if (mutex == NULL)
    {
        list_desc->use_global_mutex = true;
    }
    else
    {
        list_desc->use_global_mutex = false;
        list_desc->mutex = mutex;
        list_desc->cond = cond;
    }
    RPC_LIST_INIT (list_desc->list_head);
}

/*
**++
**
**  ROUTINE NAME:       rpc__list_element_alloc
**
**  SCOPE:              PRIVATE - declared in rpclist.h
**
**  DESCRIPTION:
**
**  Remove the first element in the lookaside list and return a pointer
**  to it.  If the lookaside list is empty, an element of the size
**  indicated in the lookaside list descriptor will be allocated from
**  the heap.
**
**  INPUTS:             none
**
**      list_desc       The lookaside list descriptor.
**
**      block           true if the alloc should block for heap to become free
**                      false if it is not to block
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
**      return          Pointer to the allocated list element.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE pointer_t rpc__list_element_alloc 
#ifdef _DCE_PROTO_
(
    rpc_list_desc_p_t       list_desc,
    boolean32               block
)
#else
(list_desc, block)
rpc_list_desc_p_t       list_desc;
boolean32               block;
#endif
{
    volatile pointer_t  element;
    unsigned32          wait_cnt;
    struct timespec     delta;
    struct timespec     abstime;

    RPC_LOG_LIST_ELT_ALLOC_NTR;

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG_EXACT(rpc_es_dbg_mem,20))
    {
        unsigned32 count;
        rpc_list_p_t next_element;

        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_LOCK (rpc_g_lookaside_rcb.res_lock);
        }

        for (count = 0,
             next_element = ((rpc_list_p_t)(list_desc->list_head.next));
             next_element != NULL;
             count++,
             next_element = ((rpc_list_p_t)(next_element->next)));

        if (list_desc->cur_size != count)
        {
  EPRINTF("(rpc__list_element_alloc) cur_size mismatch: cur_size (%d) != %d\n",
                    list_desc->cur_size, count);
            DIE ("(rpc__list_element_alloc) lookaside list is corrupted");
        }

        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
        }
    }
#endif

    for (wait_cnt = 0;
         wait_cnt < rpc_g_lookaside_rcb.max_wait_times;
         wait_cnt++)
    {
        /*
         * Acquire the global resource control lock for all lookaside
         * lists if the caller doesn't have their own lock.
         */
        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_LOCK (rpc_g_lookaside_rcb.res_lock);
        }

        /*
         * Try allocating a structure off the lookaside list given.
         */
        if (list_desc->cur_size > 0)
        {
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
            if (list_desc->list_head.next == NULL)
            {
		/*
		 * rpc_m_lookaside_corrupt
		 * "(%s) Lookaside list is corrupted"
		 */
		RPC_DCE_SVC_PRINTF ((
		    DCE_SVC(RPC__SVC_HANDLE, "%s"),
		    rpc_svc_general,
		    svc_c_sev_fatal | svc_c_action_abort,
		    rpc_m_lookaside_corrupt,
		    "rpc__list_element_alloc" ));
            }
#endif
            list_desc->cur_size--;
            RPC_LIST_REMOVE_HEAD (list_desc->list_head, element, pointer_t);

            /*
             * Release the global resource control lock for all lookaside
             * lists if the caller doesn't have their own lock.
             */
            if (list_desc->use_global_mutex)
            {
                RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
            }
            break;
        }
        else
        {
            /*
             * Release the global resource control lock if the
             * caller doesn't have their own lock for all lookaside lists
             * since the structure was available on the lookaside list.
             *
             * We do it now because allocating an element from heap is a relatively
             * time consuming operation.
             */
            if (list_desc->use_global_mutex)
            {
                RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
            }

            /*
             * The lookaside list is empty. Try and allocate from
             * heap.
             */
            RPC_MEM_ALLOC (element,
                           pointer_t,
                           list_desc->element_size,
                           list_desc->element_type,
                           RPC_C_MEM_NOWAIT);

            if (element == NULL)
            {
                /*
                 * The heap allocate failed. If the caller indicated
                 * that we should not block return right now.
                 */
                if (block == false)
                {
                    break;
                }

                delta.tv_sec = rpc_g_lookaside_rcb.wait_time;
                delta.tv_nsec = 0;
                pthread_get_expiration_np (&delta, &abstime);

                /*
                 * If we are using the global lookaside list lock
                 * then reaquire the global lookaside list lock and
                 * wait on the global lookaside list condition 
                 * variable otherwise use the caller's mutex and
                 * condition variable. 
                 */
                if (list_desc->use_global_mutex)
                {
                    RPC_MUTEX_LOCK (rpc_g_lookaside_rcb.res_lock);
                    RPC_COND_TIMED_WAIT (rpc_g_lookaside_rcb.wait_flg,
                                         rpc_g_lookaside_rcb.res_lock,
                                         &abstime); 
                    RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
                }
                else
                {
                    RPC_COND_TIMED_WAIT (*list_desc->cond,
                                         *list_desc->mutex,
                                         &abstime); 
                }

                /*
                 * Try to allocate the structure again.
                 */
                continue;
            }
            else
            {
                /*
                 * The RPC_MEM_ALLOC succeeded. If an alloc routine
                 * was specified when the lookaside list was inited
                 * call it now.
                 */
                if (list_desc->alloc_rtn != NULL)
                {
                    /*
                     * Catch any exceptions which may occur in the
                     * list-specific alloc routine. Any exceptions
                     * will be caught and the memory will be freed.
                     */
                    TRY
                    {
                        (*list_desc->alloc_rtn) (element);
                    }
                    CATCH_ALL
                    {
                        RPC_MEM_FREE (element, list_desc->element_type);
                        element = NULL;
			/*
			 * rpc_m_call_failed_no_status
			 * "%s failed"
			 */
			RPC_DCE_SVC_PRINTF ((
			    DCE_SVC(RPC__SVC_HANDLE, "%s"),
			    rpc_svc_general,
			    svc_c_sev_fatal | svc_c_action_abort,
			    rpc_m_call_failed_no_status,
			    "rpc__list_element_alloc/(*list_desc->alloc_rtn)(element)" ));
			return (NULL);
                    }
                    ENDTRY
                }
                break;
            }
        }
    }
    ((rpc_list_p_t)element)->next = NULL;
    ((rpc_list_p_t)element)->last = NULL;
    RPC_LOG_LIST_ELT_ALLOC_XIT;
    return (element);
}


/*
**++
**
**  ROUTINE NAME:       rpc__list_desc_free
**
**  SCOPE:              PRIVATE - declared in rpclist.h
**
**  DESCRIPTION:
**
**  Returns an element to the lookaside list.  If this would result
**  in the current size of the lookaside list becoming greater than
**  the maximum size, the element will be returned to the heap instead.
**
**  INPUTS:
**
**      list_desc       The list descriptor.
**
**      list_element    Pointer to the element to be freed.
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

PRIVATE void rpc__list_element_free 
#ifdef _DCE_PROTO_
(
    rpc_list_desc_p_t       list_desc,
    pointer_t               list_element
)
#else
(list_desc, list_element)
rpc_list_desc_p_t       list_desc;
pointer_t               list_element;
#endif
{
    RPC_LOG_LIST_ELT_FREE_NTR;

    assert(list_desc != NULL);
    assert(list_element != NULL);

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG_EXACT(rpc_es_dbg_mem,20))
    {
        unsigned32 count;
        rpc_list_p_t next_element;

        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_LOCK (rpc_g_lookaside_rcb.res_lock);
        }

        for (count = 0,
             next_element = ((rpc_list_p_t)(list_desc->list_head.next));
             next_element != NULL;
             count++,
             next_element = ((rpc_list_p_t)(next_element->next)))
        {
            if (next_element == (rpc_list_p_t)list_element)
            {
EPRINTF("(rpc__list_element_free) element already on lookaside list %#08x (%d)\n",
                        list_element, count);
                DIE ("(rpc__list_element_alloc) lookaside list is corrupted");
            }
        }

        if (list_desc->cur_size != count)
        {
  EPRINTF("(rpc__list_element_alloc) cur_size mismatch: cur_size (%d) != %d\n",
                    list_desc->cur_size, count);
            DIE ("(rpc__list_element_alloc) lookaside list is corrupted");
        }

        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
        }
    }
#endif

    /*
     * Acquire the global resource control lock for all lookaside
     * lists if the caller doesn't have their own lock.
     */
    if (list_desc->use_global_mutex)
    {
        RPC_MUTEX_LOCK (rpc_g_lookaside_rcb.res_lock);
    }
    
    if (list_desc->cur_size < list_desc->max_size)
    {
        list_desc->cur_size++;

        RPC_LIST_ADD_TAIL (list_desc->list_head, list_element, pointer_t);

        /*
         * Now check whether any other thread is waiting for a lookaside list
         * structure. 
         */
        if (rpc_g_lookaside_rcb.waiter_cnt > 0)
        {
            /*
             * There are waiters. Signal the global lookaside list
             * condition variable if the caller doesn't have one.
             * Otherwise signal the caller provided condition
             * variable.
             */
            if (list_desc->use_global_mutex)
            {
                RPC_COND_SIGNAL (rpc_g_lookaside_rcb.wait_flg, 
                                 rpc_g_lookaside_rcb.res_lock);
            }
            else
            {
                RPC_COND_SIGNAL (*list_desc->cond,
                                 *list_desc->mutex);
            }
        }

        /*
         * Release the global resource control lock for all lookaside
         * lists if the caller doesn't have their own lock 
         * since the structure has now been added to the list.
         */
        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
        }
    }
    else
    {
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
        if (RPC_DBG_EXACT(rpc_es_dbg_mem,20) && RPC_DBG_EXACT(rpc_es_dbg_mem_type,20))
        {
            /*
             * Free the head instead of the tail.
             */
            RPC_LIST_ADD_TAIL (list_desc->list_head, list_element, pointer_t);
            RPC_LIST_REMOVE_HEAD (list_desc->list_head, list_element,
                                  pointer_t);
        }
#endif
        /*
         * If a free routine was specified when this lookaside list
         * was inited call it now.
         */
        if (list_desc->free_rtn != NULL)
        {
            (list_desc->free_rtn) (list_element);
        }

        /*
         * Release the global resource control lock for all
         * lookaside lists if the caller doesn't have their own lock.
         * 
         * We do it now because freeing an element to the heap is a relatively
         * time consuming operation.
         */
        if (list_desc->use_global_mutex)
        {
            RPC_MUTEX_UNLOCK (rpc_g_lookaside_rcb.res_lock);
        }
        
        /*
         * Clean the element for detecting a possible reuse.
         */
        memset (list_element, 0, list_desc->element_size);
        RPC_MEM_FREE (list_element, list_desc->element_type);
    }

    RPC_LOG_LIST_ELT_FREE_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc__list_fork_handler
**
**  SCOPE:              PRIVATE - declared in rpclist.h
**
**  DESCRIPTION:
**
**  Perform fork-related processing, depending on what stage of the 
**  fork we are currently in.
**
**  INPUTS:
**
**      stage           The current stage of the fork process.
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

PRIVATE void rpc__list_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t     stage
)
#else
(stage)
rpc_fork_stage_id_t     stage;
#endif
{
    switch ((int)stage)
    {
        case RPC_C_PREFORK:
                break;
        case RPC_C_POSTFORK_PARENT:
                break;
        case RPC_C_POSTFORK_CHILD:  
                /*
                 * Reset the lookaside waiter's count.
                 */
                rpc_g_lookaside_rcb.waiter_cnt = 0;
                break;
    }  
}
