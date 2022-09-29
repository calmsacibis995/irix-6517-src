/*
 *	@(#)stat.c	1.1 97/01/03 Connectathon Testsuite
 *	1.4 Lachman ONC Test Suite source
 *
 * stat all of the files in a directory tree
 */
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#ifdef SVR3
#include	<sys/fs/nfs/time.h>
#else
#include	<sys/time.h>
#endif
#ifdef use_directs
#include	<sys/dir.h>
#else
#include	<dirent.h>
#endif

int stats = 0;
int readdirs = 0;

void statit();

main(argc, argv)
	int argc;
	char *argv[];
{
	struct timeval stim, etim;
	float elapsed;

	if (argc != 2) {
		fprintf(stderr, "usage: %s dir\n", argv[0]);
		exit(1);
	}

	gettimeofday(&stim, 0);
	statit(argv[1]);
	gettimeofday(&etim, 0);
	elapsed = (float) (etim.tv_sec - stim.tv_sec) +
	    (float)(etim.tv_usec - stim.tv_usec) / 1000000.0;
	fprintf(stdout, "%d calls in %f seconds (%f calls/sec)\n",
	    stats, elapsed, (float)stats / elapsed);
	exit(0);
}

void
statit(name)
	char *name;
{
	struct stat statb;
#ifdef use_directs
	struct direct *di;
#else
	struct dirent *di;
#endif
	DIR *dirp;
	long loc;

#ifdef SVR3
	if (stat(name, &statb) < 0) {
#else
	if (lstat(name, &statb) < 0) {
#endif
		perror(name);
	}
	if ((statb.st_mode & S_IFMT) != S_IFDIR) {
		return;
	}

	if ((dirp = opendir(name)) == NULL) {
		perror(name);
		return;
	}
	stats++;
	chdir(name);

	while ((di = readdir(dirp)) != NULL) {
		if (strcmp(di->d_name, ".") == 0 || strcmp(di->d_name, "..") == 0)
		    continue;
#ifdef SVR3
		if (stat(di->d_name, &statb) < 0) {
#else
		if (lstat(di->d_name, &statb) < 0) {
#endif
			perror(di->d_name);
		}
		stats++;
		if ((statb.st_mode & S_IFMT) == S_IFDIR) {
			loc = telldir(dirp);
			closedir(dirp);
			statit(di->d_name);
			if ((dirp = opendir(".")) == NULL) {
				perror(name);
				chdir("..");
				return;
			}
			seekdir(dirp, loc);
		}
	}
	chdir("..");
	return;
}
