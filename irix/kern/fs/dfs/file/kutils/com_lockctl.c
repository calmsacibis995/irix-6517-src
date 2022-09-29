/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: com_lockctl.c,v 65.6 1998/03/23 16:36:13 gwehrman Exp $";
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
 * $Log: com_lockctl.c,v $
 * Revision 65.6  1998/03/23 16:36:13  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.5  1998/03/19 23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.4  1998/03/03  15:14:21  lmc
 * Correct use of l_pid in flock_t to be fl_pid.  Also fixed a prototype.
 *
 * Revision 65.2  1997/11/06  20:00:30  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.55.1  1996/10/02  17:52:53  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:19  damon]
 *
 * Revision 1.1.50.3  1994/07/13  22:28:32  devsrc
 * 	merged with bl-10
 * 	[1994/06/29  11:38:12  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  15:59:45  mbs]
 * 
 * Revision 1.1.50.2  1994/06/09  14:12:02  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:32  annie]
 * 
 * Revision 1.1.50.1  1994/02/04  20:21:26  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:23  devsrc]
 * 
 * Revision 1.1.48.1  1993/12/07  17:27:09  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:40:36  jaffe]
 * 
 * Revision 1.1.3.4  1993/01/29  22:33:15  bolinger
 * 	Fix OT defect 6987: add casts to ensure that comparisons for flock
 * 	adjacency or overlap are always signed, regardless of type of
 * 	comparands.
 * 	[1993/01/29  21:22:19  bolinger]
 * 
 * Revision 1.1.3.3  1993/01/19  16:05:52  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  14:16:01  cjd]
 * 
 * Revision 1.1.3.2  1992/11/24  17:53:58  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:08:35  bolinger]
 * 
 * Revision 1.1  1992/01/19  02:55:26  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 * Copyright (C) 1995, 1990 Transarc Corporation
 * All rights reserved.
 */

/*			    Episode File System
		     Support for Vnode record locking
 */
#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>

#include <dcedfs/stds.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/queue.h>			/* struct squeue */
#include <dcedfs/lock.h>
#include <dcedfs/icl.h>
#include <dcedfs/vnl_trace.h>
#include <com_locks.h>			/* struct record_lock */

#ifdef AFS_AIX31_ENV
#include <sys/user.h>
#endif

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/com_lockctl.c,v 65.6 1998/03/23 16:36:13 gwehrman Exp $")

/* Static data structures */

static int initted = 0;			/* true if module initialized */
static struct squeue lockpoolP;		/* pointer to lockpool */
static osi_dlock_t lockpoollock;	/* protect lockpoolP */
static struct squeue sleeplist;		/* list of sleeping locks */
static osi_dlock_t sleeplock;		/* protect sleeping list */
static struct icl_set *vnl_iclSetp = NULL;
static struct icl_log *vnl_iclLogp = NULL;


/* Bogus hack */

#ifndef KERNEL
extern struct user u;
#endif

/*
 * vnl_init -- initialize record lock structures
 */

int vnl_init(int nrlocks)	/* size of record lock pool */
{
    int i;
    struct record_lock *lockpool;

    if (initted) return 0;
    initted = 1;
    QInit (&lockpoolP);
    lockpool = (struct record_lock *)osi_Alloc (nrlocks *
						sizeof (struct record_lock));
    for (i = 0; i < nrlocks; i++)
	QAdd (&lockpoolP, RLTOQ (&lockpool[i]));
    lock_Init (&lockpoollock);
    QInit (&sleeplist);
    lock_Init (&sleeplock);
#ifdef AFS_OSF11_ENV
#define	DEFAULT_LOGSIZE (2*1024)
#else
#define	DEFAULT_LOGSIZE (60*1024)
#endif
    if (icl_CreateLog("cmfx", DEFAULT_LOGSIZE, &vnl_iclLogp) == 0) {
	(void) icl_CreateSetWithFlags("vnl", vnl_iclLogp, (struct icl_log *) 0,
				      ICL_CRSET_FLAG_DEFAULT_OFF, &vnl_iclSetp);
    }
    return 0;
}

#ifdef AFS_SUNOS5_ENV
/*
 * vnl_ridset -- set process ID of flock to a remote process
 * So far, only Solaris needs this, for its remote-lock daemon.
 */

void vnl_ridset (struct flock *flock)
{
    /*
     * as it turns out, for Solaris, there's nothing to do here, since the
     * lockd daemon will have already filled in the pid and sysid for the
     * remote-lock request.
     */
}
#endif /* AFS_SUNOS5_ENV */

/*
 * vnl_idset -- set process ID of flock to current process
 */

void vnl_idset (struct flock *flock)
{
#ifdef	AFS_AIX31_ENV
    flock->l_sysid = u.u_sysid;
    flock->l_pid = u.u_epid;
#else /* AFS_AIX31_ENV */
#ifdef AFS_SUNOS5_ENV
    flock->l_pid = osi_GetPid();	/* XXX */
    flock->l_sysid = 0;	/* tag this as a local user */
#else /* AFS_SUNOS5_ENV */
#if defined(SGIMIPS) && defined(_KERNEL)
    flock->l_pid = curuthread->ut_flid.fl_pid;
    flock->l_sysid = curuthread->ut_flid.fl_sysid;
#else
    flock->l_pid = osi_GetPid();	/* XXX */
#endif /* SGIMIPS */
#endif /* AFS_SUNOS5_ENV */
#endif /* AFS_AIX31_ENV */
}

/*
 * vnl_idcmp -- compare process ID's of flocks
 *
 * Return 0 if same process, non-zero otherwise
 */

#ifdef SGIMIPS
static
int vnl_idcmp (
    struct flock *flock1,
    struct flock *flock2)
#else
static
int vnl_idcmp (flock1, flock2)
    struct flock *flock1;
    struct flock *flock2;
#endif
{
#if defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV)
    return (flock1->l_sysid != flock2->l_sysid
	      || flock1->l_pid != flock2->l_pid);
#else
    return (flock1->l_pid != flock2->l_pid);
#endif
}

/*
 * vnl_alloc -- get new record lock
 *
 * Returns 0 if one is available, or ENOLCK
 */

int vnl_alloc(
    struct flock *flock,		/* file lock request */
#ifdef SGIMIPS
    afs_hyper_t end,			/* last byte of request interval */
#else  /* SGIMIPS */
    int end,				/* last byte of request interval */
#endif /* SGIMIPS */
    struct record_lock **rlockP)	/* where to put new lock */
{
    struct record_lock *rlock;		/* new lock */

    lock_ObtainWrite (&lockpoollock);
    if (QNext (&lockpoolP) == &lockpoolP) {
	rlock = (struct record_lock *)osi_Alloc (sizeof (struct record_lock));
	if (!rlock) return (ENOLCK);
    } else {
	rlock = QTORL (QNext (&lockpoolP));
	QRemove (RLTOQ (rlock));
    }
    lock_ReleaseWrite (&lockpoollock);
    rlock->data = *flock;
    rlock->data_end = end;
    *rlockP = rlock;
    icl_Trace3(vnl_iclSetp, VNL_TRACE_ALLOC,
	      ICL_TYPE_POINTER, flock,
	      ICL_TYPE_LONG, end,
	      ICL_TYPE_POINTER, rlock);
    return 0;
}

/*
 * vnl_free -- give back record lock
 */

static
void vnl_free (struct record_lock *rlock)
    /* struct record_lock *rlock;		record lock to free */
{
    icl_Trace1(vnl_iclSetp, VNL_TRACE_FREE,
	      ICL_TYPE_POINTER, rlock);
    lock_ObtainWrite (&lockpoollock);
    QAdd (&lockpoolP, RLTOQ (rlock));
    lock_ReleaseWrite (&lockpoollock);
}

/*
 * vnl_test_deadlock -- test for deadlock caused by file locks
 *
 * Hokey deadlock detection -- we keep a list of processes sleeping on
 * file locks.  Before putting a new entry on the list, we see if it
 * will create a cycle, by examining other entries on the list.
 *
 * Returns EDEADLK if would cause deadlock, 0 if would not.
 */
static
int vnl_test_deadlock (struct flock *flock, int end, 
		struct record_lock *blocker)
   /* struct flock *flock;		 file lock request */
   /* int end;				 index of last byte of interval */
   /* struct record_lock *blocker;	 this is putting us to sleep */
{
    struct record_lock *rlock;		/* temp record lock */

    while (1) {
	for (rlock = QTORL (QNext (&sleeplist));
	     rlock != QTORL (&sleeplist);
	     rlock = QTORL (QNext (RLTOQ (rlock))))
	     if (!vnl_idcmp (&rlock->data, &blocker->data)
		  && rlock->un.blocker) {
		blocker = rlock->un.blocker;
		goto testcycle;
	     }
	return 0;
    testcycle:
	if (!vnl_idcmp (&blocker->data, flock))
	    return (EDEADLK);
    }
}

/*
 * vnl_sleep -- sleep on a lock
 *
 * Called with vnode locked for writing.  Must release this lock to sleep,
 * and re-obtain it upon waking up.
 */

int vnl_sleep(
    struct flock *flock,		/* file lock request */
#ifdef SGIMIPS 
    afs_hyper_t end,			/* index of last byte of interval */
#else  /* SGIMIPS */
    int end,				/* index of last byte of interval */
#endif /* SGIMIPS */
    struct record_lock *blocker,	/* this is putting us to sleep */
    osi_dlock_t *vnlock)		/* pointer to vnode lock */
{
    struct record_lock *rlock;		/* temp record lock */
    int code;				/* error return code */

    lock_ObtainShared (&sleeplock);
#ifdef SGIMIPS
    /* vnl_test_deadlock does not use arg end so just cast it here */
    code = vnl_test_deadlock (flock, (int)end, blocker);
#else  /* SGIMIPS */
    code = vnl_test_deadlock (flock, end, blocker);
#endif /* SGIMIPS */
    if (code) {
	lock_ReleaseShared (&sleeplock);
	return (code);
    }
    lock_UpgradeSToW (&sleeplock);
    code = vnl_alloc (flock, end, &rlock);
    if (code) {
	lock_ReleaseWrite (&sleeplock);
	return code;
    }
    rlock->un.blocker = blocker;
    blocker->un.blocking = 1;
    QAdd (&sleeplist, RLTOQ (rlock));
    lock_ReleaseWrite (&sleeplock);

    code = osi_SleepWI ((opaque)blocker, vnlock);
    if (code) code = EINTR;
    lock_ObtainWrite (vnlock);

    lock_ObtainWrite (&sleeplock);
    QRemove (RLTOQ (rlock));
    lock_ReleaseWrite (&sleeplock);

    vnl_free (rlock);
    return code;
}

/*
 * vnl_wakeup -- wake up processes sleeping on a file lock
 *
 * The wakeup process invalidates entries on the sleep list.  The
 * sleeping processes are then responsible for taking the entries off
 * the list (see vnl_sleep).
 */

static
void vnl_wakeup (struct record_lock *blocker)
{
    struct record_lock *rlock;

    if (blocker->un.blocking) {
	lock_ObtainWrite (&sleeplock);
	for (rlock = QTORL (QNext (&sleeplist));
	     rlock != QTORL (&sleeplist);
	     rlock = QTORL (QNext (RLTOQ (rlock))))
	    if (rlock->un.blocker == blocker)
		rlock->un.blocker = 0;
	blocker->un.blocking = 0;
	lock_ReleaseWrite (&sleeplock);
	osi_Wakeup (blocker);
    }
}

/*
 * vnl_blocked -- Determine if a lock would be blocked
 *
 * Return parameters:
 *	blocker - the record lock, if any, that causes the blocking
 *	marker - subsequent processing of the record lock list can
 *		 start at this point rather than at the beginning
 */

int vnl_blocked(
    struct flock *flock,		/* file lock */
#ifdef SGIMIPS
    afs_hyper_t end,			/* index of last byte in record */
#else  /* SGIMIPS */
    int end,				/* index of last byte in record */
#endif /* SGIMIPS */
    struct squeue *rllist,		/* list of record locks to search */
    struct record_lock **blockerP,	/* place to put blocking rlock */
    struct squeue **markerP)		/* place to put marker rlock */
{
    struct record_lock *rlock;		/* current record lock */

    rlock = QTORL (QNext (rllist));

    /* Skip over irrelevant locks */

    while (rlock != QTORL (rllist) &&
#ifdef SGIMIPS
		rlock->data_end < flock->l_start-1)
#else  /* SGIMIPS */
		(long)rlock->data_end < (long)flock->l_start-1)
#endif /* SGIMIPS */
	rlock = QTORL (QNext (RLTOQ (rlock)));

    *markerP = QPrev (RLTOQ (rlock));

    /* Main loop */

    while (rlock != QTORL (rllist) && rlock->data.l_start <= end) {
	if (rlock->data_end >= flock->l_start
	      && vnl_idcmp (&rlock->data, flock)
	      && (rlock->data.l_type == F_WRLCK
		    || flock->l_type == F_WRLCK)) {
	    *blockerP = rlock;
	    return 1;
	}
	rlock = QTORL (QNext (RLTOQ (rlock)));
    }

    /* if we get here, no blocking */
    return 0;
}

/*
 * vnl_adjust -- adjust existing locks in light of new one
 *
 * Return parameters:
 *	marker -- if the new one is to be inserted, this tells where.
 *		  if caller shouldn't bother to insert, marker set to zero.
 *
 * Adjustment is required when an existing lock, set by the current
 * process, locks some of the same bytes as the new one.  Adjustment consists
 * of modifying record locks so that they do not lock the bytes affected by
 * the new one.
 *
 * The same adjustment is done whether we are unlocking or locking.  The
 * only difference is that when we are locking, the caller will subsequently
 * add a new record lock to the list.
 *
 * (Optimization:  if all the bytes locked by the new lock are already locked
 * with the same type of lock, we do nothing, and the caller does nothing.)
 *
 * (Optimization:  we coalesce adjacent locks of the same type.)
 *
 * If the new lock causes an old one to be split in two, this function tries
 * to do the splitting.  It allocates a new record lock and adds it to the
 * list at the appropriate place.
 *
 * The allocation just described can fail.  Thus this function returns an
 * error code.
 */

int vnl_adjust (
    struct flock *flock,		/* new lock/unlock request */
#ifdef SGIMIPS
    afs_hyper_t *end,			/* index of last byte in record */
#else  /* SGIMIPS */
    int *end,				/* index of last byte in record */
#endif /* SGIMIPS */
    struct squeue *oldmarker,		/* place marker in
					 * list of record locks to adjust */
    struct squeue *rllist,		/* head of list of record locks */
    struct squeue **markerP)		/* place to put record lock after
					 * which to insert new lock */
{
    struct record_lock *rlock;		/* current record lock */
    struct squeue *marker;		/* record lock after which to insert */
    struct record_lock *next;		/* next record lock in main loop */
    struct record_lock *nlock;		/* new lock from split */
    struct record_lock *alock;		/* place to add nlock */
    struct flock newflock;		/* data for nlock */
    int code;				/* error return code */

    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST1,
	       ICL_TYPE_POINTER, flock,
	       ICL_TYPE_LONG, flock->l_type,
	       ICL_TYPE_LONG, flock->l_start,
	       ICL_TYPE_LONG, flock->l_len);

    rlock = QTORL (QNext (oldmarker));

    /* Skip over some irrelevant locks */

    while (rlock != QTORL (rllist) &&
#ifdef SGIMIPS
		rlock->data_end < flock->l_start-1)
#else  /* SGIMIPS */
		(long)rlock->data_end < (long)flock->l_start-1)
#endif /* SGIMIPS */
	rlock = QTORL (QNext (RLTOQ (rlock)));

    /* Initialize marker */

    if (rlock != QTORL (rllist) && rlock->data.l_start > flock->l_start)
	marker = QPrev (RLTOQ (rlock));
    else
	marker = QPrev (rllist);

    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST2,
	       ICL_TYPE_POINTER, rllist,
	       ICL_TYPE_POINTER, rlock,
	       ICL_TYPE_LONG, rlock->data.l_type,
	       ICL_TYPE_LONG, rlock->data.l_start);

    /* Main loop */

#ifdef SGIMIPS 
    while (rlock != QTORL (rllist) && rlock->data.l_start-1 <= *end) {
#else  /* SGIMIPS */
    while (rlock != QTORL (rllist) && (long)rlock->data.l_start-1 <= *end) {
#endif /* SGIMIPS */

	next = QTORL (QNext (RLTOQ (rlock)));

	icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST3,
		   ICL_TYPE_POINTER, rlock,
		   ICL_TYPE_POINTER, next,
		   ICL_TYPE_LONG, rlock->data.l_start,
		   ICL_TYPE_LONG, *end);

	/* Set marker */

	if (rlock->data.l_start <= flock->l_start)
	    marker = RLTOQ (rlock);

	/* Ignore record locks from other processes */

	if (vnl_idcmp (&rlock->data, flock)) {
	    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST4,
		       ICL_TYPE_LONG, rlock->data.l_pid,
		       ICL_TYPE_LONG, rlock->data.l_sysid,
		       ICL_TYPE_LONG, flock->l_pid,
		       ICL_TYPE_LONG, flock->l_sysid);
	    goto Next;
	}

	/* Ignore irrelevant locks */

#ifdef SGIMIPS
	if (rlock->data_end < flock->l_start-1) {
#else  /* SGIMIPS */
	if ((long)rlock->data_end < (long)flock->l_start-1) {
#endif /* SGIMIPS */
	    icl_Trace2(vnl_iclSetp, VNL_TRACE_ADJUST5,
		       ICL_TYPE_LONG, rlock->data_end,
		       ICL_TYPE_LONG, flock->l_start);
	    goto Next;
	}

	/* If we get here, the locks are either adjacent or overlapping. */

	if (rlock->data.l_type == flock->l_type) {
	    if (rlock->data.l_start > flock->l_start) {
		if (*end < rlock->data_end)
		    *end = rlock->data_end;
		icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST6,
			   ICL_TYPE_LONG, rlock->data.l_type,
			   ICL_TYPE_LONG, rlock->data.l_start,
			   ICL_TYPE_LONG, flock->l_start,
			   ICL_TYPE_LONG, *end);
		goto Delete;
	    }
	    if (rlock->data_end < *end) {
		flock->l_start = rlock->data.l_start;
		icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST7,
			   ICL_TYPE_LONG, rlock->data.l_type,
			   ICL_TYPE_LONG, rlock->data_end,
			   ICL_TYPE_LONG, *end,
			   ICL_TYPE_LONG, flock->l_start);
		goto Delete;
	    }
	    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST8,
		       ICL_TYPE_LONG, rlock->data.l_type,
		       ICL_TYPE_LONG, rlock->data.l_start,
		       ICL_TYPE_LONG, rlock->data_end,
		       ICL_TYPE_LONG, flock->l_start);
	    marker = 0;			/* tell caller to do nothing */
	    break;
	}

	/*
	   If we get here, the locks are of different types,
	   so adjacent locks are irrelevant.
	*/

	if (rlock->data.l_start > *end
	    || rlock->data_end < flock->l_start) {
	    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST9,
		       ICL_TYPE_LONG, rlock->data.l_start,
		       ICL_TYPE_LONG, *end,
		       ICL_TYPE_LONG, rlock->data_end,
		       ICL_TYPE_LONG, flock->l_start);
	    goto Next;
	}

	/* Need to add new lock? */

	if (rlock->data_end > *end) {

	    /* find place to add it */

	    alock = rlock;
	    do alock = QTORL (QNext (RLTOQ (alock)));
		while (alock != QTORL (rllist) && alock->data.l_start < *end+1);

	    /* create the new record lock */

	    newflock = rlock->data;
	    newflock.l_start = *end+1;
	    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST10,
		       ICL_TYPE_LONG, rlock->data_end,
		       ICL_TYPE_LONG, *end,
		       ICL_TYPE_LONG, rlock->data.l_start,
		       ICL_TYPE_POINTER, alock);
	    code = vnl_alloc (&newflock, rlock->data_end, &nlock);
	    if (code) return code;
	    nlock->un.blocking = 0;

	    /* add it */

	    QAddT (RLTOQ (alock), RLTOQ (nlock));
	}

	/* By now the only choice is whether to delete or adjust the old lock */

	if (rlock->data.l_start < flock->l_start) {

	    /* Adjust it */

	    rlock->data_end = flock->l_start-1;
	    vnl_wakeup (rlock);
	    icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST11,
		       ICL_TYPE_LONG, rlock->data.l_start,
		       ICL_TYPE_LONG, flock->l_start,
		       ICL_TYPE_LONG, rlock->data_end,
		       ICL_TYPE_LONG, *end);
	    goto Next;
	}

    Delete:
      icl_Trace4(vnl_iclSetp, VNL_TRACE_ADJUST12,
		 ICL_TYPE_POINTER, rlock,
		 ICL_TYPE_LONG, rlock->data.l_type,
		 ICL_TYPE_LONG, rlock->data.l_start,
		 ICL_TYPE_LONG, rlock->data_end);
	if (marker == RLTOQ (rlock)) marker = QPrev (marker);
	QRemove (RLTOQ (rlock));
	vnl_wakeup (rlock);
	vnl_free (rlock);
    Next:
	rlock = next;
    }

    *markerP = marker;
    return 0;
}

/*
 * vnl_cleanup -- clean up locks after a careless process
 *
 * We release all record locks held on a file by a process.  This is
 * done when the process closes the file.
 */

void vnl_cleanup(struct squeue *rllist)	/* list of record locks */
{
    struct record_lock *rlock;		/* current record lock */
    struct record_lock *next;		/* next record lock */
    struct flock dummyflock;		/* handy temp */

    vnl_idset (&dummyflock);

    rlock = QTORL (QNext (rllist));

    while (rlock != QTORL (rllist)) {
	next = QTORL (QNext (RLTOQ (rlock)));
	if (!vnl_idcmp (&rlock->data, &dummyflock)) {
	    QRemove (RLTOQ (rlock));
	    vnl_wakeup (rlock);
	    vnl_free (rlock);
	}
	rlock = next;
    }
}
