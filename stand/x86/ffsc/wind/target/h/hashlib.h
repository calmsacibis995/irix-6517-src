/* hashLib.h - hash table library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01b,26jun90,jcf  remove hash id error status.
01a,17nov89,jcf  written.
*/

#ifndef __INChashLibh
#define __INChashLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "slllib.h"
#include "classlib.h"
#include "private/objlibp.h"


/* status codes */

#define S_hashLib_KEY_CLASH		(M_hashLib | 1)


/* type definitions */

/* HIDDEN */

typedef struct hashtbl		/* HASH_TBL */
    {
    OBJ_CORE	objCore;		/* object management */
    int		elements;		/* number of elements in table */
    FUNCPTR	keyCmpRtn;		/* comparator function */
    FUNCPTR	keyRtn;			/* hash function */
    int		keyArg;			/* hash function argument */
    SL_LIST	*pHashTbl;		/* pointer to hash table array */
    } HASH_TBL;

typedef SL_NODE HASH_NODE;	/* HASH_NODE */

typedef HASH_TBL *HASH_ID;

/* END_HIDDEN */

/* These hash nodes are used by the hashing functions in hashLib(1) */

typedef struct			/* H_NODE_INT */
    {
    HASH_NODE	node;			/* linked list node (must be first) */
    int		key;			/* hash node key */
    int		data;			/* hash node data */
    } H_NODE_INT;

typedef struct			/* H_NODE_STRING */
    {
    HASH_NODE	node;			/* linked list node (must be first) */
    char	*string;		/* hash node key */
    int		data;			/* hash node data */
    } H_NODE_STRING;


/* class definition */

IMPORT CLASS_ID hashClassId;		/* hash table class id */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern BOOL 		hashKeyCmp (H_NODE_INT *pMatchHNode,
				    H_NODE_INT *pHNode, int keyCmpArg);
extern BOOL 		hashKeyStrCmp (H_NODE_STRING *pMatchHNode,
				       H_NODE_STRING *pHNode, int keyCmpArg);
extern HASH_ID 		hashTblCreate (int sizeLog2, FUNCPTR keyCmpRtn,
				       FUNCPTR keyRtn, int keyArg);
extern HASH_NODE *	hashTblEach (HASH_ID hashId, FUNCPTR routine,
				     int routineArg);
extern HASH_NODE *	hashTblFind (HASH_ID hashId, HASH_NODE *pMatchNode,
				     int keyCmpArg);
extern STATUS 		hashLibInit (void);
extern STATUS 		hashTblDelete (HASH_ID hashId);
extern STATUS 		hashTblDestroy (HASH_ID hashId, BOOL dealloc);
extern STATUS 		hashTblInit (HASH_TBL *pHashTbl, SL_LIST *pTblMem,
				     int sizeLog2, FUNCPTR keyCmpRtn,
				     FUNCPTR keyRtn, int keyArg);
extern STATUS 		hashTblPut (HASH_ID hashId, HASH_NODE *pHashNode);
extern STATUS 		hashTblRemove (HASH_ID hashId, HASH_NODE *pHashNode);
extern STATUS 		hashTblTerminate (HASH_ID hashId);
extern int 		hashFuncIterScale (int elements, H_NODE_STRING *pHNode,
					   int seed);
extern int 		hashFuncModulo (int elements, H_NODE_INT *pHNode,
					int divisor);
extern int 		hashFuncMultiply (int elements, H_NODE_INT *pHNode,
					  int multiplier);

#else	/* __STDC__ */

extern BOOL 	hashKeyCmp ();
extern BOOL 	hashKeyStrCmp ();
extern HASH_ID 	hashTblCreate ();
extern HASH_NODE *hashTblEach ();
extern HASH_NODE *hashTblFind ();
extern STATUS 	hashLibInit ();
extern STATUS 	hashTblDelete ();
extern STATUS 	hashTblDestroy ();
extern STATUS 	hashTblInit ();
extern STATUS 	hashTblPut ();
extern STATUS 	hashTblRemove ();
extern STATUS 	hashTblTerminate ();
extern int 	hashFuncIterScale ();
extern int 	hashFuncModulo ();
extern int 	hashFuncMultiply ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INChashLibh */
