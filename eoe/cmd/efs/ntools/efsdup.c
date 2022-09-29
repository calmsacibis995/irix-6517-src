/*
 * Find dup blocks between inodes and inodes and inodes and free "list".
 * Read in bitmap and all inodes and extents and build a reverse index.
 */
static char ident[] = "@(#) efsdup.c $Revision: 1.10 $";

#include <fcntl.h>

#include "libefs.h"

void efsdup(EFS_MOUNT *mp);
int markfree(EFS_MOUNT *mp, efs_ino_t *btoi);
void marki(EFS_MOUNT *mp, struct efs_dinode *di, efs_ino_t inum,
	efs_ino_t *btoi);
void markex(int ne, extent *ex, efs_ino_t inum, efs_ino_t *btoi);
void prdup(efs_daddr_t bn, efs_ino_t i1, efs_ino_t i2);

efs_daddr_t maxbn;

main(int argc, char **argv)
{
	char *devname = argv[1];
	EFS_MOUNT *mp;

	if (argc != 2) {
		fprintf(stderr, "usage: %s refsdev\n", argv[0]);
		exit(1);
	}
	if ((mp = efs_mount(devname, O_RDONLY)) == 0)
		exit(1);
	efsdup(mp);
	efs_umount(mp);
	exit(0);
}

void
efsdup(EFS_MOUNT *mp)
{
	efs_ino_t *btoi;
	struct efs_dinode *di0, *di, *enddi;
	int cg;
	efs_ino_t inum;
	struct efs *fs = mp->m_fs;
	efs_daddr_t bn, endbn, lastbn, startbn;
	int len;

	/*
	 * allocate block array
	 */
	if ((btoi = (efs_ino_t*)calloc(fs->fs_size, sizeof(efs_ino_t))) == NULL) {
		fprintf(stderr,"malloc failed\n");
		exit(1);
	}

	/*
	 * read bitmap, mark free blocks in array as "inode 1"
 	 */
	printf("finding free blocks...");
	fflush(stdout);
	printf("%d blocks\n", markfree(mp, btoi));
	maxbn = EFS_CGIMIN(fs, fs->fs_ncg);

	/*
	 * foreach inode
	 *	check extents against array
	 */
	printf("checking inodes (looking for dups):\n");
	for (cg = 0; cg < fs->fs_ncg; cg++) {
		if ((di = di0 = efs_igetcg(mp, cg)) == 0)
			continue;
		enddi = di + fs->fs_cgisize * EFS_INOPBB;
		if (cg == 0) {
			inum = 2;
			di += 2;
		}
		for (; di < enddi; di++, inum++)
			if (di->di_mode && di->di_numextents)
				marki(mp, di, inum, btoi);
		free(di0);
	}

	printf("checking bitmap (looking for orphaned blocks):\n");
	for (cg = 0; cg < fs->fs_ncg; cg++) {
		bn = EFS_CGIMIN(fs, cg);
		endbn = bn + fs->fs_cgfsize;
		bn += fs->fs_cgisize;
		lastbn = 0;
		for ( ; bn < endbn; bn++) {
			if (btoi[bn] == 0) {
				if (lastbn == 0) {
					printf("%d+", bn);
					lastbn = bn;
					len = 1;
				} else if (lastbn + 1 == bn) {
					len++;
					lastbn++;
				} else {
					printf("%d\n", len);
					lastbn = len = 0;
				}
			}
		}
		if (lastbn)
			printf("%d\n", len);
	}
	free(btoi);
}

markfree(EFS_MOUNT *mp, efs_ino_t *btoi)
{
	int cg;
	efs_daddr_t bn, endbn;
	struct efs *fs = mp->m_fs;
	int nfree = 0;

	if (efs_bget(mp) == -1)
		exit(1);

	for (cg = 0; cg < fs->fs_ncg; cg++) {
		bn = EFS_CGIMIN(fs, cg);
		endbn = bn + fs->fs_cgfsize;
		bn += fs->fs_cgisize;
		for (; bn < endbn; bn++)
			if (tstfree(mp->m_bitmap, bn)) {
				btoi[bn] = 1; /* free blocks are "inode 1"'s */
				nfree++;
			}
	}
	return nfree;
}

void
marki(EFS_MOUNT *mp, struct efs_dinode *di, efs_ino_t inum, efs_ino_t *btoi)
{
	extent *ex;

	ex = efs_getextents(mp, di, inum);
	if (ex == NULL)
		return;
	if (di->di_numextents > EFS_DIRECTEXTENTS)
		markex(di->di_u.di_extents[0].ex_offset, di->di_u.di_extents,
			inum, btoi);
	markex(di->di_numextents, ex, inum, btoi);
	free(ex);
}

void
markex(int ne, extent *ex, efs_ino_t inum, efs_ino_t *btoi)
{
	for ( ; ne--; ex++) {
		efs_daddr_t bn = ex->ex_bn;
		int len = ex->ex_length;
		if (bn + len > maxbn) {
			printf("bn=%d len=%d > maxbn=%d i=%d\n",
				bn, len, maxbn, inum);
			return;
		}
		for ( ; len--; bn++)
			if (btoi[bn] != 0)
				prdup(bn, inum, btoi[bn]);
			else
				btoi[bn] = inum;
	}

}

void
prdup(efs_daddr_t bn, efs_ino_t i1, efs_ino_t i2)
{
	if (i2 == 1)	/* bitmap is "inode 1" in this program */
		printf("FREE bn=%d ino=%d\n", bn, i1);
	else
		printf("DUP bn=%d ino=%d ino=%d\n", bn, i1, i2);
}

#ifdef	BROKEN
prdup(efs_daddr_t bn, efs_ino_t i1, efs_ino_t i2)
{
	static lastbn;
	static lasti1;
	static lasti2;
	static startbn;
	static len;

	if (lastbn + 1 == bn && lasti1 == i1 && lasti2 == i2)
		len++;
	else if (startbn == 0) {
		startbn = bn;
		len = 1;
	} else {
		printf("%d+%d %d %d\n", startbn, len, lasti1, lasti2);
		startbn = 0;
	}

	lastbn = bn;
	lasti1 = i1;
	lasti2 = i2;
}
#endif
