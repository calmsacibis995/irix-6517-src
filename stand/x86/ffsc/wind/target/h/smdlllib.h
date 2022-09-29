/* smDllLib.h - shared memory doubly linked list library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,29jan93,pme  added little endian support 
01c,22sep92,rrr  added support for c++
01b,27jul92,pme  changed structure declarations.
01a,19jul92,pme  written from dllLib
*/

#ifndef __INCsmDllLibh
#define __INCsmDllLibh

#ifdef __cplusplus
extern "C" {
#endif

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif /* CPU_FAMILY==I960 */

/* type definitions */

typedef struct sm_dl_node	/* Node of a shared linked list. */
    {
    struct sm_dl_node * next;	 /* Points at the next node in list */
    struct sm_dl_node * previous;/* Points at the previous node in list */
    } SM_DL_NODE;

/* HIDDEN */

typedef struct sm_dl_list	/* Header for a shared linked list. */
    {
    SM_DL_NODE * head;		/* Header list node */
    SM_DL_NODE * tail;		/* Tail list node */
    } SM_DL_LIST;

/* END_HIDDEN */

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                         /* turn off alignment requirement */
#endif /* CPU_FAMILY==I960 */

/*******************************************************************************
*
* smDllFirst - find first node in shared memory list
*
* DESCRIPTION
* Finds the first node in a doubly linked shared memory list.
*
* RETURNS
*       Local address of the first node in a list, or
*       LOC_NULL if the list is empty.
*
* NOMANUAL
*/

#define SM_DL_FIRST(pList)                     		         	\
    (                              		                     	\
    (GLOB_TO_LOC_ADRS (ntohl ((int) (((SM_DL_LIST *)(pList))->head))))	\
    )

/*******************************************************************************
*
* smDllLast - find last node in shared memory list
*
* Finds the last node in a doubly linked shared memory list.
*
* RETURNS
*       Local address pointer to the last node in list, or
*       LOC_NULL if the list is empty.
*
* NOMANUAL
*/

#define SM_DL_LAST(pList)                                	\
    (                                                    	\
    (GLOB_TO_LOC_ADRS (ntohl ((int) (((SM_DL_LIST *)(pList))->tail))))	\
    )


/*******************************************************************************
*
* smDllNext - find next node in shared memory list
*
* Locates the node immediately after the node pointed to by the pNode.
*
* RETURNS:
*       Local address pointer to the next node in the list, or
*       LOC_NULL if there is no next node.
*
* NOMANUAL
*/

#define SM_DL_NEXT(pNode)                                		\
    (                                                    		\
    (GLOB_TO_LOC_ADRS (ntohl ((int) (((SM_DL_NODE *)(pNode))->next)))) 	\
    )

/*******************************************************************************
*
* smDllPrevious - find previous node in shared memory doubly linked list
*
* Locates the node immediately before the node pointed to by the pNode.
*
* RETURNS:
*       Local address pointer to the preceding node in the list, or
*       LOC_NULL if there is no next node.
*
* NOMANUAL
*/

#define SM_DL_PREVIOUS(pNode)                                 		 \
    (                                                         		 \
    (GLOB_TO_LOC_ADRS (ntohl ((int) ((SM_DL_NODE *)(pNode))->previous))))\
    )

/*******************************************************************************
*
* smDllEmpty - boolean function to check for empty shared memory list
*
* RETURNS:
*       TRUE if list is empty
*       FALSE otherwise
*
* NOMANUAL
*/

#define SM_DL_EMPTY(pList)                         \
    (                                              \
    (((SM_DL_LIST *)pList)->head == NULL)          \
    )


/* function declarations */


#if defined(__STDC__) || defined(__cplusplus)

extern SM_DL_LIST * smDllCreate (void);
extern SM_DL_NODE * smDllEach (SM_DL_LIST * pList, FUNCPTR routine,
			       int routineArg);
extern SM_DL_NODE * smDllGet (SM_DL_LIST * pList);
extern STATUS       smDllDelete (SM_DL_LIST * pList);
extern STATUS       smDllInit (SM_DL_LIST * pList);
extern STATUS       smDllTerminate (SM_DL_LIST * pList);
extern int          smDllCount (SM_DL_LIST * pList);
extern void         smDllAdd (SM_DL_LIST * pList, SM_DL_NODE * pNode);
extern void         smDllInsert (SM_DL_LIST * pList, SM_DL_NODE * pPrev,
				 SM_DL_NODE * pNode);
extern void         smDllRemove (SM_DL_LIST * pList, SM_DL_NODE * pNode);
extern void         smDllConcat (SM_DL_LIST * pDstList, SM_DL_LIST * pAddList);

#else	/* __STDC__ */

extern SM_DL_LIST * smDllCreate ();
extern SM_DL_NODE * smDllEach ();
extern SM_DL_NODE * smDllGet ();
extern STATUS       smDllDelete ();
extern STATUS       smDllInit ();
extern STATUS       smDllTerminate ();
extern int          smDllCount ();
extern void         smDllAdd ();
extern void         smDllInsert ();
extern void         smDllRemove ();
extern void         smDllConcat ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmDllLibh */
