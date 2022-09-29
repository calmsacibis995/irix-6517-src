/*
 *	Copyright (C) 1989-1997 Softway Pty Ltd, Sydney Australia.
 *	All Rights Reserved.
 */

/*
 *	This is unpublished proprietary source code of Softway Pty Ltd.
 *	The contents of this file are protected by copyright laws and
 *	international copyright treaties, as well as other intellectual
 *	property laws and treaties.  These contents may not be extracted,
 *	copied,	modified or redistributed either as a whole or part thereof
 *	in any form, and may not be used directly in, or to assist with,
 *	the creation of derivative works of any nature without prior
 *	written permission from Softway Pty Ltd.  The above copyright notice
 *	does not evidence any actual or intended publication of this file.
 */
/*
 *	shareIIstubs.h
 *
 *	ShareII System Kernel Hooks header.
 */

#ifndef	_SYS_SHAREIISTUBS_H
#define	_SYS_SHAREIISTUBS_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _SHAREII

#include	<sys/types.h>
#include	<sys/systm.h>
#include	<sys/cred.h>
#include	<sys/syssgi.h>
#include	<sys/pda.h>
struct prpsinfo;
struct irix5_prpsinfo;

/*
 * The purpose of SHR_IF_MAJOR and SHR_IF_MINOR are to ensure consistency
 * of the version of ShareII interface mechanism used. We want to ensure
 * that the interface that a vendor uses to compile their kernel with
 * is the same version (or compatible with) that used to compile the
 * ShareII module/library.
 * _SHAREII_INTERFACE_VERSION is a string "SHR_IF_MAJOR.SHR_IF_MINOR".
 *
 * At initialisation ShareII checks the content of _shareii_interface_version
 * and may require that a certain version is current or may adapt how it
 * presents the interface based on what the kernel expects.
 *
 * NOTE: Subsequent minor level versions at the same major level are
 * expected to be backwards compatible with the preceeding minor level.
 * However, no compatibility is guaranteed between different major levels.
 */

#define	SHR_IF_MAJOR	1
#define	SHR_IF_MINOR	4

#define	SHR_STRINGIT(s)	#s
#define	SHR_MK_VERSION(m, n)	SHR_STRINGIT(m) "." SHR_STRINGIT(n)

extern const char	_shareii_interface_version[];
#define	_SHAREII_INTERFACE_VERSION	\
		SHR_MK_VERSION(SHR_IF_MAJOR, SHR_IF_MINOR)


/*
 *	This pointer is NULL while the ShareII module is not loaded
 *	or has not initialised yet. During initialisation it is set
 *	to point at a structure populated with addresses of the hooks.
 */
extern struct _shareii_interface	*shareii_hook;

struct shaddr_s;
struct syssgia;
/*
 *	Prototypes for all the Share hooks called from the mainline
 *	kernel code.
 */

# ifndef	ShrProc_t
#  include	<sys/proc.h>
#  ifdef _KERNEL
#   include	<ksys/vproc.h>			/* new vproc_t */
#   include	"os/proc/pproc_private.h"	/* real proc_t */
#  else /* _KERNEL */
struct proc;
typedef struct proc proc_t;
struct vproc;
typedef struct vproc vproc_t;
#  endif /* _KERNEL */
#  include	<sys/immu.h> /* pde_t */
#  include	<sys/timers.h> /* ktimer_t */
#  include	<sys/uthread.h>
#  include	<sys/systm.h> /* rval_t */

#  undef _KERNEL__undef_before_shareIIstubs
#  ifndef _KERNEL
#   define _KERNEL 1
#   define _KERNEL__undef_before_shareIIstubs 1
#  endif /*_KERNEL wasn't defined*/
#  include	<sys/pda.h> /* cpu_t */
#  ifdef _KERNEL__undef_before_shareIIstubs
#   undef _KERNEL
#   undef _KERNEL__undef_before_shareIIstubs
#  endif /*_KERNEL re-undef'd*/

#  define	ShrProc_t		proc_t		/* machdep.h */
#  define	ShrThread_t		uthread_t	/* machdep.h */
#  define	ShrThreadId_t		uthread_t *	/* machdep.h */
# endif	/* ShrProc_t */

typedef struct _shareii_interface {
	int	(*start)(void);
	int	(*setattr)(long, mode_t *);
	int	(*limitdisk)(struct vfs *, uid_t, u_long, int, int,
					u_long *, cred_t *);
	int	(*limitmemory)(size_t, int);
	int	(*chowndisk)(struct vfs *, uid_t, uid_t, u_long, int, cred_t *);
	void	(*exit)(ShrProc_t *);
	void	(*flush)(u_long);
	int	(*umask)(void);
	int	(*proccreate)(ShrProc_t *, int);
	void	(*procdestroy)(ShrProc_t *);
	void	(*pcntup)(ShrThreadId_t);
	void	(*pcntdn)(ShrThreadId_t);
	void	(*rdwr)(struct vnode *, int, int);
	void	(*bio)(int);
	void	(*seteuid)(uid_t);
	void	(*setruid)(uid_t);
	void	(*swtch)(ShrThreadId_t);
	void	(*syscall)(void);
	void	(*threadfail)(ShrProc_t *, ShrThreadId_t);
	void	(*threadexit)(ShrThreadId_t);
	int	(*threadnew)(ShrThreadId_t parent, ShrThreadId_t child);
	int	(*iaccess)(struct vnode *, uid_t, uid_t, mode_t, mode_t,
					cred_t *);
	int	(*limsys)(struct syssgia *, rval_t *);
	/*
	 * SGI has two flavours of binaries, 
	 * and we need to deal with the right variant
	 * of prpsinfo_t.
	 */
	void	(*irix5_prgetpsinfo)(ShrProc_t *, struct irix5_prpsinfo *);
	void	(*prgetpsinfo)(ShrProc_t *, struct prpsinfo *);
	int	(*setSchedulerClass)(ShrThreadId_t, int, int);
	micro_t	(*getWeight)(ShrThreadId_t);
	void	(*tick)(ShrThreadId_t, int how, micro_t increment);
} _shareii_interface;

#define SHR_SAFE	shareii_hook

/*
 * We mark all of the ShareII routines as infrequent in order to cause any
 * code necessary to call them to be compiled out of line.  This penalizes
 * sites that use ShareII slightly and rewards those that don't.  Since the
 * ShareII calls already involve an out-of-line function call this seems like
 * a good tradeoff.
 */

static __inline void shr_never(void) {}
#pragma mips_frequency_hint NEVER shr_never

/* start func is by design vararg'd
 * the underlying start() arg types vary with the specific implementation
 */
#define	SHR_START	if (!shareii_hook) /*nothing*/; \
			else (shr_never(), \
			      shareii_hook->start) /* called with (arg1, args...); */

#define	SHR_SETATTR(w, m) \
	(SHR_SAFE ? (shr_never(), shareii_hook->setattr(w, m)) : 0)

#define	SHR_LIMITDISK(v, u, n, b, l, s, c) \
	(SHR_SAFE \
	 ? (shr_never(), shareii_hook->limitdisk(v, u, n, b, l, s, c)) : 0)

#define	SHR_LIMITMEMORY(n, l) \
	(SHR_SAFE ? (shr_never(), shareii_hook->limitmemory(n, l)) : 0)

#define	SHR_CHOWNDISK(v, w, u, n, b, c) \
	(SHR_SAFE ? \
	 (shr_never(), shareii_hook->chowndisk(v, w, u, n, b, c)) : 0)

#define	SHR_EXIT(p) \
	(SHR_SAFE ? (shr_never(), shareii_hook->exit(p)) : (void)0)

#define	SHR_FLUSH(a) \
	(SHR_SAFE ? (shr_never(), shareii_hook->flush(a)) : (void)0)

#define	SHR_UMASK() \
	(SHR_SAFE ? (shr_never(), shareii_hook->umask()) : 0)

#define	SHR_PROCCREATE(p, f) \
	(SHR_SAFE \
	 ? (shr_never(), shareii_hook->proccreate(p, f)) \
	 : ( ((p)->p_shareP = 0), 0))

#define	SHR_PROCDESTROY(p) \
	(SHR_SAFE ? (shr_never(), shareii_hook->procdestroy(p)) : (void)0)

#define	SHR_PCNTUP(t) \
	(SHR_SAFE ? (shr_never(), shareii_hook->pcntup(t)) : (void)0)

#define	SHR_PCNTDN(t) \
	(SHR_SAFE ? (shr_never(), shareii_hook->pcntdn(t)) : (void)0)

#define	SHR_RDWR(v, m, n) \
	(SHR_SAFE ? (shr_never(), shareii_hook->rdwr(v, m, n)) : (void)0)

#define	SHR_BIO(f) \
	(SHR_SAFE ? (shr_never(), shareii_hook->bio(f)) : (void)0)

#define	SHR_SETEUID(u) \
	(SHR_SAFE ? (shr_never(), shareii_hook->seteuid(u)) : (void)0)

#define	SHR_SETRUID(u) \
	(SHR_SAFE ? (shr_never(), shareii_hook->setruid(u)) : (void)0)

#define SHR_SWTCH(t) \
	(SHR_SAFE ? (shr_never(), shareii_hook->swtch(t)) : (void)0)

#define SHR_SYSCALL() \
	(SHR_SAFE ? (shr_never(), shareii_hook->syscall()) : (void)0)

#define SHR_THREADNEW(parent, child) \
	(SHR_SAFE \
	 ? (shr_never(), shareii_hook->threadnew(parent, child)) \
	 : ( ((child)->ut_shareT = 0), 0))

#define SHR_THREADFAIL(p, ut) \
	(SHR_SAFE \
	 ? (shr_never(), shareii_hook->threadfail(p, ut)) \
	 : (void)0)

#define SHR_THREADEXIT(t) \
	(SHR_SAFE ? (shr_never(), shareii_hook->threadexit(t)) : (void)0)

#define	SHR_GET_WEIGHT(tp) \
	(SHR_SAFE ? (shr_never(), shareii_hook->getWeight(tp)) : -1)

#define	SHR_TICK(tp, how, inc) \
	(SHR_SAFE ? (shr_never(), shareii_hook->tick(tp, how, inc)) : (void)0)

/* 
 * note the double underscore!
 * Calls to this hook are generated from
 * SHR_PRIFACE_PRGETPSINFO()
 * where PRIFACE can be empty or irix5
 */
#define	SHR__PRGETPSINFO(p, psp) \
	(SHR_SAFE ? (shr_never(), shareii_hook->prgetpsinfo(p, psp)) : (void)0)

#define	SHR_irix5_PRGETPSINFO(p, psp) \
	 (SHR_SAFE \
	  ? (shr_never(), shareii_hook->irix5_prgetpsinfo(p, psp)) \
	  : (void)0)

#define SHR_SETSCHEDULER(ut, policy, pri) \
	(SHR_SAFE \
	 ? (shr_never(), shareii_hook->setSchedulerClass(ut, policy, pri)) \
	 : pri)

#define SHR_LIMSYS(uap,rvp) \
	(SHR_SAFE ? (shr_never(), shareii_hook->limsys(uap, rvp)) : nopkg())

#define	SHR_ACTIVE	(shareii_hook != NULL)

/*
 * These control the behaviour of limit enforce/update functions:
 *	SHR_LIMITDISK()
 *	SHR_LIMITMEMORY()
 */
#define	LI_ENFORCE	(1<<0)	/* return error if denied & don't update */
#define	LI_UPDATE	(1<<1)	/* update allocation record */
#define	LI_ALLOC	(1<<2)	/* allocation increase */
#define	LI_FREE		(1<<3)	/* allocation decrease */

/*
 * These are for the argument to SHR_BIO():
 */
#define	SH_BIO_ASYNC	(1<<7)	/* not waiting for the i/o to complete */
#define	SH_BIO_HOLD	(1<<6)	/* not releasing the buffer after write */
#define	SH_BIO_BREAD	(1<<0)	/* a read */
#define	SH_BIO_BWRITE	(1<<1)	/* a write */

/*
 * These are for the flag argument to SHR_PROCCREATE():
 */
#define	SH_FORK_ALL	(1<<0)	/* A fork() which duplicates all threads */
#define	SH_FORK_POSIX	(1<<1)	/* A fork1() */

/*
 * This is for the flag argument to SHR_PROCCREATE():
 * Used in combination with SH_FORK_ALL or SH_FORK_POSIX.
 */
#define	SH_FORK_V	(1<<7)	/* A vfork() */

#endif	/* _SHAREII */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SHAREIISTUBS_H */
