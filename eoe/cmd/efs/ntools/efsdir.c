
#include "libefs.h"

char buf[100*BBSIZE]; /* XXX 100 is a quick and dirty reasonable upper bound */

main(int argc, char **argv)
{
	EFS_DIR *dirp;
	char *bp;
	struct efs_dent *dentp;

	if (argc != 1) {
		fprintf(stderr,"usage: %s < dir-image\n", argv[0]);
		exit(1);
	}

	for (bp = buf; read(0, bp, BBSIZE) == BBSIZE; bp += BBSIZE)
		;

	dirp = efs_opendirb(buf, (bp - buf)/BBSIZE);

	while (dentp = efs_readdir(dirp))
		printf("%8d %.*s\n",
			EFS_GET_INUM(dentp), 
			dentp->d_namelen,
			dentp->d_name);
}
