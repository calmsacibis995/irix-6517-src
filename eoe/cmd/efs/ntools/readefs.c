/*
 * Read the contents of the file straight out of the file system device
 * and write it on stdout.
 *
 * 	readefs file
 *	readefs /dev/r<blah> inum
 */
static char ident[] = "@(#) read.c $Revision: 1.3 $";

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "libefs.h"

char buf[EFS_MAXEXTENTLEN*BBSIZE];

main(int argc, char **argv)
{
        struct stat sb;
	struct efs_dinode *di;
	struct efs_mount *mp;
	extent *ex;
	char *devname;
	efs_ino_t inum;
	int ne;

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
	for (ne = di->di_numextents; ne--; ex++) {
		if (efs_rdwrb(SGI_READB, mp->m_fd, buf, ex->ex_bn,
			ex->ex_length) != ex->ex_length) {
			fprintf(stderr,"failed to read bn=%d len=%d errno=%d\n",
				ex->ex_bn, ex->ex_length, errno);
			exit(1);
		}
		/* XXX should really only write last part of last block */
		write(1, buf, ex->ex_length * BBSIZE);
	}
	exit(0);
}
