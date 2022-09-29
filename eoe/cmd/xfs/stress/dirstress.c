

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/major.h>


static void
dirstress(
	char	*dirname,
	int	dirnum,
	int	nfiles,
	int	keep);

static void
create_entries(
	int	nfiles);

static void
scramble_entries(
	int	nfiles);

static void
remove_entries(
	int	nfiles);



int
main(
	int	argc,
	char	*argv[])
{
	char	*dirname;
	int	nprocs;
	int	nfiles;
	int	c;
	int	errflg;
	int	i;
	long	seed;
	int	childpid;
	int	nprocs_per_dir;
	int	keep;

	errflg = 0;
	dirname = NULL;
	nprocs = 4;
	nfiles = 100;
	seed = getpid();
	nprocs_per_dir = 1;
	keep = 0;
	while ((c = getopt(argc, argv, "d:p:f:s:n:k")) != EOF) {
		switch(c) {
			case 'p':
				nprocs = atoi(optarg);
				break;
			case 'f':
				nfiles = atoi(optarg);
				break;
			case 'n':
				nprocs_per_dir = atoi(optarg);
				break;
			case 'd':
				dirname = optarg;
				break;
			case 's':
				seed = strtol(optarg, NULL, 0);
				break;
			case 'k':
				keep = 1;
				break;
			case '?':
				errflg++;
				break;
		}
	}
	if (errflg || (dirname == NULL)) {
		printf("Usage: dirstress [-d dir] [-p nprocs] [-f nfiles] [-k]\n");
		exit(0);
	}

	printf("Using seed %d\n", seed);
	srandom(seed);

	for (i = 0; i < nprocs; i++) {
		childpid = fork();
		if (childpid < 0) {
			perror("Fork failed");
			exit(errno);
		}
		if (childpid == 0) {
			/* child */
			dirstress(dirname, i / nprocs_per_dir, nfiles, keep);
			exit(0);
		}
	}
	while (wait((int*)0) != -1) {
		/*
		 * Just wait for all the children to finish.
		 */
		;
	}
	printf("INFO: Dirstress complete\n");
	return 0;
}



void
dirstress(
	char	*dirname,
	int	dirnum,
	int	nfiles,
	int	keep)
{
	int		error;
	char		buf[1024];

	sprintf(buf, "%s/stressdir", dirname);
	error = mkdir(buf, 0777);
	if (error && (errno != EEXIST)) {
		perror("Create stressdir directory failed");
		return;
	}

	error = chdir(buf);
	if (error) {
		perror("Cannot chdir to main directory");
		return;
	}

	sprintf(buf, "stress.%d", dirnum);
	error = mkdir(buf, 0777);
	if (error && (errno != EEXIST)) {
		perror("Create pid directory failed");
		return;
	}

	error = chdir(buf);
	if (error) {
		perror("Cannot chdir to dirnum directory");
		return;
	}

	create_entries(nfiles);

	scramble_entries(nfiles);

	if (!keep)
		remove_entries(nfiles);

	error = chdir("..");
	if (error) {
		perror("Cannot chdir out of pid directory");
		return;
	}

	if (!keep) {
		sprintf(buf, "stress.%d", dirnum);
		(void) rmdir(buf);
	}
	
	error = chdir("..");
	if (error) {
		perror("Cannot chdir out of working directory");
		return;
	}

	if (!keep)
		(void) rmdir("stressdir");

	return;
}

void
create_entries(
       int	nfiles)
{
	int	i;
	int	fd;
	char	buf[1024];

	for (i = 0; i < nfiles; i++) {
		sprintf(buf, "XXXXXXXXXXXX.%d", i);
		switch (i % 4) {
		case 0:
			/*
			 * Create a file
			 */
			fd = creat(buf, 0666);
			if (fd > 0) {
				close(fd);
			}
			break;
		case 1:
			/*
			 * Make a directory.
			 */
			(void) mkdir(buf, 0777);

			break;
		case 2:
			/*
			 * Make a symlink
			 */
			(void) symlink(buf, buf);
			break;
		case 3:
			/*
			 * Make a dev node
			 */
			(void) mknod(buf, S_IFCHR | 0666,
				     ((ZERO_MAJOR << 18) | 32));
			break;
		default:
			break;
		}
	}
}


void
scramble_entries(
	int	nfiles)
{
	int		i;
	char		buf[1024];
	char		buf1[1024];
	long		r;
	int		fd;

	for (i = 0; i < nfiles * 2; i++) {
		switch (i % 5) {
		case 0:
			/*
			 * rename two random entries
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%d", r);
			r = random() % nfiles;
			sprintf(buf1, "XXXXXXXXXXXX.%d", r);

			(void) rename(buf, buf1);
			break;
		case 1:
			/*
			 * unlink a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%d", r);
			(void) unlink(buf);
			break;
		case 2:
			/*
			 * rmdir a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%d", r);
			(void) rmdir(buf);
			break;
		case 3:
			/*
			 * create a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%d", r);

			fd = creat(buf, 0666);
			if (fd > 0) {
				close(fd);
			}
			break;
		case 4:
			/*
			 * mkdir a random entry
			 */
			r = random() % nfiles;
			sprintf(buf, "XXXXXXXXXXXX.%d", r);
			(void) mkdir(buf, 0777);
			break;
		default:
			break;
		}
	}
}
			
void
remove_entries(
	int	nfiles)
{
	int		i;
	char		buf[1024];
	struct stat	statb;
	int		error;

	for (i = 0; i < nfiles; i++) {
		sprintf(buf, "XXXXXXXXXXXX.%d", i);
		error = lstat(buf, &statb);
		if (error) {
			continue;
		}
		if (S_ISDIR(statb.st_mode)) {
			(void) rmdir(buf);
		} else {
			(void) unlink(buf);
		}
	}
}
