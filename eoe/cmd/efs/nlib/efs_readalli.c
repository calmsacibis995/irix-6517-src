#ident "$Revision: 1.3 $"

/*
 * Read in all the file system's inodes, dirblks and indirect extents and
 * hooks them the to the efs_dinode using DIDATA (see libefs.h).
 */
#include "libefs.h"

/* debugging counters */
static int ifind;	/* no. files with indirect extents */
static int ifreg, ifdir, iflnk, ififo, ifchr, ifblk, ifsock;
static int blksreg, blksdir, blkslnk, blksind;

static void debugpr(void);

efs_ino_t
efs_readinodes(EFS_MOUNT *mp, FILE *progress, int *blkcount, int debug)
{
	struct efs_dinode *di0, *dp;
	efs_ino_t inum, lasti;
	extent *ex;
	int nblocks;
	int cg;
	int usedinodes = CAPACITYINODES(mp->m_fs) - mp->m_fs->fs_tinode;
	int readinodes = 0, progressinodes = 0, nextval = 10;

	if (progress) {
		fprintf(progress, "reading %d inodes..", usedinodes);
		fflush(progress);
	}

	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++) {
		if ((di0 = efs_figet(mp, cg * mp->m_fs->fs_ipcg)) == 0)
			return 0;
		if (cg == 0) { /* first inode is inode 2 */
			inum = 2;
			lasti = mp->m_fs->fs_ipcg;
			dp = di0 + 2;
		} else {
			inum = cg * mp->m_fs->fs_ipcg;
			lasti = inum + mp->m_fs->fs_ipcg;
			dp = di0;
		} 
		for ( ; inum < lasti; dp++, inum++) {
			if (dp->di_mode == 0) {
				continue;
			}
			if (debug) {
                		switch (dp->di_mode & IFMT) {
                		case IFREG:     ifreg++;        break;
                		case IFDIR:     ifdir++;        break;
                		case IFLNK:     iflnk++;        break;
                		case IFIFO:     ififo++;        break;
                		case IFCHR:     ifchr++;        break;
                		case IFBLK:     ifblk++;        break;
                		case IFSOCK:    ifsock++;       break;
                		default:
                        		fprintf(stderr,	
						"unknown mode 0x%x inum=%d\n",
                                		dp->di_mode & IFMT, inum);
                		}
			}

			if (dp->di_numextents > EFS_DIRECTEXTENTS) {
				ex = efs_getextents(mp, dp, inum);
				*blkcount +=
					DIDATA(dp, didata *)->indirblks =
 					INDIRBLKS(dp->di_numextents);
				DIDATA(dp, didata *)->ex = ex;

				if (debug) {
					ifind++;
					blksind += INDIRBLKS(dp->di_numextents);
				}
			} else {
				ex = dp->di_u.di_extents;
			}

			if (dp->di_size > 0) {
				nblocks = DATABLKS(ex, dp->di_numextents);
				*blkcount += nblocks;
			} else {
				nblocks = 0;
			}

			if (debug) {
				if (S_ISREG(dp->di_mode))
					blksreg += nblocks;
				else if (S_ISLNK(dp->di_mode))
					blkslnk += nblocks;
			}

			if (S_ISDIR(dp->di_mode)) {
				/* A directory must have > 0 blocks! */
				DIDATA(dp, didata *)->dirblkp =
					efs_getdirblks(mp->m_fd, dp, ex, inum);
				DIDATA(dp, didata *)->dirblocks = nblocks;

				if (debug) {
					blksdir += nblocks;
				}
			}

			if (progress) {
				progressinodes++;
				if (usedinodes / progressinodes < 10) {
					fprintf(progress, "%d%%..", nextval);
					fflush(progress);
					progressinodes = 0;
					nextval += 10;
				}
			}
			++readinodes;
			if (readinodes == usedinodes) {
				if (progress) {
					fprintf(progress, "done\n");
					fflush(progress);
				}

				if (debug) {
					debugpr();
				}
				return inum;
			}
		}
	}
	/* should never get here (in a consistent file system) */
	if (debug) {
		debugpr();
	}
	return 0;
}

static void
debugpr(void)
{
printf(
"   ifreg    ifdir    iflnk    ififo    ifchr    ifblk   ifsock ifind   itotal\n");
printf("%8d %8d %8d %8d %8d %8d %8d %5d %8d\n",
ifreg, ifdir, iflnk, ififo, ifchr, ifblk, ifsock, ifind,
ifreg + ifdir + iflnk + ififo + ifchr + ifblk + ifsock);

printf(
" blksreg  blksdir  blkslnk                                     bkind  blksall\n");
printf("%8d %8d %8d                                     %5d %8d\n",
blksreg, blksdir, blkslnk, blksind,
blksreg + blksdir + blkslnk + blksind);
}
