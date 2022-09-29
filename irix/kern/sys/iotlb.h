

#ifndef __SYS_IOTLB_H
#define __SYS_IOTLB_H

#include <sys/mips_addrspace.h>

/*
 * IOMAP TLB Defines.
 */

#if EVEREST && !_STANDALONE
#define IOMAP_VPAGESIZE		0x00400000	/* 4MB iomap page size	*/
#define IOMAP_VPAGESHIFT	22
#define IOMAP_VPAGES		(0x100)		/* 256X4Mb = 1GB	*/
#else /* EVEREST && !_STANDALONE */
#define IOMAP_VPAGESIZE		0x01000000	/* 16MB iomap page size	*/
#define IOMAP_VPAGESHIFT	24
#define IOMAP_VPAGES		(0x40)		/* 64x16MB = 1GB	*/
#endif /* !EVEREST || _STANDALONE */

#define IOMAP_BASE		K2BASE		/* starts from kseg2	*/

#define IOMAP_SHIFTL		2
#define IOMAP_SHIFTR		(IOMAP_SHIFTL+IOMAP_VPAGESHIFT)
#define IOMAP_VPAGEINDEX(a)	(((__psunsigned_t)a-IOMAP_BASE)>>IOMAP_VPAGESHIFT)

#if R4000 || R10000
#define IOMAP_PFNSHIFT		(IOMAP_VPAGESHIFT+1)
#if EVEREST && !_STANDALONE
#define IOMAP_TLBPAGEMASK	0x007FE000	/* 4M phys page mask	*/
#else /* EVEREST && !_STANDALONE*/
#define IOMAP_TLBPAGEMASK	0x01FFE000	/* 16M phys page mask	*/
#endif /* !EVEREST || _STANDALONE */
#endif

#if TFP
#define IOMAP_PFNSHIFT		12		/* 4KB for non-R400	*/
#if EVEREST && !_STANDALONE
#define IOMAP_VPFNMASK		0x003FF000	/* pfn within vpage	*/
#else	/* !EVEREST || _STANDALONE */
#define IOMAP_VPFNMASK		0x00FFF000	/* pfn within vpage	*/
#endif	/* !EVEREST || _STANDALONE */
#define IOMAP_TLBPAGEMASK	IOMAP_VPFNMASK	/* 4M phys page mask	*/
#endif	/* TFP */

#if EVEREST && _LANGUAGE_C
/*
 * iopte is initialized by the kernel to reserve kernel virtual space
 * to be mapped to the io space for all io boards/adaptors in the system
 * no matter it has valid devices on it or not.
 */

#include "sys/immu.h"
struct iopte {
	uint pte_pfn:26,
	pte_cc:3,
	pte_m:1,
	pte_vr:1,
	pte_g:1;
};

extern struct	iopte iotlb [IOMAP_VPAGES]; /* defines the kv to io mapping */
extern int	iotlb_next;		/* defines the upper bound of io map */
#endif	/* EVEREST && LANGUAGE_C */

#endif	/* __SYS_IOTLB_H */
