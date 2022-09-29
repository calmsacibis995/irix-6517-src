/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comnaf.c,v 65.5 1998/03/23 17:30:30 gwehrman Exp $";
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
 * $Log: comnaf.c,v $
 * Revision 65.5  1998/03/23 17:30:30  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/26 19:05:00  lmc
 * Changes for sockets using behaviors.  Prototype and cast changes.
 *
 * Revision 65.3  1998/01/07  17:21:39  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:39  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:48  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.13.2  1996/02/18  00:02:41  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:55:19  marty]
 *
 * Revision 1.1.13.1  1995/12/08  00:18:24  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:30 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:14 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:18 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:15 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  20:04 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:58:14  root]
 * 
 * Revision 1.1.11.2  1994/05/27  15:35:53  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added rpc__naf_inq_max_frag_size().
 * 	[1994/04/29  18:56:32  tatsu_s]
 * 
 * Revision 1.1.11.1  1994/01/21  22:35:31  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:32  cbrooks]
 * 
 * Revision 1.1.9.1  1993/09/28  21:58:09  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:34  weisman]
 * 
 * Revision 1.1.6.3  1993/01/03  23:22:19  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:11  bbelch]
 * 
 * Revision 1.1.6.2  1992/12/23  20:44:14  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:33:53  zeliff]
 * 
 * Revision 1.1.3.4  1992/06/26  06:01:08  jim
 * 	Changes for AIX 3.2
 * 	[1992/06/23  16:06:13  jim]
 * 
 * Revision 1.1.3.3  1992/05/01  19:21:12  rsalz
 * 	  5-mar-92 wh        DCE 1.0.1 merge.
 * 	 29-jan-92 ko        rpc__naf_desc_inq_peer_addr now dispatches
 * 	                     into naf specific routine.
 * 	[1992/05/01  19:16:39  rsalz]
 * 
 * Revision 1.1.2.2  1992/03/03  15:22:07  wei_hu
 * 	No changes.  Just to get around bci problem.
 * 
 * Revision 1.1.1.2  1992/02/27  18:56:05  wei_hu
 * 	16-jan-92 wh         Fix memory leak; free tower ref on error path
 * 	                     in naf_tower_flrs_to_addr.
 * 
 * Revision 1.1  1992/01/19  03:09:50  devrcs
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
**      comnaf.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definition of the Network Address Family Services for the Common
**  Communication Services component. These routines are called by the
**  other services internal to the RPC runtime (such as protocol services)
**  to dispatch to the appropriate Network Address Family service.
**
**
*/

#include <commonp.h>    /* Common declarations for all RPC runtime */
#include <com.h>        /* Common communications services */
#include <comprot.h>    /* Common protocol services */
#include <comnaf.h>     /* Common network address family services */
#include <comp.h>       /* Private communications services */
#include <comtwrref.h>  /* Private tower defs for other RPC components */


/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_alloc
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to create an RPC Address.
**
**  INPUTS:
**
**      protseq_id      The RPC Protocol Sequence ID which represents a
**                      NAF, NAF Transport Protocol, and NAF Interface Type
**
**      naf_id          The NAF ID of the RPC Address
**
**      endpoint        The string containing the endpoint to be placed 
**                      in the RPC Address
**
**      netaddr         The string containing the network address to be
**                      placed in the RPC Address
**
**      network_options A vector of network options tag and value strings 
**                      to be placed in the RPC Address
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr        The RPC Address to be created in the format of a
**                      particular NAF
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_alloc routine
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

PRIVATE void rpc__naf_addr_alloc 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t        protseq_id,
    rpc_naf_id_t            naf_id,
    unsigned_char_p_t       endpoint,
    unsigned_char_p_t       netaddr,
    unsigned_char_p_t       network_options,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(protseq_id, naf_id, endpoint, netaddr, network_options, rpc_addr, status)
rpc_protseq_id_t        protseq_id;
rpc_naf_id_t            naf_id;
unsigned_char_p_t       endpoint;
unsigned_char_p_t       netaddr;
unsigned_char_p_t       network_options;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    RPC_LOG_NAF_ADDR_ALLOC_NTR;

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_addr_alloc)
        (protseq_id, naf_id, endpoint, netaddr, network_options,
        rpc_addr, status);

    RPC_LOG_NAF_ADDR_ALLOC_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_free
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to free an RPC Address.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC Address to be freed in the format of a
**                      particular NAF
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_free routine
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

PRIVATE void rpc__naf_addr_free 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(rpc_addr, status)
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    RPC_LOG_NAF_ADDR_FREE_NTR;

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[(*rpc_addr)->sa.family].epv->naf_addr_free)
        (rpc_addr, status);

    RPC_LOG_NAF_ADDR_FREE_XIT;
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_vector_free
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to free a vector of
**  RPC Addresses.  Each address and the vector itself are freed.
**  
**  Note that we ignore NULL entries in the vector.  This is to make
**  it easy for recipients of a vector to "steal" the pointers to RPC
**  Addresses and put them in some other data structure.  After the
**  stealing, they null out the vector entry.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr_vec    The vector of RPC Addresses to be freed
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_addr_vector_free 
#ifdef _DCE_PROTO_
(
    rpc_addr_vector_p_t     *rpc_addr_vec,
    unsigned32              *status
)
#else
(rpc_addr_vec, status)
rpc_addr_vector_p_t     *rpc_addr_vec;
unsigned32              *status;
#endif
{
    unsigned16 i;

    /*
     * In case the vector is empty.
     */
    *status = rpc_s_ok;

    for (i = 0; i < (*rpc_addr_vec)->len; i++)
    {
        if ((*rpc_addr_vec)->addrs[i] != NULL)
        {
            (*rpc_g_naf_id[(*rpc_addr_vec)->addrs[i]->sa.family].epv->naf_addr_free)
                (&(*rpc_addr_vec)->addrs[i], status);
        }
    }

    RPC_MEM_FREE (*rpc_addr_vec, RPC_C_MEM_RPC_ADDR_VEC);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_copy
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to copy an RPC Address.
**
**  INPUTS:
**
**      src_rpc_addr    The RPC Address to be copied in the format of a
**                      particular NAF
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      dst_rpc_addr    The RPC Address to be created in the format of a
**                      particular NAF
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_copy routine
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

PRIVATE void rpc__naf_addr_copy 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            src_rpc_addr,
    rpc_addr_p_t            *dst_rpc_addr,
    unsigned32              *status
)
#else
(src_rpc_addr, dst_rpc_addr, status)
rpc_addr_p_t            src_rpc_addr;
rpc_addr_p_t            *dst_rpc_addr;
unsigned32              *status;
#endif
{
    RPC_LOG_NAF_ADDR_COPY_NTR;

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[src_rpc_addr->sa.family].epv->naf_addr_copy)
        (src_rpc_addr, dst_rpc_addr, status);

    RPC_LOG_NAF_ADDR_COPY_XIT;
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_overcopy
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Like rpc__naf_addr_copy, except uses storage from an existing target 
**  address, if there is one (thus saving the malloc).
**
**  INPUTS:
**
**      src_rpc_addr    The RPC Address to be copied in the format of a
**                      particular NAF
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      dst_rpc_addr    The RPC Address to be created in the format of a
**                      particular NAF
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_copy routine
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

PRIVATE void rpc__naf_addr_overcopy 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            src_rpc_addr,
    rpc_addr_p_t            *dst_rpc_addr,
    unsigned32              *status
)
#else
(src_rpc_addr, dst_rpc_addr, status)
rpc_addr_p_t            src_rpc_addr;
rpc_addr_p_t            *dst_rpc_addr;
unsigned32              *status;
#endif
{
    /*
     * If there is no existing destinition address yet, or if it's not big
     * enough to receive the source address, call the NAF copy.  (Free the
     * existing one first in the latter case.)  Otherwise, just overwrite
     * the existing destination with the source address.
     */
    if (*dst_rpc_addr == NULL || (*dst_rpc_addr)->len < src_rpc_addr->len)
    {
        if (*dst_rpc_addr != NULL)
            (*rpc_g_naf_id[(*dst_rpc_addr)->sa.family].epv->naf_addr_free)
                (dst_rpc_addr, status);
        (*rpc_g_naf_id[src_rpc_addr->sa.family].epv->naf_addr_copy)
            (src_rpc_addr, dst_rpc_addr, status);
    }
    else
    {
        assert((*dst_rpc_addr)->len >= sizeof((*dst_rpc_addr)->sa));
        **dst_rpc_addr = *src_rpc_addr;
        memmove( &(*dst_rpc_addr)->sa, &src_rpc_addr->sa, src_rpc_addr->len);
        *status = rpc_s_ok;
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_set_endpoint
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to set the endpoint in
**  an RPC Address.
**
**  INPUTS:
**
**      endpoint        The string containing the endpoint to be placed 
**                      in the RPC Address
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC Address in which to set the endpoint in the
**                      format of a particular NAF
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_set_endpoint routine
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

PRIVATE void rpc__naf_addr_set_endpoint 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       endpoint,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(endpoint, rpc_addr, status)
unsigned_char_p_t       endpoint;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[(*rpc_addr)->sa.family].epv->naf_addr_set_endpoint)
        (endpoint, rpc_addr, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_inq_endpoint
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to inquire the endpoint
**  in an RPC Address.
**
**  INPUTS:
**
**      rpc_addr        The RPC Address from which to obtain the endpoint
**                      in the format of a particular NAF
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      endpoint        The string containing the endpoint to be obtained
**                      from the RPC Address
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_inq_endpoint routine
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

PRIVATE void rpc__naf_addr_inq_endpoint 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            rpc_addr,
    unsigned_char_t         **endpoint,
    unsigned32              *status
)
#else
(rpc_addr, endpoint, status)
rpc_addr_p_t            rpc_addr;
unsigned_char_t         **endpoint;
unsigned32              *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_endpoint)
        (rpc_addr, endpoint, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_set_netaddr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to set the network address
**  in an RPC Address.
**
**  INPUTS:
**
**      netaddr         The string containing the network address to be
**                      placed in the RPC Address
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC Address in which to set the network address
**                      in the format of a particular NAF
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_set_netaddr routine
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

PRIVATE void rpc__naf_addr_set_netaddr 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       netaddr,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(netaddr, rpc_addr, status)
unsigned_char_p_t       netaddr;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[(*rpc_addr)->sa.family].epv->naf_addr_set_netaddr)
        (netaddr, rpc_addr, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_inq_netaddr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to obtain the network
**  address from an RPC Address.
**
**  INPUTS:
**
**      rpc_addr        The RPC Address from which to obtain the network
**                      address in the format of a particular NAF
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      netaddr         The string containing the network address
**                      obtained from the RPC Address
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_inq_netaddr routine
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

#ifdef SGIMIPS
PRIVATE void rpc__naf_addr_inq_netaddr (
rpc_addr_p_t            rpc_addr,
unsigned_char_t         **netaddr,
unsigned32              *status)
#else
PRIVATE void rpc__naf_addr_inq_netaddr (rpc_addr, netaddr, status)
    
rpc_addr_p_t            rpc_addr;
unsigned_char_t         **netaddr;
unsigned32              *status;
#endif

{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_netaddr)
        (rpc_addr, netaddr, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_set_options
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to set the network options
**  in an RPC Address.
**
**  INPUTS:
**
**      network_options A network options string 
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The RPC Address in which to set the network options
**                      in the format of a particular NAF
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_set_options routine
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

#ifdef SGIMIPS
PRIVATE void rpc__naf_addr_set_options (
unsigned_char_p_t       network_options,
rpc_addr_p_t            *rpc_addr,
unsigned32              *status)
#else
PRIVATE void rpc__naf_addr_set_options (network_options, rpc_addr, status)
    
unsigned_char_p_t       network_options;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif

{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[(*rpc_addr)->sa.family].epv->naf_addr_set_options)
        (network_options, rpc_addr, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_inq_options
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to obtain the network
**  options from an RPC Address.
**
**  INPUTS:
**
**      rpc_addr        The RPC Address from which to obtain the network
**                      options in the format of a particular NAF
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      network_options A network options string
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF addr_inq_options routine
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

#ifdef SGIMIPS
PRIVATE void rpc__naf_addr_inq_options (
rpc_addr_p_t            rpc_addr,
unsigned_char_t         **network_options,
unsigned32              *status)
#else
PRIVATE void rpc__naf_addr_inq_options (rpc_addr, network_options, status)
    
rpc_addr_p_t            rpc_addr;
unsigned_char_t         **network_options;
unsigned32              *status;
#endif

{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_addr_inq_options)
        (rpc_addr, network_options, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_desc_inq_addr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to return a vector
**  of RPC Addresses that reflect all the local network addresses
**  the supplied socket descriptor will listen on.  The endpoints
**  in each of the returned RPC Addresses will be the same.  If
**  the local host has multiple network addresses, each will be
**  returned in a separate RPC Addresses.
**
**  INPUTS:
**
**      protseq_id      The RPC Protocol Sequence ID which represents a
**                      NAF, NAF Transport Protocol, and NAF Interface Type
**
**      desc            The network descriptor which was returned from the
**                      NAF Service
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr_vec    The returned vector of RPC Addresses
**
**      status          A value indicating the return status of the routine
**
**          Any of the values returned by the NAF desc_inq_addr routine
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

#ifdef SGIMIPS
PRIVATE void rpc__naf_desc_inq_addr (
rpc_protseq_id_t        protseq_id,
rpc_socket_t            desc,
rpc_addr_vector_p_t     *rpc_addr_vec,
unsigned32              *status)
#else
PRIVATE void rpc__naf_desc_inq_addr (protseq_id, desc, rpc_addr_vec, status)
    
rpc_protseq_id_t        protseq_id;
rpc_socket_t            desc;
rpc_addr_vector_p_t     *rpc_addr_vec;
unsigned32              *status;
#endif

{
    rpc_naf_id_t naf_id;                                     

    naf_id = RPC_PROTSEQ_INQ_NAF_ID(protseq_id);

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_desc_inq_addr)
        (protseq_id, desc, rpc_addr_vec, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_desc_inq_network
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**
**  Dispatch to a Network Address Family Service to return the network
**  interface type and network protocol family for a descriptor (socket).
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      naf_id          network address family id
**
**      socket_type     network interface type id
**
**      protocol_id     network protocol family id
**
**      status          status returned by called routines
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

#ifdef SGIMIPS
PRIVATE void rpc__naf_desc_inq_network (
rpc_socket_t              desc,
rpc_naf_id_t              *naf_id,
rpc_network_if_id_t       *socket_type,
rpc_network_protocol_id_t *protocol_id,
unsigned32                *status)
#else
PRIVATE void rpc__naf_desc_inq_network
        (desc, naf_id, socket_type, protocol_id, status)

rpc_socket_t              desc;
rpc_naf_id_t              *naf_id;
rpc_network_if_id_t       *socket_type;
rpc_network_protocol_id_t *protocol_id;
unsigned32                *status;
#endif

{
    CODING_ERROR (status);

    /*
     * Determine the network address family.
     */
     
    rpc__naf_desc_inq_naf_id (desc, naf_id, status);
    if (*status != rpc_s_ok) return;
    
    /*
     * Vector to the appropriate NAF routine to ascertain the
     * rest of the network information for this descriptor.
     * If the naf_id isn't supported, set status to rpc_s_cant_get_if_id.
     */
    if (RPC_NAF_INQ_SUPPORTED(*naf_id))
    {
        (*rpc_g_naf_id[*naf_id].epv->naf_desc_inq_network)
            (desc, socket_type, protocol_id, status);
    }
    else
    {
        *status = rpc_s_cant_get_if_id;
    }
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_desc_inq_naf_id
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**
**  Return the network address family of a descriptor (socket).
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      naf_id          network address family id
**
**      status          status returned
**                              rpc_s_ok
**                              rpc_s_no_memory
**                              rpc_s_cant_inq_socket
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

PRIVATE void rpc__naf_desc_inq_naf_id 
#ifdef _DCE_PROTO_
(
    rpc_socket_t              desc,
    rpc_naf_id_t              *naf_id,
    unsigned32                *status
)
#else
(desc, naf_id, status)
rpc_socket_t              desc;
rpc_naf_id_t              *naf_id;
unsigned32                *status;
#endif
{
    rpc_addr_p_t        addr;
    unsigned8           buff[sizeof (*addr)];
    rpc_socket_error_t  serr;

    CODING_ERROR (status);

    /*
     * Get the endpoint info, ie. fill in the rpc_addr_t. We're
     * really only interested in the family returned in the socket
     * address and therefore only provide a socket address buffer 2
     * bytes long.
     */
    addr = (rpc_addr_p_t) buff;
#ifndef AIX32
    addr->len = sizeof (addr->sa.family);
#else
    addr->len = (long) (&(addr->sa.data) - &(addr->sa));
#endif /* AIX32 */
    serr = rpc__socket_inq_endpoint (desc, addr);
    if (RPC_SOCKET_IS_ERR (serr))
    {
        *status = rpc_s_cant_inq_socket;
    }
    else
    {
        *naf_id = addr->sa.family;
        *status = rpc_s_ok;
    }

}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_desc_inq_protseq_id
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**
**  Return the protocol sequence id for a descriptor (socket).
**  (We are passed the network protocol family id.)
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**      protocol_id     network protocol family id
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      protseq_id      network protocol sequence id
**
**      status          status returned
**                              rpc_s_ok
**                              rpc_s_invalid_rpc_protseq
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

PRIVATE void rpc__naf_desc_inq_protseq_id 
#ifdef _DCE_PROTO_
(
    rpc_socket_t              desc,
    rpc_network_protocol_id_t protocol_id,
    rpc_protseq_id_t         *protseq_id,
    unsigned32               *status
)
#else
(desc, protocol_id, protseq_id, status)
rpc_socket_t              desc;
rpc_network_protocol_id_t protocol_id;
rpc_protseq_id_t         *protseq_id;
unsigned32               *status;
#endif
{
    rpc_protseq_id_t         i;           /* index into protseq table */
    rpc_naf_id_t             naf_id;      /* naf id returned to us */
    rpc_network_if_id_t      socket_type; /* network protocol type */
    
    CODING_ERROR (status);

    /*
     * Determine the correct network address family.
     */
     
    rpc__naf_desc_inq_naf_id (desc, &naf_id, status);
    if (*status != rpc_s_ok) return;

    /*
     * Determine the rest of the network stuff.
     */
     
    rpc__naf_desc_inq_network
        (desc, &naf_id, &socket_type, &protocol_id, status);
    if (*status != rpc_s_ok) return;

    /*
     * Now that we've looked everything up, determine the protocol
     * sequence id by scanning the protocol sequence table for a match
     * on the network address family id, socket type (rpc protocol id)
     * and the network protocol id.
     */

    for (i=0; i<RPC_C_PROTSEQ_ID_MAX; i++)
    {
        if ((naf_id == rpc_g_protseq_id[i].naf_id) &&
            (protocol_id == rpc_g_protseq_id[i].network_protocol_id) &&
            (socket_type == rpc_g_protseq_id[i].network_if_id))
        {
            break;
        }
    }

    /*
     * Check the results of the scan, set status and return protseq_id.
     */

     if (i<RPC_C_PROTSEQ_ID_MAX)
     {
        *protseq_id = rpc_g_protseq_id[i].rpc_protseq_id;
        *status = rpc_s_ok;
     }
     else
     {
        *protseq_id = RPC_C_INVALID_PROTSEQ_ID;
        *status = rpc_s_invalid_rpc_protseq;
     }
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_desc_inq_peer_addr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**
**  Return the network address (name) of a connected peer.
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**      protseq_id      network protocol sequence id
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      addr            peer address
**
**      status          status returned
**                              rpc_s_ok
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

PRIVATE void rpc__naf_desc_inq_peer_addr 
#ifdef _DCE_PROTO_
(
    rpc_socket_t             desc,
    rpc_protseq_id_t         protseq_id,
    rpc_addr_p_t            *addr,
    unsigned32              *status
)
#else
(desc, protseq_id, addr, status)
rpc_socket_t             desc;
rpc_protseq_id_t         protseq_id;
rpc_addr_p_t            *addr;
unsigned32              *status;
#endif
{
    rpc_naf_id_t naf_id;                                     

    naf_id = RPC_PROTSEQ_INQ_NAF_ID(protseq_id);

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_desc_inq_peer_addr)
        (protseq_id, desc, addr, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_inq_max_tsdu
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to find out the size
**  of the largest possible transport service data unit (TSDU); i.e.,
**  the size of the largest transport protocol data unit (TPDU) that
**  can be passed through the transport service interface.  ("data units"
**  excludes all header bytes.)
**
**  INPUTS:
**
**      pseq_id         The protocol sequence of interest
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      max_tsdu        A pointer to where the maximum TSDU should be stored.
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_inq_max_tsdu 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t pseq_id,
    unsigned32 *max_tsdu,
    unsigned32     *status
)
#else
(pseq_id, max_tsdu, status)
rpc_protseq_id_t pseq_id;
unsigned32 *max_tsdu;
unsigned32     *status;
#endif
{
    rpc_naf_id_t naf_id;                                     
    rpc_network_if_id_t iftype;
    rpc_network_protocol_id_t protocol;

    naf_id   = RPC_PROTSEQ_INQ_NAF_ID(pseq_id);
    iftype   = RPC_PROTSEQ_INQ_NET_IF_ID(pseq_id);
    protocol = RPC_PROTSEQ_INQ_NET_PROT_ID(pseq_id);

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_inq_max_tsdu)
        (naf_id, iftype, protocol, max_tsdu, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_inq_max_pth_unfrg_tpdu
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to find out the size
**  of the largest transport protocol data unit (TPDU) that can be sent
**  to a particular address without being fragmented along the way.
**
**  INPUTS:
**   
**      rpc_addr        The address that forms the path of interest
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      max_tpdu        A pointer to where the maximum TPDU should be stored.
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_inq_max_pth_unfrg_tpdu 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t rpc_addr,
    unsigned32 *max_tpdu,
    unsigned32     *status
)
#else
(rpc_addr, max_tpdu, status)
rpc_addr_p_t rpc_addr;
unsigned32 *max_tpdu;
unsigned32     *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_inq_max_pth_unfrg_tpdu)
        (rpc_addr, RPC_PROTSEQ_INQ_NET_IF_ID(rpc_addr->rpc_protseq_id), 
        RPC_PROTSEQ_INQ_NET_PROT_ID(rpc_addr->rpc_protseq_id), max_tpdu, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_inq_max_loc_unfrg_tpdu
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to find out the
**  size of the largest transport protocol data unit (TPDU) that can
**  fit (without fragmentation) through the "widest" network interface
**  on the local host.
**
**  INPUTS:
**
**      pseq_id         The protocol sequence of interest
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      max_tpdu        A pointer to where the maximum TPDU should be stored.
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_inq_max_loc_unfrg_tpdu 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t pseq_id,
    unsigned32 *max_tpdu,
    unsigned32     *status
)
#else
(pseq_id, max_tpdu, status)
rpc_protseq_id_t pseq_id;
unsigned32 *max_tpdu;
unsigned32     *status;
#endif
{
    rpc_naf_id_t naf_id;                                     
    rpc_network_if_id_t iftype;
    rpc_network_protocol_id_t protocol;

    naf_id   = RPC_PROTSEQ_INQ_NAF_ID(pseq_id);
    iftype   = RPC_PROTSEQ_INQ_NET_IF_ID(pseq_id);
    protocol = RPC_PROTSEQ_INQ_NET_PROT_ID(pseq_id);

    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_inq_max_loc_unfrg_tpdu)
        (naf_id, iftype, protocol, max_tpdu, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_get_broadcast
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to find out what the maximum
**  transport protocol data unit size is for that address family.
**
**  INPUTS:
**
**      naf_id          The Netork Address Family we're interested in.
**      prot_seq        The protocol sequence 
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      rpc_addrs       A pointer to a pointer to a structure in which to put the
**                      list of broadcast addresses.  This structure will be allocated
**                      by the address family specific routine
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_get_broadcast 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_t            naf_id,
    rpc_protseq_id_t        protseq_id,
    rpc_addr_vector_p_t     *rpc_addrs,
     unsigned32 	    *status 
)
#else
(naf_id, protseq_id, rpc_addrs, status)
rpc_naf_id_t            naf_id;
rpc_protseq_id_t        protseq_id;
rpc_addr_vector_p_t     *rpc_addrs;
unsigned32              *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[naf_id].epv->naf_get_broadcast)
        (naf_id, protseq_id, rpc_addrs, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_compare
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to determine if the two
**  input addresses are equal.
**
**  INPUTS:
**
**      addr1           The first RPC Address to be compared.
**
**      addr2           The second RPC Address to be compared.
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      status          A value indicating the return status of the routine
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     rpc_c_true if the addresses are equal
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean rpc__naf_addr_compare 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            addr1,
    rpc_addr_p_t            addr2,
    unsigned32              *status
)
#else
(addr1, addr2, status)
rpc_addr_p_t            addr1;
rpc_addr_p_t            addr2;
unsigned32              *status;
#endif
{   
    /*
     * Handle NULL pointers here.
     */
    if ((addr1 == NULL) || (addr2 == NULL))
    {
        *status = rpc_s_ok;
        if (addr1 == addr2) return true;
        else return false;
    }

    /*
     * dispatch to the appropriate NAF service
     */
    if ((*rpc_g_naf_id[addr1->sa.family].epv->naf_addr_compare)
        (addr1, addr2, status))
        return true;
    else
        return false;
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_set_pkt_nodelay
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to set the option
**  for the transport which will result in the transport "pushing" onto
**  the network everything it is given as it is given.
**  input addresses are equal.
**
**  INPUTS:
**
**      desc            The network descriptor representing the
**                      transport on which to set the option.
**      rpc_addr        The rpc address of the remote side of the
**                      connection. If NULL it will be determined
**                      from the network descriptor.
**
**  INPUTS/OUTPUTS:
**
**      none
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

PRIVATE void rpc__naf_set_pkt_nodelay 
#ifdef _DCE_PROTO_
(
    rpc_socket_t            desc,
    rpc_addr_p_t            rpc_addr,
    unsigned32              *status
)
#else
(desc, rpc_addr, status)
rpc_socket_t            desc;
rpc_addr_p_t            rpc_addr;
unsigned32              *status;
#endif
{   
    rpc_naf_id_t            naf_id;

    /*
     * dispatch to the appropriate NAF service
     */
    if (rpc_addr == NULL)
    {
        rpc__naf_desc_inq_naf_id (desc, &naf_id, status);
        if (*status != rpc_s_ok) return;
    }
    else
    {
        naf_id = rpc_addr->sa.family;
    }
    (*rpc_g_naf_id[naf_id].epv->naf_set_pkt_nodelay)
        (desc, status);
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_is_connect_closed
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to determine
**  whether the connection represented by the network descriptor given
**  is closed.
**
**  INPUTS:
**
**      desc            The network descriptor representing the
**                      connection.
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:           
**
**      status          A value indicating the return status of the routine
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     
**
**      boolean         true if the connection is closed, false otherwise.
**
**  SIDE EFFECTS:       none
**
**--
**/

PRIVATE boolean rpc__naf_is_connect_closed 
#ifdef _DCE_PROTO_
(
    rpc_socket_t            desc,
    unsigned32              *status
)
#else
(desc, status)
rpc_socket_t            desc;
unsigned32              *status;
#endif
{   
    rpc_naf_id_t            naf_id;

    /*
     * dispatch to the appropriate NAF service
     */
    rpc__naf_desc_inq_naf_id (desc, &naf_id, status);
    if (*status != rpc_s_ok) return TRUE; /* ??? */
    return ((*rpc_g_naf_id[naf_id].epv->naf_is_connect_closed)
            (desc, status));
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_addr_from_sa
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Create an RPC addr from a aockaddr.
**
**  INPUTS:
**
**      sockaddr        The sockaddr to use in the created RPC addr
**
**      sockaddr_len    Length of "sockaddr"
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr        Returned RPC addr
**
**      status          A value indicating the return status of the routine:
**                          rpc_s_ok
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

PRIVATE void rpc__naf_addr_from_sa 
#ifdef _DCE_PROTO_
(
    sockaddr_p_t     sockaddr,
    unsigned32       sockaddr_len,
    rpc_addr_p_t     *rpc_addr,
    unsigned32     *status
)
#else
(sockaddr, sockaddr_len, rpc_addr, status)
sockaddr_p_t     sockaddr;
unsigned32       sockaddr_len;
rpc_addr_p_t     *rpc_addr;
unsigned32       *status;
#endif
{
    /*
     * Allocate memory for an RPC address.
     */
    RPC_MEM_ALLOC (
        *rpc_addr,
        rpc_addr_p_t,
#ifdef SGIMIPS
        (sockaddr_len + (int)sizeof(rpc_protseq_id_t) 
			+ (int)sizeof(unsigned32)),
#else
        (sockaddr_len + sizeof(rpc_protseq_id_t) + sizeof(unsigned32)),
#endif
        RPC_C_MEM_RPC_ADDR,
        RPC_C_MEM_WAITOK);
    if (*rpc_addr == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    /*
     * Copy the sockaddr into the RPC addr.
     */
    memcpy (&((*rpc_addr)->sa), sockaddr, sockaddr_len);

    /*
     * Set the sockaddr length into the RPC addr.
     */
    (*rpc_addr)->len = sockaddr_len;

    *status = rpc_s_ok;
    return;
}
/*
**++
**
**  ROUTINE NAME:       rpc__naf_tower_flrs_from_addr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to create lower tower 
**  floors from an RPC addr.
**
**  INPUTS:
**
**      rpc_addr	RPC addr to convert to lower tower floors.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      lower_flrs      The returned lower tower floors.
**
**      status          A value indicating the return status of the routine:
**                          rpc_s_ok
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

PRIVATE void rpc__naf_tower_flrs_from_addr 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t       rpc_addr,
    twr_p_t            *lower_flrs,
    unsigned32         *status
)
#else
(rpc_addr, lower_flrs, status)
rpc_addr_p_t       rpc_addr;
twr_p_t            *lower_flrs;
unsigned32         *status;
#endif
{
    /*
     * Dispatch to the appropriate NAF service tower_flrs_from_addr routine
     * passing through the args (rpc_addr, lower_flrs, status):
     */

    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_tower_flrs_from_addr)
       (rpc_addr, lower_flrs, status);
}

/*
**++
**
**  ROUTINE NAME:       rpc__naf_tower_flrs_to_addr
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to create an RPC addr
**  from a protocol tower.
**
**  INPUTS:
**
**      tower_octet_string
**                      Protocol tower octet string whose floors are used to
**                      construct an RPC addr
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      rpc_addr	RPC addr constructed from a protocol tower.
**
**      status          A value indicating the return status of the routine:
**                          rpc_s_ok
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

PRIVATE void rpc__naf_tower_flrs_to_addr 
#ifdef _DCE_PROTO_
(
  byte_p_t           tower_octet_string,
  rpc_addr_p_t       *rpc_addr,
  unsigned32         *status
)
#else
(tower_octet_string, rpc_addr, status)
byte_p_t           tower_octet_string;
rpc_addr_p_t       *rpc_addr;
unsigned32         *status;
#endif
{
    rpc_protseq_id_t  protseq_id;
    rpc_naf_id_t      naf_id;
    unsigned16        flr_count;
    rpc_tower_ref_p_t tower_ref;
    unsigned32        temp_status;

    CODING_ERROR (status);

#ifdef RPC_NO_TOWER_SUPPORT

    *status = rpc_s_coding_error;

#else

    /*
     * Need a tower_ref_t in order to determine the protocol sequence for
     * this tower. 
     */

    /*
     * Get the floor count from the octet string and convert it to local
     * endian rep.
     */
    memcpy (&flr_count, tower_octet_string, RPC_C_TOWER_FLR_COUNT_SIZE);

    if (NDR_LOCAL_INT_REP != ndr_c_int_little_endian)
    {
        SWAB_INPLACE_16 ((flr_count));
    }

    /*
     * Allocate a tower_ref_t, with flr_count floors and 
     * initialize the tower ref with the tower_octet_string. We are in
     * the import path here, we must have a complete tower. So start at
     * floor 1.
     * beginning at floor 1.
     */
    rpc__tower_ref_alloc (tower_octet_string, (unsigned32) flr_count, 1, 
        &tower_ref, status);

    if (*status != rpc_s_ok)
    {
        return;
    }

    /*
     * Get the protocol sequence id for this protocol tower.
     */
    rpc__tower_ref_inq_protseq_id (tower_ref, &protseq_id, status);

    if (*status != rpc_s_ok)
    {
        rpc__tower_ref_free (&tower_ref, &temp_status);
        return;
    }

    /*
     * Done with the tower ref
     */
    rpc__tower_ref_free (&tower_ref, status);

    if (*status != rpc_s_ok)
    {
        return;
    }

    if (! RPC_PROTSEQ_INQ_SUPPORTED(protseq_id))
    {
        *status = rpc_s_protseq_not_supported;
        return;
    }

    /*
     * Get the NAF ID from the protocol sequence.
     */
    naf_id = RPC_PROTSEQ_INQ_NAF_ID (protseq_id);

    /* Dispatch to the appropriate NAF service tower_flrs_to_addr routine
     * passing through the args (tower, rpc_addr, status):
     */
    (*rpc_g_naf_id[naf_id].epv->naf_tower_flrs_to_addr)
       (tower_octet_string, rpc_addr, status);

    if (*status != rpc_s_ok)
    {
        return;
    }

    /* 
     * Set the protocol sequence id into the RPC addr.
     */
    (*rpc_addr)->rpc_protseq_id = protseq_id;

#endif

    return;
}


/*
**++
**
**  ROUTINE NAME:       rpc__naf_set_port_restriction
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to set a protocol
**  sequence's port restriction structure.
**  
**
**  INPUTS:
**
**      protseq_id
**                      The protocol sequence id to set port restriction
**                      on. 
**      n_elements
**                      The number of port ranges passed in.
**
**      first_port_name_list
**                      An array of pointers to strings containing the
**                      lower bound port names. 
**
**      last_port_name_list
**                      An array of pointers to strings containing the
**                      upper bound port names.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       
**
**--
**/

PRIVATE void rpc__naf_set_port_restriction 
#ifdef _DCE_PROTO_
(
     rpc_protseq_id_t            protseq_id,
     unsigned32                  n_elements,
     unsigned_char_p_t           *first_port_name_list,
     unsigned_char_p_t           *last_port_name_list,
     unsigned32                  *status
)               
#else 
(protseq_id, n_elements, first_port_name_list, last_port_name_list, status)
rpc_protseq_id_t            protseq_id;
unsigned32                  n_elements;
unsigned_char_p_t           *first_port_name_list;
unsigned_char_p_t           *last_port_name_list;
unsigned32                  *status;
#endif
{

    rpc_naf_id_t        naf_id;

    /* 
     * Dispatch to appropriate NAF service.
     */

    naf_id = RPC_PROTSEQ_INQ_NAF_ID (protseq_id);

    (*rpc_g_naf_id[naf_id].epv->naf_set_port_restriction) 
        (protseq_id,
         n_elements,            
         first_port_name_list, 
         last_port_name_list,  
         status);
}                                       /* rpc__naf_set_port_restriction */


/*
**++
**
**  ROUTINE NAME:       rpc__naf_get_next_restricted_port
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Provides the next restricted port to try to bind to.  
**
**  INPUTS:
**
**      protseq_id
**                      The protocol sequence id of that the caller is
**                      trying to bind to.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      port_name
**                      A pointer to an address-family-specific port name.  
**
**      status
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

PRIVATE void rpc__naf_get_next_restricted_port
#ifdef _DCE_PROTO_
(
     rpc_protseq_id_t            protseq_id,
     unsigned_char_p_t           *port_name,
     unsigned32                  *status
)
#else
( protseq_id, port_name, status)
 rpc_protseq_id_t            protseq_id;
 unsigned_char_p_t           *port_name;
 unsigned32                  *status;
#endif
{

    rpc_naf_id_t        naf_id;

    /* 
     * Dispatch to appropriate NAF service.
     */

    naf_id = RPC_PROTSEQ_INQ_NAF_ID (protseq_id);

    (*rpc_g_naf_id[naf_id].epv->naf_get_next_restricted_port) 
        (protseq_id, port_name, status);
    
}                                       /* rpc__naf_get_next_restricted_port */


/*
**++
**
**  ROUTINE NAME:       rpc__naf_inq_max_frag_size
**
**  SCOPE:              PRIVATE - declared in com.h
**
**  DESCRIPTION:
**      
**  Dispatch to a Network Address Family Service to find out the size
**  of the largest fragment size that can be sent to a particular
**  address
**
**  INPUTS:
**
**      rpc_addr        The address that forms the path of interest
**
**  INPUTS/OUTPUTS:
**
**      none
**
**  OUTPUTS:
**
**      max_frag_size   A pointer to where the maximum fragment size
**                      should be stored.
**
**      status          A value indicating the return status of the routine
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

PRIVATE void rpc__naf_inq_max_frag_size
#ifdef _DCE_PROTO_
(
 rpc_addr_p_t rpc_addr,
 unsigned32   *max_frag_size,
 unsigned32   *status
)
#else
(rpc_addr, max_frag_size, status)
rpc_addr_p_t rpc_addr;
unsigned32   *max_frag_size;
unsigned32   *status;
#endif
{
    /*
     * dispatch to the appropriate NAF service
     */
    (*rpc_g_naf_id[rpc_addr->sa.family].epv->naf_inq_max_frag_size)
        (rpc_addr, max_frag_size, status);
}
