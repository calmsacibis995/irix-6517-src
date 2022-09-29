/* sllLib.h - singly linked list library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,08apr91,jdi  added NOMANUAL to prevent mangen.
01c,05oct90,shl  added ANSI function prototypes.
		 added copyright notice.
01b,10aug90,dnw  changed declaration of sllRemove() from STATUS to void.
01a,03jun89,jcf  written.
*/

#ifndef __INCsllLibh
#define __INCsllLibh

#ifdef __cplusplus
extern "C" {
#endif


/* type definitions */

typedef struct slnode		/* Node of a linked list. */
    {
    struct slnode *next;	/* Points at the next node in the list */
    } SL_NODE;


/* HIDDEN */

typedef struct			/* Header for a linked list. */
    {
    SL_NODE *head;	/* header of list */
    SL_NODE *tail;	/* tail of list */
    } SL_LIST;

/* END_HIDDEN */


/************************************************************************
*
* sllFirst - find first node in list
*
* DESCRIPTION
* Finds the first node in a singly linked list.
*
* RETURNS
*	Pointer to the first node in a list, or
*	NULL if the list is empty.
*
* NOMANUAL
*/

#define SLL_FIRST(pList)	\
    (				\
    (((SL_LIST *)pList)->head)	\
    )

/************************************************************************
*
* sllLast - find last node in list
*
* This routine finds the last node in a singly linked list.
*
* RETURNS
*  pointer to the last node in list, or
*  NULL if the list is empty.
*
* NOMANUAL
*/

#define SLL_LAST(pList)		\
    (				\
    (((SL_LIST *)pList)->tail)	\
    )

/************************************************************************
*
* sllNext - find next node in list
*
* Locates the node immediately after the node pointed to by the pNode.
*
* RETURNS:
* 	Pointer to the next node in the list, or
*	NULL if there is no next node.
*
* NOMANUAL
*/

#define SLL_NEXT(pNode)		\
    (				\
    (((SL_NODE *)pNode)->next)	\
    )

/************************************************************************
*
* sllEmpty - boolean function to check for empty list
*
* RETURNS:
* 	TRUE if list is empty
*	FALSE otherwise
*
* NOMANUAL
*/

#define SLL_EMPTY(pList)			\
    (						\
    (((SL_LIST *)pList)->head == NULL)		\
    )


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern SL_LIST *sllCreate (void);
extern SL_NODE *sllEach (SL_LIST *pList, FUNCPTR routine, int routineArg);
extern SL_NODE *sllGet (SL_LIST *pList);
extern SL_NODE *sllPrevious (SL_LIST *pList, SL_NODE *pNode);
extern STATUS 	sllDelete (SL_LIST *pList);
extern STATUS 	sllInit (SL_LIST *pList);
extern STATUS 	sllTerminate (SL_LIST *pList);
extern int 	sllCount (SL_LIST *pList);
extern void 	sllPutAtHead (SL_LIST *pList, SL_NODE *pNode);
extern void 	sllPutAtTail (SL_LIST *pList, SL_NODE *pNode);
extern void 	sllRemove (SL_LIST *pList, SL_NODE *pDeleteNode,
			   SL_NODE *pPrevNode);

#else	/* __STDC__ */

extern SL_LIST *sllCreate ();
extern SL_NODE *sllEach ();
extern SL_NODE *sllGet ();
extern SL_NODE *sllPrevious ();
extern STATUS 	sllDelete ();
extern STATUS 	sllInit ();
extern STATUS 	sllTerminate ();
extern int 	sllCount ();
extern void 	sllPutAtHead ();
extern void 	sllPutAtTail ();
extern void 	sllRemove ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsllLibh */
