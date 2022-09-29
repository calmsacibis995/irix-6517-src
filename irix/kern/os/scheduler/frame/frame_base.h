/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_FRAME_BASE_H__
#define __SYS_FRAME_BASE_H__

#include <sys/cpumask.h>
#include <sys/frs.h>
#include "frame_barrier.h"
#include "frame_semabarrier.h"

/*
 * Declarations for intr management functions
 * defined within their own subsystem modules.
 */

struct frs_fsched;

extern void frs_eintr_set(intrgroup_t* intrgroup);
extern void frs_eintr_clear(void);
extern void gfx_frs_install(struct frs_fsched *frs, int pipenum);
extern void gfx_frs_uninstall(int pipenum);
extern int gfx_frs_valid_pipenum(int pipenum); 

/*
 * Functions exported to scheduler only
 */
extern kthread_t *frs_fsched_dispatch(void);
extern void frs_driver_init(void);

/*
 * Forward declaration of the frame scheduler object
 */

/*
 * Dispatch Codes
 */
#define FRS_DISPCODE_TRYAGAIN      0
#define FRS_DISPCODE_OK            1

#define FRS_OK_SPACE_AVAIL         1


/*
 * Error Recovery Processing State.
 */
typedef enum {
	DPS_INVALID,
	DPS_STANDARD,
	DPS_STRETCHER,
	DPS_STEALER,
	DPS_SHRANK,
	DPS_LAST
} dpstate_t;
	
/*
 * Mfberror recovery descriptor
 */
typedef struct mfberror_recovery {
	/* static section */
	mfbe_rmode_t rmode;          /* Recovery mode */
	mfbe_tmode_t tmode;          /* Extended time mode */
	int          maxcerr;        /* Maximum consecutive recoveries */
	uint         xtime;          /* Time extension for extended-frame recovery */
} mfbe_rdesc_t;


/*
 * Shared state for a group
 * of synchronous frame
 * schedulers.
 */
typedef struct syncdesc {
        lock_t		    sl_lock;        /* lock for the slave list & master_frs */
        int                 sl_num;         /* Total Number of sync-slave frs's */
        int                 sl_index;       /* Index to next avail slot in slave list */
        struct frs_fsched** sl_list;        /* List of sync-slave frs's */
        struct frs_fsched*  master_frs;     /* Sync-master frs */

        int                 sl_term;        /* Public term indicator flag */
        int                 sl_term_ack;    /* Termination acknowledgement  */
        int                 sl_stop;        /* Public stop indicator flag */
        int                 sl_stop_ack;    /* Stop acknowledgement  */

	mfbe_rdesc_t        rdesc;	    /* MFBE recovery descriptor */
	uint                mfbestate;      /* Over/Underrun state */
				            /* trans */
        semabarrier_t*      start_sync;     /* Synchronous start (sleeping barrier) */
        semabarrier_t*      destroy_sync;   /* Destruction Synchronization (sleeping bar) */
} syncdesc_t;


typedef struct sigdesc {
        int sig_underrun;
        int sig_overrun;
        int sig_dequeue;      /* A frame sched process has been dequeued */    
        int sig_unframesched; /* A frame sched process has been unframesched */
	int sig_isequence;    /* An FRS interrupt came in out of sequence */
} sigdesc_t;


/*
 * Error Codes
 */


#define FRS_ERROR_PROCESS_NEXIST	ESRCH		/* Does not exist */
#define FRS_ERROR_PROCESS_ISCONTROLLER	ECONTROLLER   	/* Is a controller */
#define FRS_ERROR_PROCESS_NCONTROLLER   ENOTCONTROLLER	/* Not a controller */
#define FRS_ERROR_PROCESS_ISFSCHED	EENQUEUED	/* Is fsched (enqueued) */
#define FRS_ERROR_PROCESS_NFSCHED	ENOTENQUEUED	/* Not fsched (not enqueued) */
#define FRS_ERROR_PROCESS_ISJOINED	EJOINED		/* Is joined */
#define FRS_ERROR_PROCESS_NJOINED	ENOTJOINED	/* Not joined */
#define FRS_ERROR_PROCESS_NFOUND	ENOPROC         /* Not in minor */
#define FRS_ERROR_PROCESS_ISMUSTRUN	EMUSTRUN	/* Is mustrun */
#define FRS_ERROR_PROCESS_NPERM		EACCES		/* Not fsched-able */
#define FRS_ERROR_PROCESS_NSTOPPED	ENOTSTOPPED	/* Not stopped */

#define FRS_ERROR_CPU_NEXIST		ENOEXIST		/* Does not exist */
#define FRS_ERROR_CPU_CLOCK             ECLOCKCPU	/* Clock processor */
#define FRS_ERROR_CPU_BUSY		EBUSY		/* Already running frs */

#define FRS_ERROR_MINOR_NEXIST          ENOEXIST		/* Does not exist */
#define FRS_ERROR_MINOR_END		EBUFSIZE	/* Reached end of minor */
#define FRS_ERROR_MINOR_ZERO		EEMPTY		/* Major with zero minors */
#define FRS_ERROR_MINOR_ISEMPTY         EEMPTY		/* Is empty */

#define FRS_ERROR_ATTR_NEXIST           ENOATTR		/* Does not exist */

#define FRS_ERROR_INTR_INVPERIOD        EINVAL    /* Period out of range */
#define FRS_ERROR_INTR_INVGRAPHICPIPE	EINVAL    /* Invalid graphic pipe */
#define FRS_ERROR_INTR_INVDRIVER	EINVAL    /* Invalid driver */
#define FRS_ERROR_INTR_INVSOURCE        EINVAL    /* Invalid interrupt source */
#define FRS_ERROR_INTR_OUTOFINTRGROUPS  ENOINTRGROUP    /* Out of interrupt groups */

#define FRS_ERROR_RECOVERY_INVMODE	EINVALMODE	/* Invalid recovery mode */
#define FRS_ERROR_RECOVERY_CANTEXTEND	ECANTEXTENT     /* Non extendable timing source */
#define FRS_ERROR_RECOVERY_INVEXTTIME   EINVALTIME	/* Extension time is out of range */
#define FRS_ERROR_RECOVERY_INVEXTMODE	EINVALMODE	/* Extension mode is invalid */

#define FRS_ERROR_FSCHED_DESTROYED	EDESTROYED	/* Destroyed or being destroyed */
#define FRS_ERROR_FSCHED_JOININTR	EINTR		/* Join wait interrupted by signal */
#define FRS_ERROR_FSCHED_TOOMANYSLAVES	ENOSPC		/* Too many slaves */
#define FRS_ERROR_FSCHED_INVSTATE	EINVALSTATE	/* Invalid state for requested operation */

#endif /* __SYS_FRAME_BASE_H__ */
