#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <ulocks.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <wait.h>
#include "stress.h"
#include "getopt.h"
#include "sys/resource.h"

char *Cmd;
int verbose;
int fd;

int
main(int argc, char **argv)
{
        pid_t pid;
	void slave();
	int nolibc, i, c;
        static struct flock     flock;
	char tmp_buf[5000];
	char *td;		/* temp file */
	int status;
	int nblocking = 110;
	struct rlimit nopenfiles;

	Cmd = errinit(argv[0]);
	td = tempnam(NULL, "fcntl");
	nolibc = 0;
	while ((c = getopt(argc, argv, "lf:vn:")) != EOF) {
		switch (c) {
		case 'l':
			nolibc = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'n':
			nblocking = atoi(optarg);
			break;
		case 'f':
			td = optarg;
			break;
		}
	}

	parentexinit(0);

        if ((fd = open(td, O_CREAT|O_RDWR|O_TRUNC, 0777)) < 0)
		errprintf(1, "open");
		/* NOTREACHED */

        if (write(fd, tmp_buf, sizeof(tmp_buf)) != sizeof(tmp_buf)) {
		errprintf(3, "write");
		goto bad;
	}

        flock.l_type = F_WRLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 0L;
        flock.l_len = 5L;
        if (fcntl(fd, F_SETLK, &flock) < 0) {
		errprintf(3, "WRLOCK @ 5");
		goto bad;
	}

        flock.l_type = F_WRLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 6L;
        flock.l_len = 10L;
        if(fcntl (fd, F_SETLK, &flock) < 0) {
		errprintf(3, "WRLOCK @ 6");
		goto bad;
	}

	lseek(fd, 0, 0);
        if((pid = fork()) < 0) {
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "fork");
			goto bad;
		} else {
			fprintf(stderr, "%s:can't fork:s\n", Cmd, strerror(errno));
			goto done;
		}
	} else if (pid == 0) {
                flock.l_type = F_WRLCK;
                flock.l_whence = 1;
                flock.l_start = 10;
                flock.l_len = 10;
                flock.l_pid = 0;

                if(fcntl (fd, F_GETLK, &flock) < 0) {
			errprintf(3, "fcntl(GET)");
			goto bad;
		}

                if(flock.l_type != F_WRLCK) {
                        errprintf(2, "flock.l_type = %d, should be %d\n",
                                flock.l_type, F_WRLCK);
			goto bad;
		}
                if(flock.l_whence != SEEK_SET) {
                        errprintf(2, "flock.l_whence = %d, should be %d\n",
                                flock.l_whence, SEEK_SET);
			goto bad;
		}
                if(flock.l_start != 6) {
                        errprintf(2, "flock.l_start = %d, should be %d\n",
                                flock.l_start, 6);
			goto bad;
		}
                if(flock.l_len != 10) {
                        errprintf(2, "flock.l_len = %d, should be 12\n",
                                flock.l_len);
			goto bad;
		}
                if(flock.l_pid != getppid()) {
                        errprintf(2, "flock.l_pid = %d, should be %d\n",
                                flock.l_pid, getppid());
			goto bad;
		}
                exit(0);
        }
	while (wait(&status) >= 0 || errno == EINTR)
		;

	/*
	 * this is really for testing NFS locking
	 */
	if (getrlimit(RLIMIT_NOFILE, &nopenfiles) != 0) {
		errprintf(2, "getrlimit");
		goto bad;
	}
	printf("%s:nopen cur %d max %d setting %d blocking locks\n",
		Cmd, nopenfiles.rlim_cur, nopenfiles.rlim_max, nblocking);

	nopenfiles.rlim_cur = nblocking + 20;
	if (setrlimit(RLIMIT_NOFILE, &nopenfiles) != 0) {
		errprintf(2, "setrlimit");
		goto bad;
	}

        flock.l_type = F_WRLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 6L;
        flock.l_len = 10L;
        if (fcntl(fd, F_SETLK, &flock) < 0) {
		errprintf(3, "re WRLOCK @ 6");
		goto bad;
	}

	if (!nolibc)
		usconfig(CONF_INITUSERS, nblocking+1);
	for (i = 0; i < nblocking; i++) {
		if (sprocsp(slave, PR_SALL | (nolibc ? PR_NOLIBC : 0),
					(void *)(long)i, NULL, 32*1024) < 0) {
			if (errno != EAGAIN) {
				errprintf(ERR_ERRNO_RET, "sproc failed");
				goto bad;
			}
			fprintf(stderr, "%s:sproc failed:%s\n", Cmd, strerror(errno));
			/* don't core dump - just continue test */
			break;
		}
	}

	/* now release first one */
	if (verbose)
		printf("%s:master releasing lock\n", Cmd);
        flock.l_type = F_UNLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 6L;
        flock.l_len = 10L;
        if (fcntl(fd, F_SETLK, &flock) < 0) {
		errprintf(3, "UNLOCK @ 6");
		goto bad;
	}

	/* wait for all threads to finish */
	for (;;) {
		pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
			break;
		else if ((pid >= 0) && WIFSIGNALED(status)) {
			/*
			 * if anyone dies abnormally - get out
			 * But don't abort - the process that got the signal
			 * hopefully aborted - that core dump is more
			 * interesting
			 */
			errprintf(ERR_RET, "proc %d died of signal %d\n",
				pid, WTERMSIG(status));
			unlink(td);
			if (!WCOREDUMP(status))
				abort();
			exit(1);
		}
	}

done:
	close(fd);
	unlink(td);
	return 0;
bad:
	unlink(td);
	abort();
	/* NOTREACHED */
} 

/* ARGSUSED */
void
slave(void *arg, size_t stksize)
{
        struct flock flock;

	slaveexinit();

        flock.l_type = F_WRLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 6L;
        flock.l_len = 10L;
        if (fcntl(fd, F_SETLKW, &flock) < 0) {
		errprintf(1, "slave %d WRLOCK @ 6", get_pid());
	}
	if (verbose)
		printf("%s:%d acquires lock\n", Cmd, get_pid());
	/* now release it */
        flock.l_type = F_UNLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 6L;
        flock.l_len = 10L;
        if (fcntl(fd, F_SETLK, &flock) < 0) {
		errprintf(1, "slave pid %d UNLOCK @ 6", get_pid());
	}
	_exit(0);
}
