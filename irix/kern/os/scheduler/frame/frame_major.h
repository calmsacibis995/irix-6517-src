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

#ifndef __SYS_FRAME_MAJOR_H__
#define __SYS_FRAME_MAJOR_H__

#include "frame_barrier.h"

/*
 * Frame Scheduler - Major Frames
 */


/*
 * The Global Data Structure View:
 *
 *                                          Minor Frames
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
 *             **frs_major_t**  |  | etc.
 *                              |--|
 *                              
 */



typedef struct frs_major {
	int n_minors;
	frs_minor_t** p_minors;
	frs_minor_t* c_minor;
	int i_minor;
        uint mfbe_cerr;
	lock_t lock;
} frs_major_t;

#if IP30
#define frs_major_lock(m)		mutex_spinlock_spl(&(m)->lock, splprof)
#else
#define frs_major_lock(m)		mutex_spinlock(&(m)->lock)
#endif

#define frs_major_unlock(m, s)		mutex_spinunlock(&(m)->lock, (s))

#define frs_major_nested_lock(m)			\
{							\
	ASSERT(issplhi(getsr()) || issplprof(getsr())); \
	nested_spinlock(&(m)->lock);			\
}

#define frs_major_nested_unlock(m)	nested_spinunlock(&(m)->lock)

/*
 * Commands to frs_major_getidesc()
 */
#define FRS_IDESC_FIRST		1
#define FRS_IDESC_CURRENT	2
#define FRS_IDESC_NEXT		3

struct frs_fsched;

extern int  frs_major_create(struct frs_fsched*, frs_fsched_info_t*, frs_intr_info_t*);
extern int  frs_major_clone(struct frs_fsched*, struct frs_fsched*);
extern kthread_t* frs_major_choosethread(frs_major_t*);
extern void frs_major_destroy(frs_major_t*);
extern void frs_major_enqueue(frs_major_t*, int, frs_procd_t*);
extern void frs_major_start(frs_major_t* major);
extern void frs_major_unframesched(frs_major_t*, uthread_t*);
extern void frs_major_unframesched_allprocs(frs_major_t* major);
extern int frs_major_getnminors(frs_major_t* major);
extern int frs_major_verifyminor(frs_major_t* major, int);
extern int frs_major_getcminor(frs_major_t* major);
extern int frs_major_step(struct frs_fsched*);
extern intrdesc_t* frs_major_getidesc(frs_major_t*, int);
extern int frs_major_getqueuelen(frs_major_t*, int, long*);
extern int frs_major_readqueue(frs_major_t*, int, pid_t*, int);
extern int frs_major_premove(frs_major_t*, int, uthread_t*);
extern int frs_major_pinsert(frs_major_t*, int, pid_t, frs_procd_t*);
extern int frs_major_getoverruns(frs_major_t*, int, pid_t, frs_overrun_info_t*);
extern void frs_major_print(frs_major_t*);

#endif /* __SYS_FRAME_MAJOR_H__ */
