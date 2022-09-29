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

#ifndef __SYS_FRAME_MINOR_H__
#define __SYS_FRAME_MINOR_H__


/*
 * Frame Scheduler - Minor Frames
 */

/*
 * The Global Data Structure View:
 *
 *                                          ***Minor Frames***
 *                                                -----  HEAD   DLL OF Procd's 
 *                                             -->|   |-->[ ]-->[ ]-->[ ]-->
 *                           Minor Pointers   /   |   |   [ ]<--[ ]<--[ ]<--
 * -------------    --------    ----         /    -----
 * | Frame     |--->|Major |--->|  |--------/ ***frs_minor_t***   frs_prod_t
 * | Scheduler |    |Frame |    |--|              -----
 * -------------    |      |    |  |--------------|   |-->[ ]-->[ ]-->[ ]-->
 *                  |      |    |--|              |   |   [ ]<--[ ]<--[ ]<--
 *                  --------    |  |              -----
 *                              |--|
 *                frs_major_t   |  | etc.
 *                              |--|
 *                              
 */

/*
 * NOTE: minor frame objects do not have any locks, since the major frame
 *       lock is always acquired and held across minor frame operations.
 */

typedef struct frs_minor {
	frs_procd_t*     qhead;          /* Task queue header */
	frs_procd_t*     qcurr;          /* Pointer to current task */
	int              nprocs;         /* Number of tasks in queue */
        intrdesc_t       idesc;          /* Local minor interrupt descriptor */
	dpstate_t        dpstate;        /* Current recovery state */
        int              base_period;    /* Base period */
} frs_minor_t;

#define FRS_MINOR_DESTROYED_TOKEN 0x1010

struct proc;
extern frs_minor_t* frs_minor_create(intrdesc_t* intrdesc);
extern void frs_minor_destroy(frs_minor_t* minor);
extern void frs_minor_enqueue(frs_minor_t* minor, frs_procd_t* procd);
extern frs_procd_t* frs_minor_dequeue(frs_minor_t* minor);
extern void frs_minor_start(frs_minor_t* minor);
extern frs_procd_t* frs_minor_delproc(frs_minor_t*, uthread_t*);
extern frs_procd_t* frs_minor_getprocd(frs_minor_t* minor);
extern int frs_minor_getnprocs(frs_minor_t* minor);
extern int frs_minor_check(frs_minor_t* minor);
extern void frs_minor_eoframe(frs_minor_t* minor);
extern void frs_minor_reset(frs_minor_t* minor);
extern int frs_minor_rewind(frs_minor_t* minor);
extern int frs_minor_readqueue(frs_minor_t* minor, pid_t* pids, int maxlen);
extern int frs_minor_pinsert(frs_minor_t* minor, pid_t basepid, frs_procd_t* newprocd);
extern int frs_minor_get_overruns(frs_minor_t* minor, pid_t pid,
                                  frs_overrun_info_t* overrun_info);
extern void frs_minor_print(frs_minor_t* minor);
#endif /*  __SYS_FRAME_MINOR_H__ */
