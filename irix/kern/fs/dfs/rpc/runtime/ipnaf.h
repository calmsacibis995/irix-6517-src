/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ipnaf.h,v $
 * Revision 65.2  1997/10/20 19:17:01  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.913.2  1996/02/18  22:56:24  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:30  marty]
 *
 * Revision 1.1.913.1  1995/12/08  00:20:34  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:28  root]
 * 
 * Revision 1.1.911.2  1994/05/27  15:36:50  tatsu_s
 * 	Turned off MBF in KRPC.
 * 	[1994/05/03  16:13:36  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	Added RPC_C_FDDI_MAX_DATA_SIZE, RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE
 * 	and RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE.
 * 	Added rpc__ip_init_local_addr_vec(), rpc__ip_is_local_network()
 * 	and rpc__ip_is_local_addr().
 * 	[1994/04/29  19:24:06  tatsu_s]
 * 
 * Revision 1.1.911.1  1994/01/21  22:37:42  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:48  cbrooks]
 * 
 * Revision 1.1.8.1  1993/09/28  21:58:18  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:41  weisman]
 * 
 * Revision 1.1.5.5  1993/01/03  23:25:38  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:07:33  bbelch]
 * 
 * Revision 1.1.5.4  1992/12/23  20:51:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:18  zeliff]
 * 
 * Revision 1.1.5.3  1992/10/01  19:33:35  markar
 * 	One more tweak of the TPDU/TSDU constants, plus more commentary.
 * 	[1992/09/17  13:57:11  markar]
 * 
 * Revision 1.1.5.2  1992/09/14  19:28:59  markar
 * 	Fix TPDU/TSDU constants to not induce IP fragmentation on Ethernet (oops).
 * 	[1992/09/10  19:09:32  markar]
 * 
 * Revision 1.1.2.2  1992/06/26  06:01:28  jim
 * 	Removed the _AIX check before include domain.h and protosw.h.
 * 	[1992/06/05  20:16:54  jim]
 * 
 * 	included sys/domain.h and sys/protosw.h for the AIX
 * 	kernel extension on AIX 3.2.
 * 	[1992/05/29  18:25:21  jim]
 * 
 * Revision 1.1  1992/01/19  03:10:49  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _IPNAF_H
#define _IPNAF_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      ipnaf.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions and Data Type declarations
**  used by the Internet Network Address Family Extension
**  service.
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/***********************************************************************
 *
 *  Include the Internet specific socket address 
 */

#if defined(_KERNEL)
#include <sys/domain.h>
#include <sys/protosw.h>
#endif

#if defined(unix) || defined(__unix) || defined(__unix__)
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifndef _KERNEL 
#include <arpa/inet.h>
#endif
#else
#include <in.h>
#include <tcp.h>
#include <inet.h>
#endif

/***********************************************************************
 *
 * provides a masked unsigned8 required in conversion of IP network
 * address to ascii dot notation string.
 */

#define UC(b)               (((int) b) & 0xff)

/***********************************************************************
 *
 *  The representation of an RPC Address that holds an IP address.
 */

typedef struct rpc_addr_ip_t
{
    rpc_protseq_id_t        rpc_protseq_id;
    unsigned32              len;
    struct sockaddr_in      sa;
} rpc_ip_addr_t, *rpc_ip_addr_p_t;

/***********************************************************************
 *
 * Define some TPDU/TSDU constants, if they haven't already been defined
 * (typically in a system header file).
 */
                                    
/*
 * The max # of data bytes that can go into a UDP packet body such that
 * the resulting IP packet can fit through any of the local network
 * interfaces without inducing IP fragmentation. 
 *
 * NOTE WELL:  This value is derived from
 *
 *      (1) The size of the data section of data link packets.  For the
 *          time being, the data link is assumed to be ethernet.
 *
 *      (2) The size of the LLC frame.  RFC 1042, which specifies IP
 *          over 802 networks, calls for the use of the SNAP protocol.
 *          SNAP takes up 8 bytes of the ethernet frame's data section.
 *
 *      (3) The size of the UDP and IP headers, from RFCs 768 and 791.
 *
 *      (4) The length of the IP options part of the header.  Since we
 *          do not currently use any of the IP options, this value is
 *          0.   *** This constant must be modified if we ever make use
 *          of IP options in the future. ***
 *
 * !!! THIS VALUE SHOULD BE COMPUTED AT RUNTIME and with a little work
 * could be.
 */

#define RPC_C_ETHER_MAX_DATA_SIZE 1500
#define RPC_C_IP_LLC_SIZE            8 /* LLC frame for SNAP protocol */
#define RPC_C_IP_HDR_SIZE           20 /* Base IP header */
#define RPC_C_IP_OPTS_SIZE           0 /* IP options length */
#define RPC_C_UDP_HDR_SIZE           8 /* UDP header */
#ifndef RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU
#if defined(SGIMIPS) && defined(_KERNEL)
#define SGI_BIGGEST_UDP_SIZE            ((65535 - 28) & ~7)
#define RPC_C_DG_SGI_MAX_PKT_SIZE           SGI_BIGGEST_UDP_SIZE
#define RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU RPC_C_DG_SGI_MAX_PKT_SIZE
#else
#define RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU ( \
        RPC_C_ETHER_MAX_DATA_SIZE - \
        (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE + \
         RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif /* SGIMIPS && _KERNEL */
#endif /* RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU */

#define RPC_C_FDDI_MAX_DATA_SIZE 4352

/*
 * The max # of data bytes that can go into a UDP packet body such that
 * the resulting IP packet can be sent to some host without being
 * fragmented along the path.
 *
 * For now, we just set this value to be the same as above.  In general,
 * this value would be <= the above value.
 *
 * !!! THIS VALUE SHOULD BE COMPUTED AT RUNTIME as a function of the
 * target host address but can't be given the tools we have today.
 * 
 */ 
#ifndef RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU     
#define RPC_C_IP_UDP_MAX_PTH_UNFRG_TPDU RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU
#endif

/*
 * The max # of data bytes that can go into a UDP packet body and be
 * accepted through the transport service interface (i.e., sockets).
 * While logically this value is 2**16-1 (see RFC 768), in practice many
 * systems don't support such a large value.
 * This value is really only useful when it is necessary to *limit*
 * the fragment size because of a limitation in the service
 * interface.
 */
#ifndef RPC_C_IP_UDP_MAX_TSDU
#define RPC_C_IP_UDP_MAX_TSDU ( \
        64 * 1024 - \
        (RPC_C_IP_HDR_SIZE + RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif

/*
 * Max Path DG Fragment Size:
 *
 *   The size in bytes of the largest DG fragment that can be sent to
 *   a particular address. This is determined when the call handle to
 *   a particular address is created and may change in the life of the
 *   call handle.
 *
 * The constant defined here is based on experimentation.
 *
 * Caution: This must be less than RPC_C_DG_MAX_FRAG_SIZE::dg.h!
 */

#ifndef RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE
#ifdef _KERNEL
#define RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE RPC_C_IP_UDP_MAX_LOC_UNFRG_TPDU
#else
#define RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE ( \
        RPC_C_FDDI_MAX_DATA_SIZE - \
        (RPC_C_IP_LLC_SIZE + RPC_C_IP_HDR_SIZE + \
         RPC_C_IP_OPTS_SIZE + RPC_C_UDP_HDR_SIZE) \
)
#endif /* _KERNEL */
#endif

/*
 * Max Local DG Fragment Size:
 *
 *   The size in bytes of the largest DG fragment that can be sent to
 *   a "local" address. The data won't be transmitted over the "wire"
 *   by the transport service, i.e., the loopback is done on the local
 *   host. This is determined when the socket is created and won't
 *   change in the life of the socket.
 *
 * The constant defined here is based on experimentation.
 *
 * Caution: This must be less than RPC_C_DG_MAX_FRAG_SIZE::dg.h!
 */

#ifndef RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE
#ifdef _KERNEL
#define RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE RPC_C_IP_UDP_MAX_PATH_FRAG_SIZE
#else
#define RPC_C_IP_UDP_MAX_LOCAL_FRAG_SIZE (8 * 1024)
#endif /* _KERNEL */
#endif


/***********************************************************************
 *
 * The IP-specific representation of rpc_port_restriction_list_t.range_list 
 * (see com.h).  The low and high are in native machine representation, not 
 * network rep.
 */

typedef struct struct_rpc_port_range_element
{
    unsigned32                      low;
    unsigned32                      high;
} rpc_port_range_element_t, *rpc_port_range_element_p_t;

/***********************************************************************
 *
 *  Routine Prototypes for the Internet Extension service routines.
 */
    
#ifdef __cplusplus
extern "C" {
#endif


PRIVATE void rpc__ip_init _DCE_PROTOTYPE_ ((  
        rpc_naf_epv_p_t             * /*naf_epv*/,
        unsigned32                  * /*status*/
    ));

PRIVATE void rpc__ip_desc_inq_addr _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t             /*protseq_id*/,
        rpc_socket_t                 /*desc*/,
        rpc_addr_vector_p_t         * /*rpc_addr_vec*/,
        unsigned32                  * /*st*/
    ));

PRIVATE void rpc__ip_get_broadcast _DCE_PROTOTYPE_ ((
        rpc_naf_id_t                 /*naf_id*/,
        rpc_protseq_id_t             /*rpc_protseq_id*/,
        rpc_addr_vector_p_t         * /*rpc_addrs*/,
        unsigned32                  * /*status*/
    ));

PRIVATE void rpc__ip_init_local_addr_vec _DCE_PROTOTYPE_ ((
        unsigned32                  * /*status*/
    ));

PRIVATE boolean32 rpc__ip_is_local_network _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

PRIVATE boolean32 rpc__ip_is_local_addr _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                 /*rpc_addr*/,
        unsigned32                  * /*status*/
    ));

#ifdef __cplusplus
}
#endif

              
#endif /* _IPNAF_H */
