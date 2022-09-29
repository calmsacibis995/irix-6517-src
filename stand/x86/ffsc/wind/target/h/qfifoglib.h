/* qFifoGLib.h - global fifo queue header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,30jul92,pme    cleanup.
01a,19jul92,pme    written.
*/

#ifndef __INCqFifoGLibh
#define __INCqFifoGLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"
#include "smdlllib.h"
#include "tasklib.h"

/* fifo key defines */

#define	FIFO_KEY_HEAD	-1		/* put at head of queue */
#define	FIFO_KEY_TAIL	0		/* put at tail of q (any != -1) */

/*
 * qFifoGRemove returns a particular status called ALREADY_REMOVED
 * when the shared TCB has been removed on give side.
 */

#define ALREADY_REMOVED 	1

/* HIDDEN */

typedef SM_DL_NODE Q_FIFO_G_NODE;

typedef struct q_fifo_g_head	/* Q_FIFO_G_HEAD */
    {
    UINT32	  first;	/* NOT USED! */
    UINT32 * 	  pLock;	/* NULL if already acquired */
    SM_DL_LIST	* pFifoQ;	/* shared memory pending queue */
    Q_CLASS	* pQClass;	/* pointer to class */
    } Q_FIFO_G_HEAD;

extern Q_CLASS_ID qFifoGClassId;

/* END HIDDEN */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern SM_DL_NODE *	qFifoGEach (Q_FIFO_G_HEAD * pQFifoGHead,
				    FUNCPTR routine, int routineArg);
extern SM_DL_NODE *	qFifoGGet (Q_FIFO_G_HEAD * pQFifoGHead);
extern STATUS 		qFifoGInit (Q_FIFO_G_HEAD * pQFifoGHead);
extern int 		qFifoGInfo (Q_FIFO_G_HEAD * pQFifoGHead,
				    int nodeArray[], int maxNodes);
extern void 		qFifoGPut (Q_FIFO_G_HEAD * pQFifoGHead,
				   SM_DL_NODE * pQFifoGNode, ULONG key);
extern STATUS 		qFifoGRemove (Q_FIFO_G_HEAD * pQFifoGHead,
				      SM_DL_NODE * pQFifoGNode);

#else	/* __STDC__ */

extern SM_DL_NODE *    qFifoGEach ();
extern SM_DL_NODE *    qFifoGGet ();
extern STATUS          qFifoGInit ();
extern int             qFifoGInfo ();
extern void            qFifoGPut ();
extern STATUS          qFifoGRemove ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqFifoGLibh */
