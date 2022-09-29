#include "vsn.h"

#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#undef _KERNEL

#if VERS >= V_64
#include <ksys/behavior.h>
#endif

#include <sys/fs/efs.h>

/*
 * returns 1 if it is, 0 if not
 */
int
is_efs_sb(char *agb, int sector_size)
{
	struct efs	*esb;
	extern int	valid_efs(struct efs *, int);

	esb = (struct efs *) (agb + sector_size);

	if (valid_efs(esb, esb->fs_size) == 1)
		return(1);

	return(0);
}
