/* qFifoLib.h - fifo queue header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qFifoRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01b,26jun90,jcf  fixed FIFO_KEY definitions so priority 0 doesn't add at head.
		 removed definition of Q_CLASS.
01a,22oct89,jcf  written.
*/

#ifndef __INCqFifoLibh
#define __INCqFifoLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"
#include "dlllib.h"

/* fifo key defines */

#define	FIFO_KEY_HEAD	-1		/* put at head of queue */
#define	FIFO_KEY_TAIL	0		/* put at tail of q (any != -1) */


/* HIDDEN */

typedef DL_LIST Q_FIFO_HEAD;		/* Q_FIFO_HEAD */

typedef DL_NODE Q_FIFO_NODE;		/* Q_FIFO_NODE */

/* END HIDDEN */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern Q_FIFO_HEAD *	qFifoCreate (void);
extern Q_FIFO_NODE *	qFifoGet (Q_FIFO_HEAD *pQFifoHead);
extern STATUS 		qFifoDelete (Q_FIFO_HEAD *pQFifoHead);
extern STATUS 		qFifoInit (Q_FIFO_HEAD *pQFifoHead);
extern int 		qFifoInfo (Q_FIFO_HEAD *pQFifoHead, int nodeArray [],
			   	   int maxNodes);
extern void 		qFifoPut (Q_FIFO_HEAD *pQFifoHead,
				  Q_FIFO_NODE *pQFifoNode, ULONG key);
extern STATUS 		qFifoRemove (Q_FIFO_HEAD *pQFifoHead,
				     Q_FIFO_NODE *pQFifoNode);
extern Q_FIFO_NODE *	qFifoEach (Q_FIFO_HEAD *pQHead, FUNCPTR routine, int
				   routineArg);

#else	/* __STDC__ */

extern Q_FIFO_HEAD *	qFifoCreate ();
extern Q_FIFO_NODE *	qFifoEach ();
extern Q_FIFO_NODE *	qFifoGet ();
extern STATUS   	qFifoDelete ();
extern STATUS 		qFifoInit ();
extern int 		qFifoInfo ();
extern void 		qFifoPut ();
extern STATUS 		qFifoRemove ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqFifoLibh */
