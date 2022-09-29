/* vmLib.h - header for architecture independent mmu interface */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01k,26jul94,tpr  added VM_STATE_CACHEABLE_NOT_IMPRECISE for MC68060.
01j,09feb93,rdc  added prototypes for vmBaseStateSet and vmBasePageSizeGet;
01i,19oct92,pme  replaced _STDC__ by __STDC__.
01h,19oct92,jcf  cleanup. added vmContextDelete() prototype.
01g,22sep92,rrr  added support for c++
01f,22sep92,rdc  changed prototype for vmGlobalInfoGet.
		 Added prototype for vmPageBlockSizeGet.
		 Added support for sun1e.
01e,30jul92,rdc  added prototype for vmContextShow.
01d,28jul92,rdc  added non-ansi function prototypes and prototypes for 
		 vmBaseLib.c and vmShow.c.
01c,13jul92,rdc  moved private data structures to vmLibP.h.
01b,09jul92,rdc  added VM_LIB_INFO and PHYS_MEM_DESC.
01a,08jul92,rdc  written.
*/

#ifndef __INCvmLibh
#define __INCvmLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "memlib.h"
#include "lstlib.h"
#include "private/objlibp.h"
#include "private/classlibp.h"
#include "private/semlibp.h"

#include "mmulib.h"

/* status codes */

#define S_vmLib_NOT_PAGE_ALIGNED		(M_vmLib | 1)
#define S_vmLib_BAD_STATE_PARAM			(M_vmLib | 2)
#define S_vmLib_BAD_MASK_PARAM			(M_vmLib | 3)
#define S_vmLib_ADDR_IN_GLOBAL_SPACE		(M_vmLib | 4)
#define S_vmLib_TEXT_PROTECTION_UNAVAILABLE	(M_vmLib | 5)

/* physical memory descriptor is used to map virtual memory in sysLib 
 * and usrConfig.
 */

typedef struct phys_mem_desc
    {
    void *virtualAddr;
    void *physicalAddr;
    UINT len;
    UINT initialStateMask;      /* mask parameter to vmStateSet */
    UINT initialState;          /* state parameter to vmStateSet */
    } PHYS_MEM_DESC;

IMPORT PHYS_MEM_DESC sysPhysMemDesc[];
IMPORT int sysPhysMemDescNumEnt;

typedef struct vm_context
    {
    OBJ_CORE	objCore;	
    MMU_TRANS_TBL_ID mmuTransTbl;   
    SEMAPHORE sem;
    NODE links;
    } VM_CONTEXT;

typedef VM_CONTEXT *VM_CONTEXT_ID;

/* state mask constants for vmStateSet vmStateGet. These
 * constants define the bit fields in the page state. 
 */

#define VM_STATE_MASK_VALID                     0x03
#define VM_STATE_MASK_WRITABLE                  0x0c
#define VM_STATE_MASK_CACHEABLE                 0x30

/* state constants for vmStateSet and vmStateGet.  These
 * values are or'ed together to form the page state.  Additional
 * values may be defined by specific architectures below. 
 */

#define VM_STATE_VALID                          0x01
#define VM_STATE_VALID_NOT                      0x00
#define VM_STATE_WRITABLE                       0x04
#define VM_STATE_WRITABLE_NOT                   0x00
#define VM_STATE_CACHEABLE              	0x10
#define VM_STATE_CACHEABLE_NOT          	0x00

/* additional cache states for MC68040 */

#define VM_STATE_CACHEABLE_WRITETHROUGH		0x20
#define VM_STATE_CACHEABLE_NOT_NON_SERIAL	0x30

/* additional cache states for MC68060 */
#define VM_STATE_CACHEABLE_NOT_IMPRECISE	0x30

/* additional device type states for sun1e */

#define VM_STATE_MASK_DEVICE_SPACE		0xc0

#define VM_STATE_DS_MEM				0x00
#define VM_STATE_DS_IO				0x40
#define VM_STATE_DS_VME_D16			0x80
#define VM_STATE_DS_VME_D32			0xc0

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS		vmLibInit (int pageSize);
extern VM_CONTEXT_ID 	vmGlobalMapInit (PHYS_MEM_DESC *pMemDescArray, 
			  		 int numDescArrayElements, BOOL enable);
extern VM_CONTEXT *	vmContextCreate ();
extern STATUS 		vmContextDelete (VM_CONTEXT_ID context);
extern STATUS 		vmContextInit (VM_CONTEXT *context);
extern STATUS 		vmStateSet (VM_CONTEXT *context, void *pVirtual,
				    int len, UINT stateMask, UINT state);
extern STATUS 		vmStateGet (VM_CONTEXT *context, void *pageAddr,
				    UINT *state);
extern STATUS 		vmMap (VM_CONTEXT *context, void *virtualAddr,
			       void *physicalAddr, UINT len);
extern STATUS 		vmGlobalMap (void *virtualAddr, void *physicalAddr,
				     UINT len);
extern UINT8 *		vmGlobalInfoGet ();
extern int 		vmPageBlockSizeGet ();
extern STATUS 		vmTranslate (VM_CONTEXT *context, void *virtualAddr, 
		    		     void **physicalAddr);
extern int 		vmPageSizeGet ();
extern VM_CONTEXT *	vmCurrentGet ();
extern STATUS 		vmCurrentSet (VM_CONTEXT *context);
extern STATUS 		vmEnable (BOOL enable);
extern STATUS 		vmTextProtect ();

extern STATUS 		vmBaseLibInit (int pageSize);
extern VM_CONTEXT_ID 	vmBaseGlobalMapInit (PHYS_MEM_DESC *pMemDescArray, 
				      	     int numDescArrayElements,
					     BOOL enable);
extern STATUS    	vmBaseStateSet (VM_CONTEXT *context, void *pVirtual, 
				        int len, UINT stateMask, UINT state);
extern int       	vmBasePageSizeGet (void);


	
extern void 		vmShowInit (void);
extern STATUS 		vmContextShow (VM_CONTEXT_ID context);

#else	/* __STDC__ */

extern STATUS 		vmLibInit ();
extern VM_CONTEXT_ID 	vmGlobalMapInit ();
extern VM_CONTEXT *	vmContextCreate ();
extern STATUS 		vmContextInit ();
extern STATUS 		vmStateSet ();
extern STATUS 		vmStateGet ();
extern STATUS 		vmMap ();
extern STATUS 		vmGlobalMap ();
extern UINT8 *		vmGlobalInfoGet ();
extern int 		vmPageBlockSizeGet ();
extern STATUS 		vmTranslate ();
extern int 		vmPageSizeGet ();
extern VM_CONTEXT *	vmCurrentGet ();
extern STATUS 		vmCurrentSet ();
extern STATUS 		vmEnable ();
extern STATUS 		vmTextProtect ();

extern STATUS 		vmBaseLibInit ();
extern VM_CONTEXT_ID	vmBaseGlobalMapInit ();
extern STATUS    	vmBaseStateSet ();
extern int       	vmBasePageSizeGet ();
	
extern void 		vmShowInit ();
extern STATUS 		vmContextShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCvmLibh */
