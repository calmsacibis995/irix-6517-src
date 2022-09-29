#ident "symmon/machdep.c: $Revision: 1.29 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * machdep.c -- machine dependent dbgmon routines
 */

#include <fault.h>
#include "dbgmon.h"
#include "mp.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/sysmacros.h"
#include "sys/mapped_kernel.h"
#include <libsc.h>

#ifdef MAPPED_KERNEL
#define TRANSLATE_ADDR(x)	(x)
#else
#define TRANSLATE_ADDR(x)	(x)
#endif

static void _clear_cache(caddr_t, int);

/*
 * _get_memory -- read memory location as indicated by flags
 * ??? SHOULD THIS FLUSH CACHE BEFORE DOING READ ???
 */
long long
_get_memory(caddr_t caddr, int width)
{

	switch (width) {
	case SW_BYTE:
		return (*(unsigned char *)TRANSLATE_ADDR(caddr));

	case SW_HALFWORD:
		return (*(unsigned short *)TRANSLATE_ADDR(caddr));

	case SW_WORD:
		return(*(unsigned *)TRANSLATE_ADDR(caddr));

	case SW_DOUBLEWORD:
		return(load_double((long long *)TRANSLATE_ADDR(caddr)));
	
	default:
		_dbg_error("get_memory: Illegal width", 0);
	}
	/*NOTREACHED*/
}

/*
 * set_memory -- set memory location as indicated by flags
 */
void
_set_memory(caddr_t caddr, int width, long long value)
{

	switch (width) {

	case SW_BYTE:
		*(unsigned char *)TRANSLATE_ADDR(caddr) = value;
		break;

	case SW_HALFWORD:
		*(unsigned short *)TRANSLATE_ADDR(caddr) = value;
		break;

	case SW_WORD:
		*(unsigned *)TRANSLATE_ADDR(caddr) = value;
		break;

	case SW_DOUBLEWORD:
		store_double((long long *)TRANSLATE_ADDR(caddr), value);
		break;
	
	default:
		_fatal_error("set_memory: Illegal switch");
	}
	_clear_cache(caddr, width);
}

/*
 * _get_register -- read processor register as indicated by flags
 */
k_machreg_t
_get_register(int reg)
{
	if (reg >= (int)NREGS)
		_dbg_error("invalid register number", 0);
	return(private.regs[reg]);
}

/*
 * set_register -- write processor register as indicated by flags
 */
void
_set_register(int reg, k_machreg_t value)
{
	if (reg >= (int)NREGS)
		_dbg_error("invalid register number", 0);

	private.regs[reg] = value;
}


#define	_4M_MASK	0x3fffff

/* clear_cache() wrapper to handle K2 seg addresses */
static void
_clear_cache(caddr_t caddr, int len)
{
	if (IS_KSEG0(caddr) || IS_MAPPED_KERN_SPACE(caddr))	
		/* no need to do anything with K0 address */
		clear_cache(caddr, len);
	else if (IS_KSEG2(caddr)) {
#if TFP
		printf("TFP does not handle K2 addresses in cache flush yet\n");
		printf("Flushing entire cache\n");
		flush_cache();
#else
		while (len) {
			int count;
			unsigned int index;
			caddr_t k0addr;
			unsigned int pfn;
			unsigned int physaddr;

			index = probe_tlb(caddr, 0); /* index must be valid */
#if R4000 || R10000
			if (pnum(caddr) & 0x1)
				pfn = get_tlblo1(index) >> TLBLO_PFNSHIFT;
			else
				pfn = get_tlblo0(index) >> TLBLO_PFNSHIFT;
#else
			pfn = get_tlblo(index) >> TLBLO_PFNSHIFT;
#endif	/* R4000 || R10000 */

			physaddr = (ctob(pfn) + poff(caddr)) & _4M_MASK;
#if IP20 || IP22 /* || IP24 (implied by IP22) */
			k0addr = (caddr_t)PHYS_TO_K0_RAM(physaddr);
#else
			k0addr = (caddr_t)PHYS_TO_K0(physaddr);
#endif	/* IP20 || IP22 || IP24 */
			count = MIN(NBPP - poff(caddr), len);
			clear_cache(k0addr, count);
			caddr += count;
			len -= count;
		}
#endif	/* !TFP */
	}
}

