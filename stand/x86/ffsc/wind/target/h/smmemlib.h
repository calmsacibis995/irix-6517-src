/* smMemLib.h - shared memory management library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01g,29jan93,pme  made smMemAddToPool() return STATUS.
01f,15oct92,rrr  silenced warnings
01e,29sep92,pme  changed user callable routine names 
                 moved smMemPartShow prototype to private/smMemPartLibP.h
01d,22sep92,rrr  added support for c++
01c,28jul92,pme  made smMemPartCreate return PART_ID instead of SM_PART_ID.
01b,28jul92,pme  removed unnecessary includes.
01a,19jul92,pme  written from 03f memLib.h
*/

#ifndef __INCsmMemLibh
#define __INCsmMemLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "memlib.h"

typedef struct sm_obj_partition * SM_PART_ID;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern PART_ID 	  memPartSmCreate (char * pPool, unsigned poolSize);
extern STATUS     smMemFree (void * ptr);
extern int        smMemFindMax (void);
extern void *     smMemMalloc (unsigned nBytes);
extern void *     smMemCalloc (int elemNum, int elemSize);
extern void *     smMemRealloc (void * pBlock, unsigned newSize);
extern STATUS     smMemAddToPool (char * pPool, unsigned poolSize);
extern STATUS     smMemOptionsSet (unsigned options);
extern void       smMemShowInit (void);
extern void       smMemShow (int type);

#else	/* __STDC__ */

extern PART_ID    memPartSmCreate ();
extern STATUS     smMemFree ();
extern int        smMemFindMax ();
extern void *     smMemMalloc ();
extern void *     smMemCalloc ();
extern void *     smMemRealloc ();
extern STATUS     smMemAddToPool ();
extern STATUS     smMemOptionsSet ();
extern void       smMemShowInit ();
extern void       smMemShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmMemLibh */
