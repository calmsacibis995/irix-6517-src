/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_ATOMIC_OPS_H__
#define __SYS_ATOMIC_OPS_H__

#ident	"$Revision: 1.43 $"

#if _KERNEL && !_STANDALONE && !LANGUAGE_ASSEMBLY

/*
 * MIPS atomic operations based on link-load, store-conditional instructions.
 */

struct kthread;

#if  defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700) && !defined(IP28)
/*
 * Use compiler atomic operation intrinsics for the MipsPRO 7.0 and later
 * compilers.
 *
 * We don't enable the compiler intrinsics for atomic operations on the IP28
 * (Indigo2/R10K) because of a bug in the compiler: #763380,
 * ``t5_no_spec_stores not working for atomic intrinsics.''  When this bug is
 * fixed we can reenable the intrinsics for IP28.
 */

/*
 * Add, set or clear bits in a (4-byte) int.
 */
#define atomicAddInt(a, b)	__add_and_fetch(a, b)
#define atomicSetInt(a, b)	__fetch_and_or(a, b)
#define atomicClearInt(a, b)	__fetch_and_and(a, ~(b))

#define atomicAddUint(a, b)	__add_and_fetch(a, b)
#define atomicSetUint(a, b)	__fetch_and_or(a, b)
#define atomicClearUint(a, b)	__fetch_and_and(a, ~(b))

/*
 * Add, set or clear bits in a long -- works for native size of long.
 */
#define atomicAddLong(a, b)	__add_and_fetch(a, b)
#define atomicSetLong(a, b)	__fetch_and_or(a, b)
#define atomicClearLong(a, b)	__fetch_and_and(a, ~(b))

__inline uint atomicFieldAssignUint(uint *l, uint f, uint b) 
{
	uint tmp;
	do {
		tmp = *l;
	} while (!__compare_and_swap((l), (tmp),
				(((tmp) & (~((uint)f))) | (b))));
	return tmp;
} 

__inline long atomicFieldAssignLong(long *l,long f,long b) 
 {
	long tmp;
	do {
		tmp = *l;
	} while (!__compare_and_swap((l), (tmp),
				(((tmp) & (~((__uint64_t)f))) | (b))));
	return tmp;
} 

#define atomicAddUlong(a, b)	__add_and_fetch(a, b)
#define atomicSetUlong(a, b)	__fetch_and_or(a, b)
#define atomicClearUlong(a, b)	__fetch_and_and(a, ~(b))

/*
 * Add, set or clear bits in a 64 bit int
 */
#define atomicAddInt64(a, b)	__add_and_fetch(a, b)
#define atomicAddUint64(a, b)	__add_and_fetch(a, b)
#define atomicSetInt64(a, b)	__fetch_and_or(a, b)
#define atomicSetUint64(a, b)	__fetch_and_or(a, b)
#define atomicClearInt64(a, b)	__fetch_and_and(a, ~(b))
#define atomicClearUint64(a, b)	__fetch_and_and(a, ~(b))

#define bitlock_release(a, b)	__fetch_and_and(a, ~(b))
#define bitlock_release_32bit(a, b)	__fetch_and_and(a, ~(b))

#else /* ucode or ragnarok compilers or IP28 */

/*
 * Add, set or clear bits in a (4-byte) int.
 */
extern int atomicAddInt(volatile int *, int);
extern int atomicSetInt(volatile int *, int);
extern int atomicClearInt(volatile int *, int);

extern uint atomicAddUint(volatile uint *, uint);
extern uint atomicSetUint(volatile uint *, uint);
extern uint atomicClearUint(volatile uint *, uint);

/*
 * Add, set or clear bits in a long -- works for native size of long.
 */
extern long atomicAddLong(volatile long *, long);
extern long atomicSetLong(volatile long *, long);
extern long atomicClearLong(volatile long *, long);
extern long atomicFieldAssignLong(volatile long *, long, long);
extern int  atomicFieldAssignUint(volatile uint *, uint, uint);

extern unsigned long atomicAddUlong(volatile unsigned long *, unsigned long);
extern unsigned long atomicSetUlong(volatile unsigned long *, unsigned long);
extern unsigned long atomicClearUlong(volatile unsigned long *, unsigned long);

/*
 * Add, set or clear bits in a 64 bit int
 */
extern int64_t	atomicAddInt64(volatile int64_t *, int64_t);
extern uint64_t	atomicAddUint64(volatile uint64_t *, uint64_t);
extern int64_t	atomicSetInt64(volatile int64_t *, int64_t);
extern uint64_t	atomicSetUint64(volatile uint64_t *, uint64_t);
extern int64_t	atomicClearInt64(volatile int64_t *, int64_t);
extern uint64_t	atomicClearUint64(volatile uint64_t *, uint64_t);

extern uint64_t bitlock_release(volatile uint64_t *, uint64_t);
extern uint32_t bitlock_release_32bit(volatile uint32_t *, uint32_t);

#endif /* ucode or ragnarok compilers or IP28 */


/*
 * increment an (four-byte) int. Wrap back to 0 when we match second argument
 */
extern int atomicIncWithWrap(int *, int);
extern int atomicAddWithWrap(int *, int, int);

/* PCB anon port manipulation */
extern int atomicIncPort(int *, int, int);

/*
 * Set or clear bits in a cpumask
 *
 * Note: These functions have always returned incorrect values on platforms
 * that support more than 64 cpus.  Since no kernel code uses the return
 * value, the safest way to fix this problem is to change the function
 * prototype to "void".
 * 	The problem is related to: 
 *		- the register conventions for returning values 
 *		  greater than 128 bits
 *		- the functions return the wrong value for systems that 
 *		  support > 64 cpus
 */
extern void atomicSetCpumask(cpumask_t *, cpumask_t *);
extern void atomicClearCpumask(cpumask_t *, cpumask_t *);

/*
 * Atomic assembler routines used by heap allocators.
 */
extern void *atomicPush(void *, void *, void *);
extern void *atomicPull(void *);

/*
 * Exchange values in memory.  test_and_set is unconditional.
 * compare_and_swap is conditional, returning 1 and swapping if the
 * memory value equals "old".
 */

extern int	test_and_set_int(int *loc, int new);
extern long	test_and_set_long(long *loc, long new);
extern void *	test_and_set_ptr(void **loc, void *new);
extern int	compare_and_swap_int(int *loc, int old, int new);
extern int	compare_and_swap_long(long *loc, long old, long new);
extern int	compare_and_swap_ptr(void **loc, void *old, void *new);
extern int	compare_and_swap_int64(__int64_t *loc, __int64_t old, __uint64_t new);
extern int	comparegt_and_swap_int(int *loc, int compare_val, int new);
extern int	compare_and_inc_int_gt_zero(int *loc);
extern int	compare_and_dec_int_gt(int *loc, int compare_val);
int	compare_and_swap_kt(struct kthread **, struct kthread *,
					struct kthread *);
#if MP
#define compare_and_dec_int_gt_hot(cp,v) compare_and_dec_int_gt_hot2(&(cp)->value, v, &(cp)->pref)
extern int	compare_and_dec_int_gt_hot2(volatile int *loc, int compare_val, char *pref);
#else /* MP */
#define compare_and_dec_int_gt_hot(cp,v) compare_and_dec_int_gt((int*)&(cp)->value, v)
#endif /* MP */


extern int	swap_int(int *loc, int new);
extern long	swap_long(long *loc, long new);
extern void *	swap_ptr(void **loc, void *new);
extern __uint64_t swap_int64(__int64_t *loc, __uint64_t new);

/*
 * Primitives to lock the pfn field in ptes
 */

extern long bitlock_acquire(long *, long);
extern long bitlock_condacq(long *, long);

		

/*
 * Primitives to lock a 32-bit work by a bit field inside the word
 */
extern void bitlock_acquire_32bit(__uint32_t *word, __uint32_t locking_bit);
extern long bitlock_condacq_32bit(__uint32_t *, __uint32_t);


/*
 * Primitives for atomic add on hot cachelines. 
 *
 * Sometimes it's either impossible to avoid global counters, etc. or simply
 * too hard for various schedule and resource reasons to do a distributed
 * rewrite of a piece of code.  This is bad because if the counter is
 * frequently updated from lots of CPUs we'll take an enormous performance hit
 * because of cache coherency overhead.  Worse yet, if the counter is updated
 * in an atomic manner with LL/SC instructions, the ``throughput'' rate on
 * counter updates can go to near zero as the CPUs fight for control of the
 * cache line containing the counter.  One relatively cheap hack that helps in
 * the MP cache contention case and doesn't hurt SP updates appreciably is to
 * do a store into another word in the same cache line as the counter before
 * each LL/SC loop.  This works because if forces the cache line to be in the
 * local CPU's cache in Dirty Exclusive mode, letting the cache coherency
 * hardware figure out how to arbitrate this.  Then, we have a short time
 * window in which we can do a LL/SC with a high probability of success
 * because our CPU ``owns'' the cache line.
 *
 * We encode this idea via several ``hot counter'' variants of the standard
 * atomic counter routines.  The lowest level routines take all of the same
 * arguments that the standard atomic counter routines do plus an additional
 * pointer to a ``junk'' word that will be stored to just before the LL in the
 * LL/SC loop.  We also provide a convenience data type for hot counters and
 * some inline routines that take pointers to this data type and call the low
 * level routines.  
 *
 * Most of these ideas are further qualified by only being implemented on MP
 * platforms since SP platforms don't have any problems and we can save space
 * and the small amount of extra time there ...
 *
 * The counter (int, uint, etc) is embedded in a structure that contains
 * a byte to use in the LL/SC loop to cause a prefetch-exclusive 
 * of the cacheline. This avoids extra coherency traffic & substantially
 * improves performance of updating highly contended cache lines.
 *
 * NOTE: use the structure definitions for counter when possible. The actual
 * assembly language primitives take 3 arguments; the increment value,
 * the pointer to the counter & the pointer to the char to use for 
 * the prefetch. This allows the byte that is used for the prefetch to
 * be allocated separated if needed.
 */

typedef struct {
	volatile int	value;
#if MP
	char		pref;
#endif
} hotIntCounter_t;

typedef struct {
	volatile uint	value;
#if MP
	char		pref;
#endif
} hotUintCounter_t;

typedef struct {
	volatile int64_t value;
#if MP
	char		pref;
#endif
} hotInt64Counter_t;

typedef struct {
	volatile uint64_t value;
#if MP
	char		pref;
#endif
} hotUint64Counter_t;

#pragma set type attribute hotIntCounter_t align=8
#pragma set type attribute hotUintCounter_t align=8
#pragma set type attribute hotInt64Counter_t align=16
#pragma set type attribute hotUint64Counter_t align=16

#define fetchIntHot(c)		((c)->value)
#define fetchUintHot(c)		((c)->value)
#define fetchInt64Hot(c)	((c)->value)
#define fetchUint64Hot(c)	((c)->value)

#define setIntHot(c,v)		(c)->value = (v)
#define setUintHot(c,v)		(c)->value = (v)
#define setInt64Hot(c,v)	(c)->value = (v)
#define setUint64Hot(c,v)	(c)->value = (v)

#define atomicAddIntHot(c,n)	atomicAddIntHot2(&(c)->value, n, &(c)->pref)
#define atomicAddUintHot(c,n)	atomicAddUintHot2(&(c)->value, n, &(c)->pref)
#define atomicAddInt64Hot(c,n)	atomicAddInt64Hot2(&(c)->value, n, &(c)->pref)
#define atomicAddUint64Hot(c,n)	atomicAddUint64Hot2(&(c)->value, n, &(c)->pref)



#if MP
extern int		atomicAddIntHot2(volatile int *, int, char *);
extern uint		atomicAddUintHot2(volatile uint *, uint, char *);
extern int64_t		atomicAddInt64Hot2(volatile int64_t *, int64_t, char *);
extern uint64_t		atomicAddUint64Hot2(volatile uint64_t *, uint64_t, char *);

#else /* !MP */

/*
 * A small bug in the 7.2.1.3 compiler suite (#767819) causes cpp tokens to be
 * re-replaced in the original replacement list if a nested macro resolves to
 * one of the compiler intrinsics.  The hack below is designed to get around
 * this till the bug is fixed.
 */
#if  defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700) && !defined(IP28)
#define atomicAddIntHot2(cp,val,pp)	__add_and_fetch(cp,val)
#define atomicAddUintHot2(cp,val,pp)	__add_and_fetch(cp,val)
#define atomicAddInt64Hot2(cp,val,pp)	__add_and_fetch(cp,val)
#define atomicAddUint64Hot2(cp,val,pp)	__add_and_fetch(cp,val)
#else
#define atomicAddIntHot2(cp,val,pp)	atomicAddInt(cp, val)
#define atomicAddUintHot2(cp,val,pp)	atomicAddUint(cp, val)
#define atomicAddInt64Hot2(cp,val,pp)	atomicAddInt64(cp, val)
#define atomicAddUint64Hot2(cp,val,pp)	atomicAddUint64(cp,val)
#endif

#endif /* !MP */

#if (_MIPS_SZLONG == 32)

typedef hotIntCounter_t		hotLongCounter_t;
typedef hotUintCounter_t	hotUlongCounter_t;

#define fetchLongHot(c)		fetchIntHot(c)
#define fetchUlongHot(c)	fetchUintHot(c)

#define setLongHot(c,v)		setIntHot(c,v)
#define setUlongHot(c,v)	setUintHot(c,v)

#define atomicAddLongHot(c,n)	atomicAddIntHot(c,n)
#define atomicAddUlongHot(c,n)	atomicAddUintHot(c,n)

#else /* _MIPS_SZLONG == 64 */

typedef hotInt64Counter_t	hotLongCounter_t;
typedef hotUint64Counter_t	hotUlongCounter_t;

#define fetchLongHot(c)		fetchInt64Hot(c)
#define fetchUlongHot(c)	fetchUint64Hot(c)

#define setLongHot(c,v)		setInt64Hot(c,v)
#define setUlongHot(c,v)	setUint64Hot(c,v)

#define atomicAddLongHot(c,n)	atomicAddInt64Hot(c,n)
#define atomicAddUlongHot(c,n)	atomicAddUint64Hot(c,n)

#endif /* _MIPS_SZLONG == 64 */

#endif /* _KERNEL && !_STANDALONE && !LANGUAGE_ASSEMBLY */

#endif /* !__SYS_ATOMIC_OPS_H__ */
