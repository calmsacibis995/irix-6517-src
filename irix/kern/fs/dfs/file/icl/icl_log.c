/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: icl_log.c,v 65.6 1998/05/06 22:36:58 bdr Exp $";
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
 * $Log: icl_log.c,v $
 * Revision 65.6  1998/05/06 22:36:58  bdr
 * Fix icl tracing for 64 bit systems.  For ICL_TYPE_LONG types we were
 * treating a value as a pointer instead of the actual data.  Thus when
 * somebody tried to trace a zero value we attempted to deref 0 and
 * we would panic.  Also safeguard a shift by casting to an unsigned
 * so that we don't sign extend.
 *
 * Revision 65.5  1998/03/24  17:20:12  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.4  1998/03/23  16:25:35  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/01/20 16:46:55  lmc
 * Initial merge of icl tracing for kudzu.  This picks up the support for
 * tracing 64 bit kernels.  There are still some compile errors because
 * it uses AFS_hgethi/AFS_hgetlo which isn't done yet.
 *
 * Revision 65.2  1997/11/06  19:58:06  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:40  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.58.1  1996/10/02  17:52:14  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:54  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1993 Transarc Corporation - All rights reserved. */

/* This file contains common subroutines used for managing the
 * logs, and, in particular, individual log records.
 */

#include <icl.h>	/* includes standard stuff */
#include <icl_errs.h>	/* for timestamp message */
#ifndef KERNEL
#include <stdio.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/icl/RCS/icl_log.c,v 65.6 1998/05/06 22:36:58 bdr Exp $")

struct icl_log *icl_allLogs = 0;
unsigned icl_maxLogSize = (4 * 1024*1024); /* 1M words */
#define MINLOGSIZE 256			/* maximum-size record */

#if ICL_MEASURE_LOG
struct icl_log_times icl_log_times;
#endif

/* function to purge records from the start of the log, until there
 * is at least minSpace long's worth of space available without
 * making the head and the tail point to the same word.
 *
 * Log will be write-locked if LOGF_NOLOCK is set.
 */
#ifdef SGIMIPS
static void
GetLogSpace(struct icl_log *logp, signed32 minSpace)
#else
static void
GetLogSpace(struct icl_log *logp, long minSpace)
#endif
{
    unsigned int tsize;

    if (logp->states & ICL_LOGF_NOLOCK) {
	/* Our caller didn't lock the log, but we had better. */
	osi_mutex_enter(&logp->lock);
    }

    osi_assert(logp->logSize > minSpace);
    while (logp->logSize - logp->logElements <= minSpace) {
	/* eat a record */
	tsize = ((logp->datap[logp->firstUsed]) >> 24) & 0xff;
	if (tsize == 0) {
	    /* Clear the log, else we'll be in an infinite loop here. */
	    printf("icl: Internal error in ICL; clearing log %s at %d, %d\n",
		   (logp->name ? logp->name : "<no name>"),
		   logp->firstUsed, logp->logElements);
	    logp->firstUsed = logp->firstFree = 0;
	    logp->logElements = 0;
	    break;
	}
	logp->logElements -= tsize;
	logp->firstUsed += tsize;
	if (logp->firstUsed >= logp->logSize)
	    logp->firstUsed -= logp->logSize;
	logp->baseCookie += tsize;
    }

    if (logp->states & ICL_LOGF_NOLOCK) {
	/* Our caller didn't lock the log, but we better. */
	osi_mutex_exit(&logp->lock);
    }
}

/* add a long to the log, ignoring overflow (checked already) */
#define APPENDLONG(lp, x) \
    MACRO_BEGIN \
        (lp)->datap[(lp)->firstFree] = (x); \
	if (++((lp)->firstFree) >= (lp)->logSize) { \
		(lp)->firstFree = 0; \
	} \
        (lp)->logElements++; \
    MACRO_END
/* define a procedure version of the same function */

static void AppendLong (struct icl_log *lp, long x)
{
    APPENDLONG(lp,x);
}

/* AppendString -- Appends a to the log, including its terminating null char.
 *
 * PARAMETERS -- cursize gives the number of words this record already
 *     occupies.  The whole record must fit in 255 words so we reserve 16 words
 *     for the other parameters and allow the string to use the rest.  If it is
 *     too long to fit in the remaining space we just substitute a short
 *     constant error message.
 *
 * LOCKS USED -- log lock must be held.
 */
static long AppendString(struct icl_log *logp, int cursize, char *astr)
{
    char *op;				/* ptr to char to write */
    char *last;				/* ptr to first char beyond log end */
    int tc;
    int len, rsize;			/* string's length in bytes, words */

    len = strlen(astr)+1;
    if (len > (255 - cursize - 16)*4) {
	astr = "string too long";
	len = 16;
    }
    rsize = (len+3)>>2;			/* number of words we'll need */
    if (logp->logSize - logp->logElements <= rsize+20)
	GetLogSpace(logp, rsize+20);

    op = (char *) &(logp->datap[logp->firstFree]);
    last = (char *) &(logp->datap[logp->logSize]);
    while (1) {
	tc = *astr++;
	*op = tc;
	if (tc == 0)
	    break;
	if (++op == last)
	    op = (char *) &(logp->datap[0]);
    }
    logp->logElements += rsize;
    logp->firstFree += rsize;
    if (logp->firstFree >= logp->logSize)
	logp->firstFree -= logp->logSize;
    return rsize;
}

#define APPENDHEADER(logp,rsize,types,event,timestamp) \
    (AppendLong(logp, ((rsize)<<24) + (types)), \
     AppendLong(logp, event), \
     AppendLong(logp, 0), \
     AppendLong(logp, timestamp))

static void WriteTimestamp (struct icl_log *logp, struct timeval *tv)
{
#ifdef SGIMIPS
    signed32 timestamp = (signed32)((tv->tv_sec & 0x3ff) * 1000000 + 
							tv->tv_usec);
#else
    long timestamp = (tv->tv_sec & 0x3ff) * 1000000 + tv->tv_usec;
#endif

    /* the timer has wrapped -- write a timestamp record
     * corresponding to the time at the beginning of the interval
     */
    if (logp->logSize - logp->logElements <= 5)
	GetLogSpace(logp, 5);

    APPENDHEADER(logp, 5, (ICL_TYPE_UNIXDATE<<18),
		 ICL_INFO_TIMESTAMP, timestamp);
#ifdef SGIMIPS
    AppendLong(logp, tv->tv_sec);
#else
    AppendLong(logp, tv->tv_sec & ~0x3ff);
#endif

    logp->lastTS = tv->tv_sec;

#if ICL_MEASURE_LOG
#define FOURLONGS \
    ((ICL_TYPE_LONG<<18)+(ICL_TYPE_LONG<<12)+(ICL_TYPE_LONG<<6)+ICL_TYPE_LONG)
#define TWOLONGS \
    ((ICL_TYPE_LONG<<18)+(ICL_TYPE_LONG<<12))

    if (icl_log_times.records > 0) {
#ifdef SGIMIPS
	uint v;
#else
	u_long v;
#endif
	int i;

	if (logp->logSize - logp->logElements <= 8+ICL_LOG_TIME_NBINS*6)
	    GetLogSpace(logp, 8+ICL_LOG_TIME_NBINS*6);

	APPENDHEADER(logp, 8, FOURLONGS, ICL_INFO_LOGRECORDS, timestamp);
	AppendLong (logp, icl_log_times.records);
	AppendLong (logp, icl_log_times.time);
	AppendLong (logp, icl_log_times.min);
	AppendLong (logp, icl_log_times.max);

	v = 2;
	i = 0;
	while (1) {
	    APPENDHEADER(logp, 6, TWOLONGS, ICL_INFO_LOGBIN, timestamp);
	    AppendLong (logp, icl_log_times.bin[i]);
	    AppendLong (logp, v);
	    if (++i >= ICL_LOG_TIME_NBINS-1) break;
	    v += (v>>1);
	}
	APPENDHEADER(logp, 6, TWOLONGS, ICL_INFO_LOGOVERFLOW, timestamp);
	AppendLong (logp, icl_log_times.bin[i]);
	AppendLong (logp, v);
    }
#endif
}

/* icl_AppendRecord -- appends a record to the log.  Written for speed since we
 *     know that we're going to have to make this work fast pretty soon,
 *     anyway.  The log must not be locked.  This is called only from
 *     icl_Event<N>, not from the "outside".
 */

void icl_AppendRecord(
  struct icl_log *logp,
  long op,
  int tid,
  struct timeval *tv,
  long types,
  long parm[])
{
    int rsize;				/* record size in longs */
    long first;				/* index for first word of record */
    long timeStamp;
    long i;
    int type[5], t;
    long p;

    if (!logp->datap)
	return;

    if (!(logp->states & ICL_LOGF_NOLOCK)) {
	/* If this is an unshared log so don't do any locking */
	osi_mutex_enter(&logp->lock);
    }

    if (logp->logSize - logp->logElements <= 20) {
	/* Make room for max size record (4 fid parameters) that does use any
         * strings.  We'll make room for strings later if necessary.  Also
         * clear space for several records to avoid doing this on every call */
	GetLogSpace(logp, MIN(2000,logp->logSize>>4));
    }

    /* get timestamp as # of microseconds since some time that doesn't
     * change that often.  This algorithm ticks over every 20 minutes
     * or so (1000 seconds).  Write a timestamp record if it has.
     */
    if (tv->tv_sec - logp->lastTS > 1024)
	WriteTimestamp(logp, tv);

    /* Now open code the insertion of the four word header, leaving the space
     * for the first word.  We'll fill it in last after we figure the total
     * size. */

    first = logp->firstFree;
#ifdef SGIMIPS
    /* 
     * I don't think 'types' is ever negative.
     * However do the shift unsigned so we
     * don't sign extend it and get into trouble.
     */
    type[0] = ((unsigned int)types>>18);
    type[1] = ((int)types>>12) & 0x3f;
    type[2] = ((int)types>>6) & 0x3f;
    type[3] = (int)types & 0x3f;
#else
    type[0] = (types>>18);
    type[1] = (types>>12) & 0x3f;
    type[2] = (types>>6) & 0x3f;
    type[3] = types & 0x3f;
#endif
    type[4] = 0;			/* force terminiation */

    rsize = 4;				/* base case */
    timeStamp = (tv->tv_sec & 0x3ff) * 1000000 + tv->tv_usec;

    if (logp->logSize-logp->firstFree > 20) {
#ifdef SGIMIPS
	/* Everything is worked with as 32bit integers regardless
		of the actual size (32 or 64) */
	int *datap = (int *)&logp->datap[logp->firstFree];
#else
	long *datap = &logp->datap[logp->firstFree];
#endif
	/* skip first word for now */
	datap[1] = op;
	datap[2] = tid;
	datap[3] = timeStamp;

	for (i=0; (p=parm[i],t=type[i]); i++) {
#ifdef SGIMIPS
	    if (t <= ICL_TYPE_POINTER) {	/* Pointer or Long types */
#ifdef SGIMIPS64
            	datap[rsize++]=AFS_hgethi(p);
            	datap[rsize++]=AFS_hgetlo(p);
#else
		datap[rsize++] = p;
#endif
	    } 
	    else if (t == ICL_TYPE_UNIXDATE) {
		datap[rsize++] = p;
#else
	    if ((t <= ICL_TYPE_POINTER /* | LONG */) ||
		(t == ICL_TYPE_UNIXDATE)) {
		datap[rsize++] = p;
#endif
	    } else if (t == ICL_TYPE_HYPER) {
		datap[rsize++] = AFS_hgethi(*(afs_hyper_t *)p);
		datap[rsize++] = AFS_hgetlo(*(afs_hyper_t *)p);
	    }
	    else if (t == ICL_TYPE_FID) {
#ifdef SGIMIPS
		datap[rsize++] = ((unsigned32 *)p)[1];
		datap[rsize++] = ((unsigned32 *)p)[3];
		datap[rsize++] = ((unsigned32 *)p)[4];
		datap[rsize++] = ((unsigned32 *)p)[5];
#else
		datap[rsize++] = ((long *)p)[1];
		datap[rsize++] = ((long *)p)[3];
		datap[rsize++] = ((long *)p)[4];
		datap[rsize++] = ((long *)p)[5];
#endif
	    }
	    else if (t == ICL_TYPE_UUID) {
#ifdef SGIMIPS
		datap[rsize++] = ((unsigned32 *)p)[0];
		datap[rsize++] = ((unsigned32 *)p)[1];
		datap[rsize++] = ((unsigned32 *)p)[2];
		datap[rsize++] = ((unsigned32 *)p)[3];
#else
		datap[rsize++] = ((long *)p)[0];
		datap[rsize++] = ((long *)p)[1];
		datap[rsize++] = ((long *)p)[2];
		datap[rsize++] = ((long *)p)[3];
#endif
	    }
	    else /* (t == ICL_TYPE_STRING) */ {
		/* Too hard so punt to general case code */
		logp->firstFree += rsize;
		logp->logElements += rsize;
		rsize += AppendString(logp, rsize, (char *) p);
		goto maybe_finish_wrapping;
	    }
	}
	logp->datap[logp->firstFree] = (rsize<<24) + types;
	logp->firstFree += rsize;
	logp->logElements += rsize;
	if (!(logp->states & ICL_LOGF_NOLOCK)) {
	    osi_mutex_exit(&logp->lock);
	}
	return;
    }

maybe_near_wrapping:
    AppendLong(logp, 0);		/* we'll rewrite this later */
    AppendLong(logp, op);
    AppendLong(logp, tid);
    AppendLong(logp, timeStamp);

    for (i=0; (p=parm[i],t=type[i]); i++) {
	if ((t <= ICL_TYPE_POINTER /* | LONG */) || (t == ICL_TYPE_UNIXDATE)) {
	    AppendLong(logp, p);
	    rsize++;
	} else if (t == ICL_TYPE_HYPER) {
	    AppendLong(logp, AFS_hgethi(*(afs_hyper_t *)p));
	    AppendLong(logp, AFS_hgetlo(*(afs_hyper_t *)p));
	    rsize += 2;
	}
	else if (t == ICL_TYPE_FID) {
	    AppendLong(logp, ((long *)p)[1]);
	    AppendLong(logp, ((long *)p)[3]);
	    AppendLong(logp, ((long *)p)[4]);
	    AppendLong(logp, ((long *)p)[5]);
	    rsize += 4;
	}
	else if (t == ICL_TYPE_UUID) {
	    AppendLong(logp, ((long *)p)[0]);
	    AppendLong(logp, ((long *)p)[1]);
	    AppendLong(logp, ((long *)p)[2]);
	    AppendLong(logp, ((long *)p)[3]);
	    rsize += 4;
	}
	else /* (t == ICL_TYPE_STRING) */ {
	    rsize += AppendString(logp, rsize, (char *) p);
	}
maybe_finish_wrapping:;
    }
    afsl_Assert (rsize < MINLOGSIZE);	/* XXX -- asserting is too strong? */
    logp->datap[first] = (rsize<<24) + types;
    if (first > logp->firstFree) {
	/* The log has wrapped so put in new a timestamp. */
	WriteTimestamp(logp, tv);
    }

    if (!(logp->states & ICL_LOGF_NOLOCK)) {
	osi_mutex_exit(&logp->lock);
    }
}

/* icl_CreateLog -- creates a log named "name" with size logSize; returning it
 *     in *outLogpp. */

int
icl_CreateLog(
  char *name,
#ifdef SGIMIPS
  signed32 logSize,
#else
  long logSize,
#endif /* SGIMIPS */
  struct icl_log **outLogpp)
{
    icl_MakePreemptionRight();
    icl_CreateLogWithFlags_r(name, logSize, /*flags*/0, outLogpp);
    icl_UnmakePreemptionRight();
    return 0;
}

/* icl_CreateLogWithFlags -- like icl_CreateLog but 'flags' can be set to make
 *     the log unclearable. */

int
icl_CreateLogWithFlags(
  char *name,
#ifdef SGIMIPS
  signed32 logSize,
  unsigned32 flags,
#else
  long logSize,
  unsigned long flags,
#endif
  struct icl_log **outLogpp)
{
    icl_MakePreemptionRight();
    icl_CreateLogWithFlags_r(name, logSize, flags, outLogpp);
    icl_UnmakePreemptionRight();
    return 0;
}

static void FreeLog(struct icl_log *logp)
{
    if (logp->datap)
#ifdef SGIMIPS64
	osi_Free_r(logp->datap, logp->logSize * sizeof(long));
#elif SGIMIPS
	osi_Free_r(logp->datap, logp->logSize * sizeof(signed32));
#else
	osi_Free_r(logp->datap, logp->logSize * sizeof(long));
#endif
    osi_Free_r(logp->name, 1+strlen(logp->name));
    osi_Free_r(logp, sizeof(struct icl_log));
}

static void UnthreadLog(struct icl_log *logp)
{
    struct icl_log **lpp, *tp;

    for (lpp = &icl_allLogs, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == logp) {
	    /* found the dude we want to remove, so unthread it */
	    *lpp = logp->nextp;
	    break;
	}
    }
    afsl_Assert (tp == logp);		/* we found the log we were seeking */
}

void icl_CreateLogWithFlags_r(
  char *name,
#ifdef SGIMIPS
  signed32 logSize,
  unsigned32 flags,
#else
  long logSize,
  unsigned long flags,
#endif
  struct icl_log **outLogpp)
{
    struct icl_log *logp;
    struct icl_log *newlogp;

    if (!icl_inited) icl_Init();

    if (logSize <= 0)
	logSize = ICL_DEFAULT_LOGSIZE;
    else
	logSize = MIN(MAX(logSize, MINLOGSIZE), icl_maxLogSize);

    newlogp = osi_Alloc_r(sizeof(struct icl_log));
    bzero((caddr_t)newlogp, sizeof(*newlogp));
    newlogp->name = osi_Alloc_r(strlen(name)+1);
    strcpy(newlogp->name, name);
    newlogp->datap = NULL;

    /* add into global list under lock */
    osi_mutex_enter(&icl_lock);

    for (logp = icl_allLogs; logp; logp=logp->nextp) {
	if (strcmp(logp->name, name) == 0) {
	    /* found it already created, just return it */
	    logp->refCount++;
	    *outLogpp = logp;
	    if (flags & ICL_CRLOG_FLAG_PERSISTENT) {
		osi_mutex_enter(&logp->lock);
		logp->states |= ICL_LOGF_PERSISTENT;
		osi_mutex_exit(&logp->lock);
	    }
	    osi_mutex_exit(&icl_lock);
	    FreeLog (newlogp);		/* don't need this after all */
	    return;
	}
    }

    logp = newlogp;
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_mutex_init(&logp->lock, "icl_log");
#else
    osi_mutex_init(&logp->lock);
#endif
    logp->refCount = 1;
    logp->logSize = logSize;

    if (flags & ICL_CRLOG_FLAG_PERSISTENT)
	logp->states |= ICL_LOGF_PERSISTENT;

    logp->nextp = icl_allLogs;
    icl_allLogs = logp;
    osi_mutex_exit(&icl_lock);

    *outLogpp = logp;
}

/* called with a log, a pointer to a buffer, the size of the buffer
 * (in *bufSizep), the starting cookie (in *cookiep, use 0 at the start)
 * and returns data in the provided buffer, and returns output flags
 * in *flagsp.  The flag ICL_COPYOUTF_MISSEDSOME is set if we can't
 * find the record with cookie value cookie.
 */
int
icl_CopyOut(
  struct icl_log *logp,
#ifdef SGIMIPS
  signed32 *bufferp,
  signed32 *bufSizep,
  unsigned32 *cookiep,
  signed32 *flagsp)
#else
  long *bufferp,
  long *bufSizep,
  unsigned long *cookiep,
  long *flagsp)
#endif
{
#ifdef SGIMIPS
    signed32 nwords;          /* number of words to copy out */
    unsigned32 startCookie;     /* first cookie to use */
    signed32 outWords;        /* words we've copied out */
    signed32 inWords;         /* max words to copy out */
    signed32 code;            /* return code */
    signed32 ix;              /* index we're copying from */
    signed32 outFlags;        /* return flags */
    signed32 inFlags;         /* flags passed in */
    signed32 end;
#else
    long nwords;		/* number of words to copy out */
    unsigned long startCookie;	/* first cookie to use */
    long outWords;		/* words we've copied out */
    long inWords;		/* max words to copy out */
    long code;			/* return code */
    long ix;			/* index we're copying from */
    long outFlags;		/* return flags */
    long inFlags;		/* flags passed in */
    long end;
#endif

    inWords = *bufSizep;	/* max to copy out */
    outWords = 0;		/* amount copied out */
    startCookie = *cookiep;
    outFlags = 0;
    inFlags = *flagsp;
    code = 0;

    if (!logp->datap)
	goto done;

    icl_MakePreemptionRight();
    osi_mutex_enter(&logp->lock);
    /* first, compute the index of the start cookie we've been passed */
    while (1) {
	/* (re-)compute where we should start */
	if (startCookie < logp->baseCookie) {
	    /* missed some output, skip to the first available record */
	    startCookie = logp->baseCookie;
	    *cookiep = startCookie;
	    outFlags |= ICL_COPYOUTF_MISSEDSOME;
	}

	/* compute where we find the first element to copy out */
	ix = logp->firstUsed + startCookie - logp->baseCookie;
	if (ix >= logp->logSize) ix -= logp->logSize;

	/* if have some data now, break out and process it */
	if (startCookie - logp->baseCookie < logp->logElements) break;

	/* At end of log, so clear it if we need to */
	if (inFlags & ICL_COPYOUTF_CLRAFTERREAD) {
	    logp->firstUsed = logp->firstFree = 0;
	    logp->logElements = 0;
	}
	/* otherwise, either wait for the data to arrive, or return */
	if (!(inFlags & ICL_COPYOUTF_WAITIO)) {
	    code = 0;
	    goto quit;
	} else {

	    /* XXX -- Here we should implement code to wait until log data is
             *     available.  The original attempt at it was hopelessly broken
             *     it was removed. */

	    /* Currently the feature is being disabled by returning an error
	     * if a caller wishes to wait for data to arrive in a log. */

	    code = EINVAL;
	    goto quit;
	}
	/* Should never reach here, with above change to disable waiting for
	 * data */
	/* NOTREACHED */
	/* osi_assert(0); */
    }
    /* copy out data from ix to logSize or firstFree, depending
     * upon whether firstUsed <= firstFree (no wrap) or otherwise.
     * be careful not to copy out more than nwords.
     */
    if (ix >= logp->firstUsed) {
	if (logp->firstUsed <= logp->firstFree)
	    /* no wrapping */
	    end = logp->firstFree;	/* first element not to copy */
	else
	    end = logp->logSize;
	nwords = inWords;	/* don't copy more than this */
	if (end - ix < nwords)
	    nwords = end - ix;
	if (nwords > 0) {
#ifdef SGIMIPS
            bcopy((char *) &logp->datap[ix], (char *) bufferp,
                  sizeof(unsigned32) * nwords);
#else
	    bcopy((char *) &logp->datap[ix], (char *) bufferp,
		  sizeof(long) * nwords);
#endif /* SGIMIPS */
	    outWords += nwords;
	    inWords -= nwords;
	    bufferp += nwords;
	}
	/* if we're going to copy more out below, we'll start here */
	ix = 0;
    }
    /* now, if active part of the log has wrapped, there's more stuff
     * starting at the head of the log.  Copy out more from there.
     */
    if (logp->firstUsed > logp->firstFree
	&& ix < logp->firstFree && inWords > 0) {
	/* (more to) copy out from the wrapped section at the
	 * start of the log.  May get here even if didn't copy any
	 * above, if the cookie points directly into the wrapped section.
	 */
	nwords = inWords;
	if (logp->firstFree - ix < nwords)
	    nwords = logp->firstFree - ix;
#ifdef SGIMIPS
        bcopy((char *) &logp->datap[ix], (char *) bufferp,
              sizeof(unsigned32) * nwords);
#else
	bcopy((char *) &logp->datap[ix], (char *) bufferp,
	      sizeof(long) * nwords);
#endif /* SGIMIPS */
	outWords += nwords;
	inWords -= nwords;
	bufferp += nwords;
    }

  quit:
    osi_mutex_exit(&logp->lock);
    icl_UnmakePreemptionRight();

  done:
    if (code == 0) {
	*bufSizep = outWords;
	*flagsp = outFlags;
    }
    return code;
}

/* return basic parameter information about a log */
int
icl_GetLogParms(
  struct icl_log *logp,
#ifdef SGIMIPS
  signed32 *maxSizep,
  signed32 *curSizep
#else
  long *maxSizep,
  long *curSizep
#endif /* SGIMIPS */
)
{
    osi_mutex_enter(&logp->lock);
    *maxSizep = logp->logSize;
    *curSizep = logp->logElements;
    osi_mutex_exit(&logp->lock);
    return 0;
}


/* hold and release logs */
void icl_LogHold(struct icl_log *logp)
{
    osi_mutex_enter(&icl_lock);
    logp->refCount++;
    osi_mutex_exit(&icl_lock);
}

/* icl_LogUse_r -- keeps track of how many sets refer to the log.  It also
 *     allocated the data area when the number of sets increases from zero to
 *     one.
 *
 * LOCKS USED -- we must allocate the memory for the log without holding the
 *     the icl_lock.  This leads to a fairly complicated logic. */

void icl_LogUse_r(struct icl_log *logp)
{
    int done;
    
    do {
	int logSize = logp->logSize;
	long *datap = NULL;

	if (logp->setCount == 0)
#ifdef SGIMIPS64
	    datap = osi_Alloc_r(sizeof(long) * logSize);
#elif SGIMIPS
	    datap = osi_Alloc_r(sizeof(int) * logSize);
#else
	    datap = osi_Alloc_r(sizeof(long) * logSize);
#endif

	osi_mutex_enter(&logp->lock);
	if (logp->setCount > 0) {

	    /* Just bump the count, the log has already been allocated. */

	    logp->setCount++;
	    afsl_Assert (logp->datap);

	} else if (datap && (logSize == logp->logSize)) {

	    /* This is the first set actually using the log and we've
             * preallocated a log of the correct size. */

	    logp->datap = datap;
	    logp->setCount++;
	    datap = NULL;
	}
	done = (logp->setCount > 0);
	osi_mutex_exit(&logp->lock);

	if (datap) {
	    /* didn't need it or wrong size */
	    osi_Free_r (datap, logSize);
	}
    } while (!done);
}

/* decrement the number of real users of the log, free if possible */
void icl_LogFreeUse_r(struct icl_log *logp)
{
    long *datap = NULL;
    int logSize;

    osi_mutex_enter(&logp->lock);
    osi_assert(logp->setCount);

    if (--logp->setCount == 0) {
	/* no more users -- free log buffer (but keep log header around)*/
	datap = logp->datap;
	logSize = logp->logSize;
	logp->firstUsed = logp->firstFree = 0;
	logp->logElements = 0;
	logp->datap = NULL;
    }
    osi_mutex_exit(&logp->lock);

    if (datap)
#ifdef SGIMIPS64
	osi_Free_r(datap, sizeof(long) * logSize);
#elif SGIMIPS
	osi_Free_r(datap, sizeof(int) * logSize);
#else
	osi_Free_r(datap, sizeof(long) * logSize);
#endif
}

/* set the size of the log to 'logSize' */
int
#ifdef SGIMIPS
icl_LogSetSize(struct icl_log *logp, signed32 logSize)
#else
icl_LogSetSize(struct icl_log *logp, long logSize)
#endif
{
    int done;

    icl_MakePreemptionRight();

    logSize = MAX(MINLOGSIZE, MIN(logSize, icl_maxLogSize));

    do {
	long *alloced = NULL;
	long *freed = NULL;
	int oldSize;

	if (logp->datap && (logSize != logp->logSize))
#ifdef SGIMIPS64
	    alloced = osi_Alloc_r(sizeof(long) * logSize);
#elif SGIMIPS
	    alloced = osi_Alloc_r(sizeof(unsigned32) * logSize);
#else
	    alloced = osi_Alloc_r(sizeof(long) * logSize);
#endif

	osi_mutex_enter(&logp->lock);
	if (logp->datap && (logSize != logp->logSize) && alloced) {

	    /* reset log, free old log buffer and install new one */

	    logp->firstUsed = logp->firstFree = 0;
	    logp->logElements = 0;
	    freed = logp->datap;
	    oldSize = logp->logSize;
	    logp->datap = alloced;
	    logp->logSize = logSize;

	} else {

	    /* give back allocated data, if any, and set size if unallocated */

	    freed = alloced;
	    oldSize = logSize;
	    if (logp->datap == NULL)
		logp->logSize = logSize;
	}
	done = (logSize == logp->logSize);
	osi_mutex_exit(&logp->lock);

	if (freed)
#ifdef SGIMIPS64
	    osi_Free_r(freed, sizeof(long) * oldSize);
#elif SGIMIPS
	    osi_Free_r(freed, sizeof(signed32) * oldSize);
#else
	    osi_Free_r(freed, sizeof(long) * oldSize);
#endif
    } while (!done);

    icl_UnmakePreemptionRight();
    return 0;
}

/* icl_ZapLog_r -- frees a log.  Called with icl_lock locked. */

void icl_ZapLog_r(struct icl_log *logp)
{
    UnthreadLog (logp);
    FreeLog (logp);
}

/* do the release, watching for deleted entries, log already held */
void icl_LogReleNL_r(struct icl_log *logp)
{
    if (--logp->refCount == 0 && (logp->states & ICL_LOGF_DELETED)) {
	icl_ZapLog_r(logp);		/* destroys logp's lock! */
    }
}

/* do the release, watching for deleted entries */
void icl_LogRele_r(struct icl_log *logp)
{
    osi_mutex_enter(&icl_lock);
    icl_LogReleNL_r(logp);
    osi_mutex_exit(&icl_lock);
}

/* do the release, watching for deleted entries */
void icl_LogRele(struct icl_log *logp)
{
    icl_MakePreemptionRight();
    icl_LogRele_r(logp);
    icl_UnmakePreemptionRight();
}

/* zero out the log */
int icl_ZeroLog(struct icl_log *logp)
{
    osi_mutex_enter(&logp->lock);
    logp->firstUsed = logp->firstFree = 0;
    logp->logElements = 0;
    osi_mutex_exit(&logp->lock);
    return 0;
}

/* find a log by name, returning it held */
struct icl_log *
icl_FindLog(char *name)
{
    struct icl_log *tp;
    osi_mutex_enter(&icl_lock);
    for (tp = icl_allLogs; tp; tp=tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    osi_mutex_exit(&icl_lock);
    return tp;
}

int
icl_EnumerateLogs(
  int (*aproc)(char *, char *, struct icl_log *),
  char *arock)
{
    struct icl_log *tp, *next;
    int code = 0;

    osi_mutex_enter(&icl_lock);
    tp = icl_allLogs;
    while (tp != NULL && code == 0) {
	tp->refCount++;	/* hold this guy */
	osi_mutex_exit(&icl_lock);
	code = (*aproc)(tp->name, arock, tp);
	osi_mutex_enter(&icl_lock);
	next = tp->nextp;
	if (--tp->refCount == 0)
	    icl_ZapLog_r(tp);
	tp = next;
    }
    osi_mutex_exit(&icl_lock);
    return code;
}
