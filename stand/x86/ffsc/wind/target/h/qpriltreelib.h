/* qPriLTreeLib.h - priority leftist tree queue library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,19jul92,pme  made qPriTreeRemove return STATUS.
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
		 added copyright notice.
01c,10jul90,jcf  made priority key unsigned.
01b,26jun90,jcf  removed queue class definition.
01a,19oct89,jcf  created.
*/

#ifndef __INCqPriLTreeLibh
#define __INCqPriLTreeLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "qclass.h"

/* type definitions */

/* HIDDEN */

typedef struct qPriLTreeNode	/* Q_PRI_L_TREE_NODE */
    {
    struct qPriLTreeNode *left;		/* pointer to left node in the tree */
    struct qPriLTreeNode *right;	/* pointer to right node in the tree */
    int			 distance;	/* shortest distance to leaf */
    ULONG		 key;		/* insertion key */
    } Q_PRI_L_TREE_NODE;

typedef struct			/* Q_PRI_L_TREE_HEAD */
    {
    Q_PRI_L_TREE_NODE *pRoot;		/* points to root of leftist tree */
    int		      count;		/* number of nodes in tree */
    } Q_PRI_L_TREE_HEAD;

/* END_HIDDEN */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

#else

extern Q_PRI_L_TREE_HEAD *	qPriLTreeCreate ();
extern STATUS *			qPriLTreeInit ();
extern STATUS 			qPriLTreeDelete ();
extern void 			qPriLTreePut ();
extern Q_PRI_L_TREE_NODE *	qPriLTreeGet ();
extern STATUS 			qPriLTreeRemove ();
extern void 			qPriLTreeResort ();
extern void 			qPriLTreeAdvance ();
extern Q_PRI_L_TREE_NODE *	qPriLTreeGetExpired ();
extern ULONG 			qPriLTreeKey ();
extern int 			qPriLTreeInfo ();
extern Q_PRI_L_TREE_NODE *	qPriLTreeEach ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCqPriLTreeLibh */
