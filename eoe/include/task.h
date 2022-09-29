/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __TASK_H__
#define __TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 3.38 $"
/*
 * Header for tasking
 */
#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))
#include <sys/types.h>
#endif
#include <ulocks.h>
#include <sys/prctl.h>

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))
/*
 * the library portion of the PRDA
 * WARNING - this is only permitted to be 256 bytes (see sys/prctl.h)
 */
struct prda_lib {
	tid_t		t_tid;
	int		t_errno;

	/* Stuff for the compiler library routines */
	int		auto_mp_id;	/* An id for the MP compiler */
	char		*fpe_data_ptr;	/* For Floating Point Exceptions */
	char		*pixie_data_ptr;/* For Pixie per Thread data */
	int		compiler_extra[1];/* Extra space for future expansion */

	/* Stuff for OpenGL and GLX  */
	void		*glx_client;
	void		*ogl_context;
	void		*ogl_dispatch[7];

	/* stuff for SpeedShop */
	void		*speedshop_data_ptr[5];

	/* stuff for pthreads */
	int		pthread_id;
	int		pthread_cancel;	/* cancelability state */
	void 		*pthread_data[6];
};

/* check at compile time for too large an area */
extern char __prda_lib_sz[257 - sizeof(struct prda_lib)];

#define PRDALIB		((struct prda_lib *)(&PRDA->lib_prda))
#define get_tid()	(PRDALIB->t_tid)
#define get_pid()	((pid_t)(PRDA->t_sys.t_pid))
#define get_rpid()	((pid_t)(PRDA->t_sys.t_rpid))
#define get_cpu()	((int)(PRDA->t_sys.t_cpu))

/*
 * library level task block
 */
typedef volatile struct tskblk {
	void		(*t_entry)(void *);	/* entry point for task */
	void		*t_arg;		/* starting arg */
	tid_t		t_tid;		/* task id */
	pid_t		t_pid;		/* process id */
	unsigned	t_options;	/* debugging, tracing, etc */
	unsigned	t_flags;	/* state flags */
	char		*t_name;	/* name of task */
	struct tskblk	*t_link;	/* list of tasks for process */
} tskblk_t;

/* values for t_options */

/* options for taskctl */
#define TSK_ISBLOCKED	0x0001		/* is task blocked */

extern tskblk_t *_findtask(tid_t, tskblk_t **);

/*
 * prototypes for public functions
 */
extern tid_t taskcreate(char *, void (*)(void *), void *, int);
extern int taskdestroy(tid_t);
extern int taskctl(tid_t, unsigned, ...);
extern int taskblock(tid_t);
extern int taskunblock(tid_t);
extern int tasksetblockcnt(tid_t, int);

/* task header block - one per set of tasks */
typedef struct tskhdr {
	usptr_t		*t_usptr;	/* handle for lock and sema allocs */
	ulock_t		t_listlock;	/* list of tasks lock */
	tskblk_t	*t_list;	/* list of all tasks */
} tskhdr_t;
extern tskhdr_t _taskheader;

/*
 * m_* Routines - Sequent-Like parallel processing primitives
 */
#define _MAXTASKS	256
/* m_lib header block  */
typedef struct mlibhdr {
	ulock_t		*m_listlock;	/* list of tasks lock */
	tid_t		m_list[_MAXTASKS];	/* list of all tasks */
	struct barrier	*m_sbar;	/* m_sync barrier */
	volatile unsigned short	m_lcount;	/* current # threads */
	unsigned short	m_pcount;	/* parallelization count */
	ulock_t		m_glock;	/* global lock manip by m_[un]lock */
	unsigned	m_gcount;	/* global counter manip by m_next */
	ulock_t		m_gcountlock;	/* lock for m_gcount */
} mlibhdr_t;

extern int 		m_fork(void (*)(), ...);
extern int 		m_park_procs(void);
extern int 		m_rele_procs(void);
extern void 		m_sync(void);
extern int 		m_kill_procs(void);
extern int		m_get_myid(void);
extern int		m_set_procs(int);
extern int		m_get_numprocs(void);
extern unsigned		m_next(void);
extern void 		m_lock(void);		
extern void 		m_unlock(void);	


#define cpus_online()	prctl(PR_MAXPPROCS)
#endif /* (_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

#ifdef __cplusplus
}
#endif

#endif /* __TASK_H__ */
