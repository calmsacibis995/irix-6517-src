#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_itable.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

main(int argc, char **argv)
{
	int		count;
	int		fd;
	int		i;
	ino64_t		last;
	char		*name;
	int		nent;
	xfs_inogrp_t	*t;

	if (argc < 2)
		name = ".";
	else
		name = argv[1];
	fd = open(name, O_RDONLY);
	if (fd < 0) {
		perror(name);
		return 1;
	}
	if (argc < 3)
		nent = 1;
	else
		nent = atoi(argv[2]);
	t = malloc(nent * sizeof(*t));
	last = 0;
	while (syssgi(SGI_FS_INUMBERS, fd, &last, nent, t, &count) == 0) {
		if (count == 0)
			return 0;
		for (i = 0; i < count; i++) {
			printf("ino %10lld count %2d mask %016llx\n",
				t[i].xi_startino, t[i].xi_alloccount,
				t[i].xi_allocmask);
		}
	}
	perror("syssgi(SGI_FS_INUMBERS)");
	return 1;
}
