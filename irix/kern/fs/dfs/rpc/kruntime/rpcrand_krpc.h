/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcrand_krpc.h,v $
 * Revision 65.1  1997/10/20 19:16:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.58.2  1996/02/18  23:46:49  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:28  marty]
 *
 * Revision 1.1.58.1  1995/12/08  00:15:27  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:30  root]
 * 
 * Revision 1.1.56.1  1994/01/21  22:32:30  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:38  cbrooks]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:53  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:30  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:40:18  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:15  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:56:44  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:51:12  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:15:16  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
#ifndef _RPCRAND_KRPC_H
#define _RPCRAND_KRPC_H	1

/*
**  Copyright (c) 1990 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      rpcrand_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
** 
**  Random number generator abstraction to isolate random number generation
**  routines and allow alternate implementations to be substituted more
**  easily.
**
**  This package provides the following PRIVATE operations:
**  
**      void       RPC_RANDOM_INIT()
**      unsigned32 RPC_RANDOM_GET(lower, upper)
**
**
*/   


/* 
 * R P C _ R A N D O M _ I N I T
 *
 * Used for random number 'seed' routines or any other one time
 * initialization required.
 */

#define RPC_RANDOM_INIT(junk)

/* 
 * R P C _ R A N D O M _ G E T
 *
 * Get a random number in the range lower - upper (inclusive)
 *
 */ 

#define RPC_RANDOM_GET(lower, upper) \
        (((rand()) % (upper - lower + 1)) + lower) 

#endif /* _RPCRAND_KRPC_H */
        
