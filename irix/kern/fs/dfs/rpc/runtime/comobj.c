/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comobj.c,v 65.4 1998/03/23 17:30:43 gwehrman Exp $";
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
 * $Log: comobj.c,v $
 * Revision 65.4  1998/03/23 17:30:43  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:42  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:42  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.41.2  1996/02/18  00:02:48  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:55:26  marty]
 *
 * Revision 1.1.41.1  1995/12/08  00:18:32  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:31 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/02  01:14 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:18 UTC  lmm
 * 	add memory allocation failure detection
 * 	[1995/12/07  23:58:19  root]
 * 
 * Revision 1.1.39.1  1994/01/21  22:35:40  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:40  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:22:33  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:33  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:44:38  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:34:17  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:09:53  devrcs
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
**      comobj.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of the Object Services for the Common Communication
**  Services component. These routines are called by an application
**  to register objects and object types with the runtime.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Public common communications services */
#include <comprot.h>    /* Common protocol services */
#include <comnaf.h>     /* Common network address family services */
#include <comp.h>       /* Private common communications services */

/*
 * the size of the object registry hash table
 * - pick a prime number to avoid collisions
 */
#define RPC_C_OBJ_REGISTRY_SIZE         31

/*
 *  The Object Registry Table, where object/type pairs
 *  are registered, and upon which the routines contained within
 *  this module perform their actions.  Protected by "obj_mutex".
 */
INTERNAL rpc_list_t obj_registry[RPC_C_OBJ_REGISTRY_SIZE] = { 0 };
INTERNAL rpc_mutex_t obj_mutex;


/*
 * an object registry list entry
 */
typedef struct
{
    rpc_list_t      link;
    uuid_t          object_uuid;
    uuid_t          type_uuid;
} rpc_obj_rgy_entry_t, *rpc_obj_rgy_entry_p_t;


/*
 * Function registered by applicationthrough rpc_object_set_inq_fn.
 */
INTERNAL rpc_object_inq_fn_t inq_fn = NULL;


/*
**++
**
**  ROUTINE NAME:       rpc__obj_init
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Initializes this module.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          The result of the operation.
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

PRIVATE void rpc__obj_init 
#ifdef _DCE_PROTO_
(
    unsigned32                  *status
)
#else
(status)
unsigned32                  *status;
#endif
{
    CODING_ERROR (status);

    RPC_MUTEX_INIT (obj_mutex);
    *status = rpc_s_ok;
}



/*
**++
**
**  ROUTINE NAME:       rpc__obj_fork_handler
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Initializes this module.
**
**  INPUTS:             stage   The stage of the fork we are 
**                              currently handling.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
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

PRIVATE void rpc__obj_fork_handler
#ifdef _DCE_PROTO_
(
    rpc_fork_stage_id_t stage
)
#else
(stage)
rpc_fork_stage_id_t stage;
#endif
{   
    unsigned32 i;

    switch ((int)stage)
    {
        case RPC_C_PREFORK:
                break;
        case RPC_C_POSTFORK_PARENT:
                break;
        case RPC_C_POSTFORK_CHILD:  

                inq_fn = NULL;
            
                /*
                 * Empty the Object Registry Table
                 */                                  
                for (i = 0; i < RPC_C_OBJ_REGISTRY_SIZE; i++)
                {
                    obj_registry[i].next = NULL;
                    obj_registry[i].last = NULL;
                }
                break;
    }
}


/*
**++
**
**  ROUTINE NAME:       rpc_object_set_type
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**
**  Register the type UUID of an object with the Common Communications
**  Service. This routine is used in conjunction with rpc_server_register_if
**  to allow a server to support multiple implementations of the same
**  interface for different object types. Registering objects is required
**  only when generic interfaces are declared (via "rpc_server_register_if").
**  The Common Communications Service will dispatch to a specific
**  implementation, contained in a manager Entry Point Vector, based on the
**  object UUID contained in the binding of the RPC. The Common Communications
**  Service, using the results of a call to this routine, will determine
**  the type UUID of the object. A manager EPV for this type UUID can
**  then be found using the results of a call to the rpc_server_register_if
**  routine.
**
**  INPUTS:  
**
**      object_uuid     The object to be registered.
**
**      type_uuid       The type of the object.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok
**          rpc_s_coding_error
**          rpc_s_invalid_object
**          rpc_s_invalid_type
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

PUBLIC void rpc_object_set_type 
#ifdef _DCE_PROTO_
(
    uuid_p_t                object_uuid,
    uuid_p_t                type_uuid,
    unsigned32              *status
)
#else
(object_uuid, type_uuid, status)
uuid_p_t                object_uuid;
uuid_p_t                type_uuid;
unsigned32              *status;
#endif
{
    rpc_obj_rgy_entry_p_t   obj_entry;
    unsigned32              index;
    
    
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * check to see if this is a valid, non-null object UUIDs
     */
    if (UUID_IS_NIL (object_uuid, status))
    {
        *status = rpc_s_invalid_object;
        return;
    }

    if (*status != uuid_s_ok)
    {
        *status = rpc_s_invalid_object;
        return;
    }
    
    /*
     * compute a hash value using the object uuid - check the status
     * from uuid_hash to make sure the uuid has a valid format
     */
    index = (uuid_hash (object_uuid, status)) % RPC_C_OBJ_REGISTRY_SIZE;
    
    if (*status != uuid_s_ok)
    {
        return;
    }

    /*
     * take out a global lock on the object registry
     */
    RPC_MUTEX_LOCK (obj_mutex);

    /*
     * search the registry for a matching object
     */
    RPC_LIST_FIRST (obj_registry[index], obj_entry, rpc_obj_rgy_entry_p_t);

    while (obj_entry != NULL)
    {
        if (uuid_equal (&(obj_entry->object_uuid), object_uuid, status))
        {
            break;
        }

        RPC_LIST_NEXT (obj_entry, obj_entry, rpc_obj_rgy_entry_p_t);
    }

    /*
     * if the type UUID is the NIL UUID, remove the type entry
     * for this object if it exists(this is basically an "unregister")
     */
    if (UUID_IS_NIL(type_uuid, status))
    {
        /*
         * remove the object entry from the registry and free it
         */
        if (obj_entry != NULL)
        {
            RPC_LIST_REMOVE (obj_registry[index], obj_entry);
            RPC_MEM_FREE (obj_entry, RPC_C_MEM_OBJ_RGY_ENTRY);
        }
    }
    else
    {
        /*
         * this is a register - if no entry was found, create one
         */
        if (obj_entry == NULL)
        {
            RPC_MEM_ALLOC (
                obj_entry,
                rpc_obj_rgy_entry_p_t,
                sizeof (rpc_obj_rgy_entry_t),
                RPC_C_MEM_OBJ_RGY_ENTRY,
                RPC_C_MEM_WAITOK);
	    if (obj_entry == NULL){
		*status = rpc_s_no_memory;
		RPC_MUTEX_UNLOCK (obj_mutex);
		return;
	    }
            
            /*
             * initialize the entry
             */
            obj_entry->object_uuid = *object_uuid;

            /*
             * put the object on the list for this hash index
             */
            RPC_LIST_ADD_TAIL
                (obj_registry[index], obj_entry, rpc_obj_rgy_entry_p_t);
        }
        else
        {
            /*
             * see if the type uuid matches the one specified
             */
            if (uuid_equal (&(obj_entry->type_uuid), type_uuid, status))
            {
                RPC_MUTEX_UNLOCK (obj_mutex);
                *status = rpc_s_already_registered;
                return;
            }
        }

        /*
         * set the type uuid for this object entry
         */
        obj_entry->type_uuid = *type_uuid;
    }

    RPC_MUTEX_UNLOCK (obj_mutex);
    *status = rpc_s_ok;
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_object_inq_type
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**
**  Consult the object registry for the specified object's type.  If
**  it is not found and the application has registered an inquiry function,
**  call it.  Otherwise, if object is not registered, the type returned
**  is nil_uuid.
**
**  Note: If a NULL value is passed for the type UUID argument the routine
**  will execute, but the type UUID will not be returned. This can be
**  useful to determine if an object is registered without requiring a
**  return value to be supplied.
** 
**  INPUTS:  
**
**      object_uuid     The object being looked up.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      type_uuid       The type of the object.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok
**          rpc_s_object_not_found
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

PUBLIC void rpc_object_inq_type 
#ifdef _DCE_PROTO_
(
    uuid_p_t                object_uuid,
    uuid_t                  *type_uuid,
    unsigned32              *status
)
#else
(object_uuid, type_uuid, status)
uuid_p_t                object_uuid;
uuid_t                  *type_uuid;
unsigned32              *status;
#endif
{
    rpc_obj_rgy_entry_p_t   obj_entry;
    unsigned32              index;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * lookups on nil objects return nil types
     */
    if (UUID_IS_NIL (object_uuid, status))
    {
        UUID_CREATE_NIL (type_uuid);
        *status = rpc_s_ok;
        return;
    }

    /*
     * compute a hash value using the object uuid - check the status
     * from uuid_hash to make sure the uuid has a valid format
     */
    index = uuid_hash (object_uuid, status) % RPC_C_OBJ_REGISTRY_SIZE;

    if (*status != uuid_s_ok)
    {
        return;
    }

    /*
     * take out a lock to protect access to the object registry
     */
    RPC_MUTEX_LOCK (obj_mutex);

    /*
     * search the table for the specified object UUID
     */
    RPC_LIST_FIRST (obj_registry[index], obj_entry, rpc_obj_rgy_entry_p_t);

    while (obj_entry != NULL)
    {
        if (uuid_equal (&(obj_entry->object_uuid), object_uuid, status))
        {
            /*
             * if a type uuid is wanted, return it
             */
            if (type_uuid != NULL)
            {
                *type_uuid = obj_entry->type_uuid;
            }

            RPC_MUTEX_UNLOCK (obj_mutex);
            *status = rpc_s_ok;
            return;
        }

        RPC_LIST_NEXT (obj_entry, obj_entry, rpc_obj_rgy_entry_p_t);
    }

    /*
     * If there's an application function to map object to type, call it now.
     * Ensure that a object_not_found failure returns the nil-type.
     */
    if (inq_fn != NULL)
    {
        RPC_MUTEX_UNLOCK (obj_mutex);
        (*inq_fn) (object_uuid, type_uuid, status);
        if (*status == rpc_s_object_not_found)
        {
            UUID_CREATE_NIL (type_uuid);
        }
        return;
    }

    UUID_CREATE_NIL (type_uuid);

    RPC_MUTEX_UNLOCK (obj_mutex);
    *status = rpc_s_object_not_found;
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_object_set_inq_fn
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**
**  Supply a function that is called by the runtime to determine the type
**  of objects that have not been set by "rpc_object_set_type".
**
**  INPUTS:
**
**      inq_fn          function that does the inquiry
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
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_object_set_inq_fn 
#ifdef _DCE_PROTO_
(
    rpc_object_inq_fn_t     inq_fn_arg,
    unsigned32              *status
)
#else
(inq_fn_arg, status)
rpc_object_inq_fn_t     inq_fn_arg;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    inq_fn = inq_fn_arg;
    *status = rpc_s_ok;
}

