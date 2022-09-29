/* objLibP.h - private object management library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,15oct93,cd   added #ifndef _ASMLANGUAGE.
01b,22sep92,rrr  added support for c++
01a,04jul92,jcf  extracted from v1j objLib.h.
*/

#ifndef __INCobjLibPh
#define __INCobjLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "objlib.h"
#include "classlib.h"


#ifndef _ASMLANGUAGE

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

typedef struct obj_core		/* OBJ_CORE */
    {
    struct obj_class *pObjClass;	/* pointer to object's class */
    } OBJ_CORE;

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern OBJ_ID 	objCreate (CLASS_ID classId, ...);
extern STATUS 	objInit (CLASS_ID classId, OBJ_ID objId, ...);
extern STATUS 	objDelete (OBJ_ID objId);
extern STATUS 	objDestroy (OBJ_ID objId, BOOL dealloc, int timeout);
extern STATUS 	objFree (CLASS_ID classId, char *pObject);
extern STATUS 	objHelp (OBJ_ID objId, int helpType);
extern STATUS 	objTerminate (OBJ_ID objId);
extern void *	objAlloc (CLASS_ID classId);
extern void *	objAllocExtra (CLASS_ID classId, unsigned nExtraBytes,
			       void ** ppExtra);
extern void 	objCoreInit (OBJ_CORE *pObjCore, CLASS_ID pObjClass);
extern void 	objCoreTerminate (OBJ_CORE *pObjCore);

#else	/* __STDC__ */

extern OBJ_ID 	objCreate ();
extern STATUS 	objDelete ();
extern STATUS 	objDestroy ();
extern STATUS 	objFree ();
extern STATUS 	objHelp ();
extern STATUS 	objInit ();
extern STATUS 	objTerminate ();
extern void *	objAlloc ();
extern void *	objAllocExtra ();
extern void 	objCoreInit ();
extern void 	objCoreTerminate ();

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCobjLibPh */
