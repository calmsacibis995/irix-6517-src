#ident	"$Revision: 1.7 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/user.h>
#include <sys/sema.h>
#include <sys/grio.h>
#undef _KERNEL
#include <sys/proc.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/errno.h>
#include <sys/dnlc.h>
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
#include "xfs_bmap.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"


#include "sim.h"

#define	MAXLEVELS	10
vnode_t		*vps[MAXLEVELS];
char		*names[MAXLEVELS];
int		level;

main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	struct cred	cred;
	xfs_ino_t	ino;
	int		x, i;
	vattr_t		vap;
	sim_init_t	si;
	boolean_t	call_again;
	int		error;
	char		*log;
	int		fflag;
	int		c;
	char		buf[256];

	if (argc < 2) {
		printf("usage: vbigdir [-f] [-l logdev] <device>\n");
		exit(1);
	}
	xlog_debug = 0;
	fflag = 0;
	log = NULL;
	while ((c = getopt(argc, argv, "fl:")) != EOF) {
		switch (c) {
		case 'f':
			fflag = 1;
			break;
		case 'l':
			log = optarg;
			xlog_debug = 1;
			break;
		case '?':
			printf("usage: vdir [-f] [-l logdev] <device>\n");
			exit(1);
		}
	}
	if (argc - optind != 1) {
		printf("usage: vdir [-f] [-l logdev] <device>\n");
		exit(1);
	}
	bzero(&si, sizeof(si));
	si.dname = argv[optind];
	si.disfile = fflag;
	si.logname = log;
	si.lisfile = fflag;
	printf("mount %s\n", si.dname);
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", si.dname, sbp->sb_magicnum);
		exit(1);
	}

	/*
	 * Get the root inode.
	 */
	ino = sbp->sb_rootino;	

	/*
	 * Set up cred to be superuser.
	 */
	cred.cr_uid = 0;

	ip = xfs_iget(mp, NULL, ino, XFS_ILOCK_EXCL);
	vps[0] = XFS_ITOV(ip);
	VN_HOLD(vps[0]);
	xfs_iput(ip, XFS_ILOCK_EXCL);

	vap.va_type = VREG;
	vap.va_mode = 0777;
	level = 0;
	names[0] = ".";

	while (gets(buf)) {
		if (strcmp(buf, "..") == 0) {
			if (level > 0) {
				VN_RELE(vps[level]);
				vps[level] = NULL;
				free(names[level]);
				names[level] = NULL;
				level--;
			}
			continue;
		} else if (buf[i = strlen(buf) - 1] == '/') {
			buf[i] = '\0';
			error = VOP_MKDIR(vps[level], buf, &vap, &vps[level + 1], &cred);
			ASSERT(!error);
			level++;
			names[level] = malloc(i + 1);
			strcpy(names[level], buf);
			printf("mkdir ");
			for (i = 0; i <= level; i++)
				printf("%s/", names[i]);
			printf(".\n");
		} else {
			error = VOP_CREATE(vps[level], buf, &vap, 0, 0777, &vps[level + 1], &cred);
			ASSERT(!error);
			VN_RELE(vps[level + 1]);
			printf("create ");
			for (i = 0; i <= level; i++)
				printf("%s/", names[i]);
			printf("%s\n", buf);
		}
	}
	/*
	 * Release directory vnodes.
	 */
	while (level >= 0)
		VN_RELE(vps[level--]);

	xfs_umount(mp);
	dev_close(si.ddev);
	exit(0);
}
