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

#ifndef __SYS_FRAME_PROCD_H__
#define __SYS_FRAME_PROCD_H__

/*
 * Frame Scheduler - Thread Descriptors
 */

/*
 * The Global Data Structure View:
 *
 *                                              Minor Frames
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/  frs_minor_t   ***frs_prod_t***
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 *                  |      |    |--|              |   |   [ ]<--[ ]<--[ ]<--
 *                  --------    |  |              -----
 *                              |--|
 *                frs_major_t   |  | etc.
 *                              |--|
 *                              
 */

#include "frame_process.h"

/*
 * Process state transition signals. Optionally
 * sent to a process whenever the process undergoes some
 * process state transitions.
 */

typedef uint frs_disc_t;

typedef struct frs_ptsignal {
        int sig_dequeue;
        int sig_unframesched;
} frs_ptsignal_t;

typedef struct frs_procd {
	uthread_t		*thread;
	frs_threadid_t		thread_id;
	void			*frs;
	frs_disc_t		discipline;
        frs_ptsignal_t		ptsignal;
	int			underrun_counter;
	int			overrun_counter;
	struct frs_procd*	fp;
	struct frs_procd*	bp;
        semabarrier_t*		references;
} frs_procd_t;

/*
 * Disciplines
 */

#define FRSF_OVERRUN             0x1           /* overrunnable process */
#define FRSF_UNDERRUN            0x2           /* underrunnable process */
#define FRSF_CONT                0x4           /* continuation */

#define _isFSDOverrunnable(d)    ((d)->discipline & FRSF_OVERRUN)
#define _makeFSDOverrunnable(d)  (d)->discipline |= FRSF_OVERRUN
#define _unFSDOverrunnable(d)    (d)->discipline &= ~FRSF_OVERRUN
  
#define _isFSDUnderrunnable(d)   ((d)->discipline & FRSF_UNDERRUN)
#define _makeFSDUnderrunnable(d) (d)->discipline |= FRSF_UNDERRUN
#define _unFSDUnderrunnable(d)   (d)->discipline &= ~FRSF_UNDERRUN
  
#define _isFSDCont(d)            ((d)->discipline & FRSF_CONT)
#define _makeFSDCont(d)          (d)->discipline |= FRSF_CONT
#define _unFSDCont(d)            (d)->discipline &= ~FRSF_CONT

frs_procd_t* frs_procd_create(uthread_t*, frs_threadid_t*, frs_disc_t, int, int);
extern frs_procd_t* frs_procd_create_header(void);
extern void frs_procd_destroy(frs_procd_t* procd);
extern void frs_procd_destroy_header(frs_procd_t* procd);
extern void frs_procd_signaldequeue(frs_procd_t*);
extern void frs_procd_signalunframesched(frs_procd_t*);
extern void frs_procd_incrref(frs_procd_t*);
extern void frs_procd_decrref(frs_procd_t*);
extern void frs_procd_waitref(frs_procd_t*);
extern tid_t frs_procd_gettid(frs_procd_t*);
#endif /* __SYS_FRAME_PROCD_H__ */
