/*
 * Find out what file a block is in.
 *
 * If blockno is a datablock print the file it's in, or if it's free,
 * or if it's in an i-list, or if it's in the bitmap, or elsewhere, or invalid.
 *
 * 	efsbno /dev/r<blah> blockno
 */
static char ident[] = "@(#) efsbno.c $Revision: 1.7 $";

#include <fcntl.h>

#include "libefs.h"

static efs_daddr_t thebn;

void ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags);
void inex(int ne, extent *ex, int indir, struct efs_dent *dentp, char *dirname);

main(int argc, char **argv)
{
	char *devname;
	EFS_MOUNT *mp;
	struct efs *fs;
	int bmblock;

	if (argc != 3) {
		fprintf(stderr,"usage: %s /dev/r<blah> blockno\n", argv[0]);
		exit(1);
	}
	devname = argv[1];
	thebn = atoi(argv[2]);

	/* is thebn totally wacko? */
	if (thebn < 0) {
		printf("bn %d is totally wacko\n", thebn);
		exit(0);
	}

	if ((mp = efs_mount(devname, O_RDONLY)) == 0)
		exit(1);
	fs = mp->m_fs;

	/* deal with older EFS file systems */
	bmblock = fs->fs_bmblock ? fs->fs_bmblock : EFS_BITMAPBB;

	/* is bn not in a cg data area? */
	if (thebn >= bmblock &&
	    thebn < bmblock + BTOBB(fs->fs_bmsize)) {
		printf("bn %d is in bitmap\n", thebn);
		exit(0);
	}
	if (thebn < fs->fs_firstcg) {
		printf("bn %d is before first cg\n", thebn);
		exit(0);
	}
	if (thebn >= EFS_CGIMIN(fs, fs->fs_ncg)) {
		printf("bn %d is after the last data block\n", thebn);
		exit(0);
	}
	if (thebn < EFS_CGIMIN(fs, EFS_BBTOCG(fs, thebn)) + fs->fs_cgisize) {
		int cg = EFS_BBTOCG(fs, thebn);
		int ioff = (thebn - EFS_CGIMIN(fs, cg)) * EFS_INOPBB;
		efs_ino_t inum = cg * fs->fs_ipcg + ioff;
		printf("bn %d holds inodes %d - %d\n",
			thebn, (inum == 0 ? 2 : inum), inum + EFS_INOPBB - 1);
		exit(0);
	}

	/* is bn free? */
	if (efs_bget(mp))
		exit(1);
	if (tstfree(mp->m_bitmap, thebn)) {
		printf("bn %d is free\n", thebn);
		exit(0);
	}

	/*
	 * walk the name space
	 */
	efs_walk(mp, 2, ".", DO_RECURSE, ddent);
	efs_umount(mp);
}

void
ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags)
{
	struct efs_dinode *di = efs_figet(mp, EFS_GET_INUM(dentp));
	extent *ex;

	if (di == 0 || di->di_mode == 0 || di->di_numextents == 0)
		return;

	if (di->di_numextents > EFS_DIRECTEXTENTS) {
		inex(di->di_u.di_extents[0].ex_offset, di->di_u.di_extents, 1,
			dentp, dirname);
		inex(di->di_numextents,
		     ex = efs_getextents(mp, di, EFS_GET_INUM(dentp)),
		     0, dentp, dirname);
		free(ex);
	} else
		inex(di->di_numextents, di->di_u.di_extents, 0,
			dentp, dirname);
}

void
inex(int ne, extent *ex, int indir, struct efs_dent *dentp, char *dirname)
{
	for ( ; ne--; ex++)
		if (thebn >= ex->ex_bn && thebn < ex->ex_bn + ex->ex_length) {
			printf("%d %s/%.*s %s\n",
				EFS_GET_INUM(dentp),
				dirname,
				dentp->d_namelen, dentp->d_name,
				indir ? "(indirect extent)" : "");
			exit(0);
		}
}
