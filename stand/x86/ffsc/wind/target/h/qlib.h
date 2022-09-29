/* qLib.h - multi-way queue library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,15oct93,cd   added #ifndef _ASMLANGUAGE.
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
01h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01g,10jun91.del  added pragma for gnu960 alignment.
01f,05apr91,gae  added NOMANUAL to avoid fooling mangen.
01e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01d,02oct90,jcf  removed nested comments.
01c,05jul90,jcf  added Q_CALIBRATE.
01b,26jun90,jcf  added definition of all queue class ids/types.
		 remove includes of all queue classes.
		 defined Q_NODE/Q_HEAD as independent structure.
		 added Q_RESORT().
01a,21oct89,jcf  written.
*/

#ifndef __INCqLibh
#define __INCqLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "qclass.h"

/* status codes */

#define S_qLib_Q_CLASS_ID_ERROR			(M_qLib | 1)

#ifndef _ASMLANGUAGE

/* queue classes */

IMPORT Q_CLASS_ID		qFifoClassId;
IMPORT Q_CLASS_ID		qPriListClassId;
IMPORT Q_CLASS_ID		qPriListFromTailClassId;
IMPORT Q_CLASS_ID		qPriDeltaClassId;
IMPORT Q_CLASS_ID		qPriHeapClassId;
IMPORT Q_CLASS_ID		qPriBMapClassId;

/* queue types */

#define Q_FIFO			qFifoClassId
#define Q_PRI_LIST		qPriListClassId
#define Q_PRI_LIST_FROM_TAIL	qPriListFromTailClassId
#define Q_PRI_DELTA		qPriDeltaClassId
#define Q_PRI_HEAP		qPriHeapClassId
#define Q_PRI_BMAP		qPriBMapClassId

/* HIDDEN */

/* All queue class nodes must not exceed the sizeof (Q_NODE).  If larger data
 * structures must be used, use one of the private fields as pointer to provide
 * a level on indirection so the size of Q_NODE is not exceeded.
 */

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

typedef struct		/* Q_NODE */
    {
    UINT     qPriv1;			/* use is queue type dependent */
    UINT     qPriv2;			/* use is queue type dependent */
    UINT     qPriv3;			/* use is queue type dependent */
    UINT     qPriv4;			/* use is queue type dependent */
    } Q_NODE;


/* All queue classes must not exceed the sizeof (Q_HEAD).  If larger data
 * structures must be used, use one of the private fields as pointer to provide
 * a level on indirection so the size of Q_HEAD is not exceeded.
 *
 * The first field in the Q_HEAD must contain the highest priority Q_NODE.
 * The routine qFirst () makes this assumption.  The scheduler does a qFirst
 * on the ready queue as part of its scheduling.
 */

typedef struct		/* Q_HEAD */
    {
    Q_NODE  *pFirstNode;		/* first node in queue based on key */
    UINT     qPriv1;			/* use is queue type dependent */
    UINT     qPriv2;			/* use is queue type dependent */
    Q_CLASS *pQClass;			/* pointer to queue class */
    } Q_HEAD;

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

/* END HIDDEN */


/*******************************************************************************
*
* Q_FIRST - return first node in multi-way queue
*
* This routine returns a pointer to the first node in the specified multi-way
* queue head.  If the queue is empty, NULL is returned.
*
* RETURNS
*  Pointer to first queue node in queue head, or
*  NULL if queue is empty.
*
* NOMANUAL
*/

#define Q_FIRST(pQHead)							\
    ((Q_NODE *)(((Q_HEAD *)(pQHead))->pFirstNode))

/*******************************************************************************
*
* Q_PUT - insert a node into a multi-way queue
*
* This routine inserts a node into a multi-way queue.  The insertion is based
* on the key and the underlying queue class.
*
* NOMANUAL
*/

#define Q_PUT(pQHead,pQNode,key)					\
    (*(((Q_HEAD *)(pQHead))->pQClass->putRtn))				\
    (((Q_HEAD *)(pQHead)), ((Q_NODE *)(pQNode)), (key))

/*******************************************************************************
*
* Q_GET - remove and return first node in multi-way queue
*
* This routine removes and returns the first node in a multi-way queue.  If
* the queue is empty, NULL is returned.
*
* RETURNS
*  Pointer to first queue node in queue head, or
*  NULL if queue is empty.
*
* NOMANUAL
*/

#define Q_GET(pQHead)							\
    ((Q_NODE *)((*(((Q_HEAD *)(pQHead))->pQClass->getRtn))		\
     ((Q_HEAD *)(pQHead))))

/*******************************************************************************
*
* Q_REMOVE - remove a node from a multi-way queue
*
* This routine removes a node from the specified multi-way queue.
*
* NOMANUAL
*/

#define Q_REMOVE(pQHead,pQNode)						\
    (*(((Q_HEAD *)(pQHead))->pQClass->removeRtn))			\
    (((Q_HEAD *)(pQHead)), ((Q_NODE *)(pQNode)))

/*******************************************************************************
*
* Q_RESORT - resort a node to a new position based on a new key
*
* This routine resorts a node to a new position based on a new key.  It can
* be used to change the priority of a queued element, for instance.
*
* NOMANUAL
*/

#define Q_RESORT(pQHead,pQNode,newKey)					\
    (*(((Q_HEAD *)(pQHead))->pQClass->resortRtn))			\
    (((Q_HEAD *)(pQHead)), ((Q_NODE *)(pQNode)), ((ULONG)(newKey)))

/*******************************************************************************
*
* Q_ADVANCE - advance a queues concept of time (timer queues only)
*
* Multi-way queues that keep nodes prioritized by time-to-fire utilize this
* routine to advance time.  It is usually called from within a clock-tick
* interrupt service routine.
*
* NOMANUAL
*/

#define Q_ADVANCE(pQHead)						\
    (*(((Q_HEAD *)(pQHead))->pQClass->advanceRtn))			\
    (((Q_HEAD *)(pQHead)))

/*******************************************************************************
*
* Q_GET_EXPIRED - return a time-to-fire expired node
*
* This routine returns a time-to-fire expired node in a multi-way timer queue.
* Expired nodes result from a qAdvance(2) advancing a node beyond its delay.
* As many nodes may expire on a single qAdvance(2), this routine should be
* called inside a while loop until NULL is returned.  NULL is returned when
* there are no expired nodes.
*
* RETURNS
*  Pointer to first queue node in queue head, or
*  NULL if queue is empty.
*
* NOMANUAL
*/

#define Q_GET_EXPIRED(pQHead)						\
    ((Q_NODE *)((*(((Q_HEAD *)(pQHead))->pQClass->getExpiredRtn))	\
     ((Q_HEAD *)(pQHead))))

/*******************************************************************************
*
* Q_KEY - return the key of a node
*
* This routine returns the key of a node currently in a multi-way queue.  The
* keyType determines key style on certain queue classes.
*
* RETURNS
*  Node's key.
*
* NOMANUAL
*/

#define Q_KEY(pQHead,pQNode,keyType)					\
    (*(((Q_HEAD *)(pQHead))->pQClass->keyRtn))				\
    (((Q_NODE *)(pQNode)), ((int)(keyType)))

/*******************************************************************************
*
* Q_CALIBRATE - offset every node in a queue by some delta
*
* This routine offsets every node in a multi-way queue by some delta.  The
* offset may either by positive or negative.
*
* NOMANUAL
*/

#define Q_CALIBRATE(pQHead,keyDelta)					\
    (*(((Q_HEAD *)(pQHead))->pQClass->calibrateRtn))			\
    (((Q_HEAD *)(pQHead)), ((int)(keyDelta)))

/*******************************************************************************
*
* Q_INFO - gather information on a multi-way queue
*
* This routine fills up to maxNodes elements of a nodeArray with nodes
* currently in a multi-way queue.  The actual number of nodes copied to the
* array is returned.  If the nodeArray is NULL, then the number of nodes in
* the multi-way queue is returned.
*
* RETURNS
*  Number of node pointers copied into the nodeArray, or
*  Number of nodes in multi-way queue if nodeArray is NULL
*
* NOMANUAL
*/

#define Q_INFO(pQHead,nodeArray,maxNodes)				\
    (*(((Q_HEAD *)(pQHead))->pQClass->infoRtn))				\
    (((Q_HEAD *)(pQHead)),((int *)(nodeArray)),((int)(maxNodes)))

/*******************************************************************************
*
* Q_EACH - call a routine for each node in a queue
*
* This routine calls a user-supplied routine once for each node in the
* queue.  The routine should be declared as follows:
* .CS
*  BOOL routine (pQNode, arg)
*      Q_NODE	*pQNode;	* pointer to a queue node          *
*      int	arg;		* arbitrary user-supplied argument *
* .CE
* The user-supplied routine should return TRUE if qEach (2) is to continue
* calling it for each entry, or FALSE if it is done and qEach can exit.
*
* RETURNS: NULL if traversed whole queue, or pointer to Q_NODE that
*          qEach ended with.
*
* NOMANUAL
*/

#define Q_EACH(pQHead,routine,routineArg)				\
    ((Q_NODE *)((*(((Q_HEAD *)(pQHead))->pQClass->eachRtn))		\
    (((Q_HEAD *)(pQHead)),((FUNCPTR)(routine)),((int)(routineArg)))))

#if defined(__STDC__) || defined(__cplusplus)

extern Q_HEAD *	qCreate (Q_CLASS *pQClass, ...);
extern Q_NODE *	qEach (Q_HEAD *pQHead, FUNCPTR routine, int routineArg);
extern Q_NODE *	qFirst (Q_HEAD *pQHead);
extern Q_NODE *	qGet (Q_HEAD *pQHead);
extern Q_NODE *	qGetExpired (Q_HEAD *pQHead);
extern STATUS 	qDelete (Q_HEAD *pQHead);
extern STATUS 	qInit (Q_HEAD *pQHead, Q_CLASS *pQClass, ...);
extern STATUS 	qTerminate (Q_HEAD *pQHead);
extern ULONG 	qKey (Q_HEAD *pQHead, Q_NODE *pQNode, int keyType);
extern int 	qInfo (Q_HEAD *pQHead, Q_NODE *nodeArray [ ], int maxNodes);
extern void 	qAdvance (Q_HEAD *pQHead);
extern void 	qCalibrate (Q_HEAD *pQHead, ULONG keyDelta);
extern void 	qPut (Q_HEAD *pQHead, Q_NODE *pQNode, ULONG key);
extern STATUS 	qRemove (Q_HEAD *pQHead, Q_NODE *pQNode);
extern void 	qResort (Q_HEAD *pQHead, Q_NODE *pQNode, ULONG newKey);

#else	/* __STDC__ */

extern Q_HEAD *	qCreate ();
extern Q_NODE *	qEach ();
extern Q_NODE *	qFirst ();
extern Q_NODE *	qGet ();
extern Q_NODE *	qGetExpired ();
extern STATUS 	qDelete ();
extern STATUS 	qInit ();
extern STATUS 	qTerminate ();
extern ULONG 	qKey ();
extern int 	qInfo ();
extern void 	qAdvance ();
extern void 	qCalibrate ();
extern void 	qPut ();
extern STATUS 	qRemove ();
extern void 	qResort ();

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCqLibh */
