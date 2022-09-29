/* dbgLib.h - header file for dbgLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01t,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01s,15dec93,hdn  added support for I80X86.
01r,02dec93,pad  Added Am29k family support.
01q,11aug93,gae  vxsim hppa.
01p,20jun93,gae  vxsim.
01o,13nov92,dnw  removed DBG_INFO typedef to taskLib.h (SPR #1768)
01n,22sep92,rrr  added support for c++
01m,25aug92,yao  added function prototypes for dbgBreakNotifyInstall(),
		 dbgStepQuiet(), bdTask().
01l,29jul92,wmd  place #pragma aligns around DBG_INFO for the i960.
01k,10jul92,yao  renamed DBG_STATE to DBG_INFO.  removed dbgMode, pDbgSave,
		 BOOLS resumeTask, sstepTask, sstepQuite, pInterruptBreak.
		 added dbgState to DBG_INFO.  added DBG_TASK_RESUME,
		 DBG_TASK_S_STEP, DBG_TASK_QUIET.  added dbgBrkExists ().
		 added BRK_SINGLE_STEP.  renamed BRK_SO to BRK_STEP_OVER.
01j,06jul92,yao  removed dbgLockCnt in DBG_STATE.
01i,04jul92,jcf  cleaned up.
01h,12mar92,yao  moved TRON related stuff to dbgTRONLib.h. added macros
		 LST_FIRST, LST_NEXT, INST_CMP.  added data structure HWBP,
		 BRKENTRY, DBG_STATE.  added ANSI prototype for architecture
		 interface routines.
01j,26may92,rrr  the tree shuffle
01i,23apr92,wmd  moved include of dbg960Lib.h after defines of structures.
01g,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01f,16dec91,hdn  changed a type of bp->code, from INSTR to int, for G100.
01e,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01d,11sep91,hdn  added things for redesigned dbgLib.c for TRON.
01c,10jun91,del  added pragma for gnu960 alignment.
01b,24mar91,del  added things for redesigned dbgLib.c only available on
		 i960ca.
01a,05oct90,shl created.
*/

#ifndef __INCdbgLibh
#define __INCdbgLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "regs.h"
#include "esf.h"

#if 	CPU_FAMILY == MC680X0
#include "arch/mc68k/dbgmc68klib.h"
#endif 	/* CPU_FAMILY == MC680X0 */

#if 	CPU_FAMILY == I960
#include "arch/i960/dbgi960lib.h"
#endif 	/* CPU_FAMILY == I960 */

#if 	CPU_FAMILY == MIPS
#include "arch/mips/dbgmipslib.h"
#endif 	/* CPU_FAMILY == MIPS */

#if 	CPU_FAMILY == SPARC
#include "arch/sparc/dbgsparclib.h"
#endif 	/* CPU_FAMILY == SPARC */

#if 	CPU_FAMILY == SIMSPARCSUNOS
#include "arch/simsparc/dbgsimsparclib.h"
#endif 	/* CPU_FAMILY == SIMSPARCSUNOS */

#if 	CPU_FAMILY == SIMHPPA
#include "arch/simhppa/dbgsimhppalib.h"
#endif 	/* CPU_FAMILY == SIMHPPA */

#if	CPU_FAMILY==I80X86
#include "arch/i86/dbgi86lib.h"
#endif	/* CPU_FAMILY==I80X86 */

#if 	CPU_FAMILY == AM29XXX
#include "arch/am29k/dbgam29klib.h"
#endif 	/* CPU_FAMILY == AM29XXX */

#ifndef _ASMLANGUAGE

#include "lstlib.h"

/* Macro's are used by routines executing at interrupt level (task switch),
 * so that fatal breakpoints are avoided.
 */
#define LST_FIRST(pList) (((LIST *)(pList))->node.next)
#define LST_NEXT(pNode)  (((NODE *)(pNode))->next)

#define	INST_CMP(addr,inst,mask)  ((*(addr) & (mask)) == (inst))

#define ALL		0	/* breakpoint applies to all tasks */
#define LAST		0	/* continue applies to task that last broke */
#define MAX_ARGS	8	/* max args to task level call */

/* trace exception type */

#define CONTINUE	0	/* trace exception is due to a continue */
#define SINGLE_STEP	1	/* trace exception is due to a single-step */
#define STEP_FROM_BREAK	2	/* trace due to a single-step from breakpoint */

/* dbg state bits */

#define DBG_TASK_RESUME		1	/* task proceding from breakpoint */
#define DBG_TASK_S_STEP		2	/* single tepping task */
#define DBG_TASK_QUIET		4	/* quite single stepping task */
#define DBG_TRACE_MODE		8	/* set trace mode */

/* type of break-point software */

#define BRK_NORM	0x1	/* normal breakpoint type */
#define BRK_STEP_OVER	0x2	/* step-over breakpoint type */
#define BRK_NORM_QUIET	0x4	/* normal breakpoint type, no console output */

#ifndef BRK_TEMP
#define BRK_TEMP	0x8
#endif

#define BRK_SINGLE_STEP	0x10	/* single step breakpoint type */

#ifndef BRK_HW_BP
#define BRK_HW_BP	0x0		/* default no hardware breakpoint */
#endif  /* BRK_HW_BP */

#ifndef HW_REGS_NUM
#define HW_REGS_NUM	0		/* default no hardware breakpoint */
#endif  /* HW_REGS_NUM */

#ifndef	DBG_TRACE
#define DBG_TRACE	TRUE		/* default to support trace mode */
#endif	/* DBG_TRACE */

#ifndef	DBG_TT
#define DBG_TT		TRUE		/* default to support stack trace */
#endif	/* DBG_TT */

#ifndef	DBG_CRET
#define DBG_CRET	TRUE		/* default to support cret */
#endif	/* DBG_CRET */

#ifndef	DBG_TRAP_NUM
#define DBG_TRAP_NUM	2
#endif

#ifndef	DBG_INST_ALIGN
#define	DBG_INST_ALIGN	4		/* default to long word alignment */
#endif

#if 	(BRK_HW_BP != 0)
typedef struct hwbp
    {
    NODE	hbNode;		/* link list node */
    int		hbAccess;	/* breakpoint type */
    int 	hbRegNum;	/* register number */
    void *	pHwInfo;	/* pointer to arch specific infomation */
    } HWBP;
#endif /* (BRK_HW_BP != 0) */

typedef struct brkEntry	/* BRKENTRY: breakpoint table entry */
    {
    NODE 	node;	/* link list node */
    INSTR *     addr;	/* breakpoint address */
    INSTR 	code;	/* code that goes at breakpoint address */
    UINT32   	task;	/* task for which breakpoint valid (0 = all tasks) */
    UINT32   	count;	/* pass count */
    UINT32 	type;	/* kind of breakpoint */

#if	BRK_HW_BP
    HWBP *	pHwBp;	/* pointer to hardware breakpoint */
#endif /* BRK_HW_BP */

    } BRKENTRY;


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)
extern STATUS 	c (int taskNameOrId, INSTR * addr, INSTR * addr1);
extern STATUS 	s (int taskNameOrId, INSTR * addr, INSTR * addr1);

extern void 	dbgHelp (void);
extern STATUS 	dbgInit (void);
extern STATUS 	b (INSTR * addr, int taskNameOrId, int count, BOOL quiet);

#if	(BRK_HW_BP != 0)
extern STATUS 	bh (INSTR * addr, int access, int taskNameOrId,
		    int count, BOOL quiet);
#endif	/* (BRK_HW_BP != 0) */

extern STATUS 	bd (INSTR * addr, int taskNameOrId);
extern STATUS 	bdall (int taskNameOrId);
extern STATUS 	cret (int taskNameOrId);
extern STATUS 	so (int taskNameOrId);
extern void 	l (INSTR * addr, int count);
extern STATUS 	tt (int taskNameOrId);

extern void 	dbgPrintCall (INSTR * callAdrs, int funcAdrs, int nargs,
			      UINT32 * pArgs);
extern BOOL	dbgBrkExists (INSTR * addr, int task);
extern void	dbgBreakNotifyInstall (FUNCPTR breakpointRtn);
extern STATUS	dbgStepQuiet (int task);
extern STATUS	bdTask (int task);

#else

extern void 	dbgHelp ();
extern STATUS 	dbgInit ();
extern STATUS 	b ();

#if	(BRK_HW_BP != 0)
extern STATUS 	bh ();
#endif	/* (BRK_HW_BP != 0) */

extern STATUS 	bd ();
extern STATUS 	bdall ();
extern STATUS 	c ();
extern STATUS 	cret ();
extern STATUS 	s ();
extern STATUS 	so ();
extern void 	l ();
extern STATUS 	tt ();

extern void 	dbgPrintCall ();
extern BOOL	dbgBrkExists ();
extern void	dbgBreakNotifyInstall ();
extern STATUS	dbgStepQuiet ();
extern STATUS	bdTask ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */
#ifdef __cplusplus
}
#endif

#endif /* __INCdbgLibh */
