/* ioQLib.h - I/O queue library header file */

/*
modification history
--------------------
01b,26jan94,kdl  changed name to ioQLib.h.
01a,04apr93,elh  written.
*/

#ifndef __INCioQLibh
#define __INCioQLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vxworks.h"
#include "lstlib.h"
#include "semlib.h"

/* typedefs */

typedef struct io_node				/* I/O node */
    {
    NODE		node;			/* list node */
    int			op;			/* op IO_READ | IO_WRITE */ 
    int			prio;			/* priority */
    int			taskId;			/* initiating task id */
    int			retVal;			/* return value */
    int			errorVal;		/* error value */
    VOIDFUNCPTR		doneRtn;		/* done routine */
    int 		value;
    } IO_NODE;

typedef struct io_q				/* I/O queue */
    {
    LIST		workQ;			/* list structure */
    LIST		waitQ;			/* list structure */
    VOIDFUNCPTR 	lock;			/* lock routine */
    VOIDFUNCPTR		unlock;			/* unlock routine */
    int			lockArg;		/* arg for lock/unlock */
    } IO_Q;

/* defines */

/* i/o operations */

#define IO_READ	 		0		/* READ operation */
#define	IO_WRITE		1		/* WRITE operation */
#define IO_SYNC			2		/* SYNC operation */

/* i/o synchronization and operations */

#define IOQ_LOCK(pQ)		((*(pQ)->lock) ((pQ)->lockArg))
#define IOQ_UNLOCK(pQ)		((*(pQ)->unlock) ((pQ)->lockArg))

#define IOQ_WORK_ADD(pQ, pNode, prio)				\
    		ioQAdd (&(pQ)->workQ, (pNode), (prio))

#define IOQ_WORK_ADD_HEAD(pQ, pNode)				\
		lstInsert (&(pQ)->workQ, NULL, &(pNode)->node)

#define IOQ_WAIT_ADD(pQ, pNode, prio)				\
    		ioQAdd (&(pQ)->waitQ, (pNode), (prio))

#define IOQ_WORK_DELETE(pQ, pNode)	lstDelete (&(pQ)->workQ, &(pNode)->node)
#define IOQ_WAIT_DELETE(pQ, pNode)	lstDelete (&(pQ)->waitQ, &(pNode)->node)

/* forward declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	ioQInit (IO_Q * pQ, VOIDFUNCPTR pLock, VOIDFUNCPTR pUnlock,
		         int lockArg);
extern void	ioQNodeDone (IO_NODE * pNode);
extern void	ioQLockSem (SEM_ID semId);
extern void	ioQUnlockSem (SEM_ID semId);
extern void 	ioQAdd (LIST * pQ, IO_NODE * pNode, int prio);
extern void 	ioQDelete (IO_Q * pQ, IO_NODE * pNode);
extern IO_NODE * ioQEach (LIST * pQ, FUNCPTR pRoutine, int arg1, int arg2);

#else	/* __STDC__ */

extern void 	ioQInit ();
extern void	ioQNodeDone ();
extern void	ioQLockSem ();
extern void	ioQUnlockSem ();
extern void 	ioQAdd ();
extern void 	ioQDelete ();
extern IO_NODE * ioQEach ();

#endif	/* __STDC__ */
#ifdef __cplusplus
}
#endif

#endif /* __INCioQLibh */
