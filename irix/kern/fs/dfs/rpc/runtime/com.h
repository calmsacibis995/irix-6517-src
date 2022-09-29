/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* 
 * (c) Copyright 1991, 1992 
 *     Siemens-Nixdorf Information Systems, Burlington, MA, USA
 *     All Rights Reserved
 */
/*
 * HISTORY
 * $Log: com.h,v $
 * Revision 65.2  1997/10/20 19:16:45  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.20.1  1996/05/10  13:09:24  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:12 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:41 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/5]
 *
 * 	Added rpc__fork_is_in_progress().
 * 	[1995/01/25  20:31 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	merge DMS changes to call_rep
 * 	[1995/01/18  20:44 UTC  gaz  /main/HPDCE02/4]
 *
 * 	fix include ordering problem with dmsrpc.h
 * 	[1995/01/18  17:32 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
 *
 * 	Add calltag anchor to call_rep
 * 	[1995/01/18  15:29 UTC  gaz  /main/HPDCE02/gaz_dce_instr/1]
 *
 * 	Merging Local RPC mods
 * 	[1995/01/16  19:14 UTC  markar  /main/HPDCE02/3]
 *
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/01/05  15:58 UTC  markar  /main/HPDCE02/markar_local_rpc/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:17 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/3]
 *
 * 	RPC_PREFERRED_PROTSEQ: Initial version.
 * 	[1994/12/05  18:53 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:25 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/1]
 *
 * 	Merged mothra up to dce 1.1.
 * 	[1994/08/03  16:35 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	back out errantly-merged-in MOTHRA mods
 * 	[1994/07/19  21:53 UTC  mk  /main/HPDCE01/5]
 *
 * 	merge in fixes from mk_mothra_repair branch
 * 	[1994/07/19  21:03 UTC  mk  /main/HPDCE01/4]
 *
 * 	conditionalize references to uuid__create_standalone out of krpc build
 * 	[1994/07/19  20:22 UTC  mk  /main/HPDCE01/MOTHRA/mk_mothra_repair/2]
 *
 * 	merge mk_sd_uuids branch to MOTHRA
 * 	[1994/06/17  15:54 UTC  mk  /main/HPDCE01/MOTHRA/mk_mothra_repair/1]
 *
 * 	standalone uuid generation support for sd
 * 	[1994/05/26  22:30 UTC  mk  /main/HPDCE01/mk_sd_uuids/1]
 *
 * 	Submitted DG multi-buffer fragments.
 * 	[1994/04/19  17:54 UTC  tatsu_s  /main/HPDCE01/MOTHRA/1]
 *
 * 	Initial checkin of DG multi-buffer fragments.
 * 	[1994/03/31  22:12 UTC  tatsu_s  /main/HPDCE01/tatsu_s_rfc20_b0/1]
 *
 * 	merge in IC1 loopback support
 * 	[1994/03/08  23:38 UTC  mk  /main/HPDCE01/2]
 *
 * 	add loopback support for sd
 * 	[1994/03/08  00:53 UTC  mk  /main/HPDCE01/mk_LOOPBACK_IC1/1]
 *
 * 	Fix OT9577.
 * 	Lifted the limit of calls_in_progress to "signed32".
 * 	[1993/12/07  17:45:46  tatsu_s]
 * 	 *
 *
 * 	Loading drop DCE1_0_3b931005
 * 	[1993/10/05  17:59:10  tatsu_s  1.1.6.5]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  18:59 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:30 UTC  jrr  /main/HPDCE02/gaz_dce_instr/jrr_1.2_mothra/1]
 *
 * 	Ensure that DMS is NOT built in the Kernel RPC
 * 	[1995/08/17  04:18 UTC  gaz  /main/HPDCE02/gaz_dce_instr/6]
 *
 * 	Change calltags to a union
 * 	[1995/07/05  01:46 UTC  gaz  /main/HPDCE02/gaz_dce_instr/5]
 *
 * 	Merge changes out to branch
 * 	[1995/02/22  19:18 UTC  gaz  /main/HPDCE02/gaz_dce_instr/4]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:16 UTC  tatsu_s  /main/HPDCE02/5]
 *
 * 	Added rpc__fork_is_in_progress().
 * 	[1995/01/25  20:31 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Merge up changes
 * 	[1995/01/23  16:03 UTC  gaz  /main/HPDCE02/gaz_dce_instr/3]
 *
 * 	merge DMS changes to call_rep
 * 	[1995/01/18  20:44 UTC  gaz  /main/HPDCE02/4]
 *
 * 	fix include ordering problem with dmsrpc.h
 * 	[1995/01/18  17:32 UTC  gaz  /main/HPDCE02/gaz_dce_instr/2]
 *
 * 	Add calltag anchor to call_rep
 * 	[1995/01/18  15:29 UTC  gaz  /main/HPDCE02/gaz_dce_instr/1]
 *
 * 	Merging Local RPC mods
 * 	[1995/01/16  19:14 UTC  markar  /main/HPDCE02/3]
 *
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/01/05  15:58 UTC  markar  /main/HPDCE02/markar_local_rpc/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:17 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/3]
 *
 * 	RPC_PREFERRED_PROTSEQ: Initial version.
 * 	[1994/12/05  18:53 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:25 UTC  tatsu_s  /main/HPDCE02/tatsu_s.st_dg.b0/1]
 *
 * 	Merged mothra up to dce 1.1.
 * 	[1994/08/03  16:3
 *
 * Revision 1.1.15.7  1994/07/15  16:56:08  tatsu_s
 * 	Fix OT9577: Changed the type of calls_in_progress to "signed32".
 * 	[1994/07/09  02:34:53  tatsu_s]
 * 
 * Revision 1.1.15.6  1994/06/22  20:41:35  tom
 * 	Bug 10979 - add structure tag for use by handle_t definition.
 * 	[1994/06/22  20:41:14  tom]
 * 
 * Revision 1.1.15.5  1994/05/27  15:35:51  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added rpc__naf_inq_max_frag_size().
 * 	[1994/04/29  18:56:15  tatsu_s]
 * 
 * Revision 1.1.15.4  1994/04/12  16:51:58  mori_m
 * 	CR 9701:  Second submission for RPC runtime I18N support.
 * 		  Added code set I14Y info to rpc_binding_rep_t.
 * 	[1994/04/12  16:42:11  mori_m]
 * 
 * Revision 1.1.15.3  1994/03/17  23:06:39  tom
 * 	Big PAC -
 * 	  Add definition of rpc_protocol_version_t and add a pointer
 * 	  to one in the binding_rep.
 * 	  Add definition of rpc__binding_set_prot_version.
 * 	  Add definition of rpc__binding_prot_version_alloc.
 * 	  Add definition of rpc__binding_prot_version_free.
 * 	[1994/03/17  23:04:19  tom]
 * 
 * Revision 1.1.15.2  1994/01/28  23:09:20  burati
 * 	Dlg changes - Add rpc_authz_cred_handle_t to rpc_auth_info_t (dlg_bl1)
 * 	[1994/01/24  21:32:42  burati]
 * 
 * Revision 1.1.15.1  1994/01/21  22:34:57  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:54:51  cbrooks]
 * 
 * Revision 1.1.11.2  1993/09/28  21:58:05  weisman
 * 	Merge hiccup
 * 	[1993/09/28  18:28:09  weisman]
 * 
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:30  weisman]
 * 
 * Revision 1.1.11.1  1993/09/15  15:39:44  damon
 * 	Synched with ODE 2.1 based build
 * 	[1993/09/15  15:31:58  damon]
 * 
 * Revision 1.1.11.2  1993/09/14  16:48:04  tatsu_s
 * 	Bug 8103 - Moved the declaration of rpc__nlsn_fork_handler() to
 * 	comnetp.h.
 * 	[1993/09/13  17:03:12  tatsu_s]
 * 
 * Revision 1.1.8.12  1993/02/05  16:11:10  raizen
 * 	 26-jan-93 raizen    back out changes from Revision 1.1.8.7
 * 	[1993/02/04  23:19:48  raizen]
 * 
 * Revision 1.1.8.11  1993/01/03  22:59:38  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:01:02  bbelch]
 * 
 * Revision 1.1.8.10  1992/12/23  20:18:32  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:32:41  zeliff]
 * 
 * Revision 1.1.8.9  1992/12/21  21:28:40  sommerfeld
 * 	*** empty log message ***
 * 
 * Revision 1.1.8.8  1992/12/21  20:11:26  bolinger
 * 	Reinstate change made in revision 1.1.8.5.  (Consequences for
 * 	krpc have now been fixed.)
 * 	[1992/12/21  20:07:54  bolinger]
 * 
 * Revision 1.1.8.7  1992/12/21  19:19:59  wei_hu
 * 	       9-dec-92 raizen    [OT4854]  Add rpc_g_authenticated.
 * 	                          Modify RPC_GET_THREAD_CONTEXT to init
 * 	                          ns_authn_state from rpc_g_authenticated.
 * 	[1992/12/18  20:26:56  wei_hu]
 * 
 * Revision 1.1.8.6  1992/12/17  22:01:31  bolinger
 * 	Back out Bill Sommerfeld revision of 12/11/92, which seems
 * 	to cause krpc hangs on both reference platforms.
 * 	[1992/12/17  21:58:12  bolinger]
 * 
 * Revision 1.1.8.3  1992/10/13  20:54:19  weisman
 * 	      21-aug-92 wh        Add rpc_g_default_pthread_attr.
 * 	[1992/10/13  20:47:00  weisman]
 * 
 * Revision 1.1.8.2  1992/09/29  20:41:32  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:45:26  weisman]
 * 
 * Revision 1.1.4.2  1992/05/01  17:01:00  rsalz
 * 	 05-feb-92 sommerfeld fix type of bit field in rpc_key_info_t
 * 	 27-jan-92 sommerfeld key reorg: add rpc_key_info_t.
 * 	[1992/05/01  16:48:54  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:12:40  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _COM_H
#define _COM_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      com.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Private interface to the Common Communications Service for use
**  by RPC Protocol Services and Network Address Family Extension Services.
**
**
*/

#include <cominit.h>

/*
 * the value of an invalid interface hint
 */

#define RPC_C_INVALID_IHINT     0xFFFF


/***********************************************************************/
/*
 * U U I D _ G _ N I L _ U U I D
 */
EXTERNAL uuid_t    uuid_g_nil_uuid;


/***********************************************************************/
/*
 * R P C _ G _ I N I T I A L I Z E D 
 *
 * The value that indicates whether or not the RPC runtime has previously
 * been initialized. Its definition is in comp.c.
 */
EXTERNAL boolean        rpc_g_initialized;


/***********************************************************************/
/*
 * R P C _ G _ T H R E A D _ C O N T E X T _ K E Y
 *
 * The key visible to all threads that contains a pointer to the
 * per-thread context block.
 */
EXTERNAL pthread_key_t  rpc_g_thread_context_key;


/***********************************************************************/
/*
 * R P C _ G _ R U N T I M E _ P T H R E A D _ A T T R
 *
 * A pthread attribute for creation of threads internal to the runtime
 * (other than server threads).
 * Initialized by rpc_init().
 */
EXTERNAL pthread_attr_t     rpc_g_default_pthread_attr;


/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ M U T E X
 *
 * The global mutex used for the entire Communications Service. Note
 * that this may be temporary since a per-data structure mutex may
 * be employed. Its definition is in comp.c.
 */
EXTERNAL rpc_mutex_t    rpc_g_global_mutex;


/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ B I N D I N G _ C O N D
 *
 * The global binding handle condition variable used for call
 * serialization.  This condition variable is protected by the global
 * mutex.  Note that this may be temporary since a per-binding handle
 * cond var / mutex may be employed.  Its definition is in comp.c.
 */
EXTERNAL rpc_cond_t    rpc_g_global_binding_cond;


/***********************************************************************/
/*
 * R P C _ G _ F O R K _ C O U N T
 *
 * The global fork count used to detect when a process using
 * RPC has forked.  Its definition is in comp.c.
 */                                
EXTERNAL unsigned32    rpc_g_fork_count;

/***********************************************************************/
/*
 * R P C _ G _ S I N G L E _ T H R E A D
 *
 * The value that indicates whether or not the process is
 * single threaded or not. Its definition is in comp.c.
 */

EXTERNAL boolean	rpc_g_is_single_threaded;

/***********************************************************************/
/*
 * R P C _ V E R I F Y _ I N I T
 *
 * A macro which calls rpc__init if it hasn't been called yet.
 */
#define RPC_VERIFY_INIT()                                               \
{                                                                       \
    if (rpc_g_initialized == false)                                     \
    {                                                                   \
        rpc__init();                                                    \
    }                                                                   \
}


/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ I D _ T
 *
 * The RPC Protocol Sequence IDs.
 */
#define RPC_C_INVALID_PROTSEQ_ID        -1
#define RPC_C_PROTSEQ_ID_NCACN_IP_TCP   0
#define RPC_C_PROTSEQ_ID_NCACN_DNET_NSP 1
#define RPC_C_PROTSEQ_ID_NCACN_OSI_DNA  2
#define RPC_C_PROTSEQ_ID_NCADG_IP_UDP   3
#define RPC_C_PROTSEQ_ID_NCADG_DDS      4

#ifdef TEST_PROTOCOL
#define RPC_C_PROTSEQ_ID_NCATP_IP_TCP   5
#define RPC_C_PROTSEQ_ID_MAX            6
#else
#define RPC_C_PROTSEQ_ID_MAX            5
#endif /* TEST_PROTOCOL */

typedef unsigned32       rpc_protseq_id_t, *rpc_protseq_id_p_t;


/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ T
 *
 * The RPC Protocol Sequence Strings.
 */

/*
 * An RPC protocol sequence including '\0'.
 */

#define RPC_C_PROTSEQ_MAX               32

typedef unsigned_char_t rpc_protseq_t[RPC_C_PROTSEQ_MAX];

#define RPC_PROTSEQ_NCACN_IP_TCP        "ncacn_ip_tcp"
#define RPC_PROTSEQ_NCACN_DNET_NSP      "ncacn_dnet_nsp"
#define RPC_PROTSEQ_NCACN_OSI_DNA       "ncacn_osi_dna"
#define RPC_PROTSEQ_NCADG_IP_UDP        "ncadg_ip_udp"
#define RPC_PROTSEQ_NCADG_DDS           "ncadg_dds"

#ifdef PROT_NCATP
#define RPC_PROTSEQ_NCATP_IP_TCP        "ncatp_ip_tcp"
#endif


/***********************************************************************/
/*
 * R P C _ P R O T O C O L _ I D _ T
 *
 * The RPC Protocol IDs.
 */

#define RPC_C_PROTOCOL_ID_NCACN         0
#define RPC_C_PROTOCOL_ID_NCADG         1

#ifdef PROT_NCATP
#define RPC_C_PROTOCOL_ID_NCATP         2
#define RPC_C_PROTOCOL_ID_MAX           3
#else
#define RPC_C_PROTOCOL_ID_MAX           2
#endif

typedef unsigned32       rpc_protocol_id_t, *rpc_protocol_id_p_t;


/***********************************************************************/
/*
 * R P C _ N A F _ I D _ T
 *
 * The Network Address Family IDs.
 * 
 * NOTE WELL that this data type is defined as "unsigned32", not
 * "unsigned16".  It's this way because of annoying problems in passing
 * sub-"int" sized parameters using our model of how to work in both
 * ANSI and pre-ANSI C environments.  An unfortunate upshot of this fact
 * is that if you're trying to build up a "struct sockaddr" equivalent
 * data type (e.g., "rpc_addr_p_t" below), you CAN'T use "rpc_naf_id_t"
 * -- you must use "unsigned16".  (Arguably, you should just say "struct
 * sockaddr", but that's another matter.)
 */

#define RPC_C_NAF_ID_IP      2
#define RPC_C_NAF_ID_DNET    12
#define RPC_C_NAF_ID_DDS     13         /* ###Check this one ###*/
#define RPC_C_NAF_ID_OSI     19
#define RPC_C_NAF_ID_MAX     20

typedef unsigned32       rpc_naf_id_t, *rpc_naf_id_p_t;


/***********************************************************************/
/*
 * R P C _ N E T W O R K _ P R O T O C O L _ I D _ T
 *
 * The Network Protocol IDs.
 */

#define RPC_C_NETWORK_PROTOCOL_ID_TCP   6
#define RPC_C_NETWORK_PROTOCOL_ID_UDP   17
#define RPC_C_NETWORK_PROTOCOL_ID_NSP   1
#define RPC_C_NETWORK_PROTOCOL_ID_DDS   0
#define RPC_C_NETWORK_PROTOCOL_ID_DNASESSION 9
#define RPC_C_NETWORK_PROTOCOL_ID_UNS   0

typedef unsigned32 rpc_network_protocol_id_t, *rpc_network_protocol_id_p_t;


/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ I N Q _ N E T _ I F _ I D
 */

#define RPC_PROTSEQ_INQ_NET_IF_ID(id)       rpc_g_protseq_id[id].network_if_id

/*
 * R P C _ N E T W O R K _ I F _ I D _ T
 *
 * The Network Interface Type IDs.
 */

#if defined(SNI_SVR4) || defined(SGIMIPS)
/* see <sys/socket.h> */
#define RPC_C_NETWORK_IF_ID_STREAM      SOCK_STREAM
#define RPC_C_NETWORK_IF_ID_DGRAM       SOCK_DGRAM
#else
#define RPC_C_NETWORK_IF_ID_STREAM      1
#define RPC_C_NETWORK_IF_ID_DGRAM       2
#endif /* SNI_SVR4 */

#define RPC_C_NETWORK_IF_ID_SEQPACKET   5

typedef unsigned32       rpc_network_if_id_t, *rpc_network_if_id_p_t;

/***********************************************************************/
/*
 * R P C _ C _ B H _ E X T E N D E D
 *
 * The values of the flag field within rpc_binding_rep_t (extended_bind_flag).
 * If rpc_binding_rep_t is extended to include a new information in the
 * future, add the definition here, and modify the affected routines
 * accordingly.
 */
#define RPC_C_BH_EXTENDED_NONE          0x0000
#define RPC_C_BH_EXTENDED_CODESETS      0x0001
#define RPC_C_BH_IN_STUB_EVALUATION     0x0002

/***********************************************************************/
/* 
 * R P C _ P O R T _ R E S T R I C T I O N _ L I S T _ T 
 *
 * Each protocol sequence (rpc_protseq_id_elt_t) is associated with an
 * optional list of ranges of network "ports" to restrict binding dynamic
 * endpoints to.  This facility is intended for use by sites which employ
 * a network "firewall," but still wish to deploy WAN-based RPC
 * applications.  This allows the network administrator to open certain
 * ranges of ports in their firewalls to DCE servers.
 *
 * The rpc_port_restriction_list_t contains a void pointer to an
 * AF-specific array of low/high pairs.  Logic in the AF code will define
 * and interpret this array as locally appropriate.
 * 
 * Also contained are two variables which provide sequencing of returned
 * endpoints.  current_range_element points to the current active range in
 * range_elements, and current_port_in_range is used in an AF-specific
 * manner to provide the next port in a range.
 *
 */

typedef struct
{
    unsigned32              n_tries;    /* specific to the AF */
    unsigned32              n_elements; /* in range_elements */
    void                    *range_elements; 
                                        /* pointer to an AF-specific array */
    unsigned32              current_range_element;
    unsigned32              current_port_in_range;
} rpc_port_restriction_list_t, *rpc_port_restriction_list_p_t;

/***********************************************************************/
/*
 * R P C _ P O R T _ R E S T R I C T I O N _ I N Q _ N _ T R I E S 
 */

#define RPC_PORT_RESTRICTION_INQ_N_TRIES(protseq_id) \
    (rpc_g_protseq_id[(protseq_id)].port_restriction_list->n_tries)


/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ I D _ E L T _ T
 *
 * The RPC Protocol Sequence ID table element structure.  An element
 * describes a single RPC Protocol Sequence.
 *
 * The fields are:
 *
 *      supported       A boolean flag initialized to zero and filled
 *                      in by rpc__init if it determines that this Protocol
 *                      Sequence is actually supported by the system.
 *
 *      rpc_protseq_id  A constant identifier for the Protocol Sequence.
 *      
 *      rpc_protocol_id A constant identifier for the RPC Protocol used
 *                      in this Protocol Sequence.
 *      
 *      naf_id          A constant identifier for the Network Address
 *                      Family used in this Protocol Sequence.
 *      
 *      net_protocol_id A constant identifier for the network protocol
 *                      used in this Protocol Sequence.
 *      
 *      net_if_id       A constant identifier for the network interface
 *                      type used in this Protocol Sequence.
 *      
 *      rpc_protseq     A string constant defining this Protocol Sequence.
 *
 * The typedef for an RPC Protocol Sequence string is contained in rpc.idl.
 */
typedef struct
{
    boolean                     supported;
    rpc_protseq_id_t            rpc_protseq_id;
    rpc_protocol_id_t           rpc_protocol_id;
    rpc_naf_id_t                naf_id;
    rpc_network_protocol_id_t   network_protocol_id;
    rpc_network_if_id_t         network_if_id;
    rpc_protseq_t               rpc_protseq;
    rpc_port_restriction_list_p_t        port_restriction_list;
} rpc_protseq_id_elt_t, *rpc_protseq_id_elt_p_t;

/***********************************************************************/
/*
 * R P C _ G _ P R O T S E Q _ I D
 *
 * The RPC Protocol Sequence ID table.  This table is indexed by an RPC
 * Protocol Sequence ID.
 *
 * An RPC Protocol Sequence represents a specific RPC Protocol/Network
 * Address Family combination which is by definition a valid combination
 * of protocols. An RPC Protocol Sequence also represents a specific
 * NAF interface type, since there may be multiple within a NAF. Each
 * RPC Protocol Sequence has an entry in this table.
 *
 * Note that the ".rpc_protseq_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 */
EXTERNAL rpc_protseq_id_elt_t   rpc_g_protseq_id[];

/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ I N Q _ N E T _ P R O T _ I D
 */
#define RPC_PROTSEQ_INQ_NET_PROT_ID(id) \
     rpc_g_protseq_id[id].network_protocol_id


/***********************************************************************/
/*
 * R P C _ P R O T S E Q _ T E S T _ P O R T _ R E S T R I C T I O N
 */
#define RPC_PROTSEQ_TEST_PORT_RESTRICTION(id) \
    (rpc_g_protseq_id[id].port_restriction_list != NULL)


/***********************************************************************/
/*
 * R P C _ E N D P O I N T _ T
 *
 * The RPC Address endpoint structure. 
 */
typedef unsigned_char_t *rpc_endpoint_t;
typedef rpc_endpoint_t  *rpc_endpoint_p_t;


/***********************************************************************/
/*
 * R P C _ N E T A D D R _ T
 *
 * The RPC Address network address (host name) structure. 
 */
typedef unsigned_char_t *rpc_netaddr_t;


/***********************************************************************/
/*
 * R P C _ N E T W O R K _ O P T I O N S _ T
 *
 * The RPC Address network options structure. 
 */
typedef unsigned_char_t *rpc_network_options_t;


/***********************************************************************/
/*
 * R P C _ A D D R _ P _ T
 *
 * The RPC Address data structure.  Note that we don't define a non-pointer
 * variant of this data type since it's an open structure whose size
 * can never be known at compile time.
 */

typedef struct
{
    rpc_protseq_id_t        rpc_protseq_id;
    unsigned32              len;
    sockaddr_t              sa;
} *rpc_addr_p_t;  

/***********************************************************************/
/*
 * R P C _ P R O T O C O L _ V E R S I O N _ T
 *
 * Holds the version number of a binding, stored in the binding handle.
 */
typedef struct
{
    unsigned32              major_version;
    unsigned32              minor_version;
} rpc_protocol_version_t, *rpc_protocol_version_p_t;

/***********************************************************************/
/*
 *  R P C _ I F _ V E R S _ M A J O R 
 *  R P C _ I F _ V E R S _ M I N O R 
 *
 * Macros to turn a 32 bit version number into its major and minor version
 * number components.
 */
#define RPC_IF_VERS_MAJOR(_if_vers) ((_if_vers) & 0xffff)
#define RPC_IF_VERS_MINOR(_if_vers) ((_if_vers) >> 16)

/*
 *  RPC_IF_IS_COMPATIBLE
 *
 * Macro used to compare an interface uuid and version in an if registry
 * entry with a specified if UUID and version.  Note: this code will
 * only match on version numbers if the major versions match exactly
 * and if the minor version in the registry is greater than or equal
 * to that in the if version passed by the caller.
 */
#define RPC_IF_IS_COMPATIBLE(_if_entry, _if_uuid, _if_vers, _status) \
( \
    UUID_EQ ((_if_entry)->if_spec->id, *(_if_uuid), (_status)) \
    && RPC_IF_VERS_MAJOR((_if_entry)->if_spec->vers) == RPC_IF_VERS_MAJOR(_if_vers) \
    && RPC_IF_VERS_MINOR((_if_entry)->if_spec->vers) >= RPC_IF_VERS_MINOR(_if_vers) \
)



  


/*
 * R P C _ T O W E R _ F L O O R _ T
 *
 * Tower floor representation
 */
typedef struct
{
    unsigned16              free_twr_octet_flag;
    unsigned16              prot_id_count;
    unsigned16              address_count;
    byte_t                  *octet_string;
} rpc_tower_floor_t, *rpc_tower_floor_p_t;

/*
 * R P C _ T O W E R _ R E F _ T
 *
 * Runtime tower reference 
 *
 * The runtime uses a reference structure to provide access
 * to the individual floors of a tower.
 */
typedef struct
{
    unsigned16              count;
    rpc_tower_floor_p_t     floor[1];
} rpc_tower_ref_t, *rpc_tower_ref_p_t;

/*
 * R P C _ T O W E R _ R E F _ V E C T O R _ T
 *
 * Runtime tower reference vector
 *
 * The runtime uses a vector of tower reference structures while
 * converting a binding to towers (each binding maps to multiple towers,
 * one for each transfer syntax). In this case only RPC floor 2 differs
 * for each tower. 
 *
 * lower_flrs is obtained from CDS and is saved here as an optimization 
 * allowing the array of tower references (tower) to point into the
 * lower_flrs data.
 */
typedef struct
{
    twr_p_t                 lower_flrs;
    unsigned32              count;
    rpc_tower_ref_p_t       tower[1];
} rpc_tower_ref_vector_t, *rpc_tower_ref_vector_p_t;

/*
 * Protocol Tower constants
 */

/*
 * Number of bytes in the tower floor count field 
 */

#define  RPC_C_TOWER_FLR_COUNT_SIZE   2

/*
 * Number of bytes in the lhs count field of a floor.
 */

#define   RPC_C_TOWER_FLR_LHS_COUNT_SIZE  2

/*
 * Number of bytes in the rhs count field of a floor.
 */

#define   RPC_C_TOWER_FLR_RHS_COUNT_SIZE  2

/*
 * Number of bytes for storing a major or minor version.
 */

#define  RPC_C_TOWER_VERSION_SIZE  2

/*
 * Number of bytes for storing a tower floor protocol id or protocol id prefix.
 */

#define  RPC_C_TOWER_PROT_ID_SIZE  1

/*
 * Number of bytes for storing a uuid.
 */
#define  RPC_C_TOWER_UUID_SIZE  16

/* 
 * Number of upper (RPC-specific floors. Each tower *must* have exactly
 * this number of upper floors.
 */
#define  RPC_C_NUM_RPC_FLOORS   3

/*
 * Maximum number of lower (network-specific) floors used by RPC to figure
 * out the rpc_protseq_id_t of the tower.
 * Each tower *may* have more than this number of lower floors. However,
 * the additional floors are not used directly by RPC.
 */

#define RPC_C_MAX_NUM_NETWORK_FLOORS  3

/*
 * Minimum number of lower (network_specific floors used by RPC to figure
 * out the rpc_protseq_id_t of the tower. (The value is based on ip).
 */

#define RPC_C_MIN_NUM_NETWORK_FLOORS  2

/*
 * Minimum number of floors in a full rpc protocol tower.
 */
#define RPC_C_FULL_TOWER_MIN_FLR_COUNT \
                        (RPC_C_NUM_RPC_FLOORS + RPC_C_MIN_NUM_NETWORK_FLOORS)

/*
 * Minimum number of floors is a minimal rpc protocol tower.
 * (The +1 is for the rpc prot id floor).
 */

#define RPC_C_MIN_TOWER_MIN_FLR_COUNT (RPC_C_MIN_NUM_NETWORK_FLOORS + 1)

/*
 * The following macros help in processing the components
 * of a tower floor.
 */

#define RPC_PROT_ID_COUNT(floor)    \
    ((floor)->octet_string)

#define RPC_PROT_ID_START(floor)    \
    (RPC_PROT_ID_COUNT(floor) + RPC_C_TOWER_FLR_LHS_COUNT_SIZE)

#define RPC_ADDRESS_COUNT(floor)    \
    (RPC_PROT_ID_START(floor) + (floor)->prot_id_count)

#define RPC_ADDRESS_START(floor)     \
    (RPC_ADDRESS_COUNT(floor) + RPC_C_TOWER_FLR_RHS_COUNT_SIZE)

/***********************************************************************/
/*
 * R P C _ A D D R _ V E C T O R _ P _ T
 *
 * A data structure containing an array of pointers to RPC Address data
 * structures, along with a count of the number of pointers present.
 */

typedef struct
{
    unsigned32              len;
    rpc_addr_p_t            addrs[1];
} *rpc_addr_vector_p_t;

/***********************************************************************/
/*
 * R P C _ A U T H N _ P R O T O C O L _ I D _ T
 *
 * The RPC Authentication Protocol IDs.  See the "rpc_c_authn_..." constants
 * in "rpcauth.idl".
 */
typedef unsigned32 rpc_authn_protocol_id_t;

#define RPC_C_AUTHN_PROTOCOL_ID_MAX     5

/*
 * R P C _ A U T H Z _ P R O T O C O L _ I D _ T
 *
 * The RPC Authorization Protocol IDs.  See the "rpc_c_authz_..." constants
 * in "rpcauth.idl".
 */
typedef unsigned32 rpc_authz_protocol_id_t;

/*
 * R P C _ A U T H N _ L E V E L _ T
 *
 * The RPC Authentication levels.  See the "rpc_c_authn_level_..." constants
 * in "rpcauth.idl".
 */
typedef unsigned32 rpc_authn_level_t;

/***********************************************************************/
/*
 * R P C _ A U T H _ I N F O _ T
 *
 * The authentication data structures.  All authentication services capture
 * their session information in a data structure that starts with an
 * "rpc_auth_info_t".  "rpc_auth_info_t"s are attached to binding reps
 * for bindings over which authenticated RPCs are to be performed.
 * 
 * The "authn_svc" field determines which authentication service
 * created and can manage an instance of this structure.  The
 * "prot_auth_info" field is a pointer to session information that's
 * private (and known only by) a particular authentication service and
 * a particular RPC protocol service.
 *
 * All fields except those inside the union are valid regardless of whether
 * the auth info is referenced from a server-side or client-side binding
 * rep.  The "is_server" field determines which leg of the union is valid.
 * If it's "true", then the "privs" field is valid, otherwise the
 * "auth_identity" field is valid.
 */
typedef pointer_t rpc_prot_auth_info_p_t;

typedef struct
{
    rpc_list_t              cache_link;       /* MUST BE 1st */
    unsigned16              refcount;
    unsigned_char_p_t       server_princ_name;
    rpc_authn_level_t       authn_level;
    rpc_authn_protocol_id_t authn_protocol;
    rpc_authz_protocol_id_t authz_protocol;
    unsigned                is_server: 1;
    union
    {
        rpc_auth_identity_handle_t  auth_identity;
	struct
	{
	    rpc_authz_handle_t          privs;  /* pre 1.1 style credentials (client_name or PAC) */
	    rpc_authz_cred_handle_t     *creds;  /* opaque 1.1+ style credentials */
	} s;
    } u;
} rpc_auth_info_t, *rpc_auth_info_p_t;

/***********************************************************************/

/*
 * Keying information (for encryption/integrity checks).
 *
 * One of these structures is associated with each 'connection' using
 * a different key. 
 *
 * There can be many of these associated with each common auth_info,
 * which contains credential information.
 *
 * The "parent auth_info" contains the credentials used to send or
 * receive the key(s) contained herein; this counts as a reference.
 */

typedef struct rpc_key_info_t {
    rpc_auth_info_p_t           auth_info; /* parent auth_info */
    unsigned                    refcnt: 16; /* reference count */
    unsigned                    authn_level: 8;
    unsigned                    is_server: 8;
} rpc_key_info_t, *rpc_key_info_p_t;

/***********************************************************************/
/*
 * R P C _ B I N D I N G _ R E P _ T
 *
 * The binding rep data structure. This is what a binding handle
 * (handle_t) actually points to.   
 *
 * Note well that the meaning (including validity) of some of these fields
 * depends on context.  I.e., some fields don't really mean anything
 * in case you're dealing with the binding rep that's passed to a server
 * stub.
 *
 * A protocol service will typically define its own binding rep data
 * structure with an "rpc_binding_rep_t" as its first field.
 *
 * A little clarification on some fields:
 *
 *  bound_server_instance   true iff the address in the binding handle
 *                          has been used to actually communicate with
 *                          a server and that subsequent calls using
 *                          this handle should also go to this server.
 *                          This flag is used to control call serialization
 *                          so all calls go to the same server (support
 *                          for concurrent / shared binding handles).
 *                          Protocol services may also use this to
 *                          determine whether or not "server binding
 *                          information" (e.g. the rpc_addr info and/or
 *                          protseq private info) needs to be initialized.
 *
 *  addr_has_endpoint       true iff the rpc_addr specifies an endpoint.
 *                          This flag is used to support automatic call
 *                          forwarding for "bound to host" binding handles.
 *                          If bound_server_instance == true, then
 *                          addr_has_endpoint must also be true (but
 *                          not vice versa).
 *
 *  addr_is_dynamic         true iff the rpc_addr specifies an endpoint
 *                          that was dynamically assigned.
 *
 *  calls_in_progress       used to prevent inappropriate API binding
 *                          operations (e.g. rpc_binding_set_object)
 *                          while calls are in progress.
 *                          see RPC_BINDING_CALL_{START,END}()
 *
 *  refcnt                  used to support concurrent / shared handles.
 *                          see RPC_BINDING_{REFERENCE,RELEASE}() 
 *
 *  fork_count              The value of the global fork count at the time
 *                          this handle was created.  This value is used is
 *                          used to detect when an attempt is being made to
 *                          use a binding handle across a fork.
 *
 *  extended_bind_flag      The flag indicates which extended information is
 *                          attached to the binding handle.  Currently this
 *                          can be either cs_method or cs_tags.  If this flag
 *                          is set, there is more data attached to the binding
 *                          in addition to the ordinary binding information.
 */
typedef struct rpc_handle_s_t
{
    /*
     * The following fields are meaningful all the time.
     */
    rpc_list_t                  link;               /* This must be first! */
    rpc_protocol_id_t           protocol_id;
    signed8                     refcnt;
    uuid_t                      obj;
    rpc_addr_p_t                rpc_addr;
    unsigned                    is_server: 1;
    unsigned                    addr_is_dynamic: 1;
    rpc_auth_info_p_t           auth_info;
    unsigned32                  fork_count;
    unsigned32			extended_bind_flag;
    /*
     * The following fields are not meaningful for binding reps
     * that are passed to server stubs.
     */
    unsigned                    bound_server_instance: 1;
    unsigned                    addr_has_endpoint: 1;
    unsigned32                  timeout;    /* com timeout */
    signed32                    calls_in_progress;
    pointer_t                   ns_specific;
    rpc_clock_t                 call_timeout_time;  /* max execution time */
    rpc_protocol_version_p_t    protocol_version;
    rpc_cs_evaluation_t    	cs_eval;	/* code set i14y */
    /*
     *
     */
} rpc_binding_rep_t, *rpc_binding_rep_p_t;
                
#define RPC_BINDING_IS_SERVER(binding_rep)    ((binding_rep)->is_server)
#define RPC_BINDING_IS_CLIENT(binding_rep)    (! (binding_rep)->is_server)

/*
 * A string binding including '\0'. 
 *
 * "object_uuid"@"protocol_sequence":"network_address"["endpoint"]
 * "object_uuid"@"protocol_sequence":"network_address"[endpoint="endpoint"]
 * "object_uuid"@"protocol_sequence":"network_address"\
 *      [endpoint="endpoint",opt="opt"]
 */
#define RPC_C_STRING_BINDING_MAX        1088

typedef unsigned_char_t rpc_string_binding_t[RPC_C_STRING_BINDING_MAX];


/***********************************************************************/
/*
 * Signature of the call thread executor routine provided.
 *
 *      a routine whose address can be passed on a call to
 *      rpc__cthread_invoke_null that will be called back by
 *      the Call Thread Service when the thread is woken up
 */
typedef void (*rpc_prot_cthread_executor_fn_t) _DCE_PROTOTYPE_ ((
        pointer_t               /* args */,
        boolean32               /* call_was_queued */
    ));

/*
 * R P C _ C T H R E A D _ P V T _ I N F O _ T
 *
 * Information *private* to the cthread-package.
 *
 * The "u.server.cthread." fields are logically PRIVATE to the cthread
 * package; they just happen to reside directly in the call rep for
 * convienience;  only the cthread package should be examining and
 * modifying these fields.  Since these fields are logically internal
 * to the cthread package, they are concurrency protected in a fashion
 * that is most sensible for the package (which happens to ba a cthread
 * package *internal* mutex, not the call rep's mutex)!  
 *
 * The queued flag can probably be safely examined as long as no serious
 * decision is made based on its value.  E.g., the protocol service might
 * make some buffering policy decisions based on its value.).  If you
 * want to do such things, use the RPC_CTHREAD_UNSAFE_IS_QUEUED macro
 * so that it's obvious what you're doing.
 * 
 * Note that we force storage unit alignment of the cthread private data.
 * This is done because we must worry about hardware environments in
 * which the code that references bit fields ends up dragging some larger
 * unit from main memory, setting the bit and storing the unit back.
 * This would end up reading and writing other call rep bits which are
 * protected by the call rep mutex (which we won't be holding).
 */
typedef struct
{
    unsigned        : 0;            /* force alignment; see above */
    unsigned        is_queued : 1;
    rpc_prot_cthread_executor_fn_t
                    executor;
    pointer_t       optargs;
    pthread_t       thread_h;       /* valid iff !is_queued */
    pointer_t       qelt;           /* valid iff is_queued */
    unsigned        : 0;            /* force alignment; see above */
} rpc_cthread_pvt_info_t, *rpc_cthread_pvt_info_p_t;

#define RPC_CTHREAD_UNSAFE_IS_QUEUED(call_rep)  \
                (call_rep)->u.server.cthread.is_queued

                    
/***********************************************************************/
/*
 * R P C _ C A L L _ R E P _ T
 *
 * The call rep data structure. This is what the call handle
 * (rpc_call_handle_t) actually points to.  
 *
 * Note that there are two kinds of call reps:  server and client.  Both
 * kinds of call reps have the same initial fields.  A union (.u) is
 * used to contain the part that varies between server and client call
 * reps; the .is_server field says which kind of call rep a particular
 * instance is and which arm of the union is valid.
 *
 * A protocol service will typically define its own call rep data structure
 * with an "rpc_call_rep_t" as its first field.
 *
 * The mutex is lower on the locking hierarchy than the global mutex
 * (i.e. when both locks are required, the global lock must be acquired
 * first to prevent deadlock).
 */
typedef struct
{
    rpc_list_t              link;               /* This must be first! */
    rpc_mutex_t             m;                  /* common mutex */
    rpc_protocol_id_t       protocol_id;        /* RPC protocol to dispatch to */
    unsigned                is_server: 1;       /* union discriminator */
    union                                       /* client or server? */
    {
        struct {                                /* server-only info */
            struct                              /* common cancel info */
            {
                unsigned    accepting: 1;       /* T => can cancel cthread */
                unsigned    queuing: 1;         /* T => queue cancel requests */
                unsigned    had_pending: 1;     /* T => cthread had cancel pending */
                unsigned16  count;              /* # of cancels sent to cthread */
            } cancel;
            rpc_cthread_pvt_info_t  cthread;    /* rpc__cthread *private*; see above */
        } server;
        struct {                                /* client-only info */
            char            dummy;              /* no client-only info (yet) */
        } client;
    } u;
} rpc_call_rep_t, *rpc_call_rep_p_t;

#define RPC_CALL_IS_SERVER(call_h)  ((call_h)->is_server)
#define RPC_CALL_IS_CLIENT(call_h)  (! (call_h)->is_server)

/***********************************************************************/
/*
 * Call Rep Mutex lock macros 
 */

#define RPC_CALL_LOCK_INIT(call)        RPC_MUTEX_INIT((call)->m)
#define RPC_CALL_LOCK(call)             RPC_MUTEX_LOCK((call)->m)
#define RPC_CALL_UNLOCK(call)           RPC_MUTEX_UNLOCK((call)->m)
#define RPC_CALL_TRY_LOCK(call, bp)     RPC_MUTEX_TRY_LOCK((call)->m,(bp))
#define RPC_CALL_LOCK_DELETE(call)      RPC_MUTEX_DELETE((call)->m)
#define RPC_CALL_LOCK_ASSERT(call)      RPC_MUTEX_LOCK_ASSERT((call)->m)
#define RPC_CALL_UNLOCK_ASSERT(call)    RPC_MUTEX_UNLOCKED_ASSERT(call->m)

/***********************************************************************/
/*
 * R P C _ T H R E A D _ C O N T E X T _ T
 *
 * The thread context block data structure. The address of this structure
 * for a given thread can be obtained by doing a pthread_getspecific() call
 * using the rpc_g_thread_context key. This structure can be extended as
 * needed to store any thread-specific context.
 */
typedef struct
{
    signed32                cancel_timeout;
    boolean32               ns_authn_state;
} rpc_thread_context_t, *rpc_thread_context_p_t;


/***********************************************************************/
/*
 * Thread Context macros
 */

/*
 * Get Thread Context - This macro should be called with a pointer to
 * an RPC thread context block.  If the macro had previously been called
 * within that thread it will return a pointer to the existing context
 * block.  If this is the first invocation within the thread a context
 * block will be created and set for the thread.  The user of this macro
 * should check the status value on completion to be sure everything
 * worked ok.  This macro depends on the fact that the thread context
 * key has been initialized (in rpc__init) and will fail if it hasn't
 * been.
 */
#define RPC_GET_THREAD_CONTEXT(thread_context, status) \
{ \
    *status = rpc_s_ok; \
\
    pthread_getspecific (rpc_g_thread_context_key, \
        (pthread_addr_t *) &thread_context); \
\
    if (thread_context == NULL) \
    { \
        RPC_MEM_ALLOC ( \
            thread_context, \
            rpc_thread_context_p_t, \
            sizeof (rpc_thread_context_t), \
            RPC_C_MEM_THREAD_CONTEXT, \
            RPC_C_MEM_WAITOK); \
\
        if (thread_context != NULL) \
        { \
            (thread_context)->cancel_timeout = rpc_c_cancel_infinite_timeout; \
            (thread_context)->ns_authn_state = true; \
            pthread_setspecific (rpc_g_thread_context_key, \
                (pthread_addr_t) thread_context); \
        } \
        else \
        { \
            *status = rpc_s_no_memory; \
        } \
    } \
}


#define RPC_SET_CANCEL_TIMEOUT(value, status) \
{ \
    rpc_thread_context_p_t      _thread_context; \
\
    RPC_GET_THREAD_CONTEXT (_thread_context, status); \
\
    if (*status == rpc_s_ok) \
    { \
        _thread_context->cancel_timeout = value; \
    } \
}    


#define RPC_GET_CANCEL_TIMEOUT(value, status) \
{ \
    rpc_thread_context_p_t      _thread_context; \
\
    RPC_GET_THREAD_CONTEXT (_thread_context, status); \
\
    if (*status == rpc_s_ok) \
    { \
        value = _thread_context->cancel_timeout; \
    } \
}    


/***********************************************************************/
/*
 * Macro to test if running in a signle threaded env.
 */
#ifndef	RPC_IS_SINGLE_THREADED
#define	RPC_IS_SINGLE_THREADED(junk)	(0)
#endif

/***********************************************************************/
/*
 * Runtime Global Mutex lock macros 
 * (used for things that don't have their own lock)
 */

#define RPC_LOCK_INIT(junk)        RPC_MUTEX_INIT(rpc_g_global_mutex)
#define RPC_LOCK(junk)             RPC_MUTEX_LOCK(rpc_g_global_mutex)
#define RPC_UNLOCK(junk)           RPC_MUTEX_UNLOCK(rpc_g_global_mutex)
#define RPC_TRY_LOCK(bp)           RPC_MUTEX_TRY_LOCK(rpc_g_global_mutex,(bp))
#define RPC_LOCK_DELETE(junk)      RPC_MUTEX_DELETE(rpc_g_global_mutex)
#define RPC_LOCK_ASSERT(junk)      RPC_MUTEX_LOCK_ASSERT(rpc_g_global_mutex)
#define RPC_UNLOCK_ASSERT(junk)    RPC_MUTEX_UNLOCKED_ASSERT(rpc_g_global_mutex)

/*
 * Condition variable macros (here because there is no combind.h).
 */
#define RPC_BINDING_COND_INIT(junk) \
    RPC_COND_INIT(rpc_g_global_binding_cond, rpc_g_global_mutex)

#define RPC_BINDING_COND_DELETE(junk) \
    RPC_COND_DELETE(rpc_g_global_binding_cond, rpc_g_global_mutex)

#define RPC_BINDING_COND_WAIT(junk) \
    RPC_COND_WAIT(rpc_g_global_binding_cond, rpc_g_global_mutex)

#define RPC_BINDING_COND_TIMED_WAIT(abstime) \
    RPC_COND_TIMED_WAIT(rpc_g_global_binding_cond, rpc_g_global_mutex, abstime)

#define RPC_BINDING_COND_BROADCAST(junk) \
    RPC_COND_BROADCAST(rpc_g_global_binding_cond, rpc_g_global_mutex)


/***********************************************************************/
/*
 * Possible values for the interface rep ".ifspec_vers" field.
 */
#define RPC_C_IFSPEC_VERS_DCE_1_0       1

/*
 * Possible values for the interface rep ".stub_rtl_vers" field.
 */

#define RPC_C_STUB_RTL_IF_VERS_NCS_1_0  0   /* NCS 1.0 / nidl -s */
#define RPC_C_STUB_RTL_IF_VERS_NCS_1_5  1   /* NCS 1.5 / nidl -m */
#define RPC_C_STUB_RTL_IF_VERS_DCE_1_0  2   /* DCE 1.0 RPC */

#define RPC_IF_VALIDATE(ifrep, status) \
{ \
    if ((ifrep)->ifspec_vers != RPC_C_IFSPEC_VERS_DCE_1_0) \
    { \
        *(status) = rpc_s_unknown_ifspec_vers; \
        return; \
    } \
    if ((ifrep)->stub_rtl_if_vers != RPC_C_STUB_RTL_IF_VERS_NCS_1_0 && \
        (ifrep)->stub_rtl_if_vers != RPC_C_STUB_RTL_IF_VERS_NCS_1_5 && \
        (ifrep)->stub_rtl_if_vers != RPC_C_STUB_RTL_IF_VERS_DCE_1_0) \
    { \
        *(status) = rpc_s_unknown_stub_rtl_if_vers; \
        return; \
    } \
    *(status) = rpc_s_ok; \
}

/***********************************************************************/
/*
 * R P C _ F R E E _ I O V E _ B U F F E R
 *
 * Macro that calls and I/O vector element's dealloc routine and then munge
 * the element's pointer to avoid stupid mistakes.
 */

#define RPC_FREE_IOVE_BUFFER(iove) { \
    assert((iove)->buff_dealloc != NULL); \
    (*(iove)->buff_dealloc)((iove)->buff_addr); \
    (iove)->buff_dealloc = NULL; \
    CLOBBER_PTR((iove)->buff_addr); \
    CLOBBER_PTR((iove)->data_addr); \
}


/***********************************************************************/
/***********************************************************************/
/*
 * Note: This include for comsoc.h has to be after most of the typedef's
 * in this file, because they are used in it's prototype declaration.
 */
#include <comsoc.h>


/***********************************************************************/
/***********************************************************************/
/*
 * Common Binding Services - shared macros and prototypes
 */

/*
 * R P C _ B I N D I N G _ R E F E R E N C E
 *
 * Increment the reference count for the binding handle.
 */

#define RPC_BINDING_REFERENCE(binding_rep_p_t) \
{ \
    RPC_LOCK_ASSERT(0); \
    (binding_rep_p_t)->refcnt++; \
    assert((binding_rep_p_t)->refcnt > 0); \
}

/*
 * R P C _ B I N D I N G _ R E L E A S E
 *
 * Decrement the reference count for the binding handle and
 * NULL the reference (note this takes a &binding_rep_p_t).
 * Call the reall free routine if no more references exist.
 */

#define RPC_BINDING_RELEASE(binding_rep_pp_t, status) { \
    RPC_LOCK_ASSERT(0); \
    assert((*(binding_rep_pp_t))->refcnt > 0); \
    if (--(*(binding_rep_pp_t))->refcnt == 0) \
        rpc__binding_free((binding_rep_pp_t), (status)); \
    else \
    { \
        *(binding_rep_pp_t) = NULL; \
        *(status) = rpc_s_ok; \
    } \
}

/*
 * R P C _ B I N D I N G _ C A L L _ S T A R T
 *
 * A call_start has been performed using the binding handle,
 * increment the binding's calls_in_progress count.
 */

#define RPC_BINDING_CALL_START(binding_rep_p_t) \
{ \
    RPC_LOCK_ASSERT(0); \
    (binding_rep_p_t)->calls_in_progress++; \
    assert((binding_rep_p_t)->calls_in_progress > 0); \
}

/*
 * R P C _ B I N D I N G _ C A L L _ E N D
 *
 * A call_end has been performed using the binding handle,
 * decrement the binding's calls_in_progress count.
 */

#define RPC_BINDING_CALL_END(binding_rep_p_t) { \
    RPC_LOCK_ASSERT(0); \
    assert((binding_rep_p_t)->calls_in_progress > 0); \
    --(binding_rep_p_t)->calls_in_progress; \
}

PRIVATE void rpc__binding_free _DCE_PROTOTYPE_ ((
        rpc_binding_rep_p_t         * /* binding_rep */,
        unsigned32                  * /* status */
    ));

PRIVATE rpc_binding_rep_t *rpc__binding_alloc _DCE_PROTOTYPE_ ((
        boolean32                   /* is_server */,
        uuid_p_t                    /* object_uuid */,
        rpc_protocol_id_t           /* protocol_id */,
        rpc_addr_p_t                /* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__binding_inq_sockaddr _DCE_PROTOTYPE_ ((
        rpc_binding_handle_t        /* binding_h */,
        sockaddr_p_t                */* sa */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__binding_cross_fork _DCE_PROTOTYPE_ ((
        rpc_binding_rep_p_t         /* binding_rep */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__binding_set_prot_version _DCE_PROTOTYPE_ ((
    rpc_binding_handle_t	   /* binding_h */,
    rpc_tower_ref_p_t		   /* tower_ref */,
    unsigned32			    * /* status */));

PRIVATE void rpc__binding_prot_version_alloc _DCE_PROTOTYPE_ ((
    rpc_protocol_version_p_t    * /* prot_version */,
    unsigned32		          /* major_version */,
    unsigned32		          /* minor_version */,
    unsigned32		        * /* status */));

PRIVATE void rpc__binding_prot_version_free _DCE_PROTOTYPE_ ((
    rpc_protocol_version_p_t	* /* protocol_version */ ));


/***********************************************************************/
/***********************************************************************/
/*
 * Common Call Services - shared macros and prototypes
 */

/***********************************************************************/
/*
 * R P C _ _ C A L L _ R E J E C T
 *
 */

PRIVATE void rpc__call_reject _DCE_PROTOTYPE_ ((
        rpc_call_rep_p_t            /* call_r */,
        unsigned32                  /* architected_status */,
        unsigned32                  status
    ));


/***********************************************************************/
/***********************************************************************/
/*
 * Common Call Thread Services - shared macros and prototypes
 */


/***********************************************************************/
/***********************************************************************/
/*
 * Common Interface Services - shared macros and prototypes
 */

PRIVATE void rpc__if_init _DCE_PROTOTYPE_ ((
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__if_fork_handler _DCE_PROTOTYPE_ ((
        rpc_fork_stage_id_t stage
    ));

PRIVATE void rpc__if_lookup _DCE_PROTOTYPE_ ((
        uuid_p_t                    /* if_uuid */,
        unsigned32                  /* if_vers */,
        uuid_p_t                    /* type_uuid */,
        unsigned16                  */* ihint */,
        rpc_if_rep_p_t              */* ifspec */,
        rpc_v2_server_stub_epv_t    */* ss_epv */,
        rpc_mgr_epv_t               */* mgr_epv */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__if_set_wk_endpoint _DCE_PROTOTYPE_ ((      
        rpc_if_rep_p_t              /* ifspec */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  *st
    ));

PRIVATE boolean rpc__if_id_compare _DCE_PROTOTYPE_ ((
        rpc_if_id_p_t               /* if_id_ref */,
        rpc_if_id_p_t               /* if_id */,
        unsigned32                  /* if_vers_option */,
        unsigned32                  * /* status */
    ));

PRIVATE unsigned32 rpc__if_mgmt_inq_num_registered _DCE_PROTOTYPE_ ((void));

PRIVATE void rpc__if_mgmt_inq_if_ids _DCE_PROTOTYPE_ ((
        rpc_if_id_vector_p_t        */* if_info */,
        unsigned32                  * /* status */
    ));


/***********************************************************************/
/***********************************************************************/
/*
 * Common Network Services - shared macros and prototypes
 */

PRIVATE void rpc__network_init _DCE_PROTOTYPE_ ((
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_fork_handler _DCE_PROTOTYPE_ ((
        rpc_fork_stage_id_t stage
    ));

PRIVATE void rpc__network_add_desc _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        boolean32                   /* is_server */,
        boolean32                   /* is_dynamic */,
        rpc_protseq_id_t            /* rpc_protseq_id */,
        pointer_t                   /* priv_info */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_remove_desc _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_set_priv_info _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        pointer_t                   /* priv_info */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_inq_priv_info _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        pointer_t                   */* priv_info */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_inq_prot_version _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            /* rpc_protseq_id */,
        unsigned8                   */* prot_id */,
        unsigned32                  */* version_major */,
        unsigned32                  */* version_minor */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_pseq_from_pseq_id _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            /* rpc_protseq_id */, 
        unsigned_char_p_t           */* protseq */,
        unsigned32                  * /* status */
    ));

PRIVATE rpc_protocol_id_t rpc__network_pseq_id_from_pseq _DCE_PROTOTYPE_  ((
        unsigned_char_p_t           /* rpc_protseq */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__network_inq_local_addr _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            /* rpc_protseq_id */,
        unsigned_char_p_t           /* endpoint */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));




/***********************************************************************/
/***********************************************************************/
/*
 * Common Network Address Family Services - shared macros and prototypes
 */

PRIVATE void rpc__naf_addr_alloc _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            /* rpc_protseq_id */,
        rpc_naf_id_t                /* naf_id */,
        unsigned_char_p_t           /* endpoint */,
        unsigned_char_p_t           /* netaddr */,
        unsigned_char_p_t           /* network_options */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_copy _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* src_rpc_addr */,
        rpc_addr_p_t                */* dst_rpc_addr */,
        unsigned32                  * /* status */
    ));
    
PRIVATE void rpc__naf_addr_overcopy _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* src_rpc_addr */,
        rpc_addr_p_t                */* dst_rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_free _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_vector_free _DCE_PROTOTYPE_ ((
        rpc_addr_vector_p_t         */* rpc_addr_vec */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_set_endpoint _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           /* endpoint */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_inq_endpoint _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* rpc_addr */,
        unsigned_char_t             **/* endpoint */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_set_netaddr _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           /* netaddr */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_inq_netaddr _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* rpc_addr */,
        unsigned_char_t             **/* netaddr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_set_options _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           /* network_options */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_addr_inq_options _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* rpc_addr */,
        unsigned_char_t             **/* network_options */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_desc_inq_addr _DCE_PROTOTYPE_ ((
        rpc_protseq_id_t            /* protseq_id */,
        rpc_socket_t                /* desc */,
        rpc_addr_vector_p_t         */* rpc_addr_vec */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_desc_inq_network _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        rpc_naf_id_t                */* naf_id */,
        rpc_network_if_id_t         */* socket_type */,
        rpc_network_protocol_id_t   */* protocol_id */,
        unsigned32                  * /* status */
    ));
    
PRIVATE void rpc__naf_desc_inq_naf_id _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        rpc_naf_id_t                */* naf_id */,
        unsigned32                  * /* status */
    ));
    
PRIVATE void rpc__naf_desc_inq_protseq_id _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        rpc_network_protocol_id_t   /* protocol_id */,
        rpc_protseq_id_t            */* protseq_id */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_desc_inq_peer_addr _DCE_PROTOTYPE_ ((
        rpc_socket_t                /* desc */,
        rpc_protseq_id_t            /* protseq_id */,
        rpc_addr_p_t                */* addr */,
        unsigned32                  * /* status */
    ));
    
PRIVATE void rpc__naf_inq_max_tsdu _DCE_PROTOTYPE_ ((  
        rpc_protseq_id_t            /* protseq_id */,
        unsigned32                  */* max_tsdu */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_get_broadcast _DCE_PROTOTYPE_ ((
        rpc_naf_id_t                /* naf_id */,
        rpc_protseq_id_t            /* protseq_id */,
        rpc_addr_vector_p_t         */* rpc_addrs */,
        unsigned32                  * /* status */
    ));

PRIVATE boolean rpc__naf_addr_compare _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* addr1 */,
        rpc_addr_p_t                /* addr2 */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_inq_max_pth_unfrg_tpdu _DCE_PROTOTYPE_ ((  
        rpc_addr_p_t                /* rpc_addr */,
        unsigned32                  */* max_tpdu */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_inq_max_loc_unfrg_tpdu _DCE_PROTOTYPE_ ((  
        rpc_protseq_id_t            /* pseq_id */,
        unsigned32                  */* max_tpdu */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_set_pkt_nodelay _DCE_PROTOTYPE_ ((  
        rpc_socket_t                /* desc */,
        rpc_addr_p_t                /* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE boolean rpc__naf_is_connect_closed _DCE_PROTOTYPE_ ((  
        rpc_socket_t                /* desc */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_inq_max_frag_size _DCE_PROTOTYPE_ ((
        rpc_addr_p_t                /* rpc_addr */,
        unsigned32                  * /* max_frag_size */,
        unsigned32                  * /* status */
    ));


/***********************************************************************/
/***********************************************************************/
/*
 * Common Object Services - shared macros and prototypes
 */

PRIVATE void rpc__obj_init _DCE_PROTOTYPE_ (( unsigned32 * /* status */ ));

PRIVATE void rpc__obj_fork_handler _DCE_PROTOTYPE_ (( rpc_fork_stage_id_t ));


/***********************************************************************/
/***********************************************************************/
/*
 * Common Protocol Tower Services - shared macros and prototypes
 */

PRIVATE void rpc__naf_addr_from_sa _DCE_PROTOTYPE_ ((
        sockaddr_p_t                /* sockaddr */,
        unsigned32                  /* sockaddr_len */,
        rpc_addr_p_t                */* rpc_addr */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__naf_tower_flrs_from_addr _DCE_PROTOTYPE_ ((
        rpc_addr_p_t               /* rpc_addr */,
        twr_p_t                    */* lower_flrs */,
        unsigned32                 * /* status */
    ));

PRIVATE void rpc__naf_tower_flrs_to_addr _DCE_PROTOTYPE_ ((
        byte_p_t                   /* tower_octet_string */,
        rpc_addr_p_t               */* rpc_addr */,
        unsigned32                 * /* status */
    ));

/***********************************************************************/
/***********************************************************************/
/*
 * Common Server (object) Services - shared macros and prototypes
 */

PRIVATE void rpc__server_register_if_int _DCE_PROTOTYPE_ ((

        rpc_if_handle_t             /* ifspec_h */,
        uuid_p_t                    /* mgr_type_uuid */,
        rpc_mgr_epv_t               /* mgr_epv */,
        boolean32                   /* internal */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__server_unregister_if_int _DCE_PROTOTYPE_ ((
        rpc_if_handle_t             /* ifspec_h */,
        uuid_p_t                    /* mgr_type_uuid */,
        rpc_if_handle_t             */* rtn_ifspec_h */,
        unsigned32                  * /* status */
    ));

PRIVATE void rpc__server_stop_listening _DCE_PROTOTYPE_ ((
        unsigned32                  * /* status */
    ));

PRIVATE boolean32 rpc__server_is_listening _DCE_PROTOTYPE_ ((void));



/***********************************************************************/
/***********************************************************************/
/*
 * Common Utility Services - shared macros and prototypes
 */

PRIVATE unsigned32 rpc__strcspn _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           /* string */,
        char                        * /* term_set */
    ));
    
PRIVATE void rpc__strncpy _DCE_PROTOTYPE_ ((
        unsigned_char_p_t           /* dst_string */,
        unsigned_char_p_t           /* src_string */,
        unsigned32                  /* max_length */
    ));

PRIVATE unsigned32 rpc__strsqz _DCE_PROTOTYPE_ (( unsigned_char_p_t));


PRIVATE unsigned_char_p_t rpc__stralloc _DCE_PROTOTYPE_ (( unsigned_char_p_t));

#ifdef ATFORK_SUPPORTED
PRIVATE boolean32 rpc__fork_is_in_progress _DCE_PROTOTYPE_ ((void));
#endif	/* ATFORK_SUPPORTED */


/***********************************************************************/
/***********************************************************************/

/*
 * Name Service binding->ns_specific free function.
 */

typedef void (*rpc_g_ns_specific_free_fn_t) _DCE_PROTOTYPE_ ((
        pointer_t   * /* ns_specific*/
    ));

EXTERNAL rpc_g_ns_specific_free_fn_t  rpc_g_ns_specific_free_fn;

/***********************************************************************/
/*
 * Common Authentication Services - shared macros and prototypes
 */

/***********************************************************************/
/*
 * RPC Protocol specific authentication service EPV.
 */

typedef void (*rpc_auth_rpc_prot_fn_t)(void );

typedef struct
{                                   
    rpc_auth_rpc_prot_fn_t      prot_specific;
} rpc_auth_rpc_prot_epv_t, *rpc_auth_rpc_prot_epv_p_t;

typedef rpc_auth_rpc_prot_epv_p_t *rpc_auth_rpc_prot_epv_tbl_t;
typedef rpc_auth_rpc_prot_epv_tbl_t *rpc_auth_rpc_prot_epv_tbl_p_t;

PRIVATE void rpc__auth_info_reference _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t       /* auth_info */
    ));

PRIVATE void rpc__auth_info_release _DCE_PROTOTYPE_ ((
        rpc_auth_info_p_t       * /* info */
    ));

PRIVATE rpc_auth_rpc_prot_epv_t *rpc__auth_rpc_prot_epv _DCE_PROTOTYPE_ ((
        rpc_authn_protocol_id_t /* authn_prot_id */,
        rpc_protocol_id_t       /* rpc_prot_id */
    ));

PRIVATE unsigned32 rpc__auth_cvt_id_api_to_wire _DCE_PROTOTYPE_ ((
        rpc_authn_protocol_id_t /* api_authn_prot_id */,
        unsigned32              * /* status */
    ));

PRIVATE rpc_authn_protocol_id_t rpc__auth_cvt_id_wire_to_api _DCE_PROTOTYPE_((
        unsigned32              /* wire_authn_prot_id */,
        unsigned32              * /* status */
    ));

PRIVATE boolean32 rpc__auth_inq_supported _DCE_PROTOTYPE_ ((rpc_authn_protocol_id_t));

#endif /* _COM_H */
