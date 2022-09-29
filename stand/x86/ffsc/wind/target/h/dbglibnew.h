/* dbgLibNew.h - header file for dbgLibNew.c */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,01nov94,kdl  fixed __INCdbgLibNewh symbols.
01g,27aug93,hdn  changed function declaration c(), s().
01f,13apr92,hdn  changed a type of bp->code, from int to INSTR.
01e,16dec91,hdn  changed a type of bp->code, from INSTR to int, for G100.
01d,30oct91,hdn  added things for redesigned dbgLib.c for TRON.
01c,10jun91,del  added pragma for gnu960 alignment. 
01b,24mar91,del  added things for redesigned dbgLib.c only available on
		i960ca.
01a,05oct90,shl created.
*/

#ifndef __INCdbgLibNewh
#define __INCdbgLibNewh

#ifdef __cplusplus
extern "C" {
#endif

#include "regs.h"
#include "esf.h"

#if	CPU_FAMILY==I80X86
#include "arch/i86/dbgi86lib.h"
#endif	/* CPU_FAMILY==I80X86 */


#ifndef _ASMLANGUAGE
#include "lstlib.h"

/* Macro's are used by routines executing at interrupt level (task switch),
 * so that fatal breakpoints are avoided.
 */
#define LST_FIRST(pList) (((LIST *)(pList))->node.next)
#define LST_NEXT(pNode)  (((NODE *)(pNode))->next)

#define ALL		0	/* breakpoint applies to all tasks */

#define	DSM(addr,inst,mask)	((*(addr) & (mask)) == (inst))

/* type of trace exception */

#define CONTINUE	0	/* trace exception is due to a continue */
#define SINGLE_STEP	1	/* trace exception is due to a single-step */
#define STEP_FROM_BREAK	2	/* trace due to a single-step from breakpoint */

/* dbg state bits */

#define DBG_TASK_RESUME	1	/* task proceding from breakpoint */
#define DBG_TASK_S_STEP	2	/* single tepping task */
#define DBG_TASK_QUIET	4	/* quite single stepping task */
#define DBG_TRACE_MODE	8	/* set trace mode */

/* type of break-point software/hardware... */

#define BRK_NORM	0x01	/* normal breakpoint type */
#define BRK_SO		0x02	/* step-over breakpoint type */
#define BRK_NORM_QUIET	0x04	/* normal breakpoint type, no console output */
#define BRK_TEMP	0x08	/* temp breakpoint type */
#define BRK_TEMP_INT	0x10	/* temp breakpoint type for interrupt */


#ifndef	DBG_HARDWARE
#define DBG_HARDWARE	TRUE		/* default to support hardware bp */
#endif	/* DBG_HARDWARE */

#ifndef	DBG_TRACE
#define DBG_TRACE	TRUE		/* default to support trace mode */
#endif	/* DBG_TRACE */

#ifndef	DBG_TT
#define DBG_TT		TRUE		/* default to support stack trace */
#endif	/* DBG_TT */

#ifndef	DBG_CRET
#define DBG_CRET	TRUE		/* default to support cret */
#endif	/* DBG_CRET */

#ifndef	DBG_INST_ALIGN
#define	DBG_INST_ALIGN	4		/* default to long word alignment */
#endif


typedef struct brkEntry	/* BRKENTRY: breakpoint table entry */
    {
    NODE  node;		/* link list node */
    INSTR *addr;	/* breakpoint address */
    INSTR code;		/* code that goes at breakpoint address */
    int   task;		/* task for which breakpoint valid (0 = all tasks) */
    int   count;	/* pass count */
    int   type;		/* kind of breakpoint */
    INSTR *addr0;	/* 2nd breakpoint address */
    } BRKENTRY;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	    c (int taskNameOrId, INSTR * addr, INSTR * addr1);
extern STATUS 	    s (int taskNameOrId, INSTR * addr, INSTR * addr1);
extern void	    dbgHelp (void);
extern STATUS       dbgInit (void);
extern STATUS       b (INSTR *addr, int taskNameOrId, int count, BOOL quiet);
extern STATUS       bd (INSTR *addr, int taskNameOrId);
extern STATUS       bdall (int taskNameOrId);
extern STATUS       cret (int taskNameOrId);
extern STATUS       so (int taskNameOrId);
extern void	    l (INSTR *addr, int count);
extern STATUS       tt (int taskNameOrId);
extern void         dbgPrintCall (INSTR * callAdrs, int funcAdrs, int nargs,
			          UINT32 * pArgs);
extern BOOL         dbgBrkExists (INSTR * addr, int task);
extern void         dbgBreakNotifyInstall (FUNCPTR breakpointRtn);
extern STATUS       dbgStepQuiet (int task);
extern STATUS       bdTask (int task);

#if	DBG_HARDWARE==TRUE
extern STATUS       bh (INSTR *addr, int taskNameOrId, int count, int type, 
			INSTR *addr0);
#endif	/* DBG_HARDWARE==TRUE */

#else

extern STATUS 	    c ();
extern STATUS 	    s ();
extern void	    dbgHelp ();
extern STATUS       dbgInit ();
extern STATUS       b ();
extern STATUS       bd ();
extern STATUS       bdall ();
extern STATUS       cret ();
extern STATUS       so ();
extern void	    l ();
extern STATUS       tt ();
extern void         dbgPrintCall ();
extern BOOL         dbgBrkExists ();
extern void         dbgBreakNotifyInstall ();
extern STATUS       dbgStepQuiet ();
extern STATUS       bdTask ();

#if	DBG_HARDWARE==TRUE
extern STATUS       bh ();
#endif	/* DBG_HARDWARE==TRUE */

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCdbgLibNewh */
