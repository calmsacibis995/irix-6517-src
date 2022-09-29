/* classLibP.h - private object class management library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,04jul92,jcf  created.
*/

#ifndef __INCclassLibPh
#define __INCclassLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "memlib.h"
#include "classlib.h"
#include "private/objlibp.h"


typedef struct obj_class	/* OBJ_CLASS */
    {
    OBJ_CORE		objCore;	/* object core of class object */
    struct mem_part	*objPartId;	/* memory partition to allocate from */
    unsigned		objSize;	/* size of object */
    unsigned		objAllocCnt;	/* number of allocated objects */
    unsigned		objFreeCnt;	/* number of deallocated objects */
    unsigned		objInitCnt;	/* number of initialized objects */
    unsigned		objTerminateCnt;/* number of terminiated objects */
    int			coreOffset;	/* offset from object start to objCore*/
    FUNCPTR		createRtn;	/* object creation routine */
    FUNCPTR		initRtn;	/* object initialization routine */
    FUNCPTR		destroyRtn;	/* object destroy routine */
    FUNCPTR		showRtn;	/* object show routine */
    FUNCPTR		helpRtn;	/* object help routine */
    } OBJ_CLASS;


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern CLASS_ID	classCreate (unsigned objectSize, int coreOffset,
			     FUNCPTR createRtn, FUNCPTR initRtn,
			     FUNCPTR destroyRtn);
extern STATUS	classInit (OBJ_CLASS *pObjClass, unsigned objectSize,
			   int coreOffset, FUNCPTR createRtn, FUNCPTR initRtn,
			   FUNCPTR destroyRtn);
extern STATUS	classDestroy (CLASS_ID classId);
extern STATUS	classHelpConnect (CLASS_ID classId, FUNCPTR helpRtn);
extern STATUS	classShowConnect (CLASS_ID classId, FUNCPTR showRtn);

#else	/* __STDC__ */

extern CLASS_ID	classCreate ();
extern STATUS	classInit ();
extern STATUS	classDestroy ();
extern STATUS	classHelpConnect ();
extern STATUS	classShowConnect ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCclassLibPh */
