#ident	"$Revision: 1.17 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/grio.h>
#undef _KERNEL
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <bstring.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"

#include "sim.h"

main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	xfs_inode_t	*ip2;
	xfs_trans_t	*tp;
	struct cred	cred;
	xfs_ino_t	ino;
	vnode_t		*vp;
	sim_init_t	si;

	if (argc < 3) {
		printf("usage: iread <inum> <file>\n");
		exit(1);
	}
	ino = (xfs_ino_t)atoi(argv[1]);
	bzero(&si, sizeof(si));
	si.dname = argv[2];
	printf("mount %s ino %lld\n", si.dname, ino);
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", si.dname, sbp->sb_magicnum);
		exit(1);
	}

	ip = xfs_iget(mp, NULL, ino, XFS_ILOCK_SHARED);
	xfs_iprint(ip);
	xfs_iput(ip, XFS_ILOCK_SHARED);


	bflush(si.ddev);

}
