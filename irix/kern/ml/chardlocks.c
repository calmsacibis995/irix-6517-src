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

#ident "$Revision: 1.48 $"

#if MP

/*
 * Non-debug hardware locks
 * If ASMVERSION defined, then the low-level stuff is in assembly language
 */
#define ASMVERSION	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/splock.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#ifndef BITSPERBYTE
# define BITSPERBYTE	8
#endif

#define LOCK_TAKEN 1 /* Must agree with lock_busy code */
extern int lock_busy(lock_t);

#define free_lock(lck) *((lock_t)((long)(lck) & ~0x3)) &= ~LOCK_TAKEN

/*
 * splock initialization -- called very early
 *
 * Note that both ASSERT (_assfail) and cmn_err use spinlocks.
 * If any error condition occurs here, it will appear as though
 * we've bombed in spsema() since, of course, spinlocks aren't
 * useable until we've finished this routine.
 */
pfn_t
initsplocks(pfn_t fpage)
{
	return(fpage);
}

/* ARGSUSED */
void
meter_spinlock(register lock_t *lp, char *name)
{
	return;
}

/* ARGSUSED */
void
init_spinlock(register lock_t *lp, char *name, long sequencer)
{
	*lp = SPIN_INIT;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, 1);
	atomicAddInt(&syncinfo.spinlock.initialized, 1);
#endif
}

/* ARGSUSED */
void
spinlock_init(register lock_t *lp, char *name)
{
	*lp = SPIN_INIT;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, 1);
	atomicAddInt(&syncinfo.spinlock.initialized, 1);
#endif
}

/* ARGSUSED */
void
spinlock_destroy(register lock_t *lp)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, -1);
#endif
}

int
spinlock_initialized(register lock_t *sp)
{
	return *sp & SPIN_INIT;
}

int
spinlock_islocked(register lock_t *sp)
{
	return *sp & SPIN_LOCK;
}

/* ARGSUSED */
lock_t *
spinlock_alloc(register int vmflags, char *name)
{
	return (lock_t *)kmem_zalloc(sizeof(lock_t), vmflags);
}

void
spinlock_dealloc(register lock_t *lp)
{
	kmem_free(lp, sizeof(lock_t));
}

/* ARGSUSED */
void
meter_bitlock(uint *lp, char *name)
{
	return;
}

/* ARGSUSED */
void
init_bitlock(uint *lp, uint bit, char *name, long sequencer)
{
	*lp &= ~bit;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock.current, 1);
	atomicAddInt(&syncinfo.bitlock.initialized, 1);
#endif
}

/* ARGSUSED */
void
destroy_bitlock(register uint *lp)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock.current, -1);
#endif
}

int
bitlock_islocked(register uint_t *lock, register uint_t bit)
{
	return (*lock & bit) && issplhi(getsr());
}

int
bitlock64bit_islocked(register __uint64_t *lock, register uint_t bit)
{
	return (*lock & bit) && issplhi(getsr());
}

/* ARGSUSED */
void
meter_64bitlock(__uint64_t *lp, char *name)
{
	return;
}

/* ARGSUSED */
void
init_64bitlock(
	register __uint64_t *lp,
	__uint64_t bit,
	char *name,
	long sequencer)
{
	*lp &= ~bit;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock64.current, 1);
	atomicAddInt(&syncinfo.bitlock64.initialized, 1);
#endif
}

/* ARGSUSED */
void
destroy_64bitlock(register __uint64_t *lp)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock64.current, -1);
#endif
}

void
mutex_io_spinunlock(lock_t *lck, int ospl)
{
	flushbus();
	mutex_spinunlock(lck, ospl);
}

struct k_splockmeter *lockmeter_chain;	/* head of in-use splockmeters chain */

/* ARGSUSED */
k_splockmeter_t *
lockmeter_find(void *addr)
{
	return ((k_splockmeter_t *)NULL);
}

#endif /* MP */
