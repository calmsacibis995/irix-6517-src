/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/zlc/RCS/zlc.h,v 65.1 1997/10/20 19:18:12 jdoak Exp $ */

#ifndef	TRANSARC_ZLC_H
#define	TRANSARC_ZLC_H

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/hs_host.h>
#include <dcedfs/hs_errs.h>
#include <dcedfs/tkc.h>
#include <dcedfs/icl.h>

/* define remove queue item (zlc_removeItem) state values and macros */
#define ZLC_ST_START             0
#define ZLC_ST_FXDALLY           1
#define ZLC_ST_CRASHDALLY        2
#define ZLC_ST_MOVEDALLY         3
#define ZLC_ST_REPDALLY          4
#define ZLC_ST_OFDREQUEST        5
#define ZLC_ST_QUEUED            6
#define ZLC_ST_CLEANUP           7

#define ZLC_ISDALLY_ST(state) \
((state) == ZLC_ST_FXDALLY || \
 (state) == ZLC_ST_CRASHDALLY || \
 (state) == ZLC_ST_MOVEDALLY || \
 (state) == ZLC_ST_REPDALLY)

/* remove queue item state definitions
 *
 * START      - initial state of an item.
 * FXDALLY    - waiting for FX to start; maximum dally of FXDALLY_TMAX.
 * CRASHDALLY - waiting for end of server crash TSR.
 * MOVEDALLY  - waiting for end of fileset move TSR.
 * REPDALLY   - allowing time for KAs to arrive; dally of REPDALLY_TFIX.
 * OFDREQUEST - OFD request is being (or is about to be) executed.
 * QUEUED     - waiting for async OFD token grant.
 * CLEANUP    - the vnode has been released; item can be deallocated.
 */

/* declare remove queue entry item */
struct zlc_removeItem {
    struct zlc_removeItem *next;      /* next item in remove queue */
    struct zlc_removeItem *prev;      /* previous item in remove queue */
    afsFid fid;                       /* file to be removed */
    struct vnode *vp;		      /* held vnode associated with file */
    short state;		      /* current state of item */
    short refCount;		      /* reference counter */
    unsigned long timeTry;	      /* time at which to (re-)evaluate */
                                      /*   state transition criteria    */
    unsigned long timeExp;            /* time at which state expires and */
                                      /*   forced transition occurs      */
};

/* declare remove queue and associated locks and counts
 *
 * zlc_removeQLock  - protects remove queue items and counts
 * zlc_removeQ      - list of ZLC files waiting to be (or recently) removed
 * zlc_removeQSize  - number of items on zlc_removeQ list
 * zlc_removeQClean - number of items on zlc_removeQ list in CLEANUP state
 *
 * Count of held vnodes on remove queue is (zlc_removeQSize - zlc_removeQClean)
 */
extern osi_dlock_t zlc_removeQLock;
extern struct zlc_removeQueue {
    struct zlc_removeItem *head;
    struct zlc_removeItem *tail;
} zlc_removeQ;
extern int zlc_removeQSize;
extern int zlc_removeQClean;

/* define macros to enqueue/dequeue items on remove queue */
#define ZLC_RQ_ENQ(rip) \
  MACRO_BEGIN \
      (rip)->next = zlc_removeQ.head; \
      (rip)->prev = (struct zlc_removeItem *)0; \
      if (zlc_removeQ.head) { \
          zlc_removeQ.head->prev = (rip); \
      } else { \
          zlc_removeQ.tail = (rip); \
      } \
      zlc_removeQ.head = (rip); \
  MACRO_END

#define ZLC_RQ_DEQ(rqp) \
  MACRO_BEGIN \
      if ((rqp)->prev) { \
          (rqp)->prev->next = (rqp)->next; \
      } else { \
          zlc_removeQ.head = (rqp)->next; \
      } \
      if ((rqp)->next) { \
          (rqp)->next->prev = (rqp)->prev; \
      } else { \
          zlc_removeQ.tail = (rqp)->prev; \
      } \
  MACRO_END
      
/* define macros to hold and rele remove queue entries */
#define ZLC_RQ_HOLD(rqp)    ((rqp)->refCount++)
#define ZLC_RQ_RELE(rqp)    ((rqp)->refCount--)
#define ZLC_RQ_ISHELD(rqp)  ((rqp)->refCount)

/* define fixed, maximum, and retry dally-time values */
#define ZLC_T_FXDALLY_RETRY   ( 1 * 60)
#define ZLC_T_FXDALLY_MAX     (15 * 60)
#define ZLC_T_REPDALLY_FIX    (90 * 60)

/* declare held vnode target and threshold limits and their defaults */
extern int zlc_vnTarget;	  /* target "maximum" number of vnodes held */
extern int zlc_vnThreshold;	  /* absolute maximum number of vnodes held */
#define ZLC_VN_TARGET_DEF     200 /* default value for zlc_vnTarget */
#define ZLC_VN_THRESHOLD_DEF  220 /* default value for zlc_vnThreshold */

/* define target limit for number of remove queue entries in CLEANUP state */
#define ZLC_CLEANUP_TARGET   ((unsigned)zlc_vnTarget >> 3)

/* define criteria for waking manager thread to prune remove queue */
#define zlc_WakeMgrToPruneQ() \
((zlc_removeQClean > ZLC_CLEANUP_TARGET) || \
 (zlc_removeQSize - zlc_removeQClean > zlc_vnThreshold))

/* declare asynchronous OFD token grant lock
 *
 * A thread making an asynchronous token grant via the ZLC's host module
 * (zlc_AsyncGrant()) must write-lock zlc_asyncGrantLock BEFORE write-locking
 * zlc_removeQLock.  This prevents races between the thread issuing an OFD
 * request (zlc_GetDeleteToken()) and the thread making the corresponding
 * async grant (if any), without requiring a lock on zlc_removeQLock.
 */
extern osi_dlock_t zlc_asyncGrantLock;

/* declare server crash TSR states and times
 *
 * zlc_recoveryState - FX-indicated state of recovery process
 * zlc_recoveryEnd   - FX-indicated expiration time for recovery; extendable
 */
#define ZLC_REC_UNKNOWN		1  /* FX is not yet up */
#define ZLC_REC_ONGOING 	2  /* FX indicates crash TSR is ongoing */
#define ZLC_REC_OVER            3  /* FX indicates crash TSR has completed */
extern int zlc_recoveryState;
extern unsigned long zlc_recoveryEnd;

/* declare host structure for ZLC module */
extern struct hs_host zlc_host;

/* declare ICL trace set */
extern struct icl_set *zlc_iclSetp;

/* declare ZLC module state */
extern int zlc_initted;

/* export ZLC-module interface */
extern int  zlc_Init(void);
extern void zlc_TryRemove(struct vnode *avp, afsFid *afidp);
extern void zlc_CleanVolume(afs_hyper_t *volIDp);
extern void zlc_SetRestartState(unsigned long expire);

/* define number of exported symbols */
#define ZLC_SYMBOL_LINKS	4

/* declare the linkage table.  If we are inside the core component, we
 * will initialize it at compile-time; outside the core, we initialize
 * at module init time by copying the core version.
 */

#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
extern void *zlc_symbol_linkage[ZLC_SYMBOL_LINKS];
#endif /* _KERNEL && AFS_SUNOS5_ENV */

#endif	/* TRANSARC_ZLC_H */
