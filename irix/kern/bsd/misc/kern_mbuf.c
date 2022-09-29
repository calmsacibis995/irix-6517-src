/*
 * Copyright 1997, Silicon Graphics, Inc.  All rights reserved.
 *
 * IRIX mbuf allocator and utility routines.
 * Four types of mbufs are supported:
 * 1. Regular mbufs which contain up to 'MLEN' bytes of contiguous data.
 * 2. Cluster mbufs which are regular mbufs but contain a pointer
 *    to a variable-sized, non-contiguous memory buffer.
 * 3. Shared cluster mbufs which are like cluster mbufs but point
 *    to a user page marked Copy-On-Write.
 * 4. Loaned cluster mbufs which are like cluster mbufs but point
 *    to an externally allocated non-contiguous kernel buffer.
 *
 * The principal allocator data structure is the (struct mb) declared in the
 * mbuf_private.h header file. It contains a set of various size buffer lists.
 * An empty mbuf list requires that a new mbuf be constructed from the kernel
 * zone heap.  Cluster mbufs are cached as mbuf+buffer "objects" for speed.
 * One mb structure is maintained for each node (SN0) or group of CPUs
 * (EVEREST) and the memory comprising the mbufs resides on that node.
 *
 * This design trades off storage for speed, so periodic "shaking" of the
 * various sized lists is required to reclaim memory.  This is done both by
 * the normal zone shaker function as well as our own periodic timer function
 * which returns memory back to the system, but at a much slower rate.
 */
#ident "$Revision: 4.118 $"

#include "tcp-param.h"
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/idbgentry.h>
#include <sys/sema.h>
#include <sys/cmn_err.h>
#include <sys/mbuf.h>
#include <sys/mbuf_private.h>
#include <sys/tcpipstats.h>
#include <sys/rtmon.h>
#include <ksys/kern_heap.h>
#include <ksys/as.h>

#ifdef _MBUF_UTRACE
#define MBUF_UTRACE(name, mb, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(mb), \
		   UTPACK((ra), (mb) ? (mb)->m_off : 0));
#else
#define MBUF_UTRACE(name, mb, ra)
#endif

/*
 * External variables
 */
extern int mbmaxpages;
extern int mbtimeout;
extern int m_flip_off;
extern int m_shget_off;
extern int numcpus;
extern int numnodes;

/*
 * External procedures
 */
extern int start(void);
extern int zone_shake(zone_t*);

#ifdef EVEREST
extern int	io4ia_war;
#endif

uint mbuf_reclaim_needed = 0;	/* flag to trigger protocol drain */
uint mbdrain = 0;	/* # times protocol drain was called */
uint mbshake = 0;	/* # times mbshake() was called */
uint mbpages = 0;	/* # pages allocated from system */

/*
 * Constants for utility programs
 */
struct mbufconst _mbufconst = {
	MSIZE,		/* m_msize */
	MBUFCL_SIZE,	/* m_mbufclsize */
	MCLBYTES,	/* m_mclbytes */
	MMINOFF,	/* m_mminoff */
	MTAIL,		/* m_mtail */
	MMAXOFF,	/* m_maxoff */
	MLEN		/* m_mlen */
};

#if (_MIPS_SZPTR == 64)
#define	mtoindex(m)	(m)->m_index
#define	msetindex(m, n)	(m)->m_index = (n);
#define	DEADBEEF	0xdeadbeefdeadbeef
#else
#define	mtoindex(m)	0
#define	msetindex(m, n)	ASSERT(n == 0)
#define	DEADBEEF	0xdeadbeef
#endif

#define	MBISLOCKED(mb)	spinlock_islocked(&mb->mb_lock)
#define	GOODNODE(m)	ASSERT(mtoindex(m) < MBNUM)

#if defined(DEBUG) || defined(DEBUG_ALLOC)
#define	MBFREE(m, lp)	{ (m)->m_type = MT_FREE; MBCLEANFREE(m, lp); }
#else
#define	MBFREE(m, lp)	{ MBCLEANFREE(m, lp); }
#endif
#define	MBCLEANFREE(m, lp) (m)->m_next = *(lp); *(lp) = (m)

/*
 * Used only for mcb_XXX and m_pcbXXX. Others protected by lock.
 */
#define	MBTYPE_INC(mb, type)	atomicAddInt(&(mb)->mb_types[(type)], 1)
#define	MBTYPE_DEC(mb, type)	atomicAddInt(&(mb)->mb_types[(type)], -1)

struct mb *mbcache[MBMAXCPU];

/*
 * Local prototypes
 */
void mbinit(void);
static struct mbuf *m_constructreg(struct mb *mb);
static struct mbuf *m_constructcl(int size, struct mb *mb);
static void m_destruct(struct mbuf *m, struct mb *mb);
static int mbflush(struct mb *mb, struct mbuf **mlistp, zone_t *z);
static void *mbzonealloc(struct mb *mb, zone_t *z);
static void mbtune(void);
void m_reclaim(void);
int m_shake(int level);
void mbscavenge(void);

int get_mbufconst(void *user_address, int insize, int *outsize);
void m_allocstats(struct mbstat *mbstatp);
int m_count(struct mbuf *m);

void m_pcbinc(int nbytes, int type);
void m_pcbdec(int nbytes, int type);

#if defined(DEBUG) || defined(DEBUG_ALLOC)
static void m_poison(struct mbuf *m, inst_t *ra);
#define	M_POISON(m, ra)	m_poison(m, ra)
#define	M_POISON_HDR(m, ra) kmem_poison(m, offsetof(struct mbuf, m_dat), ra)
#else
#define	M_POISON(m, ra)
#define	M_POISON_HDR(m, ra)
#endif

/*
 * Initialize mbuf allocator.
 */
void
mbinit(void)
{
	struct mb *mb;
	cpuid_t cpu;

	extern int netthread_pri;

	if (mbcache[0]) { /* return if already initialized */
		return;
	}
	mbtune();

	if (MBNUM > MBMAXCPU) { /* exceed maximum cpu count supported */
		panic("mbinit: maxcpus greater than MBMAXCPU constant");
	}
	for (cpu = 0; cpu < MBNUM; cpu++) {

		if (!cpu_enabled(cpu) || pdaindr[cpu].CpuId == -1) { /* skip */
			continue;
		}
		mb = kmem_zalloc_node(sizeof(struct mb), KM_NOSLEEP,
			cputocnode(cpu));
		if (mb == NULL) {
			panic("mbinit: can't allocate mb structures");
		}

		init_spinlock(&mb->mb_lock, "mb lock", (int)cpu);
		mb->mb_index = (int)cpu;

		mb->mb_smlzone = kmem_zone_private(MSIZE, "mbuf smlzone");
		if (mb->mb_smlzone == NULL) {
			panic("mbinit: kmem_zone_private failed");
		}

		(void) kmem_zone_private_mode_noalloc(mb->mb_smlzone);
		(void) kmem_zone_enable_shake(mb->mb_smlzone);

		mb->mb_medzone = kmem_zone_private(MBUFCL_SIZE,"mbuf medzone");
		if (mb->mb_medzone == NULL) {
			panic("mbinit: kmem_zone_private failed");
		}
		(void) kmem_zone_private_mode_noalloc(mb->mb_medzone);
		(void) kmem_zone_enable_shake(mb->mb_medzone);
		sv_init(&mb->mb_sv, SV_DEFAULT, "mb sv");

		mbcache[(int)cpu] = mb;
	}

	shake_register(SHAKEMGR_MEMORY, m_shake);
	timeout_pri(mbscavenge, 0, mbtimeout * HZ, netthread_pri+1);
}

/*
 * Allocate a regular mbuf.
 */
struct mbuf*
m_get(int canwait, int type)
{
	struct mb *mb;
	struct mbuf *m;
	int s;

	GOODMT(type);

tryagain:
	mb = mbcache[MBINDEX()];

	s = mutex_spinlock(&mb->mb_lock);

	mb->mb_types[type]++;

	if (m = mb->mb_sml) {
		mb->mb_sml = m->m_next;
		mb->mb_smlfree--;
	}

	mutex_spinunlock(&mb->mb_lock, s);

	if (m == NULL)
		m = m_constructreg(mb);

	if (m) {
		ASSERT(mtoindex(m) == mb->mb_index);
		ASSERT(m->m_flags == 0);
		ASSERT(m->m_type == MT_FREE);
		GOODVA(mb->mb_index, (caddr_t) m);

		M_POISON(m, __return_address);

		m->m_next = 0;
		m->m_act = 0;
		m->m_off = MMINOFF;
		m->m_len = 0;
		m->m_type = type;
		MBUF_UTRACE(UTN('m_ge','t   '), m, __return_address);
		return (m);
	}

#pragma mips_frequency_hint NEVER
	s = mutex_spinlock(&mb->mb_lock);
	mb->mb_types[type]--;
	if (canwait == M_WAIT) {
		mb->mb_needwake = 1;
		mb->mb_wait++;
		sv_wait(&mb->mb_sv, PZERO, &mb->mb_lock, s);
		goto tryagain;
	}
	mb->mb_drops++;
	mutex_spinunlock(&mb->mb_lock, s);
	return (NULL);
}

/*
 * Allocate a variable size regular or cluster mbuf.
 */
struct mbuf*
m_vget(int canwait, int size, int type)
{
	struct mb *mb;
	struct mbuf *m;
	int s;
#ifdef MH_R10000_SPECULATION_WAR
	int osize = size;
#endif

	ASSERT((size > 0) && (size <= MCLBYTES));
	GOODMT(type);

	if (size <= MLEN) {
#pragma mips_frequency_hint NEVER
		if (m = m_get(canwait, type))
			m->m_len = size;
		MBUF_UTRACE(UTN('m_vg','et  '), m, __return_address);
		return (m);
	}

#ifdef MH_R10000_SPECULATION_WAR
	/*
	 * Avoid speculative store problems with shared pages; basically
	 * we need to ensure that another driver doesn't write into our page
	 * while we're DMAing it.
	 */
	if ((size < NBPP) && IS_R10000()) {
		size = NBPP;
	}
#endif

    tryagain:
	mb = mbcache[MBINDEX()];

	s = mutex_spinlock(&mb->mb_lock);

	if (size <= MBUFCL_SIZE) {
		if (m = mb->mb_med) {
			mb->mb_medfree--;
			mb->mb_med = m->m_next;
		}
	} else {
		if (size > MCLBYTES) {
#pragma mips_frequency_hint NEVER
			cmn_err_tag(278,CE_PANIC, 
				"m_vget() called with size %d from 0x%x", size,
				__return_address);
		}
		if (m = mb->mb_big) {
			mb->mb_bigfree--;
			mb->mb_big = m->m_next;
		}
	}
	mb->mb_types[type]++;

	mutex_spinunlock(&mb->mb_lock, s);

	if (m == NULL)
#pragma mips_frequency_hint NEVER
		m = m_constructcl(size, mb);

	if (m) {
		ASSERT(mtoindex(m) == mb->mb_index);
		ASSERT(m->m_flags == M_CLUSTER);
		ASSERT(m->m_refp == &m->m_ref);
		ASSERT(m->m_ref == 0);
		GOODVA(mb->mb_index, (caddr_t)m);
		if (m->m_size != MCLBYTES)
			GOODVA(mb->mb_index, m->m_p);

		M_POISON(m, __return_address);

		m->m_next = 0;
		m->m_act = 0;
		m->m_off = (__psunsigned_t)m->m_p - (__psunsigned_t)m;
#ifdef MH_R10000_SPECULATION_WAR
		m->m_len = osize;
#else
		m->m_len = size;
#endif
		m->m_type = type;
		m->m_ref = 1;
		MBUF_UTRACE(UTN('m_vg','et2 '), m, __return_address);

		return (m);
	}
#pragma mips_frequency_hint NEVER
	s = mutex_spinlock(&mb->mb_lock);
	mb->mb_types[type]--;
	if (canwait == M_WAIT) {
		mb->mb_needwake = 1;
		mb->mb_wait++;
		sv_wait(&mb->mb_sv, PZERO, &mb->mb_lock, s);
		goto tryagain;
	}
	mb->mb_drops++;
	mutex_spinunlock(&mb->mb_lock, s);
	return (NULL);
}

/*
 * Free an mbuf and return the next mbuf in the m_next chain.
 */
struct mbuf *
#if defined(DEBUG) || defined(DEBUG_ALLOC)
_m_free(struct mbuf *m, inst_t *ra)
#else
m_free(struct mbuf *m)
#endif
{
	struct mb *mb;
	struct mbuf *mowner;
	struct mbuf *n;
	caddr_t p;
	int ref;
	int s;

#if defined(DEBUG) || defined(DEBUG_ALLOC)
	ASSERT_ALWAYS(((m->m_type) > MT_FREE) && ((m->m_type) < MT_MAX));
	ASSERT_ALWAYS((IS_KSEG0(m) || IS_KSEG2(m)) && ((__psint_t)(m) % MSIZE) == 0);
#endif
	GOODNODE(m);

#if defined(DEBUG) || defined(DEBUG_ALLOC)
	MBUF_UTRACE(UTN('m_fr','ee  '), m, ra);
#else
	MBUF_UTRACE(UTN('m_fr','ee  '), m, __return_address);
#endif

#ifdef EVEREST
	if (io4ia_war && m->m_len) {
		volatile char c;
		c = *(mtod(m, char*) + (m->m_len - 1));
	}
#endif

	n = m->m_next;
	m->m_next = 0;
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	m->m_act = (struct mbuf*)((__psunsigned_t)ra|1);
#else
	m->m_act = 0;
#endif

	if (M_HASCL(m) && m->m_freefunc)
#if defined(DEBUG) || defined(DEBUG_ALLOC)
		(*m->m_freefunc)(m, ra);
#else
		(*m->m_freefunc)(m);
#endif

	mb = mbcache[mtoindex(m)];
	p = NULL;

	GOODVA(mb->mb_index, (caddr_t) m);

	if (!M_HASCL(m)) {	/* small mbuf */
		m->m_flags = 0;
		s = mutex_spinlock(&mb->mb_lock);
		mb->mb_types[m->m_type]--;
		M_POISON(m, ra);
		MBFREE(m, &mb->mb_sml);
		mb->mb_smlfree++;
		if (mb->mb_needwake) {
#pragma mips_frequency_hint NEVER
			mb->mb_needwake = 0;
			sv_broadcast(&mb->mb_sv);
		}
		mutex_spinunlock(&mb->mb_lock, s);
		return (n);
	}

	ASSERT(*m->m_refp > 0);
	mowner = (struct mbuf*) ((long)m->m_refp & ~(MSIZE-1));

	ref = atomicAddUint(m->m_refp, -1);

	s = mutex_spinlock(&mb->mb_lock);
	mb->mb_types[m->m_type]--;

	/* free us if we're not the owner */
	if (m != mowner) {
		m->m_flags = 0;
		m->m_refp = &m->m_ref;
		m->m_ref = 0;
		M_POISON(m, ra);
		MBFREE(m, &mb->mb_sml);
		mb->mb_smlfree++;
	}

	if (ref == 0) {
		if (m != mowner) {	/* switch */
			mutex_spinunlock(&mb->mb_lock, s);
			GOODNODE(mowner);
			mb = mbcache[mtoindex(mowner)];
			s = mutex_spinlock(&mb->mb_lock);
			m = mowner;
		}
		if ((m->m_flags & (M_SHARED|M_LOANED)) == 0) {
			if (m->m_size < MCLBYTES)
				GOODVA(mb->mb_index, m->m_p);
			M_POISON(m, ra);
			m->m_flags = M_CLUSTER;
			if (m->m_size <= MBUFCL_SIZE) {
				MBFREE(m, &mb->mb_med);
				mb->mb_medfree++;
			} else {
				MBFREE(m, &mb->mb_big);
				mb->mb_bigfree++;
			}
		}
		else {
			if (m->m_flags & M_SHARED)
				p = m->m_p;
			m->m_flags = 0;
			M_POISON(m, ra);
			MBFREE(m, &mb->mb_sml);
			mb->mb_smlfree++;
		}
	}

	if (mb->mb_needwake) {
#pragma mips_frequency_hint NEVER
		mb->mb_needwake = 0;
		sv_broadcast(&mb->mb_sv);
	}
	mutex_spinunlock(&mb->mb_lock, s);

	if (p)
#pragma mips_frequency_hint NEVER
		prelease(p);

	return (n);
}

#if defined(DEBUG) || defined(DEBUG_ALLOC)
struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n;
	n = _m_free(m, __return_address);
	return n;
}
#endif /* DEBUG || DEBUG_ALLOC */

/*
 * Disassociate a large cluster mbuf from its page and
 * replace it with a new page if possible.
 */
int
m_stealcl(struct mbuf *m)
{
	struct mb *mb;
	caddr_t p;
	pfd_t *pfd;
	int s;

	ASSERT((m->m_flags & (M_CLUSTER | M_SHARED | M_LOANED)) == M_CLUSTER);
	ASSERT(m->m_p);
	ASSERT(m->m_size == MCLBYTES);

	if (m->m_refp != &m->m_ref || *m->m_refp != 1 || m->m_freefunc)
		return -1;

	/*
	 * We need to free the virtual address mapping m->m_p, but still
	 * keep the physical page. So bump the pf_use counter here. We
	 * can't use the pfd_useinc() macro because it asserts P_HASH or
	 * P_BAD, which aren't set here.
	 */

	pfd = kvatopfdat(m->m_p);
	s = pfdat_lock(pfd);
	pfdat_inc_usecnt(pfd);
	pfdat_unlock(pfd, s);

	cache_operation(m->m_p, NBPP,
			CACH_DCACHE|CACH_WBACK|CACH_IO_COHERENCY);
	kvpfree(m->m_p, 1);

	mb = mbcache[mtoindex(m)];
	p = (caddr_t) kvpalloc_node_hint(MBCNODEID(mb->mb_index), 1, KM_NOSLEEP, 0);

	/*
	 * If we couldn't allocate a page, demote from cluster to regular mbuf
	 */
	if (p) {
		m->m_off += p - m->m_p;
		m->m_p = p;
	} else {
		atomicAddUint(&mb->mb_bigtot, -1);
		atomicAddUint(&mbpages, -1);
		m->m_off = MMINOFF;
		m->m_len = 0;
		m->m_flags ^= M_CLUSTER;
	}
	return 0;
}

/*
 * Free an entire mbuf chain
 */
void
m_freem(struct mbuf *m)
{
	MBUF_UTRACE(UTN('m_fr','eem '), m, __return_address);
	while (m) {
#if defined(DEBUG) || defined(DEBUG_ALLOC)
		m = _m_free(m, __return_address);
#else
		m = m_free(m);
#endif
	}
}

/*
 * Construct a regular mbuf from scratch.
 */
static struct mbuf*
m_constructreg(struct mb *mb)
{
	struct mbuf *m;

	if ((m = (struct mbuf*) mbzonealloc(mb, mb->mb_smlzone)) == NULL)
	    return (NULL);

	GOODVA(mb->mb_index, (caddr_t) m);

	atomicAddUint(&mb->mb_smltot, 1);

	msetindex(m, mb->mb_index);
	m->m_flags = 0;
	m->m_act = 0;
	m->m_next = 0;
	m->m_type = MT_FREE;

	return (m);
}

/*
 * Construct a cluster mbuf from scratch.
 */
/* ARGSUSED */
static struct mbuf*
m_constructcl(int size, struct mb *mb)
{
	struct mbuf *m;
	caddr_t p;

	ASSERT((size >= MLEN) && (size <= MCLBYTES));

	size = (size <= MBUFCL_SIZE)? MBUFCL_SIZE: MCLBYTES;

	if ((m = m_constructreg(mb)) == NULL)
		return (NULL);

	if (size <= MBUFCL_SIZE)
		p = mbzonealloc(mb, mb->mb_medzone);
	else {
		if (mbpages < mbmaxpages) {
			p = (caddr_t) kvpalloc_node_hint(MBCNODEID(mb->mb_index), 
						    1, KM_NOSLEEP, 0);
		} else {
			p = NULL;
		}
		if (p) {
			atomicAddUint(&mbpages, 1);
		}
	}

	if (p == NULL) {
		m_destruct(m, mb);
		return (NULL);
	}

	GOODVA(mb->mb_index, p);

	/* don't count a cluster twice */
	atomicAddUint(&mb->mb_smltot, -1);

	if (size <= MBUFCL_SIZE)
		atomicAddUint(&mb->mb_medtot, 1);
	else
		atomicAddUint(&mb->mb_bigtot, 1);

	ASSERT(mtoindex(m) == mb->mb_index);
	m->m_flags = M_CLUSTER;
	m->m_type = MT_FREE;
	m->m_act = 0;
	m->m_next = 0;
	m->m_freefunc = (void(*)()) NULL;
	m->m_farg = 0;
	m->m_dupfunc = (void(*)()) NULL;
	m->m_darg = 0;
	m->m_p = p;
	m->m_size = size;
	m->m_refp = &m->m_ref;
	m->m_ref = 0;

	return (m);
}

/*
 * Deallocate a regular or cluster mbuf.
 */
static void
m_destruct(struct mbuf *m, struct mb *mb)
{
	ASSERT(m->m_type == MT_FREE);
	ASSERT((m->m_flags == 0) || (m->m_flags == M_CLUSTER));
	GOODVA(mb->mb_index, (caddr_t) m);

	if (!M_HASCL(m)) {
		M_POISON_HDR(m, (inst_t *)__return_address);
		kmem_zone_free(mb->mb_smlzone, (caddr_t)m);
		atomicAddUint(&mb->mb_smltot, -1);
	}
	else {
		ASSERT(m->m_ref == 0);
		ASSERT((m->m_size == MBUFCL_SIZE) || (m->m_size == MCLBYTES));
		ASSERT(m->m_freefunc == NULL);
		ASSERT(m->m_dupfunc == NULL);
	
		if (m->m_size <= MBUFCL_SIZE) {
			GOODVA(mb->mb_index, m->m_p);
			M_POISON_HDR(m, (inst_t *)__return_address);
			kmem_zone_free(mb->mb_medzone, (caddr_t) m->m_p);
			atomicAddUint(&mb->mb_medtot, -1);
		} else {
			kvpfree(m->m_p, 1);
			atomicAddUint(&mb->mb_bigtot, -1);
		}

		M_POISON_HDR(m, (inst_t *)__return_address);
		kmem_zone_free(mb->mb_smlzone, (caddr_t)m);
	}
}

/*
 * Wrapper for kmem_zone_alloc().
 * The mb zones are private and require "filling" with a new page when empty.
 */
/*ARGSUSED*/
static void*
mbzonealloc(struct mb *mb, zone_t *z)
{
	void *p;

	if (p = kmem_zone_alloc(z, KM_NOSLEEP))
		return (p);

	if (mbpages >= mbmaxpages) {
		m_shake(SHAKEMGR_MEMORY);
		if (mbpages >= mbmaxpages) {
			m_reclaim();
			/* try zone again in case it is heavily fragmented */
			p = kmem_zone_alloc(z, KM_NOSLEEP);
			return (p);
		}
	}

	p = kvpalloc_node_hint(MBCNODEID(mb->mb_index), 1, KM_NOSLEEP, 0);
	if (p == NULL)
		return (NULL);

	kmem_zone_fill(z, p, NBPP);

	atomicAddUint(&mbpages, 1);

	return (kmem_zone_alloc(z, KM_NOSLEEP));
}

void
m_mcbfail(void)
{
	struct mb *mb;

	mb = mbcache[MBINDEX()];
	atomicAddInt(&mb->mb_mcbfail, 1);
}

/*
 * Allocate a control block.
 */
void*
mcb_get(int canwait, int size, int type)
{
	struct mb *mb;
	void *result;
	int flags;

	GOODMT(type);
	ASSERT(type != MT_DATA);

	mb = mbcache[MBINDEX()];

	flags = (canwait == M_WAIT) ? KM_SLEEP : KM_NOSLEEP;

	if (result = kmem_zalloc(size, flags)) {
		MBTYPE_INC(mb, type);
		atomicAddInt(&mb->mb_mcbtot, 1);
		atomicAddInt64(&mb->mb_mcbbytes, size);
	} else {
		atomicAddInt(&mb->mb_mcbfail, 1);
	}

	return (result);
}

/*
 * Free a control block.
 */
void
mcb_free(void *mcb, int size, int type)
{
	struct mb *mb;

	GOODMT(type);
	ASSERT(type != MT_DATA);

	mb = mbcache[MBINDEX()];

	kmem_free(mcb, size);

	MBTYPE_DEC(mb, type);
	atomicAddInt(&mb->mb_mcbtot, -1);
	atomicAddInt64(&mb->mb_mcbbytes, -size);
}

/*
 * Get a data mbuf and point it at len bytes of memory starting at addr.
 * Set the mbuf free and dup functions and arguments.
 */
struct mbuf *
mclgetx(dfun, darg, ffun, farg, addr, len, wait)
	void (*dfun)(), (*ffun)();
	int len, wait;
	long farg, darg;
	caddr_t addr;
{
	struct mbuf *m;

	if ((m = m_get(wait, MT_DATA)) == NULL)
#pragma mips_frequency_hint NEVER
		return (NULL);
	m->m_flags |= M_CLUSTER|M_LOANED;
	m->m_off = (__psint_t)addr - (__psint_t)m;
	m->m_len = len;
	m->m_freefunc = ffun;
	m->m_farg = farg;
	m->m_dupfunc = dfun;
	m->m_darg = darg;
	m->m_refp = &m->m_ref;
	m->m_ref = 1;
#if defined(DEBUG) || defined(DEBUG_ALLOC)
	m->m_p = (caddr_t) DEADBEEF;
	m->m_size = 0;
#endif
	MBUF_UTRACE(UTN('mclg','etx '), m, __return_address);
	return (m);
}

void
mbtune(void)
{
	if (mbmaxpages == 0)
		mbmaxpages = physmem / 4;
}

/*
 * Adjust an mbuf by a specified number of bytes starting
 * from either the head or tail of the mbuf chain.
 * If len >= 0 then remove 'len' bytes from the head.
 * If len < 0 then remove 'len' bytes from the tail.
 */
void
m_adj(struct mbuf *mp, int len)
{
	struct mbuf *m;

	if ((m = mp) == NULL)
		return;

	GOODMP(m);
	GOODMT(m->m_type);

	if (len >= 0) { /* Trim from head */

		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_off += len;
				break;
			}
		}
	} else { /* Trim from tail */

		len += m_length(mp);
		/*
		 * The correct length for the chain is "len".
		 * Find the mbuf with last data to retain, adjust its
		 * length and toss data from remaining mbufs on chain.
		 */
		for (m = mp; ; m = m->m_next) {

			len -= m->m_len;

			if (len <= 0) {
				/*
				 * Release entire mbuf chain from
				 * here on if anything is there to release.
				 */
				if (m->m_next != 0) {
					m_freem(m->m_next);
					m->m_next = 0;
				}
				if (len != 0) {
					int newlen;

					/*
					 * Adjust length field of the last mbuf
					 */
					newlen = m->m_len + len;
					if (newlen < 0)
						newlen = 0;
					m->m_len = newlen;
				}
				return;
			}
		}
	}
}

/*
 * Concatentate mbuf chain n onto mbuf chain m.
 */
void
m_cat(struct mbuf *m, struct mbuf *n)
{
	GOODMP(m);
	GOODMT(m->m_type);

	GOODMP(n);
	GOODMT(n->m_type);

	while (m->m_next)
		m = m->m_next;

	while (n) {
		if (M_HASCL(m) ||
			(m->m_off + m->m_len + n->m_len > MMAXOFF)) {
			/*
			 * just join the two chains
			 */
			m->m_next = n;
			return;
		}
		/* copy data from one into the other */
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
			(u_int)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

/*
 * Make and return a copy of the mbuf chain
 * starting at 'off' bytes and extending for 'n' bytes.
 */
struct mbuf *
m_copy(struct mbuf *m, int off, int len)
{
	struct mbuf *n, **np, *top;
	int mlen;

	ASSERT(off >= 0 && len >= 0);

	if (len == 0)
		return (0);
	do {
		GOODMP(m);
		GOODMT(m->m_type);

		if (m == NULL)
			return m;
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	} while (off >= 0);		/* skip 0-length mbufs */

	np = &top;
	top = 0;
	do {
		if (m == 0) {
			ASSERT(len == M_COPYALL);
			break;
		}
		if (m->m_len > 0) {
			n = m_get(M_DONTWAIT, m->m_type);

			if (n == (struct mbuf *)0) { /* no mbuf */
				m_freem(top);
				return (0);
			}

			*np = n;
			np = &n->m_next;

			mlen = m->m_len - off;
			if (len != M_COPYALL) {
				if (mlen > len)
					mlen = len;
				len -= mlen;
			}
			n->m_len = mlen;

			if (M_HASCL(m)) {
				ASSERT(*m->m_refp > 0);
				n->m_off = (mtod(m,__psunsigned_t) -
						(__psunsigned_t)n) + off;
				n->m_flags = m->m_flags;
				n->m_u.m_us = m->m_u.m_us;
				atomicAddUint(n->m_refp, 1);

				if (m->m_dupfunc)
#if defined(DEBUG) || defined(DEBUG_ALLOC)
					(*m->m_dupfunc)(n, __return_address);
#else
					(*m->m_dupfunc)(n);
#endif
			} else {
				ASSERT(m->m_off >= MMINOFF);
				bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t), mlen);
			}

			off = 0;
		}
		m = m->m_next;
	} while (len > 0);

	return (top);
}

/*
 * Copy up to 'maxlen' bytes of data from mbuf chain starting at 'off' bytes
 * to destination address 'cp', and return the number of bytes copied.
 */
int
m_datacopy(struct mbuf *m, int off, int maxlen, caddr_t cp)
{
	int count, done;

	ASSERT(off >= 0 && maxlen >= 0);

	GOODMP(m);
	GOODMT(m->m_type);

	done = 0;
	while (off > 0) {
		if (m == 0) {
			return done;
		}

		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}

	while (maxlen > 0) {
		if (m == 0) {
			return done;
		}
		count = MIN(m->m_len-off, maxlen);
		if (count != 0) {
			bcopy(mtod(m, char*) + off, cp, count);
			maxlen -= count;
			cp += count;
			done += count;
			off = 0;
		}
		m = m->m_next;
	}
	return done;
}

/*
 * Attempt to page flip an mbuf's data buffer
 * from the kernel's address space into the user's virtual address space.
 * This procedure is called from the receive socket system call code,
 * to flip/remap pages from the kernel's address space into the user's
 * address space, thus avoiding a data copy from kernel buffers to the
 * user's address space.
 *
 * NOTE: The caller MUST has already determined that the lengths and 
 * target address are suitable for this page oriented operation.
 *
 * Parameter Description:
 * m => cluster mbuf containing the address of a data buffer to page flip
 * ubase => Virtual address of user space page to flip
 *
 * Returns:
 * 0 => Failure; 1 => Success
 */
int
m_flip(struct mbuf *m, caddr_t ubase, int do_sync)
{
	caddr_t p;

	GOODMP(m);
	GOODMT(m->m_type);

	if (m_flip_off > 1)
		return (0);

	/*
	 * The mbuf must satisfy the following:
	 * 1. Must be a cluster mbuf AND
	 * 2. Must be one page in size AND
	 * 3. Must not be a page which was obtain via pattach from m_shget AND
	 * 4. Must have a single user (ie reference count of one).
	 * Otherwise we return a failure indication.
	 */
	if (!(M_HASCL(m)) || (m->m_size != NBPP) || (m->m_flags & (M_SHARED|M_LOANED)))
		return (0);

	/*
	 * fail if more than one user is using the cluster
	 */
	if ((*m->m_refp > 1) || (m->m_refp != &m->m_ref))
		return (0);

	/*
	 * pflip takes a kernel vaddr and user virtual address and returns
	 * the virtual address of the target OR zero on failure. On sucess
	 * the kernel page has become part of the user's address space and
	 * the page that may have been there before has been released.
	 *
	 * Upon return the kernel page that was under the cluster mbuf has
	 * been replaced and a new one returned to us.
	 */
	if ((p = pflip(m->m_p, ubase, do_sync)) == NULL)
		return (0);

	ASSERT((kvatopfdat(p)->pf_flags & (P_HASH|P_ANON)) == 0);

	/* keep the page */
	m->m_p = p;

	return (1);
}

/*
 * Called from soreceive() and sosend() to synchronize the tlb's after
 * successful calls to m_flip()/m_attach() without do_sync set. m_sync()
 * must be called before any flip'd mbufs are freed, and before the kernel
 * tries to examine the contents of any attach'd mbufs. soreceive() also
 * passes in a list of mbufs which are to be freed after the tlbs are sync'd.
 *
 * Parameter Description:
 * pages => array of pointers to mbufs to be freed, or NULL
 * count => number of mbuf pointers in the array
 *
 * Returns:
 * void
 */

void
m_sync(struct mbuf *m)
{
	psync();

	while (m) {
		MFREE(m, m);
	}
}

/*
 * Allocate and clear a regular mbuf.
 */
struct mbuf *
m_getclr(int canwait, int type)
{
	struct mbuf *m;

	if (m = m_get(canwait, type))
		bzero(mtod(m, caddr_t), MLEN);

	return (m);
}

/*
 * Take a user virtual address and a length, and convert them into
 * 'shared cluster'.  Such clusters are user address space pages which
 * have been marked Copy-on-Write.
 */
struct mbuf*
m_shget(int canwait, int type, caddr_t vaddr, int size, int do_sync)
{
	struct mbuf *m;
	caddr_t p;

	GOODMT(type);
	ASSERT(size >= 0 && size <= NBPP);

	if (m_shget_off)
		return (NULL);

	if ((m = m_get(canwait, type)) == NULL)
		return (NULL);

	/*
	 * Now map into the kernel's virtual memory the address supplied by
	 * the user from there virtual address space.
	 */
	if ((p = pattach(vaddr, do_sync)) == NULL) { /* failed */
		m_free(m);
		return (NULL);
	}

	m->m_off = (((__psunsigned_t)p - (__psunsigned_t)m) +
					((__psunsigned_t)vaddr % NBPP));

	m->m_len = size;
	m->m_size = size;
	m->m_p = p;
	m->m_flags = (M_CLUSTER | M_SHARED);
	m->m_type = type;

	m->m_freefunc = (void(*)()) NULL;
	m->m_farg = 0;
	m->m_dupfunc = (void(*)()) NULL;
	m->m_darg = 0;

	m->m_refp = &m->m_ref;
	m->m_ref = 1;

	MBUF_UTRACE(UTN('m_sh','get '), m, __return_address);

	return (m);
}

/*
 * Return the length in bytes of an mbuf chain.
 */
int
m_length(struct mbuf *m)
{
	int len = 0;

	for ( ; m; m = m->m_next) {
		GOODMP(m);
		GOODMT(m->m_type);
		len += m->m_len;
	}

	return len;
}

/*
 * Rearrange an mbuf chain so that len bytes are contiguous and in
 * the data area of a regular mbuf. This is required so that the mtod
 * macros will return a pointer to an item that is large enough.
 *
 * If there is room, it will add up to MPULL_EXTRA bytes to the contiguous
 * region in an attempt to avoid being called next time.
 *
 * Return the possibly modified mbuf chain on success.
 * Return NULL and free the entire mbuf chain on failure.
 */
struct mbuf*
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count, space;

	GOODMP(n);
	GOODMT(n->m_type);
	ASSERT(len > 0);

	if (!(M_HASCL(n)) && n->m_len >= len) {
		/*
		 * No need to be called in the first place.
		 */
		return (n);
	}

	if (!(M_HASCL(n)) && n->m_off + len <= MMAXOFF && n->m_next) {
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MLEN)
			goto bad;

		m = m_get(M_DONTWAIT, n->m_type);
		if (m == 0)
			goto bad;

		m->m_len = 0;

		/* copy only the link-level flag information */
		m->m_flags = n->m_flags & M_COPYFLAGS;
	}
	space = MMAXOFF - m->m_off;

	for (;;) {
		count = MIN(MIN(space - m->m_len, len+MPULL_EXTRA), n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t)+m->m_len, count);

		len -= count;
		m->m_len += count;
		n->m_len -= count;

		if (n->m_len != 0)
			n->m_off += count;
		else
			n = m_free(n);
		if (len <= 0)
			break;

		if (n == 0) {
			(void)m_free(m);
			goto bad;
		}
	}
	m->m_next = n;
	return (m);
bad:
	m_freem(n);
	return (0);
}

/*
 * Return TRUE if there are at least 'len' bytes of room available
 * at the front of the mbuf, else return FALSE.
 */
int
m_hasroom(struct mbuf *m, int len)
{
	int flags;
	int room;

	ASSERT(len >= 0);
	GOODMT(m->m_type);

	flags = m->m_flags;
	room = 0;

	if ((flags & M_CLUSTER) == 0)
		room = m->m_off - MMINOFF;
	else if (((flags & (M_LOANED|M_SHARED)) == 0) && (*m->m_refp == 1))
		room = mtod(m, caddr_t) - m->m_p;

	return (len <= room);
}

#ifdef EVEREST
/*
 * Bug 323277 - Challenge IO4 IA chip bug workaround.
 * Flush IA partial cache line by bringing data into processor cache.
 */
void
m_io4iawar(m)
	struct mbuf *m;
{
	volatile char c;

	for (; m; m = m->m_next)
		if (m->m_len > 0)
			c = *(mtod(m, char*) + (m->m_len - 1));
}
#endif

void
m_reclaim(void)
{
	if (mbuf_reclaim_needed) {
		/* Help is on the way */
		return;
	}
	atomicAddUint(&mbuf_reclaim_needed, 1);
}

/*
 * Shake routine called from the kernel heap manager when
 * memory is running low.  Flush all the mbuf caches.
 */
/* ARGSUSED */
int
m_shake(int level)
{
	struct mb *mb;
	int oldmbpages;
	int pages = 0;
	int i;

	atomicAddUint(&mbshake, 1);

	oldmbpages = mbpages;

	for (i = 0; i < MBNUM; i++) {
		mb = mbcache[i];
		if (mb) {
			(void) mbflush(mb, &mb->mb_big, NULL);
			(void) mbflush(mb, &mb->mb_med, mb->mb_medzone);
			(void) mbflush(mb, &mb->mb_sml, mb->mb_smlzone);
		}
	}

	/* recalculate mbpages MP safe? */
	for (i = 0; i < MBNUM; i++) {
		if (mbcache[i]) {
			pages += mbcache[i]->mb_smlzone->zone_total_pages
				+ mbcache[i]->mb_medzone->zone_total_pages
				+ mbcache[i]->mb_bigtot;
		}
	}
	mbpages = pages;

	/*
	 * Wake up everybody. Even if no full pages were given back,
	 * some might have been flushed into the zone.
	 */
	for (i = 0; i < MBNUM; i++) {
		int s;

		mb = mbcache[i];
		if (mb) {
			s = mutex_spinlock(&mb->mb_lock);
			if (mb->mb_needwake) {
				mb->mb_needwake = 0;
				sv_broadcast(&mb->mb_sv);
			}
			mutex_spinunlock(&mb->mb_lock, s);
		}
	}

	return (mbpages - oldmbpages);
}

extern int mbretain;

/*
 * Flush the designated mbuf cache freelist
 * and return the number of pages freed.
 */
int
mbflush(struct mb *mb, struct mbuf **mlistp, zone_t *z)
{
	int s;
	struct mbuf *m, *mlist;
	int old, new;
	int smlfree = 0, bigfree = 0, medfree = 0;

	s = mutex_spinlock(&mb->mb_lock);

	mlist = *mlistp;

	/* 
	 * systune(1M) should force 'mbretain' to be > zero,
	 * but handle it being zero just in case.
	 */
	if (mlist && mbretain) {
		int i;
		struct mbuf *mlast = NULL;
		/*
		 * Always keep 'mbretain' mbufs around to avoid
		 * thrashing and deadlocks when memory is low.
		 */
		for (i = 0; i < mbretain; i++) {
			if (NULL == mlist) {
				break;
			}
			mlast = mlist;
			mlist = mlist->m_next;
		}
		if (mlast) {
			mlast->m_next = NULL;
		}
	}

	mutex_spinunlock(&mb->mb_lock, s);

	old = z ? z->zone_total_pages: m_count(mlist);

	while (m = mlist) {
		if (z == 0) {
			bigfree++;
		} else {
			if (z == mb->mb_smlzone) {
				smlfree++;
			} else {
				medfree++;
			}
		}
		mlist = mlist->m_next;
		m_destruct(m, mb);
	}

	/* need to take lock again to ensure stats correct */
	s = mutex_spinlock(&mb->mb_lock);
	mb->mb_smlfree -= smlfree;	/* destroyed this many small */
	mb->mb_medfree -= medfree;	/* destroyed this many medium */
	mb->mb_bigfree -= bigfree;	/* destroyed this many large */
	mutex_spinunlock(&mb->mb_lock, s);

	if (z)
		zone_shake(z);

	new = z ? z->zone_total_pages: 0;

	return (old - new);
}

/*
 * Periodically flush one mbuf cache list at a time.
 */
void
mbscavenge()
{
	static int cache_num = 0;
	static int whichzone = 2;
	struct mb *mb;
	int npgs;
	int s;

	mb = mbcache[cache_num];

	if (mb) {
		switch (whichzone) {
		case 0:
			npgs = mbflush(mb, &mb->mb_sml, mb->mb_smlzone);
			break;

		case 1:
			npgs = mbflush(mb, &mb->mb_med, mb->mb_medzone);
			break;

		case 2:
			npgs = mbflush(mb, &mb->mb_big, NULL);
			break;
		}

		atomicAddUint(&mbpages, -npgs);
	}

	/* walk mb's in order, flush zones largest to smallest */
	if (--whichzone < 0) {
		cache_num = (cache_num + 1) % MBNUM;
		whichzone = 2;
	}

	if (mb) {
		s = mutex_spinlock(&mb->mb_lock);
		if (mb->mb_needwake) {
			mb->mb_needwake = 0;
			sv_broadcast(&mb->mb_sv);
		}
		mutex_spinunlock(&mb->mb_lock, s);
	}

	timeout(mbscavenge, 0, mbtimeout * HZ);
}

/*
 * Return the constant values used by the mbuf system.
 */
int
get_mbufconst(void *user_address, int insize, int *outsize)
{
	int error = 0;
	int size = MIN(insize, sizeof(_mbufconst));
	
	error = copyout(&_mbufconst, user_address, size);
	if (outsize) {
		error = copyout(&size, outsize, sizeof(int));
	}
	return error;
}

/*
 * Obtain a snapshot of the number of regular and cluster mbufs
 * in use by the system and return in the mbstat structure passed in.
 */
void
m_allocstats(struct mbstat *mbstatp)
{
	struct mb *mb;
	int bytesfree;
	int i, j;
	int s;

	bzero((caddr_t) mbstatp, sizeof (struct mbstat));
	bytesfree = 0;

	/*
	 * The netstat command assumes that m_clusters and m_clfree count
	 * the number of *pages* inuse and free, respectively.
	 */

	for (i = 0; i < MBNUM; i++) {
		mb = mbcache[i];
		if (mb) {
			s = mutex_spinlock(&mb->mb_lock);

			bytesfree += mb->mb_smlfree * MSIZE;
			bytesfree += mb->mb_medfree * (MSIZE + MBUFCL_SIZE);
			bytesfree += mb->mb_bigfree * (MSIZE + MCLBYTES);
			bytesfree += kmem_zone_freemem(mb->mb_smlzone);
			bytesfree += kmem_zone_freemem(mb->mb_medzone);

			mbstatp->m_mbufs += (mb->mb_smltot + mb->mb_medtot + mb->mb_bigtot);

			mbstatp->m_pcbtot += mb->mb_pcbtot;
			mbstatp->m_pcbbytes += mb->mb_pcbbytes;
			mbstatp->m_mcbtot += mb->mb_mcbtot;
			mbstatp->m_mcbfail += mb->mb_mcbfail;
			mbstatp->m_mcbbytes += mb->mb_mcbbytes;
			mbstatp->m_drops += mb->mb_drops;
			mbstatp->m_wait += mb->mb_wait;

			mb->mb_types[MT_FREE] = mb->mb_smlfree + 
				mb->mb_medfree + mb->mb_bigfree;

			for (j = 0; j < MT_MAX; j++)
				mbstatp->m_mtypes[j] += mb->mb_types[j];

			mutex_spinunlock(&mb->mb_lock, s);
		}
	}

	mbstatp->m_clusters = mbpages;
	mbstatp->m_clfree = bytesfree/NBPC;
	mbstatp->m_drain = mbdrain;
}

/*
 * Return the number of mbufs in an mbuf chain.
 */
int
m_count(struct mbuf *m)
{
	int n = 0;

	for ( ; m; m = m->m_next)
		n++;
	return (n);
}

/* accounting support for external MT_PCB allocator */
void
m_pcbinc(int nbytes, int type)
{
	struct mb *mb;

	mb = mbcache[MBINDEX()];
	MBTYPE_INC(mb, type);
	atomicAddInt(&mb->mb_pcbtot, 1);
	atomicAddInt64(&mb->mb_pcbbytes, nbytes);
}

/* accounting support for external MT_PCB allocator */
void
m_pcbdec(int nbytes, int type)
{
	struct mb *mb;

	mb = mbcache[MBINDEX()];
	MBTYPE_DEC(mb, type);
	atomicAddInt(&mb->mb_pcbtot, -1);
	atomicAddInt64(&mb->mb_pcbbytes, -nbytes);
}

/* accounting support for external MT_RTABLE allocator */
void
m_rtinc(int nbytes)
{
	struct mb *mb;

	mb = mbcache[MBINDEX()];
	MBTYPE_INC(mb, MT_RTABLE);
	atomicAddInt(&mb->mb_mcbtot, 1);
	atomicAddInt64(&mb->mb_mcbbytes, nbytes);
}

/* accounting support for external MT_RTABLE allocator */
void
m_rtdec(int nbytes)
{
	struct mb *mb;

	mb = mbcache[MBINDEX()];
	MBTYPE_DEC(mb, MT_RTABLE);
	atomicAddInt(&mb->mb_mcbtot, -1);
	atomicAddInt64(&mb->mb_mcbbytes, -nbytes);
}

#if defined(DEBUG) || defined(DEBUG_ALLOC)
static void
m_poison(struct mbuf *m, inst_t *ra)
{
	int size;
	void *buf;

	if (M_HASCL(m)) {
		buf = m->m_p;
		size = m->m_size;
	} else {
		buf = m->m_dat;
		size = MLEN;
	}

	ASSERT((size & (sizeof(__psunsigned_t)-1)) == 0);
	kmem_poison(buf, size, ra);
}
#endif
