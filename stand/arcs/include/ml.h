#ident "$Revision: 1.10 $"

#ifndef __ML_H__
#define __ML_H__ 1

#include "sys/asm.h"
#include "sys/regdef.h"

#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
#define lreg	ld
#define sreg	sd
#define BPREG	8
#else
#define lreg	lw
#define sreg	sw
#define BPREG	4
#endif

#if R4000 && (defined(IP22) || defined(IP19))
/*
 * Some 250MHz R4K modules will flip bits in the entrylo0 and
 * entrylo1 registers when written, when there's a certain bit
 * pattern in the register already, the workaround is to make
 * sure a zero exists before the value is written.
 */
#define	TLBLO_FIX_250MHz(entrylo)	mtc0	zero, entrylo
#else
#define	TLBLO_FIX_250MHz(entrylo)
#endif	/* R4000 && (IP22 || IP19) */

#endif

#ifdef IP28		/* see irix/kern/ml/ml.h for the gory details */
#define	CACHE_BARRIER_AT(o,r) \
	cache CACH_BARRIER,o(r)
#define	ORD_CACHE_BARRIER_AT(o,r) \
	.set noreorder; CACHE_BARRIER_AT(o,r); .set reorder
#define	CACHE_BARRIER_SP \
	CACHE_BARRIER_AT(0,sp)
#define	CACHE_BARRIER CACHE_BARRIER_SP
#define	AUTO_CACHE_BARRIERS_DISABLE	.set no_spec_mem
#define	AUTO_CACHE_BARRIERS_ENABLE	.set spec_mem
#else
#define	CACHE_BARRIER_AT(o,r)
#define	ORD_CACHE_BARRIER_AT(o,r)
#define	CACHE_BARRIER_SP
#define	CACHE_BARRIER
#define	AUTO_CACHE_BARRIERS_DISABLE
#define	AUTO_CACHE_BARRIERS_ENABLE
#endif /* IP28 */
