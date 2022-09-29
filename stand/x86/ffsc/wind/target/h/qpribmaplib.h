/* qPriBMapLib.h - bit mapped linked list library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02m,22sep92,rrr  added support for c++
02l,28jul92,jcf  removed arch dependence. added nPriorities. changed prototypes.
02k,19jul92,pme  made qPriBMapRemove return STATUS.
02j,04jul92,jcf  cleaned up.
02i,26may92,rrr  the tree shuffle
02h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed VOID to void
		  -changed copyright notice
02g,20sep91,hdn  changed a structure of BMAP_LIST of TRON.
02f,10sep91,wmd  added CPU==MIPS to conditional for BMAP_LIST struct.
02e,27aug91,shl  added CPU=MC68040 to conditional for BMAP_LIST struct.
02d,19jul91,hdn  changed CPU==G200 to CPU_FAMILY==TRON
02c,10jun91,del  added pragma for gnu960 alignment.
02b,10may91,wmd  added CPU=G200 to conditional for BMAP_LIST struct.
02a,22jan91,jcf  changed BMAP_LIST for portability to other architectures.
01e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01d,05oct90,shl  added ANSI function prototypes.
01c,10jul90,jcf  made priority key unsigned.
01b,26jun90,jcf  remove queue class definition.
01a,22oct89,jcf  written.
*/

#ifndef __INCqPriBMapLibh
#define __INCqPriBMapLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"
#include "vwmodnum.h"
#include "qprinode.h"
#include "dlllib.h"


#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* status codes */

#define S_qPriBMapLib_NULL_BMAP_LIST		(M_qPriBMapLib | 1)


/* HIDDEN */

typedef struct		/* BMAP_LIST */
    {
    UINT32	metaBMap;		/* lookup table for map */
    UINT8	bMap [32];		/* lookup table for listArray */
    DL_LIST	listArray [256];	/* doubly linked list head */
    } BMAP_LIST;

typedef struct		/* Q_PRI_BMAP_HEAD */
    {
    Q_PRI_NODE	*highNode;		/* highest priority node */
    BMAP_LIST	*pBMapList;		/* pointer to mapped list */
    UINT 	 nPriority;		/* priorities in queue (1,256) */
    } Q_PRI_BMAP_HEAD;

/* END_HIDDEN */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern BMAP_LIST *	qPriBMapListCreate (UINT maxPriority);
extern Q_PRI_BMAP_HEAD *qPriBMapCreate (BMAP_LIST *pBMapList, UINT maxPriority);
extern STATUS 		qPriBMapInit (Q_PRI_BMAP_HEAD *pQPriBMapHead,
			 	      BMAP_LIST *pBMapList, UINT maxPriority);
extern STATUS	 	qPriBMapDelete (Q_PRI_BMAP_HEAD *pQPriBMapHead);
extern Q_PRI_NODE *	qPriBMapEach (Q_PRI_BMAP_HEAD *pQHead, FUNCPTR routine,
				      int routineArg);
extern Q_PRI_NODE *	qPriBMapGet (Q_PRI_BMAP_HEAD *pQPriBMapHead);
extern STATUS 		qPriBMapListDelete (BMAP_LIST *pBMapList);
extern ULONG 		qPriBMapKey (Q_PRI_NODE *pQPriNode);
extern int 		qPriBMapInfo (Q_PRI_BMAP_HEAD *pQPriBMapHead,
				      int nodeArray [], int maxNodes);
extern void 		qPriBMapPut (Q_PRI_BMAP_HEAD *pQPriBMapHead,
				     Q_PRI_NODE *pQPriNode, ULONG key);
extern STATUS 		qPriBMapRemove (Q_PRI_BMAP_HEAD *pQPriBMapHead,
					Q_PRI_NODE *pQPriNode);
extern void 		qPriBMapResort (Q_PRI_BMAP_HEAD *pQPriBMapHead,
					Q_PRI_NODE *pQPriNode, ULONG newKey);

#else	/* __STDC__ */

extern BMAP_LIST *	qPriBMapListCreate ();
extern Q_PRI_BMAP_HEAD *qPriBMapCreate ();
extern Q_PRI_NODE *	qPriBMapEach ();
extern Q_PRI_NODE *	qPriBMapGet ();
extern STATUS 		qPriBMapDelete ();
extern STATUS 		qPriBMapInit ();
extern STATUS 		qPriBMapListDelete ();
extern ULONG 		qPriBMapKey ();
extern int 		qPriBMapInfo ();
extern void 		qPriBMapPut ();
extern STATUS 		qPriBMapRemove ();
extern void 		qPriBMapResort ();

#endif	/* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCqPriBMapLibh */
