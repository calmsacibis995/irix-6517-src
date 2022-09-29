/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.8 $"

#include "synonyms.h"
#include "sys/types.h"
#include "unistd.h"
#include "stdlib.h"
#include "errno.h"
#include "ulocks.h"
#include "malloc.h"
#include "us.h"
#include "mplib.h"
#include "libc_interpose.h"

/*
 * Interposing layer for libc threading. All macros used in libc to
 * thread the various routines need to call routines here. These
 * routines can be preempted by other libraries (like libpthread) that
 * need to do the locking their own way.
 *
 * These routines need to return non-NULL if they wish their unlock
 * routine to be called. Other than that, the return value from a
 * 'lock' routine is guaranteed to be used in the call to the unlock routine.
 * Note that all of these MUST be capable of being called more than once
 * by the same 'thread' (i.e. recursive).
 *
 * In addition to these routines,
 *	flockfile(), funlockfile(), ftrylockfile(), and __fislockfile()
 * should be provided.
 *
 * The '__libc_islock...' functions are for debugging only. They
 * do not get the returned parameter from the __libc_lock.. routine.
 * It is always permissible to return '1'.
 */
extern usema_t *__dirsema;
extern usema_t *__opensema;
extern usema_t *__miscsema;
extern usema_t *__localesema;
extern ulock_t __mmalloclock;
extern ulock_t __pmqlock;
extern ulock_t __randlock;
extern ulock_t __monlock;

enum {
	LIBC_RLOCK_DIR,
	LIBC_RLOCK_OPEN,
	LIBC_RLOCK_MISC,
	LIBC_RLOCK_LOCALE,
	LIBC_RLOCK_MAX
};
enum {
	LIBC_LOCK_MALLOC,
	LIBC_LOCK_PMQ,
	LIBC_LOCK_RAND,
	LIBC_LOCK_MON,
	LIBC_LOCK_MAX
};

#define MTL_RL_L(lcks, li)	\
	if (MTLIB_ACTIVE())	\
		{ return ((void*)(ulong_t) \
			  _mtlib_ctl(MTCTL_RLOCK_LOCK, lcks, li)); }

#define MTL_RL_ISL(lcks, li)	\
	if (MTLIB_ACTIVE())	\
		{ return (_mtlib_ctl(MTCTL_RLOCK_ISLOCKED, lcks, li)); }

#define MTL_RL_UL(lcks, li)	\
	if (MTLIB_ACTIVE())	\
		{ _mtlib_ctl(MTCTL_RLOCK_UNLOCK, lcks, li); return; }

#define MTL_L_OP(lop, li)	\
	if (MTLIB_ACTIVE())	\
		{ return (_mtlib_ctl(lop, misc_locks, li)); }


/* Global function pointer to import multi-threaded functionality to libc.
 */
int (*_mtlib_ctl)(int, ...) _INITBSS;

static int locate_dso(void *, ulong_t *, size_t *);
static ulong_t	libc_base_text;
static ulong_t	libc_end_text;

static void	*misc_rlocks _INITBSS;		/* semas */
static void	*misc_locks _INITBSS;		/* locks */
void		*__mtlib_io_locks _INITBSS;	/* semas */

int
__new_libc_threadinit(int type, int (*ctl)(int, ...))
{
	int	maxfds = getdtablesize();
	size_t	sz;

	if (maxfds < 100) {
		maxfds = 100;
	}
	_mtlib_ctl = ctl;
	if (_mtlib_ctl(MTCTL_RLOCK_ALLOC, LIBC_RLOCK_MAX, &misc_rlocks)
	    || _mtlib_ctl(MTCTL_LOCK_ALLOC, LIBC_LOCK_MAX, &misc_locks)
	    || _mtlib_ctl(MTCTL_RLOCK_ALLOC, maxfds+1, &__mtlib_io_locks)) {
		return (-1);
	}

	libc_threadinit();

	__multi_thread = type;

	/* Locate libc for cancellation */
	if (locate_dso((void *)__new_libc_threadinit, &libc_base_text, &sz)) {
		return (-1);
	}
	libc_end_text = libc_base_text + sz;

	return (0);
}


#pragma weak __libc_lockmisc = ___libc_lockmisc
#pragma weak __libc_islockmisc = ___libc_islockmisc
#pragma weak __libc_unlockmisc = ___libc_unlockmisc
void *
___libc_lockmisc(void)
{
	MTL_RL_L(misc_rlocks, LIBC_RLOCK_MISC);
	if (__us_rsthread_misc && (uspsema(__miscsema) == 1))
		return (void *)1L;
	return NULL;
}

int
___libc_islockmisc(void)
{
	MTL_RL_ISL(misc_rlocks, LIBC_RLOCK_MISC);
	if (__us_rsthread_misc)
		return ustestsema(__miscsema) <= 0;
	return 1; /* sure */
}

void
___libc_unlockmisc(void *v)
{
	MTL_RL_UL(misc_rlocks, LIBC_RLOCK_MISC);
	if (v)
		(void)usvsema(__miscsema);
}

#pragma weak __libc_lockdir = ___libc_lockdir
#pragma weak __libc_islockdir = ___libc_islockdir
#pragma weak __libc_unlockdir = ___libc_unlockdir
void *
___libc_lockdir(void)
{
	MTL_RL_L(misc_rlocks, LIBC_RLOCK_DIR);
	if (__us_rsthread_misc && (uspsema(__dirsema) == 1))
		return (void *)1L;
	return NULL;
}

int
___libc_islockdir(void)
{
	MTL_RL_ISL(misc_rlocks, LIBC_RLOCK_DIR);
	if (__us_rsthread_misc)
		return ustestsema(__dirsema) <= 0;
	return 1;
}

void
___libc_unlockdir(void *v)
{
	MTL_RL_UL(misc_rlocks, LIBC_RLOCK_DIR);
	if (v)
		(void)usvsema(__dirsema);
}

#pragma weak __libc_lockopen = ___libc_lockopen
#pragma weak __libc_islockopen = ___libc_islockopen
#pragma weak __libc_unlockopen = ___libc_unlockopen
void *
___libc_lockopen(void)
{
	MTL_RL_L(misc_rlocks, LIBC_RLOCK_OPEN);
	if (__us_rsthread_stdio && (uspsema(__opensema) == 1))
		return (void *)1L;
	return NULL;
}

int
___libc_islockopen(void)
{
	MTL_RL_ISL(misc_rlocks, LIBC_RLOCK_OPEN);
	if (__us_rsthread_stdio)
		return ustestsema(__opensema) <= 0;
	return 1;
}

void
___libc_unlockopen(void *v)
{
	MTL_RL_UL(misc_rlocks, LIBC_RLOCK_OPEN);
	if (v)
		(void)usvsema(__opensema);
}

#pragma weak __libc_locklocale = ___libc_locklocale
#pragma weak __libc_islocklocale = ___libc_islocklocale
#pragma weak __libc_unlocklocale = ___libc_unlocklocale
void *
___libc_locklocale(void)
{
	MTL_RL_L(misc_rlocks, LIBC_RLOCK_LOCALE);
	if (__us_rsthread_misc && (uspsema(__localesema) == 1))
		return (void *)1L;
	return NULL;
}

int
___libc_islocklocale(void)
{
	MTL_RL_ISL(misc_rlocks, LIBC_RLOCK_LOCALE);
	if (__us_rsthread_misc)
		return ustestsema(__localesema) <= 0;
	return 1;
}

void
___libc_unlocklocale(void *v)
{
	MTL_RL_UL(misc_rlocks, LIBC_RLOCK_LOCALE);
	if (v)
		(void)usvsema(__localesema);
}

#pragma weak __libc_lockfile = ___libc_lockfile
#pragma weak __libc_islockfile = ___libc_islockfile
#pragma weak __libc_unlockfile = ___libc_unlockfile
void *
___libc_lockfile(FILE *f)
{
	MTL_RL_L(__mtlib_io_locks, f->_file);
	if (__us_rsthread_stdio) {
		flockfile(f);
		return (void *)1L;
	}
	return NULL;
}

int
___libc_islockfile(FILE *f)
{
	MTL_RL_ISL(__mtlib_io_locks, f->_file);
	if (__us_rsthread_stdio)
		return __fislockfile(f);
	return 1; /* sure */
}

void
___libc_unlockfile(FILE *f, void *v)
{
	MTL_RL_UL(__mtlib_io_locks, f->_file);
	if (v)
		(void)funlockfile(f);
}

/*
 * Malloc interface returns an int to make macro in <malloc.h> work.
 * This is usually implemented with a spin lock rather than a sleeping
 * mutex.
 */
#pragma weak __libc_lockmalloc = ___libc_lockmalloc
#pragma weak __libc_unlockmalloc = ___libc_unlockmalloc
int
___libc_lockmalloc(void)
{
	MTL_L_OP(MTCTL_LOCK_LOCK, LIBC_LOCK_MALLOC);
	if (__us_rsthread_malloc)
		ussetlock(__mmalloclock);
	return 1;
}

int
___libc_unlockmalloc(void)
{
	MTL_L_OP(MTCTL_LOCK_UNLOCK, LIBC_LOCK_MALLOC);
	if (__us_rsthread_malloc)
		usunsetlock(__mmalloclock);
	return 1;
}

/*
 * lock for posix message queue structures
 */
#pragma weak __libc_lockpmq = ___libc_lockpmq
#pragma weak __libc_unlockpmq = ___libc_unlockpmq
int
___libc_lockpmq(void)
{
	MTL_L_OP(MTCTL_LOCK_LOCK, LIBC_LOCK_PMQ);
	if (__us_rsthread_pmq)
		ussetlock(__pmqlock);
	return 1;
}

int
___libc_unlockpmq(void)
{
	MTL_L_OP(MTCTL_LOCK_UNLOCK, LIBC_LOCK_PMQ);
	if (__us_rsthread_pmq)
		usunsetlock(__pmqlock);
	return 1;
}

/*
 * the rand() interface is a bit different - nothing is returned.
 * This is usually implemented with a spin lock rather than a sleeping
 * mutex.
 */
#pragma weak __libc_lockrand = ___libc_lockrand
#pragma weak __libc_unlockrand = ___libc_unlockrand
void
___libc_lockrand(void)
{
	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_LOCK_LOCK, misc_locks, LIBC_LOCK_RAND);
		return;
	}
	if (__us_rsthread_misc)
		ussetlock(__randlock);
}

void
___libc_unlockrand(void)
{
	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_LOCK_UNLOCK, misc_locks, LIBC_LOCK_RAND);
		return;
	}
	if (__us_rsthread_misc)
		usunsetlock(__randlock);
}

#pragma weak _monlock = ___libc_lockmon
#pragma weak _monunlock = ___libc_unlockmon
void
___libc_lockmon(void)
{
	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_LOCK_LOCK, misc_locks, LIBC_LOCK_MON);
		return;
	}
	if (__monlock)
		ussetlock(__monlock); 
}

void
___libc_unlockmon(void)
{
	if (MTLIB_ACTIVE()) {
		_mtlib_ctl(MTCTL_LOCK_UNLOCK, misc_locks, LIBC_LOCK_MON);
		return;
	}
	if (__monlock)
		usunsetlock(__monlock);
}

#pragma weak __libc_threadbind = ___libc_threadbind
#pragma weak __libc_threadunbind = ___libc_threadunbind
int
___libc_threadbind(void)
{
	MTLIB_RETURN( (MTCTL_BIND_HACK) );
	return (0);
}

int
___libc_threadunbind(void)
{
	MTLIB_RETURN( (MTCTL_UNBIND_HACK) );
	return (0);
}


/* DSO lookup code for cancellation
 *
 * Cancellation is peculiar in that cancellation points only need to
 * occur when explicitly called.  Indeed they are defined to _not_
 * occur in POSIX interfaces unless explicitly defined.  Furthermore
 * since they can leave data in an inconsistent state if a thread
 * is unexpectedly cancelled they need to be carefully handled.
 *
 * This code permits us to distinguish between a call from outside
 * of libc and one from inside.
 */

/* _mtlib_libc_cncl()
 *
 * False if addr lies within libc - not cancellation point.
 * True if addr outside libc - act as cancellation point.
 */
int
_mtlib_libc_cncl(void *addr)
{
	return ((ulong_t)addr > libc_end_text
		|| (ulong_t)addr < libc_base_text);
}


/* locate_dso()
 *
 * Find the base address and size of the text region containing
 * a specified text address in the current DSO.
 * Works for all ABIs and also with pixified code.
 */

#include <sys/types.h>
#include <elf.h>


#if (_MIPS_SIM == _ABI64)
#define ELF_EHDR	Elf64_Ehdr
#define ELF_PHDR	Elf64_Phdr
#else
#define ELF_EHDR	Elf32_Ehdr
#define ELF_PHDR	Elf32_Phdr
#endif

static int
locate_dso(void *vaddr, ulong_t *base, size_t *size)
{
	ulong_t		addr = (ulong_t)vaddr;
	int		h;

	extern int	__elf_header[];
	ELF_EHDR	*eh = (ELF_EHDR *)__elf_header;

	extern		int __program_header_table[];
	ELF_PHDR	*ph = (ELF_PHDR *)__program_header_table;

	ulong_t		raddr;
	extern int	__dso_displacement[];

	/* Walk through this DSOs program header table
	 */
	for (h = 0; h < eh->e_phnum; h++, ph++) {

		/* Look for a loadable read-only entry
		 */
		if (ph->p_type == PT_LOAD
		    && (ph->p_flags & (PF_R|PF_W)) == PF_R) {

			/* Compute the loaded address
			 */
			raddr = ph->p_vaddr + (ulong_t)__dso_displacement;

			/* Discover whether our target lies in this region
			 */
			if (addr > raddr && addr < raddr + ph->p_filesz) {
				*base = raddr;
				*size = ph->p_filesz;
				return (0);
			}
		}
	}

	return (ESRCH);
}
