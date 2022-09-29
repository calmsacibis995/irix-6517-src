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

#ident "$Revision: 3.42 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/debug.h>
#include <sys/runq.h>
#include <sys/proc.h>
#include <sys/par.h>

struct k_splockmeter *lockmeter_chain;	/* head of in-use splockmeters chain */

/* ARGSUSED */
k_splockmeter_t *
lockmeter_find(void *addr)
{
	return ((k_splockmeter_t *)NULL);
}

/*
 * Stubs for uniprocessor architectures that do not require spinlocks
 */

pfn_t
initsplocks(pfn_t fpage)
{
	return(fpage);
}

/* ARGSUSED */
void
meter_spinlock(lock_t *lp, char *name)
{
	return;
}

/* ARGSUSED */
void
init_spinlock(lock_t *lp, char *name, long sequence)
{
	*lp = SPIN_INIT;
}

/* ARGSUSED */
void
spinlock_init(lock_t *lp, char *name)
{
	*lp = SPIN_INIT;
}

/* ARGSUSED */
void
spinlock_destroy(lock_t *mp)
{
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
init_bitlock(uint *lp, uint bit, char *name, long sequence)
{
	*lp &= ~bit;
}

/* ARGSUSED */
void
destroy_bitlock(uint *lp)
{
}

/* ARGSUSED */
void
meter_64bitlock(__uint64_t *lp, char *name)
{
	return;
}

/* ARGSUSED */
void
init_64bitlock(__uint64_t *lp, __uint64_t bit, char *name, long sequence)
{
	*lp &= ~bit;
}

/* ARGSUSED */
void
destroy_64bitlock(__uint64_t *lp)
{
}
