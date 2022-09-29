/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: combind.c,v 65.5 1998/03/24 16:00:07 lmc Exp $";
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
 * $Log: combind.c,v $
 * Revision 65.5  1998/03/24 16:00:07  lmc
 * Fix some of the #ifdef's for SGI_RPC_DEBUG.  Also typecast a few
 * variables to fix warnings.
 *
 * Revision 65.4  1998/03/23  17:30:50  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:43  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:43  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.60.2  1996/02/18  00:02:21  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:55:02  marty]
 *
 * Revision 1.1.60.1  1995/12/08  00:18:02  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:30 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/05/25  21:40 UTC  lmm
 * 	Merge allocation failure detection cleanup work
 * 
 * 	HP revision /main/HPDCE02/lmm_alloc_fail_clnup/1  1995/05/25  21:00 UTC  lmm
 * 	allocation failure detection cleanup
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:13 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:18 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:14 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/16  14:28 UTC  markar
 * 	Implementing Local RPC mods
 * 	[1995/12/07  23:57:59  root]
 * 
 * Revision 1.1.58.8  1994/08/02  20:06:49  mori_m
 * 	CR 9701:  Added code set i14y support to rpc_binding_copy().
 * 	[1994/08/02  20:03:56  mori_m]
 * 
 * Revision 1.1.58.7  1994/05/04  21:52:53  mori_m
 * 	CR 10341:  Correction to the previous fix.  'i18n flag' should not be
 * 		   reset within rpc_binding_reset.
 * 	[1994/05/04  21:02:37  mori_m]
 * 
 * Revision 1.1.58.6  1994/04/13  21:05:39  mori_m
 * 	CR 10341  Added initialization for i18n flag within rpc_binding_reset and
 * 		  rpc__binding_alloc.
 * 		  Added i18n flag copy within rpc_binding_copy.
 * 		  Added #ifndef _KERNEL around i18n cleanup for rpc__binding_free.
 * 	[1994/04/13  21:04:32  mori_m]
 * 
 * Revision 1.1.58.5  1994/04/12  16:51:59  mori_m
 * 	CR 9701:  Second submission for RPC runtime I18N support.
 * 		  Added code set I14Y functionaltiy to rpc__binding_free.
 * 	[1994/04/12  16:44:34  mori_m]
 * 
 * Revision 1.1.58.4  1994/03/28  19:34:58  tom
 * 	Fix typo: make sure comment is terminated in rpc__binding_free.
 * 	[1994/03/28  19:34:45  tom]
 * 
 * Revision 1.1.58.3  1994/03/21  19:26:11  tom
 * 	Place rpc__binding_set_prot_version under ifndef _KERNEL.
 * 	[1994/03/21  19:25:48  tom]
 * 
 * Revision 1.1.58.2  1994/03/17  23:06:41  tom
 * 	Changes for protocol version in the binding handle:
 * 	  Add rpc__binding_prot_version_alloc/rpc__binding_prot_version_free.
 * 	  Add rpc__binding_set_prot_version.
 * 	  Free the protocol stuct in binding free.
 * 	  Set the protocol struct to NULL in binding_alloc.
 * 	  Copy the protocol struct in binding_copy.
 * 	[1994/03/17  23:04:23  tom]
 * 
 * Revision 1.1.58.1  1994/01/21  22:35:05  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:54:56  cbrooks]
 * 
 * Revision 1.1.4.4  1993/03/08  18:35:23  weisman
 * 	Fix for OT 7369
 * 	[1993/03/07  18:47:21  weisman]
 * 
 * Revision 1.1.4.3  1993/01/03  22:59:50  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:17  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:19:12  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:32:57  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:01:26  rsalz
 * 	  5-mar-92 wh        DCE 1.0.1 merge.
 * 	 07-jan-92 ouellette rpc__auth_info_release => rpc__auth_info_binding_release
 * 	[1992/05/01  16:49:12  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:09:45  devrcs
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
**      combind.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of the Binding Services for the Common Communication
**  Services component. These routines are called by applications to
**  manipulate Binding Rep data structures required by the runtime.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */
#include <comp.h>       /* Private communications services */
#include <dce/assert.h>		/* DCE1.1 assert macro */
#include <cs_s.h>	/* I18N codesets definitions */



/*
**++
**
**  ROUTINE NAME:       rpc__binding_free
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  This routine will really free the Binding Rep memory - this routine
**  should not be called directly; RPC_BINDING_RELEASE() should be used.
**  Since a Binding Rep's size is RPC Protocol Service-specific the RPC
**  Protocol Service is called to do the actual free.
**
**  INPUTS:
**
**      binding_rep     The binding rep pointer which points to the binding
**                      rep data structure to be freed.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

PRIVATE void rpc__binding_free 
#ifdef _DCE_PROTO_
(
  rpc_binding_rep_p_t     *binding_rep_p,
  unsigned32              *status
)
#else
(binding_rep_p, status)
rpc_binding_rep_p_t     *binding_rep_p;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = *binding_rep_p;
    unsigned32		    temp_status = rpc_s_ok;
#if defined(SGIMIPS) && !defined(KERNEL)
    rpc_cs_method_eval_p_t  method_p;
#endif /* SGIMIPS && !KERNEL */
#ifndef SGIMIPS			/* Not used */
    rpc_cs_tags_eval_p_t    tags_p;
#endif /* SGIMIPS */

    CODING_ERROR (status);

    /*
     * The handle must be locked so that we can "atomically"
     * free it.
     */
    RPC_LOCK_ASSERT(0);
    
    /*
     * see if an RPC address exists in this binding
     */
    if (binding_rep->rpc_addr != NULL)
    {
        /*
         * if so, ask the NAF extension service to free the RPC address
         */
        (*rpc_g_naf_id[binding_rep->rpc_addr->sa.family].epv
            ->naf_addr_free) (&(binding_rep->rpc_addr), status);
        if (*status != rpc_s_ok) return;
    }

    /*
     * see if we have a protocol version
     */
    if (binding_rep->protocol_version != NULL)
    {
        rpc__binding_prot_version_free(&(binding_rep->protocol_version));
    }

#ifndef _KERNEL
    /*
     * check code sets I14Y
     */
    if (binding_rep->extended_bind_flag == RPC_C_BH_EXTENDED_CODESETS)
    {
	/* Release codesets relating binding information.
	 * Determine the data structure
	 */
	switch (binding_rep->cs_eval.key)
	{
	case RPC_CS_EVAL_METHOD:
		method_p = &binding_rep->cs_eval.tagged_union.method_key;

		if (method_p->server != NULL)
			rpc_ns_mgmt_free_codesets (
				&(method_p->server),
				&temp_status );

		if (method_p->client != NULL)
			rpc_ns_mgmt_free_codesets (
				&(method_p->client),
				&temp_status );
		break;

	case RPC_CS_EVAL_TAGS:

		break;

	default:
		temp_status = rpc_s_ss_invalid_codeset_tag;
		break;
	}
    }
#endif

    /*
     * if we have any authentication info, free it up now.
     */
    rpc__auth_info_binding_release(binding_rep);
    
    /*
     * Free the name service-specific part of the binding.
     */
    if (binding_rep->ns_specific != NULL)
        (*rpc_g_ns_specific_free_fn) (&binding_rep->ns_specific);

    /*
     * then ask the protocol service to free the binding rep and
     * NULL the reference.
     */
    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_free) (binding_rep_p, status);

    if ((temp_status != rpc_s_ok) && (*status == rpc_s_ok))
	*status = temp_status;
}


/*
**++
**
**  ROUTINE NAME:       rpc_binding_free
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will release the reference on the Binding Rep and free
**  the rep when no more references exist.  A NULL pointer will be
**  returned.  Most runtime internal operations that want to release a binding
**  handle reference should probably use RPC_BINDING_RELEASE(), not this
**  routine.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be freed.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_free 
#ifdef _DCE_PROTO_
(
  rpc_binding_handle_t    *binding_h,
  unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    *binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) *binding_h;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * We need to lock the handle so that we can "atomically"
     * release it.
     */
    RPC_LOCK(0);

    /*
     * Release and NULL the reference (possibly free the handle).
     */
    RPC_BINDING_RELEASE((rpc_binding_rep_p_t *) binding_h, status);

    RPC_UNLOCK(0);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_vector_free
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will free the Binding Rep pointed to by each non-NULL
**  entry in the vector array. The vector memory itself is then freed.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      binding_vec     A vector of pointers to binding rep structures.
**                      
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_coding_error
**          rpc_s_invalid_arg  
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

PUBLIC void rpc_binding_vector_free 
#ifdef _DCE_PROTO_
(
  rpc_binding_vector_p_t  *binding_vec,
  unsigned32              *status
)
#else
(binding_vec, status)
rpc_binding_vector_p_t  *binding_vec;
unsigned32              *status;
#endif
{
    unsigned32              i;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    /*
     * check to see if binding_vec is NULL, and if so,
     * return with an error status
     */
    if (binding_vec == NULL)
    {
        *status = rpc_s_invalid_arg;
        return;
    }
                                       
    /*
     * free each element in the vector array (that's non-NULL)
     */
    for (i = 0; i < (*binding_vec)->count; i++)
    {
        if ((*binding_vec)->binding_h[i] != NULL)
        {
            rpc_binding_free (&(*binding_vec)->binding_h[i], status);
            if (*status != rpc_s_ok) return;
        }
    }
    
    /*
     * now free the vector memory itself
     */
    RPC_MEM_FREE (*binding_vec, RPC_C_MEM_BINDING_VEC);

    *binding_vec = NULL;
     
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_set_object
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will set the object UUID field in the Binding Rep to the
**  object UUID given. The RPC Protocol Service identified by the RPC
**  Protocol ID in the Binding Rep will be notified that the Binding Rep
**  has changed.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be modified.
**
**      object_uuid     The unique identifier of an object to which an RPC
**                      may be made.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_set_object 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    uuid_p_t                object_uuid,
    unsigned32              *status
)
#else
(binding_h, object_uuid, status)
rpc_binding_handle_t    binding_h;
uuid_p_t                object_uuid;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * copy the new object UUID into the binding rep
     * (note: a NULL object uuid pointer is treated as a nil uuid)
     */
    if (object_uuid != NULL)
    {
        binding_rep->obj = *object_uuid;
    }
    else
    {
        uuid_create_nil (&(binding_rep->obj), status);
    }

    /*
     * notify the protocol service that the binding has changed
     */
    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_changed) (binding_rep, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_inq_object
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will inquire what the object UUID is in a Binding Rep.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be read.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      object_uuid     The unique identifier of an object to which an RPC
**                      may be made.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_inq_object 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    uuid_t                  *object_uuid,
    unsigned32              *status
)
#else
(binding_h, object_uuid, status)
rpc_binding_handle_t    binding_h;
uuid_t                  *object_uuid;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    *object_uuid = binding_rep->obj;
    
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_reset
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will clear the endpoint in the RPC address of the Binding
**  Rep. The Network Address Family Extension Service identified by the
**  NAF ID in the RPC address will be called to actually clear the endpoint
**  since its format is NAF-specific. Finally the RPC Protocol Service
**  identifed by the RPC Protocol ID in the Binding Rep will be notified
**  that the Binding Rep has changed.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be modified.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_reset 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned32              *status
)
#else
(binding_h, status)
rpc_binding_handle_t    binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    

    RPC_BINDING_VALIDATE_CLIENT(binding_rep, status);
    if (*status != rpc_s_ok)
        return;
      
    /*
     * There is no longer a server instance associated with the binding.
     */

    RPC_LOCK(0);
    binding_rep->bound_server_instance = false;
    RPC_UNLOCK(0);

    /*
     * if binding endpoint is not set, don't do anything
     */
    if (!binding_rep->addr_has_endpoint)
    {
        *status = rpc_s_ok;
        return;
    }
    
    assert(binding_rep->rpc_addr != NULL);
    
    /*
     * Tell the NAF extension service to clear the endpoint. A zero
     * length string *must* be passed in to naf_addr_set_endpoint to
     * clear the endpoint. Do *not* pass in NULL. This will generate a
     * dynamic endpoint in certain nafs.
     */
    (*rpc_g_naf_id[binding_rep->rpc_addr->sa.family].epv
        ->naf_addr_set_endpoint)
            ((unsigned_char_p_t) "", &(binding_rep->rpc_addr), status);
    if (*status != rpc_s_ok) return;

    /*
     * clear the endpoint flag in the binding rep
     */
    binding_rep->addr_has_endpoint = false;
    
    /*
     * notify the protocol service that the binding rep has been reset
     */
    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_reset) (binding_rep, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_copy
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will allocate a new Binding Rep through the RPC Protocol
**  Service. The common part of the Binding Rep will then be filled in and
**  the RPC Protocol Service will be called to initialize the RPC Protocol
**  Service-specific part of the Binding Rep.
**
**  INPUTS:
**
**      src_binding_h   The binding handle which points to the source
**                      binding rep data structure to be copied.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      dst_binding_h   The binding handle which points to the destination
**                      binding rep data structure to be created.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_copy 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    src_binding_h,
    rpc_binding_handle_t    *dst_binding_h,
    unsigned32              *status
)
#else
(src_binding_h, dst_binding_h, status)
rpc_binding_handle_t    src_binding_h;
rpc_binding_handle_t    *dst_binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     src_binding_rep = (rpc_binding_rep_p_t) src_binding_h;
    rpc_binding_rep_p_t     dst_binding_rep;
    rpc_addr_p_t            rpc_addr;
    unsigned32              tmp_status;
    boolean                 have_addr = false;
#if defined(SGIMIPS) && !defined(KERNEL)
    rpc_cs_method_eval_p_t  method_s_p;
    rpc_cs_method_eval_p_t  method_d_p;
    rpc_cs_tags_eval_p_t    tags_s_p;
    rpc_cs_tags_eval_p_t    tags_d_p;
#endif /* SGIMIPS && !KERNEL */


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(src_binding_rep, status);
    if (*status != rpc_s_ok)
        return;
      
    assert(src_binding_rep->rpc_addr != NULL);

    (*rpc_g_naf_id[src_binding_rep->rpc_addr->sa.family].epv
        ->naf_addr_copy)
            (src_binding_rep->rpc_addr, &rpc_addr, status);
    if (*status != rpc_s_ok) return;
    
    have_addr = true;

    /*
     * allocate a binding rep to hold the copy and init it from the source
     */
    dst_binding_rep = rpc__binding_alloc (
        (boolean)src_binding_rep->is_server, &src_binding_rep->obj,
        src_binding_rep->protocol_id, rpc_addr, status);
    if (*status != rpc_s_ok) goto CLEANUP;
    
    /*
     * copy other common parts of the binding rep from the source to dest
     */
    dst_binding_rep->timeout            = src_binding_rep->timeout;
    dst_binding_rep->call_timeout_time  = src_binding_rep->call_timeout_time;
    dst_binding_rep->addr_is_dynamic    = src_binding_rep->addr_is_dynamic;
    dst_binding_rep->bound_server_instance
                                        = src_binding_rep->bound_server_instance;
    dst_binding_rep->extended_bind_flag = src_binding_rep->extended_bind_flag;

    /*
     * Copy the auth info.
     */
    if (src_binding_rep->auth_info != NULL)
    {
        rpc__auth_info_reference (src_binding_rep->auth_info);
        dst_binding_rep->auth_info = src_binding_rep->auth_info;
    }

    /*
     * Copy the protocol version
     */
    if (src_binding_rep->protocol_version != NULL)
    {
        rpc__binding_prot_version_alloc(
                            &(dst_binding_rep->protocol_version),
                            src_binding_rep->protocol_version->major_version, 
                            src_binding_rep->protocol_version->minor_version, 
                            status);
        if (*status != rpc_s_ok) goto CLEANUP;
    }

    /*
     * Copy the ns_specific. 
     */
    if (src_binding_rep->ns_specific != NULL)
    {
        /* 		NOTE
         *  This needs to be replaced with the real copy.
         *  For now set the field to NULL so later we don't improperly 
         *  try to free the field.
         *  
         *  When we get around to doing this, see how this field is
         *  freed in rpc_binding_free for a hint on doing this.
         *  You'll probably need to add a similar entry point in nsinit.c
	 */
        dst_binding_rep->ns_specific = NULL;
    }

    /*
     * Copy the extended code set i14y information
     */
#ifndef _KERNEL
    if (src_binding_rep->extended_bind_flag == RPC_C_BH_EXTENDED_CODESETS)
    {
	/* Copy codesets relating binding information.
	 * Determine the data structure
	 */
	switch (src_binding_rep->cs_eval.key)
	{
	case RPC_CS_EVAL_METHOD:
		method_s_p = &src_binding_rep->cs_eval.tagged_union.method_key;
		method_d_p = &dst_binding_rep->cs_eval.tagged_union.method_key;

		dst_binding_rep->cs_eval.key = src_binding_rep->cs_eval.key;
		method_d_p->method = method_s_p->method;
		method_d_p->tags.stag = method_s_p->tags.stag;
		method_d_p->tags.drtag = method_s_p->tags.drtag;
		method_d_p->tags.stag_max_bytes 
					= method_s_p->tags.stag_max_bytes;
		method_d_p->tags.client_tag = method_s_p->tags.client_tag;
		method_d_p->tags.client_max_bytes 
					= method_s_p->tags.client_max_bytes;
		method_d_p->tags.type_handle = method_d_p->tags.type_handle;
		method_d_p->fixed = method_s_p->fixed;
		method_d_p->cs_stub_eval_func = method_s_p->cs_stub_eval_func;

		method_s_p->server = NULL;
		method_d_p->client = NULL;

		break;

	case RPC_CS_EVAL_TAGS:
		tags_s_p = &src_binding_rep->cs_eval.tagged_union.tags_key;
		tags_d_p = &dst_binding_rep->cs_eval.tagged_union.tags_key;

		dst_binding_rep->cs_eval.key = src_binding_rep->cs_eval.key;
		tags_d_p->stag = tags_s_p->stag;
		tags_d_p->drtag = tags_s_p->drtag;
		tags_d_p->stag_max_bytes = tags_s_p->stag_max_bytes;
		tags_d_p->client_tag = tags_s_p->client_tag;
		tags_d_p->client_max_bytes = tags_s_p->client_max_bytes;
		tags_d_p->type_handle = tags_d_p->type_handle;

		break;

	default:
		*status = rpc_s_ss_invalid_codeset_tag;
		break;
	}
    }
#endif /* _KERNEL */
    
    /*
     * return the destination binding rep as a binding handle
     */
    *dst_binding_h = (rpc_binding_handle_t) dst_binding_rep;

    /*
     * let the protocol service copy any stuff it wants to
     */
    (*rpc_g_protocol_id[src_binding_rep->protocol_id].binding_epv
        ->binding_copy) (src_binding_rep, dst_binding_rep, status);

CLEANUP:

    if (*status != rpc_s_ok && have_addr)
        rpc__naf_addr_free (&rpc_addr, &tmp_status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_to_string_binding
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will convert a Binding Rep data structure to its string
**  represenation, which is called a "string binding". It will first convert
**  the object UUID contained in the Binding Rep to string format. The string
**  format of the Protocol Sequence ID contained in the RPC Address will be
**  looked up in the RPC Protocol Sequence ID table. It will the call the
**  appropriate Network Address Family Extension Service to return the
**  endpoint, network address and network options form the RPC Address in the
**  Binding Rep.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be converted.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      string_binding  A string representation of the binding rep data
**                      structure.
**                      
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_to_string_binding 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned_char_p_t       *string_binding,
    unsigned32              *status
)
#else
(binding_h, string_binding, status)
rpc_binding_handle_t    binding_h;
unsigned_char_p_t       *string_binding;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_addr_p_t            rpc_addr = NULL;
    unsigned_char_p_t       object_uuid = NULL;
    unsigned_char_p_t       endpoint = NULL;
    unsigned_char_p_t       netaddr = NULL;
    unsigned_char_p_t       network_options = NULL;
    unsigned32              temp_status;
    

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    /*
     * if the output argument is NULL, don't do anything
     */
    if (string_binding == NULL)
    {
        *status = rpc_s_ok;
        return;
    }

    RPC_BINDING_VALIDATE(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * convert the object UUID in the binding rep to string format
     * (if it is non-nil - otherwise, keep it as a NULL pointer)
     */
    if (!uuid_is_nil (&(binding_rep->obj), status))
    {
        uuid_to_string (&(binding_rep->obj), &object_uuid, status);

        if (*status != uuid_s_ok)
        {
            goto CLEANUP;
        }
    }
    
    /*
     * if the RPC address in the binding is NULL, get one from protocol service
     */
    if (binding_rep->rpc_addr == NULL)
    {
    
        /*
         * get the RPC address from the protocol service
         */
        (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
            ->binding_inq_addr) (binding_rep, &rpc_addr, status);

        if (*status != rpc_s_ok)
        {
            goto CLEANUP;
        }
    }
    else
    {
        /*
         * otherwise use the RPC address in the binding rep
         */
        rpc_addr = binding_rep->rpc_addr;
    }
    
    /*
     * get the endpoint from the network address family extension
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_endpoint)
        (rpc_addr, &endpoint, status);

    if (*status != rpc_s_ok)
    {
        goto CLEANUP;
    }
    
    /*
     * get the network address from the network address family extension
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_netaddr)
        (rpc_addr, &netaddr, status);

    if (*status != rpc_s_ok)
    {
        goto CLEANUP;
    }
    
    /*
     * get the network options from the network address family extension
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_options)
        (rpc_addr, &network_options, status);

    if (*status != rpc_s_ok)
    {
        goto CLEANUP;
    }

    /*
     * got everything - now build the whole string
     * (note: if the object uuid was nil, a NULL will be passed here,
     * and it won't appear in the string binding; also note, we'll
     * return at this point with the status of rpc_string_binding_compose)
     */
    rpc_string_binding_compose (
        object_uuid, rpc_g_protseq_id[rpc_addr->rpc_protseq_id].rpc_protseq,
        netaddr, endpoint, network_options, string_binding, status);


CLEANUP:

    /*
     * if anything failed along the way, or we got here succesfully,
     * cleanup local buffers and return with the last meaningful status
     */
    if (object_uuid != NULL)
    {
        rpc_string_free (&object_uuid, &temp_status);
    }
    
    if (endpoint != NULL)
    {
        rpc_string_free (&endpoint, &temp_status);
    }
    
    if (netaddr != NULL)
    {
        rpc_string_free (&netaddr, &temp_status);
    }
    
    if (network_options != NULL)
    {
        rpc_string_free (&network_options, &temp_status);
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_from_string_binding
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine converts a string binding to a Binding Rep data structure.
**  It will strip the object UUID, RPC Protocol Sequence string, endpoint,
**  network address and network options out of the string binding provided.
**  A Binding Rep will then be allocated through the RPC Protocol Service
**  identified in the RPC Protocol Sequence. An RPC Address will be allocated
**  through the Network Address Family Extension Service identified in the
**  RPC Protocol Sequence. The common fields of the Binding Rep will be set
**  to defaults and the RPC Prtotcol Service will be called to initialize
**  the RPC Protocol Service-specific part of the Binding Rep.
**
**
**  If the string binding contains an endpoint, the addr_is_dynamic field
**  in the binding rep is set to false, indicating a well-known endpoint.
**  (This field is initialized to true in rpc__binding_alloc.)
**
**  INPUTS:
**
**      string_binding  A string representation of the binding rep data
**                      structure.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be created.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          uuid_s_invalid_string_uuid
**                          Object UUID in string binding was invalid.
**          rpc_s_invalid_string_binding
**                          String binding was invalid.
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

PUBLIC void rpc_binding_from_string_binding 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       string_binding,
    rpc_binding_handle_t    *binding_h,
    unsigned32              *status
)
#else
(string_binding, binding_h, status)
unsigned_char_p_t       string_binding;
rpc_binding_handle_t    *binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep;
    uuid_t                  obj_uuid;
    rpc_addr_p_t            rpc_addr = NULL;
    unsigned_char_p_t       string_object_uuid = NULL;
    unsigned_char_p_t       protseq = NULL;
    unsigned_char_p_t       endpoint = NULL;
    unsigned_char_p_t       netaddr = NULL;
    unsigned_char_p_t       network_options = NULL;
    rpc_protseq_id_t        protseq_id;
    rpc_protocol_id_t       protocol_id;
    rpc_naf_id_t            naf_id;
    unsigned32              temp_status;
    

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    /*
     * extract the various fields from the string binding
     */
    rpc_string_binding_parse (string_binding, &string_object_uuid,
        &protseq, &netaddr, &endpoint, &network_options, status);

    if (*status != rpc_s_ok)
    {
        goto CLEANUP;
    }
    
    /*
     * Convert the protocol sequence string to and protseq ID.
     */
    protseq_id = rpc__network_pseq_id_from_pseq (protseq, status);

    if (*status != rpc_s_ok)
    {
        goto CLEANUP;
    }
         
    if (rpc_g_protseq_id[protseq_id].supported == false)
    {            
        *status = rpc_s_protseq_not_supported;
        goto CLEANUP;
    }


    protocol_id = rpc_g_protseq_id[protseq_id].rpc_protocol_id;
    naf_id      = rpc_g_protseq_id[protseq_id].naf_id;

    /*
     * set the object UUID to the one given in the string
     * - if the string is empty, none was given - use the nil uuid
     */
    if (*string_object_uuid != '\0')
    {
        uuid_from_string (string_object_uuid, &obj_uuid, status);
    }
    else
    {
        uuid_create_nil (&obj_uuid, status);
    }

    if (*status != uuid_s_ok)
    {
        goto CLEANUP;
    }

    /*
     * A NULL network address means to bind to the local host.
     * Otherwise, get an RPC address from the Network Address Family 
     * Extension Service.
     */
    if (*netaddr == '\0')
    {
        rpc__network_inq_local_addr (protseq_id, endpoint, &rpc_addr, status);
    }
    else
    {
        (*rpc_g_naf_id[naf_id].epv->naf_addr_alloc)
            (protseq_id, naf_id, endpoint, netaddr, network_options,
                &rpc_addr, status);
    }

    if (*status != rpc_s_ok) 
    {
        rpc_addr = NULL;
        goto CLEANUP;
    }

    /*
     * allocate a binding rep and initialize it
     */
    binding_rep = rpc__binding_alloc
        ((boolean)false, &obj_uuid, protocol_id, rpc_addr, status);

    if (*status != rpc_s_ok) goto CLEANUP;

    /*
     * A non-NULL endpoint means that the endpoint is well-known, so set
     * the addr_is_dynamic field to false.
     */
    if (*endpoint != '\0')
    {
        binding_rep->addr_is_dynamic = false;
    }
    
    /*
     * cast the binding handle to a binding rep pointer
     */
    *binding_h = (rpc_binding_handle_t) binding_rep;

    *status = rpc_s_ok;


CLEANUP:

    if (string_object_uuid != NULL)
    {
        rpc_string_free (&string_object_uuid, &temp_status);
    }
    
    if (protseq != NULL)
    {
        rpc_string_free (&protseq, &temp_status);
    }
    
    if (endpoint != NULL)
    {
        rpc_string_free (&endpoint, &temp_status);
    }
    
    if (netaddr != NULL)
    {
        rpc_string_free (&netaddr, &temp_status);
    }

    if (network_options != NULL)
    {
        rpc_string_free (&network_options, &temp_status);
    }

    if (*status != rpc_s_ok)
    {
        unsigned32     tstatus;

        if (rpc_addr != NULL)
        {
            (*rpc_g_naf_id[naf_id].epv->naf_addr_free) (&rpc_addr, &tstatus);
        }

        if (*status == rpc_s_invalid_binding)
        {
            *status = rpc_s_invalid_string_binding;
        }

        *binding_h = NULL;
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc_string_binding_parse
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will split the string binding provided into multiple
**  components. 
**
**  INPUTS:
**
**      string_binding  A string representation of the binding rep data
**                      structure.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      string_obj_uuid A string representation of an object UUID.
**
**      protseq         An RPC Protocol Sequence.
**
**      netaddr         A Network Address.
**
**      endpoint        An RPC Endpoint.
**
**      network_options A string of Network Options.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_string_binding 
**                          The string binding could not be parsed
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

PUBLIC void rpc_string_binding_parse 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       string_binding,
    unsigned_char_p_t       *string_object_uuid,
    unsigned_char_p_t       *protseq,
    unsigned_char_p_t       *netaddr,
    unsigned_char_p_t       *endpoint,
    unsigned_char_p_t       *network_options,
    unsigned32              *status
)
#else
(string_binding, string_object_uuid, protseq, netaddr, endpoint, 
 network_options, status)
unsigned_char_p_t       string_binding;
unsigned_char_p_t       *string_object_uuid;
unsigned_char_p_t       *protseq;
unsigned_char_p_t       *netaddr;
unsigned_char_p_t       *endpoint;
unsigned_char_p_t       *network_options;
unsigned32              *status;
#endif
{
#define RPC_C_NETWORK_OPTIONS_MAX   1024

    unsigned_char_p_t       binding_ptr;
    unsigned_char_p_t       option_ptr;
    unsigned32              count;
    boolean                 get_endpoint;
    unsigned32              temp_status;
    unsigned32              len;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * make sure we have something to start with
     */
    if (string_binding == NULL)
    {
        *status = rpc_s_invalid_string_binding;
        return;
    }
                                         
    /*
     * Initialize netaddr, endpoint and network_options if pointers are non-NULL
     */
    if (netaddr != NULL)
    {
        *netaddr = NULL;
    }
    if (endpoint != NULL)
    {
        *endpoint = NULL;
    }
    if (network_options != NULL)
    {
        *network_options = NULL;
    }
    if (string_object_uuid != NULL)
    {
        *string_object_uuid = NULL;
    }
    if (protseq != NULL)
    {
        *protseq = NULL;
    }

    binding_ptr = string_binding;
    
    /*
     * get in the object UUID and protocol sequence
     */
    count = rpc__strcspn (binding_ptr, "@");
    if (string_object_uuid != NULL) 
    {
        {
            RPC_MEM_ALLOC (
                *string_object_uuid,
                unsigned_char_p_t,
                count + 1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*string_object_uuid == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            if (count > 0)
            {
                rpc__strncpy (*string_object_uuid, binding_ptr, count - 1);
            }
            else
            {
                **string_object_uuid = '\0';
            }
        }
    }
    /*
     * In any case, advance the pointer.
     */
    binding_ptr += count;
    
    count = rpc__strcspn (binding_ptr, ":");
    if (protseq != NULL)
    {
        {
            RPC_MEM_ALLOC (
                *protseq,
                unsigned_char_p_t,
                count + 1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*protseq == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            if (count > 0)
            {
                rpc__strncpy (*protseq, binding_ptr, count - 1);
            }
            else
            {
                **protseq = '\0';
            }
        }
    }
    /*
     * In any case, advance the pointer.
     */
    binding_ptr += count;

    /*
     * see if there are options after the network address
     */
    if ((count = rpc__strcspn (binding_ptr, "[")) == 0)
    {            
        /*
         * if there was no '[' terminator, maybe there are no options -
         * but there still might be a network address - go for it!
         */
        if (netaddr != NULL)
        {
#ifdef SGIMIPS
            len = (unsigned32)strlen ((char *) binding_ptr);
#else
            len = strlen ((char *) binding_ptr);
#endif /* SGIMIPS */
            RPC_MEM_ALLOC (
                *netaddr,
                unsigned_char_p_t,
                len + 1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*netaddr == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            /*
             * Note: we are counting on null termination here, even if len == 0
             */
            rpc__strncpy (*netaddr, binding_ptr, len);
        }
    }
    else
    {
        /*
         * if there are options, first get the network address
         */
        if (netaddr != NULL)
        {
            RPC_MEM_ALLOC (
                *netaddr,
                unsigned_char_p_t,
                count,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*netaddr == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            rpc__strncpy (*netaddr, binding_ptr, count - 1);
        }
        binding_ptr += count;

        /*
         * then get the entries in the options list one by one
         */
        if (network_options != NULL)
        {
            /*
             * we've got to allocate enough space to catch all of the
             * network options that might conceivably be here - a lot
             * will probably be wasted, but we have no way of knowing at
             * this time exactly how much, but we know it can be no more than
             * the length of what remains of this string binding.
             */

              /*
               * Allocate one byte more than we need to cover bug -
               * see modification history for 21-may-91.
               */
            RPC_MEM_ALLOC (
                *network_options,
                unsigned_char_p_t,
#ifdef SGIMIPS
                (unsigned32)strlen ((char *)binding_ptr) + 1,
#else
                strlen ((char *)binding_ptr) + 1,
#endif /* SGIMIPS */
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*network_options == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            option_ptr = *network_options;
            *option_ptr = '\0';
        }
        
        while ((count > 0) && (*binding_ptr != '\0'))
        {
            get_endpoint = false;
            
            /*
             * first see if there's an option tag
             */
            count = rpc__strcspn (binding_ptr, "=,");

            if (count == 0 || *(binding_ptr + count - 1) != '=')
            {
                /*
                 * if there's no option tag, this must be an endpoint
                 */
                get_endpoint = true;
            }
            else
            {
                /*
                 * see if the tag is an endpoint specifier -
                 * but skip over any leading white space
                 */
                for (; (*binding_ptr == ' ' || *binding_ptr == '\t') &&
                    *binding_ptr != '\0'; binding_ptr++, count--);

                if ((strncmp ((char *) binding_ptr, "endpoint", (count-1))) 
                    == 0)
                {
                    /*
                     * if the tag was "endpoint", skip to the equal sign
                     */
                    get_endpoint = true;
                    binding_ptr += count;
                }
            }

            /*
             * if this is an endpoint, collect it
             */
            if (get_endpoint && endpoint != NULL)
            {
                count = rpc__strcspn (binding_ptr, ",]");

                /*
                 * We want to return an error if 1) there is no closing ']';
                 * 2) there are more than one endpoints specified (this will
                 * also catch an endpoint specified with extra brackets, 
                 * like "[[foo]]" as a side effect).
                 */
                if ((count == 0) || (*endpoint != NULL))
                {
                    *status = rpc_s_invalid_string_binding;
                    goto CLEANUP;
                }

                RPC_MEM_ALLOC (
                    *endpoint,
                    unsigned_char_p_t,
                    count,
                    RPC_C_MEM_STRING,
                    RPC_C_MEM_WAITOK);
		if (*endpoint == NULL){
		    *status = rpc_s_no_memory;
		    goto CLEANUP;
		}

                if (count > 0)
                {
                    /*
                     * copy the string, filtering out escape chars
                     */
                    int i;
                    unsigned_char_p_t   p1, p2;
                    for (i = 0, p1 = *endpoint, p2 = binding_ptr;
                                i < (count - 1);
                                i++, p2++)
                    {
                        if (*p2 != '\\') 
                        {
                            *p1++ = *p2;
                        }
                    }

                    *p1 = '\0';
                }
                else 
                {
                    **endpoint = '\0';
                }

                binding_ptr += count;
            }
            else if (get_endpoint && endpoint == NULL)
            /*
             * Skip over the endpoint
             */
            {
                count = rpc__strcspn (binding_ptr, ",]");
                binding_ptr += count;
            }
            else
            {
                count = rpc__strcspn (binding_ptr, ",]");
                /*
                 * If this is the last token, make sure the string is properly terminated.
                 */
                if ((count == 0) && ((strlen ((char *) binding_ptr)) != 0))
                {
                    *status = rpc_s_invalid_string_binding;
                    goto CLEANUP;
                }

                /*
                 * if not an endpoint, collect the option tag and value
                 */
                if (network_options != NULL)
                {
                    /*
                     * We can have any number of network options, but we'll
                     * lose the last one if the string doesn't have a 
                     * terminator.
                     * (this code is probably redundant, now)
                     */
                    if (count == 0)
                    {
                        *status = rpc_s_invalid_string_binding;
                        goto CLEANUP;
                    }

                    rpc__strncpy (option_ptr, binding_ptr, count);
                    option_ptr += count;
                }

                /*
                 * advance to the next token in the string.
                 */
                binding_ptr += count;
            }
        }
    }
    

    /*
     * if component strings were created, compress white space out of the
     * results - otherwise, return empty strings for things that were wanted
     * but not found
     */
    if (string_object_uuid != NULL)
    {
        if (*string_object_uuid != NULL)
        {
            rpc__strsqz (*string_object_uuid);
        }
        else
        {
            RPC_MEM_ALLOC (
                *string_object_uuid,
                unsigned_char_p_t,
                1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*string_object_uuid == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            **string_object_uuid = '\0';
        }
    }

    if (protseq != NULL)
    {
        if (*protseq != NULL)
        {
            rpc__strsqz (*protseq);
        }
        else
        {
            RPC_MEM_ALLOC (
                *protseq,
                unsigned_char_p_t,
                1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*protseq == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            **protseq = '\0';
        }
    }

    if (netaddr != NULL)
    {
        if (*netaddr != NULL)
        {
            rpc__strsqz (*netaddr);
        }
        else
        {
            RPC_MEM_ALLOC (
                *netaddr,
                unsigned_char_p_t,
                1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*netaddr == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            **netaddr = '\0';
        }
    }

    if (endpoint != NULL)
    {
        if (*endpoint != NULL)
        {
            rpc__strsqz (*endpoint);
        }
        else
        {
            RPC_MEM_ALLOC (
                *endpoint,
                unsigned_char_p_t,
                1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*endpoint == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            **endpoint = '\0';
        }
    }

    if (network_options != NULL)
    {
        if (*network_options != NULL)
        {
            rpc__strsqz (*network_options);

            /*
             * clip the trailing separator off the network options string
             * to be neat
             */
#ifdef SGIMIPS
            if ((count = (unsigned32)strlen ((char *) *network_options)) > 0)
#else
            if ((count = strlen ((char *) *network_options)) > 0)
#endif /* SGIMIPS */
            {
                (*network_options)[count - 1] = '\0';
            }
        }
        else
        {
            RPC_MEM_ALLOC (
                *network_options,
                unsigned_char_p_t,
                1,
                RPC_C_MEM_STRING,
                RPC_C_MEM_WAITOK);
	    if (*network_options == NULL){
		*status = rpc_s_no_memory;
		goto CLEANUP;
	    }

            **network_options = '\0';
        }
    }


    *status = rpc_s_ok;
    return;


CLEANUP:

    /*
     * free any buffers that have been allocated
     */
    if (string_object_uuid != NULL && *string_object_uuid != NULL)
    {
        rpc_string_free (string_object_uuid, &temp_status);
    }

    if (protseq != NULL && *protseq != NULL)
    {
        rpc_string_free (protseq, &temp_status);
    }

    if (netaddr != NULL && *netaddr != NULL)
    {
        rpc_string_free (netaddr, &temp_status);
    }

    if (endpoint != NULL && *endpoint != NULL)
    {
        rpc_string_free (endpoint, &temp_status);
    }

    if (network_options != NULL && *network_options != NULL)
    {
        rpc_string_free (network_options, &temp_status);
    }

    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_string_binding_compose
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  This routine will combine the components of a string binding and
**  return a formatted string binding.
**
**  INPUTS:
**
**      string_obj_uuid A string representation of an object UUID.
**
**      protseq         An RPC Protocol Sequence.
**
**      netaddr         A Network Address.
**
**      endpoint        An RPC Endpoint.
**
**      network_options A string of Network Options.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      string_binding  A string representation of the binding rep data
**                      structure.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_string_binding_compose 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       string_object_uuid,
    unsigned_char_p_t       protseq,
    unsigned_char_p_t       netaddr,
    unsigned_char_p_t       endpoint,
    unsigned_char_p_t       network_options,
    unsigned_char_p_t       *string_binding,
    unsigned32              *status
)
#else
(string_object_uuid, protseq, netaddr,endpoint, 
 network_options, string_binding, status)
unsigned_char_p_t       string_object_uuid;
unsigned_char_p_t       protseq;
unsigned_char_p_t       netaddr;
unsigned_char_p_t       endpoint;
unsigned_char_p_t       network_options;
unsigned_char_p_t       *string_binding;
unsigned32              *status;
#endif
{
    unsigned_char_p_t   string_binding_ptr;
    unsigned32          string_binding_size = 1;
    

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    /*
     * if the output argument is NULL, don't do anything
     */
    if (string_binding == NULL)
    {
        *status = rpc_s_ok;
        return;
    }

    /*
     * calculate the total size of the resulting string binding - the sum
     * of the lengths of all the non-null input strings, plus space for
     * any delimiters that might possibly be needed (and the requisite
     * null terminator)
     */
    if ((string_object_uuid != NULL) && (*string_object_uuid != '\0'))
    {
        string_binding_size += strlen ((char *) string_object_uuid) + 1;
    }

    if (protseq != NULL)
    {
        string_binding_size += strlen ((char *) protseq) + 1;
    }
    
    if (netaddr != NULL)
    {
        string_binding_size += strlen ((char *) netaddr) + 1;
    }
    
    if (endpoint != NULL)
    {
        string_binding_size += strlen ((char *) endpoint) + 2;
    }
    
    if (network_options != NULL)
    {
        string_binding_size += strlen ((char *) network_options) + 2;
    }
    
    /*
     * heap allocate storage for the string binding
     */
    RPC_MEM_ALLOC (
        *string_binding,
        unsigned_char_p_t,
        string_binding_size,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
    if (*string_binding == NULL){
	*status = rpc_s_no_memory;
	return;
    }
    
    string_binding_ptr = *string_binding;
    
    /*
     * fill in the object UUID
     */
    if ((string_object_uuid != NULL) && (*string_object_uuid != '\0'))
    {
        while (*string_object_uuid != '\0')
        {
            *(string_binding_ptr++) = *(string_object_uuid++);
        }

        *(string_binding_ptr++) = '@';
    }

    /*
     * fill in the protocol sequence
     */
    if (protseq != NULL)
    {
        while (*protseq != '\0')
        {
            *(string_binding_ptr++) = *(protseq++);
        }

        *(string_binding_ptr++) = ':';
    }

    /*
     * fill in the network address
     */
    if (netaddr != NULL)
    {
        while (*netaddr != '\0')
        {
            *(string_binding_ptr++) = *(netaddr++);
        }
    }

    if (endpoint != NULL || network_options != NULL)
    {
        *(string_binding_ptr++) = '[';

        /*
         * fill in the endpoint
         */
        if (endpoint != NULL)
        {
            while (*endpoint != '\0')
            {
                *(string_binding_ptr++) = *(endpoint++);
            }

            if (network_options != NULL && *network_options != '\0')
            {
                 *(string_binding_ptr++) = ',';
            }
        }

        /*
         * fill in any other options that might be specified
         */
        if (network_options != NULL)
        {
            while (*network_options != '\0')
            {
                *(string_binding_ptr++) = *(network_options++);
            }
        }

        *(string_binding_ptr++) = ']';
    }
    
    /*
     * terminate the string
     */
    *(string_binding_ptr) = '\0';
    
    *status = rpc_s_ok;
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc__binding_alloc
**
**  SCOPE:              PRIVATE - declared in combind.h
**
**  DESCRIPTION:
**      
**  This routine will allocate the memory for a Binding Rep data structure
**  through the RPC Protocol Service identified in the input arguments.
**  All the common fields of the Binding Rep will be initialized using
**  defaults and the input arguments to this routine. The RPC Protocol
**  Service will then be called to initialize the RPC Protocol Service
**  specific parts of the Binding Rep.
**
**  INPUTS:
**
**      is_server       T => a server-side binding handle should be created
**
**      object_uuid     The unique identifier of an object to which an
**                      RPC may be made.
**
**      protocol_id     The identifier of an RPC Protocol Service through
**                      which an RPC may be made. Since this is an internal
**                      routine it assumes this has been checked by the
**                      caller to see if it is a valid identifier.
**
**      rpc_addr        The location of the remote half of the RPC.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      binding_rep     A Binding Rep data structure containing the object
**                      UUID of an RPC as well as the location of the remote
**                      half of the RPC.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_binding_rep_t *rpc__binding_alloc 
#ifdef _DCE_PROTO_
(
    boolean32               is_server,
    uuid_p_t                object_uuid,
    rpc_protocol_id_t       protocol_id,
    rpc_addr_p_t            rpc_addr,
    unsigned32              *status
)
#else
(is_server, object_uuid, protocol_id, rpc_addr, status)
boolean32               is_server;
uuid_p_t                object_uuid;
rpc_protocol_id_t       protocol_id;
rpc_addr_p_t            rpc_addr;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     binding_rep;
    unsigned_char_p_t       endpoint = NULL;
    unsigned32              temp_status;
    

    CODING_ERROR (status);

    /*
     * get protocol service to allocate an empty binding rep
     */
    binding_rep = (*rpc_g_protocol_id[protocol_id].binding_epv
        ->binding_alloc) (is_server, status);
    if (binding_rep == NULL)
	return(NULL);
    
    /*
     * Initialize the common part of the binding rep.
     */
    binding_rep->is_server = is_server;
    binding_rep->protocol_id = protocol_id;
    binding_rep->obj = *object_uuid;
    binding_rep->timeout = rpc_c_binding_default_timeout;
    binding_rep->ns_specific = NULL;
    binding_rep->auth_info = NULL;
    binding_rep->bound_server_instance = false;
    binding_rep->addr_has_endpoint = false;
    binding_rep->refcnt = 1;            /* the reference we are returning */
    binding_rep->calls_in_progress = 0;
    binding_rep->call_timeout_time = 0;
    binding_rep->fork_count = rpc_g_fork_count;
    binding_rep->protocol_version = NULL;
    binding_rep->extended_bind_flag = RPC_C_BH_EXTENDED_NONE;

    /*
     * By default, created bindings are declared to have dynamic
     * addresses; *this* default state is important.  Callers of
     * binding_alloc that know they're associating a non-dynamic
     * addr with the binding can change this state.
     */
    binding_rep->addr_is_dynamic = true;

    if (rpc_addr != NULL)
    {
        /*
         * find out if the RPC address contains an endpoint
         */
        (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_endpoint)
            (rpc_addr, &endpoint, status);

        /*
         * If the addr has an endpoint, tell everyone.
         */
        if (*status != rpc_s_ok)
        {
            /*
             * then ask the protocol service to free the binding rep and
             * NULL the reference.
             */
            (*rpc_g_protocol_id[protocol_id].binding_epv->binding_free) 
                (&binding_rep, &temp_status);
            return (NULL);
        }
        else
        {
            if (strlen ((char *) endpoint) != 0)
            {
                binding_rep->addr_has_endpoint = true;
            }
        }
    }

    /*
     * set the pointer to the specified rpc address in the binding
     */
    binding_rep->rpc_addr = rpc_addr;

    /*
     * get the protocol service to initialize the rest of the binding
     */
    (*rpc_g_protocol_id[protocol_id].binding_epv
        ->binding_init) (binding_rep, status);

    *status = rpc_s_ok;

CLEANUP:

    if (endpoint != NULL)
    {
        rpc_string_free (&endpoint, &temp_status);
    }

    return (binding_rep);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_inq_client
**
**  SCOPE:              PUBLIC
**
**  DESCRIPTION:
**               
**  This routine returns a protocol service dependent client handle which
**  can be used by the stubs to identify a particular instance of a
**  particular client process.
**
**  INPUTS:
**
**      binding_h       Server stub binding handle
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
** 
**      client_h        Client handle for use with monitor_liveness routine
**
**      status          A value indicating the status of the routine.
**            rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
**
**
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void rpc_binding_inq_client 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_client_handle_t     *client_h,
    unsigned32              *status
)
#else
(binding_h, client_h, status)
rpc_binding_handle_t    binding_h;
rpc_client_handle_t     *client_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t binding_rep = (rpc_binding_rep_p_t) binding_h;

    CODING_ERROR (status);
    RPC_VERIFY_INIT ();

    RPC_BINDING_VALIDATE_SERVER(binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * Ask the protocol service for a client handle associated with
     * the client on the other end of the connection specified by this
     * server binding handle.
     */                      
    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_inq_client) (binding_rep, client_h, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_handle_copy
**
**  SCOPE:              PUBLIC
**
**  DESCRIPTION:
**               
**  This routine creates a duplicate handle to an existing
**  (now shared) binding (via bumping a reference count).
**
**  INPUTS:
**
**      source_binding  The binding handle which points to the source
**                      binding rep data structure to be copied.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      destination_binding   
**                      The binding handle which points to the shared
**                      binding rep data structure.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

 void rpc_binding_handle_copy 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    source_binding,
    rpc_binding_handle_t    *destination_binding,
    unsigned32              *status
)
#else
(source_binding, destination_binding, status)
rpc_binding_handle_t    source_binding;
rpc_binding_handle_t    *destination_binding;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     src_binding_rep = (rpc_binding_rep_p_t) source_binding;
    rpc_binding_rep_p_t     *dst_binding_rep = 
                                (rpc_binding_rep_p_t *) destination_binding;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_CLIENT(src_binding_rep, status);
    if (*status != rpc_s_ok)
        return;
      
    /*
     * Add a new reference.
     */

    *dst_binding_rep = src_binding_rep;

    RPC_LOCK(0);
    RPC_BINDING_REFERENCE(src_binding_rep);
    RPC_UNLOCK(0);

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_handle_equal
**
**  SCOPE:              PUBLIC
**
**  DESCRIPTION:
**               
**  This routine compares two binding handles to determine if they
**  reference the same binding object.  The current external binding handle
**  representation (a pointer) doesn't really require the assistance
**  of such a function, however, it is anticipated that we will (shortly)
**  change the external representation to something that will allow us
**  to detect dangling references users will not be able to perform a
**  comparison via the C "==" operator.
**
**  INPUTS:
**
**      binding_h1      One binding handle ref.
**      binding_h2      A second binding handle ref.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true if both handle's refer to the same binding rep
**                      false if they don't
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 rpc_binding_handle_equal 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding1,
    rpc_binding_handle_t    binding2,
    unsigned32              *status
)
#else
(binding1, binding2, status)
rpc_binding_handle_t    binding1;
rpc_binding_handle_t    binding2;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    *status = rpc_s_ok;

    return (binding1 == binding2);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_server_to_client
**
**  SCOPE:              PUBLIC - declared in rpcpvt.idl
**
**  DESCRIPTION:
**      
**  This routine provides compatibility for DCE components that were
**  coded prior to the routine name change from rpc_binding_server_to_client()
**  to rpc_binding_server_from_client().
**
**  This routine calls through to the rpc_binding_server_from_client()
**  API routine.
**
**  	NOTE: This description uses old terminology.
**
**  Convert a server binding handle to a client handle.  The new handle's    
**  endpoint is reset and it has no associated authentication information.    
**      
**  Server binding handles are those created by the runtime and provided    
**  to the server manager as a result of a [handle_t] RPC parameter.    
**
**  INPUTS:
**
**      src_binding_h   The binding handle which points to the (server)
**                      source binding rep data structure to be converted.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      dst_binding_h   The binding handle which points to the converted (client)
**                      destination binding rep data structure.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_server_to_client 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    src_binding_h,
    rpc_binding_handle_t    *dst_binding_h,
    unsigned32              *status
)
#else
(src_binding_h, dst_binding_h, status)
rpc_binding_handle_t    src_binding_h;
rpc_binding_handle_t    *dst_binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_server_from_client (src_binding_h, dst_binding_h, status);
    return;
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_server_from_client
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**      NOTE: This description and the code use phrases "server binding
**            handle" and "client binding handle" opposite from the way
**            these are used in the API documentation. At some point, we
**            should change the code to match the documentation.
**
**  Convert a server binding handle to a client handle.  The new handle's    
**  endpoint is reset and it has no associated authentication information.    
**      
**  Server binding handles are those created by the runtime and provided    
**  to the server manager as a result of a [handle_t] RPC parameter.    
**
**  INPUTS:
**
**      src_binding_h   The binding handle which points to the (server)
**                      source binding rep data structure to be converted.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      dst_binding_h   The binding handle which points to the converted (client)
**                      destination binding rep data structure.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_server_from_client 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    src_binding_h,
    rpc_binding_handle_t    *dst_binding_h,
    unsigned32              *status
    )
#else
(src_binding_h, dst_binding_h, status)
rpc_binding_handle_t    src_binding_h;
rpc_binding_handle_t    *dst_binding_h;
unsigned32              *status;
#endif
{
    rpc_binding_rep_p_t     src_binding_rep = (rpc_binding_rep_p_t) src_binding_h;
    rpc_binding_rep_p_t     dst_binding_rep;
    rpc_addr_p_t            rpc_addr;
    unsigned32              tmp_status;
    boolean                 have_addr = false;


    CODING_ERROR (status);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_SERVER(src_binding_rep, status);
    if (*status != rpc_s_ok)
        return;

    /*
     * if the RPC address in the source is NULL, get one from protocol service
     */
    if (src_binding_rep->rpc_addr == NULL)
    {
        rpc_addr_p_t srpc_addr;

        (*rpc_g_protocol_id[src_binding_rep->protocol_id].binding_epv
         ->binding_inq_addr) (src_binding_rep, &srpc_addr, status);

        if (*status != rpc_s_ok) 
            return;

        /*
         *  make a copy of the one in the source
         */
        (*rpc_g_naf_id[src_binding_rep->rpc_addr->sa.family].epv
            ->naf_addr_copy)
                (src_binding_rep->rpc_addr, &rpc_addr, status);

    }
    else
    {
        /*
         * otherwise, make a copy of the one in the source
         */    
        (*rpc_g_naf_id[src_binding_rep->rpc_addr->sa.family].epv
            ->naf_addr_copy)
                (src_binding_rep->rpc_addr, &rpc_addr, status);
        if (*status != rpc_s_ok) return;
    }
    have_addr = true;

    /*
     * The resultant binding's address has no endpoint.  It is absolutely
     * wrong to use the originating client's endpoint.
     */
    rpc__naf_addr_set_endpoint ((unsigned_char_p_t) "", &rpc_addr, status);
    if (*status != rpc_s_ok) goto CLEANUP;

    /*
     * allocate and init a *client* binding rep.
     */
    dst_binding_rep = rpc__binding_alloc (
        false, &src_binding_rep->obj,
        src_binding_rep->protocol_id, rpc_addr, status);
    if (*status != rpc_s_ok) goto CLEANUP;
    
    /*
     * copy other common parts of the binding rep from the source to dest
     *
     * We explicitly leave the dst->auth NULL; the server (using this
     * new client handle) probably wants to make calls as its own
     * identity.
     */
    dst_binding_rep->timeout            = src_binding_rep->timeout;
    dst_binding_rep->call_timeout_time  = src_binding_rep->call_timeout_time;
    dst_binding_rep->addr_is_dynamic    = src_binding_rep->addr_is_dynamic;
    
    /*
     * return the destination binding rep as a binding handle
     */
    *dst_binding_h = (rpc_binding_handle_t) dst_binding_rep;

    *status = rpc_s_ok;

CLEANUP:

    if (*status != rpc_s_ok && have_addr)
        rpc__naf_addr_free (&rpc_addr, &tmp_status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__binding_inq_sockaddr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Inquire the sockaddr that is associated with a binding handle.
**  
**  This is kernel RPC only (private) operation and exists strictly for
**  performance reasons.  The returned sockaddr pointer points to storage
**  directly associated with the binding handle.  This pointer can become
**  invalid under a number of circumstances (e.g. binding handle
**  operations); copy the sa if you need to use it after any binding handle
**  operations!
**
**  Note: Do NOT remove this routine even though it appears that nothing
**  is using it...  it is used by Kernel RPC Applications.
**
**  INPUTS:
**
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be inquired.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      sa              A pointer to the sockaddr contained in the binding.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
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

PRIVATE void rpc__binding_inq_sockaddr 
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    sockaddr_p_t            *sa,
    unsigned32              *status
)
#else
(binding_h, sa, status)
rpc_binding_handle_t    binding_h;
sockaddr_p_t            *sa;
unsigned32              *status;
#endif
{
    *sa = &((rpc_binding_rep_p_t) binding_h)->rpc_addr->sa;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc__binding_cross_fork
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Perform whatever actions are required to make a binding handle usable
**  in the client of a fork.
**                          
**  Currently there is no common processing performed, and we just pass
**  the binding rep to protocol specific routines for processing.
**
**  INPUTS:
**
**      binding_rep     A pointer to a binding rep which is being used
**                      across a fork.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       Any state associated with the binding handle is 
**                      dropped so that it is not inherited by the child
**                      of a fork.
**
**--
**/

PRIVATE void rpc__binding_cross_fork 
#ifdef _DCE_PROTO_
(
  rpc_binding_rep_p_t binding_rep,
  unsigned32 * status 
)
#else
(binding_rep, status)
rpc_binding_rep_p_t binding_rep;                
unsigned32 *status;
#endif
{  
    /*
     * Servers aren't allowed to fork, so we only allow passing
     * client binding handles across the fork.
     */
    if (! RPC_BINDING_IS_CLIENT(binding_rep))
    {
        *status = rpc_s_wrong_kind_of_binding;
        return;
    }
 
    /*
     * We only want to bring the handle across the fork once.
     * Take the global lock, then check that the handle still
     * needs to cross the fork.
     */
    RPC_LOCK(0);  
     
    if (binding_rep->fork_count != rpc_g_fork_count)
    {
        /*
         * Ask the protocol service to clean up any protocol specific
         * state that should not survive in the child of a fork.
         */
        (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
           ->binding_cross_fork) (binding_rep, status);
                
        /*
         * Update the handle to the current fork count.
         */
        binding_rep->fork_count = rpc_g_fork_count;
    }

    RPC_UNLOCK(0);  
}

/*
**++
**  ROUTINE NAME:       rpc__binding_prot_version_alloc
**
**  SCOPE:              PRIVATE
**
**  DESCRIPTION:
**
**  Allocates and initializes an rpc_protocol_version_t struct.
**
**  INPUTS:             
**      major_version       The major version of the protocol
**
**      minor_version       The minor version of the protocol.
**
**
**  OUTPUTS:
**      prot_version        The allocated protocol_version structure.
**
**      status         
**                          rpc_s_ok 
**                          
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
*/
PRIVATE void rpc__binding_prot_version_alloc(
    rpc_protocol_version_p_t    *prot_version,
    unsigned32		        major_version,
    unsigned32		        minor_version,
    unsigned32		        *status)

{

    /*
     * Alocate a version struct
     */
    RPC_MEM_ALLOC(*prot_version, 
                  rpc_protocol_version_p_t,
                  sizeof(rpc_protocol_version_t),
                  RPC_C_MEM_PROTOCOL_VERSION,
                  RPC_C_MEM_WAITOK);
    if (*prot_version == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    /*
     * Initialize data
     */
    (*prot_version)->major_version = major_version;
    (*prot_version)->minor_version = minor_version;

    *status = rpc_s_ok;

    return;
}


/*
**++
**  ROUTINE NAME:       rpc__binding_prot_version_free
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**
**  Free's an rpc_protocol_version_t struct.
**  The pointer is set to NULL.
**
**  INPUTS:             
**    protocol_version  Pointer to the version stuct.
**
**
**  OUTPUTS:            none
**
**  SIDE EFFECTS:       none
**
**--
*/
PRIVATE void rpc__binding_prot_version_free(
    rpc_protocol_version_p_t		*protocol_version)

{
    RPC_MEM_FREE (*protocol_version, RPC_C_MEM_PROTOCOL_VERSION);
    *protocol_version = NULL;

    return;
}

#ifndef _KERNEL

/*
**++
**  ROUTINE NAME:       rpc__binding_set_prot_version
**
**  SCOPE:              PRIVATE - called from nslookup.c
**
**  DESCRIPTION:
**
**  Sets the protocol version in a binding_rep data structure 
**  from a protocol tower.
**
**  INPUTS:             
**      binding_h       The binding handle which points to the binding
**                      rep data structure to be modified.
** 
**      tower_ref	The tower to get the protocol version information
**
**  OUTPUTS:
**      status          return status from called routines.
**
**  SIDE EFFECTS:       none
**
**--
*/
PRIVATE void rpc__binding_set_prot_version(
    rpc_binding_handle_t                binding_h,
    rpc_tower_ref_p_t                   tower_ref,
    unsigned32                          *status)
{
    rpc_binding_rep_p_t     binding_rep;
    rpc_protocol_id_t       temp_protocol_id;
    unsigned32              major_version;
    unsigned32              minor_version;

    /*
     * Get the major and minor version of the tower.
     */
    rpc__tower_flr_to_rpc_prot_id(tower_ref->floor[2],
                                  &temp_protocol_id,
                                  &major_version,
                                  &minor_version,
                                  status);

    if (*status != rpc_s_ok)
    {
        return;
    }

    /*
     * Allocate a protocol structure and initialize it.
     */
    binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc__binding_prot_version_alloc(&(binding_rep->protocol_version),
                                    major_version,
                                    minor_version,
                                    status);

    return;
}
#endif /* _KERNEL */
