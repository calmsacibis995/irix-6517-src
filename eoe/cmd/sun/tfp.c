#define TFP 1
#define IP21 1			/* XXX Hack */
#define	_PAGESZ	16384		/* Another Hack */

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/immu.h>

#define KERNBASE 0x80000000
/*
 * TFP specific routine to get a K0/1 address from a K2seg address
 */
off_t
ptophystfp(off_t base, pde_t Sysmap[])
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
