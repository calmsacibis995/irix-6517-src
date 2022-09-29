/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: icl_event.c,v 65.5 1998/03/23 16:25:37 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: icl_event.c,v $
 * Revision 65.5  1998/03/23 16:25:37  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/01/20 16:46:53  lmc
 * Initial merge of icl tracing for kudzu.  This picks up the support for
 * tracing 64 bit kernels.  There are still some compile errors because
 * it uses AFS_hgethi/AFS_hgetlo which isn't done yet.
 *
 * Revision 65.3  1997/11/06  19:58:07  jdoak
 * Source Identification added.
 * Revision 64.2  1997/06/24  19:43:06  lmc
 * Added support for tracing 64bit kernels.  The dfstrace command is assumed
 * to always be built as N32.  The kernel can be either 32 or 64bit.
 *
 * Revision 65.2  1997/10/24  22:01:07  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 *
 * Revision 65.1  1997/10/20  19:17:40  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.47.1  1996/10/02  17:52:13  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:53  damon]
 *
 * $EndLog$
 */

/* This file contains routines for logging events, given pointers to
 * sets.
 */

#include <icl.h>	/* includes standard stuff */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/icl/RCS/icl_event.c,v 65.5 1998/03/23 16:25:37 gwehrman Exp $")

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

osi_mutex_t icl_lock;

#ifdef AFS_SUNOS5_ENV
	/* inline procedure that returns %g7 */
#define FastThreadp() ((long)threadp())
#else
#define FastThreadp() osi_ThreadUnique()
#endif

#define THREAD_HASH(thread) \
    (((thread)+((thread)>>6)+((thread)>>12)+((thread)>>18)) & 0x3f)

/* LOCKS USED -- ignores the preemption lock assuming that it is safe to block
 *     briefly while holding it. */

int icl_InitThreadData (struct icl_set *setp, long thread, long logSize)
{
    int hi;				/* hash index */

    if (!setp || !setp->tidHT)
	return 0;
    if (!thread)
	thread = FastThreadp();

    osi_mutex_enter(&setp->lock);
    for (hi = THREAD_HASH(thread);
	 thread != setp->tidHT[hi];
	 hi = (hi+1) & ((1<<ICL_LOGPERTHREAD)-1)) {
	if (setp->tidHT[hi] == 0) {
	    if (setp->nThreads < (1<<(ICL_LOGPERTHREAD-2))*3) {

		/* Only fill hash table 3/4 full.  This way there are always at
                 * least a few empty slots and collisions don't get too bad. */

		char name[16];
#ifndef SGIMIPS
		int tid;
#endif
		struct icl_log *logp;

		setp->tid[hi] = (int)osi_ThreadUnique();

		sprintf (name, "thrlog.%u", setp->tid[hi]);
		icl_CreateLogWithFlags_r(name, logSize, 0, &logp);
		icl_LogUse_r(logp);

		/* This is an unshared log so don't do any locking in
                 * AppendRecord. */
		logp->states |= ICL_LOGF_NOLOCK;
		setp->perThreadLog[hi] = logp;
		setp->nThreads++;

		/* Now add thread to HT so that guys without the lock will see
                 * the entry. */
		setp->tidHT[hi] = thread;
	    } else {
		/* punt if hash table is more than 3/4ths full. */
		hi = -1;
	    }
	    break;
	}
    }
    osi_mutex_exit(&setp->lock);
    return hi;
}

/* exported routine: a 4 parameter event */
#ifdef SGIMIPS
int icl_Event4(
  register struct icl_set *setp,
  long eventID,
  long lAndT,
  long p1, long p2, long p3, long p4)
#else
icl_Event4(setp, eventID, lAndT, p1, p2, p3, p4)
  register struct icl_set *setp;
  long eventID;
  long lAndT;
  long p1, p2, p3, p4;
#endif
{
#ifndef SGIMIPS
    register struct icl_log *logp;
#endif
    long mask;
    register int i;
    register long tmask;
    int ix;

    /* if things aren't initialized yet (or the set is inactive), don't panic */
#ifdef SGIMIPS
    if (!ICL_SETACTIVE(setp)) return 0; /* hush up the mongoose compiler */
#else
    if (!ICL_SETACTIVE(setp)) return;
#endif /* SGIMIPS */ 

    mask = lAndT>>24 & 0xff;	/* mask of which logs to log to */
    ix = ICL_EVENTBYTE(eventID);
    osi_assert (ix < setp->nevents);
    if (setp->eventFlags[ix] & ICL_EVENTMASK(eventID)) {
	int tid;
	struct timeval tv;
	long parms[4];
#if ICL_MEASURE_LOG
	int notime = 0;
#endif
	osi_GetTime(&tv);
	parms[0] = p1;
	parms[1] = p2;
	parms[2] = p3;
	parms[3] = p4;

	if (setp->tidHT) {
	    /* Use a log for each thread to avoid set locking. */
	    long thread = FastThreadp();
	    int hi;

	    for (hi = THREAD_HASH(thread);
		 thread != setp->tidHT[hi];
		 hi = (hi+1) & ((1<<ICL_LOGPERTHREAD)-1)) {
		if (setp->tidHT[hi] == 0) {
#if ICL_MEASURE_LOG
		    notime = 1;
#endif
		    hi = icl_InitThreadData(setp, thread, 0/*logsize*/);
		    if (hi == -1)
			goto regular;	/* too many threads */
		    break;
		}
	    }
	    icl_AppendRecord(setp->perThreadLog[hi], eventID,
			     setp->tid[hi], &tv, lAndT & 0xffffff, parms);
	} else {
regular:
	    tid = (int)osi_ThreadUnique();
	    osi_mutex_enter(&setp->lock);
	    for(i=0, tmask = 1; i<ICL_LOGSPERSET; i++, tmask <<= 1) {
		if (mask & tmask) {
		    icl_AppendRecord(setp->logs[i], eventID,
				     tid, &tv, lAndT & 0xffffff, parms);
		}
		mask &= ~tmask;
		if (mask == 0) break;	/* break early */
	    }
	    osi_mutex_exit(&setp->lock);
	}
#if ICL_MEASURE_LOG
	if (setp->tidHT && !notime) {
	    struct timeval tvE;
	    long duration;
	    long v;			/* upper bound of i'th bin */

	    osi_GetTime(&tvE);
	    duration = (tvE.tv_sec-tv.tv_sec)*1000000 +
		(tvE.tv_usec-tv.tv_usec);
	    icl_log_times.records++;
	    icl_log_times.time += duration;
	    if (duration > 1) {		/* gettimeofday takes 2+ msec */
		if ((icl_log_times.min == 0) || (duration < icl_log_times.min))
		    icl_log_times.min = duration;
		icl_log_times.max = MAX(duration,icl_log_times.max);
	    }
	    v = 2;
	    for (i=0,v=2;		/* first bin is <= 2usec */
		 i<ICL_LOG_TIME_NBINS-1;/* skip last bin */
		 i++, v+=(v>>1)) {	/* next bin is 150% of this bin */
		if (duration <= v) {
		    icl_log_times.bin[i]++;
		    break;
		}
	    }
	    if (i==ICL_LOG_TIME_NBINS-1) {
		/* put all greater data points in last bin */
		icl_log_times.bin[i]++;
	    }
	}
#endif
    }
#ifdef SGIMIPS
    return 0;
#endif
}

/* Next 4 routines should be implemented via var-args or something.
 * Whole purpose is to avoid compiler warnings about parameter # mismatches.
 * Otherwise, could call icl_Event4 directly.
 */
#ifdef SGIMIPS
int icl_Event3(
  register struct icl_set *setp,
  long eventID,
  long lAndT,
  long p1, long p2, long p3)
#else
icl_Event3(setp, eventID, lAndT, p1, p2, p3)
  register struct icl_set *setp;
  long eventID;
  long lAndT;
  long p1, p2, p3;
#endif
{
    return icl_Event4(setp, eventID, lAndT, p1, p2, p3, 0);
}

#ifdef SGIMIPS
icl_Event2(
  register struct icl_set *setp,
  long eventID,
  long lAndT,
  long p1, long p2)
#else
icl_Event2(setp, eventID, lAndT, p1, p2)
  register struct icl_set *setp;
  long eventID;
  long lAndT;
  long p1, p2;
#endif
{
    return icl_Event4(setp, eventID, lAndT, p1, p2, 0, 0);
}

#ifdef SGIMIPS
icl_Event1(
  register struct icl_set *setp,
  long eventID,
  long lAndT,
  long p1)
#else
icl_Event1(setp, eventID, lAndT, p1)
  register struct icl_set *setp;
  long eventID;
  long lAndT;
  long p1;
#endif
{
    return icl_Event4(setp, eventID, lAndT, p1, 0, 0, 0);
}

#ifdef SGIMIPS
int icl_Event0(
  register struct icl_set *setp,
  long eventID,
  long lAndT)
#else
icl_Event0(setp, eventID, lAndT)
  register struct icl_set *setp;
  long eventID;
  long lAndT;
#endif
{
    return icl_Event4(setp, eventID, lAndT, 0, 0, 0, 0);
}
