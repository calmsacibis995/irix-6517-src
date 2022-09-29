/*
 *	@(#)dirdmp.c	1.1 97/01/03 Connectathon Testsuite
 *	1.2 Lachman ONC Test Suite source
 */

#include <sys/param.h>
#ifndef major
#include <sys/types.h>
#endif
#ifdef use_directs
#include <sys/dir.h>
#else
#include <dirent.h>
#endif
#include <stdio.h>
#include <ctype.h>

#ifdef OSF1
#define dd_bsize dd_bufsiz
#define dd_bbase dd_seek	/* Totally bogus, but it works */
#endif

main(argc, argv)
	int argc;
	char *argv[];
{
	argv++;
	argc--;
	while (argc--) {
		print(*argv++);
	}
}

print(dir)
	char *dir;
{
	DIR *dirp;
#ifdef use_directs
	struct direct *dp;
#else
	struct dirent *dp;
#endif
	int rec = 0;

	dirp = opendir(dir);
	if (dirp == NULL) {
		perror(dir);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) ;
	closedir(dirp);
}

#include <sys/stat.h>

/*
 * open a directory.
 */
DIR *
opendir(name)
#if defined(__STDC__)
	const char *name;
#else
	char *name;
#endif
{
	register DIR *dirp;
	register int fd;
	struct stat sb;
	extern char *malloc();

	if ((fd = open(name, 0)) == -1) {
		printf("open failed\n");
		return (NULL);
	}
	if (fstat(fd, &sb) == -1) {
		printf("stat failed\n");
		return (NULL);
	}
	/*
	if ((sb.st_mode & S_IFMT) != S_IFDIR) {
		printf("not a directory\n");
		return (NULL);
	}
	*/
	printf("%s mode %o dir %o\n", name, sb.st_mode, S_IFDIR);
	if (((dirp = (DIR *)malloc(sizeof(DIR))) == NULL) ||
#if defined(SVR3) || defined(SVR4)
	    ((dirp->dd_buf = malloc(DIRBUF)) == NULL)) {
#else
	    ((dirp->dd_buf = malloc((int)sb.st_blksize)) == NULL)) {
#endif
		if (dirp) {
			if (dirp->dd_buf) {
				free(dirp->dd_buf);
			}
			free(dirp);
		}
		close(fd);
		return (NULL);
	}
#if !defined(SVR3) && !defined(SVR4)
	dirp->dd_bsize = sb.st_blksize;
#if !defined(SUNOS4X)
	dirp->dd_bbase = 0;
#if !defined(OSF1)
	dirp->dd_entno = 0;
#endif
#endif
#endif
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	return (dirp);
}

/*
 * get next entry in a directory.
 */
#ifdef use_directs
struct direct *
#else
struct dirent *
#endif
readdir(dirp)
	register DIR *dirp;
{
#ifdef use_directs
	register struct direct *dp;
#else
	register struct dirent *dp;
#endif
	static int header;	/* 1 after header prints */

	for (;;) {
		if (dirp->dd_loc == 0) {
#if defined(SVR3) || defined(SVR4)
			dirp->dd_size = getdents(dirp->dd_fd,
#if defined(__STDC__)
			    (struct dirent *) dirp->dd_buf, DIRBUF);
#else
			    dirp->dd_buf, DIRBUF);
#endif
#else
#if defined(SUNOS4X)
			dirp->dd_size = getdents(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_bsize);
#else
			dirp->dd_size = getdirentries(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_bsize, &dirp->dd_bbase);
#endif
#endif
			if (dirp->dd_size < 0) {
				perror("getdirentries");
				return (NULL);
			}
			if (dirp->dd_size == 0) {
				printf("EOF\n");
				return (NULL);
			}
#if !defined(SVR3) && !defined(SVR4) && !defined(SUNOS4X) && !defined(OSF1)
			dirp->dd_entno = 0;
#endif
		}
		if (dirp->dd_loc >= dirp->dd_size) {
			printf("EOB offset %d\n", tell(dirp->dd_fd));
			dirp->dd_loc = 0;
			header = 0;
			continue;
		}
#ifdef use_directs
		dp = (struct direct *)(dirp->dd_buf + dirp->dd_loc);
#else
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
#endif
		if (dp->d_reclen <= 0) {
			printf("0 reclen\n");
			return (NULL);
		}
#if defined(SVR3) || defined(SVR4)
		if (!header) {
			header = 1;
			printf("  loc    ino reclen name\n");
		}
		printf("%5d %6ld %6d %s\n",
		    dirp->dd_loc, dp->d_ino, dp->d_reclen,
		    dp->d_name);
#else
		if (!header) {
			header = 1;
			printf("  loc fileno reclen namelen name\n");
		}
		printf("%5d %6d %6d %7d %s\n",
		    dirp->dd_loc, dp->d_fileno, dp->d_reclen,
		    dp->d_namlen, dp->d_name);
#endif
		dirp->dd_loc += dp->d_reclen;
#if !defined(SVR3) && !defined(SVR4) && !defined(SUNOS4X) && !defined(OSF1)
		dirp->dd_entno++;
#endif
#if defined(SVR3) || defined(SVR4)
		if (dp->d_ino == 0) {
#else
		if (dp->d_fileno == 0) {
#endif
			continue;
		}
		return (dp);
	}
}
