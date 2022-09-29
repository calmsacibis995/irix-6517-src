
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
#if _K64U64

#define	KUBASE		0
#define	KUSIZE_32	0x0000000080000000	/* KUSIZE for a 32 bit proc */
#define	K0BASE		0xa800000000000000
#define	K1BASE		0x9000000000000000
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
#define	KUSIZE		0x0000100000000000		/* 2^^44 */
#define	KUSIZE_64	0x0000100000000000		/* 2^^44 */
#define	K0SIZE		0x0000010000000000		/* 2^^40 */
#define	K1SIZE		0x0000010000000000		/* 2^^40 */
#define	K2SIZE		0x00000fff80000000
#define	KSEGSIZE	0x00000fff80000000		/* max syssegsz */
#define TO_PHYS_MASK	0x000000ffffffffff		/* 2^^40 - 1 */
#endif

#define KREGION_MASK	0xc000000000000000	/* Mask of region bits */
#define	COMPAT_K0BASE	0xffffffff80000000	/* Compatibility space */
#define	COMPAT_K1BASE	0xffffffffa0000000	/* Compatibility space */

#else	/* _K32U32 || _K32U64 */
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
#endif	/* _K32U32 || _K32U64 */

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
#if _K32U32 || _K32U64

#ifdef K0_TO_K1
#undef	K0_TO_K1
#endif
#ifdef	K1_TO_K0
#undef 	K1_TO_K0
#endif
#ifdef	K0_TO_PHYS
#undef 	K0_TO_PHYS
#endif
#ifdef	K1_TO_PHYS
#undef 	K1_TO_PHYS
#endif
#ifdef	KDM_TO_PHYS
#undef 	KDM_TO_PHYS
#endif
#ifdef	PHYS_TO_K0
#undef 	PHYS_TO_K0
#endif
#ifdef	PHYS_TO_K1
#undef 	PHYS_TO_K1
#endif


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

#if defined(EVEREST) && !defined(_STANDALONE)
/*
 * Fake out uncached access.  On Everest, the only way to get to
 * memory is via coherent access (not K1SEG).  When copying out of
 * DMA'ed buffers, coherent references will work fine, since IO is
 * fully cache-coherent (and in fact, coherent references *must* be
 * used).  On Everest, the *only* code that can create true uncached
 * references is in the ml/EVEREST directory.  Such references are 
 * used only to do PIO operations.
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

#elif _K64U64

#ifdef _LANGUAGE_ASSEMBLY
#define	K0_TO_K1(x)	(((x) & TO_PHYS_MASK) | K1BASE)	/* kseg0 to kseg1 */
#define	K1_TO_K0(x)	(((x) & TO_PHYS_MASK) | K0BASE)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* kseg0 to physical */
#define	K1_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* kseg1 to physical */
#define	KDM_TO_PHYS(x)	((x) & TO_PHYS_MASK)		/* DirectMapped
								 to physical */
#define	PHYS_TO_K0(x)	((x) | K0BASE)			/* physical to kseg0 */
#define	PHYS_TO_K1(x)	((x) | K1BASE)			/* physical to kseg1 */

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
#endif	/* _LANGUAGE_ASSEMBLY */

#endif	/* _K64U64 */

/*
 * This macro defines all valid 64 bit user addresses. Some machines may
 * choose to set KUSEG to a smaller region. For example, TFP software
 * currently only supports 40 address bits for compatibility with R4000.
 */
#if _K64U64
#define IS_KUREGION(x)	(((__psunsigned_t)(x) & KREGION_MASK) == 0)
#else
#define IS_KUREGION(x)	IS_KUSEG(x)
#endif

/*
 * Address predicates
 */
#ifdef	IS_KSEG0
#undef 	IS_KSEG0
#endif
#ifdef	IS_KSEG1
#undef 	IS_KSEG1
#endif
#ifdef	IS_KSEGDM
#undef 	IS_KSEGDM
#endif
#ifdef	IS_KPTESEG
#undef 	IS_KPTESEG
#endif
#ifdef	IS_KUSEG
#undef 	IS_KUSEG
#endif
#ifdef	IS_KUSEG32
#undef 	IS_KUSEG32
#endif
#ifdef	IS_KUSEG64
#undef 	IS_KUSEG64
#endif
#ifdef	IS_KSEG2
#undef 	IS_KSEG2
#endif


#define	IS_KSEG0(x)		((__psunsigned_t)(x) >= K0BASE &&	\
				 (__psunsigned_t)(x) < K0BASE + K0SIZE)
#define	IS_KSEG1(x)		((__psunsigned_t)(x) >= K1BASE &&	\
				 (__psunsigned_t)(x) < K1BASE + K1SIZE)
#define	IS_KSEGDM(x)		(IS_KSEG0(x) || IS_KSEG1(x))
#define	IS_KPTESEG(x)		((__psunsigned_t)(x) >= KPTE_SHDUBASE && \
				 (__psunsigned_t)(x) < KPTEBASE + KPTE_USIZE)
#define	IS_KUSEG(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE)
#define	IS_KUSEG32(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE_32)
#define	IS_KUSEG64(x)		((__psunsigned_t)(x) < KUBASE + KUSIZE_64)
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
