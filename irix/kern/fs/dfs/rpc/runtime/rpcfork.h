/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcfork.h,v $
 * Revision 65.1  1997/10/20 19:17:10  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.343.2  1996/02/18  22:56:48  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:51  marty]
 *
 * Revision 1.1.343.1  1995/12/08  00:21:55  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  00:00:22  root]
 * 
 * Revision 1.1.341.1  1994/01/21  22:39:01  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:37  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:54:42  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:11:57  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:15:36  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:43:40  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:27  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _RPCFORK_H
#define _RPCFORK_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      rpcfork.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Various macros and data to assist with fork handling.
**
**
*/

/*
 * Define constants to be passed to fork handling routines.  The value passed 
 * indicates at which stage of the fork we are.
 */

#define RPC_C_PREFORK          1
#define RPC_C_POSTFORK_PARENT  2
#define RPC_C_POSTFORK_CHILD   3

typedef unsigned32       rpc_fork_stage_id_t;

#endif /* RCPFORK_H */
