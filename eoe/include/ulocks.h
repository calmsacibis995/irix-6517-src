/**************************************************************************
 *									  *
 * Copyright (C) 1986-1992 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __ULOCKS_H__
#define __ULOCKS_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 3.55 $ $Author: joecd $"

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#endif
#include <limits.h>

#define _DEFUSUSERS	 8	/* default number of users */
#define _MAXUSUSERS	10000	/* maximum number of users allowed */
#define _USMMAPSIZE	65536	/* default size of shared area */
#define _MAXLOCKS	4096	/* maximum number of locks per share group */
#define	_USDEFSPIN	600	/* default spin for hardware locks */

/* flags used to distinguish between different types of locks (usconfig) */
#define US_NODEBUG	0	/* no debugging */
#define US_DEBUG	1	/* default debug & meter */
#define US_DEBUGPLUS	2	/* DEBUG and check for double trip, etc */
#define US_CASTYPE	3	/* internal only */
/* for 3.2 compatiblity! */
#define _USNODEBUG	0	/* no debugging */
#define _USDEBUG	1	/* default debug & meter */
#define _USDEBUGPLUS	2	/* DEBUG and check for double trip, etc */

/* flags used to distinguish SHARED and general arena (usconfig) */
/* Fortran does not like hex constants as 0x */
#define US_GENERAL	0	/* both shared/unshared accedd */
#define US_SHAREDONLY	1	/* only shared proc access */
#define US_TRACING	2	/* shared tracing */
#define US_CASONLY	4	/* arena supports only cas operations */

/*
 *  Flags for user lock calls 
 */

/* USCONFIG commands */
#define CONF_INITUSERS		1	/* set max # processes using a lock */
#define CONF_INITSIZE		2	/* set size of arena */
#define CONF_GETUSERS		3	/* return max # processes */
#define CONF_GETSIZE		4	/* return size of arena */
#define CONF_HISTON		5	/* enable global semaphore history */
#define CONF_HISTOFF		6	/* disable global semaphore history */
#define CONF_HISTFETCH		7	/* fetch history */
#define CONF_HISTRESET		8	/* clear all semaphore history */
#define CONF_LOCKTYPE		9	/* set locktype */
#define CONF_STHREADIOON	10	/* enable single-threading of stdio */
#define CONF_STHREADIOOFF	11	/* disable single-threading of stdio */
#define CONF_STHREADMISCON	12	/* enable misc single threading */
#define CONF_STHREADMISCOFF	13	/* disable misc single threading */
#define CONF_STHREADMALLOCON	14	/* enable malloc single threading */
#define CONF_STHREADMALLOCOFF	15	/* disable malloc single threading */
#define CONF_ARENATYPE		16	/* set whether supports SHARED only
					 * access */
#define CONF_CHMOD		17	/* set access rights to locks, etc. */
#define CONF_HISTSIZE		18	/* max # of entries in history */
#define CONF_ATTACHADDR		19	/* set arena attach address */
#define CONF_AUTOGROW		20	/* set whether arena should autogrow */
#define CONF_AUTORESV		21	/* set whether arena should autoresv */

/* USCTLLOCK commands */
#define CL_METERFETCH	3
#define CL_METERRESET	4
#define CL_DEBUGFETCH	7
#define CL_DEBUGRESET	8

/* USCTLSEMA commands */
#define CS_METERON	9
#define CS_METEROFF	10
#define CS_METERFETCH	11
#define CS_METERRESET	12
#define CS_DEBUGON	13
#define CS_DEBUGOFF	14
#define CS_DEBUGFETCH	15
#define CS_DEBUGRESET	16
#define CS_HISTON	17
#define CS_HISTOFF	18
#define CS_RECURSIVEON	19
#define CS_RECURSIVEOFF	20
#define CS_QUEUEFIFO	21
#define CS_QUEUEPRIO	22


/* The possible h_op - operations on a semaphore used in history logging */
#define HOP_PHIT	1	/* us[c]psema:got sema w/o waiting */
#define HOP_PSLEEP	2	/* uspsema:sema not available */
#define HOP_PWOKE	3	/* uspsema:woken with semaphore */
#define HOP_VHIT	4	/* usvsema:no one on queue */
#define HOP_VWAKE	5	/* usvsema:woke first on queue */
#define HOP_PMISS	7	/* uscpsema:sema locked */

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))

extern int _utrace;		/* this flag sets tracing mode on (USTRACE) */
extern int _uerror;		/* just turn on printing errors (USERROR) */

/*
 * arena handle
 */
typedef void usptr_t;

/*
 * Spinlock data structures.
 */
typedef struct lockmeter_s {
	int	lm_spins;			/* # times lock spun out */
	int	lm_tries;			/* # times lock requested */
	int	lm_hits;			/* # times lock acquired */
} lockmeter_t;

typedef struct lockdebug_s {
	pid_t	ld_owner_pid;			/* pid of owning process */
} lockdebug_t;

typedef void *ulock_t;				/* opaque lock handle */

/*
 * Semaphore data structures
 */
typedef void usema_t;

/* semaphore history logging structure */
typedef struct hist_s {
	usema_t			*h_sem;		/* ptr to semaphore */
	int			h_op;		/* operation */
	pid_t			h_pid;		/* process id */
	int			h_scnt;		/* value of semaphore */
	pid_t			h_wpid;		/* woken process id if any */
	char			*h_cpc;		/* calling program counter */
	struct hist_s		*h_next;	/* link list of hist structs */
	struct hist_s		*h_last;	/* link list of hist structs */
} hist_t;

typedef struct histptr_s {
	hist_t	*hp_current;			/* ptr to last hist_t */
	int	hp_entries;			/* # of entries */
	int	hp_errors;			/* errors due to lack of mem */
} histptr_t;

typedef struct semameter_s { 
	int	sm_phits;	/* # of uspsema w/o waiting */
	int	sm_psemas;	/* # of us[c]psema calls */
	int	sm_vsemas;	/* # of usvsema calls */
	int	sm_vnowait;	/* # of usvsema's w/ noone waiting */
	int	sm_nwait;	/* length of wait queue */
	int	sm_maxnwait;	/* max length of wait queue */
} semameter_t;

typedef struct semadebug_s {
	pid_t	sd_owner_pid;	/* pid of current owner (or -1) */
	char	*sd_owner_pc;	/* program counter of owner psema */
	pid_t	sd_last_pid;	/* pid of last op caller */
	char	*sd_last_pc;	/* program counter of last op caller */
} semadebug_t;

/*
 * Barrier definitions
 */
typedef struct barrier {
	volatile unsigned char b_inuse;	/* if barrier is being used */
	unsigned char b_count;		/* # procs waiting AT barrier */
	unsigned char b_waitcnt;	/* # procs waiting to re-use barrier */
	volatile unsigned char b_spin;	/* waiting at barrier */
	ulock_t b_plock;		/* struct lock */
	usptr_t *b_usptr;		/* usheader used to alloc barrier */
} barrier_t;

extern barrier_t *new_barrier(usptr_t *);
extern void init_barrier(barrier_t *);
extern void free_barrier(barrier_t *);
extern void barrier(barrier_t *, unsigned);

/*
 * these define the lock routines as indirect function calls so they
 * can be defined at initialization time to use various flavors of locks
 */

#define ussetlock 	(*_lock)  
#define usunsetlock 	(*_ulock)  
#define usnewlock	(*_nlock)    
#define usinitlock	(*_ilock)
#define usfreelock	(*_freelock)
#define uswsetlock	(*_wlock)
#define uscsetlock	(*_clock)
#define ustestlock	(*_tlock)
#define usctllock	(*_ctlock)
#define usdumplock	(*_dlock)
#define uscas		(*_cas)
#define uscas32		(*_cas32)

/*
 * Routine prototypes.
 */

extern ptrdiff_t	usconfig(int, ...);
extern usptr_t		*usinit(const char *);
extern void		usdetach(usptr_t *);
extern int		usadd(usptr_t *);
extern void		usputinfo(usptr_t *, void *);
extern void		*usgetinfo(usptr_t *);
extern int		uscasinfo(usptr_t *, void *, void *);

/* USMALLOC */
extern void		*usmalloc(size_t, usptr_t *);
extern void		usfree(void *, usptr_t *);
extern void		*usrealloc(void *, size_t, usptr_t *);
extern void		*usrecalloc(void *, size_t, size_t, usptr_t *);
extern void		*uscalloc(size_t, size_t, usptr_t *);
extern struct mallinfo	usmallinfo(usptr_t *);
extern int		usmallopt(int, int, usptr_t *);
extern size_t		usmallocblksize(void *, usptr_t *);
extern void		*usmemalign(size_t, size_t, usptr_t *);

/* LOCKS */
extern ulock_t 		usnewlock(usptr_t *);
extern int		usinitlock(ulock_t);
extern void		usfreelock(ulock_t, usptr_t *);
extern int		uswsetlock(ulock_t, unsigned);
extern int		uscsetlock(ulock_t, unsigned);
extern int		ussetlock(ulock_t);
extern int		usunsetlock(ulock_t);
extern int		ustestlock(ulock_t);
extern int		usctllock(ulock_t, int, ...);
extern int		usdumplock(ulock_t, FILE *, const char *);
extern int		uscas(void *, ptrdiff_t, ptrdiff_t, usptr_t *);
extern int		uscas32(void *, int32_t, int32_t, usptr_t *);

/* SEMAPHORES */
extern usema_t		*usnewsema(usptr_t *, int);
extern int 		usinitsema(usema_t *, int);
extern int		uspsema(usema_t *);
extern int		usvsema(usema_t *);
extern int		uscpsema(usema_t *);
extern void		usfreesema(usema_t *, usptr_t *);
extern int		ustestsema(usema_t *);
extern int		usctlsema(usema_t *, int, ...);
extern int		usdumpsema(usema_t *, FILE *, const char *);
extern usema_t		*usnewpollsema(usptr_t *, int);
extern int		usfreepollsema(usema_t *, usptr_t *);
extern int		usopenpollsema(usema_t *, mode_t);
extern int		usclosepollsema(usema_t *);

#endif /* (_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

#if defined(_LANGUAGE_FORTRAN)
/* LOCKS */
	INTEGER*4 ussetlock, usunsetlock, usnewlock, usinitlock
	INTEGER*4 uswsetlock, uscsetlock, ustestlock, usctllock, uscas, uscas32

/* ARENA */
#if (_MIPS_SZPTR == 64)
	INTEGER*8 usconfig, usinit, usgetinfo
	INTEGER*8 usmalloc, usrealloc, uscalloc, usrecalloc
#else
	INTEGER*4 usconfig, usinit, usgetinfo
	INTEGER*4 usmalloc, usrealloc, uscalloc, usrecalloc
#endif
	INTEGER*4 usmallopt

/* SEMAPHORES */

#if (_MIPS_SZPTR == 64)
	INTEGER*8 usnewsema, usnewpollsema
#else
	INTEGER*4 usnewsema, usnewpollsema
#endif
	INTEGER*4 usinitsema, uspsema, uscpsema, usvsema
	INTEGER*4 ustestsema, usctlsema, usdumpsema
	INTEGER*4 usfreepollsema, usopenpollsema, usclosepollsema

#endif /* (_LANGUAGE_FORTRAN) */

#ifdef __cplusplus
}
#endif

#include <task.h>

#endif /* __ULOCKS_H__ */
