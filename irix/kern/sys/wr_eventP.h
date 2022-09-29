
/* eventP.h - event log header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01p,22feb95,rdc  added EVENT_TASK_STATECHANGE.
01o,01feb95,rdc  added EVENT_TIMER_SYNC.
01n,27jan95,rdc  added processor number to EVENT_CONFIG.
01m,28nov94,rdc  changed 32 bit store macros for x86.
01l,02nov94,rdc  added protocol revision to EVENT_CONFIG.  
		 added lockCnt param to EVT_CTX_TASKINFO.
01k,02nov94,rdc  added 960 macros.
01j,27may94,pad  added alignment macros. Corrected some parameter comments.
01i,14apr94,smb  optimised EVT_OBJ_ macros.
01h,04mar94,smb  added EVENT_INT_EXIT_K 
		 changed parameters for EVENT_WIND_EXIT_NODISPATCH
		 removed EVENT_WIND_EXIT_IDLENOT
		 level 1 event optimisations
		 added macro for shared memory objects
01h,22feb94,smb  changed typedef EVENT_TYPE to event_t (SPR #3064)
01g,16feb94,smb  introduced new OBJ_ macros, SPR #2982
01f,26jan94,smb  redid pseudo events - generated host side
01e,24jan94,smb  added EVENT_WIND_EXIT_DISPATCH_PI and 
		 EVENT_WIND_EXIT_NODISPATCH_PI
		 added event logging macros
01d,17dec93,smb  changed typedef of EVENT_TYPE
01c,10dec93,smb  modified MAX_LEVEL3_ID to include interrupt events
	  	 renamed EVENT_CLOCK_CONFIG to EVENT_CONFIG
01b,08dec93,kdl  added include of classLibP.h; made def of OBJ_INST_FUNC
	   +smb	 conditional on OBJ_INST_RTN; reformatted macro comments;
		 made includes and obj macros depend on CPU (i.e. not host).
01a,07dec93,smb  written.
*/

#ifndef __INCeventph
#define __INCeventph

/* This file contains all windview event identifiers */

/* types */

#ifndef _ASMLANGUAGE

typedef unsigned int event_t;

#ifdef	CPU				/* only for target, not host */

#include "private/funcBindP.h"
#include "private/classLibP.h"
#include "private/objLibP.h"


/* needed to handle redefinition of obj_core helpRtn field to instRtn 
 * for instrumented kernel
 */

#ifdef OBJ_INST_RTN

#define OBJ_EVT_RTN(objId)					\
    (((OBJ_ID)(objId))->pObjClass)->instRtn

#define TASK_EVT_RTN(tId)					\
    (((OBJ_ID)(&((WIND_TCB *)(tId))->objCore))->pObjClass)->instRtn

#else  /* OBJ_INST_RTN */

#define OBJ_EVT_RTN(objId)					\
    (((OBJ_ID)(objId))->pObjClass)->helpRtn

#define TASK_EVT_RTN(tId)					\
    (((OBJ_ID)(&((WIND_TCB *)(tId))->objCore))->pObjClass)->helpRtn

#endif /* OBJ_INST_RTN */

/* event logging macros */

/******************************************************************************
*
* OBJ_INST_FUNC - check if object is instrumented
*
* RETURNS: instrumentation routine if instrumented, otherwise NULL
* NOMANUAL
*/

#define OBJ_INST_FUNC(objId, classId)   			\
    ((evtLogOIsOn && ((OBJ_VERIFY (objId, classId)) == OK)) 	\
     ? (OBJ_EVT_RTN (objId)) : (NULL)) 

/******************************************************************************
*
* TASK_INST_FUNC - check if task is instrumented
*
*
* RETURNS: instrumentation routine if instrumented, otherwise NULL
* NOMANUAL
*/

#define TASK_INST_FUNC(tid, classId)  				\
    ((evtLogOIsOn && ((TASK_ID_VERIFY(tid)) == OK))		\
     ? (TASK_EVT_RTN (tid)) : (NULL))

/******************************************************************************
*
* EVT_CTX_0 - context switch event logging with event id 
*
* NOMANUAL
*/

#define EVT_CTX_0(evtId)                                        \
    do                                                          \
        {                                                       \
        if (evtLogTIsOn == TRUE)                                \
             ( * _func_evtLogT0) (evtId);                       \
        } while (0)

/******************************************************************************
*
* EVT_CTX_1 - context switch event logging with one parameter
*
* NOMANUAL
*/

#define EVT_CTX_1(evtId, arg)                                   \
    do                                                          \
        {                                                       \
        if (evtLogTIsOn == TRUE)                                \
             ( * _func_evtLogT1) (evtId, arg);                  \
        } while (0)

/******************************************************************************
*
* EVT_CTX_TASKINFO - context switch event logging with task information
*
* This macro stores information into the event buffer about a task
* in the system.
*
* NOMANUAL
*/

#define EVT_CTX_TASKINFO(evtId, state, pri, lockCnt, tid, name)          \
    do                                                          \
        {                                                       \
        if (evtLogTIsOn == TRUE)                                \
	     ( * _func_evtLogString) (evtId, state, pri, lockCnt, tid, name); \
        } while (0)

/******************************************************************************
*
* EVT_CTX_BUF - context switch event logging which logs a buffer,
*
* This macro stores eventpoint and user events into the event buffer.
*
* NOMANUAL
*/

#define EVT_CTX_BUF(evtId, addr, bufSize, BufferAddr)		\
    do                                                          \
        {                                                       \
        if (evtLogTIsOn == TRUE)                               	\
             ( * _func_evtLogPoint) (evtId, addr, bufSize, BufferAddr); \
        } while (0)

/******************************************************************************
*
* EVT_TASK_0 - task state transition event logging with event id.
*
*
* NOMANUAL
*/

#define EVT_TASK_0(evtId)                                       \
    do                                                          \
        {                                                       \
        if (_func_evtLogM0 != NULL)                             \
            (* _func_evtLogM0) (evtId);                         \
        } while (0)

/******************************************************************************
*
* EVT_TASK_1 - task state transition event logging with one argument.
*
*
* NOMANUAL
*/

#define EVT_TASK_1(evtId, arg)                                  \
    do                                                          \
        {                                                       \
        if (_func_evtLogM1 != NULL)                             \
            (* _func_evtLogM1) (evtId, arg);                    \
        } while (0)

/******************************************************************************
*
* EVT_TASK_2 - task state transition event logging with two arguments.
*
*
* NOMANUAL
*/

#define EVT_TASK_2(evtId, arg1, arg2)                           \
    do                                                          \
        {                                                       \
        if (_func_evtLogM2 != NULL)                             \
            (* _func_evtLogM2) (evtId, arg1, arg2);             \
        } while (0)

/******************************************************************************
*
* EVT_TASK_3 - task state transition event logging with three arguments.
*
*
* NOMANUAL
*/

#define EVT_TASK_3(evtId, arg1, arg2, arg3)                     \
    do                                                          \
        {                                                       \
        if (_func_evtLogM3 != NULL)                             \
            (* _func_evtLogM3) (evtId, arg1, arg2, arg3);       \
        } while (0)

/******************************************************************************
*
* EVT_OBJ_6 - object status event logging with six arguments.
*
*
* NOMANUAL
*/

#define EVT_OBJ_6(objType, objId, objClassId, evtId, arg1, arg2, arg3,  \
		     arg4, arg5, arg6)					\
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__) (evtId, arg1, arg2, arg3, arg4, arg5, arg6);	\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_5 - object status event logging with five arguments.
*
*
* NOMANUAL
*/

#define EVT_OBJ_5(objType, objId, objClassId, evtId, arg1, arg2, arg3,  \
		     arg4, arg5)					\
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__) (evtId, arg1, arg2, arg3, arg4, arg5);	\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_4 - object status event logging with four arguments.
*
*
* NOMANUAL
*/

#define EVT_OBJ_4(objType, objId, objClassId, evtId, arg1, arg2, arg3, arg4)\
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__) (evtId, arg1, arg2, arg3, arg4);           	\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_3 - object status event logging with three arguments.
*
*
* NOMANUAL
*/

#define EVT_OBJ_3(objType, objId, objClassId, evtId, arg1, arg2, arg3)  \
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__) (evtId, arg1, arg2, arg3);			\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_2 - object status event logging with two arguments.
*
*
* NOMANUAL
*/

#define EVT_OBJ_2(objType, objId, objClassId, evtId, arg1, arg2)	\
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__)(evtId, arg1, arg2);				\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_1 - object status event logging with an argument.
*
*
* NOMANUAL
*/

#define EVT_OBJ_1(objType, objId, objClassId, evtId, arg1)		\
    do                                                          	\
        {                                                       	\
	VOIDFUNCPTR __evtRtn__;						\
        if ((__evtRtn__ = (objType##_INST_FUNC(objId,objClassId))) != NULL)\
            (* __evtRtn__) (evtId, arg1);				\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_TASKSPAWN - object status event logging for a spawned task.
*
*
* NOMANUAL
*/

#define EVT_OBJ_TASKSPAWN(evtId, arg1, arg2, arg3, arg4, arg5)  \
    do                                                          \
        {                                                       \
	if (evtLogOIsOn)					\
	    ( * _func_evtLogOIntLock) (evtId, arg1, arg2, arg3, arg4, arg5); \
        } while (0)

/******************************************************************************
*
* EVT_OBJ_SIG - signal event logging
*
*
* NOMANUAL
*/

#define EVT_OBJ_SIG(evtId, arg1, arg2)				\
    do                                                          \
        {                                                       \
        if ((evtLogOIsOn) && (sigEvtRtn != NULL))		\
            (* sigEvtRtn) (evtId, arg1, arg2);			\
        } while (0)

/******************************************************************************
*
* INST_SIG_INSTALL - install event logging routine for signals
*
*
* NOMANUAL
*/

#define INST_SIG_INSTALL					\
    do                                                          \
        {                                                       \
        if (evtLogOIsOn)                  			\
            sigEvtRtn = _func_evtLogOIntLock;			\
        else							\
            sigEvtRtn = NULL;					\
        } while (0)

/******************************************************************************
*
* EVT_OBJ_SM_MSGQ - object status event logging for SMO message queues.
*
*
* NOMANUAL
*/

#define EVT_OBJ_SM_MSGQ(evtId, arg1, arg2, arg3, arg4, arg5, arg6)     \
    do                                                          \
        {                                                       \
        if (_func_evtLogOIntLock != NULL)			\
            (* _func_evtLogOIntLock) (evtId, arg1, arg2, arg3,  \
				      arg4, arg5, arg6);        \
        } while (0)


#endif	/* CPU */			/* end target-only section */
#endif  /* _ASMLANGUAGE */

#define WV_REV_ID 0xb0b00000
#define WV_EVT_PROTO_REV 2
#define WV_EVT_PROTO_REV_1_0_FCS 1


/* ranges for the different classes of events */

#define MIN_CONTROL_ID 	0
#define MAX_CONTROL_ID	49
#define MIN_LEVEL3_ID	50
#define MAX_LEVEL3_ID	599
#define MIN_INT_ID	102
#define MAX_INT_ID	599
#define MIN_LEVEL2_ID	600
#define MAX_LEVEL2_ID	9999
#define MIN_LEVEL1_ID	10000
#define MAX_LEVEL1_ID	19999 
#define MIN_RESERVE_ID	20000
#define MAX_RESERVE_ID	39999 
#define MIN_USER_ID	40000
#define MAX_USER_ID	65535

#define INT_LEVEL(eventNum) ((eventNum)-MIN_INT_ID)

#define IS_LEVEL1_EVENT(event) \
    ((event >= MIN_LEVEL1_ID) && (event <= MAX_LEVEL1_ID))

#define IS_LEVEL2_EVENT(event) \
    ((event >= MIN_LEVEL2_ID) && (event <= MAX_LEVEL2_ID))

#define IS_LEVEL3_EVENT(event) \
    ((event >= MIN_LEVEL3_ID) && (event <= MAX_LEVEL3_ID))

#define IS_INT_ENT_EVENT(event) \
    ((event >= MIN_INT_ID) && (event <= MAX_INT_ID))

#define IS_CONTROL_EVENT(event) \
    ((event >= MIN_CONTROL_ID) && (event <= MAX_CONTROL_ID))

#define IS_USER_EVENT(event) \
    ((event >= MIN_USER_ID) && (event <= MAX_USER_ID))

#define CONTROL_EVENT(id) (MIN_CONTROL_ID + id)
#define LEVEL1_EVENT(id)  (MIN_LEVEL1_ID + id)
#define LEVEL2_EVENT(id)  (MIN_LEVEL2_ID + id)
#define LEVEL3_EVENT(id)  (MIN_LEVEL3_ID + id)
#define INT_EVENT(id)     (MIN_INT_ID + id)


/* EVENT Id's */

/* Interrupt events */

#define EVENT_INT_ENT(k) 		(k + MIN_INT_ID)
    /* Param:
     *   short EVENT_INT_ENT(k), 
     *   int timeStamp
     */

#define EVENT_INT_EXIT 	 		(MIN_INT_ID - 1)
    /* Param:
     *   short EVENT_INT_EXIT, 
     *   int timeStamp
     */

#define EVENT_INT_EXIT_K                (MIN_INT_ID - 2)
    /* Param:
     *   short EVENT_INT_EXIT_K,
     *   int timeStamp
     */

/* Control events */

#define EVENT_BEGIN                     CONTROL_EVENT(0)
    /* Param:
     *   short EVENT_BEGIN, 
     *   int CPU, 
     *   int bspSize, 
     *   char * bspName, 
     *   int taskIdCurrent, 
     *   int collectionMode, 
     *   int revision
     */

#define EVENT_END                       CONTROL_EVENT(1)
    /* Param:
     *   short EVENT_END
     */

#define EVENT_TIMER_ROLLOVER            CONTROL_EVENT(2)
    /* Param:
     *   short EVENT_TIMER_ROLLOVER
     *   int timeStamp, 
     */

#define	EVENT_TASKNAME			CONTROL_EVENT(3)
    /* Param:
     *   short EVENT_TASKNAME, 
     *   int status, 
     *   int priority, 
     *   int taskLockCount
     *   int tid, 
     *   int nameSize, 
     *   char * name
     */

#define EVENT_CONFIG              	CONTROL_EVENT(4)
    /* Param:
     *   int revision,                  WV_REV_ID | WV_EVT_PROTO_REV
     *   int timestampFreq, 
     *   int timestampPeriod, 
     *   int autoRollover, 
     *   int clkRate,
     *   int collectionMode,
     *   int processorNum
     */

#define EVENT_BUFFER                    CONTROL_EVENT(5)
    /* Param:
     *   short EVENT_BUFFER,
     *   int taskIdCurrent
     */

#define EVENT_TIMER_SYNC		CONTROL_EVENT(6)
    /* Param:
     *   int timeStamp,
     *   int spare
     */


/* LEVEL1 events */

#define	EVENT_TASKSPAWN				LEVEL1_EVENT(0)
    /* Param:
     *   short EVENT_TASKSPAWN, 
     *   int timeStamp, 
     *   int options, 
     *   int entryPt, 
     *   intstackSize, 
     *   int priority, 
     *   int pTcb
     */

#define	EVENT_TASKDESTROY			LEVEL1_EVENT(1)
    /* Param:
     *    short EVENT_TASKDESTROY, 
     *    int timeStamp, 
     *    int safeCnt, 
     *    int pTcb
     */

#define	EVENT_TASKDELAY				LEVEL1_EVENT(2)
    /* Param:
     *   short EVENT_TASKDELAY, 
     *   int timeStamp, 
     *   int ticks
     */

#define	EVENT_TASKPRIORITYSET			LEVEL1_EVENT(3)
    /* Param:
     *   short EVENT_TASKPRIORITYSET, 
     *   int timeStamp, 
     *   int oldPri, 
     *   int newPri, 
     *   int pTcb
     */

#define	EVENT_TASKSUSPEND			LEVEL1_EVENT(4)
    /* Param:
     *   short EVENT_TASKSUSPEND, 
     *   int timeStamp, 
     *   int pTcb
     */

#define	EVENT_TASKRESUME			LEVEL1_EVENT(5)
    /* Param:
     *   short EVENT_TASKRESUME, 
     *   int timeStamp, 
     *   int priority, 
     *   int pTcb
     */

#define	EVENT_TASKSAFE				LEVEL1_EVENT(6)
    /* Param:
     *   short EVENT_TASKSAFE, 
     *   int timeStamp, 
     *   int safeCnt, 
     *   int tid
     */

#define	EVENT_TASKUNSAFE			LEVEL1_EVENT(7)
    /* Param:
     *   short EVENT_TASKUNSAFE, 
     *   int timeStamp, 
     *   int safeCnt, 
     *   int tid
     */

#define	EVENT_SEMBCREATE			LEVEL1_EVENT(8)
    /* Param:
     *   short EVENT_SEMBCREATE, 
     *   int timeStamp, 
     *   int semOwner, 
     *   int options, 
     *   int semId
     */

#define	EVENT_SEMCCREATE			LEVEL1_EVENT(9)
    /* Param:
     *   short EVENT_SEMCCREATE, 
     *   int timeStamp, 
     *   int initialCount, 
     *   int options, 
     *   int semId
     */

#define	EVENT_SEMDELETE				LEVEL1_EVENT(10)
    /* Param:
     *   short EVENT_SEMDELETE, 
     *   int timeStamp, 
     *   int semId
     */

#define	EVENT_SEMFLUSH				LEVEL1_EVENT(11)
    /* Param:
     *   short EVENT_SEMFLUSH, 
     *   int timeStamp, 
     *   int semId
     */

#define	EVENT_SEMGIVE				LEVEL1_EVENT(12)
    /* Param:
     *   short EVENT_SEMGIVE, 
     *   int timeStamp, 
     *   int recurse, 
     *   int semOwner, 
     *   int semId
     */

#define	EVENT_SEMMCREATE			LEVEL1_EVENT(13)
    /* Param:
     *   short EVENT_SEMMCREATE, 
     *   int timeStamp, 
     *   int semOwner, 
     *   int options, 
     *   int semId
     */

#define	EVENT_SEMMGIVEFORCE			LEVEL1_EVENT(14)
    /* Param:
     *   short EVENT_SEMMGIVEFORCE, 
     *   int timeStamp, 
     *   int semOwner, 
     *   int options, 
     *   int semId
     */

#define	EVENT_SEMTAKE				LEVEL1_EVENT(15)
    /* Param:
     *   short EVENT_SEMTAKE, 
     *   int timeStamp, 
     *   int recurse, 
     *   int semOwner, 
     *   int semId
     */

#define	EVENT_WDCREATE				LEVEL1_EVENT(16)
    /* Param:
     *   short EVENT_WDCREATE, 
     *   int timeStamp, 
     *   int wdId
     */

#define	EVENT_WDDELETE				LEVEL1_EVENT(17)
    /* Param:
     *   short EVENT_WDDELETE, 
     *   int timeStamp, 
     *   int wdId
     */

#define	EVENT_WDSTART				LEVEL1_EVENT(18)
    /* Param:
     *   short EVENT_WDSTART, 
     *   int timeStamp, 
     *   int delay, 
     *   int wdId
     */

#define	EVENT_WDCANCEL				LEVEL1_EVENT(19)
    /* Param:
     *   short EVENT_WDCANCEL, 
     *   int timeStamp, 
     *   int wdId
     */

#define	EVENT_MSGQCREATE			LEVEL1_EVENT(20)
    /* Param:
     *   short EVENT_MSGQCREATE, 
     *   int timeStamp, 
     *   int options, 
     *   int maxMsgLen, 
     *   int maxMsg, 
     *   int msgId
     */

#define	EVENT_MSGQDELETE			LEVEL1_EVENT(21)
    /* Param:
     *   short EVENT_MSGQDELETE, 
     *   int timeStamp, 
     *   int msgId
     */

#define	EVENT_MSGQRECEIVE			LEVEL1_EVENT(22)
    /* Param:
     *   short EVENT_MSGQRECEIVE, 
     *   int timeStamp, 
     *   int timeout, 
     *   int bufSize, 
     *   int buffer, 
     *   int msgId
     */

#define	EVENT_MSGQSEND				LEVEL1_EVENT(23)
    /* Param:
     *   short EVENT_MSGQSEND, 
     *   int timeStamp, 
     *   int priority, 
     *   int timeout, 
     *   int bufSize, 
     *   int buffer, 
     *   int msgId
     */

#define	EVENT_SIGNAL				LEVEL1_EVENT(24)
    /* Param:
     *   short EVENT_SIGNAL, 
     *   int timeStamp, 
     *   int handler, 
     *   int signo
     */

#define	EVENT_SIGSUSPEND			LEVEL1_EVENT(25)
    /* Param:
     *   short EVENT_SIGSUSPEND, 
     *   int timeStamp, 
     *   int pSet
     */

#define	EVENT_PAUSE				LEVEL1_EVENT(26)
    /* Param:
     *   short EVENT_PAUSE, 
     *   int timeStamp, 
     *   int tid
     */

#define	EVENT_KILL				LEVEL1_EVENT(27)
    /* Param:
     *   short EVENT_KILL, 
     *   int timeStamp, 
     *   int tid, 
     *   int signo
     */

#define EVENT_SAFE_PEND                         LEVEL1_EVENT(28)
    /* Param:
     *   short EVENT_SAFE_PEND, 
     *   int tid
     */

#define EVENT_SIGWRAPPER                        LEVEL1_EVENT(29)
    /* Param:
     *   short EVENT_SIGWRAPPER, 
     *   int signo, 
     *   int tid
     */


/* LEVEL2 events */

#define	EVENT_WINDSPAWN				LEVEL2_EVENT(0)
    /* Param:
     *   short EVENT_WINDSPAWN, 
     *   int pTcb, 
     *   int priority
     */

#define	EVENT_WINDDELETE			LEVEL2_EVENT(1)
    /* Param:
     *   short EVENT_WINDDELETE, 
     *   int pTcb
     */

#define	EVENT_WINDSUSPEND			LEVEL2_EVENT(2)
    /* Param:
     *   short EVENT_WINDSUSPEND, 
     *   int pTcb
     */

#define	EVENT_WINDRESUME			LEVEL2_EVENT(3)
    /* Param:
     *   short EVENT_WINDRESUME, 
     *   int pTcb
     */

#define	EVENT_WINDPRIORITYSETRAISE		LEVEL2_EVENT(4)
    /* Param:
     *   short EVENT_WINDPRIORITYSETRAISE, 
     *   int pTcb, 
     *   int oldPriority, 
     *   int priority
     */


#define	EVENT_WINDPRIORITYSETLOWER		LEVEL2_EVENT(5)
    /* Param:
     *   short EVENT_WINDPRIORITYSETLOWER, 
     *   int pTcb, 
     *   int oldPriority, 
     *   int priority
     */

#define	EVENT_WINDSEMDELETE			LEVEL2_EVENT(6)
    /* Param:
     *   short EVENT_WINDSEMDELETE, 
     *   int semId
     */

#define	EVENT_WINDTICKANNOUNCETMRSLC		LEVEL2_EVENT(7)
    /* Param:
     *   short EVENT_WINDTICKANNOUNCETMRSLC
     */

#define	EVENT_WINDTICKANNOUNCETMRWD		LEVEL2_EVENT(8)
    /* Param:
     *   short EVENT_WINDTICKANNOUNCETMRWD, 
     *   int wdId
     */

#define	EVENT_WINDDELAY				LEVEL2_EVENT(9)
    /* Param:
     *   short EVENT_WINDDELAY
     *   int ticks
     */

#define	EVENT_WINDUNDELAY			LEVEL2_EVENT(10)
    /* Param:
     *   short EVENT_WINDUNDELAY, 
     *   int pTcb
     */

#define	EVENT_WINDWDSTART			LEVEL2_EVENT(11)
    /* Param:
     *   short EVENT_WINDWDSTART, 
     *   int wdId
     */

#define	EVENT_WINDWDCANCEL			LEVEL2_EVENT(12)
    /* Param:
     *   short EVENT_WINDWDCANCEL, 
     *   int wdId
     */

#define	EVENT_WINDPENDQGET			LEVEL2_EVENT(13)
    /* Param:
     *   short EVENT_WINDPENDQGET, 
     *   int pTcb
     */

#define	EVENT_WINDPENDQFLUSH			LEVEL2_EVENT(14)
    /* Param:
     *   short EVENT_WINDPENDQFLUSH, 
     *   int pTcb
     */

#define	EVENT_WINDPENDQPUT			LEVEL2_EVENT(15)
    /* Param:
     *   short EVENT_WINDPENDQPUT
     */

#define	EVENT_WINDPENDQTERMINATE		LEVEL2_EVENT(17)
    /* Param:
     *   short EVENT_WINDPENDQTERMINATE, 
     *   int pTcb
     */

#define	EVENT_WINDTICKUNDELAY			LEVEL2_EVENT(18)
    /* Param:
     *   short EVENT_WINDTICKUNDELAY, 
     *   int pTcb
     */

#define EVENT_OBJ_TASK				LEVEL2_EVENT(19)
    /* Param:
     *   short EVENT_OBJ_TASK, 
     *   int pTcb
     */

#define EVENT_OBJ_SEMGIVE			LEVEL2_EVENT(20)
    /* Param:
     *   short EVENT_OBJ_SEMGIVE, 
     *   int semId
     */

#define EVENT_OBJ_SEMTAKE			LEVEL2_EVENT(21)
    /* Param:
     *   short EVENT_OBJ_SEMTAKE, 
     *   int semId
     */

#define EVENT_OBJ_SEMFLUSH			LEVEL2_EVENT(22)
    /* Param:
     *   short EVENT_OBJ_SEMFLUSH, 
     *   int semId
     */

#define EVENT_OBJ_MSGSEND			LEVEL2_EVENT(23)
    /* Param:
     *   short EVENT_OBJ_MSGSEND, 
     *   int msgQId
     */

#define EVENT_OBJ_MSGRECEIVE			LEVEL2_EVENT(24)
    /* Param:
     *   short EVENT_OBJ_MSGRECEIVE, 
     *   int msgQId
     */

#define EVENT_OBJ_MSGDELETE			LEVEL2_EVENT(25)
    /* Param:
     *   short EVENT_OBJ_MSGDELETE, 
     *   int msgQId
     */

#define EVENT_OBJ_SIGPAUSE			LEVEL2_EVENT(26)
    /* Param:
     *   short EVENT_OBJ_SIGPAUSE, 
     *   int qHead
     */

#define EVENT_OBJ_SIGSUSPEND			LEVEL2_EVENT(27)
    /* Param:
     *   short EVENT_OBJ_SIGSUSPEND, 
     *   int sigset
     */

#define EVENT_OBJ_SIGKILL			LEVEL2_EVENT(28)
    /* Param:
     *   short EVENT_OBJ_SIGKILL, 
     *   int tid
     */

#define EVENT_WINDTICKTIMEOUT                   LEVEL2_EVENT(31)
    /* Param:
     *   short EVENT_WINDTICKTIMEOUT,
     *   int tId
     */

#define EVENT_OBJ_SIGWAIT                       LEVEL2_EVENT(32)
    /* Param:
     *   short EVENT_OBJ_SIGWAIT, 
     *   int tid
     */


/* level3 events */

#define EVENT_WIND_EXIT_DISPATCH        	LEVEL3_EVENT(2)
    /* Param:
     *   short EVENT_WIND_EXIT_DISPATCH, 
     *   int timestamp,
     *   int tId,
     *   int priority
     */

#define EVENT_WIND_EXIT_NODISPATCH        	LEVEL3_EVENT(3)
    /* Param:
     *   short EVENT_WIND_EXIT_NODISPATCH, 
     *   int timestamp,
     *   int priority
     */

#define EVENT_WIND_EXIT_DISPATCH_PI        	LEVEL3_EVENT(4)
    /* Param:
     *   short EVENT_WIND_EXIT_DISPATCH_PI, 
     *   int timestamp,
     *   int tId,
     *   int priority
     */

#define EVENT_WIND_EXIT_NODISPATCH_PI        	LEVEL3_EVENT(5)
    /* Param:
     *   short EVENT_WIND_EXIT_NODISPATCH_PI, 
     *   int timestamp,
     *   int priority
     */

#define EVENT_DISPATCH_OFFSET   0xe
#define EVENT_NODISPATCH_OFFSET   0xa
    /*
     * This definition is needed for the logging of the above _PI events
     */

#define	EVENT_WIND_EXIT_IDLE			LEVEL3_EVENT(6)
    /* Param:
     *   short EVENT_WIND_EXIT_IDLE, 
     *   int timestamp
     */

#define	EVENT_TASKLOCK				LEVEL3_EVENT(7)
    /* Param:
     *   short EVENT_TASKLOCK, 
     *   int timeStamp
     */

#define	EVENT_TASKUNLOCK			LEVEL3_EVENT(8)
    /* Param:
     *   short EVENT_TASKUNLOCK, 
     *   int timeStamp
     */

#define	EVENT_TICKANNOUNCE			LEVEL3_EVENT(9)
    /* Param:
     *   short EVENT_TICKANNOUNCE, 
     *   int timeStamp
     */

#define	EVENT_EXCEPTION				LEVEL3_EVENT(10)
    /* Param:
     *   short EVENT_EXCEPTION, 
     *   int timeStamp, 
     *   int exception
     */

#define EVENT_TASK_STATECHANGE			LEVEL3_EVENT(11)
    /* Param:
     *   short EVENT_STATECHANGE
     *   int timeStamp,
     *   int taskId,
     *   int newState
     */

/* pseudo events - generated host side */

#define MIN_PSEUDO_EVENT                        LEVEL3_EVENT(20)

#define EVENT_STATECHANGE                       LEVEL3_EVENT(20)
    /* Param:
     *   short EVENT_STATECHANGE
     *   int newState
     */

#define MAX_PSEUDO_EVENT                        LEVEL3_EVENT(24)

#define IS_PSEUDO_EVENT(event) \
    ((event >= MIN_PSEUDO_EVENT) && (event <= MAX_PSEUDO_EVENT))

/*
 * Alignment macros used to store unaligned short (16 bits) and unaligned
 * int (32 bits).
 */

#if  CPU_FAMILY==MC680X0 

#define EVT_STORE_UINT16(pBuf, event_id) \
    *pBuf++ = (event_id)

#define EVT_STORE_UINT32(pBuf, value) \
    *pBuf++ = (value); 

#else 	    /* CPU_FAMILY==MC680X0 */

#define EVT_STORE_UINT16(pBuf, event_id) \
    *pBuf++ = (event_id)

#if (CPU_FAMILY==I960 || CPU_FAMILY==I80X86)

/* little endian */
#define EVT_STORE_UINT32(pBuf, value) \
    (*((short *) pBuf)++ = (value), \
     *((short *) pBuf)++ = (value) >> 16)

#else

/* big endian */
#define EVT_STORE_UINT32(pBuf, value) \
    *((short *) pBuf)++ = (value) >> 16; \
     *((short *) pBuf)++ = (value)

#endif

#endif       /* CPU_FAMILY==MC680X0 */

#endif /* __INCeventph*/




