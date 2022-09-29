/* dllLib.h - doubly linked list library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01d,08apr91,jdi  added NOMANUAL to prevent mangen.
01c,20dec90,gae  fixed declaration of dllRemove.
01b,05oct90,shl  added ANSI function prototypes.
		 added copyright notice.
01a,07aug89,jcf  written.
*/

#ifndef __INCdllLibh
#define __INCdllLibh

#ifdef __cplusplus
extern "C" {
#endif


/* type definitions */

typedef struct dlnode		/* Node of a linked list. */
    {
    struct dlnode *next;	/* Points at the next node in the list */
    struct dlnode *previous;	/* Points at the previous node in the list */
    } DL_NODE;


/* HIDDEN */

typedef struct			/* Header for a linked list. */
    {
    DL_NODE *head;	/* header of list */
    DL_NODE *tail;	/* tail of list */
    } DL_LIST;

/* END_HIDDEN */

/*******************************************************************************
*
* dllFirst - find first node in list
*
* DESCRIPTION
* Finds the first node in a doubly linked list.
*
* RETURNS
*	Pointer to the first node in a list, or
*	NULL if the list is empty.
*
* NOMANUAL
*/

#define DLL_FIRST(pList)		\
    (					\
    (((DL_LIST *)(pList))->head)	\
    )

/*******************************************************************************
*
* dllLast - find last node in list
*
* Finds the last node in a doubly linked list.
*
* RETURNS
*  pointer to the last node in list, or
*  NULL if the list is empty.
*
* NOMANUAL
*/

#define DLL_LAST(pList)			\
    (					\
    (((DL_LIST *)(pList))->tail)	\
    )

/*******************************************************************************
*
* dllNext - find next node in list
*
* Locates the node immediately after the node pointed to by the pNode.
*
* RETURNS:
* 	Pointer to the next node in the list, or
*	NULL if there is no next node.
*
* NOMANUAL
*/

#define DLL_NEXT(pNode)			\
    (					\
    (((DL_NODE *)(pNode))->next)	\
    )

/*******************************************************************************
*
* dllPrevious - find preceding node in list
*
* Locates the node immediately before the node pointed to by the pNode.
*
* RETURNS:
* 	Pointer to the preceding node in the list, or
*	NULL if there is no next node.
*
* NOMANUAL
*/

#define DLL_PREVIOUS(pNode)		\
    (					\
    (((DL_NODE *)(pNode))->previous)	\
    )

/*******************************************************************************
*
* dllEmpty - boolean function to check for empty list
*
* RETURNS:
* 	TRUE if list is empty
*	FALSE otherwise
*
* NOMANUAL
*/

#define DLL_EMPTY(pList)			\
    (						\
    (((DL_LIST *)pList)->head == NULL)		\
    )


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern DL_LIST *dllCreate (void);
extern DL_NODE *dllEach (DL_LIST *pList, FUNCPTR routine, int routineArg);
extern DL_NODE *dllGet (DL_LIST *pList);
extern STATUS 	dllDelete (DL_LIST *pList);
extern STATUS 	dllInit (DL_LIST *pList);
extern STATUS 	dllTerminate (DL_LIST *pList);
extern int 	dllCount (DL_LIST *pList);
extern void 	dllAdd (DL_LIST *pList, DL_NODE *pNode);
extern void 	dllInsert (DL_LIST *pList, DL_NODE *pPrev, DL_NODE *pNode);
extern void 	dllRemove (DL_LIST *pList, DL_NODE *pNode);

#else	/* __STDC__ */

extern DL_LIST *dllCreate ();
extern DL_NODE *dllEach ();
extern DL_NODE *dllGet ();
extern STATUS 	dllDelete ();
extern STATUS 	dllInit ();
extern STATUS 	dllTerminate ();
extern int 	dllCount ();
extern void 	dllAdd ();
extern void 	dllInsert ();
extern void 	dllRemove ();

#endif	/* __STDC__ */
#ifdef __cplusplus
}
#endif

#endif /* __INCdllLibh */
