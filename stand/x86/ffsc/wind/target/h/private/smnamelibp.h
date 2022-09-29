/* smNameLibP.h - private shared name database library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,01aug94,dvs  backed out pme's changes for reserved fields in main data structures.
01e,20mar94,pme  added reserved fields in main data structures to allow
		 compatibility between future versions.
01d,29jan93,pme  added little endian support
01c,29sep92,pme  changed objId field to value in SM_OBJ_NAME 
		 and SM_OBJ_NAME_INFO
01b,22sep92,rrr  added support for c++
01a,19jul92,pme  extracted from smNameLib v1c.
*/

#ifndef __INCsmNameLibPh
#define __INCsmNameLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "smnamelib.h"
#include "smdlllib.h"
#include "private/semsmlibp.h"

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* typedefs */

typedef struct sm_obj_name_db   /* database header */
    {
    UINT32       initDone;      /* TRUE if name database is initialized */
    SM_DL_LIST   nameList;      /* list of entered name */
    SM_SEMAPHORE sem;           /* shared binary semaphore for access */
    UINT32       maxName;       /* maximum number of name in database */
    UINT32       curNumName;    /* current number of name in database */
    } SM_OBJ_NAME_DB;


typedef struct sm_obj_name                     /* name database element */
    {
    SM_DL_NODE   node;                         /* node for name list */
    void *       value;                        /* associated value */
    UINT32       type;                         /* associated type */
    char         name [MAX_NAME_LENGTH + 1];   /* associated name string */
    } SM_OBJ_NAME;

typedef struct sm_obj_name_info                /* database element info */
    {
    void *       value;                        /* element value */
    UINT32       type;                         /* associated type */
    char         name [MAX_NAME_LENGTH + 1];   /* associated name string */
    } SM_OBJ_NAME_INFO;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

/* globals */

extern SM_OBJ_NAME_DB * pSmNameDb; /* pointer to name database header */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void   smNameLibInit (void);
extern STATUS smNameInit (int maxNames);

#else   /* __STDC__ */

extern void   smNameLibInit ();
extern STATUS smNameInit ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmNameLibPh */
