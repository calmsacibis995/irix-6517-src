/* qPriNode.h - priority node header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01e,10jun91.del  added pragma for gnu960 alignment.
01d,16oct90,shl  made #else ANSI style.
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,10jul90,jcf  made priority key unsigned.
01a,22oct89,jcf  written.
*/

#ifndef __INCqPriNodeh
#define __INCqPriNodeh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE
#include "vxworks.h"
#include "dlllib.h"

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */
/* HIDDEN */
			/* Q_PRI_HEAD */

typedef DL_LIST Q_PRI_HEAD;

typedef struct		/* Q_PRI_NODE */
    {
    DL_NODE	node;		/* 0: priority doubly linked node */
    ULONG	key;		/* 8: insertion key (ie. priority) */
    } Q_PRI_NODE;

/* END_HIDDEN */


#else	/* _ASMLANGUAGE */

#define 	Q_PRI_NODE_KEY	 8	/* queue insertion key */

#endif	/* _ASMLANGUAGE */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */
#ifdef __cplusplus
}
#endif

#endif /* __INCqPriNodeh */
