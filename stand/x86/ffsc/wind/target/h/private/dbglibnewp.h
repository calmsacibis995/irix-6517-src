/* dbgLibNewP.h - private debugging facility header */

/* Copyright 1984-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,01nov94,kdl  general cleanup.
01c,14dec93,hdn  added archHelp_msg.
		 deleted #include "tasklib.h"
01b,12nov93,hdn  removed declaration of dsmNbytes(), added include dsmLib.h 
		 (SPR #2266).
01a,27aug93,hdn  written for 386.
*/

#ifndef __INCdbgLibNewPh
#define __INCdbgLibNewPh

#ifdef __cplusplus
extern "C" {
#endif

#include "dbglibnew.h"
#include "dsmlib.h" 
#include "tasklib.h" 

/* externals */

extern int    	shellTaskId;
extern int    	trcDefaultArgs;		/* default number of args to print */
extern char * 	_archHelp_msg;		/* architecture specific help message */
extern FUNCPTR 	_dbgDsmInstRtn; 	/* disassembler routine */

/* architecture interface routines */

#if defined(__STDC__) || defined(__cplusplus)

extern void	_dbgArchInit (void);
extern void	_dbgArchInstall (void);
extern STATUS	_dbgArchAddrCheck (INSTR * addr, int type);
extern void	_dbgArchIntSave (BREAK_ESF * info);
extern void	_dbgArchIntRestore (TRACE_ESF * info);
extern void	_dbgArchDebugMode (WIND_TCB * pNewTcb, BOOL breakMode, 
				   BOOL traceMode);
extern void	_dbgArchRegsGet (TRACE_ESF * info, int * regs, 
				 REG_SET * pRegSet, BOOL traceException);
extern INSTR *	_dbgArchCret (FAST REG_SET * pRegSet);
extern STATUS	_dbgArchSo (FAST INSTR * pc, int * pLength);
extern void	_dbgArchMessage (BREAK_ESF * info, int * regs, 
				 BOOL traceException);
extern void	_dbgTaskPCSet (int task, INSTR * pc, INSTR * npc);
extern INSTR *	_dbgTaskPCGet (int tid);
extern INSTR *	_dbgInfoPCGet (BREAK_ESF * pInfo);

#if	DBG_HARDWARE==TRUE
extern STATUS	_dbgBrkEntryHard (BRKENTRY * bp, DBG_REGS * pReg);
extern BRKENTRY * _dbgBrkGetHard (BREAK_ESF * info, int tid, int * regs);
extern void	_dbgBrkDisplayHard (BRKENTRY * bp);
extern void	_dbgRegsShow (int tid);
extern void	_dbgRegsSet (int * pReg);
extern void	_dbgRegsGet (int * pReg);
#endif	/* DBG_HARDWARE==TRUE */

#if	DBG_TRACE==FALSE
extern STATUS	_dbgStepAdd (int task, int type, BREAK_ESF * info, int * regs);
#endif	/* DBG_TRACE==FALSE */


#else	/* __STDC__ */


extern void	_dbgArchInit ();
extern void	_dbgArchInstall ();
extern STATUS	_dbgArchAddrCheck ();
extern void	_dbgArchIntSave ();
extern void	_dbgArchIntRestore ();
extern void	_dbgArchDebugMode ();
extern void	_dbgArchRegsGet ();
extern INSTR *	_dbgArchCret ();
extern STATUS	_dbgArchSo ();
extern void	_dbgArchMessage ();
extern void	_dbgTaskPCSet ();
extern INSTR *	_dbgTaskPCGet ();
extern INSTR *	_dbgInfoPCGet ();

#if	DBG_HARDWARE==TRUE
extern STATUS	_dbgBrkEntryHard ();
extern BRKENTRY * _dbgBrkGetHard ();
extern void	_dbgBrkDisplayHard ();
extern void	_dbgRegsShow ();
extern void	_dbgRegsSet ();
extern void	_dbgRegsGet ();
#endif	/* DBG_HARDWARE==TRUE */

#if	DBG_TRACE==FALSE
extern STATUS	_dbgStepAdd ();
#endif	/* DBG_TRACE==FALSE */


#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCdbgLibNewPh */
