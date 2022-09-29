/* aioPxLibP.h - asynchronous I/O header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,15apr94,kdl  changed FD_NODE_TO_SYS macro to use offsetof().
01e,08apr94,kdl  changed aio_show() to aioShow().
01d,26jan94,kdl  changed include of ioQPxLib.h to ioQLib.h.
01c,12jan94,kdl  changed aioInit() to aioPxLibInit(); general cleanup.
01b,06dec93,dvs  changed M_aioLib to M_aioPxLib
                 changed S_aioLib* to S_aioPxLib*
01a,04apr93,elh  written.
*/


#ifndef __INCaioPxLibh
#define __INCaioPxLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "private/semlibp.h"
#include "ioslibp.h"
#include "qprinode.h"
#include "lstlib.h"
#include "siglib.h"
#include "semlib.h"
#include "ioqlib.h"
#include "dlllib.h"
#include "private/mutexpxlibp.h"
#include "stddef.h"

/* defines */

/* error codes */

#define S_aioPxLib_DRV_NUM_INVALID		(M_aioPxLib | 1)
#define S_aioPxLib_AIO_NOT_DEFINED		(M_aioPxLib | 2)
#define S_aioPxLib_IOS_NOT_INITIALIZED		(M_aioPxLib | 3)
#define S_aioPxLib_NO_AIO_DEVICE 		(M_aioPxLib | 4)

/* aio driver ioctl functions */

#define  FAIO_PUSH		1		/* push request to head */
#define  FAIO_CANCEL		2		/* cancel a request */

/* aio control block states */

#define AIO_FREE		0		/* not submitted */
#define AIO_COMPLETED		1		/* completed */
#define AIO_READY		2		/* ready to be submitted */
#define AIO_QUEUED		3		/* queued */
#define AIO_WAIT   		4		/* waiting */
#define AIO_RUNNING		5		/* running */

#define AIO_CLUST_MAX           100             /* default max clusters */

/* easy access to aio_sys */

#define aio_state		aio_sys.state
#define aio_retVal 		aio_sys.ioNode.retVal
#define aio_errorVal		aio_sys.ioNode.errorVal

/* easy access to fd_entry */ 

#define drv_value		pFdEntry->value
#define drv_num			pFdEntry->pDevHdr->drvNum

/* mark request done */

#define AIO_DONE_SET(pAiocb, _ret, _err) 	\
    {						\
    (pAiocb)->aio_sys.state    = AIO_COMPLETED;	\
    (pAiocb)->aio_sys.ioNode.retVal   = (_ret);	\
    (pAiocb)->aio_sys.ioNode.errorVal = (_err);	\
    }

#define FD_NODE_TO_SYS(pNode)			\
	((AIO_SYS *)(void *)((char *)(pNode) - offsetof(AIO_SYS, fdNode)))



/* typedefs */

typedef struct aio_clust			/* aio cluster */
    {
    BOOL		inuse;			/* cluster in use */
    int			refCnt;			/* reference count */
    struct sigpend	sigpend;		/* signal */
    mutex_t		lock;			/* lock cluster */
    cond_t		wake;			/* cluster cond var */
    } AIO_CLUST;

typedef struct aio_sys				/* aio system info */
    {
    IO_NODE		ioNode;			/* I/O queue node */
    int			state;			/* state */
    AIO_CLUST *		pClust;			/* lio cluster */
    struct aiocb *	pAiocb;			/* A-I/O control block */
    struct sigpend     	sigpend;		/* signal */
    LIST		wait; 			/* wait list */
    mutex_t		lock;			/* aiocb lock */
    cond_t		wake;			/* aiocb cond var */
    DL_NODE		fdNode;			/* node for fd list */
    } AIO_SYS;


typedef struct aio_wait_id			/* wait structure */
    {
    NODE		node;			/* wait list node */
    BOOL		done;			/* completed */
    mutex_t		lock;			/* wait id lock */
    cond_t		wake;			/* wait id cond var */
    } AIO_WAIT_ID;


typedef struct aio_dev				/* driver A-I/O struct */
    {
    IO_Q		ioQ;			/* I/O queues */
    } AIO_DEV;


typedef struct aio_drv_entry			/* A-I/O driver table entry */
    {
    BOOL		inuse;			/* entry in use */ 
    FUNCPTR		insert;			/* insert routine */
    FUNCPTR		ioctl;			/* ioctl routine */
    int			flags;			/* driver specific flags */
    } AIO_DRV_ENTRY;


typedef struct aio_fd_entry			/* A-I/O fd table entry */
    {
    FD_ENTRY *		pFdEntry;		/* fd entry */
    DL_LIST 		ioQ;			/* I/O queue */
    SEMAPHORE   	ioQSem;			/* semaphore for I/O queue */
    BOOL		ioQActive;		/* I/O queue active */
    } AIO_FD_ENTRY;

extern FUNCPTR aioPrintRtn;			/* for aioShow */

/* forward declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	aioPxLibInit (int lioMax);
extern STATUS	aioDrvInstall (int drvnum, FUNCPTR pInsert, FUNCPTR pIoctl, 
			      int flags);
extern STATUS 	aioCancel (AIO_DEV * pDev, struct aiocb * pAiocb);
extern STATUS 	aioPush (AIO_DEV * pDev, struct aiocb * pAiocb);
extern STATUS 	aioSync (AIO_DEV * pDev, struct aiocb * pAiocb, 
	       	         FUNCPTR syncReqRtn);
extern void 	aioDone (AIO_SYS * pReq);

extern STATUS 	aioDrvFlagsGet (int fd, int * pFlags);
extern STATUS 	aioDrvFlagsSet ( int fd, int flags);
extern AIO_SYS *  aioNext (AIO_DEV * pDev);
extern STATUS 	aioShow (int drvNum);
extern AIO_FD_ENTRY * aioEntryFind (int fd);

#else	/* __STDC__ */

extern STATUS	aioPxLibInit ();
extern STATUS	aioDrvInstall ();
extern STATUS 	aioCancel ();
extern STATUS 	aioPush ();
extern STATUS 	aioSync ();
extern void 	aioDone ();
extern STATUS 	aioDrvFlagsGet ();
extern STATUS 	aioDrvFlagsSet ();
extern AIO_SYS *  aioNext ();
extern STATUS 	aioShow ();
extern AIO_FD_ENTRY * aioEntryFind ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCaioPxLibh */
