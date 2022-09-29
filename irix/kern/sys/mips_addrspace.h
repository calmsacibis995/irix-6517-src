
#ifndef __SYS_MIPS_ADDRSPACE_H
#define __SYS_MIPS_ADDRSPACE_H

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Segment base addresses and sizes
 */
#if _MIPS_SIM == _ABI64

#define	KUBASE		0
#define	KUSIZE_32	0x0000000080000000	/* KUSIZE for a 32 bit proc */
#define	K0BASE		0xa800000000000000
#define	K0BASE_EXL_WR	K0BASE			/* exclusive on write */
#define K0BASE_NONCOH	0x9800000000000000	/* noncoherent */
#define K0BASE_EXL	0xa000000000000000	/* exclusive */

#ifdef SN0
#define K1BASE		0x9600000000000000	/* Uncached attr 3, uncac */
#else
#define	K1BASE		0x9000000000000000
#endif
#define	K2BASE		0xc000000000000000

#if R4000
#define	KUSIZE		0x0000010000000000		/* 2^^40 */
#define	KUSIZE_64	0x0000010000000000		/* 2^^40 */
#define	K0SIZE		0x0000001000000000		/* 2^^36 */
#define	K1SIZE		0x0000001000000000		/* 2^^36 */
#define	K2SIZE		0x000000ff80000000
#define	KSEGSIZE	0x000000ff80000000		/* max syssegsz */
#define TO_PHYS_MASK	0x0000000fffffffff		/* 2^^36 - 1 */
#elif TFP
/* We keep KUSIZE consistent with R4000 for now (2^^40) instead of (2^^48) */
#define	KUSIZE		0x0000010000000000		/* 2^^40 */
#define	KUSIZE_64	0x0000010000000000		/* 2^^40 */
#define	K0SIZE		0x0000010000000000		/* 2^^40 */
#define	K1SIZE		0x0000010000000000		/* 2^^40 */
#define	K2SIZE		0x0001000000000000
#define	KSEGSIZE	0x0000010000000000		/* max syssegsz */
#define TO_PHYS_MASK	0x000000ffffffffff		/* 2^^40 - 1 */
#elif R10000
#if defined(SN0XXLnotyet) || defined(SN1)
#define	KUSIZE		0x0000100000000000		/* 2^^44 */
#define	KUSIZE_64	0x0000100000000000		/* 2^^44 */
#else
#define	KUSIZE		0x0000010000000000		/* 2^^40 */
#define	KUSIZE_64	0x0000010000000000		/* 2^^40 */
#endif /* SN0XXL */
#define	K0SIZE		0x0000010000000000		/* 2^^40 */
#define	K1SIZE		0x0000010000000000		/* 2^^40 */
#define	K2SIZE		0x00000fff80000000
#define	KSEGSIZE	0x00000fff80000000		/* max syssegsz */
#define TO_PHYS_MASK	0x000000ffffffffff		/* 2^^40 - 1 */
#elif BEAST
/*
 * For now, we keep the virtual address space the same as other 64 bits
 * architectures even though beast supports larger space. Only the physical
 * spaces are bumped up now.
 */
#define	KUSIZE		0x0000010000000000		/* 2^^40 */
#define	KUSIZE_64	0x0000010000000000		/* 2^^40 */
#define	K0SIZE		0x0000100000000000		/* 2^^44 */
#define	K1SIZE		0x0000100000000000		/* 2^^44 */
#define	K2SIZE		0x00000fff80000000
#define	KSEGSIZE	0x00000fff80000000		/* max syssegsz */
#define TO_PHYS_MASK	0x00000fffffffffff		/* 2^^44 - 1 */
#endif

#define KREGION_MASK	0xc000000000000000	/* Mask of region bits */
#define	COMPAT_K0BASE	0xffffffff80000000	/* Compatibility space */
#define	COMPAT_K1BASE	0xffffffffa0000000	/* Compatibility space */
#define COMPAT_KSIZE	0x20000000
#define TO_COMPAT_PHYS_MASK	0x1fffffff	

#ifndef TFP					/* TFP lacks compat space */
#define COMPAT_K0BASE32	0xffffffff80000000	/* sign extended 0x80000000 */
#define COMPAT_K1BASE32	0xffffffffa0000000	/* sign extended 0xa0000000 */
#endif

#else	/* !_ABI64 */
#define	KUBASE		0
#define	KUSIZE		0x80000000
#define	KUSIZE_32	0x80000000
#define	KUSIZE_64	0x80000000
#define	K0BASE		0x80000000
#define	K0SIZE		0x20000000
#define	K1BASE		0xA0000000
#define	K1SIZE		0x20000000
#define	K2BASE		0xC0000000
#if EVEREST
#define	KSEGSIZE	0x18000000			/* max syssegsz */
#else
#define	KSEGSIZE	0x10000000			/* max syssegsz */
#endif
#define	K2SIZE		0x20000000
#define TO_K0_MASK	0x9fffffff
#define TO_PHYS_MASK	0x1fffffff
#define	COMPAT_K0BASE	K0BASE		/* Compatibility space */
#define	COMPAT_K1BASE	K1BASE		/* Compatibility space */
#endif	/* !_ABI64 */

#if _MIPS_SIM == _ABIN32
#define SEXT_K0BASE	_S_EXT_(K0BASE)
#define SEXT_K1BASE	_S_EXT_(K1BASE)
#else
#define SEXT_K0BASE	K0BASE
#define SEXT_K1BASE	K1BASE
#endif /* _MIPS_SIM */


/*
 * Note that the system dynamically-allocated segment know as "sysseg"
 * always starts at K2BASE.  Its actual size is tuneable, and configured
 * at bootup in mktables.  The maximum possible sysseg size is the
 * system-dependent "KSEGSIZE" defined here.  This value should reflect
 * the system-specific kernel virtual address mappings.
 */

/*
 * Address conversion macros
 */
#if _MIPS_SIM != _ABI64

#ifdef _LANGUAGE_ASSEMBLY
#define	K0_TO_K1(x)	((x) | K1BASE)		/* kseg0 to kseg1 */
#define	K1_TO_K0(x)	((x) & TO_K0_MASK)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((x) & TO_PHYS_MASK)	/* kseg0 to physical */
#define	K1_TO_PHYS(x)	((x) & TO_PHYS_MASK)	/* kseg1 to physical */
#define	KDM_TO_PHYS(x)	((x) & TO_PHYS_MASK)	/* DirectMapped to physical */
#define	PHYS_TO_K0(x)	((x) | K0BASE)		/* physical to kseg0 */
#if !defined(EVEREST) || defined(_STANDALONE)
#define	PHYS_TO_K1(x)	((x) | K1BASE)		/* physical to kseg1 */
#endif
#else /* _LANGUAGE_C */

#if (defined(EVEREST) || defined(SN0)) && !defined(_STANDALONE)
/*
 * Fake out uncached access.  On Everest, the only way to get to
 * memory is via coherent access (not K1SEG).  When copying out of
 * DMA'ed buffers, coherent references will work fine, since IO is
 * fully cache-coherent (and in fact, coherent references *must* be
 * used).  On Everest, the *only* code that can create true uncached
 * references is in the ml/EVEREST directory.  Such references are 
 * used only to do PIO operations.  All of this also applies to SN0.
 */
#define	K0_TO_K1(x)	((__psunsigned_t)(x) | K0BASE)
#define	PHYS_TO_K1(x)	((__psunsigned_t)(x) | K0BASE)
#else
#define	K0_TO_K1(x)	((__psunsigned_t)(x) | K1BASE)	/* kseg0 to kseg1 */
#define	PHYS_TO_K1(x)	((__psunsigned_t)(x) | K1BASE)	/* physical to kseg1 */
#endif /* EVEREST */
						/* kseg1 to kseg0 */
#define	K1_TO_K0(x)	((__psunsigned_t)(x) & TO_K0_MASK)
						/* kseg0 to physical */
#define	K0_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* kseg1 to physical */
#define	K1_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* DirectMapped to physical */
#define	KDM_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* physical to kseg0 */
#define	PHYS_TO_K0(x)	((__psunsigned_t)(x) | K0BASE)
#endif	/* LANGUAGE_C */

#define	PHYS_TO_COMPATK0(x) PHYS_TO_K0(x)	/* compat compatability */
#define	PHYS_TO_COMPATK1(x) PHYS_TO_K1(x)	/* compat compatability */

#elif _MIPS_SIM == _ABI64

#ifdef _LANGUAGE_ASSEMBLY
#define	K0_TO_K1(x)	(((x) & TO_PHYS_MASK) | K1BASE)	/* kseg0 to kseg1 */
#define	K1_TO_K0(x)	(((x) & TO_PHYS_MASK) | K0BASE)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* kseg0 to pysical */
#define	K1_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* kseg1 to physical */
#define	KDM_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* DirectMapped
								 to physical */
#define	PHYS_TO_K0(x)	((x) | K0BASE)			/* physical to kseg0 */
#define	PHYS_TO_K1(x)	((x) | K1BASE)			/* physical to kseg1 */

#ifdef COMPAT_K0BASE32
#define	PHYS_TO_COMPATK0(x)	((x) | COMPAT_K0BASE32)	/* 32-bit compat k0 */
#define	PHYS_TO_COMPATK1(x)	((x) | COMPAT_K1BASE32)	/* 32-bit compat k1 */
#endif

#else	/* _LANGUAGE_C */
						/* kseg0 to kseg1 */
#define	K0_TO_K1(x)	(((__psunsigned_t)(x) & TO_PHYS_MASK) | K1BASE)
						/* kseg1 to kseg0 */
#define	K1_TO_K0(x)	(((__psunsigned_t)(x) & TO_PHYS_MASK) | K0BASE)
						/* kseg0 to physical */
#define	K0_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* kseg1 to physical */
#define	K1_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* DirectMapped to physical */
#define	KDM_TO_PHYS(x)	((__psunsigned_t)(x) & TO_PHYS_MASK)
						/* physical to kseg0 */
#define	PHYS_TO_K0(x)	((__psunsigned_t)(x) | K0BASE)
						/* physical to kseg1 */
#define	PHYS_TO_K1(x)	((__psunsigned_t)(x) | K1BASE)

#ifdef COMPAT_K0BASE32		/* 32-bit compat ksegs */
#define	PHYS_TO_COMPATK0(x)	\
	((__psint_t)(__int32_t)((__uint32_t)(x)|COMPAT_K0BASE32))
#define	PHYS_TO_COMPATK1(x)	\
	((__psint_t)(__int32_t)((__uint32_t)(x)|COMPAT_K1BASE32))
#endif
#endif	/* _LANGUAGE_ASSEMBLY */


#endif	/* _ABI64 */

/* systems with no compat space or 32-bit kernels use normal space */
#ifndef COMPAT_K0BASE32	
#define	PHYS_TO_COMPATK0(x)	PHYS_TO_K0(x)
#define	PHYS_TO_COMPATK1(x)	PHYS_TO_K1(x)
#endif


/* This macro casts a pointer to be a signed value, so that if it changes 
 * size, it will be properly sign-extended.  Example:
 *      caddr_t p;
 *      ...
 *      k_machreg_t r = (k_machreg_t)PTR_EXT(p);
 *
 * This is effectively the same as _S_EXT_() but it can be safely used on
 * all pointers, and is efficient with non-constants.
 *   It's only needed for ABIN32 kernel code.
 */

#ifdef _LANGUAGE_C
#if _MIPS_SIM == _ABIN32
#define PTR_EXT(addr)		((__psint_t)(addr))
#else
#define PTR_EXT(addr)		(addr)
#endif
#endif

/*
 * This macro defines all valid 64 bit user addresses. Some machines may
 * choose to set KUSEG to a smaller region. For example, TFP software
 * currently only supports 40 address bits for compatibility with R4000.
 */
#if _MIPS_SIM == _ABI64
#define IS_KUREGION(x)	(((__psunsigned_t)(x) & KREGION_MASK) == 0)
#else
#define IS_KUREGION(x)	IS_KUSEG(x)
#endif

/*
 * R10000 has multiple uncached spaces, identified via bits 57, 58 in
 * the uncached address. Use the following mask to turn off these 
 * bits while checking if an address is in R10000's K1 space.
 * This is in general true for all R10000 systems. But no system other
 * than SN0 seems to be using this facility. So, this is ifdefed
 * SN0 to reduce the additional (unnecessary) checking for other 
 * platforms
 */
#if _MIPS_SIM == _ABI64 && (defined(R10000) && defined(SN0))

#define	K1ATTR_SHFT	57
#define	K1ATTR_MASK	(3LL << K1ATTR_SHFT)
#define	K1MASK(x)	((__psunsigned_t)(x) & ~(K1ATTR_MASK))
#else	/* !_ABI64 && .. */
#define	K1MASK(x)	(x)
#endif	/* _ABI64 && ...*/

#if _MIPS_SIM == _ABI64 && (defined(R10000))
#define UATTR_SHFT	57
#define UATTR_MASK	(3L << UATTR_SHFT)
#define UNCACHED_ATTR(_x) (((__psunsigned_t)(_x) & (UATTR_MASK)) >> UATTR_SHFT)

#define CALGO_SHFT	59
#define CALGO_MASK	(7L << CALGO_SHFT)
#define CACHE_ALGO(_x) (((__psunsigned_t)(_x) & (CALGO_MASK)) >> CALGO_SHFT)
#endif

/*
 * Address predicates
 */

#define IS_KPHYS(x)		!((__psunsigned_t)(x) & ~TO_PHYS_MASK)
#define	IS_KSEG0(x)		((__psunsigned_t)(x) >= K0BASE &&	\
				 (__psunsigned_t)(x) < K0BASE + K0SIZE)
#define	IS_KSEG1(x)		(((__psunsigned_t)(K1MASK(x)) >= K1MASK(K1BASE))\
				&& ((__psunsigned_t)(K1MASK(x)) < K1MASK(K1BASE) + K1SIZE))
#define	IS_KSEGDM(x)		(IS_KSEG0(x) || IS_KSEG1(x))
#define	IS_KPTESEG(x)		((__psunsigned_t)(x) >= KPTE_SHDUBASE && \
				 (__psunsigned_t)(x) < KPTEBASE + KPTE_USIZE)
#define	IS_KUSEG(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE)
#define	IS_KUSEG32(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE_32)
#define	IS_KUSEG64(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE_64)

#if _MIPS_SIM == _ABI64
#define IS_COMPATK0(_x)	((__psunsigned_t)(_x) >= COMPAT_K0BASE &&\
			 (__psunsigned_t)(_x) < (COMPAT_K0BASE + COMPAT_KSIZE))

#define IS_COMPATK1(_x)	((__psunsigned_t)(_x) >= COMPAT_K1BASE &&\
			 (__psunsigned_t)(_x) < (COMPAT_K1BASE + COMPAT_KSIZE))

#define IS_COMPAT_PHYS(_x)  (IS_COMPATK0(_x) || IS_COMPATK1(_x))
#define COMPAT_TO_PHYS(_x)	((__psunsigned_t)(_x) & TO_COMPAT_PHYS_MASK)

#define IS_XKPHYS(_x)  	(((__psunsigned_t)(_x) >= 0x8000000000000000)\
			 && ((__psunsigned_t)(_x) < 0xC000000000000000))

#define XKPHYS_TO_PHYS(_x) 	((__psunsigned_t)(_x) & TO_PHYS_MASK)
#else
/* compat compatability */
#define IS_COMPAT_PHYS(_x)	(0)
#define COMPAT_TO_PHYS(_x)	((__psunsigned_t)(_x) & TO_PHYS_MASK)
#endif

#if TFP
#define	IS_KSEG2(x)		((__psunsigned_t)(x) >= K2BASE && \
				 (__psunsigned_t)(x) < K2BASE + K2SIZE)
#else
#define	IS_KSEG2(x)		((__psunsigned_t)(x) >= K2BASE && \
				 (__psunsigned_t)(x) < KPTE_SHDUBASE)
#endif


#ifdef _STANDALONE
#define	KDM_TO_LOWPHYS(x)	(KDM_TO_PHYS(x))
#endif	/* _STANDALONE */

#endif	/* __SYS_MIPS_ADDRSPACE_H */

