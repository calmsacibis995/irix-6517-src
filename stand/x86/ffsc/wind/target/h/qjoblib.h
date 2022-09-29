/* qJobLib.h - job queue header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01c,16oct90,shl  made #else ANSI style.
01b,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01a,01may90,dnw   written.
*/

#ifndef __INCqJobLibh
#define __INCqJobLibh

#ifdef __cplusplus
extern "C" {
#endif


#include "vxworks.h"

#ifndef _ASMLANGUAGE

#include "qlib.h"


/* qJobPut "priorities" */

#define Q_JOB_PRI_TAIL		0	/* add node to tail of queue */
#define Q_JOB_PRI_HEAD		1	/* add node to head of queue */
#define Q_JOB_PRI_DONT_CARE	1	/* head is a bit faster */


/* HIDDEN */

typedef struct qJobNode			/* Node of a job queue */
    {
    struct qJobNode *next;
    } Q_JOB_NODE;

typedef struct				/* Head of job queue */
    {
    Q_JOB_NODE *first;			/* first node in queue */
    Q_JOB_NODE *last;			/* last node in queue */
    int		count;			/* number of nodes in queue */
    Q_CLASS    *pQClass;		/* must be 4th long word */
    Q_HEAD	pendQ;			/* queue of blocked tasks */
    } Q_JOB_HEAD;

/* END HIDDEN */


IMPORT Q_CLASS_ID qJobClassId;		/* job queue class */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern Q_JOB_HEAD *	qJobCreate (Q_CLASS_ID pendQType);
extern Q_JOB_NODE *	qJobGet (Q_JOB_HEAD *pQHead, int timeout);
extern STATUS 		qJobDelete (Q_JOB_HEAD *pQHead);
extern STATUS 		qJobInit (Q_JOB_HEAD *pQHead, Q_CLASS_ID pendQType);
extern STATUS 		qJobTerminate (Q_JOB_HEAD *pQHead);
extern int 		qJobInfo (Q_JOB_HEAD *pQHead, int nodeArray [],
				  int maxNodes);
extern void 		qJobPut (Q_JOB_HEAD *pQHead, Q_JOB_NODE *pNode,int key);
extern Q_JOB_NODE *	qJobEach (Q_JOB_HEAD *pQHead, FUNCPTR routine,
				  int routineArg);

#else	/* __STDC__ */

extern Q_JOB_HEAD *	qJobCreate ();
extern Q_JOB_NODE *	qJobEach ();
extern Q_JOB_NODE *	qJobGet ();
extern STATUS 		qJobDelete ();
extern STATUS 		qJobInit ();
extern STATUS 		qJobTerminate ();
extern int 		qJobInfo ();
extern void 		qJobPut ();

#endif	/* __STDC__ */

#else	/* _ASMLANGUAGE */

#define _QHEAD
#define _QTAIL		(4)
#define _QCOUNT		(8)
#define _QPENDQ		(16)

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCqJobLibh */
