/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: rpcclock.c,v 65.6 1998/04/01 14:17:05 gwehrman Exp $";
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
 * $Log: rpcclock.c,v $
 * Revision 65.6  1998/04/01 14:17:05  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for sys/ktime.h in the kernel.  Replaced calls to
 * 	gettimeofday() with call to nanotime() in the kernel.
 *
 * Revision 65.5  1998/03/23 17:29:23  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/02/26 17:05:44  lmc
 * Added prototyping and casting.
 *
 * Revision 65.3  1998/01/07  17:21:22  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:25  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:09  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.842.1  1996/05/10  13:12:19  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 *
 * 	HP revision /main/DCE_1.2/2  1996/04/18  19:14 UTC  bissen
 * 	unifdef for single threaded client
 * 	[1996/02/29  20:42 UTC  bissen  /main/HPDCE02/bissen_st_rpc/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	HP revision /main/DCE_1.2/1  1996/01/03  19:03 UTC  psn
 * 	Remove Mothra specific code
 * 	[1995/11/16  21:34 UTC  jrr  /main/HPDCE02/jrr_1.2_mothra/1]
 *
 * 	Submitted the fix for CHFts16024/CHFts14943.
 * 	[1995/08/29  19:19 UTC  tatsu_s  /main/HPDCE02/2]
 *
 * 	Fixed DG awaiting_ack_timestamp.
 * 	[1995/04/19  05:10 UTC  tatsu_s  /main/HPDCE02/tatsu_s.cn_sec_fix.b0/1]
 *
 * 	Submitted rfc31.0: Single-threaded DG client and RPC_PREFERRED_PROTSEQ.
 * 	[1994/12/09  19:19 UTC  tatsu_s  /main/HPDCE02/1]
 *
 * 	rfc31.0: Cleanup.
 * 	[1994/12/07  20:59 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/2]
 *
 * 	rfc31.0: Initial version.
 * 	[1994/12/05  17:18 UTC  tatsu_s  /main/tatsu_s.st_dg.b0/1]
 *
 * Revision 1.1.837.1  1994/01/21  22:38:55  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:59:33  cbrooks]
 * 
 * Revision 1.1.7.5  1993/01/03  23:54:35  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:11:42  bbelch]
 * 
 * Revision 1.1.7.4  1992/12/23  21:15:20  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:43:26  zeliff]
 * 
 * Revision 1.1.7.3  1992/12/22  22:24:43  sommerfeld
 * 	Call rpc__clock_update() in rpc__clock_stamp if long_sleep is set.
 * 	(from sandhya at IBM)
 * 	[1992/12/15  20:44:05  sommerfeld]
 * 
 * 	Normalize timespec if tv_nsec is greater than a billion.
 * 	[1992/10/20  20:30:03  sommerfeld]
 * 
 * 	Timer reorg: add rpc__clock_timespec for use by rpctimer.c
 * 	[1992/09/01  22:56:29  sommerfeld]
 * 
 * Revision 1.1.7.2  1992/09/29  20:41:53  devsrc
 * 	SNI/SVR4 merge.  OT 5373
 * 	[1992/09/11  15:45:47  weisman]
 * 
 * Revision 1.1.3.2  1992/05/01  16:39:36  rsalz
 * 	30-mar-92 af        Initialize start_time to {0, 0} instead
 * 	                    of {0L, 0L}.
 * 	[1992/05/01  16:31:05  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:11:26  devrcs
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
**      rpcclock.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Routines for manipulating the time entities understood by the rpc_timer
**  services.
**
**
*/

#include <commonp.h>
#ifdef SGIMIPS
#include <sys/time.h>
#ifdef _KERNEL
#include <sys/ktime.h>
#endif
#endif


/* ========================================================================= */

GLOBAL rpc_clock_t rpc_g_clock_curr;
GLOBAL rpc_clock_unix_t rpc_g_clock_unix_curr;

EXTERNAL boolean32 rpc_g_long_sleep;
EXTERNAL boolean rpc_g_is_single_threaded;

INTERNAL struct timeval  start_time = { 0, 0 };
INTERNAL rpc_clock_t     rpc_clock_skew = 0;

/* ========================================================================= */

/*
 * R P C _ _ C L O C K _ S T A M P
 * 
 * Timestamp a data structure with the current approximate tick count.
 * The tick count returned is only updated by the rpc_timer routines
 * once each time through the select listen loop.  This degree of accuracy
 * should be adequate for the purpose of tracking the age of a data
 * structure.                                          
 */

PRIVATE rpc_clock_t rpc__clock_stamp(void)
{
    if (rpc_g_long_sleep)
	rpc__clock_update();

    return( rpc_g_clock_curr );
}


/*
 * R P C _ _ C L O C K _ A G E D
 *
 * A routine to determine whether a specified timestamp has aged.
 */

PRIVATE boolean rpc__clock_aged
#ifdef _DCE_PROTO_
(
    rpc_clock_t      time,
    rpc_clock_t      interval
)
#else
( time, interval )
rpc_clock_t      time;
rpc_clock_t      interval;
#endif
{
    return( rpc_g_clock_curr >= (time + interval) );
}


/*
 * R P C _ _ C L O C K _ U N I X _ E X P I R E D
 *
 * Determine whether a specified UNIX timestamp is in the past.
 */

PRIVATE boolean rpc__clock_unix_expired 
#ifdef _DCE_PROTO_
(
    rpc_clock_unix_t time
)
#else
( time )
rpc_clock_unix_t time;
#endif
{
    return( rpc_g_clock_unix_curr >= time );
}

/*
 * R P C _ _ C L O C K _ U P D A T E 
 *
 * Update the current tick count.  This routine is the only one that
 * actually makes system calls to obtain the time.
 */

PRIVATE void rpc__clock_update(void)
{

    struct timeval         tp;
    unsigned long          ticks;
    
    /*
     * On startup, just initialize start time.  Arrange for the initial
     * time to be '1' tick (0 can be confusing in some cases).
     */                             
    if (start_time.tv_sec == 0)
    {
#if defined(SGIMIPS) && defined(_KERNEL)
	{
	    struct timespec tv;
            nanotime(&tv);
	    TIMESPEC_TO_TIMEVAL(&tv,&start_time);
	}
#else /* SGIMIPS && _KERNEL */
#if defined(SNI_SVR4) || (defined(SGIMIPS) && (!defined(_BSD_TIME) && !defined(_BSD_COMPAT)))
        gettimeofday(&start_time);
#else
        gettimeofday( &start_time, (struct timezone *) 0 );
#endif /* SNI_SVR4 */
#endif /* SGIMIPS && _KERNEL */
        rpc_g_clock_unix_curr = start_time.tv_sec;
        start_time.tv_usec -= (1000000L/RPC_C_CLOCK_HZ);
        if (start_time.tv_usec < 0)
        {
            start_time.tv_usec += 1000000L;
            start_time.tv_sec --;
        }
        rpc_g_clock_curr = (rpc_clock_t) 1;
    }
    else
    {             
        /*
         * Get the time of day from the system, and convert it to the
         * tick count format we're using (RPC_C_CLOCK_HZ ticks per second).
         * For now, just use 1 second accuracy.
         */
#if defined(SGIMIPS) && defined(_KERNEL)
	{
	    struct timespec tv;
            nanotime(&tv);
	    TIMESPEC_TO_TIMEVAL(&tv,&tp);
	}
#else /* SGIMIPS && _KERNEL */
#if defined(SNI_SVR4) || (defined(SGIMIPS) && (!defined(_BSD_TIME) && !defined(_BSD_COMPAT)))
        gettimeofday(&tp);
#else
        gettimeofday(&tp, (struct timezone *) 0 );
#endif /* SNI_SVR4 */
#endif /* SGIMIPS && _KERNEL */
        rpc_g_clock_unix_curr = tp.tv_sec;      

        ticks = (tp.tv_sec - start_time.tv_sec) * RPC_C_CLOCK_HZ +
                 rpc_clock_skew;

        if (tp.tv_usec < start_time.tv_usec)
        {
            ticks -= RPC_C_CLOCK_HZ;
            tp.tv_usec += 1000000L;
        }
        ticks += (tp.tv_usec - start_time.tv_usec) / (1000000L/RPC_C_CLOCK_HZ);
           
	/*
	 * If we are single-threaded, there is no guarantee that we will be
	 * called within 1 second in the short sleep mode.
	 */
        if (rpc_g_is_single_threaded && !rpc_g_long_sleep &&
	    ticks > (rpc_g_clock_curr + RPC_CLOCK_SEC(1)))
	    rpc_g_long_sleep = true;
        /*
         * We need to watch out for changes to the system time after we
         * have stored away our starting time value.  This situation is 
         * handled by maintaining a static variable containing the amount of
         * RPC_C_CLOCK_HZ's we believe that the current time has been altered.
         * This variable gets updated each time we detect that the system time
         * has been modified.
         *
         * This scheme takes into account the fact that there are data 
         * structures that have been timestamped with tick counts;  it is 
         * not possible to simply start the tick count over, of to just
         * update the trigger counts in the list of periodic funtions.
         *
         * We determine that the system time has been modified if 1) the
         * tick count has gone backward, or 2) if we notice that we haven't
         * been called in 60 seconds.  This last condition is based on the
         * assumption that the listen loop will never intentionally wait
         * that long before calling us. It would also be possible (tho more
         * complicated) to look at the trigger count of the next periodic
         * routine and assume that if we've gone a couple of seconds past
         * that then something's wrong.
         */
	/*
	 * If we are single-threaded, there is no guarantee that we will be
	 * called at least every 60 seconds. Therefore, we can not detect the
	 * system clock's advance.
	 */
        if ( (ticks < rpc_g_clock_curr) ||
            (!rpc_g_is_single_threaded &&
	     ticks > (rpc_g_clock_curr + RPC_CLOCK_SEC(60))))
        {
            rpc_clock_skew += rpc_g_clock_curr - ticks + RPC_C_CLOCK_HZ;
            rpc_g_clock_curr += RPC_C_CLOCK_HZ;
        }
        else
        {
            rpc_g_clock_curr = (rpc_clock_t) ticks;
        }
    }
}   

/*
 * R P C _ _ C L O C K _ T I M E S P E C
 * 
 * Return a "struct timespec" equivalent to a given rpc_clock_t.
 */

PRIVATE void rpc__clock_timespec 
#ifdef _DCE_PROTO_
(
        rpc_clock_t clock,
        struct timespec *ts
)
#else
(clock, ts)
    rpc_clock_t clock;
    struct timespec *ts;
#endif
{
    clock -= rpc_clock_skew;
    ts->tv_sec = start_time.tv_sec + (clock / RPC_C_CLOCK_HZ);
    ts->tv_nsec = (1000 * start_time.tv_usec) + 
        (clock % RPC_C_CLOCK_HZ) * (1000000000 / RPC_C_CLOCK_HZ);
    if (ts->tv_nsec >= 1000000000) 
    {
	ts->tv_nsec -= 1000000000;
	ts->tv_sec += 1;
    }
}
