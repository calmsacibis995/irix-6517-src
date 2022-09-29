/* smObjLibP.h - private shared memory objects library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,01aug94,dvs  backed out pme's changes for reserved fields in main data structures.
01f,20mar94,pme  added reserved fields in main data structures to allow
		 compatibility between future versions.
01e,29jan93,pme  added little endian support
01d,22sep92,rrr  added support for c++
01c,23aug92,jcf  fixed struct reference.
01b,30jul92,pme  added smObjTaskDeleteFailRtn declaration.
01a,19jul92,pme  extracted from smObjLib v1d.
*/

#ifndef __INCsmObjLibPh
#define __INCsmObjLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "smobjlib.h"
#include "smdlllib.h"
#include "smlib.h"
#include "private/smnamelibp.h"
#include "private/smfixblklibp.h"
#include "private/smmemlibp.h"
#include "private/semsmlibp.h"

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* typedefs */

typedef struct sm_obj_tcb        /* SM_OBJ_TCB - shared memory object tcb */
    {
    SM_DL_NODE        qNode;     /* 0x00: multiway q node for pend q */
    struct windTcb *  localTcb;  /* 0x10: address of local TCB */
    UINT32            ownerCpu;  /* 0x14: cpu number on which task runs */
    UINT32            action;    /* 0x18: action when put on CPU event queue */
    UINT32            removedByGive;/* TRUE: if removed from pend Q by give */
    } SM_OBJ_TCB;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

/* globals */

extern SM_FIX_BLK_PART_ID smTcbPartId;	/* shared TCB partition */

extern FUNCPTR smObjTcbFreeRtn;		/* shared TCB free routine */
extern FUNCPTR smObjTcbFreeFailRtn;	/* shared TCB free fail routine */
extern FUNCPTR smObjTaskDeleteFailRtn;	/* taskDelete fail routine */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS smObjTcbInit ();
extern STATUS smObjTcbFree (SM_OBJ_TCB * pSmObjTcb);
extern void   smObjTimeoutLogMsg (char * routineName, char * lockLocalAdrs);
extern void   smObjTcbFreeLogMsg ();
extern STATUS smObjEventSend (SM_DL_LIST * pEventList, UINT32 destCpu);
extern void   smObjEventProcess (SM_DL_LIST * pEventList);
extern void   smObjObjShow (int smObjId, int level);

#else   /* __STDC__ */

extern STATUS smObjTcbInit ();
extern STATUS smObjTcbFree ();
extern void   smObjTimeoutLogMsg ();
extern void   smObjTcbFreeLogMsg ();
extern STATUS smObjEventSend ();
extern void   smObjEventProcess ();
extern void   smObjObjShow ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmObjLibPh */
