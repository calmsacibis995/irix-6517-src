/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1994, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 **************************************************************************/

#ifndef __SYS_FRS_H__
#define __SYS_FRS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/pthread.h>

/*
 * Public defines for frs
 */

typedef struct frs_fsched_info {
        int cpu;                    /* cpu that will run frs */
        int intr_source;            /* intr defining minor frames */
        int intr_qualifier;         /* period, pipenumber, driverid */
        int n_minors;               /* number of minor frames/major frame */
        tid_t controller;           /* FRS controller */
        int num_slaves;             /* number of sync-slave frs's */
} frs_fsched_info_t;

typedef struct frs_queue_info {
        tid_t controller;           /* target FRS controller */
        tid_t thread;               /* thread to be enqueued */
        int   minor_index;          /* identifies queue */
        uint  discipline;           /* specifies frame boundary policies */
} frs_queue_info_t;

typedef struct frs {
	frs_fsched_info_t frs_info; /* frs description */
	tid_t master_controller;    /* effective master controller */
        tid_t this_controller;      /* this frs's controller thread */
} frs_t;

typedef struct frs_intr_info {
	int intr_source;
	int intr_qualifier;
	int reserved[12];
} frs_intr_info_t;

#define syncm_controller controller

/*
 * Disciplines
 */

#define FRS_DISC_RT                 0x2
#define FRS_DISC_OVERRUNNABLE       0x4
#define FRS_DISC_UNDERRUNNABLE      0x8
#define FRS_DISC_CONT               0x100
#define FRS_DISC_BACKGROUND         (FRS_DISC_RT | \
				     FRS_DISC_OVERRUNNABLE | \
				     FRS_DISC_UNDERRUNNABLE | \
				     FRS_DISC_CONT)

/*
 * Intr Sources
 */

#define FRS_INTRSOURCE_CPUTIMER     0x0001
#define FRS_INTRSOURCE_CCTIMER      0x0002
#define FRS_INTRSOURCE_USER         0x0004
#define FRS_INTRSOURCE_EXTINTR      0x0008
#define FRS_INTRSOURCE_VSYNC        0x0010
#define FRS_INTRSOURCE_DRIVER       0x0020
#define FRS_INTRSOURCE_ULI          0x0040
#define FRS_INTRSOURCE_VARIABLE     0x0080

#define FRS_INTRSOURCE_R4KTIMER     FRS_INTRSOURCE_CPUTIMER

/*
 * Timer limits
 */

#define FRS_TIMER_SHORTEST          500
#define FRS_TIMER_LONGEST           42000000

#define FRS_SYNC_MASTER             0

/*
 * Attributes
 */

typedef enum {
	FRS_ATTR_INVALID,
	FRS_ATTR_RECOVERY,
	FRS_ATTR_SIGNALS,
        FRS_ATTR_OVERRUNS,
	FRS_ATTR_LAST
} frs_attr_t;

typedef struct frs_attr_info {
	tid_t controller;        /* select the frame scheduler */
	int   minor;             /* select minor within a frame sched */
	tid_t thread;            /* select target thread within a minor */
	frs_attr_t attr;         /* select which attribute */
	void*      param;        /* the actual attribute values */
} frs_attr_info_t;

/*
 * Recovery Management
 */

/*
 * Mfberror recovery modes
 */
typedef enum {
	MFBERM_INVALID,
	MFBERM_NORECOVERY,
	MFBERM_INJECTFRAME,
	MFBERM_EXTENDFRAME_STRETCH,
	MFBERM_EXTENDFRAME_STEAL,
	MFBERM_LAST
} mfbe_rmode_t;

typedef enum {
	EFT_INVALID,
	EFT_FIXED,
	EFT_LAST
} mfbe_tmode_t;

typedef struct frs_recv_info {
	mfbe_rmode_t  rmode;               /* Basic recovery mode */
	mfbe_tmode_t  tmode;               /* Time expansion mode */
	uint          maxcerr;             /* Max consecutive errors */
	uint          xtime;               /* Recovery extension time */
} frs_recv_info_t;

/*
 * Signal management.
 * If you set any signal, you must set a value for all signals. A
 * value of 0 will leave the signal unchanged from its current setting 
 */

typedef struct frs_signal_info {
        int sig_underrun;
        int sig_overrun;
        int sig_terminate;
        int sig_dequeue;
        int sig_unframesched;
} frs_signal_info_t;

/*
 * Reading the number of overrun and underrun errors
 */

typedef struct frs_overrun_info {
        int underruns;
        int overruns;
} frs_overrun_info_t;


#ifdef _KERNEL
extern int frs_user_create(void* arg1, void* arg2);
extern int frs_user_destroy(tid_t controller);
extern int frs_user_enqueue(void* arg);
extern int frs_user_join(tid_t controller, long* cminor); 
extern int frs_user_start(tid_t controller);
extern int frs_user_yield(long* cminor);
extern int frs_user_eventintr(tid_t controller);
extern int frs_user_getqueuelen(void* arg, long* len);
extern int frs_user_readqueue(void* arg1, void* arg2, long *retlen);
extern int frs_user_premove(void* arg);
extern int frs_user_pinsert(void* arg1, void* arg2);
extern int frs_user_dequeue(void);
extern int frs_user_stop(tid_t controller);
extern int frs_user_resume(tid_t controller);
extern int frs_user_setattr(void* arg1);
extern int frs_user_getattr(void* arg1);

extern void frs_fsched_info_print(frs_fsched_info_t* info);
extern void frs_queue_info_print(frs_queue_info_t* info);
extern void frs_attr_info_print(frs_attr_info_t* info);

/*
 * Functions exported to other kernel modules
 */

#include <sys/uthread.h>

extern void frs_thread_init(uthread_t* ut);
extern void frs_thread_destroy(uthread_t* ut);
extern void frs_thread_exit(uthread_t* ut);

/*
 * Interface for drivers
 */

#include <sys/groupintr.h>

extern int frs_driver_export(int frs_driver_id,
			     void (*frs_func_set)(intrgroup_t*),
			     void (*frs_func_clear)(void));

extern void frs_handle_driverintr(void);
extern int  vme_frs_install(int, int, intrgroup_t *);
extern int  vme_frs_uninstall(int, int, int);

#else /* _KERNEL */

/*
 * FRS User-level Interfaces
 */ 
extern frs_t* frs_create(int cpu,
                         int intr_source,
                         int intr_qualifier,
                         int n_minors,
                         pid_t sync_master_pid,
                         int num_slaves); 

extern frs_t* frs_create_vmaster(int cpu,
				 int n_minors,
				 int n_slaves,
				 frs_intr_info_t *intr_info);

extern frs_t* frs_create_master(int cpu,
                                int intr_source,
                                int intr_qualifier,
                                int n_minors,
                                int num_slaves);

extern frs_t* frs_create_slave(int cpu, frs_t* sync_master_frs);

extern int frs_destroy(frs_t* frs);
extern int frs_enqueue(frs_t* frs, pid_t pid, int minor_index, uint disc);
extern int frs_start(frs_t* frs);
extern int frs_join(frs_t* frs);
extern int frs_yield(void);
extern int frs_userintr(frs_t* frs);

extern int frs_rtmon_yield(frs_t* frs, pid_t pid);

extern int frs_getqueuelen(frs_t* frs, int minor_index);
extern int frs_readqueue(frs_t* frs, int minor_index, pid_t* pidlist);
extern int frs_premove(frs_t* frs, int minor_index, pid_t remove_pid);
extern int frs_stop(frs_t* frs);
extern int frs_resume(frs_t* frs);

extern int frs_pinsert(frs_t* frs,
		       int minor_index,
		       pid_t insert_pid,
		       uint discipline,
		       pid_t base_pid);

extern int frs_setattr(frs_t* frs,
		       int minor,
		       pid_t pid,
		       frs_attr_t attr,
		       void* param);

extern int frs_getattr(frs_t* frs,
		       int minor,
		       pid_t pid,
		       frs_attr_t attr,
		       void* param);

extern int frs_pthread_enqueue(frs_t* frs,
			       pthread_t target_pthread,
			       int minor_index,
			       uint disc);

extern int frs_pthread_readqueue(frs_t* frs,
				 int minor_index,
				 pthread_t* pt_list);

extern int frs_pthread_remove(frs_t* frs,
			      int minor_index,
			      pthread_t target_pthread);

extern int frs_pthread_insert(frs_t* frs,
			      int minor_index,
			      pthread_t target_pthread,
			      uint discipline,
			      pid_t base_pthread);

extern int frs_pthread_setattr(frs_t* frs,
			       int minor,
			       pthread_t target_pthread,
			       frs_attr_t attr,
			       void* param);

extern int frs_pthread_getattr(frs_t* frs,
			       int minor,
			       pthread_t target_pthread,
			       frs_attr_t attr,
			       void* param);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_FRS_H__ */
