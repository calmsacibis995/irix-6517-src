/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpctimer.h,v $
 * Revision 65.1  1997/10/20 19:17:12  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.316.1  1996/05/10  13:12:59  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:14 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:43 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:04 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:35 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/4]
 *
 * 	Fixed the rpcclock skew and slow pipe timeout.
 * 	[1995/08/16  20:26 UTC  tatsu_s  /main/HPDCE02/tatsu_s.psock_timeout.b0/1]
 *
 * 	Merge allocation failure detection functionality into Mothra
 * 	[1995/04/02  01:16 UTC  lmm  /main/HPDCE02/3]
 *
 * 	add memory allocation failure detection
 * 	[1995/04/02  00:20 UTC  lmm  /main/HPDCE02/lmm_rpc_alloc_fail_detect/1]
 *
 * 	Submitted the local rpc cleanup.
 * 	[1995/01/31  21:17 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Changed the signature of rpc__timer_itimer_enable().
 * 	[1995/01/27  22:06 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/2]
 *
 * 	Added VTALRM timer.
 * 	[1995/01/19  19:15 UTC  tatsu_s  /main/HPDCE02/tatsu_s.vtalarm.b0/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:19 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/11/30  22:26 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 *
 * Revision 1.1.312.1  1994/01/21  22:39:20  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  22:00:04  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:55:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:12:44  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:16:40  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:44:25  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:53  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _RPCTIMER_H
#define _RPCTIMER_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      rpctimer.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Interface to NCK timer functions.
**  
**  Suggested uasge:  These routines are tailored for the situation
**  in which a monitor routine is needed to keep track of the state
**  of a data structure.  In this case, one of the fields in the data
**  structure should be a rpc_timer_t (defined below).  The user of the
**  timer service would then call the rpc__timer_set routine with the 
**  following arguments:
**       
**      1) the address of the rpc_timer_t variable
**      2) the address of the routine to run
**      3) a pointer_t value to be sent to the routine when run
**      4) the frequency with which the routine should be run
**
**  The pointer value used will most often be the address of the data
**  structure being monitored.  (However, it also possible to declare
**  a global rpc_timer_t to be used to periodically run a routine not
**  associated with any particular data object.  N.B.  It is necessary 
**  that the rpc_timer_t variable exist during the entire time that the
**  periodic routine is registered to run;  that is, don't declare it on
**  the stack if the current routine will return before unregistering the
**  periodic routine.)
**
**  Users of this service should not keep track of time by the frequency
**  with which their periodic routines are run.  Aside from the lack of 
**  accuracy of user space time functions, it is also possible that the
**  system's idea of the current time may be changed at any time.  When 
**  necessary, the routines currently err in favor of running a periodic 
**  routine too early rather than too late.  For this reason, data 
**  struture monitoring should follow this outline:
**
**      struct {
**          .....
**          rpc_clock_t        timestamp;
**          rpc_timer_t        timer;
**          ..... 
**      } foo;                             
**
**      rpc__clock_stamp( &foo.timestamp );          
**
**      rpc__timer_set( &foo.timer, foo_mon, &foo, rpc_timer_sec(1) );
**          ...
**
**      void foo_mon( parg ) 
**      pointer_t parg;
**      {
**          if( rpc__clock_aged( parg->timestamp, rpc_timer_sec( 1 ) )
**          {
**            ...
**          }
**      }
**  
**
**
*/
           
#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

typedef void (*rpc_timer_proc_p_t) _DCE_PROTOTYPE_(( pointer_t ));

/*
 * This type is used to create a list of periodic functions ordered by
 * trigger time.
 */
typedef struct rpc_timer_t
{
    struct rpc_timer_t   *next;
    rpc_clock_t          trigger;
    rpc_clock_t          frequency;
    rpc_timer_proc_p_t   proc;
    pointer_t            parg;                           
} rpc_timer_t, *rpc_timer_p_t;

       
/* 
 * Initialize the timer package.
 */
PRIVATE unsigned32 rpc__timer_init _DCE_PROTOTYPE_((void));

/* 
 * Start the timer thread.
 */
PRIVATE unsigned32 rpc__timer_start _DCE_PROTOTYPE_((void));

       
/* 
 * Timer package fork handling routine
 */
PRIVATE void rpc__timer_fork_handler _DCE_PROTOTYPE_((
    rpc_fork_stage_id_t  /*stage*/
));

/* 
 * Shutdown the timer package.
 */
PRIVATE void rpc__timer_shutdown _DCE_PROTOTYPE_((void));

/* 
 * Register a routine to be run at a specific interval.
 */
PRIVATE void rpc__timer_set _DCE_PROTOTYPE_((
    rpc_timer_p_t /*t*/,
    rpc_timer_proc_p_t /*proc*/,
    pointer_t /*parg*/, 
    rpc_clock_t  /*freq*/
));

/* 
 * Change one or more of the characteristics of a periodic routine.
 */
PRIVATE void rpc__timer_adjust _DCE_PROTOTYPE_((
    rpc_timer_p_t /*t*/,
    rpc_clock_t /*freq*/
));

/*
 * Discontinue running a previously registered periodic routine.
 */
PRIVATE void rpc__timer_clear _DCE_PROTOTYPE_((rpc_timer_p_t /*t*/));

/*
 * Run any periodic routines that are ready.
 */                                          
PRIVATE void rpc__timer_callout _DCE_PROTOTYPE_((void));

/* 
 * Start the itimer if enabled.
 */
PRIVATE void rpc__timer_itimer_start _DCE_PROTOTYPE_((void));

/* 
 * Stop the itimer.
 */
PRIVATE boolean32 rpc__timer_itimer_stop _DCE_PROTOTYPE_((void));

/* 
 * Enable the itimer, but doesn't start it.
 */
PRIVATE boolean32 rpc__timer_itimer_enable _DCE_PROTOTYPE_((void));

/*
 * Forcefully update the current rpcclock's tick count.
 */                                          
PRIVATE void rpc__timer_update_clock _DCE_PROTOTYPE_((void));

#endif /* _RPCTIMER_H */
