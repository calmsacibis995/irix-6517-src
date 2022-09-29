/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcclock.h,v $
 * Revision 65.1  1997/10/20 19:17:09  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.440.2  1996/02/18  22:56:46  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:49  marty]
 *
 * Revision 1.1.440.1  1995/12/08  00:21:51  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:34 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/08/29  19:19 UTC  tatsu_s
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 
 * 	HP revision /main/tatsu_s.psock_timeout.b0/1  1995/08/16  20:26 UTC  tatsu_s
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/12/08  00:00:19  root]
 * 
 * Revision 1.1.438.1  1994/01/21  22:38:56  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:34  cbrooks]
 * 
 * Revision 1.1.3.4  1993/01/03  23:54:36  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:11:46  bbelch]
 * 
 * Revision 1.1.3.3  1992/12/23  21:15:22  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:43:30  zeliff]
 * 
 * Revision 1.1.3.2  1992/12/22  22:25:04  sommerfeld
 * 	Add prototype for rpc__clock_timespec
 * 	[1992/09/01  23:00:22  sommerfeld]
 * 
 * Revision 1.1  1992/01/19  03:08:43  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _RPCCLOCK_H
#define _RPCCLOCK_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      rpcclock.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/* 
 * Number of times per second the clock ticks (LSB weight for time values).
 */

#define  RPC_C_CLOCK_HZ             5      
           
#define  RPC_CLOCK_SEC(sec)         ((sec)*RPC_C_CLOCK_HZ)
#define  RPC_CLOCK_MS(ms)           ((ms)/(1000/RPC_C_CLOCK_HZ))

typedef unsigned32  rpc_clock_t, *rpc_clock_p_t;
                                      
/*
 * An absolute time, UNIX time(2) format (i.e. time since 00:00:00 GMT, 
 * Jan. 1, 1970, measured in seconds.
 */

typedef unsigned32  rpc_clock_unix_t, *rpc_clock_unix_p_t;

/*
 * Macros to help do A < B and A <= B compares on
 * unsigned numbers in rpc_clock_t. These macros are
 * useful when the numbers being compared are allowed to
 * wrap (e.g. we really want 0xfffffffe to be less than
 * 0x00000002).
 *
 * To perform such a comparison, we count the fact that
 * the number space for a given data type / object is
 * large enough that we can assert: if the unsigned
 * difference A - B is "negative", A is < B.  We define
 * "negative" as being a unsigned difference that when
 * converted to a signed type of the same precision
 * yields a negative value.
 */
#define RPC_CLOCK_IS_LT(a, b)	(((signed32) ((a) - (b))) < 0)
#define RPC_CLOCK_IS_LTE(a, b)	(((signed32) ((a) - (b))) <= 0)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get the current approximate tick count.  This routine is used to
 * timestamp data structures.  The tick count returned is only updated
 * by the rpc_timer routines once each time through the select listen
 * loop.  This degree of accuracy should be adequate for the purpose
 * of tracking the age of a data structure.
 */                                          

PRIVATE rpc_clock_t rpc__clock_stamp _DCE_PROTOTYPE_ ((void));

/*
 * A routine to determine whether a specified time interval has passed.
 */
PRIVATE boolean rpc__clock_aged    _DCE_PROTOTYPE_ ((
        rpc_clock_t          /*time*/,
        rpc_clock_t          /*interval*/
    ));

/*
 * Update the current tick count.  This routine is the only one that
 * actually makes system calls to obtain the time, and should only be
 * called from within the rpc_timer routines themselves.  Everyone else
 * should use the  routine rpc_timer_get_current_time which returns an
 * approximate tick count, or rpc_timer_aged which uses the approximate
 * tick count.  The value returned is the current tick count just
 * calculated.
 */
PRIVATE void rpc__clock_update _DCE_PROTOTYPE_ (( void ));

/*
 * Determine if a UNIX absolute time has expired
 * (relative to the system's current time).
 */

PRIVATE boolean rpc__clock_unix_expired _DCE_PROTOTYPE_ ((
        rpc_clock_unix_t    /*time*/
    ));

/*
 * Convert an rpc_clock_t back to a "struct timespec".
 */

PRIVATE void rpc__clock_timespec _DCE_PROTOTYPE_((
	rpc_clock_t  /*clock*/,
	struct timespec * /*ts*/
    ));
      
#ifdef __cplusplus
}
#endif

#endif /* _RPCCLOCK_H */
