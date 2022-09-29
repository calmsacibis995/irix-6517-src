/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dgglob.h,v $
 * Revision 65.1  1997/10/20 19:16:56  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.308.2  1996/02/18  22:56:11  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:19  marty]
 *
 * Revision 1.1.308.1  1995/12/08  00:19:50  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:01  root]
 * 
 * Revision 1.1.306.1  1994/01/21  22:36:56  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:01  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:13  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:15  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:48:15  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:01  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:27  devrcs
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
**  NAME:
**
**      dgglob.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  DG-specific runtime global variable (external) declarations.
**
**
*/

#ifndef _DGGLOB_H
#define _DGGLOB_H

/* ========================================================================= */

/*
 * Client connection table (CCT).
 */
EXTERNAL rpc_dg_cct_t rpc_g_dg_cct;

/*
 * Client call control block table (CCALLT).
 */
EXTERNAL rpc_dg_ccall_p_t rpc_g_dg_ccallt[];

/*
 * Server connection table (SCT)
 */
EXTERNAL rpc_dg_sct_elt_p_t rpc_g_dg_sct[];

#ifndef NO_STATS

/*
 * RPC statistics
 */
EXTERNAL rpc_dg_stats_t rpc_g_dg_stats;

#endif /* NO_STATS */

/*
 * The server boot time 
 */
EXTERNAL unsigned32 rpc_g_dg_server_boot_time;

/*
 * The following UUID will be used to uniquely identify a particular instance
 * of a client process.  It is periodically sent to all servers with which we
 * need to maintain liveness.
 */

EXTERNAL uuid_t rpc_g_dg_my_cas_uuid;

/*
 * The DG socket pool
 */  
EXTERNAL rpc_dg_sock_pool_t rpc_g_dg_sock_pool;

#endif /* _DGGLOB_H */
