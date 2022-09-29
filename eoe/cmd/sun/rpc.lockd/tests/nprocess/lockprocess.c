#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "util.h"
#include "lockprocess.h"

extern char *Progname;
extern int Verbose;

static int Quit = 0;

static FILE *Errorfp = NULL;

static void
proc_signal_handler(int sig, int code, struct sigcontext *sc)
{
	Quit = 1;
}

static int
modsw ( double *avg_lock_time, long *lock_requests, double *avg_openclose_time,
	long *openclose_requests )
{
	int fdS, fd9, i;
	struct flock flk;
	time_t start, end, open_start, open_end;
	time_t locksum = 0;
	int error = 0;
	pid_t pid = getpid();

	open_start = time(NULL);
	fdS = open ("SYSDB", O_CREAT + O_RDWR, 0666);
	if (fdS == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp, "modsw[%d] line %d: open SYSDB: %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

	fd9 = open ("v99", O_CREAT + O_RDWR, 0666);
	if (fd9 == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp, "modsw[%d] line %d: open v99: %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

	start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	i = fcntl (fdS, F_SETLKW, &flk);
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Lock byte 2 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 2;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (2,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Unlock byte 0 on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_UNLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLK, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLK F_UNLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* -------------------------------*/

/* Lock byte 0 to Write on 2nd fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_WRLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl v99 offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fd9, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl v99 F_SETLKW F_WRLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Lock byte 2 to Write on 2nd fd */
	flk.l_whence = 0;
	flk.l_start = 2;
	flk.l_len = 1;
	flk.l_type = F_WRLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl v99 offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fd9, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl v99 F_SETLKW F_WRLCK (2,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Unlock byte 0 on 2nd fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_UNLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl v99 offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fd9, F_SETLK, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl v99 F_SETLK F_UNLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}
	end = time(NULL);
	*lock_requests = *lock_requests + 6;
	*avg_lock_time = *avg_lock_time +
		((double)(end - start) - *avg_lock_time) / *lock_requests;
	locksum += end - start;

/* Close 2nd fd */
	close (fd9);

/* Close 1st fd */
	close (fdS);

/* -------------------------------*/

	fdS = open ("SYSDB", O_CREAT + O_RDWR);
	if (fdS == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp, "modsw[%d] line %d: open SYSDB: %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

	start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Lock byte 2 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 2;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (2,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Unlock byte 0 on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_UNLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLK, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLK F_UNLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}
	end = time(NULL);
	*lock_requests = *lock_requests + 3;
	*avg_lock_time = *avg_lock_time +
		((double)(end - start) - *avg_lock_time) / *lock_requests;
	locksum += end - start;

/* Close 1st fd */
	close (fdS);

/* -------------------------------*/

	fdS = open ("SYSDB", O_CREAT + O_RDWR);
	if (fdS == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp, "modsw[%d] line %d: open SYSDB: %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

	start = time(NULL);
/* Lock byte 0 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Lock byte 2 to Read on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 2;
	flk.l_len = 1;
	flk.l_type = F_RDLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLKW F_RDLCK (2,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Unlock byte 0 on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_UNLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"modsw[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLK, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"modsw[%d] line %d: fcntl SYSDB F_SETLK F_UNLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}
	end = time(NULL);
	*lock_requests = *lock_requests + 3;
	*avg_lock_time = *avg_lock_time +
		 ((double)(end - start) - *avg_lock_time) / *lock_requests;
	locksum += end - start;

/* Close 1st fd */
	close (fdS);
	open_end = time(NULL);
	*openclose_requests = *openclose_requests + 8;
	*avg_openclose_time = *avg_openclose_time +
		 ((double)(open_end - open_start - locksum) - *avg_openclose_time) /
		 *openclose_requests;
	return(0);

ERR: 
	return (error);
}

static int
final ( double *avg_lock_time, long *lock_requests, double *avg_openclose_time,
	long *openclose_requests )
{
	int fdS, i;
	struct flock flk;
	time_t start, end, open_start, open_end;
	int error;
	pid_t pid = getpid();

	open_start = time(NULL);
	fdS = open ("SYSDB", O_CREAT + O_RDWR);
	if (fdS == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp, "final[%d] line %d: open SYSDB: %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

	start = time(NULL);
/* Lock byte 0 to Write on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_WRLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"final[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"final[%d] line %d: fcntl SYSDB F_SETLKW F_WRLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Lock byte 2 to Write on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 2;
	flk.l_len = 1;
	flk.l_type = F_WRLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"final[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLKW, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"final[%d] line %d: fcntl SYSDB F_SETLKW F_WRLCK (2,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}

/* Unlock byte 0 on 1st fd */
	flk.l_whence = 0;
	flk.l_start = 0;
	flk.l_len = 1;
	flk.l_type = F_UNLCK;
	if (Verbose > 1) {
		fprintf(Errorfp,
			"final[%d] line %d: fcntl SYSDB offset %d len %d type %s\n",
			(int)pid, __LINE__, flk.l_start, flk.l_len,
			locktype_to_str(flk.l_type));
	}
	i = fcntl (fdS, F_SETLK, &flk);
	if (i == -1) {
		error = errno;
		if (errno != EINTR) {
			fprintf(Errorfp,
				"final[%d] line %d: fcntl SYSDB F_SETLK F_UNLCK (0,1): %s\n",
				(int)pid, __LINE__, strerror(error));
		}
		goto ERR;
	}
	end = time(NULL);
	*lock_requests = *lock_requests + 3;
	*avg_lock_time = *avg_lock_time +
		 ((double)(end - start) - *avg_lock_time) / *lock_requests;

/* Close 1st fd */
	close (fdS);
	open_end = time(NULL);
	*openclose_requests = *openclose_requests + 2;
	*avg_openclose_time = *avg_openclose_time +
		((double)(open_end - open_start - (end - start)) -
		*avg_openclose_time) / *openclose_requests;
	return(0);

ERR: 
	return (errno);
}

int
lockprocess(FILE *fp, unsigned seconds)
{
	int error = 0;
	int i;
	double avg_lock_time = (double)0.0;
	long lock_requests = (long)0;
	double avg_openclose_time = (double)0.0;
	long openclose_requests = (long)0;
	pid_t pid = getpid();

	Errorfp = fp;
	if (Verbose > 1) {
		fprintf(fp, "lockprocess[%d]: starting run, %d seconds\n",
			(int)pid, seconds);
	}
	(void)signal(SIGINT, proc_signal_handler);
	(void)signal(SIGALRM, proc_signal_handler);
	if (seconds) {
		(void)alarm(seconds);
	}

/* This program tries to reproduce a lockd problem found in PDMS */

	while ( !Quit && !error ) {

/* Do some locking */
		error = 0;
		for (i = 0; !Quit && !error && (i < 6); i++) {
			error = modsw (&avg_lock_time, &lock_requests, &avg_openclose_time,
				&openclose_requests);
		}
		if (Verbose > 1) {
			fprintf(fp, "lockprocess[%d]: Loop completed, error %d\n", (int)pid,
				error);
		}
		if (!error && !Quit) {
			error = final (&avg_lock_time, &lock_requests, &avg_openclose_time,
				&openclose_requests);
			if (Verbose > 1) {
				fprintf(fp, "lockprocess[%d]: final pass completed, error %d\n",
					(int)pid, error);
			}
		}
	}
	if (!error || (error == EINTR)) {
		fprintf(Errorfp, "lockprocess[%d] line %d: lock_requests %d "
			"avg_lock_time %lf openclose_requests %d avg_openclose_time %lf\n",
			(int)pid, __LINE__, lock_requests, avg_lock_time,
			openclose_requests, avg_openclose_time);
	}
	return ((error == EINTR) ? 0 : error);
}

/*
 * start a new process and put it onto the supplied process list
 */
pid_t
newprocess(unsigned seconds, proclist_t **plp)
{
	pid_t pid;
	int error = 0;
	int fd[2];
	proclist_t *new;
	FILE *fp;

	if (pipe(fd) == -1) {
		perror("newprocess: pipe");
		return(-1);
	}
	switch (pid = fork()) {	/* child process */
		case 0:
			/*
			 * child process
			 * catch SIGINT and close the read side of the pipe
			 */
			(void)close(fd[0]);
			if (fcntl(fd[1], F_SETFL, FNONBLOCK) == -1) {
				fprintf(stderr,
					"newprocess[%d] line %d: fcntl(F_SETFL, FNONBLOCK): %s\n",
					(int)getpid(), __LINE__, strerror(errno));
				close(fd[1]);
				exit(-1);
			}
			if (!(fp = fdopen(fd[1], "w"))) {
				error = errno;
				fprintf(stderr, "newprocess[%d] line %d: fdopen: %s\n",
					(int)getpid(), __LINE__, strerror(error));
				exit ((error == EINTR) ? 0 : error);
			}
			error = lockprocess(fp, seconds);
			fclose(fp);
			exit(error);
		case -1:
			/*
			 * fork error
			 * print a messge, close the pipe and return -1
			 */
			error = errno;
			perror("newprocess: fork");
			(void)close(fd[0]);
			(void)close(fd[1]);
			errno = error;
			break;
		default:
			/*
			 * close the write side of the pipe now that we've successfully
			 * forked
			 */
			close(fd[1]);
			if (fcntl(fd[0], F_SETFL, FNONBLOCK) == -1) {
				perror("fcntl F_SETFL FNONBLOCK");
				close(fd[0]);
				return(-1);
			}
			/*
			 * allocate a new process list entry
			 */
			if (!(new = malloc(sizeof(proclist_t)))) {
				perror("newprocess: malloc");
				close(fd[0]);
				return(-1);
			}
			/*
			 * fill in the process list entry and thread it onto the
			 * list
			 */
			new->pl_pid = pid;
			new->pl_pipefd = fd[0];
			new->pl_next = *plp;
			*plp = new;
	}
	return(pid);
}

static int
get_output(pid_t pid, proclist_t *plist)
{
	int status = 0;
	char buf[BUFSIZ];
	proclist_t *plp = NULL;
	int len;

	for (plp = plist; plp; plp = plp->pl_next) {
		if (pid == plp->pl_pid) {
			while ((len = read(plp->pl_pipefd, buf, BUFSIZ)) > 0) {
				if (fwrite(buf, sizeof(char), len, stdout) == -1) {
					status = errno;
					perror("fwrite stdout");
					break;
				}
			}
			if (len == -1) {
				status = errno;
				fprintf(stderr, "error reading pipe for process %d: %s\n",
					(int)plp->pl_pid, strerror(errno));
			}
		}
	}
	return(status);
}

int
reap_processes(proclist_t *plist)
{
	pid_t pid;
	int waitstat;
	int status = 0;

	if (Verbose) {
		printf("Waiting for processes to terminate\n");
	}
	switch(pid = wait(&waitstat)) {
		case 0:		/* unexpected return value */
			fprintf(stderr, "%s: 0 returned by wait\n", Progname);
			status = -1;
			break;
		case -1:	/* error or signal */
			switch (errno) {
				case EINTR:
					break;
				case ECHILD:
					status = errno;
					break;
				default:
					status = errno;
					perror("wait");
			}
			break;
		default:	/* child process terminated */
			if (WIFEXITED(waitstat)) {
				status = WEXITSTATUS(waitstat);
				fprintf(stderr, "%s: process %d exited: %s\n",
					Progname, pid, strerror(status));
			} else if (WIFSIGNALED(waitstat)) {
				status = WTERMSIG(waitstat);
				fprintf(stderr, "%s: process %d terminated on signal: %d\n",
					Progname, pid, status);
			} else if (WIFSTOPPED(waitstat)) {
				status = -1;
				fprintf(stderr, "%s: process %d stopped on signal: %d\n",
					Progname, pid, WSTOPSIG(waitstat));
			} else {
				fprintf(stderr, "%s: unknown wait status 0x%x\n", Progname,
					waitstat);
			}
			/*
			 * find the process in the list and collect
			 * its messages, if any
			 */
			status = get_output(pid, plist);
	}
	return(status);
}
