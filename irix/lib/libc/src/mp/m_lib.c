/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.36 $ $Author: roe $"

#ifdef __STDC__
	#pragma weak m_fork = _m_fork
	#pragma weak m_get_myid = _m_get_myid
	#pragma weak m_get_numprocs = _m_get_numprocs
	#pragma weak m_kill_procs = _m_kill_procs
	#pragma weak m_lock = _m_lock
	#pragma weak m_next = _m_next
	#pragma weak m_park_procs = _m_park_procs
	#pragma weak m_rele_procs = _m_rele_procs
	#pragma weak m_set_procs = _m_set_procs
	#pragma weak m_sync = _m_sync
	#pragma weak m_unlock = _m_unlock
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "shlib.h"
#include "unistd.h"
#include "stdlib.h"
#include "ulocks.h"
#include "task.h"
#include "stdio.h"
#include "errno.h"
#include "signal.h"
#include "stdarg.h"
#include "sys/wait.h"
#include "sys/prctl.h"
#include "mp_extern.h"

typedef void (*ptofv6)(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6);

static void dosub(void *);
static void dodeath(void *, void *, void *, void *, void *, void *);
static int m_init(void);
static void cldcatcher(int);
static int init(void);

/*
 * simple Sequent-like multi-tasking interface
 */
struct argpass {
	ptofv6 entry;
	void *args[6];
};
static volatile struct argpass *argpass;

/* task header block */
static mlibhdr_t *_mlibheader;

#define CHKINIT	(argpass == NULL && init())
static int
init(void)
{
	if (argpass == NULL) {
		/* real first time initialization */
		if ((argpass = (volatile struct argpass *)malloc(sizeof(*argpass))) == NULL) {
			if (_utrace)
				perror("m_lib: can't get memory for argpass");
			return -1;
		}
		if ((_mlibheader = (mlibhdr_t *) calloc(1, sizeof(mlibhdr_t))) == NULL) {
			if (_utrace)
				perror("m_lib: can't get memory for mlibhdr");
			free((void *)argpass);
			argpass = NULL;
			return -1;
		}
	}
	return 0;
}

/*
 * m_fork - start up a pool of process, each executing the same subprogram
 * Upon completion all but the caller (task 0) will spin waiting for
 * more work
 */

/* VARARGS1 */
int
m_fork(void (*entry)(), ...)
{
	va_list ap;
	register int ousers, i, np;
	static int firstsproc;

	if (CHKINIT)
		return -1;
	va_start(ap, entry);
	/* initialize m_lib stuff if necessary */
	if (_mlibheader->m_sbar == NULL) {
		/* make sure basic tasking is initialized */
		if (_taskinit(_MAXTASKS) != 0) {
			if (_utrace)
				perror("m_fork: task initialization failed");
			return -1;
		}
		if (m_init() < 0) {
			if (_utrace)
				perror("m_fork: barrier initialization failed");
			return -1;
		}
	}

	/* if not the parent, can't call m_fork */
	if (m_get_myid() != 0) {
		setoserror(EINVAL);
		return -1;
	}

	/* always reset global counter so that people can call m_fork again */
	_mlibheader->m_gcount = 0;

	/*
	 * if change # of threads, kill them off since otherwise
	 * barrier below won't work properly
	 */
	if (_mlibheader->m_lcount != _mlibheader->m_pcount)
		m_kill_procs();

	/* fill in args */
	argpass->entry = entry;
	argpass->args[0] = va_arg(ap, void *);
	argpass->args[1] = va_arg(ap, void *);
	argpass->args[2] = va_arg(ap, void *);
	argpass->args[3] = va_arg(ap, void *);
	argpass->args[4] = va_arg(ap, void *);
	argpass->args[5] = va_arg(ap, void *);

	/* crank up any needed sub-processes */
	np = _mlibheader->m_lcount;
	/*
	 * set lcount now - we don't really want to use pcount much
	 * since user can set that via m_set_procs
	 */
	_mlibheader->m_lcount = _mlibheader->m_pcount;
	while ((unsigned int)np < _mlibheader->m_pcount) {
		if (!firstsproc)
			ousers = (int)usconfig(CONF_INITUSERS, _MAXTASKS);
		if ((i = sproc(dosub, PR_SALL, (void *)(ptrdiff_t)np)) < 0) {
			if (_utrace)
				perror("m_fork:sproc failed");
			for (i = 1 ; _mlibheader->m_list[i] != -1 ; i++) {
				kill(_mlibheader->m_list[i], SIGKILL);
				_mlibheader->m_list[i] = -1;
			}
			_mlibheader->m_lcount = 1;
			return -1;
		}
		if (!firstsproc) {
			firstsproc = 1;
			usconfig(CONF_INITUSERS, ousers);
		}
		/* add i to m_list */
		_mlibheader->m_list[np] = i;
		if (_utrace) 
			fprintf(stdout, "TRACE: m_fork created pid %d\n", i);
		np++;
	}

	/* when we join the party, we'll all get released */
	barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);

	/* do work */
	(*argpass->entry)(argpass->args[0], argpass->args[1], argpass->args[2],
		 argpass->args[3], argpass->args[4], argpass->args[5]);

	/* wait at barrier for all to finish */
	barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);
	return 0;
}

static void
dosub(void *u)
{
	int us = (int)(ptrdiff_t)u;

	/* set our small integer id - simply index into m_list */
	get_tid() = us;
	for (;;) {
		barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);
		(*argpass->entry)(argpass->args[0], argpass->args[1],
				 argpass->args[2], argpass->args[3],
				 argpass->args[4], argpass->args[5]);
		/* line up at barrier again */
		barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);
	}
	/* NOTREACHED */
}

/*
 * m_kill_procs - kill off all tasks
 */
int
m_kill_procs(void)
{
	register int i;

	if (m_get_myid() != 0) {
		setoserror(EINVAL);
		return -1;
	}

	/* release any parked threads */
	for (i = 1; i < _MAXTASKS; i++) {
		if (_mlibheader->m_list[i] == -1)
			break;
		setblockproccnt(_mlibheader->m_list[i], 0);
	}

	/* set children NOT to send signal */
	prctl(PR_SETEXITSIG, 0);

	/* force all threads to execute death routine */
	argpass->entry = dodeath;

	/* when we join the party, we'll all get released */
	barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);

	/* now reap all children */
	for (i = 1; i < _MAXTASKS; i++) {
		if (_mlibheader->m_list[i] != -1)
			while (waitpid(_mlibheader->m_list[i], NULL, 0) < 0) {
				/* NOTE - since in the utrace case we catch
				 * SIGCLD - we may have already waited for
				 * all these guys
				 */
				if (oserror() == ECHILD)
					break;
			}
	}

	/* initialize barrier for subsequent m_fork calls */ 
	init_barrier(_mlibheader->m_sbar);
	for (i = 1; i < _MAXTASKS; i++)
		_mlibheader->m_list[i] = -1;
	_mlibheader->m_lcount = 1;

	/* re-enable signalling - we do it here so that normal m_forks
	 * do not have to make an extra system call
	 */
	prctl(PR_SETEXITSIG, SIGTERM);

	return 0;
}

/* ARGSUSED */
static void
dodeath(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
{
	/* die off gracefully */
	m_lock();
	_mlibheader->m_lcount--;
	m_unlock();
	exit(0);
}

/*
 * m_park_procs - block all tasks other than task 0
 */
int
m_park_procs(void)
{
	register int i;
	register int err = 0;

	if (m_get_myid() != 0) {
		setoserror(EINVAL);
		return -1;
	}

	for (i = 1; _mlibheader->m_list[i] != -1 ; i++) {
		if (blockproc(_mlibheader->m_list[i]) < 0) {
			err++;
			if (_utrace)
				fprintf(stderr,
				"m_park_procs: block failed for pid %d\n",
				_mlibheader->m_list[i]);
		}
	}

	if (err)
		return -1;
	else
		return 0;
}

/*
 * m_rele_procs - unblock all tasks other than task 0
 */
int
m_rele_procs(void)
{
	register int i;
	register int err=0;

	if (m_get_myid() != 0) {
		setoserror(EINVAL);
		return -1;
	}

	for (i = 1 ; _mlibheader->m_list[i] != -1 ; i++) {
		if (unblockproc(_mlibheader->m_list[i]) < 0) {
			err++;
			if (_utrace)
				fprintf(stderr,
				"m_rele_procs: block failed for pid %d\n", 
				_mlibheader->m_list[i]);
		}
	}

	if (err)
		return -1;
	else
		return 0;
}

/*
 * m_sync - all tasks check into a barrier and gcount is reset 
 */
void
m_sync(void)
{
	/* if last one in, reset global counter */
	barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);
	_mlibheader->m_gcount = 0;
	/* wait for all tasks to arrive at this point */
	barrier(_mlibheader->m_sbar, _mlibheader->m_lcount);
}

/*
 * m_init - initialize stuff for m_lib
 * MUST be called after taskinit
 */
static int
m_init(void)
{
	struct sigaction oldcld_sig, newcld_sig;
	register int i;

	if ((_mlibheader->m_sbar = new_barrier(_taskheader.t_usptr)) 
							== (barrier_t *) NULL) {
		if (_utrace)
			fprintf(stderr, "m_init: can't allocate barrier\n");
		return -1;
	}

	if ((_mlibheader->m_glock = usnewlock(_taskheader.t_usptr)) 
							== (ulock_t)NULL) {
		free_barrier(_mlibheader->m_sbar);
		if (_utrace)
			fprintf(stderr, "m_init: can't allocate lock\n");
		return -1;
	}
	if ((_mlibheader->m_gcountlock = usnewlock(_taskheader.t_usptr))
							== (ulock_t) NULL) {
		free_barrier(_mlibheader->m_sbar);
		usfreelock(_mlibheader->m_glock, _taskheader.t_usptr);
		if (_utrace)
			fprintf(stderr, "m_init: can't allocate lock\n");
		return -1;
	}
	if (_mlibheader->m_pcount == 0) 
		_mlibheader->m_pcount = (unsigned short) prctl(PR_MAXPPROCS);

	for (i = 1; i < _MAXTASKS; i++)
		_mlibheader->m_list[i] = -1;
	_mlibheader->m_list[0] = get_pid();
	get_tid() = 0;
	_mlibheader->m_lcount = 1;

	/* only catch SIGCLD if _utrace on */
	if (_utrace) {
		sigaction(SIGCLD, NULL, &oldcld_sig);
		if (oldcld_sig.sa_handler == SIG_IGN ||
					oldcld_sig.sa_handler == SIG_DFL) {
			newcld_sig.sa_handler = cldcatcher;
			newcld_sig.sa_flags = SA_NOCLDSTOP;
			sigemptyset(&newcld_sig.sa_mask);/* block no signals */
			(void) sigaction(SIGCLD, &newcld_sig, NULL);
			fprintf(stdout, "TRACE: m_fork - catching SIGCLD\n");
		}
	} else {
		/* if any child dies, kill off everyone */
		prctl(PR_SETEXITSIG, SIGTERM);
	}

	return 0;
}

/* 
 * m_set_numprocs
 *	set the number of processes to sproc in m_fork
 */
int
m_set_procs(int nprocs)
{
	if (nprocs <= 0 || nprocs > _MAXTASKS) {
		setoserror(EINVAL);
		return -1;
	}
	if (CHKINIT)
		return -1;
	_mlibheader->m_pcount = (unsigned short) nprocs;
	return 0;
}

/* 
 * m_get_numprocs
 *	get the numer of processes that will be sproced
 */
int
m_get_numprocs(void)
{
	if (CHKINIT)
		return -1;
	if (_mlibheader->m_pcount == 0)
		_mlibheader->m_pcount = (unsigned short) prctl(PR_MAXPPROCS);
	return _mlibheader->m_pcount;
}

/* 
 * m_get_myid
 *	get the task id
 */
int
m_get_myid(void)
{
	return(get_tid());
}

/*
 * m_next - increment global counter
 */
unsigned
m_next(void)
{
	register unsigned i;

	ussetlock(_mlibheader->m_gcountlock);
	i = _mlibheader->m_gcount++;
	usunsetlock(_mlibheader->m_gcountlock);
	return i;
}

/*
 * m_lock - lock the global lock
 */
void
m_lock(void)
{
	ussetlock(_mlibheader->m_glock);
}

/*
 * m_unlock - unlock the global lock
 */
void
m_unlock(void)
{
	usunsetlock(_mlibheader->m_glock);
}

/* ARGSUSED */
static void
cldcatcher(int signo)
{
	struct sigaction newcld_sig;
	auto int status;
	pid_t pid;

	if ((pid = wait(&status)) < 0) {
		if (_utrace)
			fprintf(stdout, "SIGCLD but no process! errno:%d\n", oserror());
	} else {
		if (_utrace) {
			fprintf(stdout,"TRACE: process %d died due to ",pid);
			if ((status & 0xff) == 0) {
				/* exit */
				fprintf(stdout,"exitting. status %d\n",
						status >> 8);
			} else {
				/* signal */
				fprintf(stdout,"a signal. #%d\n",status & 0xff);
			}
		}
	}
	/* look for more children */
	newcld_sig.sa_handler = cldcatcher;
	newcld_sig.sa_flags = SA_NOCLDSTOP;
	sigemptyset(&newcld_sig.sa_mask);/* block no signals */
	(void) sigaction(SIGCLD, &newcld_sig, NULL);
}
