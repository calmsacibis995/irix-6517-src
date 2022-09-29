#ident "$Revision: 1.7 $"

/*
 * Clear an efs inode.
 *
 * Written by: Kipp Hickman
 */

#include "libefs.h"
#include <diskinfo.h>

char *progname;

static int isdecimal(char *);

int
main(int argc, char **argv)
{
	int i;
	efs_ino_t inum;
	struct efs_dinode *di;
	int error = 0;
	ulong igen;
	EFS_MOUNT *mp;
	char *rawpath;

	progname = argv[0];
	if (argc < 3) {
		fprintf(stderr, "usage: %s filsys inumber ...\n", progname);
		exit(1);
	}

	/*
	 * Make sure that the provided file is a character disk device.
         */
	if ((rawpath = findrawpath(argv[1])) == NULL) {
		fprintf(stderr, "%s: %s is not a raw filesystem name.\n",
			progname, argv[1]);
		exit(1);
	}

	if ((mp = efs_mount(rawpath, O_RDWR)) == NULL) {
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
				progname, argv[1]);
		exit(1);
	}

	/*
	 * Scan arguments and check for legitimacy
	 */
	for (i = 2; i < argc; i++) {
		if (!isdecimal(argv[i])) {
			fprintf(stderr, "%s: %s is not a number\n",
				progname, argv[i]);
			error = 1;
			continue;
		}
		inum = atoi(argv[i]);
		if ((inum <= 0) ||
		    (inum >= mp->m_fs->fs_ncg * mp->m_fs->fs_cgisize *
                             EFS_INOPBB)) {
			fprintf(stderr,
				"%s: %s is not a valid inode number\n",
				progname, argv[i]);
			error = 1;
			continue;
		}
	}
	if (error)
		exit(1);

	/*
	 * Now run through argument list and clear the inodes
	 */

	for (i = 2; i < argc; i++) {
		inum = atoi(argv[i]);
		if ((di = efs_iget(mp, inum)) == NULL) {
			fprintf(stderr, 
				"%s: Couldn't read inode %d, continuing...\n",
				progname, inum);
			continue;
		}
		igen = di->di_gen;
		bzero((char *) di, sizeof *di);
		di->di_gen = igen + 1;
		if (efs_iput(mp, di, inum) == -1) {
			fprintf(stderr, 
				"%s: Couldn't write inode %d, continuing...\n",
				progname, inum);
		}
	}
	exit(error);
	/* NOTREACHED */
}

/*
 * Return non-zero if the given string is a decimal number
 */
static int
isdecimal(char *s)
{
	char c;

	while (c = *s++)
		if(c < '0' || c > '9')
			return (0);
	return (1);
}

/*
 * Print out a nice message when an i/o error occurs
 */
void
error(void)
{
	perror(progname);
	exit(1);
}
