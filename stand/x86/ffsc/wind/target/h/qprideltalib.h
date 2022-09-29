/* qPriDeltaLib.h - priority linked list header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qPriDeltaRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01c,10jul90,jcf  made priority key unsigned.
01b,26jun90,jcf  removed queue class definition.
01a,22oct89,jcf  written.
*/

#ifndef __INCqPriDeltaLibh
#define __INCqPriDeltaLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"
#include "qprinode.h"
#include "dlllib.h"


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern Q_PRI_HEAD *	qPriDeltaCreate (void);
extern Q_PRI_NODE *	qPriDeltaEach (Q_PRI_HEAD *pQHead, FUNCPTR routine,
				       int routineArg);
extern Q_PRI_NODE *	qPriDeltaGet (Q_PRI_HEAD *pQPriHead);
extern Q_PRI_NODE *	qPriDeltaGetExpired (Q_PRI_HEAD *pQPriHead);
extern STATUS 		qPriDeltaDelete (Q_PRI_HEAD *pQPriHead);
extern STATUS 		qPriDeltaInit (Q_PRI_HEAD *pQPriHead);
extern STATUS 		qPriDeltaTerminate (Q_PRI_HEAD *pQPriHead);
extern ULONG 		qPriDeltaKey (Q_PRI_NODE *pQPriNode);
extern int 		qPriDeltaInfo (Q_PRI_HEAD *pQPriHead, int nodeArray [],
				       int maxNodes);
extern void 		qPriDeltaAdvance (Q_PRI_HEAD *pQPriHead);
extern void 		qPriDeltaPut (Q_PRI_HEAD *pQPriHead,
				      Q_PRI_NODE *pQPriNode, ULONG key);
extern STATUS 		qPriDeltaRemove (Q_PRI_HEAD *pQPriHead,
					 Q_PRI_NODE *pQPriNode);
extern void 		qPriDeltaResort (Q_PRI_HEAD *pQPriHead,
					 Q_PRI_NODE *pQPriNode, ULONG newKey);

#else	/* __STDC__ */

extern Q_PRI_HEAD *	qPriDeltaCreate ();
extern Q_PRI_NODE *	qPriDeltaEach ();
extern Q_PRI_NODE *	qPriDeltaGet ();
extern Q_PRI_NODE *	qPriDeltaGetExpired ();
extern STATUS 		qPriDeltaDelete ();
extern STATUS 		qPriDeltaInit ();
extern STATUS 		qPriDeltaTerminate ();
extern ULONG 		qPriDeltaKey ();
extern int 		qPriDeltaInfo ();
extern void 		qPriDeltaAdvance ();
extern void 		qPriDeltaPut ();
extern STATUS 		qPriDeltaRemove ();
extern void 		qPriDeltaResort ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqPriDeltaLibh */
