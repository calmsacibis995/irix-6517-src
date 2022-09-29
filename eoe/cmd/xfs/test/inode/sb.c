#ident	"$Revision: 1.7 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/time.h>
#include <sys/grio.h>
#undef _KERNEL
#include <sys/vnode.h>
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
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_bmap_btree.h"
#include "xfs_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_print.h"

#include "sim.h"

main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	char		*file;
	dev_t		dev;
	xfs_inode_t	*ip2;
	xfs_trans_t	*tp;
	struct cred	cred;
	xfs_ino_t	ino;

	if (argc < 2) {
		printf("usage: sb <file>\n");
		exit(1);
	}
	file = argv[1];
	printf("mount %s \n", file);
	dev = dev_open(file);
	v.v_buf = NBUF;
	v.v_hbuf = HBUF;
	binit();
	mp = xfs_mount_init();
	xfs_mount(mp, dev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", file, sbp->sb_magicnum);
		exit(1);
	}

	xfs_print_sb(mp);

	bflush(dev);

}
