/* vmLibP.h - private header for architecture independent mmu interface */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,13feb93,kdl  fixed test in VM_TEXT_PAGE_PROTECT macro.
01f,10feb93,rdc  added support for text page protection routine. 
01e,08oct92,rdc  fixed VM_TRANSLATE macro.
01d,22sep92,rrr  added support for c++
01c,30jul92,rdc  added pointer to vmTranslate routine to VM_LIB_INFO.
01b,27jul92,rdc  modified VM_LIB_INFO.
01a,10jul92,rdc  written.
*/

#ifndef __INCvmLibPh
#define __INCvmLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "memlib.h"
#include "lstlib.h"
#include "private/objlibp.h"
#include "private/classlibp.h"
#include "private/semlibp.h"
#include "vmlib.h"
#include "mmulib.h"

/* an instance of VM_LIB_INFO is defined in usrConfig.c and is initialized
 * if vmLib has been installed. If the bundled version of vmLib.c is installed,
 * only a subset of the routines will be available - if the pointers are null,
 * then the routines are not available.
 */

typedef struct
    {
    BOOL vmLibInstalled;       /* full mmu support available */
    BOOL vmBaseLibInstalled;   /* base mmu support available */
    BOOL protectTextSegs;      /* TRUE == protext text segments */
    FUNCPTR pVmStateSetRtn;    /* pointer to vmStateSet if vmLib included */
    FUNCPTR pVmStateGetRtn;    /* pointer to vmStateGet if vmLib included */
    FUNCPTR pVmEnableRtn;      /* pointer to vmEnable if vmLib included */
    FUNCPTR pVmPageSizeGetRtn; /* ptr to vmPageSizeGet if vmLib included */
    FUNCPTR pVmTranslateRtn;   /* ptr vmTranslate if vmLib included */
    FUNCPTR pVmTextProtectRtn; /* ptr to vmTextLock routine if vmLib included */
    } VM_LIB_INFO;

#define VM_STATE_SET(context, pVirtual, len, stateMask, state) \
    ((vmLibInfo.pVmStateSetRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmStateSetRtn) (context, pVirtual, len, stateMask, state)))

#define VM_STATE_GET(context, pageAddr, pState) \
    ((vmLibInfo.pVmStateGetRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmStateGetRtn) (context, pageAddr, pState)))

#define VM_ENABLE(enable) \
    ((vmLibInfo.pVmEnableRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmEnableRtn) (enable)))

#define VM_PAGE_SIZE_GET() \
    ((vmLibInfo.pVmPageSizeGetRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmPageSizeGetRtn) ()))

#define VM_TRANSLATE(context, virtualAddr, physicalAddr) \
    ((vmLibInfo.pVmTranslateRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmTranslateRtn) (context, virtualAddr, physicalAddr)))

#define VM_TEXT_PAGE_PROTECT(addr, protect) \
    ((vmLibInfo.pVmTextProtectRtn == NULL) ? \
    (ERROR) : \
    ((*vmLibInfo.pVmTextProtectRtn) (addr, protect)))

IMPORT VM_LIB_INFO vmLibInfo;

/* class definition */

typedef struct vm_mem_mgr_class
    {
    OBJ_CLASS coreClass;
    } VM_CONTEXT_CLASS;

typedef VM_CONTEXT_CLASS *VM_CONTEXT_CLASS_ID;

IMPORT VM_CONTEXT_CLASS_ID vmContextClassId;

/* the following data structure in used internally in mmuLib to define
 * the values for the architecture dependent states
 */

typedef struct
    {
    UINT archIndepMask;
    UINT archDepMask;
    UINT archIndepState;
    UINT archDepState;
    } STATE_TRANS_TUPLE;

/* mmuLib sets up this structure to provide pointers to all its functions */

typedef struct
    {
    FUNCPTR mmuLibInit;
    MMU_TRANS_TBL_ID (*mmuTransTblCreate) ();
    FUNCPTR mmuTransTblDelete;
    FUNCPTR mmuEnable;
    FUNCPTR mmuStateSet;
    FUNCPTR mmuStateGet;
    FUNCPTR mmuPageMap;
    FUNCPTR mmuGlobalPageMap;
    FUNCPTR mmuTranslate;
    VOIDFUNCPTR mmuCurrentSet;
    } MMU_LIB_FUNCS;

#ifdef __cplusplus
}
#endif

#endif /* __INCvmLibPh */
