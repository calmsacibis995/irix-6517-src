#ifndef _PTHREADRT_H_
#define _PTHREADRT_H_

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* Pthread entry points called by the C runtime.
 */

/* Global names
 */

#define libc_sem_init		PFX_NAME(libc_sem_init)
#define libc_sem_getvalue	PFX_NAME(libc_sem_getvalue)
#define libc_sem_post		PFX_NAME(libc_sem_post)
#define libc_sem_wait		PFX_NAME(libc_sem_wait)
#define libc_sem_trywait	PFX_NAME(libc_sem_trywait)
#define libc_sem_destroy	PFX_NAME(libc_sem_destroy)

#define libc_blocking		PFX_NAME(libc_blocking)
#define libc_unblocking		PFX_NAME(libc_unblocking)

#define libc_fork		PFX_NAME(libc_fork)
#define libc_sched_yield	PFX_NAME(libc_sched_yield)
#define libc_setrlimit		PFX_NAME(libc_setrlimit)
#define libc_sysconf		PFX_NAME(libc_sysconf)
#define libc_exit		PFX_NAME(libc_exit)

#define libc_sigprocmask	PFX_NAME(libc_sigprocmask)
#define libc_sigaction		PFX_NAME(libc_sigaction)
#define libc_sigwait		PFX_NAME(libc_sigwait)
#define libc_sigtimedwait	PFX_NAME(libc_sigtimedwait)
#define libc_sigpending		PFX_NAME(libc_sigpending)
#define libc_raise		PFX_NAME(libc_raise)
#define libc_sigsuspend		PFX_NAME(libc_sigsuspend)
#define libc_siglongjmp		PFX_NAME(libc_siglongjmp)

#define libc_evt_start		PFX_NAME(libc_evt_start)

#define libc_cncl_test		PFX_NAME(libc_cncl_test)
#define libc_cncl_list		PFX_NAME(libc_cncl_list)

#define libc_threadbind		PFX_NAME(libc_threadbind)
#define libc_threadunbind	PFX_NAME(libc_threadunbind)

#define libc_ptpri		PFX_NAME(libc_ptpri)
#define libc_pttokt		PFX_NAME(libc_pttokt)
#define libc_ptuncncl		PFX_NAME(libc_ptuncncl)

#include <errno.h>
#include <signal.h>
#include <semaphore_internal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

int	libc_sem_init(sem_t *);
int	libc_sem_getvalue(sem_t *, int *);
int	libc_sem_post(sem_t *);
int	libc_sem_wait(sem_t *);
int	libc_sem_trywait(sem_t *);
int	libc_sem_destroy(sem_t *);

int	libc_blocking(int);
int	libc_unblocking(int, int);

pid_t	libc_fork(void);
int	libc_sched_yield(void);
int	libc_setrlimit(int, const struct rlimit *);
int	libc_sysconf(int);
void	libc_exit(int);

int	libc_sigprocmask(int, const sigset_t *, sigset_t *);
int	libc_sigaction(int, const struct sigaction *, struct sigaction *);
int	libc_sigwait(const sigset_t *, int *);
int	libc_sigtimedwait(const sigset_t *, siginfo_t *, const timespec_t *);
int	libc_sigpending(sigset_t *);
int	libc_raise(int);
int	libc_sigsuspend(const sigset_t *);
void	libc_siglongjmp(sigjmp_buf, int);

int	libc_evt_start(void (*)(int), void (*)(void));

int	libc_cncl_test(void);
void	libc_cncl_list(void *);

void	libc_threadbind(void);
void	libc_threadunbind(void);

int	libc_ptpri(void);
int	libc_pttokt(pthread_t, tid_t *);
void	libc_ptuncncl(void);

#endif /* !_PTHREADRT_H_ */
