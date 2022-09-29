/* taskLibP.h - private task library interface header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,19mar95,dvs  removing tron references.
01i,28jul94,dvs  added extern of pTaskLastFpTcb. (SPR #3033)
01h,12nov93,hdn  added support for I80X86
01g,02dec93,pme  added Am29K family support.
01f,03aug93,jwt  abstracted common WIND_TCB_XXX definitions; copyright.
01e,22sep92,rrr  added support for c++
01d,23aug92,jcf  added extern of taskBpHook.
01c,28jul92,jcf  changed offsets to match WIND_TCB.
01b,07jul92,ajm  removed external defines of taskSwapMaskClear/taskSwapMaskSet
01a,04jul92,jcf  written by extracting from taskLib.h v02x.
*/

#ifndef __INCtaskLibPh
#define __INCtaskLibPh

#ifdef __cplusplus
extern "C" {
#endif

/* task status values */

#define WIND_READY		0x00	/* ready to run */
#define WIND_SUSPEND		0x01	/* explicitly suspended */
#define WIND_PEND		0x02	/* pending on semaphore */
#define WIND_DELAY		0x04	/* task delay (or timeout) */
#define WIND_DEAD		0x08	/* dead task */

#ifndef	_ASMLANGUAGE
#include "vxworks.h"
#include "tasklib.h"
#include "classlib.h"
#include "objlib.h"

/* variable declarations */

extern REG_INDEX taskRegName [];
extern FUNCPTR	taskBpHook;
extern FUNCPTR	taskCreateTable [];
extern FUNCPTR	taskSwitchTable [];
extern FUNCPTR 	taskDeleteTable [];
extern FUNCPTR 	taskSwapTable [];
extern int	taskSwapReference [];
extern WIND_TCB * pTaskLastFpTcb;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int	taskCreat (char *name, int priority, int options, int stackSize,
		      	   FUNCPTR entryPt, int arg1, int arg2, int arg3,
		      	   int arg4, int arg5, int arg6, int arg7,
		      	   int arg8, int arg9, int arg10);
extern STATUS	taskTerminate (int tid);
extern STATUS	taskDestroy (int tid, BOOL dealloc, int timeout,
			     BOOL forceDestroy);
extern STATUS	taskUndelay (int tid);
extern void	taskBpHookSet (FUNCPTR bpHook);
extern void	taskIdListSort (int idList[], int nTasks);
extern int	taskCreate (char *name, int priority, int options,
			    char *pStackBase, int stackSize, WIND_TCB *pTcb,
			    FUNCPTR entryPt, int arg1, int arg2, int arg3,
			    int arg4, int arg5, int arg6, int arg7,
			    int arg8, int arg9, int arg10);

#else	/* __STDC__ */

extern int	taskCreat ();
extern STATUS	taskTerminate ();
extern STATUS	taskDestroy ();
extern STATUS	taskUndelay ();
extern void	taskIdListSort ();
extern void	taskBpHookSet ();
extern int	taskCreate ();

#endif	/* __STDC__ */

#else	/* _ASMLANGUAGE */

/* architecture-independent WIND_TCB defines */

#define	WIND_TCB_STATUS		0x3c
#define	WIND_TCB_PRIORITY	0x40
#define	WIND_TCB_PRI_NORMAL	0x44
#define	WIND_TCB_MUTEX_CNT	0x48
#define	WIND_TCB_LOCK_CNT	0x50
#define	WIND_TCB_SWAP_IN	0x58
#define	WIND_TCB_SWAP_OUT	0x5a
#define	WIND_TCB_SAFE_CNT	0x60
#define	WIND_TCB_SAFETY_Q_HEAD	0x64
#define	WIND_TCB_ENTRY		0x74
#define WIND_TCB_ERRNO		0x84

/* architecture-dependent WIND_TCB defines */

#if CPU_FAMILY==MC680X0
#define	WIND_TCB_REGS		0x12c
#define	WIND_TCB_DREGS		0x12c
#define	WIND_TCB_DREGS4		0x130
#define	WIND_TCB_DREGS8		0x134
#define	WIND_TCB_AREGS		0x14c
#define	WIND_TCB_AREGS4		0x150
#define	WIND_TCB_AREGS8		0x154
#define	WIND_TCB_SSP		0x168
#define	WIND_TCB_FRAME1		0x16e
#define	WIND_TCB_SR		0x16e
#define	WIND_TCB_PC		0x170
#define	WIND_TCB_FRAME2		0x172
#define	WIND_TCB_FOROFF		0x174
#endif	/* CPU_FAMILY==MC680X0 */

#if CPU_FAMILY==I960
#define	WIND_TCB_REGS		0x78

#define	TCB_REG_SET_BASE 	0x140
#define	TCB_REG_SET_SIZE 	0x8c
#define	WIND_TCB_R0   		TCB_REG_SET_BASE + 0x00
#define	WIND_TCB_PFP  		TCB_REG_SET_BASE + 0x00
#define	WIND_TCB_SP   		TCB_REG_SET_BASE + 0x04
#define	WIND_TCB_RIP  		TCB_REG_SET_BASE + 0x08
#define	WIND_TCB_R3   		TCB_REG_SET_BASE + 0x0c
#define	WIND_TCB_R4   		TCB_REG_SET_BASE + 0x10
#define	WIND_TCB_R8   		TCB_REG_SET_BASE + 0x20
#define	WIND_TCB_R12  		TCB_REG_SET_BASE + 0x30
#define	WIND_TCB_G0   		TCB_REG_SET_BASE + 0x40
#define	WIND_TCB_G4   		TCB_REG_SET_BASE + 0x50
#define	WIND_TCB_G8   		TCB_REG_SET_BASE + 0x60
#define	WIND_TCB_G12  		TCB_REG_SET_BASE + 0x70
#define	WIND_TCB_FP   		TCB_REG_SET_BASE + 0x7c
#define	WIND_TCB_PCW  		TCB_REG_SET_BASE + 0x80
#define	WIND_TCB_ACW  		TCB_REG_SET_BASE + 0x84
#define	WIND_TCB_TCW  		TCB_REG_SET_BASE + 0x88

#define WIND_TCB_DBG_STATE_PTR	TCB_REG_SET_BASE + 0x8c
#define WIND_TCB_RESUMPTION_RECORD	TCB_REG_SET_BASE + 0x90
#endif /* CPU_FAMILY==I960 */

#if     CPU_FAMILY==MIPS
#define WIND_TCB_REGS           0x138
#include "arch/mips/taskmipslib.h"
#endif	/* MIPS */

#if	(CPU_FAMILY == SPARC)
#define	WIND_TCB_REGS	0x130
#include "arch/sparc/regssparc.h"
#endif	/* (CPU_FAMILY == SPARC) */

#if     CPU_FAMILY==I80X86
#define WIND_TCB_REGS           0x12c
#define WIND_TCB_EDI            0x12c
#define WIND_TCB_ESI            0x130
#define WIND_TCB_EBP            0x134
#define WIND_TCB_ESP            0x138
#define WIND_TCB_EBX            0x13c
#define WIND_TCB_EDX            0x140
#define WIND_TCB_ECX            0x144
#define WIND_TCB_EAX            0x148
#define WIND_TCB_EFLAGS         0x14c
#define WIND_TCB_PC             0x150
#endif  /* CPU_FAMILY==I80X86 */

#if     CPU_FAMILY==AM29XXX
#define WIND_TCB_STATUS         0x3c
#define WIND_TCB_PRIORITY       0x40
#define WIND_TCB_PRI_NORMAL     0x44
#define WIND_TCB_MUTEX_CNT      0x48
#define WIND_TCB_LOCK_CNT       0x50
#define WIND_TCB_SWAP_IN        0x58
#define WIND_TCB_SWAP_OUT       0x5a
#define WIND_TCB_SAFE_CNT       0x60
#define WIND_TCB_SAFETY_Q_HEAD  0x64
#define WIND_TCB_ENTRY          0x74
#define WIND_TCB_ERRNO          0x84
#define WIND_TCB_REGS           0x13c

#include "arch/am29k/taskam29klib.h"
#endif	/* AM29XXX */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCtaskLibPh */
