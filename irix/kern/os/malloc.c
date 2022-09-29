/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.88 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/pda.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/signal.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/uthread.h>
#include <sys/var.h>

#ifdef _VCE_AVOIDANCE
extern uint cachecolorsize;
extern uint cachecolormask;

#ifdef CACHECOLORSIZE
#undef CACHECOLORSIZE
#define CACHECOLORSIZE cachecolorsize
#endif
#ifdef CACHECOLORMASK
#undef CACHECOLORMASK
#define CACHECOLORMASK cachecolormask
#endif

#endif /* _VCE_AVOIDANCE */

/*
 * allocate and initialize a map structure
 */
struct map *
rmallocmap(ulong_t mapsiz)
{
	struct map *mp;
	ulong_t size;
	struct a {
		lock_t	lock;
		sv_t 	sema;
	} *sync;

	if (mapsiz == 0)
		return(NULL);
	size = sizeof(struct map) * (mapsiz + 2);
	if ((mp = kmem_zalloc(size, KM_NOSLEEP)) == NULL)
		return(NULL);
	sync = kmem_zalloc(sizeof(struct a), KM_NOSLEEP);
	if (sync == NULL) {
		kmem_free(mp, size);
		return(NULL);
	}
	spinlock_init(&sync->lock, "sync->lock");
	sv_init(&sync->sema, SV_DEFAULT, "sync->sema");
	mp[1].m_size = (unsigned long) &sync->lock;
	mp[1].m_addr = (unsigned long) &sync->sema;
	mapsize(mp) = mapsiz - 1;
	return(mp);
}

/*
 * free a map structure previously allocated via rmallocmap().
 */
void
rmfreemap(struct map *mp)
{
	struct a {
		lock_t	lock;
		sv_t 	sema;
	};
	ASSERT(sv_waitq(mapout(mp)) == 0);
	sv_destroy(mapout(mp));
	spinlock_destroy(maplock(mp));
	kmem_free((void *)mp[1].m_size, sizeof(struct a));
	kmem_free(mp, (sizeof(struct map) * (mapsize(mp)+3)));
}

/*
 * Allocate 'size' units from the given map.
 * Return the base of the allocated space.
 * In a map, the addresses are increasing and the
 * list is terminated by a 0 size.
 * Algorithm is first-fit.
 */

static 
unsigned int
mapalloc(
	struct map *mp,
	int size,
	int flags)
{
	register unsigned int a;
	register struct map *bp;
	register int s;

	ASSERT(size >= 0);

	if (size == 0)
		return(NULL);

	for (;;) {
		s = mutex_spinlock(maplock(mp));

		for (bp = mapstart(mp); bp->m_size; bp++) {
			if (bp->m_size >= size) {
				a = bp->m_addr;
				bp->m_addr += size;
				if ((bp->m_size -= size) == 0) {
					do {
						bp++;
						(bp-1)->m_addr = bp->m_addr;
					} while ((bp-1)->m_size = bp->m_size);
					mapsize(mp)++;
				}

				ASSERT(bp->m_size < 0x80000000);
				mutex_spinunlock(maplock(mp), s);
				return(a);
			}
		}
		if (flags & VM_NOSLEEP) {
			mutex_spinunlock(maplock(mp), s);
			return (0);
		}
		sv_wait(mapout(mp), PMEM, maplock(mp), s);
	}
	/*NOTREACHED*/
	return(0);
}

unsigned int
malloc(struct map *mp, int size)
{
	return(mapalloc(mp, size, VM_NOSLEEP));
}

unsigned int
malloc_wait(struct map *mp, int size)
{
	return(mapalloc(mp, size, 0));
}

ulong_t
rmalloc(struct map *mp, size_t size)
{
	return((ulong_t)mapalloc(mp, size, VM_NOSLEEP));
}

/* 
 * same as rmalloc(), but wait for space if none is available.
 */
ulong_t
rmalloc_wait(struct map *mp, size_t size)
{
	return((ulong_t)mapalloc(mp, size, 0));
}

/*
 * Free the previously allocated space a of size units into the specified map.
 * Sort ``a'' into map and combine on one or both ends if possible.
 * Returns 0 on success, 1 on failure.
 */
void
mfree(struct map *mp, int size, unsigned int a)
{
	register struct map *bp;
	register unsigned int t;
	register int s;

	ASSERT(size >= 0);

	if (size == 0)
		return;

	bp = mapstart(mp);
	s = mutex_spinlock(maplock(mp));

	for ( ; bp->m_addr<=a && bp->m_size!=0; bp++)
		;
	if (bp>mapstart(mp) && (bp-1)->m_addr+(bp-1)->m_size == a) {
		(bp-1)->m_size += size;
		if (bp->m_addr) {	
			/* m_addr==0 end of map table */
			ASSERT(a+size <= bp->m_addr);
			if (a+size == bp->m_addr) { 

				/* compress adjacent map addr entries */
				(bp-1)->m_size += bp->m_size;
				while (bp->m_size) {
					bp++;
					(bp-1)->m_addr = bp->m_addr;
					(bp-1)->m_size = bp->m_size;
				}
				mapsize(mp)++;
			}
		}
	} else {
		if (a+size == bp->m_addr && bp->m_size) {
			bp->m_addr -= size;
			bp->m_size += size;
		} else {
			ASSERT(size);
			if (mapsize(mp) == 0) {
				mutex_spinunlock(maplock(mp), s);
				cmn_err_tag(286,CE_WARN,"mfree map overflow %x.\
	Lost 0x%x items at 0x%x", mp, size, a) ;
				return ;
			}
			do {
				t = bp->m_addr;
				bp->m_addr = a;
				a = t;
				t = bp->m_size;
				bp->m_size = size;
				bp++;
			} while (size = t);
			mapsize(mp)--;
		}
	}
	mutex_spinunlock(maplock(mp), s);
	/*
	 * wake up everyone waiting for space
	 */
	if (mapout(mp))
		sv_broadcast(mapout(mp));
}

void
rmfree(struct map *mp, size_t size, ulong_t a)
{
	mfree(mp, size, a);
}

#if R4000

#ifdef BITSPERWORD
#undef BITSPERWORD
#endif
#ifdef WORDMASK
#undef WORDMASK
#endif
#ifdef BITSTOWORDS
#undef BITSTOWORDS
#endif

#if (_MIPS_SZINT == 32)
#define BITSPERWORD	32
#define WORDMASK	31
#define BITSTOWORDS(x)	(x >> 5)
#endif
#if (_MIPS_SZINT == 64)
#define BITSPERWORD	64
#define WORDMASK	63
#define BITSTOWORDS(x)	(x >> 6)
#endif

static unsigned int
sptvctst(struct bitmap *bitmap, int color, register uint firstbit)
{
        register unsigned int fmask, i, w, *wp;

        /* insure that bitmap pointer is on a int boundary */
        ASSERT(((__psint_t)(bitmap->m_map) & 3) == 0);

        /* color &= CACHECOLORMASK; */
        if ((firstbit & CACHECOLORMASK) > color)
                firstbit += CACHECOLORSIZE;
        firstbit = (firstbit & ~CACHECOLORMASK) | color;

        wp = (unsigned int *)bitmap->m_map + BITSTOWORDS(firstbit);

	for (fmask = 0, i = color; i < BITSPERWORD; i += CACHECOLORSIZE)
		fmask |= (1 << i);

        for (i = firstbit & (BITSPERWORD - 1); firstbit < bitmap->m_highbit;
	     wp++, i = color) {

		if ((w = *wp) & fmask) {
                    for (; i < BITSPERWORD;
			 i+= CACHECOLORSIZE, firstbit += CACHECOLORSIZE) {
                         if (w & (1 << i))
                                return (firstbit);
                    }
                }
                else
		    firstbit += (BITSPERWORD - i + color);
        }
        return (firstbit);
}
#endif

/*
 * sptaligntst() ensures that the address chosen is aligned to a specific
 * page boundary. The alignment is specified in number of pages and is
 * a power of 2.
 * This code is very similar to sptvctst() but is written separately
 * because sptvctst() can do optimizations as it has multiple colors in
 * a single bit word. That may or may not be true for alignment. So a
 * general test is required. Moreover sptaligntst will not be called often.
 */

static unsigned int
sptaligntst(struct bitmap *bitmap, int alignment, register uint firstbit)
{
	char	*map;
	__psunsigned_t	page_addr;

        /* insure that bitmap pointer is on a int boundary */
        ASSERT(((__psint_t)(bitmap->m_map) & 3) == 0);

	ASSERT(alignment < btoct(K2SIZE));

	/*
	 * If already aligned return the aligned bit.
	 */

	page_addr = bitmap->m_unit0 + firstbit;

	if (!(page_addr & (alignment - 1))) return firstbit;

	map = bitmap->m_map;


        firstbit += (alignment - (page_addr & (alignment - 1)))
						 & (alignment - 1);

        while ( firstbit < bitmap->m_highbit) {

		if (btst(map, firstbit))
			break;
                else
		    firstbit += alignment;
        }

	/*
	 * If we don't find any valid bit we return > bitmap->m_highbit
	 * which will cause the caller to take necessary recovery action.
	 */
        return (firstbit);
}

/*
 * A hierarchy of spinning locks for bitmaps.
 *
 * Bitmap allocators hold off while the ``urgent'' bit is set in the
 * bitmap lock.  This bit is only asserted by a thread wanting to
 * push bits back into the clean bitmaps.  This avoids the problem of
 * having many would-be allocators hogging the map/lock, searching for bits
 * they might never find, while the replenisher routines fights to get in.
 */
#if MP
#define MAP_ALLOCLOCK(M,S)    { volatile uint_t *x = &(M)->m_lock; \
				while ((*x) & MAP_URGENT) ; \
				S = mutex_bitlock(&(M)->m_lock, MAP_LOCK); }
#else
#define MAP_ALLOCLOCK(M,S)	S = mutex_bitlock(&(M)->m_lock, MAP_LOCK)
#endif
#define MAP_ALLOCUNLOCK(M,S)	mutex_bitunlock(&(M)->m_lock, MAP_LOCK, S)

/*
 * No reason to check urgent bit if just updating, say, a stale map --
 * the stale lock is held for a very short time.
 */
#define MAP_UPDLOCK(M,S)	S = mutex_bitlock(&(M)->m_lock, MAP_LOCK)
#define MAP_UPDUNLOCK(M,S)	mutex_bitunlock(&(M)->m_lock, MAP_LOCK, S)

/*
 * Set urgent bit so threads trying to allocate from the maps
 * will back off and let map-replenishers do their work.
 *
 * nested_bitunlock turns off the bits using a XOR, this causes a
 * race with inbound CPUs that set the urgent bit - basicallly, 
 * 2 cpus can set it, and then the "last" cpu out can cause URGENT
 * to remain set since the first cpu clears it, and the second 
 * clears it again - but with an xor that causes it to be set again.
 * The fix is to be sure that the bit is set after we hold the LOCK.
 */
#if MP
#define MAP_URGENTLOCK(M)   {	atomicSetUint(&(M)->m_lock, MAP_URGENT); \
				nested_bitlock(&(M)->m_lock, MAP_LOCK); \
				atomicSetUint(&(M)->m_lock, MAP_URGENT);}

#define MAP_URGENTUNLOCK(M) nested_bitunlock(&(M)->m_lock, MAP_LOCK|MAP_URGENT)
#else
#define MAP_URGENTLOCK(M)
#define MAP_URGENTUNLOCK(M)
#endif

#ifdef R4000
static __psunsigned_t
color_alloc(
	struct sysbitmap *bm,
	register struct bitmap *bitmap)
{
	register char	*map;
	register int	bitlen, firstbit, length;
	int		s, rotor;
	uint_t		tries;

	if (bitmap->m_count == 0)
		return (__psunsigned_t)NULL;

	map = bitmap->m_map;
	rotor = 1;

	/*
	 * Don't bother to check urgency bits for these single-bit
	 * maps -- they are neither called often nor held long.
	 */
	s = mutex_bitlock(&bitmap->m_lock, MAP_LOCK);
	firstbit = bitmap->m_rotor;
	bitlen = bitmap->m_highbit - firstbit;
checking:
	while (bitlen > 0) {
		if (btst(map, firstbit)) {
			bclr(map, firstbit);
			bitmap->m_rotor = firstbit + 1;
			if (!rotor)
				bitmap->m_lowbit = firstbit + 1;
			bitmap->m_count--;
			ASSERT(bitmap->m_count >= 0);
			mutex_bitunlock(&bitmap->m_lock, MAP_LOCK, s);
			MINFO.sptalloc++;

			return(bitmap->m_unit0 + 
					firstbit * CACHECOLORSIZE);
		}

		if (++tries > 32) {
			tries = bm->m_gen;
			mutex_bitunlock(&bitmap->m_lock, MAP_LOCK, s);
			s = mutex_bitlock(&bitmap->m_lock, MAP_LOCK);
			if (tries != bm->m_gen)
				goto do_rotor;
			tries = 0;
		}

		length = bftstclr(map, firstbit, bitlen);
		firstbit += length;
		bitlen -= length;
	}

	if (rotor && bitmap->m_rotor > bitmap->m_lowbit) {
do_rotor:
		firstbit = bitmap->m_lowbit;
		bitlen = bitmap->m_rotor - firstbit;
		rotor = 0;
		goto checking;
	}
	mutex_bitunlock(&bitmap->m_lock, MAP_LOCK, s);

	return (__psunsigned_t)NULL;
}
#endif /* R4000 */


#define SPTROTOR_MAXMAP	(1 << (SPTROTORS-1))

/*
 * system page table allocation and deallocation routines
 * Alignment specifies the page boundary at which the virtual address
 * is to be aligned. 0 specifies no alignment. It has to be a power of 2.
 * Alignment has been added as a separate parameter as both color and alignment
 * can coexist.
 */
/* ARGSUSED */
__psunsigned_t
sptalloc(
	struct sysbitmap *bm,
	register int	want,
	int		flags,
	int		color,
	int		alignment)
{
	register struct bitmap *bitmap;
	register char	*map;
	register int	bitlen, firstbit, length, rotor;
	int		s;
	uint_t		tries;

	ASSERT(want >= 0);
	if (want == 0)
		return(NULL);

	/*
	 * Alignment has to be a power of 2. It should be checked in the
	 * the DKI interface routine.
	 */
	ASSERT ((alignment & (alignment - 1 )) == 0);

#ifdef R4000
	/*
	 * If searching for one specific color, use color-specific map.
	 * This will keep those who demand virtually-colored pages from
	 * fragmenting the general map (this breaks down, though, when
	 * callers want multiple colored pages).
	 */
	ASSERT(!(flags & VM_VACOLOR) || color >= 0 && color <= CACHECOLORMASK);
	if (flags & VM_VACOLOR && want == 1) {
		register __psunsigned_t addr;
		color &= CACHECOLORMASK;	/* paranoia */
start_over:
		if (addr = color_alloc(bm, bm->sptmap.m_color[color]))
			return addr;
	}
#endif
	bitmap = &bm->sptmap;
	map = bitmap->m_map;
	ASSERT(bitmap->m_lowbit == -1);

#if MP
	/* pre-emptive strike! */
	if (bitmap->m_count < bm->stale_sptmap.m_count)
		(void)clean_stale_sptmap(bm, NO_VADDR|TLB_NOSLEEP);
#endif

	/*
	 * rotors track the lowest-possible starting bit offsets
	 * for a power-of-two bit lengths.  Since bits are not often
	 * returned to the maps, it is efficient to keep the rotors
	 * as summary information.
	 */
	for (rotor = 0, firstbit = SPTROTORS, length = 1;
	     --firstbit > 0;
	     rotor++, length *= 2)
		if (want <= length)
			break;

	for (tries = 0; ; )
	{
		MAP_ALLOCLOCK(bitmap, s);
retry:
		firstbit = bitmap->m_startb[rotor];
#ifdef R4000
		if (flags & VM_VACOLOR)
			firstbit = sptvctst(bitmap, color, firstbit);
#endif
		if (alignment) 
			firstbit = sptaligntst(bitmap, alignment, firstbit);

		bitlen = bitmap->m_highbit - firstbit;

		while (bitlen >= want) {
			if ((length = bftstset(map, firstbit, want)) >= want) {

				bfclr(map, firstbit, want);

				/*
				 * Push up all rotors that track bit-lengths
				 * >= this bucket's (see comments below).
				 */
				if (want <= SPTROTOR_MAXMAP)
#ifdef R4000
				if (!(flags & VM_VACOLOR))
#endif
				{
					length = firstbit + want;
					ASSERT(length <= bitmap->m_highbit);
					if (want & (want-1))
						rotor++;
					while (rotor < SPTROTORS) {
						if (bitmap->m_startb[rotor] >=
									 length)
							break;
						bitmap->m_startb[rotor++] =
									 length;
					}
				}

				bitmap->m_count -= want;
				ASSERT(bitmap->m_count >= 0);
				MAP_ALLOCUNLOCK(bitmap, s);
				MINFO.sptalloc += want;

				return(bitmap->m_unit0 + firstbit);
			}

			if (++tries > 32) {
				tries = bm->m_gen;
				MAP_ALLOCUNLOCK(bitmap, s);
				delay_for_intr();
				MAP_ALLOCLOCK(bitmap, s);
				if (tries != bm->m_gen) {
					tries = 0;
					goto retry;
				}
				tries = 0;
			}

			firstbit += length;
			bitlen -= length;

			ASSERT(bitlen >= 0);
			length = bftstclr(map, firstbit, bitlen);
			firstbit += length;
#ifdef R4000
			if (flags & VM_VACOLOR) {
				firstbit = sptvctst(bitmap, color, firstbit);
				bitlen = bitmap->m_highbit - firstbit;
			} else
#endif
			if (alignment) {
				firstbit = sptaligntst(bitmap, alignment, 
								firstbit);
				bitlen = bitmap->m_highbit - firstbit;
			}

			bitlen -= length;
		}

		/*
		 * Push up all the rotors that track bit strings >= "want".
		 * Note that if "want" isn't a power-of-two, it doesn't
		 * exactly match the current rotor, so we start looking
		 * at "rotor+1".  Also note that if "want" is more than
		 * what m_startb[SPTROTORS] manages, it will still use
		 * the last rotor, so be careful not to bump it.
		 */
		if (want <= SPTROTOR_MAXMAP)
#ifdef R4000
		/*
		 * If VM_VACOLOR was set, we don't really know if we
		 * skipped otherwise-fine bits, so blow it off.
		 */
		if (!(flags & VM_VACOLOR))
#endif
		{
			length = rotor;
			if (want & (want-1))
				length++;

			if (firstbit > bitmap->m_highbit)
				firstbit = bitmap->m_highbit;

			while (length < SPTROTORS) {
				if (bitmap->m_startb[length] >= firstbit)
					break;
				bitmap->m_startb[length++] = firstbit;
			}
		}

		ASSERT(bitmap == &bm->sptmap);
		tries = bm->m_gen;
		MAP_ALLOCUNLOCK(bitmap, s);

		/*
		 * If caller can't wait, can't be important enough
		 * to disturb other subsystems.  Classic caller in
		 * this mode is single-color page table alloc -- it
		 * likely will find aged/stale pages in single-color maps.
		 */
		if (!(flags & VM_NOSLEEP))
			(void)shake_shake(SHAKEMGR_VM);

		/*
		 * Don't want a convoy of flush requests, so we
		 * check whether another process has already flushed
		 * the stale maps while we were shaking.  Clean_stale_sptmap
		 * returns immediately if another process is cleaning;
		 * if it finishes cleaning (sptmap_gen changed) before
		 * we return, fine, we search the bitmaps again;
		 * if it hasn't finished, we get on the wait-list
		 * while holding the wakeup spinlock.
		 */
		if (((tries == bm->m_gen) &&
		    (bm->stale_sptmap.m_count || bm->aged_sptmap.m_count))
#ifdef R4000
		 || (flags & VM_VACOLOR && want == 1 &&
		     (bm->stale_sptmap.m_color[color]->m_count ||
		      bm->aged_sptmap.m_color[color]->m_count))
#endif
		    ) {
			(void) clean_stale_sptmap(bm, NO_VADDR|TLB_NOSLEEP);
		}
#ifdef R4000
		/*
		 * If searching for one specific color,
		 * check color-specific map.
		 */
		if (flags & VM_VACOLOR && want == 1) {
			if (bm->sptmap.m_color[color]->m_count)
				goto start_over;
		}
#endif

		/*
		 * If sptmap's generation number has changed since we
		 * last tried to allocate, it's worth while trying again.
		 * Grab wait-structure lock and set MAP_WAIT bit before
		 * checking generation to avoid race with sptwakeup.
		 */

		MAP_ALLOCLOCK(bitmap, s);

		if (bm->m_gen != tries && bitmap->m_count >= want) {
			tries = 0;
			goto retry;
		}

		if (flags & VM_NOSLEEP) {
			MAP_ALLOCUNLOCK(bitmap, s);
			break;
		}

		bm->spt_wait.waiters = 1;

		if (flags & VM_BREAKABLE) {
			if (sv_bitlock_wait_sig(&bm->spt_wait.wait,
					 PWAIT|TIMER_SHIFT(AS_MEM_WAIT),
					 &bitmap->m_lock, MAP_LOCK, s))
				 break;
		} else
			sv_bitlock_wait(&bm->spt_wait.wait,
				 PMEM|TIMER_SHIFT(AS_MEM_WAIT),
				 &bitmap->m_lock, MAP_LOCK, s);
	}

	return NULL;
}

#if defined(MH_R10000_SPECULATION_WAR) && defined(_KRPF_TRACING)
void krpf_no_reference(__psint_t vfn);
#endif /* defined(MH_R10000_SPECULATION_WAR) && defined(_KRPF_TRACING) */

void
sptfree(
	struct sysbitmap *bm,
	register int size,
	register __psunsigned_t mapaddr)
{
	register struct bitmap *bitmap;
	register int firstbit;
	register int s;

#if defined(MH_R10000_SPECULATION_WAR) && defined(_KRPF_TRACING)
        {
                int     i;
                for (i = 0; i < size; i++)
                        krpf_no_reference(mapaddr + i);
        }
#endif /* defined(MH_R10000_SPECULATION_WAR) && defined(_KRPF_TRACING) */

#ifdef R4000
	bitmap = bm->stale_sptmap.m_color[0];
	if (bitmap && (mapaddr >= bitmap->m_unit0)) {
		ASSERT(bitmap->m_unit0 > bm->stale_sptmap.m_unit0);
		ASSERT(size == 1);
		bitmap = bm->stale_sptmap.m_color[mapaddr & CACHECOLORMASK];
		firstbit = mapaddr - bitmap->m_unit0;
		firstbit /= CACHECOLORSIZE;
	} else
#endif
	{
		bitmap = &bm->stale_sptmap;
		firstbit = mapaddr - bitmap->m_unit0;
	}

	if (size == 0)
		return;

	if (size < 0) {
		cmn_err_tag(287,CE_WARN,
			    "sptfree: bogus free size %d for bitmap at 0x%x",
			    size, bitmap);
		return;
	}
	if (firstbit < 0 || firstbit >= bitmap->m_size) {
		cmn_err_tag(288,CE_WARN,
			"sptfree: bogus bitmap value %d for bitmap at 0x%x",
			firstbit, bitmap);
			return;
	}
	if (firstbit + size > bitmap->m_size) {
		cmn_err_tag(289,CE_WARN,
			"sptfree: bitmap %d+%d overflows bitmap at 0x%x",
			firstbit, size, bitmap);
			return;
	}

	MAP_UPDLOCK(bitmap, s);

	ASSERT(bitmap->m_lowbit != -1);
	if (bitmap->m_lowbit > firstbit)
		bitmap->m_lowbit = firstbit;
	if (bitmap->m_highbit < firstbit + size)
		bitmap->m_highbit = firstbit + size;

	ASSERT(bftstclr(bitmap->m_map, firstbit, size) == size);
	bfset(bitmap->m_map, firstbit, size);
	bitmap->m_count += size;
	MAP_UPDUNLOCK(bitmap, s);

	MINFO.sptfree += size;
}

int
sptwait(struct sysbitmap *bm, int pri)
{
	register struct bitmap_wait *wp = &bm->spt_wait;
	register int s;

	pri &= (PMASK|PLTWAIT);
	pri |= TIMER_SHIFT(AS_MEM_WAIT);

	MAP_ALLOCLOCK(&bm->sptmap, s);
	wp->waiters = 1;

	if ((pri & PMASK) > PZERO) {
		return sv_bitlock_wait_sig(&wp->wait, pri,
				   &bm->sptmap.m_lock, MAP_LOCK, s);
	} else {
		sv_bitlock_wait(&wp->wait, pri,
				&bm->sptmap.m_lock, MAP_LOCK, s);
		return 0;
	}
}

void
sptwakeup(struct sysbitmap *bm)
{
	/*
	 * Don't need to always grap lock, because sptalloc is careful
	 * to set MAP_WAIT before checking sptmap generation number.
	 * At worst, we get a gratuitous call to sv_broadcast.
	 */
	register struct bitmap_wait *wp = &bm->spt_wait;

	if (wp->waiters) {
		int s;

		MAP_ALLOCLOCK(&bm->sptmap, s);
		wp->waiters = 0;
		(void)sv_broadcast(&wp->wait);
		MAP_ALLOCUNLOCK(&bm->sptmap, s);
	}
}

/*
 * The ``from'' bitmap must always be protected by a semaphore,
 */
void
mergebitmaps(
	struct sysbitmap *bm,
	register struct bitmap *to,
	register struct bitmap *from)
{
	register unsigned long *pto, *pfrom;
	register int first, last, size;

	ASSERT(issplhi(getsr()));
	ASSERT(from->m_lowbit != -1);

	first = from->m_lowbit / BITSPERLONG;
	last = (from->m_highbit + BITSPERLONG - 1) / BITSPERLONG;
	size = last - first;
	if (size < 0)
		return;

	pfrom = (unsigned long *)from->m_map + first;

	/*
	 * Set urgent bit so threads trying to allocate from the maps
	 * will back off and let map-replenishers do their work.
	 * Urgent bit gets turned off via unlock call below.
	 */
	MAP_URGENTLOCK(to);
	pto = (unsigned long *)to->m_map + first;

	while (--size >= 0) {
		register long ptmp;

		if (ptmp = *pfrom)
			*pto |= ptmp;
		pto++, pfrom++;
	}

	if (to->m_highbit < from->m_highbit)
		to->m_highbit = from->m_highbit;

	/*
	 * The from map is always managed with m_lowbit, not the set of
	 * m_startb lowbits.  If the ``to'' map is the node's sptmap,
	 * is uses rotors.
	 *
	 * It is a little tricky selecting new rotor values -- can't just
	 * reset them to MIN(from->m_lowbit, m_startb[size]) for each rotor,
	 * because there _could_ be bits behind the rotor for all but the
	 * first rotor (corresponding to groupings smaller than what that
	 * particular rotor manages).  But there should be no bits behind
	 * what m_startb[0] maps.
	 */
	if (to == &bm->sptmap) {
		register int firstbit = MIN(from->m_lowbit, to->m_startb[0]);
		size = SPTROTORS;
		while (--size >= 0)
			to->m_startb[size] = firstbit;

		bm->m_gen++;

		if (bm->spt_wait.waiters) {
			bm->spt_wait.waiters = 0;
			(void)sv_broadcast(&bm->spt_wait.wait);
		}
	} else {
		if (to->m_lowbit > from->m_lowbit)
			to->m_lowbit = from->m_lowbit;
	}

	to->m_count += from->m_count;
	ASSERT(to->m_count <= to->m_size);
	MAP_URGENTUNLOCK(to);

	from->m_count = 0;
	from->m_lowbit = from->m_size;
	from->m_highbit = 0;
	bzero(from->m_map + first * sizeof(long), (last-first) * sizeof(long));
}

void
bmapswtch(register struct bitmap *into, register struct bitmap *outof)
{
	register char	*map;
	register int	s;
	register u_int	low, high;
	int count;

	map = into->m_map;
	low = into->m_lowbit;
	high = into->m_highbit;
	count = into->m_count;

	/*
	 * Don't need high-priority lock here -- the maps being
	 * switched out-of are never the main allocator-maps.
	 * MAP_UPDLOCK just acquires the lock, without either asserting
	 * or checking the urgent bit.
	 */
	MAP_UPDLOCK(outof, s);
	into->m_map = outof->m_map;
	into->m_lowbit = outof->m_lowbit;
	into->m_highbit = outof->m_highbit;
	into->m_count = outof->m_count;
	outof->m_map = map;
	outof->m_lowbit = low;
	outof->m_highbit = high;
	outof->m_count = count;
	MAP_UPDUNLOCK(outof, s);
}

void
sptgetsizes(struct sysbitmap *bm, int *c, int *s, int *a, int *i)
{
	int clean, stale, aged, intrans;
#if R4000
	int j;
#endif

	clean = bm->sptmap.m_count;
	stale = bm->stale_sptmap.m_count;
	aged = bm->aged_sptmap.m_count;
	intrans = bm->temp_sptmap.m_count;

#if R4000
	for (j = 0; j < CACHECOLORSIZE; j++) {
		clean += bm->sptmap.m_color[j]->m_count;
		stale += bm->stale_sptmap.m_color[j]->m_count;
		aged += bm->aged_sptmap.m_color[j]->m_count;
		intrans += bm->temp_sptmap.m_color[j]->m_count;
	}
#endif
	*c += clean;
	*a += aged;
	*s += stale;
	*i += intrans;
}

#if defined(EVEREST) || defined(SN0)

extern int isolate_debug;
extern int isolate_drop;
extern int sptsz;
#ifdef R4000
extern int clrsz;
#endif

static cpumask_t
_kvfaultcheck(struct sysbitmap *bm, int flags)
{
	register char *pstale, *pfault;
	register struct bitmap *check;
	register int i, j;
	int size, s;
	cpumask_t flushmask;
	pda_t   *npda;
	extern lock_t kvfaultlock;

#ifdef R4000
	int sz, k, flushthiscpu;
	
	sz = clrsz / NBBY;
#endif
	CPUMASK_CLRALL(flushmask);
	check = (flags & NO_VADDR) ? &bm->temp_sptmap : &bm->aged_sptmap;
	size = (sptsz + NBBY - 1) / NBBY;
	for (i=0; i<maxcpus; i++) {
		if (pdaindr[i].CpuId == -1)
			continue;
		npda = pdaindr[i].pda;

		/* if any of the merged stale pages have been
		** faulted on this cpu then we have to flush its tlb
		*/
		s = mutex_spinlock_spl(&kvfaultlock, spl7);
		if (npda->p_kvfault == 0) {
			if (flags & NO_VADDR)
				 CPUMASK_SETM(flushmask, npda->p_cpumask);
			mutex_spinunlock(&kvfaultlock, s);
			continue;
		}
		pfault = (char *)npda->p_kvfault;
		pstale = (char *)check->m_map;
		for (j=size; j>0; j--) {
			if (*pfault & *pstale) {
				CPUMASK_SETM(flushmask, npda->p_cpumask);
#ifdef ISOLATE_DEBUG
				if (npda->p_flags & PDAF_ISOLATED &&
						isolate_debug) {
					cmn_err(CE_WARN,
					"Isolated proc %d tlbflush (bidx %x, %s)\n",
					npda->p_cpuid, size - j, (flags & NO_VADDR) ? "temp_map" : "aged_map");
					if (isolate_drop) debug((char *) 0);
				}
#endif
				break;
			}
			pfault++; pstale++;
		}
#ifdef R4000
		flushthiscpu = 0;
		for (k=0; k < CACHECOLORSIZE; k++) {
			pfault = (char *) (npda->p_clrkvflt[k]);
			if (flags & NO_VADDR)
				pstale = (char *)bm->temp_sptmap.m_color[k]->m_map;
			else
				pstale = (char *)bm->aged_sptmap.m_color[k]->m_map;
			for (j = sz; j > 0; j--) {
				if (*pfault & *pstale) {
					CPUMASK_SETM(flushmask, npda->p_cpumask);
#ifdef ISOLATE_DEBUG
					if (npda->p_flags & PDAF_ISOLATED
							&& isolate_debug) {
						cmn_err(CE_WARN,
						"Isolated proc %x tlbflush (cidx %x, bidx %x, %s)\n",
						npda->p_cpuid, k, sz - j, (flags & NO_VADDR) ? "temp_map" : "aged_map");
						if (isolate_drop)
							debug((char *) 0);
					}
#endif
					flushthiscpu++;
					break;
				}
				pfault++;
				pstale++;
			}
			if (flushthiscpu)
				break;
		}
#endif
		mutex_spinunlock(&kvfaultlock, s);
	}
	return(flushmask);
}

cpumask_t
kvfaultcheck(int flags)
{
	cpumask_t mask;

	CPUMASK_CLRALL(mask);
	mask = _kvfaultcheck(&sptbitmap, flags);
	return mask;
}

#else /* !EVEREST && !SN0 */

/* ARGSUSED1 */
cpumask_t
kvfaultcheck(int flags)
{
	return maskcpus;
}

#endif	/* #ifdef EVEREST */
