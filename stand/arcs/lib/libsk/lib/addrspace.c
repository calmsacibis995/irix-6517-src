
#include <sys/types.h>
#include <libsk.h>
#include <libsc.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/immu.h>

extern int _ftext[];

#ifndef SN0

/*
 * Get NUMA Address Space ID.  Only needed for distributed shared memory
 * machines.  Other machiens return 0.
 */
nasid_t
get_nasid()
{
	return (nasid_t)0;
}

#endif

/*
 * Takes a mapped address and returns a physical address.
 * We're assuming here that the address is mapped only to make it independent
 * of NASID (NUMA Address Space ID) so the virtual address is really direct
 * mapped to the physical address - for example, a mapped kernel.
 * We assume that the kernel will use 16M pages so we must mask off bits
 * higher than that.
 */
paddr_t
kdmtolocalphys(void* virtual)
{
#ifdef NODE_OFFSET
        /* If it's not mapped, don't mask off any bits */
        if (!IS_KSEG2(virtual))
                return (paddr_t)KDM_TO_PHYS(virtual);

        return (paddr_t)(NODE_OFFSET(get_nasid()) |
                ((__psunsigned_t)virtual & MAPPED_KERNEL_PAGE_MASK));
#else
        return (paddr_t)KDM_TO_PHYS(virtual);
#endif
}

/*
 * Takes a mapped address and returns a physical address.
 * We're assuming here that the address is mapped only to make it independent
 * of NASID (NUMA Address Space ID) so the virtual address is really direct
 * mapped to the physical address - for example, a mapped kernel.
 */
paddr_t
kdmtolocal(void* virtual)
{
	/* If it's not mapped, don't mess with it. */
	if (!IS_KSEG2(virtual))
		return (paddr_t)virtual;

	/* If it's mapped, change it to K0 */
        return (paddr_t)PHYS_TO_K0(kdmtolocalphys(virtual));
}


/*
 * Takes a mapped prom address and returns its physical address.  We're
 * assuming here that the prom is mapped only to make it independent of
 * NASID (NUMA Address Space ID) so the virtual address is really direct
 * mapped to the physical address. Actually the io6prom is mapped at
 * 256M + loadaddr. This is done for two reasons. A direct mapped
 * io6prom falls within 32M mappings used by the kernel and 256M is 
 * added so that branches will work until mappings are in place
 */
paddr_t
kvtophys(void* virtual)
{
	/* If it's not mapped space, return the pointer passed in. */
	if (!IS_KSEG2(virtual))
		return (paddr_t)KDM_TO_PHYS(virtual);

	if (!IS_KSEG2(_ftext))
		panic("kvtophys: Tried to translate K2 address in unmapped prom!");

#ifdef NODE_OFFSET
	return (paddr_t)(NODE_OFFSET(get_nasid()) | KDM_TO_PHYS((__psunsigned_t) virtual & 0xfffffff));
#else
	return (paddr_t)KDM_TO_PHYS(virtual);
#endif
}
