/*
 *	@(#)fstat.c	1.1 97/01/03 Connectathon Testsuite
 *	1.3 Lachman ONC Test Suite source
 *
 * test statfs for file count
 */
#define IRIX

#include <sys/param.h>
#ifdef SVR3
#include <sys/statfs.h>
#else
#if defined(SVR4) || defined(IRIX)
#include <sys/statvfs.h>
#else
#ifdef OSF1
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#endif
#endif
#include <stdio.h>

main(argc, argv)
	int argc;
	char *argv[];
{
#if defined(SVR4) || defined(IRIX)
	struct statvfs fs;
#else
	struct statfs fs;
#endif
	char *name = ".";

	if (argc > 2) {
		fprintf(stderr, "usage: %s [path]\n", argv[0]);
		exit(1);
	}
	if (argc == 2) {
		name = argv[1];
	}
	fs.f_files = 0;
	fs.f_ffree = 0;
#ifdef SVR3
	if (statfs(name, &fs, sizeof(fs), 0) < 0) {
#else
#if defined(SVR4) || defined(IRIX)
	if (statvfs(name, &fs) < 0) {
#else
	if (statfs(name, &fs) < 0) {
#endif
#endif
		perror(argv[1]);
		exit(1);
	}
	printf("inodes %d free %d\n", (int)fs.f_files, (int)fs.f_ffree);
	exit(0);
}
