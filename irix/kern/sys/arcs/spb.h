/* ARCS system parameter block code
 */

#ifndef _ARCS_SPB_H
#define _ARCS_SPB_H

#ident "$Revision: 1.20 $"

#if _MIPS_SIM == _ABI64
#define _ARCS_K0BASE	0xa800000000000000
#define _ARCS_K1BASE	0x9000000000000000
#else
#define _ARCS_K0BASE	0x80000000
#define _ARCS_K1BASE	0xa0000000
#endif

#define SPBADDR		(0x00001000)

#if	defined(_LANGUAGE_C)
#define	_ARCS_PHYS_TO_K0(x)	((__psunsigned_t)(x) | _ARCS_K0BASE)
#define	_ARCS_PHYS_TO_K1(x)	((__psunsigned_t)(x) | _ARCS_K1BASE)
#else
#define	_ARCS_PHYS_TO_K0(x)	((x) | _ARCS_K0BASE)
#define	_ARCS_PHYS_TO_K1(x)	((x) | _ARCS_K1BASE)
#endif

#define K0_SPBADDR	_ARCS_PHYS_TO_K0(SPBADDR)

#define _NBPL		(_MIPS_SZLONG/8)	/* Number of Bytes Per Long */
#define _NBPS		2			/* Number of Bytes Per Short */

/*
 * _SPB_PAD accounts for the fact that in 64-bit mode the compiler
 * has to add padding between the shorts (Version and Revision) and
 * the longs. Yuck.
 */
#if _MIPS_SZLONG == 64
#define _SPB_PAD	4
#endif

#if _MIPS_SZLONG == 32
#define _SPB_PAD	0
#endif

/* location of debug block pointer */
#define SPB_DEBUGADDR	(K0_SPBADDR + (_NBPL * 3) + (_NBPS * 2) + _SPB_PAD)
#define SPB_GEVECTOR	(K0_SPBADDR + (_NBPL * 4) + (_NBPS * 2) + _SPB_PAD)
#define SPB_UTLBVECTOR	(K0_SPBADDR + (_NBPL * 5) + (_NBPS * 2) + _SPB_PAD)

#if	defined(_LANGUAGE_C)

#include <arcs/tvectors.h>

/*  ARCS system parameter block sits at a well known location (SPBADDR),
 * and provides a place to get system information.
 */
typedef struct {
	LONG 		Signature;			/* SPBMAGIC */
	ULONG		Length;
	USHORT		Version;
	USHORT		Revision;
	LONG		*RestartBlock;
	LONG		*DebugBlock;
	LONG		*GEVector;
	LONG		*UTLBMissVector;
	ULONG		TVLength;
	FirmwareVector	*TransferVector;
	ULONG		PTVLength;
	LONG		*PrivateVector;
	LONG		AdapterCount;
	LONG		AdapterType0;
	LONG		AdapterVectorCount0;
	LONG		*AdapterVector0;
	LONG		AdapterType1;
	LONG		AdapterVectorCount1;
	LONG		*AdapterVector1;
	/* more adapter vectors up to 0x2000 (end of page) */
} SystemParameterBlock;
typedef SystemParameterBlock spb_t;

#define SPBMAGIC	0x53435241

#if __USE_SPB32 || (_K64PROM32 && !_STANDALONE)
/* a non-scaling 32-bit version of the SPB. */
typedef struct {
	__int32_t 		Signature;			/* SPBMAGIC */
	__uint32_t		Length;
	USHORT		Version;
	USHORT		Revision;
	__int32_t	RestartBlock;
	__int32_t	DebugBlock;
	__int32_t	GEVector;
	__int32_t	UTLBMissVector;
	__uint32_t	TVLength;
	__int32_t	TransferVector;
	__uint32_t	PTVLength;
	__int32_t	PrivateVector;
	__int32_t	AdapterCount;
	__int32_t	AdapterType0;
	__int32_t	AdapterVectorCount0;
	__int32_t	AdapterVector0;
	__int32_t	AdapterType1;
	__int32_t	AdapterVectorCount1;
	__int32_t	AdapterVector1;
	/* more adapter vectors up to 0x2000 (end of page) */
} SystemParameterBlock32;

typedef SystemParameterBlock32 spb32_t;
#define SPB32	((spb32_t *)_ARCS_PHYS_TO_K1(SPBADDR))
#endif

#if defined(_K64PROM32) && !defined(_STANDALONE)

#define SPB	((spb_t *)_ARCS_PHYS_TO_K1(SPBADDR))

/* Transfer vector shorthand */
#define __TV	((FirmwareVector32 *)(SPB->TransferVector))
#define __PTV	((PrivateVector32 *)(SPB->PrivateVector))

#else /* _K64PROM32 && !defined(_STANDALONE) */

#ifndef IPXX
#if _MIPS_SIM == _ABI64 && !defined(_K64PROM32)
#define SPB   ((spb_t *)K0_SPBADDR)
#else
#define SPB   ((spb_t *)_ARCS_PHYS_TO_K1(SPBADDR))
#endif
#else
extern spb_t *get_spb(void);
#define	SPB	get_spb()
#endif

/* Transfer vector shorthand */
#define __TV	SPB->TransferVector
#define __PTV	((PrivateVector *)SPB->PrivateVector)

#endif /* _K64PROM32 && !defined(_STANDALONE) */

#define GDEBUGBLOCK		SPB->DebugBlock
#define SDEBUGBLOCK(new)	SPB->DebugBlock = (LONG *)new;

#endif	/* _LANGUAGE_C */

#endif /* _ARCS_SPB_H */
