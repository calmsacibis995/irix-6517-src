/**************************************************************************
 *									  *
 * Copyright (C) 1986, 1987, 1988, 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $"

/* LINTLIBRARY */
/*
 * Lint library for libmpc
 *	Only those functions which are not in standard libc
 */
#include "stdio.h"
#include "ulocks.h"
#include "task.h"

barrier_t *new_barrier(usptr_t *u) { static barrier_t b; return(&b); }
void init_barrier(barrier_t *b) {}
void free_barrier(barrier_t *b) {}
void barrier(barrier_t *b, unsigned u) {}

/* VARARGS1 */
int usconfig(int i, ...) { return(i); }
usptr_t *usinit(const char *c) { static usptr_t *u; return(u); }
int _utrace;
void usputinfo(usptr_t *us, void *v) {}
void *usgetinfo(usptr_t *us) {}

/* AMALLOC(3P) */
void		*acreate(void *v1, size_t s, int i, usptr_t *us,
					void * (*vf)()) { return(v1); }
void		afree(void *v1, void *v2) {}
void		*arealloc(void *v1, size_t s, void *v2) { return(v1); }
void		*amalloc(size_t s, void *v1) { return(v1); }
void		*acalloc(size_t s, size_t s1, void *v1) { return(v1); }
struct mallinfo	amallinfo(void *v1) { struct mallinfo mi; return(mi); }
int		amallopt(int i, int j, void *v1) { return(i); }
void		adelete(void *p) {}

/* USMALLOC(3P) */
void		usfree(void *v1, usptr_t *u) {}
void		*usrealloc(void *v1, size_t s, usptr_t *u) { return(v1); }
void		*usmalloc(size_t s, usptr_t *u) { return((void *)0); }
void		*uscalloc(size_t s, size_t s1, usptr_t *u) { return((void *)0); }
struct mallinfo	usmallinfo(usptr_t *u) { struct mallinfo mi; return(mi); }
int		usmallopt(int i, int j, usptr_t *u) { return(i); }

/* MALLOC(3X) */
void		free(void *v1) {}
void		*realloc(void *v1, size_t s) { return(v1); }
void		*malloc(size_t s) { return((void *)0); }
void		*calloc(size_t s, size_t s1) { return((void *)0); }
struct mallinfo	mallinfo(void) { struct mallinfo mi; return(mi); }
int		mallopt(int i, int j) { return(i); }

/* This seems to make the lint func decls for these calls worthless since
 * any user callers will have the call changed to (*_xxx)
 */
#undef ussetlock
#undef usunsetlock
#undef usnewlock
#undef usinitlock
#undef usfreelock
#undef uswsetlock
#undef uscsetlock
#undef ustestlock
#undef usctllock
#undef usdumplock
#undef uscas

/* LOCKS */
ulock_t		(*_nlock)(usptr_t *us);
int		(*_ilock)(ulock_t t);
void		(*_flock)(ulock_t t, usptr_t *us);
int		(*_wlock)(ulock_t t, unsigned u);
int		(*_clock)(ulock_t t, unsigned u);
int		(*_lock)(ulock_t t);
int		(*_ulock)(ulock_t t);
int		(*_tlock)(ulock_t t);
int		(*_ctlock)(ulock_t t, int j, ...);
int		(*_dlock)(ulock_t t, FILE *fs, const char *c);
int		(*_cas)(void *, int, int, usptr_t *);

ulock_t		usnewlock(usptr_t *us) { static ulock_t s; return(s); }
int		usinitlock(ulock_t t) { static int i; return (i); }
void		usfreelock(ulock_t t, usptr_t *us) {}
int		uswsetlock(ulock_t t, unsigned u) { static int i; return (i); }
int		uscsetlock(ulock_t t, unsigned u) { static int i; return (i); }
int		ussetlock(ulock_t t) { static int i; return (i); }
int		usunsetlock(ulock_t t) { static int i; return (i); }
int		ustestlock(ulock_t t) { static int i; return (i); }
/* VARARGS2 */
int		usctllock(ulock_t t, int j, ...) { static int i; return (i); }
int		usdumplock(ulock_t t, FILE *fs, const char *c) { static int i; return (i); }
int		uscas(void *, int, int, usptr_t *);

/* SEMAPHORES */
usema_t		*usnewsema(usptr_t *us, int i) { static usema_t *s; return(s); }
int 		usinitsema(usema_t *s, int j) { return (j); }
int		uspsema(usema_t *s) { static int i; return (i); }
int		usvsema(usema_t *s) { return(0);}
int		uscpsema(usema_t *s) { static int i; return (i); }
void		usfreesema(usema_t *s, usptr_t *us) {}
int		ustestsema(usema_t *s) { static int i; return (i); }
/* VARARGS2 */
int		usctlsema(usema_t *s, int j, ...) { static int i; return (i); }
int		usdumpsema(usema_t *s, FILE *fs, const char *c) { static int i; return (i); }
usema_t		*usnewpollsema(usptr_t *us, int j) { static usema_t *s; return(s);}
int		usfreepollsema(usema_t *s, usptr_t *us) { return(0);}
int		usopenpollsema(usema_t *s, mode_t m) { return(0);}
int		usclosepollsema(usema_t *s) { return(0);}

/* task calls */
tid_t taskcreate(char *c, void (*v)(), void *v1, int i) { static tid_t t; return(t); }
tskhdr_t _taskheader;
int taskdestroy(tid_t t) { return(0); }
/* VARARGS2 */
int taskctl(tid_t t, unsigned u, ...) { return(0); }
int taskblock(tid_t t) { return(0); }
int taskunblock(tid_t t) { return(0); }
int tasksetblockcnt(tid_t t, int i) { return(0); }

/* not public - but wish to avoid lint error msgs */
tskblk_t *_findtask(tid_t t, tskblk_t **l) { return(*l); }

/* Sequent compatiblity routines */
/* VARARGS1 */
int 		m_fork(void (*vf)(), ...) { return(0); }
int 		m_park_procs() { return(0); }
int 		m_rele_procs() { return(0); }
void 		m_sync() {}
int 		m_kill_procs() { return(0); }
int		m_get_myid() { return(0); }
int		m_set_procs(int i) { return(0); }
int		m_get_numprocs() { return(0); }
unsigned	m_next() { static unsigned u; return(u); }
void 		m_lock() {}		
void 		m_unlock() {}	
#undef cpus_online
int		cpus_online() { return(0); }
