/*
 * xfs stubs
 */
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/errno.h>

struct vfsops xfs_vfsops;
struct vnodeops xfs_vnodeops;
int xfs_fstype = -1;

extern int nopkg(void);

int	xfs_clear_sharedro()	{ return nopkg(); }
int	xfs_fast_fid()		{ return ENOSYS; }
int	xfs_fsoperations()	{ return nopkg(); }
void	xfs_ichgtime()		{ }
void	xfs_icrash()		{ }
void	xfs_inobp_bwcheck()	{ }
int	xfs_itable()		{ return nopkg(); }
int	xfs_mk_sharedro()	{ return nopkg(); }
int	xfs_statdevvp()		{ return 0; }
int	xfs_swappable()		{ return EINVAL; }
int	xfs_swapext()		{ return nopkg(); }
