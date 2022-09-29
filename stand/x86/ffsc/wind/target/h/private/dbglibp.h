/* dbgLibP.h - private debugging facility header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01f,20aug93,dvs  removed declaration of dsmNbytes(), added include dsmLib.h 
		 (SPR #2266).
01e,01oct92,yao  added to pass pEsf, pRegSet to _dbgStepAdd().
01d,22sep92,rrr  added support for c++
01c,21jul92,yao  added declaration of trcDefaultArgs.  changed IMPORT
		 to extern.
01b,06jul92,yao  made user uncallable globals started with '_'.
01a,16jun92,yao  written based on mc68k dbgLib.c ver08f.
*/

#ifndef __INCdbgLibPh
#define __INCdbgLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "dbglib.h"
#include "dsmlib.h" 

/* externals */

extern int    	shellTaskId;
extern char * 	_archHelp_msg;		/* architecture specific help message */
extern int    	trcDefaultArgs;		/* default number of args to print */

extern FUNCPTR 	_dbgDsmInstRtn; 		/* disassembler routine */
extern FUNCPTR 	_dbgAdrsChkRtn; 		/* address check routine */

#if     (BRK_HW_BP != 0)
extern FUNCPTR 	_dbgHwParmGetRtn; 	/* additional param get routine */
#endif /* (BRK_HW_BP != 0) */


#if defined(__STDC__) || defined(__cplusplus)

/* architecture interface routines */

extern void	  _dbgArchInit (void);
extern void	  _dbgVecInit (void);
extern int	  _dbgInstSizeGet (INSTR * brkInst);
extern INSTR *	  _dbgRetAdrsGet (REG_SET * pRegSet);
extern void	  _dbgSStepSet (BREAK_ESF * pInfo);
extern STATUS     dbgBrkAdd (INSTR *addr, int task, int count, int type);
extern BRKENTRY * dbgBrkGet (INSTR * addr, int task, BOOL checkFlag);

#if	(DBG_TRACE == TRUE)
extern void	  _dbgTaskSStepSet (int tid);
#else
extern STATUS 	  _dbgStepAdd (int tid, int brkType, BREAK_ESF *pEsf, int * pRegSet);
#endif 	/* (DBG_TRACE == TRUE) */

extern BOOL	  _dbgFuncCallCheck (INSTR * addr);
extern void	  _dbgTaskPCSet (int tid, INSTR * pc, INSTR * npc);
extern INSTR *	  _dbgTaskPCGet (int tid);
extern void	  _dbgRegsAdjust (int tid, TRACE_ESF * pInfo, int * regs,
			          BOOL stepBreakFlag);
extern void	  _dbgIntrInfoSave (BREAK_ESF * pInfo);
extern void	  _dbgIntrInfoRestore (TRACE_ESF * pInfo);
extern INSTR *	  _dbgInfoPCGet (BREAK_ESF * pInfo);
extern void	  _dbgTaskBPModeSet (int tid);
extern void	  _dbgTaskBPModeClear (int tid);
extern void       _dbgTraceDisable (void);

#if 	(BRK_HW_BP != 0)
extern BOOL       _dbgHwAdrsCheck (INSTR * addr, int access);
extern void       _dbgHwBpSet (INSTR * addr, HWBP * pHwBp);
extern BOOL       _dbgHwBrkExists (INSTR * addr, BRKENTRY * pBrkEntry,
		   		   int access, void * pHwInfo);
extern void       _dbgHwBpClear (HWBP * pHwBp);
extern void       _dbgHwDisplay (BRKENTRY* bp);
extern HWBP *     _dbgHwBpGet (int access, LIST * pHwBpList);
extern void       _dbgArchHwBpFree (HWBP * pHwBp);
extern BOOL       _dbgHwBpCheck (INSTR * pInstr, BRKENTRY * bp, int task,
			         BOOL checkFlag);
#endif 	/* (BRK_HW_BP != 0) */

#else	/* __STDC__ */

/* architecture interface routines */

extern void	  _dbgArchInit ();
extern void	  _dbgVecInit ();
extern int	  _dbgInstSizeGet ();
extern INSTR *	  _dbgRetAdrsGet ();
extern void	  _dbgSStepSet ();
extern STATUS     dbgBrkAdd ();
extern BRKENTRY * dbgBrkGet ();

#if	(DBG_TRACE == TRUE)
extern void	  _dbgTaskSStepSet ();
#else
extern STATUS 	  _dbgStepAdd ();
#endif 	/* (DBG_TRACE == TRUE) */

extern BOOL	  _dbgFuncCallCheck ();
extern void	  _dbgTaskPCSet ();
extern INSTR *	  _dbgTaskPCGet ();
extern void	  _dbgRegsAdjust ();
extern void	  _dbgIntrInfoSave ();
extern void	  _dbgIntrInfoRestore ();
extern INSTR *	  _dbgInfoPCGet ();
extern void	  _dbgTaskBPModeSet ();
extern void	  _dbgTaskBPModeClear ();
extern void       _dbgTraceDisable ();

#if 	(BRK_HW_BP != 0)
extern BOOL       _dbgHwAdrsCheck ();
extern void       _dbgHwBpSet ();
extern BOOL       _dbgHwBrkExists ();
extern void       _dbgHwBpClear ();
extern void       _dbgHwDisplay ();
extern HWBP *     _dbgHwBpGet ();
extern void       _dbgArchHwBpFree ();
extern BOOL       _dbgHwBpCheck ();
#endif 	/* (BRK_HW_BP != 0) */

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCdbgLibPh */
