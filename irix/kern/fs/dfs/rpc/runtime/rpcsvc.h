/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 *
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: rpcsvc.h,v $
 * Revision 65.5  1998/04/01 14:17:05  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed macros defined as 0 to ((void)0).
 *
 * Revision 65.4  1998/03/24 16:00:13  lmc
 * Fix some of the #ifdef's for SGI_RPC_DEBUG.  Also typecast a few
 * variables to fix warnings.
 *
 * Revision 65.2  1997/10/20  19:17:12  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.4.2  1996/02/18  22:56:57  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:57  marty]
 *
 * Revision 1.1.4.1  1995/12/08  00:22:16  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  00:00:32  root]
 * 
 * Revision 1.1.2.4  1994/09/15  13:36:24  rsarbo
 * 	restore minimal debug/error printf functionality in KRPC
 * 	[1994/09/15  13:35:05  rsarbo]
 * 
 * Revision 1.1.2.3  1994/08/18  22:09:31  hopkins
 * 	Serviceability:
 * 	  Remove PRIVATE designation from function
 * 	  prototypes; this file is now included from
 * 	  idl/lib, and PUBLIC/PRIVATE are not defined
 * 	  when building there.  Including commonp.h to
 * 	  get the definitions seems risky, considering
 * 	  that both macros evaluate to nothing anyway.
 * 	[1994/08/18  21:48:56  hopkins]
 * 
 * Revision 1.1.2.2  1994/06/09  19:03:40  annie
 * 	cr10871 - expand copyright
 * 	[1994/06/09  18:35:20  annie]
 * 
 * Revision 1.1.2.1  1994/05/19  21:15:06  hopkins
 * 	More serviceability.
 * 	[1994/05/18  21:25:42  hopkins]
 * 
 * 	New file for serviceability.
 * 
 * $EndLog$
 */

/*
 * Macros, typedefs, and function prototypes
 * for Serviceability.
 */

#ifndef _RPCSVC_H
#define _RPCSVC_H	1

#ifdef	DCE_RPC_SVC

#include <dce/dce.h>
#include <dce/dce_msg.h>
#include <dce/dce_svc.h>
#include <dce/dcerpcmac.h>
#include <dce/dcerpcmsg.h>
#include <dce/dcerpcsvc.h>

#include <string.h>

#ifdef KERNEL
/*
 * dce_ksvc_printf is stripped down dce_svc_printf 
 * for kernel which offsets into 
 * rpc_g_svc_msg_table using message_index.
 */
#define RPC_DCE_SVC_PRINTF(args) dce_ksvc_printf args

/*
 *  Prototype for debugging print call.  Added for mongoose compiler.
 */
#ifdef SGIMIPS
void dce_ksvc_printf( dce_svc_handle_t  handle,
#if     defined(DCE_SVC_WANT__FILE__)
      const char *file, const int line,
#endif  /* defined(DCE_SVC_WANT__FILE__) */
	char *argtypes, const unsigned32 table_index,
	const unsigned32 attributes, const unsigned32 message_index,
	...);
#endif


/*
 * New debug switches map to offsets in
 * rpc_g_dbg_switches in kernel.  In user 
 * space, they map to S12Y sub-components.
 */
#define rpc_e_dbg_general rpc_es_dbg_general
#define rpc_e_dbg_mutex rpc_es_dbg_mutex
#define rpc_e_dbg_xmit  rpc_es_dbg_xmit
#define rpc_e_dbg_recv rpc_es_dbg_recv
#define rpc_e_dbg_dg_lossy rpc_es_dbg_dg_lossy
#define rpc_e_dbg_dg_state rpc_es_dbg_dg_state
#define rpc_e_dbg_ip_max_pth_unfrag_tpdu \
    rpc_es_dbg_ip_max_pth_unfrag_tpdu
#define rpc_e_dbg_ip_max_loc_unfrag_tpdu \
    rpc_es_dbg_ip_max_loc_unfrag_tpdu
#define rpc_e_dbg_dds_max_pth_unfrag_tpdu \
    rpc_es_dbg_dds_max_pth_unfrag_tpdu
#define rpc_e_dbg_dds_max_loc_unfrag_tpdu \
    rpc_es_dbg_dds_max_loc_unfrag_tpdu
#define rpc_e_dbg_dg_rq_qsize rpc_es_dbg_dg_rq_qsize
#define rpc_e_dbg_cancel rpc_es_dbg_cancel
#define rpc_e_dbg_orphan rpc_es_dbg_orphan
#define rpc_e_dbg_cn_state rpc_es_dbg_cn_state
#define rpc_e_dbg_cn_pkt rpc_es_dbg_cn_pkt
#define rpc_e_dbg_pkt_quotas rpc_es_dbg_pkt_quotas
#define rpc_e_dbg_auth rpc_es_dbg_auth
#define rpc_e_dbg_source rpc_es_dbg_source
#define rpc_e_dbg_pkt_quota_size rpc_es_dbg_pkt_quota_size
#define rpc_e_dbg_stats rpc_es_dbg_stats
#define rpc_e_dbg_mem rpc_es_dbg_mem
#define rpc_e_dbg_mem_type rpc_es_dbg_mem_type
#define rpc_e_dbg_dg_pktlog rpc_es_dbg_dg_pktlog
#define rpc_e_dbg_thread_id rpc_es_dbg_thread_id
#define rpc_e_dbg_timer rpc_es_dbg_timestamp
#define rpc_e_dbg_timestamp rpc_es_dbg_timestamp
#define rpc_e_dbg_cn_errors rpc_es_dbg_cn_errors
#define rpc_e_dbg_conv_thread rpc_es_dbg_conv_thread
#define rpc_e_dbg_pid rpc_es_dbg_pid
#define rpc_e_dbg_atfork rpc_es_dbg_atfork
#define rpc_e_dbg_cma_thread rpc_es_dbg_cma_thread
#define rpc_e_dbg_inherit rpc_es_dbg_inherit
#define rpc_e_dbg_dg_sockets rpc_es_dbg_dg_sockets
#define rpc_e_dbg_ip_max_tsdu rpc_es_dbg_ip_max_tsdu
#define rpc_e_dbg_dg_max_psock rpc_es_dbg_dg_max_psock
#define rpc_e_dbg_dg_max_window_size \
    rpc_es_dbg_dg_max_window_size
#define rpc_e_dbg_threads rpc_es_dbg_threads
#define rpc_e_dbg_last_switch rpc_es_dbg_last_switch

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
#define RPC_DBG2(switch, level) \
         (rpc_g_dbg_switches[(int) (switch)] >= (level))

#define RPC_DBG_PRINTF(switch, level, pargs) \
( \
    ! RPC_DBG2((switch), (level)) ? \
        0 : (rpc__printf pargs, rpc__print_source(__FILE__, __LINE__), 0) \
)
#endif /* DEBUG */

#define EPRINTF           printf

#else /* KERNEL */

#define RPC_DCE_SVC_PRINTF(args) dce_svc_printf args


/*
 * Map the old debug switches to the
 * new serviceability "sub-components".
 */

#define	rpc_e_dbg_general	rpc_svc_general
#define	rpc_e_dbg_mutex		rpc_svc_mutex
#define	rpc_e_dbg_xmit		rpc_svc_xmit
#define	rpc_e_dbg_recv		rpc_svc_recv
#define	rpc_e_dbg_dg_state	rpc_svc_dg_state
#define	rpc_e_dbg_cancel	rpc_svc_cancel
#define	rpc_e_dbg_orphan	rpc_svc_orphan
#define	rpc_e_dbg_cn_state	rpc_svc_cn_state
#define	rpc_e_dbg_cn_pkt	rpc_svc_cn_pkt
#define	rpc_e_dbg_pkt_quotas	rpc_svc_pkt_quotas
#define	rpc_e_dbg_auth		rpc_svc_auth
#define	rpc_e_dbg_stats		rpc_svc_stats
#define	rpc_e_dbg_mem		rpc_svc_mem
#define	rpc_e_dbg_dg_pktlog	rpc_svc_dg_pktlog
#define	rpc_e_dbg_conv_thread	rpc_svc_conv_thread
#define	rpc_e_dbg_atfork	rpc_svc_atfork
#define	rpc_e_dbg_dg_sockets	rpc_svc_dg_sockets
#define	rpc_e_dbg_timer		rpc_svc_timer
#define	rpc_e_dbg_threads	rpc_svc_threads


#define EPRINTF           	rpc__svc_eprintf

/*
 * Buffer size for messages
 */
#define	RPC__SVC_MSG_SZ	300

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))

/*
 * Map "old" RPC debug levels to new serviceability
 * debug levels.
 */
#define	RPC__SVC_DBG_LEVEL(level) \
    ((unsigned32)(level > 9 ? 9 : (level < 1 ? 1 : level)))

/*
 * Buffer size for debug messages
 */
#define	RPC__SVC_DBG_MSG_SZ	RPC__SVC_MSG_SZ

#define	RPC_DBG2(switch,level) \
    DCE_SVC_DEBUG_ATLEAST(RPC__SVC_HANDLE,switch,RPC__SVC_DBG_LEVEL(level))

/*
 * R P C _ D B G _ P R I N T F
 *
 * A macro that prints debug info based on a debug switch's level.  Note
 * that this macro is intended to be used as follows:
 *
 *      RPC_DBG_PRINTF(rpc_e_dbg_xmit, 3, ("Sent pkt %d", pkt_count));
 *
 * I.e. the third parameter is the argument list to "printf" and must be
 * enclosed in parens.  The macro is designed this way to allow us to 
 * eliminate all debug code when DEBUG is not defined.
 *
 */

/*
 * Recoded to use serviceability debug interface ...
 *
 * Call a function that can deal with the "pargs"
 * argument and then call DCE_SVC_DEBUG ... 
 *
 * rpc__svc_fmt_dbg_msg returns a pointer to malloc()'ed
 * storage, since trying to use internal allocation
 * interfaces could lead to infinte recursion.  Finite
 * recursion (due to pargs containing a call to a function
 * that calls RPC_DBG_PRINTF) prevents an implementation
 * that uses a static buffer protected by a mutex.
 *
 * Storage is deallocated with free().
 */

#define RPC_DBG_PRINTF(switch, level, pargs)						\
    if( DCE_SVC_DEBUG_ATLEAST(RPC__SVC_HANDLE,switch,RPC__SVC_DBG_LEVEL(level)) )	\
    {											\
        char *__mptr = rpc__svc_fmt_dbg_msg pargs ;					\
        DCE_SVC_DEBUG((RPC__SVC_HANDLE, switch, RPC__SVC_DBG_LEVEL(level), __mptr));	\
        free(__mptr);									\
    }

#endif	/* DEBUG */

/*
 * R P C _ _ S V C _ E P R I N T F
 *
 * Format arguments and print as a serviceability
 * debug message.  (This routine is what EPRINTF
 * now evaluates to.)
 *
 * Since we need stdarg and all DCE code is now
 * supposed to be ANSI, no need for _DCE_PROTOTYPE_.
 */

#include <stdarg.h>

int rpc__svc_eprintf (
	char          * /*fmt*/,
	...
    );

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
/*
 * R P C _ _ S V C _ F M T _ D B G _ MSG
 */

/*
 * Requires ANSI prototype for stdargs, so no
 * point in using _DCE_PROTOTYPE_ ... DCE is
 * supposed to be pure ANSI now anyway ...
 *
 * Called only by RPC_DBG_PRINTF macro.
 */

#include <stdarg.h>

char * rpc__svc_fmt_dbg_msg (
	char          * /*fmt*/,
	...
    );

#endif	/* DEBUG */

#endif /* KERNEL */

#if !defined(SGI_RPC_DEBUG) && !defined(DEBUG)

#define RPC_DBG_PRINTF(switch, level, pargs)	((void)0)
#define RPC_DBG2(switch, level) 		((void)0)

#endif /* DEBUG */

/*
 * Handle name defined in rpc.sams ...
 */
#define	RPC__SVC_HANDLE		rpc_g_svc_handle

#ifndef DIE
#  define DIE(text)         rpc__die(text, __FILE__, __LINE__)
#endif /* DIE */

#endif	/* DCE_RPC_SVC */

#endif /* _RPCSVC_H */
