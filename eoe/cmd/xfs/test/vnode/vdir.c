#ident	"$Revision: 1.20 $"

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
#include <sys/dirent.h>
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
#include "xfs_bmap.h"
#include "xfs_ialloc.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"


#include "sim.h"

vnode_t		*root_vp, *vp1, *vp2, *vp6, *tvp, *dir1_vp, *dir2_vp;
vnode_t		*vp_symlink, *dir22_vp;
int		root_vp_c, vp1_c, vp2_c, vp6_c, dir1_vp_c, dir2_vp_c;
int		vp_symlink_c, dir22_vp_c;

static void
check_vlinks(void) {
        /*
         * Check the vnode ref counts.
         */
	if (root_vp)
		ASSERT (root_vp->v_count == root_vp_c);
	if (vp1)
		ASSERT (vp1->v_count == vp1_c);
	if (vp2)
		ASSERT (vp2->v_count == vp2_c);
	if (vp6)
		ASSERT (vp6->v_count == vp6_c);
	if (dir1_vp)
		ASSERT (dir1_vp->v_count == dir1_vp_c);
	if (dir2_vp)
		ASSERT (dir2_vp->v_count == dir2_vp_c);
	if (vp_symlink)
		ASSERT (vp_symlink->v_count == vp_symlink_c);
}

main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	xfs_inode_t	*ip2;
	xfs_inode_t	*ip3;
	xfs_trans_t	*tp;
	struct cred	cred;
	xfs_ino_t	ino;
	int		x, i;
	vattr_t		vap;
	sim_init_t	si;
	boolean_t	call_again;
	buf_t		*ag_buf;
	pathname_t	pn;
	int		error;
	int		eof;
	uio_t		uio;
	iovec_t		vec;
	struct dirent	*dir_entry;
	char		symlink[1024];
	int 		bytes_left;
	char 		*byte_p;
	char		*long_path;
	char		*long_path2;
	char		*log;
	int		fflag;
	int		c;
	


	if (argc < 2) {
		printf("usage: vdir [-f] [-l logdev] <device>\n");
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

	tp = xfs_trans_alloc(mp, 7);
	if (xfs_trans_reserve(tp, 32, BBTOB(128), 0, 0) != 0) {
		xfs_trans_cancel(tp, 0);
		printf("Out of space\n");
		bflush(si.ddev);
		exit(1);
	}	
	ip = xfs_trans_iget(mp, tp, ino,
			    XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);

	root_vp = XFS_ITOV(ip);
	VN_HOLD(root_vp);	root_vp_c = 1;	/* track vp->v_count */

	xfs_trans_commit (tp, 0);

	/* Run as root. */
	cred.cr_uid = 0;
	 

	/*
	 * VOP_LOOKUP - failure case.
	 */
	pn_alloc (&pn);
	pn_set (&pn, "file1");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_LOOKUP(root_vp, "file1", &tvp, &pn, 0, root_vp, &cred);
	ASSERT (error == ENOENT);
	check_vlinks();

	/*
	 * VOP_CREATE - success case.
	 */
	vap.va_type = VREG;
	vap.va_mode = 0777;
	error = VOP_CREATE(root_vp, "file1", &vap, 0, 0777, &vp1, &cred);
	ASSERT (! error);
	vp1_c = 1;
	check_vlinks();

	/*
         * VOP_CREATE - success case.
         */
        error = VOP_CREATE(root_vp, "file2", &vap, 0, 0777, &vp2, &cred);
	ASSERT ((! error) && (vp2 != vp1));
	vp2_c = 1;
	check_vlinks();

#if 0
        /*
         * VOP_CREATE - truncate case. Not implemented.
         */
        error = VOP_CREATE(root_vp, "file2", &vap, 0, 0777, &vp2, &cred);
	ASSERT (error == EEXIST);
#endif

	/*
	 * VOP_LOOKUP - success case.
	 */
	error = VOP_LOOKUP(root_vp, "file1", &tvp, &pn, 0, root_vp, &cred);
	ASSERT ((! error) && (tvp == vp1));
	vp1_c ++;
	check_vlinks();

	/*
	 * VOP_REMOVE - success case.
	 */
	error = VOP_REMOVE(root_vp, "file1", &cred);
	ASSERT (! error);
	check_vlinks();

	/*
	 * VOP_REMOVE - failure cases.
	 */
	error = VOP_REMOVE(root_vp, "file1", &cred);
	ASSERT (error == ENOENT);
	check_vlinks();

	error = VOP_REMOVE(root_vp, "..", &cred);
	ASSERT (error == EEXIST);
	check_vlinks();

	/*
	 * VOP_LINK - success case.
	 */
	error = VOP_LINK(root_vp, vp2, "link1", &cred);
	ASSERT (! error);
	check_vlinks();

	/*
	 * VOP_LINK -failure case.
	 */
	error = VOP_LINK(root_vp, vp2, "file2", &cred);
	ASSERT (error == EEXIST);
	check_vlinks();

	/*
	 * Now do a lookup using the link.
	 */
	pn_set (&pn, "link1");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_LOOKUP(root_vp, "link1", &tvp, &pn, 0, root_vp, &cred);
	ASSERT ((!error) && (tvp == vp2));
	vp2_c++;	/* link1 is linked to vp2 */
	check_vlinks();

	/*
	 * Now create a directory.
	 */
	vap.va_type = VDIR;
	error = VOP_MKDIR(root_vp, "dir1", &vap, &dir1_vp, &cred);
	ASSERT (!error);
	dir1_vp_c ++; check_vlinks();

	VN_RELE (vp1);	vp1_c--;
	check_vlinks();

	/*
	 * MKDIR - failure case.
	 */
	vap.va_type = VDIR;
        error = VOP_MKDIR(root_vp, "dir1", &vap, &dir1_vp, &cred);
        ASSERT (error == EEXIST);
	check_vlinks();

	/*
	 * Add entry to directory
	 */
	vap.va_type = VREG;
        vap.va_mode = 0777;
        error = VOP_CREATE(dir1_vp, "file1", &vap, 0, 0777, &tvp, &cred);
	ASSERT (!error);
	VN_RELE (tvp);
	ASSERT (tvp->v_count == 0);
	check_vlinks();

	/*
	 * Get directory entries. success case.
	 */
	vec.iov_len = 4096;
	dir_entry = (struct dirent *) malloc (4096);
	vec.iov_base = (caddr_t) dir_entry;

	uio.uio_iov = &vec;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = vec.iov_len;

	error = VOP_READDIR (dir1_vp, &uio, &cred, &eof);
	ASSERT (! error);
	ASSERT (eof);

	/*
	 * We know that there are 3 entries. ".", "..", and "file1".
	 */
	bytes_left = 4096 - uio.uio_resid;
	while (bytes_left) {
		printf ("[%s (%d)] ", dir_entry->d_name, dir_entry->d_ino);
		byte_p = (unsigned char *) dir_entry + dir_entry->d_reclen;
		bytes_left -= dir_entry->d_reclen;
		dir_entry = (struct dirent *) byte_p;
	}
	printf ("\n");


	/*
	 * Remove directory. Fail - not empty.
	 */
	error = VOP_RMDIR(root_vp, "dir1", root_vp, &cred);
	ASSERT (error == EEXIST);
	check_vlinks();

	/*
	 * VOP_SYMLINK - success case (inline).
	 *
	 *  create a link to /dir1/file1 from /symlink_1.
	 */
	vap.va_type = VLNK;
        vap.va_mode = 0777;
	error = VOP_SYMLINK(root_vp, "symlink_1", &vap, "/dir1/file1",
				&cred);
	ASSERT (!error);

	/*
	 * VOP_SYMLINK - failure case.
	 */
	error = VOP_SYMLINK(root_vp, "symlink_1", &vap, "/dir1/fileXY",
                                &cred);
	ASSERT (error == EEXIST);

	/*
	 * VOP_SYMLINK - very long symlink.
	 * aaa...aaaa/aaaaa...aaaa/aaaaa...aaaa/.... etc.
	 */
	#define MAX_P_LEN 1023

	long_path = (char *)malloc (MAX_P_LEN);
	memset (long_path, 'a', MAX_P_LEN);
	for (i=100; i<MAX_P_LEN; i +=100) {
		long_path[i] = '/';
	}
	long_path[MAX_P_LEN-1] = '\0';

	error = VOP_SYMLINK(root_vp, "symlink_2", &vap, long_path,
                                &cred);
        ASSERT (!error);


	/*
	 * VOP_READLINK - very long symlink.
	 */
	pn_set (&pn, "symlink_2");
	pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_LOOKUP(root_vp, "symlink_2", &vp_symlink, &pn, 0,
                root_vp, &cred);
        ASSERT (!error);
        ASSERT (vp_symlink->v_type == VLNK);
        vp_symlink_c = 1;
        check_vlinks();

	long_path2 = (char *)malloc(MAX_P_LEN);
	bzero (long_path2, MAX_P_LEN);
	vec.iov_len = MAX_P_LEN;
	vec.iov_base = (caddr_t) long_path2;
	uio.uio_iov = &vec;
        uio.uio_iovcnt = 1;
        uio.uio_offset = 0;
        uio.uio_segflg = UIO_SYSSPACE;
        uio.uio_resid = vec.iov_len;

	error = VOP_READLINK (vp_symlink, &uio, &cred);
	ASSERT (! error);
	ASSERT (strncmp (long_path2, long_path, MAX_P_LEN) == 0);
        check_vlinks();

	/* 
	 * VOP_READLINK - success case (inline).
	 */
	pn_set (&pn, "symlink_1");
        pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_LOOKUP(root_vp, "symlink_1", &vp_symlink, &pn, 0,
                root_vp, &cred);
        ASSERT (!error);
        ASSERT (vp_symlink->v_type == VLNK);
	vp_symlink_c = 1;
        check_vlinks();

	vec.iov_len = 1024;
        vec.iov_base = (caddr_t) symlink;

	uio.uio_iov = &vec;
        uio.uio_iovcnt = 1;
        uio.uio_offset = 0;
        uio.uio_segflg = UIO_SYSSPACE;
        uio.uio_resid = vec.iov_len;

	error = VOP_READLINK (vp_symlink, &uio, &cred);
	ASSERT (! error);
	ASSERT (strncmp (symlink, "/dir1/file1", strlen("/dir1/file1")) == 0);
	check_vlinks();

	/*
	 * VOP_READLINK - failure case (inline).
	 */
	uio.uio_iov = &vec;
        uio.uio_iovcnt = 1;
        uio.uio_offset = -1;	/* invalid */
	uio.uio_resid = vec.iov_len;

	error = VOP_READLINK (vp_symlink, &uio, &cred);
        ASSERT (error == EINVAL);

	check_vlinks();


	/* 
	 * Rename /dir1/file1 to /file3..
	 */
	pn_set (&pn, "file3");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_RENAME(dir1_vp, "file1", root_vp, "file3", &pn, &cred);
	ASSERT (!error);
	check_vlinks();

	/*
	 * Rename /file3 to /file4.
	 */
	pn_set (&pn, "file4");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_RENAME(root_vp, "file3", root_vp, "file4", &pn, &cred);
	ASSERT (!error);
	check_vlinks();

	/*
	 * Verify that lookup of /file3 fails.
	 */
	pn_set (&pn, "file3");
	pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_LOOKUP(root_vp, "file3", &tvp, &pn, 0, root_vp, &cred);
        ASSERT (error == ENOENT);
	check_vlinks();

	/*
	 * Rename /file4 to /file4.
	 */
	pn_set (&pn, "file4");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_RENAME(root_vp, "file4", root_vp, "file4", &pn, &cred);
	ASSERT (!error);
	check_vlinks();

	/*
	 * Rename /file_not to /file4. (failure case)
	 */
	error = VOP_RENAME(root_vp, "file_not", root_vp, "file4", &pn, &cred);
        ASSERT (error == ENOENT);
	check_vlinks();

	/*
	 * Rename /file4 to /file2 (exists)
	 */
	pn_set (&pn, "file2");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_RENAME(root_vp, "file4", root_vp, "file2", &pn, &cred);
        ASSERT (!error);
	check_vlinks();

	/*
	 * Rename a directory to a file. (failure case.)
	 */
	pn_set (&pn, "file2");
	pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_RENAME(root_vp, "dir1", root_vp, "file2", &pn, &cred);
        ASSERT (error == ENOTDIR);
	check_vlinks();

	/*
	 * Rename a directory. /dir1 to /dir2.
	 */
	pn_set (&pn, "dir2");
	pn.pn_complen = strlen(pn.pn_buf);
	error = VOP_RENAME(root_vp, "dir1", root_vp, "dir2", &pn, &cred);
        ASSERT (!error);
	check_vlinks();

        /*
         * Add entry to the newly renamed directory
         */
	error = VOP_LOOKUP(root_vp, "dir2", &dir2_vp, &pn, 0, root_vp, &cred);
        ASSERT ((! error) && (dir2_vp == dir1_vp));
	/* dir1_vp & dir2_vp now point to the same vnode. So update both
	   counts */
	dir1_vp_c++;
	dir2_vp_c = dir1_vp_c;
	check_vlinks();

        vap.va_type = VREG;
        vap.va_mode = 0777;
        error = VOP_CREATE(dir2_vp, "file6", &vap, 0, 0777, &vp6, &cred);
	vp6_c = 1;
        ASSERT (!error);
	check_vlinks();

	/*
	 * Create directory /dir2/dir22.
	 */
	vap.va_type = VDIR;
        error = VOP_MKDIR(dir2_vp, "dir22", &vap, &dir22_vp, &cred);
        ASSERT (!error);
        dir22_vp_c = 1;
        check_vlinks();

	/*
	 * Rename a directory under itself - failure case. 
	 */
	pn_set (&pn, "dir2");
        pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_RENAME(root_vp, "dir2", dir22_vp, "dir2", &pn, &cred);
        ASSERT (error == EINVAL);
        check_vlinks();

	/*
	 * Rename /dir2/dir22 to /dir3.
	 */
	pn_set (&pn, "dir3");
        pn.pn_complen = strlen(pn.pn_buf);
        error = VOP_RENAME(dir2_vp, "dir22", root_vp, "dir3", &pn, &cred);
        ASSERT (! error);
        check_vlinks();


	/*
	 * Remove directory /dir2 - failure case.
	 */
	error = VOP_RMDIR(root_vp, "dir2", root_vp, &cred);
        ASSERT (error == EEXIST);
	check_vlinks();

	/*
	 * Remove the file /dir2/file6.
	 */
	error = VOP_REMOVE(dir2_vp, "file6", &cred);
        ASSERT (!error);
	check_vlinks();

	/*
 	 * Remove directory /dir2.
	 */
	error = VOP_RMDIR(root_vp, "dir2", root_vp, &cred);
	ASSERT (!error);

	check_vlinks();
	
	/*
	 * Create a directory under a link & then look up the
	 * directory in the original place.
	 */

	/*
	 * Release some of the vnodes.
	 */
	VN_RELE(root_vp);
	for (i=0; i<vp1_c; i++)
		VN_RELE(vp1);
	for (i=0; i<vp2_c; i++)
		VN_RELE(vp2);

	xfs_umount(mp);
	dev_close(si.ddev);
	exit(0);
}
