/*  Restart block utilities needed in by the prom, but also sash (hence
 * they are in libsc.
 */

#ident "$Revision: 1.6 $"

#include <arcs/types.h>
#include <arcs/restart.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsc_internal.h>

/*
 * get_rb - search the restart block list for this cpu
 */
RestartBlock *
get_rb (void)
{
    return get_cpu_rb (getcpuid());
}

/*
 * isvalid_rb - returns 1 if restart block for cpu exists and is valid, else 0
 */
int
isvalid_rb (void)
{
    RestartBlock *rb;

    if ((rb = get_rb ()) == 0)
	return 0;

    return rb->Checksum == checksum_rb (rb) ? 1 : 0;
}

void
rbclrbs(int flag)
{
	RestartBlock *rb = (RestartBlock *)SPB->RestartBlock;

	rb->BootStatus &= ~flag;
	rb->Checksum = checksum_rb(rb);
}
