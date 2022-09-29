/* funcBindP.h - private function binding header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01q,12may95,p_m  added _func_printErr, _func_symFindByValue, _func_spy*
                       _func_taskCreateHookAdd and _func_taskDeleteHookAdd.
01p,24jan94,smb  added function pointers for windview portable kernel.
01o,10dec93,smb  added function pointers for windview.
01n,05sep93,jcf  added _remCurId[SG]et.
01m,20aug93,jmm  added _bdall
01l,22jul93,jmm  added _netLsByName
01k,13feb93,kdl  added _procNumWasSet.
01j,13nov92,jcf  added _func_logMsg.
01i,22sep92,rrr  added support for c++
01h,20sep92,kdl  added _func_ftpLs, ftpErrorSuppress.
01g,31aug92,rrr  added _func_sigprocmask
01f,23aug92,jcf  added _func_sel*, _func_excJobAdd,_func_memalign,_func_valloc
01e,02aug92,jcf  added/changed _exc*.
01d,29jul92,jcf  added _func_fclose
01c,29jul92,rrr  added _func_sigExcKill, _func_sigTimeoutRecalc,
                 _func_excEsfCrack and _func_excSuspend.
01b,19jul92,pme  added _func_smObjObjShow.
01a,04jul92,jcf  written
*/

#ifndef __INCfuncBindPh
#define __INCfuncBindPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "semlib.h"

/* variable declarations */

extern FUNCPTR     _func_bdall;
extern FUNCPTR     _func_excBaseHook;
extern FUNCPTR     _func_excInfoShow;
extern FUNCPTR     _func_excIntHook;
extern FUNCPTR     _func_excJobAdd;
extern FUNCPTR     _func_excPanicHook;
extern FUNCPTR     _func_fclose;
extern FUNCPTR     _func_fppTaskRegsShow;
extern FUNCPTR     _func_ftpLs;
extern FUNCPTR     _func_netLsByName;
extern FUNCPTR     _func_printErr;
extern FUNCPTR     _func_logMsg;
extern FUNCPTR     _func_memalign;
extern FUNCPTR     _func_selTyAdd;
extern FUNCPTR     _func_selTyDelete;
extern FUNCPTR     _func_selWakeupAll;
extern FUNCPTR     _func_selWakeupListInit;
extern VOIDFUNCPTR _func_sigExcKill;
extern FUNCPTR     _func_sigprocmask;
extern FUNCPTR     _func_sigTimeoutRecalc;
extern FUNCPTR     _func_smObjObjShow;
extern FUNCPTR     _func_spy;
extern FUNCPTR     _func_spyStop;
extern FUNCPTR     _func_spyClkStart;
extern FUNCPTR     _func_spyClkStop;
extern FUNCPTR     _func_spyReport;
extern FUNCPTR     _func_spyTask;
extern FUNCPTR     _func_symFindByValueAndType;
extern FUNCPTR     _func_symFindByValue;
extern FUNCPTR     _func_taskCreateHookAdd;
extern FUNCPTR     _func_taskDeleteHookAdd;
extern FUNCPTR     _func_valloc;
extern FUNCPTR     _func_remCurIdGet;
extern FUNCPTR     _func_remCurIdSet;

extern BOOL	   ftpErrorSuppress;
extern BOOL	   _procNumWasSet;

extern VOIDFUNCPTR _func_evtLogO;
extern VOIDFUNCPTR _func_evtLogOIntLock;

extern VOIDFUNCPTR _func_evtLogM0;
extern VOIDFUNCPTR _func_evtLogM1;
extern VOIDFUNCPTR _func_evtLogM2;
extern VOIDFUNCPTR _func_evtLogM3;

extern VOIDFUNCPTR _func_evtLogT0;
extern VOIDFUNCPTR _func_evtLogT1;
extern VOIDFUNCPTR _func_evtLogTSched;

extern VOIDFUNCPTR _func_evtLogString;
extern FUNCPTR     _func_evtLogPoint;

extern VOIDFUNCPTR _func_scrPadToBuffer;
extern VOIDFUNCPTR _func_evtBufferCheck;

extern FUNCPTR     _func_tmrStamp;
extern FUNCPTR     _func_tmrStampLock;
extern FUNCPTR     _func_tmrFreq;
extern FUNCPTR     _func_tmrPeriod;
extern FUNCPTR     _func_tmrConnect;
extern FUNCPTR     _func_tmrEnable;
extern FUNCPTR     _func_tmrDisable;

extern BOOL   wvInstIsOn;             /* windview instrumentation ON/OFF */
extern BOOL   wvObjIsEnabled;         /* Level 1 event collection enable */
extern BOOL   evtLogTIsOn;            /* event collection ON/OFF */
extern BOOL   evtLogOIsOn;            /* Level 1 event collection ON/OFF */
extern BOOL   evtBufPostMortem;       /* post mortum mode on */
extern BOOL   evtBufOverflow;         /* buffer overflow error */
extern BOOL   evtBufIsEmpty;          /* full buffer indicator */
extern SEM_ID evtBufSem;              /* event buffer semaphore */

extern char * pScrPad;                   /* scratch-pad address */
extern char * pScrPadIndex;              /* scratch-pad Index address */

extern char * pEvtDblBuffers;            /* double buffer address */

#ifdef __cplusplus
}
#endif

#endif /* __INCfuncBindPh */
