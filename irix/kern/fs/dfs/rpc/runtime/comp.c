/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comp.c,v 65.4 1998/03/23 17:30:45 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: comp.c,v $
 * Revision 65.4  1998/03/23 17:30:45  gwehrman
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
 * Revision 1.1.104.1  1996/05/10  13:10:29  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:13 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:41 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:18 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:00 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:31 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:18 UTC  tatsu_s  /main/HPDCE02/2]
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
 * 	merge in fixes from mk_mothra_repair branch
 * 	[1994/07/19  20:24 UTC  mk  /main/HPDCE01/3]
 *
 * 	conditionalize references to uuid__create_standalone out of krpc build
 * 	[1994/07/19  20:21 UTC  mk  /main/HPDCE01/mk_mothra_repair/1]
 *
 * 	merge in mk_sd_uuids branch
 * 	[1994/06/14  22:37 UTC  mk  /main/HPDCE01/2]
 *
 * 	standalone uuid generation support for sd
 * 	[1994/05/27  16:18 UTC  mk  /main/HPDCE01/mk_sd_uuids/1]
 *
 * 	merge in IC1 loopback support
 * 	[1994/03/08  23:52 UTC  mk  /main/HPDCE01/1]
 *
 * 	add loopback support for sd
 * 	[1994/03/08  00:54 UTC  mk  /main/mk_LOOPBACK_IC1/1]
 *
 * Revision 1.1.100.1  1994/01/21  22:35:42  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:55:41  cbrooks]
 * 
 * Revision 1.1.8.1  1993/09/28  21:58:12  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:36  weisman]
 * 
 * Revision 1.1.5.7  1993/02/05  16:11:27  raizen
 * 	 26-jan-93 dr        Back out changes from Revision 1.1.5.3
 * 	[1993/02/04  23:20:19  raizen]
 * 
 * Revision 1.1.5.6  1993/01/08  21:47:10  weisman
 * 	Changed rpc_g_authenticated default to TRUE.
 * 	[1993/01/07  21:29:54  weisman]
 * 
 * Revision 1.1.5.5  1993/01/03  23:22:36  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:02:38  bbelch]
 * 
 * Revision 1.1.5.4  1992/12/23  20:44:41  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:34:22  zeliff]
 * 
 * Revision 1.1.5.3  1992/12/21  19:14:51  wei_hu
 * 	       9-dec-92 raizen    Add rpc_g_authenticated.  [OT4854]
 * 
 * 	      30-sep-92 ebm       Add vms conditional compilation to provide psect
 * 	                          name for rpc_g_protseq_id definition (in order to
 * 	                          cluster it with transfer vector (same address
 * 	                          for each build) thus providing upward
 * 	                          compatibility for rpc daemon).
 * 	[1992/12/18  20:27:29  wei_hu]
 * 
 * Revision 1.1.5.2  1992/10/13  20:54:47  weisman
 * 	      21-aug-92 wh        Add rpc_g_default_pthread_attr.
 * 	[1992/10/13  20:47:27  weisman]
 * 
 * Revision 1.1.3.2  1992/05/01  17:02:25  rsalz
 * 	  5-mar-92 wh        DCE 1.0.1 merge.
 * 	 31-jan-92 ko        change ncacn_dnet_nsp network if type to
 * 	                     stream instead of sequenced packet because
 * 	                     of ultrix/decnet flow control problems with
 * 	                     non-blocking sockets and select.
 * 	[1992/05/01  16:50:05  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:09:55  devrcs
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
**      comp.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Definitions of storage internal to Common Communications Service component.
**
**
*/

#include <commonp.h>    /* Common internals for RPC Runtime system      */
#include <com.h>        /* Externals for Common Services component      */
#include <comprot.h>    /* Externals for common Protocol Services       */
#include <comnaf.h>     /* Externals for common NAF Services            */
#include <comp.h>       /* Internals for Common Services component      */
#include <comfwd.h>     /* Externals for Common Services Fwd component  */

/***********************************************************************/
/*
 * R P C _ G _ I N I T I A L I Z E D
 *
 * The value that indicates whether or not the RPC runtime has previously
 * been initialized (via a call to rpc__init). Its declaration is
 * in com.h since the Naming Service may need to invoke the
 * RPC_VERIFY_INIT macro.
 */
GLOBAL boolean              rpc_g_initialized = false;






/***********************************************************************/
/*
 * R P C _ G _ T H R E A D _ C O N T E X T _ K E Y
 *
 * The key visible to all threads that contains a pointer to the
 * per-thread context block.
 */
GLOBAL pthread_key_t        rpc_g_thread_context_key;

     
/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ M U T E X
 *
 * The global mutex used for the entire Communications Service. Note
 * that this may be temporary since a per-data structure mutex may
 * be employed. Its declaration is in com.h since RPC Protocol
 * Services will need to reference it through the mutex macros.
 */
GLOBAL rpc_mutex_t          rpc_g_global_mutex;


/***********************************************************************/
/*
 * R P C _ G _ G L O B A L _ B I N D I N G _ C O N D
 *
 * The global binding handle condition variable used for call
 * serialization.  This condition variable is protected by the global
 * mutex.  Note that this may be temporary since a per-binding handle
 * cond var / mutex may be employed.
 */
GLOBAL rpc_cond_t           rpc_g_global_binding_cond;


/***********************************************************************/
/*
 * R P C _ G _ F O R K _ C O U N T
 *
 * The global fork count used to detect when a process using
 * RPC has forked.  After a fork, the parent is allowed to proceed
 * unaffected--all RPC helper threads are restarted, and
 * all RPC state maintained.  In the child, however, all state
 * is dropped, and the RPC initialization process will have
 * to be repeated if the child tries to use RPC.  
 * 
 * The counter here is necessitated by the fact that it is possible
 * for the application to hold the only reference to certain
 * data structures allocated by the runtime;  the example of concern
 * here is binding handles.  Since we have no way at present
 * of tracking dow these binding handles during the normal fork
 * handling, we need some way to recognize them if the application
 * tries to use one *after* the fork.  By storing the current
 * global fork count in each handle, and then incrementing the
 * global count in the child of a fork, we can recognize such
 * handles the next time they are used.  Protocol specific routine
 * are then called to perform whatever actions are necessary to
 * drop any state associated with the handle.
 *
 * Note that we are currently stranding memory across forks; eg.
 * call handles, mutexes, etc.  This is considered a necessary and
 * acceptable evil based on the following considerations:
 *
 *     1) The reset/release/etc. routines that exist today are
 *        designed to carry out any pending operations associated
 *        with the data structure.  For example, before freeing
 *        a call handle, the DG code will try to send out any
 *        pending acknowledgements.  For this reason, most of
 *        these routines are unusable.
 *     2) It is too late to redesign all of these routines to 
 *        incorporate the correct fork-aware behavior.
 *     3) Vaporizing all state in the child of a fork creates a 
 *        scenario that is easier to understand, and implement
 *        correctly.
 *     4) The amount of memory stranded does not seem to be enough
 *        to worry about.  We are assuming that the child of
 *        a fork will not itself fork a child that will itself
 *        fork a child, etc., eventually filling the VA space
 *        with stranded memory.
 */
GLOBAL unsigned32 rpc_g_fork_count;

/***********************************************************************/
/*
 * R P C _ G _ S I N G L E _ T H R E A D
 *
 * The value that indicates whether or not the process is
 * single threaded or not.  Its declaration is in com.h.
 * This variable is initialized by rpc__init() and will be
 * updated by rpc_call_start().
 */

GLOBAL boolean  rpc_g_is_single_threaded = true;


/***********************************************************************/
/*
 * R P C _ G _ F W D _ F N
 *
 * The global forwarding map function variable.  Its value indicates
 * whether or not the RPC runtime should be performing forwarding services
 * and if so, the forwarding map function to use.
 */
GLOBAL rpc_fwd_map_fn_t     rpc_g_fwd_fn = NULL;

/***********************************************************************/
/*
 * R P C _ G _ S E R V E R _ P T H R E A D _ A T T R
 *
 * A pthread attribute for server thread creation.
 * Initialized by rpc_init().
 */
GLOBAL pthread_attr_t   rpc_g_server_pthread_attr;

/***********************************************************************/
/*
 * R P C _ G _ R U N T I M E _ P T H R E A D _ A T T R
 *
 * A pthread attribute for internal thread creation.  This parameter
 * is of particular interest to those threads internal to the runtime
 * that can call security routines.  These threads include: the
 * network listener and the receiver threads.
 * Initialized by rpc_init().
 */
GLOBAL pthread_attr_t   rpc_g_default_pthread_attr;

/***********************************************************************/
/*
 * RPCMEM package statistics.  Extern'd in "rpcmem.h>.
 *
 * ### !!! seems like this should be in the (non-existent) "rpcmem.c" file.
 */
GLOBAL rpc_mem_stats_elt_t  rpc_g_mem_stats[RPC_C_MEM_MAX_TYPES];

/***********************************************************************/
/*
 * R P C _ G _ N S _ S P E C I F I C _ F R E E _ F N
 *
 * The global NS binding->ns_specific free function.  The NS init routine
 * inits this.  It's purpose is to prevent the runtime from always pulling
 * in the name service modules when they're not necessary.
 */
GLOBAL rpc_g_ns_specific_free_fn_t  rpc_g_ns_specific_free_fn = NULL;


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
 *      port_restriction_list_p
 *                      An optionally pointer to a port_restriction_list 
 *                      object.
 */

GLOBAL 
#ifdef VMS
/* Provide psect name if VMS */
{"rpc_sym9_g_protseq_id"}
#endif
rpc_protseq_id_elt_t     rpc_g_protseq_id[RPC_C_PROTSEQ_ID_MAX] = 
{
    {                                   /* Connection-RPC / IP / TCP */
        0,
        RPC_C_PROTSEQ_ID_NCACN_IP_TCP,
        RPC_C_PROTOCOL_ID_NCACN,
        RPC_C_NAF_ID_IP,
        RPC_C_NETWORK_PROTOCOL_ID_TCP,
        RPC_C_NETWORK_IF_ID_STREAM,
        RPC_PROTSEQ_NCACN_IP_TCP,
        (rpc_port_restriction_list_p_t) NULL
    },

    {                                   /* Connection-RPC / DECnet / NSP */
        0,                                
        RPC_C_PROTSEQ_ID_NCACN_DNET_NSP,
        RPC_C_PROTOCOL_ID_NCACN,
        RPC_C_NAF_ID_DNET,
        RPC_C_NETWORK_PROTOCOL_ID_UNS,
        RPC_C_NETWORK_IF_ID_STREAM,
        RPC_PROTSEQ_NCACN_DNET_NSP,
        (rpc_port_restriction_list_p_t) NULL
    },

    {                                   /* Connection-RPC / OSI / DNASESSION */
        0,                              
        RPC_C_PROTSEQ_ID_NCACN_OSI_DNA,     
        RPC_C_PROTOCOL_ID_NCACN,
        RPC_C_NAF_ID_OSI,
        RPC_C_NETWORK_PROTOCOL_ID_DNASESSION,
        RPC_C_NETWORK_IF_ID_SEQPACKET,
        RPC_PROTSEQ_NCACN_OSI_DNA,
        (rpc_port_restriction_list_p_t) NULL
    },

    {                                   /* Datagram-RPC / IP / UDP */
        0,
        RPC_C_PROTSEQ_ID_NCADG_IP_UDP,
        RPC_C_PROTOCOL_ID_NCADG,
        RPC_C_NAF_ID_IP,
        RPC_C_NETWORK_PROTOCOL_ID_UDP,
        RPC_C_NETWORK_IF_ID_DGRAM,
        RPC_PROTSEQ_NCADG_IP_UDP,
        (rpc_port_restriction_list_p_t) NULL
    },

    {                                   /* Datagram-RPC / DDS */
        0,
        RPC_C_PROTSEQ_ID_NCADG_DDS,
        RPC_C_PROTOCOL_ID_NCADG,
        RPC_C_NAF_ID_DDS,
        RPC_C_NETWORK_PROTOCOL_ID_DDS,
        RPC_C_NETWORK_IF_ID_DGRAM,
        RPC_PROTSEQ_NCADG_DDS,
        (rpc_port_restriction_list_p_t) NULL
#ifdef TEST_PROTOCOL
    },

    {                                   /* Test-RPC / IP / TCP */
        0,
        RPC_C_PROTSEQ_ID_NCATP_IP_TCP,
        RPC_C_PROTOCOL_ID_NCATP,
        RPC_C_NAF_ID_IP,
        RPC_C_NETWORK_PROTOCOL_ID_TCP,
        RPC_C_NETWORK_IF_ID_STREAM,
        RPC_PROTSEQ_NCATP_IP_TCP,
        (rpc_port_restriction_list_p_t) NULL
#endif /* TEST_PROTOCOL */
    }
};



/***********************************************************************/
/*
 * R P C _ G _ P R O T O C O L _ I D
 *
 * The RPC Protocol ID table.  Each RPC Protocol has an entry in this
 * table.  This table is index by RPC Protocol ID.
 *
 * Note that the ".rpc_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 *
 * The fields are:
 *
 *      prot_init       The address of an initialization routine in the
 *                      Protocol Service that will be called by rpc__init.
 *      
 *      prot_fork_handler  The address of a routine to call to handle 
 *                      protocol specific, fork-related processing.
 *
 *      rpc_protocol_id A constant identifier for this RPC Protocol.
 *      
 *      call_epv        An entry point vector for the Call Services in
 *                      the Protocol Service.
 *      
 *      mgmt_epv        An entry point vector for the Management Services in 
 *                      the Protocol Service.
 *      
 *      binding_epv     An entry point vector for the Binding Services
 *                      in the Protocol Service.
 *      
 *      network_epv     An entry point vector for the Network Services
 *                      in the Protocol Service.
 */

GLOBAL rpc_protocol_id_elt_t     rpc_g_protocol_id[RPC_C_PROTOCOL_ID_MAX] = 
{
#ifdef PROT_NCACN
    {
        rpc__ncacn_init,                /* Connection-RPC */
        NULL,
        RPC_C_PROTOCOL_ID_NCACN,
        NULL, NULL, NULL, NULL 
    },
#else
    {NULL},
#endif

#ifdef PROT_NCADG
    {
        rpc__ncadg_init,                /* Datagram-RPC */
        NULL,
        RPC_C_PROTOCOL_ID_NCADG,
        NULL, NULL, NULL, NULL 
    }
#else
    {NULL}
#endif

#ifdef PROT_NCATP
    ,{
        rpc__ncatp_init,                /* Test-RPC */
        NULL,
        RPC_C_PROTOCOL_ID_NCATP,
        NULL, NULL, NULL, NULL 
    }
#endif
};

/***********************************************************************/
/*
 * R P C _ G _ N A F _ I D
 *
 * The Network Address Family ID table.  This table is indexed by a NAF
 * ID.
 *
 * Each Network Address Family Extension has an entry in this table.
 * Note that this is a sparse table because it uses the Unix Address
 * Family ID's as NAF ID's.
 *
 * Note that the ".naf_id" field of i'th element in the table is always
 * "i".  While redundant, this is useful so that you can pass pointers
 * to individual table elements.
 * 
 * The fields are:
 *
 *      naf_init        The address of an initialization routine in the
 *                      NAF Service that will be called by rpc__init
 *
 *      naf_id          A constant identifier for this NAF.
 *
 *      net_if_id       A constant identifier for the network interface
 *                      type used in the NAF initialization routine (when
 *                      determining if this NAF is supported).
 *      
 *      naf_epv         An entry point vector for the NAF Service.
 */

GLOBAL rpc_naf_id_elt_t     rpc_g_naf_id[RPC_C_NAF_ID_MAX] = 
{
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
#ifdef NAF_IP
    {
        rpc__ip_init,
        RPC_C_NAF_ID_IP,
        RPC_C_NETWORK_IF_ID_DGRAM,
        NULL 
    },
#else
    {NULL, 0, 0, NULL},
#endif
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
#ifdef NAF_DNET
    {
        rpc__dnet_init,
        RPC_C_NAF_ID_DNET,
        RPC_C_NETWORK_IF_ID_SEQPACKET,
        NULL 
    },
#else
    {NULL, 0, 0, NULL},
#endif
#ifdef NAF_DDS
    {
        rpc__dds_init,
        RPC_C_NAF_ID_DDS,
        RPC_C_NETWORK_IF_ID_DGRAM,
        NULL 
    },
#else
    {NULL, 0, 0, NULL},
#endif
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
    {NULL, 0, 0, NULL},
#ifdef NAF_OSI
    {
        rpc__osi_init,
        RPC_C_NAF_ID_OSI,
        RPC_C_NETWORK_IF_ID_STREAM,
        NULL 
    }
#else
    {NULL, 0, 0, NULL}
#endif
};

/***********************************************************************/
/*
 * R P C _ G _ A U T H N _ P R O T O C O L _ I D
 *
 * The RPC Authentication Protocol ID table.
 *
 * Each RPC Authentication protocol has an entry in this table.  These
 * entries include the following fields:
 *
 *      auth_init           The address of an initialization routine in the
 *                          Authentication Service that will be called by 
 *                          rpc__init.
 *
 *      authn_protocol_id   A constant identifier for this Authentication Service.
 *
 *      dce_rpc_authn_protocol_id_t
 *                          The value that goes into RPC protocol messages to
 *                          identify which authentication protocol is in use.
 *
 *      epv                 An entry point vector for the Authentication Service
 *                          functions.
 *
 * Note that the ".authn_protocol_id" field of i'th element in the table
 * is always "i".  While redundant, this is useful so that you can pass
 * pointers to individual table elements.
 *
 * Note that the ".auth_protocol_id" contains API values (see
 * "rpc_c_authn_..." constants in "rpc.idl").
 * "dce_rpc_authn_protocol_id_t" contains architectural values that appear
 * in network messages (see "dce_c_rpc_authn_protocol_..." constants in
 * "nbase.idl").
 */

GLOBAL rpc_authn_protocol_id_elt_t rpc_g_authn_protocol_id[RPC_C_AUTHN_PROTOCOL_ID_MAX] =
{

    {                               /* 0 */
        NULL, 
        rpc_c_authn_none, 
        dce_c_rpc_authn_protocol_none, 
        NULL
    },
    {                               /* 1 */
#ifdef AUTH_KRB 
        rpc__krb_init,
#else
        NULL,
#endif
        rpc_c_authn_dce_private,
        dce_c_rpc_authn_protocol_krb5,
        NULL,
    },
    {                               /* 2 (reserved for dce_public) */
        NULL, 
        rpc_c_authn_dce_public,
        /* dce_c_rpc_authn_protocol_... */ 0,
        NULL
    },
    {                               /* 3 */
#ifdef AUTH_DUMMY
        rpc__noauth_init,
#else
        NULL,
#endif
        rpc_c_authn_dce_dummy,
        dce_c_rpc_authn_protocol_dummy,
        NULL,
    },
    {                               /* 4 (reserved for dssa_public) */
        NULL,
        rpc_c_authn_dssa_public, 
        0,
        NULL
    }
};


