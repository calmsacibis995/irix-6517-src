#ident	"lib/libsc/lib/mem.c: $Revision: 1.8 $"

/*
 * mem.c - useful libsc-level functions to manipulate memory descriptors
 */

#include <arcs/hinv.h>
#include <libsc.h>

/* 
 * Return a pointer to the memory descriptor that represents
 * the largest contiguous memory space.  This is useful for 
 * finding a default load area for a relocatable program.
 */
MEMORYDESCRIPTOR *
mem_getblock(void)
{
    MEMORYDESCRIPTOR *d, *m = NULL;

    for (d = GetMemoryDescriptor(NULL); d; d = GetMemoryDescriptor(d)) {
	if (d->Type == FreeMemory && (!m || (d->PageCount > m->PageCount))) 
	    m = d;
    }
    return m;
}

/*
 * If there exists a memory descriptor that contains the
 * address base/bytes pair, then return a pointer to it,
 * otherwise return NULL.
 */
MEMORYDESCRIPTOR *
mem_contains(__psunsigned_t base, unsigned long bytes)
{
    MEMORYDESCRIPTOR *d;

    for (d = GetMemoryDescriptor(NULL); d; d = GetMemoryDescriptor(d)) {
	if (d->Type == FreeMemory && base >= arcs_ptob(d->BasePage) &&
	    bytes <= arcs_ptob(d->PageCount))
	    return d;
    }
    return NULL;
}

void
mem_list(void)
{
    MEMORYDESCRIPTOR *d = GetMemoryDescriptor(NULL);
    int idx = 0;

    if (!d) {
	printf ("No memory descriptors available.\n");
	return;
    }

    while (d) {
	printf ("Descriptor %2d: base = 0x%08lx, pages = 0x%06lx, type = ",
	    idx++, arcs_ptob(d->BasePage), d->PageCount);
	switch (d->Type) {
	case ExceptionBlock:
	    printf ("ExceptionBlock\n");
	    break;
	case SPBPage:
	    printf ("SystemParameterBlock\n");
	    break;
	case FreeContiguous:
	    printf ("FreeContiguous\n");
	    break;
	case FreeMemory:
	    printf ("FreeMemory\n");
	    break;
	case BadMemory:
	    printf ("BadMemory\n");
	    break;
	case LoadedProgram:
	    printf ("LoadedProgram\n");
	    break;
	case FirmwareTemporary:
	    printf ("FirmwareTemporary\n");
	    break;
	case FirmwarePermanent:
	    printf ("FirmwarePermanent\n");
	    break;
	default:
	    printf ("Unknown\n");
	    break;
	}
	d = GetMemoryDescriptor(d);
    }
}
