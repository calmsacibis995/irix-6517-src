#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/syssgi.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <bstring.h>
#include <string.h>
#include <signal.h>
#include "stress.h"
#include <getopt.h>

char *Cmd;
char dummymsg[1];
int verbose;
int halfway;	/* send but don't receive files */
int nfiles = 20;
char **fnames;

void cleanup(void);
void sigc(int sig);
void usage(void);

int
recvfd(int recvsock, int *rfds, int nrfds)
{
    struct iovec iov;
    struct msghdr msg;

    iov.iov_base = dummymsg;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = 0;
    msg.msg_accrights = (caddr_t)rfds;
    msg.msg_accrightslen = nrfds * (int)sizeof(int);
    
    if (recvmsg(recvsock, &msg, 0) < 0) {
        return -1;
    }

    return 0;
}

int
sendfd(int sendsock, int *sfds, int nsfds)
{
    struct iovec iov;
    struct msghdr msg;
    int i, rv;

    iov.iov_base = dummymsg;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_accrights = (caddr_t)sfds;
    msg.msg_accrightslen = nsfds * (int)sizeof(int);

    if (verbose) {
	printf("Sending %d fds:", nsfds);
	for (i = 0; i < nsfds; i++)
		printf("%d ", sfds[i]);
	printf("\n");
    }
    /*
     * N.B. - can't really send all that many since the accrights
     * must fit in an mbuf ...
     */
    rv = sendmsg(sendsock, &msg, 0);
    if (rv < 0) {
	errprintf(3, "sendfd sending %d fds", nsfds);
	return -1;
    } else if (rv != 1) {
	errprintf(3, "sendfd - got return value of %d instead of 1", rv);
	return -1;
    }
    return 0;
}

void
hello(void *fd)
{
    write((int)(long)fd, "hello world\n", 12);
}

int
main(int argc, char *argv[])
{
	int sendsock, recvsock, sockpair[2], st;
	pid_t pid, spid;
	fd_set readfds;
	int c, i, j;
	int sflags = PR_SADDR;
	int *rfds, *sfds;
	int nper = 5;

	Cmd = errinit(argv[0]);
	while ((c = getopt(argc, argv, "hvp:n:f")) != EOF)
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'n':
			nfiles = atoi(optarg);
			break;
		case 'f':
			sflags |= PR_SFDS;
			break;
		case 'p':
			nper = atoi(optarg);
			break;
		case 'h':
			halfway = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	/* round nfiles up to multiple of nper */
	nfiles = (((nfiles + nper - 1) / nper) * nper);
	fnames = (char **)malloc(nfiles * sizeof(char *));
	for (i = 0; i < nfiles; i++)
		fnames[i] = NULL;
	rfds = malloc(nper * sizeof(int));
	sfds = malloc(nper * sizeof(int));

	sigset(SIGPIPE, sigc);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) < 0) {
		errprintf(1, "socketpair");
		/* NOTREACHED */
	}

	if (verbose)
		printf("%s:sending %d fds - %d at a time\n", Cmd, nfiles, nper);
	sendsock = sockpair[0];
	recvsock = sockpair[1];

	pid = fork();

	if (pid > 0) {
		/* parent */
		close(recvsock);
		for (i = 0; i < nfiles; i += nper) {
			for (j = 0; j < nper; j++) {
				int fno = j + i;
				fnames[fno] = tempnam(NULL, "fds");

				sfds[j] = open(fnames[fno], O_WRONLY | O_CREAT, 0666);
				if (sfds[j] < 0) {
					errprintf(3, fnames[fno]);
					goto bad;
				}
			}
			if (verbose)
				printf("%s:hifd %d\n", Cmd, getdtablehi());
			if (sendfd(sendsock, sfds, nper)) {
				errprintf(3, "sendfd");
				goto bad;
			}
		        for (j = 0; j < nper; j++)
				if (close(sfds[j])) {
					errprintf(3, "close of fd %d failed", sfds[j]);
					goto bad;
				}
			if (verbose)
				printf("%s:hifd %d\n", Cmd, getdtablehi());
		}
		unblockproc(pid);
	} else if (pid == 0) {
		/* child */
		close(sendsock);
		for (i = 0; i < nfiles; i += nper) {
		    FD_ZERO(&readfds);
		    FD_SET(recvsock, &readfds);
		    if (select(recvsock + 1, &readfds, NULL, NULL, NULL) < 0) {
			errprintf(1, "select");
			/* NOTREACHED */
		    }
		    if (halfway)
			continue;

		    if (recvfd(recvsock, rfds, nper)) {
			errprintf(1, "recvfd");
			/* NOTREACHED */
		    }

		    spid = sproc(hello, sflags, (void *)(long)rfds[0]);

		    if (spid < 0) {
			errprintf(3, "sproc");
			continue;
		    }

		    waitpid(spid, &st, 0);
		    if (verbose)
			printf("%s:child:hifd %d\n", Cmd, getdtablehi());
		    for (j = 0; j < nper; j++)
			    if (close(rfds[j])) {
				errprintf(3, "close of fd %d failed", rfds[j]);
				goto bad;
			    }
		    if (verbose)
			printf("%s:child:hifd %d\n", Cmd, getdtablehi());
		}
		/* for halfway case - let parent finish .. */
		blockproc(getpid());
		exit(0);
	} else {
		/* not really a catasrophe ... */
		if (errno != EAGAIN) {
			errprintf(ERR_ERRNO_RET, "fork");
			goto bad;
		} else
			fprintf(stderr, "%s:fork failed:%s\n", Cmd, strerror(errno));
	}
	cleanup();
	exit(0);
bad:
	cleanup();
	abort();
	/* NOTREACHED */
}

void
usage(void)
{
	fprintf(stderr, "Usage:%s [-vfh][-n files][-p #fds]\n", Cmd);
	fprintf(stderr, "\t-v\tverbose\n");
	fprintf(stderr, "\t-h\tsend but don't receive fds\n");
	fprintf(stderr, "\t-f\thave sproc child shared fds\n");
	fprintf(stderr, "\t-n\ttotal # files to create\n");
	fprintf(stderr, "\t-p\tfiles to send at once\n");
}

void
sigc(int sig)
{
	errprintf(2, "Caught signal %d\n", sig);
	cleanup();
	abort();
}

void
cleanup(void)
{
	int i;
	for (i = 0; i < nfiles; i++)
		if (fnames[i])
			unlink(fnames[i]);
}
