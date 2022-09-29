#ident	"$Revision: 1.20 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
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
	vnode_t		*vp;
	sim_init_t	si;
	buf_t		*ialloc_context;
        boolean_t       call_again;

	if (argc < 3) {
		printf("usage: ialloc <file> <logfile>\n");
		exit(1);
	}
	bzero(&si, sizeof(si));
	si.dname = argv[1];
	si.logname = argv[2];
	printf("mount %s log %s\n", si.dname, si.logname);
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", si.dname, sbp->sb_magicnum);
		exit(1);
	}

	tp = xfs_trans_alloc(mp, 7);
	if (xfs_trans_reserve(tp, 32, BBTOB(128), 0, 0) != 0) {
		printf("reserve failed\n");
		abort();
	}
	ip = xfs_trans_iget(mp, tp, mp->m_sb.sb_rootino, XFS_ILOCK_EXCL);

	xfs_iprint(ip);

	ip2 = xfs_ialloc(tp, ip, IFREG | 0777, 1, NODEV, &cred,
			 &ialloc_context, &call_again);
	if (call_again) {
		xfs_trans_bhold(tp, ialloc_context);
                xfs_trans_commit(tp, 0);
                tp = xfs_trans_alloc(mp, 7);
		if (xfs_trans_reserve(tp, 32, BBTOB(128), 0, 0) != 0) {
			printf("reserve failed\n");
			abort();
		}
                xfs_trans_bjoin (tp, ialloc_context);
		ip2 = xfs_ialloc(tp, ip, IFREG | 0777, 1, NODEV, &cred,
                         &ialloc_context, &call_again);
	}
	xfs_trans_ihold(tp, ip2);
	xfs_trans_commit(tp, 0);

	xfs_iflock(ip2);
	xfs_iflush(ip2, B_DELWRI);

	xfs_iprint(ip2);
	xfs_iput(ip2, XFS_ILOCK_EXCL);

	bflush(si.ddev);

}
