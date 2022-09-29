#include <sys/vnode.h>
#include <sys/vfs.h>
struct vfsops umfs_vfsops;
struct vnodeops umfs_vnodeops;
extern int nopkg(void);
int umfscall () { return nopkg(); }
int umfs_init () { return 0; }

