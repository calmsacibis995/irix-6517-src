#include "netstat.h"

#if SYSTEM_r4k
#define R4000		1
#define IP20		1
#define PTOPHYS		ptophys4k
#elif SYSTEM_r4kip19
#define R4000		1
#define IP19		1
#define PTOPHYS		ptophys4kip19
#elif SYSTEM_tfp
#define TFP		1
#define IP21		1
#define PTOPHYS		ptophystfp
#elif SYSTEM_r10k
#define R10000		1
#define IP25		1
#define PTOPHYS		ptophys10k
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>

#if (_MIPS_SZLONG == 64)
#define KERNBASE 0xa800000000000000
#else
#define KERNBASE 0x80000000
#endif

extern int pagesize;
extern int pnumshft;
extern struct pteconst pteconst;

#define pteconsttopfn(pte, pt) \
		((pte)->pte_pfn >> (pt)->pt_pfnshift)

#undef _PAGESZ
#undef PNUMSHFT
#define _PAGESZ		pagesize
#define PNUMSHFT	pnumshft

/*
 * System-specific routine to get a K0/1 address from a K2seg address
 */
ns_off_t
PTOPHYS(ns_off_t base, pde_t Sysmap[])
{
	ns_off_t loc;
	pte_t *ptep;
	if (!IS_KSEG2(base))
		return(base &= ~KERNBASE);
	loc = base - K2SEG;
	ptep = &Sysmap[pnum(loc)].pte;
	return (pteconsttopfn(ptep, &pteconst) * pagesize);
}
