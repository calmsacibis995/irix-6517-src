#include <sys/vnode.h>
#include <sys/vfs.h>
struct vfsops efs_vfsops;
struct vnodeops efs_vnodeops;
int efs_fstype = -1;
void ilock() {}
void iunlock() {}
int efs_statdevvp() { return 0; }
