/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: ipnaf.c,v 65.7 1998/04/01 14:17:03 gwehrman Exp $";
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
 * $Log: ipnaf.c,v $
 * Revision 65.7  1998/04/01 14:17:03  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added a cast of the last parameter in a call to
 * 	rpc__ifaddr_from_destaddr() to an integer pointer to match the
 * 	declared parameter type.
 *
 * Revision 65.6  1998/03/23  17:30:23  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/03/21 19:15:07  lmc
 * Changed the use of "#ifdef DEBUG" to "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)" to turn on debugging in rpc.
 * This allows debug rpc messages to be turned on without
 * getting the kernel debugging that comes with "DEBUG".
 * "DEBUG" can change the size of some kernel data structures.
 *
 * Revision 65.4  1998/01/07  17:21:37  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:38  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:00  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.112.2  1996/02/18  00:04:15  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:36  marty]
 *
 * Revision 1.1.112.1  1995/12/08  00:20:32  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1995/04/02  01:15 UTC  lmm
 * 	Merge allocation failure detection functionality into Mothra
 * 
 * 	HP revision /main/HPDCE02/lmm_rpc_alloc_fail_detect/1  1995/04/02  00:19 UTC  lmm
 * 	add memory allocation failure detection
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:16 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  17:02 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:59:26  root]
 * 
 * Revision 1.1.110.4  1994/06/21  21:54:07  hopkins
 * 	Serviceability
 * 	[1994/06/08  21:32:28  hopkins]
 * 
 * Revision 1.1.110.3  1994/05/27  15:36:47  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:57:22  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	Added inq_max_frag_size().
 * 	Added the call to rpc__ip_init_local_addr_vec() in
 * 	rpc__ip_init().
 * 	[1994/04/29  19:23:01  tatsu_s]
 * 
 * Revision 1.1.110.2  1994/05/19  21:14:47  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:41:56  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:21:38  hopkins]
 * 
 * Revision 1.1.110.1  1994/01/21  22:37:39  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:46  cbrooks]
 * 
 * Revision 1.1.9.1  1993/09/28  21:58:16  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:40  weisman]
 * 
 * Revision 1.1.6.2  1993/07/30  18:03:44  ganni
 * 	initialize the varibale delay in set_pkt_nodelay()
 * 	[1993/07/30  18:02:00  ganni]
 * 
 * Revision 1.1.4.3  1993/01/03  23:25:36  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:07:29  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  20:51:17  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:13  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:21:57  rsalz
 * 	  6-mar-92 wh        DCE 1.0.1 merge.
 * 	 30-jan-92 ko        Add desc_inq_peer_addr entry point.
 * 	[1992/05/01  17:17:22  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:10:47  devrcs
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
**      ipnaf.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This module contains routines specific to the Internet Protocol
**  and the Internet Network Address Family extension service.
**  An initialization routine is provided to be called at RPC
**  initialization time provided, the Internet Protocol is supported
**  on the local host platform.  The remaining routines are entered
**  through an Entry Point Vector specific to the Internet Protocol.
**
**
*/

#include <commonp.h>
#include <com.h>
#include <comnaf.h>
#include <ipnaf.h>

#ifndef DO_NOT_ALLOW_HOSTNAMES
#  include <netdb.h>
#endif

#include <dce/assert.h>


/***********************************************************************
 *
 *  Macros for sprint/scanf substitutes.
 */

#ifndef NO_SSCANF
#  define RPC__IP_ENDPOINT_SSCANF   sscanf
#else
#  define RPC__IP_ENDPOINT_SSCANF   rpc__ip_endpoint_sscanf
#endif

#ifndef NO_SPRINTF
#  define RPC__IP_ENDPOINT_SPRINTF  sprintf
#  define RPC__IP_NETWORK_SPRINTF   sprintf
#else
#  define RPC__IP_ENDPOINT_SPRINTF  rpc__ip_endpoint_sprintf
#  define RPC__IP_NETWORK_SPRINTF   rpc__ip_network_sprintf
#endif
#ifdef KERNEL
extern int RPC__IP_ENDPOINT_SSCANF(char *, char *, unsigned32 *); 
extern int RPC__IP_ENDPOINT_SPRINTF(char *, char *, int);
extern int RPC__IP_NETWORK_SPRINTF(char *, char *, int, int, int, int);
#endif /* KERNEL */


/***********************************************************************
 *
 *  Routine Prototypes for the Internet Extension service routines.
 */

INTERNAL void addr_alloc _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t             /*rpc_protseq_id*/,
        rpc_naf_id_t                 /*naf_id*/,
        unsigned_char_p_t            /*endpoint*/,
        unsigned_char_p_t            /*netaddr*/,
        unsigned_char_p_t            /*network_options*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_copy _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*srpc_addr*/,
        rpc_addr_p_t                * /*drpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_free _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_set_endpoint _DCE_PROTOTYPE_ ((
        unsigned_char_p_t            /*endpoint*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_inq_endpoint _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*endpoint*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_set_netaddr _DCE_PROTOTYPE_ ((
        unsigned_char_p_t            /*netaddr*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_inq_netaddr _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*netaddr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_set_options _DCE_PROTOTYPE_ ((
        unsigned_char_p_t            /*network_options*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void addr_inq_options _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned_char_t             ** /*network_options*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void desc_inq_network _DCE_PROTOTYPE_ ((
        rpc_socket_t                 /*desc*/,
        rpc_network_if_id_t         * /*socket_type*/,
        rpc_network_protocol_id_t   * /*protocol_id*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void inq_max_tsdu _DCE_PROTOTYPE_ ((
        rpc_naf_id_t                 /*naf_id*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tsdu*/,
        unsigned32                  * /*status*/
    ));

INTERNAL boolean addr_compare _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*addr1*/,
        rpc_addr_p_t                 /*addr2*/,
        unsigned32                  * /*status*/
    ));
      
INTERNAL void inq_max_pth_unfrag_tpdu _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tpdu*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void inq_max_loc_unfrag_tpdu _DCE_PROTOTYPE_ ((
        rpc_naf_id_t                 /*naf_id*/,
        rpc_network_if_id_t          /*iftype*/,
        rpc_network_protocol_id_t    /*protocol*/,
        unsigned32                  * /*max_tpdu*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void set_pkt_nodelay _DCE_PROTOTYPE_ ((
        rpc_socket_t                 /*desc*/,
        unsigned32                  * /*status*/
    ));
              
INTERNAL boolean is_connect_closed _DCE_PROTOTYPE_ ((
        rpc_socket_t                 /*desc*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void tower_flrs_from_addr _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        twr_p_t                     * /*lower_flrs*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void tower_flrs_to_addr _DCE_PROTOTYPE_ ((
        byte_p_t                     /*tower_octet_string*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void desc_inq_peer_addr _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t             /*protseq_id*/,
        rpc_socket_t                 /*desc*/,
        rpc_addr_p_t                * /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void set_port_restriction _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t             /*protseq_id*/,
        unsigned32                   /*n_elements*/,
        unsigned_char_p_t           * /*first_port_name_list*/,
        unsigned_char_p_t           * /*last_port_name_list*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void get_next_restricted_port _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t             /*protseq_id*/,
        unsigned_char_p_t           * /*port_name*/,
        unsigned32                  * /*status*/
    ));

INTERNAL void inq_max_frag_size _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*max_frag_size*/,
        unsigned32                  * /*status*/
    ));



/*
**++
**
**  ROUTINE NAME:       rpc__ip_init
**
**  SCOPE:              PRIVATE - EPV declared in ipnaf.h
**
**  DESCRIPTION:
**      
**  Internet Address Family Initialization routine, rpc__ip_init, is
**  calld only once, by the Communications Service initialization
**  procedure,  at the time RPC is initialized.  If the Communications
**  Service initialization determines that the Internet protocol
**  family is supported on the local host platform it will call this
**  initialization routine.  It is responsible for all Internet
**  specific initialization for the current RPC.  It will place in
**  Network Address Family Table, a pointer to the Internet family Entry
**  Point Vector.  Afterward all calls to the IP extension service
**  routines will be vectored through this EPV.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**      naf_epv         The address of a pointer in the Network Address Family
**                      Table whre the pointer to the Entry Point Vectorto
**                      the IP service routines is inserted by this routine.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**          Any of the RPC Protocol Service status codes.
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

PRIVATE void  rpc__ip_init 
#ifdef _DCE_PROTO_
(
    rpc_naf_epv_p_t         *naf_epv,
    unsigned32              *status
)
#else
(naf_epv, status)  
rpc_naf_epv_p_t         *naf_epv;
unsigned32              *status;
#endif
{
    /*  
     * The Internal Entry Point Vectors for the Internet Protocol Family
     * Extension service routines.  At RPC startup time, the IP init routine,
     * rpc__ip_init, is responsible for inserting  a pointer to this EPV into
     * the  Network Address Family Table.  Afterward,  all calls to the IP
     * Extension  Service are vectored through these  EPVs.
     */

    static rpc_naf_epv_t rpc_ip_epv =
    {
        addr_alloc,
        addr_copy,
        addr_free,
        addr_set_endpoint,
        addr_inq_endpoint,
        addr_set_netaddr,
        addr_inq_netaddr,
        addr_set_options,
        addr_inq_options,
        rpc__ip_desc_inq_addr,
        desc_inq_network,
        inq_max_tsdu,
        rpc__ip_get_broadcast,
        addr_compare,
        inq_max_pth_unfrag_tpdu,
        inq_max_loc_unfrag_tpdu,
        set_pkt_nodelay,
        is_connect_closed,
        tower_flrs_from_addr,
        tower_flrs_to_addr,
        desc_inq_peer_addr,
        set_port_restriction,
        get_next_restricted_port,
        inq_max_frag_size
    };      
    unsigned32 lstatus;

    rpc__ip_init_local_addr_vec (&lstatus);

    /*
     * place the address of EPV into Network Address Family Table
     */
    *naf_epv = &rpc_ip_epv;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_alloc
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Create a copy of an RPC address. Allocate memory for a variable
**  length RPC address, for the IP service.  Insert the Internet
**  address and endpoint along with the overall length of the allocated
**  memory, together with any additional parameters required by the IP
**  service.
**
**  INPUTS:
**
**      rpc_protseq_id  Protocol Sequence ID representing an IP Network
**                      Address Family, its Transport Protocol, and type.
**
**      naf_id          Network Address Family ID serves as index into
**                      EPV for IP routines.
**
**      endpoint        String containing endpoint to insert into newly
**                      allocated RPC address.
**
**      netaddr         String containing Internet format network
**                      address to be inserted in RPC addr.
**
**      network_options String containing options to be placed in
**                      RPC address.  - Not used by IP service.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address -
**                      returned with the address of the memory
**                      allocated by this routine. 
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
**
**          rpc_s_no_memory     Call to malloc failed to allocate memory
**
**          Any of the RPC Protocol Service status codes.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**
**--
**/

INTERNAL void addr_alloc 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t        rpc_protseq_id,
    rpc_naf_id_t            naf_id,
    unsigned_char_p_t       endpoint,
    unsigned_char_p_t       netaddr,
    unsigned_char_p_t       network_options,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(rpc_protseq_id, naf_id, endpoint, netaddr, network_options, rpc_addr, status)
rpc_protseq_id_t        rpc_protseq_id;
rpc_naf_id_t            naf_id;
unsigned_char_p_t       endpoint;
unsigned_char_p_t       netaddr;
unsigned_char_p_t       network_options;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    CODING_ERROR (status);
    
    /*
     * allocate memory for the new RPC address
     */

    RPC_MEM_ALLOC (
        *rpc_addr,
        rpc_addr_p_t,
        sizeof (rpc_ip_addr_t),
        RPC_C_MEM_RPC_ADDR,
        RPC_C_MEM_WAITOK);

    if (*rpc_addr == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * zero allocated memory
     */
    /* b_z_e_r_o ((unsigned8 *) *rpc_addr, sizeof (rpc_ip_addr_t));*/

    memset( *rpc_addr, 0, sizeof (rpc_ip_addr_t));

    /*
     * insert id, length, family into rpc address
     */
    (*rpc_addr)->rpc_protseq_id = rpc_protseq_id;
    (*rpc_addr)->len = sizeof (struct sockaddr_in);
    (*rpc_addr)->sa.family = naf_id;
    
    /*
     * set the endpoint in the RPC addr
     */
    addr_set_endpoint (endpoint, rpc_addr, status);
    if (*status != rpc_s_ok) return;

    /*
     * set the network address in the RPC addr
     */
    addr_set_netaddr (netaddr, rpc_addr, status);
    if (*status != rpc_s_ok) return;
    
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_copy
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Obtain the length from the source RPC address.  Allocate memory for a
**  new, destination  RPC address. Do a byte copy from the surce address
**  to the destination address.
**
**  INPUTS:
**
**     src_rpc_addr     The address of a pointer to an RPC address to be
**                      copied.  It must be the correct format for Internet
**                      Protocol. 
**
**  INPUTS/OUTPUTS:
**
**     dst_rpc_addr     The address of a pointer to an RPC address -returned
**                      with the address of the memory allocated by
**                      this routine. 
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**      rpc_s_ok            The call was successful.
**
**      rpc_s_no_memory     Call to malloc failed to allocate memory
**
**      rpc_s_invalid_naf_id  Source RPC address appeared invalid
**
**
**  IMPLICIT INPUTS:
**
**        A check is performed on the source RPC address before malloc.  It
**        must be the IP family.
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**
**           In the event, the addres of a of memory segment contained in
**           rpc_addr, is not valid or the length isn't as long as is 
**           indicated, a memory fault may result.
**
**--
**/

INTERNAL void addr_copy 
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
    CODING_ERROR (status);
    
    /*
     * if the source RPC address looks valid - IP family ok
     */
    if (src_rpc_addr->sa.family == RPC_C_NAF_ID_IP)
    {
        /*
         * allocate memory for the new RPC address
         */
        RPC_MEM_ALLOC (
            *dst_rpc_addr,
            rpc_addr_p_t,
            sizeof (rpc_ip_addr_t),
            RPC_C_MEM_RPC_ADDR,
            RPC_C_MEM_WAITOK);

        if (*dst_rpc_addr == NULL)
        {
            *status = rpc_s_no_memory;
            return;
        }

        /*
         * Copy source rpc address to destination rpc address
         */
        /* b_c_o_p_y ((unsigned8 *) src_rpc_addr, (unsigned8 *) *dst_rpc_addr,
                sizeof (rpc_ip_addr_t));*/

        memmove( *dst_rpc_addr, src_rpc_addr, sizeof (rpc_ip_addr_t));


        *status = rpc_s_ok;
        return;
    }

    *status = rpc_s_invalid_naf_id;
}

/*
**++
**
**  ROUTINE NAME:       addr_free
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Free the memory for the RPC address pointed to by the argument
**  address pointer rpc_addr.  Null the address pointer.  The memory
**  must have been previously allocated by RPC_MEM_ALLC.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:
**
**     rpc_addr         The address of a pointer to an RPC address -returned
**                      with a NULL value.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok        The call was successful.
**
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       
**           In the event, the segment of memory refered to by pointer
**           rpc_addr, is allocated by means other than RPC_MEM_ALLOC,
**           unpredictable results will occur when this routine is called.
**--
**/

INTERNAL void addr_free 
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
    CODING_ERROR (status);
    
    /*
     * free memory of RPC addr
     */
    RPC_MEM_FREE (*rpc_addr, RPC_C_MEM_RPC_ADDR);

    /*
     * indicate that the rpc_addr is now empty
     */
    *rpc_addr = NULL;

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_set_endpoint
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive the null terminated ascii character string,  rpc_endpoint
**    and convert to the Internet Protocol byte order format.  Insert
**    into the RPC address, pointed to by argument rpc_addr.  The only
**    acceptible endpoint for IP is numeric asci string, or a NULL string.
**
**  INPUTS:
**
**      endpoint        String containing endpoint to insert into RPC address.
**                      For IP must contain an ASCII numeric value, or NULL.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the endpoint is to be inserted.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok                 The call was successful.
**
**          rpc_s_invalid_naf_id  Argument, endpoint contains an 
**                                  unauthorized pointer value.
**          rpc_s_invalid_endpoint_format Endpoint Argument can not be 
**                                  converted (not numeric).
**
**  IMPLICIT INPUTS:
**
**      A NULL, (first byte NULL), endpoint string is an indicator to 
**      the routine to delete the endpoint from the RPC address. indicated.
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void addr_set_endpoint 
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
    rpc_ip_addr_p_t     ip_addr = (rpc_ip_addr_p_t) *rpc_addr;
    unsigned32          ep;
    int                 ret;

    
    CODING_ERROR (status);
    
    /*
     * check to see if this is a request to remove the endpoint
     */
    if (endpoint == NULL || strlen ((char *) endpoint) == 0)
    {
        ip_addr->sa.sin_port = 0;
        *status = rpc_s_ok;
        return;
    }

    if ((strspn ((char *)endpoint, "0123456789")) 
         != (strlen ((char *)endpoint)))
    {
        *status = rpc_s_invalid_endpoint_format;
        return;
    }

    /*
     * convert the endpoint string to network format
     * and insert in RPC address
     */

#ifdef VMS
    ret = RPC__IP_ENDPOINT_SSCANF((char *) endpoint, "%d", &ep);
#else
    ret = RPC__IP_ENDPOINT_SSCANF((char *) endpoint, "%u", &ep);
#endif

    if (ret != 1)
    {
        *status = rpc_s_invalid_endpoint_format;
        return;
    }

    ip_addr->sa.sin_port = htons (ep);

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_endpoint
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    From the RPC address indicated by arg., rpc_addr, examine the
**    endpoint.  Convert the endopint value to a NULL terminated asci
**    character string to be returned in the memory segment pointed to
**    by arg., endpoint.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      to be inspected.
**
**  INPUTS/OUTPUTS:     none
**
**
**  OUTPUTS:
**
**      endpoint        String pointer indicating where the endpoint
**                      string is to be placed.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
**
**          Any of the RPC Protocol Service status codes.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:
**
**      A zero length string will be returned if the RPC address contains
**      no endpoint.
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**
**    CAUTION -- since this routine has no way of knowing the exact
**      length of the endpoint string which will be derived.  It
**      is asumed that the caller has provided, rpc_c_endpoint_max
**      (or at least "enough") space for the endpoint string.
**--
**/

INTERNAL void addr_inq_endpoint 
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
#define     RPC_C_ENDPOINT_IP_MAX   6   /* 5 ascii digits + nul */
    rpc_ip_addr_p_t     ip_addr = (rpc_ip_addr_p_t) rpc_addr;
    unsigned16          ep;


    CODING_ERROR (status);

    /*
     * convert endpoint to local platform byte order format
     */
    ep = ntohs (ip_addr->sa.sin_port);

    /*
     * if no endpoint present, return null string. Otherwise,
     * return the endpoint in Internet "dot" notation.
     */    
    if (ep  ==  0)
    {
        RPC_MEM_ALLOC(
            *endpoint,
            unsigned_char_p_t,
            sizeof(unsigned32),     /* can't stand to get just 1 byte */
            RPC_C_MEM_STRING,
            RPC_C_MEM_WAITOK);
	if (*endpoint == NULL){
	    *status = rpc_s_no_memory;
	    return;
	}
        *endpoint[0] = 0;
    }
    else
    {
        RPC_MEM_ALLOC(
            *endpoint,
            unsigned_char_p_t,
            RPC_C_ENDPOINT_IP_MAX,
            RPC_C_MEM_STRING,
            RPC_C_MEM_WAITOK);
	if (*endpoint == NULL){
	    *status = rpc_s_no_memory;
	    return;
	}
        RPC__IP_ENDPOINT_SPRINTF((char *) *endpoint, "%u", ep);
    }

    *status = rpc_s_ok;    
}

/*
**++
**
**  ROUTINE NAME:       addr_set_netaddr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive the null terminated ascii character string, netaddr,
**    and convert to the Internet Protocol Network Address format.  Insert
**    into the RPC address, indicated by argument rpc_addr.
**
**  INPUTS:
**
**      netaddr         String containing network address to insert into
**                      RPC address.  It must contain an ASCII value in the
**                      Internet dot notation, (a.b.c.d), format.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the network address is to be inserted.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**      rpc_s_ok                  The call was successful.
**      rpc_s_inval_net_addr      Invalid IP network address string passed
**                                in netaddr
**
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

INTERNAL void addr_set_netaddr 
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
    rpc_ip_addr_p_t     ip_addr = (rpc_ip_addr_p_t) *rpc_addr;
    boolean             numeric;
#ifndef DO_NOT_ALLOW_HOSTNAMES
    struct hostent      *he;
#endif


    CODING_ERROR (status);  

    /*
     * check to see if this is a request to remove the netaddr
     */
    if (netaddr == NULL || strlen ((char *) netaddr) == 0)
    {
        ip_addr->sa.sin_addr.s_addr = 0;
        *status = rpc_s_ok;
        return;
    }

    /*
     * See if there's a leading "#" -- means numeric address must follow.
     * Note we accept numeric addresses withOUT the "#" too.
     */
    numeric = (netaddr[0] == '#');
    if (numeric)
        netaddr++;

    /*
     * convert Internet dot notation address to network address
     * formatted unsigned32 - check for validity
     */
    ip_addr->sa.sin_addr.s_addr = inet_addr ((char *)netaddr);
    if (ip_addr->sa.sin_addr.s_addr != -1)
    {
        *status = rpc_s_ok;
        return;
    }

#ifdef DO_NOT_ALLOW_HOSTNAMES

    *status = rpc_s_inval_net_addr;

#else

    if (numeric)
    {
        *status = rpc_s_inval_net_addr;
        return;
    }

    he = gethostbyname ((char *) netaddr);
    if (he == NULL)
    {
        *status = rpc_s_inval_net_addr;
        return;
    }

    ip_addr->sa.sin_addr.s_addr = * (unsigned32 *) he->h_addr;

    *status = rpc_s_ok;
 
#endif
   
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_netaddr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    From the RPC address indicated by arg., rpc_addr, examine the
**    IP network address.  Convert the network address from its network
**    format  to a NULL terminated ascii character string in IP dot
**    notation format.  The character string to be returned in the
**    memory segment pointed to by arg., netaddr.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      is to be inspected.
**
**  INPUTS/OUTPUTS:
**
**
**  OUTPUTS:
**
**      netaddr         String pointer indicating where the network
**                      address string is to be placed.
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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

INTERNAL void addr_inq_netaddr 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            rpc_addr,
    unsigned_char_t         **netaddr,
    unsigned32              *status
)
#else
(rpc_addr, netaddr, status)
rpc_addr_p_t            rpc_addr;
unsigned_char_t         **netaddr;
unsigned32              *status;
#endif
{
#define NA_SIZE 16      /* big enough for 255.255.255.255 */

    rpc_ip_addr_p_t     ip_addr = (rpc_ip_addr_p_t) rpc_addr;
    unsigned8           *p;
    
    
    CODING_ERROR (status);
    
    RPC_MEM_ALLOC(
        *netaddr,
        unsigned_char_p_t,
        NA_SIZE,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
    if (*netaddr == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    /*
     * get an unsigned8 pointer to IP address - network format
     */
    p = (unsigned8 *) &(ip_addr->sa.sin_addr.s_addr);

    /*
     * convert IP address to IP dot notation string - (eg, 16.0.0.4)
     * placed in buffer indicated by arg.netaddr.
     */
    RPC__IP_NETWORK_SPRINTF((char *) *netaddr, "%d.%d.%d.%d",
        UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_set_options
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**     Receive a NULL terminated network options string and insert
**     into the RPC address indicated by art., rpc_addr.
**
**      NOTE - there are no options used with the IP service this
**             routine is here only to serve as a stub.
**
**  INPUTS:
**
**      options         String containing network options to insert
**                      into  RPC address.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address where
**                      the network options strig is to be inserted.
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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


INTERNAL void addr_set_options 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       network_options,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(network_options, rpc_addr, status)
unsigned_char_p_t       network_options;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_inq_options
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**      Extract the network options from the RPC address pointed to
**      by rpc_addr and convert to a NULL terminated string placed
**      in a buffer indicated by the options arg.
**
**      NOTE - there are no options used with the IP service this
**             routine is here only to serve as a stub.
**
**  INPUTS:
**
**      rpc_addr        The address of a pointer to an RPC address that
**                      is to be inspected.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      options         String pointer indicating where the network
**                      options string is to be placed.
**
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok           The call was successful.
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

INTERNAL void addr_inq_options 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            rpc_addr,
    unsigned_char_t         **network_options,
    unsigned32              *status
)
#else
(rpc_addr, network_options, status)
rpc_addr_p_t            rpc_addr;
unsigned_char_t         **network_options;
unsigned32              *status;
#endif
{
    RPC_MEM_ALLOC(
        *network_options,
        unsigned_char_p_t,
        sizeof(unsigned32),     /* only really need 1 byte */
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);
    if (*network_options == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    *network_options[0] = 0;
    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       inq_max_tsdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tsdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_tsdu 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_t            naf_id,
    rpc_network_if_id_t     iftype,
    rpc_network_protocol_id_t protocol,
    unsigned32              *max_tsdu,
    unsigned32              *status
)
#else
(naf_id, iftype, protocol, max_tsdu, status)
rpc_naf_id_t            naf_id;
rpc_network_if_id_t     iftype;
rpc_network_protocol_id_t protocol;
unsigned32              *max_tsdu;
unsigned32              *status;
#endif
{
    if (iftype == RPC_C_NETWORK_IF_ID_DGRAM &&
        protocol == RPC_C_NETWORK_PROTOCOL_ID_UDP)    
    {
        *max_tsdu = RPC_C_IP_UDP_MAX_TSDU;
    }
    else
    {
        assert(false);      /* !!! */
    }

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG (rpc_es_dbg_ip_max_tsdu, 1))
    {
        *max_tsdu = ((unsigned32)
            (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_tsdu])) * 1024;
    }
#endif

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       addr_compare
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Determine if two address are equal.
**
**  INPUTS:
**
**      addr1
**
**      addr2
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
**  FUNCTION VALUE:    
**
**      return          Boolean; true if address are the same.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean addr_compare 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            addr1, 
    rpc_addr_p_t            addr2,
    unsigned32              *status
)
#else
(addr1, addr2, status)
rpc_addr_p_t            addr1, addr2;
unsigned32              *status;
#endif
{
    rpc_ip_addr_p_t     ip_addr1 = (rpc_ip_addr_p_t) addr1;
    rpc_ip_addr_p_t     ip_addr2 = (rpc_ip_addr_p_t) addr2;

    if (ip_addr1->sa.sin_family == ip_addr2->sa.sin_family &&
        ip_addr1->sa.sin_port == ip_addr2->sa.sin_port &&
        ip_addr1->sa.sin_addr.s_addr == ip_addr2->sa.sin_addr.s_addr &&
        ip_addr1->sa.sin_addr.s_addr == ip_addr2->sa.sin_addr.s_addr)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/*
**++
**
**  ROUTINE NAME:       inq_max_pth_unfrag_tpdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_pth_unfrag_tpdu 
#ifdef _DCE_PROTO_
(
    rpc_addr_p_t            rpc_addr,
    rpc_network_if_id_t     iftype,
    rpc_network_protocol_id_t protocol,
    unsigned32              *max_tpdu,
    unsigned32              *status
)
#else
(rpc_addr, iftype, protocol, max_tpdu, status)
rpc_addr_p_t            rpc_addr;
rpc_network_if_id_t     iftype;
rpc_network_protocol_id_t protocol;
unsigned32              *max_tpdu;
unsigned32              *status;
#endif
{
    if (iftype == RPC_C_NETWORK_IF_ID_DGRAM &&
        protocol == RPC_C_NETWORK_PROTOCOL_ID_UDP)    
    {
#if defined(SGIMIPS) && defined(_KERNEL)
	extern int rpc__ifaddr_from_destaddr(
	    struct in_addr to,
	    struct in_addr *ifaddr,
	    int *max_tpdu);

#ifdef SGIMIPS
	if (rpc__ifaddr_from_destaddr(((struct sockaddr_in *)
			(&rpc_addr->sa))->sin_addr, NULL, (int *)max_tpdu))
#else
	if (rpc__ifaddr_from_destaddr(((struct sockaddr_in *)
			(&rpc_addr->sa))->sin_addr, NULL, max_tpdu))
#endif
	{
	    *max_tpdu = RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU;
	} else {
	    /* subtract UDP and IP headers */
	    *max_tpdu = (*max_tpdu - 28) & ~7;
	}
#else
        *max_tpdu = RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU;
#endif /* SGIMIPS && _KERNEL */
    }
    else
    {
        assert(false);      /* !!! */
    }

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG (rpc_es_dbg_ip_max_pth_unfrag_tpdu, 1))
    {
        *max_tpdu = ((unsigned32)
            (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_pth_unfrag_tpdu])) * 32;
    }
#endif

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       inq_max_loc_unfrag_tpdu
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_loc_unfrag_tpdu 
#ifdef _DCE_PROTO_
(
    rpc_naf_id_t            naf_id,
    rpc_network_if_id_t     iftype,
    rpc_network_protocol_id_t protocol,
    unsigned32              *max_tpdu,
    unsigned32              *status
)
#else
(naf_id, iftype, protocol, max_tpdu, status)
rpc_naf_id_t            naf_id;
rpc_network_if_id_t     iftype;
rpc_network_protocol_id_t protocol;
unsigned32              *max_tpdu;
unsigned32              *status;
#endif
{
    if (iftype == RPC_C_NETWORK_IF_ID_DGRAM &&
        protocol == RPC_C_NETWORK_PROTOCOL_ID_UDP)    
    {
        *max_tpdu = RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU;
    }
    else
    {
        assert(false);      /* !!! */
    }

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
    if (RPC_DBG (rpc_es_dbg_ip_max_loc_unfrag_tpdu, 1))
    {
        *max_tpdu = ((unsigned32)
            (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_loc_unfrag_tpdu])) * 32;
    }
#endif

    *status = rpc_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       desc_inq_network
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  This routine is responsible for "reverse-engineering" the parameters to
**  the original socket call that was made to create the socket "desc".
**
**  INPUTS:
**
**      desc            socket descriptor to query
**
**  INPUTS/OUTPUTS:
**
**  OUTPUTS:
**
**      socket_type     network interface type id
**
**      protocol_id     network protocol family id
**
**      status          status returned
**                              rpc_s_ok
**                              rpc_s_cant_get_if_id
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

INTERNAL void desc_inq_network 
#ifdef _DCE_PROTO_
(
    rpc_socket_t              desc,
    rpc_network_if_id_t       *socket_type,
    rpc_network_protocol_id_t *protocol_id,
    unsigned32                *status
)
#else
(desc, socket_type, protocol_id, status)
rpc_socket_t              desc;
rpc_network_if_id_t       *socket_type;
rpc_network_protocol_id_t *protocol_id;
unsigned32                *status;
#endif
{
    rpc_socket_error_t serr;
    
    CODING_ERROR (status);

    /*
     * Get the socket type.
     */
     
    serr = rpc__socket_get_if_id (desc, socket_type);
    if (RPC_SOCKET_IS_ERR (serr))
    {
        RPC_DBG_GPRINTF (("rpc__socket_get_if_id: serr=%d\n",serr));
        *status = rpc_s_cant_get_if_id;
        return;
    }

    /*
     * For now, there is a one to one relationship between the protocol family
     * and the socket type.
     */

    switch ((int)(*socket_type))
    {
        case SOCK_STREAM:       *protocol_id = RPC_C_NETWORK_PROTOCOL_ID_TCP;
                                break;

        case SOCK_DGRAM:        *protocol_id = RPC_C_NETWORK_PROTOCOL_ID_UDP;
                                break;

        default:		/*
				 * rpc_m_unk_sock_type
				 * "(%s) Unknown socket type"
				 */
				RPC_DCE_SVC_PRINTF ((
				    DCE_SVC(RPC__SVC_HANDLE, "%s"),
				    rpc_svc_general,
				    svc_c_sev_fatal | svc_c_action_abort,
				    rpc_m_unk_sock_type,
				    "desc_inq_network" ));
				break;
    }

    *status = rpc_s_ok;
}


/*
**++
**
**  ROUTINE NAME:       set_pkt_nodelay
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      desc            The network descriptor to apply the nodelay
**                      option to.
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
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void set_pkt_nodelay 
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
    int                 err;
    int                 delay = 1;

#ifdef _KERNEL
    *status = rpc_s_cannot_set_nodelay;
#else

    /*
     * Assume this is a TCP socket and corresponding connection. If
     * not the setsockopt will fail.
     */
    if ((err = setsockopt (desc, 
                           IPPROTO_TCP, 
                           TCP_NODELAY, 
                           (char *) &delay,
                           sizeof (delay))) < 0)
    {
        *status = rpc_s_cannot_set_nodelay;
    }
    else
    {
        *status = rpc_s_ok;
    }
#endif
}


/*
**++
**
**  ROUTINE NAME:       is_connect_closed
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine is called when a recv on a sequenced packet
**      socket has returned zero bytes. Since TCP does not support
**      sequenced sockets the routine is a no-op. "true" is returned
**      because zero bytes received on a stream socket does mean the
**      connection is closed.
**      
**  INPUTS:
**
**      desc            The network descriptor representing the connection.
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
**  FUNCTION VALUE:     
**
**      boolean         true if the connection is closed, false otherwise.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean is_connect_closed 
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
    *status = rpc_s_ok;
    return (true);
}


/*
**++
**
**  ROUTINE NAME:       tower_flrs_from_addr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Creates the lower tower floors from an RPC addr.
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
**      status          A value indicating the return status of the routine.
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

INTERNAL void tower_flrs_from_addr 
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
    unsigned32    net_prot_id;

    CODING_ERROR (status);

#ifdef RPC_NO_TOWER_SUPPORT

    *status = rpc_s_coding_error;

#else

#if 0
    /*
     * The use of a temporary lower floors twr_t is in anticipation
     * of the twr_* routines belonging to a separate library with
     * their own memory allocation. In that case, twr_* allocates
     * memory for returning the lower towers, and we must copy from
     * twr_* memory into our (rpc) memory. After the copy, we free
     * the twr_* allocated memory. 
     *
     * For now, twr_* routines also use RPC_MEM_ALLOC and RPC_MEM_FREE,
     * so we'll skip the extra copy.
     */
    twr_p_t     temp_lower_flrs;
#endif

    /*
     * Get the network protocol id (aka transport layer protocol)
     * for this RPC addr.
     */
    net_prot_id = RPC_PROTSEQ_INQ_NET_PROT_ID(rpc_addr->rpc_protseq_id);

    /*
     * Convert sockaddr to lower tower floors.
     */
    twr_ip_lower_flrs_from_sa (net_prot_id, (sockaddr_t *) &(rpc_addr->sa), 
#if 0
        &temp_lower_flrs, 
#else
        lower_flrs, 
#endif
        status);

    if (*status != twr_s_ok)
    {
        return;
    }

#if 0
    /*
     * Allocate a tower structure to hold the wire (and nameservice)
     * representation of the lower tower floors returned from twr_*().
     *
     * The size includes the sizof twr_t + length of the tower floors 
     * returned from twr_ip_lower_flrs_from_sa - 1 (for tower_octet_string[0].
     */
    RPC_MEM_ALLOC (
        *lower_flrs, 
        twr_p_t, 
        sizeof(twr_t) + temp_lower_flrs->tower_length - 1,
        RPC_C_MEM_TOWER, 
        RPC_C_MEM_WAITOK );
    if (*lower_flrs == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    /*
     * Set the tower length to the length of the tower flrs returnd from 
     * twr_ip_lower_flrs_from_sa.
     */
    (*lower_flrs)->tower_length = temp_lower_flrs->tower_length;

    /*
     * Copy the lower tower floors to the tower octet string.
     */
    memcpy ((*lower_flrs)->tower_octet_string, 
        temp_lower_flrs->tower_octet_string,
        temp_lower_flrs->tower_length);

    /*
     * Free the twr_ip_lower_flrs_from_sa allocated memory.
     */
    RPC_MEM_FREE (temp_lower_flrs, RPC_C_MEM_TOWER);
#endif

#endif

    return;
}

/*
**++
**
**  ROUTINE NAME:       tower_flrs_to_addr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  Creates an RPC addr from a protocol tower.
**
**  INPUTS:
**
**      tower_octet_string
**                      Protocol tower whose floors are used to construct
**                      an RPC addr
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

INTERNAL void tower_flrs_to_addr 
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
    sockaddr_t    *sa;
    unsigned32    sa_len;

    CODING_ERROR (status);

#ifdef RPC_NO_TOWER_SUPPORT

    *status = rpc_s_coding_error;

#else

    /*
     * Convert the lower floors of a tower to a sockaddr.
     */
    twr_ip_lower_flrs_to_sa (
        tower_octet_string,        /* tower octet string (has flr count). */
        &sa,                       /* returned sockaddr     */
        &sa_len,                   /* returned sockaddr len */
        status);

    if (*status != twr_s_ok)
    {
        return;
    }

    /*
     * Call the common NAF routine to create an RPC addr from a sockaddr.
     * (rpc__naf_addr_from_sa doesn't dispatch to a naf-specific routine.)
     */
    rpc__naf_addr_from_sa (sa, sa_len, rpc_addr, status);

    /*
     * Always free the twr_ip_lower_flrs_to_sa allocated memory - regardless 
     * of the status from rpc__naf_addr_from_sa.
     */
    RPC_MEM_FREE (sa, RPC_C_MEM_SOCKADDR);

#endif /* RPC_NO_TOWER_SUPPORT */

    /*
     * Return whatever status we had.
     */
    return;
}


/*
**++
**
**  ROUTINE NAME:       desc_inq_peer_addr
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**    Receive a socket descriptor which is queried to obtain family,
**    remote endpoint and remote network address.  If this information appears valid
**    for an DECnet IV address,  space is allocated for an RPC address which
**    is initialized with the information obtained from this socket.  The
**    address indicating the created RPC address is returned in, arg., rpc_addr.
**
**  INPUTS:
**
**      protseq_id             Protocol Sequence ID representing a
**                             particular Network Address Family,
**                             its Transport Protocol, and type.
**
**      desc                   Descriptor, indicating a socket that
**                             has been created on the local operating
**                             platform.
**
**  INPUTS/OUTPUTS:
**
**      rpc_addr        The address of a pointer where the RPC address
**                      created by this routine will be indicated.
**
**  OUTPUTS:
**
**      status          A value indicating the status of the routine.
**
**          rpc_s_ok               The call was successful.
**
**          rpc_s_no_memory         Call to malloc failed to allocate memory.
**
**          rpc_s_cant_get_peername Call to getpeername failed.
**
**          rpc_s_invalid_naf_id   Socket that arg desc refers is not DECnet IV.
**
**          Any of the RPC Protocol Service status codes.
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

INTERNAL void desc_inq_peer_addr 
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t        protseq_id,
    rpc_socket_t            desc,
    rpc_addr_p_t            *rpc_addr,
    unsigned32              *status
)
#else
(protseq_id, desc, rpc_addr, status)
rpc_protseq_id_t        protseq_id;
rpc_socket_t            desc;
rpc_addr_p_t            *rpc_addr;
unsigned32              *status;
#endif
{
    rpc_socket_error_t  serr;
    
    CODING_ERROR (status);


    /*
     * allocate memory for the new RPC address
     */
    RPC_MEM_ALLOC (*rpc_addr,
                   rpc_addr_p_t,
                   sizeof (rpc_ip_addr_t),
                   RPC_C_MEM_RPC_ADDR,
                   RPC_C_MEM_WAITOK);
    
    /*
     * successful malloc
     */
    if (*rpc_addr == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /*
     * insert individual parameters into RPC address
     */
    (*rpc_addr)->rpc_protseq_id = protseq_id;
    (*rpc_addr)->len = sizeof (struct sockaddr_in);

    /*
     * Get the peer address (name).
     *
     * If we encounter an error, free the address structure and return
     * the status from the getpeername() call, not the free() call.
     */

    serr = rpc__socket_getpeername (desc, *rpc_addr);
    if (RPC_SOCKET_IS_ERR (serr))
    {
        RPC_MEM_FREE (*rpc_addr, RPC_C_MEM_RPC_ADDR);
        *rpc_addr = (rpc_addr_p_t)NULL;
        *status = rpc_s_cant_getpeername;
    }
    else
    {
        *status = rpc_s_ok;
    }
}


/*
**++
**
**  ROUTINE NAME:       set_port_restriction
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Builds an rpc_port_restriction_list_t and glues it into the
**  rpc_protseq_id_elt_t in the rpc_g_protseq_id[] list.  
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
**  SIDE EFFECTS:       none
**
**--
**/


INTERNAL void set_port_restriction
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

    rpc_port_restriction_list_p_t list_p;
    rpc_port_range_element_p_t   range_elements;
    unsigned32                   i;
#ifdef KERNEL
    extern int rand(void);
#endif /* KERNEL */


    CODING_ERROR (status);

    /* 
     * It is only meaningful to do this once per protocol sequence.
     */

    if (rpc_g_protseq_id [protseq_id].port_restriction_list != NULL)
    {
        *status = rpc_s_already_registered;
        return;
    }

    /* 
     * Allocate the port_restriction_list.
     */

    RPC_MEM_ALLOC 
        (list_p,
         rpc_port_restriction_list_p_t,
         sizeof (rpc_port_restriction_list_t),
         RPC_C_MEM_PORT_RESTRICT_LIST,
         RPC_C_MEM_WAITOK);

    if (list_p == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }
                   
    /* 
     * Allocate the port_range_element vector.
     */

    RPC_MEM_ALLOC (range_elements,        
                   rpc_port_range_element_p_t,
                   sizeof (rpc_port_range_element_t) * n_elements,
                   RPC_C_MEM_PORT_RANGE_ELEMENTS,
                   RPC_C_MEM_WAITOK);

    if (range_elements == NULL)
    {
        *status = rpc_s_no_memory;
        return;
    }

    /* 
     * Initialize the rpc_port_restriction_list 
     */

    list_p -> n_tries = 0;
    list_p -> n_elements = n_elements;
    list_p -> range_elements = range_elements;

    /* 
     * Loop and initialize the range element list.
     */

    for (i = 0; i < n_elements; i++)
    {
        
        if ((RPC__IP_ENDPOINT_SSCANF
             ((char *) first_port_name_list[i], "%u", &range_elements[i].low)
             != 1)       ||
            (RPC__IP_ENDPOINT_SSCANF
             ((char *) last_port_name_list[i], "%u", &range_elements[i].high) 
             != 1)       ||
            (range_elements[i].low > range_elements[i].high))
        {
            RPC_MEM_FREE (list_p, RPC_C_MEM_PORT_RESTRICT_LIST);
            RPC_MEM_FREE (range_elements, RPC_C_MEM_PORT_RANGE_ELEMENTS);

            *status = rpc_s_invalid_endpoint_format;

            return;
        }                               /* error from scanf */

        list_p -> n_tries += 
            range_elements[i].high - range_elements[i].low + 1;
    }                                   /* for i */

    /* 
     * Randomly choose a starting range and a port within the range.
     */

    list_p -> current_range_element = RPC_RANDOM_GET (0, n_elements - 1);
    i = list_p -> current_range_element;

    list_p -> current_port_in_range = 
        RPC_RANDOM_GET (range_elements[i].low, range_elements[i].high);

    /* 
     * Everything was successful.  Wire the port_restriction_list into the 
     * protseq descriptor table.
     */

    rpc_g_protseq_id [protseq_id].port_restriction_list = list_p;
    *status = rpc_s_ok;

}                                       /* set_port_restriction */


/*
**++
**
**  ROUTINE NAME:       get_next_restricted_port
**
**  SCOPE:              INTERNAL
**
**  DESCRIPTION:
**      
**  Returns the next restricted port in a sequence.  There is no guarantee
**  that the port is available, that is up to bind() to determine.
**  
**  INPUTS:
**
**      protseq_id
**                      The protocol sequence id to get the port on. 
**      
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      port_name
**                      An IP port name as a text string.
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

INTERNAL void get_next_restricted_port
#ifdef _DCE_PROTO_
(
    rpc_protseq_id_t           protseq_id,
     unsigned_char_p_t          *port_name,
     unsigned32                 *status
)
#else
(protseq_id, port_name, status)
rpc_protseq_id_t           protseq_id;
unsigned_char_p_t          *port_name;
unsigned32                 *status;
#endif
{

    rpc_port_restriction_list_p_t list_p;
    rpc_port_range_element_p_t  range_p;

    CODING_ERROR (status);

    /* 
     * Validate that this protocol sequence has a port restriction.
     */

    list_p = rpc_g_protseq_id [protseq_id].port_restriction_list;

    if (list_p == NULL)
    {
        /* 
         * Return an error to tell the caller that there is no range
         * restriction on this protocol sequence.
         */

        *status = rpc_s_invalid_arg;
        return;
    }

    /* 
     * Alloc a string and return to caller.
     */

    RPC_MEM_ALLOC
        (*port_name,
         unsigned_char_p_t,
         RPC_C_ENDPOINT_IP_MAX,
         RPC_C_MEM_STRING,
         RPC_C_MEM_WAITOK);
    if (*port_name == NULL){
	*status = rpc_s_no_memory;
	return;
    }

    RPC__IP_ENDPOINT_SPRINTF
        ((char *) *port_name, "%u", list_p -> current_port_in_range);

    /* 
     * Increment to the next restricted port number.  Handle wrapping 
     * beyond end of this range.
     */
    
    range_p = (rpc_port_range_element_p_t) list_p -> range_elements + 
        list_p -> current_range_element;

    if (++ (list_p -> current_port_in_range) > range_p -> high)
    {
        /* 
         * Advance to next range and wraparound as needed.
         */
        
        list_p -> current_range_element =
            (++ list_p -> current_range_element) % (list_p -> n_elements);

        range_p = (rpc_port_range_element_p_t) list_p -> range_elements +
            list_p -> current_range_element;

        /* 
         * Set next port in new range to the lowest in that range.
         */

        list_p -> current_port_in_range = range_p -> low;

    }                                   /* wrapped to end of range */
        
    /* 
     * Success
     */

    *status = rpc_s_ok;
}                                       /* get_next_restricted_port */

/*
**++
**
**  ROUTINE NAME:       inq_max_frag_size
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**      
**  INPUTS:
**
**      naf_id          Network Address Family ID serves
**                      as index into EPV for IP routines.
**
**      iftype          Network interface type ID
**
**      protocol        Network protocol ID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:                        
**
**      max_tpdu
**
**      status          A value indicating the status of the routine.
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

INTERNAL void inq_max_frag_size
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
    boolean     flag;
    unsigned32  lstatus;

    /*
     * This should be called from ncadg_ip_udp only.
     */
    if (RPC_PROTSEQ_INQ_NET_IF_ID(rpc_addr->rpc_protseq_id)
          != RPC_C_NETWORK_IF_ID_DGRAM
        || RPC_PROTSEQ_INQ_NET_PROT_ID(rpc_addr->rpc_protseq_id)
            != RPC_C_NETWORK_PROTOCOL_ID_UDP)    
    {
        assert(false);      /* !!! */
    }

    *status = rpc_s_ok;

    flag = rpc__ip_is_local_addr (rpc_addr, &lstatus);
    if (lstatus == rpc_s_ok && flag)
    {
        *max_frag_size = RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE;
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
        switch (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_loc_unfrag_tpdu])
        {
        case 0:
            break;
        case 1:
            *max_frag_size = RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU;
            break;
        case 2:
            *max_frag_size = RPC_C_FDDI_MAX_DATA_SIZE -
                (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE +
                 RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE);
            break;
        case 3:
            *max_frag_size = 4608 -
                (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE +
                 RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE);
            break;
        default:
            if (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_loc_unfrag_tpdu] > 200)
            {
                *max_frag_size = ((unsigned32)
                    (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_loc_unfrag_tpdu])
                     - 200) * 1024;
            }
            else
            {
                *max_frag_size = ((unsigned32)
                    (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_loc_unfrag_tpdu])) * 32;
            }
            break;
        }
#endif
        return;
    }

    flag = rpc__ip_is_local_network (rpc_addr, &lstatus);
    if (lstatus == rpc_s_ok && flag)
    {
        *max_frag_size = RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE;
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
        switch (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_pth_unfrag_tpdu])
        {
        case 0:
            break;
        case 1:
            *max_frag_size = RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU;
            break;
        case 2:
            *max_frag_size = RPC_C_FDDI_MAX_DATA_SIZE -
                (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE +
                 RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE);
            break;
        case 3:
            *max_frag_size = 4608 -
                (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE +
                 RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE);
            break;
        default:
            if (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_pth_unfrag_tpdu] > 200)
            {
                *max_frag_size = ((unsigned32)
                    (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_pth_unfrag_tpdu])
                     - 200) * 1024;
            }
            else
            {
                *max_frag_size = ((unsigned32)
                    (rpc_g_dbg_switches[(int) rpc_es_dbg_ip_max_pth_unfrag_tpdu])) * 32;
            }
            break;
        }
#endif
        return;
    }

    *max_frag_size = RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU;
    return;

}
