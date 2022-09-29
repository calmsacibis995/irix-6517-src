/* smFixBlkLibP.h - private fixed block shared mem mgr library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,01aug94,dvs  backed out pme's changes for reserved fields in main data structures.
01c,20mar94,pme  added reserved fields in main data structures to allow
		 compatibility between future versions.
01b,22sep92,rrr  added support for c++
01a,19jul92,pme  extracted from smFixBlkLib v1a.
*/

#ifndef __INCsmFixBlkLibPh
#define __INCsmFixBlkLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "smdlllib.h"

/* defines */

#define MEM_PART_TYPE_SM_FIX    21      /* fixed block shared mem partition */

/* typedefs */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

typedef struct sm_fix_blk_part 		/* Fixed block partition header */
    {
    UINT32       verify;                /* partition is initialized */
    UINT32       objType;               /* fixed block partition */

    SM_DL_LIST   freeList;              /* list of free blocks */
    int          lock;                  /* partition spinlock */
    unsigned     totalBlocks;           /* total number of blocks in pool */
    int          blockSize;             /* block size in bytes */

    /* allocation statistics */

    unsigned curBlocksAllocated;        /* current # of blocks allocated */
    unsigned cumBlocksAllocated;        /* cumulative # of blocks allocated */

    } SM_FIX_BLK_PART;

typedef struct sm_fix_blk_part * SM_FIX_BLK_PART_ID;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void           smFixBlkPartInit (SM_FIX_BLK_PART_ID gPartId,
                                     char * pPool, unsigned poolSize,
                                     unsigned blockSize);
extern STATUS         smFixBlkPartFree (SM_FIX_BLK_PART_ID gPartId,
                                     char * pBlock);
extern STATUS         smFixBlkPartShow (SM_FIX_BLK_PART_ID gPartId);
extern void *         smFixBlkPartAlloc (SM_FIX_BLK_PART_ID gPartId);

#else   /* __STDC__ */

extern void           smFixBlkPartInit ();
extern STATUS         smFixBlkPartFree ();
extern STATUS         smFixBlkPartShow ();
extern void *         smFixBlkPartAlloc ();

#endif  /* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmFixBlkLibPh */
