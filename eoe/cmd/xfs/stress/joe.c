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
#include <dirent.h>


void
user(
	char	*dirname,
	int	cycles,
	int	sleep_range,
	int	sleep_base);

void
fork_sh(void);

void
read_file(
	char	*filename);

void
write_null(void);

void
read_zero(void);

void
create_tmpfile(
       char	*dirname);


void
write_logfile(
       char	*dirname);

void
remove_logfile(
       char	*dirname);

void
chmod_logfile(
       char	*dirname);

void
chown_logfile(
       char	*dirname);
void
statdir(
       char	*dirname);

int	global_garbage;

int
main(
	int	argc,
	char	*argv[])
{
	char	*dirname;
	int	nprocs;
	int	c;
	int	errflg;
	int	i;
	int	childpid;
	int	cycles;
	int	sleep_base;
	int	sleep_range;

	cycles = 300000;
	errflg = 0;
	nprocs = 4;

	dirname = "/var/tmp";
	sleep_base = 5;
	sleep_range = 100;
	while ((c = getopt(argc, argv, "c:d:p:r:s:")) != EOF) {
		switch(c) {
		case 'c':
		        cycles = atoi(optarg);
			break;
		case 'd':
			dirname = optarg;
			break;
		case 'p':
			nprocs = atoi(optarg);
			break;
		case 'r':
			sleep_range = atoi(optarg);
			break;
		case 's':
			sleep_base = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}
	}
	if (errflg) {
		printf("Usage: joe [-c usercycles] [-d dirname] [-p nprocs] [ -r sleep range ticks ] [-s sleep ticks]\n");
		exit(0);
	}

	for (i = 0; i < nprocs; i++) {
		childpid = fork();
		if (childpid < 0) {
			perror("Fork failed");
			exit(errno);
		}
		if (childpid == 0) {
			/* child */
			user(dirname, cycles, sleep_range, sleep_base);
			exit(0);
		}
	}
	while (wait((int*)0) != -1) {
		/*
		 * Just wait for all the children to finish.
		 */
		;
	}
	printf("INFO: joe complete\n");
	exit(0);
	/*NOTREACHED*/
}



void
user(
	char	*dirname,
	int	cycles,
	int	sleep_range,
	int	sleep_base)
{
	int		i;
	int		seed;
	int		x;

	seed = getpid();
	srandom(seed);

	/*
	 * Randomize our uid and gid so we'll test quotas a bit
	 * too.
	 */
	setuid((uid_t)random());
	setgid((uid_t)random());

	for (;;) {
		i = random();

		switch (i % 11) {
		case 0:
			/*
			 * Create a file and remove it
			 */
			create_tmpfile(dirname);
			break;
		case 1:
			/*
			 * Read n bytes from /dev/zero
			 */
			read_zero();
			break;
		case 2:
			/*
			 * Write n bytes to /dev/null
			 */
			write_null();
			break;
		case 3:
			/*
			 * Read /etc/passwd
			 */
			read_file("/etc/passwd");
			break;
		case 4:
			/*
			 * Read /etc/hosts
			 */
			read_file("/etc/hosts");
			break;
		case 5:
			/*
			 * Fork off an empty shell process
			 */
			fork_sh();
			break;
		case 6:
			write_logfile(dirname);
			break;
		case 7:
			remove_logfile(dirname);
			break;
		case 8:
			chmod_logfile(dirname);
			break;
		case 9:
			chown_logfile(dirname);
			break;
		case 10:
			statdir(dirname);
			break;
		default:
			break;
		}

		/*
		 * Always burn a little user time.
		 */
		for (x = 0; x < cycles; x++) {
			global_garbage += i * x;
		}
	     
		/*
		 * Sleep for a variable amount of time.
		 */
		sginap((i % sleep_range) + sleep_base);
	}
}


void
write_logfile(
       char	*dirname)
{
	int	i;
	int	fd;
	char	buf[8192];

	sprintf(buf, "%s/joe.user.log.%d", dirname, getpid());
	fd = open(buf, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) {
		perror("Failed to create joe.user file");
		return;
	}

	i = random() % 819;
	(void) write(fd, buf, i);
	close(fd);
}

void
remove_logfile(
       char	*dirname)
{
	char	buf[8192];

	sprintf(buf, "%s/joe.user.log.%d", dirname, getpid());
	(void) unlink(buf);
}

void
chmod_logfile(
       char	*dirname)
{
	char	buf[8192];

	sprintf(buf, "%s/joe.user.log.%d", dirname, getpid());
	(void) chmod(buf, 0777);
}

void
chown_logfile(
       char	*dirname)
{
	char	buf[8192];

	sprintf(buf, "%s/joe.user.log.%d", dirname, getpid());
	(void) chown(buf, getuid(), getgid());
}

void
create_tmpfile(
       char	*dirname)
{
	int	i;
	int	fd;
	char	buf[8192];

	sprintf(buf, "%s/joe.user.%d", dirname, getpid());
	fd = creat(buf, 0666);
	if (fd < 0) {
		perror("Failed to create joe.user file");
		return;
	}

	i = random() % 8192;
	(void) write(fd, buf, i);
	close(fd);
	unlink(buf);
}

void
read_zero(void)
{
	int	fd;
	int	i;
	char	buf[8192];

	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0) {
		perror("Failed to open /dev/zero");
		return;
	}

	i = random() % 100;
	(void) read(fd, buf, i);
	close(fd);
}

void
write_null(void)
{
	int	i;
	int	fd;
	char	buf[8192];

	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) {
		perror("Failed to open /dev/null");
		return;
	}

	i = random() % 100;
	(void) write(fd, buf, i);
	close(fd);
}


void
read_file(
	char	*filename)
{
	int	fd;
	int	n;
	char	buf[8192];

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("Failed to open read file");
		return;
	}

	do {
		n = read(fd, buf, 4096);
	} while (n > 0);

	close(fd);
}


void
fork_sh(void)
{
	int	childpid;

	childpid = fork();
	if (childpid < 0) {
		return;
	}

	if (childpid > 0) {
		while (wait((int *)NULL) != -1);
	} else {
		execl("/bin/sh", "/bin/sh", "-c", "exit", NULL);
		exit(0);
	}
	return;
}

void
statdir(
	char	*dirname)
{
	struct stat64	statbuf;
	DIR		*dirp;
	struct dirent64	*entryp;
	char		buf[512];

	dirp = opendir(dirname);
	if (dirp == NULL) {
		perror("Statdir can't open directory\n");
		return;
	}
	for (;;) {
		entryp = readdir64(dirp);
		if (entryp == NULL) {
			closedir(dirp);
			return;
		}

		sprintf(buf, "%s/%s", dirname, entryp->d_name);
		(void) stat64(buf, &statbuf);
	}
}
