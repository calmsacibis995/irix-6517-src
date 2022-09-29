#ident	"$Revision: 1.24 $"

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
#include <string.h>
#include <bstring.h>
#include <sys/debug.h>
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
	xfs_inode_t	*ip3;
	xfs_trans_t	*tp;
	struct cred	cred;
	int		ninodes;
	int		x;
	xfs_ino_t	ino;
	vnode_t		*vp;
	sim_init_t	si;
	extern void	bunlocked(void);
	buf_t		*ialloc_context = 0;
	boolean_t	call_again;
	int		i = 1;

	if (argc < 4) {
		printf("usage: ialloc2 <file> <logfile > <ninodes>\n");
		exit(1);
	}
	bzero(&si, sizeof(si));
	i = 1;
	if (!strcmp(argv[i], "-f")) {
		si.disfile = 1;
		si.lisfile = 1;
		i++;
	}
	si.dname = argv[i];
	si.logname = argv[i+1];
	ninodes = atoi(argv[i+2]);
	printf("mount %s log %s inodes %d\n", si.dname, si.logname, ninodes);
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n",
			si.dname, sbp->sb_magicnum);
		exit(1);
	}

	ino = sbp->sb_rootino;	
	for (x = 0; x < ninodes; x++) {
		tp = xfs_trans_alloc(mp, 7);
		if (xfs_trans_reserve(tp, 32, BBTOB(70), 0, 0) != 0) {
			xfs_trans_cancel(tp, 0);
			printf("Out of space\n");
			bflush(si.ddev);
			exit(1);
		}	
		ip = xfs_trans_iget(mp, tp, ino,
				    XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
		call_again = B_FALSE;
		ialloc_context = 0;
		ip2 = xfs_ialloc(tp, ip, IFREG | 0777, 1, NODEV, &cred,
				 &ialloc_context, &call_again);
		if (call_again) {
			xfs_trans_bhold(tp, ialloc_context);
			xfs_trans_commit(tp, 0);
			tp = xfs_trans_alloc(mp, 7);
			if (xfs_trans_reserve(tp, 32, BBTOB(70), 0, 0) != 0) {
				xfs_trans_cancel(tp, 0);
				printf("Out of space\n");
				bflush(si.ddev);
				exit(1);
			}

			xfs_trans_bjoin (tp, ialloc_context);
			ip2 = xfs_ialloc(tp, ip, IFREG | 0777, 1, NODEV, &cred,
                                 &ialloc_context, &call_again);
		}

		ip2->i_d.di_nlink = 2;
		xfs_trans_log_inode(tp, ip2, XFS_ILOG_CORE);
		xfs_trans_ihold(tp, ip2);
		ip3 = xfs_trans_iget(mp, tp, ino,
				     XFS_ILOCK_EXCL | XFS_IOLOCK_SHARED);
		ASSERT(ip3 == ip);
		xfs_trans_commit(tp, 0);
		ino = ip2->i_ino;
		printf("New ino is %lld\n", ino);
		xfs_iflock(ip2);
		xfs_iflush(ip2, B_DELWRI);

		vp = XFS_ITOV(ip2);
		VOP_RWLOCK(vp, 0);
		VOP_RWUNLOCK(vp, 0);

		xfs_iput(ip2, XFS_ILOCK_EXCL);
		bunlocked();
	}

	bflush(si.ddev);
	xfs_umount(mp);

}
