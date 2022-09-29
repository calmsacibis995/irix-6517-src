/*
 * Dump out a directory in a structured manner.
 *
 * $Revision: 1.3 $
 */
#include <stdio.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/fs/efs_dir.h>

void
dumpblock(db, block)
	register struct efs_dirblk *db;
	int block;
{
	register struct efs_dent *dep;
	register int i;

	printf("Dirblk %d: magic=%x firstused=%d slots=%d\n",
		       block, db->magic, EFS_DIR_FREEOFF(db->firstused),
		       db->slots);
	if (EFS_DIR_FREEOFF(db->firstused) >
			   EFS_DIRBLK_HEADERSIZE + db->slots) {
		printf("Free space from %d to %d\n",
			     EFS_DIRBLK_HEADERSIZE + db->slots,
			     EFS_DIR_FREEOFF(db->firstused)-1);
	}
	printf("Slot Offset  Inum Length Name\n");
	for (i = 0; i < db->slots; i++) {
		printf("%4d %6d ", i, EFS_SLOTAT(db, i));
		if (EFS_SLOTAT(db, i) == 0) {
			printf("            <empty>\n");
			continue;
		}
		dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, i));
		printf("%5d %6d %.*s\n", EFS_GET_INUM(dep), dep->d_namelen,
			    dep->d_namelen, dep->d_name);
	}
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int fd;
	char buf[EFS_DIRBSIZE];
	int nb;
	int block;

	if (argc != 2) {
		fprintf(stderr, "usage: prdir directory\n");
		exit(-1);
	}
	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror("prdir: open");
		exit(-1);
	}

	block = 0;
	for (;;) {
		if ((nb = read(fd, buf, sizeof(buf))) != sizeof(buf)) {
			if (nb == 0)
				break;
			if (nb < 0) {
				perror("prdir: read");
				exit(-1);
			}
			printf("prdir: count=%d?\n", nb);
		}
		dumpblock((struct efs_dirblk *) buf, block);
		block += EFS_DIRBSIZE;
	}
}
