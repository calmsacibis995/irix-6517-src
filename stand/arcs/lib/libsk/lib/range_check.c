#ident	"lib/libsc/lib/range_check.c:  $Revision: 1.5 $"

#include <sys/sbd.h>
#include <arcs/hinv.h>
#include <libsc.h>

/*
 * range_check - test to see that base and bytes are within a
 *	free memory fragment
 *
 *	returns 0 if memory is free
 *		1 if memory is not free
 *		-1 if address/size is not contained on list
 */
int
range_check(__psunsigned_t base, unsigned bytes)
{
    MEMORYDESCRIPTOR *m;
    __psunsigned_t addr;

    /* If the caller is doing virtual address mapping, we can't
     * do much about it.
     */
    if (!IS_KSEGDM(base))
	return 0;

    addr = KDM_TO_PHYS(base);

    /* Search the list for this base address
     */
    m = GetMemoryDescriptor(NULL);
    while (m) {
	if ((addr >= arcs_ptob(m->BasePage)) &&
		(addr+bytes <= arcs_ptob(m->BasePage+m->PageCount)))
	    break;
	m = GetMemoryDescriptor(m);
    }

    if (!m)
	return -1;

    if (m->Type == FreeMemory)
	return 0;
    else
	return 1;
}
