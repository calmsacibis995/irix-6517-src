#define R4000 1
#define IP19 1			/* XXX Hack */
#define	_PAGESZ	16384

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>

#define KERNBASE 0x80000000
/*
 * R4K CPU board specific routine to get a K0/1 address from a K2seg address
 * Used with pagesize of 16KB.
 */
off_t
ptophys4kip19_16k(off_t base, pde_t Sysmap[])
{
	if (!IS_KSEG2(base))
		return(base &= ~KERNBASE);
#ifndef _IRIX4
	return (off_t)((Sysmap[pnum((uint)base 
				 - (uint)K2SEG)].pte.pg_pfn)<<PNUMSHFT);
#else
	return (off_t)((Sysmap[pnum((uint)base 
				 - (uint)K2SEG)].pgm.pg_pfn)<<PNUMSHFT);
#endif
}
