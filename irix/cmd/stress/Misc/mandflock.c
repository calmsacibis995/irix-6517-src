#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "stress.h"

#define EXEC_CHILD "./mandflock_child"

char *Cmd;

int
main(int argc, char **argv)
{
	int fd, fd2,  c;
	char *fname = NULL;
	char *fname2;
	char buf[512];
	char exec_buf[4];
	struct flock fl;
	pid_t pid;
	char *sargv[2];

	Cmd = errinit(argv[0]);

	while ((c = getopt(argc, argv, "f:")) != EOF)
		switch (c) {
		case 'f':
			fname = optarg;
			break;
		}

	/***** First phase of testing. *****/

	/*
	 * Before doing read/write tests, test the logic for
	 * already existing files with mandatory locking enabled,
	 * for which a lock is applied.  It should disallow:
	 * - open w/ O_TRUNC
	 * - open w/ O_CREAT
	 */
	fname2 = tempnam(NULL, "mandflock2\n");

	/*
	 * Create the file in mandatory locking mode and test that the
	 * two cases are denied iff a lock is applied.
	 */
	if ((fd2 = open(fname2, O_RDWR|O_CREAT|O_TRUNC, S_ISGID|0666)) < 0) {
		printf("mandflock ERROR: open2\n");
		abort();
	}
	if (write(fd2, buf, 512) != 512) {
		printf("mandflock ERROR: initial write2\n");
		abort();
	}

	/*
	 * Start without any locks and ensure that the ops. are allowed.
	 */
	if (open(fname2, O_RDWR|O_TRUNC, 0666) < 0) {
		printf("mandflock ERROR: open2 w/ trunc\n");
	}
	/* must set S_ISGID again since we're re-creating the file (maybe) */
	if (open(fname2, O_RDWR|O_CREAT, S_ISGID|0666) < 0) {
		printf("mandflock ERROR: create2\n");
	}

	/*
	 * Now grab a lock and ensure that the ops. are denied.
	 */
	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 1;
	fl.l_len = 511;
	if (fcntl(fd2, F_SETLK, &fl)) {
		printf("mandflock ERROR: fcntl-SETLK2\n");
		abort();
	}

	if (open(fname2, O_RDWR|O_TRUNC, 0666) != -1) {
		printf("mandflock ERROR: open w/ trunc\n");
	}
	if (open(fname2, O_RDWR|O_CREAT, 0666) != -1) {
		printf("mandflock ERROR: open w/ creat\n");	
	}

	/*
	 * Now turn off mandatory mode and ensure that the ops. are allowed.
	 */
	if (fchmod(fd2, 0666)) {
		printf("mandflock ERROR: turn off mand. locking2\n");
		abort();
	}
	if (open(fname2, O_RDWR|O_TRUNC, 0666) < 0) {
		printf("mandflock ERROR: open2 w/ trunc\n");
	}
	if (open(fname2, O_RDWR|O_CREAT, 0666) < 0) {
		printf("mandflock ERROR: create2\n");
	}

	unlink(fname2);
	
	/***** Second phase of testing. *****/

	if (fname == NULL)
		fname = tempnam(NULL, "mandflock\n");

	/*
	 * Create the file in mandatory locking mode.
	 */
	if ((fd = open(fname, O_RDWR|O_CREAT|O_TRUNC, S_ISGID|0666)) < 0) {
		printf("mandflock ERROR: open\n");
		abort();
	}

	if (write(fd, buf, 512) != 512) {
		printf("mandflock ERROR: initial write\n");
		goto err;
	}

	/* grab file lock and read - we should be able to */
	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 1;
	fl.l_len = 511;
	if (fcntl(fd, F_SETLK, &fl)) {
		printf("mandflock ERROR: fcntl-SETLK\n");
		goto err;
	}
	lseek(fd, 0, 0);
	if (read(fd, buf, 512) != 512) {
		printf("mandflock ERROR: read-back\n");
	}

	if ((pid = fork()) == 0) {
		/*
		 * child - should be able to read and write first byte, 
		 * but thats all
		 */
		if (fcntl(fd, F_SETFL, FNDELAY)) {
			printf("mandflock ERROR: child fcntl to NDELAY\n");
			abort();
		}
		lseek(fd, 0, 0);
		if (read(fd, buf, 1) != 1) {
			printf("mandflock ERROR: child read of first byte\n");
		}
		lseek(fd, 0, 0);
		if (read(fd, buf, 512) != -1 || errno != EAGAIN) {
			printf("mandflock ERROR: child read of all bytes\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 1) != 1) {
			printf("mandflock ERROR: child write of first byte\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 512) != -1 || errno != EAGAIN) {
			printf("mandflock ERROR: child write of all bytes\n");
		}
		/*
		 * Now chmod the file to turn off mandatory locking.
		 * Should be able to read and write all bytes.
		 */
		if (fchmod(fd, 0666)) {
			printf("mandflock ERROR: turn off mand. locking\n");
			abort();
		}
		lseek(fd, 0, 0);
		if (read(fd, buf, 512) != 512) {
			printf("mandflock ERROR: second child read of all bytes\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 512) != 512) {
			printf("mandflock ERROR: Second child write of all bytes\n");
		}
		/*
		 * And turn it on again...
		 */
		if (fchmod(fd, S_ISGID|0666)) {
			printf("mandflock ERROR: turn on mand. locking\n");
			abort();
		}
		lseek(fd, 0, 0);
		if (read(fd, buf, 1) != 1) {
			printf("mandflock ERROR: third child read of first byte\n");
		}
		lseek(fd, 0, 0);
		if (read(fd, buf, 512) != -1 || errno != EAGAIN) {
			printf("mandflock ERROR: third child read of all bytes\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 1) != 1) {
			printf("mandflock ERROR: third child write of first byte\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 512) != -1 || errno != EAGAIN) {
			printf("mandflock ERROR: third child write of all bytes\n");
		}

		/*
		 * Just for grins, exec a child and make sure it
		 * obeys mandatory locking.  exec is used instead
		 * of fork because this can go remote in cell-based
		 * systems.	
		 *
		 * Note that before exiting the child will turn off
		 * mandatory locking.  Thus, the parent should be
		 * able to read/write again (see below).
		 */
		sprintf(exec_buf, "%u", fd);
		sargv[0] = exec_buf;
		sargv[1] = NULL;
		if (execv(EXEC_CHILD, sargv)) {
			printf("mandflock ERROR: exec\n");
			abort();
		}
		/* not reached */

	} else if (pid < 0) {
		printf("mandflock ERROR: fork failed\n");
		goto err;
	} else {
		/* parent */
		while (wait(NULL) >= 0 || errno != ECHILD)
			errno = 0;

		/*
		 * Child turned off mandatory locking.  Test that
		 * the parent can read/write again.
		 */
		lseek(fd, 0, 0);
		if (read(fd, buf, 512) != 512) {
			printf("mandflock ERROR: parent read of all bytes\n");
		}
		lseek(fd, 0, 0);
		if (write(fd, buf, 512) != 512) {
			printf("mandflock ERROR: parent write of all bytes\n");
		}
	}

	unlink(fname);
	return 0;

err:
	unlink(fname);
	abort();
	/* NOTREACHED */
}
