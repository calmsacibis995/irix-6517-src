/*
 * setmnt(1m) - initialize the mounted-filesystem table
 *
 * author: Brendan Eich
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/setmnt/RCS/setmnt.c,v $
 * $Revision: 1.8 $
 * $Date: 1995/08/28 01:16:30 $
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include <stdio.h>
#include <mntent.h>
#include <errno.h>

char	line[2*(MNTMAXSTR+1)];
char	filesys[MNTMAXSTR] = "";
char	*fsname = filesys;
char	dir[MNTMAXSTR];
char	opts[MNTMAXSTR];

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *mtab;
	register FILE *mtabp;

	if (argc == 3 && strcmp(argv[1], "-f") == 0) {
		mtab = argv[2];
	} else {
		if (argc != 1) {
			fprintf(stderr, "usage: setmnt [-f file]\n");
			return -1;
		}
		mtab = MOUNTED;
	}

	if ((mtabp = setmntent(mtab, "w")) == NULL) {
		fprintf(stderr, "setmnt: cannot open ");
		perror(mtab);
		return errno;
	}
	while (gets(line) != NULL) {
		struct statvfs64 sfsb;
		struct mntent mnt;
		char *raw, *ro, *rawname();

		if (sscanf(line, "%s %s\n", fsname, dir) != 2) {
			fprintf(stderr, "setmnt: malformed input %s.\n",
			    line);
			continue;
		}

		/*
		 * Get filesystem type name, readonly option.
		 */
		if (statvfs64(dir, &sfsb) < 0) {
			perror(dir);
			continue;
		}
		ro = (sfsb.f_flag & ST_RDONLY) ? MNTOPT_RO : MNTOPT_RW;

		/*
		 * Try to find the raw device name, to set the raw option.
		 */
		raw = rawname(fsname);
		if (raw == NULL) {
			strcpy(opts, ro);
		} else {
			(void) sprintf(opts, "%s,%s=%s", ro, MNTOPT_RAW, raw);
		}

		/*
		 * Add an mtab entry for filesys.
		 */
		mnt.mnt_fsname = filesys;
		mnt.mnt_dir = dir;
		mnt.mnt_type = sfsb.f_basetype;
		mnt.mnt_opts = opts;
		mnt.mnt_freq = 0;
		mnt.mnt_passno = 0;
		if (addmntent(mtabp, &mnt)) {
			fprintf(stderr, "setmnt: can't add entry to ");
			perror(mtab);
			return errno;
		}
	}
	endmntent(mtabp);
	return 0;
}

char *
rawname(fsname)
	char *fsname;	/* full path for SVR3, filename in /dev for SVR0 */
{
	struct stat stb;
	static char rawdev[MNTMAXSTR];

	if (sscanf(fsname, "/dev/dsk/%s", rawdev + 10) == 1)
		strncpy(rawdev, "/dev/rdsk/", 10);
	else if (sscanf(fsname, "/dev/%s", rawdev + 6) == 1)
		strncpy(rawdev, "/dev/r", 6);
	else if (sscanf(fsname, "/dev/dsk/xlv/%s", rawdev + 14) == 1)
		strncpy(rawdev, "/dev/rdsk/xlv/", 14);
	else
		rawdev[0] = '\0';
	if (stat(rawdev, &stb) < 0 || (stb.st_mode & S_IFMT) != S_IFCHR)
		return NULL;
	return rawdev;
}
