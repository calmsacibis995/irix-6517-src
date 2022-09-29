#ident "$Revision: 1.9 $"

/*
 * peruse EFS directories
 */
#include "libefs.h"

/*
 * Create the EFS_DIR for inum (which requires reading the efs_dinode,
 * the extents and the dirblks).
 */
EFS_DIR *
efs_opendiri(EFS_MOUNT *mp, efs_ino_t inum)
{
	EFS_DIR *dirp;
	struct efs_dinode *dip;
	extent *ex;
	char *bp;

	if ((dip = efs_figet(mp, inum)) == 0)
		return 0;
	if ((ex = efs_getextents(mp, dip, inum)) == 0)
		return 0;
	if ((bp = efs_getdirblks(mp->m_fd, dip, ex, inum)) == 0)
		return 0;
	free(ex);
	if ((dirp = efs_opendirb(bp, DATABLKS(ex, dip->di_numextents))) == 0)
		return 0;
	dirp->dir_mp = mp;
	return dirp;
}

EFS_DIR *
efs_opendir(EFS_MOUNT *mp, char *filename)
{
	return efs_opendiri(mp, efs_namei(mp, filename));
}

/*
 * Given nblocks of dirblks at dp make a EFS_DIR
 */
EFS_DIR *
efs_opendirb(char *dp, int nblocks)
{
	EFS_DIR *dirp;

        if ((dirp = malloc(sizeof(EFS_DIR))) == NULL) {
		fprintf(stderr,"malloc failed in efs_opendirb()\n");
		return 0;
	}

	dirp->dir_curblk = 0;
	dirp->dir_curslot = 0;
	dirp->dir_mp = 0;
	dirp->dir_blkp = dp;	/* XXX call better not call efs_closedir */
	dirp->dir_nblocks = nblocks;

	return dirp;
}

struct efs_dent *
efs_readdir(EFS_DIR *dirp)
{
	struct efs_dirblk *dp;
	struct efs_dent *dentp;

nextblock:
	if (dirp->dir_curblk >= dirp->dir_nblocks)
		return NULL;

	dp = ((struct efs_dirblk*)dirp->dir_blkp) + dirp->dir_curblk;

	if (dp->magic != EFS_DIRBLK_MAGIC) { /* XXX */
		fprintf(stderr,"bad magic: %d dirblk=%d, skipping...\n",
			dp->magic, dirp->dir_curblk);
		dirp->dir_curblk++;
		goto nextblock;
	}

	if (dirp->dir_curslot >= dp->slots) {
		dirp->dir_curslot = 0;
		dirp->dir_curblk++;
		goto nextblock;
	}

	while (EFS_SLOTAT(dp, dirp->dir_curslot) == 0) {
		dirp->dir_curslot++;
		if (dirp->dir_curslot >= dp->slots)
			goto nextblock;
	}

	dentp = EFS_SLOTOFF_TO_DEP(dp, EFS_SLOTAT(dp, dirp->dir_curslot));
	dirp->dir_curslot++;

	return dentp;
}

/*
 * The other half of efs_opendiri,opendir()
 */
void
efs_closedir(EFS_DIR *dirp)
{
	free(dirp->dir_blkp);
	free(dirp);
}

void
efs_rewinddir(EFS_DIR *dirp)
{
	dirp->dir_curslot = dirp->dir_curblk = 0;
}


char *
efs_getdirblks(int fd, struct efs_dinode *dp, extent *ex, efs_ino_t inum)
{
	int i, nblocks = DATABLKS(ex, dp->di_numextents);
	char *retbp, *bp;

	if ((retbp = bp = malloc(nblocks * EFS_BBSIZE)) == 0) {
		return 0;
	}

	for (i = 0; i < dp->di_numextents; i++, ex++) {
		if (efs_readb(fd, bp, ex->ex_bn, ex->ex_length) !=
			ex->ex_length) {
			fprintf(stderr,
			    "efs_getblocks(i%d) efs_readb(%d+%d) failed: %s\n",
				inum, ex->ex_bn, ex->ex_length,
				strerror(errno));
			return 0;
		}
		bp += ex->ex_length * EFS_BBSIZE;
	}
	return retbp;
}
