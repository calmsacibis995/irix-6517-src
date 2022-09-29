/*
 * Print extent info for the specified file.
 *
 * 	prex file
 *	prex /dev/r<blah> inum
 */
static char ident[] = "@(#) prex.c $Revision: 1.2 $";

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "../libefs/libefs.h"

main(int argc, char **argv)
{
        struct stat sb;
	struct efs_dinode *di;
	struct efs_mount *mp;
	extent *ex;
	char *devname;
	efs_ino_t inum;

        if (argc != 2 && argc != 3) {
                fprintf(stderr, "usage: %s file\n", argv[0]);
                fprintf(stderr, "usage: %s /dev/r<blah> inum\n", argv[0]);
                exit(1);
        }

	if (argc == 2) {
		if (stat(argv[1], &sb) == -1) {
			perror("stat");
			exit(1);
		}
		devname = devnm(sb.st_dev);
		inum = sb.st_ino;
	} else {
		devname = argv[1];
		inum = atoi(argv[2]);
	}
	if ((mp = efs_mount(devname, O_RDONLY)) == 0)
		exit(1);
	if ((di = efs_iget(mp, inum)) == 0)
		exit(1);
	if ((ex = efs_getextents(mp, di, inum)) == 0)
		exit(1);
	efs_prino(stdout, mp, inum, di, ex, 0, 0);
}
