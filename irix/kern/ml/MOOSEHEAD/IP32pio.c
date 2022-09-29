#include <sys/types.h>
#include <sys/param.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>

piomap_t*
pio_mapalloc (uint_t bus, uint_t adap, iospace_t* iospace, int flag, char *name)
{
    int	 i;
    piomap_t *piomap;
    caddr_t  vaddr;
    paddr_t  paddr;
    uint     size;

    if (bus != ADAP_PCI)
	return(0);

	/* Allocates a handle */

    piomap = (piomap_t*) kern_calloc(1, sizeof(piomap_t));

    /* fills the handle */

    piomap->pio_bus	= bus;
    piomap->pio_adap	= adap;
    piomap->pio_type	= iospace->ios_type;
    piomap->pio_iopaddr	= iospace->ios_iopaddr;
    piomap->pio_size	= iospace->ios_size;
    piomap->pio_flag	= flag;
    piomap->pio_reg	= PIOREG_NULL;
    for (i = 0; i < 7; i++)
	if (name[i])
	    piomap->pio_name[i] = name[i];

    if (bus == ADAP_PCI)
	switch (iospace->ios_type) {

	case PIOMAP_PCI_IO:
	case PIOMAP_PCI_MEM:
	    size = btoc(iospace->ios_size);
	    paddr = iospace->ios_iopaddr;
	    piomap->pio_vaddr = vaddr = (caddr_t)kvalloc(size, VM_UNCACHED, 0);
	    for (i = 0; i < size; i++, vaddr += NBPP, paddr += NBPP)
		pg_setpgi(kvtokptbl(vaddr), mkpde(PG_VR|PG_G|PG_SV|PG_M|PG_N,
						  paddr >> PTE_PFNSHFT));

	    break;
	default:
	    kern_free(piomap);
	    return 0;
	}

    return(piomap);
}

caddr_t 
pio_mapaddr (piomap_t* piomap, iopaddr_t addr)
{
    if (addr > (piomap->pio_iopaddr + piomap->pio_size))
	return NULL;
    return piomap->pio_vaddr + (addr - piomap->pio_iopaddr);
}

void
pio_mapfree (piomap_t* piomap)
{
	if (piomap) {
	        kvfree(piomap->pio_vaddr, btoc(piomap->pio_size));
		kern_free(piomap);
	}
}

void
orb_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(uchar_t *)addr |= (uchar_t)m;

	splx(s);
}

void
orh_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(ushort_t *)addr |= (uchar_t)m;

	splx(s);
}

void
orw_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(ulong_t *)addr |= (ulong_t)m;

	splx(s);
}

void
andb_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(uchar_t *)addr &= (uchar_t)m;

	splx(s);
}

void
andh_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(ushort_t *)addr &= (uchar_t)m;

	splx(s);
}

void
andw_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = spl7();

	*(ulong_t *)addr &= (ulong_t)m;

	splx(s);
}
