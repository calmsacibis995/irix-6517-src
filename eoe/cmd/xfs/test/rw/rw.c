
#ident	"$Revision: 1.8 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL 1
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/grio.h>
#undef _KERNEL
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <bstring.h>
#include <memory.h>
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
#include "xfs_iunlink_item.h"
#include "xfs_bmap.h"
#include "xfs_rw.h"

#include "sim.h"


void
alloc_inum(xfs_mount_t *mp);

void
free_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino);

void
print_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino);

void
push_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino);

void
read_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino,
	  off_t		off,
	  int		len,
	  int		iosize,
	  char		byte);

void
write_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino,
	   off_t	off,
	   int		len,
	   int		iosize,
	   char		byte);

void
flush_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino);

void
trunc_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino,
	   __int64_t	new_size);


extern void
xfs_write_file(vnode_t	*vp,
	       uio_t	*uiop,
	       int	ioflag,
	       cred_t	*credp);

extern void
xfs_read_file(vnode_t	*vp,
	      uio_t	*uiop,
	      int	ioflag,
	      cred_t	*credp);


main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	char		*file;
	dev_t		dev;
	int		x;
	xfs_ino_t	inum;
	vnode_t		*vp;
	char		*logfile;
	dev_t		logdev;
	extern void	bunlocked(void);
	off_t		off;
	int		len;
	int		iosize;
	char		c;
	__int64_t	new_size;
	sim_init_t	si;
	char		line[80];

	if (argc < 3) {
		printf("usage: rw <file> <logfile > \n");
		exit(1);
	}
	file = argv[1];
	logfile = argv[2];
	printf("mount %s log %s\n", file, logfile);

	si.volname = NULL;
	si.rtname = NULL;
	si.dname = file;
	si.disfile = 1;
	si.dcreat = 0;
	si.logname = logfile;
	si.lisfile = 1;
	si.lcreat = 0;
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, NULL);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", file, sbp->sb_magicnum);
		exit(1);
	}

	printf("At your command:\n");
	while (fgets(line, sizeof(line), stdin)) {
		switch (line[0]) {
		case 'a':
			alloc_inum(mp);
			break;

		case 'f':
			sscanf(&line[1], "%lld", &inum);
			flush_inum(mp, inum);
			break;

		case 'F':
			sscanf(&line[1], "%lld", &inum);
			free_inum(mp, inum);
			break;

		case 'p':
			sscanf(&line[1], " %lld", &inum);
			print_inum(mp, inum);
			break;

		case 'P':
			sscanf(&line[1], " %lld", &inum);
			push_inum(mp, inum);
			break;

		case 'r':
			sscanf(&line[1], "%lld %d %d %d %s", &inum, &off,
			       &len, &iosize, &c);
			read_inum(mp, inum, off, len, iosize, c);
			break;

		case 't':
			sscanf(&line[1], "%lld %lld", &inum, &new_size);
			trunc_inum(mp, inum, new_size);
			break;

		case 'w':
			sscanf(&line[1], "%lld %d %d %d %s", &inum, &off,
			       &len, &iosize, &c);
			write_inum(mp, inum, off, len, iosize, c);
			break;

		default:
			printf("bad arg\n");
			break;
		}
		printf("At your command:\n");
	}
}

void
alloc_inum(xfs_mount_t	*mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_inode_t	*rootip;
	xfs_ino_t	root_ino;
	xfs_ino_t	ino;
	buf_t		*agibp;
	boolean_t	call_again;
	cred_t		cred;

	cred.cr_uid = 0;
	cred.cr_gid = 0;

	call_again = B_FALSE;
	agibp = NULL;
	tp = xfs_trans_alloc(mp, 7);
	if (xfs_trans_reserve(tp, 32, BBTOB(16), 0, 0) != 0) {
		xfs_trans_cancel(tp, 0);
		printf("Out of space, can't allocate inode\n");
		return;
	}	

	root_ino = mp->m_sb.sb_rootino;
	rootip = xfs_trans_iget(mp, tp, root_ino,
				XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	ip = xfs_ialloc(tp, rootip, IFREG | 0777, 1, NODEV, &cred,
			&agibp, &call_again);
	if (call_again) {
		xfs_trans_bhold(tp, agibp);
		xfs_trans_ihold(tp, rootip);
		xfs_trans_commit(tp, 0);

		tp = xfs_trans_alloc(mp, 7);
		if (xfs_trans_reserve(tp, 0, BBTOB(16), 0, 0) != 0) {
			xfs_trans_cancel(tp, 0);
			printf("Out of space, can't allocate inode\n");
			return;
		}
		xfs_trans_bjoin(tp, agibp);
		xfs_trans_ijoin(tp, rootip,
				XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
		ip = xfs_ialloc(tp, rootip, IFREG | 0777, 1, NODEV, &cred,
				&agibp, &call_again);
		ASSERT(call_again == 0);
	}
	xfs_trans_ihold(tp, ip);
	xfs_trans_commit(tp, 0);
	ino = ip->i_ino;
	printf("New ino is %lld\n", ino);
	xfs_iflock(ip);
	xfs_iflush(ip, 0);
	xfs_iput(ip, XFS_ILOCK_EXCL);
	bflush(mp->m_dev);
}

void
free_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	
	tp = xfs_trans_alloc(mp, 7);
	if (xfs_trans_reserve(tp, 0, BBTOB(16), 0, XFS_TRANS_PERM_LOG_RES)
	    != 0) {
		xfs_trans_cancel(tp, 0);
		printf("Out of space, can't allocate inode\n");
		return;
	}	

	ip = xfs_trans_iget(mp, tp, ino,
			    XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	ip->i_d.di_size = 0;
	ip->i_d.di_nextents = 0;
	ip->i_d.di_nlink = 0;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_log_iui(tp, ip);
	xfs_trans_commit(tp, 0);

	tp = xfs_trans_alloc(mp, 7);
	if (xfs_trans_reserve(tp, 0, BBTOB(16), 0, 0) != 0) {
		xfs_trans_cancel(tp, 0);
		printf("Out of space, can't allocate inode\n");
		return;
	}	

	ip = xfs_trans_iget(mp, tp, ino,
			    XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
	xfs_ifree(tp, ip);
	xfs_trans_log_iui_done(tp, ip);
	xfs_trans_commit(tp, 0);
}


void
print_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino)
{
	xfs_inode_t	*ip;

	ip = xfs_iget(mp, NULL, ino, XFS_ILOCK_EXCL);

	if (ip == NULL) {
		printf("No such inode %lld\n", ino);
		return;
	}
	
	xfs_iprint(ip);

	xfs_iput(ip, XFS_ILOCK_EXCL);
}


void
read_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino,
	  off_t		off,
	  int		len,
	  int		iosize,
	  char		byte)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	uio_t		uio;
	iovec_t		iovec;
	char		*buf;
	char		*check_buf;
	int		total_read;
	int		diff;
	__int64_t	isize;

	ip = xfs_iget(mp, NULL, ino, XFS_IOLOCK_SHARED);
	if (ip == NULL) {
		printf("unknown ino %lld\n", ino);
		return;
	}
	isize = ip->i_d.di_size;
	check_buf = (char *)malloc(iosize);
	memset(check_buf, byte, iosize);

	buf = (char *)malloc(iosize);
	bzero(buf, iosize);
	iovec.iov_base = buf;
	iovec.iov_len = iosize;
	uio.uio_iov = &iovec;
	uio.uio_iovcnt = 1;
	uio.uio_offset = off;
	uio.uio_resid = iosize;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_limit = 0x7fffffff;
	uio.uio_fmode = 0;

	vp = XFS_ITOV(ip);

	total_read = 0;
	while (total_read < len) {
		xfs_read(vp, &uio, 0, NULL);
		if (uio.uio_resid != 0) {
			ASSERT(total_read + iosize - uio.uio_resid == isize);
		}
		diff = bcmp(buf, check_buf, iosize - uio.uio_resid);
		ASSERT(diff == 0);
		total_read += iosize;
		if (total_read + iosize > len) {
			iosize = len - total_read;
		}
		/* uio_offset is bumped automtically */
		uio.uio_resid = iosize;
		iovec.iov_base = buf;
		iovec.iov_len = iosize;
		bzero(buf, iosize);
	}

	xfs_iput(ip, XFS_IOLOCK_SHARED);
}


void
write_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino,
	   off_t	off,
	   int		len,
	   int		iosize,
	   char		byte)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	uio_t		uio;
	iovec_t		iovec;
	char		*buf;
	int		total_written;

	ip = xfs_iget(mp, NULL, ino, XFS_IOLOCK_EXCL);
	if (ip == NULL) {
		printf("unknown ino %lld\n", ino);
		return;
	}

	buf = (char *)malloc(iosize);
	memset(buf, byte, iosize);
	iovec.iov_base = buf;
	iovec.iov_len = iosize;
	uio.uio_iov = &iovec;
	uio.uio_iovcnt = 1;
	uio.uio_offset = off;
	uio.uio_resid = iosize;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_limit = 0x7fffffff;
	uio.uio_fmode = 0;

	vp = XFS_ITOV(ip);

	total_written = 0;
	while (total_written < len) {
		xfs_write(vp, &uio, 0, NULL);
		if (uio.uio_resid != 0) {
			ASSERT(0);
		}
		total_written += iosize;
		if (total_written + iosize > len) {
			iosize = len - total_written;
		}
		/* uio_offset is bumped automatically */
		uio.uio_resid = iosize;
		iovec.iov_base = buf;
		iovec.iov_len = iosize;
	}

	xfs_iput(ip, XFS_IOLOCK_EXCL);
}


void
flush_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	off_t		isize;

	ip = xfs_iget(mp, NULL, ino, XFS_IOLOCK_SHARED);
	if (ip == NULL) {
		printf("unknown ino %lld\n", ino);
		return;
	}

	vp = XFS_ITOV(ip);

	isize = (off_t)ip->i_d.di_size;
	chunkpush(vp, 0, isize, 0);

	xfs_iput(ip, XFS_IOLOCK_SHARED);
}

void
push_inum(xfs_mount_t	*mp,
	  xfs_ino_t	ino)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	
	ip = xfs_iget(mp, NULL, ino, XFS_IOLOCK_SHARED | XFS_ILOCK_SHARED);
	if (ip == NULL) {
		printf("unknown ino %lld\n", ino);
		return;
	}

	xfs_iflock(ip);
	xfs_iflush(ip, 0);

	xfs_iput(ip, XFS_IOLOCK_SHARED | XFS_ILOCK_SHARED);
}


void
trunc_inum(xfs_mount_t	*mp,
	   xfs_ino_t	ino,
	   __int64_t	new_size)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	xfs_trans_t	*tp;

	tp = xfs_trans_alloc(mp, 0);
	if (xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0,
			      XFS_TRANS_PERM_LOG_RES) != 0) {
		printf("trunc_inum: reserve failed\n");
		xfs_trans_cancel(tp, 0);
		return;
	}

	ip = xfs_trans_iget(mp, tp, ino, XFS_IOLOCK_EXCL);
	if (ip == NULL) {
		printf("trunc_inum: iget failed\n");
		xfs_trans_cancel(tp, 0);
		return;
	}

	if (new_size >= ip->i_d.di_size) {
		printf("trunc_inum: new_size >= current size\n");
		xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
		return;
	}

	xfs_itruncate_start(ip, new_size, XFS_ITRUNC_DEFINITE);
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_itruncate_finish(&tp, ip, new_size);

	xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES);
	xfs_iput(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL);
}
