/* usrWdb.c - configuration file for the WDB agent */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01s,24oct95,ms	moved wdbBpSet here.
01r,16oct95,ms  host mem pool created via static buffer instead of malloc
01q,11oct95,ms  turned off character echoing in TTY_TEST to fix SPR 5116
01p,21sep95,ms  macro shuffle + fix for SPR 4936.
01o,31aug95,ms  INCLUDE_WDB_TTY_TEST works if started before kernel (SPR #4808)
01n,23aug95,ms  bump intCnt to fake ISR context in extern agents extern hook
01m,28jun95,tpr added NETROM_TASK_POLL_DELAY macro.
01l,23jun95,ms	changed logMsg to _func_logMsg
01k,21jun95,ms	added taskRestart for the agent on exception.
01j,20jun95,tpr added wdbMemCoreLibInit().
01i,20jun95,ms	moved gopher buffer back to gopherLib.o for DSA
01h,19jun95,ms	moved event lib initialization near the begining of wdbConfig
		fixed vxTaskDeleteHookAdd to return OK or ERROR
		added initialization for INCLUDE_WDB_TTY_TEST
01g,15jun95,ms	updated for new serial drivers.
01f,07jun95,ms	exit hook no longer uses the spare TCB field
		memory size based on sysMemTop() instead of LOCAL_MEM_SIZE
		added some scalability
		WDB_STACK_SIZE used for both task and system agent.
		WDB_COMM_TYCO changed to WDB_COMM_TTY
01e,01jun95,ms	added taskLock/unlock.
		pass buffer to wdbGopherLibInit().
		decreace WDB_MTU when going over serial line.
		changed MTU->WDB_MTU, EXC_NOTIFY->WDB_EXC_NOTIFY,
			MAX_SERVICES->WDB_MAX_SERVICES,
			AGENT_POOL_SIZE->WDB_POOL_SIZE.
01d,25may95,ms	added fpp support for the system agent.
01c,23may95,ms	added some include files.
01b,17jan95,ms  cleaned up.
01a,21sep94,ms  written.
*/

/*
DESCRIPTION

This library configures and initializes the WDB agent.
The only user callable routine is wdbConfig(). This routine
initializes the agents OS function pointers, the communication
function pointers, and then the agent itself.
*/

#include "vxworks.h"
#include "syslib.h"
#include "stdlib.h"
#include "vxlib.h"
#include "tasklib.h"
#include "taskhooklib.h"
#include "fpplib.h"
#include "intlib.h"
#include "rebootlib.h"
#include "bootlib.h"
#include "version.h"
#include "cachelib.h"
#include "exclib.h"
#include "config.h"
#include "string.h"
#include "buflib.h"
#include "siolib.h"
#include "private/tasklibp.h"
#include "private/vmlibp.h"
#include "private/funcbindp.h"
#include "wdb/wdb.h"
#include "wdb/wdblib.h"
#include "wdb/wdblibp.h"
#include "wdb/wdbbplib.h"
#include "wdb/wdbsvclib.h"
#include "wdb/wdbudplib.h"
#include "wdb/wdbudpsocklib.h"
#include "wdb/wdbtycodrv.h"
#include "wdb/wdbrtiflib.h"
#include "wdb/wdbcommiflib.h"
#include "wdb/wdbmbuflib.h"
#include "wdb/wdbrpclib.h"
#include "wdb/wdbregs.h"
#include "drv/wdb/wdbviodrv.h"
#include "drv/wdb/wdbslippktdrv.h"
#include "drv/wdb/wdbulippktdrv.h"
#include "drv/wdb/wdbnetrompktdrv.h"

/* defines */

#define	NUM_MBUFS		5
#define MILLION			1000000
#define MAX_LEN			100
#define DUALPORT_SIZE		2048
#define	INCLUDE_VXWORKS_KERNEL		/* don't remove this */
#define	WDB_RESTART_TIME	10
#define	WDB_MAX_RESTARTS	5
#define WDB_BP_MAX              50      /* max # of break points */
#define WDB_MAX_SERVICES        50      /* max # of agent services */
#define WDB_TASK_PRIORITY       3       /* priority of task agent */
#define WDB_TASK_OPTIONS        VX_UNBREAKABLE  /* agent options */

/* lower WDB_MTU to SLMTU bytes for serial connection */

#if	(WDB_COMM_TYPE == WDB_COMM_TYCODRV_5_2) || \
	(WDB_COMM_TYPE == WDB_COMM_SERIAL)
#if	WDB_MTU > SLMTU
#undef	WDB_MTU
#define	WDB_MTU	SLMTU
#endif	/* WDB_MTU > SLMTU */
#endif

/* lower WDB_MTU to NETROM_MTU for netrom connections */

#if	(WDB_COMM_TYPE == WDB_COMM_NETROM)
#if	WDB_MTU > NETROM_MTU
#undef	WDB_MTU
#define	WDB_MTU	NETROM_MTU
#endif	/* WDB_MTU > NETROM_MTU */
#endif

/* lower WDB_MTU to ULIP_MTU for ULIP connections */

#if	(WDB_COMM_TYPE == WDB_COMM_ULIP)
#if	WDB_MTU > ULIP_MTU
#undef	WDB_MTU
#define	WDB_MTU	ULIP_MTU
#endif	/* WDB_MTU > ULIP_MTU */
#endif

/* change agent mode to task mode for NETWORK or TYCODRV_5_2 connections */

#if	(WDB_COMM_TYPE == WDB_COMM_TYCODRV_5_2) || (WDB_COMM_TYPE == WDB_COMM_NETWORK)
#undef	WDB_MODE
#define	WDB_MODE	WDB_MODE_TASK
#endif

/* globals */

uint_t		 wdbCommMtu	= WDB_MTU;
int		 wdbNumMemRegions = 0;	/* number of extra memory regions */
WDB_MEM_REGION * pWdbMemRegions = NULL;	/* array of regions */

/* locals */

static BUF_POOL wdbMbufPool;
static char *	hostPoolBase;
static u_int	hostPoolSize;
static char	vxBootFile [MAX_LEN];
static WDB_RT_IF wdbRtIf;

/*
 * These are private - but configurable size arrays can't be malloc'ed
 * in external mode, so we define them here.
 */

#if     (WDB_MODE & WDB_MODE_EXTERN)
static uint_t	wdbExternStackArray [WDB_STACK_SIZE/sizeof(uint_t)];
#endif

static WDB_SVC		wdbSvcArray       [WDB_MAX_SERVICES];
static uint_t		wdbSvcArraySize = WDB_MAX_SERVICES;

#ifdef INCLUDE_WDB_BP
static struct breakpoint wdbBreakPoints [WDB_BP_MAX];
#endif

#if	(WDB_MODE == WDB_MODE_EXTERN)
static char		wdbHostPool [WDB_POOL_SIZE];
#endif

/* forward static declarations */

static void wdbMbufInit ();
static void wdbCommIfInit ();
static void wdbRtIfInit ();

/******************************************************************************
*
* wdbConfig - configure and initialize the WDB agent.
*
*/

STATUS wdbConfig (void)
    {
    STATUS	status1 = OK;
    STATUS	status2 = OK;

#if     (WDB_MODE & WDB_MODE_EXTERN)
    caddr_t	pExternStack;
#endif	/* WDB_MODE & WDB_MODE_EXTERN */

    /* Initialize the agents interface function pointers */

    wdbRtIfInit   ();		/* run-time interface functions */
    wdbCommIfInit ();		/* communication interface functions */


    /* Install some agent services */

    wdbSvcLibInit (wdbSvcArray, wdbSvcArraySize);

    wdbConnectLibInit ();	/* required agent service */

    wdbMemCoreLibInit ();	/* required agent service */

#ifdef	INCLUDE_WDB_MEM
    wdbMemLibInit ();		/* extra memory services */
#endif	/* INCLUDE_WDB_MEM */

#ifdef	INCLUDE_WDB_EVENTS
    wdbEventLibInit();
#endif	/* INCLUDE_WDB_EVENTS */

#ifdef  INCLUDE_WDB_DIRECT_CALL
    wdbDirectCallLibInit ();
#endif  /* INCLUDE_WDB_DIRECT_CALL */

#ifdef	INCLUDE_WDB_CTXT
    wdbCtxLibInit ();
#endif	/* INCLUDE_WDB_CTXT */

#ifdef	INCLUDE_WDB_REG
    wdbRegsLibInit ();
#endif	/* INCLUDE_WDB_REG */

#ifdef	INCLUDE_WDB_GOPHER
    wdbGopherLibInit();
#endif	/* INCLUDE_WDB_GOPHER */

#ifdef	INCLUDE_WDB_EXIT_NOTIFY
    wdbCtxExitLibInit();
#endif	/* INCLUDE_WDB_EXIT_NOTIFY */

#ifdef	INCLUDE_WDB_EXC_NOTIFY
    wdbExcLibInit();
#endif	/* INCLUDE_WDB_EXC_NOTIFY */

#ifdef	INCLUDE_WDB_FUNC_CALL
    wdbFuncCallLibInit ();
#endif	/* INCLUDE_WDB_FUNC_CALL */

#ifdef	INCLUDE_WDB_VIO
    wdbVioLibInit();
    wdbVioDrv("/vio");
#endif	/* INCLUDE_WDB_VIO */

#ifdef  INCLUDE_WDB_BP
    wdbSysBpLibInit (wdbBreakPoints, WDB_BP_MAX);
#if	(WDB_MODE & WDB_MODE_TASK)
    wdbTaskBpLibInit ();
#endif	/* WDB_MODE & WDB_MODE_TASK */
#endif	/* INCLUDE_WDB_BP */


    /* Initialize the agent(s) */

#if	(WDB_MODE & WDB_MODE_TASK)
    status1 = wdbTaskInit (WDB_TASK_PRIORITY,
			WDB_TASK_OPTIONS, NULL, WDB_STACK_SIZE);
#endif

#if	(WDB_MODE & WDB_MODE_EXTERN)
#if	_STACK_DIR == _STACK_GROWS_DOWN
    pExternStack = (caddr_t)&wdbExternStackArray
				[WDB_STACK_SIZE/sizeof(uint_t)];
    pExternStack = (caddr_t)STACK_ROUND_DOWN (pExternStack);
#else	/* _STACK_DIR == _STACK_GROWS_UP */
    pExternStack = (caddr_t)wdbExternStackArray;
    pExternStack = (caddr_t)STACK_ROUND_UP (pExternStack);
#endif	/* _STACK_DIR == _STACK_GROWS_DOWN */
    status2 = wdbExternInit (pExternStack);

#ifdef	INCLUDE_HW_FP
    if (wdbTgtHasFpp())
	{
	WDB_REG_SET_OBJ * pFpRegs;
	pFpRegs = wdbFpLibInit();
	wdbExternRegSetObjAdd (pFpRegs);
	}
#endif	/* INCLUDE_HW_FP */
#endif	/* WDB_MODE & WDB_MODE_EXTERN */


    /* activate one agent only */

#if	(WDB_MODE & WDB_MODE_TASK)
    wdbModeSet (WDB_MODE_TASK);
#else
    wdbModeSet (WDB_MODE_EXTERN);
#endif


    return (status1 && status2);
    }

/******************************************************************************
*
* wdbExternEnterHook - hook to call when external agent is entered.
*/

void wdbExternEnterHook (void)
    {
    intCnt++;			/* always fake an interrupt context */
#ifdef	INCLUDE_WDB_BP
    wdbBpRemove ();
#endif
    }

/******************************************************************************
*
* wdbExternExitHook - hook to call when the external agent resumes the system.
*/

void wdbExternExitHook (void)
    {
    intCnt--;			/* restore original intCnt value */
#ifdef  INCLUDE_WDB_BP
    wdbBpInstall ();
#endif
    }

/******************************************************************************
*
* wdbTgtHasFpp - TRUE if target has floating point support.
*/

bool_t wdbTgtHasFpp (void)
    {
#ifdef	INCLUDE_HW_FP
    if (fppProbe() == OK)
	return (TRUE);
    return (FALSE);
#else
    return (FALSE);
#endif
    }



/******************************************************************************
*
* wdbRtInfoGet - get info on the VxWorks run time system.
*/

void wdbRtInfoGet
    (
    WDB_RT_INFO *	pRtInfo
    )
    {
    pRtInfo->rtType	= WDB_RT_VXWORKS;
    pRtInfo->rtVersion	= vxWorksVersion;
    pRtInfo->cpuType	= CPU;
    pRtInfo->hasFpp	= wdbTgtHasFpp();
#ifdef	INCLUDE_PROTECT_TEXT
    pRtInfo->hasWriteProtect	= (vmLibInfo.pVmTextProtectRtn != NULL);
#else	/* !INCLUDE_PROTECT_TEXT */
    pRtInfo->hasWriteProtect	= FALSE;
#endif	/* !INCLUDE_PROTECT_TEXT */
    pRtInfo->pageSize   = VM_PAGE_SIZE_GET();
    pRtInfo->endian	= _BYTE_ORDER;
    pRtInfo->bspName	= sysModel();

    pRtInfo->bootline	= vxBootFile;

    pRtInfo->memBase	= (char *)(LOCAL_MEM_LOCAL_ADRS);
    pRtInfo->memSize	= (int)sysMemTop() - (int)LOCAL_MEM_LOCAL_ADRS;
    pRtInfo->numRegions	= wdbNumMemRegions;
    pRtInfo->memRegion	= pWdbMemRegions;

    pRtInfo->hostPoolBase = hostPoolBase;
    pRtInfo->hostPoolSize = hostPoolSize;
    }

/******************************************************************************
*
* vxReboot - reboot the system.
*/

void vxReboot (void)
    {
    reboot (0);
    }

/******************************************************************************
*
* vxMemProtect - protect a region of memory.
*/

STATUS vxMemProtect
    (
    char * addr,
    u_int  nBytes,
    bool_t protect		/* TRUE = protect, FALSE = unprotect */
    )
    {
    return (VM_STATE_SET (NULL, addr, nBytes, VM_STATE_MASK_WRITABLE, 
                     (protect ? VM_STATE_WRITABLE_NOT : VM_STATE_WRITABLE)));
    }

#ifdef	INCLUDE_VXWORKS_KERNEL
/******************************************************************************
*
* vxTaskCreate - WDB callout to create a task (and leave suspended).
*/

static int vxTaskCreate
    (
    char *   name,       /* name of new task (stored at pStackBase)   */
    int      priority,   /* priority of new task                      */
    int      options,    /* task option word                          */
    caddr_t  stackBase,  /* base of stack. ignored by VxWorks	      */
    int      stackSize,  /* size (bytes) of stack needed plus name    */
    caddr_t  entryPt,    /* entry point of new task                   */
    int      arg[10],	 /* 1st of 10 req'd task args to pass to func */
    int      fdIn,	 /* fd for input redirection		      */
    int      fdOut,	 /* fd for output redirection		      */
    int      fdErr	 /* fd for error output redirection	      */
    )
    {
    int tid;

    if (stackSize == 0)
	stackSize = WDB_SPAWN_STACK_SIZE;

    tid = taskCreat (name, priority, options, stackSize, (int (*)())entryPt,
		arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6],
		arg[7], arg[8], arg[9]);

#ifdef	INCLUDE_IO_SYSTEM
    if (tid != ERROR)
	{
	if (fdIn != 0)
	    ioTaskStdSet (tid, 0, fdIn);
	if (fdOut != 0)
	    ioTaskStdSet (tid, 1, fdOut);
	if (fdErr != 0)
	    ioTaskStdSet (tid, 2, fdErr);
	}
#endif	/* INCLUDE_IO_SYSTEM */

    return (tid);
    }


/******************************************************************************
*
* vxTaskResume - WDB callout to resume a suspended task.
*/

static STATUS vxTaskResume
    (
    WDB_CTX *	pContext
    )
    {
    if (pContext->contextType != WDB_CTX_TASK)
	return (ERROR);

    return (taskResume (pContext->contextId));
    }

/******************************************************************************
*
* vxTaskSuspend - WDB callout to suspend a task.
*/

static STATUS vxTaskSuspend
    (
    WDB_CTX *	pContext
    )
    {
    if (pContext->contextType != WDB_CTX_TASK)
	return (ERROR);

    return (taskSuspend (pContext->contextId));
    }

/******************************************************************************
*
* vxTaskDelete - WDB callout to delete a task.
*/

static STATUS vxTaskDelete
    (
    WDB_CTX *	pContext
    )
    {
    if (pContext->contextType != WDB_CTX_TASK)
	return (ERROR);

    return (taskDelete (pContext->contextId));
    }

/******************************************************************************
*
* vxTaskLock - disable preemption.
*/

static void vxTaskLock (void)
    {
    taskLock();
    }

/******************************************************************************
*
* vxTaskUnlock - reenable preemption.
*/ 

static void vxTaskUnlock (void)
    {
    taskUnlock();
    }

/******************************************************************************
*
* vxTaskRegsSet - WDB callout to get a task register set.
*/

static STATUS vxTaskRegsSet
    (
    WDB_CTX *	 pContext,
    WDB_REG_SET_TYPE regSetType,
    char *	 pRegSet
    )
    {
    STATUS		status;

    if (pContext->contextType != WDB_CTX_TASK)
	return (ERROR);

    switch (regSetType)
	{
	case WDB_REG_SET_IU:
	    status = taskRegsSet (pContext->contextId, (REG_SET *)pRegSet);
	    break;

#ifdef  INCLUDE_HW_FP
	case WDB_REG_SET_FPU:
	    status = fppTaskRegsSet (pContext->contextId, (FPREG_SET *)pRegSet);
	    break;
#endif	/* INCLUDE_HW_FP */

	default:
	    status = ERROR;
	}

    return (status);
    }

/******************************************************************************
*
* vxTaskRegsGet - WDB callout to get a tasks register set.
*
* This routine is not reentrant, but it it only called by one thread (the
* WDB agent).
*/

static STATUS vxTaskRegsGet
    (
    WDB_CTX *		pContext,
    WDB_REG_SET_TYPE 	regSetType,
    char **		ppRegSet
    )
    {
    STATUS		status;

    static union
	{
	REG_SET		iuRegs;
	FPREG_SET	fpRegs;
	} regSet;

    if (pContext->contextType != WDB_CTX_TASK)
	return (ERROR);

    switch (regSetType)
	{
	case WDB_REG_SET_IU:
	    status = taskRegsGet (pContext->contextId, &regSet.iuRegs);
	    break;

#ifdef  INCLUDE_HW_FP
	case WDB_REG_SET_FPU:
	    status = fppTaskRegsGet (pContext->contextId, &regSet.fpRegs);
	    break;
#endif	/* INCLUDE_HW_FP */

	default:
	    status = ERROR;
	}

    *ppRegSet = (char *)&regSet;

    return (status);
    }

/******************************************************************************
*
* vxSemCreate - create a SEMAPHORE
*/

void * vxSemCreate (void)
    {
    return ((void *)semBCreate (0, 0));
    }

/******************************************************************************
*
* vxSemGive - give a semaphore
*/

STATUS vxSemGive
    (
    void * semId
    )
    {
    return (semGive ((SEM_ID)semId));
    }

/******************************************************************************
*
* vxSemTake - take a semaphore
*/

STATUS vxSemTake
    (
    void *		semId,
    struct timeval *	tv
    )
    {
    return (semTake ((SEM_ID) semId, 
	(tv == NULL ? WAIT_FOREVER :
	tv->tv_sec * sysClkRateGet() +
	(tv->tv_usec * sysClkRateGet()) / MILLION)));
    }
#endif	/* INCLUDE_VXWORKS_KERNEL */

/******************************************************************************
*
* vxExcHookAdd -
*/

static void (*vxExcHook)();

int vxExcHookWrapper (int vec, char *pESF, WDB_IU_REGS *pRegs)
    {
    WDB_CTX	context;
    static int	restartCnt;
    extern int	wdbTaskId;

    if (INT_CONTEXT() || wdbIsNowExternal() || (taskIdCurrent == 0))
	context.contextType = WDB_CTX_SYSTEM;
    else
	context.contextType = WDB_CTX_TASK;
    context.contextId	= (int)taskIdCurrent;

    (*vxExcHook)(context, vec, pESF, pRegs);

    /*
     * if the exception is in the agent task, restart the agent
     * after a delay.
     */

    if (((int)taskIdCurrent == wdbTaskId) && (restartCnt < WDB_MAX_RESTARTS))
	{
	restartCnt++;
	if (_func_logMsg != NULL)
	    _func_logMsg ("WDB exception. restarting agent in %d seconds...\n",
		WDB_RESTART_TIME, 0,0,0,0,0);
	taskDelay (sysClkRateGet() * WDB_RESTART_TIME);
	taskRestart (0);
	}

    return (FALSE);
    }

void vxExcHookAdd
    (
    void	(*hook)()
    )
    {
    vxExcHook = hook;

    _func_excBaseHook = vxExcHookWrapper;
    }

/******************************************************************************
*
* __wdbTaskDeleteHook -
*/ 

int __wdbTaskDeleteHook
    (
    WIND_TCB *pTcb
    )
    {
    WDB_CTX	ctx;
    void	(*hook)();

    hook = pTcb->wdbExitHook;

    if (hook != NULL)
	{
	ctx.contextType	= WDB_CTX_TASK;
	ctx.contextId	= (UINT32)pTcb;
	(*hook) (ctx, pTcb->exitCode, pTcb->errorStatus);
	}

    return (OK);
    }

/******************************************************************************
*
* vxTaskDeleteHookAdd - task-specific delete hook (one per task).
*
* currently only one hook per task.
*/ 

STATUS vxTaskDeleteHookAdd
    (
    UINT32	tid,
    void	(*hook)()
    )
    {
    static initialized = FALSE;

    if (taskIdVerify ((int)tid) == ERROR)
	return (ERROR);

    taskTcb (tid)->wdbExitHook = hook;

    if (!initialized)
	{
	taskDeleteHookAdd (__wdbTaskDeleteHook);
	initialized = TRUE;
	}

    return (OK);
    }

/******************************************************************************
*
* wdbRtIfInit - Initialize pointers to the VxWorks routines.
*/

static void wdbRtIfInit ()
    {
    static	initialized;
    int 	ix;
    WDB_RT_IF *	pRtIf = &wdbRtIf;

    bzero ((char *)pRtIf, sizeof (WDB_RT_IF));

    pRtIf->rtInfoGet	= wdbRtInfoGet;

    pRtIf->reboot	= vxReboot;

    pRtIf->cacheTextUpdate = (void (*)())cacheLib.textUpdateRtn;

    pRtIf->memProtect   = vxMemProtect;
    pRtIf->memProbe	= (STATUS (*)())vxMemProbe;

    pRtIf->excHookAdd	= vxExcHookAdd;

#ifdef	INCLUDE_VXWORKS_KERNEL
    pRtIf->taskCreate	= vxTaskCreate;
    pRtIf->taskResume	= vxTaskResume;
    pRtIf->taskSuspend	= vxTaskSuspend;
    pRtIf->taskDelete	= vxTaskDelete;

    pRtIf->taskLock	= vxTaskLock;
    pRtIf->taskUnlock	= vxTaskUnlock;

    pRtIf->taskRegsSet	= vxTaskRegsSet;
    pRtIf->taskRegsGet  = vxTaskRegsGet;

    pRtIf->malloc	= malloc;
    pRtIf->free		= free;

    pRtIf->semCreate	= vxSemCreate;
    pRtIf->semGive	= vxSemGive;
    pRtIf->semTake	= vxSemTake;

    pRtIf->taskDeleteHookAdd	= vxTaskDeleteHookAdd;
    pRtIf->taskSwitchHookAdd	= (STATUS (*)())taskSwitchHookAdd;
    pRtIf->taskCreateHookAdd	= (STATUS (*)())taskCreateHookAdd;
#endif	/* INCLUDE_VXWORKS_KERNEL */

    for (ix = 0; ix < MAX_LEN; ix ++)
	{
	if (*(sysBootLine + ix) == ')')
	    {
	    ix++;
	    break;
	    }
	}

    bcopy (sysBootLine + ix, vxBootFile, MAX_LEN - ix);

    for (ix = 0; ix < MAX_LEN - 1; ix ++)
	{
	if (*(vxBootFile + ix) == ' ')
	    break;
	}
    *(vxBootFile + ix) = '\0';

    if (!initialized)
	{
#if	(WDB_MODE == WDB_MODE_EXTERN)
	hostPoolBase = wdbHostPool;
	hostPoolSize = WDB_POOL_SIZE;
#else	/* WDB_MODE & WDB_MODE_TASK */
	hostPoolBase = (char *)malloc(WDB_POOL_SIZE);
	hostPoolSize = WDB_POOL_SIZE;
#endif
	}

    wdbInstallRtIf (pRtIf);

    initialized = TRUE;
    }


/******************************************************************************
*
* wdbCommIfInit - 
*/

static void wdbCommIfInit ()
    {
    static uint_t	wdbInBuf	  [WDB_MTU/4];
    static uint_t	wdbOutBuf	  [WDB_MTU/4];

    static WDB_XPORT	wdbXport;
    static WDB_COMM_IF	wdbCommIf;

    WDB_COMM_IF * pCommIf = &wdbCommIf;

    wdbMbufInit ();

#if	(WDB_COMM_TYPE == WDB_COMM_NETWORK)
    /* UDP sockets - supports a task agent */

    wdbUdpSockIfInit (pCommIf);
#endif	/* (WDB_COMM_TYPE == WDB_COMM_NETWORK) */

#if	(WDB_COMM_TYPE == WDB_COMM_TYCODRV_5_2)
    {
    /* SLIP lite built on a VxWorks serial driver - supports a task agent */

    static WDB_TYCO_SIO_CHAN tyCoSioChan;	/* serial I/O device */
    static WDB_SLIP_PKT_DEV  wdbSlipPktDev;	/* SLIP packet device */

    wdbTyCoDevInit	(&tyCoSioChan, WDB_TTY_DEV_NAME, WDB_TTY_BAUD);

#ifdef	INCLUDE_WDB_TTY_TEST
    wdbSioTest ((SIO_CHAN *)&tyCoSioChan, SIO_MODE_INT, 0);
#endif	/* INCLUDE_WDB_TTY_TEST */

    wdbSlipPktDevInit	(&wdbSlipPktDev, (SIO_CHAN *)&tyCoSioChan, udpRcv);
    udpCommIfInit	(pCommIf, &wdbSlipPktDev.wdbDrvIf);
    }
#endif	/* (WDB_COMM_TYPE == WDB_COMM_TYCODRV_5_2) */

#if	(WDB_COMM_TYPE == WDB_COMM_ULIP)
    {
    /* ULIP packet driver (VxSim only) - supports task or external agent */

    static WDB_ULIP_PKT_DEV	wdbUlipPktDev;	/* ULIP packet device */

    wdbUlipPktDevInit (&wdbUlipPktDev, WDB_ULIP_DEV, udpRcv);
    udpCommIfInit     (pCommIf, &wdbUlipPktDev.wdbDrvIf);
    }
#endif	/* (WDB_COMM_TYPE == WDB_COMM_ULIP) */

#if	(WDB_COMM_TYPE == WDB_COMM_SERIAL)
    {
    /* SLIP-lite over a raw serial channel - supports task or external agent */

    SIO_CHAN *			pSioChan;	/* serial I/O channel */
    static WDB_SLIP_PKT_DEV	wdbSlipPktDev;	/* SLIP packet device */

    pSioChan = sysSerialChanGet (WDB_TTY_CHANNEL);
    sioIoctl (pSioChan, SIO_BAUD_SET, (void *)WDB_TTY_BAUD);

#ifdef	INCLUDE_WDB_TTY_TEST
    /* test in polled mode if the kernel hasn't started */

    if (taskIdCurrent == 0)
	wdbSioTest (pSioChan, SIO_MODE_POLL, 0);
    else
	wdbSioTest (pSioChan, SIO_MODE_INT, 0);
#endif	/* INCLUDE_WDB_TTY_TEST */

    wdbSlipPktDevInit	(&wdbSlipPktDev, pSioChan, udpRcv);
    udpCommIfInit	(pCommIf, &wdbSlipPktDev.wdbDrvIf);
    }
#endif	/* (WDB_COMM_TYPE == WDB_COMM_SERIAL) */

#if     (WDB_COMM_TYPE == WDB_COMM_NETROM)
    {
    /* netrom packet driver - supports task or external agent */

    int dpOffset;				/* offset of dualport RAM */
    static WDB_NETROM_PKT_DEV	wdbNetromPktDev; /* NETROM packet device */

    dpOffset = (ROM_SIZE - DUALPORT_SIZE) * WDB_NETROM_WIDTH;

    wdbNetromPktDevInit (&wdbNetromPktDev, (caddr_t)ROM_BASE_ADRS + dpOffset,
			 WDB_NETROM_WIDTH, WDB_NETROM_INDEX,
			 WDB_NETROM_NUM_ACCESS, udpRcv,
			 WDB_NETROM_POLL_DELAY);

    udpCommIfInit       (pCommIf, &wdbNetromPktDev.wdbDrvIf);
    }
#endif  /* (WDB_COMM_TYPE == WDB_COMM_NETROM) */

#if     (WDB_COMM_TYPE == WDB_COMM_CUSTOM)
    {
    /* custom packet driver - supports task or external agent */

    static WDB_CUSTOM_PKT_DEV	wdbCustomPktDev; /* custom packet device */

    wdbCustomPktDevInit (&wdbCustomPktDev, udpRcv);
    udpCommIfInit       (pCommIf, &wdbCustomPktDev.wdbDrvIf);
    }
#endif  /* (WDB_COMM_TYPE == WDB_COMM_CUSTOM) */


    /*
     * Install the agents communication interface and RPC transport handle.
     * Currently only one agent will be active at a time, so both
     * agents can share the same communication interface and XPORT handle.
     */

    wdbRpcXportInit  (&wdbXport, pCommIf, (char *)wdbInBuf,
		      (char *)wdbOutBuf, WDB_MTU);
    wdbInstallCommIf (pCommIf, &wdbXport);
    }



/******************************************************************************
*
* wdbMbufInit - initialize the agent's mbuf memory allocator.
*
* wdbMbufLib manages I/O buffers for the agent since the agent
* can't use malloc().
*
* If the agent is ever hooked up to a network driver that uses standard
* MGET/MFREE for mbuf managment, then the routines wdbMBufAlloc()
* and wdbMBufFree() below should be changed accordingly.
*/ 

void wdbMbufInit (void)
    {
    static struct mbuf mbufs[NUM_MBUFS];
    bufPoolInit (&wdbMbufPool, (char *)mbufs, NUM_MBUFS, sizeof (struct mbuf));
    }

/******************************************************************************
*
* wdbMbufAlloc - allocate an mbuf
*
* RETURNS: a pointer to an mbuf, or NULL on error.
*/ 

struct mbuf *	wdbMbufAlloc (void)
    {
    struct mbuf * pMbuf;

    pMbuf = (struct mbuf *)bufAlloc (&wdbMbufPool);
    if (pMbuf == NULL)
	return (NULL);

    pMbuf->m_next	= NULL;
    pMbuf->m_act	= NULL;
    return (pMbuf);
    }

/******************************************************************************
*
* wdbMbufFree - free an mbuf
*/ 

void wdbMbufFree
    (
    struct mbuf *	pMbuf		/* mbuf chain to free */
    )
    {
    /* if it is a cluster, see if we need to perform a callback */

    if (pMbuf->m_ctype == MC_UCLUSTER)
	{
	if ((--(*pMbuf->m_refcnt) <= 0) && (pMbuf->m_cfreeRtn != NULL))
	    (*pMbuf->m_cfreeRtn) (pMbuf->m_arg1, pMbuf->m_arg2, pMbuf->m_arg3);
	}

    bufFree (&wdbMbufPool, (char *)pMbuf);
    }

/******************************************************************************
*
* wdbBpSet - set a breakpoint.
*/ 

void wdbBpSet
    (
    INSTR *addr,
    INSTR value
    )
    {
    void * pageAddr;
    int    pageSize;

    pageSize = VM_PAGE_SIZE_GET();
    pageAddr = (void *) ((UINT) addr & ~(pageSize - 1));

    VM_TEXT_PAGE_PROTECT(pageAddr, FALSE);
    *addr = value;
    VM_TEXT_PAGE_PROTECT(pageAddr, TRUE);

    CACHE_TEXT_UPDATE (addr, sizeof (INSTR));
    }

