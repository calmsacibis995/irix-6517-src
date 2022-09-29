/* qPriListLib.h - priority linked list header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qPriListRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01c,05jul90,jcf  added qPriListCalibrate().
		 made priority key unsigned.
01b,26jun90,jcf  removed queue class definition.
01a,22oct89,jcf  written.
*/

#ifndef __INCqPriListLibh
#define __INCqPriListLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"
#include "qprinode.h"
#include "dlllib.h"


/*******************************************************************************
*
* qPriListFirst - find first node in list
*
* DESCRIPTION
*
* RETURNS
*	Pointer to the first node in a list, or
*	NULL if the list is empty.
*/

#define Q_PRI_LIST_FIRST(pList)						\
    (									\
    (DLL_FIRST((Q_PRI_HEAD *)(pList)))					\
    )

/*******************************************************************************
*
* qPriListEmpty - boolean function to check for empty list
*
* RETURNS:
* 	TRUE if list is empty
*	FALSE otherwise
*/

#define Q_PRI_LIST_EMPTY(pList)						\
    (									\
    (DLL_EMPTY((Q_PRI_HEAD *)(pList)))					\
    )

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern Q_PRI_HEAD *qPriListCreate (void);
extern Q_PRI_NODE *qPriListEach (Q_PRI_HEAD *pQHead, FUNCPTR routine,
				 int routineArg);
extern Q_PRI_NODE *qPriListGet (Q_PRI_HEAD *pQPriHead);
extern Q_PRI_NODE *qPriListGetExpired (Q_PRI_HEAD *pQPriHead);
extern STATUS 	qPriListDelete (Q_PRI_HEAD *pQPriHead);
extern STATUS 	qPriListInit (Q_PRI_HEAD *pQPriHead);
extern STATUS 	qPriListTerminate (Q_PRI_HEAD *pQPriHead);
extern ULONG 	qPriListKey (Q_PRI_NODE *pQPriNode, int keyType);
extern int 	qPriListInfo (Q_PRI_HEAD *pQPriHead, int nodeArray [],
			      int maxNodes);
extern void 	qPriListAdvance (Q_PRI_HEAD *pQPriHead);
extern void 	qPriListCalibrate (Q_PRI_HEAD *pQHead, ULONG keyDelta);
extern void 	qPriListPut (Q_PRI_HEAD *pQPriHead, Q_PRI_NODE *pQPriNode,
			     ULONG key);
extern void 	qPriListPutFromTail (Q_PRI_HEAD *pQPriHead,
				     Q_PRI_NODE *pQPriNode, ULONG key);
extern STATUS 	qPriListRemove (Q_PRI_HEAD *pQPriHead, Q_PRI_NODE *pQPriNode);
extern void 	qPriListResort (Q_PRI_HEAD *pQPriHead, Q_PRI_NODE *pQPriNode,
				ULONG newKey);

#else	/* __STDC__ */

extern Q_PRI_HEAD *qPriListCreate ();
extern Q_PRI_NODE *qPriListEach ();
extern Q_PRI_NODE *qPriListGet ();
extern Q_PRI_NODE *qPriListGetExpired ();
extern STATUS 	qPriListDelete ();
extern STATUS 	qPriListInit ();
extern STATUS 	qPriListTerminate ();
extern ULONG 	qPriListKey ();
extern int 	qPriListInfo ();
extern void 	qPriListAdvance ();
extern void 	qPriListCalibrate ();
extern void 	qPriListPut ();
extern void 	qPriListPutFromTail ();
extern STATUS 	qPriListRemove ();
extern void 	qPriListResort ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqPriListLibh */
