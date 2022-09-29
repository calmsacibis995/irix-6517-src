/* qPriHeapLib.h - heap library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qPriHeapRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
		 added copyright notice.
01c,05jul90,jcf  added qPriHeapCalibrate().
		 make priority key unsigned.
01b,26jun90,jcf  remove queue class definition.
01a,19oct89,jcf  created.
*/

#ifndef __INCqPriHeapLibh
#define __INCqPriHeapLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "qclass.h"

/* status codes */

#define S_qPriHeapLib_NULL_HEAP_ARRAY		(M_qPriHeapLib | 1)


/* type definitions */

/* HIDDEN */

typedef struct		/* Q_PRI_HEAP_NODE */
    {
    ULONG key;			/* heap key (0 is highest priority) */
    int   index;		/* current index into heap array */
    } Q_PRI_HEAP_NODE;

typedef Q_PRI_HEAP_NODE *HEAP_ARRAY[];

typedef struct		/* Q_PRI_HEAP_HEAD */
    {
    Q_PRI_HEAP_NODE *pHighNode;		/* highest priority node in heap */
    HEAP_ARRAY	    *pHeapArray;	/* start of heap table */
    int		     heapIndex;		/* index of next available heap slot */
    } Q_PRI_HEAP_HEAD;

/* END_HIDDEN */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern HEAP_ARRAY *	qPriHeapArrayCreate (int heapSize);
extern Q_PRI_HEAP_HEAD *qPriHeapCreate (HEAP_ARRAY *pHeapArray);
extern Q_PRI_HEAP_NODE *qPriHeapEach (Q_PRI_HEAP_HEAD *pQHead, FUNCPTR routine,
				      int routineArg);
extern Q_PRI_HEAP_NODE *qPriHeapGet (Q_PRI_HEAP_HEAD *pQPriHeapHead);
extern Q_PRI_HEAP_NODE *qPriHeapGetExpired (Q_PRI_HEAP_HEAD *pQPriHeapHead);
extern STATUS 		qPriHeapArrayDelete (HEAP_ARRAY *pHeapArray);
extern STATUS 		qPriHeapDelete (Q_PRI_HEAP_HEAD *pQPriHeapHead);
extern STATUS 		qPriHeapInit (Q_PRI_HEAP_HEAD *pQPriHeapHead,
				      HEAP_ARRAY *pHeapArray);
extern STATUS 		qPriHeapTerminate (Q_PRI_HEAP_HEAD *pQPriHeapHead);
extern ULONG 		qPriHeapKey (Q_PRI_HEAP_NODE *pQPriHeapNode,
				     int keyType);
extern int 		qPriHeapInfo (Q_PRI_HEAP_HEAD *pQPriHeapHead,
				      int nodeArray [], int maxNodes);
extern void 		qPriHeapAdvance (Q_PRI_HEAP_HEAD *pQPriHeapHead);
extern void 		qPriHeapCalibrate (Q_PRI_HEAP_HEAD *pQPriHeapHead,
					   ULONG keyDelta);
extern void 		qPriHeapPut (Q_PRI_HEAP_HEAD *pQPriHeapHead,
				     Q_PRI_HEAP_NODE *pQPriHeapNode, ULONG key);
extern STATUS 		qPriHeapRemove (Q_PRI_HEAP_HEAD *pQPriHeapHead,
					Q_PRI_HEAP_NODE *pQPriHeapNode);
extern void 		qPriHeapResort (Q_PRI_HEAP_HEAD *pQPriHeapHead,
					Q_PRI_HEAP_NODE *pQPriHeapNode,
					ULONG newKey);
extern void 		qPriHeapShow (Q_PRI_HEAP_HEAD *pHeap, int format);

#else	/* __STDC__ */

extern HEAP_ARRAY *	qPriHeapArrayCreate ();
extern Q_PRI_HEAP_HEAD *qPriHeapCreate ();
extern Q_PRI_HEAP_NODE *qPriHeapEach ();
extern Q_PRI_HEAP_NODE *qPriHeapGet ();
extern Q_PRI_HEAP_NODE *qPriHeapGetExpired ();
extern STATUS 		qPriHeapArrayDelete ();
extern STATUS 		qPriHeapDelete ();
extern STATUS 		qPriHeapInit ();
extern STATUS 		qPriHeapTerminate ();
extern ULONG 		qPriHeapKey ();
extern int 		qPriHeapInfo ();
extern void 		qPriHeapAdvance ();
extern void 		qPriHeapCalibrate ();
extern void 		qPriHeapPut ();
extern STATUS 		qPriHeapRemove ();
extern void 		qPriHeapResort ();
extern void 		qPriHeapShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqPriHeapLibh */
