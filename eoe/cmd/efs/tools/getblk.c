/*
 * Dump a disk block out to stdout
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/tools/RCS/getblk.c,v $
 * $Revision: 1.1 $
 * $Date: 1987/02/12 17:06:38 $
 */
#include "efs.h"

main(argc, argv)
	int argc;
	char *argv[];
{
	int fs_fd;
	char buf[BBSIZE];

	if (argc != 3) {
		fprintf(stderr, "usage: %s special block\n", argv[0]);
		exit(-1);
	}
	if ((fs_fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", argv[0], argv[1]);
		exit(-1);
	}
	lseek(fs_fd, BBTOB(atoi(argv[2])), 0);
	if (read(fs_fd, buf, BBSIZE) != BBSIZE) {
		fprintf(stderr, "%s: can't read block %d\n",
				argv[0], atoi(argv[2]));
		exit(-1);
	}
	write(1, buf, BBSIZE);
}
