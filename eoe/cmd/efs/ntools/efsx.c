/*
 * Extract files from an EFS image.  (Kinda like "tar x..." for tar images).
 *
 * efsx -t [-v] -f efsdev [names...]			list
 * efsx -x [-v] -f efsdev [-d basedir] [names...]	extract
 * efsx -C [-v] -f efsdev [-d basedir] [names...]	compare
 */
static char ident[] = "@(#) efsx.c $Revision: 1.2 $";

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* MAXPATHLEN */
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>

#include "libefs.h"

extern int errno;

void args(int argc, char **argv);
void ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags);

char *progname;
char *dirname = ".";
efs_ino_t dirinum;
char *special;

char *basedir = "";	/* efs_walk calls ddent with "." already */

int vflag;
int noregfiles;

#define	DO_DIDDLY	0	 /* or is it DIDDLEY? */
#define	DO_EXTRACT	1
#define	DO_LIST		2
#define	DO_COMPARE	3

int dowhat;


main(int argc, char **argv)
{
	struct efs_mount *mp;
	char *bitmap;

	args(argc, argv);
	if ((mp = efs_mount(special, O_RDONLY)) == 0)
		exit(1);

	if (dowhat == DO_EXTRACT) {
		if (basedir[0])
			if (mkdir(basedir, 0755) == -1) {
				fprintf(stderr, "mkdir(%s) %s\n",
					basedir, strerror(errno));
				exit(1);
			}
		umask(0);
	}

	/*
	 * call 'ddent()' for each entry in the hierarchy starting
	 * at 'dirname'.
	 */
	if (dirinum == 0)
		dirinum = efs_namei(mp, dirname);
	efs_walk(mp, dirinum, dirname, DO_RECURSE, ddent);
}

void
args(int argc, char **argv)
{
        char c;
        extern char *optarg;
        extern int optind;


	progname = argv[0];

        while ((c = getopt(argc, argv, "txCvd:f:z")) != (char)-1)
                switch (c) {
                case '?':
		case 'h': goto usage;
		case 't': dowhat = DO_LIST;	break;
		case 'x': dowhat = DO_EXTRACT;	break;
		case 'C': dowhat = DO_COMPARE;	break;
                case 'v': vflag = 1;		break;
		case 'd': basedir = optarg;	break;
		case 'f': special = optarg;	break;
		case 'z': noregfiles = 1;
                }

        if (!special || dowhat == DO_DIDDLY) {
usage:
printf("This extracts|lists|compares files from an EFS file system image\n");
printf("usage: %s -x|-t|-C [-v] -f efsdev [dirname]\n",
	progname);
printf(" -h		this\n");
printf(" -v		verbose\n");
printf(" -C		compare\n");
printf(" -t		list only\n");
printf(" -f efsdev	block device or file of efs\n");
exit(1);
        }

	/* XXX parse list of names to extract.. if nothing extract everything */
	if (argc > optind) {
	}
}

/*
 * Given the name (dirname + efs_dent->d_name) find the efs_dinode and
 * either 1) extract (regular file), 2) mkdir (directory), 3) link, 4) symlink,
 * or 5) mknod (char, block, pipe).  socket?
 *
 * XXX hard link
 */
char buf[EFS_MAXEXTENTLEN * BBSIZE];

void
ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags)
{
        register struct efs_dinode *dip;
	extent *ext = 0;
	efs_ino_t inum = EFS_GET_INUM(dentp);
	char *fmt, *fmtmode();
	char path[MAXPATHLEN];

	/* efs_walk is breadth-first so we'll see the dir name before . */
	if (ISDOTORDOTDOT(dentp->d_name, dentp->d_namelen))
		return;

	if ((dip = efs_figet(mp, inum)) == 0)
		return;

	fmt = fmtmode(dip->di_mode);

	if (strlen(basedir) > 0) {
		strcpy(path, basedir);
		strcat(path, "/");
	} else
		path[0] = '\0';
	strcat(path, dirname);
	strcat(path, "/");
	strncat(path, dentp->d_name, dentp->d_namelen);

	if (!vflag) {
		printf("%c %s\n", fmt[0], path);
		if (dowhat == DO_LIST)
			return;
	}

	if ((dowhat == DO_EXTRACT && S_ISREG(dip->di_mode)) ||
	    S_ISLNK(dip->di_mode))
                if ((ext = efs_getextents(mp, dip, inum)) == 0)
			return;

	if (S_ISLNK(dip->di_mode)) {
		if (efs_readb(mp->m_fd, buf, ext->ex_bn, ext->ex_length) !=
								ext->ex_length)
			fprintf(stderr, "symlink efs_read(%s) %s\n",
							path, strerror(errno));
		buf[dip->di_size] = '\0';
		printf("%s %d %s -> %s\n", 
				fmt, dip->di_nlink, path, buf);
	} else if (S_ISCHR(dip->di_mode) || S_ISBLK(dip->di_mode)) {
		printf("%s %x %s\n", fmt, efs_getdev(&dip->di_u), path);
	} else {
		printf("%s %d %s\n", fmt, dip->di_nlink, path);
	}
	if (dowhat == DO_LIST)
		return;

	/* here only if dowhat == DO_EXTRACT */
        switch (dip->di_mode & S_IFMT) {
        case S_IFREG:
		mkfile(path, dip, mp->m_fd, ext);
		break;
        case S_IFDIR:
		if (mkdir(path, dip->di_mode))
			fprintf(stderr,"mkdir(%s) %s\n", path, strerror(errno));
		break;
        case S_IFCHR: 
        case S_IFBLK:
        case S_IFIFO:
		if (mknod(path, dip->di_mode, efs_getdev(&dip->di_u)))
			fprintf(stderr,"mknod(%s) %s\n", path, strerror(errno));
		break;
        case S_IFLNK:
		if (symlink(buf, path))
			fprintf(stderr, "symlink(%s) %s\n",
							path, strerror(errno));
		break;
        case S_IFSOCK:
		/*socket()?*/
		break;
        }

	if (!S_ISLNK(dip->di_mode)) {
		struct utimbuf ub;

		if (chown(path, dip->di_uid, dip->di_gid) == -1)
			fprintf(stderr,"fchown(%s,%d,%d) %s\n",
				path, dip->di_uid, dip->di_gid,
				strerror(errno));

        	ub.actime = dip->di_atime;
        	ub.modtime = dip->di_mtime;
		if (utime(path, &ub) == -1)
			fprintf(stderr,"utime(%s) %s\n", path, strerror(errno));
	}

	if (ext)
		free(ext);
}

mkfile(char *path, struct efs_dinode *di, int efsfd, extent *ext)
{
	int fd, numex;

	fd = open(path, O_WRONLY|O_CREAT, di->di_mode);
	if (fd == -1) {
		fprintf(stderr, "open(%s, O_WRONLY, %o) %s\n",
			path, di->di_mode, strerror(errno));
		return;
	}

	if (noregfiles) {
		close(fd);
		return;
	}

	for (numex = di->di_numextents; numex--; ext++) {
		if (efs_readb(efsfd, buf, ext->ex_bn, ext->ex_length) !=
							ext->ex_length) {
			fprintf(stderr, "efs_readb(%s bn=%d len=%d)\n",
				path, ext->ex_bn, ext->ex_length);
			return;
		}
		if (write(fd, buf, ext->ex_length * BBSIZE) !=
						ext->ex_length * BBSIZE) {
			fprintf(stderr, "write(%s bn=%d len=%d) %s\n",
				path, ext->ex_bn, ext->ex_length,
				strerror(errno));
			return;
		}
	}

	/* easier than sizing that last write just so */
	if (ftruncate(fd, di->di_size) == -1)
		fprintf(stderr,"fruncate(%s,%d) %s\n",
			path, di->di_size, strerror(errno));


	close(fd);
}
