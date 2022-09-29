/* memPartLibP.h - private memory management library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,28jul92,jcf  added external declaration for memPartOptionsDefault.
01b,19jul92,pme  added external declarations for sm partition functions.
01a,01jul92,jcf  extracted from memLib v3r.
*/

#ifndef __INCmemPartLibPh
#define __INCmemPartLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "memlib.h"
#include "classlib.h"
#include "dlllib.h"
#include "private/semlibp.h"
#include "private/objlibp.h"


/* macros for getting to next and previous blocks */

#define NEXT_HDR(pHdr)  ((BLOCK_HDR *) ((char *) (pHdr) + (2 * (pHdr)->nWords)))
#define PREV_HDR(pHdr)	((pHdr)->pPrevHdr)


/* macros for converting between the "block" that caller knows
 * (actual available data area) and the block header in front of it */

#define HDR_TO_BLOCK(pHdr)	((char *) ((int) pHdr + sizeof (BLOCK_HDR)))
#define BLOCK_TO_HDR(pBlock)	((BLOCK_HDR *) ((int) pBlock - \
						sizeof(BLOCK_HDR)))


/* macros for converting between the "node" that is strung on the freelist
 * and the block header in front of it */

#define HDR_TO_NODE(pHdr)	(& ((FREE_BLOCK *) pHdr)->node)
#define NODE_TO_HDR(pNode)	((BLOCK_HDR *) ((int) pNode - \
						OFFSET (FREE_BLOCK, node)))

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* typedefs */

typedef struct mem_part
    {
    OBJ_CORE	objCore;		/* object management */
    DL_LIST	freeList;		/* list of free blocks */
    SEMAPHORE	sem;			/* partition semaphore */
    unsigned	totalWords;		/* total number of words in pool */
    unsigned	minBlockWords;		/* min blk size in words includes hdr */
    unsigned	options;		/* options */

    /* allocation statistics */

    unsigned curBlocksAllocated;	/* current # of blocks allocated */
    unsigned curWordsAllocated;		/* current # of words allocated */
    unsigned cumBlocksAllocated;	/* cumulative # of blocks allocated */
    unsigned cumWordsAllocated;		/* cumulative # of words allocated */

    } PARTITION;

typedef struct blockHdr		/* BLOCK_HDR */
    {
    struct blockHdr *	pPrevHdr;	/* pointer to previous block hdr */
    unsigned		nWords : 31;	/* size in words of this block */
    unsigned		free   : 1;	/* TRUE = this block is free */
#if CPU_FAMILY==I960
    UINT32		pad[2];		/* 8 byte pad for round up */
#endif	/* CPU_FAMILY==I960 */
    } BLOCK_HDR;

typedef struct			/* FREE_BLOCK */
    {
    BLOCK_HDR		hdr;		/* normal block header */
    DL_NODE		node;		/* freelist links */
#if CPU_FAMILY==I960
    UINT32		pad[2];		/* 8 byte pad for round up */
#endif	/* CPU_FAMILY==I960 */
    } FREE_BLOCK;


/* variable declarations */

extern CLASS_ID memPartClassId;		/* memory partition class id */
extern FUNCPTR  memPartBlockErrorRtn;	/* block error method */
extern FUNCPTR  memPartAllocErrorRtn;	/* alloc error method */
extern FUNCPTR  memPartSemInitRtn;	/* partition semaphore init method */
extern unsigned	memPartOptionsDefault;	/* default partition options */
extern UINT	memDefaultAlignment;	/* default alignment */
extern int	mutexOptionsMemLib;	/* mutex options */

/* shared memory manager function pointers */

extern FUNCPTR  smMemPartAddToPoolRtn;
extern FUNCPTR  smMemPartFreeRtn;
extern FUNCPTR  smMemPartAllocRtn;

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void *	memPartAlignedAlloc (PART_ID partId, unsigned nBytes,
				     unsigned alignment);
extern BOOL	memPartBlockIsValid (PART_ID partId, BLOCK_HDR *pHdr,
				     BOOL isFree);

#else	/* __STDC__ */

extern void *	memPartAlignedAlloc ();
extern BOOL	memPartBlockIsValid ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmemPartLibPh */
