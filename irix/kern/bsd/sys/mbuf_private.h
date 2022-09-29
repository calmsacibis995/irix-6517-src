#ifndef __SYS_MBUFPRIVATE_H__
#define __SYS_MBUFPRIVATE_H__
/*
 * Copyright (C) 1997 Silicon Graphics, Inc. All rights reserved.
 *
 * Private mbuf allocatr related constants and definitions.
 * This allows for "private" symbols and the definition of the 'struct mb'
 * to be shared with the bsdidbg.c code and icrash any others without the
 * consequences of changing the public mbuf.h header file.
 *
 * mbuf cluster field shorthands
 * NOTE: Some of these conflict with other field names in other header files
 * so including this file requires caution with respect to name conflicts.
 */
#define	m_p		m_u.m_us.mu_p
#define m_refp		m_u.m_us.mu_refp
#define m_ref		m_u.m_us.mu_ref
#define m_size		m_u.m_us.mu_size

/*
 * One mbcache structure per cpu is allocated
 * index mbcache array using cpu number to find associated mbcache structure
 */
#if defined(SN)
#include <sys/SN/arch.h>
#define MBMAXCPU        MAXCPUS
#else
#define MBMAXCPU        256
#endif
#define	GOODVA(index, va)

#if SN0
/*
 * multi-processor definitions
 */
#define	MBINDEX()	(cpuid())
#define	MBCNODEID(i)	(cputocnode(i))
/*
 * use maxcpus since kernel sometimes boots with "holes" in cpuid() space
 * we check if the cpu has been enabled on initialization
 */
#define	MBNUM		(maxcpus)

#elif EVEREST

#ifdef MULTIKERNEL
/*
 * MULTIKERNEL systems have "holes" in the cpuid() space
 * we check if the cpu has been enabled on initialization
 */
#define	MBINDEX()	(cpuid()/evmk.numcells)
#else
#define	MBINDEX()	(cpuid())
#endif /* MULTIKERNEL */

#define	MBCNODEID(i)	(cputocnode(i))
#define	MBNUM		(maxcpus)
#else
/*
 * uniprocessor definitions
 */
#define	MBINDEX()	0
#define	MBCNODEID(i)	0
#define	MBNUM		1
#endif /* SN0 */

/*
 * mbuflist head structure definition
 */
struct mb {
	lock_t  mb_lock;		/* mb spinlock */
	struct mbuf *mb_sml;		/* MSIZE regular mbufs */
	struct mbuf *mb_med;		/* MBUFCL_SIZE cluster mbufs */

	uint    mb_needwake;		/* anyone waiting? */
	uint	mb_smlfree;		/* free sml mbuf */
	uint	mb_medfree;		/* free med mbuf */
	int	mb_types[MT_MAX];	/* type specific mbuf allocations */

	zone_t *mb_smlzone;		/* small list private zone handle */

		/* new cache line for 128 byte per cache line systems */
	zone_t *mb_medzone;		/* medium list private zone handle */
	struct mbuf *mb_big;		/* PAGE size NBPP cluster mbufs */

	uint	mb_bigfree;		/* free big mbuf */
	uint	mb_smltot;		/* total # small mbufs alloc zone */
	uint	mb_medtot;		/* total # med mbufs alloc from zone */
	uint	mb_bigtot;		/* total # big mbufs alloc from zone */
	uint	mb_drops;		/* total number allocation failures */
	uint	mb_wait;		/* total number times slept */
	int     mb_index;		/* index of this mb */
	sv_t	mb_sv;			/* what we wait on */

	int	mb_mcbtot;		/* total mcb_get() */
	__int64_t	mb_mcbbytes;	/* mcb_get() byte count */
	int	mb_pcbtot;		/* total m_pcbinc() */
	__int64_t	mb_pcbbytes;	/* m_pcbinc() bytes */
	int	mb_mcbfail;		/* failures of memory allocation */
};

#ifdef MP
typedef struct mb mb_t;
#pragma set type attribute mb_t align=128
#endif /* MP */

#endif /* __SYS_MBUFPRIVATE_H__ */

