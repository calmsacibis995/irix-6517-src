/*
 * movex - command interface to movex()
 *
 * movex offset tobn len filename
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "libefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

extern int errno;

char    *progname;

main(int argc, char **argv)
{
        struct stat statbuf;
	struct fscarg fa;
	off_t offset;
	efs_daddr_t tobn;
	int len;
	char *devname, *filename;
	struct efs_mount *mp;

        progname = argv[0];

        if (argc != 5) {
                fprintf(stderr, "usage: %s offset tobn len\n", progname);
                exit(1);
        }
	offset = atoi(argv[1]);
	tobn = atoi(argv[2]);
	len = atoi(argv[3]);
	filename = argv[4];

	if (stat(filename, &statbuf) == -1) {
		perror("stat");
		exit(1);
	}

	efs_init(printf);

	devname = devnm(statbuf.st_dev);
        mp = efs_mount(devname, O_RDWR);

        if (fsc_init(printf, 1, 1)) {
		fprintf(stderr, "%s: fsc_init() failed e=%d\n", progname,errno);
		exit(1);
	}

	fa.fa_dev = statbuf.st_dev;
	fa.fa_ino = statbuf.st_ino;

	if (fsc_ilock(&fa) == EBUSY) {
		printf("%s: ino=%d in use\n", progname, fa.fa_ino);
		exit(0);
	}
	getex(mp, &fa);

	if (movex(mp, &fa, offset, tobn, len) == -1) {
		fprintf(stderr,"%s: movex failed\n", progname);
		exit(1);
	}

	if (fsc_iunlock(&fa))
		exit(1);

	exit(0);
}
