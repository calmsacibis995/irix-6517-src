/* memLib.h - memory management library header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
03r,30oct94,kdl  merge cleanup - removed extra "#ifndef _ASMLANGUAGE".
03o,15oct93,cd   added #ifndef _ASMLANGUAGE.
03q,27oct94,ism  Fixed assembly problem from merge
03p,01dec93,jag  added struct MEM_PART_STATS and function memPartInfoGet.
03o,02apr93,edm  ifdef'd out non-ASMLANGUAGE portions
03n,05feb93,smb  added include of vxWorks.h
03m,22sep92,rrr  added support for c++
03l,19jul92,pme  added external declarations for sm partition functions.
03k,13jul92,rdc  added prototype for valloc.
03j,04jul92,jcf  cleaned up.
03i,22jun92,rdc  added memalign and memPartAlignedAlloc.
03h,26may92,rrr  the tree shuffle
03g,25mar92,jmm  added new options for error handling
03f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
03e,10jun91.del  added pragma for gnu960 alignment.
03d,05oct90,dnw  deleted private routines.
03c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
03b,10aug90,dnw  added declaration of memPartInit().
03a,18jul90,jcf  made partitions objects.
		 changed malloc(),realloc(),calloc() etc to return void *.
02c,17mar90,jcf  added structure type definition.
02b,28jun89,llk  added declaration for free();
02a,11jun88,dnw  changed for rev 03a of memLib.
01l,28mar88,gae  added function decl. of calloc().
01j,13nov87,gae  removed FRAGMENT definition; made function decl.'s IMPORTs.
01h,23oct87,rdc  added PARTITION defenitions.
01g,24dec86,gae  changed stsLib.h to vwModNum.h.
01f,21may86,rdc	 added forward declarations for malloc and realloc.
		 added FRAGMENT structure.
01e,13aug84,ecs  changed S_memLib_NO_MORE_MEMORY to S_memLib_NOT_ENOUGH_MEMORY.
01d,07aug84,ecs  added include of stsLib.h, and status codes.
01c,15jun84,dnw  removed declaration of memEnd (no longer exists).
01b,27jan84,ecs  added inclusion test.
01a,24may83,dnw  written
*/

#ifndef __INCmemLibh
#define __INCmemLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"

/* status codes */

#define S_memLib_NOT_ENOUGH_MEMORY		(M_memLib | 1)
#define S_memLib_INVALID_NBYTES			(M_memLib | 2)
#define S_memLib_BLOCK_ERROR			(M_memLib | 3)
#define S_memLib_NO_PARTITION_DESTROY		(M_memLib | 4)
#define S_memLib_PAGE_SIZE_UNAVAILABLE		(M_memLib | 5)

/* types */

#ifndef _ASMLANGUAGE

typedef struct mem_part *PART_ID;

/* Partition statistics structure */

typedef struct
    {

    unsigned long numBytesFree,	   /* Number of Free Bytes in Partition       */
		  numBlocksFree,   /* Number of Free Blocks in Partition      */
		  maxBlockSizeFree,/* Maximum block size that is free.	      */
		  numBytesAlloc,   /* Number of Allocated Bytes in Partition  */
		  numBlocksAlloc;  /* Number of Allocated Blocks in Partition */

    }  MEM_PART_STATS;
#endif /* ~ _ASMLANGUAGE */

/* partition options */

/* optional check for bad blocks */

#define MEM_BLOCK_CHECK			0x10

/* response to errors when allocating memory */

#define MEM_ALLOC_ERROR_LOG_FLAG	0x20
#define MEM_ALLOC_ERROR_SUSPEND_FLAG	0x40

/* response to errors when freeing memory */

#define MEM_BLOCK_ERROR_LOG_FLAG	0x80
#define MEM_BLOCK_ERROR_SUSPEND_FLAG	0x100

/* old style allocation errors - block too big, insufficient space */

#define MEM_ALLOC_ERROR_MASK		0x03
#define MEM_ALLOC_ERROR_RETURN		0
#define MEM_ALLOC_ERROR_LOG_MSG		0x01
#define MEM_ALLOC_ERROR_LOG_AND_SUSPEND	0x02

/* old style bad block found */

#define MEM_BLOCK_ERROR_MASK		0x0c
#define MEM_BLOCK_ERROR_RETURN		0
#define MEM_BLOCK_ERROR_LOG_MSG		0x04
#define MEM_BLOCK_ERROR_LOG_AND_SUSPEND	0x08


#ifndef _ASMLANGUAGE

/* variable declarations */

/* system partition */

extern PART_ID memSysPartId;

/* shared memory manager function pointers */

extern FUNCPTR  smMemPartOptionsSetRtn;
extern FUNCPTR  smMemPartFindMaxRtn;
extern FUNCPTR  smMemPartReallocRtn;
extern FUNCPTR  smMemPartShowRtn;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	memInit (char *pPool, unsigned poolSize);
extern STATUS 	memPartLibInit (char *pPool, unsigned poolSize);
extern PART_ID 	memPartCreate (char *pPool, unsigned poolSize);
extern void 	memPartInit (PART_ID partId, char *pPool, unsigned poolSize);
extern STATUS 	memPartAddToPool (PART_ID partId, char *pPool,
				  unsigned poolSize);
extern void 	memAddToPool (char *pPool, unsigned poolSize);
extern void *	memPartAlloc (PART_ID partId, unsigned nBytes);
extern void *	memalign (unsigned alignment, unsigned size);
extern void *   valloc (unsigned size);
extern STATUS 	memPartFree (PART_ID partId, char *pBlock);
extern STATUS 	memPartOptionsSet (PART_ID partId, unsigned options);
extern int 	memFindMax (void);
extern int 	memPartFindMax (PART_ID partId);
extern void *	memPartRealloc (PART_ID partId, char *pBlock, unsigned nBytes);
extern void 	memOptionsSet (unsigned options);
extern STATUS 	cfree (char *pBlock);
extern void 	memShowInit (void);
extern void 	memShow (int type);
extern STATUS 	memPartShow (PART_ID partId, int type);
extern STATUS   memPartInfoGet (PART_ID	partId, MEM_PART_STATS * ppartStats);

#else	/* __STDC__ */

extern STATUS 	memInit ();
extern STATUS 	memPartLibInit ();
extern PART_ID 	memPartCreate ();
extern void 	memPartInit ();
extern STATUS 	memPartAddToPool ();
extern void 	memAddToPool ();
extern void *	memPartAlloc ();
extern void *	memalign ();
extern void *   valloc ();
extern STATUS 	memPartFree ();
extern STATUS 	memPartOptionsSet ();
extern int 	memFindMax ();
extern int 	memPartFindMax ();
extern void *	memPartRealloc ();
extern void 	memOptionsSet ();
extern STATUS 	cfree ();
extern void 	memShowInit ();
extern void 	memShow ();
extern STATUS 	memPartShow ();
extern STATUS   memPartInfoGet ();

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCmemLibh */
