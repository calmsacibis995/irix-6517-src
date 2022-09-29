/* wdbLibP.h - private header file for remote debug agent */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,03jun95,ms	added prototype for wdbTargetIsConnected()
01c,05apr95,ms  new data types.
01b,06feb95,ms	removes wdbRpcLib specific things.
01a,20sep94,ms  written.
*/

#ifndef __INCwdbRpcLibPh
#define __INCwdbRpcLibPh

/* includes */

#include "wdb/wdb.h"

/* defines */

#define WDB_STATE_EXTERN_RUNNING        1

/* agent variables */

extern uint_t   wdbCommMtu;
extern uint_t	wdbTgtMemBase;
extern uint_t	wdbTgtMemSize;
extern uint_t	wdbTgtNumMemRegions;
extern WDB_MEM_REGION * pWdbTgtMemRegions;

/* function prototypes */

#if defined(__STDC__)

extern void	wdbInfoGet	  (WDB_AGENT_INFO * pInfo);
extern void	wdbNotifyHost	  (void);
extern BOOL	wdbTgtHasFpp	  (void);
extern BOOL	wdbTargetIsConnected (void);

extern STATUS	wdbModeSet	  (int newMode);
extern BOOL	wdbIsNowExternal  (void);
extern BOOL	wdbRunsExternal   (void);
extern BOOL	wdbIsNowTasking   (void);
extern BOOL	wdbRunsTasking	  (void);

extern void	wdbSuspendSystem     (
				     /*
				      WDB_IU_REGS *pRegs,
				      void (*callBack)(),
				      int arg
				     */
				     );
extern void	wdbSuspendSystemHere (void (*callBack)(), int arg);
extern void	wdbResumeSystem      (void);
extern STATUS	wdbExternInfRegsGet  (WDB_REG_SET_TYPE type, char **ppRegs);
extern void     wdbExternEnterHook   (void);
extern void     wdbExternExitHook    (void);

extern UINT32	wdbCtxCreate	     (WDB_CTX_CREATE_DESC *, UINT32 *);
extern UINT32	wdbCtxResume	     (WDB_CTX *);
extern void	wdbCtxExitNotifyHook (WDB_CTX, UINT32, UINT32);

#else	/* __STDC__ */

extern void	wdbInfoGet	  ();
extern void	wdbNotifyHost	  ();
extern BOOL	wdbTgtHasFpp	  ();
extern BOOL	wdbTargetIsConnected ();

extern STATUS	wdbModeSet	  ();
extern BOOL	wdbIsNowExternal  ();
extern BOOL	wdbRunsExternal	  ();
extern BOOL	wdbIsNowTasking   ();
extern BOOL	wdbRunsTasking	  ();

extern void	wdbSuspendSystem  ();
extern void	wdbResumeSystem   ();
extern void	wdbSuspendSystemHere ();
extern STATUS	wdbExternInfRegsGet  ();
extern void     wdbExternEnterHook();
extern void     wdbExternExitHook ();

extern UINT32	wdbCtxCreate	     ();
extern UINT32	wdbCtxResume	     ();
extern void	wdbCtxExitNotifyHook ();

#endif	/* __STDC__ */

#endif /* __INCwdbRpcLibPh */
