
/***********************************************************************\
*									*
* evaddrmacros.h-							*
*	Provides code and macros to convert between 64-bit addresses	*
*	and MIPS 1/2 K0 and K1 addresses.  Since we are unable to	*
*	replace all Everest CPU PROMS when we introduce 64-bit Irix,	*
*	we must find a way to keep PROM-generated structures from	*
*	changing size.  This file provides the means for using 32-bit	*
*	pointers (limiting us to 29-bit addresses) in all PROM		*
*	structures (i.e. mpconf, evconfig).  gda.h provides extra	*
*	space for 64-bit pointers.					*
*									*
\***********************************************************************/

#ifndef __EVADDRMACROS_H__
#define __EVADDRMACROS_H__

#if _LANGUAGE_C

/*
 * Turn a kphys or physcial address into a 32-bit K0 (cached, unmapped) address
 */
#define KPHYSTO32K0(_x)		(int)(((__psint_t)(_x) & 0x1fffffff) | (0x80000000))

/*
 * Turn a kphys or physical address into a 32-bit K1 (uncached, unmapped) addr.
 */
#define KPHYSTO32K1(_x)		(int)(((__psint_t)(_x) & 0x1fffffff) | (0xa0000000))

#elif _LANGUAGE_ASSEMBLY

/*
 * Turn a kphys or physcial address into a 32-bit K0 (cached, unmapped) address
 */
#define KPHYSTO32K0(_x)		and _x, 0x1fffffff ; or _x 0x80000000

/*
 * Turn a kphys or physical address into a 32-bit K1 (uncached, unmapped) addr.
 */
#define KPHYSTO32K1(_x)		and _x, 0x1fffffff ; or _x 0xa0000000

#if (_MIPS_SZPTR == 64)

/*
 * Turn a kphys or physcial address into a native K0 (cached, unmapped) address
 */
#define KPHYSTOK0(_x)		dsll	_x, 5; \
				dsrl	_x, 5; \
				daddu	_x, 0xa800000000000000;

/*
 * Turn a kphys or physical address into a native K1 (uncached, unmapped) addr.
 */
#define KPHYSTOK1(_x)		dsll	_x, 5; \
				dsrl	_x, 5; \
				daddu	_x, 0x9000000000000000;

#elif (_MIPS_SZPTR == 32)

#define KPHYSTOK0(_x)		KPHYSTO32K0(_x)
#define KPHYSTOK1(_x)		KPHYSTO32K1(_x)

#endif /* MIPS_SZ_PTR == 64 */

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY

#  if (_MIPS_SZPTR == 64)

/*
 * Assembly code to convert 32-bit addresses back into 64-bit addresses.
 * We must detect whether the address is K0 or K1 and synthesize an appropriate
 * KPhys addresses.
 */
#define K32TOKPHYS(_x, _tmp1, _tmp2)	\
	lui	_tmp1, 0x2000;		/* The bit that changes from K0/K1 */ \
	and	_tmp1, _x;		/* See if it's on. */ \
	lui	_tmp2, 0xe000;		/* Get ready to lop off top bits. */ \
					/* 0xe000 should be sign-extended. */ \
	not	_tmp2;			/* Form the mask  */\
	beqz	_tmp1, 200f;		/* If it's K0, jump forward. */ \
	and	_x, _tmp2;		/* Lop 'em off  (BD SLOT) */ \
	/*  Synthesize an uncached 64-bit KPhys address */ \
	lui	_tmp1, 0x9000;		\
	b	201f;			\
	nop;				\
	/*  Synthesize a cached 64-bit KPhys address */	\
200:	lui     _tmp1, 0xa800;		\
201:	dsll	_tmp1, 32;		/*  Shift the thing _way_ over. */ \
	or	_x, _tmp1;		/*  Form the address */

#  elif (_MIPS_SZPTR == 32)

/* We don't need the thing in 32-bit mode. */
#define K32TOKPHYS(_x, _tmp1, _tmp2)

#  endif /* _MIPS_SZPTR == 64 */

#elif _LANGUAGE_C

#  if (_MIPS_SZPTR == 64)

/* Turn a 32-bit K1 address into an uncached KPhys address. */
#define K132TOKPHYS(_x)	(((_x) & 0x1fffffff) | (0x9000000000000000L))

/* Turn a 32-bit K0 address into a cached, coherent KPhys address. */
#define K032TOKPHYS(_x) (((_x) & 0x1fffffff) | (0xa800000000000000L))

#  elif (_MIPS_SZPTR == 32)

/* These are unnecessary in 32-bit mode. */
#define K132TOKPHYS(_x)	_x
#define K032TOKPHYS(_x) _x

#  endif /* (_MIPS_SZPTR == 64) */

#endif /* _LANGUAGE_ASSEMBLY */

#endif /* __EVADDRMACROS_H__ */
