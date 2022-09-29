/*
 *	@(#)dirprt.c	1.1 97/01/03 Connectathon Testsuite
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

DIR *my_opendir();

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

	dirp = my_opendir(dir);
	if (dirp == NULL) {
		perror(dir);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
#if defined(SVR3) || defined(SVR4)
		printf("%5d %5ld %5d %s\n", telldir(dirp), dp->d_ino,
		       dp->d_reclen, dp->d_name);
#else
		printf("%5d %5d %5d %5d %s\n", telldir(dirp), dp->d_fileno,
		       dp->d_reclen, dp->d_namlen, dp->d_name);
#endif
	}
	closedir(dirp);
}

#include <sys/stat.h>

/*
 * open a directory.
 */
DIR *
my_opendir(name)
	char *name;
{
	struct stat sb;

	if (stat(name, &sb) == -1) {
		printf("stat failed\n");
		return (NULL);
	}
	if ((sb.st_mode & S_IFMT) != S_IFDIR) {
		printf("not a directory\n");
		return (NULL);
	}
	printf("%s mode %o dir %o\n", name, sb.st_mode, S_IFDIR);
	return(opendir(name));
}
