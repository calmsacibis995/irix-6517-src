#include "sys/types.h"
#include "sys/vnode.h"
#include "sys/vfs.h"

dev_t nfs_major = ((dev_t)-1);
struct vnodeops nfs_vnodeops = {
    0,
};
struct vfsops nfs_vfsops = {
	0,
};

int nfs_fstype = -1, nfs3_fstype = -1;

struct exportinfo *exihashtab[256];

/* client NFS s/w is not included in Mini Root */
void
clntkudpxid_init()
{
}
caddr_t
nfs_getrcstat() 
{
	return((caddr_t)0);
}
caddr_t
nfs_getclstat() 
{
	return((caddr_t)0);
}
caddr_t
nfs_getclstat3() 
{
	return((caddr_t)0);
}
caddr_t
nfs_getrsstat() 
{
	return((caddr_t)0);
}
caddr_t
nfs_getsvstat3() 
{
	return((caddr_t)0);
}
caddr_t
nfs_getsvstat() 
{
	return((caddr_t)0);
}

void
nfs_clearstat() 
{
}

void stop_nfs(void) { }

void nfs_icrash(void) { }
