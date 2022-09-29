/*
 * cxfs array stubs
 */
#include <sys/errno.h>
#include <sys/types.h>
#include <fs/xfs/cxfs_clnt.h>

void
cxfs_arrinit() {}

/* ARGSUSED */
int
cxfs_mount(
	void *mp,
	struct xfs_args *ap,
	dev_t dev,
	int *clp)
{
	*clp = 0;
	if (ap && XFSARGS_FOR_CXFSARR(ap))
		return (EINVAL);
	else
	        return (0);
}
 
void
cxfs_unmount() {}

