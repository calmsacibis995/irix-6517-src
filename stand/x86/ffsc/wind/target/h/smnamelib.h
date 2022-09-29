/* smNameLib.h - shared memory name database library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,29sep92,pme  changed function names and changed smObjId parameter to value.
                 changed S_smNameLib_ID_NOT_FOUND to S_smNameLib_VALUE_NOT_FOUND
01b,22sep92,rrr  added support for c++
01a,19jul92,pme  cleaned up according to code review.
                 written.
*/

#ifndef __INCsmNameLibh
#define __INCsmNameLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "smdlllib.h"

/* generic status codes */

#define S_smNameLib_NOT_INITIALIZED           (M_smNameLib | 1)
#define S_smNameLib_NAME_TOO_LONG             (M_smNameLib | 2)
#define S_smNameLib_NAME_NOT_FOUND            (M_smNameLib | 3)
#define S_smNameLib_VALUE_NOT_FOUND           (M_smNameLib | 4)
#define S_smNameLib_NAME_ALREADY_EXIST        (M_smNameLib | 5)
#define S_smNameLib_DATABASE_FULL             (M_smNameLib | 6)
#define S_smNameLib_INVALID_WAIT_TYPE         (M_smNameLib | 7)

#define MAX_NAME_LENGTH 19	/* maximum lengh of a name in database */

/* default object types */

#define MAX_DEF_TYPE   5	/* number of predefined types */

#define T_SM_SEM_B   0		/* shared binary semaphore type */
#define T_SM_SEM_C   1		/* shared counting semaphore type */
#define T_SM_MSG_Q   2		/* shared message queue type */
#define T_SM_PART_ID 3		/* shared memory partition type */
#define T_SM_BLOCK   4		/* shared memory allocated block type */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern    STATUS       smNameAdd (char * name, void * value, int type);
extern    STATUS       smNameFind (char * name, void ** pValue,
                                   int * pType, int waitType);
extern    STATUS       smNameFindByValue (void * value, char * name, 
                                          int * pType, int waitType);
extern    STATUS       smNameRemove (char * name);
extern    void         smNameShowInit (void);
extern    STATUS       smNameShow (int level);

#else

extern	  STATUS       smNameAdd ();
extern	  STATUS       smNameFind ();
extern	  STATUS       smNameFindByValue ();
extern	  STATUS       smNameRemove ();
extern	  void         smNameShowInit ();
extern	  STATUS       smNameShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmNameLibh */
