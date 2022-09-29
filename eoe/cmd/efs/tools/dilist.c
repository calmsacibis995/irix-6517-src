/*
 * Print all inodes for the specified device
 *
 * dilist /dev/r<foo>
 */
static char ident[] = "@(#) dilist.c $Revision: 1.3 $";

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "../libefs/libefs.h"

void dcg(EFS_MOUNT *mp, int cg);

main(int argc, char **argv)
{
        struct stat sb;
	struct efs_mount *mp;
	char *efsdev;
	int cg;

        if (argc != 2) {
                fprintf(stderr, "usage: %s /dev/r<efsdev>\n", argv[0]);
                exit(1);
        }
	if ((mp = efs_mount(argv[1], O_RDONLY)) == 0)
		exit(1);
	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++)
		dcg(mp, cg);
	efs_umount(mp);
}

void
dcg(EFS_MOUNT *mp, int cg)
{
	struct efs_dinode *di0, *di;
	efs_ino_t inum, lasti;
	extent *ex;

	inum = cg * mp->m_fs->fs_ipcg;
	lasti = inum + mp->m_fs->fs_ipcg;
	if ((di0 = efs_igetcg(mp, cg)) == 0)
		return;
	for (di = di0 ; inum < lasti; di++, inum++) {
		if (di->di_mode == 0)
			continue;
		if ((ex = efs_getextents(mp, di, inum)) == 0)
			continue;
		efs_prino(stdout, mp, inum, di, ex, 0, 0);
		free(ex);
	}
	free(di0);
}
