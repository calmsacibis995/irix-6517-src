/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcdbg.h,v $
 * Revision 65.2  1998/03/21 19:05:11  lmc
 * Changed all "#ifdef DEBUG" to use "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)".  This allows RPC debugging to be
 * turned on in the kernel without turning on any kernel
 * debugging.
 *
 * Revision 65.1  1997/10/20  19:17:10  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.742.2  1996/02/18  22:56:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:50  marty]
 *
 * Revision 1.1.742.1  1995/12/08  00:21:53  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  00:00:21  root]
 * 
 * Revision 1.1.740.5  1994/09/15  13:36:23  rsarbo
 * 	restore minimal debug capability in kernel
 * 	[1994/09/15  13:25:58  rsarbo]
 * 
 * Revision 1.1.740.4  1994/05/27  15:36:55  tatsu_s
 * 	Merged up with DCE1_1.
 * 	[1994/05/20  20:57:53  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	Added rpc_e_dbg_ip_max_tsdu, rpc_e_dbg_dg_frag_size,
 * 	rpc_e_dbg_dg_max_psock and rpc_e_dbg_dg_max_window_size.
 * 	[1994/04/29  19:28:52  tatsu_s]
 * 
 * Revision 1.1.740.3  1994/05/19  21:15:00  hopkins
 * 	More serviceability.
 * 	[1994/05/18  21:25:22  hopkins]
 * 
 * 	Merge with DCE1_1.
 * 
 * 	Serviceability:
 * 	  Change names of enums for debug switches,
 * 	  so that rpcsvc.h can map all the rpc_e_*
 * 	  codes to rpc_svc_ subcomponents without
 * 	  having recode all the RPC_DBG_PRINTF calls
 * 	  or change the size of the switch table, etc.
 * 	  References in the code outside of DBG_PRINTF
 * 	  calls will have to be changed.
 * 	[1994/05/03  20:27:33  hopkins]
 * 
 * Revision 1.1.740.2  1994/03/15  20:33:50  markar
 * 	     private client socket mods
 * 	[1994/02/22  18:59:31  markar]
 * 
 * Revision 1.1.740.1  1994/01/21  22:38:59  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:36  cbrooks]
 * 
 * Revision 1.1.6.2  1993/08/26  19:20:16  tatsu_s
 * 	Added rpc_e_dbg_pid for printing the process id.
 * 	Added rpc_e_dbg_atfork for fork_handlers.
 * 	Added rpc_e_dbg_cma_thread (reserved, not used).
 * 	Added rpc_e_dbg_inherit for the child process
 * 	inheriting debug switches.
 * 	[1993/08/24  16:54:20  tatsu_s]
 * 
 * Revision 1.1.2.4  1993/01/03  23:54:40  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:11:53  bbelch]
 * 
 * Revision 1.1.2.3  1992/12/23  21:15:30  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:43:37  zeliff]
 * 
 * Revision 1.1.2.2  1992/12/21  21:32:01  sommerfeld
 * 	Add debug code for conv_thread; make more room for more debug codes.
 * 	[1992/12/03  18:45:59  sommerfeld]
 * 
 * Revision 1.1  1992/01/19  03:11:29  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _RPCDBG_H
#define _RPCDBG_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      rpcdbg.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Various macros and data to assist with debugging related code.
**
**
*/

#ifdef DCE_RPC_SVC
/*
 * Debug has been separated into messaging
 * code and "other" ... messaging now largely
 * implemented by rpcsvc.{c,h}
 */
#  include <rpcsvc.h>
#endif

/*
 * A few macros for dealing with debugging code / printfs.  In general,
 * code is generated only if DEBUG is defined.
 *
 * The model here is that there are a number of debug "switches", each one
 * of which can be set to some debug "level".  The switches are represented
 * as an enumeration.
 */

/*
 * The debug switches.
 */

typedef enum {
    rpc_es_dbg_general = 0,              /*  0 */
    rpc_es_dbg_mutex,                    /*  1 */
    rpc_es_dbg_xmit,                     /*  2 */
    rpc_es_dbg_recv,                     /*  3 */
    rpc_es_dbg_dg_lossy,                 /*  4 */
    rpc_es_dbg_dg_state,                 /*  5 */
    rpc_es_dbg_ip_max_pth_unfrag_tpdu,   /*  6 */
    rpc_es_dbg_ip_max_loc_unfrag_tpdu,   /*  7 */
    rpc_es_dbg_dds_max_pth_unfrag_tpdu,  /*  8 */
    rpc_es_dbg_dds_max_loc_unfrag_tpdu,  /*  9 */
    rpc_es_dbg_dg_rq_qsize,              /* 10 */
    rpc_es_dbg_cancel,                   /* 11 */
    rpc_es_dbg_orphan,                   /* 12 */
    rpc_es_dbg_cn_state,                 /* 13 */
    rpc_es_dbg_cn_pkt,                   /* 14 */
    rpc_es_dbg_pkt_quotas,               /* 15 */
    rpc_es_dbg_auth,                     /* 16 */
    rpc_es_dbg_source,                   /* 17 */
    rpc_es_dbg_pkt_quota_size,           /* 18 */
    rpc_es_dbg_stats,                    /* 19 */
    rpc_es_dbg_mem,                      /* 20 */
    rpc_es_dbg_mem_type,                 /* 21 */
    rpc_es_dbg_dg_pktlog,                /* 22 */
    rpc_es_dbg_thread_id,                /* 23 */
    rpc_es_dbg_timestamp,                /* 24 */
    rpc_es_dbg_cn_errors,                /* 25 */
    rpc_es_dbg_conv_thread,              /* 26 */
    rpc_es_dbg_pid,                      /* 27 */
    rpc_es_dbg_atfork,                   /* 28 */
    rpc_es_dbg_cma_thread,               /* 29 */
    rpc_es_dbg_inherit,                  /* 30 */
    rpc_es_dbg_dg_sockets,               /* 31 */
    rpc_es_dbg_ip_max_tsdu,              /* 32 */
    rpc_es_dbg_dg_max_psock,             /* 33 */
    rpc_es_dbg_dg_max_window_size,       /* 34 */
    rpc_es_dbg_threads,       	 	 /* 35 */

    /* 
     * Add new switches above this comment and adjust the
     * "last_switch" value if necessary.  We keep a few
     * empty slots to allow for easy temporary additions.
     */
    rpc_es_dbg_last_switch       = 40    /* 40 */
} rpc_dbg_switch_t;

#define RPC_DBG_N_SWITCHES (((int) rpc_es_dbg_last_switch) + 1)

#define RPC_C_DBG_SWITCHES      40      /* size of rpc_g_dbg_switches[] */

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG)) 

/*
 * Debug table
 *
 * A vector of "debug levels", one level per "debug switch".
 */
EXTERNAL unsigned8 rpc_g_dbg_switches[];

#endif


/*
 * R P C _ D B G
 *
 * Tests whether a particular debug switch is set at a particular level (or
 * higher).
 */
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG)) 

#define RPC_DBG(switch, level) (rpc_g_dbg_switches[(int) (switch)] >= (level))

#else /* DEBUG */

#define RPC_DBG(switch, level) (0)

#endif /* DEBUG */

/*
 * R P C _ D B G _ E X A C T
 *
 * Tests whether a particular debug switch is set at exactly a particular level
 */
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG)) 

#define RPC_DBG_EXACT(switch, level) (rpc_g_dbg_switches[(int) (switch)] == (level))

#else /* DEBUG */

#define RPC_DBG_EXACT(switch, level) (0)

#endif /* DEBUG */


/*
 * a macro to set *status rpc_s_coding_error
 */
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG)) 

#define CODING_ERROR(status)        *(status) = rpc_s_coding_error

#else /* DEBUG */

#define CODING_ERROR(status)

#endif /* DEBUG */

#ifndef DCE_RPC_SVC
/*
 * R P C _ D B G _ P R I N T F
 *
 * A macro that prints debug info based on a debug switch's level.  Note
 * that this macro is intended to be used as follows:
 *
 *      RPC_DBG_PRINTF(rpc_es_dbg_xmit, 3, ("Sent pkt %d", pkt_count));
 *
 * I.e. the third parameter is the argument list to "printf" and must be
 * enclosed in parens.  The macro is designed this way to allow us to 
 * eliminate all debug code when DEBUG is not defined.
 *
 */
/*
 *  Want a different switch than "DEBUG" because that changes the
 *   sizes of some kernel data structures.  This can be used to
 *   get ONLY rpc messages.
 */
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG)) 

#define RPC_DBG_PRINTF(switch, level, pargs) \
( \
    ! RPC_DBG((switch), (level)) ? \
        0 : (rpc__printf pargs, rpc__print_source(__FILE__, __LINE__), 0) \
)

#else /* DEBUG */

#define RPC_DBG_PRINTF(switch, level, pargs) (0)

#endif /* DEBUG */

#endif	/* !DCE_RPC_SVC */

/*
 * R P C _ D B G _ G P R I N T F
 *
 * A macro on top of RPC_DBG_PRINTF that's used for printing random general
 * debug info.  Sample usage:
 *
 *      RPC_DBG_GPRINTF(("Sent pkt %d", pkt_count));
 *
 */

#define RPC_DBG_GPRINTF(pargs) \
    RPC_DBG_PRINTF(rpc_es_dbg_general, 1, pargs)

/*
 * R P C _ _ D B G _ S E T _ S W I T C H E S
 */

PUBLIC void rpc__dbg_set_switches    _DCE_PROTOTYPE_ ((
        char            * /*s*/,
        unsigned32      * /*st*/
    ));


#ifndef	DCE_RPC_SVC
/*
 * R P C _ _ P R I N T F
 *
 * Note: This function uses a variable-length argument list. The "right"
 * way to handle this is using the ANSI C notation (listed below under
 * #ifdef STDARG_PRINTF). However, not all of the compilers support this,
 * so it's here just for future reference purposes.
 *
 * An alternative is to use the "varargs" convention (listed below under
 * #ifndef NO_VARARGS_PRINTF). Most compilers support this convention,
 * however you can't use prototypes with this.
 *
 * The next to last choice is to use the "old" notation. In this case
 * also you can't use prototypes.
 *
 * The last choice is to abandon this all together, define NO_RPC_PRINTF
 * and just use "printf" (from commonp.h).
 */
#if defined(VMS) || (defined(ultrix) && defined(vaxc))
#define NO_RPC_PRINTF
#endif

#ifndef NO_RPC_PRINTF

#include <stdarg.h>

PRIVATE int rpc__printf _DCE_PROTOTYPE_ (( char * /*format*/, ...));

#endif /* NO_RPC_PRINTF */

#endif	/* !DCE_RPC_SVC */

/*
 * R P C _ _ D I E
 */

PRIVATE void rpc__die _DCE_PROTOTYPE_ ((
        char            * /*text*/,
        char            * /*file*/,
        int              /*line*/
    ));

/*
 * R P C _ _ U U I D _ S T R I N G
 */

PRIVATE char *rpc__uuid_string _DCE_PROTOTYPE_(( uuid_t */*uuid*/));

/*
 * R P C _ _ P R I N T _ S O U R C E
 */

PRIVATE void rpc__print_source _DCE_PROTOTYPE_((
        char            * /*file*/,
        int             /*line*/
    ));

#endif /* _RPCDBG_H */
