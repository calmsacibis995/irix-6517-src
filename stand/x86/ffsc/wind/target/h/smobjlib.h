/* smObjLib.h - shared memory object library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01k,01aug94,dvs  backed out pme's changes for reserved fields in main data structures.
01j,20mar94,pme  added reserved fields in main data structures to allow 
		 compatibility between future versions.
01i,29jan93,pme  added little endian support
		 made smObjLibInit() return STATUS.
01h,15oct92,rrr  silenced warnings
01g,29sep92,pme  added version number
01f,22sep92,rrr  added support for c++
01e,11sep92,ajm  moved redundant define of DEFAULT_BEATS_TO_WAIT to smLib.h
01d,30jul92,pme  made SM_LOCK_GIVE() call smLockGive().
                 added pre-declaration of smObjTasClearRoutine.
01c,24jul92,elh  added heartbeat to header.
01b,22jul92,pme  added S_smObjLib_NO_OBJECT_DESTROY status
01a,19jul92,pme  added DEFAULT_BEATS_TO_WAIT.
                 written.
*/

#ifndef __INCsmObjLibh
#define __INCsmObjLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "smdlllib.h"
#include "smlib.h"
#include "netinet/in.h"
#include "private/smfixblklibp.h"
#include "private/smmemlibp.h"
#include "private/smnamelibp.h"

/* generic status codes */

#define S_smObjLib_NOT_INITIALIZED        (M_smObjLib | 1)
#define S_smObjLib_NOT_A_GLOBAL_ADRS      (M_smObjLib | 2)
#define S_smObjLib_NOT_A_LOCAL_ADRS       (M_smObjLib | 3)
#define S_smObjLib_SHARED_MEM_TOO_SMALL   (M_smObjLib | 4)
#define S_smObjLib_TOO_MANY_CPU           (M_smObjLib | 5)
#define S_smObjLib_LOCK_TIMEOUT           (M_smObjLib | 6)
#define S_smObjLib_NO_OBJECT_DESTROY      (M_smObjLib | 7)

#define SM_OBJ_MAX_CPU		20      /* absolute maximum number of CPU */

/* useful shared memory object ids handling macros */

#define ID_IS_LOCAL(id)		((((UINT32) id) & 1) == 0)
#define ID_IS_SHARED(id)	((((UINT32) id) & 1) != 0)
#define SM_OBJ_ID_TO_ADRS(id)	(((int)(id)) + smObjPoolMinusOne)
#define SM_OBJ_ADRS_TO_ID(adrs)	(((int)(adrs)) - smObjPoolMinusOne)

/* local to global and global to local address conversion; local NULL pointer */

#define GLOB_TO_LOC_ADRS(adrs)	(((int)(adrs)) + localToGlobalOffset)
#define LOC_TO_GLOB_ADRS(adrs)	(((int)(adrs)) - localToGlobalOffset)
#define LOC_NULL		((void *)localToGlobalOffset)


/*******************************************************************************
*
* SM_OBJ_VERIFY - check the validity of a shared memory object
*
* This macro verifies the validity of the specified shared memory object by
* comparing the id and its verify field.
*
* RETURNS: OK or ERROR if invalid shared memory object
*
* ERRNO:
*
*   S_objLib_OBJ_ID_ERROR
*
* NOMANUAL
*/

#define SM_OBJ_VERIFY(smObjId) 						   \
	(								   \
	((LOC_TO_GLOB_ADRS ((smObjId))) == ntohl ((smObjId->verify))) ? OK \
           :								   \
	   (errno = S_objLib_OBJ_ID_ERROR, ERROR)			   \
        )

/*******************************************************************************
*
* SM_OBJ_LOCK_TAKE - acquire lock access on a shared ressource
*
* This macro tries to acquire exclusive access to a shared ressource
* via a test-and-set on a long word memory location. It uses shMemLockTake
* with smObjSpinTries tries to get lock using smObjTasRoutine test and
* set routine (usually sysBusTas).
*
* NOMANUAL
*/

#define SM_OBJ_LOCK_TAKE(lockLocalAdrs, pOldLvl) \
                    (smLockTake ((int *) lockLocalAdrs, smObjTasRoutine, \
                    		 smObjSpinTries, (int *) pOldLvl))

/*******************************************************************************
*
* SM_OBJ_LOCK_GIVE - release lock access on a shared ressource
*
* This macro release exclusive access to a shared ressource by clearing
* a long word memory location.
*
* NOMANUAL
*/

#define SM_OBJ_LOCK_GIVE(lockLocalAdrs,oldLvl) \
                    (smLockGive ((int *) lockLocalAdrs, smObjTasClearRoutine, \
				 oldLvl))

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* typedefs */

/* shared memory object header located on top of shared memory pool */

typedef struct sm_obj_mem_hdr      /* SM_OBJ_MEM_HDR - shared memory header */
    {
    UINT            heartBeat; /* incremented via smObjBeat() */
    BOOL            initDone;  /* TRUE if initialization done */
    UINT            version;   /* version number */
    SM_FIX_BLK_PART smTcbPart; /* partition for shared TCB's */
    SM_PARTITION    smSemPart; /* partition for shared semaphores */
    SM_PARTITION    smNamePart;/* partition for shared names */
    SM_PARTITION    smMsgQPart;/* partition for shared msgQ */
    SM_PARTITION    smPartPart;/* partition for shared user partitions */
    SM_PARTITION    smSysPart; /* default shared system partition */
    SM_OBJ_NAME_DB  nameDtb;   /* name database header */
    int             objCpuTbl; /* smObj descriptor table (offset) */

    int maxSems;                /* max number of semaphores */
    int maxMsgQueues;           /* max number of messages queues */
    int maxTasks;               /* max number of tasks */
    int maxMemParts;            /* max number of shared memory partitions */
    int maxNames;               /* max number of name of shared objects */

    int curNumSemB;             /* current number of binary semaphores */
    int curNumSemC;             /* current number of counting semaphores */
    int curNumMsgQ;             /* current number of messages queues */
    int curNumTask;             /* current number of tasks */
    int curNumPart;             /* current number of shared partitions */
    int curNumName;             /* current number of names */

    } SM_OBJ_MEM_HDR;

#define smSemPartId    (&pSmObjHdr->smSemPart)
#define smNamePartId   (&pSmObjHdr->smNamePart)
#define smMsgQPartId   (&pSmObjHdr->smMsgQPart)
#define smPartPartId   (&pSmObjHdr->smPartPart)
#define smSystemPartId (&pSmObjHdr->smSysPart)

typedef struct sm_obj_event_q     /* SM_OBJ_EVENT_Q - events input queue */
    {
    UINT32       lock;            /* multi processor lock */
    SM_DL_LIST   eventList;       /* list of smObj events */
    } SM_OBJ_EVENT_Q;

/* per CPU shared memory object Descriptor */

typedef struct sm_obj_cpu_desc     /* SM_OBJ_CPU_DESC */
    {
    int             status;        /* CPU status - attached/unattached */
    SM_OBJ_EVENT_Q  smObjEventQ;   /* smObj CPU event queue */
    } SM_OBJ_CPU_DESC;

typedef struct sm_obj_desc   /* SM_OBJ_DESC - shared memory object descriptor */
    {
    int                 status;
    SM_DESC             smDesc;         /* shared memory descriptor */
    SM_OBJ_MEM_HDR *    hdrLocalAdrs;   /* smObj memory header local adrs */
    SM_OBJ_CPU_DESC *   cpuLocalAdrs;   /* smObj cpu descriptor local adrs */
    } SM_OBJ_DESC;

typedef struct sm_obj_params	/* setup parameters */
    {
    BOOL        allocatedPool;  /* TRUE if shared memory pool is malloced */
    SM_ANCHOR * pAnchor;        /* shared memory anchor */
    char *      smObjFreeAdrs;  /* start address of shared memory pool */
    int         smObjMemSize;   /* memory size reserved for shared memory */
    int         maxCpus;        /* max number of CPU in the system */
    int         maxTasks;       /* max number of tasks using smObj */
    int         maxSems;        /* max number of shared semaphores */
    int         maxMsgQueues;   /* max number of shared message queues */
    int         maxMemParts;    /* max number of shared memory partitions */
    int         maxNames;       /* max number of name of shared objects */
    } SM_OBJ_PARAMS;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

/* variable declarations */

extern int	smObjProcNum;		/* processor number */
extern int	smObjPoolMinusOne;	/* smObj pool local address - 1 */
extern int	localToGlobalOffset;	/* localAdrs - globalAdrs */
extern FUNCPTR	smObjTasRoutine;	/* test and set routine */
extern FUNCPTR  smObjTasClearRoutine;   /* clear routine */
extern int	smObjSpinTries;		/* maximum retries for lock access */
extern SM_HDR *         pSmHdr;		/* pointer to shared memory header */
extern SM_OBJ_MEM_HDR * pSmObjHdr;	/* pointer to sm objects header */
extern SM_OBJ_DESC      smObjDesc;	/* shared memory object descriptor */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS smObjLibInit (void);
extern STATUS smObjSetup (SM_OBJ_PARAMS * smObjParams);
extern void   smObjInit (SM_OBJ_DESC * pSmObjDesc, SM_ANCHOR * pAnchor,
			 int ticksPerBeat, int smObjMaxTries, int intType,
			 int intArg1, int intArg2, int intArg3);
extern STATUS smObjAttach (SM_OBJ_DESC * pSmObjDesc);
extern void * smObjLocalToGlobal (void * localAdrs);
extern void * smObjGlobalToLocal (void * globalAdrs);
extern void   smObjTimeoutLogEnable (BOOL timeoutLogEnable);
extern void   smObjShowInit (void);
extern STATUS smObjShow (void);

#else	/* __STDC__ */

extern STATUS smObjLibInit ();
extern STATUS smObjSetup ();
extern void   smObjInit ();
extern STATUS smObjAttach ();
extern void * smObjLocalToGlobal ();
extern void * smObjGlobalToLocal ();
extern void   smObjTimeoutLogEnable ();
extern void   smObjShowInit ();
extern STATUS smObjShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsmObjLibh */
