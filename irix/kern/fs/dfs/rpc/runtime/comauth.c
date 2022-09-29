/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comauth.c,v 65.7 1999/04/23 16:06:28 gwehrman Exp $";
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
 * $Log: comauth.c,v $
 * Revision 65.7  1999/04/23 16:06:28  gwehrman
 * Ensure that the auth_info_cache_mutex is always locked when incrementing the
 * auth_info->refcount.  Fix for bug 672309.
 *
 * Revision 65.6  1998/09/29 21:08:54  bdr
 * Added the use of the auth_info_cache_mutex lock in
 * rpc__auth_info_release() to protect the refcount usage
 * and testing.  I don't see how it could possibly be a good
 * idea to let this code mess with this refcount without the
 * lock.  This might be the fix for PV 630042.  I have not
 * hit this panic since running with this code in my test kernels.
 *
 * Revision 65.5  1998/03/23  17:28:16  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/09 17:44:40  lmc
 * Cast an unsigned32 as an int so that comparison against 0 is
 * meaningful.
 *
 * Revision 65.3  1998/01/07  17:21:06  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:06  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:45  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.70.1  1996/06/04  21:54:08  arvind
 * 	u2u: Merge implementation of rpc__auth_inq_my_princ_name_tgt() and
 * 	rpc_server_register_auth_ident()
 * 	[1996/05/06  20:19 UTC  burati  /main/DCE_1.2/2]
 *
 * 	merge u2u work
 * 	[1996/04/29  20:25 UTC  burati  /main/DCE_1.2/mb_u2u/1]
 *
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:30 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	merge fix for server_princ_name NULL check
 * 	[1995/09/15  19:57 UTC  lmm  /main/HPDCE02/3]
 *
 * 	add server_princ_name NULL check for alloc fail detection
 * 	[1995/09/15  19:44 UTC  lmm  /main/HPDCE02/lmm_inq_auth_fix/1]
 *
 * 	Submitted the workaround for OT12857.
 * 	[1995/05/18  20:24 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Workaround for auth_info cache miss in rpc_binding_set_auth_info().
 * 	[1995/05/08  20:12 UTC  tatsu_s  /main/HPDCE02/tatsu_s.scale_fix.b0/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:13 UTC  lmm  /main/HPDCE02/1]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:17 UTC  lmm  /main/lmm_rpc_alloc_fail_detect/1]
 *
 * Revision 1.1.68.2  1996/02/18  00:02:18  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:54:59  marty]
 * 
 * Revision 1.1.68.1  1995/12/08  00:17:57  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:30 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/3  1995/09/15  19:57 UTC  lmm
 * 	merge fix for server_princ_name NULL check
 * 
 * 	HP revision /main/HPDCE02/lmm_inq_auth_fix/1  1995/09/15  19:44 UTC  lmm
 * 	add server_princ_name NULL check for alloc fail detection
 * 
 * 	HP revision /main/HPDCE02/2  1995/05/18  20:24 UTC  tatsu_s
 * 	Submitted the workaround for OT12857.
 * 
 * 	HP revision /main/HPDCE02/tatsu_s.scale_fix.b0/1  1995/05/08  20:12 UTC  tatsu_s
 * 	Workaround for auth_info cache miss in rpc_binding_set_auth_info().
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/02  01:13 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:17 UTC  lmm
 * 	add memory allocation failure detection
 * 	[1995/12/07  23:57:57  root]
 * 
 * Revision 1.1.66.2  1994/01/28  23:09:22  burati
 * 	DLG changes - Add rpc_binding_inq_auth_caller() (dlg_bl1)
 * 	[1994/01/24  21:53:09  burati]
 * 
 * Revision 1.1.66.1  1994/01/21  22:35:00  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:54:53  cbrooks]
 * 
 * Revision 1.1.4.6  1993/01/03  22:59:44  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:08  bbelch]
 * 
 * Revision 1.1.4.5  1992/12/23  20:18:46  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:32:47  zeliff]
 * 
 * Revision 1.1.4.4  1992/12/18  19:56:16  wei_hu
 * 	      09-dec-92 raizen    Modify assert of refcount when incrementing
 * 	[1992/12/18  19:45:56  wei_hu]
 * 
 * Revision 1.1.4.3  1992/10/07  14:56:30  markar
 * 	  CR 5177: Check for NULL input arg in rpc_binding_inq_auth_client()
 * 	[1992/10/02  20:28:55  markar]
 * 
 * Revision 1.1.4.2  1992/10/02  20:14:00  markar
 * 	   CR 5350 related fix: rpc__auth_info_release wasn't NULLing the caller's
 * 	                        auth_info reference.
 * 	[1992/10/01  20:32:14  markar]
 * 
 * Revision 1.1.2.2  1992/05/01  17:01:11  rsalz
 * 	 24-apr-92 nm          Use new RPC_AUTHN_CHECK_SUPPORTED_RPC_PROT instead of
 * 	                       RPC_AUTHN_CHECK_SUPPORTED in rpc_binding_set_auth_info().
 * 	  6-apr-92 wh          If no server principal name is supplied to
 * 	                       binding_set_auth_info, get it from the server.
 * 	 04-apr-92 ebm         Change rpc__stralloc to rpc_stralloc.
 * 	 19-mar-92 wh          Fix prototype errors.
 * 	  5-mar-92 wh          DCE 1.0.1 merge.
 * 	 27-jan-92 sommerfeld  Fix caching of auth identity:
 * 	                       call underlying auth protocol to
 * 	                       canonicalize before doing cache lookup.
 * 	 27-jan-92 sommerfeld  key reorg.
 * 	[1992/05/01  16:49:02  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:09:42  devrcs
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
**      comauth.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Generic interface to authentication services
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */
#include <comp.h>       /* Private communications services */
#include <comauth.h>    /* Common Authentication services */

#include <dce/assert.h>


/*
 * Internal variables to maintain the auth info cache.
 */
INTERNAL rpc_list_t     auth_info_cache;
INTERNAL rpc_mutex_t    auth_info_cache_mutex;    

#ifdef SGIMIPS
/*
 * R P C _ _ A U T H _ I N F O _ R E F E R E N C E _ N O L O C K
 */
PRIVATE void rpc__auth_info_reference_nolock _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t       	     /* auth_info */
    ));
#endif /* SGIMIPS */

/*
 * R P C _ _ A U T H _ I N F O _ C A C H E _ L K U P
 */
INTERNAL rpc_auth_info_t *rpc__auth_info_cache_lkup _DCE_PROTOTYPE_ ((
        unsigned_char_p_t                    /*server_princ_name*/,
        rpc_authn_level_t                    /*authn_level*/,
        rpc_auth_identity_handle_t           /*auth_identity*/,
        rpc_authz_protocol_id_t              /*authz_protocol*/,
        rpc_authn_protocol_id_t             /* authn_protocol*/
    ));

/*
 * R P C _ _ A U T H _ I N F O _ C A C H E _ A D D
 */

INTERNAL void rpc__auth_info_cache_add _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t                   /*auth_info*/
    ));

/*
 * R P C _ _ A U T H _ I N F O _ C A C H E _ R E M O V E
 */
INTERNAL void rpc__auth_info_cache_remove _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t                   /*auth_info*/
    ));


/*
 * Macro to assign through a pointer iff the pointer is non-NULL.  Note
 * that we depend on the fact that "val" is not evaluated if "ptr" is
 * NULL (i.e., this must be a macro, not a function).
 */
#define ASSIGN(ptr, val) \
( \
    (ptr) != NULL ? *(ptr) = (val) : 0 \
)


/*
**++
**
**  ROUTINE NAME:       rpc__auth_inq_supported
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Return a boolean indicating whether the authentication protocol
**  is supported by the runtime.
**      
**  INPUTS:
**
**      authn_prot_id           Authentication protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**  true if supported
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean32 rpc__auth_inq_supported 
#ifdef _DCE_PROTO_
(
  rpc_authn_protocol_id_t         authn_prot_id
)
#else
(authn_prot_id)
rpc_authn_protocol_id_t         authn_prot_id;
#endif
{
    return (RPC_AUTHN_INQ_SUPPORTED(authn_prot_id));
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_cvt_id_api_to_wire
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Return the wire value of an authentication protocol ID given its
**  API counterpart.
**      
**  INPUTS:
**
**      api_authn_prot_id       API Authentication protocol ID
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:            none
**
**      status          A value indicating the return status of the routine
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     unsigned32
**
**  The wire Authentication protocol ID.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE unsigned32 rpc__auth_cvt_id_api_to_wire 
#ifdef _DCE_PROTO_
(
  rpc_authn_protocol_id_t api_authn_prot_id,
  unsigned32              *status
)
#else
(api_authn_prot_id, status)
rpc_authn_protocol_id_t api_authn_prot_id;
unsigned32              *status;
#endif
{
#ifdef SGIMIPS
    if (! RPC_AUTHN_IN_RANGE((int)api_authn_prot_id) || 
#else
    if (! RPC_AUTHN_IN_RANGE(api_authn_prot_id) || 
#endif
        ! RPC_AUTHN_INQ_SUPPORTED(api_authn_prot_id))
    {
        *status = rpc_s_unknown_auth_protocol;
        return (0xeffaced);
    }

    *status = rpc_s_ok;
    return (rpc_g_authn_protocol_id[api_authn_prot_id].dce_rpc_authn_protocol_id);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_cvt_id_wire_to_api
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Return the API value of an authentication protocol ID given its
**  wire counterpart.
**      
**  INPUTS:
**
**      wire_authn_prot_id      Wire Authentication protocol ID
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:            
**
**      status          A value indicating the return status of the routine
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     unsigned32
**
**  The API Authentication protocol ID.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_authn_protocol_id_t rpc__auth_cvt_id_wire_to_api 
#ifdef _DCE_PROTO_
(
  unsigned32      wire_authn_prot_id,
  unsigned32      *status
)
#else
(wire_authn_prot_id, status)
unsigned32      wire_authn_prot_id;
unsigned32      *status;
#endif
{
    rpc_authn_protocol_id_t authn_protocol;

    for (authn_protocol = 0; 
         authn_protocol < RPC_C_AUTHN_PROTOCOL_ID_MAX; 
         authn_protocol++)
    {
        rpc_authn_protocol_id_elt_p_t aprot = &rpc_g_authn_protocol_id[authn_protocol];

        if (aprot->epv != NULL &&
            aprot->dce_rpc_authn_protocol_id == wire_authn_prot_id)
        {
            break;
        }
    }

    if (authn_protocol >= RPC_C_AUTHN_PROTOCOL_ID_MAX)
    {
        *status = rpc_s_unknown_auth_protocol;
        return ((rpc_authn_protocol_id_t)0xdeaddeadUL);
    }

    *status = rpc_s_ok;
    return (authn_protocol);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_inq_rpc_prot_epv
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Return the RPC protocol specific entry point vector for a given
**  Authentication protocol.
**      
**  INPUTS:
**
**      authn_prot_id   Authentication protocol ID
**      rpc_prot_id     RPC protocol ID
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     rpc_prot_epv_tbl
**
**  The address of the RPC protocol specific, authentication
**  protocol specific entry point vector.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE rpc_auth_rpc_prot_epv_t *rpc__auth_rpc_prot_epv
#ifdef _DCE_PROTO_
(
  rpc_authn_protocol_id_t authn_prot_id,
  rpc_protocol_id_t       rpc_prot_id
)
#else
(authn_prot_id, rpc_prot_id)  
rpc_authn_protocol_id_t authn_prot_id;
rpc_protocol_id_t       rpc_prot_id;
#endif
{
    return (RPC_AUTHN_INQ_RPC_PROT_EPV(authn_prot_id,rpc_prot_id));
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_reference
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Establish a reference to authentication info.
**      
**  INPUTS:
**
**      auth_info       Authentication information   
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__auth_info_reference
#ifdef _DCE_PROTO_
(
  rpc_auth_info_p_t   auth_info
)
#else
(auth_info)  
rpc_auth_info_p_t   auth_info;
#endif
{
    char *info_type = auth_info->is_server?"server":"client";

#ifdef SGIMIPS
    RPC_MUTEX_LOCK (auth_info_cache_mutex);
#endif /* SGIMIPS */
    RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(rpc__auth_info_reference) %x: bumping %s refcount (was %d, now %d)\n",
        auth_info,
        info_type, auth_info->refcount,
        auth_info->refcount + 1));

/*    assert (auth_info->refcount >= 0);  XXX unsigned values always >= 0*/
    auth_info->refcount++;
#ifdef SGIMIPS
    RPC_MUTEX_UNLOCK (auth_info_cache_mutex);
#endif /* SGIMIPS */
}


#ifdef SGIMIPS
/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_reference_nolock
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:   
**
**  Establish a reference to authentication info without locking the
**  auth_info_cache_mutex.  The lock must be obtained before calling this
**  function.
**      
**  INPUTS:
**
**      auth_info       Authentication information   
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__auth_info_reference_nolock
#ifdef _DCE_PROTO_
(
  rpc_auth_info_p_t   auth_info
)
#else
(auth_info)  
rpc_auth_info_p_t   auth_info;
#endif
{
    char *info_type = auth_info->is_server?"server":"client";

    RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(rpc__auth_info_reference_nolock) %x: bumping %s refcount (was %d, now %d)\n",
        auth_info,
        info_type, auth_info->refcount,
        auth_info->refcount + 1));

/*    assert (auth_info->refcount >= 0);  XXX unsigned values always >= 0*/
    auth_info->refcount++;
}
#endif /* SGIMIPS */


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_binding_release  
**
**  SCOPE:              PRIVATE - declared in comauth.h
**
**  DESCRIPTION:   
**
**  Release reference to authentication info (stored in passed binding
**  handle) previously returned by set_server or inq_caller.  If we don't
**  have any auth info, do nothing.
**      
**  INPUTS:
**
**      binding_rep     RPC binding handle
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__auth_info_binding_release
#ifdef _DCE_PROTO_
(
  rpc_binding_rep_p_t     binding_rep
)
#else
(binding_rep)
rpc_binding_rep_p_t     binding_rep;
#endif
{
    rpc__auth_info_release (&binding_rep->auth_info);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_release
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Release reference to authentication info previously returned by
**  set_server or inq_caller.
**      
**  INPUTS:
**
**      info            Authentication info
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__auth_info_release
#ifdef _DCE_PROTO_
(
 rpc_auth_info_p_t       *info
)
#else
(info)
rpc_auth_info_p_t       *info;
#endif
{
    rpc_auth_info_p_t auth_info = *info;
    char *info_type;
    
    if (auth_info == NULL)
    {
        return;
    }

#ifdef SGIMIPS
    RPC_MUTEX_LOCK (auth_info_cache_mutex);
#endif /* SGIMIPS */
    
    info_type = auth_info->is_server?"server":"client";
    RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(rpc__auth_info_release) %x: dropping %s refcount (was %d, now %d)\n",
        auth_info,
        info_type,
        auth_info->refcount,
        auth_info->refcount-1 ));
    assert(auth_info->refcount >= 1);

    /*
     * Remove the reference.
     */
    auth_info->refcount--;

    /*
     * Some special logic is required for cache maintenance on the
     * client side.
     */
    if (!(auth_info->is_server))
    {
        if (auth_info->refcount == 1)
        {
            /*
             * The auth info can be removed from the cache if there is only
             * one reference left to it. That single reference is the cache's
             * reference. 
             */
            rpc__auth_info_cache_remove (auth_info);
        }
    }

    /*
     * Free the auth info when nobody holds a reference to it.
     */
    if (auth_info->refcount == 0)
    {
        (*rpc_g_authn_protocol_id[auth_info->authn_protocol].epv->free_info) 
            (&auth_info);
    }

    /*
     * NULL out the caller's reference to the auth info.
     */
    *info = NULL;
#ifdef SGIMIPS
    RPC_MUTEX_UNLOCK (auth_info_cache_mutex);
#endif /* SGIMIPS */
}


/*
**++
**
**  ROUTINE NAME:       rpc__key_info_reference
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Establish a reference to keyentication info.
**      
**  INPUTS:
**
**      key_info       Authentication information   
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__key_info_reference
#ifdef _DCE_PROTO_
(
  rpc_key_info_p_t   key_info
)
#else
(key_info)  
rpc_key_info_p_t   key_info;
#endif
{
    RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(rpc__key_info_reference) %x: bumping %s refcnt (was %d, now %d)\n",
        key_info,
        (key_info->is_server?"server":"client"),
        key_info->refcnt,
        key_info->refcnt + 1));

    assert (key_info->refcnt >= 1);
    key_info->refcnt++;
}

/*
**++
**
**  ROUTINE NAME:       rpc__key_info_release
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:   
**
**  Release reference to keyentication info previously returned by
**  set_server or inq_caller.
**      
**  INPUTS:
**
**      info            Authentication info
**
**  INPUTS/OUTPUTS:
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

PRIVATE void rpc__key_info_release
#ifdef _DCE_PROTO_
(
  rpc_key_info_p_t       *info
)
#else
(info)
rpc_key_info_p_t       *info;
#endif
{
    rpc_key_info_p_t key_info = *info;
    
    if (key_info == NULL)
    {
        return;
    }
    *info = NULL;
    
    RPC_DBG_PRINTF(rpc_e_dbg_auth, 3,
        ("(rpc__key_info_release) %x: dropping %s refcnt (was %d, now %d)\n",
            key_info,
            key_info->is_server?"server":"client",
            key_info->refcnt,
            key_info->refcnt-1 ));
    assert(key_info->refcnt >= 1);
    
    /*
     * Remove the reference.
     */
    key_info->refcnt--;

    /*
     * Free the auth info when nobody holds a reference to it.
     */
    if (key_info->refcnt == 0)
    {
        (*rpc_g_authn_protocol_id[key_info->auth_info->authn_protocol].epv->free_key) 
            (&key_info);
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc__auth_inq_my_princ_name
**
**  SCOPE:              PRIVATE - declared in comauth.h
**
**  DESCRIPTION:   
**
**  
**  
**  
**      
**  INPUTS:
**
**      h               RPC binding handle
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__auth_inq_my_princ_name
#ifdef _DCE_PROTO_
(
  unsigned32              dce_rpc_authn_protocol,
  unsigned32              princ_name_size,
  unsigned_char_p_t       princ_name,
  unsigned32              *st
)
#else
(dce_rpc_authn_protocol, princ_name_size, princ_name, st)
unsigned32              dce_rpc_authn_protocol;
unsigned32              princ_name_size;
unsigned_char_p_t       princ_name;
unsigned32              *st;
#endif
{
    rpc_authn_protocol_id_t authn_protocol;

    authn_protocol = rpc__auth_cvt_id_wire_to_api(dce_rpc_authn_protocol, st);
    if (*st != rpc_s_ok)
    {
        return;
    }

    (*rpc_g_authn_protocol_id[authn_protocol]
        .epv->inq_my_princ_name)
            (princ_name_size, princ_name, st);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_inq_my_princ_name_tgt
**
**  SCOPE:              PRIVATE - declared in comauth.h
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      h               RPC binding handle
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__auth_inq_my_princ_name_tgt
#ifdef _DCE_PROTO_
(
  unsigned32              dce_rpc_authn_protocol,
  unsigned32              princ_name_size,
  unsigned32              max_tgt_size,
  unsigned_char_p_t       princ_name,
  unsigned32              *tgt_length,
  unsigned_char_p_t       tgt_data,
  unsigned32              *st
)
#else
(dce_rpc_authn_protocol, princ_name_size, max_tgt_size, princ_name,
 tgt_length, tgt_data, st)
unsigned32              dce_rpc_authn_protocol;
unsigned32              princ_name_size;
unsigned32              max_tgt_size;
unsigned_char_p_t       princ_name;
unsigned32              *tgt_length;
unsigned_char_p_t       tgt_data;
unsigned32              *st;
#endif
{
    rpc_authn_protocol_id_t authn_protocol;

    authn_protocol = rpc__auth_cvt_id_wire_to_api(dce_rpc_authn_protocol, st);
    if (*st != rpc_s_ok)
    {
        return;
    }

    (*rpc_g_authn_protocol_id[authn_protocol].epv->inq_my_princ_name_tgt)
        (princ_name_size, max_tgt_size, princ_name, tgt_length, tgt_data, st);
}

 
/*
**++
**
**  ROUTINE NAME:       rpc_binding_set_auth_info
**
**  SCOPE:              PUBLIC - declared in rpcauth.idl
**
**  DESCRIPTION:
**      
**  Set up client handle for authentication.
**  
**  INPUTS:
**
**      h               RPC binding handle
**
**      server_princ_name     
**                      Name of server to authenticate to
**
**      authn_level     Authentication level
**  
**      authn_protocol  Desired authentication protocol to use
**
**      auth_identity   Credentials to use on calls
**
**      authz_protocol  Authorization protocol to use
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_set_auth_info 
#ifdef _DCE_PROTO_
(
  rpc_binding_handle_t    binding_h,
  unsigned_char_p_t       server_princ_name,
  unsigned32              authn_level,
  unsigned32              authn_protocol,
  rpc_auth_identity_handle_t auth_identity,
  unsigned32              authz_protocol,
  unsigned32              *st
)
#else
(binding_h, server_princ_name, authn_level, authn_protocol, 
 auth_identity, authz_protocol, st)
rpc_binding_handle_t    binding_h;
unsigned_char_p_t       server_princ_name;
unsigned32              authn_level;
unsigned32              authn_protocol;  
rpc_auth_identity_handle_t auth_identity;
unsigned32              authz_protocol;
unsigned32              *st;
#endif
{
    rpc_auth_identity_handle_t ref_auth_identity;
    rpc_auth_info_p_t       auth_info;
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_auth_epv_p_t        auth_epv;
    boolean                 need_to_free_server_name = false;
    
    CODING_ERROR (st);  
    RPC_VERIFY_INIT (); 

    RPC_BINDING_VALIDATE_CLIENT(binding_rep, st);
    if (*st != rpc_s_ok)
        return;

    /*
     * If asking to set to authentication type "none", just free any auth info
     * we have and return now.
     */
    
    if (authn_protocol == rpc_c_authn_none)
    {
        rpc__auth_info_binding_release(binding_rep);
        return;
    }

    RPC_AUTHN_CHECK_SUPPORTED_RPC_PROT(authn_protocol, binding_rep->protocol_id, st);

    /*
     * If asking for default authn level, get the actual level now (i.e., 
     * spare each auth service the effort of coding this logic).
     */

    if (authn_level == rpc_c_authn_level_default)
    {
        rpc_mgmt_inq_dflt_authn_level (authn_protocol, &authn_level, st);
        if (*st != rpc_s_ok)
            return;
    }

    auth_epv = rpc_g_authn_protocol_id[authn_protocol].epv;
    /*
     * Resolve the auth_identity into a real reference to the identity
     * prior to the cache lookup.
     */
    
    *st = (*auth_epv->resolve_id)
        (auth_identity, &ref_auth_identity);

    if (*st != rpc_s_ok)
        return;


    /*
     * If no server principal name was specified, go ask for it.
     *
     * Not all authentication protocols require a server principal
     * name to do authentication.  Hence, only inquire the server
     * principal name if we know the authentication protocol is
     * secret key based.
     *
     * We did not move the inq_princ_name function to an
     * authentication service specific module because we need
     * a server principal name for the auth_info cache lookup.
     *
     * Note that we want to be avoid bypassing the auth_info
     * cache because certain protocol services cache credentials.
     * Allocating a new auth_info structure on every call can
     * cause these credentials to accumulate until the heap is
     * exhausted.
     */
    if (authn_protocol == rpc_c_authn_dce_secret &&
        server_princ_name == NULL)
    {
        rpc_mgmt_inq_server_princ_name
            (binding_h,
             authn_protocol,
             &server_princ_name,
             st);

        if (*st != rpc_s_ok)
            return;

        need_to_free_server_name = true;
    }


    /*
     * Consult the cache before creating a new auth info structure.
     * We may be able to add a reference to an already existing one.
     */
    if ((auth_info = rpc__auth_info_cache_lkup (server_princ_name,
                                                authn_level,
                                                ref_auth_identity,
                                                authz_protocol, 
                                                authn_protocol)) == NULL)
    {
        
        /*
         * A new auth info will have to be created.
         * Call authentication service to do generic (not specific
         * to a single RPC protocol) "set server" function.
         */
        (*auth_epv->binding_set_auth_info)
            (server_princ_name, authn_level, auth_identity, 
             authz_protocol, binding_h, &auth_info, st);
        
        if (*st != rpc_s_ok)
        {
            if (need_to_free_server_name)
                RPC_MEM_FREE (server_princ_name, RPC_C_MEM_STRING);
            return;
        }
        
        /*
         * Add this new auth info to a cache of auth infos. This cache
         * will be consulted on a subsequent call to this routine.
         */
        rpc__auth_info_cache_add (auth_info);

    }
        
    /*
     * Release our reference to the identity.  If a new auth_info was
     * created, then it added a reference also.
     */

    (*auth_epv->release_id) (&ref_auth_identity);

    /*
     * If we inquired the server principal name, free it now.
     */
    if (need_to_free_server_name)
        RPC_MEM_FREE (server_princ_name, RPC_C_MEM_STRING);

    /*
     * If we have any auth info for this binding already, lose it.
     */

    if (binding_rep->auth_info != NULL)
    {
        rpc__auth_info_binding_release(binding_rep);
    }

    binding_rep->auth_info = auth_info;

    /*
     * Notify the protocol service that the binding has changed.
     */

    (*rpc_g_protocol_id[binding_rep->protocol_id].binding_epv
        ->binding_changed) (binding_rep, st);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_inq_auth_info
**
**  SCOPE:              PUBLIC - declared in rpcauth.idl
**
**  DESCRIPTION:
**      
**  Return authentication and authorization information from a binding
**  handle.
**  
**  INPUTS:
**
**      h               RPC binding handle
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      server_princ_name     
**                      Name of server to authenticate to
**
**      authn_level     Authentication level
**  
**      authn_protocol  Desired authentication protocol to use
**
**      auth_identity   Credentials to use on calls
**
**      authz_protocol  Authorization protocol to use
**
**      status          A value indicating the return status of the routine
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_inq_auth_info
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    unsigned_char_p_t       *server_princ_name,
    unsigned32              *authn_level,
    unsigned32              *authn_protocol,  
    rpc_auth_identity_handle_t *auth_identity,
    unsigned32              *authz_protocol,
    unsigned32              *st
    )
#else
(binding_h, server_princ_name, authn_level, authn_protocol, 
     auth_identity, authz_protocol, st)
rpc_binding_handle_t    binding_h;
unsigned_char_p_t       *server_princ_name;
unsigned32              *authn_level;
unsigned32              *authn_protocol;  
rpc_auth_identity_handle_t *auth_identity;
unsigned32              *authz_protocol;
unsigned32              *st;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_auth_info_p_t       auth_info;

    CODING_ERROR (st);  
    RPC_VERIFY_INIT (); 

    RPC_BINDING_VALIDATE_CLIENT(binding_rep, st);
    if (*st != rpc_s_ok)
        return;

    auth_info = ((rpc_binding_rep_p_t)binding_h)->auth_info;

    if (auth_info == NULL)
    {
        *st = rpc_s_binding_has_no_auth;
        return;
    }

    assert(! auth_info->is_server);

    if (auth_info->server_princ_name == NULL) 
    {
        ASSIGN(server_princ_name, NULL);
    } else 
    {
        ASSIGN(server_princ_name, rpc_stralloc(auth_info->server_princ_name));
	if (server_princ_name != NULL && *server_princ_name == NULL){
	    *st = rpc_s_no_memory;
	    return;
	}
    }
    ASSIGN(authn_level,         auth_info->authn_level);
    ASSIGN(authn_protocol,      auth_info->authn_protocol);  
    ASSIGN(auth_identity,       auth_info->u.auth_identity);
    ASSIGN(authz_protocol,      auth_info->authz_protocol);  

    *st = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc_server_register_auth_info
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Register authentication information with the RPC runtime.
**
**  INPUTS:
**
**      authn_protocol  Desired authentication protocol to use
**
**      server_princ_name     
**                      Name server should use
**
**      get_key_func    Function ptr to call to get keys
**
**      arg             Opaque params for key func.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
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

PUBLIC void rpc_server_register_auth_info 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       server_princ_name,
    unsigned32              authn_protocol,
    rpc_auth_key_retrieval_fn_t get_key_func,
    ndr_void_p_t            arg,
    unsigned32              *st
)
#else
(server_princ_name, authn_protocol, get_key_func, arg, st)
unsigned_char_p_t       server_princ_name;
unsigned32              authn_protocol;
rpc_auth_key_retrieval_fn_t get_key_func;
ndr_void_p_t            arg;
unsigned32              *st;
#endif
{
    CODING_ERROR (st);
    RPC_VERIFY_INIT ();
    
    if (authn_protocol == rpc_c_authn_none)
    {
        *st = rpc_s_ok;
        return;
    }

    if (authn_protocol == rpc_c_authn_default && get_key_func != NULL)
    {
        *st = rpc_s_key_func_not_allowed;
        return;
    }

#ifdef SGIMIPS
    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, st);
#else
    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, st);
#endif
    
    (*rpc_g_authn_protocol_id[authn_protocol]
        .epv->server_register_auth_info)
            (server_princ_name, get_key_func, (pointer_t) arg, st);
}

/*
**++
**
**  ROUTINE NAME:       rpc_server_register_auth_ident
**
**  SCOPE:              PUBLIC - declared in rpc.idl
**
**  DESCRIPTION:
**      
**  Register authentication identity information with the RPC runtime.
**
**  INPUTS:
**
**      authn_protocol  Desired authentication protocol to use
**
**      server_princ_name     
**                      Name server should use
**
**      id_h            RPC AUTH identity handle (maps to a sec_login_handle_t)
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
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

PUBLIC void rpc_server_register_auth_ident
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t           server_princ_name,
    unsigned32                  authn_protocol,
    rpc_auth_identity_handle_t  id_h,
    unsigned32                  *st
)
#else
(server_princ_name, authn_protocol, id_h, st)
unsigned_char_p_t               server_princ_name;
unsigned32                      authn_protocol;
rpc_auth_identity_handle_t      id_h,
unsigned32                      *st;
#endif
{
    CODING_ERROR (st);
    RPC_VERIFY_INIT ();

    if (authn_protocol == rpc_c_authn_none)
    {
        *st = rpc_s_ok;
        return;
    }

    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, st);

    (*rpc_g_authn_protocol_id[authn_protocol]
        .epv->server_register_auth_u2u)
            (server_princ_name, id_h, st);
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_inq_auth_client
**
**  SCOPE:              PUBLIC - declared in rpcauth.idl
**
**  DESCRIPTION:
**
**  Return authentication and authorization information from a binding
**  handle to an authenticated client.
**  be freed.
**
**  INPUTS:
**
**      binding_h       Server-side binding handle to remote caller whose
**                      identity is being requested.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      privs           PAC for remote caller.
**
**      server_princ_name         
**                      Server name that caller authenticated to.
**
**      authn_level     Authentication level used by remote caller.
**
**      authn_protocol  Authentication protocol used by remote caller.
**
**      authz_protocol  Authorization protocol used by remote caller.
**
**      status          A value indicating the return status of the routine
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_inq_auth_client
    
#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_authz_handle_t      *privs,
    unsigned_char_p_t       *server_princ_name,
    unsigned32              *authn_level,
    unsigned32              *authn_protocol,
    unsigned32              *authz_protocol,
    unsigned32              *st
)
#else
(binding_h, privs, server_princ_name, authn_level, 
     authn_protocol, authz_protocol, st)
rpc_binding_handle_t    binding_h;
rpc_authz_handle_t      *privs;
unsigned_char_p_t       *server_princ_name;
unsigned32              *authn_level;
unsigned32              *authn_protocol;
unsigned32              *authz_protocol;
unsigned32              *st;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_auth_info_p_t       auth_info;

    CODING_ERROR (st);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_SERVER(binding_rep, st);
    if (*st != rpc_s_ok)
        return;

    auth_info = ((rpc_binding_rep_p_t)binding_h)->auth_info;

    if (auth_info == NULL)
    {
        *st = rpc_s_binding_has_no_auth;
        return;
    }

    assert(auth_info->is_server);

    ASSIGN(privs,               auth_info->u.s.privs);

    if (server_princ_name != NULL)
    {
        if (auth_info->server_princ_name == NULL) 
        {
            ASSIGN(server_princ_name,   NULL);
        } 
        else 
        {
            ASSIGN(server_princ_name, rpc_stralloc(auth_info->server_princ_name));
	    if (*server_princ_name == NULL){
		*st = rpc_s_no_memory;
		return;
	    }
        }
    }

    ASSIGN(authn_level,         auth_info->authn_level);
    ASSIGN(authn_protocol,      auth_info->authn_protocol);
    ASSIGN(authz_protocol,      auth_info->authz_protocol);

    *st = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_binding_inq_auth_caller
**
**  SCOPE:              PUBLIC - declared in rpcauth.idl
**
**  DESCRIPTION:
**
**  Return authentication and 1.1+ authorization information from a binding
**  handle to an authenticated client.
**
**  INPUTS:
**
**      binding_h       Server-side binding handle to remote caller whose
**                      identity is being requested.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      creds           Opaque handle on caller's credentials to
**                      be used in making sec_cred_ calls
**
**      server_princ_name         
**                      Server name that caller authenticated to.
**
**      authn_level     Authentication level used by remote caller.
**
**      authn_protocol  Authentication protocol used by remote caller.
**
**      authz_protocol  Authorization protocol used by remote caller.
**
**      status          A value indicating the return status of the routine
**          rpc_s_invalid_binding
**                          RPC Protocol ID in binding handle was invalid.
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

PUBLIC void rpc_binding_inq_auth_caller

#ifdef _DCE_PROTO_
(
    rpc_binding_handle_t    binding_h,
    rpc_authz_cred_handle_t *creds,
    unsigned_char_p_t       *server_princ_name,
    unsigned32              *authn_level,
    unsigned32              *authn_protocol,
    unsigned32              *authz_protocol,
    unsigned32              *st
)
#else
(binding_h, creds, server_princ_name, authn_level, 
     authn_protocol, authz_protocol, st)
rpc_binding_handle_t    binding_h;
rpc_authz_cred_handle_t *creds;
unsigned_char_p_t       *server_princ_name;
unsigned32              *authn_level;
unsigned32              *authn_protocol;
unsigned32              *authz_protocol;
unsigned32              *st;
#endif
{
    rpc_binding_rep_p_t     binding_rep = (rpc_binding_rep_p_t) binding_h;
    rpc_auth_info_p_t       auth_info;

    CODING_ERROR (st);
    RPC_VERIFY_INIT ();
    
    RPC_BINDING_VALIDATE_SERVER(binding_rep, st);
    if (*st != rpc_s_ok)
        return;

    auth_info = ((rpc_binding_rep_p_t)binding_h)->auth_info;

    if (auth_info == NULL)
    {
        *st = rpc_s_binding_has_no_auth;
        return;
    }

    assert(auth_info->is_server);

    if (auth_info->u.s.creds != NULL) {
	*creds = *auth_info->u.s.creds;
    }

    if (server_princ_name != NULL)
    {
        if (auth_info->server_princ_name == NULL) 
        {
            ASSIGN(server_princ_name,   NULL);
        } 
        else 
        {
            ASSIGN(server_princ_name, rpc_stralloc(auth_info->server_princ_name));
	    if (*server_princ_name == NULL){
		*st = rpc_s_no_memory;
		return;
	    }
        }
    }

    ASSIGN(authn_level,         auth_info->authn_level);
    ASSIGN(authn_protocol,      auth_info->authn_protocol);
    ASSIGN(authz_protocol,      auth_info->authz_protocol);

    *st = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       rpc_mgmt_inq_dflt_protect_level
**
**  SCOPE:              PUBLIC - declared in rpcauth.idl
**
**  DESCRIPTION:
**      
**  Returns the default authentication level for an authentication service.
**
**  INPUTS:
**
**      authn_protocol  Desired authentication protocol.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      authn_level     Authentication level used by remote caller.
**
**      status          A value indicating the return status of the routine
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

PUBLIC void rpc_mgmt_inq_dflt_protect_level 
#ifdef _DCE_PROTO_
(
    unsigned32              authn_protocol,
    unsigned32              *authn_level,
    unsigned32              *st
)
#else
(authn_protocol, authn_level, st)
unsigned32              authn_protocol;
unsigned32              *authn_level;
unsigned32              *st;
#endif
{
    CODING_ERROR (st);
    RPC_VERIFY_INIT ();
    
    if (authn_protocol == rpc_c_authn_none)
    {
        *authn_level = rpc_c_authn_level_none;
        *st = rpc_s_ok;
        return;
    }

    RPC_AUTHN_CHECK_SUPPORTED (authn_protocol, st);
    
    (*rpc_g_authn_protocol_id[authn_protocol]
        .epv->mgmt_inq_dflt_auth_level)
            (authn_level, st);
}

/*
 * Retain entry point with old name for compatibility.
 */

PUBLIC void rpc_mgmt_inq_dflt_authn_level 
#ifdef _DCE_PROTO_
(
  unsigned32              authn_protocol,
  unsigned32              *authn_level,
  unsigned32              *st
)
#else
(authn_protocol, authn_level, st)
unsigned32              authn_protocol;
unsigned32              *authn_level;
unsigned32              *st;
#endif
{
    rpc_mgmt_inq_dflt_protect_level (authn_protocol, authn_level, st);
}



/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_cache_init
**
**  SCOPE:              PRIVATE - declared in comauth.h
**
**  DESCRIPTION:
**      
**  Initialize the auth info cache including mutexes and list head.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:           
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__auth_info_cache_init 
#ifdef _DCE_PROTO_
(
  unsigned32      *status
)
#else
(status)
unsigned32      *status;
#endif
{
    CODING_ERROR (status);

    RPC_MUTEX_INIT (auth_info_cache_mutex);
    RPC_LIST_INIT (auth_info_cache);
    *status = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_cache_lkup
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Scan a linked list of auth info structures looking for one which
**  contains fields which match the input parameters. If and when a
**  match is found the reference count of the auth info structure will
**  be incremented before being returned.
**
**  Note that it is possible for a null server principal name to
**  match an auth info structure if that structure also has a 
**  null server principal name.
**
**  INPUTS:
**
**      server_princ_name Server principal name.
**      authn_level     Authentication level.
**      authn_identity  Authentication identity handle.
**      authz_protocol  Authorization protocol.
**      authn_protocol  Authentication protocol.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     rpc_auth_info_t *
**
**      A pointer to a matching auth info, NULL if none found.
**
**  SIDE EFFECTS:      
**
**      If a matching auth info is found the reference count of it
**      will have already been incremented.
**
**--
**/

INTERNAL rpc_auth_info_t *rpc__auth_info_cache_lkup 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t                   server_princ_name,
    rpc_authn_level_t                   authn_level,
    rpc_auth_identity_handle_t          auth_identity,
    rpc_authz_protocol_id_t             authz_protocol,
    rpc_authn_protocol_id_t             authn_protocol
)
#else
(server_princ_name, authn_level, auth_identity, authz_protocol, authn_protocol)
unsigned_char_p_t                   server_princ_name;
rpc_authn_level_t                   authn_level;
rpc_auth_identity_handle_t          auth_identity;
rpc_authz_protocol_id_t             authz_protocol;
rpc_authn_protocol_id_t             authn_protocol;
#endif
{
    rpc_auth_info_t     *auth_info;

    RPC_MUTEX_LOCK (auth_info_cache_mutex);

    /*
     * Scan the linked list of auth info structure looking for one
     * whose fields match the input args.
     */
    RPC_LIST_FIRST (auth_info_cache,
                    auth_info,
                    rpc_auth_info_p_t);
    while (auth_info != NULL)
    {
        if ((
             /*
              * DCE secret key authentication requires a
              * non-null server principal name for authentication.
              * We allow a null server principal name here so
              * that this will work in the future when an 
              * authentication service without this requirement
              * is used.
              */
             ((server_princ_name == NULL)
              && (auth_info->server_princ_name == NULL))
             ||
             (strcmp ((char *) server_princ_name, 
                      (char *) auth_info->server_princ_name) == 0)
            )
            &&
            (authn_level == auth_info->authn_level)
            &&
            (authn_protocol == auth_info->authn_protocol)
            &&
            (authz_protocol == auth_info->authz_protocol)
            &&
            (auth_identity == auth_info->u.auth_identity))
        {
            /*
             * A matching auth info was found. 
             */
#ifdef SGIMIPS
            rpc__auth_info_reference_nolock (auth_info);
#else /* SGIMIPS */
            rpc__auth_info_reference (auth_info);
#endif /* SGIMIPS */
            break;
        }
        RPC_LIST_NEXT (auth_info, auth_info, rpc_auth_info_p_t);
    }
    RPC_MUTEX_UNLOCK (auth_info_cache_mutex);
    return (auth_info);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_cache_add
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Add an auth info structure to a linked list of them. The
**  reference count of the added auth info structure will be incremented
**  by the caller to account for its presence in the cache.
**
**  INPUTS:
**
**      auth_info       The auth info to be added.
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
**      The reference count of the added auth info structure will be
**      incremented. 
**
**--
**/

INTERNAL void rpc__auth_info_cache_add 
#ifdef _DCE_PROTO_
(
  rpc_auth_info_p_t auth_info
)
#else
(auth_info)
rpc_auth_info_p_t auth_info;
#endif
{
    assert (!auth_info->is_server);

    RPC_MUTEX_LOCK (auth_info_cache_mutex);
    RPC_LIST_ADD_HEAD (auth_info_cache,
                       auth_info,
                       rpc_auth_info_p_t);
#ifdef SGIMIPS
    rpc__auth_info_reference_nolock (auth_info);
#else /* SGIMIPS */
    rpc__auth_info_reference (auth_info);
#endif /* SGIMIPS */
    RPC_MUTEX_UNLOCK (auth_info_cache_mutex);
}


/*
**++
**
**  ROUTINE NAME:       rpc__auth_info_cache_remove
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Remove an auth info structure from a linked list of them. The
**  cache's reference to the auth info structure will be released by
**  the caller.
**
**  It is assumed that the caller has already determined it is OK to
**  remove this auth info from the cache. It is expected that the
**  cache holds the last reference to this auth info structure at
**  this point. 
**
**  INPUTS:             
**
**      auth_info       The auth info to be removed.
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

INTERNAL void rpc__auth_info_cache_remove 
#ifdef _DCE_PROTO_
(
  rpc_auth_info_p_t       auth_info
)
#else
(auth_info)
rpc_auth_info_p_t       auth_info;
#endif
{
    char *info_type;

    assert (!auth_info->is_server);

#ifndef SGIMIPS
    RPC_MUTEX_LOCK (auth_info_cache_mutex);
#endif /* !SGIMIPS */
    
    /*
     * Make sure, under the protection of the cache lock, that this
     * really should be removed from the cache.
     */
    if (auth_info->refcount == 1)
    {
        RPC_LIST_REMOVE (auth_info_cache, auth_info);
        info_type = auth_info->is_server?"server":"client";
        RPC_DBG_PRINTF(rpc_e_dbg_auth, 3, ("(rpc__auth_info_release) %x: dropping %s refcount (was %d, now %d)\n",
                                           auth_info,
                                           info_type,
                                           auth_info->refcount,
                                           auth_info->refcount-1 ));
        assert(auth_info->refcount >= 1);
        auth_info->refcount--;
    }
#ifndef SGIMIPS
    RPC_MUTEX_UNLOCK (auth_info_cache_mutex);
#endif /* !SGIMIPS */
}
